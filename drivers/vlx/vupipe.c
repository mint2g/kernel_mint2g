/*
 ****************************************************************
 *
 *  Component: VLX virtual user-space pipe driver
 *
 *  Copyright (C) 2011, Red Bend Ltd.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  You should have received a copy of the GNU General Public License Version 2
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *  Contributor(s):
 *    Guennadi Maslov (guennadi.maslov@redbend.com)
 *
 ****************************************************************
 */

#include <linux/version.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/sched.h>	/* TASK_INTERRUPTIBLE */
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#include <nk/nkern.h>
#include <nk/vupipe.h>

MODULE_DESCRIPTION("VUPIPE communication driver");
MODULE_AUTHOR("Guennadi Maslov <guennadi.maslov@redbend.com>");
MODULE_LICENSE("GPL");

#define VUPIPE_MAJOR		222

#define VUPIPE_MSG		"VUPIPE: "
#define VUPIPE_ERR		KERN_ERR    VUPIPE_MSG
#define VUPIPE_INFO		KERN_NOTICE VUPIPE_MSG

#define WAIT_QUEUE		wait_queue_head_t
#define MUTEX			struct mutex

//#define TRACE_SYSCONF		/* enable to see handshake traces */
//#define TRACE_XIRQ		/* enable to see all cross interrupts */

    /*
     * The vupipe uses 1 unidirectional link.
     * This link is a simple circular character buffer with
     * "free running" buffer indexes for "producer" and "consumer".
     * We use cross OS interrupts to inform peer driver that circular
     * buffer is full or empty.
     *
     * This circular buffer will mmapped by an application, then ioctls
     * will be used to work with this buffer (send cross-interrupts
     * to another OS for example).
     *
     * By convention a driver connected to "client" side of communication
     * link will put characters into circular buffer, and a driver
     * connected to "server" side of communication link will get
     * characters from circular buffer.
     *
     * This circular buffer (or ring) is described by ExRing data
     * structure. It is placed in the "shared" persistent memory (pmem)
     * and visible from "both sides". The ExRing descriptor is also
     * placed in the "shared" persistent memory. It is hidden from
     * an application though.
     *
     * There is no synchronization between OSes and VLX can switch
     * to another OS at any moment. Because of that several fields
     * of ExRing data structure have "volatile" attribute. Those
     * fields can be changed by peer driver at any moment so we
     * shall inform compiler about that.
     *
     * The vupipe driver uses cross interrupts to wake its peer
     * driver up when it knows that its peer has some work to do
     * (it put some some character to circular buffer, so peer driver
     * can read them, or it got some characters from circular buffer,
     * so peer driver can write more).
     */

#define DEF_RING_SIZE		0x1000

typedef struct ExRing {
    volatile nku32_f	s_idx;	/* "free running" "server" index */
    volatile nku32_f	c_idx;	/* "free running" "client" index */
} ExRing;

    /*
     * We use the following macros to calculate available space ("room")
     * in the circular buffer (see 2 diagrams below)
     *
     *          s_idx                c_idx
     *            |      available     |
     *            |    consumer room   |
     *            |<------------------>|
     *            |                    |
     *  |---------v--------------------v----------------------|
     *            |                                           |
     *            |<----------------------------------------->|
     *                       contiguous consumer room
     *
     *
     *          s_idx                c_idx
     *            |                    |       available
     *            |                    |     producer room
     *  --------->|                    |<----------------------
     *            |                    |
     *  |---------v--------------------v----------------------|
     *                                 |                      |
     *                                 |<-------------------->|
     *                                        contiguous
     *                                      producer room
     *
     *
     * RING_P_ROOM  - how much "room" in a circular ring (i.e. how many
     *		      available bytes) we have for the producer
     * RING_P_CROOM - how much "contiguous room" (from the current position
     *		      up to end of ring w/o ring overlapping)
     *		      in a circular ring we have for the producer
     * RING_C_ROOM  - how much "room" in a circular ring (i.e. how many
     *		      available bytes) we have for the consumer
     * RING_C_CROOM - how much "contiguous room" (from the current position
     *		      up to end of ring w/o ring overlapping)
     *		      in a circular ring we have for the consumer
     */
#define RING_P_ROOM(rng,size)	((size) - ((rng)->c_idx - (rng)->s_idx))
#define RING_P_CROOM(ex_dev)	((ex_dev)->size - (ex_dev)->pos)
#define RING_C_ROOM(rng)	((rng)->c_idx - (rng)->s_idx)
#define RING_C_CROOM(ex_dev)	((ex_dev)->size - (ex_dev)->pos)

    /*
     * The vupipe driver has a "local" (invisible to peer driver)
     * ExDev data structure for each detected communication device.
     * It contains everything we should "know" to work with this device.
     *
     * The "enable" field is used to say that the device is in a good
     * shape and has all resources allocated.
     *
     * The "server" field is used to say which side of the link is managed
     * by our driver.
     *
     * We use mutual exclusion lock for all basic operations
     * (open, mmap, ioctl, close) to ensure that only one thread
     * executes them.
     *
     * We use a single waiting queue for all blocking operations.
     * All interrupt handlers (cross interrupt handlers and
     * sysconfig handler) simple wake a sleeping thread up.
     * We use only wait_event() as sleeping primitive, so awaken
     * thread will recheck its sleeping conditions and perform
     * appropriate actions.
     *
     * All sleeping conditions check the peer driver status.
     * If peer driver have a problem (not in NK_DEV_VLINK_ON state)
     * the awaken thread will abort current operation.
     *
     * The "count" field is used to prevent multiply "open" operation
     * for the same communication device.
     */
typedef struct ExDev {
    _Bool	enabled;	/* flag: device has all resources allocated */
    _Bool	server;		/* driver acts as a server */
    NkDevVlink* vlink;		/* vlink */
    ExRing*	ring;		/* circular ring descriptor */
    NkPhAddr	data;		/* circular ring data */
    size_t	size;		/* size of circular ring */
    size_t	pos;		/* reading/writing position inside ring */
    NkXIrq	s_xirq;		/* server side xirq */
    NkXIrq	c_xirq;		/* client side xirq */
    NkXIrqId	xid;		/* cross interrupt handler id */
    MUTEX	lock;		/* mutual exclusion lock for all ops */
    WAIT_QUEUE	wait;		/* waiting queue for all ops */
    int		count;		/* usage counter */
} ExDev;


    /*
     * a few "driver wide" global variables
     */
static unsigned ex_dev_num;	/* total number of communication links */
static ExDev*   ex_devs;	/* pointer to array of device descriptors */
static NkXIrqId ex_sysconf_id;  /* xirq id for sysconf handler */
static int	ex_chrdev;	/* flag - character devices are registered */
static struct class* ex_class;	/* "vupipe" device class pointer */

    /*
     * Initialize our own variables in ExRing data structure
     * (server index or client index)
     *
     * This function is called by handshake mechanism when driver
     * state changes from NK_DEV_VLINK_OFF to NK_DEV_VLINK_RESET.
     */
    static void
ex_ring_reset (ExDev* ex_dev)
{
    if (ex_dev->server) {
	ex_dev->ring->s_idx = 0;
    } else {
	ex_dev->ring->c_idx = 0;
    }
    ex_dev->pos = 0;
}

    /*
     * complete initialization.
     *
     * This function is called by handshake mechanism when driver
     * state changes from NK_DEV_VLINK_RESET to NK_DEV_VLINK_ON.
     *
     * we allowed here to read peer driver variables in ExRing
     */
    static void
ex_ring_init (ExDev* ex_dev)
{
    (void) ex_dev;
    /*
     * Nothing to do here in our driver.
     * More sophisticated drivers
     * can read "peer variables" and initialize
     * their "shadow copies" in a "local" descriptor
     */
}

    /*
     * send NK_XIRQ_SYSCONF cross interrupt to peer driver
     */
    static void
ex_sysconf_trigger(ExDev* ex_dev)
{
    if (ex_dev->server) {
#ifdef TRACE_SYSCONF
	printk(VUPIPE_MSG "Sending sysconf OS#%d->OS#%d(%d,%d)\n",
	       ex_dev->vlink->s_id,
	       ex_dev->vlink->c_id,
	       ex_dev->vlink->s_state,
	       ex_dev->vlink->c_state);
#endif
	nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, ex_dev->vlink->c_id);
    } else {
#ifdef TRACE_SYSCONF
	printk(VUPIPE_MSG "Sending sysconf OS#%d<-OS#%d(%d,%d)\n",
	       ex_dev->vlink->s_id,
	       ex_dev->vlink->c_id,
	       ex_dev->vlink->s_state,
	       ex_dev->vlink->c_state);
#endif
	nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, ex_dev->vlink->s_id);
    }
}

    /*
     * Perform a handshake
     *
     * It is actually a "helper" function. It analyzes
     * our state and peer state and change our state
     * accordingly. The "server" parameter tells us
     * for which link (server or client) we perform
     * a handshake.
     *
     * it is recommended to "reuse" this function in
     * "your" drivers, because it implements correct
     * state transitions. There is no much freedom here.
     */
    static int
ex_handshake (ExDev* ex_dev)
{
    volatile int* my_state;
    int           peer_state;

    if (ex_dev->server) {
	my_state   = &ex_dev->vlink->s_state;
	peer_state =  ex_dev->vlink->c_state;
    } else {
	my_state   = &ex_dev->vlink->c_state;
	peer_state =  ex_dev->vlink->s_state;
    }

#ifdef TRACE_SYSCONF
    printk(VUPIPE_MSG "ex_handshake me=%d peer=%d server=%d\n",
	   *my_state, peer_state, ex_dev->server);
#endif
    switch (*my_state) {
	case NK_DEV_VLINK_OFF:
	    if (peer_state != NK_DEV_VLINK_ON) {
		ex_ring_reset(ex_dev);
		*my_state = NK_DEV_VLINK_RESET;
		ex_sysconf_trigger(ex_dev);
	    }
	    break;
	case NK_DEV_VLINK_RESET:
	    if (peer_state != NK_DEV_VLINK_OFF) {
		ex_ring_init(ex_dev);
		*my_state = NK_DEV_VLINK_ON;
		ex_sysconf_trigger(ex_dev);
	    }
	    break;
	case NK_DEV_VLINK_ON:
	    if (peer_state == NK_DEV_VLINK_OFF) {
		*my_state = NK_DEV_VLINK_OFF;
		ex_sysconf_trigger(ex_dev);
	    }
	    break;
    }

    return (*my_state  == NK_DEV_VLINK_ON) &&
	   (peer_state == NK_DEV_VLINK_ON);
}

    /*
     * Perform a handshake for both server and client links
     * and test if both links are on
     */
    static int
ex_link_ready(ExDev* ex_dev)
{
    return ex_handshake(ex_dev);
}

    /*
     * NK_XIRQ_SYSCONFIG handler
     *
     * It scans all known communications devices
     * and wakes threads sleeping on corresponding
     * wait queues. Actual device state analyze
     * will be performed by awaken thread.
     */
    static void
ex_sysconf_hdl(void* cookie, NkXIrq xirq)
{
    ExDev*   ex_dev;
    unsigned i;

    (void) cookie;
    (void) xirq;
#ifdef TRACE_SYSCONF
    printk(VUPIPE_MSG "ex_sysconf_hdl\n");
#endif
    for (i = 0, ex_dev = ex_devs; i < ex_dev_num; i++, ex_dev++) {
	if (ex_dev->enabled) {
#ifdef TRACE_SYSCONF
	    if (ex_dev->server) {
		printk(VUPIPE_MSG "Getting sysconf OS#%d<-OS#%d(%d,%d)\n",
			ex_dev->vlink->s_id,
			ex_dev->vlink->c_id,
			ex_dev->vlink->s_state,
			ex_dev->vlink->c_state);
	    } else {
		printk(VUPIPE_MSG "Getting sysconf OS#%d->OS#%d(%d,%d)\n",
			ex_dev->vlink->s_id,
			ex_dev->vlink->c_id,
			ex_dev->vlink->s_state,
			ex_dev->vlink->c_state);
	    }
#endif
	    wake_up_interruptible(&ex_dev->wait);
	}
    }
}

    /*
     * cross interrupt handler.
     *
     * driver sends a cross interrupt to its counterpart
     * to say it has something to read or it has some
     * room to write, so it should wake up and do its work.
     * our job is simple - we need to wake reader or writer up.
     * we use a single waiting semaphore because only one
     * operation is allowed at time.
     *
     * the available room and device state will be retested
     * by awaken thread.
     */
    static void
ex_xirq_hdl (void* cookie, NkXIrq xirq)
{
    ExDev*       ex_dev = (ExDev*)cookie;

    (void) xirq;
    wake_up_interruptible(&ex_dev->wait);
}

    /*
     * link clean up - it shuts link down
     * and free some resources (detach cross
     * interrupt handlers in our example)
     *
     * this function is called only from ex_open
     * and ex_release functions, so it is guaranteed
     * that there is no other activity on this link.
     */
    static void
ex_dev_cleanup(ExDev* ex_dev)
{
	/*
	 * Detach cross interrupt handler
	 */
    if (ex_dev->xid != 0) {
	nkops.nk_xirq_detach(ex_dev->xid);
    }
	/*
	 * Reset usage counter
	 */
    ex_dev->count = 0;

	/*
	 * Say we are off
	 */
    if (ex_dev->server) {
	ex_dev->vlink->s_state = NK_DEV_VLINK_OFF;
    } else {
	ex_dev->vlink->c_state = NK_DEV_VLINK_OFF;
    }
    ex_sysconf_trigger(ex_dev);
}

    /*
     * ex_xirq_attach is a "helper" function used
     * to attach a cross interrupt handler to
     * "client"/"server" cross interrupt.
     * The "server" parameter tells us which
     * cross interrupt is used.
     */
    static int
ex_xirq_attach (ExDev* ex_dev, NkXIrqHandler hdl)
{
    NkDevVlink* link = ex_dev->vlink;
    NkXIrq      xirq;
    NkXIrqId	xid;

    if (ex_dev->server) {
	xirq = ex_dev->s_xirq;
    } else {
	xirq = ex_dev->c_xirq;
    }

    xid = nkops.nk_xirq_attach(xirq, hdl, ex_dev);
    if (xid == 0) {
	printk(VUPIPE_ERR "OS#%d<-OS#%d link=%d cannot attach xirq handler\n",
			   link->s_id, link->c_id, link->link);
	return -ENOMEM;
    }

    ex_dev->xid = xid;

    return 0;
}

    /*
     * ex_open implements open operation required
     * by linux semantic for character devices
     *
     * for the 1st open it attaches cross interrupt
     * handlers and perform "handshake" with peer
     * driver
     */
    static int
ex_open (struct inode* inode, struct file* file)
{
    unsigned int minor   = iminor(inode);
    ExDev*       ex_dev;

	/*
	 * Check for "legal" minor
	 */
    if (minor >= ex_dev_num) {
	return -ENXIO;
    }

    ex_dev = &ex_devs[minor];

	/*
	 * check if we was able to allocate all resources
	 * for this device
	 */
    if (ex_dev->enabled == 0) {
	return -ENXIO;
    }
	/*
         * ensure only one thread executes this operation
	 */
    if (mutex_lock_interruptible(&ex_dev->lock)) {
	return -EINTR;
    }
	/*
	 * Increase usage counter
	 */
    ex_dev->count += 1;

    if (ex_dev->count != 1) {
	mutex_unlock(&ex_dev->lock);
	return 0;
    }
	/*
	 * Do more initialization for the 1st open
	 */

	/*
	 * Attach cross interrupt handlers
	 */
    if (ex_xirq_attach(ex_dev, ex_xirq_hdl) != 0) {
	ex_dev_cleanup(ex_dev);
	mutex_unlock(&ex_dev->lock);
	return -ENOMEM;
    }
	/*
	 * perform handshake until both client and server links are ready
	 */
    if (wait_event_interruptible(ex_dev->wait, ex_link_ready(ex_dev))) {
		/*
		 * if open fails because of signal
		 * perform clean up and exit
		 */
	    ex_dev_cleanup(ex_dev);

	    mutex_unlock(&ex_dev->lock);
	    return -EINTR;
    }
#ifdef TRACE_SYSCONF
    printk(VUPIPE_MSG "ex_open for minor=%u is ok\n", minor);
#endif
    mutex_unlock(&ex_dev->lock);

    return 0;
}

    /*
     * ex_release implements release (close) operation
     * required by linux semantic for character devices
     *
     * for the last close it shuts link down.
     *
     * Note that ioctl operations can detect
     * that something is wrong with peer driver
     * (it went to NK_DEV_OFF state). They will exit
     * with -EPIPE error code or they will return less bytes
     * than required in this case,  but they will not change
     * link state.
     *
     * It is supposed that application will perform
     * appropriate actions for this situation, then
     * call close() function to shut link down
     * and change link state to NK_DEV_VLINK_OFF.
     *
     * After that the application may call open() to reopen
     * communication device and resume communication.
     */
    static int
ex_release (struct inode* inode, struct file* file)
{
    unsigned int minor   = iminor(inode);
    ExDev*       ex_dev;

    ex_dev = &ex_devs[minor];

	/*
         * ensure only one thread executes this operation
	 */
    if (mutex_lock_interruptible(&ex_dev->lock)) {
	return -EINTR;
    }
	/*
	 * Decrease usage counter
	 */
    ex_dev->count -= 1;

	/*
	 * Do clean up for the last usage
	 */
    if (ex_dev->count == 0) {
	ex_dev_cleanup(ex_dev);
#ifdef TRACE_SYSCONF
	printk(VUPIPE_MSG "ex_release for minor=%u is ok\n", minor);
#endif
    }

    mutex_unlock(&ex_dev->lock);

    return 0;
}

    /*
     * Inform peer driver that we have read <arg> bytes from upipe.
     */
    static int
ex_read_notify(ExDev* ex_dev, unsigned long arg)
{
    NkDevVlink* link = ex_dev->vlink;
    ExRing*	ring = ex_dev->ring;

	/*
	 * Check we are server/reader
	 */
    if (ex_dev->server == 0) {
	return -EBADF;
    }
	/*
	 * Reader cannot use more data than we have
	 */
    if (arg > RING_C_ROOM(ring)) {
	return -EINVAL;
    }
	/*
	 * Advance consumer/server index and position
	 */
    ring->s_idx += arg;
    ex_dev->pos += arg;
    if (ex_dev->pos >= ex_dev->size) {
	ex_dev->pos -= ex_dev->size;
    }
	/*
	 * Notify the peer if buffer was full
	 */
//    if (RING_P_ROOM(ring, ex_dev->size) == arg) {
#ifdef TRACE_XIRQ
	printk(VUPIPE_MSG "ex_read_notify xirq send OS#%d->OS#%d %ld\n",
			  link->s_id, link->c_id, arg);
#endif
	nkops.nk_xirq_trigger(ex_dev->c_xirq, link->c_id);
//    }
#ifdef TRACE_XIRQ
    printk(VUPIPE_MSG "ex_read_notify completed arg=%ld\n", arg);
#endif

    return 0;
}

    /*
     * Inform peer driver that we have write <arg> bytes to upipe.
     */
    static int
ex_write_notify(ExDev* ex_dev, unsigned long arg)
{
    NkDevVlink* link = ex_dev->vlink;
    ExRing*	ring = ex_dev->ring;

	/*
	 * Check we are client/writer
	 */
    if (ex_dev->server == 1) {
	return -EBADF;
    }
	/*
	 * Writer cannot use more room than we have
	 */
    if (arg > RING_P_ROOM(ring, ex_dev->size)) {
	return -EINVAL;
    }
	/*
	 * Advance producer/client index and position
	 */
    ring->c_idx += arg;
    ex_dev->pos += arg;
    if (ex_dev->pos >= ex_dev->size) {
	ex_dev->pos -= ex_dev->size;
    }
	/*
	 * Notify the peer if buffer was empty
	 */
//    if (RING_C_ROOM(ring) == arg) {
#ifdef TRACE_XIRQ
	printk(VUPIPE_MSG "ex_write_notify xirq send OS#%d->OS#%d %ld\n",
			  link->c_id, link->s_id, arg);
#endif
	nkops.nk_xirq_trigger(ex_dev->s_xirq, link->s_id);
//    }

#ifdef TRACE_XIRQ
    printk(VUPIPE_MSG "ex_write_notify completed arg=%ld\n", arg);
#endif

    return 0;
}


    /*
     * Wait until <arg> bytes are available to read.
     */
    static int
ex_available_data(ExDev* ex_dev, struct file* file, unsigned long arg)
{
    NkDevVlink* link = ex_dev->vlink;
    ExRing*	ring = ex_dev->ring;
    int         done;

#ifdef TRACE_XIRQ
    printk(VUPIPE_MSG "ex_available_data arg=%ld\n", arg);
#endif
	/*
	 * Check we are server/reader
	 */
    if (ex_dev->server == 0) {
	return -EBADF;
    }
	/*
	 * Reader cannot use more data than buffer size
	 */
    if (arg > ex_dev->size) {
	return -EINVAL;
    }
	/*
	 * For non blocking reads just return what we have;
	 * return -EAGAIN if we have nothing;
	 * return 0 if client side is not ok
	 */
    if (file->f_flags & O_NONBLOCK) {
	done = RING_C_ROOM(ring);
	if ((done == 0) && (link->c_state == NK_DEV_VLINK_ON)) {
	    done = -EAGAIN;
	}
	return done;
    }
	/*
	 * wait until we have required amount of data
	 * exit if something is wrong.
	 */
    if (wait_event_interruptible(ex_dev->wait,
		    (RING_C_ROOM(ring) >= arg)			||
		    (link->c_state != NK_DEV_VLINK_ON))) {
	return -EINTR;
    }
	/*
	 * Even if link->c_state != NK_DEV_LINK_ON we should
	 * continue to read until RING_C_ROOM(ring) != 0
	 * This read operation will return less than required
	 * bytes, next reads will return 0 bytes - end of file.
	 * This is "pipe" semantic.
	 */
#ifdef TRACE_XIRQ
    printk(VUPIPE_MSG "ex_available_data completed arg=%d\n",
		      RING_C_ROOM(ring));
#endif
    return RING_C_ROOM(ring);
}

    /*
     * Wait until <arg> bytes are available to write;
     */
    static int
ex_available_room(ExDev* ex_dev, struct file* file, unsigned long arg)
{
    NkDevVlink* link = ex_dev->vlink;
    ExRing*	ring = ex_dev->ring;

#ifdef TRACE_XIRQ
    printk(VUPIPE_MSG "ex_available_room arg=%ld\n", arg);
#endif
	/*
	 * Check we are client/writer
	 */
    if (ex_dev->server == 1) {
	return -EBADF;
    }
	/*
	 * Writer cannot use more room than buffer size
	 */
    if (arg > ex_dev->size) {
	return -EINVAL;
    }
	/*
	 * for non blocking writes check we can write all <arg> bytes;
	 * return -EPIPE if server side is not ok
	 * return -EAGAIN if we cannot write all <arg> bytes;
	 */
    if (file->f_flags & O_NONBLOCK) {
	if (link->s_state != NK_DEV_VLINK_ON) {
	    return -EPIPE;
	}
	if (RING_P_ROOM(ring, ex_dev->size) < arg) {
	    return -EAGAIN;
	} else {
	    return arg;
	}
    }
	/*
	 * wait until we have required amount of room
	 * exit if something is wrong.
	 */
    if (wait_event_interruptible(ex_dev->wait,
		    (RING_P_ROOM(ring, ex_dev->size) >= arg) ||
		    (link->s_state != NK_DEV_VLINK_ON))) {
	return -EINTR;
    }

    if (link->s_state != NK_DEV_VLINK_ON) {
	return -EPIPE;
    }

#ifdef TRACE_XIRQ
    printk(VUPIPE_MSG "ex_available_room completed arg=%ld\n", arg);
#endif
    return arg;
}

    /*
     * Main 6 ioctls: read/write notify, available data/space
     * pipe buffer size, "current" offset in the buffer.
     */
    static long
ex_ioctl (struct file* file, unsigned int cmd, unsigned long arg)
{
    unsigned int minor   = iminor(file->f_path.dentry->d_inode);
    ExDev*       ex_dev;
    int		 ret;

    ex_dev = &ex_devs[minor];

	/*
         * ensure only one thread executes this operation
	 */
    if (mutex_lock_interruptible(&ex_dev->lock)) {
	return -EINTR;
    }

    switch (cmd) {
	case PIOC_READ_NOTIFY:
	    ret = ex_read_notify(ex_dev, arg);
	    break;
	case PIOC_WRITE_NOTIFY:
	    ret = ex_write_notify(ex_dev, arg);
	    break;
	case PIOC_AVAILABLE_DATA:
	    ret = ex_available_data(ex_dev, file, arg);
	    break;
	case PIOC_AVAILABLE_ROOM:
	    ret = ex_available_room(ex_dev, file, arg);
	    break;
	case PIOC_GET_BUFFER_SIZE:
	    ret = ex_dev->size;
	    break;
	case PIOC_GET_OFFSET:
	    ret = ex_dev->pos;
	    break;
	default:
	    ret = -EINVAL;
    }

    mutex_unlock(&ex_dev->lock);

    return ret;
}

    /*
     * ex_mmap maps communication buffer to user space
     */
    static int
ex_mmap(struct file* file, struct vm_area_struct* vma)
{
    unsigned int minor   = iminor(file->f_path.dentry->d_inode);
    ExDev*       ex_dev  = &ex_devs[minor];
    int          ret;

	/*
	 * Check mmap parameters
	 */
    if (((vma->vm_end - vma->vm_start) !=  ex_dev->size) || vma->vm_pgoff) {
	return -EINVAL;
    }

    vma->vm_flags    |= VM_IO;
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    ret = io_remap_pfn_range(vma, vma->vm_start, ex_dev->data >> PAGE_SHIFT,
			     ex_dev->size, vma->vm_page_prot);
    return ret;
}

    /*
     * ex_poll implements poll operation required
     * by linux semantic for character devices
     *
     * it checks if there are any characters to read and
     * if there is any room to write and reports to upper level.
     */
    static unsigned int
ex_poll(struct file* file, poll_table* wait)
{
    unsigned int minor   = iminor(file->f_path.dentry->d_inode);
    ExDev*       ex_dev  = &ex_devs[minor];
    ExRing*	 ring    = ex_dev->ring;
    unsigned int res     = 0;

    poll_wait(file, &ex_dev->wait, wait);

    if (ex_dev->vlink->c_state != NK_DEV_VLINK_ON ||
	ex_dev->vlink->s_state != NK_DEV_VLINK_ON) {
	return (POLLERR | POLLHUP);
    }
    if (ex_dev->server) {
	if (RING_C_ROOM(ring) != 0) {
	    res |= (POLLIN | POLLRDNORM);
	}
    } else {
	if (RING_P_ROOM(ring, ex_dev->size) != 0) {
	    res |= (POLLOUT | POLLWRNORM);
	}
    }
    return res;
}

    /*
     * this data structure will we passed as a parameter
     * to linux character device framework and inform it
     * we have implemented all 5 basic operations (open,
     * ioctl, mmap, close, poll)
     */
static const struct file_operations ex_fops = {
    .owner	= THIS_MODULE,
    .open	= ex_open,
    .unlocked_ioctl = ex_ioctl,
    .mmap	= ex_mmap,
    .release	= ex_release,
    .poll	= ex_poll,
    .llseek	= no_llseek,
};

    /*
     * ex_dev_ring_alloc() is a helper function to
     * allocate a communication ring associated with
     * client or server links. it performs actual ring
     * allocation and reports errors if any.
     */
    static int
ex_dev_ring_alloc(ExDev* ex_dev)
{
    NkDevVlink* vlink = ex_dev->vlink;
    NkPhAddr    plink;
    NkPhAddr    pring;
    NkPhAddr    pdata;

    plink = nkops.nk_vtop(vlink);

    pring = nkops.nk_pmem_alloc(plink, 0, sizeof(ExRing));
    if (pring == 0) {
	printk(VUPIPE_ERR "OS#%d<-OS#%d link=%d ring desc alloc "
			  "failed (%d bytes)\n", vlink->s_id, vlink->c_id,
			  vlink->link, sizeof(ExRing));
	return -ENOMEM;
    }

    pdata = nkops.nk_pmem_alloc(plink, 1, ex_dev->size);
    if (pdata == 0) {
	printk(VUPIPE_ERR "OS#%d<-OS#%d link=%d ring alloc failed "
			  "(%d bytes)\n", vlink->s_id, vlink->c_id,
			  vlink->link, ex_dev->size);
	return -ENOMEM;
    }

    ex_dev->ring = (ExRing*) nkops.nk_mem_map(pring, sizeof(ExRing));
    ex_dev->data = pdata;

    return 0;
}

    /*
     * ex_dev_xirq_alloc() is a "helper" function to
     * allocate a client or server cross interrupt
     * associated with server or client links.
     * it performs actual cross interrupt allocation
     * and reports errors if any.
     */
    static int
ex_dev_xirq_alloc(ExDev* ex_dev, _Bool server)
{
    NkDevVlink* vlink = ex_dev->vlink;
    NkPhAddr    plink;
    NkXIrq      xirq;

    plink = nkops.nk_vtop(vlink);

    if (server) {
	xirq = nkops.nk_pxirq_alloc(plink, 1, vlink->s_id, 1);
    } else {
	xirq = nkops.nk_pxirq_alloc(plink, 0, vlink->c_id, 1);
    }

    if (xirq == 0) {
	printk(VUPIPE_ERR "OS#%d<-OS#%d link=%d %s xirq alloc failed\n",
			  vlink->s_id, vlink->c_id, vlink->link,
			  (server ? "server" : "client") );
	return -ENOMEM;
    }

    if(server) {
	ex_dev->s_xirq = xirq;
    } else {
	ex_dev->c_xirq = xirq;
    }

    return 0;
}

    /*
     * ex_dev_init() function is called to initialize a device
     * instance during driver initialization phase
     * (see ex_vlink_module_init()).
     *
     * It allocates communication rings if they are not
     * already allocated by peer driver.
     *
     * It allocates cross interrupts if they are not already allocated
     * by previous driver "incarnation" (cross interrupts are "restart"
     * persistent).
     *
     * It creates "class device" so our communication device will
     * be "visible" by applications as /dev/vupipe*
     *
     * Finally it initialize mutual exclusion lock and
     * waiting queue.
     */
    static void
ex_dev_init (ExDev* ex_dev, unsigned minor)
{
    NkDevVlink*		 vlink = ex_dev->vlink;
    const NkOsId	 my_id = nkops.nk_id_get();
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    struct device*	 cls_dev;
#else
    struct class_device* cls_dev;
#endif

	/*
	 * Set "server" flag
	 */
    if (vlink->s_id == my_id) {
	ex_dev->server = 1;
    } else {
	ex_dev->server = 0;
    }
	/*
	 * Allocate communication ring
	 */
    if (ex_dev_ring_alloc(ex_dev) != 0) {
	return;
    }
	/*
	 * Allocate client/server cross interrupts
	 */
    if ((ex_dev_xirq_alloc(ex_dev, 0) != 0) ||
	(ex_dev_xirq_alloc(ex_dev, 1) != 0)
       ) {
	return;
    }
	/*
	 * create a class device
	 */
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    cls_dev = device_create(ex_class, NULL, MKDEV(VUPIPE_MAJOR, minor),
#else
    cls_dev = class_device_create(ex_class, NULL, MKDEV(VUPIPE_MAJOR, minor),
#endif
			    NULL, "vupipe%u", minor);
    if (IS_ERR(cls_dev)) {
	printk(VUPIPE_ERR "OS#%d<-OS#%d link=%d class device create failed"
			   " err =%ld\n",
		vlink->s_id, vlink->c_id, vlink->link, PTR_ERR(cls_dev));
	return;
    }
	/*
	 * Initialize mutual exclusion lock
	 * and waiting queue
	 */
    mutex_init         (&ex_dev->lock);
    init_waitqueue_head(&ex_dev->wait);

	/*
	 * Say this device has all resources allocated
	 */
    ex_dev->enabled = 1;
}

    /*
     * ex_dev_destroy() function is called to free devices
     * resources and destroy a device instance during driver
     * exit phase (see ex_vlink_module_exit()).
     *
     * actually in our example it only destroys "class device"
     * because all other device resources (circular buffers
     * and cross interrupts) are persistent.
     */
    static void
ex_dev_destroy (ExDev* ex_dev, unsigned minor)
{
	/*
	 * Destroy class device
	 */
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    device_destroy(ex_class, MKDEV(VUPIPE_MAJOR, minor));
#else
    class_device_destroy(ex_class, MKDEV(VUPIPE_MAJOR, minor));
#endif
}

    /*
     * ex_vlink_module_cleanup() function is called
     * to free driver resources. It is used during driver
     * initialization phase (if something goes wrong) and
     * during driver exit phase.
     *
     * In both cases the linux driver framework guarantees
     * that there is no other activity in this driver.
     *
     * this function detaches sysconfig handler.
     *
     * It destroys all device instances and then
     * frees memory allocated for device descriptors.
     *
     * It destroys "example" device class
     * and unregisters our devices as character ones.
     */
    static void
ex_vlink_module_cleanup (void)
{
	/*
	 * Detach sysconfig handler
	 */
    if (ex_sysconf_id != 0) {
        nkops.nk_xirq_detach(ex_sysconf_id);
    }
	/*
	 * Destroy all devices and free devices descriptors
	 */
    if (ex_devs != NULL) {
	ExDev*   ex_dev;
	unsigned i;

	for (i = 0, ex_dev = ex_devs; i < ex_dev_num; i++, ex_dev++) {
	    if (ex_dev->enabled) {
		ex_dev_destroy(ex_dev, i);
	    }
	}

	kfree(ex_devs);
    }
	/*
	 * Destroy "vupipe" device class
	 */
    if (ex_class != NULL) {
	class_destroy(ex_class);
    }
	/*
	 * Unregister character devices
	 */
    if (ex_chrdev != 0) {
	unregister_chrdev(VUPIPE_MAJOR, "vupipe");
    }
}

    /*
     * Conversion from character string to integer number
     */
    static char*
ex_a2ui (const char* s, unsigned int* i)
{
    unsigned int xi = 0;
    char         c  = *s;

    while (('0' <= c) && (c <= '9')) {
	xi = xi * 10 + (c - '0');
	c = *(++s);
    }

    if        (*s == 'K') {
	xi *= 1024;
	s  += 1;
    } else if (*s == 'M') {
	xi *= (1024*1024);
	s  += 1;
    }

    *i = xi;

    return (char*) s;
}

    /*
     * Parse device specific parameters:
     * size of communication memory in our case
     */
    static void
ex_param(ExDev* ex_dev)
{
    NkDevVlink*  vlink = ex_dev->vlink;
    unsigned int size  = DEF_RING_SIZE;
    const char*  s;
    const char*  e;

    if (vlink->s_info != 0) {
	s = (const char*) nkops.nk_ptov(vlink->s_info);

	e = ex_a2ui(s, &size);

	if (*e != 0) {
	    printk(VUPIPE_INFO "OS#%d<-OS#%d buffer length syntax error:"
			      " <%s>\n",
			      vlink->s_id, vlink->c_id, s);
	    size = DEF_RING_SIZE;
	}
    }

    ex_dev->size = size;
}

    /*
     * ex_vlink_module_init() is the 1st function
     * called by linux driver framework. It initializes
     * our driver.
     *
     * It finds how many devices will be managed by this driver.
     *
     * It registers them as a character devices
     *
     * It creates "vupipe" class device
     *
     * It allocates memory for all devices descriptors.
     *
     * It finds all links managed by this driver
     * and "assign" them minors *sequentially*.
     *
     * Finally it attaches a sysconfig handler
     */
    static int
vupipe_module_init (void)
{
    NkPhAddr    plink;
    NkDevVlink* vlink;
    ExDev*	ex_dev;
    int		ret;
    unsigned	minor;
    NkOsId      my_id = nkops.nk_id_get();

	/*
	 * Find how many communication links
	 * should be managed by this driver
	 */
    ex_dev_num = 0;
    plink      = 0;

    while ((plink = nkops.nk_vlink_lookup("vupipe", plink))) {
	vlink = (NkDevVlink*) nkops.nk_ptov(plink);
	if ((vlink->s_id == my_id) || (vlink->c_id == my_id)) {
	    ex_dev_num += 1;
	}
    }
	/*
	 * Nothing to do if no links found
	 */
    if (ex_dev_num == 0) {
	printk(VUPIPE_ERR "no vupipe vlinks found\n");
	return -EINVAL;
    }

	/*
	 * Register devices as character ones
	 */
    if ((ret = register_chrdev(VUPIPE_MAJOR, "vupipe", &ex_fops))) {
        printk(VUPIPE_ERR "can't register chardev\n");
        return ret;
    } else {
	ex_chrdev = 1;
    }

	/*
         * Create "vupipe" device class
	 */
    ex_class = class_create(THIS_MODULE, "vupipe");
    if (IS_ERR(ex_class)) {
	ret      = PTR_ERR(ex_class);
	ex_class = NULL;
	ex_vlink_module_cleanup();
        printk(VUPIPE_ERR "can't create class\n");
        return ret;
    }

	/*
	 * Allocate memory for all device descriptors
	 */
    ex_devs = (ExDev*)kzalloc(sizeof(ExDev) * ex_dev_num, GFP_KERNEL);
    if (ex_devs == NULL) {
        printk(VUPIPE_ERR "out of memory\n");
	ex_vlink_module_cleanup();
	return -ENOMEM;
    }
	/*
	 * Find all server and client links for this OS,
	 * assign minors to them sequentially
	 * and parse parameters if any
	 */
    ex_dev = ex_devs;
    plink  = 0;
    minor  = 0;

    while ((plink = nkops.nk_vlink_lookup("vupipe", plink))) {
	vlink = (NkDevVlink*) nkops.nk_ptov(plink);
	if ((vlink->s_id == my_id) || (vlink->c_id == my_id)) {
	    ex_dev->vlink = vlink;

	    ex_param(ex_dev);
	    ex_dev_init(ex_dev, minor);

	    if (ex_dev->enabled) {
		printk(VUPIPE_INFO "device vupipe%d is created"
				   " for OS#%d<-OS#%d link=%d\n",
			minor, vlink->s_id, vlink->c_id, vlink->link);
	    }

	    ex_dev += 1;
	    minor  += 1;
	}
    }
	/*
	 * Attach sysconfig handler
	 */
    ex_sysconf_id = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF, ex_sysconf_hdl, 0);
    if (ex_sysconf_id == 0) {
	ex_vlink_module_cleanup();
        printk(VUPIPE_ERR "can't attach sysconf handler\n");
	return -ENOMEM;
    }

    printk(VUPIPE_INFO "module loaded\n");

    return 0;
}

    /*
     * ex_vlink_module_exit() is the last function
     * called by linux driver framework.
     *
     * it calls ex_vlink_module_cleanup() to free
     * driver resources
     */
    static void
vupipe_module_exit (void)
{
	/*
	 * Perform module cleanup
	 */
    ex_vlink_module_cleanup();

    printk(VUPIPE_INFO "module unloaded\n");
}

module_init(vupipe_module_init);
module_exit(vupipe_module_exit);

/*
 ****************************************************************
 *
 *  Component: VLX virtual uni-directional pipe driver
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
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

#include <linux/version.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/ioctls.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <nk/nkern.h>

MODULE_DESCRIPTION("VPIPE communication driver");
MODULE_AUTHOR("Guennadi Maslov <guennadi.maslov@redbend.com>");
MODULE_LICENSE("GPL");

#define VPIPE_MAJOR		221

#define VPIPE_MSG		"VPIPE: "
#define VPIPE_ERR		KERN_ERR    VPIPE_MSG
#define VPIPE_INFO		KERN_NOTICE VPIPE_MSG

#define WAIT_QUEUE		wait_queue_head_t
#define MUTEX			struct mutex

//#define TRACE_SYSCONF		/* enable to see handshake traces */
//#define TRACE_XIRQ		/* enable to see all cross interrupts */

    /*
     * The vpipe uses 1 unidirectional link.
     * This link is a simple circular character buffer with
     * "free running" buffer indexes for "producer" and "consumer".
     * We use cross OS interrupts to inform peer driver that circular
     * buffer is full or empty.
     *
     * By convention a driver connected to "client" side of communication
     * link will put characters into circular buffer, and a driver
     * connected to "server" side of communication link will get
     * characters from circular buffer.
     *
     * This circular buffer (or ring) is described by ExRing data
     * structure. It is placed in the "shared" persistent memory (pmem)
     * and visible from "both sides".
     *
     * There is no synchronization between OSes and VLX can switch
     * to another OS at any moment. Because of that several fields
     * of ExRing data structure have "volatile" attribute. Those
     * fields can be changed by peer driver at any moment so we
     * shall inform compiler about that.
     *
     * The vpipe driver uses cross interrupts to wake its peer
     * driver up when it knows that its peer has some work to do
     * (it put some some character to circular buffer, so peer driver
     * can read them, or it got some characters from circular buffer,
     * so peer driver can write more).
     */

#define DEF_RING_SIZE		0x1000
#define MIN_RING_SIZE		    24

typedef struct ExRing {
    volatile nku32_f	s_idx;	/* "free running" "server" index */
    volatile nku32_f	c_idx;	/* "free running" "client" index */
    nku8_f	ring[MIN_RING_SIZE];
				/* circular communication buffer */
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
     * The vpipe driver has a "local" (invisible to peer driver)
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
     * (open, read, write, close) to ensure that only one thread
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
    size_t	size;		/* size of circular ring */
    size_t	pos;		/* reading/writing position inside ring */
    NkXIrq	s_xirq;		/* server side xirq */
    NkXIrq	c_xirq;		/* client side xirq */
    NkXIrqId	xid;		/* cross interrupt handler id */
    MUTEX	lock;		/* mutual exclusion lock for all ops */
    WAIT_QUEUE	wait;		/* waiting queue for all ops */
    int		count;		/* usage counter */
	/* Statistics */
    unsigned	 opens;
    unsigned	 reads;
    unsigned	 writes;
    unsigned long long read_bytes;
    unsigned long long written_bytes;
} ExDev;


    /*
     * a few "driver wide" global variables
     */
static unsigned ex_dev_num;	/* total number of communication links */
static ExDev*   ex_devs;	/* pointer to array of device descriptors */
static NkXIrqId ex_sysconf_id;  /* xirq id for sysconf handler */
static int	ex_chrdev;	/* flag - character devices are registered */
static struct class* ex_class;	/* "vpipe" device class pointer */
static _Bool	ex_proc_exists;	/* whether /proc/nk/vpipe has been created */

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
	printk(VPIPE_MSG "Sending sysconf OS#%d->OS#%d(%d,%d)\n",
	       ex_dev->vlink->s_id,
	       ex_dev->vlink->c_id,
	       ex_dev->vlink->s_state,
	       ex_dev->vlink->c_state);
#endif
	nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, ex_dev->vlink->c_id);
    } else {
#ifdef TRACE_SYSCONF
	printk(VPIPE_MSG "Sending sysconf OS#%d<-OS#%d(%d,%d)\n",
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
     * accordingly. The "server" field tells us
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
    printk(VPIPE_MSG "ex_handshake me=%d peer=%d server=%d\n",
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
     * Perform a handshake for server or link
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
    printk(VPIPE_MSG "ex_sysconf_hdl\n");
#endif
    for (i = 0, ex_dev = ex_devs; i < ex_dev_num; i++, ex_dev++) {
	if (ex_dev->enabled) {
#ifdef TRACE_SYSCONF
	    if (ex_dev->server) {
		printk(VPIPE_MSG "Getting sysconf OS#%d<-OS#%d(%d,%d)\n",
			ex_dev->vlink->s_id,
			ex_dev->vlink->c_id,
			ex_dev->vlink->s_state,
			ex_dev->vlink->c_state);
	    } else {
		printk(VPIPE_MSG "Getting sysconf OS#%d->OS#%d(%d,%d)\n",
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
     * The "server" field tells us which
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
	printk(VPIPE_ERR "OS#%d<-OS#%d link=%d cannot attach xirq handler\n",
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
	 * check file access
	 */
    if (ex_dev->server) {
	if ((file->f_flags & O_ACCMODE) != O_RDONLY) {
	    return -EACCES;
	}
    } else {
	if ((file->f_flags & O_ACCMODE) != O_WRONLY) {
	    return -EACCES;
	}
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
	 * perform handshake until both client and server are ready
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
    ++ex_dev->opens;

#ifdef TRACE_SYSCONF
    printk(VPIPE_MSG "ex_open for minor=%u is ok\n", minor);
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
     * Note that read/write operation can detect
     * that something is wrong with peer driver
     * (it went to NK_DEV_OFF state). They will exit
     * with -EPIPE error code in this case,
     * but they will not change link state.
     *
     * It is supposed that application will perform
     * appropriate actions for -EPIPE error, then
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
	printk(VPIPE_MSG "ex_release for minor=%u is ok\n", minor);
#endif
    }

    mutex_unlock(&ex_dev->lock);

    return 0;
}

    /*
     * ex_read implements read operation required
     * by linux semantic for character devices
     *
     * it will attempt to read all <count> bytes from
     * writer side.
     *
     * It can return less than required bytes if writer side
     * is not ok.
     *
     * it waits until there is something to read in the
     * circular buffer.
     *
     * when awaken it checks the peer state and
     * exits with less than required bytes if it is not NK_DEV_VLINK_ON
     *
     * if communication link is ok it reads
     * as many characters as possible and waits again until
     * all <count> bytes have been transferred.
     *
     * it sends a cross interrupt to peer if
     * circular buffer was full, so peer driver
     * can put more characters in the circular buffer
     */
    static ssize_t
ex_read (struct file* file, char __user* buf,
         size_t count, loff_t* ppos)
{
    unsigned int minor   = iminor(file->f_path.dentry->d_inode);
    ExDev*       ex_dev  = &ex_devs[minor];
    NkDevVlink*  link    = ex_dev->vlink;
    ExRing*	 ring    = ex_dev->ring;
    ssize_t	 done;
    size_t	 cnt;
    size_t	 room;

	/*
	 * Check we are server/reader
	 */
    if (ex_dev->server == 0) {
	return -EBADF;
    }
#ifdef TRACE_XIRQ
    printk(VPIPE_MSG "ex_read minor=%u count=%d\n", minor, count);
#endif
	/*
         * ensure only one thread executes this operation
	 */
    if (mutex_lock_interruptible(&ex_dev->lock)) {
	return -EINTR;
    }
	/*
	 * Main read loop, we need to transfer all <count> bytes
	 */
    done = 0;
    while ((cnt = (count - done))) {

	    /*
	     * for non blocking read exit from the read loop
	     * when no characters in the circular buffer
	     */
	if ((file->f_flags & O_NONBLOCK) &&
	    (RING_C_ROOM(ring) == 0)) {
		/*
		 * return -EAGAIN if we have read nothing;
		 * return 0 if client side is not ok
		 */
	    if ((done == 0) && (link->c_state == NK_DEV_VLINK_ON)) {
		done = -EAGAIN;
	    }
	    break;
	}
	    /*
	     * wait until there is something to read
	     * exit if something is wrong.
	     */
	if (wait_event_interruptible(ex_dev->wait,
			(RING_C_ROOM(ring) != 0)		||
			(link->c_state != NK_DEV_VLINK_ON))) {
	    done = -EINTR;
	    break;
	}
	    /*
	     * Even if link->c_state != NK_DEV_LINK_ON we should
	     * continue to read until RING_C_ROOM(ring) != 0
	     * This read operation will return less than required
	     * bytes, next reads will return 0 bytes - end of file.
	     * This is "pipe" semantic.
	     */

	    /*
	     * check how many bytes we can transfer to user space:
	     * min (count - done, "available room", "contiguous room")
	     */
	room = RING_C_ROOM(ring);
	if (room < cnt) {
	    cnt = room;
	}

	room = RING_C_CROOM(ex_dev);
	if (room < cnt) {
	    cnt = room;
	}
	    /*
	     * exit if no more bytes to read. It may happen
	     * only if link->c_state != NK_DEV_VLINK_ON.
	     */
	if (cnt == 0) {
	    break;
	}
	    /*
	     * Copy to user buffer directly from server ring,
	     * exit if we have a problem
	     */
	if (copy_to_user(buf, &ring->ring[ex_dev->pos], cnt)) {
	    done = -EFAULT;
	    break;
	}
	    /*
	     * Next chunk
	     */
	buf         += cnt;
	ring->s_idx += cnt;
	done        += cnt;

	ex_dev->pos += cnt;
	if (ex_dev->pos == ex_dev->size) {
	    ex_dev->pos = 0;
	}
	    /*
	     * Notify the peer if buffer was full
	     */
	if (RING_P_ROOM(ring, ex_dev->size) == cnt) {
#ifdef TRACE_XIRQ
	    printk(VPIPE_MSG "ex_read xirq send OS#%d->OS#%d %d\n",
			      link->s_id, link->c_id, cnt);
#endif
	    nkops.nk_xirq_trigger(ex_dev->c_xirq, link->c_id);
	}
    }
#ifdef TRACE_XIRQ
    printk(VPIPE_MSG "ex_read from minor=%u completed done=%d\n",
		      minor, done);
#endif

    if (done >= 0) {
	++ex_dev->reads;
	ex_dev->read_bytes += done;
    }
    mutex_unlock(&ex_dev->lock);

    return done;
}

    /*
     * ex_write implements write operation required
     * by linux semantic for character devices
     *
     * it will transfer all <count> bytes to the reader side
     *
     * it will return -EPIPE if the reader is not ok
     *
     * it waits until there is room to write in the
     * circular buffer.
     *
     * when awaken it checks the peer state and
     * exits if it is not NK_DEV_VLINK_ON
     *
     * if communication link is ok it writes
     * as many characters as possible and waits again
     * until all <count> bytes have been transferred.
     *
     * it sends a cross interrupt to peer if
     * circular buffer was empty, so peer driver
     * can get more characters from the circular buffer
     */
    static ssize_t
ex_write (struct file* file, const char __user* buf,
          size_t count, loff_t* ppos)
{
    unsigned int minor   = iminor(file->f_path.dentry->d_inode);
    ExDev*       ex_dev  = &ex_devs[minor];
    NkDevVlink*  link    = ex_dev->vlink;
    ExRing*	 ring    = ex_dev->ring;
    ssize_t	 done;
    size_t	 cnt;
    size_t	 room;

	/*
	 * Check we are client/writer
	 */
    if (ex_dev->server == 1) {
	return -EBADF;
    }
#ifdef TRACE_XIRQ
    printk(VPIPE_MSG "ex_write minor=%u count=%d\n", minor, count);
#endif
	/*
         * ensure only one thread executes this operation
	 */
    if (mutex_lock_interruptible(&ex_dev->lock)) {
	return -EINTR;
    }
	/*
	 * for non blocking "small" writes (count <= ex_dev->size)
	 * ensure its atomicity.
	 */
    if ((file->f_flags & O_NONBLOCK) && (count <= ex_dev->size) &&
		(RING_P_ROOM(ring, ex_dev->size) < count)) {
	mutex_unlock(&ex_dev->lock);
	if (link->s_state == NK_DEV_VLINK_ON) {
	    return -EAGAIN;
	} else {
	    return -EPIPE;
	}
    }
	/*
	 * Main write loop, we need to transfer all <count> bytes
	 */
    done = 0;
    while ((cnt = (count - done))) {

	    /*
	     * for non blocking write exit from the write loop
	     * when no more room in the circular buffer
	     * or server side is not ok
	     */
	if (file->f_flags & O_NONBLOCK) {
	    if (link->s_state != NK_DEV_VLINK_ON) {
		done = -EPIPE;
		break;
	    }
	    if (RING_P_ROOM(ring, ex_dev->size) == 0) {
		if (done == 0) {
		    done = -EAGAIN;
		}
		break;
	    }
	}
	    /*
	     * wait until we can write
	     * exit if something is wrong.
	     */
	if (wait_event_interruptible(ex_dev->wait,
			(RING_P_ROOM(ring, ex_dev->size) != 0) ||
			(link->s_state != NK_DEV_VLINK_ON))) {
	    done = -EINTR;
	    break;
	}

	if (link->s_state != NK_DEV_VLINK_ON) {
	    done = -EPIPE;
	    break;
	}
	    /*
	     * check how many bytes we can transfer from user space:
	     * min (count - done, "available room", "continuous room")
	     */
	room = RING_P_ROOM(ring, ex_dev->size);
	if (room < cnt) {
	    cnt = room;
	}

	room = RING_P_CROOM(ex_dev);
	if (room < cnt) {
	    cnt = room;
	}
	    /*
	     * Copy user buffer directly to client ring,
	     * exit if we have a problem
	     */
	if (copy_from_user(&ring->ring[ex_dev->pos], buf, cnt)) {
	    done= -EFAULT;
	    break;
	}
	    /*
	     * Next chunk
	     */
	buf         += cnt;
	ring->c_idx += cnt;
	done        += cnt;

	ex_dev->pos += cnt;
	if (ex_dev->pos == ex_dev->size) {
	    ex_dev->pos = 0;
	}
	    /*
	     * Notify the peer if buffer was empty
	     */
	if (RING_C_ROOM(ring) == cnt) {
#ifdef TRACE_XIRQ
	    printk(VPIPE_MSG "ex_write xirq send OS#%d->OS#%d %d\n",
			      link->c_id, link->s_id, cnt);
#endif
	    nkops.nk_xirq_trigger(ex_dev->s_xirq, link->s_id);
	}
    }

#ifdef TRACE_XIRQ
    printk(VPIPE_MSG "ex_write to minor=%u completed done=%d\n",
		      minor, done);
#endif
    if (done >= 0) {
	++ex_dev->writes;
	ex_dev->written_bytes += done;
    }

    mutex_unlock(&ex_dev->lock);

    return done;
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
     * ex_ioctl currently only allows to see how many
     * bytes there are available for reading.
     * Just like Linux pipes, no error is generated
     * after the write end of the vpipe has been closed.
     */
    static long
ex_ioctl (struct file* file, unsigned int cmd, unsigned long addr)
{
    unsigned int minor  = iminor(file->f_path.dentry->d_inode);
    ExDev*	 ex_dev = &ex_devs[minor];

    switch (cmd) {
    case FIONREAD:
	if (!ex_dev->server) return -EINVAL;
	return put_user(RING_C_ROOM(ex_dev->ring), (int*) addr);

    default:
	break;
    }
    return -ENOIOCTLCMD;
}

    /*
     * this data structure will we passed as a parameter
     * to linux character device framework and inform it
     * we have implemented all 5 basic operations (open,
     * read, write, close, poll)
     */
static const struct file_operations ex_fops = {
    .owner	= THIS_MODULE,
    .open	= ex_open,
    .read	= ex_read,
    .write	= ex_write,
    .release	= ex_release,
    .poll	= ex_poll,
    .llseek	= no_llseek,
    .unlocked_ioctl = ex_ioctl,
};

    /*
     * Management of /proc/nk/vpipe
     */

    static char
ex_link_state (const int state)
{
    switch (state) {
    case NK_DEV_VLINK_ON:	return 'O';
    case NK_DEV_VLINK_RESET:	return 'R';
    case NK_DEV_VLINK_OFF:	return 'F';
    default:			break;
    }
    return '?';
}

    static int
ex_seq_proc_show (struct seq_file* seq, void* v)
{
    ExDev*   ex_dev;
    unsigned minor;

    (void) v;
    seq_printf (seq,
	"Mi Pr Id EDU St Size Opns Reads ReadBytes- Wrtes WriteBytes\n");
    for (minor = 0, ex_dev = ex_devs; minor < ex_dev_num; ++minor, ++ex_dev) {
	seq_printf (seq, "%2u %2d %2d %c%c%1d %c%c %4x %4d %5d %10lld "
		    "%5d %10lld\n",
		    minor,
		    ex_dev->server ? ex_dev->vlink->c_id : ex_dev->vlink->s_id,
		    ex_dev->vlink->link,
		    ex_dev->enabled ? 'E' : '.',
		    ex_dev->server ? 'R' : 'W',
		    ex_dev->count,
		    ex_link_state (ex_dev->vlink->c_state),
		    ex_link_state (ex_dev->vlink->s_state),
		    ex_dev->size, ex_dev->opens, ex_dev->reads,
		    ex_dev->read_bytes, ex_dev->writes, ex_dev->written_bytes);
    }
    return 0;
}

    static int
ex_proc_open (struct inode* inode, struct file* file)
{
    return single_open (file, ex_seq_proc_show, PDE (inode)->data);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,15)
static struct file_operations ex_proc_fops =
#else
static const struct file_operations ex_proc_fops =
#endif
{
    .owner	= THIS_MODULE,
    .open	= ex_proc_open,
    .read	= seq_read,
    .llseek	= seq_lseek,
    .release	= single_release,
};

    static int __init
ex_proc_init (void)
{
    struct proc_dir_entry* ent;

    ent = create_proc_entry ("nk/vpipe", 0, NULL);
    if (!ent) {
	printk (KERN_ERR "Could not create /proc/nk/vpipe\n");
	return -ENOMEM;
    }
    ent->proc_fops = &ex_proc_fops;
    ent->data      = NULL;
    ex_proc_exists = 1;
    return 0;
}

    static void
ex_proc_exit (void)
{
    if (ex_proc_exists) {
	remove_proc_entry ("nk/vpipe", NULL);
    }
}

    /*
     * ex_dev_ring_alloc() is a helper function to
     * allocate a communication ring associated with
     * client or server links. it performs actual ring
     * allocation and reports errors if any.
     */
    static int
ex_dev_ring_alloc(ExDev* ex_dev)
{
    NkDevVlink* vlink	= ex_dev->vlink;
    NkPhAddr    plink;
    NkPhAddr    pring;
    int         sz	= ex_dev->size + sizeof(ExRing) - MIN_RING_SIZE;

    plink = nkops.nk_vtop(vlink);

    pring = nkops.nk_pmem_alloc(plink, 0, sz);
    if (pring == 0) {
	printk(VPIPE_ERR "OS#%d<-OS#%d link=%d ring alloc failed (%d bytes)\n",
			  vlink->s_id, vlink->c_id, vlink->link, sz);
	return -ENOMEM;
    }

    ex_dev->ring = (ExRing*) nkops.nk_mem_map(pring, sz);

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
	printk(VPIPE_ERR "OS#%d<-OS#%d link=%d %s xirq alloc failed\n",
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
     * be "visible" by applications as /dev/vpipe*
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
    cls_dev = device_create(ex_class, NULL, MKDEV(VPIPE_MAJOR, minor),
#else
    cls_dev = class_device_create(ex_class, NULL, MKDEV(VPIPE_MAJOR, minor),
#endif
			    NULL, "vpipe%u", minor);
    if (IS_ERR(cls_dev)) {
	printk(VPIPE_ERR "OS#%d<-OS#%d link=%d class device create failed"
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
    device_destroy(ex_class, MKDEV(VPIPE_MAJOR, minor));
#else
    class_device_destroy(ex_class, MKDEV(VPIPE_MAJOR, minor));
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
    ex_proc_exit();

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
	 * Destroy "vpipe" device class
	 */
    if (ex_class != NULL) {
	class_destroy(ex_class);
    }
	/*
	 * Unregister character devices
	 */
    if (ex_chrdev != 0) {
	unregister_chrdev(VPIPE_MAJOR, "vpipe");
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
	    printk(VPIPE_INFO "OS#%d<-OS#%d buffer length syntax error:"
			      " <%s>\n",
			      vlink->s_id, vlink->c_id, s);
	    size = DEF_RING_SIZE;
	}
	if (size < sizeof(ExRing)) {
	    printk(VPIPE_INFO "OS#%d<-OS#%d buffer length is too small:"
			      " <%s>\n",
			      vlink->s_id, vlink->c_id, s);
	    size = DEF_RING_SIZE;
	}
    }

    ex_dev->size = size - sizeof(ExRing) + MIN_RING_SIZE;
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
     * It creates "vpipe" class device
     *
     * It allocates memory for all devices descriptors.
     *
     * It finds all links managed by this driver
     * and "assign" them minors *sequentially*.
     *
     * Finally it attaches a sysconfig handler
     */
    static int
vpipe_module_init (void)
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

    while ((plink = nkops.nk_vlink_lookup("vpipe", plink))) {
	vlink = (NkDevVlink*) nkops.nk_ptov(plink);
	if ((vlink->s_id == my_id) || (vlink->c_id == my_id)) {
	    ex_dev_num += 1;
	}
    }
	/*
	 * Nothing to do if no links found
	 */
    if (ex_dev_num == 0) {
	printk(VPIPE_ERR "no vpipe vlinks found\n");
	return -EINVAL;
    }
	/*
	 * Register devices as character ones
	 */
    if ((ret = register_chrdev(VPIPE_MAJOR, "vpipe", &ex_fops))) {
        printk(VPIPE_ERR "can't register chardev\n");
	return ret;
    } else {
	ex_chrdev = 1;
    }
	/*
         * Create "vpipe" device class
	 */
    ex_class = class_create(THIS_MODULE, "vpipe");
    if (IS_ERR(ex_class)) {
	ret      = PTR_ERR(ex_class);
	ex_class = NULL;
	ex_vlink_module_cleanup();
        printk(VPIPE_ERR "can't create class\n");
	return ret;
    }
	/*
	 * Allocate memory for all device descriptors
	 */
    ex_devs = (ExDev*)kzalloc(sizeof(ExDev) * ex_dev_num, GFP_KERNEL);
    if (ex_devs == NULL) {
	ex_vlink_module_cleanup();
        printk(VPIPE_ERR "out of memory\n");
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

    while ((plink = nkops.nk_vlink_lookup("vpipe", plink))) {
	vlink = (NkDevVlink*) nkops.nk_ptov(plink);
	if ((vlink->s_id == my_id) || (vlink->c_id == my_id)) {
	    ex_dev->vlink = vlink;

	    ex_param(ex_dev);
	    ex_dev_init(ex_dev, minor);

	    if (ex_dev->enabled) {
		printk(VPIPE_INFO "device vpipe%u is created"
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
        printk(VPIPE_ERR "can't attach sysconf handler\n");
	return -ENOMEM;
    }
    ex_proc_init();
    printk(VPIPE_INFO "module loaded\n");

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
vpipe_module_exit (void)
{
	/*
	 * Perform module cleanup
	 */
    ex_vlink_module_cleanup();

    printk(VPIPE_INFO "module unloaded\n");
}

module_init(vpipe_module_init);
module_exit(vpipe_module_exit);

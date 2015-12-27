/*
 ****************************************************************
 *
 *  Component: VLX virtual video backend driver
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
 *    Christophe Lizzi (Christophe.Lizzi@redbend.com)
 *
 ****************************************************************
 */

#include <linux/device.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/sched.h>	/* TASK_INTERRUPTIBLE */
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#include <linux/videodev2.h>

#include <nk/nkern.h>
#include <nk/nkdev.h>
#include <nk/nk.h>

#include <vlx/vvideo_common.h>
#include "vvideo-be.h"


//#define TRACE_SYSCONF
#if 0
#define VVIDEO_LOG(format, args...) printk("VVIDEO-BE:  " format, ## args)
#else
#define VVIDEO_LOG(format, args...) do {} while (0)
#endif


typedef struct VVideoDev {
    NkPhAddr             plink;
    NkDevVlink*          vlink;
    int                  minor;
    VVideoDesc*          desc;
    VVideoRequest*       req;
    VVideoOpen*          req_open;
    VVideoRelease*       req_release;
    VVideoIoctl*         req_ioctl;
    VVideoMMap*          req_mmap;
    VVideoMUnmap*        req_munmap;
    NkXIrqId             xid;
    NkXIrq	         s_xirq;		/* server side xirq */
    NkXIrq	         c_xirq;		/* client side xirq */
    wait_queue_head_t    wait;
    struct mutex         lock;
    int                  enabled;
    struct completion    thread_completion;
    pid_t                thread_id;
    void*                private_data;
} VVideoDev;


static vvideo_hw_ops_t* vvideo_hw_ops = NULL;

static int           vvideo_dev_num;	/* number of configured devices */
static VVideoDev*    vvideo_devs;	/* array of device descriptors */
static NkXIrqId      vvideo_sysconf_id; /* xirq id for sysconf handler */


#define VVIDEO_MSG		"VVIDEO-BE:  "
#define VVIDEO_ERR		KERN_ERR    VVIDEO_MSG
#define VVIDEO_INFO		KERN_NOTICE VVIDEO_MSG

#define VVIDEO_ASSERT(x)       do { if(!(x)) BUG(); } while (0)


    /*
     * send a NK_XIRQ_SYSCONF cross interrupt to the client (front-end) driver
     */
    static void
vvideo_sysconf_trigger (VVideoDev* vvideo_dev)
{
    NkDevVlink* link = vvideo_dev->vlink;

#ifdef TRACE_SYSCONF
	printk(VVIDEO_MSG "Sending sysconf OS#%d->OS#%d(%d,%d)\n",
	       link->s_id,
	       link->c_id,
	       link->s_state,
	       link->c_state);
#endif
	nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, link->c_id);
}


    /*
     * send a cross interrupt to the client (front-end) driver
     */
    static void
vvideo_xirq_trigger (VVideoDev* vvideo_dev)
{
    NkDevVlink* link = vvideo_dev->vlink;

    VVIDEO_LOG("vvideo_xirq_trigger: minor %d, FIRING xirq to client\n", vvideo_dev->minor);

    nkops.nk_xirq_trigger(vvideo_dev->c_xirq, link->c_id);
}


    /*
     * Perform a handshake
     *
     * It analyzes our client (front-end) state and peer server (back-end) state
     * and change our state accordingly.
     */
    static int
vvideo_handshake (VVideoDev* vvideo_dev)
{
    volatile int* my_state;
    int           peer_state;

    my_state   = &vvideo_dev->vlink->s_state;
    peer_state =  vvideo_dev->vlink->c_state;

#ifdef TRACE_SYSCONF
    printk(VVIDEO_MSG "vvideo_handshake CURRENT server (me)=%d, client (peer)=%d\n",
      *my_state, peer_state);
#endif

    switch (*my_state) {
	case NK_DEV_VLINK_OFF:
	    if (peer_state != NK_DEV_VLINK_ON) {
		*my_state = NK_DEV_VLINK_RESET;
		vvideo_sysconf_trigger(vvideo_dev);
	    }
	    break;
	case NK_DEV_VLINK_RESET:
	    if (peer_state != NK_DEV_VLINK_OFF) {
		*my_state = NK_DEV_VLINK_ON;
		vvideo_sysconf_trigger(vvideo_dev);
	    }
	    break;
	case NK_DEV_VLINK_ON:
	    if (peer_state == NK_DEV_VLINK_OFF) {
		vvideo_dev->req->req    = VVIDEO_REQ_NONE;
	        vvideo_dev->req->result = VVIDEO_REQ_PENDING;
	        wake_up_interruptible(&vvideo_dev->wait);
	    }
	    break;
    }

#ifdef TRACE_SYSCONF
    printk(VVIDEO_MSG "vvideo_handshake NEW server (me)=%d, client (peer)=%d\n",
      *my_state, peer_state);
#endif

    return (*my_state  == NK_DEV_VLINK_ON) &&
	   (peer_state == NK_DEV_VLINK_ON);
}


    /*
     * NK_XIRQ_SYSCONFIG cross interrupt handler
     *
     * It scans all enabled devices and and perform handshakes
     */
    static void
vvideo_sysconf_hdl (void* cookie, NkXIrq xirq)
{
    VVideoDev* vvideo_dev;
    int        i;

    (void) cookie;
    (void) xirq;

#ifdef TRACE_SYSCONF
    printk(VVIDEO_MSG "vvideo_sysconf_hdl\n");
#endif

    for (i = 0, vvideo_dev = vvideo_devs; i < vvideo_dev_num; i++, vvideo_dev++) {

	if (vvideo_dev->enabled) {
#ifdef TRACE_SYSCONF
	    printk(VVIDEO_MSG "Getting sysconf OS#%d->OS#%d(%d,%d)\n",
	      vvideo_dev->vlink->s_id,
	      vvideo_dev->vlink->c_id,
	      vvideo_dev->vlink->s_state,
	      vvideo_dev->vlink->c_state);
#endif
	    vvideo_handshake(vvideo_dev);
	}
    }
}


    /*
     * cross interrupt handler.
     *
     * The client (front-end) driver sends a cross interrupt to us
     * to say it has posted a new request, so the server thread
     * should wake up. The request will be processed by the awaken
     * server thread.
     */
    static void
vvideo_xirq_hdl (void* cookie, NkXIrq xirq)
{
    VVideoDev* vvideo_dev = (VVideoDev*) cookie;

    (void) xirq;

    VVIDEO_LOG("vvideo_xirq_hdl: minor %d, WAKING UP server thread\n", vvideo_dev->minor);

    wake_up_interruptible(&vvideo_dev->wait);
}


    /*
     * clean up -  shut the vlink down and free device resources.
     *
     * This function is called only from the open()
     * and release() functions, so it is guaranteed
     * that there is no other activity on this device.
     */
    static void
vvideo_dev_cleanup (VVideoDev* vvideo_dev)
{
    VVIDEO_LOG("vvideo_dev_cleanup: minor %d\n", vvideo_dev->minor);

	/*
	 * Detach the cross interrupt handler
	 */
    if (vvideo_dev->xid != 0) {
	nkops.nk_xirq_detach(vvideo_dev->xid);
    }

    // Shut the vlink down by setting our server state to off.
    vvideo_dev->vlink->s_state = NK_DEV_VLINK_OFF;

    vvideo_sysconf_trigger(vvideo_dev);

    VVIDEO_LOG("vvideo_dev_cleanup: minor %d done\n", vvideo_dev->minor);
}


    /*
     * attach a cross interrupt handler.
     */
    static int
vvideo_xirq_attach (VVideoDev* vvideo_dev, NkXIrqHandler hdl)
{
    NkDevVlink* link = vvideo_dev->vlink;
    NkXIrq      xirq;
    NkXIrqId	xid;

    VVIDEO_LOG("vvideo_xirq_attach: minor %d\n", vvideo_dev->minor);

    // Attach to the server (back-end)'s xirq
    xirq = vvideo_dev->s_xirq;

    xid = nkops.nk_xirq_attach(xirq, hdl, vvideo_dev);
    if (xid == 0) {
	printk(VVIDEO_ERR "OS#%d<-OS#%d link=%d cannot attach xirq handler\n",
			   link->s_id, link->c_id, link->link);
	return -ENOMEM;
    }

    vvideo_dev->xid = xid;

    VVIDEO_LOG("vvideo_xirq_attach: minor %d, DONE\n", vvideo_dev->minor);

    return 0;
}


    static int
vvideo_open (VVideoDev* vvideo_dev)
{
    VVideoOpen* vvideo_req_open = vvideo_dev->req_open;
    void* private_data = NULL;
    int err;

    VVIDEO_LOG("vvideo_open: minor %d\n", vvideo_dev->minor);

    if (vvideo_dev->private_data != NULL) {
	err = -EBUSY;
	goto out;
    }

    if (!vvideo_dev->enabled) {
	err = -ENODEV;
	goto out;
    }

    err = vvideo_hw_ops->open(vvideo_dev->minor, vvideo_dev->plink,
			      vvideo_dev->desc, &private_data);

    if (err == 0) {
	vvideo_dev->private_data = private_data;
    }

    /*
     * Give real minor to Front-end. This minor is unique and will be used
     * to identify the right video buffer set in the vshm area.
     */
    vvideo_req_open->minor = vvideo_dev->minor;

out:
    VVIDEO_LOG("vvideo_open: minor %d DONE, err %d\n", vvideo_dev->minor, err);

    return err;
}


    static int
vvideo_release (VVideoDev* vvideo_dev)
{
    VVideoRelease* vvideo_req_release = vvideo_dev->req_release;
    int err;

    (void) vvideo_req_release;

    VVIDEO_LOG("vvideo_release: minor %d\n", vvideo_dev->minor);

    if (vvideo_dev->private_data == NULL) {
	err = -EINVAL;
	goto out;
    }

    err = vvideo_hw_ops->release(vvideo_dev->private_data);

    vvideo_dev->private_data = NULL;

out:
    VVIDEO_LOG("vvideo_release: minor %d DONE, err %d\n", vvideo_dev->minor, err);

    return err;
}


    static int
vvideo_ioctl (VVideoDev* vvideo_dev)
{
    VVideoIoctl* vvideo_req_ioctl = vvideo_dev->req_ioctl;
    void* arg = NULL;
    int err;

    VVIDEO_LOG("vvideo_ioctl: minor %d, cmd %08x\n", vvideo_dev->minor, vvideo_req_ioctl->cmd);

    if (vvideo_req_ioctl->arg_size > 0) {

	int is_ext_ctrls = ((vvideo_req_ioctl->cmd == VIDIOC_S_EXT_CTRLS) ||
	                    (vvideo_req_ioctl->cmd == VIDIOC_G_EXT_CTRLS) ||
	                    (vvideo_req_ioctl->cmd == VIDIOC_TRY_EXT_CTRLS));

	if (is_ext_ctrls) {
	    struct v4l2_ext_controls* p = (struct v4l2_ext_controls*) vvideo_req_ioctl->arg;
	    p->controls = (struct v4l2_ext_control*) vvideo_req_ioctl->ext;
	}

	arg = vvideo_req_ioctl->arg;
    }

    err = vvideo_hw_ops->ioctl(vvideo_dev->private_data, vvideo_req_ioctl->cmd, arg);

    VVIDEO_LOG("vvideo_ioctl: minor %d, cmd %08x DONE, err %d\n", vvideo_dev->minor, vvideo_req_ioctl->cmd, err);

    return err;
}


    static int
vvideo_mmap (VVideoDev* vvideo_dev)
{
    VVideoMMap* vvideo_req_mmap = vvideo_dev->req_mmap;
    unsigned long bus_addr = 0;
    int err;

    VVIDEO_LOG("vvideo_mmap: minor %d, pgoff %08lx, offset %lu\n",
      vvideo_dev->minor, vvideo_req_mmap->pgoff, vvideo_req_mmap->pgoff << PAGE_SHIFT);

    err = vvideo_hw_ops->mmap(vvideo_dev->private_data, vvideo_req_mmap->pgoff, &bus_addr);

    if (err == 0) {
	vvideo_req_mmap->paddr = bus_addr;
    }

    VVIDEO_LOG("vvideo_mmap: minor %d, pgoff %08lx, offset %lu DONE, bus_addr %lx, err %d\n",
      vvideo_dev->minor, vvideo_req_mmap->pgoff, vvideo_req_mmap->pgoff << PAGE_SHIFT, bus_addr, err);

    return err;
}


    static int
vvideo_munmap (VVideoDev* vvideo_dev)
{
    VVideoMUnmap* vvideo_req_munmap = vvideo_dev->req_munmap;
    int err;

    VVIDEO_LOG("vvideo_munmap: minor %d, pgoff %08lx, offset %lu, size %lu\n",
      vvideo_dev->minor, vvideo_req_munmap->pgoff, vvideo_req_munmap->pgoff << PAGE_SHIFT, vvideo_req_munmap->size);

    err = vvideo_hw_ops->munmap(vvideo_dev->private_data, vvideo_req_munmap->pgoff, vvideo_req_munmap->size);

    VVIDEO_LOG("vvideo_munmap: minor %d, pgoff %08lx, offset %lu, size %lu DONE, err %d\n",
      vvideo_dev->minor, vvideo_req_munmap->pgoff, vvideo_req_munmap->pgoff << PAGE_SHIFT, vvideo_req_munmap->size, err);

    return err;
}


    static int
vvideo_ack (VVideoDev* vvideo_dev)
{
    VVIDEO_LOG("vvideo_ack: minor %d, req %d, result %d\n", vvideo_dev->minor, vvideo_dev->req->req, vvideo_dev->req->result);

    // Ack the request by sending a cross interrupt to the client (front-end)
    vvideo_xirq_trigger(vvideo_dev);

    return 0;
}


    static int
vvideo_req_posted (VVideoDev* vvideo_dev)
{
    VVideoRequest* vvideo_req = vvideo_dev->req;
    int ret = 0;

    // Any pending request from the client (front-end)?
    if (vvideo_req->result == VVIDEO_REQ_PENDING) {
	ret = 1;
    }

    VVIDEO_LOG("vvideo_req_posted: minor %d, c_state %lu, req %d, result %x -> ret %d\n",
      vvideo_dev->minor, (unsigned long)vvideo_dev->vlink->c_state, vvideo_req->req, vvideo_req->result, ret);

    return ret;
}


    /*
     * Video server thread.
     */
    static int
vvideo_thread (void* data)
{
    VVideoDev*     vvideo_dev = (VVideoDev*) data;
    VVideoRequest* vvideo_req = vvideo_dev->req;
    int err = 0;
    int open = 0;

    daemonize("vvideo_d");
    allow_signal(SIGTERM);

    for (;;) {

	VVIDEO_LOG("vvideo_thread: minor %d, waiting for request\n", vvideo_dev->minor);

	if (wait_event_interruptible(vvideo_dev->wait, vvideo_req_posted(vvideo_dev))) {

	    VVIDEO_LOG("vvideo_thread: minor %d, interrupted!!!\n", vvideo_dev->minor);
	    break;
	}

	VVIDEO_LOG("vvideo_thread: minor %d, req %d, result %x\n",
	  vvideo_dev->minor, vvideo_req->req, vvideo_req->result);

        if ((vvideo_dev->vlink->s_state == NK_DEV_VLINK_ON) &&
	    (vvideo_dev->vlink->c_state == NK_DEV_VLINK_OFF)) {
	    VVIDEO_LOG("vvideo_thread: >>> restart (open %d) <<<\n", open);
	    if (open) {
	        vvideo_release(vvideo_dev);
	        open = 0;
	    }
	    vvideo_dev->vlink->s_state = NK_DEV_VLINK_RESET;
	    vvideo_req->req            = VVIDEO_REQ_NONE;
	    vvideo_req->result         = 0;
	    vvideo_sysconf_trigger(vvideo_dev);
	}

	if (vvideo_req->req == VVIDEO_REQ_NONE) {
	    VVIDEO_LOG("vvideo_thread: minor %d, no req, ignoring\n",
	      vvideo_dev->minor);
	    continue;
	}

	switch (vvideo_req->req) {

	case VVIDEO_REQ_OPEN:
	    err = vvideo_open(vvideo_dev);
	    if (!err) {
		open = 1;
	    }
	    break;

	case VVIDEO_REQ_RELEASE:
	    err  = vvideo_release(vvideo_dev);
	    open = 0;
	    break;

	case VVIDEO_REQ_IOCTL:
	    err = vvideo_ioctl(vvideo_dev);
	    break;

	case VVIDEO_REQ_MMAP:
	    err = vvideo_mmap(vvideo_dev);
	    break;

	case VVIDEO_REQ_MUNMAP:
	    err = vvideo_munmap(vvideo_dev);
	    break;

	default:
	    err = -EINVAL;
	}

	VVIDEO_LOG("vvideo_thread: minor %d, req %d, request result %d\n",
	  vvideo_dev->minor, vvideo_req->req, err);

	vvideo_req->result = err;

	vvideo_ack(vvideo_dev);
    }

    VVIDEO_LOG("vvideo_thread: minor %d EXITING!!!\n", vvideo_dev->minor);

    complete_and_exit(&vvideo_dev->thread_completion, 0);
    return 0;
}


    /*
     * vvideo_dev_req_alloc() is a helper function to
     * allocate communication data structures shared
     * with the client (front-end).
     */
    static int
vvideo_dev_req_alloc (VVideoDev* vvideo_dev)
{
    NkDevVlink*    vlink = vvideo_dev->vlink;
    unsigned long  size  = sizeof(VVideoShared);
    VVideoShared*  shared;
    NkPhAddr       pdev;

    pdev = nkops.nk_pdev_alloc(vvideo_dev->plink, 0, size);

    VVIDEO_LOG("vvideo_dev_req_alloc: minor %d, plink %lx, pdev %lx, size %lu\n",
      vvideo_dev->minor, (unsigned long)vvideo_dev->plink, (unsigned long)pdev, size);

    if (pdev == 0) {
	printk(VVIDEO_ERR "OS#%d<-OS#%d link=%d pdev alloc failed\n",
			  vlink->s_id, vlink->c_id, vlink->link);
	return -ENOMEM;
    }

    shared = (VVideoShared*)nkops.nk_ptov(pdev);

    vvideo_dev->desc        = &shared->desc;
    vvideo_dev->req         = &shared->req;
    vvideo_dev->req_open    = &vvideo_dev->req->u.open;
    vvideo_dev->req_release = &vvideo_dev->req->u.release;
    vvideo_dev->req_ioctl   = &vvideo_dev->req->u.ioctl;
    vvideo_dev->req_mmap    = &vvideo_dev->req->u.mmap;
    vvideo_dev->req_munmap  = &vvideo_dev->req->u.munmap;

    VVIDEO_LOG("vvideo_dev_req_alloc: minor %d, plink %lx, pdev %lx, size %lu DONE -> desc %p, req %p\n",
      vvideo_dev->minor, (unsigned long)vvideo_dev->plink, (unsigned long)pdev, size, vvideo_dev->desc, vvideo_dev->req);

    return 0;
}


    /*
     * allocate a client (front-end) or server (back-end)
     * cross interrupt.
     */
    static int
vvideo_dev_xirq_alloc (VVideoDev* vvideo_dev, int backend)
{
    NkDevVlink* vlink = vvideo_dev->vlink;
    NkPhAddr    plink;
    NkXIrq      xirq;

    plink = nkops.nk_vtop(vlink);

    if (backend) {
	xirq = nkops.nk_pxirq_alloc(plink, 1, vlink->s_id, 1);
    } else {
	xirq = nkops.nk_pxirq_alloc(plink, 0, vlink->c_id, 1);
    }

    VVIDEO_LOG("vvideo_dev_xirq_alloc: minor %d, backend %d, s_id %lu, c_id %lu -> xirq %lu\n",
      vvideo_dev->minor, backend, (unsigned long)vlink->s_id, (unsigned long)vlink->c_id, (unsigned long)xirq);

    if (xirq == 0) {
	printk(VVIDEO_ERR "OS#%d<-OS#%d link=%d %s xirq alloc failed\n",
			   vlink->s_id, vlink->c_id, vlink->link,
			   (backend ? "backend" : "frontend") );
	return -ENOMEM;
    }

    if(backend) {
	vvideo_dev->s_xirq = xirq;
    } else {
	vvideo_dev->c_xirq = xirq;
    }

    VVIDEO_LOG("vvideo_dev_xirq_alloc: minor %d, backend %d, s_id %lu, c_id %lu DONE, s_xirq %lx, c_xirq %lx\n",
      vvideo_dev->minor, backend, (unsigned long)vlink->s_id, (unsigned long)vlink->c_id,
      (unsigned long)vvideo_dev->s_xirq, (unsigned long)vvideo_dev->c_xirq);

    return 0;
}

    /*
     * vvideo_dev_init() function is called to initialize a device
     * instance during driver initialization phase
     * (see vvideo_vlink_module_init()).
     *
     * It allocates communication rings if they are not
     * already allocated by peer driver.
     *
     * It allocates cross interrupts if they are not already allocated
     * by previous driver "incarnation" (cross interrupts are "restart"
     * persistent).
     *
     * Finally it initialize mutual exclusion lock and
     * waiting queue.
     */
    static void
vvideo_dev_init (VVideoDev* vvideo_dev, int minor)
{
    vvideo_dev->minor = minor;

    VVIDEO_LOG("vvideo_dev_init: minor %d\n", vvideo_dev->minor);

	/*
	 * Allocate request
	 */
    if (vvideo_dev_req_alloc(vvideo_dev) != 0) {
	return;
    }

	/*
	 * Allocate client (front-end) and server (back-end) cross interrupts
	 */
    if ((vvideo_dev_xirq_alloc(vvideo_dev, 0) != 0) ||
	(vvideo_dev_xirq_alloc(vvideo_dev, 1) != 0)
       ) {
	return;
    }

	/*
	 * Initialize mutual exclusion lock
	 * and waiting queue
	 */
    mutex_init         (&vvideo_dev->lock);
    init_waitqueue_head(&vvideo_dev->wait);

	/*
	 * Attach cross interrupt handlers
	 */
    if (vvideo_xirq_attach(vvideo_dev, vvideo_xirq_hdl) != 0) {

	vvideo_dev_cleanup(vvideo_dev);
	return;
    }

	/*
	 * Say this device has all resources allocated
	 */
    vvideo_dev->enabled = 1;

        /*
	 * Start the server thread
	 */
    init_completion(&vvideo_dev->thread_completion);
    vvideo_dev->thread_id = kernel_thread(vvideo_thread, vvideo_dev, 0);


    VVIDEO_LOG("vvideo_dev_init: minor %d enabled\n", vvideo_dev->minor);

	/*
	 * perform handshake
	 */
    vvideo_handshake(vvideo_dev);

    VVIDEO_LOG("vvideo_dev_init: minor %d DONE, handshake s_state %lu, c_state %lu\n",
      vvideo_dev->minor, (unsigned long)vvideo_dev->vlink->s_state, (unsigned long)vvideo_dev->vlink->c_state);
}


    /*
     * vvideo_dev_destroy() function is called to free devices
     * resources and destry a device instance during driver
     * exit phase (see vvideo_module_exit()).
     */
    static void
vvideo_dev_destroy (VVideoDev* vvideo_dev, int minor)
{
    (void) vvideo_dev;
    (void) minor;

    VVIDEO_LOG("vvideo_dev_destroy: minor %d\n", vvideo_dev->minor);
}


    /*
     * vvideo_module_cleanup() function is called
     * to free driver resources. It is used during driver
     * initialization phase (if something goes wrong) and
     * during driver exit phase.
     *
     * In both cases the linux driver framework guarantees
     * that there is no other activity in this driver.
     *
     * this function detach sysconfig handler.
     *
     * It destroys all device instances and then
     * free memory allocated for device descriptors.
     *
     * It destroys the device class and unregisters our char devices.
     */
    static void
vvideo_module_cleanup (void)
{
    VVIDEO_LOG("vvideo_module_cleanup\n");

	/*
	 * Detach sysconfig handler
	 */
    if (vvideo_sysconf_id != 0) {
        nkops.nk_xirq_detach(vvideo_sysconf_id);
    }
	/*
	 * Destroy all devices and free devices descriptors
	 */
    if (vvideo_devs != NULL) {
	VVideoDev* vvideo_dev;
	int	i;

	for (i = 0, vvideo_dev = vvideo_devs; i < vvideo_dev_num; i++, vvideo_dev++) {
	    if (vvideo_dev->enabled) {
		vvideo_dev_destroy(vvideo_dev, VVIDEO_MINOR_BASE + i);
	    }
	}

	kfree(vvideo_devs);
    }

    VVIDEO_LOG("vvideo_module_cleanup DONE\n");
}


// Hardware video driver entry point
extern int vvideo_hw_ops_init (vvideo_hw_ops_t** hw_ops);

    /*
     * vvideo_module_init() is the 1st function
     * called by linux driver framework. It initializes
     * our driver.
     *
     * It finds how many devices will be managed by this driver.
     *
     * It allocates memory for all devices descriptors.
     *
     * It finds all the devices managed by this driver
     * and "assign" them minors.
     *
     * Finally it attaches a sysconfig handler
     */
    static int
vvideo_module_init (void)
{
    NkPhAddr    plink;
    NkDevVlink* vlink;
    VVideoDev*	vvideo_dev;
    int 	err;
    int		minor;
    NkOsId      my_id = nkops.nk_id_get();

    VVIDEO_LOG("vvideo_module_init, my_id %ld\n", (unsigned long)my_id);

	/*
	 * Bind to the hardware video driver
	 */
    err = vvideo_hw_ops_init(&vvideo_hw_ops);
    if ((err != 0) || (vvideo_hw_ops == NULL)) {
	printk(VVIDEO_ERR "no hw ops\n");
	err = -ENODEV;
	goto out;
    }

    if (vvideo_hw_ops->version != VVIDEO_HW_OPS_VERSION) {
	printk(VVIDEO_ERR "unexpected hw ops version, expected %u, got %u\n",
	  VVIDEO_HW_OPS_VERSION, vvideo_hw_ops->version);
	err = -ENODEV;
	goto out;
    }

	/*
	 * Find how many devices should be managed by this driver
	 */
    vvideo_dev_num = 0;
    plink          = 0;

    while ((plink = nkops.nk_vlink_lookup("vvideo", plink)) != 0) {
	vlink = (NkDevVlink*) nkops.nk_ptov(plink);

	// My server link?
	if (vlink->s_id == my_id) {
	    vvideo_dev_num++;
	}
    }
	/*
	 * Nothing to do if no links found
	 */
    if (vvideo_dev_num == 0) {
	printk(VVIDEO_ERR "no vvideo vlinks found\n");
	err = -EINVAL;
	goto out;
    }

	/*
	 * Allocate memory for all device descriptors
	 */
    vvideo_devs = (VVideoDev*)kzalloc(sizeof(VVideoDev) * vvideo_dev_num, GFP_KERNEL);
    if (vvideo_devs == NULL) {
        printk(VVIDEO_ERR "out of memory\n");
	vvideo_module_cleanup();
	err = -ENOMEM;
	goto out;
    }

	/*
	 * Init all server links for this guest OS
	 */
    vvideo_dev = vvideo_devs;
    plink  = 0;
    minor  = VVIDEO_MINOR_BASE;

    while ((plink = nkops.nk_vlink_lookup("vvideo", plink)) != 0) {

	vlink = (NkDevVlink*) nkops.nk_ptov(plink);

	// My server link?
	if (vlink->s_id == my_id) {

	    vvideo_dev->plink = plink;
	    vvideo_dev->vlink = vlink;
	    vvideo_dev_init(vvideo_dev, minor);

	    if (vvideo_dev->enabled) {
		printk(VVIDEO_INFO "device vvideo%d is created"
				   " for OS#%d<-OS#%d link=%d\n",
			minor, vlink->s_id, vlink->c_id, vlink->link);
	    }

	    vvideo_dev++;
	    minor++;
	}
    }

	/*
	 * Attach sysconfig handler
	 */
    vvideo_sysconf_id = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF, vvideo_sysconf_hdl, 0);
    if (vvideo_sysconf_id == 0) {
	vvideo_module_cleanup();
	err = -ENOMEM;
	goto out;
    }

out:
    VVIDEO_LOG("vvideo_module_init, my_id %ld DONE, err %d\n", (unsigned long)my_id, err);

    printk(VVIDEO_INFO "module loaded %s\n", err ? "with errors" : "ok");

    return err;
}


    /*
     * vvideo_module_exit() is the last function
     * called by linux driver framework.
     *
     * it calls vvideo_module_cleanup() to free
     * driver resources
     */
    static void
vvideo_module_exit (void)
{
    /*
     * Perform module cleanup
     */
    vvideo_module_cleanup();

    printk(VVIDEO_INFO "module unloaded\n");
}

MODULE_LICENSE ("GPL");

module_init(vvideo_module_init);
module_exit(vvideo_module_exit);

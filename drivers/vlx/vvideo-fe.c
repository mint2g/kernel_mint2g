/*
 ****************************************************************
 *
 *  Component: VLX virtual video frontend driver
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
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#include <linux/videodev2.h>

#include <nk/nkern.h>
#include <nk/nkdev.h>
#include <nk/nk.h>

#include <vlx/vvideo_common.h>


//#define TRACE_SYSCONF
#if 0
#define VVIDEO_LOG(format, args...) printk("VVIDEO-FE: " format, ## args)
#else
#define VVIDEO_LOG(format, args...) do {} while (0)
#endif
#define VVIDEO_LOG1(format, args...) printk("VVIDEO-FE: " format, ## args)

/*
 * The virtual video devices will appear as /dev/video, and not as /dev/vvideo
 * as this front-end driver is expected to appear as a transparent drop-in
 * replacement for the back-end hardware video driver.
 * Only the extra control device will show up as /dev/vvideo (of minor 0).
 */
#define VVIDEO_NAME "video"

typedef struct VVideoBuffer {
    unsigned long vaddr;
    unsigned long paddr;
    unsigned long size;
    unsigned long offset;
} VVideoBuffer;

#define VVIDEO_BUFFER_MAX 32


typedef struct VVideoDev {
    NkPhAddr             plink;
    NkDevVlink*          vlink;
    unsigned int         minor;
    unsigned int         be_minor;
    NkPhAddr             pmem;
    unsigned char*       mem_base;
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
    int                  count;
    int                  enabled;
    int                  mmap_count;
    // Technically, buffer mapping information should be stored in per-process tables,
    // but as we support only a single open, all the buffers will actually be mapped
    // by the same process. Store everything in the same vaddr-indexed table then.
    VVideoBuffer         buffer[ VVIDEO_BUFFER_MAX ];
} VVideoDev;


#define VVIDEO_IOC_BUFINFO  1

typedef struct vvideo_bufinfo_t {
    unsigned long vaddr;  // in
    unsigned long paddr;  // out
    unsigned long size;   // out
    unsigned long offset; // out
    unsigned int  minor;  // out
} vvideo_bufinfo_t;



static int            vvideo_dev_num    = 0;    /* number of configured devices */
static VVideoDev*     vvideo_devs       = NULL;	/* array of device descriptors */
static NkXIrqId       vvideo_sysconf_id = 0;    /* xirq id for sysconf handler */
static int	      vvideo_chrdev     = 0;	/* is the character device registered */
static struct class*  vvideo_class      = NULL;	/* device class pointer */
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
static struct device* vvideo_dev0       = NULL; /* /dev/vvideo ctrl device */
#else
static struct class_device* vvideo_dev0 = NULL; /* /dev/vvideo ctrl device */
#endif


#define VVIDEO_MINOR2DEV(minor)  (&vvideo_devs[(minor) - VVIDEO_MINOR_BASE])


#define VVIDEO_MSG		"VVIDEO-FE: "
#define VVIDEO_ERR		KERN_ERR    VVIDEO_MSG
#define VVIDEO_INFO		KERN_NOTICE VVIDEO_MSG


#define VVIDEO_ASSERT(x)       do { if(!(x)) BUG(); } while (0)


    /*
     * send a NK_XIRQ_SYSCONF cross interrupt to the server (back-end) driver
     */
    static void
vvideo_sysconf_trigger (VVideoDev* vvideo_dev)
{
    NkDevVlink* link = vvideo_dev->vlink;

#ifdef TRACE_SYSCONF
	printk(VVIDEO_MSG "Sending sysconf OS#%d<-OS#%d(%d,%d)\n",
	       link->s_id,
	       link->c_id,
	       link->s_state,
	       link->c_state);
#endif
	nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, link->s_id);
}


    /*
     * send a cross interrupt to the server (back-end) driver
     */
    static void
vvideo_xirq_trigger (VVideoDev* vvideo_dev)
{
    NkDevVlink* link = vvideo_dev->vlink;

    VVIDEO_LOG("vvideo_xirq_trigger: minor %d, FIRING xirq to server\n", vvideo_dev->minor);

    nkops.nk_xirq_trigger(vvideo_dev->s_xirq, link->s_id);
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

    my_state   = &vvideo_dev->vlink->c_state;
    peer_state =  vvideo_dev->vlink->s_state;

#ifdef TRACE_SYSCONF
    printk(VVIDEO_MSG "vvideo_handshake client (me)=%d, server (peer)=%d\n",
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
		*my_state = NK_DEV_VLINK_OFF;
		vvideo_sysconf_trigger(vvideo_dev);
	    }
	    break;
    }

    return (*my_state  == NK_DEV_VLINK_ON) &&
	   (peer_state == NK_DEV_VLINK_ON);
}


    static int
vvideo_vlink_ready (VVideoDev* vvideo_dev)
{
    return vvideo_handshake(vvideo_dev);
}


    /*
     * NK_XIRQ_SYSCONFIG cross interrupt handler

     * It scans all enabled devices and wakes threads
     * sleeping on corresponding wait queues. Handshaking
     * will be performed by the thread awaken.
     */
    static void
vvideo_sysconf_hdl (void* cookie, NkXIrq xirq)
{
    VVideoDev* vvideo_dev;
    int        i;

#ifdef TRACE_SYSCONF
    printk(VVIDEO_MSG "vvideo_sysconf_hdl\n");
#endif

    for (i = 0, vvideo_dev = vvideo_devs; i < vvideo_dev_num; i++, vvideo_dev++) {

	if (vvideo_dev->enabled) {
#ifdef TRACE_SYSCONF
		printk(VVIDEO_MSG "Getting sysconf OS#%d->OS#%d(%d,%d) -> waking up dev minor %d\n",
			vvideo_dev->vlink->s_id,
			vvideo_dev->vlink->c_id,
			vvideo_dev->vlink->s_state,
		        vvideo_dev->vlink->c_state,
		        vvideo_dev->minor);
#endif
	    wake_up_interruptible(&vvideo_dev->wait);
	}
    }

#ifdef TRACE_SYSCONF
    printk(VVIDEO_MSG "vvideo_sysconf_hdl DONE\n");
#endif
}


    /*
     * cross interrupt handler.
     *
     * the server (back-end) driver sends a cross interrupt to us
     * to say it has processed our request, so the waiting thread
     * should wake up. The result of the request will be processed
     * by the awaken thread.
     * We use a single waiting semaphore because only one
     * operation is allowed at a time.
     */
    static void
vvideo_xirq_hdl (void* cookie, NkXIrq xirq)
{
    VVideoDev* vvideo_dev = (VVideoDev*)cookie;

    VVIDEO_LOG("vvideo_xirq_hdl: minor %d, WAKING UP client thread\n", vvideo_dev->minor);

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

    vvideo_dev->count = 0;

    // Shut the vlink down by setting our client state to off.
    vvideo_dev->vlink->c_state = NK_DEV_VLINK_OFF;

    vvideo_sysconf_trigger(vvideo_dev);

    VVIDEO_LOG("vvideo_dev_cleanup: minor %d done\n", vvideo_dev->minor);
}


    /*
     * attach a cross interrupt handler to the device.
     */
    static int
vvideo_xirq_attach (VVideoDev* vvideo_dev, NkXIrqHandler hdl)
{
    NkDevVlink* link = vvideo_dev->vlink;
    NkXIrq      xirq;
    NkXIrqId	xid;

    VVIDEO_LOG("vvideo_xirq_attach: minor %d\n", vvideo_dev->minor);

    // Attach to the client (front-end) xirq
    xirq = vvideo_dev->c_xirq;

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
vvideo_post_done (VVideoDev* vvideo_dev)
{
    NkDevVlink*      link       = vvideo_dev->vlink;
    VVideoRequest*   vvideo_req = vvideo_dev->req;
    int ret = 0;

    // link down on server side?
    if (link->s_state != NK_DEV_VLINK_ON) {
	ret = 1;
    }
    else
    // result available?
    if (vvideo_req->result != VVIDEO_REQ_PENDING) {
	ret = 1;
    }

    VVIDEO_LOG("vvideo_post_done: minor %d, s_state %lu, req %d, result %x -> ret %d\n",
      vvideo_dev->minor, (unsigned long)link->s_state, vvideo_req->req, vvideo_req->result, ret);

    return ret;
}


    static int
vvideo_post (VVideoDev* vvideo_dev, int req)
{
    VVideoRequest* vvideo_req = vvideo_dev->req;

    VVIDEO_LOG("vvideo_post: minor %d, req %d\n", vvideo_dev->minor, req);

    vvideo_req->req    = req;
    vvideo_req->result = VVIDEO_REQ_PENDING;

    // Post the request by sending a cross interrupt to the server (back-end)
    vvideo_xirq_trigger(vvideo_dev);

    // Wait for the server to process the request (or an error to occur)
    if (wait_event_interruptible(vvideo_dev->wait, vvideo_post_done(vvideo_dev))) {
	return -EINTR;
    }

    VVIDEO_LOG("vvideo_post: minor %d AWAKEN, s_state %lu, req %d, result %d/%08x\n",
      vvideo_dev->minor, (unsigned long)vvideo_dev->vlink->s_state, vvideo_req->req, vvideo_req->result, vvideo_req->result);

    vvideo_req->req = VVIDEO_REQ_NONE;

    if (vvideo_req->result == VVIDEO_REQ_PENDING) {
	return -EPIPE;
    }

    return vvideo_req->result;
}


    /*
     * Implement open operation required by char devices.
     * For the 1st open it attaches cross interrupt
     * handler and perform "handshake" with peer server
     * (back-end)  driver
     */
    static int
vvideo_open (struct inode* inode, struct file* file)
{
    unsigned int minor   = iminor(inode);
    VVideoDev*   vvideo_dev;
    VVideoOpen*  vvideo_req_open;
    int err;

    VVIDEO_LOG("vvideo_open: inode minor %d\n", minor);

    if (minor == 0) {
	VVIDEO_LOG("vvideo_open: ctrl device OPENED\n");
	return 0;
    }

    if ((minor < VVIDEO_MINOR_BASE) ||
        (minor >= (VVIDEO_MINOR_BASE + vvideo_dev_num))) {
	return -ENXIO;
    }

    vvideo_dev = VVIDEO_MINOR2DEV(minor);
    vvideo_req_open = vvideo_dev->req_open;

	/*
	 * check if we was able to allocate all resources
	 * for this device
	 */
    if (vvideo_dev->enabled == 0) {
	return -ENXIO;
    }

    if (mutex_lock_interruptible(&vvideo_dev->lock)) {
	return -EINTR;
    }

#if 0
    // The backend hardware driver supports only a single open.
    if (vvideo_dev->count != 0) {
	VVIDEO_LOG("vvideo_open: device %d already open\n", minor);
	mutex_unlock(&vvideo_dev->lock);
	return -EAGAIN;
    }
#endif

	/*
	 * Increase usage counter
	 */
    vvideo_dev->count++;

    if (vvideo_dev->count != 1) {
	VVIDEO_LOG("vvideo_open: device %d re-opened, new count %d\n", minor, vvideo_dev->count);
	mutex_unlock(&vvideo_dev->lock);

	// not the 1st open, we're done.
	return 0;
    }

    VVIDEO_LOG("vvideo_open: minor %d, FIRST OPEN\n", vvideo_dev->minor);

	/*
	 * Do more initialization at 1st open
	 */

	/*
	 * Attach cross interrupt handlers
	 */
    if (vvideo_xirq_attach(vvideo_dev, vvideo_xirq_hdl) != 0) {

	vvideo_dev_cleanup(vvideo_dev);

	mutex_unlock(&vvideo_dev->lock);
	return -ENOMEM;
    }

    VVIDEO_LOG("vvideo_open: minor %d, performing handshake\n", vvideo_dev->minor);

	/*
	 * perform handshake until the vlink is ready
	 */
    if (wait_event_interruptible(vvideo_dev->wait, vvideo_vlink_ready(vvideo_dev))) {
		/*
		 * if open fails because of signal
		 * perform clean up and exit
		 */
	    vvideo_dev_cleanup(vvideo_dev);

	    mutex_unlock(&vvideo_dev->lock);
	    return -EINTR;
    }

    VVIDEO_LOG("vvideo_open: minor %d, VLINK READY\n", vvideo_dev->minor);


#ifdef TRACE_SYSCONF
    printk(VVIDEO_MSG "vvideo_open for minor=%d is ok\n", minor);
#endif

    vvideo_req_open->minor = minor;


    VVIDEO_LOG("vvideo_open: minor %d, POSTING OPEN REQ\n", vvideo_dev->minor);

    err = vvideo_post(vvideo_dev, VVIDEO_REQ_OPEN);

    VVIDEO_LOG("vvideo_open: minor %d, OPEN REQ DONE, err %d\n", vvideo_dev->minor, err);

    if (err) {
	vvideo_dev_cleanup(vvideo_dev);

	mutex_unlock(&vvideo_dev->lock);
	return err;
    }

    vvideo_dev->be_minor = vvideo_req_open->minor;

    if (vvideo_dev->desc->pmem_size) {
	// Allocate the video buffers from the persistent shared memory area.
	vvideo_dev->pmem = nkops.nk_pmem_alloc(vvideo_dev->plink, 0, vvideo_dev->desc->pmem_size);
	if (!vvideo_dev->pmem) {
	    printk(VVIDEO_INFO "vvideo_open: nk_pmem_alloc(%lu bytes) failed\n", vvideo_dev->desc->pmem_size);

	    vvideo_dev_cleanup(vvideo_dev);

	    mutex_unlock(&vvideo_dev->lock);
	    return -ENOMEM;
	}

	// Map the pmem video buffers.
	vvideo_dev->mem_base = (unsigned char*) nkops.nk_mem_map(vvideo_dev->pmem, vvideo_dev->desc->pmem_size);
	if (!vvideo_dev->mem_base) {
	    printk(VVIDEO_INFO "vvideo_open: nk_pmem_map(%lu bytes) failed\n", vvideo_dev->desc->pmem_size);

	    vvideo_dev_cleanup(vvideo_dev);

	    mutex_unlock(&vvideo_dev->lock);
	    return -ENOMEM;
	}

	VVIDEO_LOG("vvideo_open: nk_pmem_alloc(%lu bytes) succeeded\n", vvideo_dev->desc->pmem_size);
    }

    mutex_unlock(&vvideo_dev->lock);
    return 0;
}


    /*
     * implement release (close) operation for character devices.
     * for the last close it shuts the link down.
     *
     * Note that ioctl operations can detect
     * that something is wrong with peer driver
     * (it went to NK_DEV_OFF state) and will exit
     * with -EPIPE error code.
     *
     * Applications are expected to perform appropriate
     * actions for this situation, typically to call
     * close() to shut the link down and change the
     * link state to NK_DEV_VLINK_OFF.
     *
     * After that the application may call open() to reopen
     * the device and resume the service.
     */
    static int
vvideo_release (struct inode* inode, struct file* file)
{
    unsigned int minor = iminor(inode);
    VVideoDev*   vvideo_dev;
    VVideoRelease* vvideo_req_release;
    int err = 0;

    VVIDEO_LOG("vvideo_release: inode minor %d\n", minor);

    if (minor == 0) {
	VVIDEO_LOG("vvideo_release: ctrl device CLOSED\n");
	return 0;
    }

    vvideo_dev = VVIDEO_MINOR2DEV(minor);
    vvideo_req_release = vvideo_dev->req_release;

    if (mutex_lock_interruptible(&vvideo_dev->lock)) {
	return -EINTR;
    }
	/*
	 * Decrease usage counter
	 */
    vvideo_dev->count--;

	/*
	 * Do clean up at last close
	 */
    if (vvideo_dev->count == 0) {

	VVIDEO_LOG("vvideo_release: minor %d, LAST CLOSE, POSTING CLOSE REQ\n", vvideo_dev->minor);

	vvideo_req_release->minor = minor;

	err = vvideo_post(vvideo_dev, VVIDEO_REQ_RELEASE);

	VVIDEO_LOG("vvideo_release: minor %d, CLOSE REQ DONE, err %d\n", vvideo_dev->minor, err);

	vvideo_dev_cleanup(vvideo_dev);

#ifdef TRACE_SYSCONF
	printk(VVIDEO_MSG "vvideo_release for minor=%d is ok\n", minor);
#endif
    }

    mutex_unlock(&vvideo_dev->lock);

    return err;
}

/*
 * ripped from omap_vout_uservirt_to_phys() (see drivers/media/video/omap/omap_vout.c)
 */
static u32
vvideo_uservirt_to_phys (u32 virtp)
{
	unsigned long physp = 0;
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;

	vma = find_vma(mm, virtp);
	/* For kernel direct-mapped memory, take the easy way */
	if (virtp >= PAGE_OFFSET) {
		physp = virt_to_phys((void *) virtp);
	} else if ((vma) && (vma->vm_flags & VM_IO)
			&& (vma->vm_pgoff)) {
		/* this will catch, kernel-allocated,
		   mmaped-to-usermode addresses */
		physp = (vma->vm_pgoff << PAGE_SHIFT) + (virtp - vma->vm_start);
	} else {
		/* otherwise, use get_user_pages() for general userland pages */
		int res, nr_pages = 1;
		struct page *pages;
		down_read(&current->mm->mmap_sem);

		res = get_user_pages(current, current->mm, virtp, nr_pages,
				1, 0, &pages, NULL);
		up_read(&current->mm->mmap_sem);

		if (res == nr_pages) {
			physp =  __pa(page_address(&pages[0]) +
					(virtp & ~PAGE_MASK));
		} else {
			printk("vvideo_uservirt_to_phys: get_user_pages() failed\n");
			return 0;
		}
	}

	return physp;
}


/*
 * inspired by video_usercopy() (see drivers/media/video/v4l2-ioctl.c).
 */
    static int
vvideo_ioctl_usercopy (VVideoDev* vvideo_dev, unsigned int cmd, unsigned long arg)
{
    VVideoIoctl* vvideo_req_ioctl = vvideo_dev->req_ioctl;
    int	         err              = -EINVAL;
    int          is_ext_ctrls     = 0;
    void __user* ext_ctrls_ptr    = NULL;

    is_ext_ctrls = ((cmd == VIDIOC_S_EXT_CTRLS) ||
                    (cmd == VIDIOC_G_EXT_CTRLS) ||
                    (cmd == VIDIOC_TRY_EXT_CTRLS));

    vvideo_req_ioctl->cmd       = cmd;
    vvideo_req_ioctl->arg_size  = 0;
    vvideo_req_ioctl->ext_size  = 0;
    vvideo_req_ioctl->arg_value = arg; // if arg_size == 0

    VVIDEO_LOG("vvideo_ioctl_usercopy: minor %d, cmd %08x, size %d (is_ext %d), arg %08lx\n",
      vvideo_dev->minor, cmd, _IOC_SIZE(cmd), is_ext_ctrls, arg);

    switch (_IOC_DIR(cmd)) {

    case _IOC_NONE:
	vvideo_req_ioctl->arg_value = 0;
	break;
    case _IOC_READ:
    case _IOC_WRITE:
    case _IOC_WRITE | _IOC_READ:

	vvideo_req_ioctl->arg_size = _IOC_SIZE(cmd);

	// A non-v4l2_ext_controls struct is granted to fill both arg[] and ext[].
	if (vvideo_req_ioctl->arg_size > (VVIDEO_REQ_IOCTL_ARG_SIZE + VVIDEO_REQ_IOCTL_EXT_SIZE)) {

	    printk(VVIDEO_ERR "ioctl arg too large, size %u, cmd %08x!!!\n", vvideo_req_ioctl->arg_size, cmd);
	    err =  -EINVAL;
	    goto out;
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE) {

	    if (copy_from_user(vvideo_req_ioctl->arg, (void __user *)arg, vvideo_req_ioctl->arg_size)) {
		err = -EFAULT;
		goto out;
	    }
	}
	break;
    }

    if (is_ext_ctrls) {
	struct v4l2_ext_controls* p = (struct v4l2_ext_controls*) vvideo_req_ioctl->arg;

	p->error_idx  = p->count;
	ext_ctrls_ptr = (void __user *)p->controls;

	if (p->count) {

	    vvideo_req_ioctl->ext_size = sizeof(struct v4l2_ext_control) * p->count;

	    if (vvideo_req_ioctl->ext_size > VVIDEO_REQ_IOCTL_EXT_SIZE) {

		printk(VVIDEO_ERR "ioctl ext ctrl too large, size %u, cmd %08x!!!\n", vvideo_req_ioctl->ext_size, cmd);
		err =  -EINVAL;
		goto out;
	    }

	    // The v4l2_ext_controls struct fits in arg[] so ext[] is still empty.
	    if (copy_from_user(vvideo_req_ioctl->ext, ext_ctrls_ptr, vvideo_req_ioctl->ext_size)) {
		err = -EFAULT;
		goto out_ext_ctrl;
	    }
	    p->controls = (void*)vvideo_req_ioctl->ext;  // do that on back-end side.
	}
    }

    VVIDEO_LOG("vvideo_ioctl_usercopy: minor %d, POSTING IOCTL REQ, cmd %08x, arg_value %08lx, arg_size %08x, ext_size %08x\n",
      vvideo_dev->minor, cmd, vvideo_req_ioctl->arg_value, vvideo_req_ioctl->arg_size, vvideo_req_ioctl->ext_size);

    if (cmd == VIDIOC_QBUF) {

	struct v4l2_buffer* qbuf = (struct v4l2_buffer*) vvideo_req_ioctl->arg;

	qbuf->reserved = 0;

	// Provide the back-end with the physical address of the user buffer.
	// This user buffer is either a small-resolution, pmem-backed overlay
	// buffer used for video capture preview, or a full-resolution,
	// user-allocated buffer used for video snapshot.
	if ((qbuf->memory == V4L2_MEMORY_USERPTR) && qbuf->m.userptr) {
	    qbuf->reserved = vvideo_uservirt_to_phys(qbuf->m.userptr);

	    VVIDEO_LOG("vvideo_ioctl_usercopy: buffer %d, userptr 0x%lx, len %u -> phys 0x%x\n",
	      qbuf->index, qbuf->m.userptr, qbuf->length, qbuf->reserved);
	}
    }

    err = vvideo_post(vvideo_dev, VVIDEO_REQ_IOCTL);

    VVIDEO_LOG("vvideo_ioctl_usercopy: minor %d, IOCTL REQ DONE, cmd %08x, err %d\n",
      vvideo_dev->minor, cmd, err);

    if ((err == -EINTR) || (err == -EPIPE)) {
	goto out;
    }

    if (is_ext_ctrls) {
	struct v4l2_ext_controls* p = (struct v4l2_ext_controls*) vvideo_req_ioctl->ext;

	p->controls = (void *)ext_ctrls_ptr;

	if ((p->count) && (err == 0) && copy_to_user(ext_ctrls_ptr, vvideo_req_ioctl->ext, vvideo_req_ioctl->ext_size)) {
	    err = -EFAULT;
	}

	goto out_ext_ctrl;
    }
    if (err < 0) {
	goto out;
    }

    if (cmd == VIDIOC_DQBUF) {

	struct v4l2_buffer* dqbuf = (struct v4l2_buffer*) vvideo_req_ioctl->arg;

	// Copy the content of the pmem (holding a full-reolution video snapshot)
	// to the user-allocated, non-pmem buffer.
	if (dqbuf->reserved == 0xFEEDFEED) {
	    VVIDEO_LOG1("vvideo_ioctl_usercopy: copying pmem %p to buffer %d, userptr 0x%lx, len %u, bytesused %u\n",
	      vvideo_dev->mem_base, dqbuf->index, dqbuf->m.userptr, dqbuf->length, dqbuf->bytesused);

	    if (copy_to_user((void __user *)dqbuf->m.userptr, vvideo_dev->mem_base, dqbuf->bytesused)) {
		err = -EFAULT;
	    }
	}

	dqbuf->reserved = 0;
    }

out_ext_ctrl:
    switch (_IOC_DIR(cmd)) {
    case _IOC_READ:
    case _IOC_READ |_IOC_WRITE:
	if (copy_to_user((void __user *)arg, vvideo_req_ioctl->arg, _IOC_SIZE(cmd))) {
	    err = -EFAULT;
	}
	break;
    }

out:
    return err;
}


    static long
vvideo_ctrl_ioctl (unsigned int cmd, unsigned long arg)
{
    VVIDEO_LOG("vvideo_ctrl_ioctl: cmd %08x, arg %08lx\n",  cmd, arg);

    switch (cmd) {
    default:
	return -EINVAL;

    case VVIDEO_IOC_BUFINFO: {
	VVideoDev*       vvideo_dev;
	VVideoBuffer*    buffer;
	vvideo_bufinfo_t info;
	unsigned long    vaddr;
	int minor;
	int i;

        if (copy_from_user(&info, (void*)arg, sizeof(info))) {
            return -EFAULT;
	}

	vaddr = info.vaddr;

	VVIDEO_LOG("vvideo_ctrl_ioctl: request info about buffer %08lx\n", vaddr);

	info.vaddr  = 0;
	info.paddr  = 0;
	info.size   = 0;
	info.offset = 0;
	info.minor  = 0;

	for (minor = VVIDEO_MINOR_BASE; minor < VVIDEO_MINOR_BASE + vvideo_dev_num; minor++) {

	    vvideo_dev = VVIDEO_MINOR2DEV(minor);
	    if (!vvideo_dev->enabled) {
		continue;
	    }

	    buffer = vvideo_dev->buffer;

	    for (i = 0; i < VVIDEO_BUFFER_MAX; i++, buffer++) {
		if (vaddr == buffer->vaddr) {
		    info.vaddr  = buffer->vaddr;
		    info.paddr  = buffer->paddr;
		    info.size   = buffer->size;
		    info.offset = buffer->offset;
		    /*
		     * Use minor returned by the Back-end. This minor is unique
		     * and is used to identify the right video buffer set in
		     * the vshm area
		     */
		    info.minor  = vvideo_dev->be_minor;

		    VVIDEO_LOG("vvideo_ctrl_ioctl: found info about buffer %08lx: paddr %08lx, size %lu, offset %lu, minor %u\n",
		      info.vaddr, info.paddr, info.size, info.offset, info.minor);

		    goto found;
		}
	    }
	}

	VVIDEO_LOG("vvideo_ctrl_ioctl: NO info about buffer %08lx\n", vaddr);
found:
	if (copy_to_user((void*)arg, &info, sizeof(info))) {
	    return -EFAULT;
	}
	break;
    }
    }

    return 0;
}

char* vvideo_ioc_name (unsigned int ioc);

    static long
vvideo_ioctl (struct file* file, unsigned int cmd, unsigned long arg)
{
    struct inode* inode = file->f_path.dentry->d_inode;
    unsigned int  minor = iminor(inode);
    VVideoDev*    vvideo_dev;
    int		  ret;

    if (cmd == 0) {
	return -EINVAL;
    }

    if (minor == 0) {
	return vvideo_ctrl_ioctl(cmd, arg);
    }

    vvideo_dev = VVIDEO_MINOR2DEV(minor);

    VVIDEO_LOG("vvideo_ioctl: minor %d, cmd %08x (%s), arg %08lx\n",
      vvideo_dev->minor, cmd, vvideo_ioc_name(cmd), arg);

    if (mutex_lock_interruptible(&vvideo_dev->lock)) {
	return -EINTR;
    }

    ret = vvideo_ioctl_usercopy(vvideo_dev, cmd, arg);

    VVIDEO_LOG("vvideo_ioctl: minor %d, cmd %08x (%s), arg %08lx DONE, ret %d\n",
      vvideo_dev->minor, cmd, vvideo_ioc_name(cmd), arg, ret);

    mutex_unlock(&vvideo_dev->lock);

    return ret;
}


static void
vvideo_vm_open (struct vm_area_struct* vma)
{
    VVideoDev*    vvideo_dev = (VVideoDev*) vma->vm_private_data;

    VVIDEO_LOG(VVIDEO_MSG "vvideo_vm_open [vma=%08lx-%08lx], offset %lx\n",
      vma->vm_start, vma->vm_end, vma->vm_pgoff << PAGE_SHIFT);

    vvideo_dev->mmap_count++;
}


static int vvideo_munmap (VVideoDev* vvideo_dev, unsigned long pgoff, unsigned long size);

static void
vvideo_vm_close (struct vm_area_struct *vma)
{
    VVideoDev*    vvideo_dev = (VVideoDev*) vma->vm_private_data;
    VVideoBuffer* buffer     = vvideo_dev->buffer;
    unsigned long pgoff;
    unsigned long size;
    int           i;

    size = vma->vm_end - vma->vm_start;

    VVIDEO_LOG(VVIDEO_MSG "vvideo_vm_close [vma=%08lx-%08lx], size %lu\n",
      vma->vm_start, vma->vm_end, size);

    for (i = 0; i < VVIDEO_BUFFER_MAX; i++, buffer++) {
	if (buffer->vaddr == vma->vm_start) {

	    pgoff = buffer->offset >> PAGE_SHIFT;

	    VVIDEO_LOG(VVIDEO_MSG "vvideo_vm_close [vma=%08lx-%08lx], -> paddr %lx, offset %lu, pgoff %lx\n",
	      vma->vm_start, vma->vm_end, buffer->paddr, buffer->offset, pgoff);

	    vvideo_munmap(vvideo_dev, pgoff, size);

	    VVIDEO_LOG("vvideo_vm_close: erasing info about buffer %08lx: paddr %lx, size %lu, offset %lu, minor %u\n",
	      buffer->paddr, buffer->vaddr, buffer->size, buffer->offset, vvideo_dev->minor);

	    buffer->paddr  = 0;
	    buffer->vaddr  = 0;
	    buffer->size   = 0;
	    buffer->offset = 0;
	    break;
	}
    }

    if (i == VVIDEO_BUFFER_MAX) {
	printk(VVIDEO_ERR "minor %d, buffer %08lx not in list!", vvideo_dev->minor, vma->vm_start);
    }

    vvideo_dev->mmap_count--;
}


static struct vm_operations_struct vvideo_vm_ops = {
	.open  = vvideo_vm_open,
	.close = vvideo_vm_close,
};


    static int
vvideo_do_mmap (VVideoDev* vvideo_dev, struct vm_area_struct* vma,
                unsigned long paddr, unsigned long size, unsigned long pgoff, unsigned int cacheable)
{
    vma->vm_ops          = &vvideo_vm_ops;
    vma->vm_private_data = (void*)vvideo_dev;

    VVIDEO_LOG("vvideo_do_mmap: minor %d, paddr %08lx, size %ld, cacheable %u\n", vvideo_dev->minor, paddr, size, cacheable);

    cacheable = 0; // force non-cacheable

    if (!cacheable) {
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
    }

    vma->vm_flags |= VM_IO;

    if (io_remap_pfn_range(vma, vma->vm_start, (unsigned long)paddr >> PAGE_SHIFT, size, vma->vm_page_prot)) {
	return -EAGAIN;
    }

    VVIDEO_LOG("vvideo_do_mmap: minor %d, vma start %08lx, end %08lx, size %ld, flags %08lx, prot %08lx, pgoff %08lx, paddr %08lx\n",
      vvideo_dev->minor, vma->vm_start, vma->vm_end, size, vma->vm_flags, vma->vm_page_prot, vma->vm_pgoff, paddr);

    vvideo_dev->mmap_count++;

    VVIDEO_LOG("vvideo_do_mmap: minor %d, paddr %08lx, size %ld, cacheable %u DONE, mmap_count %d\n",
      vvideo_dev->minor, paddr, size, cacheable, vvideo_dev->mmap_count);

    return 0;

}


    static int
vvideo_mmap (struct file* file, struct vm_area_struct* vma)
{
    unsigned int  minor           = iminor(file->f_path.dentry->d_inode);
    VVideoDev*    vvideo_dev      = NULL;
    VVideoMMap*   vvideo_req_mmap = NULL;
    unsigned long size            = 0;
    unsigned long pgoff           = 0;
    int           err             = 0;

    vvideo_dev      = VVIDEO_MINOR2DEV(minor);
    vvideo_req_mmap = vvideo_dev->req_mmap;

    if (mutex_lock_interruptible(&vvideo_dev->lock)) {
	return -EINTR;
    }

    size  = vma->vm_end - vma->vm_start;
    pgoff = vma->vm_pgoff;

    VVIDEO_LOG("vvideo_mmap: minor %d, vm_start %lx, vm_end %lx, vm_pgoff %lx, offset %lu, size %lu\n",
      vvideo_dev->minor, vma->vm_start, vma->vm_end, pgoff, pgoff << PAGE_SHIFT, size);

    vvideo_req_mmap->pgoff     = pgoff;
    vvideo_req_mmap->size      = size;
    vvideo_req_mmap->paddr     = 0;
    vvideo_req_mmap->cacheable = 0;

    err = vvideo_post(vvideo_dev, VVIDEO_REQ_MMAP);

    VVIDEO_LOG("vvideo_mmap: minor %d, vm_start %lx, vm_end %lx, vm_pgoff %lx, offset %lu, size %lu DONE, err %d -> paddr %lx\n",
      vvideo_dev->minor, vma->vm_start, vma->vm_end, pgoff, pgoff << PAGE_SHIFT, size, err, vvideo_req_mmap->paddr);

    if (err) {
	goto out;
    }

    err = vvideo_do_mmap(vvideo_dev, vma, vvideo_req_mmap->paddr, size, pgoff, vvideo_req_mmap->cacheable);

    if (err == 0) {
	VVideoBuffer* buffer = vvideo_dev->buffer;
	int i;

	for (i = 0; i < VVIDEO_BUFFER_MAX; i++, buffer++) {
	    if (buffer->vaddr == 0) {
		buffer->vaddr  = vma->vm_start;
		buffer->paddr  = vvideo_req_mmap->paddr;
		buffer->size   = size;
		buffer->offset = pgoff << PAGE_SHIFT;

		VVIDEO_LOG("vvideo_mmap: saved info about buffer %08lx: size %lu, offset %lu, minor %u, paddr %lx\n",
		  buffer->vaddr, buffer->size, buffer->offset, vvideo_dev->minor, buffer->paddr);
		break;
	    }
	}

	if (i == VVIDEO_BUFFER_MAX) {
	    printk(VVIDEO_ERR "minor %d, buffer list full!", vvideo_dev->minor);
	}
    }

out:
    mutex_unlock(&vvideo_dev->lock);

    return err;

}


    static int
vvideo_munmap (VVideoDev* vvideo_dev, unsigned long pgoff, unsigned long size)
{
    VVideoMUnmap* vvideo_req_munmap = NULL;
    int           err               = 0;

    vvideo_req_munmap = vvideo_dev->req_munmap;

    if (mutex_lock_interruptible(&vvideo_dev->lock)) {
	return -EINTR;
    }

    VVIDEO_LOG("vvideo_munmap: minor %d, pgoff %lx, offset %lu, size %lu\n",
      vvideo_dev->minor, pgoff, pgoff << PAGE_SHIFT, size);

    vvideo_req_munmap->pgoff = pgoff;
    vvideo_req_munmap->size  = size;

    err = vvideo_post(vvideo_dev, VVIDEO_REQ_MUNMAP);

    VVIDEO_LOG("vvideo_munmap: minor %d, pgoff %lx, offset %lu, size %lu DONE, err %d\n",
      vvideo_dev->minor, pgoff, pgoff << PAGE_SHIFT, size, err);

    mutex_unlock(&vvideo_dev->lock);

    return err;
}


    /*
     * this data structure will we passed as a parameter
     * to linux character device framework and inform it
     * we have implemented all 5 basic operations (open,
     * ioctl, mmap, close, poll)
     */
static const struct file_operations vvideo_file_ops = {
    .owner	= THIS_MODULE,
    .open	= vvideo_open,
    .unlocked_ioctl = vvideo_ioctl,
    .mmap	= vvideo_mmap,
    .release	= vvideo_release,
    .llseek	= no_llseek,
};


    /*
     * vvideo_dev_req_alloc() is a helper function to
     * allocate communication data structures shared
     * with the server (back-end).
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

    memset(vvideo_dev->req, 0x00, sizeof(VVideoRequest));

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
     * It creates "class device" so our communication device will
     * be "visible" by applications as /dev/vvideo*
     *
     * Finally it initialize mutual exclusion lock and
     * waiting queue.
     */
    static void
vvideo_dev_init (VVideoDev* vvideo_dev, int minor)
{
    NkDevVlink*    vlink = vvideo_dev->vlink;
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    struct device* cls_dev;
#else
    struct class_device* cls_dev;
#endif

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
	 * create a class device
	 */
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    cls_dev = device_create
#else
    cls_dev = class_device_create
#endif
	(vvideo_class, NULL, MKDEV(VVIDEO_MAJOR, minor),
	 NULL, VVIDEO_NAME "%d", minor);
    if (IS_ERR(cls_dev)) {
	printk(VVIDEO_ERR "OS#%d<-OS#%d link=%d class device create failed"
			   " err =%ld\n",
		vlink->s_id, vlink->c_id, vlink->link, PTR_ERR(cls_dev));
	return;
    }
	/*
	 * Initialize mutual exclusion lock
	 * and waiting queue
	 */
    mutex_init         (&vvideo_dev->lock);
    init_waitqueue_head(&vvideo_dev->wait);

	/*
	 * Say this device has all resources allocated
	 */
    vvideo_dev->enabled = 1;

    VVIDEO_LOG("vvideo_dev_init: minor %d enabled\n", vvideo_dev->minor);
}

    /*
     * vvideo_dev_destroy() function is called to free devices
     * resources and destry a device instance during driver
     * exit phase (see vvideo_module_exit()).
     *
     * actually in our example it only destroys "class device"
     * because all other device resources (circular buffers
     * and cross interrupts) are persistent.
     */
    static void
vvideo_dev_destroy (VVideoDev* vvideo_dev, int minor)
{
    VVIDEO_LOG("vvideo_dev_destroy: minor %d\n", vvideo_dev->minor);

	/*
	 * Destroy the character device
	 */
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    device_destroy
#else
    class_device_destroy
#endif
	(vvideo_class, MKDEV(VVIDEO_MAJOR, minor));
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

	/*
	 * Destroy the ctrl device
	 */
    if (vvideo_dev0 && vvideo_class) {
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
	device_destroy
#else
	class_device_destroy
#endif
	    (vvideo_class, MKDEV(VVIDEO_MAJOR, 0));
    }

	/*
	 * Destroy the device class
	 */
    if (vvideo_class != NULL) {
	class_destroy(vvideo_class);
    }
	/*
	 * Unregister the character device.
	 */
    if (vvideo_chrdev != 0) {
	unregister_chrdev(VVIDEO_MAJOR, VVIDEO_NAME);
    }

    VVIDEO_LOG("vvideo_module_cleanup DONE\n");
}


    /*
     * vvideo_module_init() is the 1st function
     * called by linux driver framework. It initializes
     * our driver.
     *
     * It finds how many devices will be managed by this driver.
     *
     * It registers them as a character devices
     *
     * It creates the class device
     *
     * It allocates memory for all devices descriptors.
     *
     * It finds all the devices managed by this driver
     * and "assign" them minors *sequentially*.
     *
     * Finally it attaches a sysconfig handler
     */
    static int
vvideo_module_init (void)
{
    NkPhAddr       plink;
    NkDevVlink*    vlink;
    VVideoDev*	   vvideo_dev;
    int 	   ret;
    int		   minor;
    NkOsId         my_id = nkops.nk_id_get();

    VVIDEO_LOG("vvideo_module_init, my_id %ld\n", (unsigned long)my_id);

	/*
	 * Find how many devices should be managed by this driver
	 */
    vvideo_dev_num = 0;
    plink          = 0;

    while ((plink = nkops.nk_vlink_lookup("vvideo", plink))) {
	vlink = nkops.nk_ptov(plink);

	// My client link?
	if (vlink->c_id == my_id) {
	    vvideo_dev_num++;
	}
    }

	/*
	 * Nothing to do if no links found
	 */
    if (vvideo_dev_num == 0) {
	printk(VVIDEO_ERR "no vvideo vlinks found\n");
	ret = -EINVAL;
	goto out;
    }

	/*
	 * Register the character device
	 */
    if ((ret = register_chrdev(VVIDEO_MAJOR, VVIDEO_NAME, &vvideo_file_ops))) {
        printk(VVIDEO_ERR "can't register vvideo device\n");
	goto out;
    } else {
	vvideo_chrdev = 1;
    }

	/*
         * Create device class
	 */
    vvideo_class = class_create(THIS_MODULE, VVIDEO_NAME);
    if (IS_ERR(vvideo_class)) {
	ret      = PTR_ERR(vvideo_class);
	vvideo_class = NULL;
	vvideo_module_cleanup();
        printk(VVIDEO_ERR "can't create vvideo class\n");
	goto out;
    }

	/*
	 * Allocate memory for all device descriptors
	 */
    vvideo_devs = (VVideoDev*)kzalloc(sizeof(VVideoDev) * vvideo_dev_num, GFP_KERNEL);
    if (vvideo_devs == NULL) {
        printk(VVIDEO_ERR "out of memory\n");
	vvideo_module_cleanup();
	ret = -ENOMEM;
	goto out;
    }

	/*
	 * Create /dev/vvideo control char device
	 */
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    vvideo_dev0 = device_create
#else
    vvideo_dev0 = class_device_create
#endif
	(vvideo_class, NULL, MKDEV(VVIDEO_MAJOR, 0), NULL, "vvideo");
    if (IS_ERR(vvideo_dev0)) {
        printk(VVIDEO_ERR "could not create ctrl dev\n");
	vvideo_module_cleanup();
	ret = -ENOMEM;
	goto out;
    }

	/*
	 * Find all client links for this guest OS, and assign minors to them sequentially
	 */
    vvideo_dev = vvideo_devs;
    plink  = 0;
    minor  = VVIDEO_MINOR_BASE;

    while ((plink = nkops.nk_vlink_lookup("vvideo", plink))) {

	vlink = nkops.nk_ptov(plink);

	// My client link?
	if (vlink->c_id == my_id) {

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
	ret = -ENOMEM;
	goto out;
    }

out:
    VVIDEO_LOG("vvideo_module_init, my_id %ld DONE, vvideo_dev_num %d, ret %d\n", (unsigned long)my_id, vvideo_dev_num, ret);

    printk(VVIDEO_INFO "module loaded %s\n", ret ? "with errors" : "ok");

    return ret;
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

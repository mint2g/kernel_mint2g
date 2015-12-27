/*
 ****************************************************************
 *
 *  Component: VLX virtual blitter frontend driver
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
 *    Vladimir Grouzdev (Vladimir.Grouzdev@redbend.com)
 *    Christian Jacquemot (Christian.Jacquemot@redbend.com)
 *
 ****************************************************************
 */

#include <linux/version.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/mutex.h>

#include <nk/nkern.h>
#include <vlx/vblitter_common.h>
#include "vrpc.h"

//#define VBLITTER_DEBUG

#define TRACE(format, args...) 	 printk("VBLITTER-FE: " format, ## args)
#define ETRACE(format, args...)  printk("VBLITTER-FE: [E] " format, ## args)

#ifdef VBLITTER_DEBUG
#define DTRACE(format, args...)  printk("VBLITTER-FE: [D] " format, ## args)
#else
#define DTRACE(format, args...)  do {} while (0)
#endif
#define DTRACE0(format, args...) do {} while (0)


typedef struct vblitter {
    struct vrpc_t*          vrpc;
    void*                   data;
    vrpc_size_t             msize;
    struct mutex            mutex;
} vblitter_t;


static vblitter_t _vblitter;


    static vrpc_size_t
_vblitter_call (int cmd, vblit_blit_req_list_t* arg)
{
    struct vrpc_t*  vrpc = _vblitter.vrpc;
    vblitter_req_t* req  = _vblitter.data;
    vrpc_size_t     size = 0;
    int             err  = 0;

    for (;;) {
        req->cmd = cmd;
        req->arg = *arg;
	size     = sizeof(vblitter_req_t);

	err = vrpc_call(vrpc, &size);
	if (err == 0) {
	    break;
	}

	vrpc_close(vrpc);
	if (vrpc_client_open(vrpc, 0, 0)) {
	    BUG();
	}
    }

    return size;
}


    static int
__vblitter_blit (vblit_blit_req_list_t* req_list)
{
    vrpc_size_t     size;
    vblitter_res_t* res = _vblitter.data;

    size = _vblitter_call(VBLITTER_CMD_BLIT, req_list);

    DTRACE("_vblitter_call() -> size %u\n", size);

    if (size != sizeof(vblitter_res_t)) {
	return -EFAULT;
    }

    return res->res;
}


    static int
_vblitter_blit (vblit_blit_req_list_t* req_list)
{
    int     res;

    res = __vblitter_blit(req_list);

    return res;
}


    static int
vblitter_open (struct inode* inode, struct file* file)
{
    return 0;
}


    static int
vblitter_release (struct inode* inode, struct file* file)
{
    return 0;
}


    static long
vblitter_ioctl (struct file* file, unsigned int cmd, unsigned long arg)
{
    int err = -EFAULT;

    DTRACE("vblitter_ioctl() entered, cmd 0x%x\n", cmd);

    if (mutex_lock_interruptible(&_vblitter.mutex)) {
	return -EINTR;
    }

    switch (cmd) {
    case VBLITTER_IOCTL_BLIT:
    {
        typedef struct {
            nku32_f count;
            vblit_blit_req_t req[1];
        } list_t;
        vblit_blit_req_list_t req_list;
        int offset;
        int i;

        if (copy_from_user(&req_list, (const void*)arg, sizeof(list_t))) {
	    goto OUT;
	}

        if (req_list.count > 1) {
            for (i = 1, offset = sizeof(list_t); i < req_list.count; i++, offset += sizeof(vblit_blit_req_t)) {
                if (copy_from_user(&req_list.req[i], (const void*)(arg+offset), sizeof(vblit_blit_req_t))) {
		    goto OUT;
		}
            }
        }

#ifdef VBLITTER_DEBUG
        for (i = 0; i < req_list.count; i++) {
            DTRACE("%d: src={w=%d, h=%d, f=%d, o=%d, m=%d, rect={%d,%d,%d,%d}}\n",
                   i,
                   req_list.req[i].src.width,
                   req_list.req[i].src.height,
                   req_list.req[i].src.format,
                   req_list.req[i].src.offset,
                   req_list.req[i].src.memory_id,
                   req_list.req[i].src_rect.x,
                   req_list.req[i].src_rect.y,
                   req_list.req[i].src_rect.w,
                   req_list.req[i].src_rect.h);
            DTRACE("    dst={w=%d, h=%d, f=%d, o=%d, m=%d, rect={%d,%d,%d,%d}}\n",
                   req_list.req[i].dst.width,
                   req_list.req[i].dst.height,
                   req_list.req[i].dst.format,
                   req_list.req[i].dst.offset,
                   req_list.req[i].dst.memory_id,
                   req_list.req[i].dst_rect.x,
                   req_list.req[i].dst_rect.y,
                   req_list.req[i].dst_rect.w,
                   req_list.req[i].dst_rect.h);
            DTRACE("    flags=%08x\n",
                   req_list.req[i].flags);
        }
#endif

        err = _vblitter_blit(&req_list);
	break;
    }

    default:
        break;
    }

OUT:
    mutex_unlock(&_vblitter.mutex);

    DTRACE("vblitter_ioctl() returning -> err %d\n", err);

    return err;
}


    static void
_vblitter_ready (void* cookie)
{
}


    static int
vblitter_init (void)
{
    int result;
    struct vrpc_t* vrpc;

    memset(&_vblitter, 0x00, sizeof(_vblitter));
    mutex_init(&_vblitter.mutex);

    vrpc = vrpc_client_lookup(VBLITTER_VRPC_NAME, 0);
    if (vrpc == NULL) {
        return -EINVAL;
    }

    _vblitter.vrpc  = vrpc;
    _vblitter.msize = vrpc_maxsize(vrpc);
    _vblitter.data  = vrpc_data(vrpc);

    if (_vblitter.msize < sizeof(vblitter_req_t)) {
        vrpc_release(vrpc);
	return -EINVAL;
    }
    if (_vblitter.msize < sizeof(vblitter_res_t)) {
        vrpc_release(vrpc);
	return -EINVAL;
    }


    result = vrpc_client_open(vrpc, _vblitter_ready, &_vblitter);
    if (result)
        vrpc_release(vrpc);

    return result;
}


    static void
vblitter_exit (void)
{
    vrpc_close(_vblitter.vrpc);

    vrpc_release(_vblitter.vrpc);
}


static struct file_operations _vblitter_fops = {
    .owner   = THIS_MODULE,
    .llseek  = NULL,
    .read    = NULL,
    .write   = NULL,
    .readdir = NULL,
    .poll    = NULL,
    .unlocked_ioctl = vblitter_ioctl,
    .mmap    = NULL,
    .open    = vblitter_open,
    .flush   = NULL,
    .release = vblitter_release,
    .fsync   = NULL,
    .fasync  = NULL,
    .lock    = NULL
};


static struct class* _vblitter_class = NULL;


    static int __init
_vblitter_init (void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    struct device* cls_dev;
#else
    struct class_device* cls_dev;
#endif
    int            err;

    _vblitter_class = class_create(THIS_MODULE, VBLITTER_NAME);
    if (IS_ERR(_vblitter_class)) {
	return -EIO;
    }

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    cls_dev = device_create
#else
    cls_dev = class_device_create
#endif
	(_vblitter_class, NULL, MKDEV(VBLITTER_MAJOR, VBLITTER_MINOR),
	 NULL, VBLITTER_NAME);
    if (IS_ERR(cls_dev)) {
	class_destroy(_vblitter_class);
	return -EIO;
    }

    err = register_chrdev(VBLITTER_MAJOR, VBLITTER_NAME, &_vblitter_fops);
    if (err != 0) {
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
	device_destroy
#else
	class_device_destroy
#endif
	    (_vblitter_class, MKDEV(VBLITTER_MAJOR, VBLITTER_MINOR));
	class_destroy(_vblitter_class);
	return -EIO;
    }

    err = vblitter_init();
    if (err != 0) {
	return -EIO;
    }

    TRACE("VLX virtual blitter initialized\n");

    return 0;
}


    static void __exit
_vblitter_exit (void)
{
    unregister_chrdev(VBLITTER_MAJOR, VBLITTER_NAME);
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    device_destroy
#else
    class_device_destroy
#endif
	(_vblitter_class, MKDEV(VBLITTER_MAJOR, VBLITTER_MINOR));
    class_destroy(_vblitter_class);

    vblitter_exit();
}


MODULE_DESCRIPTION("VLX virtual blitter frontend driver");
MODULE_AUTHOR("Vladimir Grouzdev <vladimir.grouzdev@redbend.com>");
MODULE_AUTHOR("Christian Jacquemot <Christian.Jacquemot@redbend.com>");
MODULE_LICENSE("GPL");

module_init(_vblitter_init);
module_exit(_vblitter_exit);

/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Display (VDISP).                                  *
 *             VDISP frontend kernel driver implementation.                  *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  This program is free software: you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License Version 2           *
 *  as published by the Free Software Foundation.                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  Version 2 along with this program.                                       *
 *  If not, see <http://www.gnu.org/licenses/>.                              *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *    Chi Dat Truong    <chidat.truong@redbend.com>                          *
 *                                                                           *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/poll.h>

#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <mach/hardware.h>

#include "vdisplay.h"

MODULE_DESCRIPTION("VDISP Front-End Driver");
MODULE_AUTHOR("Chi Dat Truong <chidat.truong@redbend.com>");
MODULE_LICENSE("GPL");

static VdispDrv vdisp_clt_drv;

    /*
     *
     */
    static void*
vdisp_clt_rpc_req_alloc (VdispCltSession* session, VdispCmd cmd)
{
    VdispSize       size;
    VdispReqHeader* hdr;

    if (cmd > VDISP_CMD_NR) {
        size = sizeof(VdispDrvReq);
    } else {
        size = sizeof(VdispReq);
    }

    hdr = vdisp_rpc_req_alloc(&session->dev->rpc, size);
    if (IS_ERR(hdr)) {
        return hdr;
    }

    hdr->clId.vmId  = session->dev->gen_dev.vlink->id;
//    hdr->clId.appId = session->app_id;
    hdr->cmd        = cmd;

    return hdr;
}

    /*
     *
     */
    static inline void*
vdisp_clt_rpc_call (VdispCltSession* session, VdispSize* rsize)
{
    void* resp;
    resp = vdisp_rpc_call(&session->dev->rpc, session->vls, rsize);
    return resp;
}

    /*
     *
     */
     static void
vdisp_session_init (VdispCltSession* session, VdispCltDev* dev)
{
    memset(session, 0, sizeof(VdispCltSession));
    session->dev = dev;
}

    /*
     *
     */
     static int
vdisp_session_create (VdispCltFile* vdisp_file)
{
    VdispCltDev*     dev     = vdisp_file->dev;
    Vlink*           vlink   = dev->gen_dev.vlink;
    VdispCltSession* session = &vdisp_file->session;
    int              diag;

    vdisp_session_init(session, dev);

    diag = vlink_session_create(vlink,
                                NULL,
                                NULL,
                                &session->vls);

    if (!diag) {
        vlink_session_leave(session->vls);
    }

    return diag;
}

    /*
     *
     */
     static int
vdisp_session_destroy (VdispCltSession* session)
{
    vlink_session_destroy(session->vls);
    return 0;
}

    /*
     *
     */
    static VdispCltDev*
vdisp_clt_dev_find (unsigned int minor)
{
    VdispGenDrv* gen_drv = &vdisp_clt_drv.gen;
    VdispDev*    dev;

    if (minor >= gen_drv->vlink_drv->nr_units) {
        return NULL;
    }

    dev = &gen_drv->devs[minor];
    if (!dev->gen.enabled) {
        return NULL;
    }

    return &dev->clt;
}

    /*
     *
     */
    static int
vdisp_clt_open (struct inode* inode, struct file* file)
{
    unsigned int  minor = iminor(file->f_path.dentry->d_inode);
    VdispCltDev*  dev;
    VdispCltFile* vdisp_file;
    int           diag;

    dev = vdisp_clt_dev_find(minor);
    if (!dev) {
        return -ENXIO;
    }

    vdisp_file = (VdispCltFile*) kzalloc(sizeof(VdispCltFile), GFP_KERNEL);
    if (!vdisp_file) {
        return -ENOMEM;
    }

    vdisp_file->dev    = dev;
    file->private_data = vdisp_file;

    diag = vdisp_session_create(vdisp_file);
    if (diag) {
        kfree(vdisp_file);
    }

    return diag;
}

    /*
     *
     */
    static int
vdisp_clt_release (struct inode* inode, struct file* file)
{
    VdispCltFile* vdisp_file = file->private_data;
    int           diag;

    diag = vdisp_session_destroy(&vdisp_file->session);

    kfree(vdisp_file);

    return diag;
}

    /*
     *
     */
static int vdisp_bad_ioctl (struct VdispCltFile* vdisp_file, void* arg,
                            unsigned int sz)
{
    VDISP_ERROR("invalid ioctl command\n");
    return -EINVAL;
}

    /*
     *
     */
     static int
vdisp_rpc_set_brightness (VdispCltSession* session, nku32_f brightness)
{
    VdispCltDev*     dev     = session->dev;
    VdispReq*        req;
    VdispResp*       resp;
    int              diag;

    if (vlink_session_enter_and_test_alive(session->vls)) {
        diag = -ESESSIONBROKEN;
        goto out_session_leave;
    }

    req = vdisp_clt_rpc_req_alloc(session, VDISP_CMD_SET_BRIGHTNESS);
    if (IS_ERR(req)) {
        diag = PTR_ERR(req);
        goto out_session_leave;
    }

    req->screenBrightness = brightness;

    resp = vdisp_clt_rpc_call(session, NULL);
    if (IS_ERR(resp)) {
        diag = PTR_ERR(resp);
        goto out_rpc_end;
    }

    diag = 0;

out_rpc_end:
    vdisp_rpc_call_end(&dev->rpc);

out_session_leave:
    vlink_session_leave(session->vls);
    return diag;
}

    /*
     *
     */
static int vdisp_set_brightness_ioctl (struct VdispCltFile* vdisp_file, void* arg,
                            unsigned int sz)
{
    nku32_f   brightness;
    int       size = sizeof(nku32_f);
    int       diag;

 
    if (sz < size) {
        return -EINVAL;
    }

    if (copy_from_user(&brightness, arg, size)) {
        return -EFAULT;
    }

    if (brightness > 255) {
        brightness = 255;
    }

    diag = vdisp_rpc_set_brightness(&vdisp_file->session, brightness);

    return diag;
}

    /*
     *
     */
     static int
vdisp_rpc_get_brightness (VdispCltSession* session, nku32_f* brightness)
{
    VdispCltDev*     dev     = session->dev;
    VdispReq*        req;
    VdispResp*       resp;
    int              diag;

    if (vlink_session_enter_and_test_alive(session->vls)) {
        diag = -ESESSIONBROKEN;
        goto out_session_leave;
    }

    req = vdisp_clt_rpc_req_alloc(session, VDISP_CMD_GET_BRIGHTNESS);
    if (IS_ERR(req)) {
        diag = PTR_ERR(req);
        goto out_session_leave;
    }

    resp = vdisp_clt_rpc_call(session, NULL);
    if (IS_ERR(resp)) {
        diag = PTR_ERR(resp);
        goto out_rpc_end;
    }

    *brightness = resp->screenBrightness;
    diag = 0;


out_rpc_end:
    vdisp_rpc_call_end(&dev->rpc);

out_session_leave:
    vlink_session_leave(session->vls);
    return diag;
}

    /*
     *
     */
static int vdisp_get_brightness_ioctl (struct VdispCltFile* vdisp_file, void* arg,
                            unsigned int sz)
{
    nku32_f   brightness;
    int       size = sizeof(nku32_f);
    int       diag;

 
    if (sz < size) {
        return -EINVAL;
    }

    diag = vdisp_rpc_get_brightness(&vdisp_file->session, &brightness);

    if (brightness > 255) {
        brightness = 255;
    }

    if (copy_to_user(arg, &brightness, size)) {
        return -EFAULT;
    }

    return diag;
}

    /*
     *
     */
     static int
vdisp_rpc_get_power_status (VdispCltSession* session, nku32_f* state)
{
    VdispCltDev*     dev     = session->dev;
    VdispReq*        req;
    VdispResp*       resp;
    int              diag;

    if (vlink_session_enter_and_test_alive(session->vls)) {
        diag = -ESESSIONBROKEN;
        goto out_session_leave;
    }

    req = vdisp_clt_rpc_req_alloc(session, VDISP_CMD_GET_POWER);
    if (IS_ERR(req)) {
        diag = PTR_ERR(req);
        goto out_session_leave;
    }

    resp = vdisp_clt_rpc_call(session, NULL);
    if (IS_ERR(resp)) {
        diag = PTR_ERR(resp);
        goto out_rpc_end;
    }

    *state = resp->screenPowerState;
    diag = 0;


out_rpc_end:
    vdisp_rpc_call_end(&dev->rpc);

out_session_leave:
    vlink_session_leave(session->vls);
    return diag;
}

    /*
     *
     */
static int vdisp_get_power_status_ioctl (struct VdispCltFile* vdisp_file, void* arg,
                            unsigned int sz)
{
    nku32_f   state;
    int       size = sizeof(nku32_f);
    int       diag;

 
    if (sz < size) {
        return -EINVAL;
    }

    diag = vdisp_rpc_get_power_status(&vdisp_file->session, &state);

    if (state != PANEL_POWER_OFF) {
        state = PANEL_POWER_ON;
    }

    if (copy_to_user(arg, &state, size)) {
        return -EFAULT;
    }

    return diag;
}

    /*
     *
     */
     static int
vdisp_rpc_set_power_state (VdispCltSession* session, nku32_f state)
{
    VdispCltDev*     dev     = session->dev;
    VdispDrvReq*     req;
    VdispDrvResp*    resp;
    int              diag;

    if (vlink_session_enter_and_test_alive(session->vls)) {
        diag = -ESESSIONBROKEN;
        goto out_session_leave;
    }

    req = vdisp_clt_rpc_req_alloc(session, VDISP_CMD_SET_POWER);
    if (IS_ERR(req)) {
        diag = PTR_ERR(req);
        goto out_session_leave;
    }

    req->screenPowerState = state;

    resp = vdisp_clt_rpc_call(session, NULL);
    if (IS_ERR(resp)) {
        diag = PTR_ERR(resp);
        goto out_rpc_end;
    }

    diag = 0;

out_rpc_end:
    vdisp_rpc_call_end(&dev->rpc);

out_session_leave:
    vlink_session_leave(session->vls);
    return diag;
}

/* Android early suspend */
#ifdef CONFIG_HAS_EARLYSUSPEND
    static void
vdisplay_fb_early_suspend(struct early_suspend* es)
{
    unsigned int  minor = 0; // Limitation: support only 1 frontend
    VdispCltDev*  dev;
    VdispCltFile  vdisp_file;
    int           diag;

    // create session
    dev = vdisp_clt_dev_find(minor);
    if (!dev) {
        return;
    }

    vdisp_file.dev    = dev;

    diag = vdisp_session_create(&vdisp_file);
    if (diag) {
        return;
    }

    diag = vdisp_rpc_set_power_state(&vdisp_file.session, PANEL_POWER_OFF);
    if (diag) {
        VDISP_ERROR("Cannot set power state");;
    }

    // close session
    diag = vdisp_session_destroy(&vdisp_file.session);
    if (diag) {
        VDISP_ERROR("Cannot destroy session");;
    }

    return;
}

    static void
vdisplay_fb_late_resume(struct early_suspend* es)
{
    unsigned int  minor = 0; // Limitation: support only 1 frontend
    VdispCltDev*  dev;
    VdispCltFile  vdisp_file;
    int           diag;

    // create session
    dev = vdisp_clt_dev_find(minor);
    if (!dev) {
        return;
    }

    vdisp_file.dev    = dev;

    diag = vdisp_session_create(&vdisp_file);
    if (diag) {
        return;
    }

    diag = vdisp_rpc_set_power_state(&vdisp_file.session, PANEL_POWER_ON);
    if (diag) {
        VDISP_ERROR("Cannot set power state");;
    }

    // close session
    diag = vdisp_session_destroy(&vdisp_file.session);
    if (diag) {
        VDISP_ERROR("Cannot destroy session");;
    }

    return;
}
#endif

    /*
     *
     */
     static int
vdisp_rpc_get_property (VdispCltSession* session)
{
    VdispCltDev*     dev     = session->dev;
    VdispDrvReq*     drv_req;
    VdispDrvResp*    drv_resp;
    int              diag;

    if (vlink_session_enter_and_test_alive(session->vls)) {
        diag = -ESESSIONBROKEN;
        goto out_session_leave;
    }

    drv_req = vdisp_clt_rpc_req_alloc(session, VDISP_CMD_GET_PROPERTY);
    if (IS_ERR(drv_req)) {
        diag = PTR_ERR(drv_req);
        goto out_session_leave;
    }

    drv_resp = vdisp_clt_rpc_call(session, NULL);
    if (IS_ERR(drv_resp)) {
        diag = PTR_ERR(drv_resp);
        goto out_rpc_end;
    }

    memcpy(&dev->property, &drv_resp->displayProperty, sizeof(DisplayProperty));
    diag = 0;


out_rpc_end:
    vdisp_rpc_call_end(&dev->rpc);

out_session_leave:
    vlink_session_leave(session->vls);
    return diag;
}

    /*
     *
     */
static int vdisp_get_display_info_ioctl (struct VdispCltFile* vdisp_file, void* arg,
                            unsigned int sz)
{
    VdispCltDev* dev = vdisp_file->session.dev;
    int          size = sizeof(DisplayProperty);
    int          diag;
 
    if (sz < size) {
        return -EINVAL;
    }

    if (dev->property.xres == 0 && dev->property.yres == 0) {
        // Not initialized. Get display info from BE (Blocking call)
        diag = vdisp_rpc_get_property(&vdisp_file->session);
        if (diag) {
             VDISP_ERROR("Cannot get display property");
             return -EFAULT;
        }
    }

    if (copy_to_user(arg, &dev->property, size)) {
        return -EFAULT;
    }
    return diag;
}

    /*
     *
     */
static VdispCltIoctl vdisp_clt_ioctl_ops[VDISP_IOCTL_CMD_MAX] = {
    (VdispCltIoctl)vdisp_set_brightness_ioctl,     /*  0 - set brightness       */
    (VdispCltIoctl)vdisp_get_brightness_ioctl,     /*  1 - get brightness       */
    (VdispCltIoctl)vdisp_get_display_info_ioctl,   /*  2 - get display info     */
    (VdispCltIoctl)vdisp_get_power_status_ioctl,   /*  3 - get power status     */
    (VdispCltIoctl)vdisp_bad_ioctl,                /*  4 - Unused               */
    (VdispCltIoctl)vdisp_bad_ioctl,                /*  5 - Unused               */
    (VdispCltIoctl)vdisp_bad_ioctl,                /*  6 - set power state (DRV)*/
    (VdispCltIoctl)vdisp_bad_ioctl,                /*  7 - Unused               */
    (VdispCltIoctl)vdisp_bad_ioctl,                /*  8 - Unused               */
    (VdispCltIoctl)vdisp_bad_ioctl,                /*  9 - Unused               */
    (VdispCltIoctl)vdisp_bad_ioctl,                /* 10 - Unused               */
    (VdispCltIoctl)vdisp_bad_ioctl,                /* 11 - Unused               */
    (VdispCltIoctl)vdisp_bad_ioctl,                /* 12 - Unused               */
    (VdispCltIoctl)vdisp_bad_ioctl,                /* 13 - Unused               */
    (VdispCltIoctl)vdisp_bad_ioctl,                /* 14 - Unused               */
    (VdispCltIoctl)vdisp_bad_ioctl,                /* 15 - Unused               */
};

    /*
     *
     */
    static int
vdisp_clt_ioctl (struct inode* inode, struct file* file,
                 unsigned int cmd, unsigned long arg)
{
    VdispCltFile* vdisp_file = file->private_data;
    unsigned int  nr;
    unsigned int  sz;

    nr = _IOC_NR(cmd) & (VDISP_IOCTL_CMD_MAX - 1);
    sz = _IOC_SIZE(cmd);

    return vdisp_clt_ioctl_ops[nr](vdisp_file, (void*)arg, sz);
}

    /*
     *
     */
    static ssize_t
vdisp_clt_read (struct file* file, char __user* ubuf, size_t cnt, loff_t* ppos)
{
    return -EINVAL;
}

    /*
     *
     */
    static ssize_t
vdisp_clt_write (struct file* file, const char __user* ubuf,
                 size_t count, loff_t* ppos)
{
    return -EINVAL;
}

    /*
     *
     */
    static unsigned int
vdisp_clt_poll(struct file* file, poll_table* wait)
{
    return -ENOSYS;
}

    /*
     *
     */
static struct file_operations vdisp_clt_fops = {
    .owner      = THIS_MODULE,
    .open       = vdisp_clt_open,
    .read       = vdisp_clt_read,
    .write      = vdisp_clt_write,
    .ioctl      = vdisp_clt_ioctl,
    .release    = vdisp_clt_release,
    .poll       = vdisp_clt_poll,
};


    /*
     *
     */
    static int
vdisp_clt_vlink_abort (Vlink* vlink, void* cookie)
{
    VdispCltDev* dev = (VdispCltDev*) cookie;

    VLINK_DTRACE(vlink, "vdisp_clt_vlink_abort called\n");

    vdisp_rpc_wakeup(&dev->rpc);

    return 0;
}

    /*
     *
     */
    static int
vdisp_clt_vlink_reset (Vlink* vlink, void* cookie)
{
    VdispCltDev* dev = (VdispCltDev*) cookie;

    VLINK_DTRACE(vlink, "vdisp_clt_vlink_reset called\n");

    vdisp_rpc_reset(&dev->rpc);

    return 0;
}

    /*
     *
     */
    static int
vdisp_clt_vlink_start (Vlink* vlink, void* cookie)
{
    VdispCltDev* dev = (VdispCltDev*) cookie;

    VLINK_DTRACE(vlink, "vdisp_clt_vlink_start called\n");

    nkops.nk_xirq_unmask(dev->gen_dev.cxirqs);

    return 0;
}

    /*
     *
     */
    static int
vdisp_clt_vlink_stop (Vlink* vlink, void* cookie)
{
    VdispCltDev* dev = (VdispCltDev*) cookie;

    VLINK_DTRACE(vlink, "vdisp_clt_vlink_stop called\n");

    nkops.nk_xirq_mask(dev->gen_dev.cxirqs);

    return 0;
}

    /*
     *
     */
    static int
vdisp_clt_vlink_cleanup (Vlink* vlink, void* cookie)
{
    VdispCltDev* dev = (VdispCltDev*) cookie;

    VLINK_DTRACE(vlink, "vdisp_clt_vlink_cleanup called\n");

    dev->gen_dev.enabled = 0;

    vdisp_gen_dev_cleanup(&dev->gen_dev);

    return 0;
}

    /*
     *
     */
static VlinkOpDesc vdisp_clt_vlink_ops[] = {
    { VLINK_OP_RESET,   vdisp_clt_vlink_reset   },
    { VLINK_OP_START,   vdisp_clt_vlink_start   },
    { VLINK_OP_ABORT,   vdisp_clt_vlink_abort   },
    { VLINK_OP_STOP,    vdisp_clt_vlink_stop    },
    { VLINK_OP_CLEANUP, vdisp_clt_vlink_cleanup },
    { 0,                NULL                    },
};

    /*
     *
     */
    static int
vdisp_clt_prop_get (Vlink* vlink, unsigned int type, void* prop)
{
    switch (type) {
    case VDISP_PROP_PDEV_SIZE:
    {
        NkPhSize* sz = (NkPhSize*) prop;
        *sz = 16 * 1024;
        break;
    }
    default:
        return -EINVAL;
    }
    return 0;
}

    /*
     *
     */
static VdispDrv vdisp_clt_drv = {
    .clt = {
        .gen_drv = {
             .name         = "vdisplay",
             .resc_id_base = 0,
             .prop_get     = vdisp_clt_prop_get,
         },
        .major             = VDISP_CLT_MAJOR,
    },
};

    /*
     *
     */
    static int
vdisp_clt_vlink_init (Vlink* vlink)
{
    VdispCltDrv* drv = &vdisp_clt_drv.clt;
    VdispCltDev* dev = &drv->gen_drv.devs[vlink->unit].clt;
    int          diag;

    VLINK_DTRACE(vlink, "vdisp_clt_vlink_init called\n");

    dev->gen_dev.drv   = &vdisp_clt_drv;
    dev->gen_dev.vlink = vlink;

    if ((diag = vdisp_gen_dev_init(&dev->gen_dev)) != 0) {
        vdisp_clt_vlink_cleanup(vlink, dev);
        return diag;
    }

    dev->class_dev = device_create(drv->class,
                                   NULL,
                                   MKDEV(drv->major, vlink->unit),
                                   NULL,
                                   "%s%d",
                                   drv->gen_drv.name,
                                   vlink->unit);
    if (IS_ERR(dev->class_dev)) {
        VLINK_ERROR(vlink, "cannot create the '%s%d' device\n",
                    drv->gen_drv.name, vlink->unit);
        diag = PTR_ERR(dev->class_dev);
        dev->class_dev = NULL;
        vdisp_clt_vlink_cleanup(vlink, dev);
        return diag;
    }

    diag = vdisp_rpc_setup(&dev->gen_dev, &dev->rpc, VDISP_RPC_TYPE_CLIENT);
    if (diag) {
        vdisp_clt_vlink_cleanup(vlink, dev);
        return diag;
    }

    dev->gen_dev.enabled = 1;

    return 0;
}

    /*
     *
     */
    static void
vdisp_clt_vlink_drv_cleanup (VlinkDrv* vlink_drv)
{
    VdispCltDrv* drv = &vdisp_clt_drv.clt;

    vdisp_gen_drv_cleanup(&drv->gen_drv);

#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&drv->early_suspend);
#endif
}

    /*
     *
     */
    static int
vdisp_clt_vlink_drv_init (VlinkDrv* vlink_drv)
{
    VdispCltDrv* drv = &vdisp_clt_drv.clt;
    int          diag;

    drv->gen_drv.vlink_drv = vlink_drv;
    drv->gen_drv.vops      = vdisp_clt_vlink_ops;
    drv->gen_drv.devs      = NULL;

    drv->fops              = &vdisp_clt_fops;
    drv->chrdev_major      = 0;
    drv->class             = NULL;

    if ((diag = vdisp_gen_drv_init(&drv->gen_drv)) != 0) {
        vdisp_clt_vlink_drv_cleanup(vlink_drv);
        return diag;
    }

    diag = register_chrdev(drv->major, drv->gen_drv.name, drv->fops);
    if ((drv->major && diag) || (!drv->major && diag <= 0)) {
        VDISP_ERROR("%s: cannot register major number '%d'\n",
                    drv->gen_drv.name, drv->major);
        vdisp_clt_vlink_drv_cleanup(vlink_drv);
        return diag;
    }
    drv->chrdev_major = drv->major ? drv->major : diag;

    drv->class = class_create(THIS_MODULE, drv->gen_drv.name);
    if (IS_ERR(drv->class)) {
        VDISP_ERROR("%s: cannot create the device class\n", drv->gen_drv.name);
        diag = PTR_ERR(drv->class);
        drv->class = NULL;
        vdisp_clt_vlink_drv_cleanup(vlink_drv);
        return diag;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
	drv->early_suspend.suspend = vdisplay_fb_early_suspend;
	drv->early_suspend.resume = vdisplay_fb_late_resume;
        drv->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
        register_early_suspend(&drv->early_suspend);
#endif

    return 0;
}

    /*
     *
     */
static VlinkDrv vdisp_clt_vlink_drv = {
    .name       = "vdisplay",
    .init       = vdisp_clt_vlink_drv_init,
    .cleanup    = vdisp_clt_vlink_drv_cleanup,
    .vlink_init = vdisp_clt_vlink_init,
    .flags      = VLINK_DRV_TYPE_CLIENT,
};

    /*
     *
     */
    static int
vdisp_fe_module_init (void)
{
    int diag;

    if ((diag = vlink_drv_probe(&vdisp_clt_vlink_drv)) != 0) {
        return diag;
    }
    vlink_drv_startup(&vdisp_clt_vlink_drv);

    return 0;
}

    /*
     *
     */
    static void
vdisp_fe_module_exit (void)
{
    vlink_drv_shutdown(&vdisp_clt_vlink_drv);
    vlink_drv_cleanup(&vdisp_clt_vlink_drv);
}

module_init(vdisp_fe_module_init);
module_exit(vdisp_fe_module_exit);

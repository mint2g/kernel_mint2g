/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Display (VDISP).                                  *
 *             VDISP backend kernel driver implementation.                   *
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
#include "vdisplay.h"

MODULE_DESCRIPTION("VDISP Back-End Driver");
MODULE_AUTHOR("Chi Dat Truong <chidat.truong@redbend.com>");
MODULE_LICENSE("GPL");

static VdispControlDrv vdisp_control_drv;

/****************************************************************************/
/*
 * RPC subcomponent attached to vDisplay vLink.
 *  - vRPC communication between User_BE <--> FE
 */

static VdispDrv vdisp_srv_drv;
static void _propagate_event (nku32_f type, int value);

    /*
     *
     */
     static void
vdisp_session_init (VdispSrvSession* session, VdispSrvDev* dev)
{
    memset(session, 0, sizeof(VdispSrvSession));
    session->dev = dev;
}

    /*
     *
     */
     static int
vdisp_session_create (VdispSrvFile* vdisp_file)
{
    VdispSrvDev*     dev     = vdisp_file->dev;
    Vlink*           vlink   = dev->gen_dev.vlink;
    VdispSrvSession* session = &vdisp_file->session;
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
vdisp_session_destroy (VdispSrvSession* session)
{
    VdispSrvDev* dev  = session->dev;
    int          diag = 0;

    if (!session->cmd) {
        goto out_session_destroy;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
        diag = -ESESSIONBROKEN;
        goto out_session_leave;
    }

    vdisp_rpc_respond(&dev->rpc, -ESESSIONBROKEN);

out_session_leave:
    vlink_session_leave(session->vls);
out_session_destroy:
    vlink_session_destroy(session->vls);
    return diag;
}

    /*
     *
     */
    static VdispSrvDev*
vdisp_srv_dev_find (unsigned int minor)
{
    VdispGenDrv* gen_drv = &vdisp_srv_drv.gen;
    VdispDev*    dev;

    if (minor >= gen_drv->vlink_drv->nr_units) {
        return NULL;
    }

    dev = &gen_drv->devs[minor];
    if (!dev->gen.enabled) {
        return NULL;
    }

    return &dev->srv;
}

    /*
     *
     */
    static int
vdisp_srv_open (struct inode* inode, struct file* file)
{
    unsigned int  minor = iminor(file->f_path.dentry->d_inode);
    VdispSrvDev*  dev;
    VdispSrvFile* vdisp_file;
    int           diag;

    dev = vdisp_srv_dev_find(minor);
    if (!dev) {
        return -ENXIO;
    }

    vdisp_file = (VdispSrvFile*) kzalloc(sizeof(VdispSrvFile), GFP_KERNEL);
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
vdisp_srv_release (struct inode* inode, struct file* file)
{
    VdispSrvFile* vdisp_file = file->private_data;
    int           diag;

    diag = vdisp_session_destroy(&vdisp_file->session);

    kfree(vdisp_file);

    return diag;
}

    /*
     *
     */
    static int
vdisp_bad_ioctl (struct VdispSrvFile* vdisp_file, void* arg,
                            unsigned int sz)
{
    VLINK_ERROR(vdisp_file->dev->gen_dev.vlink, "invalid ioctl command\n");
    return -EINVAL;
}

    /*
     *
     */
    static int
vdisp_triggger_power_event (nku32_f state, int vm_id)
{
    if (state == PANEL_POWER_ON)
        _propagate_event(VDISP_EVENT_TYPE_PANEL_ON, vm_id);
    else
        _propagate_event(VDISP_EVENT_TYPE_PANEL_OFF, vm_id);
    return 0;
}

    /*
     *
     */
    static int
vdisp_get_display_property (VdispSrvSession* session)
{
    DisplayProperty* hw_property = &vdisp_control_drv.hw_property;
    int              diag;

    diag = wait_event_interruptible(vdisp_control_drv.hw_property_wait,
                        (hw_property->xres != 0 && hw_property->yres != 0) ||
                        VLINK_SESSION_IS_ABORTED(session->vls));
    if (diag) {
        return diag;
    }

    if (VLINK_SESSION_IS_ABORTED(session->vls))
        return -ESESSIONBROKEN;

    return 0;
}

    /*
     *
     */
static int vdisp_receive_ioctl (struct VdispSrvFile* vdisp_file, void* arg,
                            unsigned int sz)
{
    VdispSrvSession* session = &vdisp_file->session;
    VdispSrvDev*     dev     = session->dev;
    VdispReqHeader*  hdr;
    VdispDrvReq*     drv_req;
    VdispDrvResp*    drv_resp;
    VdispReq*        req;
    VdispResp*       resp;
    VdispSize        size;
    int              diag;

    if (session->cmd) {
        return -EINVAL;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
        diag = -ESESSIONBROKEN;
        goto out_session_leave;
    }

receive_again:
    hdr = vdisp_rpc_receive(&dev->rpc, session->vls, &size);
    if (IS_ERR(hdr)) {
        diag = PTR_ERR(hdr);
        goto out_session_leave;
    }

    /* Intercept calls */
    switch (hdr->cmd) {
    case VDISP_CMD_SET_BRIGHTNESS:
    {
        /* Call for User, but we keep brightness state in DRV */
        req = (VdispReq*) hdr;
        dev->backlight_state = req->screenBrightness;
        break;
    }
    case VDISP_CMD_SET_POWER:
      {
        /* call for DRV */
        drv_req = (VdispDrvReq*) hdr;
        dev->panel_state = drv_req->screenPowerState;
        diag = vdisp_triggger_power_event(drv_req->screenPowerState,
                                          hdr->clId.vmId);
        if (!diag) {
            drv_resp = (VdispDrvResp*)
                       vdisp_rpc_resp_alloc(&dev->rpc, sizeof(VdispDrvResp));
            if (IS_ERR(drv_resp)) {
                diag = PTR_ERR(drv_resp);
            }
        }
        vdisp_rpc_respond(&dev->rpc, diag);
        goto receive_again;
      }
    case VDISP_CMD_GET_POWER:
      {
        /*
         * call for User, but replied from DRV since
         * the user daemon doesn't support it yet
         */
        diag = 0;
        resp = (VdispResp*)
               vdisp_rpc_resp_alloc(&dev->rpc, sizeof(VdispResp));
        if (IS_ERR(resp)) {
            diag = PTR_ERR(resp);
        } else {
            resp->screenPowerState = dev->panel_state;
        }
        vdisp_rpc_respond(&dev->rpc, diag);
        goto receive_again;
      }
    case VDISP_CMD_GET_PROPERTY:
      {
        /* call for DRV */
        /* get display properties (blocking call) */
        diag = vdisp_get_display_property(session);
        if (!diag) {
            drv_resp = (VdispDrvResp*)
                       vdisp_rpc_resp_alloc(&dev->rpc, sizeof(VdispDrvResp));
            if (IS_ERR(drv_resp)) {
                diag = PTR_ERR(drv_resp);
            } else {
                memcpy(&drv_resp->displayProperty,
                       &vdisp_control_drv.hw_property,
                       sizeof(DisplayProperty));
            }
        }
        vdisp_rpc_respond(&dev->rpc, diag);
        goto receive_again;
      }
    default:;
    }

    if (copy_to_user(arg, hdr, size)) {
        diag = -EFAULT;
        vdisp_rpc_respond(&dev->rpc, diag);
        goto out_session_leave;
    }

    diag = 0;
    session->cmd = hdr->cmd;
    goto out_session_leave;


out_session_leave:
    vlink_session_leave(session->vls);
    return diag;
}

    /*
     *
     */
static int vdisp_respond_ioctl (struct VdispSrvFile* vdisp_file, void* arg,
                            unsigned int sz)
{
    VdispSrvSession*   session = &vdisp_file->session;
    VdispSrvDev*       dev     = session->dev;
    VdispResp          kresp;
    VdispResp*         resp;
    VdispSize          size;
    int                diag;
    int                retval;

    if (!session->cmd) {
        return -EINVAL;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
        diag = -ESESSIONBROKEN;
        goto out_session_leave;
    }

    if (copy_from_user(&kresp, arg, sizeof(VdispResp))) {
        retval = diag = -EFAULT;
        goto out_resp_err;
    }

    diag   = 0;
    retval = -kresp.retVal;
    if (retval != 0) {
        if (!IS_ERR(ERR_PTR(retval))) {
            retval = diag = -EINVAL;
        }
        goto out_resp_err;
    }

    size = sizeof(VdispResp);
    resp = vdisp_rpc_resp_alloc(&dev->rpc, size);
    if (IS_ERR(resp)) {
        retval = diag = PTR_ERR(resp);
        goto out_resp_err;
    }

    memcpy(resp, &kresp, sizeof(VdispResp));
    goto out_resp;

out_resp_err:

out_resp:
    session->cmd = 0;
    vdisp_rpc_respond(&dev->rpc, retval);
out_session_leave:
    vlink_session_leave(session->vls);
    return diag;
}

    /*
     *
     */
static VdispSrvIoctl vdisp_srv_ioctl_ops[VDISP_IOCTL_CMD_MAX] = {
    (VdispSrvIoctl)vdisp_bad_ioctl,                /*  0 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /*  1 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /*  2 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /*  3 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /*  4 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /*  5 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /*  6 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /*  7 - Unused               */
    (VdispSrvIoctl)vdisp_receive_ioctl,            /*  8 - RECEIVE              */
    (VdispSrvIoctl)vdisp_respond_ioctl,            /*  9 - RESPOND              */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /* 10 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /* 11 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /* 12 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /* 13 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /* 14 - Unused               */
    (VdispSrvIoctl)vdisp_bad_ioctl,                /* 15 - Unused               */
};

    /*
     *
     */
    static long
vdisp_srv_ioctl (struct file* file, unsigned int cmd, unsigned long arg)
{
    VdispSrvFile* vdisp_file = file->private_data;
    unsigned int  nr;
    unsigned int  sz;

    nr = _IOC_NR(cmd) & (VDISP_IOCTL_CMD_MAX - 1);
    sz = _IOC_SIZE(cmd);

    return vdisp_srv_ioctl_ops[nr](vdisp_file, (void*)arg, sz);
}

    /*
     *
     */
    static ssize_t
vdisp_srv_read (struct file* file, char __user* ubuf, size_t cnt, loff_t* ppos)
{
    return -EINVAL;
}

    /*
     *
     */
    static ssize_t
vdisp_srv_write (struct file* file, const char __user* ubuf,
                 size_t count, loff_t* ppos)
{
    return -EINVAL;
}

    /*
     *
     */
    static unsigned int
vdisp_srv_poll(struct file* file, poll_table* wait)
{
    return -ENOSYS;
}

    /*
     *
     */
static struct file_operations vdisp_srv_fops = {
    .owner      = THIS_MODULE,
    .open       = vdisp_srv_open,
    .read       = vdisp_srv_read,
    .write      = vdisp_srv_write,
    .unlocked_ioctl = vdisp_srv_ioctl,
    .release    = vdisp_srv_release,
    .poll       = vdisp_srv_poll,
};

    /*
     *
     */
    static int
vdisp_srv_vlink_abort (Vlink* vlink, void* cookie)
{
    VdispSrvDev* dev = (VdispSrvDev*) cookie;

    VLINK_DTRACE(vlink, "vdisp_srv_vlink_abort called\n");

    vdisp_rpc_wakeup(&dev->rpc);

    wake_up(&vdisp_control_drv.hw_property_wait);

    return 0;
}

    /*
     *
     */
    static int
vdisp_srv_vlink_reset (Vlink* vlink, void* cookie)
{
    VdispSrvDev* dev = (VdispSrvDev*) cookie;

    VLINK_DTRACE(vlink, "vdisp_srv_vlink_reset called\n");

    vdisp_rpc_reset(&dev->rpc);


    return 0;
}

    /*
     *
     */
    static int
vdisp_srv_vlink_start (Vlink* vlink, void* cookie)
{
    VdispSrvDev* dev = (VdispSrvDev*) cookie;

    VLINK_DTRACE(vlink, "vdisp_srv_vlink_start called\n");

    nkops.nk_xirq_unmask(dev->gen_dev.sxirqs);

    return 0;
}

    /*
     *
     */
    static int
vdisp_srv_vlink_stop (Vlink* vlink, void* cookie)
{
    VdispSrvDev* dev = (VdispSrvDev*) cookie;

    VLINK_DTRACE(vlink, "vdisp_srv_vlink_stop called\n");

    nkops.nk_xirq_mask(dev->gen_dev.sxirqs);

    return 0;
}

    /*
     *
     */
    static int
vdisp_srv_vlink_cleanup (Vlink* vlink, void* cookie)
{
    VdispSrvDev* dev = (VdispSrvDev*) cookie;
    VdispSrvDrv* drv = &dev->gen_dev.drv->srv;

    VLINK_DTRACE(vlink, "vdisp_srv_vlink_cleanup called\n");

    dev->gen_dev.enabled = 0;

    vdisp_gen_dev_cleanup(&dev->gen_dev);

    if (dev->class_dev) {
        device_destroy(drv->class, MKDEV(drv->major, vlink->unit));
        dev->class_dev = NULL;
    }

    return 0;
}

    /*
     *
     */
static VlinkOpDesc vdisp_srv_vlink_ops[] = {
    { VLINK_OP_RESET,   vdisp_srv_vlink_reset   },
    { VLINK_OP_START,   vdisp_srv_vlink_start   },
    { VLINK_OP_ABORT,   vdisp_srv_vlink_abort   },
    { VLINK_OP_STOP,    vdisp_srv_vlink_stop    },
    { VLINK_OP_CLEANUP, vdisp_srv_vlink_cleanup },
    { 0,                NULL                    },
};

    /*
     *
     */
    static int
vdisp_srv_prop_get (Vlink* vlink, unsigned int type, void* prop)
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
static VdispDrv vdisp_srv_drv = {
    .srv = {
        .gen_drv = {
             .name         = "vdisplay-srv",
             .resc_id_base = 0,
             .prop_get     = vdisp_srv_prop_get,
         },
        .major             = VDISP_SRV_MAJOR,
    },
};

/****************************************************************************/
/*
 * vDisplay subcomponent
 *  - local BE-User channel for events 
 *    export to userland /dev/vdisplay-ctrl
 */

/* init the control clients list */
VdispControlClient control_clients;
spinlock_t         control_clients_lock;

static int current_focused;
static int previous_focused;

struct class*      vdisp_control_class;
struct device*     vdisp_control_class_dev;

    /*
     *
     */
static VdispControlDrv vdisp_control_drv = {
    .name         = "vdisplay-ctrl",
    .major        = VDISP_SRV_MAJOR+1,
};


    /*
     *
     */
    static void
_propagate_event (nku32_f type, int value)
{
    VdispControlClient* client;

    /* Protect the global list */
    spin_lock(&control_clients_lock);
    list_for_each_entry(client, &control_clients.list, list) {
        VDISP_DTRACE ("client=%p ->inode=%p ->file=%p ->event_mask=%x\n",
                      client, client->inode, client->file, client->enabled_event_mask);
        /* Put event to client requesting the corresponding event type */
        if (client->enabled_event_mask & type) {
            nku32_f event = VDISP_EVENT(type, value);
            kfifo_in_locked(&client->events_fifo,
                            &event,
                            sizeof (nku32_f),
                            &client->events_fifo_lock);
            VDISP_DTRACE ("  add event=0x%x\n", event);
            wake_up(&client->events_fifo_wait);
        }
    }
    spin_unlock(&control_clients_lock);
}

    /*
     *
     */
    static int
vdisp_control_open (struct inode* inode, struct file* file)
{
    VdispControlClient* client;

    VDISP_FUNC_ENTER();
    /* create a new event client */
    client = (VdispControlClient*) kzalloc(sizeof(VdispControlClient), GFP_KERNEL);
    if (!client) {
        return -ENOMEM;
    }
    /* Initialize fifo lock and wait queue */
    spin_lock_init (&client->events_fifo_lock);
    init_waitqueue_head(&client->events_fifo_wait);

    /* create an associated event fifo supporting up to 8 events */
    if (kfifo_alloc(&client->events_fifo,
                    sizeof(long) * VDISP_EVENT_FIFO_SIZE,
                    GFP_KERNEL)) {
        kfree(client);
        return -ENOMEM;
    }

    /* initialize remaining field */
    client->enabled_event_mask = 0; // disable all events by default
    client->inode = inode;
    client->file = file;
    file->private_data = client;

    /* Protect the global list */
    spin_lock(&control_clients_lock);
    /* Register (add) new client to the end of the global list */
    list_add_tail(&(client->list), &(control_clients.list));
    spin_unlock(&control_clients_lock);
    VDISP_FUNC_LEAVE();

    return 0;
}

    /*
     *
     */
    static int
vdisp_control_release (struct inode* inode, struct file* file)
{
    VdispControlClient*   client;

    VDISP_FUNC_ENTER(); 

    /* Get the event client */
    client = (VdispControlClient*) file->private_data;
    if (!client) {
        return -ENODEV;
    }
    file->private_data = NULL;

    /* Protect the global list */
    spin_lock(&control_clients_lock);
    /* Remove first the client from global control_clients */
    list_del(&client->list); 
    spin_unlock(&control_clients_lock);

    /* Destroy safely the event fifo */

    kfifo_free(&client->events_fifo);

    /* Destroy the event client */
    kfree(client);

    VDISP_FUNC_LEAVE();

    return 0;
}
    /*
     *
     */
static int vdisp_control_enable_events_ioctl (
                    VdispControlClient* client,
                    void* arg,
                    unsigned int sz)
{
    nku32_f   events_mask;
    int       size = sizeof(nku32_f);

    if (sz < size) {
        return -EINVAL;
    }

    if (copy_from_user(&events_mask, arg, size)) {
        return -EFAULT;
    }

    /* set events mask */
    spin_lock(&control_clients_lock);
    client->enabled_event_mask = events_mask;
    spin_unlock(&control_clients_lock);

    return 0;
}

    /*
     *
     */
static int vdisp_control_listen_events_ioctl (
                    VdispControlClient* client,
                    void* arg,
                    unsigned int sz)
{
    nku32_f   event = 0;
    int       diag;

    if (sz < sizeof(nku32_f)) {
        return -EINVAL;
    }

    /* wait if fifo is empty */
    diag = wait_event_interruptible(client->events_fifo_wait,
                                    !kfifo_is_empty(&client->events_fifo));
    if (diag) {
        return diag;
    }

    /* get event */
    WARN_ON(kfifo_out_locked(&client->events_fifo,
			     &event,
			     sizeof(nku32_f),
			     &client->events_fifo_lock) != sizeof(nku32_f));

    if (copy_to_user(arg, &event, sizeof(nku32_f))) {
        return -EFAULT;
    }

    return 0;
}

    /*
     *
     */
static int vdisp_control_get_focused_client_ioctl (
                    VdispControlClient* client,
                    void* arg,
                    unsigned int sz)
{
    nku32_f   focus_id = current_focused;
    int       size = sizeof(nku32_f);


    if (sz < size) {
        return -EINVAL;
    }

    if (copy_to_user(arg, &focus_id, size)) {
        return -EFAULT;
    }

    return 0;
}

    /*
     *
     */
static int vdisp_control_get_client_state_ioctl (
                    VdispControlClient* client,
                    void* arg,
                    unsigned int sz)
{
    nku32_f    states = 0;
    int        size = sizeof(nku32_f);

    if (sz < size) {
        return -EINVAL;
    }

    if (copy_to_user(arg, &states, size)) {
        return -EFAULT;
    }

    return 0;
}

    /*
     *
     */
static int vdisp_control_set_display_info_ioctl (VdispControlClient* client, void* arg,
                            unsigned int sz)
{
    DisplayProperty* hw_property = &vdisp_control_drv.hw_property;
    int              size = sizeof(DisplayProperty);

    if (sz < size) {
        return -EINVAL;
    }

    // TODO: check concurrent access of hw_property
    if (copy_from_user(hw_property, arg, size)) {
        return -EFAULT;
    }

    /* wake up hardware info wait queue */
    wake_up(&vdisp_control_drv.hw_property_wait);

    return 0;
}


    /*
     *
     */
static int vdisp_control_bad_ioctl (VdispControlClient* client, void* arg,
                            unsigned int sz)
{
    VDISP_ERROR("Events: invalid ioctl command\n");
    return -EINVAL;
}

    /*
     *
     */
static VdispControlIoctl vdisp_control_ioctl_ops[VDISP_CONTROL_IOCTL_CMD_MAX] = {
    (VdispControlIoctl)vdisp_control_enable_events_ioctl,    /*  0 - Enable events */
    (VdispControlIoctl)vdisp_control_listen_events_ioctl,    /*  1 - Listen events */
    (VdispControlIoctl)vdisp_control_get_focused_client_ioctl,  /*  2 - Get focused FE */
    (VdispControlIoctl)vdisp_control_get_client_state_ioctl, /*  3 - Get FE state  */
    (VdispControlIoctl)vdisp_control_set_display_info_ioctl, /*  4 - Set common display property */
    (VdispControlIoctl)vdisp_control_bad_ioctl,              /*  5 - unused        */
    (VdispControlIoctl)vdisp_control_bad_ioctl,              /*  6 - unused        */
    (VdispControlIoctl)vdisp_control_bad_ioctl,              /*  7 - unused        */
};

    /*
     *
     */
    static long
vdisp_control_ioctl (struct file* file, unsigned int cmd, unsigned long arg)
{
    VdispControlClient* client = (VdispControlClient*) file->private_data;
    unsigned int  nr;
    unsigned int  sz;

    nr = _IOC_NR(cmd) & (VDISP_CONTROL_IOCTL_CMD_MAX - 1);
    sz = _IOC_SIZE(cmd);

    return vdisp_control_ioctl_ops[nr](client, (void*)arg, sz);

    return 0;
}

    /*
     *
     */
    static ssize_t
vdisp_control_read (struct file* file, char __user* ubuf, size_t cnt, loff_t* ppos)
{
    /* Not supported */
    return -EINVAL;
}

    /*
     *
     */
    static ssize_t
vdisp_control_write (struct file* file, const char __user* ubuf,
                 size_t count, loff_t* ppos)
{
    /* Not supported */
    return -EINVAL;
}

    /*
     *
     */
    static unsigned int
vdisp_control_poll(struct file* file, poll_table* wait)
{
    /* Not supported */
    return -ENOSYS;
}

    /*
     *
     */
static struct file_operations vdisp_control_fops = {
    .owner      = THIS_MODULE,
    .open       = vdisp_control_open,
    .read       = vdisp_control_read,
    .write      = vdisp_control_write,
    .unlocked_ioctl = vdisp_control_ioctl,
    .release    = vdisp_control_release,
    .poll       = vdisp_control_poll,
};

    /*
     * vDisplay vm focus management
     */
extern int focus_register_client(struct notifier_block *nb);
extern int focus_unregister_client(struct notifier_block *nb);
static struct notifier_block nb_vm_focus;

    static int
vdisp_notify_focus (struct notifier_block *self,
                   unsigned long event, void *data)
{
    NkOsId vm_focus;
    vm_focus = (NkOsId)event;

    VDISP_DTRACE ("focus changed to OS#%d\n", vm_focus);

    previous_focused = current_focused;
    current_focused = vm_focus;

    /* propagate event to all clients */
    _propagate_event(VDISP_EVENT_TYPE_FOCUS, vm_focus);
    return 0;
}

/****************************************************************************/
/* Main vDisplay module driver on top of vLink */
/****************************************************************************/

    /*
     *
     */
    static int
vdisp_srv_vlink_init (Vlink* vlink)
{
    VdispSrvDrv* drv = &vdisp_srv_drv.srv;
    VdispSrvDev* dev = &drv->gen_drv.devs[vlink->unit].srv;
    int          diag;

    VLINK_DTRACE(vlink, "vdisp_srv_vlink_init called\n");

    /* */
    dev->gen_dev.drv   = &vdisp_srv_drv;
    dev->gen_dev.vlink = vlink;

    if ((diag = vdisp_gen_dev_init(&dev->gen_dev)) != 0) {
        vdisp_srv_vlink_cleanup(vlink, dev);
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
        vdisp_srv_vlink_cleanup(vlink, dev);
        return diag;
    }

    diag = vdisp_rpc_setup(&dev->gen_dev, &dev->rpc, VDISP_RPC_TYPE_SERVER);
    if (diag) {
        vdisp_srv_vlink_cleanup(vlink, dev);
        return diag;
    }

    dev->gen_dev.enabled = 1;

    return 0;
}

    /*
     *
     */
    static void
vdisp_srv_vlink_drv_cleanup (VlinkDrv* vlink_drv)
{
    VdispSrvDrv* drv = &vdisp_srv_drv.srv;

    if (drv->class) {
        class_destroy(drv->class);
        drv->class = NULL;
    }

    if (drv->chrdev_major) {
        unregister_chrdev(drv->major, drv->gen_drv.name);
        drv->chrdev_major = 0;
    }

    vdisp_gen_drv_cleanup(&drv->gen_drv);

    /* Unregister global vm focus */
    focus_unregister_client(&nb_vm_focus);

    if (vdisp_control_class_dev) {
        device_destroy(vdisp_control_class, MKDEV(vdisp_control_drv.major, 0));
        vdisp_control_class_dev = NULL;
    }

    /* Cleanup control channel */
    unregister_chrdev(vdisp_control_drv.major, vdisp_control_drv.name);

    if (vdisp_control_class) {
        class_destroy(vdisp_control_class);
        vdisp_control_class = NULL;
    }
}

    /*
     *
     */
    static int
vdisp_srv_vlink_drv_init (VlinkDrv* vlink_drv)
{
    VdispSrvDrv* drv = &vdisp_srv_drv.srv;
    int          diag;

    /* Initialize global vdisplay event client list */
    INIT_LIST_HEAD (&control_clients.list);
    spin_lock_init (&control_clients_lock);

    /* Setup backend id */
    current_focused = nkops.nk_id_get();

    /* Register to global vm focus */
    nb_vm_focus.notifier_call = vdisp_notify_focus;
    focus_register_client(&nb_vm_focus);

    init_waitqueue_head(&vdisp_control_drv.hw_property_wait);

    /* Initialize control channel */
    diag = register_chrdev(vdisp_control_drv.major,
                           vdisp_control_drv.name,
                           &vdisp_control_fops);
    if (diag < 0) {
        VDISP_ERROR("%s: cannot register major number '%d'\n",
                    vdisp_control_drv.name, vdisp_control_drv.major);
        return diag;
    }
    vdisp_control_class = class_create(THIS_MODULE, vdisp_control_drv.name);
    if (IS_ERR(vdisp_control_class)) {
        VDISP_ERROR("%s: cannot create the device class\n", vdisp_control_drv.name);
        diag = PTR_ERR(vdisp_control_class);
        vdisp_control_class = NULL;
        return diag;
    }
    vdisp_control_class_dev = device_create(vdisp_control_class,
                                   NULL,
                                   MKDEV(vdisp_control_drv.major, 0),
                                   NULL,
                                   "%s",
                                   vdisp_control_drv.name);
    if (IS_ERR(vdisp_control_class_dev)) {
        VDISP_ERROR("%s: cannot create the '%s' device\n",
                    vdisp_control_drv.name, vdisp_control_drv.name);
        diag = PTR_ERR(vdisp_control_class_dev);
        vdisp_control_class_dev = NULL;
        return diag;
    }

    /* Generic driver init */
    drv->gen_drv.vlink_drv = vlink_drv;
    drv->gen_drv.vops      = vdisp_srv_vlink_ops;
    drv->gen_drv.devs      = NULL;

    drv->fops              = &vdisp_srv_fops;
    drv->chrdev_major      = 0;
    drv->class             = NULL;

    if ((diag = vdisp_gen_drv_init(&drv->gen_drv)) != 0) {
        vdisp_srv_vlink_drv_cleanup(vlink_drv);
        return diag;
    }

    diag = register_chrdev(drv->major, drv->gen_drv.name, drv->fops);
    if ((drv->major && diag) || (!drv->major && diag <= 0)) {
        VDISP_ERROR("%s: cannot register major number '%d'\n",
                    drv->gen_drv.name, drv->major);
        vdisp_srv_vlink_drv_cleanup(vlink_drv);
        return diag;
    }
    drv->chrdev_major = drv->major ? drv->major : diag;

    drv->class = class_create(THIS_MODULE, drv->gen_drv.name);
    if (IS_ERR(drv->class)) {
        VDISP_ERROR("%s: cannot create the device class\n", drv->gen_drv.name);
        diag = PTR_ERR(drv->class);
        drv->class = NULL;
        vdisp_srv_vlink_drv_cleanup(vlink_drv);
        return diag;
    }
    return 0;
}

    /*
     *
     */
static VlinkDrv vdisp_srv_vlink_drv = {
    .name       = "vdisplay",
    .init       = vdisp_srv_vlink_drv_init,
    .cleanup    = vdisp_srv_vlink_drv_cleanup,
    .vlink_init = vdisp_srv_vlink_init,
    .flags      = VLINK_DRV_TYPE_SERVER,
};

    /*
     * vDisplay module init
     */
    static int
vdisp_be_module_init (void)
{
    int diag;

    /* Initialize driver */
    if ((diag = vlink_drv_probe(&vdisp_srv_vlink_drv)) != 0) {
        return diag;
    }
    vlink_drv_startup(&vdisp_srv_vlink_drv);

    VDISP_PRINTK("vDisplay initialized !\n");

    return 0;
}

    /*
     * vDisplay module exit
     */
    static void
vdisp_be_module_exit (void)
{
    vlink_drv_shutdown(&vdisp_srv_vlink_drv);
    vlink_drv_cleanup(&vdisp_srv_vlink_drv);
}

module_init(vdisp_be_module_init);
module_exit(vdisp_be_module_exit);

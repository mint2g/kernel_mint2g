/*
 ****************************************************************
 *
 *  Component: VLX virtual Real Time Clock backend driver
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
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

/*----- System header files -----*/

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include <nk/nkern.h>
#include <vlx/vrtc_common.h>
#include "vrpc.h"

/*----- Local configuration -----*/

#if 0
#define VRTC_DEBUG
#endif

#define VRTC_VRPC_NAME	"vrtc"
#define VRTC_DEVICE	"rtc0"

/*----- Tracing -----*/

#ifdef VRTC_DEBUG
#define DTRACE(format, args...)	printk ("%s: " format, __func__, ##args)
#else
#define DTRACE(x...)
#endif

#define TRACE(x...)	printk (KERN_NOTICE  "VRTC-BE: "          x)
#define WTRACE(x...)	printk (KERN_WARNING "VRTC-BE: Warning: " x)
#define ETRACE(x...)	printk (KERN_ERR     "VRTC-BE: Error: "   x)

#ifdef VRTC_DEBUG
static const char* vrtc_cmd_name [VRTC_CMD_MAX] = VRTC_CMD_NAME;
#endif

/*----- Data types -----*/

typedef struct vrtc_t {
    struct rtc_device*	rtc_device;
    struct vrpc_t*	vrpc;
    void*		vrpc_data;
    vrpc_size_t		vrpc_maxsize;
    NkXIrq		cxirq;
    struct vrtc_t*	next;
    rtc_task_t		rtc_task;
    NkPhAddr		pshared;
    vrtc_shared_t*	vshared;
} vrtc_t;

static vrtc_t*	vrtcs;
static struct delayed_work vrtc_work;

#define VRTC_FOR_ALL_VRTCS(vrtc) \
    for ((vrtc) = vrtcs; (vrtc); (vrtc) = (vrtc)->next)

/*----- Implementation of remote commands -----*/

    static vrpc_size_t
vrtc_cmd_read_time (vrtc_t* vrtc)
{
    vrtc_res_time_t*	res = vrtc->vrpc_data;
    struct rtc_time	tm;

    DTRACE ("\n");
    res->common.res = rtc_read_time (vrtc->rtc_device, &tm);
    if (res->common.res) {
	return sizeof *res;
    }
#define VRTC_ASSIGN(x)	res->time.x = tm.x
    VRTC_ASSIGN (tm_sec);
    VRTC_ASSIGN (tm_min);
    VRTC_ASSIGN (tm_hour);
    VRTC_ASSIGN (tm_mday);
    VRTC_ASSIGN (tm_mon);
    VRTC_ASSIGN (tm_year);
    VRTC_ASSIGN (tm_wday);
    VRTC_ASSIGN (tm_yday);
    VRTC_ASSIGN (tm_isdst);
#undef VRTC_ASSIGN

    return sizeof *res;
}

    static vrpc_size_t
vrtc_cmd_set_time (vrtc_t* vrtc)
{
    const vrtc_req_time_t*	req = vrtc->vrpc_data;
    vrtc_res_time_t*		res = vrtc->vrpc_data;
    struct rtc_time		tm;

    DTRACE ("\n");
#define VRTC_ASSIGN(x)	tm.x = req->time.x
    VRTC_ASSIGN (tm_sec);
    VRTC_ASSIGN (tm_min);
    VRTC_ASSIGN (tm_hour);
    VRTC_ASSIGN (tm_mday);
    VRTC_ASSIGN (tm_mon);
    VRTC_ASSIGN (tm_year);
    VRTC_ASSIGN (tm_wday);
    VRTC_ASSIGN (tm_yday);
    VRTC_ASSIGN (tm_isdst);
#undef VRTC_ASSIGN

    res->common.res = rtc_set_time (vrtc->rtc_device, &tm);
    DTRACE ("set_time %d\n", res->common.res);
    return sizeof *res;
}

    static int
vrtc_cmd_read_alarm (vrtc_t* vrtc)
{
    vrtc_res_wkalrm_t*	res = vrtc->vrpc_data;
    vrtc_wkalrm_t*	remote_wkalrm = &res->alarm;
    struct rtc_wkalrm	local_wkalrm;

    DTRACE ("\n");
    res->common.res = rtc_read_alarm (vrtc->rtc_device, &local_wkalrm);
    if (res->common.res) {
	return sizeof *res;
    }
    remote_wkalrm->enabled = local_wkalrm.enabled;
    remote_wkalrm->pending = local_wkalrm.pending;

#define VRTC_ASSIGN(x)	remote_wkalrm->time.x = local_wkalrm.time.x
    VRTC_ASSIGN (tm_sec);
    VRTC_ASSIGN (tm_min);
    VRTC_ASSIGN (tm_hour);
    VRTC_ASSIGN (tm_mday);
    VRTC_ASSIGN (tm_mon);
    VRTC_ASSIGN (tm_year);
    VRTC_ASSIGN (tm_wday);
    VRTC_ASSIGN (tm_yday);
    VRTC_ASSIGN (tm_isdst);
#undef VRTC_ASSIGN

    return sizeof *res;
}

    static int
vrtc_cmd_set_alarm (vrtc_t* vrtc)
{
    vrtc_req_wkalrm_t*		req = vrtc->vrpc_data;
    const vrtc_wkalrm_t*	remote_wkalrm = &req->alarm;
    vrtc_res_wkalrm_t*		res = vrtc->vrpc_data;
    struct rtc_wkalrm		local_wkalrm;

    DTRACE ("\n");
    local_wkalrm.enabled = remote_wkalrm->enabled;
    local_wkalrm.pending = remote_wkalrm->pending;

#define VRTC_ASSIGN(x)	local_wkalrm.time.x = remote_wkalrm->time.x
    VRTC_ASSIGN (tm_sec);
    VRTC_ASSIGN (tm_min);
    VRTC_ASSIGN (tm_hour);
    VRTC_ASSIGN (tm_mday);
    VRTC_ASSIGN (tm_mon);
    VRTC_ASSIGN (tm_year);
    VRTC_ASSIGN (tm_wday);
    VRTC_ASSIGN (tm_yday);
    VRTC_ASSIGN (tm_isdst);
#undef VRTC_ASSIGN

    res->common.res = rtc_set_alarm (vrtc->rtc_device, &local_wkalrm);
    DTRACE ("set_alarm %d\n", res->common.res);
    return sizeof *res;
}

    static vrpc_size_t
vrtc_process_calls (void* cookie, vrpc_size_t size)
{
    vrtc_t*		vrtc = (vrtc_t*) cookie;
    vrtc_req_t*		req  = (vrtc_req_t*) vrtc->vrpc_data;
    vrtc_res_t*		res  = (vrtc_res_t*) vrtc->vrpc_data;
    struct rtc_device*	rtc_device = vrtc->rtc_device;

    DTRACE ("\n");
    if (vrtc->vrpc_maxsize < sizeof (vrtc_res_t) ||
	    size < sizeof (vrtc_req_t)) {
	ETRACE ("VRPC request size error (max %d res %d size %d req %d)\n",
		vrtc->vrpc_maxsize, sizeof (vrtc_res_t), size,
		sizeof (vrtc_req_t));
	return 0;
    }
    DTRACE ("received vcmd %u (%s), arg %u\n", req->vcmd,
	    vrtc_cmd_name [req->vcmd % VRTC_CMD_MAX], req->arg);

    switch (req->vcmd) {
    case VRTC_CMD_OPEN:
    case VRTC_CMD_RELEASE:
	res->res = 0;
	return sizeof *res;

    case VRTC_CMD_IOCTL_RTC_AIE_ON:	/* Alarm Interrupt On */
    case VRTC_CMD_IOCTL_RTC_AIE_OFF:	/* Alarm Interrupt Off */
    case VRTC_CMD_IOCTL_RTC_UIE_ON:	/* Update Interrupt On */
    case VRTC_CMD_IOCTL_RTC_UIE_OFF:	/* Update Interrupt Off */
    case VRTC_CMD_IOCTL_RTC_PIE_ON:	/* Periodic Interrupt On */
    case VRTC_CMD_IOCTL_RTC_PIE_OFF:	/* Periodic Interrupt Off */
    case VRTC_CMD_IOCTL_RTC_WIE_ON:	/* Watchdog Interrupt On */
    case VRTC_CMD_IOCTL_RTC_WIE_OFF: {	/* Watchdog Interrupt Off */
	unsigned	cmd;

	if (!rtc_device->ops->ioctl) {
	    DTRACE ("Driver has no ioctl op\n");
	    res->res = -ENOIOCTLCMD;
	    return sizeof *res;
	}
#define VRTC_CASE(x)	case VRTC_CMD_IOCTL_##x: cmd = x; break
	switch (req->vcmd) {
	default:
	VRTC_CASE (RTC_AIE_ON);
	VRTC_CASE (RTC_AIE_OFF);
	VRTC_CASE (RTC_UIE_ON);
	VRTC_CASE (RTC_UIE_OFF);
	VRTC_CASE (RTC_PIE_ON);
	VRTC_CASE (RTC_PIE_OFF);
	VRTC_CASE (RTC_WIE_ON);
	VRTC_CASE (RTC_WIE_OFF);
	}
#undef VRTC_CASE
	res->res = rtc_device->ops->ioctl (rtc_device->dev.parent,
					   cmd, req->arg);
	return sizeof *res;
    }
    case VRTC_CMD_READ_TIME:
	return vrtc_cmd_read_time (vrtc);

    case VRTC_CMD_SET_TIME:
	return vrtc_cmd_set_time (vrtc);

    case VRTC_CMD_READ_ALARM:
	return vrtc_cmd_read_alarm (vrtc);

    case VRTC_CMD_SET_ALARM:
	return vrtc_cmd_set_alarm (vrtc);

    case VRTC_CMD_IRQ_SET_STATE:
	res->res = rtc_irq_set_state (rtc_device, rtc_device->irq_task,
				      req->arg);
	return sizeof *res;

    case VRTC_CMD_IRQ_SET_FREQ:
	res->res = rtc_irq_set_freq (rtc_device, rtc_device->irq_task,
				     req->arg);
	return sizeof *res;

    default:
	break;
    }
    return 0;
}

/*----- Initialization -----*/

    static void
vrtc_rtc_task (void* cookie)
{
    struct rtc_device*	rtc_device = (struct rtc_device*) cookie;
    nku32_f		vevents = 0;
    vrtc_t*		vrtc;
    unsigned long	events;

    spin_lock_irq (&rtc_device->irq_lock);
    events = rtc_device->irq_data;
    rtc_device->irq_data = 0;
    spin_unlock_irq (&rtc_device->irq_lock);

    DTRACE ("events 0x%lx\n", events);
    if (!events) return;

    if (events & RTC_UF) {
	vevents |= VRTC_EVENT_UF;
    }
    if (events & RTC_AF) {
	vevents |= VRTC_EVENT_AF;
    }
    if (events & RTC_PF) {
	vevents |= VRTC_EVENT_PF;
    }
    VRTC_FOR_ALL_VRTCS (vrtc) {
	if (vrtc->rtc_device == rtc_device) {
	    DTRACE ("changed %x, sending interrupt to FE\n", vevents);
	    nkops.nk_atomic_set (&vrtc->vshared->events, vevents);
	    nkops.nk_xirq_trigger (vrtc->cxirq, vrpc_vlink (vrtc->vrpc)->c_id);
	}
    }
}

    static _Bool
vrtc_create (struct rtc_device* rtc_device, struct vrpc_t* vrpc)
{
    vrtc_t*	vrtc;
    int		diag;

    DTRACE ("\n");
    vrtc = (vrtc_t*) kzalloc (sizeof (vrtc_t), GFP_KERNEL);
    if (!vrtc) {
	ETRACE ("memory allocation failed\n");
	return 0;
    }
    vrtc->rtc_device	= rtc_device;
    vrtc->vrpc		= vrpc;
    vrtc->vrpc_data	= vrpc_data (vrpc);
    vrtc->vrpc_maxsize	= vrpc_maxsize (vrpc);

    vrtc->rtc_task.func		= vrtc_rtc_task;
    vrtc->rtc_task.private_data	= rtc_device;
    diag = rtc_irq_register (rtc_device, &vrtc->rtc_task);
    if (diag) {
	WTRACE ("Could not register RTC interrupt (%d), "
		"alarms will not be delivered to frontend %d (vrpc,%d).\n",
		diag, vrpc_peer_id (vrpc), vrpc_vlink (vrpc)->link);
	vrtc->rtc_task.func = 0;
    }
    if (vrtc->vrpc_maxsize < sizeof (vrtc_req_t) ||
	    vrtc->vrpc_maxsize < sizeof (vrtc_res_t)) {
	ETRACE ("Not enough VRPC shared memory (%d)\n", vrtc->vrpc_maxsize);
	goto error_irq_unregister;
    }
    if ((diag = vrpc_server_open (vrpc, vrtc_process_calls, vrtc, 0)) != 0) {
	ETRACE ("VRPC open failed (%d)\n", diag);
	goto error_irq_unregister;
    }
    vrtc->pshared = nkops.nk_pmem_alloc (vrpc_plink (vrtc->vrpc),
					 VRPC_PMEM_BASE,
					 sizeof (vrtc_shared_t));
    if (!vrtc->pshared) {
	ETRACE ("Cannot allocate %d bytes of pmem.\n", sizeof (vrtc_shared_t));
	goto error_irq_unregister;
    }
    vrtc->vshared  = (vrtc_shared_t*)
	nkops.nk_mem_map (vrtc->pshared, sizeof (vrtc_shared_t));
    if (!vrtc->vshared) {
	ETRACE ("Could not map shared pmem.\n");
	goto error_irq_unregister;
    }
    vrtc->cxirq = nkops.nk_pxirq_alloc (vrpc_plink (vrtc->vrpc),
					VRPC_PXIRQ_BASE,
					vrpc_vlink (vrtc->vrpc)->c_id, 1);
    if (!vrtc->cxirq) {
	ETRACE ("Could not allocate cross-interrupt\n");
	goto error_irq_unregister;
    }
    DTRACE ("VRTC '%s' exported to guest %d\n", rtc_device->name,
	    vrpc_peer_id (vrpc));
    vrtc->next	= vrtcs;
    vrtcs	= vrtc;
    return 1;

error_irq_unregister:
    if (vrtc->rtc_task.func) {
	rtc_irq_unregister (rtc_device, &vrtc->rtc_task);
    }
    kfree (vrtc);
    return 0;
}

    static void __exit
vrtc_destroy (vrtc_t* vrtc)
{
    vrtc_t** link = &vrtcs;

    DTRACE ("VRTC '%s' for guest %d destroyed\n", vrtc->rtc_device->name,
	    vrpc_peer_id (vrtc->vrpc));
    vrpc_close (vrtc->vrpc);
    vrpc_release (vrtc->vrpc);

    while (*link != vrtc) link = &(*link)->next;
    *link = vrtc->next;

    if (vrtc->rtc_task.func) {
	rtc_irq_unregister (vrtc->rtc_device, &vrtc->rtc_task);
    }
    rtc_class_close (vrtc->rtc_device);
    kfree (vrtc);
}

    static vrtc_t*
vrtc_lookup (struct rtc_device* rtc_device, struct vrpc_t* vrpc)
{
    vrtc_t* vrtc;

    DTRACE ("\n");
    VRTC_FOR_ALL_VRTCS (vrtc) {
	if (vrtc->rtc_device == rtc_device &&
		vrpc_peer_id (vrtc->vrpc) == vrpc_peer_id (vrpc)) {
	    break;
	}
    }
    return vrtc;
}

    static unsigned
vrtc_setup (struct rtc_device* rtc_device)
{
    struct vrpc_t* vrpc = 0;
    unsigned used = 0;

    DTRACE ("\n");
    while ((vrpc = vrpc_server_lookup (VRTC_VRPC_NAME, vrpc)) != 0) {
	if (vrtc_lookup (rtc_device, vrpc)) {
	    vrpc_release (vrpc);
	} else if (!vrtc_create (rtc_device, vrpc)) {
	    vrpc_release (vrpc);
	} else {
	    ++used;
	}
    }
    return used;
}

    /*
     * -1 = hopeless, stop retrying
     *  0 = keep retrying
     *  1 = done, stop retrying
     */

    static int
vrtc_try_setup (void)
{
    struct rtc_device*	rtc_device;

    rtc_device = rtc_class_open ((char*) VRTC_DEVICE);
    if (!rtc_device) {
	DTRACE ("No %s device (yet)\n", VRTC_DEVICE);
	return 0;
    }
    if (!vrtc_setup (rtc_device)) {
	rtc_class_close (rtc_device);
	ETRACE ("No vlinks need device\n");
	return -1;
    }
    TRACE ("Using device %s\n", VRTC_DEVICE);
    return 1;
}

    static void
vrtc_work_hdl (struct work_struct* work)
{
    if (vrtc_try_setup()) return;
    schedule_delayed_work ((struct delayed_work*) work, HZ);
}

/*----- Initialization and exit code -----*/

#ifdef VRTC_DEBUG
    static void
vrtc_check (const char* name)
{
    struct rtc_device* rtc_device;

    rtc_device = rtc_class_open ((char*) name);
    if (rtc_device) {
	printk ("OK open '%s' => '%s'\n", name, rtc_device->name);
	rtc_class_close (rtc_device);
    } else {
	printk ("Could NOT open '%s'\n", name);
    }
}
#endif

    static int __init
vrtc_init (void)
{
    int	diag;

    DTRACE ("\n");
    INIT_DELAYED_WORK (&vrtc_work, vrtc_work_hdl);
#ifdef VRTC_DEBUG
    vrtc_check ("rtc");
    vrtc_check ("rtc0");
    vrtc_check ("rtc1");
#endif
    diag = vrtc_try_setup();
    if (!diag) {
	WTRACE ("No %s device yet, will keep trying\n", VRTC_DEVICE);
	schedule_delayed_work (&vrtc_work, HZ);
    } else if (diag < 0) {
	return -ESRCH;
	    /* Otherwise got the device */
    }
    TRACE ("module loaded\n");
    return 0;
}

    static void __exit
vrtc_exit (void)
{
    DTRACE ("\n");
    cancel_delayed_work_sync (&vrtc_work);
    while (vrtcs) {
	vrtc_destroy (vrtcs);
    }
    TRACE ("module unloaded\n");
}

#ifdef device_initcall_sync
device_initcall_sync (vrtc_init);
#else
device_initcall (vrtc_init);
#endif
module_exit (vrtc_exit);

/*----- Module description -----*/

MODULE_AUTHOR      ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_DESCRIPTION ("VLX Real Time Clock backend driver (RTC)");
MODULE_LICENSE     ("GPL");

/*----- End of file -----*/

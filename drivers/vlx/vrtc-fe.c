/*
 ****************************************************************
 *
 *  Component: VLX virtual Real Time Clock frontend driver
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

#include <linux/version.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <nk/nk.h>
#include <nk/nkdev.h>
#include <nk/nkern.h>
#include "vrpc.h"
#include <vlx/vrtc_common.h>

/*----- Local configuration -----*/

#if 0
#define VRTC_DEBUG
#endif

#define VRTC_DEVICE_NAME	VRTC_VRPC_NAME

/*----- Tracing -----*/

#ifdef VRTC_DEBUG
#define DTRACE(format, args...)	\
	printk (KERN_CRIT "%s: " format, __func__, ##args)
#else
#define DTRACE(x...)
#endif

#define TRACE(x...)	printk (KERN_NOTICE  "VRTC-FE: " x)
#define WTRACE(x...)	printk (KERN_WARNING "VRTC-FE: " x)
#define ETRACE(x...)	printk (KERN_ERR     "VRTC-FE: " x)

static const char* vrtc_cmd_name [VRTC_CMD_MAX] = VRTC_CMD_NAME;

/*----- Data types -----*/

typedef struct {
	/* From Linux */
    struct platform_device* pdev;
    struct rtc_device*	rtc;
	/* Internal */
    struct vrpc_t*	vrpc;
    void*		vrpc_data;
    NkXIrq		cxirq;
    NkXIrqId		cxid;
    NkPhAddr		pshared;
    vrtc_shared_t*	vshared;
    int			irq_wake_enabled;
	/* Statistics */
    unsigned		remote_calls [VRTC_CMD_MAX];
    unsigned		irqs_total;
    unsigned		irqs_split [3];
} vrtc_t;

#define VRTC_REMOTE_CALL(vrtc,name)     ++(vrtc)->remote_calls [name]

/*----- Helpers ------*/

    static vrpc_size_t
vrtc_call_ext (vrtc_t* vrtc, const vrtc_cmd_t vcmd, const nku32_f arg,
	       const vrpc_size_t size0)
{
    struct vrpc_t*	vrpc = vrtc->vrpc;
    vrtc_req_t*		req  = vrtc->vrpc_data;
    vrpc_size_t		size;

    DTRACE ("vcmd %s arg %x\n", vrtc_cmd_name [vcmd], arg);
    VRTC_REMOTE_CALL (vrtc, vcmd);
    for (;;) {
	req->vcmd = vcmd;
	req->arg = arg;
	size     = size0;
	if (!vrpc_call (vrpc, &size)) {
	    break;
	}
	ETRACE ("Lost backend. Closing and reopening.\n");
	vrpc_close (vrpc);
	if (vrpc_client_open (vrpc, 0, 0)) {
	    BUG();
	}
	TRACE ("Re-established backend link.\n");
    }
    return size;
}

    static inline vrpc_size_t
vrtc_call (vrtc_t* vrtc, const vrtc_cmd_t vcmd, const nku32_f arg)
{
    return vrtc_call_ext (vrtc, vcmd, arg, sizeof (vrtc_req_t));
}

    /* Interrupt signalling an RTC change */

    static void
vrtc_cxirq_handler (void* cookie, NkXIrq xirq)
{
    vrtc_t*	vrtc = (vrtc_t*) cookie;
    nku32_f	vevents;
    unsigned	events = 0;

    (void) xirq;
    DTRACE ("\n");
    ++vrtc->irqs_total;

#define VRTC_FOR_EVENTS(addr, val) \
    for ((val) = *(addr); (val); \
	 (val) = nkops.nk_clear_and_test ((addr), (val)))

    VRTC_FOR_EVENTS (&vrtc->vshared->events, vevents) {
	if (vevents & VRTC_EVENT_UF) {
	    events |= RTC_UF | RTC_IRQF;
	    ++vrtc->irqs_split [0];
	}
	if (vevents & VRTC_EVENT_AF) {
	    events |= RTC_AF | RTC_IRQF;
	    ++vrtc->irqs_split [1];
	}
	if (vevents & VRTC_EVENT_PF) {
	    events |= RTC_PF | RTC_IRQF;
	    ++vrtc->irqs_split [2];
	}
    }
    rtc_update_irq (vrtc->rtc, 1, events);
}

#undef VRTC_FOR_EVENTS

/*----- Exported RTC operations -----*/

    static int
vrtc_op_open (struct device* dev)
{
    vrtc_t*	vrtc = dev_get_drvdata (dev);
    vrtc_res_t*	res = vrtc->vrpc_data;
    vrpc_size_t	size;

    DTRACE ("\n");
    size = vrtc_call (vrtc, VRTC_CMD_OPEN, 0);
    DTRACE ("got: size %d res %d value %d\n", size, res->res, res->value);
    if (size != sizeof (vrtc_res_t) || res->res) {
	return -EFAULT;
    }
    return 0;
}

    static void
vrtc_op_release (struct device* dev)
{
    vrtc_t* vrtc = dev_get_drvdata (dev);

    DTRACE ("\n");
    vrtc_call (vrtc, VRTC_CMD_RELEASE, 0);
}

    static int
vrtc_op_ioctl (struct device* dev, unsigned int cmd, unsigned long arg)
{
    vrtc_t*	vrtc = dev_get_drvdata (dev);
    vrtc_res_t*	res = vrtc->vrpc_data;
    vrtc_cmd_t	vcmd;
    vrpc_size_t	size;

    DTRACE ("cmd %x arg %ld\n", cmd, arg);

#define VRTC_CASE(x)	case x: vcmd = VRTC_CMD_IOCTL_##x; break;
    switch (cmd) {
    VRTC_CASE (RTC_AIE_ON)
    VRTC_CASE (RTC_AIE_OFF)
    VRTC_CASE (RTC_UIE_ON)
    VRTC_CASE (RTC_UIE_OFF)
    default:
	DTRACE ("ioctl 0x%x not handled\n", cmd);
	return -ENOIOCTLCMD;
    }
#undef VRTC_CASE

    DTRACE ("vcmd %d name %s\n", vcmd, vrtc_cmd_name [vcmd]);
    size = vrtc_call (vrtc, vcmd, arg);
    DTRACE ("got: size %d res %d value %d\n", size, res->res, res->value);
    if (size != sizeof (vrtc_res_t) || res->res) {
	return -EFAULT;
    }
    return 0;
}

    static int
vrtc_op_read_time (struct device* dev, struct rtc_time* tm)
{
    vrtc_t*		vrtc = dev_get_drvdata (dev);
    vrtc_res_time_t*	res = vrtc->vrpc_data;
    vrpc_size_t		size = sizeof (*res);

    DTRACE ("\n");
    size = vrtc_call_ext (vrtc, VRTC_CMD_READ_TIME, 0, size);
    DTRACE ("got: size %d res %d value %d\n", size, res->common.res,
	    res->common.value);
    if (size != sizeof (*res) || res->common.res) {
	return -EFAULT;
    }
#define VRTC_ASSIGN(x)	tm->x = res->time.x
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

    return 0;
}

    static int
vrtc_op_set_time (struct device* dev, struct rtc_time* tm)
{
    vrtc_t*		vrtc = dev_get_drvdata (dev);
    vrtc_req_time_t*	req = vrtc->vrpc_data;
    vrpc_size_t		size = sizeof (*req);

    DTRACE ("\n");

#define VRTC_ASSIGN(x)	req->time.x = tm->x
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

    size = vrtc_call_ext (vrtc, VRTC_CMD_SET_TIME, 0, size);
    DTRACE ("got: size %d res %d value %d\n", size, req->common.vcmd,
	    req->common.arg);
    if (size != sizeof (*req) || req->common.arg) {
	return -EFAULT;
    }
    return 0;
}

    static int
vrtc_op_read_alarm (struct device* dev, struct rtc_wkalrm* alrm)
{
    vrtc_t*		vrtc = dev_get_drvdata (dev);
    vrtc_res_wkalrm_t*	res = vrtc->vrpc_data;
    vrpc_size_t		size = sizeof (*res);

    DTRACE ("\n");
    size = vrtc_call_ext (vrtc, VRTC_CMD_READ_ALARM, 0, size);
    DTRACE ("got: size %d res %d value %d\n", size, res->common.res,
	    res->common.value);
    if (size != sizeof (*res) || res->common.res) {
	return -EFAULT;
    }
    alrm->enabled = res->alarm.enabled;
    alrm->pending = res->alarm.pending;

#define VRTC_ASSIGN(x)	alrm->time.x = res->alarm.time.x
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

    return 0;
}

    static int
vrtc_op_set_alarm (struct device* dev, struct rtc_wkalrm* alrm)
{
    vrtc_t*		vrtc = dev_get_drvdata (dev);
    vrtc_req_wkalrm_t*	req = vrtc->vrpc_data;
    vrpc_size_t		size = sizeof (*req);

    DTRACE ("\n");
    req->alarm.enabled = alrm->enabled;
    req->alarm.pending = alrm->pending;

#define VRTC_ASSIGN(x)	req->alarm.time.x = alrm->time.x
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

    size = vrtc_call_ext (vrtc, VRTC_CMD_SET_ALARM, 0, size);
    DTRACE ("got: size %d res %d value %d\n", size, req->common.vcmd,
	    req->common.arg);
    if (size != sizeof (*req) || req->common.arg) {
	return -EFAULT;
    }
    return 0;
}

    static int
vrtc_op_proc (struct device* dev, struct seq_file* seq)
{
    vrtc_t* vrtc = dev_get_drvdata (dev);
    unsigned i;

    (void) vrtc;
    DTRACE ("\n");
	/* Actually, if non-registered, there will be no proc file */
    seq_printf (seq, "VLX VRTC frontend. RTC %s registered.\n",
		vrtc->rtc ? "is" : "not");
    seq_printf (seq, "RPC buffer 0x%p cxirq %d cxid 0x%x IRQ pshared 0x%lx\n",
		vrtc->vrpc_data, vrtc->cxirq, (int) vrtc->cxid,
		(long) vrtc->pshared);
    seq_printf (seq, "IRQ vshared 0x%p IRQ events 0x%x irq_wake_enabled %d\n",
		vrtc->vshared, vrtc->vshared->events, vrtc->irq_wake_enabled);
    seq_printf (seq, "Calls:");
    for (i = 0; i < VRTC_CMD_MAX; ++i) {
	if (!vrtc->remote_calls [i]) continue;
	seq_printf (seq, " %s:%u", vrtc_cmd_name [i], vrtc->remote_calls [i]);
    }
    seq_printf (seq, "\nirqs %u Update %u Alarm %u Periodic %u\n",
		vrtc->irqs_total, vrtc->irqs_split [0], vrtc->irqs_split [1],
		vrtc->irqs_split [2]);
    return 0;
}


#define VRTC_ASSIGN(x)	.x = vrtc_op_##x

    static const struct rtc_class_ops
vrtc_ops = {
    VRTC_ASSIGN (open),
    VRTC_ASSIGN (release),
    VRTC_ASSIGN (ioctl),
    VRTC_ASSIGN (read_time),
    VRTC_ASSIGN (set_time),
    VRTC_ASSIGN (read_alarm),
    VRTC_ASSIGN (set_alarm),
    VRTC_ASSIGN (proc),
    .set_mmss = NULL,
    .read_callback = NULL
};

#undef VRTC_ASSIGN

/*----- Power Management -----*/

#ifdef CONFIG_PM
    static int
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
vrtc_pm_suspend (struct device* dev)
{
#else
vrtc_pm_suspend (struct platform_device* pdev, pm_message_t state)
{
    struct device* dev = &pdev->dev;
#endif
    vrtc_t* vrtc = dev_get_drvdata (dev);
    int diag = 0;

    DTRACE ("may_wakeup %d irq_wake_enabled %d\n", device_may_wakeup (dev),
	    vrtc->irq_wake_enabled);
    if (device_may_wakeup (dev)) {
	diag = enable_irq_wake (vrtc->cxirq);
	if (diag) {
	    ETRACE ("vrtc_pm_suspend: enable_irq_wake(%d) failed (%d)\n",
		    vrtc->cxirq, diag);
	} else {
	    ++vrtc->irq_wake_enabled;
	}
    }
    return diag;
}

    static int
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
vrtc_pm_resume (struct device* dev)
{
#else
vrtc_pm_resume (struct platform_device* pdev)
{
    struct device* dev = &pdev->dev;
#endif
    vrtc_t* vrtc = dev_get_drvdata (dev);
    int diag = 0;

    DTRACE ("may_wakeup %d irq_wake_enabled %d\n", device_may_wakeup (dev),
	    vrtc->irq_wake_enabled);
	/*
	 * We had a situation where Linux has complained that we
	 * called disable_irq_wake() (an inline for set_irq_wake(!on))
	 * while irq_desc::wake_depth was already zero. So now we
	 * only call disable_irq_wake() if we previously called
	 * enable_irq_wake().
	 */
    if (device_may_wakeup (dev) && vrtc->irq_wake_enabled > 0) {
	diag = disable_irq_wake (vrtc->cxirq);
	if (diag) {
	    ETRACE ("vrtc_pm_resume: disable_irq_wake(%d) failed (%d)\n",
		    vrtc->cxirq, diag);
	} else {
	    --vrtc->irq_wake_enabled;
	}
    }
    return diag;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    static struct dev_pm_ops
vrtc_pm_ops = {
    .suspend	= vrtc_pm_suspend,
    .resume	= vrtc_pm_resume
};
#endif
#endif	/* CONFIG_PM */

/*----- Initialization and exit entry points -----*/

    /* Not exported */

    static void
vrtc_release (vrtc_t* vrtc)
{
    DTRACE ("\n");
    nkops.nk_xirq_detach (vrtc->cxid);
    vrpc_close (vrtc->vrpc);
    vrpc_release (vrtc->vrpc);
    kfree (vrtc);
}

    static void
vrtc_ready (void* cookie)
{
    vrtc_t*	vrtc = (vrtc_t*) cookie;
    int		ret;

    DTRACE ("\n");
    vrtc->rtc = rtc_device_register (VRTC_DEVICE_NAME, &vrtc->pdev->dev,
				     &vrtc_ops, THIS_MODULE);
    ret = PTR_ERR (vrtc->rtc);
    if (IS_ERR (vrtc->rtc)) {
	dev_err (&vrtc->pdev->dev, "Failed to register RTC device (%d)\n",
		 ret);
	vrtc_release (vrtc);
	return;
    }
    device_init_wakeup (&vrtc->pdev->dev, 1);
    TRACE ("Device registered.\n");
}

    static int __init
vrtc_probe (struct platform_device* pdev)
{
    struct vrpc_t* vrpc = vrpc_client_lookup (VRTC_VRPC_NAME, 0);
    vrtc_t*	vrtc;
    int ret;

    if (!vrpc) {
	ETRACE ("No vrpc link.\n");
	return -ENODEV;
    }
    DTRACE ("Have vrpc link.\n");
    vrtc = kzalloc (sizeof (vrtc_t), GFP_KERNEL);
    if (!vrtc) {
	ETRACE ("Out of memory.\n");
	return -ENOMEM;
    }
    vrtc->pdev      = pdev;
    vrtc->vrpc      = vrpc;
    vrtc->vrpc_data = vrpc_data (vrpc);

    if (vrpc_maxsize (vrpc) < sizeof (vrtc_ipc_t)) {
	ETRACE ("vrpc_maxsize() too small.\n");
	ret = -EINVAL;
	goto error;
    }
    platform_set_drvdata (pdev, vrtc);

    vrtc->pshared = nkops.nk_pmem_alloc (vrpc_plink (vrtc->vrpc),
					 VRPC_PMEM_BASE,
					 sizeof (vrtc_shared_t));
    if (!vrtc->pshared) {
	ETRACE ("Cannot allocate %d bytes of pmem.\n", sizeof (vrtc_shared_t));
	ret = -ENOMEM;
	goto error;
    }
    vrtc->vshared  = (vrtc_shared_t*)
	nkops.nk_mem_map (vrtc->pshared, sizeof (vrtc_shared_t));
    if (!vrtc->vshared) {
	ETRACE ("Could not map shared pmem.\n");
	ret = -ENOMEM;
	goto error;
    }
    vrtc->cxirq = nkops.nk_pxirq_alloc (vrpc_plink (vrtc->vrpc),
					VRPC_PXIRQ_BASE,
					vrpc_vlink (vrtc->vrpc)->c_id, 1);
    if (!vrtc->cxirq) {
	ETRACE ("Could not allocate xirq.\n");
	ret = -ENOMEM;
	goto error;
    }
    vrtc->cxid = nkops.nk_xirq_attach (vrtc->cxirq, vrtc_cxirq_handler, vrtc);
    if (!vrtc->cxid) {
	ETRACE ("Could not attach xirq.\n");
	ret = -ENOMEM;
	goto error;
    }
    ret = vrpc_client_open (vrpc, vrtc_ready, vrtc);
    if (ret) {
	ETRACE ("Could not open VRPC client (%d).\n", ret);
	goto error_detach;
    }
    DTRACE ("Probe ok, cxirq %d\n", vrtc->cxirq);
    return 0;

error_detach:
    nkops.nk_xirq_detach (vrtc->cxid);
error:
    kfree (vrtc);
    return ret;
}

    /* Callback from Linux */

    static int __exit
vrtc_remove (struct platform_device* pdev)
{
    vrtc_t* vrtc = platform_get_drvdata (pdev);

    DTRACE ("\n");
    if (!vrtc) return 0;
    rtc_device_unregister (vrtc->rtc);
    platform_set_drvdata (pdev, 0);
    vrtc_release (vrtc);
    return 0;
}

    static struct platform_driver
vrtc_driver = {
    .probe	= NULL,	/* vrtc_probe */
    .remove	= __exit_p (vrtc_remove),
    .shutdown	= NULL,
#ifdef CONFIG_PM
#if LINUX_VERSION_CODE <= KERNEL_VERSION (2,6,23)
    .suspend	= vrtc_pm_suspend,
    .resume	= vrtc_pm_resume,
#endif
#endif
    .driver = {
	.name	= VRTC_DEVICE_NAME,
#ifdef CONFIG_PM
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
	.pm	= &vrtc_pm_ops,
#endif
#endif
    }
};

    static void
vrtc_dev_release (struct device* dev)
{
    (void) dev;
    DTRACE ("\n");
}

    static struct platform_device
vrtc_device = {
    .name	= VRTC_DEVICE_NAME,
    .id		= 1,
    .dev	= {
	.release = vrtc_dev_release
    }
};

    static int __init
vrtc_init (void)
{
    int diag;

    DTRACE ("\n");
    diag = platform_device_register (&vrtc_device);
    if (diag) {
	ETRACE ("platform_device_register() failed (%d).\n", diag);
	return diag;
    }
	/* Alternatively: platform_driver_register() */
    diag = platform_driver_probe (&vrtc_driver, vrtc_probe);
    if (diag) {
	ETRACE ("platform_driver_probe() failed (%d).\n", diag);
	platform_device_unregister (&vrtc_device);	/* void */
	return diag;
    }
    TRACE ("Initialized.\n");
    return diag;
}

    static void __exit
vrtc_exit (void)
{
    DTRACE ("\n");
    platform_driver_unregister (&vrtc_driver);	/* void */
    platform_device_unregister (&vrtc_device);	/* void */
    TRACE ("Module unloaded.\n");
}

module_init (vrtc_init);
module_exit (vrtc_exit);

/*----- Module description -----*/

MODULE_AUTHOR      ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_DESCRIPTION ("VLX Real Time Clock driver (RTC)");
MODULE_LICENSE     ("GPL");
MODULE_ALIAS       ("platform:vrtc");

/*----- End of file -----*/

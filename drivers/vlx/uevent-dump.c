/*
 ****************************************************************
 *
 *  Component: VLX uevent dumping driver
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
#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/platform_device.h>
#include <nk/nkern.h>

/*----- VLX services driver library -----*/

#define VLX_SERVICES_THREADS
#define VLX_SERVICES_UEVENT
#include "vlx-services.c"

/*----- Local configuration -----*/

#if 0
#define UED_DEBUG
#endif

#if 0
#define UED_FULL
#endif

#if 0
#define UED_NETLINK
#endif

/*----- Tracing -----*/

#define TRACE(_f, _a...)	printk (KERN_INFO "UED: " _f , ## _a)
#define ETRACE(_f, _a...)	printk (KERN_ERR  "UED: Error: " _f , ## _a)

#ifdef UED_DEBUG
#define DTRACE(_f, _a...)	printk (KERN_ALERT "%s: " _f, __func__, ## _a)
#else
#define DTRACE(_f, _a...)
#endif

/*----- Data types -----*/

typedef struct {
    _Bool			thread_abort;
    vlx_thread_t		thread_desc;
    vlx_uevent_t		uevent;
    char			buf [1024];
    wait_queue_head_t		wait_queue_head;
#ifdef UED_NETLINK
    struct notifier_block	netlink_notifier;
#endif
    struct notifier_block	platform_notifier;
} ued_t;

/*----- Functions -----*/

#ifdef UED_NETLINK
    static int
ued_netlink_notify (struct notifier_block* nb, unsigned long event, void* ptr)
{
    TRACE ("netlink: event %ld ptr %p\n", event, ptr);
#ifdef UED_DEBUG
    dump_stack();
#endif
    return NOTIFY_DONE;
}
#endif

    static int
ued_uevent_recv (ued_t* ued)
{
    int	diag;

    diag = vlx_uevent_recv (&ued->uevent, ued->buf, sizeof ued->buf);
    if (diag < 0) return diag;
    TRACE ("msg: '%s' (%d bytes)\n", ued->buf, diag);
#ifdef UED_FULL
    {
	    /* Print the environment */
	char*	ch = ued->buf;
	size_t	len;

	while (diag > (len = strlen (ch) + 1)) {
	    diag -= len;
	    ch   += len;
	    if (!*ch) break;
	    TRACE ("env: '%s'\n", ch);
	}
    }
#endif
    return diag;
}

    static int
ued_thread (void* arg)
{
    ued_t*		ued = (ued_t*) arg;
    DECLARE_WAITQUEUE	(wait, current);
    _Bool		had_msg = 0;

    add_wait_queue (&ued->wait_queue_head, &wait);
    while (!ued->thread_abort) {
	int diag;

	set_current_state (TASK_INTERRUPTIBLE);
	if ((diag = ued_uevent_recv (ued)) < 0) {
	    if (diag == -EAGAIN) {
		if (had_msg) {
		    had_msg = 0;
		    DTRACE ("no more msgs\n");
		}
		schedule();
	    } else {
		    /* Only ever saw -ENOBUFS (-105) */
		DTRACE ("uevent_recv failed (%d)\n", diag);
		schedule_timeout (HZ);
	    }
	} else {
	    had_msg = 1;
	}
    }
    remove_wait_queue (&ued->wait_queue_head, &wait);
    set_current_state (TASK_RUNNING);
    return 0;
}

    static void
ued_uevent_data_ready (vlx_uevent_t* ue, int bytes)
{
    ued_t* ued = container_of (ue, ued_t, uevent);

    (void) bytes;
    wake_up_interruptible (&ued->wait_queue_head);
}

    static int
ued_platform_notify (struct notifier_block* nb, unsigned long action,
		     void* data)
{
    struct device*		dev = data;
    struct platform_device*	pdev = to_platform_device (dev);
    const char*			string;

    switch (action) {
    case BUS_NOTIFY_ADD_DEVICE:    string = "add-device";    break;
    case BUS_NOTIFY_DEL_DEVICE:    string = "del-device";    break;
    case BUS_NOTIFY_BOUND_DRIVER:  string = "bound-driver";  break;
    case BUS_NOTIFY_UNBIND_DRIVER: string = "unbind-driver"; break;
    default:                       string = "?";             break;
    }
    TRACE ("platform: %s '%s'\n", string, pdev->name);
    return 0;
}

/*----- Initialization and termination -----*/

    static void
ued_cleanup (ued_t* ued)
{
	/* Wake up the thread */
    ued->thread_abort = 1;
    wake_up_interruptible (&ued->wait_queue_head);
    vlx_thread_join (&ued->thread_desc);

    vlx_uevent_exit (&ued->uevent);
    if (ued->platform_notifier.notifier_call) {
	bus_unregister_notifier (&platform_bus_type, &ued->platform_notifier);
    }
#ifdef UED_NETLINK
    if (ued->netlink_notifier.notifier_call) {
	netlink_unregister_notifier (&ued->netlink_notifier);
    }
#endif
}

    static int __init
ued_init (ued_t* ued)
{
    signed	diag;

    init_waitqueue_head (&ued->wait_queue_head);

#ifdef UED_NETLINK
    ued->netlink_notifier.notifier_call  = ued_netlink_notify;
    diag = netlink_register_notifier (&ued->netlink_notifier);
    if (diag) {
	ETRACE ("netlink_register_notifier() failed (%d)\n", diag);
	ued->netlink_notifier.notifier_call = 0;
	goto error;
    }
#endif
    ued->platform_notifier.notifier_call = ued_platform_notify;
    diag = bus_register_notifier (&platform_bus_type, &ued->platform_notifier);
    if (diag) {
	ETRACE ("bus_register_notifier(platform) failed (%d)\n", diag);
	ued->platform_notifier.notifier_call = 0;
	goto error;
    }
    diag = vlx_uevent_init (&ued->uevent, ued_uevent_data_ready);
    if (diag) {
	ETRACE ("vlx_uevent_init() failed (%d)\n", diag);
	goto error;
    }
    diag = vlx_thread_start_ex (&ued->thread_desc, ued_thread,
				ued, "uevent-dump", 1 /*will_suicide*/);
    if (diag) {
	ETRACE ("thread start failed (%d)\n", diag);
	goto error;
    }
    TRACE ("loaded\n");
    return 0;

error:
    ued_cleanup (ued);
    return diag;
}

/*----- Globals and module glue -----*/

static ued_t	ued;

    static int __init
ued_module_init (void)
{
    return ued_init (&ued);
}

    static void __exit
ued_module_exit (void)
{
    ued_cleanup (&ued);
    TRACE ("unloaded\n");
}

module_init (ued_module_init);
module_exit (ued_module_exit);

/*----- Module description -----*/

MODULE_DESCRIPTION ("VLX uevent dumping driver");
MODULE_AUTHOR ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_LICENSE ("GPL");

/*----- End of file -----*/

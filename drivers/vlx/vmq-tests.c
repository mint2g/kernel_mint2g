/*
 ****************************************************************
 *
 *  Component: VLX VMQ tests
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

    /*
     * This test checks that even if the backend suddenly
     * stops processing and freeing messages, the frontend
     * will still be able to re-use all messages previously
     * freed by the backend.
     * It also checks that freeing a message in backend
     * will unblock an allocating thread in frontend.
     *
     * Command line:
     *  (+2) vdev=(vmqtests,0|be) (+3) vdev=(vmqtests,0|fe)
     */

/*----- System header files -----*/

#include <linux/module.h>	/* __exit, __init */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,7)
    /* This include exists in 2.6.6 but functions are not yet exported */
#include <linux/kthread.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
#include <linux/semaphore.h>
#endif
#include <linux/init.h>		/* module_init() in 2.6.0 and before */
#include <linux/slab.h>
#include <nk/nkern.h>
#include "vlx-vmq.h"

/*----- Local configuration -----*/

#if 0
#define VMQT_DEBUG
#endif

#define VMQT_MSG_COUNT	4

/*----- Tracing -----*/

#ifdef VMQT_DEBUG
#define DTRACE(x...)	do {printk ("(%d) %s: ", current->tgid, __func__);\
			    printk (x);} while (0)
#else
#define DTRACE(x...)
#endif

#define TRACE(x...)	printk (KERN_CRIT    "VMQT: " x)
#define WTRACE(x...)	printk (KERN_WARNING "VMQT: " x)
#define ETRACE(x...)	printk (KERN_ERR     "VMQT: " x)

/*----- Data types -----*/

typedef struct {
    nku32_f	dummy;
} vmqt_req_t;

typedef struct vmqt_link_t {
    vmq_xx_config_t	xx_config;
} vmqt_link_t;

#define VMQT_LINK(link) \
	(*(vmqt_link_t**) &((vmq_link_public_t*) (link))->priv)

/*----- Data -----*/

static struct semaphore	vmqt_sem;
static struct semaphore	vmqt_be_sem;
static _Bool		vmqt_service_thread_aborted;
static _Bool		vmqt_sysconf;
static unsigned		vmqt_counter;
static void*		vmqt_nonfreed_msg;
static vmq_links_t*	vmqt_links_fe;
static vmq_links_t*	vmqt_links_be;

/*----- Routines -----*/

    static void
vmqt_service_thread_aborted_notify (void)
{
    DTRACE ("\n");
    vmqt_service_thread_aborted = true;
    up (&vmqt_sem);
}

    static void
vmqt_cb_sysconf_notify (vmq_links_t* links)
{
    DTRACE ("\n");
    (void) links;
    vmqt_sysconf = true;
    up (&vmqt_sem);
}

    /* Executes in interrupt context */

    static void
vmqt_cb_receive_notify (vmq_link_t* link2)
{
    void*  msg;

    DTRACE ("link %d\n", vmq_peer_osid (link2));
    if (vmqt_counter >= VMQT_MSG_COUNT) {
	TRACE ("Ignoring message(s)\n");
	return;
    }
    while (!vmq_msg_receive (link2, &msg)) {
	if (++vmqt_counter >= VMQT_MSG_COUNT) {
	    TRACE ("Not freeing message %u (%p)\n", vmqt_counter, msg);
	    vmqt_nonfreed_msg = msg;
	    up (&vmqt_be_sem);
	    break;
	}
	vmq_msg_free (link2, msg);
    }
}

    static int
vmqt_service_thread (void* unused)
{
    (void) unused;
    DTRACE ("starting\n");
    while (!vmqt_service_thread_aborted) {
	down (&vmqt_sem);
	DTRACE ("%s %s\n",
		vmqt_service_thread_aborted ? "service-aborted" : "",
		vmqt_sysconf                ? "sysconf"         : "");
	if (vmqt_service_thread_aborted) break;
	if (vmqt_sysconf) {
	    vmqt_sysconf = false;
	    if (vmqt_links_fe) {
		vmq_links_sysconf (vmqt_links_fe);
	    }
	    if (vmqt_links_be) {
		vmq_links_sysconf (vmqt_links_be);
	    }
	}
    }
    return 0;
}

    static _Bool
vmqt_fe_one_link (vmq_link_t* link2, void* cookie)
{
    int loops = VMQT_MSG_COUNT * 2;
    void* msg;
    int diag;

    (void) cookie;
    DTRACE ("link %d\n", vmq_peer_osid (link2));
    while (loops-- > 0) {
	TRACE ("Allocating... ");
	    /* Can sleep for place in FIFO */
	diag = vmq_msg_allocate (link2, 0, &msg, 0);
	if (diag) {
	    ETRACE ("Failed to allocate message\n");
	    return true;
	}
	printk ("done\n");
	vmq_msg_send (link2, msg);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout (3*HZ);
    }
    TRACE ("Test succeeded\n");
    return true;	/* break processing */
}

    static int
vmqt_fe_thread (void* arg)
{
    vmq_links_t* links = (vmq_links_t*) arg;

    DTRACE ("starting\n");
    vmq_links_iterate (links, vmqt_fe_one_link, NULL);
    DTRACE ("terminating\n");
    return 0;
}

    static _Bool
vmqt_be_one_link (vmq_link_t* link2, void* cookie)
{
    (void) cookie;
    DTRACE ("link %d\n", vmq_peer_osid (link2));
    down (&vmqt_be_sem);

    DTRACE ("sleeping now\n");
	/* Wake up after 10s to free message and unblock sender */
    set_current_state (TASK_INTERRUPTIBLE);
    schedule_timeout (10*HZ);

    DTRACE ("freeing\n");
    vmq_msg_free (link2, vmqt_nonfreed_msg);
    vmqt_nonfreed_msg = NULL;
    return true;	/* break processing */
}

    static int
vmqt_be_thread (void* arg)
{
    vmq_links_t* links = (vmq_links_t*) arg;

    DTRACE ("starting\n");
    vmq_links_iterate (links, vmqt_be_one_link, NULL);
    DTRACE ("terminating\n");
    return 0;
}

/*----- Initialization and exit entry points -----*/

#define VLX_SERVICES_THREADS
#include "vlx-services.c"

static vlx_thread_t		vmqt_service_thread_desc;
static vlx_thread_t		vmqt_fe_thread_desc;
static vlx_thread_t		vmqt_be_thread_desc;

    static _Bool
vmqt_free_link (vmq_link_t* link2, void* cookie)
{
    (void) cookie;
    kfree (VMQT_LINK (link2));
    return false;
}

    static void
vmqt_exit (void)
{
    DTRACE ("\n");
    if (vmqt_links_fe) {
	vmq_links_abort (vmqt_links_fe);
    }
    if (vmqt_links_be) {
	vmq_links_abort (vmqt_links_be);
    }
    vmqt_service_thread_aborted_notify();
    vlx_thread_join (&vmqt_service_thread_desc);
    if (vmqt_links_fe) {
	vlx_thread_join (&vmqt_fe_thread_desc);
	vmq_links_iterate (vmqt_links_fe, vmqt_free_link, NULL);
	vmq_links_finish (vmqt_links_fe);
	vmqt_links_fe = NULL;
    }
    if (vmqt_links_be) {
	vlx_thread_join (&vmqt_be_thread_desc);
	vmq_links_iterate (vmqt_links_be, vmqt_free_link, NULL);
	vmq_links_finish (vmqt_links_be);
	vmqt_links_be = NULL;
    }
}

    static _Bool
vmqt_init_link (vmq_link_t* link2, void* cookie)
{
    vmqt_link_t* xe_link = (vmqt_link_t*) kzalloc (sizeof *xe_link, GFP_KERNEL);

    if (!xe_link) {
	*(int*) cookie = -ENOMEM;
	return true;
    }
    VMQT_LINK (link2) = xe_link;
    return false;
}

    /*
     * Each link can have a custom communications configuration,
     * depending on the vlink parameter indicated by the "start"
     * string. To store the resulting various vmq_xx_config_t we
     * use the private data of the vmq_link_t (vmqt_link_t), which
     * must therefore be allocated here.
     */

    static const vmq_xx_config_t*
vmqt_cb_get_xx_config (vmq_link_t* link2, const char* start)
{
    vmqt_link_t* vmqt = (vmqt_link_t*) kzalloc (sizeof *vmqt, GFP_KERNEL);

    if (!vmqt) {
	ETRACE ("out of memory for link\n");
	return NULL;
    }
    VMQT_LINK (link2) = vmqt;
	/*
	 * vdev=(vmqtests,<linkid>|[be|fe])
	 *
	 * Backend  gets start == rx_s_info.
	 * Frontend gets start == tx_s_info.
	 * So the frontend does not see the "be" component
	 * in the "start" configuration parameter.
	 * Instead, it must look into vmq_link_rx_s_info(link)
	 * string to find it.
	 */
    if (!start || start == vmq_link_tx_s_info (link2)) {
	const char* rx_s_info = vmq_link_rx_s_info (link2);

	DTRACE ("frontend start %p (%s) rx_s_info %p (%s)\n",
		start,     start     ? start     : "NULL",
		rx_s_info, rx_s_info ? rx_s_info : "NULL");
	if (rx_s_info && strstr (rx_s_info, "be")) {
	    DTRACE ("ignoring vlink\n");
	    return VMQ_XX_CONFIG_IGNORE_VLINK;
	}
    }
    if (!start || start == vmq_link_rx_s_info (link2)) {
	DTRACE ("frontend start %p (%s))\n",
		start, start ? start : "NULL");
	if (start && strstr (start, "fe")) {
	    DTRACE ("ignoring vlink\n");
	    return VMQ_XX_CONFIG_IGNORE_VLINK;
	}
    }
    vmqt->xx_config.msg_count = VMQT_MSG_COUNT;
    vmqt->xx_config.msg_max   = sizeof (vmqt_req_t);
    return &vmqt->xx_config;
}

#define VMQT_FIELD(name,value)	value

    static const vmq_callbacks_t
vmqt_callbacks_fe = {
    VMQT_FIELD (link_on,		NULL),
    VMQT_FIELD (link_off,		NULL),
    VMQT_FIELD (link_off_completed,	NULL),
    VMQT_FIELD (sysconf_notify,		vmqt_cb_sysconf_notify),
    VMQT_FIELD (receive_notify,		NULL),
    VMQT_FIELD (return_notify,		NULL),
    VMQT_FIELD (get_tx_config,		vmqt_cb_get_xx_config),
    VMQT_FIELD (get_rx_config,		NULL)
};

    static const vmq_callbacks_t
vmqt_callbacks_be = {
    VMQT_FIELD (link_on,		NULL),
    VMQT_FIELD (link_off,		NULL),
    VMQT_FIELD (link_off_completed,	NULL),
    VMQT_FIELD (sysconf_notify,		vmqt_cb_sysconf_notify),
    VMQT_FIELD (receive_notify,		vmqt_cb_receive_notify),
    VMQT_FIELD (return_notify,		NULL),
    VMQT_FIELD (get_tx_config,		NULL),
    VMQT_FIELD (get_rx_config,		vmqt_cb_get_xx_config)
};

    static const vmq_xx_config_t
vmqt_xx_config = {
    VMQT_FIELD (msg_count,	1),
    VMQT_FIELD (msg_max,	1),
    VMQT_FIELD (data_count,	0),
    VMQT_FIELD (data_max,	0)
};

#undef VMQT_FIELD

    static int __init
vmqt_init (void)
{
    signed diag;

    DTRACE ("\n");
    sema_init (&vmqt_sem, 0);	/* Before it is signaled */
    sema_init (&vmqt_be_sem, 0);	/* Before it is signaled */

    diag = vmq_links_init_ex (&vmqt_links_fe, "vmqtests", &vmqt_callbacks_fe,
			      NULL, &vmqt_xx_config, NULL, true);
    if (diag) goto error;
    diag = vmq_links_init_ex (&vmqt_links_be, "vmqtests", &vmqt_callbacks_be,
			      &vmqt_xx_config, NULL, NULL, false);
    if (diag) goto error;

    if (vmq_links_iterate (vmqt_links_fe, vmqt_init_link, &diag)) goto error;
    if (vmq_links_iterate (vmqt_links_be, vmqt_init_link, &diag)) goto error;

    diag = vmq_links_start (vmqt_links_fe);
    if (diag) goto error;
    diag = vmq_links_start (vmqt_links_be);
    if (diag) goto error;

    diag = vlx_thread_start (&vmqt_service_thread_desc, vmqt_service_thread,
			     NULL, "vmqt-service");
    if (diag) goto error;
    diag = vlx_thread_start (&vmqt_fe_thread_desc, vmqt_fe_thread,
			     vmqt_links_fe, "vmqt-fe");
    if (diag) goto error;
    diag = vlx_thread_start (&vmqt_be_thread_desc, vmqt_be_thread,
			     vmqt_links_be, "vmqt-be");
    if (diag) goto error;
    TRACE ("initialized\n");
    return 0;

error:
    ETRACE ("init failed (%d)\n", diag);
    vmqt_exit();
    return diag;
}

module_init (vmqt_init);
module_exit (vmqt_exit);

/*----- Module description -----*/

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_DESCRIPTION ("VLX VMQ tests driver");

/*----- End of file -----*/

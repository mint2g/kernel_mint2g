/*
 ****************************************************************
 *
 *  Component: VLX virtual watchdog driver
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
 *    Pascal Piovesan (pascal.piovesan@redbend.com)
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *    Chi Dat Truong (chidat.truong@redbend.com)
 *
 ****************************************************************
 */

#include <linux/version.h>   /* LINUX_VERSION_CODE */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/interrupt.h>    // tasklet ...
#include <linux/slab.h>
#include <nk/nkern.h>

MODULE_DESCRIPTION("VLX Virtual Watchdog driver - secondary backend");
MODULE_AUTHOR("Pascal Piovesan <pascal.piovesan@redbend.com>,\n\t"
	      "Vladimir Grouzdev <vladimir.grouzdev@redbend.com>,\n\t"
	      "Chi Dat Truong <chidat.truong@redbend.com>");
MODULE_LICENSE("GPL");

/*
 * loading parameters
 */
static int nk_wdt_nr = -1;
static unsigned long restart_policy[NK_OS_LIMIT];
static char* nk_wdt_policy[NK_OS_LIMIT];

#ifndef CONFIG_NKERNEL_WDT_COUNT
#define CONFIG_NKERNEL_WDT_COUNT 1
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
MODULE_PARM(nk_dwt_nr,"i");
#else
module_param(nk_wdt_nr, int, 0444);
#endif

MODULE_PARM_DESC(nk_wdt_nr, " integer\n\t\t"
	"  number of virtual watchdog timer devices ("
	__MODULE_STRING(CONFIG_NKERNEL_WDT_COUNT) ")");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
MODULE_PARM(nk_wdt_policy, "1-" __MODULE_STRING(NK_OS_LIMIT) "s");
#else
module_param_array(nk_wdt_policy, charp, NULL, 0444);
#endif

MODULE_PARM_DESC(nk_wdt_policy, " policy string for the watchdog\n\t\t"
	"  Watchdog back-end driver policy :\n\t\t"
	"   \"nk_wdt_policy=<osid>:<restart>,...\"\n\t\t"
	"  where:\n\t\t"
	"   <osid>    is OS identity number in [0.." __MODULE_STRING(NK_OS_LIMIT) "]"
	"   <restart> is restart time number of the corresponding OS\n\t\t");

#define PFX		"NKWDT: "

#define NKWDT_ASSERT(x) \
	if (!(x)) { \
		panic("NKWDT: assertion failed at " __FILE__ " line %d\n", __LINE__); \
	}

#if 0
#define DTRACE(fmt, args...)  \
	printk(KERN_CRIT PFX "%s(%d): " fmt,  \
	__FUNCTION__ , __LINE__ , ## args);
#else
#define DTRACE(fmt, args...)
#endif

/*
 * This pseudo-driver provides virtual WDT device for secondary kernels
 * running on top of the nano-kernel. It is running in the primary/secondary 
 * (Linux) kernel and it uses kernel timeout services to restart secondary 
 * kernels which doesn't pat the watchdog during the watchdog period.
 *
 * The driver code is mostly executing by Linux tasklets, except for timeout
 * and cross IRQ handlers (invoked by the timer and the NKDDI drivers
 * respectively). Such (interrupt) functions are named with one leading
 * dash "_" (e.g., _wdt_interrupt()).
 */

/*
 * Virtual Wdt instance descriptor
 */
typedef struct Wdt {
	struct timer_list       timer;        /* timer */
	unsigned long           period;       /* wdt period in jiffies */
	NkPhAddr                pdev;         /* physical device */
	NkDevWdt*               vwdt;         /* virtualized WDT device header */
	NkOsMask	            enabled;      /* vwdt->enabled image */
	NkXIrqId                cf_xid;       /* config xirq ID */
	unsigned long           cf_pending;   /* config pending flag */
	struct tasklet_struct   tlet_config;  /* device config tasklet */
	struct tasklet_struct   tlet_timeout; /* device timer tasklet */
	struct Wdts*            wdts;         /* back ref. to the driver instance */
	struct Wdt*             next;         /* next WDT device */
} Wdt;

/*
 * Driver instance descriptor
 */
typedef struct Wdts {
	NkDevOps*               ops;            /* NK DDI ops   */
	NkOsMask                running;        /* saved value of running OS mask */
	NkOsId                  lastid;         /* last OS ID */
	NkXIrqId                scf_xid;        /* sys config xirq ID */
	unsigned long           scf_pending;    /* sys config pending flag */
	struct tasklet_struct   tlet_sysconfig; /* sysconfig tasklet */
	Wdt*                    wdt;            /* virtual WDT devices */
} Wdts;

static void  wdt_timer_stop (Wdt* wdt);

static Wdts wdts;	/* there is only one (global) driver instance */

	int
wdt_trigger(NkOsId   osid)
{
	Wdt*       wdt;
	NkOsMask   osmask  = wdts.ops->nk_bit2mask(osid);
	NkVexMask* vex_addr;
	NkVexMask  vex_mask;

	DTRACE("osid %d (" __DATE__ " " __TIME__ ")\n", osid);

	wdt = wdts.wdt;
	while (wdt) {
		if (wdt->vwdt->enabled & osmask) {
			break;
		}
		wdt = wdt->next;
	}

	if (!wdt) {
		DTRACE("No wdt found\n");
		return 0;
	}

	wdt->vwdt->enabled &= ~osmask;
	wdt->enabled &= ~osmask;
	wdt_timer_stop(wdt);
	vex_mask = wdt->vwdt->vex_mask[osid];

	if (vex_mask) {
		DTRACE("vex_mask 0x%x\n", vex_mask);
		vex_addr = (NkVexMask*)wdts.ops->nk_ptov(wdt->vwdt->vex_addr[osid]);
		*vex_addr |= vex_mask;
	} else {
		if (restart_policy[osid] != 0) {
			DTRACE("Restart OS %d [restart_policy=%u]\n", osid, restart_policy[osid]);
			restart_policy[osid]--;
#if defined(__i386__)
			nkctx->p.restart(nkctx, osid); /* Should be generic */
#elif defined(__arm__)
	    	os_ctx->restart(os_ctx, osid);
#else
			printk("Unsupported architecture: cannot restart OS\n");
#endif
		}
		else {
			DTRACE("Stop OS %d [restart_policy=%u]\n", osid, restart_policy[osid]);
#if defined(__i386__)
			nkctx->p.stop(nkctx, osid); /* Should be generic */
#elif defined(__arm__)
	    	os_ctx->stop(os_ctx, osid);
#else
			printk("Unsupported architecture, cannot stop OS\n");
#endif
		}
	}
	return 1;
}

	static void
_wdt_interrupt (unsigned long cookie)
{
	Wdt* wdt         = (Wdt*)cookie;
	NkOsMask enabled = wdt->vwdt->enabled;
	DTRACE("\n");
	while (enabled) {
		NkOsId   osid   = wdts.ops->nk_mask2bit(enabled);
		NkOsMask osmask = wdts.ops->nk_bit2mask(osid);
		NkOsMask ospat  = wdt->vwdt->pat & osmask;
		if (!ospat) {
			wdt_trigger(osid);
		}
		wdt->vwdt->pat &= ~osmask;
		enabled &= ~osmask;
	}
}

	static void
_wdt_timeout (unsigned long token)
{
	Wdt* wdt = (Wdt*)token;
	wdt->timer.expires = jiffies + wdt->period;
	add_timer(&(wdt->timer));
	tasklet_schedule(&wdt->tlet_timeout);
}

/*
 * Compute period.
 */
	static void
wdt_timer_period (Wdt* wdt)
{
	int         quantum_ms = 0;

#if defined(__i386__)
	NkOsCtx*    ctx     = nkctx;
	NkCpuDesc*  cinfo;
	int         per_ms;

	/*
	 * Provides time counter value corresponding to 1 milli-second
	 */
	cinfo  = ((NkCpuInfo*)nkops.nk_ptov(nkctx->cpu_info))->cpus + nkctx->cpuid;
	per_ms = cinfo->khz;

	do {
		ctx   = (NkOsCtx*)nkops.nk_ptov(ctx->next);
		if (ctx->id != 1) {
		quantum_ms += (int)(ctx->quantum)/per_ms;
		}
	} while (ctx != nkctx);
#endif

	wdt->period = ((wdt->vwdt->period + quantum_ms) * HZ)/1000;
	if (!wdt->period) {
		wdt->period = HZ;
	}
	DTRACE("Period: %d\n", wdt->period);
}

/*
 * Start Timer device in periodic mode.
 */
	static void
wdt_timer_start (Wdt* wdt)
{
	DTRACE("Enter\n");
	wdt_timer_period(wdt);

	wdt->timer.data     = (unsigned long)wdt;
	wdt->timer.function = _wdt_timeout;
	wdt->timer.expires  = jiffies + wdt->period;
	tasklet_init(&(wdt->tlet_timeout), _wdt_interrupt, (unsigned long)wdt);
	add_timer(&(wdt->timer));
}

/*
 * Stop Timer device.
 */
	static void
wdt_timer_stop (Wdt* wdt)
{
	NkOsMask enabled = wdt->vwdt->enabled;

	if (!enabled) {
		DTRACE("Deleting\n");
		del_timer(&(wdt->timer));
		wdt->period = 0;
	}
	else DTRACE("Doing nothing\n");
}

/*
 * This function handles the reconfiguration request 
 * (i.e., WDT enable/disable) sent by a secondary kernel.
 */
	static void
wdt_config (unsigned long cookie)
{
	Wdt* wdt         = (Wdt*)cookie;
	NkOsMask enabled = wdt->vwdt->enabled;

	wdt->cf_pending = 0;	/* reset pending flag */

	if (wdt->enabled != enabled) {
		DTRACE("wdt->enabled 0x%x != enabled 0x%x\n", wdt->enabled, enabled);
		if (wdt->enabled) {
			if (!enabled) {
			wdt_timer_stop(wdt);	/* stop Timer */
			}
		} else {
			if (enabled) {
				wdt_timer_start(wdt);	/* start Timer */
			}
		}
		wdt->enabled = enabled;
	}
	else DTRACE("wdt->enabled == enabled\n");
}

/*
 * This handler is called by the NKDDI driver when a re-configuration
 * cross interrupt is posted by a secondary kernel.
 * <<< called in the interrupt context >>>
 */
	static void
_wdt_config (void* cookie, NkXIrq xirq)
{
	Wdt* wdt = (Wdt*)cookie;
	if (!wdt->cf_pending) { /* avoid multiple invocations for the same token */
		DTRACE("XIRQ trigger %d  (0x%x)\n", xirq, wdt);
		wdt->cf_pending = 1;
		tasklet_schedule(&(wdt->tlet_config));
	}
	else {
		DTRACE("No XIRQ trigger\n");
	}
}

/*
 * Shutdown virtual WDT device instance...
 */
	static void
wdt_down (Wdt* wdt)
{
	if (wdt->cf_xid) {
		wdts.ops->nk_xirq_detach(wdt->cf_xid);
	}

	if (wdt->wdts) {
		Wdt** link = &wdts.wdt;
		Wdt*  curr;
		while ((curr = *link) != wdt) link = &(curr->next);
		*link = wdt->next;
	}
	tasklet_kill(&(wdt->tlet_config));
	tasklet_kill(&(wdt->tlet_timeout));
	kfree(wdt);
}

/*
 * Initialize new virtual WDT instance...
 */
	static NkPhAddr
wdt_lookup (Wdt* wdt)
{
	NkPhAddr   pdev;
	NkDevDesc* vdev;

	pdev = (wdts.wdt ? wdts.wdt->pdev : 0);
	pdev = wdts.ops->nk_dev_lookup_by_type(NK_DEV_ID_WDT, pdev);
	if (!pdev) {
		return 0;
	}

	vdev      = (NkDevDesc*)wdts.ops->nk_ptov(pdev);
	wdt->vwdt = (NkDevWdt*)wdts.ops->nk_ptov(vdev->dev_header);
	wdt->pdev = pdev;

	return pdev;
}

	static NkPhAddr
wdt_create (Wdt* wdt)
{
	NkPhAddr   pdev;
	NkDevDesc* vdev;

	pdev = wdts.ops->nk_dev_alloc(sizeof(NkDevDesc)+sizeof(NkDevWdt));
	if (!pdev) {
		return 0;
	}
	vdev = (NkDevDesc*)wdts.ops->nk_ptov(pdev);

		/*
	* Initialize new virtualized WDT device
	*/
	memset(vdev, 0, sizeof(NkDevDesc)+sizeof(NkDevWdt));

	vdev->class_id   = NK_DEV_CLASS_GEN;
	vdev->dev_id     = NK_DEV_ID_WDT;
	vdev->dev_header = pdev + sizeof(NkDevDesc);
	vdev->dev_owner  = nkops.nk_id_get();

	wdt->vwdt = (NkDevWdt*)wdts.ops->nk_ptov(vdev->dev_header);
	wdt->pdev  = pdev;

	return pdev;
}

	static void
wdt_init (void)
{
	Wdt*       wdt;
	NkPhAddr   pdev;
	NkDevWdt*  vwdt;

	wdt = (Wdt*) kmalloc(sizeof(Wdt), GFP_KERNEL);

	if (!wdt) {
		printk(KERN_CRIT PFX "kmalloc failed\n");
		return;
	}

	memset(wdt, 0, sizeof(Wdt));

	/*
	 * init_timer() clears timer->list.next/prev.
	 * It is not really necessary, because of the bzero()
	 * above, but it has been added to the "timer" driver,
	 * so let's keep sources similar.
	 */
	init_timer(&wdt->timer);

	pdev = 0;
	if (!wdt_lookup(wdt)) {
		pdev = wdt_create(wdt);
		if (!pdev) {
			printk(KERN_CRIT PFX "error -- dev_alloc() failed\n");
			wdt_down(wdt);
			return;
		}
	}
	vwdt = wdt->vwdt;

	/*
	 * Allocate one XIRQ for the the (re-)configuration request.
	 */
	vwdt->xirq = wdts.ops->nk_xirq_alloc(1);
	if (!vwdt->xirq) {
		printk(KERN_CRIT PFX "error -- xirq_alloc() failed\n");
		wdt_down(wdt);
		return;
	}

	wdt->wdts = &wdts;
	wdt->next = wdts.wdt;
	wdts.wdt  = wdt;

	/*
	 * Connect CONFIG cross interrupt handler.
	 */
	wdt->cf_xid = wdts.ops->nk_xirq_attach(vwdt->xirq, _wdt_config, wdt);
	if (!wdt->cf_xid) {
		printk(KERN_CRIT PFX "error -- xirq_attach(%d) failed\n", vwdt->xirq);
		wdt_down(wdt);
		return;
	}

	/* Create a linux kernel tasklet for handling watchdog XIRQ */
	tasklet_init(&wdt->tlet_config, wdt_config, (unsigned long)wdt);

	printk(PFX "time-out based virtual wdt %s\n", (pdev ? "created" : "activated"));

	/*
	 * Make device visible for the secondary kernels...
	 */
	if (pdev) {
		wdts.ops->nk_dev_add(pdev);
	}
	wdt_config((unsigned long)wdt);
}

/*
 * Shutdown the driver.
 */
	static void
wdts_down (void)
{
	tasklet_kill(&wdts.tlet_sysconfig);
	while (wdts.wdt) {
		wdt_down(wdts.wdt);
	}
	memset(&wdts, 0, sizeof(wdts));
}

/*
 * This function handles the system wide reconfiguration request 
 * SYSCONF (i.e., secondary kernel start/stop events) sent by
 * the nano-kernel. 
 * We are actually interested only in crashed kernels. For such
 * kernels we have to disconnect the WDT device.
 */
	static void
wdts_sys_config (unsigned long cookie)
{
	Wdt* wdt;
	DTRACE("wdts.scf_pending %ld\n", wdts.scf_pending);
	wdts.scf_pending = 0;	/* reset pending flag */
	wdt = wdts.wdt;
	while (wdt) {
		wdt_config((unsigned long)wdt);	/* cleanup WDT device */
		wdt = wdt->next;
	}
}

/*
 * This handler is called by the NKDDI driver when a system re-configuration
 * cross interrupt is posted by the nano-kernel.
 * We are actually interested only in crashed kernels. For such
 * kernels we have to disconnect the WDT device.
 * <<< called in the interrupt context >>>
 */
	static void
_wdts_sys_config (void* cookie, NkXIrq xirq)
{
	NkOsMask running;
	NkOsMask down;

	NKWDT_ASSERT (xirq == NK_XIRQ_SYSCONF);

	/*
	 * Calculate the mask of kernels which has been shut
	 * down since the previous SYSCONF event.
	 */
	running      = wdts.ops->nk_running_ids_get();
	down         = (wdts.running ^ running) & ~running;
	DTRACE("running 0x%x wdts.running 0x%x down 0x%x\n", running, wdts.running, down);
	wdts.running = running;

	if (down) {
		Wdt* wdt = wdts.wdt;
		DTRACE("Have down\n");
		while (wdt) {
			/* Disconnect shut down OSes from WDT device */
			wdts.ops->nk_atomic_clear(&(wdt->vwdt->enabled), down);
			wdt = wdt->next;
		}

		if (!wdts.scf_pending) {
			wdts.scf_pending = 1;
			tasklet_schedule(&wdts.tlet_sysconfig);
		}
	}
	else DTRACE(": No down\n");
}

/*
 * Driver initialization.
 */

extern int (*wdt_hook)(NkOsId osid); /* in-kernel NMI watchdog hook */

	static void
wdts_init (void)
{
	int         dev_nr;
	DTRACE("init watchdog driver\n");

	wdts.ops = &nkops;
	wdts.running = wdts.ops->nk_running_ids_get();
	wdts.lastid  = wdts.ops->nk_last_id_get();

	wdts.scf_xid = wdts.ops->nk_xirq_attach(NK_XIRQ_SYSCONF,
	                                        _wdts_sys_config, &wdts);
	if (!wdts.scf_xid) {
		printk(KERN_CRIT PFX "error -- xirq_attach(%d) failed\n",
		       NK_XIRQ_SYSCONF);
		wdts_down();
		return;
	}

	/* Create a linux kernel tasklet for handling sys_config XIRQ */
	tasklet_init(&wdts.tlet_sysconfig, wdts_sys_config, (unsigned long) &wdts);

	printk(PFX "backend driver started (%d devices)\n", nk_wdt_nr);

	/*
	 * Create virtual Wdt devices based on the time-out.
	 */
	dev_nr = nk_wdt_nr;
	while (dev_nr--) {
		wdt_init();
	}

#ifdef CONFIG_NMI_WATCHDOG
    DTRACE("Attaching NMI watchdog\n");
    if (!wdt_hook) {
		wdt_hook = wdt_trigger;
	} else {
		printk(KERN_CRIT PFX "NMI watchdog hook already used\n");
	}
#else
    DTRACE("No NMI watchdog found\n");
#endif
}

/*
 * Shutdown the driver.
 */
	static int
wdts_unload (void)
{
	wdts_down();
	return 0;
}

/*
 * Parse kernel command line for "nk_wdt_nr" option:
 *  nk_wdt_nr=<number_of_devices>
 */
	static int __init
nk_wdt_nr_setup (char* opt)
{
	char*   end;
	int     val;

	val = simple_strtoul(opt, &end, 0);
	if (end == opt) {
		return 0;
	}
	nk_wdt_nr = val;
	return 1;
}

/*
 * Parse kernel command line for "nk_wdt_policy" option:
 *  nk_wdt_policy=<osid_n>:<restart_time_n>,<osid_m>:<restart_time_m>
 */
	static int __init
nk_wdt_restart_time_setup (char* opt)
{
	char*           pos;
	unsigned long   val;
	int             i;
	NkOsId          id;

	for (i = NK_OS_PRIM+1; i <= NK_OS_LIMIT; i++) {
		id = simple_strtoul(opt, &pos, 0);
		if (pos == opt) {
			return 0;
		}
		else {
			opt = pos;
		}
		if ( (id >= (NK_OS_PRIM+1)) && (id < NK_OS_LIMIT) ) {
			if (*opt == ':') {
				opt++;
				val = simple_strtoul(opt, &pos, 0);
				if (pos != opt) {
					DTRACE("found id=%d --- val=%ld\n", id, val);
					restart_policy[id] = val;
					opt = pos;
				}
			}
		}
		// find next couple of value
		pos = strchr(opt, ',');
		if (pos == NULL) {
			break;
		}
		else {
			opt = ++pos;
		}
	}
	return 1;
}

#ifndef	MODULE
__setup("nk_wdt_nr=", nk_wdt_nr_setup);
__setup("nk_wdt_policy=", nk_wdt_restart_time_setup);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
extern char saved_command_line[];
#endif
#endif

	static int
wdt_module_init (void)
{
	// Default policy is set to infinite restart for all OSes
	memset(restart_policy, 0xFF, sizeof(long) * NK_OS_LIMIT);

#ifdef	MODULE
	/*
	 * If no option was given at module load time,
	 * then look for one in the kernel command line ...
	 */
	if (nk_wdt_nr < 0) {
		char*  cmdline = saved_command_line;
		while ((cmdline = strstr(cmdline, "nk_wdt_nr="))) {
			cmdline += 10;
			(void)nk_wdt_nr_setup(cmdline);
		}
	}

	{
		char*  cmdline = saved_command_line;
		char** opt;

		// Parse command line options first ...
		while ((cmdline = strstr(cmdline, "nk_wdt_policy="))) {
			cmdline += 14;
			(void)nk_wdt_restart_time_setup(cmdline);
		}
		// ... then arguments given to insmod
		opt = nk_wdt_policy;
		while (*opt) {
			(void)nk_wdt_restart_time_setup(*opt);
			opt++;
		}
	}
#endif	/* MODULE */

	/*
	 * If no option was found, then fallback to default ...
	 */
	if (nk_wdt_nr < 0) {
		nk_wdt_nr = CONFIG_NKERNEL_WDT_COUNT;
	}

	/*
	 * Initialize the watchdog driver
	 */
	wdts_init();
	return 0;
}

	static void
wdt_module_exit (void)
{
	if (wdts_unload() < 0) {
		printk(KERN_CRIT PFX "error -- module exit failed.\n");
	}
}

module_init(wdt_module_init);
module_exit(wdt_module_exit);

#undef NKWDT_ASSERT

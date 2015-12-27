/*
 ****************************************************************
 *
 *  Component: VLX vtimer frontend driver
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
 *    Thomas Charleux (thomas.charleux@redbend.com)
 *
 ****************************************************************
 */

#include <linux/clockchips.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/time.h>

#include <asm/mach-types.h>
#include <asm/mach/time.h>
#include <asm/mach/arch.h>
#include <asm/div64.h>
#include <asm/sched_clock.h>

#include <nk/nkern.h>

#undef VTIMER_DEBUG

#ifdef VTIMER_DEBUG
#define DTRACE(x...)    printk (         "VTIMER: " x) 
#else
#define DTRACE(x...)
#endif
#define  TRACE(x...)	printk (         "VTIMER: " x)
#define ETRACE(x...)	printk (KERN_ERR "VTIMER: " x)

typedef struct NkDevTimeEvent {
    NkTime min_delta;  /* min delta in time source cycles                   */
    NkTime max_delta;  /* max delta in time source cycles                   */
    NkTime delta;      /* relative time out in time source cycles           */
    NkTime expires;    /* absolute time out in time source cycles           */
    NkBool periodic;   /* 1: periodic, 0: one shot                          */
    nku32_f filler;    /* The size of the structure must be 64-bits aligned */
} NkDevTimeEvent;

typedef struct VTimer {
    nku32_f         freq;	  /* timer frequency Hz */
    NkDevVlink*     vlink;        /* vlink            */
    NkDevTimeEvent* tevent;       /* Time Event       */
    NkXIrq	    s_xirq;	  /* server side xirq */
    NkXIrq	    c_xirq;	  /* client side xirq */
    void*           id;		  /* NK CBSP timer ID */
} VTimer;

int              nk_use_htimer = 1; /* use hardware timer */
struct sys_timer nk_vtick_timer;    /* system timer       */

static VTimer                    vtimer;            /* One virtual timer     */
static struct clock_event_device vtimer_clockevent; /* Clock Event           */
static int                       vtimer_ready;

    /*
     * Calculate appropriate shift and mult for a clock source
     */
    static inline void
_clocksource_shift_and_mult (struct clocksource *cs, unsigned int clock)
{
    u64 temp;
    u32 shift;

        /*
         * Find a shift value
         */
    for (shift = 32; shift > 0; shift--) {
	temp = (u64) NSEC_PER_SEC << shift;
	do_div(temp, clock);
	if ((temp >> 32) == 0) {
	    break;
	}
    }
    cs->shift = shift;
    cs->mult  = (u32) temp;
}

    /*
     * Calculate appropriate shift and mult for a clock event
     */
    static inline void
_clockevent_shift_and_mult (struct clock_event_device *cd, unsigned int clock)
{
    u64 temp;
    u32 shift;

        /*
         * Find a shift value
         */
    for (shift = 32; shift > 0; shift--) {
	temp = (u64) clock << shift;
	do_div(temp, NSEC_PER_SEC);
	if ((temp >> 32) == 0) {
	    break;
	}
    }
    cd->shift = shift;
    cd->mult  = (u32) temp;
}

    /*
     * Cross interrupt handler
     */
    static irqreturn_t
vtimer_xirq_hdl (int irq, void *dev_id)
{
    struct clock_event_device* evt = &vtimer_clockevent;
        /*
         * call the clock event handler
         */
    evt->event_handler(evt);
    return IRQ_HANDLED;
}

    /*
     * The structure used by setup_irq
     */
static struct irqaction vtimer_irq = {
    .name    = "vtimer",
    .flags   = IRQF_DISABLED | IRQF_TIMER,
    .handler = vtimer_xirq_hdl,
};

    /*
     * Search for our specific vlink: vtimer
     * Make standard initialization
     */
    static int
vtimer_lookup_and_setup (void)
{
    NkPhAddr paddr;
    NkPhAddr plink = 0;
    NkOsId   my_id = nkops.nk_id_get();

    while ((plink = nkops.nk_vlink_lookup("vtimer", plink))) {
	vtimer.vlink = nkops.nk_ptov(plink);	
	if ((vtimer.vlink->c_id == my_id) && vtimer.vlink->s_id) {
	        /*
	         * Allocate our specific structure in the persistent device
                 * repository
	         */
	    paddr = nkops.nk_pdev_alloc(plink, 0, sizeof(NkDevTimeEvent));
	    if (paddr == 0) {
		ETRACE("nk_pdev_alloc failed, size=%d\n",
		       sizeof(NkDevTimeEvent));
		return 0;
	    }
	        /*
                 * Retrieve a virtual address
                 */
	    vtimer.tevent = (NkDevTimeEvent*)nkops.nk_ptov(paddr);
	    if (vtimer.tevent == 0) {
		ETRACE("nk_ptov failed, paddr=0x%x\n", paddr);
		return 0;
	    }
	        /*
                 * Allocate the server cross interrupt
                 */
	    vtimer.s_xirq = nkops.nk_pxirq_alloc(plink, 0, vtimer.vlink->s_id,
						 1);
	    if (vtimer.s_xirq == 0) {
		ETRACE("nk_pxirq_alloc failed (server)\n");
		return 0;
	    }
	        /*
                 * Allocate the client cross interrupt
                 */
	    vtimer.c_xirq = nkops.nk_pxirq_alloc(plink, 1, vtimer.vlink->c_id,
						 1);
	    if (vtimer.c_xirq == 0) {
		ETRACE("nk_pxirq_alloc failed (client)\n");
		return 0;
	    }
	    return 1;
	}
    }

    ETRACE("no vlink found\n");
    return 0;
}

    /*
     * Handle the different clock event modes
     */
    static void
vtimer_legacy_set_mode (enum clock_event_mode mode,
			struct clock_event_device *evt)
{
    NkTime delta;

    switch(mode) {
    case CLOCK_EVT_MODE_PERIODIC:
	DTRACE("%s PERIODIC mode\n", evt->name);
	    /*
	     * Calculate the delta for a periodic tick (HZ)
             */
	delta   = NSEC_PER_SEC/HZ;
	delta  *= evt->mult;
	delta >>= evt->shift;

	vtimer.tevent->expires  = os_ctx->smp_time() + delta;
	vtimer.tevent->delta    = delta;
	vtimer.tevent->periodic = 1;
	nkops.nk_xirq_trigger(vtimer.s_xirq, vtimer.vlink->s_id);	
	break;
	
    case CLOCK_EVT_MODE_SHUTDOWN:
    case CLOCK_EVT_MODE_UNUSED:
	DTRACE("%s SHUTDOWN mode\n", evt->name);
	if (evt->mode == CLOCK_EVT_MODE_PERIODIC ||
	    evt->mode == CLOCK_EVT_MODE_ONESHOT) {
	    vtimer.tevent->delta = 0;
	    nkops.nk_xirq_trigger(vtimer.s_xirq, vtimer.vlink->s_id);
	}
	break;
	
    case CLOCK_EVT_MODE_ONESHOT:
	DTRACE("%s ONESHOT mode\n", evt->name);
	vtimer.tevent->periodic = 0;
	break;
	
    case CLOCK_EVT_MODE_RESUME:
	    /*
             * Nothing to do here
             */
	break;
    }
}

    /*
     * Program the next event
     */
    static int
vtimer_legacy_set_next_event (unsigned long delta,
			      struct clock_event_device *evt)
{
    vtimer.tevent->expires = os_ctx->smp_time() + delta;
    vtimer.tevent->delta   = delta;
    nkops.nk_xirq_trigger(vtimer.s_xirq, vtimer.vlink->s_id);
    return 0;
}

    static void
vtimer_set_mode (enum clock_event_mode mode, struct clock_event_device *evt)
{
    switch(mode) {
    case CLOCK_EVT_MODE_PERIODIC:
	DTRACE("%s PERIODIC mode\n", evt->name);
	os_ctx->smp_timer_start_periodic(vtimer.id, HZ);
	break;
	
    case CLOCK_EVT_MODE_SHUTDOWN:
    case CLOCK_EVT_MODE_UNUSED:
	DTRACE("%s SHUTDOWN mode\n", evt->name);
	if (evt->mode == CLOCK_EVT_MODE_PERIODIC ||
	    evt->mode == CLOCK_EVT_MODE_ONESHOT) {
	    os_ctx->smp_timer_stop(vtimer.id);
	}
	break;
	
    case CLOCK_EVT_MODE_ONESHOT:
	DTRACE("%s ONESHOT mode\n", evt->name);
	if (evt->mode == CLOCK_EVT_MODE_PERIODIC) {
	    os_ctx->smp_timer_stop(vtimer.id);
	}
	break;
	
    case CLOCK_EVT_MODE_RESUME:
	    /*
             * Nothing to do here
             */
	break;
    }
}

    /*
     * Program the next event
     */
    static int
vtimer_set_next_event (unsigned long delta, struct clock_event_device *evt)
{
    os_ctx->smp_timer_start_oneshot(vtimer.id, delta);
    return 0;
}

static struct clock_event_device vtimer_clockevent = {
    .name	    = "vtimer",
    .features	    = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
    .set_mode	    = vtimer_legacy_set_mode,
    .set_next_event = vtimer_legacy_set_next_event,
};

    /*
     * Handle the command line
     */
    static int __init
vtimer_cmd_line (char *s)
{
    if (!strcmp(s, "virtual")) {
	nk_use_htimer = 0;
	machine_desc->timer = &nk_vtick_timer;
    } else {
	nk_use_htimer = 1;
    }

    return 1;
}

__setup("linux-timer=", vtimer_cmd_line);

    static __init int
vtimer_legacy_setup (void)
{
    if (!vtimer_lookup_and_setup()) {
	return 0;
    }
        /*
         * Retrieve the time source frequency
         */
    vtimer.freq = os_ctx->smp_time_hz();
        /*
         * Calculate shift and mult
         */
    _clockevent_shift_and_mult(&vtimer_clockevent, vtimer.freq);

    DTRACE("clockevent shift=%d mult=%u\n", vtimer_clockevent.shift,
	   vtimer_clockevent.mult);

    vtimer_clockevent.max_delta_ns =
	clockevent_delta2ns(vtimer.tevent->max_delta, &vtimer_clockevent);
    vtimer_clockevent.min_delta_ns =
	clockevent_delta2ns(vtimer.tevent->min_delta, &vtimer_clockevent);

    return 1;
}

    static __init int
vtimer_setup (void)
{
    NkTimerInfo info;

    if (!os_ctx->smp_dxirq_alloc || !os_ctx->smp_timer_alloc) {
	return 0;
    }

    vtimer.c_xirq = os_ctx->smp_dxirq_alloc(1);
    if (!vtimer.c_xirq) {
	ETRACE("Unable to allocate dynamic XIRQ\n");
	return 0;
    }

    vtimer.id = os_ctx->smp_timer_alloc(vtimer.c_xirq,
			   NK_TIMER_FLAGS_PERIODIC | NK_TIMER_FLAGS_ONESHOT);
    if (!vtimer.id) {
	ETRACE("Unable to allocate NK timer\n");
	return 0;
    }

    vtimer_clockevent.set_mode       = vtimer_set_mode;
    vtimer_clockevent.set_next_event = vtimer_set_next_event;

    os_ctx->smp_timer_info(vtimer.id, nkops.nk_vtop(&info));

    vtimer.freq = info.freq_hz;

    vtimer_clockevent.features = 0;
    if (info.flags & NK_TIMER_FLAGS_PERIODIC) {
       vtimer_clockevent.features |=  CLOCK_EVT_FEAT_PERIODIC;
    }
    if (info.flags & NK_TIMER_FLAGS_ONESHOT) {
       vtimer_clockevent.features |=  CLOCK_EVT_FEAT_ONESHOT;
    }

    _clockevent_shift_and_mult(&vtimer_clockevent, vtimer.freq);

    vtimer_clockevent.max_delta_ns =
	clockevent_delta2ns(info.max_delta, &vtimer_clockevent);
    vtimer_clockevent.min_delta_ns =
	clockevent_delta2ns(info.min_delta, &vtimer_clockevent);

    DTRACE("NK CBSP interface used\n");

    return 1;
}

    /*
     * Register a clock event device
     */
    static __init void
vtimer_clockevent_init (void)
{
    if (!vtimer_setup() && !vtimer_legacy_setup()) {
	return;
    }
	/*
	 * Here we call setup_irq() directly, instead of request_irq()
	 * or xirq_attach_masked(), because those functions rely on 
         * kmalloc() which is not available yet (kmem_cache_init() has 
         * not been called).
	 */
    setup_irq(vtimer.c_xirq, &vtimer_irq);

    vtimer_clockevent.cpumask = cpu_all_mask;
    vtimer_clockevent.irq     = vtimer.c_xirq;

    clockevents_register_device(&vtimer_clockevent);

    TRACE("initialized\n");
}

    /*
     * Read time source
     */
    static cycle_t
vtimer_time_read (struct clocksource* cs)
{
    return os_ctx->smp_time();
}

static struct clocksource vtimer_clocksource = {
    .name   = "vtimer",
    .read   = vtimer_time_read,
    .mask   = CLOCKSOURCE_MASK(64),
    .flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

static DEFINE_CLOCK_DATA(vtimer_cd);

    static
void vtimer_update_sched_clock (void)
{
    DTRACE("%s\n", __FUNCTION__);
    update_sched_clock(&vtimer_cd, vtimer_time_read(&vtimer_clocksource), (u32)~0);
}

    static int __init
vtimer_clocksource_init (void)
{
    nku32_f freq = os_ctx->smp_time_hz();
        /*
         * Calculate shift and mult
         */
    _clocksource_shift_and_mult(&vtimer_clocksource, freq);
        /*
         * Calclate a somewhat reasonable rating value
         */
    vtimer_clocksource.rating = 200 + freq / 10000000;

    DTRACE("clocksource shift=%u mult=%u rating=%d\n",
	   vtimer_clocksource.shift, vtimer_clocksource.mult,
	   vtimer_clocksource.rating);

    init_sched_clock(&vtimer_cd, vtimer_update_sched_clock, 64, freq);

    if (clocksource_register(&vtimer_clocksource)) {
	ETRACE("Failed to register the clock source\n");
    } else {
	/* 
	 * Now sched_clock() can use our clock source
	 */
	vtimer_ready = 1;
    }

    return 0;
}

    unsigned long long
sched_clock(void)
{
    if (vtimer_ready)
	return cyc_to_sched_clock(&vtimer_cd,
				  vtimer_clocksource.read(&vtimer_clocksource),
				  (u32)~0);
    else
	return 0;
}

    static __init void
vtimer_init (void)
{
    vtimer_clockevent_init();
    vtimer_clocksource_init();
}

struct sys_timer nk_vtick_timer = {
    .init = vtimer_init,
};

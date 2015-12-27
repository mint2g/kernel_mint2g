/*
 ****************************************************************
 *
 *  Component: VLX vtick frontend driver
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
 ****************************************************************
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/mach-types.h>
#include <asm/mach/time.h>

#include <nk/nkern.h>

#define NK_VTICK_MODULO

/*
 * Functions which are board dependent which must be provided
 * to support this vtick front-end driver
 */
extern unsigned long nk_vtick_get_ticks_per_hz(void);
extern unsigned long nk_vtick_get_modulo(void);
extern unsigned long nk_vtick_read_stamp(void);
extern unsigned long nk_vtick_ticks_to_usecs(unsigned long);

static NkDevTick* nktick;
static NkOsId     nktick_owner;
static NkOsId     nkosid;
int    nk_use_htimer = 1;

struct sys_timer nk_vtick_timer;

unsigned long nk_vtick_last_tick = 0;
unsigned long nk_vtick_ticks_per_hz;
unsigned long nk_vtick_modulo;

NkDevTick* nk_tick_lookup (void)
{
	int waiting = 0;
	NkDevTick* nktick;

	for (;;) {
		NkPhAddr pdev = 0;
		while ((pdev = nkops.nk_dev_lookup_by_type(NK_DEV_ID_TICK,
							   pdev))) {
			unsigned long flags;

			NkDevDesc*  vdev = (NkDevDesc*)nkops.nk_ptov(pdev);
			NkOsId      id   = nkops.nk_id_get();
			NkOsMask    mask = nkops.nk_bit2mask(id);

			nktick = (NkDevTick*)nkops.nk_ptov(vdev->dev_header);
			nktick_owner = (vdev->dev_owner ? vdev->dev_owner :
					NK_OS_PRIM);
			flags = hw_local_irq_save();
			if (!nktick->freq) {
				nktick->freq = HZ;
			}
			if (nktick->freq != HZ) {
				nktick = 0;
			}
			if (nktick) {
				nktick->enabled   |= mask;
				nktick->silent[id] = (NkTickCounter)(-1);
			}
			hw_local_irq_restore(flags);
			if (nktick) {
				if (waiting) {
				    printnk("NK: Virtual timer detected.\n");
				}
				return nktick;
			}
		}
		if (!waiting) {
			waiting = 1;
			printnk("NK: Waiting for a virtual timer device...\n");
		}
	}
	return nktick;
}

static struct irqaction nk_tick_irq = {
	.name		= "virtual tick",
	.flags		= IRQF_DISABLED,
	.handler	= 0,
};

void nk_tick_connect (NkDevTick*    nktick,
		      irqreturn_t (*hdl)(int, void *))
{
	NkXIrq   xirq;

	nkosid = nkops.nk_id_get();

	xirq = nktick->xirq[nkosid];
	if (!xirq) {
		xirq = nkops.nk_xirq_alloc(1);
		if (!xirq) {
			panic("cannot allocate a xirq");
		}
		nktick->xirq[nkosid] = xirq;
	}

	/*
	 * set the interrupt handler
	 */
	nk_tick_irq.handler = hdl;
	/*
	 * Here we call setup_irq() directly, instead of request_irq()
	 * or xirq_attach_masked(), because those functions rely on kmalloc()
	 * which is not available yet (kmem_cache_init() has not been called).
	 */
	setup_irq(xirq, &nk_tick_irq);

	printk("NK: Using virtual timer\n");

	nktick->silent[nkosid] = 0;

	nkops.nk_xirq_trigger(nktick->xirq[nktick_owner], nktick_owner);
}

static int __init nk_vtimer_opt (char *s)
{
	if (!strcmp(s, "virtual") && !machine_is_arm_osware()) {
		nk_use_htimer = 0;
		system_timer = &nk_vtick_timer;
	} else {
		nk_use_htimer = 1;
	}
	return 1;
}

__setup("linux-timer=", nk_vtimer_opt);


/*
 * The following is relying on some board dependent services
 */
static unsigned nk_modulo_count = 0; /* Counts 1/HZ units */

static irqreturn_t nk_vtick_timer_interrupt(int irq, void *dev_id)
{
	unsigned long now;

	now = nktick->last_stamp;

	while (now - nk_vtick_last_tick >= nk_vtick_ticks_per_hz) {
#ifdef NK_VTICK_MODULO
		/* Modulo addition may put nk_vtick_last_tick ahead of now
		 * and cause unwanted repetition of the while loop.
		 */
		if (unlikely(now - nk_vtick_last_tick == ~0))
			break;

		nk_modulo_count += nk_vtick_modulo;
		if (nk_modulo_count > HZ) {
			++nk_vtick_last_tick;
			nk_modulo_count -= HZ;
		}
#endif
		nk_vtick_last_tick += nk_vtick_ticks_per_hz;
		timer_tick();
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_NO_IDLE_HZ

void nk_vtick_timer_reprogram(unsigned long next_tick)
{
	if (next_tick > 1) {
		nktick->silent[nkosid] = next_tick;
	}
}

static int nk_vtick_timer_enable_dyn_tick(void)
{
	/* No need to reprogram timer, just use the next interrupt */
	return 0;
}

static int nk_vtick_timer_disable_dyn_tick(void)
{
	nktick->silent[nkosid] = 0;
	return 0;
}


static struct dyn_tick_timer nk_vtick_dyn_tick_timer = {
	.enable		= nk_vtick_timer_enable_dyn_tick,
	.disable	= nk_vtick_timer_disable_dyn_tick,
	.reprogram	= nk_vtick_timer_reprogram,
	.handler	= nk_vtick_timer_interrupt,
};
#endif

static __init void nk_init_vtick_timer(void)
{
	nk_vtick_ticks_per_hz   = nk_vtick_get_ticks_per_hz();
	nk_vtick_modulo         = nk_vtick_get_modulo();
#ifdef CONFIG_NO_IDLE_HZ
	nk_vtick_timer.dyn_tick = &nk_vtick_dyn_tick_timer;
#endif
	nktick = nk_tick_lookup();
	nk_vtick_last_tick      = nktick->last_stamp;
	nk_tick_connect(nktick, nk_vtick_timer_interrupt);
}

struct sys_timer nk_vtick_timer = {
	.init		= nk_init_vtick_timer
};

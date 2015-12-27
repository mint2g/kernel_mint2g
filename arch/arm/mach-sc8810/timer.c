/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>

#include <asm/sched_clock.h>
#include <asm/mach/time.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/globalregs.h>

/* timer0/1 is trigged by RTC, 32KHZ */
#define GPTIMER_FREQ	32768

/* timer2 is trigged by PCLK, 26MHZ */
#define	PCLKTIMER_FREQ	26000000

static uint32_t timer_rates[] = {
	GPTIMER_FREQ,
	GPTIMER_FREQ,
	PCLKTIMER_FREQ
};

#define	TIMER_LOAD(id)	(SPRD_TIMER_BASE + 0x20 * (id) + 0x0000)
#define	TIMER_VALUE(id)	(SPRD_TIMER_BASE + 0x20 * (id) + 0x0004)
#define	TIMER_CTL(id)	(SPRD_TIMER_BASE + 0x20 * (id) + 0x0008)
#define	TIMER_INT(id)	(SPRD_TIMER_BASE + 0x20 * (id) + 0x000C)

#define	ONETIME_MODE	(0 << 6)
#define	PERIOD_MODE	(1 << 6)

#define	TIMER_DISABLE	(0 << 7)
#define	TIMER_ENABLE	(1 << 7)

#define	TIMER_INT_EN	(1 << 0)
#define	TIMER_INT_CLR	(1 << 3)
#define	TIMER_INT_BUSY	(1 << 4)

/* timer0 is reserved by modem side,
 * timer1 is used as clockevent,
 * timer2 is used as clocksource
 */
#define	EVENT_TIMER	1
#define	SOURCE_TIMER	2

/* syscnt is trigged by RTC, 1000HZ */
#define	SYSCNT_FREQ	1000

#define	SYSCNT_COUNT	(SPRD_SYSCNT_BASE + 0x0004)
#define	SYSCNT_CTL	(SPRD_SYSCNT_BASE + 0x0008)

static inline void sprd_gptimer_ctl(int timer_id, int enable, int mode)
{
	__raw_writel(enable | mode, TIMER_CTL(timer_id));
}

static int sprd_gptimer_set_next_event(unsigned long cycles, struct clock_event_device *c)
{
	while (__raw_readl(TIMER_INT(EVENT_TIMER)) & TIMER_INT_BUSY);

	sprd_gptimer_ctl(EVENT_TIMER, TIMER_DISABLE, ONETIME_MODE);
	__raw_writel(cycles, TIMER_LOAD(EVENT_TIMER));
	sprd_gptimer_ctl(EVENT_TIMER, TIMER_ENABLE, ONETIME_MODE);

	return 0;
}

static void sprd_gptimer_set_mode(enum clock_event_mode mode, struct clock_event_device *c)
{
	unsigned int saved;

	while (__raw_readl(TIMER_INT(EVENT_TIMER)) & TIMER_INT_BUSY);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		sprd_gptimer_ctl(EVENT_TIMER, TIMER_DISABLE, PERIOD_MODE);
		__raw_writel(LATCH, TIMER_LOAD(EVENT_TIMER));
		sprd_gptimer_ctl(EVENT_TIMER, TIMER_ENABLE, PERIOD_MODE);
		__raw_writel(TIMER_INT_EN, TIMER_INT(EVENT_TIMER));
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		sprd_gptimer_ctl(EVENT_TIMER, TIMER_ENABLE, ONETIME_MODE);
		__raw_writel(TIMER_INT_EN, TIMER_INT(EVENT_TIMER));
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		saved = __raw_readl(TIMER_CTL(EVENT_TIMER)) & PERIOD_MODE;
		sprd_gptimer_ctl(EVENT_TIMER, TIMER_DISABLE, saved);
		break;
	case CLOCK_EVT_MODE_RESUME:
		saved = __raw_readl(TIMER_CTL(EVENT_TIMER)) & PERIOD_MODE;
		sprd_gptimer_ctl(EVENT_TIMER, TIMER_ENABLE, saved);
		break;
	}
}

static struct clock_event_device sprd_gptimer_event = {
	.name		= "gptimer1",
	.features	= CLOCK_EVT_FEAT_ONESHOT | CLOCK_EVT_FEAT_PERIODIC,
	.shift		= 32,
	.rating		= 200,
	.set_next_event	= sprd_gptimer_set_next_event,
	.set_mode	= sprd_gptimer_set_mode,
};

static irqreturn_t sprd_gptimer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;
	unsigned int value;

	/* clear interrupt */
	value = __raw_readl(TIMER_INT(EVENT_TIMER));
	value |= TIMER_INT_CLR;
	__raw_writel(value, TIMER_INT(EVENT_TIMER));

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct irqaction sprd_gptimer_irq = {
	.name		= "gptimer1",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= sprd_gptimer_interrupt,
	.dev_id		= &sprd_gptimer_event,
};

static void sprd_gptimer_clockevent_init(void)
{
	__raw_writel(TIMER_DISABLE, TIMER_CTL(EVENT_TIMER));
	__raw_writel(TIMER_INT_CLR, TIMER_INT(EVENT_TIMER));

	setup_irq(IRQ_TIMER1_INT, &sprd_gptimer_irq);

	sprd_gptimer_event.mult =
		div_sc(timer_rates[EVENT_TIMER], NSEC_PER_SEC, sprd_gptimer_event.shift);
	sprd_gptimer_event.max_delta_ns =
		clockevent_delta2ns(ULONG_MAX, &sprd_gptimer_event);
	sprd_gptimer_event.min_delta_ns =
		clockevent_delta2ns(2, &sprd_gptimer_event);
	sprd_gptimer_event.cpumask = cpumask_of(0);

	clockevents_register_device(&sprd_gptimer_event);
}

/* ****************************************************************** */

static cycle_t sprd_gptimer_read(struct clocksource *cs)
{
	unsigned long val;
	/* It is driven  by pclk,no boundary issue.*/
	val = __raw_readl(TIMER_VALUE(SOURCE_TIMER));
	return (ULONG_MAX - val);
}

static struct clocksource sprd_gptimer_src = {
	.name		= "gptimer2",
	.rating		= 300,
	.read		= sprd_gptimer_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static void sprd_gptimer_clocksource_init(void)
{
	/* disalbe irq since it's just a read source */
	__raw_writel(0, TIMER_INT(SOURCE_TIMER));
	disable_irq(IRQ_TIMER2_INT);

	/* set timer load as maximal */
	sprd_gptimer_ctl(SOURCE_TIMER, TIMER_DISABLE, PERIOD_MODE);
	__raw_writel(ULONG_MAX, TIMER_LOAD(SOURCE_TIMER));
	sprd_gptimer_ctl(SOURCE_TIMER, TIMER_ENABLE, PERIOD_MODE);

	clocksource_register_hz(&sprd_gptimer_src, timer_rates[SOURCE_TIMER]);
}

/* ****************************************************************** */

static cycle_t sprd_syscnt_read(struct clocksource *cs)
{
	unsigned int val1, val2;
	unsigned long flags;

	/* read multiple times in case of boundary issue */
	local_irq_save(flags);
	val1 = __raw_readl(SYSCNT_COUNT);
	val2 = __raw_readl(SYSCNT_COUNT);
	while((int)(val2 - val1) >> 1) {
		val1 = val2;
		val2 = __raw_readl(SYSCNT_COUNT);
	}
	local_irq_restore(flags);

	return val2;
}

static struct clocksource sprd_syscnt = {
	.name		= "syscnt",
	.rating		= 200,
	.read		= sprd_syscnt_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static void sprd_syscnt_clocksource_init(void)
{
	/* disable irq for syscnt */
	__raw_writel(0, SYSCNT_CTL);
	disable_irq(IRQ_SYST_INT);

	clocksource_register_hz(&sprd_syscnt, SYSCNT_FREQ);
}

/* ****************************************************************** */

static DEFINE_CLOCK_DATA(cd);

unsigned long long notrace sched_clock(void)
{
	/* use gptimer2 as highres sched clock tick */
	struct clocksource *src = &sprd_gptimer_src;

	return cyc_to_sched_clock(&cd, src->read(src), (u32)~0);
}

static void notrace sc8810_update_sched_clock(void)
{
	/* use gptimer2 as highres sched clock tick */
	struct clocksource *src = &sprd_gptimer_src;

	update_sched_clock(&cd, src->read(src), (u32)~0);
}

void __init sc8810_timer_init(void)
{
	/* enable timer & syscnt in global regs */
	sprd_greg_set_bits(REG_TYPE_GLOBAL, GEN0_TIMER_EN | GEN0_SYST_EN, GR_GEN0);

	init_sched_clock(&cd, sc8810_update_sched_clock,
			32, timer_rates[SOURCE_TIMER]);

	/* setup timer2 as clocksource */
	sprd_gptimer_clocksource_init();
#if 0
	sprd_syscnt_clocksource_init();
#endif

	/* setup timer1 as clockevent.  */
	sprd_gptimer_clockevent_init();
}

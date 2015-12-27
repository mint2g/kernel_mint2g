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

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <asm/irqflags.h>
#include <mach/pm_debug.h>

extern void sc8810_pm_init(void);
extern void sc8810_standby_sleep(void);
extern int pm_deep_sleep(suspend_state_t state);

static int sc8810_pm_enter(suspend_state_t state)
{
	int rval = 0;
	switch (state) {
		case PM_SUSPEND_STANDBY:
			sc8810_standby_sleep();
			break;
		case PM_SUSPEND_MEM:
			rval = pm_deep_sleep(state);
			break;
		default:
			break;
	}

	return rval;
}

static int sc8810_pm_valid(suspend_state_t state)
{
	pr_debug("pm_valid: %d\n", state);
	switch (state) {
		case PM_SUSPEND_ON:
		case PM_SUSPEND_STANDBY:
		case PM_SUSPEND_MEM:
			return 1;
		default:
			return 0;
	}
}

extern void check_ldo(void);
extern void check_pd(void);
static int sc8810_pm_prepare(void)
{
	pr_debug("enter %s\n", __func__);
	check_ldo();
	check_pd();
	return 0;
}

static void sc8810_pm_finish(void)
{
	pr_debug("enter %s\n", __func__);
	print_statisic();
}

static struct platform_suspend_ops sc8810_pm_ops = {
	.valid		= sc8810_pm_valid,
	.enter		= sc8810_pm_enter,
	.prepare		= sc8810_pm_prepare,
	.prepare_late 	= NULL,
	.finish		= sc8810_pm_finish,
};

static int __init pm_init(void)
{
	sc8810_pm_init();

#ifdef CONFIG_SUSPEND
	suspend_set_ops(&sc8810_pm_ops);
#endif

	return 0;
}

device_initcall(pm_init);

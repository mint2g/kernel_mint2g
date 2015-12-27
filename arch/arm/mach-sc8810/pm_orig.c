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

extern int sc8810_deep_sleep(void);
extern void l2x0_suspend(void);
extern void l2x0_resume(int collapsed);

/*idle for sc8810*/
void sc8810_idle(void)
{
	if (!need_resched()) {
#ifdef CONFIG_CACHE_L2X0
		/*l2cache power control, standby mode enable*/
		__raw_writel(1, SPRD_CACHE310_BASE+0xF80/*L2X0_POWER_CTRL*/);
		l2x0_suspend();
#endif
		cpu_do_idle();
#ifdef CONFIG_CACHE_L2X0
		l2x0_resume(1);
#endif
	}
	local_irq_enable();
	return;
}

/*TODO: maybe need check battery status*/
 int pm_deep_sleep(suspend_state_t state){
	/* add for debug & statisic*/
	clr_sleep_mode();
	time_statisic_begin();

	WARN_ONCE(!irqs_disabled(), "#####: Interrupts enabled in sc8810_sleep()!\n");
	sc8810_deep_sleep();

	/* add for debug & statisic*/
	irq_wakeup_set();
	time_statisic_end();
	return 0;
}

void sc8810_standby_sleep(void){
	cpu_do_idle();
}















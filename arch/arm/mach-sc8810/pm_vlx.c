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
#include <asm/hardware/cache-l2x0.h>

extern int sc8810_deep_sleep(void);
extern void l2x0_suspend(void);
extern void l2x0_resume(int collapsed);

/*idle for sc8810*/
void sc8810_idle(void)
{
	int val;
	if (!need_resched()) {
		hw_local_irq_disable();
		if (!arch_local_irq_pending()) {
			val = os_ctx->idle(os_ctx);
			if (0 == val) {
#ifdef CONFIG_CACHE_L2X0
				/*l2cache power control, standby mode enable*/
				__raw_writel(1, SPRD_CACHE310_BASE+0xF80/*L2X0_POWER_CTRL*/);
#endif
				l2x0_suspend();
				cpu_do_idle();
				l2x0_resume(1);
			}
		}
		hw_local_irq_enable();
	}
	local_irq_enable();
	return;
}

void sc8810_standby_sleep(void){
	cpu_do_idle();
}

#ifdef CONFIG_STC3115_FUELGAUGE
/*for battery*/
#define BATTERY_CHECK_INTERVAL_HIGH 300000 /* 5min */
#define BATTERY_CHECK_INTERVAL_LOW 5000  /* 5sec */
extern u8 stc311x_is_low_batt;
#else
/*for battery*/
#define BATTERY_CHECK_INTERVAL 30000
extern int battery_updata(void);
extern void battery_sleep(void);

static int sprd_check_battery(void)
{
        int ret_val = 0;
        if (battery_updata()) {
                ret_val = 1;
        }
        return ret_val;
}
#endif

int pm_deep_sleep(suspend_state_t state)
{
	int ret_val = 0;
	unsigned long flags;
#ifdef CONFIG_STC3115_FUELGAUGE	
	u32 check_interval;
#endif
	u32 battery_time, cur_time;
	battery_time = cur_time = get_sys_cnt();
	/* add for debug & statisic*/
	clr_sleep_mode();
	time_statisic_begin();

#ifdef CONFIG_STC3115_FUELGAUGE
	if(stc311x_is_low_batt == 1)
		check_interval = BATTERY_CHECK_INTERVAL_HIGH;
	else
		check_interval = BATTERY_CHECK_INTERVAL_LOW;
#endif

	while(1){
		hw_local_irq_disable();
		local_irq_save(flags);
		if (arch_local_irq_pending()) {
			/* add for debug & statisic*/
			irq_wakeup_set();

			local_irq_restore(flags);
			hw_local_irq_enable();
			break;
		}else{
			local_irq_restore(flags);
			WARN_ONCE(!irqs_disabled(), "#####: Interrupts enabled in pm_enter()!\n");

			ret_val = os_ctx->idle(os_ctx);
			if (0 == ret_val) {
				sc8810_deep_sleep();
			}
			hw_local_irq_enable();
		}

#ifdef CONFIG_STC3115_FUELGAUGE
		if(stc311x_is_low_batt)
		{
			cur_time = get_sys_cnt();			
			if ((cur_time -  battery_time) > check_interval) {
				battery_time = cur_time;
				if(stc311x_is_low_batt == 1)
					check_interval = BATTERY_CHECK_INTERVAL_HIGH;
				else
					check_interval = BATTERY_CHECK_INTERVAL_LOW;
				
				printk("### wakeup for battery update %d ###\n", check_interval);
				break;
			}
		}
#else
		/*for battery check */
		battery_sleep();
		cur_time = get_sys_cnt();
		if ((cur_time -  battery_time) > BATTERY_CHECK_INTERVAL) {
			battery_time = cur_time;
			if (sprd_check_battery()) {
				printk("###: battery low!\n");
				break;
			}
		}
#endif			
	}
	/* add for debug & statisic*/
	time_statisic_end();

	return ret_val;
}

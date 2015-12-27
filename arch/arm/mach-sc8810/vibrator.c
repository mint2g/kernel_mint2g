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
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <../../../drivers/staging/android/timed_output.h>
#include <linux/sched.h>
#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/adc.h>
#if defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif /*CONFIG_HAS_WAKELOCK*/

#define SPRD_ANA_BASE           (SPRD_MISC_BASE + 0x600)
#define ANA_REG_BASE            (SPRD_ANA_BASE)
#define ANA_VIBR_WR_PROT        (ANA_REG_BASE + 0x90)
#define ANA_VIBRATOR_CTRL0      (ANA_REG_BASE + 0x6c)
#define ANA_VIBRATOR_CTRL1      (ANA_REG_BASE + 0x70)

#define VIBRATOR_REG_UNLOCK     (0xA1B2)
#define VIBRATOR_REG_LOCK       ((~VIBRATOR_REG_UNLOCK) & 0xffff)
#define VIBRATOR_STABLE_LEVEL   (3)
#define VIBRATOR_INIT_LEVEL     (11)
#define VIBRATOR_INIT_STATE_CNT (10)

#define VIBR_STABLE_V_SHIFT     (12)
#define VIBR_STABLE_V_MSK       (0x0f << VIBR_STABLE_V_SHIFT)
#define VIBR_INIT_V_SHIFT       (8)
#define VIBR_INIT_V_MSK         (0x0f << VIBR_INIT_V_SHIFT)
#define VIBR_V_BP_SHIFT         (4)
#define VIBR_V_BP_MSK           (0x0f << VIBR_V_BP_SHIFT)
#define VIBR_PD_RST             (1 << 3)
#define VIBR_PD_SET             (1 << 2)
#define VIBR_BP_EN              (1 << 1)
#define VIBR_RTC_EN             (1 << 0)

#define VIB_ON 1
#define VIB_OFF 0
#define MIN_TIME_MS 100

#if defined(CONFIG_HAS_WAKELOCK)
static struct wake_lock vib_wl;
#endif /*CONFIG_HAS_WAKELOCK*/

static struct timer_list	vibrate_timer;
static struct work_struct	vibrator_off_work;
static int Is_vib_shortly;

static void set_vibrator(int on)
{
	/* unlock vibrator registor */
	sci_adi_write(ANA_VIBR_WR_PROT, VIBRATOR_REG_UNLOCK, 0xffff);
	sci_adi_clr(ANA_VIBRATOR_CTRL0, VIBR_PD_SET | VIBR_PD_RST);
	if (on)
		sci_adi_set(ANA_VIBRATOR_CTRL0, VIBR_PD_RST);
	else
		sci_adi_set(ANA_VIBRATOR_CTRL0, VIBR_PD_SET);
	/* lock vibrator registor */
	sci_adi_write(ANA_VIBR_WR_PROT, VIBRATOR_REG_LOCK, 0xffff);
}

static void vibrator_hw_init(void)
{
	sci_adi_write(ANA_VIBR_WR_PROT, VIBRATOR_REG_UNLOCK, 0xffff);
	sci_adi_set(ANA_VIBRATOR_CTRL0, VIBR_RTC_EN);
	sci_adi_clr(ANA_VIBRATOR_CTRL0, VIBR_BP_EN);
	/* set init current level */
	sci_adi_write(ANA_VIBRATOR_CTRL0,
		      (VIBRATOR_INIT_LEVEL << VIBR_INIT_V_SHIFT),
		      VIBR_INIT_V_MSK);
	sci_adi_write(ANA_VIBRATOR_CTRL0,
		      (VIBRATOR_STABLE_LEVEL << VIBR_STABLE_V_SHIFT),
		      VIBR_STABLE_V_MSK);
	/* set stable current level */
	sci_adi_write(ANA_VIBRATOR_CTRL1, VIBRATOR_INIT_STATE_CNT, 0xffff);
	sci_adi_write(ANA_VIBR_WR_PROT, VIBRATOR_REG_LOCK, 0xffff);
}

static void vibrator_ctrl_regulator(int on_off)
{
	printk(KERN_NOTICE "Vibrator: %s\n",(on_off?"ON":"OFF"));
	set_vibrator(on_off);
}

static void vibrator_off_worker(struct work_struct *work)
{
	vibrator_ctrl_regulator(VIB_OFF);

#if defined(CONFIG_HAS_WAKELOCK)
	wake_unlock(&vib_wl);
#endif /*CONFIG_HAS_WAKELOCK*/
}

static void on_vibrate_timer_expired(unsigned long x)
{
   printk(KERN_NOTICE "Vibrator: expired %ldms\n", x);
	Is_vib_shortly = false;
	schedule_work(&vibrator_off_work);
}

static void vibrator_enable_set_timeout(struct timed_output_dev *sdev,
	int timeout)
{
	int ret_mod_timer = 0;
	printk(KERN_NOTICE "Vibrator: Set duration: %dms\n", timeout);

	if( timeout == 0 )
	{
		if(Is_vib_shortly == false)
      {
			vibrator_ctrl_regulator(VIB_OFF);
			del_timer(&vibrate_timer);
			#if defined(CONFIG_HAS_WAKELOCK)
			wake_unlock(&vib_wl);
			#endif /*CONFIG_HAS_WAKELOCK*/
		}
      
		return;
	}

#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock(&vib_wl);
#endif /*CONFIG_HAS_WAKELOCK*/

	vibrator_ctrl_regulator(VIB_ON);
	if(timeout < MIN_TIME_MS)
   {
		Is_vib_shortly = true;
		timeout *= 2;
	}
   
	if(timeout == 5000 || timeout == 10000)
   {
		printk(KERN_NOTICE "Vibrator: timeout= %dms, skip off\n", timeout);
   }
	else
   {
		if(!timer_pending(&vibrate_timer))
      {
			ret_mod_timer = mod_timer(&vibrate_timer, jiffies + msecs_to_jiffies(timeout));
         if( ret_mod_timer )
         {
				printk(KERN_NOTICE "Vibrator: ret_mod_timer= %d\n", ret_mod_timer);
            vibrator_ctrl_regulator(VIB_OFF);
				    #if defined(CONFIG_HAS_WAKELOCK)
				    wake_unlock(&vib_wl);
				    #endif /*CONFIG_HAS_WAKELOCK*/				
         }
      }
		else
		{
#if defined(CONFIG_HAS_WAKELOCK)
			wake_unlock(&vib_wl);
#endif /*CONFIG_HAS_WAKELOCK*/
		}
	}		
   
	return;
}

static int vibrator_get_remaining_time(struct timed_output_dev *sdev)
{
	int retTime = jiffies_to_msecs(jiffies - vibrate_timer.expires);
	printk(KERN_NOTICE "Vibrator: Current duration: %dms\n", retTime);
	return retTime;
}

static struct timed_output_dev sprd_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_remaining_time,
	.enable = vibrator_enable_set_timeout,
};

static int __init sprd_init_vibrator(void)
{
	int ret = 0;
	
	vibrator_hw_init();

	init_timer(&vibrate_timer);
	vibrate_timer.function = on_vibrate_timer_expired;
	vibrate_timer.data = (unsigned long)NULL;

	INIT_WORK(&vibrator_off_work, vibrator_off_worker);
#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock_init(&vib_wl, WAKE_LOCK_SUSPEND, __stringify(vib_wl));
#endif

	Is_vib_shortly = false;

	ret = timed_output_dev_register(&sprd_vibrator);
	if (ret < 0) {
		printk(KERN_ERR "Vibrator: timed_output dev registration failure\n");
		goto error;
	}

	return 0;

error:
#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock_destroy(&vib_wl);
#endif
	return ret;
}

static void __exit sprd_exit_vibrator(void)
{
	timed_output_dev_unregister(&sprd_vibrator);
#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock_destroy(&vib_wl);
#endif
}

module_init(sprd_init_vibrator);
module_exit(sprd_exit_vibrator);

MODULE_DESCRIPTION("vibrator driver for spreadtrum Processors");
MODULE_LICENSE("GPL");

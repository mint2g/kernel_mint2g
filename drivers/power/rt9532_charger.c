/*
 * RT9532 Linear Charger driver
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <mach/gpio.h>
#include <mach/board.h>
#if defined(CONFIG_SEC_CHARGING_FEATURE)
#include <linux/spa_power.h>
#include <linux/spa_agent.h>
#endif

#ifndef GPIO_EXT_CHARGER_DET
#define GPIO_EXT_CHARGER_DET		135
#endif
#ifndef GPIO_EXT_CHARGER_EN
#define GPIO_EXT_CHARGER_EN		142
#endif
#ifndef GPIO_EXT_CHARGER_STATE
#define GPIO_EXT_CHARGER_STATE	143
#endif

static struct charger_data {
	unsigned int irq_pgb;
	unsigned int irq_chgsb;
	unsigned int ovp_flag;
	unsigned int cc_cv_flag;
	unsigned int is_charge_enabled;
	unsigned int current_charger_type;
	struct work_struct	duration_work;
	struct delayed_work	init_delay_work;
	struct delayed_work	irq_work;
	struct wake_lock	irq_wakelock;
};

spinlock_t	change_current_lock;
static struct charger_data charger;

static void rt9532_set_charge_current_USB500(void)
{
	pr_info("%s\n", __func__);
	gpio_direction_output(GPIO_EXT_CHARGER_EN, 0);
	udelay(100);
}

static void rt9532_set_charge_current_ISET(void)
{
	unsigned long flags;
	pr_info("%s\n", __func__);

	gpio_direction_output(GPIO_EXT_CHARGER_EN, 0);
	udelay(100);
	
	spin_lock_irqsave(&change_current_lock, flags);	
	gpio_direction_output(GPIO_EXT_CHARGER_EN, 1);
	udelay(200);
	gpio_direction_output(GPIO_EXT_CHARGER_EN, 0);
	spin_unlock_irqrestore(&change_current_lock, flags);	

	udelay(2000);
}

int rt9532_get_charger_online(void)
{
	return charger.is_charge_enabled;
}
EXPORT_SYMBOL(rt9532_get_charger_online);

void rt9532_disable_charge(void)
{
	pr_info("%s\n", __func__);

	charger.is_charge_enabled = 0;
	charger.current_charger_type = 0;
	gpio_direction_output(GPIO_EXT_CHARGER_EN, 1);
	udelay(2000);
}

void rt9532_enable_charge(enum power_supply_type type)
{
	charger.is_charge_enabled = 1;
	charger.current_charger_type = type;

	pr_info("%s charger type %d\n", __func__, type);	
	switch(type)
	{
		case POWER_SUPPLY_TYPE_USB_DCP:
			rt9532_set_charge_current_ISET();
			break;
		case POWER_SUPPLY_TYPE_USB:
			rt9532_set_charge_current_USB500();
			break;
		default:
			pr_err("%s wrong charger type %d\n", __func__, type);
			break;
	}
}

static void rt9532_irq_work(struct work_struct *work)
{
	int pgb = 0 , chgsb = 0;

	pgb = gpio_get_value(GPIO_EXT_CHARGER_DET)? 0 : 1;
	chgsb = gpio_get_value(GPIO_EXT_CHARGER_STATE) ? 0 : 1;

	pr_info("%s pgb : %d chgsb : %d\n", __func__, pgb, chgsb);

	if(chgsb)
	{
		if(pgb)
		{
			if(charger.ovp_flag)
			{
				pr_info("%s OVP/UVLO DISABLE\n", __func__);
#if defined(CONFIG_SEC_CHARGING_FEATURE)					
				spa_event_handler(SPA_EVT_OVP, 0);
#endif
				charger.ovp_flag = 0;
			}
			charger.cc_cv_flag = 1;
		}
	}
	else		
	{
		if(pgb)
		{
			if(charger.is_charge_enabled)
			{
				if(charger.cc_cv_flag)
				{
#if !defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)					
					pr_info("%s FULL CHARGING\n", __func__);
#if defined(CONFIG_SEC_CHARGING_FEATURE)					
					spa_event_handler(SPA_EVT_EOC, 0);
#endif
#endif
				}
			}			
			else
			{
				if(charger.ovp_flag)
				{
					pr_info("%s OVP/UVLO DISABLE\n", __func__);
#if defined(CONFIG_SEC_CHARGING_FEATURE)					
					spa_event_handler(SPA_EVT_OVP, 0);
#endif
					charger.ovp_flag = 0;
				}
			}
			
			charger.cc_cv_flag = 0;
		}
		else
		{
			if(charger.is_charge_enabled)
			{
				if(!charger.ovp_flag)
				{
					pr_info("%s OVP/UVLO ENABLE\n", __func__);
					charger.ovp_flag = 1;
#if defined(CONFIG_SEC_CHARGING_FEATURE)					
					spa_event_handler(SPA_EVT_OVP, 1);
#endif
				}
			}
			else
			{
				if(charger.ovp_flag)
				{
					pr_info("%s OVP/UVLO DISABLE\n", __func__);					
#if defined(CONFIG_SEC_CHARGING_FEATURE)					
					spa_event_handler(SPA_EVT_OVP, 0);
#endif
					charger.ovp_flag = 0;
				}
			}
			charger.cc_cv_flag = 0;
		}
	}
}

static void rt9532_duration_work(struct work_struct *work)
{
	if(delayed_work_pending(&charger.irq_work))
		cancel_delayed_work_sync(&charger.irq_work);	
	
	schedule_delayed_work(&charger.irq_work, msecs_to_jiffies(1000));
}

static irqreturn_t rt9532_irq_handler(int irq, void *dev)
{
	pr_info("%s %d\n", __func__, irq);
	
	wake_lock_timeout(&charger.irq_wakelock, (5 * HZ));

	schedule_work(&charger.duration_work);

	return IRQ_HANDLED;
}

static void rt9532_init_delay_work(struct work_struct *work)
{
	int ret = 0;
	rt9532_irq_work(NULL);

	ret = request_threaded_irq(charger.irq_chgsb, rt9532_irq_handler, NULL, 
		(IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND), "rt9532_CHGSB", &charger); 
	if(ret)
		pr_err("%s %d request_irq failed\n", __func__, charger.irq_chgsb);

	ret = request_threaded_irq(charger.irq_pgb,   rt9532_irq_handler, NULL,
		(IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND), "rt9532_PGB", &charger); 
	if(ret)
		pr_err("%s %d request_irq failed\n", __func__, charger.irq_pgb);
}

static int __init rt9532_init(void)
{	
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = gpio_request(GPIO_EXT_CHARGER_EN, "rt9532 charger en");
	if(ret)
	{
		pr_err("%s %d gpio request failed\n", __func__, GPIO_EXT_CHARGER_EN);
		goto rt9532_error1;
	}
	gpio_direction_output(GPIO_EXT_CHARGER_EN, 0);		

	ret = gpio_request(GPIO_EXT_CHARGER_DET, "rt9532 charger pgb");
	if(ret)
	{
		pr_err("%s %d gpio request failed\n", __func__, GPIO_EXT_CHARGER_DET);
		goto rt9532_error2;
	}
	gpio_direction_input(GPIO_EXT_CHARGER_DET);
	charger.irq_pgb = gpio_to_irq(GPIO_EXT_CHARGER_DET);

	ret = gpio_request(GPIO_EXT_CHARGER_STATE, "rt9532 charger chgsb");
	if(ret)
	{
		pr_err("%s %d gpio request failed\n", __func__, GPIO_EXT_CHARGER_STATE);
		goto rt9532_error3;
	}
	gpio_direction_input(GPIO_EXT_CHARGER_STATE);
	charger.irq_chgsb = gpio_to_irq(GPIO_EXT_CHARGER_STATE);

	INIT_DELAYED_WORK(&charger.init_delay_work, rt9532_init_delay_work);
	INIT_DELAYED_WORK(&charger.irq_work, rt9532_irq_work);
	INIT_WORK(&charger.duration_work, rt9532_duration_work);
	wake_lock_init(&charger.irq_wakelock, WAKE_LOCK_SUSPEND, "rt9532_irq_wakelock");
	spin_lock_init(&change_current_lock);
	charger.ovp_flag = 0;
	charger.cc_cv_flag = 0;

	schedule_delayed_work(&charger.init_delay_work, msecs_to_jiffies(2000));

	return 0;

rt9532_error3:
	gpio_free(GPIO_EXT_CHARGER_DET);	
rt9532_error2:
	gpio_free(GPIO_EXT_CHARGER_EN);
rt9532_error1:
	return ret;	
}

static void __exit rt9532_exit(void)
{
	cancel_work_sync(&charger.duration_work);
	cancel_delayed_work_sync(&charger.init_delay_work);	
	cancel_delayed_work_sync(&charger.irq_work);	
	wake_lock_destroy(&charger.irq_wakelock);
	free_irq(charger.irq_pgb, &charger);	
	free_irq(charger.irq_chgsb, &charger);	
	gpio_free(GPIO_EXT_CHARGER_STATE);		
	gpio_free(GPIO_EXT_CHARGER_DET);	
	gpio_free(GPIO_EXT_CHARGER_EN);
}

module_init(rt9532_init);
module_exit(rt9532_exit);

MODULE_DESCRIPTION("RT9532 Linear Charger driver");
MODULE_LICENSE("GPL");

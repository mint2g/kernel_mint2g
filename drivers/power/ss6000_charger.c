
/*
 *  ss6000_charger.c
 *  charger-IC driver  
 *
 *
 *  Copyright (C) 2011, Samsung Electronics
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */ 


#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <asm/irq.h>
#include <mach/gpio.h>
#include <asm/uaccess.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
//#include <mach/mfp.h>

#include <linux/ss6000_charger.h>

#if defined(CONFIG_SPA)
#include <linux/power/spa.h>
static void (*spa_external_event)(int, int) = NULL;
#endif

static enum power_supply_type current_charger_type = 0;
static int prev_chgsb_status = 1;
static int is_charge_enabled = 0;
static unsigned int en_set = 0;
spinlock_t	change_current_lock;

static void ss6000_set_charge_current_USB500(void)
{
	printk("%s\n", __func__);

	if(!en_set)
		goto err_fail_set_charge_current_USB500;

	gpio_direction_output(en_set, 0);
	udelay(100);

	return; 

err_fail_set_charge_current_USB500:
	printk("%s en_set is NULL\n", __func__);
}

static void ss6000_set_charge_current_ISET(void)
{
	unsigned long flags;
	printk("%s\n", __func__);

	if(!en_set)
		goto err_fail_set_charge_current_ISET;
	gpio_direction_output(en_set, 0);
	udelay(100);

	spin_lock_irqsave(&change_current_lock, flags);
	gpio_direction_output(en_set, 1);
	udelay(150);
	gpio_direction_output(en_set, 0);
	spin_unlock_irqrestore(&change_current_lock, flags);

#if defined(CONFIG_BOARD_CORI)
	udelay(1500);
#else
	msleep(2);
#endif

	return; 

err_fail_set_charge_current_ISET:
	printk("%s en_set is NULL\n", __func__);
}

static void ss6000_disable_charge(void)
{
	printk("%s\n", __func__);
	
	current_charger_type = 0;
	prev_chgsb_status = 1;
	is_charge_enabled = 0;	

	if(!en_set)
		goto err_fail_disable_charge;
	gpio_direction_output(en_set, 1);

#if defined(CONFIG_BOARD_CORI)
	udelay(1500);
#else
	msleep(2);
#endif
	
	return; 

err_fail_disable_charge:
	printk("%s en_set is 0\n", __func__);
}

static void ss6000_enable_charge(enum power_supply_type type)
{
	is_charge_enabled = 1;
	current_charger_type = type;

printk("%s charger type %d\n", __func__, type);	

	switch(type)
	{
		case POWER_SUPPLY_TYPE_MAINS:
			ss6000_set_charge_current_ISET();
			break;
		case POWER_SUPPLY_TYPE_USB:
			ss6000_set_charge_current_USB500();
			break;
		default:
			printk("%s wrong charger type %d\n", __func__, type);
			break;
	}
}

static void ss6000_restart_charge(enum power_supply_type type)
{
	printk("%s\n", __func__);
	if(!en_set)
		goto err_fail_restart_charge;
	gpio_direction_output(en_set, 1);

#if defined(CONFIG_BOARD_CORI)
	udelay(1500);
#else
	msleep(2);
#endif

	switch(type)
	{
		case POWER_SUPPLY_TYPE_MAINS:
			ss6000_set_charge_current_ISET();
			break;
		case POWER_SUPPLY_TYPE_USB:
			ss6000_set_charge_current_USB500();
			break;
		default:
			printk("%s wrong charger type %d\n", __func__, type);
			break;
	}
	
	return; 

err_fail_restart_charge:
	printk("%s en_set is 0\n", __func__);
}

void external_enable_charge(enum power_supply_type type)
{
	ss6000_enable_charge(type);
}

void external_disable_charge(void)
{
	ss6000_disable_charge();
}

void external_restart_charge(enum power_supply_type type)
{
	ss6000_restart_charge(type);
}

static void ss6000_chgsb_irq_work(struct work_struct *work)
{
	struct ss6000_platform_data *pdata = container_of(work, struct ss6000_platform_data, chgsb_irq_work.work);
	int pgb = 0, chgsb = 0;
	struct power_supply *psy = NULL;
	union power_supply_propval psy_data; 
	
	pgb = gpio_get_value(pdata->pgb)? 1 : 0;
	chgsb = gpio_get_value(pdata->chgsb) ? 1 : 0;

	printk("%s pgb : %d chgsb : %d prev_chgsb : %d\n", __func__, pgb, chgsb, prev_chgsb_status);

	if(pgb)
	{
		
	}
	else
	{
		if(chgsb)
		{
			if((prev_chgsb_status==0) && (is_charge_enabled))
			{
				psy = power_supply_get_by_name("battery");
				if(psy == NULL)
				{
					printk("%s fail to get power supply\n", __func__);
					goto ss6000_power_supply_failed;
				}
				if(psy->get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &psy_data))
				{
					printk("%s fail to get property\n", __func__);
					goto ss6000_power_supply_failed;
				}

				if(psy_data.intval >= 90)
				{
#if defined(CONFIG_SPA)
					if(spa_external_event)
					{
						spa_external_event(SPA_CATEGORY_BATTERY, SPA_BATTERY_EVENT_CHARGE_FULL);		
					}
#endif
				}
				else
				{
					printk("%s battery capacity %d\n", __func__, psy_data.intval);
					printk("%s skip battery full event because battery capacity is less than 99\n", __func__);
					goto ss6000_skip_battery_full_event;
				}
			}
			else
			{
				printk("%s skip wrong battery full event\n", __func__);	
				goto ss6000_skip_battery_full_event;
			}
		}
		else
		{
		}
	}
	prev_chgsb_status = chgsb;
	wake_unlock(&pdata->chgsb_irq_wakelock);
	return;

ss6000_power_supply_failed:
ss6000_skip_battery_full_event:
	prev_chgsb_status = chgsb;
	wake_unlock(&pdata->chgsb_irq_wakelock);
	if(is_charge_enabled)
	{
		ss6000_restart_charge(current_charger_type);
	}
}

static void ss6000_pgb_irq_work(struct work_struct *work)
{
	struct ss6000_platform_data *pdata = container_of(work, struct ss6000_platform_data, pgb_irq_work.work);
	int pgb = 0, chgsb = 0;

	pgb = gpio_get_value(pdata->pgb)? 1 : 0;
	chgsb = gpio_get_value(pdata->chgsb) ? 1 : 0;

	printk("%s pgb : %d chgsb : %d\n", __func__, pgb, chgsb);
	
	if(pgb)
	{
                if(chgsb)
                {
#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				spa_external_event(SPA_CATEGORY_BATTERY, SPA_BATTERY_EVENT_OVP_CHARGE_STOP);
			}
#endif
                }
                else
                {
                        printk("%s wrong status. please check pgb, chgsb status.\n", __func__);
                }

	}
	else
	{
#if defined(CONFIG_SPA)
		if(spa_external_event)
		{
			spa_external_event(SPA_CATEGORY_BATTERY, SPA_BATTERY_EVENT_OVP_CHARGE_RESTART);
		}
#endif
	}

	wake_unlock(&pdata->pgb_irq_wakelock);
}

static void ss6000_check_init_status(struct ss6000_platform_data *pdata)
{
	int pgb = 0 , chgsb = 0;

	pgb = gpio_get_value(pdata->pgb)? 1 : 0;
	chgsb = gpio_get_value(pdata->chgsb) ? 1 : 0;	

	printk("%s pgb : %d chgsb : %d\n", __func__, pgb, chgsb);

	if(pgb)
	{
		if(chgsb)
		{
			if(is_charge_enabled)
			{
#if defined(CONFIG_SPA)
				if(spa_external_event)
				{
					spa_external_event(SPA_CATEGORY_BATTERY, SPA_BATTERY_EVENT_OVP_CHARGE_STOP);		
				}
#endif
			}	
		}
		else
		{
			printk("%s wrong status. please check pgb, chgsb status.\n", __func__);
		}	
	}
	else
	{
		if(chgsb)
		{
			if(is_charge_enabled)
			{
#if defined(CONFIG_SPA)
				if(spa_external_event)
				{
					spa_external_event(SPA_CATEGORY_BATTERY, SPA_BATTERY_EVENT_CHARGE_FULL);		
				}
#endif
			}
		}
		else
		{
		}
	}
	prev_chgsb_status = chgsb;
}

static irqreturn_t ss6000_chgsb_irq_handler(int irq, void *dev_id)
{
	struct ss6000_platform_data *pdata = dev_id; 

printk("ss6000_chgsb_irq_handler\n");

	if(delayed_work_pending(&pdata->chgsb_irq_work))
	{
		printk("%s cancel previous irq work\n", __func__);
		wake_unlock(&pdata->chgsb_irq_wakelock);
		cancel_delayed_work_sync(&pdata->chgsb_irq_work);
	}

	wake_lock(&pdata->chgsb_irq_wakelock);
	schedule_delayed_work(&pdata->chgsb_irq_work, SS6000_CHGSB_IRQ_DELAY);

	return IRQ_HANDLED;
}

static irqreturn_t ss6000_pgb_irq_handler(int irq, void *dev_id)
{
	struct ss6000_platform_data *pdata = dev_id; 

	if(delayed_work_pending(&pdata->pgb_irq_work))
	{
		printk("%s cancel previous irq work\n", __func__);
		wake_unlock(&pdata->pgb_irq_wakelock);
		cancel_delayed_work_sync(&pdata->pgb_irq_work);
	}

	wake_lock(&pdata->pgb_irq_wakelock);
	schedule_delayed_work(&pdata->pgb_irq_work, SS6000_PGB_IRQ_DELAY);

	return IRQ_HANDLED;
}
static __devinit int ss6000_probe(struct platform_device *pdev)
{
	struct ss6000_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;
	unsigned int pgb = 0, chgsb = 0;

	printk("%s\n",__func__);

	INIT_DELAYED_WORK(&pdata->chgsb_irq_work, ss6000_chgsb_irq_work);
	wake_lock_init(&pdata->chgsb_irq_wakelock, WAKE_LOCK_SUSPEND, "ss6000_chgsb_irq");
	INIT_DELAYED_WORK(&pdata->pgb_irq_work, ss6000_pgb_irq_work);
	wake_lock_init(&pdata->pgb_irq_wakelock, WAKE_LOCK_SUSPEND, "ss6000_pgb_irq");
	spin_lock_init(&change_current_lock);

	en_set = pdata->en_set; 
	ret = gpio_request(en_set, "ss6000_charger en/set");
	if(ret)
	{
		printk("%s %d gpio request failed\n", __func__, en_set);
		goto err_request_gpio_ss6000_en_set;
	}
	gpio_direction_output(en_set, 0);		

	pgb = pdata->pgb;
	ret = gpio_request(pgb, "ss6000_charger pgb");
	if(ret)
	{
		printk("%s %d gpio request failed\n", __func__, pgb);
		goto err_request_gpio_ss6000_pgb;
	}
	gpio_direction_input(pgb);
	pdata->irq_pgb = gpio_to_irq(pgb);

	chgsb = pdata->chgsb;
	ret = gpio_request(chgsb, "ss6000_charger chgsb");
	if(ret)
	{
		printk("%s %d gpio request failed\n", __func__, chgsb);
		goto err_request_gpio_ss6000_chgsb;
	}
	gpio_direction_input(chgsb);
	pdata->irq_chgsb = gpio_to_irq(chgsb);

	ss6000_disable_charge();

#if defined(CONFIG_SPA)
	ret = spa_chg_register_enable_charge(ss6000_enable_charge);
	if(ret)
	{
		printk("%s fail to register enable_charge function\n", __func__);
		goto err_register_enable_charge;	
	}
	ret = spa_chg_register_disable_charge(ss6000_disable_charge);
	if(ret)
	{
		printk("%s fail to register disable_charge function\n", __func__);
		goto err_register_disable_charge;
	}
	spa_external_event = spa_get_external_event_handler();
#endif	

	msleep(300);

	ss6000_check_init_status(pdata);
	ret = request_irq(pdata->irq_chgsb, ss6000_chgsb_irq_handler, 
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "SS6000_CHGSB", pdata); 
	if(ret)
	{
		printk("%s %d request_irq failed\n", __func__, chgsb);
		goto err_request_irq_ss6000_chgsb; 
	}

	ret = request_irq(pdata->irq_pgb, ss6000_pgb_irq_handler, 
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "SS6000_PGB", pdata); 

	if(ret)
	{
		printk("%s %d request_irq failed\n", __func__, pgb);
		goto err_request_irq_ss6000_pgb; 
	}

	return 0;

err_request_irq_ss6000_pgb:
	gpio_free(pgb);
	free_irq(gpio_to_irq(chgsb), pdata);
err_request_irq_ss6000_chgsb: 
#if defined(CONFIG_SPA)
err_register_disable_charge:
err_register_enable_charge:
#endif
	gpio_free(chgsb);
err_request_gpio_ss6000_chgsb:
	gpio_free(pgb);
err_request_gpio_ss6000_pgb:
	gpio_free(en_set);
err_request_gpio_ss6000_en_set:
	return ret;
}

static int __devexit ss6000_remove(struct platform_device *pdev)
{
	printk("%s\n",__func__);
	return 0;
}


#ifdef CONFIG_PM
static int ss6000_suspend(struct device *dev)
{
	printk("%s\n",__func__);
	return 0;
}

static int ss6000_resume(struct device *dev)
{
	printk("%s\n",__func__);
	return 0;
}
static struct dev_pm_ops ss6000_pm_ops = {
	.suspend	= ss6000_suspend,
	.resume		= ss6000_resume,
}; 
#endif

/* Probe & Remove function */
static struct platform_driver ss6000_driver = {
        .driver         = {
                .name   = "ss6000_charger",
                .owner  = THIS_MODULE,
#ifdef CONFIG_PM
                .pm     = &ss6000_pm_ops,
#endif
        },
        .probe          = ss6000_probe,
        .remove         = __devexit_p(ss6000_remove),
};


static int __init ss6000_init(void)
{
	int retVal = -EINVAL;
	printk(KERN_ALERT "%s\n", __func__);

	retVal = platform_driver_register(&ss6000_driver);

	return (retVal);
}

static void __exit ss6000_exit(void)
{
}

module_init(ss6000_init);
module_exit(ss6000_exit);

MODULE_AUTHOR("YongTaek Lee <ytk.lee@samsung.com>");
MODULE_DESCRIPTION("Linux Driver for charger-IC");
MODULE_LICENSE("GPL");

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
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <linux/earlysuspend.h>

#define KPLED_DBG 0

#ifdef  KPLED_DBG
#define KPLED_DBG(fmt, arg...)  printk(KERN_ERR "%s: " fmt "\n" , __func__ , ## arg)
#else
#define KPLED_DBG(fmt, arg...) 
#endif

#define SPRD_ANA_BASE 	        (SPRD_MISC_BASE + 0x600)
#define ANA_REG_BASE            SPRD_ANA_BASE
#ifndef CONFIG_ARCH_SC8825
#define ANA_LED_CTRL           (ANA_REG_BASE + 0X68)
#else
#define ANA_LED_CTRL           (ANA_REG_BASE + 0X70)
#endif
#define KPLED_CTL               ANA_LED_CTRL
#define KPLED_PD_SET            (1 << 11)
#define KPLED_PD_RST            (1 << 12)
#define KPLED_V_SHIFT           7
#define KPLED_V_MSK             (0x07 << KPLED_V_SHIFT)

/* sprd keypad backlight */
struct sprd_kb_led {
	struct platform_device *dev;
	struct mutex mutex;
	struct work_struct work;
	spinlock_t value_lock;
	enum led_brightness value;
	struct led_classdev cdev;
	int enabled;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif

};

#define to_sprd_led(led_cdev) \
	container_of(led_cdev, struct sprd_kb_led, cdev)

static inline uint32_t kpled_read(uint32_t reg)
{
	return sci_adi_read(reg);
}
	
static void Kb_SetBackLightBrightness( unsigned long  value)
{
	if(value > 255)
		value = 255;
	
	if(value > 32)
		value = value/32;
	else
		value = 0;
	
	// Set Output Current
	sci_adi_write(KPLED_CTL, ((value << KPLED_V_SHIFT) & KPLED_V_MSK), KPLED_V_MSK);
	KPLED_DBG("Set KPLED_CTL : 0x%x \n", kpled_read(KPLED_CTL) );
}

static void sprd_led_enable(struct sprd_kb_led *led)
{
	/* backlight on */
	sci_adi_clr(KPLED_CTL, KPLED_PD_SET|KPLED_PD_RST);
	sci_adi_set(KPLED_CTL, KPLED_PD_RST);

	Kb_SetBackLightBrightness(led->value);
	
	led->enabled = 1;
}

static void sprd_led_disable(struct sprd_kb_led *led)
{
	/* backlight off */
	sci_adi_clr(KPLED_CTL, KPLED_PD_SET|KPLED_PD_RST);
	sci_adi_set(KPLED_CTL, KPLED_PD_SET);

	Kb_SetBackLightBrightness(led->value);
	led->enabled = 0;
}

static void led_work(struct work_struct *work)
{
	struct sprd_kb_led *led = container_of(work, struct sprd_kb_led, work);
	unsigned long flags;

	mutex_lock(&led->mutex);
	spin_lock_irqsave(&led->value_lock, flags);
	if (led->value == LED_OFF) {
		spin_unlock_irqrestore(&led->value_lock, flags);
		sprd_led_disable(led);
		goto out;
	}
	spin_unlock_irqrestore(&led->value_lock, flags);
	sprd_led_enable(led);
out:
	mutex_unlock(&led->mutex);
}

static void sprd_led_set(struct led_classdev *led_cdev,
			   enum led_brightness value)
{
	struct sprd_kb_led *led = to_sprd_led(led_cdev);
	unsigned long flags;
	
	spin_lock_irqsave(&led->value_lock, flags);
	led->value = value;
	spin_unlock_irqrestore(&led->value_lock, flags);
	schedule_work(&led->work);	
}

static void sprd_kb_led_shutdown(struct platform_device *dev)
{
	struct sprd_kb_led *led = platform_get_drvdata(dev);

	mutex_lock(&led->mutex);
	led->value = LED_OFF;
	led->enabled = 1;
	sprd_led_disable(led);
	mutex_unlock(&led->mutex);
}

#ifdef CONFIG_EARLYSUSPEND
static void sprd_led_early_suspend(struct early_suspend *es)
{
	struct sprd_kb_led *led = container_of(es, struct sprd_kb_led, early_suspend);
	printk(KERN_INFO "sprd_led_early_suspend\n");
	sprd_led_disable(led);
}

static void sprd_led_late_resume(struct early_suspend *es)
{
 	struct sprd_kb_led *led = container_of(es, struct sprd_kb_led, early_suspend);
	printk(KERN_INFO "sprd_led_late_resume\n");
	sprd_led_enable(led);
}
#endif


static int sprd_kb_led_probe(struct platform_device *dev)
{
	struct sprd_kb_led *led;
	int ret;

	led = kzalloc(sizeof(struct sprd_kb_led), GFP_KERNEL);
	if (led == NULL) {
		dev_err(&dev->dev, "No memory for device\n");
		return -ENOMEM;
	}

	led->cdev.brightness_set = sprd_led_set;
	//led->cdev.default_trigger = "heartbeat";
	led->cdev.default_trigger = "none";
	led->cdev.name = "keyboard-backlight";
	led->cdev.brightness_get = NULL;
	led->cdev.flags |= LED_CORE_SUSPENDRESUME;
	led->enabled = 0;

	spin_lock_init(&led->value_lock);
	mutex_init(&led->mutex);
	INIT_WORK(&led->work, led_work);
	led->value = LED_OFF;
	platform_set_drvdata(dev, led);

	/* register our new led device */

	ret = led_classdev_register(&dev->dev, &led->cdev);
	if (ret < 0) {
		dev_err(&dev->dev, "led_classdev_register failed\n");
		kfree(led);
		return ret;
	}
	
	led->value = LED_FULL;
	led->enabled = 0;
#ifdef CONFIG_HAS_EARLYSUSPEND
	led->early_suspend.suspend = sprd_led_early_suspend;
	led->early_suspend.resume  = sprd_led_late_resume;
	led->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&led->early_suspend);
#endif
	schedule_work(&led->work);

	return 0;
}

static int sprd_kb_led_remove(struct platform_device *dev)
{
	struct sprd_kb_led *led = platform_get_drvdata(dev);

	led_classdev_unregister(&led->cdev);
	flush_scheduled_work();
	led->value = LED_OFF;
	led->enabled = 1;
	sprd_led_disable(led);
	kfree(led);

	return 0;
}

static struct platform_driver sprd_kb_led_driver = {
	.driver = {
		.name  = "keyboard-backlight",
		.owner = THIS_MODULE,
	},
	.probe    = sprd_kb_led_probe,
	.remove   = sprd_kb_led_remove,
	.shutdown = sprd_kb_led_shutdown,
};

static int __devinit sprd_kb_led_init(void)
{
	return platform_driver_register(&sprd_kb_led_driver);
}

static void sprd_kb_led_exit(void)
{
	platform_driver_unregister(&sprd_kb_led_driver);
}

module_init(sprd_kb_led_init);
module_exit(sprd_kb_led_exit);

MODULE_AUTHOR("Ye Wu <ye.wu@spreadtrum.com>");
MODULE_DESCRIPTION("Sprd Keyboard backlight driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:keyboard-backlight");


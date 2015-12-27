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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#ifdef CONFIG_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/* register definitions */
#define	BK_MOD_MAX	0xff
#define	BK_LOW		0
#define	BK_HIGH		1

enum {
	CMD_OFF	= 0,
	CMD_PULSE
};

static unsigned long step_bak = 0;
static unsigned long last_backlight = 0;
static unsigned int  bl_suspend = 0;

static struct early_suspend early_suspend_desc;

static void send_cmd(int cmd, int count)
{
	unsigned long flags;

	switch(cmd) {
	case CMD_PULSE:
		if (step_bak == 0 ){
			gpio_set_value(GPIO_BK, BK_HIGH);
			udelay(100);
		}
		while(count-- > 0) {
			local_irq_save(flags);
			gpio_set_value(GPIO_BK, BK_LOW);
			udelay(10);
			gpio_set_value(GPIO_BK, BK_HIGH);
			local_irq_restore(flags);
			udelay(10);
		}
		break;
	case CMD_OFF:
		gpio_set_value(GPIO_BK, BK_LOW);
		mdelay(4);
		break;
	default:
		break;
	}
}

static void gsm3727_backlight_set(int is_on,int val)
{
	uint32_t value;
	uint32_t value_ret;

	if (is_on) {
		value = val;
		if(value > 255)
			value = 255;
		if(value < 8)
			value = 8;
		value_ret = 32 - value/8;

		send_cmd(CMD_PULSE, (value_ret - step_bak + 32) & 31);

		step_bak = value_ret;
	} else {
		send_cmd(CMD_OFF, 0);
		step_bak = 0;
	}
}

static int gsm3727_backlight_update_status(struct backlight_device *bldev)
{
	last_backlight=bldev->props.brightness & BK_MOD_MAX;

	if ((bldev->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK)) ||
			bldev->props.power != FB_BLANK_UNBLANK ||
			bldev->props.brightness == 0) {
		gsm3727_backlight_set(0,0);
	} else {
#ifdef CONFIG_EARLYSUSPEND
		if (!bl_suspend )
#endif
		{
			gsm3727_backlight_set(1,last_backlight);
		}
	}

	return 0;
}

static int gsm3727_backlight_get_brightness(struct backlight_device *bldev)
{
	return 0;
}

static const struct backlight_ops gsm3727_backlight_ops = {
	.update_status = gsm3727_backlight_update_status,
	.get_brightness = gsm3727_backlight_get_brightness,
};

#ifdef CONFIG_EARLYSUSPEND
static void gsm3727_backlight_earlysuspend(struct early_suspend *h)
{
	bl_suspend = 1;
}

static void gsm3727_backlight_lateresume(struct early_suspend *h)
{
	bl_suspend = 0;
	gsm3727_backlight_set(1,last_backlight);
}
#endif


static int __devinit gsm3727_backlight_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct backlight_device *bldev;
	int ret;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = BK_MOD_MAX;
	props.type = BACKLIGHT_RAW;
	props.brightness = BK_MOD_MAX;
	props.power = FB_BLANK_UNBLANK;


	bldev = backlight_device_register(
			dev_name(&pdev->dev), &pdev->dev,
			NULL, &gsm3727_backlight_ops, &props);
	if (IS_ERR(bldev)) {
		printk(KERN_ERR "Failed to register backlight device\n");
		return -ENOMEM;
	}

	if (gpio_is_valid(GPIO_BK)) {
		ret = gpio_request(GPIO_BK, "Back Light Data");
		if (ret) {
			printk(KERN_ERR "Failed requesting Back Light Data %d\n", GPIO_BK);
			goto err_bk_gpio;
		}
		gpio_direction_output(GPIO_BK, BK_HIGH);
	}

	platform_set_drvdata(pdev, bldev);

#ifdef CONFIG_EARLYSUSPEND
	early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	early_suspend_desc.suspend = gsm3727_backlight_earlysuspend;
	early_suspend_desc.resume  = gsm3727_backlight_lateresume;
	register_early_suspend(&early_suspend_desc);
#endif

	return 0;

err_bk_gpio:
	gpio_free(GPIO_BK);
	return ret;
}

static int __devexit gsm3727_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bldev;

	bldev = platform_get_drvdata(pdev);
	bldev->props.power = FB_BLANK_UNBLANK;
	bldev->props.brightness = BK_MOD_MAX;
	backlight_update_status(bldev);
	backlight_device_unregister(bldev);
	platform_set_drvdata(pdev, NULL);

#ifdef CONFIG_EARLYSUSPEND
	unregister_early_suspend(&early_suspend_desc);
#endif
	return 0;
}

#ifdef CONFIG_PM
static int gsm3727_backlight_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	return 0;
}

static int gsm3727_backlight_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define gsm3727_backlight_suspend NULL
#define gsm3727_backlight_resume NULL
#endif

static struct platform_driver gsm3727_backlight_driver = {
	.probe = gsm3727_backlight_probe,
	.remove = __devexit_p(gsm3727_backlight_remove),
	.suspend = gsm3727_backlight_suspend,
	.resume = gsm3727_backlight_resume,
	.driver = {
		.name = "sprd_backlight",
		.owner = THIS_MODULE,
	},
};

static int __init gsm3727_backlight_init(void)
{
	return platform_driver_register(&gsm3727_backlight_driver);
}

static void __exit gsm3727_backlight_exit(void)
{
	platform_driver_unregister(&gsm3727_backlight_driver);
}

module_init(gsm3727_backlight_init);
module_exit(gsm3727_backlight_exit);

MODULE_DESCRIPTION("gsm3727 Backlight Driver");
MODULE_LICENSE("GPL v2");

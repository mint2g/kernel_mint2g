/*
 ****************************************************************
 *
 *  Component:	VirtualLogix VLED
 *
 *  Copyright (C) 2009, VirtualLogix. All Rights Reserved.
 *
 *  Contributor(s):
 *
 ****************************************************************
 */

/*----- System header files -----*/

#include <linux/module.h>	/* __exit, __init */
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
#include <linux/semaphore.h>
#endif
#include <linux/init.h>		/* module_init() in 2.6.0 and before */
#include <nk/nkern.h>

struct vled_data {
    struct led_classdev cdev;
    enum led_brightness value;
};

static void vled_set(struct led_classdev *led_cdev,
			 enum led_brightness value)
{
    struct vled_data *led_dat =
	container_of(led_cdev, struct vled_data, cdev);
    led_dat->value = value;
}

#ifdef OLD
static int vblink_set(struct led_classdev *led_cdev,
			  unsigned long *delay_on, unsigned long *delay_off)
{
     return 0;
}

#endif
static int vled_probe(struct platform_device *pdev)
{
    struct led_platform_data *pdata = pdev->dev.platform_data;
    struct led_info *cur_led;
    struct vled_data *leds_data, *led_dat;
    int i, ret = 0;

    if (!pdata)
	return -EBUSY;

    leds_data = kzalloc(sizeof(struct vled_data) * pdata->num_leds,
			GFP_KERNEL);
    if (!leds_data)
	return -ENOMEM;

    for (i = 0; i < pdata->num_leds; i++) {
	cur_led = &pdata->leds[i];
	led_dat = &leds_data[i];

	led_dat->cdev.name = cur_led->name;
	led_dat->cdev.default_trigger = cur_led->default_trigger;
	led_dat->cdev.brightness_set = vled_set;
	led_dat->cdev.brightness = LED_OFF;
	led_dat->cdev.flags |= LED_CORE_SUSPENDRESUME;

	ret = led_classdev_register(&pdev->dev, &led_dat->cdev);
	if (ret < 0) {
	    goto err;
	}
    }

    platform_set_drvdata(pdev, leds_data);

    return 0;

err:
    if (i > 0) {
	for (i = i - 1; i >= 0; i--) {
	    led_classdev_unregister(&leds_data[i].cdev);
	}
    }

    kfree(leds_data);

    return ret;
}

static int __devexit vled_remove(struct platform_device *pdev)
{
    int i;
    struct led_platform_data *pdata = pdev->dev.platform_data;
    struct vled_data *leds_data;

    leds_data = (struct vled_data*) platform_get_drvdata(pdev);

    for (i = 0; i < pdata->num_leds; i++) {
	led_classdev_unregister(&leds_data[i].cdev);
    }

    kfree(leds_data);

    return 0;
}

static struct platform_driver vled_driver = {
    .probe		= vled_probe,
    .remove		= __devexit_p(vled_remove),
    .driver		= {
	.name	= "vleds",
	.owner	= THIS_MODULE,
    },
};

static int __init vled_init(void)
{
    return platform_driver_register(&vled_driver);
}

static void __exit vled_exit(void)
{
    platform_driver_unregister(&vled_driver);
}

module_init(vled_init);
module_exit(vled_exit);

MODULE_AUTHOR("Raphael Assenat <raph@8d.com>");
MODULE_DESCRIPTION("Virtual LED driver");
MODULE_LICENSE("GPL");

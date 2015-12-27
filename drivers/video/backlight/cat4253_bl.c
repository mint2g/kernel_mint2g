/*
 * linux/drivers/video/backlight/s2c_bl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*******************************************************************************
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* 	@file	drivers/video/backlight/s2c_bl.c
*
* Unless you and Broadcom execute a separate written software license agreement
* governing use of this software, this software is licensed to you under the
* terms of the GNU General Public License version 2, available at
* http://www.gnu.org/copyleft/gpl.html (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include "cat4253_bl.h"

int current_intensity;
static int backlight_pin = 138;

static DEFINE_SPINLOCK(bl_ctrl_lock);
int real_level = 1;

#ifdef CONFIG_HAS_EARLYSUSPEND
/* early suspend support */
#endif
static int backlight_mode=1;

#define MAX_BRIGHTNESS_IN_BLU	33

#define DIMMING_VALUE		32

#define MAX_BRIGHTNESS_VALUE	255
#define MIN_BRIGHTNESS_VALUE	20
#define BACKLIGHT_DEBUG 1
#define BACKLIGHT_SUSPEND 0
#define BACKLIGHT_RESUME 1

#if BACKLIGHT_DEBUG
#define BLDBG(fmt, args...) printk(fmt, ## args)
#else
#define BLDBG(fmt, args...)
#endif

struct cat4253_bl_data {
	struct platform_device *pdev;
	unsigned int ctrl_pin;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_desc;
#endif
};

struct brt_value{
	int level;				// Platform setting values
	int tune_level;			// Chip Setting values
};

struct brt_value brt_table_ktd[] = {
  { MIN_BRIGHTNESS_VALUE,  32 }, // Min pulse 32
   { 32,  31 },
   { 46,  30 },
   { 60,  29 },  
   { 73,  28 }, 
   { 86,  27 }, 
   { 98,  26 }, 
   { 105,  25 }, 
   { 110,  24 }, 
   { 115,  23 }, 
   { 120,  22 }, 
   { 125,  21 }, 
   { 130,  20 }, 
   { 140,  19 },//default value  
   { 155,  18 },   
   { 165,  17 },
   { 176,  16 }, 
   { 191,  15 }, 
   { 207,  14 },
   { 214,  13 },
   { 221,  12 },
   { 228,  10 },
   { 235,  8 },
   { 242,  7 },
   { 249,  5 },
   { MAX_BRIGHTNESS_VALUE,  3 }, // Max pulse 1
};


#define MAX_BRT_STAGE_KTD (int)(sizeof(brt_table_ktd)/sizeof(struct brt_value))


static void lcd_backlight_control(int num)
{
	int limit;

	limit = num;

	spin_lock(&bl_ctrl_lock);
	for(;limit>0;limit--)
	{
		udelay(2);
		gpio_set_value(backlight_pin,0);
		udelay(2); 
		gpio_set_value(backlight_pin,1);		
	}
	spin_unlock(&bl_ctrl_lock);

}

/* input: intensity in percentage 0% - 100% */
static int cat4253_backlight_update_status(struct backlight_device *bd)
{

	int user_intensity = bd->props.brightness;
    	int tune_level = 0;
	int pulse;
	int i;
	
	BLDBG("[BACKLIGHT] cat4253_backlight_update_status ==> user_intensity  : %d\n", user_intensity);


      if(backlight_mode==BACKLIGHT_RESUME){

    		if(user_intensity > 0) {
			if(user_intensity < MIN_BRIGHTNESS_VALUE) {
				tune_level = DIMMING_VALUE; //DIMMING
			} else if (user_intensity == MAX_BRIGHTNESS_VALUE) {
				tune_level = brt_table_ktd[MAX_BRT_STAGE_KTD-1].tune_level;
			} else {
				for(i = 0; i < MAX_BRT_STAGE_KTD; i++) {
					if(user_intensity <= brt_table_ktd[i].level ) {
						tune_level = brt_table_ktd[i].tune_level;
							break;
						}
					}
				}
			}

		BLDBG("[BACKLIGHT] cat4253_backlight_update_status ==> user = %d, tune = %d\n", user_intensity, tune_level);

    if (real_level==tune_level)
      {
	    return 0;
         }
     else
	{
    if(real_level == 0)
	 {
                gpio_set_value(backlight_pin,1);
                mdelay(2); 
         }
 
	    if(tune_level<=0)
	    {
                gpio_set_value(backlight_pin,0);
                mdelay(10); 

			    }
	    else
	    {
    		if( real_level<=tune_level)
    		{
    			pulse = tune_level - real_level;
    		}
		else
				{
			pulse = 32 - (real_level - tune_level);
				}

    		//pulse = MAX_BRIGHTNESS_IN_BLU -tune_level;
            if (pulse==0)
				{
                return 0;
				}

				lcd_backlight_control(pulse);
    }
				
    real_level = tune_level;
				return 0;
		    }

	    }
       return 0;
	}


static int cat4253_backlight_get_brightness(struct backlight_device *bl)
{
	BLDBG("[BACKLIGHT] cat4253_backlight_get_brightness\n");
    
	return current_intensity;
}

static struct backlight_ops cat4253_backlight_ops = {
	.update_status	= cat4253_backlight_update_status,
	.get_brightness	= cat4253_backlight_get_brightness,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cat4253_backlight_earlysuspend(struct early_suspend *desc)
{
 	
	backlight_mode=BACKLIGHT_SUSPEND;
	gpio_set_value(backlight_pin,0);
        mdelay(10); 
	real_level=0;
    printk("[BACKLIGHT] cat4253_backlight_earlysuspend\n");
}

static void cat4253_backlight_earlyresume(struct early_suspend *desc)
{
	struct cat4253_bl_data *cat4253 = container_of(desc, struct cat4253_bl_data, early_suspend_desc);
	struct backlight_device *bl = platform_get_drvdata(cat4253->pdev);

	backlight_mode=BACKLIGHT_RESUME;
    printk("[BACKLIGHT] cat4253_backlight_earlyresume\n");
    
       msleep(200); 	
    backlight_update_status(bl);
}
#else
#ifdef CONFIG_PM
static int cat4253_backlight_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct cat4253_bl_data *cat4253 = dev_get_drvdata(&bl->dev);
    
	printk("[BACKLIGHT] cat4253_backlight_suspend\n");
        
	return 0;
}

static int cat4253_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	printk("[BACKLIGHT] cat4253_backlight_resume\n");
        
	backlight_update_status(bl);
        
	return 0;
}
#else
#define cat4253_backlight_suspend  NULL
#define cat4253_backlight_resume   NULL
#endif
#endif

static int cat4253_backlight_probe(struct platform_device *pdev)
{
	struct platform_cat4253_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct cat4253_bl_data *cat4253;
	struct backlight_properties props;
	
	int ret;

	printk("[BACKLIGHT] cat4253_backlight_probe\n");
    
	if (!data) 
	{
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	cat4253 = kzalloc(sizeof(*cat4253), GFP_KERNEL);
	if (!cat4253) 
	{
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	cat4253->ctrl_pin = data->ctrl_pin;
    
	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = data->max_brightness;

	bl = backlight_device_register(pdev->name, &pdev->dev, cat4253, &cat4253_backlight_ops, &props);
	if (IS_ERR(bl)) 
	{
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

	gpio_request(backlight_pin,"lcd_backlight");
#ifdef CONFIG_HAS_EARLYSUSPEND
	cat4253->pdev = pdev;
	cat4253->early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	cat4253->early_suspend_desc.suspend = cat4253_backlight_earlysuspend;
	cat4253->early_suspend_desc.resume = cat4253_backlight_earlyresume;
	register_early_suspend(&cat4253->early_suspend_desc);
#endif

	bl->props.max_brightness = data->max_brightness;
	bl->props.brightness = data->dft_brightness;

	platform_set_drvdata(pdev, bl);

	cat4253_backlight_update_status(bl);
    
	return 0;

err_bl:
	kfree(cat4253);
err_alloc:
	return ret;
}

static int cat4253_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct cat4253_bl_data *cat4253 = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);


#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&cat4253->early_suspend_desc);
#endif

	kfree(cat4253);

	gpio_free(backlight_pin);

	return 0;
}


static struct platform_driver cat4253_backlight_driver = {
	.driver		= {
		.name	= "panel",
		.owner	= THIS_MODULE,
	},
	.probe		= cat4253_backlight_probe,
	.remove		= cat4253_backlight_remove,

#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend        = cat4253_backlight_suspend,
	.resume         = cat4253_backlight_resume,
#endif

};

static int __init cat4253_backlight_init(void)
{
	return platform_driver_register(&cat4253_backlight_driver);
}
module_init(cat4253_backlight_init);

static void __exit cat4253_backlight_exit(void)
{
	platform_driver_unregister(&cat4253_backlight_driver);
}
module_exit(cat4253_backlight_exit);

MODULE_DESCRIPTION("cat4253 based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:cat4253-backlight");


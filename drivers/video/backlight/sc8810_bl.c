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
#include <linux/clk.h>
#include <mach/hardware.h>
#include <linux/earlysuspend.h>

/* register definitions */
#define        PWM_PRESCALE    (0x0000)
#define        PWM_CNT         (0x0004)
#define        PWM_TONE_DIV    (0x0008)
#define        PWM_PAT_LOW     (0x000C)
#define        PWM_PAT_HIG     (0x0010)

#define        PWM_ENABLE      (1 << 8)
#define        PWM_SCALE       1
#define        PWM_REG_MSK     0xffff
#define        PWM_MOD_MAX     0xff

struct sc8810bl {
       int             pwm;
       uint32_t        value;
       int             suspend;
       struct clk      *clk;
       struct mutex    mutex;
       struct early_suspend sprd_early_suspend_desc;
};

static struct sc8810bl sc8810bl;

static inline uint32_t pwm_read(int index, uint32_t reg)
{
       return __raw_readl(SPRD_PWM_BASE + index * 0x20 + reg);
}

static void pwm_write(int index, uint32_t value, uint32_t reg)
{
       __raw_writel(value, SPRD_PWM_BASE + index * 0x20 + reg);
}

#ifdef CONFIG_EARLYSUSPEND
static void sc8810_backlight_earlysuspend(struct early_suspend *h)
{
       mutex_lock(&sc8810bl.mutex);
       sc8810bl.suspend = 1;
       mutex_unlock(&sc8810bl.mutex);
}

static void sc8810_backlight_lateresume(struct early_suspend *h)
{
       mutex_lock(&sc8810bl.mutex);
       sc8810bl.suspend = 0;
       mutex_unlock(&sc8810bl.mutex);

       pwm_write(sc8810bl.pwm, PWM_SCALE, PWM_PRESCALE);
       pwm_write(sc8810bl.pwm, sc8810bl.value, PWM_CNT);
       pwm_write(sc8810bl.pwm, PWM_REG_MSK, PWM_PAT_LOW);
       pwm_write(sc8810bl.pwm, PWM_REG_MSK, PWM_PAT_HIG);
       pwm_write(sc8810bl.pwm, PWM_SCALE | PWM_ENABLE, PWM_PRESCALE);
}
#endif

static int sc8810_backlight_update_status(struct backlight_device *bldev)
{
       struct sc8810bl *bl = bl_get_data(bldev);
       uint32_t value;

       value = bldev->props.brightness & PWM_MOD_MAX;
       if(value <0x20){
	       value = 0x20;
       }
       value = (value << 8) | PWM_MOD_MAX;
       sc8810bl.value = value;

       if ((bldev->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK)) ||
                       bldev->props.power != FB_BLANK_UNBLANK ||
                       sc8810bl.suspend ||
                       bldev->props.brightness == 0) {
               /* disable backlight */
               pwm_write(bl->pwm, 0, PWM_PRESCALE);
       } else {

               pwm_write(bl->pwm, PWM_SCALE, PWM_PRESCALE);
               pwm_write(bl->pwm, value, PWM_CNT);
               pwm_write(bl->pwm, PWM_REG_MSK, PWM_PAT_LOW);
               pwm_write(bl->pwm, PWM_REG_MSK, PWM_PAT_HIG);

               pwm_write(bl->pwm, PWM_SCALE | PWM_ENABLE, PWM_PRESCALE);
       }

       return 0;
}

static int sc8810_backlight_get_brightness(struct backlight_device *bldev)
{
       struct sc8810bl *bl = bl_get_data(bldev);
       uint32_t brightness = pwm_read(bl->pwm, PWM_CNT) >> 8;

       return brightness & PWM_MOD_MAX;
}

static const struct backlight_ops sc8810_backlight_ops = {
       .update_status = sc8810_backlight_update_status,
       .get_brightness = sc8810_backlight_get_brightness,
};

static int __devinit sc8810_backlight_probe(struct platform_device *pdev)
{
       struct backlight_properties props;
       struct backlight_device *bldev;

       memset(&props, 0, sizeof(struct backlight_properties));
       props.max_brightness = PWM_MOD_MAX;
       props.type = BACKLIGHT_RAW;
       props.brightness = PWM_MOD_MAX;
       props.power = FB_BLANK_UNBLANK;

       /* use PWM0 now only, may need dynamic config in the future */
       sc8810bl.pwm = 0;
       sc8810bl.suspend = 0;
       sc8810bl.clk = clk_get(NULL, "clk_pwm0");
       if (!sc8810bl.clk) {
               printk(KERN_ERR "Failed to get clk_pwm0\n");
               return -EIO;
       }
       clk_enable(sc8810bl.clk);

       bldev = backlight_device_register(
                       dev_name(&pdev->dev), &pdev->dev,
                       &sc8810bl, &sc8810_backlight_ops, &props);
       if (IS_ERR(bldev)) {
               printk(KERN_ERR "Failed to register backlight device\n");
               return -ENOMEM;
       }

       platform_set_drvdata(pdev, bldev);

		mutex_init(&sc8810bl.mutex);

#ifdef CONFIG_EARLYSUSPEND
       sc8810bl.sprd_early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
       sc8810bl.sprd_early_suspend_desc.suspend = sc8810_backlight_earlysuspend;
       sc8810bl.sprd_early_suspend_desc.resume  = sc8810_backlight_lateresume;
       register_early_suspend(&sc8810bl.sprd_early_suspend_desc);
#endif

       return 0;
}

static int __devexit sc8810_backlight_remove(struct platform_device *pdev)
{
       struct backlight_device *bldev;

#ifdef CONFIG_EARLYSUSPEND
       unregister_early_suspend(&sc8810bl.sprd_early_suspend_desc);
#endif

       bldev = platform_get_drvdata(pdev);
       bldev->props.power = FB_BLANK_UNBLANK;
       bldev->props.brightness = PWM_MOD_MAX;
       backlight_update_status(bldev);
       backlight_device_unregister(bldev);
       platform_set_drvdata(pdev, NULL);
       clk_disable(sc8810bl.clk);
       clk_put(sc8810bl.clk);

       return 0;
}

#ifdef CONFIG_PM
static int sc8810_backlight_suspend(struct platform_device *pdev,
               pm_message_t state)
{
	clk_disable(sc8810bl.clk);
       return 0;
}

static int sc8810_backlight_resume(struct platform_device *pdev)
{
	clk_enable(sc8810bl.clk);
       return 0;
}
#else
#define sc8810_backlight_suspend NULL
#define sc8810_backlight_resume NULL
#endif

static struct platform_driver sc8810_backlight_driver = {
       .probe = sc8810_backlight_probe,
       .remove = __devexit_p(sc8810_backlight_remove),
       .suspend = sc8810_backlight_suspend,
       .resume = sc8810_backlight_resume,
       .driver = {
               .name = "sprd_backlight",
               .owner = THIS_MODULE,
       },
};

static int __init sc8810_backlight_init(void)
{
       return platform_driver_register(&sc8810_backlight_driver);
}

static void __exit sc8810_backlight_exit(void)
{
       platform_driver_unregister(&sc8810_backlight_driver);
}

module_init(sc8810_backlight_init);
module_exit(sc8810_backlight_exit);

MODULE_DESCRIPTION("SC8810 Backlight Driver");

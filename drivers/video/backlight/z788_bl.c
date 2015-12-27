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
#include <linux/delay.h>
#include<mach/globalregs.h>
#include <linux/earlysuspend.h>
#define GR_CLK_EN			(SPRD_GREG_BASE + 0x0074)
#define PIN_MOD_PWMA			(SPRD_CPC_BASE  + 0x03e0)
#define SPRD_PWM0_PRESCALE   		(SPRD_PWM_BASE + 0x0000)
#define SPRD_PWM0_CNT 			(SPRD_PWM_BASE + 0x0004)
#define SPRD_PWM0_PAT_LOW 	       	(SPRD_PWM_BASE + 0x000C)
#define SPRD_PWM0_PAT_HIG 	       	(SPRD_PWM_BASE + 0x0010)

#define PIN_PWM0_MOD_VALUE   0x20

/* register definitions */
#define        PWM_PRESCALE    (0x0000)
#define        PWM_CNT         (0x0004)
#define        PWM_TONE_DIV    (0x0008)
#define        PWM_PAT_LOW     (0x000C)
#define        PWM_PAT_HIG     (0x0010)

#define        PWM_ENABLE      (1 << 8)
#define        PWM_SCALE       8
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

static inline pwm_read(int index, uint32_t reg)
{
       return __raw_readl(SPRD_PWM_BASE + index * 0x20 + reg);
}

static void pwm_write(int index, uint32_t value, uint32_t reg)
{
       __raw_writel(value, SPRD_PWM_BASE + index * 0x20 + reg);
}

static inline void __raw_bits_or(unsigned int v, unsigned int a)
{
	__raw_writel((__raw_readl(a) | v), a);
}

static void z788_SetPwmRatio(unsigned short value)
{
	/*printk("sprd:  %s  ,value = 0x%x  --1\n",__func__,value);*/
	__raw_bits_or(CLK_PWM0_EN, GR_CLK_EN);
	__raw_bits_or(CLK_PWM0_SEL, GR_CLK_EN);
	__raw_bits_or(PIN_PWM0_MOD_VALUE, PIN_MOD_PWMA);
	__raw_writel(PWM_SCALE, SPRD_PWM0_PRESCALE);
	__raw_writel(value, SPRD_PWM0_CNT);
	__raw_writel(PWM_REG_MSK, SPRD_PWM0_PAT_LOW);
	__raw_writel(PWM_REG_MSK, SPRD_PWM0_PAT_HIG);

	__raw_bits_or(PWM_ENABLE, SPRD_PWM0_PRESCALE);

}

#ifdef CONFIG_EARLYSUSPEND
static void sc8810_backlight_earlysuspend(struct early_suspend *h)
{
	/*printk("sprd: --  %s  -- sc8810bl.value = 0x%x-- \n",__func__,sc8810bl.value);*/ 
       mutex_lock(&sc8810bl.mutex);
       sc8810bl.suspend = 1;
	clk_disable(sc8810bl.clk);
       mutex_unlock(&sc8810bl.mutex);
}

static void sc8810_backlight_lateresume(struct early_suspend *h)
{
	/*printk("sprd: --  %s  -- sc8810bl.value = 0x%x--\n",__func__,sc8810bl.value);*/ 
       mutex_lock(&sc8810bl.mutex);
       clk_enable(sc8810bl.clk);
       sc8810bl.suspend = 0;
	z788_SetPwmRatio(sc8810bl.value);
       mutex_unlock(&sc8810bl.mutex);
}
#endif

static int sc8810_backlight_update_status(struct backlight_device *bldev)
{
       struct sc8810bl *bl = bl_get_data(bldev);
       uint32_t value;
	/*printk("sprd:  %s  --2\n",__func__);*/

       mutex_lock(&sc8810bl.mutex);
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
		z788_SetPwmRatio(0);
       } else {
	       z788_SetPwmRatio(value);
       }
       mutex_unlock(&sc8810bl.mutex);
	   /*printk("sprd:  %s  -- sc8810bl.value = 0x%x--\n",__func__,sc8810bl.value);*/ 
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
	printk("sprd: --  %s  --\n",__func__);
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
	printk("sprd: --  %s  --\n",__func__);
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
	printk("sprd: --  %s  --success!\n",__func__);
       return 0;
}

#ifdef CONFIG_PM
static int sc8810_backlight_suspend(struct platform_device *pdev,
               pm_message_t state)
{
	printk("sprd: --  %s  --\n",__func__);
	clk_disable(sc8810bl.clk);
       return 0;
}

static int sc8810_backlight_resume(struct platform_device *pdev)
{
	printk("sprd: --  %s  --\n",__func__);
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
	/*.suspend = sc8810_backlight_suspend,*/
	/*.resume = sc8810_backlight_resume,  */
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

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
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <mach/globalregs.h>
#include "sc8810_keypad.h"

#define keypad_readl(off)           __raw_readl(off)
#define keypad_writel(off, val)     __raw_writel((val), (off))

struct sprd_keypad_t *sprd_keypad;

#define INT_MASK_STS                (SPRD_INTCV_BASE + 0x0000)
#define INT_RAW_STS                 (SPRD_INTCV_BASE + 0x0004)
#define INT_EN                      (SPRD_INTCV_BASE + 0x0008)
#define INT_DIS                     (SPRD_INTCV_BASE + 0x000C)

/* sys fs  */
struct class *key_class;
EXPORT_SYMBOL(key_class);
struct device *key_dev;
EXPORT_SYMBOL(key_dev);
struct class *powerkey_class;
EXPORT_SYMBOL(powerkey_class);
struct device *powerkey_dev;
EXPORT_SYMBOL(powerkey_dev);

static ssize_t key_show(struct device *dev, struct device_attribute *attr, char *buf);
static DEVICE_ATTR(key_short, S_IRUGO, key_show, NULL);

static ssize_t powerkey_show(struct device *dev, struct device_attribute *attr, char *buf);
static DEVICE_ATTR(powerkey_short, S_IRUGO, powerkey_show, NULL);

static ssize_t powerkey_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t powerkey_pressed = 0;
	unsigned long powerkey_value = gpio_get_value(ANA_GPI_PB);

	printk("[KEYPAD] %s, powerkey_value=0x%08x\n", __func__, powerkey_value);
	if(powerkey_value ? 0 : 1)
	    {
	        /* key press */
	        powerkey_pressed = 1;
	        
	    } 
	    else 
	    {
	        /* key release */
	        powerkey_pressed = 0;                        
	    }
	
     return sprintf(buf, "%d\n", powerkey_pressed );
}

/* sys fs */
static ssize_t key_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t keys_pressed = 0;
	unsigned long int_status = keypad_readl(KPD_INT_RAW_STATUS);

	printk("[KEYPAD] %s, int_status=0x%08x\n", __func__, int_status);
	if(int_status&0xFFFF)
	    {
	        /* key press */
	        keys_pressed = 1;
	        
	    } 
	    else 
	    {
	        /* key release */
	        keys_pressed = 0;                        
	    }
	
     return sprintf(buf, "%d\n", keys_pressed );
}
/* sys fs */

static void dump_keypad_register(void)
{
	printk("\nREG_INT_MASK_STS = 0x%08x\n", keypad_readl(INT_MASK_STS));
	printk("REG_INT_RAW_STS = 0x%08x\n", keypad_readl(INT_RAW_STS));
	printk("REG_INT_EN = 0x%08x\n", keypad_readl(INT_EN));
	printk("REG_INT_DIS = 0x%08x\n", keypad_readl(INT_DIS));
	printk("REG_KPD_CTRL = 0x%08x\n", keypad_readl(KPD_CTRL));
	printk("REG_KPD_INT_EN = 0x%08x\n", keypad_readl(KPD_INT_EN));
	printk("REG_KPD_INT_RAW_STATUS = 0x%08x\n",
	       keypad_readl(KPD_INT_RAW_STATUS));
	printk("REG_KPD_INT_MASK_STATUS = 0x%08x\n",
	       keypad_readl(KPD_INT_MASK_STATUS));
	printk("REG_KPD_INT_CLR = 0x%08x\n", keypad_readl(KPD_INT_CLR));
	printk("REG_KPD_POLARITY = 0x%08x\n", keypad_readl(KPD_POLARITY));
	printk("REG_KPD_DEBOUNCE_CNT = 0x%08x\n",
	       keypad_readl(KPD_DEBOUNCE_CNT));
	printk("REG_KPD_LONG_KEY_CNT = 0x%08x\n",
	       keypad_readl(KPD_LONG_KEY_CNT));
	printk("REG_KPD_SLEEP_CNT = 0x%08x\n", keypad_readl(KPD_SLEEP_CNT));
	printk("REG_KPD_CLK_DIV_CNT = 0x%08x\n", keypad_readl(KPD_CLK_DIV_CNT));
	printk("REG_KPD_KEY_STATUS = 0x%08x\n", keypad_readl(KPD_KEY_STATUS));
	printk("REG_KPD_SLEEP_STATUS = 0x%08x\n",
	       keypad_readl(KPD_SLEEP_STATUS));
}

static irqreturn_t sprd_keypad_isr(int irq, void *dev_id)
{
	unsigned short key = 0;
	unsigned long value;
	unsigned long int_status = keypad_readl(KPD_INT_RAW_STATUS);
	unsigned long key_status = keypad_readl(KPD_KEY_STATUS);

	value = keypad_readl(KPD_INT_CLR);
	value |= KPD_INT_ALL;
	keypad_writel(KPD_INT_CLR, value);

	if ((int_status & KPD_PRESS_INT0)) {
		key = KEYCODE(key_status & (KPD0_ROW_CNT | KPD0_COL_CNT));
		if(key==0x9)
			key = KEY_VOLUMEDOWN;
		if((key==0x8)||(key==0xA))
			key = KEY_VOLUMEUP;	
		if(key==0x1A)
			key = KEY_HOME;			
		input_report_key(sprd_keypad->input, key, 1);
		input_sync(sprd_keypad->input);
		printk("%02xD\n", key);
	}

	if ((int_status & KPD_PRESS_INT1)) {
		key =
		    KEYCODE((key_status & (KPD1_ROW_CNT | KPD1_COL_CNT)) >> 8);
		input_report_key(sprd_keypad->input, key, 1);
		input_sync(sprd_keypad->input);
		printk("%02xD\n", key);
	}

	if ((int_status & KPD_PRESS_INT2)) {
		key =
		    KEYCODE((key_status & (KPD2_ROW_CNT | KPD2_COL_CNT)) >> 16);
		input_report_key(sprd_keypad->input, key, 1);
		input_sync(sprd_keypad->input);
		printk("%02xD\n", key);
	}

	if ((int_status & KPD_PRESS_INT3)) {
		key =
		    KEYCODE((key_status & (KPD3_ROW_CNT | KPD3_COL_CNT)) >> 24);
		input_report_key(sprd_keypad->input, key, 1);
		input_sync(sprd_keypad->input);
		printk("%02xD\n", key);
	}

	if (int_status & KPD_RELEASE_INT0) {
		key = KEYCODE(key_status & (KPD0_ROW_CNT | KPD0_COL_CNT));
		if(key==9)
			key = KEY_VOLUMEDOWN;	
		if((key==0x8)||(key==0xA))
			key = KEY_VOLUMEUP;	
		if(key==0x1A)
			key = KEY_HOME;			
		input_report_key(sprd_keypad->input, key, 0);
		input_sync(sprd_keypad->input);
		printk("%02xU\n", key);
	}

	if (int_status & KPD_RELEASE_INT1) {
		key =
		    KEYCODE((key_status & (KPD1_ROW_CNT | KPD1_COL_CNT)) >> 8);
		input_report_key(sprd_keypad->input, key, 0);
		input_sync(sprd_keypad->input);
		printk("%02xU\n", key);
	}

	if (int_status & KPD_RELEASE_INT2) {
		key =
		    KEYCODE((key_status & (KPD2_ROW_CNT | KPD2_COL_CNT)) >> 16);
		input_report_key(sprd_keypad->input, key, 0);
		input_sync(sprd_keypad->input);
		printk("%02xU\n", key);
	}

	if (int_status & KPD_RELEASE_INT3) {
		key =
		    KEYCODE((key_status & (KPD3_ROW_CNT | KPD3_COL_CNT)) >> 24);
		input_report_key(sprd_keypad->input, key, 0);
		input_sync(sprd_keypad->input);
		printk("%02xU\n", key);
	}

	return IRQ_HANDLED;
}

static irqreturn_t sprd_powerkey_isr(int irq, void *dev_id)
{
	static unsigned long last_value = 1;
	unsigned short key = KEY_POWER;
	unsigned long value = gpio_get_value(ANA_GPI_PB);

	if (last_value == value) {
		/* seems an event is missing, just report it */
		input_report_key(sprd_keypad->input, key, last_value);
		input_sync(sprd_keypad->input);

		printk("%dX\n", key);
	}

	if (value) {
		/* Release : HIGHT level */
		input_report_key(sprd_keypad->input, key, 0);
		input_sync(sprd_keypad->input);
		printk("%dU\n", key);
		irq_set_irq_type(irq, IRQF_TRIGGER_LOW);
	} else {
		/* Press : LOW level */
		input_report_key(sprd_keypad->input, key, 1);
		input_sync(sprd_keypad->input);
		printk("%dD\n", key);
		irq_set_irq_type(irq, IRQF_TRIGGER_HIGH);
	}

	last_value = value;

	return IRQ_HANDLED;
}

static int __devinit sprd_keypad_probe(struct platform_device *pdev)
{
	struct sprd_keypad_platform_data *pdata;
	struct input_dev *input;
	int i, error, key_type;
	unsigned long value;

	pdev->dev.platform_data = &sprd_keypad_data;
	pdata = pdev->dev.platform_data;
	if (!pdata->rows || !pdata->cols) {
		dev_err(&pdev->dev, "no rows, cols or keymap from pdata\n");
		return -EINVAL;
	}

	sprd_keypad = kzalloc(sizeof(struct sprd_keypad_t), GFP_KERNEL);
	if (!sprd_keypad)
		return -ENOMEM;
	platform_set_drvdata(pdev, sprd_keypad);
	sprd_keypad->keycode = sprd_keymap;
	sprd_keypad->irq = platform_get_irq(pdev, 0);
	if (sprd_keypad->irq < 0) {
		error = -ENODEV;
		goto out2;
	}

	sprd_greg_set_bits(REG_TYPE_GLOBAL, SWRST_KPD_RST, GR_SOFT_RST);
	mdelay(2);		/* NOTE : When reset keypad controller, must wait 2ms */
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, SWRST_KPD_RST, GR_SOFT_RST);

	if (unlikely(pdata->cols < KPD_COL_MIN_NUM))
		pdata->cols = KPD_COL_MIN_NUM;
	else if (unlikely(pdata->cols > KPD_COL_MAX_NUM))
		pdata->cols = KPD_COL_MAX_NUM;

	if (unlikely(pdata->rows < KPD_ROW_MIN_NUM))
		pdata->rows = KPD_ROW_MIN_NUM;
	else if (unlikely(pdata->rows > KPD_ROW_MAX_NUM))
		pdata->rows = KPD_ROW_MAX_NUM;

	key_type = ((((~(0xffffffff << (pdata->cols - KPD_COL_MIN_NUM))) << 20)
		     | ((~(0xffffffff << (pdata->rows - KPD_ROW_MIN_NUM))) <<
			16))
		    & (KPDCTL_ROW | KPDCTL_COL));

	value = 0x6 | key_type;
	keypad_writel(KPD_CTRL, value);

	sprd_greg_set_bits(REG_TYPE_GLOBAL, GEN0_KPD_EN | GEN0_KPD_RTC_EN,
			   GR_GEN0);

	keypad_writel(KPD_INT_CLR, KPD_INT_ALL);
	value = CFG_ROW_POLARITY | CFG_COL_POLARITY;
	keypad_writel(KPD_POLARITY, value);
	keypad_writel(KPD_CLK_DIV_CNT, 1);
	keypad_writel(KPD_LONG_KEY_CNT, 0xc);
	keypad_writel(KPD_DEBOUNCE_CNT, 0x5);

	error =
	    request_irq(sprd_keypad->irq, sprd_keypad_isr, 0, "sprd-keypad",
			pdev);
	if (error) {
		dev_err(&pdev->dev, "unable to claim irq %d\n",
			sprd_keypad->irq);
		goto out2;
	}

	input = input_allocate_device();
	if (!input) {
		error = -ENOMEM;
		goto out3;
	}

	sprd_keypad->input = input;
	input->name = pdev->name;
	input->phys = "sprd-key/input0";
	input->dev.parent = &pdev->dev;
	input_set_drvdata(input, sprd_keypad);

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	input->keycodesize = sizeof(unsigned short);
	input->keycodemax = ARRAY_SIZE(sprd_keymap);
	input->keycode = sprd_keypad->keycode;

	__set_bit(EV_KEY, input->evbit);
	if (pdata->repeat)
		__set_bit(EV_REP, input->evbit);

	for (i = 0; i < input->keycodemax; i++)
		__set_bit(sprd_keypad->keycode[i], input->keybit);

	__clear_bit(KEY_RESERVED, input->keybit);

	error = input_register_device(input);
	if (error) {
		dev_err(&pdev->dev, "unable to register input device\n");
		goto out4;
	}

	device_init_wakeup(&pdev->dev, 1);
	keypad_writel(KPD_INT_EN, KPD_INT_DOWNUP);
	value = keypad_readl(KPD_CTRL);
	value |= 0x1;
	keypad_writel(KPD_CTRL, value);

	gpio_request(ANA_GPI_PB, "powerkey");
	gpio_direction_input(ANA_GPI_PB);

	error =
	    request_irq(gpio_to_irq(ANA_GPI_PB), sprd_powerkey_isr,
			IRQF_TRIGGER_LOW, "powerkey", (void *)0);
	if (error) {
		dev_err(&pdev->dev, "unable to claim irq %d\n",
			gpio_to_irq(ANA_GPI_PB));
		goto out2;
	}

        /* sys fs */
	key_class = class_create(THIS_MODULE, "sec_key");
	if (IS_ERR(key_class))
		pr_err("Failed to create class(key)!\n");

	key_dev = device_create(key_class, NULL, 0, NULL, "sec_key_pressed");
	if (IS_ERR(key_dev))
		pr_err("Failed to create device(key)!\n");

	if (device_create_file(key_dev, &dev_attr_key_short) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_key_short.attr.name); 

	powerkey_class = class_create(THIS_MODULE, "sec_powerkey");
	if (IS_ERR(powerkey_class))
		pr_err("Failed to create class(powerkey)!\n");

	powerkey_dev = device_create(powerkey_class, NULL, 0, NULL, "sec_powerkey_pressed");
	if (IS_ERR(powerkey_dev))
		pr_err("Failed to create device(powerkey)!\n");

	if (device_create_file(powerkey_dev, &dev_attr_powerkey_short) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_powerkey_short.attr.name); 
	
	/* sys fs */
	
	/* dump_keypad_register(); */

	return 0;

out4:
	input_free_device(input);
out3:
	free_irq(sprd_keypad->irq, pdev);
out2:
	kfree(sprd_keypad);
	platform_set_drvdata(pdev, NULL);
	return error;
}

static int __devexit sprd_keypad_remove(struct platform_device *pdev)
{
	unsigned long value;
	struct sprd_keypad_t *sprd_keypad = platform_get_drvdata(pdev);

	free_irq(sprd_keypad->irq, pdev);
	input_unregister_device(sprd_keypad->input);
	kfree(sprd_keypad);
	platform_set_drvdata(pdev, NULL);

	/* disable sprd keypad controller */
	keypad_writel(KPD_INT_CLR, KPD_INT_ALL);
	value = keypad_readl(KPD_CTRL);
	value &= ~(1 << 0);
	keypad_writel(KPD_CTRL, value);
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, GEN0_KPD_EN | GEN0_KPD_RTC_EN,
			     GR_GEN0);

	return 0;
}

#define sprd_keypad_suspend	NULL
#define sprd_keypad_resume	NULL

struct platform_driver sprd_keypad_driver = {
	.probe = sprd_keypad_probe,
	.remove = __devexit_p(sprd_keypad_remove),
	.suspend = sprd_keypad_suspend,
	.resume = sprd_keypad_resume,
	.driver = {
		   .name = "sprd-keypad",
		   .owner = THIS_MODULE,
		   },
};

static int __init sprd_keypad_init(void)
{
	return platform_driver_register(&sprd_keypad_driver);
}

static void __exit sprd_keypad_exit(void)
{
	platform_driver_unregister(&sprd_keypad_driver);
}

module_init(sprd_keypad_init);
module_exit(sprd_keypad_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("spreadtrum.com");
MODULE_DESCRIPTION("Keypad driver for spreadtrum Processors");
MODULE_ALIAS("platform:sprd-keypad");

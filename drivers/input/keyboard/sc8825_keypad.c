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
#include <linux/input/matrix_keypad.h>
#include <linux/sysrq.h>

#include <mach/globalregs.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/adi.h>
#include <mach/kpd.h>
#include <mach/regs_glb.h>
#include <mach/sci.h>

#define DEBUG_KEYPAD	0

#define KPD_REG_BASE                (SPRD_KPD_BASE)

#define KPD_CTRL                	(KPD_REG_BASE + 0x00)
#define KPD_EN						(0x01 << 0)
#define KPD_SLEEP_EN				(0x01 << 1)
#define KPD_LONG_KEY_EN				(0x01 << 2)
#define KPDCTL_ROW_MSK                  (0x3f << 18)	/* enable rows 2 - 7 */
#define KPDCTL_COL_MSK                  (0x3f << 10)	/* enable cols 2 - 7 */

#define KPD_INT_EN              	(KPD_REG_BASE + 0x04)
#define KPD_INT_RAW_STATUS          (KPD_REG_BASE + 0x08)
#define KPD_INT_MASK_STATUS     	(KPD_REG_BASE + 0x0C)
#define KPD_INT_ALL                 (0xfff)
#define KPD_INT_DOWNUP              (0x0ff)
#define KPD_INT_LONG				(0xf00)
#define KPD_PRESS_INT0              (1 << 0)
#define KPD_PRESS_INT1              (1 << 1)
#define KPD_PRESS_INT2              (1 << 2)
#define KPD_PRESS_INT3              (1 << 3)
#define KPD_RELEASE_INT0            (1 << 4)
#define KPD_RELEASE_INT1            (1 << 5)
#define KPD_RELEASE_INT2            (1 << 6)
#define KPD_RELEASE_INT3            (1 << 7)
#define KPD_LONG_KEY_INT0           (1 << 8)
#define KPD_LONG_KEY_INT1           (1 << 9)
#define KPD_LONG_KEY_INT2           (1 << 10)
#define KPD_LONG_KEY_INT3           (1 << 11)

#define KPD_INT_CLR             	(KPD_REG_BASE + 0x10)
#define KPD_POLARITY            	(KPD_REG_BASE + 0x18)
#define KPD_CFG_ROW_POLARITY		(0xff)
#define KPD_CFG_COL_POLARITY		(0xFF00)
#define CFG_ROW_POLARITY            (KPD_CFG_ROW_POLARITY & 0x00FF)
#define CFG_COL_POLARITY            (KPD_CFG_COL_POLARITY & 0xFF00)

#define KPD_DEBOUNCE_CNT        	(KPD_REG_BASE + 0x1C)
#define KPD_LONG_KEY_CNT        	(KPD_REG_BASE + 0x20)

#define KPD_SLEEP_CNT           	(KPD_REG_BASE + 0x24)
#define KPD_SLEEP_CNT_VALUE(_X_MS_)	(_X_MS_ * 32.768 -1)
#define KPD_CLK_DIV_CNT         	(KPD_REG_BASE + 0x28)

#define KPD_KEY_STATUS          	(KPD_REG_BASE + 0x2C)
#define KPD_INT0_COL(_X_)	(((_X_)>> 0) & 0x7)
#define KPD_INT0_ROW(_X_)	(((_X_)>> 4) & 0x7)
#define KPD_INT1_COL(_X_)	(((_X_)>> 8) & 0x7)
#define KPD_INT1_ROW(_X_)	(((_X_)>> 12) & 0x7)
#define KPD_INT2_COL(_X_)	(((_X_)>> 16) & 0x7)
#define KPD_INT2_ROW(_X_)	(((_X_)>> 20) & 0x7)
#define KPD_INT3_COL(_X_)	(((_X_)>> 24) & 0x7)
#define KPD_INT3_ROW(_X_)	(((_X_)>> 28) & 0x7)

#define KPD_SLEEP_STATUS        	(KPD_REG_BASE + 0x0030)
#define KPD_DEBUG_STATUS1        	(KPD_REG_BASE + 0x0034)
#define KPD_DEBUG_STATUS2        	(KPD_REG_BASE + 0x0038)

#define ANA_GPI_PB                  EIC_KEY_POWER

struct sci_keypad_t {
	struct input_dev *input_dev;
	int irq;
	int rows;
	int cols;
	unsigned int keyup_test_jiffies;
};

//custom define end

#define keypad_readl(off)           __raw_readl(off)
#define keypad_writel(off, val)     __raw_writel((val), (off))

static void dump_keypad_register(void)
{
#if	DEBUG_KEYPAD

#define INT_MASK_STS                (SPRD_INTC0_BASE + 0x0000)
#define INT_RAW_STS                 (SPRD_INTC0_BASE + 0x0004)
#define INT_EN                      (SPRD_INTC0_BASE + 0x0008)
#define INT_DIS                     (SPRD_INTC0_BASE + 0x000C)

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
#endif
}

#ifdef CONFIG_MAGIC_SYSRQ
#define SPRD_VOL_UP_KEY		115
#define SPRD_VOL_DOWN_KEY	114
#define SPRD_CAMERA_KEY		212
static int check_key_down(struct sci_keypad_t *sci_kpd, int key_status, int key_value)
{
	int col, row, key;
	unsigned short *keycodes = sci_kpd->input_dev->keycode;
	unsigned int row_shift = get_count_order(sci_kpd->cols);

	if((key_status & 0xff) != 0) {
		col = KPD_INT0_COL(key_status);
		row = KPD_INT0_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
		if(key == key_value)
			return 1;
	}
	if((key_status & 0xff00) != 0) {
		col = KPD_INT1_COL(key_status);
		row = KPD_INT1_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
		if(key == key_value)
			return 1;
	}
	return 0;
}
#endif

static irqreturn_t sci_keypad_isr(int irq, void *dev_id)
{
	unsigned short key = 0;
	unsigned long value;
	struct sci_keypad_t *sci_kpd = dev_id;
	unsigned long int_status = keypad_readl(KPD_INT_MASK_STATUS);
	unsigned long key_status = keypad_readl(KPD_KEY_STATUS);
	unsigned short *keycodes = sci_kpd->input_dev->keycode;
	unsigned int row_shift = get_count_order(sci_kpd->cols);
	int col, row;

	value = keypad_readl(KPD_INT_CLR);
	value |= KPD_INT_ALL;
	keypad_writel(KPD_INT_CLR, value);

	if ((int_status & KPD_PRESS_INT0)) {
		col = KPD_INT0_COL(key_status);
		row = KPD_INT0_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
		input_report_key(sci_kpd->input_dev, key, 1);
		input_sync(sci_kpd->input_dev);
		printk("%03dD\n", key);
	}
	if (int_status & KPD_RELEASE_INT0) {
		col = KPD_INT0_COL(key_status);
		row = KPD_INT0_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];

		input_report_key(sci_kpd->input_dev, key, 0);
		input_sync(sci_kpd->input_dev);
		printk("%03dU\n", key);
	}

	if ((int_status & KPD_PRESS_INT1)) {
		col = KPD_INT1_COL(key_status);
		row = KPD_INT1_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];

		input_report_key(sci_kpd->input_dev, key, 1);
		input_sync(sci_kpd->input_dev);
		printk("%03dD\n", key);
	}
	if (int_status & KPD_RELEASE_INT1) {
		col = KPD_INT1_COL(key_status);
		row = KPD_INT1_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];

		input_report_key(sci_kpd->input_dev, key, 0);
		input_sync(sci_kpd->input_dev);
		printk("%03dU\n", key);
	}

	if ((int_status & KPD_PRESS_INT2)) {
		col = KPD_INT2_COL(key_status);
		row = KPD_INT2_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];

		input_report_key(sci_kpd->input_dev, key, 1);
		input_sync(sci_kpd->input_dev);
		printk("%03d\n", key);
	}
	if (int_status & KPD_RELEASE_INT2) {
		col = KPD_INT2_COL(key_status);
		row = KPD_INT2_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];

		input_report_key(sci_kpd->input_dev, key, 0);
		input_sync(sci_kpd->input_dev);
		printk("%03d\n", key);
	}

	if (int_status & KPD_PRESS_INT3) {
		col = KPD_INT3_COL(key_status);
		row = KPD_INT3_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];

		input_report_key(sci_kpd->input_dev, key, 1);
		input_sync(sci_kpd->input_dev);
		printk("%03d\n", key);
	}
	if (int_status & KPD_RELEASE_INT3) {
		col = KPD_INT3_COL(key_status);
		row = KPD_INT3_ROW(key_status);
		key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];

		input_report_key(sci_kpd->input_dev, key, 0);
		input_sync(sci_kpd->input_dev);
		printk("%03d\n", key);
	}

#ifdef CONFIG_MAGIC_SYSRQ
	{
		static unsigned long key_status_prev = 0;

		if (check_key_down(sci_kpd, key_status, SPRD_CAMERA_KEY) &&
			check_key_down(sci_kpd, key_status, SPRD_VOL_DOWN_KEY) && key_status != key_status_prev) {
			key_status_prev = key_status;
			panic("!!!! Combine key: vol_down + camera !!!!\n");
		}

		if (check_key_down(sci_kpd, key_status, SPRD_CAMERA_KEY) &&
			check_key_down(sci_kpd, key_status, SPRD_VOL_UP_KEY) && key_status != key_status_prev) {
			unsigned long flags;
			static int rebooted = 0;
			key_status_prev = key_status;
			local_irq_save(flags);
			if (rebooted == 0) {
				rebooted = 1;
				pr_warn("!!!!!! Combine Key : vol_up + camera is Down !!!!!!\n");
				/* handle_sysrq('t'); */
				handle_sysrq('m');
				handle_sysrq('w');
				handle_sysrq('b');
				pr_warn("!!!!!! /proc/sys/kernel/sysrq is disabled !!!!!!\n");
				rebooted = 0;
			}
			local_irq_restore(flags);
		}
	}
#endif
	return IRQ_HANDLED;
}

static irqreturn_t sci_powerkey_isr(int irq, void *dev_id)
{				//TODO: if usign gpio(eic), need add row , cols to platform data.
	static unsigned long last_value = 1;
	unsigned short key = KEY_POWER;
	unsigned long value = !(gpio_get_value(ANA_GPI_PB));
	struct sci_keypad_t *sci_kpd = dev_id;

	if (last_value == value) {
		/* seems an event is missing, just report it */
		input_report_key(sci_kpd->input_dev, key, last_value);
		input_sync(sci_kpd->input_dev);

		printk("%dX\n", key);
	}

	if (value) {
		/* Release : low level */
		input_report_key(sci_kpd->input_dev, key, 0);
		input_sync(sci_kpd->input_dev);
		printk("Powerkey:%dU\n", key);
		irq_set_irq_type(irq, IRQF_TRIGGER_HIGH);
	} else {
		/* Press : high level */
		input_report_key(sci_kpd->input_dev, key, 1);
		input_sync(sci_kpd->input_dev);
		printk("Powerkey:%dD\n", key);
		irq_set_irq_type(irq, IRQF_TRIGGER_LOW);
	}

	last_value = value;

	return IRQ_HANDLED;
}

static int __devinit sci_keypad_probe(struct platform_device *pdev)
{

	struct sci_keypad_t *sci_kpd;
	struct input_dev *input_dev;
	struct sci_keypad_platform_data *pdata = pdev->dev.platform_data;
	int error;
	unsigned long value;
	unsigned int row_shift, keycodemax;

	row_shift = get_count_order(pdata->cols);
	keycodemax = pdata->rows << row_shift;

	sci_kpd = kzalloc(sizeof(struct sci_keypad_t) +
			  keycodemax * sizeof(unsigned short), GFP_KERNEL);
	input_dev = input_allocate_device();

	if (!sci_kpd || !input_dev) {
		kfree(sci_kpd);
		input_free_device(input_dev);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, sci_kpd);

	sci_kpd->input_dev = input_dev;
	sci_kpd->rows = pdata->rows;
	sci_kpd->cols = pdata->cols;

	sci_glb_set(REG_GLB_SOFT_RST,BIT_KPD_RST);
	mdelay(2);
	sci_glb_clr(REG_GLB_SOFT_RST,BIT_KPD_RST);
	sci_glb_set(REG_GLB_GEN0,BIT_KPD_EB | BIT_RTC_KPD_EB);

	keypad_writel(KPD_INT_CLR, KPD_INT_ALL);
	value = CFG_ROW_POLARITY | CFG_COL_POLARITY;
	keypad_writel(KPD_POLARITY, value);
	keypad_writel(KPD_CLK_DIV_CNT, 1);
	keypad_writel(KPD_LONG_KEY_CNT, 0xc);
	keypad_writel(KPD_DEBOUNCE_CNT, 0x5);

	sci_kpd->irq = platform_get_irq(pdev, 0);
	if (sci_kpd->irq < 0) {
		error = -ENODEV;
		dev_err(&pdev->dev, "Get irq number error,Keypad Module\n");
		goto out2;
	}

	error =
	    request_irq(sci_kpd->irq, sci_keypad_isr, 0, "sci-keypad", sci_kpd);
	if (error) {
		dev_err(&pdev->dev, "unable to claim irq %d\n", sci_kpd->irq);
		goto out2;
	}

	input_dev->name = pdev->name;
	input_dev->phys = "sci-key/input0";
	input_dev->dev.parent = &pdev->dev;
	input_set_drvdata(input_dev, sci_kpd);

	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

	input_dev->keycode = &sci_kpd[1];
	input_dev->keycodesize = sizeof(unsigned short);
	input_dev->keycodemax = keycodemax;

	matrix_keypad_build_keymap(pdata->keymap_data, row_shift,
				   input_dev->keycode, input_dev->keybit);

	/* there are keys from hw other than keypad controller */
	__set_bit(KEY_POWER, input_dev->keybit);

	__set_bit(EV_KEY, input_dev->evbit);
	if (pdata->repeat)
		__set_bit(EV_REP, input_dev->evbit);

	error = input_register_device(input_dev);
	if (error) {
		dev_err(&pdev->dev, "unable to register input device\n");
		goto out4;
	}

	device_init_wakeup(&pdev->dev, 1);

	value = KPD_INT_DOWNUP;
	if (pdata->support_long_key)
		value |= KPD_INT_LONG;
	keypad_writel(KPD_INT_EN, value);

	value = KPD_SLEEP_CNT_VALUE(1000);
	keypad_writel(KPD_SLEEP_CNT, value);

	value = KPD_SLEEP_EN | (pdata->rows_choose_hw & KPDCTL_ROW_MSK) |
	    (pdata->cols_choose_hw & KPDCTL_COL_MSK);
	if (pdata->support_long_key)
		value |= KPD_LONG_KEY_EN;
	value |= KPD_EN;
	keypad_writel(KPD_CTRL, value);

	gpio_request(ANA_GPI_PB, "powerkey");
	gpio_direction_input(ANA_GPI_PB);

	error = request_irq(gpio_to_irq(ANA_GPI_PB), sci_powerkey_isr,
			IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND, "powerkey", sci_kpd);
	if (error) {
		dev_err(&pdev->dev, "unable to claim irq %d\n",
			gpio_to_irq(ANA_GPI_PB));
		goto out2;
	}


	dump_keypad_register();

	return 0;

out4:
	input_free_device(input_dev);
	free_irq(sci_kpd->irq, pdev);
out2:
	kfree(sci_kpd);
	platform_set_drvdata(pdev, NULL);
	return error;
}

static int __devexit sci_keypad_remove(struct platform_device *pdev)
{
	unsigned long value;
	struct sci_keypad_t *sci_kpd = platform_get_drvdata(pdev);

	free_irq(sci_kpd->irq, pdev);
	input_unregister_device(sci_kpd->input_dev);
	kfree(sci_kpd);
	platform_set_drvdata(pdev, NULL);

	/* disable sci keypad controller */
	keypad_writel(KPD_INT_CLR, KPD_INT_ALL);
	value = keypad_readl(KPD_CTRL);
	value &= ~(1 << 0);
	keypad_writel(KPD_CTRL, value);
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, GEN0_KPD_EN | GEN0_KPD_RTC_EN,
			     GR_GEN0);

	return 0;
}

#ifdef CONFIG_PM
static int sci_keypad_suspend(struct platform_device *dev, pm_message_t state)
{
	/* Nothing yet */

	return 0;
}

static int sci_keypad_resume(struct platform_device *dev)
{
	/* Nothing yet */

	return 0;
}
#else
#define sci_keypad_suspend	NULL
#define sci_keypad_resume	NULL
#endif

struct platform_driver sci_keypad_driver = {
	.probe = sci_keypad_probe,
	.remove = __devexit_p(sci_keypad_remove),
	.suspend = sci_keypad_suspend,
	.resume = sci_keypad_resume,
	.driver = {
		   .name = "sci-keypad",
		   .owner = THIS_MODULE,
		   },
};

static int __init sci_keypad_init(void)
{
	return platform_driver_register(&sci_keypad_driver);
}

static void __exit sci_keypad_exit(void)
{
	platform_driver_unregister(&sci_keypad_driver);
}

module_init(sci_keypad_init);
module_exit(sci_keypad_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("spreadtrum.com");
MODULE_DESCRIPTION("Keypad driver for spreadtrum Processors");
MODULE_ALIAS("platform:sci-keypad");

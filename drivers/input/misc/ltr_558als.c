/*
 * File:         ltr_558als.c
 * Based on:
 * Author:       Liuxd <liuxiaodong@cellroam.com>
 *
 * Created:      2011-11-08
 * Description:  LTR-558ALS Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/wakelock.h>
#include <linux/i2c/ltr_558als.h>
#include <linux/slab.h>

typedef struct tag_ltr558 {
	struct input_dev *input;
	struct i2c_client *client;
	struct work_struct work;
	struct workqueue_struct *ltr_work_queue;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend ltr_early_suspend;
#endif
} ltr558_t, *ltr558_p;

static int p_flag;
static int l_flag;
static int p_gainrange;
static int l_gainrange;

static struct i2c_client *this_client = NULL;

// I2C Read
static int ltr558_i2c_read_reg(u8 regnum)
{
	int readdata;

	/*
	 * i2c_smbus_read_byte_data - SMBus "read byte" protocol
	 * @client: Handle to slave device
	 * @command: Byte interpreted by slave
	 *
	 * This executes the SMBus "read byte" protocol, returning negative errno
	 * else a data byte received from the device.
	 */
	if (!this_client)
		return -1;

	readdata = i2c_smbus_read_byte_data(this_client, regnum);

	return readdata;
}

// I2C Write
static int ltr558_i2c_write_reg(u8 regnum, u8 value)
{
	int writeerror;

	/*
	 * i2c_smbus_write_byte_data - SMBus "write byte" protocol
	 * @client: Handle to slave device
	 * @command: Byte interpreted by slave
	 * @value: Byte being written
	 *
	 * This executes the SMBus "write byte" protocol, returning negative errno
	 * else zero on success.
	 */

	if (!this_client)
		return -1;

	writeerror = i2c_smbus_write_byte_data(this_client, regnum, value);

	if (writeerror < 0)
		return writeerror;
	else
		return 0;
}

/*
 * ###############
 * ## PS CONFIG ##
 * ###############
 */
static int ltr558_ps_enable(int gainrange)
{
	int error;
	int setgain;

	alert_mesg("ltr558_ps_enable %d\n", gainrange);
	switch (gainrange) {
	case PS_RANGE1:
		setgain = MODE_PS_ON_Gain1;
		break;

	case PS_RANGE2:
		setgain = MODE_PS_ON_Gain4;
		break;

	case PS_RANGE4:
		setgain = MODE_PS_ON_Gain8;
		break;

	case PS_RANGE8:
		setgain = MODE_PS_ON_Gain16;
		break;

	default:
		setgain = MODE_PS_ON_Gain8;
		break;
	}

	error = ltr558_i2c_write_reg(LTR558_PS_CONTR, setgain);
	mdelay(WAKEUP_DELAY);

	/* ===============
	 * ** IMPORTANT **
	 * ===============
	 * Other settings like timing and threshold to be set here, if required.
	 * Not set and kept as device default for now.
	 */
	ltr558_i2c_read_reg(LTR558_ALS_PS_STATUS);
	ltr558_i2c_read_reg(LTR558_PS_DATA_0);
	ltr558_i2c_read_reg(LTR558_PS_DATA_1);
	ltr558_i2c_read_reg(LTR558_ALS_DATA_CH1_0);
	ltr558_i2c_read_reg(LTR558_ALS_DATA_CH1_1);
	ltr558_i2c_read_reg(LTR558_ALS_DATA_CH0_0);
	ltr558_i2c_read_reg(LTR558_ALS_DATA_CH0_1);

	return error;
}

// Put PS into Standby mode
static int ltr558_ps_disable(void)
{
	int error;

	alert_mesg("ltr558_ps_disable \n");

	error = ltr558_i2c_write_reg(LTR558_PS_CONTR, MODE_PS_StdBy);

	return error;
}

static int ltr558_ps_read(void)
{
	int psval_lo, psval_hi, psdata;

	psval_lo = ltr558_i2c_read_reg(LTR558_PS_DATA_0);
	if (psval_lo < 0) {
		psdata = psval_lo;
		goto out;
	}

	psval_hi = ltr558_i2c_read_reg(LTR558_PS_DATA_1);
	if (psval_hi < 0) {
		psdata = psval_hi;
		goto out;
	}

	psdata = ((psval_hi & 0x07) << 8) | psval_lo;

out:
	return psdata;
}

/*
 * ################
 * ## ALS CONFIG ##
 * ################
 */
static int ltr558_als_enable(int gainrange)
{
	int error;
	int setgain;

	alert_mesg("ltr558_als_enable %d\n", gainrange);

	switch (gainrange) {
	case ALS_RANGE1_320:
		setgain = MODE_ALS_ON_Range1;
		break;

	case ALS_RANGE2_64K:
		setgain = MODE_ALS_ON_Range2;
		break;

	default:
		setgain = MODE_ALS_ON_Range1;
		break;
	}

	error = ltr558_i2c_write_reg(LTR558_ALS_CONTR, setgain);
	mdelay(WAKEUP_DELAY);

	/* ===============
	 * ** IMPORTANT **
	 * ===============
	 * Other settings like timing and threshold to be set here, if required.
	 * Not set and kept as device default for now.
	 */
	ltr558_i2c_read_reg(LTR558_ALS_PS_STATUS);
	ltr558_i2c_read_reg(LTR558_PS_DATA_0);
	ltr558_i2c_read_reg(LTR558_PS_DATA_1);
	ltr558_i2c_read_reg(LTR558_ALS_DATA_CH1_0);
	ltr558_i2c_read_reg(LTR558_ALS_DATA_CH1_1);
	ltr558_i2c_read_reg(LTR558_ALS_DATA_CH0_0);
	ltr558_i2c_read_reg(LTR558_ALS_DATA_CH0_1);

	return error;
}

// Put ALS into Standby mode
static int ltr558_als_disable(void)
{
	int error;

	alert_mesg("ltr558_als_disable \n");
	error = ltr558_i2c_write_reg(LTR558_ALS_CONTR, MODE_ALS_StdBy);

	return error;
}

static int ltr558_als_read(int gainrange)
{
	int alsval_ch0_lo, alsval_ch0_hi;
	int alsval_ch1_lo, alsval_ch1_hi;
	int luxdata_int;
	int luxdata_flt, ratio;
	int alsval_ch0, alsval_ch1;

	alsval_ch1_lo = ltr558_i2c_read_reg(LTR558_ALS_DATA_CH1_0);
	alsval_ch1_hi = ltr558_i2c_read_reg(LTR558_ALS_DATA_CH1_1);
	alsval_ch1 = (alsval_ch1_hi * 256) + alsval_ch1_lo;

	alsval_ch0_lo = ltr558_i2c_read_reg(LTR558_ALS_DATA_CH0_0);
	alsval_ch0_hi = ltr558_i2c_read_reg(LTR558_ALS_DATA_CH0_1);
	alsval_ch0 = (alsval_ch0_hi * 256) + alsval_ch0_lo;

	dbg_mesg("alsval_ch0[%d],  alsval_ch1[%d]\n", alsval_ch0, alsval_ch1);

	if (0 == alsval_ch0)
		ratio = 100;
	else
		ratio = (alsval_ch1 * 100) / alsval_ch0;

	// Compute Lux data from ALS data (ch0 and ch1)
	// For Ratio < 0.69:
	// 1.3618*CH0 – 1.5*CH1
	// For 0.69 <= Ratio < 1:
	// 0.57*CH0 – 0.345*CH1
	// For high gain, divide the calculated lux by 150.
	if (ratio < 69) {
		luxdata_flt = (13618 * alsval_ch0) - (15000 * alsval_ch1);
		luxdata_flt = luxdata_flt / 10000;
	} else if ((ratio >= 69) && (ratio < 100)) {
		luxdata_flt = (5700 * alsval_ch0) - (3450 * alsval_ch1);
		luxdata_flt = luxdata_flt / 10000;
	} else {
		luxdata_flt = 0;
	}

	// For Range1
	if (gainrange == ALS_RANGE1_320)
		luxdata_flt = luxdata_flt / 150;

	luxdata_int = luxdata_flt * 50;

	return luxdata_int;
}

static int ltr558_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ltr558_release(struct inode *inode, struct file *file)
{
	dbg_mesg("+-\n");
	return 0;
}

static long ltr558_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int flag;

	dbg_mesg("++, cmd = %d,%d\n", _IOC_NR(cmd), cmd);

	switch (cmd) {
	case LTR_IOCTL_SET_PFLAG:
		{
			if (copy_from_user(&flag, argp, sizeof(flag)))
				return -EFAULT;
			dbg_mesg("++, flag = %d", flag);
			if (1 == flag) {
				if (ltr558_ps_enable(p_gainrange))
					return -EIO;
			} else if (0 == flag) {
				if (ltr558_ps_disable())
					return -EIO;
			} else {
				return -EINVAL;
			}

			p_flag = flag;
		}
		break;

	case LTR_IOCTL_SET_LFLAG:
		{
			if (copy_from_user(&flag, argp, sizeof(flag)))
				return -EFAULT;
			dbg_mesg("++, flag = %d", flag);
			if (1 == flag) {
				if (ltr558_als_enable(l_gainrange))
					return -EIO;
			} else if (0 == flag) {
				if (ltr558_als_disable())
					return -EIO;
			} else {
				return -EINVAL;
			}

			l_flag = flag;
		}
		break;

	case LTR_IOCTL_GET_PFLAG:
		{
			flag = p_flag;
			if (copy_to_user(argp, &flag, sizeof(flag)))
				return -EFAULT;
		}
		break;

	case LTR_IOCTL_GET_LFLAG:
		{
			printk("LTR_IOCTL_GET_LFLAG3\n");
			flag = l_flag;
			if (copy_to_user(argp, &flag, sizeof(flag)))

				return -EFAULT;
		}
		break;
	default:
		break;
	}

	dbg_mesg("--\n");

	return 0;
}

static struct file_operations ltr558_fops = {
	.owner = THIS_MODULE,
	.open = ltr558_open,
	.release = ltr558_release,
	.unlocked_ioctl = ltr558_ioctl,
};

static struct miscdevice ltr558_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = LTR558_I2C_NAME,
	.fops = &ltr558_fops,
};

static void ltr558_work(struct work_struct *work)
{
	int als_ps_status, val, logpval;
	ltr558_t *pls = container_of(work, ltr558_t, work);

	alert_mesg("ltr558_work \n");

	als_ps_status = ltr558_i2c_read_reg(LTR558_ALS_PS_STATUS);
	dbg_mesg("als_ps_status=0x%02x\n", als_ps_status);

	if (0x03 == (als_ps_status & 0x03)) {
		val = ltr558_ps_read();
		dbg_mesg("p -> val=0x%04x\n", val);

		if (val >= 0x49d) {	// 3cm
			ltr558_i2c_write_reg(0x90, 0xff);
			ltr558_i2c_write_reg(0x91, 0x07);
			ltr558_i2c_write_reg(0x92, 0x6d);
			ltr558_i2c_write_reg(0x93, 0x05);

			input_report_abs(pls->input, ABS_DISTANCE, 0);
			input_sync(pls->input);
		} else if (val <= 0x56d) {	// 5cm
			ltr558_i2c_write_reg(0x90, 0x9d);
			ltr558_i2c_write_reg(0x91, 0x04);
			ltr558_i2c_write_reg(0x92, 0x00);
			ltr558_i2c_write_reg(0x93, 0x00);

			input_report_abs(pls->input, ABS_DISTANCE, 1);
			input_sync(pls->input);
		}
	}

	if (0x0c == (als_ps_status & 0x0c)) {
		val = ltr558_als_read(l_gainrange);
		dbg_mesg("l -> val=0x%04x\n", val);

//		logpval = ltr558_ps_read();
//		dbg_mesg("p -> logpval=0x%04x\n", logpval);

		input_report_abs(pls->input, ABS_MISC, val);
		input_sync(pls->input);
	}

	enable_irq(pls->client->irq);
	//dbg_mesg("--\n");
}

static irqreturn_t ltr558_irq_handler(int irq, void *dev_id)
{
	ltr558_t *pls = (ltr558_t *) dev_id;

	disable_irq_nosync(pls->client->irq);
	queue_work(pls->ltr_work_queue, &pls->work);

	return IRQ_HANDLED;
}

static void ltr558_pls_pininit(int irq_pin)
{
	printk(KERN_INFO "%s [irq=%d]\n", __func__, irq_pin);
	gpio_request(irq_pin, LTR558_PLS_IRQ_PIN);
	gpio_direction_input(irq_pin);
}

static int ltr558_sw_reset(void)
{
	alert_mesg("ltr558_sw_reset \n");

	return ltr558_i2c_write_reg(LTR558_ALS_CONTR, 0x04);
}

static int ltr558_reg_init(void)
{
	int ret = 0;

	alert_mesg("ltr558_reg_init \n");

	//ltr558_sw_reset();
	//mdelay(PON_DELAY);

	ltr558_i2c_write_reg(0x82, 0x7b);
	ltr558_i2c_write_reg(0x83, 0x0f);
	ltr558_i2c_write_reg(0x84, 0x00);
	ltr558_i2c_write_reg(0x85, 0x03);
	ltr558_i2c_write_reg(0x8f, 0x0B);
	ltr558_i2c_write_reg(0x9e, 0x02);

	// ps
	ltr558_i2c_write_reg(0x90, 0x01);
	ltr558_i2c_write_reg(0x91, 0x00);
	ltr558_i2c_write_reg(0x92, 0x00);
	ltr558_i2c_write_reg(0x93, 0x00);

	// als
	ltr558_i2c_write_reg(0x97, 0x00);
	ltr558_i2c_write_reg(0x98, 0x00);
	ltr558_i2c_write_reg(0x99, 0x01);
	ltr558_i2c_write_reg(0x9a, 0x00);
	mdelay(WAKEUP_DELAY);

	// dump register
#if 0
	{
		uint8_t reg;
		uint8_t val;

		for (reg = 0x80; reg <= 0x93; reg++) {
			val = ltr558_i2c_read_reg(reg);
			dbg_mesg("reg[%02x] = %02x\n", reg, val);
		}

		for (reg = 0x97; reg <= 0x9a; reg++) {
			val = ltr558_i2c_read_reg(reg);
			dbg_mesg("reg[%02x] = %02x\n", reg, val);
		}

		for (reg = 0x9e; reg <= 0x9e; reg++) {
			val = ltr558_i2c_read_reg(reg);
			dbg_mesg("reg[%02x] = %02x\n", reg, val);
		}
	}

	printk("%s:  0x87 = [%x] \n", __func__, ltr558_i2c_read_reg(0x87));
	printk("%s: 0x8f  = [%x]\n", __func__, ltr558_i2c_read_reg(0x8f));
	printk("%s: 0x90 = [%x], 0x91 = [%x]\n", __func__,
	       ltr558_i2c_read_reg(0x90), ltr558_i2c_read_reg(0x91));
	printk("%s: 0x92 = [%x], 0x93 = [%x]\n", __func__,
	       ltr558_i2c_read_reg(0x92), ltr558_i2c_read_reg(0x93));
	printk("%s: 0x97 = [%x], 0x98 = [%x]\n", __func__,
	       ltr558_i2c_read_reg(0x97), ltr558_i2c_read_reg(0x98));
	printk("%s: 0x99 = [%x], 0x9a = [%x]\n", __func__,
	       ltr558_i2c_read_reg(0x99), ltr558_i2c_read_reg(0x9a));
#endif

	dbg_mesg("--, ret = %d\n", ret);
	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND

static void ltr558_early_suspend(struct early_suspend *handler)
{
	dbg_mesg("+-\n");
}

static void ltr558_early_resume(struct early_suspend *handler)
{
	dbg_mesg("+-\n");
}

#endif

static int ltr558_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	ltr558_t *ltr_558als;
	struct ltr558_pls_platform_data *pdata = client->dev.platform_data;
	struct input_dev *input_dev;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		alert_mesg("call i2c_check_functionality() error.\n");
		ret = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ltr_558als = kzalloc(sizeof(ltr558_t), GFP_KERNEL);
	if (!ltr_558als) {
		alert_mesg("call kzalloc() error.\n");
		ret = -ENOMEM;
		goto exit_request_memory_failed;
	}

	i2c_set_clientdata(client, ltr_558als);
	ltr_558als->client = client;
	this_client = client;

	// init LTR_558ALS
	p_gainrange = PS_RANGE4;
	l_gainrange = ALS_RANGE2_64K;
	if (ltr558_reg_init() < 0) {
		alert_mesg("call ltr558_reg_init() error.\n");
		ret = -1;
		goto exit_device_init_failed;
	}
	// register device
	ret = misc_register(&ltr558_device);
	if (ret) {
		alert_mesg("call misc_register() error, ret = %d\n", ret);
		goto exit_misc_register_failed;
	}
	// register input device
	input_dev = input_allocate_device();
	if (!input_dev) {
		alert_mesg("call input_allocate_device() error, ret = %d\n",
			   ret);
		ret = -ENOMEM;
		goto exit_input_allocate_failed;
	}

	ltr_558als->input = input_dev;
	input_dev->name = LTR558_INPUT_DEV;
	input_dev->phys = LTR558_INPUT_DEV;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0010;

	__set_bit(EV_ABS, input_dev->evbit);
	// for proximity
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);
	// for lightsensor
	input_set_abs_params(input_dev, ABS_MISC, 0, 100001, 0, 0);

	ret = input_register_device(input_dev);
	if (ret < 0) {
		alert_mesg("call input_register_device() error, ret = %d\n",
			   ret);
		goto exit_input_register_failed;
	}

	// create work queue
	INIT_WORK(&ltr_558als->work, ltr558_work);
	ltr_558als->ltr_work_queue =
	    create_singlethread_workqueue(LTR558_I2C_NAME);
	if (!ltr_558als->ltr_work_queue) {
		alert_mesg
		    ("call create_singlethread_workqueue() error, ret = %d\n",
		     ret);
		goto exit_create_workqueue_failed;
	}

	/*pin init */
	ltr558_pls_pininit(pdata->irq_gpio_number);

	// register irq
	client->irq = gpio_to_irq(pdata->irq_gpio_number);
	alert_mesg("client->irq = %d\n", client->irq);

	if (client->irq > 0) {
		ret =
		    request_irq(client->irq, ltr558_irq_handler,
				IRQ_TYPE_LEVEL_LOW, client->name, ltr_558als);
		if (ret < 0) {
			free_irq(client->irq, ltr_558als);
			client->irq = 0;

			alert_mesg("call request_irq() error, ret = %d\n", ret);
			goto exit_irq_request_err;
		}
	}
	// register early suspend
#ifdef CONFIG_HAS_EARLYSUSPEND
	ltr_558als->ltr_early_suspend.level =
	    EARLY_SUSPEND_LEVEL_DISABLE_FB + 25;
	ltr_558als->ltr_early_suspend.suspend = ltr558_early_suspend;
	ltr_558als->ltr_early_suspend.resume = ltr558_early_resume;
	register_early_suspend(&ltr_558als->ltr_early_suspend);
#endif

#if 0				//gionee licz add for test
	ltr558_ps_enable(MODE_PS_ON_Gain8);
	ltr558_als_enable(MODE_ALS_ON_Range2);
#endif
	//mdelay(200);
	//ltr558_ps_read();
	return 0;

exit_irq_request_err:
	destroy_workqueue(ltr_558als->ltr_work_queue);
	ltr_558als->ltr_work_queue = NULL;
exit_create_workqueue_failed:
	input_unregister_device(input_dev);
exit_input_register_failed:
	input_free_device(input_dev);
	input_dev = NULL;
exit_input_allocate_failed:
	misc_deregister(&ltr558_device);
exit_misc_register_failed:
exit_device_init_failed:
	kfree(ltr_558als);
	ltr_558als = NULL;
exit_request_memory_failed:
exit_check_functionality_failed:

	alert_mesg("--, ERR! ret = %d\n", ret);
	return ret;
}

static int ltr558_remove(struct i2c_client *client)
{
	ltr558_t *ltr_558als = i2c_get_clientdata(client);

	alert_mesg("++\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ltr_558als->ltr_early_suspend);
#endif

	flush_workqueue(ltr_558als->ltr_work_queue);
	destroy_workqueue(ltr_558als->ltr_work_queue);
	ltr_558als->ltr_work_queue = NULL;

	input_unregister_device(ltr_558als->input);
	input_free_device(ltr_558als->input);
	ltr_558als->input = NULL;

	misc_deregister(&ltr558_device);

	// free irq
	free_irq(ltr_558als->client->irq, ltr_558als);
	//sprd_free_gpio_irq(ltr_558als->client->irq);

	// free memory
	kfree(ltr_558als);
	ltr_558als = NULL;
	this_client = NULL;

	alert_mesg("--\n");

	return 0;
}

static const struct i2c_device_id ltr558_id[] = {
	{LTR558_I2C_NAME, 0},
	{}
};

static struct i2c_driver ltr558_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = LTR558_I2C_NAME,
		   },
	.probe = ltr558_probe,
	.remove = ltr558_remove,
	.id_table = ltr558_id,
};

static int __init ltr558_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&ltr558_driver);
	return ret;
}

static void __exit ltr558_exit(void)
{
	// reset
	ltr558_sw_reset();
	// delete driver
	i2c_del_driver(&ltr558_driver);
}

module_init(ltr558_init);
module_exit(ltr558_exit);

MODULE_AUTHOR("Liuxd <liuxiaodong@cellroam.com>");
MODULE_DESCRIPTION("Proximity&Light Sensor LTR558ALS DRIVER");
MODULE_LICENSE("GPL");

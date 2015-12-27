/* alps-input.c
 *
 * MagneticField device driver for I2C
 *
 * Copyright (C) 2011 ALPS ELECTRIC CO., LTD. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
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
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/io.h>
#include <asm/uaccess.h>
#include <mach/gpio.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>

#define I2C_RETRY_DELAY		5
#define HSCD_I2C_RETRIES		5

#define I2C_HSCD_ADDR (0x0c)	/* 000 1100	*/
#define I2C_BUS_NUMBER	4

#define HSCD_DRIVER_NAME "hscd_i2c"

#define HSCD_STBA		0x0B
#define HSCD_STBB		0x0C
#define HSCD_XOUT		0x10
#define HSCD_YOUT		0x12
#define HSCD_ZOUT		0x14
#define HSCD_XOUT_H		0x11
#define HSCD_XOUT_L		0x10
#define HSCD_YOUT_H		0x13
#define HSCD_YOUT_L		0x12
#define HSCD_ZOUT_H		0x15
#define HSCD_ZOUT_L		0x14

#define HSCD_STATUS		0x18
#define HSCD_CTRL1		0x1b
#define HSCD_CTRL2		0x1c
#define HSCD_CTRL3		0x1d
#ifdef HSCDTD002A
	#define HSCD_CTRL4	0x28
#endif

#define HSCD_DEBUG  0

#if HSCD_DEBUG
#define MAGDBG(fmt, args...) printk(KERN_INFO fmt, ## args)
#else
#define MAGDBG(fmt, args...)
#endif

#define GEO_SDA 14
#define GEO_SCL 5
#define GEO_RST 35

struct class *mag_class;
static struct i2c_driver hscd_driver;
static struct i2c_client *client_hscd = NULL;

static atomic_t flgEna;
static atomic_t delay;

static int hscd_i2c_readm(u8 *rxData, u8 length)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = client_hscd->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = rxData,
		 },
		{
		 .addr = client_hscd->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxData,
		 },
	};

	do {
		err = i2c_transfer(client_hscd->adapter, msgs, 2);
                if(err != 2) mdelay(10);
	} while ((err != 2) && (++tries < HSCD_I2C_RETRIES));

	if (err != 2) {
		dev_err(&client_hscd->adapter->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}


static int hscd_i2c_writem(u8 *txData, u8 length)
{
	int err;
	int tries = 0;
//	int i;

	struct i2c_msg msg[] = {
		{
		 .addr = client_hscd->addr,
		 .flags = 0,
		 .len = length,
		 .buf = txData,
		 },
	};
#if 0
	printk(KERN_INFO "[HSCD] i2c_writem : ");
	for (i=0; i<length;i++) printk("0X%02X, ", txData[i]);
	printk("\n");
#endif
	do {
		err = i2c_transfer(client_hscd->adapter, msg, 1);
                if(err != 1) mdelay(10);        
	} while ((err != 1) && (++tries < HSCD_I2C_RETRIES));

	if (err != 1) {
		dev_err(&client_hscd->adapter->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}


int hscd_self_test_A(void)
{
	u8 sx[2], cr1[1];

	printk(KERN_INFO "[HSCD] [%s]\n",__FUNCTION__);

    /* Control resister1 backup  */
    cr1[0] = HSCD_CTRL1;
    if (hscd_i2c_readm(cr1, 1)) return 0;
    else printk("[HSCD] Control resister1 value, %02X\n", cr1[0]);
    msleep(20);

    /* Stndby Mode  */
    if (cr1[0] & 0x80) {
        sx[0] = HSCD_CTRL1;
        sx[1] = 0x60;
        if (hscd_i2c_writem(sx, 2)) return 0;
    }

    /* Get inital value of self-test-A register  */
	sx[0] = HSCD_STBA;
    if (hscd_i2c_readm(sx, 1)) return 0;
    else printk("[HSCD] self test A register value, %02X\n", sx[0]);

    if (sx[0] != 0x55) {
        printk("error: self-test-A, initial value is %02X\n", sx[0]);
        return 0;
    }

    /* do self-test-A  */
	sx[0] = HSCD_CTRL3;
	sx[1] = 0x20;
	if (hscd_i2c_writem(sx, 2)) return 0;
    msleep(20);

    /* Get 1st value of self-test-A register  */
	sx[0] = HSCD_STBA;
    if (hscd_i2c_readm(sx, 1)) return 0;
    else printk("[HSCD] self test A register value, %02X\n", sx[0]);

    if (sx[0] != 0xAA) {
        printk("error: self-test-A, 1st value is %02X\n", sx[0]);
        return 0;
    }
    msleep(20);

    /* Get 2nd value of self-test-A register  */
	sx[0] = HSCD_STBA;
    if (hscd_i2c_readm(sx, 1)) return 0;
    else printk("[HSCD] self test A register value, %02X\n", sx[0]);

    if (sx[0] != 0x55) {
        printk("error: self-test-A, 2nd value is %02X\n", sx[0]);
        return 0;
    }

    /* Active Mode  */
    if (cr1[0] & 0x80) {
        sx[0] = HSCD_CTRL1;
        sx[1] = cr1[0];
        if (hscd_i2c_writem(sx, 2)) return 0;
    }

	return 1;
}

int hscd_self_test_B(void)
{
    int rc = 1;
    u8 sx[2], cr1[1];

	printk(KERN_INFO "[HSCD] [%s]\n",__FUNCTION__);

    /* Control resister1 backup  */
    cr1[0] = HSCD_CTRL1;
    if (hscd_i2c_readm(cr1, 1)) return 0;
    else printk("[HSCD] Control resister1 value, %02X\n", cr1[0]);

    msleep(20);

    /* Get inital value of self-test-B register  */
    sx[0] = HSCD_STBB;
    if (hscd_i2c_readm(sx, 1)) return 0;
    else printk("[HSCD] self test B register value, %02X\n", sx[0]);

    if (sx[0] != 0x55) {
        printk("error: self-test-B, initial value is %02X\n", sx[0]);
        return 0;
    }

    /* Active mode (Force state)  */
    sx[0] = HSCD_CTRL1;
    sx[1] = 0xC2;
    if (hscd_i2c_writem(sx, 2)) return 0;
    msleep(20);

    do {
        /* do self-test-B  */
	    sx[0] = HSCD_CTRL3;
	    sx[1] = 0x10;
	    if (hscd_i2c_writem(sx, 2)) {
            rc = 0;
            break;
        }
        msleep(20);

        /* Get 1st value of self-test-A register  */
	    sx[0] = HSCD_STBB;
        if (hscd_i2c_readm(sx, 1)) {
            rc = 0;
            break;
        }
        else printk("[HSCD] self test B register value, %02X\n", sx[0]);

        if (sx[0] != 0xAA) {
            if ((sx[0] < 0x01) || (sx[0] > 0x07)) {
                printk("error: self-test-B, 1st value is %02X\n", sx[0]);
                rc = 0;
                break;
            }
        }
        msleep(20);

        /* Get 2nd value of self-test-B register  */
	    sx[0] = HSCD_STBB;
        if (hscd_i2c_readm(sx, 1)) {
            rc = 0;
            break;
        }
        else printk("[HSCD] self test B register value, %02X\n", sx[0]);

        if (sx[0] != 0x55) {
            printk("error: self-test-B, 2nd value is %02X\n", sx[0]);
            rc = 0;
            break;
        }
    } while(0);

    /* Active Mode  */
    if (cr1[0] & 0x80) {
        sx[0] = HSCD_CTRL1;
        sx[1] = cr1[0];
        if (hscd_i2c_writem(sx, 2)) return 0;
    }

	return rc;
}

int hscd_get_magnetic_field_data(int *xyz)
{
	int err = -1;
	int i;
	u8 sx[6];

	sx[0] = HSCD_XOUT;
	err = hscd_i2c_readm(sx, 6);
	if (err < 0) return err;
	for (i=0; i<3; i++) {
		xyz[i] = (int) ((short)((sx[2*i+1] << 8) | (sx[2*i])));
	}
	

	/*** DEBUG OUTPUT - REMOVE ***/
	MAGDBG("Mag_I2C, x:%d, y:%d, z:%d\n",xyz[0], xyz[1], xyz[2]);
	/*** <end> DEBUG OUTPUT - REMOVE ***/


	return err;
}
EXPORT_SYMBOL(hscd_get_magnetic_field_data);

void hscd_activate(int flgatm, int flg, int dtime)
{
	u8 buf[2];

	MAGDBG("[HSCD] hscd_activate : flgatm=%d, flg=%d, dtime=%d\n", flgatm, flg, dtime);

	if (flg != 0) flg = 1;

	if      (dtime <=  10) buf[1] = (0x60 | 3<<2);		// 100Hz- 10msec
	else if (dtime <=  20) buf[1] = (0x60 | 2<<2);		//  50Hz- 20msec
	else if (dtime <=  70) buf[1] = (0x60 | 1<<2);		//  20Hz- 50msec
	else                   buf[1] = (0x60 | 0<<2);		//  10Hz-100msec
	buf[0]  = HSCD_CTRL1;
	buf[1] |= (flg<<7);
	//buf[1] |= 0x60;	                    // RS1:1(Reverse Input Substraction drive), RS2:1(13bit)
	hscd_i2c_writem(buf, 2);
	mdelay(1);

	if (flg) {
		buf[0] = HSCD_CTRL3;
		buf[1] = 0x02;
		hscd_i2c_writem(buf, 2);
	}

	if (flgatm) {
		atomic_set(&flgEna, flg);
		atomic_set(&delay, dtime);
	}
}
EXPORT_SYMBOL(hscd_activate);

static void hscd_register_init(void)
{
	printk(KERN_INFO "[HSCD] register_init\n");

}

static ssize_t hscd_fs_selftestA_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        int result = 0;
	result = hscd_self_test_A();
	printk(KERN_INFO "[HSCD] self test A result = %d\n", result);    
    
	return sprintf(buf, "%d\n", result);
}

static ssize_t hscd_fs_selftestB_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        int result = 0;    
	result = hscd_self_test_B();
	printk(KERN_INFO "[HSCD] self test B result = %d\n", result);        

	return sprintf(buf, "%d\n", result);
}

static ssize_t hscd_fs_readADC_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int d[3];
	hscd_get_magnetic_field_data(d);
	printk(KERN_INFO "[HSCD] read ADC x:%d y:%d z:%d\n",d[0],d[1],d[2]);

	return sprintf(buf, "%d,%d,%d\n",d[0],d[1],d[2]);
}

static ssize_t hscd_fs_readDAC_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d,%d,%d\n",0,0,0);;
}

static DEVICE_ATTR(self_testA, S_IRUGO | S_IRUSR | S_IROTH, hscd_fs_selftestA_show, NULL);
static DEVICE_ATTR(self_testB, S_IRUGO | S_IRUSR | S_IROTH, hscd_fs_selftestB_show, NULL);
static DEVICE_ATTR(read_adc, S_IRUGO | S_IRUSR | S_IROTH, hscd_fs_readADC_show, NULL);
static DEVICE_ATTR(read_dac, S_IRUGO | S_IRUSR | S_IROTH, hscd_fs_readDAC_show, NULL);


static int hscd_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "[HSCD] %s called\n",__func__);    
	return nonseekable_open(inode, file);
}

static int hscd_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "[HSCD] %s called\n",__func__);        
	return 0;
}

static long hscd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}
static struct file_operations hscd_fops = {
	.owner		= THIS_MODULE,
	.open		= hscd_open,
	.release	= hscd_release,
	.unlocked_ioctl     = hscd_ioctl,
};

static struct miscdevice hscd_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = HSCD_DRIVER_NAME,
	.fops = &hscd_fops,
};


static int hscd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int d[3];    
	int res = 0;

	printk(KERN_INFO "[HSCD] [%s]\n",__FUNCTION__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: functionality check failed\n", __FUNCTION__);
		res = -ENODEV;
		goto out;
	}

//	client_hscd = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
//	if (!client_hscd) {
//		dev_err(&client->adapter->dev, "failed to allocate memory for module data\n");
//		return -ENOMEM;
//	}

	printk(KERN_INFO "[HSCD] [%s] slave addr = %x\n", __func__, client->addr);

//	i2c_set_clientdata(client, client_hscd);
	client_hscd = client;

	res = misc_register(&hscd_device);
	if (res) {
		pr_err("%s: hscd_device register failed\n", __FUNCTION__);
		goto out;
	}

	hscd_register_init();

	//dev_info(&client->adapter->dev, "detected HSCD magnetic field sensor\n");

	hscd_activate(0, 1, atomic_read(&delay));
        mdelay(5);
	hscd_get_magnetic_field_data(d);
	printk(KERN_INFO "[HSCD] x:%d y:%d z:%d\n",d[0],d[1],d[2]);
	hscd_activate(0, 0, atomic_read(&delay));

	printk(KERN_INFO "[HSCD] [%s] end!!\n",__FUNCTION__);

	return 0;

out:
	return res;
}


static void hscd_shutdown(struct i2c_client *client)
{
	printk(KERN_INFO "[HSCD] shutdown\n");

	hscd_activate(0, 0, atomic_read(&delay));
}

static int hscd_suspend(struct device *dev)
{
	MAGDBG("[HSCD] suspend\n");

	hscd_activate(0, 0, atomic_read(&delay));
	return 0;
}

static int hscd_resume(struct device *dev)
{
	MAGDBG("[HSCD] resume\n");

	hscd_activate(0, atomic_read(&flgEna), atomic_read(&delay));
	return 0;
}


static int hscd_remove(struct i2c_client *client)
{
	printk(KERN_INFO "[HSCD] [%s]\n",__FUNCTION__);    
    
	return 0;
}

static const struct i2c_device_id ALPS_id[] = {
	{ HSCD_DRIVER_NAME, 0 },
	{ }
};

static const struct dev_pm_ops hscd_pm_ops = {
	.suspend = hscd_suspend,
	.resume = hscd_resume,
};

static struct i2c_driver hscd_driver = {
	.probe 		= hscd_probe,
	.remove 	= hscd_remove,
	.id_table	= ALPS_id,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= HSCD_DRIVER_NAME,
                .pm = &hscd_pm_ops,
	},
};


static int __init hscd_init(void)
{
	struct device *dev_t;    
	struct i2c_board_info i2c_info;
	struct i2c_adapter *adapter;
	int rc;

	printk(KERN_INFO "[HSCD] hscd_init\n");

	mag_class = class_create(THIS_MODULE, "magnetic");

	if (IS_ERR(mag_class)) 
		return PTR_ERR( mag_class );

	dev_t = device_create( mag_class, NULL, 0, "%s", "magnetic");

	if (device_create_file(dev_t, &dev_attr_self_testA) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_self_testA.attr.name);

	if (device_create_file(dev_t, &dev_attr_self_testB) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_self_testB.attr.name);

	if (device_create_file(dev_t, &dev_attr_read_adc) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_read_adc.attr.name);

	if (device_create_file(dev_t, &dev_attr_read_dac) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_read_dac.attr.name);

	if (IS_ERR(dev_t)) 
	{
		return PTR_ERR(dev_t);
	}
#if 0
        gpio_request(GEO_RST, "GEO_RST");
        gpio_direction_output(GEO_RST, 0);
        mdelay(50);

	printk(KERN_INFO "[HSCD] gpio_get_value of GEO_RST is %d\n",gpio_get_value(GEO_RST));
#endif    
	atomic_set(&flgEna, 0);
	atomic_set(&delay, 200);

	rc = i2c_add_driver(&hscd_driver);
	if (rc != 0) {
		printk(KERN_ERR "can't add i2c driver\n");
		rc = -ENOTSUPP; 
		return rc;
	}

	memset(&i2c_info, 0, sizeof(struct i2c_board_info));
	i2c_info.addr = I2C_HSCD_ADDR;
	strlcpy(i2c_info.type, HSCD_DRIVER_NAME , I2C_NAME_SIZE);
  
	adapter = i2c_get_adapter(I2C_BUS_NUMBER);
	if (!adapter) {
		printk(KERN_ERR "can't get i2c adapter %d\n", I2C_BUS_NUMBER);
		rc = -ENOTSUPP;
		goto probe_done;
}

#if 0
	client_hscd = i2c_new_device(adapter, &i2c_info);
	client_hscd->adapter->timeout = 0;
	client_hscd->adapter->retries = 0;
#endif
  
	i2c_put_adapter(adapter);
//	if (!client_hscd) {
//		printk(KERN_ERR "can't add i2c device at 0x%x\n",(unsigned int)i2c_info.addr);
//		//rc = -ENOTSUPP;  
//	}

	printk(KERN_INFO "[HSCD] hscd_init end!!!!\n");

	probe_done: 

	return rc;
}

static void __exit hscd_exit(void)
{
	printk(KERN_INFO "[HSCD] exit\n");

	i2c_del_driver(&hscd_driver);
}

module_init(hscd_init);
module_exit(hscd_exit);

MODULE_DESCRIPTION("Alps HSCDTD Device");
MODULE_AUTHOR("ALPS ELECTRIC CO., LTD.");
MODULE_LICENSE("GPL v2");

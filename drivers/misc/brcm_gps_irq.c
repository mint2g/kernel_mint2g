/*
 * ADXL345/346 Three-Axis Digital Accelerometers
 *
 * Enter bugs at http://blackfin.uclinux.org/
 *
 * Copyright (C) 2009 Michael Hennerich, Analog Devices Inc.
 * Licensed under the GPL-2 or later.
 *
 * History:
 * KOM: 06-10-2012 GPS irq driver was updated to i2c-client driver.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/bug.h>
#include <mach/board.h>

#define GPS_VERSION	"1.00"


#define MAX_RX_PCKT_LEN  256
#define MAX_TX_PCKT_LEN  256


#define PFX     "bcm47511: "

//#define GPS_IRQ_LOG
//efine USE_LINUX_2_6 0

#if defined(GPS_IRQ_LOG)
static unsigned long init_time = 0;
unsigned long gps_get_timer(void)
{
	struct timeval t;
	unsigned long now;

	do_gettimeofday(&t);
	now = t.tv_usec / 1000 + t.tv_sec * 1000;
	if ( init_time == 0 )
		init_time = now;

	return now - init_time;
}
#define PK_INFO(fmt, arg...)    printk(KERN_INFO PFX "#%06ldI %s: " fmt,gps_get_timer() % 1000000,__func__ ,##arg)
//#define PK_INFO(fmt, arg...)  do {} while (0)
#define PK_DBG(fmt, arg...)     printk(KERN_INFO PFX "#%06ldD %s: " fmt,gps_get_timer() % 1000000,__func__ ,##arg)
#else
#define PK_INFO(fmt, arg...)    dev_dbg(&client->dev, PFX "%s: " fmt,__func__,##arg);
#define PK_DBG(fmt, arg...)     do {} while (0)
#endif


struct gps_irq {
	wait_queue_head_t wait;
	int irq;
	int host_req_pin;
	struct miscdevice misc;
	struct i2c_client *client;
};


irqreturn_t gps_irq_handler(int irq, void *dev_id)
{
	int i;
	struct gps_irq *ac_data = dev_id;

	wake_up_interruptible(&ac_data->wait);

	i = gpio_get_value(ac_data->host_req_pin);
	PK_DBG("host_req %d\n",i);

	return IRQ_HANDLED;
}



static int gps_irq_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	struct gps_irq *ac_data = container_of(filp->private_data,
			struct gps_irq,
			misc);

	filp->private_data = ac_data;
	return ret;
}

static int gps_irq_release(struct inode *inode, struct file *filp)
{
	/*
	   struct gps_irq *ac_data = container_of(filp->private_data,
	   struct gps_irq,
	   misc);
	   */
	return 0;
}


static unsigned int gps_irq_poll(struct file *file, poll_table *wait)
{
	char gpio_value;
	struct gps_irq *ac_data = file->private_data;

	BUG_ON(!ac_data);

	poll_wait(file, &ac_data->wait, wait);

	gpio_value = gpio_get_value(ac_data->host_req_pin);

	PK_DBG("host_req=%d\n",gpio_value);

	if (gpio_value)
		return POLLIN | POLLRDNORM;

	return 0;
}


static ssize_t gps_irq_read(struct file *file, char __user *buf,
		size_t count, loff_t *offset)
{
	/* tomtom_gps_data_t *gps_drv_data = gps_device; */
	struct gps_irq *ac_data = file->private_data;
	struct i2c_client *client = ac_data->client;
	int num_read;
	uint8_t tmp[MAX_RX_PCKT_LEN];
	struct i2c_msg msgs[] = {
		{
			.addr  = client->addr,
			.flags = client->flags & I2C_M_TEN,
			.len   = 1,
			.buf   = tmp,
		},
		{
			.addr  = client->addr,
			.flags = (client->flags & I2C_M_TEN) | I2C_M_RD,
			.len   = count,
			.buf   = tmp,
		}
	};

	BUG_ON(!client);

	/* Adjust for binary packet size */
	if (count > MAX_RX_PCKT_LEN)
		count = MAX_RX_PCKT_LEN;

	PK_DBG("reading %d bytes\n", count);

	/* num_read = i2c_master_recv(client, tmp, count);*/
	num_read = i2c_transfer(client->adapter, msgs,2);

	if (num_read < 0) {
		/* dev_err(&client->dev, "got %d bytes instead of %d\n",
		   num_read, count);
		   */
		PK_DBG("read fail!!!");
		return num_read;
	} else {
		num_read = count;
	}

	return copy_to_user(buf, tmp, num_read) ? -EFAULT : num_read;
}


static ssize_t gps_irq_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct gps_irq *ac_data = file->private_data;
	struct i2c_client *client = ac_data->client;

	uint8_t tmp[MAX_TX_PCKT_LEN];
	int num_sent;

	BUG_ON(!client);

	if (count > MAX_TX_PCKT_LEN)
		count = MAX_TX_PCKT_LEN;

	PK_DBG("writing %d bytes\n", count);

	if (copy_from_user(tmp, buf, count))
		return -EFAULT;

	num_sent = i2c_master_send(client, tmp, count);

	PK_DBG("writing %d bytes returns %d",
			count, num_sent);

	return num_sent;
}


#if defined(USE_LINUX_2_6)
static int gps_irq_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
#else
static long gps_irq_ioctl( struct file *filp,
		unsigned int cmd, unsigned long arg)
#endif
{
	struct gps_irq *ac_data = filp->private_data;
	struct i2c_client *client = ac_data->client;

	BUG_ON(!client);

	/* dev_dbg(&client->dev, "ioctl: cmd = 0x%02x, arg=0x%02lx\n", cmd, arg); */
	PK_INFO("cmd = 0x%02x, arg=0x%02lx\n", cmd, arg);

	switch (cmd) {
		case I2C_SLAVE:
		case I2C_SLAVE_FORCE:
			if ((arg > 0x3ff) ||
					(((client->flags & I2C_M_TEN) == 0) &&
					 arg > 0x7f))
				return -EINVAL;
			client->addr = arg;
			PK_INFO("ioctl: client->addr = 0x%x\n", client->addr);
			break;

		case I2C_TENBIT:
			if (arg)
				client->flags |= I2C_M_TEN;
			else
				client->flags &= ~I2C_M_TEN;
			PK_INFO("ioctl: client->flags = 0x%x, %d bits\n", client->flags, client->flags & I2C_M_TEN ? 10:7);
			break;

			/*
			   case I2C_RDWR:
			   return i2cdev_ioctl_rdrw(client, arg);
			   */

		case I2C_RETRIES:
			client->adapter->retries = arg;
			PK_INFO("ioctl, client->adapter->retries = %d\n",client->adapter->retries);
			break;

		case I2C_TIMEOUT:
			/* For historical reasons, user-space sets the timeout
			 * value in units of 10 ms.
			 */
			client->adapter->timeout = msecs_to_jiffies(arg * 10);
			PK_INFO("ioctl, client->adapter->timeout = %d\n",client->adapter->timeout);
			break;
		default:
			return -ENOTTY;
	}
	return 0;
}


static const struct file_operations gps_irq_fops = {
	.owner = THIS_MODULE,
	.open = gps_irq_open,
	.release = gps_irq_release,
	.poll = gps_irq_poll,
	.read = gps_irq_read,
	.write = gps_irq_write,
#if defined(USE_LINUX_2_6)
	.ioctl = gps_irq_ioctl
#else
		.unlocked_ioctl = gps_irq_ioctl
#endif
};

extern void sc8810_i2c_set_clk(unsigned int id_nr, unsigned int freq);

static int gps_hostwake_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct gps_irq *ac_data = kzalloc(sizeof(struct gps_irq), GFP_KERNEL);
	int irq;
	int ret;
	int err;

	printk(KERN_INFO PFX "%s\n",__func__);

	sc8810_i2c_set_clk(3,400000);/*temp method set i2c clk to 400K*/

	init_waitqueue_head(&ac_data->wait);

	// Pass if return is fail because the HOST_REQ GPIO pin 25 has been already registered as input pin of 3rd party
	// in startup script for T8810:
	//     init.sp8810.3rdparty.rc for Linux version 2.6.35.7.
	//     init.sp8810.rc for Linux version Linux version 3.0.8-00029-g0511df3-dirty
	// E.g.
	// on init
	// mkdir -p /dev/socket/
	// chmod 777 /dev/socket/
	// write /sys/class/gpio/export 25
	// write /sys/class/gpio/gpio25/direction in
	//
	// On Linux version 2.6.35.7
	//
	// The following messages should be in boot log if 3rd party pins have been configured properly:
	// $dmesg
	// <4>[   11.804000] I2C: request gpio_25
	// <4>[   11.809000] I2C: request gpio_26
	// <4>[   11.815000] I2C: request gpio_27
	//
	// Check that all 3rd party pins has been configured properly after boot has been finished
	// $cat /d/gpio
	// GPIOs 0-220, sc8810-gpio:
	// gpio-25  (sysfs               ) in  lo irq-43 edge-rising
	// gpio-26  (sysfs               ) out lo
	// gpio-27  (sysfs               ) out lo
	//
	// The following line has been commented out for Linux version 2.6.35.7 and should be uncommnet for Linux version 3.0.8-00029-g0511df3-dirty
	// because this driver starts before GPIO driver.
#if defined(USE_LINUX_2_6)
#else
	if ((err = gpio_request(GPS_IRQ_NUM, "gps_irq"))) {
		printk(KERN_INFO PFX "Can't request HOST_REQ GPIO %d.It may be already registered in init.sp8810.3rdparty.rc/init.sp8810.rc\n",GPS_IRQ_NUM);
		//	   gpio_free(GPS_IRQ_NUM);
		//	   return -EIO;
	}
	// The following line gpio_export() has been added for Linux version 3.0.8-00029-g0511df3-dirty only
	gpio_export(GPS_IRQ_NUM,1);
#endif
	gpio_direction_input(GPS_IRQ_NUM);

#if defined(USE_LINUX_2_6)
	irq = sprd_alloc_gpio_irq(GPS_IRQ_NUM);
#else
	irq = gpio_to_irq(GPS_IRQ_NUM);
#endif
	if (irq < 0) {
		printk(KERN_ERR PFX "Could not get GPS IRQ = %d!\n",GPS_IRQ_NUM);
		gpio_free(GPS_IRQ_NUM);
		return -EIO;
	}

	ac_data->irq = irq;
	ac_data->host_req_pin = GPS_IRQ_NUM;
	ret = request_irq(irq, gps_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			//KOM -- IRQF_TRIGGER_RISING,
			"gps_interrupt",
			ac_data);

	ac_data->client = client;

	ac_data->misc.minor = MISC_DYNAMIC_MINOR;
	ac_data->misc.name = "gps_irq";
	ac_data->misc.fops = &gps_irq_fops;

	printk(KERN_INFO PFX "%s misc register, name %s, irq %d, GPS irq num %d\n",__func__,ac_data->misc.name,irq,GPS_IRQ_NUM);
	if (0 != (ret = misc_register(&ac_data->misc))) {
		printk(KERN_ERR PFX "cannot register gps miscdev on minor=%d (%d)\n",MISC_DYNAMIC_MINOR, ret);
		free_irq(ac_data->irq, ac_data);
		gpio_free(GPS_IRQ_NUM);
		return ret;
	}

	i2c_set_clientdata(client, ac_data);

	printk(KERN_INFO PFX "Initialized.\n");

	return 0;
}

static int gps_hostwake_remove(struct i2c_client *client)
{
	struct gps_platform_data *pdata;
	struct gps_irq *ac_data;

	pdata = client->dev.platform_data;

	ac_data = i2c_get_clientdata(client);
	free_irq(ac_data->irq, ac_data);
	misc_deregister(&ac_data->misc);
	kfree(ac_data);
	return 0;
}

static const struct i2c_device_id gpsi2c_id[] = {
	{"gpsi2c", 0},
	{}
};

static struct i2c_driver gps_driver = {
	.id_table = gpsi2c_id,
	.probe = gps_hostwake_probe,
	.remove = gps_hostwake_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "gpsi2c",
	},
};

static int gps_irq_init(void)
{
	printk(KERN_INFO PFX "Generic GPS IRQ Driver v%s\n", GPS_VERSION);
	return i2c_add_driver(&gps_driver);
}

static void gps_irq_exit(void)
{
	i2c_del_driver(&gps_driver);
}

module_init(gps_irq_init);
module_exit(gps_irq_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver for gps host wake interrupt");

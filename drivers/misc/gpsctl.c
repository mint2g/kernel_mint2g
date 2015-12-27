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

#include <linux/module.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <mach/board.h>
#include <gps/gpsctl.h>

#define GPIO_UNCONFIG 0xffffffff


struct clk    *gps_clk;
struct regulator *gps_regulator = NULL;
static struct platform_gpsctl_data *data;
void clk_32k_config(int is_on)
{
	if (is_on){
		clk_enable(gps_clk);
	}
	else{
		clk_disable(gps_clk);
	}
}

int gps_power_ctl(int is_on)
{
	int err;

	if (data->pwr_type && !strcmp(data->pwr_type, "pwr_gpio")) {
		if(GPIO_UNCONFIG != data->power_pin){
			gpio_set_value(data->power_pin, is_on);
		} else {
			printk("gpsctl warning : use gpio power but gpio not configured");
		}
	} else {
            	#if defined(CONFIG_ARCH_SC8825)
		gps_regulator = regulator_get(NULL, data->pwr_type);                   
		#else
		gps_regulator = regulator_get(NULL, REGU_NAME_GPS);
		#endif              
		if (IS_ERR(gps_regulator)) {
		    pr_err("gpsctl:could not get 1.8v regulator\n");
		    return -1;
		   }
		if (is_on) {
		    err = regulator_set_voltage(gps_regulator, 1800000, 1800000);
		     if (err)
			pr_err("gpsctl:could not set to 1800mv.\n");
			regulator_enable(gps_regulator);
		} else {
			regulator_disable(gps_regulator);
		}
	}
	return 0;
}

static void gps_reset(int is_reset)
{
	if(GPIO_UNCONFIG != data->reset_pin) {
		gpio_set_value(data->reset_pin, is_reset);
	}
	return 0;
}
static void gps_onoff(int is_onoff)
{
	if(GPIO_UNCONFIG != data->onoff_pin) {
		gpio_set_value(data->onoff_pin, is_onoff);
	}
}

static int gpsctl_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int gpsctl_release(struct inode *inode, struct file *filp)
{
    return 0;
}


ssize_t gpsctl_write(struct file *file, const char __user *buf,
        size_t count, loff_t *offset)
{
    return 0;
}

static long gpsctl_ioctl(struct file *file,unsigned int cmd, unsigned long arg)
{
	void __user *pa = (void __user *)arg;
	short is_poweron;
	short is_clkon;
	short is_reset;
	short is_on;
	switch (cmd) {
		case GPSCTL_IOC_SET_POWER:
			if (copy_from_user(&is_poweron, pa, sizeof(is_poweron)))
				return -EFAULT;
			gps_power_ctl(is_poweron);
			break;

		case GPSCTL_IOC_SET_CLK:
			if (copy_from_user(&is_clkon, pa, sizeof(is_clkon)))
				return -EFAULT;
			clk_32k_config(is_clkon);
			break;

		case GPSCTL_IOC_RESET:
			 if (copy_from_user(&is_reset, pa, sizeof(is_reset)))
				return -EFAULT;
			 gps_reset(is_reset);
			break;
		case GPSCTL_IOC_ONOFF:
			if (copy_from_user(&is_on, pa, sizeof(is_on)))
				return -EFAULT;
			gps_onoff(is_on);
			break;
		default:
			break;
	}
	return 0;
}

static const struct file_operations gpsctl_fops = {
	.open           =  gpsctl_open,
	.write          =  gpsctl_write,
	.unlocked_ioctl =  gpsctl_ioctl,
	.release        =  gpsctl_release,
	.owner          =  THIS_MODULE,
};

static struct miscdevice gpsctl_dev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name  =    "gpsctl",
	.fops  =    &gpsctl_fops
};

static int gpsctl_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct clk *clk_parent;
	data = pdev->dev.platform_data;
	/*
	gpio_request(GPIO_GPS_RESET,"gps_reset" );
	gpio_direction_output(GPIO_GPS_RESET,0);
	gpio_request(GPIO_GPS_ONOFF,"gps_onoff" );
	gpio_direction_output(GPIO_GPS_ONOFF,0);
	*/
        if(GPIO_UNCONFIG != data->reset_pin) {
		gpio_request(data->reset_pin, "gps_reset");
		gpio_direction_output(data->reset_pin, 0);
        }
        if(GPIO_UNCONFIG != data->onoff_pin) {
		gpio_request(data->onoff_pin, "gps_onoff");
		gpio_direction_output(data->onoff_pin, 0);
	}


	if (data->pwr_type && !strcmp(data->pwr_type, "pwr_gpio")) {
		if(GPIO_UNCONFIG != data->power_pin){
			gpio_request(data->power_pin, "gps_power");
			gpio_direction_output(data->power_pin, 0);
		} else {
			printk("gpsctl warning : use gpio power but gpio not configured");
		}
	}

	if (data->clk_type) {
		if (strcmp(data->clk_type, "clk_aux0") == 0) {
			printk("gps_clk: clk_type = = %s\n", "clk_aux0");
			gps_clk = clk_get(NULL, "clk_aux0");
			if (IS_ERR(gps_clk)) {
				printk("clock: failed to get clk_aux0\n");
			}

			clk_parent = clk_get(NULL, "ext_32k");
			if (IS_ERR(clk_parent)) {
				printk("failed to get parent ext_32k\n");
			}

			clk_set_parent(gps_clk, clk_parent);
			clk_set_rate(gps_clk, 32000);
		} else {
			printk("gps_clk: clk_type ! = %s\n", "clk_aux0");
		}
	} else {
		printk("gps_clk: clk_type is null\n");
	}

	ret = misc_register(&gpsctl_dev);

	return ret;
}

static int gpsctl_remove(struct platform_device *pdev)
{
	data = pdev->dev.platform_data;

//	gpio_free(GPIO_GPS_RESET);
//	gpio_free(GPIO_GPS_ONOFF);

        if(GPIO_UNCONFIG != data->reset_pin) {
		gpio_free(data->reset_pin);
        }
        if(GPIO_UNCONFIG != data->onoff_pin) {
		gpio_free(data->onoff_pin);
        }
	misc_deregister(&gpsctl_dev);
	return 0;
}



static struct platform_driver gpsctl_drv = {
	.probe   = gpsctl_probe,
	.remove  = gpsctl_remove,
	.driver  = {
		.name	= "gpsctl",
		.owner	= THIS_MODULE,
	},
};

static int __init gpsctl_init(void)
{
	int ret;
	ret = platform_driver_register(&gpsctl_drv);
	return ret;
}

static void __exit gpsctl_exit(void)
{
	platform_driver_unregister(&gpsctl_drv);
}

late_initcall(gpsctl_init);
module_exit(gpsctl_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Allen Zhang <Allen.Zhang@spreadtrum.com>");
MODULE_DESCRIPTION("Driver for gps hw control");

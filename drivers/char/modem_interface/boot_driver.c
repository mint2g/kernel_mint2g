/* linux/arch/arm/mach-sc8800g/boot_driver.c
 *
 * Common setup code for boot up MODEM
 *
 * Copyright (C) 2012 Spreadtrum
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/major.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <asm/uaccess.h>
#include <mach/modem_interface.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/mmc/sdio_channel.h>
#include <linux/wakelock.h>

#define 	DLOADER_NAME	        "dloader"
#define		READ_BUFFER_SIZE	(4096)
#define		WRITE_BUFFER_SIZE	(33*1024)

struct dloader_dev{
	enum MODEM_Mode_type	mode;
	int 			open_count;
	atomic_t 		read_excl;
	atomic_t 		write_excl;
};
static struct dloader_dev *dl_dev=NULL;
static struct wake_lock dloader_wake_lock;
static inline int dloader_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void dloader_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static int dloader_open(struct inode *inode,struct file *filp)
{
	int rval;
	if (dl_dev==NULL) {
		return -EIO;
	}
	if (dl_dev->open_count!=0) {
		printk(KERN_INFO "dloader_open %d \n",dl_dev->open_count);
		return -EBUSY;
	}


	dl_dev->open_count++;
	wake_lock(&dloader_wake_lock);
	if(modem_intf_open(MODEM_MODE_BOOT,0)< 0){
		printk(KERN_INFO "modem_intf_open failed \n");
		return -EBUSY;
	}
	dl_dev->mode = MODEM_MODE_BOOT;

	printk(KERN_INFO "DLoader_open %d times: %d\n", dl_dev->open_count, rval);
	return 0;
}
static int dloader_read(struct file *filp,char __user *buf,size_t count,loff_t *pos)
{
	int read_len=0;

	if ((dl_dev==NULL)||(dl_dev->open_count!=1)) {
		return -EIO;
	}

	if (dloader_lock(&dl_dev->read_excl))
		return -EBUSY;

	if(dl_dev->mode==MODEM_MODE_BOOT){
		read_len = modem_intf_read(buf,count,0);
	}

	dloader_unlock(&dl_dev->read_excl);

	return read_len;
}

static int dloader_write(struct file *filp, const char __user *buf,size_t count,loff_t *pos)
{
	if ((dl_dev==NULL)||(dl_dev->open_count!=1))
		return -EIO;

	if (dloader_lock(&dl_dev->write_excl))
		return -EBUSY;
	if (dl_dev->mode==MODEM_MODE_BOOT) {
		modem_intf_write(buf,count,0);
	}

	dloader_unlock(&dl_dev->write_excl);

	return count;
}

static int dloader_release(struct inode *inode,struct file *filp)
{
	printk(KERN_INFO "dloader_release time = 0x%x \n",jiffies);
	wake_unlock(&dloader_wake_lock);
	dl_dev->open_count--;
	modem_intf_set_mode(MODEM_MODE_NORMAL,0);
	dl_dev->mode = MODEM_MODE_NORMAL;
	return 0;
}

static struct file_operations dloader_fops = {
	.owner = THIS_MODULE,
	.read  = dloader_read,
	.write = dloader_write,
	.open  = dloader_open,
	.release = dloader_release,
};
static struct miscdevice dl_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DLOADER_NAME,
	.fops = &dloader_fops,
};
static int __init init(void)
{
	struct dloader_dev *dev;
	int		   err=0;
	printk(KERN_INFO "dloader_init\n");

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	dev->open_count = 0;

	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev ->write_excl, 0);
	dl_dev = dev;
	wake_lock_init(&dloader_wake_lock,WAKE_LOCK_SUSPEND,"dloader_wake_lock");
	err = misc_register(&dl_device);

	if(err){
		printk(KERN_INFO "cdev_add failed!!!\n");
		kfree(dev);
	}
	return err;
}
static void __exit cleanup(void)
{
	misc_deregister(&dl_device);
	kfree(dl_dev);
	dl_dev = NULL;
}
module_init(init);
module_exit(cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPRD boot driver");

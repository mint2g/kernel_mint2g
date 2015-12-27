/*
 * For FSA880
 *
 * Copyright (C) 2009 Samsung Electronics
 * Wonguk Jeong <wonguk.jeong@samsung.com>
 * Minkyu Kang <mk7.kang@samsung.com>
 *
 * Modified by Sumeet Pawnikar <sumeet.p@samsung.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/fsa880.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/power_supply.h>
//#include <mach/mfp.h>

#include <linux/mfd/88pm860x.h>
#include <linux/wakelock.h>

#if defined(CONFIG_SPA)
#include <linux/power/spa.h>
static void (*spa_external_event)(int, int) = NULL;
#endif

#if defined(CONFIG_SEC_CHARGING_FEATURE)
#include <linux/spa_power.h>
#include <linux/spa_agent.h>
#endif

#include <linux/delay.h>
#define MUIC_DETECT_GPIO 136

/* FSA880 I2C registers */
#define FSA880_REG_DEVID              0x01
#define FSA880_REG_CTRL               0x02
#define FSA880_REG_INT1               0x03
#define FSA880_REG_INT2               0x04
#define FSA880_REG_INT1_MASK          0x05
#define FSA880_REG_INT2_MASK          0x06
#define FSA880_REG_ADC                        0x07
#define FSA880_REG_TIMING1            0x08
#define FSA880_REG_TIMING2            0x09
#define FSA880_REG_DEV_T1             0x0a
#define FSA880_REG_DEV_T2             0x0b
#define FSA880_REG_BTN1               0x0c
#define FSA880_REG_BTN2               0x0d
#define FSA880_REG_CK                 0x0e
#define FSA880_REG_CK_INT1            0x0f
#define FSA880_REG_CK_INT2            0x10
#define FSA880_REG_CK_INTMASK1                0x11
#define FSA880_REG_CK_INTMASK2                0x12
#define FSA880_REG_MANSW1             0x13
#define FSA880_REG_MANSW2             0x14

/* MANSW1 */
#define VAUDIO                 0x90
#define UART                   0x6c
#define AUDIO                  0x48
#define DHOST                  0x24
#define AUTO                   0x0

/*FSA885 MANSW1*/
#define VAUDIO_880            0x93
#define AUDIO_880             0x4B
#define DHOST_880             0x27

/* MANSW2 */
#define MANSW2_JIG		(1 << 2)

/* Control */
#define SWITCH_OPEN            (1 << 4)
#define RAW_DATA               (1 << 3)
#define MANUAL_SWITCH          (1 << 2)
#define WAIT                   (1 << 1)
#define INT_MASK               (1 << 0)
#define CTRL_MASK              (SWITCH_OPEN | RAW_DATA | MANUAL_SWITCH | \
                                       WAIT | INT_MASK)
/* Device Type 1*/
#define DEV_USB_OTG            (1 << 7)
#define DEV_DEDICATED_CHG      (1 << 6)
#define DEV_USB_CHG            (1 << 5)
#define DEV_CAR_KIT            (1 << 4)
#define DEV_UART               (1 << 3)
#define DEV_USB                        (1 << 2)
#define DEV_AUDIO_2            (1 << 1)
#define DEV_AUDIO_1            (1 << 0)

#define FSA880_DEV_T1_HOST_MASK               (DEV_USB_OTG)
#define FSA880_DEV_T1_USB_MASK                (DEV_USB)
#define FSA880_DEV_T1_UART_MASK       (DEV_UART)
#define FSA880_DEV_T1_CHARGER_MASK    (DEV_DEDICATED_CHG | DEV_USB_CHG|DEV_CAR_KIT)
#define FSA880_DEV_T1_AUDIO_MASK    (DEV_AUDIO_1 | DEV_AUDIO_2)

/* Device Type 2*/
#define DEV_AV                 (1 << 6)
#define DEV_TTY                        (1 << 5)
#define DEV_PPD                        (1 << 4)
#define DEV_JIG_UART_OFF       (1 << 3)
#define DEV_JIG_UART_ON                (1 << 2)
#define DEV_JIG_USB_OFF                (1 << 1)
#define DEV_JIG_USB_ON         (1 << 0)

#define FSA880_DEV_T2_USB_MASK                (DEV_JIG_USB_OFF | DEV_JIG_USB_ON)
#define FSA880_DEV_T2_UART_MASK       (DEV_JIG_UART_OFF | DEV_JIG_UART_ON)

#define FSA880_DEV_T2_JIG_MASK                (DEV_JIG_USB_OFF | DEV_JIG_USB_ON | \
                                       DEV_JIG_UART_OFF | DEV_JIG_UART_ON)

#define DEV_MHL                 (DEV_AV)
#define FSA880_DEV_T2_MHL_MASK         (DEV_MHL)


struct fsa880_usbsw {
       struct i2c_client               *client;
       struct fsa880_platform_data    *pdata;
       struct work_struct              work;
       int                             dev1;
       int                             dev2;
       int                             mansw;
	   u8                              id;
};

static struct fsa880_usbsw *chip;


static struct wake_lock JIGConnect_idle_wake;
static struct wake_lock JIGConnect_suspend_wake;

static int isProbe=0;
enum {
	muicTypeTI6111 = 1,
	muicTypeFSA880 = 2,
	muicTypeFSA = 3,
};
static int muic_type=0;
//static int isManual=0;

static int fsa880_write_reg(struct i2c_client *client,        u8 reg, u8 data)
{
       int ret = 0;
       u8 buf[2];
       struct i2c_msg msg[1];

       buf[0] = reg;
       buf[1] = data;

       msg[0].addr = client->addr;
       msg[0].flags = 0;
       msg[0].len = 2;
       msg[0].buf = buf;

       ret = i2c_transfer(client->adapter, msg, 1);
       if (ret != 1) {
               printk("\n [FSA880] i2c Write Failed (ret=%d) \n", ret);
               return -1;
       }
      
       return ret;
}

static int fsa880_read_reg(struct i2c_client *client, u8 reg, u8 *data)
{
       int ret = 0;
       u8 buf[1];
       struct i2c_msg msg[2];

       buf[0] = reg;

        msg[0].addr = client->addr;
        msg[0].flags = 0;
        msg[0].len = 1;
        msg[0].buf = buf;

        msg[1].addr = client->addr;
        msg[1].flags = I2C_M_RD;
        msg[1].len = 1;
        msg[1].buf = buf;
		
       ret = i2c_transfer(client->adapter, msg, 2);
       if (ret != 2) {
               printk("\n [FSA880] i2c Read Failed (ret=%d) \n", ret);
               return -1;
       }
       *data = buf[0];

       return 0;
}

static void fsa880_read_adc_value(void)
{
	u8 adc=0;
	struct fsa880_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;

	fsa880_read_reg(client, FSA880_REG_ADC, &adc);
}

static void fsa880_id_open(void)
{
	struct fsa880_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;

	fsa880_write_reg(client, 0x1B, 1);
}

void fsa880_set_switch(const char *buf)
{
       struct fsa880_usbsw *usbsw = chip;
       struct i2c_client *client = usbsw->client;
       u8 value = 0;
       unsigned int path = 0;

       fsa880_read_reg(client, FSA880_REG_CTRL, &value);

	if (!strncmp(buf, "VAUDIO", 6)) {
		if(usbsw->id == 0)
			path = VAUDIO_880;
		else
			path = VAUDIO;
		value &= ~MANUAL_SWITCH;
	} else if (!strncmp(buf, "UART", 4)) {
		path = UART;
		value &= ~MANUAL_SWITCH;
	} else if (!strncmp(buf, "AUDIO", 5)) {
		if(usbsw->id == 0)
			path = AUDIO_880;
		else
			path = AUDIO;
		value &= ~MANUAL_SWITCH;
	} else if (!strncmp(buf, "DHOST", 5)) {
		path = DHOST;
		value &= ~MANUAL_SWITCH;
	} else if (!strncmp(buf, "AUTO", 4)) {
		path = AUTO;
		value |= MANUAL_SWITCH;
	} else {
		printk(KERN_ERR "Wrong command\n");
		return;
	}

       usbsw->mansw = path;
       fsa880_write_reg(client, FSA880_REG_MANSW1, path);
       fsa880_write_reg(client, FSA880_REG_CTRL, value);
}
EXPORT_SYMBOL_GPL(fsa880_set_switch);

ssize_t fsa880_get_switch(char *buf)
{
struct fsa880_usbsw *usbsw = chip;
       struct i2c_client *client = usbsw->client;
       u8 value;

       fsa880_read_reg(client, FSA880_REG_MANSW1, &value);

       if (value == VAUDIO)
               return sprintf(buf, "VAUDIO\n");
       else if (value == UART)
               return sprintf(buf, "UART\n");
       else if (value == AUDIO)
               return sprintf(buf, "AUDIO\n");
       else if (value == DHOST)
               return sprintf(buf, "DHOST\n");
       else if (value == AUTO)
               return sprintf(buf, "AUTO\n");
       else
               return sprintf(buf, "%x", value);
}
EXPORT_SYMBOL_GPL(fsa880_get_switch);

static ssize_t fsa880_show_status(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
       struct fsa880_usbsw *usbsw = dev_get_drvdata(dev);
       struct i2c_client *client = usbsw->client;
       u8 devid, ctrl, adc, dev1, dev2, intr;
       u8 intmask1, intmask2, time1, time2, mansw1;

       fsa880_read_reg(client, FSA880_REG_DEVID, &devid);
       fsa880_read_reg(client, FSA880_REG_CTRL, &ctrl);
       fsa880_read_reg(client, FSA880_REG_ADC, &adc);
       fsa880_read_reg(client, FSA880_REG_INT1_MASK, &intmask1);
       fsa880_read_reg(client, FSA880_REG_INT2_MASK, &intmask2);
       fsa880_read_reg(client, FSA880_REG_DEV_T1, &dev1);
       fsa880_read_reg(client, FSA880_REG_DEV_T2, &dev2);
       fsa880_read_reg(client, FSA880_REG_TIMING1, &time1);
       fsa880_read_reg(client, FSA880_REG_TIMING2, &time2);
       fsa880_read_reg(client, FSA880_REG_MANSW1, &mansw1);

       fsa880_read_reg(client, FSA880_REG_INT1, &intr);
       intr &= 0xffff;

       return sprintf(buf, "Device ID(%02x), CTRL(%02x)\n"
                       "ADC(%02x), DEV_T1(%02x), DEV_T2(%02x)\n"
                       "INT(%04x), INTMASK(%02x, %02x)\n"
                       "TIMING(%02x, %02x), MANSW1(%02x)\n",
                       devid, ctrl, adc, dev1, dev2, intr,
                       intmask1, intmask2, time1, time2, mansw1);
}

static ssize_t fsa880_show_manualsw(struct device *dev,
               struct device_attribute *attr, char *buf)
{
       return fsa880_get_switch(buf);

}

static ssize_t fsa880_set_manualsw(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count)
{
       fsa880_set_switch(buf);
       return count;
}

static ssize_t fsa880_set_syssleep(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count)
{
	struct fsa880_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;
	u8 value = 0;

	if (!strncmp(buf, "1", 1))
	{
		wake_unlock(&JIGConnect_idle_wake);
		wake_unlock(&JIGConnect_suspend_wake);

		if(muic_type==muicTypeFSA880)
		{
			fsa880_read_reg(client, FSA880_REG_MANSW2, &value);
			fsa880_write_reg(client, FSA880_REG_MANSW2, 0x00);

			fsa880_read_reg(client, FSA880_REG_CTRL, &value);
			fsa880_write_reg(client, FSA880_REG_CTRL, 0x00);
		}
	}
	else
	{
		fsa880_read_reg(client, FSA880_REG_CTRL, &value);
		value |= MANUAL_SWITCH;
		fsa880_write_reg(client, FSA880_REG_CTRL, value);
	}
	return count;
}
					

static DEVICE_ATTR(status, S_IRUGO, fsa880_show_status, NULL);
static DEVICE_ATTR(switch, S_IRUGO | S_IWGRP,
               fsa880_show_manualsw, fsa880_set_manualsw);
static DEVICE_ATTR(syssleep, S_IWUSR, NULL, fsa880_set_syssleep);

static struct attribute *fsa880_attributes[] = {
       &dev_attr_status.attr,
       &dev_attr_switch.attr,
       &dev_attr_syssleep.attr,
       NULL
};

static const struct attribute_group fsa880_group = {
       .attrs = fsa880_attributes,
};

static irqreturn_t fsa880_irq_handler(int irq, void *data)
{
       struct fsa880_usbsw *usbsw = data;

       if (!work_pending(&usbsw->work)) {
               disable_irq_nosync(irq);
               schedule_work(&usbsw->work);
                    }

       return IRQ_HANDLED;
}

#if defined(CONFIG_SEC_CHARGING_FEATURE)
extern void sprd_update_charger_type (enum power_supply_type type);
#endif

static void fsa880_detect_dev(struct fsa880_usbsw *usbsw, u8 intr)
{
       u8 val1, val2;
       struct fsa880_platform_data *pdata = usbsw->pdata;
       struct i2c_client *client = usbsw->client;

       fsa880_read_reg(client, FSA880_REG_DEV_T1, &val1);
       fsa880_read_reg(client, FSA880_REG_DEV_T2, &val2);
	printk("%s val1=0x%x val2=0x%x\n",__func__,val1,val2);

	if((intr==0x01) &&(val1==0x00) && (val2==0x00) && (isProbe == 0))
	{
		fsa880_read_adc_value();

		msleep(50);

		fsa880_read_reg(client, FSA880_REG_DEV_T1, &val1);
		fsa880_read_reg(client, FSA880_REG_DEV_T2, &val2);
	}

       /* Attached */
       if (intr & (1 << 0)) 
	{
               if (val1 & FSA880_DEV_T1_USB_MASK ||val2 & FSA880_DEV_T2_USB_MASK) {
			printk("[FSA880] FSA880_USB ATTACHED*****\n");
#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				spa_external_event(SPA_CATEGORY_DEVICE, SPA_DEVICE_EVENT_USB_ATTACHED);
			}
#endif
#if defined(CONFIG_SEC_CHARGING_FEATURE)
						sprd_update_charger_type(POWER_SUPPLY_TYPE_USB);
						spa_event_handler(SPA_EVT_CHARGER, POWER_SUPPLY_TYPE_USB);
#endif	
               }

               if (val1 & FSA880_DEV_T1_UART_MASK ||val2 & FSA880_DEV_T2_UART_MASK) {
                       //if (pdata->uart_cb)
                       //        pdata->uart_cb(FSA9480_ATTACHED);				   
               }

               if (val1 & FSA880_DEV_T1_CHARGER_MASK) {
			printk("[FSA880] Charger ATTACHED*****\n");
#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				spa_external_event(SPA_CATEGORY_DEVICE, SPA_DEVICE_EVENT_TA_ATTACHED);
			}
#endif
#if defined(CONFIG_SEC_CHARGING_FEATURE)
						sprd_update_charger_type(POWER_SUPPLY_TYPE_USB_DCP);
						spa_event_handler(SPA_EVT_CHARGER, POWER_SUPPLY_TYPE_USB_DCP);
#endif
               }

               if (val2 & FSA880_DEV_T2_JIG_MASK) {
			printk("[FSA880] JIG ATTACHED*****\n"); 			   
			wake_lock(&JIGConnect_idle_wake);
			wake_lock(&JIGConnect_suspend_wake);
#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				spa_external_event(SPA_CATEGORY_DEVICE, SPA_DEVICE_EVENT_JIG_ATTACHED);
			}
#endif
               }
       } 
	else if (intr & (1 << 1)) 
	{    /* DETACH */
               if (usbsw->dev1 & FSA880_DEV_T1_USB_MASK ||usbsw->dev2 & FSA880_DEV_T2_USB_MASK) {
                       printk("[FSA880] FSA880_USB Detached*****\n");
#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				spa_external_event(SPA_CATEGORY_DEVICE, SPA_DEVICE_EVENT_USB_DETACHED);
			}
#endif
#if defined(CONFIG_SEC_CHARGING_FEATURE)
					sprd_update_charger_type(POWER_SUPPLY_TYPE_BATTERY);
					spa_event_handler(SPA_EVT_CHARGER, POWER_SUPPLY_TYPE_BATTERY);
#endif
               }

               if (usbsw->dev1 & FSA880_DEV_T1_UART_MASK ||usbsw->dev2 & FSA880_DEV_T2_UART_MASK) {
                       //if (pdata->uart_cb)
                          //     pdata->uart_cb(FSA9480_DETACHED);
               }

               if (usbsw->dev1 & FSA880_DEV_T1_CHARGER_MASK) {
			printk("[FSA880] Charger Detached*****\n");
#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				spa_external_event(SPA_CATEGORY_DEVICE, SPA_DEVICE_EVENT_TA_DETACHED);
			}
#endif
#if defined(CONFIG_SEC_CHARGING_FEATURE)
							sprd_update_charger_type(POWER_SUPPLY_TYPE_BATTERY);
							spa_event_handler(SPA_EVT_CHARGER, POWER_SUPPLY_TYPE_BATTERY);
#endif
               }

               if (usbsw->dev2 & FSA880_DEV_T2_JIG_MASK) {      
			printk("[FSA880] JIG Detached*****\n");			   	
			wake_unlock(&JIGConnect_idle_wake);
			wake_unlock(&JIGConnect_suspend_wake);
#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				spa_external_event(SPA_CATEGORY_DEVICE, SPA_DEVICE_EVENT_JIG_DETACHED);
			}
#endif
               }
       }

       usbsw->dev1 = val1;
       usbsw->dev2 = val2;

       chip->dev1 = val1;
       chip->dev2 = val2;

}

static void fsa880_work_cb(struct work_struct *work)
{
       u8 intr, intr2;//, val1, val2;
       struct fsa880_usbsw *usbsw = container_of(work, struct fsa880_usbsw, work);
       struct i2c_client *client = usbsw->client;

       /* clear interrupt */
	fsa880_read_reg(client, FSA880_REG_INT1, &intr);
	printk("[FSA880] %s: intr=0x%x \n",__func__,intr);
	intr &= 0xffff;

	enable_irq(client->irq); 

	if(intr== 0x00)
	{	
		printk("[FSA880] (intr== 0x00) in work_cb !!!!!\n");
		fsa880_read_adc_value();

		return;
	}

         /* device detection */
       fsa880_detect_dev(usbsw, intr);
}

static int fsa880_irq_init(struct fsa880_usbsw *usbsw)
{
       struct i2c_client *client = usbsw->client;
       int ret, irq = -1;

       INIT_WORK(&usbsw->work, fsa880_work_cb);

	ret = gpio_request(MUIC_DETECT_GPIO,"fsa880 irq");
	if(ret)
	{
		dev_err(&client->dev,"fsa880: Unable to get gpio %d\n", client->irq);
		goto gpio_out;
	}
	
	gpio_direction_input(MUIC_DETECT_GPIO);
	client->irq = gpio_to_irq(MUIC_DETECT_GPIO);

       ret = request_irq(client->irq, fsa880_irq_handler,
                      IRQF_NO_SUSPEND|IRQF_TRIGGER_FALLING/*|IRQF_TRIGGER_LOW*/,
                       "fsa880 micro USB", usbsw); /*2. Low level detection*/
       if (ret) {
               dev_err(&client->dev,
                       "fsa880: Unable to get IRQ %d\n", irq);
               goto out;
       }

       return 0;
gpio_out:
	   gpio_free(MUIC_DETECT_GPIO);
out:
       return ret;

}

static int __devinit fsa880_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
       struct fsa880_platform_data *pdata = client->dev.platform_data;
       struct fsa880_usbsw *usbsw;
	unsigned int data;
       int ret = 0;
	u8 devID;
	
       u8 intr, intr2;
       u8 mansw1;
       unsigned int ctrl = CTRL_MASK;

       printk("[FSA880] PROBE ......\n");
	   
	isProbe = 1;
	   
       //add for AT Command 
       wake_lock_init(&JIGConnect_idle_wake, WAKE_LOCK_IDLE, "jig_connect_idle_wake");
       wake_lock_init(&JIGConnect_suspend_wake, WAKE_LOCK_SUSPEND, "jig_connect_suspend_wake");
	   
       usbsw = kzalloc(sizeof(struct fsa880_usbsw), GFP_KERNEL);
       if (!usbsw) {
               dev_err(&client->dev, "failed to allocate driver data\n");
               return -ENOMEM;
       }

       usbsw->client = client;
       usbsw->pdata = client->dev.platform_data;

       chip = usbsw;

       i2c_set_clientdata(client, usbsw);

#if defined(CONFIG_SPA)
	spa_external_event = spa_get_external_event_handler();
#endif

	/* clear interrupt */
	fsa880_read_reg(client, FSA880_REG_INT1, &intr);

	fsa880_read_reg(client, FSA880_REG_DEVID, &devID);
	if(devID==0x0a)
		muic_type=muicTypeTI6111;
	else if(devID==0x00)
		muic_type=muicTypeFSA880;
	else
		muic_type=muicTypeFSA;

	if(muic_type==muicTypeFSA880)
	{
		intr &= 0xffff;
		/* set control register */
		fsa880_write_reg(client, FSA880_REG_CTRL, 0x04);
	}
	else
		printk("[FSA880] Error!!!! No Type. Check dev ID(0x01 addr) ......\n");

       ret = fsa880_irq_init(usbsw);
       if (ret)
               goto fsa880_probe_fail;

       ret = sysfs_create_group(&client->dev.kobj, &fsa880_group);
       if (ret) {
               dev_err(&client->dev,
                               "[FSA880] Creating fsa880 attribute group failed");
               goto fsa880_probe_fail2;
       }

       /* device detection */
       fsa880_detect_dev(usbsw, 1);
	isProbe = 0;

	/*reset UIC*/
	if(muic_type==muicTypeFSA880)
		fsa880_write_reg(client, FSA880_REG_CTRL, 0x04);
  
       printk("[FSA880] PROBE Done.\n");
       return 0;

fsa880_probe_fail2:
       if (client->irq)
               free_irq(client->irq, NULL);
fsa880_probe_fail:
       i2c_set_clientdata(client, NULL);
       kfree(usbsw);
       return ret;
}

static int __devexit fsa880_remove(struct i2c_client *client)
{
       struct fsa880_usbsw *usbsw = i2c_get_clientdata(client);
       if (client->irq)
               free_irq(client->irq, NULL);
       i2c_set_clientdata(client, NULL);

       sysfs_remove_group(&client->dev.kobj, &fsa880_group);
       kfree(usbsw);
       return 0;
}

static int fsa880_suspend(struct i2c_client *client)
{
	printk("[FSA880] fsa880_suspend  enable_irq_wake...\n");
	enable_irq_wake(client->irq);

       return 0;
}


#ifdef CONFIG_PM
static int fsa880_resume(struct i2c_client *client)
{
	printk("[FSA880] fsa880_resume  disable_irq_wake...\n");
	disable_irq_wake(client->irq);
	return 0;
}
#else
#define fsa880_resume         NULL
#endif

static const struct i2c_device_id fsa880_id[] = {
       {"fsa880", 0},
       {}
};
MODULE_DEVICE_TABLE(i2c, fsa880_id);

static struct i2c_driver fsa880_i2c_driver = {
       .driver = {
               .name = "fsa880",
       },
       .probe = fsa880_probe,
       .remove = __devexit_p(fsa880_remove),
       .suspend=fsa880_suspend,
       .resume = fsa880_resume,
       .id_table = fsa880_id,
};

static int __init fsa880_init(void)
{
       return i2c_add_driver(&fsa880_i2c_driver);
}

module_init(fsa880_init);

#ifdef CONFIG_CHARGER_DETECT_BOOT
charger_module_init(fsa880_init);
#endif

static void __exit fsa880_exit(void)
{
       i2c_del_driver(&fsa880_i2c_driver);
}

module_exit(fsa880_exit);

MODULE_AUTHOR("Wonguk.Jeong <wonguk.jeong@samsung.com>");
MODULE_DESCRIPTION("FSA880 USB Switch driver");
MODULE_LICENSE("GPL");




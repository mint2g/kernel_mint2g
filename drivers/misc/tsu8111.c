/*
 * For TSU 8111 micro usb IC
 *
 * Copyright (C) 2012 Samsung Electronics
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
#include <linux/tsu8111.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/delay.h>

#if defined(CONFIG_SPA)
#include <linux/power/spa.h>
static void (*spa_external_event)(int, int) = NULL;
#endif

/* tsu8111 I2C registers */
#define tsu8111_REG_DEVID              0x01
#define tsu8111_REG_CTRL               0x02
#define tsu8111_REG_INT1               0x03
#define tsu8111_REG_INT2               0x04
#define tsu8111_REG_INT1_MASK          0x05
#define tsu8111_REG_INT2_MASK          0x06
#define tsu8111_REG_ADC                0x07
#define tsu8111_REG_TIMING1            0x08
#define tsu8111_REG_TIMING2            0x09
#define tsu8111_REG_DEV_T1             0x0a
#define tsu8111_REG_DEV_T2             0x0b
#define tsu8111_REG_BTN1               0x0c
#define tsu8111_REG_BTN2               0x0d
#define tsu8111_REG_CK                 0x0e
#define tsu8111_REG_CK_INT1            0x0f
#define tsu8111_REG_CK_INT2            0x10
#define tsu8111_REG_CK_INTMASK1        0x11
#define tsu8111_REG_CK_INTMASK2        0x12
#define tsu8111_REG_MANSW1             0x13
#define tsu8111_REG_MANSW2             0x14

// Luke
#define tsu8111_REG_RESET              0x1B
#define tsu8111_REG_CHG_CTRL1          0x20
#define tsu8111_REG_CHG_CTRL2          0x21
#define tsu8111_REG_CHG_CTRL3          0x22
#define tsu8111_REG_CHG_CTRL4          0x23
#define tsu8111_REG_CHG_INT            0x24
#define tsu8111_REG_CHG_INT_MASK       0x25
#define tsu8111_REG_CHG_STATUS         0x26

/* MANSW1 */
#define VAUDIO                 0x90
#define UART                   0x6c
#define AUDIO                  0x48
#define DHOST                  0x24
#define AUTO                   0x0

/*FSA9485 MANSW1*/
#define VAUDIO_9485            0x93
#define AUDIO_9485             0x4B
#define DHOST_9485             0x27

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
#define DEV_USB                (1 << 2)
#define DEV_VBUS               (1 << 1)
#define DEV_MHL                (1 << 0)

#define tsu8111_DEV_T1_HOST_MASK               (DEV_USB_OTG)
#define tsu8111_DEV_T1_USB_MASK                (DEV_USB)
#define tsu8111_DEV_T1_UART_MASK       (DEV_UART)
#define tsu8111_DEV_T1_CHARGER_MASK    (DEV_DEDICATED_CHG | DEV_USB_CHG)
#define tsu8111_DEV_T1_AUDIO_MASK    (DEV_AUDIO_1 | DEV_AUDIO_2)

/* Device Type 2*/
#define DEV_AV                 (1 << 6)
#define DEV_TTY                (1 << 5)
#define DEV_PPD                (1 << 4)
#define DEV_JIG_UART_OFF       (1 << 3)
#define DEV_JIG_UART_ON        (1 << 2)
#define DEV_JIG_USB_OFF        (1 << 1)
#define DEV_JIG_USB_ON         (1 << 0)

#define tsu8111_DEV_T2_USB_MASK                (DEV_JIG_USB_OFF | DEV_JIG_USB_ON)
#define tsu8111_DEV_T2_UART_MASK       (DEV_JIG_UART_OFF | DEV_JIG_UART_ON)

#define tsu8111_DEV_T2_JIG_MASK                (DEV_JIG_USB_OFF | DEV_JIG_USB_ON | \
                                       DEV_JIG_UART_OFF | DEV_JIG_UART_ON)

#define tsu8111_DEV_T2_MHL_MASK         (DEV_MHL)


/* Charger Control 1 Luke*/
#define CH_DIS                 (1 << 7)

/* Charger Interrupt*/
#define CH_DONE                (1 << 4)

struct tsu8111_usbsw {
       struct i2c_client               *client;
       struct tsu8111_platform_data    *pdata;
       struct work_struct              work;
       int                             dev1;
       int                             dev2;
       int                             mansw;
	   u8                              id;
};

static struct tsu8111_usbsw *chip;

static struct wake_lock JIGConnect_idle_wake;
static struct wake_lock JIGConnect_suspend_wake;

#ifdef CONFIG_VIDEO_MHL_V1
/*for MHL cable insertion*/
static int isMHLconnected=0;
#endif

static int isProbe=0;

// Static function prototype
static void tsu8111_Charger_Enable(enum power_supply_type type);
static void tsu8111_Charger_Disable(unsigned char type);
	
static int tsu8111_write_reg(struct i2c_client *client,        u8 reg, u8 data)
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

	   printk("[tsu8111] tsu8111_write_reg   reg[0x%2x] data[0x%2x]\n",buf[0],buf[1]);

       ret = i2c_transfer(client->adapter, msg, 1);
       if (ret != 1) {
               printk("\n [tsu8111] i2c Write Failed (ret=%d) \n", ret);
               return -1;
       }
      
       return ret;
}

static int tsu8111_read_reg(struct i2c_client *client, u8 reg, u8 *data)
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
		
    	printk("[tsu8111] tsu8111_read_reg reg[0x%2x] ", buf[0]);

       ret = i2c_transfer(client->adapter, msg, 2);
       if (ret != 2) {
               printk("\n [tsu8111] i2c Read Failed (ret=%d) \n", ret);
               return -1;
       }
       *data = buf[0];

       return 0;
}

static void tsu8111_Charger_Enable(enum power_supply_type type)
{
	u8 val1;
	int ret = 0;
	struct i2c_client *client = chip->client;

    tsu8111_read_reg(client, tsu8111_REG_CHG_CTRL3, &val1);
    val1 &= ~0x0F;

    switch(type)
	{
		case POWER_SUPPLY_TYPE_MAINS:
            val1 |= 0x07; //550mA
            ret = tsu8111_write_reg(client, tsu8111_REG_CHG_CTRL3, val1);
        	if (ret < 0)
		        pr_err("%s. Register(%d) write failed\n", __func__, tsu8111_REG_CHG_CTRL3);
			break;
		case POWER_SUPPLY_TYPE_USB:
            val1 |= 0x05; //450mA
            ret = tsu8111_write_reg(client, tsu8111_REG_CHG_CTRL3, val1);
            if (ret < 0)
                pr_err("%s. Register(%d) write failed\n", __func__, tsu8111_REG_CHG_CTRL3);
			break;
		default:
			printk("%s wrong charger type %d\n", __func__, type);
			break;
	}

    val1 = 0;
	tsu8111_read_reg(client, tsu8111_REG_CHG_CTRL1, &val1);
	val1 &= ~CH_DIS;
	pr_info("%s. Register(%d) = (0x%X)\n", __func__, tsu8111_REG_CHG_CTRL1, val1);
	ret = tsu8111_write_reg(client, tsu8111_REG_CHG_CTRL1, val1);
	if (ret < 0)
		pr_err("%s. Register(%d) write failed\n", __func__, tsu8111_REG_CHG_CTRL1);
}

static void tsu8111_Charger_Disable(unsigned char type)
{
	u8 val1;
	int ret = 0;
	struct i2c_client *client = chip->client;

	tsu8111_read_reg(client, tsu8111_REG_CHG_CTRL1, &val1);
	val1 |= CH_DIS;
	pr_info("%s. Register(%d) = (0x%X)\n", __func__, tsu8111_REG_CHG_CTRL1, val1);
	ret = tsu8111_write_reg(client, tsu8111_REG_CHG_CTRL1, val1);
	if (ret < 0)
		pr_err("%s. Register(%d) write failed\n", __func__, tsu8111_REG_CHG_CTRL1);
}

int tsu8111_read_charger_status(u8 *val)
{
	u8 val1;
	int ret = 0;
	struct i2c_client *client = chip->client;

	ret = tsu8111_read_reg(client, tsu8111_REG_CHG_STATUS, &val1);
	if(ret < 0)
		ret = -EIO;
	else {
		*val = val1;
	}

	return ret;
}
EXPORT_SYMBOL(tsu8111_read_charger_status);

static void tsu8111_read_adc_value(void)
{
	u8 adc=0;
    struct tsu8111_usbsw *usbsw = chip;
    struct i2c_client *client = usbsw->client;
	
	tsu8111_read_reg(client, tsu8111_REG_ADC, &adc);
	printk("[tsu8111] %s: adc is 0x%x\n",__func__,adc);
}

void tsu8111_set_switch(const char *buf)
{
       struct tsu8111_usbsw *usbsw = chip;
       struct i2c_client *client = usbsw->client;
       u8 value = 0;
       unsigned int path = 0;

       tsu8111_read_reg(client, tsu8111_REG_CTRL, &value);

       if (!strncmp(buf, "VAUDIO", 6)) {
	   	       if(usbsw->id == 0)
			   	path = VAUDIO_9485;
			   else
			   	path = VAUDIO;
               value &= ~MANUAL_SWITCH;
       } else if (!strncmp(buf, "UART", 4)) {
               path = UART;
               value &= ~MANUAL_SWITCH;
       } else if (!strncmp(buf, "AUDIO", 5)) {
               if(usbsw->id == 0)
			   	path = AUDIO_9485;
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
       tsu8111_write_reg(client, tsu8111_REG_MANSW1, path);
       tsu8111_write_reg(client, tsu8111_REG_CTRL, value);
}
EXPORT_SYMBOL_GPL(tsu8111_set_switch);

ssize_t tsu8111_get_switch(char *buf)
{
struct tsu8111_usbsw *usbsw = chip;
       struct i2c_client *client = usbsw->client;
       u8 value;

       tsu8111_read_reg(client, tsu8111_REG_MANSW1, &value);

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
EXPORT_SYMBOL_GPL(tsu8111_get_switch);

#ifdef CONFIG_VIDEO_MHL_V1
static void Disabletsu8111Interrupts(void)
{
       struct tsu8111_usbsw *usbsw = chip;
       struct i2c_client *client = usbsw->client;
        printk ("Disabletsu8111Interrupts-2\n");

     tsu8111_write_reg(client, tsu8111_REG_INT1_MASK, 0xFF);
     tsu8111_write_reg(client, tsu8111_REG_INT2_MASK, 0x1F);
	 
} // Disabletsu8111Interrupts()

static void Enabletsu8111Interrupts(void)
{
	struct tsu8111_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;
	u8 intr, intr2;

	printk ("Enabletsu8111Interrupts\n");

     /*clear interrupts*/
     tsu8111_read_reg(client, tsu8111_REG_INT1, &intr);
     tsu8111_read_reg(client, tsu8111_REG_INT2, &intr2);
	 
     tsu8111_write_reg(client, tsu8111_REG_INT1_MASK, 0x00);
     tsu8111_write_reg(client, tsu8111_REG_INT2_MASK, 0x00);

} //Enabletsu8111Interrupts()

void tsu8111_EnableIntrruptByMHL(bool _bDo)
{
	struct tsu8111_platform_data *pdata = chip->pdata;
	struct i2c_client *client = chip->client;
	char buf[16];

	if(true == _bDo)
	{
		tsu8111_write_reg(client,tsu8111_REG_CTRL, 0x1E);
		Enabletsu8111Interrupts();
	}
	else
	{
		Disabletsu8111Interrupts();
	}

	tsu8111_get_switch(buf);
	printk("[%s] fsa switch status = %s\n",__func__, buf);
}

/*MHL call this function to change VAUDIO path*/
void tsu8111_CheckAndHookAudioDock(void)
{
   struct tsu8111_platform_data *pdata = chip->pdata;
   struct i2c_client *client = chip->client;

   printk("[tsu8111] %s: FSA9485 VAUDIO\n",__func__);
   
   isMHLconnected = 0;
   isDeskdockconnected = 1;
   
   if (pdata->mhl_cb)
   	       pdata->mhl_cb(tsu8111_DETACHED);

   Enabletsu8111Interrupts();

   if(chip->id == 0)
	chip->mansw = VAUDIO_9485;
   else
	chip->mansw = VAUDIO;

   /*make ID change report*/
   tsu8111_write_reg(client,tsu8111_REG_CTRL, 0x16);
   
   if(pdata->deskdock_cb)
           pdata->deskdock_cb(tsu8111_ATTACHED);   

}
EXPORT_SYBMOL_GPL(tsu8111_CheckAndHookAudioDock);
#endif

static ssize_t tsu8111_show_status(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
       struct tsu8111_usbsw *usbsw = dev_get_drvdata(dev);
       struct i2c_client *client = usbsw->client;
       u8 devid, ctrl, adc, dev1, dev2, intr;
       u8 intmask1, intmask2, time1, time2, mansw1;

       tsu8111_read_reg(client, tsu8111_REG_DEVID, &devid);
       tsu8111_read_reg(client, tsu8111_REG_CTRL, &ctrl);
       tsu8111_read_reg(client, tsu8111_REG_ADC, &adc);
       tsu8111_read_reg(client, tsu8111_REG_INT1_MASK, &intmask1);
       tsu8111_read_reg(client, tsu8111_REG_INT2_MASK, &intmask2);
       tsu8111_read_reg(client, tsu8111_REG_DEV_T1, &dev1);
       tsu8111_read_reg(client, tsu8111_REG_DEV_T2, &dev2);
       tsu8111_read_reg(client, tsu8111_REG_TIMING1, &time1);
       tsu8111_read_reg(client, tsu8111_REG_TIMING2, &time2);
       tsu8111_read_reg(client, tsu8111_REG_MANSW1, &mansw1);

       tsu8111_read_reg(client, tsu8111_REG_INT1, &intr);
       intr &= 0xffff;

       return sprintf(buf, "Device ID(%02x), CTRL(%02x)\n"
                       "ADC(%02x), DEV_T1(%02x), DEV_T2(%02x)\n"
                       "INT(%04x), INTMASK(%02x, %02x)\n"
                       "TIMING(%02x, %02x), MANSW1(%02x)\n",
                       devid, ctrl, adc, dev1, dev2, intr,
                       intmask1, intmask2, time1, time2, mansw1);
}

static ssize_t tsu8111_show_manualsw(struct device *dev,
               struct device_attribute *attr, char *buf)
{
       return tsu8111_get_switch(buf);

}

static ssize_t tsu8111_set_manualsw(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count)
{
       tsu8111_set_switch(buf);
       return count;
}

static ssize_t tsu8111_set_syssleep(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count)
{
	struct tsu8111_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;
	u8 value = 0;

	if (!strncmp(buf, "1", 1))
	{
		wake_unlock(&JIGConnect_idle_wake);
		wake_unlock(&JIGConnect_suspend_wake);

		tsu8111_read_reg(client, tsu8111_REG_CTRL, &value);
		value &= ~MANUAL_SWITCH;
		tsu8111_write_reg(client, tsu8111_REG_CTRL, value);
	
		tsu8111_read_reg(client, tsu8111_REG_MANSW2, &value);
		value &= ~MANSW2_JIG;
		tsu8111_write_reg(client, tsu8111_REG_MANSW2, value);
	}
	else
	{
		tsu8111_read_reg(client, tsu8111_REG_CTRL, &value);
		value |= MANUAL_SWITCH;
		tsu8111_write_reg(client, tsu8111_REG_CTRL, value);
	}
	return count;
}
					

static DEVICE_ATTR(status, S_IRUGO, tsu8111_show_status, NULL);
static DEVICE_ATTR(switch, S_IRUGO | S_IWGRP,
               tsu8111_show_manualsw, tsu8111_set_manualsw);
static DEVICE_ATTR(syssleep, S_IWUSR, NULL, tsu8111_set_syssleep);

static struct attribute *tsu8111_attributes[] = {
       &dev_attr_status.attr,
       &dev_attr_switch.attr,
       &dev_attr_syssleep.attr,
       NULL
};

static const struct attribute_group tsu8111_group = {
       .attrs = tsu8111_attributes,
};

static irqreturn_t tsu8111_irq_handler(int irq, void *data)
{
       struct tsu8111_usbsw *usbsw = data;

	   printk("[tsu8111] tsu8111_irq_handler\n");	// TEST ONLY - will be removed

       if (!work_pending(&usbsw->work)) {
               disable_irq_nosync(irq);
               schedule_work(&usbsw->work);
       }

       return IRQ_HANDLED;
}

static void tsu8111_detect_dev(struct tsu8111_usbsw *usbsw, u8 intr)
{
       u8 val1, val2;// , ctrl,temp;
       struct i2c_client *client = usbsw->client;
	   printk("[tsu8111] tsu8111_detect_dev !!!!!\n");

       tsu8111_read_reg(client, tsu8111_REG_DEV_T1, &val1);
       tsu8111_read_reg(client, tsu8111_REG_DEV_T2, &val2);

	if((intr==0x01) &&(val1==0x00) && (val2==0x00) && (isProbe == 0))
	{
		printk("[tsu8111] (intr==0x01) &&(val1==0x00) && (val2==0x00) !!!!!\n");
		tsu8111_read_adc_value();

		msleep(50);

		tsu8111_read_reg(client, tsu8111_REG_DEV_T1, &val1);
		tsu8111_read_reg(client, tsu8111_REG_DEV_T2, &val2);
	}

	/* Attached */
	if (intr & (1 << 0)) 
	{
		if (val1 & tsu8111_DEV_T1_USB_MASK ||
				val2 & tsu8111_DEV_T2_USB_MASK) {
			printk("[tsu8111] tsu8111_USB ATTACHED*****\n");

#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				spa_external_event(SPA_CATEGORY_DEVICE, SPA_DEVICE_EVENT_USB_ATTACHED);
			}
#endif
		}

		if (val1 & tsu8111_DEV_T1_UART_MASK || val2 & tsu8111_DEV_T2_UART_MASK) {
			//if (pdata->uart_cb)
			//        pdata->uart_cb(tsu8111_ATTACHED);				   
		}

		if (val1 & tsu8111_DEV_T1_CHARGER_MASK) {
			printk("[tsu8111] Charger ATTACHED*****\n");
#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				spa_external_event(SPA_CATEGORY_DEVICE, SPA_DEVICE_EVENT_TA_ATTACHED);
			}
#endif
		}

		if (val2 & tsu8111_DEV_T2_JIG_MASK) {
			printk("[tsu8111] JIG ATTACHED*****\n"); 			   
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
		if(usbsw->dev1 & tsu8111_DEV_T1_USB_MASK ||
				usbsw->dev2 & tsu8111_DEV_T2_USB_MASK) {
				printk("[tsu8111] tsu8111_USB Detached*****\n");
#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				spa_external_event(SPA_CATEGORY_DEVICE, SPA_DEVICE_EVENT_USB_DETACHED);
			}
#endif
		}

		if (usbsw->dev1 & tsu8111_DEV_T1_UART_MASK ||
                       usbsw->dev2 & tsu8111_DEV_T2_UART_MASK) {
			//if (pdata->uart_cb)
			//     pdata->uart_cb(tsu8111_DETACHED);
		}

		if (usbsw->dev1 & tsu8111_DEV_T1_CHARGER_MASK) {
			printk("[tsu8111] Charger Detached*****\n");
#if defined(CONFIG_SPA)
			if(spa_external_event)
			{
				spa_external_event(SPA_CATEGORY_DEVICE, SPA_DEVICE_EVENT_TA_DETACHED);
			}
#endif
		}

		if (usbsw->dev2 & tsu8111_DEV_T2_JIG_MASK) {      
			printk("[tsu8111] JIG Detached*****\n");			   	
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

static void tsu8111_work_cb(struct work_struct *work)
{
	u8 intr, intr2, intr3;
	struct tsu8111_usbsw *usbsw = container_of(work, struct tsu8111_usbsw, work);
	struct i2c_client *client = usbsw->client;

	printk("[tsu8111] in work_cb !!!!!\n"); 	// TEST ONLY
	
   /* clear interrupt */
	msleep(200);
	
	tsu8111_read_reg(client, tsu8111_REG_INT1, &intr);
	tsu8111_read_reg(client, tsu8111_REG_INT2, &intr2);	
	tsu8111_read_reg(client, tsu8111_REG_CHG_INT, &intr3);
	printk("[tsu8111] %s: intr=0x%x, intr2 = 0x%X, chg_intr=0x%x \n",__func__,intr, intr2, intr3);

	intr &= 0xffff;

	enable_irq(client->irq); 

	if((intr== 0x00) && (intr3 == 0))
	{	
		printk("[tsu8111] (intr== 0x00) in work_cb !!!!!\n");
		tsu8111_read_adc_value();
        
		return;
	}

	/* device detection */
	tsu8111_detect_dev(usbsw, intr);

    //EOC
    if(intr3 & CH_DONE)
    {
#if defined(CONFIG_SPA)
		if(spa_external_event)
		{
			spa_external_event(SPA_CATEGORY_BATTERY, SPA_BATTERY_EVENT_CHARGE_FULL);
		}
#endif
    }    
}

static int tsu8111_irq_init(struct tsu8111_usbsw *usbsw)
{
	struct tsu8111_platform_data *pdata = usbsw->pdata;
	struct i2c_client *client = usbsw->client;
	int ret, irq = -1;

	INIT_WORK(&usbsw->work, tsu8111_work_cb);

	printk(KERN_ERR "[tsu8111] tsu8111_irq_init\n");

	ret = gpio_request(pdata->intb_gpio, "tsu8111 irq");
	if(ret)
	{
		dev_err(&client->dev,"tsu8111: Unable to get gpio %d\n", client->irq);
		goto gpio_out;
	}
	gpio_direction_input(pdata->intb_gpio);
    client->irq = gpio_to_irq(pdata->intb_gpio),

	ret = request_irq(client->irq, tsu8111_irq_handler,
	               (IRQF_NO_SUSPEND| IRQF_TRIGGER_FALLING),
	               "tsu8111 micro USB", usbsw);
	if (ret) {
		dev_err(&client->dev,
				   "tsu8111: Unable to get IRQ %d\n", irq);
		goto out;
	}

	return 0;
gpio_out:
	gpio_free(client->irq);
out:
    return ret;
}

static int __devinit tsu8111_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
	struct tsu8111_usbsw *usbsw;

	int ret = 0;
	
	u8 intr, intr2, intr_chg;
	u8 mansw1;
	unsigned int ctrl = CTRL_MASK;

	printk("[tsu8111] PROBE ......\n");
	   
	isProbe = 1;
	   
	//add for AT Command 
	wake_lock_init(&JIGConnect_idle_wake, WAKE_LOCK_IDLE, "jig_connect_idle_wake");
	wake_lock_init(&JIGConnect_suspend_wake, WAKE_LOCK_SUSPEND, "jig_connect_suspend_wake");

	usbsw = kzalloc(sizeof(struct tsu8111_usbsw), GFP_KERNEL);
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
	tsu8111_read_reg(client, tsu8111_REG_INT1, &intr);

	intr &= 0xffff;

	/* clear interrupt */
	tsu8111_read_reg(client, tsu8111_REG_INT2, &intr2);
	tsu8111_read_reg(client, tsu8111_REG_CHG_INT, &intr_chg);

	/* unmask interrupt (attach/detach only) */
	ret = tsu8111_write_reg(client, tsu8111_REG_INT1_MASK, 0x00);
	if (ret < 0)
		return ret;

	/*TI USB : not to get Connect Interrupt : no more double interrupt*/
	ret = tsu8111_write_reg(client, tsu8111_REG_INT1_MASK, 0x40);
	if (ret < 0)
		return ret;

	ret = tsu8111_write_reg(client, tsu8111_REG_INT2_MASK, 0x20);
	if (ret < 0)
		return ret;

	ret = tsu8111_write_reg(client, tsu8111_REG_CHG_INT_MASK, 0x2F);
	if (ret < 0)
		return ret;

   tsu8111_read_reg(client, tsu8111_REG_MANSW1, &mansw1);
   usbsw->mansw = mansw1;

	ctrl &= ~INT_MASK;			  /* Unmask Interrupt */
	if (usbsw->mansw)
		ctrl &= ~MANUAL_SWITCH; /* Manual Switching Mode */
   
	tsu8111_write_reg(client, tsu8111_REG_CTRL, ctrl);

    tsu8111_Charger_Disable(0);

#if defined(CONFIG_SPA)
	spa_chg_register_enable_charge(tsu8111_Charger_Enable);
	spa_chg_register_disable_charge(tsu8111_Charger_Disable);
#endif	

   ret = tsu8111_irq_init(usbsw);
   if (ret)
           goto tsu8111_probe_fail;

   ret = sysfs_create_group(&client->dev.kobj, &tsu8111_group);
   if (ret) {
           dev_err(&client->dev,
                           "[tsu8111] Creating tsu8111 attribute group failed");
           goto tsu8111_probe_fail2;
   }

	/* device detection */
	tsu8111_detect_dev(usbsw, 1);
	isProbe = 0;

	/*reset UIC*/
	tsu8111_write_reg(client, tsu8111_REG_CTRL, 0x1E);
    printk("[tsu8111] PROBE Done.\n");

    return 0;

tsu8111_probe_fail2:
   if (client->irq)
       free_irq(client->irq, NULL);
tsu8111_probe_fail:
	i2c_set_clientdata(client, NULL);
	kfree(usbsw);
	return ret;
}

static int __devexit tsu8111_remove(struct i2c_client *client)
{
       struct tsu8111_usbsw *usbsw = i2c_get_clientdata(client);
       if (client->irq)
               free_irq(client->irq, NULL);
       i2c_set_clientdata(client, NULL);

       sysfs_remove_group(&client->dev.kobj, &tsu8111_group);
       kfree(usbsw);
       return 0;
}

static int tsu8111_suspend(struct i2c_client *client,
		pm_message_t state)
{
	printk("[tsu8111] tsu8111_suspend  enable_irq_wake...\n");
    enable_irq_wake(client->irq);

    return 0;
}

static int tsu8111_resume(struct i2c_client *client)
{
	   printk("[tsu8111] tsu8111_resume  disable_irq_wake...\n");
	   disable_irq_wake(client->irq);
       return 0;
}

static const struct i2c_device_id tsu8111_id[] = {
       {"tsu8111", 0},
       {}
};

static struct i2c_driver tsu8111_i2c_driver = {
       .driver = {
               .name = "tsu8111",
       },
       .probe = tsu8111_probe,
       .remove = __devexit_p(tsu8111_remove),
       .suspend=tsu8111_suspend,
       .resume = tsu8111_resume,
       .id_table = tsu8111_id,
};

static int __init tsu8111_init(void)
{
       return i2c_add_driver(&tsu8111_i2c_driver);
}

module_init(tsu8111_init);

static void __exit tsu8111_exit(void)
{
       i2c_del_driver(&tsu8111_i2c_driver);
}

module_exit(tsu8111_exit);

MODULE_DESCRIPTION("tsu8111 USB Switch driver");
MODULE_LICENSE("GPL");

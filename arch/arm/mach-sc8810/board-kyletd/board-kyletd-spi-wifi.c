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

#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <mach/board.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>

//#define GET_WIFI_MAC_ADDR_FROM_NV_ITEM	1

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define PREALLOC_WLAN_NUMBER_OF_SECTIONS	4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define WLAN_SKB_BUF_NUM	16

static int bcm_wifi_cd = 0; /* WIFI virtual 'card detect' status */
static int bcm_wifi_power_state;
static int bcm_wifi_reset_state;
static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;
static struct regulator *wlan_regulator_18 = NULL;

int bcm_wifi_power(int on);
int bcm_wifi_reset(int on);
int bcm_wifi_set_carddetect(int on);


static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;

static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
static void *bcm_wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;
	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}

int __init bcm_init_wifi_mem(void)
{
	int i;

	for(i=0;( i < WLAN_SKB_BUF_NUM );i++) {
		if (i < (WLAN_SKB_BUF_NUM/2))
			wlan_static_skb[i] = dev_alloc_skb(4096);
		else
			wlan_static_skb[i] = dev_alloc_skb(8192);
    if (unlikely(wlan_static_skb[i] == NULL)) {//SE request-Low memory Check//
    	pr_err("%s Low memory\n",__func__); 
    	return -ENOMEM;  // fail case  
    } 

	}
	for(i=0;( i < PREALLOC_WLAN_NUMBER_OF_SECTIONS );i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
				GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL)
			return -ENOMEM;
	}
	wlan_static_scan_buf0 = kmalloc (65536, GFP_KERNEL);
	if(!wlan_static_scan_buf0)
		return -ENOMEM;
	wlan_static_scan_buf1 = kmalloc (65536, GFP_KERNEL);
	if(!wlan_static_scan_buf1)
		return -ENOMEM;
	return 0;
}

/* Control the BT_VDDIO and WLAN_VDDIO
Always power on  According to spec
*/
static int brcm_ldo_enable(void)
{
	int err;
	wlan_regulator_18 = regulator_get(NULL, REGU_NAME_WIFI);

	if (IS_ERR(wlan_regulator_18)) {
		pr_err("can't get wlan 1.8V regulator\n");
		return -1;
	}

	err = regulator_set_voltage(wlan_regulator_18,1800000,1800000);
	if (err){
		pr_err("can't set wlan to 1.8V.\n");
		return -1;
	}
	regulator_enable(wlan_regulator_18);
}

int bcm_wifi_power(int on)
{
	pr_info("%s:%d \n", __func__, on);

	if(on) {

		gpio_direction_output(GPIO_WIFI_SHUTDOWN, 0);
		mdelay(10);
		gpio_direction_output(GPIO_WIFI_SHUTDOWN, 1);
		mdelay(200);
	}
	else {
		gpio_direction_output(GPIO_WIFI_SHUTDOWN, 0);

	}
	bcm_wifi_power_state = on;
	return 0;
}

int bcm_wifi_reset(int on)
{
	pr_info("%s: do nothing\n", __func__);
	bcm_wifi_reset_state = on;
	return 0;
}


static int bcm_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static unsigned int bcm_wifi_status(struct device *dev)
{
	return bcm_wifi_cd;
}

int bcm_wifi_set_carddetect(int val)
{
	pr_info("%s: %d\n", __func__, val);
	bcm_wifi_cd = val;
	if (wifi_status_cb) {
		wifi_status_cb(val, wifi_status_cb_devid);
	} else
		pr_warning("%s: Nobody to notify\n", __func__);
	return 0;
}



#ifdef GET_WIFI_MAC_ADDR_FROM_NV_ITEM
static unsigned char bcm_mac_addr[IFHWADDRLEN] = { 0x11,0x22,0x33,0x44,0x55,0x66 };

static int bcm_wifi_get_mac_addr(unsigned char *buf)
{
	int rc = 0;
	unsigned int data1, data2;

	if (!buf){
		pr_err("%s, null parameter !!\n", __func__);
		return -EFAULT;
	}

	data2 = (1<<31);

	if(!rc) {
		bcm_mac_addr[5] = (unsigned char)((data2>>8)&0xff);
		bcm_mac_addr[4] = (unsigned char)(data2&0xff);
		bcm_mac_addr[3] = (unsigned char)((data1>>24)&0xff);
		bcm_mac_addr[2] = (unsigned char)((data1>>16)&0xff);
		bcm_mac_addr[1] = (unsigned char)((data1>>8)&0xff);
		bcm_mac_addr[0] = (unsigned char)(data1&0xff);

		memcpy(buf, bcm_mac_addr, IFHWADDRLEN);
		pr_info("wifi mac: %x:%x:%x:%x:%x:%x\n", bcm_mac_addr[0], bcm_mac_addr[1], bcm_mac_addr[2], bcm_mac_addr[3], bcm_mac_addr[4], bcm_mac_addr[5]);

		return 0;
	} else {
		pr_err("%s fail !!\n", __func__);
		return -EFAULT;
	}
}
#endif

static struct wifi_platform_data bcm_wifi_control = {
	.set_power      = bcm_wifi_power,
	.set_reset      = bcm_wifi_reset,
	.set_carddetect = bcm_wifi_set_carddetect,
	.mem_prealloc	= bcm_wifi_mem_prealloc,
#ifdef GET_WIFI_MAC_ADDR_FROM_NV_ITEM
	.get_mac_addr	= bcm_wifi_get_mac_addr,
#endif
};

static struct resource spi_resources[] = {
	[0] = {
		.start  = SPRD_SPI0_PHYS,
		.end    = SPRD_SPI0_PHYS + SZ_4K - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.name = "bcmdhd_wlan_irq",
                .flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE,
	},
};
static struct platform_device sprd_spi_controller_device = {
	.name   = "bcmdhd_wlan",
	.id     = 1,
	.dev    = {
		.platform_data = &bcm_wifi_control,
	},
	.resource	= spi_resources,
	.num_resources	= ARRAY_SIZE(spi_resources),
};

static int __init bcm_wifi_init(void)
{
	int ret;

	ret=bcm_init_wifi_mem();//SE request-Fail case//
	if (ret < 0) 
  	return -1;  // fail case  

	//hskang gps power off
	//brcm_ldo_enable();
/*
	gpio_request(GPIO_WIFI_IRQ, "oob_irq");
	gpio_direction_input(GPIO_WIFI_IRQ);
	spi_resources[1].start = gpio_to_irq(GPIO_WIFI_IRQ);
	spi_resources[1].end = gpio_to_irq(GPIO_WIFI_IRQ);
*/
	gpio_request(SPI0_WIFI_IRQ_GPIO, "oob_irq");
	gpio_direction_input(SPI0_WIFI_IRQ_GPIO);
	spi_resources[1].start = gpio_to_irq(SPI0_WIFI_IRQ_GPIO);
	spi_resources[1].end = gpio_to_irq(SPI0_WIFI_IRQ_GPIO);
	gpio_request(GPIO_WIFI_SHUTDOWN,"wifi_pwd");
	
	ret = platform_device_register(&sprd_spi_controller_device);

	return ret;
}

late_initcall(bcm_wifi_init);

MODULE_DESCRIPTION("Broadcomm wlan driver");
MODULE_LICENSE("GPL");


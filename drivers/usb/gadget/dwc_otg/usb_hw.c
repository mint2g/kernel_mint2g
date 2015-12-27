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
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <asm/irq.h>
#include <mach/globalregs.h>
#include <mach/board.h>
#include "usb_hw.h"

#define	 USB_LDO_NAME    "V_USB"
#define  USB_CLK_NAME    "clk_usb_ref"

static void usb_ldo_switch(int is_on)
{
	struct regulator *usb_regulator = NULL;

	if(usb_regulator == NULL){
		usb_regulator = regulator_get(NULL,USB_LDO_NAME);
	}
	if(usb_regulator){
		if(is_on){
			regulator_enable(usb_regulator);
		}else{
			regulator_disable(usb_regulator);
		}
		regulator_put(usb_regulator);
	}
}
static int usb_clock_enable(int is_on)
{
	struct clk *usb_clock = NULL;

	usb_clock = clk_get(NULL,USB_CLK_NAME);
	if (usb_clock) {
		if (is_on) {
			clk_enable(usb_clock);
		} else {
			clk_disable(usb_clock);
		}
	}
	return 0;
}
static void usb_enable_module(int en)
{
	if (en){
		usb_clock_enable(1);
		sprd_greg_clear_bits(REG_TYPE_GLOBAL,BIT(9),GR_CLK_GEN5);
	}else {
		usb_clock_enable(0);
		sprd_greg_set_bits(REG_TYPE_GLOBAL,BIT(9),GR_CLK_GEN5);
		sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,AHB_CTL0_USBD_EN,AHB_CTL0);
	}
}
void usb_phy_init(void)
{
    if (sprd_greg_read(REG_TYPE_AHB_GLOBAL,CHIP_ID) == CHIP_ID_8810S){
                /*SMIC chip id == 0x88100001*/
                sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,BIT(3)|BIT(2), USB_PHY_CTRL);
                sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,BIT(1) | BIT(0), USB_PHY_CTRL);
                sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,BIT(9), USB_PHY_CTRL);
                sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,BIT(16), USB_PHY_CTRL);
                sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,BIT(17), USB_PHY_CTRL);
                sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,BIT(13), USB_PHY_CTRL);
                sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, BIT(12), USB_PHY_CTRL);
                sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,BIT(15)|BIT(14), USB_PHY_CTRL);
                sprd_greg_write(REG_TYPE_AHB_GLOBAL,0x28,USB_SPR_REG);
        }else{
                /*
                 * config usb phy controller
                 */
                sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,BIT(8), USB_PHY_CTRL);
                sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,BIT(17), USB_PHY_CTRL);
                sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,BIT(16), USB_PHY_CTRL);
                sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,BIT(13)|BIT(12), USB_PHY_CTRL);
                sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,BIT(15)|BIT(14), USB_PHY_CTRL);
        }
}
static void usb_startup(void)
{
	usb_enable_module(1);
	mdelay(10);
	//usb_ldo_switch(0);
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,BIT(1)|BIT(2),AHB_CTL3);
	usb_ldo_switch(1);
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,BIT(6),AHB_CTL3);

	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,BIT(8)|BIT(14)|BIT(15)|BIT(17),AHB_CTL3);
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,BIT(12)|BIT(13)|BIT(16),AHB_CTL3);

	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,BIT(6)|BIT(7),AHB_SOFT_RST);
	mdelay(5);
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,BIT(6)|BIT(7),AHB_SOFT_RST);
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,AHB_CTL0_USBD_EN,AHB_CTL0);
}

void udc_enable(void)
{
	pr_info("%s \n", __func__);
	usb_startup();
}
void udc_disable(void)
{
        pr_info("%s \n", __func__);
        usb_enable_module(0);
        usb_ldo_switch(0);
}


int usb_alloc_vbus_irq(void)
{
	int irq;

	gpio_request(EIC_CHARGER_DETECT,"sprd_ogt");
	gpio_direction_input(EIC_CHARGER_DETECT);
	irq = gpio_to_irq(EIC_CHARGER_DETECT);
	set_irq_flags(irq, IRQF_VALID | IRQF_NOAUTOEN);

	return irq;
}

void usb_free_vbus_irq(int irq)
{
	gpio_free(EIC_CHARGER_DETECT);
}

int usb_get_vbus_irq(void)
{
	int value;

	value = gpio_to_irq(EIC_CHARGER_DETECT);

	return value;
}
int usb_get_vbus_state(void)
{
	int value;

	value = gpio_get_value(EIC_CHARGER_DETECT);
	return !!value;
}

void usb_set_vbus_irq_type(int irq, int irq_type)
{
	if (irq_type == VBUS_PLUG_IN)
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);
	else if (irq_type == VBUS_PLUG_OUT)
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_LOW);
	else {
		pr_warning("error type for usb vbus\n");
	}

	return;
}
EXPORT_SYMBOL(udc_disable);
EXPORT_SYMBOL(udc_enable);

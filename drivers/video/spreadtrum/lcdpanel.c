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
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/bitops.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include "sprdfb.h"

int lcd_id = 0;
EXPORT_SYMBOL(lcd_id);

static int32_t lcm_send_cmd(uint32_t cmd)
{
	/* busy wait for ahb fifo full sign's disappearance */
	while(lcdc_read(LCM_CTRL) & BIT(20));

	lcdc_write(cmd, LCM_CMD);

	return 0;
}

static int32_t lcm_send_cmd_data(uint32_t cmd, uint32_t data)
{
	/* busy wait for ahb fifo full sign's disappearance */
	while(lcdc_read(LCM_CTRL) & BIT(20));

	lcdc_write(cmd, LCM_CMD);

	/* busy wait for ahb fifo full sign's disappearance */
	while(lcdc_read(LCM_CTRL) & BIT(20));

	lcdc_write(data, LCM_DATA);

	return 0;
}

static int32_t lcm_send_data(uint32_t data)
{
	/* busy wait for ahb fifo full sign's disappearance */
	while(lcdc_read(LCM_CTRL) & BIT(20));

	lcdc_write(data, LCM_DATA);

	return 0;
}
int call_reports = 0;
EXPORT_SYMBOL(call_reports);
int lcd_frame_inversion_mode(void)
{
#if 0
#if defined(CONFIG_MACH_MINT)
	printk("lcd_frame_inversion_mode\n");
	call_reports = 1;
	/*Frame Inversion Mode*/
	lcm_send_cmd(0xB4); 
	lcm_send_data(0x06);
#endif
#endif
	return 0;
}
EXPORT_SYMBOL(lcd_frame_inversion_mode);
	
int lcd_line_inversion_mode(void)
{
#if 0
#if defined(CONFIG_MACH_MINT)
	printk("lcd_line_inversion_mode\n");
	call_reports = 0;
	msleep(1000);	
	/*line Inversion Mode*/
	lcm_send_cmd(0xB4); 
	lcm_send_data(0x02);
#endif
#endif
	return 0;
}
EXPORT_SYMBOL(lcd_line_inversion_mode);

static uint32_t lcm_read_data(void)
{
	/* busy wait for ahb fifo full sign's disappearance */
	while(lcdc_read(LCM_CTRL) & BIT(20));
	lcdc_write(1 << 24, LCM_DATA);
	udelay(50);
	return lcdc_read(LCM_RDDATA);
}

static struct ops_mcu lcm_mcu_ops = {
	.send_cmd = lcm_send_cmd,
	.send_cmd_data = lcm_send_cmd_data,
	.send_data = lcm_send_data,
	.read_data = lcm_read_data,
};

static int32_t mcu_reset(struct panel_spec *self)
{
	/* panel reset */
	lcdc_write(1, LCM_RSTN);
	msleep(50);	
	lcdc_write(0, LCM_RSTN);
	msleep(20);
	lcdc_write(1, LCM_RSTN);

	/* wait 10ms util the lcd is stable */
	msleep(50);
	return 0;
}

static void mcu_lcm_configure(struct panel_spec *panel, int cs)
{
	uint32_t reg_val = 0;

	if (cs == 0) {
		/* CS0 bus mode [BIT0]: 8080/6800 */
		switch (panel->info.mcu->bus_mode) {
		case LCD_BUS_8080:

			break;
		case LCD_BUS_6800:
			reg_val |= 1;
			break;
		default:
			break;
		}
		/* CS0 bus width [BIT1:0] */
		switch (panel->info.mcu->bus_width) {
		case 8:
			break;
		case 9:
			reg_val |= ((1 << 1) | (1 << 4));
			break;
		case 16:
			reg_val |= (2 << 1);
			break;
		case 18:
			reg_val |= ((3 << 1) | (1 << 4));
			break;
		case 24:
			reg_val |= ((4 << 1) | (2 << 4));
			break;
		default:
			break;

		}
		lcdc_write(reg_val, LCM_CTRL);
	} else {
		/* CS1 bus mode [BIT0]: 8080/6800 */
		switch (panel->info.mcu->bus_mode) {
		case LCD_BUS_8080:

			break;
		case LCD_BUS_6800:
			reg_val |= (1 << 8);
			break;
		default:
			break;
		}
		/* CS1 bus width [BIT1:0] */
		switch (panel->info.mcu->bus_width) {
		case 8:
			break;
		case 9:
			reg_val |= ((1 << 9) | (1 << 12));
			break;
		case 16:
			reg_val |= (2 << 9);
			break;
		case 18:
			reg_val |= ((3 << 9) | (1 << 12));
			break;
		case 24:
			reg_val |= ((4 << 9) | (2 << 12));
			break;
		default:
			break;

		}
		/* CS1	select */
		reg_val |= (1 << 16);
		lcdc_write(reg_val, LCM_CTRL);
	}
}

static uint32_t mcu_readid(struct panel_spec *self)
{
	uint32_t id = 0;

	/* default id reg is 0 */
	self->info.mcu->ops->send_cmd(0x0);

	if(self->info.mcu->bus_width == 8) {
		id = (self->info.mcu->ops->read_data()) & 0xff;
		id <<= 8;
		id |= (self->info.mcu->ops->read_data()) & 0xff;
	} else {
		id = self->info.mcu->ops->read_data();
	}

	return id;
}

static uint32_t mcu_calc_timing(struct timing_mcu *timing)
{
	uint32_t  ahb_clk;
	uint32_t  rcss, rlpw, rhpw, wcss, wlpw, whpw;

	struct clk * clk = clk_get(NULL,"clk_ahb");
	ahb_clk = clk_get_rate(clk) / 1000000;

	printk("[%s] ahb_clk: 0x%x\n", __FUNCTION__, ahb_clk);

	/********************************************************
	* we assume : t = ? ns, AHB = ? MHz   so
	*      1ns need cycle  :  AHB /1000
	*      tns need cycles :  t * AHB / 1000
	*
	********************************************************/
#define MAX_LCDC_TIMING_VALUE	15
#define LCDC_CYCLES(ns) (( (ns) * ahb_clk + 1000 - 1)/ 1000)

	/* ceiling*/
	rcss = LCDC_CYCLES(timing->rcss);
	if (rcss > MAX_LCDC_TIMING_VALUE) {
		rcss = MAX_LCDC_TIMING_VALUE ;
	}

	rlpw = LCDC_CYCLES(timing->rlpw);
	if (rlpw > MAX_LCDC_TIMING_VALUE) {
		rlpw = MAX_LCDC_TIMING_VALUE ;
	}

	rhpw = LCDC_CYCLES (timing->rhpw);
	if (rhpw > MAX_LCDC_TIMING_VALUE) {
		rhpw = MAX_LCDC_TIMING_VALUE ;
	}

	wcss = LCDC_CYCLES(timing->wcss);
	if (wcss > MAX_LCDC_TIMING_VALUE) {
		wcss = MAX_LCDC_TIMING_VALUE ;
	}

	wlpw = LCDC_CYCLES(timing->wlpw);
	if (wlpw > MAX_LCDC_TIMING_VALUE) {
		wlpw = MAX_LCDC_TIMING_VALUE ;
	}

	 /* lcdc will waste one cycle */
	whpw = LCDC_CYCLES (timing->whpw) - 1;
	if (whpw > MAX_LCDC_TIMING_VALUE) {
		whpw = MAX_LCDC_TIMING_VALUE ;
	}

	return (whpw | (wlpw << 4) | (wcss << 8)
			| (rhpw << 16) |(rlpw << 20) | (rcss << 24));
}

static int mcu_set_timing(struct sprdfb_device *dev, int32_t type)
{
	if (dev->csid == 0) {
		switch (type)
		{
		case LCD_REGISTER_TIMING:
			lcdc_write(dev->timing[LCD_REGISTER_TIMING],LCM_TIMING0);
			break;

		case LCD_GRAM_TIMING:
			lcdc_write(dev->timing[LCD_GRAM_TIMING],LCM_TIMING0);
			break;
		default:
			break;
		}
	} else if (dev->csid == 1) {
		switch (type)
		{
		case LCD_REGISTER_TIMING:
			lcdc_write(dev->timing[LCD_REGISTER_TIMING],LCM_TIMING1);
			break;

		case LCD_GRAM_TIMING:
			lcdc_write(dev->timing[LCD_GRAM_TIMING],LCM_TIMING1);
			break;
		default:
			break;
		}
	}
	return 0;
}

void lcdc_dithering_enable(void)
{
	lcdc_set_bits(BIT(4), LCDC_CTRL);
}
EXPORT_SYMBOL(lcdc_dithering_enable);

static int mcu_mount_panel(struct sprdfb_device *dev, struct panel_spec *panel)
{
	uint32_t bus_width;
	struct timing_mcu *timing;
	/* TODO: check whether the mode/res are supported */
	dev->panel = panel;

	if (panel->ops->panel_reset == NULL) {
		panel->ops->panel_reset = mcu_reset;
	}
	if (panel->ops->panel_readid == NULL) {
		panel->ops->panel_readid = mcu_readid;
	}

	dev->bpp = 32;
	bus_width = (uint32_t)((panel->info.mcu)->bus_width);

	if (bus_width != 24) {
		lcdc_dithering_enable();
	}

	timing = ((panel->info).mcu)->timing;
	dev->timing[LCD_REGISTER_TIMING] = mcu_calc_timing(timing);
	timing++;
	dev->timing[LCD_GRAM_TIMING] = mcu_calc_timing(timing);

	return 0;
}

static int mcu_init_panel(struct sprdfb_device *dev)
{
	mcu_lcm_configure(dev->panel, dev->csid);

	mcu_set_timing(dev, LCD_REGISTER_TIMING);

	#if 0
	dev->panel->ops->panel_init(dev->panel);
	#endif
	return 0;
}

static LIST_HEAD(panel_list);
static DEFINE_MUTEX(panel_mutex);

static uint32_t lcd_id_from_uboot = 0;
static int __init lcd_id_get(char *str)
{
	if ((str != NULL) && (str[0] == 'I') && (str[1] == 'D')) {
		sscanf(&str[2], "%x", &lcd_id_from_uboot);
	}
	printk(KERN_INFO "LCD Panel ID from uboot: 0x%x\n", lcd_id_from_uboot);
	return 1;
}
__setup("lcd_id=", lcd_id_get);

static struct panel_spec *adapt_panel_from_uboot(int cs)
{
	struct panel_cfg *cfg;

	if (lcd_id_from_uboot == 0) {
		return NULL;
	}

	list_for_each_entry(cfg, &panel_list, list) {
		if(lcd_id_from_uboot == cfg->lcd_id) {
			printk(KERN_INFO "LCD Panel 0x%x is attached!\n", cfg->lcd_id);
			return cfg->panel;
		}
	}
	printk(KERN_ERR "Failed to match LCD Panel from uboot!\n");

	return NULL;
}

static struct panel_spec *adapt_panel_from_readid(struct sprdfb_device *dev, int cs)
{
	struct panel_cfg *cfg;

	list_for_each_entry(cfg, &panel_list, list) {
		dev->mount_panel(dev, cfg->panel);
		dev->init_panel(dev);
		//dev->panel->ops->panel_init(dev->panel);
#if 1		
		lcd_id = dev->panel->ops->panel_readid(dev->panel);
		printk("\n\n adapt_panel_from_readid %x %x\n\n\n", lcd_id, cfg->lcd_id);
		if(lcd_id == cfg->lcd_id) {
			printk(KERN_INFO "LCD Panel 0x%x is attached!\n", cfg->lcd_id);
			return cfg->panel;
		}
#else
		return true;
#endif

	}
	printk(KERN_ERR "Failed to attach LCD Panel!\n");
	return NULL;
}

extern struct panel_spec lcd_dummy;
int sprd_probe_panel(struct sprdfb_device *dev, int cs)
{
	struct panel_spec *panel;
	int rval = 0;

	if (dev->mode == LCD_MODE_MCU) {
		dev->mount_panel = mcu_mount_panel;
		dev->init_panel = mcu_init_panel;
	} else {
		printk(KERN_ERR "RGB mode is not supported!\n");
		return rval;
	}

	panel = adapt_panel_from_uboot(cs);
	if (panel) {
		dev->mount_panel(dev, panel);
		dev->init_panel(dev);
	} else {
		/* can not be here in normal; we get correct device id from uboot */
		//mcu_reset(NULL); /* hardware reset , Need not ? */
		panel = adapt_panel_from_readid(dev, cs);
	}

#if 0
	if (panel) {
		printk(KERN_NOTICE "LCD Panel found!\n");
	} else {
		printk(KERN_NOTICE "LCD Panel NOT found! use dummy one!\n");
		panel = &lcd_dummy;
		rval = 1;
	}
#endif

	dev->ctrl->set_timing = mcu_set_timing;
	return rval;
}

int sprd_register_panel(struct panel_cfg *cfg)
{
	if (cfg->panel->mode == LCD_MODE_MCU) {
		cfg->panel->info.mcu->ops = &lcm_mcu_ops;
	}

	mutex_lock(&panel_mutex);

	list_add_tail(&cfg->list, &panel_list);

	mutex_unlock(&panel_mutex);

	return 0;
}


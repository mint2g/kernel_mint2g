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

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/delay.h>

#include "sprdfb.h"
#include "sprdfb_panel.h"
#include "sprdfb_dispc_reg.h"
#include "sprdfb_lcdc_reg.h"

static LIST_HEAD(panel_list_main);/* for main_lcd*/
static LIST_HEAD(panel_list_sub);/* for sub_lcd */
static DEFINE_MUTEX(panel_mutex);

static uint32_t lcd_id_from_uboot = 0;

extern struct panel_if_ctrl sprdfb_mcu_ctrl;
extern struct panel_if_ctrl sprdfb_rgb_ctrl;
extern struct panel_if_ctrl sprdfb_mipi_ctrl;

extern void sprdfb_panel_remove(struct sprdfb_device *dev);


static int __init lcd_id_get(char *str)
{
	if ((str != NULL) && (str[0] == 'I') && (str[1] == 'D')) {
		sscanf(&str[2], "%x", &lcd_id_from_uboot);
	}
	printk(KERN_INFO "sprdfb: [%s]LCD Panel ID from uboot: 0x%x\n", __FUNCTION__, lcd_id_from_uboot);
	return 1;
}
__setup("lcd_id=", lcd_id_get);

static int32_t panel_reset_dispc(struct panel_spec *self)
{
	dispc_write(1, DISPC_RSTN);
	mdelay(20);
	dispc_write(0, DISPC_RSTN);
	mdelay(20);
	dispc_write(1, DISPC_RSTN);

	/* wait 10ms util the lcd is stable */
	msleep(120);
	return 0;
}

static int32_t panel_reset_lcdc(struct panel_spec *self)
{
	lcdc_write(0, LCM_RSTN);
	mdelay(20);
	lcdc_write(1, LCM_RSTN);

	/* wait 10ms util the lcd is stable */
	msleep(20);
	return 0;
}

static void panel_reset(uint16_t dev_id, struct panel_spec *panel)
{
	pr_debug("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev_id);

	/* panel reset */
	if(SPRDFB_MAINLCD_ID == dev_id){
		panel_reset_dispc(panel);
	}else{
		panel_reset_lcdc(panel);
	}
}

static bool panel_check(struct panel_cfg *cfg)
{
	bool rval = true;

	if(NULL == cfg || NULL == cfg->panel){
		printk(KERN_ERR "sprdfb: [%s] :Invalid Param!\n", __FUNCTION__);
		return false;
	}

	//pr_debug("sprdfb: [%s], dev_id = %d, lcd_id = 0x%x, type = %d\n",__FUNCTION__, cfg->dev_id, cfg->lcd_id, cfg->panel->type);
	printk("sprdfb: [%s], dev_id = %d, lcd_id = 0x%x, type = %d\n",__FUNCTION__, cfg->dev_id, cfg->lcd_id, cfg->panel->type);		//zxdbg

	switch(cfg->panel->type){
	case SPRDFB_PANEL_TYPE_MCU:
		cfg->panel->if_ctrl = &sprdfb_mcu_ctrl;
		break;
	case SPRDFB_PANEL_TYPE_RGB:
		cfg->panel->if_ctrl = &sprdfb_rgb_ctrl;
		break;
	case SPRDFB_PANEL_TYPE_MIPI:
		cfg->panel->if_ctrl = &sprdfb_mipi_ctrl;
		break;
	default:
		printk("sprdfb: [%s]: erro panel type.(%d,%d, %d)",__FUNCTION__, cfg->dev_id, cfg->lcd_id, cfg->panel->type);
		cfg->panel->if_ctrl = NULL;
		break;
	};

	if(cfg->panel->if_ctrl->panel_if_check){
		rval = cfg->panel->if_ctrl->panel_if_check(cfg->panel);
	}
	return rval;
}

static int panel_mount(struct sprdfb_device *dev, struct panel_spec *panel)
{
	printk("sprdfb: [%s], dev_id = %d panel_spec addr 0x%x \n",__FUNCTION__, dev->dev_id, panel);

	/* TODO: check whether the mode/res are supported */
	dev->panel = panel;

	if(NULL == dev->panel->ops->panel_reset){
		if(SPRDFB_MAINLCD_ID == dev->dev_id){
			dev->panel->ops->panel_reset = panel_reset_dispc;
		}else{
			dev->panel->ops->panel_reset = panel_reset_lcdc;
		}
	}

	panel->if_ctrl->panel_if_mount(dev);

	printk("sprdfb: [%s], mount accomlished! \n",__FUNCTION__ );
	return 0;
}


int panel_init(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	pr_debug("sprdfb: [%s], dev_id= %d, type = %d\n",__FUNCTION__, dev->dev_id, dev->panel->type);

	if(!dev->panel->if_ctrl->panel_if_init(dev)){
		printk(KERN_ERR "sprdfb: [%s]: panel_if_init fail!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

int panel_ready(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	pr_debug("sprdfb: [%s], dev_id= %d, type = %d\n",__FUNCTION__, dev->dev_id, dev->panel->type);

	if(NULL != dev->panel->if_ctrl->panel_if_ready){
		dev->panel->if_ctrl->panel_if_ready(dev);
	}

	return 0;
}


static struct panel_spec *adapt_panel_from_uboot(uint16_t dev_id)
{
	struct panel_cfg *cfg;
	struct list_head *panel_list;

	//pr_debug("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev_id);
	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev_id);

	if (lcd_id_from_uboot == 0) {
		printk("sprdfb: [%s]: Not got lcd id from uboot\n", __FUNCTION__);
		return NULL;
	}

	if(SPRDFB_MAINLCD_ID == dev_id){
		panel_list = &panel_list_main;
	}else{
		panel_list = &panel_list_sub;
	}

	list_for_each_entry(cfg, panel_list, list) {
		if(lcd_id_from_uboot == cfg->lcd_id) {
			printk(KERN_INFO "sprdfb: [%s]: LCD Panel 0x%x is attached!\n", __FUNCTION__,cfg->lcd_id);
			return cfg->panel;
		}
	}
	printk(KERN_ERR "sprdfb: [%s]: Failed to match LCD Panel from uboot!\n", __FUNCTION__);

	return NULL;
}

static struct panel_spec *adapt_panel_from_readid(struct sprdfb_device *dev)
{
	struct panel_cfg *cfg;
	struct list_head *panel_list;
	int id;

	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	if(SPRDFB_MAINLCD_ID == dev->dev_id){
		panel_list = &panel_list_main;
	}else{
		panel_list = &panel_list_sub;
	}

	list_for_each_entry(cfg, panel_list, list) {
		printk("sprdfb: [%s]: try panel 0x%x\n", __FUNCTION__, cfg->lcd_id);
		panel_mount(dev, cfg->panel);
		panel_init(dev);
		dev->panel->ops->panel_reset(cfg->panel);
		id = dev->panel->ops->panel_readid(dev->panel);
		if(id == cfg->lcd_id) {
			pr_debug(KERN_INFO "sprdfb: [%s]: LCD Panel 0x%x is attached!\n", __FUNCTION__, cfg->lcd_id);
			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
			return cfg->panel;
		}
		sprdfb_panel_remove(dev);
	}
	printk(KERN_ERR "sprdfb:  [%s]: failed to attach LCD Panel!\n", __FUNCTION__);
	return NULL;
}


bool sprdfb_panel_get(struct sprdfb_device *dev)
{
	struct panel_spec *panel = NULL;

	if(NULL == dev){
		printk("sprdfb: [%s]: Invalid param\n", __FUNCTION__);
	}

	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	panel = adapt_panel_from_uboot(dev->dev_id);
	if (panel) {
		dev->panel_ready = true;
		panel_mount(dev, panel);
		panel_init(dev);
		printk("sprdfb: [%s] got panel\n", __FUNCTION__);
		return true;
	}

	printk("sprdfb: [%s] can not got panel\n", __FUNCTION__);

	return false;
}


bool sprdfb_panel_probe(struct sprdfb_device *dev)
{
	struct panel_spec *panel;

	if(NULL == dev){
		printk("sprdfb: [%s]: Invalid param\n", __FUNCTION__);
	}

	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	/* can not be here in normal; we should get correct device id from uboot */
	panel = adapt_panel_from_readid(dev);

	if (panel) {
		printk("sprdfb: [%s] got panel\n", __FUNCTION__);
		return true;
	}

	printk("sprdfb: [%s] can not got panel\n", __FUNCTION__);

	return false;
}

void sprdfb_panel_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	/*Jessica TODO: */
	if(NULL != self->ops->panel_invalidate_rect){
		self->ops->panel_invalidate_rect(self, left, top, right, bottom);
	}
	/*Jessica TODO: Need set timing to GRAM timing*/
}

void sprdfb_panel_invalidate(struct panel_spec *self)
{
	/*Jessica TODO:*/
	if(NULL != self->ops->panel_invalidate){
		self->ops->panel_invalidate(self);
	}
	/*Jessica TODO: Need set timing to GRAM timing*/
}

void sprdfb_panel_before_refresh(struct sprdfb_device *dev)
{
	if(NULL != dev->panel->if_ctrl->panel_if_before_refresh){
		dev->panel->if_ctrl->panel_if_before_refresh(dev);
	}
}

void sprdfb_panel_after_refresh(struct sprdfb_device *dev)
{
	if(NULL != dev->panel->if_ctrl->panel_if_after_refresh){
		dev->panel->if_ctrl->panel_if_after_refresh(dev);
	}
}

void sprdfb_panel_suspend(struct sprdfb_device *dev)
{
	if(NULL == dev->panel){
		return;
	}

	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);
	/*Jessica TODO: Need do some I2c, SPI, mipi sleep here*/
	/* let lcdc sleep in */
	if (dev->panel->ops->panel_enter_sleep != NULL) {
		dev->panel->ops->panel_enter_sleep(dev->panel,1);
	}

	msleep(100);

	if(NULL != dev->panel->if_ctrl->panel_if_suspend){
		dev->panel->if_ctrl->panel_if_suspend(dev);
	}
}

void sprdfb_panel_resume(struct sprdfb_device *dev, bool from_deep_sleep)
{
	if(NULL == dev->panel){
		return;
	}

	printk(KERN_INFO "sprdfb:[%s], dev->enable= %d, from_deep_sleep = %d\n",__FUNCTION__, dev->enable, from_deep_sleep);
#if 0
	/*Jessica TODO: resume i2c, spi, mipi*/
	if(NULL != dev->panel->if_ctrl->panel_if_resume){
		dev->panel->if_ctrl->panel_if_resume(dev);
	}
#endif

	if(from_deep_sleep){
		panel_init(dev);
		dev->panel->ops->panel_reset(dev->panel);
		dev->panel->ops->panel_init(dev->panel);
		panel_ready(dev);
	}else{
		/*Jessica TODO: resume i2c, spi, mipi*/
		if(NULL != dev->panel->if_ctrl->panel_if_resume){
			dev->panel->if_ctrl->panel_if_resume(dev);
		}

		/* let lcd sleep out */
		if(NULL != dev->panel->ops->panel_enter_sleep){
			dev->panel->ops->panel_enter_sleep(dev->panel,0);
		}
	}

}

void sprdfb_panel_remove(struct sprdfb_device *dev)
{
	if(NULL == dev->panel){
		return;
	}

	/*Jessica TODO:close panel, i2c, spi, mipi*/
	if(NULL != dev->panel->if_ctrl->panel_if_uninit){
		dev->panel->if_ctrl->panel_if_uninit(dev);
	}
	dev->panel = NULL;
}


int sprdfb_panel_register(struct panel_cfg *cfg)
{
	//pr_debug("sprdfb: [%s], panel id = %d\n",__FUNCTION__, cfg->dev_id);
	printk("sprdfb: [%s], panel id = %d\n",__FUNCTION__, cfg->lcd_id);		//zxdbg

	if(!panel_check(cfg)){
		printk("sprdfb: [%s]: panel check fail!id = %d\n",__FUNCTION__,  cfg->dev_id);
		return -1;
	}

	mutex_lock(&panel_mutex);

	if (cfg->dev_id == SPRDFB_MAINLCD_ID) {
		list_add_tail(&cfg->list, &panel_list_main);
	} else if (cfg->dev_id == SPRDFB_SUBLCD_ID) {
		list_add_tail(&cfg->list, &panel_list_sub);
	} else {
		list_add_tail(&cfg->list, &panel_list_main);
		list_add_tail(&cfg->list, &panel_list_sub);
	}

	mutex_unlock(&panel_mutex);

	return 0;
}

 

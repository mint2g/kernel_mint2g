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

#ifndef _SPRDFB_H_
#define _SPRDFB_H_

#include <linux/earlysuspend.h>


enum{
	SPRDFB_PANEL_IF_DBI = 0,
	SPRDFB_PANEL_IF_DPI,
	SPRDFB_PANEL_IF_EDPI,
	SPRDFB_PANEL_IF_LIMIT
};


enum{
	MCU_LCD_REGISTER_TIMING = 0,
	MCU_LCD_GRAM_TIMING,
	MCU_LCD_TIMING_KIND_MAX
};

enum{
	RGB_LCD_H_TIMING = 0,
	RGB_LCD_V_TIMING,
	RGB_LCD_TIMING_KIND_MAX
};


#ifdef  CONFIG_FB_LCD_OVERLAY_SUPPORT
#define SPRD_LAYER_IMG (0x01)   /*support YUV & RGB*/
#define SPRD_LAYER_OSD (0x02) /*support RGB only*/

enum {
	SPRD_DATA_TYPE_YUV422 = 0,
	SPRD_DATA_TYPE_YUV420,
	SPRD_DATA_TYPE_YUV400,
	SPRD_DATA_TYPE_RGB888,
	SPRD_DATA_TYPE_RGB666,
	SPRD_DATA_TYPE_RGB565,
	SPRD_DATA_TYPE_RGB555,
	SPRD_DATA_TYPE_LIMIT
};

enum{
	SPRD_IMG_DATA_ENDIAN_B0B1B2B3 = 0,
	SPRD_IMG_DATA_ENDIAN_B3B2B1B0,
	SPRD_IMG_DATA_ENDIAN_B2B3B1B0,
	SPRD_IMG_DATA_ENDIAN_LIMIT
};

enum{
	SPRD_OVERLAY_STATUS_OFF = 0,
	SPRD_OVERLAY_STATUS_ON,
	SPRD_OVERLAY_STATUS_STARTED,
	SPRD_OVERLAY_STATUS_MAX
};

enum{
	SPRD_OVERLAY_DISPLAY_ASYNC = 0,
	SPRD_OVERLAY_DISPLAY_SYNC,
	SPRD_OVERLAY_DISPLAY_MAX
};

typedef struct overlay_rect {
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;
}overlay_rect;

typedef struct overlay_info{
	int layer_index;
	int data_type;
	int y_endian;
	int uv_endian;
	bool rb_switch;
	overlay_rect rect;
	unsigned char *buffer;
}overlay_info;

typedef struct overlay_display{
	int layer_index;
	overlay_rect rect;
	int display_mode;
}overlay_display;
#endif


struct sprdfb_device {
	struct fb_info	*fb;

	uint16_t		enable;
	uint16_t	 	dev_id; /*main_lcd, sub_lcd*/

	uint32_t	 	bpp;  /*input bit per pixel*/

	uint16_t		panel_ready; /*panel has been inited by uboot*/
	uint16_t		panel_if_type; /*panel IF*/

	union{
		uint32_t	mcu_timing[MCU_LCD_TIMING_KIND_MAX];
		uint32_t	rgb_timing[RGB_LCD_TIMING_KIND_MAX];
	}panel_timing;

	struct panel_spec	*panel;
	struct display_ctrl	*ctrl;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif
};

struct display_ctrl {
	const char	*name;

	int32_t	(*early_init)	  (struct sprdfb_device *dev);
	int32_t	(*init)		  (struct sprdfb_device *dev);
	int32_t	(*uninit)		  (struct sprdfb_device *dev);

	int32_t 	(*refresh)	  (struct sprdfb_device *dev);

	int32_t	(*suspend)	  (struct sprdfb_device *dev);
	int32_t 	(*resume)	  (struct sprdfb_device *dev);

#ifdef  CONFIG_FB_LCD_OVERLAY_SUPPORT
	int32_t 	(*enable_overlay) 	(struct sprdfb_device *dev, struct overlay_info* info, int enable);
	int32_t	(*display_overlay)	(struct sprdfb_device *dev, struct overlay_display* setting);
#endif
};


#endif

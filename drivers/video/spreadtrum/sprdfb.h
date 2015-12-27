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

#include <asm/io.h>
#include <mach/hardware.h>
#include <linux/earlysuspend.h>
#include <linux/semaphore.h>
#include "lcdpanel.h"

/* LCDC regs offset */
#define LCDC_CTRL			(0x0000)
#define LCDC_DISP_SIZE			(0x0004)
#define LCDC_LCM_START			(0x0008)
#define LCDC_LCM_SIZE			(0x000c)
#define LCDC_BG_COLOR			(0x0010)
#define LCDC_FIFO_STATUS		(0x0014)

#define LCDC_IMG_CTRL			(0x0020)
#define LCDC_IMG_Y_BASE_ADDR		(0x0024)
#define LCDC_IMG_UV_BASE_ADDR		(0x0028)
#define LCDC_IMG_SIZE_XY		(0x002c)
#define LCDC_IMG_PITCH			(0x0030)
#define LCDC_IMG_DISP_XY		(0x0034)

#define LCDC_OSD1_CTRL			(0x0050)
#define LCDC_OSD2_CTRL			(0x0080)
#define LCDC_OSD3_CTRL			(0x00b0)
#define LCDC_OSD4_CTRL			(0x00e0)
#define LCDC_OSD5_CTRL			(0x0110)

#define LCDC_OSD1_BASE_ADDR		(0x0054)
#define LCDC_OSD1_ALPHA_BASE_ADDR	(0x0058)
#define LCDC_OSD1_SIZE_XY		(0x005c)
#define LCDC_OSD1_PITCH			(0x0060)
#define LCDC_OSD1_DISP_XY		(0x0064)
#define LCDC_OSD1_ALPHA			(0x0068)
#define LCDC_OSD1_GREY_RGB		(0x006c)
#define LCDC_OSD1_CK			(0x0070)

#define LCDC_OSD2_BASE_ADDR		(0x0084)
#define LCDC_OSD2_SIZE_XY		(0x8c)
#define LCDC_OSD2_PITCH			(0x0090)
#define LCDC_OSD2_DISP_XY		(0x0094)
#define LCDC_OSD2_ALPHA			(0x0098)
#define LCDC_OSD2_GREY_RGB		(0x009c)
#define LCDC_OSD2_CK			(0x00a0)

#define LCDC_Y2R_CTRL			(0x0160)
#define LCDC_Y2R_CONTRAST 		(0x0164)
#define LCDC_Y2R_SATURATION		(0x0168)
#define LCDC_Y2R_BRIGHTNESS		(0x016c)

#define LCDC_IRQ_EN			(0x0170)
#define LCDC_IRQ_CLR			(0x0174)
#define LCDC_IRQ_STATUS			(0x0178)
#define LCDC_IRQ_RAW			(0x017c)

#define LCM_CTRL			(0x0180)
#define LCM_TIMING0			(0x0184)
#define LCM_TIMING1			(0x0188)
#define LCM_RDDATA			(0x018c)
#define LCM_RSTN			(0x0190)
#define LCM_CMD				(0x01A0)
#define LCM_DATA			(0x01A4)

static inline uint32_t lcdc_read(uint32_t reg)
{
	return __raw_readl(SPRD_LCDC_BASE + reg);
}

static inline void lcdc_write(uint32_t value, uint32_t reg)
{
	__raw_writel(value, (SPRD_LCDC_BASE + reg));
}

static inline void lcdc_set_bits(uint32_t bits, uint32_t reg)
{
	lcdc_write(lcdc_read(reg) | bits, reg);
}

static inline void lcdc_clear_bits(uint32_t bits, uint32_t reg)
{
	lcdc_write(lcdc_read(reg) & ~bits, reg);
}


struct panel_ctrl;

#define SPRDFB_PANEL_NUM  1
struct sprdfb_device {
	struct fb_info	 *fb;

	uint32_t         enable;
	uint32_t	 csid;
	uint32_t	 mode;

	struct panel_spec *panel;
	uint32_t	 bpp;  // bit per pixel


	uint32_t	 vsync_waiter;
	uint32_t	 pending_addr;

	int32_t		 (*mount_panel)(struct sprdfb_device *dev, struct panel_spec *panel);
	int32_t		 (*init_panel)(struct sprdfb_device *dev);

	uint32_t	 timing[LCD_TIMING_KIND_MAX];

	struct panel_ctrl  *ctrl;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif
	struct semaphore work_proceedure_lock;
    	struct work_struct  work;
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


struct panel_ctrl {
	const char	*name;
	void		*data;

	/* for read device id ; only can do once */
	int32_t		(*early_init)	  (void);
	/* for more hw init; may be do more if need */
	int32_t		(*init)		  (struct sprdfb_device *dev);
	/* clean up */
	int32_t 	(*cleanup)	  (struct sprdfb_device *dev);

	int32_t  	(*enable_plane)	  (struct sprdfb_device *dev, int enable);

	int32_t 	(*set_timing)	  (struct sprdfb_device *dev, int32_t type);

	/* setup parameter, such as framebuffer addr, lcd width, andso on */
	int32_t 	(*refresh)	  (struct sprdfb_device *dev);

#ifdef  CONFIG_FB_LCD_OVERLAY_SUPPORT
	int32_t 	(*enable_overlay) 	(struct sprdfb_device *dev, struct overlay_info* info, int enable);
	int32_t	(*display_overlay)	(struct sprdfb_device *dev, struct overlay_display* setting);
#endif

	#if 0
	/* io ctl */
	int32_t 	(*ioctl)          (struct sprdfb_device *dev,
	 					uint32_t cmd, void *args);
        #endif

	int32_t 	(*sync)		  (struct sprdfb_device *dev);

	int32_t		(*suspend)	  (struct sprdfb_device *dev);
	int32_t 	(*resume)	  (struct sprdfb_device *dev);

	#if 0
	int32_t 	(*set_rotate)	  (int32_t angle);
	int32_t 	(*set_scale)	  (int32_t orig_width, int32_t orig_height,
					   int32_t out_width, int32_t out_height);
	int32_t 	(*mmap)		  (struct fb_info *info, struct vm_area_struct *vma);
	#endif

	int32_t		(*uninit)		  (struct sprdfb_device *dev);

	uint32_t        reserved[5];
};



#endif

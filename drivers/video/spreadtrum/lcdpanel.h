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
 
#ifndef _LCDPANEL_H_
#define _LCDPANEL_H_

/* LCD mode */
#define LCD_MODE_MCU			0
#define LCD_MODE_RGB			1
#define LCD_MODE_DSI			2

/* bus mode */
#define LCD_BUS_8080			0
#define LCD_BUS_6800			1
#define LCD_BUS_SPI			2

/* lcd directions */
#define LCD_DIRECT_NORMAL		0
#define LCD_DIRECT_ROT_90		1
#define LCD_DIRECT_ROT_180		2
#define LCD_DIRECT_ROT_270		3
#define LCD_DIRECT_MIR_H		4
#define LCD_DIRECT_MIR_V		5
#define LCD_DIRECT_MIR_HV		6

#define LCD_DelayMS      msleep


enum LCD_TIMING {
	LCD_REGISTER_TIMING = 0,
	LCD_GRAM_TIMING,
	LCD_TIMING_KIND_MAX,
};

struct panel_spec;

/* LCD operations */
struct panel_operations {
	int32_t (*panel_init)(struct panel_spec *self);
	int32_t (*panel_close)(struct panel_spec *self);
	int32_t (*panel_reset)(struct panel_spec *self);
	int32_t (*panel_enter_sleep)(struct panel_spec *self, uint8_t is_sleep);
	int32_t (*panel_set_contrast)(struct panel_spec *self, uint16_t contrast);
	int32_t (*panel_set_brightness)(struct panel_spec *self,
				uint16_t brightness);
	int32_t (*panel_set_window)(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom);
	int32_t (*panel_invalidate)(struct panel_spec *self);
	int32_t (*panel_invalidate_rect)(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom);
	int32_t (*panel_rotate_invalidate_rect)(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom,
				uint16_t angle);
	int32_t (*panel_set_direction)(struct panel_spec *self, uint16_t direction);
	uint32_t (*panel_readid)(struct panel_spec *self);
};

/* RGB LCD specific properties */
struct timing_rgb {
	uint16_t bfw;
	uint16_t efw;
	uint16_t vsw;
	uint16_t blw;
	uint16_t elw;
	uint16_t hsw;
};

struct ops_rgb {
	int32_t (*send_cmd)(uint32_t cmd);
	int32_t (*send_cmd_data)(uint32_t cmd, uint32_t data);
};

struct info_rgb {
	/* under construction... */
	struct timing_rgb timing;
	struct ops_rgb *ops;
};

/* MCU LCD specific properties */
struct timing_mcu {
	uint16_t rcss;
	uint16_t rlpw;
	uint16_t rhpw;
	uint16_t wcss;
	uint16_t wlpw;
	uint16_t whpw;
};

typedef int32_t (*send_cmd_t)(uint32_t data);
typedef int32_t (*send_data_t)(uint32_t data);
typedef int32_t (*send_cmd_data_t)(uint32_t cmd, uint32_t data);
typedef uint32_t (*read_data_t)(void);

struct ops_mcu {
	int32_t (*send_cmd)(uint32_t cmd);
	int32_t (*send_cmd_data)(uint32_t cmd, uint32_t data);
	int32_t (*send_data)(uint32_t data);
	uint32_t (*read_data)(void);
};

struct info_mcu {
	uint16_t bus_mode;
	uint16_t bus_width;
	struct timing_mcu *timing;
	struct ops_mcu *ops;
};

/* LCD abstraction */
struct panel_spec {
	uint16_t width;
	uint16_t height;
	uint16_t fps;
	uint16_t mode;
	uint16_t direction;
	union {
		struct info_rgb *rgb;
		struct info_mcu *mcu;
	} info;
	struct panel_operations *ops;
};

struct panel_cfg {
	struct list_head list;
	uint32_t lcd_cs;
	uint32_t lcd_id;
	const char *lcd_name;
	struct panel_spec *panel;
};

int sprd_register_panel(struct panel_cfg *cfg);

#endif

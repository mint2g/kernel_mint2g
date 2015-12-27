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

#ifndef _SC8810_REG_KEYPAD_H_
#define _SC8810_REG_KEYPAD_H_

#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/adi.h>

#define KPD_REG_BASE                (SPRD_KPD_BASE)

#define KPD_CTRL                	(KPD_REG_BASE + 0x0000)
#define KPD_INT_EN              	(KPD_REG_BASE + 0x0004)
#define KPD_INT_RAW_STATUS          (KPD_REG_BASE + 0x0008)
#define KPD_INT_MASK_STATUS     	(KPD_REG_BASE + 0x000C)

#define KPD_INT_CLR             	(KPD_REG_BASE + 0x0010)
#define KPD_POLARITY            	(KPD_REG_BASE + 0x0018)
#define KPD_DEBOUNCE_CNT        	(KPD_REG_BASE + 0x001C)
#define KPD_LONG_KEY_CNT        	(KPD_REG_BASE + 0x0020)

#define KPD_SLEEP_CNT           	(KPD_REG_BASE + 0x0024)
#define KPD_CLK_DIV_CNT         	(KPD_REG_BASE + 0x0028)
#define KPD_KEY_STATUS          	(KPD_REG_BASE + 0x002C)
#define KPD_SLEEP_STATUS        	(KPD_REG_BASE + 0x0030)

#define CUSTOM_KEYPAD_ROWS          3
#define CUSTOM_KEYPAD_COLS          3

#define KPD_ROW_MIN_NUM             4	/* keypad row min value */
#define KPD_COL_MIN_NUM             3	/* keypad col min value */
#define KPD_ROW_MAX_NUM             8	/* keypad row max value */
#define KPD_COL_MAX_NUM             8	/* keypad col max value */

#define KPDCTL_ROW                  (0x0f << 16)	/* enable rows4 - 7 */
#define KPDCTL_COL                  (0x1f << 20)	/* enable cols3 - 4 */
#define TB_KPD_GPIO_KEY             (0x0FF00)

#define KPD_INT_ALL                 (0xfff)
#define KPD_INT_DOWNUP              (0x0ff)

#define KPD_PRESS_INT0              (1 << 0)
#define KPD_PRESS_INT1              (1 << 1)
#define KPD_PRESS_INT2              (1 << 2)
#define KPD_PRESS_INT3              (1 << 3)

#define KPD_RELEASE_INT0            (1 << 4)
#define KPD_RELEASE_INT1            (1 << 5)
#define KPD_RELEASE_INT2            (1 << 6)
#define KPD_RELEASE_INT3            (1 << 7)

#define KPD_LONG_KEY_INT0           (1 << 8)
#define KPD_LONG_KEY_INT1           (1 << 9)
#define KPD_LONG_KEY_INT2           (1 << 10)
#define KPD_LONG_KEY_INT3           (1 << 11)

#define KPD0_COL_CNT                0x7
#define KPD0_ROW_CNT                0x70
#define KPD1_COL_CNT                0x700
#define KPD1_ROW_CNT                0x7000
#define KPD2_COL_CNT                0x70000
#define KPD2_ROW_CNT                0x700000
#define KPD3_COL_CNT                0x7000000
#define KPD3_ROW_CNT                0x70000000

#define KPDPOLARITY_ROW             (0x00FF)
#define KPDPOLARITY_COL             (0xFF00)

#define CFG_ROW_POLARITY            (0x00FF & KPDPOLARITY_ROW)
#define CFG_COL_POLARITY            (0xFF00 & KPDPOLARITY_COL)

#define ANA_GPI_PB                  EIC_KEY_POWER
#define	KEYCODE(x)                  ((x) | 0x08)

struct sprd_keypad_t {
	struct input_dev *input;
	int irq;
	unsigned short *keycode;
	unsigned int keyup_test_jiffies;
};

struct sprd_keypad_platform_data {
	int rows;		/* keypad rows */
	int cols;		/* keypad cols */
	unsigned short repeat;
	unsigned int debounce_time;	/* in ns */
	unsigned int coldrive_time;	/* in ns */
	unsigned int keyup_test_interval;	/* in ms */
};

unsigned short sprd_keymap[] = {
	KEYCODE(0x00),
	KEYCODE(0x01),
	KEYCODE(0x10),
	KEYCODE(0x12),
	KEYCODE(0x20),
	KEY_POWER,
	KEY_VOLUMEDOWN,
	KEY_VOLUMEUP,
	KEY_HOME,
};

struct sprd_keypad_platform_data sprd_keypad_data = {
	.rows = CUSTOM_KEYPAD_ROWS,
	.cols = CUSTOM_KEYPAD_COLS,
	.repeat = 0,
	.debounce_time = 5000,
	.coldrive_time = 1000,
	.keyup_test_interval = 50,
};

#endif

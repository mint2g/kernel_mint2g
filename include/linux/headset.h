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

#ifndef __HEADSET_H__
#define __HEADSET_H__
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/platform_device.h>
enum {
	BIT_HEADSET_OUT = 0,
	BIT_HEADSET_MIC = (1 << 0),
	BIT_HEADSET_NO_MIC = (1 << 1),
};

enum {
	HEADSET_BUTTON_DOWN_INVALID = -1,
	HEADSET_BUTTON_DOWN_SHORT,
	HEADSET_BUTTON_DOWN_LONG,
};

struct _headset_gpio {
	int active_low;
	int gpio;
	int irq;
	unsigned int irq_type_active;
	unsigned int irq_type_inactive;
	int debounce;
	int debounce_sw;
	int holded;
	int active;
	int irq_enabled;
	const char *desc;
	struct _headset *parent;
	unsigned int timeout_ms;
	struct hrtimer timer;
	enum hrtimer_restart (*callback)(int active, struct _headset_gpio *hgp);
};

struct _headset_keycap {
	unsigned int type;
	unsigned int key;
};

struct _headset_button {
	struct _headset_keycap cap[15];
	unsigned int (*headset_get_button_code_board_method)(int v);
	unsigned int (*headset_map_code2push_code_board_method)(unsigned int code, int push_type);
};

struct _headset {
	struct switch_dev sdev;
	struct input_dev *input;
	struct _headset_gpio detect;
	struct _headset_gpio button;
	int headphone;
	int type;
	struct work_struct switch_work;
	struct workqueue_struct * switch_workqueue;
};

#ifndef ARRY_SIZE
#define ARRY_SIZE(A) (sizeof(A)/sizeof(A[0]))
#endif

#endif

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

#ifndef __ARCH_KPD_H
#define __ARCH_KPD_H 

struct sci_keypad_platform_data {
	int rows_choose_hw;	/* choose chip keypad controler rows */
	int cols_choose_hw;	/* choose chip keypad controler cols */
	int rows;
	int cols;
	const struct matrix_keymap_data *keymap_data;
	int support_long_key;
	unsigned short repeat;
	unsigned int debounce_time;	/* in ns */
	unsigned int keyup_test_interval;	/* in ms */
};

//chip define begin
#define SCI_COL7	(0x01 << 15)
#define SCI_COL6	(0x01 << 14)
#define SCI_COL5	(0x01 << 13)
#define SCI_COL4	(0x01 << 12)
#define SCI_COL3	(0x01 << 11)
#define SCI_COL2	(0x01 << 10)

#define SCI_ROW7	(0x01 << 23)
#define SCI_ROW6	(0x01 << 22)
#define SCI_ROW5	(0x01 << 21)
#define SCI_ROW4	(0x01 << 20)
#define SCI_ROW3	(0x01 << 19)
#define SCI_ROW2	(0x01 << 18)
//chip define end


#endif

/*
 ****************************************************************
 *
 *  Copyright (C) 2011, Red Bend Ltd.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  You should have received a copy of the GNU General Public License Version 2
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************
 */

#ifndef _VEVDEV_COMMON_H_
#define _VEVDEV_COMMON_H_

#define VKP_NAME	"vkp"
#define VTS_NAME	"vts"
#define VMS_NAME	"vms"

#define VRING_SIZE              16
#define VRING_MASK              (VRING_SIZE - 1)
#define VRING_POS(idx)          ((idx) & VRING_MASK)
#define VRING_PTR(rng,idx)      (rng)->ring[VRING_POS((rng)->idx)]

typedef struct {
        nku16_f type;
        nku16_f code;
        nku32_f value;
} input_vevent ;

typedef struct VRing {
        nku32_f      c_idx;            /* "free running" consumer index */
        nku32_f      p_idx;            /* "free running" producer index */
        input_vevent ring[VRING_SIZE]; /* circular communication buffer */
} VRing;

                             // Linux 2.6.9
#define VEVENT_EV_CNT  0x20  // 0x20
#define VEVENT_KEY_CNT 0x600 // 0x300
#define VEVENT_REL_CNT 0x20  // 0x10
#define VEVENT_ABS_CNT 0x80  // 0x40
#define VEVENT_MSC_CNT 0x10  // 0x08
#define VEVENT_LED_CNT 0x20  // 0x10
#define VEVENT_SND_CNT 0x10  // 0x08
#define VEVENT_FF_CNT  0x100 // 0x80
#define VEVENT_SW_CNT  0x20  // 0x10

typedef struct {
	// Currently we only support following combined events:
	// More information see "struct input_dev" in linux kernel header include/linux/input.h
	unsigned long evbit[BITS_TO_LONGS(VEVENT_EV_CNT)];
	unsigned long keybit[BITS_TO_LONGS(VEVENT_KEY_CNT)];
	unsigned long absbit[BITS_TO_LONGS(VEVENT_ABS_CNT)];
	unsigned long relbit[BITS_TO_LONGS(VEVENT_REL_CNT)];
	unsigned long mscbit[BITS_TO_LONGS(VEVENT_MSC_CNT)];
	unsigned long ledbit[BITS_TO_LONGS(VEVENT_LED_CNT)];
	unsigned long sndbit[BITS_TO_LONGS(VEVENT_SND_CNT)];
	unsigned long ffbit[BITS_TO_LONGS(VEVENT_FF_CNT)];
	unsigned long swbit[BITS_TO_LONGS(VEVENT_SW_CNT)];

	unsigned long key[BITS_TO_LONGS(VEVENT_KEY_CNT)];
	unsigned long led[BITS_TO_LONGS(VEVENT_LED_CNT)];
	unsigned long snd[BITS_TO_LONGS(VEVENT_SND_CNT)];
	unsigned long sw[BITS_TO_LONGS(VEVENT_SW_CNT)];

	int abs[VEVENT_ABS_CNT];
	int absmax[VEVENT_ABS_CNT];
	int absmin[VEVENT_ABS_CNT];
	int absfuzz[VEVENT_ABS_CNT];
	int absflat[VEVENT_ABS_CNT];
} VIdev;

#endif /* _VEVDEV_COMMON_H_ */

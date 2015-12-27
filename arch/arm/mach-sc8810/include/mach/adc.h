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
 *
 ************************************************
 * Automatically generated C config: don't edit *
 ************************************************
 */

#ifndef __CTL_ADC_H__
#define __CTL_ADC_H__

/* adc channel definition */
#define ADC_CHANNEL_INVALID	-1
enum adc_channel {
	ADC_CHANNEL_0 = 	0,
	ADC_CHANNEL_TEMP =	1,
	ADC_CHANNEL_2 =		2,
	ADC_CHANNEL_3 =		3,
	ADC_CHANNEL_PROG =	4,
	ADC_CHANNEL_VBAT =	5,
	ADC_CHANNEL_VCHG =	6,
	ADC_CHANNEL_7 =		7,
	ADC_CHANNEL_8 =		8,
	ADC_CHANNEL_9 =		ADC_CHANNEL_INVALID,
	ADC_CHANNEL_DCDCARM = 10,
	ADC_CHANNEL_DCDC =	11,
	ADC_CHANNEL_12 =	ADC_CHANNEL_INVALID,
	ADC_CHANNEL_13 =	ADC_CHANNEL_INVALID,
	ADC_CHANNEL_14 =	14,
	ADC_CHANNEL_15 =	15,
	ADC_MAX =		16,
};

/* adc scale definition */
enum adc_scale {
	ADC_SCALE_3V = 0,
	ADC_SCALE_1V2 = 1,
};

int sci_adc_get_value(unsigned int channel, int scale);

#endif

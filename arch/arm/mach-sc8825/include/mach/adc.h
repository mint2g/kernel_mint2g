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
	ADC_CHANNEL_0 = 0,
	ADC_CHANNEL_1 = 1,
	ADC_CHANNEL_2 = 2,
	ADC_CHANNEL_3 = 3,
	ADC_CHANNEL_PROG = 4,
	ADC_CHANNEL_VBAT = 5,
	ADC_CHANNEL_VCHGSEN = 6,
	ADC_CHANNEL_VCHGBG = 7,
	ADC_CHANNEL_ISENSE = 8,
	ADC_CHANNEL_TPYD = 9,
	ADC_CHANNEL_TPYU = 10,
	ADC_CHANNEL_TPXR = 11,
	ADC_CHANNEL_TPXL = 12,
	ADC_CHANNEL_DCDCCORE = 13,
	ADC_CHANNEL_DCDCARM = 14,
	ADC_CHANNEL_DCDCMEM = 15,
	ADC_CHANNEL_DCDCLDO = 16,
	ADC_CHANNEL_VBATBK = 17,
	ADC_CHANNEL_HEADMIC = 18,
	ADC_MAX = 18,
};

struct adc_sample_data {
	int sample_num;
	int sample_bits;	/*0: 10bits mode, 1:12 bits mode */
	int signal_mode;	/*0:resistance,1:capacitance */
	int sample_speed;	/*0:quick mode, 1: slow mode */
	int scale;		/*0:little scale, 1:big scale */
	int hw_channel_delay;	/*0:disable, 1:enable */
	int channel_id;		/*channel id of software, Or dedicatid hw channel number */
	int channel_type;	/*0: software, 1: slow hardware , 2: fast hardware */
	int *pbuf;
};

extern void sci_adc_init(void __iomem * adc_base);
extern void sci_adc_dump_register(void);

/*
 * Use this interface to get adc values and could config adc sample behavior.
 */
extern int sci_adc_get_values(struct adc_sample_data *adc);

/*
 * get adc channel voltage divider ratio.
 */
void sci_adc_get_vol_ratio(unsigned int channel_id, int scale, unsigned int* div_numerators,
			unsigned int* div_denominators );
/*
 * Use this interface to get one adc value and this function have set default
 * adc sample behavior.
 */
static inline int sci_adc_get_value(unsigned int channel, int scale)
{
	struct adc_sample_data adc;
	int32_t result[1];

	adc.channel_id = channel;
	adc.channel_type = 0;
	adc.hw_channel_delay = 0;
	adc.pbuf = &result[0];
	adc.sample_bits = 1;
	adc.sample_num = 1;
	adc.sample_speed = 0;
	adc.scale = scale;
	adc.signal_mode = 0;

	if (0 != sci_adc_get_values(&adc)) {
		printk("sci_adc_get_value, return error\n");
		BUG();
	}

	return result[0];
}

#endif

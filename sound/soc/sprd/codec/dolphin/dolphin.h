/*
 * sound/soc/sprd/codec/dolphin/dolphin.h
 *
 * DOLPHIN -- SpreadTrum sc88xx intergrated Dolphin codec.
 *
 * Copyright (C) 2012 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY ork FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __DOLPHIN_H
#define __DOLPHIN_H

#include <mach/sprd-audio.h>

#ifndef CONFIG_CODEC_DAC_MUTE_WAIT
/* #define CONFIG_CODEC_DAC_MUTE_WAIT */
#endif

/* unit: ms */
#define DOLPHIN_LDO_WAIT_TIME			(5)
#define DOLPHIN_TSBYU_WAIT_TIME			(120)
#define DOLPHIN_HP_POP_WAIT_TIME		(295)
#define DOLPHIN_DAC_MUTE_WAIT_TIME		(20)

#define DOLPHIN_RATE_8000   (10)
#define DOLPHIN_RATE_9600   ( 9)
#define DOLPHIN_RATE_11025  ( 8)
#define DOLPHIN_RATE_12000  ( 7)
#define DOLPHIN_RATE_16000  ( 6)
#define DOLPHIN_RATE_22050  ( 5)
#define DOLPHIN_RATE_24000  ( 4)
#define DOLPHIN_RATE_32000  ( 3)
#define DOLPHIN_RATE_44100  ( 2)
#define DOLPHIN_RATE_48000  ( 1)
#define DOLPHIN_RATE_96000  ( 0)

#define VBCAICR_MODE_ADC_I2S	(1 << 0)
#define VBCAICR_MODE_DAC_I2S	(1 << 1)
#define VBCAICR_MODE_ADC_SERIAL	(1 << 2)
#define VBCAICR_MODE_DAC_SERIAL	(1 << 3)

/* VBCR1 */
enum {
	BTL_MUTE = 1,
	BYPASS,
	DACSEL,
	HP_DIS,
	DAC_MUTE,
	MONO,
	SB_MICBIAS,
};
/* VBCR2 */
#define DAC_DATA_WIDTH_16_bit	(0x00)
#define DAC_DATA_WIDTH_18_bit	(0x01)
#define DAC_DATA_WIDTH_20_bit	(0x02)
#define DAC_DATA_WIDTH_24_bit	(0x03)

#define ADC_DATA_WIDTH_16_bit	(0x00)
#define ADC_DATA_WIDTH_18_bit	(0x01)
#define ADC_DATA_WIDTH_20_bit	(0x02)
#define ADC_DATA_WIDTH_24_bit	(0x03)

#define MICROPHONE1				(0)
#define MICROPHONE2				(1)
enum {
	MICSEL = 1,
	ADC_HPF,
	ADC_ADWL,
	DAC_ADWL = 5,
	DAC_DEEMP = 7,
};

/* VBPMR1 */
enum {
	SB_LOUT = 1,
	SB_BTL,
	SB_LIN,
	SB_ADC,
	SB_MIX,
	SB_OUT,
	SB_DAC,
};

/* VBPMR2 */
enum {
	SB_SLEEP = 0,
	SB,
	SB_MC,
	GIM,
	RLGOD,
	LRGOD,
};
/* -------------------------- */

#define ARM_DOL_BASE		CODEC_BASE

#define VBAICR			(ARM_DOL_BASE + 0x0100)	/* 0x0080 Voice band Codec AICR */
#define VBCR1			(ARM_DOL_BASE + 0x0104)	/* 0x0082 Voice band Codec CR1 */
#define VBCR2			(ARM_DOL_BASE + 0x0108)	/* 0x0084 Voice band Codec CR2 */
#define VBCCR1			(ARM_DOL_BASE + 0x010C)	/* 0x0086 Voice band Codec CCR1 */
#define VBCCR2			(ARM_DOL_BASE + 0x0110)	/* 0x0088 Voice band Codec CCR2 */
#define VBPMR1			(ARM_DOL_BASE + 0x0114)	/* 0x008A Voice band Codec PMR1 */
#define VBPMR2			(ARM_DOL_BASE + 0x0118)	/* 0x008C Voice band Codec PMR2 */
#define VBCRR			(ARM_DOL_BASE + 0x011C)	/* 0x008E Voice band Codec CRR */
#define VBICR			(ARM_DOL_BASE + 0x0120)	/* 0x0090 Voice band Codec ICR */
#define VBIFR			(ARM_DOL_BASE + 0x0124)	/* 0x0092 Voice band Codec IFR */
#define VBCGR1			(ARM_DOL_BASE + 0x0128)	/* 0x0094 Voice band Codec CGR1 */
#define VBCGR2			(ARM_DOL_BASE + 0x012C)	/* 0x0096 Voice band Codec CGR2 */
#define VBCGR3			(ARM_DOL_BASE + 0x0130)	/* 0x0098 Voice band Codec CGR3 */
#define VBCGR8			(ARM_DOL_BASE + 0x0144)	/* 0x00A2 Voice band Codec CGR8 */
#define VBCGR9			(ARM_DOL_BASE + 0x0148)	/* 0x00A4 Voice band Codec CGR9 */
#define VBCGR10			(ARM_DOL_BASE + 0x014C)	/* 0x00A6 Voice band Codec CGR10 */
#define VBTR1			(ARM_DOL_BASE + 0x0150)	/* 0x00A8 Voice band Codec TR1 */
#define VBTR2			(ARM_DOL_BASE + 0x0154)	/* 0x00AA Voice band Codec TR2 */
#define IS_DOLPHIN_RANG(reg) (((reg) >= VBAICR) && ((reg) <= VBTR2))

#endif /* __DOLPHIN_H */

/*
 * sound/soc/sprd/dai/vbc/vbc.h
 *
 * SPRD SoC CPU-DAI -- SpreadTrum SOC DAI with EQ&ALC and some loop.
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
#ifndef __VBC_H
#define __VBC_H

#include <mach/sprd-audio.h>

#define VBC_VERSION	"vbc.r0p0"

#define VBC_EQ_FIRMWARE_MAGIC_LEN	(4)
#define VBC_EQ_FIRMWARE_MAGIC_ID	("VBEQ")
#define VBC_EQ_PROFILE_VERSION		(0x00000001)
#define VBC_EQ_PROFILE_CNT_MAX		(50)
#define VBC_EQ_PROFILE_NAME_MAX		(32)
#define VBC_EFFECT_PARAS_LEN            (61)

/* VBADBUFFDTA */
enum {
	VBISADCK_INV = 9,
	VBISDACK_INV,
	VBLSB_EB,
	VBIIS_DLOOP = 13,
	VBPCM_MODE,
	VBIIS_LRCK,
};
/* VBDABUFFDTA */
enum {
	RAMSW_NUMB = 9,
	RAMSW_EN,
	VBAD0DMA_EN,
	VBAD1DMA_EN,
	VBDA0DMA_EN,
	VBDA1DMA_EN,
	VBENABLE,
};
/* -------------------------- */

#define PHYS_VBDA0		(VBC_PHY_BASE + 0x0000)	/* 0x0000  Voice band DAC0 data buffer */
#define PHYS_VBDA1		(VBC_PHY_BASE + 0x0004)	/* 0x0002  Voice band DAC1 data buffer */
#define PHYS_VBAD0		(VBC_PHY_BASE + 0x0008)	/* 0x0004  Voice band ADC0 data buffer */
#define PHYS_VBAD1		(VBC_PHY_BASE + 0x000C)	/* 0x0006  Voice band ADC1 data buffer */

#define ARM_VB_BASE		VBC_BASE
#define VBDA0			(ARM_VB_BASE + 0x0000)	/* 0x0000  Voice band DAC0 data buffer */
#define VBDA1			(ARM_VB_BASE + 0x0004)	/* 0x0002  Voice band DAC1 data buffer */
#define VBAD0			(ARM_VB_BASE + 0x0008)	/* 0x0004  Voice band ADC0 data buffer */
#define VBAD1			(ARM_VB_BASE + 0x000C)	/* 0x0006  Voice band ADC1 data buffer */
#define VBBUFFSIZE		(ARM_VB_BASE + 0x0010)	/* 0x0008  Voice band buffer size */
#define VBADBUFFDTA		(ARM_VB_BASE + 0x0014)	/* 0x000A  Voice band AD buffer control */
#define VBDABUFFDTA		(ARM_VB_BASE + 0x0018)	/* 0x000C  Voice band DA buffer control */
#define VBADCNT			(ARM_VB_BASE + 0x001C)	/* 0x000E  Voice band AD buffer counter */
#define VBDACNT			(ARM_VB_BASE + 0x0020)	/* 0x0010  Voice band DA buffer counter */
#define VBDAICTL		(ARM_VB_BASE + 0x0024)	/* 0x0012  Voice band DAI control */
#define VBDAIIN			(ARM_VB_BASE + 0x0028)	/* 0x0014  Voice band DAI input */
#define VBDAIOUT		(ARM_VB_BASE + 0x002C)	/* 0x0016  Voice band DAI output */

#define DAPATCHCTL		(ARM_VB_BASE + 0x0040)
#define DADGCTL			(ARM_VB_BASE + 0x0044)
#define DAHPCTL			(ARM_VB_BASE + 0x0048)
#define DAALCCTL0		(ARM_VB_BASE + 0x004C)
#define DAALCCTL1		(ARM_VB_BASE + 0x0050)
#define DAALCCTL2		(ARM_VB_BASE + 0x0054)
#define DAALCCTL3		(ARM_VB_BASE + 0x0058)
#define DAALCCTL4		(ARM_VB_BASE + 0x005C)
#define DAALCCTL5		(ARM_VB_BASE + 0x0060)
#define DAALCCTL6		(ARM_VB_BASE + 0x0064)
#define DAALCCTL7		(ARM_VB_BASE + 0x0068)
#define DAALCCTL8		(ARM_VB_BASE + 0x006C)
#define DAALCCTL9		(ARM_VB_BASE + 0x0070)
#define DAALCCTL10		(ARM_VB_BASE + 0x0074)
#define STCTL0			(ARM_VB_BASE + 0x0078)
#define STCTL1			(ARM_VB_BASE + 0x007C)
#define ADPATCHCTL		(ARM_VB_BASE + 0x0080)
#define ADDGCTL			(ARM_VB_BASE + 0x0084)
#define HPCOEF0			(ARM_VB_BASE + 0x0100)
#define HPCOEF1			(ARM_VB_BASE + 0x0104)
#define HPCOEF2			(ARM_VB_BASE + 0x0108)
#define HPCOEF3			(ARM_VB_BASE + 0x010C)
#define HPCOEF4			(ARM_VB_BASE + 0x0110)
#define HPCOEF5			(ARM_VB_BASE + 0x0114)
#define HPCOEF6			(ARM_VB_BASE + 0x0118)
#define HPCOEF7			(ARM_VB_BASE + 0x011C)
#define HPCOEF8			(ARM_VB_BASE + 0x0120)
#define HPCOEF9			(ARM_VB_BASE + 0x0124)
#define HPCOEF10		(ARM_VB_BASE + 0x0128)
#define HPCOEF11		(ARM_VB_BASE + 0x012C)
#define HPCOEF12		(ARM_VB_BASE + 0x0130)
#define HPCOEF13		(ARM_VB_BASE + 0x0134)
#define HPCOEF14		(ARM_VB_BASE + 0x0138)
#define HPCOEF15		(ARM_VB_BASE + 0x013C)
#define HPCOEF16		(ARM_VB_BASE + 0x0140)
#define HPCOEF17		(ARM_VB_BASE + 0x0144)
#define HPCOEF18		(ARM_VB_BASE + 0x0148)
#define HPCOEF19		(ARM_VB_BASE + 0x014C)
#define HPCOEF20		(ARM_VB_BASE + 0x0150)
#define HPCOEF21		(ARM_VB_BASE + 0x0154)
#define HPCOEF22		(ARM_VB_BASE + 0x0158)
#define HPCOEF23		(ARM_VB_BASE + 0x015C)
#define HPCOEF24		(ARM_VB_BASE + 0x0160)
#define HPCOEF25		(ARM_VB_BASE + 0x0164)
#define HPCOEF26		(ARM_VB_BASE + 0x0168)
#define HPCOEF27		(ARM_VB_BASE + 0x016C)
#define HPCOEF28		(ARM_VB_BASE + 0x0170)
#define HPCOEF29		(ARM_VB_BASE + 0x0174)
#define HPCOEF30		(ARM_VB_BASE + 0x0178)
#define HPCOEF31		(ARM_VB_BASE + 0x017C)
#define HPCOEF32		(ARM_VB_BASE + 0x0180)
#define HPCOEF33		(ARM_VB_BASE + 0x0184)
#define HPCOEF34		(ARM_VB_BASE + 0x0188)
#define HPCOEF35		(ARM_VB_BASE + 0x018C)
#define HPCOEF36		(ARM_VB_BASE + 0x0190)
#define HPCOEF37		(ARM_VB_BASE + 0x0194)
#define HPCOEF38		(ARM_VB_BASE + 0x0198)
#define HPCOEF39		(ARM_VB_BASE + 0x019C)
#define HPCOEF40		(ARM_VB_BASE + 0x01A0)
#define HPCOEF41		(ARM_VB_BASE + 0x01A4)
#define HPCOEF42		(ARM_VB_BASE + 0x01A8)

#define ARM_VB_END		(ARM_VB_BASE + 0x01AC)

#define VBADBUFFERSIZE_SHIFT	(0)
#define VBADBUFFERSIZE_MASK	(0xFF<<VBADBUFFERSIZE_SHIFT)
#define VBDABUFFERSIZE_SHIFT	(8)
#define VBDABUFFERSIZE_MASK	(0xFF<<VBDABUFFERSIZE_SHIFT)

#endif /* __VBC_H */

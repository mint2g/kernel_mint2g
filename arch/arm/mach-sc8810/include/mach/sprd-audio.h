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

#ifndef __AUDIO_GLB_REG_H
#define __AUDIO_GLB_REG_H

#include <linux/delay.h>
#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <mach/adi.h>
#include <mach/dma.h>

/* OKAY, this is for other else owner
   if you do not care the audio config
   you can set FIXED_AUDIO  to 0
   for compile happy.
*/
/* FIXME */
#define FIXED_AUDIO 1

enum {
	AUDIO_NO_CHANGE,
	AUDIO_TO_DSP_CTRL,
	AUDIO_TO_ARM_CTRL,
};

#if FIXED_AUDIO
#define VBC_BASE	SPRD_VB_BASE
#define CODEC_BASE	SPRD_MISC_BASE
#define VBC_PHY_BASE	SPRD_VB_PHYS
#define CODEC_PHY_BASE	SPRD_MISC_PHYS

#define	LDO_REG_BASE		(SPRD_MISC_BASE + 0x600)
#define ANA_AUDIO_CTRL		(LDO_REG_BASE + 0x0074)
#define	ANA_AUDIO_PA_CTRL0	(LDO_REG_BASE + 0x0078)
#define	ANA_AUDIO_PA_CTRL1	(LDO_REG_BASE + 0x007C)
#endif

#define AUDIO_HEADSET_ENABLE BIT(7)

/* ------------------------------------------------------------------------- */

/* NOTE: all function maybe will call by atomic funtion
         don NOT any complex oprations. Just register.
return
   0:  	unchanged
   1:	changed
   ohter error
*/
/* vbc setting */

static inline int arch_audio_vbc_reg_enable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sprd_greg_set_bits(REG_TYPE_GLOBAL, GEN0_VB_EN, GR_GEN0);
#endif

	return ret;
}

static inline int arch_audio_vbc_reg_disable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, GEN0_VB_EN, GR_GEN0);
#endif

	return ret;
}

static inline int arch_audio_vbc_enable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sprd_greg_set_bits(REG_TYPE_GLOBAL, ARM_VB_ANAON, GR_BUSCLK);
#endif

	return ret;
}

static inline int arch_audio_vbc_disable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, ARM_VB_ANAON, GR_BUSCLK);
#endif

	return ret;
}

static inline int arch_audio_vbc_switch(int master)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (master) {
	case AUDIO_TO_ARM_CTRL:
		sprd_greg_set_bits(REG_TYPE_GLOBAL, ARM_VB_ACC, GR_BUSCLK);
		break;
	case AUDIO_TO_DSP_CTRL:
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, ARM_VB_ACC, GR_BUSCLK);
		break;
	case AUDIO_NO_CHANGE:
		ret = sprd_greg_read(REG_TYPE_GLOBAL, GR_BUSCLK) & ARM_VB_ACC;
		if (ret == 0)
			ret = AUDIO_TO_DSP_CTRL;
		else
			ret = AUDIO_TO_ARM_CTRL;
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_vbc_ad_enable(int chan)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (chan) {
	case 0:
		sprd_greg_set_bits(REG_TYPE_GLOBAL, ARM_VB_AD0ON, GR_BUSCLK);
		break;
	case 1:
		sprd_greg_set_bits(REG_TYPE_GLOBAL, ARM_VB_AD1ON, GR_BUSCLK);
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_vbc_ad_disable(int chan)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (chan) {
	case 0:
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, ARM_VB_AD0ON, GR_BUSCLK);
		break;
	case 1:
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, ARM_VB_AD1ON, GR_BUSCLK);
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_vbc_da_enable(int chan)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (chan) {
	case 0:
		sprd_greg_set_bits(REG_TYPE_GLOBAL, ARM_VB_DA0ON, GR_BUSCLK);
		break;
	case 1:
		sprd_greg_set_bits(REG_TYPE_GLOBAL, ARM_VB_DA1ON, GR_BUSCLK);
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_vbc_da_disable(int chan)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (chan) {
	case 0:
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, ARM_VB_DA0ON, GR_BUSCLK);
		break;
	case 1:
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, ARM_VB_DA1ON, GR_BUSCLK);
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_vbc_da_dma_info(int chan)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (chan) {
	case 0:
		ret = DMA_VB_DA0;
		break;
	case 1:
		ret = DMA_VB_DA1;
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_vbc_ad_dma_info(int chan)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (chan) {
	case 0:
		ret = DMA_VB_AD0;
		break;
	case 1:
		ret = DMA_VB_AD1;
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_vbc_reset(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sprd_greg_set_bits(REG_TYPE_GLOBAL, SWRST_VBC_RST, GR_SOFT_RST);
	udelay(10);
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, SWRST_VBC_RST, GR_SOFT_RST);
#endif

	return ret;
}

/* some SOC will move this into vbc module */
static inline int arch_audio_vbc_ad_int_clr(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sprd_greg_set_bits(REG_TYPE_GLOBAL, ICLR_VBCAD_IRQ_CLR, GR_ICLR);
#endif

	return ret;
}

/* some SOC will move this into vbc module */
static inline int arch_audio_vbc_da_int_clr(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sprd_greg_set_bits(REG_TYPE_GLOBAL, ICLR_VBCDA_IRQ_CLR, GR_ICLR);
#endif

	return ret;
}

/* some SOC will move this into vbc module */
static inline int arch_audio_vbc_is_ad_int(void)
{
	int ret = 0;

#if FIXED_AUDIO
	ret = sprd_greg_read(REG_TYPE_GLOBAL, GR_IRQ) & IRQ_VBCAD_IRQ;
#endif

	return ret;
}

/* some SOC will move this into vbc module */
static inline int arch_audio_vbc_is_da_int(void)
{
	int ret = 0;

#if FIXED_AUDIO
	ret = sprd_greg_read(REG_TYPE_GLOBAL, GR_IRQ) & IRQ_VBCDA_IRQ;
#endif

	return ret;
}

/* ------------------------------------------------------------------------- */

/* codec setting */
static inline int arch_audio_codec_write(int reg, int val)
{
	int ret = 0;

#if FIXED_AUDIO
	ret = sci_adi_raw_write(reg, val);
#endif

	return ret;
}

static inline int arch_audio_codec_read(int reg)
{
	int ret = 0;

#if FIXED_AUDIO
	ret = sci_adi_read(reg);
#endif

	return ret;
}

static inline int arch_audio_codec_enable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sprd_greg_set_bits(REG_TYPE_GLOBAL, ARM_VB_MCLKON, GR_BUSCLK);
	ret = sci_adi_set(ANA_AUDIO_CTRL, BIT(0));
#endif

	return ret;
}

static inline int arch_audio_codec_disable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, ARM_VB_MCLKON, GR_BUSCLK);
	ret = sci_adi_clr(ANA_AUDIO_CTRL, BIT(0));
#endif

	return ret;
}

static inline int arch_audio_codec_switch(int master)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (master) {
	case AUDIO_TO_ARM_CTRL:
		ret = sci_adi_set(ANA_AUDIO_CTRL, BIT(1));
		break;
	case AUDIO_TO_DSP_CTRL:
		ret = sci_adi_clr(ANA_AUDIO_CTRL, BIT(1));
		break;
	case AUDIO_NO_CHANGE:
		ret = sci_adi_read(ANA_AUDIO_CTRL) & BIT(1);
		if (ret == 0)
			ret = AUDIO_TO_DSP_CTRL;
		else
			ret = AUDIO_TO_ARM_CTRL;
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_codec_reset(void)
{
	int ret = 0;

#if FIXED_AUDIO
	ret = sci_adi_set(ANA_AUDIO_CTRL, BIT(15));
	if (ret < 0)
		return ret;
	udelay(10);
	ret = sci_adi_clr(ANA_AUDIO_CTRL, BIT(15));
#endif

	return ret;
}

/* some SOC will move this into codec module */
static inline int arch_audio_codec_lineinrec_enable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	ret = sci_adi_set(ANA_AUDIO_CTRL, BIT(3));
#endif

	return ret;
}

/* some SOC will move this into codec module */
static inline int arch_audio_codec_lineinrec_disable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	ret = sci_adi_clr(ANA_AUDIO_CTRL, BIT(3));
#endif

	return ret;
}

/* some SOC will move this into codec module */
static inline int arch_audio_codec_lineinrec_set_pga(int pga)
{
	int ret = 0;

#if FIXED_AUDIO
	int reg = sci_adi_read(ANA_AUDIO_CTRL);
	reg &= ~(0x0070);
	ret = sci_adi_raw_write(ANA_AUDIO_CTRL, (((pga & 0x07) << 4) | reg));
#endif

	return ret;
}

/* some SOC will move this into codec module */
static inline int arch_audio_codec_lineinrec_get_pga(void)
{
	int ret = 0;

#if FIXED_AUDIO
	int reg = sci_adi_read(ANA_AUDIO_CTRL);
	ret = (reg >> 4) & 0x07;
#endif

	return ret;
}

/* some SOC will move this into codec module */
static inline int arch_audio_inter_pa_disable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	int reg = sci_adi_read(ANA_AUDIO_PA_CTRL0) & 0xF0;
	ret = sci_adi_raw_write(ANA_AUDIO_PA_CTRL0, (0x1182 | reg));
	if (ret < 0)
		return ret;
	ret = sci_adi_raw_write(ANA_AUDIO_PA_CTRL1, 0x5e41);
#endif
	udelay(300);

	return ret;
}

/* some SOC will move this into codec module */
static inline int arch_audio_inter_pa_enable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	int reg = sci_adi_read(ANA_AUDIO_PA_CTRL0) & 0xF0;
	ret = sci_adi_raw_write(ANA_AUDIO_PA_CTRL0, (0x1181 | reg));
	if (ret < 0)
		return ret;
	ret = sci_adi_raw_write(ANA_AUDIO_PA_CTRL1, 0x5e41);
#endif

	return ret;
}

/* some SOC will move this into codec module */
static inline int arch_audio_inter_pa_set_pga(int pga)
{
	int ret = 0;

#if FIXED_AUDIO
	int reg = sci_adi_read(ANA_AUDIO_PA_CTRL0);
	reg &= ~(0x00F0);
	ret =
	    sci_adi_raw_write(ANA_AUDIO_PA_CTRL0, (((pga & 0x0F) << 4) | reg));
#endif

	return ret;
}

/* some SOC will move this into codec module */
static inline int arch_audio_inter_pa_get_pga(void)
{
	int ret = 0;

#if FIXED_AUDIO
	int reg = sci_adi_read(ANA_AUDIO_PA_CTRL0);
	ret = (reg >> 4) & 0x0F;
#endif

	return ret;
}

/* ------------------------------------------------------------------------- */

/* i2s setting */

static inline int arch_audio_i2s_enable(int id)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (id) {
	case 0:
		sprd_greg_set_bits(REG_TYPE_GLOBAL, GEN0_I2S0_EN, GR_GEN0);
		break;
	case 1:
		sprd_greg_set_bits(REG_TYPE_GLOBAL, GEN0_I2S1_EN, GR_GEN0);
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_i2s_disable(int id)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (id) {
	case 0:
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, GEN0_I2S0_EN, GR_GEN0);
		break;
	case 1:
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, GEN0_I2S1_EN, GR_GEN0);
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_i2s_tx_dma_info(int id)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (id) {
	case 0:
		ret = DMA_IIS_TX;
		break;
	case 1:
		ret = DMA_IIS1_TX;
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_i2s_rx_dma_info(int id)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (id) {
	case 0:
		ret = DMA_IIS_RX;
		break;
	case 1:
		ret = DMA_IIS1_RX;
		break;
	default:
		ret = -ENODEV;
		break;
	}

#endif

	return ret;
}

static inline int arch_audio_i2s_reset(int id)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (id) {
	case 0:
		sprd_greg_set_bits(REG_TYPE_GLOBAL, SWRST_IIS_RST, GR_SOFT_RST);
		udelay(10);
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, SWRST_IIS_RST,
				     GR_SOFT_RST);
		break;
	case 1:
		sprd_greg_set_bits(REG_TYPE_GLOBAL, SWRST_IIS1_RST,
				   GR_SOFT_RST);
		udelay(10);
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, SWRST_IIS1_RST,
				     GR_SOFT_RST);
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

static inline int arch_audio_i2s_switch(int id, int master)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (id) {
	case 0:
		switch (master) {
		case AUDIO_TO_ARM_CTRL:
			sprd_greg_set_bits(REG_TYPE_GLOBAL, IIS0_SEL, GR_PCTL);
			break;
		case AUDIO_TO_DSP_CTRL:
			sprd_greg_clear_bits(REG_TYPE_GLOBAL, IIS0_SEL,
					     GR_PCTL);
			break;
		case AUDIO_NO_CHANGE:
			ret =
			    sprd_greg_read(REG_TYPE_GLOBAL, GR_PCTL) & IIS0_SEL;
			if (ret == 0)
				ret = AUDIO_TO_DSP_CTRL;
			else
				ret = AUDIO_TO_ARM_CTRL;
			break;
		default:
			ret = -ENODEV;
			break;
		}
		break;
	case 1:
		switch (master) {
		case AUDIO_TO_ARM_CTRL:
			sprd_greg_set_bits(REG_TYPE_GLOBAL, IIS1_SEL, GR_PCTL);
			break;
		case AUDIO_TO_DSP_CTRL:
			sprd_greg_clear_bits(REG_TYPE_GLOBAL, IIS1_SEL,
					     GR_PCTL);
			break;
		case AUDIO_NO_CHANGE:
			ret =
			    sprd_greg_read(REG_TYPE_GLOBAL, GR_PCTL) & IIS1_SEL;
			if (ret == 0)
				ret = AUDIO_TO_DSP_CTRL;
			else
				ret = AUDIO_TO_ARM_CTRL;
			break;
		default:
			ret = -ENODEV;
			break;
		}
		break;
	default:
		ret = -ENODEV;
		break;
	}
#endif

	return ret;
}

#endif

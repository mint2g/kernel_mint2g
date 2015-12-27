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
#include <mach/sci.h>
#include <mach/regs_glb.h>
#include <mach/regs_ana_glb.h>
#include <mach/adi.h>
#include <mach/dma.h>
#include <mach/irqs.h>

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
	AUDIO_TO_ARM_CP_CTRL,
};

#if FIXED_AUDIO
#define VBC_BASE		SPRD_VB_BASE
#define CODEC_DP_BASE 		(SPRD_VB_BASE + 0x1000)
#define CODEC_AP_BASE		(SPRD_MISC_BASE + 0x0700)
#define VBC_PHY_BASE		SPRD_VB_PHYS
#define CODEC_DP_PHY_BASE	(SPRD_VB_PHYS + 0x1000)
#define CODEC_AP_PHY_BASE	(SPRD_MISC_PHYS + 0x0700)
#define CODEC_AP_IRQ		(IRQ_ANA_AUD_INT)
#define CODEC_DP_IRQ		(IRQ_REQ_AUD_INT)

#ifdef CONFIG_SPRD_AUDIO_BUFFER_USE_IRAM
#define SPRD_IRAM_ALL_PHYS	0X00000000
#define SPRD_IRAM_ALL_SIZE	SZ_32K
#endif
#endif

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
	sci_glb_set(REG_GLB_GEN1, BIT_VBC_EN);
#endif

	return ret;
}

static inline int arch_audio_vbc_reg_disable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sci_glb_clr(REG_GLB_GEN1, BIT_VBC_EN);
#endif

	return ret;
}

static inline int arch_audio_vbc_enable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sci_glb_set(REG_GLB_BUSCLK, BIT_ARM_VBC_ANAON);
#endif

	return ret;
}

static inline int arch_audio_vbc_disable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sci_glb_clr(REG_GLB_BUSCLK, BIT_ARM_VBC_ANAON);
#endif

	return ret;
}

static inline int arch_audio_vbc_switch(int master)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (master) {
	case AUDIO_TO_ARM_CTRL:
		sci_glb_set(REG_GLB_BUSCLK, BIT_ARM_VBC_ACC);
		sci_glb_clr(REG_GLB_BUSCLK, BIT_ARM_VBC_ACC_CP);
		break;
	case AUDIO_TO_ARM_CP_CTRL:
		sci_glb_set(REG_GLB_BUSCLK, BIT_ARM_VBC_ACC);
		sci_glb_set(REG_GLB_BUSCLK, BIT_ARM_VBC_ACC_CP);
		break;
	case AUDIO_TO_DSP_CTRL:
		sci_glb_clr(REG_GLB_BUSCLK, BIT_ARM_VBC_ACC);
		break;
	case AUDIO_NO_CHANGE:
		ret = sci_glb_read(REG_GLB_BUSCLK, BIT_ARM_VBC_ACC);
		if (ret == 0) {
			ret = AUDIO_TO_DSP_CTRL;
		} else {
			ret = sci_glb_read(REG_GLB_BUSCLK, BIT_ARM_VBC_ACC_CP);
			if (ret == 0)
				ret = AUDIO_TO_ARM_CTRL;
			else
				ret = AUDIO_TO_ARM_CP_CTRL;
		}
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
		sci_glb_set(REG_GLB_BUSCLK, BIT_ARM_VBC_AD0ON);
		break;
	case 1:
		sci_glb_set(REG_GLB_BUSCLK, BIT_ARM_VBC_AD1ON);
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
		sci_glb_clr(REG_GLB_BUSCLK, BIT_ARM_VBC_AD0ON);
		break;
	case 1:
		sci_glb_clr(REG_GLB_BUSCLK, BIT_ARM_VBC_AD1ON);
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
		sci_glb_set(REG_GLB_BUSCLK, BIT_ARM_VBC_DA0ON);
		break;
	case 1:
		sci_glb_set(REG_GLB_BUSCLK, BIT_ARM_VBC_DA1ON);
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
		sci_glb_clr(REG_GLB_BUSCLK, BIT_ARM_VBC_DA0ON);
		break;
	case 1:
		sci_glb_clr(REG_GLB_BUSCLK, BIT_ARM_VBC_DA1ON);
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
	sci_glb_set(REG_GLB_SOFT_RST, BIT_VBC_RST);
	udelay(10);
	sci_glb_clr(REG_GLB_SOFT_RST, BIT_VBC_RST);
#endif

	return ret;
}

/* some SOC will move this into vbc module */
static inline int arch_audio_vbc_ad_int_clr(void)
{
	int ret = 0;

#if FIXED_AUDIO
#endif

	return ret;
}

/* some SOC will move this into vbc module */
static inline int arch_audio_vbc_da_int_clr(void)
{
	int ret = 0;

#if FIXED_AUDIO
#endif

	return ret;
}

/* some SOC will move this into vbc module */
static inline int arch_audio_vbc_is_ad_int(void)
{
	int ret = 0;

#if FIXED_AUDIO
#endif

	return ret;
}

/* some SOC will move this into vbc module */
static inline int arch_audio_vbc_is_da_int(void)
{
	int ret = 0;

#if FIXED_AUDIO
#endif

	return ret;
}

/* ------------------------------------------------------------------------- */

/* codec setting */
static inline int arch_audio_codec_write_mask(int reg, int val, int mask)
{
	int ret = 0;

#if FIXED_AUDIO
	ret = sci_adi_write(reg, val, mask);
#endif

	return ret;
}

static inline int arch_audio_codec_write(int reg, int val)
{
	int ret = 0;

#if FIXED_AUDIO
	ret = sci_adi_write(reg, val, 0xFFFF);
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

static inline int arch_audio_codec_audif_enable(int auto_clk)
{
	int ret = 0;

#if FIXED_AUDIO
	if (auto_clk) {
		sci_glb_clr(REG_GLB_GEN1, BIT_AUD_IF_EB);
		sci_glb_set(REG_GLB_GEN1, BIT_AUDIF_AUTO_EN);
	} else {
		sci_glb_set(REG_GLB_GEN1, BIT_AUD_IF_EB);
		sci_glb_clr(REG_GLB_GEN1, BIT_AUDIF_AUTO_EN);
	}
#endif

	return ret;
}

static inline int arch_audio_codec_audif_disable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sci_glb_clr(REG_GLB_GEN1, BIT_AUDIF_AUTO_EN);
	sci_glb_clr(REG_GLB_GEN1, BIT_AUD_IF_EB);
#endif

	return ret;
}

static inline int arch_audio_codec_reg_enable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sci_glb_set(REG_GLB_GEN1, BIT_AUD_TOP_EB);
	ret =
	    sci_adi_write(ANA_REG_GLB_ARM_AUD_CLK_RST, BIT_AUD_ARM_EN,
			      BIT_AUD_ARM_EN);
#endif

	return ret;
}

static inline int arch_audio_codec_reg_disable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	sci_glb_clr(REG_GLB_GEN1, BIT_AUD_TOP_EB);
	ret = sci_adi_write(ANA_REG_GLB_ARM_AUD_CLK_RST, 0, BIT_AUD_ARM_EN);
#endif

	return ret;
}

static inline int arch_audio_codec_enable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	int mask =
	    BIT_RTC_AUD_ARM_EN | BIT_CLK_AUD_6M5_ARM_EN | BIT_CLK_AUDIF_ARM_EN;
	ret = sci_adi_write(ANA_REG_GLB_ARM_AUD_CLK_RST, mask, mask);
	if (ret >= 0)
		arch_audio_codec_audif_enable(1);
#endif

	return ret;
}

static inline int arch_audio_codec_disable(void)
{
	int ret = 0;

#if FIXED_AUDIO
	int mask =
	    BIT_RTC_AUD_ARM_EN | BIT_CLK_AUD_6M5_ARM_EN | BIT_CLK_AUDIF_ARM_EN;
	arch_audio_codec_audif_disable();
	ret = sci_adi_write(ANA_REG_GLB_ARM_AUD_CLK_RST, 0, mask);
#endif

	return ret;
}

static inline int arch_audio_codec_switch(int master)
{
	int ret = 0;

#if FIXED_AUDIO
	switch (master) {
	case AUDIO_TO_ARM_CTRL:
		sci_glb_set(REG_GLB_GEN1, BIT_AUD_CTL_SEL | BIT_AUD_CLK_SEL);
		sci_glb_set(REG_GLB_BUSCLK, BIT_ARM_VB_SEL);
		ret =
		    sci_adi_write(ANA_REG_GLB_ARM_AUD_CLK_RST,
				      BIT_AUD_ARM_ACC, BIT_AUD_ARM_ACC);
		break;
	case AUDIO_TO_DSP_CTRL:
		sci_glb_clr(REG_GLB_GEN1, BIT_AUD_CTL_SEL | BIT_AUD_CLK_SEL);
		sci_glb_clr(REG_GLB_BUSCLK, BIT_ARM_VB_SEL);
		ret =
		    sci_adi_write(ANA_REG_GLB_ARM_AUD_CLK_RST, 0,
				      BIT_AUD_ARM_ACC);
		break;
	case AUDIO_NO_CHANGE:
		ret =
		    sci_adi_read(ANA_REG_GLB_ARM_AUD_CLK_RST) & BIT_AUD_ARM_ACC;
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
	int mask =
	    BIT_AUD_ARM_SOFT_RST | BIT_AUDTX_ARM_SOFT_RST |
	    BIT_AUDRX_ARM_SOFT_RST;
	sci_glb_set(REG_GLB_SOFT_RST, BIT_AUD_TOP_RST);
	sci_glb_set(REG_GLB_SOFT_RST, BIT_AUD_IF_RST);
	ret = sci_adi_write(ANA_REG_GLB_ARM_AUD_CLK_RST, mask, mask);
	udelay(10);
	sci_glb_clr(REG_GLB_SOFT_RST, BIT_AUD_TOP_RST);
	sci_glb_clr(REG_GLB_SOFT_RST, BIT_AUD_IF_RST);
	if (ret >= 0)
		ret = sci_adi_write(ANA_REG_GLB_ARM_AUD_CLK_RST, 0, mask);
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
		sci_glb_set(REG_GLB_GEN0, BIT_IIS0_EB);
		break;
	case 1:
		sci_glb_set(REG_GLB_GEN0, BIT_IIS1_EB);
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
		sci_glb_clr(REG_GLB_GEN0, BIT_IIS0_EB);
		break;
	case 1:
		sci_glb_clr(REG_GLB_GEN0, BIT_IIS1_EB);
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
		sci_glb_set(REG_GLB_SOFT_RST, BIT_IIS0_RST);
		udelay(10);
		sci_glb_clr(REG_GLB_SOFT_RST, BIT_IIS0_RST);
		break;
	case 1:
		sci_glb_set(REG_GLB_SOFT_RST, BIT_IIS1_RST);
		udelay(10);
		sci_glb_clr(REG_GLB_SOFT_RST, BIT_IIS1_RST);
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
			sci_glb_set(REG_GLB_PCTRL, BIT_IIS0_CTL_SEL);
			break;
		case AUDIO_TO_DSP_CTRL:
			sci_glb_clr(REG_GLB_PCTRL, BIT_IIS0_CTL_SEL);
			break;
		case AUDIO_NO_CHANGE:
			ret = sci_glb_read(REG_GLB_PCTRL, BIT_IIS0_CTL_SEL);
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
			sci_glb_set(REG_GLB_PCTRL, BIT_IIS1_CTL_SEL);
			break;
		case AUDIO_TO_DSP_CTRL:
			sci_glb_clr(REG_GLB_PCTRL, BIT_IIS1_CTL_SEL);
			break;
		case AUDIO_NO_CHANGE:
			ret = sci_glb_read(REG_GLB_PCTRL, BIT_IIS1_CTL_SEL);
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

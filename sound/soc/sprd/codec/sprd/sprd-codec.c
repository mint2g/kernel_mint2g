/*
 * sound/soc/sprd/codec/sprd/sprd-codec.c
 *
 * SPRD-CODEC -- SpreadTrum Tiger intergrated codec.
 *
 * Copyright (C) 2012 SpreadTrum Ltd.
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
#define pr_fmt(fmt) "[audio:codec] " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/atomic.h>
#include <linux/regulator/consumer.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#include <mach/sprd-audio.h>
#include <mach/globalregs.h>
#include "sprd-codec.h"

#ifdef CONFIG_SPRD_AUDIO_DEBUG
#define sprd_codec_dbg pr_debug
#define sprd_bug_on BUG_ON
#else
#define sprd_codec_dbg(...)
#define sprd_bug_on(...)
#endif

#define SOC_REG(r) ((unsigned short)(r))
#define FUN_REG(f) ((unsigned short)(-((f) + 1)))
#define ID_FUN(id, lr) ((unsigned short)(((id) << 1) | (lr)))

#define SPRD_CODEC_AP_BASE_HI (SPRD_CODEC_AP_BASE & 0xFFFF0000)
#define SPRD_CODEC_DP_BASE_HI (SPRD_CODEC_DP_BASE & 0xFFFF0000)

#define SPRD_CODEC_NUM_SUPPLIES 8
static const char *sprd_codec_supply_names[SPRD_CODEC_NUM_SUPPLIES] = {
	"audio_auxmicbias",	/* AUXMICBIAS_EN        (0) */
	"audio_micbias",	/* MICBIAS_EN           (1) */
	"audio_vbo",		/* VBO_EN               (2) */
	"audio_vcmbuf",		/* VCM_BUF_EN           (3) */
	"audio_vcm",		/* VCM_EN               (4) */
	"audio_bg_ibias",	/* BG_IBIAS_EN          (5) */
	"audio_bg",		/* BG_EN                (6) */
	"audio_vb",		/* VB_EN                (7) */
};

enum {
	SPRD_CODEC_PGA_SPKL = 0,
	SPRD_CODEC_PGA_SPKR,
	SPRD_CODEC_PGA_HPL,
	SPRD_CODEC_PGA_HPR,
	SPRD_CODEC_PGA_EAR,
	SPRD_CODEC_PGA_ADCL,
	SPRD_CODEC_PGA_ADCR,

	SPRD_CODEC_PGA_MAX
};

const char *sprd_codec_pga_debug_str[SPRD_CODEC_PGA_MAX] = {
	"SPKL",
	"SPKR",
	"HPL",
	"HPR",
	"EAR",
	"ADCL",
	"ADCR",
};

typedef int (*sprd_codec_pga_set) (struct snd_soc_codec * codec, int pgaval);

struct sprd_codec_pga {
	sprd_codec_pga_set set;
	int min;
};

struct sprd_codec_pga_op {
	int pgaval;
	sprd_codec_pga_set set;
};

enum {
	SPRD_CODEC_LEFT = 0,
	SPRD_CODEC_RIGHT = 1,
};

enum {
	SPRD_CODEC_MIXER_START = 0,
	SPRD_CODEC_AIL = SPRD_CODEC_MIXER_START,
	SPRD_CODEC_AIR,
	SPRD_CODEC_MAIN_MIC,
	SPRD_CODEC_AUX_MIC,
	SPRD_CODEC_HP_MIC,
	SPRD_CODEC_ADC_MIXER_MAX,
	SPRD_CODEC_HP_DACL = SPRD_CODEC_ADC_MIXER_MAX,
	SPRD_CODEC_HP_DACR,
	SPRD_CODEC_HP_ADCL,
	SPRD_CODEC_HP_ADCR,
	SPRD_CODEC_HP_MIXER_MAX,
	SPRD_CODEC_SPK_DACL = SPRD_CODEC_HP_MIXER_MAX,
	SPRD_CODEC_SPK_DACR,
	SPRD_CODEC_SPK_ADCL,
	SPRD_CODEC_SPK_ADCR,
	SPRD_CODEC_SPK_MIXER_MAX,
	SPRD_CODEC_EAR_DACL = SPRD_CODEC_SPK_MIXER_MAX,
	SPRD_CODEC_EAR_MIXER_MAX,

	SPRD_CODEC_MIXER_MAX = SPRD_CODEC_EAR_MIXER_MAX << SPRD_CODEC_RIGHT
};

const char *sprd_codec_mixer_debug_str[SPRD_CODEC_MIXER_MAX] = {
	"AIL->ADCL",
	"AIL->ADCR",
	"AIR->ADCL",
	"AIR->ADCR",
	"MAIN MIC->ADCL",
	"MAIN MIC->ADCR",
	"AUX MIC->ADCL",
	"AUX MIC->ADCR",
	"HP MIC->ADCL",
	"HP MIC->ADCR",
	"DACL->HPL",
	"DACL->HPR",
	"DACR->HPL",
	"DACR->HPR",
	"ADCL->HPL",
	"ADCL->HPR",
	"ADCR->HPL",
	"ADCR->HPR",
	"DACL->SPKL",
	"DACL->SPKR",
	"DACR->SPKL",
	"DACR->SPKR",
	"ADCL->SPKL",
	"ADCL->SPKR",
	"ADCR->SPKL",
	"ADCR->SPKR",
	"ADCL->EAR",
	"ADCR->EAR(bug)"
};

#define IS_SPRD_CODEC_MIXER_RANG(reg) (((reg) >= SPRD_CODEC_MIXER_START) && ((reg) <= SPRD_CODEC_MIXER_MAX))

typedef int (*sprd_codec_mixer_set) (struct snd_soc_codec * codec, int on);
struct sprd_codec_mixer {
	int on;
	sprd_codec_mixer_set set;
};

struct sprd_codec_inter_pa {
	/* FIXME little endian */
	int LDO_V_sel:4;
	int DTRI_F_sel:4;
	int is_DEMI_mode:1;
	int is_classD_mode:1;
	int is_LDO_mode:1;
	int is_auto_LDO_mode:1;
	int RESV:20;
};

struct sprd_codec_pa_setting {
	union {
		struct sprd_codec_inter_pa setting;
		u32 value;
	};
	int set;
};

static DEFINE_MUTEX(inter_pa_mutex);
static struct sprd_codec_pa_setting inter_pa;

enum {
	SPRD_CODEC_MIC_BIAS,
	SPRD_CODEC_AUXMIC_BIAS,
	SPRD_CODEC_MIC_BIAS_MAX
};

static const char *mic_bias_name[SPRD_CODEC_MIC_BIAS_MAX] = {
	"Mic Bias",
	"AuxMic Bias",
};

/* codec private data */
struct sprd_codec_priv {
	struct snd_soc_codec *codec;
	atomic_t power_refcount;
	atomic_t adie_dac_refcount;
	atomic_t adie_adc_refcount;
	int da_sample_val;
	int ad_sample_val;
	struct sprd_codec_mixer mixer[SPRD_CODEC_MIXER_MAX];
	struct sprd_codec_pga_op pga[SPRD_CODEC_PGA_MAX];
	int mic_bias[SPRD_CODEC_MIC_BIAS_MAX];
#ifdef CONFIG_SPRD_CODEC_USE_INT
	int ap_irq;
	struct completion completion_hp_pop;

	int dp_irq;
	struct completion completion_dac_mute;
#endif
};

/* codec local power suppliy */
static struct sprd_codec_power_suppliy {
	struct regulator_bulk_data supplies[SPRD_CODEC_NUM_SUPPLIES];
	struct delayed_work mic_delayed_work;
	struct delayed_work auxmic_delayed_work;
	atomic_t mic_on;
	atomic_t auxmic_on;
	atomic_t ldo_refcount;
} sprd_codec_power;

#define SPRD_CODEC_PA_SW_AOL (BIT(0))
#define SPRD_CODEC_PA_SW_EAR (BIT(1))
#define SPRD_CODEC_PA_SW_FUN (SPRD_CODEC_PA_SW_AOL | SPRD_CODEC_PA_SW_EAR)
static int sprd_codec_fun = 0;
static DEFINE_SPINLOCK(sprd_codec_fun_lock);

static void sprd_codec_set_fun(int fun)
{
	spin_lock(&sprd_codec_fun_lock);
	sprd_codec_fun |= fun;
	spin_unlock(&sprd_codec_fun_lock);
}

static void sprd_codec_clr_fun(int fun)
{
	spin_lock(&sprd_codec_fun_lock);
	sprd_codec_fun &= ~fun;
	spin_unlock(&sprd_codec_fun_lock);
}

static int sprd_codec_test_fun(int fun)
{
	int ret;
	spin_lock(&sprd_codec_fun_lock);
	ret = sprd_codec_fun & fun;
	spin_unlock(&sprd_codec_fun_lock);
	return ret;
}

static void sprd_codec_wait(u32 wait_time)
{
	if (wait_time)
		schedule_timeout_uninterruptible(msecs_to_jiffies(wait_time));
}

#if 0				/* sometime we will use this print function */
static unsigned int sprd_codec_read(struct snd_soc_codec *codec,
				    unsigned int reg);
static void sprd_codec_print_regs(struct snd_soc_codec *codec)
{
	int reg;
	pr_warn("sprd_codec register digital part\n");
	for (reg = SPRD_CODEC_DP_BASE; reg < SPRD_CODEC_DP_END; reg += 0x10) {
		pr_warn("0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			(reg - SPRD_CODEC_DP_BASE)
			, sprd_codec_read(codec, reg + 0x00)
			, sprd_codec_read(codec, reg + 0x04)
			, sprd_codec_read(codec, reg + 0x08)
			, sprd_codec_read(codec, reg + 0x0C)
		    );
	}
	pr_warn("sprd_codec register analog part\n");
	for (reg = SPRD_CODEC_AP_BASE; reg < SPRD_CODEC_AP_END; reg += 0x10) {
		pr_warn("0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			(reg - SPRD_CODEC_AP_BASE)
			, sprd_codec_read(codec, reg + 0x00)
			, sprd_codec_read(codec, reg + 0x04)
			, sprd_codec_read(codec, reg + 0x08)
			, sprd_codec_read(codec, reg + 0x0C)
		    );
	}
}
#endif

static int sprd_codec_pga_spk_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = DCGR3;
	val = (pgaval & 0xF) << 4;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0xF0, val);
}

static int sprd_codec_pga_spkr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = DCGR3;
	val = pgaval & 0xF;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0x0F, val);
}

static int sprd_codec_pga_hpl_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = DCGR1;
	val = (pgaval & 0xF) << 4;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0xF0, val);
}

static int sprd_codec_pga_hpr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = DCGR1;
	val = pgaval & 0xF;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0x0F, val);
}

static int sprd_codec_pga_ear_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = DCGR2;
	val = ((pgaval & 0xF) << 4);
	return snd_soc_update_bits(codec, SOC_REG(reg), 0xF0, val);
}

static int sprd_codec_pga_adcl_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = ACGR;
	val = (pgaval & 0xF) << 4;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0xF0, val);
}

static int sprd_codec_pga_adcr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val;
	reg = ACGR;
	val = pgaval & 0xF;
	return snd_soc_update_bits(codec, SOC_REG(reg), 0x0F, val);
}

static struct sprd_codec_pga sprd_codec_pga_cfg[SPRD_CODEC_PGA_MAX] = {
	{sprd_codec_pga_spk_set, 0},
	{sprd_codec_pga_spkr_set, 0},
	{sprd_codec_pga_hpl_set, 0},
	{sprd_codec_pga_hpr_set, 0},
	{sprd_codec_pga_ear_set, 0},

	{sprd_codec_pga_adcl_set, 0},
	{sprd_codec_pga_adcr_set, 0},
};

/* adc mixer */

static int ailadcl_set(struct snd_soc_codec *codec, int on)
{
	int ret;
	int need_on;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	ret = snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(AIL_ADCL),
				  on << AIL_ADCL);
	if (ret < 0)
		return ret;
	need_on = snd_soc_read(codec, AAICR3) & BIT(AIL_ADCL) & BIT(AIR_ADCL);
	return snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(VCM_ADCL),
				   (! !need_on) << VCM_ADCL);
}

static int ailadcr_set(struct snd_soc_codec *codec, int on)
{
	int ret;
	int need_on;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	ret = snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(AIL_ADCR),
				  on << AIL_ADCR);
	if (ret < 0)
		return ret;
	need_on = snd_soc_read(codec, AAICR3) & BIT(AIL_ADCR) & BIT(AIR_ADCR);
	return snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(VCM_ADCR),
				   (! !need_on) << VCM_ADCR);
}

static int airadcl_set(struct snd_soc_codec *codec, int on)
{
	int ret;
	int need_on;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	ret = snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(AIR_ADCL),
				  on << AIR_ADCL);
	if (ret < 0)
		return ret;
	need_on = snd_soc_read(codec, AAICR3) & (BIT(AIL_ADCL) | BIT(AIR_ADCL));
	return snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(VCM_ADCL),
				   (! !need_on) << VCM_ADCL);
}

static int airadcr_set(struct snd_soc_codec *codec, int on)
{
	int ret;
	int need_on;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	ret = snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(AIR_ADCR),
				  on << AIR_ADCR);
	if (ret < 0)
		return ret;
	need_on = snd_soc_read(codec, AAICR3) & (BIT(AIL_ADCR) | BIT(AIR_ADCR));
	return snd_soc_update_bits(codec, SOC_REG(AAICR3), BIT(VCM_ADCR),
				   (! !need_on) << VCM_ADCR);
}

static int mainmicadcl_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	mask = BIT(MICP_ADCL) | BIT(MICN_ADCL);
	return snd_soc_update_bits(codec, SOC_REG(AAICR1), mask, on ? mask : 0);
}

static int mainmicadcr_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	mask = BIT(MICP_ADCR) | BIT(MICN_ADCR);
	return snd_soc_update_bits(codec, SOC_REG(AAICR1), mask, on ? mask : 0);
}

static int auxmicadcl_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	mask = BIT(AUXMICP_ADCL) | BIT(AUXMICN_ADCL);
	return snd_soc_update_bits(codec, SOC_REG(AAICR2), mask, on ? mask : 0);
}

static int auxmicadcr_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	mask = BIT(AUXMICP_ADCR) | BIT(AUXMICN_ADCR);
	return snd_soc_update_bits(codec, SOC_REG(AAICR2), mask, on ? mask : 0);
}

static int hpmicadcl_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	mask = BIT(HPMICP_ADCL) | BIT(HPMICN_ADCL);
	return snd_soc_update_bits(codec, SOC_REG(AAICR1), mask, on ? mask : 0);
}

static int hpmicadcr_set(struct snd_soc_codec *codec, int on)
{
	int mask;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	mask = BIT(HPMICP_ADCR) | BIT(HPMICN_ADCR);
	return snd_soc_update_bits(codec, SOC_REG(AAICR1), mask, on ? mask : 0);
}

/* adcpga*_en setting */
static int adcpgax_auto_setting(struct snd_soc_codec *codec)
{
	unsigned int reg1, reg2, reg3;
	int val;
	int mask;
	reg1 = snd_soc_read(codec, DAOCR1);
	reg2 = snd_soc_read(codec, DAOCR2);
	reg3 = snd_soc_read(codec, DAOCR3);
	val = 0;
	if ((reg1 & BIT(ADCL_P_HPL)) || (reg2 & BIT(ADCL_P_AOLP))
	    || (reg3 & BIT(ADCL_P_AORP))) {
		val |= BIT(ADCPGAL_P_EN);
	}
	if ((reg1 & BIT(ADCL_N_HPR)) || (reg2 & BIT(ADCL_N_AOLN))
	    || (reg3 & BIT(ADCL_N_AORN))) {
		val |= BIT(ADCPGAL_N_EN);
	}
	if ((reg1 & (BIT(ADCR_P_HPL) | BIT(ADCR_P_HPR)))
	    || (reg2 & BIT(ADCR_P_AOLP)) || (reg3 & BIT(ADCR_P_AORP))) {
		val |= BIT(ADCPGAR_P_EN);
	}
	if ((reg2 & BIT(ADCR_N_AOLN)) || (reg3 & BIT(ADCR_N_AORN))) {
		val |= BIT(ADCPGAR_N_EN);
	}
	mask =
	    BIT(ADCPGAL_P_EN) | BIT(ADCPGAL_N_EN) | BIT(ADCPGAR_P_EN) |
	    BIT(ADCPGAR_N_EN);
	return snd_soc_update_bits(codec, SOC_REG(AACR2), mask, val);
}

/* hp mixer */

static int daclhpl_set(struct snd_soc_codec *codec, int on)
{
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(DACL_P_HPL),
				   on << DACL_P_HPL);
}

static int daclhpr_set(struct snd_soc_codec *codec, int on)
{
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(DACL_N_HPR),
				   on << DACL_N_HPR);
}

static int dacrhpl_set(struct snd_soc_codec *codec, int on)
{
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(DACR_P_HPL),
				   on << DACR_P_HPL);
}

static int dacrhpr_set(struct snd_soc_codec *codec, int on)
{
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(DACR_P_HPR),
				   on << DACR_P_HPR);
}

static int adclhpl_set(struct snd_soc_codec *codec, int on)
{
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(ADCL_P_HPL),
			    on << ADCL_P_HPL);
	return adcpgax_auto_setting(codec);
}

static int adclhpr_set(struct snd_soc_codec *codec, int on)
{
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(ADCL_N_HPR),
			    on << ADCL_N_HPR);
	return adcpgax_auto_setting(codec);
}

static int adcrhpl_set(struct snd_soc_codec *codec, int on)
{
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(ADCR_P_HPL),
			    on << ADCR_P_HPL);
	return adcpgax_auto_setting(codec);
}

static int adcrhpr_set(struct snd_soc_codec *codec, int on)
{
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR1), BIT(ADCR_P_HPR),
			    on << ADCR_P_HPR);
	return adcpgax_auto_setting(codec);
}

/* spkl mixer */

static int daclspkl_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(DACL_P_AOLP) | BIT(DACL_N_AOLN);
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR2), mask, on ? mask : 0);
}

static int dacrspkl_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(DACR_P_AOLP) | BIT(DACR_N_AOLN);
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR2), mask, on ? mask : 0);
}

static int adclspkl_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(ADCL_P_AOLP) | BIT(ADCL_N_AOLN);
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR2), mask, on ? mask : 0);
	return adcpgax_auto_setting(codec);
}

static int adcrspkl_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(ADCR_P_AOLP) | BIT(ADCR_N_AOLN);
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR2), mask, on ? mask : 0);
	return adcpgax_auto_setting(codec);
}

/* spkr mixer */

static int daclspkr_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(DACL_P_AORP) | BIT(DACL_N_AORN);
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR3), mask, on ? mask : 0);
}

static int dacrspkr_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(DACR_P_AORP) | BIT(DACR_N_AORN);
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR3), mask, on ? mask : 0);
}

static int adclspkr_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(ADCL_P_AORP) | BIT(ADCL_N_AORN);
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR3), mask, on ? mask : 0);
	return adcpgax_auto_setting(codec);
}

static int adcrspkr_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(ADCR_P_AORP) | BIT(ADCR_N_AORN);
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	snd_soc_update_bits(codec, SOC_REG(DAOCR3), mask, on ? mask : 0);
	return adcpgax_auto_setting(codec);
}

/* ear mixer */

static int daclear_set(struct snd_soc_codec *codec, int on)
{
	int mask = BIT(DACL_P_EARP) | BIT(DACL_N_EARN);
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(DAOCR4), mask, on ? mask : 0);
}

static sprd_codec_mixer_set mixer_setting[SPRD_CODEC_MIXER_MAX] = {
	/* adc mixer */
	ailadcl_set, ailadcr_set,
	airadcl_set, airadcr_set,
	mainmicadcl_set, mainmicadcr_set,
	auxmicadcl_set, auxmicadcr_set,
	hpmicadcl_set, hpmicadcr_set,
	/* hp mixer */
	daclhpl_set, daclhpr_set,
	dacrhpl_set, dacrhpr_set,
	adclhpl_set, adclhpr_set,
	adcrhpl_set, adcrhpr_set,
	/* spk mixer */
	daclspkl_set, daclspkr_set,
	dacrspkl_set, dacrspkr_set,
	adclspkl_set, adclspkr_set,
	adcrspkl_set, adcrspkr_set,
	/* ear mixer */
	daclear_set, 0,
};

/* DO NOT USE THIS FUNCTION */
static inline void __sprd_codec_pa_sw_en(int on)
{
	int mask;
	int val;
	mask = BIT(PA_SW_EN);
	val = on ? mask : 0;
	arch_audio_codec_write_mask(PMUR2, val, mask);
}

static DEFINE_SPINLOCK(sprd_codec_pa_sw_lock);
static inline void sprd_codec_pa_sw_set(int fun)
{
	sprd_codec_dbg("Entering %s fun 0x%08x\n", __func__, fun);
	spin_lock(&sprd_codec_pa_sw_lock);
	sprd_codec_set_fun(fun);
	__sprd_codec_pa_sw_en(1);
	spin_unlock(&sprd_codec_pa_sw_lock);
}

static inline void sprd_codec_pa_sw_clr(int fun)
{
	sprd_codec_dbg("Entering %s fun 0x%08x\n", __func__, fun);
	spin_lock(&sprd_codec_pa_sw_lock);
	sprd_codec_clr_fun(fun);
	if (!sprd_codec_test_fun(SPRD_CODEC_PA_SW_FUN))
		__sprd_codec_pa_sw_en(0);
	spin_unlock(&sprd_codec_pa_sw_lock);
}


/* inter PA */

static inline void sprd_codec_pa_d_en(int on)
{
	int mask;
	int val;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	mask = BIT(PA_D_EN);
	val = on ? mask : 0;
	arch_audio_codec_write_mask(DCR2, val, mask);
}

static inline void sprd_codec_pa_demi_en(int on)
{
	int mask;
	int val;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	mask = BIT(PA_DEMI_EN);
	val = on ? mask : 0;
	arch_audio_codec_write_mask(DCR2, val, mask);

	mask = BIT(DRV_OCP_AOL_PD) | BIT(DRV_OCP_AOR_PD);
	val = mask;
	arch_audio_codec_write_mask(DCR3, val, mask);
}

static inline void sprd_codec_pa_ldo_en(int on)
{
	int mask;
	int val;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	mask = BIT(PA_LDO_EN);
	val = on ? mask : 0;
	arch_audio_codec_write_mask(PMUR2, val, mask);
	if (on) {
		sprd_codec_pa_sw_clr(SPRD_CODEC_PA_SW_AOL);
	}
}

static inline void sprd_codec_pa_ldo_v_sel(int v_sel)
{
	int mask;
	int val;
	sprd_codec_dbg("Entering %s set %d\n", __func__, v_sel);
	mask = PA_LDO_V_MASK << PA_LDO_V;
	val = (v_sel << PA_LDO_V) & mask;
	arch_audio_codec_write_mask(PMUR4, val, mask);
}

static inline void sprd_codec_pa_dtri_f_sel(int f_sel)
{
	int mask;
	int val;
	sprd_codec_dbg("Entering %s set %d\n", __func__, f_sel);
	mask = PA_DTRI_F_MASK << PA_DTRI_F;
	val = (f_sel << PA_DTRI_F) & mask;
	arch_audio_codec_write_mask(DCR2, val, mask);
}

static inline void sprd_codec_pa_en(int on)
{
	int mask;
	int val;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	spin_lock(&sprd_codec_pa_sw_lock);
	if (on) {
		mask = BIT(PA_EN);
		val = mask;
	} else {
		if (!sprd_codec_test_fun(SPRD_CODEC_PA_SW_FUN))
			mask = BIT(PA_EN) | BIT(PA_SW_EN) | BIT(PA_LDO_EN);
		else
			mask = BIT(PA_EN) | BIT(PA_LDO_EN);
		val = 0;
	}
	arch_audio_codec_write_mask(PMUR2, val, mask);
	spin_unlock(&sprd_codec_pa_sw_lock);
}

static inline void sprd_codec_inter_pa_init(void)
{
	inter_pa.setting.LDO_V_sel = 0x03;
	inter_pa.setting.DTRI_F_sel = 0x01;
}

int sprd_inter_speaker_pa(int on)
{
	pr_info("inter PA switch %s\n", on ? "ON" : "OFF");
	mutex_lock(&inter_pa_mutex);
	if (on) {
		sprd_codec_pa_d_en(inter_pa.setting.is_classD_mode);
		sprd_codec_pa_d_en(inter_pa.setting.is_DEMI_mode);
		sprd_codec_pa_ldo_en(inter_pa.setting.is_LDO_mode);
		if (inter_pa.setting.is_LDO_mode
		    && !inter_pa.setting.is_auto_LDO_mode) {
			sprd_codec_pa_ldo_v_sel(inter_pa.setting.LDO_V_sel);
		}		/* FIXME auto adjust voltage */
		sprd_codec_pa_dtri_f_sel(inter_pa.setting.DTRI_F_sel);
		sprd_codec_pa_en(1);
		inter_pa.set = 1;
	} else {
		inter_pa.set = 0;
		sprd_codec_pa_en(0);
		sprd_codec_pa_ldo_en(0);
	}
	mutex_unlock(&inter_pa_mutex);
	return 0;
}

EXPORT_SYMBOL(sprd_inter_speaker_pa);

/* mic bias external */

static inline void sprd_codec_mic_bias_en(int on)
{
	int mask;
	int val;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	mask = BIT(MICBIAS_EN);
	val = on ? mask : 0;
	arch_audio_codec_write_mask(PMUR1, val, mask);
}

static inline void sprd_codec_auxmic_bias_en(int on)
{
	int mask;
	int val;
	sprd_codec_dbg("Entering %s set %d\n", __func__, on);
	mask = BIT(AUXMICBIAS_EN);
	val = on ? mask : 0;
	arch_audio_codec_write_mask(PMUR1, val, mask);
}

static int sprd_codec_set_sample_rate(struct snd_soc_codec *codec, int rate,
				      int mask, int shift)
{
	switch (rate) {
	case 8000:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_8000 << shift);
		break;
	case 11025:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_11025 << shift);
		break;
	case 16000:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_16000 << shift);
		break;
	case 22050:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_22050 << shift);
		break;
	case 32000:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_32000 << shift);
		break;
	case 44100:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_44100 << shift);
		break;
	case 48000:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_48000 << shift);
		break;
	case 96000:
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    SPRD_CODEC_RATE_96000 << shift);
		break;
	default:
		pr_err("sprd_codec not supports rate %d\n", rate);
		break;
	}
	return 0;
}

static int sprd_codec_set_ad_sample_rate(struct snd_soc_codec *codec, int rate,
					 int mask, int shift)
{
	int set;
	set = rate / 4000;
	if (set > 13) {
		pr_err("sprd_codec not supports ad rate %d\n", rate);
	}
	snd_soc_update_bits(codec, SOC_REG(AUD_ADC_CTL), mask, set << shift);
	return 0;
}

static int sprd_codec_sample_rate_setting(struct sprd_codec_priv *sprd_codec)
{
	if (sprd_codec->ad_sample_val) {
		sprd_codec_set_ad_sample_rate(sprd_codec->codec,
					      sprd_codec->ad_sample_val, 0x0F,
					      0);
	}
	if (sprd_codec->da_sample_val) {
		sprd_codec_set_sample_rate(sprd_codec->codec,
					   sprd_codec->da_sample_val, 0x0F, 0);
	}
	return 0;
}

static int sprd_codec_update_bits(struct snd_soc_codec *codec,
				  unsigned short reg, unsigned int mask,
				  unsigned int value)
{
	if (!codec) {
		int rreg = reg | SPRD_CODEC_AP_BASE_HI;
		return arch_audio_codec_write_mask(rreg, value, mask);
	} else {
		return snd_soc_update_bits(codec, reg, mask, value);
	}
}

static int sprd_codec_ldo_on(struct sprd_codec_priv *sprd_codec)
{
	int i;
	int ret;
	struct snd_soc_codec *codec = 0;
	sprd_codec_dbg("Entering %s\n", __func__);

	atomic_inc(&sprd_codec_power.ldo_refcount);
	if (atomic_read(&sprd_codec_power.ldo_refcount) == 1) {
		sprd_codec_dbg("ldo on!\n");
		if (sprd_codec) {
			codec = sprd_codec->codec;
		}
		arch_audio_codec_switch(AUDIO_TO_ARM_CTRL);
		arch_audio_codec_reg_enable();
		arch_audio_codec_enable();
		arch_audio_codec_reset();

		for (i = 0; i < ARRAY_SIZE(sprd_codec_power.supplies); i++)
			sprd_codec_power.supplies[i].supply =
			    sprd_codec_supply_names[i];

		ret =
		    regulator_bulk_get(NULL,
				       ARRAY_SIZE(sprd_codec_power.supplies),
				       sprd_codec_power.supplies);
		if (ret != 0) {
			pr_err("Failed to request supplies: %d\n", ret);
		} else {

			regulator_bulk_enable(ARRAY_SIZE
					      (sprd_codec_power.supplies),
					      sprd_codec_power.supplies);
			for (i = 0; i < ARRAY_SIZE(sprd_codec_power.supplies);
			     i++)
				regulator_set_mode(sprd_codec_power.supplies[i].
						   consumer,
						   REGULATOR_MODE_STANDBY);
		}

		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(BG_IBIAS_EN),
				       BIT(BG_IBIAS_EN));
		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(BG_EN),
				       BIT(BG_EN));
		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(VCM_EN),
				       BIT(VCM_EN));
		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(VCM_BUF_EN),
				       BIT(VCM_BUF_EN));
		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(VB_EN),
				       BIT(VB_EN));
		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(VBO_EN),
				       BIT(VBO_EN));

		if (sprd_codec) {
			sprd_codec_wait(SPRD_CODEC_LDO_WAIT_TIME);
		}
	}

	sprd_codec_dbg("Leaving %s\n", __func__);
	return 0;
}

static int sprd_codec_ldo_off(struct sprd_codec_priv *sprd_codec)
{
	int i;
	struct snd_soc_codec *codec = 0;
	sprd_codec_dbg("Entering %s\n", __func__);

	if (atomic_dec_and_test(&sprd_codec_power.ldo_refcount)) {
		if (sprd_codec) {
			codec = sprd_codec->codec;
		}
		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(VCM_EN), 0);
		sprd_codec_wait(SPRD_CODEC_LDO_VCM_TIME);
		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(VCM_BUF_EN),
				       0);
		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(VB_EN), 0);
		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(VBO_EN), 0);
		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(BG_IBIAS_EN),
				       0);
		sprd_codec_update_bits(codec, SOC_REG(PMUR1), BIT(BG_EN), 0);

		regulator_bulk_disable(ARRAY_SIZE(sprd_codec_power.supplies),
				       sprd_codec_power.supplies);
		for (i = 0; i < ARRAY_SIZE(sprd_codec_power.supplies); i++)
			regulator_set_mode(sprd_codec_power.supplies[i].
					   consumer, REGULATOR_MODE_NORMAL);

		regulator_bulk_free(ARRAY_SIZE(sprd_codec_power.supplies),
				    sprd_codec_power.supplies);

		arch_audio_codec_reset();
		arch_audio_codec_disable();
		arch_audio_codec_reg_disable();
		sprd_codec_dbg("ldo off!\n");
	}

	sprd_codec_dbg("Leaving %s\n", __func__);
	return 0;
}

static int sprd_codec_ldo_control(int on)
{
	if (on) {
		return sprd_codec_ldo_on(0);
	} else {
		return sprd_codec_ldo_off(0);
	}
}

static void sprd_codec_mic_delay_worker(struct work_struct *work)
{
	int on = atomic_read(&sprd_codec_power.mic_on) > 0;
	sprd_codec_ldo_control(on);
	sprd_codec_mic_bias_en(on);
}

static void sprd_codec_auxmic_delay_worker(struct work_struct *work)
{
	int on = atomic_read(&sprd_codec_power.auxmic_on) > 0;
	sprd_codec_ldo_control(on);
	sprd_codec_auxmic_bias_en(on);
}

static int sprd_codec_mic_bias_inter(int on, atomic_t * v)
{
	if (on) {
		atomic_inc(v);
		return 1;
	} else {
		if (atomic_read(v) > 0) {
			atomic_dec(v);
			return 1;
		}
	}
	return 0;
}

static void sprd_codec_init_delayed_work(struct delayed_work *delayed_work,
					 work_func_t func)
{
	if (!delayed_work->work.func) {
		INIT_DELAYED_WORK(delayed_work, func);
	}
}

int sprd_codec_mic_bias_control(int on)
{
	if (sprd_codec_mic_bias_inter(on, &sprd_codec_power.mic_on)) {
		sprd_codec_init_delayed_work(&sprd_codec_power.mic_delayed_work,
					     sprd_codec_mic_delay_worker);
		schedule_delayed_work(&sprd_codec_power.mic_delayed_work,
				      msecs_to_jiffies(1));
	}
	return 0;
}

EXPORT_SYMBOL(sprd_codec_mic_bias_control);

int sprd_codec_auxmic_bias_control(int on)
{
	if (sprd_codec_mic_bias_inter(on, &sprd_codec_power.auxmic_on)) {
		sprd_codec_init_delayed_work
		    (&sprd_codec_power.auxmic_delayed_work,
		     sprd_codec_auxmic_delay_worker);
		schedule_delayed_work(&sprd_codec_power.auxmic_delayed_work,
				      msecs_to_jiffies(1));
	}
	return 0;
}

EXPORT_SYMBOL(sprd_codec_auxmic_bias_control);

static int sprd_codec_open(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sprd_codec_dbg("Entering %s\n", __func__);

	sprd_codec_sample_rate_setting(sprd_codec);

	sprd_codec_dbg("Leaving %s\n", __func__);
	return ret;
}

static void sprd_codec_power_enable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret;

	atomic_inc(&sprd_codec->power_refcount);
	if (atomic_read(&sprd_codec->power_refcount) == 1) {
		sprd_codec_dbg("Entering %s\n", __func__);
		ret = sprd_codec_ldo_on(sprd_codec);
		if (ret != 0)
			pr_err("sprd_codec open ldo error %d\n", ret);
		sprd_codec_open(codec);
		sprd_codec_dbg("Leaving %s\n", __func__);
	}
}

static void sprd_codec_power_disable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	if (atomic_dec_and_test(&sprd_codec->power_refcount)) {
		sprd_codec_dbg("Entering %s\n", __func__);
		sprd_codec_ldo_off(sprd_codec);
		sprd_codec_dbg("Leaving %s\n", __func__);
	}
}

static const char *get_event_name(int event)
{
	const char *ev_name;
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ev_name = "PRE_PMU";
		break;
	case SND_SOC_DAPM_POST_PMU:
		ev_name = "POST_PMU";
		break;
	case SND_SOC_DAPM_PRE_PMD:
		ev_name = "PRE_PMD";
		break;
	case SND_SOC_DAPM_POST_PMD:
		ev_name = "POST_PMD";
		break;
	default:
		BUG();
		return 0;
	}
	return ev_name;
}

static int power_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int ret = 0;

	sprd_codec_dbg("Entering %s event is %s\n", __func__,
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sprd_codec_power_enable(codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		sprd_codec_power_disable(codec);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int adie_dac_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sprd_codec_dbg("Entering %s event is %s\n", __func__,
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		atomic_inc(&sprd_codec->adie_dac_refcount);
		if (atomic_read(&sprd_codec->adie_dac_refcount) == 1) {
			snd_soc_update_bits(codec, SOC_REG(AUDIF_ENB),
					    BIT(AUDIFA_DAC_EN),
					    BIT(AUDIFA_DAC_EN));
			pr_info("DAC ON\n");
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (atomic_dec_and_test(&sprd_codec->adie_dac_refcount)) {
			snd_soc_update_bits(codec, SOC_REG(AUDIF_ENB),
					    BIT(AUDIFA_DAC_EN), 0);
			pr_info("DAC OFF\n");
		}
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	sprd_codec_dbg("Leaving %s\n", __func__);

	return ret;
}

static int adie_adc_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sprd_codec_dbg("Entering %s event is %s\n", __func__,
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		atomic_inc(&sprd_codec->adie_adc_refcount);
		if (atomic_read(&sprd_codec->adie_adc_refcount) == 1) {
			snd_soc_update_bits(codec, SOC_REG(AUDIF_ENB),
					    BIT(AUDIFA_ADC_EN),
					    BIT(AUDIFA_ADC_EN));
			pr_info("ADC ON\n");
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (atomic_dec_and_test(&sprd_codec->adie_adc_refcount)) {
			snd_soc_update_bits(codec, SOC_REG(AUDIF_ENB),
					    BIT(AUDIFA_ADC_EN), 0);
			pr_info("ADC OFF\n");
		}
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	sprd_codec_dbg("Leaving %s\n", __func__);

	return ret;
}

static int _mixer_set_mixer(struct snd_soc_codec *codec, int id, int lr,
			    int try_on)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int reg = ID_FUN(id, lr);
	struct sprd_codec_mixer *mixer = &(sprd_codec->mixer[reg]);
	if (try_on) {
		mixer->set = mixer_setting[reg];
		return mixer->set(codec, mixer->on);
	} else {
		mixer_setting[reg] (codec, 0);
		mixer->set = 0;
	}
	return 0;
}

static inline int _mixer_setting(struct snd_soc_codec *codec, int start,
				 int end, int lr, int try_on)
{
	int id;
	for (id = start; id < end; id++) {
		_mixer_set_mixer(codec, id, lr, try_on);
	}
	return 0;
}

static inline int _mixer_setting_one(struct snd_soc_codec *codec, int id,
				     int try_on)
{
	int lr = id & 0x1;
	id >>= 1;
	return _mixer_setting(codec, id, id + 1, lr, try_on);
}

#ifdef CONFIG_SPRD_CODEC_USE_INT
#ifndef CONFIG_CODEC_NO_HP_POP
static void sprd_codec_hp_pop_irq_enable(struct snd_soc_codec *codec)
{
	int mask = BIT(AUDIO_POP_IRQ);
	snd_soc_update_bits(codec, SOC_REG(AUDIF_INT_CLR), mask, mask);
	snd_soc_update_bits(codec, SOC_REG(AUDIF_INT_EN), mask, mask);
}
#endif

static irqreturn_t sprd_codec_ap_irq(int irq, void *dev_id)
{
	int mask;
	struct sprd_codec_priv *sprd_codec = dev_id;
	struct snd_soc_codec *codec = sprd_codec->codec;
	mask = snd_soc_read(codec, AUDIF_INT_MASK);
	sprd_codec_dbg("hp pop irq mask = 0x%x\n", mask);
	if (BIT(AUDIO_POP_IRQ) & mask) {
		mask = BIT(AUDIO_POP_IRQ);
		snd_soc_update_bits(codec, SOC_REG(AUDIF_INT_EN), mask, 0);
		complete(&sprd_codec->completion_hp_pop);
	}
	return IRQ_HANDLED;
}
#endif

#ifndef CONFIG_CODEC_NO_HP_POP
static inline int is_hp_pop_compelet(struct snd_soc_codec *codec)
{
	int val;
	val = snd_soc_read(codec, IFR1);
	val = (val >> HP_POP_FLG) & HP_POP_FLG_MASK;
	sprd_codec_dbg("HP POP= 0x%x\n", val);
	return HP_POP_FLG_NEAR_CMP == val;
}

static inline int hp_pop_wait_for_compelet(struct snd_soc_codec *codec)
{
#ifdef CONFIG_SPRD_CODEC_USE_INT
	int i;
	int hp_pop_complete;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	hp_pop_complete = msecs_to_jiffies(SPRD_CODEC_HP_POP_TIMEOUT);
	for (i = 0; i < 2; i++) {
		sprd_codec_dbg("hp pop %d irq enable\n", i);
		sprd_codec_hp_pop_irq_enable(codec);
		init_completion(&sprd_codec->completion_hp_pop);
		hp_pop_complete =
		    wait_for_completion_timeout(&sprd_codec->completion_hp_pop,
						hp_pop_complete);
		sprd_codec_dbg("hp pop %d completion %d\n", i, hp_pop_complete);
		if (!hp_pop_complete) {
			if (!is_hp_pop_compelet(codec)) {
				pr_err("hp pop %d timeout not complete\n", i);
			} else {
				pr_err("hp pop %d timeout but complete\n", i);
			}
		} else {
			/* 01 change to 10 maybe walk to 11 shortly,
			   so, check it double. */
			sprd_codec_wait(2);
			if (is_hp_pop_compelet(codec)) {
				return 0;
			}
		}
	}
#else
	int times;
	for (times = 0; times < SPRD_CODEC_HP_POP_TIME_COUNT; times++) {
		if (is_hp_pop_compelet(codec)) {
			/* 01 change to 10 maybe walk to 11 shortly,
			   so, check it double. */
			sprd_codec_wait(2);
			if (is_hp_pop_compelet(codec)) {
				return 0;
			}
		}
		sprd_codec_wait(SPRD_CODEC_HP_POP_TIME_STEP);
	}
	pr_err("hp pop wait timeout: times = %d \n", times);
#endif
	return 0;
}

static int hp_pop_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int mask;
	int ret = 0;

	sprd_codec_dbg("Entering %s event is %s\n", __func__,
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mask = HP_POP_STEP_MASK << HP_POP_STEP;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_STEP_2 << HP_POP_STEP);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_UP << HP_POP_CTL);
		sprd_codec_dbg("U PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mask = HP_POP_STEP_MASK << HP_POP_STEP;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_STEP_1 << HP_POP_STEP);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_DOWN << HP_POP_CTL);
		sprd_codec_dbg("D PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
		break;
	case SND_SOC_DAPM_POST_PMU:
		ret = hp_pop_wait_for_compelet(codec);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_HOLD << HP_POP_CTL);
		sprd_codec_dbg("HOLD PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = hp_pop_wait_for_compelet(codec);
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_DIS << HP_POP_CTL);
		sprd_codec_dbg("DIS PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	sprd_codec_dbg("Leaving %s\n", __func__);

	return ret;
}
#else
#ifndef CONFIG_HP_POP_DELAY_TIME
#define CONFIG_HP_POP_DELAY_TIME (0)
#endif
static int hp_pop_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	sprd_codec_dbg("Entering %s event is %s wait %dms\n", __func__,
		       get_event_name(event), CONFIG_HP_POP_DELAY_TIME);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		sprd_codec_wait(CONFIG_HP_POP_DELAY_TIME);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}
	sprd_codec_dbg("Leaving %s\n", __func__);
	return ret;
}
#endif

static int hp_switch_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
#ifndef CONFIG_CODEC_NO_HP_POP
	int mask;
#endif
	int ret = 0;

	sprd_codec_dbg("Entering %s event is %s\n", __func__,
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
#if 0				/* do not enable the diff function from weifeng.ni */
		snd_soc_update_bits(codec, SOC_REG(DCR1), BIT(DIFF_EN),
				    BIT(DIFF_EN));
#endif

#ifndef CONFIG_CODEC_NO_HP_POP
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_DIS << HP_POP_CTL);
		sprd_codec_dbg("DIS(en) PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
#endif
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, SOC_REG(DCR1), BIT(DIFF_EN), 0);

#ifndef CONFIG_CODEC_NO_HP_POP
		mask = HP_POP_CTL_MASK << HP_POP_CTL;
		snd_soc_update_bits(codec, SOC_REG(PNRCR1), mask,
				    HP_POP_CTL_HOLD << HP_POP_CTL);
		sprd_codec_dbg("HOLD(en) PNRCR1 = 0x%x\n",
			       snd_soc_read(codec, PNRCR1));
#endif
		goto _pre_pmd;
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* do nothing now */
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	_mixer_setting(codec, SPRD_CODEC_HP_DACL,
		       SPRD_CODEC_HP_MIXER_MAX, SPRD_CODEC_LEFT,
		       snd_soc_read(codec, DCR1) & BIT(HPL_EN));

	_mixer_setting(codec, SPRD_CODEC_HP_DACL,
		       SPRD_CODEC_HP_MIXER_MAX, SPRD_CODEC_RIGHT,
		       snd_soc_read(codec, DCR1) & BIT(HPR_EN));

_pre_pmd:
	sprd_codec_dbg("Leaving %s\n", __func__);

	return ret;
}

static int spk_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	sprd_codec_dbg("Entering %s event is %s\n", __func__,
		       get_event_name(event));

	if (snd_soc_read(codec, DCR1) & BIT(AOL_EN)) {
		if (event == SND_SOC_DAPM_POST_PMU)
			sprd_codec_pa_sw_set(SPRD_CODEC_PA_SW_AOL);
		else
			sprd_codec_pa_sw_clr(SPRD_CODEC_PA_SW_AOL);

		_mixer_setting(codec, SPRD_CODEC_SPK_DACL,
			       SPRD_CODEC_SPK_MIXER_MAX, SPRD_CODEC_LEFT, 1);
	}

	_mixer_setting(codec, SPRD_CODEC_SPK_DACL,
		       SPRD_CODEC_SPK_MIXER_MAX, SPRD_CODEC_RIGHT,
		       (snd_soc_read(codec, DCR1) & BIT(AOR_EN)));

	sprd_codec_dbg("Leaving %s\n", __func__);

	return 0;
}

#ifdef CONFIG_SPRD_CODEC_EAR_WITH_IN_SPK
static int ear_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	sprd_codec_dbg("Entering %s event is %s\n", __func__,
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sprd_codec_pa_sw_set(SPRD_CODEC_PA_SW_EAR);
		break;
	case SND_SOC_DAPM_POST_PMD:
		sprd_codec_pa_sw_clr(SPRD_CODEC_PA_SW_EAR);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	sprd_codec_dbg("Leaving %s\n", __func__);

	return ret;
}
#endif

static int adc_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	sprd_codec_dbg("Entering %s event is %s\n", __func__,
		       get_event_name(event));

	_mixer_setting(codec, SPRD_CODEC_AIL, SPRD_CODEC_ADC_MIXER_MAX,
		       SPRD_CODEC_LEFT,
		       (!(snd_soc_read(codec, AACR2) & BIT(ADCPGAL_PD))));

	_mixer_setting(codec, SPRD_CODEC_AIL, SPRD_CODEC_ADC_MIXER_MAX,
		       SPRD_CODEC_RIGHT,
		       (!(snd_soc_read(codec, AACR2) & BIT(ADCPGAR_PD))));

	sprd_codec_dbg("Leaving %s\n", __func__);

	return 0;
}

static int pga_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = FUN_REG(w->reg);
	struct sprd_codec_pga_op *pga = &(sprd_codec->pga[id]);
	int ret = 0;
	int min = sprd_codec_pga_cfg[id].min;

	sprd_codec_dbg("Entering %s set %s(%d) event is %s\n", __func__,
		       sprd_codec_pga_debug_str[id], pga->pgaval,
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		pga->set = sprd_codec_pga_cfg[id].set;
		ret = pga->set(codec, pga->pgaval);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		pga->set = 0;
		ret = sprd_codec_pga_cfg[id].set(codec, min);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	sprd_codec_dbg("Leaving %s\n", __func__);

	return ret;
}

static int mic_bias_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	int id = FUN_REG(w->reg);
	int ret = 0;

	sprd_codec_dbg("Entering %s %s event is %s\n", __func__,
		       mic_bias_name[id], get_event_name(event));

	switch (id) {
	case SPRD_CODEC_MIC_BIAS:
		if (!(atomic_read(&sprd_codec_power.mic_on) > 0)) {
			sprd_codec_mic_bias_en(SND_SOC_DAPM_EVENT_ON(event));
		}
		break;
	case SPRD_CODEC_AUXMIC_BIAS:
		if (!(atomic_read(&sprd_codec_power.auxmic_on) > 0)) {
			sprd_codec_auxmic_bias_en(SND_SOC_DAPM_EVENT_ON(event));
		}
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	sprd_codec_dbg("Leaving %s\n", __func__);

	return ret;
}

static int mixer_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = FUN_REG(w->reg);
	struct sprd_codec_mixer *mixer = &(sprd_codec->mixer[id]);
	int ret = 0;

	pr_info("%s event is %s\n", sprd_codec_mixer_debug_str[id],
		get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mixer->on = 1;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mixer->on = 0;
		break;
	default:
		BUG();
		ret = -EINVAL;
	}
	if (ret >= 0)
		_mixer_setting_one(codec, id, mixer->on);

	sprd_codec_dbg("Leaving %s\n", __func__);

	return ret;
}

static int mixer_get(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = wlist->widgets[0]->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = FUN_REG(mc->reg);
	ucontrol->value.integer.value[0] = sprd_codec->mixer[id].on;
	return 0;
}

static int mixer_set(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = wlist->widgets[0]->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = FUN_REG(mc->reg);
	struct sprd_codec_mixer *mixer = &(sprd_codec->mixer[id]);
	int ret = 0;

	pr_info("set %s switch %s\n", sprd_codec_mixer_debug_str[id],
		ucontrol->value.integer.value[0] ? "ON" : "OFF");

	if (mixer->on == ucontrol->value.integer.value[0])
		return 0;

	snd_soc_dapm_put_volsw(kcontrol, ucontrol);

	mixer->on = ucontrol->value.integer.value[0];

	if (mixer->set)
		ret = mixer->set(codec, mixer->on);

	sprd_codec_dbg("Leaving %s\n", __func__);

	return ret;
}

#define SPRD_CODEC_MIXER(xname, xreg)\
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, mixer_get, mixer_set)

/* ADCL Mixer */
static const struct snd_kcontrol_new adcl_mixer_controls[] = {
	SPRD_CODEC_MIXER("AILADCL Switch",
			 ID_FUN(SPRD_CODEC_AIL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("AIRADCL Switch",
			 ID_FUN(SPRD_CODEC_AIR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("MainMICADCL Switch",
			 ID_FUN(SPRD_CODEC_MAIN_MIC, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("AuxMICADCL Switch",
			 ID_FUN(SPRD_CODEC_AUX_MIC, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("HPMICADCL Switch",
			 ID_FUN(SPRD_CODEC_HP_MIC, SPRD_CODEC_LEFT)),
};

/* ADCR Mixer */
static const struct snd_kcontrol_new adcr_mixer_controls[] = {
	SPRD_CODEC_MIXER("AILADCR Switch",
			 ID_FUN(SPRD_CODEC_AIL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("AIRADCR Switch",
			 ID_FUN(SPRD_CODEC_AIR, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("MainMICADCR Switch",
			 ID_FUN(SPRD_CODEC_MAIN_MIC, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("AuxMICADCR Switch",
			 ID_FUN(SPRD_CODEC_AUX_MIC, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("HPMICADCR Switch",
			 ID_FUN(SPRD_CODEC_HP_MIC, SPRD_CODEC_RIGHT)),
};

/* HPL Mixer */
static const struct snd_kcontrol_new hpl_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLHPL Switch",
			 ID_FUN(SPRD_CODEC_HP_DACL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("DACRHPL Switch",
			 ID_FUN(SPRD_CODEC_HP_DACR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("ADCLHPL Switch",
			 ID_FUN(SPRD_CODEC_HP_ADCL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("ADCRHPL Switch",
			 ID_FUN(SPRD_CODEC_HP_ADCR, SPRD_CODEC_LEFT)),
};

/* HPR Mixer */
static const struct snd_kcontrol_new hpr_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLHPR Switch",
			 ID_FUN(SPRD_CODEC_HP_DACL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("DACRHPR Switch",
			 ID_FUN(SPRD_CODEC_HP_DACR, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("ADCLHPR Switch",
			 ID_FUN(SPRD_CODEC_HP_ADCL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("ADCRHPR Switch",
			 ID_FUN(SPRD_CODEC_HP_ADCR, SPRD_CODEC_RIGHT)),
};

/* SPKL Mixer */
static const struct snd_kcontrol_new spkl_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLSPKL Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("DACRSPKL Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("ADCLSPKL Switch",
			 ID_FUN(SPRD_CODEC_SPK_ADCL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("ADCRSPKL Switch",
			 ID_FUN(SPRD_CODEC_SPK_ADCR, SPRD_CODEC_LEFT)),
};

/* SPKR Mixer */
static const struct snd_kcontrol_new spkr_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLSPKR Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("DACRSPKR Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACR, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("ADCLSPKR Switch",
			 ID_FUN(SPRD_CODEC_SPK_ADCL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("ADCRSPKR Switch",
			 ID_FUN(SPRD_CODEC_SPK_ADCR, SPRD_CODEC_RIGHT)),
};

static const struct snd_soc_dapm_widget sprd_codec_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY("Power", SND_SOC_NOPM, 0, 0, power_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("DA Clk", 1, SOC_REG(CCR), DAC_CLK_EN, 0, NULL,
			      0),
	SND_SOC_DAPM_SUPPLY_S("DRV Clk", 2, SOC_REG(CCR), DRV_CLK_EN, 0, NULL,
			      0),
	SND_SOC_DAPM_SUPPLY_S("AD IBUF", 1, SOC_REG(AACR1), ADC_IBUF_PD, 1,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AD Clk", 2, SOC_REG(CCR), ADC_CLK_PD, 1, NULL,
			      0),

	SND_SOC_DAPM_PGA_S("Digital DACL Switch", 4, SOC_REG(AUD_TOP_CTL),
			   DAC_EN_L, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Digital DACR Switch", 4, SOC_REG(AUD_TOP_CTL),
			   DAC_EN_R, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital DACL Switch", 5, SND_SOC_NOPM, 0, 0,
			   adie_dac_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADie Digital DACR Switch", 5, SND_SOC_NOPM, 0, 0,
			   adie_dac_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("DACL Switch", 6, SOC_REG(DACR), DACL_EN, 0, NULL,
			   0),
	SND_SOC_DAPM_PGA_S("DACR Switch", 6, SOC_REG(DACR), DACR_EN, 0, NULL,
			   0),
	SND_SOC_DAPM_DAC_E("DAC", "Playback", SND_SOC_NOPM, 0, 0, NULL, 0),
#ifdef CONFIG_CODEC_NO_HP_POP
	SND_SOC_DAPM_PGA_S("HP POP", 6, SND_SOC_NOPM, 0, 0, hp_pop_event,
			   SND_SOC_DAPM_POST_PMU),
#else
	SND_SOC_DAPM_SUPPLY_S("HP POP", 3, SND_SOC_NOPM, 0, 0, hp_pop_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD |
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
#endif
	SND_SOC_DAPM_PGA_S("HPL Switch", 5, SOC_REG(DCR1), HPL_EN, 0,
			   hp_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD |
			   SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPR Switch", 5, SOC_REG(DCR1), HPR_EN, 0,
			   hp_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD |
			   SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPL Mute", 6, FUN_REG(SPRD_CODEC_PGA_HPL), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPR Mute", 6, FUN_REG(SPRD_CODEC_PGA_HPR), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER("HPL Mixer", SND_SOC_NOPM, 0, 0,
			   &hpl_mixer_controls[0],
			   ARRAY_SIZE(hpl_mixer_controls)),
	SND_SOC_DAPM_MIXER("HPR Mixer", SND_SOC_NOPM, 0, 0,
			   &hpr_mixer_controls[0],
			   ARRAY_SIZE(hpr_mixer_controls)),
	SND_SOC_DAPM_MIXER("SPKL Mixer", SND_SOC_NOPM, 0, 0,
			   &spkl_mixer_controls[0],
			   ARRAY_SIZE(spkl_mixer_controls)),
	SND_SOC_DAPM_MIXER("SPKR Mixer", SND_SOC_NOPM, 0, 0,
			   &spkr_mixer_controls[0],
			   ARRAY_SIZE(spkr_mixer_controls)),
	SND_SOC_DAPM_PGA_S("SPKL Switch", 5, SOC_REG(DCR1), AOL_EN, 0,
			   spk_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("SPKR Switch", 5, SOC_REG(DCR1), AOR_EN, 0,
			   spk_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("SPKL Mute", 6, FUN_REG(SPRD_CODEC_PGA_SPKL), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("SPKR Mute", 6, FUN_REG(SPRD_CODEC_PGA_SPKR), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("EAR Mixer", 5,
			   FUN_REG(ID_FUN
				   (SPRD_CODEC_EAR_DACL, SPRD_CODEC_LEFT)), 0,
			   0, mixer_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
#ifdef CONFIG_SPRD_CODEC_EAR_WITH_IN_SPK
	SND_SOC_DAPM_PGA_S("EAR Switch", 6, SOC_REG(DCR1), EAR_EN, 0,
			   ear_switch_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
#else
	SND_SOC_DAPM_PGA_S("EAR Switch", 6, SOC_REG(DCR1), EAR_EN, 0, NULL, 0),
#endif

	SND_SOC_DAPM_PGA_S("EAR Mute", 7, FUN_REG(SPRD_CODEC_PGA_EAR), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MIXER("ADCL Mixer", SND_SOC_NOPM, 0, 0,
			   &adcl_mixer_controls[0],
			   ARRAY_SIZE(adcl_mixer_controls)),
	SND_SOC_DAPM_MIXER("ADCR Mixer", SND_SOC_NOPM, 0, 0,
			   &adcr_mixer_controls[0],
			   ARRAY_SIZE(adcr_mixer_controls)),
	SND_SOC_DAPM_PGA_S("Digital ADCL Switch", 3, SOC_REG(AUD_TOP_CTL),
			   ADC_EN_L, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Digital ADCR Switch", 3, SOC_REG(AUD_TOP_CTL),
			   ADC_EN_R, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital ADCL Switch", 2, SND_SOC_NOPM, 0, 0,
			   adie_adc_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADie Digital ADCR Switch", 2, SND_SOC_NOPM, 0, 0,
			   adie_adc_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADCL Switch", 1, SOC_REG(AACR1), ADCL_PD, 1, NULL,
			   0),
	SND_SOC_DAPM_PGA_S("ADCR Switch", 1, SOC_REG(AACR1), ADCR_PD, 1, NULL,
			   0),
	SND_SOC_DAPM_PGA_E("ADCL PGA", SOC_REG(AACR2), ADCPGAL_PD, 1, NULL, 0,
			   adc_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ADCR PGA", SOC_REG(AACR2), ADCPGAR_PD, 1, NULL, 0,
			   adc_switch_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADCL Mute", 3, FUN_REG(SPRD_CODEC_PGA_ADCL), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("ADCR Mute", 3, FUN_REG(SPRD_CODEC_PGA_ADCR), 0, 0,
			   pga_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_ADC_E("ADC", "Capture", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MICBIAS_E("Mic Bias", FUN_REG(SPRD_CODEC_MIC_BIAS), 0, 0,
			       mic_bias_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MICBIAS_E("AuxMic Bias", FUN_REG(SPRD_CODEC_AUXMIC_BIAS),
			       0, 0,
			       mic_bias_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUTPUT("HEAD_P_L"),
	SND_SOC_DAPM_OUTPUT("HEAD_P_R"),
	SND_SOC_DAPM_OUTPUT("AOL"),
	SND_SOC_DAPM_OUTPUT("SPKL"),
	SND_SOC_DAPM_OUTPUT("AOR"),
	SND_SOC_DAPM_OUTPUT("EAR"),

	SND_SOC_DAPM_INPUT("MIC"),
	SND_SOC_DAPM_INPUT("AUXMIC"),
	SND_SOC_DAPM_INPUT("HPMIC"),
	SND_SOC_DAPM_INPUT("AIL"),
	SND_SOC_DAPM_INPUT("AIR"),
};

/* sprd_codec supported interconnection*/
static const struct snd_soc_dapm_route sprd_codec_intercon[] = {
	/* Power */
	{"DA Clk", NULL, "Power"},
	{"DAC", NULL, "DA Clk"},

	{"AD IBUF", NULL, "Power"},
	{"AD Clk", NULL, "AD IBUF"},
	{"ADC", NULL, "AD Clk"},

	{"ADCL PGA", NULL, "AD Clk"},
	{"ADCR PGA", NULL, "AD Clk"},

	{"HP POP", NULL, "DRV Clk"},
	{"HPL Switch", NULL, "DRV Clk"},
	{"HPR Switch", NULL, "DRV Clk"},
	{"SPKL Switch", NULL, "DRV Clk"},
	{"SPKR Switch", NULL, "DRV Clk"},
	{"EAR Switch", NULL, "DRV Clk"},

	/* Playback */
	{"Digital DACL Switch", NULL, "DAC"},
	{"Digital DACR Switch", NULL, "DAC"},
	{"ADie Digital DACL Switch", NULL, "Digital DACL Switch"},
	{"ADie Digital DACR Switch", NULL, "Digital DACR Switch"},
	{"DACL Switch", NULL, "ADie Digital DACL Switch"},
	{"DACR Switch", NULL, "ADie Digital DACR Switch"},

	/* Output */
	{"HPL Mixer", "DACLHPL Switch", "DACL Switch"},
	{"HPL Mixer", "DACRHPL Switch", "DACR Switch"},
	{"HPR Mixer", "DACLHPR Switch", "DACL Switch"},
	{"HPR Mixer", "DACRHPR Switch", "DACR Switch"},

	{"HPL Mixer", "ADCLHPL Switch", "ADCL PGA"},
	{"HPL Mixer", "ADCRHPL Switch", "ADCR PGA"},
	{"HPR Mixer", "ADCLHPR Switch", "ADCL PGA"},
	{"HPR Mixer", "ADCRHPR Switch", "ADCR PGA"},

#ifdef CONFIG_CODEC_NO_HP_POP
	{"HEAD_P_L", NULL, "HP POP"},
	{"HEAD_P_R", NULL, "HP POP"},
	{"HP POP", NULL, "HPL Switch"},
	{"HP POP", NULL, "HPR Switch"},
#else
	{"HPL Switch", NULL, "HP POP"},
	{"HPR Switch", NULL, "HP POP"},
#endif
	{"HPL Switch", NULL, "HPL Mixer"},
	{"HPR Switch", NULL, "HPR Mixer"},
	{"HPL Mute", NULL, "HPL Switch"},
	{"HPR Mute", NULL, "HPR Switch"},
	{"HEAD_P_L", NULL, "HPL Mute"},
	{"HEAD_P_R", NULL, "HPR Mute"},

	{"SPKL Mixer", "DACLSPKL Switch", "DACL Switch"},
	{"SPKL Mixer", "DACRSPKL Switch", "DACR Switch"},
	{"SPKR Mixer", "DACLSPKR Switch", "DACL Switch"},
	{"SPKR Mixer", "DACRSPKR Switch", "DACR Switch"},

	{"SPKL Mixer", "ADCLSPKL Switch", "ADCL PGA"},
	{"SPKL Mixer", "ADCRSPKL Switch", "ADCR PGA"},
	{"SPKR Mixer", "ADCLSPKR Switch", "ADCL PGA"},
	{"SPKR Mixer", "ADCRSPKR Switch", "ADCR PGA"},

	{"SPKL Switch", NULL, "SPKL Mixer"},
	{"SPKR Switch", NULL, "SPKR Mixer"},
	{"SPKL Mute", NULL, "SPKL Switch"},
	{"SPKR Mute", NULL, "SPKR Switch"},
	{"AOL", NULL, "SPKL Mute"},
	{"AOR", NULL, "SPKR Mute"},

	{"EAR Mixer", NULL, "DACL Switch"},
	{"EAR Switch", NULL, "EAR Mixer"},
	{"EAR Mute", NULL, "EAR Switch"},
	{"EAR", NULL, "EAR Mute"},

	{"ADCL Mute", NULL, "ADCL Mixer"},
	{"ADCR Mute", NULL, "ADCR Mixer"},
	{"ADCL PGA", NULL, "ADCL Mute"},
	{"ADCR PGA", NULL, "ADCR Mute"},
	/* LineIN */
	{"ADCL Mixer", "AILADCL Switch", "AIL"},
	{"ADCR Mixer", "AILADCR Switch", "AIL"},
	{"ADCL Mixer", "AIRADCL Switch", "AIR"},
	{"ADCR Mixer", "AIRADCR Switch", "AIR"},
	/* Input */
	{"ADCL Mixer", "MainMICADCL Switch", "Mic Bias"},
	{"ADCR Mixer", "MainMICADCR Switch", "Mic Bias"},
	{"ADCL Mixer", "AuxMICADCL Switch", "AuxMic Bias"},
	{"ADCR Mixer", "AuxMICADCR Switch", "AuxMic Bias"},
	{"ADCL Mixer", "HPMICADCL Switch", "HPMIC"},
	{"ADCR Mixer", "HPMICADCR Switch", "HPMIC"},
	/* Captrue */
	{"ADCL Switch", NULL, "ADCL PGA"},
	{"ADCR Switch", NULL, "ADCR PGA"},
	{"ADie Digital ADCL Switch", NULL, "ADCL Switch"},
	{"ADie Digital ADCR Switch", NULL, "ADCR Switch"},
	{"Digital ADCL Switch", NULL, "ADie Digital ADCL Switch"},
	{"Digital ADCR Switch", NULL, "ADie Digital ADCR Switch"},
	{"ADC", NULL, "Digital ADCL Switch"},
	{"ADC", NULL, "Digital ADCR Switch"},
	{"Mic Bias", NULL, "MIC"},
	{"AuxMic Bias", NULL, "AUXMIC"},
	/* Bias independent */
	{"Mic Bias", NULL, "Power"},
	{"AuxMic Bias", NULL, "Power"},
};

static int sprd_codec_vol_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	struct sprd_codec_pga_op *pga = &(sprd_codec->pga[reg]);
	int ret = 0;

	pr_info("set PGA[%s] to %ld\n", sprd_codec_pga_debug_str[reg],
		ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	pga->pgaval = val;
	if (pga->set) {
		ret = pga->set(codec, pga->pgaval);
	}
	sprd_codec_dbg("Leaving %s\n", __func__);
	return ret;
}

static int sprd_codec_vol_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int invert = mc->invert;
	struct sprd_codec_pga_op *pga = &(sprd_codec->pga[reg]);

	ucontrol->value.integer.value[0] = pga->pgaval;
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static int sprd_codec_inter_pa_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	int ret = 0;

	pr_info("config inter PA 0x%08x\n", (int)ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	mutex_lock(&inter_pa_mutex);
	inter_pa.value = (u32) val;
	if (inter_pa.set) {
		mutex_unlock(&inter_pa_mutex);
		sprd_inter_speaker_pa(1);
	} else {
		mutex_unlock(&inter_pa_mutex);
	}
	sprd_codec_dbg("Leaving %s\n", __func__);
	return ret;
}

static int sprd_codec_inter_pa_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int max = mc->max;
	unsigned int invert = mc->invert;

	mutex_lock(&inter_pa_mutex);
	ucontrol->value.integer.value[0] = inter_pa.value;
	mutex_unlock(&inter_pa_mutex);
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static int sprd_codec_mic_bias_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	int ret = 0;
	int id = reg;

	val = (ucontrol->value.integer.value[0] & mask);

	pr_info("%s switch %s\n", mic_bias_name[id], val ? "ON" : "OFF");

	if (invert)
		val = max - val;
	if (sprd_codec->mic_bias[id] == val) {
		return 0;
	}

	sprd_codec->mic_bias[id] = val;
	if (val) {
		ret =
		    snd_soc_dapm_force_enable_pin(&codec->card->dapm,
						  mic_bias_name[id]);
	} else {
		ret =
		    snd_soc_dapm_disable_pin(&codec->card->dapm,
					     mic_bias_name[id]);
	}

	/* signal a DAPM event */
	snd_soc_dapm_sync(&codec->card->dapm);

	sprd_codec_dbg("Leaving %s\n", __func__);
	return ret;
}

static int sprd_codec_mic_bias_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int invert = mc->invert;
	int id = reg;

	ucontrol->value.integer.value[0] = sprd_codec->mic_bias[id];
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static const DECLARE_TLV_DB_SCALE(adc_tlv, -600, 300, 0);
static const DECLARE_TLV_DB_SCALE(hp_tlv, -3600, 300, 1);
static const DECLARE_TLV_DB_SCALE(ear_tlv, -3600, 300, 1);
static const DECLARE_TLV_DB_SCALE(spk_tlv, -2400, 300, 1);

#define SPRD_CODEC_PGA(xname, xreg, tlv_array) \
	SOC_SINGLE_EXT_TLV(xname, FUN_REG(xreg), 0, 15, 0, \
			sprd_codec_vol_get, sprd_codec_vol_put, tlv_array)

#define SPRD_CODEC_MIC_BIAS(xname, xreg) \
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, \
			sprd_codec_mic_bias_get, sprd_codec_mic_bias_put)

static const struct snd_kcontrol_new sprd_codec_snd_controls[] = {
	SPRD_CODEC_PGA("SPKL Playback Volume", SPRD_CODEC_PGA_SPKL, spk_tlv),
	SPRD_CODEC_PGA("SPKR Playback Volume", SPRD_CODEC_PGA_SPKR, spk_tlv),
	SPRD_CODEC_PGA("HPL Playback Volume", SPRD_CODEC_PGA_HPL, hp_tlv),
	SPRD_CODEC_PGA("HPR Playback Volume", SPRD_CODEC_PGA_HPR, hp_tlv),
	SPRD_CODEC_PGA("EAR Playback Volume", SPRD_CODEC_PGA_EAR, ear_tlv),

	SPRD_CODEC_PGA("ADCL Capture Volume", SPRD_CODEC_PGA_ADCL, adc_tlv),
	SPRD_CODEC_PGA("ADCR Capture Volume", SPRD_CODEC_PGA_ADCR, adc_tlv),

	SOC_SINGLE_EXT("Inter PA Config", 0, 0, LONG_MAX, 0,
		       sprd_codec_inter_pa_get, sprd_codec_inter_pa_put),

	SPRD_CODEC_MIC_BIAS("MIC Bias Switch", SPRD_CODEC_MIC_BIAS),

	SPRD_CODEC_MIC_BIAS("AUXMIC Bias Switch", SPRD_CODEC_AUXMIC_BIAS),
};

static unsigned int sprd_codec_read(struct snd_soc_codec *codec,
				    unsigned int reg)
{
	/* Because snd_soc_update_bits reg is 16 bits short type,
	   so muse do following convert
	 */
	if (IS_SPRD_CODEC_AP_RANG(reg | SPRD_CODEC_AP_BASE_HI)) {
		reg |= SPRD_CODEC_AP_BASE_HI;
		return arch_audio_codec_read(reg);
	} else if (IS_SPRD_CODEC_DP_RANG(reg | SPRD_CODEC_DP_BASE_HI)) {
		reg |= SPRD_CODEC_DP_BASE_HI;
		return __raw_readl(reg);
	} else if (IS_SPRD_CODEC_MIXER_RANG(FUN_REG(reg))) {
		struct sprd_codec_priv *sprd_codec =
		    snd_soc_codec_get_drvdata(codec);
		int id = FUN_REG(reg);
		struct sprd_codec_mixer *mixer = &(sprd_codec->mixer[id]);
		return mixer->on;
	}
	sprd_codec_dbg("read the register is not codec's reg = 0x%x\n", reg);
	return 0;
}

static int sprd_codec_write(struct snd_soc_codec *codec, unsigned int reg,
			    unsigned int val)
{
	if (IS_SPRD_CODEC_AP_RANG(reg | SPRD_CODEC_AP_BASE_HI)) {
		reg |= SPRD_CODEC_AP_BASE_HI;
		return arch_audio_codec_write(reg, val);
	} else if (IS_SPRD_CODEC_DP_RANG(reg | SPRD_CODEC_DP_BASE_HI)) {
		reg |= SPRD_CODEC_DP_BASE_HI;
		return __raw_writel(val, reg);
	}
	sprd_codec_dbg("write the register is not codec's reg = 0x%x\n", reg);
	return 0;
}

static int sprd_codec_pcm_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_soc_dapm_force_enable_pin(&codec->card->dapm, "DAC");
	} else {
		snd_soc_dapm_force_enable_pin(&codec->card->dapm, "ADC");
	}
	return 0;
}

static void sprd_codec_pcm_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_soc_dapm_disable_pin(&codec->card->dapm, "DAC");
	} else {
		snd_soc_dapm_disable_pin(&codec->card->dapm, "ADC");
	}
}

static int sprd_codec_pcm_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int mask = 0x0F;
	int shift = 0;
	int rate = params_rate(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sprd_codec->da_sample_val = rate;
		pr_info("playback rate is [%d]\n", rate);
		sprd_codec_set_sample_rate(codec, rate, mask, shift);
	} else {
		sprd_codec->ad_sample_val = rate;
		pr_info("capture rate is [%d]\n", rate);
		sprd_codec_set_ad_sample_rate(codec, rate, mask, shift);
	}

	return 0;
}

static int sprd_codec_pcm_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	sprd_codec_dbg("Entering %s\n", __func__);
	sprd_codec_dbg("Leaving %s\n", __func__);
	return 0;
}

#ifdef CONFIG_SPRD_CODEC_USE_INT
#ifdef CONFIG_CODEC_DAC_MUTE_WAIT
static void sprd_codec_dac_mute_irq_enable(struct snd_soc_codec *codec)
{
	int mask = BIT(DAC_MUTE_D);
	snd_soc_update_bits(codec, SOC_REG(AUD_INT_CLR), mask, mask);
	snd_soc_update_bits(codec, SOC_REG(AUD_INT_EN), mask, mask);
}
#endif

static irqreturn_t sprd_codec_dp_irq(int irq, void *dev_id)
{
	int mask;
	struct sprd_codec_priv *sprd_codec = dev_id;
	struct snd_soc_codec *codec = sprd_codec->codec;
	mask = snd_soc_read(codec, AUD_AUD_STS0);
	sprd_codec_dbg("dac mute irq mask = 0x%x\n", mask);
	if (BIT(DAC_MUTE_D_MASK) & mask) {
		mask = BIT(DAC_MUTE_D);
		complete(&sprd_codec->completion_dac_mute);
	}
	if (BIT(DAC_MUTE_U_MASK) & mask) {
		mask = BIT(DAC_MUTE_U);
	}
	snd_soc_update_bits(codec, SOC_REG(AUD_INT_EN), mask, 0);
	return IRQ_HANDLED;
}
#endif

static int sprd_codec_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sprd_codec_dbg("Entering %s\n", __func__);

	if (atomic_read(&sprd_codec->power_refcount) >= 1) {
		sprd_codec_dbg("mute %i\n", mute);

		ret =
		    snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL),
					BIT(DAC_MUTE_START),
					mute ? BIT(DAC_MUTE_START) : 0);
#ifdef CONFIG_CODEC_DAC_MUTE_WAIT
#ifdef CONFIG_SPRD_CODEC_USE_INT
		if (mute && ret) {
			int dac_mute_complete;
			struct sprd_codec_priv *sprd_codec =
			    snd_soc_codec_get_drvdata(codec);
			sprd_codec_dbg("dac mute irq enable\n");
			sprd_codec_dac_mute_irq_enable(codec);
			init_completion(&sprd_codec->completion_dac_mute);
			dac_mute_complete =
			    wait_for_completion_timeout
			    (&sprd_codec->completion_dac_mute,
			     msecs_to_jiffies(SPRD_CODEC_DAC_MUTE_TIMEOUT));
			sprd_codec_dbg("dac mute completion %d\n",
				       dac_mute_complete);
			if (!dac_mute_complete) {
				pr_err("dac mute timeout\n");
			}
		}
#else
		if (mute && ret) {
			sprd_codec_wait(SPRD_CODEC_DAC_MUTE_WAIT_TIME);
		}
#endif
#endif

		sprd_codec_dbg("return %i\n", ret);
	}

	sprd_codec_dbg("Leaving %s\n", __func__);

	return ret;
}

static struct snd_soc_dai_ops sprd_codec_dai_ops = {
	.startup = sprd_codec_pcm_startup,
	.shutdown = sprd_codec_pcm_shutdown,
	.hw_params = sprd_codec_pcm_hw_params,
	.hw_free = sprd_codec_pcm_hw_free,
	.digital_mute = sprd_codec_digital_mute,
};

#ifdef CONFIG_PM
int sprd_codec_soc_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
	sprd_codec_dbg("Entering %s\n", __func__);

	sprd_codec_dbg("Leaving %s\n", __func__);

	return 0;
}

int sprd_codec_soc_resume(struct snd_soc_codec *codec)
{
	sprd_codec_dbg("Entering %s\n", __func__);
	sprd_codec_dbg("Leaving %s\n", __func__);
	return 0;
}
#else
#define sprd_codec_soc_suspend NULL
#define sprd_codec_soc_resume  NULL
#endif

/*
 * proc interface
 */

#ifdef CONFIG_PROC_FS
static void sprd_codec_proc_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	struct sprd_codec_priv *sprd_codec = entry->private_data;
	struct snd_soc_codec *codec = sprd_codec->codec;
	int reg;

	snd_iprintf(buffer, "%s digital part\n", codec->name);
	for (reg = SPRD_CODEC_DP_BASE; reg < SPRD_CODEC_DP_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (reg - SPRD_CODEC_DP_BASE)
			    , snd_soc_read(codec, reg + 0x00)
			    , snd_soc_read(codec, reg + 0x04)
			    , snd_soc_read(codec, reg + 0x08)
			    , snd_soc_read(codec, reg + 0x0C)
		    );
	}
	snd_iprintf(buffer, "%s analog part\n", codec->name);
	for (reg = SPRD_CODEC_AP_BASE; reg < SPRD_CODEC_AP_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (reg - SPRD_CODEC_AP_BASE)
			    , snd_soc_read(codec, reg + 0x00)
			    , snd_soc_read(codec, reg + 0x04)
			    , snd_soc_read(codec, reg + 0x08)
			    , snd_soc_read(codec, reg + 0x0C)
		    );
	}
}

static void sprd_codec_proc_init(struct sprd_codec_priv *sprd_codec)
{
	struct snd_info_entry *entry;
	struct snd_soc_codec *codec = sprd_codec->codec;

	if (!snd_card_proc_new(codec->card->snd_card, "sprd-codec", &entry))
		snd_info_set_text_ops(entry, sprd_codec, sprd_codec_proc_read);
}
#else /* !CONFIG_PROC_FS */
static inline void sprd_codec_proc_init(struct sprd_codec_priv *sprd_codec)
{
}
#endif

#define SPRD_CODEC_PCM_RATES 	\
	(SNDRV_PCM_RATE_8000 |  \
	 SNDRV_PCM_RATE_11025 | \
	 SNDRV_PCM_RATE_16000 | \
	 SNDRV_PCM_RATE_22050 | \
	 SNDRV_PCM_RATE_32000 | \
	 SNDRV_PCM_RATE_44100 | \
	 SNDRV_PCM_RATE_48000 | \
	 SNDRV_PCM_RATE_96000)

#define SPRD_CODEC_PCM_AD_RATES	\
	(SNDRV_PCM_RATE_8000 |  \
	 SNDRV_PCM_RATE_16000 | \
	 SNDRV_PCM_RATE_32000 | \
	 SNDRV_PCM_RATE_48000)

/* PCM Playing and Recording default in full duplex mode */
struct snd_soc_dai_driver sprd_codec_dai[] = {
	{
	 .name = "sprd-codec-i2s",
	 .playback = {
		      .stream_name = "Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SPRD_CODEC_PCM_RATES,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .stream_name = "Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SPRD_CODEC_PCM_AD_RATES,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &sprd_codec_dai_ops,
	 },
};

static int sprd_codec_soc_probe(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	sprd_codec_dbg("Entering %s\n", __func__);

	codec->dapm.idle_bias_off = 1;

	sprd_codec->codec = codec;

	sprd_codec_proc_init(sprd_codec);

	sprd_codec_dbg("return %i\n", ret);
	sprd_codec_dbg("Leaving %s\n", __func__);
	return ret;
}

/* power down chip */
static int sprd_codec_soc_remove(struct snd_soc_codec *codec)
{
	sprd_codec_dbg("Entering %s\n", __func__);

	sprd_codec_dbg("Leaving %s\n", __func__);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_sprd_codec = {
	.probe = sprd_codec_soc_probe,
	.remove = sprd_codec_soc_remove,
	.suspend = sprd_codec_soc_suspend,
	.resume = sprd_codec_soc_resume,
	.read = sprd_codec_read,
	.write = sprd_codec_write,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
	.dapm_widgets = sprd_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sprd_codec_dapm_widgets),
	.dapm_routes = sprd_codec_intercon,
	.num_dapm_routes = ARRAY_SIZE(sprd_codec_intercon),
	.controls = sprd_codec_snd_controls,
	.num_controls = ARRAY_SIZE(sprd_codec_snd_controls),
};

static __devinit int sprd_codec_probe(struct platform_device *pdev)
{
	struct sprd_codec_priv *sprd_codec;
	int ret;

	sprd_codec_dbg("Entering %s\n", __func__);

	sprd_codec = devm_kzalloc(&pdev->dev, sizeof(struct sprd_codec_priv),
				  GFP_KERNEL);
	if (sprd_codec == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, sprd_codec);

	atomic_set(&sprd_codec->power_refcount, 0);
	atomic_set(&sprd_codec->adie_adc_refcount, 0);
	atomic_set(&sprd_codec->adie_dac_refcount, 0);

	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_sprd_codec, sprd_codec_dai,
				     ARRAY_SIZE(sprd_codec_dai));
	if (ret != 0) {
		pr_err("Failed to register CODEC: %d\n", ret);
		return ret;
	}
#ifdef CONFIG_SPRD_CODEC_USE_INT
	sprd_codec->ap_irq = CODEC_AP_IRQ;

	ret =
	    request_irq(sprd_codec->ap_irq, sprd_codec_ap_irq, 0,
			"sprd_codec_ap", sprd_codec);
	if (ret) {
		pr_err("request_irq ap failed!\n");
		goto err_irq;
	}

	sprd_codec->dp_irq = CODEC_DP_IRQ;

	ret =
	    request_irq(sprd_codec->dp_irq, sprd_codec_dp_irq, 0,
			"sprd_codec_dp", sprd_codec);
	if (ret) {
		pr_err("request_irq dp failed!\n");
		goto dp_err_irq;
	}
#endif

	sprd_codec_dbg("Leaving %s\n", __func__);

	return 0;

#ifdef CONFIG_SPRD_CODEC_USE_INT
dp_err_irq:
	free_irq(sprd_codec->ap_irq, sprd_codec);
err_irq:
	snd_soc_unregister_codec(&pdev->dev);
	return -EINVAL;
#endif
}

static int __devexit sprd_codec_remove(struct platform_device *pdev)
{
#ifdef CONFIG_SPRD_CODEC_USE_INT
	struct sprd_codec_priv *sprd_codec = platform_get_drvdata(pdev);
	free_irq(sprd_codec->ap_irq, sprd_codec);
	free_irq(sprd_codec->dp_irq, sprd_codec);
#endif
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver sprd_codec_codec_driver = {
	.driver = {
		   .name = "sprd-codec",
		   .owner = THIS_MODULE,
		   },
	.probe = sprd_codec_probe,
	.remove = __devexit_p(sprd_codec_remove),
};

static int sprd_codec_init(void)
{
	sprd_codec_inter_pa_init();
	arch_audio_codec_switch(AUDIO_TO_ARM_CTRL);
	return platform_driver_register(&sprd_codec_codec_driver);
}

static void sprd_codec_exit(void)
{
	platform_driver_unregister(&sprd_codec_codec_driver);
}

module_init(sprd_codec_init);
module_exit(sprd_codec_exit);

MODULE_DESCRIPTION("SPRD-CODEC ALSA SoC codec driver");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("codec:sprd-codec");

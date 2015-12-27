/*
 * sound/soc/sprd/codec/dolphin/dolphin.c
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

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#include <mach/sprd-audio.h>
#include <mach/globalregs.h>
#include "dolphin.h"

#ifdef CONFIG_SPRD_AUDIO_DEBUG
#define dol_dbg pr_debug
#else
#define dol_dbg(...)
#endif

#define SOC_REG(r) ((unsigned short)(r))
#define FUN_REG(f) ((unsigned short)(-(f+1)))

#define DOLPHIN_NUM_SUPPLIES 1
static const char *dolphin_supply_names[DOLPHIN_NUM_SUPPLIES] = {
	"AVDDVB",
};

enum {
	DOLPHIN_LDO_NONE = 0,
	DOLPHIN_LDO_INIT,
	DOLPHIN_LDO_ON,
	DOLPHIN_LDO_OFF,
};

enum {
	DOLPHIN_PGA_MASTER = 0,
	DOLPHIN_PGA_LINE,
	DOLPHIN_PGA_HP,
	DOLPHIN_PGA_ADC,
	DOLPHIN_PGA_MIC_BOOST,
	DOLPHIN_PGA_EAR_BOOST,

	DOLPHIN_PGA_MAX
};

const char *dolphin_pga_debug_str[DOLPHIN_PGA_MAX] = {
	"Master",
	"Linein",
	"HP",
	"ADC",
	"Mic Boost",
	"Ear Boost",
};

typedef int (*dolphin_pga_set) (struct snd_soc_codec * codec, int left,
				int right);

struct dolphin_pga {
	dolphin_pga_set set;
	int min;
};

struct dolphin_pga_lr {
	int left;
	int right;
	dolphin_pga_set set;
};

/* codec private data */
struct dolphin_priv {
	struct snd_soc_codec *codec;
	struct regulator_bulk_data supplies[DOLPHIN_NUM_SUPPLIES];
	int ldo_status;
	atomic_t sb_refcount;
	atomic_t sb_sleep_refcount;
	atomic_t sb_dac;
	int da_sample_val;
	int ad_sample_val;
	struct dolphin_pga_lr pga[DOLPHIN_PGA_MAX];
};

static void dolphin_wait(u32 wait_time)
{
	if (wait_time)
		schedule_timeout_uninterruptible(msecs_to_jiffies(wait_time));
}

#if 0				/* sometime we will use this print function */
static void dolphin_print_regs(struct snd_soc_codec *codec)
{
	int reg;
	pr_info("dolphin register\n");
	for (reg = VBAICR; reg <= VBTR2; reg += 0x10) {
		pr_info("0x%04x | 0x%02x 0x%02x 0x%02x 0x%02x\n", (reg - VBAICR)
			, snd_soc_read(codec, reg + 0x00)
			, snd_soc_read(codec, reg + 0x04)
			, snd_soc_read(codec, reg + 0x08)
			, snd_soc_read(codec, reg + 0x0C)
		    );
	}
}
#endif

static int __dolphin_sb_dac_set(struct snd_soc_codec *codec, int on)
{
	int reg, val, mask;
	reg = VBPMR1;
	mask = (1 << SB_DAC);
	val = on ? 0 : mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int dolphin_sb_dac_enable(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);
	dol_dbg("Entering %s %d\n", __func__, atomic_read(&dolphin->sb_dac));
	atomic_inc(&dolphin->sb_dac);
	if (atomic_read(&dolphin->sb_dac) == 1) {
		ret = __dolphin_sb_dac_set(codec, 1);
	}
	return ret;
}

static int dolphin_sb_dac_disable(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);
	dol_dbg("Entering %s %d\n", __func__, atomic_read(&dolphin->sb_dac));
	if (atomic_dec_and_test(&dolphin->sb_dac)) {
		ret = __dolphin_sb_dac_set(codec, 0);
	}
	return ret;
}

static int dolphin_pga_master_set(struct snd_soc_codec *codec, int left,
				  int right)
{
	int reg, val;
	reg = VBCGR1;
	val = (left & 0xF) | ((right & 0xF) << 4);
	return snd_soc_update_bits(codec, SOC_REG(reg), 0xFF, val);
}

static int dolphin_pga_line_set(struct snd_soc_codec *codec, int left,
				int right)
{
	int ret;
	int reg, val;
	reg = VBCGR2;
	val = (left & 0x1F);
	ret = snd_soc_update_bits(codec, SOC_REG(reg), 0x1F, val);
	if (ret < 0)
		return ret;
	reg = VBCGR3;
	val = (right & 0x1F);
	return snd_soc_update_bits(codec, SOC_REG(reg), 0x1F, val);
}

static int dolphin_pga_hp_set(struct snd_soc_codec *codec, int left, int right)
{
	int ret;
	int reg, val;
	reg = VBCGR8;
	val = (left & 0x1F);
	ret = snd_soc_update_bits(codec, SOC_REG(reg), 0x1F, val);
	if (ret < 0)
		return ret;
	reg = VBCGR9;
	val = (right & 0x1F);
	return snd_soc_update_bits(codec, SOC_REG(reg), 0x1F, val);
}

static int dolphin_pga_adc_set(struct snd_soc_codec *codec, int left, int right)
{
	int reg, val;
	reg = VBCGR10;
	val = ((left & 0xF) << 4);
	return snd_soc_update_bits(codec, SOC_REG(reg), 0xF0, val);
}

static int dolphin_pga_mic_boost_set(struct snd_soc_codec *codec, int left,
				     int right)
{
	int reg, val;
	reg = VBPMR2;
	val = ((left & 0x1) << 3);
	return snd_soc_update_bits(codec, SOC_REG(reg), 0x08, val);
}

static int dolphin_pga_ear_boost_set(struct snd_soc_codec *codec, int left,
				     int right)
{
	int reg, val;
	reg = VBCGR3;
	val = ((left & 0x1) << 7);
	return snd_soc_update_bits(codec, SOC_REG(reg), 0x80, val);
}

struct dolphin_pga dolphin_pga_cfg[DOLPHIN_PGA_MAX] = {
	{dolphin_pga_master_set, 15},
	{dolphin_pga_line_set, 19},
	{dolphin_pga_hp_set, 31},

	{dolphin_pga_adc_set, 0},
	{dolphin_pga_mic_boost_set, 0},
	{dolphin_pga_ear_boost_set, 0},
};

static int dolphin_set_sample_rate(struct snd_soc_codec *codec, int rate,
				   int mask, int shift)
{
	switch (rate) {
	case 8000:
		snd_soc_update_bits(codec, SOC_REG(VBCCR2), mask,
				    DOLPHIN_RATE_8000 << shift);
		break;
	case 11025:
		snd_soc_update_bits(codec, SOC_REG(VBCCR2), mask,
				    DOLPHIN_RATE_11025 << shift);
		break;
	case 16000:
		snd_soc_update_bits(codec, SOC_REG(VBCCR2), mask,
				    DOLPHIN_RATE_16000 << shift);
		break;
	case 22050:
		snd_soc_update_bits(codec, SOC_REG(VBCCR2), mask,
				    DOLPHIN_RATE_22050 << shift);
		break;
	case 32000:
		snd_soc_update_bits(codec, SOC_REG(VBCCR2), mask,
				    DOLPHIN_RATE_32000 << shift);
		break;
	case 44100:
		snd_soc_update_bits(codec, SOC_REG(VBCCR2), mask,
				    DOLPHIN_RATE_44100 << shift);
		break;
	case 48000:
		snd_soc_update_bits(codec, SOC_REG(VBCCR2), mask,
				    DOLPHIN_RATE_48000 << shift);
		break;
	case 96000:
		snd_soc_update_bits(codec, SOC_REG(VBCCR2), mask,
				    DOLPHIN_RATE_96000 << shift);
		break;
	default:
		pr_err("dolphin not supports rate %d\n", rate);
		break;
	}
	return 0;
}

static int dolphin_sample_rate_setting(struct dolphin_priv *dolphin)
{
	if (dolphin->ad_sample_val) {
		dolphin_set_sample_rate(dolphin->codec, dolphin->ad_sample_val,
					0x0F, 0);
	}
	if (dolphin->da_sample_val) {
		dolphin_set_sample_rate(dolphin->codec, dolphin->da_sample_val,
					0xF0, 4);
	}
	return 0;
}

static int dolphin_ldo_on(struct dolphin_priv *dolphin)
{
	int ret;
	dol_dbg("Entering %s\n", __func__);
	if (DOLPHIN_LDO_NONE != dolphin->ldo_status) {
		ret = regulator_bulk_enable(ARRAY_SIZE(dolphin->supplies),
					    dolphin->supplies);
		if (ret != 0) {
			pr_err("Failed to enable supplies: %d\n", ret);
			goto ldo_on_out;
		}
		arch_audio_codec_enable();
		if (DOLPHIN_LDO_ON != dolphin->ldo_status) {
			dolphin_wait(DOLPHIN_LDO_WAIT_TIME);
		}
		dolphin->ldo_status = DOLPHIN_LDO_ON;
		arch_audio_codec_reset();
		dolphin_sample_rate_setting(dolphin);
	} else {
		pr_err("Uninit before enable supplies\n");
		ret = -EINVAL;
		goto ldo_on_out;
	}
ldo_on_out:
	dol_dbg("return %i\n", ret);
	dol_dbg("Leaving %s\n", __func__);
	return ret;
}

static int dolphin_ldo_off(struct dolphin_priv *dolphin)
{
	int ret;
	dol_dbg("Entering %s\n", __func__);
	if (DOLPHIN_LDO_NONE != dolphin->ldo_status) {
		arch_audio_codec_disable();
		ret = regulator_bulk_disable(ARRAY_SIZE(dolphin->supplies),
					     dolphin->supplies);
		if (ret != 0) {
			pr_err("Failed to disable supplies: %d\n", ret);
			goto ldo_off_out;
		}
		dolphin->ldo_status = DOLPHIN_LDO_OFF;
		arch_audio_codec_reset();
	} else {
		pr_err("Uninit before disable supplies: %d\n", ret);
		ret = -EINVAL;
		goto ldo_off_out;
	}
ldo_off_out:
	dol_dbg("return %i\n", ret);
	dol_dbg("Leaving %s\n", __func__);
	return ret;
}

static int dolphin_open(struct snd_soc_codec *codec)
{
	int ret = 0;

	dol_dbg("Entering %s\n", __func__);

	snd_soc_update_bits(codec, SOC_REG(VBPMR2), (1 << SB_MC), (1 << SB_MC));
	/*Set serial interface, IIS mode */
	snd_soc_update_bits(codec, SOC_REG(VBAICR), 0x0F, 0x0F);
	snd_soc_update_bits(codec, SOC_REG(VBCR1),
			    ((0x3 << ADC_ADWL) | (0x03 << DAC_ADWL)),
			    ((ADC_DATA_WIDTH_16_bit << ADC_ADWL) |
			     (DAC_DATA_WIDTH_16_bit << DAC_ADWL)));
	/*dolphin PGA bug fix */
	snd_soc_update_bits(codec, SOC_REG(VBCGR1), 0xFF, 0x11);
	ndelay(1);
	snd_soc_update_bits(codec, SOC_REG(VBCGR1), 0xFF, 0x00);
	dol_dbg("Leaving %s\n", __func__);
	return ret;
}

static void dolphin_sb_enable(struct snd_soc_codec *codec)
{
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);
	int ret;

	atomic_inc(&dolphin->sb_refcount);
	if (atomic_read(&dolphin->sb_refcount) == 1) {
		dol_dbg("Entering %s\n", __func__);
		ret = dolphin_ldo_on(dolphin);
		if (ret != 0)
			pr_err("dolphin open ldo error %d\n", ret);
		dolphin_open(codec);
		snd_soc_update_bits(codec, SOC_REG(VBPMR2), (1 << SB),
				    (0 << SB));
		dolphin_wait(DOLPHIN_TSBYU_WAIT_TIME);
		dol_dbg("Leaving %s\n", __func__);
	}
}

static void dolphin_sb_disable(struct snd_soc_codec *codec)
{
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);

	if (atomic_dec_and_test(&dolphin->sb_refcount)) {
		dol_dbg("Entering %s\n", __func__);
		snd_soc_update_bits(codec, SOC_REG(VBPMR2), (1 << SB),
				    (1 << SB));
		dolphin_ldo_off(dolphin);
		dol_dbg("Leaving %s\n", __func__);
	}
}

static int sb_event(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dolphin_sb_enable(codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		dolphin_sb_disable(codec);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static void dolphin_sb_sleep_enable(struct snd_soc_codec *codec)
{
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);

	atomic_inc(&dolphin->sb_sleep_refcount);
	if (atomic_read(&dolphin->sb_sleep_refcount) == 1) {
		dol_dbg("Entering %s\n", __func__);
		snd_soc_update_bits(codec, SOC_REG(VBPMR2), (1 << SB_SLEEP),
				    (0 << SB_SLEEP));
		dol_dbg("Leaving %s\n", __func__);
	}
}

static void dolphin_sb_sleep_disable(struct snd_soc_codec *codec)
{
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);

	if (atomic_dec_and_test(&dolphin->sb_sleep_refcount)) {
		dol_dbg("Entering %s\n", __func__);
		snd_soc_update_bits(codec, SOC_REG(VBPMR2), (1 << SB_SLEEP),
				    (1 << SB_SLEEP));
		dol_dbg("Leaving %s\n", __func__);
	}
}

static int sb_sleep_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dolphin_sb_sleep_enable(codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		dolphin_sb_sleep_disable(codec);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int sb_out_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_PRE_PMD:
		dolphin_sb_dac_enable(w->codec);
		break;
	case SND_SOC_DAPM_POST_PMU:
	case SND_SOC_DAPM_POST_PMD:
		dol_dbg("Entering %s\n", __func__);
		dol_dbg("SB_OUT:0x%x\n",
			snd_soc_read(w->codec, VBPMR1) & (1 << SB_OUT));
		dolphin_wait(DOLPHIN_HP_POP_WAIT_TIME);
		dolphin_sb_dac_disable(w->codec);
		dol_dbg("Leaving %s\n", __func__);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int pga_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event, int id)
{
	struct snd_soc_codec *codec = w->codec;
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);
	struct dolphin_pga_lr *pga = &(dolphin->pga[id]);
	int ret = 0;
	int min = dolphin_pga_cfg[id].min;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		pga->set = dolphin_pga_cfg[id].set;
		ret = pga->set(codec, pga->left, pga->right);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		pga->set = 0;
		ret = dolphin_pga_cfg[id].set(codec, min, min);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int sb_dac_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	pr_info("DAC %s\n", SND_SOC_DAPM_EVENT_ON(event) ? "ON" : "OFF");
	switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
		dolphin_sb_dac_enable(w->codec);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dolphin_sb_dac_disable(w->codec);
		break;
	default:
		break;
	}
	return pga_event(w, kcontrol, event, DOLPHIN_PGA_MASTER);
}

static int sb_line_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	return pga_event(w, kcontrol, event, DOLPHIN_PGA_LINE);
}

static int sb_hp_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	return pga_event(w, kcontrol, event, DOLPHIN_PGA_HP);
}

static int sb_ear_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	return pga_event(w, kcontrol, event, DOLPHIN_PGA_EAR_BOOST);
}

static int sb_adc_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	int ret;
	pr_info("ADC %s\n", SND_SOC_DAPM_EVENT_ON(event) ? "ON" : "OFF");
	ret = pga_event(w, kcontrol, event, DOLPHIN_PGA_ADC);
	if (ret < 0)
		return ret;
	return pga_event(w, kcontrol, event, DOLPHIN_PGA_MIC_BOOST);
}

static const struct snd_soc_dapm_widget dolphin_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY("Power", SND_SOC_NOPM, 0, 0, sb_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("Sleep", 1, SND_SOC_NOPM, 0, 0, sb_sleep_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA("Output Mixer", SOC_REG(VBPMR1), SB_MIX, 1, NULL, 0),

	SND_SOC_DAPM_PGA("LineIn Switch", SOC_REG(VBCR1), BYPASS, 0, NULL, 0),

	SND_SOC_DAPM_PGA("DAC Switch", SOC_REG(VBCR1), DACSEL, 0, NULL, 0),

	SND_SOC_DAPM_DAC_E("DAC", "Playback", SND_SOC_NOPM, 0, 0,
			   sb_dac_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_S("HP Switch", 2, SOC_REG(VBPMR1), SB_OUT, 1,
			   sb_out_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD |
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("HP Mute", 3, SOC_REG(VBCR1), HP_DIS, 1, sb_hp_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_S("SPK Switch", 1, SOC_REG(VBPMR1), SB_LOUT, 1, NULL,
			   0),

	SND_SOC_DAPM_PGA_S("EAR Switch", 1, SOC_REG(VBPMR1), SB_BTL, 1, NULL,
			   0),

	SND_SOC_DAPM_PGA_S("EAR Mute", 3, SOC_REG(VBCR1), BTL_MUTE, 1,
			   sb_ear_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_ADC_E("ADC", "Capture", SOC_REG(VBPMR1), SB_ADC, 1,
			   sb_adc_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA("Mic Switch", SOC_REG(VBCR2), MICSEL, 1, NULL, 0),

	SND_SOC_DAPM_PGA("AuxMic Switch", SOC_REG(VBCR2), MICSEL, 0, NULL, 0),

	SND_SOC_DAPM_PGA_E("Line Input", SOC_REG(VBPMR1), SB_LIN, 1, NULL, 0,
			   sb_line_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MICBIAS("Mic Bias", SOC_REG(VBCR1), SB_MICBIAS, 1),

	SND_SOC_DAPM_OUTPUT("HEAD_P"),
	SND_SOC_DAPM_OUTPUT("AUXSP"),
	SND_SOC_DAPM_OUTPUT("EAR"),

	SND_SOC_DAPM_INPUT("MIC"),
	SND_SOC_DAPM_INPUT("AUXMIC"),
	SND_SOC_DAPM_INPUT("AI"),
};

/* dolphin supported interconnection*/
static const struct snd_soc_dapm_route dolphin_intercon[] = {
	/* Power */
	{"Sleep", NULL, "Power"},
	{"DAC", NULL, "Sleep"},
	{"ADC", NULL, "Sleep"},
	{"Line Input", NULL, "Sleep"},

	/* Playback */
	{"Output Mixer", NULL, "DAC Switch"},
	{"DAC Switch", NULL, "DAC"},

	/* Line */
	{"Output Mixer", NULL, "LineIn Switch"},
	{"LineIn Switch", NULL, "SPK Switch"},
	{"SPK Switch", NULL, "Line Input"},
	{"Line Input", NULL, "AI"},

	/* Output */
	{"HP Switch", NULL, "Output Mixer"},
	{"SPK Switch", NULL, "Output Mixer"},
	{"EAR Switch", NULL, "SPK Switch"},

	{"HP Mute", NULL, "HP Switch"},
	{"HEAD_P", NULL, "HP Mute"},
	{"AUXSP", NULL, "SPK Switch"},
	{"EAR Mute", NULL, "EAR Switch"},
	{"EAR", NULL, "EAR Mute"},

	/* Captrue */
	{"ADC", NULL, "Mic Switch"},
	{"ADC", NULL, "AuxMic Switch"},
	{"Mic Switch", NULL, "Mic Bias"},
	{"AuxMic Switch", NULL, "AUXMIC"},

	/* Input */
	{"Mic Bias", NULL, "MIC"},
};

static int dolphin_vol_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	struct dolphin_pga_lr *pga = &(dolphin->pga[reg]);
	int ret = 0;

	pr_info("set PGA[%s] to %ld\n", dolphin_pga_debug_str[reg],
		ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	pga->left = val;
	if (shift != rshift) {
		val = (ucontrol->value.integer.value[1] & mask);
		if (invert)
			val = max - val;
		pga->right = val;
	}
	if (pga->set) {
		ret = pga->set(codec, pga->left, pga->right);
	}
	dol_dbg("Leaving %s\n", __func__);
	return ret;
}

static int dolphin_vol_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;
	int max = mc->max;
	unsigned int invert = mc->invert;
	struct dolphin_pga_lr *pga = &(dolphin->pga[reg]);

	ucontrol->value.integer.value[0] = pga->left;
	if (shift != rshift)
		ucontrol->value.integer.value[1] = pga->right;
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
		if (shift != rshift)
			ucontrol->value.integer.value[1] =
			    max - ucontrol->value.integer.value[1];
	}

	return 0;
}

static int dolphin_vol_put_2r(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	int ret = 0;
	unsigned int val, val2;
	struct dolphin_pga_lr *pga = &(dolphin->pga[reg]);

	pr_info("set PGA[%s] to %ld %ld\n", dolphin_pga_debug_str[reg],
		ucontrol->value.integer.value[0],
		ucontrol->value.integer.value[1]);

	val = (ucontrol->value.integer.value[0] & mask);
	val2 = (ucontrol->value.integer.value[1] & mask);

	if (invert) {
		val = max - val;
		val2 = max - val2;
	}

	pga->left = val;
	pga->right = val2;

	if (pga->set) {
		ret = pga->set(codec, pga->left, pga->right);
	}

	return ret;
}

static int dolphin_vol_get_2r(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int invert = mc->invert;
	struct dolphin_pga_lr *pga = &(dolphin->pga[reg]);

	ucontrol->value.integer.value[0] = pga->left;
	ucontrol->value.integer.value[1] = pga->right;
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
		ucontrol->value.integer.value[1] =
		    max - ucontrol->value.integer.value[1];
	}

	return 0;
}

static const DECLARE_TLV_DB_SCALE(dac_tlv, -2250, 150, 0);
static const DECLARE_TLV_DB_SCALE(line_bypass_tlv, -2250, 150, 0);
static const DECLARE_TLV_DB_SCALE(mic_tlv, 0, 150, 0);
static const DECLARE_TLV_DB_SCALE(mic_boost_tlv, 0, 2000, 0);
static const DECLARE_TLV_DB_SCALE(blt_boost_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_MINMAX(hp_tlv, -3350, 450);

static const struct snd_kcontrol_new dolphin_snd_controls[] = {
	SOC_DOUBLE_EXT_TLV("Master Playback Volume",
			   FUN_REG(DOLPHIN_PGA_MASTER), 0, 4,
			   15, 1, dolphin_vol_get, dolphin_vol_put, dac_tlv),
	SOC_DOUBLE_R_EXT_TLV("Line Playback Volume", FUN_REG(DOLPHIN_PGA_LINE),
			     0, 0, 19,
			     1, dolphin_vol_get_2r, dolphin_vol_put_2r,
			     line_bypass_tlv),
	SOC_SINGLE_EXT_TLV("Capture Volume", FUN_REG(DOLPHIN_PGA_ADC), 4, 15, 0,
			   dolphin_vol_get, dolphin_vol_put, mic_tlv),
	SOC_SINGLE_EXT_TLV("Mic Boost Volume", FUN_REG(DOLPHIN_PGA_MIC_BOOST),
			   GIM, 1, 0,
			   dolphin_vol_get, dolphin_vol_put, mic_boost_tlv),
	SOC_SINGLE_EXT_TLV("Ear Boost Volume", FUN_REG(DOLPHIN_PGA_EAR_BOOST),
			   7, 1, 0,
			   dolphin_vol_get, dolphin_vol_put, blt_boost_tlv),
	SOC_DOUBLE_R_EXT_TLV("HeadPhone Playback Volume",
			     FUN_REG(DOLPHIN_PGA_HP), 0, 0,
			     31, 1, dolphin_vol_get_2r, dolphin_vol_put_2r,
			     hp_tlv),
};

#define ARM_DOL_BASE_HI (ARM_DOL_BASE & 0xFFFF0000)

static unsigned int dolphin_read(struct snd_soc_codec *codec, unsigned int reg)
{
	/* Because snd_soc_update_bits reg is 16 bits short type,
	   so muse do following convert
	 */
	reg |= ARM_DOL_BASE_HI;
	if (IS_DOLPHIN_RANG(reg))
		return arch_audio_codec_read(reg);
	dol_dbg("read the register is not codec's reg = 0x%x\n", reg);
	return 0;
}

static int dolphin_write(struct snd_soc_codec *codec, unsigned int reg,
			 unsigned int val)
{
	int ret;
	reg |= ARM_DOL_BASE_HI;
	if (IS_DOLPHIN_RANG(reg)) {
		ret = arch_audio_codec_write(reg, val);
	} else {
		ret = 0;
		dol_dbg("write the register is not codec's reg = 0x%x\n", reg);
	}
	return ret;
}

static int dolphin_pcm_startup(struct snd_pcm_substream *substream,
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

static void dolphin_pcm_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_soc_dapm_disable_pin(&codec->card->dapm, "DAC");
	} else {
		snd_soc_dapm_disable_pin(&codec->card->dapm, "ADC");
	}
}

static int dolphin_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);
	int mask = 0x0F;
	int shift = 0;
	int rate = params_rate(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		shift = 4;
		mask <<= shift;
		dolphin->da_sample_val = rate;
		pr_info("playback rate is [%d]\n", rate);
	} else {
		dolphin->ad_sample_val = rate;
		pr_info("capture rate is [%d]\n", rate);
	}
	dolphin_set_sample_rate(codec, rate, mask, shift);

	return 0;
}

static int dolphin_pcm_hw_free(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	dol_dbg("Entering %s\n", __func__);
	dol_dbg("Leaving %s\n", __func__);
	return 0;
}

static int dolphin_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dol_dbg("Entering %s\n", __func__);
	if (atomic_read(&dolphin->sb_refcount) >= 1) {
		dol_dbg("mute %i\n", mute);

		ret =
		    snd_soc_update_bits(codec, SOC_REG(VBCR1), (1 << DAC_MUTE),
					((mute ? 1 : 0) << DAC_MUTE));

#ifdef CONFIG_CODEC_DAC_MUTE_WAIT
		if (mute && ret) {
			dolphin_wait(DOLPHIN_DAC_MUTE_WAIT_TIME);
		}
#endif

		dol_dbg("return %i\n", ret);
	}
	dol_dbg("Leaving %s\n", __func__);

	return ret;
}

static struct snd_soc_dai_ops dolphin_dai_ops = {
	.startup = dolphin_pcm_startup,
	.shutdown = dolphin_pcm_shutdown,
	.hw_params = dolphin_pcm_hw_params,
	.hw_free = dolphin_pcm_hw_free,
	.digital_mute = dolphin_digital_mute,
};

#ifdef CONFIG_PM
int dolphin_soc_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
	dol_dbg("Entering %s\n", __func__);

	dol_dbg("Leaving %s\n", __func__);

	return 0;
}

int dolphin_soc_resume(struct snd_soc_codec *codec)
{
	dol_dbg("Entering %s\n", __func__);
	dol_dbg("Leaving %s\n", __func__);
	return 0;
}
#else
#define dolphin_soc_suspend NULL
#define dolphin_soc_resume  NULL
#endif

/*
 * proc interface
 */

#ifdef CONFIG_PROC_FS
static void dolphin_proc_read(struct snd_info_entry *entry,
			      struct snd_info_buffer *buffer)
{
	struct dolphin_priv *dolphin = entry->private_data;
	struct snd_soc_codec *codec = dolphin->codec;
	int reg;

	snd_iprintf(buffer, "%s\n", codec->name);
	for (reg = VBAICR; reg <= VBTR2; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%02x 0x%02x 0x%02x 0x%02x\n",
			    (reg - VBAICR)
			    , snd_soc_read(codec, reg + 0x00)
			    , snd_soc_read(codec, reg + 0x04)
			    , snd_soc_read(codec, reg + 0x08)
			    , snd_soc_read(codec, reg + 0x0C)
		    );
	}
}

static void dolphin_proc_init(struct dolphin_priv *dolphin)
{
	struct snd_info_entry *entry;
	struct snd_soc_codec *codec = dolphin->codec;

	if (!snd_card_proc_new(codec->card->snd_card, "dolphin", &entry))
		snd_info_set_text_ops(entry, dolphin, dolphin_proc_read);
}
#else /* !CONFIG_PROC_FS */
static inline void dolphin_proc_init(struct dolphin_priv *dolphin)
{
}
#endif

#define DOLPHIN_PCM_RATES 	\
	(SNDRV_PCM_RATE_8000 |  \
	 SNDRV_PCM_RATE_11025 | \
	 SNDRV_PCM_RATE_16000 | \
	 SNDRV_PCM_RATE_22050 | \
	 SNDRV_PCM_RATE_32000 | \
	 SNDRV_PCM_RATE_44100 | \
	 SNDRV_PCM_RATE_48000 | \
	 SNDRV_PCM_RATE_96000)

/* PCM Playing and Recording default in full duplex mode */
struct snd_soc_dai_driver dolphin_dai[] = {
	{
	 .name = "dolphin-i2s",
	 .playback = {
		      .stream_name = "Playback",
		      .channels_min = 2,
		      .channels_max = 2,
		      .rates = DOLPHIN_PCM_RATES,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .capture = {
		     .stream_name = "Capture",
		     .channels_min = 1,
		     .channels_max = 1,
		     .rates = DOLPHIN_PCM_RATES,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &dolphin_dai_ops,
	 },
};

static int dolphin_soc_probe(struct snd_soc_codec *codec)
{
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);
	int ret = 0, i;

	dol_dbg("Entering %s\n", __func__);

	codec->dapm.idle_bias_off = 1;

	dolphin->codec = codec;

	for (i = 0; i < ARRAY_SIZE(dolphin->supplies); i++)
		dolphin->supplies[i].supply = dolphin_supply_names[i];

	ret = regulator_bulk_get(codec->dev, ARRAY_SIZE(dolphin->supplies),
				 dolphin->supplies);
	if (ret != 0) {
		pr_err("Failed to request supplies: %d\n", ret);
		goto probe_out;
	}
	dolphin->ldo_status = DOLPHIN_LDO_INIT;

	dolphin_proc_init(dolphin);

probe_out:
	dol_dbg("return %i\n", ret);
	dol_dbg("Leaving %s\n", __func__);
	return ret;
}

/* power down chip */
static int dolphin_soc_remove(struct snd_soc_codec *codec)
{
	struct dolphin_priv *dolphin = snd_soc_codec_get_drvdata(codec);

	dol_dbg("Entering %s\n", __func__);

	regulator_bulk_disable(ARRAY_SIZE(dolphin->supplies),
			       dolphin->supplies);
	regulator_bulk_free(ARRAY_SIZE(dolphin->supplies), dolphin->supplies);

	dol_dbg("Leaving %s\n", __func__);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_dolphin = {
	.probe = dolphin_soc_probe,
	.remove = dolphin_soc_remove,
	.suspend = dolphin_soc_suspend,
	.resume = dolphin_soc_resume,
	.read = dolphin_read,
	.write = dolphin_write,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
	.dapm_widgets = dolphin_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(dolphin_dapm_widgets),
	.dapm_routes = dolphin_intercon,
	.num_dapm_routes = ARRAY_SIZE(dolphin_intercon),
	.controls = dolphin_snd_controls,
	.num_controls = ARRAY_SIZE(dolphin_snd_controls),
};

static __devinit int dolphin_probe(struct platform_device *pdev)
{
	struct dolphin_priv *dolphin;
	int ret;

	dol_dbg("Entering %s\n", __func__);

	dolphin = devm_kzalloc(&pdev->dev, sizeof(struct dolphin_priv),
			       GFP_KERNEL);
	if (dolphin == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, dolphin);

	atomic_set(&dolphin->sb_refcount, 0);
	atomic_set(&dolphin->sb_sleep_refcount, 0);
	atomic_set(&dolphin->sb_dac, 0);

	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_dolphin, dolphin_dai,
				     ARRAY_SIZE(dolphin_dai));
	if (ret != 0) {
		pr_err("Failed to register CODEC: %d\n", ret);
		return ret;
	}

	dol_dbg("Leaving %s\n", __func__);

	return 0;

}

static int __devexit dolphin_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver dolphin_codec_driver = {
	.driver = {
		   .name = "dolphin",
		   .owner = THIS_MODULE,
		   },
	.probe = dolphin_probe,
	.remove = __devexit_p(dolphin_remove),
};

static int dolphin_init(void)
{
	arch_audio_codec_switch(AUDIO_TO_ARM_CTRL);
	return platform_driver_register(&dolphin_codec_driver);
}

static void dolphin_exit(void)
{
	platform_driver_unregister(&dolphin_codec_driver);
}

module_init(dolphin_init);
module_exit(dolphin_exit);

MODULE_DESCRIPTION("DOLPHIN ALSA SoC codec driver");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("codec:dolphin");

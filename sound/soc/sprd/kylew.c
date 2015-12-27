/*
 * sound/soc/sprd/kylew.c
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
#define pr_fmt(fmt) "[audio:kylew] " fmt

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#include <sound/audio_pa.h>
#include <mach/sprd-audio.h>

int vbc_add_controls(struct snd_soc_codec *codec);

#ifdef CONFIG_SPRD_AUDIO_DEBUG
#define kylew_dbg pr_debug
#else
#define kylew_dbg(...)
#endif

#define FUN_REG(f) ((unsigned short)(-(f+1)))

#define SWITCH_FUN_ON    1
#define SWITCH_FUN_OFF   0

enum {
	KYLEW_PGA_INTER_PA = 0,
	KYLEW_PGA_LINEINREC,

	KYLEW_PGA_MAX
};

enum {
	KYLEW_FUNC_SPK = 0,
	KYLEW_FUNC_EAR,
	KYLEW_FUNC_HP,
	KYLEW_FUNC_LINE,
	KYLEW_FUNC_MIC,
	KYLEW_FUNC_HP_MIC,
	KYLEW_FUNC_LINEINREC,
	KYLEW_FUNC_MIC_BIAS,

	KYLEW_FUNC_MAX
};

struct kylew_pga {
	int (*set) (int pga);
	int (*get) (void);
};

static struct kylew_pga kylew_pga_cfg[KYLEW_PGA_MAX] = {
	{arch_audio_inter_pa_set_pga,
	 arch_audio_inter_pa_get_pga},
	{arch_audio_codec_lineinrec_set_pga,
	 arch_audio_codec_lineinrec_get_pga},
};

static struct kylew_priv {
	int func[KYLEW_FUNC_MAX];
	int pga[KYLEW_PGA_MAX];
} kylew;

static const char *func_name[KYLEW_FUNC_MAX] = {
	"Ext Spk",
	"Ext Ear",
	"HeadPhone Jack",
	"Line Jack",
	"Mic Jack",
	"HP Mic Jack",
	"Line Rec",
	"Mic Bias",
};

static void kylew_ext_control(struct snd_soc_dapm_context *dapm)
{
	int i;
	for (i = 0; i < KYLEW_FUNC_LINEINREC; i++) {
		if (kylew.func[i] == SWITCH_FUN_ON)
			snd_soc_dapm_enable_pin(dapm, func_name[i]);
		else
			snd_soc_dapm_disable_pin(dapm, func_name[i]);
	}

	if (kylew.func[KYLEW_FUNC_LINEINREC] == SWITCH_FUN_ON){
        arch_audio_codec_lineinrec_enable();
		snd_soc_dapm_enable_pin(dapm, func_name[KYLEW_FUNC_HP_MIC]);
    }
    else
        arch_audio_codec_lineinrec_disable();

	if (kylew.func[KYLEW_FUNC_MIC] == SWITCH_FUN_ON)
		snd_soc_dapm_disable_pin(dapm, func_name[KYLEW_FUNC_HP_MIC]);

	if (kylew.func[KYLEW_FUNC_MIC_BIAS] == SWITCH_FUN_ON)
		snd_soc_dapm_force_enable_pin(dapm, func_name[KYLEW_FUNC_MIC_BIAS]);
	else
		snd_soc_dapm_disable_pin(dapm, func_name[KYLEW_FUNC_MIC_BIAS]);

	/* signal a DAPM event */
	snd_soc_dapm_sync(dapm);
}

static inline void local_cpu_pa_control(bool enable)
{
	int ret;
	if (enable) {
		ret = arch_audio_inter_pa_enable();
	} else {
		ret = arch_audio_inter_pa_disable();
	}
	if (ret < 0)
		pr_err("kylew audio inter pa control error: %d\n", enable);
}

static void audio_speaker_enable(int enable)
{
	if (audio_pa_amplifier && audio_pa_amplifier->speaker.control)
		audio_pa_amplifier->speaker.control(enable, NULL);
	else
		local_cpu_pa_control(enable);
}

static int kylew_hp_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *k, int event)
{
	kylew_dbg("Entering %s e=0x%x\n", __func__, event);
	if (audio_pa_amplifier && audio_pa_amplifier->headset.control)
		audio_pa_amplifier->
		    headset.control(SND_SOC_DAPM_EVENT_ON(event), NULL);
	kylew_dbg("Leaving %s\n", __func__);
	return 0;
}

static int kylew_ear_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *k, int event)
{
	kylew_dbg("Entering %s e=0x%x\n", __func__, event);
	if (audio_pa_amplifier && audio_pa_amplifier->earpiece.control)
		audio_pa_amplifier->
		    earpiece.control(SND_SOC_DAPM_EVENT_ON(event), NULL);
	kylew_dbg("Leaving %s\n", __func__);
	return 0;
}

static int kylew_amp_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *k, int event)
{
	kylew_dbg("Entering %s e=0x%x\n", __func__, event);
	audio_speaker_enable(SND_SOC_DAPM_EVENT_ON(event));
	kylew_dbg("Leaving %s\n", __func__);
	return 0;
}

static const struct snd_soc_dapm_widget dolphin_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_MIC("HP Mic Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", kylew_amp_event),
	SND_SOC_DAPM_SPK("Ext Ear", kylew_ear_event),
	SND_SOC_DAPM_LINE("Line Jack", NULL),
	SND_SOC_DAPM_HP("HeadPhone Jack", kylew_hp_event),
};

/* kylew supported audio map */
static const struct snd_soc_dapm_route kylew_audio_map[] = {

	{"HeadPhone Jack", NULL, "HEAD_P"},
	{"Ext Spk", NULL, "AUXSP"},
	{"Ext Ear", NULL, "EAR"},
	{"MIC", NULL, "Mic Jack"},
	{"AUXMIC", NULL, "HP Mic Jack"},
	{"AI", NULL, "Line Jack"},
};

static inline int kylew_func_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	ucontrol->value.integer.value[0] = kylew.func[id];
	return 0;
}

static int kylew_func_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_card *card = codec->card;
	int id = FUN_REG(mc->reg);

	if (kylew.func[id] == ucontrol->value.integer.value[0])
		return 0;

	pr_info("Entering %s %d = %ld\n", __func__, id,
		   ucontrol->value.integer.value[0]);
	kylew.func[id] = ucontrol->value.integer.value[0];
	kylew_ext_control(&card->dapm);
	kylew_dbg("Leaving %s\n", __func__);
	return 1;
}

static int kylew_pga_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int ret;
	int val;
	int id = FUN_REG(mc->reg);

	ret = kylew_pga_cfg[id].get();
	if (ret < 0)
		return ret;
	kylew.pga[id] = ret;
	if (mc->invert) {
		val = mc->max - ret;
	} else {
		val = ret;
	}
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int kylew_pga_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int ret;
	int val;
	int id = FUN_REG(mc->reg);
	unsigned int mask = (1 << fls(mc->max)) - 1;

	kylew_dbg("Entering %s\n", __func__);
	if (0 == kylew.pga[id]) {
		ret = kylew_pga_cfg[id].get();
		if (ret < 0)
			return ret;
		kylew.pga[id] = ret;
	}

	val = ucontrol->value.integer.value[0] & mask;
	if (mc->invert) {
		val = mc->max - val;
	}

	if (kylew.pga[id] == val)
		return 0;

	kylew.pga[id] = val;
	ret = kylew_pga_cfg[id].set(kylew.pga[id]);
	kylew_dbg("pga %d val %d\n", id, kylew.pga[id]);
	kylew_dbg("Leaving %s\n", __func__);
	return 1;
}

static const DECLARE_TLV_DB_SCALE(inter_pa_tlv, -2400, 300, 1);
static const DECLARE_TLV_DB_SCALE(lineinrec_tlv, -1050, 150, 0);

#define KYLEW_CODEC_FUNC(xname, xreg)\
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, kylew_func_get, kylew_func_set)

static const struct snd_kcontrol_new dolphin_kylew_controls[] = {
	KYLEW_CODEC_FUNC("Speaker Function", KYLEW_FUNC_SPK),
	KYLEW_CODEC_FUNC("Earpiece Function", KYLEW_FUNC_EAR),
	KYLEW_CODEC_FUNC("HeadPhone Function", KYLEW_FUNC_HP),
	KYLEW_CODEC_FUNC("Line Function", KYLEW_FUNC_LINE),
	KYLEW_CODEC_FUNC("Mic Function", KYLEW_FUNC_MIC),
	KYLEW_CODEC_FUNC("HP Mic Function", KYLEW_FUNC_HP_MIC),
	KYLEW_CODEC_FUNC("Linein Rec Function", KYLEW_FUNC_LINEINREC),
	KYLEW_CODEC_FUNC("Mic Bias Function", KYLEW_FUNC_MIC_BIAS),

	SOC_SINGLE_EXT_TLV("Inter PA Playback Volume",
			   FUN_REG(KYLEW_PGA_INTER_PA), 4,
			   15, 0, kylew_pga_get, kylew_pga_set, inter_pa_tlv),

	SOC_SINGLE_EXT_TLV("LineinRec Capture Volume",
			   FUN_REG(KYLEW_PGA_LINEINREC), 4,
			   7, 1, kylew_pga_get, kylew_pga_set, lineinrec_tlv),
};

static int kylew_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = list_first_entry(&card->codec_dev_list,
						       struct snd_soc_codec,
						       card_list);
	vbc_add_controls(codec);

	kylew_ext_control(&card->dapm);
#if 0
	snd_soc_dapm_ignore_suspend(&card->dapm, "Mic Jack");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HP Mic Jack");
#endif
	snd_soc_dapm_ignore_suspend(&card->dapm, "Line Jack");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Ext Spk");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Ext Ear");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HeadPhone Jack");
	return 0;
}

static struct snd_soc_dai_link kylew_dai[] = {
	{
	 .name = "kylew-vbc",
	 .stream_name = "vbc-dac",

	 .codec_name = "dolphin",
	 .platform_name = "sprd-vbc-pcm-audio",
	 .cpu_dai_name = "vbc",
	 .codec_dai_name = "dolphin-i2s",
	 },
#ifdef CONFIG_SND_SPRD_SOC_VAUDIO
	{
	 .name = "kylew-dsp",
	 .stream_name = "vbc-dsp",

	 .codec_name = "dolphin",
	 .platform_name = "sprd-vbc-pcm-audio",
	 .cpu_dai_name = "vaudio",
	 .codec_dai_name = "dolphin-i2s",
	 },
#endif
};

static struct snd_soc_card kylew_card = {
	.name = "sprdphone",
	.dai_link = kylew_dai,
	.num_links = ARRAY_SIZE(kylew_dai),
	.owner = THIS_MODULE,

	.controls = dolphin_kylew_controls,
	.num_controls = ARRAY_SIZE(dolphin_kylew_controls),
	.dapm_widgets = dolphin_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(dolphin_dapm_widgets),
	.dapm_routes = kylew_audio_map,
	.num_dapm_routes = ARRAY_SIZE(kylew_audio_map),
	.late_probe = kylew_late_probe,
};

static struct platform_device *kylew_snd_device;

static int __init kylew_modinit(void)
{
	int ret;

	kylew_snd_device = platform_device_alloc("soc-audio", -1);
	if (!kylew_snd_device)
		return -ENOMEM;

	platform_set_drvdata(kylew_snd_device, &kylew_card);
	ret = platform_device_add(kylew_snd_device);

	if (ret)
		platform_device_put(kylew_snd_device);

	return ret;
}

static void __exit kylew_modexit(void)
{
	platform_device_unregister(kylew_snd_device);
}

module_init(kylew_modinit);
module_exit(kylew_modexit);

MODULE_DESCRIPTION("ALSA SoC SpreadTrum VBC+dolphin kylew");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("machine:vbc+dolphin");

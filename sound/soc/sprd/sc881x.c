/*
 * sound/soc/sprd/sc881x.c
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
#define pr_fmt(fmt) "[audio:sc881] " fmt

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#include <sound/audio_pa.h>
#include <mach/sprd-audio.h>

int vbc_add_controls(struct snd_soc_codec *codec);

#ifdef CONFIG_SPRD_AUDIO_DEBUG
#define sc881x_dbg pr_debug
#else
#define sc881x_dbg(...)
#endif

#define FUN_REG(f) ((unsigned short)(-(f+1)))

#define SWITCH_FUN_ON    1
#define SWITCH_FUN_OFF   0

enum {
	SC881X_PGA_INTER_PA = 0,
	SC881X_PGA_LINEINREC,

	SC881X_PGA_MAX
};

enum {
	SC881X_FUNC_SPK = 0,
	SC881X_FUNC_EAR,
	SC881X_FUNC_HP,
	SC881X_FUNC_LINE,
	SC881X_FUNC_MIC,
	SC881X_FUNC_HP_MIC,
	SC881X_FUNC_LINEINREC,
	SC881X_FUNC_MIC_BIAS,

	SC881X_FUNC_MAX
};

struct sc881x_pga {
	int (*set) (int pga);
	int (*get) (void);
};

static struct sc881x_pga sc881x_pga_cfg[SC881X_PGA_MAX] = {
	{arch_audio_inter_pa_set_pga,
	 arch_audio_inter_pa_get_pga},
	{arch_audio_codec_lineinrec_set_pga,
	 arch_audio_codec_lineinrec_get_pga},
};

static struct sc881x_priv {
	int func[SC881X_FUNC_MAX];
	int pga[SC881X_PGA_MAX];
} sc881x;

static const char *func_name[SC881X_FUNC_MAX] = {
	"Ext Spk",
	"Ext Ear",
	"HeadPhone Jack",
	"Line Jack",
	"Mic Jack",
	"HP Mic Jack",
	"Line Rec",
	"Mic Bias",
};

static void sc881x_ext_control(struct snd_soc_dapm_context *dapm)
{
	int i;
	for (i = 0; i < SC881X_FUNC_LINEINREC; i++) {
		if (sc881x.func[i] == SWITCH_FUN_ON)
			snd_soc_dapm_enable_pin(dapm, func_name[i]);
		else
			snd_soc_dapm_disable_pin(dapm, func_name[i]);
	}

	if (sc881x.func[SC881X_FUNC_LINEINREC] == SWITCH_FUN_ON) {
		arch_audio_codec_lineinrec_enable();
		snd_soc_dapm_enable_pin(dapm, func_name[SC881X_FUNC_HP_MIC]);
	} else {
		arch_audio_codec_lineinrec_disable();
	}

	if (sc881x.func[SC881X_FUNC_MIC] == SWITCH_FUN_ON)
		snd_soc_dapm_disable_pin(dapm, func_name[SC881X_FUNC_HP_MIC]);

	if (sc881x.func[SC881X_FUNC_MIC_BIAS] == SWITCH_FUN_ON)
		snd_soc_dapm_force_enable_pin(dapm,
					      func_name[SC881X_FUNC_MIC_BIAS]);
	else
		snd_soc_dapm_disable_pin(dapm, func_name[SC881X_FUNC_MIC_BIAS]);

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
		pr_err("sc881x audio inter pa control error: %d\n", enable);
}

static void audio_speaker_enable(int enable)
{
	if (audio_pa_amplifier && audio_pa_amplifier->speaker.control)
		audio_pa_amplifier->speaker.control(enable, NULL);
	else
		local_cpu_pa_control(enable);
}

static int sc881x_hp_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *k, int event)
{
	sc881x_dbg("Entering %s switch %s\n", __func__,
		   SND_SOC_DAPM_EVENT_ON(event) ? "ON" : "OFF");
	if (audio_pa_amplifier && audio_pa_amplifier->headset.control)
		audio_pa_amplifier->headset.
		    control(! !SND_SOC_DAPM_EVENT_ON(event), NULL);
	sc881x_dbg("Leaving %s\n", __func__);
	return 0;
}

static int sc881x_ear_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *k, int event)
{
	sc881x_dbg("Entering %s switch %s\n", __func__,
		   SND_SOC_DAPM_EVENT_ON(event) ? "ON" : "OFF");
	if (audio_pa_amplifier && audio_pa_amplifier->earpiece.control)
		audio_pa_amplifier->earpiece.
		    control(! !SND_SOC_DAPM_EVENT_ON(event), NULL);
	sc881x_dbg("Leaving %s\n", __func__);
	return 0;
}

static int sc881x_amp_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *k, int event)
{
	sc881x_dbg("Entering %s switch %s\n", __func__,
		   SND_SOC_DAPM_EVENT_ON(event) ? "ON" : "OFF");
	audio_speaker_enable(! !SND_SOC_DAPM_EVENT_ON(event));
	sc881x_dbg("Leaving %s\n", __func__);
	return 0;
}

static const struct snd_soc_dapm_widget dolphin_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_MIC("HP Mic Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", sc881x_amp_event),
	SND_SOC_DAPM_SPK("Ext Ear", sc881x_ear_event),
	SND_SOC_DAPM_LINE("Line Jack", NULL),
	SND_SOC_DAPM_HP("HeadPhone Jack", sc881x_hp_event),
};

/* sc881x supported audio map */
static const struct snd_soc_dapm_route sc881x_audio_map[] = {

	{"HeadPhone Jack", NULL, "HEAD_P"},
	{"Ext Spk", NULL, "AUXSP"},
	{"Ext Ear", NULL, "EAR"},
	{"MIC", NULL, "Mic Jack"},
	{"AUXMIC", NULL, "HP Mic Jack"},
	{"AI", NULL, "Line Jack"},
};

static inline int sc881x_func_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	ucontrol->value.integer.value[0] = sc881x.func[id];
	return 0;
}

static int sc881x_func_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_card *card = codec->card;
	int id = FUN_REG(mc->reg);

	pr_info("%s switch %s\n", func_name[id],
		ucontrol->value.integer.value[0] ? "ON" : "OFF");

	if (sc881x.func[id] == ucontrol->value.integer.value[0])
		return 0;

	sc881x.func[id] = ucontrol->value.integer.value[0];
	sc881x_ext_control(&card->dapm);
	sc881x_dbg("Leaving %s\n", __func__);
	return 1;
}

static int sc881x_pga_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int ret;
	int val;
	int id = FUN_REG(mc->reg);

	ret = sc881x_pga_cfg[id].get();
	if (ret < 0)
		return ret;
	sc881x.pga[id] = ret;
	if (mc->invert) {
		val = mc->max - ret;
	} else {
		val = ret;
	}
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int sc881x_pga_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int ret;
	int val;
	int id = FUN_REG(mc->reg);
	unsigned int mask = (1 << fls(mc->max)) - 1;

	sc881x_dbg("Entering %s\n", __func__);
	if (0 == sc881x.pga[id]) {
		ret = sc881x_pga_cfg[id].get();
		if (ret < 0)
			return ret;
		sc881x.pga[id] = ret;
	}

	val = ucontrol->value.integer.value[0] & mask;
	if (mc->invert) {
		val = mc->max - val;
	}

	if (sc881x.pga[id] == val)
		return 0;

	sc881x.pga[id] = val;
	ret = sc881x_pga_cfg[id].set(sc881x.pga[id]);
	sc881x_dbg("pga %d val %d\n", id, sc881x.pga[id]);
	sc881x_dbg("Leaving %s\n", __func__);
	return 1;
}

static const DECLARE_TLV_DB_SCALE(inter_pa_tlv, -2400, 300, 1);
static const DECLARE_TLV_DB_SCALE(lineinrec_tlv, -1050, 150, 0);

#define SC881X_CODEC_FUNC(xname, xreg)\
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, sc881x_func_get, sc881x_func_set)

static const struct snd_kcontrol_new dolphin_sc881x_controls[] = {
	SC881X_CODEC_FUNC("Speaker Function", SC881X_FUNC_SPK),
	SC881X_CODEC_FUNC("Earpiece Function", SC881X_FUNC_EAR),
	SC881X_CODEC_FUNC("HeadPhone Function", SC881X_FUNC_HP),
	SC881X_CODEC_FUNC("Line Function", SC881X_FUNC_LINE),
	SC881X_CODEC_FUNC("Mic Function", SC881X_FUNC_MIC),
	SC881X_CODEC_FUNC("HP Mic Function", SC881X_FUNC_HP_MIC),
	SC881X_CODEC_FUNC("Linein Rec Function", SC881X_FUNC_LINEINREC),
	SC881X_CODEC_FUNC("Mic Bias Function", SC881X_FUNC_MIC_BIAS),

	SOC_SINGLE_EXT_TLV("Inter PA Playback Volume",
			   FUN_REG(SC881X_PGA_INTER_PA), 4,
			   15, 0, sc881x_pga_get, sc881x_pga_set, inter_pa_tlv),

	SOC_SINGLE_EXT_TLV("LineinRec Capture Volume",
			   FUN_REG(SC881X_PGA_LINEINREC), 4,
			   7, 1, sc881x_pga_get, sc881x_pga_set, lineinrec_tlv),
};

static int sc881x_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = list_first_entry(&card->codec_dev_list,
						       struct snd_soc_codec,
						       card_list);
	vbc_add_controls(codec);

	sc881x_ext_control(&card->dapm);
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

static struct snd_soc_dai_link sc881x_dai[] = {
	{
	 .name = "sc881x-vbc",
	 .stream_name = "vbc-dac",

	 .codec_name = "dolphin",
	 .platform_name = "sprd-vbc-pcm-audio",
	 .cpu_dai_name = "vbc",
	 .codec_dai_name = "dolphin-i2s",
	 },
#ifdef CONFIG_SND_SPRD_SOC_VAUDIO
	{
	 .name = "sc881x-dsp",
	 .stream_name = "vbc-dsp",

	 .codec_name = "dolphin",
	 .platform_name = "sprd-vbc-pcm-audio",
	 .cpu_dai_name = "vaudio",
	 .codec_dai_name = "dolphin-i2s",
	 },
#endif
};

static struct snd_soc_card sc881x_card = {
	.name = "sprdphone",
	.dai_link = sc881x_dai,
	.num_links = ARRAY_SIZE(sc881x_dai),
	.owner = THIS_MODULE,

	.controls = dolphin_sc881x_controls,
	.num_controls = ARRAY_SIZE(dolphin_sc881x_controls),
	.dapm_widgets = dolphin_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(dolphin_dapm_widgets),
	.dapm_routes = sc881x_audio_map,
	.num_dapm_routes = ARRAY_SIZE(sc881x_audio_map),
	.late_probe = sc881x_late_probe,
};

static struct platform_device *sc881x_snd_device;

static int __init sc881x_modinit(void)
{
	int ret;

	sc881x_snd_device = platform_device_alloc("soc-audio", -1);
	if (!sc881x_snd_device)
		return -ENOMEM;

	platform_set_drvdata(sc881x_snd_device, &sc881x_card);
	ret = platform_device_add(sc881x_snd_device);

	if (ret)
		platform_device_put(sc881x_snd_device);

	return ret;
}

static void __exit sc881x_modexit(void)
{
	platform_device_unregister(sc881x_snd_device);
}

module_init(sc881x_modinit);
module_exit(sc881x_modexit);

MODULE_DESCRIPTION("ALSA SoC SpreadTrum VBC+dolphin sc881x");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("machine:vbc+dolphin");

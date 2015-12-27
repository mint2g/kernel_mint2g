/*
 * sound/soc/sprd/dai/vbc/vbc.c
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "[audio: vbc ] " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/workqueue.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include "sprd-vbc-pcm.h"
#include "vbc.h"

#ifndef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
#define CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
#endif

#ifdef CONFIG_SPRD_AUDIO_DEBUG
#define vbc_dbg pr_debug
#else
#define vbc_dbg(...)
#endif

#define FUN_REG(f) ((unsigned short)(-((f) + 1)))

enum {
	VBC_LEFT = 0,
	VBC_RIGHT = 1,
};

#define VBC_DG_VAL_MAX (0x7F)

struct vbc_fw_header {
	char magic[VBC_EQ_FIRMWARE_MAGIC_LEN];
	u32 profile_version;
	u32 num_profile;
};

struct vbc_eq_profile {
	char magic[VBC_EQ_FIRMWARE_MAGIC_LEN];
	char name[VBC_EQ_PROFILE_NAME_MAX];
	/* TODO */
	u32 effect_paras[VBC_EFFECT_PARAS_LEN];
};

static const u32 vbc_eq_profile_default[VBC_EFFECT_PARAS_LEN] = {
/* TODO the default register value */
	0x00000000,		/*  DAPATCHCTL      */
	0x00001818,		/*  DADGCTL         */
	0x0000007F,		/*  DAHPCTL         */
	0x00000000,		/*  DAALCCTL0       */
	0x00000000,		/*  DAALCCTL1       */
	0x00000000,		/*  DAALCCTL2       */
	0x00000000,		/*  DAALCCTL3       */
	0x00000000,		/*  DAALCCTL4       */
	0x00000000,		/*  DAALCCTL5       */
	0x00000000,		/*  DAALCCTL6       */
	0x00000000,		/*  DAALCCTL7       */
	0x00000000,		/*  DAALCCTL8       */
	0x00000000,		/*  DAALCCTL9       */
	0x00000000,		/*  DAALCCTL10      */
	0x00000183,		/*  STCTL0          */
	0x00000183,		/*  STCTL1          */
	0x00000000,		/*  ADPATCHCTL      */
	0x00001818,		/*  ADDGCTL         */
	0x00000000,		/*  HPCOEF0         */
};

#ifndef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
struct vbc_eq_delayed_work {
	struct workqueue_struct *workqueue;
	struct delayed_work delayed_work;
	struct snd_soc_codec *codec;
};
#endif

struct vbc_equ {
	struct device *dev;
	int is_active;
	int is_loaded;
	int is_loading;
	struct snd_soc_dai *codec_dai;
	int now_profile;
	struct vbc_fw_header hdr;
	struct vbc_eq_profile *data;
	void (*vbc_eq_apply) (struct snd_soc_dai * codec_dai, void *data);
#ifndef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
	int is_active_loaded;
	struct soc_enum equalizer_enum;
	struct snd_kcontrol_new equalizer_control;
	struct vbc_eq_delayed_work *delay_work;
#endif
};

typedef int (*vbc_dma_set) (int enable);
typedef int (*vbc_dg_set) (int enable, int dg);
static DEFINE_MUTEX(vbc_mutex);
struct vbc_priv {
	vbc_dma_set dma_set[2];

	int (*arch_enable) (int chan);
	int (*arch_disable) (int chan);
	int is_open;
	int is_active;
	int used_chan_count;
	int dg_switch[2];
	int dg_val[2];
	vbc_dg_set dg_set[2];
};

static DEFINE_MUTEX(load_mutex);
static struct vbc_equ vbc_eq_setting = { 0 };

static void vbc_eq_try_apply(struct snd_soc_dai *codec_dai);
#ifndef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
static void vbc_eq_delay_work(struct work_struct *work);
#endif
static struct vbc_priv vbc[2];
static struct clk *s_vbc_clk = 0;
static struct sprd_pcm_dma_params vbc_pcm_stereo_out = {
	.name = "VBC PCM Stereo out",
	.workmode = DMA_LINKLIST,
	.irq_type = TRANSACTION_DONE,
	.desc = {
		 .cfg_req_mode_sel = DMA_REQMODE_NORMAL,
		 .cfg_src_data_width = DMA_SDATA_WIDTH16,
		 .cfg_dst_data_width = DMA_DDATA_WIDTH16,
		 .src_burst_mode = SRC_BURST_MODE_4,
		 .dst_burst_mode = SRC_BURST_MODE_SINGLE,
		 },
#ifdef CONFIG_SPRD_VBC_LR_INVERT
	.dev_paddr = {PHYS_VBDA1, PHYS_VBDA0},
#else
	.dev_paddr = {PHYS_VBDA0, PHYS_VBDA1},
#endif
};

static struct sprd_pcm_dma_params vbc_pcm_stereo_in = {
	.name = "VBC PCM Stereo in",
	.workmode = DMA_LINKLIST,
	.irq_type = TRANSACTION_DONE,
	.desc = {
		 .cfg_req_mode_sel = DMA_REQMODE_NORMAL,
		 .cfg_src_data_width = DMA_SDATA_WIDTH16,
		 .cfg_dst_data_width = DMA_DDATA_WIDTH16,
		 .src_burst_mode = SRC_BURST_MODE_SINGLE,
		 .dst_burst_mode = SRC_BURST_MODE_4,
		 },
	.dev_paddr = {PHYS_VBAD0, PHYS_VBAD1},
};

static inline void vbc_safe_mem_release(void **free)
{
	if (*free) {
		kfree(*free);
		*free = NULL;
	}
}

#define vbc_safe_kfree(p) vbc_safe_mem_release((void**)p)

static DEFINE_SPINLOCK(vbc_lock);
/* local register setting */
static int vbc_reg_write(int reg, int val, int mask)
{
	int tmp, ret;
	spin_lock(&vbc_lock);
	tmp = __raw_readl(reg);
	ret = tmp;
	tmp &= ~(mask);
	tmp |= val & mask;
	__raw_writel(tmp, reg);
	spin_unlock(&vbc_lock);
	return ret & (mask);
}

static inline int vbc_reg_read(int reg)
{
	int tmp;
	tmp = __raw_readl(reg);
	return tmp;
}

static int vbc_set_buffer_size(int ad_buffer_size, int da_buffer_size)
{
	int val = vbc_reg_read(VBBUFFSIZE);
	WARN_ON(ad_buffer_size > VBC_FIFO_FRAME_NUM);
	WARN_ON(da_buffer_size > VBC_FIFO_FRAME_NUM);
	if ((ad_buffer_size > 0)
	    && (ad_buffer_size <= VBC_FIFO_FRAME_NUM)) {
		val &= ~(VBADBUFFERSIZE_MASK);
		val |= (((ad_buffer_size - 1) << VBADBUFFERSIZE_SHIFT)
			& VBADBUFFERSIZE_MASK);
	}
	if ((da_buffer_size > 0)
	    && (da_buffer_size <= VBC_FIFO_FRAME_NUM)) {
		val &= ~(VBDABUFFERSIZE_MASK);
		val |= (((da_buffer_size - 1) << VBDABUFFERSIZE_SHIFT)
			& VBDABUFFERSIZE_MASK);
	}
	vbc_reg_write(VBBUFFSIZE, val,
		      (VBDABUFFERSIZE_MASK | VBADBUFFERSIZE_MASK));
	return 0;
}

static inline int vbc_sw_write_buffer(int enable)
{
	/* Software access ping-pong buffer enable when VBENABE bit low */
	vbc_reg_write(VBDABUFFDTA, ((enable ? 1 : 0) << RAMSW_EN),
		      (1 << RAMSW_EN));
	return 0;
}

static inline int vbc_enable_set(int enable)
{
	vbc_reg_write(VBADBUFFDTA, (0 << VBIIS_LRCK), (1 << VBIIS_LRCK));
	vbc_reg_write(VBDABUFFDTA, ((enable ? 1 : 0) << VBENABLE),
		      (1 << VBENABLE));
	return 0;
}

static inline int vbc_ad0_dma_set(int enable)
{
	vbc_reg_write(VBDABUFFDTA, ((enable ? 1 : 0) << VBAD0DMA_EN),
		      (1 << VBAD0DMA_EN));
	return 0;
}

static inline int vbc_ad1_dma_set(int enable)
{
	vbc_reg_write(VBDABUFFDTA, ((enable ? 1 : 0) << VBAD1DMA_EN),
		      (1 << VBAD1DMA_EN));
	return 0;
}

static inline int vbc_da0_dma_set(int enable)
{
	vbc_reg_write(VBDABUFFDTA, ((enable ? 1 : 0) << VBDA0DMA_EN),
		      (1 << VBDA0DMA_EN));
	return 0;
}

static inline int vbc_da1_dma_set(int enable)
{
	vbc_reg_write(VBDABUFFDTA, ((enable ? 1 : 0) << VBDA1DMA_EN),
		      (1 << VBDA1DMA_EN));
	return 0;
}

static void vbc_da_buffer_clear(int id)
{
	int i;
	vbc_reg_write(VBDABUFFDTA, ((id ? 1 : 0) << RAMSW_NUMB),
		      (1 << RAMSW_NUMB));
	for (i = 0; i < VBC_FIFO_FRAME_NUM; i++) {
		__raw_writel(0, VBDA0);
		__raw_writel(0, VBDA1);
	}
}

static void vbc_da_buffer_clear_all(struct snd_soc_dai *dai)
{
	int i;
	int ret;
	int save_enable = 0;
	ret = arch_audio_vbc_enable();
	if (ret < 0) {
		pr_err("Failed to enable VBC\n");
	}
	save_enable |= (ret << 2);
	for (i = 0; i < 2; i++) {
		ret = arch_audio_vbc_da_enable(i);
		if (ret < 0) {
			pr_err("Failed to enable VBC DA%d\n", i);
		}
		save_enable |= (ret << i);
	}
	vbc_sw_write_buffer(true);
	vbc_set_buffer_size(0, VBC_FIFO_FRAME_NUM);
	vbc_da_buffer_clear(1);	/* clear data buffer 1 */
	vbc_da_buffer_clear(0);	/* clear data buffer 0 */
	vbc_sw_write_buffer(false);
	for (i = 0; i < 2; i++, save_enable >>= 1) {
		if (save_enable & 0x1) {
			arch_audio_vbc_da_disable(i);
		}
	}
	save_enable >>= 1;
	if (save_enable & 0x1) {
		arch_audio_vbc_disable();
	}
}

static inline int vbc_str_2_index(int stream);

static int vbc_da_arch_enable(int chan)
{
	int ret;
	ret = arch_audio_vbc_da_enable(chan);
	if (ret < 0) {
		pr_err("VBC da enable error:%i\n", ret);
		return ret;
	} else {
		arch_audio_vbc_enable();
		vbc[vbc_str_2_index(SNDRV_PCM_STREAM_PLAYBACK)].is_active = 1;
	}
	return ret;
}

static int vbc_da_arch_disable(int chan)
{
	int ret;
	ret = arch_audio_vbc_da_disable(chan);
	if (ret < 0) {
		pr_err("VBC da disable error:%i\n", ret);
		return ret;
	} else {
		vbc[vbc_str_2_index(SNDRV_PCM_STREAM_PLAYBACK)].is_active = 0;
	}
	return ret;
}

static int vbc_ad_arch_enable(int chan)
{
	int ret;
	ret = arch_audio_vbc_ad_enable(chan);
	if (ret < 0) {
		pr_err("VBC ad enable error:%i\n", ret);
		return ret;
	} else {
		arch_audio_vbc_enable();
		vbc[vbc_str_2_index(SNDRV_PCM_STREAM_CAPTURE)].is_active = 1;
	}
	return ret;
}

static int vbc_ad_arch_disable(int chan)
{
	int ret;
	ret = arch_audio_vbc_ad_disable(chan);
	if (ret < 0) {
		pr_err("VBC ad disable error:%i\n", ret);
		return ret;
	} else {
		vbc[vbc_str_2_index(SNDRV_PCM_STREAM_CAPTURE)].is_active = 0;
	}
	return ret;
}

static inline int vbc_da0_dg_set(int enable, int dg)
{
	if (enable) {
		vbc_reg_write(DADGCTL, 0x80 | (0xFF & dg), 0xFF);
	} else {
		vbc_reg_write(DADGCTL, 0, 0x80);
	}
	return 0;
}

static inline int vbc_da1_dg_set(int enable, int dg)
{
	if (enable) {
		vbc_reg_write(DADGCTL, (0x80 | (0xFF & dg)) << 8, 0xFF00);
	} else {
		vbc_reg_write(DADGCTL, 0, 0x8000);
	}
	return 0;
}

static inline int vbc_ad0_dg_set(int enable, int dg)
{
	if (enable) {
		vbc_reg_write(ADDGCTL, 0x80 | (0xFF & dg), 0xFF);
	} else {
		vbc_reg_write(ADDGCTL, 0, 0x80);
	}
	return 0;
}

static inline int vbc_ad1_dg_set(int enable, int dg)
{
	if (enable) {
		vbc_reg_write(ADDGCTL, (0x80 | (0xFF & dg)) << 8, 0xFF00);
	} else {
		vbc_reg_write(ADDGCTL, 0, 0x8000);
	}
	return 0;
}

static int vbc_try_dg_set(int vbc_idx, int id)
{
	int dg = vbc[vbc_idx].dg_val[id];
	if (vbc[vbc_idx].dg_switch[id]) {
		vbc[vbc_idx].dg_set[id] (1, dg);
	} else {
		vbc[vbc_idx].dg_set[id] (0, dg);
	}
	return 0;
}

static struct vbc_priv vbc[2] = {
	{			/*PlayBack */
	 .dma_set = {vbc_da0_dma_set, vbc_da1_dma_set},
	 .arch_enable = vbc_da_arch_enable,
	 .arch_disable = vbc_da_arch_disable,
	 .is_active = 0,
	 .dg_switch = {0, 0},
	 .dg_val = {0x18, 0x18},
	 .dg_set = {vbc_da0_dg_set, vbc_da1_dg_set},
	 },
	{			/*Capture */
	 .dma_set = {vbc_ad0_dma_set, vbc_ad1_dma_set},
	 .arch_enable = vbc_ad_arch_enable,
	 .arch_disable = vbc_ad_arch_disable,
	 .is_active = 0,
	 .dg_switch = {0, 0},
	 .dg_val = {0x18, 0x18},
	 .dg_set = {vbc_ad0_dg_set, vbc_ad1_dg_set},
	 },
};

/* NOTE:
   this index need use for the [struct vbc_priv] vbc[2] index
   default MUST return 0.
 */
static inline int vbc_str_2_index(int stream)
{
	if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		return 1;
	}
	return 0;
}

static inline void vbc_reg_enable(void)
{
	if (s_vbc_clk) {
		clk_enable(s_vbc_clk);
	} else {
		arch_audio_vbc_reg_enable();
	}
}

static int vbc_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	int vbc_idx;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	vbc_dbg("Entering %s\n", __func__);
	vbc_idx = vbc_str_2_index(substream->stream);

	if (vbc[vbc_idx].is_open || vbc[vbc_idx].is_active) {
		pr_err("vbc is actived:%d\n", substream->stream);
	}

	mutex_lock(&vbc_mutex);
	vbc[vbc_idx].is_open = 1;
	mutex_unlock(&vbc_mutex);

	vbc_reg_enable();

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		vbc_da_buffer_clear_all(dai);
		vbc_set_buffer_size(0, VBC_FIFO_FRAME_NUM);
		vbc_eq_setting.codec_dai = codec_dai;
		vbc_eq_try_apply(codec_dai);
	} else {
		vbc_set_buffer_size(VBC_FIFO_FRAME_NUM, 0);
	}

	vbc_try_dg_set(vbc_idx, VBC_LEFT);
	vbc_try_dg_set(vbc_idx, VBC_RIGHT);

	WARN_ON(!vbc[vbc_idx].arch_enable);
	WARN_ON(!vbc[vbc_idx].arch_disable);
	WARN_ON(!vbc[vbc_idx].dma_set[0]);
	WARN_ON(!vbc[vbc_idx].dma_set[1]);

	vbc_dbg("Leaving %s\n", __func__);
	return 0;
}

static inline int vbc_can_close(void)
{
	return !(vbc[vbc_str_2_index(SNDRV_PCM_STREAM_PLAYBACK)].is_open
		 || vbc[vbc_str_2_index(SNDRV_PCM_STREAM_CAPTURE)].is_open);
}

static void vbc_shutdown(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	int vbc_idx;
	int i;

	vbc_dbg("Entering %s\n", __func__);

	/* vbc da close MUST clear da buffer */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		vbc_da_buffer_clear_all(dai);
		vbc_eq_setting.codec_dai = 0;
	}

	vbc_idx = vbc_str_2_index(substream->stream);

	for (i = 0; i < 2; i++) {
		vbc[vbc_idx].arch_disable(i);
		vbc[vbc_idx].dma_set[i] (0);
	}

	mutex_lock(&vbc_mutex);
	vbc[vbc_idx].is_open = 0;
	if (vbc_can_close()) {
		vbc_enable_set(0);
		arch_audio_vbc_reset();
		arch_audio_vbc_disable();
		if (!s_vbc_clk) {
			arch_audio_vbc_reg_disable();
		}
		pr_info("close the VBC\n");
	}

	mutex_unlock(&vbc_mutex);

	if (s_vbc_clk) {
		clk_disable(s_vbc_clk);
	}

	vbc_dbg("Leaving %s\n", __func__);
}

static int vbc_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	int vbc_idx;
	struct sprd_pcm_dma_params *dma_data;

	vbc_dbg("Entering %s\n", __func__);

	vbc_idx = vbc_str_2_index(substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = &vbc_pcm_stereo_out;
	else
		dma_data = &vbc_pcm_stereo_in;

	snd_soc_dai_set_dma_data(dai, substream, dma_data);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	default:
		pr_err("vbc only supports format SNDRV_PCM_FORMAT_S16_LE\n");
		break;
	}

	vbc[vbc_idx].used_chan_count = params_channels(params);
	if (vbc[vbc_idx].used_chan_count > 2) {
		pr_err("vbc can not supports grate 2 channels\n");
	}

	vbc_dbg("Leaving %s\n", __func__);
	return 0;
}

static int vbc_trigger(struct snd_pcm_substream *substream, int cmd,
		       struct snd_soc_dai *dai)
{
	int vbc_idx;
	int ret = 0;
	int i;

#if 0
	vbc_dbg("Entering %s\n", __func__);
#endif

	vbc_idx = vbc_str_2_index(substream->stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		for (i = 0; i < vbc[vbc_idx].used_chan_count; i++) {
			vbc[vbc_idx].arch_enable(i);
			vbc[vbc_idx].dma_set[i] (1);
		}
		vbc_enable_set(1);

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		for (i = 0; i < vbc[vbc_idx].used_chan_count; i++) {
			vbc[vbc_idx].arch_disable(i);
			vbc[vbc_idx].dma_set[i] (0);
		}

		if (vbc_can_close()) {
			vbc_enable_set(0);
			arch_audio_vbc_disable();
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

#if 0
	vbc_dbg("Leaving %s\n", __func__);
#endif
	return ret;
}

static struct snd_soc_dai_ops vbc_dai_ops = {
	.startup = vbc_startup,
	.shutdown = vbc_shutdown,
	.hw_params = vbc_hw_params,
	.trigger = vbc_trigger,
};

struct snd_soc_dai_driver vbc_dai = {
	.playback = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	.capture = {
		    .channels_min = 1,
		    .channels_max = 2,
		    .rates = SNDRV_PCM_RATE_CONTINUOUS,
		    .rate_max = 96000,
		    .formats = SNDRV_PCM_FMTBIT_S16_LE,
		    },
	.ops = &vbc_dai_ops,
};

static int vbc_drv_probe(struct platform_device *pdev)
{
	int i;
	int ret;
	struct clk *vbc_clk;
#ifndef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
	struct vbc_eq_delayed_work *delay_work = 0;
#endif

	vbc_dbg("Entering %s\n", __func__);

	for (i = 0; i < 2; i++) {
#ifdef CONFIG_SPRD_VBC_LR_INVERT
		vbc_pcm_stereo_out.channels[i] = arch_audio_vbc_da_dma_info(1 - i);
#else
		vbc_pcm_stereo_out.channels[i] = arch_audio_vbc_da_dma_info(i);
#endif
		vbc_pcm_stereo_in.channels[i] = arch_audio_vbc_ad_dma_info(i);
	}

	vbc_eq_setting.dev = &pdev->dev;

	ret = snd_soc_register_dai(&pdev->dev, &vbc_dai);

	if (ret < 0) {
		pr_err("%s err!\n", __func__);
		goto probe_err;
	}
#ifndef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
	delay_work = kzalloc(sizeof(struct vbc_eq_delayed_work), GFP_KERNEL);
	if (!delay_work) {
		ret = -ENOMEM;
		goto probe_err;
	}

	delay_work->workqueue = create_singlethread_workqueue("vbc_eq_wq");
	if (!delay_work->workqueue) {
		ret = -ENOMEM;
		goto work_err;
	}

	INIT_DELAYED_WORK(&delay_work->delayed_work, vbc_eq_delay_work);
	vbc_eq_setting.delay_work = delay_work;
#endif

	vbc_clk = clk_get(&pdev->dev, "clk_vbc");
	if (IS_ERR(vbc_clk)) {
		pr_err("Cannot request clk_vbc\n");
	} else {
		s_vbc_clk = vbc_clk;
	}

#ifndef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
	goto probe_err;

work_err:
	kfree(delay_work);
#endif
probe_err:
	vbc_dbg("return %i\n", ret);
	vbc_dbg("Leaving %s\n", __func__);

	return ret;
}

static int __devexit vbc_drv_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	vbc_eq_setting.dev = 0;
#ifndef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
	destroy_workqueue(vbc_eq_setting.delay_work->workqueue);
	vbc_safe_kfree(&vbc_eq_setting.delay_work);
	vbc_safe_kfree(&vbc_eq_setting.equalizer_enum.values);
#endif
	vbc_safe_kfree(&vbc_eq_setting.data);
	if (s_vbc_clk) {
		clk_put(s_vbc_clk);
		s_vbc_clk = 0;
	}
	return 0;
}

static struct platform_driver vbc_driver = {
	.probe = vbc_drv_probe,
	.remove = __devexit_p(vbc_drv_remove),

	.driver = {
		   .name = "vbc",
		   .owner = THIS_MODULE,
		   },
};

/*
 * proc interface
 */

#ifdef CONFIG_PROC_FS
static void vbc_proc_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	int reg;

	snd_iprintf(buffer, "vbc register dump\n");
	for (reg = ARM_VB_BASE + 0x10 ; reg < ARM_VB_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (reg - ARM_VB_BASE)
			    , vbc_reg_read(reg + 0x00)
			    , vbc_reg_read(reg + 0x04)
			    , vbc_reg_read(reg + 0x08)
			    , vbc_reg_read(reg + 0x0C)
		    );
	}
}

static void vbc_proc_init(struct snd_soc_codec *codec)
{
	struct snd_info_entry *entry;

	if (!snd_card_proc_new(codec->card->snd_card, "vbc", &entry))
		snd_info_set_text_ops(entry, NULL, vbc_proc_read);
}
#else /* !CONFIG_PROC_FS */
static inline void vbc_proc_init(struct snd_soc_codec *codec)
{
}
#endif

static int vbc_eq_reg_offset(u32 reg)
{
	int i = 0;
	if ((reg >= DAPATCHCTL) && (reg <= ADDGCTL)) {
		i = (reg - DAPATCHCTL) >> 2;
	} else if ((reg >= HPCOEF0) && (reg <= HPCOEF42)) {
		i = ((reg - HPCOEF0) >> 2) + ((ADDGCTL - DAPATCHCTL) >> 2) + 1;
	}
	BUG_ON(i >= VBC_EFFECT_PARAS_LEN);
	return i;
}

static inline void vbc_eq_reg_set(u32 reg, void *data)
{
	u32 *effect_paras = data;
	vbc_dbg("reg(0x%x) = (0x%x)\n", reg,
		effect_paras[vbc_eq_reg_offset(reg)]);
	__raw_writel(effect_paras[vbc_eq_reg_offset(reg)], reg);
}

static inline void vbc_eq_reg_set_range(u32 reg_start, u32 reg_end, void *data)
{
	u32 reg_addr;
	for (reg_addr = reg_start; reg_addr <= reg_end; reg_addr += 4) {
		vbc_eq_reg_set(reg_addr, data);
	}
}

static void vbc_eq_reg_apply(struct snd_soc_dai *codec_dai, void *data)
{
	vbc_eq_reg_set_range(DAALCCTL0, DAALCCTL10, data);
	vbc_eq_reg_set_range(HPCOEF0, HPCOEF42, data);

	vbc_eq_reg_set(DAHPCTL, data);
	vbc_eq_reg_set(DAPATCHCTL, data);

	vbc_eq_reg_set(STCTL0, data);
	vbc_eq_reg_set(STCTL1, data);

	vbc_eq_reg_set(ADPATCHCTL, data);
}

static void vbc_eq_profile_apply(struct snd_soc_dai *codec_dai, void *data)
{
	if (vbc_eq_setting.codec_dai) {
		vbc_eq_reg_apply(codec_dai, data);
	}
}

static void vbc_eq_profile_close(void)
{
	vbc_eq_profile_apply(vbc_eq_setting.codec_dai, &vbc_eq_profile_default);
}

static void vbc_eq_try_apply(struct snd_soc_dai *codec_dai)
{
	u32 *data;
	vbc_dbg("Entering %s 0x%x\n", __func__,
		(int)vbc_eq_setting.vbc_eq_apply);
	if (vbc_eq_setting.vbc_eq_apply) {
		mutex_lock(&load_mutex);
		if (vbc_eq_setting.is_loaded) {
			struct vbc_eq_profile *now =
			    &vbc_eq_setting.data[vbc_eq_setting.now_profile];
			data = now->effect_paras;
			pr_info("vbc eq apply '%s'\n", now->name);
			vbc_eq_setting.vbc_eq_apply(codec_dai, data);
		}
		mutex_unlock(&load_mutex);
	}
	vbc_dbg("Leaving %s\n", __func__);
}

static int vbc_eq_profile_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = vbc_eq_setting.now_profile;
	return 0;
}

static int vbc_eq_profile_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;

	pr_info("vbc eq select %ld max %d\n", ucontrol->value.integer.value[0],
		vbc_eq_setting.hdr.num_profile);

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc_eq_setting.now_profile) {
		return ret;
	}
	if (ret < vbc_eq_setting.hdr.num_profile) {
		vbc_eq_setting.now_profile = ret;
	}

	vbc_eq_try_apply(vbc_eq_setting.codec_dai);

	vbc_dbg("Leaving %s\n", __func__);
	return ret;
}

#ifndef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
int snd_soc_info_enum_ext1(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	char **dtexts;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	dtexts = (char **)e->values;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = e->max;

	if (uinfo->value.enumerated.item > e->max - 1)
		uinfo->value.enumerated.item = e->max - 1;
	strcpy(uinfo->value.enumerated.name,
	       dtexts[uinfo->value.enumerated.item]);
	return 0;
}

/**
 * vbc_replace_controls - code copy from snd_soc_add_controls
 */
static int vbc_replace_controls(struct snd_soc_codec *codec,
				const struct snd_kcontrol_new *controls,
				int num_controls)
{
	struct snd_card *card = codec->card->snd_card;
	int err, i;

	for (i = 0; i < num_controls; i++) {
		const struct snd_kcontrol_new *control = &controls[i];
		err = snd_ctl_replace(card, snd_soc_cnew(control, codec,
							 control->name,
							 codec->name_prefix),
				      true);
		if (err < 0) {
			vbc_dbg("%s: Failed to add %s: %d\n",
				codec->name, control->name, err);
			return err;
		}
	}

	return 0;
}

static void vbc_eq_delay_work(struct work_struct *work)
{
	struct vbc_eq_delayed_work *delay_work = container_of(work,
							      struct
							      vbc_eq_delayed_work,
							      delayed_work.
							      work);
	struct snd_soc_codec *codec = delay_work->codec;
	int ret;
	ret = vbc_replace_controls(codec, &vbc_eq_setting.equalizer_control, 1);
	if (ret < 0)
		pr_err("add the vbc eq profile failed");
	else if (vbc_eq_setting.is_active_loaded) {
		vbc_eq_setting.is_loaded = 1;
		vbc_eq_setting.is_active_loaded = 0;
		vbc_eq_try_apply(vbc_eq_setting.codec_dai);
	}
}

static int vbc_eq_profile_add_action(struct snd_soc_codec *codec)
{
	struct vbc_eq_delayed_work *delay_work;
	delay_work = vbc_eq_setting.delay_work;
	delay_work->codec = codec;
	queue_delayed_work(delay_work->workqueue, &delay_work->delayed_work,
			   msecs_to_jiffies(5));
	return 0;
}

static int vbc_eq_profile_add_widgets(struct snd_soc_codec *codec)
{
	struct soc_enum *equalizer_enum = &vbc_eq_setting.equalizer_enum;
	struct snd_kcontrol_new *equalizer_control =
	    &vbc_eq_setting.equalizer_control;
	char **dtexts;
	int count;
	int j;
	int ret = 0;

	vbc_eq_setting.is_loaded = 0;
	vbc_eq_setting.is_active_loaded = 0;
	vbc_dbg("Entering %s %d\n", __func__, vbc_eq_setting.is_loading);
	if (vbc_eq_setting.is_loading) {
		count = vbc_eq_setting.hdr.num_profile;
	} else {
		count = 1;
	}

	equalizer_enum->max = count;
	dtexts = kzalloc(count * sizeof(char *), GFP_KERNEL);
	if (dtexts == NULL) {
		ret = -ENOMEM;
		goto err_texts;
	}

	if (vbc_eq_setting.is_loading) {
		for (j = 0; j < count; j++) {
			vbc_dbg("profile[%d] name is %s\n", j,
				vbc_eq_setting.data[j].name);
			dtexts[j] = vbc_eq_setting.data[j].name;
		}
		vbc_eq_setting.is_active_loaded = 1;
	} else {
		static char default_str[] = "null";
		dtexts[0] = default_str;
	}
	vbc_safe_kfree(&equalizer_enum->values);
	equalizer_enum->values = (const unsigned int *)dtexts;

	equalizer_control->name = "VBC EQ Profile Select";
	equalizer_control->private_value = (unsigned long)equalizer_enum;
	equalizer_control->get = vbc_eq_profile_get;
	equalizer_control->put = vbc_eq_profile_put;
	equalizer_control->info = snd_soc_info_enum_ext1;
	equalizer_control->iface = SNDRV_CTL_ELEM_IFACE_MIXER;

	ret = vbc_eq_profile_add_action(codec);

err_texts:
	vbc_dbg("return %i\n", ret);
	vbc_dbg("Leaving %s\n", __func__);
	return ret;
}
#endif

static int vbc_eq_loading(struct snd_soc_codec *codec)
{
	int ret;
	const u8 *fw_data;
	const struct firmware *fw;
	int i;
	int offset = 0;
	int len = 0;
	int old_num_profile;

	vbc_dbg("Entering %s\n", __func__);
	mutex_lock(&load_mutex);
	vbc_eq_setting.is_loading = 1;

	/* request firmware for VBC EQ */
	ret = request_firmware(&fw, "vbc_eq", vbc_eq_setting.dev);
	if (ret != 0) {
		pr_err("Failed to load firmware: %d\n", ret);
		goto req_fw_err;
	}
	fw_data = fw->data;
	old_num_profile = vbc_eq_setting.hdr.num_profile;
	memcpy(&vbc_eq_setting.hdr, fw_data, sizeof(vbc_eq_setting.hdr));

	if (strncmp(vbc_eq_setting.hdr.magic, VBC_EQ_FIRMWARE_MAGIC_ID,
		    VBC_EQ_FIRMWARE_MAGIC_LEN)) {
		pr_err("Firmware magic error!\n");
		ret = -EINVAL;
		goto load_err;
	}

	if (vbc_eq_setting.hdr.profile_version != VBC_EQ_PROFILE_VERSION) {
		pr_err("Firmware support version is 0x%x!\n",
		       VBC_EQ_PROFILE_VERSION);
		ret = -EINVAL;
		goto load_err;
	}

	if (vbc_eq_setting.hdr.num_profile > VBC_EQ_PROFILE_CNT_MAX) {
		pr_err("Firmware profile to large at %d, max count is %d!\n",
		       vbc_eq_setting.hdr.num_profile, VBC_EQ_PROFILE_CNT_MAX);
		ret = -EINVAL;
		goto load_err;
	}

	len = vbc_eq_setting.hdr.num_profile * sizeof(struct vbc_eq_profile);

	if (old_num_profile != vbc_eq_setting.hdr.num_profile) {
		vbc_safe_kfree(&vbc_eq_setting.data);
	}
	if (vbc_eq_setting.data == NULL) {
		vbc_eq_setting.data = kzalloc(len, GFP_KERNEL);
		if (vbc_eq_setting.data == NULL) {
			ret = -ENOMEM;
			goto load_err;
		}
	}
	offset = sizeof(struct vbc_fw_header);
	memcpy(vbc_eq_setting.data, fw_data + offset, len);

	for (i = 0; i < vbc_eq_setting.hdr.num_profile; i++) {
		if (strncmp(vbc_eq_setting.data[i].magic,
			    VBC_EQ_FIRMWARE_MAGIC_ID,
			    VBC_EQ_FIRMWARE_MAGIC_LEN)) {
			pr_err("Firmware profile[%d] magic error!\n", i);
			ret = -EINVAL;
			goto eq_err;
		}
	}
#ifndef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
	ret = vbc_eq_profile_add_widgets(codec);
#else
	ret = 0;
#endif
	goto eq_out;

eq_err:
	vbc_safe_kfree(&vbc_eq_setting.data);
eq_out:
load_err:
	release_firmware(fw);
req_fw_err:
	vbc_eq_setting.is_loading = 0;
	mutex_unlock(&load_mutex);
#ifdef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
	if (ret >= 0) {
		vbc_eq_setting.is_loaded = 1;
		vbc_eq_try_apply(vbc_eq_setting.codec_dai);
	}
#endif
	vbc_dbg("return %i\n", ret);
	vbc_dbg("Leaving %s\n", __func__);
	return ret;
}

static int vbc_switch_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	ret = arch_audio_vbc_switch(AUDIO_NO_CHANGE);
	if (ret >= 0)
		ucontrol->value.integer.value[0] =
		    (ret == AUDIO_TO_DSP_CTRL ? 0 : 1);
	return ret;
}

static int vbc_switch_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	pr_info("VBC switch to %s\n",
		texts->texts[ucontrol->value.integer.value[0]]);

	ret = ucontrol->value.integer.value[0];
	ret = arch_audio_vbc_switch(ret == 0 ?
				    AUDIO_TO_DSP_CTRL : AUDIO_TO_ARM_CTRL);

	vbc_dbg("Leaving %s\n", __func__);
	return ret;
}

static int vbc_eq_switch_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = vbc_eq_setting.is_active;
	return 0;
}

static int vbc_eq_switch_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	pr_info("VBC eq switch %s\n",
		ucontrol->value.integer.value[0] ? "ON" : "OFF");

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc_eq_setting.is_active) {
		return ret;
	}
	if ((ret == 0) || (ret == 1)) {
		vbc_eq_setting.is_active = ret;
		if (vbc_eq_setting.is_active) {
			vbc_eq_setting.vbc_eq_apply = vbc_eq_profile_apply;
			vbc_eq_try_apply(vbc_eq_setting.codec_dai);
		} else {
			vbc_eq_setting.vbc_eq_apply = 0;
			vbc_eq_profile_close();
		}
	}

	vbc_dbg("Leaving %s\n", __func__);
	return ret;
}

static int vbc_eq_load_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = vbc_eq_setting.is_loading;
	return 0;
}

static int vbc_eq_load_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int ret;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	pr_info("VBC eq %s\n", texts->texts[ucontrol->value.integer.value[0]]);

	ret = ucontrol->value.integer.value[0];
	if (ret == 1) {
		ret = vbc_eq_loading(codec);
	}

	vbc_dbg("Leaving %s\n", __func__);
	return ret;
}

static int vbc_dg_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int vbc_idx = vbc_str_2_index(mc->shift);
	ucontrol->value.integer.value[0] = vbc[vbc_idx].dg_val[id];
	return 0;
}

static int vbc_dg_put(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int vbc_idx = vbc_str_2_index(mc->shift);

	pr_info("VBC %s%s DG set 0x%02x\n", vbc_idx == 1 ? "ADC" : "DAC",
		id == VBC_LEFT ? "L" : "R", (int)ucontrol->value.integer.value[0]);

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc[vbc_idx].dg_val[id]) {
		return ret;
	}
	if (ret <= VBC_DG_VAL_MAX) {
		vbc[vbc_idx].dg_val[id] = ret;
	}

	vbc_try_dg_set(vbc_idx, id);

	vbc_dbg("Leaving %s\n", __func__);
	return ret;
}

static int vbc_dg_switch_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int vbc_idx = vbc_str_2_index(mc->shift);
	ucontrol->value.integer.value[0] = vbc[vbc_idx].dg_switch[id];
	return 0;
}

static int vbc_dg_switch_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int vbc_idx = vbc_str_2_index(mc->shift);

	pr_info("VBC %s%s DG switch %s\n", vbc_idx == 1 ? "ADC" : "DAC",
		id == VBC_LEFT ? "L" : "R",
		ucontrol->value.integer.value[0] ? "ON" : "OFF");

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc[vbc_idx].dg_switch[id]) {
		return ret;
	}

	vbc[vbc_idx].dg_switch[id] = ret;

	vbc_try_dg_set(vbc_idx, id);

	vbc_dbg("Leaving %s\n", __func__);
	return ret;
}

static const char *switch_function[] = { "dsp", "arm" };
static const char *eq_load_function[] = { "idle", "loading" };

static const struct soc_enum vbc_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, switch_function),
	SOC_ENUM_SINGLE_EXT(2, eq_load_function),
};

static const struct snd_kcontrol_new vbc_controls[] = {
	SOC_ENUM_EXT("VBC Switch", vbc_enum[0], vbc_switch_get,
		     vbc_switch_put),
	SOC_SINGLE_EXT("VBC EQ Switch", SND_SOC_NOPM, 0, 1, 0,
		       vbc_eq_switch_get,
		       vbc_eq_switch_put),
	SOC_ENUM_EXT("VBC EQ Update", vbc_enum[1], vbc_eq_load_get,
		     vbc_eq_load_put),

	SOC_SINGLE_EXT("VBC DACL DG Set", FUN_REG(VBC_LEFT),
		       SNDRV_PCM_STREAM_PLAYBACK, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC DACR DG Set", FUN_REG(VBC_RIGHT),
		       SNDRV_PCM_STREAM_PLAYBACK, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC ADCL DG Set", FUN_REG(VBC_LEFT),
		       SNDRV_PCM_STREAM_CAPTURE, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC ADCR DG Set", FUN_REG(VBC_RIGHT),
		       SNDRV_PCM_STREAM_CAPTURE, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),

	SOC_SINGLE_EXT("VBC DACL DG Switch", FUN_REG(VBC_LEFT),
		       SNDRV_PCM_STREAM_PLAYBACK, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC DACR DG Switch", FUN_REG(VBC_RIGHT),
		       SNDRV_PCM_STREAM_PLAYBACK, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC ADCL DG Switch", FUN_REG(VBC_LEFT),
		       SNDRV_PCM_STREAM_CAPTURE, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC ADCR DG Switch", FUN_REG(VBC_RIGHT),
		       SNDRV_PCM_STREAM_CAPTURE, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),

#ifdef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
	SOC_SINGLE_EXT("VBC EQ Profile Select", 0, 0, VBC_EQ_PROFILE_CNT_MAX, 0,
		       vbc_eq_profile_get, vbc_eq_profile_put),
#endif
};

int vbc_add_controls(struct snd_soc_codec *codec)
{
	int ret;
#ifndef CONFIG_SPRD_VBC_EQ_PROFILE_ASSUME
	ret = vbc_eq_profile_add_widgets(codec);
	if (ret < 0) {
		pr_err("Failed to VBC add default profile\n");
	}
#endif
	ret = snd_soc_add_controls(codec, vbc_controls,
				   ARRAY_SIZE(vbc_controls));
	if (ret < 0) {
		pr_err("Failed to VBC add controls\n");
	}

	vbc_proc_init(codec);

	return ret;
}

EXPORT_SYMBOL_GPL(vbc_add_controls);

static int __init vbc_init(void)
{
	arch_audio_vbc_switch(AUDIO_TO_ARM_CTRL);
	return platform_driver_register(&vbc_driver);
}

static void __exit vbc_exit(void)
{
	platform_driver_unregister(&vbc_driver);
}

module_init(vbc_init);
module_exit(vbc_exit);

MODULE_DESCRIPTION("SPRD ASoC VBC CUP-DAI driver");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cpu-dai:vbc");

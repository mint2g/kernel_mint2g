/*
 * sound/soc/sprd/dai/vbc/sprd-vbc-pcm.c
 *
 * SpreadTrum VBC for the pcm stream.
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
#define pr_fmt(fmt) "[audio: pcm ] " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include <mach/dma.h>
#include <mach/sprd-audio.h>

#include "sprd-vbc-pcm.h"
#include "../vaudio/vaudio.h"

#ifdef CONFIG_SPRD_AUDIO_DEBUG
#define sprd_pcm_dbg pr_debug
#else
#define sprd_pcm_dbg(...)
#endif

typedef struct sprd_dma_desc {
	volatile u32 cfg;
	volatile u32 tlen;	/* Total transfer length, in bytes */
	volatile u32 dsrc;	/* Source address. This address value should align to the SRC_DATA_WIDTH */
	volatile u32 ddst;	/* Destination address. This address value should align to the DES_DATA_WIDTH. */
	volatile u32 llptr;	/* Linked list pointer to the next node address */
	volatile u32 pmod;	/* POST MODE */
	volatile u32 sbm;	/* src burst mode */
	volatile u32 dbm;	/* dst burst mode */
} sprd_dma_desc;

struct sprd_runtime_data {
	int dma_addr_offset;
	struct sprd_pcm_dma_params *params;
	int uid_cid_map[2];
	int int_pos_update[2];
	sprd_dma_desc *dma_desc_array;
	dma_addr_t dma_desc_array_phys;
#ifdef CONFIG_SPRD_VBC_INTERLEAVED
	int interleaved;
#endif
#ifdef CONFIG_SPRD_AUDIO_BUFFER_USE_IRAM
	int buffer_in_iram;
#endif
};

#ifdef CONFIG_SPRD_AUDIO_BUFFER_USE_IRAM
#define SPRD_VBC_DMA_NODE_SIZE (1024)
#endif

static const struct snd_pcm_hardware sprd_pcm_hardware = {
	.info = SNDRV_PCM_INFO_MMAP |
	    SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_NONINTERLEAVED |
#ifdef CONFIG_SPRD_VBC_INTERLEAVED
	    SNDRV_PCM_INFO_INTERLEAVED |
#endif
	    SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.formats = VBC_PCM_FORMATS,
	/* 16bits, stereo-2-channels */
	.period_bytes_min = VBC_FIFO_FRAME_NUM * 4,
	/* non limit */
	.period_bytes_max = VBC_FIFO_FRAME_NUM * 4 * 100,
	.periods_min = 1,
	/* non limit */
	.periods_max = PAGE_SIZE / sizeof(sprd_dma_desc),
	.buffer_bytes_max = 128 * 1024,
};

#ifdef CONFIG_SPRD_AUDIO_BUFFER_USE_IRAM
static char *s_mem_for_iram = 0;
static char *s_iram_remap_base = 0;

static int sprd_buffer_iram_backup(void)
{
	void __iomem *iram_start;
	sprd_pcm_dbg("Entering %s 0x%x\n", __func__, (int)s_mem_for_iram);
	if (!s_iram_remap_base) {
		s_iram_remap_base =
		    ioremap_nocache(SPRD_IRAM_ALL_PHYS, SPRD_IRAM_ALL_SIZE);
	}
	if (!s_mem_for_iram) {
		s_mem_for_iram = kzalloc(SPRD_IRAM_ALL_SIZE, GFP_KERNEL);
	} else {
		sprd_pcm_dbg("iram is backup, be careful use iram!\n");
		return 0;
	}
	if (!s_mem_for_iram) {
		pr_err("iram backup error\n");
		return -ENOMEM;
	}
	iram_start = (void __iomem *)(s_iram_remap_base);
	memcpy_fromio(s_mem_for_iram, iram_start, SPRD_IRAM_ALL_SIZE);
	sprd_pcm_dbg("Leaving %s\n", __func__);
	return 0;
}

static int sprd_buffer_iram_restore(void)
{
	void __iomem *iram_start;
	sprd_pcm_dbg("Entering %s 0x%x\n", __func__, (int)s_mem_for_iram);
	if (!s_mem_for_iram) {
		pr_err("iram not backup\n");
		return 0;
	}
	iram_start = (void __iomem *)(s_iram_remap_base);
	memcpy_toio(iram_start, s_mem_for_iram, SPRD_IRAM_ALL_SIZE);
	kfree(s_mem_for_iram);
	s_mem_for_iram = 0;
	sprd_pcm_dbg("Leaving %s\n", __func__);
	return 0;
}
#endif

#ifdef CONFIG_SPRD_VBC_INTERLEAVED
static inline int sprd_pcm_is_interleaved(struct snd_pcm_runtime *runtime)
{
	return (runtime->access == SNDRV_PCM_ACCESS_RW_INTERLEAVED ||
		runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED);
}
#endif

#define PCM_DIR_NAME(stream) (stream == SNDRV_PCM_STREAM_PLAYBACK ? "Playback" : "Captrue")

static int sprd_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd;
	int ret;

	pr_info("open %s\n", PCM_DIR_NAME(substream->stream));

	snd_soc_set_runtime_hwparams(substream, &sprd_pcm_hardware);

	/*
	 * For mysterious reasons (and despite what the manual says)
	 * playback samples are lost if the DMA count is not a multiple
	 * of the DMA burst size.  Let's add a rule to enforce that.
	 */
	ret = snd_pcm_hw_constraint_step(runtime, 0,
					 SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
					 (VBC_FIFO_FRAME_NUM * 4));
	if (ret)
		goto out;

	ret = snd_pcm_hw_constraint_step(runtime, 0,
					 SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
					 (VBC_FIFO_FRAME_NUM * 4));
	if (ret)
		goto out;

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto out;

	ret = -ENOMEM;
	rtd = kzalloc(sizeof(*rtd), GFP_KERNEL);
	if (!rtd)
		goto out;
#ifdef CONFIG_SPRD_AUDIO_BUFFER_USE_IRAM
	if (!((substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	      && 0 == sprd_buffer_iram_backup())) {
#endif
		rtd->dma_desc_array =
		    dma_alloc_writecombine(substream->pcm->card->dev,
					   2 * PAGE_SIZE,
					   &rtd->dma_desc_array_phys,
					   GFP_KERNEL);
#ifdef CONFIG_SPRD_AUDIO_BUFFER_USE_IRAM
	} else {
		runtime->hw.periods_max =
		    SPRD_VBC_DMA_NODE_SIZE / sizeof(sprd_dma_desc),
		    runtime->hw.buffer_bytes_max =
		    SPRD_IRAM_ALL_SIZE - (2 * SPRD_VBC_DMA_NODE_SIZE),
		    rtd->dma_desc_array =
		    (void *)(s_iram_remap_base + runtime->hw.buffer_bytes_max);
		rtd->dma_desc_array_phys =
		    SPRD_IRAM_ALL_PHYS + runtime->hw.buffer_bytes_max;
		rtd->buffer_in_iram = 1;
	}
#endif
	if (!rtd->dma_desc_array)
		goto err1;
	rtd->uid_cid_map[0] = rtd->uid_cid_map[1] = -1;
	runtime->private_data = rtd;
	ret = 0;
	goto out;

err1:
	kfree(rtd);
out:
	sprd_pcm_dbg("return %i\n", ret);
	sprd_pcm_dbg("Leaving %s\n", __func__);
	return ret;
}

static int sprd_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;

	pr_info("close %s\n", PCM_DIR_NAME(substream->stream));

#ifdef CONFIG_SPRD_AUDIO_BUFFER_USE_IRAM
	if (rtd->buffer_in_iram)
		sprd_buffer_iram_restore();
	else
#endif
		dma_free_writecombine(substream->pcm->card->dev, 2 * PAGE_SIZE,
				      rtd->dma_desc_array,
				      rtd->dma_desc_array_phys);
	kfree(rtd);

	sprd_pcm_dbg("Leaving %s\n", __func__);

	return 0;
}

static irqreturn_t sprd_pcm_dma_irq_ch(int dma_ch, void *dev_id)
{
	struct snd_pcm_substream *substream = dev_id;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	int i = 0;

	for (i = 0; i < 2; i++) {
		if (dma_ch == rtd->uid_cid_map[i]) {
			rtd->int_pos_update[i] = 1;

			if (rtd->uid_cid_map[1 - i] >= 0) {
				if (rtd->int_pos_update[1 - i])
					goto irq_ready;
			} else {
				goto irq_ready;
			}
		}
	}
	goto irq_ret;
irq_ready:
	rtd->int_pos_update[0] = 0;
	rtd->int_pos_update[1] = 0;
	snd_pcm_period_elapsed(dev_id);
irq_ret:
	return IRQ_HANDLED;
}

static int sprd_pcm_dma_config(struct snd_pcm_substream *substream)
{
	struct sprd_runtime_data *rtd = substream->runtime->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_pcm_dma_params *dma;
	struct sprd_dma_channel_desc dma_cfg = { 0 };
	dma_addr_t next_desc_phys[2];
	int i;

	sprd_pcm_dbg("Entering %s\n", __func__);

	if (!rtd || !rtd->params)
		return 0;

	dma = rtd->params;
	dma_cfg = dma->desc;

	next_desc_phys[0] = rtd->dma_desc_array_phys;
	next_desc_phys[1] = rtd->dma_desc_array_phys +
	    runtime->hw.periods_max * sizeof(sprd_dma_desc);
	for (i = 0; i < 2; i++) {
		if (rtd->uid_cid_map[i] >= 0) {
			dma_cfg.llist_ptr = next_desc_phys[i];
			sprd_dma_channel_config(rtd->uid_cid_map[i],
						dma->workmode, &dma_cfg);
		}
	}

	sprd_pcm_dbg("Leaving %s\n", __func__);

	return 0;
}

static int sprd_pcm_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_pcm_dma_params *dma;
	size_t totsize = params_buffer_bytes(params);
	size_t period = params_period_bytes(params);
	sprd_dma_desc *dma_desc[2];
	dma_addr_t dma_buff_phys[2], next_desc_phys[2];
	int ret = 0;
	int i;
	int used_chan_count;

	sprd_pcm_dbg("Entering %s\n", __func__);

	dma = snd_soc_dai_get_dma_data(srtd->cpu_dai, substream);

	if (!dma)
		goto no_dma;

	used_chan_count = params_channels(params);
	pr_info("chan=%d totsize=%d period=%d\n", used_chan_count, totsize,
		period);

#ifdef CONFIG_SPRD_VBC_INTERLEAVED
	rtd->interleaved = (used_chan_count == 2)
	    && sprd_pcm_is_interleaved(runtime);
	if (rtd->interleaved) {
		sprd_pcm_dbg("interleaved access\n");
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dma->desc.src_burst_mode = SRC_BURST_MODE_SINGLE;
		} else {
			dma->desc.dst_burst_mode = SRC_BURST_MODE_SINGLE;
		}
	}
#endif

	/* this may get called several times by oss emulation
	 * with different params */
	if (rtd->params == NULL) {
		rtd->params = dma;
		for (i = 0; i < used_chan_count; i++) {
			ret = sprd_dma_request(dma->channels[i],
					       sprd_pcm_dma_irq_ch, substream);
			if (ret < 0) {
				pr_err("sprd-vbc-pcm request dma error %d\n",
				       dma->channels[i]);
				for (i--; i >= 0; i--) {
					sprd_dma_free(dma->channels[i]);
					rtd->uid_cid_map[i] = -1;
					rtd->params = NULL;
				}
				goto hw_param_err;
			}
			sprd_pcm_dbg("chan%d dma id=%d\n", i, ret);
			rtd->uid_cid_map[i] = ret;
			sprd_dma_set_irq_type(rtd->uid_cid_map[i],
					      rtd->params->irq_type, ON);
		}
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	runtime->dma_bytes = totsize;

	dma_desc[0] = rtd->dma_desc_array;
	dma_desc[1] = rtd->dma_desc_array + runtime->hw.periods_max;
	next_desc_phys[0] = rtd->dma_desc_array_phys;
	next_desc_phys[1] = rtd->dma_desc_array_phys +
	    runtime->hw.periods_max * sizeof(sprd_dma_desc);
	dma_buff_phys[0] = runtime->dma_addr;
	rtd->dma_addr_offset = (totsize / used_chan_count);
#ifdef CONFIG_SPRD_VBC_INTERLEAVED
	if (sprd_pcm_is_interleaved(runtime))
		rtd->dma_addr_offset = 2;
#endif
	dma_buff_phys[1] = runtime->dma_addr + rtd->dma_addr_offset;

	do {
		for (i = 0; i < used_chan_count; i++) {
			next_desc_phys[i] += sizeof(sprd_dma_desc);
			if (rtd->params->workmode == DMA_LINKLIST) {
				dma_desc[i]->llptr = next_desc_phys[i];
			}

			dma_desc[i]->cfg = dma->desc.cfg_req_mode_sel |
			    dma->desc.cfg_src_data_width |
			    dma->desc.cfg_dst_data_width |
			    (VBC_FIFO_FRAME_NUM * 2);
			dma_desc[i]->sbm = dma->desc.src_burst_mode |
			    dma->desc.src_blk_postm;
			dma_desc[i]->dbm = dma->desc.dst_burst_mode |
			    dma->desc.dst_blk_postm;
			dma_desc[i]->tlen = period / used_chan_count;

			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				dma_desc[i]->dsrc = dma_buff_phys[i];
				dma_desc[i]->ddst = dma->dev_paddr[i];
#ifdef CONFIG_SPRD_VBC_INTERLEAVED
				if (rtd->interleaved)
					dma_desc[i]->pmod =
					    (4 << SRC_ELEM_POSTM_SHIFT);
#endif
			} else {
				dma_desc[i]->dsrc = dma->dev_paddr[i];
				dma_desc[i]->ddst = dma_buff_phys[i];
#ifdef CONFIG_SPRD_VBC_INTERLEAVED
				if (rtd->interleaved)
					dma_desc[i]->pmod = 4;
#endif
			}
			dma_buff_phys[i] += dma_desc[i]->tlen;
#ifdef CONFIG_SPRD_VBC_INTERLEAVED
			if (rtd->interleaved)
				dma_buff_phys[i] += dma_desc[i]->tlen;
#endif
			dma_desc[i]++;
		}

		if (period > totsize)
			period = totsize;

	} while (totsize -= period);

	if (rtd->params->workmode == DMA_LINKLIST) {
		dma_desc[0][-1].llptr = rtd->dma_desc_array_phys;
		if (used_chan_count > 1) {
			dma_desc[1][-1].llptr = rtd->dma_desc_array_phys
			    + runtime->hw.periods_max * sizeof(sprd_dma_desc);
		}
	}

	sprd_pcm_dma_config(substream);

	goto ok_go_out;

no_dma:
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totsize;
hw_param_err:
ok_go_out:
	sprd_pcm_dbg("return %i\n", ret);
	sprd_pcm_dbg("Leaving %s\n", __func__);
	return ret;
}

static int sprd_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct sprd_runtime_data *rtd = substream->runtime->private_data;
	struct sprd_pcm_dma_params *dma = rtd->params;

	sprd_pcm_dbg("Entering %s\n", __func__);
        if(substream->dma_buffer.area)
              memset(substream->dma_buffer.area,0,substream->dma_buffer.bytes);
	snd_pcm_set_runtime_buffer(substream, NULL);

	if (dma) {
                int i;
		for (i = 0; i < 2; i++) {
			if (rtd->uid_cid_map[i] >= 0) {
				sprd_dma_free(rtd->uid_cid_map[i]);
				rtd->uid_cid_map[i] = -1;
			}
		}
		rtd->params = NULL;
	}

	sprd_pcm_dbg("Leaving %s\n", __func__);

	return 0;
}

static int sprd_pcm_prepare(struct snd_pcm_substream *substream)
{
	sprd_pcm_dbg("Entering %s\n", __func__);

	sprd_pcm_dbg("Leaving %s\n", __func__);

	return 0;
}

static int sprd_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct sprd_runtime_data *rtd = substream->runtime->private_data;
	int ret = 0;
	int i;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		for (i = 0; i < 2; i++) {
			if (rtd->uid_cid_map[i] >= 0) {
				sprd_dma_start(rtd->uid_cid_map[i]);
			}
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		for (i = 0; i < 2; i++) {
			if (rtd->uid_cid_map[i] >= 0) {
				sprd_dma_stop(rtd->uid_cid_map[i]);
			}
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static inline int sprd_pcm_dma_get_addr(int dma_ch,
					struct snd_pcm_substream *substream)
{
	int offset;
	offset = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
	    DMA_CH_SRC_ADDR : DMA_CH_DEST_ADDR;
	return dma_get_reg(DMA_CHx_BASE(dma_ch) + offset);
}

static snd_pcm_uframes_t sprd_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	snd_pcm_uframes_t x;
	int now_pointer;
	int bytes_of_pointer = 0;
	int shift = 1;
#ifdef CONFIG_SPRD_VBC_INTERLEAVED
	if (rtd->interleaved)
		shift = 0;
#endif

	if (rtd->uid_cid_map[0] >= 0) {
		now_pointer = sprd_pcm_dma_get_addr(rtd->uid_cid_map[0],
						    substream) -
		    runtime->dma_addr;
		bytes_of_pointer = now_pointer;
	}
	if (rtd->uid_cid_map[1] >= 0) {
		now_pointer = sprd_pcm_dma_get_addr(rtd->uid_cid_map[1],
						    substream) -
		    runtime->dma_addr - rtd->dma_addr_offset;
		if (!bytes_of_pointer)
			bytes_of_pointer = now_pointer;
		else
			bytes_of_pointer =
			    min(bytes_of_pointer, now_pointer) << shift;
	}

	x = bytes_to_frames(runtime, bytes_of_pointer);

	if (x == runtime->buffer_size)
		x = 0;

#if 0
	sprd_pcm_dbg("p=%d f=%d\n", bytes_of_pointer, x);
#endif

	return x;
}

static int sprd_pcm_mmap(struct snd_pcm_substream *substream,
			 struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

#ifndef CONFIG_SPRD_AUDIO_BUFFER_USE_IRAM
	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr, runtime->dma_bytes);
#else
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	return remap_pfn_range(vma, vma->vm_start,
			       runtime->dma_addr,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);
#endif
}

static struct snd_pcm_ops sprd_pcm_ops = {
	.open = sprd_pcm_open,
	.close = sprd_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = sprd_pcm_hw_params,
	.hw_free = sprd_pcm_hw_free,
	.prepare = sprd_pcm_prepare,
	.trigger = sprd_pcm_trigger,
	.pointer = sprd_pcm_pointer,
	.mmap = sprd_pcm_mmap,
};

static int sprd_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = sprd_pcm_hardware.buffer_bytes_max;
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
#ifdef CONFIG_SPRD_AUDIO_BUFFER_USE_IRAM
	if (!((substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	      && 0 == sprd_buffer_iram_backup())) {
#endif
		buf->private_data = NULL;
		buf->area = dma_alloc_writecombine(pcm->card->dev, size,
						   &buf->addr, GFP_KERNEL);
#ifdef CONFIG_SPRD_AUDIO_BUFFER_USE_IRAM
	} else {
		buf->private_data = buf;
		buf->area = (void *)(s_iram_remap_base);
		buf->addr = SPRD_IRAM_ALL_PHYS;
		size = SPRD_IRAM_ALL_SIZE - (2 * SPRD_VBC_DMA_NODE_SIZE);
	}
#endif
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	return 0;
}

static u64 sprd_pcm_dmamask = DMA_BIT_MASK(32);
static struct snd_dma_buffer *save_p_buf = 0;
static struct snd_dma_buffer *save_c_buf = 0;

static int sprd_pcm_new(struct snd_card *card, struct snd_soc_dai *dai,
			struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	struct snd_pcm_substream *substream;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	sprd_pcm_dbg("Entering %s id = 0x%x\n", __func__, cpu_dai->driver->id);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &sprd_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (substream) {
		struct snd_dma_buffer *buf = &substream->dma_buffer;
		if (!save_p_buf) {
			ret = sprd_pcm_preallocate_dma_buffer(pcm,
							      SNDRV_PCM_STREAM_PLAYBACK);
			if (ret)
				goto out;
			save_p_buf = buf;
			sprd_pcm_dbg("playback alloc memery\n");
		} else {
			memcpy(buf, save_p_buf, sizeof(*buf));
			sprd_pcm_dbg("playback share memery\n");
		}
	}

	substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if (substream) {
		struct snd_dma_buffer *buf = &substream->dma_buffer;
		if (!save_c_buf) {
			ret = sprd_pcm_preallocate_dma_buffer(pcm,
							      SNDRV_PCM_STREAM_CAPTURE);
			if (ret)
				goto out;
			save_c_buf = buf;
			sprd_pcm_dbg("capture alloc memery\n");
		} else {
			memcpy(buf, save_c_buf, sizeof(*buf));
			sprd_pcm_dbg("capture share memery\n");
		}
	}
out:
	sprd_pcm_dbg("return %i\n", ret);
	sprd_pcm_dbg("Leaving %s\n", __func__);
	return ret;
}

static void sprd_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	sprd_pcm_dbg("Entering %s\n", __func__);

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;
		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;
#ifdef CONFIG_SPRD_AUDIO_BUFFER_USE_IRAM
		if (buf->private_data)
			sprd_buffer_iram_restore();
		else
#endif
			dma_free_writecombine(pcm->card->dev, buf->bytes,
					      buf->area, buf->addr);
		buf->area = NULL;
		if (buf == save_p_buf) {
			save_p_buf = 0;
		}
		if (buf == save_c_buf) {
			save_c_buf = 0;
		}
	}
	sprd_pcm_dbg("Leaving %s\n", __func__);
}

static struct snd_soc_platform_driver sprd_soc_platform = {
	.ops = &sprd_pcm_ops,
	.pcm_new = sprd_pcm_new,
	.pcm_free = sprd_pcm_free_dma_buffers,
};

static int __devinit sprd_soc_platform_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &sprd_soc_platform);
}

static int __devexit sprd_soc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver sprd_pcm_driver = {
	.driver = {
		   .name = "sprd-vbc-pcm-audio",
		   .owner = THIS_MODULE,
		   },

	.probe = sprd_soc_platform_probe,
	.remove = __devexit_p(sprd_soc_platform_remove),
};

static int __init snd_sprd_pcm_init(void)
{
	return platform_driver_register(&sprd_pcm_driver);
}

static void __exit snd_sprd_pcm_exit(void)
{
	platform_driver_unregister(&sprd_pcm_driver);
}

module_init(snd_sprd_pcm_init);
module_exit(snd_sprd_pcm_exit);

MODULE_DESCRIPTION("SPRD ASoC PCM VBC DMA");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sprd-vbc-audio");

/*
 ****************************************************************
 *
 *  Component: VLX ALSA audio frontend driver
 *
 *  Copyright (C) 2011, Red Bend Ltd.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  You should have received a copy of the GNU General Public License Version 2
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *  Contributor(s):
 *    Pascal Piovesan (pascal.piovesan@redbend.com)
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#include <nk/nkern.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION (2,6,27)
#include <sound/driver.h>
#endif
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>

#include "vaudio.h"

MODULE_DESCRIPTION("VLX ALSA audio frontend driver");
MODULE_AUTHOR("Pascal Piovesan <pascal.piovesan@redbend.com>");
MODULE_LICENSE("GPL");

#define VAUDIO_CONFIG_NK_PMEM

#if 0
#define VAUDIO_DEBUG
#endif

#define TRACE(format, args...)	printk(KERN_INFO "VAUDIO-FE: " format, ## args)
#define WTRACE(format, args...)	\
	printk(KERN_WARNING "VAUDIO-FE: Warning: " format, ## args)
#define ETRACE(format, args...)	\
	printk(KERN_ERR "VAUDIO-FE: Error: " format, ## args)

#ifdef VAUDIO_DEBUG
#define DTRACE(format, args...)	TRACE("%s: " format, __func__, ## args)
#define ADEBUG()		DTRACE("line %d\n", __LINE__)
#else
#define DTRACE(format, args...)
#define ADEBUG()		/* nop */
#endif

    /*
     * Buffer management for ALSA and DMA.
     */
struct vaudio_stream {
    int              stream_id;	  /* numeric identification */
    struct snd_pcm_substream *stream; /* the PCM stream */
    int              active;	  /* we are using this stream for transfer */
    int              use_dma;	  /* we are using dma for this transfer */
    nku32_f          hwptr_done;
    int              period;
    int              periods_avail;
    int              periods_tosend;

    const NkVaudioHw*      hw_conf;
    NkDevRing*       ring;
    NkVaudioCtrl*    ctrl;
    NkXIrqId         ring_xid;
    NkXIrqId         ctrl_xid;
    NkRingDesc*      rbase;	 /* ring descriptor base */
    nku32_f          resp;	 /* consumer response index */
    struct semaphore ctrl_sem;

#ifdef VAUDIO_CONFIG_NK_PMEM
    NkPhAddr		ring_buf_p;
    unsigned char*	ring_buf_v;
    int			ring_size;
#endif

    NkVaudio  vaudio;	/* pointer to struct _NkVaudio */
};

    /*
     * ALSA card structure for virtual audio.
     */

struct _NkVaudio {
    struct snd_card*   card;
    NkDevVlink*        vlink;
    NkXIrqId           sysconf_xid;

    NkVaudioMixer*     mixer;
    NkXIrqId           mixer_xid;
    struct semaphore   mixer_sem;

    struct snd_pcm*      pcm [NK_VAUDIO_DEV_MAX];
    struct vaudio_stream s [NK_VAUDIO_DEV_MAX] [NK_VAUDIO_STREAM_MAX];
};

static DECLARE_COMPLETION(vaudio_thread_completion);
static pid_t              vaudio_thread_id;
static struct semaphore   vaudio_thread_sem;
static bool		  vaudio_thread_aborted;
static unsigned char	  vaudio_thread_init_now;
#define VAUDIO_PROC_SYNC    1
#define VAUDIO_VTIMER_ROUND_JIFFIES (msecs_to_jiffies(800))

#if VAUDIO_PROC_SYNC
static int vaudio_snd_card_close(struct snd_pcm_substream* substream);
static int vaudio_snd_trigger(struct snd_pcm_substream* substream, int cmd);
static DEFINE_MUTEX(vaudio_proc_sync_lock);
static struct vaudio_stream *vrs; // vaudio_record_stream;
static volatile int vaudio_sync_force_close;

#include <linux/timer.h>
static struct timer_list lutimer;
#if 1
static void lutimer_handler(unsigned long data)
{
    if (vaudio_sync_force_close && vrs && vrs->stream) {
        struct vaudio_stream *s = vrs;
        struct snd_pcm_runtime *runtime = s->stream->runtime;
        int periods_avail = runtime->periods - snd_pcm_capture_avail(runtime) / runtime->period_size;
        printk("vaudio dummy capture data flushing\n");
        memset(s->stream->dma_buffer.area, 0, NK_VAUDIO_MAX_RING_SIZE);
        while (periods_avail-- > 2) {
            s->hwptr_done++;
            s->hwptr_done %= runtime->periods;
            snd_pcm_period_elapsed(s->stream);
            s->periods_avail = 2;
        }
        mod_timer(&lutimer, jiffies + VAUDIO_VTIMER_ROUND_JIFFIES);
    } else {
        printk("vaudio dummy capture exit automatically[%d]...\n", vaudio_sync_force_close);
    }
}

static unsigned long round_jiffies_common(unsigned long j, int cpu,
		bool force_up)
{
	int rem;
	unsigned long original = j;

	/*
	 * We don't want all cpus firing their timers at once hitting the
	 * same lock or cachelines, so we skew each extra cpu with an extra
	 * 3 jiffies. This 3 jiffies came originally from the mm/ code which
	 * already did this.
	 * The skew is done by adding 3*cpunr, then round, then subtract this
	 * extra offset again.
	 */
	j += cpu * 3;

	rem = j % HZ;

	/*
	 * If the target jiffie is just after a whole second (which can happen
	 * due to delays of the timer irq, long irq off times etc etc) then
	 * we should round down to the whole second, not up. Use 1/4th second
	 * as cutoff for this rounding as an extreme upper bound for this.
	 * But never round down if @force_up is set.
	 */
	if (rem < HZ/4 && !force_up) /* round down */
		j = j - rem;
	else /* round up */
		j = j - rem + HZ;

	/* now that we have rounded, subtract the extra skew again */
	j -= cpu * 3;

	if (j <= jiffies) /* rounding ate our timeout entirely; */
		return original;
	return j;
}

static unsigned long round_jiffies2(unsigned long j)
{
	return round_jiffies_common(j, raw_smp_processor_id(), false);
}

static int lutimer_init(void)
{
    init_timer(&lutimer);
    lutimer.expires = round_jiffies2(jiffies + 1);
    lutimer.data = 1;
    lutimer.function = &lutimer_handler;
    add_timer(&lutimer);
    return 0;
}
#endif

static void lutimer_exit(void)
{
    if (lutimer.data) {
        printk("vaudio dummy capture exit forced...\n");
        del_timer_sync(&lutimer);
        lutimer.data = 0;
    }
}
#include <linux/proc_fs.h>
static int
vaudio_proc_read(char *page, char **start, off_t offset,
               int count, int *eof, void *data)
{
    char *p = page;
    char *endp = page + count;

    mutex_lock(&vaudio_proc_sync_lock);
    p += snprintf(p, endp - p, "%d", vrs ? 1 : 0);
    mutex_unlock(&vaudio_proc_sync_lock);

    return (p - page);
}

static int
vaudio_proc_write(struct file *f, const char *buf, unsigned long len, void *data)
{
    mutex_lock(&vaudio_proc_sync_lock);
    if (vrs && vrs->stream) {
        if (vaudio_sync_force_close == 0) {
            vaudio_sync_force_close = 1;
            lutimer_init();
            // snd_pcm_stop(vrs->stream, SNDRV_PCM_STATE_XRUN);
        }
    } else printk("vaudio capture doesn't open yet!\n");
    mutex_unlock(&vaudio_proc_sync_lock);
    return len;
}

static struct proc_dir_entry *vaudio_proc = NULL;
static void vaudio_proc_create(const char *name)
{
    vaudio_proc = proc_mkdir("vaudio", NULL);
    if (vaudio_proc) {
        struct proc_dir_entry *r;
        r = create_proc_entry(name, 0777, vaudio_proc);
        r->read_proc = vaudio_proc_read;
        r->write_proc = vaudio_proc_write;
    }
}

static void vaudio_proc_delete(const char *name)
{
    if (vaudio_proc) {
        remove_proc_entry(name, vaudio_proc);
        remove_proc_entry("vaudio", NULL);
    }
}
#endif

#ifdef VAUDIO_CONFIG_NK_PMEM
    static int
vaudio_pcm_lib_preallocate_pages_for_all (struct snd_pcm* pcm,
					  int type, void* data,
					  size_t size, size_t max)
{
    const int                 dev = pcm->device;
    struct snd_pcm_substream *substream;
    int                       stream;

    (void) size;
    for (stream = 0; stream < 2; stream++) {
	for (substream = pcm->streams[stream].substream; substream;
	     substream = substream->next) {
	    NkVaudio               chip = pcm->private_data;
	    struct vaudio_stream*  s = &chip->s[dev][stream];
	    struct snd_dma_buffer *dmab = &substream->dma_buffer;

	    substream->dma_buffer.dev.type = type;
	    substream->dma_buffer.dev.dev = data;
	    dmab->area = s->ring_buf_v;
	    dmab->addr = s->ring_buf_p;

	    if (substream->dma_buffer.bytes > 0)
		substream->buffer_bytes_max = substream->dma_buffer.bytes;
	    substream->dma_max = max;
	}
    }
    return 0;
}

    static int
vaudio_pcm_lib_malloc_pages (struct snd_pcm_substream* substream, size_t size)
{
    struct snd_pcm_runtime* runtime = substream->runtime;
    struct snd_dma_buffer* dmab = &substream->dma_buffer;

	/* Use the pre-allocated buffer */
    snd_pcm_set_runtime_buffer(substream, dmab);
    runtime->dma_bytes = size;
    return 1;
}

void vaudio_pcm_lib_preallocate_free_for_all(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	int stream;

	for (stream = 0; stream < 2; stream++)
		for (substream = pcm->streams[stream].substream; substream; substream = substream->next)
		    substream->dma_buffer.area = NULL;
}


    static int
vaudio_pcm_lib_free_pages (struct snd_pcm_substream* substream)
{
    snd_pcm_set_runtime_buffer(substream, NULL);
    return 0;
}
#endif	/* VAUDIO_CONFIG_NK_PMEM */

    static int
vaudio_send_data (struct vaudio_stream* s)
{
    NkDevRing*        ring  = s->ring;
    const nku32_f     oreq = ring->ireq;
    const nku32_f     nreq = oreq + 1;
    const nku32_f     mask = ring->imask;
    NkRingDesc*       desc = s->rbase + (oreq & mask);
    bool              xirq_trigger = 0;
    struct snd_pcm_runtime* runtime = s->stream->runtime;

    if ((ring->iresp & mask) == (ring->ireq & mask)) {
	DTRACE ("ring empty\n");
	xirq_trigger = 1;
    }
    if ((nreq & mask) == (ring->iresp & mask)) {
	DTRACE ("ring full\n");
	return -ENOMEM;
    }
    ring->ireq = nreq;
    desc->status  = NK_VAUDIO_STATUS_OK;
    desc->bufsize = frames_to_bytes(runtime, runtime->period_size);
#ifdef VAUDIO_CONFIG_NK_PMEM
    desc->bufaddr = s->stream->dma_buffer.addr + desc->bufsize * s->period;
#else
    desc->bufaddr =
	nkops.nk_vtop(runtime->dma_area + desc->bufsize * s->period);
#endif
    DTRACE ("%x %x\n", desc->bufaddr, desc->bufsize);
    s->period++;
    s->period %= runtime->periods;

    return xirq_trigger;
}

    static void
vaudio_intr_data (void* cookie, NkXIrq xirq)
{
    struct vaudio_stream* s = (struct vaudio_stream*) cookie;
    NkDevRing*   ring   = s->ring;
    nku32_f	 oresp  = s->resp;
    const nku32_f mask   = ring->imask;
    const nku32_f nresp  = ring->iresp;
    bool	 trigger = 0;
#if VAUDIO_PROC_SYNC
    if (vaudio_sync_force_close &&
        s->stream->pcm->device == 0 &&
        s->stream->pstr->stream == SNDRV_PCM_STREAM_CAPTURE) {
        while (vaudio_send_data(s) >= 0);
        // memset(s->stream->dma_buffer.area, 0, NK_VAUDIO_MAX_RING_SIZE);
        return;
    }
#endif
    (void) xirq;
    if (s->active) {
	struct snd_pcm_runtime* runtime = s->stream->runtime;
	int periods_avail;
	int periods_tosend;

	if (s->stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
	    periods_avail = snd_pcm_playback_avail(runtime) /
				    runtime->period_size;
	} else {
	    periods_avail = snd_pcm_capture_avail(runtime) /
				    runtime->period_size;
	}
	DTRACE ("%d %d %d\n",
		s->periods_avail, periods_avail, s->periods_tosend);
	periods_tosend = s->periods_avail - periods_avail;
	if (periods_tosend > 0) {
	    s->periods_tosend += periods_tosend;
	}
	s->periods_avail = periods_avail;
	while (s->periods_tosend) {
	    const int res = vaudio_send_data(s);
	    if (res < 0) break;
	    if (res == 1) trigger = 1;
	    s->periods_tosend--;
	}
    }
    while (oresp != nresp) {
	NkRingDesc*  desc = s->rbase + (oresp & mask);

	if (desc->status == (nku32_f) NK_VAUDIO_STATUS_ERROR) {
	    snd_pcm_stop(s->stream, SNDRV_PCM_STATE_XRUN);
	} else {
	    if (s->active) {
		struct snd_pcm_runtime* runtime = s->stream->runtime;

		s->hwptr_done++;
		s->hwptr_done %= runtime->periods;
		snd_pcm_period_elapsed(s->stream);
		s->periods_avail++;
	    }
	}
	oresp++;
    }
    s->resp = oresp;
    if (trigger) {
	nkops.nk_xirq_trigger(ring->cxirq, s->vaudio->vlink->s_id);
    }
}

    static void
vaudio_intr_ctrl (void* cookie, NkXIrq xirq)
{
    struct vaudio_stream* s = (struct vaudio_stream*) cookie;

    (void) xirq;
    up(&s->ctrl_sem);
    s->ctrl->command = NK_VAUDIO_COMMAND_NONE;
}

    static void
vaudio_intr_mixer (void* cookie, NkXIrq xirq)
{
    NkVaudio vaudio = (NkVaudio) cookie;

    (void) xirq;
    up(&vaudio->mixer_sem);
    vaudio->mixer->command = NK_VAUDIO_COMMAND_NONE;
}

    /* PCM settings */

    static int
vaudio_snd_trigger (struct snd_pcm_substream* substream, int cmd)
{
    const NkVaudio          chip = snd_pcm_substream_chip(substream);
    const int               stream_id = substream->pstr->stream;
    const int               dev = substream->pcm->device;
    struct vaudio_stream*   s = &chip->s[dev][stream_id];
    struct snd_pcm_runtime* runtime = substream->runtime;
    NkVaudioCtrl*           ctrl  = s->ctrl;
    NkDevVlink*             vlink = chip->vlink;

    ADEBUG();
	/* Local interrupts are already disabled in the midlevel code */
    switch (cmd) {
    case SNDRV_PCM_TRIGGER_START:
	    /* Requested stream startup */
	s->active = 1;
	if (stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
	    s->periods_avail = snd_pcm_playback_avail(runtime) /
				  runtime->period_size;
	} else {
	    s->periods_avail = snd_pcm_capture_avail(runtime) /
				  runtime->period_size;
	}
	s->periods_tosend = runtime->periods - s->periods_avail;
	while (s->periods_tosend) {
	    if (vaudio_send_data(s) < 0) break;
	    s->periods_tosend--;
	}
	ctrl->command = NK_VAUDIO_COMMAND_START;
	nkops.nk_xirq_trigger(ctrl->cxirq, vlink->s_id);
	    /* This command is asynchronous, no waiting on down() */
	return 0;

    case SNDRV_PCM_TRIGGER_STOP:
	    /* Requested stream shutdown */
	s->active = 0;
	ctrl->command = NK_VAUDIO_COMMAND_STOP;
	nkops.nk_xirq_trigger(ctrl->cxirq, vlink->s_id);
	    /* This command is asynchronous, no waiting on down() */
	return 0;

    default:
	break;
    }
    return -EINVAL;
}

    static int
vaudio_snd_prepare (struct snd_pcm_substream* substream)
{
    const NkVaudio          chip = snd_pcm_substream_chip(substream);
    const int               stream_id = substream->pstr->stream;
    const int               dev = substream->pcm->device;
    struct vaudio_stream*   s = &chip->s[dev][stream_id];
    struct snd_pcm_runtime* runtime = substream->runtime;
    NkVaudioCtrl*           ctrl = s->ctrl;
    NkDevVlink*             vlink = chip->vlink;

    ADEBUG();
	/* Set requested sample rate */
    ctrl->rate     = runtime->rate;
    ctrl->channels = runtime->channels;
    ctrl->format   = runtime->format;
    ctrl->period   = frames_to_bytes(runtime, runtime->period_size);
    ctrl->periods  = runtime->periods;
#ifdef VAUDIO_CONFIG_NK_PMEM
    ctrl->dma_addr = s->stream->dma_buffer.addr;
#else
    ctrl->dma_addr = nkops.nk_vtop(runtime->dma_area);
#endif
    ctrl->command  = NK_VAUDIO_COMMAND_SET_RATE;
    nkops.nk_xirq_trigger(ctrl->cxirq, vlink->s_id);
    down(&s->ctrl_sem);
    return ctrl->status;
}

    static snd_pcm_uframes_t
vaudio_snd_pointer (struct snd_pcm_substream* substream)
{
    const NkVaudio          chip = snd_pcm_substream_chip(substream);
    const int               stream_id = substream->pstr->stream;
    const int               dev = substream->pcm->device;
    struct vaudio_stream*   s = &chip->s[dev][stream_id];
    struct snd_pcm_runtime* runtime = substream->runtime;
    unsigned int            offset;

    ADEBUG();
    offset = s->hwptr_done * frames_to_bytes(runtime, runtime->period_size);
    return bytes_to_frames(runtime, offset);
}

    /* Hardware capabilities */

    static struct snd_pcm_hardware
vaudio_snd_capture [NK_VAUDIO_DEV_MAX] [NK_VAUDIO_STREAM_MAX];

    static struct snd_pcm_hardware
vaudio_snd_playback [NK_VAUDIO_DEV_MAX] [NK_VAUDIO_STREAM_MAX];

    static int
vaudio_snd_card_open (struct snd_pcm_substream* substream)
{
    const NkVaudio          chip = snd_pcm_substream_chip(substream);
    const int               dev = substream->pcm->device;
    const int               stream_id = substream->pstr->stream;
    struct vaudio_stream*   s = &chip->s[dev][stream_id];
    struct snd_pcm_runtime* runtime = substream->runtime;
    NkVaudioCtrl*           ctrl = s->ctrl;
    NkDevRing*              ring = s->ring;
    NkDevVlink*             vlink = chip->vlink;

    ADEBUG();
    s->stream     = substream;
    s->stream_id  = stream_id;
    s->hwptr_done = 0;
    s->period     = 0;
    s->resp       = 0;
    ring->ireq    = 0;
    ring->iresp   = 0;

    if (stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
	runtime->hw = vaudio_snd_playback[dev][stream_id];
	ctrl->session_type =  NK_VAUDIO_SS_TYPE_PLAYBACK;
    } else {
	runtime->hw = vaudio_snd_capture[dev][stream_id];
	ctrl->session_type = NK_VAUDIO_SS_TYPE_CAPTURE;
    }
    ctrl->stream_type = NK_VAUDIO_ST_TYPE_PCM;
    ctrl->command     = NK_VAUDIO_COMMAND_OPEN;
    nkops.nk_xirq_trigger(ctrl->cxirq, vlink->s_id);
    down(&s->ctrl_sem);
#if VAUDIO_PROC_SYNC
    if (/*dev == 0 && */stream_id == SNDRV_PCM_STREAM_CAPTURE) {
        mutex_lock(&vaudio_proc_sync_lock);
        vrs = s;
        mutex_unlock(&vaudio_proc_sync_lock);
    }
#endif
    return ctrl->status;
}

    static int
vaudio_snd_card_close (struct snd_pcm_substream* substream)
{
    const NkVaudio        chip = snd_pcm_substream_chip(substream);
    const int             dev = substream->pcm->device;
    const int             stream_id = substream->pstr->stream;
    struct vaudio_stream* s = &chip->s[dev][stream_id];
    NkVaudioCtrl*         ctrl = s->ctrl;
#if VAUDIO_PROC_SYNC
    mutex_lock(&vaudio_proc_sync_lock);
    if (/*dev == 0 && */stream_id == SNDRV_PCM_STREAM_CAPTURE) {
        vaudio_sync_force_close = 0;
        lutimer_exit();
        vrs = NULL;
    }
    mutex_unlock(&vaudio_proc_sync_lock);
#endif
    ADEBUG();
    s->stream = NULL;
    ctrl->session_type = NK_VAUDIO_SS_TYPE_INVAL;
    ctrl->stream_type  = NK_VAUDIO_ST_TYPE_INVAL;
    ctrl->command      = NK_VAUDIO_COMMAND_CLOSE;
    nkops.nk_xirq_trigger(ctrl->cxirq, chip->vlink->s_id);
    down(&s->ctrl_sem);
    return ctrl->status;
}

    static int
vaudio_snd_hw_params (struct snd_pcm_substream* substream,
		      struct snd_pcm_hw_params* hw_params)
{
    ADEBUG();
#ifdef VAUDIO_CONFIG_NK_PMEM
    return vaudio_pcm_lib_malloc_pages (substream,
					params_buffer_bytes(hw_params));
#else
    return snd_pcm_lib_malloc_pages (substream,
				     params_buffer_bytes(hw_params));
#endif
}

    static int
vaudio_snd_hw_free (struct snd_pcm_substream* substream)
{
    ADEBUG();
#ifdef VAUDIO_CONFIG_NK_PMEM
    return vaudio_pcm_lib_free_pages(substream);
#else
    return snd_pcm_lib_free_pages(substream);
#endif
}

#ifdef VAUDIO_CONFIG_NK_PMEM
    static int
vaudio_snd_mmap (struct snd_pcm_substream* substream,
		 struct vm_area_struct *vma)
{
    const NkVaudio        chip = snd_pcm_substream_chip(substream);
    const int             dev = substream->pcm->device;
    const int             stream_id = substream->pstr->stream;
    struct vaudio_stream* s = &chip->s[dev][stream_id];

    ADEBUG();

    if (s->use_dma) {
        vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
    }
    return remap_pfn_range(vma, vma->vm_start,
		   substream->dma_buffer.addr >> PAGE_SHIFT,
		   vma->vm_end - vma->vm_start, vma->vm_page_prot);
}
#endif

    /* PCM operations */

    static struct snd_pcm_ops
vaudio_snd_card_playback_ops = {
    .open      = vaudio_snd_card_open,
    .close     = vaudio_snd_card_close,
    .ioctl     = snd_pcm_lib_ioctl,
    .hw_params = vaudio_snd_hw_params,
    .hw_free   = vaudio_snd_hw_free,
    .prepare   = vaudio_snd_prepare,
    .trigger   = vaudio_snd_trigger,
    .pointer   = vaudio_snd_pointer,
#ifdef VAUDIO_CONFIG_NK_PMEM
    .mmap      = vaudio_snd_mmap,
#endif
};

    static struct snd_pcm_ops
vaudio_snd_card_capture_ops = {
    .open      = vaudio_snd_card_open,
    .close     = vaudio_snd_card_close,
    .ioctl     = snd_pcm_lib_ioctl,
    .hw_params = vaudio_snd_hw_params,
    .hw_free   = vaudio_snd_hw_free,
    .prepare   = vaudio_snd_prepare,
    .trigger   = vaudio_snd_trigger,
    .pointer   = vaudio_snd_pointer,
#ifdef VAUDIO_CONFIG_NK_PMEM
    .mmap      = vaudio_snd_mmap,
#endif
};

    /*
     *  Initialize PCM ALSA structures.
     */

    static int __init
vaudio_snd_card_pcm (NkVaudio vaudio, int device)
{
    struct snd_pcm* pcm;
    int err;

    ADEBUG();
    if ((err = snd_pcm_new(vaudio->card, "VAUDIO PCM", device, 1, 1, &pcm)) < 0)
	return err;
    pcm->private_data = vaudio;

	/* Set up initial buffer with continuous allocation */
#ifdef VAUDIO_CONFIG_NK_PMEM
    pcm->private_free = vaudio_pcm_lib_preallocate_free_for_all;
    vaudio_pcm_lib_preallocate_pages_for_all(pcm,
					  SNDRV_DMA_TYPE_CONTINUOUS,
					  snd_dma_continuous_data
					  (GFP_KERNEL),
					  NK_VAUDIO_MAX_RING_SIZE,
					  NK_VAUDIO_MAX_RING_SIZE);
#else
    snd_pcm_lib_preallocate_pages_for_all(pcm,
					  SNDRV_DMA_TYPE_CONTINUOUS,
					  snd_dma_continuous_data
					  (GFP_KERNEL),
					  NK_VAUDIO_MAX_RING_SIZE,
					  NK_VAUDIO_MAX_RING_SIZE);
#endif
    snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK,
		    &vaudio_snd_card_playback_ops);
    snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE,
		    &vaudio_snd_card_capture_ops);
    pcm->info_flags = 0;
    snprintf (pcm->name, sizeof pcm->name, "virtual audio pcm");
    vaudio->pcm[device] = pcm;
    return 0;
}

static void vaudio_sysconf_trigger(NkVaudio dev);

static struct snd_card *vaudio_card;

    static void
vaudio_snd_free (struct snd_card* card)
{
    const NkVaudio chip = card->private_data;
    int            i, j;

    if (vaudio_card == 0) {
	WTRACE("Trying to free `veaudio' twice\n");
	return;
    }
    vaudio_card = 0;

    ADEBUG();
    for (i = 0; i < NK_VAUDIO_DEV_MAX; i++) {
	for (j = 0; j < NK_VAUDIO_STREAM_MAX; j++) {
	    struct vaudio_stream* s = &chip->s[i][j];

	    if (s->ring_xid) {
		nkops.nk_xirq_detach(s->ring_xid);
	    }
	    if (s->ctrl_xid) {
		nkops.nk_xirq_detach(s->ctrl_xid);
	    }
	}
    }
    if (chip->mixer_xid) {
	nkops.nk_xirq_detach(chip->mixer_xid);
    }
    if (chip->sysconf_xid) {
	nkops.nk_xirq_detach(chip->sysconf_xid);
    }
    chip->vlink->c_state = NK_DEV_VLINK_OFF;
    vaudio_sysconf_trigger(chip);
}

    /*
     * Vaudio mixer.
     */

static struct snd_kcontrol_new
			vaudio_snd_controls     [NK_VAUDIO_MIXER_MAX];
static unsigned char	vaudio_snd_control_names[NK_VAUDIO_MIXER_MAX][64];
static nku8_f		vaudio_snd_control_res  [NK_VAUDIO_MIXER_MAX][64];

static size_t vaudio_uinfo_id_name_size;
static size_t vaudio_uinfo_reserved_size;
static size_t vaudio_uinfo_value_enum_size;

    static int
vaudio_snd_info (struct snd_kcontrol* kcontrol,
		 struct snd_ctl_elem_info* uinfo)
{
    const NkVaudio chip = snd_kcontrol_chip(kcontrol);
    NkVaudioMixer* mixer = chip->mixer;
    NkDevVlink*    vlink = chip->vlink;
    int            reserved;

    mixer->command = NK_VAUDIO_COMMAND_MIXER;
    mixer->mix_cmd = NK_VAUDIO_MIXER_INFO;
    mixer->mix_idx = kcontrol->private_value & 0xffff;
    mixer->mix_info.value.enumerated.item = uinfo->value.enumerated.item;
    nkops.nk_xirq_trigger(mixer->cxirq, vlink->s_id);
    down(&chip->mixer_sem);
    if (mixer->status != NK_VAUDIO_STATUS_OK) {
	return 1;
    }
    memcpy(uinfo->id.name, mixer->mix_info.name, vaudio_uinfo_id_name_size);
    reserved = *(int*)mixer->mix_info.reserved;
    if (reserved) {
	memcpy(uinfo->reserved, mixer->mix_info.reserved,
	       vaudio_uinfo_reserved_size);
    }
    switch (mixer->mix_info.type) {
    case NK_CTL_ELEM_TYPE_BOOLEAN:
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = mixer->mix_info.count;
	uinfo->value.integer.min = mixer->mix_info.value.integer.min;
	uinfo->value.integer.max = mixer->mix_info.value.integer.max;
	break;

    case NK_CTL_ELEM_TYPE_INTEGER:
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = mixer->mix_info.count;
	uinfo->value.integer.min = mixer->mix_info.value.integer.min;
	uinfo->value.integer.max = mixer->mix_info.value.integer.max;
	break;

    case NK_CTL_ELEM_TYPE_ENUMERATED:
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = mixer->mix_info.count;
	uinfo->value.enumerated.items = mixer->mix_info.value.enumerated.items;
	uinfo->value.enumerated.item = mixer->mix_info.value.enumerated.item;
	memcpy(uinfo->value.enumerated.name,
	       mixer->mix_info.value.enumerated.name,
	       vaudio_uinfo_value_enum_size);
	break;
    }
    return 0;
}

    static int
vaudio_snd_get (struct snd_kcontrol* kcontrol,
		struct snd_ctl_elem_value* ucontrol)
{
    const NkVaudio chip = snd_kcontrol_chip(kcontrol);
    NkVaudioMixer* mixer = chip->mixer;
    NkDevVlink*    vlink = chip->vlink;

    mixer->command  = NK_VAUDIO_COMMAND_MIXER;
    mixer->mix_cmd  = NK_VAUDIO_MIXER_GET;
    mixer->mix_idx  = kcontrol->private_value & 0xffff;
    mixer->mix_type = kcontrol->private_value >> 16;
    nkops.nk_xirq_trigger(mixer->cxirq, vlink->s_id);
    down(&chip->mixer_sem);
    if (mixer->status != NK_VAUDIO_STATUS_OK) {
	return 1;
    }
    switch (mixer->mix_type) {
    case NK_CTL_ELEM_TYPE_BOOLEAN:
    case NK_CTL_ELEM_TYPE_INTEGER:
	memcpy(&ucontrol->value.integer, &mixer->mix_val.value.integer,
	       sizeof(mixer->mix_val.value.integer));
	break;

    case NK_CTL_ELEM_TYPE_ENUMERATED:
	memcpy(&ucontrol->value.enumerated,
	       &mixer->mix_val.value.enumerated,
	       sizeof(mixer->mix_val.value.enumerated));
	break;
    }
    return 0;
}

    static int
vaudio_snd_put (struct snd_kcontrol* kcontrol,
		struct snd_ctl_elem_value* ucontrol)
{
    const NkVaudio chip = snd_kcontrol_chip(kcontrol);
    NkVaudioMixer* mixer = chip->mixer;

    mixer->command  = NK_VAUDIO_COMMAND_MIXER;
    mixer->mix_cmd  = NK_VAUDIO_MIXER_PUT;
    mixer->mix_idx  = kcontrol->private_value & 0xffff;
    mixer->mix_type = kcontrol->private_value >> 16;

    switch (mixer->mix_type) {
    case NK_CTL_ELEM_TYPE_BOOLEAN:
    case NK_CTL_ELEM_TYPE_INTEGER:
	memcpy(&mixer->mix_val.value.integer, &ucontrol->value.integer,
	       sizeof(mixer->mix_val.value.integer));
	break;

    case NK_CTL_ELEM_TYPE_ENUMERATED:
	memcpy(&mixer->mix_val.value.enumerated,
	       &ucontrol->value.enumerated,
	       sizeof(mixer->mix_val.value.enumerated));
	break;
    }
    nkops.nk_xirq_trigger(mixer->cxirq, chip->vlink->s_id);
    down(&chip->mixer_sem);
    return mixer->status != NK_VAUDIO_STATUS_OK;
}

    static int
vaudio_snd_mixer (NkVaudio chip)
{
    struct snd_card* card = chip->card;
    unsigned int     idx;
    struct snd_ctl_elem_info info;
    NkCtlElemInfo    mix_info;

    snprintf(card->mixername, sizeof card->mixername, "Mixer vaudio");

	/* Check struct sizes */
    vaudio_uinfo_id_name_size  = sizeof(info.id.name);
    if (vaudio_uinfo_id_name_size != sizeof(mix_info.name)) {
	DTRACE ("warning different sizes for id.name structure: %d %d\n",
		vaudio_uinfo_id_name_size, sizeof(mix_info.name));
	if (vaudio_uinfo_id_name_size > sizeof(mix_info.name)) {
	    vaudio_uinfo_id_name_size = sizeof(mix_info.name);
	}
    }
    vaudio_uinfo_reserved_size = sizeof(info.reserved);
    if (vaudio_uinfo_reserved_size != sizeof(mix_info.reserved)) {
	DTRACE ("warning different sizes for reserved structure: %d %d\n",
		vaudio_uinfo_reserved_size, sizeof(mix_info.reserved));
	if (vaudio_uinfo_reserved_size > sizeof(mix_info.reserved)) {
	    vaudio_uinfo_reserved_size = sizeof(mix_info.reserved);
	}
    }
    vaudio_uinfo_value_enum_size = sizeof(info.value.enumerated.name);
    if (vaudio_uinfo_value_enum_size !=
	    sizeof(mix_info.value.enumerated.name)) {
	DTRACE ("warning different sizes for value.enumerated.name "
		"structures: %d %d\n", vaudio_uinfo_value_enum_size,
		sizeof(mix_info.value.enumerated.name));
	if (vaudio_uinfo_value_enum_size >
		sizeof(mix_info.value.enumerated.name)) {
	    vaudio_uinfo_value_enum_size =
		sizeof(mix_info.value.enumerated.name);
	}
    }
	/* Registering ALSA mixer controls */
    for (idx = 0; idx < NK_VAUDIO_MIXER_MAX; idx++) {
	struct snd_kcontrol control;
	int reserved;
	int err;

	control.private_data  = chip;
	control.private_value = idx;
	info.value.enumerated.item = 0;
	if (vaudio_snd_info(&control, &info)) {
	    break;
	}
	vaudio_snd_controls[idx].index = 0;
	vaudio_snd_controls[idx].private_value = idx | (info.type << 16);
	memcpy(vaudio_snd_control_names[idx], info.id.name,
	       vaudio_uinfo_id_name_size);
	vaudio_snd_controls[idx].name  = vaudio_snd_control_names[idx];
	vaudio_snd_controls[idx].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	vaudio_snd_controls[idx].info  = vaudio_snd_info;
	vaudio_snd_controls[idx].get   = vaudio_snd_get;
	vaudio_snd_controls[idx].put   = vaudio_snd_put;
	reserved = *(int*)info.reserved;
	if (reserved) {
	    memcpy(vaudio_snd_control_res[idx], info.reserved,
		   vaudio_uinfo_reserved_size);
	    vaudio_snd_controls[idx].tlv.p =
				(unsigned int*)vaudio_snd_control_res[idx];
	    vaudio_snd_controls[idx].access |=
		    SNDRV_CTL_ELEM_ACCESS_READWRITE |
		    SNDRV_CTL_ELEM_ACCESS_TLV_READ;
	}
	if ((err = snd_ctl_add(card, snd_ctl_new1(&vaudio_snd_controls[idx],
			       chip))) < 0)
	    return err;
    }
    return 0;
}

    static int
vaudio_snd_init_card (NkVaudio dev)
{
    int err = 0;
    int i, j;

	/*
	 * Wait until the backend is on.
	 */
    while (dev->vlink->c_state != dev->vlink->s_state) {
	msleep_interruptible(10);
    }
    for (i = 0; i < NK_VAUDIO_DEV_MAX; i++) {
	for (j = 0; j < NK_VAUDIO_STREAM_MAX; j++) {
	    struct snd_pcm_hardware* pcm_hw;
	    const NkVaudioHw* config = dev->s[i][j].hw_conf;

	    if (j == SNDRV_PCM_STREAM_PLAYBACK) {
	        pcm_hw = &vaudio_snd_playback[i][j];
	    } else {
	        pcm_hw = &vaudio_snd_capture[i][j];
	    }
	    pcm_hw->info = (SNDRV_PCM_INFO_INTERLEAVED |
			    SNDRV_PCM_INFO_BLOCK_TRANSFER |
			    SNDRV_PCM_INFO_MMAP |
			    SNDRV_PCM_INFO_MMAP_VALID);
	    if (!(config->stream_cap & HW_CAP_PCM)) {
	        err = -ENODEV;
	        goto nodev;
	    }
	    pcm_hw->formats  = config->pcm.formats;
	    pcm_hw->rates    = config->pcm.rates;
	    pcm_hw->rate_min = config->pcm.rate_min;
	    pcm_hw->rate_max = config->pcm.rate_max;
	    pcm_hw->channels_min = config->pcm.channels_min;
	    pcm_hw->channels_max = config->pcm.channels_max;
	    pcm_hw->buffer_bytes_max = config->pcm.buffer_bytes_max;
	    if (pcm_hw->buffer_bytes_max > NK_VAUDIO_MAX_RING_SIZE) {
		pcm_hw->buffer_bytes_max = NK_VAUDIO_MAX_RING_SIZE;
	    }
	    pcm_hw->period_bytes_min = config->pcm.period_bytes_min;
	    pcm_hw->period_bytes_max = config->pcm.period_bytes_max;
	    pcm_hw->periods_min = config->pcm.periods_min;
	    if (pcm_hw->periods_min < 8) {
		pcm_hw->periods_min = 8;
	    }
	    pcm_hw->periods_max = config->pcm.periods_max;
	    if (pcm_hw->periods_max > NK_VAUDIO_RING_DESC_NB) {
		pcm_hw->periods_max = NK_VAUDIO_RING_DESC_NB;
	    }
	    pcm_hw->fifo_size = config->pcm.fifo_size;
	    if (config->stream_cap & HW_CAP_DMA) {
		dev->s[i][j].use_dma = 1;
	    } else {
		dev->s[i][j].use_dma = 0;
	    }
	}
	if ((err = vaudio_snd_card_pcm(dev, i)) < 0) {
	    WTRACE("device %d initialization failed\n", i);
	}
    }
    if ((err = vaudio_snd_mixer(dev)) < 0)
	goto nodev;

    snprintf (dev->card->driver, sizeof dev->card->driver, "VAUDIO");
    snprintf (dev->card->shortname, sizeof dev->card->shortname,
	      "VIRTUAL AUDIO");
    snprintf (dev->card->longname, sizeof dev->card->longname,
	      "VIRTUAL AUDIO");
    do {
	    /*
	     * Here ALSA sound core might not be setup yet.
	     * We wait until the registering of vaudio card is successful.
	     */
	if ((err = snd_card_register(dev->card)) == 0) {
	    TRACE("audio support initialized\n");
	    return 0;
	}
	msleep_interruptible(10);
    } while (err);

nodev:
    ETRACE ("initialization failed\n");
    return err;
}

static NkVaudio	   vaudio;	/* Pointer to struct _NkVaudio */
static int vaudio_snd_probe (void);
    static int
vaudio_thread (void* data)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    daemonize();
#else
    daemonize("vaudio-fe");
#endif
    data = (void*) vaudio;
    while (!vaudio_thread_aborted) {
	down (&vaudio_thread_sem);

	if ( (vaudio_thread_init_now & 2) ) {
	    vaudio_thread_init_now &= ~2;
	    snd_card_free(vaudio_card);
	    if (vaudio_snd_probe() < 0) {
		ETRACE ("virtual audio ALSA card initialization failed\n");
	    }
	}

	if ( (vaudio_thread_init_now & 1) ) {
	    vaudio_thread_init_now &= ~1;
	    vaudio_snd_init_card(data);
	}
    }
    complete_and_exit(&vaudio_thread_completion, 0);
	/*NOTREACHED*/
    return 0;
}

    static void
vaudio_sysconf_trigger (NkVaudio dev)
{
    DTRACE ("Sending sysconf OS#%d(%d)->OS#%d(%d)\n",
	    dev->vlink->c_id, dev->vlink->c_state,
	    dev->vlink->s_id, dev->vlink->s_state);
    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, dev->vlink->s_id);
}

    static int
vaudio_handshake (NkVaudio dev)
{
    volatile int* my_state   = &dev->vlink->c_state;
    const int	  peer_state =  dev->vlink->s_state;

    DTRACE ("handshake OS#%d(%d)->OS#%d(%d)\n",
	    dev->vlink->s_id, peer_state,
	    dev->vlink->c_id, *my_state);

    switch (*my_state) {
    case NK_DEV_VLINK_OFF:
	if (peer_state != NK_DEV_VLINK_ON) {
	    *my_state = NK_DEV_VLINK_RESET;
	    vaudio_sysconf_trigger(dev);
	}
	break;

    case NK_DEV_VLINK_RESET:
	if (peer_state != NK_DEV_VLINK_OFF) {
	    *my_state = NK_DEV_VLINK_ON;
	    vaudio_thread_init_now |= 1;
	    up(&vaudio_thread_sem);
	    vaudio_sysconf_trigger(dev);
	}
	break;

    case NK_DEV_VLINK_ON:
	if (peer_state == NK_DEV_VLINK_OFF) {
	    *my_state = NK_DEV_VLINK_OFF;
	    vaudio_thread_init_now |= 2;
	    up(&vaudio_thread_sem);
	    vaudio_sysconf_trigger(dev);
	}
	break;
    }
    return (*my_state  == NK_DEV_VLINK_ON) &&
	   (peer_state == NK_DEV_VLINK_ON);
}

    static void
vaudio_sysconf_intr (void* cookie, NkXIrq xirq)
{
    (void) xirq;
    vaudio_handshake ((NkVaudio) cookie);
}

    /* Module init & exit */


    /*
     *  Initializes ALSA sound-card structure.
     */

    static int 
vaudio_snd_probe (void)
{
    NkPhAddr	   pdev;
    NkDevVlink*    vlink;
    NkPhAddr       plink = 0;
    NkXIrq         cxirq;
    NkXIrq         pxirq;
    NkVaudioMixer* mixer;
    const size_t   pdev_size = sizeof(NkDevRing) + sizeof(NkVaudioHw) +
			       sizeof(NkVaudioCtrl) +
			       NK_VAUDIO_RING_DESC_NB * sizeof(NkRingDesc);
    int            cur_stream;
    int            i, j;
    const NkOsId   my_id = nkops.nk_id_get();
    char*          resinfo;
    char*          info;
    int            slot;
#ifdef VAUDIO_CONFIG_NK_PMEM
    NkPhAddr       ring_buf_p;
    int            ring_size;
#endif

    ADEBUG();
	/*
	 * Find the communication vlink.
	 */
    while ((plink = nkops.nk_vlink_lookup("vaudio", plink))) {
	vlink = nkops.nk_ptov(plink);
	if (vlink->c_id == my_id) {
	    break;
	}
    }
    if (!plink) {
	return -ENODEV;
    }
    if (vlink->c_info != 0) {
	info = (char*) nkops.nk_ptov(vlink->c_info);
	slot = simple_strtoul(info, &resinfo, 0);
        if (resinfo == info) {
	    slot = -1;	
	}
    } else {
	slot = -1;
    }
	/*
	 * Perform initialization of NK virtual audio device.
	 */
    pdev = nkops.nk_pdev_alloc (plink, 0, NK_VAUDIO_STREAMS_NB * pdev_size +
				sizeof(NkVaudioMixer));
    if (!pdev) {
	return -ENOMEM;
    }
	/*
	 * Allocate cross-interrupts.
	 */
    cxirq = nkops.nk_pxirq_alloc(plink, 0, vlink->s_id,
				 2 * NK_VAUDIO_STREAMS_NB + 1);
    if (!cxirq) {
	return -ENOMEM;
    }
    pxirq = nkops.nk_pxirq_alloc(plink, 1, vlink->c_id,
				 2 * NK_VAUDIO_STREAMS_NB + 1);
    if (!pxirq) {
	return -ENOMEM;
    }
	/* Register the sound-card */
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,29)
    {
	int err = snd_card_create (slot, "VAUDIO", THIS_MODULE,
				   sizeof *vaudio, &vaudio_card);
	if (err) {
	    return err;
	}
    }
#else
    vaudio_card = snd_card_new (slot, "VAUDIO", THIS_MODULE, sizeof *vaudio);
    if (!vaudio_card) {
	return -ENOMEM;
    }
#endif
	/*
	 * Perform vaudio initialization.
	 * Code relies on *vaudio being zeroed by the alloc call above.
	 */
    vaudio = vaudio_card->private_data;
    vaudio_card->private_free = vaudio_snd_free;
    vaudio->card    = vaudio_card;
    vaudio->vlink   = vlink;
    vaudio->sysconf_xid = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF,
					       vaudio_sysconf_intr, vaudio);
    if (!vaudio->sysconf_xid) {
        snd_card_free(vaudio_card);
	return -EINVAL;
    }
    sema_init(&vaudio->mixer_sem, 0);
#ifdef VAUDIO_CONFIG_NK_PMEM
	/*
	 * The DMA ring buffer is allocated in the PMEM.
	 */
    ring_size  = NK_VAUDIO_MAX_RING_SIZE;
    ring_buf_p = nkops.nk_pmem_alloc(plink, 0,
				     NK_VAUDIO_STREAMS_NB * ring_size);
    if (ring_buf_p == 0) {
	ETRACE ("Cannot alloc %d bytes of pmem\n",
		NK_VAUDIO_STREAMS_NB * ring_size);
        snd_card_free(vaudio_card);
	return -ENOMEM;
    }
#endif
	/*
	 * Perform stream initialization.
	 */
    cur_stream = 0;
    for (i = 0; i < NK_VAUDIO_DEV_MAX; i++) {
	for (j = 0; j < NK_VAUDIO_STREAM_MAX; j++) {
	    NkDevRing*     ring;
	    NkVaudioCtrl*  control;

	    vaudio->s[i][j].vaudio  = vaudio;
	    sema_init(&vaudio->s[i][j].ctrl_sem, 0);
		/*
		 * Perform initialization of the descriptor ring.
		 */
	    ring = nkops.nk_ptov(pdev + cur_stream);
	    ring->cxirq = cxirq++;
	    ring->pxirq = pxirq++;
	    ring->dsize = sizeof(NkRingDesc);
	    ring->imask = NK_VAUDIO_RING_DESC_NB - 1;
	    vaudio->s[i][j].ring = ring;
	    vaudio->s[i][j].ring_xid = nkops.nk_xirq_attach(ring->pxirq,
					vaudio_intr_data, &vaudio->s[i][j]);
	    if (!vaudio->s[i][j].ring_xid) {
                snd_card_free(vaudio_card);
		return -EINVAL;
	    }
		/*
		 * Initialize hardware config pointer.
		 */
	    vaudio->s[i][j].hw_conf = nkops.nk_ptov(pdev + cur_stream +
						    sizeof(NkDevRing));
		/*
		 * Perform initialization of control.
		 */
	    control = nkops.nk_ptov(pdev + cur_stream + sizeof(NkDevRing) +
				    sizeof(NkVaudioHw));
	    control->cxirq = cxirq++;
	    control->pxirq = pxirq++;
	    vaudio->s[i][j].ctrl = control;
	    vaudio->s[i][j].ctrl_xid = nkops.nk_xirq_attach(control->pxirq,
						       vaudio_intr_ctrl,
						       &vaudio->s[i][j]);
	    if (!vaudio->s[i][j].ctrl_xid) {
                snd_card_free(vaudio_card);
		return -EINVAL;
	    }
		/*
		 * Set ring descriptor base address.
		 */
	    vaudio->s[i][j].resp   = 0;
	    vaudio->s[i][j].rbase = nkops.nk_ptov(pdev + cur_stream  +
						  sizeof(NkDevRing)  +
						  sizeof(NkVaudioHw) +
						  sizeof(NkVaudioCtrl));
#ifdef VAUDIO_CONFIG_NK_PMEM
		/*
		 * The DMA ring buffer is allocated in the PMEM.
		 */
	    vaudio->s[i][j].ring_buf_p = ring_buf_p;
	    vaudio->s[i][j].ring_buf_v =
		(unsigned char*) nkops.nk_mem_map(ring_buf_p, ring_size);
	    ring_buf_p += ring_size;
#endif
	    cur_stream += pdev_size;
	}
    }
	/*
	 * Perform initialization of mixer.
	 */
    mixer = nkops.nk_ptov(pdev + NK_VAUDIO_STREAMS_NB * pdev_size);
    mixer->cxirq  = cxirq;
    mixer->pxirq  = pxirq;
    vaudio->mixer = mixer;
    vaudio->mixer_xid = nkops.nk_xirq_attach(mixer->pxirq,
					     vaudio_intr_mixer, vaudio);
    if (!vaudio->mixer_xid) {
        snd_card_free(vaudio_card);
	return -EINVAL;
    }
	/*
	 * Perform handshake until both links are ready.
	 */
    vaudio_handshake(vaudio);
    return 0;
}

    static void __exit
vaudio_exit (void)
{
    ADEBUG();
#if VAUDIO_PROC_SYNC
    vaudio_proc_delete("close");
#endif
    vaudio_thread_aborted = 1;
    up (&vaudio_thread_sem);
    wait_for_completion (&vaudio_thread_completion);

    snd_card_free(vaudio_card);
}

    static int __init
vaudio_init (void)
{
    ADEBUG();
    sema_init(&vaudio_thread_sem, 0);
    if (vaudio_snd_probe() < 0) {
	ETRACE ("virtual audio ALSA card initialization failed\n");
	return -ENODEV;
    }
    vaudio_thread_id = kernel_thread(vaudio_thread, 0, 0);
    if (vaudio_thread_id < 0) {
	ETRACE ("virtual audio kernel thread creation failure \n");
	return vaudio_thread_id;
    }

#if VAUDIO_PROC_SYNC
    vaudio_proc_create("close");
#endif
    return 0;
}

module_init(vaudio_init)
module_exit(vaudio_exit)

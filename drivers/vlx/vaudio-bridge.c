/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Audio (vAudio).                                   *
 *             vAudio bridge kernel driver implementation.                   *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Software. All Rights Reserved.              *
 *                                                                           *
 *  #ident  "%Z%%M% %I%     %E% Red Bend Software"                           *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Pascal Piovesan <pascal.piovesan@redbend.com>                          *
 *                                                                           *
 *****************************************************************************/

#include <linux/delay.h>
#include <nk/nkern.h>

typedef struct RaudioStream {
    int            local;
    int            sid;
    int            did;
    void          *peer;
    void          *dma_cb_cookie;
    NkPhAddr       buf;
    unsigned int   size;
    struct hrtimer hr_timer;
} RaudioStream;

static int backend_connected;
static AudioCallback dma_callback;

static  RaudioStream raudio_stream_l[DEV_MAX][STREAM_MAX];
static  RaudioStream raudio_stream_r[DEV_MAX][STREAM_MAX];

static struct completion stream_copy_thread_completion[DEV_MAX][STREAM_MAX];
static struct semaphore  stream_copy_thread_sem[DEV_MAX][STREAM_MAX];
static pid_t             stream_copy_thread_id[DEV_MAX][STREAM_MAX];
static bool		 stream_copy_thread_aborted;

static unsigned long     hr_time_sec;
static unsigned long     hr_time_nsec;

//#define DEBUG_BRIDGE

    static void*
raudio_open (int dev_id, int stream_id, void* cookie)
{
    RaudioStream *rstream;
    AudioStream  *astream = *(AudioStream**)cookie;

    if (astream->osid == backend_id) {
        rstream = &raudio_stream_l[dev_id][stream_id];
    } else {
        rstream = &raudio_stream_r[dev_id][stream_id];
    }
    rstream->dma_cb_cookie = cookie;
#ifdef DEBUG_BRIDGE
    printk("VAUDIO BRIDGE raudio_open %x %d %d %d %d\n",
	   (unsigned int)rstream, backend_id, astream->osid, dev_id, stream_id);
#endif

    return rstream;
}

    static void
raudio_close (void* cookie)
{
    RaudioStream *rstream;

    rstream = (RaudioStream*)cookie;
    rstream->buf  = 0;
    rstream->size = 0;
    rstream->dma_cb_cookie = 0;
#ifdef DEBUG_BRIDGE
    printk("VAUDIO BRIDGE raudio_close %x\n", (unsigned int)rstream);
#endif
}

    static int
raudio_set_sample (void* cookie, unsigned int channels,
                   AudioFormat  format, unsigned int rate,
                   unsigned int period, unsigned int periods,
                   unsigned int dma_addr)
{
    RaudioStream *rstream;
    unsigned int samples;

    rstream = (RaudioStream*)cookie;

    samples = period / (channels * format_size[format]);

    hr_time_sec  = 0;
    hr_time_nsec = ((samples * 1000000) / rate) * 1000 ;
#ifdef DEBUG_BRIDGE
    printk("VAUDIO BRIDGE raudio_set_sample1 %x channels %d format %d rate %d\n",
	   (unsigned int)rstream, channels, format_size[format], rate);
    printk("VAUDIO BRIDGE raudio_set_sample2 %x period size %d periods %d dma addr %08x\n",
	   (unsigned int)rstream, period, periods, dma_addr);
    printk("VAUDIO BRIDGE raudio_set_sample3 %x ticks %d %d\n",
	   (unsigned int)rstream, (int)hr_time_sec, (int)hr_time_nsec);
#endif
    return 0;
}

   static int
raudio_start (void* cookie, char* buf, unsigned int size)
{
    RaudioStream *rstream1;
    RaudioStream *rstream2;

    rstream1 = (RaudioStream*)cookie;
    rstream1->buf  = (NkPhAddr)buf;
    rstream1->size = size;

    rstream2 = rstream1->peer;
#ifdef DEBUG_BRIDGE
    printk("VAUDIO BRIDGE raudio_start %x %x %x %x\n",
    	    (unsigned int)rstream1, rstream1->buf, (unsigned int)rstream2, rstream2->buf);
#endif
    if (rstream2->buf) {
        if (rstream1->size != rstream2->size) {
	    printk("VAUDIO BRIDGE raudio_start: buf sizes mismatch %d %d\n",
                   rstream1->size, rstream2->size);
            return 1;
        }
	if (rstream1->local) {
	    hrtimer_start(&rstream1->hr_timer,
                          ktime_set(hr_time_sec, hr_time_nsec), HRTIMER_MODE_REL);
            up(&stream_copy_thread_sem[rstream1->did][rstream1->sid]);
	} else {
	    hrtimer_start(&rstream2->hr_timer,
                          ktime_set(hr_time_sec, hr_time_nsec), HRTIMER_MODE_REL);
            up(&stream_copy_thread_sem[rstream2->did][rstream2->sid]);
	}
    }

    return 0;
}

   static void
raudio_stop (void* cookie)
{
    RaudioStream *rstream1;
    RaudioStream *rstream2;

    rstream1 = (RaudioStream*)cookie;
#ifdef DEBUG_BRIDGE
    printk("VAUDIO BRIDGE raudio_stop %x\n", (unsigned int)rstream1);
#endif
    rstream2 = rstream1->peer;
    if (rstream1->local) {
        hrtimer_cancel(&rstream1->hr_timer);
    } else {
        hrtimer_cancel(&rstream2->hr_timer);
    }
}

#define NK_VAUDIO_MIXER_MAX 128

static struct snd_kcontrol* ctrl_controls[NK_VAUDIO_MIXER_MAX];
static snd_kcontrol_get_t*  ctrl_gets    [NK_VAUDIO_MIXER_MAX];
static snd_kcontrol_put_t*  ctrl_puts    [NK_VAUDIO_MIXER_MAX];
static snd_kcontrol_put_t*  ctrl_puts    [NK_VAUDIO_MIXER_MAX];
static unsigned char*       ctrl_names   [NK_VAUDIO_MIXER_MAX];
static int                  ctrl_idx;

    static int
raudio_mixer_info(int idx, struct snd_ctl_elem_info* info)
{
    struct snd_kcontrol* ctrl;

#ifdef DEBUG_BRIDGE
    printk("VAUDIO BRIDGE raudio_mixer_info %d\n", idx);
#endif
    if (idx >= NK_VAUDIO_MIXER_MAX) {
        return 1;
    }
    ctrl = ctrl_controls[idx];
    if (!ctrl) {
        return 1;
    }
    if (ctrl->tlv.p) {
	memcpy(info->reserved, ctrl->tlv.p, sizeof(info->reserved));
    } else {
	memset(info->reserved, 0, sizeof(info->reserved));
    }
    memcpy(info->id.name, ctrl_names[idx], sizeof(info->id.name));

    return ctrl->info(ctrl, info);
}

    static int
raudio_mixer_get(int idx, struct snd_ctl_elem_value* val)
{
    struct snd_kcontrol* ctrl;
    snd_kcontrol_get_t*  get;

#ifdef DEBUG_BRIDGE
    printk("VAUDIO BRIDGE raudio_mixer_get %d\n", idx);
#endif
    if (idx >= NK_VAUDIO_MIXER_MAX) {
	return 1;
    }

    ctrl = ctrl_controls[idx];
    get  = ctrl_gets[idx];
    if (!ctrl || !get) {
	return 1;
    }

    return get(ctrl, val);
}

    static int
raudio_mixer_put(int idx, struct snd_ctl_elem_value* val)
{
    struct snd_kcontrol* ctrl;
    snd_kcontrol_put_t*  put;

#ifdef DEBUG_BRIDGE
    printk("VAUDIO BRIDGE raudio_mixer_put %d\n", idx);
#endif
    if (idx >= NK_VAUDIO_MIXER_MAX) {
	return 1;
    }

    ctrl = ctrl_controls[idx];
    put  = ctrl_puts[idx];
    if (!ctrl || !put) {
	return 1;
    }

    return put(ctrl, val);
}

static RaudioOps raudio_ops = {
    raudio_open,
    raudio_close,
    raudio_set_sample,
    raudio_start,
    raudio_stop,
    raudio_mixer_info,
    raudio_mixer_get,
    raudio_mixer_put
};

static const struct snd_pcm_hardware pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.period_bytes_min	= 16 * 1024,
	.period_bytes_max	= 16 * 1024,
	.periods_min		= 8,
	.periods_max		= 8,
	.buffer_bytes_max	= 128 * 1024,
	.channels_min = 2,
	.channels_max = 2,
        .rate_min = 48000,
        .rate_max = 48000,
	.rates = SNDRV_PCM_RATE_48000,
};

    static enum hrtimer_restart
timer_handler (struct hrtimer *hr_timer)
{
    RaudioStream* rstream1 = container_of(hr_timer, struct RaudioStream, hr_timer);
    RaudioStream* rstream2;

#ifdef DEBUG_BRIDGE
    printk("VAUDIO BRIDGE timer_handler start\n");
#endif
    rstream2 = rstream1->peer;
    rstream1->buf = 0;
    rstream2->buf = 0;
    if (rstream1->dma_cb_cookie) {
        dma_callback(AUDIO_STATUS_OK, rstream1->dma_cb_cookie);
    }
    if (rstream2->dma_cb_cookie) {
        dma_callback(AUDIO_STATUS_OK, rstream2->dma_cb_cookie);
    }
#ifdef DEBUG_BRIDGE
    printk("VAUDIO BRIDGE timer_handler end\n");
#endif
    return HRTIMER_NORESTART;
}

    static int
stream_copy_thread (void* data)
{
    RaudioStream *rstream1;
    RaudioStream *rstream2;
    unsigned char* buf1;
    unsigned char* buf2;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    daemonize();
#else
    daemonize("vaudio-bridge");
#endif
    rstream1 = (RaudioStream*)data;
    while (!stream_copy_thread_aborted) {
        down(&stream_copy_thread_sem[rstream1->did][rstream1->sid]);
        rstream2 = rstream1->peer;
#ifdef DEBUG_BRIDGE
        printk("VAUDIO BRIDGE stream_copy_thread %x %x %x %x\n",
	       (unsigned int)rstream1, rstream1->buf,
               (unsigned int)rstream2, rstream2->buf);
#endif
        if (!rstream1->buf || !rstream2->buf) continue;
        buf1 = nkops.nk_ptov(rstream1->buf);
        buf2 = nkops.nk_ptov(rstream2->buf);
        if (rstream1->sid == SNDRV_PCM_STREAM_PLAYBACK) {
	    memcpy(buf2, buf1,  rstream1->size);
        } else {
	    memcpy(buf1, buf2,  rstream2->size);
        }
    }
    complete_and_exit(&stream_copy_thread_completion[rstream1->did][rstream1->sid], 0);
    /*NOTREACHED*/
    return 0;
}

int  raudio_be_callback(int command, void* ops, void* dma_cb_func, unsigned int* prefetch,
                                           struct snd_pcm_hardware* pcm_hw)
{
    int i, j;
    struct snd_pcm_hardware *cur_stream;

    if (command == RAUDIO_CONNECT) {
	memcpy(ops, &raudio_ops, sizeof(RaudioOps));
	dma_callback = dma_cb_func;
	*prefetch = 0;
	ctrl_idx = 0;

	cur_stream = pcm_hw;
        for (i = 0; i < NK_VAUDIO_DEV_MAX; i++) {
	    for (j = 0; j < NK_VAUDIO_STREAM_MAX; j++) {
	        raudio_stream_l[i][j].did = i;
	        raudio_stream_l[i][j].sid = j;
                if (j == SNDRV_PCM_STREAM_PLAYBACK) {
                    raudio_stream_l[i][j].peer = &raudio_stream_r[i][SNDRV_PCM_STREAM_CAPTURE];
                } else {
                    raudio_stream_l[i][j].peer = &raudio_stream_r[i][SNDRV_PCM_STREAM_PLAYBACK];
                }
	        raudio_stream_l[i][j].local = 1;

	        raudio_stream_r[i][j].did = i;
	        raudio_stream_r[i][j].sid = j;
                if (j == SNDRV_PCM_STREAM_PLAYBACK) {
                    raudio_stream_r[i][j].peer = &raudio_stream_l[i][SNDRV_PCM_STREAM_CAPTURE];
                } else {
                    raudio_stream_r[i][j].peer = &raudio_stream_l[i][SNDRV_PCM_STREAM_PLAYBACK];
                }
	        raudio_stream_r[i][j].local = 0;

		hrtimer_init(&raudio_stream_l[i][j].hr_timer,
                             CLOCK_MONOTONIC, HRTIMER_MODE_REL);
                raudio_stream_l[i][j].hr_timer.function = timer_handler;

	        sema_init(&stream_copy_thread_sem[i][j], 0);
                stream_copy_thread_id[i][j] = kernel_thread(stream_copy_thread,
                                                     (void*)&raudio_stream_l[i][j], 0);

		memcpy(cur_stream, &pcm_hardware, sizeof(struct snd_pcm_hardware));
		cur_stream++;
	    }
	    cur_stream++;
	}

        backend_connected = 1;
	printk(KERN_INFO "vaudio backend connected to bridge\n");
    } else {
        stream_copy_thread_aborted = 1;
        for (i = 0; i < NK_VAUDIO_DEV_MAX; i++) {
	    for (j = 0; j < NK_VAUDIO_STREAM_MAX; j++) {
	        up (&stream_copy_thread_sem[i][j]);
	        if (stream_copy_thread_id[i][j] >= 0) {
	            wait_for_completion (&stream_copy_thread_completion[i][j]);
	        }
	    }
	}

        backend_connected = 0;
	printk(KERN_INFO "vaudio backend disconnected from bridge\n");
    }
    return 0;
}

static BLOCKING_NOTIFIER_HEAD(vaudio_be_notifier_list);
int vaudio_be_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&vaudio_be_notifier_list, nb);
}
EXPORT_SYMBOL(vaudio_be_register_client);
int vaudio_be_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&vaudio_be_notifier_list, nb);
}
EXPORT_SYMBOL(vaudio_be_unregister_client);

/*
 ****************************************************************
 *
 *  Component: VLX ALSA audio driver
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
 *    Chi Dat Truong (chidat.truong@redbend.com)
 *
 ****************************************************************
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION (2,6,27)
#include <sound/driver.h>
#endif
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>

#if 0
#define VAUDIO_DEBUG
#endif

#if 1
#define VAUDIO_USE_RAUDIO
#endif

#if 1
#define VAUDIO_NKDDI_ATOMIC_OPS
#endif

#include "vaudio.h"
#define VAUDIO_PREFIX	"VAUDIO-BE: "
#include "vaudio.c"

MODULE_DESCRIPTION("VLX ALSA audio backend driver");
MODULE_AUTHOR("Pascal Piovesan <pascal.piovesan@redbend.com>");
MODULE_LICENSE("GPL");

/******************************************************************/

    /* Atomic operations */

#define VAUDIO_STATIC_ASSERT(x) \
	extern char vaudio_static_assert [(x) ? 1 : -1]

#ifdef VAUDIO_NKDDI_ATOMIC_OPS

VAUDIO_STATIC_ASSERT (sizeof(nku32_f) == sizeof(unsigned long));

typedef nku32_f		vaudio_atomic_t;
#define vaudio_atomic_read(x)	*(x)

    static inline void
vaudio_atomic_set (vaudio_atomic_t* addr, unsigned long value)
{
    *addr = 0;
}

    static inline void
vaudio_atomic_set_mask (vaudio_atomic_t* addr, unsigned long mask)
{
    nkops.nk_atomic_set (addr, mask);
}

    static inline void
vaudio_atomic_clear_mask (vaudio_atomic_t* addr, unsigned long mask)
{
    nkops.nk_atomic_clear (addr, mask);
}

#else	/* Linux kernel atomic ops */

VAUDIO_STATIC_ASSERT (sizeof(atomic_t) == sizeof(unsigned long));
VAUDIO_STATIC_ASSERT (sizeof(int)      == sizeof(unsigned long));

typedef atomic_t	vaudio_atomic_t;
#define vaudio_atomic_read(x)	atomic_read(x)

    static inline void
vaudio_atomic_set (vaudio_atomic_t* addr, unsigned long value)
{
    atomic_set (addr, value);
}

    static inline void
vaudio_atomic_set_mask (vaudio_atomic_t* addr, unsigned long mask)
{
    int oldval;
    int newval;

    do {
	oldval = atomic_read (addr);
	newval = oldval | mask;
    } while (atomic_cmpxchg (addr, oldval, newval) != oldval);
}

    static inline void
vaudio_atomic_clear_mask (vaudio_atomic_t* addr, unsigned long mask)
{
    atomic_clear_mask (mask, (unsigned long*) &addr->counter);
}
#endif

/******************************************************************/

#define RAUDIO_CONNECT    -1
#define RAUDIO_DISCONNECT -2

typedef enum {
    PCM_FORMAT_S8 = 0,
    PCM_FORMAT_U8,
    PCM_FORMAT_S16_LE,
    PCM_FORMAT_S16_BE,
    PCM_FORMAT_U16_LE,
    PCM_FORMAT_U16_BE,
    PCM_FORMAT_S24_LE,
    PCM_FORMAT_S24_BE,
    PCM_FORMAT_U24_LE,
    PCM_FORMAT_U24_BE,
    PCM_FORMAT_S32_LE,
    PCM_FORMAT_S32_BE,
    PCM_FORMAT_U32_LE,
    PCM_FORMAT_U32_BE
} AudioFormat;

typedef enum AudioStatus {
    AUDIO_STATUS_OK = 0,
    AUDIO_STATUS_ERROR
} AudioStatus;

    /*
     * Audio DMA channel interrupt handler.
     *
     * This handler is called back by the audio driver in the context of
     * a LISR. It must therefore limits its calls to what is allowed
     * in such a context.
     */

typedef void (*AudioCallback) (AudioStatus status, void* cookie);

    /*
     * Real Audio operations.
     */
typedef struct {
    void* (*open) (int dev_id, int stream_id, void* be_cb_cookie);
    void  (*close)(void* cookie);

    int   (*set_sample)(void* cookie, unsigned int channels,
			AudioFormat  format, unsigned int rate,
                        unsigned int period, unsigned int periods,
                        unsigned int dma_addr);
    int   (*start) (void* cookie, char* buf, unsigned int size);
    void  (*stop)  (void* cookie);

    int   (*mixer_info)(int idx, struct snd_ctl_elem_info*  info);
    int   (*mixer_get) (int idx, struct snd_ctl_elem_value* val);
    int   (*mixer_put) (int idx, struct snd_ctl_elem_value* val);
} RaudioOps;

/*****************************************************************/

#ifdef  CONFIG_SND_VAUDIO_BE_BRIDGE
static NkOsId backend_id;
#endif

#if defined(CONFIG_SND_VAUDIO_BE_USER)
#define in_focus (1)
#elif defined(CONFIG_SND_VAUDIO_BE_BRIDGE)
#define in_focus ((osid == vaudio_sw.owner) || (osid == backend_id))
#else
#define in_focus (osid == vaudio_sw.owner)
#endif

static const unsigned char format_size[HW_PCM_FORMAT_U32_BE + 1] = {
    1, 1,
    2, 2, 2, 2,
    3, 3, 3, 3,
    4, 4, 4, 4
};

static const unsigned char format_signed[HW_PCM_FORMAT_U32_BE + 1] = {
    1, 0,
    1, 1, 0, 0,
    1, 1, 0, 0,
    1, 1, 0, 0
};

static const unsigned char format_le[HW_PCM_FORMAT_U32_BE + 1] = {
    0, 0,
    1, 0, 1, 0,
    1, 0, 1, 0,
    1, 0, 1, 0
};

    /*
     * Audio switch driver data.
     */

#define OS_MAX      5
#define DEV_MAX     NK_VAUDIO_DEV_MAX
#define STREAM_MAX  NK_VAUDIO_STREAM_MAX
#define MIX_MAX     NK_VAUDIO_MIXER_MAX

typedef struct {
    bool              opened;	        /* open was called */
    bool              started;	        /* start was called */
    NkOsId            osid;             /* OS id of the stream */
    int               dev;	        /* device */
    int               stream_id;        /* playback or capture id */
    void*             session;          /* playback or capture handle */
    NkStream          stream;           /* vaudio call-back handle */
    NkEventSetRate    set_rate;         /* sample config. per OS */
    unsigned int      pending_dmas;     /* count of pending DMAs */
    struct timer_list timer_play;
} AudioStream;

static struct {
    NkOsId                owner;           /* OS currently owning the audio */
    spinlock_t            lock;
    unsigned int          prefetch;	   /* DMA needs prefetch */
    unsigned int          dev_nb;	   /* device max */
    unsigned int          stream_nb;	   /* stream_max */
    RaudioOps             audio;           /* Real audio device operations */

    vaudio_atomic_t       pending_open;      /* bit mask */
    vaudio_atomic_t       pending_close;     /* bit mask */
    vaudio_atomic_t       pending_set_rate;  /* bit mask */
    vaudio_atomic_t       pending_start;     /* bit mask */
    vaudio_atomic_t       pending_stop;      /* bit mask */
    vaudio_atomic_t       pending_mixer_put; /* bit mask */

    unsigned int          mix_nb;             /* mixer controls number */
    struct snd_ctl_elem_value
			 mix_val[OS_MAX][MIX_MAX]; /* mixer controls per OS */
    int                  mix_idx[OS_MAX];

    NkVaudio             vaudio[OS_MAX]; /* Virtual audio device descriptors */
    AudioStream          s[OS_MAX][DEV_MAX][STREAM_MAX];
    AudioStream*         dma_callbacks[OS_MAX][DEV_MAX][STREAM_MAX];
    void*                hw_sessions[OS_MAX][DEV_MAX][STREAM_MAX];
} vaudio_sw;

#ifdef  CONFIG_SND_VAUDIO_BE_BRIDGE
#include "vaudio-bridge.c"
#endif
#ifdef  CONFIG_SND_VAUDIO_BE_USER
#include "vaudio-user.c"
#endif

static struct completion vaudio_thread_completion[OS_MAX];
static struct semaphore vaudio_thread_sem[OS_MAX];
static pid_t            vaudio_thread_id[OS_MAX];
static bool		vaudio_thread_aborted;

    /*
     * Compute number of ticks to wait for in order to simulate
     * audio buffer processing.
     * <size> is the buffer size in bytes.
     */
    static unsigned int
vaudio_size_to_ticks (AudioStream* s, int size)
{
    unsigned int ticks;
    unsigned int samples;

    samples = size / (s->set_rate.channels * format_size[s->set_rate.format]);
    ticks   = ((samples * HZ) / s->set_rate.rate);
    if (!ticks) {
	ticks = 1;
    }
    return ticks;
}

    /*
     * System timer handler is used to simulate audio processing
     * for the OSes which are _not owner_ of the audio device.
     */
    static void
vaudio_timer_HISR (unsigned long index)
{
    NkPhAddr     addr;
    nku32_f      size;
    int          delay;
    AudioStream* s = (AudioStream*)index;

    OTRACE ("-t-");
    if (!s->pending_dmas) {
        return;
    }
	/*
	 * Acknowledge processing of previous DATA buffer.
	 */
    vaudio_ring_put(s->stream, NK_VAUDIO_STATUS_OK);
    s->pending_dmas--;
	/*
	 * Try to process next buffer (re-arming timeout).
	 */
    if (vaudio_ring_get(s->stream, &addr, &size)) {
	s->pending_dmas++;
        delay = vaudio_size_to_ticks(s, size);
        s->timer_play.expires = jiffies + delay;
	add_timer(&s->timer_play);
    } else {
	while (s->pending_dmas) {
	    vaudio_ring_put(s->stream, NK_VAUDIO_STATUS_OK);
	    s->pending_dmas--;
	}
    }
}

#ifdef VAUDIO_USE_RAUDIO
    /*
     * Audio device DMA transfer call-back.
     * This is called when the audio buffer is processed.
     */
    static void
vaudio_LISR (AudioStatus status, void* cookie)
{
    AudioStream* s;
    NkPhAddr     addr;
    nku32_f	 size;

    (void) status;
    OTRACE ("<d>");
	/*
	 * Check spurious callbacks.
	 */
    if (!cookie) {
        return;
    } else {
        s = *((AudioStream**)cookie);
        if (!s) {
            return;
	}
    }
    if (!s->pending_dmas) {
        return;
    }
	/*
	 * Acknowledge DATA event.
	 */
    vaudio_ring_put(s->stream, NK_VAUDIO_STATUS_OK);
    s->pending_dmas--;
	/*
	 * Try to process next buffer.
	 */
    if (vaudio_ring_get(s->stream, &addr, &size)) {
	s->pending_dmas++;
	vaudio_sw.audio.start(s->session, (char*)addr, size);
    } else {
        while (s->pending_dmas) {
            vaudio_ring_put(s->stream, NK_VAUDIO_STATUS_OK);
            s->pending_dmas--;
        }
    }
}
#endif

    /*
     * Virtual audio event handler (called in HISR context).
     */
    static void
vaudio_HISR (void*         stream,
	     NkVaudioEvent event,
	     void*         params,
	     void*         cookie)
{
    const NkOsId osid = (NkOsId)cookie;
    NkVaudio     vaudio = vaudio_sw.vaudio[osid];
    NkPhAddr     addr = 0;
    nku32_f      size = 0;
    AudioStream* s = (AudioStream*)stream;

    DTRACE ("osid=%d (owner=%d)\n", osid, vaudio_sw.owner);
    switch (event) {
    case NK_VAUDIO_STREAM_OPEN: {
	NkEventOpen* arg = (NkEventOpen*)params;

	DTRACE ("open event\n");
	if (s->opened) {
	    vaudio_event_ack(vaudio, s->stream, event, 0,
			     (nku32_f) NK_VAUDIO_STATUS_ERROR);
	    break;
	}
	if (arg->stream_type != NK_VAUDIO_ST_TYPE_PCM) {
	    vaudio_event_ack(vaudio, s->stream, event, 0,
			     (nku32_f) NK_VAUDIO_STATUS_ERROR);
	    break;
	}
        if (arg->session_type == NK_VAUDIO_SS_TYPE_PLAYBACK) {
	    s->stream_id = SNDRV_PCM_STREAM_PLAYBACK;
	} else if (arg->session_type == NK_VAUDIO_SS_TYPE_CAPTURE) {
	    s->stream_id = SNDRV_PCM_STREAM_CAPTURE;
	} else {
	    vaudio_event_ack(vaudio, s->stream, event, 0,
			     (nku32_f) NK_VAUDIO_STATUS_ERROR);
	    break;
	}
	vaudio_atomic_set_mask (&vaudio_sw.pending_open,
				1 << (s->dev * STREAM_MAX + s->stream_id));
        up(&vaudio_thread_sem[osid]);
	break;
    }
    case NK_VAUDIO_STREAM_CLOSE:
	DTRACE ("close event\n");
	if (!s->opened) {
	    vaudio_event_ack(vaudio, s->stream, event, 0,
			     (nku32_f) NK_VAUDIO_STATUS_ERROR);
	} else {
	    vaudio_atomic_set_mask (&vaudio_sw.pending_close,
				    1 << (s->dev * STREAM_MAX + s->stream_id));
            up(&vaudio_thread_sem[osid]);
	}
	break;

    case NK_VAUDIO_STREAM_SET_RATE: {
	NkEventSetRate* dst = &s->set_rate;
	NkEventSetRate* evt = (NkEventSetRate*)params;

	DTRACE ("set_rate event\n");
	    /*
	     * Save a copy of the sample configuration.
	     */
	dst->channels = evt->channels;
	dst->format   = evt->format;
	dst->rate     = evt->rate;
	dst->period   = evt->period;
	dst->periods  = evt->periods;
	dst->dma_addr = evt->dma_addr;

	vaudio_atomic_set_mask (&vaudio_sw.pending_set_rate,
				1 << (s->dev * STREAM_MAX + s->stream_id));
        up(&vaudio_thread_sem[osid]);
	break;
    }
    case NK_VAUDIO_STREAM_START:
	DTRACE ("start event\n");
	if (!s->opened) {
	    vaudio_event_ack(vaudio, s->stream, event, 0,
			     (nku32_f) NK_VAUDIO_STATUS_ERROR);
	} else {
	    vaudio_atomic_set_mask (&vaudio_sw.pending_start,
				    1 << (s->dev * STREAM_MAX + s->stream_id));
            up(&vaudio_thread_sem[osid]);
	}
	break;

    case NK_VAUDIO_STREAM_DATA: {
	int      res;

	DTRACE ("data event\n");
	if (!s->started) {
	    break;
	}
	res = vaudio_ring_get(s->stream, &addr, &size);
	if (!res) {
#ifdef VAUDIO_DEBUG
	    ETRACE("DATA event and no buffer!\n");
#endif
	    break;
	}
	s->pending_dmas++;
	if (in_focus) {
	    int j = vaudio_sw.prefetch;

	    vaudio_sw.audio.start(s->session, (char*)addr, size);
	    while (j--) {
                if (vaudio_ring_get(s->stream, &addr, &size)) {
	            s->pending_dmas++;
	            vaudio_sw.audio.start(s->session, (char*)addr, size);
                }
	    }
	} else {
            const int delay = vaudio_size_to_ticks(s, size);
            s->timer_play.expires = jiffies + delay;
	    add_timer(&s->timer_play);
	}
	break;
    }
    case NK_VAUDIO_STREAM_STOP:
	DTRACE ("stop event\n");
	vaudio_atomic_set_mask (&vaudio_sw.pending_stop,
				1 << (s->dev * STREAM_MAX + s->stream_id));
        up(&vaudio_thread_sem[osid]);
	break;

    case NK_VAUDIO_STREAM_MIXER: {
	NkEventMixer* evt = (NkEventMixer*)params;
	int           res = NK_VAUDIO_STATUS_OK;

	DTRACE ("mixer event idx %d cmd %d\n", evt->mix_idx, evt->mix_cmd);
        if (evt->mix_idx >= vaudio_sw.mix_nb) {
	    vaudio_event_ack(vaudio, 0, event, 0,
			     (nku32_f) NK_VAUDIO_STATUS_ERROR);
	    break;
	}
	switch (evt->mix_cmd) {
	case NK_VAUDIO_MIXER_INFO: {
	    struct snd_ctl_elem_info info;
	    int			     reserved;

	    info.value.enumerated.item = evt->mix_info.value.enumerated.item;
	    res = vaudio_sw.audio.mixer_info(evt->mix_idx, &info);

	    memcpy(evt->mix_info.name, info.id.name,
		   sizeof(evt->mix_info.name));
	    reserved = *(int*)info.reserved;
	    if (reserved) {
		memcpy(evt->mix_info.reserved, info.reserved,
		       sizeof(evt->mix_info.reserved));
	    }
	    switch (info.type) {
	    case SNDRV_CTL_ELEM_TYPE_BOOLEAN:
		evt->mix_info.type  = NK_CTL_ELEM_TYPE_BOOLEAN;
		evt->mix_info.count = info.count;
		evt->mix_info.value.integer.min = info.value.integer.min;
		evt->mix_info.value.integer.max = info.value.integer.max;
		break;

	    case SNDRV_CTL_ELEM_TYPE_INTEGER:
		evt->mix_info.type  = NK_CTL_ELEM_TYPE_INTEGER;
		evt->mix_info.count = info.count;
		evt->mix_info.value.integer.min = info.value.integer.min;
		evt->mix_info.value.integer.max = info.value.integer.max;
		break;

	    case SNDRV_CTL_ELEM_TYPE_ENUMERATED:
		evt->mix_info.type  = NK_CTL_ELEM_TYPE_ENUMERATED;
		evt->mix_info.count = info.count;
		evt->mix_info.value.enumerated.items =
			info.value.enumerated.items;
		memcpy(evt->mix_info.value.enumerated.name,
		       info.value.enumerated.name,
		       sizeof(info.value.enumerated.name));
		break;

	    default:
		break;
	    }
	    vaudio_event_ack(vaudio, 0, event, evt, res);
	    break;
	}
	case NK_VAUDIO_MIXER_GET: {
	    struct snd_ctl_elem_value* val =
		&vaudio_sw.mix_val[osid][evt->mix_idx];

	    switch (evt->mix_type) {
	    case NK_CTL_ELEM_TYPE_BOOLEAN:
	    case NK_CTL_ELEM_TYPE_INTEGER:
		memcpy(&evt->mix_val.value.integer, &val->value.integer,
		       sizeof(evt->mix_val.value.integer));
		break;

	    case NK_CTL_ELEM_TYPE_ENUMERATED:
		memcpy(&evt->mix_val.value.enumerated, &val->value.enumerated,
		       sizeof(evt->mix_val.value.enumerated));
		break;
	    }
	    vaudio_event_ack(vaudio, 0, event, evt, res);
	    break;
	}
	case NK_VAUDIO_MIXER_PUT: {
	    struct snd_ctl_elem_value* val =
		&vaudio_sw.mix_val[osid][evt->mix_idx];

	    switch (evt->mix_type) {
	    case NK_CTL_ELEM_TYPE_BOOLEAN:
	    case NK_CTL_ELEM_TYPE_INTEGER:
		memcpy(&val->value.integer, &evt->mix_val.value.integer,
		       sizeof(evt->mix_val.value.integer));
		break;

	    case NK_CTL_ELEM_TYPE_ENUMERATED:
		memcpy(&val->value.enumerated, &evt->mix_val.value.enumerated,
		       sizeof(evt->mix_val.value.enumerated));
		break;
	    }
	    if (in_focus) {
		vaudio_atomic_set_mask (&vaudio_sw.pending_mixer_put,
					1 << osid);
		vaudio_sw.mix_idx[osid] = evt->mix_idx;
		up(&vaudio_thread_sem[osid]);
	    } else {
		vaudio_event_ack(vaudio, 0, event, evt, res);
	    }
	    break;
	}
	}
	break;
    }
    default:
	WTRACE ("unknown event %d\n", event);
	break;
    }
    DTRACE ("end\n");
}

    static inline void
vaudio_set_hardware_session (const int os_id, const int dev_id, const int stream_id,
			     void* session)
{
    vaudio_sw.hw_sessions[os_id][dev_id][stream_id] = session;
}

    static inline void*
vaudio_get_hardware_session (const int os_id, const int dev_id, const int stream_id)
{
    return vaudio_sw.hw_sessions[os_id][dev_id][stream_id];
}

static DEFINE_MUTEX(vaudio_hardware_stream_lock);

    static AudioStatus
vaudio_stream_open (AudioStream* audio_stream, const bool focused)
{
    const int osid = audio_stream->osid;
    const int did = audio_stream->dev;
    const int sid = audio_stream->stream_id;
    AudioStatus status = NK_VAUDIO_STATUS_OK;
    void* hw_session;

    mutex_lock(&vaudio_hardware_stream_lock);
    audio_stream->opened = 1;
    audio_stream->session = NULL;
    vaudio_sw.dma_callbacks[osid][did][sid] = audio_stream;
    if (focused) {
	/*
	 * Try to open a hardware session.
	 */
        hw_session = vaudio_get_hardware_session(osid, did, sid);
        if (hw_session == NULL) {
	    /*
	     * Create a unique hardware session.
	     */
	    hw_session = vaudio_sw.audio.open(did, sid,
					  &vaudio_sw.dma_callbacks[osid][did][sid]);
	    vaudio_set_hardware_session(osid, did, sid, hw_session);
	    if (hw_session) {
		audio_stream->session = hw_session;
	    } else {
	        status = NK_VAUDIO_STATUS_ERROR;
	    }
        }
    }
    mutex_unlock(&vaudio_hardware_stream_lock);
    return status;
}

    static void
vaudio_stream_close (AudioStream* audio_stream, const bool focused)
{
    const int osid = audio_stream->osid;
    const int did = audio_stream->dev;
    const int sid = audio_stream->stream_id;

    audio_stream->session = NULL;
    audio_stream->opened = 0;
    mutex_lock(&vaudio_hardware_stream_lock);
    if (focused) {
        void* hw_session = vaudio_get_hardware_session(osid, did, sid);
	    /* We suppose here that DMA transfers are correctly stopped */
	if (hw_session) {
	    vaudio_sw.audio.close(hw_session);
        }
	vaudio_set_hardware_session(osid, did, sid, NULL);
    }
    mutex_unlock(&vaudio_hardware_stream_lock);
}

    /*
     * Processing thread.
     */
    static int
vaudio_thread (void* data)
{
    const NkOsId osid = (NkOsId)data;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    daemonize();
#else
    daemonize("vaudio-be");
#endif
    while (!vaudio_thread_aborted) {
        down(&vaudio_thread_sem[osid]);

	while (vaudio_atomic_read (&vaudio_sw.pending_start)) {
	    const nku32_f j = nkops.nk_mask2bit
		(vaudio_atomic_read (&vaudio_sw.pending_start));
	    const int did = j / STREAM_MAX;
	    const int sid = j % STREAM_MAX;
	    AudioStream* s = &vaudio_sw.s[osid][did][sid];
	    NkPhAddr addr;
	    nku32_f  size;

	    if (in_focus) {
		int k = vaudio_sw.prefetch + 1;

		while (k--) {
		    if (vaudio_ring_get(s->stream,
					&addr, &size)){
			s->pending_dmas++;
			vaudio_sw.audio.start(s->session,
						  (char*)addr, size);
		    }
		}
	    } else {
		if (vaudio_ring_get(s->stream, &addr, &size)) {
		    int delay = vaudio_size_to_ticks(s, size);
		    s->pending_dmas++;
		    s->timer_play.expires = jiffies + delay;
		    add_timer(&s->timer_play);
		}
	    }
	    s->started = 1;
	    vaudio_atomic_clear_mask (&vaudio_sw.pending_start, 1 << j);
	}
	while (vaudio_atomic_read (&vaudio_sw.pending_stop)) {
	    const nku32_f j = nkops.nk_mask2bit
		(vaudio_atomic_read (&vaudio_sw.pending_stop));
	    const int did = j / STREAM_MAX;
	    const int sid = j % STREAM_MAX;
	    AudioStream* s = &vaudio_sw.s[osid][did][sid];

	    if (s->started) {
		if (in_focus) {
		    vaudio_sw.audio.stop(s->session);
		} else {
		    del_timer(&s->timer_play);
		}
		while (s->pending_dmas) {
		    vaudio_ring_put(s->stream,
				    NK_VAUDIO_STATUS_OK);
		    s->pending_dmas--;
		}
		s->started = 0;
	    }
	    vaudio_atomic_clear_mask (&vaudio_sw.pending_stop, 1 << j);
	}
	while (vaudio_atomic_read (&vaudio_sw.pending_open)) {
	    const nku32_f j = nkops.nk_mask2bit
		(vaudio_atomic_read (&vaudio_sw.pending_open));
	    int res = NK_VAUDIO_STATUS_OK;
	    const int did = j / STREAM_MAX;
	    const int sid = j % STREAM_MAX;
	    AudioStream* s = &vaudio_sw.s[osid][did][sid];

	    if (!s->opened) {
		res = vaudio_stream_open(s, in_focus);
		if (res == NK_VAUDIO_STATUS_OK) {
		    DTRACE ("open session=%x type=%d\n",
			    (unsigned int)s->session, s->stream_id);
		}
	    }
	    vaudio_atomic_clear_mask (&vaudio_sw.pending_open, 1 << j);
	    vaudio_event_ack(vaudio_sw.vaudio[osid], s->stream,
			     NK_VAUDIO_STREAM_OPEN, 0, res);
	}
	while (vaudio_atomic_read (&vaudio_sw.pending_close)) {
	    const nku32_f j = nkops.nk_mask2bit
		(vaudio_atomic_read (&vaudio_sw.pending_close));
	    const int did = j / STREAM_MAX;
	    const int sid = j % STREAM_MAX;
	    AudioStream* s = &vaudio_sw.s[osid][did][sid];

	    if (s->opened) {
		vaudio_stream_close(s, in_focus);
	    }
	    vaudio_atomic_clear_mask (&vaudio_sw.pending_close, 1 << j);
	    vaudio_event_ack(vaudio_sw.vaudio[osid], s->stream,
			     NK_VAUDIO_STREAM_CLOSE, 0,
			     NK_VAUDIO_STATUS_OK);
	}
	while (vaudio_atomic_read (&vaudio_sw.pending_set_rate)) {
	    const nku32_f j = nkops.nk_mask2bit
		(vaudio_atomic_read (&vaudio_sw.pending_set_rate));
	    int res = NK_VAUDIO_STATUS_OK;
	    const int did = j / STREAM_MAX;
	    const int sid = j % STREAM_MAX;
	    AudioStream* s = &vaudio_sw.s[osid][did][sid];
	    NkEventSetRate* evt = &s->set_rate;

	    if (in_focus) {
		res = vaudio_sw.audio.set_sample(s->session,
			     evt->channels, evt->format,
			     evt->rate, evt->period, evt->periods,
			     evt->dma_addr);
	    }
	    vaudio_atomic_clear_mask (&vaudio_sw.pending_set_rate, 1 << j);
	    vaudio_event_ack(vaudio_sw.vaudio[osid], s->stream,
			     NK_VAUDIO_STREAM_SET_RATE, 0, res);
	}
        if (vaudio_atomic_read (&vaudio_sw.pending_mixer_put)) {
            const int                  idx = vaudio_sw.mix_idx[osid];
	    struct snd_ctl_elem_value* val = &vaudio_sw.mix_val[osid][idx];
            int res = vaudio_sw.audio.mixer_put(idx, val);

	    if (res >= 0) {
		res = NK_VAUDIO_STATUS_OK;
	    }
            vaudio_atomic_set (&vaudio_sw.pending_mixer_put, 0);
	    vaudio_event_ack(vaudio_sw.vaudio[osid], 0, NK_VAUDIO_STREAM_MIXER,
			     0, res);
	}
    }
    complete_and_exit(&vaudio_thread_completion [osid], 0);
    /*NOTREACHED*/
    return 0;
}

#ifndef CONFIG_SND_VAUDIO_BE_USER
/*
 * Process to do when switching focus:
 *  - Stop activities (dma or timers) of 'from' and 'to' VMs.
 *  - Close opened hardware streams of 'from' VM.
 *  - Apply mixers settings
 *  - Open hardware streams of 'to' VM.
 *  - Switch streams
 */
    static void
vaudio_switch_focused_streams (const NkOsId from, const NkOsId to)
{
    NkEventSetRate* evt;
    unsigned long   flags;
    unsigned int    i, j, k;
    NkPhAddr        addr;
    nku32_f         size;
    int             delay;
    void*           hw_session;

    for (j = 0; j < DEV_MAX; j++) {
        for (k = 0; k < STREAM_MAX; k++) {
	    AudioStream* sfrom = &vaudio_sw.s[from][j][k];
	    AudioStream* sto = &vaudio_sw.s[to][j][k];

	    spin_lock_irqsave(&vaudio_sw.lock, flags);
		/*
		 * Stop "owner's" DMA transfer, if any.
		 */
#ifndef  CONFIG_SND_VAUDIO_BE_BRIDGE
	    if (sfrom->started) {
#else
	    if ((from != backend_id) && sfrom->started) {
#endif
		if (sfrom->session) {
		    vaudio_sw.audio.stop(sfrom->session);
		}
		else {
		    del_timer(&sfrom->timer_play);
		}
		while (sfrom->pending_dmas) {
		    vaudio_ring_put(sfrom->stream, NK_VAUDIO_STATUS_OK);
		    sfrom->pending_dmas--;
		}
            }

		/*
		 * Stop "to's" timers, if any.
		 */
#ifndef  CONFIG_SND_VAUDIO_BE_BRIDGE
	    if (sto->started) {
#else
	    if ((to != backend_id) && sto->started) {
#endif
		del_timer(&sto->timer_play);
		while (sto->pending_dmas) {
		    vaudio_ring_put(sto->stream, NK_VAUDIO_STATUS_OK);
		    sto->pending_dmas--;
		}
	    }
	    spin_unlock_irqrestore(&vaudio_sw.lock, flags);

		/*
		 * Close hardware stream
		 */
#ifndef  CONFIG_SND_VAUDIO_BE_BRIDGE
            if (sfrom->opened) {
#else
            if ((from != backend_id) && sfrom->opened) {
#endif
	        hw_session = vaudio_get_hardware_session(from, j, k);
	            /* We suppose here that DMA transfers are correctly stopped */
	        if (hw_session) {
	            vaudio_sw.audio.close(hw_session);
	        }
	        vaudio_set_hardware_session(from, j, k, NULL);
	    }
	}
    }

	/*
	 * Apply new owner's volume configuration
	 * then switch to new owner.
	 */
    for (i = 0; i < vaudio_sw.mix_nb; i++) {
        vaudio_sw.audio.mixer_put(i, &vaudio_sw.mix_val[to][i]);
    }

    for (j = 0; j < DEV_MAX; j++) {
        for (k = 0; k < STREAM_MAX; k++) {
	    AudioStream* sfrom = &vaudio_sw.s[from][j][k];
	    AudioStream* sto = &vaudio_sw.s[to][j][k];

#ifndef  CONFIG_SND_VAUDIO_BE_BRIDGE
	    if (sto->opened) {
#else
            if ((to != backend_id) && sto->opened) {
#endif
	        /*
	         * Try to open a hardware session.
	         */
	       hw_session = vaudio_get_hardware_session(to, j, k);
               if (hw_session == NULL) {
	            /*
	             * Create a unique hardware session.
	             */
		    hw_session = vaudio_sw.audio.open(j, k,
					  &vaudio_sw.dma_callbacks[to][j][k]);
                    vaudio_set_hardware_session(to, j, k, hw_session);
	        }
            }
		/*
		 * Switch hardware sessions.
		 */
	    vaudio_sw.owner = to;
#ifdef  CONFIG_SND_VAUDIO_BE_BRIDGE
            if (to != backend_id) {
#endif
	    vaudio_sw.dma_callbacks[to][j][k] = sto;
	    sto->session = vaudio_get_hardware_session(to, j, k);
#ifdef  CONFIG_SND_VAUDIO_BE_BRIDGE
            if (from != backend_id)
#endif
	    sfrom->session = NULL;
	    evt = &sto->set_rate;
	    if (sto->session && evt && evt->channels) {
		(void)vaudio_sw.audio.set_sample(sto->session, evt->channels,
						 evt->format, evt->rate,
						 evt->period, evt->periods,
						 evt->dma_addr);
	    }
#ifdef  CONFIG_SND_VAUDIO_BE_BRIDGE
	    }
#endif
	    spin_lock_irqsave(&vaudio_sw.lock, flags);
		/*
		 * Notify "from", if required ... using timers.
		 */
#ifndef  CONFIG_SND_VAUDIO_BE_BRIDGE
	    if (sfrom->started) {
#else
            if ((from != backend_id) && sfrom->started) {
#endif
	        if (vaudio_ring_get(sfrom->stream, &addr, &size)) {
	            delay = vaudio_size_to_ticks(sfrom, size);
		    sfrom->pending_dmas++;
	            sfrom->timer_play.expires = jiffies + delay;
		    add_timer(&sfrom->timer_play);
		}
	    }
		/*
		 * Notify "to", if required ... using real audio device.
		 */
#ifndef  CONFIG_SND_VAUDIO_BE_BRIDGE
	    if (sto->started) {
#else
            if ((to != backend_id) && sto->started) {
#endif
	        i = vaudio_sw.prefetch + 1;
		while (i--) {
	            if (vaudio_ring_get(sto->stream, &addr, &size)) {
		        sto->pending_dmas++;
		        vaudio_sw.audio.start(sto->session, (char*)addr,
						  size);
	            }
		}
	    }
	    spin_unlock_irqrestore(&vaudio_sw.lock, flags);
	}
    }
}
#endif

extern int focus_register_client  (struct notifier_block *nb);
extern int focus_unregister_client(struct notifier_block *nb);

static struct notifier_block focus_nb;

#ifndef CONFIG_SND_VAUDIO_BE_USER
    /*
     * Audio device switch routine.
     */
    static int
vaudio_notify_focus (struct notifier_block* self,
		     unsigned long          event,
		     void*                  data)
{
    const NkOsId    to    = (NkOsId) event;
    const NkOsId    from  = vaudio_sw.owner;

    (void) self;
    (void) data;
    if (to != from) {
	vaudio_switch_focused_streams(from, to);
    }
    return NOTIFY_DONE;
}
#endif

    /*
     * Driver initialization.
     */

#ifdef VAUDIO_USE_RAUDIO
extern int raudio_be_callback(int, void*, void*, unsigned int*,
                              struct snd_pcm_hardware* pcm_hw);
extern int vaudio_be_register_client  (struct notifier_block *nb);
extern int vaudio_be_unregister_client(struct notifier_block *nb);

#pragma weak raudio_be_callback
#pragma weak vaudio_be_register_client
#pragma weak vaudio_be_unregister_client

static struct notifier_block vaudio_be_nb;
static int vaudio_init(void);

    static int
vaudio_be_audio_init (struct notifier_block *nb, unsigned long p1, void *p2)
{
    (void) nb;
    (void) p1;
    (void) p2;
    return vaudio_init();
}
#endif /* VAUDIO_USE_RAUDIO */

    static void
vaudio_cleanup (void)
{
    NkOsId osid;

    for (osid = NK_OS_PRIM; osid < OS_MAX; osid++) {
	if (!vaudio_sw.vaudio[osid]) continue;
	vaudio_destroy (vaudio_sw.vaudio[osid]);
    }
}

    static int
vaudio_init (void)
{
    unsigned                 frontends = 0;
    int                      res;
    int                      i;
    NkOsId                   osid;
    struct snd_pcm_hardware* pcm_hw;
    struct snd_pcm_hardware* cur_pcm_hw;
    const int                pcm_size = sizeof(struct snd_pcm_hardware) *
				DEV_MAX * STREAM_MAX;
    NkVaudioHw*              nk_pcm_hw;
    NkVaudioHw*              cur_nk_pcm_hw;
    const int                nk_pcm_size = sizeof(NkVaudioHw) * DEV_MAX *
				STREAM_MAX;

#ifdef VAUDIO_USE_RAUDIO
    if (!raudio_be_callback || !vaudio_be_register_client ||
	!vaudio_be_unregister_client) {
	ETRACE ("No VLX-adapted real audio driver.\n");
	return -EINVAL;
    }
#endif
        /*
	 * Open real audio driver.
	 */
    DTRACE ("opening real audio driver\n");
    pcm_hw = (struct snd_pcm_hardware*)kmalloc(pcm_size, GFP_KERNEL);
    if (pcm_hw == 0) {
	return -ENOMEM;
    }
    nk_pcm_hw = (NkVaudioHw*)kmalloc(nk_pcm_size, GFP_KERNEL);
    if (nk_pcm_hw == 0) {
	kfree (pcm_hw);
	return -ENOMEM;
    }
    spin_lock_init(&vaudio_sw.lock);
#ifdef VAUDIO_USE_RAUDIO
    res = raudio_be_callback(RAUDIO_CONNECT, &vaudio_sw.audio, vaudio_LISR,
			     &vaudio_sw.prefetch, pcm_hw);
    if (res) {
	TRACE ("No AUDIO driver found, waiting ...\n");
        kfree(pcm_hw);
        kfree(nk_pcm_hw);
        vaudio_be_nb.notifier_call = vaudio_be_audio_init;
        res = vaudio_be_register_client(&vaudio_be_nb);
        if (res) {
	    ETRACE ("vaudio_be_register_client() failed (%d)", res);
	    return res;
        }
	return 0;
    }
    if (vaudio_be_nb.notifier_call) {
        vaudio_be_unregister_client(&vaudio_be_nb);
	vaudio_be_nb.notifier_call = NULL;
    }
#endif
#ifndef CONFIG_SND_VAUDIO_BE_USER
        /*
	 * Register as a switchable HID device.
	 */
    DTRACE ("registering HID switch\n");
    focus_nb.notifier_call = vaudio_notify_focus;
    res = focus_register_client(&focus_nb);
    if (res) {
	ETRACE ("focus_register_client() failed (%d)\n", res);
#ifdef VAUDIO_USE_RAUDIO
	raudio_be_callback (RAUDIO_DISCONNECT, NULL, NULL, NULL, NULL); /* Disconnect */
#endif
        kfree(pcm_hw);
        kfree(nk_pcm_hw);
	return res;
    }
#endif
        /*
	 * Create virtual audio devices.
	 */
    cur_pcm_hw = pcm_hw;
    cur_nk_pcm_hw = nk_pcm_hw;
    for (i = 0; i < DEV_MAX; i++) {
	int j;

        for (j = 0; j < STREAM_MAX; j++) {
	    cur_nk_pcm_hw->pcm.formats      = cur_pcm_hw->formats;
	    cur_nk_pcm_hw->pcm.rates        = cur_pcm_hw->rates;
	    cur_nk_pcm_hw->pcm.rate_min     = cur_pcm_hw->rate_min;
	    cur_nk_pcm_hw->pcm.rate_max     = cur_pcm_hw->rate_max;
	    cur_nk_pcm_hw->pcm.channels_min = cur_pcm_hw->channels_min;
	    cur_nk_pcm_hw->pcm.channels_max = cur_pcm_hw->channels_max;
	    cur_nk_pcm_hw->pcm.buffer_bytes_max = cur_pcm_hw->buffer_bytes_max;
	    cur_nk_pcm_hw->pcm.period_bytes_min = cur_pcm_hw->period_bytes_min;
	    cur_nk_pcm_hw->pcm.period_bytes_max = cur_pcm_hw->period_bytes_max;
	    cur_nk_pcm_hw->pcm.periods_min = cur_pcm_hw->periods_min;
	    cur_nk_pcm_hw->pcm.periods_max = cur_pcm_hw->periods_max;
	    cur_nk_pcm_hw->pcm.fifo_size = cur_pcm_hw->fifo_size;
#if defined(CONFIG_SND_VAUDIO_BE_BRIDGE) || defined(CONFIG_SND_VAUDIO_BE_USER)
	    cur_nk_pcm_hw->stream_cap    = HW_CAP_PCM;
#else
	    cur_nk_pcm_hw->stream_cap    = HW_CAP_PCM | HW_CAP_DMA;
#endif
	    cur_nk_pcm_hw++;
	    cur_pcm_hw++;
	}
    }
#ifdef  CONFIG_SND_VAUDIO_BE_BRIDGE
    backend_id = nkops.nk_id_get();
#endif
    vaudio_sw.owner = nkops.nk_id_get();
    for (osid = NK_OS_PRIM; osid < OS_MAX; osid++) {
	if (!vaudio_configured (osid)) continue;
	DTRACE ("creating vaudio (os=%d)\n", osid);
	    /*
	     * vaudio_HISR() can signal this semaphore, so initialize
	     * it before creating the handler.
	     */
	sema_init(&vaudio_thread_sem[osid], 0);
	vaudio_sw.vaudio[osid] = vaudio_create (osid, vaudio_HISR,
						(void*)osid, nk_pcm_hw);
	if (!vaudio_sw.vaudio[osid]) {
	    WTRACE ("vaudio_create(osid=%d) failed\n", osid);
	    continue;
	}
	++frontends;
	for (i = 0; i < DEV_MAX; i++) {
	    int j;

	    for (j = 0; j < STREAM_MAX; j++) {
		vaudio_sw.vaudio[osid]->stream[i][j].cookie =
		    &vaudio_sw.s[osid][i][j];
		vaudio_sw.s[osid][i][j].stream =
		    &vaudio_sw.vaudio[osid]->stream[i][j];
		vaudio_sw.hw_sessions[osid][i][j] = NULL;
		vaudio_sw.dma_callbacks[osid][i][j] = NULL;
	    }
	}
    }
    kfree(pcm_hw);
    kfree(nk_pcm_hw);
        /*
	 * Create timers in disabled state.
	 */
    for (osid = NK_OS_PRIM; osid < OS_MAX; osid++) {
	if (!vaudio_sw.vaudio[osid]) {
	    continue;
	}
        for (i = 0; i < DEV_MAX; i++) {
	    int j;

            for (j = 0; j < STREAM_MAX; j++) {
		AudioStream* s = &vaudio_sw.s[osid][i][j];

                init_timer(&s->timer_play);
                s->timer_play.function = vaudio_timer_HISR;
                s->timer_play.data     = (unsigned long)s;
                s->dev  = i;
                s->osid = osid;
	    }
	}
    }
        /*
	 * Find audio controls.
	 */
    vaudio_sw.mix_nb = 0;
    for (i = 0; i < MIX_MAX; i++) {
	int j;

#ifndef VAUDIO_USE_RAUDIO
        if (!vaudio_sw.audio.mixer_get) {
	    break;
	}
#endif
        if (vaudio_sw.audio.mixer_get(i, &vaudio_sw.mix_val[0][i])) {
	    break;
        }
        for (j = NK_OS_PRIM; j < OS_MAX; j++) {
	    vaudio_sw.mix_val[j][i] = vaudio_sw.mix_val[0][i];
	}
        vaudio_sw.mix_nb++;
    }
    for (osid = NK_OS_PRIM; osid < OS_MAX; osid++) {
	if (!vaudio_sw.vaudio[osid]) {
	    continue;
	}
	if (!vaudio_start (vaudio_sw.vaudio[osid])) {
	    ETRACE ("vaudio_start() %d failed\n", osid);
	    vaudio_cleanup();
	    return -ENOMEM;
	}
    }
        /*
	 * Start the vaudio control threads.
	 */
    for (osid = NK_OS_PRIM; osid < OS_MAX; osid++) {
	if (!vaudio_sw.vaudio[osid]) {
	    continue;
	}
	init_completion (&vaudio_thread_completion [osid]);
        vaudio_thread_id[osid] = kernel_thread(vaudio_thread, (void*)osid, 0);
	    /* TBD LATER */
	if (vaudio_thread_id[osid] < 0) {
	    WTRACE("thread for guest %d did not start\n", osid);
	}
    }
    TRACE ("module loaded, %u frontend(s)\n", frontends);
    return 0;
}

    static void __exit
vaudio_exit (void)
{
    NkOsId osid;

    vaudio_thread_aborted = 1;
    for (osid = NK_OS_PRIM; osid < OS_MAX; osid++) {
	if (!vaudio_sw.vaudio[osid]) {
	    continue;
	}
	up (&vaudio_thread_sem [osid]);
	if (vaudio_thread_id[osid] >= 0) {
	    wait_for_completion (&vaudio_thread_completion [osid]);
	}
    }
    for (osid = NK_OS_PRIM; osid < OS_MAX; osid++) {
	int i;

	if (!vaudio_sw.vaudio[osid]) {
	    continue;
	}
        for (i = 0; i < DEV_MAX; i++) {
	    int j;

            for (j = 0; j < STREAM_MAX; j++) {
		del_timer_sync (&vaudio_sw.s[osid][i][j].timer_play);
	    }
	}
    }
    focus_unregister_client (&focus_nb);
#ifdef VAUDIO_USE_RAUDIO
    if (vaudio_be_nb.notifier_call) {
	vaudio_be_unregister_client (&vaudio_be_nb);
    }
    raudio_be_callback (-2, NULL, NULL, NULL, NULL); /* Disconnect */
#endif
    vaudio_cleanup();
    TRACE ("module unloaded\n");
}

module_init(vaudio_init);
module_exit(vaudio_exit);

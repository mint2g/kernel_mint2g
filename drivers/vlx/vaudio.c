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
 *
 ****************************************************************
 */

#include <linux/gfp.h>
#include <linux/module.h>

#include <nk/nkern.h>
#include <nk/nkdev.h>
#include "vaudio.h"

#ifndef VAUDIO_PREFIX
#define VAUDIO_PREFIX	"VAUDIO: "
#endif

#define TRACE(x...)	printk (VAUDIO_PREFIX x)
#define ETRACE(x...)	TRACE ("Error: " x)
#define WTRACE(x...)	TRACE ("Warning: " x)
#define OTRACE(x...)

#ifdef VAUDIO_DEBUG
#define DTRACE(format, args...) \
	TRACE("%s: " format, __func__, ##args)
#else
#define DTRACE(format, args...)
#endif

struct _NkStream {
    NkVaudioCtrl* ctrl;
    NkXIrqId      ctrl_xid;
    NkDevRing*    ring;
    NkXIrqId      ring_xid;
    NkRingDesc*   rbase;
    nku32_f	  req;
    NkVaudio	  vaudio;
    void*         cookie;
};

struct _NkVaudio {
    NkDevVlink*          vlink;
    NkXIrqId             sysconf_xid;
    NkVaudioMixer*       mixer;
    NkXIrqId             mixer_xid;
    NkVaudioEventHandler hdl;
    void*                cookie;
    struct _NkStream     stream[NK_VAUDIO_DEV_MAX][NK_VAUDIO_STREAM_MAX];
};

    static NkDevVlink*
vaudio_lookup_vlink (const NkOsId id)
{
    NkPhAddr	 plink = 0;
    const NkOsId my_id = nkops.nk_id_get();

    while ((plink = nkops.nk_vlink_lookup("vaudio", plink)) != 0) {
	NkDevVlink* vlink = (NkDevVlink*) nkops.nk_ptov(plink);

	if ((vlink->s_id == my_id) && (vlink->c_id == id))
	    return vlink;
    }
    return 0;
}

    static void
vaudio_sysconf_trigger (const NkVaudio vaudio)
{
    DTRACE("Sending sysconf OS#%d(%d)->OS#%d(%d)\n",
	   vaudio->vlink->s_id, vaudio->vlink->s_state,
	   vaudio->vlink->c_id, vaudio->vlink->c_state);
    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, vaudio->vlink->c_id);
}

    static int
vaudio_handshake (const NkVaudio vaudio)
{
    volatile int* my_state   = &vaudio->vlink->s_state;
    const int     peer_state =  vaudio->vlink->c_state;

    DTRACE("handshake OS#%d(%d)->OS#%d(%d)\n",
	   vaudio->vlink->c_id, peer_state, vaudio->vlink->s_id, *my_state);

    switch (*my_state) {
    case NK_DEV_VLINK_OFF:
	if (peer_state != NK_DEV_VLINK_ON) {
	    int i, j;

	    *my_state = NK_DEV_VLINK_RESET;
	    for (i = 0; i < NK_VAUDIO_DEV_MAX; i++) {
		for (j = 0; j < NK_VAUDIO_STREAM_MAX; j++) {
		    vaudio->stream [i] [j].ring->iresp = 0;
		}
	    }
	    vaudio_sysconf_trigger(vaudio);
	}
	break;

    case NK_DEV_VLINK_RESET:
	if (peer_state != NK_DEV_VLINK_OFF) {
	    *my_state = NK_DEV_VLINK_ON;
	    vaudio_sysconf_trigger(vaudio);
	}
	break;

    case NK_DEV_VLINK_ON:
	if (peer_state == NK_DEV_VLINK_OFF) {
	    int i, j;

	    *my_state = NK_DEV_VLINK_RESET;
	    for (i = 0; i < NK_VAUDIO_DEV_MAX; i++) {
		for (j = 0; j < NK_VAUDIO_STREAM_MAX; j++) {
		    NkStream stream = &vaudio->stream [i] [j];

		    stream->ring->iresp = 0;
		    vaudio->hdl (stream->cookie, NK_VAUDIO_STREAM_STOP,  0,
				 vaudio->cookie);
		    vaudio->hdl (stream->cookie, NK_VAUDIO_STREAM_CLOSE, 0,
				 vaudio->cookie);
		}
	    }
	    vaudio_sysconf_trigger (vaudio);
	}
	break;
    }
    return (*my_state  == NK_DEV_VLINK_ON) &&
           (peer_state == NK_DEV_VLINK_ON);
}

    static void
vaudio_sysconf_xirq (void* cookie, NkXIrq xirq)
{
    (void) xirq;
    vaudio_handshake ((NkVaudio) cookie);
}

    static void
vaudio_ring_xirq (void* cookie, NkXIrq xirq)
{
    NkStream	stream = (NkStream)cookie;
    NkVaudio	vaudio = stream->vaudio;

    (void) xirq;
    vaudio->hdl(stream->cookie, NK_VAUDIO_STREAM_DATA, 0, vaudio->cookie);
}

    static void
vaudio_ctrl_xirq (void* cookie, NkXIrq xirq)
{
    const NkStream stream = (NkStream)cookie;
    const NkVaudio vaudio = stream->vaudio;
    NkVaudioCtrl*  ctrl = stream->ctrl;

    (void) xirq;
    switch (ctrl->command) {
    case NK_VAUDIO_COMMAND_OPEN: {
	NkEventOpen params;

	params.session_type = ctrl->session_type;
	params.stream_type  = ctrl->stream_type;
	vaudio->hdl(stream->cookie, NK_VAUDIO_STREAM_OPEN, &params,
		    vaudio->cookie);
	stream->req = 0;
	stream->ring->iresp = 0;
	stream->ring->ireq  = 0;
	break;
    }
    case NK_VAUDIO_COMMAND_CLOSE: {
	NkEventClose params;

	params.session_type = ctrl->session_type;
	params.stream_type  = ctrl->stream_type;
	vaudio->hdl(stream->cookie, NK_VAUDIO_STREAM_CLOSE, &params,
		    vaudio->cookie);
	break;
    }
    case NK_VAUDIO_COMMAND_START:
	vaudio->hdl(stream->cookie, NK_VAUDIO_STREAM_START, 0, vaudio->cookie);
	break;

    case NK_VAUDIO_COMMAND_STOP:
	vaudio->hdl(stream->cookie, NK_VAUDIO_STREAM_STOP, 0, vaudio->cookie);
	break;

    case NK_VAUDIO_COMMAND_SET_RATE: {
	NkEventSetRate params;

	params.rate     = ctrl->rate;
	params.channels = ctrl->channels;
	params.format   = ctrl->format;
	params.period   = ctrl->period;
	params.periods  = ctrl->periods;
	params.dma_addr = ctrl->dma_addr;
	vaudio->hdl(stream->cookie, NK_VAUDIO_STREAM_SET_RATE, &params,
		    vaudio->cookie);
	break;
    }
    default:
	WTRACE("Ignoring invalid ctrl command %x\n", ctrl->command);
	break;
    }
}

    static void
vaudio_mixer_xirq (void* cookie, NkXIrq xirq)
{
    const NkVaudio  vaudio = (NkVaudio)cookie;
    NkVaudioMixer*  mixer = vaudio->mixer;

    (void) xirq;
    switch (mixer->command) {
    case NK_VAUDIO_COMMAND_MIXER: {
	NkEventMixer params;

	params.mix_cmd  = mixer->mix_cmd;
	params.mix_idx  = mixer->mix_idx;
	params.mix_type = mixer->mix_type;
	if (params.mix_cmd == NK_VAUDIO_MIXER_PUT) {
	    params.mix_val = mixer->mix_val;
	}
	if (params.mix_cmd == NK_VAUDIO_MIXER_INFO) {
	    params.mix_info = mixer->mix_info;
	}
	vaudio->hdl(0, NK_VAUDIO_STREAM_MIXER, &params, vaudio->cookie);
	break;
    }
    default:
	WTRACE("Ignoring invalid mixer command %x\n", mixer->command);
	break;
    }
}

    void
vaudio_destroy (const NkVaudio vaudio)
{
    int i, j;

    for (i = 0; i < NK_VAUDIO_DEV_MAX; i++) {
        for (j = 0; j < NK_VAUDIO_STREAM_MAX; j++) {
	    NkStream stream = &vaudio->stream [i] [j];

	    if (stream->ring_xid) {
		nkops.nk_xirq_detach (stream->ring_xid);
            }
	    if (stream->ctrl_xid) {
		nkops.nk_xirq_detach (stream->ctrl_xid);
            }
	}
    }
    if (vaudio->mixer_xid) {
	nkops.nk_xirq_detach(vaudio->mixer_xid);
    }
    if (vaudio->sysconf_xid) {
	nkops.nk_xirq_detach(vaudio->sysconf_xid);
    }
    kfree(vaudio);
}

#ifdef VAUDIO_DEBUG
    static void
vaudio_print_NkVaudioHw (int stream, const NkVaudioHw* hw)
{
    TRACE ("stream %d: stream_cap %d fifo_size %x\n", stream,
	hw->stream_cap, hw->pcm.fifo_size);
    TRACE ("formats %llx rates %x (%d..%d)\n", (long long) hw->pcm.formats,
	hw->pcm.rates, hw->pcm.rate_min, hw->pcm.rate_max);
    TRACE ("channels %d..%d, buffer 0x%x\n", hw->pcm.channels_min,
	hw->pcm.channels_max, hw->pcm.buffer_bytes_max);
    TRACE ("period bytes 0x%x..0x%x, periods %d..%d\n",
	hw->pcm.period_bytes_min, hw->pcm.period_bytes_max,
	hw->pcm.periods_min, hw->pcm.periods_max);
}
#endif

    /*
     * See .h for full documentation.
     */
    bool
vaudio_configured (const NkOsId osid)
{
    return vaudio_lookup_vlink(osid) != NULL;
}

    NkVaudio
vaudio_create (const NkOsId	    osid,
	       const NkVaudioEventHandler hdl,
	       void*                cookie,
	       NkVaudioHw* const    hw_conf)
{
    NkPhAddr	   pdev;
    NkDevVlink*    vlink;
    NkPhAddr       plink;
    NkXIrq         cxirq;
    NkXIrq         pxirq;
    NkVaudioMixer* mixer;
    NkVaudio	   vaudio;
    const int      pdev_size = sizeof(NkDevRing) + sizeof(NkVaudioHw) +
				sizeof(NkVaudioCtrl) +
				NK_VAUDIO_RING_DESC_NB * sizeof(NkRingDesc);
    NkVaudioHw*    cur_hw_conf;
    int            cur_stream;
    int            i;

    DTRACE ("pdev_size %d\n", pdev_size);
	/*
	 * Check the vlink for this guest OS.
	 */
    vlink = vaudio_lookup_vlink(osid);
    if (!vlink) {
        return (NkVaudio) 0;
    }
    plink = nkops.nk_vtop(vlink);
	/*
	 * Perform initialization of NK virtual audio device.
	 */
    pdev = nkops.nk_pdev_alloc(plink, 0,
                               NK_VAUDIO_STREAMS_NB * pdev_size +
                               sizeof(NkVaudioMixer));
    if (!pdev) {
        return (NkVaudio) 0;
    }
	/*
	 * Allocate cross-interrupts.
	 */
    cxirq = nkops.nk_pxirq_alloc(plink, 0, vlink->s_id,
                                 2 * NK_VAUDIO_STREAMS_NB + 1);
    if (!cxirq) {
        return (NkVaudio) 0;
    }
    pxirq = nkops.nk_pxirq_alloc(plink, 1, vlink->c_id,
                                 2 * NK_VAUDIO_STREAMS_NB + 1);
    if (!pxirq) {
        return (NkVaudio) 0;
    }
	/*
	 * Perform vaudio initialization.
	 */
    vaudio = (NkVaudio)kmalloc(sizeof *vaudio, GFP_KERNEL);
    if (!vaudio) {
        return (NkVaudio) 0;
    }
    memset (vaudio, 0, sizeof *vaudio);
    vaudio->vlink   = vlink;
    vaudio->hdl     = hdl;
    vaudio->cookie  = cookie;
	/*
	 * Perform streams initialization.
	 */
    cur_hw_conf = hw_conf;
    cur_stream  = 0;
    for (i = 0; i < NK_VAUDIO_DEV_MAX; i++) {
	int j;

        for (j = 0; j < NK_VAUDIO_STREAM_MAX; j++) {
	    NkStream		stream = &vaudio->stream [i] [j];
	    NkDevRing*		ring;
	    NkVaudioHw*		conf;
	    NkVaudioCtrl*	control;

            stream->vaudio  = vaudio;
		/*
		 * Perform initialization of descriptor ring.
		 */
            ring = (NkDevRing*) nkops.nk_ptov(pdev + cur_stream);
            ring->cxirq = cxirq++;
            ring->pxirq = pxirq++;
            ring->dsize = sizeof(NkRingDesc);
            ring->imask = NK_VAUDIO_RING_DESC_NB - 1;
            stream->ring = ring;
		/*
		 * Perform initialization of hardware config.
		 */
            conf = (NkVaudioHw*) nkops.nk_ptov(pdev + cur_stream +
                                 sizeof(NkDevRing));
            *conf = *cur_hw_conf;
#ifdef VAUDIO_DEBUG
	    vaudio_print_NkVaudioHw (cur_stream, cur_hw_conf);
#endif
            cur_hw_conf++;
		/*
		 * Perform initialization of control.
		 */
            control = (NkVaudioCtrl*) nkops.nk_ptov(pdev + cur_stream +
                                    sizeof(NkDevRing) +
                                    sizeof(NkVaudioHw));
            control->cxirq = cxirq++;
            control->pxirq = pxirq++;
            stream->ctrl = control;
		/*
		 * Set ring descriptor base address.
		 */
            stream->req   = 0;
            stream->rbase = (NkRingDesc*)
		nkops.nk_ptov (pdev + cur_stream  + sizeof(NkDevRing) +
			       sizeof(NkVaudioHw) + sizeof(NkVaudioCtrl));
	    cur_stream += pdev_size;
		/* Remain to initialize: ring_xid, ctrl_xid, cookie */
        }
    }
	/*
	 * Perform initialization of mixer.
	 */
    mixer = (NkVaudioMixer*)
	nkops.nk_ptov(pdev + NK_VAUDIO_STREAMS_NB * pdev_size);
    mixer->cxirq    = cxirq;
    mixer->pxirq    = pxirq;
    vaudio->mixer   = mixer;

    return vaudio;
}

    int
vaudio_start (const NkVaudio vaudio)
{
    int i;
	/*
	 * Attach all XIRQ handlers.
	 */
    vaudio->sysconf_xid = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF,
					       vaudio_sysconf_xirq, vaudio);
    if (!vaudio->sysconf_xid) {
	return 0;
    }
    for (i = 0; i < NK_VAUDIO_DEV_MAX; i++) {
	int j;

	for (j = 0; j < NK_VAUDIO_STREAM_MAX; j++) {
	    NkStream stream = &vaudio->stream [i] [j];

	    stream->ring_xid = nkops.nk_xirq_attach (stream->ring->cxirq,
						     vaudio_ring_xirq, stream);
	    if (!stream->ring_xid) {
		return 0;
	    }
	    stream->ctrl_xid = nkops.nk_xirq_attach (stream->ctrl->cxirq,
						     vaudio_ctrl_xirq, stream);
	    if (!stream->ctrl_xid) {
		return 0;
	    }
	}
    }
    vaudio->mixer_xid = nkops.nk_xirq_attach(vaudio->mixer->cxirq,
					     vaudio_mixer_xirq, vaudio);
    if (!vaudio->mixer_xid) {
	return 0;
    }
	/*
	 * Perform handshake until both links are ready.
	 */
    vaudio_handshake(vaudio);
    return 1;
}

    int
vaudio_ring_get (NkStream s, NkPhAddr* addr, nku32_f* size)
{
    NkDevRing*  ring = s->ring;
    const nku32_f mask = ring->imask;
    const nku32_f nreq = ring->ireq & mask;
    const nku32_f oreq = s->req & mask;
    NkRingDesc* desc;

    if (oreq == nreq) {
        return 0;
    }
    s->req++;
    desc = s->rbase + oreq;
    *addr = desc->bufaddr;
    *size = desc->bufsize;
    return 1;
}

    void
vaudio_ring_put (NkStream s, nku32_f status)
{
    NkDevVlink* vlink = s->vaudio->vlink;
    NkDevRing*  ring  = s->ring;
    const nku32_f mask  = ring->imask;
    const nku32_f nresp = ring->iresp & mask;
    NkRingDesc* desc  = s->rbase + nresp;

    desc->status = status;
    ring->iresp++;
    if (vlink->c_state == NK_DEV_VLINK_ON) {
        nkops.nk_xirq_trigger(ring->pxirq, vlink->c_id);
    }
}

    void
vaudio_event_ack (NkVaudio vaudio, NkStream s,
                  NkVaudioEvent event, void* params, nku32_f status)
{
    NkDevVlink*    vlink =  vaudio->vlink;
    NkVaudioMixer* mixer =  vaudio->mixer;

#ifdef VAUDIO_DEBUG
    if (status != NK_VAUDIO_STATUS_OK) {
	ETRACE ("event %d status %d\n", event, status);
    }
#endif
    if (event == NK_VAUDIO_STREAM_MIXER) {
	    /*
	     * The check for params being non-NULL is to satisfy
	     * PC-Lint only. Indeed, vaudio_thread() in vaudio-be.c
	     * sometimes calls us in such a condition, but mixer->mix_cmd
	     * is then equal to NK_VAUDIO_MIXER_PUT, so the NULL pointer
	     * is not dereferenced.
	     */
        if (status == NK_VAUDIO_STATUS_OK && params != NULL) {
            NkEventMixer* mix_ev = (NkEventMixer*)params;

            if (mixer->mix_cmd == NK_VAUDIO_MIXER_INFO) {
                mixer->mix_info = mix_ev->mix_info;
	    }
            if (mixer->mix_cmd == NK_VAUDIO_MIXER_GET) {
                mixer->mix_val = mix_ev->mix_val;
	    }
	}
        mixer->status = status;
        if (vlink->c_state == NK_DEV_VLINK_ON) {
            nkops.nk_xirq_trigger(mixer->pxirq, vlink->c_id);
	}
    } else if (event != NK_VAUDIO_STREAM_DATA) {
        NkVaudioCtrl*  ctrl  =  s->ctrl;

        ctrl->status = status;
        if (vlink->c_state == NK_DEV_VLINK_ON) {
            nkops.nk_xirq_trigger(ctrl->pxirq, vlink->c_id);
	}
    }
}

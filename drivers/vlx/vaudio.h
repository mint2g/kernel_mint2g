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

    /*! \file vaudio.h
     *  \brief Provide virtual AUDIO driver interface
     */

#ifndef VAUDIO_H
#define VAUDIO_H

#include <nk/nkern.h>

#define NK_VAUDIO_DEV_MAX     2
#define NK_VAUDIO_STREAM_MAX  2
#define NK_VAUDIO_STREAMS_NB  (NK_VAUDIO_DEV_MAX * NK_VAUDIO_STREAM_MAX)

    /*!
     * This defines the vaudio types.
     */
typedef struct _NkVaudio* NkVaudio;
typedef struct _NkStream* NkStream;

    /*!
     * Here are the virtual AUDIO events.
     */
typedef enum {
	/*!
	 * Indicate that remote site has opened the device.
	 */
    NK_VAUDIO_STREAM_OPEN               = 0x0,
	/*!
	 * Indicate that remote site has closed the device.
	 */
    NK_VAUDIO_STREAM_CLOSE              = 0x1,
	/*!
	 * Start the audio stream.
	 */
    NK_VAUDIO_STREAM_START              = 0x2,
	/*!
	 * Stop the audio stream.
	 */
    NK_VAUDIO_STREAM_STOP               = 0x3,
	/*!
	 * Set the rate of the audio stream.
	 */
    NK_VAUDIO_STREAM_SET_RATE           = 0x4,
	/*!
	 * Indicate that a data buffer is available.
	 */
    NK_VAUDIO_STREAM_DATA               = 0x5,
	/*!
	 * Get/set stream volume control.
	 */
    NK_VAUDIO_STREAM_MIXER              = 0x6,

    NK_VAUDIO_EVENT_MAX			= 0x7
} NkVaudioEvent;

    /*!
     * Values for the session_type field of the NkEventOpen event.
     */
#define NK_VAUDIO_SS_TYPE_INVAL      0
#define NK_VAUDIO_SS_TYPE_PLAYBACK   1
#define NK_VAUDIO_SS_TYPE_CAPTURE    2

    /*!
     * Values for the stream_type field of the NkEventOpen event.
     */
#define NK_VAUDIO_ST_TYPE_INVAL      0
#define NK_VAUDIO_ST_TYPE_PCM        1

    /*!
     * NK_VAUDIO_STREAM_OPEN event parameters:
     *   session_type : playback or capture
     *   stream_type  : PCM only
     */
typedef struct {
    nku8_f	  session_type;
    nku8_f	  stream_type;
} NkEventOpen;

typedef NkEventOpen NkEventClose;

    /*!
     * Values for the format field of the NkEventSetRate event.
     */
enum hw_pcm_format {
    HW_PCM_FORMAT_S8 = 0,
    HW_PCM_FORMAT_U8,
    HW_PCM_FORMAT_S16_LE,
    HW_PCM_FORMAT_S16_BE,
    HW_PCM_FORMAT_U16_LE,
    HW_PCM_FORMAT_U16_BE,
    HW_PCM_FORMAT_S24_LE,
    HW_PCM_FORMAT_S24_BE,
    HW_PCM_FORMAT_U24_LE,
    HW_PCM_FORMAT_U24_BE,
    HW_PCM_FORMAT_S32_LE,
    HW_PCM_FORMAT_S32_BE,
    HW_PCM_FORMAT_U32_LE,
    HW_PCM_FORMAT_U32_BE
};

    /*!
     * NK_VAUDIO_STREAM_SET_RATE event parameters:
     *   channels : number of channels
     *   format   : endianess and size of samples
     *   rate     : rate in HZ
     *   period   : period size in bytes
     *   periods  : number of periods in the ring buffer
     *   dma_addr : dma physical address of the ring buffer
     */
typedef struct {
    nku8_f        channels;
    nku8_f        format;
    nku32_f	  rate;
    nku32_f	  period;
    nku32_f	  periods;
    nku32_f	  dma_addr;
} NkEventSetRate;

    /*!
     * Values for the mix_cmd field of the NkEventMixer event.
     */
#define NK_VAUDIO_MIXER_INFO        1
#define NK_VAUDIO_MIXER_GET         2
#define NK_VAUDIO_MIXER_PUT         3

    /*!
     * Values for the mix_info.type field of the NkEventMixer event.
     */
#define NK_CTL_ELEM_TYPE_NONE       0
#define NK_CTL_ELEM_TYPE_BOOLEAN    1
#define NK_CTL_ELEM_TYPE_INTEGER    2
#define NK_CTL_ELEM_TYPE_ENUMERATED 3
#define NK_CTL_ELEM_TYPE_LAST       NK_CTL_ELEM_TYPE_ENUMERATED

    /*!
     * NK_VAUDIO_STREAM_MIXER event parameters.
     */
#define NK_VAUDIO_MIXER_MAX 128

typedef struct {
    nku8_f   name[64];
    nku32_f  type;
    nku32_f  count;
    union {
	struct {
	    nku32_f min;
	    nku32_f max;
	    nku32_f step;
	} integer;
	struct {
	    nku32_f items;
	    nku32_f item;
	    nku8_f  name[64];
	} enumerated;
    } value;
    nku8_f  reserved[64];
} NkCtlElemInfo;

typedef struct {
    union {
        struct {
	    nku32_f value[8];
	} integer;
	struct {
	    nku32_f item[8];
	} enumerated;
    } value;
} NkCtlElemValue;

typedef struct {
    nku32_f        mix_cmd;
    nku32_f        mix_idx;
    nku32_f        mix_type;
    NkCtlElemInfo  mix_info;
    NkCtlElemValue mix_val;
} NkEventMixer;

    /*!
     * Call-back function prototype for virtual AUDIO events.
     * The NkVaudioEventHandler is called by the virtual AUDIO interrupt
     * handler on occurrence of an NkVaudioEvent event.
     *
     * So an event handler should not use APIs which are not allowed by the
     * underlying operating system within interrupt handlers.
     *
     *   stream : NkStream cookie.
     *   event  : NkVaudioEvent event.
     *   params : pointer to event parameters.
     *   cookie : cookie given to vaudio_create().
     */

typedef void (*NkVaudioEventHandler) (void*           stream,
				      NkVaudioEvent   event,
				      void*           params,
				      void*           cookie);

    /*!
     * Values for the formats field of the NkVaudioHwPcm configuration.
     */
#define HW_PCM_FMTBIT_S8                (1ULL << HW_PCM_FORMAT_S8)
#define HW_PCM_FMTBIT_U8                (1ULL << HW_PCM_FORMAT_U8)
#define HW_PCM_FMTBIT_S16_LE		(1ULL << HW_PCM_FORMAT_S16_LE)
#define HW_PCM_FMTBIT_S16_BE		(1ULL << HW_PCM_FORMAT_S16_BE)
#define HW_PCM_FMTBIT_U16_LE		(1ULL << HW_PCM_FORMAT_U16_LE)
#define HW_PCM_FMTBIT_U16_BE		(1ULL << HW_PCM_FORMAT_U16_BE)
#define HW_PCM_FMTBIT_S24_LE		(1ULL << HW_PCM_FORMAT_S24_LE)
#define HW_PCM_FMTBIT_S24_BE		(1ULL << HW_PCM_FORMAT_S24_BE)
#define HW_PCM_FMTBIT_U24_LE		(1ULL << HW_PCM_FORMAT_U24_LE)
#define HW_PCM_FMTBIT_U24_BE		(1ULL << HW_PCM_FORMAT_U24_BE)
#define HW_PCM_FMTBIT_S32_LE		(1ULL << HW_PCM_FORMAT_S32_LE)
#define HW_PCM_FMTBIT_S32_BE		(1ULL << HW_PCM_FORMAT_S32_BE)
#define HW_PCM_FMTBIT_U32_LE		(1ULL << HW_PCM_FORMAT_U32_LE)
#define HW_PCM_FMTBIT_U32_BE		(1ULL << HW_PCM_FORMAT_U32_BE)

    /*!
     * Values for the rates field of the NkVaudioHwPcm configuration.
     */
#define HW_PCM_RATE_5512                (1<<0)		/* 5512Hz */
#define HW_PCM_RATE_8000                (1<<1)		/* 8000Hz */
#define HW_PCM_RATE_11025		(1<<2)		/* 11025Hz */
#define HW_PCM_RATE_16000		(1<<3)		/* 16000Hz */
#define HW_PCM_RATE_22050		(1<<4)		/* 22050Hz */
#define HW_PCM_RATE_32000		(1<<5)		/* 32000Hz */
#define HW_PCM_RATE_44100		(1<<6)		/* 44100Hz */
#define HW_PCM_RATE_48000		(1<<7)		/* 48000Hz */
#define HW_PCM_RATE_64000		(1<<8)		/* 64000Hz */
#define HW_PCM_RATE_88200		(1<<9)		/* 88200Hz */
#define HW_PCM_RATE_96000		(1<<10)		/* 96000Hz */
#define HW_PCM_RATE_176400		(1<<11)		/* 176400Hz */
#define HW_PCM_RATE_192000		(1<<12)		/* 192000Hz */

    /*! NkVaudioHwPcm : configuration for the PCM stream type.
     *   formats      : PCM formats supported
     *   rates        : sample rates supported
     *   rate_min     : minimal rate in HZ
     *   rate_min     : maximal rate in HZ
     *   channels_min : minimal number of channels
     *   channels_max : maximal number of channels
     */
typedef struct {
    nku64_f formats;
    nku32_f rates;
    nku32_f rate_min;
    nku32_f rate_max;
    nku32_f channels_min;
    nku32_f channels_max;
    nku32_f buffer_bytes_max;
    nku32_f period_bytes_min;
    nku32_f period_bytes_max;
    nku32_f periods_min;
    nku32_f periods_max;
    nku32_f fifo_size;
    nku32_f pad64;
} NkVaudioHwPcm;

    /*!
     * Values for the stream_cap field of the NkVaudioHw configuration.
     */
#define HW_CAP_PCM    0x00000001
#define HW_CAP_DMA    0x80000000

    /*! NkVaudioHw : hardware configuration for the audio device.
     *    stream_cap : supported stream types
     *    pcm        : PCM configuration
     */
typedef struct {
    nku32_f		stream_cap;
    nku32_f		pad64;
    NkVaudioHwPcm	pcm;
} NkVaudioHw;

    /*!
     * Error codes for the vaudio_ring_put and vaudio_event_ack
     * status parameters.
     */
#define NK_VAUDIO_STATUS_OK         0
#define NK_VAUDIO_STATUS_ERROR     -1

#define NK_VAUDIO_COMMAND_NONE      0x00000000
#define NK_VAUDIO_COMMAND_OPEN      0x00000001
#define NK_VAUDIO_COMMAND_CLOSE     0x00000002
#define NK_VAUDIO_COMMAND_START     0x00000004
#define NK_VAUDIO_COMMAND_STOP      0x00000008
#define NK_VAUDIO_COMMAND_SET_RATE  0x00000010
#define NK_VAUDIO_COMMAND_MIXER     0x00000020

    /*!
     * Virtual audio control definition.
     */
typedef struct {
    nku32_f	   cxirq;
    nku32_f	   pxirq;
    nku32_f	   command;
    nku32_f	   status;

    nku8_f	   session_type;
    nku8_f	   stream_type;
    nku8_f         channels;
    nku8_f         format;
    nku32_f	   rate;
    nku32_f	   period;
    nku32_f	   periods;
    nku32_f	   dma_addr;
} NkVaudioCtrl;

typedef struct {
    nku32_f	   cxirq;
    nku32_f	   pxirq;
    nku32_f	   command;
    nku32_f	   status;

    nku32_f        mix_cmd;
    nku32_f        mix_idx;
    nku32_f        mix_type;
    NkCtlElemInfo  mix_info;
    NkCtlElemValue mix_val;
} NkVaudioMixer;

    /*!
     * Ring of descriptors definition.
     */
#define NK_VAUDIO_RING_DESC_NB   64
#define NK_VAUDIO_MAX_RING_SIZE  (128 * 1024)

typedef struct NkRingDesc {
    nku32_f	  status;
    nku32_f	  bufsize;
    NkPhAddr	  bufaddr;
} NkRingDesc;

    /*!
     * This routine checks if vaudio is configured for a given frontend
     * osid.
     *
     * \param osid    : NkOsId of the frontend driver
     * \return In case of success this routine returns TRUE.
     */
bool		vaudio_configured(NkOsId osid);

    /*!
     * This routine creates a virtual audio device.
     *
     * \param osid    : NkOsId of the frontend driver.
     * \param hdl     : event handler.
     * \param cookie  : client cookie passed back to the event handler.
     * \param hw_conf : hardware AUDIO configuration.
     * \return In case of success this routine returns a handle
     *	to the created vaudio. Otherwise 0 is returned in case of failure.
     */
NkVaudio        vaudio_create(NkOsId		   osid,
			      NkVaudioEventHandler hdl,
			      void*                cookie,
                              NkVaudioHw*	   hw_conf);

    /*!
     * This routine starts a virtual audio device: attaches cross-interrupt
     * handlers and initiates the handshake with peer.
     *
     * \param vaudio  : handle returned by vaudio_create().
     * \return In case of success this routine returns 1.
     *	Otherwise 0 is returned and vaudio_destroy() must be called
     *  to clean up.
     */
int		vaudio_start (NkVaudio vaudio);

    /*!
     * This routine destroys a virtual audio device.
     *
     */
void		vaudio_destroy (const NkVaudio vaudio);

    /*!
     * Get a data buffer from the virtual AUDIO device.
     * If the opened stream is in playback mode, the buffer
     * contains data to be played by the audio device.
     * If the opened stream is in capture mode, the buffer
     * will be filled by the audio device.
     *
     * \param stream : the stream handle.
     * \param addr   : pointer filled with the physical address of the buffer.
     * \param size   : pointer filled with the size of the buffer.
     * \return 1 in case of success.
     * \return 0 when no buffer is available.
     */
int             vaudio_ring_get(NkStream         stream,
                                NkPhAddr*	 addr,
                                nku32_f*         size);

    /*!
     * Put a data buffer to the virtual AUDIO device.
     * If the opened stream is in capture mode, the buffer
     * contains data captured by the audio device.
     * If the opened stream is in playback mode, the buffer
     * has been played by the audio device.
     *
     * \param stream : the stream handle.
     * \param status : status of the buffer:
     *                 NK_VAUDIO_STATUS_OK    operation has succeeded
     *                 NK_VAUDIO_STATUS_ERROR operation has failed
     */
void            vaudio_ring_put(NkStream         stream,
                                nku32_f          status);

    /*!
     * Acknowledge an event to the virtual AUDIO device.
     *
     * \param vaudio : the virtual audio device handle.
     * \param stream : the stream handle.
     * \param event  : the event to be acknowledged.
     * \param params : pointer to the event parameters.
     * \param status : status of the event:
     *                 NK_VAUDIO_STATUS_OK    operation has succeeded
     *                 NK_VAUDIO_STATUS_ERROR operation has failed
     */
void            vaudio_event_ack(NkVaudio         vaudio,
                                 NkStream         stream,
                                 NkVaudioEvent    event,
                                 void*            params,
                                 nku32_f          status);
#endif

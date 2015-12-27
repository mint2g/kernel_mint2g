/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Display (VDISP).                                  *
 *             Definitions for server VDISP entities.                        *
 *                                                                           *
 *  This file provides definitions that are shared by the server library     *
 *  and the kernel back-end driver.                                          *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *    Chi Dat Truong    <chidat.truong@redbend.com>                          *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VDISP_SERVER_DRV_H
#define _VLX_VDISP_SERVER_DRV_H

#include <vlx/vdisplay/common.h>

/*
 * VDISP client/server request commands.
 *
 * These commands correspond to the primitives of the client VDISP library.
 */
typedef nku32_f VdispCmd;

/* RPC commands for user-level daemon */
#define VDISP_CMD_SET_BRIGHTNESS        1
#define VDISP_CMD_GET_BRIGHTNESS        2
#define VDISP_CMD_GET_INFO              3
#define VDISP_CMD_GET_POWER             4
#define VDISP_CMD_YYYY                  5
#define VDISP_CMD_NR                    5


#define VDISP_SRV_IOCTL(c, s)			\
    VDISP_IOCTL((c) + VDISP_IOCTL_CLT_CMD_MAX, s)

/* RPC interface User IOCTL <=> BE DRV */
#define VDISP_IOCTL_RECEIVE                     VDISP_SRV_IOCTL(0, 0)
#define VDISP_IOCTL_RESPOND                     VDISP_SRV_IOCTL(1, 0)

/*
 * Control IOCTL Interface
 */
#define VDISP_CONTROL_IOCTL(c, s)		_IOC(_IOC_NONE,'C',c,s)
#define VDISP_CONTROL_IOCTL_CMD_MAX		8

#define VDISP_C_IOCTL_ENABLE_EVENTS		VDISP_CONTROL_IOCTL(0, sizeof(nku32_f))
#define VDISP_C_IOCTL_LISTEN_EVENTS		VDISP_CONTROL_IOCTL(1, sizeof(nku32_f))
#define VDISP_C_IOCTL_GET_FOCUSED_CLIENT	VDISP_CONTROL_IOCTL(2, sizeof(nku32_f))
#define VDISP_C_IOCTL_GET_CLIENT_STATE		VDISP_CONTROL_IOCTL(3, sizeof(nku32_f))
#define VDISP_C_IOCTL_SET_DISPLAY_PROPERTY	VDISP_CONTROL_IOCTL(4, sizeof(DisplayProperty))

/*
 * An event is defined as follow:
 *   32 bits = [24 bits + 8 bits] = [event_type + event_value]
 *    => 24 event types in bitmask
 *    => 0-255 value in 8 bits.
 *
 * Currently we support 3 types of event:
 *  - FOCUS     providing the 'focus vm id' value.
 *  - PANEL_ON  providing the 'frontend vm id' value.
 *  - PANEL_OFF providing the 'frontend vm id' value.
 */

#define VDISP_EVENT_TYPE_FOCUS       (1 << 8)
#define VDISP_EVENT_TYPE_PANEL_ON    (1 << 9)
#define VDISP_EVENT_TYPE_PANEL_OFF   (1 << 10)

#define VDISP_EVENT_VALUE_MASK       0xff
#define VDISP_EVENT_FIFO_SIZE        8 // max 8 events

#define VDISP_EVENT(type, value)     ((nku32_f)((type)|(value&0xff)))

/*
 * VDISP client/server request header.
 */
typedef struct VdispReqHeader {
    VdispClientId       clId; /* Client ID of the session */
    VdispCmd            cmd;  /* Command */
} VdispReqHeader;

/*
 * VDISP client/server request.
 */
typedef struct VdispReq {
    VdispReqHeader           header; /* Request header */
    union {
        BacklightBrightness  screenBrightness; 
   }; /* Input parameters */
} VdispReq;

/*
 * VDISP client/server response.
 */
typedef struct VdispResp {
    unsigned int             retVal;
    union {
        BacklightBrightness  screenBrightness; 
        PanelPowerState      screenPowerState;
    };
} VdispResp;

#endif /* _VLX_VDISP_SERVER_DRV_H */

/*
 ****************************************************************
 *
 *  Component: VLX virtual Real Time Clock Interface
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
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

#ifndef VRTC_COMMON_H
#define VRTC_COMMON_H

#define	VRTC_VRPC_NAME	"vrtc"

typedef struct {
    nku32_f	tm_sec;
    nku32_f	tm_min;
    nku32_f	tm_hour;
    nku32_f	tm_mday;
    nku32_f	tm_mon;
    nku32_f	tm_year;
    nku32_f	tm_wday;
    nku32_f	tm_yday;
    nku32_f	tm_isdst;
} vrtc_time_t;

typedef struct {
    nku16_f	enabled;	/* Alarm enabled (1) or disabled (0) */
    nku16_f	pending;	/* Alarm pending (1) or not not pending (0) */
    vrtc_time_t	time;		/* time the alarm is set to */
} vrtc_wkalrm_t;

    /* VRTC RPC request */
typedef struct {
    nku32_f vcmd;	/* vrtc_cmd_t */
    nku32_f arg;	/* argument (optional) */
} vrtc_req_t;

typedef struct {
    vrtc_req_t	common;
    vrtc_time_t	time;
} vrtc_req_time_t;

typedef struct {
    vrtc_req_t		common;
    vrtc_wkalrm_t	alarm;
} vrtc_req_wkalrm_t;

    /* VRTC RPC result */
typedef struct {
    nku32_f res;	/* result */
    nku32_f value;	/* value */
} vrtc_res_t;

typedef struct {
    vrtc_res_t	common;
    vrtc_time_t	time;
} vrtc_res_time_t;

typedef struct {
    vrtc_res_t		common;
    vrtc_wkalrm_t	alarm;
} vrtc_res_wkalrm_t;

typedef union {
    vrtc_req_t		req;
    vrtc_req_time_t	req_time;
    vrtc_req_wkalrm_t	req_wkalrm;
    vrtc_res_t		res;
    vrtc_res_time_t	res_time;
    vrtc_res_wkalrm_t	res_wkalrm;
} vrtc_ipc_t;

typedef enum {
    VRTC_CMD_OPEN,
    VRTC_CMD_RELEASE,
    VRTC_CMD_IOCTL_RTC_AIE_ON,	/* Alarm Interrupt On */
    VRTC_CMD_IOCTL_RTC_AIE_OFF,	/* Alarm Interrupt Off */
    VRTC_CMD_IOCTL_RTC_UIE_ON,	/* Update Interrupt On */
    VRTC_CMD_IOCTL_RTC_UIE_OFF,	/* Update Interrupt Off */
    VRTC_CMD_IOCTL_RTC_PIE_ON,	/* Periodic Interrupt On */
    VRTC_CMD_IOCTL_RTC_PIE_OFF,	/* Periodic Interrupt Off */
    VRTC_CMD_IOCTL_RTC_WIE_ON,	/* Watchdog Interrupt On */
    VRTC_CMD_IOCTL_RTC_WIE_OFF,	/* Watchdog Interrupt Off */
    VRTC_CMD_READ_TIME,
    VRTC_CMD_SET_TIME,
    VRTC_CMD_READ_ALARM,
    VRTC_CMD_SET_ALARM,
    VRTC_CMD_IRQ_SET_STATE,
    VRTC_CMD_IRQ_SET_FREQ,
    VRTC_CMD_MAX
} vrtc_cmd_t;

#define VRTC_CMD_NAME \
    {"open",       "release",   "aie_on",        "aie_off", \
     "uie_on",     "uie_off",   "pie_on",        "pie_off", \
     "wie_on",     "wie_off",   "read_time",     "set_time", \
     "read_alarm", "set_alarm", "irq_set_state", "irq_set_freq"}

    /* Interrupt flags */
#define VRTC_EVENT_UF		0x1	/* Update */
#define VRTC_EVENT_AF		0x2	/* Alarm */
#define VRTC_EVENT_PF		0x4	/* Periodic */

typedef struct {
    volatile nku32_f	events;
} vrtc_shared_t;

#endif /* VRTC_COMMON_H */

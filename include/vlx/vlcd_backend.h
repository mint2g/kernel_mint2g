/*
 ****************************************************************
 *
 *  Component: VLX VLCD backend interface
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
 ****************************************************************
 */

#ifndef _VLCD_BACKEND_H_
#define _VLCD_BACKEND_H_

#include <linux/platform_device.h>
#include <vlx/vlcd_common.h>

/*
 * Frontend states
 */

/* EVT_INIT has not been received. */
#define VLCD_FSTATE_UNINITIALIZED 0
/* EVT_INIT has been received. */
#define VLCD_FSTATE_INITIALIZED   1


/*
 * Backend states
 */

/* Backend initialization not done. */
#define VLCD_BSTATE_UNINITIALIZED   0
/* Backend driver initialization called, native driver init not done. */
#define VLCD_BSTATE_INITIALIZING    1
/* Backend initialization done. */
#define VLCD_BSTATE_INITIALIZED     2 

#define VLCD_BMAX_HW_DEV_SUP        2


/*
 * Backend event set when receiving SYSCONF interrupts
 */

/* Frontend has been disconnected. */
#define VLCD_BEVENT_FRESET    0x00000001
/* Frontend has been started. */
#define VLCD_BEVENT_FSTARTED  0x00000002

typedef struct _vlcd_frontend_device {
    /* Part shared with frontend. */
    NkDevVlcd* common;
    /* Status of the frontend device. */
    nku32_f state;
    /* Cross irq id (to be able to detach irq handler). */
    NkXIrqId xid;
    /* Vlink used by this frontend. */
    NkDevVlink* vlink;
    /* Handshake events to be treated in irq threads. */
    nku32_f event;
    /* Tell if this frontend get focus on the screen. */
    nku8_f get_focus;
    /* Backend->frontend device cross-irq */
    NkXIrq sxirq;
    /* Synchronization mode */
    unsigned int sync;
    /* Next frontend device in list. */
    struct _vlcd_frontend_device* next_frontend_device;

    /*
     * VOGL extension
     */

    /* Description of the frontend framebuffer. */
    struct fb_info* fbInfo;
    /* 
     * Boolean that indicates if the frontend framebuffer is registered or not.
     */
    int fbRegistered;
} vlcd_frontend_device_t;

typedef struct _vlcd_hwOps vlcd_hw_ops_t;

typedef struct
{
    /* List of frontend devices. */
    vlcd_frontend_device_t* frontend_device_list;
    /* Os id of the frontend that get the screen focus. */
    nku32_f screen_owner[VLCD_BMAX_HW_DEV_SUP];
    /* Backend state. */
    nku32_f state[VLCD_BMAX_HW_DEV_SUP];
    /* Data used by callbacks in native driver. */
    void* hw_data[VLCD_BMAX_HW_DEV_SUP];
    /* Function to be called to send commands to native driver. */
    vlcd_hw_ops_t* hw_ops[VLCD_BMAX_HW_DEV_SUP];
    /* Complete when vlcd_be thread died. */
    struct completion xirq_completion;
    /* Semaphore used to treat irq events in vlcd_be thread. */
    struct semaphore xirq_semaphore;
    /* Reset to 0 when we want to kill vlcd_be thread. */
    int xirq_active;
    /* SYSCONF irq id (to be able to detach handler). */
    NkXIrqId sysconf_xid;
} vlcd_b_driver_t;

struct _vlcd_hwOps
{
    /* Get possible configurations supported by hw driver. */
    void (*get_possible_config)(vlcd_b_driver_t* vlcd_drv,
           nku8_f hw_device_id,
           vlcd_pconf_t* possible_config);
    /* Set hw driver screen size and offset. */
    void (*set_size)(vlcd_b_driver_t* vlcd_drv, nku8_f hw_device_id,
           vlcd_conf_t* config);
    /* Set hw driver screen color configuration. */
    void (*set_color)(vlcd_b_driver_t* vlcd_drv, nku8_f hw_device_id,
           vlcd_conf_t* config);
    /* Turn screen on and set dma zone. */
    void (*set_dma_zone)(vlcd_b_driver_t* vlcd_drv, nku8_f hw_device_id,
           nku32_f dma_size, nku32_f dma_paddr);
    /* Turn screen off. */
    void (*switch_off_screen)(vlcd_b_driver_t* vlcd_drv, nku8_f hw_device_id);
};

/*
 * Called by the native driver to retrieve driver specific information
 */
void* vlcd_b_get_hw_data (nku8_f hw_device_id);

/*
 * Called by native driver when it is initialized.
 */
int vlcd_b_hw_initialized(void* hw_data, vlcd_hw_ops_t* hw_ops, nku8_f hw_device_id);

/*
 * Give focus to frontend_os_id.
 */
void vlcd_b_switch_screen(nku8_f frontend_os_id, nku8_f hw_device_id);

#endif /* _VLCD_BACKEND_H_ */

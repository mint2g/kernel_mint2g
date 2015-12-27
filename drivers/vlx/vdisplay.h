/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Display (VDISP).                                  *
 *             Kernel-specific VDISP front-end/back-end drivers definitions. *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  This program is free software: you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License Version 2           *
 *  as published by the Free Software Foundation.                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  Version 2 along with this program.                                       *
 *  If not, see <http://www.gnu.org/licenses/>.                              *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *    Chi Dat Truong    <chidat.truong@redbend.com>                          *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VDISP_H
#define _VLX_VDISK_H

#define VDISP_DEBUG
#ifdef VDISP_DEBUG
#define VLINK_DEBUG
#endif
#include "vdisplay-proto.h"
#include "vlink-lib.h"
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/bitops.h>
#include <asm/atomic.h>
#include <linux/kfifo.h>        // kfifo for events management
#include <linux/list.h>         // list for events clients management
#include <linux/fb.h>           // linux framebuffer
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define VDISP_PRINTK(ll, m...)                  \
    printk(ll "vdisp: " m)

#define VDISP_ERROR(m...)                       \
    VDISP_PRINTK(KERN_ERR, m)

#define VDISP_WARN(m...)                        \
    VDISP_PRINTK(KERN_WARNING, m)

#ifdef VDISP_DEBUG
#define VDISP_DTRACE(m...)                      \
    VDISP_PRINTK(KERN_DEBUG, m)
#else
#define VDISP_DTRACE(m...)
#endif

#define VDISP_FUNC_ENTER()                      \
    VDISP_DTRACE(">>> %s\n", __FUNCTION__)

#define VDISP_FUNC_LEAVE()                      \
    VDISP_DTRACE("<<< %s\n", __FUNCTION__)

union VDispDev;
union VDispDrv;

#define VDISP_SRV_MAJOR                 228
#define VDISP_CLT_MAJOR                 228

#define VDISP_PROP_PDEV_SIZE            0

typedef int (*VdispPropGet) (Vlink* vlink, unsigned int type, void* prop);

typedef struct VdispGenDrv {
    /* Public */
    const char*             name;
    unsigned int            resc_id_base;
    VdispPropGet            prop_get;
    /* Internal */
    VlinkDrv*               vlink_drv;
    VlinkOpDesc*            vops;
    union VdispDev*         devs;
} VdispGenDrv;

typedef struct VdispGenDev {
    /* In */
    union VdispDrv*         drv;
    Vlink*                  vlink;
    /* Out */
    unsigned int            enabled;
    NkPhSize                pdev_size;
    NkPhAddr                pdev_paddr;
    nku8_f*                 pdev_vaddr;
    NkXIrq                  cxirqs;
    NkXIrq                  sxirqs;
    unsigned int            xirq_id_nr;
    NkXIrqId                xirq_id[1];
} VdispGenDev;

/*
 *
 */

#define VDISP_RPC_TYPE_CLIENT           0
#define VDISP_RPC_TYPE_SERVER           1

typedef struct VdispRpc {
    unsigned int            type;
    VdispRpcShm*            shm;
    VdispSize               size_max;
    NkXIrq                  xirq;
    NkXIrq                  xirq_peer;
    NkOsId                  osid_peer;
    unsigned int            aborted;
    wait_queue_head_t       wait;
    struct semaphore        lock;
} VdispRpc;

extern int   vdisp_rpc_setup      (VdispGenDev*  dev,
                                   VdispRpc*     rpc,
                                   int           type);
extern void  vdisp_rpc_reset      (VdispRpc*     rpc);
extern void  vdisp_rpc_wakeup     (VdispRpc*     rpc);

extern void* vdisp_rpc_req_alloc  (VdispRpc*     rpc,
                                   VdispSize     size);
extern void* vdisp_rpc_call       (VdispRpc*     rpc,
                                   VlinkSession* vls,
                                   VdispSize*    psize);
extern void  vdisp_rpc_call_end   (VdispRpc*     rpc);

extern void* vdisp_rpc_receive    (VdispRpc*     rpc,
                                   VlinkSession* vls,
                                   VdispSize*    psize);
extern void* vdisp_rpc_resp_alloc (VdispRpc*     rpc,
                                   VdispSize     size);
extern void  vdisp_rpc_respond    (VdispRpc*     rpc,
                                   int           err);

/*
 *
 */

typedef struct VdispCltDrv {
    /* FE-BE */
    VdispGenDrv             gen_drv;
    /* FE <--> User interface */
    unsigned int            major;
    unsigned int            chrdev_major;
    struct class*           class;
    struct file_operations* fops;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend    early_suspend;
#endif
} VdispCltDrv;

typedef struct VdispCltDev {
    VdispGenDev             gen_dev;        /* Must be first */
    struct device*          class_dev;
    VdispRpc                rpc;
    DisplayProperty         property; 
    struct fb_info          fb;
} VdispCltDev;

typedef struct VdispCltSession {
    VdispCltDev*            dev;
    VlinkSession*           vls;
} VdispCltSession;

typedef struct VdispCltFile {
    VdispCltDev*            dev;
    VdispCltSession         session;
} VdispCltFile;

typedef int (*VdispCltIoctl) (struct VdispCltFile* vdisp_file,
                              void*                arg,
                              unsigned int         sz);

/*
 *
 */

typedef struct VdispSrvDrv {
    /* BE <--> FE   interface */
    VdispGenDrv             gen_drv;
    /* BE <--> User interface */
    unsigned int            major;
    unsigned int            chrdev_major;
    struct class*           class;
    struct file_operations* fops;
} VdispSrvDrv;

typedef struct VdispSrvDev {
    VdispGenDev             gen_dev;        /* Must be first */
    struct device*          class_dev;
    VdispRpc                rpc;
    /* Internal data mainly used for the restart of BE daemon */
    PanelPowerState         panel_state;      /* on/off */
    BacklightBrightness     backlight_state;  /* brightness level 0-255 */
} VdispSrvDev;

typedef struct VdispSrvSession {
    VdispSrvDev*            dev;
    VlinkSession*           vls;
    VdispCmd                cmd;
} VdispSrvSession;

typedef struct VdispSrvFile {
    VdispSrvDev*            dev;
    VdispSrvSession         session;
} VdispSrvFile;

typedef int (*VdispSrvIoctl) (struct VdispSrvFile* vdisp_file,
                              void*                arg,
                              unsigned int         sz);

/*
 * Control interface (BE <=> User)
 */

typedef struct VdispControlDrv {
    /* BE <--> User interface */
    const char*             name;
    unsigned int            major;
    unsigned int            chrdev_major;
    struct class*           class;
    struct device*          class_dev;
    struct file_operations* fops;
    /* Internal hardware data */
    DisplayProperty         hw_property;
    wait_queue_head_t       hw_property_wait;
} VdispControlDrv;


typedef struct VdispControlClient {
    struct list_head   list;
    nku32_f            enabled_event_mask; // 24 MSB
    /* userland identification */
    struct inode*      inode;
    struct file*       file;
    /* internal events fifo */
    spinlock_t         events_fifo_lock;
    struct kfifo       events_fifo;
    wait_queue_head_t  events_fifo_wait;
} VdispControlClient;

typedef int (*VdispControlIoctl) (VdispControlClient* client,
                                  void*               arg,
                                  unsigned int        sz);

/*
 *
 */

typedef union VdispDrv {
    VdispGenDrv gen;
    VdispCltDrv clt;
    VdispSrvDrv srv;
} VdispDrv;

typedef union VdispDev {
    VdispGenDev gen;
    VdispCltDev clt;
    VdispSrvDev srv;
} VdispDev;

extern void  vdisp_gen_drv_cleanup (VdispGenDrv* drv);
extern int   vdisp_gen_drv_init    (VdispGenDrv* drv);

extern int   vdisp_gen_dev_init    (VdispGenDev* dev);
extern void  vdisp_gen_dev_cleanup (VdispGenDev* dev);

extern int   vdisp_xirq_attach (VdispGenDev*  dev,
                                NkXIrq        xirq,
                                NkXIrqHandler hdl,
                                void*         cookie);

extern VdispDrv* vdisp_drv_find (unsigned int major);
extern VdispDev* vdisp_dev_find (unsigned int major,
                                 unsigned int minor);

#endif /* _VLX_VDISP_H */

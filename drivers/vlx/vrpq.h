/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Remote Procedure Queue (VRPQ).                    *
 *             Kernel-specific VRPQ front-end/back-end drivers definitions.  *
 *                                                                           *
 *  This file provides definitions that enable to implement the VRPQ         *
 *  front-end and back-end drivers in the Linux kernel.                      *
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
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VRPQ_H
#define _VLX_VRPQ_H

//#define VRPQ_DEBUG
#ifdef VRPQ_DEBUG
#define VLINK_DEBUG
#endif

#include "vrpq-proto.h"
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

#define VRPQ_PRINTK(ll, m...)			\
    printk(ll "vrpq: " m)

#define VRPQ_ERROR(m...)			\
    VRPQ_PRINTK(KERN_ERR, m)

#define VRPQ_WARN(m...)				\
    VRPQ_PRINTK(KERN_WARNING, m)

#ifdef VRPQ_DEBUG
#define VRPQ_DTRACE(m...)			\
    VRPQ_PRINTK(KERN_DEBUG, m)
#else
#define VRPQ_DTRACE(m...)
#endif

#define VRPQ_FUNC_ENTER()			\
    VRPQ_DTRACE(">>> %s\n", __FUNCTION__)

#define VRPQ_FUNC_LEAVE()			\
    VRPQ_DTRACE("<<< %s\n", __FUNCTION__)

union VrpqDev;

#define VRPQ_PROP_PMEM_SIZE	0
#define VRPQ_PROP_RING_REQS_MAX	1

typedef int (*VrpqPropGet) (Vlink* vlink, unsigned int type, void* prop);

typedef struct VrpqDrv {
    /* Public */
    const char*             name;
    unsigned int            major;
    unsigned int            resc_id_base;
    VrpqPropGet             prop_get;
    /* Internal */
    VlinkDrv*               parent_drv;
    VlinkOpDesc*            vops;
    struct file_operations* fops;
    unsigned int            chrdev_major;
    struct class*           class;
    union VrpqDev*          devs;
    struct list_head        link;
} VrpqDrv;

typedef struct VrpqGenDev {
    /* In */
    VrpqDrv*                vrpq_drv;
    Vlink*                  vlink;
    /* Out */
    unsigned int            enabled;
    NkPhAddr                pmem_paddr;
    VrpqPmemLayout          pmem_layout;
    NkXIrq                  cxirqs;
    NkXIrq                  sxirqs;
    unsigned int            xirq_id_nr;
    NkXIrqId                xirq_id[2];
    struct device*          class_dev;
} VrpqGenDev;

#define VRPQ_FILE_NONE			0
#define VRPQ_FILE_SESSION		1
#define VRPQ_FILE_CHAN			2

typedef struct VrpqPmemMap {
    NkPhAddr                paddr;
    VrpqSize                size;
    void*                   ubase;
    void*                   kbase;
    unsigned long           k2u_offset;
} VrpqPmemMap;

#define VRPQ_PMEM_K2O(kaddr, kbase)				\
    ((VrpqSize)(((nku8_f*)(kaddr)) - ((nku8_f*)(kbase))))

#define VRPQ_PMEM_O2K(offset, kbase)				\
    ((void*)(((nku8_f*)(kbase)) + (offset)))

#define VRPQ_PMEM_K2U(kaddr, k2u_off)				\
    ((void*)(((nku8_f*)(kaddr)) + (k2u_off)))

#define VRPQ_PMEM_U2K(uaddr, k2u_off)				\
    ((void*)(((nku8_f*)(uaddr)) - (k2u_off)))

/*
 *
 */

#ifdef CONFIG_VRPQ_PROFILE

struct VrpqProcStatTbl;

typedef struct VrpqProcStatEnt {
    union {
	struct VrpqProcStatTbl* tbl;
	unsigned int            count;
    };
} VrpqProcStatEnt;

typedef struct VrpqProcStatTbl {
    VrpqProcStatEnt         ent[256];
} VrpqProcStatTbl;

typedef struct VrpqCltStats {
    union {
	struct {
	    atomic_t        adm_calls;
	    atomic_t        session_create;
	    atomic_t        session_destroy;
	    atomic_t        chan_create;
	    atomic_t        chan_destroy;
	    atomic_t        notify;
	    atomic_t        post_multi_call;
	    atomic_t        posts_per_call;
	    atomic_t        posts_per_call_max;
	    atomic_t        post_multi_notify;
	    atomic_t        posts_per_notify;
	    atomic_t        posts_per_notify_max;
	    atomic_t        post_multi;
	    atomic_t        grouped_posts;
	    atomic_t        grouped_posts_max;
	    atomic_t        resps_intr;
	    atomic_t        param_alloc_fast;
	    atomic_t        param_alloc_slow;
	    atomic_t        in_param_ref;
	    atomic_t        param_single_sz_max;
	    atomic_t        param_alloc_sz;
	    atomic_t        param_alloc_sz_max;
	    atomic_t        reqs_nr;
	    atomic_t        reqs_nr_max;
	    atomic_t        reqs_free;
	    atomic_t        reqs_per_free;
	    atomic_t        req_ring_full;
	    atomic_t        param_ring_full;
	    atomic_t        wait_for_space_intr;
	    atomic_t        proc_stat_tbl;
	};
	atomic_t            counts[30];
    };
    VrpqProcStatTbl*        proc_stat_dir;
    unsigned int            proc_stat_version;
    spinlock_t              proc_stat_lock;
} VrpqCltStats;

#else  /* CONFIG_VRPQ_PROFILE */

typedef struct VrpqCltStats {
    unsigned int            dummy;
} VrpqCltStats;

#endif /* CONFIG_VRPQ_PROFILE */

typedef struct VrpqReqAlloc {
    volatile atomic_t       free_excl;
    volatile unsigned int   free_needed;
    VrpqReqRing             ring;
    spinlock_t 	            lock;
} VrpqReqAlloc;

typedef struct VrpqParamAlloc {
    VrpqParamRing           ring;
    spinlock_t	            lock;
} VrpqParamAlloc;

typedef struct VrpqCltCall {
    unsigned int            idx;
    unsigned int            cancel;
    VrpqMsgId               req_id;
    VrpqMsgId               rsp_id;
    VrpqReqMsg*             req;
    VrpqSize                out_offset;
    unsigned int            diag;
    wait_queue_head_t       wait;
    wait_queue_head_t*      cancel_pwait;
    struct VrpqCltCall*     next;
} VrpqCltCall;

typedef struct VrpqCltCallAlloc {
    unsigned int            nr;
    VrpqCltCall**           calls_dir;
    VrpqCltCall*            free;
    VrpqCltCall*            cancel;
    spinlock_t 	            lock;
    volatile unsigned int   full_flag;
    wait_queue_head_t       full_wait;
    void*                   cancel_thread;
    wait_queue_head_t       cancel_wait;
} VrpqCltCallAlloc;

typedef struct VrpqCltPeer {
    NkXIrq                  xirq_req;
    NkXIrq                  xirq_cancel;
    NkOsId                  osid;
} VrpqCltPeer;

struct VrpqqCltChan;

typedef struct VrpqCltDev {
    VrpqGenDev              gen_dev;	/* Must be first */
    VrpqCltPeer             peer;
    VrpqReqAlloc            req_alloc;
    VrpqRspRing             rsp_ring;
    VrpqCltCallAlloc        call_alloc;
    VrpqParamAlloc          usr_param_alloc;
    VrpqParamAlloc          adm_param_alloc;
    struct semaphore        adm_sem;
    volatile unsigned int   full_flag;
    wait_queue_head_t       full_wait;
    VrpqCltStats            stats;
} VrpqCltDev;

struct VrpqCltSession;

typedef struct VrpqCltChan {
    VrpqChanId              id;
    VrpqCltDev*             dev;
    struct VrpqCltSession*  session;
    VlinkSession*           vls;
} VrpqCltChan;

typedef struct VrpqCltSession {
    VrpqSessionId           id;
    VrpqCltDev*             dev;
    atomic_t                refcount;
    VlinkSession*           vls;
    VrpqPmemMap             pmem_map;
} VrpqCltSession;

struct VrpqCltFile;

typedef int (*VrpqCltIoctl) (struct VrpqCltFile*, void* arg, unsigned int sz);

typedef struct VrpqCltFile {
    unsigned int            type;
    VrpqCltDev*             dev;
    struct semaphore        lock;
    union {
	VrpqCltChan         chan;
	VrpqCltSession      session;
    };
} VrpqCltFile;

#define VRPQ_CLT_CHAN_TO_FILE(c)		\
    container_of(c, VrpqCltFile, chan)

#define VRPQ_CLT_SESSION_TO_FILE(s)		\
    container_of(s, VrpqCltFile, session)

extern int  vrpq_clt_drv_init    (VlinkDrv* parent_drv,
				  VrpqDrv*  vrpq_drv);
extern void vrpq_clt_drv_cleanup (VrpqDrv*  vrpq_drv);

extern int  vrpq_clt_vlink_init  (VrpqDrv*  vrpq_drv,
				  Vlink*    vlink);

/*
 *
 */

#ifdef CONFIG_VRPQ_PROFILE

typedef struct VrpqSrvStats {
    atomic_t                dummy;
} VrpqSrvStats;

#else  /* CONFIG_VRPQ_PROFILE */

typedef struct VrpqSrvStats {
    unsigned int            dummy;
} VrpqSrvStats;

#endif /* CONFIG_VRPQ_PROFILE */

#define VRPQ_CHAN_REQ_RING_IS_FULL(h, t)			\
    (((h) - (t)) >= VRPQ_CHAN_REQS_MASK)

#define VRPQ_CHAN_REQ_RING_IS_EMPTY(h, t)			\
    ((t) == (h))

#define VRPQ_ADM_REQS_WAIT		(1 << 0)
#define VRPQ_ADM_REQS_VALID		(1 << 1)
#define VRPQ_ADM_PENDING_REQS_MAX	(VRPQ_SESSION_MAX + VRPQ_CHAN_MAX)

typedef struct VrpqRspAlloc {
    VrpqRspRing             ring;
    spinlock_t 	            lock;
} VrpqRspAlloc;

typedef struct VrpqChanReqRing {
    volatile VrpqRingIdx    head_idx;
    volatile VrpqRingIdx    tail_idx;
    volatile VrpqRingIdx    rcv_tail_idx;
    VrpqReqMsg*             reqs[VRPQ_CHAN_REQS_MAX];
    volatile unsigned int   wait_flag;
    wait_queue_head_t       wait;
} VrpqChanReqRing;

typedef struct VrpqBitmapAlloc {
    nku32_f                 l1;
    nku32_f                 l2[32];
    spinlock_t 	            lock;
} VrpqBitmapAlloc;

typedef struct VrpqSrvPeer {
    NkXIrq                  xirq_rsp;
    NkXIrq                  xirq_full;
    NkOsId                  osid;
} VrpqSrvPeer;

struct VrpqqSrvChan;
struct VrpqSrvSession;

typedef struct VrpqSrvDev {
    VrpqGenDev              gen_dev;	/* Must be first */
    VrpqSrvPeer             peer;
    VrpqReqRing             req_ring;
    VrpqRspAlloc            rsp_alloc;
    void*                   adm_thread;
    struct list_head        adm_reqs;
    atomic_t                adm_reqs_nr;
    spinlock_t	            adm_reqs_lock;
    struct semaphore        adm_lock;
    volatile atomic_t       dispatch_excl;
    volatile unsigned int   dispatch_needed;
    volatile unsigned int   cancel_flag;
    VrpqBitmapAlloc*        sessions_alloc;
    struct VrpqSrvSession** sessions;
    VrpqBitmapAlloc*        chans_alloc;
    struct VrpqSrvChan**    chans;
    VrpqSrvStats            stats;
} VrpqSrvDev;

typedef union {
    nku32_f                 all;
    struct {
	nku8_f              clt;
	nku8_f              srv;
    };
} VrpqCloseFlags;

typedef struct VrpqSrvChan {
    VrpqChanId              id;
    VrpqSrvDev*             dev;
    atomic_t                refcount;
    VrpqCloseFlags          close;
    struct VrpqSrvSession*  session;
    VlinkSession*           vls;
    VrpqChanReqRing         chan_req_ring;
    int                     call_reason;
    volatile atomic_t       recv_excl;
} VrpqSrvChan;

typedef struct VrpqSrvAdmReq {
    VrpqReqMsg*             req;
    VlinkSession*           vls;
    struct list_head        link_session;
    struct list_head        link_all;
} VrpqSrvAdmReq;

typedef struct VrpqSrvSessionAdm {
    struct list_head        reqs;
    wait_queue_head_t       wait;
} VrpqSrvSessionAdm;

typedef struct VrpqSrvSession {
    VrpqSessionId           id;
    VrpqSrvDev*             dev;
    atomic_t                refcount;
    VrpqCloseFlags          close;
    AppId                   app_id;
    VlinkSession*           vls;
    VrpqSrvSessionAdm       adm;
    VrpqPmemMap             pmem_map;
} VrpqSrvSession;

struct VrpqSrvFile;

typedef int (*VrpqSrvIoctl) (struct VrpqSrvFile*, void* arg, unsigned int sz);

typedef struct VrpqSrvFile {
    unsigned int            type;
    struct file*            filp;
    VrpqSrvDev*             dev;
    struct semaphore        lock;
    union {
	VrpqSrvChan         chan;
	VrpqSrvSession      session;
    };
} VrpqSrvFile;

#define VRPQ_SRV_CHAN_TO_FILE(c)		\
    container_of(c, VrpqSrvFile, chan)

#define VRPQ_SRV_SESSION_TO_FILE(s)		\
    container_of(s, VrpqSrvFile, session)

extern int  vrpq_srv_drv_init    (VlinkDrv* parent_drv,
				  VrpqDrv*  vrpq_drv);
extern void vrpq_srv_drv_cleanup (VrpqDrv*  vrpq_drv);

extern int  vrpq_srv_vlink_init  (VrpqDrv*  vrpq_drv,
				  Vlink*    vlink);

/*
 *
 */

#ifdef CONFIG_VRPQ_PROFILE

#define VRPQ_PROF_INIT(dev)					\
    do {							\
	vrpq_prof_init(dev);					\
    } while (0)

#define VRPQ_PROF_SET(dev, var, val)				\
    ({								\
	(dev)->stats.var.counter = val;				\
    })

#define VRPQ_PROF_ATOMIC_SET(dev, var, val)			\
    ({								\
	atomic_set(&(dev)->stats.var, val);			\
    })

#define VRPQ_PROF_READ(dev, var)				\
    ({								\
	(dev)->stats.var.counter;				\
    })

#define VRPQ_PROF_ATOMIC_READ(dev, var)				\
    ({								\
	atomic_read(&(dev)->stats.var);				\
    })

#define VRPQ_PROF_INC(dev, var)					\
    do {							\
	(dev)->stats.var.counter++;				\
    } while (0)

#define VRPQ_PROF_ATOMIC_INC(dev, var)				\
    do {							\
	atomic_inc(&(dev)->stats.var);				\
    } while (0)

#define VRPQ_PROF_DEC(dev, var)					\
    do {							\
	(dev)->stats.var.counter--;				\
    } while (0)

#define VRPQ_PROF_ATOMIC_DEC(dev, var)				\
    do {							\
	atomic_dec(&(dev)->stats.var);				\
    } while (0)

#define VRPQ_PROF_ADD(dev, var, c)				\
    do {							\
	(dev)->stats.var.counter += (c);			\
    } while (0)

#define VRPQ_PROF_ATOMIC_ADD(dev, var, c)			\
    do {							\
	atomic_add(c, &(dev)->stats.var);			\
    } while (0)

#define VRPQ_PROF_SUB(dev, var, c)				\
    do {							\
	(dev)->stats.var.counter -= (c);			\
    } while (0)

#define VRPQ_PROF_ATOMIC_SUB(dev, var, c)			\
    do {							\
	atomic_sub(c, &(dev)->stats.var);			\
    } while (0)

#define VRPQ_PROF_MAX(dev, var, v)				\
    do {							\
	if ((v) > (dev)->stats.var.counter) {			\
	    (dev)->stats.var.counter = (v);			\
	}							\
    } while (0)

#define VRPQ_PROF_ATOMIC_MAX(dev, var, v)			\
    do {							\
	atomic_t* _pvar = &(dev)->stats.var;			\
	int       _old = atomic_read(_pvar);			\
	while ((v) > _old) {					\
	    _old = atomic_cmpxchg(_pvar, _old, v);		\
	}							\
    } while (0)

#else  /* CONFIG_VRPQ_PROFILE */

#define VRPQ_PROF_INIT(dev)
#define VRPQ_PROF_SET(dev, var, val)
#define VRPQ_PROF_ATOMIC_SET(dev, var, val)
#define VRPQ_PROF_READ(dev, var)
#define VRPQ_PROF_ATOMIC_READ(dev, var)
#define VRPQ_PROF_INC(dev, var)
#define VRPQ_PROF_ATOMIC_INC(dev, var)
#define VRPQ_PROF_DEC(dev, var)
#define VRPQ_PROF_ATOMIC_DEC(dev, var)
#define VRPQ_PROF_ADD(dev, var, c)
#define VRPQ_PROF_ATOMIC_ADD(dev, var, c)
#define VRPQ_PROF_SUB(dev, var, c)
#define VRPQ_PROF_ATOMIC_SUB(dev, var, c)
#define VRPQ_PROF_MAX(dev, var, v)
#define VRPQ_PROF_ATOMIC_MAX(dev, var, v)

#endif /* CONFIG_VRPQ_PROFILE */

#define COPYIN(dst, usrc, sz)						\
    ({									\
	int _r;								\
	if (access_ok(VERIFY_READ, usrc, sz) &&				\
	    (__copy_from_user(dst, usrc, sz) == 0))			\
	    _r = 0;							\
	else _r = 1;							\
	(_r);								\
    })

#define COPYOUT(udst, src, sz)						\
    ({									\
	int _r;								\
	if (access_ok(VERIFY_WRITE, udst, sz) &&			\
	    (__copy_to_user(udst, src, sz) == 0))			\
	    _r = 0;							\
	else _r = 1;							\
	(_r);								\
    })

typedef union VrpqDev {
    VrpqGenDev gen;
    VrpqCltDev clt;
    VrpqSrvDev srv;
} VrpqDev;

extern void  vrpq_gen_drv_cleanup (VrpqDrv* vrpq_drv);
extern int   vrpq_gen_drv_init    (VrpqDrv* vrpq_drv);

extern int   vrpq_gen_dev_init    (VrpqGenDev* dev);
extern void  vrpq_gen_dev_cleanup (VrpqGenDev* dev);

extern void* vrpq_thread_create (VrpqGenDev* dev,
				 int       (*f) (void*),
				 void*       cookie);
extern void  vrpq_thread_delete (void**      thread);

extern int   vrpq_xirq_attach (VrpqGenDev*   dev,
			       NkXIrq        xirq,
			       NkXIrqHandler hdl);

extern VrpqDrv* vrpq_drv_find (unsigned int major);
extern VrpqDev* vrpq_dev_find (unsigned int major,
			       unsigned int minor);

#endif /* _VLX_VRPQ_H */

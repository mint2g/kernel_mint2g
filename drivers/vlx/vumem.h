/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual User MEMory Buffers (VUMEM).                      *
 *             Kernel-specific VUMEM front-end/back-end drivers definitions. *
 *                                                                           *
 *  This file provides definitions that enable to implement the VUMEM        *
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

#ifndef _VLX_VUMEM_H
#define _VLX_VUMEM_H

//#define VUMEM_DEBUG
#ifdef VUMEM_DEBUG
#define VLINK_DEBUG
#endif
#include "vumem-proto.h"
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

#define VUMEM_PRINTK(ll, m...)			\
    printk(ll "vumem: " m)

#define VUMEM_ERROR(m...)			\
    VUMEM_PRINTK(KERN_ERR, m)

#define VUMEM_WARN(m...)			\
    VUMEM_PRINTK(KERN_WARNING, m)

#ifdef VUMEM_DEBUG
#define VUMEM_DTRACE(m...)			\
    VUMEM_PRINTK(KERN_DEBUG, m)
#else
#define VUMEM_DTRACE(m...)
#endif

#define VUMEM_FUNC_ENTER()			\
    VUMEM_DTRACE(">>> %s\n", __FUNCTION__)

#define VUMEM_FUNC_LEAVE()			\
    VUMEM_DTRACE("<<< %s\n", __FUNCTION__)

union VumemDev;

#define VUMEM_PROP_PDEV_SIZE	0

typedef int (*VumemPropGet) (Vlink* vlink, unsigned int type, void* prop);

typedef struct VumemDrv {
    /* Public */
    const char*             name;
    unsigned int            major;
    unsigned int            resc_id_base;
    VumemPropGet            prop_get;
    /* Internal */
    VlinkDrv*               parent_drv;
    VlinkOpDesc*            vops;
    struct file_operations* fops;
    unsigned int            chrdev_major;
    struct class*           class;
    union VumemDev*         devs;
    struct list_head        link;
} VumemDrv;

typedef struct VumemGenDev {
    /* In */
    VumemDrv*               vumem_drv;
    Vlink*                  vlink;
    /* Out */
    unsigned int            enabled;
    NkPhSize                pdev_size;
    NkPhAddr                pdev_paddr;
    nku8_f*                 pdev_vaddr;
    NkXIrq                  cxirqs;
    NkXIrq                  sxirqs;
    unsigned int            xirq_id_nr;
    NkXIrqId                xirq_id[2];
    struct device*          class_dev;
} VumemGenDev;

/*
 *
 */

#define VUMEM_RPC_TYPE_CLIENT		0
#define VUMEM_RPC_TYPE_SERVER		1

#define VUMEM_RPC_CHAN_REQS		0
#define VUMEM_RPC_CHAN_ABORT		1

#define VUMEM_RPC_FLAGS_INTR		(1 << 0)

typedef struct VumemRpc {
    unsigned int            type;
    unsigned int            chan;
    VumemRpcShm*            shm;
    VumemSize               size_max;
    NkXIrq                  xirq;
    NkXIrq                  xirq_peer;
    NkOsId                  osid_peer;
    unsigned int            aborted;
    wait_queue_head_t       wait;
    struct semaphore        lock;
} VumemRpc;

typedef struct VumemRpcSession {
    VumemSessionId          id;
    VumemRpc*               rpc;
    VlinkSession*           vls;
} VumemRpcSession;

extern int   vumem_rpc_setup  (VumemGenDev*   dev,
			       VumemRpc*      rpc,
			       int            type,
			       int            dir);
extern void  vumem_rpc_reset  (VumemRpc*      rpc);
extern void  vumem_rpc_wakeup (VumemRpc*      rpc);

extern void  vumem_rpc_session_init (VumemRpcSession* rpc_session,
				     VumemRpc*        rpc,
				     VlinkSession*    vls);

extern void* vumem_rpc_req_alloc (VumemRpcSession* rpc_session,
				  VumemSize        size,
				  int              flags);
extern void* vumem_rpc_call      (VumemRpcSession* rpc_session,
				  VumemSize*       psize);
extern void  vumem_rpc_call_end  (VumemRpcSession* rpc_session);

extern void* vumem_rpc_receive     (VumemRpcSession* rpc_session,
				    VumemSize*       psize,
				    int              flags);
extern void* vumem_rpc_resp_alloc  (VumemRpcSession* rpc_session,
				    VumemSize        size);
extern void  vumem_rpc_respond     (VumemRpcSession* rpc_session,
				    int              err);
extern void  vumem_rpc_receive_end (VumemRpcSession* rpc_session);

/*
 *
 */

#ifdef CONFIG_VUMEM_PROFILE

typedef struct VumemCltStats {
    union {
	struct {
	    atomic_t        buffer_alloc;
	    atomic_t        buffer_free;
	    atomic_t        buffer_map;
	    atomic_t        buffer_register;
	    atomic_t        buffer_unregister;
	    atomic_t        buffer_cache_ctl;
	    atomic_t        buffer_cache_ctl_rem;
	    atomic_t        buffer_ctl_remote;
	    atomic_t        rpc_calls;
	    atomic_t        sessions;
	    atomic_t        buffers;
	    atomic_t        buffer_mappings;
	    atomic_t        buffer_size_max;
	    atomic_t        buffer_allocated;
	    atomic_t        buffer_allocated_max;
	};
	atomic_t            counts[15];
    };
} VumemCltStats;

#else  /* CONFIG_VUMEM_PROFILE */

typedef struct VumemCltStats {
    unsigned int            dummy;
} VumemCltStats;

#endif /* CONFIG_VUMEM_PROFILE */

struct VumemCltSession;
struct VumemCltBuffer;

typedef struct VumemXLink {
    void*                   item[2];
    struct list_head        link[2];
} VumemXLink; 

typedef struct VumemXListHead {
    struct list_head        head;
    spinlock_t*             lock;
    unsigned int            nr;
} VumemXListHead;

typedef struct VumemCltDev {
    VumemGenDev             gen_dev;	/* Must be first */
    VumemRpc                rpc_clt;
    VumemRpc                rpc_srv;
    spinlock_t 	            xlink_lock;
    spinlock_t 	            buffer_map_lock;
    struct list_head        buffer_maps;
    spinlock_t 	            buffer_lock;
    struct list_head        buffers;
    spinlock_t 	            session_lock;
    struct list_head        sessions;
    struct VumemCltSession* msession;
    void*                   thread;
    VumemCltStats           stats;
} VumemCltDev;

typedef struct VumemCltSession {
    VumemCltDev*            dev;
    VlinkSession*           vls;
    atomic_t                refcount;
    VumemRpcSession         rpc_session;
    unsigned int            aborted;
    unsigned int            master;
    AppId                   app_id;
    VumemXListHead          xlink_buffers;
    struct list_head        link;
} VumemCltSession;

typedef struct VumemCltBuffer {
    VumemBufferId           id;
    atomic_t                refcount;
    VumemBufferLayout*      layout;
    unsigned long           garbage_pfn;
    unsigned long           garbage_clean;
    VumemCltSession*        session;
    VumemXListHead          xlink_sessions;
    struct list_head        buffer_maps;
    struct list_head        link;
} VumemCltBuffer;

typedef struct VumemCltBufferMap {
    VumemCltBuffer*         buffer;
    struct vm_area_struct*  vma;
    unsigned int            revoked;
    struct list_head        link_buffer;
    struct list_head        link_all;
} VumemCltBufferMap;

struct VumemCltFile;

typedef int (*VumemCltIoctl) (struct VumemCltFile* vumem_file,
			      void*                arg,
			      unsigned int         sz);

#define VUMEM_FILE_NONE			0
#define VUMEM_FILE_SESSION		1
#define VUMEM_FILE_BUFFER		2

typedef struct VumemCltFile {
    unsigned int            type;
    struct file*            filp;
    VumemCltDev*            dev;
    struct semaphore        lock;
    union {
	VumemCltSession     session;
	VumemCltBuffer      buffer;
    };
} VumemCltFile;

#define VUMEM_BUFFER_TO_FILE(c)			\
    container_of(c, VumemCltFile, buffer)

#define VUMEM_SESSION_TO_FILE(s)		\
    container_of(s, VumemCltFile, session)

extern int  vumem_clt_drv_init    (VlinkDrv* parent_drv,
				   VumemDrv* vumem_drv);
extern void vumem_clt_drv_cleanup (VumemDrv* vumem_drv);

extern int  vumem_clt_vlink_init  (VumemDrv* vumem_drv,
				   Vlink*    vlink);

/*
 *
 */

typedef struct VumemSrvDev {
    VumemGenDev             gen_dev;	/* Must be first */
    VumemRpc                rpc_clt;
    VumemRpc                rpc_srv;
    VumemSessionId          session_id;
    volatile atomic_t       excl;
} VumemSrvDev;

typedef struct VumemSrvBufferId {
    VumemBufferId           id;
    struct list_head        link;
} VumemSrvBufferId;

typedef struct VumemSrvBuffer {
    VumemSrvBufferId*       sid;
    struct list_head        link;
    VumemBufferLayout       layout;
} VumemSrvBuffer;

typedef struct VumemSrvSession {
    VumemSrvDev*            dev;
    VlinkSession*           vls;
    VumemRpcSession         rpc_session;
    VumemCmd                rpc_cmd;
    VumemSrvBufferId*       buffer_sid;
    struct list_head        buffers;
    volatile atomic_t       excl;
} VumemSrvSession;

struct VumemSrvFile;

typedef int (*VumemSrvIoctl) (VumemSrvSession* session,
			      void*            arg,
			      unsigned int     sz);

typedef struct VumemSrvFile {
    VumemSrvDev*            dev;
    VumemSrvSession         session;
} VumemSrvFile;

extern int  vumem_srv_drv_init    (VlinkDrv* parent_drv,
				   VumemDrv* vumem_drv);
extern void vumem_srv_drv_cleanup (VumemDrv* vumem_drv);

extern int  vumem_srv_vlink_init  (VumemDrv* vumem_drv,
				   Vlink*    vlink);

/*
 *
 */

#ifdef CONFIG_VUMEM_PROFILE

#define VUMEM_PROF_INIT(dev)					\
    do {							\
	vumem_prof_init(dev);					\
    } while (0)

#define VUMEM_PROF_READ(dev, var)				\
    ({								\
	(dev)->stats.var.counter;				\
    })

#define VUMEM_PROF_ATOMIC_READ(dev, var)			\
    ({								\
	atomic_read(&(dev)->stats.var);				\
    })

#define VUMEM_PROF_INC(dev, var)				\
    do {							\
	(dev)->stats.var.counter++;				\
    } while (0)

#define VUMEM_PROF_ATOMIC_INC(dev, var)				\
    do {							\
	atomic_inc(&(dev)->stats.var);				\
    } while (0)

#define VUMEM_PROF_DEC(dev, var)				\
    do {							\
	(dev)->stats.var.counter--;				\
    } while (0)

#define VUMEM_PROF_ATOMIC_DEC(dev, var)				\
    do {							\
	atomic_dec(&(dev)->stats.var);				\
    } while (0)

#define VUMEM_PROF_ADD(dev, var, c)				\
    do {							\
	(dev)->stats.var.counter += (c);			\
    } while (0)

#define VUMEM_PROF_ATOMIC_ADD(dev, var, c)			\
    do {							\
	atomic_add(c, &(dev)->stats.var);			\
    } while (0)

#define VUMEM_PROF_SUB(dev, var, c)				\
    do {							\
	(dev)->stats.var.counter -= (c);			\
    } while (0)

#define VUMEM_PROF_ATOMIC_SUB(dev, var, c)			\
    do {							\
	atomic_sub(c, &(dev)->stats.var);			\
    } while (0)

#define VUMEM_PROF_MAX(dev, var, v)				\
    do {							\
	if ((v) > (dev)->stats.var.counter) {			\
	    (dev)->stats.var.counter = (v);			\
	}							\
    } while (0)

#define VUMEM_PROF_ATOMIC_MAX(dev, var, v)			\
    do {							\
	atomic_t* _pvar = &(dev)->stats.var;			\
	int       _old = atomic_read(_pvar);			\
	while ((v) > _old) {					\
	    _old = atomic_cmpxchg(_pvar, _old, v);		\
	}							\
    } while (0)

#else  /* CONFIG_VUMEM_PROFILE */

#define VUMEM_PROF_INIT(dev)
#define VUMEM_PROF_READ(dev, var)
#define VUMEM_PROF_ATOMIC_READ(dev, var)
#define VUMEM_PROF_INC(dev, var)
#define VUMEM_PROF_ATOMIC_INC(dev, var)
#define VUMEM_PROF_DEC(dev, var)
#define VUMEM_PROF_ATOMIC_DEC(dev, var)
#define VUMEM_PROF_ADD(dev, var, c)
#define VUMEM_PROF_ATOMIC_ADD(dev, var, c)
#define VUMEM_PROF_SUB(dev, var, c)
#define VUMEM_PROF_ATOMIC_SUB(dev, var, c)
#define VUMEM_PROF_MAX(dev, var, v)
#define VUMEM_PROF_ATOMIC_MAX(dev, var, v)

#endif /* CONFIG_VUMEM_PROFILE */

/*
 *
 */

typedef union VumemDev {
    VumemGenDev gen;
    VumemCltDev clt;
    VumemSrvDev srv;
} VumemDev;

extern void  vumem_gen_drv_cleanup (VumemDrv* vumem_drv);
extern int   vumem_gen_drv_init    (VumemDrv* vumem_drv);

extern int   vumem_gen_dev_init    (VumemGenDev* dev);
extern void  vumem_gen_dev_cleanup (VumemGenDev* dev);

extern void* vumem_thread_create (VumemGenDev* dev,
				  int        (*f) (void*),
				  void*        cookie);
extern void  vumem_thread_delete (void**       thread);

extern int   vumem_xirq_attach (VumemGenDev*  dev,
				NkXIrq        xirq,
				NkXIrqHandler hdl,
				void*         cookie);

extern VumemDrv* vumem_drv_find (unsigned int major);
extern VumemDev* vumem_dev_find (unsigned int major,
				 unsigned int minor);

/*
 *
 */

#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

extern unsigned int vumem_cache_attr_get (pgprot_t prot);

extern pgprot_t     vumem_cache_prot_get (pgprot_t     prot,
					  unsigned int cache_policy);

extern int          vumem_cache_op       (VumemCacheOp       cache_op,
					  VumemBufferLayout* layout, 
					  void*              start);

#endif /* _VLX_VUMEM_H */

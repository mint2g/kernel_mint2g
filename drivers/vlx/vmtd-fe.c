/*
 ****************************************************************
 *
 *  Component: VLX VMTD-FE
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

/*----- System header files -----*/

#include <linux/module.h>	/* __exit, __init */
#include <linux/mtd/mtd.h>	/* mtd_info */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,7)
    /* This include exists in 2.6.6 but functions are not yet exported */
#include <linux/kthread.h>
#endif
#include <linux/proc_fs.h>
#include <asm/io.h>		/* ioremap */
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
#include <linux/semaphore.h>
#endif
#include <linux/init.h>		/* module_init() in 2.6.0 and before */
#include <nk/nkern.h>

#ifdef CONFIG_X86
#include "vmtd.h"
#else
#include <vlx/vmtd.h>
#endif

/*----- Local configuration -----*/

#if 0
#define VMTD_DEBUG
#endif

#if 0
#undef VMTD_ODEBUG		/* Activates old/noisy debug traces */
#endif

    /*
     *  After how many failed attempts should the driver
     *  stop using the NK_DEV_MTD_FUNC_READ_MANY_OOBS call
     *  on a given MTD device.
     */
#define VMTD_MAX_READ_MANY_OOBS_FAILURES	16

/*----- Local header files -----*/

#include "vlx-vmq.h"
#include "vlx-vipc.h"

/*----- Tracing -----*/

#ifdef VMTD_DEBUG
#define DTRACE(x...)	do {printk ("(%d) %s: ", current->tgid, __func__);\
			    printk (x);} while (0)
#else
#define DTRACE(x...)
#endif

#ifdef VMTD_ODEBUG
#define OTRACE(x...)	DTRACE(x)
#else
#define OTRACE(x...)
#endif

#define TRACE(x...)	printk (KERN_NOTICE "VMTD-FE: " x)
#define WTRACE(x...)	printk (KERN_WARNING "VMTD-FE: " x)
#define ETRACE(x...)	printk (KERN_ERR "VMTD-FE: " x)

/*----- Locking -----*/

static DEFINE_SPINLOCK(vmtd_spinlock);
static unsigned long vmtd_spinlock_flags;

#define VMTD_LOCK() \
    do { \
	OTRACE ("lock\n"); \
	spin_lock_irqsave (&vmtd_spinlock, vmtd_spinlock_flags); \
    } while (0)

#define VMTD_UNLOCK() \
    do { \
	OTRACE ("unlock\n"); \
	spin_unlock_irqrestore (&vmtd_spinlock, vmtd_spinlock_flags); \
    } while (0)

/*----- Version compatibility functions -----*/

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,7)
    static inline void*
kzalloc (size_t size, unsigned flags)
{
    void* ptr = kmalloc (size, flags);
    if (ptr) {
	memset (ptr, 0, size);
    }
    return ptr;
}

    static inline void
mtd_erase_callback (struct erase_info* ei)
{
    if (ei->callback) ei->callback(ei);
}

#define kvec iovec

#endif

/*----- Data types: vmtd descriptor -----*/

#define VMTD_CACHE_IN(vmtd)	((vmtd)->cache_size-1)
#define VMTD_CACHE_OUT(vmtd)	~VMTD_CACHE_IN(vmtd)

typedef struct vmtd_vmtd_t {
    struct list_head	list;	/* There is no array */
    NkDevMtd		remote_mtd;
    NkDevMtdEraseRegionInfo*
			erase_region_info;
    struct mtd_info	local_mtd;
    _Bool		added;
    _Bool		deleted;
    _Bool		get_device_done;	/* from backend POV */
    _Bool		implicit_get_device;
    vmq_link_t*		link;
    vipc_ctx_t*		vipc_ctx;
    char*		data_area;	/* for link */

    char*		cache_data;
    nku32_f		cache_size;
    nku32_f		cache_offset;
    _Bool		cache_valid;

    nku32_f		isbad_badmask;
    nku32_f		isbad_index;	/* 32-member eraseblock group */
    unsigned		isbad_validbits;

    char		oob_data [512];
    nku32_f		oob_offset;	/* inside whole device */
    nku32_f		oob_oobmode;	/* NK_DEV_MTD_OOB_... */
    nku32_f		oob_ooboffset;	/* Inside oob block */
    nku32_f		oob_ooblen_requested;	/* Inside oob block */
    nku32_f		oob_ooblen_obtained;	/* Inside oob block */
    unsigned		oob_validoobs;
    unsigned		oob_read_many_failures;

    unsigned		local_calls  [NK_DEV_MTD_FUNC_MAX];
    unsigned		remote_calls [NK_DEV_MTD_FUNC_MAX];
} vmtd_vmtd_t;

#define VMTD(mtd)		((vmtd_vmtd_t*) (mtd)->priv)

#define VMTD_LOCAL_CALL(vmtd,name)	++(vmtd)->local_calls  [name]
#define VMTD_REMOTE_CALL(vmtd,name)	++(vmtd)->remote_calls [name]

#define VMTD_TOO_MANY_READ_OOB_MANY_FAILURES(vmtd) \
    ((vmtd)->oob_read_many_failures > VMTD_MAX_READ_MANY_OOBS_FAILURES)

/*----- Allocation/deallocation of messages -----*/

typedef struct vmtd_fe_link_t {
    vipc_list_t		vei_list;
    _Bool		mtd_changes;
    vipc_ctx_t		vipc_ctx;
} vmtd_fe_link_t;

#define VMTD_FE_LINK(link) \
	(*(vmtd_fe_link_t**) &((vmq_link_public_t*) (link))->priv)
#define VMTD_MAX_INDEX	0x7fffffff

    /* Can sleep for place in FIFO */

    static signed
vmtd_alloc_req_ex (vmq_link_t* link, unsigned remote_index, unsigned function,
		   unsigned data_len, NkDevMtdRequest** preq,
		   unsigned* data_offset)
{
    NkDevMtdMsg*	msg;
    signed		diag;

    diag = vmq_msg_allocate (link, data_len, (void**) &msg, data_offset);
    if (unlikely (diag)) {
	return diag;
    }
    OTRACE ("msg %p\n", msg);
    msg->req.index    = remote_index;
    msg->req.function = function;
    msg->req.flags    = NK_DEV_MTD_FLAGS_REQUEST;
    msg->req.dataOffset = data_offset ? *data_offset : 0;
    *preq = &msg->req;
    return 0;
}

    /* Can sleep for place in FIFO */

    static inline signed
vmtd_alloc_req (struct mtd_info* mtd, unsigned function, unsigned data_len,
		NkDevMtdRequest** preq, unsigned* data_offset)
{
    vmtd_vmtd_t* vmtd = VMTD (mtd);

    OTRACE ("\n");
    if (unlikely (vmtd->deleted)) return -ESTALE;
    return vmtd_alloc_req_ex (vmtd->link, vmtd->remote_mtd.index, function,
			      data_len, preq, data_offset);
}

    static inline void
vmtd_free_reply (vmq_link_t* link, NkDevMtdReply* reply)
{
    reply->cookie = 0;	/* For security versus stale requests */
    vmq_return_msg_free (link, reply);
}

    static inline void
vmtd_free_async (vmq_link_t* link, NkDevMtdReply* async)
{
    async->cookie = 0;	/* For security versus stale requests */
    vmq_msg_free (link, async);
}

/*----- Common call management -----*/

typedef struct {
    vipc_waiter_t	waiter;	/* Must be first */
    struct erase_info*	ei;
    _Bool		caller_waiting;
} vmtd_erase_info_t;

    static inline vmtd_erase_info_t*
vmtd_erase_cookie_to_vei (vmq_link_t* link, nku64_f erase_cookie)
{
    vipc_list_t* pvei_list = &VMTD_FE_LINK (link)->vei_list;

    return (vmtd_erase_info_t*)
	vipc_list_cookie_to_waiter (pvei_list, erase_cookie);
}

static const char* vmtd_func_names[] = {NK_DEV_MTD_FUNC_NAMES};
static void vmtd_erases_finished_notify (void);
static void vmtd_mtd_changes_notify (vmq_link_t* link);

    /* Executes in interrupt context */

    static void
vmtd_process_async (vmq_link_t* link, NkDevMtdReply* async)
{
    DTRACE ("%s link %d cookie %llx retcode %d\n", vmtd_func_names
	    [async->function % NK_DEV_MTD_FUNC_MAX],
	    vmq_peer_osid (link), async->cookie, async->retcode);
    if (async->flags != NK_DEV_MTD_FLAGS_NOTIFICATION) {
	ETRACE ("Got invalid flags %x cookie %llx\n", async->flags,
		async->cookie);
    } else if (async->function == NK_DEV_MTD_FUNC_ERASE_FINISHED) {
	vmtd_erase_info_t* vei;

	if ((vei = vmtd_erase_cookie_to_vei (link,
					     async->erase_cookie)) == NULL) {
	    ETRACE ("Got invalid cookie %llx\n", async->erase_cookie);
	} else {
	    vei->ei->state     = async->retcode;
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)
	    vei->ei->fail_addr = async->addr;	/* possible truncation */
#endif
	    vmtd_erases_finished_notify();
	}
    } else if (async->function == NK_DEV_MTD_FUNC_MTD_CHANGES) {
	vmtd_mtd_changes_notify (link);
    } else {
	ETRACE ("Got invalid function %d cookie %llx\n", async->function,
		async->cookie);
    }
    vmtd_free_async (link, async);
}

    /* Executes in interrupt context */

    static void
vmtd_process_reply (vmq_link_t* link, NkDevMtdMsg* msg)
{
    NkDevMtdReply* reply = (NkDevMtdReply*) msg;

    DTRACE ("%s link %d cookie %llx retcode %d\n", vmtd_func_names
	    [reply->function % NK_DEV_MTD_FUNC_MAX],
	    vmq_peer_osid (link), reply->cookie, reply->retcode);
    if (reply->flags != NK_DEV_MTD_FLAGS_REPLY ||
	    vipc_ctx_process_reply (&VMTD_FE_LINK (link)->vipc_ctx,
				    &reply->cookie)) {
	vmtd_free_reply (link, reply);
    }
}

    /*
     *   IN: have req
     *  OUT: req released, may have reply
     */

    static NkDevMtdReply*
vmtd_call_be (vmtd_vmtd_t* vmtd, NkDevMtdRequest* req)
{
    nku64_f* reply;

    VMTD_REMOTE_CALL (vmtd, req->function);
    reply = vipc_ctx_call (vmtd->vipc_ctx, &req->cookie);
    return reply ? container_of (reply, NkDevMtdReply, cookie) : NULL;
}

/*----- Handling of mtdblock.c -----*/

    /*
     *  The mtdblock.c driver calls read/write/erase/sync without
     *  ever performing get_device, so do it for him.
     */

static signed vmtd_get_device (struct mtd_info* mtd);

    static signed
vmtd_check_get_device (vmtd_vmtd_t* vmtd)
{
    VMTD_LOCK();
    if (!vmtd->get_device_done) {
	signed diag;

	vmtd->get_device_done = true;
	vmtd->implicit_get_device = true;
	VMTD_UNLOCK();
	diag = vmtd_get_device (&vmtd->local_mtd);
	if (diag) {
	    VMTD_LOCK();
	    vmtd->get_device_done = false;
	    vmtd->implicit_get_device = false;
	    VMTD_UNLOCK();
	    return diag;
	}
    } else {
	VMTD_UNLOCK();
    }
    return 0;
}

#define VMTD_CHECK_GET_DEVICE(vmtd) \
    if (!(vmtd)->get_device_done) { \
	signed diag2 = vmtd_check_get_device (vmtd); \
	if (diag2) return diag2; \
    }

#define VMTD_CHECK_GET_DEVICE_NO_RETURN(vmtd) \
    if (!(vmtd)->get_device_done) { \
	if (vmtd_check_get_device (vmtd)) return; \
    }

/*----- Implementation of struct mtd_info function pointers -----*/

#define VMTD_ERASE_FINISHED(state) \
    ((state) == MTD_ERASE_DONE || (state) == MTD_ERASE_FAILED)

    static _Bool
vmtd_erases_one_link (vmq_link_t* link, void* unused_cookie)
{
    vipc_list_t*	pvei_list = &VMTD_FE_LINK (link)->vei_list;
    unsigned long	spinlock_flags;
    vmtd_erase_info_t*	vei;

    (void) unused_cookie;
    DTRACE ("link %d\n", vmq_peer_osid (link));
restart:
    VIPC_LIST_LOCK (pvei_list, spinlock_flags);
    list_for_each_entry (vei, &pvei_list->list, waiter.list) {
	OTRACE ("vei %p\n", vei);
	if (VMTD_ERASE_FINISHED (vei->ei->state)) {
	    vipc_waiter_detach (&vei->waiter);
	    VIPC_LIST_UNLOCK (pvei_list, spinlock_flags);
	    DTRACE ("callback at %p priv %lx\n", vei->ei->callback,
		    vei->ei->priv);
	    mtd_erase_callback (vei->ei);
	    if (vei->caller_waiting) {
		DTRACE ("callback finished, signaling waiter\n");
		vipc_waiter_wakeup (&vei->waiter);
	    } else {
		DTRACE ("callback finished, freeing\n");
		kfree (vei);
	    }
	    goto restart;
	}
    }
    VIPC_LIST_UNLOCK (pvei_list, spinlock_flags);
    return false;
}

    static void
vmtd_erases_all_links (vmq_links_t* links)
{
    DTRACE ("\n");
    vmq_links_iterate (links, vmtd_erases_one_link, NULL);
}

static const _Bool vmtd_await_erase_callback = true;

    static signed
vmtd_erase (struct mtd_info* mtd, struct erase_info* ei)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    vipc_list_t*	pvei_list = &VMTD_FE_LINK (vmtd->link)->vei_list;
    vmtd_erase_info_t*	vei;
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;

    OTRACE ("ei %p priv %lx\n", ei, ei->priv);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_ERASE);

    VMTD_CHECK_GET_DEVICE (vmtd);

    vei = kzalloc (sizeof *vei, GFP_KERNEL);
    if (unlikely (!vei)) return -ENOMEM;

    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_ERASE, 0, &req, NULL);
    if (unlikely (diag)) {
	kfree (vei);
	return diag;
    }
    OTRACE ("vei %p\n", vei);

    vipc_waiter_init (&vei->waiter);
    vei->ei             = ei;
    vei->caller_waiting = vmtd_await_erase_callback;

    vmtd->cache_valid = false;
    req->offset = ei->addr;
    req->size   = ei->len;

    ei->state = MTD_ERASE_PENDING;

	/* The erase_cookie is passed through "addr" */
    vipc_list_add (pvei_list, &req->addr, &vei->waiter);

    reply = vmtd_call_be (vmtd, req);
    if (!reply) {
	vipc_list_del (pvei_list, &vei->waiter);
	kfree (vei);	/* XXX: should we do this here? */
	return -ESTALE;
    }
    DTRACE ("retcode %d fail_addr %llx\n", reply->retcode, reply->addr);
    diag = reply->retcode;
    if (diag) {
	ei->state     = MTD_ERASE_FAILED;
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)
	ei->fail_addr = reply->addr;	/* possible truncation */
#endif
	vipc_list_del (pvei_list, &vei->waiter);
	kfree (vei);
    }
    vmtd_free_reply (vmtd->link, reply);

    if (!diag && vei->caller_waiting) {
	DTRACE ("awaiting async callback\n");
	vipc_list_wait (pvei_list, &vei->waiter);
	DTRACE ("got async callback\n");
	    /* list_del performed before up() */
	kfree (vei);
    }
    return diag;
}

    static signed
vmtd_point (struct mtd_info* mtd, loff_t from, size_t len, size_t* retlen,
#if LINUX_VERSION_CODE < KERNEL_VERSION (2,6,27)
	    u_char** virt)	/* mtdbuf originally */
#else
	    void** virt, resource_size_t* phys)
#endif
{
    vmtd_vmtd_t*		vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;

    DTRACE ("from %llx len %x\n", from, len);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_POINT);
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_POINT, 0, &req, NULL);
    if (unlikely (diag)) return diag;
    req->offset = from;
    req->size   = len;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) return -ESTALE;

    *retlen = reply->retsize;
    diag    = reply->retcode;
    if (!diag) {
	nku64_f paddr = reply->addr;

	vmtd_free_reply (vmtd->link, reply);
	*virt = ioremap (paddr, *retlen);
	if (*virt) {
	    DTRACE ("Mapped %llx at %p\n", paddr, *virt);
	    return 0;
	}
	if (vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_UNPOINT, 0, &req, NULL)) {
	    return -ENOSPC;
	}
	req->offset = from;
	req->size   = len;
	req->addr   = paddr;

	reply = vmtd_call_be (vmtd, req);
	if (!reply) return -ESTALE;
    }
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}

    static void
vmtd_unpoint (struct mtd_info* mtd,
#if LINUX_VERSION_CODE < KERNEL_VERSION (2,6,27)
	      u_char* addr,
#endif
	      loff_t from, size_t len)
{
    vmtd_vmtd_t*       vmtd = VMTD (mtd);
    NkDevMtdRequest* req;
    NkDevMtdReply*   reply;

    DTRACE ("from %llx len %x\n", from, len);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_UNPOINT);
    if (vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_UNPOINT, 0, &req, NULL)) return;
    req->offset = from;
    req->size   = len;
#if LINUX_VERSION_CODE < KERNEL_VERSION (2,6,27)
    req->addr   = virt_to_phys (addr);
    DTRACE ("virt %p phys %llx\n", addr, req->addr);
#else
    req->addr   = 0;
#endif

    reply = vmtd_call_be (vmtd, req);
    if (!reply) return;
    vmtd_free_reply (vmtd->link, reply);
}

#define VMTD_FITS(vmtd,from,len) ((from & VMTD_CACHE_OUT(vmtd)) == \
			  ((from+len-1) & VMTD_CACHE_OUT(vmtd)))

    static signed
vmtd_read (struct mtd_info* mtd, loff_t from, size_t len, size_t* retlen,
	   u_char* buf)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    const _Bool		fits = VMTD_FITS (vmtd, from, len);
    const loff_t	from_down = from & VMTD_CACHE_OUT (vmtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;
    unsigned		data_offset;

    OTRACE ("from %llx len %x fits %d\n", from, len, fits);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_READ);

    VMTD_CHECK_GET_DEVICE (vmtd);

    if (len > vmq_data_max (vmtd->link)) {
	size_t retlentotal = 0;

	do {
	    size_t retlen2;

	    diag = vmtd_read (mtd, from, min (vmq_data_max (vmtd->link),
			      len), &retlen2, buf);
	    if (diag) return diag;
	    if (retlen2 <= 0) break;
	    retlentotal += retlen2;
	    from	+= retlen2;
	    len		-= retlen2;
	    buf		+= retlen2;
	} while (len > 0);

	*retlen = retlentotal;
	return 0;
    }
    if (fits) {
	if (vmtd->cache_valid && from_down == vmtd->cache_offset) {
	    memcpy (buf, vmtd->cache_data + (from & VMTD_CACHE_IN (vmtd)), len);
	    *retlen = len;
	    return 0;
	}
	diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_READ, vmtd->cache_size,
			       &req, &data_offset);
    } else {
	diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_READ, len, &req,
			       &data_offset);
    }
    if (diag) return diag;
    if (fits) {
	req->offset = from_down;
	req->size   = vmtd->cache_size;
    } else {
	req->offset = from;
	req->size   = len;
    }

    DTRACE ("call: offset %llx size %llx\n", req->offset, req->size);
    reply = vmtd_call_be (vmtd, req);
    if (!reply) {
	vmq_data_free (vmtd->link, data_offset);
	return -ESTALE;
    }
    diag = reply->retcode;
    if (!diag) {
	if (fits) {
	    const unsigned before = from & VMTD_CACHE_IN (vmtd);

	    if (reply->retsize == vmtd->cache_size) {
		memcpy (vmtd->cache_data, vmtd->data_area + data_offset,
			vmtd->cache_size);
		vmtd->cache_offset = from_down;
		vmtd->cache_valid = true;
		*retlen = len;
		memcpy (buf, vmtd->cache_data + before, len);
	    } else {
		if (reply->retsize <= before) {
		    *retlen = 0;
		} else if (reply->retsize <= before + len) {
		    *retlen = reply->retsize - before;
		} else {
		    *retlen = len;
		}
		memcpy (buf, vmtd->cache_data + before, *retlen);
	    }
	} else {
	    *retlen = reply->retsize;
	    memcpy (buf, vmtd->data_area + data_offset, *retlen);
	}
    } else {
	if (fits) {
	    vmtd->cache_valid = false;
	}
    }
    vmq_data_free (vmtd->link, data_offset);
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}

    static signed
vmtd_common_write (unsigned function, struct mtd_info* mtd, loff_t to,
		   size_t len, size_t* retlen, const u_char* buf)
{
    vmtd_vmtd_t*		vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;
    unsigned		data_offset;

    DTRACE ("%s: to %llx len %x\n", vmtd_func_names [function], to, len);
    VMTD_LOCAL_CALL (vmtd, function);

    VMTD_CHECK_GET_DEVICE (vmtd);

    diag = vmtd_alloc_req (mtd, function, len, &req, &data_offset);
    if (diag) return diag;
    vmtd->cache_valid = false;
    req->offset = to;
    req->size   = len;
    memcpy (vmtd->data_area + data_offset, buf, len);

    reply = vmtd_call_be (vmtd, req);
    vmq_data_free (vmtd->link, data_offset);
    if (!reply) return -ESTALE;

    *retlen = reply->retsize;
    diag    = reply->retcode;
    vmtd_free_reply (vmtd->link, reply);

    return diag;
}

    static signed
vmtd_write (struct mtd_info* mtd, loff_t to, size_t len, size_t* retlen,
	    const u_char* buf)
{
    return vmtd_common_write (NK_DEV_MTD_FUNC_WRITE, mtd, to, len, retlen,
			      buf);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
    static signed
vmtd_panic_write (struct mtd_info* mtd, loff_t to, size_t len, size_t* retlen,
		  const u_char* buf)
{
    return vmtd_common_write (NK_DEV_MTD_FUNC_PANIC_WRITE, mtd, to, len,
			      retlen, buf);
}
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,17)
    static signed
vmtd_read_oob_uncached (struct mtd_info* mtd, const loff_t from,
			struct mtd_oob_ops* ops)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    const size_t	ops_len    = ops->datbuf ? ops->len    : 0;
    const size_t	ops_ooblen = ops->oobbuf ? ops->ooblen : 0;
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;
    unsigned		data_offset;
	/*
	 *  We can get ops->datbuf=NULL and ops->len=<junk>.
	 *  We will consider that ops->len=0 in this case,
	 *  given that it is not possible to pass a NULL
	 *  pointer to the backend, except if len is 0 too.
	 */
    DTRACE ("mode %d from %llx ooboffs %x len %x ooblen %x\n", ops->mode,
	from, ops->ooboffs, ops->len, ops->ooblen);
	/* Data and OOB buffers follow each other in shared memory */
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_READ_OOB,
			   ops_len + ops_ooblen, &req, &data_offset);
    if (diag) return diag;
    req->mode    = ops->mode;
    req->offset  = from;
    req->offset2 = ops->ooboffs;
    req->size    = ops_len;
    req->size2   = ops_ooblen;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) {
	vmq_data_free (vmtd->link, data_offset);
	return -ESTALE;
    }
    ops->retlen    = reply->retsize;
    ops->oobretlen = reply->retsize2;
    diag           = reply->retcode;
    DTRACE ("diag %d retlen %x oobretlen %x\n", diag, ops->retlen,
	    ops->oobretlen);
    if (!diag) {
	memcpy (ops->datbuf, vmtd->data_area + data_offset, ops->retlen);
	memcpy (ops->oobbuf, vmtd->data_area + data_offset + ops_len,
		ops->oobretlen);
    }
    vmq_data_free (vmtd->link, data_offset);
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}

    static signed
vmtd_read_many_oobs (struct mtd_info* mtd, const loff_t from,
		     const struct mtd_oob_ops* ops, unsigned requested_oobs)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;
    unsigned		data_offset;

    DTRACE ("mode %d from %llx ooboffs %x ooblen %x\n", ops->mode,
	    from, ops->ooboffs, ops->ooblen);
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_READ_MANY_OOBS,
			   sizeof vmtd->oob_data, &req, &data_offset);
    if (diag) return diag;
    req->mode    = ops->mode;
    req->offset  = from;
    req->offset2 = ops->ooboffs;
    req->size    = requested_oobs;
    req->size2   = ops->ooblen;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) {
	vmq_data_free (vmtd->link, data_offset);
	return -ESTALE;
    }
    diag = reply->retcode;
    DTRACE ("diag %d retsize %x retsize2 %x\n", diag, reply->retsize,
	    reply->retsize2);
    if (!diag) {
	    /*
	     *  retsize is the number of items returned.
	     *  retsize2 is the total number of useful bytes,
	     *  rather than the total number of requested bytes.
	     *  Useful bytes is equal to requested bytes, except
	     *  when the caller asked for more OOB bytes than available.
	     *  +-------------+-------------+------------+
	     *  |        |    |        |    |        |   |
	     *  +-------------+-------------+------------+
	     *  <--- ooblen -->
	     *         1      +      1      +     1     == retsize (items)
	     *  <--------> +  <-------->  + <-------->  == retsize2
	     *  <----- retsize (items) * ops->ooblen ---->
	     */
	memcpy (vmtd->oob_data, vmtd->data_area + data_offset,
		reply->retsize * ops->ooblen);
	vmtd->oob_offset           = from;
	vmtd->oob_oobmode          = ops->mode;
	vmtd->oob_ooboffset        = ops->ooboffs;
	vmtd->oob_ooblen_requested = ops->ooblen;
	vmtd->oob_ooblen_obtained  = reply->retsize2 / reply->retsize;
	vmtd->oob_validoobs        = reply->retsize;
    }
    vmq_data_free (vmtd->link, data_offset);
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}

    static signed
vmtd_read_oob (struct mtd_info* mtd, const loff_t from,
	       struct mtd_oob_ops* ops)
{
    vmtd_vmtd_t* vmtd = VMTD (mtd);
    const size_t ops_len = ops->datbuf ? ops->len : 0;
    loff_t	sector;
    unsigned	offset_in_sector;
    nku32_f	aligned_from;
    unsigned	oob_index;
    unsigned	requested_oobs;
	/*
	 *  We can get ops->datbuf=NULL and ops->len=<junk>.
	 *  We will consider that ops->len=0 in this case,
	 *  given that it is not possible to pass a NULL
	 *  pointer to the backend, except if len is 0 too.
	 *
	 *  We could also try to redirect this call to
	 *  plain vmtd_read() when OOB data are not asked
	 *  for, to benefit from read caching, but in
	 *  practice callers do not use this function when
	 *  they do not need OOB data.
	 */
    OTRACE ("mode %d from %llx ooboffs %x len %x ooblen %x\n", ops->mode,
	    from, ops->ooboffs, ops->len, ops->ooblen);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_READ_OOB);
    if (ops_len || !ops->ooblen || ops->ooblen > sizeof vmtd->oob_data ||
	    !ops->oobbuf || VMTD_TOO_MANY_READ_OOB_MANY_FAILURES (vmtd)) {
	return vmtd_read_oob_uncached (mtd, from, ops);
    }
    sector           = from;
    offset_in_sector = do_div (sector, mtd->writesize);
    if (unlikely (offset_in_sector || sector > (unsigned long) -1)) {
	return vmtd_read_oob_uncached (mtd, from, ops);
    }
    requested_oobs = sizeof vmtd->oob_data / ops->ooblen;
    aligned_from = ((unsigned long) sector/requested_oobs) *
	requested_oobs * mtd->writesize;

    if (!vmtd->oob_validoobs ||
	 vmtd->oob_oobmode          != ops->mode ||
	 vmtd->oob_ooboffset        != ops->ooboffs ||
	 vmtd->oob_ooblen_requested != ops->ooblen ||
	 vmtd->oob_offset           != aligned_from) {

	if (vmtd_read_many_oobs (mtd, aligned_from, ops, requested_oobs)) {
	    ++vmtd->oob_read_many_failures;
	    return vmtd_read_oob_uncached (mtd, from, ops);
	}
    }
    oob_index = (unsigned long) sector % requested_oobs;
    if (unlikely (oob_index >= vmtd->oob_validoobs)) {
	return vmtd_read_oob_uncached (mtd, from, ops);
    }
    ops->oobretlen = vmtd->oob_ooblen_obtained;
    memcpy (ops->oobbuf,
	    vmtd->oob_data + oob_index * vmtd->oob_ooblen_requested,
	    vmtd->oob_ooblen_obtained);
    return 0;
}

    static signed
vmtd_write_oob (struct mtd_info* mtd, loff_t to, struct mtd_oob_ops* ops)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    const size_t	ops_len    = ops->datbuf ? ops->len    : 0;
    const size_t	ops_ooblen = ops->oobbuf ? ops->ooblen : 0;
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;
    unsigned		data_offset;
	/*
	 *  We can get ops->datbuf=NULL and ops->len=<junk>.
	 *  We will consider that ops->len=0 in this case,
	 *  given that it is not possible to pass a NULL
	 *  pointer to the backend, except if len is 0 too.
	 *
	 *  When using "nandwrite -a -n /dev/mtdX imagefile", we can
	 *  get ops->oobbuf=NULL and a ops->ooblen=<junk>, for example
	 *  equal to the writesize. We will consider that ops->ooblen=0
	 *  in this case.
	 */
    DTRACE ("mode %d to %llx ooboffs %x dat %p %x oob %p %x\n", ops->mode,
	to, ops->ooboffs, ops->datbuf, ops->len, ops->oobbuf, ops->ooblen);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_WRITE_OOB);
	/* Data and OOB buffers follow each other in shared memory */
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_WRITE_OOB,
			   ops_len + ops_ooblen, &req, &data_offset);
    if (diag) return diag;
    vmtd->cache_valid = false;
    vmtd->oob_validoobs = 0;
    req->mode    = ops->mode;
    req->offset  = to;
    req->offset2 = ops->ooboffs;
    req->size    = ops_len;
    req->size2   = ops_ooblen;
    memcpy (vmtd->data_area + data_offset, ops->datbuf, ops_len);
    memcpy (vmtd->data_area + data_offset + ops_len, ops->oobbuf,
	    ops_ooblen);

    reply = vmtd_call_be (vmtd, req);
    vmq_data_free (vmtd->link, data_offset);
    if (!reply) return -ESTALE;

    ops->retlen    = reply->retsize;
    ops->oobretlen = reply->retsize2;
    diag           = reply->retcode;
    vmtd_free_reply (vmtd->link, reply);

    return diag;
}
#endif

    /*
     *  For now, we suppose that local "struct otp_info" definition
     *  is identical to NkDevMtdOtpInfo all the time, so we do not
     *  need to convert between formats at tx/rx time.
     */

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)
typedef char vmtd_check3_t [sizeof (struct otp_info) ==
			    sizeof (NkDevMtdOtpInfo) ? 1 : -1];

    /* We must return the size in bytes or the -ERROR */

    static signed
vmtd_get_fact_prot_info (struct mtd_info* mtd, struct otp_info* buf,
			 size_t len)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;
    unsigned		data_offset;

    DTRACE ("len %x\n", len);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_GET_FACT_PROT_INFO);

    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_GET_FACT_PROT_INFO, len, &req,
			   &data_offset);
    if (diag) return diag;
    req->size   = len;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) {
	vmq_data_free (vmtd->link, data_offset);
	return -ESTALE;
    }
    diag = reply->retcode;
    if (diag > 0) {
	memcpy (buf, vmtd->data_area + data_offset, diag);
    }
    vmq_data_free (vmtd->link, data_offset);
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}
#endif

    /* Like vmtd_read, without cache management */

    static signed
vmtd_read_fact_prot_reg (struct mtd_info* mtd, loff_t from, size_t len,
			 size_t* retlen, u_char* buf)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;
    unsigned		data_offset;

    DTRACE ("from %llx len %x\n", from, len);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_READ_FACT_PROT_REG);

    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_READ_FACT_PROT_REG, len, &req,
			   &data_offset);
    if (diag) return diag;
    req->offset = from;
    req->size   = len;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) {
	vmq_data_free (vmtd->link, data_offset);
	return -ESTALE;
    }
    diag = reply->retcode;
    if (!diag) {
	*retlen = reply->retsize;
	memcpy (buf, vmtd->data_area + data_offset, *retlen);
    }
    vmq_data_free (vmtd->link, data_offset);
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)

    /* Identical to get_fact_prot_info, except for code */

    static signed
vmtd_get_user_prot_info (struct mtd_info* mtd, struct otp_info* buf,
			 size_t len)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;
    unsigned		data_offset;

    DTRACE ("len %x\n", len);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_GET_USER_PROT_INFO);

    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_GET_USER_PROT_INFO, len, &req,
			   &data_offset);
    if (diag) return diag;
    req->size   = len;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) {
	vmq_data_free (vmtd->link, data_offset);
	return -ESTALE;
    }
    diag = reply->retcode;
    if (diag > 0) {
	memcpy (buf, vmtd->data_area + data_offset, diag);
    }
    vmq_data_free (vmtd->link, data_offset);
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}
#endif

    /* Like vmtd_read, without cache management */

    static signed
vmtd_read_user_prot_reg (struct mtd_info* mtd, loff_t from, size_t len,
			 size_t* retlen, u_char* buf)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;
    unsigned		data_offset;

    DTRACE ("from %llx len %x\n", from, len);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_READ_USER_PROT_REG);

    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_READ_USER_PROT_REG, len, &req,
			   &data_offset);
    if (diag) return diag;
    req->offset = from;
    req->size   = len;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) {
	vmq_data_free (vmtd->link, data_offset);
	return -ESTALE;
    }
    diag = reply->retcode;
    if (!diag) {
	*retlen = reply->retsize;
	memcpy (buf, vmtd->data_area + data_offset, *retlen);
    }
    vmq_data_free (vmtd->link, data_offset);
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}

    static signed
vmtd_write_user_prot_reg (struct mtd_info* mtd, loff_t to, size_t len,
			  size_t* retlen, u_char* buf)
{
    return vmtd_common_write (NK_DEV_MTD_FUNC_WRITE_USER_PROT_REG, mtd,
			      to, len, retlen, buf);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)

    /* Identical to vmtd_lock, except for function code */

    static signed
vmtd_lock_user_prot_reg (struct mtd_info* mtd, loff_t from, size_t len)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;

    DTRACE ("\n");
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_LOCK_USER_PROT_REG);
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_LOCK_USER_PROT_REG, 0, &req,
			   NULL);
    if (diag) return diag;
    req->offset = from;
    req->size   = len;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) return -ESTALE;

    diag    = reply->retcode;
    vmtd_free_reply (vmtd->link, reply);

    return diag;
}
#endif

    static signed
vmtd_writev (struct mtd_info* mtd, const struct kvec* vecs,
	     const unsigned long count, const loff_t to, size_t* retlen)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    size_t		len = 0;
    unsigned		i;
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;
    unsigned		data_offset;

    for (i = 0; i < count; ++i) {
	len += vecs [i].iov_len;
    }
    DTRACE ("to %llx count %ld len %x\n", to, count, len);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_WRITEV);
    if (count <= 1 || len > vmq_data_max (vmtd->link)) {
	return default_mtd_writev (mtd, vecs, count, to, retlen);
    }
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_WRITE, len, &req,
			   &data_offset);
    if (diag) return diag;
    vmtd->cache_valid = false;
    req->offset = to;
    req->size   = len;
    {
	char* dst = vmtd->data_area + data_offset;

	for (i = 0; i < count; ++i, ++vecs) {
	    memcpy (dst, vecs->iov_base, vecs->iov_len);
	    dst += vecs->iov_len;
	}
    }
    reply = vmtd_call_be (vmtd, req);
    vmq_data_free (vmtd->link, data_offset);
    if (!reply) return -ESTALE;

    *retlen = reply->retsize;
    diag    = reply->retcode;
    vmtd_free_reply (vmtd->link, reply);

    return diag;
}

    static void
vmtd_sync (struct mtd_info* mtd)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;

    DTRACE ("\n");
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_SYNC);

    VMTD_CHECK_GET_DEVICE_NO_RETURN (vmtd);

    if (vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_SYNC, 0, &req, NULL)) return;
    reply = vmtd_call_be (vmtd, req);
    if (!reply) return;
    vmtd_free_reply (vmtd->link, reply);
}

    static signed
vmtd_lock (struct mtd_info* mtd, loff_t ofs,
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,29)
    uint64_t len)
#else
    size_t len)
#endif
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;

    DTRACE ("\n");
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_LOCK);
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_LOCK, 0, &req, NULL);
    if (diag) return diag;
    req->offset = ofs;
    req->size   = len;		/* 64 bits in 2.6.30 */

    reply = vmtd_call_be (vmtd, req);
    if (!reply) return -ESTALE;

    diag    = reply->retcode;
    vmtd_free_reply (vmtd->link, reply);

    return diag;
}

    static signed
vmtd_unlock (struct mtd_info* mtd, loff_t ofs,
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,29)
    uint64_t len)
#else
    size_t len)
#endif
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;

    DTRACE ("\n");
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_UNLOCK);
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_UNLOCK, 0, &req, NULL);
    if (diag) return diag;
    req->offset = ofs;
    req->size   = len;		/* 64 bits in 2.6.30 */

    reply = vmtd_call_be (vmtd, req);
    if (!reply) return -ESTALE;

    diag    = reply->retcode;
    vmtd_free_reply (vmtd->link, reply);

    return diag;
}

    static signed
vmtd_suspend (struct mtd_info* mtd)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;

    DTRACE ("\n");
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_SUSPEND);
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_SUSPEND, 0, &req, NULL);
    if (diag) return diag;
    reply = vmtd_call_be (vmtd, req);
    if (!reply) return -ESTALE;

    diag = reply->retcode;
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}

    static void
vmtd_resume (struct mtd_info* mtd)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;

    DTRACE ("\n");
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_RESUME);
    if (vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_RESUME, 0, &req, NULL)) return;
    reply = vmtd_call_be (vmtd, req);
    if (!reply) return;

    vmtd_free_reply (vmtd->link, reply);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)

    static signed
vmtd_block_isbad_uncached (struct mtd_info *mtd, loff_t ofs)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;

    DTRACE ("ofs %llx\n", ofs);
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_BLOCK_ISBAD, 0, &req, NULL);
    if (diag) return diag;
    req->offset = ofs;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) return -ESTALE;

    diag = reply->retcode;
    vmtd_free_reply (vmtd->link, reply);

    return diag;
}

    static signed
vmtd_blocks_arebad (struct mtd_info *mtd, loff_t ofs, unsigned* bits,
		    nku32_f* badmask)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;

    DTRACE ("ofs %llx\n", ofs);
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_BLOCKS_AREBAD, 0, &req, NULL);
    if (diag) return diag;
    req->offset = ofs;
    req->size   = 32;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) return -ESTALE;

    diag     = reply->retcode;
    *bits    = reply->retsize;
    *badmask = reply->retsize2;
    vmtd_free_reply (vmtd->link, reply);

    return diag;
}

    static signed
vmtd_block_isbad (struct mtd_info *mtd, const loff_t ofs)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    loff_t		result = ofs;
    const unsigned	remains = do_div (result, mtd->erasesize);
    const unsigned	erase_block = result;
    const unsigned	erase_index = erase_block / 32;
    const unsigned	erase_bit   = erase_block % 32;

    DTRACE ("ofs %llx\n", ofs);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_BLOCK_ISBAD);
    if (remains) {
	return vmtd_block_isbad_uncached (mtd, ofs);
    }
    if (!vmtd->isbad_validbits || erase_index != vmtd->isbad_index) {
	unsigned bits;
	nku32_f	badmask;

	if (vmtd_blocks_arebad (mtd, (loff_t)erase_index * 32 * mtd->erasesize,
				&bits, &badmask)) {
	    return vmtd_block_isbad_uncached (mtd, ofs);
	}
	vmtd->isbad_badmask	= badmask;
	vmtd->isbad_index	= erase_index;
	vmtd->isbad_validbits	= bits;
    }
    if (erase_bit >= vmtd->isbad_validbits) {
	return vmtd_block_isbad_uncached (mtd, ofs);
    }
    return !!(vmtd->isbad_badmask & (1 << erase_bit));
}

    static signed
vmtd_block_markbad (struct mtd_info *mtd, loff_t ofs)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;

    DTRACE ("\n");
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_BLOCK_MARKBAD);
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_BLOCK_MARKBAD, 0, &req, NULL);
    if (diag) return diag;
    req->offset = ofs;
    vmtd->isbad_validbits = 0;
    vmtd->oob_validoobs = 0;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) return -ESTALE;

    diag = reply->retcode;
    vmtd_free_reply (vmtd->link, reply);

    return diag;
}
#endif

   static signed
vmtd_get_device (struct mtd_info* mtd)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;

    DTRACE ("index %d\n", mtd->index);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_GET_DEVICE);
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_GET_DEVICE, 0, &req, NULL);
    if (diag) return diag;
    reply = vmtd_call_be (vmtd, req);
    if (!reply) return -ESTALE;

    diag = reply->retcode;

    if (!diag) {
	VMTD_LOCK();
	vmtd->get_device_done = true;
	VMTD_UNLOCK();
    }
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,17)
static void vmtd_retry_deletions_notify (void);

   static void
vmtd_put_device (struct mtd_info* mtd)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;

    DTRACE ("index %d\n", mtd->index);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_PUT_DEVICE);
    if (vmtd->deleted) {
	    /*
	     *  The mtd_table_mutex is taken here, so
	     *  calling mtd_device_unregister() would deadlock.
	     */
	vmtd_retry_deletions_notify();
	return;
    }
    if (vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_PUT_DEVICE, 0, &req, NULL))
	return;
    reply = vmtd_call_be (vmtd, req);
    if (!reply) return;

    VMTD_LOCK();
    vmtd->get_device_done = false;
    vmtd->implicit_get_device = false;
    VMTD_UNLOCK();

    vmtd_free_reply (vmtd->link, reply);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,30)
    static unsigned long
vmtd_get_unmapped_area (struct mtd_info* mtd, unsigned long len,
			unsigned long offset, unsigned long flags)
{
    vmtd_vmtd_t*	vmtd = VMTD (mtd);
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;

    DTRACE ("len %lx offset %lx flags %lx\n", len, offset, flags);
    VMTD_LOCAL_CALL (vmtd, NK_DEV_MTD_FUNC_GET_UNMAPPED_AREA);
    diag = vmtd_alloc_req (mtd, NK_DEV_MTD_FUNC_GET_UNMAPPED_AREA, 0, &req,
			   NULL);
    if (diag) return diag;
    req->offset = offset;
    req->size   = len;
    req->addr   = flags;

    reply = vmtd_call_be (vmtd, req);
    if (!reply) return -ESTALE;

    diag = reply->retcode;
    if (!diag) {
	unsigned long retvalue = reply->addr;

	vmtd_free_reply (vmtd->link, reply);
	return retvalue;
    }
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}
#endif

/*----- VMTD creation and deletion -----*/

    /* Called from vmtd_acquire_mtds_from() only */

    static signed
vmtd_get_next_mtd (vmtd_vmtd_t* vmtd, signed remote_index)
{
    NkDevMtdRequest*	req;
    NkDevMtdReply*	reply;
    signed		diag;
    unsigned		data_offset;
    _Bool		had_mtd = false;
    _Bool		had_erase_region_info = false;
    unsigned		numeraseregions = 0;

    DTRACE ("remote_index %d\n", remote_index);
    diag = vmtd_alloc_req_ex (vmtd->link, remote_index,
			      NK_DEV_MTD_FUNC_GET_NEXT_MTD,
			      vmq_data_max (vmtd->link),
			      &req, &data_offset);
    if (diag) return diag;
    req->offset2 = 0x300;	/* Version 3.0 */
    req->size    = vmq_data_max (vmtd->link);

    reply = vmtd_call_be (vmtd, req);
    if (!reply) {
	vmq_data_free (vmtd->link, data_offset);
	return -ESTALE;
    }
    diag = reply->retcode;
    if (!diag) {
	NkDevMtdGetNextMtd*	next    = (NkDevMtdGetNextMtd*)
					  (vmtd->data_area + data_offset);
	unsigned		remains = reply->retsize;

	while (remains >= sizeof *next + next->len) {
	    switch (next->type) {
	    case NK_DEV_MTD_FIELD_MTD:
		    /* The size of this structure can grow */
		memcpy (&vmtd->remote_mtd, next + 1,
			min (next->len, sizeof (vmtd->remote_mtd)));
		had_mtd = true;
		break;

	    case NK_DEV_MTD_FIELD_ERASE_REGION_INFO:
		    /* The size of NkDevMtdEraseRegionInfo cannot grow */
		if (vmtd->erase_region_info) {
		    DTRACE ("Duplicate erase region info record.\n");
		    diag = -EINVAL;
		    goto error;
		}
		vmtd->erase_region_info = kzalloc (next->len, GFP_KERNEL);
		if (!vmtd->erase_region_info) {
		    DTRACE ("Out of memory for erase region info.\n");
		    diag = -ENOMEM;
		    goto error;
		}
		numeraseregions = next->len / sizeof (NkDevMtdEraseRegionInfo);
		memcpy (vmtd->erase_region_info, next + 1, next->len);
		had_erase_region_info = true;
		break;

	    default:
		DTRACE ("Ignoring unknown type %d len %d\n",
			next->type, next->len);
		break;
	    }
	    remains -= sizeof *next + next->len;
	    next = (NkDevMtdGetNextMtd*) ((char*)(next + 1) + next->len);
	}
	if (!had_mtd || (vmtd->remote_mtd.numeraseregions != numeraseregions)) {
	    DTRACE ("Protocol error, had_mtd %d, regions %d versus %d\n",
		    had_mtd, vmtd->remote_mtd.numeraseregions, numeraseregions);
	    diag = -EINVAL;
	}
    }
error:
    vmq_data_free (vmtd->link, data_offset);
    vmtd_free_reply (vmtd->link, reply);
    return diag;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,17)
typedef char vmtd_check1_t [sizeof (struct nand_oobfree) ==
			    sizeof (NkDevMtdNandOobFree) ? 1 : -1];
typedef char vmtd_check2_t [sizeof (struct nand_ecclayout) ==
			    sizeof (NkDevMtdNandEccLayout) ? 1 : -1];
#endif

    /* Called only from vmtd_acquire_mtds_from() */

    static signed
vmtd_init_mtd_info (vmtd_vmtd_t* vmtd)
{
    NkDevMtd*		remote = &vmtd->remote_mtd;
    struct mtd_info*	mtd    = &vmtd->local_mtd;
    signed		diag;

    DTRACE ("remote index %d type %d name '%s'\n", remote->index, remote->type,
	    remote->name);
    memset (mtd, 0, sizeof *mtd);

#define VMTD_FIELD(x)	mtd->x = remote->x

    VMTD_FIELD (type);
    VMTD_FIELD (flags);
    VMTD_FIELD (size);
    VMTD_FIELD (erasesize);
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,17)
    VMTD_FIELD (writesize);
#else
    mtd->oobblock = remote->writesize;
#endif
    VMTD_FIELD (oobsize);

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)
    VMTD_FIELD (oobavail);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,30)
    VMTD_FIELD (erasesize_shift);
    VMTD_FIELD (writesize_shift);
    VMTD_FIELD (erasesize_mask);
    VMTD_FIELD (writesize_mask);
#endif

    mtd->name = kmalloc (strlen (remote->name) + 1, GFP_KERNEL);
    if (!mtd->name) {
	ETRACE ("Out of memory for mtd name.\n");
	diag = -ENOMEM;
	goto error;
    }
	/* "name" is "char*" in 2.6.23 and "const char*" in 2.6.27 */
    strcpy ((char*) mtd->name, remote->name);

	/* mtd_device_register() will set mtd->index (local value) */

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,17)
    if (remote->ecclayoutexists) {
	mtd->ecclayout = kmalloc (sizeof (remote->ecclayout), GFP_KERNEL);
	if (!mtd->ecclayout) {
	    ETRACE ("Out of memory for ecclayout.\n");
	    diag = -ENOMEM;
	    goto error;
	}
	memcpy (mtd->ecclayout, &remote->ecclayout, sizeof remote->ecclayout);
    }
#endif
    VMTD_FIELD (numeraseregions);
    if (remote->numeraseregions) {
	unsigned i;

	mtd->eraseregions = kzalloc (mtd->numeraseregions *
				     sizeof (struct mtd_erase_region_info),
				     GFP_KERNEL);
	if (!mtd->eraseregions) {
	    ETRACE ("Out of memory for erase regions.\n");
	    diag = -ENOMEM;
	    goto error;
	}
#define VMTD_FIELD2(field) \
	mtd->eraseregions [i].field = vmtd->erase_region_info [i].field

	for (i = 0; i < (unsigned) mtd->numeraseregions; ++i) {
	    VMTD_FIELD2 (offset);
	    VMTD_FIELD2 (erasesize);
	    VMTD_FIELD2 (numblocks);
	}
#undef VMTD_FIELD2
    }

#define VMTD_FUNC(x,y) \
    if (remote->available_functions & (1 << NK_DEV_MTD_FUNC_##x)) { \
	mtd->y = vmtd_##y; \
    }

    VMTD_FUNC (ERASE,		erase);
    VMTD_FUNC (POINT,		point);
    VMTD_FUNC (UNPOINT,		unpoint);
    VMTD_FUNC (READ,		read);
    VMTD_FUNC (WRITE,		write);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
    VMTD_FUNC (PANIC_WRITE,	panic_write);
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,17)
    VMTD_FUNC (READ_OOB,	read_oob);
    VMTD_FUNC (WRITE_OOB,	write_oob);
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)
    VMTD_FUNC (GET_FACT_PROT_INFO,	get_fact_prot_info);
#endif
    VMTD_FUNC (READ_FACT_PROT_REG,	read_fact_prot_reg);
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)
    VMTD_FUNC (GET_USER_PROT_INFO,	get_user_prot_info);
#endif
    VMTD_FUNC (READ_USER_PROT_REG,	read_user_prot_reg);
    VMTD_FUNC (WRITE_USER_PROT_REG,	write_user_prot_reg);
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)
    VMTD_FUNC (LOCK_USER_PROT_REG,	lock_user_prot_reg);
#endif

    VMTD_FUNC (WRITEV,		writev);
    VMTD_FUNC (SYNC,		sync);
    VMTD_FUNC (LOCK,		lock);
    VMTD_FUNC (UNLOCK,		unlock);
    VMTD_FUNC (SUSPEND,		suspend);
    VMTD_FUNC (RESUME,		resume);
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)
    VMTD_FUNC (BLOCK_ISBAD,	block_isbad);
    VMTD_FUNC (BLOCK_MARKBAD,	block_markbad);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,30)
    VMTD_FUNC (GET_UNMAPPED_AREA, get_unmapped_area);
#endif

    /* mtd->reboot_notifier */

	/* XXX: we need to sync this with backend better */
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,17)
    VMTD_FIELD (ecc_stats.corrected);
    VMTD_FIELD (ecc_stats.failed);
    VMTD_FIELD (ecc_stats.badblocks);
    VMTD_FIELD (ecc_stats.bbtblocks);

    VMTD_FIELD (subpage_sft);
#endif

    mtd->priv        = vmtd;
    mtd->owner       = THIS_MODULE;
    mtd->usecount    = 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,17)
    mtd->get_device  = vmtd_get_device;
    mtd->put_device  = vmtd_put_device;
#endif

    vmtd->cache_size = mtd->writesize;
	/* NOR memory has a writesize of just one byte */
    if (vmtd->cache_size < 512) {
	vmtd->cache_size = 512;
    }
    if (vmtd->cache_size & VMTD_CACHE_IN (vmtd)) {
	ETRACE ("Flash writesize is not a power of 2.\n");
	diag = -EINVAL;
	goto error;
    }
    vmtd->cache_data = kzalloc (vmtd->cache_size, GFP_KERNEL);
    if (!vmtd->cache_data) {
	ETRACE ("Out of memory for cache data.\n");
	diag = -ENOMEM;
	goto error;
    }

    diag = mtd_device_register (mtd, NULL, 0);
    if (diag) goto error;
    vmtd->added = true;
    return 0;

error:
	/* Cleanup done by caller */
    return diag;
}

#undef VMTD_FIELD

    static void
vmtd_free_vmtd (vmtd_vmtd_t* vmtd)
{
    kfree (vmtd->cache_data);
    kfree (vmtd->local_mtd.name);
    kfree (vmtd->local_mtd.eraseregions);
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,17)
    kfree (vmtd->local_mtd.ecclayout);
#endif
    kfree (vmtd->erase_region_info);
    kfree (vmtd);
}

static LIST_HEAD (vmtd_vmtds);

    /*
     *  Callback from vlx-vmq.c in the context of vmtd_thread()
     *  so no need to lock the list of vmtds.
     */

    static void
vmtd_link_off (vmq_link_t* link)
{
    vmtd_vmtd_t* vmtd;

    DTRACE ("link %d\n", vmq_peer_osid (link));
	/*
	 *  Mark deleted to make all future invocations
	 *  fail, until the client decides to put_mtd_device
	 *  finally.
	 */
    list_for_each_entry (vmtd, &vmtd_vmtds, list) {
	if (vmtd->link != link) continue;
	DTRACE ("deleting remote index %d\n", vmtd->remote_mtd.index);
	vmtd->deleted = true;
    }
    vipc_ctx_abort_calls (&VMTD_FE_LINK (link)->vipc_ctx);
}

    static void
vmtd_release_deleted (void)
{
    vmtd_vmtd_t* vmtd;

    DTRACE ("\n");
restart:
    list_for_each_entry (vmtd, &vmtd_vmtds, list) {
	DTRACE ("deleting remote index %d\n", vmtd->remote_mtd.index);
	if (!vmtd->deleted) continue;
	if (mtd_device_unregister (&vmtd->local_mtd)) {
	    WTRACE ("Still busy mtd index %d name '%s'\n",
		    vmtd->local_mtd.index, vmtd->local_mtd.name);
	    continue;
	}
	DTRACE ("Finally deleted mtd index %d name '%s'\n",
		vmtd->local_mtd.index, vmtd->local_mtd.name);
	list_del (&vmtd->list);
	vmtd_free_vmtd (vmtd);
	goto restart;
    }
}

    static void
vmtd_release_mtds (const vmq_link_t* link, const signed first_index,
		   const signed last_index)
{
    vmtd_vmtd_t* vmtd;

    DTRACE ("link %d first %d last %d\n",
	    link ? vmq_peer_osid (link) : 0, first_index, last_index);
restart:
    list_for_each_entry (vmtd, &vmtd_vmtds, list) {
	if (link && (vmtd->link != link ||
	       (signed) vmtd->remote_mtd.index < first_index ||
	       (signed) vmtd->remote_mtd.index > last_index))
	    continue;
	DTRACE ("deleting remote index %d\n", vmtd->remote_mtd.index);
	if (vmtd->added && mtd_device_unregister (&vmtd->local_mtd)) {
	    vmtd->deleted = true;
	    WTRACE ("Deferring deletion of busy mtd index %d name '%s'\n",
		    vmtd->local_mtd.index, vmtd->local_mtd.name);
	    continue;
	}
	list_del (&vmtd->list);
	vmtd_free_vmtd (vmtd);
	goto restart;
    }
}

    static void
vmtd_link_off_completed (vmq_link_t* link)
{
    vmtd_release_mtds (link, 0, VMTD_MAX_INDEX);
}

    static signed
vmtd_acquire_mtds_from (vmq_link_t* link)
{
    signed	diag = -EINVAL;
    signed	start_index, last_start_index;
    char*	data_area = vmq_tx_data_area (link);

    DTRACE ("\n");
    for (start_index = last_start_index = -1;;) {
	vmtd_vmtd_t* vmtd = kzalloc (sizeof (struct vmtd_vmtd_t), GFP_KERNEL);
	const NkDevMtd* remote = &vmtd->remote_mtd;

	if (!vmtd) {
	    ETRACE ("Out of memory for vmtd_vmtd_t.\n");
	    diag = -ENOMEM;
	    goto error;
	}
	INIT_LIST_HEAD (&vmtd->list);
	vmtd->link = link;
	vmtd->vipc_ctx = &VMTD_FE_LINK (link)->vipc_ctx;
	vmtd->data_area = data_area;
	if (vmtd_get_next_mtd (vmtd, start_index)) {
		/* Disappeared from server, so delete ours */
	    vmtd_free_vmtd (vmtd);
	    vmtd_release_mtds (link, start_index + 1, VMTD_MAX_INDEX);
	    break;
	}
	DTRACE ("remote: index %d type %d name '%s'\n", remote->index,
		remote->type, remote->name);
	if (remote->version != sizeof *remote) {
		/* We ignore this entry, and throw away ours if any */
	    ETRACE ("Version mismatch %d != %d\n", remote->version,
		    sizeof *remote);
	    vmtd_free_vmtd (vmtd);
	    vmtd_release_mtds (link, last_start_index + 1, remote->index);
	    start_index = last_start_index = remote->index;
	    continue;
	}
	    /* unsigned versus signed, -1 would become maxuint */
	if ((signed) remote->index <= start_index) {
		/*
		 * Protocol error, we stop everything
		 * and delete all MTDs for this link.
		 */
	    ETRACE ("MTD index mismatch %d <= %d\n", remote->index,
		    start_index);
	    vmtd_free_vmtd (vmtd);
	    diag = -EIO;
	    break;
	}
	{
	    vmtd_vmtd_t* vmtd2;

	    list_for_each_entry (vmtd2, &vmtd_vmtds, list) {
		if (vmtd2->link == link && !vmtd2->deleted &&
		    vmtd2->remote_mtd.index == remote->index) break;
	    }
	    vmtd_release_mtds (link, last_start_index + 1, remote->index - 1);
	    if (&vmtd2->list != &vmtd_vmtds) {
		    /* Already have this MTD */
		vmtd_free_vmtd (vmtd);
		start_index = last_start_index = remote->index;
		continue;
	    }
	}
	    /* MTD is new */
	if ((diag = vmtd_init_mtd_info (vmtd)) != 0) {
	    vmtd_free_vmtd (vmtd);
	    goto error;
	}
	list_add (&vmtd->list, &vmtd_vmtds);
	start_index = last_start_index = remote->index;
    }
    return 0;

error:
    vmtd_release_mtds (link, 0, VMTD_MAX_INDEX);
    return diag;
}

/*----- Module thread -----*/

static struct semaphore	vmtd_sem;
static _Bool		vmtd_thread_aborted;
static _Bool		vmtd_sysconf;
static _Bool		vmtd_erases_finished;
static _Bool		vmtd_retry_deletions;
static _Bool		vmtd_mtd_changes;

    static void
vmtd_thread_aborted_notify (void)
{
    vmtd_thread_aborted = true;
    up (&vmtd_sem);
}

    static void
vmtd_sysconf_notify (vmq_links_t* links)
{
    (void) links;
    vmtd_sysconf = true;
    up (&vmtd_sem);
}

    static void
vmtd_erases_finished_notify (void)
{
    vmtd_erases_finished = true;
    up (&vmtd_sem);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,17)
    static void
vmtd_retry_deletions_notify (void)
{
    vmtd_retry_deletions = true;
    up (&vmtd_sem);
}
#endif

    static void
vmtd_mtd_changes_notify (vmq_link_t* link)
{
    VMTD_FE_LINK (link)->mtd_changes = true;
    vmtd_mtd_changes = true;
    up (&vmtd_sem);
}

    static _Bool
vmtd_link_mtd_changes (vmq_link_t* link, void* unused_cookie)
{
    (void) unused_cookie;
    if (VMTD_FE_LINK (link)->mtd_changes) {
	VMTD_FE_LINK (link)->mtd_changes = false;
	vmtd_acquire_mtds_from (link);
    }
    return false;
}

    static void
vmtd_return_notify (vmq_link_t* link)
{
    void*  msg;

    while (!vmq_return_msg_receive (link, &msg)) {
	vmtd_process_reply (link, (NkDevMtdMsg*) msg);
    }
}

    static void
vmtd_receive_notify (vmq_link_t* link)
{
    void*  msg;

    while (!vmq_msg_receive (link, &msg)) {
	vmtd_process_async (link, msg);
    }
}

    static void
vmtd_link_on (vmq_link_t* link)
{
    DTRACE ("link on (local <-> OS %d).\n", vmq_peer_osid (link));
    vmtd_acquire_mtds_from (link);
}

    static int
vmtd_thread (void* arg)
{
    DTRACE ("\n");
    while (!vmtd_thread_aborted) {
	down (&vmtd_sem);
	DTRACE ("%s %s %s %s %s\n",
		vmtd_thread_aborted  ? "thread-aborted"  : "",
		vmtd_sysconf         ? "sysconf"         : "",
		vmtd_erases_finished ? "erases-finished" : "",
		vmtd_retry_deletions ? "retry-deletions" : "",
		vmtd_mtd_changes     ? "mtd-changes"     : "");
	if (vmtd_thread_aborted) break;
	if (vmtd_sysconf) {
	    vmtd_sysconf = false;
	    vmq_links_sysconf (arg);
	}
	if (vmtd_erases_finished) {
	    vmtd_erases_finished = false;
	    vmtd_erases_all_links (arg);
	}
	if (vmtd_retry_deletions) {
	    vmtd_retry_deletions = false;
	    vmtd_release_deleted();
	}
	if (vmtd_mtd_changes) {
	    vmtd_mtd_changes = false;
	    vmq_links_iterate (arg, vmtd_link_mtd_changes, NULL);
	}
    }
    return 0;
}

/*----- Support for /proc/nk/vmtd-fe -----*/

static const char* vmtd_type_names[] = {NK_DEV_MTD_NAMES};

    static int
vmtd_read_proc (char* page, char** start, off_t off, int count, int* eof,
		void* data)
{
    off_t		begin = 0;
    int			len;
    const vmtd_vmtd_t*	vmtd;

    len = sprintf (page,
	"De OS:Id Typ   Size   Erase Flags    CachOffs U  Funcs  Name\n");

    list_for_each_entry (vmtd, &vmtd_vmtds, list) {
	len += sprintf (page+len,
	    "%2d %2d:%-2d %3.3s %8llx %5x %c%c%c%c%c%c%c%c %8x %d %7llx %s\n",
	    vmtd->local_mtd.index,
	    vmq_peer_osid (vmtd->link),
	    vmtd->remote_mtd.index,
	    vmtd_type_names [vmtd->remote_mtd.type],
	    (unsigned long long) vmtd->local_mtd.size,
	    vmtd->local_mtd.erasesize,
	    vmtd->added       ? 'A' : '.',
	    vmtd->deleted     ? 'D' : '.',
	    vmtd->cache_valid ? 'C' : '.',
	    vmtd->remote_mtd.flags & NK_DEV_MTD_WRITEABLE     ? 'W' : '.',
	    vmtd->remote_mtd.flags & NK_DEV_MTD_BIT_WRITEABLE ? 'B' : '.',
	    vmtd->remote_mtd.flags & NK_DEV_MTD_NO_ERASE      ? '.' : 'E',
	    vmtd->implicit_get_device                         ? 'G' : '.',
	    VMTD_TOO_MANY_READ_OOB_MANY_FAILURES (vmtd)       ? 'F' : '.',
	    vmtd->cache_offset,
	    vmtd->local_mtd.usecount,
	    vmtd->remote_mtd.available_functions,
	    vmtd->local_mtd.name);

	if (data) {
	    const struct mtd_info* mtd = &vmtd->local_mtd;
	    _Bool had_any = false;
	    unsigned i;

#define VMTD_ELEMS(a)	(sizeof (a) / sizeof (a) [0])

	    if (mtd->ecclayout) {
		const struct nand_ecclayout* ecc = mtd->ecclayout;
		_Bool had_oobfree = false;

		len += sprintf (page+len, "   OOB: eccbytes %d oobavail %d\n",
				ecc->eccbytes, ecc->oobavail);
		len += sprintf (page+len, "   OOB: eccpos");
		for (i = 0; i < VMTD_ELEMS (ecc->eccpos); ++i) {
		    if (ecc->eccpos [i] || !i) {
			len += sprintf (page+len, " %u:%u", i,
					ecc->eccpos [i]);
		    }
		}
		len += sprintf (page+len, "\n");
		for (i = 0; i < VMTD_ELEMS (ecc->oobfree); ++i) {
		    const struct nand_oobfree* f = ecc->oobfree + i;

		    if (f->length) {
			if (!had_oobfree) {
			    had_oobfree = true;
			    len += sprintf (page+len,
					    "   OOB: free (offs:len)");
			}
			len += sprintf (page+len, " %d:%d", f->offset,
					f->length);
		    }
		}
		if (had_oobfree) {
		    len += sprintf (page+len, "\n");
		}
	    }
	    for (i = 0; i < NK_DEV_MTD_FUNC_MAX; ++i) {
		if (!vmtd->remote_calls [i] && !vmtd->local_calls [i])
		    continue;
		if (!had_any) {
		    len += sprintf (page+len, "  ");
		}
		if (NK_DEV_MTD_FUNC_IS_STANDARD (i)) {
		    len += sprintf (page+len, " %s:%d:%d", vmtd_func_names [i],
				    vmtd->local_calls [i],
				    vmtd->remote_calls [i]);
		} else {
		    len += sprintf (page+len, " %s:%d", vmtd_func_names [i],
				    vmtd->remote_calls [i]);
		}
		had_any = true;
	    }
	    if (had_any) {
		len += sprintf (page+len, "\n");
	    }
	}
	if (len + begin > off + count)
	    goto done;
	if (len + begin < off) {
	    begin += len;
	    len = 0;
	}
    }
    *eof = 1;

done:
    if (off >= len+begin) return 0;
    *start = page + off - begin;
    return (count < begin + len - off ? count : begin + len - off);
}

/*----- Initialization and exit entry points -----*/

#define VLX_SERVICES_THREADS
#include "vlx-services.c"

static struct proc_dir_entry*	vmtd_proc;
static struct proc_dir_entry*	vmtd_proc_ext;
static vmq_links_t*		vmtd_links;
static vlx_thread_t		vmtd_thread_desc;

    static _Bool
vmtd_free_link (vmq_link_t* link, void* cookie)
{
    (void) cookie;
    kfree (VMTD_FE_LINK (link));
    return false;
}

    static void
vmtd_exit (void)
{
    DTRACE ("\n");
    if (vmtd_links) {
	vmq_links_abort (vmtd_links);
    }
    vmtd_thread_aborted_notify();
    vlx_thread_join (&vmtd_thread_desc);
    vmtd_release_mtds (NULL, 0, VMTD_MAX_INDEX);
    if (vmtd_links) {
	vmq_links_iterate (vmtd_links, vmtd_free_link, NULL);
	vmq_links_finish (vmtd_links);
	vmtd_links = NULL;
    }
    if (vmtd_proc)     remove_proc_entry ("nk/vmtd-fe",     NULL);
    if (vmtd_proc_ext) remove_proc_entry ("nk/vmtd-fe.ext", NULL);
}

    static _Bool
vmtd_init_link (vmq_link_t* link, void* cookie)
{
    vmtd_fe_link_t* fe_link = kzalloc (sizeof *fe_link, GFP_KERNEL);

    if (!fe_link) {
	*(int*) cookie = -ENOMEM;
	return true;
    }
    VMTD_FE_LINK (link) = fe_link;
    vipc_list_init (&fe_link->vei_list);
    vipc_ctx_init (&fe_link->vipc_ctx, link);
    return false;
}

#define VMTD_FIELD(name,value)	value

static const vmq_callbacks_t vmtd_callbacks = {
    VMTD_FIELD (link_on,		vmtd_link_on),
    VMTD_FIELD (link_off,		vmtd_link_off),
    VMTD_FIELD (link_off_completed,	vmtd_link_off_completed),
    VMTD_FIELD (sysconf_notify,		vmtd_sysconf_notify),
    VMTD_FIELD (receive_notify,		vmtd_receive_notify),
    VMTD_FIELD (return_notify,		vmtd_return_notify)
};

static const vmq_xx_config_t vmtd_tx_config = {
    VMTD_FIELD (msg_count,	16),
    VMTD_FIELD (msg_max,	sizeof (NkDevMtdMsg)),
    VMTD_FIELD (data_count,	16),
    VMTD_FIELD (data_max,	8 * (512 + 16))
};

static const vmq_xx_config_t vmtd_rx_config = {
    VMTD_FIELD (msg_count,	8),
    VMTD_FIELD (msg_max,	sizeof (NkDevMtdMsg)),
    VMTD_FIELD (data_count,	0),
    VMTD_FIELD (data_max,	0)
};

#undef VMTD_FIELD

    static int __init
vmtd_init (void)
{
    signed diag;

    DTRACE ("\n");
    sema_init (&vmtd_sem, 0);	/* Before it is signaled */
    vmtd_proc     = create_proc_read_entry ("nk/vmtd-fe",     0, NULL,
					    vmtd_read_proc, NULL);
    vmtd_proc_ext = create_proc_read_entry ("nk/vmtd-fe.ext", 0, NULL,
					    vmtd_read_proc, (void*) 1);
    diag = vmq_links_init_ex (&vmtd_links, "vmtd", &vmtd_callbacks,
			      &vmtd_tx_config, &vmtd_rx_config, NULL, true);
    if (diag) goto error;
    if (vmq_links_iterate (vmtd_links, vmtd_init_link, &diag)) goto error;
    diag = vmq_links_start (vmtd_links);
    if (diag) goto error;
    diag = vlx_thread_start (&vmtd_thread_desc, vmtd_thread, vmtd_links,
    			     "vmtd-fe");
    if (diag) goto error;
    TRACE ("initialized\n");
    return 0;

error:
    ETRACE ("init failed (%d)\n", diag);
    vmtd_exit();
    return diag;
}

    /*
     *  module_init(vmtd_init) executes too early, before mtdcore.c
     *  had time to call class_register() for the mtd_class used
     *  during mtd_device_register(), hence crash.
     */
late_initcall (vmtd_init);
module_exit (vmtd_exit);

/*----- Module description -----*/

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_DESCRIPTION ("VLX Virtual MTD frontend driver");

/*----- End of file -----*/

/*
 ****************************************************************
 *
 *  Component: VLX VMTD-BE
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
#include <asm/io.h>		/* virt_to_phys */
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

    /* Kernels 2.6.35 and above replaced MAX_MTD_DEVICES with idr.h */
#ifndef MAX_MTD_DEVICES
#define MAX_MTD_DEVICES	32
#endif

/*----- Local header files -----*/

#include "vlx-vmq.h"

/*----- Tracing -----*/

#ifdef VMTD_DEBUG
#define DTRACE(x...)	do {printk ("%s: ", __func__); printk (x);} while (0)
#else
#define DTRACE(x...)
#endif

#define OTRACE(x...)
#define TRACE(x...)	printk (KERN_NOTICE "VMTD-BE: " x)
#define WTRACE(x...)	printk (KERN_WARNING "VMTD-BE: " x)
#define ETRACE(x...)	printk (KERN_ERR "VMTD-BE: " x)

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
#endif

#define list_for_each_entry_type(pos, type, head, member) \
	list_for_each_entry(pos, head, member)

/*----- Pointed regions -----*/

typedef struct vmtd_pointed_t vmtd_pointed_t;

struct vmtd_pointed_t {
    struct list_head	list;
    struct mtd_info*	mtd;
    void*		virt;
    unsigned long	phys;
    loff_t		from;
    size_t		len;
    NkOsId		osid;
};

static LIST_HEAD (vmtd_pointed_head);

    static signed
vmtd_pointed_add (struct mtd_info* mtd, void* virt, const unsigned long phys,
		  const loff_t from, const size_t len, const NkOsId osid)
{
    vmtd_pointed_t* p = kzalloc (sizeof *p, GFP_KERNEL);
    if (unlikely (!p)) return -ENOMEM;

    INIT_LIST_HEAD (&p->list);
    p->mtd  = mtd;
    p->virt = virt;
    p->phys = phys;
    p->from = from;
    p->len  = len;
    p->osid = osid;

    list_add (&p->list, &vmtd_pointed_head);

    return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION (2,6,27)
#define VMTD_UNPOINT(mtd,virt,from,len) do {\
	DTRACE ("unpoint mtd %d virt %p [%llx+%x]\n", mtd->index, virt, \
		(long long) from, len); \
	mtd->unpoint ((mtd), (virt), (from), (len));} while (0)
#else
#define VMTD_UNPOINT(mtd,virt,from,len) do { \
	DTRACE ("unpoint mtd %d virt %p [%llx+%x]\n", mtd->index, virt, \
		(long long) from, len); \
	mtd->unpoint ((mtd), (from), (len));} while (0)
#endif

    static void
vmtd_pointed_free (struct mtd_info* mtd, const NkOsId osid)
{
    vmtd_pointed_t* p;

restart:
    list_for_each_entry_type (p, vmtd_pointed_t, &vmtd_pointed_head, list) {
	if (p->mtd != mtd) continue;
	if (p->osid != osid) continue;
	VMTD_UNPOINT (mtd, p->virt, p->from, p->len);
	list_del (&p->list);
	kfree (p);
	goto restart;
    }
}

    /* "phys" does not need to be provided (recent kernels) */

    static signed
vmtd_unpoint (struct mtd_info* mtd, unsigned long phys, const loff_t from,
	      const size_t len)
{
    vmtd_pointed_t* p;

    list_for_each_entry_type (p, vmtd_pointed_t, &vmtd_pointed_head, list) {
	if (p->mtd != mtd) continue;
	if (phys && p->phys != phys) continue;
	if (p->from != from || p->len != len) continue;

	VMTD_UNPOINT (mtd, p->virt, p->from, p->len);
	list_del (&p->list);
	kfree (p);
	return 0;
    }
    ETRACE ("mtd%d: did not find pointed region at %lx [%llx+%x]\n",
	mtd->index, phys, from, len);
    return -EINVAL;
}

/*----- Data types -----*/

typedef struct vmtd_vmtd_t {
    struct mtd_info*	mtd;
    NkOsMask		users;
    unsigned		calls [NK_DEV_MTD_FUNC_MAX];
} vmtd_vmtd_t;

static vmtd_vmtd_t* vmtd_vmtds [MAX_MTD_DEVICES];

/*----- Interface with vlx-vmq.c -----*/

    static void
vmtd_link_off (vmq_link_t* link)
{
    const NkOsId osid = vmq_peer_osid (link);
    const NkOsMask mask = 1 << osid;
    unsigned mtd_index;

    DTRACE ("\n");
    for (mtd_index = 0; mtd_index < MAX_MTD_DEVICES; ++mtd_index) {
	vmtd_vmtd_t* vmtd = vmtd_vmtds [mtd_index];

	if (!vmtd) continue;
	if (!(vmtd->users & mask)) continue;

	vmtd_pointed_free (vmtd->mtd, osid);
	vmtd->users &= ~mask;
	if (!vmtd->users) {
	    put_mtd_device (vmtd->mtd);	/* void */
	}
    }
}

/*----- Processing of frontend requests -----*/

    static signed
vmtd_next (signed mtd_index)
{
    mtd_index = mtd_index < 0 ? 0 : mtd_index + 1;
    for (; mtd_index < MAX_MTD_DEVICES; ++mtd_index) {
	if (vmtd_vmtds [mtd_index]) return mtd_index;
    }
    return -1;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION (2,6,30)
    static unsigned
vmtd_get_shift (unsigned value)
{
    unsigned bitnb;	/* "bit" is a keyword in RVDS */

    for (bitnb = 0; bitnb < 32; ++bitnb) {
	if (value == (1U << bitnb)) return bitnb;
    }
    return 0;
}
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
typedef char vmtd_check1_t [sizeof (struct nand_oobfree) ==
			    sizeof (NkDevMtdNandOobFree) ? 1 : -1];
typedef char vmtd_check2_t [sizeof (struct nand_ecclayout) ==
			    sizeof (NkDevMtdNandEccLayout) ? 1 : -1];
#endif

    static void
vmtd_init_mtd (NkDevMtd* remote, const struct mtd_info* mtd)
{
    DTRACE ("mtd %d type %d name '%s'\n", mtd->index, mtd->type, mtd->name);

    memset (remote, 0, sizeof *remote);

#define VMTD_FIELD(x)	remote->x = mtd->x

    remote->version = sizeof *remote;
    VMTD_FIELD (type);
    VMTD_FIELD (flags);
    VMTD_FIELD (size);
    VMTD_FIELD (erasesize);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
    VMTD_FIELD (writesize);
#else
    remote->writesize = mtd->oobblock;
#endif
    VMTD_FIELD (oobsize);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
    VMTD_FIELD (oobavail);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,30)
    VMTD_FIELD (erasesize_shift);
    VMTD_FIELD (writesize_shift);
    VMTD_FIELD (erasesize_mask);
    VMTD_FIELD (writesize_mask);
#else
    remote->erasesize_shift = vmtd_get_shift (mtd->erasesize);
    remote->writesize_shift = vmtd_get_shift (remote->writesize);
    remote->erasesize_mask = (1 << remote->erasesize_shift) - 1;
    remote->writesize_mask = (1 << remote->writesize_shift) - 1;
#endif

    strncpy ((char*) remote->name, mtd->name, sizeof remote->name);
    remote->name [sizeof remote->name - 1] = '\0';

    VMTD_FIELD (index);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
    if (mtd->ecclayout) {
	remote->ecclayoutexists = 1;
	memcpy (&remote->ecclayout, mtd->ecclayout, sizeof remote->ecclayout);
    } else {
	remote->ecclayoutexists = 0;
    }
#endif

    VMTD_FIELD (numeraseregions);

#define VMTD_FUNC(f,n) \
    if (mtd->f) remote->available_functions |= 1 << NK_DEV_MTD_FUNC_##n

    remote->available_functions = 0;

    VMTD_FUNC (erase,               ERASE);
    VMTD_FUNC (point,               POINT);
    VMTD_FUNC (unpoint,             UNPOINT);
    VMTD_FUNC (read,                READ);
    VMTD_FUNC (write,               WRITE);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
    VMTD_FUNC (panic_write,         PANIC_WRITE);
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
    VMTD_FUNC (read_oob,            READ_OOB);
    VMTD_FUNC (write_oob,           WRITE_OOB);
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
    VMTD_FUNC (get_fact_prot_info,  GET_FACT_PROT_INFO);
#endif
    VMTD_FUNC (read_fact_prot_reg,  READ_FACT_PROT_REG);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
    VMTD_FUNC (get_user_prot_info,  GET_USER_PROT_INFO);
#endif
    VMTD_FUNC (read_user_prot_reg,  READ_USER_PROT_REG);
    VMTD_FUNC (write_user_prot_reg, WRITE_USER_PROT_REG);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
    VMTD_FUNC (lock_user_prot_reg,  LOCK_USER_PROT_REG);
#endif
    VMTD_FUNC (writev,              WRITEV);	/* Will not use it actually */
    VMTD_FUNC (sync,                SYNC);
    VMTD_FUNC (lock,                LOCK);
    VMTD_FUNC (unlock,              UNLOCK);
    VMTD_FUNC (suspend,             SUSPEND);
    VMTD_FUNC (resume,              RESUME);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
    VMTD_FUNC (block_isbad,         BLOCK_ISBAD);
    VMTD_FUNC (block_markbad,       BLOCK_MARKBAD);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,30)
    VMTD_FUNC (get_unmapped_area,   GET_UNMAPPED_AREA);
#endif

    /* mtd->reboot_notifier */

	/* XXX: we need to sync this with frontend better */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
    VMTD_FIELD (ecc_stats.corrected);
    VMTD_FIELD (ecc_stats.failed);
    VMTD_FIELD (ecc_stats.badblocks);
    VMTD_FIELD (ecc_stats.bbtblocks);
    VMTD_FIELD (subpage_sft);
#endif
}

#undef VMTD_FIELD

typedef struct {
    NkDevMtdGetNextMtd		mtd_desc;
    NkDevMtd			mtd;
    NkDevMtdGetNextMtd		erase_region_info_desc;
    NkDevMtdEraseRegionInfo	erase_region_info [1];
} NkDevMtdGetNextMtdReply;

    static unsigned
vmtd_get_next_mtd_reply (void* msg, unsigned space, const struct mtd_info* mtd)
{
    const unsigned erase_size = sizeof (NkDevMtdEraseRegionInfo) *
				mtd->numeraseregions;
    const unsigned total_size = erase_size ? sizeof (NkDevMtdGetNextMtdReply) -
				sizeof (NkDevMtdEraseRegionInfo) + erase_size :
				sizeof (NkDevMtdGetNextMtd) + sizeof(NkDevMtd);
    NkDevMtdGetNextMtdReply* reply = msg;
    unsigned i;

    if (space < total_size) {
	DTRACE ("Reply buffer is too small, %d < %d\n", space, total_size);
	return 0;
    }
    reply->mtd_desc.type = NK_DEV_MTD_FIELD_MTD;
    reply->mtd_desc.len  = sizeof (NkDevMtd);

    vmtd_init_mtd (&reply->mtd, mtd);

    if (erase_size) {
	reply->erase_region_info_desc.type = NK_DEV_MTD_FIELD_ERASE_REGION_INFO;
	reply->erase_region_info_desc.len  = erase_size;

#define VMTD_FIELD(field) \
	reply->erase_region_info [i].field = mtd->eraseregions [i].field

	for (i = 0; i < (unsigned) mtd->numeraseregions; ++i) {
	    VMTD_FIELD (offset);
	    VMTD_FIELD (erasesize);
	    VMTD_FIELD (numblocks);
	}
    }
#undef VMTD_FIELD

    return total_size;
}

    /* Can sleep for place in FIFO */

    static NkDevMtdReply*
vmtd_alloc_async (vmq_link_t* link, unsigned function, unsigned retcode)
{
    NkDevMtdMsg*	msg;

    if (unlikely (vmq_msg_allocate (link, 0, (void**) &msg, NULL)))
	return NULL;
    OTRACE ("msg %p retcode %d\n", msg, retcode);
    DTRACE ("retcode %d\n", retcode);
    msg->reply.function = function;
    msg->reply.retcode  = retcode;
    msg->reply.cookie   = 0;	/* Async msgs have null cookie */
    msg->reply.flags    = NK_DEV_MTD_FLAGS_NOTIFICATION;
    return &msg->reply;
}

typedef struct {
	/* Must be initialized */
    struct erase_info	ei;
    vmq_link_t*		link;
    nku64_f		erase_cookie;
} vmtd_erase_info_t;

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,7)
    /* fail_addr is 32 or 64 bits depending on version */
#define VMTD_EI_FAIL_ADDR(ei)	(ei)->fail_addr
#else
#define VMTD_EI_FAIL_ADDR(ei)	(-1)
#endif

    /*
     *  ei->fail_addr can be 32 bits, while in the protocol it is
     *  always 64 bits, so we must convert a 32 bit error into
     *  a 64 bit error.
     */

    static inline void
vmtd_set_fail_addr (NkDevMtdReply* reply, nku64_f fail_addr)
{
    if (fail_addr == (nku32_f) -1) {
	reply->addr = -1ULL;
    } else {
	reply->addr = fail_addr;
    }
}

    static void
vmtd_erase_callback (struct erase_info* ei)
{
    vmtd_erase_info_t*	vei = container_of (ei, vmtd_erase_info_t, ei);
    NkDevMtdReply*	async;

	/* fail_addr is 32 or 64 bits depending on version */
    DTRACE ("state %d fail_addr %llx\n", ei->state,
	    (long long) VMTD_EI_FAIL_ADDR (ei));
    async = vmtd_alloc_async (vei->link, NK_DEV_MTD_FUNC_ERASE_FINISHED,
			      ei->state);
    if (async) {
	vmtd_set_fail_addr (async, VMTD_EI_FAIL_ADDR (ei));
	async->erase_cookie = vei->erase_cookie;
	vmq_msg_send (vei->link, async);
    }
    kfree (vei);
}

static const char* vmtd_func_names[] = {NK_DEV_MTD_FUNC_NAMES};

static unsigned vmtd_changes;

    static inline void
vmtd_reply (vmq_link_t* link, signed retcode, NkDevMtdRequest* req)
{
    NkDevMtdReply* reply = (NkDevMtdReply*) req;

    OTRACE ("msg %p retcode %d cookie %llx\n", req, retcode, req->cookie);
    DTRACE ("retcode %d cookie %llx\n", retcode, req->cookie);
    reply->retcode = (unsigned) retcode;
    reply->flags   = NK_DEV_MTD_FLAGS_REPLY;
    vmq_msg_return (link, reply);
}

static _Bool vmtd_allow_point = false;	/* Mappings do not work yet */
static _Bool vmtd_thread_aborted;

typedef struct vmtd_be_link_t {
    unsigned		changes;
    NkOsMask		mtd_visible;
    char*		mtd_names;
    unsigned		fe_version;
} vmtd_be_link_t;

#define VMTD_BE_LINK(link) \
	((vmtd_be_link_t*) ((vmq_link_public*) (link))->priv)
#undef VMTD_BE_LINK
#define VMTD_BE_LINK(link) (*(vmtd_be_link_t**) (link))

    static _Bool
vmtd_mtd_allowed (vmq_link_t* link, const struct mtd_info* mtd)
{
    const char* mtd_names = VMTD_BE_LINK (link)->mtd_names;

    if (VMTD_BE_LINK (link)->mtd_visible & (1 << mtd->index)) {
	DTRACE ("mtd %d (%s) allowed by number\n", mtd->index, mtd->name);
	return true;
    }
    while (mtd_names && *mtd_names) {
	if (!strcmp (mtd->name, mtd_names)) {
	    DTRACE ("mtd %d (%s) allowed by name\n", mtd->index, mtd->name);
	    return true;
	}
	mtd_names += strlen (mtd_names) + 1;
    }
    DTRACE ("mtd %d (%s) not allowed\n", mtd->index, mtd->name);
    return false;
}

    /* Frees the msg */

    static void
vmtd_process_msg (vmq_link_t* link, NkDevMtdMsg* msg, char* data_area,
		  NkOsId osid)
{
    NkDevMtdRequest*	req = (NkDevMtdRequest*) msg;
    const NkOsMask	mask = 1 << osid;
    unsigned		data_offset = req->dataOffset;
    vmtd_vmtd_t*	vmtd;
    struct mtd_info*	mtd;

	/* Just stopping processing messages will soon block sender */
    if (vmtd_thread_aborted) return;
    DTRACE ("mtd %d func %d (%s) dataOffset %x offs %llx size %llx\n",
	    req->index, req->function,
	    vmtd_func_names [req->function % NK_DEV_MTD_FUNC_MAX],
	    data_offset, req->offset, req->size);

    if (req->flags != NK_DEV_MTD_FLAGS_REQUEST) {
	DTRACE ("Ignoring request with invalid flags %x\n", req->flags);
	vmq_msg_free (link, req);
	return;
    }
    if (!vmq_data_offset_ok (link, data_offset)) {
	DTRACE ("Ignoring request with invalid dataOffset %x\n", data_offset);
	vmq_msg_free (link, req);
	return;
    }
    if (req->function == NK_DEV_MTD_FUNC_GET_NEXT_MTD) {
	req->index = vmtd_next (req->index);
    }
    if (req->index >= MAX_MTD_DEVICES || !vmtd_vmtds [req->index]) {
	DTRACE ("mtd %d invalid\n", req->index);
	vmtd_reply (link, -ENODEV, req);
	return;
    }
    vmtd = vmtd_vmtds [req->index];
    mtd  = vmtd->mtd;
    ++vmtd->calls [req->function % NK_DEV_MTD_FUNC_MAX];

    if (!(vmtd->users & mask) && req->function != NK_DEV_MTD_FUNC_GET_DEVICE &&
			     req->function != NK_DEV_MTD_FUNC_GET_NEXT_MTD) {
	DTRACE ("mtd %d func %d users %x invalid request\n", req->index,
		req->function, vmtd->users);
	vmtd_reply (link, -EINVAL, req);
	return;
    }

    switch (req->function) {
    case NK_DEV_MTD_FUNC_ERASE: {
	vmtd_erase_info_t*	vei = kzalloc (sizeof *vei, GFP_KERNEL);
	NkDevMtdReply*		reply = (NkDevMtdReply*) req;
	signed			diag;

	if (unlikely (!vei)) {
	    vmtd_reply (link, -ENOMEM, req);
	    return;
	}
	    /* No "list" member to init in this vei */
	vei->ei.mtd      = mtd;
	vei->ei.callback = vmtd_erase_callback;
	vei->ei.addr     = req->offset;
	vei->ei.len      = req->size;
	vei->ei.state    = MTD_ERASE_PENDING;
	vei->link	 = link;
	vei->erase_cookie = req->addr;

	    /* Callback/async-msg is only called/sent if diag=0 */
	diag = mtd->erase (mtd, &vei->ei);
	if (diag) {
	    vmtd_set_fail_addr (reply, VMTD_EI_FAIL_ADDR (&vei->ei));
	    kfree (vei);
	}
	vmtd_reply (link, diag, req);
	break;
    }
    case NK_DEV_MTD_FUNC_POINT: {
	size_t		retlen;
#if LINUX_VERSION_CODE < KERNEL_VERSION (2,6,27)
	u_char*		buf;
#else
	void*		buf;
#endif
	NkDevMtdReply*	reply = (NkDevMtdReply*) req;
	signed		diag;

	if (!vmtd_allow_point) {
	    DTRACE ("mtd %d point disallowed\n", req->index);
	    vmtd_reply (link, -EINVAL, req);
	    break;
	}
	diag = mtd->point (mtd, req->offset, req->size,
	    &retlen, &buf
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
	    , NULL /*resource_size_t *phys */
#endif
	    );
	reply->retsize = retlen;
	if (!diag) {
	    reply->addr = virt_to_phys (buf);
	    DTRACE ("pointed virt %p phys %llx\n", buf, reply->addr);
	    diag = vmtd_pointed_add (mtd, buf, reply->addr, req->offset,
				     retlen, osid);
	    if (diag) {
		VMTD_UNPOINT (mtd, buf, req->offset, retlen);
	    }
	}
	vmtd_reply (link, diag, req);
	break;
    }
    case NK_DEV_MTD_FUNC_UNPOINT: {
	vmtd_reply (link, vmtd_unpoint (mtd, req->addr, req->offset,
		    req->size), req);
	break;
    }
    case NK_DEV_MTD_FUNC_READ: {
	signed diag;
	size_t retlen;
	NkDevMtdReply* reply = (NkDevMtdReply*) req;

	diag = mtd->read (mtd, req->offset, req->size, &retlen,
			  (unsigned char*) (data_area + data_offset));
	reply->retsize = retlen;
	vmtd_reply (link, diag, req);
	break;
    }
    case NK_DEV_MTD_FUNC_WRITE:
    case NK_DEV_MTD_FUNC_PANIC_WRITE: {
	signed		diag;
	size_t		retlen;
	NkDevMtdReply*	reply = (NkDevMtdReply*) req;

	DTRACE ("%s: mtd %d offset %llx size %llx\n",
		vmtd_func_names [req->function], mtd->index,
		req->offset, req->size);
	if (req->function == NK_DEV_MTD_FUNC_WRITE) {
	    diag = mtd->write (mtd, req->offset, req->size, &retlen,
			       (unsigned char*) (data_area + data_offset));
	} else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
	    diag = mtd->panic_write (mtd, req->offset, req->size, &retlen,
				     data_area + data_offset);
#else
	    diag = -ENOSYS;
#endif
	}
	DTRACE ("%s: diag %d retlen %d\n", vmtd_func_names [req->function],
		diag, retlen);
	reply->retsize = retlen;
	vmtd_reply (link, diag, req);
	break;
    }
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
    case NK_DEV_MTD_FUNC_READ_OOB:
    case NK_DEV_MTD_FUNC_WRITE_OOB: {
	struct mtd_oob_ops ops;
	signed		diag;
	NkDevMtdReply*	reply = (NkDevMtdReply*) req;

	    /* Data and OOB buffers follow each other in shared memory */
	ops.mode    = (mtd_oob_mode_t) req->mode;
	    /* from/to = req->offset */
	ops.ooboffs = req->offset2;
	ops.len     = req->size;
	ops.ooblen  = req->size2;
	    /*
	     *  If we do not pass a NULL oob.datbuf pointer when oob.len
	     *  is 0, then only one OOB is going to be read when jffs2
	     *  requests for example four of them.
	     */
	ops.datbuf  = ops.len ? (nku8_f*) (data_area + data_offset) : NULL;
	if (ops.ooblen) {
	    data_offset += req->size;
	    if (!vmq_data_offset_ok (link, data_offset)) {
		DTRACE ("Ignoring request with invalid size %llx\n",
			(long long) req->size);
		vmq_msg_free (link, req);
		break;
	    }
	    ops.oobbuf = (nku8_f*) (data_area + data_offset);
	} else {
	    ops.oobbuf = NULL;
	}
	DTRACE ("mode %d from %llx ooboffs %x len %x ooblen %x\n", ops.mode,
		req->offset, ops.ooboffs, ops.len, ops.ooblen);

	if (req->function == NK_DEV_MTD_FUNC_READ_OOB) {
	    diag = mtd->read_oob (mtd, req->offset, &ops);
	} else {
	    diag = mtd->write_oob (mtd, req->offset, &ops);
	}
	DTRACE ("retlen %x oobretlen %x\n", ops.retlen, ops.oobretlen);
	    /*
	     *  Some drivers set ops.retlen even though ops.datbuf and
	     *  ops.len were NULL/0, so make sure the frontend gets a
	     *  clean/coherent response.
	     */
	reply->retsize  = ops.datbuf ? ops.retlen    : 0;
	reply->retsize2 = ops.oobbuf ? ops.oobretlen : 0;
	vmtd_reply (link, diag, req);
	break;
    }
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
	/* We must return the size in bytes or the -ERROR */
    case NK_DEV_MTD_FUNC_GET_FACT_PROT_INFO:
	vmtd_reply (link, mtd->get_fact_prot_info (mtd,
		    (struct otp_info*) (data_area + data_offset),
		    req->size), req);
	break;
#endif

	/* Like READ */
    case NK_DEV_MTD_FUNC_READ_FACT_PROT_REG: {
	signed diag;
	size_t retlen;
	NkDevMtdReply* reply = (NkDevMtdReply*) req;

	diag = mtd->read_fact_prot_reg (mtd, req->offset, req->size, &retlen,
				        data_area + data_offset);
	reply->retsize = retlen;
	vmtd_reply (link, diag, req);
	break;
    }
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
	/* Identical to GET_FACT_PROT_INFO, except for function */
    case NK_DEV_MTD_FUNC_GET_USER_PROT_INFO:
	vmtd_reply (link, mtd->get_user_prot_info (mtd,
		    (struct otp_info*) (data_area + data_offset),
		    req->size), req);
	break;
#endif

	/* Like READ */
    case NK_DEV_MTD_FUNC_READ_USER_PROT_REG: {
	signed diag;
	size_t retlen;
	NkDevMtdReply* reply = (NkDevMtdReply*) req;

	diag = mtd->read_user_prot_reg (mtd, req->offset, req->size, &retlen,
				        data_area + data_offset);
	reply->retsize = retlen;
	vmtd_reply (link, diag, req);
	break;
    }
	/* Identical to WRITE, except for function code */
    case NK_DEV_MTD_FUNC_WRITE_USER_PROT_REG: {
	signed		diag;
	size_t		retlen;
	NkDevMtdReply*	reply = (NkDevMtdReply*) req;

	DTRACE ("write_user_prot_reg: mtd %d offset %llx size %llx\n",
		mtd->index, req->offset, req->size);
	diag = mtd->write_user_prot_reg (mtd, req->offset, req->size, &retlen,
					 data_area + data_offset);
	DTRACE ("write_user_prot_reg: diag %d retlen %d\n", diag, retlen);
	reply->retsize = retlen;
	vmtd_reply (link, diag, req);
	break;
    }
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
	/* Identical to LOCK, except for function code */
    case NK_DEV_MTD_FUNC_LOCK_USER_PROT_REG:
	vmtd_reply (link, mtd->lock_user_prot_reg (mtd, req->offset,
		    req->size), req);
	break;
#endif

    case NK_DEV_MTD_FUNC_SYNC:
	if (mtd->sync) {
	    mtd->sync (mtd);	/* void */
	    DTRACE ("sync: finished\n");
	}
	vmtd_reply (link, 0, req);
	break;

    case NK_DEV_MTD_FUNC_LOCK:
	vmtd_reply (link, mtd->lock (mtd, req->offset, req->size), req);
	break;

    case NK_DEV_MTD_FUNC_UNLOCK:
	vmtd_reply (link, mtd->unlock (mtd, req->offset, req->size), req);
	break;

    case NK_DEV_MTD_FUNC_SUSPEND:
	vmtd_reply (link, mtd->suspend (mtd), req);
	break;

    case NK_DEV_MTD_FUNC_RESUME:
	mtd->resume (mtd);	/* void */
	vmtd_reply (link, 0, req);
	break;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
    case NK_DEV_MTD_FUNC_BLOCK_ISBAD:
	vmtd_reply (link, mtd->block_isbad (mtd, req->offset), req);
	break;

    case NK_DEV_MTD_FUNC_BLOCK_MARKBAD:
	vmtd_reply (link, mtd->block_markbad (mtd, req->offset), req);
	break;
#endif

    case NK_DEV_MTD_FUNC_GET_DEVICE: {
	signed	diag;

	if (!vmtd_mtd_allowed (link, mtd)) {
	    DTRACE ("mtd %d name %s not allowed for osid %d\n", req->index,
		mtd->name, osid);
	    diag = -ENODEV;
	} else if (get_mtd_device (mtd, req->index) != mtd) {
	    DTRACE ("mtd %d func %d stale device\n", req->index,
		    req->function);
	    diag = -ENODEV;
	} else {
	    vmtd->users |= mask;
	    diag = 0;
	}
	vmtd_reply (link, diag, req);
	break;
    }
    case NK_DEV_MTD_FUNC_PUT_DEVICE:
	vmtd_pointed_free (mtd, osid);
	vmtd->users &= ~mask;
	if (!vmtd->users) {
	    put_mtd_device (mtd);	/* void */
	}
	vmtd_reply (link, 0, req);
	break;

    case NK_DEV_MTD_FUNC_GET_NEXT_MTD: {
	signed		diag;
	NkDevMtdReply*	reply = (NkDevMtdReply*) req;

	if (req->offset2 != 0x300) {
	    ETRACE ("Frontend version 0x%x not supported.\n", req->offset2);
	    vmtd_reply (link, -EIO, req);
	    return;
	}
	VMTD_BE_LINK (link)->fe_version = req->offset2;
	if (!req->index) {
	    VMTD_BE_LINK (link)->changes = vmtd_changes;
	}
	while (!vmtd_mtd_allowed (link, mtd)) {
	    req->index = vmtd_next (req->index);
	    if (req->index >= MAX_MTD_DEVICES) {
		DTRACE ("mtd %d invalid\n", req->index);
		vmtd_reply (link, -ENODEV, req);
		return;
	    }
	    vmtd = vmtd_vmtds [req->index];
	    mtd  = vmtd->mtd;
	}
	if (get_mtd_device (mtd, req->index) != mtd) {
	    diag = -ENODEV;
	} else {
	    reply->retsize = vmtd_get_next_mtd_reply (data_area + data_offset,
						      req->size, mtd);
	    reply->retsize2 = 0x300;	/* version */
	    put_mtd_device (mtd);	/* void */
	    diag = reply->retsize ? 0 : -EFBIG;
	}
	vmtd_reply (link, diag, req);
	break;
    }
    case NK_DEV_MTD_FUNC_BLOCKS_AREBAD: {
	nku32_f badmask = 0;
	unsigned bitnb = 0;
	NkDevMtdReply* reply = (NkDevMtdReply*) req;

	if (req->size > 32) {
	    vmtd_reply (link, -EINVAL, req);
	    return;
	}
	    /* 1=bad, 0=ok, -ERROR=error while checking */
	for (bitnb = 0; req->size > 0; ++bitnb, --req->size,
				     req->offset += mtd->erasesize) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
	    signed diag = mtd->block_isbad (mtd, req->offset);
#else
	    signed diag = 0;
#endif
	    if (diag < 0) {
		if (!bitnb) {
		    vmtd_reply (link, diag, req);
		    return;
		}
		break;
	    }
	    badmask |= !!diag << bitnb;
	}
	reply->retsize  = bitnb;
	reply->retsize2 = badmask;
	vmtd_reply (link, 0, req);
	break;
    }
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
    case NK_DEV_MTD_FUNC_READ_MANY_OOBS: {
	const nku32_f	count = req->size;
	const nku32_f	ooblen = req->size2;
	nku32_f		oobretlen_total = 0;
	nku32_f		oobretlen_prev = 0;
	unsigned	loops;
	NkDevMtdReply*	reply = (NkDevMtdReply*) req;

	if (count && !vmq_data_offset_ok (link,
					  data_offset + ooblen * (count-1))) {
	    DTRACE ("Ignoring request with invalid ooblen %d count %d\n",
		    ooblen, count);
	    vmq_msg_free (link, req);
	    break;
	}
	DTRACE ("mode %d from %llx ooboffs %x ooblen %x count %x\n",
		req->mode, req->offset, req->offset2, ooblen, count);

	for (loops = 0; loops < count; ++loops) {
	    struct mtd_oob_ops	ops;
	    signed		diag;

	    ops.mode    = (mtd_oob_mode_t) req->mode;
		/* from = req->offset */
	    ops.ooboffs = req->offset2;
	    ops.len     = 0;
	    ops.ooblen  = ooblen;
	    ops.datbuf  = NULL;
	    ops.oobbuf  = (nku8_f*) (data_area + data_offset);

	    OTRACE ("mode %d from %llx ooboffs %x len %x ooblen %x\n",
		    ops.mode, req->offset, ops.ooboffs, ops.len, ops.ooblen);

	    diag = mtd->read_oob (mtd, req->offset, &ops);
		/*
		 *  If we error at first read, then we return
		 *  the error itself. If we error at subsequent
		 *  reads, we just return partial data.
		 */
	    if (diag < 0) {
		if (!loops) {
		    vmtd_reply (link, diag, req);
		    return;
		}
		break;
	    }
		/*
		 * Here are the mtd->read_oob() unexpected size
		 * returns which we consider as non-recoverable:
		 *  1) Frontend older than version 0x300.
		 *  2) More bytes returned than what was requested.
		 *  3) Less bytes returned than mtd->oobavail (or
		 *     mtd->ecclayout->oobavail) when asked for a
		 *     count greater than that.
		 *  4) Less bytes returned than ooblen when asked
		 *     for at most mtd->oobavail.
		 *  5) A read returns a different amount of OOB bytes
		 *     than the previous one.
		 *  In all cases, data are padded in returned
		 *  buffer, but the total size returned is the
		 *  actually useful amount of bytes inside.
		 */
	    if (ops.oobretlen != ooblen &&
		(VMTD_BE_LINK (link)->fe_version < 0x300 ||
		ops.oobretlen > ooblen ||
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
		(ooblen >  mtd->oobavail && ops.oobretlen < mtd->oobavail) ||
		(ooblen <= mtd->oobavail && ops.oobretlen < ooblen) ||
#endif
		(oobretlen_prev && ops.oobretlen != oobretlen_prev))) {
		DTRACE ("ops.oobretlen %x ooblen %x fe_version %x "
			"oobretlen_prev %x\n", ops.oobretlen, ooblen,
			VMTD_BE_LINK (link)->fe_version, oobretlen_prev);
		vmtd_reply (link, -EIO, req);
		return;
	    }
	    req->offset     += mtd->writesize;
	    data_offset     += ooblen;
	    oobretlen_total += ops.oobretlen;	/* used to be "+= ooblen" */
	    oobretlen_prev   = ops.oobretlen;
	}
	DTRACE ("loops %d oobretlen_total %x expected %x\n", loops,
		oobretlen_total, loops * ooblen);
	reply->retsize  = loops;
	reply->retsize2 = oobretlen_total;
	vmtd_reply (link, 0, req);
	break;
    }
#endif
    default:
	DTRACE ("mtd %d func %d invalid request\n", req->index, req->function);
	vmtd_reply (link, -EINVAL, req);
	break;
    }
}

/*----- Module thread -----*/

#define VLX_SERVICES_THREADS
#include "vlx-services.c"

static struct semaphore vmtd_sem;
static _Bool		vmtd_sysconf;
static _Bool		vmtd_receive;

    static inline void
vmtd_thread_aborted_notify (void)
{
    vmtd_thread_aborted = true;
    up (&vmtd_sem);
}

    static void
vmtd_sysconf_notify (vmq_links_t* links)
{
    (void) links;
    DTRACE ("\n");
    vmtd_sysconf = true;
    up (&vmtd_sem);
}

    static void
vmtd_receive_notify (vmq_link_t* link)
{
    (void) link;
    OTRACE ("\n");
    vmtd_receive = true;
    up (&vmtd_sem);
}

    static _Bool
vmtd_receive_link (vmq_link_t* link, void* cookie)
{
    NkOsId osid      = vmq_peer_osid    (link);
    char*  data_area = vmq_rx_data_area (link);
    void*  msg;

    (void) cookie;
    while (!vmq_msg_receive (link, &msg)) {
	vmtd_process_msg (link, msg, data_area, osid);
    }
    return false;
}

    static int
vmtd_thread (void* arg)
{
    DTRACE ("\n");
    while (!vmtd_thread_aborted) {
	set_current_state (TASK_INTERRUPTIBLE);
	down (&vmtd_sem);
	if (vmtd_thread_aborted) break;
	if (vmtd_sysconf) {
	    vmtd_sysconf = false;
	    vmq_links_sysconf (arg);
	}
	if (vmtd_receive) {
	    vmtd_receive = false;
	    vmq_links_iterate (arg, vmtd_receive_link, NULL);
	}
    }
    return 0;
}

/*----- Notification of MTD list changes -----*/

    static _Bool
vmtd_signal_link_change (vmq_link_t* link, void* cookie)
{
    DTRACE ("\n");
    (void) cookie;
    if (VMTD_BE_LINK (link)->changes < vmtd_changes) {
	NkDevMtdReply* async;

	async = vmtd_alloc_async (link, NK_DEV_MTD_FUNC_MTD_CHANGES,
				  0 /*diag*/);
	if (async) {
	    vmq_msg_send (link, async);
	}
    }
    return false;
}

static vmq_links_t* vmtd_links;

    static void
vmtd_signal_changes (void)
{
    DTRACE ("\n");
    if (!vmtd_links) return;	/* At register_mtd_user() time */
    vmq_links_iterate (vmtd_links, vmtd_signal_link_change, NULL);
}

    /* mtd_table_mutex is TAKEN here, so get_mtd_device() would deadlock */

    static void
vmtd_notifier_add (struct mtd_info* mtd)
{
    vmtd_vmtd_t* vmtd;

    DTRACE ("%d\n", mtd->index);
    if (mtd->index >= MAX_MTD_DEVICES) {
	WTRACE ("Ignoring device with too high index %d\n", mtd->index);
	return;
    }
    vmtd = kzalloc (sizeof *vmtd, GFP_KERNEL);
    if (unlikely (!vmtd)) {
	WTRACE ("Could not allocate memory for device %d; ignoring it.\n",
		mtd->index);
	return;
    }
	/* No "list" member to init in this vmtd */
    ++vmtd_changes;
    vmtd->mtd = mtd;
    vmtd_vmtds [mtd->index] = vmtd;
    vmtd_signal_changes();
}

    static void
vmtd_notifier_remove (struct mtd_info* mtd)
{
    const unsigned mtd_index = mtd->index;
    vmtd_vmtd_t* vmtd = vmtd_vmtds [mtd_index];

    DTRACE ("%d\n", mtd->index);

    if (!vmtd) return;
    kfree (vmtd);
    vmtd_vmtds [mtd_index] = NULL;
    ++vmtd_changes;
    vmtd_signal_changes();
}

#define VMTD_FIELD(name,value)	value

static struct mtd_notifier vmtd_notifier = {
    VMTD_FIELD (add,	vmtd_notifier_add),
    VMTD_FIELD (remove,	vmtd_notifier_remove)
};

#undef VMTD_FIELD

/*----- Command line options -----*/

#define VMTD_ISDIGIT(ch)	((ch) >= '0' && (ch) <= '9')

    static _Bool
vmtd_have_mtd_names (const char* ch)
{
    for (; *ch && *ch != ';'; ++ch) {
	if (!VMTD_ISDIGIT (*ch) || *ch != ',') {
	    return true;
	}
    }
    return false;
}

    static _Bool
vmtd_is_name (const char* ch)
{
    for (; *ch && *ch != ';' && *ch != ','; ++ch) {
	if (!VMTD_ISDIGIT (*ch)) {
	    return true;
	}
    }
    return false;
}

    static unsigned
vmtd_extract_name (const char* start, char** endp, char* mtd_names,
		   unsigned mtd_names_offs)
{
    const char* ch;
    unsigned len;

    for (ch = start; *ch && *ch != ';' && *ch != ','; ++ch);
    *endp = (char*) ch;
    len = ch - start;
    memcpy (mtd_names + mtd_names_offs, start, len);
    mtd_names [mtd_names_offs + len] = '\0';
    return mtd_names_offs + len + 1;
}

#ifdef VMTD_DEBUG
    static void
vmtd_print_options (const char* mtd_names, NkOsMask mtd_visible)
{
    printk ("visible: %x\n", mtd_visible);
    while (mtd_names && *mtd_names) {
	printk ("name: '%s'\n", mtd_names);
	mtd_names += strlen (mtd_names) + 1;
    }
}
#endif

    static char*
vmtd_parse_options (const char* s_info, NkOsMask* mtd_visible)
{
    char* mtd_names = NULL;
    unsigned mtd_names_used = 0;

    *mtd_visible = 0;
    if (s_info) {
	if (vmtd_have_mtd_names (s_info)) {
	    mtd_names = kmalloc (strlen (s_info) + 2, GFP_KERNEL);
	}
	for (;;) {
	    char* end;

	    if (mtd_names && vmtd_is_name (s_info)) {
		mtd_names_used = vmtd_extract_name (s_info, &end, mtd_names,
						    mtd_names_used);
	    } else {
		unsigned mtd_index = simple_strtoul (s_info, &end, 10);
		*mtd_visible |= 1 << mtd_index;
	    }
	    if (*end != ',') break;
	    s_info = end + 1;
	}
	if (mtd_names) {
	    mtd_names [mtd_names_used] = '\0';
	}
    }
    if (!mtd_names && !*mtd_visible) {
	*mtd_visible = (NkOsMask) -1;
    }
#ifdef VMTD_DEBUG
    vmtd_print_options (mtd_names, *mtd_visible);
#endif
    return mtd_names;
}

/*----- Support for /proc/nk/vmtd-be -----*/

static const char* vmtd_type_names[] = {NK_DEV_MTD_NAMES};

    static int
vmtd_read_proc (char* page, char** start, off_t off, int count, int* eof,
		void* data)
{
    off_t	begin = 0;
    int		len;
    unsigned	num;

    len = sprintf (page,
	"De Typ   Size   Erase WrSz OOB/A NEL Flg U Name\n");

    for (num = 0; num < MAX_MTD_DEVICES; ++num) {
	const vmtd_vmtd_t* vmtd = vmtd_vmtds [num];
	const struct mtd_info* mtd;

	if (!vmtd) continue;
	mtd = vmtd->mtd;
	if (!mtd) continue;

	len += sprintf (page+len,
	    "%2d %3.3s %8llx %5x %4x %2x/%-2x %d%c%c %c%c%c %d %s\n",
	    mtd->index,
	    vmtd_type_names [mtd->type],
	    (unsigned long long) mtd->size,
	    mtd->erasesize,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
	    mtd->writesize,
#else
	    mtd->oobblock,
#endif
	    mtd->oobsize,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
	    mtd->oobavail,
#else
	    0,
#endif
	    mtd->numeraseregions,
	    mtd->eraseregions ? 'E' : '.',
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
	    mtd->ecclayout    ? 'L' : '.',
#else
	    '?',
#endif
	    mtd->flags & MTD_WRITEABLE     ? 'W' : '.',
#ifdef MTD_BIT_WRITEABLE
	    mtd->flags & MTD_BIT_WRITEABLE ? 'B' : '.',
#else
	    '?',
#endif
#ifdef MTD_NO_ERASE
	    mtd->flags & MTD_NO_ERASE      ? '.' : 'E',
#else
	    '?',
#endif
	    mtd->usecount,
	    mtd->name);

	if (data) {
	    _Bool had_any = false;
	    unsigned i;

#define VMTD_ELEMS(a)	(sizeof (a) / sizeof (a) [0])

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
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
#endif
	    for (i = 0; i < NK_DEV_MTD_FUNC_MAX; ++i) {
		if (vmtd->calls [i]) {
		    len += sprintf (page+len, " %s:%d", vmtd_func_names [i],
				    vmtd->calls [i]);
		    had_any = true;
		}
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

static struct proc_dir_entry*	vmtd_proc;
static struct proc_dir_entry*	vmtd_proc_ext;
static vlx_thread_t		vmtd_thread_desc;

    static _Bool
vmtd_free_link (vmq_link_t* link, void* cookie)
{
    vmtd_be_link_t* be_link = VMTD_BE_LINK (link);

    (void) cookie;
	/* Make sure pointed regions and get_device are released */
    vmtd_link_off (link);
    kfree (be_link->mtd_names);
    kfree (be_link);
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
    if (vmtd_links) {
	vmq_links_iterate (vmtd_links, vmtd_free_link, NULL);
	vmq_links_finish (vmtd_links);
	vmtd_links = NULL;
    }
	/*
	 *  Calls vmtd_notifier_remove() for all MTDs.
	 *  Always returns 0.
	 */
    unregister_mtd_user (&vmtd_notifier);
    if (vmtd_proc)     remove_proc_entry ("nk/vmtd-be",     NULL);
    if (vmtd_proc_ext) remove_proc_entry ("nk/vmtd-be.ext", NULL);
}

    static _Bool
vmtd_init_link (vmq_link_t* link, void* cookie)
{
    vmtd_be_link_t* be_link = kzalloc (sizeof *be_link, GFP_KERNEL);

    if (unlikely (!be_link)) {
	*(int*) cookie = -ENOMEM;
	return true;
    }
    VMTD_BE_LINK (link) = be_link;
    be_link->mtd_names = vmtd_parse_options (vmq_link_s_info (link),
					     &be_link->mtd_visible);
    return false;
}

#define VMTD_FIELD(name,value)	value

static const vmq_callbacks_t vmtd_callbacks = {
    VMTD_FIELD (link_on,		NULL),
    VMTD_FIELD (link_off,		vmtd_link_off),
    VMTD_FIELD (link_off_completed,	NULL),
    VMTD_FIELD (sysconf_notify,		vmtd_sysconf_notify),
    VMTD_FIELD (receive_notify,		vmtd_receive_notify),
    VMTD_FIELD (return_notify,		NULL)
};

static const vmq_xx_config_t vmtd_tx_config = {
    VMTD_FIELD (msg_count,	8),
    VMTD_FIELD (msg_max,	sizeof (NkDevMtdMsg)),
    VMTD_FIELD (data_count,	0),
    VMTD_FIELD (data_max,	0)
};

static const vmq_xx_config_t vmtd_rx_config = {
    VMTD_FIELD (msg_count,	16),
    VMTD_FIELD (msg_max,	sizeof (NkDevMtdMsg)),
    VMTD_FIELD (data_count,	16),
    VMTD_FIELD (data_max,	8 * (512 + 16))
};

#undef VMTD_FIELD

    static int __init
vmtd_init (void)
{
    signed diag;

    DTRACE ("\n");
    sema_init (&vmtd_sem, 0);	/* Before it is signaled */
    vmtd_proc     = create_proc_read_entry ("nk/vmtd-be",     0, NULL,
					    vmtd_read_proc, NULL);
    vmtd_proc_ext = create_proc_read_entry ("nk/vmtd-be.ext", 0, NULL,
					    vmtd_read_proc, (void*) 1);
	/*
	 *  Get MTDs first, so that frontend immediately gets the full list.
	 *  Calls vmtd_notifier_add() for all MTDs.
	 */
    register_mtd_user (&vmtd_notifier);	/* void */
    diag = vmq_links_init_ex (&vmtd_links, "vmtd", &vmtd_callbacks,
			      &vmtd_tx_config, &vmtd_rx_config, NULL, false);
    if (diag) goto error;
    if (vmq_links_iterate (vmtd_links, vmtd_init_link, &diag)) goto error;
    diag = vmq_links_start (vmtd_links);
    if (diag) goto error;
    diag = vlx_thread_start (&vmtd_thread_desc, vmtd_thread, vmtd_links,
    			     "vmtd-be");
    if (diag) goto error;
    TRACE ("initialized\n");
    return 0;

error:
    ETRACE ("init failed (%d)\n", diag);
    vmtd_exit();
    return diag;
}

module_init (vmtd_init);
module_exit (vmtd_exit);

/*----- Module description -----*/

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_DESCRIPTION ("VLX Virtual MTD backend driver");

/*----- End of file -----*/

/*
 ****************************************************************
 *
 *  Component: VLX Virtual Block Device v.2 backend driver
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
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *    Eric Lescouet (eric.lescouet@redbend.com)
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 *
 * This driver is based on the original work done in Xen by
 * Keir Fraser & Steve Hand, for the virtual block driver back-end:
 */

/******************************************************************************
 * arch/xen/drivers/blkif/backend/main.c
 *
 * Back-end of the driver for virtual block devices. This portion of the
 * driver exports a 'unified' block-device interface that can be accessed
 * by any operating system that implements a compatible front end. A
 * reference front-end implementation can be found in:
 *  arch/xen/drivers/blkif/frontend
 *
 * Copyright (c) 2003-2004, Keir Fraser & Steve Hand
 *
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*----- System header files -----*/

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
#include <linux/config.h>
#endif
#include <linux/module.h>
#include <asm/system.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/proc_fs.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <linux/preempt.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
#include <linux/freezer.h>
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
#include  <linux/sched.h>
#endif
#endif
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/pgalloc.h>
#include <linux/init.h>		/* module_init() in 2.6.0 and before */
#include <linux/cdrom.h>
#include <linux/loop.h>
#include <asm/uaccess.h>
#include <linux/hdreg.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <linux/dma-mapping.h>
#endif
#include <net/sock.h>
#include <linux/netlink.h>
#include <nk/nkern.h>

/*----- Local configuration -----*/

#if 0
#define VBD_DEBUG
#endif

#if 1
#define VBD_UEVENT
#endif

#ifdef CONFIG_X86
#define VBD2_ATAPI
#define VBD2_FAST_MAP
#endif

#if 0
#define VBD2_FI
#endif

#ifndef CONFIG_NKERNEL_VBD_NR
#define	CONFIG_NKERNEL_VBD_NR	16
#endif

#define	VBD_LINK_MAX_SEGS_PER_REQ	128
#define	VBD_LINK_DEFAULT_MSG_COUNT	64

    /*
     * Maximum number of requests pulled from a given link
     * before scheduling the next one. As there is only one
     * kernel thread currently for all the link interfaces,
     * it gives a chance to all interfaces to be scheduled
     * in a more fair way.
     */
#define VBD_LINK_MAX_REQ	16

/*----- VLX services driver library -----*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#undef VBD_UEVENT
#endif

#define VLX_SERVICES_THREADS
#define VLX_SERVICES_SEQ_FILE
#ifdef VBD_UEVENT
#define VLX_SERVICES_UEVENT
#endif
#include "vlx-services.c"

/*----- Local header files -----*/

#include <vlx/vbd2_common.h>
#include "vlx-vmq.h"

/*----- Tracing -----*/

#define TRACE(_f, _a...)	printk (KERN_INFO "VBD2-BE: " _f , ## _a)
#define WTRACE(_f, _a...) \
			printk (KERN_WARNING "VBD2-BE: Warning: " _f , ## _a)
#define WTRACE_VDISK(_vd, _f, _a...) \
				WTRACE ("%s: " _f, (_vd)->name, ## _a)
#define ETRACE(_f, _a...)	printk (KERN_ERR  "VBD2-BE: Error: " _f , ## _a)
#define ETRACE_VDISK(_vd, _f, _a...) \
				ETRACE ("%s: " _f, (_vd)->name, ## _a)
#define EFTRACE(_f, _a...)	ETRACE ("%s: " _f, __func__, ## _a)
#define XTRACE(_f, _a...)

#ifdef VBD_DEBUG
#define DTRACE(_f, _a...)	printk (KERN_DEBUG "%s: " _f, __func__, ## _a)
#define VBD_ASSERT(c)		BUG_ON (!(c))
#else
#define DTRACE(_f, _a...)
#define VBD_ASSERT(c)
#endif

#define DTRACE_VDISK(_vd, _f, _a...) \
	DTRACE ("%s: " _f, (_vd)->name, ## _a)

/*----- Version compatibility functions -----*/

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,7)
    static void*
kzalloc (size_t size, unsigned flags)
{
    void* ptr = kmalloc (size, flags);

    if (ptr) {
	memset (ptr, 0, size);
    }
    return ptr;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define PDE(i)	((struct proc_dir_entry *)((i)->u.generic_ip))

#define mutex_lock(x)	down(x)
#define mutex_unlock(x)	up(x)
#define mutex_init(x)	init_MUTEX(x)
#define preempt_disable()
#define preempt_enable()
#ifndef BIO_MAX_PAGES
#define BIO_MAX_PAGES	128
#endif
#endif

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/*----- Cache management helpers -----*/

    static inline void
vbd_cache_clean (const char* const start, const size_t len)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
    dmac_map_area (start, len, DMA_TO_DEVICE);
#else
#ifndef CONFIG_X86
    dmac_clean_range (start, start + len);
#endif
#endif
}

    static inline void
vbd_cache_invalidate (const char* const start, const size_t len)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
    dmac_map_area (start, len, DMA_FROM_DEVICE);
#else
#ifndef CONFIG_X86
    dmac_inv_range (start, start + len);
#endif
#endif
}

/*----- Definitions -----*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
typedef sector_t	vbd_sector_t;
#else
typedef u64		vbd_sector_t;
#endif

#define VBD_PAGE_64_MASK	~((NkPhAddr)0xfff)

    /*
     *  The backend can avoid informing the frontend about specific
     *  vdisks until they are actually available, or better, till
     *  their sizes are also non-zero. This allows e.g. to hold the
     *  boot of a frontend guest using a CD for root until losetup(1)
     *  is performed in the backend guest.
     */
typedef enum {
    VBD_VDISK_WAIT_NO,		/* Do not wait for this vdisk */
    VBD_VDISK_WAIT_YES,		/* Wait for this vdisk */
    VBD_VDISK_WAIT_NON_ZERO	/* Wait for vdisk to be non-zero sized */
} vbd_vdisk_wait_t;

static const char vbd_vdisk_wait_modes [3] [3] = {"nw", "wa", "nz"};

    /* The "vdisk" property identifies a virtual disk */
typedef struct {
    nku32_f		tag;
    NkOsId		owner;	/* frontend */
    vbd2_devid_t	devid;
    vbd_vdisk_wait_t	wait;
} vbd_prop_vdisk_t;

    static inline void
vbd_prop_vdisk_init (vbd_prop_vdisk_t* pvd, const nku32_f tag,
		     const NkOsId owner, const vbd2_devid_t devid,
		     const vbd_vdisk_wait_t wait)
{
    pvd->tag   = tag;
    pvd->owner = owner;
    pvd->devid = devid;
    pvd->wait  = wait;
}

    /* The "extent" property identifies a virtual disk extent */
typedef struct {
    vbd_sector_t start;		/* Start sector */
    vbd_sector_t sectors;	/* Size in sectors (0 - up to the end) */
    nku32_f access;		/* Access rights */
    nku32_f minor;		/* Disk minor */
    nku32_f tag;		/* Vdisk tag */
    int     major;		/* Disk major (-1 if unused) */
} vbd_prop_extent_t;

    static inline void
vbd_prop_extent_init (vbd_prop_extent_t* pde, const vbd_sector_t start,
		      const vbd_sector_t sectors, const nku32_f access2,
		      const nku32_f minor, const nku32_f tag, const int major)
{
    pde->start   = start;
    pde->sectors = sectors;
    pde->access  = access2;
    pde->minor   = minor;
    pde->tag     = tag;
    pde->major   = major;
}

#define	VBD_DISK_ACC_R	0x1
#define	VBD_DISK_ACC_W	0x2
#define	VBD_DISK_ACC_RW	(VBD_DISK_ACC_R | VBD_DISK_ACC_W)

typedef struct vbd_be_t			vbd_be_t;
typedef struct vbd_link_t		vbd_link_t;
#ifdef VBD2_FAST_MAP
typedef struct vbd_fast_map_t		vbd_fast_map_t;
#endif
typedef struct vbd_pending_req_t	vbd_pending_req_t;
typedef struct vbd_vdisk_t		vbd_vdisk_t;

    /* Link descriptor */
struct vbd_link_t {
    vmq_link_t*		link;
    _Bool		connected;
    vmq_xx_config_t	xx_config;	/* Receive side */
    unsigned		segs_per_req_max;
    vbd_vdisk_t*	vdisks;
    vbd_be_t*		be;
    struct list_head	blkio_list;	/* (if set) on be.blkio_thread.list */
    atomic_t		refcount;
#ifdef VBD2_FAST_MAP
    struct {
	vbd_fast_map_t*	head;
	spinlock_t	lock;
    } fast_map;
#endif
    struct {
	struct list_head	head;
	spinlock_t		lock;
    } done_list;

    vbd2_req_header_t*	pending_msg;	/* If resource error */
    vbd_pending_req_t*	pending_xreq;	/* If resource error */
    _Bool		changes_signaled;

	/* Performance counters */
    struct {
	unsigned long long	bytes_read;
	unsigned long long	bytes_written;

#ifdef VBD2_FAST_MAP
	struct {
	    unsigned		probes;
	    unsigned		reads;
	    unsigned		writes;
	    unsigned long long	read_bytes;
	    unsigned long long	written_bytes;
	} fast_map;
#endif
	struct {
	    unsigned		probes;
	    unsigned		reads;
	    unsigned		writes;
	    unsigned long long	read_bytes;
	    unsigned long long	written_bytes;
	} nk_mem_map;

	struct {
	    unsigned		reads;
	    unsigned		writes;
	    unsigned long long	read_bytes;
	    unsigned long long	written_bytes;
	} dma;

	unsigned		msg_replies;
	unsigned		requests;
	unsigned		segments;
	unsigned		no_struct_page;

	unsigned		page_allocs;
	unsigned		page_freeings;
	unsigned		alloced_pages;
	unsigned		max_alloced_pages;

#ifdef CONFIG_X86
	unsigned		first_meg_copy;
#endif
	unsigned		resource_errors;
	unsigned		error_replies;	/* subset of msg_replies */
    } stats;
};

#define VBD_LINK_FOR_ALL_VDISKS(_vd,_bl) \
    for ((_vd) = (_bl)->vdisks; (_vd); (_vd) = (_vd)->next)

#ifdef VBD2_ATAPI
    /* Virtual disk ATAPI descriptor */
typedef struct {
    vbd2_req_header_t*	req;
    vbd2_devid_t	devid;
    NkPhAddr		paddr;
    uint8_t		cdb [VBD2_ATAPI_PKT_SZ];
    uint32_t		buflen;
} vbd_atapi_req_t;

typedef struct {
    vbd_atapi_req_t	req;	/* Pending ATAPI request from frontend */
    uint32_t		count;	/* Request count (0-1) */
    wait_queue_head_t	wait;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    struct mutex	lock;
#else
    struct semaphore	lock;
#endif
    int			ctrl_open;
    int			data_open;
    char		ctrl_name [32];
    char		data_name [32];
} vbd_atapi_t;
#endif /* VBD2_ATAPI */

typedef struct vbd_extent_t vbd_extent_t;

    /* Virtual disk descriptor */
struct vbd_vdisk_t {
    nku32_f		tag;		/* Virtual disk tag */
    vbd2_devid_t	devid;		/* Virtual disk ID */
    vbd2_genid_t	genid;		/* Generation of devid */
    vbd_extent_t*	extents;	/* List of extents */
    vbd_vdisk_t*	next;		/* Next virtual disk */
    vbd_link_t*		bl;		/* Link to frontend */
    vbd_vdisk_wait_t	wait;		/* Vdisk wait mode (none, existence,
					   non-zero size) */
    _Bool		open;		/* vdisk has been opened */
    char		name [24];	/* (guest,major,minor) string */
#ifdef VBD2_ATAPI
    vbd_atapi_t*	atapi;		/* ATAPI device support */
#endif
};

    static void
vbd_vdisk_update_name (vbd_vdisk_t* vd)
{
    snprintf (vd->name, sizeof vd->name, "(%d,%d,%d:%d)",
	      vmq_peer_osid (vd->bl->link), VBD2_DEVID_MAJOR (vd->devid),
	      VBD2_DEVID_MINOR (vd->devid), vd->genid);
}

    static inline void
vbd_vdisk_init (vbd_vdisk_t* vd, nku32_f tag, vbd2_devid_t devid,
		vbd_link_t* bl, vbd_vdisk_wait_t wait)
{
    vd->tag        = tag;
    vd->devid      = devid;
	/* genid = 0; */
	/* extents = NULL; */
	/* next = NULL; */
    vd->bl   = bl;
    vd->wait = wait;
	/* open = false; */
    vbd_vdisk_update_name (vd);
	/* atapi = NULL; */
}

#define VBD_VDISK_FOR_ALL_EXTENTS(_ex,_vd) \
    for ((_ex) = (_vd)->extents; (_ex); (_ex) = (_ex)->next)

    /* Virtual disk extent descriptor */
struct vbd_extent_t {
    vbd_extent_t*		next;	/* Next extent */
    vbd_sector_t		start;	/* Start sector */
    vbd_sector_t		sectors;/* Size in sectors */
    dev_t			dev;	/* Real device id */
    struct block_device*	bdev;	/* Real block device */
    nku32_f			access;	/* Access rights */
    const vbd_prop_extent_t*	prop;	/* Property */
    vbd_vdisk_t*		vdisk;	/* Virtual disk */
    _Bool			openable;
};

    static inline void
vbd_extent_init (vbd_extent_t* ex, vbd_sector_t start,
		 vbd_sector_t sectors, dev_t dev, nku32_f access2,
		 const vbd_prop_extent_t* prop, vbd_vdisk_t* vdisk)
{
    ex->start   = start;
    ex->sectors = sectors;
    ex->dev     = dev;
	/* bdev = NULL;	set at connect time */
    ex->access  = access2;
    ex->prop    = prop;
    ex->vdisk   = vdisk;
}

    /* Driver descriptor */
struct vbd_be_t {
    vmq_links_t*	links;
    atomic_t		refcount;		/* Reference count */
    _Bool		resource_error;
    vbd_prop_extent_t	prop_extents [CONFIG_NKERNEL_VBD_NR];
    vbd_prop_vdisk_t	prop_vdisks [CONFIG_NKERNEL_VBD_NR];
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
    kmem_cache_t*	pending_req_cachep;
#else
    struct kmem_cache*	pending_req_cachep;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    kmem_cache_t*	buffer_head_cachep;
#endif
    struct {
	wait_queue_head_t	wait;
	vlx_thread_t		desc;
	struct list_head	list;
	spinlock_t		list_lock;
    } blkio_thread;
    struct {
	vlx_thread_t		desc;
	_Bool			abort;
	wait_queue_head_t	wait;
	_Bool			recheck_extents;
	char			buf [1024];
	_Bool			polling_needed;
    } event_thread;
#ifdef VBD_UEVENT
    vlx_uevent_t	uevent;
#endif
    _Bool		sysconf;
    _Bool		proc_inited;
	/* Performance counters */
    struct {
	unsigned long long	sleeps_no_timeout;
	unsigned		sleeps_with_timeout;
	unsigned		timeouts;
    } stats;
};

#define VBD_BE_FOR_ALL_PROP_EXTENTS(_pe,_be) \
    for ((_pe) = (_be)->prop_extents; \
	 (_pe) < (_be)->prop_extents + VBD_ARRAY_ELEMS ((_be)->prop_extents)\
	 && (_pe)->major; \
	 ++(_pe))

#define VBD_BE_FOR_ALL_PROP_VDISKS(_pvd,_be) \
    for ((_pvd) = (_be)->prop_vdisks; \
	 (_pvd) < (_be)->prop_vdisks + VBD_ARRAY_ELEMS ((_be)->prop_vdisks)\
	 && (_pvd)->devid; \
	 ++(_pvd))

static int vbd2_dma = 1;	/* DMA (default) or COPY mode */

    /*
     * Each outstanding request that we've passed to the lower device
     * layers has a 'pending_req' allocated with it. Each buffer_head
     * that completes decrements the pendcount towards zero. When it
     * hits zero, a response is queued for with the saved 'cookie'
     * passed back.
     */
typedef struct vbd_pending_seg_t {
    NkPhAddr      gaddr;	/* Only used when reading in copy mode */
    unsigned long vaddr;
    unsigned int  size;		/* Only used when reading in copy mode */
} vbd_pending_seg_t;

struct vbd_pending_req_t {
    vbd_link_t*		bl;
    vbd2_req_header_t*	req;
    vbd2_op_t		op;	/* Trusted backup of original request */
    int			error;
    atomic_t		pendcount;
    struct list_head	list;
    nku32_f		nsegs;
    vbd_pending_seg_t	segs [VBD_LINK_MAX_SEGS_PER_REQ];
    vbd2_sector_t	vsector;
};

    static inline void
vbd_pending_req_init (vbd_pending_req_t* xreq, vbd_link_t* bl,
		      vbd2_req_header_t* req)
{
    xreq->bl        = bl;
    xreq->req       = req;
    xreq->op        = req->op;	/* In case frontend maliciously changes it */
    xreq->error     = FALSE;
    atomic_set (&xreq->pendcount, 1);
	/* xreq->list does not require init */
    xreq->nsegs     = 0;
	/* xreq->segs[] does not require init */
    xreq->vsector   = req->sector;
}

    /* Only from vbd_link_init() <- vbd_init() */
#define vbd_be_get(_be)	\
    do { \
	atomic_inc (&(_be)->refcount); \
	DTRACE ("be.refcount now %d\n", (_be)->refcount.counter); \
    } while (0)

    /* Only used by vbd_link_release() <- vbd_link_put() <- many */
#define vbd_be_put(_be) \
    do { \
	if (atomic_dec_and_test (&(_be)->refcount)) \
	    vbd_be_release (_be); \
    } while (0)

#define vbd_link_get(_link)	atomic_inc (&(_link)->refcount)
#define vbd_link_put(_link) \
    do { \
	if (atomic_dec_and_test (&(_link)->refcount)) \
	    vbd_link_release (_link); \
    } while (0)

    /*
     * When drivers are built-in, Linux errors if __exit is
     * defined on functions which are called by normal code.
     */
#ifdef MODULE
#define VBD_EXIT __exit
#else
#define VBD_EXIT
#endif

#define VBD_ARRAY_ELEMS(a)	(sizeof (a) / sizeof (a) [0])

/*----- Fast map -----*/

#ifdef VBD2_FAST_MAP

#ifdef CONFIG_X86
#define	VBD_BIO_MAX_VECS	(BIO_MAX_PAGES/2)

struct vbd_fast_map_t {
    void*		addr;
    vbd_fast_map_t*	next;
#if defined CONFIG_X86_PAE || defined CONFIG_X86_64
    u64*		pte;
#ifdef VBD_DEBUG
    u64			inactive_pte;
    u64			active_pte;
#endif
#else
    u32*		pte;
#ifdef VBD_DEBUG
    u32			inactive_pte;
    u32			active_pte;
#endif
#endif
};

#ifdef VBD_DEBUG
    static void
vbd_fast_map_print (const char* func, const unsigned line,
		    const vbd_fast_map_t* map)
{
    TRACE ("%s:%d addr %p next %p ptep %p *ptep %llx inactive %llx "
	   "active %llx\n", func, line, map->addr, map->next, map->pte,
	   (u64) *map->pte, (u64) map->inactive_pte, (u64) map->active_pte);
}

#define VBD_FAST_MAP_CHECK(map,field) \
    if ((*(map)->pte & ~0x70) != ((map)->field & ~0x70)) { \
	vbd_fast_map_print (__func__, __LINE__, (map)); \
    }
#define VBD_FAST_MAP_SAVE(map,field,val)	(map)->field = (val)
#else
#define VBD_FAST_MAP_CHECK(map,field)		do {} while (0)
#define VBD_FAST_MAP_SAVE(map,field,val)	do {} while (0)
#endif

#if defined CONFIG_X86_64
    static u64*
vbd_x86_get_ptep (void* vaddr)
{
    u64 pte;

    __asm__ __volatile__("movq  %%cr3, %0" : "=r"(pte));
    pte = ((u64*)__va (pte & PAGE_MASK))[(((u64) vaddr) >> 39) & 0x1ff];
    pte = ((u64*)__va (pte & PAGE_MASK))[(((u64) vaddr) >> 30) & 0x1ff];
    pte = ((u64*)__va (pte & PAGE_MASK))[(((u64) vaddr) >> 21) & 0x1ff];
    return ((u64*)__va (pte & PAGE_MASK)) + ((((u64) vaddr) >> 12) & 0x1ff);
}
#elif defined CONFIG_X86_PAE
    static u64*
vbd_x86_get_ptep (void* vaddr)
{
    u32  cr3;
    u64  pte;

    __asm__ __volatile__("movl  %%cr3, %0" : "=r"(cr3));
    pte = ((u64*)__va (cr3))[(((u32) vaddr) >> 30) & 0x3];
    pte = ((u64*)__va (pte & VBD_PAGE_64_MASK))[(((u32) vaddr) >> 21) & 0x1ff];
    return ((u64*)__va (pte & VBD_PAGE_64_MASK)) + (((u32) vaddr >> 12) & 0x1ff);
}
#else
    static u32*
vbd_x86_get_ptep (void* vaddr)
{
    u32 pte;

    __asm__ __volatile__("movl  %%cr3, %0" : "=r"(pte));
    pte = ((u32*)__va (pte & PAGE_MASK))[(((u32) vaddr) >> 22) & 0x3ff];
    return ((u32*)__va (pte & PAGE_MASK)) + ((((u32) vaddr) >> 12) & 0x3ff);
}
#endif

    /*
     * Only called from vbd_link_init() <- vbd_init().
     */

    static void __init
vbd_link_fast_map_create (vbd_link_t* bl)
{
	/* 64 * 128 * 256/2 */
    int pages = (bl->xx_config.msg_count * bl->segs_per_req_max) /
	VBD_BIO_MAX_VECS;

    spin_lock_init (&bl->fast_map.lock);
    bl->fast_map.head = NULL;
    while (pages--) {
	void*		addr;
	vbd_fast_map_t*	map;

	addr = nkops.nk_mem_map ((unsigned int) PAGE_MASK, 1 << PAGE_SHIFT);
	if (unlikely (!addr)) {
	    ETRACE ("Fast mapping allocation failure\n");
	    return;
	}
	map = kmalloc (sizeof *map, GFP_KERNEL);
	if (unlikely (!map)) {
	    nkops.nk_mem_unmap (addr, (unsigned int) PAGE_MASK,
				1 << PAGE_SHIFT);
	    ETRACE ("Fast mapping allocation failure\n");
	    return;
	}
	map->addr = addr;
	map->pte  = vbd_x86_get_ptep (addr);
	VBD_FAST_MAP_SAVE (map, inactive_pte, *map->pte);
	map->next = bl->fast_map.head;
	bl->fast_map.head = map;
	DTRACE ("vaddr 0x%p ptep 0x%p (pte 0x%llx)\n",
		map->addr, map->pte, (u64) *map->pte);
    }
}

    static void
vbd_link_fast_map_delete (vbd_link_t* bl)
{
    while (bl->fast_map.head) {
	vbd_fast_map_t* map = bl->fast_map.head;

	bl->fast_map.head = map->next;
	nkops.nk_mem_unmap (map->addr, (unsigned int) PAGE_MASK,
			    1 << PAGE_SHIFT);
	kfree (map);
    }
}

    static vbd_fast_map_t*
vbd_link_fast_map_alloc (vbd_link_t* bl)
{
    unsigned long	flags;
    vbd_fast_map_t*	map;

    spin_lock_irqsave (&bl->fast_map.lock, flags);
    map = bl->fast_map.head;
    if (map) {
	bl->fast_map.head = map->next;
    }
    spin_unlock_irqrestore (&bl->fast_map.lock, flags);
    return map;
}

    static void
vbd_link_fast_map_free (vbd_link_t* bl, vbd_fast_map_t* map)
{
    unsigned long flags;

    spin_lock_irqsave (&bl->fast_map.lock, flags);
    map->next = bl->fast_map.head;
    bl->fast_map.head = map;
    spin_unlock_irqrestore (&bl->fast_map.lock, flags);
}

    static inline void
vbd_x86_flush (char* addr)
{
#if 0
    nku32_f cr3;
#endif

    __asm__ __volatile__("invlpg %0"::"m" (*addr));

#if 0
    __asm__ __volatile__(
	"movl %%cr3, %0;\n\t"
	"movl %0, %%cr3\n\t;"
    : "=r" (cr3) :: "memory");
#endif
}

#if defined CONFIG_X86_64 || defined CONFIG_X86_PAE
    static inline void
vbd_fast_map_map (vbd_fast_map_t* map, u64 paddr)
{
    preempt_disable();
    VBD_FAST_MAP_CHECK (map, inactive_pte);
    *map->pte = paddr | (1 << 8) | (1 << 1) | (1 << 0);
    VBD_FAST_MAP_SAVE (map, active_pte, paddr | (1 << 8) | (1 << 1) | (1 << 0));
    vbd_x86_flush ((char*) map->addr);
}
#else
    static inline void
vbd_fast_map_map (vbd_fast_map_t* map, u32 paddr)
{
    preempt_disable();
    VBD_FAST_MAP_CHECK (map, inactive_pte);
    *map->pte = paddr | (1 << 8) | (1 << 1) | (1 << 0);
    VBD_FAST_MAP_SAVE (map, active_pte, paddr | (1 << 8) | (1 << 1) | (1 << 0));
    vbd_x86_flush ((char*) map->addr);
}
#endif

    static inline void
vbd_fast_map_unmap (vbd_fast_map_t* map)
{
    VBD_FAST_MAP_CHECK (map, active_pte);
    *map->pte = (unsigned int) PAGE_MASK | (1 << 8) | (1 << 1) | (1 << 0);
    vbd_x86_flush ((char*) map->addr);
    VBD_FAST_MAP_CHECK (map, inactive_pte);
    preempt_enable();
}
#else	/* not CONFIG_X86 */

struct vbd_fast_map_t {
    void* addr;
};

    static inline void
vbd_link_fast_map_create (vbd_link_t* bl)
{
}

    static inline void
vbd_link_fast_map_delete (vbd_link_t* bl)
{
}

    static inline vbd_fast_map_t*
vbd_link_fast_map_alloc (vbd_link_t* bl)
{
    return NULL;
}

    static inline void
vbd_link_fast_map_free (vbd_link_t* bl, vbd_fast_map_t* map)
{
}

    static inline void
vbd_fast_map_map (vbd_fast_map_t* map, unsigned long paddr)
{
}

    static inline void
vbd_fast_map_unmap (vbd_fast_map_t* map)
{
}
#endif  /* not CONFIG_X86 */
#endif	/* not VBD2_FAST_MAP */

/*----- VBD link -----*/

#define VBD_LINK(link) \
    ((vbd_link_t*) ((vmq_link_public*) (link))->priv)
#undef VBD_LINK
#define VBD_LINK(link) (*(vbd_link_t**) (link))

typedef struct {
    NkOsId	osid;
    vmq_link_t*	link;
} vbd_links_find_osid_t;

    /* Only by vbd_links_find_osid() <- vbd_be_vdisk_create() <- vbd_init() */

    static _Bool __init
vbd_link_match_osid (vmq_link_t* link2, void* cookie)
{
    vbd_links_find_osid_t* ctx = (vbd_links_find_osid_t*) cookie;

    if (vmq_peer_osid (link2) != ctx->osid) return false;
    ctx->link = link2;
    return true;
}

    /* Only called by vbd_be_vdisk_create() <- vbd_init() */

    static vbd_link_t* __init
vbd_links_find_osid (vmq_links_t* links, NkOsId osid)
{
    vbd_links_find_osid_t	ctx;

    ctx.osid = osid;
    if (!vmq_links_iterate (links, vbd_link_match_osid, &ctx)) return NULL;
    return VBD_LINK (ctx.link);
}

static void vbd_vdisk_close (vbd_vdisk_t*);
static void vbd_vdisk_free  (vbd_vdisk_t*);

    /* Only called from vbd_be_destroy() <- vbd_exit() */

    static _Bool VBD_EXIT
vbd_link_destroy (vmq_link_t* link2, void* cookie)
{
    vbd_link_t* bl = VBD_LINK (link2);
    vbd_vdisk_t* vd;

    (void) cookie;
    while ((vd = bl->vdisks) != NULL) {
	bl->vdisks = vd->next;
	vbd_vdisk_close (vd);
	vbd_vdisk_free (vd);
    }
#ifdef VBD2_FAST_MAP
    vbd_link_fast_map_delete (bl);
#endif
    kfree (bl);
    return false;
}

    /* Called by vbd_be_put() and vbd_link_release() */

    static void
vbd_be_release (vbd_be_t* be)
{
    DTRACE ("entered\n");
	/* Awake the thread */
    wake_up (&be->blkio_thread.wait);
}

    /* Only used by vbd_link_put() macro */

    static void
vbd_link_release (vbd_link_t* bl)
{
    vbd_be_put (bl->be);
}

    /*
     *  This routine is called both from the cross-interrupt through
     *  which requests are received from the frontend, in case of
     *  synchronous requests which can be answered immediately,
     *  like probes, and from hardware disk interrupt, in case of
     *  asynchronous requests which really went to disk. These two
     *  interrupts can nest.
     */

    static void
vbd_link_resp (vbd_link_t* bl, vbd2_req_header_t* req, vbd2_status_t status)
{
    vbd2_resp_t* resp = (vbd2_resp_t*) req;

    resp->count = status;
    vmq_msg_return (bl->link, resp);
    ++bl->stats.msg_replies;
	/* For probes, the return value is non-zero and this is ok */
    if (status == VBD2_STATUS_ERROR) {
	++bl->stats.error_replies;
    }
}

static void vbd_vdisk_probe (vbd_vdisk_t* vd, vbd2_probe_t* probe);

    static vbd_vdisk_t*
vbd_link_vdisk_lookup (vbd_link_t* bl, const vbd2_req_header_t* req)
{
    vbd_vdisk_t* vd;

    VBD_LINK_FOR_ALL_VDISKS (vd, bl) {
	if (vd->devid == req->devid && vd->genid == req->genid) {
	    return vd;
	}
    }
    ETRACE ("(%d,%d,%d:%d) virtual disk not found\n",
	    vmq_peer_osid (bl->link), VBD2_DEVID_MAJOR (req->devid),
	    VBD2_DEVID_MINOR (req->devid), req->genid);
    return NULL;
}

    static vbd_prop_extent_t*
vbd_be_prop_extent_lookup (vbd_be_t* be, nku32_f tag)
{
    vbd_prop_extent_t* pe;

    VBD_BE_FOR_ALL_PROP_EXTENTS (pe, be) {
	if (pe->tag == tag) return pe;
    }
    return NULL;
}

static vbd_sector_t vbd_vdisk_sectors  (const vbd_vdisk_t*);
static _Bool        vbd_vdisk_openable (const vbd_vdisk_t*);

    /* Probing of vdisks */

    static vbd2_status_t
vbd_link_op_probe (vbd_link_t* bl, const vbd2_req_header_t* req,
		   vbd2_probe_t* probe, NkPhSize psize)
{
    vbd_vdisk_t*	vd;
    vbd2_status_t	status = 0;
    unsigned		skip = req->sector;

    if (!req->sector) {		/* Beginning of probing */
	bl->changes_signaled = false;
    }
    VBD_LINK_FOR_ALL_VDISKS (vd, bl) {
	if (psize < sizeof (vbd2_probe_t)) break;

	switch (vd->wait) {
	case VBD_VDISK_WAIT_NO:
	    break;

	case VBD_VDISK_WAIT_YES:
	    if (!vbd_vdisk_openable (vd)) continue;
	    break;

	case VBD_VDISK_WAIT_NON_ZERO:
	    if (!vbd_vdisk_openable (vd)) continue;
	    if (!vbd_vdisk_sectors (vd)) continue;
	    break;
	}
	if (skip > 0) {
	    --skip;
	    continue;
	}
	vbd_vdisk_probe (vd, probe);	/* void */
	psize -= sizeof (vbd2_probe_t);
	probe++;
	status++;
    }
    return status;
}

    /* Polling of vdisks */

    static _Bool
vbd_link_vdisk_polling_needed (vmq_link_t* link2, void* cookie)
{
    vbd_link_t*		bl = VBD_LINK (link2);
    vbd_vdisk_t*	vd;

    VBD_LINK_FOR_ALL_VDISKS (vd, bl) {
#ifdef VBD_UEVENT
	if (vd->wait != VBD_VDISK_WAIT_NON_ZERO) continue;
	    /*
	     * If the disk is not openable, we assume
	     * we are going to first get a uevent when
	     * it becomes so, therefore no need to poll.
	     */
	if (!vbd_vdisk_openable (vd)) continue;
#else
	if (vd->wait == VBD_VDISK_WAIT_NO) continue;
#endif
	DTRACE_VDISK (vd, "must poll it\n");
	*(_Bool*) cookie = TRUE;
	return TRUE;	/* interrupt vmq link scanning */
    }
    return FALSE;
}

    static _Bool
vbd_be_vdisk_polling_needed (vbd_be_t* be)
{
    _Bool polling_needed = FALSE;

    vmq_links_iterate (be->links, vbd_link_vdisk_polling_needed,
		       &polling_needed);
    return be->event_thread.polling_needed = polling_needed;
}

    /* Operations called by vbd_vdisk_probe_op() for each request */
typedef vbd2_status_t (*vbd_vdisk_probe_op_t)(vbd_vdisk_t* vd,
					      const vbd2_req_header_t* req,
					      vbd2_probe_t* probe,
					      NkPhSize psize);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
#define VBD_GENDISK(bdev)		(bdev)->bd_disk
#define VBD_BDEV_CLOSE(bdev,mode)	blkdev_put ((bdev),(mode))
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define VBD_GENDISK(bdev)		(bdev)->bd_disk
#define VBD_BDEV_CLOSE(bdev,mode)	blkdev_put (bdev)
#else
#define VBD_GENDISK(bdev)		get_gendisk ((bdev)->bd_inode->i_rdev)
#define VBD_BDEV_CLOSE(bdev,mode)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
typedef int (*vbd_fops_ioctl_t) (struct block_device*, fmode_t, unsigned,
				 unsigned long);
#define VBD_GET_IOCTL(fops)						\
  ((fops)->ioctl ? (fops)->ioctl : (fops)->compat_ioctl)
#define VBD_DO_IOCTL(ioctl,bdev,mode,cmd,arg) ioctl (bdev, mode, cmd, arg)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
    /*
     *  This code is suitable for versions from 2.6.27 down to 2.6.17
     *  at least, but might be required for older releases as well.
     */
typedef int (*vbd_fops_ioctl_t) (struct inode*, struct file*, unsigned,
				 unsigned long);
#define VBD_GET_IOCTL(fops)	fops->ioctl
#define VBD_DO_IOCTL(ioctl,bdev,mode,cmd,arg) ioctl (bdev->bd_inode, \
					      (struct file*) NULL, cmd, arg)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
typedef int (*vbd_fops_ioctl_t) (struct inode*, fmode_t, unsigned,
				 unsigned long);
#define VBD_GET_IOCTL(fops)	fops->ioctl
#define VBD_DO_IOCTL(ioctl,bdev,mode,cmd,arg) \
		ioctl (bdev->bd_inode, mode, cmd, arg)
#else
typedef int (*vbd_fops_ioctl_t) (struct inode*, struct file*, unsigned,
				 unsigned long);
#define VBD_GET_IOCTL(fops)	fops->ioctl
#define VBD_DO_IOCTL(ioctl,bdev,mode,cmd,arg) \
		ioctl (bdev->bd_inode, mode, cmd, arg)
#endif

#define VBD_BDEV_OPEN(bdev)	((bdev) && !IS_ERR(bdev))

static void vbd_vdisk_recheck_extents_async (vbd_vdisk_t* vd);

    /*
     * Removable media probing (called periodically by the front-end)
     * Use for virtual ATAPI - LOOP device only
     */

    static vbd2_status_t
vbd_vdisk_op_media_probe (vbd_vdisk_t* vd, const vbd2_req_header_t* req,
			  vbd2_probe_t* probe, NkPhSize psize)
{
    vbd_extent_t*				ex;
    struct gendisk*				gdisk;
    const struct block_device_operations*	fops;

    probe->devid = req->devid;
    probe->genid = req->genid;

    if (psize < sizeof (vbd2_probe_t)) {
	EFTRACE ("invalid request\n");
	return VBD2_STATUS_ERROR;
    }
    ex = vd->extents;
    if (!ex->sectors) {
	vbd_vdisk_recheck_extents_async (vd);
	DTRACE_VDISK (vd, "no extent\n");
	return VBD2_STATUS_ERROR;
    }
    if (ex->next) {
	EFTRACE ("exactly 1 extent expected for removable media\n");
	return VBD2_STATUS_ERROR;
    }
    VBD_ASSERT (VBD_BDEV_OPEN (ex->bdev));
    if (!(gdisk = VBD_GENDISK (ex->bdev))) {
	EFTRACE ("no gendisk\n");
	return VBD2_STATUS_ERROR;
    }
    if (!(fops = gdisk->fops)) {
	EFTRACE ("no fops\n");
	return VBD2_STATUS_ERROR;
    }
    vbd_vdisk_probe (vd, probe);	/* void */
    if (!probe->sectors) {
	DTRACE_VDISK (vd, "zero size - disconnecting\n");
	return VBD2_STATUS_ERROR;
    }
    return VBD2_STATUS_OK;
}

    /*
     * Removable media Load/Eject.
     * Use for virtual ATAPI - LOOP device only.
     */

    static vbd2_status_t
vbd_vdisk_op_media_control (vbd_vdisk_t* vd, const vbd2_req_header_t* req,
			    vbd2_probe_t* probe, NkPhSize psize)
{
    vbd_extent_t*				ex;
    struct gendisk*				gdisk;
    const struct block_device_operations*	fops;
    vbd2_sector_t				flags;
    vbd_fops_ioctl_t				ioctl;
    int						res;

    probe->devid = req->devid;
    probe->genid = req->genid;

    if (psize < sizeof (vbd2_probe_t)) {
	EFTRACE ("invalid request\n");
	return VBD2_STATUS_ERROR;
    }
    ex = vd->extents;
    if (ex->next) {
	ETRACE_VDISK (vd, "exactly 1 extent expected for removable media\n");
	return VBD2_STATUS_ERROR;
    }
    VBD_ASSERT (VBD_BDEV_OPEN (ex->bdev));
    if (!(gdisk = VBD_GENDISK (ex->bdev))) {
	EFTRACE ("no gendisk\n");
	return VBD2_STATUS_ERROR;
    }
    if (!(fops = gdisk->fops)) {
	EFTRACE ("no fops\n");
	return VBD2_STATUS_ERROR;
    }
    if (!(ioctl = VBD_GET_IOCTL (fops))) {
	EFTRACE ("no ioctl fops\n");
	return VBD2_STATUS_ERROR;
    }
    flags = req->sector;
    DTRACE_VDISK (vd, "flags=0x%llx\n", flags);

	/* Start always succeeds with LOOP device */
    if (!(flags & VBD2_FLAG_START)) {
	    /* Stop always succeeds with LOOP device */
	if (flags & VBD2_FLAG_LOEJ) {
		/* Eject */
	    DTRACE_VDISK (vd, "eject - disconnecting\n");
		/*
		 * Detach the loop device.
		 * -> will require the user to re-attach (new media)
		 */
	    res = VBD_DO_IOCTL (ioctl, ex->bdev, 0, LOOP_CLR_FD, 0);
	    if (res) {
		EFTRACE ("eject failed (%d)\n", res);
		probe->info |= VBD2_FLAG_LOCKED;
		return VBD2_STATUS_ERROR;
	    }
	    ex->sectors = 0;
	}
    }
    return VBD2_STATUS_OK;
}

    /*
     * Removable media lock/unlock.
     * Use for virtual ATAPI - LOOP device only.
     */

    static vbd2_status_t
vbd_vdisk_op_media_lock (vbd_vdisk_t* vd, const vbd2_req_header_t* req,
			 vbd2_probe_t* probe, NkPhSize psize)
{
    (void) vd;
    (void) req;
    (void) probe;
    (void) psize;
    return VBD2_STATUS_OK;	/* Always succeed with LOOP device */
}

#ifdef VBD2_ATAPI
    /* Removable media - generic PACKET commmand (ATAPI) */

    static _Bool
vbd_vdisk_is_atapi (vbd_vdisk_t* vd, int* major, int* minor)
{
    vbd_prop_extent_t* prop = vbd_be_prop_extent_lookup (vd->bl->be, vd->tag);

    if (prop && (prop->major == SCSI_CDROM_MAJOR)) {
	*major = prop->major;
	*minor = prop->minor;
	return TRUE;
    }
    return FALSE;
}

    static int
vbd_atapi_ctrl_proc_open (struct inode* inode, struct file* file)
{
    vbd_vdisk_t* vd = PDE (inode)->data;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    MOD_INC_USE_COUNT;
#endif
    vd->atapi->ctrl_open++;
    return 0;
}

    static int
vbd_atapi_ctrl_proc_release (struct inode* inode, struct file* file)
{
    vbd_vdisk_t* vd = PDE (inode)->data;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    MOD_DEC_USE_COUNT;
#endif
    vd->atapi->ctrl_open--;
    return 0;
}

    static loff_t
vbd_atapi_ctrl_proc_lseek (struct file* file, loff_t off, int whence)
{
    loff_t    new;

    switch (whence) {
    case 0:  new = off; break;
    case 1:  new = file->f_pos + off; break;
    case 2:
    default: return -EINVAL;
    }
    if (new > sizeof (vbd2_atapi_req_t)) {
	return -EINVAL;
    }
    return file->f_pos = new;
}

    static ssize_t
vbd_atapi_ctrl_proc_read (struct file* file, char* buf, size_t count,
			  loff_t* ppos)
{
    vbd_vdisk_t* vd    = PDE (file->f_dentry->d_inode)->data;
    vbd_atapi_t* atapi = vd->atapi;

    if (*ppos || count != (VBD2_ATAPI_PKT_SZ + sizeof (uint32_t))) {
	return -EINVAL;
    }
    if (wait_event_interruptible (atapi->wait, atapi->count)) {
	return -EINTR;
    }
    if (copy_to_user (buf, atapi->req.cdb, count)) {
	return -EFAULT;
    }
    *ppos += count;
    return count;
}

    /* Check if extents need connect/disconnect on */
#define TEST_UNIT_READY		0x00

    static void
vbd_atapi_vdisk_extent_check_tur (vbd_vdisk_t* vd, vbd2_atapi_req_t* areq)
{
    if (areq->status) {	/* Unit is not ready */
	if (vd->extents->sectors) {
	    DTRACE_VDISK (vd, "disconnecting - media removed "
		    "status=%d key(%x/%x/%x)\n",
		    areq->status, areq->sense.sense_key,
		    areq->sense.asc, areq->sense.ascq);
	    vd->extents->sectors = 0;
	}
    } else {		/* Unit is ready */
	if (!vd->extents->sectors) {
	    DTRACE_VDISK (vd, "connecting - media present\n");
	    vbd_vdisk_recheck_extents_async (vd);
	}
    }
}

    /* Check if extents need connect/disconnect on */

#define GET_EVENT_STATUS_NOTIFICATION	0x4a
#define		GESN_OPCHANGE		1	/* Operational Change Class */
#define		GESN_MEDIA		4	/* Media Class */
#define		GESN_MASK(e)		(1 << (e))
#define		GESN_MEDIA_NEW		0x02    /* Event */
#define		GESN_MEDIA_REMOVAL	0x03    /* Event */
#define		GESN_MEDIA_PRESENT	0x02    /* Status */
#define		GESN_TRAY_OPEN		0x01    /* Status */

    static _Bool
vbd_atapi_vdisk_extent_check_gesn (vbd_vdisk_t* vd, uint8_t* reply)
{
    if ((reply [2] == GESN_MEDIA) && (reply [1] > 4)) { /* MEDIA event */
	if (reply [5] == GESN_MEDIA_PRESENT) {	/* Status */
	    if (!vd->extents->sectors) {
		    /*
		     * HACK for Windows:
		     * The backend driver may have "consumed" the NewMedia
		     * event issued immediately after inserting the media.
		     * We just inject one to be sure the frontend will have
		     * it, as some guests rely on the event and not on the
		     * status.
		     */
		reply [4] = GESN_MEDIA_NEW; /* NewMedia event */
		DTRACE_VDISK (vd, "connecting - media present\n");
		vbd_vdisk_recheck_extents_async (vd);
	    }
	} else {
	    if (vd->extents->sectors) {
		    /*
		     * HACK for Windows:
		     * The backend driver may have "consumed" the MediaRemoval
		     * event issued immediately after ejecting the media.
		     * We just inject one to be sure the frontend will have it,
		     * as some guests rely on the event and not on the status.
		     */
		reply [4] = GESN_MEDIA_REMOVAL; /* MediaRemoval event */
		DTRACE_VDISK (vd, "disconnecting - media removed\n");
		vd->extents->sectors = 0;
	    }
	}
	return FALSE;
    }
    if ((reply [2] == GESN_OPCHANGE) && (reply [1] >= 6) && /* OPC event */
	(reply [4] == 0x02) && (reply [7] == 0x01)) {	  /* Feature change */
	if (!vd->extents->sectors) {
	    DTRACE_VDISK (vd, "(lost?) MEDIA event needed\n");
	    return TRUE; /* Notify user process that media event is needed */
	}
    }
    return FALSE;
}

    static ssize_t
vbd_atapi_ctrl_proc_write (struct file* file, const char* buf, size_t size,
			   loff_t* ppos)
{
    vbd_vdisk_t*	vd = PDE (file->f_dentry->d_inode)->data;
    vbd_link_t*		bl = vd->bl;
    vbd_atapi_t*	atapi = vd->atapi;
    vbd2_status_t	status;
    ssize_t		res;
    NkPhAddr		paddr;
    vbd2_atapi_req_t*	areq;
#ifdef VBD2_FAST_MAP
    vbd_fast_map_t*	map;
#endif

    if (!atapi->count ||
	(*ppos != (uint32_t)&(((vbd2_atapi_req_t*)0)->status)) ||
	(size  != (sizeof (uint32_t) + sizeof (vbd2_atapi_sense_t)))) {
	return -EINVAL;
    }
    status = VBD2_STATUS_OK;
    res    = size;
    paddr  = atapi->req.paddr; /* ATAPI request is page aligned */
    *ppos  = 0;

#ifdef VBD2_FAST_MAP
    if ((map = vbd_link_fast_map_alloc (bl)) != NULL) {
	vbd_fast_map_map (map, paddr);	/* Preemption is disabled */
	++bl->stats.fast_map.probes;
	areq = (vbd2_atapi_req_t*) map->addr;
	if (copy_from_user (&areq->status, buf, size)) {
	    res    = -EFAULT;
	    status = VBD2_STATUS_ERROR;
	} else if (atapi->req.cdb [0] == TEST_UNIT_READY) {
		/* Check if extent needs to be connected/disconnected */
	    vbd_atapi_vdisk_extent_check_tur (vd, areq);
	}
	vbd_fast_map_unmap (map);		/* Preemption is enabled */
	vbd_link_fast_map_free (bl, map);
    } else
#endif
    {
	areq = (vbd2_atapi_req_t*) nkops.nk_mem_map (paddr, size);
	++bl->stats.nk_mem_map.probes;
	if (!areq) {
	    ETRACE_VDISK (vd, "nk_mem_map(0x%llx, 0x%llx) failure\n",
			  (long long) paddr, (long long) size);
	    atapi->count = 0; /* Release process request */
	    vbd_link_resp (bl, atapi->req.req, VBD2_STATUS_ERROR);
	    return -EFAULT;
	}
	if (copy_from_user (&areq->status, buf, size)) {
	    res    = -EFAULT;
	    status = VBD2_STATUS_ERROR;
	} else if (atapi->req.cdb [0] == TEST_UNIT_READY) {
		/* Check if extent needs to be connected/disconnected */
	    vbd_atapi_vdisk_extent_check_tur (vd, areq);
	}
	nkops.nk_mem_unmap (areq, paddr, size);
    }
    atapi->count = 0;  /* Release process request */
	/* Reply to front-end */
    vbd_link_resp (bl, atapi->req.req, status);
    return res;
}

    static int
vbd_atapi_data_proc_open (struct inode* inode, struct file* file)
{
    vbd_vdisk_t*	vd = PDE (inode)->data;
    vbd_atapi_t*	atapi = vd->atapi;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    MOD_INC_USE_COUNT;
#endif
    atapi->data_open++;
    return 0;
}

    static int
vbd_atapi_data_proc_release (struct inode* inode, struct file* file)
{
    vbd_vdisk_t*	vd = PDE (inode)->data;
    vbd_atapi_t*	atapi = vd->atapi;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    MOD_DEC_USE_COUNT;
#endif
    atapi->data_open--;
    return 0;
}

    static loff_t
vbd_atapi_data_proc_lseek (struct file* file, loff_t off, int whence)
{
    loff_t new;

    switch (whence) {
    case 0:  new = off; break;
    case 1:  new = file->f_pos + off; break;
    case 2:
    default: return -EINVAL;
    }
    if (new > VBD2_ATAPI_REP_SZ) {
	return -EINVAL;
    }
    return (file->f_pos = new);
}

    static ssize_t
vbd_atapi_data_proc_write (struct file* file, const char* buf, size_t size,
			   loff_t* ppos)
{
    vbd_vdisk_t*	vd = PDE (file->f_dentry->d_inode)->data;
    vbd_link_t*		bl = vd->bl;
    vbd_atapi_t*	atapi = vd->atapi;
    ssize_t		res;
    NkPhAddr		paddr;
    void*		vaddr;
#ifdef VBD2_FAST_MAP
    vbd_fast_map_t*	map;
#endif

    if (!atapi->count || (*ppos + size > VBD2_ATAPI_REP_SZ)) {
	return -EINVAL;
    }
    if (!size) {
	return 0;
    }
    res    = size;
    paddr  = atapi->req.paddr + 0x1000; /* ATAPI reply is on next page */

#ifdef VBD2_FAST_MAP
    if ((map = vbd_link_fast_map_alloc (bl)) != NULL) {
	char* src = (char*) buf;

	while (size) {
	    size_t csize = (~VBD_PAGE_64_MASK) + 1; /* Page size */
	    _Bool  first = TRUE;

	    vbd_fast_map_map (map, paddr);	/* Preemption is disabled */
	    ++bl->stats.fast_map.probes;
	    vaddr = map->addr;
	    if (size < csize) {
		csize = size;
	    }
	    if (copy_from_user (vaddr, src, csize)) {
		res = -EFAULT;
		vbd_fast_map_unmap (map);	/* Preemption is enabled */
		break;
	    }
	    if (first &&
		(atapi->req.cdb [0] == GET_EVENT_STATUS_NOTIFICATION)) {
		    /* Check if extent needs to be connected/disconnected */
		if (vbd_atapi_vdisk_extent_check_gesn (vd, vaddr)) {
		    res = -EAGAIN; /* Media event needed */
		}
	    }
	    first  = FALSE;
	    vbd_fast_map_unmap (map);	/* Preemption is enabled */
	    size  -= csize;
	    src   += csize;
	    paddr += csize;
	}
	vbd_link_fast_map_free (bl, map);
    } else
#endif
    {
	vaddr = nkops.nk_mem_map (paddr, size);
	++bl->stats.nk_mem_map.probes;
	if (!vaddr) {
	    ETRACE_VDISK (vd, "nk_mem_map(0x%llx, 0x%llx) failure\n",
			  (long long) paddr, (long long) size);
	    return -EFAULT;
	}
	if (copy_from_user (vaddr, buf, size)) {
	    res = -EFAULT;
	} else if (atapi->req.cdb [0] == GET_EVENT_STATUS_NOTIFICATION) {
		/* Check if extent needs to be connected/disconnected */
	    if (vbd_atapi_vdisk_extent_check_gesn (vd, (uint8_t*) vaddr)) {
		res = -EAGAIN; /* Media event needed */
	    }
	}
	nkops.nk_mem_unmap (vaddr, paddr, size);
    }
    return res;
}

    static ssize_t
vbd_atapi_data_proc_read (struct file* file, char* buf, size_t count,
			  loff_t* ppos)
{
    (void) file;
    (void) buf;
    (void) count;
    (void) ppos;
    return -EPERM;
}

    static void
vbd_link_atapi (vbd_vdisk_t* vd, vbd2_req_header_t* req)
{
    vbd_link_t*		bl = vd->bl;
    vbd_atapi_t*	atapi;
    vbd2_atapi_req_t*	areq;
    NkPhAddr		paddr;
#ifdef VBD2_FAST_MAP
    vbd_fast_map_t*	map;
#endif

    if (req->count != 1) {
	EFTRACE ("Invalid request count\n");
	goto error;
    }
    if (!(atapi = vd->atapi)) {
	EFTRACE ("%s not an ATAPI device\n", vd->name);
	goto error;
    }
	/* Check if user process is present */
    if (!atapi->ctrl_open || !atapi->data_open) {
	EFTRACE ("User process not present\n");
	goto error;
    }
	/* Only one request at a time */
    mutex_lock (&atapi->lock);
    if (atapi->count) {
	mutex_unlock (&atapi->lock);
	EFTRACE ("too many requests\n");
	goto error;
    }
    atapi->count = 1;	/* Get the process request */
    mutex_unlock (&atapi->lock);

	/* Map the front-end's ATAPI request */
    paddr = *VBD2_FIRST_BUF (req);
    paddr = VBD2_BUF_BASE (paddr);
#ifdef VBD2_FAST_MAP
    if ((map = vbd_link_fast_map_alloc (bl)) != NULL) {
	unsigned int off = (paddr & ~VBD_PAGE_64_MASK);
	vbd_fast_map_map (map, paddr);
	    /* Now preemption is disabled */
	areq = (vbd2_atapi_req_t*) ((unsigned long) map->addr + off);
	++bl->stats.fast_map.probes;
    } else
#endif
    {
	areq = (vbd2_atapi_req_t*) nkops.nk_mem_map (paddr,
						     sizeof (vbd2_atapi_req_t));
	++bl->stats.nk_mem_map.probes;
    }
    if (!areq) {
	ETRACE_VDISK (vd, "nk_mem_map(0x%llx, 0x%llx) failure\n",
		      (long long) paddr, (long long) sizeof (vbd2_atapi_req_t));
	atapi->count = 0;  /* Release the process request */
	goto error;
    }
	/* Prepare the user process' request */
    atapi->req.req    = req;
    atapi->req.devid  = req->devid;
    atapi->req.paddr  = paddr;
    memcpy (atapi->req.cdb, areq->cdb, VBD2_ATAPI_PKT_SZ);
    atapi->req.buflen = areq->buflen;

	/* Unmap the front-end's ATAPI request */
#ifdef VBD2_FAST_MAP
    if (map) {
	vbd_fast_map_unmap (map);
	vbd_link_fast_map_free (bl, map);
    } else
#endif
    {
	nkops.nk_mem_unmap (areq, paddr, sizeof (vbd2_atapi_req_t));
    }
	/* Wake up the user process */
    wake_up_interruptible (&atapi->wait);
    return;	/* No reply yet */

error:
    vbd_link_resp (bl, req, VBD2_STATUS_ERROR);
}
#endif	/* VBD2_ATAPI */

    static void
vbd_vdisk_probe_op (vbd_vdisk_t* vd, vbd_vdisk_probe_op_t probe_op,
		    vbd2_req_header_t* req)
{
    vbd2_probe_link_t*	probe = (vbd2_probe_link_t*) req;
    vbd2_status_t	status;

	/* Call request-specific operation to fill in the vbd2_probe_t data */
    status = probe_op (vd, req, probe->probe,
		       req->count * sizeof (vbd2_probe_t));
    vbd_link_resp (vd->bl, req, status);
}

/*----- Virtual disk -----*/

#ifdef VBD2_ATAPI
static _Bool __init vbd_atapi_vdisk_init (vbd_vdisk_t* vd);
#endif

    /* Called from vbd_init() only */
    /* A vdisk is created without extents */

    static int __init
vbd_be_vdisk_create (vbd_be_t* be, const vbd_prop_vdisk_t* prop)
{
    vbd_link_t*		bl;
    vbd_vdisk_t*	vd;
    vbd_vdisk_t**	link2;

    bl = vbd_links_find_osid (be->links, prop->owner);
    if (!bl) {
	ETRACE ("no link to %d found\n", prop->owner);
	return -ESRCH;
    }
    VBD_LINK_FOR_ALL_VDISKS (vd, bl) {
	if (vd->devid == prop->devid) {
		/* Take the strongest wait mode among vbd2= parameters */
	    if (prop->wait > vd->wait) {
		vd->wait = prop->wait;
	    }
	    return 0;		/* Vdisk already exists */
	}
    }
    vd = (vbd_vdisk_t*) kzalloc (sizeof (vbd_vdisk_t), GFP_KERNEL);
    if (unlikely (!vd)) {
	ETRACE ("out of memory for vdisk\n");
	return -ENOMEM;
    }
    vbd_vdisk_init (vd, prop->tag, prop->devid, bl, prop->wait);

#ifdef VBD2_ATAPI
    if (!vbd_atapi_vdisk_init (vd)) {
	    /* Message already issued */
	vbd_vdisk_free (vd);
	return -EINVAL;
    }
#endif
    link2 = &bl->vdisks;
    while (*link2) link2 = &(*link2)->next;
    *link2 = vd;

    TRACE ("%s created", vd->name);
#ifdef VBD2_ATAPI
    if (vd->atapi) {
	printk (" - ATAPI ctrl: %s data: %s",
		vd->atapi->ctrl_name, vd->atapi->data_name);
    }
#endif
    printk ("\n");
	/* Do not signal the backend as the vdisk has no extents yet */
    return 0;
}

    /*
     *  On 2.6.x we can have zero-sized peripherals,
     *  typically initially empty loopback devices.
     */

    /* vdisk does not need to be open */

    static vbd_sector_t
vbd_vdisk_sectors (const vbd_vdisk_t* vd)
{
    vbd_sector_t	sectors = 0;
    vbd_extent_t*	ex;

    VBD_VDISK_FOR_ALL_EXTENTS (ex, vd) {
	sectors += ex->sectors;
    }
    return sectors;
}

    /* vdisk does not need to be open */

    static _Bool
vbd_vdisk_openable (const vbd_vdisk_t* vd)
{
    vbd_extent_t*	ex;

    VBD_VDISK_FOR_ALL_EXTENTS (ex, vd) {
	if (!ex->openable) return FALSE;
    }
    return TRUE;
}

    /* vdisk does not need to be open */

    static _Bool
vbd_vdisk_readonly (const vbd_vdisk_t* vd)
{
    const vbd_extent_t* ex;

    VBD_VDISK_FOR_ALL_EXTENTS (ex, vd) {
	if (ex->access & VBD_DISK_ACC_W) {
	    return FALSE;
	}
    }
    return TRUE;
}

static void vbd_extent_close (vbd_extent_t*);

    /* Idempotent */

    static void
vbd_vdisk_close (vbd_vdisk_t* vd)
{
    vbd_extent_t* ex;

    VBD_VDISK_FOR_ALL_EXTENTS (ex, vd) {
	vbd_extent_close (ex);
    }
    vd->open = FALSE;
}

static _Bool vbd_extent_open (vbd_extent_t*);

    /* Idempotent */
    /* Called from blkio thread only */

    static void
vbd_vdisk_open (vbd_vdisk_t* vd)
{
    vbd_extent_t* ex;

    if (vd->open) return;
    VBD_VDISK_FOR_ALL_EXTENTS (ex, vd) {
	if (!vbd_extent_open (ex)) {
	    vbd_vdisk_close (vd);
	    return;
	}
    }
    vd->open = TRUE;
}

    /* vdisk does not need to be open */

    static void
vbd_vdisk_probe (vbd_vdisk_t* vd, vbd2_probe_t* probe)
{
    const vbd_prop_extent_t* prop =
	vbd_be_prop_extent_lookup (vd->bl->be, vd->tag);

    probe->sectors = vbd_vdisk_sectors (vd);
    probe->devid   = vd->devid;
    probe->genid   = vd->genid;
    if (prop && (prop->major == FLOPPY_MAJOR)) {
	    /* Vdisk is a floppy */
	probe->info = VBD2_TYPE_FLOPPY;
    } else {
	    /* Vdisk is a hard disk */
	probe->info = VBD2_TYPE_DISK;
    }
    if (prop && (prop->major == LOOP_MAJOR)) {
	    /* Vdisk is connected to a loop device */
	probe->info |= VBD2_FLAG_VIRT;
    }
    if (vbd_vdisk_readonly (vd)) {
	probe->info |= VBD2_FLAG_RO;
    }
    XTRACE ("%s, %lld sectors, flags 0x%x\n", vd->name,
	    (long long) probe->sectors, probe->info);
}

    static vbd_extent_t*
vbd_vdisk_translate (vbd_vdisk_t* vd, nku32_f acc, vbd2_sector_t vsec,
		     vbd_sector_t* psec)
{
    vbd_extent_t* ex;

    VBD_VDISK_FOR_ALL_EXTENTS (ex, vd) {
	if (vsec < ex->sectors) {
	    break;
	}
	vsec -= ex->sectors;
    }
    if (!ex) {
	DTRACE_VDISK (vd, "extent not found (%s extents)\n",
		      vd->extents ? "but do have" : "no");
	return NULL;
    }
    if (!(ex->access & acc)) {
	return NULL;
    }
    *psec = (ex->start + vsec);
    return ex;
}

#ifdef VBD2_ATAPI
static void VBD_EXIT vbd_atapi_vdisk_free (vbd_vdisk_t* vd);
#endif

    /*
     * Called from vbd_link_destroy() <- from vbd_be_destroy() <- vbd_exit().
     * Called from vbd_be_vdisk_create() <- vbd_init().
     */

    static void
vbd_vdisk_free (vbd_vdisk_t* vd)
{
    vbd_extent_t* ex = vd->extents;

    while (ex) {
	vbd_extent_t* prev = ex;

	ex = ex->next;
	kfree (prev);
    }
    vd->extents = NULL;
#ifdef VBD2_ATAPI
    vbd_atapi_vdisk_free (vd);
#endif
    kfree (vd);
}

/*----- Extent management -----*/

    static inline int
vbd_access_to_fmode (const nku32_f access)
{
    return access == VBD_DISK_ACC_R ? FMODE_READ : FMODE_WRITE;
}

    /* Idempotent */

    static void
vbd_extent_close (vbd_extent_t* ex)
{
    if (VBD_BDEV_OPEN (ex->bdev)) {
	VBD_BDEV_CLOSE (ex->bdev, vbd_access_to_fmode (ex->access));
	ex->bdev = NULL;
    }
}

    /* Called from vbd_be_extent_create() <- vbd_init() */
    /* Called from vbd_vdisk_open() <- vbd_vdisk_op() <-
     *		vbd_link_do_blkio_op() <- vbd_blkio_thread() */

    static _Bool
vbd_extent_open (vbd_extent_t* ex)
{
    vbd_sector_t sectors;

	/* Force the extent to start at the beginning of the block device */
    ex->start = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    ex->bdev = blkdev_get_by_dev (ex->dev, vbd_access_to_fmode (ex->access), ex);
    if (IS_ERR (ex->bdev)) {
	    /* ENXIO is "No such device or address" */
	if (PTR_ERR (ex->bdev) == -ENXIO) {
	    DTRACE_VDISK (ex->vdisk, "open_by_devnum(%d,%d) failed (%ld).\n",
			  ex->prop->major, ex->prop->minor, PTR_ERR (ex->bdev));
	} else {
	    ETRACE_VDISK (ex->vdisk, "open_by_devnum(%d,%d) failed (%ld).\n",
			  ex->prop->major, ex->prop->minor, PTR_ERR (ex->bdev));
	}
	return FALSE;
    }
    if (!ex->bdev->bd_disk) {
	ETRACE_VDISK (ex->vdisk, "bd_disk(%d,%d) is NULL.\n",
		      ex->prop->major, ex->prop->minor);
	VBD_BDEV_CLOSE (ex->bdev, vbd_access_to_fmode (ex->access));
	return FALSE;
    }
	/* Get block device size in sectors (512 bytes) */
    if (ex->bdev->bd_part) {
	sectors = ex->bdev->bd_part->nr_sects;
    } else {
	sectors = get_capacity (ex->bdev->bd_disk);
    }
	/*
	 * Work-around strange kernel behavior where the
	 * block device bd_inode->i_size is not updated when inserting
	 * a media (maybe due to using: open_by_devnum()?).
	 * This leads to common block device code to fail in
	 * checking submitted bio(s) accessing sectors beyond 0x1fffff,
	 * on /dev/sr0 (sr initializes bd_inode->i_size with that value).
	 */
    if (sectors != (i_size_read (ex->bdev->bd_inode) >> 9)) {
	    /* Update bdev inode size to make blk-core.c happy */
	DTRACE_VDISK (ex->vdisk,
	    "device (%d,%d): hacking bdev inode size (%llu -> %llu).\n",
	    ex->prop->major, ex->prop->minor,
	    i_size_read (ex->bdev->bd_inode), (loff_t) sectors << 9);
	i_size_write (ex->bdev->bd_inode, (loff_t) sectors << 9);
    }
#else
    if (!blk_size [MAJOR (ex->dev)]) {
	ETRACE_VDISK (ex->vdisk, "blk_size(%d,%d) is NULL.\n",
		      ex->prop->major, ex->prop->minor);
	return FALSE;
    }
	/* Convert blocks (1KB) to sectors */
    sectors = blk_size [MAJOR (ex->dev)] [MINOR (ex->dev)] * 2;
#endif
    DTRACE_VDISK (ex->vdisk,
	"extent size requested %llu actual %llu sectors, access %i\n",
	(unsigned long long) ex->sectors, (long long) sectors, ex->access);
	/* NB: this test assumes ex->start == 0 */
    if (!ex->sectors || (ex->sectors > sectors)) {
	ex->sectors = sectors;
    }
    ex->openable = TRUE;
    return TRUE;
}

/*----- Event thread -----*/

typedef struct {
    nku32_f		tag;	/* Vdisk tag */
    vbd_vdisk_t*	vd;
} vbd_link_find_tag_t;

    /* Called by vbd_be_lookup_vd_by_prop() only */

    static _Bool __init
vbd_link_match_tag (vmq_link_t* link2, void* cookie)
{
    vbd_link_find_tag_t*	ctx = (vbd_link_find_tag_t*) cookie;
    vbd_vdisk_t*		vd;

    VBD_LINK_FOR_ALL_VDISKS (vd, VBD_LINK (link2)) {
	if (vd->tag == ctx->tag) {
	    ctx->vd = vd;
	    return true;
	}
    }
    return false;
}

    /*
     * Only called from vbd_be_extent_create() <- vbd_init().
     */

    static vbd_vdisk_t* __init
vbd_be_lookup_vd_by_prop (vbd_be_t* be, const vbd_prop_extent_t* prop)
{
    vbd_link_find_tag_t	ctx;

    ctx.tag = prop->tag;
    if (!vmq_links_iterate (be->links, vbd_link_match_tag, &ctx)) return NULL;
    return ctx.vd;
}

    /*
     * Return TRUE if ok, FALSE on error.
     * Called only from vbd_be_extent_create() <- vbd_init().
     */

    static _Bool __init
vbd_vdisk_extent_create (vbd_vdisk_t* vd, const vbd_prop_extent_t* prop)
{
    vbd_extent_t** last = &vd->extents;
    vbd_extent_t*  ex;

    if (!prop || !vd) {
	return FALSE;
    }
    ex = (vbd_extent_t*) kzalloc (sizeof (vbd_extent_t), GFP_KERNEL);
    if (!ex) {
	ETRACE_VDISK (vd, "extent allocation failure\n");
	return FALSE;
    }
    vbd_extent_init (ex, 0, prop->sectors, MKDEV (prop->major, prop->minor),
		     prop->access, prop, vd);
    if (vbd_extent_open (ex)) {
	DTRACE_VDISK (vd, "could be opened, %lld sectors.\n",
		      (long long) ex->sectors);
	vbd_extent_close (ex);
    }
    while (*last) last = &(*last)->next;
    *last = ex;
    return TRUE;
}

    /* Called from vbd_init() only */

    static _Bool __init
vbd_be_extent_create (vbd_be_t* be, const vbd_prop_extent_t* prop)
{
    vbd_vdisk_t* vd;

    vd = vbd_be_lookup_vd_by_prop (be, prop);
    if (!vd) return FALSE;
    return vbd_vdisk_extent_create (vd, prop);
}

    /* This function must not issue error traces */

    static long long
vbd_prop_extent_sectors (const vbd_prop_extent_t* pe)
{
    vbd_sector_t		sectors;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    struct block_device*	bdev;

    bdev = blkdev_get_by_dev (MKDEV (pe->major, pe->minor),
			      vbd_access_to_fmode (pe->access), (void*)pe);
    if (IS_ERR (bdev)) return -1;
    if (!bdev->bd_disk) {
	VBD_BDEV_CLOSE (bdev, vbd_access_to_fmode (pe->access));
	return -1;
    }
    if (bdev->bd_part) {
	sectors = bdev->bd_part->nr_sects;
    } else {
	sectors = get_capacity (bdev->bd_disk);
    }
    VBD_BDEV_CLOSE (bdev, vbd_access_to_fmode (pe->access));
#else
    if (!blk_size [pe->major]) return -1;
    sectors = blk_size [pe->major] [pe->minor] * 2;
#endif
    return sectors;
}

    static _Bool
vbd_vdisk_recheck_extents (vbd_vdisk_t* vd)
{
    _Bool		changes = FALSE;
    _Bool		new_disk = FALSE;
    vbd_extent_t*	ex;

    VBD_VDISK_FOR_ALL_EXTENTS (ex, vd) {
	const long long		sectors = vbd_prop_extent_sectors (ex->prop);
	const _Bool		old_openable = ex->openable;
	const vbd_sector_t	old_sectors  = ex->sectors;

	if (sectors >= 0) {
	    ex->openable = TRUE;
	    ex->sectors  = sectors;
	} else {
	    ex->openable = FALSE;
	    ex->sectors  = 0;
	}
	if (ex->openable != old_openable ||
	    ex->sectors  != old_sectors) {
	    DTRACE_VDISK (vd, "openable/sectors: %d/%lld => %d/%lld\n",
			  old_openable, (long long) old_sectors,
			  ex->openable, (long long) ex->sectors);
	    changes = TRUE;
	    if (old_openable && !ex->openable) {
		new_disk = TRUE;
	    }
	}
    }
	/* If disk lost, bump genid */
    if (new_disk) {
	++vd->genid;
	DTRACE_VDISK (vd, "lost disk, bumping genid to %d\n", vd->genid);
	vbd_vdisk_update_name (vd);
    }
    return changes;
}

static void	vbd_link_signal_changes (vbd_link_t*);

    static _Bool
vbd_link_recheck_vdisks (vmq_link_t* link2, void* cookie)
{
    vbd_link_t*		bl = VBD_LINK (link2);
    _Bool		changes = FALSE;
    vbd_vdisk_t*	vd;

    (void) cookie;
    VBD_LINK_FOR_ALL_VDISKS (vd, bl) {
	if (vbd_vdisk_recheck_extents (vd)) {
	    changes = TRUE;
	}
    }
    if (changes) {
	vbd_link_signal_changes (bl);
    }
    return false;	/* Do not interrupt scanning */
}

    static void
vbd_be_recheck_vdisks (vbd_be_t* be)
{
    vmq_links_iterate (be->links, vbd_link_recheck_vdisks, NULL);
}

#ifdef VBD_UEVENT
    static int
vbd_uevent_recv (vbd_be_t* be)
{
    int	diag;

    diag = vlx_uevent_recv (&be->uevent, be->event_thread.buf,
			    sizeof be->event_thread.buf);
    if (diag < 0) return diag;
    DTRACE ("msg: '%s' (%d bytes)\n", be->event_thread.buf, diag);
    return strstr (be->event_thread.buf, "/block/") ? diag : 0;
}
#endif

    static int
vbd_event_thread (void* arg)
{
    vbd_be_t*		be = (vbd_be_t*) arg;
    _Bool		had_msg = 0;
    _Bool		polling_needed = vbd_be_vdisk_polling_needed (be);
    DECLARE_WAITQUEUE	(wait, current);

    add_wait_queue (&be->event_thread.wait, &wait);
    while (!be->event_thread.abort) {
	int diag = 0;
	    /*
	     *  We sometimes wait until a disk size is non-zero,
	     *  which requires periodic polling, so we setup a
	     *  sleep with timeout.
	     */
	set_current_state (TASK_INTERRUPTIBLE);
	if (be->event_thread.recheck_extents || polling_needed) {
	    be->event_thread.recheck_extents = FALSE;
	    vbd_be_recheck_vdisks (be);
	    polling_needed = vbd_be_vdisk_polling_needed (be);
	}
	(void) diag;
#ifdef VBD_UEVENT
	if ((diag = vbd_uevent_recv (be)) < 0)
#endif
	{
	    if (had_msg) {
		had_msg = 0;
		DTRACE ("no msgs (%d)\n", diag);
	    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
	    try_to_freeze();
#endif
	    if (polling_needed || diag != -EAGAIN) {
		schedule_timeout (HZ);
	    } else {
		schedule();
	    }
	}
#ifdef VBD_UEVENT
	else if (diag) {
	    if (!had_msg) {
		had_msg = 1;
		DTRACE ("got msg\n");
	    }
	    vbd_be_recheck_vdisks (be);
	    polling_needed = vbd_be_vdisk_polling_needed (be);
	}
#endif
    }
    remove_wait_queue (&be->event_thread.wait, &wait);
    set_current_state (TASK_RUNNING);
    return 0;
}

#ifdef VBD_UEVENT
    static void
vbd_uevent_data_ready (vlx_uevent_t* ue, int bytes)
{
    vbd_be_t* be = container_of (ue, vbd_be_t, uevent);

    (void) bytes;
    wake_up_interruptible (&be->event_thread.wait);
}
#endif

    static void
vbd_vdisk_recheck_extents_async (vbd_vdisk_t* vd)
{
    vbd_be_t* be = vd->bl->be;

    DTRACE_VDISK (vd, "\n");
    be->event_thread.recheck_extents = TRUE;
    wake_up_interruptible (&be->event_thread.wait);
}

/*----- Block I/O operations -----*/

    static inline _Bool
vbd_link_on_blkio_thread_list (vbd_link_t* bl)
{
    return bl->blkio_list.next != NULL;
}

    /* Called from vbd_blkio_thread() only */

    static void
vbd_link_remove_from_blkio_thread_list (vbd_link_t* bl)
{
    unsigned long flags;

    if (!vbd_link_on_blkio_thread_list (bl)) {
	return;
    }
    spin_lock_irqsave (&bl->be->blkio_thread.list_lock, flags);
    if (vbd_link_on_blkio_thread_list (bl)) {
	list_del (&bl->blkio_list);
	bl->blkio_list.next = NULL;
	vbd_link_put (bl);
    }
    spin_unlock_irqrestore (&bl->be->blkio_thread.list_lock, flags);
}

    /* Called when a link requires attention */

    static void
vbd_link_add_to_blkio_thread_list_tail (vbd_link_t* bl)
{
    unsigned long flags;

    if (vbd_link_on_blkio_thread_list (bl)) {
	    /* Already enlisted on vbd_be.blkio_thread.list */
	return;
    }
    spin_lock_irqsave (&bl->be->blkio_thread.list_lock, flags);
    if (!vbd_link_on_blkio_thread_list (bl))  {
	list_add_tail (&bl->blkio_list, &bl->be->blkio_thread.list);
	    /* Increment refcount */
	vbd_link_get (bl);
    }
    spin_unlock_irqrestore (&bl->be->blkio_thread.list_lock, flags);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    static vbd_pending_req_t*
vbd_link_get_from_done_list (vbd_link_t* bl)
{
    unsigned long	flags;
    vbd_pending_req_t*	req;

    spin_lock_irqsave (&bl->done_list.lock, flags);
    if (list_empty (&bl->done_list.head)) {
	req = 0;
    } else {
	struct list_head* head = bl->done_list.head.next;

	    /* Convert pointer to field into pointer to struct */
	req = list_entry (head, vbd_pending_req_t, list);
	list_del (head);
    }
    spin_unlock_irqrestore (&bl->done_list.lock, flags);
    return req;
}
#endif

    static void
vbd_be_maybe_wake_up_blkio_thread (vbd_be_t* be)
{
    XTRACE ("entered\n");
	/*
	 * Needed so that two processes, who together make the
	 * following predicate true, don't both read stale values
	 * and evaluate the predicate incorrectly.
	 */
    smp_mb();
    if (!list_empty (&be->blkio_thread.list)) {
	DTRACE ("wake_up\n");
	wake_up (&be->blkio_thread.wait);
    }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    static void
vbd_link_put_to_done_list (vbd_link_t* bl, vbd_pending_req_t* req)
{
    unsigned long	flags;
    _Bool		empty;

    spin_lock_irqsave (&bl->done_list.lock, flags);
    empty = list_empty (&bl->done_list.head);
    list_add_tail (&req->list, &bl->done_list.head);
    spin_unlock_irqrestore (&bl->done_list.lock, flags);
    if (empty) {
	vbd_link_add_to_blkio_thread_list_tail (bl);
	vbd_be_maybe_wake_up_blkio_thread (bl->be);
    }
}
#endif

static int  vbd_link_do_blkio_op (vbd_link_t* bl, int max_to_do);

    static int
vbd_blkio_thread (void* arg)
{
    vbd_be_t*		be = (vbd_be_t*) arg;
    DECLARE_WAITQUEUE	(wq, current);

    DTRACE ("starting\n");
    while (atomic_read (&be->refcount)) {
	    /* Wait for work to do */
	add_wait_queue (&be->blkio_thread.wait, &wq);
	set_current_state (TASK_INTERRUPTIBLE);

	if (be->resource_error) {
	    be->resource_error = FALSE;
	    ++be->stats.sleeps_with_timeout;
	    if (!schedule_timeout (HZ/10)) {
		++be->stats.timeouts;
	    }
	} else if (atomic_read (&be->refcount) &&
		list_empty (&be->blkio_thread.list) && !be->sysconf) {
	    ++be->stats.sleeps_no_timeout;
	    schedule();
	}
	__set_current_state (TASK_RUNNING);
	remove_wait_queue (&be->blkio_thread.wait, &wq);
	DTRACE ("wakeup\n");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
	    /* Check if freeze signal has been received */
	try_to_freeze();
#endif
	if (be->sysconf) {
	    be->sysconf = false;
	    vmq_links_sysconf (be->links);
	}
	    /* Queue up a batch of requests */
	while (!list_empty (&be->blkio_thread.list) && !be->resource_error) {
	    struct list_head*	ent = be->blkio_thread.list.next;
		/* Convert pointer to field into pointer to struct */
	    vbd_link_t*	bl = list_entry (ent, vbd_link_t, blkio_list);

	    vbd_link_get (bl);
	    vbd_link_remove_from_blkio_thread_list (bl);
		/*
		 *  In case of be->resource_error, vbd_link_do_blkio_op()
		 *  returns 0 even though "bl" still requires
		 *  attention, so check for that specifically.
		 */
	    if (vbd_link_do_blkio_op (bl, VBD_LINK_MAX_REQ) ||
		be->resource_error) {
		    /*
		     *  We have processed all the VBD_LINK_MAX_REQ
		     *  quota of requests, so there can be more, so we
		     *  put back "bl" on the list, at the end/tail.
		     */
		DTRACE ("Re-adding to list tail\n");
		vbd_link_add_to_blkio_thread_list_tail (bl);
	    }
	    vbd_link_put (bl);
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	    /* Push the batch through to disk */
	run_task_queue (&tq_disk);
#endif
    }
    DTRACE ("exiting\n");
    return 0;
}

    static void
vbd_req_blkio_done (vbd_pending_req_t *req)
{
    vbd_link_t*	bl = req->bl;

    vbd_link_resp (bl, req->req,
		   req->error ? VBD2_STATUS_ERROR : VBD2_STATUS_OK);
    vbd_link_put (bl);
    kmem_cache_free (bl->be->pending_req_cachep, req);
    vbd_be_maybe_wake_up_blkio_thread (bl->be);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

    /* Called from vbd_bh_end_blkio_op() */

    static void
vbd_req_end_blkio_op (vbd_pending_req_t* xreq, _Bool uptodate)
{
    if (!uptodate) {
	xreq->error = TRUE;
    }
	/* old code: if (!atomic_sub_return (1, &xreq->pendcount)) */
    if (atomic_sub_and_test (1, &xreq->pendcount)) {
	vbd_req_blkio_done (xreq);
    }
}

    /*
     *  This is a callback set up in vbd_vdisk_rw(),
     *  through the "bh->b_end_io" field.
     *  (In 2.6, we have "bio->bi_end_io")
     */

    static void
vbd_bh_end_blkio_op (struct buffer_head* bh, int uptodate)
{
    vbd_pending_req_t* xreq = bh->b_private;

    vbd_req_end_blkio_op (xreq, uptodate);
    nkops.nk_mem_unmap (bh->b_data, __pa (bh->b_data), bh->b_size);
    kmem_cache_free (xreq->bl->be->buffer_head_cachep, bh);
}

#else	/* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
    /*
     * Since 2.6.25 blk_get_queue and blk_put_queue are not exported
     * anymore. It seems the VBD code should be changed to get rid of
     * these functions.
     */

    static inline int
vbd_blk_get_queue (struct request_queue* q)
{
    if (likely (!test_bit (QUEUE_FLAG_DEAD, &q->queue_flags))) {
	kobject_get (&q->kobj);
	return 0;
    }
    return 1;
}

    static inline void
vbd_blk_put_queue (struct request_queue* q)
{
    kobject_put (&q->kobj);
}

#define blk_get_queue vbd_blk_get_queue
#define blk_put_queue vbd_blk_put_queue
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) */

    static void
vbd_req_submit_bio (vbd_pending_req_t* req, struct bio* bio)
{
    struct request_queue* q;

	/*
	 *  If __get_free_page() fails in vbd2_dma=0 mode,
	 *  we can be called with bio->bi_size=0. We must
	 *  not call submit_bio() then, because the routine
	 *  will panic.
	 *  xreq->error should be TRUE already then.
	 */
    if (!bio->bi_size) {
	bio_put (bio);
	return;
    }
    q = bdev_get_queue (bio->bi_bdev);
    if (q) {
	blk_get_queue (q);
	atomic_inc (&req->pendcount);
	submit_bio (bio->bi_rw, bio);
	blk_put_queue (q);
    } else {
	req->error = TRUE;
    }
}

    /* Called only from vbd_req_end_blkio_op() */

#ifdef VBD2_FAST_MAP
    static void
vbd_req_end_blkio_op_read_fast (vbd_pending_req_t* req, vbd_fast_map_t* map)
{
    vbd_pending_seg_t* seg   = req->segs;
    vbd_pending_seg_t* limit = req->segs + req->nsegs;

    while (seg != limit) {
	if (seg->vaddr) {
	    const unsigned int  off = (seg->gaddr & ~VBD_PAGE_64_MASK);
	    const unsigned long dst = (unsigned long) map->addr + off;
	    const unsigned long src = seg->vaddr + off;

	    vbd_fast_map_map (map, seg->gaddr & VBD_PAGE_64_MASK);
	    memcpy ((void*) dst, (void*) src, seg->size);
	    vbd_cache_clean ((const char*) dst, seg->size);
	    vbd_fast_map_unmap (map);
	    free_page (seg->vaddr);
		/* Collect statistics */
	    ++req->bl->stats.fast_map.reads;
	    req->bl->stats.fast_map.read_bytes += seg->size;
	    ++req->bl->stats.page_freeings;
	    --req->bl->stats.alloced_pages;
	}
	seg++;
    }
    vbd_req_blkio_done (req);
}
#endif

    static void
vbd_req_end_blkio_op (vbd_pending_req_t* req)
{
    if (!atomic_sub_return (1, &req->pendcount)) {
	if (req->op == VBD2_OP_READ || req->op == VBD2_OP_READ_EXT) {
#ifdef VBD2_FAST_MAP
	    vbd_link_t*		bl  = req->bl;
	    vbd_fast_map_t*	map = vbd_link_fast_map_alloc (bl);

	    if (map) {
		vbd_req_end_blkio_op_read_fast (req, map);
		vbd_link_fast_map_free (bl, map);
	    } else
#endif
	    {
		vbd_link_put_to_done_list (req->bl, req);
	    }
	} else {
	    vbd_pending_seg_t* seg   = req->segs;
	    const vbd_pending_seg_t* limit = req->segs + req->nsegs;

	    while (seg != limit) {
		if (seg->vaddr) {
		    free_page (seg->vaddr);
		    ++req->bl->stats.page_freeings;
		    --req->bl->stats.alloced_pages;
		}
		seg++;
	    }
	    vbd_req_blkio_done (req);
	}
    }
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
    static int
vbd_bio_end_blkio_op (struct bio* bio, unsigned int done, int error)
{
    vbd_pending_req_t* req = bio->bi_private;

    if (!done || error) {
	req->error = TRUE;
    }
    vbd_req_end_blkio_op (req);
    bio_put (bio);
    return error;
}
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24) */
    static void
vbd_bio_end_blkio_op (struct bio* bio, int error)
{
    vbd_pending_req_t* req = bio->bi_private;

    if (error) {
	req->error = TRUE;
    }
    vbd_req_end_blkio_op (req);
    bio_put (bio);
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24) */
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) */

    /*
     * Called only by vbd_link_do_blkio_op() <- vbd_blkio_thread().
     * Return value of TRUE indicates that the "req" has been
     * consumed and does not need to be resubmitted.
     */

    static _Bool
vbd_vdisk_rw (vbd_vdisk_t* vd, vbd2_req_header_t* req)
{
    const _Bool		is_read = req->op == VBD2_OP_READ ||
				  req->op == VBD2_OP_READ_EXT;
    const _Bool		is_ext  = req->op == VBD2_OP_WRITE_EXT ||
				  req->op == VBD2_OP_READ_EXT;
    vbd_link_t*		bl = vd->bl;
    const nku32_f	acc = is_read ? VBD_DISK_ACC_R : VBD_DISK_ACC_W;
    vbd_pending_req_t*	xreq;
    vbd_extent_t*	ex;
    unsigned int	seg;
    unsigned int	count;

    XTRACE ("%s req %d sector %lld count %d\n", vd->name, req->op,
	    req->sector, req->count);
    ++bl->stats.requests;

    count = req->count;
    if (!count) {
	count = 256;
    }
    if (bl->pending_xreq) {
	xreq = bl->pending_xreq;
	bl->pending_xreq = NULL;
    } else {
#ifdef VBD2_FI
	static int xreq_allocs;

	if (!(xreq_allocs++ % 5)) {
	    xreq = 0;
	} else
#endif
	xreq = (vbd_pending_req_t*)
	    kmem_cache_alloc (bl->be->pending_req_cachep, GFP_ATOMIC);

	if (unlikely (!xreq)) {
	    WTRACE_VDISK (vd, "sector %lld, out of descriptors; will retry\n",
			  req->sector);
	    bl->be->resource_error = TRUE;
	    bl->pending_msg = req;
	    ++bl->stats.resource_errors;
	    return FALSE;	/* "req" not consumed */
	}
	vbd_pending_req_init (xreq, bl, req);
    }
    vbd_link_get (bl);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    for (seg = 0; seg < count; seg++) {
	NkPhAddr	paddr;
	nku32_f		psize;
	vbd_sector_t	sectors;
	struct buffer_head* bh;
	void*		vaddr;
	vbd_sector_t	psector = 0;
	const int	operation = (acc == VBD_DISK_ACC_W ? WRITE : READ);

	if (is_ext) {
	    ETRACE_VDISK (vd, "op %d not supported\n", xreq->op);
	    xreq->error = TRUE;
	    break;
	}
	paddr = *(VBD2_FIRST_BUF (req) + seg);

	XTRACE (" 0x%llx:%lld:%lld\n",
		VBD2_BUF_PAGE (paddr),
		VBD2_BUF_SSECT (paddr),
		VBD2_BUF_ESECT (paddr));

	sectors = VBD2_BUF_SECTS (paddr);
	psize   = VBD2_BUF_SIZE (paddr);
	paddr   = VBD2_BUF_BASE (paddr);

	++bl->stats.segments;

	ex = vbd_vdisk_translate (vd, acc, xreq->vsector, &psector);
	if (unlikely (!ex)) {
	    ETRACE_VDISK (vd, "sector %lld translation failure\n",
			  xreq->vsector);
	    xreq->error = TRUE;
	    break;
	}
	bh = kmem_cache_alloc (bl->be->buffer_head_cachep, GFP_ATOMIC);
	if (unlikely (!bh)) {
	    ETRACE_VDISK (vd, "buffer header allocation failure\n");
	    xreq->error = TRUE;
	    break;
	}
	vaddr = nkops.nk_mem_map (paddr, psize);
	if (!vaddr) {
	    kmem_cache_free (bl->be->buffer_head_cachep, bh);
	    ETRACE_VDISK (vd, "nk_mem_map(0x%llx, 0x%x) failure\n",
		    (long long) paddr, psize);
	    xreq->error = TRUE;
	    break;
	}
	memset (bh, 0, sizeof *bh);

	init_waitqueue_head (&bh->b_wait);
	bh->b_size    = psize;
	bh->b_dev     = ex->dev;
	bh->b_rdev    = ex->dev;
	bh->b_rsector = (unsigned long) psector;
	bh->b_data    = (char*) vaddr;
	bh->b_page    = virt_to_page (vaddr);
	bh->b_end_io  = vbd_bh_end_blkio_op;
	bh->b_private = xreq;
	bh->b_state   = ((1 << BH_Mapped) | (1 << BH_Lock) |
			 (1 << BH_Req)    | (1 << BH_Launder));
	if (operation == WRITE)
	    bh->b_state |= (1 << BH_JBD) | (1 << BH_Req) | (1 << BH_Uptodate);

	atomic_set (&bh->b_count, 1);
	atomic_inc (&xreq->pendcount);
	    /*
	     * Dispatch a single request. We'll flush it to disc later.
	     * This is a linux kernel function.
	     */
	generic_make_request (operation, bh);

	xreq->vsector += sectors;
    }
    vbd_req_end_blkio_op (xreq, !xreq->error);
#else
    {
	_Bool		one_bio = 0;
	struct bio*	bio = 0;
	vbd_sector_t	nsector = 0;
#ifdef VBD2_FAST_MAP
	vbd_fast_map_t*	map = is_read ? 0 : vbd_link_fast_map_alloc (bl);
#endif
	unsigned int	bi_max_vecs = 0;
	_Bool		finish_later = FALSE;

	for (seg = xreq->nsegs; seg < count; seg++) {
	    NkPhAddr		paddr;
	    nku32_f		psize;
	    vbd_sector_t	sectors;
	    struct bio_vec*	bv;
	    vbd_sector_t	psector = 0;
	    vbd_pending_seg_t*	pseg;
	    _Bool		copy_mode;

	    paddr = *(VBD2_FIRST_BUF (req) + seg);

	    if (is_ext) {
		XTRACE (" 0x%llx:%lld\n",
			(long long) VBD2_BUF_BASE_EXT (paddr),
			(long long) VBD2_BUF_SIZE_EXT (paddr));
	    } else {
		XTRACE (" 0x%llx:%lld:%lld\n",
			(long long) VBD2_BUF_PAGE (paddr),
			(long long) VBD2_BUF_SSECT (paddr),
			(long long) VBD2_BUF_ESECT (paddr));
	    }
	    if (is_ext) {
		psize = VBD2_BUF_SIZE_EXT (paddr);
		paddr = VBD2_BUF_BASE_EXT (paddr);
		if (psize & (VBD2_SECT_SIZE - 1)) {
		    sectors = 0;
		    one_bio = 1;
		} else {
		    sectors = psize >> VBD2_SECT_SIZE_BITS;
		}
	    } else {
		sectors = VBD2_BUF_SECTS (paddr);
		psize   = VBD2_BUF_SIZE (paddr);
		paddr   = VBD2_BUF_BASE (paddr);
	    }
	    ++bl->stats.segments;
	    if (!vbd2_dma) {
		copy_mode = 1;
	    } else {
		    /* Check if we are allowed to use pfn_to_page() below */
		if (!pfn_valid (paddr >> PAGE_SHIFT)) {
		    DTRACE_VDISK (vd, "op %d vsector %lld physical address "
			    "%llx translation failure.\n",
			    req->op, xreq->vsector,
			    (unsigned long long) paddr);
		    ++bl->stats.no_struct_page;
		    copy_mode = 1;
		} else {
#ifdef CONFIG_X86
			/*
			 *  This check is only necessary on x86/VT
			 *  and on ARM the function is not implemented.
			 */
		    NkMhAddr maddr = nkops.nk_machine_addr (paddr);
		    if (maddr < 0x100000 && maddr != paddr) {
			DTRACE_VDISK (vd,
			    "COPY mode forced (mach=0x%llx phys=0x%llx)\n",
			    maddr, (unsigned long long) paddr);
			++bl->stats.first_meg_copy;
			copy_mode = 1;
		    } else
#endif
		    {
			copy_mode = 0;
		    }
		}
	    }
#if defined CONFIG_X86 && !defined CONFIG_X86_PAE && !defined CONFIG_X86_64
		/*
		 *  Fix for BugId 4676708 ("VBD-BE crashes if passed a
		 *  physical address above 4GB when not compiled for PAE
		 *  or 64 bits"). Catch it here, rather than waiting
		 *  till the vbd_fast_map_map() call.
		 */
	    if (paddr > (nku32_f) -1) {
		ETRACE_VDISK (vd, "op %d vsector %lld physical address "
			"%llx is not reachable.\n",
			req->op, xreq->vsector, (unsigned long long) paddr);
		xreq->error = TRUE;
		break;
	    }
#endif
	    ex = vbd_vdisk_translate (vd, acc, xreq->vsector, &psector);
	    if (unlikely (!ex)) {
		ETRACE_VDISK (vd, "sector %lld translation failure on %s\n",
			      xreq->vsector, is_read ? "read" : "write");
		xreq->error = TRUE;
		break;
	    }
	    if (!bio || bio->bi_vcnt == bi_max_vecs || psector != nsector) {
		unsigned int		vsize;
		unsigned int		nr_vecs;
		struct request_queue*	q;

		if (bio) {
		    vbd_req_submit_bio (xreq, bio);
		}
		vsize = count - seg;
		    /*
		     * We cannot place a request longer than the bio
		     * hw sector size, so in case of large requests
		     * we create chunks of number of bio vectors accepted
		     * by the physical device if needed.
		     */
		q	= bdev_get_queue (ex->bdev);
		nr_vecs	= bio_get_nr_vecs (ex->bdev);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
		{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
		    const unsigned max_hw_pg = queue_max_hw_sectors (q) >> 3;
#else
		    const unsigned max_hw_pg = q->max_hw_sectors >> 3;
#endif
		    if (nr_vecs > max_hw_pg) {
			nr_vecs = max_hw_pg;
		    }
		}
#endif
		if (vsize > nr_vecs) {
		    vsize = nr_vecs;
		}
		if (one_bio && vsize != count) {
		    ETRACE_VDISK (vd, "cannot alloc several bios\n");
		    xreq->error = TRUE;
		    bio		= NULL;
		    break;
		}
		{
#ifdef VBD2_FI
		    static int bio_allocs;

		    if (!(bio_allocs++ % 5)) {
			bio = 0;
		    } else
#endif
		    bio = bio_alloc (GFP_ATOMIC, vsize);
		}
		if (unlikely (!bio)) {
		    WTRACE_VDISK (vd, "sector %lld, out of bios; will retry\n",
				  xreq->vsector);
		    bl->be->resource_error = TRUE;
		    bl->pending_xreq = xreq;
		    bl->pending_msg = req;
		    ++bl->stats.resource_errors;
		    finish_later = TRUE;
		    break;
		}
		    /*
		     * bio_alloc() fixes vsize at bio's allocation time
		     * according to the memory pool where the bio has
		     * been allocated - we have to backup the value of
		     * vsize to be able to send the bio request when the
		     * allocated space is full.
		     */
		bi_max_vecs	= vsize;
		bio->bi_bdev    = ex->bdev;
		bio->bi_private = xreq;
		bio->bi_end_io  = vbd_bio_end_blkio_op;
		bio->bi_sector  = psector;
		bio->bi_rw      = acc == VBD_DISK_ACC_W ? WRITE : READ;
		bio->bi_size    = 0;
		bio->bi_vcnt    = 0;
	    }
	    bv = bio_iovec_idx (bio, bio->bi_vcnt);
	    bv->bv_len    = psize;
	    bv->bv_offset = paddr & ~VBD_PAGE_64_MASK;

	    pseg = xreq->segs + xreq->nsegs;

	    if (copy_mode) {
		unsigned long vaddr;

#ifdef VBD2_FI
		static int page_allocs;

		if (!(page_allocs++ % 5)) {
		    vaddr = 0;
		} else
#endif
		vaddr = __get_free_page (0);

		if (unlikely (!vaddr)) {
		    WTRACE_VDISK (vd, "sector %lld, out of pages; will retry\n",
				  xreq->vsector);
		    bl->be->resource_error = TRUE;
		    bl->pending_xreq = xreq;
		    bl->pending_msg = req;
		    ++bl->stats.resource_errors;
		    finish_later = TRUE;
			/*
			 *  "bio" retains all the segments which have
			 *  been prepared and for which variables like
			 *  xreq->nsegs, xreq->vsector and stats have
			 *  been updated. It will be submitted before
			 *  returning from this function.
			 */
		    break;
		}
		++bl->stats.page_allocs;
		++bl->stats.alloced_pages;
		if (bl->stats.alloced_pages > bl->stats.max_alloced_pages) {
		    bl->stats.max_alloced_pages = bl->stats.alloced_pages;
		}
		bv->bv_page = virt_to_page (vaddr);
		pseg->vaddr = vaddr;
		if (is_read) {
		    pseg->gaddr = paddr;
		    pseg->size  = psize;
			/*
			 *  We do not know yet if this is going to
			 *  be a fast_map read or an nk_mem_map read.
			 */
		} else {	/* Write to disk */
			/* Copy guest page */
		    unsigned long dst = vaddr + bv->bv_offset;
#ifdef VBD2_FAST_MAP
		    if (map) {
			unsigned long src = (unsigned long) map->addr +
			    bv->bv_offset;

			vbd_fast_map_map (map, paddr & VBD_PAGE_64_MASK);
			memcpy ((void*) dst, (void*) src, psize);
			vbd_fast_map_unmap (map);
			++bl->stats.fast_map.writes;
			bl->stats.fast_map.written_bytes += psize;
		    } else
#endif
		    {
			void* src = nkops.nk_mem_map (paddr, psize);

			if (!src) {
			    ETRACE_VDISK
				(vd, "sector %lld, cannot mem map; fatal\n",
				 xreq->vsector);
			    free_page (vaddr);
			    ++bl->stats.page_freeings;
			    --bl->stats.alloced_pages;
			    xreq->error = TRUE;
			    break;
			}
			memcpy ((void*) dst, src, psize);
			nkops.nk_mem_unmap (src, paddr, psize);
			++bl->stats.nk_mem_map.writes;
			bl->stats.nk_mem_map.written_bytes += psize;
		    }
		    vbd_cache_clean ((const char*) dst, psize);
		}
	    } else {	/* DMA mode */
		pseg->vaddr = 0; /* No page */
		bv->bv_page = pfn_to_page (paddr >> PAGE_SHIFT);
		if (is_read) {
		    bl->stats.dma.read_bytes += psize;
		    ++bl->stats.dma.reads;
		} else {
		    bl->stats.dma.written_bytes += psize;
		    ++bl->stats.dma.writes;
		}
	    }
	    if (is_read) {
		bl->stats.bytes_read += psize;
	    } else {
		bl->stats.bytes_written += psize;
	    }
	    bio->bi_vcnt++;
	    bio->bi_size += psize;
		/* Increment nsegs only now, to validate "pseg" */
	    ++xreq->nsegs;

	    xreq->vsector += sectors;
	    nsector  = psector + sectors;
	}
#ifdef VBD2_FAST_MAP
	if (map) {
	    vbd_link_fast_map_free (bl, map);
	}
#endif
	if (bio) {
	    vbd_req_submit_bio (xreq, bio);
	}
	    /*
	     *  The xreq->pendcount reference count, which has been set
	     *  to 1 initially, protects xreq against being considered
	     *  as complete when all "bios" submitted up to now complete.
	     */
	if (!finish_later) {
	    vbd_req_end_blkio_op (xreq);
	}
    }
#endif
    return TRUE;	/* "req" consumed */
}

    /* Interrupt-level callback */

    static void
vbd_cb_receive_notify (vmq_link_t* link2)
{
    vbd_link_t* bl = VBD_LINK (link2);

    DTRACE ("guest %d\n", vmq_peer_osid (link2));
    vbd_link_add_to_blkio_thread_list_tail (bl);
    vbd_be_maybe_wake_up_blkio_thread (bl->be);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)

    /* Called by vbd_link_do_blkio_op() only */

    static void
vbd_link_blkio_done (vbd_link_t* bl)
{
    vbd_pending_req_t* req;

    while ((req = vbd_link_get_from_done_list (bl)) != NULL) {
	vbd_pending_seg_t* seg   = req->segs;
	vbd_pending_seg_t* limit = req->segs + req->nsegs;
	while (seg != limit) {
	    if (seg->vaddr) {
		void*		dst = nkops.nk_mem_map (seg->gaddr, seg->size);
		unsigned long	src =
			    seg->vaddr + (seg->gaddr & ~VBD_PAGE_64_MASK);

		if (dst) {
		    memcpy (dst, (void*) src, seg->size);
		    vbd_cache_clean ((const char*) dst, seg->size);
		    nkops.nk_mem_unmap (dst, seg->gaddr, seg->size);
		    ++bl->stats.nk_mem_map.reads;
		    bl->stats.nk_mem_map.read_bytes += seg->size;
		} else {
		    req->error   = TRUE;
		}
		free_page (seg->vaddr);
		++bl->stats.page_freeings;
		--bl->stats.alloced_pages;
	    }
	    seg++;
	}
	vbd_req_blkio_done (req);
    }
}
#endif	/* 2.6 */

    static void
vbd_vdisk_op (vbd_vdisk_t* vd, vbd2_req_header_t* req)
{
    vbd_link_t*	bl = vd->bl;

    if (!vd->open && req->op != VBD2_OP_OPEN) {
	ETRACE_VDISK (vd, "not open\n");
	goto error;
    }
    switch (req->op) {
    case VBD2_OP_MEDIA_PROBE:
	vbd_vdisk_probe_op (vd, vbd_vdisk_op_media_probe, req);
	return;

    case VBD2_OP_MEDIA_CONTROL:
	vbd_vdisk_probe_op (vd, vbd_vdisk_op_media_control, req);
	return;

    case VBD2_OP_MEDIA_LOCK:
	vbd_vdisk_probe_op (vd, vbd_vdisk_op_media_lock, req);
	return;

#ifdef VBD2_ATAPI
    case VBD2_OP_ATAPI:
	vbd_link_atapi (vd, req);
	return;
#endif
    case VBD2_OP_READ_EXT:
    case VBD2_OP_WRITE_EXT:
    case VBD2_OP_READ:
    case VBD2_OP_WRITE: {
	_Bool msg_used = vbd_vdisk_rw (vd, req);
	    /*
	     * On resource_error, pending_msg should be set always,
	     * and pending_xirq most of the time.
	     */
	VBD_ASSERT (msg_used || (bl->be->resource_error && bl->pending_msg));
	(void) msg_used;
	return;
    }
    case VBD2_OP_OPEN:
	if (vd->open) {
	    ETRACE_VDISK (vd, "already open\n");
	    break;
	}
	vbd_vdisk_open (vd);	/* void */
	if (!vd->open) break;	/* Error message already issued */
	vbd_link_resp (bl, req, VBD2_STATUS_OK);
	return;

    case VBD2_OP_CLOSE:
	    /* Error condition already checked at entry */
	vbd_vdisk_close (vd);
	vbd_link_resp (bl, req, VBD2_STATUS_OK);
	return;

    case VBD2_OP_GETGEO: {
	vbd2_get_geo_t* const	resp = (vbd2_get_geo_t*) req;
	const vbd_extent_t*	ex = vd->extents;
	const struct gendisk*	gd;
	struct hd_geometry	geo;
	int			diag;

	if (ex->next) {
	    ETRACE_VDISK (vd, "exactly 1 extent required\n");
	    break;
	}
	VBD_ASSERT (VBD_BDEV_OPEN (ex->bdev));
	gd = VBD_GENDISK (ex->bdev);
	if (!gd) {
	    ETRACE_VDISK (vd, "no gendisk\n");
	    break;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
	if (!gd->fops->getgeo) {
	    ETRACE_VDISK (vd, "no getgeo\n");
	    break;
	}
	diag = gd->fops->getgeo (ex->bdev, &geo);
	if (diag) {
	    ETRACE_VDISK (vd, "getgeo failed (%d)\n", diag);
	    break;
	}
	resp->heads           = geo.heads;
	resp->sects_per_track = geo.sectors;
	resp->cylinders       = geo.cylinders;
	DTRACE_VDISK (vd, "heads %d sects_per_track %d cylinders %d\n",
		      resp->heads, resp->sects_per_track, resp->cylinders);
	vbd_link_resp (bl, req, VBD2_STATUS_OK);
	return;
#else
	(void) resp;
	(void) geo;
	(void) diag;
	ETRACE_VDISK (vd, "getgeo not implemented\n");
	break;
#endif
    }
    default:
	break;
    }
error:
    vbd_link_resp (bl, req, VBD2_STATUS_ERROR);
}

static const char vbd_op_names[VBD2_OP_MAX][13] = {VBD2_OP_NAMES};

    /*
     * Called only by vbd_blkio_thread().
     * Can set be->resource_error.
     */

    static int
vbd_link_do_blkio_op (vbd_link_t* bl, int max_to_do)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    vbd_link_blkio_done (bl);
#endif
    while (max_to_do > 0) {
	vbd2_req_header_t* req;

	if (bl->pending_msg) {
	    req = bl->pending_msg;
	    bl->pending_msg = NULL;
	} else {
	    if (vmq_msg_receive (bl->link, (void**) &req)) break;
	}
	DTRACE ("guest %d: %s\n", vmq_peer_osid (bl->link),
		vbd_op_names [req->op % VBD2_OP_MAX]);
	switch (req->op) {
	case VBD2_OP_PROBE:
	    vbd_link_resp (bl, req, vbd_link_op_probe (bl, req,
			   ((vbd2_probe_link_t*) req)->probe,
			   req->count * sizeof (vbd2_probe_t)));
	    break;

	case VBD2_OP_MEDIA_PROBE:
	case VBD2_OP_MEDIA_CONTROL:
	case VBD2_OP_MEDIA_LOCK:
#ifdef VBD2_ATAPI
	case VBD2_OP_ATAPI:
#endif
	case VBD2_OP_READ_EXT:
	case VBD2_OP_WRITE_EXT:
	case VBD2_OP_READ:
	case VBD2_OP_WRITE:
	case VBD2_OP_OPEN:
	case VBD2_OP_CLOSE:
	case VBD2_OP_GETGEO: {
	    vbd_vdisk_t* vd = vbd_link_vdisk_lookup (bl, req);

	    if (!vd) {
		    /* Message already issued */
		vbd_link_resp (bl, req, VBD2_STATUS_ERROR);
		break;
	    }
	    vbd_vdisk_op (vd, req);	/* void */
	    break;
	}
	default:
	    vbd_link_resp (bl, req, VBD2_STATUS_ERROR);
	    break;
	}
	    /*
	     *  In case of be->resource_error, current request
	     *  is not finished yet.
	     */
	if (bl->be->resource_error) break;
	max_to_do--;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	vbd_link_blkio_done (bl);
#endif
    }
    return !max_to_do;
}

/*----- Notification of VBD device list changes -----*/

    /* Can sleep for place in FIFO */

    static vbd2_msg_t*
vbd_link_alloc_async (vmq_link_t* link2, vbd2_op_t op)
{
    vbd2_msg_t*	msg;

    if (unlikely (vmq_msg_allocate (link2, 0, (void**) &msg, NULL)))
	return NULL;
    memset (msg, 0, sizeof *msg);
    msg->req.op = op;
    return msg;
}

    static void
vbd_link_signal_changes (vbd_link_t* bl)
{
    DTRACE ("guest %d\n", vmq_peer_osid (bl->link));
    if (!bl->changes_signaled) {
	vbd2_msg_t* async;

	async = vbd_link_alloc_async (bl->link, VBD2_OP_CHANGES);
	if (async) {
	    vmq_msg_send (bl->link, async);
	    bl->changes_signaled = true;
	} else {
	    DTRACE ("failed to alloc async msg\n");
	}
    }
}

/*----- /proc/nk/vbd2-be management -----*/

    static _Bool
vbd_link_proc_show (vmq_link_t* link2, void* cookie)
{
    const vbd_link_t*	bl = VBD_LINK (link2);
    struct seq_file*	seq = (struct seq_file*) cookie;
    const vbd_vdisk_t*	vd;

    seq_printf (seq, "I/O with guest %d (%s connected):\n",
	vmq_peer_osid (bl->link), bl->connected ? "is" : "not");
    seq_printf (seq, " General: requests %u segments %u read %llu"
	" written %llu\n",
	bl->stats.requests,
	bl->stats.segments,
	bl->stats.bytes_read,
	bl->stats.bytes_written);
    seq_printf (seq, " DMA: reads %u writes %u read %llu written %llu\n",
	bl->stats.dma.reads,
	bl->stats.dma.writes,
	bl->stats.dma.read_bytes,
	bl->stats.dma.written_bytes);
#ifdef VBD2_FAST_MAP
    seq_printf (seq,
	" fast-map: probes %u reads %u writes %u read %llu written %llu\n",
	bl->stats.fast_map.probes,
	bl->stats.fast_map.reads,
	bl->stats.fast_map.writes,
	bl->stats.fast_map.read_bytes,
	bl->stats.fast_map.written_bytes);
#endif
    seq_printf (seq,
	" nk-mem-map: probes %u reads %u writes %u read %llu "
	"written %llu\n",
	bl->stats.nk_mem_map.probes,
	bl->stats.nk_mem_map.reads,
	bl->stats.nk_mem_map.writes,
	bl->stats.nk_mem_map.read_bytes,
	bl->stats.nk_mem_map.written_bytes);
    seq_printf (seq, " no_struct_page %u page_allocs %u freeings %u"
	" max %u replies %u\n", bl->stats.no_struct_page, bl->stats.page_allocs,
	bl->stats.page_freeings, bl->stats.max_alloced_pages,
	bl->stats.msg_replies);
#ifdef CONFIG_X86
    seq_printf (seq, " first_meg_copy %u resource_errors %u error_replies %u\n",
	bl->stats.first_meg_copy, bl->stats.resource_errors,
	bl->stats.error_replies);
#else
    seq_printf (seq, " resource_errors %u error_replies %u\n",
	bl->stats.resource_errors, bl->stats.error_replies);
#endif

    VBD_LINK_FOR_ALL_VDISKS (vd, bl) {
	const vbd_extent_t* ex;
	unsigned count = 0;

	VBD_VDISK_FOR_ALL_EXTENTS (ex, vd) ++count;
	seq_printf (seq, " vdisk %d: %s %u extent(s), %llu "
		    "sectors, %s mode, %s, %s",
		    vd->tag, vd->name, count,
		    (unsigned long long) vbd_vdisk_sectors (vd),
		    vbd_vdisk_wait_modes [vd->wait % 3],
		    vd->open ? "open" : "closed",
		    vbd_vdisk_openable (vd) ? "openable" : "non-openable");
#ifdef VBD2_ATAPI
	if (vd->atapi) {
	    seq_printf (seq, ", ATAPI(%s,%s)",
			vd->atapi->ctrl_name, vd->atapi->data_name);
	}
#endif
	seq_printf (seq, "\n");
	VBD_VDISK_FOR_ALL_EXTENTS (ex, vd) {
	    const _Bool nok = !VBD_BDEV_OPEN (ex->bdev);

	    seq_printf (seq,
		"  ext (%d,%d): bdev %s/%s start %lld sect %lld %s\n",
			MAJOR (ex->dev), MINOR (ex->dev), nok ? "NO" : "OK",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
			nok ? "?" : ex->bdev->bd_part ? "Part" : "Disk",
#else
			nok ? "?" : "Disk",
#endif
			(long long) ex->start, (long long) ex->sectors,
			ex->openable ? "openable" : "not-openable");
	}
    }
    return false;
}

    static int
vbd_seq_proc_show (struct seq_file* seq, void* v)
{
    const vbd_be_t*		be = seq->private;
    const vbd_prop_vdisk_t*	pvd;
    const vbd_prop_extent_t*	pe;

    (void) v;
    if (be->links) {
	vmq_links_iterate (be->links, vbd_link_proc_show, seq);
    }
    seq_printf (seq, "Sleeps %llu withTimeout %u timeouts %u polling %d\n",
		be->stats.sleeps_no_timeout, be->stats.sleeps_with_timeout,
		be->stats.timeouts, be->event_thread.polling_needed);

    seq_printf (seq, "Configured vdisks:\n");
    VBD_BE_FOR_ALL_PROP_VDISKS (pvd, be) {
	seq_printf (seq, " tag %d (%d,%d,%d) %s\n", pvd->tag,
		    pvd->owner, VBD2_DEVID_MAJOR (pvd->devid),
		    VBD2_DEVID_MINOR (pvd->devid),
		    vbd_vdisk_wait_modes [pvd->wait % 3]);
    }
    seq_printf (seq, "Configured extents:\n");
    VBD_BE_FOR_ALL_PROP_EXTENTS (pe, be) {
	seq_printf (seq,
		" tag %d (%d,%d,%s) start %lld sect %lld curr %lld\n",
		    pe->tag, pe->major, pe->minor,
		    pe->access == VBD_DISK_ACC_R ? "ro" : "rw",
		    (long long) pe->start, (long long) pe->sectors,
		    vbd_prop_extent_sectors (pe));
    }
    return 0;
}

    static int
vbd_proc_open (struct inode* inode, struct file* file)
{
    return single_open (file, vbd_seq_proc_show, PDE (inode)->data);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,15)
static struct file_operations vbd_proc_fops =
#else
static const struct file_operations vbd_proc_fops =
#endif
{
    .owner	= THIS_MODULE,
    .open	= vbd_proc_open,
    .read	= seq_read,
    .llseek	= seq_lseek,
    .release	= single_release,
};

    /* On ARM, /proc/nk is created, but no proc_root_nk symbol is offered */

    static int __init
vbd_be_proc_init (vbd_be_t* be)
{
    struct proc_dir_entry* ent;

    ent = create_proc_entry ("nk/vbd2-be", 0, NULL);
    if (!ent) {
	ETRACE ("Could not create /proc/nk/vbd2-be\n");
	return -ENOMEM;
    }
    ent->proc_fops  = &vbd_proc_fops;
    ent->data       = be;
    be->proc_inited = TRUE;
    return 0;
}

    static void VBD_EXIT
vbd_proc_exit (vbd_be_t* be)
{
    if (!be->proc_inited) return;
    remove_proc_entry ("nk/vbd2-be", NULL);
}

/*----- /proc/nk/vbd-atapi-... management -----*/

#ifdef VBD2_ATAPI
static struct file_operations vbd_atapi_ctrl_proc_fops = {
    open:    vbd_atapi_ctrl_proc_open,
    release: vbd_atapi_ctrl_proc_release,
    llseek:  vbd_atapi_ctrl_proc_lseek,
    read:    vbd_atapi_ctrl_proc_read,
    write:   vbd_atapi_ctrl_proc_write,
};

static struct file_operations vbd_atapi_data_proc_fops = {
    open:    vbd_atapi_data_proc_open,
    release: vbd_atapi_data_proc_release,
    llseek:  vbd_atapi_data_proc_lseek,
    read:    vbd_atapi_data_proc_read,
    write:   vbd_atapi_data_proc_write,
};

    static _Bool __init
vbd_atapi_vdisk_init (vbd_vdisk_t* vd)
{
    struct proc_dir_entry*	ent;
    int				major, minor;
    vbd_atapi_t*		atapi;

    if (!vbd_vdisk_is_atapi (vd, &major, &minor)) {
	return TRUE;
    }
    atapi = (vbd_atapi_t*) kzalloc (sizeof *atapi, GFP_KERNEL);
    if (unlikely (!atapi)) {
	EFTRACE ("out of memory\n");
	return FALSE;
    }
    mutex_init (&atapi->lock);
    init_waitqueue_head (&atapi->wait);

    snprintf (atapi->ctrl_name, sizeof atapi->ctrl_name,
	      "nk/vbd-atapi-c-%u-%u", major, minor);
    ent = create_proc_entry (atapi->ctrl_name, 0, NULL);
    if (!ent) {
	EFTRACE ("could not create /proc/%s\n", atapi->ctrl_name);
	kfree (atapi);
	return FALSE;
    }
    ent->data = vd;
    ent->proc_fops = &vbd_atapi_ctrl_proc_fops;

    snprintf (atapi->data_name, sizeof atapi->data_name,
	      "nk/vbd-atapi-d-%u-%u", major, minor);
    ent = create_proc_entry (atapi->data_name, 0, NULL);
    if (!ent) {
	EFTRACE ("could not create /proc/%s\n", atapi->data_name);
	remove_proc_entry (atapi->ctrl_name, NULL);
	kfree (atapi);
	return FALSE;
    }
    ent->data = vd;
    ent->proc_fops = &vbd_atapi_data_proc_fops;

    vd->atapi = atapi;
    return TRUE;
}

    static void VBD_EXIT
vbd_atapi_vdisk_free (vbd_vdisk_t* vd)
{
    if (vd->atapi) {
	remove_proc_entry (vd->atapi->ctrl_name, NULL);
	remove_proc_entry (vd->atapi->data_name, NULL);
	kfree (vd->atapi);
	vd->atapi = 0;
    }
}
#endif /* VBD2_ATAPI */

/*----- Command line / module options management -----*/

static vbd_be_t vbd_be;		/* Driver global data */

    static _Bool __init
vbd_vbd2_syntax (const char* opt)
{
    ETRACE ("Syntax error near '%s'\n", opt);
    return FALSE;
}

    static inline _Bool __init
vbd_vbd2_end (const char ch)
{
    return ch == ')' || ch == ';';
}

    static _Bool __init
vbd_vbd2_one (const char* start, char** endp)
{
#ifdef MODULE
    vbd_vdisk_wait_t	wait = VBD_VDISK_WAIT_NO;
#else
    vbd_vdisk_wait_t	wait = VBD_VDISK_WAIT_YES;
#endif
    NkOsId		owner;
    long		vmajor;
    long		vminor;
    long		major;
    long		minor;
    long		acc;
    unsigned		idx;
    char*		end;

    owner = simple_strtoul (start, &end, 0);
    if (end == start || *end != ',') {
	return vbd_vbd2_syntax (end);
    }
    start = end+1;

    vmajor = simple_strtoul (start, &end, 0);
    if (end == start || *end != ',') {
	return vbd_vbd2_syntax (end);
    }
    start = end+1;

    vminor = simple_strtoul (start, &end, 0);
    if (end == start || (*end != ':' && *end != '|' && *end != '/')) {
	return vbd_vbd2_syntax (end);
    }
    start = end+1;

    major = simple_strtoul (start, &end, 0);
    if (end == start || *end != ',') {
	return vbd_vbd2_syntax (end);
    }
    start = end+1;

    minor = simple_strtoul (start, &end, 0);
    if (end == start || *end != ',') {
	return vbd_vbd2_syntax (end);
    }
    start = end+1;

    if (!strncmp (start, "ro", 2)) {
	acc = VBD_DISK_ACC_R;
    } else if (!strncmp (start, "rw", 2)) {
	acc = VBD_DISK_ACC_RW;
    } else {
	return vbd_vbd2_syntax (start);
    }
    start += 2;

	/*
	 * vbd2=(... [,[nw|wa|nz]])
	 */
    if (*start == ',') {
	++start;
	if (*start != ',' && !vbd_vbd2_end (*start)) {
	    if (!strncmp (start, vbd_vdisk_wait_modes
			  [VBD_VDISK_WAIT_NO], 2)) {
		wait = VBD_VDISK_WAIT_NO;
	    } else if (!strncmp (start, vbd_vdisk_wait_modes
				 [VBD_VDISK_WAIT_YES], 2)) {
		wait = VBD_VDISK_WAIT_YES;
	    } else if (!strncmp (start, vbd_vdisk_wait_modes
				 [VBD_VDISK_WAIT_NON_ZERO], 2)) {
		wait = VBD_VDISK_WAIT_NON_ZERO;
	    } else {
		return vbd_vbd2_syntax (start);
	    }
	    start += 2;
	}
    }
    if (!vbd_vbd2_end (*start)) {
	return vbd_vbd2_syntax (start);
    }
    {
	vbd_prop_vdisk_t*	pvd;

	VBD_BE_FOR_ALL_PROP_VDISKS (pvd, &vbd_be) {
	    if (pvd->owner == owner &&
		pvd->devid == VBD2_DEVID (vmajor, vminor)) {
		WTRACE ("overwriting vdisk(%ld,%ld) config.\n", vmajor, vminor);
		break;
	    }
	}
	idx = pvd - vbd_be.prop_vdisks;
    }
    if (idx < VBD_ARRAY_ELEMS (vbd_be.prop_vdisks) &&
        idx < VBD_ARRAY_ELEMS (vbd_be.prop_extents)) {
	vbd_prop_vdisk_init (&vbd_be.prop_vdisks [idx], idx, owner,
			     VBD2_DEVID (vmajor, vminor), wait);
	vbd_prop_extent_init (&vbd_be.prop_extents [idx], 0, 0, acc, minor,
			      idx, major);
    } else {
	WTRACE ("too many disks: vdisk(%ld,%ld) ignored\n", vmajor, vminor);
    }
    *endp = (char*) start;
    return TRUE;
}

    /*
     * This function needs to be "int" for the __setup() macro
     * It returns 1 on success and 0 on failure.
     */

    static int __init
vbd_vbd2_setup (char* start)
{
    do {
	if (*start != '(') {
	    return vbd_vbd2_syntax (start);
	}
	++start;
	do {
	    if (!vbd_vbd2_one (start, &start)) {
		return FALSE;
	    }
	} while (*start++ == ';');
    } while (*start++ == ',');
    return TRUE;
}

    static int __init
vbd_vbd2_dma_setup (char* start)
{
    char* end;

    vbd2_dma = simple_strtoul (start, &end, 0);
    return 1;
}

#ifndef MODULE
__setup ("vbd2=",     vbd_vbd2_setup);
__setup ("vbd2_dma=", vbd_vbd2_dma_setup);
#else

    /* Loading parameters */
static char* vbd2 [CONFIG_NKERNEL_VBD_NR];
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
MODULE_PARM (vbd2, "1-" __MODULE_STRING (CONFIG_NKERNEL_VBD_NR) "s");
MODULE_PARM (vbd2_dma, "i");
#else
module_param_array (vbd2, charp, NULL, 0);
module_param       (vbd2_dma, int, 0);
#endif
MODULE_PARM_DESC (vbd2_dma, " Use DMA to front-end buffers (dflt: 1).");
MODULE_PARM_DESC (vbd2,
		 "  virtual disks configuration:\n\t\t"
		 "   \"vbd2=(<owner>,<vmaj>,<vmin>/<maj>,<min>,"
			"<access>)[,...]\"\n\t\t"
		 "  where:\n\t\t"
		 "   <owner>       is the owner OS ID in [0..31]\n\t\t"
		 "   <vmaj>,<vmin> is the virtual disk ID (major,minor)\n\t\t"
		 "   <maj>,<min>   is the associated real device "
			"major,minor\n\t\t"
		 "   <access>      defines access rights: \"ro\" or"
		 " \"rw\"\n");
#ifdef CONFIG_ARM
#define vlx_command_line	saved_command_line
#else
extern char* vlx_command_line;
#endif
#endif

/*----- Initialization and exit entry points -----*/

    static void* __init
vbd_vlink_syntax (const char* opt)
{
    ETRACE ("Syntax error near '%s'\n", opt);
    return NULL;
}

#include "vlx-vbd2-common.c"

    /* Only called from vbd_be_destroy() <- vbd_exit() */

    static _Bool VBD_EXIT
vbd_link_stop (vmq_link_t* link2, void* cookie)
{
    vbd_link_t* bl = VBD_LINK (link2);

    (void) cookie;
    vbd_link_put (bl);
    DTRACE ("be.refcount %d link.refcount %d\n",
	    bl->be->refcount.counter, bl->refcount.counter);
    return false;
}

    /* Called from vbd_exit() only */

    static void VBD_EXIT
vbd_be_destroy (vbd_be_t* be)
{
    if (be->links) {
	vmq_links_abort (be->links);
	vmq_links_iterate (be->links, vbd_link_stop, NULL);
    }
    be->event_thread.abort = TRUE;	/* Wake up the event thread */
    wake_up_interruptible (&be->event_thread.wait);
    vlx_thread_join (&be->blkio_thread.desc);
    vlx_thread_join (&be->event_thread.desc);

    if (be->links) {
	vmq_links_iterate (be->links, vbd_link_destroy, NULL);
	vmq_links_finish (be->links);
	be->links = NULL;
    }
    if (be->pending_req_cachep) {
	kmem_cache_destroy (be->pending_req_cachep);
	be->pending_req_cachep = NULL;
    }
#ifdef VBD_UEVENT
    vlx_uevent_exit (&be->uevent);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    if (be->buffer_head_cachep) {
	kmem_cache_destroy (be->buffer_head_cachep);
	be->buffer_head_cachep = NULL;
    }
#endif
}

    /* Only called from vbd_init() */

    static _Bool __init
vbd_link_init (vmq_link_t* link2, void* cookie)
{
    vbd_link_t*	bl = VBD_LINK (link2);
    vbd_be_t*	be = (vbd_be_t*) cookie;

    bl->link = link2;
	/* Not yet connected, wait for "link_on" */
	/* No vdisks yet */
    bl->be = be;
	/*
	 * bl->blkio_thread.list should not be initialized
	 * with INIT_LIST_HEAD() because this would
	 * fail the logic in vbd_link_on_blkio_thread_list().
	 */

	/* This map can also be used in "DMA" mode if necessary */
#ifdef VBD2_FAST_MAP
    vbd_link_fast_map_create (bl);
#endif

#ifdef CONFIG_X86
#ifdef VBD2_FAST_MAP
    if (!bl->fast_map.head) {
	TRACE ("no fast map, performance will be reduced\n");
    }
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if (!pfn_valid ((800*1024*1024) >> PAGE_SHIFT)) {
	TRACE ("no high page, performance will be reduced\n");
    }
#endif
#endif
    INIT_LIST_HEAD (&bl->done_list.head);
    spin_lock_init (&bl->done_list.lock);
    vbd_be_get (be);	/* Bump be->refcount for every link */
    vbd_link_get (bl);	/* Bump bl->refcount for link */
    return false;
}

#define VBD_FIELD(name,value)	value

    static void
vbd_cb_link_on (vmq_link_t* link2)
{
    DTRACE ("guest %d\n", vmq_peer_osid (link2));
    VBD_LINK (link2)->connected = 1;
}

    static void
vbd_cb_link_off (vmq_link_t* link2)
{
    vbd_link_t*		bl = VBD_LINK (link2);
    vbd_vdisk_t*	vd;

    DTRACE ("guest %d\n", vmq_peer_osid (link2));
    bl->connected = 0;
    VBD_LINK_FOR_ALL_VDISKS (vd, bl) {
	vbd_vdisk_close (vd);
    }
}

    static void
vbd_cb_link_off_completed (vmq_link_t* link2)
{
    (void) link2;
    DTRACE ("guest %d\n", vmq_peer_osid (link2));
}

#define VBD_LINKS_BE(links) \
    ((vbd_be_t*) ((vmq_links_public*) (links))->priv)
#undef VBD_LINKS_BE
#define VBD_LINKS_BE(links) (*(vbd_be_t**) (links))

    static void
vbd_cb_sysconf_notify (vmq_links_t* links)
{
    vbd_be_t* be = VBD_LINKS_BE (links);

    DTRACE ("entered\n");
    be->sysconf = true;
    wake_up (&be->blkio_thread.wait);
}

    static const vmq_callbacks_t
vbd_callbacks = {
    VBD_FIELD (link_on,			vbd_cb_link_on),
    VBD_FIELD (link_off,		vbd_cb_link_off),
    VBD_FIELD (link_off_completed,	vbd_cb_link_off_completed),
    VBD_FIELD (sysconf_notify,		vbd_cb_sysconf_notify),
    VBD_FIELD (receive_notify,		vbd_cb_receive_notify),
    VBD_FIELD (return_notify,		NULL),
    VBD_FIELD (get_tx_config,		NULL),
    VBD_FIELD (get_rx_config,		vbd_cb_get_xx_config)
};

    static const vmq_xx_config_t
vbd_tx_config = {
    VBD_FIELD (msg_count,	4),
    VBD_FIELD (msg_max,		sizeof (vbd2_msg_t)),
    VBD_FIELD (data_count,	0),
    VBD_FIELD (data_max,	0)
};

#undef VBD_FIELD

    static int __init
vbd_be_init (vbd_be_t* be)
{
    be->pending_req_cachep = kmem_cache_create ("vbd2_pending_req",
	sizeof (vbd_pending_req_t), 0, SLAB_HWCACHE_ALIGN,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
	NULL,
#endif
	NULL);
    if (!be->pending_req_cachep) {
	ETRACE ("could not create kmem_cache\n");
	return -ENOMEM;
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    be->buffer_head_cachep = kmem_cache_create ("vbd2_buffer_head",
	sizeof (struct buffer_head), 0, SLAB_HWCACHE_ALIGN, NULL, NULL);
    if (!be->buffer_head_cachep) {
	ETRACE ("could not create kmem_cache\n");
	return -ENOMEM;
    }
#endif
    init_waitqueue_head (&be->blkio_thread.wait);
    INIT_LIST_HEAD (&be->blkio_thread.list);
    spin_lock_init (&be->blkio_thread.list_lock);
    init_waitqueue_head (&be->event_thread.wait);
#ifdef VBD_UEVENT
	/*
	 * We can only call vbd_be_uevent_init() after initializing
	 * the be->event_thread.wait queue, because this routine sets
	 * up a callback which will wake that queue up.
	 */
    return vlx_uevent_init (&be->uevent, vbd_uevent_data_ready);
#else
    return 0;
#endif
}

/*----- Module entry and exit points -----*/

    static void VBD_EXIT
vbd_exit (void)
{
    DTRACE ("entered\n");
    vbd_proc_exit (&vbd_be);
    vbd_be_destroy (&vbd_be);
    TRACE ("finished\n");
}

    static int __init
vbd_init (void)
{
    vbd_be_t*	be = &vbd_be;
    signed	diag;

    diag = vbd_be_init (be);
    if (diag) goto error;
#ifdef MODULE
    {
	char**	opt;
	char* cmdline;

	    /* Parse kernel command line options first */
	cmdline = vlx_command_line;
	while ((cmdline = strstr (cmdline, "vbd2="))) {
	    cmdline += 5;
	    if (!vbd_vbd2_setup (cmdline)) {
		diag = -EINVAL;
		goto error;
	    }
	}
	cmdline = vlx_command_line;
	if ((cmdline = strstr (cmdline, "vbd2_dma="))) {
	    vbd_vbd2_dma_setup (cmdline + 9);	/* Never fails */
	}
	    /* Then arguments given to insmod */
	for (opt = vbd2; *opt; ++opt) {
	    if (!vbd_vbd2_setup (*opt)) {
		diag = -EINVAL;
		goto error;
	    }
	}
    }
#endif	/* MODULE */
    if (!be->prop_vdisks [0].devid) {
	ETRACE ("No virtual disk configured\n");
	diag = -EINVAL;
	goto error;
    }
    diag = vmq_links_init_ex (&be->links, "vbd2", &vbd_callbacks,
			      &vbd_tx_config, NULL /*rx_config*/, be, false);
    if (diag) goto error;
    vmq_links_iterate (be->links, vbd_link_init, be); /* Cannot fail */
    {
	const vbd_prop_vdisk_t*	pvd;

	VBD_BE_FOR_ALL_PROP_VDISKS (pvd, be) {
	    diag = vbd_be_vdisk_create (be, pvd);
	    if (diag) goto error;
	}
    }
    {
	vbd_prop_extent_t*	pe;

	VBD_BE_FOR_ALL_PROP_EXTENTS (pe, be) {
	    if (!vbd_be_extent_create (be, pe)) {
		diag = -ENODEV;
		goto error;
	    }
	}
    }
    vbd_be_proc_init (be);
    diag = vmq_links_start (be->links);
    if (diag) goto error;
    diag = vlx_thread_start (&be->blkio_thread.desc,
			     vbd_blkio_thread, be, "vbd2-be-blkio");
    if (diag) goto error;
    diag = vlx_thread_start_ex (&be->event_thread.desc, vbd_event_thread,
				be, "vbd2-be-extent", 1 /*will_suicide*/);
    if (diag) goto error;
    TRACE ("initialized\n");
    return 0;

error:
    ETRACE ("init failed (%d)\n", diag);
    vbd_exit();
    return diag;
}

module_init (vbd_init);
module_exit (vbd_exit);

/*----- Module description -----*/

MODULE_DESCRIPTION ("VLX Virtual Block Device v.2 backend driver");
MODULE_AUTHOR ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_LICENSE ("GPL");

/*----- End of file -----*/

/*
 ****************************************************************
 *
 *  Component: VLX Virtual Block Device backend driver
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
    /* This include exists in 2.6.6 but functions are not yet exported */
#include <linux/kthread.h>
#endif
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

#ifdef CONFIG_X86
#define VBD_ATAPI
#endif

#include <nk/nkern.h>
#include <nk/rdisk.h>

    /* Compensate for obsolete rdisk.h files */
#ifndef RDISK_OP_READ_EXT
#define RDISK_OP_READ_EXT  4
#define RDISK_OP_WRITE_EXT 5
#define RDISK_EXT_SHIFT         12
#define RDISK_EXT_SIZE          (1 << RDISK_EXT_SHIFT)
#define RDISK_EXT_MASK          (RDISK_EXT_SIZE - 1)
#define RDISK_BUF_SIZE_EXT(buff) (((buff) & RDISK_EXT_MASK) ? \
				  ((buff) & RDISK_EXT_MASK) : RDISK_EXT_SIZE)
#define RDISK_BUF_BASE_EXT(buff) ((buff) >> RDISK_EXT_SHIFT)
#endif

#define VBD_PAGE_64_MASK	~((NkPhAddr)0xfff)
#ifndef CONFIG_NKERNEL_VBD_NR
#define	CONFIG_NKERNEL_VBD_NR	64
#endif

#define	VBD_BLKIF_MAX_SEGS_PER_REQ	128
#define	VBD_BLKIF_DEFAULT_RING_IMASK 0x3f
#define	VBD_BLKIF_DESC_SIZE(bl)	\
	(sizeof(RDiskReqHeader) + (sizeof(RDiskBuffer) * (bl)->segs_per_req))
#define	VBD_BLKIF_RING_SIZE(bl)	\
	(VBD_BLKIF_DESC_SIZE(bl) * ((bl)->ring_imask + 1))

#define	VBD_DISK_ID(major,minor)	(((major) << 8) | (minor))
#define	VBD_DISK_MAJOR(id)	((id) >> 8)
#define	VBD_DISK_MINOR(id)	((id) & 0xff)

    /*
     *  The driver can defer nk_dev_add() for a given frontend till
     *  specific vdisks are available, or better, till their sizes
     *  are also non-zero. This allows e.g. to hold the boot of a
     *  frontend guest using a CD for root until losetup(1) is
     *  performed in the backend guest.
     */
typedef enum {
    VBD_VDISK_WAIT_NO,		/* Do not wait for this vdisk */
    VBD_VDISK_WAIT_YES,		/* Wait for this vdisk */
    VBD_VDISK_WAIT_NON_ZERO	/* Wait for vdisk to be non-zero sized */
} vbd_vdisk_wait_t;

static const char vbd_vdisk_wait_modes[3][3] = {"nw", "wa", "nz"};

    /*
     * The "vdisk" property identifies a virtual disk.
     */
typedef struct {
    nku32_f tag;	/* vdisk tag */
    nku32_f owner;	/* vdisk owner (frontend) */
    nku16_f id;		/* vdisk ID */
    nku16_f ring_imask;	/* must be of "(2^n)-1" form */
    nku16_f segs_per_req; /* value limited to VBD_BLKIF_MAX_SEGS_PER_REQ */
    vbd_vdisk_wait_t wait;	/* vdisk wait mode (none, existence, non-0) */
} VbdPropDiskVdisk;

#define VBD_PROP_DISK_VDISK_IS_DEFAULT(prop) \
    ((prop)->ring_imask   == VBD_BLKIF_DEFAULT_RING_IMASK && \
     (prop)->segs_per_req == VBD_BLKIF_MAX_SEGS_PER_REQ)

    /*
     * The "extent" property identifies a virtual disk extent.
     */
typedef struct {
    nku32_f start;		/* start sector */
    nku32_f size;		/* size in sectors (0 - up to the end) */
    nku32_f access;		/* access rights */
    nku32_f minor;		/* disk minor */
    nku32_f tag;		/* vdisk tag */
    int     major;		/* disk major (-1 if unused) */
    _Bool   bound;		/* extent is bound to the real device */
} VbdPropDiskExtent;

#define	VBD_DISK_ACC_R	0x1
#define	VBD_DISK_ACC_W	0x2
#define	VBD_DISK_ACC_RW	(VBD_DISK_ACC_R | VBD_DISK_ACC_W)

#undef	DEBUG

#ifdef  DEBUG
#define TRACE(_f, _a...)  printk (KERN_ALERT "VBD-BE: " _f , ## _a)
#define DTRACE(_f, _a...) \
	do {printk (KERN_ALERT "%s: " _f, __func__, ## _a);} while (0)
#define XTRACE(_f, _a...)
#else
#define TRACE(_f, _a...)  printk (KERN_INFO  "VBD-BE: " _f , ## _a)
#define DTRACE(_f, _a...)
#define XTRACE(_f, _a...)
#endif
#define ETRACE(_f, _a...) printk (KERN_ALERT "VBD-BE: Error: " _f , ## _a)

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

typedef struct vbd_fast_map_t vbd_fast_map_t;
typedef struct vbd_pending_req_t vbd_pending_req_t;

    /*
     * Block interface descriptor
     */
typedef struct VbdBlkif {
    NkPhAddr           pdev;		/* device physical address */
    NkDevDesc*         vdev;		/* device descriptor */
    NkDevRing*         ring;		/* ring descriptor */
    spinlock_t         ring_iresp_lock;	/* ring response index lock */
    volatile int       connected;	/* front-end driver is connected */
	/* "connected" actually just means today that the VM is running */
    nku16_f            ring_imask;
    nku16_f            segs_per_req;
    unsigned long      pring;		/* disk ring physical address */
    unsigned long      psize;		/* disk ring physical size */
    void* 	       vring;		/* disk ring virtual address */
    nku32_f	       req;		/* request pointer */
    NkXIrqId           xid;		/* connected XIRQ ID */
    struct VbdVdisk*   vdisks;		/* virtual disks */
    struct VbdBlkif*   next;		/* next block interface */
    struct VbdBe*      be;		/* parent disk driver */
    struct list_head   blkdev_list;	/* (if set) on blkio_schedule_list */
    atomic_t           refcnt;
    vbd_fast_map_t*    fmap;		/* fast map */
    spinlock_t         flock;	  	/* fast map lock */
    struct list_head   done_list;	/* done list */
    spinlock_t         done_lock;	/* done list lock */
    _Bool              nk_dev_added;	/* device added in nanokernel */
    vbd_pending_req_t* pending_xreq;	/* if resource error */

	/* Performance counters */
    unsigned long long	bytesRead;
    unsigned long long	bytesWritten;

    unsigned		fastMapProbes;
    unsigned		fastMapReads;
    unsigned		fastMapWrites;
    unsigned long long	fastMapReadBytes;
    unsigned long long	fastMapWrittenBytes;

    unsigned		nkMemMapProbes;
    unsigned		nkMemMapReads;
    unsigned		nkMemMapWrites;
    unsigned long long  nkMemMapReadBytes;
    unsigned long long  nkMemMapWrittenBytes;

    unsigned		dmaReads;
    unsigned		dmaWrites;
    unsigned long long  dmaReadBytes;
    unsigned long long  dmaWrittenBytes;

    unsigned		xirqTriggers;
    unsigned		requests;
    unsigned		segments;
    unsigned		noStructPage;

    unsigned		pageAllocs;
    unsigned		pageFreeings;
    unsigned		allocedPages;
    unsigned		maxAllocedPages;

    unsigned		firstMegCopy;
    unsigned		resourceErrors;
} VbdBlkif;

#define VBD_BLKIF_FOR_ALL_VDISKS(_vd,_bl) \
    for ((_vd) = (_bl)->vdisks; (_vd); (_vd) = (_vd)->next)

#ifdef VBD_ATAPI
    /*
     * Virtual disk ATAPI descriptor
     */
typedef struct atapi_req {
    RDiskCookie		cookie;
    RDiskDevId          devid;
    RDiskOp		op;
    NkPhAddr		paddr;
    uint8_t             cdb[RDISK_ATAPI_PKT_SZ];
    uint32_t		buflen;
} atapi_req;

typedef struct atapi_t {
    atapi_req		req;	/* pending ATAPI request from frontend */
    uint32_t            count;	/* request count (0-1) */
    wait_queue_head_t	wait;
    struct mutex	lock;
    int 		ctrl_open;
    int 		data_open;
    char                ctrl_name[32];
    char                data_name[32];
} atapi_t;
#endif /* VBD_ATAPI */

    /*
     * Virtual disk descriptor
     */
typedef struct VbdVdisk {
    nku32_f		tag;		/* virtual disk tag */
    RDiskDevId		id;		/* virtual disk ID */
    struct VbdExtent*	extents;	/* list of extents */
    struct VbdVdisk*	next;		/* next virtual disk */
    VbdBlkif*		blkif;		/* block interface */
    vbd_vdisk_wait_t	wait;		/* vdisk wait mode (none, existence,
					   non-zero size) */
    _Bool		extents_ok;	/* tmp used during extent scanning */
    char		name [24];	/* (guest,major,minor) string */
#ifdef VBD_ATAPI
    atapi_t*		atapi;		/* ATAPI device support */
#endif
} VbdVdisk;

#define VBD_VDISK_FOR_ALL_EXTENTS(_ex,_vd) \
    for ((_ex) = (_vd)->extents; (_ex); (_ex) = (_ex)->next)

    /*
     * Virtual disk extent descriptor
     */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
typedef sector_t VbdDiskSize;
#else
typedef u64 VbdDiskSize;
#endif

typedef struct VbdExtent {
    struct VbdExtent*		next;	/* next extent */
    VbdDiskSize			start;	/* start sector */
    VbdDiskSize			size;	/* size in sectors */
    dev_t			dev;	/* real device id */
    struct block_device*	bdev;	/* real block device */
    nku32_f			access;	/* access rights */
    const VbdPropDiskExtent*	prop;	/* property */
    VbdVdisk*			vdisk;	/* virtual disk */
} VbdExtent;

    /*
     * Driver descriptor
     */
typedef struct VbdBe {
    NkXIrqId		xid;		/* SYSCONF XIRQ ID */
    VbdBlkif*		blkifs;		/* interfaces */
    atomic_t		refcnt;		/* reference count */
    _Bool		resource_error;
} VbdBe;

#define VBD_BE_FOR_ALL_BLKIFS(_bl, _be) \
    for ((_bl) = (_be)->blkifs; (_bl); (_bl) = (_bl)->next)

static VbdPropDiskExtent vbdextent[CONFIG_NKERNEL_VBD_NR];

static VbdBe vbd_be;	/* Driver global data */

#define	VBD_BLKIF(x)	((VbdBlkif*)(x))
#define	VBD_VDISK(x)	((VbdVdisk*)(x))
#define	VBD_EXTENT(x)	((VbdExtent*)(x))
#define	VBD_BE(x)	((VbdBe*)(x))

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static int vdisk_dma = 1; // DMA (default) or COPY mode
#endif

/*
 * Maximum number of requests pulled from a given blkif interface ring
 * before scheduling the next one.
 * As there is only one kernel thread currently for all the blkif interfaces,
 * it gives a chance to all interfaces to be scheduled in a more fair way.
 */
#define VBD_MAX_REQ_PER_BLKIF 16

/*
 * Each outstanding request that we've passed to the lower device layers has a
 * 'pending_req' allocated to it. Each buffer_head that completes decrements
 * the pendcnt towards zero. When it hits zero, a response is queued for
 * with the saved 'cookie' passed back.
 */
typedef struct vbd_pending_seg_t {
    NkPhAddr      gaddr;	/* only used when reading in copy mode */
    unsigned long vaddr;
    unsigned int  size;		/* only used when reading in copy mode */
} vbd_pending_seg_t;

struct vbd_pending_req_t {
    VbdBlkif*		blkif;
    RDiskCookie		cookie;
    RDiskOp		op;
    int			error;
    atomic_t		pendcnt;
    struct list_head	list;
    nku32_f		nsegs;
    vbd_pending_seg_t	segs[VBD_BLKIF_MAX_SEGS_PER_REQ];
    RDiskSector		vsector;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static kmem_cache_t* vbd_pending_req_cachep;
#else
static struct kmem_cache* vbd_pending_req_cachep;
#endif

static DECLARE_WAIT_QUEUE_HEAD(vbd_blkio_schedule_wait);

    // only used by vbd_blkif_create()
#define vbd_get(_v) (atomic_inc(&(_v)->refcnt))

    // only used by vbd_blkif_release(), only used by vbd_blkif_put()
#define vbd_put(_v)                             \
    do {                                        \
	if (atomic_dec_and_test(&(_v)->refcnt)) \
	    vbd_release(_v);                    \
    } while (0)

#define vbd_blkif_get(_b) (atomic_inc(&(_b)->refcnt))

#define vbd_blkif_put(_b)                           \
    do {                                        \
	if (atomic_dec_and_test(&(_b)->refcnt)) \
	    vbd_blkif_release(_b);                  \
    } while (0)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

static kmem_cache_t* vbd_buffer_head_cachep;

#else

#define	VBD_BIO_MAX_VECS	(BIO_MAX_PAGES/2)

#if defined(CONFIG_X86)

struct vbd_fast_map_t {
    void*			addr;
    struct vbd_fast_map_t*	next;
#if defined(CONFIG_X86_PAE) || defined(CONFIG_X86_64)
    u64*			pte;
#ifdef DEBUG
    u64				inactive_pte;
    u64				active_pte;
#endif
#else
    u32*			pte;
#ifdef DEBUG
    u32				inactive_pte;
    u32				active_pte;
#endif
#endif
};

#ifdef DEBUG
    static void
vbd_x86_map_print (const char* func, const unsigned line,
		   const vbd_fast_map_t* map)
{
    ETRACE ("%s:%d addr %p next %p ptep %p *ptep %llx inactive %llx "
	    "active %llx\n", func, line, map->addr, map->next, map->pte,
	    (u64) *map->pte, (u64) map->inactive_pte, (u64) map->active_pte);
}

#define VBD_X86_MAP_CHECK(map,field) \
    if ((*(map)->pte & ~0x70) != ((map)->field & ~0x70)) { \
	vbd_x86_map_print (__func__, __LINE__, (map)); \
    }
#define VBD_X86_MAP_SAVE(map,field,val)	(map)->field = (val)
#else
#define VBD_X86_MAP_CHECK(map,field)	do {} while (0)
#define VBD_X86_MAP_SAVE(map,field,val)	do {} while (0)
#endif

#if defined(CONFIG_X86_64)
    static u64*
vbd_x86_get_ptep (void* vaddr)
{
    u64 pte;
    __asm__ __volatile__("movq  %%cr3, %0" : "=r"(pte));
    pte = ((u64*)__va(pte & PAGE_MASK))[(((u64)vaddr) >> 39) & 0x1ff];
    pte = ((u64*)__va(pte & PAGE_MASK))[(((u64)vaddr) >> 30) & 0x1ff];
    pte = ((u64*)__va(pte & PAGE_MASK))[(((u64)vaddr) >> 21) & 0x1ff];
    return ((u64*)__va(pte & PAGE_MASK)) + ((((u64)vaddr) >> 12) & 0x1ff);
}
#elif defined(CONFIG_X86_PAE)
    static u64*
vbd_x86_get_ptep (void* vaddr)
{
    u32  cr3;
    u64  pte;
    __asm__ __volatile__("movl  %%cr3, %0" : "=r"(cr3));
    pte = ((u64*)__va(cr3))[(((u32)vaddr) >> 30) & 0x3];
    pte = ((u64*)__va(pte & VBD_PAGE_64_MASK))[(((u32)vaddr) >> 21) & 0x1ff];
    return ((u64*)__va(pte & VBD_PAGE_64_MASK)) + (((u32)vaddr >> 12) & 0x1ff);
}
#else
    static u32*
vbd_x86_get_ptep (void* vaddr)
{
    u32 pte;
    __asm__ __volatile__("movl  %%cr3, %0" : "=r"(pte));
    pte = ((u32*)__va(pte & PAGE_MASK))[(((u32)vaddr) >> 22) & 0x3ff];
    return ((u32*)__va(pte & PAGE_MASK)) + ((((u32)vaddr) >> 12) & 0x3ff);
}
#endif

    // Called from vbd_blkif_create(),
    //    called from vbd_vdisk_create(), called from vbd_module_init().

    static void __init
vbd_x86_map_create (VbdBlkif* bl)
{
	//             63+1                   128                 256/2
    int pages = ((bl->ring_imask+1) * bl->segs_per_req) / VBD_BIO_MAX_VECS;

    spin_lock_init(&bl->flock);

    bl->fmap  = 0;

    while (pages--) {
	void*       addr;
	vbd_fast_map_t* map;

        addr = nkops.nk_mem_map((unsigned int)PAGE_MASK, (1 << PAGE_SHIFT));
 	if (unlikely(!addr)) {
	    ETRACE("Fast mapping allocation failure\n");
	    return;
	}

	map = kmalloc(sizeof(vbd_fast_map_t), GFP_KERNEL);
 	if (unlikely(!map)) {
	    nkops.nk_mem_unmap(addr, (unsigned int)PAGE_MASK,
			       (1 << PAGE_SHIFT));
	    ETRACE("Fast mapping allocation failure\n");
	    return;
	}
	map->addr = addr;
	map->pte  = vbd_x86_get_ptep(addr);
	VBD_X86_MAP_SAVE (map, inactive_pte, *map->pte);
	map->next = bl->fmap;
	bl->fmap  = map;
	DTRACE("vaddr 0x%p ptep 0x%p (pte 0x%llx)\n",
	       map->addr, map->pte, (u64)*map->pte);
    }
}

    static void
vbd_x86_map_delete (VbdBlkif* bl)
{
    while (bl->fmap) {
	vbd_fast_map_t* map = bl->fmap;
	bl->fmap = map->next;
	nkops.nk_mem_unmap(map->addr, (unsigned int)PAGE_MASK,
			   (1 << PAGE_SHIFT));
	kfree(map);
    }
}

    static vbd_fast_map_t*
vbd_x86_map_alloc (VbdBlkif* bl)
{
    unsigned long flags;
    vbd_fast_map_t*   map;
    spin_lock_irqsave(&bl->flock, flags);
    map = bl->fmap;
    if (map) {
        bl->fmap = map->next;
    }
    spin_unlock_irqrestore(&bl->flock, flags);
    return map;
}

    static void
vbd_x86_map_free (VbdBlkif* bl, vbd_fast_map_t* map)
{
    unsigned long flags;
    spin_lock_irqsave(&bl->flock, flags);
    map->next = bl->fmap;
    bl->fmap = map;
    spin_unlock_irqrestore(&bl->flock, flags);
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

#if defined(CONFIG_X86_64) || defined(CONFIG_X86_PAE)

    static inline void
vbd_x86_map (vbd_fast_map_t* map, u64 paddr)
{
    preempt_disable();
    VBD_X86_MAP_CHECK(map, inactive_pte);
    *map->pte = paddr | (1 << 8) | (1 << 1) | (1 << 0);
    VBD_X86_MAP_SAVE(map, active_pte, paddr | (1 << 8) | (1 << 1) | (1 << 0));
    vbd_x86_flush((char*)map->addr);
}

#else

    static inline void
vbd_x86_map (vbd_fast_map_t* map, u32 paddr)
{
    preempt_disable();
    VBD_X86_MAP_CHECK(map, inactive_pte);
    *map->pte = paddr | (1 << 8) | (1 << 1) | (1 << 0);
    VBD_X86_MAP_SAVE(map, active_pte, paddr | (1 << 8) | (1 << 1) | (1 << 0));
    vbd_x86_flush((char*)map->addr);
}

#endif

    static inline void
vbd_x86_unmap (vbd_fast_map_t* map)
{
    VBD_X86_MAP_CHECK(map, active_pte);
    *map->pte = (unsigned int)PAGE_MASK | (1 << 8) | (1 << 1) | (1 << 0);
    vbd_x86_flush((char*)map->addr);
    VBD_X86_MAP_CHECK(map, inactive_pte);
    preempt_enable();
}

#define	vbd_fast_map_create	vbd_x86_map_create
#define	vbd_fast_map_delete	vbd_x86_map_delete
#define	vbd_fast_map_alloc	vbd_x86_map_alloc
#define	vbd_fast_map_free	vbd_x86_map_free
#define	vbd_fast_map		vbd_x86_map
#define	vbd_fast_unmap		vbd_x86_unmap

#else

struct vbd_fast_map_t {
    void* addr;
};

    static inline void
vbd_def_map_create (VbdBlkif* bl)
{
}

    static inline void
vbd_def_map_delete (VbdBlkif* bl)
{
}

    static inline vbd_fast_map_t*
vbd_def_map_alloc (VbdBlkif* bl)
{
    return NULL;
}

    static inline void
vbd_def_map_free (VbdBlkif* bl, vbd_fast_map_t* map)
{
}

    static inline void
vbd_def_map (vbd_fast_map_t* map, unsigned long paddr)
{
}

    static inline void
vbd_def_unmap (vbd_fast_map_t* map)
{
}

#define	vbd_fast_map_create	vbd_def_map_create
#define	vbd_fast_map_delete	vbd_def_map_delete
#define	vbd_fast_map_alloc	vbd_def_map_alloc
#define	vbd_fast_map_free	vbd_def_map_free
#define	vbd_fast_map		vbd_def_map
#define	vbd_fast_unmap		vbd_def_unmap

#endif  /* defined(CONFIG_X86) */

#endif	/* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) */

/*
 * Block interface (blkif)
 */
    static void
vbd_blkif_free (VbdBlkif* bl)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    vbd_fast_map_delete(bl);
#endif
    kfree(bl);
}

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

    static VbdBlkif*
vbd_blkif_alloc (void)
{
    void* bl;

    if (unlikely((bl = kzalloc(sizeof(VbdBlkif), GFP_KERNEL)) == NULL)) {
	ETRACE("vbd_blkif_alloc: out of memory\n");
	return NULL;
    }
    return VBD_BLKIF(bl);
}

static void vbd_blkif_interrupt (void* cookie, NkXIrq xirq);

    // Only called by vbd_vdisk_create(), called from vbd_module_init().

    static VbdBlkif* __init
vbd_blkif_create (VbdBe* be, const VbdPropDiskVdisk* prop,
		  const _Bool nk_dev_add)
{
    VbdBlkif*  bl;
    NkPhAddr   pdev;
    NkDevDesc* vdev = 0;
    NkDevRing* ring = 0;

    VBD_BE_FOR_ALL_BLKIFS(bl, be) {
	if (bl->ring->pid == prop->owner) break;
    }
    if (bl) {
	if ((bl->ring_imask   != prop->ring_imask ||
	     bl->segs_per_req != prop->segs_per_req) &&
	    !VBD_PROP_DISK_VDISK_IS_DEFAULT(prop)) {
	    ETRACE("Incompatible <elems> or <segs> parameters for "
		   "disk (%d,%d,%d)\n", prop->owner, VBD_DISK_MAJOR(prop->id),
		   VBD_DISK_MINOR(prop->id));
	}
	return bl;
    }
    bl = vbd_blkif_alloc();
    if (!bl) {
	return NULL;
    }
    pdev = 0;
    while ((pdev = nkops.nk_dev_lookup_by_type(NK_DEV_ID_RING, pdev))) {
	vdev = (NkDevDesc*)nkops.nk_ptov(pdev);
	ring = (NkDevRing*)nkops.nk_ptov(vdev->dev_header);
	if ((ring->type == RDISK_RING_TYPE) && (ring->pid == prop->owner)) {
	    break;
	}
    }
	/*
	 * allocate and initialize new block interface if not found
	 */
    if (!pdev) {
	    /* required by VBD_BLKIF_DESC_SIZE(bl) & VBD_BLKIF_RING_SIZE(bl) */
	bl->ring_imask = prop->ring_imask;
	bl->segs_per_req = prop->segs_per_req;

	pdev = nkops.nk_dev_alloc(sizeof(NkDevDesc) +
				  sizeof(NkDevRing) +
				  VBD_BLKIF_RING_SIZE(bl));
        if (!pdev) {
	    ETRACE("ring device allocation failure\n");
	    vbd_blkif_free(bl);
	    return NULL;
	}
        vdev = (NkDevDesc*)nkops.nk_ptov(pdev);
        memset(vdev, 0, sizeof(NkDevDesc)+sizeof(NkDevRing));

        vdev->class_id   = NK_DEV_CLASS_GEN;
        vdev->dev_id     = NK_DEV_ID_RING;
        vdev->dev_header = pdev + sizeof(NkDevDesc);
	vdev->dev_owner  = nkops.nk_id_get();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	if (vdisk_dma) {
	    TRACE("DMA mode preferred\n");
	} else {
	    TRACE("COPY mode required\n");
	    vdev->class_id |= (1 << 31);	// COPY mode hint
 	}
#endif

	ring = nkops.nk_ptov(vdev->dev_header);

	ring->cxirq = nkops.nk_xirq_alloc(1);
	if (!ring->cxirq) {
	    ETRACE("cross IRQ allocation failure\n");
	    vbd_blkif_free(bl);
	    return NULL;
	}
	ring->pid   = prop->owner;
	ring->type  = RDISK_RING_TYPE;
	ring->dsize = VBD_BLKIF_DESC_SIZE(bl);
	ring->imask = bl->ring_imask;

	bl->psize = VBD_BLKIF_RING_SIZE(bl);
	bl->pring = vdev->dev_header + sizeof(NkDevRing);
	bl->vring = nkops.nk_ptov(bl->pring);

	ring->base = bl->pring;
	    /*
	     *  If requested, do not add the device now
	     *  and wait for all vdisks to become ready.
	     */
	if (nk_dev_add) {
	    nkops.nk_dev_add(pdev);
	}
	TRACE("new ring interface (%d) created [elems %d dsize 0x%x "
	      "psize 0x%lx]\n", ring->pid, ring->imask+1, ring->dsize,
	      bl->psize);
    } else {
	TRACE("existing ring interface (%d) found\n", ring->pid);

	    /* required by VBD_BLKIF_DESC_SIZE(bl) & VBD_BLKIF_RING_SIZE(bl) */
	bl->ring_imask = ring->imask;
	bl->segs_per_req = (ring->dsize - sizeof(RDiskReqHeader))
			    / sizeof(RDiskBuffer);

	bl->psize = VBD_BLKIF_RING_SIZE(bl);
	bl->pring = ring->base;
	bl->vring = nkops.nk_ptov(bl->pring);
	vdev->dev_lock++;	// incarnation number
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	/* This map can also be used in "DMA" mode if necessary */
    vbd_fast_map_create(bl);

    if (!bl->fmap) {
	ETRACE ("(%d) no fast map, performance will be reduced\n", ring->pid);
    }
#ifdef CONFIG_X86
    if (!pfn_valid ((800*1024*1024) >> PAGE_SHIFT)) {
	ETRACE ("(%d) no high page, performance will be reduced\n", ring->pid);
    }
#endif
#endif

    spin_lock_init(&bl->done_lock);
    INIT_LIST_HEAD(&bl->done_list);

    bl->pdev  = pdev;
    bl->vdev  = vdev;
    bl->ring  = ring;
    bl->req   = ring->iresp;
    bl->nk_dev_added = nk_dev_add;
    spin_lock_init(&bl->ring_iresp_lock);

    bl->be    = be;
    bl->next  = be->blkifs;
    be->blkifs = bl;

    vbd_get(be);

	/*
	 *  Send SYSCONF only when all vdisks are ready.
	 *  Holding sysconf here is not very effective,
	 *  as a sysconf could be sent by some other driver
	 *  or from another guest.
	 */
    if (nk_dev_add) {
	nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, prop->owner);
    }
    return bl;
}

static void vbd_extents_disconnect (VbdVdisk* vd);

    static inline void
vbd_vdisk_free (VbdVdisk* vd)
{
    kfree(vd);
}

    // Called from vbd_destroy() only, called from vbd_module_exit() only.

#ifdef VBD_ATAPI
static void __exit vbd_atapi_exit (VbdVdisk* vd);
#endif

    static void __exit
vbd_blkif_destroy (VbdBlkif* bl)
{
    VbdVdisk* vd;

    if (!bl) return;

    while ((vd = bl->vdisks)) {
	bl->vdisks = vd->next;
	vbd_extents_disconnect(vd);
#ifdef VBD_ATAPI
	vbd_atapi_exit(vd);
#endif
	vbd_vdisk_free(vd);
    }
    vbd_blkif_free(bl);
}

    /*
     * Activate block interface (connect interrupt handler)
     * Called from vbd_start() only, called from vbd_module_init().
     */

    static void __init
vbd_blkif_start (VbdBlkif* bl)
{
    NkOsMask running = nkops.nk_running_ids_get();
    NkOsMask mask    = nkops.nk_bit2mask(bl->ring->pid);

    vbd_blkif_get(bl);
	/*
	 *  We must set bl->connected before attaching the
	 *  cross-interrupt handler vbd_blkif_interrupt(bl),
	 *  because if the cross-interrupt is already
	 *  pending, the handler will be called immediately,
	 *  and if bl->connected is not set, the handler will
	 *  terminate all pending requests with an error status
	 *  for no good reason.
	 */
    if (running & mask) {
        bl->connected = 1;
        DTRACE("front-end driver %d connected\n", bl->ring->pid);
    }
    bl->xid = nkops.nk_xirq_attach(bl->ring->cxirq,
				   vbd_blkif_interrupt, bl);
    if (!bl->xid) {
	ETRACE("XIRQ %d attach failure\n", bl->ring->cxirq);
	return;
    }
    vbd_blkif_interrupt(bl, bl->ring->cxirq);
}

    static void
vbd_release (VbdBe* be)
{
	// awake the thread
    wake_up(&vbd_blkio_schedule_wait);
}

    // only used by vbd_blkif_put().

    static void
vbd_blkif_release (VbdBlkif* bl)
{
    vbd_put(bl->be);
}

    static void __exit
vbd_blkif_stop (VbdBlkif* bl)
{
    if (bl->xid) {
        nkops.nk_xirq_detach(bl->xid);
    }
    vbd_blkif_put(bl);
}

    /*
     *  This routine is called both from the cross-interrupt through
     *  which requests are received from the frontend, in case of
     *  synchronous requests which can be answered immediately,
     *  like probes, and from hardware disk interrupt, in case of
     *  asynchronous requests which really went to disk. These two interrupts
     *  can nest. This becomes visible if a printk() is issued after the
     *  reading of iresp and before modifying it. Before the printk()
     *  for the sync request has finished, the asynchronous request comes
     *  and reuses the same response index position. This results
     *  in a "BDEV request lookup failure: 0x0" error from the
     *  nanokernel and a loss of the sync answer to frontend.
     */

    static void
vbd_blkif_resp (VbdBlkif* bl, RDiskCookie cookie, RDiskOp op,
		RDiskStatus status)
{
    NkDevRing*    ring = bl->ring;
    nku8_f*       base = bl->vring;
    unsigned long flags;
    RDiskResp*    resp;

    spin_lock_irqsave(&bl->ring_iresp_lock, flags);
    resp = (RDiskResp*)(base + ((ring->iresp & ring->imask) * ring->dsize));
    resp->cookie  = cookie;
    resp->op      = op;
    resp->status  = status;

    ring->iresp++;
    spin_unlock_irqrestore(&bl->ring_iresp_lock, flags);
}

    static void
vbd_blkif_resp_trigger (VbdBlkif* bl, RDiskCookie cookie, RDiskOp op,
			RDiskStatus status)
{
    NkDevRing*    ring = bl->ring;
    nku8_f*       base = bl->vring;
    unsigned long flags;
    RDiskResp*    resp;

    spin_lock_irqsave(&bl->ring_iresp_lock, flags);

    resp = (RDiskResp*)(base + ((ring->iresp & ring->imask) * ring->dsize));
    resp->cookie  = cookie;
    resp->op      = op;
    resp->status  = status;

    ring->iresp++;

    nkops.nk_xirq_trigger(ring->pxirq, ring->pid);
    ++bl->xirqTriggers;

    spin_unlock_irqrestore(&bl->ring_iresp_lock, flags);
}

static void vbd_vdisk_probe (const VbdVdisk* vd, RDiskProbe* probe);

    static VbdVdisk*
vbd_blkif_vdisk_lookup (VbdBlkif* bl, RDiskDevId id)
{
    VbdVdisk* vd;

    VBD_BLKIF_FOR_ALL_VDISKS(vd, bl) {
	if (vd->id == id) {
	    return vd;
	}
    }
    return NULL;
}

    static VbdPropDiskExtent*
vbd_prop_disk_extent_lookup (nku32_f tag)
{
    int i;

    for (i = 0; i < CONFIG_NKERNEL_VBD_NR; ++i) {
	if (vbdextent[i].tag == tag) {
	    return &vbdextent[i];
	}
    }
    return 0;
}

static _Bool vbd_vdisk_extent_create (VbdVdisk* vd, VbdPropDiskExtent* prop);

    // Operations called by vbd_blkif_probe() for each requests
typedef RDiskStatus (*vbd_probe_op)(      VbdBlkif*       bl,
				    const RDiskReqHeader* req,
				          void*           buffer,
				          NkPhSize        psize);

    // Initial probing of vdisks
    static RDiskStatus
vbd_op_probe (      VbdBlkif*       bl,
	      const RDiskReqHeader* req,
	            void*	    buffer,
	            NkPhSize        psize)
{
    const VbdVdisk*	vd;
          RDiskProbe*   probe  = buffer;
          RDiskStatus	status = 0;

    VBD_BLKIF_FOR_ALL_VDISKS(vd, bl) {
	if (psize < sizeof(RDiskProbe)) break;
	    // If creating extents has failed at module insertion,
	    // then try again at probe time
	if (!vd->extents) {
	    vbd_vdisk_extent_create((VbdVdisk*)vd,
				    vbd_prop_disk_extent_lookup(vd->tag));
	}
	vbd_vdisk_probe(vd, probe);
	psize -= sizeof(RDiskProbe);
	probe++;
	status++;
    }

    return status;
}

#if	LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define GENDISK(bdev)		(bdev)->bd_disk

#if	LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
#define BDEV_CLOSE(bdev,mode) 	blkdev_put(bdev, mode) 
#else
#define BDEV_CLOSE(bdev,mode)	blkdev_put(bdev)
#endif

#else
#define GENDISK(bdev)		get_gendisk((bdev)->bd_inode->i_rdev)
#define BDEV_CLOSE(bdev,mode)
#endif

#if	LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
typedef int (*fops_ioctl) (struct block_device*, fmode_t, unsigned,
			   unsigned long);
#define GET_IOCTL(fops)							\
  ((fops)->ioctl ? (fops)->ioctl : (fops)->compat_ioctl)
#define DO_IOCTL(ioctl,bdev,mode,cmd,arg) ioctl(bdev, mode, cmd, arg)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
    /*
     *  This code is suitable for versions from 2.6.27 down to 2.6.17
     *  at least, but might be required for older releases as well.
     */
typedef int (*fops_ioctl) (struct inode*, struct file*, unsigned,
			   unsigned long);
#define GET_IOCTL(fops)	fops->ioctl
#define DO_IOCTL(ioctl,bdev,mode,cmd,arg) ioctl(bdev->bd_inode, \
						(struct file*) NULL, cmd, arg)
#else
typedef int (*fops_ioctl) (struct inode*, fmode_t, unsigned, unsigned long);
#define GET_IOCTL(fops)	fops->ioctl
#define DO_IOCTL(ioctl,bdev,mode,cmd,arg) ioctl(bdev->bd_inode, mode, cmd, arg)
#endif

    // Removable media probing (called periodically by the front-end)
    // Use for virtual ATAPI - LOOP device only
    static RDiskStatus
vbd_op_media_probe (      VbdBlkif*       bl,
		    const RDiskReqHeader* req,
			  void*           buffer,
			  NkPhSize        psize)
{
    VbdVdisk*                             vd;
    VbdExtent*                            ex;
    struct gendisk*                       gdisk;
    const struct block_device_operations* fops;
    RDiskProbe*			     	  probe = buffer;

    probe->devid = req->devid;

    vd = vbd_blkif_vdisk_lookup(bl, req->devid);
    if (!vd || (psize < sizeof(RDiskProbe))) {
	ETRACE("%s: invalid request\n", __FUNCTION__);
	return RDISK_STATUS_ERROR;
    }
    if (!vd->extents) {
	if (!vbd_vdisk_extent_create(vd,
				     vbd_prop_disk_extent_lookup(vd->tag))) {
	    DTRACE("no extent!\n");
	    return RDISK_STATUS_ERROR;
	}
    }
    ex = vd->extents;
    if (!ex || ex->next) {
	ETRACE("%s: exactly 1 extent expected for removable media\n",
	       __FUNCTION__);
	return RDISK_STATUS_ERROR;
    }
    if (!(gdisk = GENDISK(ex->bdev))) {
	ETRACE("%s: no gendisk!\n", __FUNCTION__);
	return RDISK_STATUS_ERROR;
    }
    if (!(fops = gdisk->fops)) {
	ETRACE("%s: no fops!\n", __FUNCTION__);
	return RDISK_STATUS_ERROR;
    }
    vbd_vdisk_probe(vd, probe);

    if (!probe->size[0]) {
	DTRACE("zero size - disconnecting\n");
	vbd_extents_disconnect(vd);
	return RDISK_STATUS_ERROR;
    }

    return RDISK_STATUS_OK;
}

    // Removable media Load/Eject
    // Use for virtual ATAPI - LOOP device only
    static RDiskStatus
vbd_op_media_control (      VbdBlkif*       bl,
		      const RDiskReqHeader* req,
			    void*	    buffer,
			    NkPhSize        psize)
{
    VbdVdisk*                             vd;
    VbdExtent*                            ex;
    struct gendisk*                       gdisk;
    const struct block_device_operations* fops;
    RDiskSector                           flags;
    fops_ioctl                            ioctl;
    int                                   res;
    RDiskProbe*				  probe = buffer;

    probe->devid = req->devid;

    vd = vbd_blkif_vdisk_lookup(bl, req->devid);
    if (!vd || (psize < sizeof(RDiskProbe))) {
	ETRACE("%s: invalid request\n", __FUNCTION__);
	return RDISK_STATUS_ERROR;
    }
    ex = vd->extents;
    if (!ex || ex->next) {
	DTRACE("exactly 1 extent expected for removable media!\n");
	return RDISK_STATUS_ERROR;
    }
    if (!(gdisk = GENDISK(ex->bdev))) {
	ETRACE("%s: no gendisk!\n", __FUNCTION__);
	return RDISK_STATUS_ERROR;
    }
    if (!(fops = gdisk->fops)) {
	ETRACE("%s: no fops!\n", __FUNCTION__);
	return RDISK_STATUS_ERROR;
    }
    if (!(ioctl = GET_IOCTL(fops))) {
	ETRACE("%s: no ioctl fops !\n", __FUNCTION__);
	return RDISK_STATUS_ERROR;
    }

    flags = req->sector[0];
    DTRACE("flags=0x%x\n", flags);

        // Start always succeed with LOOP device

    if (!(flags & RDISK_FLAG_START)) {
	    // Stop always succeed with LOOP device
	if (flags & RDISK_FLAG_LOEJ) {
	        // Eject
	    DTRACE("eject - disconnecting\n");
	        // Detach the loop device
	        // -> will require the user to re-attach (new media)
	    res = DO_IOCTL(ioctl, ex->bdev, 0, LOOP_CLR_FD, 0);
	    vbd_extents_disconnect(vd);
	    if (res) {
		ETRACE("%s: eject failed (%d)\n", __FUNCTION__, res);
		probe->info |= RDISK_FLAG_LOCKED;
		return RDISK_STATUS_ERROR;
	    }
	}
    }

    return RDISK_STATUS_OK;
}

    // Removable media lock/unlock
    // Use for virtual ATAPI - LOOP device only
    static RDiskStatus
vbd_op_media_lock (      VbdBlkif*       bl,
		   const RDiskReqHeader* req,
			 void*		 buffer,
			 NkPhSize        psize)
{
    return RDISK_STATUS_OK; // Always succeed with LOOP device
}

#ifdef VBD_ATAPI
//
// Removable media - generic PACKET commmand (ATAPI)
//

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define PDE(i)	((struct proc_dir_entry *)((i)->u.generic_ip))
#endif

    static _Bool
vbd_is_atapi (VbdVdisk* vd, int* major, int* minor)
{
    VbdPropDiskExtent* prop = vbd_prop_disk_extent_lookup(vd->tag);

    if (prop && (prop->major == SCSI_CDROM_MAJOR)) {
	*major = prop->major;
	*minor = prop->minor;
	return TRUE;
    }
    return FALSE;
}

    static int
_vbd_atapi_ctrl_proc_open (struct inode* inode, struct file*  file)
{
    VbdVdisk* vd = PDE(inode)->data;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    MOD_INC_USE_COUNT;
#endif
    vd->atapi->ctrl_open++;
    return 0;
}

    static int
_vbd_atapi_ctrl_proc_release (struct inode* inode, struct file*  file)
{
    VbdVdisk* vd = PDE(inode)->data;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    MOD_DEC_USE_COUNT;
#endif
    vd->atapi->ctrl_open--;
    return 0;
}

    static loff_t
_vbd_atapi_ctrl_proc_lseek (struct file* file,
			    loff_t       off,
			    int          whence)
{
    loff_t    new;

    switch (whence) {
	case 0:  new = off; break;
	case 1:  new = file->f_pos + off; break;
	case 2:
	default: return -EINVAL;
    }
    if (new > sizeof(RDiskAtapiReq)) {
	return -EINVAL;
    }
    return (file->f_pos = new);
}

    static ssize_t
_vbd_atapi_ctrl_proc_read (struct file* file,
			   char*        buf,
			   size_t       count,
			   loff_t*      ppos)
{
    VbdVdisk* vd    = PDE(file->f_dentry->d_inode)->data;
    atapi_t*  atapi = vd->atapi;

    if (*ppos || count != (RDISK_ATAPI_PKT_SZ + sizeof(uint32_t))) {
	return -EINVAL;
    }

    if (wait_event_interruptible(atapi->wait, atapi->count)) {
	return -EINTR;
    }
    if (copy_to_user(buf, atapi->req.cdb, count)) {
	return -EFAULT;
    }
    *ppos += count;
    return count;
}

    /*
     * Check if extents need connect/disconnect on:
     */
#define TEST_UNIT_READY		0x00

    static void
_vbd_atapi_extent_check_tur (VbdVdisk* vd, RDiskAtapiReq* areq)
{
    if (areq->status) {	/* unit is not ready */
	if (vd->extents) {
	    DTRACE("disconnecting - media removed "
		   "status=%d key(%x/%x/%x)\n",
		   areq->status, areq->sense.sense_key,
		   areq->sense.asc, areq->sense.ascq);
	    vbd_extents_disconnect(vd);
	}
    } else {		/* unit is ready */
	if (!vd->extents) {
	    DTRACE("connecting - media present\n");
	    vbd_vdisk_extent_create(vd,
				    vbd_prop_disk_extent_lookup(vd->tag));
	}
    }
}
    /*
     * Check if extents need connect/disconnect on:
     */
#define GET_EVENT_STATUS_NOTIFICATION	0x4a
#define 	GESN_OPCHANGE		1	// Operational Change Class
#define 	GESN_MEDIA		4	// Media Class
#define 	GESN_MASK(e)		(1 << (e))
#define 	GESN_MEDIA_NEW		0x02    // Event
#define 	GESN_MEDIA_REMOVAL	0x03    // Event
#define 	GESN_MEDIA_PRESENT	0x02    // Status
#define 	GESN_TRAY_OPEN		0x01    // Status

    static _Bool
_vbd_atapi_extent_check_gesn (VbdVdisk* vd, uint8_t* reply)
{
    if ((reply[2] == GESN_MEDIA) && (reply[1] > 4)) { /* MEDIA event */
	if (reply[5] == GESN_MEDIA_PRESENT) {	/* Status */
	    if (!vd->extents) {
		/*
		 * HACK for Windows:
		 * The backend driver may have "consumed" the NewMedia
		 * event issued immediately after inserting the media.
		 * We just inject one to be sure the frontend will have it,
		 * as some (stupid) guests rely on the event and not on the
		 * status.
		 */
		reply[4] = GESN_MEDIA_NEW; /* NewMedia event */

		DTRACE("connecting - media present\n");
		vbd_vdisk_extent_create(vd,
					vbd_prop_disk_extent_lookup(vd->tag));
	    }
	} else {
	    if (vd->extents) {
		/*
		 * HACK for Windows:
		 * The backend driver may have "consumed" the MediaRemoval
		 * event issued immediately after ejecting the media.
		 * We just inject one to be sure the frontend will have it,
		 * as some (stupid) guests rely on the event and not on the
		 * status.
		 */
		reply[4] = GESN_MEDIA_REMOVAL; /* MediaRemoval event */

		DTRACE("disconnecting - media removed\n");
		vbd_extents_disconnect(vd);
	    }
	}

	return FALSE;
    }

    if ((reply[2] == GESN_OPCHANGE) && (reply[1] >= 6) && /* OPC event */
	(reply[4] == 0x02) && (reply[7] == 0x01)) {	  /* Feature change */
	if (!vd->extents) {
	    DTRACE("(lost?) MEDIA event needed\n");
	    return TRUE; /* notify user process that media event is needed */
	}
    }

    return FALSE;
}

    static ssize_t
_vbd_atapi_ctrl_proc_write (struct file* file,
			    const char*  buf,
			    size_t       size,
			    loff_t*      ppos)
{
    VbdVdisk*		vd = PDE(file->f_dentry->d_inode)->data;
    VbdBlkif*		bl = vd->blkif;
    atapi_t*    	atapi = vd->atapi;
    RDiskStatus         status;
    ssize_t             res;
    NkPhAddr		paddr;
    RDiskAtapiReq*	areq;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    vbd_fast_map_t*	map;
#endif
    if (!atapi->count ||
	(*ppos != (uint32_t)&(((RDiskAtapiReq*)0)->status)) ||
	(size  != (sizeof(uint32_t) + sizeof(RDiskAtapiSense)))) {
	return -EINVAL;
    }

    status = RDISK_STATUS_OK;
    res    = size;
    paddr  = atapi->req.paddr; /* ATAPI request is page aligned */
    *ppos  = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if ((map = vbd_fast_map_alloc(bl))) {
	vbd_fast_map(map, paddr);	/* preemption is disabled */
	++bl->fastMapProbes;
	areq = (RDiskAtapiReq*)map->addr;
	if (copy_from_user(&(areq->status), buf, size)) {
	    res    = -EFAULT;
	    status = RDISK_STATUS_ERROR;
	} else if (atapi->req.cdb[0] == TEST_UNIT_READY) {
	        /* Check if extent needs to be connected/disconnected */
	    _vbd_atapi_extent_check_tur(vd, areq);
	}
	vbd_fast_unmap(map);		/* preemption is enabled */
	vbd_fast_map_free(bl, map);
    } else {
#endif /* 2.6 */
	areq = (RDiskAtapiReq*)nkops.nk_mem_map(paddr, size);
	++bl->nkMemMapProbes;
	if (!areq) {
	    ETRACE("nk_mem_map(0x%llx, 0x%llx) failure\n", (long long) paddr,
		   (long long) size);
	    atapi->count = 0; /* Release process request */
	    vbd_blkif_resp_trigger(bl, atapi->req.cookie, atapi->req.op,
				   RDISK_STATUS_ERROR);
	    return -EFAULT;
	}
	if (copy_from_user(&(areq->status), buf, size)) {
	    res    = -EFAULT;
	    status = RDISK_STATUS_ERROR;
	} else if (atapi->req.cdb[0] == TEST_UNIT_READY) {
	        /* Check if extent needs to be connected/disconnected */
	    _vbd_atapi_extent_check_tur(vd, areq);
	}
	nkops.nk_mem_unmap(areq, paddr, size);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    }
#endif
    atapi->count = 0;  /* Release process request */
        /* Reply to front-end */
    vbd_blkif_resp_trigger(bl, atapi->req.cookie, atapi->req.op, status);

    return res;
}

    static int
_vbd_atapi_data_proc_open (struct inode* inode, struct file*  file)
{
    VbdVdisk*	vd = PDE(inode)->data;
    atapi_t*	atapi = vd->atapi;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    MOD_INC_USE_COUNT;
#endif
    atapi->data_open++;
    return 0;
}

    static int
_vbd_atapi_data_proc_release (struct inode* inode, struct file*  file)
{
    VbdVdisk*	vd = PDE(inode)->data;
    atapi_t*	atapi = vd->atapi;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    MOD_DEC_USE_COUNT;
#endif
    atapi->data_open--;
    return 0;
}

    static loff_t
_vbd_atapi_data_proc_lseek (struct file* file,
			    loff_t       off,
			    int          whence)
{
    loff_t new;

    switch (whence) {
	case 0:  new = off; break;
	case 1:  new = file->f_pos + off; break;
	case 2:
	default: return -EINVAL;
    }
    if (new > RDISK_ATAPI_REP_SZ) {
	return -EINVAL;
    }
    return (file->f_pos = new);
}

    static ssize_t
_vbd_atapi_data_proc_write (struct file* file,
			    const char*  buf,
			    size_t       size,
			    loff_t*      ppos)
{
    VbdVdisk*		vd = PDE(file->f_dentry->d_inode)->data;
    VbdBlkif*		bl = vd->blkif;
    atapi_t*    	atapi = vd->atapi;
    ssize_t      	res;
    NkPhAddr	 	paddr;
    void*        	vaddr;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    vbd_fast_map_t*	map;
#endif
    if (!atapi->count || (*ppos + size > RDISK_ATAPI_REP_SZ)) {
	return -EINVAL;
    }
    if (!size) {
	return 0;
    }

    res    = size;
    paddr  = atapi->req.paddr + 0x1000; /* ATAPI reply is on next page */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if ((map = vbd_fast_map_alloc(bl))) {
	char* src = (char*)buf;

	while (size) {
	    size_t csize = (~VBD_PAGE_64_MASK) + 1; /* page size */
	    _Bool  first = TRUE;

	    vbd_fast_map(map, paddr);	/* preemption is disabled */
	    ++bl->fastMapProbes;
	    vaddr = map->addr;
	    if (size < csize) {
		csize = size;
	    }
	    if (copy_from_user(vaddr, src, csize)) {
		res = -EFAULT;
		vbd_fast_unmap(map);	/* preemption is enabled */
		break;
	    }
	    if (first &&
		(atapi->req.cdb[0] == GET_EVENT_STATUS_NOTIFICATION)) {
	            /* Check if extent needs to be connected/disconnected */
		if (_vbd_atapi_extent_check_gesn(vd, vaddr)) {
		    res = -EAGAIN; /* media event needed */
		}
	    }
	    first  = FALSE;
	    vbd_fast_unmap(map);	/* preemption is enabled */
	    size  -= csize;
	    src   += csize;
	    paddr += csize;
	}
	vbd_fast_map_free(bl, map);
    } else {
#endif /* 2.6 */
	vaddr = nkops.nk_mem_map(paddr, size);
	++bl->nkMemMapProbes;
	if (!vaddr) {
	    ETRACE("nk_mem_map(0x%llx, 0x%llx) failure\n",
		   (long long) paddr, (long long) size);
	    return -EFAULT;
	}
	if (copy_from_user(vaddr, buf, size)) {
	    res = -EFAULT;
	} else if (atapi->req.cdb[0] == GET_EVENT_STATUS_NOTIFICATION) {
	        /* Check if extent needs to be connected/disconnected */
	    if (_vbd_atapi_extent_check_gesn(vd, vaddr)) {
		res = -EAGAIN; /* media event needed */
	    }
	}
	nkops.nk_mem_unmap(vaddr, paddr, size);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    }
#endif

    return res;
}

    static ssize_t
_vbd_atapi_data_proc_read (struct file* file,
			   char*        buf,
			   size_t       count,
			   loff_t*      ppos)
{
    return -EPERM;
}

    static _Bool
vbd_blkif_atapi (VbdBlkif* bl, const RDiskReqHeader* req)
{
    VbdVdisk*           vd;
    atapi_t*		atapi;
    RDiskAtapiReq*      areq;
    NkPhAddr		paddr;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    vbd_fast_map_t*	map;
#endif

    if (req->count != 1) {
	vbd_blkif_resp(bl, req->cookie, req->op, RDISK_STATUS_ERROR);
	ETRACE("%s: Invalid request count!\n", __FUNCTION__);
	return TRUE;
    }
    vd = vbd_blkif_vdisk_lookup(bl, req->devid);
    if (!vd) {
	ETRACE("%s: Invalid request devid!\n", __FUNCTION__);
	vbd_blkif_resp(bl, req->cookie, req->op, RDISK_STATUS_ERROR);
	return TRUE;
    }
    if (!(atapi = vd->atapi)) {
	ETRACE("%s: 0x%xd not an ATAPI device!\n", __FUNCTION__, req->devid);
	vbd_blkif_resp(bl, req->cookie, req->op, RDISK_STATUS_ERROR);
	return TRUE;
    }
        // Check if User process is present ...
    if (!atapi->ctrl_open || !atapi->data_open) {
	ETRACE("%s: User process not present!\n", __FUNCTION__);
	vbd_blkif_resp(bl, req->cookie, req->op, RDISK_STATUS_ERROR);
	return TRUE;
    }
        // Only one request at a time ...
    mutex_lock(&atapi->lock);
    if (atapi->count) {
	ETRACE("%s: too many requests!\n", __FUNCTION__);
	vbd_blkif_resp(bl, req->cookie, req->op, RDISK_STATUS_ERROR);
	mutex_unlock(&atapi->lock);
	return TRUE;
    }
    atapi->count = 1; // Get the process request
    mutex_unlock(&atapi->lock);

        // Map the front-end's ATAPI request
    paddr = *RDISK_FIRST_BUF(req);
    paddr = RDISK_BUF_BASE(paddr);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if ((map = vbd_fast_map_alloc(bl))) {
	unsigned int off = (paddr & ~VBD_PAGE_64_MASK);
	vbd_fast_map(map, paddr);
	    /* Now preemption is disabled */
	areq = (RDiskAtapiReq*)(((unsigned long)map->addr) + off);
	++bl->fastMapProbes;
    } else {
	areq = (RDiskAtapiReq*)nkops.nk_mem_map(paddr, sizeof(RDiskAtapiReq));
	++bl->nkMemMapProbes;
    }
#else	// 2.4
    areq = (RDiskAtapiReq*)nkops.nk_mem_map(paddr, sizeof(RDiskAtapiReq));
    ++bl->nkMemMapProbes;
#endif
    if (!areq) {
	ETRACE("nk_mem_map(0x%llx, 0x%llx) failure\n", (long long) paddr,
	       (long long) sizeof(RDiskAtapiReq));
	atapi->count = 0;  // Release the process request
	vbd_blkif_resp(bl, req->cookie, req->op, RDISK_STATUS_ERROR);
	return TRUE;
    }
        // Prepare the user process' request
    atapi->req.cookie = req->cookie;
    atapi->req.devid  = req->devid;
    atapi->req.op     = req->op;
    atapi->req.paddr  = paddr;
    memcpy(atapi->req.cdb, areq->cdb, RDISK_ATAPI_PKT_SZ);
    atapi->req.buflen = areq->buflen;
        // Unmap the front-end's ATAPI request
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if (map) {
	vbd_fast_unmap(map);
	vbd_fast_map_free(bl, map);
    } else
#endif
      nkops.nk_mem_unmap(areq, paddr, sizeof(RDiskAtapiReq));

        // Wake up the user process
    wake_up_interruptible(&atapi->wait);

    return FALSE; // No reply yet
}
#endif	/* VBD_ATAPI */

    static _Bool
vbd_blkif_probe (VbdBlkif*             bl,
		 vbd_probe_op          probe_op,
		 const RDiskReqHeader* req)
{
    NkPhAddr		paddr;
    NkPhSize		psize;
    void*		vaddr;
    RDiskStatus		status;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    vbd_fast_map_t*	map;
#endif

    if (req->count != 1) {
	vbd_blkif_resp(bl, req->cookie, req->op, RDISK_STATUS_ERROR);
	return TRUE;
    }
    paddr = *RDISK_FIRST_BUF(req);
    psize = RDISK_BUF_SIZE(paddr);
    paddr = RDISK_BUF_BASE(paddr);

    XTRACE("paddr=0x%llx(0x%llx)\n", (long long) paddr, (long long) psize);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if ((map = vbd_fast_map_alloc(bl))) {
	unsigned int off = (paddr & ~VBD_PAGE_64_MASK);
	vbd_fast_map(map, paddr);
	    /* Now preemption is disabled */
	vaddr = (void*)(((unsigned long)map->addr) + off);
	++bl->fastMapProbes;
    } else {
	vaddr = nkops.nk_mem_map(paddr, psize);
	++bl->nkMemMapProbes;
    }
#else	/* 2.4 */
    vaddr = nkops.nk_mem_map(paddr, psize);
    ++bl->nkMemMapProbes;
#endif
    if (!vaddr) {
	ETRACE("nk_mem_map(0x%llx, 0x%llx) failure\n", (long long) paddr,
	       (long long) psize);
	vbd_blkif_resp(bl, req->cookie, req->op, RDISK_STATUS_ERROR);
	return TRUE;
    }

        // Call request specific operation to fill in the RDiskProbe data
    status = probe_op(bl, req, (RDiskProbe*)vaddr, psize);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if (map) {
	vbd_fast_unmap(map);
	vbd_fast_map_free(bl, map);
    } else {
	nkops.nk_mem_unmap(vaddr, paddr, psize);
    }
#else	/* 2.4 */
    nkops.nk_mem_unmap(vaddr, paddr, psize);
#endif

    vbd_blkif_resp(bl, req->cookie, req->op, status);
    return TRUE;
}


    // Can set be->resource_error.
static _Bool vbd_vdisk_rw (VbdVdisk* vd, const RDiskReqHeader* req);

    // Called only by vbd_do_block_io_op().
    // Return value indicates if a response has been prepared
    // and hence a cross-interrupt should be sent.
    // Can set be->resource_error.

    static _Bool
vbd_blkif_rw (VbdBlkif* bl, const RDiskReqHeader* req)
{
    VbdVdisk* vd = vbd_blkif_vdisk_lookup(bl, req->devid);

    if (!vd) {
	ETRACE("(%d,%d,%d) virtual disk not found\n", bl->ring->pid,
	       VBD_DISK_MAJOR(req->devid), VBD_DISK_MINOR(req->devid));
	vbd_blkif_resp(bl, req->cookie, req->op, RDISK_STATUS_ERROR);
	return TRUE;
    }
    return vbd_vdisk_rw(vd, req);
}

/*
 * Virtual disk
 */

    // Called from vbd_vdisk_create() only, called from vbd_module_init() only.

    static VbdVdisk* __init
vbd_vdisk_alloc (void)
{
    void* vd;
    if (unlikely((vd = kzalloc(sizeof(VbdVdisk), GFP_KERNEL)) == NULL)) {
	ETRACE("vbd_vdisk_alloc: out of memory\n");
	return NULL;
    }
    return VBD_VDISK(vd);
}

    // Called from vbd_module_init() only.

#ifdef VBD_ATAPI
static _Bool __init vbd_atapi_init (VbdVdisk* vd);
#endif

    static void __init
vbd_vdisk_create (VbdBe* be, const VbdPropDiskVdisk* prop)
{
    VbdBlkif*  bl;
    VbdVdisk*  vd;
    VbdVdisk** link;

    bl = vbd_blkif_create(be, prop, FALSE /*!nk_dev_add*/);
    if (!bl) {
	ETRACE("block interface allocation failure\n");
	return;
    }
    VBD_BLKIF_FOR_ALL_VDISKS(vd, bl) {
	if (vd->id == prop->id) {
		/* Take the strongest wait mode among vdisk= parameters */
	    if (prop->wait > vd->wait) {
		vd->wait = prop->wait;
	    }
	    return;		/* vdisk already exists */
	}
    }
    vd = vbd_vdisk_alloc();
    if (!vd) {
	ETRACE("virtual disk allocation failure\n");
	return;
    }
    vd->tag    = prop->tag;
    vd->id     = prop->id;
    vd->wait   = prop->wait;
    vd->blkif  = bl;
    vd->next   = NULL;
    snprintf (vd->name, sizeof vd->name, "(%d,%d,%d)", bl->ring->pid,
	      VBD_DISK_MAJOR(vd->id), VBD_DISK_MINOR(vd->id));

#ifdef VBD_ATAPI
    if (!vbd_atapi_init(vd)) {
	vbd_vdisk_free(vd);
	ETRACE("virtual disk %s - ATAPI initialization failure!\n", vd->name);
	return;
    }
#endif

    link = &bl->vdisks;
    while (*link) link = &(*link)->next;

    *link = vd;

    TRACE ("virtual disk %s created", vd->name);
#ifdef VBD_ATAPI
    if (vd->atapi) {
	printk (" - ATAPI ctrl: %s data: %s",
	        vd->atapi->ctrl_name, vd->atapi->data_name);
    }
#endif
    printk ("\n");
}

    /*
     *  On 2.6.x we can have zero-sized peripherals,
     *  typically initially empty loopback devices. In
     *  this case, we try to acquire the size at every
     *  vbd_vdisk_probe() and vbd_vdisk_translate(),
     *  so that the guest is informed about the most
     *  current value and in order not to refuse an
     *  I/O request which is actually valid now.
     */

    static inline void
vbd_extent_maybe_acquire_size (VbdExtent* ex)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if (!ex->size) {
	if (ex->bdev->bd_part) {
	    ex->size = ex->bdev->bd_part->nr_sects;
	} else {
	    ex->size = get_capacity(ex->bdev->bd_disk);
	}
    }
#else
    (void) ex;
#endif
}

    static VbdDiskSize
vbd_vdisk_size (const VbdVdisk* vd)
{
    VbdDiskSize  size = 0;
    VbdExtent* ex;

    VBD_VDISK_FOR_ALL_EXTENTS(ex, vd) {
	vbd_extent_maybe_acquire_size(ex);
	size += ex->size;
    }
    return size;
}

    static _Bool
vbd_vdisk_readonly (const VbdVdisk* vd)
{
    const VbdExtent* ex;

    VBD_VDISK_FOR_ALL_EXTENTS(ex, vd) {
	if (ex->access & VBD_DISK_ACC_W) {
	    return FALSE;
	}
    }
    return TRUE;
}

    static void
vbd_vdisk_probe (const VbdVdisk* vd, RDiskProbe* probe)
{
    VbdPropDiskExtent* prop = vbd_prop_disk_extent_lookup(vd->tag);

    probe->size[0] = vbd_vdisk_size(vd);
    probe->size[1] = 0;
    probe->devid   = vd->id;
    if (prop && (prop->major == FLOPPY_MAJOR)) {
	    /* vdisk is a floppy */
	probe->info = RDISK_TYPE_FLOPPY;
    } else {
	    /* vdisk is a disk */
	probe->info = RDISK_TYPE_DISK;
    }
    if (prop && (prop->major == LOOP_MAJOR)) {
	    /* vdisk is connected to a loop device */
	probe->info |= RDISK_FLAG_VIRT;
    }
    if (vbd_vdisk_readonly(vd)) {
	probe->info |= RDISK_FLAG_RO;
    }
    XTRACE("%s, %d sectors, flags 0x%x\n", vd->name,
	   probe->size[0], probe->info);
}

    static VbdExtent*
vbd_vdisk_translate (VbdVdisk* vd, nku32_f acc, RDiskSector vsec,
		     VbdDiskSize* psec)
{
    VbdExtent* ex;

    VBD_VDISK_FOR_ALL_EXTENTS(ex, vd) {
	vbd_extent_maybe_acquire_size(ex);
	if (vsec < ex->size) {
	    break;
	}
	vsec -= ex->size;
    }
    if (!ex) {
	DTRACE("ex not found. extents %p\n", vd->extents);
        return NULL;
    }
    if (!(ex->access & acc)) {
        return NULL;
    }
    *psec = (ex->start + vsec);
    return ex;
}

/*
 * Extent management
 */

    static inline void
vbd_extent_free (VbdExtent* ex)
{
    kfree(ex);
}

    // Called from vbd_extent_create() only.

    static VbdExtent*
vbd_extent_alloc (void)
{
    return VBD_EXTENT (kzalloc (sizeof (VbdExtent), GFP_KERNEL));
}

    // Called from vbd_extent_create() only.

    static _Bool
vbd_extent_connect (VbdExtent* ex)
{
    nku32_f sectors;
        /*
	 * Enforce the extent to start at the beginning of the block device
	 */
    ex->start = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    ex->bdev = blkdev_get_by_dev(ex->dev,
				 ((ex->access ==  VBD_DISK_ACC_R) ? FMODE_READ
				  : FMODE_WRITE),
				 ex);
    if (IS_ERR(ex->bdev)) {
        DTRACE("device (%d,%d) doesn't exist.\n",
	       ex->prop->major, ex->prop->minor);
	return FALSE;
    }
    if (ex->bdev->bd_disk == NULL) {
        DTRACE("device (%d,%d) doesn't exist 2.\n",
		ex->prop->major, ex->prop->minor);
	BDEV_CLOSE(ex->bdev, ((ex->access ==  VBD_DISK_ACC_R) ? FMODE_READ
			                                      : FMODE_WRITE));
	return FALSE;
    }

        /* get block device size in sectors (512 bytes) */
    if (ex->bdev->bd_part) {
	sectors = ex->bdev->bd_part->nr_sects;
    } else {
	sectors = get_capacity(ex->bdev->bd_disk);
    }
    if (!sectors && ex->vdisk->wait == VBD_VDISK_WAIT_NON_ZERO) {
        DTRACE("device (%d,%d) size is 0.\n",
		ex->prop->major, ex->prop->minor);
	BDEV_CLOSE(ex->bdev, ((ex->access ==  VBD_DISK_ACC_R) ? FMODE_READ
			                                      : FMODE_WRITE));
	return FALSE;
    }
        /*
	 * Work-around strange kernel behavior where the 
	 * block device bd_inode->i_size is not updated when inserting
	 * a media (maybe due to using: open_by_devnum()?).
	 * This leads to common block device code to fail in 
	 * checking submitted bio(s) accessing sectors beyond 0x1fffff,
	 * on /dev/sr0 (sr initializes bd_inode->i_size with that value).
	 */
    if (sectors != (i_size_read(ex->bdev->bd_inode) >> 9)) {
	    /* Update bdev inode size to make blk-core.c happy */
	DTRACE("device (%d,%d): hacking bdev inode size (%llu -> %llu).\n",
	       ex->prop->major, ex->prop->minor,
	       i_size_read(ex->bdev->bd_inode), ((loff_t)sectors) << 9);
	i_size_write(ex->bdev->bd_inode, ((loff_t)sectors) << 9);
    }
#else
    if (!blk_size[MAJOR(ex->dev)]) {
        DTRACE("device (%d,%d) doesn't exist.\n",
		ex->prop->major, ex->prop->minor);
	return FALSE;
    }
    /* convert blocks (1KB) to sectors */
    sectors = blk_size[MAJOR(ex->dev)][MINOR(ex->dev)] * 2;

    if (sectors == 0) {
        DTRACE("device (%d,%d) doesn't exist.\n",
		ex->prop->major, ex->prop->minor);
	return FALSE;
    }
#endif

    DTRACE("extent size requested %llu actual %u sectors, access %i\n",
	   (unsigned long long)ex->size, sectors, ex->access);
        /*
	 * NB. This test assumes ex->start == 0
	 */
    if (!ex->size || (ex->size > sectors)) {
        ex->size = sectors;
    }
    return TRUE;
}

    // Called from vbd_extent_create() only, called from
    // vbd_extent_thread() only.

    static VbdVdisk*
vbd_lookup_vd_by_prop (VbdBe* be, const VbdPropDiskExtent* prop)
{
    VbdBlkif* bl;

    VBD_BE_FOR_ALL_BLKIFS(bl, be) {
	VbdVdisk* vd;

	VBD_BLKIF_FOR_ALL_VDISKS(vd, bl) {
            if (vd->tag == prop->tag) {
                return vd;
            }
        }
    }
    return NULL;
}

    //
    // We consider that a vdisk is composed of only one extent.
    // Return TRUE if ok, FALSE on error.
    // Called from vbd_extent_create().
    //    and from vbd_op_media_probe().
    //
    static _Bool
vbd_vdisk_extent_create (VbdVdisk* vd, VbdPropDiskExtent* prop)
{
    VbdExtent* ex;

    if (!prop || !vd) {
	return FALSE;
    }
    ex = vbd_extent_alloc();
    if (!ex) {
        ETRACE("extent allocation failure\n");
	vd->extents_ok = FALSE;
        return FALSE;
    }
    ex->prop   = prop;
    ex->vdisk  = vd;
    ex->size   = prop->size;
    ex->access = prop->access;
    ex->dev    = MKDEV(prop->major, prop->minor);

    if (vbd_extent_connect(ex)) {
	VbdExtent** last = &vd->extents;

        while (*last) last = &(*last)->next;
        *last = ex;
        prop->bound = TRUE;
	DTRACE("Found %s, %lld sectors.\n", vd->name, (long long) ex->size);
    } else {
	vbd_extent_free(ex);
        DTRACE("connect KO for OS#%d\n", vd->blkif->ring->pid);
	vd->extents_ok = FALSE;
        return FALSE;
    }
    return TRUE;
}

    // Called from vbd_extent_thread() only.
    static _Bool
vbd_extent_create (VbdBe* be, VbdPropDiskExtent* prop)
{
    VbdVdisk* vd;

    vd = vbd_lookup_vd_by_prop(be, prop);
    if (!vd) {
        return FALSE;
    }
    return vbd_vdisk_extent_create(vd, prop);
}

    // Called from vbd_extent_thread() only.
    // Return TRUE if we have all extents/vdisks now.

    static _Bool
vbd_be_maybe_nk_dev_add (VbdBe* be)
{
    _Bool	blkifs_ok = TRUE;
    VbdBlkif*	bl;

    VBD_BE_FOR_ALL_BLKIFS(bl, be) {
	_Bool		vdisks_ok = TRUE;
	VbdVdisk*	vd;

	if (bl->nk_dev_added) continue;
	VBD_BLKIF_FOR_ALL_VDISKS(vd, bl) {
	    if (!vd->extents_ok && vd->wait != VBD_VDISK_WAIT_NO) {
		vdisks_ok = FALSE;
		break;
	    }
	}
	if (vdisks_ok) {
	    DTRACE("adding device for pid %i\n", bl->ring->pid);
	    nkops.nk_dev_add(bl->pdev);
	    bl->nk_dev_added = TRUE;
	    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, bl->ring->pid);
	} else {
	    blkifs_ok = FALSE;
	}
    }
    return blkifs_ok;
}

    // Called from vbd_blkif_destroy()
    //    <- from vbd_destroy() <- vbd_module_exit().
    // Also called from vbd_op_media_probe().

    static void
vbd_extents_disconnect (VbdVdisk* vd)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    VbdExtent* ex = vd->extents;

    while (ex) {
	VbdExtent* prev;
        BDEV_CLOSE(ex->bdev, FMODE_READ|FMODE_WRITE);
	prev = ex;
	ex   = ex->next;
	vbd_extent_free(prev);
    }
    vd->extents = 0;
#endif
}

    /*
     *  The extents list for a VbdVdisk only contains already bound
     *  extents, so scanning it does not allow to determine if
     *  a vdisk is now complete. So we have "missing extents" notion
     *  for VbdVdisks which is updated as the vbdextent[] is scanned.
     *  Before scanning, all vdisks are temporarily set to "extents_ok"
     *  state, which is removed if some extent is missing.
     */
    static void
vbd_be_mark_vdisks (VbdBe* be)
{
    VbdBlkif* bl;

    VBD_BE_FOR_ALL_BLKIFS(bl, be) {
	VbdVdisk* vd;

	if (bl->nk_dev_added) continue;
	VBD_BLKIF_FOR_ALL_VDISKS(vd, bl) {
	    vd->extents_ok = TRUE;
	}
    }
}

static int vbd_extent_abort;
static DECLARE_COMPLETION(vbd_th_extent_completion);
#define VBD_EXTENT_THREAD	"vbd-be-extent"

    static int
vbd_extent_thread (void* arg)
{
    VbdBe* be = arg;

	/*
	 *  This thread is always started with kernel_thread(),
	 *  so we always need to daemonize and set a name.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    daemonize(VBD_EXTENT_THREAD);
#else
    daemonize();
    snprintf(current->comm, sizeof current->comm, VBD_EXTENT_THREAD);
#endif

	/* Analyze virtual disk extents */
    while (!vbd_extent_abort) {
	VbdPropDiskExtent* vde;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
	try_to_freeze();
#endif

	vbd_be_mark_vdisks(be);
	for (vde = vbdextent; vde->major; ++vde) {
            if (vde->bound) continue;
	    vbd_extent_create(be, vde);
	}
	if (vbd_be_maybe_nk_dev_add(be)) {
	    DTRACE("all extents bound, suiciding.\n");
	    break;
	}
	    /*
	     *  When waiting for an extent to appear, we could
	     *  go to sleep, waiting for the vlx_vbd_disk_created()
	     *  callback which is performed from add_disk(). But
	     *  we also sometimes wait until a disk size is non-zero,
	     *  which requires periodic polling, so we would need
	     *  to setup a sleep with timeout, and this requires
	     *  more work than it is worth. So we just poll every
	     *  half second until we have all the devices in OK state.
	     */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(HZ/2);
    }
    complete_and_exit(&vbd_th_extent_completion, 0);
    /*NOTREACHED*/
    return 0;
}

    void
vlx_vbd_disk_created (void)
{
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
EXPORT_SYMBOL(vlx_vbd_disk_created);
#endif

/******************************************************************
 * BLOCK-DEVICE SCHEDULER LIST MAINTENANCE
 */

static struct list_head vbd_blkio_schedule_list;
static spinlock_t       vbd_blkio_schedule_list_lock;

    static inline _Bool
vbd_on_blkdev_list (VbdBlkif* bl)
{
    return bl->blkdev_list.next != NULL;
}

    // Called from vbd_blkio_schedule() only.

    static void
vbd_remove_from_blkdev_list (VbdBlkif* bl)
{
    unsigned long flags;
    if (!vbd_on_blkdev_list(bl)) {
	return;
    }
    spin_lock_irqsave(&vbd_blkio_schedule_list_lock, flags);
    if (vbd_on_blkdev_list(bl)) {
        list_del(&bl->blkdev_list);
        bl->blkdev_list.next = NULL;
	vbd_blkif_put(bl);
    }
    spin_unlock_irqrestore(&vbd_blkio_schedule_list_lock, flags);
}

    // called when a block interface requires attention

    static void
vbd_add_to_blkdev_list_tail (VbdBlkif* bl)
{
    unsigned long flags;
    if (vbd_on_blkdev_list(bl)) {
	    // already enlisted on vbd_blkio_schedule_list
	return;
    }
    spin_lock_irqsave(&vbd_blkio_schedule_list_lock, flags);
    if (!vbd_on_blkdev_list(bl))  {
	list_add_tail(&bl->blkdev_list, &vbd_blkio_schedule_list);
	    // increment refcnt
	vbd_blkif_get(bl);
    }
    spin_unlock_irqrestore(&vbd_blkio_schedule_list_lock, flags);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    static vbd_pending_req_t*
vbd_get_from_done_list (VbdBlkif* bl)
{
    unsigned long  flags;
    vbd_pending_req_t* req;
    spin_lock_irqsave(&bl->done_lock, flags);
    if (list_empty(&bl->done_list)) {
	req = 0;
    } else {
	struct list_head* head = bl->done_list.next;
	    // convert pointer to field into pointer to struct
	req = list_entry(head, vbd_pending_req_t, list);
        list_del(head);
    }
    spin_unlock_irqrestore(&bl->done_lock, flags);
    return req;
}
#endif

    static void
vbd_maybe_trigger_blkio_schedule (void)
{
    /*
     * Needed so that two processes, who together make the following predicate
     * true, don't both read stale values and evaluate the predicate
     * incorrectly. Incredibly unlikely to stall the scheduler on x86, but...
     */
    smp_mb();
    if (!list_empty(&vbd_blkio_schedule_list)) {
	    // awake the thread
	wake_up(&vbd_blkio_schedule_wait);
    }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    static void
vbd_put_to_done_list (VbdBlkif* bl, vbd_pending_req_t* req)
{
    unsigned long flags;
    int           empty;
    spin_lock_irqsave(&bl->done_lock, flags);
    empty = list_empty(&bl->done_list);
    list_add_tail(&req->list, &bl->done_list);
    spin_unlock_irqrestore(&bl->done_lock, flags);
    if (empty) {
	vbd_add_to_blkdev_list_tail(bl);
	vbd_maybe_trigger_blkio_schedule();
    }
}
#endif

/******************************************************************
 * SCHEDULER FUNCTIONS
 */
static int  vbd_do_block_io_op (VbdBlkif* bl, int max_to_do);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)
static DECLARE_COMPLETION(vbd_th_completion);
#endif

#define VBD_BLKIO_SCHEDULE_THREAD	"vbd-be"

static unsigned vbd_timeouts;

    static void
vbd_timeout (unsigned long data)
{
    ++vbd_timeouts;
    wake_up_process ((void*) data);
}

    void
vbd_schedule_with_timeout (long timeout)
{
    struct timer_list timer;

    init_timer(&timer);
    timer.expires  = jiffies + timeout;
    timer.data     = (unsigned long) current;
    timer.function = vbd_timeout;

    add_timer(&timer);
    schedule();
    del_timer_sync(&timer);
}

static unsigned long long	vbd_sleeps_no_timeout;
static unsigned			vbd_sleeps_with_timeout;

    // This is a kernel thread.

    static int
vbd_blkio_schedule (void* arg)
{
    DECLARE_WAITQUEUE(wq, current);
    VbdBlkif*		bl;
    struct list_head*	ent;
    VbdBe*		be = arg;

	/*
	 *  On 2.6.7 and above this thread is started with kthread_run(),
	 *  so we do not need to daemonize or set a name.
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    daemonize(VBD_BLKIO_SCHEDULE_THREAD);
#else
    daemonize();
    snprintf(current->comm, sizeof current->comm, VBD_BLKIO_SCHEDULE_THREAD);
#endif
#endif

    while (atomic_read(&be->refcnt)) {
        /* Wait for work to do. */

	add_wait_queue(&vbd_blkio_schedule_wait, &wq);
        set_current_state(TASK_INTERRUPTIBLE);

	if (be->resource_error) {
	    be->resource_error = FALSE;
	    ++vbd_sleeps_with_timeout;
	    vbd_schedule_with_timeout(HZ/10);
	} else
	if (atomic_read(&be->refcnt) && list_empty(&vbd_blkio_schedule_list)) {
	    ++vbd_sleeps_no_timeout;
            schedule();
	}
        __set_current_state(TASK_RUNNING);
	remove_wait_queue(&vbd_blkio_schedule_wait, &wq);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
        // check if freeze signal has been received
	try_to_freeze();
#endif
        /* Queue up a batch of requests. */

	while (!list_empty(&vbd_blkio_schedule_list) && !be->resource_error) {
	    ent = vbd_blkio_schedule_list.next;
		// convert pointer to field into pointer to struct
	    bl = list_entry(ent, VbdBlkif, blkdev_list);
	    vbd_blkif_get(bl);
	    vbd_remove_from_blkdev_list(bl);
		/*
		 *  In case of be->resource_error, vbd_do_block_io_op()
		 *  returns 0 even though "bl" still requires
		 *  attention, so check for that specifically.
		 */
	    if (vbd_do_block_io_op(bl, VBD_MAX_REQ_PER_BLKIF) ||
		be->resource_error) {
		    /*
		     *  We have processed all the VBD_MAX_REQ_PER_BLKIF
		     *  quota of requests, so there can be more, so we
		     *  put back "bl" on the list, at the end/tail.
		     */
		vbd_add_to_blkdev_list_tail(bl);
	    }
	    vbd_blkif_put(bl);
        }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
        /* Push the batch through to disc. */
        run_task_queue(&tq_disk);
#endif
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
	/*
	 *  The stop order has been issued from vbd_module_exit() by calling
	 *  vbd_stop() before kthread_stop() has been called. If we
	 *  exited from this thread here immediately, task_struct
	 *  vbd_th_desc would become deallocated before vbd_module_exit()
	 *  has used it to call kthread_stop(). Therefore, we must
	 *  await this call here.
	 */
    while (!kthread_should_stop()) {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1);
    }
#else
    complete_and_exit(&vbd_th_completion, 0);
    /*NOTREACHED*/
#endif

    return 0;
}

/******************************************************************
 * COMPLETION CALLBACK -- Called as bh->b_end_io()
 */

    static void
vbd_block_io_done (vbd_pending_req_t *req)
{
    VbdBlkif*  bl   = req->blkif;
#if 0
    NkDevRing* ring = bl->ring;

    vbd_blkif_resp(bl, req->cookie, req->op,
		   req->error ? RDISK_STATUS_ERROR : RDISK_STATUS_OK);
    nkops.nk_xirq_trigger(ring->pxirq, ring->pid);
    ++bl->xirqTriggers;
#else
    vbd_blkif_resp_trigger(bl, req->cookie, req->op,
			   req->error ? RDISK_STATUS_ERROR : RDISK_STATUS_OK);
#endif
    vbd_blkif_put(bl);
    kmem_cache_free(vbd_pending_req_cachep, req);
    vbd_maybe_trigger_blkio_schedule();
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

    // Called from vbd_bh_end_block_io_op()

    static void
vbd_req_end_block_io_op (vbd_pending_req_t* xreq, int uptodate)
{
    if (!uptodate) {
        xreq->error = TRUE;
    }
    if (!atomic_sub_return(1, &xreq->pendcnt)) {
	vbd_block_io_done(xreq);
    }
}

    /*
     *  This is a callback set up in vbd_vdisk_rw(),
     *  through the "bh->b_end_io" field.
     *  (In 2.6, we have "bio->bi_end_io")
     */

    static void
vbd_bh_end_block_io_op (struct buffer_head *bh, int uptodate)
{
    vbd_req_end_block_io_op(bh->b_private, uptodate);
    nkops.nk_mem_unmap(bh->b_data, __pa(bh->b_data), bh->b_size);
    kmem_cache_free(vbd_buffer_head_cachep, bh);
}

#else	/* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) // FIXME
/*
 * Since 2.6.25 blk_get_queue and blk_put_queue are not exported anymore. It
 * seems the VBD code should be changed to get rid of these functions.
 */
    static inline int
vbd_blk_get_queue (struct request_queue* q)
{
    if (likely(!test_bit(QUEUE_FLAG_DEAD, &q->queue_flags))) {
	kobject_get(&q->kobj);
	return 0;
    }
    return 1;
}

    static inline void
vbd_blk_put_queue (struct request_queue* q)
{
    kobject_put(&q->kobj);
}

#define blk_get_queue vbd_blk_get_queue
#define blk_put_queue vbd_blk_put_queue
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) */

    static void
vbd_submit_bio (vbd_pending_req_t* req, struct bio* bio)
{
    struct request_queue* q;

	/*
	 *  If __get_free_page() fails in vdisk_dma=0 mode,
	 *  we can be called with bio->bi_size=0. We must
	 *  not call submit_bio() then, because the routine
	 *  will panic.
	 *  xreq->error should be TRUE already then.
	 */
    if (!bio->bi_size) {
	bio_put(bio);
	return;
    }
    q = bdev_get_queue(bio->bi_bdev);
    if (q) {
	blk_get_queue(q);
	atomic_inc(&req->pendcnt);
	submit_bio(bio->bi_rw, bio);
	blk_put_queue(q);
    } else {
	req->error = TRUE;
    }
}

    // Called only from vbd_req_end_block_io_op()

    static void
vbd_end_block_io_op_read_fast (vbd_pending_req_t* req, vbd_fast_map_t* map)
{
    vbd_pending_seg_t* seg   = req->segs;
    vbd_pending_seg_t* limit = req->segs + req->nsegs;
    while (seg != limit) {
	if (seg->vaddr) {
	    unsigned int  off = (seg->gaddr & ~VBD_PAGE_64_MASK);
	    unsigned long dst = ((unsigned long)map->addr) + off;
	    unsigned long src = seg->vaddr + off;

	    vbd_fast_map(map, seg->gaddr & VBD_PAGE_64_MASK);
	    memcpy((void*)dst, (void*)src, seg->size);
	    vbd_fast_unmap(map);
	    free_page(seg->vaddr);
		/* Collect statistics */
	    ++req->blkif->fastMapReads;
	    req->blkif->fastMapReadBytes += seg->size;
	    ++req->blkif->pageFreeings;
	    --req->blkif->allocedPages;
	}
	seg++;
    }
    vbd_block_io_done(req);
}

    static void
vbd_req_end_block_io_op (vbd_pending_req_t* req)
{
    if (!atomic_sub_return(1, &req->pendcnt)) {
	if (req->op == RDISK_OP_READ || req->op == RDISK_OP_READ_EXT) {
	    VbdBlkif*       bl  = req->blkif;
	    vbd_fast_map_t* map = vbd_fast_map_alloc(bl);

	    if (map) {
		vbd_end_block_io_op_read_fast(req, map);
		vbd_fast_map_free(bl, map);
	    } else {
		vbd_put_to_done_list(req->blkif, req);
	    }
	} else {
	    vbd_pending_seg_t* seg   = req->segs;
	    vbd_pending_seg_t* limit = req->segs + req->nsegs;
	    while (seg != limit) {
		if (seg->vaddr) {
		    free_page(seg->vaddr);
		    ++req->blkif->pageFreeings;
		    --req->blkif->allocedPages;
		}
		seg++;
	    }
	    vbd_block_io_done(req);
	}
    }
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
    static int
vbd_bio_end_block_io_op (struct bio *bio, unsigned int done, int error)
{
    vbd_pending_req_t* req = bio->bi_private;
    if (!done || error) {
	req->error = TRUE;
    }
    vbd_req_end_block_io_op(req);
    bio_put(bio);
    return error;
}
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24) */
    static void
vbd_bio_end_block_io_op (struct bio *bio, int error)
{
    vbd_pending_req_t* req = bio->bi_private;
    if (error) {
	req->error = TRUE;
    }
    vbd_req_end_block_io_op(req);
    bio_put(bio);
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24) */
#endif	/* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) */

    // Called only by vbd_blkif_rw() <- vbd_do_block_io_op() <-
    // vbd_blkio_schedule()
    // Return value indicates if a response has been prepared
    // and hence a cross-interrupt should be sent.

    static _Bool
vbd_vdisk_rw (VbdVdisk* vd, const RDiskReqHeader* req)
{
    const int	   isRead = req->op == RDISK_OP_READ ||
			    req->op == RDISK_OP_READ_EXT;
    const int	   isExt  = req->op == RDISK_OP_WRITE_EXT ||
			    req->op == RDISK_OP_READ_EXT;
    VbdBlkif*       bl     = vd->blkif;
    const nku32_f  acc    = isRead ? VBD_DISK_ACC_R : VBD_DISK_ACC_W;
    vbd_pending_req_t* xreq;
    VbdExtent*	   ex;
    unsigned int   seg;
    unsigned int   count;

    XTRACE("%s req %d sector %d count %d\n", vd->name, req->op,
	   req->sector[0], req->count);
    ++bl->requests;

    if (unlikely(req->sector[1])) {
	ETRACE("%s sector number too large\n", vd->name);
	vbd_blkif_resp(bl, req->cookie, req->op, RDISK_STATUS_ERROR);
	return TRUE;
    }
    count = req->count;
    if (!count) {
	count = 256;
    }
    if (bl->pending_xreq) {
	xreq = bl->pending_xreq;
	bl->pending_xreq = NULL;
    } else {
	xreq = kmem_cache_alloc(vbd_pending_req_cachep, GFP_ATOMIC);

	if (unlikely(!xreq)) {
	    ETRACE("%s sector %d, out of descriptors; will retry\n",
		   vd->name, req->sector[0]);
	    bl->be->resource_error = TRUE;
	    ++bl->resourceErrors;
	    return FALSE;
	}
	xreq->blkif   = bl;
	xreq->cookie  = req->cookie;
	xreq->op      = req->op;
	xreq->error   = FALSE;
	atomic_set (&xreq->pendcnt, 1);
	xreq->nsegs   = 0;
	xreq->vsector = req->sector[0];
    }
    vbd_blkif_get(bl);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    for (seg = 0; seg < count; seg++) {
	NkPhAddr	paddr;
	nku32_f		psize;
	nku32_f		ssize;
        struct buffer_head* bh;
	void*		vaddr;
	VbdDiskSize	psector = 0;
	int		operation = (acc == VBD_DISK_ACC_W ? WRITE : READ);

	if (isExt) {
	    ETRACE("%s op %d not supported\n", vd->name, xreq->op);
	    xreq->error = TRUE;
	    break;
	}
	paddr = *(RDISK_FIRST_BUF(req) + seg);

	XTRACE(" 0x%llx:%lld:%lld\n",
	       RDISK_BUF_PAGE(paddr),
	       RDISK_BUF_SSECT(paddr),
	       RDISK_BUF_ESECT(paddr));

	ssize = RDISK_BUF_SECTS(paddr);
	psize = RDISK_BUF_SIZE(paddr);
	paddr = RDISK_BUF_BASE(paddr);

	++bl->segments;

	ex = vbd_vdisk_translate(vd, acc, xreq->vsector, &psector);
	if (unlikely(!ex)) {
	    ETRACE("%s sector %d translation failure\n", vd->name,
	    	   xreq->vsector);
	    xreq->error = TRUE;
	    break;
	}
	bh = kmem_cache_alloc(vbd_buffer_head_cachep, GFP_ATOMIC);
	if (unlikely(!bh)) {
	    ETRACE("%s buffer header allocation failure\n", vd->name);
	    xreq->error = TRUE;
	    break;
	}
	vaddr = nkops.nk_mem_map(paddr, psize);
	if (!vaddr) {
	    kmem_cache_free(vbd_buffer_head_cachep, bh);
	    ETRACE("%s nk_mem_map(0x%llx, 0x%x) failure\n", vd->name,
	    	   (long long) paddr, psize);
	    xreq->error = TRUE;
	    break;
	}
	memset(bh, 0, sizeof(struct buffer_head));

        init_waitqueue_head(&bh->b_wait);
        bh->b_size    = psize;
        bh->b_dev     = ex->dev;
        bh->b_rdev    = ex->dev;
        bh->b_rsector = (unsigned long)psector;
        bh->b_data    = (char *)vaddr;
        bh->b_page    = virt_to_page(vaddr);
	bh->b_end_io  = vbd_bh_end_block_io_op;
        bh->b_private = xreq;
        bh->b_state   = ((1 << BH_Mapped) | (1 << BH_Lock) |
			 (1 << BH_Req)    | (1 << BH_Launder));
        if (operation == WRITE)
            bh->b_state |= (1 << BH_JBD) | (1 << BH_Req) | (1 << BH_Uptodate);

        atomic_set(&bh->b_count, 1);
	atomic_inc(&xreq->pendcnt);

        /* Dispatch a single request. We'll flush it to disc later. */
	    // This is a linux kernel function.
        generic_make_request(operation, bh);

	xreq->vsector += ssize;
    }
    vbd_req_end_block_io_op(xreq, !xreq->error);
#else
    {
	int		oneBio      = 0;
	struct bio*	bio         = 0;
	VbdDiskSize	nsector     = 0;
	vbd_fast_map_t*	map         = isRead ? 0 : vbd_fast_map_alloc(bl);
	unsigned int	bi_max_vecs = 0;
	_Bool		finish_later = FALSE;

	for (seg = xreq->nsegs; seg < count; seg++) {
	    NkPhAddr		paddr;
	    nku32_f		psize;
	    nku32_f		ssize;
	    struct bio_vec*	bv;
	    VbdDiskSize		psector = 0;
	    vbd_pending_seg_t*	pseg;
	    int			copyMode;

	    paddr = *(RDISK_FIRST_BUF(req) + seg);

	    if (isExt) {
		XTRACE(" 0x%llx:%lld\n",
		       (long long) RDISK_BUF_BASE_EXT(paddr),
		       (long long) RDISK_BUF_SIZE_EXT(paddr));
	    } else {
		XTRACE(" 0x%llx:%lld:%lld\n",
		       (long long) RDISK_BUF_PAGE(paddr),
		       (long long) RDISK_BUF_SSECT(paddr),
		       (long long) RDISK_BUF_ESECT(paddr));
	    }
	    if (isExt) {
		psize = RDISK_BUF_SIZE_EXT(paddr);
		paddr = RDISK_BUF_BASE_EXT(paddr);
		if (psize & (RDISK_SECT_SIZE - 1)) {
		    ssize  = 0;
		    oneBio = 1;
		} else {
		    ssize = psize >> RDISK_SECT_SIZE_BITS;
		}
	    } else {
		ssize = RDISK_BUF_SECTS(paddr);
		psize = RDISK_BUF_SIZE(paddr);
		paddr = RDISK_BUF_BASE(paddr);
	    }
	    ++bl->segments;
	    if (!vdisk_dma) {
		copyMode = 1;
	    } else {
		    /* Check if we are allowed to use pfn_to_page() below */
		if (!pfn_valid (paddr >> PAGE_SHIFT)) {
		    DTRACE("%s op %d vsector %d physical address "
			    "%llx translation failure.\n", vd->name,
			    req->op, xreq->vsector,
			    (unsigned long long) paddr);
		    ++bl->noStructPage;
		    copyMode = 1;
		} else {
#ifdef CONFIG_X86
			/*
			 *  This check is only necessary on x86/VT
			 *  and on ARM the function is not implemented.
			 */
		    NkMhAddr maddr = nkops.nk_machine_addr(paddr);
		    if (maddr < 0x100000 && maddr != paddr) {
			DTRACE("COPY mode forced (mach=0x%llx phys=0x%llx)\n",
			       maddr, paddr);
			++bl->firstMegCopy;
			copyMode = 1;
		    } else
#endif
		    {
			copyMode = 0;
		    }
		}
	    }
#if defined CONFIG_X86 && !defined CONFIG_X86_PAE && !defined CONFIG_X86_64
		/*
		 *  Fix for BugId 4676708 ("VBD-BE crashes if passed a
		 *  physical address above 4GB when not compiled for PAE
		 *  or 64 bits"). Catch it here, rather than waiting
		 *  till the vbd_fast_map() call.
		 */
	    if (paddr > (nku32_f) -1) {
		ETRACE("%s op %d vsector %d physical address "
			"%llx is not reachable.\n", vd->name,
			req->op, xreq->vsector, paddr);
		xreq->error = TRUE;
		break;
	    }
#endif
	    ex = vbd_vdisk_translate(vd, acc, xreq->vsector, &psector);
	    if (unlikely(!ex)) {
		ETRACE("%s sector %d translation failure on %s\n", vd->name,
			xreq->vsector, isRead ? "read" : "write");
	        xreq->error = TRUE;
	        break;
	    }
	    if (!bio ||
		(bio->bi_vcnt == bi_max_vecs) ||
		(psector != nsector)) {
		unsigned int	vsize;
		unsigned int	nr_vecs;
		struct request_queue* q;

	        if (bio) {
		    vbd_submit_bio(xreq, bio);
	        }
		vsize = count - seg;

		/* we cannot place a request longer than the bio hw sector size
                - so in case of large requests, we create chunks of number of
                bio vectors accepted by the phys. dev. if needed */
		q         = bdev_get_queue (ex->bdev);
		nr_vecs   = bio_get_nr_vecs(ex->bdev);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
		{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
		    const unsigned max_hw_pg = queue_max_hw_sectors(q) >> 3;
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
		if (oneBio && vsize != count) {
	            ETRACE("%s cannot alloc several bios\n", vd->name);
	            xreq->error = TRUE;
		    bio         = NULL;
	            break;
		}
		bio = bio_alloc(GFP_ATOMIC, vsize);

                if (unlikely(!bio)) {
	            ETRACE("%s sector %d, out of bios; will retry\n",
			   vd->name, xreq->vsector);
		    bl->be->resource_error = TRUE;
		    bl->pending_xreq = xreq;
		    ++bl->resourceErrors;
		    finish_later = TRUE;
	            break;
                }
		/* bio_alloc fixes vsize at bio's allocation time according to
                the mem. pool where the bio has been allocated - we have to
                backup the value of vsize to be able to send the bio request
                when the allocated space is full */
		bi_max_vecs 	= vsize;
                bio->bi_bdev    = ex->bdev;
                bio->bi_private = xreq;
		bio->bi_end_io  = vbd_bio_end_block_io_op;
                bio->bi_sector  = psector;
		bio->bi_rw      = acc == VBD_DISK_ACC_W ? WRITE : READ;
	        bio->bi_size    = 0;
                bio->bi_vcnt    = 0;
	    }
	    bv = bio_iovec_idx(bio, bio->bi_vcnt);
            bv->bv_len    = psize;
	    bv->bv_offset = paddr & ~VBD_PAGE_64_MASK;

	    pseg = xreq->segs + xreq->nsegs;

	    if (copyMode) {
		unsigned long vaddr = __get_free_page(0);

		if (unlikely(!vaddr)) {
		    ETRACE("%s sector %d, out of pages; will retry\n",
			   vd->name, xreq->vsector);
		    bl->be->resource_error = TRUE;
		    bl->pending_xreq = xreq;
		    ++bl->resourceErrors;
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
		++bl->pageAllocs;
		++bl->allocedPages;
		if (bl->allocedPages > bl->maxAllocedPages) {
		    bl->maxAllocedPages = bl->allocedPages;
		}
		bv->bv_page   = virt_to_page(vaddr);

		pseg->vaddr = vaddr;
		if (isRead) {
		    pseg->gaddr = paddr;
		    pseg->size  = psize;
			/*
			 *  We do not know yet if this is going to
			 *  be a fastMapRead or an nkMemMapRead.
			 */
		} else {
			// copy guest page
		    if (map) {
			unsigned long src = ((unsigned long)map->addr) +
			    bv->bv_offset;
			unsigned long dst = vaddr + bv->bv_offset;
			vbd_fast_map(map, paddr & VBD_PAGE_64_MASK);
			memcpy((void*)dst, (void*)src, psize);
			vbd_fast_unmap(map);
			++bl->fastMapWrites;
			bl->fastMapWrittenBytes += psize;
		    } else {
			void*         src = nkops.nk_mem_map(paddr, psize);
			unsigned long dst = vaddr + bv->bv_offset;
			if (!src) {
			    ETRACE("%s sector %d, cannot mem map; fatal\n",
				   vd->name, xreq->vsector);
			    free_page(vaddr);
			    ++bl->pageFreeings;
			    --bl->allocedPages;
			    xreq->error = TRUE;
			    break;
			}
			memcpy((void*)dst, src, psize);
			nkops.nk_mem_unmap(src, paddr, psize);
			++bl->nkMemMapWrites;
			bl->nkMemMapWrittenBytes += psize;
		    }
		}
	    } else {
		pseg->vaddr = 0; /* no page */
		bv->bv_page = pfn_to_page(paddr >> PAGE_SHIFT);
		if (isRead) {
		    bl->dmaReadBytes += psize;
		    ++bl->dmaReads;
		} else {
		    bl->dmaWrittenBytes += psize;
		    ++bl->dmaWrites;
		}
	    }
	    if (isRead) {
		bl->bytesRead += psize;
	    } else {
		bl->bytesWritten += psize;
	    }
            bio->bi_vcnt++;
            bio->bi_size += psize;
		/* Increment nsegs only now, to validate "pseg" */
	    ++xreq->nsegs;

	    xreq->vsector += ssize;
	    nsector  = psector + ssize;
        }
	if (map) {
	    vbd_fast_map_free(bl, map);
	}
	if (bio) {
	    vbd_submit_bio(xreq, bio);
        }
	    /*
	     *  The xreq->pendcnt reference count, which has been set
	     *  to 1 initially, protects xreq against being considered
	     *  as complete when all "bios" submitted up to now complete.
	     */
	if (!finish_later) {
	    vbd_req_end_block_io_op(xreq);
	}
    }
#endif

    return FALSE;
}

/******************************************************************************
 * NOTIFICATION FROM GUEST OS.
 */
    static void
vbd_blkif_interrupt (void* cookie, NkXIrq xirq)
{
    VbdBlkif* bl = VBD_BLKIF(cookie);

    vbd_add_to_blkdev_list_tail(bl);
    vbd_maybe_trigger_blkio_schedule();
}

/******************************************************************
 * DOWNWARD CALLS -- These interface with the block-device layer proper.
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)

    // Called by vbd_do_block_io_op() only.

    static void
vbd_blkio_done (VbdBlkif* bl)
{
    vbd_pending_req_t* req;

    while ((req = vbd_get_from_done_list(bl))) {
	vbd_pending_seg_t* seg   = req->segs;
	vbd_pending_seg_t* limit = req->segs + req->nsegs;
	while (seg != limit) {
	    if (seg->vaddr) {
		void*         dst = nkops.nk_mem_map(seg->gaddr, seg->size);
		unsigned long src =
			    seg->vaddr + (seg->gaddr & ~VBD_PAGE_64_MASK);

		if (dst) {
		    memcpy(dst, (void*)src, seg->size);
		    nkops.nk_mem_unmap(dst, seg->gaddr, seg->size);
		    ++bl->nkMemMapReads;
		    bl->nkMemMapReadBytes += seg->size;
		} else {
		    req->error   = TRUE;
		}
		free_page(seg->vaddr);
		++bl->pageFreeings;
		--bl->allocedPages;
	    }
	    seg++;
	}
	vbd_block_io_done(req);
    }
}

#endif

    // Called only by the vbd_blkio_schedule() thread.
    // Can set be->resource_error.

    static int
vbd_do_block_io_op (VbdBlkif* bl, int max_to_do)
{
    NkDevRing* ring = bl->ring;
    nku8_f*    base = bl->vring;
    nku32_f    oreq = bl->req;
    nku32_f    mask = ring->imask;
    nku32_f    size = ring->dsize;
    int	       resp = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    vbd_blkio_done(bl);
#endif

    while (max_to_do && (oreq != ring->ireq)) {
	RDiskReqHeader* req = (RDiskReqHeader*)(base + ((oreq & mask) * size));

	    /* "connected" just means today that the VM is running */
	if (bl->connected) {
	    switch (req->op) {
	    case RDISK_OP_PROBE:
		resp += vbd_blkif_probe(bl, vbd_op_probe, req);
		break;
	    case RDISK_OP_MEDIA_PROBE:
		resp += vbd_blkif_probe(bl, vbd_op_media_probe, req);
		break;
	    case RDISK_OP_MEDIA_CONTROL:
		resp += vbd_blkif_probe(bl, vbd_op_media_control, req);
		break;
	    case RDISK_OP_MEDIA_LOCK:
		resp += vbd_blkif_probe(bl, vbd_op_media_lock, req);
		break;
#ifdef VBD_ATAPI
	    case RDISK_OP_ATAPI:
		resp += vbd_blkif_atapi(bl, req);
		break;
#endif

	    case RDISK_OP_READ_EXT:
	    case RDISK_OP_WRITE_EXT:
	    case RDISK_OP_READ:
	    case RDISK_OP_WRITE:
		resp += vbd_blkif_rw(bl, req);
		break;

	    default:
		vbd_blkif_resp(bl, req->cookie, req->op,RDISK_STATUS_ERROR);
		resp += 1;
		break;
	    }
	} else {
	    vbd_blkif_resp(bl, req->cookie, req->op, RDISK_STATUS_ERROR);
	    resp += 1;
  	}
	    /*
	     *  Do not increment oreq in case of be->resource_error,
	     *  because current request is not finished yet.
	     */
	if (bl->be->resource_error) break;

	oreq++;
	max_to_do--;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	vbd_blkio_done(bl);
#endif
    }
    bl->req = oreq;
	/*
	 * If a response was en-queued, then trigger appropriate XIRQ
	 * Most of the time there should be no response yet, because
	 * the request has just been submitted to the disk driver.
	 */
    if (resp) {
	nkops.nk_xirq_trigger(ring->pxirq, ring->pid);
	++bl->xirqTriggers;
    }
    return !max_to_do;
}

/******************************************************************
 * MISCELLANEOUS SETUP / TEARDOWN / DEBUGGING
 */

/*
 * VBD disk driver
 */

    static void
vbd_be_sysconf_xirq (void* cookie, NkXIrq xirq)
{
    VbdBe*	be      = VBD_BE(cookie);
    NkOsMask	running = nkops.nk_running_ids_get();
    VbdBlkif*	bl;

    VBD_BE_FOR_ALL_BLKIFS(bl, be) {
        NkOsMask mask = nkops.nk_bit2mask(bl->ring->pid);
        if (running & mask) {
	    if (!bl->connected) {
                bl->connected = 1;
                DTRACE("front-end driver %d connected\n", bl->ring->pid);
	    }
	} else {
            if (bl->connected) {
                bl->connected = 0;
                DTRACE("front-end driver %d disconnected\n", bl->ring->pid);
            }
        }
    }
}

/*
 *  /proc/nk/vbd-be management
 */

#include <linux/seq_file.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) && \
    LINUX_VERSION_CODE != KERNEL_VERSION(2,4,21) && \
    LINUX_VERSION_CODE != KERNEL_VERSION(2,4,25)

    static void*
single_start (struct seq_file *p, loff_t *pos)
{
    return NULL + (*pos == 0);
}

    static void*
single_next (struct seq_file *p, void *v, loff_t *pos)
{
    ++*pos;
    return NULL;
}

    static void
single_stop (struct seq_file *p, void *v)
{
}

    static int
single_open (struct file *file, int (*show)(struct seq_file *, void *),
	     void *data)
{
    struct seq_operations *op = kmalloc(sizeof(*op), GFP_KERNEL);
    int res = -ENOMEM;

    if (op) {
	op->start = single_start;
	op->next = single_next;
	op->stop = single_stop;
	op->show = show;
	res = seq_open(file, op);
	if (!res)
	    ((struct seq_file *)file->private_data)->private = data;
	else
	    kfree(op);
    }
    return res;
}

    static int
single_release (struct inode *inode, struct file *file)
{
    struct seq_operations *op = ((struct seq_file *)file->private_data)->op;
    int res = seq_release(inode, file);

    kfree(op);
    return res;
}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) */

static const char vbd_version[] = "vlx-vbd-be.c 1.74 10/06/15";

    static int
vbd_proc_show (struct seq_file* seq, void* v)
{
    VbdBlkif* bl;

    (void) v;
    seq_printf (seq, "Driver version: %s\n", vbd_version);
    VBD_BE_FOR_ALL_BLKIFS(bl, &vbd_be) {
	VbdVdisk* vd;

	seq_printf (seq, "I/O with VM %d (%s connected, %s added):\n",
	    bl->ring->pid, bl->connected ? "is" : "not",
	    bl->nk_dev_added ? "is" : "not");
	seq_printf (seq, " General: requests %u segments %u read %llu"
	    " written %llu\n",
	    bl->requests,
	    bl->segments,
	    bl->bytesRead,
	    bl->bytesWritten);
	seq_printf (seq, " DMA: reads %u writes %u read %llu written %llu\n",
	    bl->dmaReads,
	    bl->dmaWrites,
	    bl->dmaReadBytes,
	    bl->dmaWrittenBytes);
	seq_printf (seq,
	    " FastMap: probes %u reads %u writes %u read %llu written %llu\n",
	    bl->fastMapProbes,
	    bl->fastMapReads,
	    bl->fastMapWrites,
	    bl->fastMapReadBytes,
	    bl->fastMapWrittenBytes);
	seq_printf (seq,
	    " NkMemMap: probes %u reads %u writes %u read %llu written %llu\n",
	    bl->nkMemMapProbes,
	    bl->nkMemMapReads,
	    bl->nkMemMapWrites,
	    bl->nkMemMapReadBytes,
	    bl->nkMemMapWrittenBytes);
	seq_printf (seq, " noStructPage %u pageAllocs %u Freeings %u"
	    " Max %u xirqs %u\n", bl->noStructPage, bl->pageAllocs,
	    bl->pageFreeings, bl->maxAllocedPages, bl->xirqTriggers);
	seq_printf (seq, " firstMegCopy %u resourceErrors %u\n",
	    bl->firstMegCopy, bl->resourceErrors);

	VBD_BLKIF_FOR_ALL_VDISKS (vd, bl) {
	    const VbdExtent* ex;
	    unsigned count = 0;

	    VBD_VDISK_FOR_ALL_EXTENTS (ex, vd) ++count;
	    seq_printf (seq, " vdisk %d: %s %u extent(s), %llu "
	    		"sectors, %s mode",
			vd->tag, vd->name, count,
			(unsigned long long) vbd_vdisk_size (vd),
			vbd_vdisk_wait_modes [vd->wait % 3]);
#ifdef VBD_ATAPI
	    if (vd->atapi) {
		seq_printf (seq, ", ATAPI(%s,%s)",
			    vd->atapi->ctrl_name, vd->atapi->data_name);
	    }
#endif
	    seq_printf (seq, "\n");
	}
    }
    seq_printf (seq, "Sleeps %llu withTimeout %u timeouts %u\n",
	vbd_sleeps_no_timeout, vbd_sleeps_with_timeout, vbd_timeouts);
    return  0;
}

    static int
vbd_proc_open (struct inode *inode, struct file *file)
{
    return single_open(file, vbd_proc_show, NULL);
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

#ifdef VBD_ATAPI
static struct file_operations vbd_atapi_ctrl_proc_fops = {
    open:    _vbd_atapi_ctrl_proc_open,
    release: _vbd_atapi_ctrl_proc_release,
    llseek:  _vbd_atapi_ctrl_proc_lseek,
    read:    _vbd_atapi_ctrl_proc_read,
    write:   _vbd_atapi_ctrl_proc_write,
};

static struct file_operations vbd_atapi_data_proc_fops = {
    open:    _vbd_atapi_data_proc_open,
    release: _vbd_atapi_data_proc_release,
    llseek:  _vbd_atapi_data_proc_lseek,
    read:    _vbd_atapi_data_proc_read,
    write:   _vbd_atapi_data_proc_write,
};
#endif /* VBD_ATAPI */

    static int __init
vbd_proc_init (void)
{
    struct proc_dir_entry* ent;

    ent = create_proc_entry("nk/vbd-be", 0, NULL);
    if (!ent) {
	return -ENOMEM;
    }
    ent->proc_fops = &vbd_proc_fops;
    return 0;
}

    static void __exit
vbd_proc_exit (void)
{
    remove_proc_entry ("nk/vbd-be", NULL);
}

#ifdef VBD_ATAPI
    static _Bool __init
vbd_atapi_init (VbdVdisk* vd)
{
    struct proc_dir_entry* ent;
    int                    major, minor;
    atapi_t*               atapi;

    if (!vbd_is_atapi(vd, &major, &minor)) {
	return 1;
    }
    if (unlikely(!(atapi = kzalloc(sizeof(atapi_t), GFP_KERNEL)))) {
	ETRACE("%s: out of memory\n", __FUNCTION__);
	return 0;
    }
    mutex_init(&atapi->lock);
    init_waitqueue_head(&atapi->wait);

    snprintf(atapi->ctrl_name, sizeof atapi->ctrl_name,
	     "nk/vbd-atapi-c-%u-%u", major, minor);
    ent = create_proc_entry(atapi->ctrl_name, 0, NULL);
    if (!ent) {
	kfree(atapi);
	return 0;
    }
    ent->data = vd;
    ent->proc_fops = &vbd_atapi_ctrl_proc_fops;

    snprintf(atapi->data_name, sizeof atapi->data_name,
	     "nk/vbd-atapi-d-%u-%u", major, minor);
    ent = create_proc_entry(atapi->data_name, 0, NULL);
    if (!ent) {
	remove_proc_entry (atapi->ctrl_name, NULL);
	kfree(atapi);
	return 0;
    }
    ent->data = vd;
    ent->proc_fops = &vbd_atapi_data_proc_fops;

    vd->atapi = atapi;
    return 1;
}

    static void __exit
vbd_atapi_exit (VbdVdisk* vd)
{
    if (vd->atapi) {
	remove_proc_entry (vd->atapi->ctrl_name, NULL);
	remove_proc_entry (vd->atapi->data_name, NULL);
	kfree(vd->atapi);
	vd->atapi = 0;
    }
}
#endif /* VBD_ATAPI */

/*
 * Command line / module options management
 */

    static _Bool __init
vbd_vdisk_syntax (const char* opt)
{
    ETRACE("Syntax error near '%s'\n", opt);
    return FALSE;
}

    static _Bool inline __init
vbd_vdisk_end (const char ch)
{
    return ch == ')' || ch == ';';
}

static VbdPropDiskVdisk  vbddisk[CONFIG_NKERNEL_VBD_NR];

    static _Bool __init
vbd_vdisk_one (char* start, char** endp)
{
    long		ring_elems   = VBD_BLKIF_DEFAULT_RING_IMASK + 1;
    long		segs_per_req = VBD_BLKIF_MAX_SEGS_PER_REQ;
#ifdef MODULE
    vbd_vdisk_wait_t	wait = VBD_VDISK_WAIT_NO;
#else
    vbd_vdisk_wait_t	wait = VBD_VDISK_WAIT_YES;
#endif
    long		owner;
    long		vmajor;
    long		vminor;
    long		major;
    long		minor;
    long		acc;
    int			idx;
    char*		end;

    owner = simple_strtoul(start, &end, 0);
    if ((end == start) || (*end != ',')) {
	return vbd_vdisk_syntax(end);
    }
    start = end+1;

    vmajor = simple_strtoul(start, &end, 0);
    if ((end == start) || (*end != ',')) {
	return vbd_vdisk_syntax(end);
    }
    start = end+1;

    vminor = simple_strtoul(start, &end, 0);
    if ((end == start) ||
	((*end != ':') && (*end != '|') && (*end != '/'))) {
	return vbd_vdisk_syntax(end);
    }
    start = end+1;

    major = simple_strtoul(start, &end, 0);
    if ((end == start) || (*end != ',')) {
	return vbd_vdisk_syntax(end);
    }
    start = end+1;

    minor = simple_strtoul(start, &end, 0);
    if ((end == start) || (*end != ',')) {
	return vbd_vdisk_syntax(end);
    }
    start = end+1;

    if (!strncmp(start, "ro", 2)) {
	acc = VBD_DISK_ACC_R;
    } else if (!strncmp(start, "rw", 2)) {
	acc = VBD_DISK_ACC_RW;
    } else {
	return vbd_vdisk_syntax(start);
    }
    start += 2;

	/*
	 * vdisk=(... [,[nw|wa|nz]])
	 */
    if (*start == ',') {
	++start;
	if (*start != ',' && !vbd_vdisk_end(*start)) {
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
		return vbd_vdisk_syntax(start);
	    }
	    start += 2;
	}
    }
	/*
	 * vdisk=(... [,[<elem>][,[<segs_per_req>]]])
	 */
    if (*start == ',') {
	++start;
	if (*start != ',' && !vbd_vdisk_end(*start)) {
	    ring_elems = simple_strtoul(start, &end, 0);
	    if (end == start) return vbd_vdisk_syntax(end);
	    start = end;
	    if (ring_elems > 0) {
		    /* Round down to highest power of 2 */
		ring_elems = nkops.nk_bit2mask(nkops.nk_mask2bit(ring_elems));
	    } else {
		ring_elems = VBD_BLKIF_DEFAULT_RING_IMASK + 1;
	    }
	}
	if (*start == ',') {
	    ++start;
	    if (!vbd_vdisk_end(*start)) {
		segs_per_req = simple_strtoul(start, &end, 0);
		if (end == start) return vbd_vdisk_syntax(end);
		if (segs_per_req <= 0 ||
		    segs_per_req > VBD_BLKIF_MAX_SEGS_PER_REQ) {
		    segs_per_req = VBD_BLKIF_MAX_SEGS_PER_REQ;
		}
	    }
	}
    }
    if (!vbd_vdisk_end(*start)) {
	return vbd_vdisk_syntax(start);
    }
    for (idx = 0; idx < CONFIG_NKERNEL_VBD_NR; ++idx) {
	if (!vbddisk[idx].id) {
	    break;
	}
	if ((vbddisk[idx].owner == owner) &&
	    (vbddisk[idx].id == VBD_DISK_ID(vmajor, vminor))) {
	    ETRACE("overwriting vdisk(%ld,%ld) config.\n",
		    vmajor, vminor);
	    break;
	}
    }
    if (idx < CONFIG_NKERNEL_VBD_NR) {
	vbddisk[idx].tag          = idx;
	vbddisk[idx].owner        = owner;
	vbddisk[idx].id           = VBD_DISK_ID(vmajor, vminor);
	vbddisk[idx].ring_imask   = ring_elems-1;
	vbddisk[idx].segs_per_req = segs_per_req;
	vbddisk[idx].wait         = wait;

	vbdextent[idx].major    = major;
	vbdextent[idx].minor    = minor;
	vbdextent[idx].tag      = vbddisk[idx].tag;
	vbdextent[idx].access   = acc;
	vbdextent[idx].start    = 0;
	vbdextent[idx].size     = 0;
	vbdextent[idx].bound    = FALSE;
    } else {
	ETRACE("too many disks: vdisk(%ld,%ld) ignored\n", vmajor, vminor);
    }
    *endp = start;
    return TRUE;
}

    /* This function needs to be "int" for the __setup() macro */

    static int __init
vbd_vdisk_setup (char* start)
{
    do {
	if (*start != '(') {
	    return vbd_vdisk_syntax(start);
	}
	++start;
	do {
	    if (!vbd_vdisk_one(start, &start)) {
		return FALSE;
	    }
	} while (*start++ == ';');
    } while (*start++ == ',');
    return TRUE;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    static int __init
vbd_vdisk_dma_setup (char* start)
{
    char* end;
    vdisk_dma = simple_strtoul(start, &end, 0);
    return 1;
}
#endif

#ifndef MODULE

__setup("vdisk=", vbd_vdisk_setup);
__setup("vdisk_dma=", vbd_vdisk_dma_setup);

#else

MODULE_DESCRIPTION("VLX virtual disk backend driver");
MODULE_AUTHOR("Eric Lescouet <eric.lescouet@redbend.com>, "
	"derived from Keir Fraser and Steve Hand Xen driver");
MODULE_LICENSE("GPL");
    /*
     * loading parameters
     */
static char* vdisk[CONFIG_NKERNEL_VBD_NR];
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
MODULE_PARM(vdisk, "1-" __MODULE_STRING(CONFIG_NKERNEL_VBD_NR) "s");
MODULE_PARM(vdisk_dma, "i");
#else
module_param_array(vdisk, charp, NULL, 0);
module_param(vdisk_dma, int, 0);
#endif
MODULE_PARM_DESC(vdisk_dma, " Use DMA to front-end buffers (dflt: 1).");
MODULE_PARM_DESC(vdisk,
		 "  virtual disks configuration:\n\t\t"
		 "   \"vdisk=(<owner>,<vmaj>,<vmin>/<maj>,<min>,"
			"<access>[,[<wait>][,[<elems>][,"
			"[<segs>]]]])[,...]\"\n\t\t"
		 "  where:\n\t\t"
		 "   <owner>       is the owner OS ID in [0..31]\n\t\t"
		 "   <vmaj>,<vmin> is the virtual disk ID (major,minor)\n\t\t"
		 "   <maj>,<min>   is the associated real device "
		 	"major,minor\n\t\t"
		 "   <access>      defines access rights: \"ro\" or"
		 " \"rw\"\n\t\t"
		 "   <wait>        \"wa\"(wait) \"nw\"(no wait) or"
		 " \"nz\"(wait non-zero)\n\t\t"
		 "   <elems>       fifo element count\n\t\t"
		 "   <segs>        max length of i/o requests\n");

#ifdef CONFIG_ARM
#define vlx_command_line	saved_command_line
#else
extern char* vlx_command_line;
#endif

#endif

    // Called from vbd_module_init() only.

    static void __init
vbd_start (VbdBe* be)
{
    VbdBlkif* bl;

    VBD_BE_FOR_ALL_BLKIFS(bl, be) {
	vbd_blkif_start(bl);
    }
}

    // Called from vbd_module_exit() only.

    static void __exit
vbd_stop (VbdBe* be)
{
    VbdBlkif* bl;

    VBD_BE_FOR_ALL_BLKIFS(bl, be) {
	vbd_blkif_stop(bl);
    }
}

    // Called from vbd_module_exit() only.

    static void __exit
vbd_destroy (VbdBe* be)
{
    VbdBlkif* bl;

    while ((bl = be->blkifs)) {
	be->blkifs = bl->next;
	vbd_blkif_destroy(bl);
    }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
static struct task_struct* vbd_th_desc;		// thread descriptor
#else
static pid_t vbd_th_pid;
#endif
static pid_t vbd_th_extent_pid;

    static int __init
vbd_module_init (void)
{
    VbdBe* be = &vbd_be;

#ifdef	MODULE
    char** opt;
    char*  cmdline;
        /*
	 * Parse kernel command line options first ...
	 */
    cmdline = vlx_command_line;
    while ((cmdline = strstr(cmdline, "vdisk="))) {
	cmdline += 6;
	vbd_vdisk_setup(cmdline);
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    cmdline = vlx_command_line;
    if ((cmdline = strstr(cmdline, "vdisk_dma="))) {
	vbd_vdisk_dma_setup(cmdline + 10);
    }
#endif

        /*
	 * ... then arguments given to insmod.
	 */
    opt = vdisk;
    while (*opt) {
	vbd_vdisk_setup(*opt);
	opt++;
    }
#endif	/* MODULE */

    if (!(vbddisk[0].id)) {
	/* No virtual disk configured */
        return 0;
    }
	/*
	 * Attach handler to the SYSCONF XIRQ
	 */
    be->xid = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF, vbd_be_sysconf_xirq, be);
    if (!be->xid) {
	ETRACE("xirq_attach(%d) failure\n", NK_XIRQ_SYSCONF);
	return 0;
    }
	/*
	 * Analyze virtual disks configuration.
	 */
    {
	const VbdPropDiskVdisk*  vds = vbddisk;
	while (vds->id) {
	    vbd_vdisk_create(be, vds++);
	}
    }
    spin_lock_init(&vbd_blkio_schedule_list_lock);
    INIT_LIST_HEAD(&vbd_blkio_schedule_list);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)
    if ((vbd_th_pid = kernel_thread(vbd_blkio_schedule, be,
				    CLONE_FS | CLONE_FILES)) < 0) {
	BUG();
    }
#else
    vbd_th_desc = kthread_run(vbd_blkio_schedule, be,
			      VBD_BLKIO_SCHEDULE_THREAD);
    if (IS_ERR(vbd_th_desc)) {
	BUG();
    }
#endif
    if ((vbd_th_extent_pid = kernel_thread(vbd_extent_thread, be,
					   CLONE_FS | CLONE_FILES)) < 0) {
	BUG();
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    vbd_buffer_head_cachep = kmem_cache_create("buffer_head_cache",
					   sizeof(struct buffer_head),
					   0, SLAB_HWCACHE_ALIGN, NULL, NULL);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
    vbd_pending_req_cachep = kmem_cache_create("vbd_pending_req_cache",
					   sizeof(vbd_pending_req_t),
					   0, SLAB_HWCACHE_ALIGN, NULL, NULL);
#else
    vbd_pending_req_cachep = kmem_cache_create("vbd_pending_req_cache",
					   sizeof(vbd_pending_req_t),
					   0, SLAB_HWCACHE_ALIGN, NULL);
#endif
    vbd_start(be);	// Activate block interfaces...

    vbd_proc_init();

    TRACE("module loaded (%s)\n", vbd_version);
    return 0;
}

    static void __exit
vbd_module_exit (void)
{
    VbdBe* be = &vbd_be;

    if (be->xid) {
        nkops.nk_xirq_detach(be->xid);
    }
    vbd_stop(be);
	/* Wake up the extent thread */
    vbd_extent_abort = 1;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)
    wait_for_completion(&vbd_th_completion);
#else
    kthread_stop(vbd_th_desc);
#endif
    wait_for_completion(&vbd_th_extent_completion);

    vbd_proc_exit();

    vbd_destroy(be);

    kmem_cache_destroy(vbd_pending_req_cachep);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    kmem_cache_destroy(vbd_buffer_head_cachep);
#endif

    TRACE("module unloaded\n");
}

module_init(vbd_module_init);
module_exit(vbd_module_exit);

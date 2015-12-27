/*
 ****************************************************************
 *
 *  Component: VLX Virtual Block Device v.2 frontend driver
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
 */

/******************************************************************************
 * arch/xen/drivers/blkif/frontend/common.h
 *
 * Shared definitions between all levels of XenoLinux Virtual block devices.
 */

/******************************************************************************
 * blkfront.c
 *
 * XenLinux virtual block-device driver.
 *
 * Copyright (c) 2003-2004, Keir Fraser & Steve Hand
 * Modifications by Mark A. Williamson are (c) Intel Research Cambridge
 * Copyright (c) 2004, Christian Limpach
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

/******************************************************************************
 * arch/xen/drivers/blkif/frontend/vbd.c
 *
 * Xenolinux virtual block-device driver.
 *
 * Copyright (c) 2003-2004, Keir Fraser & Steve Hand
 * Modifications by Mark A. Williamson are (c) Intel Research Cambridge
 */

/*----- Detection of MontaVista CGE 3.1 -----*/

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#include <linux/termios.h>		/* Only to detect Montavista */
#include <linux/tty_driver.h>		/* Only to detect Montavista */
#ifdef _KOBJECT_H_
#define VBD_MV_CGE_31		/* Specific support for MontaVista CGE 3.1 */
#endif
#endif

/*----- System header files -----*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
#include <linux/config.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
#include <linux/freezer.h>
#endif
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/major.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
#include <linux/devfs_fs_kernel.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
    /* This include exists in 2.6.6 but functions are not yet exported */
#include <linux/kthread.h>
#endif
#ifdef VBD_MV_CGE_31
#include <linux/kobject.h>
#endif
#include <asm/io.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <asm/cacheflush.h>
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#include <linux/blk.h>
#include <linux/tqueue.h>
#include <linux/signal.h>
#endif
#include <linux/cdrom.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <scsi/scsi.h>
#include <linux/blkpg.h>	/* blk_ioctl() */
#include <linux/proc_fs.h>
#include <asm/io.h>		/* ioremap */
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
#include <linux/semaphore.h>
#endif
#include <linux/init.h>		/* module_init() in 2.6.0 and before */
#include <linux/loop.h>		/* LOOP_CLR_FD ioctl */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <linux/dma-mapping.h>
#endif
#include <linux/fd.h>           /* FDEJECT */
#include <nk/nkern.h>
#include <vlx/vbd2_common.h>
#include "vlx-vmq.h"
#include "vlx-vipc.h"

/*----- Local configuration -----*/

#if 0
#define VBD_DEBUG
#endif

#if 0
#define VBD_ASSERTS
#endif

#if 0
    /* For 2.4 only */
#define VBD_DEBUG_NOISY
#endif

#if 0
    /* For 2.4 only */
#define VBD_DEBUG_STATS
#endif

#if 0
    /* For 2.4 only */
#define VBD_DEBUG_REQUESTS
#endif

#if 0
    /* For 2.4 only */
#define VBD_DEBUG_OBSOLETE
#endif

    /* Select at most one of the histogram-enabling #defines below */
#if 0
#define VBD_DEBUG_HISTO_BHS
#elif 0
#define VBD_DEBUG_HISTO_SEGMENTS
#elif 1
#define VBD_DEBUG_HISTO_SECTORS
#endif

    /*
     * For convenience we distinguish between ide, scsi and 'other' (i.e.
     * potentially combinations of the two) in the naming scheme and in a few
     * other places (like default readahead, etc).
     */
#define VBD_NUM_IDE_MAJORS	10
#define VBD_NUM_SCSI_MAJORS	9
#define VBD_NUM_VBD_MAJORS	1
#define VBD_NUM_FD_MAJORS	1
#ifdef MMC_BLOCK_MAJOR
#define VBD_NUM_MMC_MAJORS	1
#else
#define VBD_NUM_MMC_MAJORS	0
#endif

#define VBD_LINK_MAX_DISKS		64
#define	VBD_LINK_MAX_SEGS_PER_REQ	128
#define VBD_LINK_DEFAULT_MSG_COUNT	64

/*----- Tracing -----*/

#define TRACE(_f, _a...)  printk (KERN_INFO    "VBD2-FE: " _f, ## _a)
#define WTRACE(_f, _a...) printk (KERN_WARNING "VBD2-FE: Warning: " _f, ## _a)
#define ETRACE(_f, _a...) printk (KERN_ERR     "VBD2-FE: Error: " _f, ## _a)
#define XTRACE(_f, _a...)

#ifdef	VBD_DEBUG
#define DTRACE(_f, _a...) \
	do {printk (KERN_ALERT "%s: " _f, __func__, ## _a);} while (0)
#define VBD_CATCHIF(cond,action)	if (cond) action;
#else
#define DTRACE(_f, _a...) ((void)0)
#define VBD_CATCHIF(cond,action)
#endif

#ifdef VBD_ASSERTS
#define VBD_ASSERT(c)	do {if (!(c)) BUG();} while (0)
#else
#define VBD_ASSERT(c)
#endif

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

#ifndef container_of
#define container_of(ptr, type, member) \
	((type*)((char*)(ptr)-(unsigned long)(&((type*) 0)->member)))
#endif

/*----- Data types -----*/

typedef struct {
    const char*	name;
    int		partn_shift;
    int		devs_per_major;
} vbd_type_t;

   /*
    * We have one of these per vbd, whether ide, scsi or 'other'.
    * They hang in private_data off the gendisk structure.
    */
typedef struct {
    int			major;
    int			usage;
    const vbd_type_t*	type;
    int			index;
    int			major_idx;	/* Position of entry in array */
} vbd_major_t;

typedef struct vbd_link_t vbd_link_t;

typedef struct vbd_disk_t {
    vbd2_devid_t		xd_devid;	/* Portable representation */
    vbd2_genid_t		xd_genid;
    vbd_major_t*		major;
    struct gendisk*		gd;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    struct request_queue*	gd_queue;
    _Bool			req_is_pending;
    _Bool			is_part;
    devfs_handle_t		devfs_handle;
#endif
    dev_t			device;		/* Local representation */
    struct vbd_disk_t*		next;
    int				usage;
    _Bool			is_zombie;
    vbd_link_t*			vbd;
    _Bool			still_valid;	/* For syncing with backend */
    char			name [24];
} vbd_disk_t;

/*----- Const data -----*/

static const vbd_type_t vbd_type_ide  = {"hd",  6,  2};
static const vbd_type_t vbd_type_scsi = {"sd",  4, 16};
static const vbd_type_t vbd_type_vbd  = {"xvd", 4,  0};
static const vbd_type_t vbd_type_fd   = {"fd",  0,  1};
#ifdef MMC_BLOCK_MAJOR
static const vbd_type_t vbd_type_mmc  = {"mmc", 3, 256 >> 3};
#endif

/*----- Data -----*/

typedef struct vbd_fe_t vbd_fe_t;

struct vbd_link_t {
    vbd_fe_t*		fe;
    vmq_link_t*		link;
    unsigned		msg_max;
    unsigned		segs_per_req_max;
    vbd_disk_t*		disks;
    spinlock_t		io_lock;
    vmq_xx_config_t	xx_config;
    vipc_ctx_t		vipc_ctx;
    _Bool		is_up;
    _Bool		changes;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    unsigned*		data_offsets;
#endif
    struct request**	reqs;		/* indexed by msg slot */
	/* Statistics */
    unsigned		errors;
};

    static inline void
vbd_disk_init (vbd_disk_t* di, vbd2_devid_t xd_devid, vbd2_genid_t xd_genid,
	       vbd_major_t* major, struct gendisk* gd, dev_t device,
	       vbd_link_t* vbd)
{
    di->xd_devid = xd_devid;
    di->xd_genid = xd_genid;
    di->major    = major;
    di->gd       = gd;
    di->device   = device;
    di->vbd      = vbd;
    snprintf (di->name, sizeof di->name, "(%d,%d:%d)",
	      VBD2_DEVID_MAJOR (xd_devid), VBD2_DEVID_MINOR (xd_devid),
	      xd_genid);
    di->still_valid = true;
	/* Prepend to the list of disks */
    di->next     = vbd->disks;
    vbd->disks   = di;
}

#define VBD_LINK_FOR_ALL_DISKS(_di,_vbd) \
    for ((_di) = (_vbd)->disks; (_di); (_di) = (_di)->next)

    /*
     * _26 and _24 functions are specific to one release of Linux
     * and are only called from specific code. So they exist in one
     * copy.
     * _2x functions have different implementations according
     * to the release of Linux, but are called from shared code.
     * So they exist in 2 copies.
     * Functions without _2 are generic.
     */

    /* We plug the I/O ring if the driver is suspended or out of msgs */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
static int  vbd_blkif_open_2x (struct block_device* bdev, fmode_t fmode);
static int  vbd_blkif_release_2x (struct gendisk* gd, fmode_t fmode);
static int  vbd_blkif_ioctl   (struct block_device* bdev, fmode_t fmode,
			       unsigned command, unsigned long argument);
#else
static int  vbd_blkif_open_2x (struct inode* inode, struct file* filep);
static int  vbd_blkif_release_2x (struct inode* inode, struct file* filep);
static int  vbd_blkif_ioctl   (struct inode* inode, struct file* filep,
			       unsigned command, unsigned long argument);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
static int  vbd_blkif_getgeo_26  (struct block_device*, struct hd_geometry*);
#endif

    /* Cannot be defined as "const" */

    static struct block_device_operations
vbd_block_fops = {
    .owner   = THIS_MODULE,
    .open    = vbd_blkif_open_2x,
    .release = vbd_blkif_release_2x,
    .ioctl   = vbd_blkif_ioctl,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
    .getgeo  = vbd_blkif_getgeo_26
#endif
};

#define VLX_SERVICES_THREADS
#include "vlx-services.c"

#define VBD_ARRAY_ELEMS(a)	(sizeof (a) / sizeof (a) [0])

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
typedef struct {
    int blksize_size  [256];
    int hardsect_size [256];
    int max_sectors   [256];
} vbd_onemajor_t;
#endif

struct vbd_fe_t {
    vbd_major_t*	majors [VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS
				+ VBD_NUM_VBD_MAJORS + VBD_NUM_FD_MAJORS
				+ VBD_NUM_MMC_MAJORS];
    volatile _Bool	must_wait;
    vbd2_devid_t	wait_devid;
    struct semaphore	thread_sem;
    _Bool		is_thread_aborted;
    _Bool		is_sysconf;
    vlx_thread_t	thread_desc;
    struct proc_dir_entry* proc;
    vmq_links_t*	links;
    _Bool		changes;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	/* The below are for the generic drivers/block/ll_rw_block.c code */
    vbd_onemajor_t	ide;
    vbd_onemajor_t	scsi;
    vbd_onemajor_t	vbd;
    vbd_onemajor_t	fd;
#ifdef MMC_BLOCK_MAJOR
    vbd_onemajor_t	mmc;
#endif

#ifdef VBD_DEBUG_STATS
    struct {
	int sectors_per_buffer_head [8];
	int new_bh_same_page [2];
	int len_histo [1024];  /* Histogram of bhs, segments or sectors */
	int was_already_pending;
	int was_not_not_yet_pending;
	int was_pending_at_irq;
	int not_pending_at_irq;
	int ring_not_full;
    } stats;
#endif
#endif
};

/*----- Functions -----*/

#define vbd_kfree_and_clear(ptr) \
	do {kfree (ptr); (ptr) = NULL;} while (0)

#define	VBD_LINK_MAX_DEVIDS_PER_PROBE(vbd) \
    (((vbd)->msg_max - sizeof (vbd2_req_header_t)) / sizeof (vbd2_probe_t))

    /* Only called from vbd_link_init_device() <- vbd_link_acquire_disks() */

    static vbd_major_t*
vbd_fe_get_major (vbd_fe_t* fe, const vbd2_devid_t xd_devid, int* const minor)
{
    const int		xd_major = VBD2_DEVID_MAJOR (xd_devid);
    const int		xd_minor = VBD2_DEVID_MINOR (xd_devid);
    int			major_idx, new_major;
    vbd_major_t*	major;

    *minor = xd_minor;
    switch (xd_major) {
    case IDE0_MAJOR: major_idx = 0; new_major = IDE0_MAJOR; break;
    case IDE1_MAJOR: major_idx = 1; new_major = IDE1_MAJOR; break;
    case IDE2_MAJOR: major_idx = 2; new_major = IDE2_MAJOR; break;
    case IDE3_MAJOR: major_idx = 3; new_major = IDE3_MAJOR; break;
    case IDE4_MAJOR: major_idx = 4; new_major = IDE4_MAJOR; break;
    case IDE5_MAJOR: major_idx = 5; new_major = IDE5_MAJOR; break;
    case IDE6_MAJOR: major_idx = 6; new_major = IDE6_MAJOR; break;
    case IDE7_MAJOR: major_idx = 7; new_major = IDE7_MAJOR; break;
    case IDE8_MAJOR: major_idx = 8; new_major = IDE8_MAJOR; break;
    case IDE9_MAJOR: major_idx = 9; new_major = IDE9_MAJOR; break;
    case SCSI_DISK0_MAJOR: major_idx = 10; new_major = SCSI_DISK0_MAJOR; break;
    case SCSI_DISK1_MAJOR ... SCSI_DISK7_MAJOR:
	major_idx = 11 + xd_major - SCSI_DISK1_MAJOR;
	new_major = SCSI_DISK1_MAJOR + xd_major - SCSI_DISK1_MAJOR;
	break;
    case SCSI_CDROM_MAJOR: major_idx = 18; new_major = SCSI_CDROM_MAJOR;
	break;
    case FLOPPY_MAJOR: major_idx = 19; new_major = FLOPPY_MAJOR; break;
#ifdef MMC_BLOCK_MAJOR
    case MMC_BLOCK_MAJOR: major_idx = 20; new_major = MMC_BLOCK_MAJOR; break;
    default: major_idx = 21; new_major = 0; break;
#else
    default: major_idx = 20; new_major = 0; break;
#endif
    }
    if (fe->majors [major_idx]) {
	return fe->majors [major_idx];
    }
    major = fe->majors [major_idx] = kzalloc (sizeof *major, GFP_KERNEL);
    if (!major) {
	ETRACE ("out of memory for major descriptor\n");
	return NULL;
    }
    switch (major_idx) {
    case 0 ... VBD_NUM_IDE_MAJORS - 1:
	major->type = &vbd_type_ide;
	major->index = major_idx;
	break;

    case VBD_NUM_IDE_MAJORS ...  VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS - 1:
	major->type = &vbd_type_scsi;
	major->index = major_idx - VBD_NUM_IDE_MAJORS;
	break;

    case VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS ...
	    VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS + VBD_NUM_FD_MAJORS - 1:
	major->type = &vbd_type_fd;
	major->index = major_idx - (VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS);
	break;

#ifdef MMC_BLOCK_MAJOR
    case VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS + VBD_NUM_FD_MAJORS ...
	    VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS + VBD_NUM_FD_MAJORS +
	    VBD_NUM_MMC_MAJORS - 1:
	major->type = &vbd_type_mmc;
	major->index = major_idx -
		(VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS + VBD_NUM_FD_MAJORS);
	break;
#endif
    case VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS + VBD_NUM_FD_MAJORS +
	    VBD_NUM_MMC_MAJORS ...
	    VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS + VBD_NUM_FD_MAJORS +
	    VBD_NUM_MMC_MAJORS + VBD_NUM_VBD_MAJORS - 1:
	major->type = &vbd_type_vbd;
	major->index = major_idx -
		(VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS +
		 VBD_NUM_FD_MAJORS + VBD_NUM_MMC_MAJORS);
	break;
    }
    major->major = new_major;
    major->major_idx = major_idx;

    if (register_blkdev (major->major, major->type->name
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
			 , &vbd_block_fops
#endif
			 )) {
	ETRACE ("Cannot get major %d with name %s\n",
		major->major, major->type->name);
	goto out;
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    devfs_mk_dir (major->type->name);
#endif
#endif
    return major;

out:
    vbd_kfree_and_clear (fe->majors [major_idx]);
    return NULL;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static void vbd_rq_do_blkif_request_26 (struct request_queue*);

    static struct gendisk*
vbd_link_get_gendisk_2x (vbd_link_t* const vbd, vbd_major_t* const major,
			 const int xd_minor,
			 const vbd2_probe_t* const disk_probe)
{
    struct gendisk*	gd;
    vbd_disk_t*		di;
    int			part;

    di = kzalloc (sizeof *di, GFP_KERNEL);
    if (!di) {
	ETRACE ("out of memory for disk descriptor\n");
	return NULL;
    }
	/* Construct an appropriate gendisk structure */
    part = xd_minor & ((1 << major->type->partn_shift) - 1);
    if (part) {
	    /* VBD partitions are not expected to contain sub-partitions */
	DTRACE ("xd_minor %d -> alloc_disk(%d)\n", xd_minor, 1);
	gd = alloc_disk (1);
    } else {
	    /* VBD disks can contain sub-partitions */
	DTRACE ("xd_minor %d -> alloc_disk(%d)\n",
		xd_minor, 1 << major->type->partn_shift);
	gd = alloc_disk (1 << major->type->partn_shift);
    }
    if (!gd) {
	ETRACE ("could not alloc disk\n");
	goto out;
    }
    gd->major        = major->major;
    gd->first_minor  = xd_minor;
    gd->fops         = &vbd_block_fops;
    gd->private_data = di;
    if (part) {
#ifdef MMC_BLOCK_MAJOR
	if (major->major == MMC_BLOCK_MAJOR) {
	    snprintf (gd->disk_name, sizeof gd->disk_name, "mmcblk%dp%d",
		      xd_minor >> major->type->partn_shift, part);
	} else
#endif
	{
	    snprintf (gd->disk_name, sizeof gd->disk_name,
		      "%s%c%d", major->type->name,
		      'a' + (major->index << 1) +
		      (xd_minor >> major->type->partn_shift), part);
	}
    } else {
	    /* Floppy disk special naming rules */
	if (!strcmp (major->type->name, "fd")) {
	    snprintf (gd->disk_name, sizeof gd->disk_name,
		      "%s%c", major->type->name, '0');
#ifdef MMC_BLOCK_MAJOR
	} else if (major->major == MMC_BLOCK_MAJOR) {
	    snprintf (gd->disk_name, sizeof gd->disk_name, "mmcblk%d",
		      xd_minor >> major->type->partn_shift);
#endif
	} else {
	    snprintf (gd->disk_name, sizeof gd->disk_name, "%s%c",
		      major->type->name, 'a' + (major->index << 1) +
		      (xd_minor >> major->type->partn_shift));
	}
    }
    if (disk_probe->sectors) {
	set_capacity (gd, disk_probe->sectors);
    } else {
	set_capacity (gd, 0xffffffffL);
    }
    DTRACE ("%s: gd %p di %p major %d first minor %d\n",
	    gd->disk_name, gd, di, gd->major, gd->first_minor);
    gd->queue = blk_init_queue (vbd_rq_do_blkif_request_26, &vbd->io_lock);
    if (!gd->queue) goto out;

    gd->queue->queuedata = vbd;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
    elevator_init (gd->queue, "noop");
#else
    elevator_init (gd->queue, &elevator_noop);
#endif
	/*
	 * Turn off 'headactive' mode. We dequeue buffer
	 * heads as soon as we pass them to the back-end driver.
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
    blk_queue_headactive (gd->queue, 0);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
    blk_queue_logical_block_size (gd->queue, 512);
#else
    blk_queue_hardsect_size (gd->queue, 512);
#endif

#if defined CONFIG_ARM && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
	/* Limit max hw read size to 128 (255 loopback limitation) */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
    blk_queue_max_sectors (gd->queue, 128);
#else
    blk_queue_max_hw_sectors (gd->queue, 128);
#endif
#else
    blk_queue_max_sectors (gd->queue, vbd->segs_per_req_max * (PAGE_SIZE/512));
#endif

    blk_queue_segment_boundary (gd->queue, PAGE_SIZE - 1);
    blk_queue_max_segment_size (gd->queue, PAGE_SIZE);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
    blk_queue_max_phys_segments (gd->queue, vbd->segs_per_req_max);
    blk_queue_max_hw_segments (gd->queue, vbd->segs_per_req_max);
#else
    blk_queue_max_segments (gd->queue, vbd->segs_per_req_max);
#endif

    blk_queue_dma_alignment (gd->queue, 511);

    vbd_disk_init (di, disk_probe->devid, disk_probe->genid, major, gd,
		   MKDEV (major->major, xd_minor), vbd);
    add_disk (gd);
    return gd;

out:
    if (gd) del_gendisk (gd);	/* void */
    kfree (di);
    return NULL;
}
#else	/* 2.4.x */

    /*
     *  Private gendisk->flags[] values.
     *  Linux 2.4 only defines GENHD_FL_REMOVABLE (1).
     *  More recent Linuxes define other flags too, which
     *  could clash with our private flags, except we do
     *  not use private flags on 2.6: we just use the
     *  official GENHD_FL_CD (8) in 2.6 specific code.
     */
#define GENHD_FL_VIRT_PARTNS	4	/* Are unit partitions virtual? */

#define VBD_FD_DISK_MAJOR(M) ((M) == FLOPPY_MAJOR)
#ifdef MMC_BLOCK_MAJOR
#define VBD_MMC_DISK_MAJOR(M) ((M) == MMC_BLOCK_MAJOR)
#endif

static void vbd_rq_do_blkif_request_24 (struct request_queue*);

    /* Linux 2.4 only */

    static struct gendisk*
vbd_link_get_gendisk_2x (vbd_link_t* const vbd, vbd_major_t* const major,
			 const int xd_minor,
			 const vbd2_probe_t* const disk_probe)
{
    vbd_fe_t*		fe = vbd->fe;
    const int		fe_major  = VBD2_DEVID_MAJOR (disk_probe->devid);
    const int		minor  = VBD2_DEVID_MINOR (disk_probe->devid);
    const int		device = MKDEV (fe_major, minor);
    const _Bool		is_ide = IDE_DISK_MAJOR (fe_major);
    const _Bool		is_scsi= SCSI_BLK_MAJOR (fe_major);
    const _Bool		is_fd  = VBD_FD_DISK_MAJOR (fe_major);
#ifdef MMC_BLOCK_MAJOR
    const _Bool		is_mmc  = VBD_MMC_DISK_MAJOR (fe_major);
#endif
    struct gendisk*	gd;
    vbd_disk_t*		di;
    unsigned long	capacity;
    int			partno;
    int			max_part;
    unsigned char	buf [64];

    DTRACE ("dev 0x%x\n", device);
    max_part = 1 << major->type->partn_shift;

	/* Construct an appropriate gendisk structure */
    if ((gd = get_gendisk (device)) == NULL) {
	gd = kzalloc (sizeof *gd, GFP_KERNEL);
	DTRACE ("gd %p\n", gd);
	if (!gd) {
	    ETRACE ("could not alloc memory for gendisk\n");
	    goto out;
	}
	gd->major       = major->major;
	    /*
	     * major_name is used by devfs as a directory to automatically
	     * create partition devices (and discs/disc? symlink).
	     */
	gd->major_name  = major->type->name;
	gd->max_p       = max_part;
	gd->minor_shift = major->type->partn_shift;
	gd->nr_real     = major->type->devs_per_major;
	gd->next        = NULL;
	gd->fops        = &vbd_block_fops;
	    /*
	     * The sizes[] and part[] arrays hold the sizes and other
	     * information about every partition with this 'major' (i.e.
	     * every disk sharing the 8 bit prefix * max partns per disk).
	     */
#ifdef VBD_MV_CGE_31
	gd->sizes = kzalloc (256 * sizeof (int), GFP_KERNEL);
#else
	gd->sizes = kzalloc (max_part * gd->nr_real * sizeof (int), GFP_KERNEL);
#endif
	if (!gd->sizes) goto out;

#ifdef VBD_MV_CGE_31
	gd->part  = kzalloc (256 * sizeof (struct hd_struct), GFP_KERNEL);
#else
	gd->part  = kzalloc (max_part * gd->nr_real * sizeof (struct hd_struct),
			     GFP_KERNEL);
#endif
	if (!gd->part) goto out;
	    /* VBD private data (disk info) */
	gd->real_devices = kzalloc (max_part * gd->nr_real *
				    sizeof (vbd_disk_t), GFP_KERNEL);
	if (!gd->real_devices) goto out;

	gd->flags = kzalloc (gd->nr_real * sizeof *gd->flags, GFP_KERNEL);
	if (!gd->flags) goto out;

#ifdef VBD_MV_CGE_31
	{
	    int unit;

	    gd->kobj = kzalloc (256 * sizeof (struct kobject), GFP_ATOMIC);
	    if (!gd->kobj) goto out;

	    for (unit = 0; unit < gd->nr_real; ++unit) {
		snprintf (gd->kobj [unit].name, sizeof gd->kobj [unit].name,
			  "%s%c", major->type->name,
			  'a' + (major->index * gd->nr_real) +
			  (xd_minor >> major->type->partn_shift));
	    }
	}
#endif
	if (is_ide) {
	    blksize_size  [fe_major] = fe->ide.blksize_size;
	    hardsect_size [fe_major] = fe->ide.hardsect_size;
	    max_sectors   [fe_major] = fe->ide.max_sectors;
	    read_ahead    [fe_major] = 8; /* From drivers/ide/ide-probe.c */
	} else if (is_scsi) {
	    blksize_size  [fe_major] = fe->scsi.blksize_size;
	    hardsect_size [fe_major] = fe->scsi.hardsect_size;
	    max_sectors   [fe_major] = fe->scsi.max_sectors;
	    read_ahead    [fe_major] = 0;
	} else if (is_fd) {
	    blksize_size  [fe_major] = fe->fd.blksize_size;
	    hardsect_size [fe_major] = fe->fd.hardsect_size;
	    max_sectors   [fe_major] = fe->fd.max_sectors;
	    read_ahead    [fe_major] = 0;
#ifdef MMC_BLOCK_MAJOR
	} else if (is_mmc) {
	    blksize_size  [fe_major] = fe->mmc.blksize_size;
	    hardsect_size [fe_major] = fe->mmc.hardsect_size;
	    max_sectors   [fe_major] = fe->mmc.max_sectors;
	    read_ahead    [fe_major] = 0;
#endif
	} else {
	    blksize_size  [fe_major] = fe->vbd.blksize_size;
	    hardsect_size [fe_major] = fe->vbd.hardsect_size;
	    max_sectors   [fe_major] = fe->vbd.max_sectors;
	    read_ahead    [fe_major] = 8;
	}
	{
		/* Make sure Linux does not make single-segment requests */
	    int minor2;

	    for (minor2 = 0; minor2 < 256; ++minor2) {
		    /* Maximum number of sectors per request */
		max_sectors [fe_major] [minor2] =
		    vbd->segs_per_req_max * (PAGE_SIZE/512);

#ifndef VBD_MV_CGE_31
		    /*
		     * On RHEL3, we can get a "too many segments in req (128)"
		     * panic on filesystems with 1KB blocks (BugId 4676706).
		     * 128 is vbd->segs_per_req_max. Observation shows
		     * that Linux uses a different memory page and a different
		     * buffer head for every block in the request, that there
		     * can be more buffer heads than 128, and that after
		     * coalescing same-page buffer heads here in the driver,
		     * we always get 129 pages, which is still greater than
		     * the backend's limit of 128, hence panic. We try to
		     * avoid this situation by limiting the transfer size to
		     * 128KB, which with 1KB blocks can result in maximum
		     * 128 chunks. This is not ideal, since the problem can
		     * reappear if the filesystem uses 512-byte blocks, or if
		     * for some reason (I don't know if this is possible) an
		     * I/O request is performed as a long list of sector-sized
		     * buffer heads.
		     */
		max_sectors [fe_major] [minor2] /= (PAGE_SIZE/1024);
#endif
	    }
	    TRACE ("max_sectors for major %d is %d\n", fe_major,
		   max_sectors [fe_major][0]);
	}
	add_gendisk (gd);

	blk_size [fe_major] = gd->sizes;
	((struct request_queue*) BLK_DEFAULT_QUEUE (fe_major))->queuedata = vbd;
	blk_init_queue (BLK_DEFAULT_QUEUE (fe_major),
			vbd_rq_do_blkif_request_24);

#ifdef VBD_DEBUG_OBSOLETE
#ifndef VBD_MV_CGE_31
    {
	int new_max_segments = vbd->segs_per_req_max - 10;

	TRACE ("Changing max_segments for major %d from %d to %d\n", fe_major,
	       (BLK_DEFAULT_QUEUE (fe_major))->max_segments, new_max_segments);
	(BLK_DEFAULT_QUEUE (fe_major))->max_segments = new_max_segments;
    }
#endif
#endif
	    /*
	     * Turn off 'headactive' mode. We dequeue buffer
	     * heads as soon as we pass them to the back-end driver.
	     */
	blk_queue_headactive (BLK_DEFAULT_QUEUE (fe_major), 0);
    }
    partno = minor & (max_part-1);
	/* Private data */
    di = (vbd_disk_t*) gd->real_devices + minor;
    DTRACE ("disk %p major %p\n", di, major);

    vbd_disk_init (di, disk_probe->devid, disk_probe->genid, major, gd,
		   MKDEV (major->major, minor), vbd);
    di->gd_queue  = BLK_DEFAULT_QUEUE (fe_major);

    if (disk_probe->info & VBD2_FLAG_RO) {
	set_device_ro (device, 1);
    }
    if (disk_probe->sectors) {
	capacity = disk_probe->sectors;
    } else {
	capacity = 0xffffffff;
    }
    if (partno) {
	    /*
	     * If this was previously set up as a real disc we will have set
	     * up partition-table information. Virtual partitions override
	     * 'real' partitions, and the two cannot coexist on a device.
	     */
	if (!(gd->flags [minor >> gd->minor_shift] & GENHD_FL_VIRT_PARTNS) &&
	     (gd->sizes [minor & ~(max_part-1)] != 0)) {
		/*
		 * Any non-zero sub-partition entries must be cleaned out
		 * before installing 'virtual' partition entries. The two
		 * types cannot coexist, and virtual partitions are favored.
		 */
	    int i;
	    kdev_t dev = device & ~(max_part-1);

	    for (i = max_part - 1; i > 0; i--) {
		invalidate_device (dev + i, 1);
		gd->part [MINOR (dev + i)].start_sect = 0;
		gd->part [MINOR (dev + i)].nr_sects   = 0;
		gd->sizes [MINOR (dev + i)]           = 0;
	    }
	    WTRACE ("Virtual partitions found for /dev/%s - ignoring any "
		    "real partition information we may have found.\n",
		    disk_name (gd, MINOR (device), buf));
	}
	    /* Need to skankily setup 'partition' information */
	gd->part [minor].start_sect = 0;
	gd->part [minor].nr_sects   = capacity;
	gd->sizes [minor]           = capacity >> (BLOCK_SIZE_BITS-9);
	gd->flags [minor >> gd->minor_shift] |= GENHD_FL_VIRT_PARTNS;
	DTRACE ("devfs_register(%s)\n", disk_name (gd, minor, buf));
	di->devfs_handle = devfs_register (NULL, disk_name (gd, minor, buf),
					   0, fe_major, minor,
					   S_IFBLK|S_IRUSR|S_IWUSR|S_IRGRP,
					   &vbd_block_fops, di);
    } else {
	vbd2_info_t	disk_probe_info = disk_probe->info;
	char		short_name [8];

	    /* Get disk short name. Must be done before register_disk() */
	if (is_fd) {
	    snprintf (short_name, sizeof short_name,
		      "%s%c", gd->major_name, '0' + minor);
	    disk_probe_info =
		(disk_probe_info & ~VBD2_TYPE_MASK) | VBD2_TYPE_FLOPPY;
	} else {
	    (void) disk_name (gd, minor, short_name);
	}
	DTRACE ("short_name %s\n", short_name);

	gd->part [minor].nr_sects = capacity;
	gd->sizes [minor] = capacity >> (BLOCK_SIZE_BITS-9);
	    /* Some final fix-ups depending on the device type */
	switch (VBD2_TYPE (disk_probe_info)) {
	case VBD2_TYPE_CDROM:
	case VBD2_TYPE_FLOPPY:
	case VBD2_TYPE_TAPE:
	    gd->flags [minor >> gd->minor_shift] |= GENHD_FL_REMOVABLE;
	    WTRACE ("Skipping partition check on %s /dev/%s\n",
		    VBD2_TYPE (disk_probe_info)==VBD2_TYPE_CDROM ? "cdrom" :
		   (VBD2_TYPE (disk_probe_info)==VBD2_TYPE_TAPE ? "tape" :
		    "floppy"), short_name);
	    break;

	case VBD2_TYPE_DISK:
		/* Only check partitions on real discs (not virtual) */
	    if (gd->flags [minor >> gd->minor_shift] & GENHD_FL_VIRT_PARTNS) {
		WTRACE ("Skipping partition check on virtual /dev/%s\n",
			disk_name (gd, MINOR (device), buf));
		break;
	    }
	    DTRACE ("register_disk: 0x%x %d %ld %d(%d %d %d %d)\n",
		    device,gd->max_p,capacity,
		    (major->index * gd->nr_real) + (minor >> gd->minor_shift),
		    major->index, gd->nr_real, minor, gd->minor_shift);

	    register_disk (gd, device, gd->max_p, &vbd_block_fops, capacity
#ifdef VBD_MV_CGE_31
		      ,(major->index * gd->nr_real) + (minor >>gd->minor_shift)
#endif
			  );
		/*
		 * register_disk() has checked for internal disk partitions
		 * and initialized gd->part[] accordingly.
		 * For each valid partition:
		 * - we duplicate the disk (di) structure created for the
		 *   disk (minor 0)
		 * - we update the copy to indicate it is a "partition"
		 *   in particular the copy (p_di) is not linked into the
		 *   disk list
		 *   Therefore, the next pointer is used to point to the
		 *   di of the
		 *   full disk. This pointer will be used to set the
		 *   'req_is_pending' field in the parent disk di when a
		 *   ring full condition is encountered (and not in the copy).
		 */
	    {
		int i;
		kdev_t dev = device & ~(max_part-1);
		char symlink [8];

		for (i = max_part - 1; i > 0; i--) {
		    vbd_disk_t*	p_di;
		    const int	p_min = MINOR (dev + i);

		    if (gd->part [p_min].nr_sects) {
			DTRACE ("part %s: di %d -> %d\n",
				disk_name (gd, p_min, buf), minor, p_min);
			p_di  = (vbd_disk_t*) gd->real_devices + p_min;
			*p_di = *di;
			p_di->device  = MKDEV (major->major, p_min);
			p_di->is_part = 1;  /* Set "partition" flag */
			p_di->next    = di; /* Point to parent disk di */
			    /*
			     * Add short name devices as symlink to
			     * the partition in devfs.
			     */
			snprintf (symlink, sizeof symlink,
				  "%s%d", short_name, p_min);
			DTRACE ("devfs_mk_symlink(%s -> %s)\n",
				symlink, disk_name (gd, p_min, buf));
			devfs_mk_symlink (NULL, symlink, DEVFS_FL_DEFAULT,
					  disk_name (gd, p_min, buf),
					  &p_di->devfs_handle, NULL);
		    }
		}
		DTRACE ("devfs_mk_symlink(%s -> %s)\n",
			short_name, disk_name (gd, minor, buf));
		devfs_mk_symlink (NULL, short_name, DEVFS_FL_DEFAULT,
				  disk_name (gd, minor, buf),
				  &di->devfs_handle, NULL);
	    }
	    break;

	default:
	    WTRACE ("unknown device type %d\n", VBD2_TYPE (disk_probe_info));
	    break;
	}
    }
    return gd;

out:
    if (gd) {
	del_gendisk (gd);	/* void */
	kfree (gd->sizes);
	kfree (gd->part);
	kfree (gd->real_devices);
	kfree (gd->flags);
#ifdef VBD_MV_CGE_31
	kfree (gd->kobj);
#endif
    }
    return NULL;
}

#endif	/* 2.4.x */

    /*
     * Initialize a VBD device.
     * Only from vbd_link_acquire_disks() <- vbd_cb_link_on()
     *
     * Takes a vbd2_probe_t* that describes a VBD the guest has access to.
     * Performs appropriate initialization and registration of the device.
     *
     * Care needs to be taken when making re-entrant calls to ensure that
     * corruption does not occur. Also, devices that are in use should not have
     * their details updated. This is the caller's responsibility.
     */
    static int
vbd_link_init_device (vbd_link_t* const vbd,
		      const vbd2_probe_t* const disk_probe)
{
    vbd_fe_t*		fe = vbd->fe;
    int			err = -ENOMEM;
    struct block_device* bdev;
    struct gendisk*	gd;
    vbd_major_t*	major;
    dev_t		device;
    int			minor;

    DTRACE ("entered\n");
    major = vbd_fe_get_major (fe, disk_probe->devid, &minor);
    if (!major) {
	return -EPERM;
    }
    device = MKDEV (major->major, minor);
    if ((bdev = bdget (device)) == NULL) {
	return -EPERM;
    }
	/*
	 * Update of partition info, and check of usage count,
	 * is protected by the per-block-device semaphore.
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    down (&bdev->bd_sem);
#endif
    gd = vbd_link_get_gendisk_2x (vbd, major, minor, disk_probe);
    if (!gd || !major) {
	err = -EPERM;
	goto out;
    }
    major->usage++;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if (disk_probe->info & VBD2_FLAG_RO) {
	set_disk_ro (gd, 1);
    }
	/* Some final fix-ups depending on the device type */
    switch (VBD2_TYPE (disk_probe->info)) {
    case VBD2_TYPE_CDROM:
	gd->flags |= GENHD_FL_REMOVABLE | GENHD_FL_CD;
	/* FALLTHROUGH */
    case VBD2_TYPE_FLOPPY:
    case VBD2_TYPE_TAPE:
	gd->flags |= GENHD_FL_REMOVABLE;
	break;

    case VBD2_TYPE_DISK:
#ifdef MMC_BLOCK_MAJOR
	if (major->major == MMC_BLOCK_MAJOR) {
	    DTRACE ("Marking MMC as removable\n");
	    gd->flags |= GENHD_FL_REMOVABLE;
	}
#endif
	break;

    default:
	ETRACE ("unknown device type %d\n", VBD2_TYPE (disk_probe->info));
	break;
    }
#endif
    if (fe->must_wait && fe->wait_devid == disk_probe->devid) {
	fe->must_wait = false;
    }
    err = 0;
out:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    up (&bdev->bd_sem);
#endif
    bdput (bdev);	/* void */
    return err;
}

    void
vbd_disk_del_gendisk (vbd_disk_t* const di)
{
    struct block_device* bdev = bdget (di->device);

    if (bdev) {
	if (di->gd) {
		/* gd->queue must still be valid here */
	    del_gendisk (di->gd);	/* void */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	    if (di->gd->queue) {
		blk_cleanup_queue (di->gd->queue);	/* void */
	    }
#else
	    if (di->gd_queue) {
		blk_cleanup_queue (di->gd_queue);	/* void */
	    }
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	    put_disk (di->gd);	/* kobject cleanup */
#endif
	    di->gd = NULL;	/* Lost the gendisk logically */
	} else {
	    DTRACE ("di->gd null for %x\n", di->device);
	}
	bdput (bdev);	/* void */
    } else {
	DTRACE ("bdget(%x) failed\n", di->device);
    }
}

    /* From vbd_link_delete_disks() <- vbd_link_free() <- vbd_exit() */
    /* From vbd_link_delete_disks() <- vbd_cb_link_off_completed() */

    static void
vbd_disk_destroy (vbd_disk_t* const di, vbd_link_t* vbd)
{
    vbd_major_t*	major = di->major;

	/* di->usage should be 0 here and major->usage 1 */
    DTRACE ("%s: disk-usage %d major-usage %d\n",
	    di->name, di->usage, major->usage);
    vbd_disk_del_gendisk (di);

    if (!--major->usage) {
	DTRACE ("unregistering major %d\n", major->major);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
	devfs_remove (major->type->name);
#endif
#else
	devfs_unregister (di->devfs_handle);
#endif
	unregister_blkdev (major->major, major->type->name);
	    /* kfree(major) is not enough, must also clear pointer in table */
	vbd_kfree_and_clear (vbd->fe->majors [major->major_idx]);
    }
#ifdef VBD_DEBUG
    memset (di, 0x77, sizeof *di);
#endif
    kfree (di);
}

#define VBD_DISK_ITERATE_NEXT	0
#define VBD_DISK_ITERATE_DELETE	1
#define VBD_DISK_ITERATE_STOP	2

    static void
vbd_link_disks_iterate (vbd_link_t* vbd, int (*func) (vbd_disk_t*, void*),
			void* cookie)
{
    vbd_disk_t* prev = NULL;
    vbd_disk_t* di = vbd->disks;

    while (di) {
	const int diag = func (di, cookie);

	if (diag & VBD_DISK_ITERATE_DELETE) {
	    vbd_disk_t* next;

	    if (prev) {
		prev->next = next = di->next;
	    } else {
		vbd->disks = next = di->next;
	    }
	    vbd_disk_destroy (di, vbd);	/* void */
	    di = next;	/* prev stays the same */
	} else {
	    prev = di;
	    di = di->next;
	}
	if (diag & VBD_DISK_ITERATE_STOP) break;
    }
}

typedef struct {
    const vbd2_probe_t*	probe;
    _Bool		have_it;
} vbd_disk_match_t;

    /*
     * Also deletes obsolete disks, if unused, so we
     * can immediately replace them with new ones.
     */

    static int
vbd_disk_match (vbd_disk_t* di, void* cookie)
{
    vbd_disk_match_t* m = cookie;

    if (di->xd_devid != m->probe->devid) return VBD_DISK_ITERATE_NEXT;
    if (di->xd_genid == m->probe->genid && !di->is_zombie) {
	di->still_valid = true;
	m->have_it = true;
	return VBD_DISK_ITERATE_STOP;
    }
    DTRACE ("%s: obsolete, disk-usage %d\n", di->name, di->usage);
    if (di->usage) {
	di->is_zombie = true;		/* Cannot delete it yet */
	vbd_disk_del_gendisk (di);
	m->have_it = true;		/* Do not add, rescan when disk gone */
	return VBD_DISK_ITERATE_STOP;
    }
	/* m->have_it remains as "false", ie. disk not found */
    return VBD_DISK_ITERATE_DELETE | VBD_DISK_ITERATE_STOP;
}

    static _Bool
vbd_link_find_disk (vbd_link_t* vbd, const vbd2_probe_t* const probe)
{
    vbd_disk_match_t match;

    match.probe   = probe;
    match.have_it = false;
    vbd_link_disks_iterate (vbd, vbd_disk_match, &match);
    return match.have_it;
}

    static int
vbd_disk_delete (vbd_disk_t* di, void* cookie)
{
    DTRACE ("%s: disk-usage %d major-usage %d\n",
	    di->name, di->usage, di->major->usage);
    if (di->usage) {
	DTRACE ("%s: still busy\n", di->name);
	di->is_zombie = true;
	vbd_disk_del_gendisk (di);
	return VBD_DISK_ITERATE_NEXT;
    }
    DTRACE ("%s: can delete\n", di->name);
    return VBD_DISK_ITERATE_DELETE;
}

    static int
vbd_disk_can_delete (vbd_disk_t* di, void* cookie)
{
    if (di->still_valid) return VBD_DISK_ITERATE_NEXT;
    return vbd_disk_delete (di, cookie);
}

    /* Called from vbd_cb_link_on() and vbd_link_changes() <- vbd_thread() */

    static void
vbd_link_acquire_disks (vbd_link_t* vbd)
{
    const unsigned	max_per_probe = VBD_LINK_MAX_DEVIDS_PER_PROBE (vbd);
    unsigned		offset = 0;
    int			count;

    DTRACE ("entered\n");
    {
	vbd_disk_t* di;

	VBD_LINK_FOR_ALL_DISKS (di, vbd) di->still_valid = false;
    }
    do {
	vbd2_probe_link_t*	pl;
	int			i, diag;

	diag = vmq_msg_allocate (vbd->link, 0 /*data_len*/, (void**) &pl,
				 NULL /*data_offset*/);
	if (diag) {
	    ETRACE ("failed to alloc msg for disk acquisition (%d)\n", diag);
	    ++vbd->errors;
	    break;
	}
	memset (pl, 0, sizeof *pl);
	pl->common.op     = VBD2_OP_PROBE;
	pl->common.count  = max_per_probe;
	pl->common.sector = offset;
	{
	    nku64_f* reply = vipc_ctx_call (&vbd->vipc_ctx, &pl->common.cookie);

	    if (!reply) {
		ETRACE ("disk acquisition failed\n");
		++vbd->errors;
		break;
	    }
	    pl = container_of (reply, vbd2_probe_link_t, common.cookie);
	}
	if (pl->common.count == VBD2_STATUS_ERROR) {
	    ETRACE ("Could not probe disks (%d)\n", pl->common.count);
	    vmq_return_msg_free (vbd->link, pl);
	    ++vbd->errors;
	    break;
	}
	count = pl->common.count;
	for (i = 0; i < count; i++) {
	    vbd2_probe_t* probe = pl->probe + i;

	    if (vbd_link_find_disk (vbd, probe)) {
		DTRACE ("already had (%d,%d:%d)\n",
			VBD2_DEVID_MAJOR (probe->devid),
			VBD2_DEVID_MINOR (probe->devid), probe->genid);
		continue;
	    }
	    diag = vbd_link_init_device (vbd, probe);
	    if (diag) {
		ETRACE ("device (%d,%d:%d) creation failure (%d)\n",
			VBD2_DEVID_MAJOR (probe->devid),
			VBD2_DEVID_MINOR (probe->devid), probe->genid, diag);
		++vbd->errors;
	    } else {
		TRACE ("device (%d,%d:%d) created, %lld sectors\n",
		       VBD2_DEVID_MAJOR (probe->devid),
		       VBD2_DEVID_MINOR (probe->devid), probe->genid,
		       probe->sectors);
	    }
	}
	offset += count;
	vmq_return_msg_free (vbd->link, pl);
    } while (count == max_per_probe);
    TRACE ("%d virtual disk(s) detected\n", offset);
    DTRACE ("deleting obsolete disks\n");
    vbd_link_disks_iterate (vbd, vbd_disk_can_delete, NULL);
}

    /* Called from: vbd_link_free() <- vbd_exit() */
    /* Also from: vbd_cb_link_off_completed() */

    static void
vbd_link_delete_disks (vbd_link_t* vbd)
{
    DTRACE ("\n");
    vbd_link_disks_iterate (vbd, vbd_disk_delete, NULL);
}

    static void
vbd_update (void)
{
    XTRACE ("entered\n");
}

static const char vbd_op_names[VBD2_OP_MAX][13] = {VBD2_OP_NAMES};

    static int
vbd_disk_open_release (vbd_disk_t* di, const vbd2_op_t op)
{
    vbd_link_t* vbd = di->vbd;
    vbd2_req_header_t* rreq;
    nku64_f* reply;
    int diag;

    diag = vmq_msg_allocate (vbd->link, 0 /*data_len*/, (void**) &rreq,
			     NULL /*data_offset*/);
    if (diag) {
	ETRACE ("%s: %s failed to alloc msg (%d)\n", di->name,
		vbd_op_names [op], diag);
	++vbd->errors;
	return diag;
    }
    memset (rreq, 0, sizeof *rreq);
    rreq->op    = op;
    rreq->devid = di->xd_devid;
    rreq->genid = di->xd_genid;

    reply = vipc_ctx_call (&vbd->vipc_ctx, &rreq->cookie);
    if (!reply) {
	ETRACE ("%s: %s IPC call failed\n", di->name, vbd_op_names [op]);
	++vbd->errors;
	return -ESTALE;
    }
    rreq = container_of (reply, vbd2_resp_t, cookie);
    if (rreq->count) {	/* status */
	ETRACE ("%s: %s failed (%d)\n", di->name,
		vbd_op_names [op], rreq->count);
	vmq_return_msg_free (vbd->link, rreq);
	++vbd->errors;
	return -EINVAL;
    }
    vmq_return_msg_free (vbd->link, rreq);
    return 0;
}

    /* Called from Linux thru vbd_disk_open() and vbd_disk_release() */

    static int
vbd_disk_drop (vbd_disk_t* di, int diag)
{
    DTRACE ("%s: disk-usage %d major-usage %d\n",
	    di->name, di->usage, di->major->usage);
    if (!--di->usage && di->is_zombie) {
	vbd_fe_t* fe = di->vbd->fe;

	DTRACE ("awakening thread\n");
	di->vbd->changes = true;
	fe->changes = true;
	up (&fe->thread_sem);
    }
    if (!--di->major->usage) {
	vbd_update();
    }
    return diag;
}

    static int
vbd_disk_open (vbd_disk_t* di)
{
	/* Update of usage count is protected by per-device semaphore */
    di->major->usage++;
    di->usage++;
    if (di->usage == 1) {
	int diag = vbd_disk_open_release (di, VBD2_OP_OPEN);
	if (diag) return vbd_disk_drop (di, diag);
    }
    DTRACE ("%s: disk-usage %d major-usage %d\n",
	    di->name, di->usage, di->major->usage);
    return 0;
}

    static int
vbd_disk_release (vbd_disk_t* di)
{
    DTRACE ("%s: disk-usage %d major-usage %d\n",
	    di->name, di->usage, di->major->usage);
	/*
	 * When usage drops to zero it may allow more VBD updates to occur.
	 * Update of usage count is protected by a per-device semaphore.
	 */
    if (di->usage == 1) {
	int diag = vbd_disk_open_release (di, VBD2_OP_CLOSE);
	if (diag) {
	    WTRACE ("%s: ignoring error, releasing disk.\n", di->name);
	}
    }
    return vbd_disk_drop (di, 0);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)

    /*
     * "kick" means "resume" here.
     * Only called from vbd_link_kick_pending_request_queues_2x()
     */

    static inline void
vbd_rq_kick_pending_26 (struct request_queue* rq)
{
    if (rq && test_bit (QUEUE_FLAG_STOPPED, &rq->queue_flags)) {
	blk_start_queue (rq);
	rq->request_fn (rq);
    }
}

    /* Called from vbd_link_return_msg(), under vbd->io_lock */

    static void
vbd_link_kick_pending_request_queues_2x (vbd_link_t* vbd)
{
    vbd_disk_t* di;

    VBD_LINK_FOR_ALL_DISKS (di, vbd) {
	if (di->gd) {
	    vbd_rq_kick_pending_26 (di->gd->queue);
	}
    }
}

    static int
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
vbd_blkif_open_2x (struct block_device* bdev, fmode_t fmode)
#else
vbd_blkif_open_2x (struct inode* inode, struct file* filep)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    struct gendisk* gd = bdev->bd_disk;
#else
    struct gendisk* gd = inode->i_bdev->bd_disk;
#endif
    vbd_disk_t* di = (vbd_disk_t*) gd->private_data;

    DTRACE ("%s\n", di->name);
    return vbd_disk_open (di);
}

    static int
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
vbd_blkif_release_2x (struct gendisk* gd, fmode_t fmode)
#else
vbd_blkif_release_2x (struct inode* inode, struct file* filep)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
    struct gendisk* gd = inode->i_bdev->bd_disk;
#endif
    vbd_disk_t* di = (vbd_disk_t*) gd->private_data;

    DTRACE ("%s\n", di->name);
    return vbd_disk_release (di);
}

#else	/* 2.4.x kernel versions */

    /*
     * Linux 2.4 only.
     * "kick" means "resume" here.
     * Called from vbd_link_return_msg(), under vbd->io_lock.
     */

    static void
vbd_link_kick_pending_request_queues_2x (vbd_link_t* vbd)
{
#ifdef VBD_DEBUG_STATS
    vbd_fe_t*	fe = vbd->fe;
#endif
#ifdef VBD_DEBUG_NOISY
    static long next_jiffies;
#endif
    vbd_disk_t* di;

    VBD_LINK_FOR_ALL_DISKS (di, vbd) {
	    /*
	     *  req_is_pending was set in previous vbd_rq_do_blkif_request_2...
	     *  and means that FIFO was full.
	     *  "di" can only be a real disk here, or maybe a standalone
	     *  partition. Indeed, di's for partitions are not put on the
	     *  vbd->disks list if they have been found by Linux.
	     */
	VBD_CATCHIF (!di->gd,       WTRACE ("<!di->gd>"));
	VBD_CATCHIF (!di->gd_queue, WTRACE ("<!di->gd_queue>"));

#ifdef VBD_DEBUG_STATS
	if (MAJOR (di->device) == 8) {
	    if (di->req_is_pending) {
		++fe->stats.was_pending_at_irq;
	    } else {
		++fe->stats.not_pending_at_irq;
	    }
	}
#endif

#ifdef VBD_DEBUG_NOISY
	if (MAJOR (di->device) == 8 && jiffies > next_jiffies) {
	    const int major = MAJOR (di->device);
	    const struct request_queue* q = BLK_DEFAULT_QUEUE (major);

	    WTRACE ("%s: di %p dev %s pend %d\n", __FUNCTION__, di,
		    di->name, di->req_is_pending);
	    VBD_ASSERT (q->queuedata == vbd);

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,4,18)
	    printk ("queue %d: plugged %d head_active %d\n", major,
		    q->plugged, q->head_active);
#else
	    printk ("queue %d: nr_requests %d batch_requests %d plugged %d "
		    "head_active %d\n", major, q->nr_requests,
		    q->batch_requests, q->plugged, q->head_active);
#endif

#ifdef VBD_MV_CGE_31
	    printk ("queue %d: nr_sectors %d batch_sectors %d "
		    "max_queue_sectors %d full %d can_throttle %d "
		    "low_latency %d\n", major, atomic_read (&q->nr_sectors),
		    q->batch_sectors, q->max_queue_sectors, q->full,
		    q->can_throttle, q->low_latency);
#endif
	    next_jiffies = jiffies + 10 * HZ;
#ifdef VBD_DEBUG_STATS
	    printk ("AlrPend %d notyet %d pend@irq %d not %d !full %d\n",
		    fe->stats.was_already_pending,
		    fe->stats.was_not_not_yet_pending,
		    fe->stats.was_pending_at_irq,
		    fe->stats.not_pending_at_irq,
		    fe->stats.ring_not_full);
	    printk ("Sectors per buf/head: %d %d %d %d  %d %d %d %d ",
		    fe->stats.sectors_per_buffer_head [0],
		    fe->stats.sectors_per_buffer_head [1],
		    fe->stats.sectors_per_buffer_head [2],
		    fe->stats.sectors_per_buffer_head [3],
		    fe->stats.sectors_per_buffer_head [4],
		    fe->stats.sectors_per_buffer_head [5],
		    fe->stats.sectors_per_buffer_head [6],
		    fe->stats.sectors_per_buffer_head [7]);
	    printk (" SamePage %d %d\n",
		    fe->stats.new_bh_same_page [0],
		    fe->stats.new_bh_same_page [1]);
	    {
		int i;

		for (i = 0; i < VBD_ARRAY_ELEMS (fe->stats.len_histo);
		     i += 16) {
		    printk ("len_histo(%3d): %d %d %d %d  %d %d %d %d  "
			    "%d %d %d %d  %d %d %d %d\n", i,
			fe->stats.len_histo [i+ 0], fe->stats.len_histo [i+ 1],
			fe->stats.len_histo [i+ 2], fe->stats.len_histo [i+ 3],
			fe->stats.len_histo [i+ 4], fe->stats.len_histo [i+ 5],
			fe->stats.len_histo [i+ 6], fe->stats.len_histo [i+ 7],
			fe->stats.len_histo [i+ 8], fe->stats.len_histo [i+ 9],
			fe->stats.len_histo [i+10], fe->stats.len_histo [i+11],
			fe->stats.len_histo [i+12], fe->stats.len_histo [i+13],
			fe->stats.len_histo [i+14], fe->stats.len_histo [i+15]);
		}
	    }
#endif /* VBD_DEBUG_STATS */
	    {
		static _Bool is_already_done;
		int minor;

		if (!is_already_done) {
		    is_already_done = 1;

		    printk ("blk_size:");
		    for (minor = 0; minor < 256 && blk_size [8]; ++minor) {
			printk (" %d", blk_size [8] [minor]);
		    }
		    printk ("\n");

		    printk ("blksize_size:");
		    for (minor = 0; minor < 256 && blksize_size [8]; ++minor) {
			printk (" %d", blksize_size [8] [minor]);
		    }
		    printk ("\n");

		    printk ("hardsect_size:");
		    for (minor = 0; minor < 256 && hardsect_size [8]; ++minor) {
			printk (" %d", hardsect_size [8] [minor]);
		    }
		    printk ("\n");

		    printk ("max_readahead:");
		    for (minor = 0; minor < 256 && max_readahead [8]; ++minor) {
			printk (" %d", max_readahead [8] [minor]);
		    }
		    printk ("\n");

		    printk ("max_sectors:");
		    for (minor = 0; minor < 256 && max_sectors [8]; ++minor) {
			printk (" %d", max_sectors [8] [minor]);
		    }
		    printk ("\n");
		}
	    }
	}
#endif /* VBD_DEBUG_NOISY */
	if (di->gd && di->req_is_pending && di->gd_queue) {
	    DTRACE ("di %p\n", di);
	    di->req_is_pending = 0;
	    vbd_rq_do_blkif_request_24 (di->gd_queue);
	}
    }
}

    /* Linux 2.4 only */

    static int
vbd_blkif_open_2x (struct inode* inode, struct file* filep)
{
    struct gendisk*	gd = get_gendisk (inode->i_rdev);
    vbd_disk_t*		di;

    (void) filep;
    DTRACE ("i_rdev %x gd %p\n", inode->i_rdev, gd);
    VBD_ASSERT (gd);
    di = (vbd_disk_t*) gd->real_devices + MINOR (inode->i_rdev);
    VBD_ASSERT (di);
	/* We get a NULL here on RHEL3 when non-existing minor is opened */
    if (!di->major) {
	WTRACE ("Open of non-existing device %x\n", inode->i_rdev);
	return -ENODEV;
    }
    return vbd_disk_open (di);
}

    /* Linux 2.4 only */

    static int
vbd_blkif_release_2x (struct inode* inode, struct file* filep)
{
    struct gendisk*	gd = get_gendisk (inode->i_rdev);

    (void) filep;
    DTRACE ("gd %p\n", gd);
    VBD_ASSERT (gd);
    return vbd_disk_release
	((vbd_disk_t*) gd->real_devices + MINOR (inode->i_rdev));
}
#endif	/* 2.4.x */

    static int
vbd_blkif_ioctl (
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
		 struct block_device* bdev, fmode_t fmode,
#else
		 struct inode* inode, struct file* filep,
#endif
		 unsigned command, unsigned long argument)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    const struct gendisk*	gd    = bdev->bd_disk;
    const dev_t			dev   = bdev->bd_inode->i_rdev;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    const struct gendisk*	gd    = inode->i_bdev->bd_disk;
    const dev_t			dev   = inode->i_rdev;
#else
    const kdev_t		dev   = inode->i_rdev;
    const struct gendisk*	gd    = get_gendisk (dev);
    const int			minor = MINOR (dev);
    const struct hd_struct*	part  = &gd->part [minor];
#endif

    (void) dev;
    DTRACE ("command 0x%x argument 0x%lx dev 0x%04x\n",
	    command, (long) argument, dev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    (void) fmode;
#else
    (void) filep;
#endif

    switch (command) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	/* This is used by "fdisk -l" */
    case BLKGETSIZE:
	return put_user (part->nr_sects, (unsigned long*) argument);

	/* This is not used by "fdisk -l" */
    case BLKGETSIZE64:
	return put_user ((u64) part->nr_sects * 512, (u64*) argument);

#if 0
	/*
	 * This ioctl is not used by fdisk -l. Before activating this
	 * code, we need to bring blkif_revalidate() from an older
	 * version of this driver.
	 */
    case BLKRRPART: /* Re-read partition table */
	return blkif_revalidate (dev);
#endif

    case BLKROGET:	/* Used by "hdparm /dev/sda" */
    case BLKFLSBUF:	/* Used by "hdparm -f/-t /dev/sda" */
	    /* We could use this for most of ioctls here */
	return blk_ioctl (dev, command, argument);

	/* This is used by "fdisk -l" */
    case BLKSSZGET:
	    /*
	     * This value is 0 for now, because of still missing
	     * initialization code. This is fixed by routine.
	     */
	return blk_ioctl (dev, command, argument);

	/* These 4 ioctls are not used by "fdisk -l" */
    case BLKBSZGET: /* Get block size */
	return blk_ioctl (dev, command, argument);

    case BLKBSZSET: /* Set block size */
	break;

    case BLKRASET: /* Set read-ahead */
	break;

    case BLKRAGET: /* Get read-ahead */
	    /* Used by "hdparm -a /dev/sda" */
	return blk_ioctl (dev, command, argument);

    case HDIO_GETGEO:
    case HDIO_GETGEO_BIG: {
	struct hd_geometry *geo = (struct hd_geometry*) argument;
	const u8 heads = 0xff;
	const u8 sectors = 0x3f;
	const unsigned long start_sect = gd->part [minor].start_sect;
	unsigned cylinders;

	    /* This code is never reached on 2.6.15 */
	    /* This is used by "fdisk -l" and by hdparm */
	if (!argument) return -EINVAL;
	    /*
	     * We don't have real geometry info, but let's at least return
	     * values consistent with the size of the device
	     * We take the start_sect from the partition, but the size from
	     * the whole disk, except if it is zero, so this is a virtual
	     * partition, in which case we take its size.
	     */
	part = &gd->part [minor & ~((1 << gd->minor_shift)-1)];
	if (!part->nr_sects) {
	    part = &gd->part [minor];
	}
	cylinders = part->nr_sects / (heads * sectors);

	if (put_user (heads, &geo->heads)) return -EFAULT;
	if (put_user (sectors, &geo->sectors)) return -EFAULT;
	if (command == HDIO_GETGEO) {
	    if (put_user (cylinders, &geo->cylinders)) return -EFAULT;
	    if (put_user (start_sect, &geo->start)) return -EFAULT;
	} else {
	    struct hd_big_geometry* geobig = (struct hd_big_geometry*) argument;
	    if (put_user (cylinders, &geobig->cylinders)) return -EFAULT;
	    if (put_user (start_sect, &geobig->start)) return -EFAULT;
	}
	break;
    }
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) */

    case LOOP_CLR_FD:
	    /* We get this at ext2/ext3 umount time */
	DTRACE ("LOOP_CLR_FD not supported\n");
	return -ENOSYS;

    case FDEJECT:
    case CDROMEJECT: {
	const char*		name = command == FDEJECT ?
					"FDEJECT" : "CDROMEJECT";
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	vbd_disk_t*		di   = gd->private_data;
#else
	vbd_disk_t*		di = (vbd_disk_t*) gd->real_devices +
					MINOR (dev);
#endif
	vbd_link_t*		vbd  = di->vbd;
	vbd2_probe_link_t*	rreq;
	nku64_f*		reply;
	int			diag;

	diag = vmq_msg_allocate (vbd->link, 0 /*data_len*/, (void**) &rreq,
				 NULL /*data_offset*/);
	if (diag) {
	    ETRACE ("%s: %s failed to alloc msg (%d)\n", di->name, name, diag);
	    return diag;
	}
	memset (rreq, 0, sizeof *rreq);
	rreq->common.op     = VBD2_OP_MEDIA_CONTROL;
	rreq->common.devid  = di->xd_devid;
	rreq->common.genid  = di->xd_genid;
	rreq->common.count  = 1;
	rreq->common.sector = VBD2_FLAG_LOEJ;	/* flags */

	reply = vipc_ctx_call (&vbd->vipc_ctx, &rreq->common.cookie);
	if (!reply) {
	    ETRACE ("%s: %s IPC call failed\n", di->name, name);
	    return -ENOMEM;
	}
	rreq = container_of (reply, vbd2_probe_link_t, common.cookie);
	if (rreq->common.count) {	/* status */
	    ETRACE ("%s: %s request failed %x (%d)\n", di->name, name,
		    rreq->probe[0].info, rreq->common.count);
	    vmq_return_msg_free (vbd->link, rreq);
	    return -ENOSYS;
	}
	DTRACE ("%s: %s OK flags %x\n", di->name, name, rreq->probe[0].info);
	vmq_return_msg_free (vbd->link, rreq);
	break;
    }
    default:
	ETRACE ("ioctl 0x%x not supported\n", command);
	return -ENOSYS;
    }
    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
    /*
     *  Implement the HDIO_GETGEO ioctl: the HDIO_GETGEO
     *  value is not passed to the common vbd_blkif_ioctl() above
     *  on 2.6.15 and maybe other versions.
     *  We always return the full disk geometry, even if a
     *  partition has been opened. Only "start" changes.
     *  This is how the ioctl behaves on both 2.4 and 2.6
     *  Linuxes. This makes me wonder if the implementation
     *  on 2.4 in vbd_blkif_ioctl() is correct.
     */

    static int
vbd_blkif_getgeo_26 (struct block_device* bdev, struct hd_geometry* geo)
{
    struct gendisk*	gd = bdev->bd_disk;
    vbd_disk_t*		di = gd->private_data;
    vbd_link_t*		vbd = di->vbd;
    sector_t		sectors = get_capacity (gd);
    vbd2_get_geo_t*	rreq;
    nku64_f*		reply;
    int			diag;

    diag = vmq_msg_allocate (vbd->link, 0 /*data_len*/, (void**) &rreq,
			     NULL /*data_offset*/);
    if (diag) {
	WTRACE ("%s: %s failed to alloc msg (%d)\n", di->name,
		vbd_op_names [VBD2_OP_GETGEO], diag);
	goto use_hardcoded;
    }
    memset (rreq, 0, sizeof *rreq);
    rreq->common.op    = VBD2_OP_GETGEO;
    rreq->common.devid = di->xd_devid;
    rreq->common.genid = di->xd_genid;

    reply = vipc_ctx_call (&vbd->vipc_ctx, &rreq->common.cookie);
    if (!reply) {
	WTRACE ("%x: %s IPC call failed\n", di->xd_devid,
		vbd_op_names [VBD2_OP_GETGEO]);
	goto use_hardcoded;
    }
    rreq = container_of (reply, vbd2_get_geo_t, common.cookie);
    if (rreq->common.count) {	/* status */
	WTRACE ("%s: %s request failed (%d)\n", di->name,
		vbd_op_names [VBD2_OP_GETGEO], rreq->common.count);
	vmq_return_msg_free (vbd->link, rreq);
	goto use_hardcoded;
    }
    geo->heads     = rreq->heads;
    geo->sectors   = rreq->sects_per_track;
    geo->cylinders = rreq->cylinders;
    vmq_return_msg_free (vbd->link, rreq);
    return 0;

use_hardcoded:
	/* geo->start was set by caller blkdev_ioctl() in block/ioctl.c */
#ifdef MMC_BLOCK_MAJOR
    if (gd->major == MMC_BLOCK_MAJOR) {
	geo->heads     = 4;
	geo->sectors   = 16;	/* Per track */
    } else
#endif
    {
	geo->heads     = 0xff;
	geo->sectors   = 0x3f;	/* Per track */
    }
	/*
	 *  get_capacity() returns an unsigned 64-bit sector_t.
	 *  Performing a division by heads*sectors generates an
	 *  unresolved call to __udivdi3(). Given that capacity
	 *  was earlier set as a 32 bit integer, we could safely
	 *  typecast it to 32 bits here. But we use sector_div()
	 *  for universality. It returns the remainder and modifies
	 *  its first parameter to be the result.
	 */
    sector_div (sectors, geo->heads * geo->sectors);
    geo->cylinders = sectors;
    return 0;
}
#endif

    static void
vbd_request_end (struct request* req, const _Bool is_error)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
	/* __blk_end_request() is no more GPL */
    __blk_end_request (req, is_error ? -EIO : 0, blk_rq_bytes (req));
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
	/*
	 * Should call:
	 * __blk_end_request(req, is_error ? -EIO : 0,
	 *                   req->hard_nr_sectors << 9);
	 * but it is EXPORT_SYMBOL_GPL
	 */
    req->hard_cur_sectors = req->hard_nr_sectors;
    end_request (req, !is_error);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
    end_dequeued_request (req, !is_error);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if (unlikely (end_that_request_first (req, !is_error,
		  req->hard_nr_sectors))) {
	BUG();
    }
#else
	/*
	 *  This loop is necessary on 2.4 if we have
	 *  several buffer_head structures linked from
	 *  req->bh.
	 */
    while (end_that_request_first (req, !is_error, "VBD2"));
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
	/* Nothing to do */
#elif LINUX_VERSION_CODE > KERNEL_VERSION(2,6,15)
    end_that_request_last (req, !is_error);
#elif LINUX_VERSION_CODE == KERNEL_VERSION(2,6,15)
	/*
	 * This is a work around in order to distinguish between a
	 * vanilla Linux kernel 2.6.15 and a patched FC5 kernel.
	 */
#if defined MODULES_ARE_ELF32 || defined MODULES_ARE_ELF64
    end_that_request_last (req, !is_error);
#else
    end_that_request_last (req);
#endif
#else
    end_that_request_last (req);
#endif
}

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

    /*
     * Request block io.
     * Called from vbd_rq_do_blkif_request_2x() only.
     *
     * id: for guest use only.
     * operation: VBD2_OP_{READ,WRITE,PROBE}
     * buffer: buffer to read/write into. This should be a
     * virtual address in the guest os.
     */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    static _Bool
vbd_link_queue_request_26 (vbd_link_t* vbd, struct request* req,
			   vbd2_req_header_t* rreq, const unsigned data_offset)
{
    const unsigned	slot = vmq_msg_slot (vbd->link, rreq);
    const vbd_disk_t*	di = (vbd_disk_t*) req->rq_disk->private_data;
    char*		vshared = vmq_tx_data_area  (vbd->link) + data_offset;
    unsigned long	pshared = vmq_ptx_data_area (vbd->link) + data_offset;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
    struct req_iterator iter;
#else
    struct bio*         bio;
    int                 idx;
#endif
    struct bio_vec*     bvec;
    struct page*        page = NULL;
    unsigned long       paddr = 0;	/* page phys addr */
    void*		vaddr = NULL;	/* page virt addr */
    unsigned int        pfsect = 0, plsect = 0;
    u32		        count = 0;

    DTRACE ("vmq_tx_data_area %p data_offset %x vshared/pshared %p/%lx\n",
	    vmq_tx_data_area (vbd->link), data_offset, vshared, pshared);

    if (unlikely (!vbd->is_up)) {
	ETRACE ("link to %d not up\n", vmq_peer_osid (vbd->link));
	++vbd->errors;
	return 1;
    }
    vbd->reqs [slot] = req;
    rreq->cookie = (unsigned long) req;
    rreq->op     = rq_data_dir (req) ? VBD2_OP_WRITE : VBD2_OP_READ;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
	/* blk_rq_pos() is an inline function returning req->__sector */
    rreq->sector = blk_rq_pos (req);
#else
    rreq->sector = req->sector;
#endif
    rreq->devid  = di->xd_devid;
    rreq->genid  = di->xd_genid;

    DTRACE ("%s, sector 0x%llx\n", vbd_op_names [rreq->op % VBD2_OP_MAX],
	    rreq->sector);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
    rq_for_each_segment (bvec, req, iter)
#else
    rq_for_each_bio (bio, req) {
	bio_for_each_segment (bvec, bio, idx)
#endif
	{
	    const unsigned fsect = bvec->bv_offset >> 9;
	    const unsigned lsect = fsect + (bvec->bv_len >> 9) - 1;

	    if (unlikely (lsect > 7)) {
		BUG();
	    }
	    if ((page == bvec->bv_page) && (fsect == (plsect+1))) {
		    /* Extend the previously set segment */
		DTRACE ("extend\n");
		plsect = lsect;
		if (vbd->xx_config.data_count) {
		    char* const start = vshared - PAGE_SIZE + bvec->bv_offset;

		    DTRACE ("count %d vshared/pshared %p/%lx "
			    "pfsect/plsect %d/%d\n", count-1, vshared
			    - PAGE_SIZE, pshared - PAGE_SIZE, pfsect, plsect);
		    if (rq_data_dir (req)) {	/* Disk write */
			DTRACE ("paddr/vaddr %lx/%p bv_offset/len %x/%x\n",
				paddr, vaddr, bvec->bv_offset, bvec->bv_len);
			memcpy (start, vaddr + bvec->bv_offset, bvec->bv_len);
			vbd_cache_clean (start, bvec->bv_len);
		    } else {
			vbd_cache_invalidate (start, bvec->bv_len);
		    }
		    VBD2_FIRST_BUF (rreq) [count-1] =
			VBD2_BUFFER (pshared - PAGE_SIZE, pfsect, plsect);
		} else {
		    VBD2_FIRST_BUF (rreq) [count-1] =
			VBD2_BUFFER (paddr, pfsect, plsect);
		}
	    } else {
		if (unlikely (count == vbd->segs_per_req_max)) {
		    BUG();
	        }
		page   = bvec->bv_page;
		paddr  = page_to_phys (page);
		pfsect = fsect;
		plsect = lsect;

		if (vbd->xx_config.data_count) {
		    char* const start = vshared + bvec->bv_offset;

			/* We do not try to compact the requests */
		    DTRACE ("count %d vshared/pshared %p/%lx "
			    "pfsect/plsect %d/%d\n", count, vshared, pshared,
			    pfsect, plsect);
		    if (rq_data_dir (req)) {	/* Disk write */
			vaddr = phys_to_virt (paddr);
			DTRACE ("paddr/vaddr %lx/%p bv_offset/len %x/%x\n",
				paddr, vaddr, bvec->bv_offset, bvec->bv_len);
			memcpy (start, vaddr + bvec->bv_offset, bvec->bv_len);
			vbd_cache_clean (start, bvec->bv_len);
		    } else {
			vbd_cache_invalidate (start, bvec->bv_len);
		    }
		    VBD2_FIRST_BUF (rreq) [count++] =
			VBD2_BUFFER (pshared, pfsect, plsect);
		    vshared += PAGE_SIZE;
		    pshared += PAGE_SIZE;
		} else {
		    DTRACE ("count %d paddr %lx pfsect %d plsect %d\n",
			    count, paddr, pfsect, plsect);
		    VBD2_FIRST_BUF (rreq) [count++] =
			VBD2_BUFFER (paddr, pfsect, plsect);
		}
	    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	}
#endif
    }
    rreq->count = (vbd2_count_t) count;
    if (vbd->xx_config.data_count) {
	VBD_ASSERT (vbd->data_offsets [slot] == 0xFFFFFFFF);
	vbd->data_offsets [slot] = data_offset;
    }
    vmq_msg_send_async (vbd->link, rreq);
    return 0;
}

    static void
vbd_link_copy_back_26 (vbd_link_t* vbd, struct request* req,
		       const vbd2_req_header_t* rreq)
{
    const unsigned	data_offset =
			    vbd->data_offsets [vmq_msg_slot (vbd->link, rreq)];
    char*		vshared = vmq_tx_data_area  (vbd->link) + data_offset;
    unsigned long	pshared = vmq_ptx_data_area (vbd->link) + data_offset;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
    struct req_iterator iter;
#else
    struct bio*         bio;
    int                 idx;
#endif
    struct bio_vec*     bvec;
    struct page*        page = NULL;
    unsigned long       paddr = 0;	/* page phys addr */
    void*		vaddr = NULL;	/* page virt addr */
    unsigned int        pfsect = 0, plsect = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
    rq_for_each_segment (bvec, req, iter)
#else
    rq_for_each_bio (bio, req) {
	bio_for_each_segment (bvec, bio, idx)
#endif
	{
	    const unsigned fsect = bvec->bv_offset >> 9;
	    const unsigned lsect = fsect + (bvec->bv_len >> 9) - 1;

	    if ((page == bvec->bv_page) && (fsect == (plsect+1))) {
		    /* Extend the previously set segment */
		DTRACE ("EXTEND vshared/pshared %p/%lx pfsect/plsect %d/%d\n",
			vshared - PAGE_SIZE, pshared - PAGE_SIZE,
			pfsect, plsect);
		DTRACE ("paddr/vaddr %lx/%p bv_offset/len %x/%x\n",
			paddr, vaddr, bvec->bv_offset, bvec->bv_len);
		plsect = lsect;
		memcpy (vaddr               + bvec->bv_offset,
			vshared - PAGE_SIZE + bvec->bv_offset, bvec->bv_len);
	    } else {
		page   = bvec->bv_page;
		paddr  = page_to_phys (page);
		pfsect = fsect;
		plsect = lsect;

		vaddr = phys_to_virt (paddr);
		DTRACE ("paddr %lx vaddr %p bv_offset %x bv_len %x\n",
			paddr, vaddr, bvec->bv_offset, bvec->bv_len);
		DTRACE ("pshared %lx pfsect %d plsect %d\n",
			pshared, pfsect, plsect);
		memcpy (vaddr   + bvec->bv_offset,
			vshared + bvec->bv_offset, bvec->bv_len);
		vshared += PAGE_SIZE;
		pshared += PAGE_SIZE;
	    }
	    vbd_cache_clean (vaddr + bvec->bv_offset, bvec->bv_len);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	}
#endif
    }
}

    /*
     * Linux 2.6 code.
     * Read a block. Request is in a request queue.
     * Called from the kernel only.
     */
    static void
vbd_rq_do_blkif_request_26 (struct request_queue* rq)
{
    vbd_link_t*     vbd = rq->queuedata;
    struct request* req;

    DTRACE ("link %d\n", vmq_peer_osid (vbd->link));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
    while ((req = blk_peek_request (rq)) != NULL)
#else
    while ((req = elv_next_request (rq)) != NULL)
#endif
    {
	const vbd_disk_t*	di = req->rq_disk->private_data;
	vbd2_req_header_t*	rreq;
	unsigned		data_offset;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
	if (!blk_fs_request (req)) {
	    end_request (req, 0);
	    continue;
	}
#endif
	if (di->is_zombie || !vbd->is_up) {
	    DTRACE ("disk zombie %d is_up %d\n", di->is_zombie, vbd->is_up);
	    rreq = NULL;
	} else {
	    int diag = vmq_msg_allocate_ex
		(vbd->link, vbd->xx_config.data_count ? PAGE_SIZE : 0,
		 (void**) &rreq, vbd->xx_config.data_count ? &data_offset :
		 NULL, 1 /*nonblocking*/);
	    if (diag) {
		DTRACE ("failed to alloc msg (%d)\n", diag);
		if (diag == -ESTALE) {
		    rreq = NULL;
		} else {
		    blk_stop_queue (rq);
		    break;
		}
	    }
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
	blk_start_request (req);

	if (req->cmd_type != REQ_TYPE_FS) {
	    DTRACE ("ending with -EIO\n");
	    __blk_end_request_all (req, -EIO);
	    if (rreq) {
		vmq_return_msg_free (vbd->link, rreq);
		if (vbd->xx_config.data_count) {
		    vmq_data_free (vbd->link, data_offset);
		}
	    }
	    continue;
	}
	DTRACE ("%p: cmd %p sec %llx (%u/%u) buffer %p [%s]\n",
		req, req->cmd, (u64) blk_rq_pos (req),
		blk_rq_cur_sectors (req), blk_rq_sectors (req),
		req->buffer, rq_data_dir (req) ? "write" : "read");
#else
	DTRACE ("%p: cmd %p sec %llx (%u/%li) buffer %p [%s]\n",
		req, req->cmd, (u64) req->sector, req->current_nr_sectors,
		req->nr_sectors, req->buffer,
		rq_data_dir (req) ? "write" : "read");

	blkdev_dequeue_request (req);
#endif
	if (!rreq) {
	    DTRACE ("ending with error\n");
	    vbd_request_end (req, true);
	    continue;
	}
	if (vbd_link_queue_request_26 (vbd, req, rreq, data_offset)) {
		/* Error message already issued and error accounted */
	    blk_stop_queue (rq);
	    vmq_return_msg_free (vbd->link, rreq);
	    if (vbd->xx_config.data_count) {
		vmq_data_free (vbd->link, data_offset);
	    }
	    break;
	}
    }
    vmq_msg_send_flush (vbd->link);
}

#else /* 2.4.x */

#ifdef VBD_DEBUG_REQUESTS
    static void
vbd_print_struct_request_24 (const struct request* req)
{
    int			count = 0;
    unsigned long	paddr = 0;
    unsigned long	page = 0;
    unsigned int        plsect = 0;	/* Previous last sector */
    int			continued = 0;
    struct buffer_head*	bh;

    printk ("request %p: sector %lu cmd %d\n", req, req->sector, req->cmd);
    if (!req->bh) {
	printk ("req->bh NULL\n");
	return;
    }
    bh = req->bh;
    while (bh) {
	unsigned long b_paddr = virt_to_phys (bh->b_data);
	const unsigned fsect = (b_paddr & ~PAGE_MASK) >> 9;
	const unsigned lsect = fsect + (bh->b_size >> 9) - 1;

	printk ("%d: bh %p b_rsector %lu b_paddr %lx ", count++, bh,
		bh->b_rsector, b_paddr);
	printk ("b_size %d fsect %d lsect %d", bh->b_size, fsect, lsect);

	b_paddr &= PAGE_MASK;
	if ((page == b_paddr) && (fsect == (plsect+1))) {
		/* New buffer header is in same page, continuing old one */
	    plsect = lsect;
	    printk (" <CONT>");
	    ++continued;
	} else {
	    page   = b_paddr;
	    paddr  = page;
	    plsect = lsect;
	}
	printk ("\n");
	bh = bh->b_reqnext;
    }
    printk ("Continued: %d in %d\n", continued, count);
}
#endif

    /*
     * Linux 2.4 only.
     * Called from vbd_rq_do_blkif_request_24() only.
     */

    static _Bool
vbd_link_queue_request_24 (vbd_link_t* vbd, struct request* req,
			   vbd2_req_header_t* rreq)
{
#ifdef VBD_DEBUG_STATS
    vbd_fe_t*			fe = vbd->fe;
#endif
    const struct gendisk*	gd;
    const vbd_disk_t*		di;
    const struct buffer_head*	bh;
    unsigned long		page;
    unsigned long		paddr;
    unsigned int		fsect, lsect, pfsect, plsect;
    u32				count;
#ifdef VBD_DEBUG_HISTO_BHS
    int				histo_bhs = 0;
#endif
#ifdef VBD_DEBUG_HISTO_SECTORS
    int				histo_sectors = 0;
#endif
#ifdef VBD_DEBUG
    unsigned long		bh_sector;
#endif

    DTRACE ("vbd %p\n", vbd);
    if (unlikely (!vbd->is_up)) {
	ETRACE ("link to %d not up\n", vmq_peer_osid (vbd->link));
	++vbd->errors;
	return 1;
    }
    if (unlikely (!req->bh)) {
	ETRACE ("no buffer head\n");
	++vbd->errors;
	return 1;
    }
    gd = get_gendisk (req->rq_dev);
    di = (vbd_disk_t*) gd->real_devices + MINOR (req->rq_dev);
    VBD_ASSERT (di->gd == gd);

    vbd->reqs [vmq_msg_slot (vbd->link, rreq)] = req;
    rreq->cookie     = (unsigned long) req;
    rreq->op         = req->cmd == WRITE ? VBD2_OP_WRITE : VBD2_OP_READ;
    VBD_ASSERT (req->sector == req->bh->b_rsector);
    rreq->sector     = req->bh->b_rsector +
		       gd->part [MINOR (req->rq_dev)].start_sect;
    rreq->devid      = di->xd_devid;
    rreq->genid      = di->xd_genid;

    page   = 0;
    count  = 0;
    paddr  = 0;
    plsect = 0;		/* Previous  last sector */
    pfsect = 0;		/* Previous first sector */

    bh = req->bh;
#ifdef VBD_DEBUG
    bh_sector = bh->b_rsector;
#endif
    while (bh) {
	unsigned long b_paddr = virt_to_phys (bh->b_data);

#ifdef VBD_DEBUG_HISTO_BHS
	++histo_bhs;
#endif
	VBD_ASSERT (bh->b_rsector == bh_sector);
	if (unlikely ((b_paddr & ((1<<9)-1)) != 0)) {
		/* Not sector aligned */
	    BUG();
	}
	fsect = (b_paddr & ~PAGE_MASK) >> 9;
	lsect = fsect + (bh->b_size >> 9) - 1;
	if (unlikely (lsect > 7)) {
	    BUG();
	}
#ifdef VBD_DEBUG_HISTO_SECTORS
	histo_sectors += bh->b_size >> 9;
#endif
#ifdef VBD_DEBUG_STATS
	VBD_ASSERT (lsect-fsect <= 7 && lsect-fsect >= 0);
	++fe->stats.sectors_per_buffer_head [lsect-fsect];
#endif
	b_paddr &= PAGE_MASK;
	if ((page == b_paddr) && (fsect == (plsect+1))) {
		/* New buffer header is in same page, continuing old one */
	    plsect = lsect;
	    VBD2_FIRST_BUF (rreq) [count-1] =
		VBD2_BUFFER (paddr, pfsect, plsect);

#ifdef VBD_DEBUG_STATS
	    ++fe->stats.new_bh_same_page [0];
#endif
	} else {
	    if (unlikely (count == vbd->segs_per_req_max)) {
		ETRACE ("too many segments in req (%d)\n", count);
#ifdef VBD_DEBUG_REQUESTS
		vbd_print_struct_request_24 (req);
#endif
		BUG();
	    }
	    page   = b_paddr;
	    paddr  = page;
	    pfsect = fsect;
	    plsect = lsect;
	    VBD2_FIRST_BUF (rreq) [count++] =
		VBD2_BUFFER (paddr, pfsect, plsect);
#ifdef VBD_DEBUG_STATS
	    ++fe->stats.new_bh_same_page [1];
#endif
	}
#ifdef VBD_DEBUG
	bh_sector += bh->b_size >> 9;
#endif
	bh = bh->b_reqnext;
    }
    rreq->count = (vbd2_count_t) count;

#ifdef VBD_DEBUG_STATS
#ifdef VBD_DEBUG_HISTO_BHS
    if (histo_bhs >= VBD_ARRAY_ELEMS (fe->stats.len_histo)) {
	histo_bhs  = VBD_ARRAY_ELEMS (fe->stats.len_histo) - 1;
    }
    ++fe->stats.len_histo [histo_bhs];
#elif defined VBD_DEBUG_HISTO_SECTORS
    if (histo_sectors >= VBD_ARRAY_ELEMS (fe->stats.len_histo)) {
	histo_sectors  = VBD_ARRAY_ELEMS (fe->stats.len_histo) - 1;
    }
    ++fe->stats.len_histo [histo_sectors];
#elif defined VBD_DEBUG_HISTO_SEGMENTS
	/* Only modify "count" after it has been used, above */
    if (count >= VBD_ARRAY_ELEMS (fe->stats.len_histo)) {
	count  = VBD_ARRAY_ELEMS (fe->stats.len_histo) - 1;
    }
    ++fe->stats.len_histo [count];
#endif
#endif
    vmq_msg_send_async (vbd->link, rreq);
    return 0;
}

    /*
     *  Linux 2.4 only.
     *  Called by the kernel, and also specifically on 2.4,
     *  from vbd_link_kick_pending_request_queues_2x(), after some place
     *  has just been made in a full FIFO.
     */

    static void
vbd_rq_do_blkif_request_24 (struct request_queue* rq)
{
    vbd_link_t*		vbd = rq->queuedata;
#ifdef VBD_DEBUG_STATS
    vbd_fe_t*		fe = vbd->fe;
#endif
    struct request*	req;

    DTRACE ("entered, vbd %p\n", vbd);
    while (!rq->plugged && !list_empty (&rq->queue_head) &&
	   (req = blkdev_entry_next_request (&rq->queue_head)) != NULL) {
	vbd2_req_header_t*	rreq;

	VBD_CATCHIF (rq != BLK_DEFAULT_QUEUE (MAJOR (req->rq_dev)),
	    WTRACE("<Non-def-queue:%d:%d>", MAJOR (req->rq_dev),
		  MINOR (req->rq_dev)));
	VBD_CATCHIF (((vbd_disk_t*) get_gendisk(req->rq_dev)->real_devices
	    + MINOR (req->rq_dev))->gd_queue !=
	    BLK_DEFAULT_QUEUE (MAJOR (req->rq_dev)),
	    WTRACE ("<Bad-gd_queue:%d:%d>", MAJOR (req->rq_dev),
		    MINOR (req->rq_dev)));

	if (vmq_msg_allocate_ex (vbd->link, 0 /*data_len*/, (void**) &rreq,
			         NULL /*data_offset*/, 1 /*nonblocking*/)) {
	    struct gendisk*	gd = get_gendisk (req->rq_dev);
	    vbd_disk_t*		di;

	    VBD_ASSERT (gd);
	    di = (vbd_disk_t*) gd->real_devices + MINOR (req->rq_dev);

	    if (di->is_part) {
		    /* We come here when accessing /dev/sdaX */
#ifdef VBD_DEBUG_STATS
		if (di->next->req_is_pending) {
		    ++fe->stats.was_already_pending;
		} else {
		    ++fe->stats.was_not_not_yet_pending;
		}
#endif
		di->next->req_is_pending = 1;
	    } else {
		    /* We come here when accessing /dev/sda */
		di->req_is_pending = 1;
	    }
	    DTRACE ("ring full! (di %p)\n", di);
	    break;
	}
#ifdef VBD_DEBUG_STATS
	++fe->stats.ring_not_full;
#endif
	DTRACE ("%p: cmd %x sec %llx (%lu/%li) buffer %p [%s]\n",
		req, req->cmd, (u64) req->sector, req->current_nr_sectors,
		req->nr_sectors, req->buffer,
		req->cmd == WRITE ? "write" : "read");
	VBD_ASSERT ((req->cmd == READA) || (req->cmd == READ) ||
		    (req->cmd == WRITE));

	blkdev_dequeue_request (req);
	if (vbd_link_queue_request_24 (vbd, req, rreq)) {
		/* Error message already issued and error accounted */
	    vmq_return_msg_free (vbd->link, rreq);
	    break;
	}
    }
    vmq_msg_send_flush (vbd->link);
}
#endif	/* 2.4.x */

#define VBD_LINK(link) \
    (*(vbd_link_t**) &((vmq_link_public_t*) (link))->priv)

    /* Only called by vbd_cb_return_notify() */

    static void
vbd_link_return_msg (vbd_link_t* vbd, vbd2_resp_t* resp)
{
    const unsigned	slot = vmq_msg_slot (vbd->link, resp);
    const vbd2_op_t	op = resp->op;	/* Sample for safety */

    DTRACE ("entered\n");
    switch (op) {
    case VBD2_OP_READ:
    case VBD2_OP_WRITE: {
	struct request* const req = vbd->reqs [slot];
	_Bool		is_error = resp->count != VBD2_STATUS_OK;
	unsigned long	flags;

	if ((struct request*)(unsigned long) resp->cookie != req) {
	    ETRACE ("OS %d bad cookie %llx != %p slot %u resp %p\n",
		    vmq_peer_osid (vbd->link), resp->cookie, req, slot, resp);
	    is_error = 1;
	}
	if (is_error) {
	    ++vbd->errors;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	if (vbd->xx_config.data_count && op == VBD2_OP_READ && !is_error) {
	    vbd_link_copy_back_26 (vbd, req, resp);
	}
#endif
	VBD_CATCHIF (is_error,
		     ETRACE ("Bad return from %s: %x\n", vbd_op_names [op],
			     resp->count));
	spin_lock_irqsave (&vbd->io_lock, flags);
	vbd_request_end (req, is_error);
	vbd_link_kick_pending_request_queues_2x (vbd);
	spin_unlock_irqrestore (&vbd->io_lock, flags);
	break;
    }
    case VBD2_OP_PROBE:
    case VBD2_OP_MEDIA_CONTROL:
    case VBD2_OP_OPEN:
    case VBD2_OP_CLOSE:
    case VBD2_OP_GETGEO:
	DTRACE ("response to %s received\n", vbd_op_names [op]);
	if (!vipc_ctx_process_reply (&VBD_LINK (vbd)->vipc_ctx,
				     &resp->cookie)) {
	    resp = NULL;	/* Will be freed by caller */
	}
	break;

    default:
	WTRACE ("Unexpected response code %u\n", op);
	break;
    }
    if (resp) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	if (vbd->xx_config.data_count) {
	    vmq_data_free (vbd->link, vbd->data_offsets [slot]);
	    vbd->data_offsets [slot] = 0xFFFFFFFF;
	}
#endif
	vmq_return_msg_free (vbd->link, resp);
    }
}

/*----- Module thread -----*/

    static void
vbd_thread_aborted_notify (vbd_fe_t* fe)
{
    fe->is_thread_aborted = true;
    up (&fe->thread_sem);
}

    static _Bool
vbd_link_changes (vmq_link_t* link, void* unused_cookie)
{
    vbd_link_t* vbd = VBD_LINK (link);

    (void) unused_cookie;
    if (vbd->changes) {
	vbd->changes = false;
	vbd_link_acquire_disks (vbd);
    }
    return false;
}

    static int
vbd_thread (void* arg)
{
    vbd_fe_t* fe = arg;

    DTRACE ("started\n");
    while (!fe->is_thread_aborted) {
	int diag = down_interruptible (&fe->thread_sem);

	(void) diag;
	DTRACE ("wakeup%s%s%s\n",
		fe->is_thread_aborted ? " thread-aborted"  : "",
		fe->changes           ? " changes"         : "",
		fe->is_sysconf        ? " sysconf"         : "");
	if (fe->is_thread_aborted) break;
	if (fe->is_sysconf) {
	    fe->is_sysconf = false;
	    vmq_links_sysconf (fe->links);
	}
	if (fe->changes) {
	    fe->changes = false;
	    vmq_links_iterate (fe->links, vbd_link_changes, NULL);
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
	    /* Check if freeze signal has been received */
	try_to_freeze();
#endif
    }
    DTRACE ("exiting\n");
    return 0;
}

/*----- VMQ event handlers (callbacks) -----*/

    /*
     *  Callback from vlx-vmq.c in the context of vbd_thread().
     */

    static void
vbd_cb_link_on (vmq_link_t* link)
{
    vbd_link_t* vbd = VBD_LINK (link);

    DTRACE ("link on (local <-> OS %d).\n", vmq_peer_osid (link));
    vbd->is_up = true;
    vbd_link_acquire_disks (vbd);
}

    /*
     *  Callback from vlx-vmq.c in the context of vbd_thread().
     */

    static void
vbd_cb_link_off (vmq_link_t* link)
{
    vbd_link_t* vbd = VBD_LINK (link);

    DTRACE ("link %d\n", vmq_peer_osid (link));
    vbd->is_up = false;
	/*
	 *  Mark deleted to make all future invocations
	 *  fail, until the client decides to close device
	 *  finally.
	 *  XXX: we need to abort current async disk ops too.
	 */
    vipc_ctx_abort_calls (&VBD_LINK (link)->vipc_ctx);
}

    /*
     *  Callback from vlx-vmq.c in the context of vbd_thread().
     */

    static void
vbd_cb_link_off_completed (vmq_link_t* link)
{
    vbd_link_t* vbd = VBD_LINK (link);

    DTRACE ("entered\n");
	/*
	 * Called before link_off if no buffers used,
	 * so make sure the state is current.
	 */
    vbd->is_up = false;
    vbd_link_delete_disks (vbd);
}

#define VBD_LINKS(links) \
    ((vbd_fe_t*) ((vmq_links_public*) (links))->priv)
#undef VBD_LINKS
#define VBD_LINKS(links) (*(vbd_fe_t**) (links))

    static void
vbd_cb_sysconf_notify (vmq_links_t* links)
{
    vbd_fe_t* fe = VBD_LINKS (links);

    DTRACE ("entered\n");
    fe->is_sysconf = true;
    up (&fe->thread_sem);
}

    /* Executes in interrupt context */
    /* Called from vbd_link_receive_msg() only */

    static inline void
vbd_link_changes_notify (vmq_link_t* link)
{
    vbd_link_t*	vbd = VBD_LINK (link);
    vbd_fe_t*	fe  = vbd->fe;

    vbd->changes = true;
    fe->changes = true;
    up (&fe->thread_sem);
}

    /* Executes in interrupt context */

    static void
vbd_link_receive_msg (vmq_link_t* link, vbd2_msg_t* async)
{
    DTRACE ("%s link %d cookie %llx\n",
	    vbd_op_names [async->req.op % VBD2_OP_MAX],
	    vmq_peer_osid (link), async->req.cookie);
    if (async->req.op == VBD2_OP_CHANGES) {
	vbd_link_changes_notify (link);
    } else {
	ETRACE ("Got invalid op %d cookie %llx\n", async->req.op,
		async->req.cookie);
	++VBD_LINK (link)->errors;
    }
    vmq_msg_free (link, async);
}

    static void
vbd_cb_receive_notify (vmq_link_t* link)
{
    void*  msg;

    DTRACE ("got async from %d\n", vmq_peer_osid (link));
    while (!vmq_msg_receive (link, &msg)) {
	vbd_link_receive_msg (link, (vbd2_msg_t*) msg);
    }
}

    static void
vbd_cb_return_notify (vmq_link_t* link)
{
    void*  msg;

    while (!vmq_return_msg_receive (link, &msg)) {
	vbd_link_return_msg (VBD_LINK (link), (vbd2_resp_t*) msg);
    }
}

/*----- Support for /proc/nk/vbd2-fe -----*/

typedef struct {
    char* page;
    int   len;
} vbd_proc_t;

    static _Bool
vbd_link_proc (vmq_link_t* link, void* cookie)
{
    vbd_link_t*	vbd = VBD_LINK (link);
    vbd_proc_t*	ctx = cookie;
    vbd_disk_t*	di;

    ctx->len += sprintf (ctx->page + ctx->len,
			 "BE Rq MsgMax SegRqM MaxProbe IsUp DB Errs\n");
    ctx->len += sprintf (ctx->page + ctx->len,
			 "%2d %2d %6d %6d %8d %4d %s %4d\n",
			 vmq_peer_osid (link), vbd->xx_config.msg_count,
			 vbd->msg_max, vbd->segs_per_req_max,
			 VBD_LINK_MAX_DEVIDS_PER_PROBE (vbd), vbd->is_up,
			 vbd->xx_config.data_count ? "On" : "No", vbd->errors);

    if (!vbd->disks) return false;
    ctx->len += sprintf (ctx->page + ctx->len,
			 "Disk--- Xdev----- Name Device- Usage Zombie\n");
    VBD_LINK_FOR_ALL_DISKS(di,vbd) {
	const char* disk_name2;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	char buf [64];
#endif

	if (di->gd) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	    disk_name2 = di->gd->disk_name;
#else
	    disk_name2 = disk_name (di->gd, MINOR (di->device), buf);
#endif
	} else {
	    disk_name2 = "*******";
	}
	ctx->len += sprintf (ctx->page + ctx->len, "%7s %9s %4s %7x %5d %6s\n",
			     disk_name2,
			     di->name, di->major->type->name, di->device,
			     di->usage, di->is_zombie ? "yes" : "no");
    }
    return false;
}

    static int
vbd_read_proc (char* page, char** start, off_t off, int count, int* eof,
	       void* data)
{
    vbd_fe_t*	fe = data;
    off_t	begin = 0;
    vbd_proc_t	ctx;
    int		i;

    ctx.len = sprintf (page,
	"Major Usage Index MajorIdx TypeName PartShift Devs/Major\n");
    for (i = 0; i < VBD_ARRAY_ELEMS (fe->majors); ++i) {
	vbd_major_t* major = fe->majors [i];

	if (!major) continue;
	ctx.len += sprintf (page + ctx.len, "%5d %5d %5d %8d %8s %9d %10d\n",
			    major->major, major->usage, major->index,
			    major->major_idx, major->type->name,
			    major->type->partn_shift,
			    major->type->devs_per_major);
    }
    ctx.page = page;
    vmq_links_iterate (fe->links, vbd_link_proc, &ctx);
    page = ctx.page;

    if (ctx.len + begin > off + count)
	goto done;
    if (ctx.len + begin < off) {
	begin += ctx.len;
	ctx.len = 0;
    }
    *eof = 1;
done:
    if (off >= ctx.len + begin) return 0;
    *start = page + off - begin;
    return (count < begin + ctx.len - off ? count : begin + ctx.len - off);
}

/*----- Initialization and exit -----*/

static vbd_fe_t vbd_fe;

    static int __init
vbd_wait (char* start)
{
    long  vmajor;
    long  vminor;
    char* end;

    if (*start++ != '(') {
	return 0;
    }
    vmajor = simple_strtoul (start, &end, 0);
    if (end == start || *end != ',') {
	return 0;
    }
    start = end + 1;
    vminor = simple_strtoul (start, &end, 0);
    if (end == start || *end != ')') {
	return 0;
    }
    vbd_fe.must_wait = 1;
    vbd_fe.wait_devid   = (vmajor << 8) | vminor;
    return 1;
}

#ifndef MODULE
__setup ("vbd2-wait=", vbd_wait);
#else
#ifdef CONFIG_ARM
#define vlx_command_line	saved_command_line
#else
extern char* vlx_command_line;
#endif
#endif

    static void* __init
vbd_vlink_syntax (const char* opt)
{
    ETRACE ("Syntax error near '%s'\n", opt);
    return NULL;
}

#include "vlx-vbd2-common.c"

    static _Bool
vbd_link_init (vmq_link_t* link, void* cookie)
{
    vbd_link_t* vbd = VBD_LINK (link);
    vbd_fe_t*	fe = cookie;

    VBD_ASSERT (vbd);
    vbd->fe      = fe;
    vbd->link    = link;
    vbd->msg_max = vmq_msg_max (link);
	/* disks = NULL */
    spin_lock_init(&vbd->io_lock);
	/* xx_config initialized by vbd_cb_get_xx_config() */
    vipc_ctx_init (&vbd->vipc_ctx, link);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if (vbd->xx_config.data_count) {
	if (vmq_ptx_data_area (vbd->link) % PAGE_SIZE) {
	    ETRACE ("data area is not page aligned\n");
	    return true;
	}
	VBD_ASSERT (vbd->xx_config.data_count == vbd->xx_config.msg_count);
	vbd->data_offsets = kmalloc
	    (sizeof (unsigned) * vbd->xx_config.data_count, GFP_KERNEL);
	if (!vbd->data_offsets) {
	    ETRACE ("Out of memory for data offsets array\n");
	    return true;
	}
	memset (vbd->data_offsets, 0xFF,
		sizeof (unsigned) * vbd->xx_config.data_count);
    }
#endif
    vbd->reqs = kzalloc
	(sizeof (struct request*) * vbd->xx_config.msg_count, GFP_KERNEL);
    if (!vbd->reqs) {
	ETRACE ("Out of memory for request pointer array\n");
	return true;
    }
    return false;
}

    /* Only called from vbd_exit() */

    static _Bool
vbd_link_free (vmq_link_t* link, void* cookie)
{
    vbd_link_t* vbd = VBD_LINK (link);

    (void) cookie;
    vbd_link_delete_disks (vbd);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    vbd_kfree_and_clear (vbd->data_offsets);
#endif
    vbd_kfree_and_clear (vbd->reqs);
    vbd_kfree_and_clear (vbd);
    return false;
}

#define VBD_FIELD(name,value)	value

    static const vmq_callbacks_t
vbd_callbacks = {
    VBD_FIELD (link_on,			vbd_cb_link_on),
    VBD_FIELD (link_off,		vbd_cb_link_off),
    VBD_FIELD (link_off_completed,	vbd_cb_link_off_completed),
    VBD_FIELD (sysconf_notify,		vbd_cb_sysconf_notify),
    VBD_FIELD (receive_notify,		vbd_cb_receive_notify),
    VBD_FIELD (return_notify,		vbd_cb_return_notify),
    VBD_FIELD (get_tx_config,		vbd_cb_get_xx_config),
    VBD_FIELD (get_rx_config,		NULL)
};

    static const vmq_xx_config_t
vbd_rx_config = {
    VBD_FIELD (msg_count,	4),
    VBD_FIELD (msg_max,		sizeof (vbd2_msg_t)),
    VBD_FIELD (data_count,	0),
    VBD_FIELD (data_max,	0)
};

#undef VBD_FIELD

    static void
vbd_exit (void)
{
    vbd_fe_t* fe = &vbd_fe;

    DTRACE ("exiting\n");
    if (fe->links) {
	vmq_links_abort (fe->links);
    }
    vbd_thread_aborted_notify (fe);
    vlx_thread_join (&fe->thread_desc);
    if (fe->links) {
	vmq_links_iterate (fe->links, vbd_link_free, NULL);
	vmq_links_finish (fe->links);
	fe->links = NULL;
    }
    if (fe->proc) {
	remove_proc_entry ("nk/vbd2-fe", NULL);
    }
}

    static int __init
vbd_init (void)
{
    vbd_fe_t*	fe = &vbd_fe;
    int		diag;

    DTRACE ("initializing\n");
#ifdef MODULE
    {
	char* cmdline;
	cmdline = vlx_command_line;
	while ((cmdline = strstr (cmdline, "vbd2-wait="))) {
	    cmdline += 11;
	    vbd_wait (cmdline);
	}
    }
#endif
    fe->proc = create_proc_read_entry ("nk/vbd2-fe", 0, NULL,
				       vbd_read_proc, fe);
    sema_init (&fe->thread_sem, 0);	/* Before it is signaled */
    diag = vmq_links_init_ex (&fe->links, "vbd2", &vbd_callbacks,
			      NULL /*tx_config*/, &vbd_rx_config, fe, true);
    if (diag) goto error;
    if (vmq_links_iterate (fe->links, vbd_link_init, fe)) {
	diag = -ENOMEM;
	goto error;
    }
    diag = vmq_links_start (fe->links);
    if (diag) goto error;
    diag = vlx_thread_start (&fe->thread_desc, vbd_thread, fe, "vbd2-fe");
    if (diag) {
	ETRACE ("thread start failure\n");
	goto error;
    }
    while (fe->must_wait) {	/* Waiting for the virtual disk */
	set_current_state (TASK_INTERRUPTIBLE);
	schedule_timeout (1);
    }
    TRACE ("initialized\n");
    return 0;

error:
    ETRACE ("init failed (%d)\n", diag);
    vbd_exit();
    return diag;
}

    /* late_initcall does not exist on eg. 2.4.18 */
#ifdef late_initcall
late_initcall (vbd_init);
#else
module_init (vbd_init);
#endif
module_exit (vbd_exit);

/*----- Module description -----*/

MODULE_DESCRIPTION ("VLX Virtual Block Device v.2 frontend driver");
MODULE_AUTHOR ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_LICENSE ("GPL");

/*----- End of file -----*/

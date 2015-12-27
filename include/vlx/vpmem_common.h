/*
 ****************************************************************
 *
 *  Component: VLX virtual Android physical memory
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
 *    Christophe Lizzi (Christophe.Lizzi@redbend.com)
 *    Vladimir Grouzdev (Vladimir.Grouzdev@redbend.com)
 *
 ****************************************************************
 */

#ifndef _VPMEM_COMMON_H_
#define _VPMEM_COMMON_H_

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>	/* struct inode for android_pmem.h */
#include <linux/android_pmem.h>

#include <nk/nkern.h>
#include <nk/nkdev.h>
#include <nk/nk.h>

#define VPMEM_DEFAULT_SIZE   (1024*1024)

#define VPMEM_VLINK_NAME     "vpmem"

#ifndef VPMEM_DRV_NAME
#define VPMEM_DRV_NAME       "vpmem"
#endif

typedef struct vpmem_shdev_t {	// shared vpmem device
    nku32_f base;		// vpmem physical base address
    nku32_f size;		// vpmem size 
} vpmem_shdev_t;

typedef struct vpmem_dev {
    unsigned int                      id;
    NkPhAddr                          plink;
    NkDevVlink*                       vlink;
    char*                             info;
    char                              name[25];
    unsigned int                      pmem_size;
    NkPhAddr                          pmem_phys;
    unsigned char*                    pmem_base;
    struct vpmem_dev*                 next;

#if defined CONFIG_VPMEM_FRONTEND || defined CONFIG_VPMEM_FRONTEND_MODULE
    struct platform_device            plat_dev;
    struct android_pmem_platform_data plat_data;
#endif

} vpmem_dev_t;

#ifdef VPMEM_DEBUG
#define DTRACE(fmt, args...)    \
	printk(VPMEM_DRV_NAME ": %s: " fmt, __func__, ## args)
#else
#define DTRACE(fmt, args...)    \
	do {} while (0)
#endif

        // always enabled
#define DTRACE1(fmt, args...)   \
	printk(VPMEM_DRV_NAME ": %s: " fmt, __func__, ## args)

        // always disabled
#define DTRACE0(fmt, args...)   \
	do {} while (0)

#define ETRACE(fmt, args...)    \
	printk(VPMEM_DRV_NAME ": ERROR: %s: " fmt, __func__, ## args)

    typedef int
vpmem_dev_init_t (vpmem_dev_t* vpmem);

    typedef void
vpmem_dev_exit_t (vpmem_dev_t* vpmem);

    extern int
vpmem_module_init (int is_client, vpmem_dev_init_t dev_init);

    extern void
vpmem_module_exit (vpmem_dev_exit_t dev_exit);

    extern int
vpmem_info_name (char* info, char* name, int maxlen);

    extern unsigned int
vpmem_info_size (char* info);

    extern unsigned int
vpmem_info_base (char* info);

    extern unsigned int
vpmem_info_id (char* info);

    vpmem_dev_t*
vpmem_dev_peer (vpmem_dev_t* vpmem);

#endif // _VPMEM_COMMON_H_

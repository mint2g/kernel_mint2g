/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_BOARD_H
#define __ASM_ARCH_BOARD_H

#ifdef	CONFIG_MACH_SP8810EA
#include <mach/gpio-sp8810ea.h>
#endif

#ifdef	CONFIG_MACH_SP8810EB
#include <mach/gpio-sp8810eb.h>
#endif

#ifdef	CONFIG_MACH_SP8810EC
#include <mach/gpio-sp8810ec.h>
#endif

#ifdef	CONFIG_MACH_SP8810GA
#include <mach/gpio-sp8810ga.h>
#endif

#ifdef CONFIG_MACH_SP6820GA
#include <mach/gpio-sp6820ga.h>
#endif

#ifdef	CONFIG_MACH_SP8810GB
#include <mach/gpio-sp8810gb.h>
#endif

#ifdef	CONFIG_MACH_OPENPHONE_SC6820
#include <mach/gpio-sc6820-openphone.h>
#endif

#ifdef	CONFIG_MACH_AMAZING
#include <mach/gpio-amazing.h>
#endif

#ifdef CONFIG_MACH_KYLETD
#include <mach/gpio-kyletd.h>
#endif

#ifdef CONFIG_MACH_KYLEW
#include <mach/gpio-kylew.h>
#endif

#ifdef  CONFIG_MACH_MINT
#include <mach/gpio-mint.h>
#endif

#ifdef	CONFIG_MACH_Z788
#include <mach/gpio-z788.h>
#endif

#ifdef	CONFIG_MACH_SP7710G_OPENPHONE
#include <mach/gpio-sp7710g_openphone.h>
#endif

#ifdef	CONFIG_MACH_SP7702
#include <mach/gpio-sp7702.h>
#endif

/*
 * pmem area definition
 */
#include <asm/sizes.h>

#define SPRD_PMEM_SIZE		(0)
#define SPRD_PMEM_ADSP_SIZE	(0)

#define SPRD_ROT_MEM_SIZE	(0)
#define SPRD_SCALE_MEM_SIZE	(0)

#ifdef CONFIG_ION
//#define SPRD_ION_SIZE           (CONFIG_SPRD_ION_SIZE*SZ_1M)
    #if defined(CONFIG_CAMERA_8M)
    #define SPRD_ION_SIZE   (23*1024*1024)
    #elif defined(CONFIG_CAMERA_5M)
    #define SPRD_ION_SIZE   (19*1024*1024)
    #elif defined(CONFIG_CAMERA_3M)
    #define SPRD_ION_SIZE   (13*1024*1024)
    #elif defined(CONFIG_CAMERA_2M)
        #ifdef CONFIG_CAMERA_ROTATION
        #define SPRD_ION_SIZE   (13*1024*1024)
        #else
        #define SPRD_ION_SIZE   (8*1024*1024)
        #endif
    #else
    #define SPRD_ION_SIZE   (19*1024*1024)
    #endif

    #define SPRD_ION_OVERLAY_SIZE   (CONFIG_SPRD_ION_OVERLAY_SIZE*SZ_1M)
#else
#define SPRD_ION_SIZE           (0*SZ_1M)
#define SPRD_ION_OVERLAY_SIZE   (0*SZ_1M)
#endif

#ifdef CONFIG_ANDROID_RAM_CONSOLE
#define SPRD_RAM_CONSOLE_SIZE   (2*SZ_1M)
#endif

#define SPRD_IO_MEM_SIZE	(SPRD_PMEM_SIZE+SPRD_PMEM_ADSP_SIZE+ \
				SPRD_ROT_MEM_SIZE+SPRD_SCALE_MEM_SIZE+ \
				SPRD_ION_SIZE+SPRD_ION_OVERLAY_SIZE+SPRD_RAM_CONSOLE_SIZE)

#define SPRD_PMEM_BASE		((256*SZ_1M)-SPRD_IO_MEM_SIZE)
#define SPRD_PMEM_ADSP_BASE	(SPRD_PMEM_BASE+SPRD_PMEM_SIZE)
#define SPRD_ROT_MEM_BASE	(SPRD_PMEM_ADSP_BASE+SPRD_PMEM_ADSP_SIZE)
#define SPRD_SCALE_MEM_BASE	(SPRD_ROT_MEM_BASE+SPRD_ROT_MEM_SIZE)
#define SPRD_ION_BASE           (SPRD_SCALE_MEM_BASE+SPRD_SCALE_MEM_SIZE)
#define SPRD_ION_OVERLAY_BASE   (SPRD_ION_BASE+SPRD_ION_SIZE)
#define SPRD_RAM_CONSOLE_START (SPRD_ION_OVERLAY_BASE+SPRD_ION_OVERLAY_SIZE)


#endif

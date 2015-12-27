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
#ifndef _ROTATION_REG_H_
#define _ROTATION_REG_H_

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/io.h>
#include <mach/io.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

typedef enum {
	ROTATION_INT_ROTATION_DONE = 0,
	ROTATION_INT_MAX
} ROTATON_INT_TYPE_E;

typedef enum {
	ROTATION_DATA_YUV = 0,
	ROTATION_DATA_RGB,
	ROTATION_DATA_MAX
} ROTATION_DATA_MODE_E;

typedef enum {
	ROTATION_ONE_BYTE = 0,
	ROTATION_TWO_BYTES,
	ROTATION_FOUR_BYTES,
	ROTATION_BYTE_MAX
} ROTATION_PIXEL_FORMAT_E;

typedef enum {
	ROTATION_NORMAL = 0,
	ROTATION_UV422,
	ROTATION_DATA_FORMAT_MAX
} ROTATION_UV_MODE_E;

#define __pad(a) __raw_readl(a)
#define _pard(a) __raw_readl(a)
#define _pawd(a,v) __raw_writel(v,a)
#define _paad(a,v) __raw_writel((__raw_readl(a) & v), a)
#define _paod(a,v) __raw_writel((__raw_readl(a) | v), a)
#define _pacd(a,v) __raw_writel((__raw_readl(a) ^ v), a)
#define _pamd(a,m,v) do{uint32 _tmp=__raw_readl(a); _tmp &= ~(m); __raw_writel(_tmp|((m)&(v)), (a));}while(0)

#define AHB_BASE SPRD_AHB_BASE	//0xE000A000 <--> 0x20900000UL
#define AHB_GLOBAL_REG_CTL0 AHB_BASE + 0x200UL
#define AHB_GLOBAL_REG_CTL1 AHB_BASE + 0x204UL
#define AHB_GLOBAL_REG_SOFTRST AHB_BASE + 0x210UL

#define REG_ROTATION_REG_BASE SPRD_ROTO_BASE	//0xE0009000 <-->0x20800000
//#define REG_ROTATION_REG_BASE               0x20800000
#ifndef CONFIG_ARCH_SC8810
#define REG_ROTATION_SRC_ADDR           (REG_ROTATION_REG_BASE + 0x0200)
#define REG_ROTATION_DST_ADDR           (REG_ROTATION_REG_BASE + 0x0204)
#define REG_ROTATION_IMG_SIZE           (REG_ROTATION_REG_BASE + 0x0208)
#define REG_ROTATION_CTRL               (REG_ROTATION_REG_BASE + 0x020c)
#define REG_ROTATION_DMA_CHN_CFG0       (REG_ROTATION_REG_BASE + 0x0210)
#define REG_ROTATION_DMA_CHN_CFG1       (REG_ROTATION_REG_BASE + 0x0214)
#define REG_ROTATION_DMA_CHN_SRC_ADDR   (REG_ROTATION_REG_BASE + 0x0218)
#define REG_ROTATION_DMA_CHN_DST_ADDR   (REG_ROTATION_REG_BASE + 0x021c)
#define REG_ROTATION_DMA_CHN_LLPTR      (REG_ROTATION_REG_BASE + 0x0220)
#define REG_ROTATION_DMA_CHN_SDI        (REG_ROTATION_REG_BASE + 0x0224)
#define REG_ROTATION_DMA_CHN_SBI        (REG_ROTATION_REG_BASE + 0x0228)
#define REG_ROTATION_DMA_CHN_DBI        (REG_ROTATION_REG_BASE + 0x022c)
#else
#define REG_ROTATION_SRC_ADDR                         (REG_ROTATION_REG_BASE + 0x0400)
#define REG_ROTATION_DST_ADDR                         (REG_ROTATION_REG_BASE + 0x0404)
#define REG_ROTATION_IMG_SIZE                            (REG_ROTATION_REG_BASE + 0x0408)
#define REG_ROTATION_CTRL                                    (REG_ROTATION_REG_BASE + 0x040c)
#define REG_ROTATION_ORIGWIDTH                      (REG_ROTATION_REG_BASE + 0x0410)
#define REG_ROTATION_OFFSET                               (REG_ROTATION_REG_BASE + 0x0414)
#define REG_ROTATION_DMA_CHN_CFG0              (REG_ROTATION_REG_BASE + 0x0420)
#define REG_ROTATION_DMA_CHN_CFG1   		 (REG_ROTATION_REG_BASE + 0x0424)
#define REG_ROTATION_DMA_CHN_SRC_ADDR   (REG_ROTATION_REG_BASE + 0x0428)
#define REG_ROTATION_DMA_CHN_DST_ADDR   (REG_ROTATION_REG_BASE + 0x042c)
#define REG_ROTATION_DMA_CHN_LLPTR            (REG_ROTATION_REG_BASE + 0x0430)
#define REG_ROTATION_DMA_CHN_SDI                 (REG_ROTATION_REG_BASE + 0x0434)
#define REG_ROTATION_DMA_CHN_SBI	          (REG_ROTATION_REG_BASE + 0x0438)
#define REG_ROTATION_DMA_CHN_DBI	          (REG_ROTATION_REG_BASE + 0x043c)
#endif

#endif //_ROTATION_REG_H_

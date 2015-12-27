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
#ifndef _ROT_REG_H_
#define _ROT_REG_H_

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/io.h>
#include <mach/io.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

typedef enum {
	ROT_INT_ROTATION_DONE = 0,
	ROT_INT_MAX
} ROT_INT_TYPE_E;

typedef enum {
	ROT_DATA_YUV = 0,
	ROT_DATA_RGB,
	ROT_DATA_MAX
} ROT_DATA_MODE_E;

typedef enum {
	ROT_ONE_BYTE = 0,
	ROT_TWO_BYTES,
	ROT_FOUR_BYTES,
	ROT_BYTE_MAX
} ROT_PIXEL_FORMAT_E;

typedef enum {
	ROT_NORMAL = 0,
	ROT_UV422,
	ROT_DATA_FORMAT_MAX
} ROT_UV_MODE_E;

#define AHB_BASE                                 SPRD_AHB_BASE
#define AHB_GLOBAL_REG_CTL0                      (AHB_BASE + 0x200UL)
#define AHB_GLOBAL_REG_CTL1                      (AHB_BASE + 0x204UL)
#define AHB_GLOBAL_REG_SOFTRST                   (AHB_BASE + 0x210UL)
#define REG_ROTATION_REG_BASE                    SPRD_ROTO_BASE
#define REG_ROTATION_SRC_ADDR                    (REG_ROTATION_REG_BASE + 0x0400)
#define REG_ROTATION_DST_ADDR                    (REG_ROTATION_REG_BASE + 0x0404)
#define REG_ROTATION_IMG_SIZE                    (REG_ROTATION_REG_BASE + 0x0408)
#define REG_ROTATION_CTRL                        (REG_ROTATION_REG_BASE + 0x040c)
#define REG_ROTATION_ORIGWIDTH                   (REG_ROTATION_REG_BASE + 0x0410)
#define REG_ROTATION_OFFSET                      (REG_ROTATION_REG_BASE + 0x0414)
#define REG_ROTATION_DMA_CHN_CFG0                (REG_ROTATION_REG_BASE + 0x0420)
#define REG_ROTATION_DMA_CHN_CFG1   		 (REG_ROTATION_REG_BASE + 0x0424)
#define REG_ROTATION_DMA_CHN_SRC_ADDR            (REG_ROTATION_REG_BASE + 0x0428)
#define REG_ROTATION_DMA_CHN_DST_ADDR            (REG_ROTATION_REG_BASE + 0x042c)
#define REG_ROTATION_DMA_CHN_LLPTR               (REG_ROTATION_REG_BASE + 0x0430)
#define REG_ROTATION_DMA_CHN_SDI                 (REG_ROTATION_REG_BASE + 0x0434)
#define REG_ROTATION_DMA_CHN_SBI	         (REG_ROTATION_REG_BASE + 0x0438)
#define REG_ROTATION_DMA_CHN_DBI	         (REG_ROTATION_REG_BASE + 0x043c)

#endif //_ROT_REG_H_

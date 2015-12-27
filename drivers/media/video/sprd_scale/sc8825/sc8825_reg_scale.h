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
#ifndef _REG_SCALE_TIGER_H_
#define _REG_SCALE_TIGER_H_

#include <mach/globalregs.h>

#ifdef   __cplusplus
extern   "C"
{
#endif


#define SCALE_BASE                                     SPRD_DCAM_BASE 
#define SCALE_CFG                                      (SCALE_BASE + 0x0018UL)
#define SCALE_SRC_SIZE                                 (SCALE_BASE + 0x001CUL)
#define SCALE_DST_SIZE                                 (SCALE_BASE + 0x0020UL)
#define SCALE_TRIM_START                               (SCALE_BASE + 0x0024UL)
#define SCALE_TRIM_SIZE                                (SCALE_BASE + 0x0028UL)
#define SCALE_SLICE_VER                                (SCALE_BASE + 0x002CUL)
#define SCALE_INT_STS                                  (SCALE_BASE + 0x0030UL)
#define SCALE_INT_MASK                                 (SCALE_BASE + 0x0034UL)
#define SCALE_INT_CLR                                  (SCALE_BASE + 0x0038UL)
#define SCALE_INT_RAW                                  (SCALE_BASE + 0x003CUL)
#define SCALE_FRM_IN_Y                                 (SCALE_BASE + 0x0040UL)
#define SCALE_FRM_IN_U                                 (SCALE_BASE + 0x0044UL)
#define SCALE_FRM_IN_V                                 (SCALE_BASE + 0x0048UL)
#define SCALE_FRM_OUT_Y                                (SCALE_BASE + 0x0050UL)
#define SCALE_FRM_OUT_U                                (SCALE_BASE + 0x0054UL)
#define SCALE_FRM_SWAP_Y                               (SCALE_BASE + 0x0048UL)
#define SCALE_FRM_SWAP_U                               (SCALE_BASE + 0x004CUL)
#define SCALE_FRM_LINE                                 (SCALE_BASE + 0x0058UL)
#define SCALE_ENDIAN_SEL                               (SCALE_BASE + 0x0064UL)
#define SCALE_REG_START                                (SCALE_BASE + 0x0010UL)
#define SCALE_REG_END                                  (SCALE_BASE + 0x0070UL)


#define SCALE_FRAME_WIDTH_MAX                          4092
#define SCALE_FRAME_HEIGHT_MAX                         4092
#define SCALE_SC_COEFF_MAX                             4
#define SCALE_DECI_FAC_MAX                             4
#define SCALE_LINE_BUF_LENGTH                          1280
#define SCALE_IRQ_BIT                                  (1 << 9)
#define SCALE_IRQ                                      IRQ_DCAM_INT

#ifdef   __cplusplus
}
#endif

#endif //_REG_SCALE_TIGER_H_

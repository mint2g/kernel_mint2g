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
#ifndef _SCALE_DRV_SC8810_H_
#define _SCALE_DRV_SC8810_H_

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/io.h>
#include <mach/io.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include "../sprd_dcam/sc8810/sc8810_reg_isp.h"
#include "gen_scale_coef.h"
#include "../sprd_dcam/common/isp_control.h"

#define __pad(a) __raw_readl(a)
#define _pard(a) __raw_readl(a)
#define _pawd(a,v) __raw_writel(v,a)
#define _paad(a,v) __raw_writel((__raw_readl(a) & v), a)
#define _paod(a,v) __raw_writel((__raw_readl(a) | v), a)
#define _pacd(a,v) __raw_writel((__raw_readl(a) ^ v), a)
#define _pamd(a,m,v) do{uint32 _tmp=__raw_readl(a); _tmp &= ~(m); __raw_writel(_tmp|((m)&(v)), (a));}while(0)

#define BOOLEAN char
#define PNULL  ((void *)0)
#ifndef NULL
#define NULL 0
#endif

#define SCALE_Sleep msleep
#define SCALE_SUCCESS 1
#define SCALE_FAIL 0
#define SCALE_FALSE 0
#define SCALE_TRUE 1
#define SCALE_NULL 0
#ifndef SCALE_ASSERT
#define SCALE_ASSERT(a) do{}while(!(a));
#endif
#ifndef SCALE_MEMCPY
#define SCALE_MEMCPY memcpy
#endif
#ifndef SCALE_TRACE
#define SCALE_TRACE printk
#endif
typedef void *DCAM_MUTEX_PTR;
typedef void *DCAM_SEMAPHORE_PTR;
typedef void *DCAM_TIMER_PTR;

//0xE0002000UL <-->0x20200000
#define DCAM_REG_BASE SPRD_ISP_BASE

//0xE0002E00UL <--> 0x8b000000
#define GLOBAL_BASE SPRD_GREG_BASE
#define ARM_GLOBAL_REG_GEN0 GLOBAL_BASE + 0x008UL
#define ARM_GLOBAL_REG_GEN3 GLOBAL_BASE + 0x01CUL
#define ARM_GLOBAL_PLL_SCR GLOBAL_BASE + 0x070UL
#define GR_CLK_GEN5 GLOBAL_BASE + 0x07CUL

//0xE000A000 <--> 0x20900000UL
#define AHB_BASE SPRD_AHB_BASE
#define AHB_GLOBAL_REG_CTL0 AHB_BASE + 0x200UL
#define AHB_GLOBAL_REG_SOFTRST AHB_BASE + 0x210UL

//0xE0021000<-->0x80003000
#define IRQ_BASE SPRD_ASHB_BASE
#define SCL_INT_IRQ_EN IRQ_BASE + 0x008UL
#define SCL_INT_IRQ_DISABLE IRQ_BASE + 0x00CUL

#define ISP_PATH_SC_COEFF_MAX                           4
#define ISP_SCALE_FRAME_MODE_WIDTH_TH     960
#define ISP_SCALE_COEFF_H_NUM            48
#define ISP_SCALE_COEFF_V_NUM             68

#define ISP_AHB_SLAVE_ADDR                               DCAM_REG_BASE
#define ISP_AHB_CTRL_MOD_EN_OFFSET           0
#define ISP_AHB_CTRL_MEM_SW_OFFSET           4
#define ISP_AHB_CTRL_SOFT_RESET_OFFSET    0x10

#define ISP_SCALE1_H_TAB_OFFSET                        0x200
#define ISP_SCALE1_V_TAB_OFFSET                        0x2F0
#define ISP_SCALE2_H_TAB_OFFSET                        0x400
#define ISP_SCALE2_V_TAB_OFFSET                        0x4F0

#define ISP_IRQ_SCL_LINE_MASK	0x200UL	//0x000003FFUL
#define ISP_IRQ_SENSOR_SOF_BIT	BIT(0)
#define ISP_IRQ_SENSOR_EOF_BIT	BIT(1)
#define ISP_IRQ_CAP_SOF_BIT		BIT(2)
#define ISP_IRQ_CAP_EOF_BIT		BIT(3)
#define ISP_IRQ_CMR_DONE_BIT     BIT(4)
#define ISP_IRQ_CAP_BUF_OV_BIT   BIT(5)
#define ISP_IRQ_SENSOR_LE_BIT      BIT(6)
#define ISP_IRQ_SENSOR_FE_BIT      BIT(7)
#define ISP_IRQ_JPG_BUF_OV_BIT    BIT(8)
#define ISP_IRQ_REVIEW_DONE_BIT	BIT(9)

#define ISP_PATH_FRAME_HIGH_BITS                       6
#define ISP_PATH_FRAME_WIDTH_MAX                    4092
#define ISP_PATH_FRAME_HEIGHT_MAX                  4092
#define ISP_PATH_SUB_SAMPLE_MAX                        3	//0.....1/2, 1......1/4, 2......1/8, 3.......1/16
#define ISP_PATH_SUB_SAMPLE_FACTOR_BASE     1	// no subsample
#define ISP_PATH_SCALE_LEVEL                                    64
#define ISP_PATH_SCALE_LEVEL_MAX                         256
#define ISP_PATH_SCALE_LEVEL_MIN                         16
#define ISP_PATH_SLICE_MASK                                     0xFFF

typedef struct _isp_size_tag {
	uint32_t w;
	uint32_t h;
} ISP_SIZE_T;

typedef struct _isp_rect_tag {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
} ISP_RECT_T;

typedef enum {
	ISP_SCALE_NOEMAL = 0,
	ISP_SCALE_SLICE,
	ISP_SCALE__MODE_MAX
} ISP_SCALE_MODE_E;

typedef enum {
	ISP_DATA_YUV422 = 0,
	ISP_DATA_YUV420,
	ISP_DATA_YUV400,
	ISP_DATA_YUV420_3FRAME,
	ISP_DATA_RGB565,
	ISP_DATA_RGB888,
	ISP_DATA_CCIR656,
	ISP_DATA_JPEG,
	ISP_DATA_MAX
} ISP_DATA_FORMAT_E;

enum {
	ISP_PATH_DATA_FORMAT_YUV = 0,
	ISP_PATH_DATA_FORMAT_RGB
};
typedef struct _isp_data_addr_tag {
	uint32_t yaddr;
	uint32_t uaddr;
	uint32_t vaddr;
} ISP_ADDRESS_T;

typedef struct _isp_frame_t {
	uint32_t type;
	uint32_t lock;
	uint32_t flags;
	uint32_t fid;
	uint32_t afval;
	uint32_t width;
	uint32_t height;
	uint32_t yaddr;
	uint32_t uaddr;
	uint32_t vaddr;
	uint32_t rgbaddr;
	struct _isp_frame_t *prev;
	struct _isp_frame_t *next;
} ISP_FRAME_T;

typedef struct _isp_path_desc_tag {
	ISP_SIZE_T input_size;
	ISP_RECT_T input_rect;
	ISP_RECT_T input_range;
	ISP_SIZE_T sc_input_size;
	ISP_SIZE_T output_size;
	ISP_FRAME_T input_frame;
	ISP_FRAME_T output_frame;
	uint32_t input_format;
	ISP_FRAME_T *p_output_frame_head;
	ISP_FRAME_T *p_output_frame_cur;
	uint32_t output_frame_count;
	uint32_t output_format;
	uint32_t output_frame_flag;
	ISP_ENDIAN_T input_frame_endian;
	ISP_ENDIAN_T output_frame_endian;
	ISP_FRAME_T swap_frame;
	ISP_FRAME_T line_frame;
	uint32_t scale_en;
	uint32_t sub_sample_en;
	uint32_t sub_sample_factor;
	uint32_t sub_sample_mode;
	uint32_t slice_en;
	uint32_t slice_height;
	uint32_t slice_count;
	uint32_t slice_line_count;
	uint32_t is_last_slice;
	uint32_t h_scale_coeff;
	uint32_t v_scale_coeff;
} ISP_PATH_DESCRIPTION_T;

typedef void (*ISP_ISR_FUNC_PTR) (void *);

typedef enum {
	ISP_MODE_IDLE = 0,
	ISP_MODE_CAPTURE,
	ISP_MODE_PREVIEW,
	ISP_MODE_PREVIEW_EX,
	ISP_MODE_REVIEW,
	ISP_MODE_SCALE,
	ISP_MODE_MPEG,
	ISP_MODE_VT,
	ISP_MODE_VT_REVIEW,
	ISP_MODE_MAX
} ISP_MODE_E;

enum {
	ISP_IRQ_SENSOR_SOF = 0,
	ISP_IRQ_SENSOR_EOF,
	ISP_IRQ_CAP_SOF,
	ISP_IRQ_CAP_EOF,
	ISP_IRQ_PATH1_DONE,
	ISP_IRQ_CAP_FIFO_OF,
	ISP_IRQ_SENSOR_LINE_ERR,
	ISP_IRQ_SENSOR_FRAME_ERR,
	ISP_IRQ_JPEG_BUF_OF,
	ISP_IRQ_PATH2_DONE,
	ISP_IRQ_NUMBER
};

typedef struct _isp_cap_desc_tag {
	uint32_t input_format;
	uint32_t frame_deci_factor;
	uint32_t img_x_deci_factor;
	uint32_t img_y_deci_factor;
} ISP_CAP_DESCRIPTION_T;

typedef struct _isp_module_tagss {
	ISP_MODE_E isp_mode;
	uint32_t module_addr;
	ISP_CAP_DESCRIPTION_T isp_cap;
	ISP_PATH_DESCRIPTION_T isp_path1;
	ISP_PATH_DESCRIPTION_T isp_path2;
	ISP_ISR_FUNC_PTR user_func[ISP_IRQ_NUMBER];
} ISP_MODULE_T;

typedef enum {
	ISP_IRQ_NOTICE_SENSOR_SOF = 0,
	ISP_IRQ_NOTICE_SENSOR_EOF,
	ISP_IRQ_NOTICE_CAP_SOF,
	ISP_IRQ_NOTICE_CAP_EOF,
	ISP_IRQ_NOTICE_PATH1_DONE,
	ISP_IRQ_NOTICE_CAP_FIFO_OF,
	ISP_IRQ_NOTICE_SENSOR_LINE_ERR,
	ISP_IRQ_NOTICE_SENSOR_FRAME_ERR,
	ISP_IRQ_NOTICE_JPEG_BUF_OF,
	ISP_IRQ_NOTICE_PATH2_DONE,
	ISP_IRQ_NOTICE_NUMBER,
} ISP_IRQ_NOTICE_ID_E;

enum {
	ISP_FRAME_UNLOCK = 0,
	ISP_FRAME_LOCK_WRITE = 0x10011001,
	ISP_FRAME_LOCK_READ = 0x01100110
};

enum {
	ISP_MASTER_READ,
	ISP_MASTER_WRITE,
	ISP_MASTER_MAX
};

enum {
	ISP_MASTER_ENDIANNESS_BIG,
	ISP_MASTER_ENDIANNESS_LITTLE,
	ISP_MASTER_ENDIANNESS_HALFBIG,
	ISP_MASTER_ENDIANNESS_MAX
};

typedef enum {
	ISP_DRV_RTN_SUCCESS = 0,
	ISP_DRV_RTN_PARA_ERR = 0x10,
	ISP_DRV_RTN_IO_ID_UNSUPPORTED,
	ISP_DRV_RTN_ISR_NOTICE_ID_ERR,
	ISP_DRV_RTN_MASTER_SEL_ERR,
	ISP_DRV_RTN_MODE_ERR,

	ISP_DRV_RTN_CAP_FRAME_SEL_ERR = 0x20,
	ISP_DRV_RTN_CAP_INPUT_FORMAT_ERR,
	ISP_DRV_RTN_CAP_INPUT_YUV_ERR,
	ISP_DRV_RTN_CAP_SYNC_POL_ERR,
	ISP_DRV_RTN_CAP_FIFO_DATA_RATE_ERR,
	ISP_DRV_RTN_CAP_SKIP_FRAME_TOO_MANY,
	ISP_DRV_RTN_CAP_FRAME_DECI_FACTOR_ERR,
	ISP_DRV_RTN_CAP_XY_DECI_FACTOR_ERR,
	ISP_DRV_RTN_CAP_FRAME_SIZE_ERR,
	ISP_DRV_RTN_CAP_JPEG_DROP_NUM_ERR,

	ISP_DRV_RTN_PATH_SRC_SIZE_ERR = 0x30,
	ISP_DRV_RTN_PATH_TRIM_SIZE_ERR,
	ISP_DRV_RTN_PATH_DES_SIZE_ERR,
	ISP_DRV_RTN_PATH_INPUT_FORMAT_ERR,
	ISP_DRV_RTN_PATH_OUTPUT_FORMAT_ERR,
	ISP_DRV_RTN_PATH_SC_COEFF_ERR,
	ISP_DRV_RTN_PATH_SUB_SAMPLE_ERR,
	ISP_DRV_RTN_PATH_ADDR_INVALIDE,
	ISP_DRV_RTN_PATH_FRAME_TOO_MANY,
	ISP_DRV_RTN_PATH_FRAME_LOCKED,
	ISP_DRV_RTN_KMALLOC_BUF_ERR,
	ISP_DRV_RTN_GEN_SCALECOEFF_ERR,
	ISP_DRV_RTN_ROTATION_ANGLE_ERR,
	ISP_DRV_RTN_MAX
} ISP_DRV_RTN_E;

typedef enum {
	ISP_PATH_INPUT_FORMAT = 0,
	ISP_PATH_INPUT_SIZE,
	ISP_PATH_INPUT_RECT,
	ISP_PATH_INPUT_ADDR,
	ISP_PATH_OUTPUT_SIZE,
	ISP_PATH_OUTPUT_FORMAT,
	ISP_PATH_OUTPUT_ADDR,
	ISP_PATH_OUTPUT_FRAME_FLAG,
	ISP_PATH_SWAP_BUFF,
	ISP_PATH_LINE_BUFF,
	ISP_PATH_SUB_SAMPLE_EN,
	ISP_PATH_SUB_SAMPLE_MOD,
	ISP_PATH_SLICE_SCALE_EN,
	ISP_PATH_SLICE_SCALE_HEIGHT,
	ISP_PATH_DITHER_EN,
	ISP_PATH_IS_IN_SCALE_RANGE,
	ISP_PATH_IS_SCALE_EN,
	ISP_PATH_SLICE_OUT_HEIGHT,
	ISP_PATH_MODE,
	ISP_PATH_INPUT_ENDIAN,
	ISP_PATH_OUTPUT_ENDIAN,
	ISP_PATH_ROT_MODE,
	ISP_PATH_INPUT_RECT_PHY,
	ISP_CFG_ID_E_MAX
} ISP_CFG_ID_E;

typedef enum
{
	ISP_ROTATION_0 = 0,
	ISP_ROTATION_90,
	ISP_ROTATION_180,
	ISP_ROTATION_270,
	ISP_ROTATION_MIRROR,
	ISP_ROTATION_MAX
}ISP_ROTATION_E;

#define ISP_CLK_DOMAIN_AHB                             1
#define ISP_CLK_DOMAIN_DCAM                          0

#endif //_SCALE_DRV_SC8810_H_

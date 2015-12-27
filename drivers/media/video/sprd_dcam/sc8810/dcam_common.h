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

#ifndef _DCAM_COMMON_H_
#define _DCAM_COMMON_H_
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/io.h>
#include <mach/io.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#define __pad(a) __raw_readl(a)
#define _pard(a) __raw_readl(a)
#define _pawd(a,v) __raw_writel(v,a)
#define _paad(a,v) __raw_writel((__raw_readl(a) & v), a)
#define _paod(a,v) __raw_writel((__raw_readl(a) | v), a)
#define _pacd(a,v) __raw_writel((__raw_readl(a) ^ v), a)
#define _pamd(a,m,v) do{uint32 _tmp=__raw_readl(a); _tmp &= ~(m); __raw_writel(_tmp|((m)&(v)), (a));}while(0)

/*#define DCAM_DEBUG*/
#define DCAM_TRACE  pr_debug
#define DCAM_TRACE_ERR printk
#define DCAM_TRACE_HIGH printk
#define BOOLEAN char
#define PNULL  ((void *)0)
#define LOCAL static
#define DCAM_NULL 0
#define DCAM_Sleep msleep
#define DCAM_SUCCESS 0
#define DCAM_FAIL 1
#define DCAM_FALSE 0
#define DCAM_TRUE 1
#ifndef DCAM_ASSERT
#define DCAM_ASSERT(a) do{}while(!(a));
#endif
#ifndef DCAM_MEMCPY
#define DCAM_MEMCPY memcpy
#endif

typedef void *DCAM_MUTEX_PTR;
typedef void *DCAM_SEMAPHORE_PTR;
typedef void *DCAM_TIMER_PTR;

typedef enum {
	ISP_ROTATION_0 = 0,
	ISP_ROTATION_90,
	ISP_ROTATION_180,
	ISP_ROTATION_270,
	ISP_ROTATION_MIRROR,
	ISP_ROTATION_MAX
} ISP_ROTATION_E;

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
	ISP_DRV_RTN_CAP_SENSOR_MODE_ERR,	/*will be discarded! */
	ISP_DRV_RTN_CAP_ID_ENDIAN_ERR,
	ISP_DRV_RTN_CAP_IF_MODE_ERR,
	ISP_DRV_RTN_CAP_SPI_WIDTH_ERR,
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

typedef enum {
	ISP_MODE_IDLE = 0,
	ISP_MODE_CAPTURE,
	ISP_MODE_CAPTURE_EX,
	ISP_MODE_PREVIEW,
	ISP_MODE_PREVIEW_EX,
	ISP_MODE_REVIEW,
	ISP_MODE_SCALE,
	ISP_MODE_MPEG,
	ISP_MODE_VT,
	ISP_MODE_VT_REVIEW,
	ISP_MODE_MAX
} ISP_MODE_E;

typedef enum {
	ISP_CAP_CCIR656_FRAME_SEL_ODD = 0,
	ISP_CAP_CCIR656_FRAME_SEL_EVEN,
	ISP_CAP_CCIR656_FRAME_SEL_BOTH,
	ISP_CAP_CCIR656_FRAME_SEL_MAX
} ISP_CAP_CCIR656_FRAME_SEL_E;

typedef enum {
	ISP_CAP_INPUT_FORMAT_YUV = 0,
	ISP_CAP_INPUT_FORMAT_JPEG,
	ISP_CAP_INPUT_FORMAT_MAX
} ISP_CAP_INPUT_FORMAT_E;

typedef enum {
	ISP_DCAM_PATH1_OUT_FORMAT_YUV422 = 0,
	ISP_DCAM_PATH1_OUT_FORMAT_YUV420,
	ISP_DCAM_PATH1_OUT_FORMAT_RGB,
	ISP_DCAM_PATH1_OUT_FORMAT_JPEG,
	ISP_DCAM_PATH1_OUT_FORMAT_MAX
} ISP_DCAM_PATH1_OUT_FORMAT_E;

typedef enum {
	ISP_PATTERN_YUYV = 0,
	ISP_PATTERN_YVYU,
	ISP_PATTERN_UYVY,
	ISP_PATTERN_VYUY,
	ISP_PATTERN_MAX
} ISP_CAP_PATTERN_E;

typedef enum {
	ISP_CAP_FIFO_DATA_RATE_FULL = 0,
	ISP_CAP_FIFO_DATA_RATE_HALF,
	ISP_CAP_FIFO_DATA_RATE_RES,
	ISP_CAP_FIFO_DATA_RATE_DOUBLE,
	ISP_CAP_FIFO_DATA_RATE_MAX
} ISP_CAP_FIFO_DATA_RATE_E;

typedef enum {
	ISP_CAPTURE_MODE_SINGLE = 0,
	ISP_CAPTURE_MODE_MULTIPLE,
	ISP_CAPTURE__MODE_MAX
} ISP_CAPTURE_MODE_E;

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

typedef enum {
	ISP_CAP_CCIR565_ENABLE = 0,
	ISP_CAP_TV_FRAME_SEL,
	ISP_CAP_SYNC_POL,
	ISP_CAP_INPUT_FORMAT,
	ISP_CAP_YUV_TYPE,
	ISP_CAP_FIFO_DATA_RATE,
	ISP_CAP_PRE_SKIP_CNT,
	ISP_CAP_FRM_DECI,
	ISP_CAP_FRM_COUNT_CLR,
	ISP_CAP_FRM_COUNT_GET,
	ISP_CAP_INPUT_RECT,
	ISP_CAP_IMAGE_XY_DECI,
	ISP_CAP_JPEG_GET_NUM,
	ISP_CAP_JPEG_DROP_NUM,
	ISP_CAP_JPEG_GET_LENGTH,
	ISP_CAP_JPEG_MEM_IN_16K,
	ISP_CAP_SENSOR_MODE,
	ISP_CAP_IF_ENDIAN,
	ISP_CAP_IF_MODE,
	ISP_CAP_SPI_ORIG_WIDTH,
	ISP_PATH_INPUT_FORMAT,
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
	ISP_PATH_UV420_AVG_EN,
	ISP_PATH_DATA_ENDIAN,
	ISP_PATH_ROT_MODE,
	ISP_CFG_ID_E_MAX
} ISP_CFG_ID_E;

typedef enum {
	ISP_SUB_2 = 0,
	ISP_SUB_4,
	ISP_SUB_8,
	ISP_SUB_16,
	ISP_SUB_MAX
} ISP_SUB_SAMPLE_MODE_E;

typedef enum {
	ISP_CAP_IMG_DEC_MODE_DIRECT = 0,
	ISP_CAP_IMG_DEC_MODE_AVERAGE
} ISP_CAP_IMA_DEC_MODE_E;

typedef enum {
	ISP_SCALE_NOEMAL = 0,
	ISP_SCALE_SLICE,
	ISP_SCALE__MODE_MAX
} ISP_SCALE_MODE_E;

typedef enum {
	ISP_AHB_FRAME_SRC,
	ISP_AHB_FRAME_PATH1_DST,
	ISP_AHB_FRAME_PATH2_DST,
	ISP_AHB_FRAME_SWAP,
	ISP_AHB_FRAME_LINE,
	ISP_AHB_FRAME_MAX
} ISP_AHB_FRAME_E;

enum {
	IRAM_FOR_ISP = 0,
	IRAM_FOR_ARM
};

typedef enum {
	ISP_CLK_128M = 0,
	ISP_CLK_76M8,
	ISP_CLK_64M,
	ISP_CLK_48M
} ISP_CLK_SEL_E;

typedef enum {
	ISP_CAP_IF_8BITS = 0,
	ISP_CAP_IF_4BITS,
	ISP_CAP_IF_2BITS,
	ISP_CAP_IF_1BITS,
	ISP_CAP_IF_MODE_MAX
} ISP_CAP_IF_MODE_E;

typedef enum {
	ISP_CAP_MODE_YUV = 0,
	ISP_CAP_MODE_SPI,
	ISP_CAP_MODE_JPEG,
	ISP_CAP_MODE_RAWRGB,
	ISP_CAP_MODE_MAX
} ISP_CAP_SENSOR_MODE_E;

typedef enum {
	ISP_CAP_IF_ENDIAN_BE = 0,
	ISP_CAP_IF_ENDIAN_LE,
	ISP_CAP_IF_ENDIAN_MAX
} ISP_CAP_IF_ENDIAN_E;

typedef struct _isp_cap_sync_pol_tag {
	uint8_t vsync_pol;
	uint8_t hsync_pol;
	uint8_t pclk_pol;
	uint8_t rsd;
} ISP_CAP_SYNC_POL_T;

typedef struct _isp_cap_dec_tag {
	uint8_t x_factor;
	uint8_t y_factor;
	uint8_t x_mode;
	uint8_t reserved;
} ISP_CAP_DEC_T;

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

typedef enum {
	ISR_DONE = 0x0,
	CALL_HISR = 0x5a5
} ISR_EXE_T;
#endif

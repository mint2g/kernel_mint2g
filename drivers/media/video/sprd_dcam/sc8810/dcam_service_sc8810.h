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
#ifndef _DCAM_SERVICE_SC8810_H_
#define _DCAM_SERVICE_SC8810_H_

#include "dcam_drv_sc8810.h"
#include "../common/sensor_drv.h"

#define ISP_DISPLAY_NONE                0xFF

typedef enum {
	DCAM_DATA_YUV422 = 0,
	DCAM_DATA_YUV420,
	DCAM_DATA_RGB,
	DCAM_DATA_JPEG,
	DCAM_DATA_MAX
} DCAM_DATA_FORMAT_E;

typedef enum {
	DCAM_MODE_TYPE_IDLE = 0,
	DCAM_MODE_TYPE_PREVIEW,
	DCAM_MODE_TYPE_CAPTURE = 3,
	DCAM_MODE_TYPE_REVIEW,
	DCAM_MODE_TYPE_MAX
} DCAM_MODE_TYPE_E;

typedef enum {
	YUV_YUYV = 0,
	YUV_YVYU,
	YUV_UYVY,
	YUV_VYUY,
	YUV_MAX
} DCAM_YUV_PATTERN_E;
typedef enum {
	RGB_565 = 0,
	RGB_RESERVED,
	RGB_666,
	RGB_888,
	RGB_MAX
} RGB_TYPE_E;
typedef enum {
	DCAM_ROTATION_0 = 0,
	DCAM_ROTATION_90,
	DCAM_ROTATION_270,
	DCAM_ROTATION_180,
	DCAM_ROTATION_MAX
} DCAM_ROTATION_E;

typedef struct dcam_size {
	uint32_t w;
	uint32_t h;
} DCAM_SIZE_T0;

typedef struct dcam_polarity {
	uint32_t hsync;
	uint32_t vsync;
	uint32_t pclk;
} DCAM_POLARITY_T;

typedef struct dcam_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
} DCAM_RECT_T0;

typedef struct dcam_init_param {
	DCAM_MODE_TYPE_E mode;
	DCAM_DATA_FORMAT_E format;
	DCAM_YUV_PATTERN_E yuv_pattern;
	RGB_TYPE_E display_rgb_type;
	DCAM_SIZE_T0 input_size;
	DCAM_POLARITY_T polarity;
	DCAM_RECT_T0 input_rect;
	DCAM_RECT_T0 display_rect;
	DCAM_RECT_T0 encoder_rect;
	DCAM_ROTATION_E rotation;
	int skip_frame;
	uint32_t first_buf_addr;
	uint32_t first_u_buf_addr;
	uint32_t zoom_level;
	uint32_t zoom_multiple;
} DCAM_INIT_PARAM_T;

typedef enum {
	DCAM_CB_SENSOR_SOF = 0,
	DCAM_CB_SENSOR_EOF,
	DCAM_CB_CAP_SOF,
	DCAM_CB_CAP_EOF,
	DCAM_CB_PATH1_DONE,
	DCAM_CB_CAP_FIFO_OF,
	DCAM_CB_SENSOR_LINE_ERR,
	DCAM_CB_SENSOR_FRAME_ERR,
	DCAM_CB_JPEG_BUF_OF,
	DCAM_CB_PATH2_DONE,
	DCAM_CB_NUMBER,
} DCAM_CB_ID_E;

typedef int (*get_data) (ISP_ADDRESS_T addr, uint32_t width, uint32_t height);
typedef void (*CALLBACK_FUNC_PTR) (void);

int dcam_open(void);
int dcam_close(void);
int dcam_parameter_init(DCAM_INIT_PARAM_T * init_param);
int dcam_start(void);
int dcam_stop(void);
uint32_t dcam_callback_fun_register(DCAM_CB_ID_E cb_id,
				    CALLBACK_FUNC_PTR user_func);
uint32_t dcam_set_buffer_address(uint32_t yaddr, uint32_t uv_addr);
void dcam_powerdown(uint32_t sensor_id, uint32_t value);
void dcam_reset_sensor(uint32_t value);
int dcam_is_previewing(uint32_t zoom_level);
void dcam_get_jpg_len(uint32_t * len);
void dcam_get_zoom_trim(ISP_RECT_T * trim_rect, uint32_t zoom_level);
void dcam_error_close(void);
void dcam_set_first_buf_addr(uint32_t y_addr, uint32_t uv_addr);
void dcam_enableint(void);
void dcam_disableint(void);
#endif

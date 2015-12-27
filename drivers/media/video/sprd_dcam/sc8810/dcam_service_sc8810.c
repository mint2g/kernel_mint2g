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
#include "dcam_service_sc8810.h"
#include "../common/isp_control.h"
#include <linux/clk.h>
#include <linux/err.h>

#define ISP_ALIGNED_PIXELS 4

typedef struct dcam_parameter {
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
	uint32_t first_buf_uv_addr;
	uint32_t zoom_level;
	uint32_t zoom_multiple;
	uint32_t no_skip_frame_flag;
} DCAM_PARAMETER_T;
DCAM_PARAMETER_T g_dcam_param;

typedef int (*ISP_USER_FUNC_PTR) (void *);

#ifdef DCAM_DEBUG
static void get_dcam_reg(void);
#endif

/*service ID*/
#define ISP_SERVICE_IDLE                      0x00
#define ISP_SERVICE_MPEG                   0x01
#define ISP_SERVICE_JPEG                     0x02
#define ISP_SERVICE_PREVIEW             0x03
#define ISP_SERVICE_SCALE                  0x04
#define ISP_SERVICE_REVIEW               0x07
#define ISP_SERVICE_VP                         0x08
#define ISP_SERVICE_VP_ENCODE      0x09
#define ISP_SERVICE_VP_DECODE      0x10
#define ISP_SERVICE_VP_PLAYBACK  0x17

/*machine state ID*/
#define ISP_STATE_STOP                       1
#define ISP_STATE_PREVIEW                 2
#define ISP_STATE_CAPTURE                3
#define ISP_STATE_PAUSE                     4
#define ISP_STATE_SCALE_DONE        7
#define ISP_STATE_MPEG                      8

/*isp next to when jpeg mode*/
#define ISP_STATE_CAPTURE_DONE          9
#define ISP_STATE_CAPTURE_SCALE         10
#define ISP_STATE_CAPTURE_CFA              11
#define ISP_STATE_VP                             12
#define ISP_STATE_IDLETEMP              13
#define ISP_STATE_REVIEW_DONE         14
#define ISP_STATE_REVIEW_SLICE          15
#define ISP_STATE_SCALE_SLICE             16
#define ISP_STATE_SCALE                    17
#define ISP_STATE_REVIEW                 18
#define ISP_STATE_CLOSED                0xFF
#define DCAMERA_PREVIEW_ZOOM_LEVEL_MAX			    0x08

#define ZOOM_STEP(x) 	     ((x * 2)/4 / DCAMERA_PREVIEW_ZOOM_LEVEL_MAX)

typedef struct _isp_display_block_t {
	uint32_t lcd_id;
	uint32_t block_id;
	ISP_RECT_T img_rect;
	ISP_RECT_T lcd_inv_rect;
	uint32_t img_fmt;
	uint32_t is_enable;
} ISP_DISP_BLOCK_T;

typedef struct _isp_service_t {
	uint32_t service;
	uint32_t state;
	uint32_t module_addr;
	/*for cap */
	uint32_t cap_input_image_format;
	uint32_t cap_input_image_pattern;
	uint32_t vsync_polarity;
	uint32_t hsync_polarity;
	uint32_t pclk_polarity;
	uint32_t is_first_frame;
	uint32_t preview_skip_frame_num;
	uint32_t preview_deci_frame_num;
	uint32_t capture_skip_frame_num;
	ISP_SIZE_T cap_input_size;
	ISP_RECT_T cap_input_range;
	ISP_CAP_DEC_T cap_img_dec;
	ISP_SIZE_T cap_output_size;
	uint32_t ccir656_en;
	uint32_t cap_if_endian;
	uint32_t width_of_spi;
	ISP_CAP_IF_MODE_E cap_if_mode;
	ISP_CAP_SENSOR_MODE_E sensor_mode;
	ISP_DCAM_PATH1_OUT_FORMAT_E dcam_path1_out_format;
	/*for review , scale , vt review */
	ISP_SIZE_T input_size;
	ISP_RECT_T input_range;
	ISP_ADDRESS_T input_addr;
	ISP_ADDRESS_T swap_buff;
	ISP_ADDRESS_T line_buff;
	ISP_ADDRESS_T output_addr;
	uint32_t is_slice;
	uint32_t slice_height;
	uint32_t slice_out_height;
	uint32_t total_scale_out;
	uint32_t total_slice_count;
	uint32_t vt_review_from;	/*0 local, 1, remote */
	uint32_t is_review_dc;
	/*local display(preview,review parameter , include block,round buffer, */
	ISP_DISP_BLOCK_T display_block;
	ISP_RECT_T display_range;	/*preview, review,mpeg preview */
	ISP_ADDRESS_T display_addr[ISP_PATH1_FRAME_COUNT_MAX];
	ISP_FRAME_T display_frame;
	ISP_FRAME_T *display_frame_locked_ptr;
	ISP_ROTATION_E display_rotation;
	ISP_SIZE_T encoder_size;	/*for jpeg capture ,mpeg capture, vt capture, scale out */
	ISP_ADDRESS_T encoder_addr[ISP_PATH2_FRAME_COUNT_MAX];
	ISP_ADDRESS_T encoder_rot_addr[ISP_PATH2_FRAME_COUNT_MAX];
} ISP_SERVICE_T;

#define ISP_RTN_IF_ERR(n)               if(n)  goto exit

static ISP_SERVICE_T s_isp_service;
CALLBACK_FUNC_PTR g_dcam_cb[DCAM_CB_NUMBER];
LOCAL struct clk *g_dcam_clk = NULL;

#ifdef DCAM_DEBUG
static void get_dcam_reg(void)
{
	uint32_t i, value;
	for (i = 0; i < 29; i++) {
		value = _pard(DCAM_REG_BASE + i * 4);
		DCAM_TRACE_HIGH("DCAM reg:0x%x, 0x%x.\n", DCAM_REG_BASE + i * 4,
				value);
	}
	for (i = 0; i < 9; i++) {
		value = _pard(DCAM_REG_BASE + 0x0100 + i * 4);
		DCAM_TRACE_HIGH("DCAM reg:0x%x, 0x%x.\n",
				DCAM_REG_BASE + 0x0100 + i * 4, value);
	}
	for (i = 0; i < 20; i++) {
		value = _pard(AHB_BASE + 0x200 + i * 4);
		DCAM_TRACE_HIGH("DCAM AHB reg:0x%x, 0x%x.\n",
				AHB_BASE + 0x0200 + i * 4, value);
	}
}
#endif

static int dcam_set_mclk(ISP_CLK_SEL_E clk_sel)
{
	char *name_parent = NULL;
	struct clk *clk_parent = NULL;
	int ret;

	switch (clk_sel) {
	case ISP_CLK_128M:
		name_parent = "clk_128m";
		break;
	case ISP_CLK_76M8:
		name_parent = "clk_76m800k";
		break;
	case ISP_CLK_64M:
		name_parent = "clk_64m";
		break;
	default:
		name_parent = "clk_48m";
		break;
	}

	clk_parent = clk_get(NULL, name_parent);
	if (clk_parent && clk_parent != clk_get_parent(g_dcam_clk)) {
		ret = clk_set_parent(g_dcam_clk, clk_parent);
		if (ret) {
			DCAM_TRACE_ERR
			    ("DCAM:dcam_set_mclk,clock: clk_set_parent() failed!\n");
			return -EINVAL;
		}
	}

	ret = clk_enable(g_dcam_clk);
	if (ret) {
		DCAM_TRACE_ERR
		    ("DCAM:dcam_set_mclk,clock: clk_enable() failed!\n");
	} else {
		DCAM_TRACE("DCAM:dcam_set_mclk,clk_enable ok.\n");
	}
	return 0;
}

int dcam_parameter_init(DCAM_INIT_PARAM_T * init_param)
{
	DCAM_TRACE("DCAM: dcam_parameter_init start. \n");
	g_dcam_param.mode = init_param->mode;
	g_dcam_param.format = init_param->format;
	g_dcam_param.yuv_pattern = init_param->yuv_pattern;
	g_dcam_param.display_rgb_type = init_param->display_rgb_type;
	g_dcam_param.input_size.w = init_param->input_size.w;
	g_dcam_param.input_size.h = init_param->input_size.h;
	g_dcam_param.polarity.hsync = init_param->polarity.hsync;
	g_dcam_param.polarity.vsync = init_param->polarity.vsync;
	g_dcam_param.polarity.pclk = init_param->polarity.pclk;
	g_dcam_param.input_rect.x = init_param->input_rect.x;
	g_dcam_param.input_rect.y = init_param->input_rect.y;
	g_dcam_param.input_rect.w = init_param->input_rect.w;
	g_dcam_param.input_rect.h = init_param->input_rect.h;
	g_dcam_param.display_rect.x = init_param->display_rect.x;
	g_dcam_param.display_rect.y = init_param->display_rect.y;
	g_dcam_param.display_rect.w = init_param->display_rect.w;
	g_dcam_param.display_rect.h = init_param->display_rect.h;
	g_dcam_param.encoder_rect.x = init_param->encoder_rect.x;
	g_dcam_param.encoder_rect.y = init_param->encoder_rect.y;
	g_dcam_param.encoder_rect.w = init_param->encoder_rect.w;
	g_dcam_param.encoder_rect.h = init_param->encoder_rect.h;
	g_dcam_param.rotation = init_param->rotation;
	g_dcam_param.skip_frame = init_param->skip_frame;
	g_dcam_param.first_buf_addr = init_param->first_buf_addr;
	g_dcam_param.first_buf_uv_addr = init_param->first_u_buf_addr;
	g_dcam_param.zoom_level = init_param->zoom_level;
	g_dcam_param.zoom_multiple = init_param->zoom_multiple;

	DCAM_TRACE
	    ("DCAM: dcam_parameter_init mode: %d, format: %d, yuv_pattern: %d. \n",
	     g_dcam_param.mode, g_dcam_param.format, g_dcam_param.yuv_pattern);
	DCAM_TRACE
	    ("DCAM: dcam_parameter_init disp w: %d, disp h: %d, input_rect:w: %d, h:%d\n",
	     g_dcam_param.display_rect.w, g_dcam_param.display_rect.h,
	     g_dcam_param.input_rect.w, g_dcam_param.input_rect.h);
	DCAM_TRACE("DCAM: dcam_parameter_init end. \n");
	DCAM_TRACE("DCAM: dcam_parameter_init  input rect:%d,%d,%d,%d\n",
		   g_dcam_param.input_rect.x, g_dcam_param.input_rect.y,
		   g_dcam_param.input_rect.w, g_dcam_param.input_rect.h);
	return 0;
}

static void _ISP_ServiceInit(void)
{
	ISP_SERVICE_T *s = &s_isp_service;

	s->module_addr = ISP_AHB_SLAVE_ADDR;
	s->service = ISP_SERVICE_IDLE;
	s->state = ISP_STATE_STOP;
	s->display_block.block_id = ISP_DISPLAY_NONE;
	return;
}

static void _ISP_ServiceDeInit(void)
{
	ISP_SERVICE_T *s = &s_isp_service;

	s->state = ISP_STATE_CLOSED;
	s->service = ISP_SERVICE_IDLE;

	return;
}

static void _ISP_ServiceOpen(void)
{
	_ISP_ServiceInit();
	ISP_DriverScalingCoeffReset();
}

static uint32_t _ISP_ServiceClose(void)
{
	ISP_SERVICE_T *s = &s_isp_service;

	if (ISP_STATE_CLOSED == s->state) {
		return DCAM_SUCCESS;
	}
	DCAM_TRACE("DCAM:ISP_ServiceClose, service = %d", s->service);

	ISP_DriverStop(s->module_addr);
	/*ISP_DriverSoftReset(AHB_GLOBAL_REG_CTL0);*/

	_ISP_ServiceDeInit();

	return DCAM_SUCCESS;
}

uint32_t dcam_callback_fun_register(DCAM_CB_ID_E cb_id,
				    CALLBACK_FUNC_PTR user_func)
{
	uint32_t rtn = DCAM_SUCCESS;

	if (cb_id >= DCAM_CB_NUMBER) {
		rtn = DCAM_FAIL;
	} else {
		g_dcam_cb[cb_id] = user_func;
	}
	return rtn;
}

static void _ISP_ServiceOnSensorSOF(void *p)
{
	CALLBACK_FUNC_PTR cb_fun = g_dcam_cb[DCAM_CB_SENSOR_SOF];
	if (cb_fun)
		(*cb_fun) ();
}

static void _ISP_ServiceOnSensorEOF(void *p)
{
	DCAM_TRACE("_ISP_ServiceOnSensorEOF\n");
}

static void _ISP_ServiceOnCAPSOF(void *p)
{
	CALLBACK_FUNC_PTR cb_fun = g_dcam_cb[DCAM_CB_CAP_SOF];
	if (cb_fun)
		(*cb_fun) ();
}

static void _ISP_ServiceOnCAPEOF(void *p)
{
	CALLBACK_FUNC_PTR cb_fun = g_dcam_cb[DCAM_CB_CAP_EOF];
	if (cb_fun)
		(*cb_fun) ();
}

static void _ISP_ServiceOnPath1(void *p)
{
	CALLBACK_FUNC_PTR cb_fun = g_dcam_cb[DCAM_CB_PATH1_DONE];
	if (cb_fun)
		(*cb_fun) ();
#ifdef DCAM_DEBUG
	get_dcam_reg();
#endif
}

static void _ISP_ServiceOnPath2(void *p)
{
	CALLBACK_FUNC_PTR cb_fun = g_dcam_cb[DCAM_CB_PATH2_DONE];
	if (cb_fun)
		(*cb_fun) ();
#ifdef DCAM_DEBUG
	get_dcam_reg();
#endif
}

static void _ISP_ServiceOnCAPBufOF(void *p)
{
	CALLBACK_FUNC_PTR cb_fun = g_dcam_cb[DCAM_CB_CAP_FIFO_OF];
	if (cb_fun)
		(*cb_fun) ();
}

static void _ISP_ServiceOnSensorLineErr(void *p)
{
	CALLBACK_FUNC_PTR cb_fun = g_dcam_cb[DCAM_CB_SENSOR_LINE_ERR];
	if (cb_fun)
		(*cb_fun) ();
}

static void _ISP_ServiceOnSensorFrameErr(void *p)
{
	CALLBACK_FUNC_PTR cb_fun = g_dcam_cb[DCAM_CB_SENSOR_FRAME_ERR];
	if (cb_fun)
		(*cb_fun) ();
}

static void _ISP_ServiceOnJpegBufOF(void *p)
{
	CALLBACK_FUNC_PTR cb_fun = g_dcam_cb[DCAM_CB_JPEG_BUF_OF];
	if (cb_fun)
		(*cb_fun) ();
}

int32_t _ISP_ServiceStartPreview(void)
{
	ISP_SERVICE_T *s = &s_isp_service;
	ISP_CAP_SYNC_POL_T cap_sync = { 0 };
	int32_t rtn_drv = DCAM_SUCCESS;
	ISP_SIZE_T disp_size = { 0 };
	ISP_MODE_E isp_mode = ISP_MODE_PREVIEW;
	uint32_t reg_val = 0;

	DCAM_TRACE("DCAM:_ISP_ServiceStartPreview s->module_addr=0x%x E.\n",
		   s->module_addr);

	rtn_drv = ISP_DriverModuleInit(s->module_addr);
	ISP_RTN_IF_ERR(rtn_drv);
	DCAM_TRACE("DCAM:ISP_DriverModuleInit.\n");

	if (0 != dcam_set_mclk(ISP_CLK_128M)) {
		DCAM_TRACE_ERR
		    ("DCAM:_ISP_ServiceStartPreview, fail to set dcam mclk.\n");
		return 1;
	}
	DCAM_TRACE("DCAM:ISP_DriverSetClk.\n");
	disp_size.w = s->display_range.w;
	disp_size.h = s->display_range.h;

	if (ISP_ROTATION_0 != s->display_rotation) {
		if (ISP_DCAM_PATH1_OUT_FORMAT_YUV420 ==
		    s->dcam_path1_out_format) {
			if (((ISP_ROTATION_90 == s->display_rotation)
			     || (ISP_ROTATION_270 == s->display_rotation))
			    && (0 == (disp_size.h & 0x7))) {
				rtn_drv =
				    ISP_DriverSetMode(s->module_addr,
						      ISP_MODE_PREVIEW_EX);
				isp_mode = ISP_MODE_PREVIEW_EX;
			} else if (0 == (disp_size.w & 0x7)) {
				rtn_drv =
				    ISP_DriverSetMode(s->module_addr,
						      ISP_MODE_PREVIEW_EX);
				isp_mode = ISP_MODE_PREVIEW_EX;
			} else {
				rtn_drv =
				    ISP_DriverSetMode(s->module_addr,
						      ISP_MODE_PREVIEW);
				isp_mode = ISP_MODE_PREVIEW;
			}
		} else {
			rtn_drv =
			    ISP_DriverSetMode(s->module_addr,
					      ISP_MODE_PREVIEW_EX);
			isp_mode = ISP_MODE_PREVIEW_EX;
		}
	} else {
		rtn_drv = ISP_DriverSetMode(s->module_addr, ISP_MODE_PREVIEW);
		isp_mode = ISP_MODE_PREVIEW;
	}
	DCAM_TRACE("DCAM:_ISP_ServiceStartPreview,preview mode =%d .\n ",
		   isp_mode);

	ISP_RTN_IF_ERR(rtn_drv);

	/*Set CAP */
	if (0 == g_dcam_param.no_skip_frame_flag) {
		rtn_drv = ISP_DriverCapConfig(s->module_addr,
					      ISP_CAP_PRE_SKIP_CNT,
					      (void *)&s->
					      preview_skip_frame_num);
	} else {
		rtn_drv = ISP_DriverCapConfig(s->module_addr,
					      ISP_CAP_PRE_SKIP_CNT,
					      (void *)&reg_val);
	}
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_FRM_DECI,
				      (void *)&s->preview_deci_frame_num);
	ISP_RTN_IF_ERR(rtn_drv);

	cap_sync.vsync_pol = (uint8_t) s->vsync_polarity;
	cap_sync.hsync_pol = (uint8_t) s->hsync_polarity;
	cap_sync.pclk_pol = (uint8_t) s->pclk_polarity;
	DCAM_TRACE("DCAM:_ISP_ServiceStartPreview:hsync=%d,vsync=%d,plck=%d.\n",
		   cap_sync.hsync_pol, cap_sync.vsync_pol, cap_sync.pclk_pol);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_SYNC_POL, (void *)&cap_sync);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_YUV_TYPE,
				      (void *)&s->cap_input_image_pattern);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_INPUT_RECT,
				      (void *)&s->cap_input_range);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_FRM_COUNT_CLR, NULL);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_IMAGE_XY_DECI,
				      (void *)&s->cap_img_dec);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_SENSOR_MODE,
				      (void *)&s->sensor_mode);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_IF_MODE, (void *)&s->cap_if_mode);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_IF_ENDIAN,
				      (void *)&s->cap_if_endian);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_SPI_ORIG_WIDTH,
				      (void *)&s->width_of_spi);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_CCIR565_ENABLE,
				      (void *)&s->ccir656_en);
	ISP_RTN_IF_ERR(rtn_drv);

	DCAM_TRACE("DCAM:ISP_DriverCapConfig done!.\n");

	if (ISP_MODE_PREVIEW == isp_mode) {
		/*Set Path1 */
		rtn_drv = ISP_DriverPath1Config(s->module_addr,
						ISP_PATH_INPUT_SIZE,
						(void *)&s->input_size);
		ISP_RTN_IF_ERR(rtn_drv);

		rtn_drv = ISP_DriverPath1Config(s->module_addr,
						ISP_PATH_INPUT_RECT,
						(void *)&s->input_range);
		ISP_RTN_IF_ERR(rtn_drv);

		rtn_drv = ISP_DriverPath1Config(s->module_addr,
						ISP_PATH_OUTPUT_SIZE,
						(void *)&disp_size);
		ISP_RTN_IF_ERR(rtn_drv);

		rtn_drv = ISP_DriverPath1Config(s->module_addr,
						ISP_PATH_OUTPUT_FORMAT,
						(void *)&s->dcam_path1_out_format);
		ISP_RTN_IF_ERR(rtn_drv);

		if(ISP_DCAM_PATH1_OUT_FORMAT_YUV420 == s->dcam_path1_out_format) {
			reg_val = 1;
			rtn_drv = ISP_DriverPath1Config(s->module_addr,
									ISP_PATH_UV420_AVG_EN,
									(void*)&reg_val);
			ISP_RTN_IF_ERR(rtn_drv);
		}

		rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
						   ISP_IRQ_NOTICE_PATH1_DONE,
						   _ISP_ServiceOnPath1);
		ISP_RTN_IF_ERR(rtn_drv);
	} else {
		isp_get_path2();
		DCAM_TRACE
		    ("DCAM:_ISP_ServiceStartPreview,use path2 when preview.\n");
		/*Set Path2 */
		rtn_drv = ISP_DriverPath2Config(s->module_addr,
						ISP_PATH_INPUT_SIZE,
						(void *)&s->input_size);
		ISP_RTN_IF_ERR(rtn_drv);

		rtn_drv = ISP_DriverPath2Config(s->module_addr,
						ISP_PATH_INPUT_RECT,
						(void *)&s->input_range);
		ISP_RTN_IF_ERR(rtn_drv);

		rtn_drv = ISP_DriverPath2Config(s->module_addr,
						ISP_PATH_OUTPUT_SIZE,
						(void *)&disp_size);
		ISP_RTN_IF_ERR(rtn_drv);

		rtn_drv = ISP_DriverPath2Config(s->module_addr,
						ISP_PATH_OUTPUT_FORMAT,
						(void *)&s->
						dcam_path1_out_format);
		ISP_RTN_IF_ERR(rtn_drv);

		rtn_drv = ISP_DriverPath2Config(s->module_addr,
						ISP_PATH_ROT_MODE,
						(void *)&s->display_rotation);
		ISP_RTN_IF_ERR(rtn_drv);

		rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
						   ISP_IRQ_NOTICE_PATH2_DONE,
						   _ISP_ServiceOnPath2);
		ISP_RTN_IF_ERR(rtn_drv);
	}
	/*Register ISR callback */
	rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
					   ISP_IRQ_NOTICE_SENSOR_SOF,
					   _ISP_ServiceOnSensorSOF);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
					   ISP_IRQ_NOTICE_CAP_EOF,
					   _ISP_ServiceOnCAPEOF);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
					   ISP_IRQ_NOTICE_CAP_SOF,
					   _ISP_ServiceOnCAPSOF);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
					   ISP_IRQ_NOTICE_CAP_FIFO_OF,
					   _ISP_ServiceOnCAPBufOF);
	ISP_RTN_IF_ERR(rtn_drv);
	rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
					   ISP_IRQ_NOTICE_SENSOR_LINE_ERR,
					   _ISP_ServiceOnSensorLineErr);
	ISP_RTN_IF_ERR(rtn_drv);
	rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
					   ISP_IRQ_NOTICE_SENSOR_FRAME_ERR,
					   _ISP_ServiceOnSensorFrameErr);
	ISP_RTN_IF_ERR(rtn_drv);
	ISP_DriverSetBufferAddress(s->module_addr, g_dcam_param.first_buf_addr,
				   g_dcam_param.first_buf_uv_addr);
#ifdef DCAM_DEBUG
	get_dcam_reg();
#endif
	rtn_drv = ISP_DriverStart(s->module_addr);
exit:
	if (rtn_drv) {
		DCAM_TRACE_ERR("DCAM: start preview: driver error code 0x%x",
			       rtn_drv);
	} else {
		s->state = ISP_STATE_PREVIEW;
	}
	DCAM_TRACE("DCAM:_ISP_ServiceStartPreview X.\n");
	return rtn_drv;
}

static int ISP_ServiceStartPreview(void)
{
	ISP_SERVICE_T *s = &s_isp_service;
	int ret = DCAM_SUCCESS;

	DCAM_TRACE("DCAM:ISP_ServiceStartPreview");
	ret = _ISP_ServiceStartPreview();
	if (DCAM_SUCCESS == ret) {
		s->is_first_frame = 1;
	}
	return ret;
}

int dcam_open(void)
{
	DCAM_TRACE("DCAM:dcam_open begin.\n");
	g_dcam_clk = clk_get(NULL, "clk_dcam");

	if (IS_ERR(g_dcam_clk)) {
		DCAM_TRACE_ERR("DCAM:dcam_open,get clk fail!.\n");
		return 1;
	}

	_ISP_ServiceOpen();
	ISP_DriverRegisterIRQ();
	DCAM_TRACE("DCAM:dcam_open end.\n");
	return DCAM_SUCCESS;
}

int dcam_close(void)
{
	//unregister dcam IRQ
	ISP_DriverUnRegisterIRQ();
	_ISP_ServiceClose();

	if (g_dcam_clk) {
		clk_put(g_dcam_clk);
		DCAM_TRACE("DCAM:dcam_close,clk_put ok.\n");
		g_dcam_clk = NULL;
	}

	return DCAM_SUCCESS;
}

static int _ISP_ServiceStartJpeg(void)
{
	ISP_SERVICE_T *s = &s_isp_service;
	ISP_CAP_SYNC_POL_T cap_sync = { 0 };
	int32_t rtn_drv = DCAM_SUCCESS;
	ISP_SIZE_T output_size = { 0 };
	uint32 jpeg_buf_size=0;

	DCAM_TRACE("DCAM:_ISP_ServiceStartJpeg.\n");

	rtn_drv = ISP_DriverModuleInit(s->module_addr);
	ISP_RTN_IF_ERR(rtn_drv);

	if (0 != dcam_set_mclk(ISP_CLK_128M)) {
		DCAM_TRACE_ERR
		    ("DCAM:_ISP_ServiceStartJpeg, fail to set dcam mclk.\n");
		return 1;
	}

	rtn_drv = ISP_DriverSetMode(s->module_addr, ISP_MODE_CAPTURE);
	ISP_RTN_IF_ERR(rtn_drv);

	/*Set CAP */
	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_PRE_SKIP_CNT,
				      (void *)&s->capture_skip_frame_num);
	ISP_RTN_IF_ERR(rtn_drv);

	cap_sync.vsync_pol = s->vsync_polarity;
	cap_sync.hsync_pol = s->hsync_polarity;
	cap_sync.pclk_pol = s->pclk_polarity;
	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_SYNC_POL, (void *)&cap_sync);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_YUV_TYPE,
				      (void *)&s->cap_input_image_pattern);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_INPUT_RECT,
				      (void *)&s->cap_input_range);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_FRM_COUNT_CLR, NULL);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_IMAGE_XY_DECI,
				      (void *)&s->cap_img_dec);
	ISP_RTN_IF_ERR(rtn_drv);

	DCAM_TRACE("DCAM:_ISP_ServiceStartJpeg sensor_mode=%d.\n",
		   s->sensor_mode);
	rtn_drv =
	    ISP_DriverCapConfig(s->module_addr, ISP_CAP_SENSOR_MODE,
				(void *)&s->sensor_mode);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_IF_MODE, (void *)&s->cap_if_mode);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_IF_ENDIAN,
				      (void *)&s->cap_if_endian);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_SPI_ORIG_WIDTH,
				      (void *)&s->width_of_spi);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverCapConfig(s->module_addr,
				      ISP_CAP_CCIR565_ENABLE,
				      (void *)&s->ccir656_en);
	ISP_RTN_IF_ERR(rtn_drv);

	/*Set Path1 */
	rtn_drv = ISP_DriverPath1Config(s->module_addr,
					ISP_PATH_INPUT_SIZE,
					(void *)&s->input_size);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverPath1Config(s->module_addr,
					ISP_PATH_INPUT_RECT,
					(void *)&s->input_range);
	ISP_RTN_IF_ERR(rtn_drv);

	jpeg_buf_size = s->input_size.w*s->input_size.h/4;
	rtn_drv = ISP_DriverCapConfig(s->module_addr,
			                                   ISP_CAP_JPEG_MEM_IN_16K,
			                                   (void*)&jpeg_buf_size);
	ISP_RTN_IF_ERR(rtn_drv);
	if ((s->encoder_size.w > 960)
	    && (s->cap_output_size.w != s->encoder_size.w)) {
		output_size.w = s->cap_output_size.w;
		output_size.h = s->cap_output_size.h;
	} else {
		output_size.w = s->encoder_size.w;
		output_size.h = s->encoder_size.h;
	}

	rtn_drv = ISP_DriverPath1Config(s->module_addr,
					ISP_PATH_OUTPUT_SIZE,
					(void *)&output_size);
	ISP_RTN_IF_ERR(rtn_drv);

	if (ISP_DCAM_PATH1_OUT_FORMAT_JPEG != s->dcam_path1_out_format) {
		rtn_drv = ISP_DriverPath1Config(s->module_addr,
						ISP_PATH_OUTPUT_FORMAT,
						(void *)&s->
						dcam_path1_out_format);
		ISP_RTN_IF_ERR(rtn_drv);
	}

	/*Register ISR callback */
	rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
					   ISP_IRQ_NOTICE_PATH1_DONE,
					   _ISP_ServiceOnPath1);
	ISP_RTN_IF_ERR(rtn_drv);

	rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
					   ISP_IRQ_NOTICE_CAP_SOF,
					   _ISP_ServiceOnCAPSOF);
	ISP_RTN_IF_ERR(rtn_drv);

	if (ISP_CAP_MODE_YUV == s->sensor_mode) {
		rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
						   ISP_IRQ_NOTICE_CAP_FIFO_OF,
						   _ISP_ServiceOnCAPBufOF);
		ISP_RTN_IF_ERR(rtn_drv);
		rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
						   ISP_IRQ_NOTICE_SENSOR_LINE_ERR,
						   _ISP_ServiceOnSensorLineErr);
		ISP_RTN_IF_ERR(rtn_drv);
		rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
						   ISP_IRQ_NOTICE_SENSOR_FRAME_ERR,
						   _ISP_ServiceOnSensorFrameErr);
		ISP_RTN_IF_ERR(rtn_drv);
	}else {
		rtn_drv = ISP_DriverNoticeRegister(s->module_addr,
	                                           ISP_IRQ_NOTICE_JPEG_BUF_OF,
	                                           _ISP_ServiceOnJpegBufOF);
		ISP_RTN_IF_ERR(rtn_drv);
	}
	ISP_DriverSetBufferAddress(s->module_addr, g_dcam_param.first_buf_addr,
				   g_dcam_param.first_buf_uv_addr);
#ifdef DCAM_DEBUG
	get_dcam_reg();
#endif
	rtn_drv = ISP_DriverStart(s->module_addr);
	ISP_RTN_IF_ERR(rtn_drv);
exit:

	if (rtn_drv) {
		DCAM_TRACE_ERR("DCAM:start jpeg driver error code 0x%x",
			       rtn_drv);
	} else {
		if (0 == s->is_slice) {
			s->state = ISP_STATE_CAPTURE_DONE;
		} else {
			s->state = ISP_STATE_CAPTURE_SCALE;
		}
	}
	return rtn_drv;
}

static int ISP_ServiceStartCapture(void)
{
	int ret = DCAM_SUCCESS;
	ret = _ISP_ServiceStartJpeg();
	return ret;
}

static uint32_t _ISP_ServiceGetXYDeciFactor(uint32_t * src_width,
					    uint32_t * src_height,
					    uint32_t dst_width,
					    uint32_t dst_height)
{
	uint32_t i = 0;
	uint32_t width = 0;
	uint32_t height = 0;

	DCAM_TRACE
	    ("DCAM:_ISP_ServiceGetXYDeciFactor.src_w=%d,src_h=%d,dst_w=%d,dst_h=%d .\n",
	     *src_width, *src_height, dst_width, dst_height);

	for(i = 0; i < ISP_CAP_DEC_XY_MAX + 1; i++) {
		width = *src_width / (1<<i);
		height = *src_height / (1<<i);
		if(width <= (dst_width * ISP_PATH_SC_COEFF_MAX) &&
		height <= (dst_height * ISP_PATH_SC_COEFF_MAX) )
			break;
	}

	*src_width = width;
	*src_height = height;
	return i;
}

static int _ISP_GetSensorInterfaceInfo(void)
{
	ISP_SERVICE_T *s = &s_isp_service;
	SENSOR_EXP_INFO_T *sensor_info_ptr;
	SENSOR_INTERFACE_E sensor_interface = SENSOR_INTERFACE_MAX;
	uint32_t width = 0;

	sensor_info_ptr = Sensor_GetInfo();
	if (PNULL == sensor_info_ptr) {
		DCAM_TRACE_HIGH
		    ("DCAM :_ISP_GetSensorInterfaceInfo,get sensor info fail.\n");
		return -1;
	}

	width = s->cap_input_range.w * 2 - 1;

	s->cap_if_endian = 0;
	s->cap_if_mode = 0;
	s->sensor_mode = ISP_CAP_MODE_YUV;
	s->ccir656_en = 0;
	sensor_interface = sensor_info_ptr->sensor_interface;
	switch (sensor_interface) {
	case SENSOR_INTERFACE_CCIR601_8BITS:
		break;
	case SENSOR_INTERFACE_CCIR601_4BITS:
		s->cap_if_mode = ISP_CAP_IF_4BITS;
		break;
	case SENSOR_INTERFACE_CCIR601_2BITS:
		s->cap_if_mode = ISP_CAP_IF_2BITS;
		break;
	case SENSOR_INTERFACE_CCIR601_1BITS:
		s->cap_if_mode = ISP_CAP_IF_1BITS;
		break;
	case SENSOR_INTERFACE_CCIR656_8BITS:
		s->ccir656_en = 1;
		s->cap_if_mode = ISP_CAP_IF_8BITS;
		break;
	case SENSOR_INTERFACE_CCIR656_4BITS:
		s->ccir656_en = 1;
		s->cap_if_mode = ISP_CAP_IF_4BITS;
		break;
	case SENSOR_INTERFACE_CCIR656_2BITS:
		s->ccir656_en = 1;
		s->cap_if_mode = ISP_CAP_IF_2BITS;
		break;
	case SENSOR_INTERFACE_CCIR656_1BITS:
		s->ccir656_en = 1;
		s->cap_if_mode = ISP_CAP_IF_1BITS;
		break;
	case SENSOR_INTERFACE_SPI_8BITS:
		s->cap_if_mode = ISP_CAP_IF_8BITS;
		s->sensor_mode = ISP_CAP_MODE_SPI;
		s->width_of_spi = width;
		break;
	case SENSOR_INTERFACE_SPI_4BITS:
		s->cap_if_mode = ISP_CAP_IF_4BITS;
		s->sensor_mode = ISP_CAP_MODE_SPI;
		s->cap_if_endian = ISP_CAP_IF_ENDIAN_LE;
		s->width_of_spi = width;
		break;
	case SENSOR_INTERFACE_SPI_4BITS_BE:
		s->cap_if_mode = ISP_CAP_IF_4BITS;
		s->sensor_mode = ISP_CAP_MODE_SPI;
		s->cap_if_endian = ISP_CAP_IF_ENDIAN_BE;
		s->width_of_spi = width;
		break;
	case SENSOR_INTERFACE_SPI_2BITS:
		s->cap_if_mode = ISP_CAP_IF_2BITS;
		s->sensor_mode = ISP_CAP_MODE_SPI;
		s->cap_if_endian = ISP_CAP_IF_ENDIAN_LE;
		s->width_of_spi = width;
		break;
	case SENSOR_INTERFACE_SPI_2BITS_BE:
		s->cap_if_mode = ISP_CAP_IF_4BITS;
		s->sensor_mode = ISP_CAP_MODE_SPI;
		s->cap_if_endian = ISP_CAP_IF_ENDIAN_BE;
		s->width_of_spi = width;
		break;
	case SENSOR_INTERFACE_SPI_1BITS:
		s->cap_if_mode = ISP_CAP_IF_4BITS;
		s->sensor_mode = ISP_CAP_MODE_SPI;
		s->cap_if_endian = ISP_CAP_IF_ENDIAN_LE;
		s->width_of_spi = width;
		break;
	case SENSOR_INTERFACE_SPI_1BITS_BE:
		s->cap_if_mode = ISP_CAP_IF_4BITS;
		s->sensor_mode = ISP_CAP_MODE_SPI;
		s->cap_if_endian = ISP_CAP_IF_ENDIAN_BE;
		s->width_of_spi = width;
		break;
	default:
		DCAM_TRACE_ERR
		    ("DCAM:_ISP_GetSensorInterfaceInfo sensor interface is default!");
		break;
	}
	if (SENSOR_IMAGE_FORMAT_JPEG == sensor_info_ptr->image_format) {
		s->sensor_mode = ISP_CAP_MODE_JPEG;
	} else if (SENSOR_IMAGE_FORMAT_RAW == sensor_info_ptr->image_format) {
		s->sensor_mode = ISP_CAP_MODE_RAWRGB;
	}

	DCAM_TRACE_HIGH
	    ("DCAM:_ISP_GetSensorInterfaceInfo,sensor_mode:%d,cap_endian:%d,cap mode:%d,spi width:%d \n",
	     s->sensor_mode, s->cap_if_endian, s->cap_if_mode, s->width_of_spi);
	return 0;
}

static ISP_DCAM_PATH1_OUT_FORMAT_E dcam_outformat_convert(DCAM_DATA_FORMAT_E
							  input_format)
{
	ISP_DCAM_PATH1_OUT_FORMAT_E out = ISP_DCAM_PATH1_OUT_FORMAT_MAX;

	switch (input_format) {
	case DCAM_DATA_YUV422:
		out = ISP_DCAM_PATH1_OUT_FORMAT_YUV422;
		break;
	case DCAM_DATA_YUV420:
		out = ISP_DCAM_PATH1_OUT_FORMAT_YUV420;
		break;
	case DCAM_DATA_RGB:
		out = ISP_DCAM_PATH1_OUT_FORMAT_RGB;
		break;
	case DCAM_DATA_JPEG:
		out = ISP_DCAM_PATH1_OUT_FORMAT_JPEG;
		break;
	default:
		DCAM_TRACE_ERR
		    ("DCAM:dcam_outformat_convert param error,input_format=%d .\n",
		     input_format);
		break;
	}
	DCAM_TRACE("DCAM:dcam_outformat_convert out_format=%d .\n", out);
	return out;
}

#define DCAMERA_PIXEL_ALIGNED               4
#define DCAMERA_WIDTH(w)                    ((w)& ~(DCAMERA_PIXEL_ALIGNED - 1))
#define DCAMERA_HEIGHT(h)                   ((h)& ~(DCAMERA_PIXEL_ALIGNED - 1))

void dcam_get_zoom_trim(ISP_RECT_T * trim_rect, uint32_t zoom_level)
{
	uint32_t zoom_step_w, zoom_step_h;
	uint32_t trim_width = trim_rect->w, trim_height = trim_rect->h;

	if (0 == zoom_level)
		return;
	DCAM_TRACE_HIGH
	    ("DCAM:dcam_get_zoom_trim ,level =%d,x=%d,y=%d,w=%d,h=%d .\n",
	     zoom_level, trim_rect->x, trim_rect->y, trim_rect->w,
	     trim_rect->h);

	zoom_step_w = ZOOM_STEP(trim_width);
	zoom_step_w &= ~1;
	zoom_step_w *= zoom_level;

	zoom_step_h = ZOOM_STEP(trim_height);
	zoom_step_h &= ~1;
	zoom_step_h *= zoom_level;
	trim_width = trim_width - zoom_step_w;
	trim_height = trim_height - zoom_step_h;

	trim_rect->x = (trim_rect->w - trim_width) / 2;
	trim_rect->x &= ~1;
	trim_rect->y = (trim_rect->h - trim_height) / 2;
	trim_rect->y &= ~1;

	trim_rect->w = DCAMERA_WIDTH(trim_width);
	trim_rect->h = DCAMERA_HEIGHT(trim_height);
	DCAM_TRACE_HIGH("DCAM:dcam_get_zoom_trim,x=%d,y=%d,w=%d,h=%d .\n",
			trim_rect->x, trim_rect->y, trim_rect->w, trim_rect->h);
}

static int ISP_ServiceSetParameters(void)
{
	ISP_SERVICE_T *s = &s_isp_service;
	SENSOR_EXP_INFO_T *sensor_info_ptr;
	ISP_SIZE_T dst_img_size;
	uint32_t trim_width = 0;
	uint32_t trim_height = 0;
	uint32_t zoom_step_w = 0, zoom_step_h = 0;
	int ret = 0;

	sensor_info_ptr = Sensor_GetInfo();
	if (PNULL == sensor_info_ptr) {
		DCAM_TRACE_HIGH
		    ("DCAM:ISP_ServiceSetParameters,get sensor info fail.\n");
		return -1;
	}

	s->cap_input_image_format = sensor_info_ptr->image_format;
	s->cap_input_image_pattern = sensor_info_ptr->image_pattern;
	s->hsync_polarity = sensor_info_ptr->hsync_polarity;
	s->vsync_polarity = sensor_info_ptr->vsync_polarity;
	s->pclk_polarity = sensor_info_ptr->pclk_polarity;
	s->preview_skip_frame_num = sensor_info_ptr->preview_skip_num;
	s->preview_deci_frame_num = sensor_info_ptr->preview_deci_num;
	s->cap_input_size.w = g_dcam_param.input_size.w;
	s->cap_input_size.h = g_dcam_param.input_size.h;
	dst_img_size.w = g_dcam_param.display_rect.w;
	dst_img_size.h = g_dcam_param.display_rect.h;

	DCAM_TRACE("DCAM:ISP_ServiceSetParameters,pclk polarity is %d.\n",
		   s->pclk_polarity);
	//full screen
	if (dst_img_size.w * g_dcam_param.input_size.h <
	    g_dcam_param.input_size.w * dst_img_size.h) {
		trim_width =
		    dst_img_size.w * g_dcam_param.input_size.h / dst_img_size.h;
		trim_height = g_dcam_param.input_size.h;
	} else {
		trim_width = g_dcam_param.input_size.w;
		trim_height =
		    g_dcam_param.input_size.w * dst_img_size.h / dst_img_size.w;
	}
	if (0 != g_dcam_param.zoom_level) {
		zoom_step_w = ZOOM_STEP(trim_width);
		zoom_step_w &= ~1;
		zoom_step_w *= g_dcam_param.zoom_level;
		zoom_step_h = ZOOM_STEP(trim_height);
		zoom_step_h &= ~1;
		zoom_step_h *= g_dcam_param.zoom_level;
		trim_width = trim_width - zoom_step_w;
		trim_height = trim_height - zoom_step_h;
	}

	s->cap_input_range.x = (g_dcam_param.input_size.w - trim_width) / 2;
	s->cap_input_range.x &= ~1;
	s->cap_input_range.y = (g_dcam_param.input_size.h - trim_height) / 2;
	s->cap_input_range.y &= ~1;

	trim_width = DCAMERA_WIDTH(trim_width);
	trim_height = DCAMERA_HEIGHT(trim_height);
	s->cap_input_range.w = trim_width;
	s->cap_input_range.h = trim_height;
	s->cap_output_size.w = trim_width;
	s->cap_output_size.h = trim_height;
	s->encoder_size.w = g_dcam_param.encoder_rect.w;
	s->encoder_size.h = g_dcam_param.encoder_rect.h;
	s->preview_deci_frame_num = 0;

	DCAM_TRACE_HIGH
	    ("DCAM:ISP_ServiceSetParameters,zoom_level =%d ,trim_width=%d,trim_height=%d,input size:%d,%d..\n",
	     g_dcam_param.zoom_level, trim_width, trim_height,
	     g_dcam_param.input_size.w, g_dcam_param.input_size.h);

	s->cap_img_dec.x_factor =
	    _ISP_ServiceGetXYDeciFactor(&s->cap_output_size.w,
					&s->cap_output_size.h, dst_img_size.w,
					dst_img_size.h);
	s->cap_img_dec.y_factor = s->cap_img_dec.x_factor;
	s->cap_img_dec.x_mode = ISP_CAP_IMG_DEC_MODE_DIRECT;
	s->input_size.w = s->cap_output_size.w;
	s->input_size.h = s->cap_output_size.h;
	trim_width = DCAMERA_WIDTH(s->cap_output_size.w);
	trim_height = DCAMERA_HEIGHT(s->cap_output_size.h);
	s->input_range.x = (s->cap_output_size.w-trim_width)/2;
	s->input_range.x &= ~1;
	s->input_range.y = (s->cap_output_size.h-trim_height)/2;
	s->input_range.y &= ~1;
	s->input_range.w = trim_width;
	s->input_range.h = trim_height;
	s->display_range.x = g_dcam_param.display_rect.x;
	s->display_range.y = g_dcam_param.display_rect.y;
	s->display_range.w = dst_img_size.w;
	s->display_range.h = dst_img_size.h;
	if (DCAM_ROTATION_90 == g_dcam_param.rotation) {
		s->display_rotation = ISP_ROTATION_90;
	} else if (DCAM_ROTATION_180 == g_dcam_param.rotation) {
		s->display_rotation = ISP_ROTATION_180;
	} else if (DCAM_ROTATION_270 == g_dcam_param.rotation) {
		s->display_rotation = ISP_ROTATION_270;
	} else {
		s->display_rotation = ISP_ROTATION_0;
	}

	DCAM_TRACE("DCAM:ISP_ServiceSetParameters,display rotation =%d .\n",
		   s->display_rotation);

	ret = _ISP_GetSensorInterfaceInfo();
	if (0 != ret)
		return -1;
	s->dcam_path1_out_format = dcam_outformat_convert(g_dcam_param.format);
	return 0;
}

int dcam_start(void)
{
	int ret = DCAM_SUCCESS;
	isp_get_path2();
	if (0 == dcam_get_user_count()) {
		ISP_DriverIramSwitch(AHB_GLOBAL_REG_CTL0, IRAM_FOR_ISP);/*switch IRAM to isp*/
		ISP_DriverSoftReset(AHB_GLOBAL_REG_CTL0);
		DCAM_TRACE("DCAM: scam_start softreset and set clk.\n");
	}
	isp_put_path2();
	dcam_inc_user_count();

	DCAM_TRACE("DCAM: dcam_start start. \n");
	ret = ISP_ServiceSetParameters();
	if (0 != ret)
		goto dcam_start_end;

	if (DCAM_MODE_TYPE_PREVIEW == g_dcam_param.mode)
		ret = ISP_ServiceStartPreview();
	else if (DCAM_MODE_TYPE_CAPTURE == g_dcam_param.mode)
		ret = ISP_ServiceStartCapture();

	DCAM_TRACE("DCAM: dcam_start end. \n");
dcam_start_end:
	if (ret != 0) {
		printk("dcam:dcam_start return ret=%d.\n", ret);
		ret = 1;
		dcam_dec_user_count();
	}
	return ret;
}

int dcam_is_previewing(uint32_t zoom_level)
{
	ISP_SERVICE_T *s = &s_isp_service;
	int32_t ret = 0;

	ret = (ISP_STATE_PREVIEW == s->state) ? 1 : 0;

	if (ret) {
		g_dcam_param.zoom_level = zoom_level;
	}

	DCAM_TRACE("DCAM: dcam_is_previewing %d,zoom=%d. \n", ret,
		   g_dcam_param.zoom_level);
	return ret;
}

void dcam_error_close(void)
{
	ISP_SERVICE_T *s = &s_isp_service;

	ISP_DriverHandleErr(AHB_GLOBAL_REG_CTL0, s->module_addr);
}

void ISP_ServiceStopPreview(void)
{
	ISP_SERVICE_T *s = &s_isp_service;

	DCAM_TRACE("DCAM:ISP_ServiceStopPreview E.\n");

	if (s->state != ISP_STATE_PREVIEW) {
		DCAM_TRACE_ERR
		    ("DCAM:ISP_ServiceStopPreview: state is not preview.\n");
		return;
	}

	ISP_DriverStop(s->module_addr);
	s->state = ISP_STATE_STOP;
	if (ISP_MODE_PREVIEW_EX == ISP_DriverGetMode()) {
		printk
		    ("DCAM:_ISP_ServiceClose, ISP_MODE_PREVIEW_EX put path2 .\n");
		isp_put_path2();
	}

	DCAM_TRACE("DCAM:ISP_ServiceStopPreview X.\n");
	return;
}

void ISP_ServiceStopCapture(void)
{
	ISP_SERVICE_T *s = &s_isp_service;

	DCAM_TRACE("DCAM:ISP_ServiceStopCapture");

	ISP_DriverStop(s->module_addr);
	s->state = ISP_STATE_STOP;
	return;
}

void dcam_get_jpg_len(uint32_t * len)
{
	ISP_SERVICE_T *s = &s_isp_service;
	*len = 0;

	ISP_DriverCapGetInfo(s->module_addr, ISP_CAP_JPEG_GET_LENGTH, len);
	return;
}

int dcam_stop(void)
{
	ISP_SERVICE_T *s = &s_isp_service;
	DCAM_TRACE("DCAM: dcam_stop start. \n");

	g_dcam_param.no_skip_frame_flag = 0;
	ISP_DriverStop(s->module_addr);
	s->state = ISP_STATE_STOP;
	dcam_dec_user_count();
	isp_get_path2();
	if(0 == dcam_get_user_count()) {
		ISP_DriverSoftReset(AHB_GLOBAL_REG_CTL0);
		ISP_DriverModuleDisable(AHB_GLOBAL_REG_CTL0);
		ISP_DriverIramSwitch(AHB_GLOBAL_REG_CTL0, IRAM_FOR_ARM); /*switch IRAM to ARM	*/
		DCAM_TRACE("DCAM: dcam stop softreset and set clk.\n");
	}
         isp_put_path2();
	if (g_dcam_clk) {
		clk_disable(g_dcam_clk);
		DCAM_TRACE("DCAM:dcam_stop,clk_disable ok.\n");
	}

	DCAM_TRACE("DCAM: dcam_stop end. \n");
	return DCAM_SUCCESS;
}

uint32_t dcam_set_buffer_address(uint32_t yaddr, uint32_t uv_addr)
{
	ISP_SERVICE_T *s = &s_isp_service;
	return ISP_DriverSetBufferAddress(s->module_addr, yaddr, uv_addr);
}

void dcam_powerdown(uint32_t sensor_id, uint32_t value)
{
	ISP_SERVICE_T *s = &s_isp_service;
	DCAM_TRACE("DCAM :dcam_powerdown:module addr:0x%x,value:%d.\n",
		   s->module_addr, value);
	ISP_DriverPowerDown(s->module_addr, sensor_id, value);
}

void dcam_reset_sensor(uint32_t value)
{
	ISP_SERVICE_T *s = &s_isp_service;

	DCAM_TRACE("DCAM :dcam_reset_sensor:module addr:0x%x,value:%d.\n",
		   s->module_addr, value);
	ISP_DriverReset(s->module_addr, value);
}

void dcam_set_first_buf_addr(uint32_t y_addr, uint32_t uv_addr)
{
	g_dcam_param.first_buf_addr = y_addr;
	g_dcam_param.first_buf_uv_addr = uv_addr;
	g_dcam_param.no_skip_frame_flag = 1;
}

void dcam_enableint(void)
{
	_ISP_DriverEnableInt();
}

void dcam_disableint(void)
{
	_ISP_DriverDisableInt();
}

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
#ifndef _SENSOR_DRV_H_
#define _SENSOR_DRV_H_
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include "jpeg_exif_header_k.h"

#define SENSOR_SUCCESS 0
#define SENSOR_FAIL 1
#define SENSOR_FALSE 0
#define SENSOR_TRUE 1
#define SENSOR_ASSERT(a) do{}while(!(a));
#define SENSOR_Sleep(m) msleep(m)
#define SENSOR_MEMSET memset
#define SENSOR_MALLOC kmalloc
typedef void *SENSOR_MUTEX_PTR;
#define SENSOR_PASSERT(m, n) if(!m) printk n
#define BOOLEAN char
#define PNULL  ((void *)0)
#define LOCAL static
#define SENSOR_NULL 0
#define SENOR_CLK_M_VALUE   1000000
#define SENSOER_VDD_1200MV	1200000
#define SENSOER_VDD_1300MV	1300000
#define SENSOER_VDD_1500MV	1500000
#define SENSOER_VDD_1800MV	1800000
#define SENSOER_VDD_2500MV	2500000
#define SENSOER_VDD_2800MV	2800000
#define SENSOER_VDD_3000MV	3000000
#define SENSOER_VDD_3300MV	3300000
#define SENSOER_VDD_3800MV	3800000
#define DEBUG_SENSOR_DRV 1
#ifdef DEBUG_SENSOR_DRV
#define SENSOR_PRINT   pr_debug
#else
#define SENSOR_PRINT(...)
#endif

#define SENSOR_PRINT_ERR   printk
#define SENSOR_PRINT_HIGH printk
#define SENSOR_TRACE   SENSOR_PRINT
#define GLOBAL_BASE SPRD_GREG_BASE	/*0xE0002E00UL <--> 0x8b000000 */
#define ARM_GLOBAL_REG_GEN0 GLOBAL_BASE + 0x008UL
#define ARM_GLOBAL_REG_GEN3 GLOBAL_BASE + 0x01CUL
#define ARM_GLOBAL_PLL_SCR GLOBAL_BASE + 0x070UL
#define GR_CLK_GEN5 GLOBAL_BASE + 0x07CUL

#define AHB_BASE SPRD_AHB_BASE	/*0xE000A000 <--> 0x20900000UL */
#define AHB_GLOBAL_REG_CTL0 AHB_BASE + 0x200UL
#define AHB_GLOBAL_REG_SOFTRST AHB_BASE + 0x210UL

#define PIN_CTL_BASE SPRD_CPC_BASE	/*0xE002F000<-->0x8C000000UL */
#define PIN_CTL_CCIRPD1 PIN_CTL_BASE + 0x344UL
#define PIN_CTL_CCIRPD0 PIN_CTL_BASE + 0x348UL

#define MISC_BASE SPRD_MISC_BASE	/*0xE0033000<-->0x82000000 */
#ifdef CONFIG_ARCH_SC8810
#define ANA_REG_BASE MISC_BASE + 0x600
#define ANA_LDO_PD_CTL ANA_REG_BASE + 0x10
#define ANA_LDO_VCTL2 ANA_REG_BASE + 0x20
#else
#define ANA_REG_BASE MISC_BASE + 0x480
#define ANA_LDO_PD_CTL ANA_REG_BASE + 0x10
#define ANA_LDO_VCTL2 ANA_REG_BASE + 0x1C
#endif

#define SENSOR_MAIN_I2C_NAME "sensor_main"
#define SENSOR_SUB_I2C_NAME "sensor_sub"
#define SENSOR_MAIN_I2C_ADDR 0x30
#define SENSOR_SUB_I2C_ADDR 0x21
#define NUMBER_OF_ARRAY(a)    				(sizeof(a)/sizeof(a[0]))
#define ADDR_AND_LEN_OF_ARRAY(a) 			(SENSOR_REG_T*)a, NUMBER_OF_ARRAY(a)
#define SENSOR_DISABLE_MCLK					0	// MHZ
#define SENSOR_DEFALUT_MCLK					24	// MHZ
#define SENSOR_MAX_MCLK						96	// MHZ
#define SENSOR_LOW_PULSE_RESET				0x00
#define SENSOR_HIGH_PULSE_RESET				0x01

#define SENSOR_RESET_PULSE_WIDTH_DEFAULT	20
#define SENSOR_RESET_PULSE_WIDTH_MAX		200

#define SENSOR_LOW_LEVEL_PWDN				0x00
#define SENSOR_HIGH_LEVEL_PWDN				0x01

#define SENSOR_IDENTIFY_CODE_COUNT			0x02

/*Image effect*/
#define SENSOR_IMAGE_EFFECT_NORMAL			(0x01 << 0)
#define SENSOR_IMAGE_EFFECT_BLACKWHITE		(0x01 << 1)
#define SENSOR_IMAGE_EFFECT_RED				(0x01 << 2)
#define SENSOR_IMAGE_EFFECT_GREEN			(0x01 << 3)
#define SENSOR_IMAGE_EFFECT_BLUE				(0x01 << 4)
#define SENSOR_IMAGE_EFFECT_YELLOW			(0x01 << 5)
#define SENSOR_IMAGE_EFFECT_NEGATIVE			(0x01 << 6)
#define SENSOR_IMAGE_EFFECT_CANVAS			(0x01 << 7)
#define SENSOR_IMAGE_EFFECT_RELIEVOS			(0x01 << 8)

/*While balance mode*/
#define SENSOR_WB_MODE_AUTO 				(0x01 << 0)
#define SENSOR_WB_MODE_INCANDESCENCE		(0x01 << 1)
#define SENSOR_WB_MODE_U30					(0x01 << 2)
#define SENSOR_WB_MODE_CWF					(0x01 << 3)
#define SENSOR_WB_MODE_FLUORESCENT		(0x01 << 4)
#define SENSOR_WB_MODE_SUN					(0x01 << 5)
#define SENSOR_WB_MODE_CLOUD				(0x01 << 6)

/*Preview mode*/
#define SENSOR_ENVIROMENT_NORMAL			(0x01 << 0)
#define SENSOR_ENVIROMENT_NIGHT			(0x01 << 1)
#define SENSOR_ENVIROMENT_SUNNY			(0x01 << 2)
#define SENSOR_ENVIROMENT_SPORTS			(0x01 << 3)
#define SENSOR_ENVIROMENT_LANDSCAPE		(0x01 << 4)
#define SENSOR_ENVIROMENT_PORTRAIT			(0x01 << 5)
#define SENSOR_ENVIROMENT_PORTRAIT_NIGHT	(0x01 << 6)
#define SENSOR_ENVIROMENT_BACKLIGHT		(0x01 << 7)
#define SENSOR_ENVIROMENT_MARCO			(0x01 << 8)

#define SENSOR_ENVIROMENT_MANUAL			(0x01 << 30)
#define SENSOR_ENVIROMENT_AUTO				(0x01 << 31)

/*YUV PATTERN*/
#define SENSOR_IMAGE_PATTERN_YUV422_YUYV	0x00
#define SENSOR_IMAGE_PATTERN_YUV422_YVYU	0x01
#define SENSOR_IMAGE_PATTERN_YUV422_UYVY	0x02
#define SENSOR_IMAGE_PATTERN_YUV422_VYUY	0x03
/*RAW RGB BAYER*/
#define SENSOR_IMAGE_PATTERN_RAWRGB_GR	0x00
#define SENSOR_IMAGE_PATTERN_RAWRGB_R		0x01
#define SENSOR_IMAGE_PATTERN_RAWRGB_B		0x02
#define SENSOR_IMAGE_PATTERN_RAWRGB_GB	0x03

/*I2C REG/VAL BIT count*/
#define SENSOR_I2C_VAL_8BIT			0x00
#define SENSOR_I2C_VAL_16BIT			0x01
#define SENSOR_I2C_REG_8BIT			(0x00 << 1)
#define SENSOR_I2C_REG_16BIT			(0x01 << 1)
#define SENSOR_I2C_CUSTOM 			(0x01 << 2)

/*I2C ACK/STOP BIT count*/
#define SNESOR_I2C_ACK_BIT (0x00 << 3)
#define SNESOR_I2C_NOACK_BIT (0x00 << 3)
#define SNESOR_I2C_STOP_BIT (0x00 << 3)
#define SNESOR_I2C_NOSTOP_BIT (0x00 << 3)

/*I2C FEEQ BIT count*/
#define SENSOR_I2C_CLOCK_MASK      (7<<5)
#define SENSOR_I2C_FREQ_20	(0x01 << 5)
#define SENSOR_I2C_FREQ_50 	(0x02 << 5)
#define SENSOR_I2C_FREQ_100 	(0x00 << 5)
#define SENSOR_I2C_FREQ_200 	(0x03 << 5)
#define SENSOR_I2C_FREQ_400  	(0x04 << 5)

#define SENSOR_I2C_ID			1

/*Hardward signal polarity*/
#define SENSOR_HW_SIGNAL_PCLK_N				0x00
#define SENSOR_HW_SIGNAL_PCLK_P				0x01
#define SENSOR_HW_SIGNAL_VSYNC_N			(0x00 << 2)
#define SENSOR_HW_SIGNAL_VSYNC_P			(0x01 << 2)
#define SENSOR_HW_SIGNAL_HSYNC_N			(0x00 << 4)
#define SENSOR_HW_SIGNAL_HSYNC_P			(0x01 << 4)

#define SENSOR_WRITE_DELAY					0xff

#define SENSOR_IOCTL_FUNC_NOT_REGISTER		0xffffffff

/*sensor focus mode*/
#define SENSOR_FOCUS_TRIG 0x01
#define SENSOR_FOCUS_ZONE (0x01<<1)

/*sensor exposure mode*/
#define SENSOR_EXPOSURE_AUTO 0x01
#define SENSOR_EXPOSURE_ZONE (0x01<<1)

#define FOCUS_ZONE_CNT_MAX   6

typedef enum {
	SENSOR_OP_SUCCESS = SENSOR_SUCCESS,
	SENSOR_OP_PARAM_ERR,
	SENSOR_OP_STATUS_ERR,
	SENSOR_OP_ERR,
	SENSOR_OP_MAX = 0xFFFF
} ERR_SENSOR_E;

typedef enum {
	SENSOR_MAIN = 0,
	SENSOR_SUB,
	SENSOR_ATV = 5,
	SENSOR_ID_MAX
} SENSOR_ID_E;

typedef enum {
	SENSOR_TYPE_NONE = 0x00,
	SENSOR_TYPE_IMG_SENSOR,
	SENSOR_TYPE_ATV,
	SENSOR_TYPE_MAX
} SENSOR_TYPE_E;

typedef enum {
	SENSOR_AVDD_3800MV = 0,
	SENSOR_AVDD_3000MV,
	SENSOR_AVDD_2800MV,
	SENSOR_AVDD_2500MV,
	SENSOR_AVDD_1800MV,
	SENSOR_AVDD_1500MV,
	SENSOR_AVDD_1300MV,
	SENSOR_AVDD_1200MV,
	SENSOR_AVDD_CLOSED,
	SENSOR_AVDD_UNUSED
} SENSOR_AVDD_VAL_E;

typedef enum {
	SENSOR_MCLK_12M = 12,
	SENSOR_MCLK_13M = 13,
	SENSOR_MCLK_24M = 24,
	SENSOR_MCLK_26M = 26,
	SENSOR_MCLK_MAX
} SENSOR_M_CLK_E;

typedef enum {
	SENSOR_INTERFACE_CCIR601_8BITS = 0,
	SENSOR_INTERFACE_CCIR601_4BITS,
	SENSOR_INTERFACE_CCIR601_2BITS,
	SENSOR_INTERFACE_CCIR601_1BITS,
	SENSOR_INTERFACE_CCIR656_8BITS,
	SENSOR_INTERFACE_CCIR656_4BITS,
	SENSOR_INTERFACE_CCIR656_2BITS,
	SENSOR_INTERFACE_CCIR656_1BITS,
	SENSOR_INTERFACE_SPI_8BITS,
	SENSOR_INTERFACE_SPI_4BITS,
	SENSOR_INTERFACE_SPI_4BITS_BE,
	SENSOR_INTERFACE_SPI_2BITS,
	SENSOR_INTERFACE_SPI_2BITS_BE,
	SENSOR_INTERFACE_SPI_1BITS,
	SENSOR_INTERFACE_SPI_1BITS_BE,
	SENSOR_INTERFACE_MAX
} SENSOR_INTERFACE_E;
typedef enum {
	SENSOR_MODE_COMMON_INIT = 0,
	SENSOR_MODE_PREVIEW_ONE,
	SENSOR_MODE_SNAPSHOT_ONE_FIRST,
	SENSOR_MODE_SNAPSHOT_ONE_SECOND,
	SENSOR_MODE_SNAPSHOT_ONE_THIRD,
	SENSOR_MODE_SNAPSHOT_ONE_FOURTH,
	SENSOR_MODE_PREVIEW_TWO,
	SENSOR_MODE_SNAPSHOT_TWO_FIRST,
	SENSOR_MODE_SNAPSHOT_TWO_SECOND,
	SENSOR_MODE_SNAPSHOT_TWO_THIRD,
	SENSOR_MODE_MAX
} SENSOR_MODE_E;

typedef enum {
	SENSOR_IMAGE_FORMAT_YUV422 = 0,
	SENSOR_IMAGE_FORMAT_YUV420,
	SENSOR_IMAGE_FORMAT_RAW,
	SENSOR_IMAGE_FORMAT_RGB565,
	SENSOR_IMAGE_FORMAT_RGB666,
	SENSOR_IMAGE_FORMAT_RGB888,
	SENSOR_IMAGE_FORMAT_CCIR656,
	SENSOR_IMAGE_FORMAT_JPEG,
	SENSOR_IMAGE_FORMAT_MAX
} SENSOR_IMAGE_FORMAT;

typedef enum {
	SENSOR_IOCTL_RESET = 0,
	SENSOR_IOCTL_POWER,
	SENSOR_IOCTL_ENTER_SLEEP,
	SENSOR_IOCTL_IDENTIFY,
	SENSOR_IOCTL_WRITE_REG,
	SENSOR_IOCTL_READ_REG,
	SENSOR_IOCTL_CUS_FUNC_1,
	SENSOR_IOCTL_CUS_FUNC_2,
	SENSOR_IOCTL_AE_ENABLE,
	SENSOR_IOCTL_HMIRROR_ENABLE,
	SENSOR_IOCTL_VMIRROR_ENABLE,
	SENSOR_IOCTL_BRIGHTNESS,
	SENSOR_IOCTL_CONTRAST,
	SENSOR_IOCTL_SHARPNESS,
	SENSOR_IOCTL_SATURATION,
	SENSOR_IOCTL_PREVIEWMODE,
	SENSOR_IOCTL_IMAGE_EFFECT,
	SENSOR_IOCTL_BEFORE_SNAPSHOT,
	SENSOR_IOCTL_AFTER_SNAPSHOT,
	SENSOR_IOCTL_FLASH,
	SENSOR_IOCTL_READ_EV,
	SENSOR_IOCTL_WRITE_EV,
	SENSOR_IOCTL_READ_GAIN,
	SENSOR_IOCTL_WRITE_GAIN,
	SENSOR_IOCTL_READ_GAIN_SCALE,
	SENSOR_IOCTL_SET_FRAME_RATE,
	SENSOR_IOCTL_AF_ENABLE,
	SENSOR_IOCTL_AF_GET_STATUS,
	SENSOR_IOCTL_SET_WB_MODE,
	SENSOR_IOCTL_SET_DTP,
	SENSOR_IOCTL_GET_SKIP_FRAME,
	SENSOR_IOCTL_ISO,
	SENSOR_IOCTL_EXPOSURE_COMPENSATION,
	SENSOR_IOCTL_CHECK_IMAGE_FORMAT_SUPPORT,
	SENSOR_IOCTL_CHANGE_IMAGE_FORMAT,
	SENSOR_IOCTL_ZOOM,
	SENSOR_IOCTL_CUS_FUNC_3,
	SENSOR_IOCTL_FOCUS,
	SENSOR_IOCTL_ANTI_BANDING_FLICKER,
	SENSOR_IOCTL_VIDEO_MODE,
	SENSOR_IOCTL_PICK_JPEG_STREAM,
	SENSOR_IOCTL_SET_MERTERING_MODE,
	/*add wenfeng.yan for esd test*/
	SENSOR_IOCTL_GET_ESD,
	/*esd end*/
	SENSOR_IOCTL_MAX,
	SENSOR_IOCTL_SET_FPS
} SENSOR_IOCTL_CMD_E;

typedef enum {
	SENSOR_EXT_FOCUS_NONE = 0x00,
	SENSOR_EXT_FOCUS_TRIG,
	SENSOR_EXT_FOCUS_ZONE,
	SENSOR_EXT_FOCUS_MULTI_ZONE,
	SENSOR_EXT_FOCUS_MACRO,
	SENSOR_EXT_FOCUS_MAX
} SENSOR_EXT_FOCUS_CMD_E;

typedef enum {
	SENSOR_EXT_EXPOSURE_NONE = 0x00,
	SENSOR_EXT_EXPOSURE_AUTO,
	SENSOR_EXT_EXPOSURE_ZONE,
	SENSOR_EXT_EXPOSURE_MAX
} SENSOR_EXT_EXPOSURE_CMD_E;

typedef enum {
	SENSOR_EXT_FUNC_NONE = 0x00,
	SENSOR_EXT_FUNC_INIT,
	SENSOR_EXT_FOCUS_START,
	SENSOR_EXT_EXPOSURE_START,
	SENSOR_EXT_FUNC_MAX
} SENSOR_EXT_FUNC_CMD_E;

typedef enum {
	SENSOR_EXIF_CTRL_EXPOSURETIME = 0x00,
	SENSOR_EXIF_CTRL_FNUMBER,
	SENSOR_EXIF_CTRL_EXPOSUREPROGRAM,
	SENSOR_EXIF_CTRL_SPECTRALSENSITIVITY,
	SENSOR_EXIF_CTRL_ISOSPEEDRATINGS,
	SENSOR_EXIF_CTRL_OECF,
	SENSOR_EXIF_CTRL_SHUTTERSPEEDVALUE,
	SENSOR_EXIF_CTRL_APERTUREVALUE,
	SENSOR_EXIF_CTRL_BRIGHTNESSVALUE,
	SENSOR_EXIF_CTRL_EXPOSUREBIASVALUE,
	SENSOR_EXIF_CTRL_MAXAPERTUREVALUE,
	SENSOR_EXIF_CTRL_SUBJECTDISTANCE,
	SENSOR_EXIF_CTRL_METERINGMODE,
	SENSOR_EXIF_CTRL_LIGHTSOURCE,
	SENSOR_EXIF_CTRL_FLASH,
	SENSOR_EXIF_CTRL_FOCALLENGTH,
	SENSOR_EXIF_CTRL_SUBJECTAREA,
	SENSOR_EXIF_CTRL_FLASHENERGY,
	SENSOR_EXIF_CTRL_SPATIALFREQUENCYRESPONSE,
	SENSOR_EXIF_CTRL_FOCALPLANEXRESOLUTION,
	SENSOR_EXIF_CTRL_FOCALPLANEYRESOLUTION,
	SENSOR_EXIF_CTRL_FOCALPLANERESOLUTIONUNIT,
	SENSOR_EXIF_CTRL_SUBJECTLOCATION,
	SENSOR_EXIF_CTRL_EXPOSUREINDEX,
	SENSOR_EXIF_CTRL_SENSINGMETHOD,
	SENSOR_EXIF_CTRL_FILESOURCE,
	SENSOR_EXIF_CTRL_SCENETYPE,
	SENSOR_EXIF_CTRL_CFAPATTERN,
	SENSOR_EXIF_CTRL_CUSTOMRENDERED,
	SENSOR_EXIF_CTRL_EXPOSUREMODE,
	SENSOR_EXIF_CTRL_WHITEBALANCE,
	SENSOR_EXIF_CTRL_DIGITALZOOMRATIO,
	SENSOR_EXIF_CTRL_FOCALLENGTHIN35MMFILM,
	SENSOR_EXIF_CTRL_SCENECAPTURETYPE,
	SENSOR_EXIF_CTRL_GAINCONTROL,
	SENSOR_EXIF_CTRL_CONTRAST,
	SENSOR_EXIF_CTRL_SATURATION,
	SENSOR_EXIF_CTRL_SHARPNESS,
	SENSOR_EXIF_CTRL_DEVICESETTINGDESCRIPTION,
	SENSOR_EXIF_CTRL_SUBJECTDISTANCERANGE,
	SENSOR_EXIF_CTRL_MAX,
} SENSOR_EXIF_CTRL_E;

typedef enum {
	DCAMERA_ENVIRONMENT_NORMAL = 0x00,
	DCAMERA_ENVIRONMENT_NIGHT,
	DCAMERA_ENVIRONMENT_SUNNY,
	DCAMERA_ENVIRONMENT_SPORTS,
	DCAMERA_ENVIRONMENT_LANDSCAPE,
	DCAMERA_ENVIRONMENT_PORTRAIT,
	DCAMERA_ENVIRONMENT_PORTRAIT_NIGHT,
	DCAMERA_ENVIRONMENT_BACKLIGHT,
	DCAMERA_ENVIRONMENT_MACRO,
	DCAMERA_ENVIRONMENT_MANUAL = 30,
	DCAMERA_ENVIRONMENT_AUTO = 31,
	DCAMERA_ENVIRONMENT_MAX
} DCAMERA_PARAM_ENVIRONMENT_E;

typedef enum {
	DCAMERA_WB_MODE_AUTO = 0x00,
	DCAMERA_WB_MODE_INCANDESCENCE,
	DCAMERA_WB_MODE_U30,
	DCAMERA_WB_MODE_CWF,
	DCAMERA_WB_MODE_FLUORESCENT,
	DCAMERA_WB_MODE_SUN,
	DCAMERA_WB_MODE_CLOUD,
	DCAMERA_WB_MODE_MAX
} DCAMERA_PARAM_WB_MODE_E;

typedef enum {
	DCAMERA_EFFECT_NORMAL = 0x00,
	DCAMERA_EFFECT_BLACKWHITE,
	DCAMERA_EFFECT_RED,
	DCAMERA_EFFECT_GREEN,
	DCAMERA_EFFECT_BLUE,
	DCAMERA_EFFECT_YELLOW,
	DCAMERA_EFFECT_NEGATIVE,
	DCAMERA_EFFECT_CANVAS,
	DCAMERA_EFFECT_RELIEVOS,
	DCAMERA_EFFECT_MAX
} DCAMERA_PARAM_EFFECT_E;

typedef uint32_t(*SENSOR_IOCTL_FUNC_PTR) (uint32_t param);

typedef struct sensor_ioctl_func_tab_tag {
	/*1: Internal IOCTL function */
	uint32_t(*reset) (uint32_t param);
	uint32_t(*power) (uint32_t param);
	uint32_t(*enter_sleep) (uint32_t param);
	uint32_t(*identify) (uint32_t param);
	uint32_t(*write_reg) (uint32_t param);
	uint32_t(*read_reg) (uint32_t param);
	/*Custom function */
	uint32_t(*cus_func_1) (uint32_t param);
	uint32_t(*get_trim) (uint32_t param);
	/*External IOCTL function */
	uint32_t(*ae_enable) (uint32_t param);
	uint32_t(*hmirror_enable) (uint32_t param);
	uint32_t(*vmirror_enable) (uint32_t param);

	uint32_t(*set_brightness) (uint32_t param);
	uint32_t(*set_contrast) (uint32_t param);
	uint32_t(*set_sharpness) (uint32_t param);
	uint32_t(*set_saturation) (uint32_t param);
	uint32_t(*set_preview_mode) (uint32_t param);

	uint32_t(*set_image_effect) (uint32_t param);
	uint32_t(*before_snapshort) (uint32_t param);
	uint32_t(*after_snapshort) (uint32_t param);
	uint32_t(*flash) (uint32_t param);
	uint32_t(*read_ae_value) (uint32_t param);
	uint32_t(*write_ae_value) (uint32_t param);
	uint32_t(*read_gain_value) (uint32_t param);
	uint32_t(*write_gain_value) (uint32_t param);
	uint32_t(*read_gain_scale) (uint32_t param);
	uint32_t(*set_frame_rate) (uint32_t param);
	uint32_t(*af_enable) (uint32_t param);
	uint32_t(*af_get_status) (uint32_t param);
	uint32_t(*set_wb_mode) (uint32_t param);
	uint32_t(*get_skip_frame) (uint32_t param);
	uint32_t(*set_DTP) (uint32_t param);
	uint32_t(*set_iso) (uint32_t param);
	uint32_t(*set_exposure_compensation) (uint32_t param);
	uint32_t(*check_image_format_support) (uint32_t param);
	uint32_t(*change_image_format) (uint32_t param);
	uint32_t(*set_zoom) (uint32_t param);
	/*CUSTOMER FUNCTION */
	uint32_t(*get_exif) (uint32_t param);
	uint32_t(*set_focus) (uint32_t param);
	uint32_t(*set_anti_banding_flicker) (uint32_t param);
	uint32_t(*set_video_mode) (uint32_t param);
	uint32_t(*pick_jpeg_stream) (uint32_t param);
	uint32_t(*set_meter_mode) (uint32_t param);
	/*add wenfeng.yan for esd test*/
	uint32_t(*get_esd) (uint32_t param);
	/*esd end*/
} SENSOR_IOCTL_FUNC_TAB_T, *SENSOR_IOCTL_FUNC_TAB_T_PTR;

typedef struct sensor_reg_tag {
	uint16_t reg_addr;
	uint16_t reg_value;
} SENSOR_REG_T, *SENSOR_REG_T_PTR;

typedef struct sensor_reg_bits_tag {
	uint16_t reg_addr;
	uint16_t reg_value;
	uint32_t reg_bits;
} SENSOR_REG_BITS_T, *SENSOR_REG_BITS_T_PTR;
typedef struct sensor_trim_tag {
	uint16_t trim_start_x;
	uint16_t trim_start_y;
	uint16_t trim_width;
	uint16_t trim_height;
	uint32_t line_time;
	uint32_t pclk;
} SENSOR_TRIM_T, *SENSOR_TRIM_T_PTR;

typedef struct _sensor_rect_tag {
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;
} SENSOR_RECT_T, *SENSOR_RECT_T_PTR;

typedef struct _sensor_ext_fun_tag {
	uint32_t cmd;
	uint32_t param;
	SENSOR_RECT_T zone;
} SENSOR_EXT_FUN_T, *SENSOR_EXT_FUN_T_PTR;
typedef struct _sensor_ext_fun_param_tag {
	uint8_t cmd;
	uint8_t param;
	uint16_t zone_cnt;
	SENSOR_RECT_T zone[FOCUS_ZONE_CNT_MAX];
} SENSOR_EXT_FUN_PARAM_T, *SENSOR_EXT_FUN_PARAM_T_PTR;

typedef struct sensor_reg_tab_info_tag {
	SENSOR_REG_T_PTR sensor_reg_tab_ptr;
	uint32_t reg_count;
	uint16_t width;
	uint16_t height;
	uint32_t xclk_to_sensor;
	SENSOR_IMAGE_FORMAT image_format;

} SENSOR_REG_TAB_INFO_T, *SENSOR_REG_TAB_INFO_T_PTR;

typedef struct sensor_mode_info_tag {
	SENSOR_MODE_E mode;
	uint16_t width;
	uint16_t height;
	uint16_t trim_start_x;
	uint16_t trim_start_y;
	uint16_t trim_width;
	uint16_t trim_height;
	uint32_t line_time;
	SENSOR_IMAGE_FORMAT image_format;
} SENSOR_MODE_INFO_T, *SENSOR_MODE_INFO_T_PTR;

typedef struct sensor_raw_info_tag {
	uint32_t res;
} SENSOR_RAW_INFO_T, *SENSOR_RAW_INFO_T_PTR;

typedef struct sensor_extend_info_tag {
	uint32_t focus_mode;
	uint32_t exposure_mode;
} SENSOR_EXTEND_INFO_T, *SENSOR_EXTEND_INFO_T_PTR;

typedef struct sensor_register_tag {
	uint32_t img_sensor_num;
	uint8_t cur_id;
	uint8_t is_register[SENSOR_ID_MAX];
} SENSOR_REGISTER_INFO_T, *SENSOR_REGISTER_INFO_T_PTR;

typedef struct sensor_exp_info_tag {
	SENSOR_IMAGE_FORMAT image_format;
	uint32_t image_pattern;
	uint8_t pclk_polarity;
	uint8_t vsync_polarity;
	uint8_t hsync_polarity;
	uint8_t pclk_delay;
	uint16_t source_width_max;
	uint16_t source_height_max;
	uint32_t environment_mode;
	uint32_t image_effect;
	uint32_t wb_mode;
	uint32_t step_count;	/*bit[0:7]: count of step in brightness, contrast, sharpness, saturation
				   bit[8:15] count of step in ISO
				   bit[16:23] count of step in exposure compensation
				   bit[24:31] reseved */
	SENSOR_MODE_INFO_T sensor_mode_info[SENSOR_MODE_MAX];
	SENSOR_IOCTL_FUNC_TAB_T_PTR ioctl_func_ptr;
	SENSOR_RAW_INFO_T_PTR raw_info_ptr;
	SENSOR_EXTEND_INFO_T_PTR ext_info_ptr;
	uint32_t preview_skip_num;
	uint32_t capture_skip_num;
	uint32_t preview_deci_num;
	uint32_t video_preview_deci_num;
	uint16_t threshold_eb;
	uint16_t threshold_mode;
	uint16_t threshold_start;
	uint16_t threshold_end;
	SENSOR_INTERFACE_E sensor_interface;
} SENSOR_EXP_INFO_T, *SENSOR_EXP_INFO_T_PTR;

typedef struct sensor_info_tag {
	uint8_t salve_i2c_addr_w;
	uint8_t salve_i2c_addr_r;
	uint8_t reg_addr_value_bits;
	uint8_t hw_signal_polarity;
	uint32_t environment_mode;
	uint32_t image_effect;
	uint32_t wb_mode;
	uint32_t step_count;
	uint16_t reset_pulse_level;
	uint16_t reset_pulse_width;
	uint32_t power_down_level;
	uint32_t identify_count;
	SENSOR_REG_T identify_code[SENSOR_IDENTIFY_CODE_COUNT];
	SENSOR_AVDD_VAL_E avdd_val;
	uint16_t source_width_max;
	uint16_t source_height_max;
	const char *name;
	SENSOR_IMAGE_FORMAT image_format;
	uint32_t image_pattern;
	SENSOR_REG_TAB_INFO_T_PTR resolution_tab_info_ptr;
	SENSOR_IOCTL_FUNC_TAB_T_PTR ioctl_func_tab_ptr;
	SENSOR_RAW_INFO_T_PTR raw_info_ptr;
	SENSOR_EXTEND_INFO_T_PTR ext_info_ptr;
	SENSOR_AVDD_VAL_E iovdd_val;
	SENSOR_AVDD_VAL_E dvdd_val;
	uint32_t preview_skip_num;
	uint32_t capture_skip_num;
	uint32_t preview_deci_num;
	uint32_t video_preview_deci_num;
	uint16_t threshold_eb;
	uint16_t threshold_mode;
	uint16_t threshold_start;
	uint16_t threshold_end;
	int32_t i2c_dev_handler;
	SENSOR_INTERFACE_E sensor_interface;
} SENSOR_INFO_T;

typedef enum {
	SENSOR_PARAM_WB_MODE_AUTO = 0x00,
	SENSOR_PARAM_WB_MODE_INCANDESCENCE,
	SENSOR_PARAM_WB_MODE_U30,
	SENSOR_PARAM_WB_MODE_CWF,
	SENSOR_PARAM_WB_MODE_FLUORESCENT,
	SENSOR_PARAM_WB_MODE_SUN,
	SENSOR_PARAM_WB_MODE_CLOUD,
	SENSOR_PARAM_WB_MODE_MAX
} SENSOR_PARAM_WB_MODE_E;

typedef enum {
	SENSOR_PARAM_ENVIRONMENT_NORMAL = 0x00,
	SENSOR_PARAM_ENVIRONMENT_NIGHT,
	SENSOR_PARAM_ENVIRONMENT_SUNNY,
	SENSOR_PARAM_ENVIRONMENT_SPORTS,
	SENSOR_PARAM_ENVIRONMENT_LANDSCAPE,
	SENSOR_PARAM_ENVIRONMENT_PORTRAIT,
	SENSOR_PARAM_ENVIRONMENT_PORTRAIT_NIGHT,
	SENSOR_PARAM_ENVIRONMENT_BACKLIGHT,
	SENSOR_PARAM_ENVIRONMENT_MACRO,
	SENSOR_PARAM_ENVIRONMENT_MANUAL = 30,
	SENSOR_PARAM_ENVIRONMENT_AUTO = 31,
	SENSOR_PARAM_ENVIRONMENT_MAX
} SENSOR_PARAM_ENVIRONMENT_E;

typedef enum {
	SENSOR_PARAM_EFFECT_NORMAL = 0x00,
	SENSOR_PARAM_EFFECT_BLACKWHITE,
	SENSOR_PARAM_EFFECT_RED,
	SENSOR_PARAM_EFFECT_GREEN,
	SENSOR_PARAM_EFFECT_BLUE,
	SENSOR_PARAM_EFFECT_YELLOW,
	SENSOR_PARAM_EFFECT_NEGATIVE,
	SENSOR_PARAM_EFFECT_CANVAS,
	SENSOR_PARAM_EFFECT_RELIEVOS,
	SENSOR_PARAM_EFFECT_MAX
} SENSOR_PARAM_EFFECT_E;

typedef enum {
	SENSOR_PARAM_WB = 0x00,
	SENSOR_PARAM_METERING,
	SENSOR_PARAM_DTP,
	SENSOR_PARAM_EV,
	SENSOR_PARAM_WORKMODE,
	SENSOR_PARAM_ISO,
	SENSOR_PARAM_SCENEMODE,
	SENSOR_PARAM_MAX
} SENSOR_PARAM_KEY_T;

typedef struct sensor_param_info {
	uint8_t wb_param;
	uint8_t metering_param;
	uint8_t dtp_param;
	uint8_t ev_param;
	uint8_t work_mode;
	uint8_t iso_param;
	uint8_t scenemode_param;
} SENSOR_PARAM_INFO_T;


/*****************************************************************************/
//  Description:    This function is for sensor tuning
//  Author:         dhee79.lee@samsung.com
//  Note:           
/*****************************************************************************/
int32_t Sensor_regs_table_init(void);
int32_t Sensor_regs_table_write(char *name);
/*****************************************************************************/
int32_t Sensor_WriteReg(uint16_t subaddr, uint16_t data);
uint16_t Sensor_ReadReg(uint16_t subaddr);
int32_t Sensor_WriteReg_8bits(uint16_t reg_addr, uint8_t value);
int32_t Sensor_ReadReg_8bits(uint8_t reg_addr, uint8_t * reg_val);
ERR_SENSOR_E Sensor_SendRegTabToSensor(SENSOR_REG_TAB_INFO_T *
				       sensor_reg_tab_info_ptr);
uint32_t Sensor_Init(uint32_t sensor_id);
BOOLEAN Sensor_IsInit(void);
ERR_SENSOR_E Sensor_SetMode(SENSOR_MODE_E mode);
ERR_SENSOR_E Sensor_SetModeCamcorder();
uint32_t Sensor_Ioctl(uint32_t cmd, uint32_t arg);
SENSOR_EXP_INFO_T *Sensor_GetInfo(void);
ERR_SENSOR_E Sensor_Close(void);
void Sensor_Reset(uint32_t level);
BOOLEAN Sensor_PowerDown(BOOLEAN power_down);
void Sensor_SetVoltage(SENSOR_AVDD_VAL_E dvdd_val,
		       SENSOR_AVDD_VAL_E avdd_val, SENSOR_AVDD_VAL_E iodd_val);
BOOLEAN Sensor_PowerDownFront(BOOLEAN power_down);
BOOLEAN Sensor_SetResetLevel(BOOLEAN plus_level);
int Sensor_SetMCLK(uint32_t mclk);
BOOLEAN Sensor_IsOpen(void);
uint32_t Sensor_SetCurId(SENSOR_ID_E sensor_id);
SENSOR_ID_E Sensor_GetCurId(void);
SENSOR_REGISTER_INFO_T_PTR Sensor_GetRegisterInfo(void);
uint32_t Sensor_SetSensorType(SENSOR_TYPE_E sensor_type);
void ImgSensor_DeleteMutex(void);
void ImgSensor_GetMutex(void);
void ImgSensor_GetMutex(void);
void ImgSensor_PutMutex(void);
uint32_t Sensor_SetFlash(uint32_t is_open);
struct i2c_client *Sensor_GetI2CClien(void);
uint32 Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_E cmd, uint32 param);
EXIF_SPEC_PIC_TAKING_COND_T *Sensor_GetSensorExifInfo(void);
int Sensor_SetSensorParam(uint8_t *buf);
int Sensor_GetSensorParam(uint8_t *buf,uint8_t *is_saved_ptr);
void Sensor_SetSensorParamByKey(int key, uint8_t value);
uint8_t Sensor_GetSensorParamByKey(int key);
#endif

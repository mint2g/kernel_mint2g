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
#include "common/sensor.h"
/**---------------------------------------------------------------------------*
 ** 						   Macro Define
 **---------------------------------------------------------------------------*/
#define SENSOR_TRACE SENSOR_PRINT
#define OV7660_I2C_ADDR_W	0x21	//0x42
#define OV7660_I2C_ADDR_R		0x21	//0x43
#define SENSOR_GAIN_SCALE		16
#define OV7675_COM11 0x3b
#define OV7675_REGCF 0xcf
#define PLL_ADDR    0x11
#define AE_ENABLE 0x13
/**---------------------------------------------------------------------------*
 ** 					Local Function Prototypes							  *
 **---------------------------------------------------------------------------*/
LOCAL uint32_t OV7660_Identify(uint32_t param);
#if 0
LOCAL uint32_t set_ov7660_ae_enable(uint32_t enable);
LOCAL uint32_t set_hmirror_enable(uint32_t enable);
LOCAL uint32_t set_vmirror_enable(uint32_t enable);
LOCAL uint32_t set_preview_mode(uint32_t preview_mode);
//LOCAL uint32_t OV7675_BeforeSnapshot(uint32_t param);
//LOCAL uint32_t OV7675_After_Snapshot(uint32_t param);

LOCAL uint32_t set_brightness(uint32_t level);
LOCAL uint32_t set_contrast(uint32_t level);
LOCAL uint32_t set_sharpness(uint32_t level);
LOCAL uint32_t set_saturation(uint32_t level);
LOCAL uint32_t set_image_effect(uint32_t effect_type);
LOCAL uint32_t read_ev_value(uint32_t value);
LOCAL uint32_t write_ev_value(uint32_t exposure_value);
LOCAL uint32_t read_gain_value(uint32_t value);
LOCAL uint32_t write_gain_value(uint32_t gain_value);
LOCAL uint32_t read_gain_scale(uint32_t value);
LOCAL uint32_t set_frame_rate(uint32_t param);
LOCAL uint32_t OV7660_set_work_mode(uint32_t mode);
LOCAL uint32_t set_ov7660_ev(uint32_t level);
LOCAL uint32_t set_ov7660_awb(uint32_t mode);
LOCAL uint32_t set_ov7660_anti_flicker(uint32_t mode);
LOCAL uint32_t set_ov7660_video_mode(uint32_t mode);
LOCAL uint32_t _ov7660_GetExifInfo(uint32_t param);
LOCAL uint32_t _ov7660_InitExifInfo(void);
LOCAL uint32_t s_preview_mode;
#endif
/**---------------------------------------------------------------------------*
 ** 						Local Variables 								 *
 **---------------------------------------------------------------------------*/
typedef enum {
	FLICKER_50HZ = 0,
	FLICKER_60HZ,
	FLICKER_MAX
} FLICKER_E;
LOCAL const SENSOR_REG_T ov7660_YUV_640X480[] = {
};

LOCAL const SENSOR_REG_T ov7660_YUV_MOTION_320X240[] = {
};

LOCAL SENSOR_REG_TAB_INFO_T s_OV7660_resolution_Tab_YUV[] = {
	// COMMON INIT
	{PNULL, 0, 640, 480, 24, SENSOR_IMAGE_FORMAT_YUV422},

	// YUV422 PREVIEW 1     
	{PNULL, 0, 640, 480, 24, SENSOR_IMAGE_FORMAT_YUV422},
	{PNULL, 0, 0, 0, 0, 0},
	{PNULL, 0, 0, 0, 0, 0},
	{PNULL, 0, 0, 0, 0, 0},

	// YUV422 PREVIEW 2 
	{PNULL, 0, 0, 0, 0, 0},
	{PNULL, 0, 0, 0, 0, 0},
	{PNULL, 0, 0, 0, 0, 0},
	{PNULL, 0, 0, 0, 0, 0}
};

LOCAL SENSOR_IOCTL_FUNC_TAB_T s_OV7660_ioctl_func_tab = {
	// Internal 
	PNULL,
	PNULL,
	PNULL,
	OV7660_Identify,

	PNULL,			// write register
	PNULL,			// read  register       
	PNULL,
	PNULL,

	// External
	PNULL,			//set_ov7675_ae_enable,
	PNULL,			//set_hmirror_enable,
	PNULL,			//set_vmirror_enable,

	PNULL,			//set_brightness,
	PNULL,			//set_contrast,
	PNULL,			//set_sharpness,
	PNULL,			//set_saturation,

	PNULL,			//set_preview_mode,     
	PNULL,			//set_image_effect,

	PNULL,			//OV7675_BeforeSnapshot, 
	PNULL,			//OV7675_After_Snapshot,

	PNULL,

	PNULL,			//read_ev_value,
	PNULL,			//write_ev_value,
	PNULL,			//read_gain_value,
	PNULL,			//write_gain_value,
	PNULL,			//read_gain_scale,
	PNULL,			//set_frame_rate,       
	PNULL,
	PNULL,
	PNULL,			//set_ov7675_awb,
	PNULL,
	PNULL,
	PNULL,			//set_ov7675_ev,
	PNULL,
	PNULL,
	PNULL,
	PNULL,			//_ov7675_GetExifInfo,
	PNULL,
	PNULL,			//set_ov7675_anti_flicker,
	PNULL,			//set_ov7675_video_mode,
	PNULL
};

/**---------------------------------------------------------------------------*
 ** 						Global Variables								  *
 **---------------------------------------------------------------------------*/
SENSOR_INFO_T g_OV7660_yuv_info = {
	OV7660_I2C_ADDR_W,	// salve i2c write address
	OV7660_I2C_ADDR_R,	// salve i2c read address

	0,			// bit0: 0: i2c register value is 8 bit, 1: i2c register value is 16 bit
	// bit2: 0: i2c register addr  is 8 bit, 1: i2c register addr  is 16 bit
	// other bit: reseved
	SENSOR_HW_SIGNAL_PCLK_N | SENSOR_HW_SIGNAL_VSYNC_P | \SENSOR_HW_SIGNAL_HSYNC_N,	// bit0: 0:negative; 1:positive -> polarily of pixel clock
	// bit2: 0:negative; 1:positive -> polarily of horizontal synchronization signal
	// bit4: 0:negative; 1:positive -> polarily of vertical synchronization signal
	// other bit: reseved                                                                                   

	// preview mode
	SENSOR_ENVIROMENT_NORMAL |
	    SENSOR_ENVIROMENT_NIGHT | SENSOR_ENVIROMENT_SUNNY,

	// image effect
	SENSOR_IMAGE_EFFECT_NORMAL |
	    SENSOR_IMAGE_EFFECT_BLACKWHITE |
	    SENSOR_IMAGE_EFFECT_RED |
	    SENSOR_IMAGE_EFFECT_GREEN |
	    SENSOR_IMAGE_EFFECT_BLUE |
	    SENSOR_IMAGE_EFFECT_YELLOW |
	    SENSOR_IMAGE_EFFECT_NEGATIVE | SENSOR_IMAGE_EFFECT_CANVAS,

	// while balance mode
	0,

	0x77777,		// brightness/contrast/sharpness/saturation/EV

	SENSOR_LOW_PULSE_RESET,	// reset pulse level
	20,			// reset pulse width(ms)

	SENSOR_HIGH_LEVEL_PWDN,	// 1: high level valid; 0: low level valid

	1,			// count of identify code
	{{0x0A, 0x76},		// supply two code to identify sensor.
	 {0x0B, 0x73}},		// for Example: index = 0-> Device id, index = 1 -> version id  

	SENSOR_AVDD_2800MV,	// voltage of avdd      

	640,			// max width of source image
	480,			// max height of source image
	"OV7660",		// name of sensor                                                                                               

	SENSOR_IMAGE_FORMAT_YUV422,	// define in SENSOR_IMAGE_FORMAT_E enum,
	// if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T
	SENSOR_IMAGE_PATTERN_YUV422_YUYV,	// pattern of input image form sensor;                  

	s_OV7660_resolution_Tab_YUV,	// point to resolution table information structure
	&s_OV7660_ioctl_func_tab,	// point to ioctl function table

	PNULL,			// information and table about Rawrgb sensor
	PNULL,			// extend information about sensor      
	SENSOR_AVDD_2800MV,	// iovdd
	SENSOR_AVDD_1800MV,	// dvdd
	1,			// skip frame num before preview 
	1,			// skip frame num before capture
	0,			// deci frame num during preview        
	2,			// deci frame num during video preview
	0,			// threshold enable(only analog TV)      
	0,			// atv output mode 0 fix mode 1 auto mode        
	0,			// atv output start postion       
	0,			// atv output end postion
	0
};

/**---------------------------------------------------------------------------*
 ** 							Function  Definitions
 **---------------------------------------------------------------------------*/
#if 0
LOCAL void OV7660_WriteReg(uint8_t subaddr, uint8_t data)
{

	Sensor_WriteReg_8bits(subaddr, data);

	SENSOR_TRACE("SENSOR: OV7675_WriteReg reg/value(%x,%x) !!\n", subaddr,
		     data);

}

LOCAL uint8_t OV7660_ReadReg(uint8_t subaddr)
{
	uint8_t value = 0;

	Sensor_ReadReg_8bits(subaddr, &value);

	//SENSOR_TRACE("SENSOR: OV7675_ReadReg reg/value(%x,%x) !!\n", subaddr, value);
	return value;
}
#endif
LOCAL uint32_t OV7660_Identify(uint32_t param)
{
	SENSOR_TRACE("SENSOR: OV7660_Identify: it is OV7660.\n");
	return 0;
}

#if 0
/******************************************************************************/
// Description: Intialize Exif information
// Global resource dependence: 
// Author:
// Note:
/******************************************************************************/
LOCAL uint32_t _ov7660_InitExifInfo(void)
{
	return 0;
}

LOCAL uint32_t set_ov7660_ae_enable(uint32_t enable)
{
	return 0;
}

LOCAL uint32_t set_hmirror_enable(uint32_t enable)
{

	SENSOR_TRACE("set_hmirror_enable: enable = %d", enable);

	return 0;
}

LOCAL uint32_t set_vmirror_enable(uint32_t enable)
{

	SENSOR_TRACE("set_vmirror_enable: enable = %d", enable);

	return 0;
}

LOCAL const SENSOR_REG_T ov7660_ev_tab[][4] = {
};

LOCAL uint32_t set_ov7660_ev(uint32_t level)
{
	return 0;
}

/******************************************************************************/
// Description: anti 50/60 hz banding flicker
// Global resource dependence: 
// Author:
// Note:
//              level  must smaller than 8
/******************************************************************************/
LOCAL uint32_t set_ov7660_anti_flicker(uint32_t mode)
{
	return 0;
}

/******************************************************************************/
// Description: set video mode
// Global resource dependence: 
// Author:
// Note:
//               
/******************************************************************************/
LOCAL const SENSOR_REG_T ov7660_video_mode_nand_tab[][4] = {

};

/******************************************************************************/
// Description: set video mode
// Global resource dependence: 
// Author:
// Note:
//               
/******************************************************************************/
LOCAL const SENSOR_REG_T ov7660_video_mode_nor_tab[][7] = {

};

LOCAL uint32_t set_ov7660_video_mode(uint32_t mode)
{
	return 0;
}

/******************************************************************************/
// Description: set wb mode 
// Global resource dependence: 
// Author:
// Note:
//              
/******************************************************************************/
LOCAL const SENSOR_REG_T ov7660_awb_tab[][7] = {
};

LOCAL uint32_t set_ov7660_awb(uint32_t mode)
{
	return 0;
}

/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author:
// Note:
//              level  must smaller than 8
/******************************************************************************/
LOCAL const SENSOR_REG_T ov7660_brightness_tab[][2] = {
};

LOCAL uint32_t set_brightness(uint32_t level)
{
	return 0;
}

LOCAL const SENSOR_REG_T ov7675_contrast_tab[][2] = {
};

LOCAL uint32_t set_contrast(uint32_t level)
{
	return 0;
}

LOCAL uint32_t set_sharpness(uint32_t level)
{
	return 0;
}

LOCAL uint32_t set_saturation(uint32_t level)
{
	return 0;
}

/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author:
// Note:
//              level  must smaller than 8
/******************************************************************************/

LOCAL uint32_t set_preview_mode(uint32_t preview_mode)
{
	return 0;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//              
/******************************************************************************/
LOCAL const SENSOR_REG_T ov7660_image_effect_tab[][4] = {

};

LOCAL uint32_t set_image_effect(uint32_t effect_type)
{
	return 0;
}

LOCAL uint32_t read_ev_value(uint32_t value)
{
	return 0;
}

LOCAL uint32_t write_ev_value(uint32_t exposure_value)
{
	return 0;
}

LOCAL uint32_t read_gain_value(uint32_t value)
{
	return 0;
}

LOCAL uint32_t write_gain_value(uint32_t gain_value)
{
	return 0;
}

LOCAL uint32_t read_gain_scale(uint32_t value)
{
	return SENSOR_GAIN_SCALE;
}

LOCAL uint32_t set_frame_rate(uint32_t param)
{
	return 0;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//              mode 0:normal;   1:night 
/******************************************************************************/
LOCAL const SENSOR_REG_T ov7660_mode_tab[][5] = {

};

LOCAL uint32_t OV7660_set_work_mode(uint32_t mode)
{
	return 0;
}

LOCAL uint32_t _ov7660_GetExifInfo(uint32_t param)
{
	return 0;
}
#endif
struct sensor_drv_cfg sensor_ov7660 = {
	.sensor_pos = CONFIG_DCAM_SENSOR_POS_OV7660,
	.sensor_name = "ov7660",
	.driver_info = &g_OV7660_yuv_info,
};

static int __init sensor_ov7660_init(void)
{
	return dcam_register_sensor_drv(&sensor_ov7660);
}

subsys_initcall(sensor_ov7660_init);

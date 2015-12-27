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
#define OV9660_I2C_ADDR_W	0x30//0x60
#define OV9660_I2C_ADDR_R		0x30//0x61 
/**---------------------------------------------------------------------------*
 ** 					Local Function Prototypes							  *
 **---------------------------------------------------------------------------*/
//LOCAL uint32_t OV9660_set_ae_enable(uint32_t enable);
//LOCAL uint32_t OV9660_set_hmirror_enable(uint32_t enable);
//LOCAL uint32_t OV9660_set_vmirror_enable(uint32_t enable);
LOCAL uint32_t OV9660_set_preview_mode(uint32_t preview_mode);
LOCAL uint32_t OV9660_Identify(uint32_t param);
LOCAL uint32_t OV9660_BeforeSnapshot(uint32_t param);	
LOCAL uint32_t OV9660_set_brightness(uint32_t level);
LOCAL uint32_t OV9660_set_contrast(uint32_t level);
//LOCAL uint32_t OV9660_set_sharpness(uint32_t level);
//LOCAL uint32_t OV9660_set_saturation(uint32_t level);
LOCAL uint32_t OV9660_set_image_effect(uint32_t effect_type);
LOCAL uint32_t OV9660_set_work_mode(uint32_t mode);
LOCAL uint32_t OV9660_chang_image_format(uint32_t param);
LOCAL uint32_t OV9660_check_image_format_support(uint32_t param);
LOCAL uint32_t OV9660_after_snapshot(uint32_t param);
LOCAL uint32_t set_OV9660_ev(uint32_t level);
LOCAL uint32_t set_OV9660_awb(uint32_t mode);
//LOCAL uint32_t set_OV9660_anti_flicker(uint32_t mode);
LOCAL uint32_t set_OV9660_video_mode(uint32_t mode); 
//LOCAL uint32_t OV9660_zoom(uint32_t level);
LOCAL uint32_t OV9660_get_skip_frame (uint32_t param);
/**---------------------------------------------------------------------------*
 ** 						Local Variables 								 *
 **---------------------------------------------------------------------------*/
//LOCAL uint32 s_preview_mode;
//LOCAL uint32 s_image_effect = 0;
const SENSOR_REG_T OV9660_YUV_COMMON[]= {
	//OV9663 refernece setting 05212007
	//24MHz //15FPS
	//VGA //YUV 8bit output
	//HREF positive
	//Vsync positive
	//AEC// Auto//
	//AGC// Auto// 16x ceiling//
	//Banding Filter//Auto//
	//AWB// Auto//
	//LC ON//
	//WBC ON//
	//Gamma ON//
	//DNS ON//
	//Sharpness ON//
	//Night mode off//
	////
	//Reset
	{0x12, 0x80},
        {SENSOR_WRITE_DELAY, 0x0a},//delay 10ms 	
	//Add some dealy or wait a few seconds after register reset
	//
	//IO output
	{0xd5, 0xff},
	{0xd6, 0x3f},
	//
	//Clock 24Mhz 15 FPS
	{0x3d, 0x3c},
	{0x11, 0x81},
	{0x2a, 0x00},
	{0x2b, 0x00},
	//
	//Power control
	{0x3a, 0xd9},
	{0x3b, 0x00},
	{0x3c, 0x58},
	{0x3e, 0x50},
	{0x71, 0x00},
	//
	//Sync signal
	{0x15, 0x00},
	//
	//Data Format YUV
	{0xd7, 0x10},
	{0x6a, 0x24},
	{0x85, 0xe7},
	//
	//Sample Option
	{0x63, 0x00},
	//
	//Windowing
	{0x12, 0x40},
	{0x4d, 0x09},
	{0x17, 0x0c},
	{0x18, 0x5c},
	{0x19, 0x02},
	{0x1a, 0x3f},
	{0x03, 0x03},
	{0x32, 0xb4},
	{0x2b, 0x00},
	{0x5c, 0x80},
	//
	//BLC
	{0x36, 0xb4},
	{0x65, 0x10},
	{0x70, 0x02},
	{0x71, 0x9f},
	{0x64, 0xa4},
	//
	//AEC// Average//9 zone//
	{0x43, 0x00},
	{0x5d, 0x55},
	{0x5e, 0x57},
	{0x5f, 0x21},
	//
	// Brightness
	{0x24, 0x3e},
	{0x25, 0x38},
	{0x26, 0x72},
	//
	//BF auto //60Hz
	//{0x14, 0x68},
	//{0x0c, 0x3a},//38
	//{0x4f, 0x4f},
	//{0x50, 0x42},
	//{0x5a, 0x67},
	//BF 50Hz
	{0x14, 0x68},
	{0x0c, 0x3c},
	{0x4f, 0x4e},
	{0x50, 0x41},
	{0x5a, 0x67},
	{0x2a, 0x00},
	{0x2b, 0x4c},//for 14.3
	//
	//LC enable Largon9306//
	{0x7d, 0x30},
	{0x7e, 0x00},
	{0x82, 0x03},
	{0x7f, 0x00},
	{0x83, 0x07},
	{0x80, 0x03},
	{0x81, 0x04},
	//
	//AWB advance Largon9306//
	{0x96, 0xf0},
	{0x97, 0x00},
	{0x92, 0x33},
	{0x94, 0x5a},
	{0x93, 0x3a},
	{0x95, 0x48},
	{0x91, 0xfc},
	{0x90, 0xff},
	{0x8e, 0x4e},
	{0x8f, 0x4e},
	{0x8d, 0x13},
	{0x8c, 0x0c},
	{0x8b, 0x0c},
	{0x86, 0x9e},
	{0x87, 0x11},
	{0x88, 0x22},
	{0x89, 0x05},
	{0x8a, 0x03},
	//
	// Gamma enable
	{0x9b, 0x0e},
	{0x9c, 0x1c},
	{0x9d, 0x34},
	{0x9e, 0x5a},
	{0x9f, 0x68},
	{0xa0, 0x76},
	{0xa1, 0x82},
	{0xa2, 0x8e},
	{0xa3, 0x98},
	{0xa4, 0xa0},
	{0xa5, 0xb0},
	{0xa6, 0xbe},
	{0xa7, 0xd2},
	{0xa8, 0xe2},
	{0xa9, 0xee},
	{0xaa, 0x18},
	//
	//De-noise enable auto
	{0xab, 0xe7},
	{0xb0, 0x43},
	{0xac, 0x04},
	{0x84, 0x40},
	//
	//Sharpness
	{0xad, 0x81},
	{0xd9, 0x01},
	{0xda, 0x00},
	{0xae, 0x10},
	//
	//Scaling
	{0xab, 0xe7},
	{0xb9, 0x50},
	{0xba, 0x3c},
	{0xbb, 0x50},
	{0xbc, 0x3c},
	//
	//CMX
	{0xbd, 0x8 },
	{0xbe, 0x19},
	{0xbf, 0x2 },
	{0xc0, 0x8},
	{0xc1, 0x2a},
	{0xc2, 0x34},
	{0xc3, 0x2d},
	{0xc4, 0x2d},
	{0xc5, 0x00},
	{0xc6, 0x98},
	{0xc7, 0x18},
	{0x69, 0x48},
	//
	//UVave
	{0x74, 0xc0},
	//
	//UVadj
	{0x7c, 0x28},
	{0x65, 0x11},
	{0x66, 0x00},
	{0x41, 0xc0},
	{0x5b, 0x24},
	{0x60, 0x82},
	{0x05, 0x07},
	{0x03, 0x03},
	{0xd2, 0x94},
	//
	//SAT & Brightness
	{0xc8, 0x06},
	{0xcb, 0x40},
	{0xcc, 0x40},
	{0xcf, 0x00},
	{0xd0, 0x20},
	{0xd1, 0x00},
	{0xc7, 0x18},
	//
	//BLC
	{0x72, 0xc0},
	{0x0d, 0x92},
	{0x0d, 0x90},
	//AE default
	{0x10, 0x62}
};

const SENSOR_REG_T OV9660_YUV_1280X960[]=
{
	{0x63, 0x01},
	//Windowing
	{0x12, 0x00},
	{0x3d, 0x3c},
	{0x4d, 0x11},
	{0x17, 0x0c},
	{0x18, 0x5c},
	{0x19, 0x01},
	{0x1a, 0x82},
	{0x03, 0x0f},
	{0x2b, 0x00},
	{0x32, 0x34},
	{0x5c, 0x00},
	{0x71, 0x9c},
	{0x64, 0x24},
	{0x0c, 0x3c},
	{0x4f, 0x4e},//
	{0x50, 0x41},//
	{0x5a, 0xd0},//
	//Sharpness
	{0xad, 0x82},
	{0xd9, 0x12},
	{0xda, 0x00},
	{0xae, 0x10},
	//
	//Scaling
	{0xab, 0xe7},
	{0xb9, 0xa0},
	{0xba, 0x80},
	{0xbb, 0xa0},
	{0xbc, 0x80},
	{0x03, 0x0f},
	{0x0d, 0x82},
	{0x0d, 0x80}
};

const SENSOR_REG_T OV9660_YUV_640X480[]=
{
	{0x63, 0x00},
	{0x12, 0x40},
	{0x3d, 0x3c},
	{0x4d, 0x09},
	{0x17, 0x0c},
	{0x18, 0x5c},
	{0x19, 0x02},
	{0x1a, 0x3f},
	{0x03, 0x03},
	{0x32, 0xb4},
	{0x2b, 0x00},
	{0x5c, 0x80},
	{0xab, 0xe7},
	{0xb9, 0x50},
	{0xba, 0x3c},
	{0xbb, 0x50},
	{0x71, 0x9f},
	{0x64, 0xa4},
	{0x0c, 0x3c},
	{0x4f, 0x4e},
	{0x50, 0x41},
	{0x5a, 0x67},
	{0x2a, 0x00},
	{0x2b, 0x00},
	{0x2d, 0x00},
	{0x2e, 0x00},
         {0x46, 0x1b},//for 14.3
         {0x47, 0x00},
	//Sharpness
	{0xad, 0x81},
	{0xd9, 0x01},
	{0xda, 0x00},
	{0xae, 0x10},
	//
	//Scaling
	{0xab, 0xe7},
	{0xb9, 0x50},
	{0xba, 0x3c},
	{0xbb, 0x50},
	{0xbc, 0x3c},
	{0x03, 0x03},
	{0x0d, 0x92},
	{0x0d, 0x90},
       {SENSOR_WRITE_DELAY, 100}//delay 100ms	

};

const SENSOR_REG_T OV9660_yuv422_mode[]=
{
        /**/
        {0xff, 0xff},
};

LOCAL SENSOR_REG_TAB_INFO_T s_OV9660_resolution_Tab_YUV[]=
{	
        // COMMON INIT
        {ADDR_AND_LEN_OF_ARRAY(OV9660_YUV_COMMON), 0, 0, 24, SENSOR_IMAGE_FORMAT_YUV422}, 

        // YUV422 PREVIEW 1 
        {ADDR_AND_LEN_OF_ARRAY(OV9660_YUV_640X480), 640, 480, 24, SENSOR_IMAGE_FORMAT_YUV422},
        {ADDR_AND_LEN_OF_ARRAY(OV9660_YUV_1280X960), 1280, 960, 12, SENSOR_IMAGE_FORMAT_YUV422},
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},

        // YUV422 PREVIEW 2 
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0}
};

LOCAL SENSOR_IOCTL_FUNC_TAB_T s_OV9660_ioctl_func_tab = 
{
        // Internal 
        PNULL,
        PNULL,
        PNULL,
        OV9660_Identify,

        PNULL,			// write register
        PNULL,			// read  register	
        PNULL,
        PNULL,

        // External
        PNULL,	// OV9660_set_ae_enable
        PNULL,	// OV9660_set_hmirror_enable
        PNULL,	// OV9660_set_vmirror_enable

        OV9660_set_brightness,	
        OV9660_set_contrast,
        PNULL,//OV9660_set_sharpness,
        PNULL,//OV9660_set_saturation,

        OV9660_set_preview_mode,	// OV9660_set_preview_mode
        OV9660_set_image_effect,

        OV9660_BeforeSnapshot,
        OV9660_after_snapshot,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        set_OV9660_awb,
        OV9660_get_skip_frame,
        PNULL,
        set_OV9660_ev,
        OV9660_check_image_format_support,
        OV9660_chang_image_format,
        PNULL,	// OV9660_zoom
        PNULL,
        PNULL,
        PNULL,//set_OV9660_anti_flicker
        set_OV9660_video_mode,	// set_OV9660_video_mode

        PNULL,
};

LOCAL SENSOR_EXTEND_INFO_T OV9660_ext_info = {
        256,    //jpeg_seq_width
        0//1938       //jpeg_seq_height
};

/**---------------------------------------------------------------------------*
 ** 						Global Variables								  *
 **---------------------------------------------------------------------------*/
 SENSOR_INFO_T g_OV9660_yuv_info =
{
        OV9660_I2C_ADDR_W,				// salve i2c write address
        OV9660_I2C_ADDR_R, 				// salve i2c read address

        0,								// bit0: 0: i2c register value is 8 bit, 1: i2c register value is 16 bit
        								// bit2: 0: i2c register addr  is 8 bit, 1: i2c register addr  is 16 bit
        								// other bit: reseved
        SENSOR_HW_SIGNAL_PCLK_N|\
        SENSOR_HW_SIGNAL_VSYNC_N|\
        SENSOR_HW_SIGNAL_HSYNC_P,		// bit0: 0:negative; 1:positive -> polarily of pixel clock
        								// bit2: 0:negative; 1:positive -> polarily of horizontal synchronization signal
        								// bit4: 0:negative; 1:positive -> polarily of vertical synchronization signal
        								// other bit: reseved											
        										
        // preview mode
        SENSOR_ENVIROMENT_NORMAL|\
        SENSOR_ENVIROMENT_NIGHT|\
        SENSOR_ENVIROMENT_SUNNY,		

        // image effect
        SENSOR_IMAGE_EFFECT_NORMAL|\
        SENSOR_IMAGE_EFFECT_BLACKWHITE|\
        SENSOR_IMAGE_EFFECT_RED|\
        SENSOR_IMAGE_EFFECT_GREEN|\
        SENSOR_IMAGE_EFFECT_BLUE|\
        SENSOR_IMAGE_EFFECT_YELLOW|\
        SENSOR_IMAGE_EFFECT_NEGATIVE,

        // while balance mode
        SENSOR_WB_MODE_AUTO|\
        SENSOR_WB_MODE_INCANDESCENCE|\
        SENSOR_WB_MODE_CWF|\
        SENSOR_WB_MODE_FLUORESCENT|\
        SENSOR_WB_MODE_SUN,
        	
        7,								// bit[0:7]: count of step in brightness, contrast, sharpness, saturation
        								// bit[8:31] reseved

        SENSOR_LOW_PULSE_RESET,			// reset pulse level
        20,								// reset pulse width(ms)

        SENSOR_HIGH_LEVEL_PWDN,			// 1: high level valid; 0: low level valid

        1,								// count of identify code
        {{0x0A, 0x96},						// supply two code to identify sensor.
        {0x0B, 0x63}},						// for Example: index = 0-> Device id, index = 1 -> version id											
        										
        SENSOR_AVDD_2800MV,				// voltage of avdd	

        1280,							// max width of source image
        960,							// max height of source image
        "OV9660",						// name of sensor												

        SENSOR_IMAGE_FORMAT_YUV422,		// define in SENSOR_IMAGE_FORMAT_E enum,SENSOR_IMAGE_FORMAT_MAX
        								// if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T

        SENSOR_IMAGE_PATTERN_YUV422_YUYV,	// pattern of input image form sensor;			

        s_OV9660_resolution_Tab_YUV,	// point to resolution table information structure
        &s_OV9660_ioctl_func_tab,		// point to ioctl function table
        		
        PNULL,							// information and table about Rawrgb sensor
        &OV9660_ext_info,				// extend information about sensor
        SENSOR_AVDD_2800MV,                     // iovdd
        SENSOR_AVDD_1300MV,                      // dvdd
        0,                     // skip frame num before preview 
        0,                      // skip frame num before capture		
        0,                      // deci frame num during preview	
        0,                      // deci frame num during video preview
        0,                     // threshold enable(only analog TV)	
        0,                     // atv output mode 0 fix mode 1 auto mode	
        0,                    // atv output start postion	
        0,                    // atv output end postion
        0                     // IIC
};
/**---------------------------------------------------------------------------*
 ** 							Function  Definitions
 **---------------------------------------------------------------------------*/
 #if 0
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t OV9660_set_ae_enable(uint32_t enable)
{
	unsigned char value;
	
	SENSOR_PRINT("OV9660_set_ae_enable: enable = %d", enable);
	
	value = Sensor_ReadReg(0x13);
	value &= 0xFE;
	value |= enable;
	Sensor_WriteReg(0x13, value);
	
	return 0;
}

/******************************************************************************/
// Description: anti 50/60 hz banding flicker
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/
const SENSOR_REG_T OV9660_banding_flicker_tab[][3]=
{
    // 50HZ
    {
        {0x0c, 0x3c},{0xff, 0xff},{0xff, 0xff}
    },    
    //60HZ
    {
        {0x0c, 0x38},{0xff, 0xff},{0xff, 0xff}
    },
};   

LOCAL uint32_t set_OV9660_anti_flicker(uint32_t mode)
{//107 us
	uint16_t i;
	SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV9660_banding_flicker_tab[mode];

	//    SCI_ASSERT(PNULL != sensor_reg_ptr);

	for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr)|| (0xFF != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

	msleep(100); 
	SENSOR_PRINT("SENSOR: set_OV9660_flicker: 0x%x .\n", mode);
	return 0;
}
 #endif
/******************************************************************************/
// Description: set video mode
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
const SENSOR_REG_T OV9660_video_mode_nand_tab[][10]=
{
        // normal mode
        {
        {0x11, 0x81},
        {0x3d, 0x3c},
        {0x46, 0x1b},
        {0x47, 0x00},
        {0x14, 0x28},
        {0x4f, 0x4e},
        {0x50, 0x41},
        {0x5a, 0x67},
        {SENSOR_WRITE_DELAY, 500},
        {0xff, 0xff},
        },    
        //vodeo mode
        {
        {0x11, 0x81},
        {0x3d, 0x3c},
        {0x46, 0x1b},
        {0x47, 0x00},
        {0x14, 0x28},
        {0x4f, 0x4e},
        {0x50, 0x41},
        {0x5a, 0x67},
        {SENSOR_WRITE_DELAY, 500},
        {0xff, 0xff},
        },
};   

/******************************************************************************/
// Description: set video mode
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
const SENSOR_REG_T OV9660_video_mode_nor_tab[][10]=
{
        // normal mode
        {
        {0x11, 0x81},
        {0x3d, 0x3c},
        {0x46, 0x1b},
        {0x47, 0x00},
        {0x14, 0x28},
        {0x4f, 0x4e},
        {0x50, 0x41},
        {0x5a, 0x67},
        {SENSOR_WRITE_DELAY, 500},
        {0xff, 0xff},
        },    
        //vodeo mode
        {
        {0x11, 0x81},
        {0x3d, 0x3c},
        {0x46, 0x1b},
        {0x47, 0x00},
        {0x14, 0x28},
        {0x4f, 0x4e},
        {0x50, 0x41},
        {0x5a, 0x67},
        {SENSOR_WRITE_DELAY, 500},
        {0xff, 0xff},
        },
};   

LOCAL uint32_t set_OV9660_video_mode(uint32_t mode)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = PNULL;

        sensor_reg_ptr = (SENSOR_REG_T*)OV9660_video_mode_nand_tab[mode];

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr)|| (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        msleep(100); 
        SENSOR_PRINT("SENSOR: set_vodeo_mode: %d ",mode);
        return 0;
}
/******************************************************************************/
// Description: set wb mode 
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
const SENSOR_REG_T OV9660_awb_tab[][6]=
{
        //AUTO
        {
        {0x13, 0xe7},
        {0xff, 0xff},
        {0xff, 0xff},       
        {0xff, 0xff},    
        {0xff, 0xff},    
        {0xff, 0xff}            
        },    
        //INCANDESCENCE:
        {
        {0x13, 0xe5},
        {0x02, 0x46},
        {0x16, 0x40},       
        {0x01, 0x56},    
        {0xff, 0xff},    
        {0xff, 0xff}   
        },
        //U30
        {
        {0x13, 0xe5},
        {0x02, 0x46},
        {0x16, 0x40},       
        {0x01, 0x56},    
        {0xff, 0xff},    
        {0xff, 0xff}   
        },  
        //CWF
        {
        {0x13, 0xe5},
        {0x02, 0x4b},
        {0x16, 0x40},       
        {0x01, 0x4a},    
        {0xff, 0xff},    
        {0xff, 0xff}           
        },    
        //FLUORESCENT:
        {
        {0x13, 0xe7},
        {0x02, 0x4d},
        {0x16, 0x40},       
        {0x01, 0x4a},    
        {0xff, 0xff},    
        {0xff, 0xff}           
        },
        //SUN:
        {
        {0x13, 0xe5},
        {0x02, 0x67},
        {0x16, 0x45},       
        {0x01, 0x3c},    
        {0xff, 0xff},    
        {0xff, 0xff}        
        },
        //CLOUD:
        {
        {0x13, 0xe5},
        {0x02, 0x70},
        {0x16, 0x45},       
        {0x01, 0x3c},     
        {0xff, 0xff},    
        {0xff, 0xff}   
        },
};

LOCAL uint32_t set_OV9660_awb(uint32_t mode)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV9660_awb_tab[mode];

        if(mode >= 7) {
                SENSOR_PRINT("set_OV9660_awb:param error,mode=%d\n",mode);
                return SENSOR_FAIL;
        }

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        msleep(100); 
        SENSOR_PRINT("SENSOR: set_awb_mode: mode = %d", mode);
        return 0;
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
const SENSOR_REG_T OV9660_ev_tab[][4]=
{
        {{0x24, 0x1e}, {0x25, 0x18}, {0x26, 0x40}, {0xff, 0xff}},
        {{0x24, 0x25}, {0x25, 0x20}, {0x26, 0x61}, {0xff, 0xff}},
        {{0x24, 0x32}, {0x25, 0x2e}, {0x26, 0x61}, {0xff, 0xff}},
        {{0x24, 0x3e}, {0x25, 0x38}, {0x26, 0x72}, {0xff, 0xff}},
        {{0x24, 0x4e}, {0x25, 0x48}, {0x26, 0x82}, {0xff, 0xff}},
        {{0x24, 0x5e}, {0x25, 0x58}, {0x26, 0x93}, {0xff, 0xff}},
        {{0x24, 0x6e}, {0x25, 0x68}, {0x26, 0x94}, {0xff, 0xff}}
};
LOCAL uint32_t set_OV9660_ev(uint32_t level)
{
        uint16_t i;    
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV9660_ev_tab[level];

        if(level >= 7) {
                SENSOR_PRINT("set_OV9660_ev:param error,level=%d .\n",level);
                return SENSOR_FAIL;
        }
        for(i = 0;(0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        msleep(100);
        SENSOR_PRINT("SENSOR: set_ev: level = %d", level);
        return 0;
}
#if 0
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t OV9660_set_hmirror_enable(uint32_t enable)
{
	uint16_t reg04;

	Sensor_WriteReg(0xff, 0x01);
	reg04 = Sensor_ReadReg(0x04);
	
	if (enable)
	{
		reg04 = (reg04 | 0x80);
		Sensor_WriteReg(0xff, 0x01);
		Sensor_WriteReg(0x04, reg04);
	}
	else
	{
		reg04 = (reg04 & (~(0x80)));
		Sensor_WriteReg(0xff, 0x01);
		Sensor_WriteReg(0x04, reg04);
	}
	return 0;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t OV9660_set_vmirror_enable(uint32_t enable)
{
	uint16_t reg04;
	
	Sensor_WriteReg(0xff, 0x01);
	reg04 = Sensor_ReadReg(0x04);
	
	if (enable)
	{
		reg04 = (reg04 | 0x50);
		Sensor_WriteReg(0xff, 0x01);
		Sensor_WriteReg(0x04, reg04);
	}
	else
	{
		reg04 = (reg04 & (~(0x50)));
		Sensor_WriteReg(0xff, 0x01);
		Sensor_WriteReg(0x04, reg04);
	}
	return 0;
}
#endif
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t OV9660_set_preview_mode(uint32_t preview_mode)
{
        SENSOR_PRINT("set_preview_mode: preview_mode = %d", preview_mode);

        switch (preview_mode) {
        case SENSOR_PARAM_ENVIRONMENT_NORMAL: 
                OV9660_set_work_mode(0);
                break;
        case SENSOR_PARAM_ENVIRONMENT_NIGHT:
                OV9660_set_work_mode(1);
                break;
        case SENSOR_PARAM_ENVIRONMENT_SUNNY:
                OV9660_set_work_mode(0);
                break;
        default:
                break;
        }
        msleep(300);	
        return 0;
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t OV9660_Identify(uint32_t param)
{
#define OV9660_PID_VALUE	0x96	
#define OV9660_PID_ADDR 	0x0A
#define OV9660_VER_VALUE	0x63	
#define OV9660_VER_ADDR 	0x0B
        	
        uint8_t pid_value = 0;
        uint8_t ver_value = 0;
        BOOLEAN ret_value = 0XFF;

        pid_value =  Sensor_ReadReg(OV9660_PID_ADDR);
        // Get Version Code	
        ver_value =  Sensor_ReadReg(OV9660_VER_ADDR);

        if(OV9660_PID_VALUE == pid_value) {
                // Get Device Code		
                if(OV9660_VER_VALUE == ver_value) {
                        SENSOR_PRINT("That is OV9660 sensor !");		
                } else {
                        SENSOR_PRINT("OV9660_Identify: That is OV%x%x sensor !", pid_value, ver_value);
                }
                ret_value = 0;
        }
        SENSOR_PRINT("OV9660_Identify: PID = %x, VER = %x",pid_value, ver_value);	
        return (uint32_t)ret_value;
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL void OV9660_CalculateExposureGain(SENSOR_MODE_E sensor_preview_mode, SENSOR_MODE_E sensor_snapshot_mode)
{
        uint32_t   TgtMaxExp =1248;
        uint32_t   PrvExp,TgtExp = 0;
        uint32_t   dummyline = 0;
        uint8_t     PrvGain,val45,val10,val04,val2d,val2e;
        uint32_t  preview_xclk = s_OV9660_resolution_Tab_YUV[sensor_preview_mode].xclk_to_sensor;
        uint32_t  snapshot_xclk = s_OV9660_resolution_Tab_YUV[sensor_snapshot_mode].xclk_to_sensor;

        if((0==preview_xclk)||(0==snapshot_xclk)) {
                SENSOR_PRINT("OV9660_CalculateExposureGain: error! .\n");
                SENSOR_FAIL;
        }
        if(sensor_preview_mode != sensor_snapshot_mode) 
        {
                // 1. Close AE/AG(0x13 register).
                Sensor_WriteReg(0x13,0xe0);
                // 2. Read out gain/expsoure values, 0x00 and 0x10,0x04,0x45 register, (PrvGain, PrvExp)
                // 3. Change resolution and frame to capture mode
                // 4 . Calculate exposure/gain
                PrvGain = Sensor_ReadReg(0x00);
                PrvExp = Sensor_ReadReg(0x10) <<2;
                PrvExp += (Sensor_ReadReg(0x04) & 0x03); 
                PrvExp += (Sensor_ReadReg(0x45) & 0x3f) << 10;
                PrvExp += Sensor_ReadReg(0x2d);
                PrvExp += Sensor_ReadReg(0x2e)<<8;
                SENSOR_PRINT("OV9660: prvGain %d, prvExp %d", PrvGain, PrvExp);
                TgtExp = PrvExp * snapshot_xclk / preview_xclk;
                if ( PrvGain & 0x10 ) {
                        PrvGain &= 0xef;
                        TgtExp <<= 1;
                }
                if ( PrvGain & 0x20 ) {
                        PrvGain &= 0xdf;
                        TgtExp <<= 1;
                }
                if ( PrvGain & 0x40 ) {
                        PrvGain &= 0xbf;
                        TgtExp <<= 1;
                }
                if ( PrvGain & 0x80 ) {
                        PrvGain &= 0x7f;
                        TgtExp <<= 1;
                }

                TgtExp <<= 1;

                if(TgtExp > TgtMaxExp) {
                        dummyline = TgtExp - TgtMaxExp;
                        TgtExp = TgtMaxExp;
                }
                SENSOR_PRINT("OV9660: PrvGain %d, TgtExp %d", PrvGain, TgtExp);
                val45 =(uint8_t)( (TgtExp>>10) | ((Sensor_ReadReg(0x45) & 0xc0)));	//	 val45 = (TgtExp>>10);
                val10 = (uint8_t)(TgtExp>>2);
                val04 =(uint8_t)( Sensor_ReadReg(0x04)&0xfc);
                val04 = (val04|(uint8_t)(TgtExp&0x03));
                val2d = (uint8_t)(dummyline & 0xff);
                val2e = (uint8_t)((dummyline>>8) & 0xff);
                Sensor_WriteReg(0x45,val45);
                Sensor_WriteReg(0x10,val10);
                Sensor_WriteReg(0x04,val04);
                Sensor_WriteReg(0x00,PrvGain);
                Sensor_WriteReg(0x2d,val2d);
                Sensor_WriteReg(0x2e,val2e);
                Sensor_WriteReg(0x46,0);
                Sensor_WriteReg(0x47,0);
                msleep(300);
        }
}

LOCAL uint32_t OV9660_BeforeSnapshot(uint32_t param)
{
        uint32_t  preview_mode = (param >= SENSOR_MODE_PREVIEW_TWO) ? SENSOR_MODE_PREVIEW_TWO:SENSOR_MODE_PREVIEW_ONE;

        if(param > SENSOR_MODE_PREVIEW_ONE) {
                OV9660_CalculateExposureGain(preview_mode, param);
        }
        return 0;
}
/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/
const SENSOR_REG_T OV9660_brightness_tab[][3]=
{
        {{0xD1, 0x30}, {0xCF, 0x00}, {0xff, 0xff}},//level0
        {{0xD1, 0x20}, {0xCF, 0x00}, {0xff, 0xff}},//level1
        {{0xD1, 0x10}, {0xCF, 0x00}, {0xff, 0xff}},//level2
        {{0xD1, 0x00}, {0xCF, 0x00}, {0xff, 0xff}},//level3
        {{0xD1, 0x00}, {0xCF, 0x10}, {0xff, 0xff}},//level4
        {{0xD1, 0x00}, {0xCF, 0x20}, {0xff, 0xff}},//level5
        {{0xD1, 0x00}, {0xCF, 0x30}, {0xff, 0xff}} //level6
};

LOCAL uint32_t OV9660_set_brightness(uint32_t level)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV9660_brightness_tab[level];
        if(level >= 7) {
                SENSOR_PRINT("OV9660_set_brightness:param error,level=%d .\n",level);
                return SENSOR_FAIL;
        }
	
        for(i = 0;(0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value) ; i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        msleep(10); 
        SENSOR_PRINT("set_brightness: level = %d", level);
        return 0;
}
/******************************************************************************/
// Description: set contrast
// Global resource dependence: 
// Author:
// Note:
//		level must smaller than 9
/******************************************************************************/
const SENSOR_REG_T OV9660_contrast_tab[ ]=
{	
        {0xd0, 0x08}, 
        {0xd0, 0x10},
        {0xd0, 0x18}, 
        {0xd0, 0x20},
        {0xd0, 0x30},
        {0xd0, 0x40}, 
        {0xd0, 0x50} 
};

LOCAL uint32_t OV9660_set_contrast(uint32_t level)
{
        SENSOR_REG_T sensor_reg = OV9660_contrast_tab[level];

        if(level >= 7) {
                SENSOR_PRINT("OV9660_set_contrast:param error,level=%d .\n",level);
                return SENSOR_FAIL;
        }

        Sensor_WriteReg(sensor_reg.reg_addr, sensor_reg.reg_value);
        msleep(10);
        SENSOR_PRINT("set_contrast: level = %d \n", level);
        return 0;
}
#if 0
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
const SENSOR_REG_T OV9660_sharpness_tab[][3]=
{
	//weakest
	{{0xff, 0xff}, {0xff, 0xff}, {0xff, 0xff}},
	{{0xff, 0xff}, {0xff, 0xff}, {0xff, 0xff}},
	{{0xff, 0xff}, {0xff, 0xff}, {0xff, 0xff}},
	{{0xff, 0xff}, {0xff, 0xff}, {0xff, 0xff}},
	//strongest
	{{0xff, 0xff}, {0xff, 0xff}, {0xff, 0xff}}
};

LOCAL uint32_t OV9660_set_sharpness(uint32_t level)
{
	uint16_t i;
	SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV9660_sharpness_tab[level];
	
//	SCI_ASSERT(level <= 4 );
//	SCI_ASSERT(PNULL != sensor_reg_ptr);
	if(level > 4)
	{
		SENSOR_PRINT("OV9660_set_sharpness:param error,level=%d .\n",level);
		return SENSOR_FAIL;
	}
	
	for(i = 0; i < 3 ; i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}
	SENSOR_PRINT("set_sharpness: level = %d", level);
	return 0;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
const SENSOR_REG_T OV9660_saturation_tab[][3]=
{
	// level 0
	{ 
		{0xcb, 0x10}, {0xcc, 0x10}, {0xff, 0xff}
	},
	//level 1
	{ 
		{0xcb, 0x1c}, {0xcc, 0x1c}, {0xff, 0xff}
	},
	//level 2
	{ 
		{0xcb, 0x28}, {0xcc, 0x28}, {0xff, 0xff} 
	},
	//level 3
	{ 
		{0xcb, 0x34}, {0xcc, 0x34}, {0xff, 0xff} 
	},
	//level 4
	{ 
		{0xcb, 0x40}, {0xcc, 0x40}, {0xff, 0xff} 
	},
	//level 5
	{ 
		{0xcb, 0x4c}, {0xcc, 0x4c}, {0xff, 0xff} 
	},
	//level 6
	{ 
		{0xcb, 0x58}, {0xcc, 0x58}, {0xff, 0xff}
	},
	//level 7
	{ 
		{0xcb, 0x64}, {0xcc, 0x64}, {0xff, 0xff} 
	},
	//level 8
	{ 
		{0xcb, 0x70}, {0xcc, 0x70}, {0xff, 0xff} 
	}
};
LOCAL uint32_t OV9660_set_saturation(uint32_t level)
{
	uint16_t i;
	SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV9660_saturation_tab[level];
	
//	SCI_ASSERT(level <= 8 );
//	SCI_ASSERT(PNULL != sensor_reg_ptr);
	if(level > 8)
	{
		SENSOR_PRINT("OV9660_set_saturation:param error,level=%d .\n",level);
		SENSOR_FAIL;
	}
	
	for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value) ; i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}
	SENSOR_PRINT("set_saturation: level = %d", level);
	return 0;
}
#endif
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
const SENSOR_REG_T OV9660_image_effect_tab[][5]=
{
        // effect normal
        {
        {0xc7, 0x18}, {0xc8, 0x06},{0xcd, 0x80}, {0xce, 0x80},{0xff, 0xff}  //normal
        },
        //effect BLACKWHITE
        {
        {0xc7, 0x98}, {0xc8, 0x1e},{0xcd, 0x80}, {0xce, 0x80},{0xff, 0xff} //grey
        },
        // effect RED
        {
        {0xc7, 0x98}, {0xc8, 0x1e},{0xcd, 0x80}, {0xce, 0xb0},{0xff, 0xff} //red
        },

        // effect GREEN
        {
        {0xc7, 0x98}, {0xc8, 0x1e},{0xcd, 0x60}, {0xce, 0x30},{0xff, 0xff} //green
        },

        // effect  BLUE
        {
        {0xc7, 0x98}, {0xc8, 0x1e},{0xcd, 0xa0}, {0xce, 0x40},{0xff, 0xff}  //blue
        },
        //effect YELLOW
        {
        {0xc7, 0x98}, {0xc8, 0x1e},{0xcd, 0x40}, {0xce, 0x90},{0xff, 0xff} //
        },
        // effect NEGATIVE
        {
        {0xc7, 0x98}, {0xc8, 0x46},{0xff, 0xff}, {0xff, 0xff},{0xff, 0xff}
        },
        // effect CANVAS ANTIQUE
        {
        {0xc7, 0x98}, {0xc8, 0x1e},{0xcd, 0x40}, {0xce, 0xa0},{0xff, 0xff} //
        },
};
LOCAL uint32_t OV9660_set_image_effect(uint32_t effect_type)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV9660_image_effect_tab[effect_type];

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value) ; i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        if(1 == effect_type) {
                msleep(200);	
        } else {
                msleep(10);	
        }
        SENSOR_PRINT("set_image_effect: effect_type = %d. \n", effect_type);
        return 0;
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		mode 0:normal;   1:night 
/******************************************************************************/
const SENSOR_REG_T OV9660_work_mode_tab[][7]=
{
        {//normal fix 15fps
        //  {0x13, 0xc7},
        {0x13, 0xe7},
        {0x14, 0x28},
        {0x03, 0x03},
        {0x0f, 0x46},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff}

        },
        {//night 7.5fps-15fps
        {0x13, 0xe7},
        {0x14, 0x48},
        {0x03, 0x43},
        {0x0f, 0x4e},
        {0xff, 0xff},
        {0xff, 0xff},
        {0xff, 0xff}
        }
};
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		mode 0:normal;	 1:night 
/******************************************************************************/
LOCAL uint32_t OV9660_set_work_mode(uint32_t mode)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV9660_work_mode_tab[mode];
        if(mode > 1) {
                SENSOR_PRINT("OV9660_set_work_mode:param error,mode=%d .\n",mode);
                return SENSOR_FAIL;
        }
        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_PRINT("set_work_mode: mode = %d .\n", mode);
        return 0;
}

LOCAL uint32_t OV9660_after_snapshot(uint32_t param)
{
        OV9660_chang_image_format(SENSOR_IMAGE_FORMAT_YUV422);    
        return SENSOR_SUCCESS;
}
LOCAL uint32_t OV9660_chang_image_format(uint32_t param)
{
        SENSOR_REG_TAB_INFO_T st_yuv422_reg_table_info = { ADDR_AND_LEN_OF_ARRAY(OV9660_YUV_COMMON),0,0,0,0};
        uint32_t ret_val = SENSOR_FAIL;

        switch(param) {
        case SENSOR_IMAGE_FORMAT_YUV422:
                ret_val = Sensor_SendRegTabToSensor(&st_yuv422_reg_table_info);
                break;
        case SENSOR_IMAGE_FORMAT_JPEG:
                ret_val = SENSOR_FAIL;
                break;
        default:
                SENSOR_PRINT ("OV3640 only support SENSOR_IMAGE_FORMAT_JPEG & SENSOR_IMAGE_FORMAT_YUV422, input is %d.\n", param);
                break;
        }
        return ret_val;
}

LOCAL uint32_t OV9660_check_image_format_support(uint32_t param) 
{
        uint32_t ret_val = SENSOR_FAIL;

        switch(param) {
        case SENSOR_IMAGE_FORMAT_YUV422:
                ret_val = SENSOR_SUCCESS;
                break;
        case SENSOR_IMAGE_FORMAT_JPEG:
                ret_val = SENSOR_FAIL;
                break;
        default:
                SENSOR_PRINT("OV3640 only support SENSOR_IMAGE_FORMAT_JPEG & SENSOR_IMAGE_FORMAT_YUV422, input is %d.\n", param);
                break;
        }
        return ret_val;
}
#if 0
LOCAL uint32_t OV9660_zoom(uint32_t param)
{
        return SENSOR_SUCCESS;
}
#endif
LOCAL uint32_t OV9660_get_skip_frame (uint32_t param)
{
        return 2;
}
struct sensor_drv_cfg sensor_ov9660 = {
        .sensor_pos = CONFIG_DCAM_SENSOR_POS_OV9660,
        .sensor_name = "ov9660",
        .driver_info = &g_OV9660_yuv_info,
};

static int __init sensor_ov9660_init(void)
{
        return dcam_register_sensor_drv(&sensor_ov9660);
}

subsys_initcall(sensor_ov9660_init);

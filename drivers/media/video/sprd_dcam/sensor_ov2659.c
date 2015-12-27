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
#define OV2659_I2C_ADDR_W	0x30//0x60
#define OV2659_I2C_ADDR_R		0x30//0x61 
/**---------------------------------------------------------------------------*
 ** 					Local Function Prototypes							  *
 **---------------------------------------------------------------------------*/
LOCAL uint32_t OV2659_set_ae_enable(uint32_t enable);
LOCAL uint32_t OV2659_set_hmirror_enable(uint32_t enable);
LOCAL uint32_t OV2659_set_vmirror_enable(uint32_t enable);
LOCAL uint32_t OV2659_set_preview_mode(uint32_t preview_mode);
LOCAL uint32_t OV2659_Identify(uint32_t param);
LOCAL uint32_t OV2659_BeforeSnapshot(uint32_t param);	
LOCAL uint32_t OV2659_set_brightness(uint32_t level);
LOCAL uint32_t OV2659_set_contrast(uint32_t level);
LOCAL uint32_t OV2659_set_sharpness(uint32_t level);
LOCAL uint32_t OV2659_set_saturation(uint32_t level);
LOCAL uint32_t OV2659_set_image_effect(uint32_t effect_type);
LOCAL uint32_t OV2659_set_work_mode(uint32_t mode);
LOCAL uint32_t OV2659_chang_image_format(uint32_t param);
LOCAL uint32_t OV2659_check_image_format_support(uint32_t param);
LOCAL uint32_t OV2659_after_snapshot(uint32_t param);

LOCAL uint32_t set_ov2659_ev(uint32_t level);
LOCAL uint32_t set_ov2659_awb(uint32_t mode);
LOCAL uint32_t set_ov2659_anti_flicker(uint32_t mode);
LOCAL uint32_t OV2659_GetExifInfo(uint32_t param);
LOCAL uint32_t OV2659_InitExifInfo(void);
/**---------------------------------------------------------------------------*
 ** 						Local Variables 								 *
 **---------------------------------------------------------------------------*/
//LOCAL EXIF_SPEC_PIC_TAKING_COND_T s_ov2659_exif = {0};

LOCAL SENSOR_REG_T ov2659_YUV_COMMON[]= {
    {0x0103, 0x01},
    {0x3000, 0x0f},
    {0x3001, 0xff},
    {0x3002, 0xff},
    {0x0100, 0x01},
    {0x3633, 0x3d},
    {0x3620, 0x02},
    {0x3631, 0x11},
    {0x3612, 0x04},
    {0x3630, 0x20},
    {0x4702, 0x02},
    {0x370c, 0x34},
    {0x3004, 0x10},
    {0x3005, 0x18},
    {0x3800, 0x00},
    {0x3801, 0x00},
    {0x3802, 0x00},
    {0x3803, 0x00},
    {0x3804, 0x06},
    {0x3805, 0x5f},
    {0x3806, 0x04},
    {0x3807, 0xb7},
    {0x3808, 0x03},
    {0x3809, 0x20},
    {0x380a, 0x02},
    {0x380b, 0x58},
    {0x380c, 0x05},
    {0x380d, 0x14},
    {0x380e, 0x02},
    {0x380f, 0x68},
    {0x3811, 0x08},
    {0x3813, 0x02},
    {0x3814, 0x31},
    {0x3815, 0x31},
    {0x3a02, 0x02},
    {0x3a03, 0x68},
    {0x3a08, 0x00},
    {0x3a09, 0x5c},
    {0x3a0a, 0x00},
    {0x3a0b, 0x4d},
    {0x3a0d, 0x08},
    {0x3a0e, 0x06},
    {0x3a14, 0x02},
    {0x3a15, 0x28},

    {0x3623, 0x00},
    {0x3634, 0x76},
    {0x3701, 0x44},
    {0x3702, 0x18},
    {0x3703, 0x24},
    {0x3704, 0x24},
    {0x3705, 0x0c},
    {0x3820, 0x81},
    {0x3821, 0x01},
    {0x370a, 0x52},
    {0x4608, 0x00},
    {0x4609, 0x80},
    {0x4300, 0x30},
    {0x5086, 0x02},
    {0x5000, 0xfb},
    {0x5001, 0x1f},
    {0x5002, 0x00},
    {0x5025, 0x0e},
    {0x5026, 0x18},
    {0x5027, 0x34},
    {0x5028, 0x4c},
    {0x5029, 0x62},
    {0x502a, 0x74},
    {0x502b, 0x85},
    {0x502c, 0x92},
    {0x502d, 0x9e},
    {0x502e, 0xb2},
    {0x502f, 0xc0},
    {0x5030, 0xcc},
    {0x5031, 0xe0},
    {0x5032, 0xee},
    {0x5033, 0xf6},
    {0x5034, 0x11},
    {0x5070, 0x1c},
    {0x5071, 0x5b},
    {0x5072, 0x05},
    {0x5073, 0x20},
    {0x5074, 0x94},
    {0x5075, 0xb4},
    {0x5076, 0xb4},
    {0x5077, 0xaf},
    {0x5078, 0x05},
    {0x5079, 0x98},
    {0x507a, 0x21},
    {0x5035, 0x6a},
    {0x5036, 0x11},
    {0x5037, 0x92},
    {0x5038, 0x21},

    {0x5039, 0xe1},
    {0x503a, 0x01},
    {0x503c, 0x05},
    {0x503d, 0x08},
    {0x503e, 0x08},
    {0x503f, 0x64},
    {0x5040, 0x58},
    {0x5041, 0x2a},
    {0x5042, 0xc5},
    {0x5043, 0x2e},
    {0x5044, 0x3a},
    {0x5045, 0x3c},
    {0x5046, 0x44},
    {0x5047, 0xf8},
    {0x5048, 0x08},
    {0x5049, 0x70},
    {0x504a, 0xf0},
    {0x504b, 0xf0},
    {0x500c, 0x03},
    {0x500d, 0x20},
    {0x500e, 0x02},
    {0x500f, 0x5c},
    {0x5010, 0x48},
    {0x5011, 0x00},
    {0x5012, 0x66},
    {0x5013, 0x03},
    {0x5014, 0x30},
    {0x5015, 0x02},
    {0x5016, 0x7c},
    {0x5017, 0x40},
    {0x5018, 0x00},
    {0x5019, 0x66},
    {0x501a, 0x03},
    {0x501b, 0x10},
    {0x501c, 0x02},
    {0x501d, 0x7c},
    {0x501e, 0x3a},
    {0x501f, 0x00},
    {0x5020, 0x66},
    {0x506e, 0x44},
    {0x5064, 0x08},
    {0x5065, 0x10},
    {0x5066, 0x12},
    {0x5067, 0x02},
    {0x506c, 0x08},
    {0x506d, 0x10},
    {0x506f, 0xa6},
    {0x5068, 0x08},

    {0x5069, 0x10},
    {0x506a, 0x04},
    {0x506b, 0x12},
    {0x507e, 0x40},
    {0x507f, 0x20},
    {0x507b, 0x02},
    {0x507a, 0x01},
    {0x5084, 0x0c},
    {0x5085, 0x3e},
    {0x5005, 0x80},
    {0x3a0f, 0x30},
    {0x3a10, 0x28},
    {0x3a1b, 0x32},
    {0x3a1e, 0x26},
    {0x3a11, 0x60},
    {0x3a1f, 0x14},
    {0x5060, 0x69},
    {0x5061, 0x7d},
    {0x5062, 0x7d},
    {0x5063, 0x69},
    {0x3004, 0x20}
};

//pclk = 36MHz @24MHz MCLK
//7.5fps
LOCAL SENSOR_REG_T ov2659_YUV_1600X1200[]=
{
//7.5fpss
    {0x3800, 0x00},
    {0x3801, 0x00},
    {0x3802, 0x00},
    {0x3803, 0x00},
    {0x3804, 0x06},
    {0x3805, 0x5f},
    {0x3806, 0x04},
    {0x3807, 0xbb},
    {0x3808, 0x06},
    {0x3809, 0x40},
    {0x380a, 0x04},
    {0x380b, 0xb0},
    {0x380c, 0x07},
    {0x380d, 0x9f},
    {0x380e, 0x04},
    {0x380f, 0xd0},
    {0x3811, 0x10},
    {0x3813, 0x06},
    {0x3814, 0x11},
    {0x3815, 0x11},
    {0x3a02, 0x04},
    {0x3a03, 0xd0},
    {0x3a08, 0x00},
    {0x3a09, 0xb8},
    {0x3a0a, 0x00},
    {0x3a0b, 0x9a},
    {0x3a0d, 0x08},
    {0x3a0e, 0x06},
    {0x3a14, 0x04},
    {0x3a15, 0x50},
    {0x3623, 0x00},
    {0x3634, 0x44},
    {0x3701, 0x44},
    {0x3702, 0x30},
    {0x3703, 0x48},
    {0x3704, 0x48},
    {0x3705, 0x18},
    {0x3820, 0x80},
    {0x3821, 0x00},
    {0x370a, 0x12},
    {0x4608, 0x00},
    {0x4609, 0x80},
    {0x5002, 0x00},
    {0x3005, 0x24},
    {0x3004, 0x20}
};

LOCAL SENSOR_REG_T ov2659_YUV_1280X960[]=
{
     //7.5fpss
    {0x3800, 0x00},
    {0x3801, 0x00},
    {0x3802, 0x00},
    {0x3803, 0x00},
    {0x3804, 0x06},
    {0x3805, 0x5f},
    {0x3806, 0x04},
    {0x3807, 0xbb},
    {0x3808, 0x05},
    {0x3809, 0x00},
    {0x380a, 0x03},
    {0x380b, 0xc0},
    {0x380c, 0x07},
    {0x380d, 0x9f},
    {0x380e, 0x04},
    {0x380f, 0xd0},
    {0x3811, 0x10},
    {0x3813, 0x06},
    {0x3814, 0x11},
    {0x3815, 0x11},
    {0x3a02, 0x04},
    {0x3a03, 0xd0},
    {0x3a08, 0x00},
    {0x3a09, 0xb8},
    {0x3a0a, 0x00},
    {0x3a0b, 0x9a},
    {0x3a0d, 0x08},
    {0x3a0e, 0x06},
    {0x3a14, 0x04},
    {0x3a15, 0x50},
    {0x3623, 0x00},
    {0x3634, 0x44},
    {0x3701, 0x44},
    {0x3702, 0x30},
    {0x3703, 0x48},
    {0x3704, 0x48},
    {0x3705, 0x18},
    {0x3820, 0x80},
    {0x3821, 0x00},
    {0x370a, 0x12},
    {0x4608, 0x00},  
    {0x4609, 0xa0},
    {0x5002, 0x10},
    {0x3005, 0x24},
    {0x3004, 0x20}   
};

LOCAL SENSOR_REG_T ov2659_YUV_640X480[]=
  // 24M MCLK  input ,25fps ,40M PCLK 
{	
    {0x3800, 0x00},
    {0x3801, 0x00},
    {0x3802, 0x00},
    {0x3803, 0x00},
    {0x3804, 0x06},
    {0x3805, 0x5f},
    {0x3806, 0x04},
    {0x3807, 0xb7},
    {0x3808, 0x02},
    {0x3809, 0x80},
    {0x380a, 0x01},
    {0x380b, 0xe0},
    {0x380c, 0x05},
    {0x380d, 0x14},
    {0x380e, 0x02},
    {0x380f, 0x68},
    {0x3811, 0x08},
    {0x3813, 0x02},
    {0x3814, 0x31},
    {0x3815, 0x31},
    {0x3a02, 0x02},
    {0x3a03, 0x68},
    {0x3a08, 0x00},
    {0x3a09, 0x5c},
    {0x3a0a, 0x00},
    {0x3a0b, 0x4d},
    {0x3a0d, 0x08},
    {0x3a0e, 0x06},
    {0x3a14, 0x02},
    {0x3a15, 0x28},
    {0x3623, 0x00},
    {0x3634, 0x76},
    {0x3701, 0x44},
    {0x3702, 0x18},
    {0x3703, 0x24},
    {0x3704, 0x24},
    {0x3705, 0x0c},
    {0x3820, 0x81},
    {0x3821, 0x01},
    {0x370a, 0x52},
    {0x4608, 0x00},
    {0x4609, 0xa0},
    {0x5002, 0x10},
    {0x3005, 0x28},
    {0x3004, 0x20}
};

LOCAL SENSOR_REG_TAB_INFO_T s_OV2659_resolution_Tab_YUV[]=
{	
        // COMMON INIT
        {ADDR_AND_LEN_OF_ARRAY(ov2659_YUV_COMMON), 0, 0, 24, SENSOR_IMAGE_FORMAT_YUV422}, 

        // YUV422 PREVIEW 1 
        {ADDR_AND_LEN_OF_ARRAY(ov2659_YUV_640X480), 640, 480, 24, SENSOR_IMAGE_FORMAT_YUV422},
        {ADDR_AND_LEN_OF_ARRAY(ov2659_YUV_1280X960), 1280, 960, 24, SENSOR_IMAGE_FORMAT_YUV422},
        {ADDR_AND_LEN_OF_ARRAY(ov2659_YUV_1600X1200), 1600, 1200, 24,SENSOR_IMAGE_FORMAT_YUV422},
        {PNULL, 0, 0, 0, 0, 0},

        // YUV422 PREVIEW 2 
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0}
};

LOCAL SENSOR_IOCTL_FUNC_TAB_T s_OV2659_ioctl_func_tab = 
{
        // Internal 
        PNULL,
        PNULL,
        PNULL,
        OV2659_Identify,

        PNULL,			// write register
        PNULL,			// read  register	
        PNULL,
        PNULL,

        // External
        OV2659_set_ae_enable,
        OV2659_set_hmirror_enable,
        OV2659_set_vmirror_enable,

        OV2659_set_brightness,
        OV2659_set_contrast,
        OV2659_set_sharpness,
        OV2659_set_saturation,

        OV2659_set_preview_mode,	
        OV2659_set_image_effect,

        OV2659_BeforeSnapshot,
        OV2659_after_snapshot,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        set_ov2659_awb,
        PNULL,
        PNULL,
        set_ov2659_ev,
        OV2659_check_image_format_support,
        OV2659_chang_image_format,
        PNULL,
        OV2659_GetExifInfo,
        PNULL,
        set_ov2659_anti_flicker,
        PNULL,
        PNULL,
        PNULL
};
/**---------------------------------------------------------------------------*
 ** 						Global Variables								  *
 **---------------------------------------------------------------------------*/
 SENSOR_INFO_T g_OV2659_yuv_info =
{
        OV2659_I2C_ADDR_W,				// salve i2c write address
        OV2659_I2C_ADDR_R, 				// salve i2c read address

        SENSOR_I2C_REG_16BIT,			// bit0: 0: i2c register value is 8 bit, 1: i2c register value is 16 bit
        						// bit1: 0: i2c register addr  is 8 bit, 1: i2c register addr  is 16 bit
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
        SENSOR_IMAGE_EFFECT_NEGATIVE|\
        SENSOR_IMAGE_EFFECT_CANVAS,

        // while balance mode
        0,

        0x77777,						// brightness/contrast/sharpness/saturation/EV

        SENSOR_LOW_PULSE_RESET,			// reset pulse level
        20,								// reset pulse width(ms)

        SENSOR_HIGH_LEVEL_PWDN,			// 1: high level valid; 0: low level valid

        2,								// count of identify code
        {{0x300A, 0x26},						// supply two code to identify sensor.
        {0x300B, 0x56}},						// for Example: index = 0-> Device id, index = 1 -> version id											
        								
        SENSOR_AVDD_2800MV,				// voltage of avdd	

        1600,							// max width of source image
        1200,							// max height of source image
        "OV2659",						// name of sensor												

        SENSOR_IMAGE_FORMAT_YUV422,		// define in SENSOR_IMAGE_FORMAT_E enum,SENSOR_IMAGE_FORMAT_MAX
        						// if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T

        SENSOR_IMAGE_PATTERN_YUV422_YUYV,	// pattern of input image form sensor;			

        s_OV2659_resolution_Tab_YUV,	// point to resolution table information structure
        &s_OV2659_ioctl_func_tab,		// point to ioctl function table

        PNULL,							// information and table about Rawrgb sensor
        PNULL,				// extend information about sensor
        SENSOR_AVDD_2800MV,                     // iovdd
        SENSOR_AVDD_1300MV,                      // dvdd
        1,                     // skip frame num before preview 
        2,                     // skip frame num before capture		
        0,                     // deci frame num during preview;		
        0,                     // deci frame num during video preview;
        0,                     // threshold enable
        0,                     // threshold mode
        0,                     // threshold start postion	
        0,                     // threshold end postion 
        -1                     // i2c_dev_handler
};


/**---------------------------------------------------------------------------*
 ** 							Function  Definitions
 **---------------------------------------------------------------------------*/
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t OV2659_set_ae_enable(uint32_t enable)
{
        uint16_t temp_reg = 0;
        if(enable) {
                temp_reg = Sensor_ReadReg(0x3503);
                temp_reg &= 0xf8;
                Sensor_WriteReg(0x3503,temp_reg);
        } else {
                temp_reg = Sensor_ReadReg(0x3503);
                temp_reg |= 0x07;
                Sensor_WriteReg(0x3503,temp_reg);
        }
        return 0;
}
/******************************************************************************/
// Description: anti 50/60 hz banding flicker
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/
LOCAL const SENSOR_REG_T ov2659_banding_flicker_tab[][3]=
{
        // 50HZ
        {
        {0x3014, 0x84},{0x3013, 0xf7},{0xffff,0xff} 
        },    
        //60HZ
        {
        {0x3014, 0x04},{0x3013, 0xf7},{0xffff,0xff}       
        },
};   

LOCAL uint32_t set_ov2659_anti_flicker(uint32_t mode)
{
        uint16_t temp_reg_0 = 0;
        uint16_t base_shutter = 0;
        uint16_t banding = 0;

        switch (mode) {
        case 0:
                base_shutter=40*1000000/1300/2/100;
                Sensor_WriteReg(0x3a09,base_shutter);//banding filter
                banding=618/base_shutter;
                Sensor_WriteReg(0x3a0e,banding);//banding step
                temp_reg_0 = Sensor_ReadReg(0x3a05);
                temp_reg_0 &= 0x7f;
                Sensor_WriteReg(0x3a05, temp_reg_0);
                break;
        case 1:
                base_shutter=40*1000000/1300/2/120;
                Sensor_WriteReg(0x3a09,base_shutter);//banding filter
                banding=618/base_shutter;
                Sensor_WriteReg(0x3a0e,banding);//banding step
                temp_reg_0 = Sensor_ReadReg(0x3a05);
                temp_reg_0 |= 0x80;
                Sensor_WriteReg(0x3a05, temp_reg_0);
                break;
        default:
                break;
        }
        SENSOR_PRINT("SENSOR: set_ov2659_flicker: 0x%x", mode);
        return 0;
}

/******************************************************************************/
// Description: set wb mode 
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t set_ov2659_awb(uint32_t mode)
{
        uint32_t ret = 0;
        uint16_t  temp_reg;
        temp_reg = Sensor_ReadReg(0x3406);
        switch (mode) {
        case 0://auto
                Sensor_WriteReg(0x3406,0x00);   // select Auto WB
                break;
        case 1: //office
                Sensor_WriteReg(0x3406 ,temp_reg|0x1 );
                Sensor_WriteReg(0x3400 ,0x6 );
                Sensor_WriteReg(0x3401 ,0x2a);
                Sensor_WriteReg(0x3402 ,0x4 );
                Sensor_WriteReg(0x3403 ,0x0 );
                Sensor_WriteReg(0x3404 ,0x7 );
                Sensor_WriteReg(0x3405 ,0x24 );
                break;
        case 2://U30
        case 3://CWF
                ret = 1;
                break;
        case 4://HOME
                Sensor_WriteReg(0x3406 ,temp_reg|0x1 );
                Sensor_WriteReg(0x3400 ,0x4 );
                Sensor_WriteReg(0x3401 ,0x58);
                Sensor_WriteReg(0x3402 ,0x4 );
                Sensor_WriteReg(0x3403 ,0x0 );
                Sensor_WriteReg(0x3404 ,0x8 );
                Sensor_WriteReg(0x3405 ,0x40);
                break;  
        case 5://SUN
                Sensor_WriteReg(0x3406 ,temp_reg|0x1 );
                Sensor_WriteReg(0x3400 ,0x7);
                Sensor_WriteReg(0x3401 ,0x2);
                Sensor_WriteReg(0x3402 ,0x4 );
                Sensor_WriteReg(0x3403 ,0x0 );
                Sensor_WriteReg(0x3404 ,0x5 );
                Sensor_WriteReg(0x3405 ,0x15);
                break;
        case 6: //cloudy
                Sensor_WriteReg(0x3406 ,temp_reg|0x1 );
                Sensor_WriteReg(0x3400 ,0x7 );
                Sensor_WriteReg(0x3401 ,0x8);
                Sensor_WriteReg(0x3402 ,0x4 );
                Sensor_WriteReg(0x3403 ,0x0 );
                Sensor_WriteReg(0x3404 ,0x5 );
                Sensor_WriteReg(0x3405 ,0x0 );
                break;
        default:
                ret = 2;
        }

        //Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_LIGHTSOURCE, (uint32)mode);
        msleep(100); 
        SENSOR_PRINT("SENSOR: set_awb_mode: mode = %d,ret = %d", mode,ret);

        return ret;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:yangbin
// Note:
//		
/******************************************************************************/
LOCAL const SENSOR_REG_T ov2659_ev_tab[][6]=
{
        {{0x3a0f, 0x24},{0x3a10, 0x20},{0x3a11, 0x41},{0x3a1b, 0x24},{0x3a1e,0x20},{0x3a1f,0x10}},
        {{0x3a0f, 0x28},{0x3a10, 0x24},{0x3a11, 0x41},{0x3a1b, 0x28},{0x3a1e,0x24},{0x3a1f,0x10}},
        {{0x3a0f, 0x2c},{0x3a10, 0x28},{0x3a11, 0x51},{0x3a1b, 0x2c},{0x3a1e,0x28},{0x3a1f,0x10}},
        {{0x3a0f, 0x30},{0x3a10, 0x2c},{0x3a11, 0x61},{0x3a1b, 0x31},{0x3a1e,0x2c},{0x3a1f,0x10}},
        {{0x3a0f, 0x38},{0x3a10, 0x30},{0x3a11, 0x61},{0x3a1b, 0x39},{0x3a1e,0x2f},{0x3a1f,0x10}},
        {{0x3a0f, 0x40},{0x3a10, 0x38},{0x3a11, 0x62},{0x3a1b, 0x41},{0x3a1e,0x37},{0x3a1f,0x10}},
        {{0x3a0f, 0x48},{0x3a10, 0x40},{0x3a11, 0x72},{0x3a1b, 0x49},{0x3a1e,0x3f},{0x3a1f,0x10}},
};

LOCAL uint32_t set_ov2659_ev(uint32_t level)
{
        uint16_t i;    
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)ov2659_ev_tab[level];
 
        if(level >= 7) {
                SENSOR_PRINT("OV2659 set_ov2659_ev,param error,level=%d .\n",level);
        }

        for(i = 0; i < 4 /*(0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value)*/; i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        msleep(100); 
        SENSOR_PRINT("SENSOR: set_ev: level = %d", level);

        return 0;
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t OV2659_set_hmirror_enable(uint32_t enable)
{
        uint16_t flip = 0;
        uint16_t mirror = 0; 

        flip   = Sensor_ReadReg(0x3820);
        mirror = Sensor_ReadReg(0x3821);

        if(enable) {
                Sensor_WriteReg(0x3820, flip&0xf9);     
                Sensor_WriteReg(0x3821, mirror|0x06);
        } else {
                Sensor_WriteReg(0x3820, flip&0xf9);     
                Sensor_WriteReg(0x3821, mirror&0xf9);
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
LOCAL uint32_t OV2659_set_vmirror_enable(uint32_t enable)
{
        uint16_t flip = 0;
        uint16_t mirror = 0; 

        flip   = Sensor_ReadReg(0x3820);
        mirror = Sensor_ReadReg(0x3821);

        if(enable) {
                Sensor_WriteReg(0x3820, flip|0x06);     
                Sensor_WriteReg(0x3821, mirror&0xf9);
        } else {
                Sensor_WriteReg(0x3820, flip&0xf9);     
                Sensor_WriteReg(0x3821, mirror&0xf9);
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
LOCAL uint32_t OV2659_set_preview_mode(uint32_t preview_mode)
{
        SENSOR_PRINT("set_preview_mode: preview_mode = %d", preview_mode);

        switch (preview_mode) {
        case SENSOR_PARAM_ENVIRONMENT_NORMAL: 
                OV2659_set_work_mode(0);
                break;
        case SENSOR_PARAM_ENVIRONMENT_NIGHT:
                OV2659_set_work_mode(1);
                break;
        case SENSOR_PARAM_ENVIRONMENT_SUNNY:
                OV2659_set_work_mode(0);
                break;
        default:
                break;
         }
        msleep(250);	
        return 0;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t OV2659_Identify(uint32_t param)
{
        uint16_t  iden_reg_val = 0;
        uint32_t  ret = SENSOR_OP_ERR;

        Sensor_WriteReg(0x0103,0x01);
        msleep(2);
        iden_reg_val = Sensor_ReadReg(0x300A);
        if(iden_reg_val == 0x26) {
        	iden_reg_val = Sensor_ReadReg(0x300B);
        	if(iden_reg_val == 0x56)
        	ret = SENSOR_OP_SUCCESS;
        }

        OV2659_InitExifInfo();
        SENSOR_PRINT("OV2659_Identify: ret = %d", ret);
        return ret;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
#define CAPTURE_MAX_GAIN16 					128
#define OV2659_CAPTURE_MAXIMUM_SHUTTER		1200


#define PV_PERIOD_PIXEL_NUMS 1190
#define  FULL_PERIOD_PIXEL_NUMS 1922
#define  g_Capture_Dummy_Pixels 0
#define  FULL_EXPOSURE_LIMITATION 1234
#define g_Capture_Dummy_Lines 0
#define g_Capture_PCLK_Frequency 36
#if 0
LOCAL uint16_t read_OV2659_gain(void)
{
	uint8_t  temp_reg;
	uint16_t sensor_gain;

	temp_reg=Sensor_ReadReg(0x3000);  
	sensor_gain=(16+(temp_reg&0x0F));
	if(temp_reg&0x10)
		sensor_gain<<=1;
	if(temp_reg&0x20)
		sensor_gain<<=1;

	if(temp_reg&0x40)
		sensor_gain<<=1;

	if(temp_reg&0x80)
		sensor_gain<<=1;
	return sensor_gain;
}  /* read_OV2659_gain */
#endif
LOCAL uint32_t m_gain_preview;
LOCAL uint32_t m_gain_still;
LOCAL uint32_t m_shutter_preview;
//LOCAL uint32 m_shutter_still;
LOCAL uint32_t m_dbExposure;
LOCAL uint32_t m_dwFreq_preview = 10;
LOCAL uint32_t m_dwFreq_still = 36;

LOCAL void OV2659Core_Get_ExposureValue(void)
{
        unsigned char regv;
        unsigned long linetp,shutter;
        unsigned int gain16;

        SENSOR_PRINT("OV2659Core_Get_ExposureValue()\r\n");

        //Default Timing
        regv = Sensor_ReadReg(0x380c);
        linetp = regv&0x0F;
        linetp <<=8;

        regv = Sensor_ReadReg(0x380d);
        linetp += regv;
        linetp ++;
        SENSOR_PRINT("linetp = %ld\r\n",linetp);
        //Shutter
        regv = Sensor_ReadReg(0x3500);
        shutter = regv&0x0F;
        shutter <<=8;

        regv = Sensor_ReadReg(0x3501);
        shutter += regv;
        shutter <<=8;

        regv = Sensor_ReadReg(0x3502);
        shutter += regv;
        SENSOR_PRINT("shutter = %ld\r\n",shutter);
        //Gain

        regv = Sensor_ReadReg(0x350A);
        gain16 = regv&0x01;
        gain16 <<=8;

        regv = Sensor_ReadReg(0x350B);
        gain16+=regv;
        SENSOR_PRINT("gain16 = %d\r\n",gain16);
        m_gain_preview = gain16;
        m_shutter_preview=shutter;
        shutter >>=4;
        m_dbExposure = shutter;
        m_dbExposure *=linetp;
        m_dbExposure *=gain16;
        m_dbExposure *=2;

        SENSOR_PRINT("m_dbExposure = %d\r\n",m_dbExposure);
}

LOCAL void OV2659Core_Set_ExposureValue(void)
{
        uint32_t regv;
        uint32_t linetp,framelines,framelines_banding;
        uint32_t shutter,maxshutter;
        uint32_t bandinglines;
        uint32_t exposuretp;
        uint8_t lightfreq;
        uint32_t maxgain16,gain16;

        SENSOR_PRINT("OV2659Core_Set_ExposureValue()\r\n");

        if(m_dbExposure == 0) 
        	return; //exposure value is invalid

        regv = Sensor_ReadReg(0x380c);
        linetp = regv;
        linetp <<=8;

        regv = Sensor_ReadReg(0x380d);
        linetp += regv;
        linetp ++;
        SENSOR_PRINT("linetp = %d\r\n",linetp);

        regv = Sensor_ReadReg(0x380e);
        framelines = regv;

        regv = Sensor_ReadReg(0x350c);
        framelines += regv;
        framelines <<=8;

        regv = Sensor_ReadReg(0x380f);
        framelines += regv;

        regv = Sensor_ReadReg(0x350d);
        framelines += regv;
        SENSOR_PRINT("framelines = %d\r\n",framelines);
        maxshutter = (uint32_t)(framelines-4);
        //获取当前时序下总曝光量
        SENSOR_PRINT("m_dbExposure = %d\r\n",m_dbExposure);
        exposuretp = m_dbExposure;
        exposuretp *= m_dwFreq_still;
        exposuretp /= m_dwFreq_preview;
        SENSOR_PRINT("exposuretp = %d, before adjust\r\n",exposuretp);
        SENSOR_PRINT("exposuretp = %d, after adjust, m_gain_preview=%x\r\n",exposuretp, m_gain_preview);
        //获取Banding相关参数

        regv = Sensor_ReadReg(0x3a08);
        bandinglines=regv;
        bandinglines<<=8;

        regv = Sensor_ReadReg(0x3a09);
        bandinglines+=regv;
        shutter = m_shutter_preview/bandinglines;
        if(m_shutter_preview == (shutter*bandinglines)) {
        	lightfreq = 50;
        } else {
        	lightfreq = 60;
        }
        //
        bandinglines = (uint32_t)((m_dwFreq_still*1000000*2+lightfreq*linetp)/(2*2*lightfreq*linetp));
        framelines_banding = (maxshutter/bandinglines)*bandinglines;
        SENSOR_PRINT("bandinglines = %d\r\n",bandinglines);
        SENSOR_PRINT("lightfreq = %d\r\n",lightfreq);
        maxgain16= 31*4;
        if(exposuretp<(linetp*16)) {
        	SENSOR_PRINT("smaller than 1 line\r\n");
        	exposuretp *=4;
        	shutter = 1;
        	gain16 = (uint32_t)((exposuretp*2+linetp)/linetp/2/4);
        } else if(exposuretp<(bandinglines*linetp*16)) {
        	SENSOR_PRINT("smaller than 1 banding, bandinglines=%d\r\n", bandinglines);
        	shutter = exposuretp/(linetp*16);
        	gain16 = 16;
        } else if(exposuretp<(framelines*linetp*16)) {
        	SENSOR_PRINT("smaller than 1 frame\r\n");
        	shutter = (uint32_t)(exposuretp / (bandinglines*linetp*16));
        	shutter *= bandinglines;
        	gain16 = (uint32_t)((exposuretp*2+shutter*linetp)/(shutter*linetp)/2);
        } else {
        	SENSOR_PRINT("larger than 1 frame\r\n");
        	shutter = (uint32_t)((framelines/bandinglines)*bandinglines);
        	gain16 = (uint32_t)((exposuretp*2+shutter*linetp)/(shutter*linetp)/2);
        	if(gain16> maxgain16) {
        		SENSOR_PRINT("larger than maxim gain & frame\r\n");
        		shutter = (uint32_t)(exposuretp*11/(10*maxgain16*linetp));
        		shutter /=bandinglines;
        		shutter *=bandinglines;
        		gain16 = (uint32_t)(exposuretp/(shutter*linetp));
        	}
        }
        SENSOR_PRINT("shutter = %d\r\n",shutter);
        SENSOR_PRINT("gain16 = %d\r\n",gain16);
        //   m_shutter_still = shutter;
        m_gain_still = gain16;
        SENSOR_PRINT("m_gain_still = %x\r\n",m_gain_still);
        //设置快门值
        if(maxshutter<=shutter) {
        	maxshutter = shutter+4;
        	regv =maxshutter&0xFF;

        	Sensor_WriteReg(0x350d,0x00);		
        	Sensor_WriteReg(0x380f,regv);
        	regv = maxshutter>>8;	
        	Sensor_WriteReg(0x350c,0x00);
        	Sensor_WriteReg(0x380e,regv);
        }
        regv =(shutter&0x0F)<<4;
        Sensor_WriteReg(0x3502,regv);
        regv =((shutter>>4)&0xFF);
        Sensor_WriteReg(0x3501,regv);
        regv =((shutter>>12)&0x0F);
        Sensor_WriteReg(0x3500,regv);
        //设置增益
        if(m_gain_still>0x20) {
        	gain16 = m_gain_still;
        	gain16>>=1;
        } else if(m_gain_still<0x10) {
        	gain16 = 0x18;
        	gain16<<=1;
        } else {
        	gain16 = 0x08;
        }
        regv = gain16&0xFF;
        Sensor_WriteReg(0x350B,regv);
        regv = (gain16>>8);
        Sensor_WriteReg(0x350A,regv);
}

LOCAL void OV2659_CalculateExposureGain(SENSOR_MODE_E sensor_preview_mode, SENSOR_MODE_E sensor_snapshot_mode)
{        
        if(sensor_snapshot_mode < SENSOR_MODE_SNAPSHOT_ONE_FIRST)  { //less than 640X480
                return ;
        }

        OV2659Core_Set_ExposureValue();       
}

LOCAL uint32_t OV2659_BeforeSnapshot(uint32_t param)
{
        uint32_t  preview_mode = (param >= SENSOR_MODE_PREVIEW_TWO) ? \
                        SENSOR_MODE_PREVIEW_TWO:SENSOR_MODE_PREVIEW_ONE;
        OV2659_set_ae_enable(0);

        OV2659Core_Get_ExposureValue();
        Sensor_SetMode(param);
        OV2659_CalculateExposureGain(preview_mode, param);
        return 0;
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t OV2659_set_sharpness(uint32_t level)
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
LOCAL uint32_t OV2659_set_saturation(uint32_t level)
{
        uint16_t temp_reg = 0;
        uint32_t ret = 0;    
        uint16_t temp_reg_0 = 0;
        uint16_t temp_reg_1 = 0;

        temp_reg_0 = Sensor_ReadReg(0x5001);
        temp_reg_0 |= 0x08;
        Sensor_WriteReg(0x5001,temp_reg_0);

        temp_reg = Sensor_ReadReg(0x507a);
        temp_reg |= 0x10;
        Sensor_WriteReg(0x507B,temp_reg);    

        temp_reg_1 = Sensor_ReadReg(0x507b);
        temp_reg_1 |= 0x02;
        Sensor_WriteReg(0x5083,temp_reg_1);    

        switch (level) {
        case 0://normal    	    
                Sensor_WriteReg(0x507e, 0x20);
                Sensor_WriteReg(0x507f, 0x20);
                break;
        case 1://  BLACKWHITE
                Sensor_WriteReg(0x507e, 0x30);
                Sensor_WriteReg(0x507f, 0x30);
                break;
        case 2://RED
                Sensor_WriteReg(0x507e, 0x40);
                Sensor_WriteReg(0x507f, 0x40);
                break;
        case 3://GREEN
                Sensor_WriteReg(0x507e, 0x50);
                Sensor_WriteReg(0x507f, 0x50);
                break;
        case 4://BLUE
                Sensor_WriteReg(0x507e, 0x60);
                Sensor_WriteReg(0x507f, 0x60);
                break;
        default:
                ret = 2;
                break;
        }
        SENSOR_PRINT("set_saturation: level = %d,ret = %d", level,ret);
        return ret;
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t OV2659_set_image_effect(uint32_t effect_type)
{
        uint16_t temp_reg = 0;
        uint32_t ret = 0;    
        uint16_t temp_reg_0 = 0;

        temp_reg=Sensor_ReadReg(0x507B);
        temp_reg&=0x87;
        temp_reg_0 = Sensor_ReadReg(0x5001);

        switch (effect_type) {
        case 0://normal
                temp_reg_0 &= 0xf7;
                Sensor_WriteReg(0x5001,temp_reg_0);
                Sensor_WriteReg(0x507B, temp_reg);
                break;
        case 1://  BLACKWHITE
                temp_reg_0 |= 0x08;
                Sensor_WriteReg(0x5001,temp_reg_0);        
                Sensor_WriteReg(0x507B, (temp_reg|0x20));
                break;
        case 2://RED
                temp_reg_0 |= 0x08;
                Sensor_WriteReg(0x5001,temp_reg_0);  
                Sensor_WriteReg(0x507B, (temp_reg|0x18));
                Sensor_WriteReg(0x507E, (0x80));
                Sensor_WriteReg(0x507F, (0xc0));	
                break;
        case 3://GREEN
                temp_reg_0 |= 0x08;
                Sensor_WriteReg(0x5001,temp_reg_0);  
                Sensor_WriteReg(0x507B, (temp_reg|0x18));
                Sensor_WriteReg(0x507E, (0x60));
                Sensor_WriteReg(0x507F, (0x60));
                break;
        case 4://BLUE
                temp_reg_0 |= 0x08;
                Sensor_WriteReg(0x5001,temp_reg_0);  
                Sensor_WriteReg(0x507B, (temp_reg|0x18));
                Sensor_WriteReg(0x507E, (0xA0));
                Sensor_WriteReg(0x507F, (0x40));
                break;
        case 5://YELLOW
                temp_reg_0 |= 0x08;
                Sensor_WriteReg(0x5001,temp_reg_0);  
                Sensor_WriteReg(0x507B, (temp_reg|0x18));
                Sensor_WriteReg(0x507E, (0x40));
                Sensor_WriteReg(0x507F, (0xA0));
                break;
        case 6://NEGATIVE      
                temp_reg_0 |= 0x08;
                Sensor_WriteReg(0x5001,temp_reg_0);  
                temp_reg |= 0x40;
                Sensor_WriteReg(0x507B, temp_reg);
                break;
        case 7://CANVAS ANTIQUE
                temp_reg_0 |= 0x08;
                Sensor_WriteReg(0x5001,temp_reg_0);  
                Sensor_WriteReg(0x507B, (temp_reg|0x18));
                Sensor_WriteReg(0x507E, (0x40));
                Sensor_WriteReg(0x507F, (0x90));
                break;      	
        default:
                ret = 2;
        }
        SENSOR_PRINT("set_image_effect: effect_type = %d,ret = %d", effect_type,ret);
        return ret;
}

/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/
LOCAL uint32_t OV2659_set_brightness(uint32_t level)
{  
        uint16_t temp_reg = 0;
        uint32_t ret = 0;    
        uint16_t temp_reg_0 = 0;
        uint16_t temp_reg_1 = 0;

        temp_reg_0 = Sensor_ReadReg(0x5001);
        temp_reg_0 |= 0x08;
        Sensor_WriteReg(0x5001,temp_reg_0);

        temp_reg = Sensor_ReadReg(0x507B);
        temp_reg |= 0x04;
        Sensor_WriteReg(0x507B,temp_reg);    

        temp_reg_1 = Sensor_ReadReg(0x5083); 

        switch (level) {
        case 0://normal    	    
                Sensor_WriteReg(0x5082, 0x30);
                temp_reg_1 |= 0x08;
                Sensor_WriteReg(0x5083,temp_reg_1); 
                break;
        case 1://  BLACKWHITE
                Sensor_WriteReg(0x5082, 0x20);
                temp_reg_1 |= 0x08;
                Sensor_WriteReg(0x5083,temp_reg_1); 
                break;
        case 2://RED
                Sensor_WriteReg(0x5082, 0x10);
                temp_reg_1 |= 0x08;
                Sensor_WriteReg(0x5083,temp_reg_1); 
                break;
        case 3://GREEN
                Sensor_WriteReg(0x5082, 0x00);
                temp_reg_1 &= 0xF7;
                Sensor_WriteReg(0x5083,temp_reg_1); 
                break;
        case 4://BLUE
                Sensor_WriteReg(0x5082, 0x10);
                temp_reg_1 &= 0xF7;
                Sensor_WriteReg(0x5083,temp_reg_1); 
                break;
        case 5://YELLOW
                Sensor_WriteReg(0x5082, 0x20);
                temp_reg_1 &= 0xF7;
                Sensor_WriteReg(0x5083,temp_reg_1); 
                break;
        case 6://NEGATIVE
                Sensor_WriteReg(0x5082, 0x30);
                temp_reg_1 &= 0xF7;
                Sensor_WriteReg(0x5083,temp_reg_1); 
                break;      
        default:
                ret = 2;
        }
        return 0;
}

/******************************************************************************/
// Description: set contrast
// Global resource dependence: 
// Author:
// Note:
//		level must smaller than 9
/******************************************************************************/
LOCAL uint32_t OV2659_set_contrast(uint32_t level)
{
        uint16_t temp_reg = 0;
        uint32_t ret = 0;    
        uint16_t temp_reg_0 = 0;
        uint16_t temp_reg_1 = 0;

        temp_reg_0 = Sensor_ReadReg(0x5001);
        temp_reg_0 |= 0x08;
        Sensor_WriteReg(0x5001,temp_reg_0);

        temp_reg = Sensor_ReadReg(0x507B);
        temp_reg |= 0x04;
        Sensor_WriteReg(0x507B,temp_reg);    

        temp_reg_1 = Sensor_ReadReg(0x5083);
        temp_reg_1 &= 0xfb;
        Sensor_WriteReg(0x5083,temp_reg_1);    

        switch (level) {
        case 0://normal    	    
        	Sensor_WriteReg(0x5080, 0x14);           
        	Sensor_WriteReg(0x5081,0x14); 
        	break;
        case 1://  BLACKWHITE
        	Sensor_WriteReg(0x5080, 0x18);           
        	Sensor_WriteReg(0x5081,0x18); 
        	break;
        case 2://RED
        	Sensor_WriteReg(0x5080, 0x1c);           
        	Sensor_WriteReg(0x5081,0x1c); 
        	break;
        case 3://GREEN
        	Sensor_WriteReg(0x5080, 0x20);           
        	Sensor_WriteReg(0x5081,0x20); 
        	break;
        case 4://BLUE
        	Sensor_WriteReg(0x5080, 0x24);           
        	Sensor_WriteReg(0x5081,0x24); 
        	break;
        case 5://YELLOW
        	Sensor_WriteReg(0x5080, 0x28);           
        	Sensor_WriteReg(0x5081,0x28); 
        	break;
        case 6://NEGATIVE
        	Sensor_WriteReg(0x5080, 0x2c);           
        	Sensor_WriteReg(0x5081,0x2c); 
        	break;
        default:
        	ret = 2;
        	break;
        }
        SENSOR_PRINT("set_contrast: level = %d,ret = %d", level,ret);

        return ret;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		mode 0:normal;   1:night 
/******************************************************************************/
LOCAL const SENSOR_REG_T ov2659_work_mode_tab[][10]=
{
        {//normal fix 25fps
                {0x3003, 0x80},
                {0x3004, 0x20},
                {0x3005, 0x28},
                {0x3006, 0x0d},
                {0x3a00, 0x78},
                {0x3a02 ,0x07},
                {0x3a03 ,0x38},
                {0x3a14 ,0x07},
                {0x3a15 ,0x38},
                {0xffff, 0xff}            
        },
        {//night 7.5fps-15fps
                {0x3003, 0x80},
                {0x3004, 0x20},
                {0x3005, 0x18},
                {0x3006, 0x0d},
                {0x3a00, 0x7c},
                {0x3a02 ,0x07},
                {0x3a03 ,0x38},
                {0x3a14 ,0x07},
                {0x3a15 ,0x38},
                {0xffff, 0xff}
        }
};
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		mode 0:normal;	 1:night 
/******************************************************************************/
LOCAL uint32_t OV2659_set_work_mode(uint32_t mode)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)ov2659_work_mode_tab[mode];

        if(mode > 1)
                return SENSOR_OP_PARAM_ERR;

        for(i = 0; (0xFFFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        //Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_SCENECAPTURETYPE, (uint32)mode);
        SENSOR_PRINT("set_work_mode: mode = %d", mode);
        return 0;
}

LOCAL uint32_t OV2659_after_snapshot(uint32_t param)
{   
        Sensor_SetMode(param); 
        OV2659_set_ae_enable(1);
        return SENSOR_SUCCESS;
}

LOCAL uint32_t OV2659_chang_image_format(uint32_t param)
{
        SENSOR_REG_TAB_INFO_T st_yuv422_reg_table_info = { ADDR_AND_LEN_OF_ARRAY(ov2659_YUV_640X480),0,0,0,0};//ov2655_YUV_COMMON
        uint32_t ret_val = SENSOR_FAIL;

        switch(param) {
        case SENSOR_IMAGE_FORMAT_YUV422:
                ret_val = Sensor_SendRegTabToSensor(&st_yuv422_reg_table_info);
                break;
        case SENSOR_IMAGE_FORMAT_JPEG:
                ret_val = SENSOR_FAIL;//Sensor_SendRegTabToSensor(&st_jpeg_reg_table_info);
                break;
        default:
                SENSOR_PRINT ("OV2659 only support SENSOR_IMAGE_FORMAT_JPEG & SENSOR_IMAGE_FORMAT_YUV422, input is %d", param);	/*assert to do*/
                break;
        }
        return ret_val;
}

LOCAL uint32_t OV2659_check_image_format_support(uint32_t param) 
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
                SENSOR_PRINT ("OV2659 only support SENSOR_IMAGE_FORMAT_YUV422, input is %d", param);	/*assert to do*/
                break;
        }
        return ret_val;
}

LOCAL uint32_t OV2659_InitExifInfo(void)
{
#if 0
    EXIF_SPEC_PIC_TAKING_COND_T* exif_ptr=&s_ov2659_exif;

    SCI_TRACE_LOW("SENSOR: OV2659_InitExifInfo");

    exif_ptr->valid.FNumber = 1;
    exif_ptr->FNumber.numerator = 14;
    exif_ptr->FNumber.denominator = 5;
    
    exif_ptr->valid.ExposureProgram = 1;
    exif_ptr->ExposureProgram = 0x04;

    //exif_ptr->SpectralSensitivity[MAX_ASCII_STR_SIZE];	
    //exif_ptr->ISOSpeedRatings;
    //exif_ptr->OECF;
    
    //exif_ptr->ShutterSpeedValue;
    
    exif_ptr->valid.ApertureValue=1;
    exif_ptr->ApertureValue.numerator=14;
    exif_ptr->ApertureValue.denominator=5;
    
    //exif_ptr->BrightnessValue;
    //exif_ptr->ExposureBiasValue;

    exif_ptr->valid.MaxApertureValue=1;
    exif_ptr->MaxApertureValue.numerator=14;
    exif_ptr->MaxApertureValue.denominator=5;
    
    //exif_ptr->SubjectDistance;
    //exif_ptr->MeteringMode;
    //exif_ptr->LightSource;
    //exif_ptr->Flash;

    exif_ptr->valid.FocalLength=1;
    exif_ptr->FocalLength.numerator=289;
    exif_ptr->FocalLength.denominator=100;
    
    //exif_ptr->SubjectArea;
    //exif_ptr->FlashEnergy;
    //exif_ptr->SpatialFrequencyResponse;
    //exif_ptr->FocalPlaneXResolution;
    //exif_ptr->FocalPlaneYResolution;
    //exif_ptr->FocalPlaneResolutionUnit;
    //exif_ptr->SubjectLocation[2];
    //exif_ptr->ExposureIndex;
    //exif_ptr->SensingMethod;

    exif_ptr->valid.FileSource=1;
    exif_ptr->FileSource=0x03;

    //exif_ptr->SceneType;
    //exif_ptr->CFAPattern;
    //exif_ptr->CustomRendered;

    exif_ptr->valid.ExposureMode=1;
    exif_ptr->ExposureMode=0x00;

    exif_ptr->valid.WhiteBalance=1;
    exif_ptr->WhiteBalance=0x00;
    
    //exif_ptr->DigitalZoomRatio;
    //exif_ptr->FocalLengthIn35mmFilm;
    //exif_ptr->SceneCaptureType;	
    //exif_ptr->GainControl;
    //exif_ptr->Contrast;
    //exif_ptr->Saturation;
    //exif_ptr->Sharpness;
    //exif_ptr->DeviceSettingDescription;
    //exif_ptr->SubjectDistanceRange;
#endif	
        return SENSOR_SUCCESS;
}

LOCAL uint32_t OV2659_GetExifInfo(uint32_t param)
{
        //return (uint32_t)&s_ov2659_exif;
        return 0;
}
struct sensor_drv_cfg sensor_ov2659 = {
        .sensor_pos = CONFIG_DCAM_SENSOR_POS_OV2659,
        .sensor_name = "ov2659",
        .driver_info = &g_OV2659_yuv_info,
};

static int __init sensor_ov2659_init(void)
{
        return dcam_register_sensor_drv(&sensor_ov2659);
}

subsys_initcall(sensor_ov2659_init);

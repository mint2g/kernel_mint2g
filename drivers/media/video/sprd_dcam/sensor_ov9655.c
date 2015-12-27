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


#define ov9655_I2C_ADDR_W    	0x30//0x60
#define ov9655_I2C_ADDR_R    	0x30//0x61
#define SENSOR_GAIN_SCALE		16
LOCAL uint32_t set_ae_enable(uint32_t enable);
LOCAL uint32_t set_hmirror_enable(uint32_t enable);
LOCAL uint32_t set_vmirror_enable(uint32_t enable);
LOCAL uint32_t set_preview_mode(uint32_t preview_mode);
LOCAL uint32_t ov9655_Identify(uint32_t param);
LOCAL uint32_t ov9655_BeforeSnapshot(uint32_t param);
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
LOCAL uint32_t s_preview_mode=SENSOR_PARAM_ENVIRONMENT_NORMAL;

const SENSOR_REG_T ov9655_YUV_COMMON[]=
{
        {0x12, 0x80},
};
const SENSOR_REG_T ov9655_YUV_1280X1024[]=
{
    //{0x12, 0x80},
    {0x13, 0x00},
    {0x00, 0x00},
    {0x01, 0x80},
    {0x02, 0x80},
    {0x03, 0x1b},
    {0x0e, 0x61},
    {0x0f, 0x42},
    {0x10, 0xf0},
    {0x11, 0x81},//0x00, Internal clock pre-scalar,F(internal clock) = F(input clock) / (Bit[5:0]+1)
    {0x12, 0x02},
    {0x14, 0x1a},//0x2a
    {0x16, 0x14},//0x24, Bit[5:4]-00,01,10: Add dummy frame when gain is greater than 2x,4x,8x
    {0x17, 0x1d},
    {0x18, 0xbd},
    {0x19, 0x01},
    {0x1a, 0x81},
    {0x1e, 0x14},
    {0x24, 0xa0},//0x3c
    {0x25, 0x90},//0x36
    {0x26, 0xc7},//0x72
    {0x27, 0x08},
    {0x28, 0x08},
    {0x2a, 0x00},
    {0x2b, 0x00},
    {0x2c, 0x08},
    {0x32, 0xff},
    {0x33, 0x00},
    {0x34, 0x3d},
    {0x35, 0x00},
    {0x36, 0xF0},
    {0x39, 0x57},
    //{0x3a, 0x80},
    {0x3b, 0x0c},
    {0x3d, 0x99},
    {0x3e, 0x0c},
    {0x3f, 0x42},
    {0x41, 0x40},
    {0x42, 0xc0},	
    {0x6b, 0x5a},//0x5a
    {0x71, 0xa0},//0x78, Bit[5:4]-00:normal,01,10,11:output 1 line for every 2,4,8 lines
    {0x72, 0x00},
    {0x73, 0x01},
    {0x74, 0x3a},
    {0x75, 0x35},
    {0x76, 0x01},
    {0x77, 0x02},
    {0x7a, 0x20},
    {0x7b, 0x1C},
    {0x7c, 0x28},
    {0x7d, 0x3C},
    {0x7e, 0x5A},
    {0x7f, 0x68},
    {0x80, 0x76},
    {0x81, 0x80},
    {0x82, 0x88},
    {0x83, 0x8f},
    {0x84, 0x96},
    {0x85, 0xa3},
    {0x86, 0xaf},
    {0x87, 0xc4},
    {0x88, 0xd7},
    {0x89, 0xe8},
    {0x8a, 0x23},
    {0x8c, 0x0d},
    {0x90, 0x20},
    {0x91, 0x20},
    {0x9f, 0x20},
    {0xa0, 0x20},
    {0xa4, 0x50},
    {0xa5, 0x90},//0x68
    {0xa8, 0xc1},
    {0xa9, 0xfa},
    {0xaa, 0x92},
    {0xab, 0x04},
    {0xac, 0x80},
    {0xad, 0x80},
    {0xae, 0x80},
    {0xaf, 0x80},
    {0xb2, 0xf2},
    {0xb3, 0x20},
    {0xb4, 0x20},
    {0xb5, 0x52},
    {0xb6, 0xaf},
    {0xbb, 0xae},
    {0xb5, 0x00},
    {0xc1, 0xc8},
    {0xc2, 0x01},
    {0xc3, 0x4e},
    {0xC6, 0x85},
    {0xc7, 0x80},
    {0xc9, 0xe0},
    {0xca, 0xe8},
    {0xcb, 0xf0},
    {0xcc, 0xd8},
    {0xcd, 0x93},
    {0x4f, 0x98}, 
    {0x50, 0x98}, 
    {0x51, 0x00}, 
    {0x52, 0x28}, 
    {0x53, 0x70}, 
    {0x54, 0x98}, 
    {0x3b, 0x0c}, //0xcc
    {0x43, 0x14},
    {0x44, 0xf0},
    {0x45, 0x46},
    {0x46, 0x62},
    {0x47, 0x2a},
    {0x48, 0x3c},
    {0x59, 0x85},
    {0x5a, 0xa9},
    {0x5b, 0x64},
    {0x5c, 0x84},
    {0x5d, 0x53},
    {0x5e, 0xe },
    {0x6c, 0x0c},
    {0x6d, 0x55},
    {0x6e, 0x0 },
    {0x6f, 0x9e},
    {0x62, 0x0 },
    {0x63, 0x0 },
    {0x64, 0x2 },
    {0x65, 0x20},
    {0x66, 0x0 },
    {0x9d, 0x2 },
    {0x9e, 0x2 },
    {0x29, 0x15},
    {0xa9, 0xef},
    {0x13, 0xe7},     
    {0x6a, 0x83},  //added later   
    {0xc6, 0x05},// added later
    {0xff, 0xff},              
};

const SENSOR_REG_T ov9655_YUV_640X480[]=
{
	
	//{0x12, 0x80}, 
	{0xb5, 0x00}, 
	{0x35, 0x00}, 
	{0xa8, 0xc1}, 
	{0x3a, 0x80}, 
	{0x3d, 0x99}, 
	{0x77, 0x06}, //0x02
	{0x26, 0x72}, 
	{0x27, 0x08}, 
	{0x28, 0x08}, 
	{0x29, 0x15}, 
	{0x2c, 0x08}, 
	{0xab, 0x04}, 
	{0x6e, 0x00}, 
	{0x6d, 0x55}, 
	{0x00, 0x32}, 
	{0x10, 0x7b}, 
	{0xbb, 0xae}, 
	{0x72, 0x00}, 
	{0x3e, 0x0e}, //0x0c
	{0x74, 0x3a}, 
	{0x76, 0x01}, 
	{0x75, 0x35}, 
	{0x73, 0x00}, 
	{0xc7, 0x80}, 
	{0xc3, 0x4e}, 
	{0x33, 0x00}, 
	{0xa4, 0x50}, 
	{0xaa, 0x92}, 
	{0xc2, 0x01}, 
	{0xc1, 0xc8}, 
	{0x1e, 0x14}, 
	{0xa9, 0xfa}, 
	{0x0e, 0x61}, 
	{0x39, 0x57}, 
	{0x0f, 0x42}, 
	{0x24, 0x3c}, 
	{0x25, 0x36}, 
	{0x12, 0x62}, // 30fps vga with variopixel
	{0x03, 0x12}, 
	{0x32, 0xff}, 
	{0x17, 0x16}, 
	{0x18, 0x02}, 
	{0x19, 0x01}, 
	{0x1a, 0x3d}, 
	{0x36, 0xfa}, 
	{0x69, 0x0a}, 
	{0x8c, 0x8d}, 
	{0xc0, 0xaa}, 
	{0x40, 0xc0}, 
	{0xc6, 0x85}, 
	{0xcb, 0xf0}, 
	{0xcc, 0xd8}, 
	{0x71, 0xa0}, 
	{0xa5, 0x90}, 
	{0x6f, 0x9e}, 
	{0x42, 0xc0}, 
	{0x3f, 0x82}, 
	{0x8a, 0x23}, 
	{0x14, 0x1a}, 
	{0x3b, 0x0c}, 
	{0x2d, 0x00}, 
	{0x2e, 0x00}, 	
	{0x34, 0x3d}, 
	{0x41, 0x40}, 
	{0xc9, 0xe0}, 
	{0xca, 0xe8}, 
	{0xcd, 0x93}, 
	{0x7a, 0x20}, 
	{0x7b, 0x1c}, 
	{0x7c, 0x28}, 
	{0x7d, 0x3c}, 
	{0x7e, 0x5a}, 
	{0x7f, 0x68}, 
	{0x80, 0x76}, 
	{0x81, 0x80}, 
	{0x82, 0x88}, 
	{0x83, 0x8f}, 
	{0x84, 0x96}, 
	{0x85, 0xa3}, 
	{0x86, 0xaf}, 
	{0x87, 0xc4}, 
	{0x88, 0xd7}, 
	{0x89, 0xe8}, 
	{0x4f, 0x98}, 
	{0x50, 0x98}, 
	{0x51, 0x00}, 
	{0x52, 0x28}, 
	{0x53, 0x70}, 
	{0x54, 0x98}, 
	{0x58, 0x1a}, 
	{0x90, 0x86}, 
	{0x91, 0x84}, 
	{0x9f, 0x75}, 
	{0xa0, 0x73}, 
	{0x16, 0x14}, 
	{0x2a, 0x00}, 
	{0x2b, 0x00}, 
	{0xac, 0x80}, 
	{0xad, 0x80}, 
	{0xae, 0x80}, 
	{0xaf, 0x80}, 
	{0xb2, 0xf2}, 
	{0xb3, 0x20}, 
	{0xb4, 0x20}, 
	{0xb6, 0xaf}, 
	{0xb6, 0xaf}, 
	{0x04, 0x03}, 
	{0x05, 0x2b}, 
	{0x06, 0x35}, 
	{0x07, 0x36}, 
	{0x08, 0x3b}, 
	{0x2f, 0x35}, 
	{0x4a, 0xea}, 
	{0x4b, 0xe6}, 
	{0x4c, 0xe6}, 
	{0x4d, 0xe6}, 
	{0x4e, 0xe6}, 
	{0x70, 0x0b}, 
	{0xa6, 0x40}, 
	{0xbc, 0x04}, 
	{0xbd, 0x01}, 
	{0xbe, 0x03}, 
	{0xbf, 0x01}, 
	{0xbf, 0x01}, 
	{0x43, 0x14}, 
	{0x44, 0xf0}, 
	{0x45, 0x46}, 
	{0x46, 0x62}, 
	{0x47, 0x2a}, 
	{0x48, 0x3c}, 
	{0x59, 0x85}, 
	{0x5a, 0xa9}, 
	{0x5b, 0x64}, 
	{0x5c, 0x84}, 
	{0x5d, 0x53}, 
	{0x5e, 0x0e}, 
	{0x6c, 0x0c}, 
	{0x6d, 0x55}, 
	{0x6e, 0x00}, 
	{0x6f, 0x9e}, 
	{0x62, 0x00}, 
	{0x63, 0x00}, 
	{0x64, 0x02}, 
	{0x65, 0x20}, 
	{0x66, 0x01}, 
	{0x9d, 0x02}, 
	{0x9e, 0x03}, 
	{0x29, 0x15}, 
	{0xa9, 0xef}, 
	{0x11, 0x40}, 
	{0x6b, 0x1a}, 
	{0x92, 0x64}, 
	{0x93, 0x00}, 
	{0xa2, 0x94}, 
	{0xa3, 0x94}, 
	{0x6a, 0x4b}, 
	{0x13, 0xe7}, 


};


LOCAL SENSOR_REG_TAB_INFO_T s_ov9655_resolution_Tab_YUV[]=
{	
        // COMMON INIT
        {ADDR_AND_LEN_OF_ARRAY(ov9655_YUV_640X480), 640, 480, 24,SENSOR_IMAGE_FORMAT_YUV422},	

        // YUV422 PREVIEW 1
        {ADDR_AND_LEN_OF_ARRAY(ov9655_YUV_640X480), 640, 480, 24,SENSOR_IMAGE_FORMAT_YUV422},
        {ADDR_AND_LEN_OF_ARRAY(ov9655_YUV_1280X1024), 1280, 1024, 11,SENSOR_IMAGE_FORMAT_YUV422},	
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},

        // YUV422 PREVIEW 2 
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0} 
};

LOCAL SENSOR_IOCTL_FUNC_TAB_T s_ov9655_ioctl_func_tab = 
{
        // Internal 
        PNULL,
        PNULL,
        PNULL,
        ov9655_Identify,

        PNULL,			// write register
        PNULL,			// read  register	
        PNULL,
        PNULL,

        // External
        set_ae_enable,
        set_hmirror_enable,
        set_vmirror_enable,

        set_brightness,
        set_contrast,
        set_sharpness,
        set_saturation,

        set_preview_mode,	
        set_image_effect,

        ov9655_BeforeSnapshot,
        PNULL,

        PNULL,

        read_ev_value,
        write_ev_value,
        read_gain_value,
        write_gain_value,
        read_gain_scale,
        set_frame_rate,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL
};

SENSOR_INFO_T g_OV9655_yuv_info =
{
        ov9655_I2C_ADDR_W,				// salve i2c write address
        ov9655_I2C_ADDR_R, 				// salve i2c read address

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
        SENSOR_IMAGE_EFFECT_NEGATIVE|\
        SENSOR_IMAGE_EFFECT_CANVAS,

        // while balance mode
        0,

        7,								// bit[0:7]: count of step in brightness, contrast, sharpness, saturation
        						// bit[8:31] reseved

        SENSOR_LOW_PULSE_RESET,			// reset pulse level
        20,								// reset pulse width(ms)

        SENSOR_HIGH_LEVEL_PWDN,			// 1: high level valid; 0: low level valid

        1,								// count of identify code
        {{0x0A, 0x96},						// supply two code to identify sensor.
        {0x0B, 0x57}},						// for Example: index = 0-> Device id, index = 1 -> version id											
        								
        SENSOR_AVDD_2800MV,				// voltage of avdd	

        1280,							// max width of source image
        1024,							// max height of source image
        "OV9655",						// name of sensor												

        SENSOR_IMAGE_FORMAT_YUV422,		// define in SENSOR_IMAGE_FORMAT_E enum,
        						// if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T
        SENSOR_IMAGE_PATTERN_YUV422_YUYV,	// pattern of input image form sensor;			

        s_ov9655_resolution_Tab_YUV,	// point to resolution table information structure
        &s_ov9655_ioctl_func_tab,		// point to ioctl function table

        PNULL,							// information and table about Rawrgb sensor
        PNULL,							// extend information about sensor	
        SENSOR_AVDD_2800MV,                     // iovdd
        SENSOR_AVDD_1800MV,                      // dvdd
        4,                     // skip frame num before preview 
        3,                      // skip frame num before capture
        0,                      // deci frame num during preview	
        2,                      // deci frame num during video preview

        0,	// threshold enable		
        0,  // threshold mode 	
        0,	// threshold start postion	
        0,	// threshold end postion 
        0	
};

LOCAL uint32_t set_ae_enable(uint32_t enable)
{
        unsigned char value;
        SENSOR_PRINT("set_ae_enable: enable = %d", enable);
        value = Sensor_ReadReg(0x13);
        value &= 0xFE;
        value |= enable;
        Sensor_WriteReg(0x13, value);
        return 0;
}

LOCAL uint32_t set_hmirror_enable(uint32_t enable)
{
        return 0;
#if 0	
	SCI_TRACE_LOW("set_hmirror_enable: enable = %d", enable);

	value = Sensor_ReadReg(0x1E);
	value &= 0xDF;
	value |= enable<<5;
	Sensor_WriteReg(0x1E, value);

	return 0;
#endif
}

LOCAL uint32_t set_vmirror_enable(uint32_t enable)
{
        return 0;
#if 0	
	SCI_TRACE_LOW("set_vmirror_enable: enable = %d", enable);

	value = Sensor_ReadReg(0x1E);
	value &= 0xEF;
	value |= enable<<4;
	Sensor_WriteReg(0x1E, value);

	return 0;
#endif
}

LOCAL uint32_t read_ev_value(uint32_t value)
{
        uint8_t expo_low;
        uint8_t expo_mid;
        uint8_t expo_high;
        uint32_t read_exposure = 0;

        expo_low  = Sensor_ReadReg(0x04);
        expo_mid  = Sensor_ReadReg(0x10);
        expo_high = Sensor_ReadReg(0xA1);	
        read_exposure = (expo_low & 0x03) + (expo_mid << 0x02) + ((expo_high & 0x3f) << 0x0a);
        SENSOR_PRINT("read_ae_value: %x", read_exposure);
        return read_exposure;
}

LOCAL uint32_t write_ev_value(uint32_t exposure_value)
{
        uint8_t expo_low;
        uint8_t expo_mid;
        uint8_t expo_high;
        SENSOR_PRINT("write_ae_value: %x", exposure_value);
        expo_low  = Sensor_ReadReg(0x04);	
        expo_high = Sensor_ReadReg(0xA1);
        expo_low = (expo_low&0xfc) + (exposure_value&0x03);
        expo_mid = (exposure_value>>0x02)&0xff;
        expo_high = (expo_high&0xc0) + ((exposure_value>>0x0a)&0x3f);
        Sensor_WriteReg(0x04, expo_low);
        Sensor_WriteReg(0x10, expo_mid);
        Sensor_WriteReg(0xa1, expo_high);
        return 0;	
}

LOCAL uint32_t read_gain_value(uint32_t value)
{
        uint32_t read_gain;
        uint8_t pre_gain00;
        uint8_t pre_gain03;

        pre_gain00 = Sensor_ReadReg(0x00);
        pre_gain03 = Sensor_ReadReg(0x03);

        read_gain = (pre_gain00 & 0x0f) + 16;
        if(pre_gain00 & 0x10)
        read_gain = read_gain << 1;
        if (pre_gain00 & 0x20)
        read_gain = read_gain << 1;
        if (pre_gain00 & 0x40)
        read_gain = read_gain << 1;
        if (pre_gain00 & 0x80)
        read_gain = read_gain << 1;

        if (pre_gain03 & 0x40)
                read_gain = read_gain << 1;		
        if (pre_gain03 & 0x80)
                read_gain = read_gain << 1;
        SENSOR_PRINT("read_gain_value: %x", read_gain);
        return read_gain;
}

LOCAL uint32_t write_gain_value(uint32_t gain_value)
{
        uint8_t write_gain00;
        uint8_t write_gain03;	

        SENSOR_PRINT("write_gain_value: %x \n", gain_value);

        write_gain00 =0;
        write_gain03 = Sensor_ReadReg(0x03);
        write_gain03 &= 0x3f;
	
        if (gain_value > 31) {
                write_gain00 |= 0x10;
                gain_value = gain_value >> 1;
        }	
        if (gain_value > 31) {
                write_gain00 |= 0x20;
                gain_value = gain_value >> 1;
        }	
        if (gain_value > 31) {
                write_gain00 |= 0x40;
                gain_value = gain_value >> 1;
        }	
        if (gain_value > 31) {
                write_gain00 |= 0x80;
                gain_value = gain_value >> 1;
        }
        if (gain_value > 31) {
                write_gain03 |= 0x40;
                gain_value = gain_value >> 1;
        }	
        if (gain_value > 31) {
                write_gain03 |= 0x80;
                gain_value = gain_value >> 1;
        }		

        if (gain_value > 16) 
                write_gain00 |= ((gain_value -16) & 0x0f); 

        Sensor_WriteReg(0x00, write_gain00);
        Sensor_WriteReg(0x03, write_gain03);
        return 0;
}

LOCAL uint32_t read_gain_scale(uint32_t value)
{
        return SENSOR_GAIN_SCALE;
}
LOCAL uint32_t set_preview_mode(uint32_t preview_mode)
{
        SENSOR_PRINT("set_preview_mode: preview_mode = %d \n", preview_mode);
        return 0;
#if 0	
	s_preview_mode = preview_mode;
	
	switch (preview_mode)
	{
		case DCAMERA_ENVIRONMENT_NORMAL: 
			Sensor_WriteReg(0x11, 0x81);
			Sensor_WriteReg(0x14, 0x1e);
			Sensor_WriteReg(0x6A, 0x4B);
			Sensor_WriteReg(0x3b, 0x19);
			Sensor_WriteReg(0x39, 0x43);
			Sensor_WriteReg(0x38, 0x12);
			Sensor_WriteReg(0x35, 0x91);
			Sensor_WriteReg(0x24, 0x90);
			Sensor_WriteReg(0x25, 0x80);
			Sensor_WriteReg(0x26, 0xC6);
			Sensor_WriteReg(0x62, 0x1E);
			Sensor_WriteReg(0x63, 0x00);
			Sensor_WriteReg(0x64, 0x04);
			Sensor_WriteReg(0x65, 0x10);
			Sensor_WriteReg(0x66, 0x05);
			Sensor_WriteReg(0x9D, 0x04);
			Sensor_WriteReg(0x9E, 0x04);

			break;
		case DCAMERA_ENVIRONMENT_NIGHT:

			Sensor_WriteReg(0x11, 0x83);
			Sensor_WriteReg(0x14, 0x3e);
			Sensor_WriteReg(0x3b, 0x19); 
			Sensor_WriteReg(0x6A, 0x25);
			Sensor_WriteReg(0x39, 0x43);
			Sensor_WriteReg(0x38, 0x12);
			Sensor_WriteReg(0x35, 0x91);
			Sensor_WriteReg(0x24, 0xA0);
			Sensor_WriteReg(0x25, 0x90);
			Sensor_WriteReg(0x26, 0xe2);        
			Sensor_WriteReg(0x66, 0x00); 
			
			break;
		case DCAMERA_ENVIRONMENT_SUNNY:
		
			Sensor_WriteReg(0x11, 0x80);
			Sensor_WriteReg(0x14, 0x1e);
			Sensor_WriteReg(0x6A, 0x96);
			Sensor_WriteReg(0x3b, 0x19);
			Sensor_WriteReg(0x39, 0x50);
			Sensor_WriteReg(0x38, 0x92);
			Sensor_WriteReg(0x35, 0x81);
			Sensor_WriteReg(0x24, 0x70);
			Sensor_WriteReg(0x25, 0x60);
			Sensor_WriteReg(0x26, 0xe2); 
			Sensor_WriteReg(0x62, 0x1E);
			Sensor_WriteReg(0x63, 0x00);
			Sensor_WriteReg(0x64, 0x04);
			Sensor_WriteReg(0x65, 0x10);
			Sensor_WriteReg(0x66, 0x05);
			Sensor_WriteReg(0x9D, 0x04);
			Sensor_WriteReg(0x9E, 0x04);	
			break;
		default:
			break;
	}
	
	return 0;
#endif
}

const SENSOR_REG_T ov9655_brightness_tab[][2]=
{
        {
        {0x55,0x50},{0xFF,0xFF},
        },

        {
        {0x55,0x90},{0xFF,0xFF},
        },

        {
        {0x55,0x00},{0xFF,0xFF},
        },

        {
        {0x55,0x10},{0xFF,0xFF},	
        },

        {
        {0x55,0x20},{0xFF,0xFF},
        },

        {
        {0x55,0x30},{0xFF,0xFF},
        },

        {
        {0x55,0x40},{0xFF,0xFF},
        }
};

const SENSOR_REG_T ov9655_contrast_tab[][2]=
{
        {
        {0x56, 0x20},{0xFF, 0xFF},
        },

        {
        {0x56, 0x30},{0xFF, 0xFF},
        },

        {
        {0x56, 0x40},{0xFF, 0xFF},
        },

        {
        {0x56, 0x50},{0xFF, 0xFF},
        },

        {
        {0x56, 0x50},{0xFF, 0xFF},
        },

        {
        {0x56, 0x50},{0xFF, 0xFF},
        },


        {
        {0x56, 0x50},{0xFF, 0xFF},
        },
};

const SENSOR_REG_T ov9655_image_effect_tab[][4]=
{
        // effect normal
        {
        {0x3a, 0x00},{0x67, 0x80},{0x68, 0x80},	{0xff, 0xff},
        },
        // effect BLACKWHITE
        {
        {0x3a, 0x10},{0x67, 0x80},{0x68, 0x80},	{0xff, 0xff},
        },
        // effect RED 
        {
        {0x3a, 0x10},{0x67, 0x80},{0x68, 0xc0},	{0xff, 0xff},
        },

        // effect GREEN 
        {
        {0x3a, 0x10},{0x67, 0x40},{0x68, 0x40},	{0xff, 0xff},
        },

        // effect  BLUE
        {
        {0x3a, 0x10},{0x67, 0xc0},{0x68, 0x80},	{0xff, 0xff},
        },
        // effect yellow
        {
        {0x3a, 0x10},{0x67, 0x40},{0x68, 0x80},	{0xff, 0xff},
        },

        // effect NEGATIVE
        {
        {0x3a, 0x20},{0x67, 0x80},{0x68, 0x80},	{0xff, 0xff},
        },

        // effect canvas
        {
        {0x3a, 0x10},{0x67, 0x40},{0x68, 0xa0},	{0xff, 0xff},
        },
        /*
        // effect CANVAS ANTIQUE
        {
        {0x3a, 0x18},{0x67, 0x40},{0x68, 0xb0},	{0xff, 0xff},
        }
        */
};

const SENSOR_REG_T ov9655_saturation_tab[][12]=
{
        {
        {0x4f, 0x0f}, {0x50, 0x0f}, {0x51, 0x01}, {0x52, 0x05}, {0x53, 0x0a},
        {0x54, 0x0e}, {0x55, 0x40}, {0x56, 0x40}, {0x57, 0x40}, {0x58, 0x0d},
        {0xff, 0xff}, {0xff, 0xff},
        },

        {
        {0x4f, 0x1d},{0x50, 0x1f},{0x51, 0x02},{0x52, 0x09},{0x53, 0x13},
        {0x54, 0x1c},{0x55, 0x40},{0x56, 0x40},{0x57, 0x40},{0x58, 0x0d},
        {0xff, 0xff},{0xff, 0xff},		
        },

        {
        {0x4f, 0x2c},{0x50, 0x2e},{0x51, 0x02},{0x52, 0x0e},{0x53, 0x1d},
        {0x54, 0x2a},{0x55, 0x40},{0x56, 0x40},{0x57, 0x40},{0x58, 0x0d},
        {0xff, 0xff},{0xff, 0xff},		
        },	

        {      
        {0x4f, 0x3a},{0x50, 0x3d},{0x51, 0x03},{0x52, 0x12},{0x53, 0x26},
        {0x54, 0x38},{0x55, 0x40},{0x56, 0x40},{0x57, 0x40},{0x58, 0x0d},
        {0xff, 0xff},{0xff, 0xff},		
        },

        {
        {0x4f, 0x49},{0x50, 0x4c},{0x51, 0x04},{0x52, 0x17},{0x53, 0x30},
        {0x54, 0x46},{0x55, 0x40},{0x56, 0x40},{0x57, 0x40},{0x58, 0x0d},
        {0xff, 0xff},{0xff, 0xff},		
        },
};

const SENSOR_REG_T ov9655_sharpness_tab[][2]=
{
        {
        {0x3F,0xA7},{0xFF,0xFF},
        },			

        {
        {0x3F,0xA6},{0xFF,0xFF},
        },
        {
        {0x3F,0xA5},{0xFF,0xFF},
        },

        {
        {0x3F,0xA4},{0xFF,0xFF},
        },

        {
        {0x3F,0xA3},{0xFF,0xFF},
        },

        {
        {0x3F,0xA2},{0xFF,0xFF},
        },

        {
        {0x3F,0xA1},{0xFF,0xFF},
        },

        {
        {0x3F,0xA0},{0xFF,0xFF},
        },			
};
LOCAL uint32_t set_brightness(uint32_t level)
{
        uint16_t i;

        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)ov9655_brightness_tab[level];
        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value) ; i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_PRINT("set_brightness: level = %d", level);
        return 0;
}
LOCAL uint32_t set_contrast(uint32_t level)
{
        uint16_t i;	
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)ov9655_contrast_tab[level];

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value) ; i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_PRINT("set_contrast: level = %d", level);
        return 0;
}

LOCAL uint32_t set_sharpness(uint32_t level)
{
        return 0;
#if 0
	SCI_ASSERT(PNULL != sensor_reg_ptr);
	
	for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value) ; i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

	SCI_TRACE_LOW("set_sharpness: level = %d", level);
	
	return 0;
#endif
}

LOCAL uint32_t set_saturation(uint32_t level)
{
        return 0;
#if 0
	SCI_ASSERT(PNULL != sensor_reg_ptr);
	
	for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value) ; i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}	
	
	SCI_TRACE_LOW("set_saturation: level = %d", level);
	
	return 0;
#endif
}

LOCAL uint32_t set_image_effect(uint32_t effect_type)
{
        uint16_t i;	
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)ov9655_image_effect_tab[effect_type];

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value) ; i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_PRINT("-----------set_image_effect: effect_type = %d------------", effect_type);
        return 0;
}

LOCAL uint32_t set_frame_rate(uint32_t param)
{
        uint32_t clkrc = 0;

        SENSOR_PRINT("set_frame_rate: param = %d", param);

        if((param<1)||(param>64)) {
                SENSOR_PRINT("OV9655 set_frame_rate:param error,param=%d .\n",param);
                return SENSOR_FAIL;
        }
        clkrc = Sensor_ReadReg(0x11);
        clkrc = (clkrc&0xc0)+(param-1);
        Sensor_WriteReg(0x11,clkrc); 
        return 0;
}

LOCAL uint32_t ov9655_Identify(uint32_t param)
{
#define ov9655_PID_VALUE	0x96	
#define ov9655_PID_ADDR		0x0A
#define ov9655_VER_VALUE	0x57	
#define ov9655_VER_ADDR		0x0B
	
        uint8_t pid_value = 0;
        uint8_t ver_value = 0;
        BOOLEAN ret_value = 0XFF;
        uint8_t i2c_cmd[2];

        i2c_cmd[0] = ov9655_PID_ADDR;

        // Get Version Code
        pid_value =Sensor_ReadReg(ov9655_PID_ADDR);// i2c_cmd[0];
        ver_value =Sensor_ReadReg(ov9655_VER_ADDR); //i2c_cmd[1];
	
        if(ov9655_PID_VALUE == pid_value) {
                // Get Device Code		
                if(ov9655_VER_VALUE == ver_value) {
                        SENSOR_PRINT("That is ov9655 sensor !\n");		
                } else {
                        SENSOR_PRINT("ov9655_Identify: That is OV%x%x sensor !\n", pid_value, ver_value);
                }
                ret_value = 0;
        }
        SENSOR_PRINT("ov9655_Identify: PID = %x, VER = %x",pid_value, ver_value);
        return (uint32_t)ret_value;
}

LOCAL void caculate_expo( uint16_t pre_expo,uint16_t pre_gain ,uint16_t *cap_expo,uint16_t *cap_gain  )
{//preview as VGA 12.5fps,capture at SXGA 6.25fps  no auto nightmode
        uint32_t Capture_Exposure_Gain ;
        uint16_t Capture_Exposure =0;
        uint16_t Capture_gain;
        uint16_t pre_array1[3] = {105, 210, 53};	
	
        SENSOR_PRINT("SENSOR ov9655 : caculate_expo PREVIEW MODE %d", s_preview_mode);
        pre_expo =pre_expo*pre_array1[s_preview_mode]/100 ; //pre_expo*21*/40; 
        Capture_Exposure_Gain = pre_gain * pre_expo;
        if (Capture_Exposure_Gain < 1048*16) {
                Capture_Exposure = Capture_Exposure_Gain/16;
                if (Capture_Exposure > 78)  {
                        Capture_Exposure = (Capture_Exposure+1)/78*78;
                }		
                //Capture_gain = 16;		
                Capture_gain = Capture_Exposure_Gain/Capture_Exposure;
        }
        else {//if image is dark,//and use the correct exposure time to eliminate 50/60 hz line
                Capture_Exposure = 1048;
                Capture_gain = Capture_Exposure_Gain / 1048;
        }
        *cap_expo =Capture_Exposure;
        *cap_gain=Capture_gain;
}

LOCAL void Sensor_Set_ChangeMega_ov9655(void) 
{
        uint8_t pre_gain00;	
        uint8_t pre_expoA1;	
        uint8_t pre_expo10;	
        uint8_t pre_expo04;	
        uint16_t  pre_expo;
        uint16_t pre_gain ;
        uint16_t cap_expo,cap_gain;
        uint8_t data, cap_expo04, cap_expo10, cap_expoA1, cap_gain00;
        uint8_t reg_value = 0;

        reg_value = Sensor_ReadReg(0x13);
        reg_value &= 0xfa;
        Sensor_WriteReg(0x13, reg_value);

        pre_gain00 = Sensor_ReadReg(0x00);
        pre_expo10 = Sensor_ReadReg(0x10);
        pre_expoA1 = Sensor_ReadReg(0xA1);
        pre_expo04 = Sensor_ReadReg(0x04);

        //caculate pre_exposure
        pre_expo= (pre_expoA1&0x3f);
        pre_expo= ((pre_expo<<8)+pre_expo10);
        pre_expo= ((pre_expo<<2)+(pre_expo04&0x03));
        //caculate pre_gain
        pre_gain = (pre_gain00 & 0x0f) + 16;//pre_gain = (pre_gain00 & 0x0f) + 16;
        if(pre_gain00 & 0x10)
                pre_gain = pre_gain << 1;
        if (pre_gain00 & 0x20)
                pre_gain = pre_gain << 1;
        if (pre_gain00 & 0x40)
                pre_gain = pre_gain << 1;
        if (pre_gain00 & 0x80)
                pre_gain = pre_gain << 1;

        caculate_expo( pre_expo,pre_gain ,&cap_expo,&cap_gain  );	
        data = cap_expo & 0x03;
        cap_expo04=((pre_expo04 & 0xfc)|data);
	 
        data = (cap_expo >> 2);
        cap_expo10 = data;	
        	
        data = (uint8_t)(cap_expo >> 10);	
        cap_expoA1 = data;
         //exposure

        //write gain
        cap_gain00 =0;
        if (cap_gain > 31) {
                cap_gain00 |= 0x10;
                cap_gain = cap_gain >> 1;
        }	
        if (cap_gain > 31) {
                cap_gain00 |= 0x20;
                cap_gain = cap_gain >> 1;
        }	
        if (cap_gain > 31) {
                cap_gain00 |= 0x40;
                cap_gain = cap_gain >> 1;
        }	
        if (cap_gain > 31) {
                cap_gain00 |= 0x80;
                cap_gain = cap_gain >> 1;
        }	

        if (cap_gain > 16) 
                cap_gain00 |= ((cap_gain -16) & 0x0f); 

        //show reseult// write 
        Sensor_WriteReg(0x00, cap_gain00);
        Sensor_WriteReg(0xA1, cap_expoA1);
        Sensor_WriteReg(0x10, cap_expo10);
        Sensor_WriteReg(0x04, cap_expo04);

        Sensor_ReadReg(0x00);
        Sensor_ReadReg(0x10);
        Sensor_ReadReg(0xA1);
        Sensor_ReadReg(0x04);	
}

LOCAL uint32_t ov9655_BeforeSnapshot(uint32_t param)
{
//	SENSOR_MODE_E sensor_mode = (SENSOR_MODE_E)param;
        return 0;
#if 0	
	SCI_TRACE_LOW("ov9655_BeforeSnapshot: param = %d", param);
	
	if(SENSOR_MODE_SNAPSHOT_ONE_FIRST == sensor_mode)
	{
		Sensor_Set_ChangeMega_ov9655();
	}
#endif	
}
struct sensor_drv_cfg sensor_ov9655 = {
        .sensor_pos = CONFIG_DCAM_SENSOR_POS_OV9655,
        .sensor_name = "ov9655",
        .driver_info = &g_OV9655_yuv_info,
};

static int __init sensor_ov9655_init(void)
{
        return dcam_register_sensor_drv(&sensor_ov9655);
}

subsys_initcall(sensor_ov9655_init);

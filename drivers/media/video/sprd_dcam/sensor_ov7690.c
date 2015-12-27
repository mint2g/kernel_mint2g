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
 #define DCAM_Sleep msleep
#define OV7690_I2C_ADDR_W 0x21 //0x42
#define OV7690_I2C_ADDR_R 0x21 //0x43
#define SENSOR_GAIN_SCALE		16
//select Lense type
//#define 	Sunny_F28
//#define 	Sunny_F24
//#define 	Phoenix_F28
#define	Phoenix_F24
//#define 	Dongya_F24
LOCAL uint32_t s_preview_mode; 
/**---------------------------------------------------------------------------*
 ** 					Local Function Prototypes							  *
 **---------------------------------------------------------------------------*/
LOCAL uint32_t set_OV7690_ae_awb_enable(uint32_t ae_enable, uint32_t awb_enable);
LOCAL uint32_t set_OV7690_ae_enable(uint32_t enable);
LOCAL uint32_t set_OV7690_hmirror_enable(uint32_t enable);
LOCAL uint32_t set_OV7690_vmirror_enable(uint32_t enable);
LOCAL uint32_t set_OV7690_preview_mode(uint32_t preview_mode);
//LOCAL uint32_t _OV7690_Power_On(uint32_t power_on);
LOCAL uint32_t OV7690_Identify(uint32_t param);
LOCAL uint32_t OV7690_BeforeSnapshot(uint32_t param);
LOCAL uint32_t OV7690_After_Snapshot(uint32_t param);
LOCAL uint32_t OV7690_set_brightness(uint32_t level);
LOCAL uint32_t OV7690_set_contrast(uint32_t level);
LOCAL uint32_t OV7690_set_sharpness(uint32_t level);
LOCAL uint32_t OV7690_set_saturation(uint32_t level);
LOCAL uint32_t OV7690_set_image_effect(uint32_t effect_type);
LOCAL uint32_t OV7690_read_ev_value(uint32_t value);
LOCAL uint32_t OV7690_write_ev_value(uint32_t exposure_value);
LOCAL uint32_t OV7690_read_gain_value(uint32_t value);
LOCAL uint32_t OV7690_write_gain_value(uint32_t gain_value);
LOCAL uint32_t OV7690_read_gain_scale(uint32_t value);
LOCAL uint32_t OV7690_set_frame_rate(uint32_t param);
LOCAL uint32_t OV7690_set_work_mode(uint32_t mode);
LOCAL uint32_t OV7690_set_whitebalance_mode(uint32_t mode);
LOCAL uint32_t set_ov7690_video_mode(uint32_t mode);
LOCAL uint32_t set_ov7690_ev(uint32_t level);
/**---------------------------------------------------------------------------*
 ** 						Local Variables 								 *
 **---------------------------------------------------------------------------*/
const SENSOR_REG_T OV7690_YUV_640X480[]= {	// 640*480 preview param
        {0x12, 0x80},
        {SENSOR_WRITE_DELAY, 0x0A},//10ms

        {0x0c,0x96},
        {0x48,0x46},// 1.75x pre-gain
        {0x49,0x0d},
        {0x41,0x43},
        {0x81,0xff},
        {0x21,0x45},//BD step	0x45-->24M, 0x23-->26M
        {0x16,0x03},
        {0x39,0x80},
        //,,===Format===,,
        {0x11,0x00},
        {0x12,0x00},
        {0x82,0x03},
        {0xd0,0x48},
        {0x80,0x7f},
        {0x3e,0x30},
        {0x22,0x00},
        {0x2a,0x30},
        {0x2b,0x0b},////horizontal: 0x30b->780tp(24M Pclk) , 0x34d->845tp(26M Pclk)                         
//     [   {0x2c,0xff},//vertial: 768 lines 
        {0x2c,0x80},//vertial: 768 lines 
        {0x15,0x94},//auto down to 1/3 frame rate, 30fps~10fps //modify for fixed frame!Wanwei
        //,,===Resolution===,,
        {0x17,0x69},
        {0x18,0xa4},
        {0x19,0x0c},
        {0x1a,0xf6},
        {0xc8,0x02},
        {0xc9,0x80},
        {0xca,0x01},
        {0xcb,0xe0},
        {0xcc,0x02},
        {0xcd,0x80},
        {0xce,0x01},
        {0xcf,0xe0},

        //;;===Lens Correction==;
        {0x85,0x90},
        {0x86,0x00},
        {0x87,0x00},
        {0x88,0x00},
        {0x89,0x18},
        {0x8a,0x14},
        {0x8b,0x15},
                
        //;;====Color Matrix======
                
        {0xbb,0xac},
        {0xbc,0xae},
        {0xbd,0x02},
        {0xbe,0x1f},
        {0xbf,0x93},
        {0xc0,0xb1},  
        {0xc1,0x1A},
                
        //;;===Edge + Denoise====;
                
        {0xb4,0x04},
        {0xb7,0x07},
        {0xb8,0x08},
        {0xb9,0x00},
        {0xba,0x00},

        //;;====AEC/AGC target===

        /*This only in fixed frame change!*/
        {0x24,0x58},//0x5a
        {0x25,0x48},
        {0x26,0x73},
                
        //;;=====UV adjust======; 
        {0x81,0xff},
        {0x5A,0x74},
        {0x5B,0x9f},
        {0x5C,0x42},
        {0x5d,0x42},

        //;;====Gamma====;;               
                            
        {0xa3,0x05},     
        {0xa4,0x10},     
        {0xa5,0x24},     
        {0xa6,0x4a},     
        {0xa7,0x5a},     
        {0xa8,0x68},     
        {0xa9,0x74},     
        {0xaa,0x7f},     
        {0xab,0x89},     
        {0xac,0x91},     
        {0xad,0x9f},     
        {0xae,0xAc},     
        {0xaf,0xC1},     
        {0xb0,0xD5},     
        {0xb1,0xE7},     
        {0xb2,0x21},     

        //;;==Advance AWB==;;

        {0x8c, 0x5c},
        {0x8d, 0x11},
        {0x8e, 0x12},
        {0x8f, 0x19},
        {0x90, 0x50},
        {0x91, 0x00},
        {0x92, 0x86},
        {0x93, 0x80},
        {0x94, 0x13},
        {0x95, 0x1b},
        {0x96, 0xff},
        {0x97, 0x00},
        {0x98, 0x3e},
        {0x99, 0x31},
        {0x9a, 0x4e},
        //(0x9b, 0x41)20081217 Here adjust AWB, when enter preview, AWB didn't work!
        {0x9b, 0x3d},
        {0x9c, 0xf0},
        {0x9d, 0xf0},
        {0x9e, 0xf0},
        {0x9f, 0xff},
        {0xa0, 0x6a},
        {0xa1, 0x65},
        {0xa2, 0x11},

        {0x14,0x21},//8x gain ceiling, PPChrg off

        {0x50, 0x9a},//50hz banding filter value
        {0x51, 0x80},//60hz banding filter value
        {0x13,0xf7},
        {0x1e, 0x33},

        {0x68, 0xb2} 
};

LOCAL SENSOR_REG_TAB_INFO_T s_OV7690_resolution_Tab_YUV[]=
{	
        // COMMON INIT
        { ADDR_AND_LEN_OF_ARRAY(OV7690_YUV_640X480),		640,	480,	24 ,0},
        //{ ADDR_AND_LEN_OF_ARRAY(OV7690_YUV_640X480),		0,	0,	24 ,SENSOR_IMAGE_FORMAT_YUV422},
        // YUV422 PREVIEW 1
        { NULL, 0, 640, 480, 24, 0},
        //{ ADDR_AND_LEN_OF_ARRAY(OV7690_YUV_640X480),		640,	480,	24 ,SENSOR_IMAGE_FORMAT_YUV422},

        { PNULL,		0,			0,		0,	0,							0 },
        { PNULL,		0,			0,		0,	0,							0 },
        { PNULL,		0,			0,		0,	0,							0 },

        // YUV422 PREVIEW 2 
        { PNULL, 0,	0, 0,	0, 0},
        { PNULL, 0,	0, 0,	0, 0},
        { PNULL, 0,	0, 0,	0, 0},
        { PNULL, 0,	0, 0,	0, 0}  
};

LOCAL SENSOR_IOCTL_FUNC_TAB_T s_OV7690_ioctl_func_tab = 
{
        // Internal 
        PNULL,

        //#if defined(PLATFORM_SC8800G)
        PNULL,
        //#else
        //   _OV7690_Power_On,
        //#endif

        PNULL,
        OV7690_Identify,

        PNULL,			// write register
        PNULL,			// read  register	
        PNULL,
        PNULL,

        // External
        set_OV7690_ae_enable,
        set_OV7690_hmirror_enable,
        set_OV7690_vmirror_enable,

        OV7690_set_brightness,
        OV7690_set_contrast,
        OV7690_set_sharpness,
        OV7690_set_saturation,

        set_OV7690_preview_mode,	
        OV7690_set_image_effect,

        OV7690_BeforeSnapshot,
        OV7690_After_Snapshot,

        PNULL,

        OV7690_read_ev_value,
        OV7690_write_ev_value,
        OV7690_read_gain_value,
        OV7690_write_gain_value,
        OV7690_read_gain_scale,
        OV7690_set_frame_rate,

        PNULL,
        PNULL,
        OV7690_set_whitebalance_mode,
        PNULL,	// get snapshot skip frame num from customer, input SENSOR_MODE_E paramter

        PNULL,	// set ISO level					 0: auto; other: the appointed level
        set_ov7690_ev, // Set exposure compensation	 0: auto; other: the appointed level

        PNULL, // check whether image format is support
        PNULL, //change sensor image format according to param
        PNULL, //change sensor image format according to param

        // CUSTOMER FUNCTION	                      
        PNULL,  	// function 3 for custumer to configure                      
        PNULL,	// function 4 for custumer to configure 	
        PNULL, // Set anti banding flicker	 0: 50hz;1: 60	
        set_ov7690_video_mode, // set video mode

        PNULL,   // pick out the jpeg stream from given buffer
};
/**---------------------------------------------------------------------------*
 ** 						Global Variables								  *
 **---------------------------------------------------------------------------*/
 SENSOR_INFO_T g_OV7690_yuv_info =
{
        OV7690_I2C_ADDR_W,				// salve i2c write address
        OV7690_I2C_ADDR_R, 				// salve i2c read address

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

        0x77777,
        //7,								// bit[0:7]: count of step in brightness, contrast, sharpness, saturation
        							// bit[8:31] reseved

        SENSOR_LOW_PULSE_RESET,			// reset pulse level
        20,								// reset pulse width(ms)

        SENSOR_HIGH_LEVEL_PWDN,			// 1: high level valid; 0: low level valid

        2,								// count of identify code
        //0x0A, 0x76,						// supply two code to identify sensor.
        //0x0B, 0x91,						// for Example: index = 0-> Device id, index = 1 -> version id	
        {{0x0A, 0x76},						// supply two code to identify sensor.
        {0x0B, 0x91}},						// for Example: index = 0-> Device id, index = 1 -> version id	


        SENSOR_AVDD_2800MV,				// voltage of avdd	

        640,							// max width of source image
        480,							// max height of source image
        "OV7690",						// name of sensor												

        SENSOR_IMAGE_FORMAT_YUV422,		// define in SENSOR_IMAGE_FORMAT_E enum,
        							// if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T
        SENSOR_IMAGE_PATTERN_YUV422_YUYV,	// pattern of input image form sensor;			

        s_OV7690_resolution_Tab_YUV,	// point to resolution table information structure
        &s_OV7690_ioctl_func_tab,		// point to ioctl function table
        	
        PNULL,							// information and table about Rawrgb sensor
        PNULL,							// extend information about sensor	
        SENSOR_AVDD_2800MV,                     // iovdd
        SENSOR_AVDD_1800MV,                      // dvdd
        4,                     // skip frame num before preview 
        3,                      // skip frame num before capture
        0,                      // deci frame num during preview	
        2,                       // deci frame num during video preview
        0,
        0,
        0,
        0,
        0	
};
/**---------------------------------------------------------------------------*
 ** 							Function  Definitions
 **---------------------------------------------------------------------------*/
LOCAL void OV7690_WriteReg( uint8_t  subaddr, uint8_t data )
{
        Sensor_WriteReg_8bits( subaddr, data);
        SENSOR_TRACE("SENSOR: OV7690_WriteReg reg/value(%x,%x) !!\n", subaddr, data);
}
LOCAL uint8_t OV7690_ReadReg( uint8_t  subaddr)
{
        uint8_t value = 0;	
        Sensor_ReadReg_8bits(subaddr, &value);
        SENSOR_TRACE("SENSOR: OV7690_ReadReg reg/value(%x,%x) !!\n", subaddr, value);
        return value;
}
LOCAL uint32_t OV7690_Identify(uint32_t param)
{
#define OV7690_PID_VALUE	0x76	
#define OV7690_PID_ADDR	0x0A
#define OV7690_VER_VALUE	0x73	
#define OV7690_VER_ADDR		0x0B	

        uint32_t i;
        uint8_t ret;
        uint32_t err_cnt = 0;
        uint8_t reg[2] 	= {0x0A, 0x0B};
        uint8_t value[2] 	= {0x76, 0x91};

        for(i = 0; i<2; ) {
                //nLoop = 1000;
                ret = OV7690_ReadReg(reg[i]);
                if( ret != value[i]) {
                        err_cnt++;
                        if(err_cnt>3) {
                                SENSOR_TRACE("Fail to OV7690_Identify: ret: %d, value[%d]: %d.\n", ret, i, value[i]);
                                return SENSOR_FAIL;
                        } else {
                                //Masked by frank.yang,DCAM_Sleep() will cause a  Assert when called in boot precedure
                                //DCAM_Sleep(10);
                                //while(nLoop--);
                                msleep(10);
                                continue;
                        }
                }
                err_cnt = 0;
                i++;
        }
        SENSOR_TRACE("OV7690_Identify: it is OV7690.\n");
        return 0;
}
#if 0
LOCAL uint32_t _OV7690_Power_On(uint32_t power_on)
{
    SENSOR_AVDD_VAL_E		dvdd_val=g_OV7690_yuv_info.dvdd_val;
    SENSOR_AVDD_VAL_E		avdd_val=g_OV7690_yuv_info.avdd_val;
    SENSOR_AVDD_VAL_E		iovdd_val=g_OV7690_yuv_info.iovdd_val;  
    BOOLEAN 				power_down=g_OV7690_yuv_info.power_down_level;	    
    BOOLEAN 				reset_level=g_OV7690_yuv_info.reset_pulse_level;
    uint32_t 				reset_width=g_OV7690_yuv_info.reset_pulse_width;	    
    
    if(DCAM_TRUE==power_on)
    {
        Sensor_SetVoltage(dvdd_val, avdd_val, iovdd_val);
        
        Sensor_PowerDown(!power_down);

        // Open Mclk in default frequency
        Sensor_SetMCLK(SENSOR_DEFALUT_MCLK);   
        
        DCAM_Sleep(20);
        Sensor_SetResetLevel(reset_level);
        DCAM_Sleep(reset_width);
        Sensor_SetResetLevel(!reset_level);
        DCAM_Sleep(100);
    }
    else
    {
        Sensor_PowerDown(power_down);

        Sensor_SetMCLK(SENSOR_DISABLE_MCLK);           

        Sensor_SetVoltage(SENSOR_AVDD_CLOSED, SENSOR_AVDD_CLOSED, SENSOR_AVDD_CLOSED);        
    }

    SENSOR_TRACE("SENSOR: _OV7690_Power_On(1:on, 0:off): %d", power_on);    
    
    return DCAM_SUCCESS;
}
#endif
LOCAL uint32_t set_OV7690_ae_enable(uint32_t enable)
{
#define OV7690_AE_ENABLE 0x13
        unsigned char ae_value;

        ae_value=OV7690_ReadReg(OV7690_AE_ENABLE);

        if(0x00==enable) {
                ae_value&=0xfa;
                OV7690_WriteReg(OV7690_AE_ENABLE,ae_value);
        } else if(0x01==enable) {
                ae_value|=0x05;
                OV7690_WriteReg(OV7690_AE_ENABLE,ae_value);
        }
        SENSOR_TRACE("set_ae_enable: enable = %d", enable);
        return 0;
}

LOCAL uint32_t set_OV7690_ae_awb_enable(uint32_t ae_enable, uint32_t awb_enable)
{
#define OV7690_AE_ENABLE 0x13
        unsigned char ae_value;

        ae_value=OV7690_ReadReg(OV7690_AE_ENABLE);

        if(0x00==ae_enable) {
                ae_value&=0xfa;
        } else if(0x01==ae_enable) {
                ae_value|=0x05;
        }

        if(0x00==awb_enable) {
                ae_value&=0xfd;
        } else if(0x01==awb_enable) {
                ae_value|=0x02;
        }        

        OV7690_WriteReg(OV7690_AE_ENABLE,ae_value);
        SENSOR_TRACE("set_ae_awb_enable: ae=%d awb=%d", ae_enable, awb_enable);
        return 0;
}
LOCAL uint32_t set_OV7690_hmirror_enable(uint32_t enable)
{
        SENSOR_TRACE("set_OV7690_hmirror_enable: enable = %d", enable);
        return 0;
}
LOCAL uint32_t set_OV7690_vmirror_enable(uint32_t enable)
{
        SENSOR_TRACE("set_OV7690_vmirror_enable: enable = %d", enable);
        return 0;
}
/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/
 const SENSOR_REG_T OV7690_brightness_tab[][4]=
{
        {{0xd3, 0x30},{0xd2, 0x04},{0xdc, 0x00},{0xff,0xff}},
        {{0xd3, 0x20},{0xd2, 0x04},{0xdc, 0x00},{0xff,0xff}},
        {{0xd3, 0x10},{0xd2, 0x04},{0xdc, 0x00},{0xff,0xff}},
        {{0xd3, 0x00},{0xd2, 0x04},{0xdc, 0x00},{0xff,0xff}},
        {{0xd3, 0x10},{0xd2, 0x04},{0xdc, 0x08},{0xff,0xff}},
        {{0xd3, 0x20},{0xd2, 0x04},{0xdc, 0x08},{0xff,0xff}},
        {{0xd3, 0x30},{0xd2, 0x04},{0xdc, 0x08},{0xff,0xff}},
};
LOCAL uint32_t OV7690_set_brightness(uint32_t level)
{
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV7690_brightness_tab[level];
        uint8_t temp = 0;

        if(level>6)
                return 0;
        SENSOR_TRACE("OV7690_set_brightness: level = %d", level);    
        temp = OV7690_ReadReg(0x0c);
        SENSOR_TRACE("OV7690_set_brightness: 0x0c = %d", temp); 
        temp = OV7690_ReadReg(0x81);
        temp |= 0x33;
        OV7690_WriteReg(0x81, temp);	
        switch (level) {
        case 6:
                OV7690_WriteReg(0xd3, 0x30);
                temp = OV7690_ReadReg(0xdc);
                temp &= 0xf7;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;        
        case 5:
                OV7690_WriteReg(0xd3, 0x20);
                temp = OV7690_ReadReg(0xdc);
                temp &= 0xf7;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;            
        case 4:
                OV7690_WriteReg(0xd3, 0x10);
                temp = OV7690_ReadReg(0xdc);
                temp &= 0xf7;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;            
        case 3 :
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;
        case 2:
                OV7690_WriteReg(0xd3, 0x10);
                temp = OV7690_ReadReg(0xdc);
                temp |= 0x08;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;
        case 1:
                OV7690_WriteReg(0xd3, 0x20);
                temp = OV7690_ReadReg(0xdc);
                temp |= 0x08;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;
        case 0:
                OV7690_WriteReg(0xd3, 0x30);
                temp = OV7690_ReadReg(0xdc);
                temp |= 0x08;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;
        default:
                break;
        }
        return 0;
}

 const SENSOR_REG_T OV7690_contrast_tab[][6]=
{
        {{0xd5, 0x20},{0xd4, 0x2c},{0xd3, 0x00},{0xdc, 0x00},{0xd2, 0x04}, {0xff,0xff}},
        {{0xd5, 0x20},{0xd4, 0x28},{0xd3, 0x00},{0xdc, 0x00},{0xd2, 0x04}, {0xff,0xff}},
        {{0xd5, 0x20},{0xd4, 0x24},{0xd3, 0x00},{0xdc, 0x00},{0xd2, 0x04}, {0xff,0xff}},
        {{0xd5, 0x20},{0xd4, 0x20},{0xd3, 0x00},{0xdc, 0x00},{0xd2, 0x04}, {0xff,0xff}},
        {{0xd5, 0x20},{0xd4, 0x1c},{0xd3, 0x20},{0xdc, 0x04},{0xd2, 0x04}, {0xff,0xff}},
        {{0xd5, 0x20},{0xd4, 0x18},{0xd3, 0x48},{0xdc, 0x04},{0xd2, 0x04}, {0xff,0xff}},
        {{0xd5, 0x20},{0xd4, 0x14},{0xd3, 0x80},{0xdc, 0x04},{0xd2, 0x04}, {0xff,0xff}},
};
LOCAL uint32_t OV7690_set_contrast(uint32_t level)
{
        uint8_t temp;
        SENSOR_REG_T* sensor_reg_ptr;
        sensor_reg_ptr = (SENSOR_REG_T*)OV7690_contrast_tab[level];

        if(level>6)
        return 0;

        SENSOR_TRACE("OV7690_set_contrast: level = %d", level);

        temp = OV7690_ReadReg(0x81);
        temp |= 0x33;
        OV7690_WriteReg(0x81, temp);
        //TIANYUDRV:ALEX_MA add for TBG6313
        switch (level) {
        case 6:
                OV7690_WriteReg(0xd5, 0x20);
                OV7690_WriteReg(0xd4, 0x30);
                OV7690_WriteReg(0xd3, 0x00);
                temp = OV7690_ReadReg(0xdc);
                temp &= 0xfb;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;
        case 5:
                OV7690_WriteReg(0xd5, 0x20);
                OV7690_WriteReg(0xd4, 0x28);
                OV7690_WriteReg(0xd3, 0x00);
                temp = OV7690_ReadReg(0xdc);
                temp &= 0xfb;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;
        case 4:
                OV7690_WriteReg(0xd5, 0x20);
                OV7690_WriteReg(0xd4, 0x24);
                OV7690_WriteReg(0xd3, 0x00);
                temp = OV7690_ReadReg(0xdc);
                temp &= 0xfb;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;
        case 3:   
                OV7690_WriteReg(0xd5, 0x20);
                OV7690_WriteReg(0xd4, 0x20);
                OV7690_WriteReg(0xd3, 0x00);            
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);            
                temp = OV7690_ReadReg(0xdc);
                temp &= 0xfb;
                OV7690_WriteReg(0xdc, temp);
                break;
        case 2:
                OV7690_WriteReg(0xd5, 0x20);
                OV7690_WriteReg(0xd4, 0x1c);
                OV7690_WriteReg(0xd3, 0x20);
                temp = OV7690_ReadReg(0xdc);
                temp |= 0x04;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;        
        case 1:
                OV7690_WriteReg(0xd5, 0x20);
                OV7690_WriteReg(0xd4, 0x18);
                OV7690_WriteReg(0xd3, 0x48);
                temp = OV7690_ReadReg(0xdc);
                temp |= 0x04;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;        
        case 0:
                OV7690_WriteReg(0xd5, 0x20);
                OV7690_WriteReg(0xd4, 0x10);
                OV7690_WriteReg(0xd3, 0xd0);
                temp = OV7690_ReadReg(0xdc);
                temp |= 0x04;
                OV7690_WriteReg(0xdc, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x04;
                OV7690_WriteReg(0xd2, temp);
                break;        
        default:
                break;
        }
        return 0;
}
LOCAL uint32_t OV7690_set_sharpness(uint32_t level)
{	
        return 0;
}
LOCAL uint32_t OV7690_set_saturation(uint32_t level)
{	
        return 0;
}
/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/
LOCAL uint32_t set_OV7690_preview_mode(uint32_t preview_mode)
{
        SENSOR_TRACE("set_OV7690_preview_mode: preview_mode = %d", preview_mode);
        s_preview_mode = preview_mode;

        switch (preview_mode) {
        case DCAMERA_ENVIRONMENT_NORMAL: 
                OV7690_set_work_mode(0);
                break;
        case DCAMERA_ENVIRONMENT_NIGHT:
                OV7690_set_work_mode(1);
                break;
        case DCAMERA_ENVIRONMENT_SUNNY:
                OV7690_set_work_mode(0);
                break;
        default:
                break;
        }
        DCAM_Sleep(250);
        return 0;
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
 const SENSOR_REG_T OV7690_image_effect_tab[][5]=
{
        // effect normal
        {
        {0x28, 0x00},{0xD2, 0x00},{0xda, 0x80},{0xdb, 0x80},{0xff, 0xff}
        },
        //effect BLACKWHITE
        {
        {0x28, 0x00},{0xD2, 0x18},{0xda, 0x80},{0xdb, 0x80},{0xff, 0xff}
        },
        // effect RED
        {
        {0x28, 0x00},{0xD2, 0x18},{0xda, 0x80},{0xdb, 0xc0},{0xff, 0xff}
        },
        // effect GREEN
        {
        {0x28, 0x00},{0xD2, 0x18},{0xda, 0x60},{0xdb, 0x60},{0xff, 0xff}
        },
        // effect  BLUE
        {
        {0x28, 0x00},{0xD2, 0x18},{0xda, 0xa0},{0xdb, 0x40},{0xff, 0xff}
        },
        // effect YELLOW
        {
        {0x28, 0x00},{0xD2, 0x18},{0xda, 0x50},{0xdb, 0x90},{0xff, 0xff}//?  
        },
        // effect NEGATIVE
        {	     
        {0x28, 0x80},{0xD2, 0x00},{0xda, 0x80},{0xdb, 0x80},{0xff, 0xff}
        },
        //effect ANTIQUE
        {
        {0x28, 0x00},{0xD2, 0x18},{0xda, 0x40},{0xdb, 0xa0},{0xff, 0xff} 
        },
};
LOCAL uint32_t OV7690_set_image_effect(uint32_t effect_type)
{
        uint8_t temp;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV7690_image_effect_tab[effect_type];

        switch (effect_type) {
        case 7:   //effect ANTIQUE
                temp = OV7690_ReadReg(0x81);
                temp |= 0x20;
                OV7690_WriteReg(0x81, temp);
                temp = OV7690_ReadReg(0x28);
                temp &= 0x7f;
                OV7690_WriteReg(0x28, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x18;
                OV7690_WriteReg(0xd2, temp);
                OV7690_WriteReg(0xda, 0x40);
                OV7690_WriteReg(0xdb, 0xa0);            
                break;
        case 6:    // effect NEGATIVE
                temp = OV7690_ReadReg(0x81);
                temp |= 0x20;
                OV7690_WriteReg(0x81, temp);
                temp = OV7690_ReadReg(0x28);
                temp |= 0x80;
                OV7690_WriteReg(0x28, temp);
                temp = OV7690_ReadReg(0xd2);
                temp &= 0xe7;
                OV7690_WriteReg(0xd2, temp);
                OV7690_WriteReg(0xda, 0x80);
                OV7690_WriteReg(0xdb, 0x80);
                break;
        case 5:    // effect YELLOW
                temp = OV7690_ReadReg(0x81);
                temp |= 0x20;
                OV7690_WriteReg(0x81, temp);
                temp = OV7690_ReadReg(0x28);
                temp &= 0x7f;
                OV7690_WriteReg(0x28, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x18;
                OV7690_WriteReg(0xd2, temp);
                OV7690_WriteReg(0xda, 0x50);
                OV7690_WriteReg(0xdb, 0x90);
                break;
        case 4:   // effect  BLUE
                temp = OV7690_ReadReg(0x81);
                temp |= 0x20;
                OV7690_WriteReg(0x81, temp);
                temp = OV7690_ReadReg(0x28);
                temp &= 0x7f;
                OV7690_WriteReg(0x28, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x18;
                OV7690_WriteReg(0xd2, temp);
                OV7690_WriteReg(0xda, 0xa0);
                OV7690_WriteReg(0xdb, 0x40);
                break;
        case 3:    // effect GREEN
                temp = OV7690_ReadReg(0x81);
                temp |= 0x20;
                OV7690_WriteReg(0x81, temp);
                temp = OV7690_ReadReg(0x28);
                temp &= 0x7f;
                OV7690_WriteReg(0x28, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x18;
                OV7690_WriteReg(0xd2, temp);
                OV7690_WriteReg(0xda, 0x60);
                OV7690_WriteReg(0xdb, 0x60);
                break;
        case 2:  // effect RED
                temp = OV7690_ReadReg(0x81);
                temp |= 0x20;
                OV7690_WriteReg(0x81, temp);
                temp = OV7690_ReadReg(0x28);
                temp &= 0x7f;
                OV7690_WriteReg(0x28, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x18;
                OV7690_WriteReg(0xd2, temp);
                OV7690_WriteReg(0xda, 0x80);
                OV7690_WriteReg(0xdb, 0xc0);
                break;   
        case 1:    //effect BLACKWHITE
                temp = OV7690_ReadReg(0x81);
                temp |= 0x20;
                OV7690_WriteReg(0x81, temp);
                temp = OV7690_ReadReg(0x28);
                temp &= 0x7f;
                OV7690_WriteReg(0x28, temp);
                temp = OV7690_ReadReg(0xd2);
                temp |= 0x18;
                OV7690_WriteReg(0xd2, temp);
                OV7690_WriteReg(0xda, 0x80);
                OV7690_WriteReg(0xdb, 0x80);
                break;
        case 0:   	// effect normal
                temp = OV7690_ReadReg(0x81);
                temp &= 0xdf;
                OV7690_WriteReg(0x81, temp);             
                temp = OV7690_ReadReg(0x28);
                temp &= 0x7f;
                OV7690_WriteReg(0x28, temp);            
                temp = OV7690_ReadReg(0xd2);
                temp &= 0xe7;
                OV7690_WriteReg(0xd2, temp);            
                OV7690_WriteReg(0xda, 0x80);
                OV7690_WriteReg(0xdb, 0x80);
                break;
        default:
                break;
        }	
        SENSOR_TRACE("OV7690_set_image_effect: effect_type = %d", effect_type);
        return 0;
}

LOCAL uint32_t OV7690_After_Snapshot(uint32_t param)
{// Tim.zhu@20080520 for cr116612 modify the switch preview to capture
#define PLL_ADDR    0x11

        SENSOR_TRACE("OV7690_BeforeSnapshot: OV7690_After_Snapshot ");  
        set_OV7690_ae_awb_enable(0x01, 0x01);
        OV7690_WriteReg(PLL_ADDR, 0x01); 
        OV7690_WriteReg(0x2a, 0x30);
        DCAM_Sleep(200); // wait 2 frame the sensor working normal if no delay the lum is incorrect
        return 0;    
}

LOCAL uint32_t OV7690_BeforeSnapshot(uint32_t param)
{// Tim.zhu@20080514 for crcr115898 modify the switch preview to capture
#if 0// xujun delete 080724
#define GAIN_ADDR   0x00
#define EXPOSAL_ADDR1   0x0f
#define EXPOSAL_ADDR0    0x10
#define PLL_ADDR    0x11
#define DUMMY_LINE0     0x2d
#define DUMMY_LINE1     0x2e

    uint8_t temp=0x00;
    uint8_t gain=0x00;
    uint16_t exposal=0x00;
    uint16_t dummy_line=0x00;

    SENSOR_TRACE("OV7690_BeforeSnapshot: OV7690_BeforeSnapshot ");  

	if(DCAMERA_ENVIRONMENT_MAX == s_preview_mode)
		return 0;
    set_OV7690_ae_awb_enable(0x00, 0x00);

    gain=OV7690_ReadReg(GAIN_ADDR);

    temp=OV7690_ReadReg(EXPOSAL_ADDR0);
    exposal=(temp);
    temp=OV7690_ReadReg(EXPOSAL_ADDR1);
    exposal|=(temp<<8);

    temp=OV7690_ReadReg(DUMMY_LINE0);
    dummy_line|=temp;
    temp=OV7690_ReadReg(DUMMY_LINE1); 
    dummy_line|=(temp<<0x08); 

    exposal+=dummy_line;  

#if 1
	OV7690_WriteReg(PLL_ADDR, 0x02);
	exposal=exposal*2/3;
	//OV7690_WriteReg(PLL_ADDR, 0x03);
	//exposal=exposal>>0x01; //03
#else
    OV7690_WriteReg(PLL_ADDR, 0x01);
#endif

    if(0x80==(gain&0x80))
    {
        gain&=0x7f;
        exposal*=0x02;
    }

    if(0x40==(gain&0x40))
    {
        gain&=0xbf;
        exposal*=0x02;
    }

    if(0x20==(gain&0x20))
    {
        gain&=0xdf;
        exposal*=0x02;
    }

    if(0x10==(gain&0x10))
    {
        gain&=0xef;
        exposal*=0x02;
    } 

    if(exposal>508)
    {
        dummy_line+=(exposal-508);
        exposal=508;
    }    


   //This is for 15fps setting , any adjust frame rate is base on this. 	
//    OV7690_WriteReg(0x2b, 0x34);
//    OV7690_WriteReg(0x2b, 0x34+dummy_pixel_value);


    OV7690_WriteReg(GAIN_ADDR, gain);

    temp=exposal&0Xff;
    OV7690_WriteReg(EXPOSAL_ADDR0, temp);

     temp=(exposal>>0x08)&0xff;
    OV7690_WriteReg(EXPOSAL_ADDR1, temp);
  
    temp=dummy_line&0xff;
    OV7690_WriteReg(DUMMY_LINE0, temp);  

    temp=(dummy_line>>0x08)&0xff;
    OV7690_WriteReg(DUMMY_LINE1, temp); 
    
    if(DCAMERA_ENVIRONMENT_NORMAL == s_preview_mode)
    {
    	     DCAM_Sleep(200); // wait 2 frame the sensor working normal if no delay the lum is incorrect
    }
    else if(DCAMERA_ENVIRONMENT_NIGHT == s_preview_mode)
    {
    	    DCAM_Sleep(400); // wait 2 frame the sensor working normal if no delay the lum is incorrect
    }
    else
    {
          DCAM_Sleep(400); // wait 2 frame the sensor working normal if no delay the lum is incorrect

    }
#endif
    return 0;
    
}
LOCAL uint32_t OV7690_read_ev_value(uint32_t value)
{
        return 0;
}
LOCAL uint32_t OV7690_write_ev_value(uint32_t exposure_value)
{	
        return 0;	
}
LOCAL uint32_t OV7690_read_gain_value(uint32_t value)
{
        return 0;
}
LOCAL uint32_t OV7690_write_gain_value(uint32_t gain_value)
{	
        return 0;
}
LOCAL uint32_t OV7690_read_gain_scale(uint32_t value)
{
        return SENSOR_GAIN_SCALE;	
}
LOCAL uint32_t OV7690_set_frame_rate(uint32_t param)
{	
        return 0;
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		mode 0:normal;	 1:night 
/******************************************************************************/
 const SENSOR_REG_T OV7690_mode_tab[][4]=
{
        {{0x15, 0x94}, {0x2d, 0x00}, {0x2e, 0x00}, {0xff,0xff}}, // normal
        {{0x15, 0xb4}, {0xff,0xff},{0x00, 0x00}, {0x00, 0x00}} // night
};
LOCAL uint32_t OV7690_set_work_mode(uint32_t mode)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV7690_mode_tab[mode];
        SENSOR_ASSERT(mode <= 1);
        SENSOR_ASSERT(PNULL != sensor_reg_ptr);

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_TRACE("OV7690_set_work_mode: mode = %d", mode);
        return 0;
}
const SENSOR_REG_T OV7690_WB_mode_tab[][5]=
{
        //AUTO
        { 
        {0x13,0xf7},
        {0x2d,0x00},
        {0x2e,0x00},
        {0xff,0xff}, 
        {0xff,0xff}
        },

        //INCANDESCENCE
        { 
        {0x13,0xf5},
        {0x01,0x67},
        {0x02,0x40},
        {0xff,0xff},
        {0xff,0xff} 
        },

        //U30  not used
        { 
        {0x13,0xf5},
        {0x01,0x60},
        {0x02,0x60},
        {0xff,0xff},
        {0xff,0xff} 
        },

        //CWF  not used
        { 
        {0x13,0xf5},
        {0x01,0xa0},
        {0x02,0x40},
        {0xff,0xff},
        {0xff,0xff} 
        },

        //FLUORESCENT
        { 
        {0x13,0xf5},
        {0x01,0x58},
        {0x02,0x60},
        {0xff,0xff},
        {0xff,0xff} 
        },

        //SUN
        { 
        {0x13,0xf5},
        {0x01,0x58},
        {0x02,0x56},
        {0xff,0xff},
        {0xff,0xff} 
        },

        //CLOUD
        { 
        {0x13,0xf5},
        {0x01,0x5c},
        {0x02,0x56},
        {0xff,0xff},
        {0xff,0xff} 
        },
};
LOCAL uint32_t OV7690_set_whitebalance_mode(uint32_t mode )
{
        uint16_t i;
        uint16_t temp;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)OV7690_WB_mode_tab[mode];

        if(mode>6)
                return 0;

        temp = Sensor_ReadReg( 0x13 );
        if (mode < 1 ) {
                Sensor_WriteReg( 0x13, temp |BIT_1 );//	Auto mode : enable auto  white balanec
        } else {
                Sensor_WriteReg( 0x13, temp &( ~BIT_1) );//	other mode : disable auto white balanec
        }	
        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_TRACE("OV7690_set_whitebalance_mode: mode = %d", mode);
        return 0;
}
/******************************************************************************/
// Description: set video mode
// Global resource dependence: 
// Author:
// Note:
//		 
/******************************************************************************/
 const SENSOR_REG_T ov7690_video_mode_nand_tab[][9]=
{
        // normal mode 15fps
        {
        {0x11, 0x00},
        {0x29, 0x50},
        {0x2a, 0x30},
        {0x2b, 0x34},
        {0x2c, 0x00},
        {0x15, 0x00},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff}
        },  
        {// 30fps
        {0x11, 0x00},
        {0x29, 0x50},
        {0x2a, 0x30},
        {0x2b, 0x34},
        {0x2c, 0x00},
        {0x15, 0x00},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff}
        }
};
/******************************************************************************/
// Description: set video mode
// Global resource dependence: 
// Author:
// Note:
//		 
/******************************************************************************/
 const SENSOR_REG_T ov7690_video_mode_nor_tab[][9]=
{
    // normal mode 15fps
        {
        {0x11, 0x00},
        {0x29, 0x50},
        {0x2a, 0x30},
        {0x2b, 0x34},
        {0x2c, 0x00},
        {0x15, 0x00},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff}
    },

#ifdef CHIP_DRV_SC6600L
    // video mode
    // 30 fps (for 1 page 10fps, for brust 15 fps)
    {
        {0x11, 0x00},
        {0x29, 0x50},
        {0x2a, 0x30},
        {0x2b, 0x34},
        {0x2c, 0x00},
        {0x15, 0x00},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff}
    }
    #else
    // video mode 20 fps
    {
        {0x11, 0x00},
        {0x29, 0x50},
        {0x2a, 0x30},
        {0x2b, 0x34},
        {0x2c, 0x80},
        {0x15, 0x00},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff}
    }    
   #endif
};   
LOCAL uint32_t set_ov7690_video_mode(uint32_t mode)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = PNULL;

        SENSOR_ASSERT(mode <= 1);
        if(1 == mode)
                s_preview_mode = DCAMERA_ENVIRONMENT_MAX;
        sensor_reg_ptr = (SENSOR_REG_T*)ov7690_video_mode_nand_tab[mode];
        SENSOR_ASSERT(PNULL != sensor_reg_ptr);
        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        DCAM_Sleep(150);
        SENSOR_TRACE("SENSOR: set_ov7690_video_mode: mode = %d", mode);
        return 0;
}
 const SENSOR_REG_T ov7690_ev_tab[][4]=
{
        {{0x24, 0x30}, {0x25, 0x20}, {0x26, 0xa2}, {0xff, 0xff}},
        {{0x24, 0x40}, {0x25, 0x30}, {0x26, 0xb2}, {0xff, 0xff}},            
        {{0x24, 0x50}, {0x25, 0x40}, {0x26, 0xc2}, {0xff, 0xff}},            
        {{0x24, 0x58}, {0x25, 0x48}, {0x26, 0x73}, {0xff, 0xff}},
        {{0x24, 0x70}, {0x25, 0x60}, {0x26, 0xc2}, {0xff, 0xff}},            
        {{0x24, 0x80}, {0x25, 0x70}, {0x26, 0xd4}, {0xff, 0xff}},            
        {{0x24, 0x90}, {0x25, 0x80}, {0x26, 0xd5}, {0xff, 0xff}}      
};

LOCAL uint32_t set_ov7690_ev(uint32_t level)
{
        uint16_t i;    
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)ov7690_ev_tab[level];

        if(level>6)
                return 0;

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value) ; i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_TRACE("SENSOR: set_ov7690_ev: level = %d", level);
        return 0;
}
struct sensor_drv_cfg sensor_ov7690 = {
        .sensor_pos = CONFIG_DCAM_SENSOR_POS_OV7690,
        .sensor_name = "ov7690",
        .driver_info = &g_OV7690_yuv_info,
};

static int __init sensor_ov7690_init(void)
{
        return dcam_register_sensor_drv(&sensor_ov7690);
}

subsys_initcall(sensor_ov7690_init);
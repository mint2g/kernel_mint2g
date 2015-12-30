/******************************************************************************
 ** Copyright (c) 
 ** File Name:		sensor_hi253.c 										  *
 ** Author: 													  *
 ** DATE:															  *
 ** Description:   This file contains driver for sensor HI253. 
 ** Spreadtrum_8800s+Hynix_HI253_V1.2														 
 ******************************************************************************

 ******************************************************************************
 ** 					   Edit History 									  *
 ** ------------------------------------------------------------------------- *
 ** DATE		   NAME 			DESCRIPTION 							  *
 ** 2012-04-05	  
 ******************************************************************************/

/**---------------------------------------------------------------------------*
 ** 						Dependencies									  *
 **---------------------------------------------------------------------------*/
//#include "sensor_cfg.h"
//#include "sensor_drv.h"
//#include "i2c_drv.h"
//#include "os_api.h"
//#include "chip.h"
//#include "dal_dcamera.h"
//#include <linux/delay.h>
//#include "sensor.h"
//#include <mach/common.h>
//#include <mach/sensor_drv.h>

#include "common/sensor.h"
#include "common/sensor_drv.h"
#include "common/jpeg_exif_header_k.h"
#include "sensor_hi253_regs.h"
// for AT+CMD Begin
#include <linux/err.h>
#include <linux/delay.h>
// for AT+CMD End


/**---------------------------------------------------------------------------*
 ** 						Compiler Flag									  *
 **---------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------*
 ** 						Const variables 								  *
 **---------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------*
 ** 						   Macro Define
 **---------------------------------------------------------------------------*/
#define HI253_I2C_ADDR_W		         0x20//0x40
#define HI253_I2C_ADDR_R			0x20//0x41

#define HI253_NIGHT_MODE_NORMAL          0
#define HI253_NIGHT_MODE_DARK               1
//#define HI253_WRITE_DELAY		          0x04

//#define SENSOR_GAIN_SCALE		16

//LOCAL uint32 s_preview_mode;

// for AT+CMD Begin
struct class *camera_class;
// for AT+CMD End

//#define CONFIG_LOAD_FILE

/**---------------------------------------------------------------------------*
 ** 					Local Function Prototypes							  *
 **---------------------------------------------------------------------------*/

LOCAL uint32_t HI253_InitExifInfo(void);
//LOCAL uint32_t set_hi253_ae_awb_enable(uint32_t ae_enable, uint32_t awb_enable);
LOCAL uint32_t set_hi253_ae_enable(uint32_t enable);
//LOCAL uint32_t set_hi253_hmirror_enable(uint32_t enable);
//LOCAL uint32_t set_hi253_vmirror_enable(uint32_t enable);
LOCAL uint32_t HI253_set_scene_mode(uint32_t preview_mode);
LOCAL uint32_t _hi253_Power_On(uint32_t power_on);
LOCAL uint32_t HI253_Identify(uint32_t param);
LOCAL uint32_t HI253_GetExpValue();
LOCAL uint32_t HI253_GetISOValue();
LOCAL uint32_t HI253_GetNightMode();
LOCAL uint32_t HI253_Before_Snapshot(uint32_t param);  
LOCAL uint32_t HI253_After_Snapshot(uint32_t param);
LOCAL uint32_t HI253_set_brightness(uint32_t level);
LOCAL uint32_t HI253_set_contrast(uint32_t level);
//LOCAL uint32_t HI253_set_sharpness(uint32_t level);
//LOCAL uint32_t HI253_set_saturation(uint32_t level);
LOCAL uint32_t HI253_set_image_effect(uint32_t effect_type);
//LOCAL uint32_t HI253_read_ev_value(uint32_t value);
//LOCAL uint32_t HI253_write_ev_value(uint32_t exposure_value);
//LOCAL uint32_t HI253_read_gain_value(uint32_t value);
//LOCAL uint32_t HI253_write_gain_value(uint32_t gain_value);
//LOCAL uint32_t HI253_read_gain_scale(uint32_t value);
//LOCAL uint32_t HI253_set_frame_rate(uint32_t param);
LOCAL uint32_t HI253_set_whitebalance_mode(uint32_t mode);
LOCAL uint32_t HI253_set_DTP(uint32_t dtp_mode);
LOCAL uint32_t HI253_set_video_mode(uint32_t mode);
LOCAL uint32_t HI253_set_exposure_value(uint32_t level);
LOCAL uint32_t _hi253_GetResolutionTrimTab(uint32_t param);
LOCAL uint32_t HI253_GetExifInfo(uint32_t param);
LOCAL uint32_t HI253_set_Metering(uint32_t metering_mode);
/*modify begin tianxiaohui 2012-03-30*/
LOCAL uint32_t HI253_flash(uint32_t param); //tianxiaohui
static uint32_t  g_flash_mode_en = 0; //tianxiaohui
/*modify end tianxiaohui 2012-03-30*/

/*add wenfeng.yan for esd test*/
LOCAL uint32_t HI253_GetEsd(uint32_t param);
/*esd end*/

LOCAL void    HI253_Write_Group_Regs( SENSOR_REG_T* sensor_reg_ptr );
LOCAL SENSOR_TRIM_T s_HI253_Resolution_Trim_Tab[]=
{	
		// COMMON INIT
		{0, 0, 0, 0, 0, 64},
		
		// YUV422 PREVIEW 1	
		{0, 0, 0, 0, 0, 64},
		{0, 0, 0, 0, 0, 64},
		
		{0, 0, 0, 0, 0, 64},
		{0, 0, 0, 0, 0, 64},
		
		// YUV422 PREVIEW 2 
		{0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0}
};

LOCAL SENSOR_REG_TAB_INFO_T s_hi253_resolution_Tab_YUV[]=
{	
    // COMMON INIT
    {ADDR_AND_LEN_OF_ARRAY(HI253_sensor_ini),640,480,24,SENSOR_IMAGE_FORMAT_YUV422},
    {ADDR_AND_LEN_OF_ARRAY(HI253_camera_mode),640,480,24,SENSOR_IMAGE_FORMAT_YUV422},

    
    // YUV422 PREVIEW 1
    {ADDR_AND_LEN_OF_ARRAY(HI253_YUV_640X480),640,480,24,SENSOR_IMAGE_FORMAT_YUV422},
    {ADDR_AND_LEN_OF_ARRAY(HI253_YUV_1280X960), 1280, 960, 24, SENSOR_IMAGE_FORMAT_YUV422},
    {ADDR_AND_LEN_OF_ARRAY(HI253_YUV_1600X960), 1600, 960, 24, SENSOR_IMAGE_FORMAT_YUV422},
    {ADDR_AND_LEN_OF_ARRAY(HI253_YUV_1600X1200), 1600, 1200,24,SENSOR_IMAGE_FORMAT_YUV422},
    //{PNULL,	0, 0,	0, 0, 0},

	{ADDR_AND_LEN_OF_ARRAY(HI253_video_mode_nand_tab),640,480,24,SENSOR_IMAGE_FORMAT_YUV422},
    // YUV422 PREVIEW 2 
    {PNULL, 0, 0, 0, 0, 0},
    {PNULL, 0, 0, 0, 0, 0},
    {PNULL, 0, 0, 0, 0, 0}

};


LOCAL EXIF_SPEC_PIC_TAKING_COND_T s_hi253_exif={0};

LOCAL SENSOR_IOCTL_FUNC_TAB_T s_hi253_ioctl_func_tab =
{
    // Internal IOCTL function
    PNULL,//0. reset
    PNULL,//1. power,
    PNULL,//2. enter_sleep
    HI253_Identify,//3. identify
    PNULL,//4. write_reg
    PNULL,//5. read_reg

    // Custom function
    PNULL,//6. cus_func_1
    PNULL,//7. get_trim, _hi253_GetResolutionTrimTab,

    // External IOCTL function
    set_hi253_ae_enable,//8. ae_enable
    PNULL,//9. hmirror_enable,
    PNULL,//10. vmirror_enable,
    HI253_set_brightness,//11. set_brightness
    HI253_set_contrast,//12. set_contrast
    PNULL,//13. set_sharpness,
    PNULL,//14. set_saturation,
    HI253_set_scene_mode,//15. set_preview_mode	
    HI253_set_image_effect,//16. set_image_effect    
    HI253_Before_Snapshot,//17. before_snapshort
    HI253_After_Snapshot,//18. after_snapshort    
    HI253_flash, //19. flash
    PNULL,//20. read_ae_value,
    PNULL,//21. write_ae_value,
    PNULL,//22. read_gain_value,
    PNULL,//23. write_gain_value,
    PNULL,//24. read_gain_scale,
    PNULL,//25. set_frame_rate,
    PNULL,//26. af_enable
    PNULL,//27. af_get_status
    HI253_set_whitebalance_mode,//28. set_wb_mode
    HI253_set_DTP,//29. set_DTP
    PNULL,//30. get_skip_frame
    PNULL,  //31.set_iso
    HI253_set_exposure_value, //32. set_exposure_compensation
    PNULL, //33. check_image_format_support
    PNULL, //34. change_image_format
    PNULL, //35. set_zoom

    // CUSTOMER FUNCTION	                      
    HI253_GetExifInfo,//36. get_exif
    PNULL,	//37. set_focus
    PNULL,	//38. set_anti_banding_flicker
    PNULL,	//39. set_video_mode
    PNULL,   //40.pick_jpeg_stream  pick out the jpeg stream from given buffer
    HI253_set_Metering, //41. set_meter_mode
    /*add wenfeng.yan for esd test*/
    HI253_GetEsd,// get esd
    /*esd end*/
};

LOCAL SENSOR_EXTEND_INFO_T hi253_ext_info = {
	
    256,    //jpeg_seq_width
    0//1938       //jpeg_seq_height
};

SENSOR_INFO_T g_hi253_yuv_info =
{
	HI253_I2C_ADDR_W,				// salve i2c write address
	HI253_I2C_ADDR_R, 				// salve i2c read address
		
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
	100,								// reset pulse width(ms)
		
	SENSOR_HIGH_LEVEL_PWDN,			// 1: high level valid; 0: low level valid
		
	2,								// count of identify code
	{{0x04, 0x92},						// supply two code to identify sensor.
	{0x04, 0x92}},						// for Example: index = 0-> Device id, index = 1 -> version id	
										
	SENSOR_AVDD_2800MV,				// voltage of avdd	
	
	1600,							// max width of source image
	1200,							// max height of source image
	"HI253",						// name of sensor												
	
	SENSOR_IMAGE_FORMAT_YUV422,		// define in SENSOR_IMAGE_FORMAT_E enum,
										// if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T
	SENSOR_IMAGE_PATTERN_YUV422_YUYV,	// pattern of input image form sensor;			
	
	s_hi253_resolution_Tab_YUV,	// point to resolution table information structure
	&s_hi253_ioctl_func_tab,		// point to ioctl function table
				
	PNULL,							// information and table about Rawrgb sensor
	&hi253_ext_info,				// extend information about sensor	
	SENSOR_AVDD_1800MV,//SENSOR_AVDD_2800MV,                     // iovdd
	SENSOR_AVDD_1800MV,                      // dvdd
	0,                     // skip frame num before preview 
	0,                      // skip frame num before capture        
      0,                      // deci frame num during preview    
      0,                      // deci frame num during video preview
      0,                     // threshold enable(only analog TV)  
      0,                    // threshold mode 0 fix mode 1 auto mode      
      0,                     // threshold start postion
      0,                     // threshold end postion       
      0

};

/**---------------------------------------------------------------------------*
 ** 							Function  Definitions
 **---------------------------------------------------------------------------*/
LOCAL void HI253_WriteReg( uint8_t  subaddr, uint8_t data )
{
		Sensor_WriteReg_8bits(subaddr,data);
}

LOCAL uint8_t HI253_ReadReg( uint8_t  subaddr)
{
		uint8_t value = 0;
		value = Sensor_ReadReg(subaddr);
		return value;
}

LOCAL uint32_t HI253_Identify(uint32_t param)
{
		uint32_t i;
		uint32_t nLoop;
		uint8_t ret;
		uint32_t err_cnt = 0;
		uint8_t reg[2] = {0x04, 0x04};
		uint8_t value[2] = {0x92, 0x92};

		SENSOR_PRINT("HI253_Identify \n");
		for(i = 0; i<2; )
		{
			nLoop = 1000;
			ret = HI253_ReadReg(reg[i]);
			if( ret != value[i])
			{
				err_cnt++;
				if(err_cnt>3)
				{
					SENSOR_PRINT_ERR("It is not HI253 \n");
					return SENSOR_FAIL;
				}
				else
				{
					//Masked by frank.yang,SENSOR_Sleep() will cause a  Assert when called in boot precedure
					//SENSOR_Sleep(10);
					while(nLoop--);
					continue;
				}
			}
			err_cnt = 0;
			i++;
		}
		SENSOR_PRINT("HI253 identify: It is HI253\n");

		HI253_InitExifInfo();

		return SENSOR_SUCCESS;

}

/*add wenfeng.yan for esd test*/
LOCAL uint32_t HI253_GetEsd(uint32_t param)
{
	SENSOR_PRINT("SENSOR: HI253_GetEsd \n");
	 Sensor_WriteReg(0x03, 0x00);
	 if((Sensor_ReadReg(0x0B) == 0xAA)
	  &&(Sensor_ReadReg(0x0c) == 0xAA)
	  &&(Sensor_ReadReg(0x0d) == 0xAA))
	 {
	  return SENSOR_SUCCESS;
	 }
	 else
	 {
	  return SENSOR_FAIL;
	 }	
}
/*esd end*/

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: xxiao.chen
// Note:
/******************************************************************************/
LOCAL uint32_t HI253_GetExpValue()
{
	uint32_t data1 = 0, data2 = 0, data3 = 0;
	uint32_t exp_time = 0, cal_value_1 =0, cal_value_2 = 0;
	SENSOR_PRINT("SENSOR: HI253_GetExpValue \n");

	/* exposure time */
	EXIF_SPEC_PIC_TAKING_COND_T* exif_ptr=&s_hi253_exif;
    uint32_t Exptime;
    Sensor_WriteReg(0x03, 0x20);
//    Sensor_WriteReg(0x10, 0x0c); // AE off	// Not be necessary
	data1 = (uint32_t)Sensor_ReadReg(0x80);
	data2 = (uint32_t)Sensor_ReadReg(0x81);
	data3 = (uint32_t)Sensor_ReadReg(0x82);

	exp_time = (data3 << 3) + (data2 << 11) + (data1 << 19);

	if(exp_time == 0)
	{
		exif_ptr->valid.ExposureTime = 0;
	}
	else
	{
		cal_value_1 = 24000000/exp_time;

		if(cal_value_1 >= 8)
			cal_value_2 = 0;
		else{
			cal_value_2 = 24000000%exp_time;

			if( cal_value_2 >= (exp_time/2) )
				cal_value_2 = 1;
			else
				cal_value_2 = 0;
		}

		Exptime = cal_value_1 + cal_value_2;

		exif_ptr->valid.ExposureTime = 1;
		exif_ptr->ExposureTime.denominator = Exptime;
		exif_ptr->ExposureTime.numerator = 1;

		exif_ptr->valid.ShutterSpeedValue = 1;

		if(exp_time >= 400*1000*30)
				exif_ptr->ShutterSpeedValue.numerator = -5;
		else if(exp_time >= 6000000)
				exif_ptr->ShutterSpeedValue.numerator = -4;
		else if(exp_time >= 3200000)
				exif_ptr->ShutterSpeedValue.numerator = -3;
		else if(exp_time >= 1600000)
				exif_ptr->ShutterSpeedValue.numerator = -2;
		else if(exp_time >= 800000)
				exif_ptr->ShutterSpeedValue.numerator = -1;
		else if(exp_time >= 400000)
				exif_ptr->ShutterSpeedValue.numerator =  0;
		else if(exp_time >= 200000)
				exif_ptr->ShutterSpeedValue.numerator =  1;
		else if(exp_time >= 100000)
				exif_ptr->ShutterSpeedValue.numerator =  2;
		else if(exp_time >= 50000)
				exif_ptr->ShutterSpeedValue.numerator =  3;
		else if(exp_time >= 26666)
				exif_ptr->ShutterSpeedValue.numerator =  4;
		else if(exp_time >= 13333)
				exif_ptr->ShutterSpeedValue.numerator =  5;
		else if(exp_time >= 6666)
				exif_ptr->ShutterSpeedValue.numerator =  6;
		else if(exp_time >= 3200)
				exif_ptr->ShutterSpeedValue.numerator =  7;
		else if(exp_time >= 1600)
				exif_ptr->ShutterSpeedValue.numerator =  8;
		else if(exp_time >= 800)
				exif_ptr->ShutterSpeedValue.numerator =  9;
		else if(exp_time >= 400)
				exif_ptr->ShutterSpeedValue.numerator = 10;
		else if(exp_time >= 200)
				exif_ptr->ShutterSpeedValue.numerator = 11;
		exif_ptr->ShutterSpeedValue.denominator = 1;
	}
	
#if 0
    Exptime = (Sensor_ReadReg(0x80)) << 19 | (Sensor_ReadReg(0x81)) << 11 | (Sensor_ReadReg(0x82)) << 3;
	if (0 == Exptime) {
		exif_ptr->valid.ExposureTime = 0;
	}else{
		exif_ptr->valid.ExposureTime = 1;
		exif_ptr->ExposureTime.denominator = 24000000 / Exptime;
		exif_ptr->ExposureTime.numerator = 1;

		exif_ptr->valid.ShutterSpeedValue = 1;	
		if(Exptime >= 400*1000*30)
				exif_ptr->ShutterSpeedValue.numerator = -5;
		else if(Exptime >= 6000000)
				exif_ptr->ShutterSpeedValue.numerator = -4;
		else if(Exptime >= 3200000)
				exif_ptr->ShutterSpeedValue.numerator = -3;
		else if(Exptime >= 1600000)
				exif_ptr->ShutterSpeedValue.numerator = -2;
		else if(Exptime >= 800000)
				exif_ptr->ShutterSpeedValue.numerator = -1;
		else if(Exptime >= 400000)
				exif_ptr->ShutterSpeedValue.numerator =  0;
		else if(Exptime >= 200000)
				exif_ptr->ShutterSpeedValue.numerator =  1;
		else if(Exptime >= 100000)
				exif_ptr->ShutterSpeedValue.numerator =  2;
		else if(Exptime >= 50000)
				exif_ptr->ShutterSpeedValue.numerator =  3;
		else if(Exptime >= 26666)
				exif_ptr->ShutterSpeedValue.numerator =  4;
		else if(Exptime >= 13333)
				exif_ptr->ShutterSpeedValue.numerator =  5;
		else if(Exptime >= 6666)
				exif_ptr->ShutterSpeedValue.numerator =  6;
		else if(Exptime >= 3200)
				exif_ptr->ShutterSpeedValue.numerator =  7;
		else if(Exptime >= 1600)
				exif_ptr->ShutterSpeedValue.numerator =  8;
		else if(Exptime >= 800)
				exif_ptr->ShutterSpeedValue.numerator =  9;
		else if(Exptime >= 400)
				exif_ptr->ShutterSpeedValue.numerator = 10;
		else if(Exptime >= 200)
				exif_ptr->ShutterSpeedValue.numerator = 11;
		exif_ptr->ShutterSpeedValue.denominator = 1;
	}
#endif
	SENSOR_PRINT("SENSOR: HI253_GetExpValue Exptime = %d \n", Exptime);
    return SENSOR_SUCCESS;
}
/******************************************************************************/
// Description:get ISO
// Global resource dependence: 
// Author: xxiao.chen
// Note:
/******************************************************************************/
LOCAL uint32_t HI253_GetISOValue()
{
	SENSOR_PRINT("SENSOR: HI253_GetISOValue \n");
	
	EXIF_SPEC_PIC_TAKING_COND_T* exif_ptr=&s_hi253_exif;
    uint16_t regData;
	uint16_t gainValue;
	uint16_t isoValue = 50;
    Sensor_WriteReg(0x03, 0x20);

	regData = Sensor_ReadReg(0xb0);
//	gainValue = (float)((float)regData / 32 ) + 0.5f;
	gainValue = (regData*50) + 800;

#if 1
	if(gainValue < 1824)
		isoValue = 50;
	else if(gainValue < 3424)
		isoValue = 100;
	else if(gainValue < 4224)
		isoValue = 200;
	else if(gainValue < 12032)
		isoValue = 400;
	else
		isoValue = 800;
#else
	if(gainValue < 1.14)
		isoValue = 50;
	else if ( gainValue < 2.14 )
		isoValue = 100;
	else if ( gainValue < 2.64 )
		isoValue = 200;
	else if ( gainValue < 7.52 )
		isoValue = 400;
	else
		isoValue = 800;
#endif
	exif_ptr->valid.ISOSpeedRatings = 1;
	exif_ptr->ISOSpeedRatings = isoValue;
#if 0
    regData = Sensor_ReadReg(0x80);
	gainValue = 50*regData + 800;
	SENSOR_PRINT("SENSOR: HI253_GetISOValue:gainValue %d \n",gainValue);
	
	exif_ptr->valid.ISOSpeedRatings = 1;
	exif_ptr->ISOSpeedRatings.type = EXIF_SHORT;
	exif_ptr->ISOSpeedRatings.count = 1;
	
	if(gainValue < 1824)
		isoValue = 50;
	else if(gainValue < 3424)
		isoValue = 100;
	else if(gainValue < 4224)
		isoValue = 200;
	else if(gainValue < 12032)
		isoValue = 400;
	else
		isoValue = 800;
	exif_ptr->ISOSpeedRatings.ptr[0] = isoValue;
#endif
	SENSOR_PRINT("SENSOR: HI253_GetISOValue iso = %d \n", isoValue);
    return SENSOR_SUCCESS;

}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: xxiao.chen
// Note:
/******************************************************************************/
LOCAL uint32_t HI253_GetNightMode()
{
	

    uint32_t Exptime, Expmax; // modify from uint16 to uint32 because of overflow
    Sensor_WriteReg(0x03, 0x00); 	
    Sensor_WriteReg(0x11, 0x90);
    Sensor_WriteReg(0x03, 0x20);
    Sensor_WriteReg(0x10, 0x0c); // AE off
    Exptime = (Sensor_ReadReg(0x80)) << 16 | (Sensor_ReadReg(0x81)) << 8 | (Sensor_ReadReg(0x82));
    Expmax = (Sensor_ReadReg(0x88)) << 16 | (Sensor_ReadReg(0x89)) << 8 | (Sensor_ReadReg(0x8A));
    if ( Exptime < Expmax )// Normal condition
    {
        SENSOR_PRINT("SENSOR: HI253_GetNightMode  HI253_NIGHT_MODE_NORMAL\n");
        return HI253_NIGHT_MODE_NORMAL;
    }
    else //dark condition
    {
        SENSOR_PRINT("SENSOR: HI253_GetNightMode  HI253_NIGHT_MODE_DARK\n");
        return HI253_NIGHT_MODE_DARK;
    }
    return HI253_NIGHT_MODE_NORMAL;
	
}

LOCAL uint32_t HI253_GetExifInfo(uint32_t param)
{
    SENSOR_PRINT("SENSOR: HI253_GetExifInfo \n");
	return (uint32_t) & s_hi253_exif;
}
LOCAL uint32_t HI253_InitExifInfo(void)
{
#if 1
	EXIF_SPEC_PIC_TAKING_COND_T *exif_ptr = &s_hi253_exif;

	memset(&s_hi253_exif, 0, sizeof(EXIF_SPEC_PIC_TAKING_COND_T));

	SENSOR_PRINT("SENSOR: HI253_InitExifInfo \n");

	exif_ptr->valid.FNumber = 1;
	exif_ptr->FNumber.numerator = 280;
	exif_ptr->FNumber.denominator = 100;

	exif_ptr->valid.ExposureProgram = 1;
	exif_ptr->ExposureProgram = 0x04;

	//exif_ptr->SpectralSensitivity[MAX_ASCII_STR_SIZE];
	exif_ptr->valid.ISOSpeedRatings = 1;
//	exif_ptr->ISOSpeedRatings.type = EXIF_SHORT;
//	exif_ptr->ISOSpeedRatings.count = 1;
	exif_ptr->ISOSpeedRatings = 50;
	//exif_ptr->OECF;

	//exif_ptr->ShutterSpeedValue;

	exif_ptr->valid.ApertureValue = 1;
	exif_ptr->ApertureValue.numerator = 280;
	exif_ptr->ApertureValue.denominator = 100;

	//exif_ptr->BrightnessValue;
	//exif_ptr->ExposureBiasValue;

	exif_ptr->valid.MaxApertureValue = 1;
	exif_ptr->MaxApertureValue.numerator = 297;
	exif_ptr->MaxApertureValue.denominator = 100;

	//exif_ptr->SubjectDistance;
	uint8_t meteringMode = Sensor_GetSensorParamByKey(SENSOR_PARAM_METERING);
	exif_ptr->valid.MeteringMode = 1;
	switch(meteringMode)
    {
        case 0:
			exif_ptr->MeteringMode = 2;
            break;
        case 1:
			exif_ptr->MeteringMode = 1;
			break;
        case 2:
			exif_ptr->MeteringMode = 3;
            break;
		default:
			exif_ptr->MeteringMode = 2;
			break;

    }
	SENSOR_PRINT("MeteringMode = %d \n", exif_ptr->MeteringMode);
	//exif_ptr->Flash;

	exif_ptr->valid.FocalLength = 1;
	exif_ptr->FocalLength.numerator = 270;
	exif_ptr->FocalLength.denominator = 100;

	//exif_ptr->SubjectArea;
	//exif_ptr->FlashEnergy;
	//exif_ptr->SpatialFrequencyResponse;
	//exif_ptr->FocalPlaneXResolution;
	//exif_ptr->FocalPlaneYResolution;
	//exif_ptr->FocalPlaneResolutionUnit;
	//exif_ptr->SubjectLocation[2];
	//exif_ptr->ExposureIndex;
	//exif_ptr->SensingMethod;

	exif_ptr->valid.FileSource = 1;
	exif_ptr->FileSource = 0x03;

	exif_ptr->valid.SceneType = 1;
	exif_ptr->SceneType = 1;	
	//exif_ptr->CFAPattern;
	//exif_ptr->CustomRendered;

	exif_ptr->valid.ExposureMode = 1;
	exif_ptr->ExposureMode = 0x00;

	uint8_t whiteBalance = Sensor_GetSensorParamByKey(SENSOR_PARAM_WB);
	exif_ptr->valid.LightSource = 1;
	exif_ptr->valid.WhiteBalance = 1;
	switch(whiteBalance)
    {
        case 0:
			exif_ptr->LightSource = 0x00;
			exif_ptr->WhiteBalance = 0;
            break;
        case 1:
			exif_ptr->LightSource = 0x03;
			exif_ptr->WhiteBalance = 1;
			break;
        case 4:
			exif_ptr->LightSource = 0x03;
			exif_ptr->WhiteBalance = 1;
            break;
        case 5:
			exif_ptr->LightSource = 0x01;
			exif_ptr->WhiteBalance = 1;
            break;
        case 6:
			exif_ptr->LightSource = 0x0a;
			exif_ptr->WhiteBalance = 1;
            break;
		default:
			exif_ptr->LightSource = 0x00;
			exif_ptr->WhiteBalance = 1;
			break;

    }
	SENSOR_PRINT("WhiteBalance = %d \n", exif_ptr->WhiteBalance);

	//exif_ptr->DigitalZoomRatio;
	//exif_ptr->FocalLengthIn35mmFilm;
	exif_ptr->valid.SceneCaptureType = 1;
	uint8_t scenemode = Sensor_GetSensorParamByKey(SENSOR_PARAM_SCENEMODE);
	switch(scenemode)
    {
        case 1:
			exif_ptr->SceneCaptureType = 1;
            break;
        case 8:
			exif_ptr->SceneCaptureType = 3;
			break;
		default:
			exif_ptr->SceneCaptureType = 0;
			break;

    }
	//exif_ptr->GainControl;
	//exif_ptr->Contrast;
	//exif_ptr->Saturation;
	//exif_ptr->Sharpness;
	//exif_ptr->DeviceSettingDescription;
	//exif_ptr->SubjectDistanceRange;
#endif
	return SENSOR_SUCCESS;
}

/******************************************************************************/
// Description: get ov7670 rssolution trim tab
// Global resource dependence: 
// Author:
// Note:
/******************************************************************************/
LOCAL uint32_t _hi253_GetResolutionTrimTab(uint32_t param)
{
  	 return (uint32_t)s_HI253_Resolution_Trim_Tab;
}
LOCAL uint32_t set_hi253_ae_enable(uint32_t enable)
{
    SENSOR_PRINT("HI253_test set_hi253_ae_enable: enable = %d \n", enable);
#if 1    
    if(0x00==enable)
    {
        HI253_WriteReg(0x03,0x20);
        HI253_WriteReg(0x10,0x1c);// AE Off
    }
    else if(0x01==enable)
    {
        HI253_WriteReg(0x03, 0x20);//page 3
        HI253_WriteReg(0x10, 0x9c);//ae on
    }
#endif
    return 0;
}


LOCAL void HI253_Write_Group_Regs( SENSOR_REG_T* sensor_reg_ptr )
{
    SENSOR_PRINT("HI253_test HI253_Write_Group_Regs \n");
    uint32_t i;

    for(i = 0;  ; i++)
    {
       if((sensor_reg_ptr[i].reg_addr == 0xFF) && (sensor_reg_ptr[i].reg_value != 0xFF)) {
	     SENSOR_Sleep(10*sensor_reg_ptr[i].reg_value);
       }else if((sensor_reg_ptr[i].reg_addr == 0xFF) && (sensor_reg_ptr[i].reg_value == 0xFF)) {
            break;
        }else
            Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
    }

}

LOCAL uint32_t HI253_set_brightness(uint32_t level)
{
    SENSOR_PRINT("HI253_test HI253_set_brightness: level = %d \n", level);

    //not app menu use ev 
    return SENSOR_OP_SUCCESS;
    
    if((level > 8) || (level < 0))
        return SENSOR_OP_SUCCESS;

#ifdef CONFIG_LOAD_FILE
   
    switch(level)
    {
        case 0:
            Sensor_regs_table_write("HI253_brightness_m4");
            break;
        case 1:
            Sensor_regs_table_write("HI253_brightness_m3");
            break;
        case 2:
            Sensor_regs_table_write("HI253_brightness_m2");
            break;
        case 3:
           Sensor_regs_table_write("HI253_brightness_m1");
            break;
        case 4:
            Sensor_regs_table_write("HI253_brightness_0");
            break;
        case 5:
            Sensor_regs_table_write("HI253_brightness_p1");
            break;
        case 6:
            Sensor_regs_table_write("HI253_brightness_p2");
            break;
        case 7:
            Sensor_regs_table_write("HI253_brightness_p3");
            break;
        case 8:
            Sensor_regs_table_write("HI253_brightness_p4");
            break;
    }
#else      


    SENSOR_REG_T* sensor_reg_ptr = NULL; 

    switch(level)
    {
        case 0:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_m4;
            break;
        case 1:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_m3;
            break;
        case 2:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_m2;
            break;
        case 3:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_m1;
            break;
        case 4:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_0;
            break;
        case 5:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_p1;
            break;
        case 6:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_p2;
            break;
        case 7:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_p3;
            break;
        case 8:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_p4;
            break;
    }

    SENSOR_ASSERT(PNULL != sensor_reg_ptr);
    HI253_Write_Group_Regs(sensor_reg_ptr);
#endif  
    
    return SENSOR_OP_SUCCESS;
}

LOCAL uint32_t HI253_set_contrast(uint32_t level)
{
    SENSOR_PRINT("HI253_test HI253_set_contrast: level = %d \n", level);

    if((level > 6) || (level < 0))
            return SENSOR_OP_SUCCESS;
#ifdef CONFIG_LOAD_FILE
    switch(level)
    {
        case 0:
            Sensor_regs_table_write("HI253_contrast_m3");
            break;
        case 1:
            Sensor_regs_table_write("HI253_contrast_m2");
            break;
        case 2:
            Sensor_regs_table_write("HI253_contrast_m1");
            break;
        case 3:
           Sensor_regs_table_write("HI253_contrast_0");
            break;
        case 4:
            Sensor_regs_table_write("HI253_contrast_p1");
            break;
        case 5:
            Sensor_regs_table_write("HI253_contrast_p2");
            break;
        case 6:
            Sensor_regs_table_write("HI253_contrast_p3");
            break;
    }
#else   

    SENSOR_REG_T* sensor_reg_ptr = NULL;
    switch(level)
    {
        case 0:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_contrast_m3;
            break;
        case 1:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_contrast_m2;
            break;
        case 2:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_contrast_m1;
            break;
        case 3:
           sensor_reg_ptr = (SENSOR_REG_T*)HI253_contrast_0;
            break;
        case 4:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_contrast_p1;
            break;
        case 5:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_contrast_p2;
            break;
        case 6:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_contrast_p3;
            break;
    }
    SENSOR_ASSERT(PNULL != sensor_reg_ptr);

    HI253_Write_Group_Regs(sensor_reg_ptr);
#endif
    return SENSOR_OP_SUCCESS;
}

/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/
LOCAL uint32_t HI253_set_scene_mode(uint32_t  scene_mode)
{
    SENSOR_PRINT("HI253_test HI253_set_scene_mode: scene_mode = %d \n", scene_mode);
    if((scene_mode > 8) || (scene_mode < 0))
        return SENSOR_OP_PARAM_ERR;
	EXIF_SPEC_PIC_TAKING_COND_T* exif_ptr=&s_hi253_exif;
	exif_ptr->valid.SceneCaptureType = 1;
	exif_ptr->SceneCaptureType = 0;
#ifdef CONFIG_LOAD_FILE
    switch(scene_mode)
    {
        case 0:
            Sensor_regs_table_write("HI253_scene_mode_none");
            break;
        case 1:
            Sensor_regs_table_write("HI253_scene_mode_landscape");
			exif_ptr->SceneCaptureType = 1; // 1 = Landscape
            break;
        case 2:
            Sensor_regs_table_write("HI253_scene_mode_party");
            break;
        case 3:
           Sensor_regs_table_write("HI253_scene_mode_sunset");
            break;
        case 4:
            Sensor_regs_table_write("HI253_scene_mode_dawn");
            break;
        case 5:
            Sensor_regs_table_write("HI253_scene_mode_autumncolor");
            break;
        case 6:
            Sensor_regs_table_write("HI253_scene_mode_candlelight");
            break;
        case 7:
            Sensor_regs_table_write("HI253_scene_mode_backlight");
            break;
        case 8:
            {
                if(HI253_GetNightMode() == HI253_NIGHT_MODE_DARK)
                    Sensor_regs_table_write("HI253_scene_mode_night_dark");
                else
                    Sensor_regs_table_write("HI253_scene_mode_night_normal");
				exif_ptr->SceneCaptureType = 3;//Night scene
            }
            break;
    }
    
#else      
        
    SENSOR_REG_T* sensor_reg_ptr = NULL;
    switch(scene_mode)
    {
        case 0:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_scene_mode_none;
            break;
        case 1:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_scene_mode_landscape;
			exif_ptr->SceneCaptureType = 1; // 1 = Landscape
            break;
        case 2:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_scene_mode_party;
            break;
        case 3:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_scene_mode_sunset;
            break;
        case 4:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_scene_mode_dawn;
            break;
        case 5:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_scene_mode_autumncolor;
            break;
        case 6:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_scene_mode_candlelight;
            break;
        case 7:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_scene_mode_backlight;
            break;
        case 8:
            {
                if(HI253_GetNightMode() == HI253_NIGHT_MODE_DARK)
                    sensor_reg_ptr = (SENSOR_REG_T*)HI253_scene_mode_night_dark;
                else
                    sensor_reg_ptr = (SENSOR_REG_T*)HI253_scene_mode_night_normal;
				exif_ptr->SceneCaptureType = 3;//Night scene
            }
            break;
    }
    SENSOR_ASSERT(PNULL != sensor_reg_ptr);

    HI253_Write_Group_Regs(sensor_reg_ptr);
#endif
    return SENSOR_OP_SUCCESS;
}


LOCAL uint32_t HI253_set_image_effect(uint32_t effect_type)
{
    SENSOR_PRINT("HI253_test HI253_set_image_effect: effect_type = %d \n", effect_type);
    if((effect_type > 7) || (effect_type < 0))
        return SENSOR_OP_SUCCESS;
    
#ifdef CONFIG_LOAD_FILE
    switch(effect_type)
    {
        case 0:
            Sensor_regs_table_write("HI253_image_effect_default");
            break;
        case 1:
            Sensor_regs_table_write("HI253_image_effect_gray");
            break;
        case 6:
            Sensor_regs_table_write("HI253_image_effect_colorinv");
            break;
        case 7:
            Sensor_regs_table_write("HI253_image_effect_sepia");
            break;
        default:
            return SENSOR_OP_SUCCESS;
    }
#else    
    SENSOR_REG_T* sensor_reg_ptr = NULL;
    switch(effect_type)
    {
        case 0:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_image_effect_default;
            break;
        case 1:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_image_effect_gray;
            break;
        case 6:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_image_effect_colorinv;
            break;
        case 7:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_image_effect_sepia;
            break;
        default:
            return SENSOR_OP_SUCCESS;
    }    
    

    SENSOR_ASSERT(PNULL != sensor_reg_ptr);

    HI253_Write_Group_Regs(sensor_reg_ptr);
#endif
    return SENSOR_OP_SUCCESS;
}


LOCAL uint32_t HI253_After_Snapshot (uint32_t param)
{
	SENSOR_PRINT("HI253_test HI253_After_Snapshot param = %d \n", param);

	HI253_GetExpValue();	
	HI253_GetISOValue();
	return 0;    
}

LOCAL uint32_t HI253_Before_Snapshot(uint32_t param)
{    
	SENSOR_PRINT("HI253_test sensor:HI253_Before_Snapshot param = %d \n", param); 
#if 0
    /*modify begin tianxiaohui 2012-03-30*/
    if(g_flash_mode_en) //tianxiaohui
    {
    	//Sensor_SetFlash(0);
    	Sensor_SetFlash(1);
    	g_flash_mode_en = 0;
    }
    /*modify end tianxiaohui 2012-03-30*/

    SENSOR_PRINT("HI253_test sensor:HI253_Before_Snapshot param = %d \n", param); 

    if(SENSOR_MODE_SNAPSHOT_ONE_SECOND == param)
    {   
    	Sensor_SetMode(param);         
    } 
    else if(SENSOR_MODE_SNAPSHOT_ONE_FIRST == param)
    {  
    	Sensor_SetMode(param);
    }	
    else
    {
    	SENSOR_PRINT("HI253_test HI253_Before_Snapshot:dont set any \n");
    }
    msleep(200);
#endif
    return SENSOR_OP_SUCCESS;
}

LOCAL uint32_t HI253_set_whitebalance_mode(uint32_t mode )
{
    SENSOR_PRINT("HI253_test sensor:HI253_set_whitebalance_mode: mode = %d \n", mode);
    if((mode > 6) || (mode < 0))
        return SENSOR_OP_SUCCESS;
    if((mode == 2) || (mode == 3)) // not set
        return SENSOR_OP_SUCCESS;
	EXIF_SPEC_PIC_TAKING_COND_T* exif_ptr=&s_hi253_exif;
	exif_ptr->valid.LightSource = 1;
	exif_ptr->valid.WhiteBalance = 1;
#ifdef CONFIG_LOAD_FILE
    switch(mode)
    {
        case 0:
            Sensor_regs_table_write("HI253_WB_mode_Auto");
			exif_ptr->LightSource = 0x00;
			exif_ptr->WhiteBalance = 0;
            break;
        case 1:
            Sensor_regs_table_write("HI253_WB_mode_Incandescence");
			exif_ptr->LightSource = 0x03;
			exif_ptr->WhiteBalance = 1;
            break;
        case 4:
            Sensor_regs_table_write("HI253_WB_mode_Fluorescent");
			exif_ptr->LightSource = 0x03;
			exif_ptr->WhiteBalance = 1;
            break;
        case 5:
			Sensor_regs_table_write("HI253_WB_mode_Sun");
			exif_ptr->LightSource = 0x01;
			exif_ptr->WhiteBalance = 1;
            break;
        case 6:
            Sensor_regs_table_write("HI253_WB_mode_Cloud");
			exif_ptr->LightSource = 0x0a;
			exif_ptr->WhiteBalance = 1;
            break;
    }
#else
    SENSOR_REG_T* sensor_reg_ptr = NULL;

    switch(mode)
    {
        case 0:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_WB_mode_Auto;
			exif_ptr->LightSource = 0x00;
			exif_ptr->WhiteBalance = 0;
            break;
        case 1:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_WB_mode_Incandescence;
			exif_ptr->LightSource = 0x03;
			exif_ptr->WhiteBalance = 1;
            break;
        case 4:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_WB_mode_Fluorescent;
			exif_ptr->LightSource = 0x03;
			exif_ptr->WhiteBalance = 1;
            break;
        case 5:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_WB_mode_Sun;
			exif_ptr->LightSource = 0x01;
			exif_ptr->WhiteBalance = 1;
            break;
        case 6:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_WB_mode_Cloud;
			exif_ptr->LightSource = 0x0a;
			exif_ptr->WhiteBalance = 1;
            break;
    }

    SENSOR_ASSERT(PNULL != sensor_reg_ptr);

    HI253_Write_Group_Regs(sensor_reg_ptr);

#endif
	return SENSOR_OP_SUCCESS;
}


/******************************************************************************/
// Description: HI253_set_DTP
// Global resource dependence: 
// Author:
// Note:
//		
//******************************************************************************/
LOCAL uint32_t HI253_set_DTP(uint32_t dtp_mode)
{
       SENSOR_PRINT("HI253_test HI253_set_DTP mode is : %d \n",  dtp_mode); 
       if((dtp_mode > 1) || (dtp_mode < 0))
		return SENSOR_OP_SUCCESS;
       
#ifdef CONFIG_LOAD_FILE
    switch(dtp_mode)
    {
        case 0:
            Sensor_regs_table_write("HI253_dtp_off");
            break;
        case 1:
            Sensor_regs_table_write("HI253_dtp_on");
            break;
    }
#else     
        SENSOR_REG_T* sensor_reg_ptr = NULL;

        switch(dtp_mode)
        {
            case 0:
                sensor_reg_ptr = (SENSOR_REG_T*)HI253_dtp_off;
                break;
            case 1:
                sensor_reg_ptr = (SENSOR_REG_T*)HI253_dtp_on;
                break;
        }
        SENSOR_ASSERT(PNULL != sensor_reg_ptr);
        HI253_Write_Group_Regs(sensor_reg_ptr);	
#endif	
	return SENSOR_OP_SUCCESS;
}
/******************************************************************************/
// Description: HI253_set_DTP
// Global resource dependence: 
// Author:
// Note:
//		
//******************************************************************************/
LOCAL uint32_t HI253_set_Metering(uint32_t metering_mode)
{
       SENSOR_PRINT("HI253_test HI253_set_Metering metering_mode is : %d \n",  metering_mode); 
       if((metering_mode > 2) || (metering_mode < 0))
		return SENSOR_OP_SUCCESS;
	   EXIF_SPEC_PIC_TAKING_COND_T* exif_ptr=&s_hi253_exif;
	   exif_ptr->valid.MeteringMode = 1;
#ifdef CONFIG_LOAD_FILE

    switch(metering_mode)
    {
        case 0:
            Sensor_regs_table_write("HI253_metering_mode_centerweighted");
			exif_ptr->MeteringMode = 2;
            break;
        case 1:
            Sensor_regs_table_write("HI253_metering_mode_matrix");
			exif_ptr->MeteringMode = 1;
            break;
        case 2:
            Sensor_regs_table_write("HI253_metering_mode_spot");
			exif_ptr->MeteringMode = 3;
            break;
    }
#else
        SENSOR_REG_T* sensor_reg_ptr = NULL;

        switch(metering_mode)
        {
            case 0:
                sensor_reg_ptr = (SENSOR_REG_T*)HI253_metering_mode_centerweighted;
				exif_ptr->MeteringMode = 2;
                break;
            case 1:
                sensor_reg_ptr = (SENSOR_REG_T*)HI253_metering_mode_matrix;
				exif_ptr->MeteringMode = 1;
                break;
            case 2:
                sensor_reg_ptr = (SENSOR_REG_T*)HI253_metering_mode_spot;
				exif_ptr->MeteringMode = 3;
                break;
        }
        SENSOR_ASSERT(PNULL != sensor_reg_ptr);
        HI253_Write_Group_Regs(sensor_reg_ptr);
#endif
	return SENSOR_OP_SUCCESS;
}




LOCAL uint32_t HI253_set_video_mode(uint32_t mode)
{
    SENSOR_PRINT("HI253_test SENSOR: HI253_set_video_mode: mode = %d \n", mode);
    return 0;
}

/*
    exposure value
*/
LOCAL uint32_t HI253_set_exposure_value(uint32_t level)
{
    SENSOR_PRINT("HI253_test HI253_set_exposure_value level is : %d \n",  level); 
    /*
        Now EV is used now on Mint model. 
        it should be changed from EV setting to Brightness setting.

    */
     if((level > 8) || (level < 0))
        return SENSOR_OP_PARAM_ERR;
     
#ifdef CONFIG_LOAD_FILE
    switch(level)
    {
        case 0:
            Sensor_regs_table_write("HI253_brightness_m4");
            break;
        case 1:
            Sensor_regs_table_write("HI253_brightness_m3");
            break;
        case 2:
            Sensor_regs_table_write("HI253_brightness_m2");
            break;
        case 3:
            Sensor_regs_table_write("HI253_brightness_m1");
            break;
        case 4:
            Sensor_regs_table_write("HI253_brightness_0");
            break;
        case 5:
            Sensor_regs_table_write("HI253_brightness_p1");
            break;
        case 6:
            Sensor_regs_table_write("HI253_brightness_p2");
            break;   
        case 7:
            Sensor_regs_table_write("HI253_brightness_p3");
            break;               
        case 8:
            Sensor_regs_table_write("HI253_brightness_p4");
            break; 
    }
   
#else         
    SENSOR_REG_T* sensor_reg_ptr = NULL;

    switch(level)
    {
        case 0:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_m4;
            break;
        case 1:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_m3;
            break;
        case 2:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_m2;
            break;
        case 3:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_m1;
            break;
        case 4:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_0;
            break;
        case 5:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_p1;
            break;
        case 6:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_p2;
            break;   
        case 7:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_p3;
            break;               
        case 8:
            sensor_reg_ptr = (SENSOR_REG_T*)HI253_brightness_p4;
            break; 
    }

    SENSOR_ASSERT(PNULL != sensor_reg_ptr);

    HI253_Write_Group_Regs(sensor_reg_ptr);
#endif 
     return SENSOR_OP_SUCCESS;
   
}

/*modify begin tianxiaohui 2012-03-30*/
LOCAL uint32_t HI253_flash(uint32_t param)
{
    SENSOR_PRINT("HI253_test HI253_set_exposure_value HI253_flash is : %d \n",  param); 
     /* enable flash, disable in HI253_After_Snapshot */
    g_flash_mode_en = param;

    Sensor_SetFlash(param);
}
/*modify end tianxiaohui 2012-03-30*/
struct sensor_drv_cfg sensor_hi253 = {
        .sensor_pos = CONFIG_DCAM_SENSOR_POS_HI253,
        .sensor_name = "hi253",
        .driver_info = &g_hi253_yuv_info,
};

// for AT+CMD Begin
static ssize_t Rear_Cam_Sensor_ID(struct device *dev, struct device_attribute *attr, char *buf)  
{ 
	char type[] = "SR200PC20M";
	
	return  sprintf(buf, "%s\n",type);
}

static ssize_t Rear_Cam_rear_camtype(struct device *dev,struct device_attribute *attr, char *buf)
{
    char type[] = "SR200PC20M_FIMC_IS";
 
    return sprintf(buf, "%s\n", type);
}

static DEVICE_ATTR(rear_camtype, S_IWUSR|S_IWGRP|S_IROTH,Rear_Cam_rear_camtype, NULL);
//wf.zhao marked | S_IWOTH | S_IXOTH, other just have read permission
static DEVICE_ATTR(rear_camfw, S_IRUGO | S_IWUSR/* | S_IWOTH | S_IXOTH*/, Rear_Cam_Sensor_ID, NULL);
// for AT+CMD End

static int __init sensor_hi253_init(void)
{
	// for AT+CMD Begin
	struct device *dev_t;

	camera_class = class_create(THIS_MODULE, "camera");
	if (IS_ERR(camera_class))
	{
		SENSOR_PRINT_ERR("Failed to create camera_class!\n");
		return PTR_ERR( camera_class );
	}
	dev_t = device_create(camera_class, NULL, 0, "%s", "rear");

	if (device_create_file(dev_t, &dev_attr_rear_camtype) < 0) 
		SENSOR_PRINT_ERR("failed to create device file, %s\n",dev_attr_rear_camtype.attr.name);
	if (device_create_file(dev_t, &dev_attr_rear_camfw) < 0)
		 SENSOR_PRINT_ERR("Failed to create device file(%s)!\n", dev_attr_rear_camfw.attr.name);
	// for AT+CMD End

        return dcam_register_sensor_drv(&sensor_hi253);
}

subsys_initcall(sensor_hi253_init);


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
#ifndef _GT2005_C_
#define _GT2005_C_
#include "common/sensor.h"
/**---------------------------------------------------------------------------*
 **      Extern Function Declaration         *
 **---------------------------------------------------------------------------*/
//extern uint32_t OS_TickDelay(uint32_t ticks);
                                           
/**---------------------------------------------------------------------------*
 **                         Const variables                                   *
 **---------------------------------------------------------------------------*/
#define SENSOR_TRACE SENSOR_PRINT
#define GT2005_I2C_ADDR_W    			0x3C //0x78 -> 0x78 >> 1
#define GT2005_I2C_ADDR_R    			0x3C //0x79 -> 0x79 >> 1
#define GT2005_I2C_ACK					0x0
#define SENSOR_WRITE_DELAY			0xffff
/**---------------------------------------------------------------------------*
 **                     Local Function Prototypes                             *
 **---------------------------------------------------------------------------*/
//LOCAL uint32_t GT2005_Power_On(uint32_t power_on);
LOCAL uint32_t GT2005_Identify(uint32_t param);
//LOCAL void    GT2005_WriteReg( uint16_t  subaddr, uint8_t data );
//LOCAL uint8_t 	GT2005_ReadReg( uint16_t  subaddr);
LOCAL uint32_t Set_GT2005_Brightness(uint32_t level);
LOCAL uint32_t Set_GT2005_Contrast(uint32_t level);
LOCAL uint32_t Set_GT2005_Image_Effect(uint32_t effect_type);
LOCAL uint32_t Set_GT2005_Ev(uint32_t level);
LOCAL uint32_t Set_GT2005_Anti_Flicker(uint32_t mode);
LOCAL uint32_t Set_GT2005_Preview_Mode(uint32_t preview_mode);
LOCAL uint32_t Set_GT2005_AWB(uint32_t mode);
//LOCAL uint32_t GT2005_AE_Enable(uint32_t ae_enable, uint32_t awb_enable);
LOCAL uint32_t GT2005_Before_Snapshot(uint32_t sensor_snapshot_mode);
LOCAL void    GT2005_Set_Shutter(void);
LOCAL uint32_t GT2005_After_Snapshot(uint32_t para);
LOCAL uint32_t Set_GT2005_Video_Mode(uint32_t mode);
LOCAL void    GT2005_Write_Group_Regs( SENSOR_REG_T* sensor_reg_ptr );
LOCAL uint32_t GT2005_Change_Image_Format(uint32_t param);
//LOCAL uint32_t GT2005_Zoom(uint32_t level);
/**---------------------------------------------------------------------------*
 ** 						Local Variables 								 *
 **---------------------------------------------------------------------------*/
 typedef enum
{
        FLICKER_50HZ = 0,
        FLICKER_60HZ,
        FLICKER_MAX
}FLICKER_E;

SENSOR_REG_T GT2005_YUV_COMMON[]=
{
	{0x0101 , 0x00},
	{0x0103 , 0x00},

	{0x0105 , 0x00},
	{0x0106 , 0xF0},
	{0x0107 , 0x00},
	{0x0108 , 0x1C},
	
	{0x0109 , 0x01},
	{0x010A , 0x00},
	{0x010B , 0x00},
	{0x010C , 0x00},
	{0x010D , 0x08},
	{0x010E , 0x00},
	{0x010F , 0x08},
	{0x0110 , 0x06},
	{0x0111 , 0x40},
	{0x0112 , 0x04},
	{0x0113 , 0xB0},

	{0x0114 , 0x04},
	
	{0x0115 , 0x00},
	
	{0x0116 , 0x01},
	{0x0117 , 0x00},
	{0x0118 , 0x34},
	{0x0119 , 0x01},
	{0x011A , 0x04},
	{0x011B , 0x01},
	
	{0x011C , 0x00},
	
	{0x011D , 0x01},
	{0x011E , 0x00},
	
	{0x011F , 0x00},
	{0x0120 , 0x1C},
	{0x0121 , 0x00},
	{0x0122 , 0x04},
	{0x0123 , 0x00},
	{0x0124 , 0x00},
	{0x0125 , 0x00},
	{0x0126 , 0x00},
	{0x0127 , 0x00},
	{0x0128 , 0x00},
	
	{0x0200 , 0x00},
	
	{0x0201 , 0x00},

	{0x0202 , 0x40},
	
	{0x0203 , 0x00},
	{0x0204 , 0x03},
	{0x0205 , 0x1F},
	{0x0206 , 0x0B},
	{0x0207 , 0x20},
	{0x0208 , 0x00},
	{0x0209 , 0x2A},
	{0x020A , 0x01},
	
	{0x020B , 0x48},
	{0x020C , 0x64},

	{0x020D , 0xC8},
	{0x020E , 0xBC},
	{0x020F , 0x08},
	{0x0210 , 0xD6},
	{0x0211 , 0x00},
	{0x0212 , 0x20},
	{0x0213 , 0x81},
	{0x0214 , 0x15},
	{0x0215 , 0x00},
	{0x0216 , 0x00},
	{0x0217 , 0x00},
	{0x0218 , 0x46},
	{0x0219 , 0x30},
	{0x021A , 0x03},
	{0x021B , 0x28},
	{0x021C , 0x02},
	{0x021D , 0x60},
	{0x021E , 0x00},
	{0x021F , 0x00},
	{0x0220 , 0x08},
	{0x0221 , 0x08},
	{0x0222 , 0x04},
	{0x0223 , 0x00},
	{0x0224 , 0x1F},
	{0x0225 , 0x1E},
	{0x0226 , 0x18},
	{0x0227 , 0x1D},
	{0x0228 , 0x1F},
	{0x0229 , 0x1F},
	{0x022A , 0x01},
	{0x022B , 0x04},
	{0x022C , 0x05},
	{0x022D , 0x05},
	{0x022E , 0x04},
	{0x022F , 0x03},
	{0x0230 , 0x02},
	{0x0231 , 0x1F},
	{0x0232 , 0x1A},
	{0x0233 , 0x19},
	{0x0234 , 0x19},
	{0x0235 , 0x1B},
	{0x0236 , 0x1F},
	{0x0237 , 0x04},
	{0x0238 , 0xEE},
	{0x0239 , 0xFF},
	{0x023A , 0x00},
	{0x023B , 0x00},
	{0x023C , 0x00},
	{0x023D , 0x00},
	{0x023E , 0x00},
	{0x023F , 0x00},
	{0x0240 , 0x00},
	{0x0241 , 0x00},
	{0x0242 , 0x00},
	{0x0243 , 0x21},
	{0x0244 , 0x42},
	{0x0245 , 0x53},
	{0x0246 , 0x54},
	{0x0247 , 0x54},
	{0x0248 , 0x54},
	{0x0249 , 0x33},
	{0x024A , 0x11},
	{0x024B , 0x00},
	{0x024C , 0x00},
	{0x024D , 0xFF},
	{0x024E , 0xEE},
	{0x024F , 0xDD},
	{0x0250 , 0x00},
	{0x0251 , 0x00},
	{0x0252 , 0x00},
	{0x0253 , 0x00},
	{0x0254 , 0x00},
	{0x0255 , 0x00},
	{0x0256 , 0x00},
	{0x0257 , 0x00},
	{0x0258 , 0x00},
	{0x0259 , 0x00},
	{0x025A , 0x00},
	{0x025B , 0x00},
	{0x025C , 0x00},
	{0x025D , 0x00},
	{0x025E , 0x00},
	{0x025F , 0x00},
	{0x0260 , 0x00},
	{0x0261 , 0x00},
	{0x0262 , 0x00},
	{0x0263 , 0x00},
	{0x0264 , 0x00},
	{0x0265 , 0x00},
	{0x0266 , 0x00},
	{0x0267 , 0x00},
	{0x0268 , 0x8F},
	{0x0269 , 0xA3},
	{0x026A , 0xB4},
	{0x026B , 0x90},
	{0x026C , 0x00},
	{0x026D , 0xD0},
	{0x026E , 0x60},
	{0x026F , 0xA0},
	{0x0270 , 0x40},
	{0x0300 , 0x81},
	{0x0301 , 0x80},
	{0x0302 , 0x22},
	{0x0303 , 0x06},
	{0x0304 , 0x03},
	{0x0305 , 0x83},
	{0x0306 , 0x00},
	{0x0307 , 0x22},
	{0x0308 , 0x00},
	{0x0309 , 0x55},
	{0x030A , 0x55},
	{0x030B , 0x55},
	{0x030C , 0x54},
	{0x030D , 0x1F},
	{0x030E , 0x13},
	{0x030F , 0x10},
	{0x0310 , 0x04},
	{0x0311 , 0xFF},
	{0x0312 , 0x98},
	
	{0x0313 , 0x28},
	{0x0314 , 0x66},
	{0x0315 , 0x16},
	{0x0316 , 0x26},

	{0x0317 , 0x02},
	{0x0318 , 0x08},
	{0x0319 , 0x0C},
	
	{0x031A , 0x81},
	{0x031B , 0x00},
	{0x031C , 0x3D},
	{0x031D , 0x00},
	{0x031E , 0xF9},
	{0x031F , 0x00},
	{0x0320 , 0x24},
	{0x0321 , 0x14},
	{0x0322 , 0x1A},
	{0x0323 , 0x24},
	{0x0324 , 0x08},
	{0x0325 , 0xF0},
	{0x0326 , 0x30},
	{0x0327 , 0x17},
	{0x0328 , 0x11},
	{0x0329 , 0x22},
	{0x032A , 0x2F},
	{0x032B , 0x21},
	{0x032C , 0xDA},
	{0x032D , 0x10},
	{0x032E , 0xEA},
	{0x032F , 0x18},
	{0x0330 , 0x29},
	{0x0331 , 0x25},
	{0x0332 , 0x12},
	{0x0333 , 0x0F},
	{0x0334 , 0xE0},
	{0x0335 , 0x13},
	{0x0336 , 0xFF},
	{0x0337 , 0x20},
	{0x0338 , 0x46},
	{0x0339 , 0x04},
	{0x033A , 0x04},
	{0x033B , 0xFF},
	{0x033C , 0x01},
	{0x033D , 0x00},

	/* A Light Correction
	{0x031A , 0x81},
	{0x031B , 0x00},
	{0x031C , 0x1D},
	{0x031D , 0x00},
	{0x031E , 0xFD},
	{0x031F , 0x00},
	{0x0320 , 0xE1},
	{0x0321 , 0x1A},
	{0x0322 , 0xDE},
	{0x0323 , 0x11},
	{0x0324 , 0x1A},
	{0x0325 , 0xEE},
	{0x0326 , 0x50},
	{0x0327 , 0x18},
	{0x0328 , 0x25},
	{0x0329 , 0x37},
	{0x032A , 0x24},
	{0x032B , 0x32},
	{0x032C , 0xA9},
	{0x032D , 0x32},
	{0x032E , 0xFF},
	{0x032F , 0x7F},
	{0x0330 , 0xBA},
	{0x0331 , 0x7F},
	{0x0332 , 0x7F},
	{0x0333 , 0x14},
	{0x0334 , 0x81},
	{0x0335 , 0x14},
	{0x0336 , 0xFF},
	{0x0337 , 0x20},
	{0x0338 , 0x46},
	{0x0339 , 0x04},
	{0x033A , 0x04},
	{0x033B , 0x00},
	{0x033C , 0x00},
	{0x033D , 0x00},
	*/
	
	{0x033E , 0x03},
	{0x033F , 0x28},
	{0x0340 , 0x02},
	{0x0341 , 0x60},
	{0x0342 , 0xAC},
	{0x0343 , 0x97},
	{0x0344 , 0x7F},
	{0x0400 , 0xE8},
	{0x0401 , 0x40},
	{0x0402 , 0x00},
	{0x0403 , 0x00},
	{0x0404 , 0xF8},
	{0x0405 , 0x03},
	{0x0406 , 0x03},
	{0x0407 , 0x85},
	{0x0408 , 0x44},
	{0x0409 , 0x1F},
	{0x040A , 0x40},
	{0x040B , 0x33},
	
	{0x040C , 0xA0},
	{0x040D , 0x00},
	{0x040E , 0x00},
	{0x040F , 0x00},
	{0x0410 , 0x0D},
	{0x0411 , 0x0D},
	{0x0412 , 0x0C},
	{0x0413 , 0x04},
	{0x0414 , 0x00},
	{0x0415 , 0x00},
	{0x0416 , 0x07},
	{0x0417 , 0x09},
	{0x0418 , 0x16},
	{0x0419 , 0x14},
	{0x041A , 0x11},
	{0x041B , 0x14},
	{0x041C , 0x07},
	{0x041D , 0x07},
	{0x041E , 0x06},
	{0x041F , 0x02},
	{0x0420 , 0x42},
	{0x0421 , 0x42},
	{0x0422 , 0x47},
	{0x0423 , 0x39},
	{0x0424 , 0x3E},
	{0x0425 , 0x4D},
	{0x0426 , 0x46},
	{0x0427 , 0x3A},
	{0x0428 , 0x21},
	{0x0429 , 0x21},
	{0x042A , 0x26},
	{0x042B , 0x1C},
	{0x042C , 0x25},
	{0x042D , 0x25},
	{0x042E , 0x28},
	{0x042F , 0x20},
	{0x0430 , 0x3E},
	{0x0431 , 0x3E},
	{0x0432 , 0x33},
	{0x0433 , 0x2E},
	{0x0434 , 0x54},
	{0x0435 , 0x53},
	{0x0436 , 0x3C},
	{0x0437 , 0x51},
	{0x0438 , 0x2B},
	{0x0439 , 0x2B},
	{0x043A , 0x38},
	{0x043B , 0x22},
	{0x043C , 0x3B},
	{0x043D , 0x3B},
	{0x043E , 0x31},
	{0x043F , 0x37},
	
	{0x0440 , 0x00},
	{0x0441 , 0x4B},
	{0x0442 , 0x00},
	{0x0443 , 0x00},
	{0x0444 , 0x31},
	
	{0x0445 , 0x00},
	{0x0446 , 0x00},
	{0x0447 , 0x00},
	{0x0448 , 0x00},
	{0x0449 , 0x00},
	{0x044A , 0x00},
	{0x044D , 0xE0},
	{0x044E , 0x05},
	{0x044F , 0x07},
	{0x0450 , 0x00},
	{0x0451 , 0x00},
	{0x0452 , 0x00},
	{0x0453 , 0x00},
	{0x0454 , 0x00},
	{0x0455 , 0x00},
	{0x0456 , 0x00},
	{0x0457 , 0x00},
	{0x0458 , 0x00},
	{0x0459 , 0x00},
	{0x045A , 0x00},
	{0x045B , 0x00},
	{0x045C , 0x00},
	{0x045D , 0x00},
	{0x045E , 0x00},
	{0x045F , 0x00},
	
	{0x0460 , 0x80},
	{0x0461 , 0x10},
	{0x0462 , 0x10},
	{0x0463 , 0x10},
	{0x0464 , 0x08},
	{0x0465 , 0x08},
	{0x0466 , 0x11},
	{0x0467 , 0x09},
	{0x0468 , 0x23},
	{0x0469 , 0x2A},
	{0x046A , 0x2A},
	{0x046B , 0x47},
	{0x046C , 0x52},
	{0x046D , 0x42},
	{0x046E , 0x36},
	{0x046F , 0x46},
	{0x0470 , 0x3A},
	{0x0471 , 0x32},
	{0x0472 , 0x32},
	{0x0473 , 0x38},
	{0x0474 , 0x3D},
	{0x0475 , 0x2F},
	{0x0476 , 0x29},
	{0x0477 , 0x48},
	
	{0x0686 , 0x6F},

	{0x0100 , 0x01},
	{0x0102 , 0x02},
	{0x0104 , 0x03},

	///////////////////////////////////////////////////////////////////
	///////////////////////////GAMMA//////////////////////////////////
	///////////////////////////////////////////////////////////////////

	//-----------GAMMA Select(0)---------------//


	/*
		1:	//smallest gamma curve
			{0x0461 , 0x00},
			{0x0462 , 0x00},
			{0x0463 , 0x00},
			{0x0464 , 0x00},
			{0x0465 , 0x00},
			{0x0466 , 0x12},
			{0x0467 , 0x3B},
			{0x0468 , 0x34},
			{0x0469 , 0x26},
			{0x046A , 0x1E},
			{0x046B , 0x33},
			{0x046C , 0x2E},
			{0x046D , 0x2C},
			{0x046E , 0x28},
			{0x046F , 0x42},
			{0x0470 , 0x42},
			{0x0471 , 0x38},
			{0x0472 , 0x37},
			{0x0473 , 0x4D},
			{0x0474 , 0x48},
			{0x0475 , 0x44},
			{0x0476 , 0x40},
			{0x0477 , 0x56},

		2:
			{0x0461 , 0x00},
			{0x0462 , 0x00},
			{0x0463 , 0x00},
			{0x0464 , 0x00},
			{0x0465 , 0x00},
			{0x0466 , 0x29},
			{0x0467 , 0x37},
			{0x0468 , 0x3A},
			{0x0469 , 0x26},
			{0x046A , 0x21},
			{0x046B , 0x34},
			{0x046C , 0x34},
			{0x046D , 0x2B},
			{0x046E , 0x28},
			{0x046F , 0x41},
			{0x0470 , 0x3F},
			{0x0471 , 0x3A},
			{0x0472 , 0x36},
			{0x0473 , 0x47},
			{0x0474 , 0x44},
			{0x0475 , 0x3B},
			{0x0476 , 0x3B},
			{0x0477 , 0x4D},

		3:
			{0x0461 , 0x00},
			{0x0462 , 0x00},
			{0x0463 , 0x00},
			{0x0464 , 0x00},
			{0x0465 , 0x00},
			{0x0466 , 0x29},
			{0x0467 , 0x4B},
			{0x0468 , 0x41},
			{0x0469 , 0x2A},
			{0x046A , 0x25},
			{0x046B , 0x3A},
			{0x046C , 0x2C},
			{0x046D , 0x2B},
			{0x046E , 0x28},
			{0x046F , 0x40},
			{0x0470 , 0x3D},
			{0x0471 , 0x38},
			{0x0472 , 0x31},
			{0x0473 , 0x44},
			{0x0474 , 0x3E},
			{0x0475 , 0x3E},
			{0x0476 , 0x37},
			{0x0477 , 0x43},

		4:
			{0x0461 , 0x00},
			{0x0462 , 0x00},
			{0x0463 , 0x00},
			{0x0464 , 0x00},
			{0x0465 , 0x00},
			{0x0466 , 0x2F},
			{0x0467 , 0x4E},
			{0x0468 , 0x50},
			{0x0469 , 0x31},
			{0x046A , 0x27},
			{0x046B , 0x3C},
			{0x046C , 0x35},
			{0x046D , 0x27},
			{0x046E , 0x23},
			{0x046F , 0x46},
			{0x0470 , 0x3A},
			{0x0471 , 0x32},
			{0x0472 , 0x32},
			{0x0473 , 0x38},
			{0x0474 , 0x3E},
			{0x0475 , 0x36},
			{0x0476 , 0x33},
			{0x0477 , 0x41},

		5:	//largest gamma curve
			{0x0461 , 0x00},
			{0x0462 , 0x00},
			{0x0463 , 0x00},
			{0x0464 , 0x00},
			{0x0465 , 0x15},
			{0x0466 , 0x33},
			{0x0467 , 0x61},
			{0x0468 , 0x56},
			{0x0469 , 0x30},
			{0x046A , 0x22},
			{0x046B , 0x3E},
			{0x046C , 0x2E},
			{0x046D , 0x2B},
			{0x046E , 0x28},
			{0x046F , 0x3C},
			{0x0470 , 0x38},
			{0x0471 , 0x2F},
			{0x0472 , 0x2A},
			{0x0473 , 0x3C},
			{0x0474 , 0x34},
			{0x0475 , 0x31},
			{0x0476 , 0x31},
			{0x0477 , 0x39},
		*/

	//-------------H_V_Switch(Normal)---------------//
			{0x0101 , 0x00},
			
	 /*GC0309_H_V_Switch,

		1:  // normal
	    		{0x0101 , 0x00},
	    		
		2:  // IMAGE_H_MIRROR
	    		{0x0101 , 0x01},
	    		
		3:  // IMAGE_V_MIRROR
	    		{0x0101 , 0x02},
	    		
		4:  // IMAGE_HV_MIRROR
	    		{0x0101 , 0x03},
	*/		    
	//-------------H_V_Select End--------------//

       {0x0109 , 0x00},
	{0x010A , 0x04},
	{0x010B , 0x0B},
	{0x0110 , 0x02},
	{0x0111 , 0x80},
	{0x0112 , 0x01},
	{0x0113 , 0xE0},


       //{SENSOR_WRITE_DELAY, 200},
	{SENSOR_WRITE_DELAY, 50},
	

	{0xff , 0xff}	
};

SENSOR_REG_T GT2005_YUV_1600X1200[] = 
{
	{0x0109 , 0x01},
	{0x010A , 0x00},
	{0x010B , 0x00},
	{0x0110 , 0x06},
	{0x0111 , 0x40},
	{0x0112 , 0x04},
	{0x0113 , 0xB0},

	{0xff , 0xff}
};

SENSOR_REG_T GT2005_YUV_1280X960[] = 
{
	{0x0109 , 0x01},
	{0x010A , 0x00},
	{0x010B , 0x0B},
	{0x0110 , 0x05},
	{0x0111 , 0x00},
	{0x0112 , 0x03},
	{0x0113 , 0xC0},

	{0xff , 0xff}
};

SENSOR_REG_T GT2005_YUV_640X480[] = 
{
	{0x0109 , 0x00},
	{0x010A , 0x04},
	{0x010B , 0x0B},
	{0x0110 , 0x02},
	{0x0111 , 0x80},
	{0x0112 , 0x01},
	{0x0113 , 0xE0},

	{0xff , 0xff}
};

SENSOR_REG_T GT2005_JPEG_MODE[] =
{
	{0xff , 0xff}
};

LOCAL SENSOR_REG_TAB_INFO_T s_GT2005_resolution_Tab_YUV[]=
{   
        { ADDR_AND_LEN_OF_ARRAY(GT2005_YUV_COMMON),	640, 480, 12, SENSOR_IMAGE_FORMAT_YUV422 },
        // YUV422 PREVIEW 1
        { ADDR_AND_LEN_OF_ARRAY(GT2005_YUV_640X480), 	640, 480, 12, SENSOR_IMAGE_FORMAT_YUV422},
        { ADDR_AND_LEN_OF_ARRAY(GT2005_YUV_1280X960),	1280, 960, 12, SENSOR_IMAGE_FORMAT_YUV422},
        { ADDR_AND_LEN_OF_ARRAY(GT2005_YUV_1600X1200),1600, 1200, 12,SENSOR_IMAGE_FORMAT_YUV422},
        { PNULL,                    0,      0,  	0  ,        0,       0      },
        // YUV422 PREVIEW 2 
         { PNULL,                   0,      0,  	0  ,        0,        0      },
         { PNULL,                   0,      0,  	0  ,        0,        0      },
         { PNULL,                   0,      0,  	0  ,        0,        0      },
         { PNULL,                   0,      0,  	0  ,        0,        0      }
}; 

LOCAL SENSOR_IOCTL_FUNC_TAB_T s_GT2005_ioctl_func_tab = 
{
        // Internal 
        PNULL,
        PNULL,//GT2005_Power_On, 
        PNULL,
        GT2005_Identify,

        PNULL,       
        PNULL,           
        PNULL,
        PNULL,

        // External
        PNULL,
        PNULL,
        PNULL,

        Set_GT2005_Brightness,
        Set_GT2005_Contrast, 
        PNULL,
        PNULL, 
        Set_GT2005_Preview_Mode, 
        Set_GT2005_Image_Effect, 
        GT2005_Before_Snapshot,
        GT2005_After_Snapshot,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        Set_GT2005_AWB,    
        PNULL, 
        PNULL, 
        Set_GT2005_Ev, 
        PNULL, 
        GT2005_Change_Image_Format, 
        PNULL,

        PNULL,
        PNULL,
        Set_GT2005_Anti_Flicker,
        Set_GT2005_Video_Mode,   
        PNULL,
};
/**---------------------------------------------------------------------------*
 ** 						Global Variables								  *
 **---------------------------------------------------------------------------*/
SENSOR_INFO_T g_GT2005_yuv_info =
{
        GT2005_I2C_ADDR_W,				// salve i2c write address
        GT2005_I2C_ADDR_R, 				// salve i2c read address

        BIT_2,							// bit0: 0: i2c register value is 8 bit, 1: i2c register value is 16 bit
        							// bit2: 0: i2c register addr  is 8 bit, 1: i2c register addr  is 16 bit
        							// other bit: reseved
        SENSOR_HW_SIGNAL_PCLK_N|\
        SENSOR_HW_SIGNAL_VSYNC_P|\
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

        SENSOR_LOW_PULSE_RESET,		// reset pulse level
        100,								// reset pulse width(ms)

        SENSOR_LOW_LEVEL_PWDN,		// 1: high level valid; 0: low level valid

        2,								// count of identify code 
        {{0x0000, 0x51},						// supply two code to identify sensor.
        {0x0001, 0x38}},						// for Example: index = 0-> Device id, index = 1 -> version id											
        									
        SENSOR_AVDD_2800MV,			// voltage of avdd	

        1600,							// max width of source image
        1200,							// max height of source image
        "GT2005",						// name of sensor												

        SENSOR_IMAGE_FORMAT_YUV422,	// define in SENSOR_IMAGE_FORMAT_E enum,
        							// if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T
        SENSOR_IMAGE_PATTERN_YUV422_YUYV,	// pattern of input image form sensor;			

        s_GT2005_resolution_Tab_YUV,	// point to resolution table information structure
        &s_GT2005_ioctl_func_tab,		// point to ioctl function table
        	
        PNULL,							// information and table about Rawrgb sensor
        PNULL,							// extend information about sensor	
        SENSOR_AVDD_2800MV,                    // iovdd
        SENSOR_AVDD_1500MV,                    // dvdd
        0,                     // skip frame num before preview 
        0,                      // skip frame num before capture	
        0,
        2,
        0,
        0,
        0,
        0
};
/**---------------------------------------------------------------------------*
 **                         Function Definitions                              *
 **---------------------------------------------------------------------------*/
/******************************************************************************/
// Description: sensor probe function
// Author:     benny.zou
// Input:      none
// Output:     result
// Return:     0           successful
//             others      failed
// Note:       this function only to check whether sensor is work, not identify 
//              whitch it is!!
/******************************************************************************/
LOCAL uint32_t Set_GT2005_Preview_Mode(uint32_t preview_mode)
{
        switch (preview_mode) {
        case DCAMERA_ENVIRONMENT_NORMAL: 
                Sensor_WriteReg(0x0312, 0x98);	// 1/2 Frame rate	
                break;
        case DCAMERA_ENVIRONMENT_NIGHT:
                Sensor_WriteReg(0x0312, 0xa8);	// 1/3 Frame rate	
                break;
        default:
                break;
        }
        SENSOR_TRACE("set_GT2005_preview_mode: level = %d", preview_mode);
        msleep(100);
        return 0;
}
LOCAL uint32_t GT2005_Identify(uint32_t param)
{
#define GT2005_ID_H_VALUE	0x51
#define GT2005_ID_L_VALUE	0x38
		
        uint8_t id_h_value = 0;
        uint8_t id_l_value = 0;
        uint32_t ret_value = 0xFF;

        id_h_value = Sensor_ReadReg(0x0000);
        SENSOR_TRACE("GT2005_Identify-id_h_value %d.\n", id_h_value);

        id_l_value = Sensor_ReadReg(0x0001);
        SENSOR_TRACE("GT2005_Identify-id_l_value %d.\n", id_l_value);

        if((GT2005_ID_H_VALUE == id_h_value) && (GT2005_ID_L_VALUE == id_l_value)) {
                ret_value = 0;
                SENSOR_TRACE("It Is GT2005 Sensor !\n");	
        }
        return ret_value;

}

LOCAL void GT2005_Write_Group_Regs( SENSOR_REG_T* sensor_reg_ptr )
{
        uint32_t i;

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value) ; i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
}

SENSOR_REG_T GT2005_brightness_tab[][2]=
{
        {{0x0201 , 0xa0},{0xff , 0xff}},
        {{0x0201 , 0xc0},{0xff , 0xff}},
        {{0x0201 , 0xd0},{0xff , 0xff}},
        {{0x0201 , 0x00},{0xff , 0xff}},
        {{0x0201 , 0x20},{0xff , 0xff}},
        {{0x0201 , 0x40},{0xff , 0xff}},
        {{0x0201 , 0x60},{0xff , 0xff}}
};

LOCAL uint32_t Set_GT2005_Brightness(uint32_t level)
{
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)GT2005_brightness_tab[level];

        SENSOR_ASSERT(PNULL != sensor_reg_ptr);
        GT2005_Write_Group_Regs(sensor_reg_ptr);
        SENSOR_TRACE("set_GT2005_brightness: level = %d", level);
        return 0;
}

SENSOR_REG_T GT2005_contrast_tab[][2]=
{
        {{0x0200 , 0xa0},{0xff , 0xff}},
        {{0x0200 , 0xc0},{0xff , 0xff}},
        {{0x0200 , 0xd0},{0xff , 0xff}},
        {{0x0200 , 0x00},{0xff , 0xff}},
        {{0x0200 , 0x20},{0xff , 0xff}},
        {{0x0200 , 0x40},{0xff , 0xff}},
        {{0x0200 , 0x60},{0xff , 0xff}}
};

LOCAL uint32_t Set_GT2005_Contrast(uint32_t level)
{ 
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)GT2005_contrast_tab[level];

        SENSOR_ASSERT(PNULL != sensor_reg_ptr);
        GT2005_Write_Group_Regs(sensor_reg_ptr);
        SENSOR_TRACE("set_GT2005_contrast: level = %d", level);
        return 0;
}
	
SENSOR_REG_T GT2005_image_effect_tab[][4]=	
{
        //Normal
        {{0x0115 , 0x00},{0xff , 0xff},{0xff , 0xff},{0xff , 0xff}},
        //BLACK&WHITE
        {{0x0115 , 0x06},{0xff , 0xff},{0xff , 0xff},{0xff , 0xff}},
        //RED
        {{0x0115 , 0x0a},{0x026e , 0x70},{0x026f , 0xf0},{0xff , 0xff}},
        //GREEN
        {{0x0115 , 0x0a},{0x026e , 0x20},{0x026f , 0x00},{0xff , 0xff}},
        //BLUE
        {{0x0115 , 0x0a},{0x026e , 0xf0},{0x026f , 0x00},{0xff , 0xff}},
        //YELLOW
        {{0x0115 , 0x0a},{0x026e , 0x00},{0x026f , 0x80},{0xff , 0xff}},
        //NEGATIVE
        {{0x0115 , 0x09},{0xff , 0xff},{0xff , 0xff},{0xff , 0xff}},
        //SEPIA
        {{0x0115 , 0x0a},{0x026e , 0x60},{0x026f , 0xa0},{0xff , 0xff}}
};

LOCAL uint32_t Set_GT2005_Image_Effect(uint32_t effect_type)
{   
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)GT2005_image_effect_tab[effect_type];
        SENSOR_ASSERT(PNULL != sensor_reg_ptr);
        GT2005_Write_Group_Regs(sensor_reg_ptr);
        SENSOR_TRACE("set_GT2005_image_effect: effect_type = %d", effect_type);
        return 0;
}

SENSOR_REG_T GT2005_ev_tab[][3]=
{   
        {{0x0300 , 0x81},{0x0301 , 0x50},{0xff , 0xff}},
        {{0x0300 , 0x81},{0x0301 , 0x60},{0xff , 0xff}},
        {{0x0300 , 0x81},{0x0301 , 0x70},{0xff , 0xff}},
        {{0x0300 , 0x81},{0x0301 , 0x80},{0xff , 0xff}},
        {{0x0300 , 0x81},{0x0301 , 0x90},{0xff , 0xff}},
        {{0x0300 , 0x81},{0x0301 , 0xa0},{0xff , 0xff}},
        {{0x0300 , 0x81},{0x0301 , 0xb0},{0xff , 0xff}}
};

LOCAL uint32_t Set_GT2005_Ev(uint32_t level)
{
  
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)GT2005_ev_tab[level];

        SENSOR_ASSERT(PNULL != sensor_reg_ptr);
        GT2005_Write_Group_Regs(sensor_reg_ptr );
        SENSOR_TRACE("set_GT2005_ev: level = %d", level);
        return 0;
}
/******************************************************************************/
// Description: anti 50/60 hz banding flicker
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL uint32_t Set_GT2005_Anti_Flicker(uint32_t mode)
{ 
        //PLL Setting 15FPS Under 19.5MHz PCLK
        Sensor_WriteReg(0x0116 , 0x01);
        Sensor_WriteReg(0x0118 , 0x34);
        Sensor_WriteReg(0x0119 , 0x01);
        Sensor_WriteReg(0x011a , 0x04);	
        Sensor_WriteReg(0x011B , 0x01);
        Sensor_WriteReg(0x0313 , 0x34); 
        Sensor_WriteReg(0x0314 , 0x3B); 

        switch(mode) {
        case FLICKER_50HZ:
                Sensor_WriteReg(0x0315 , 0x16);                  			
                break;
        case FLICKER_60HZ:
                Sensor_WriteReg(0x0315 , 0x56);                  			
                break;
        default:
                return 0;
        }

        SENSOR_TRACE("set_GT2005_anti_flicker-mode=%d",mode);
        msleep(200);
        return 0;
}

SENSOR_REG_T GT2005_awb_tab[][10]=
{
        //Auto
        {
                {0x031a , 0x81},                                                             
                {0x0320 , 0x24},
                {0x0321 , 0x14},				
                {0x0322 , 0x1a},		
                {0x0323 , 0x24},
                {0x0441 , 0x4B},
                {0x0442 , 0x00},
                {0x0443 , 0x00},
                {0x0444 , 0x31},
                {0xff , 0xff}
        },
        //Office
        {
                {0x0320 , 0x02},
                {0x0321 , 0x02},
                {0x0322 , 0x02},
                {0x0323 , 0x02},
                {0x0441 , 0x60},
                {0x0442 , 0x00},
                {0x0443 , 0x00},
                {0x0444 , 0x80},
                {0xff , 0xff},
                {0xff , 0xff}
        },
        //U30  //not use
        {
                {0xff , 0xff},
                {0xff , 0xff},
                {0xff , 0xff},       
                {0xff , 0xff},    
                {0xff , 0xff},  
                {0xff , 0xff},
                {0xff , 0xff},
                {0xff , 0xff},         
                {0xff , 0xff},
                {0xff , 0xff}
        },
        //CWF  //not use
        {
                {0xff , 0xff},
                {0xff , 0xff},
                {0xff , 0xff},       
                {0xff , 0xff},    
                {0xff , 0xff},  
                {0xff , 0xff},
                {0xff , 0xff},
                {0xff , 0xff},         
                {0xff , 0xff},
                {0xff , 0xff}
        },
        //HOME
        {
                {0x0320 , 0x02},
                {0x0321 , 0x02},
                {0x0322 , 0x02},
                {0x0323 , 0x02},
                {0x0441 , 0x30},
                {0x0442 , 0x00},
                {0x0443 , 0x00},
                {0x0444 , 0x80},   
                {0xff, 0xff},
                {0xff, 0xff}
        },  

        //SUN:
        {
                {0x0320 , 0x02},
                {0x0321 , 0x02},
                {0x0322 , 0x02},
                {0x0323 , 0x02},  
                {0x0441 , 0x90},
                {0x0442 , 0x00},
                {0x0443 , 0x00},
                {0x0444 , 0x40},     
                {0xff , 0xff},
                {0xff , 0xff}
        },

        //CLOUD:
        {
                {0x0320 , 0x02},
                {0x0321 , 0x02},
                {0x0322 , 0x02},
                {0x0323 , 0x02},  
                {0x0441 , 0x70},
                {0x0442 , 0x00},
                {0x0443 , 0x00},
                {0x0444 , 0x30}, 
                {0xff , 0xff},
                {0xff , 0xff}
        }
};

LOCAL uint32_t Set_GT2005_AWB(uint32_t mode)
{	
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)GT2005_awb_tab[mode];

        SENSOR_ASSERT(PNULL != sensor_reg_ptr);
        GT2005_Write_Group_Regs(sensor_reg_ptr);
        msleep(100); 
        SENSOR_TRACE("set_GT2005_awb_mode: mode = %d", mode);
        return 0;
}

LOCAL void    GT2005_Set_Shutter(void)
{
        uint16_t shutter,AGain_shutter,DGain_shutter;

        Sensor_WriteReg(0x020B , 0x28);
        Sensor_WriteReg(0x020C , 0x44);
        Sensor_WriteReg(0x040B , 0x44);
        Sensor_WriteReg(0x0300 , 0xc1);
        shutter = (Sensor_ReadReg(0x0012)<<8 )|( Sensor_ReadReg(0x0013));    
        AGain_shutter = (Sensor_ReadReg(0x0014)<<8 )|( Sensor_ReadReg(0x0015));
        DGain_shutter = (Sensor_ReadReg(0x0016)<<8 )|( Sensor_ReadReg(0x0017));
        Sensor_WriteReg(0x0300 , 0x41); //close ALC
        shutter = shutter / 2; 
        Sensor_WriteReg(0x0305 , shutter&0xff);           
        Sensor_WriteReg(0x0304 , (shutter>>8)&0xff); 
        Sensor_WriteReg(0x0307 , AGain_shutter&0xff);      
        Sensor_WriteReg(0x0306 , (AGain_shutter>>8)&0xff); //AG
        Sensor_WriteReg(0x0308,  DGain_shutter&0xff);   //DG	
}

LOCAL uint32_t GT2005_Before_Snapshot(uint32_t sensor_snapshot_mode)
{
        switch(sensor_snapshot_mode) {
        case SENSOR_MODE_PREVIEW_ONE:    //VGA
                SENSOR_TRACE("Capture VGA Size");
                break;
        case SENSOR_MODE_SNAPSHOT_ONE_FIRST:    // 1.3 M
        case SENSOR_MODE_SNAPSHOT_ONE_SECOND:    // 2 M
                SENSOR_TRACE("Capture 1.3M&2M Size");
                GT2005_Set_Shutter	();
                //PLL Setting 3FPS Under 10MHz PCLK 
                Sensor_WriteReg(0x0119 , 0x02);
                break;
        default:
        	break;
        }
        SENSOR_TRACE("SENSOR_GT2005: Before Snapshot");
        return 0;
}

LOCAL uint32_t GT2005_After_Snapshot(uint32_t para)
{	
        GT2005_Change_Image_Format(SENSOR_IMAGE_FORMAT_YUV422);
        SENSOR_TRACE("SENSOR_GT2005: After Snapshot");
        return 0;
}

LOCAL uint32_t GT2005_Change_Image_Format(uint32_t param)
{
        SENSOR_REG_TAB_INFO_T st_jpeg_reg_table_info = { ADDR_AND_LEN_OF_ARRAY(GT2005_JPEG_MODE), 0,0,0, 0};
        SENSOR_REG_TAB_INFO_T st_yuv422_reg_table_info = { ADDR_AND_LEN_OF_ARRAY(GT2005_YUV_COMMON),0,0,0,0};
        uint32_t ret_val = SENSOR_FAIL;

        switch(param) {
        case SENSOR_IMAGE_FORMAT_YUV422:
                ret_val = Sensor_SendRegTabToSensor(&st_yuv422_reg_table_info);
                break;
        case SENSOR_IMAGE_FORMAT_JPEG:
                ret_val = Sensor_SendRegTabToSensor(&st_jpeg_reg_table_info);
                break;
        default:
                break;
        }
        return ret_val;
} 
/******************************************************************************/
// Description: set video mode
// Global resource dependence: 
// Author:
// Note:
//		 
/******************************************************************************/
SENSOR_REG_T GT2005_video_mode_nor_tab[][8]=
{
        // normal mode 
        {{0xff,0xff},{0xff,0xff},{0xff,0xff},{0xff,0xff},{0xff,0xff},{0xff,0xff},{0xff,0xff},{0xff,0xff}},    
        //vodeo mode     10fps Under 13MHz MCLK
        {{0x0116 , 0x01},{0x0118 , 0x45},{0x0119 , 0x02},{0x011a , 0x04},{0x011B , 0x02},{0x0313 , 0x32}, {0x0314 , 0xCE}, {0xff , 0xff}},
        // UPCC  mode	  10fps Under 13MHz MCLK
        {{0x0116 , 0x01},{0x0118 , 0x45},{0x0119 , 0x02},{0x011a , 0x04},{0x011B , 0x02},{0x0313 , 0x32}, {0x0314 , 0xCE}, {0xff , 0xff}}
};

LOCAL uint32_t Set_GT2005_Video_Mode(uint32_t mode)
{    
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)GT2005_video_mode_nor_tab[mode];
        GT2005_Write_Group_Regs(sensor_reg_ptr );
        SENSOR_TRACE("set_GT2005_video_mode=%d",mode);
        return 0;
}	
	
#endif
struct sensor_drv_cfg sensor_gt2005 = {
        .sensor_pos = CONFIG_DCAM_SENSOR_POS_GT2005,
        .sensor_name = "gt2005",
        .driver_info = &g_GT2005_yuv_info,
};

static int __init sensor_gt2005_init(void)
{
        return dcam_register_sensor_drv(&sensor_gt2005);
}

subsys_initcall(sensor_gt2005_init);

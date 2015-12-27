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
#ifndef _GC0307_C_
#define _GC0307_C_

#include "common/sensor.h"
/**---------------------------------------------------------------------------*
 **                         Const variables                                   *
 **---------------------------------------------------------------------------*/
#define GC0307_I2C_ADDR_W    			0x21//0x42
#define GC0307_I2C_ADDR_R    			0x21//0x43 
#define GC0307_I2C_ACK				0x0
/**---------------------------------------------------------------------------*
 **                         Local variables                                   *
 **---------------------------------------------------------------------------*/
LOCAL uint32_t DARK_SEQUENCE_0x47 = 0xee;
LOCAL uint32_t DARK_SEQUENCE_0x47_Effect = 0xee;
LOCAL BOOLEAN  bl_GC_50Hz_GC0307 = SENSOR_FALSE;
/**---------------------------------------------------------------------------*
 **                     Local Function Prototypes                             *
 **---------------------------------------------------------------------------*/
const SENSOR_REG_T GC0307_YVU_640X480[] = 
{
	{	0x43  ,0x00}, 
	{	0x44  ,0xa2}, 
	
	//========= close some functions
	// open them after configure their parmameters
	{	0x40  ,0x10}, 
	{	0x41  ,0x00}, 			
	{	0x42  ,0x10},					  	
	{	0x47  ,0x00}, //mode1,				  	
	{	0x48  ,0xc3}, //mode2, 	
	{	0x49  ,0x00}, //dither_mode 		
	{	0x4a  ,0x00}, //clock_gating_en
	{	0x4b  ,0x00}, //mode_reg3
	{	0x4E  ,0x23}, //sync mode
	{	0x4F  ,0x01}, //AWB, AEC, every N frame	
	
	//========= frame timing
//	{	0x01  ,0xff}, //HB   0x6a
//	{	0x02  ,0x25}, //VB  0x25s
	{	0x1C  ,0x00}, //Vs_st
	{	0x1D  ,0x00}, //Vs_et
//	{	0x10  ,0x00}, //high 4 bits of VB, HB
	{	0x11  ,0x05}, //row_tail,  AD_pipe_number
	
	{	0x03  ,0x00}, //row_start
	{	0x04  ,0x96},
	//========= windowing
	{	0x05  ,0x00}, //row_start
	{	0x06  ,0x00},
	{	0x07  ,0x00}, //col start
	{	0x08  ,0x00}, 
	{	0x09  ,0x01}, //win height
	{	0x0A  ,0xE8},
	{	0x0B  ,0x02}, //win width, pixel array only 640
	{	0x0C  ,0x80},
	
	//========= analog
	{	0x0D  ,0x22}, //rsh_width
					  
	{	0x0E  ,0x03}, //CISCTL mode2,  

			  
	{	0x12  ,0x70}, //7 hrst, 6_4 darsg,
	{	0x13  ,0x00}, //7 CISCTL_restart, 0 apwd
	{	0x14  ,0x00}, //NA
	{	0x15  ,0xba}, //7_4 vref
	{	0x16  ,0x13}, //5to4 _coln_r,  __1to0__da18
	{	0x17  ,0x52}, //opa_r, ref_r, sRef_r
	//{	0x18  ,0xc0}, //analog_mode, best case for left band.
	
	{	0x1E  ,0x0d}, //tsp_width 		   
	{	0x1F  ,0x32}, //sh_delay
	
	//========= offset
	{	0x47  ,0x00},  //7__test_image, __6__fixed_pga, __5__auto_DN, __4__CbCr_fix, 
				//__3to2__dark_sequence, __1__allow_pclk_vcync, __0__LSC_test_image
	{	0x19  ,0x06},  //pga_o			 
	{	0x1a  ,0x06},  //pga_e			 
	
	{	0x31  ,0x00},  //4	//pga_oFFset ,	 high 8bits of 11bits
	{	0x3B  ,0x00},  //global_oFFset, low 8bits of 11bits
	
	{	0x59  ,0x0f},  //offset_mode 	
	{	0x58  ,0x88},  //DARK_VALUE_RATIO_G,  DARK_VALUE_RATIO_RB
	{	0x57  ,0x08},  //DARK_CURRENT_RATE
	{	0x56  ,0x77},  //PGA_OFFSET_EVEN_RATIO, PGA_OFFSET_ODD_RATIO
	
	//========= blk
	{	0x35  ,0xd8},  //blk_mode

	{	0x36  ,0x40},  
	
	{	0x3C  ,0x00}, 
	{	0x3D  ,0x00}, 
	{	0x3E  ,0x00}, 
	{	0x3F  ,0x00}, 
	
	{	0xb5  ,0x70}, 
	{	0xb6  ,0x40}, 
	{	0xb7  ,0x00}, 
	{	0xb8  ,0x38}, 
	{	0xb9  ,0xc3}, 		  
	{	0xba  ,0x0f}, 
	
	{	0x7e  ,0x35}, 
	{	0x7f  ,0x86}, 
	
	{	0x5c  ,0x68}, //78
	{	0x5d  ,0x78}, //88
	
	
	//========= manual_gain 
	{	0x61  ,0x80}, //manual_gain_g1	
	{	0x63  ,0x80}, //manual_gain_r
	{	0x65  ,0x98}, //manual_gai_b, 0xa0=1.25, 0x98=1.1875
	{	0x67  ,0x80}, //manual_gain_g2
	{	0x68  ,0x18}, //global_manual_gain	 2.4bits
	
	//=========CC _R
	{	0x69  ,0x58},  //54
	{	0x6A  ,0xf6},  //ff
	{	0x6B  ,0xfb},  //fe
	{	0x6C  ,0xf4},  //ff
	{	0x6D  ,0x5a},  //5f
	{	0x6E  ,0xe6},  //e1

	{	0x6f  ,0x00}, 	
	
	//=========lsc							  
	{	0x70  ,0x14}, 
	{	0x71  ,0x1c}, 
	{	0x72  ,0x20}, 
	
	{	0x73  ,0x10}, 	
	{	0x74  ,0x3c}, 
	{	0x75  ,0x52}, 
	
	//=========dn																			 
	{	0x7d  ,0x2f},  //dn_mode   	
	{	0x80  ,0x0c}, //when auto_dn, check 7e,7f
	{	0x81  ,0x0c},
	{	0x82  ,0x44},
																						
	//dd																		   
	{	0x83  ,0x18},  //DD_TH1 					  
	{	0x84  ,0x18},  //DD_TH2 					  
	{	0x85  ,0x04},  //DD_TH3 																							  
	{	0x87  ,0x34},  //32 b DNDD_low_range X16,  DNDD_low_range_C_weight_center					
	
	   
	//=========intp-ee																		   
	{	0x88  ,0x04},  													   
	{	0x89  ,0x01},  										  
	{	0x8a  ,0x50},//60  										   
	{	0x8b  ,0x50},//60  										   
	{	0x8c  ,0x07},  												 				  
																					  
	{	0x50  ,0x0c},   						   		
	{	0x5f  ,0x3c}, 																					 
																					 
	{	0x8e  ,0x02},  															  
	{	0x86  ,0x02},  																  
																					
	{	0x51  ,0x20},  																
	{	0x52  ,0x08},  
	{	0x53  ,0x00}, 
	
	
	//========= YCP 
	//contrast_center																			  
	{	0x77  ,0x80}, //contrast_center 																  
	{	0x78  ,0x00}, //fixed_Cb																		  
	{	0x79  ,0x00}, //fixed_Cr																		  
	{	0x7a  ,0x00}, //luma_offset 																																							
	{	0x7b  ,0x40}, //hue_cos 																		  
	{	0x7c  ,0x00}, //hue_sin 																		  
																							 
	//saturation																				  
	{	0xa0  ,0x40}, //global_saturation
	{	0xa1  ,0x40}, //luma_contrast																	  
	{	0xa2  ,0x34}, //saturation_Cb																	  
	{	0xa3  ,0x34}, //saturation_Cr
																				
	{	0xa4  ,0xc8}, 																  
	{	0xa5  ,0x02}, 
	{	0xa6  ,0x28}, 																			  
	{	0xa7  ,0x02}, 
	
	//skin																								  
	{	0xa8  ,0xee}, 															  
	{	0xa9  ,0x12}, 															  
	{	0xaa  ,0x01}, 														  
	{	0xab  ,0x20}, 													  
	{	0xac  ,0xf0}, 														  
	{	0xad  ,0x10}, 															  
		
	//========= ABS
	{	0xae  ,0x18}, 
	{	0xaf  ,0x74}, 
	{	0xb0  ,0xe0}, 	  
	{	0xb1  ,0x20}, 
	{	0xb2  ,0x6c}, 
	{	0xb3  ,0xc0}, 
	{	0xb4  ,0x04}, 
		
	//========= AWB 
	{	0xbb  ,0x42}, 
	{	0xbc  ,0x60},
	{	0xbd  ,0x50},
	{	0xbe  ,0x50},
	
	{	0xbf  ,0x0c}, 
	{	0xc0  ,0x06}, 
	{	0xc1  ,0x60}, 
	{	0xc2  ,0xf1},  //f1
	{	0xc3  ,0x40},
	{	0xc4  ,0x1c}, //18//20
	{	0xc5  ,0x56},  //33
	{	0xc6  ,0x1d}, 

	{	0xca  ,0x56}, //70
	{	0xcb  ,0x52}, //70
	{	0xcc  ,0x66}, //78
	
	{	0xcd  ,0x80}, //R_ratio 									 
	{	0xce  ,0x80}, //G_ratio  , cold_white white 								   
	{	0xcf  ,0x80}, //B_ratio  	
	
	//=========  aecT  
	{	0x20  ,0x06},//0x02 
	{	0x21  ,0xc0}, 
	{	0x22  ,0x60},    
	{	0x23  ,0x88}, 
	{	0x24  ,0x96}, 
	{	0x25  ,0x30}, 
	{	0x26  ,0xd0}, 
	{	0x27  ,0x00}, 
/*	
	{	0x28  ,0x02}, //AEC_exp_level_1bit11to8   
	{	0x29  ,0x0d}, //AEC_exp_level_1bit7to0	  
	{	0x2a  ,0x02}, //AEC_exp_level_2bit11to8   
	{	0x2b  ,0x0d}, //AEC_exp_level_2bit7to0			 
	{	0x2c  ,0x02}, //AEC_exp_level_3bit11to8   659 - 8FPS,  8ca - 6FPS  //	 
	{	0x2d  ,0x0d}, //AEC_exp_level_3bit7to0			 
	{	0x2e  ,0x05}, //AEC_exp_level_4bit11to8   4FPS 
	{	0x2f  ,0xdc}, //AEC_exp_level_4bit7to0	 
*/	
	{	0x30  ,0x20}, 						  
	{	0x31  ,0x00}, 					   
	{	0x32  ,0x1c}, 
	{	0x33  ,0x90}, 			  
	{	0x34  ,0x10},	
	
	{	0xd0  ,0x34}, 
			   
	{	0xd1  ,0x50}, //AEC_target_Y						   
	{	0xd2  ,0x61},//0xf2 	  
	{	0xd4  ,0x96}, 
	{	0xd5  ,0x01}, // william 0318
	{	0xd6  ,0x4b}, //antiflicker_step 					   
	{	0xd7  ,0x03}, //AEC_exp_time_min ,william 20090312			   
	{	0xd8  ,0x02}, 
			   
	{	0xdd  ,0x22},//0x12 

	{0x7a, 0xf0},
	{0xd1, 0x48}, 
	  															
	//========= measure window										
	{	0xe0  ,0x03}, 						 
	{	0xe1  ,0x02}, 							 
	{	0xe2  ,0x27}, 								 
	{	0xe3  ,0x1e}, 				 
	{	0xe8  ,0x3b}, 					 
	{	0xe9  ,0x6e}, 						 
	{	0xea  ,0x2c}, 					 
	{	0xeb  ,0x50}, 					 
	{	0xec  ,0x73}, 		 
	
	//========= close_frame													
	{	0xed  ,0x00}, //close_frame_num1 ,can be use to reduce FPS				 
	{	0xee  ,0x00}, //close_frame_num2  
	{	0xef  ,0x00}, //close_frame_num
	
	// page1
	{	0xf0  ,0x01}, //select page1 
	
	{	0x00  ,0x20}, 							  
	{	0x01  ,0x20}, 							  
	{	0x02  ,0x20}, 									
	{	0x03  ,0x20}, 	
							
	{	0x04  ,0x78}, 
	{	0x05  ,0x78}, 					 
	{	0x06  ,0x78}, 								  
	{	0x07  ,0x78}, 									 
	
	{	0x10  ,0x04}, 						  
	{	0x11  ,0x04},							  
	{	0x12  ,0x04}, 						  
	{	0x13  ,0x04}, 							  
	{	0x14  ,0x01}, 							  
	{	0x15  ,0x01}, 							  
	{	0x16  ,0x01}, 						 
	{	0x17  ,0x01}, 						 
		  
													 
	{	0x20  ,0x00}, 					  
	{	0x21  ,0x00}, 					  
	{	0x22  ,0x00}, 						  
	{	0x23  ,0x00}, 						  
	{	0x24  ,0x00}, 					  
	{	0x25  ,0x00}, 						  
	{	0x26  ,0x00}, 					  
	{	0x27  ,0x00},  						  
	
	{	0x40  ,0x11}, 
	
	//=============================lscP 
	{	0x45  ,0x06}, 	 
	{	0x46  ,0x06}, 			 
	{	0x47  ,0x05}, 
	
	{	0x48  ,0x04}, 	
	{	0x49  ,0x03}, 		 
	{	0x4a  ,0x03}, 
	

	{	0x62  ,0xd8}, 
	{	0x63  ,0x24}, 
	{	0x64  ,0x24},
	{	0x65  ,0x24}, 
	{	0x66  ,0xd8}, 
	{	0x67  ,0x24},
	
	{	0x5a  ,0x00}, 
	{	0x5b  ,0x00}, 
	{	0x5c  ,0x00}, 
	{	0x5d  ,0x00}, 
	{	0x5e  ,0x00}, 
	{	0x5f  ,0x00}, 
	
	
	//============================= ccP 
	
	{	0x69  ,0x03}, //cc_mode
		  
	//CC_G
	{	0x70  ,0x5d}, 
	{	0x71  ,0xed}, 
	{	0x72  ,0xff}, 
	{	0x73  ,0xe5}, 
	{	0x74  ,0x5f}, 
	{	0x75  ,0xe6}, 
	
        //CC_B
	{	0x76  ,0x41}, 
	{	0x77  ,0xef}, 
	{	0x78  ,0xff}, 
	{	0x79  ,0xff}, 
	{	0x7a  ,0x5f}, 
	{	0x7b  ,0xfa}, 	 
	
	
	//============================= AGP
	
	{	0x7e  ,0x00},  
	{	0x7f  ,0x00},  
	{	0x80  ,0xc8},  
	{	0x81  ,0x06},  
	{	0x82  ,0x08},  
	
	/*{	0x83  ,0x23},  // 这是0x00对应的Gamma
	{	0x84  ,0x38},  
	{	0x85  ,0x4F},  
	{	0x86  ,0x61},  
	{	0x87  ,0x72},  
	{	0x88  ,0x80},  
	{	0x89  ,0x8D},  
	{	0x8a  ,0xA2},  
	{	0x8b  ,0xB2},  
	{	0x8c  ,0xC0},  
	{	0x8d  ,0xCA},  
	{	0x8e  ,0xD3},  
	{	0x8f  ,0xDB},  
	{	0x90  ,0xE2},  
	{	0x91  ,0xED},  
	{	0x92  ,0xF6},  
	{	0x93  ,0xFD}, */ 

	{	0x83  ,0x13},  // 相当于0x20对应的Gamma
	{	0x84  ,0x23},  
	{	0x85  ,0x35},  
	{	0x86  ,0x44},  
	{	0x87  ,0x53},  
	{	0x88  ,0x60},  
	{	0x89  ,0x6D},  
	{	0x8a  ,0x84},  
	{	0x8b  ,0x98},  
	{	0x8c  ,0xaa},  
	{	0x8d  ,0xb8},  
	{	0x8e  ,0xc6},  
	{	0x8f  ,0xd1},  
	{	0x90  ,0xdb},  
	{	0x91  ,0xea},  
	{	0x92  ,0xf5},  
	{	0x93  ,0xFb},
	
	//about gamma1 is hex r oct
	{	0x94  ,0x04},  //这是0x40对应的Gamma
	{	0x95  ,0x0E},  
	{	0x96  ,0x1B},  
	{	0x97  ,0x28},  
	{	0x98  ,0x35},  
	{	0x99  ,0x41},  
	{	0x9a  ,0x4E},  
	{	0x9b  ,0x67},  
	{	0x9c  ,0x7E},  
	{	0x9d  ,0x94},  
	{	0x9e  ,0xA7},  
	{	0x9f  ,0xBA},  
	{	0xa0  ,0xC8},  
	{	0xa1  ,0xD4},  
	{	0xa2  ,0xE7},  
	{	0xa3  ,0xF4},  
	{	0xa4  ,0xFA}, 
	
	//========= open functions	
	{	0xf0  ,0x00}, //set back to page0	
	{	0x40  ,0x7e}, 
  	{	0x41  ,0x2f}, //keep AEC close here
  	
	{0x0f, 0xb2},       
	{0x45, 0x27},
	{0x47, 0x2c},	
#if 0
//  IMAGE_NORMAL:
	{0x0f, 0xb2},
	{0x45, 0x27},
	{0x47, 0x2c},			

// IMAGE_H_MIRROR:
	{0x0f, 0xa2},
	{0x45, 0x26},
	{0x47, 0x28},	
	
// IMAGE_V_MIRROR:			
	{0x0f, 0x92},
	{0x45, 0x25},
	{0x47, 0x24},			

// IMAGE_HV_MIRROR:	   // 180
	{0x0f, 0x82},
	{0x45, 0x24},
	{0x47, 0x20},	
#endif

	{0x43  ,0x40},
	{0x44  ,0xE2},
    {SENSOR_WRITE_DELAY, 0X20},//delay 100ms
};
/**---------------------------------------------------------------------------*
 **                         Function Definitions                              *
 **---------------------------------------------------------------------------*/
LOCAL void GC0307_WriteReg( uint8_t  subaddr, uint8_t data )
{	
#ifndef	_USE_DSP_I2C_
        Sensor_WriteReg(subaddr, data);
#else
        DSENSOR_IICWrite((uint16_t)subaddr, (uint16_t)data);
#endif
        SENSOR_PRINT("SENSOR: GC0307_WriteReg reg/value(%x,%x) !!", subaddr, data);
}

LOCAL uint8_t GC0307_ReadReg( uint8_t  subaddr)
{
        uint8_t value = 0;
#ifndef	_USE_DSP_I2C_
        value =Sensor_ReadReg( subaddr);
#else
        value = (uint16)DSENSOR_IICRead((uint16)subaddr);
#endif
        return value;
}
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
LOCAL uint32_t GC0307_Identify(uint32_t param)
{
        uint8_t reg[2]    = {0x00, 0x00};
        uint8_t value[2]  = {0x99, 0x99};
        uint8_t ret       = 0;
        uint32_t i;
        uint8_t err_cnt = 0;
        uint32_t nLoop = 1000;
        SENSOR_PRINT("GC0307_Identify-START");
        GC0307_WriteReg(0x18, 0xf0);		

        msleep(10);

        for(i = 0; i<2; ) {
                nLoop = 1000;
                ret = GC0307_ReadReg(reg[i]);    
                SENSOR_PRINT( "GC0307 read ret is %X",ret );
                if( ret != value[i]) {
                        err_cnt++;
                        if(err_cnt>3) {
                                SENSOR_PRINT( "GC0307 Fail" );
                                return 0xFF;
                        } else {
                                while(nLoop--);
                                continue;
                        }
                }
                err_cnt = 0;
                i++;
        }
        SENSOR_PRINT( "GC0307 succ" );
        SENSOR_PRINT("GC0307_Identify-END");
        return (uint32_t)SENSOR_SUCCESS;
}

void GC0307_H_V_Switch(uint8_t direction)
{
        switch(direction) {
        case 1:  // normal
                GC0307_WriteReg(0x0f, 0xb2);
                GC0307_WriteReg(0x45, 0x27);
                GC0307_WriteReg(0x47, 0x2c);			
                DARK_SEQUENCE_0x47 = 0x2c;
                DARK_SEQUENCE_0x47_Effect = 0x3c;	
                break;
        case 2:  // IMAGE_H_MIRROR
                GC0307_WriteReg(0x0f, 0xa2);
                GC0307_WriteReg(0x45, 0x26);
                GC0307_WriteReg(0x47, 0x28);			
                DARK_SEQUENCE_0x47 = 0x28;
                DARK_SEQUENCE_0x47_Effect = 0x38;	
                break;		  
        case 3:  // IMAGE_V_MIRROR
                GC0307_WriteReg(0x0f, 0x92);
                GC0307_WriteReg(0x45, 0x25);
                GC0307_WriteReg(0x47, 0x24);			
                DARK_SEQUENCE_0x47 = 0x24;
                DARK_SEQUENCE_0x47_Effect = 0x34;	
                break;	
        case 4:  // IMAGE_HV_MIRROR
                GC0307_WriteReg(0x0f, 0x82);
                GC0307_WriteReg(0x45, 0x24);
                GC0307_WriteReg(0x47, 0x20);			
                DARK_SEQUENCE_0x47 = 0x20;
                DARK_SEQUENCE_0x47_Effect = 0x30;	
                break;	
        default:
                break;
        } 
}

LOCAL uint32_t hmirror_enable(uint32_t param)
{
         return SENSOR_SUCCESS; 
}
LOCAL uint32_t vmirror_enable(uint32_t param)
{
        return SENSOR_SUCCESS; 
}
const SENSOR_REG_T GC0307_brightness_tab[][2]=
{
        {		
                {0x7a, 0xd0},
                {0xff,0xff},
        },

        {
                {0x7a, 0xe0},
                {0xff,0xff},
        },

        {
                {0x7a, 0xf0},
                {0xff,0xff},
        },

        {
                {0x7a, 0x00},
                {0xff,0xff},
        },

        {
                {0x7a, 0x20},
                {0xff,0xff},
        },

        {
                {0x7a, 0x30},
                {0xff,0xff},
        },

        {
                {0x7a, 0x40},
                {0xff,0xff},
        },
};
LOCAL uint32_t set_brightness(uint32_t level)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)GC0307_brightness_tab[level];

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value) ; i++) {
                GC0307_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_PRINT("set_brightness: level = %d", level);
        return 0;
}

const SENSOR_REG_T GC0307_contrast_tab[][2]=
{
        {		
                {0xa1,0x50}, //\0x0e
                {0xff,0xff},
        },

        {
                {0xa1,0x48}, //\0x0e
                {0xff,0xff}, 
        },

        {
                {0xa1,0x44}, //\0x0e
                {0xff,0xff}, 
        },

        {
                {0xa1,0x40}, //\0x0e
                {0xff,0xff},
        },

        {
                {0xa1,0x3d}, //\0x0e
                {0xff,0xff}, 		
        },

        {
                {0xa1,0x38}, //\0x0e
                {0xff,0xff}, 	
        },

        {
                {0xa1,0x34}, //\0x0e
                {0xff,0xff},
        },	              
};

LOCAL uint32_t set_contrast(uint32_t level)
{
        uint16_t i;    

        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)GC0307_contrast_tab[level];

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value) ; i++)  {
                GC0307_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_PRINT("GC0307 set_contrast: level = %d", level);
        return 0;
}

LOCAL uint32_t set_preview_mode(uint32_t preview_mode)
{
        uint8_t temp = GC0307_ReadReg(0x41);

        if(preview_mode) {  // night
                GC0307_WriteReg(	0xdd  ,0x32);
                GC0307_WriteReg(	0x41  ,temp &~0x20);
                GC0307_WriteReg(	0xb0  ,0x10);
                GC0307_WriteReg(	0x21  ,0xf0);
        } else {  //normal
                GC0307_WriteReg(	0xdd  ,0x22);  //0x12
                GC0307_WriteReg(	0x41  ,temp | 0x20); 
                GC0307_WriteReg(	0x21  ,0xc0);
                GC0307_WriteReg(	0xd2  ,0x02);
                msleep(100);
                GC0307_WriteReg(	0xd2  ,0x61);		
        }
        SENSOR_PRINT("set preview mod %d",preview_mode);

        msleep(100);
        return 0;
} 

const SENSOR_REG_T GC0307_image_effect_tab[][17]=
{
        {
                {0x41,0x2f},			
                {0x40,0x7e},
                {0x42,0x10},
                {0x47,0x2c},
                {0x48,0xc3},
                {0x8a,0x50},//60
                {0x8b,0x50},
                {0x8c,0x07},
                {0x50,0x0c},
                {0x77,0x80},
                {0xa1,0x40},
                {0x7a,0x00},
                {0x78,0x00},
                {0x79,0x00},
                {0x7b,0x40},
                {0x7c,0x00},
                {0xff,0xff},
        },

        {
                {0x41,0x2f},			
                {0x40,0x7e},
                {0x42,0x10},
                {0x47,0x3c},
                {0x48,0xc3},
                {0x8a,0x60},
                {0x8b,0x60},
                {0x8c,0x07},
                {0x50,0x0c},
                {0x77,0x80},
                {0xa1,0x40},
                {0x7a,0x00},
                {0x78,0x00},
                {0x79,0x00},
                {0x7b,0x40},
                {0x7c,0x00},	
                {0xff,0xff},
        },

        {
                {0x41,0x2f},	
                {0x40,0x7e},
                {0x42,0x10},
                {0x47,0x3c},
                {0x48,0xc3},
                {0x8a,0x60},
                {0x8b,0x60},
                {0x8c,0x07},
                {0x50,0x0c},
                {0x77,0x80},
                {0xa1,0x40},
                {0x7a,0x00},
                {0x78,0x20},
                {0x79,0x70},
                {0x7b,0x40},
                {0x7c,0x00},	
                {0xff,0xff},		
        },	

        {
                {0x41,0x2f},			
                {0x40,0x7e},
                {0x42,0x10},
                {0x47,0x3c},
                {0x48,0xc3},
                {0x8a,0x60},
                {0x8b,0x60},
                {0x8c,0x07},
                {0x50,0x0c},
                {0x77,0x80},
                {0xa1,0x40},
                {0x7a,0x00},
                {0x78,0xc0},
                {0x79,0xc0},
                {0x7b,0x40},
                {0x7c,0x00},
                {0xff,0xff},
        },

        {
                {0x41,0x2f},			
                {0x40,0x7e},
                {0x42,0x10},
                {0x47,0x3c},
                {0x48,0xc3},
                {0x8a,0x60},
                {0x8b,0x60},
                {0x8c,0x07},
                {0x50,0x0c},
                {0x77,0x80},
                {0xa1,0x40},
                {0x7a,0x00},
                {0x78,0x70},
                {0x79,0x00},
                {0x7b,0x3f},
                {0x7c,0xf5},
                {0xff,0xff},
        },

        {
                {0x41,0x2f},			
                {0x40,0x7e},
                {0x42,0x10},
                {0x47,0x3c},
                {0x48,0xc3},
                {0x8a,0x60},
                {0x8b,0x60},
                {0x8c,0x07},
                {0x50,0x0c},
                {0x77,0x80},
                {0xa1,0x40},
                {0x7a,0x00},
                {0x78,0x80},
                {0x79,0x10},
                {0x7b,0x40},
                {0x7c,0x00},	
                {0xff,0xff},
        },

        {
                {0x41,0x6f},		
                {0x40,0x7e},
                {0x42,0x10},
                {0x47,0x2c},
                {0x48,0xc3},
                {0x8a,0x60},
                {0x8b,0x60},
                {0x8c,0x07},
                {0x50,0x0c},
                {0x77,0x80},
                {0xa1,0x40},
                {0x7a,0x00},
                {0x78,0x00},
                {0x79,0x00},
                {0x7b,0x40},
                {0x7c,0x00},
                {0xff,0xff},
        },

        {
                {0x41,0x2f},			
                {0x40,0x7e},
                {0x42,0x10},
                {0x47,0x3c},
                {0x48,0xc3},
                {0x8a,0x60},
                {0x8b,0x60},
                {0x8c,0x07},
                {0x50,0x0c},
                {0x77,0x80},
                {0xa1,0x40},
                {0x7a,0x00},
                {0x78,0xc0},
                {0x79,0x20},
                {0x7b,0x40},
                {0x7c,0x00},	
                {0xff,0xff},
        }
};

LOCAL uint32_t set_image_effect(uint32_t effect_type)
{
        uint16_t i;
        uint8_t temp_reg47 = GC0307_ReadReg(0x47);
        uint8_t temp_reg41 = GC0307_ReadReg(0x41);	

        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)GC0307_image_effect_tab[effect_type];

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value) ; i++) {
                if(0x47 == sensor_reg_ptr[i].reg_addr) {
                        if((0x00 == effect_type)  || (0x06 == effect_type))  // NORMAL&color INV
                                GC0307_WriteReg(0x47, temp_reg47&~0x10);
                        else
                                GC0307_WriteReg(0x47, temp_reg47|0x10);
                } else if(0x41 == sensor_reg_ptr[i].reg_addr) {
                        if(0x06 == effect_type)   // color INV
                                GC0307_WriteReg(0x41, temp_reg41|0x40);
                        else
                                GC0307_WriteReg(0x41, temp_reg41&~0x40);
                } else {
                        GC0307_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
                }
        }
        SENSOR_PRINT("-----------set_image_effect: effect_type = %d------------", effect_type);
        return 0;
}

LOCAL uint32_t GC0307_before_snapshot(uint32_t para)
{
#if 0        // For FAE adjusting!
        uint8 reg_val = 0;
        uint16 exposal_time=0x00;

        reg_val = GC0307_ReadReg(0x41);
        reg_val = reg_val & 0xf3;

        GC0307_WriteReg(0x41,reg_val);   // close aec awb

        reg_val = GC0307_ReadReg(0x04);
        exposal_time=reg_val&0x00ff;
        reg_val = GC0307_ReadReg(0x03);
        exposal_time|=((reg_val&0x00ff)<<0x08);

        exposal_time=exposal_time*2/3;

        if(exposal_time<2)
        {
        	exposal_time=2;
        }

        reg_val=exposal_time&0x00ff;
        GC0307_WriteReg(0x04, reg_val);
        reg_val=(exposal_time&0xff00)>>0x08;
        GC0307_WriteReg(0x03, reg_val);


        GC0307_WriteReg(0x01,0xfa);
        GC0307_WriteReg(0x02,0x70);
        GC0307_WriteReg(0x10,0x01);
        GC0307_WriteReg(0xd6,0x64);


        Delayms(400);	
#endif
        SENSOR_PRINT("SENSOR_GC0307: Before Snapshot");

        return 0;
}

LOCAL uint32_t GC0307_after_snapshot(uint32_t para)
{
#if 0    // for FAE adjusting!
        GC0307_WriteReg(0x01,0x6a);
        GC0307_WriteReg(0x02,0x25);
        GC0307_WriteReg(0x10,0x00);
        GC0307_WriteReg(0xd6,0x4B);
        GC0307_WriteReg(0x41,0x2f);

        Delayms(400);
#endif
        SENSOR_PRINT("SENSOR: After Snapshot");

        return 0;
}

//@ Chenfeng for adding AWB & AE functions
const SENSOR_REG_T GC0307_awb_tab[][5]=
{
        //AUTO
        {
                {0xc7, 0x4c},
                {0xc8, 0x40},
                {0xc9, 0x4a},
                {0x41, 0x04},
                {0xff, 0xff}
        },    
        //INCANDESCENCE:
        {
                {0x41, 0x04},
                {0xc7, 0x48},
                {0xc8, 0x40},        
                {0xc9, 0x5c},
                {0xff, 0xff} 
        },
        //U30
        {
                {0x41, 0x04},   // Enable AWB 
                {0xc7, 0x40},
                {0xc8, 0x54},
                {0xc9, 0x70},
                {0xff, 0xff} 
        },  
        //CWF  //???????
        {
                {0x41, 0x04},   // Enable AWB 
                {0xc7, 0x40},
                {0xc8, 0x54},
                {0xc9, 0x70},
                {0xff, 0xff} 
        },    
        //FLUORESCENT:
        {
                {0x41, 0x04},   // Enable AWB 
                {0xc7, 0x40},
                {0xc8, 0x42},
                {0xc9, 0x50},
                {0xff, 0xff} 
        },
        //SUN:
        {
                {0x41, 0x04},   // Enable AWB 
                {0xc7, 0x50},
                {0xc8, 0x45},
                {0xc9, 0x40},
                {0xff, 0xff} 
        },
        //CLOUD:
        {
                {0x41, 0x04},   // Enable AWB 
                {0xc7, 0x5a}, //WB_manual_gain
                {0xc8, 0x42},
                {0xc9, 0x40},
                {0xff, 0xff} 
        },
};

LOCAL uint32_t set_GC0307_awb(uint32_t mode)
{
        uint8_t temp_value;
        uint16_t i;

        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)GC0307_awb_tab[mode];

        temp_value = GC0307_ReadReg(0x41);

        if(mode >= 7) {
                SENSOR_PRINT("set_GC0307_awb:param error,mode=%d .\n",mode);
                return SENSOR_FAIL;
        }
	
        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                if(0x41 == sensor_reg_ptr[i].reg_addr) {
                        if(mode == 0)
                                GC0307_WriteReg(0x41, temp_value |0x04 );
                        else
                                GC0307_WriteReg(0x41, temp_value &~0x04 );
                } else {
                        GC0307_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
                }
        }
        msleep(100); 
        SENSOR_PRINT("SENSOR: set_awb_mode: mode = %d", mode);
        return 0;
}
#if 0
LOCAL uint32_t set_GC0307_ae_enable(uint32_t enable)
{
#define AE_ENABLE 0x41
	unsigned char ae_value;

	ae_value=GC0307_ReadReg(AE_ENABLE);

	if(0x00==enable)
	{
		ae_value&=0xf7;
		GC0307_WriteReg(AE_ENABLE,ae_value);
	}
	else if(0x01==enable)
	{
		ae_value|=0x08;
		GC0307_WriteReg(AE_ENABLE,ae_value);
	}

	SENSOR_PRINT("SENSOR: set_ae_enable: enable = %d", enable);

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
const SENSOR_REG_T GC0307_video_mode_nand_tab[][8]=
{
        // normal mode
        {
                {0x01,0x6a},{0x02,0x32},{0x10,0x20},{0xd6,0x96},{0x2c,0x04},{0x2d,0x1a}, {0xdd,0x2},{0xff, 0xff} 
        },    
        //vodeo mode
        {
                {0x01,0x6a},{0x02,0x70},{0x10,0x00},{0xd6,0x96},{0x2c,0x02},{0x2d,0x58}, {0xdd,0x2},{0xff, 0xff}      
        }
};
/******************************************************************************/
// Description: set video mode
// Global resource dependence: 
// Author:
// Note:
//		 
/******************************************************************************/
const SENSOR_REG_T GC0307_video_mode_nor_tab[][13]=
{
        // normal mode
        {
                {0x01  ,0x6a}, 
                {0x02  ,0x25}, 
                {0x10  ,0x00}, 
                {0xd6  ,0x4b}, 
                {0x28  ,0x02}, //AEC_exp_level_1bit11to8   
                {0x29  ,0x0d}, //AEC_exp_level_1bit7to0	  
                {0x2a  ,0x02}, //AEC_exp_level_2bit11to8   
                {0x2b  ,0x0d}, //AEC_exp_level_2bit7to0			 
                {0x2c  ,0x02}, //AEC_exp_level_3bit11to8   659 - 8FPS,  8ca - 15FPS  //	 
                {0x2d  ,0x0d}, //AEC_exp_level_3bit7to0			 
                {0x2e  ,0x05}, //AEC_exp_level_4bit11to8  8FPS 
                {0x2f  ,0xdc}, //AEC_exp_level_4bit7to0	
                {0xff  ,0xff}
        },    
        //vodeo mode
        {
                {0x01  ,0x32}, 
                {0x02  ,0x70}, 
                {0x10  ,0x01}, 
                {0xd6  ,0x3c}, 
                {0x28  ,0x02}, //AEC_exp_level_1bit11to8   
                {0x29  ,0x58}, //AEC_exp_level_1bit7to0	  
                {0x2a  ,0x02}, //AEC_exp_level_2bit11to8   
                {0x2b  ,0x58}, //AEC_exp_level_2bit7to0			 
                {0x2c  ,0x02}, //AEC_exp_level_3bit11to8   659 - 8FPS,  8ca - 15FPS  //	 
                {0x2d  ,0x58}, //AEC_exp_level_3bit7to0			 
                {0x2e  ,0x05}, //AEC_exp_level_4bit11to8  8FPS 
                {0x2f  ,0xa0}, //AEC_exp_level_4bit7to0	
                {0xff  ,0xff}
        },
        //upcc mode
        {
                {0x01  ,0x32}, 
                {0x02  ,0x70}, 
                {0x10  ,0x01}, 
                {0xd6  ,0x3c}, 
                {0x28  ,0x02}, //AEC_exp_level_1bit11to8   
                {0x29  ,0x58}, //AEC_exp_level_1bit7to0	  
                {0x2a  ,0x02}, //AEC_exp_level_2bit11to8   
                {0x2b  ,0x58}, //AEC_exp_level_2bit7to0			 
                {0x2c  ,0x02}, //AEC_exp_level_3bit11to8   659 - 8FPS,  8ca - 15FPS  //	 
                {0x2d  ,0x58}, //AEC_exp_level_3bit7to0			 
                {0x2e  ,0x05}, //AEC_exp_level_4bit11to8  8FPS 
                {0x2f  ,0xa0}, //AEC_exp_level_4bit7to0	
                {0xff  ,0xff}
        } 
};    

#define ov7670_video_mode_wqvga_nand_tab GC0307_video_mode_nor_tab

LOCAL uint32_t set_GC0307_video_mode(uint32_t mode)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = PNULL;   
        sensor_reg_ptr = (SENSOR_REG_T*)GC0307_video_mode_nor_tab[mode];

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                GC0307_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_PRINT("SENSOR: set_video_mode: mode = %d,bl_GC_50Hz_GC0307=%d", mode,bl_GC_50Hz_GC0307);
        if(1==mode ||2==mode ) {
                if(bl_GC_50Hz_GC0307) {  // is 50hz in video
                        GC0307_WriteReg(0x01,0x32);      
                        GC0307_WriteReg(0x02,0x70);    
                        GC0307_WriteReg(0x10,0x01);    
                        GC0307_WriteReg(0xd6,0x3c);    
                        GC0307_WriteReg(0x28,0x02);    
                        GC0307_WriteReg(0x29,0x58);    
                        GC0307_WriteReg(0x2a,0x02);    
                        GC0307_WriteReg(0x2b,0x58);    
                        GC0307_WriteReg(0x2c,0x02);    
                        GC0307_WriteReg(0x2d,0x58);    
                        GC0307_WriteReg(0x2e,0x05);    
                        GC0307_WriteReg(0x2f,0xdc);    
                } else { // is 60hz in video mode
                        GC0307_WriteReg(0x01,0x32);      
                        GC0307_WriteReg(0x02,0x70);    
                        GC0307_WriteReg(0x10,0x01);    
                        GC0307_WriteReg(0xd6,0x32);    
                        GC0307_WriteReg(0x28,0x02);    
                        GC0307_WriteReg(0x29,0x58);    
                        GC0307_WriteReg(0x2a,0x02);    
                        GC0307_WriteReg(0x2b,0x58);    
                        GC0307_WriteReg(0x2c,0x02);    
                        GC0307_WriteReg(0x2d,0x58);    
                        GC0307_WriteReg(0x2e,0x05);    
                        GC0307_WriteReg(0x2f,0xdc);  
                }
        }
        SENSOR_PRINT("SENSOR: set_video_mode: mode = %d", mode);
        return 0;
}

const SENSOR_REG_T GC0307_ev_tab[][2]=
{   
        {{0xd1, 0x18}, {0xff, 0xff}},
        {{0xd1, 0x28}, {0xff, 0xff}},
        {{0xd1, 0x38}, {0xff, 0xff}},
        {{0xd1, 0x48}, {0xff, 0xff}},
        {{0xd1, 0x58}, {0xff, 0xff}},
        {{0xd1, 0x68}, {0xff, 0xff}},
        {{0xd1, 0x78}, {0xff, 0xff}},
};
LOCAL uint32_t set_GC0307_ev(uint32_t level)
{
        uint16_t i; 
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)GC0307_ev_tab[level];

        if(level >= 7) {
                SENSOR_PRINT("set_GC0307_ev:param error,level=%d .\n",level);
        }

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value) ; i++) {
                GC0307_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        msleep(10);
        SENSOR_PRINT("SENSOR: set_ev: level = %d", level);
        return 0;
}
/******************************************************************************/
// Description: anti 50/60 hz banding flicker
// Global resource dependence: 
// Author:
// Note:
//		mode  must smaller than 2
/******************************************************************************/
LOCAL uint32_t set_GC0307_AntiFlicker(uint32_t mode)
{//24m->65.6 us 12m->131us
        switch(mode) {
        case 0://DCAMERA_FLICKER_50HZ:
                bl_GC_50Hz_GC0307 = SENSOR_TRUE;
                GC0307_WriteReg(0x01,0xe5);      
                GC0307_WriteReg(0x02,0x20);    
                GC0307_WriteReg(0x10,0x00);    
                GC0307_WriteReg(0xd6,0x41);    
                GC0307_WriteReg(0x28,0x02);    
                GC0307_WriteReg(0x29,0x08);    
                GC0307_WriteReg(0x2a,0x02);    
                GC0307_WriteReg(0x2b,0x08);    
                GC0307_WriteReg(0x2c,0x02);    
                GC0307_WriteReg(0x2d,0x08);    
                GC0307_WriteReg(0x2e,0x06);    
                GC0307_WriteReg(0x2f,0x59);    
                break;
        case 1://DCAMERA_FLICKER_60HZ:
                bl_GC_50Hz_GC0307 = SENSOR_FALSE;
                GC0307_WriteReg(0x01,0xe8);      
                GC0307_WriteReg(0x02,0x34);    
                GC0307_WriteReg(0x10,0x00);    
                GC0307_WriteReg(0xd6,0x36);    
                GC0307_WriteReg(0x28,0x02);    
                GC0307_WriteReg(0x29,0x1c);    
                GC0307_WriteReg(0x2a,0x02);    
                GC0307_WriteReg(0x2b,0x1c);    
                GC0307_WriteReg(0x2c,0x02);    
                GC0307_WriteReg(0x2d,0x1c);    
                GC0307_WriteReg(0x2e,0x06);    
                GC0307_WriteReg(0x2f,0x54);                       
                break;
        default:
                break;
        }
        msleep(100); 
        SENSOR_PRINT("SENSOR: set_GC0307_AntiFlicker: mode = %d", mode);
        return 0;
}

LOCAL SENSOR_REG_TAB_INFO_T s_GC0307_resolution_Tab_YUV[]=
{   
        // COMMON INIT
        { ADDR_AND_LEN_OF_ARRAY(GC0307_YVU_640X480),    640,    480, 12,  0 },
        // YUV422 PREVIEW 1
        { PNULL, 0, 640, 480, 12, 0 },
        { PNULL,   0,  0,  0, 0 ,0   },   
        { PNULL,   0,  0,  0, 0, 0   },
        { PNULL,   0,  0,  0, 0, 0   },
        // YUV422 PREVIEW 2 
        { PNULL,  0, 0,  0,  0,  0 },
        { PNULL,  0, 0,  0,  0,  0 },
        { PNULL,  0, 0,  0,  0,  0 },
        { PNULL,  0, 0,  0,  0,  0 }
};
LOCAL SENSOR_IOCTL_FUNC_TAB_T s_GC0307_ioctl_func_tab = 
{
        // Internal 
        PNULL,
        PNULL,
        PNULL,
        GC0307_Identify,

        PNULL,          // write register
        PNULL,          // read  register   
        PNULL,
        PNULL,

        // External
        PNULL,//set_ae_enable,
        hmirror_enable, //set_hmirror_enable,
        vmirror_enable, //set_vmirror_enable,
        set_brightness,   //11

        set_contrast,
        PNULL, 
        PNULL, 
        set_preview_mode,   //15

        set_image_effect,    //16  
        GC0307_before_snapshot,
        GC0307_after_snapshot,
        PNULL,

        PNULL,
        PNULL,
        PNULL,
        PNULL,

        PNULL,
        PNULL,
        PNULL,
        PNULL,

        set_GC0307_awb,   //28   
        PNULL, 
        PNULL, 
        set_GC0307_ev,   //31

        PNULL, 
        PNULL, 
        PNULL,     
        PNULL,

        PNULL,
        set_GC0307_AntiFlicker,
        set_GC0307_video_mode,    
        PNULL,
};

SENSOR_INFO_T g_GC0307_yuv_info =
{
        GC0307_I2C_ADDR_W,				// salve i2c write address
        GC0307_I2C_ADDR_R, 				// salve i2c read address

        0,								// bit0: 0: i2c register value is 8 bit, 1: i2c register value is 16 bit
        						// bit2: 0: i2c register addr  is 8 bit, 1: i2c register addr  is 16 bit
        						// other bit: reseved
        SENSOR_HW_SIGNAL_PCLK_P|\
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

        SENSOR_LOW_PULSE_RESET,		// reset pulse level
        100,								// reset pulse width(ms)

        SENSOR_HIGH_LEVEL_PWDN,			// 1: high level valid; 0: low level valid

        2,								// count of identify code
        {{0x00, 0x99},						// supply two code to identify sensor.
        {0x00, 0x99}},						// for Example: index = 0-> Device id, index = 1 -> version id											
        								
        SENSOR_AVDD_2800MV,				// voltage of avdd	

        640,							// max width of source image
        480,							// max height of source image
        "GC0307",						// name of sensor												

        SENSOR_IMAGE_FORMAT_YUV422,		// define in SENSOR_IMAGE_FORMAT_E enum,
        						// if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T
        SENSOR_IMAGE_PATTERN_YUV422_YUYV,	// pattern of input image form sensor;			

        s_GC0307_resolution_Tab_YUV,	// point to resolution table information structure
        &s_GC0307_ioctl_func_tab,		// point to ioctl function table

        PNULL,							// information and table about Rawrgb sensor
        PNULL,							// extend information about sensor	
        SENSOR_AVDD_2800MV,                     // iovdd
        SENSOR_AVDD_1800MV,                      // dvdd
        2,                     // skip frame num before preview 
        2,                      // skip frame num before capture
        0,                      // deci frame num during preview	
        2,                       // deci frame num during video preview
        0,
        0,
        0,
        0
};
#endif

struct sensor_drv_cfg sensor_gc0307 = {
        .sensor_pos = CONFIG_DCAM_SENSOR_POS_GC0307,
        .sensor_name = "gc0307",
        .driver_info = &g_GC0307_yuv_info,
};

static int __init sensor_gc0307_init(void)
{
        return dcam_register_sensor_drv(&sensor_gc0307);
}

subsys_initcall(sensor_gc0307_init);

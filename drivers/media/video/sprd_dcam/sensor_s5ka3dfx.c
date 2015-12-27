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
#define S5KA3DFX_I2C_ADDR_W		0x62//0xC4
#define S5KA3DFX_I2C_ADDR_R		0x62//0xC5
#define SENSOR_GAIN_SCALE			16
#define S5KA3DFX_SIGNAL_PCLK_PHASE               0x00
#define S5KA3DFX_SWITCH_PAGE_ID          0xEF // modify this by different sensor chris.shen 20101126
#define S5KA3DFX_PAGE0_ID                        0x00
#define S5KA3DFX_PAGE1_ID                        0x01
#define S5KA3DFX_PAGE2_ID                        0x02
#define S5KA3DFX_PAGE3_ID                        0x03
#define S5KA3DFX_PAGE4_ID                        0x04 
/**---------------------------------------------------------------------------*
 ** 					Local Function Prototypes							  *
 **---------------------------------------------------------------------------*/
//LOCAL uint32_t set_s5ka3dfx_ae_enable(uint32_t enable);
//LOCAL uint32_t set_hmirror_enable(uint32_t enable);
//LOCAL uint32_t set_vmirror_enable(uint32_t enable);
LOCAL uint32_t s5ka3dfx_set_preview_mode(uint32_t preview_mode);
LOCAL uint32_t s5ka3dfx_Identify(uint32_t param);
//LOCAL uint32_t s5ka3dfx_BeforeSnapshot(uint32_t param);
LOCAL uint32_t s5ka3dfx_After_Snapshot(uint32_t param);
LOCAL uint32_t s5ka3dfx_set_brightness(uint32_t level);
//LOCAL uint32_t s5ka3dfx_set_contrast(uint32_t level);
//LOCAL uint32_t set_sharpness(uint32_t level);
//LOCAL uint32_t set_saturation(uint32_t level);
LOCAL uint32_t s5ka3dfx_set_image_effect(uint32_t effect_type);
//LOCAL uint32_t read_ev_value(uint32_t value);
//LOCAL uint32_t write_ev_value(uint32_t exposure_value);
//LOCAL uint32_t read_gain_value(uint32_t value);
//LOCAL uint32_t write_gain_value(uint32_t gain_value);
//LOCAL uint32_t read_gain_scale(uint32_t value);
//LOCAL uint32_t set_frame_rate(uint32_t param);
LOCAL uint32_t s5ka3dfx_set_work_mode(uint32_t mode);
LOCAL uint32_t s5ka3dfx_ev_compensation(uint32_t ev_mode);
//LOCAL uint32_t s5ka3dfx_set_flicker_elimination(uint32_t flicker_mode);
//LOCAL uint32_t s5ka3dfx_set_iso(uint32_t iso_type);
LOCAL uint32_t s5ka3dfx_get_ev(uint32_t param);
LOCAL uint32_t s5ka3dfx_set_wb_mode(uint32_t wb_mode);
LOCAL uint32_t _s5ka3dfx_Power_On(uint32_t power_on);
LOCAL uint32_t _s5ka3dfx_Reset(uint32_t param);
//LOCAL uint32_t s5ka3dfx_set_meter_mode(uint32_t param);
LOCAL uint32_t s_preview_mode;
/**---------------------------------------------------------------------------*
 ** 						Local Variables 								 *
 **---------------------------------------------------------------------------*/

// init seq 
const SENSOR_REG_T s5ka3dfx_YUV_640X480[]=
{
{0xef,0x02},                                                             
{0x13,0xa0}, //ob sel(r reference)                                       
{0x23,0x53}, //tx_width                                                  
{0x26,0x24}, //clp level                                                 
{0x2c,0x05}, //s1s                                                       
{0x05,0x00}, //s1s end value                                             
{0x03,0x58},

{0x24,0x0a}, //cds //1a                                                  
                                                                   
{0x0b,0x84}, //analog offset                                             
{0x1e,0xb7}, //global gain                                               
{0x56,0x05}, //adlc                                                      
{0x28,0x96}, //cfpn    
{0x67,0x3c}, //or 3ch, or 38h //to reduce hn at low lux                                                 

{0xef,0x03},                                                             
{0x50,0xd2}, //e6,d2 mclk :24MHz
                                                              
{0x0f,0x31}, //add vblank value 

{0xEF,0x03},
{0x5F,0x03},	//NT Cintr Max
{0x60,0x02},	//PAL Cintr Max
{0x61,0x0F},	//NT shutter Max (FrameAE mode)
{0x62,0x0C},	//PAL shutter Max (FrameAE mode)
{0x63,0x01},	//02, NT Vblank
{0x64,0xE7},	//39
{0x65,0x01},	//02, PAL Vblank
{0x66,0xE7},	//39                                                          

{0x6d,0x58},	//ll_agc_b
{0x6e,0x90},	//dgain min
{0x6f,0x90},	//dgain max                                            

{0x4c,0x00},    //pal hblank
{0x4d,0x9e},                                                            
                                                             
{0xef,0x03},                                                           
{0x00,0x87}, //ae, awb, flicker on
{0x01,0x80},
{0x02,0x7f},
                                                             
{0x2b,0x41}, //auto e-shutter enable 
                                                              
{0x31,0x00}, //nl_bt(brightness)                                            
{0x32,0x0a}, //8 ll_bt                                                           
                                                                    
{0x33,0x80}, //ct(contrast)                                              
{0x34,0x79},                                                            
                                                                   
{0x36,0x3C}, //nl_st(saturation)_gain
{0x37,0x33}, //30 ll_st_gain
                                                                   
{0x6a,0x00}, //bpr                                                       
{0x7b,0x05},                                                             
{0x38,0x05},                                                             
{0x39,0x03},                                                             

{0x2d,0x08}, //0a //08 //nr
{0x2e,0x20}, //20
{0x2f,0x1e}, //30 //2a //20 ll_nr_gain
{0x30,0xff}, //ff //50 ll_nr_edge
{0x7c,0x06}, //06
{0x7d,0x20}, //20
{0x7e,0x0c}, //0c
{0x7f,0x20}, //20            

{0x28,0x02}, //02 //sharpness                                                 
{0x29,0x8f},                                                             
{0x2a,0x00}, 

{0xef,0x03}, 
{0x13,0x00}, //awb outdoor cintr limi                                    
{0x14,0x9f},
{0x19,0x43}, 
                                                              
{0x1a,0x5e}, //5f outdoor rgain max                                         
{0x1b,0x59}, //5b outdoor rgain min                                         
{0x1c,0x5e}, //63 outdoor bgain max                                         
{0x1d,0x50}, //outdoor bgain min                                         

{0x1e,0x6b}, //indoor rgain max                                          
{0x1F,0x45},  //48 Indoor Rgain Min  3f
{0x20,0x79},  //79 Indoor Bgain Max  8c
{0x21,0x4C}, // //4c Indoor Bgain Min                                       
                                                                      
{0x3a,0x13}, //14 //[7:4]awb speed, [3:0] awb threshold                       
{0x3b,0x3e}, //awb skip brt <= same value with ae target<3. 01>          
{0x3c,0x00}, //awb skip gain                                             
{0x3d,0x18}, //awb skip avg                                              

{0x23,0x80}, //awb window select                                         
                                   
{0x15,0x0b}, //awb cnt                                                   
{0x16,0xd2},                                                             
{0x17,0x64},                                                             
{0x18,0x78},  

  //page 00                                                                  
{0xef,0x00}, //x shade                                                   
{0xde,0x00},                                                             
{0xdf,0x1f},                                                             
{0xe0,0x00},                                                             
{0xe1,0x37},                                                             
{0xe2,0x08},                                                             
{0xe3,0x42},                                                             
{0xe4,0x00},                                                             
{0xe5,0x12},                                                             
{0xe6,0x9e},                                                             
{0xe9,0x00},                                                             
{0xe7,0x00},                                                             
{0xe8,0x6d},                                                             
{0xe9,0x01},                                                             
{0xe7,0x00},                                                             
{0xe8,0x81},                                                             
{0xe9,0x02},                                                             
{0xe7,0x00},                                                             
{0xe8,0x85},                                                             
{0xe9,0x03},                                                             
{0xe7,0x00},                                                             
{0xe8,0x84},                                                             
{0xe9,0x04},                                                             
{0xe7,0x00},                                                             
{0xe8,0x9d},                                                             
{0xe9,0x05},                                                             
{0xe7,0x00},                                                             
{0xe8,0x97},                                                             
{0xe9,0x06},                                                             
{0xe7,0x00},                                                             
{0xe8,0xb4},                                                             
{0xe9,0x07},                                                             
{0xe7,0x00},                                                             
{0xe8,0x25},                                                             
{0xe9,0x08},                                                             
{0xe7,0x00},                                                             
{0xe8,0x46},                                                             
{0xe9,0x09},                                                             
{0xe7,0x00},                                                             
{0xe8,0x3d},                                                             
{0xe9,0x0a},                                                             
{0xe7,0x00},                                                             
{0xe8,0x3a},                                                             
{0xe9,0x0b},                                                             
{0xe7,0x00},                                                             
{0xe8,0x4e},                                                             
{0xe9,0x0c},                                                             
{0xe7,0x00},                                                             
{0xe8,0x4f},                                                             
{0xe9,0x0d},                                                             
{0xe7,0x00},                                                             
{0xe8,0x68},                                                             
{0xe9,0x0e},                                                             
{0xe7,0x07},                                                             
{0xe8,0xed},                                                             
{0xe9,0x0f},                                                             
{0xe7,0x00},                                                             
{0xe8,0x02},                                                             
{0xe9,0x10},                                                             
{0xe7,0x07},                                                             
{0xe8,0xfd},                                                             
{0xe9,0x11},                                                             
{0xe7,0x07},                                                             
{0xe8,0xf0},                                                             
{0xe9,0x12},                                                             
{0xe7,0x00},                                                             
{0xe8,0x07},                                                             
{0xe9,0x13},                                                             
{0xe7,0x00},                                                             
{0xe8,0x02},                                                             
{0xe9,0x14},                                                             
{0xe7,0x00},                                                             
{0xe8,0x1c},                                                             
{0xe9,0x15},                                                             
{0xe7,0x07},                                                             
{0xe8,0xab},                                                             
{0xe9,0x16},                                                             
{0xe7,0x07},                                                             
{0xe8,0xb3},                                                             
{0xe9,0x17},                                                             
{0xe7,0x07},                                                             
{0xe8,0xa6},                                                             
{0xe9,0x18},                                                             
{0xe7,0x07},                                                             
{0xe8,0x9a},                                                             
{0xe9,0x19},                                                             
{0xe7,0x07},                                                             
{0xe8,0xb4},                                                             
{0xe9,0x1a},                                                             
{0xe7,0x07},                                                             
{0xe8,0xb0},                                                             
{0xe9,0x1b},                                                             
{0xe7,0x07},                                                             
{0xe8,0xc7},                                                             
{0xe9,0x1c},                                                             
{0xe7,0x07},                                                             
{0xe8,0x62},                                                             
{0xe9,0x1d},                                                             
{0xe7,0x07},                                                             
{0xe8,0x64},                                                             
{0xe9,0x1e},                                                             
{0xe7,0x07},                                                             
{0xe8,0x59},
{0xe9,0x1f},                                                             
{0xe7,0x07},                                                             
{0xe8,0x4f},                                                             
{0xe9,0x20},                                                             
{0xe7,0x07},                                                             
{0xe8,0x58},                                                             
{0xe9,0x21},                                                             
{0xe7,0x07},                                                             
{0xe8,0x5f},                                                             
{0xe9,0x22},                                                             
{0xe7,0x07},                                                             
{0xe8,0x6f},                                                             
{0xe9,0x40},                                                            

{0xd1,0x08}, //00 ilkwon 0302 08=>00 yc order                                                  
{0xdd,0x03}, //x shade on                                                                                                                                                    

{0x23,0x17}, //grgb                                                      
{0x24,0x17},                                                             
{0x25,0x17},                                                             
{0x27,0x18}, //18                                                             
{0x29,0x60},                                                             
{0x2a,0x22}, //7b                                                             

{0x2f,0x01}, //intp_coef_sharpness                                                                                                                                                                                                            

{0x36,0x01}, //shading (r, g, b)                                         
{0x37,0x81},                                                             
{0x38,0x6c},                                                             
{0x39,0x5a},                                                             
{0x3a,0x00},                                                             
{0x3b,0xf0},                                                             
{0x3c,0x01},                                                             
{0x3d,0x54},                                                                                                                                                                                                                                                  

{0xb9,0x02}, //sharpness on                                              
{0xbb,0xb0}, //a8 //hpf
{0xbc,0x18}, //10
{0xbd,0x30},
{0xbf,0x38}, //18 //core
{0xc1,0x88}, //38                                                            

{0xc8,0x11}, //yc delay                                                  
{0xeb,0x81}, //edege color suppress control                                                       
{0xed,0x05}, //edge color suppress color gain slope                                                            

{0xb1,0x00}, //awb window                                                
{0xb2,0x62},                                                             
{0xb3,0x00},                                                             
{0xb4,0x00},                                                             
{0xb5,0x01},                                                             
{0xb6,0xa3},                                                             
{0xb7,0x02},                                                             
{0xb8,0x80},                                                             

{0x77,0x00}, //saturation sin                                            
{0x78,0x00}, //sat sin                                                                                                                                                                                                                                                                        
                                                           
{0x93,0x40}, //awb map                                                   
{0x94,0x80},                                                             
{0x95,0xc0},                                                             
{0x96,0xc0},                                                             
{0x97,0x20},                                                             
{0x98,0x20},                                                             
{0x99,0x30},                                                             
{0xa0,0x00},                                                             
{0xa1,0x00},                                                             
{0xa2,0x1c},                                                             
{0xa3,0x16},                                                             
{0xa4,0x03},                                                             
{0xa5,0x07},                                                             
{0xa6,0x00},                                                             
                                                                                                                               
{0xef,0x00},                                                                                                                             
{0xad,0xd0}, //awb up data                                               
{0xaf,0x10}, //26 //awb dn data                                               
                                                             
{0xef,0x00},     
{0x42,0x60}, //65
{0x44,0x60}, //62

{0x57,0x00}, //ae min skip

  //page 03                                                        
{0xef,0x03},                                                             
{0x01,0x3e}, //ae target                                                 
{0x02,0x05}, //ae threshold                                              
{0x03,0x20}, //ae step                                                   
{0x04,0x63}, //agc max of lowlux                                        
{0x06,0x1c}, //agc max of hl                                             
{0x07,0x01}, //ae win_a weight                                           
{0x08,0x01}, //ae win_b weight                                           
{0x0b,0x01}, //cintc max high

  //ae speed setting ----------------------------
{0x51,0x10}, //chip_dbg                                                  
{0x52,0x00},                                                             
{0x53,0x00},                                                             
{0x54,0x00},                                                             
{0x55,0x22},                                                             
{0x56,0x01},                                                             
{0x57,0x61},                                                             
{0x58,0x25},                                                             
{0x67,0xcf}, //ae speed
{0x69,0x17}, //ae row flag                                               

  //page 00                                                                 
  // AE Window - centerweighted
{0xef,0x00},
{0x58,0x00},                                                
{0x59,0x00},                                                             
{0x5a,0x02},                                                             
{0x5b,0x73},                                                             
{0x5c,0x00},                                                             
{0x5d,0x00},                                                            
{0x5e,0x01},                                                             
{0x5f,0xe0},                                                             
{0x60,0x00},                                                             
{0x61,0xae},                                                             
{0x62,0x01},                                                             
{0x63,0xbb},                                                             
{0x64,0x00},                                                             
{0x65,0x7e},                                                             
{0x66,0x01},                                                             
{0x67,0x8e},                                                             

  //flicker setting
{0x6a,0x01}, //080331 flicker h size high    
{0x6b,0xe0}, //080331 flicker h size low     
{0x6c,0x05}, //04 //080331 flicker window vsize 02
{0x6d,0x00}, //080331 flicker v size start h 
{0x6e,0x0e}, //080331 flicker v size start l 
{0x6f,0x00}, //080331 flicker h size start h 
{0x70,0x10}, //080331 flicker h size start l 


  //page 03
{0xef,0x03},
{0x22,0x24}, //23 //flicker sensitivity h/l
{0x3e,0x23},
{0x3f,0x23},
{0x40,0x00},
{0x41,0x09}, //60hz light - 50hz setting threshold
{0x4a,0x09}, //50hz light - 60hz setting threshold
{0x4b,0x04}, 
{0x5b,0x20}, //10 //detection haunting protection count 
{0x5d,0x35}, //55
{0x5e,0x13},
{0x78,0x0f},

  //page 00
{0xef,0x00}, 
{0x4c,0x80}, //ccm                                                       
{0x4d,0xbb},                                                             
{0x4e,0x84},                                                             
{0x4f,0x91},                                                             
{0x50,0x64},                                                             
{0x51,0x93},                                                             
{0x52,0x03},                                                             
{0x53,0xc7},                                                             
{0x54,0x83},                                                               
                                                                   
{0xef,0x03},
{0x6e,0x40},	//dgain min
{0x6f,0x6a},	//dgain max

{0xef,0x00},                                                                 
{0x48,0x00}, //gamma of r g b
{0x49,0x00}, 
{0x4a,0x03}, 
{0x48,0x01}, 
{0x49,0x00}, 
{0x4a,0x05},
{0x48,0x02},
{0x49,0x00},
{0x4a,0x39},
{0x48,0x03},
{0x49,0x00},
{0x4a,0xA1},
{0x48,0x04},
{0x49,0x01},
{0x4a,0x44},
{0x48,0x05},
{0x49,0x01},
{0x4a,0xBF},
{0x48,0x06},
{0x49,0x02},
{0x4a,0x27},
{0x48,0x07},
{0x49,0x02},
{0x4a,0x7E},
{0x48,0x08},
{0x49,0x02},
{0x4a,0xC6},
{0x48,0x09},
{0x49,0x03},
{0x4a,0x02},
{0x48,0x0A},
{0x49,0x03},
{0x4a,0x33},
{0x48,0x0B},
{0x49,0x03},
{0x4a,0x5E},
{0x48,0x0C},
{0x49,0x03},
{0x4a,0x83},
{0x48,0x0D},
{0x49,0x03},
{0x4a,0xA1},
{0x48,0x0E},
{0x49,0x03},
{0x4a,0xB8},
{0x48,0x0F},
{0x49,0x03},
{0x4a,0xCC},
{0x48,0x10},
{0x49,0x03},
{0x4a,0xE0},
{0x48,0x11},
{0x49,0x03},
{0x4a,0xF0},
{0x48,0x12},
{0x49,0x03},
{0x4a,0xFF},

{0x48,0x20},
{0x49,0x00},
{0x4a,0x03},
{0x48,0x21},
{0x49,0x00},
{0x4a,0x05},
{0x48,0x22},
{0x49,0x00},
{0x4a,0x39},
{0x48,0x23},
{0x49,0x00},
{0x4a,0xA1},
{0x48,0x24},
{0x49,0x01},
{0x4a,0x44},
{0x48,0x25},
{0x49,0x01},
{0x4a,0xBF},
{0x48,0x26},
{0x49,0x02},
{0x4a,0x27},
{0x48,0x27},
{0x49,0x02},
{0x4a,0x7E},
{0x48,0x28},
{0x49,0x02},
{0x4a,0xC6},
{0x48,0x29},
{0x49,0x03},
{0x4a,0x02},
{0x48,0x2A},
{0x49,0x03},
{0x4a,0x33},
{0x48,0x2B},
{0x49,0x03},
{0x4a,0x5E},
{0x48,0x2C},
{0x49,0x03},
{0x4a,0x83},
{0x48,0x2D},
{0x49,0x03},
{0x4a,0xA1},
{0x48,0x2E},
{0x49,0x03},
{0x4a,0xB8},
{0x48,0x2F},
{0x49,0x03},
{0x4a,0xCC},
{0x48,0x30},
{0x49,0x03},
{0x4a,0xE0},
{0x48,0x31},
{0x49,0x03},
{0x4a,0xF0},
{0x48,0x32},
{0x49,0x03},
{0x4a,0xFF},

{0x48,0x40},
{0x49,0x00},
{0x4a,0x03},
{0x48,0x41},
{0x49,0x00},
{0x4a,0x05},
{0x48,0x42},
{0x49,0x00},
{0x4a,0x39},
{0x48,0x43},
{0x49,0x00},
{0x4a,0xA1},
{0x48,0x44},
{0x49,0x01},
{0x4a,0x44},
{0x48,0x45},
{0x49,0x01},
{0x4a,0xBF},
{0x48,0x46},
{0x49,0x02},
{0x4a,0x27},
{0x48,0x47},
{0x49,0x02},
{0x4a,0x7E},
{0x48,0x48},
{0x49,0x02},
{0x4a,0xC6},
{0x48,0x49},
{0x49,0x03},
{0x4a,0x02},
{0x48,0x4A},
{0x49,0x03},
{0x4a,0x33},
{0x48,0x4B},
{0x49,0x03},
{0x4a,0x5E},
{0x48,0x4C},
{0x49,0x03},
{0x4a,0x83},
{0x48,0x4D},
{0x49,0x03},
{0x4a,0xA1},
{0x48,0x4E},
{0x49,0x03},
{0x4a,0xB8},
{0x48,0x4F},
{0x49,0x03},
{0x4a,0xCC},
{0x48,0x50},
{0x49,0x03},
{0x4a,0xE0},
{0x48,0x51},
{0x49,0x03},
{0x4a,0xF0},
{0x48,0x52},
{0x49,0x03},
{0x4a,0xFF},
};

const SENSOR_REG_T s5ka3dfx_cameramode_tab[]=
{
{0xef,0x03},
{0x5f,0x03},
{0x60,0x02},
{0x61,0x0f},
{0x62,0x0c},
{0x63,0x01},
{0x64,0x88},
{0x65,0x01},
{0x66,0x88},
{0x6f,0x6a},
};

const SENSOR_REG_T s5ka3dfx_camcordermode_tab[]=
{
        {0xef,0x03},
        {0x5f,0x08},
        {0x60,0x06},
        {0x61,0x08},
        {0x62,0x06},
        {0x63,0x02},
        {0x64,0x39},
        {0x65,0x02},
        {0x66,0x39},
        {0x6f,0x41}, 
};

LOCAL SENSOR_REG_TAB_INFO_T s_s5ka3dfx_resolution_Tab_YUV[]=
{
        // COMMON INIT
        {ADDR_AND_LEN_OF_ARRAY(s5ka3dfx_YUV_640X480), 640, 480, SENSOR_MCLK_24M, SENSOR_IMAGE_FORMAT_YUV422},

        // YUV422 PREVIEW 1	
        {ADDR_AND_LEN_OF_ARRAY(s5ka3dfx_cameramode_tab), 640, 480,SENSOR_MCLK_24M, SENSOR_IMAGE_FORMAT_YUV422},
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},

        // YUV422 PREVIEW 2 
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0},
        {PNULL, 0, 0, 0, 0, 0}
};

LOCAL SENSOR_IOCTL_FUNC_TAB_T s_s5ka3dfx_ioctl_func_tab = 
{
        // 1: Internal IOCTL function
        _s5ka3dfx_Reset,//PNULL,      // use to reset sensor 
        _s5ka3dfx_Power_On,      // Power on/off sensor selected by input parameter(0:off,1:on) 
        PNULL,      // enter sleep
        s5ka3dfx_Identify,    // identify sensor: 0 -> successful ; others -> fail	
        PNULL,      // write register
        PNULL,      // read  register	
        // Custom function
        PNULL,
        PNULL,
        // 2: External IOCTL function
        PNULL,                   // enable auto exposure
        PNULL,          // enable horizontal mirror
        PNULL,          // enable vertical mirror 	
        s5ka3dfx_set_brightness,                  // set brightness 	0: auto; other: the appointed level
        PNULL,//s5ka3dfx_set_contrast,                    // set contrast     0: auto; other: the appointed level  
        PNULL,               // set sharpness    0: auto; other: the appointed level
        PNULL,                  // set saturation   0: auto; other: the appointed level		
        s5ka3dfx_set_preview_mode,        // set preview mode : 0: normal mode; 1: night mode; 2: sunny mode
        s5ka3dfx_set_image_effect,        // set image effect
        PNULL,          // do something before do snapshort
        s5ka3dfx_After_Snapshot,      // do something after do snapshort                           	
        PNULL,      // 1: open flash; 0: close falsh                                                                                                           
        PNULL,      // return AE value
        PNULL,      // input AE value
        PNULL,      // return GAIN value
        PNULL,      // input GAIN value
        PNULL,      // return GAIN scale (for ov9650, 16)
        PNULL,      // set sensor frame rate based on current clock
        PNULL,      // input 1: enable; input 0: disable
        PNULL,      //return value: return 0 -> focus ok, other value -> lose focus
        s5ka3dfx_set_wb_mode,      // set while balance mode
        PNULL,      // get snapshot skip frame num from customer, input SENSOR_MODE_E paramter
        PNULL,//s5ka3dfx_set_iso,      // set ISO level					 0: auto; other: the appointed level
        s5ka3dfx_ev_compensation,      // Set exposure compensation	 0: auto; other: the appointed level
        s5ka3dfx_get_ev,      // Get the current frame lum
        PNULL,    // change image format
        PNULL,    // zoom
        PNULL,    // func_3,
        PNULL,    //func_4
        PNULL,//s5ka3dfx_set_flicker_elimination,      // Set banding flicker compensation	 0: 50Hz; 1: 60Hz 
        // CUSTOMER FUNCTION	                      
        PNULL,  	// video mode                      
        PNULL,	// pick up jpeg stream
        //	s5ka3dfx_set_meter_mode
};

/**---------------------------------------------------------------------------*
 ** 						Global Variables								  *
 **---------------------------------------------------------------------------*/
 SENSOR_INFO_T g_s5ka3dfx_yuv_info =
{
        S5KA3DFX_I2C_ADDR_W,				// salve i2c write address
        S5KA3DFX_I2C_ADDR_R, 				// salve i2c read address

        0,								// bit0: 0: i2c register value is 8 bit, 1: i2c register value is 16 bit
        							// bit2: 0: i2c register addr  is 8 bit, 1: i2c register addr  is 16 bit
        							// other bit: reseved
        SENSOR_HW_SIGNAL_PCLK_P|\
        SENSOR_HW_SIGNAL_VSYNC_P|\
        SENSOR_HW_SIGNAL_HSYNC_P|\
        S5KA3DFX_SIGNAL_PCLK_PHASE,		// bit0: 0:negative; 1:positive -> polarily of pixel clock
        							// bit2: 0:negative; 1:positive -> polarily of horizontal synchronization signal
        							// bit4: 0:negative; 1:positive -> polarily of vertical synchronization signal
        							// other bit: reseved	
        							// bit5~7: ccir delay sel
        									
        // preview mode
        SENSOR_ENVIROMENT_NORMAL|\
        SENSOR_ENVIROMENT_NIGHT,
        	
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
        SENSOR_WB_MODE_AUTO|\
        SENSOR_WB_MODE_INCANDESCENCE|\
        SENSOR_WB_MODE_U30|\
        SENSOR_WB_MODE_CWF|\
        SENSOR_WB_MODE_FLUORESCENT|\
        SENSOR_WB_MODE_SUN|\
        SENSOR_WB_MODE_CLOUD,

        // bit[0:7]: count of step in brightness, contrast, sharpness, saturation	
        // bit[8:16]: ISO type
        // bit[17:23]: EV compenation level
        // bit[24:32]: banding flicker elimination
        0,

        SENSOR_LOW_PULSE_RESET,			// reset pulse level  not used by samsung 
        20,								// reset pulse width(ms) not used by samsung 

        SENSOR_LOW_LEVEL_PWDN,			// 1: high level valid; 0: low level valid

        0,								// count of identify code  not used by samsung 
        {{0x04, 0x79},						// supply two code to identify sensor. not used by samsung 
        {0x0B, 0x04}},						// for Example: index = 0-> Device id, index = 1 -> version id	 not used by samsung 
        							
        SENSOR_AVDD_2800MV,				// voltage of avdd	

        640,							// max width of source image
        480,							// max height of source image
        "S5KA3DFX",						// name of sensor												

        SENSOR_IMAGE_FORMAT_YUV422,		// define in SENSOR_IMAGE_FORMAT_E enum,
        							// if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T
        SENSOR_IMAGE_PATTERN_YUV422_UYVY,	// pattern of input image form sensor;			

        s_s5ka3dfx_resolution_Tab_YUV,	// point to resolution table information structure
        &s_s5ka3dfx_ioctl_func_tab,		// point to ioctl function table
        	
        PNULL,							// information and table about Rawrgb sensor
        PNULL,							// extend information about sensor	
        SENSOR_AVDD_1800MV,                     // iovdd
        SENSOR_AVDD_1800MV,                      // dvdd
        4,                     // skip frame num before preview 
        1,                      // skip frame num before capture
        0,                      // deci frame num during preview	
        2,                       // deci frame num during video preview
        0,
        0,
        0,
        0,
        0	
};

LOCAL uint32_t _s5ka3dfx_Reset(uint32_t param)
{
        return (uint32_t)SENSOR_SUCCESS;    
}

LOCAL uint32_t _s5ka3dfx_Power_On(uint32_t power_on)
{
        SENSOR_AVDD_VAL_E		dvdd_val;
        SENSOR_AVDD_VAL_E		avdd_val;
        SENSOR_AVDD_VAL_E		iovdd_val;    

        //power_down = (BOOLEAN)g_s5ka3dfx_yuv_info.power_down_level;
        dvdd_val   = g_s5ka3dfx_yuv_info.dvdd_val;
        avdd_val   = g_s5ka3dfx_yuv_info.avdd_val;
        iovdd_val   = g_s5ka3dfx_yuv_info.iovdd_val;

        SENSOR_PRINT("SENSOR: _s5ka3dfx_Power_On  power_on =0x%x.\n", power_on);

        if(power_on) {
                // NOT in power down mode(maybe also open DVDD and DOVDD)
                //		GPIO_SetSensorPwdn(!power_on);
                //		GPIO_SetSensorPower(!power_on);
                //		GPIO_SetMainSensorReset(!power_on);
                // Open power
                Sensor_SetVoltage(dvdd_val, avdd_val, iovdd_val);

                //		GPIO_SetSensorPower(power_on);
                msleep(1);
                //		GPIO_SetSensorPwdn(power_on);
                msleep(1);
                // Open Mclk in default frequency
                Sensor_SetMCLK(SENSOR_MCLK_24M);    
                // Reset sensor
                msleep(5);
                //		GPIO_SetMainSensorReset(power_on);
                msleep(10);
        } else {
                //		GPIO_SetMainSensorReset(0);
                // Power down sensor and maybe close DVDD, DOVDD
                //		GPIO_SetSensorPwdn(0);
                //		GPIO_SetSensorPower(0);
                Sensor_SetVoltage(SENSOR_AVDD_CLOSED, SENSOR_AVDD_CLOSED, SENSOR_AVDD_CLOSED);

                // Close Mclk
                Sensor_SetMCLK(SENSOR_DISABLE_MCLK);		
        }
        return 0;
}
/**---------------------------------------------------------------------------*
 ** 							Function  Definitions
 **---------------------------------------------------------------------------*/
LOCAL void s5ka3dfx_WriteReg( uint8_t  subaddr, uint8_t data )
{	
#ifndef	_USE_DSP_I2C_		
        Sensor_WriteReg( subaddr,  data);
#else
        DSENSOR_IICWrite((uint16)subaddr, (uint16)data);
#endif
        SENSOR_PRINT("SENSOR: S5KA3DFX_WriteReg 0x%x=0x%x.\n", subaddr, data);
}

LOCAL uint8_t s5ka3dfx_ReadReg( uint8_t  subaddr)
{
        uint8_t value = 0;
#ifndef	_USE_DSP_I2C_
        //I2C_WriteCmdArr(S5KA3DFX_I2C_ADDR_W, &subaddr, 2, SCI_TRUE);
        //I2C_ReadCmd(S5KA3DFX_I2C_ADDR_R, &value, SCI_TRUE);
        value=Sensor_ReadReg(subaddr);
#else
        value = (uint16)DSENSOR_IICRead((uint16)subaddr);
#endif
        SENSOR_PRINT("SENSOR: S5KA3DFX_WriteReg 0x%x=0x%x.\n", subaddr, value);	
        return value;
}
LOCAL uint32_t s5ka3dfx_Identify(uint32_t param) 
{
        uint8_t ret = 0;
        uint8_t reg[1] 	= {0xC5};

        SENSOR_PRINT("S5KA3DFX_Identify: it is S5KA3DFX");
        s5ka3dfx_WriteReg(S5KA3DFX_SWITCH_PAGE_ID, S5KA3DFX_PAGE1_ID);
        ret = s5ka3dfx_ReadReg(reg[0]);  
        SENSOR_PRINT("S5KA3DFX_Identify: it is S5KA3DFX 0x%x after read reg", ret);
        return (uint32_t)SENSOR_SUCCESS;    
}
#if 0
LOCAL uint32_t set_s5ka3dfx_ae_enable(uint32_t enable)
{
	return 0;
}
LOCAL uint32_t set_hmirror_enable(uint32_t enable)
{
	SENSOR_PRINT("set_hmirror_enable: enable = %d.\n", enable);		
	return 0;
}
LOCAL uint32_t set_vmirror_enable(uint32_t enable)
{
	SENSOR_PRINT("set_vmirror_enable: enable = %d.\n", enable);
	return 0;
}
#endif
/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/
const SENSOR_REG_T s5ka3dfx_brightness_tab[][5]=
{
        {		
        {0xef, 0x03}, 
        {0x31, 0xaa}, 
        {0x32, 0xaa}, 			
        {0x6d, 0x55},
        {0xff, 0xff}
        },
        {		
        {0xef, 0x03}, 
        {0x31, 0x98}, 
        {0x32, 0x98}, 			
        {0x6d, 0x55},
        {0xff, 0xff}
        },
        {
        {0xef, 0x03}, 
        {0x31, 0x8f}, 
        {0x32, 0x8f}, 
        {0x6d, 0x55},
        {0xff, 0xff}
        },

        {//level 4
        {0xef, 0x03}, 
        {0x31, 0x00}, 
        {0x32, 0x0a},
        {0x6d, 0x55},
        {0xff, 0xff}
        },

        {
        {0xef, 0x03}, 
        {0x31, 0x18}, 
        {0x32, 0x20}, 
        {0x6d, 0x55},
        {0xff, 0xff}		
        },

        {
        {0xef, 0x03}, 
        {0x31, 0x23}, 
        {0x32, 0x23}, 
        {0x6d, 0x55},
        {0xff, 0xff}		
        },

        {
        {0xef, 0x03}, 
        {0x31, 0x29}, 
        {0x32, 0x29},
        {0x6d, 0x55},
        {0xff, 0xff}		
        },
};
LOCAL uint32_t s5ka3dfx_set_brightness(uint32_t level)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)s5ka3dfx_brightness_tab[level];
        if(level >= 7) {
                SENSOR_PRINT("s5ka3dfx_set_brightness:param error,level=%d .\n",level);
                return SENSOR_FAIL;
        }

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                s5ka3dfx_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        msleep(100); 
        SENSOR_PRINT("set_brightness: level = %d.\n", level);
        return 0;
}

const SENSOR_REG_T s5ka3dfx_contrast_tab[][2]=
{
        {{0xff,0xff}},
        {{0xff,0xff}},
        {{0xff,0xff}},
        {{0xff,0xff}},
        {{0xff,0xff}},
        {{0xff,0xff}},
        {{0xff,0xff}},
};
#if 0
LOCAL uint32_t s5ka3dfx_set_contrast(uint32_t level)
{
	uint16_t i;
	SENSOR_REG_T* sensor_reg_ptr;

	sensor_reg_ptr = (SENSOR_REG_T*)s5ka3dfx_contrast_tab[level];

//	SCI_ASSERT(level < 7 );
//	SCI_ASSERT(PNULL != sensor_reg_ptr);
	if(level >= 7)
	{
		SENSOR_PRINT("s5ka3dfx_set_contrast:param error,level=%d.\n",level);
		return SENSOR_FAIL;
	}

	for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value); i++)
	{
		s5ka3dfx_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}
	msleep(20);
	SENSOR_PRINT("set_contrast: level = %d.\n", level);

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
#endif
/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/
LOCAL uint32_t s5ka3dfx_set_preview_mode(uint32_t preview_mode)
{
        SENSOR_PRINT("set_preview_mode: preview_mode = %d.\n", preview_mode);

        s_preview_mode = preview_mode;
	
        switch (preview_mode) {
        case SENSOR_PARAM_ENVIRONMENT_NORMAL: 
                s5ka3dfx_set_work_mode(0);
                break;
        case SENSOR_PARAM_ENVIRONMENT_NIGHT:
                s5ka3dfx_set_work_mode(1);
                break;
        case SENSOR_PARAM_ENVIRONMENT_SUNNY:
                s5ka3dfx_set_work_mode(0);
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
 const SENSOR_REG_T s5ka3dfx_image_effect_tab[][10]=
{//Tim.zhu@20080602 for tim.zhu_cr116992	
        { // normal
        {0xef, 0x00},
        {0xd3, 0x00},
        {0xd4, 0x00},
        {0xd5, 0x01},
        {0xd6, 0xa3},    
        {0xff,0xff},
        },

        { //white black                           
        {0xef, 0x00},
        {0xd3, 0x00},
        {0xd4, 0x03},
        {0xd5, 0x80},
        {0xd6, 0x80},   
        {0xff, 0xff},       
        },

        { // red
        {0xff, 0xff},
        },

        {   // green
        //****** Effect GREEN ******//
        {0xef,0x00},
        {0xd3,0x00},
        {0xd4,0x03},
        {0xd5,0x2c},
        {0xd6,0x41}, 
        {0xff, 0xff},
        },

        { // blue     
        {0xff, 0xff},
        },

        { // yellow
        {0xff, 0xff},
        },        

        {  // negative                             
        {0xef, 0x00},
        {0xd3, 0x01},
        {0xd4, 0x00},
        {0xd5, 0x2c},
        {0xd6, 0x81},     
        {0xff, 0xff},
        }, 

        { // antique
        {0xef,0x00},
        {0xd3,0x00},
        {0xd4,0x03},
        {0xd5,0xdc},
        {0xd6,0x00}, 
        {0xff, 0xff},       
        },
};		

LOCAL uint32_t s5ka3dfx_set_image_effect(uint32_t effect_type)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)s5ka3dfx_image_effect_tab[effect_type];

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value) ; i++) {
                s5ka3dfx_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_PRINT("set_image_effect: effect_type = %d.\n", effect_type);

        return 0;
}
#ifdef PRODUCT_DRIVER_HIMALAYA
/******************************************************************************/
// Description: change the sensor setting to preview mode
// Global resource dependence: 
// Author: Tim.zhu
// Note:
//
/******************************************************************************/
LOCAL uint32_t s5ka3dfx_After_Snapshot(uint32_t param)
{    
        return 0;    
}
#else
LOCAL uint32_t s5ka3dfx_After_Snapshot(uint32_t param)
{
        return 0;   
}
#if 0
LOCAL uint32_t s5ka3dfx_BeforeSnapshot(uint32_t param)
{    
	return 0;    
}
#endif
#endif
#if 0
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
#endif
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		mode 0:normal;   1:night 
/******************************************************************************/
const SENSOR_REG_T s5ka3dfx_iso_tab[][3]=
{
        //IS0 atuto
        {
        {0xff, 0xff}
        },
        //IS0 low
        {
        {0xff, 0xff}
        },
        //IS0 mid
        {
        {0xff, 0xff}
        },
        //IS0 high
        {
        {0xff, 0xff}
        },	
};
#if 0
LOCAL uint32_t s5ka3dfx_set_iso(uint32_t iso_type)
{
	uint16_t i;
	SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)s5ka3dfx_iso_tab[iso_type];

//	SCI_ASSERT(PNULL != sensor_reg_ptr);
	
	for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

	SENSOR_PRINT("s5ka3dfx_set_iso: iso_type = %d.\n", iso_type);

	return 0;
}
#endif
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		white blance 
/******************************************************************************/
const SENSOR_REG_T s5ka3dfx_wb_mode_tab[][8]=
{
        {//auto
        {0xef,0x03},
        {0x00,0x87},

        {0xef,0x00},
        {0x42,0x60}, //rgain (start point of awb)
        {0x43,0x40}, //ggain (start point of awb)  
        {0x44,0x60}, //bgain (start point of awb)
        {0xff, 0xff},
        },

        {//sunny
        {0xef, 0x03},
        {0x00, 0x85},
        {0xef, 0x00},
        {0x42, 0x68},
        {0x43, 0x43},
        {0x44, 0x4F},
        {0xff, 0xff},
        },

        {//cloudy
        {0xef,0x03},
        {0x00,0x85},//05
        {0xef,0x00},
        {0x42,0x7b}, //rgain (start point of awb) //77
        {0x43,0x3d}, //ggain (start point of awb) 
        {0x44,0x44}, //bgain (start point of awb)  
        {0xff, 0xff},
        },

        {//office
        {0xef,0x03},
        {0x00,0x85},
        {0xef,0x00},
        {0x42,0x6b}, //rgain (start point of awb)
        {0x43,0x40}, //ggain (start point of awb)  
        {0x44,0x50}, //bgain (start point of awb)  
        {0xff, 0xff},
        },

        {//home
        {0xef,0x03},
        {0x00,0x85},
        {0xef,0x00},
        {0x42,0x5e}, //rgain (start point of awb)
        {0x43,0x40}, //ggain (start point of awb)  
        {0x44,0x6d}, //bgain (start point of awb) //6f  
        {0xff, 0xff},
        },
};

LOCAL uint32_t s5ka3dfx_set_wb_mode(uint32_t wb_mode)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)s5ka3dfx_wb_mode_tab[wb_mode];
        if(wb_mode>4) {
                return 0;
        }
	
        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_PRINT("OV2640_set_wb_mode: wb_mode = %d.\n", wb_mode);
        return 0; 
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		mode 0:normal;   1:night 
/******************************************************************************/
const SENSOR_REG_T s5ka3dfx_flicker_mode_tab[][5]=
{
        //svga 50Hz
        {
        {0xff, 0xff}	
        },
        //svga 60Hz
        {
        {0xff, 0xff}	
        }
};
#if 0
LOCAL uint32_t s5ka3dfx_set_flicker_elimination(uint32_t flicker_mode)
{
	uint16_t i;
	SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)s5ka3dfx_flicker_mode_tab[flicker_mode];

//	SCI_ASSERT(PNULL != sensor_reg_ptr);
	
	for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

	SENSOR_PRINT("s5ka3dfx_set_flicker_elimination_mode: flicker_mode = %d.\n", flicker_mode);
    
	return 0;
}
#endif
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		ev mode 0:-3 -2 -1 0 1 2 3 
/******************************************************************************/
const SENSOR_REG_T s5ka3dfx_ev_compensation_tab[][3]=
{
        {{0xff, 0xff}},
        {{0xff, 0xff}},
        {{0xff, 0xff}},
        {{0xff, 0xff}},
        {{0xff, 0xff}},
        {{0xff, 0xff}},
        {{0xff, 0xff}}    
};
LOCAL uint32_t s5ka3dfx_ev_compensation(uint32_t ev_mode)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)s5ka3dfx_ev_compensation_tab[ev_mode];

        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_PRINT("s5ka3dfx_ev_compensation: ev_mode = %d.\n", ev_mode);    
        return 0;
}

LOCAL uint32_t s5ka3dfx_get_ev(uint32_t param)
{
        return 0x00;     
}
#if 0
LOCAL uint32_t s5ka3dfx_set_meter_mode(uint32_t param)
{
    // the param is a pointer to a uint32 array, param[0] means meter mode, 0---FULL meter, 1----Center meter, 2----Spot meter,
    // if param[0] equals 2,then the following 4 members from param[1] to param[4] desricbe the rectangle of spot meter.
    
    // add code here
	uint32_t  *p_para = (uint32_t*)param;

	if(NULL == p_para)
		return 0xff;
    
	switch(p_para[0])
	{
		case 0: //Full meter
			break;
		case 1: //center meter
			break;
		case 2: // spot meter
			break;
		default:
			return 0xff;
	}

	SENSOR_PRINT("s5ka3dfx_set_meter_mode: mode = %d, rect(%d,%d,%d,%d).\n", p_para[0],p_para[1],p_para[2],p_para[3],p_para[4]);
        
	return 0x00;     
}
#endif
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		mode 0:normal;	 1:night 
/******************************************************************************/
const SENSOR_REG_T s5ka3dfx_mode_tab[][64]=
{
        { 

        {0xEF,0x03},
        {0x5F,0x03},  //NT Cintr Max
        {0x60,0x02},  //PAL Cintr Max
        {0x61,0x0F},  //NT shutter Max (FrameAE mode)
        {0x62,0x0C},  //PAL shutter Max (FrameAE mode)
        {0x63,0x02},  //NT Vblank
        {0x64,0x39},
        {0x65,0x02},  //PAL Vblank
        {0x66,0x39},    

        {0x6d,0x4f},  //ll_agc_b
        {0x6e,0x40},  //dgain min
        {0x6f,0x6a},  //dgain max  

        {0xef,0x00},  
        {0xed,0x05}, //edge color suppress color gain slope                                                            
        {0x42,0x60},  //Rgain (start point of AWB)
        {0x44,0x60},  //Bgain (start point of AWB)

        {0xef,0x03},  
        {0x02,0x05}, //ae threshold       
        {0x04,0x5b}, //agc max of lowlux    

        {0x31,0x00}, //nl_bt(brightness)                                            
        {0x32,0x0b}, //8 ll_bt 

        {0x26,0x01},

        {0x2f,0x1e}, //30 //2a //20 ll_nr_gain
        {0x30,0xff}, //ff //50 ll_nr_edge

        {0x36,0x3C}, //nl_st(saturation)_gain
        {0x37,0x33}, //30 ll_st_gain

        {0x15,0x0b}, //awb cnt  

        {0xef,0x02},  
        {0x1e,0xb7}, //global gain  
        {0x26,0x24}, //clp level  

        {0xef,0x00},  //awb map
        {0x93,0x40},
        {0x94,0x80},                                                             
        {0x95,0xc0},                                                             
        {0x96,0xc0},                                                             
        {0x97,0x20},                                                             
        {0x98,0x20},                                                             
        {0x99,0x30},                                                             
        {0xa0,0x00},                                                             
        {0xa1,0x00},                                                             
        {0xa2,0x1c},                                                             
        {0xa3,0x16},                                                             
        {0xa4,0x03},                                                             
        {0xa5,0x07},                                                             
        {0xa6,0x00},

        }, // normal

        // night
        { 

        {0xef,0x03},  
        {0x00,0x00},  //AWB, AE Off

        {0xef,0x03},  
        {0x5f,0x03},  //NT Cintr Max                    
        {0x60,0x02},  //PAL Cintr Max                   
        {0x61,0x22},  //NT shutter Max (FrameAE mode)   
        {0x62,0x1c},  //PAL shutter Max (FrameAE mode)  
        {0x63,0x02},  //NT Vblank                       
        {0x64,0x39},                                     
        {0x65,0x02},  //PAL Vblank                                                          
        {0x66,0x39},  

        {0x6d,0x48},  //AGC Min for Suppress B

        {0x6f,0x6c},  //Dgain Max

        {0xef,0x00},  
        {0xed,0x30},  //ECS th. 
        {0x42,0x46},  //Rgain (start point of AWB)
        {0x44,0x84},  //Bgain (start point of AWB)

        {0xef,0x03},  
        {0x02,0x07},  //ae threshold   
        {0x04,0x65},  //agc max of lowlux  

        {0x31,0x00},  //nl_bt(brightness)                                            
        {0x32,0x1c},  //ll_bt  

        {0x26,0x08},

        {0x2f,0x60},  //2a //20 ll_nr_gain
        {0x30,0xd8},  //50 ll_nr_edge

        {0x36,0x4b},  //nl_st(saturation)_gain
        {0x37,0x4b},  //30 ll_st_gain

        {0x15,0x03},  //awb cnt

        {0xef,0x02},  
        {0x1e,0xc7},  //global gain
        {0x26,0x34},  //clp level

        {0xef,0x00},  //awb map
        {0x93,0x40},  
        {0x94,0xf0},  
        {0x95,0xf0},  
        {0x96,0xf8},  
        {0x97,0xf8},  
        {0x98,0xfc},  
        {0x99,0xfe},  
        {0xA0,0x00},  
        {0xA1,0x3c},  
        {0xA2,0x3d},  
        {0xA3,0x1f},  
        {0xA4,0x0f},  
        {0xA5,0x07},  
        {0xA6,0x07},  

        {0xef,0x03},  
        {0x00,0x87},  //AWB, AE ON

        }
};
LOCAL uint32_t s5ka3dfx_set_work_mode(uint32_t mode)
{
        uint16_t i;
        SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)s5ka3dfx_mode_tab[mode];
        if(mode > 1) {
                SENSOR_PRINT("s5ka3dfx_set_work_mode:param error,mode=%d.\n",mode);
                return SENSOR_FAIL;
        }
        for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++) {
                Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
        }
        SENSOR_PRINT("set_work_mode: mode = %d", mode);
        return 0;
}
struct sensor_drv_cfg sensor_s5ka3dfx = {
        .sensor_pos = CONFIG_DCAM_SENSOR_POS_S5KA3DFX,
        .sensor_name = "s5ka3dfx",
        .driver_info = &g_s5ka3dfx_yuv_info,
};

static int __init sensor_s5ka3dfx_init(void)
{
        return dcam_register_sensor_drv(&sensor_s5ka3dfx);
}

subsys_initcall(sensor_s5ka3dfx_init);

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
#include <linux/slab.h>
/**---------------------------------------------------------------------------*
 **                            Macro Define
 **---------------------------------------------------------------------------*/

#define s5k5cagx_I2C_ADDR_W       0x2d//0x5a 
#define s5k5cagx_I2C_ADDR_R        0x2d//0x5b

#define I2C_WRITE_BURST_LENGTH    512
/**---------------------------------------------------------------------------*
 **                     Local Function Prototypes                                                                                                         *
 **---------------------------------------------------------------------------*/
LOCAL uint32_t _s5k5cagx_InitExifInfo(void);
//LOCAL uint32_t _s5k5cagx_GetResolutionTrimTab(uint32_t param);
LOCAL uint32_t _s5k5cagx_PowerOn(uint32_t power_on);
LOCAL uint32_t _s5k5cagx_Identify(uint32_t param);
LOCAL uint32_t _s5k5cagx_set_brightness(uint32_t level);
LOCAL uint32_t _s5k5cagx_set_contrast(uint32_t level);
//LOCAL uint32_t _s5k5cagx_set_sharpness(uint32_t level);
LOCAL uint32_t _s5k5cagx_set_saturation(uint32_t level);
LOCAL uint32_t _s5k5cagx_set_image_effect(uint32_t effect_type);
LOCAL uint32_t _s5k5cagx_set_ev(uint32_t level); 
LOCAL uint32_t _s5k5cagx_set_awb(uint32_t mode);
LOCAL uint32_t _s5k5cagx_set_work_mode(uint32_t mode);
LOCAL uint32_t _s5k5cagx_BeforeSnapshot(uint32_t param);
LOCAL uint32_t _s5k5cagx_check_image_format_support(uint32_t param); 
LOCAL uint32_t _s5k5cagx_pick_out_jpeg_stream(uint32_t param);
LOCAL uint32_t _s5k5cagx_set_video_mode(uint32_t mode);
//LOCAL uint32_t _s5k5cagx_chang_image_format(uint32_t param);
LOCAL uint32_t _s5k5cagx_after_snapshot(uint32_t param);
//LOCAL uint32_t _s5k5cagx_GetExifInfo(uint32_t param);
//LOCAL uint32_t _s5k5cagx_ExtFunc(uint32_t ctl_param);
LOCAL uint32_t _s5k5cagx_EnterSleep(uint32_t pwd_level);
LOCAL uint32_t _s5k5cagx_InitExt(uint32_t param);
LOCAL uint32_t _s5k5cagx_Reset(uint32_t power_on);


LOCAL const SENSOR_REG_T s5k5cagx_common[]= 
{
#if 0
// ARM GO
// Direct mode 
{SENSOR_WRITE_DELAY,10},
{0xFCFC,0xD000},
{0x0010,0x0001}, // Reset
{0x1030,0x0000}, // Clear host interrupt so main will wait
{0x0014,0x0001}, // ARM go
{0x002A,0x026C},
{0x0F12,0x0320}, //#REG_0TC_PCFG_usWidth//800   
{0x0F12,0x0258},  //#REG_0TC_PCFG_usHeight //600    
{0x0F12,0x0005}, //#REG_0TC_PCFG_Format             
{0x0F12,0x2ee0},// //2ee0, //#REG_0TC_PCFG_usMaxOut4KHzRate  
{0x0F12,0x2ea0},// //2ea0, //#REG_0TC_PCFG_usMinOut4KHzRate  
{0x0F12,0x0100}, //#REG_0TC_PCFG_OutClkPerPix88    
{0x0F12,0x0800}, //#REG_0TC_PCFG_uMaxBpp88           
{0x0F12,0x0052}, //#REG_0TC_PCFG_PVIMask //s0050 = FALSE in MSM6290 : s0052 = TRUE in MSM6800 //reg 027A
{0x0F12,0x4000}, //#REG_0TC_PCFG_OIFMask
{0x0F12,0x0400}, //#REG_0TC_PCFG_usJpegPacketSize
{0x0F12,0x0258}, //#REG_0TC_PCFG_usJpegTotalPackets
{0x0F12,0x0000}, //#REG_0TC_PCFG_uClockInd
{0x0F12,0x0000}, //#REG_0TC_PCFG_usFrTimeType
{0x0F12,0x0001}, //#REG_0TC_PCFG_FrRateQualityType
{0x0F12,0x0400}, //#REG_0TC_PCFG_usMaxFrTimeMsecMult10 
{0x0F12,0x0330}, //#REG_0TC_PCFG_usMinFrTimeMsecMult10
{0x0F12,0x0000}, //#REG_0TC_PCFG_bSmearOutput
{0x0F12,0x0000}, //#REG_0TC_PCFG_sSaturation
{0x0F12,0x0000}, //#REG_0TC_PCFG_sSharpBlur
{0x0F12,0x0000}, //#REG_0TC_PCFG_sColorTemp
{0x0F12,0x0000}, //#REG_0TC_PCFG_uDeviceGammaIndex
{0x0F12,0x0003}, //#REG_0TC_PCFG_uPrevMirror///mirror
{0x0F12,0x0003}, //#REG_0TC_PCFG_uCaptureMirror///mirror
{0x0F12,0x0000}, //#REG_0TC_PCFG_uRotation
{0x002A,0x023C}, 
{0x0F12,0x0000}, // #REG_TC_GP_ActivePrevConfig // Select preview configuration_0
{0x002A,0x0240}, 
{0x0F12,0x0001}, // #REG_TC_GP_PrevOpenAfterChange
{0x002A,0x0230}, 
{0x0F12,0x0001}, // #REG_TC_GP_NewConfigSync // Update preview configuration
{0x002A,0x023E}, 
{0x0F12,0x0001}, // #REG_TC_GP_PrevConfigChanged
{0x002A,0x0220}, 
{0x0F12,0x0001}, // #REG_TC_GP_EnablePreview // Start preview
{0x0F12,0x0001}, // #REG_TC_GP_EnablePreviewChanged
{0x002A,0x035C}, 
{0x0F12,0x0000}, //#REG_0TC_CCFG_uCaptureModeJpEG
{0x0F12,0x0800}, //#REG_0TC_CCFG_usWidth 
{0x0F12,0x0600}, //#REG_0TC_CCFG_usHeight
{0x0F12,0x0005}, //#REG_0TC_CCFG_Format//5:YUV9:JPEG 
{0x0F12,0x2ee0}, //#REG_0TC_CCFG_usMaxOut4KHzRate
{0x0F12,0x2ea0}, //#REG_0TC_CCFG_usMinOut4KHzRate
{0x0F12,0x0100}, //#REG_0TC_CCFG_OutClkPerPix88
{0x0F12,0x0800}, //#REG_0TC_CCFG_uMaxBpp88 
{0x0F12,0x0052}, //#REG_0TC_CCFG_PVIMask 
{0x0F12,0x0050}, //#REG_0TC_CCFG_OIFMask   
{0x0F12,0x0400}, //#REG_0TC_CCFG_usJpegPacketSize
{0x0F12,0x0258}, //#REG_0TC_CCFG_usJpegTotalPackets
{0x0F12,0x0000}, //#REG_0TC_CCFG_uClockInd 
{0x0F12,0x0000}, //#REG_0TC_CCFG_usFrTimeType
{0x0F12,0x0000}, //#REG_0TC_CCFG_FrRateQualityType 
{0x0F12,0x2000}, //#REG_0TC_CCFG_usMaxFrTimeMsecMult10
{0x0F12,0x1000}, //#REG_0TC_CCFG_usMinFrTimeMsecMult10
{0x0F12,0x0000}, //#REG_0TC_CCFG_bSmearOutput
{0x0F12,0x0000}, //#REG_0TC_CCFG_sSaturation 
{0x0F12,0x0000}, //#REG_0TC_CCFG_sSharpBlur
{0x0F12,0x0000}, //#REG_0TC_CCFG_sColorTemp
{0x0F12,0x0000}, //#REG_0TC_CCFG_uDeviceGammaIndex 
{0x0028,0xD000},
{0x002A,0x1000},
{0x0F12,0x0001}
#else
		//<CAMTUNING_INIT>
		//********************************//
		////************************************/																															//
		////Ver 0.1 Nnalog Settings for 5CA EVT0 30FPS in Binning, including latest TnP for EVT1			  //
		////20091310 ded analog settings for 430LSB, long exposure mode only. Settings are for 32MHz Sys. CLK //
		////20091102 ded all calibration data, final settings for STW EVT1 module, SCLK 32MHz, PCLK 60 MHz.   //
		////20091104 anged the shading alpha &Near off														  //
		////20091104 anged awbb_GridEnable from 0001h to 0002h	//awbb_GridEnable							  //
		////aetarget4, gamma change for improving contrast													  //
		////20100113 w TnP updated//
		
		{0xFCFC, 0xD000},	//Reset 								 //
		{0x0010, 0x0001},	//Clear host interrupt so main will wait //
		{0x1030, 0x0000},	//ARM go								 //
		{0x0014, 0x0001},	//Wait100mSec							 //
		
		//Delay 100ms	
		{0xffff, 0x0064},  //Delay 100ms	
		
		// Start T&P part
		// DO NOT DELETE T&P SECTION COMMENTS! They are required to debug T&P related issues.
		// svn://transrdsrv/svn/svnroot/System/Software/tcevb/SDK+FW/ISP_5CA/Firmware
		// Rev: 33047-33047
		// Signature:
		// md5 6254dfca66adca40485075c9b910ba19 .btp
		// md5 cfb4ae7a2d8ae0084b859906a6b2a12c .htp
		// md5 eef3c9ff422110ae87ecc792a436d69e .RegsMap.h
		// md5 e18c15b6de4bae04b1e75aef7c2317ea .RegsMap.bin
		// md5 506b4144bd48cdb79cbecdda4f7176ba .base.RegsMap.h
		// md5 fd8f92f13566c1a788746b23691c5f5f .base.RegsMap.bin
		//
		
		{0x0028, 0x7000},
		{0x002A, 0x2CF8},
		{0x0F12, 0xB510},
		{0x0F12, 0x4827},
		{0x0F12, 0x21C0},
		{0x0F12, 0x8041},
		{0x0F12, 0x4825},
		{0x0F12, 0x4A26},
		{0x0F12, 0x3020},
		{0x0F12, 0x8382},
		{0x0F12, 0x1D12},
		{0x0F12, 0x83C2},
		{0x0F12, 0x4822},
		{0x0F12, 0x3040},
		{0x0F12, 0x8041},
		{0x0F12, 0x4821},
		{0x0F12, 0x4922},
		{0x0F12, 0x3060},
		{0x0F12, 0x8381},
		{0x0F12, 0x1D09},
		{0x0F12, 0x83C1},
		{0x0F12, 0x4821},
		{0x0F12, 0x491D},
		{0x0F12, 0x8802},
		{0x0F12, 0x3980},
		{0x0F12, 0x804A},
		{0x0F12, 0x8842},
		{0x0F12, 0x808A},
		{0x0F12, 0x8882},
		{0x0F12, 0x80CA},
		{0x0F12, 0x88C2},
		{0x0F12, 0x810A},
		{0x0F12, 0x8902},
		{0x0F12, 0x491C},
		{0x0F12, 0x80CA},
		{0x0F12, 0x8942},
		{0x0F12, 0x814A},
		{0x0F12, 0x8982},
		{0x0F12, 0x830A},
		{0x0F12, 0x89C2},
		{0x0F12, 0x834A},
		{0x0F12, 0x8A00},
		{0x0F12, 0x4918},
		{0x0F12, 0x8188},
		{0x0F12, 0x4918},
		{0x0F12, 0x4819},
		{0x0F12, 0xF000},
		{0x0F12, 0xFA0E},
		{0x0F12, 0x4918},
		{0x0F12, 0x4819},
		{0x0F12, 0x6341},
		{0x0F12, 0x4919},
		{0x0F12, 0x4819},
		{0x0F12, 0xF000},
		{0x0F12, 0xFA07},
		{0x0F12, 0x4816},
		{0x0F12, 0x4918},
		{0x0F12, 0x3840},
		{0x0F12, 0x62C1},
		{0x0F12, 0x4918},
		{0x0F12, 0x3880},
		{0x0F12, 0x63C1},
		{0x0F12, 0x4917},
		{0x0F12, 0x6301},
		{0x0F12, 0x4917},
		{0x0F12, 0x3040},
		{0x0F12, 0x6181},
		{0x0F12, 0x4917},
		{0x0F12, 0x4817},
		{0x0F12, 0xF000},
		{0x0F12, 0xF9F7},
		{0x0F12, 0x4917},
		{0x0F12, 0x4817},
		{0x0F12, 0xF000},
		{0x0F12, 0xF9F3},
		{0x0F12, 0x4917},
		{0x0F12, 0x4817},
		{0x0F12, 0xF000},
		{0x0F12, 0xF9EF},
		{0x0F12, 0xBC10},
		{0x0F12, 0xBC08},
		{0x0F12, 0x4718},
		{0x0F12, 0x1100},
		{0x0F12, 0xD000},
		{0x0F12, 0x267C},
		{0x0F12, 0x0000},
		{0x0F12, 0x2CE8},
		{0x0F12, 0x0000},
		{0x0F12, 0x3274},
		{0x0F12, 0x7000},
		{0x0F12, 0xF400},
		{0x0F12, 0xD000},
		{0x0F12, 0xF520},
		{0x0F12, 0xD000},
		{0x0F12, 0x2DF1},
		{0x0F12, 0x7000},
		{0x0F12, 0x89A9},
		{0x0F12, 0x0000},
		{0x0F12, 0x2E43},
		{0x0F12, 0x7000},
		{0x0F12, 0x0140},
		{0x0F12, 0x7000},
		{0x0F12, 0x2E7D},
		{0x0F12, 0x7000},
		{0x0F12, 0xB4F7},
		{0x0F12, 0x0000},
		{0x0F12, 0x2F07},
		{0x0F12, 0x7000},
		{0x0F12, 0x2F2B},
		{0x0F12, 0x7000},
		{0x0F12, 0x2FD1},
		{0x0F12, 0x7000},
		{0x0F12, 0x2FE5},
		{0x0F12, 0x7000},
		{0x0F12, 0x2FB9},
		{0x0F12, 0x7000},
		{0x0F12, 0x013D},
		{0x0F12, 0x0001},
		{0x0F12, 0x306B},
		{0x0F12, 0x7000},
		{0x0F12, 0x5823},
		{0x0F12, 0x0000},
		{0x0F12, 0x30B9},
		{0x0F12, 0x7000},
		{0x0F12, 0xD789},
		{0x0F12, 0x0000},
		{0x0F12, 0xB570},
		{0x0F12, 0x6804},
		{0x0F12, 0x6845},
		{0x0F12, 0x6881},
		{0x0F12, 0x6840},
		{0x0F12, 0x2900},
		{0x0F12, 0x6880},
		{0x0F12, 0xD007},
		{0x0F12, 0x49C3},
		{0x0F12, 0x8949},
		{0x0F12, 0x084A},
		{0x0F12, 0x1880},
		{0x0F12, 0xF000},
		{0x0F12, 0xF9BA},
		{0x0F12, 0x80A0},
		{0x0F12, 0xE000},
		{0x0F12, 0x80A0},
		{0x0F12, 0x88A0},
		{0x0F12, 0x2800},
		{0x0F12, 0xD010},
		{0x0F12, 0x68A9},
		{0x0F12, 0x6828},
		{0x0F12, 0x084A},
		{0x0F12, 0x1880},
		{0x0F12, 0xF000},
		{0x0F12, 0xF9AE},
		{0x0F12, 0x8020},
		{0x0F12, 0x1D2D},
		{0x0F12, 0xCD03},
		{0x0F12, 0x084A},
		{0x0F12, 0x1880},
		{0x0F12, 0xF000},
		{0x0F12, 0xF9A7},
		{0x0F12, 0x8060},
		{0x0F12, 0xBC70},
		{0x0F12, 0xBC08},
		{0x0F12, 0x4718},
		{0x0F12, 0x2000},
		{0x0F12, 0x8060},
		{0x0F12, 0x8020},
		{0x0F12, 0xE7F8},
		{0x0F12, 0xB510},
		{0x0F12, 0xF000},
		{0x0F12, 0xF9A2},
		{0x0F12, 0x48B2},
		{0x0F12, 0x8A40},
		{0x0F12, 0x2800},
		{0x0F12, 0xD00C},
		{0x0F12, 0x48B1},
		{0x0F12, 0x49B2},
		{0x0F12, 0x8800},
		{0x0F12, 0x4AB2},
		{0x0F12, 0x2805},
		{0x0F12, 0xD003},
		{0x0F12, 0x4BB1},
		{0x0F12, 0x795B},
		{0x0F12, 0x2B00},
		{0x0F12, 0xD005},
		{0x0F12, 0x2001},
		{0x0F12, 0x8008},
		{0x0F12, 0x8010},
		{0x0F12, 0xBC10},
		{0x0F12, 0xBC08},
		{0x0F12, 0x4718},
		{0x0F12, 0x2800},
		{0x0F12, 0xD1FA},
		{0x0F12, 0x2000},
		{0x0F12, 0x8008},
		{0x0F12, 0x8010},
		{0x0F12, 0xE7F6},
		{0x0F12, 0xB5F8},
		{0x0F12, 0x2407},
		{0x0F12, 0x2C06},
		{0x0F12, 0xD035},
		{0x0F12, 0x2C07},
		{0x0F12, 0xD033},
		{0x0F12, 0x48A3},
		{0x0F12, 0x8BC1},
		{0x0F12, 0x2900},
		{0x0F12, 0xD02A},
		{0x0F12, 0x00A2},
		{0x0F12, 0x1815},
		{0x0F12, 0x4AA4},
		{0x0F12, 0x6DEE},
		{0x0F12, 0x8A92},
		{0x0F12, 0x4296},
		{0x0F12, 0xD923},
		{0x0F12, 0x0028},
		{0x0F12, 0x3080},
		{0x0F12, 0x0007},
		{0x0F12, 0x69C0},
		{0x0F12, 0xF000},
		{0x0F12, 0xF96B},
		{0x0F12, 0x1C71},
		{0x0F12, 0x0280},
		{0x0F12, 0xF000},
		{0x0F12, 0xF967},
		{0x0F12, 0x0006},
		{0x0F12, 0x4898},
		{0x0F12, 0x0061},
		{0x0F12, 0x1808},
		{0x0F12, 0x8D80},
		{0x0F12, 0x0A01},
		{0x0F12, 0x0600},
		{0x0F12, 0x0E00},
		{0x0F12, 0x1A08},
		{0x0F12, 0xF000},
		{0x0F12, 0xF96A},
		{0x0F12, 0x0002},
		{0x0F12, 0x6DE9},
		{0x0F12, 0x6FE8},
		{0x0F12, 0x1A08},
		{0x0F12, 0x4351},
		{0x0F12, 0x0300},
		{0x0F12, 0x1C49},
		{0x0F12, 0xF000},
		{0x0F12, 0xF953},
		{0x0F12, 0x0401},
		{0x0F12, 0x0430},
		{0x0F12, 0x0C00},
		{0x0F12, 0x4301},
		{0x0F12, 0x61F9},
		{0x0F12, 0xE004},
		{0x0F12, 0x00A2},
		{0x0F12, 0x4990},
		{0x0F12, 0x1810},
		{0x0F12, 0x3080},
		{0x0F12, 0x61C1},
		{0x0F12, 0x1E64},
		{0x0F12, 0xD2C5},
		{0x0F12, 0x2006},
		{0x0F12, 0xF000},
		{0x0F12, 0xF959},
		{0x0F12, 0x2007},
		{0x0F12, 0xF000},
		{0x0F12, 0xF956},
		{0x0F12, 0xBCF8},
		{0x0F12, 0xBC08},
		{0x0F12, 0x4718},
		{0x0F12, 0xB510},
		{0x0F12, 0xF000},
		{0x0F12, 0xF958},
		{0x0F12, 0x2800},
		{0x0F12, 0xD00A},
		{0x0F12, 0x4881},
		{0x0F12, 0x8B81},
		{0x0F12, 0x0089},
		{0x0F12, 0x1808},
		{0x0F12, 0x6DC1},
		{0x0F12, 0x4883},
		{0x0F12, 0x8A80},
		{0x0F12, 0x4281},
		{0x0F12, 0xD901},
		{0x0F12, 0x2001},
		{0x0F12, 0xE7A1},
		{0x0F12, 0x2000},
		{0x0F12, 0xE79F},
		{0x0F12, 0xB5F8},
		{0x0F12, 0x0004},
		{0x0F12, 0x4F80},
		{0x0F12, 0x227D},
		{0x0F12, 0x8938},
		{0x0F12, 0x0152},
		{0x0F12, 0x4342},
		{0x0F12, 0x487E},
		{0x0F12, 0x9000},
		{0x0F12, 0x8A01},
		{0x0F12, 0x0848},
		{0x0F12, 0x1810},
		{0x0F12, 0xF000},
		{0x0F12, 0xF91D},
		{0x0F12, 0x210F},
		{0x0F12, 0xF000},
		{0x0F12, 0xF940},
		{0x0F12, 0x497A},
		{0x0F12, 0x8C49},
		{0x0F12, 0x090E},
		{0x0F12, 0x0136},
		{0x0F12, 0x4306},
		{0x0F12, 0x4979},
		{0x0F12, 0x2C00},
		{0x0F12, 0xD003},
		{0x0F12, 0x2001},
		{0x0F12, 0x0240},
		{0x0F12, 0x4330},
		{0x0F12, 0x8108},
		{0x0F12, 0x4876},
		{0x0F12, 0x2C00},
		{0x0F12, 0x8D00},
		{0x0F12, 0xD001},
		{0x0F12, 0x2501},
		{0x0F12, 0xE000},
		{0x0F12, 0x2500},
		{0x0F12, 0x4972},
		{0x0F12, 0x4328},
		{0x0F12, 0x8008},
		{0x0F12, 0x207D},
		{0x0F12, 0x00C0},
		{0x0F12, 0xF000},
		{0x0F12, 0xF92E},
		{0x0F12, 0x2C00},
		{0x0F12, 0x496E},
		{0x0F12, 0x0328},
		{0x0F12, 0x4330},
		{0x0F12, 0x8108},
		{0x0F12, 0x88F8},
		{0x0F12, 0x2C00},
		{0x0F12, 0x01AA},
		{0x0F12, 0x4310},
		{0x0F12, 0x8088},
		{0x0F12, 0x9800},
		{0x0F12, 0x8A01},
		{0x0F12, 0x486A},
		{0x0F12, 0xF000},
		{0x0F12, 0xF8F1},
		{0x0F12, 0x496A},
		{0x0F12, 0x8809},
		{0x0F12, 0x4348},
		{0x0F12, 0x0400},
		{0x0F12, 0x0C00},
		{0x0F12, 0xF000},
		{0x0F12, 0xF918},
		{0x0F12, 0x0020},
		{0x0F12, 0xF000},
		{0x0F12, 0xF91D},
		{0x0F12, 0x4866},
		{0x0F12, 0x7004},
		{0x0F12, 0xE7A3},
		{0x0F12, 0xB510},
		{0x0F12, 0x0004},
		{0x0F12, 0xF000},
		{0x0F12, 0xF91E},
		{0x0F12, 0x6020},
		{0x0F12, 0x4963},
		{0x0F12, 0x8B49},
		{0x0F12, 0x0789},
		{0x0F12, 0xD001},
		{0x0F12, 0x0040},
		{0x0F12, 0x6020},
		{0x0F12, 0xE74C},
		{0x0F12, 0xB510},
		{0x0F12, 0xF000},
		{0x0F12, 0xF91B},
		{0x0F12, 0x485F},
		{0x0F12, 0x8880},
		{0x0F12, 0x0601},
		{0x0F12, 0x4854},
		{0x0F12, 0x1609},
		{0x0F12, 0x8141},
		{0x0F12, 0xE742},
		{0x0F12, 0xB5F8},
		{0x0F12, 0x000F},
		{0x0F12, 0x4C55},
		{0x0F12, 0x3420},
		{0x0F12, 0x2500},
		{0x0F12, 0x5765},
		{0x0F12, 0x0039},
		{0x0F12, 0xF000},
		{0x0F12, 0xF913},
		{0x0F12, 0x9000},
		{0x0F12, 0x2600},
		{0x0F12, 0x57A6},
		{0x0F12, 0x4C4C},
		{0x0F12, 0x42AE},
		{0x0F12, 0xD01B},
		{0x0F12, 0x4D54},
		{0x0F12, 0x8AE8},
		{0x0F12, 0x2800},
		{0x0F12, 0xD013},
		{0x0F12, 0x484D},
		{0x0F12, 0x8A01},
		{0x0F12, 0x8B80},
		{0x0F12, 0x4378},
		{0x0F12, 0xF000},
		{0x0F12, 0xF8B5},
		{0x0F12, 0x89A9},
		{0x0F12, 0x1A41},
		{0x0F12, 0x484E},
		{0x0F12, 0x3820},
		{0x0F12, 0x8AC0},
		{0x0F12, 0x4348},
		{0x0F12, 0x17C1},
		{0x0F12, 0x0D89},
		{0x0F12, 0x1808},
		{0x0F12, 0x1280},
		{0x0F12, 0x8961},
		{0x0F12, 0x1A08},
		{0x0F12, 0x8160},
		{0x0F12, 0xE003},
		{0x0F12, 0x88A8},
		{0x0F12, 0x0600},
		{0x0F12, 0x1600},
		{0x0F12, 0x8160},
		{0x0F12, 0x200A},
		{0x0F12, 0x5E20},
		{0x0F12, 0x42B0},
		{0x0F12, 0xD011},
		{0x0F12, 0xF000},
		{0x0F12, 0xF8AB},
		{0x0F12, 0x1D40},
		{0x0F12, 0x00C3},
		{0x0F12, 0x1A18},
		{0x0F12, 0x214B},
		{0x0F12, 0xF000},
		{0x0F12, 0xF897},
		{0x0F12, 0x211F},
		{0x0F12, 0xF000},
		{0x0F12, 0xF8BA},
		{0x0F12, 0x210A},
		{0x0F12, 0x5E61},
		{0x0F12, 0x0FC9},
		{0x0F12, 0x0149},
		{0x0F12, 0x4301},
		{0x0F12, 0x483D},
		{0x0F12, 0x81C1},
		{0x0F12, 0x9800},
		{0x0F12, 0xE74A},
		{0x0F12, 0xB5F1},
		{0x0F12, 0xB082},
		{0x0F12, 0x2500},
		{0x0F12, 0x483A},
		{0x0F12, 0x9001},
		{0x0F12, 0x2400},
		{0x0F12, 0x2028},
		{0x0F12, 0x4368},
		{0x0F12, 0x4A39},
		{0x0F12, 0x4925},
		{0x0F12, 0x1887},
		{0x0F12, 0x1840},
		{0x0F12, 0x9000},
		{0x0F12, 0x9800},
		{0x0F12, 0x0066},
		{0x0F12, 0x9A01},
		{0x0F12, 0x1980},
		{0x0F12, 0x218C},
		{0x0F12, 0x5A09},
		{0x0F12, 0x8A80},
		{0x0F12, 0x8812},
		{0x0F12, 0xF000},
		{0x0F12, 0xF8CA},
		{0x0F12, 0x53B8},
		{0x0F12, 0x1C64},
		{0x0F12, 0x2C14},
		{0x0F12, 0xDBF1},
		{0x0F12, 0x1C6D},
		{0x0F12, 0x2D03},
		{0x0F12, 0xDBE6},
		{0x0F12, 0x9802},
		{0x0F12, 0x6800},
		{0x0F12, 0x0600},
		{0x0F12, 0x0E00},
		{0x0F12, 0xF000},
		{0x0F12, 0xF8C5},
		{0x0F12, 0xBCFE},
		{0x0F12, 0xBC08},
		{0x0F12, 0x4718},
		{0x0F12, 0xB570},
		{0x0F12, 0x6805},
		{0x0F12, 0x2404},
		{0x0F12, 0xF000},
		{0x0F12, 0xF8C5},
		{0x0F12, 0x2800},
		{0x0F12, 0xD103},
		{0x0F12, 0xF000},
		{0x0F12, 0xF8C9},
		{0x0F12, 0x2800},
		{0x0F12, 0xD000},
		{0x0F12, 0x2400},
		{0x0F12, 0x3540},
		{0x0F12, 0x88E8},
		{0x0F12, 0x0500},
		{0x0F12, 0xD403},
		{0x0F12, 0x4822},
		{0x0F12, 0x89C0},
		{0x0F12, 0x2800},
		{0x0F12, 0xD002},
		{0x0F12, 0x2008},
		{0x0F12, 0x4304},
		{0x0F12, 0xE001},
		{0x0F12, 0x2010},
		{0x0F12, 0x4304},
		{0x0F12, 0x481F},
		{0x0F12, 0x8B80},
		{0x0F12, 0x0700},
		{0x0F12, 0x0F81},
		{0x0F12, 0x2001},
		{0x0F12, 0x2900},
		{0x0F12, 0xD000},
		{0x0F12, 0x4304},
		{0x0F12, 0x491C},
		{0x0F12, 0x8B0A},
		{0x0F12, 0x42A2},
		{0x0F12, 0xD004},
		{0x0F12, 0x0762},
		{0x0F12, 0xD502},
		{0x0F12, 0x4A19},
		{0x0F12, 0x3220},
		{0x0F12, 0x8110},
		{0x0F12, 0x830C},
		{0x0F12, 0xE691},
		{0x0F12, 0x0C3C},
		{0x0F12, 0x7000},
		{0x0F12, 0x3274},
		{0x0F12, 0x7000},
		{0x0F12, 0x26E8},
		{0x0F12, 0x7000},
		{0x0F12, 0x6100},
		{0x0F12, 0xD000},
		{0x0F12, 0x6500},
		{0x0F12, 0xD000},
		{0x0F12, 0x1A7C},
		{0x0F12, 0x7000},
		{0x0F12, 0x1120},
		{0x0F12, 0x7000},
		{0x0F12, 0xFFFF},
		{0x0F12, 0x0000},
		{0x0F12, 0x3374},
		{0x0F12, 0x7000},
		{0x0F12, 0x1D6C},
		{0x0F12, 0x7000},
		{0x0F12, 0x167C},
		{0x0F12, 0x7000},
		{0x0F12, 0xF400},
		{0x0F12, 0xD000},
		{0x0F12, 0x2C2C},
		{0x0F12, 0x7000},
		{0x0F12, 0x40A0},
		{0x0F12, 0x00DD},
		{0x0F12, 0xF520},
		{0x0F12, 0xD000},
		{0x0F12, 0x2C29},
		{0x0F12, 0x7000},
		{0x0F12, 0x1A54},
		{0x0F12, 0x7000},
		{0x0F12, 0x1564},
		{0x0F12, 0x7000},
		{0x0F12, 0xF2A0},
		{0x0F12, 0xD000},
		{0x0F12, 0x2440},
		{0x0F12, 0x7000},
		{0x0F12, 0x05A0},
		{0x0F12, 0x7000},
		{0x0F12, 0x2894},
		{0x0F12, 0x7000},
		{0x0F12, 0x1224},
		{0x0F12, 0x7000},
		{0x0F12, 0xB000},
		{0x0F12, 0xD000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0x1A3F},
		{0x0F12, 0x0001},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xF004},
		{0x0F12, 0xE51F},
		{0x0F12, 0x1F48},
		{0x0F12, 0x0001},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0x24BD},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0x36DD},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0xB4CF},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0xB5D7},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0x36ED},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0xF53F},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0xF5D9},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0x013D},
		{0x0F12, 0x0001},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0xF5C9},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0xFAA9},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0x3723},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0x5823},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0xD771},
		{0x0F12, 0x0000},
		{0x0F12, 0x4778},
		{0x0F12, 0x46C0},
		{0x0F12, 0xC000},
		{0x0F12, 0xE59F},
		{0x0F12, 0xFF1C},
		{0x0F12, 0xE12F},
		{0x0F12, 0xD75B},
		{0x0F12, 0x0000},
		{0x0F12, 0x8117},
		{0x0F12, 0x0000},
		//
		// Parameters Defined in T&P:
		// Mon_SARR_usGammaLutRGBInterpolate		  120 700005A0 ARRAY
		// Mon_SARR_usGammaLutRGBInterpolate[0][0]		2 700005A0 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][1]		2 700005A2 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][2]		2 700005A4 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][3]		2 700005A6 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][4]		2 700005A8 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][5]		2 700005AA SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][6]		2 700005AC SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][7]		2 700005AE SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][8]		2 700005B0 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][9]		2 700005B2 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][10] 	2 700005B4 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][11] 	2 700005B6 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][12] 	2 700005B8 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][13] 	2 700005BA SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][14] 	2 700005BC SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][15] 	2 700005BE SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][16] 	2 700005C0 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][17] 	2 700005C2 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][18] 	2 700005C4 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[0][19] 	2 700005C6 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][0]		2 700005C8 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][1]		2 700005CA SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][2]		2 700005CC SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][3]		2 700005CE SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][4]		2 700005D0 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][5]		2 700005D2 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][6]		2 700005D4 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][7]		2 700005D6 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][8]		2 700005D8 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][9]		2 700005DA SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][10] 	2 700005DC SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][11] 	2 700005DE SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][12] 	2 700005E0 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][13] 	2 700005E2 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][14] 	2 700005E4 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][15] 	2 700005E6 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][16] 	2 700005E8 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][17] 	2 700005EA SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][18] 	2 700005EC SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[1][19] 	2 700005EE SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][0]		2 700005F0 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][1]		2 700005F2 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][2]		2 700005F4 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][3]		2 700005F6 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][4]		2 700005F8 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][5]		2 700005FA SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][6]		2 700005FC SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][7]		2 700005FE SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][8]		2 70000600 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][9]		2 70000602 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][10] 	2 70000604 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][11] 	2 70000606 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][12] 	2 70000608 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][13] 	2 7000060A SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][14] 	2 7000060C SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][15] 	2 7000060E SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][16] 	2 70000610 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][17] 	2 70000612 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][18] 	2 70000614 SHORT
		// Mon_SARR_usGammaLutRGBInterpolate[2][19] 	2 70000616 SHORT
		// TnP_SvnVersion								2 7000326C SHORT
		// Tune_TP									  268 70003274 STRUCT
		// Tune_TP_IO_DrivingCurrent_D0_D4_cs10 		2 70003274 SHORT
		// Tune_TP_IO_DrivingCurrent_D9_D5_cs10 		2 70003276 SHORT
		// Tune_TP_IO_DrivingCurrent_GPIO_cd10			2 70003278 SHORT
		// Tune_TP_IO_DrivingCurrent_CLKs_cd10			2 7000327A SHORT
		// Tune_TP_atop_dblr_reg_1						2 7000327C SHORT
		// Tune_TP_atop_dblr_reg_3						2 7000327E SHORT
		// Tune_TP_atop_ramp_reg_1						2 70003280 SHORT
		// Tune_TP_atop_ramp_reg_2						2 70003282 SHORT
		// Tune_TP_atop_rmp_offset_sig					2 70003284 SHORT
		// Tune_TP_bEnablePrePostGammaAfControl 		2 70003286 SHORT
		// Tune_TP_seti 							  240 70003288 STRUCT
		// SARR_usDualGammaLutRGBIndoor 			  120 70003288 ARRAY
		// SARR_usDualGammaLutRGBIndoor[0][0]			2 70003288 SHORT
		// SARR_usDualGammaLutRGBIndoor[0][1]			2 7000328A SHORT
		// SARR_usDualGammaLutRGBIndoor[0][2]			2 7000328C SHORT
		// SARR_usDualGammaLutRGBIndoor[0][3]			2 7000328E SHORT
		// SARR_usDualGammaLutRGBIndoor[0][4]			2 70003290 SHORT
		// SARR_usDualGammaLutRGBIndoor[0][5]			2 70003292 SHORT
		// SARR_usDualGammaLutRGBIndoor[0][6]			2 70003294 SHORT
		// SARR_usDualGammaLutRGBIndoor[0][7]			2 70003296 SHORT
		// SARR_usDualGammaLutRGBIndoor[0][8]			2 70003298 SHORT
		// SARR_usDualGammaLutRGBIndoor[0][9]			2 7000329A SHORT
		// SARR_usDualGammaLutRGBIndoor[0][10]			2 7000329C SHORT
		// SARR_usDualGammaLutRGBIndoor[0][11]			2 7000329E SHORT
		// SARR_usDualGammaLutRGBIndoor[0][12]			2 700032A0 SHORT
		// SARR_usDualGammaLutRGBIndoor[0][13]			2 700032A2 SHORT
		// SARR_usDualGammaLutRGBIndoor[0][14]			2 700032A4 SHORT
		// SARR_usDualGammaLutRGBIndoor[0][15]			2 700032A6 SHORT
		// SARR_usDualGammaLutRGBIndoor[0][16]			2 700032A8 SHORT
		// SARR_usDualGammaLutRGBIndoor[0][17]			2 700032AA SHORT
		// SARR_usDualGammaLutRGBIndoor[0][18]			2 700032AC SHORT
		// SARR_usDualGammaLutRGBIndoor[0][19]			2 700032AE SHORT
		// SARR_usDualGammaLutRGBIndoor[1][0]			2 700032B0 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][1]			2 700032B2 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][2]			2 700032B4 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][3]			2 700032B6 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][4]			2 700032B8 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][5]			2 700032BA SHORT
		// SARR_usDualGammaLutRGBIndoor[1][6]			2 700032BC SHORT
		// SARR_usDualGammaLutRGBIndoor[1][7]			2 700032BE SHORT
		// SARR_usDualGammaLutRGBIndoor[1][8]			2 700032C0 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][9]			2 700032C2 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][10]			2 700032C4 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][11]			2 700032C6 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][12]			2 700032C8 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][13]			2 700032CA SHORT
		// SARR_usDualGammaLutRGBIndoor[1][14]			2 700032CC SHORT
		// SARR_usDualGammaLutRGBIndoor[1][15]			2 700032CE SHORT
		// SARR_usDualGammaLutRGBIndoor[1][16]			2 700032D0 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][17]			2 700032D2 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][18]			2 700032D4 SHORT
		// SARR_usDualGammaLutRGBIndoor[1][19]			2 700032D6 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][0]			2 700032D8 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][1]			2 700032DA SHORT
		// SARR_usDualGammaLutRGBIndoor[2][2]			2 700032DC SHORT
		// SARR_usDualGammaLutRGBIndoor[2][3]			2 700032DE SHORT
		// SARR_usDualGammaLutRGBIndoor[2][4]			2 700032E0 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][5]			2 700032E2 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][6]			2 700032E4 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][7]			2 700032E6 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][8]			2 700032E8 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][9]			2 700032EA SHORT
		// SARR_usDualGammaLutRGBIndoor[2][10]			2 700032EC SHORT
		// SARR_usDualGammaLutRGBIndoor[2][11]			2 700032EE SHORT
		// SARR_usDualGammaLutRGBIndoor[2][12]			2 700032F0 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][13]			2 700032F2 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][14]			2 700032F4 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][15]			2 700032F6 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][16]			2 700032F8 SHORT
		// SARR_usDualGammaLutRGBIndoor[2][17]			2 700032FA SHORT
		// SARR_usDualGammaLutRGBIndoor[2][18]			2 700032FC SHORT
		// SARR_usDualGammaLutRGBIndoor[2][19]			2 700032FE SHORT
		// SARR_usDualGammaLutRGBOutdoor			  120 70003300 ARRAY
		// SARR_usDualGammaLutRGBOutdoor[0][0]			2 70003300 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][1]			2 70003302 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][2]			2 70003304 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][3]			2 70003306 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][4]			2 70003308 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][5]			2 7000330A SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][6]			2 7000330C SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][7]			2 7000330E SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][8]			2 70003310 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][9]			2 70003312 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][10] 		2 70003314 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][11] 		2 70003316 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][12] 		2 70003318 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][13] 		2 7000331A SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][14] 		2 7000331C SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][15] 		2 7000331E SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][16] 		2 70003320 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][17] 		2 70003322 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][18] 		2 70003324 SHORT
		// SARR_usDualGammaLutRGBOutdoor[0][19] 		2 70003326 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][0]			2 70003328 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][1]			2 7000332A SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][2]			2 7000332C SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][3]			2 7000332E SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][4]			2 70003330 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][5]			2 70003332 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][6]			2 70003334 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][7]			2 70003336 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][8]			2 70003338 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][9]			2 7000333A SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][10] 		2 7000333C SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][11] 		2 7000333E SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][12] 		2 70003340 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][13] 		2 70003342 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][14] 		2 70003344 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][15] 		2 70003346 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][16] 		2 70003348 SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][17] 		2 7000334A SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][18] 		2 7000334C SHORT
		// SARR_usDualGammaLutRGBOutdoor[1][19] 		2 7000334E SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][0]			2 70003350 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][1]			2 70003352 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][2]			2 70003354 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][3]			2 70003356 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][4]			2 70003358 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][5]			2 7000335A SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][6]			2 7000335C SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][7]			2 7000335E SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][8]			2 70003360 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][9]			2 70003362 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][10] 		2 70003364 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][11] 		2 70003366 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][12] 		2 70003368 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][13] 		2 7000336A SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][14] 		2 7000336C SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][15] 		2 7000336E SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][16] 		2 70003370 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][17] 		2 70003372 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][18] 		2 70003374 SHORT
		// SARR_usDualGammaLutRGBOutdoor[2][19] 		2 70003376 SHORT
		// Tune_TP_reserved 							2 70003378 SHORT
		// Tune_TP_atop_dbus_reg						2 7000337A SHORT
		// Tune_TP_dblr_base_freq_mhz					2 7000337C SHORT
		// Tune_TP_GL_sen_sOfs							2 7000337E SHORT
		//
		// End T&P part
		
		//========================================================						
		// CIs/APs/An setting		 - 400LsB  sYsCLK 32MHz 							
		//========================================================						
		// This regis are for FACTORY ONLY. If you change it without prior notification,
		// YOU are REsIBLE for the FAILURE that will happen in the future.				
		//========================================================						
		
		
		{0x002A, 0x157A},
		{0x0F12, 0x0001},
		{0x002A, 0x1578},
		{0x0F12, 0x0001},
		{0x002A, 0x1576},
		{0x0F12, 0x0020},
		{0x002A, 0x1574},
		{0x0F12, 0x0006},
		{0x002A, 0x156E},
		{0x0F12, 0x0001},	// slope calibration tolerance in units of 1/256	
		{0x002A, 0x1568},
		{0x0F12, 0x00FC},
			  
		//ADC control 
		{0x002A, 0x155A},
		{0x0F12, 0x01CC},	//ADC sAT of 450mV for 10bit default in EVT1							
		{0x002A, 0x157E},																		 
		{0x0F12, 0x0C80},	// 3200 Max. Reset ramp DCLK counts (default 2048 0x800)			 
		{0x0F12, 0x0578},	// 1400 Max. Reset ramp DCLK counts for x3.5						 
		{0x002A, 0x157C},																		 
		{0x0F12, 0x0190},	// 400 Reset ramp for x1 in DCLK counts 							 
		{0x002A, 0x1570},																		 
		{0x0F12, 0x00A0},	// 160 LsB															 
		{0x0F12, 0x0010},	// reset threshold													 
		{0x002A, 0x12C4},																		 
		{0x0F12, 0x006A},	// 106 additional timing columns.									 
		{0x002A, 0x12C8},																		 
		{0x0F12, 0x08AC},	// 2220 ADC columns in normal mode including Hold & Latch			 
		{0x0F12, 0x0050},	// 80 addition of ADC columns in Y-ave mode (default 244 0x74)
			  
		{0x002A, 0x1696},	// based on APs guidelines						  
		{0x0F12, 0x0000},	// based on APs guidelines						  
		{0x0F12, 0x0000},	// default. 1492 used for ADC dark characteristics
		{0x0F12, 0x00C6},	// default. 1492 used for ADC dark characteristics
		{0x0F12, 0x00C6},																										
																		   
		{0x002A, 0x1690},	// when set double sampling is activated - requires different set of pointers												  
		{0x0F12, 0x0001},													  
																		   
		{0x002A, 0x12B0},	// comp and pixel bias control 0xF40E - default for EVT1																	  
		{0x0F12, 0x0055},	// comp and pixel bias control 0xF40E for binning mode																		  
		{0x0F12, 0x005A},													  
																		   
		{0x002A, 0x337A},	// [7] - is used for rest-only mode (EVT0 value is 0xD and HW 0x6)															  
		{0x0F12, 0x0006},
		{0x0F12, 0x0068},
		{0x002A, 0x169E},
		{0x0F12, 0x0007},
		{0x002A, 0x0BF6},
		{0x0F12, 0x0000},
					   
						 
		{0x002A, 0x327C},
		{0x0F12, 0x1000},
		{0x0F12, 0x6998},
		{0x0F12, 0x0078},
		{0x0F12, 0x04FE},
		{0x0F12, 0x8800},
						 
		{0x002A, 0x3274},
#if 0    //modified by huangbo
		{0x0F12, 0x0100},	//set IO driving current 2mA for Gs500
		{0x0F12, 0x0100},	//set IO driving current			  
		{0x0F12, 0x1555},	//set IO driving current			  
		{0x0F12, 0x05d5},	//set IO driving current		
#else
		{0x0F12, 0x0155},	//set IO driving current 2mA for Gs500
		{0x0F12, 0x0155},	//set IO driving current			  
		{0x0F12, 0x1555},	//set IO driving current			  
		{0x0F12, 0x0555},	//set IO driving current			  
#endif		
		{0x0028, 0x7000},
		{0x002A, 0x0572},
		{0x0F12, 0x0007},	//#skl_usConfigStbySettings // Enable T&P code after HW stby + skip ZI part on HW wakeup.
						  
		{0x0028, 0x7000},	
		{0x002A, 0x12D2},	   
		{0x0F12, 0x0003},	//senHal_pContSenModesRegsArray[0][0]2 700012D2 	
		{0x0F12, 0x0003},	//senHal_pContSenModesRegsArray[0][1]2 700012D4  
		{0x0F12, 0x0003},	//senHal_pContSenModesRegsArray[0][2]2 700012D6  
		{0x0F12, 0x0003},	//senHal_pContSenModesRegsArray[0][3]2 700012D8  
		{0x0F12, 0x0884},	//senHal_pContSenModesRegsArray[1][0]2 700012DA  
		{0x0F12, 0x08CF},	//senHal_pContSenModesRegsArray[1][1]2 700012DC  
		{0x0F12, 0x0500},	//senHal_pContSenModesRegsArray[1][2]2 700012DE  
		{0x0F12, 0x054B},	//senHal_pContSenModesRegsArray[1][3]2 700012E0  
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[2][0]2 700012E2  
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[2][1]2 700012E4  
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[2][2]2 700012E6  
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[2][3]2 700012E8  
		{0x0F12, 0x0885},	//senHal_pContSenModesRegsArray[3][0]2 700012EA  
		{0x0F12, 0x0467},	//senHal_pContSenModesRegsArray[3][1]2 700012EC  
		{0x0F12, 0x0501},	//senHal_pContSenModesRegsArray[3][2]2 700012EE  
		{0x0F12, 0x02A5},	//senHal_pContSenModesRegsArray[3][3]2 700012F0  
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[4][0]2 700012F2  
		{0x0F12, 0x046A},	//senHal_pContSenModesRegsArray[4][1]2 700012F4  
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[4][2]2 700012F6  
		{0x0F12, 0x02A8},	//senHal_pContSenModesRegsArray[4][3]2 700012F8  
		{0x0F12, 0x0885},	//senHal_pContSenModesRegsArray[5][0]2 700012FA  
		{0x0F12, 0x08D0},	//senHal_pContSenModesRegsArray[5][1]2 700012FC  
		{0x0F12, 0x0501},	//senHal_pContSenModesRegsArray[5][2]2 700012FE  
		{0x0F12, 0x054C},	//senHal_pContSenModesRegsArray[5][3]2 70001300  
		{0x0F12, 0x0006},	//senHal_pContSenModesRegsArray[6][0]2 70001302  
		{0x0F12, 0x0020},	//senHal_pContSenModesRegsArray[6][1]2 70001304  
		{0x0F12, 0x0006},	//senHal_pContSenModesRegsArray[6][2]2 70001306  
		{0x0F12, 0x0020},	//senHal_pContSenModesRegsArray[6][3]2 70001308  
		{0x0F12, 0x0881},	//senHal_pContSenModesRegsArray[7][0]2 7000130A  
		{0x0F12, 0x0463},	//senHal_pContSenModesRegsArray[7][1]2 7000130C  
		{0x0F12, 0x04FD},	//senHal_pContSenModesRegsArray[7][2]2 7000130E  
		{0x0F12, 0x02A1},	//senHal_pContSenModesRegsArray[7][3]2 70001310  
		{0x0F12, 0x0006},	//senHal_pContSenModesRegsArray[8][0]2 70001312  
		{0x0F12, 0x0489},	//senHal_pContSenModesRegsArray[8][1]2 70001314  
		{0x0F12, 0x0006},	//senHal_pContSenModesRegsArray[8][2]2 70001316  
		{0x0F12, 0x02C7},	//senHal_pContSenModesRegsArray[8][3]2 70001318  
		{0x0F12, 0x0881},	//senHal_pContSenModesRegsArray[9][0]2 7000131A  
		{0x0F12, 0x08CC},	//senHal_pContSenModesRegsArray[9][1]2 7000131C  
		{0x0F12, 0x04FD},	//senHal_pContSenModesRegsArray[9][2]2 7000131E  
		{0x0F12, 0x0548},	//senHal_pContSenModesRegsArray[9][3]2 70001320  
		{0x0F12, 0x03A2},	//senHal_pContSenModesRegsArray[10][0] 2 70001322
		{0x0F12, 0x01D3},	//senHal_pContSenModesRegsArray[10][1] 2 70001324
		{0x0F12, 0x01E0},	//senHal_pContSenModesRegsArray[10][2] 2 70001326
		{0x0F12, 0x00F2},	//senHal_pContSenModesRegsArray[10][3] 2 70001328
		{0x0F12, 0x03F2},	//senHal_pContSenModesRegsArray[11][0] 2 7000132A
		{0x0F12, 0x0223},	//senHal_pContSenModesRegsArray[11][1] 2 7000132C
		{0x0F12, 0x0230},	//senHal_pContSenModesRegsArray[11][2] 2 7000132E
		{0x0F12, 0x0142},	//senHal_pContSenModesRegsArray[11][3] 2 70001330
		{0x0F12, 0x03A2},	//senHal_pContSenModesRegsArray[12][0] 2 70001332
		{0x0F12, 0x063C},	//senHal_pContSenModesRegsArray[12][1] 2 70001334
		{0x0F12, 0x01E0},	//senHal_pContSenModesRegsArray[12][2] 2 70001336
		{0x0F12, 0x0399},	//senHal_pContSenModesRegsArray[12][3] 2 70001338
		{0x0F12, 0x03F2},	//senHal_pContSenModesRegsArray[13][0] 2 7000133A
		{0x0F12, 0x068C},	//senHal_pContSenModesRegsArray[13][1] 2 7000133C
		{0x0F12, 0x0230},	//senHal_pContSenModesRegsArray[13][2] 2 7000133E
		{0x0F12, 0x03E9},	//senHal_pContSenModesRegsArray[13][3] 2 70001340
		{0x0F12, 0x0002},	//senHal_pContSenModesRegsArray[14][0] 2 70001342
		{0x0F12, 0x0002},	//senHal_pContSenModesRegsArray[14][1] 2 70001344
		{0x0F12, 0x0002},	//senHal_pContSenModesRegsArray[14][2] 2 70001346
		{0x0F12, 0x0002},	//senHal_pContSenModesRegsArray[14][3] 2 70001348
		{0x0F12, 0x003C},	//senHal_pContSenModesRegsArray[15][0] 2 7000134A
		{0x0F12, 0x003C},	//senHal_pContSenModesRegsArray[15][1] 2 7000134C
		{0x0F12, 0x003C},	//senHal_pContSenModesRegsArray[15][2] 2 7000134E
		{0x0F12, 0x003C},	//senHal_pContSenModesRegsArray[15][3] 2 70001350
		{0x0F12, 0x01D3},	//senHal_pContSenModesRegsArray[16][0] 2 70001352
		{0x0F12, 0x01D3},	//senHal_pContSenModesRegsArray[16][1] 2 70001354
		{0x0F12, 0x00F2},	//senHal_pContSenModesRegsArray[16][2] 2 70001356
		{0x0F12, 0x00F2},	//senHal_pContSenModesRegsArray[16][3] 2 70001358
		{0x0F12, 0x020B},	//senHal_pContSenModesRegsArray[17][0] 2 7000135A
		{0x0F12, 0x024A},	//senHal_pContSenModesRegsArray[17][1] 2 7000135C
		{0x0F12, 0x012A},	//senHal_pContSenModesRegsArray[17][2] 2 7000135E
		{0x0F12, 0x0169},	//senHal_pContSenModesRegsArray[17][3] 2 70001360
		{0x0F12, 0x0002},	//senHal_pContSenModesRegsArray[18][0] 2 70001362
		{0x0F12, 0x046B},	//senHal_pContSenModesRegsArray[18][1] 2 70001364
		{0x0F12, 0x0002},	//senHal_pContSenModesRegsArray[18][2] 2 70001366
		{0x0F12, 0x02A9},	//senHal_pContSenModesRegsArray[18][3] 2 70001368
		{0x0F12, 0x0419},	//senHal_pContSenModesRegsArray[19][0] 2 7000136A
		{0x0F12, 0x04A5},	//senHal_pContSenModesRegsArray[19][1] 2 7000136C
		{0x0F12, 0x0257},	//senHal_pContSenModesRegsArray[19][2] 2 7000136E
		{0x0F12, 0x02E3},	//senHal_pContSenModesRegsArray[19][3] 2 70001370
		{0x0F12, 0x0630},	//senHal_pContSenModesRegsArray[20][0] 2 70001372
		{0x0F12, 0x063C},	//senHal_pContSenModesRegsArray[20][1] 2 70001374
		{0x0F12, 0x038D},	//senHal_pContSenModesRegsArray[20][2] 2 70001376
		{0x0F12, 0x0399},	//senHal_pContSenModesRegsArray[20][3] 2 70001378
		{0x0F12, 0x0668},	//senHal_pContSenModesRegsArray[21][0] 2 7000137A
		{0x0F12, 0x06B3},	//senHal_pContSenModesRegsArray[21][1] 2 7000137C
		{0x0F12, 0x03C5},	//senHal_pContSenModesRegsArray[21][2] 2 7000137E
		{0x0F12, 0x0410},	//senHal_pContSenModesRegsArray[21][3] 2 70001380
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[22][0] 2 70001382
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[22][1] 2 70001384
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[22][2] 2 70001386
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[22][3] 2 70001388
		{0x0F12, 0x03A2},	//senHal_pContSenModesRegsArray[23][0] 2 7000138A
		{0x0F12, 0x01D3},	//senHal_pContSenModesRegsArray[23][1] 2 7000138C
		{0x0F12, 0x01E0},	//senHal_pContSenModesRegsArray[23][2] 2 7000138E
		{0x0F12, 0x00F2},	//senHal_pContSenModesRegsArray[23][3] 2 70001390
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[24][0] 2 70001392
		{0x0F12, 0x0461},	//senHal_pContSenModesRegsArray[24][1] 2 70001394
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[24][2] 2 70001396
		{0x0F12, 0x029F},	//senHal_pContSenModesRegsArray[24][3] 2 70001398
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[25][0] 2 7000139A
		{0x0F12, 0x063C},	//senHal_pContSenModesRegsArray[25][1] 2 7000139C
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[25][2] 2 7000139E
		{0x0F12, 0x0399},	//senHal_pContSenModesRegsArray[25][3] 2 700013A0
		{0x0F12, 0x003D},	//senHal_pContSenModesRegsArray[26][0] 2 700013A2
		{0x0F12, 0x003D},	//senHal_pContSenModesRegsArray[26][1] 2 700013A4
		{0x0F12, 0x003D},	//senHal_pContSenModesRegsArray[26][2] 2 700013A6
		{0x0F12, 0x003D},	//senHal_pContSenModesRegsArray[26][3] 2 700013A8
		{0x0F12, 0x01D0},	//senHal_pContSenModesRegsArray[27][0] 2 700013AA
		{0x0F12, 0x01D0},	//senHal_pContSenModesRegsArray[27][1] 2 700013AC
		{0x0F12, 0x00EF},	//senHal_pContSenModesRegsArray[27][2] 2 700013AE
		{0x0F12, 0x00EF},	//senHal_pContSenModesRegsArray[27][3] 2 700013B0
		{0x0F12, 0x020C},	//senHal_pContSenModesRegsArray[28][0] 2 700013B2
		{0x0F12, 0x024B},	//senHal_pContSenModesRegsArray[28][1] 2 700013B4
		{0x0F12, 0x012B},	//senHal_pContSenModesRegsArray[28][2] 2 700013B6
		{0x0F12, 0x016A},	//senHal_pContSenModesRegsArray[28][3] 2 700013B8
		{0x0F12, 0x039F},	//senHal_pContSenModesRegsArray[29][0] 2 700013BA
		{0x0F12, 0x045E},	//senHal_pContSenModesRegsArray[29][1] 2 700013BC
		{0x0F12, 0x01DD},	//senHal_pContSenModesRegsArray[29][2] 2 700013BE
		{0x0F12, 0x029C},	//senHal_pContSenModesRegsArray[29][3] 2 700013C0
		{0x0F12, 0x041A},	//senHal_pContSenModesRegsArray[30][0] 2 700013C2
		{0x0F12, 0x04A6},	//senHal_pContSenModesRegsArray[30][1] 2 700013C4
		{0x0F12, 0x0258},	//senHal_pContSenModesRegsArray[30][2] 2 700013C6
		{0x0F12, 0x02E4},	//senHal_pContSenModesRegsArray[30][3] 2 700013C8
		{0x0F12, 0x062D},	//senHal_pContSenModesRegsArray[31][0] 2 700013CA
		{0x0F12, 0x0639},	//senHal_pContSenModesRegsArray[31][1] 2 700013CC
		{0x0F12, 0x038A},	//senHal_pContSenModesRegsArray[31][2] 2 700013CE
		{0x0F12, 0x0396},	//senHal_pContSenModesRegsArray[31][3] 2 700013D0
		{0x0F12, 0x0669},	//senHal_pContSenModesRegsArray[32][0] 2 700013D2
		{0x0F12, 0x06B4},	//senHal_pContSenModesRegsArray[32][1] 2 700013D4
		{0x0F12, 0x03C6},	//senHal_pContSenModesRegsArray[32][2] 2 700013D6
		{0x0F12, 0x0411},	//senHal_pContSenModesRegsArray[32][3] 2 700013D8
		{0x0F12, 0x087C},	//senHal_pContSenModesRegsArray[33][0] 2 700013DA
		{0x0F12, 0x08C7},	//senHal_pContSenModesRegsArray[33][1] 2 700013DC
		{0x0F12, 0x04F8},	//senHal_pContSenModesRegsArray[33][2] 2 700013DE
		{0x0F12, 0x0543},	//senHal_pContSenModesRegsArray[33][3] 2 700013E0
		{0x0F12, 0x0040},	//senHal_pContSenModesRegsArray[34][0] 2 700013E2
		{0x0F12, 0x0040},	//senHal_pContSenModesRegsArray[34][1] 2 700013E4
		{0x0F12, 0x0040},	//senHal_pContSenModesRegsArray[34][2] 2 700013E6
		{0x0F12, 0x0040},	//senHal_pContSenModesRegsArray[34][3] 2 700013E8
		{0x0F12, 0x01D0},	//senHal_pContSenModesRegsArray[35][0] 2 700013EA
		{0x0F12, 0x01D0},	//senHal_pContSenModesRegsArray[35][1] 2 700013EC
		{0x0F12, 0x00EF},	//senHal_pContSenModesRegsArray[35][2] 2 700013EE
		{0x0F12, 0x00EF},	//senHal_pContSenModesRegsArray[35][3] 2 700013F0
		{0x0F12, 0x020F},	//senHal_pContSenModesRegsArray[36][0] 2 700013F2
		{0x0F12, 0x024E},	//senHal_pContSenModesRegsArray[36][1] 2 700013F4
		{0x0F12, 0x012E},	//senHal_pContSenModesRegsArray[36][2] 2 700013F6
		{0x0F12, 0x016D},	//senHal_pContSenModesRegsArray[36][3] 2 700013F8
		{0x0F12, 0x039F},	//senHal_pContSenModesRegsArray[37][0] 2 700013FA
		{0x0F12, 0x045E},	//senHal_pContSenModesRegsArray[37][1] 2 700013FC
		{0x0F12, 0x01DD},	//senHal_pContSenModesRegsArray[37][2] 2 700013FE
		{0x0F12, 0x029C},	//senHal_pContSenModesRegsArray[37][3] 2 70001400
		{0x0F12, 0x041D},	//senHal_pContSenModesRegsArray[38][0] 2 70001402
		{0x0F12, 0x04A9},	//senHal_pContSenModesRegsArray[38][1] 2 70001404
		{0x0F12, 0x025B},	//senHal_pContSenModesRegsArray[38][2] 2 70001406
		{0x0F12, 0x02E7},	//senHal_pContSenModesRegsArray[38][3] 2 70001408
		{0x0F12, 0x062D},	//senHal_pContSenModesRegsArray[39][0] 2 7000140A
		{0x0F12, 0x0639},	//senHal_pContSenModesRegsArray[39][1] 2 7000140C
		{0x0F12, 0x038A},	//senHal_pContSenModesRegsArray[39][2] 2 7000140E
		{0x0F12, 0x0396},	//senHal_pContSenModesRegsArray[39][3] 2 70001410
		{0x0F12, 0x066C},	//senHal_pContSenModesRegsArray[40][0] 2 70001412
		{0x0F12, 0x06B7},	//senHal_pContSenModesRegsArray[40][1] 2 70001414
		{0x0F12, 0x03C9},	//senHal_pContSenModesRegsArray[40][2] 2 70001416
		{0x0F12, 0x0414},	//senHal_pContSenModesRegsArray[40][3] 2 70001418
		{0x0F12, 0x087C},	//senHal_pContSenModesRegsArray[41][0] 2 7000141A
		{0x0F12, 0x08C7},	//senHal_pContSenModesRegsArray[41][1] 2 7000141C
		{0x0F12, 0x04F8},	//senHal_pContSenModesRegsArray[41][2] 2 7000141E
		{0x0F12, 0x0543},	//senHal_pContSenModesRegsArray[41][3] 2 70001420
		{0x0F12, 0x0040},	//senHal_pContSenModesRegsArray[42][0] 2 70001422
		{0x0F12, 0x0040},	//senHal_pContSenModesRegsArray[42][1] 2 70001424
		{0x0F12, 0x0040},	//senHal_pContSenModesRegsArray[42][2] 2 70001426
		{0x0F12, 0x0040},	//senHal_pContSenModesRegsArray[42][3] 2 70001428
		{0x0F12, 0x01D0},	//senHal_pContSenModesRegsArray[43][0] 2 7000142A
		{0x0F12, 0x01D0},	//senHal_pContSenModesRegsArray[43][1] 2 7000142C
		{0x0F12, 0x00EF},	//senHal_pContSenModesRegsArray[43][2] 2 7000142E
		{0x0F12, 0x00EF},	//senHal_pContSenModesRegsArray[43][3] 2 70001430
		{0x0F12, 0x020F},	//senHal_pContSenModesRegsArray[44][0] 2 70001432
		{0x0F12, 0x024E},	//senHal_pContSenModesRegsArray[44][1] 2 70001434
		{0x0F12, 0x012E},	//senHal_pContSenModesRegsArray[44][2] 2 70001436
		{0x0F12, 0x016D},	//senHal_pContSenModesRegsArray[44][3] 2 70001438
		{0x0F12, 0x039F},	//senHal_pContSenModesRegsArray[45][0] 2 7000143A
		{0x0F12, 0x045E},	//senHal_pContSenModesRegsArray[45][1] 2 7000143C
		{0x0F12, 0x01DD},	//senHal_pContSenModesRegsArray[45][2] 2 7000143E
		{0x0F12, 0x029C},	//senHal_pContSenModesRegsArray[45][3] 2 70001440
		{0x0F12, 0x041D},	//senHal_pContSenModesRegsArray[46][0] 2 70001442
		{0x0F12, 0x04A9},	//senHal_pContSenModesRegsArray[46][1] 2 70001444
		{0x0F12, 0x025B},	//senHal_pContSenModesRegsArray[46][2] 2 70001446
		{0x0F12, 0x02E7},	//senHal_pContSenModesRegsArray[46][3] 2 70001448
		{0x0F12, 0x062D},	//senHal_pContSenModesRegsArray[47][0] 2 7000144A
		{0x0F12, 0x0639},	//senHal_pContSenModesRegsArray[47][1] 2 7000144C
		{0x0F12, 0x038A},	//senHal_pContSenModesRegsArray[47][2] 2 7000144E
		{0x0F12, 0x0396},	//senHal_pContSenModesRegsArray[47][3] 2 70001450
		{0x0F12, 0x066C},	//senHal_pContSenModesRegsArray[48][0] 2 70001452
		{0x0F12, 0x06B7},	//senHal_pContSenModesRegsArray[48][1] 2 70001454
		{0x0F12, 0x03C9},	//senHal_pContSenModesRegsArray[48][2] 2 70001456
		{0x0F12, 0x0414},	//senHal_pContSenModesRegsArray[48][3] 2 70001458
		{0x0F12, 0x087C},	//senHal_pContSenModesRegsArray[49][0] 2 7000145A
		{0x0F12, 0x08C7},	//senHal_pContSenModesRegsArray[49][1] 2 7000145C
		{0x0F12, 0x04F8},	//senHal_pContSenModesRegsArray[49][2] 2 7000145E
		{0x0F12, 0x0543},	//senHal_pContSenModesRegsArray[49][3] 2 70001460
		{0x0F12, 0x003D},	//senHal_pContSenModesRegsArray[50][0] 2 70001462
		{0x0F12, 0x003D},	//senHal_pContSenModesRegsArray[50][1] 2 70001464
		{0x0F12, 0x003D},	//senHal_pContSenModesRegsArray[50][2] 2 70001466
		{0x0F12, 0x003D},	//senHal_pContSenModesRegsArray[50][3] 2 70001468
		{0x0F12, 0x01D2},	//senHal_pContSenModesRegsArray[51][0] 2 7000146A
		{0x0F12, 0x01D2},	//senHal_pContSenModesRegsArray[51][1] 2 7000146C
		{0x0F12, 0x00F1},	//senHal_pContSenModesRegsArray[51][2] 2 7000146E
		{0x0F12, 0x00F1},	//senHal_pContSenModesRegsArray[51][3] 2 70001470
		{0x0F12, 0x020C},	//senHal_pContSenModesRegsArray[52][0] 2 70001472
		{0x0F12, 0x024B},	//senHal_pContSenModesRegsArray[52][1] 2 70001474
		{0x0F12, 0x012B},	//senHal_pContSenModesRegsArray[52][2] 2 70001476
		{0x0F12, 0x016A},	//senHal_pContSenModesRegsArray[52][3] 2 70001478
		{0x0F12, 0x03A1},	//senHal_pContSenModesRegsArray[53][0] 2 7000147A
		{0x0F12, 0x0460},	//senHal_pContSenModesRegsArray[53][1] 2 7000147C
		{0x0F12, 0x01DF},	//senHal_pContSenModesRegsArray[53][2] 2 7000147E
		{0x0F12, 0x029E},	//senHal_pContSenModesRegsArray[53][3] 2 70001480
		{0x0F12, 0x041A},	//senHal_pContSenModesRegsArray[54][0] 2 70001482
		{0x0F12, 0x04A6},	//senHal_pContSenModesRegsArray[54][1] 2 70001484
		{0x0F12, 0x0258},	//senHal_pContSenModesRegsArray[54][2] 2 70001486
		{0x0F12, 0x02E4},	//senHal_pContSenModesRegsArray[54][3] 2 70001488
		{0x0F12, 0x062F},	//senHal_pContSenModesRegsArray[55][0] 2 7000148A
		{0x0F12, 0x063B},	//senHal_pContSenModesRegsArray[55][1] 2 7000148C
		{0x0F12, 0x038C},	//senHal_pContSenModesRegsArray[55][2] 2 7000148E
		{0x0F12, 0x0398},	//senHal_pContSenModesRegsArray[55][3] 2 70001490
		{0x0F12, 0x0669},	//senHal_pContSenModesRegsArray[56][0] 2 70001492
		{0x0F12, 0x06B4},	//senHal_pContSenModesRegsArray[56][1] 2 70001494
		{0x0F12, 0x03C6},	//senHal_pContSenModesRegsArray[56][2] 2 70001496
		{0x0F12, 0x0411},	//senHal_pContSenModesRegsArray[56][3] 2 70001498
		{0x0F12, 0x087E},	//senHal_pContSenModesRegsArray[57][0] 2 7000149A
		{0x0F12, 0x08C9},	//senHal_pContSenModesRegsArray[57][1] 2 7000149C
		{0x0F12, 0x04FA},	//senHal_pContSenModesRegsArray[57][2] 2 7000149E
		{0x0F12, 0x0545},	//senHal_pContSenModesRegsArray[57][3] 2 700014A0
		{0x0F12, 0x03A2},	//senHal_pContSenModesRegsArray[58][0] 2 700014A2
		{0x0F12, 0x01D3},	//senHal_pContSenModesRegsArray[58][1] 2 700014A4
		{0x0F12, 0x01E0},	//senHal_pContSenModesRegsArray[58][2] 2 700014A6
		{0x0F12, 0x00F2},	//senHal_pContSenModesRegsArray[58][3] 2 700014A8
		{0x0F12, 0x03AF},	//senHal_pContSenModesRegsArray[59][0] 2 700014AA
		{0x0F12, 0x01E0},	//senHal_pContSenModesRegsArray[59][1] 2 700014AC
		{0x0F12, 0x01ED},	//senHal_pContSenModesRegsArray[59][2] 2 700014AE
		{0x0F12, 0x00FF},	//senHal_pContSenModesRegsArray[59][3] 2 700014B0
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[60][0] 2 700014B2
		{0x0F12, 0x0461},	//senHal_pContSenModesRegsArray[60][1] 2 700014B4
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[60][2] 2 700014B6
		{0x0F12, 0x029F},	//senHal_pContSenModesRegsArray[60][3] 2 700014B8
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[61][0] 2 700014BA
		{0x0F12, 0x046E},	//senHal_pContSenModesRegsArray[61][1] 2 700014BC
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[61][2] 2 700014BE
		{0x0F12, 0x02AC},	//senHal_pContSenModesRegsArray[61][3] 2 700014C0
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[62][0] 2 700014C2
		{0x0F12, 0x063C},	//senHal_pContSenModesRegsArray[62][1] 2 700014C4
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[62][2] 2 700014C6
		{0x0F12, 0x0399},	//senHal_pContSenModesRegsArray[62][3] 2 700014C8
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[63][0] 2 700014CA
		{0x0F12, 0x0649},	//senHal_pContSenModesRegsArray[63][1] 2 700014CC
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[63][2] 2 700014CE
		{0x0F12, 0x03A6},	//senHal_pContSenModesRegsArray[63][3] 2 700014D0
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[64][0] 2 700014D2
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[64][1] 2 700014D4
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[64][2] 2 700014D6
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[64][3] 2 700014D8
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[65][0] 2 700014DA
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[65][1] 2 700014DC
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[65][2] 2 700014DE
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[65][3] 2 700014E0
		{0x0F12, 0x03AA},	//senHal_pContSenModesRegsArray[66][0] 2 700014E2
		{0x0F12, 0x01DB},	//senHal_pContSenModesRegsArray[66][1] 2 700014E4
		{0x0F12, 0x01E8},	//senHal_pContSenModesRegsArray[66][2] 2 700014E6
		{0x0F12, 0x00FA},	//senHal_pContSenModesRegsArray[66][3] 2 700014E8
		{0x0F12, 0x03B7},	//senHal_pContSenModesRegsArray[67][0] 2 700014EA
		{0x0F12, 0x01E8},	//senHal_pContSenModesRegsArray[67][1] 2 700014EC
		{0x0F12, 0x01F5},	//senHal_pContSenModesRegsArray[67][2] 2 700014EE
		{0x0F12, 0x0107},	//senHal_pContSenModesRegsArray[67][3] 2 700014F0
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[68][0] 2 700014F2
		{0x0F12, 0x0469},	//senHal_pContSenModesRegsArray[68][1] 2 700014F4
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[68][2] 2 700014F6
		{0x0F12, 0x02A7},	//senHal_pContSenModesRegsArray[68][3] 2 700014F8
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[69][0] 2 700014FA
		{0x0F12, 0x0476},	//senHal_pContSenModesRegsArray[69][1] 2 700014FC
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[69][2] 2 700014FE
		{0x0F12, 0x02B4},	//senHal_pContSenModesRegsArray[69][3] 2 70001500
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[70][0] 2 70001502
		{0x0F12, 0x0644},	//senHal_pContSenModesRegsArray[70][1] 2 70001504
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[70][2] 2 70001506
		{0x0F12, 0x03A1},	//senHal_pContSenModesRegsArray[70][3] 2 70001508
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[71][0] 2 7000150A
		{0x0F12, 0x0651},	//senHal_pContSenModesRegsArray[71][1] 2 7000150C
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[71][2] 2 7000150E
		{0x0F12, 0x03AE},	//senHal_pContSenModesRegsArray[71][3] 2 70001510
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[72][0] 2 70001512
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[72][1] 2 70001514
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[72][2] 2 70001516
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[72][3] 2 70001518
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[73][0] 2 7000151A
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[73][1] 2 7000151C
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[73][2] 2 7000151E
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[73][3] 2 70001520
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[74][0] 2 70001522
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[74][1] 2 70001524
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[74][2] 2 70001526
		{0x0F12, 0x0001},	//senHal_pContSenModesRegsArray[74][3] 2 70001528
		{0x0F12, 0x000F},	//senHal_pContSenModesRegsArray[75][0] 2 7000152A
		{0x0F12, 0x000F},	//senHal_pContSenModesRegsArray[75][1] 2 7000152C
		{0x0F12, 0x000F},	//senHal_pContSenModesRegsArray[75][2] 2 7000152E
		{0x0F12, 0x000F},	//senHal_pContSenModesRegsArray[75][3] 2 70001530
		{0x0F12, 0x05AD},	//senHal_pContSenModesRegsArray[76][0] 2 70001532
		{0x0F12, 0x03DE},	//senHal_pContSenModesRegsArray[76][1] 2 70001534
		{0x0F12, 0x030A},	//senHal_pContSenModesRegsArray[76][2] 2 70001536
		{0x0F12, 0x021C},	//senHal_pContSenModesRegsArray[76][3] 2 70001538
		{0x0F12, 0x062F},	//senHal_pContSenModesRegsArray[77][0] 2 7000153A
		{0x0F12, 0x0460},	//senHal_pContSenModesRegsArray[77][1] 2 7000153C
		{0x0F12, 0x038C},	//senHal_pContSenModesRegsArray[77][2] 2 7000153E
		{0x0F12, 0x029E},	//senHal_pContSenModesRegsArray[77][3] 2 70001540
		{0x0F12, 0x07FC},	//senHal_pContSenModesRegsArray[78][0] 2 70001542
		{0x0F12, 0x0847},	//senHal_pContSenModesRegsArray[78][1] 2 70001544
		{0x0F12, 0x0478},	//senHal_pContSenModesRegsArray[78][2] 2 70001546
		{0x0F12, 0x04C3},	//senHal_pContSenModesRegsArray[78][3] 2 70001548
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[79][0] 2 7000154A
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[79][1] 2 7000154C
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[79][2] 2 7000154E
		{0x0F12, 0x0000},	//senHal_pContSenModesRegsArray[79][3] 2 70001550
		
		
		
		{0x002A, 0x0252},
		{0x0F12, 0x0003}, //init 
		
		{0x002A, 0x12B8},	//disable CINTR 0								  
		{0x0F12, 0x1000},		 
		
		//============================================================
		// ISP-FE Setting
		//============================================================					  
		{0x002A, 0x158A},	
		{0x0F12, 0xEAF0},	
		{0x002A, 0x15C6},	
		{0x0F12, 0x0020},	
		{0x0F12, 0x0060},	
		{0x002A, 0x15BC},	
		{0x0F12, 0x0200},	
							
		{0x002A, 0x1608},	
		{0x0F12, 0x0100},	
		{0x0F12, 0x0100},	
		{0x0F12, 0x0100},	
		{0x0F12, 0x0100},	
							
		{0x002A, 0x0F70},	
		{0x0F12, 0x0040},	 //36 //TVAR_ae_BrAve  //ae Target//
		{0x002A, 0x0530},															
		{0x0F12, 0x3415},	//3A98 //3A98////lt_uMaxExp1	32 30ms  9~10ea// 15fps  // 
		{0x002A, 0x0534},																
		{0x0F12, 0x682A},	//68b0 //7EF4////lt_uMaxExp2	67 65ms 18~20ea // 7.5fps //
		{0x002A, 0x167C},																
		{0x0F12, 0x8235},	//8340 //9C40//MaxExp3	83 80ms  24~25ea // 				
		{0x002A, 0x1680},																
		{0x0F12, 0xc350},	 //F424//MaxExp4   125ms  38ea //							
				
		{0x002A, 0x0538},																
		{0x0F12, 0x3415},	// 15fps // 												
		{0x002A, 0x053C},																
		{0x0F12, 0x682A},	// 7.5fps //												
		{0x002A, 0x1684},																
		{0x0F12, 0x8235},	//CapMaxExp3 // 											
		{0x002A, 0x1688},																
		{0x0F12, 0xC350},	//CapMaxExp4 // 											
		
		//Shutter tunpoint//		//gain * 256 = value//
		{0x002A, 0x0540},																
		{0x0F12, 0x01B3},	 //0170//0150//lt_uMaxAnGain1_700lux//												
		{0x0F12, 0x01B3},	//0200//0400//lt_uMaxAnGain2_400lux//							   
		{0x002A, 0x168C},																
		{0x0F12, 0x02A0},	//0300//MaxAnGain3_200lux// 									  
		{0x0F12, 0x0710},	//MaxAnGain4 // 											
		//Shutter tunend//
				
		{0x002A, 0x0544},																
		{0x0F12, 0x0100},															
		{0x0F12, 0x8000},	//Max Gain 8 // 											
				
				
		{0x002A, 0x1694},																
		{0x0F12, 0x0001},	//expand forbidde zone //									
		
		{0x002A, 0x021A},																
		{0x0F12, 0x0000},  //MBR off//														
		
		
		//==============================================//
		//AFC										   //
		//==============================================//
		{0x002A, 0x04d2},
		{0x0F12, 0x065f},	  //065f : Manual AFC on   067f : Manual AFC off //
		{0x002A, 0x04ba},	  
		{0x0F12, 0x0001},	  // 0002: 60hz  0001 : 50hz //
		{0x0F12, 0x0001},	  // afc update command //
								 
							
							
		{0x002A, 0x06CE},	
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[0] //	   
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[1] // 
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[2] // 
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[3] // 
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[4] // 
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[5] // 
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[6] // 
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[7] // 
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[8] // 
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[9] // 
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[10] //
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[11] //
		{0x0F12, 0x00e0},	//TVAR_ash_GAsalpha[12] //
		{0x0F12, 0x00f8},	//TVAR_ash_GAsalpha[13] //
		{0x0F12, 0x00f8},	//TVAR_ash_GAsalpha[14] //
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[15] //
		{0x0F12, 0x00e8},	//TVAR_ash_GAsalpha[16] //
		{0x0F12, 0x00f8},	//TVAR_ash_GAsalpha[17] //
		{0x0F12, 0x00f8},	//TVAR_ash_GAsalpha[18] //
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[19] //
		{0x0F12, 0x00f0},	//TVAR_ash_GAsalpha[20] //
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[21] //
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[22] //
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[23] //
		{0x0F12, 0x00f0},	//TVAR_ash_GAsalpha[24] //
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[25] //
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[26] //
		{0x0F12, 0x0100},	//TVAR_ash_GAsalpha[27] //
							
		{0x0F12, 0x00f8},	//TVAR_ash_GAS OutdoorAlpha[0] //	
		{0x0F12, 0x0100},	//TVAR_ash_GAS OutdoorAlpha[1] //
		{0x0F12, 0x0100},	//TVAR_ash_GAS OutdoorAlpha[2] //
		{0x0F12, 0x0100},	//TVAR_ash_GAS OutdoorAlpha[3] //
							
							
		{0x0F12, 0x0036},	//ash_GASBeta[0] // 	   
		{0x0F12, 0x001F},	//ash_GASBeta[1] //    
		{0x0F12, 0x0020},	//ash_GASBeta[2] // 
		{0x0F12, 0x0000},	//ash_GASBeta[3] // 
		{0x0F12, 0x0036},	//ash_GASBeta[4] // 
		{0x0F12, 0x001F},	//ash_GASBeta[5] // 
		{0x0F12, 0x0020},	//ash_GASBeta[6] // 
		{0x0F12, 0x0000},	//ash_GASBeta[7] // 
		{0x0F12, 0x0036},	//ash_GASBeta[8] // 
		{0x0F12, 0x001F},	//ash_GASBeta[9] // 
		{0x0F12, 0x0020},	//ash_GASBeta[10] //
		{0x0F12, 0x0000},	//ash_GASBeta[11] //
		{0x0F12, 0x0010},	//ash_GASBeta[12] //
		{0x0F12, 0x001F},	//ash_GASBeta[13] //
		{0x0F12, 0x0020},	//ash_GASBeta[14] //
		{0x0F12, 0x0000},	//ash_GASBeta[15] //
		{0x0F12, 0x0020},	//ash_GASBeta[16] //
		{0x0F12, 0x001F},	//ash_GASBeta[17] //
		{0x0F12, 0x0020},	//ash_GASBeta[18] //
		{0x0F12, 0x0000},	//ash_GASBeta[19] //
		{0x0F12, 0x0036},	//ash_GASBeta[20] //
		{0x0F12, 0x001F},	//ash_GASBeta[21] //
		{0x0F12, 0x0020},	//ash_GASBeta[22] //
		{0x0F12, 0x0000},	//ash_GASBeta[23] //
		{0x0F12, 0x0036},	//ash_GASBeta[24] //
		{0x0F12, 0x001F},	//ash_GASBeta[25] //
		{0x0F12, 0x0020},	//ash_GASBeta[26] //
		{0x0F12, 0x0000},	//ash_GASBeta[27] //
							
		{0x0F12, 0x0036},	//ash_GAS OutdoorBeta[0] //   
		{0x0F12, 0x001F},	//ash_GAS OutdoorBeta[1] //
		{0x0F12, 0x0020},	//ash_GAS OutdoorBeta[2] //
		{0x0F12, 0x0000},	//ash_GAS OutdoorBeta[3] //
							
		{0x002A, 0x075A},	//ash_bParabolicEstimation//   
		{0x0F12, 0x0000},	//ash_uParabolicCenterX   //
		{0x0F12, 0x0400},	//ash_uParabolicCenterY   //
		{0x0F12, 0x0300},	//ash_uParabolicscalingA  //
		{0x0F12, 0x0010},	//ash_uParabolicscalingB  //
		{0x0F12, 0x0011},	
							
		{0x002A, 0x347C},	
		{0x0F12, 0x020B},	//TVAR_ash_pGAS[0] //	  
		{0x0F12, 0x019E},	//TVAR_ash_pGAS[1] //  
		{0x0F12, 0x0155},	//TVAR_ash_pGAS[2] //  
		{0x0F12, 0x0121},	//TVAR_ash_pGAS[3] //  
		{0x0F12, 0x00FA},	//TVAR_ash_pGAS[4] //  
		{0x0F12, 0x00E2},	//TVAR_ash_pGAS[5] //  
		{0x0F12, 0x00D7},	//TVAR_ash_pGAS[6] //  
		{0x0F12, 0x00DC},	//TVAR_ash_pGAS[7] //  
		{0x0F12, 0x00F0},	//TVAR_ash_pGAS[8] //  
		{0x0F12, 0x0114},	//TVAR_ash_pGAS[9] //  
		{0x0F12, 0x0148},	//TVAR_ash_pGAS[10] // 
		{0x0F12, 0x0196},	//TVAR_ash_pGAS[11] // 
		{0x0F12, 0x01F5},	//TVAR_ash_pGAS[12] // 
		{0x0F12, 0x01B0},	//TVAR_ash_pGAS[13] // 
		{0x0F12, 0x0160},	//TVAR_ash_pGAS[14] // 
		{0x0F12, 0x0118},	//TVAR_ash_pGAS[15] // 
		{0x0F12, 0x00E0},	//TVAR_ash_pGAS[16] // 
		{0x0F12, 0x00B6},	//TVAR_ash_pGAS[17] // 
		{0x0F12, 0x009B},	//TVAR_ash_pGAS[18] // 
		{0x0F12, 0x008F},	//TVAR_ash_pGAS[19] // 
		{0x0F12, 0x0094},	//TVAR_ash_pGAS[20] // 
		{0x0F12, 0x00AD},	//TVAR_ash_pGAS[21] // 
		{0x0F12, 0x00D8},	//TVAR_ash_pGAS[22] // 
		{0x0F12, 0x0110},	//TVAR_ash_pGAS[23] // 
		{0x0F12, 0x0156},	//TVAR_ash_pGAS[24] // 
		{0x0F12, 0x01AC},	//TVAR_ash_pGAS[25] // 
		{0x0F12, 0x0172},	//TVAR_ash_pGAS[26] // 
		{0x0F12, 0x0124},	//TVAR_ash_pGAS[27] // 
		{0x0F12, 0x00DB},	//TVAR_ash_pGAS[28] // 
		{0x0F12, 0x009F},	//TVAR_ash_pGAS[29] // 
		{0x0F12, 0x0073},	//TVAR_ash_pGAS[30] // 
		{0x0F12, 0x0057},	//TVAR_ash_pGAS[31] // 
		{0x0F12, 0x004C},	//TVAR_ash_pGAS[32] // 
		{0x0F12, 0x0054},	//TVAR_ash_pGAS[33] // 
		{0x0F12, 0x006C},	//TVAR_ash_pGAS[34] // 
		{0x0F12, 0x0097},	//TVAR_ash_pGAS[35] // 
		{0x0F12, 0x00D5},	//TVAR_ash_pGAS[36] // 
		{0x0F12, 0x0120},	//TVAR_ash_pGAS[37] // 
		{0x0F12, 0x0170},	//TVAR_ash_pGAS[38] // 
		{0x0F12, 0x0147},	//TVAR_ash_pGAS[39] // 
		{0x0F12, 0x00FC},	//TVAR_ash_pGAS[40] // 
		{0x0F12, 0x00AC},	//TVAR_ash_pGAS[41] // 
		{0x0F12, 0x006F},	//TVAR_ash_pGAS[42] // 
		{0x0F12, 0x0044},	//TVAR_ash_pGAS[43] // 
		{0x0F12, 0x002A},	//TVAR_ash_pGAS[44] // 
		{0x0F12, 0x0020},	//TVAR_ash_pGAS[45] // 
		{0x0F12, 0x0027},	//TVAR_ash_pGAS[46] // 
		{0x0F12, 0x0040},	//TVAR_ash_pGAS[47] // 
		{0x0F12, 0x006C},	//TVAR_ash_pGAS[48] // 
		{0x0F12, 0x00AB},	//TVAR_ash_pGAS[49] // 
		{0x0F12, 0x00FB},	//TVAR_ash_pGAS[50] // 
		{0x0F12, 0x014B},	//TVAR_ash_pGAS[51] // 
		{0x0F12, 0x0131},	//TVAR_ash_pGAS[52] // 
		{0x0F12, 0x00E4},	//TVAR_ash_pGAS[53] // 
		{0x0F12, 0x0090},	//TVAR_ash_pGAS[54] // 
		{0x0F12, 0x0052},	//TVAR_ash_pGAS[55] // 
		{0x0F12, 0x0027},	//TVAR_ash_pGAS[56] // 
		{0x0F12, 0x0010},	//TVAR_ash_pGAS[57] // 
		{0x0F12, 0x0008},	//TVAR_ash_pGAS[58] // 
		{0x0F12, 0x000F},	//TVAR_ash_pGAS[59] // 
		{0x0F12, 0x0027},	//TVAR_ash_pGAS[60] // 
		{0x0F12, 0x0053},	//TVAR_ash_pGAS[61] // 
		{0x0F12, 0x0093},	//TVAR_ash_pGAS[62] // 
		{0x0F12, 0x00EA},	//TVAR_ash_pGAS[63] // 
		{0x0F12, 0x0138},	//TVAR_ash_pGAS[64] // 
		{0x0F12, 0x0129},	//TVAR_ash_pGAS[65] // 
		{0x0F12, 0x00DA},	//TVAR_ash_pGAS[66] // 
		{0x0F12, 0x0085},	//TVAR_ash_pGAS[67] // 
		{0x0F12, 0x0046},	//TVAR_ash_pGAS[68] // 
		{0x0F12, 0x001D},	//TVAR_ash_pGAS[69] // 
		{0x0F12, 0x0007},	//TVAR_ash_pGAS[70] // 
		{0x0F12, 0x0000},	//TVAR_ash_pGAS[71] // 
		{0x0F12, 0x0007},	//TVAR_ash_pGAS[72] // 
		{0x0F12, 0x001F},	//TVAR_ash_pGAS[73] // 
		{0x0F12, 0x004B},	//TVAR_ash_pGAS[74] // 
		{0x0F12, 0x008E},	//TVAR_ash_pGAS[75] // 
		{0x0F12, 0x00E4},	//TVAR_ash_pGAS[76] // 
		{0x0F12, 0x0138},	//TVAR_ash_pGAS[77] // 
		{0x0F12, 0x0130},	//TVAR_ash_pGAS[78] // 
		{0x0F12, 0x00E2},	//TVAR_ash_pGAS[79] // 
		{0x0F12, 0x008D},	//TVAR_ash_pGAS[80] // 
		{0x0F12, 0x004E},	//TVAR_ash_pGAS[81] // 
		{0x0F12, 0x0025},	//TVAR_ash_pGAS[82] // 
		{0x0F12, 0x000E},	//TVAR_ash_pGAS[83] // 
		{0x0F12, 0x0007},	//TVAR_ash_pGAS[84] // 
		{0x0F12, 0x000F},	//TVAR_ash_pGAS[85] // 
		{0x0F12, 0x0027},	//TVAR_ash_pGAS[86] // 
		{0x0F12, 0x0055},	//TVAR_ash_pGAS[87] // 
		{0x0F12, 0x009A},	//TVAR_ash_pGAS[88] // 
		{0x0F12, 0x00F2},	//TVAR_ash_pGAS[89] // 
		{0x0F12, 0x0142},	//TVAR_ash_pGAS[90] // 
		{0x0F12, 0x0145},	//TVAR_ash_pGAS[91] // 
		{0x0F12, 0x00F9},	//TVAR_ash_pGAS[92] // 
		{0x0F12, 0x00A6},	//TVAR_ash_pGAS[93] // 
		{0x0F12, 0x0067},	//TVAR_ash_pGAS[94] // 
		{0x0F12, 0x003C},	//TVAR_ash_pGAS[95] // 
		{0x0F12, 0x0024},	//TVAR_ash_pGAS[96] // 
		{0x0F12, 0x001D},	//TVAR_ash_pGAS[97] // 
		{0x0F12, 0x0025},	//TVAR_ash_pGAS[98] // 
		{0x0F12, 0x0041},	//TVAR_ash_pGAS[99] // 
		{0x0F12, 0x0071},	//TVAR_ash_pGAS[100] //
		{0x0F12, 0x00B5},	//TVAR_ash_pGAS[101] //
		{0x0F12, 0x010B},	//TVAR_ash_pGAS[102] //
		{0x0F12, 0x015A},	//TVAR_ash_pGAS[103] //
		{0x0F12, 0x0169},	//TVAR_ash_pGAS[104] //
		{0x0F12, 0x011F},	//TVAR_ash_pGAS[105] //
		{0x0F12, 0x00CF},	//TVAR_ash_pGAS[106] //
		{0x0F12, 0x0092},	//TVAR_ash_pGAS[107] //
		{0x0F12, 0x0066},	//TVAR_ash_pGAS[108] //
		{0x0F12, 0x004D},	//TVAR_ash_pGAS[109] //
		{0x0F12, 0x0044},	//TVAR_ash_pGAS[110] //
		{0x0F12, 0x004F},	//TVAR_ash_pGAS[111] //
		{0x0F12, 0x006B},	//TVAR_ash_pGAS[112] //
		{0x0F12, 0x009E},	//TVAR_ash_pGAS[113] //
		{0x0F12, 0x00E2},	//TVAR_ash_pGAS[114] //
		{0x0F12, 0x0136},	//TVAR_ash_pGAS[115] //
		{0x0F12, 0x0183},	//TVAR_ash_pGAS[116] //
		{0x0F12, 0x01A9},	//TVAR_ash_pGAS[117] //
		{0x0F12, 0x0157},	//TVAR_ash_pGAS[118] //
		{0x0F12, 0x010E},	//TVAR_ash_pGAS[119] //
		{0x0F12, 0x00D2},	//TVAR_ash_pGAS[120] //
		{0x0F12, 0x00A7},	//TVAR_ash_pGAS[121] //
		{0x0F12, 0x008C},	//TVAR_ash_pGAS[122] //
		{0x0F12, 0x0086},	//TVAR_ash_pGAS[123] //
		{0x0F12, 0x0090},	//TVAR_ash_pGAS[124] //
		{0x0F12, 0x00AE},	//TVAR_ash_pGAS[125] //
		{0x0F12, 0x00E0},	//TVAR_ash_pGAS[126] //
		{0x0F12, 0x0121},	//TVAR_ash_pGAS[127] //
		{0x0F12, 0x0170},	//TVAR_ash_pGAS[128] //
		{0x0F12, 0x01C5},	//TVAR_ash_pGAS[129] //
		{0x0F12, 0x01F7},	//TVAR_ash_pGAS[130] //
		{0x0F12, 0x0193},	//TVAR_ash_pGAS[131] //
		{0x0F12, 0x014B},	//TVAR_ash_pGAS[132] //
		{0x0F12, 0x0114},	//TVAR_ash_pGAS[133] //
		{0x0F12, 0x00F0},	//TVAR_ash_pGAS[134] //
		{0x0F12, 0x00D7},	//TVAR_ash_pGAS[135] //
		{0x0F12, 0x00D0},	//TVAR_ash_pGAS[136] //
		{0x0F12, 0x00D7},	//TVAR_ash_pGAS[137] //
		{0x0F12, 0x00F4},	//TVAR_ash_pGAS[138] //
		{0x0F12, 0x0125},	//TVAR_ash_pGAS[139] //
		{0x0F12, 0x0160},	//TVAR_ash_pGAS[140] //
		{0x0F12, 0x01B2},	//TVAR_ash_pGAS[141] //
		{0x0F12, 0x021A},	//TVAR_ash_pGAS[142] //
		{0x0F12, 0x01DF},	//TVAR_ash_pGAS[143] //
		{0x0F12, 0x0172},	//TVAR_ash_pGAS[144] //
		{0x0F12, 0x012C},	//TVAR_ash_pGAS[145] //
		{0x0F12, 0x00FB},	//TVAR_ash_pGAS[146] //
		{0x0F12, 0x00D5},	//TVAR_ash_pGAS[147] //
		{0x0F12, 0x00BB},	//TVAR_ash_pGAS[148] //
		{0x0F12, 0x00AF},	//TVAR_ash_pGAS[149] //
		{0x0F12, 0x00B3},	//TVAR_ash_pGAS[150] //
		{0x0F12, 0x00C7},	//TVAR_ash_pGAS[151] //
		{0x0F12, 0x00E7},	//TVAR_ash_pGAS[152] //
		{0x0F12, 0x0110},	//TVAR_ash_pGAS[153] //
		{0x0F12, 0x0155},	//TVAR_ash_pGAS[154] //
		{0x0F12, 0x01B2},	//TVAR_ash_pGAS[155] //
		{0x0F12, 0x018C},	//TVAR_ash_pGAS[156] //
		{0x0F12, 0x0135},	//TVAR_ash_pGAS[157] //
		{0x0F12, 0x00F5},	//TVAR_ash_pGAS[158] //
		{0x0F12, 0x00C0},	//TVAR_ash_pGAS[159] //
		{0x0F12, 0x0097},	//TVAR_ash_pGAS[160] //
		{0x0F12, 0x007C},	//TVAR_ash_pGAS[161] //
		{0x0F12, 0x0072},	//TVAR_ash_pGAS[162] //
		{0x0F12, 0x0078},	//TVAR_ash_pGAS[163] //
		{0x0F12, 0x008D},	//TVAR_ash_pGAS[164] //
		{0x0F12, 0x00B2},	//TVAR_ash_pGAS[165] //
		{0x0F12, 0x00E0},	//TVAR_ash_pGAS[166] //
		{0x0F12, 0x011E},	//TVAR_ash_pGAS[167] //
		{0x0F12, 0x0170},	//TVAR_ash_pGAS[168] //
		{0x0F12, 0x0159},	//TVAR_ash_pGAS[169] //
		{0x0F12, 0x0106},	//TVAR_ash_pGAS[170] //
		{0x0F12, 0x00C2},	//TVAR_ash_pGAS[171] //
		{0x0F12, 0x008B},	//TVAR_ash_pGAS[172] //
		{0x0F12, 0x0061},	//TVAR_ash_pGAS[173] //
		{0x0F12, 0x0046},	//TVAR_ash_pGAS[174] //
		{0x0F12, 0x003C},	//TVAR_ash_pGAS[175] //
		{0x0F12, 0x0043},	//TVAR_ash_pGAS[176] //
		{0x0F12, 0x0058},	//TVAR_ash_pGAS[177] //
		{0x0F12, 0x007E},	//TVAR_ash_pGAS[178] //
		{0x0F12, 0x00B3},	//TVAR_ash_pGAS[179] //
		{0x0F12, 0x00F4},	//TVAR_ash_pGAS[180] //
		{0x0F12, 0x0141},	//TVAR_ash_pGAS[181] //
		{0x0F12, 0x0132},	//TVAR_ash_pGAS[182] //
		{0x0F12, 0x00E5},	//TVAR_ash_pGAS[183] //
		{0x0F12, 0x009C},	//TVAR_ash_pGAS[184] //
		{0x0F12, 0x0062},	//TVAR_ash_pGAS[185] //
		{0x0F12, 0x0039},	//TVAR_ash_pGAS[186] //
		{0x0F12, 0x0021},	//TVAR_ash_pGAS[187] //
		{0x0F12, 0x0018},	//TVAR_ash_pGAS[188] //
		{0x0F12, 0x0020},	//TVAR_ash_pGAS[189] //
		{0x0F12, 0x0034},	//TVAR_ash_pGAS[190] //
		{0x0F12, 0x005B},	//TVAR_ash_pGAS[191] //
		{0x0F12, 0x0092},	//TVAR_ash_pGAS[192] //
		{0x0F12, 0x00D6},	//TVAR_ash_pGAS[193] //
		{0x0F12, 0x0121},	//TVAR_ash_pGAS[194] //
		{0x0F12, 0x011D},	//TVAR_ash_pGAS[195] //
		{0x0F12, 0x00CC},	//TVAR_ash_pGAS[196] //
		{0x0F12, 0x0083},	//TVAR_ash_pGAS[197] //
		{0x0F12, 0x004A},	//TVAR_ash_pGAS[198] //
		{0x0F12, 0x0023},	//TVAR_ash_pGAS[199] //
		{0x0F12, 0x000C},	//TVAR_ash_pGAS[200] //
		{0x0F12, 0x0005},	//TVAR_ash_pGAS[201] //
		{0x0F12, 0x000D},	//TVAR_ash_pGAS[202] //
		{0x0F12, 0x0022},	//TVAR_ash_pGAS[203] //
		{0x0F12, 0x0048},	//TVAR_ash_pGAS[204] //
		{0x0F12, 0x007F},	//TVAR_ash_pGAS[205] //
		{0x0F12, 0x00C9},	//TVAR_ash_pGAS[206] //
		{0x0F12, 0x0115},	//TVAR_ash_pGAS[207] //
		{0x0F12, 0x0115},	//TVAR_ash_pGAS[208] //
		{0x0F12, 0x00C4},	//TVAR_ash_pGAS[209] //
		{0x0F12, 0x007A},	//TVAR_ash_pGAS[210] //
		{0x0F12, 0x0041},	//TVAR_ash_pGAS[211] //
		{0x0F12, 0x001B},	//TVAR_ash_pGAS[212] //
		{0x0F12, 0x0006},	//TVAR_ash_pGAS[213] //
		{0x0F12, 0x0000},	//TVAR_ash_pGAS[214] //
		{0x0F12, 0x0008},	//TVAR_ash_pGAS[215] //
		{0x0F12, 0x001E},	//TVAR_ash_pGAS[216] //
		{0x0F12, 0x0044},	//TVAR_ash_pGAS[217] //
		{0x0F12, 0x007D},	//TVAR_ash_pGAS[218] //
		{0x0F12, 0x00C9},	//TVAR_ash_pGAS[219] //
		{0x0F12, 0x0115},	//TVAR_ash_pGAS[220] //
		{0x0F12, 0x011C},	//TVAR_ash_pGAS[221] //
		{0x0F12, 0x00CB},	//TVAR_ash_pGAS[222] //
		{0x0F12, 0x0081},	//TVAR_ash_pGAS[223] //
		{0x0F12, 0x0048},	//TVAR_ash_pGAS[224] //
		{0x0F12, 0x0022},	//TVAR_ash_pGAS[225] //
		{0x0F12, 0x000D},	//TVAR_ash_pGAS[226] //
		{0x0F12, 0x0007},	//TVAR_ash_pGAS[227] //
		{0x0F12, 0x0010},	//TVAR_ash_pGAS[228] //
		{0x0F12, 0x0027},	//TVAR_ash_pGAS[229] //
		{0x0F12, 0x004E},	//TVAR_ash_pGAS[230] //
		{0x0F12, 0x0088},	//TVAR_ash_pGAS[231] //
		{0x0F12, 0x00D5},	//TVAR_ash_pGAS[232] //
		{0x0F12, 0x011C},	//TVAR_ash_pGAS[233] //
		{0x0F12, 0x012D},	//TVAR_ash_pGAS[234] //
		{0x0F12, 0x00E0},	//TVAR_ash_pGAS[235] //
		{0x0F12, 0x0097},	//TVAR_ash_pGAS[236] //
		{0x0F12, 0x005F},	//TVAR_ash_pGAS[237] //
		{0x0F12, 0x0038},	//TVAR_ash_pGAS[238] //
		{0x0F12, 0x0021},	//TVAR_ash_pGAS[239] //
		{0x0F12, 0x001C},	//TVAR_ash_pGAS[240] //
		{0x0F12, 0x0026},	//TVAR_ash_pGAS[241] //
		{0x0F12, 0x003D},	//TVAR_ash_pGAS[242] //
		{0x0F12, 0x0068},	//TVAR_ash_pGAS[243] //
		{0x0F12, 0x00A3},	//TVAR_ash_pGAS[244] //
		{0x0F12, 0x00EE},	//TVAR_ash_pGAS[245] //
		{0x0F12, 0x0138},	//TVAR_ash_pGAS[246] //
		{0x0F12, 0x0151},	//TVAR_ash_pGAS[247] //
		{0x0F12, 0x0102},	//TVAR_ash_pGAS[248] //
		{0x0F12, 0x00BC},	//TVAR_ash_pGAS[249] //
		{0x0F12, 0x0083},	//TVAR_ash_pGAS[250] //
		{0x0F12, 0x005C},	//TVAR_ash_pGAS[251] //
		{0x0F12, 0x0046},	//TVAR_ash_pGAS[252] //
		{0x0F12, 0x0041},	//TVAR_ash_pGAS[253] //
		{0x0F12, 0x004B},	//TVAR_ash_pGAS[254] //
		{0x0F12, 0x0066},	//TVAR_ash_pGAS[255] //
		{0x0F12, 0x0092},	//TVAR_ash_pGAS[256] //
		{0x0F12, 0x00CD},	//TVAR_ash_pGAS[257] //
		{0x0F12, 0x0115},	//TVAR_ash_pGAS[258] //
		{0x0F12, 0x015F},	//TVAR_ash_pGAS[259] //
		{0x0F12, 0x018A},	//TVAR_ash_pGAS[260] //
		{0x0F12, 0x0133},	//TVAR_ash_pGAS[261] //
		{0x0F12, 0x00F0},	//TVAR_ash_pGAS[262] //
		{0x0F12, 0x00BB},	//TVAR_ash_pGAS[263] //
		{0x0F12, 0x0097},	//TVAR_ash_pGAS[264] //
		{0x0F12, 0x007F},	//TVAR_ash_pGAS[265] //
		{0x0F12, 0x0079},	//TVAR_ash_pGAS[266] //
		{0x0F12, 0x0084},	//TVAR_ash_pGAS[267] //
		{0x0F12, 0x00A1},	//TVAR_ash_pGAS[268] //
		{0x0F12, 0x00CD},	//TVAR_ash_pGAS[269] //
		{0x0F12, 0x0105},	//TVAR_ash_pGAS[270] //
		{0x0F12, 0x014C},	//TVAR_ash_pGAS[271] //
		{0x0F12, 0x01A0},	//TVAR_ash_pGAS[272] //
		{0x0F12, 0x01D5},	//TVAR_ash_pGAS[273] //
		{0x0F12, 0x0171},	//TVAR_ash_pGAS[274] //
		{0x0F12, 0x012D},	//TVAR_ash_pGAS[275] //
		{0x0F12, 0x00FE},	//TVAR_ash_pGAS[276] //
		{0x0F12, 0x00D9},	//TVAR_ash_pGAS[277] //
		{0x0F12, 0x00C2},	//TVAR_ash_pGAS[278] //
		{0x0F12, 0x00BD},	//TVAR_ash_pGAS[279] //
		{0x0F12, 0x00C9},	//TVAR_ash_pGAS[280] //
		{0x0F12, 0x00E5},	//TVAR_ash_pGAS[281] //
		{0x0F12, 0x0111},	//TVAR_ash_pGAS[282] //
		{0x0F12, 0x0146},	//TVAR_ash_pGAS[283] //
		{0x0F12, 0x018E},	//TVAR_ash_pGAS[284] //
		{0x0F12, 0x01EE},	//TVAR_ash_pGAS[285] //
		{0x0F12, 0x01CF},	//TVAR_ash_pGAS[286] //
		{0x0F12, 0x0162},	//TVAR_ash_pGAS[287] //
		{0x0F12, 0x011E},	//TVAR_ash_pGAS[288] //
		{0x0F12, 0x00F1},	//TVAR_ash_pGAS[289] //
		{0x0F12, 0x00CF},	//TVAR_ash_pGAS[290] //
		{0x0F12, 0x00B9},	//TVAR_ash_pGAS[291] //
		{0x0F12, 0x00B0},	//TVAR_ash_pGAS[292] //
		{0x0F12, 0x00BB},	//TVAR_ash_pGAS[293] //
		{0x0F12, 0x00D6},	//TVAR_ash_pGAS[294] //
		{0x0F12, 0x00F9},	//TVAR_ash_pGAS[295] //
		{0x0F12, 0x0128},	//TVAR_ash_pGAS[296] //
		{0x0F12, 0x016F},	//TVAR_ash_pGAS[297] //
		{0x0F12, 0x01D3},	//TVAR_ash_pGAS[298] //
		{0x0F12, 0x017B},	//TVAR_ash_pGAS[299] //
		{0x0F12, 0x0127},	//TVAR_ash_pGAS[300] //
		{0x0F12, 0x00E9},	//TVAR_ash_pGAS[301] //
		{0x0F12, 0x00B7},	//TVAR_ash_pGAS[302] //
		{0x0F12, 0x0091},	//TVAR_ash_pGAS[303] //
		{0x0F12, 0x007B},	//TVAR_ash_pGAS[304] //
		{0x0F12, 0x0074},	//TVAR_ash_pGAS[305] //
		{0x0F12, 0x007F},	//TVAR_ash_pGAS[306] //
		{0x0F12, 0x0099},	//TVAR_ash_pGAS[307] //
		{0x0F12, 0x00C2},	//TVAR_ash_pGAS[308] //
		{0x0F12, 0x00F6},	//TVAR_ash_pGAS[309] //
		{0x0F12, 0x0139},	//TVAR_ash_pGAS[310] //
		{0x0F12, 0x018E},	//TVAR_ash_pGAS[311] //
		{0x0F12, 0x014A},	//TVAR_ash_pGAS[312] //
		{0x0F12, 0x00FA},	//TVAR_ash_pGAS[313] //
		{0x0F12, 0x00B9},	//TVAR_ash_pGAS[314] //
		{0x0F12, 0x0084},	//TVAR_ash_pGAS[315] //
		{0x0F12, 0x005D},	//TVAR_ash_pGAS[316] //
		{0x0F12, 0x0046},	//TVAR_ash_pGAS[317] //
		{0x0F12, 0x003E},	//TVAR_ash_pGAS[318] //
		{0x0F12, 0x0049},	//TVAR_ash_pGAS[319] //
		{0x0F12, 0x0061},	//TVAR_ash_pGAS[320] //
		{0x0F12, 0x008C},	//TVAR_ash_pGAS[321] //
		{0x0F12, 0x00C5},	//TVAR_ash_pGAS[322] //
		{0x0F12, 0x0107},	//TVAR_ash_pGAS[323] //
		{0x0F12, 0x0155},	//TVAR_ash_pGAS[324] //
		{0x0F12, 0x0129},	//TVAR_ash_pGAS[325] //
		{0x0F12, 0x00DA},	//TVAR_ash_pGAS[326] //
		{0x0F12, 0x0096},	//TVAR_ash_pGAS[327] //
		{0x0F12, 0x005F},	//TVAR_ash_pGAS[328] //
		{0x0F12, 0x0037},	//TVAR_ash_pGAS[329] //
		{0x0F12, 0x0021},	//TVAR_ash_pGAS[330] //
		{0x0F12, 0x001A},	//TVAR_ash_pGAS[331] //
		{0x0F12, 0x0023},	//TVAR_ash_pGAS[332] //
		{0x0F12, 0x003B},	//TVAR_ash_pGAS[333] //
		{0x0F12, 0x0065},	//TVAR_ash_pGAS[334] //
		{0x0F12, 0x009F},	//TVAR_ash_pGAS[335] //
		{0x0F12, 0x00E5},	//TVAR_ash_pGAS[336] //
		{0x0F12, 0x012F},	//TVAR_ash_pGAS[337] //
		{0x0F12, 0x0116},	//TVAR_ash_pGAS[338] //
		{0x0F12, 0x00C7},	//TVAR_ash_pGAS[339] //
		{0x0F12, 0x0080},	//TVAR_ash_pGAS[340] //
		{0x0F12, 0x0049},	//TVAR_ash_pGAS[341] //
		{0x0F12, 0x0022},	//TVAR_ash_pGAS[342] //
		{0x0F12, 0x000C},	//TVAR_ash_pGAS[343] //
		{0x0F12, 0x0006},	//TVAR_ash_pGAS[344] //
		{0x0F12, 0x000E},	//TVAR_ash_pGAS[345] //
		{0x0F12, 0x0026},	//TVAR_ash_pGAS[346] //
		{0x0F12, 0x004E},	//TVAR_ash_pGAS[347] //
		{0x0F12, 0x0086},	//TVAR_ash_pGAS[348] //
		{0x0F12, 0x00CF},	//TVAR_ash_pGAS[349] //
		{0x0F12, 0x011C},	//TVAR_ash_pGAS[350] //
		{0x0F12, 0x0113},	//TVAR_ash_pGAS[351] //
		{0x0F12, 0x00C4},	//TVAR_ash_pGAS[352] //
		{0x0F12, 0x007A},	//TVAR_ash_pGAS[353] //
		{0x0F12, 0x0042},	//TVAR_ash_pGAS[354] //
		{0x0F12, 0x001D},	//TVAR_ash_pGAS[355] //
		{0x0F12, 0x0007},	//TVAR_ash_pGAS[356] //
		{0x0F12, 0x0000},	//TVAR_ash_pGAS[357] //
		{0x0F12, 0x0008},	//TVAR_ash_pGAS[358] //
		{0x0F12, 0x001F},	//TVAR_ash_pGAS[359] //
		{0x0F12, 0x0045},	//TVAR_ash_pGAS[360] //
		{0x0F12, 0x007F},	//TVAR_ash_pGAS[361] //
		{0x0F12, 0x00C9},	//TVAR_ash_pGAS[362] //
		{0x0F12, 0x0112},	//TVAR_ash_pGAS[363] //
		{0x0F12, 0x011C},	//TVAR_ash_pGAS[364] //
		{0x0F12, 0x00CF},	//TVAR_ash_pGAS[365] //
		{0x0F12, 0x0086},	//TVAR_ash_pGAS[366] //
		{0x0F12, 0x004C},	//TVAR_ash_pGAS[367] //
		{0x0F12, 0x0025},	//TVAR_ash_pGAS[368] //
		{0x0F12, 0x000F},	//TVAR_ash_pGAS[369] //
		{0x0F12, 0x0007},	//TVAR_ash_pGAS[370] //
		{0x0F12, 0x000E},	//TVAR_ash_pGAS[371] //
		{0x0F12, 0x0025},	//TVAR_ash_pGAS[372] //
		{0x0F12, 0x004B},	//TVAR_ash_pGAS[373] //
		{0x0F12, 0x0084},	//TVAR_ash_pGAS[374] //
		{0x0F12, 0x00CD},	//TVAR_ash_pGAS[375] //
		{0x0F12, 0x0115},	//TVAR_ash_pGAS[376] //
		{0x0F12, 0x0134},	//TVAR_ash_pGAS[377] //
		{0x0F12, 0x00E7},	//TVAR_ash_pGAS[378] //
		{0x0F12, 0x009E},	//TVAR_ash_pGAS[379] //
		{0x0F12, 0x0065},	//TVAR_ash_pGAS[380] //
		{0x0F12, 0x003C},	//TVAR_ash_pGAS[381] //
		{0x0F12, 0x0024},	//TVAR_ash_pGAS[382] //
		{0x0F12, 0x001B},	//TVAR_ash_pGAS[383] //
		{0x0F12, 0x0022},	//TVAR_ash_pGAS[384] //
		{0x0F12, 0x0039},	//TVAR_ash_pGAS[385] //
		{0x0F12, 0x0062},	//TVAR_ash_pGAS[386] //
		{0x0F12, 0x0099},	//TVAR_ash_pGAS[387] //
		{0x0F12, 0x00DF},	//TVAR_ash_pGAS[388] //
		{0x0F12, 0x0126},	//TVAR_ash_pGAS[389] //
		{0x0F12, 0x0157},	//TVAR_ash_pGAS[390] //
		{0x0F12, 0x010C},	//TVAR_ash_pGAS[391] //
		{0x0F12, 0x00C6},	//TVAR_ash_pGAS[392] //
		{0x0F12, 0x008D},	//TVAR_ash_pGAS[393] //
		{0x0F12, 0x0063},	//TVAR_ash_pGAS[394] //
		{0x0F12, 0x0049},	//TVAR_ash_pGAS[395] //
		{0x0F12, 0x0041},	//TVAR_ash_pGAS[396] //
		{0x0F12, 0x0047},	//TVAR_ash_pGAS[397] //
		{0x0F12, 0x005F},	//TVAR_ash_pGAS[398] //
		{0x0F12, 0x0087},	//TVAR_ash_pGAS[399] //
		{0x0F12, 0x00BF},	//TVAR_ash_pGAS[400] //
		{0x0F12, 0x0100},	//TVAR_ash_pGAS[401] //
		{0x0F12, 0x0149},	//TVAR_ash_pGAS[402] //
		{0x0F12, 0x0194},	//TVAR_ash_pGAS[403] //
		{0x0F12, 0x0140},	//TVAR_ash_pGAS[404] //
		{0x0F12, 0x00FF},	//TVAR_ash_pGAS[405] //
		{0x0F12, 0x00CA},	//TVAR_ash_pGAS[406] //
		{0x0F12, 0x00A0},	//TVAR_ash_pGAS[407] //
		{0x0F12, 0x0083},	//TVAR_ash_pGAS[408] //
		{0x0F12, 0x007B},	//TVAR_ash_pGAS[409] //
		{0x0F12, 0x007F},	//TVAR_ash_pGAS[410] //
		{0x0F12, 0x0097},	//TVAR_ash_pGAS[411] //
		{0x0F12, 0x00BE},	//TVAR_ash_pGAS[412] //
		{0x0F12, 0x00F2},	//TVAR_ash_pGAS[413] //
		{0x0F12, 0x0131},	//TVAR_ash_pGAS[414] //
		{0x0F12, 0x0181},	//TVAR_ash_pGAS[415] //
		{0x0F12, 0x01E4},	//TVAR_ash_pGAS[416] //
		{0x0F12, 0x017E},	//TVAR_ash_pGAS[417] //
		{0x0F12, 0x013B},	//TVAR_ash_pGAS[418] //
		{0x0F12, 0x010C},	//TVAR_ash_pGAS[419] //
		{0x0F12, 0x00E5},	//TVAR_ash_pGAS[420] //
		{0x0F12, 0x00CA},	//TVAR_ash_pGAS[421] //
		{0x0F12, 0x00BF},	//TVAR_ash_pGAS[422] //
		{0x0F12, 0x00C4},	//TVAR_ash_pGAS[423] //
		{0x0F12, 0x00D8},	//TVAR_ash_pGAS[424] //
		{0x0F12, 0x00FE},	//TVAR_ash_pGAS[425] //
		{0x0F12, 0x012D},	//TVAR_ash_pGAS[426] //
		{0x0F12, 0x016E},	//TVAR_ash_pGAS[427] //
		{0x0F12, 0x01CC},	//TVAR_ash_pGAS[428] //
		{0x0F12, 0x0194},	//TVAR_ash_pGAS[429] //
		{0x0F12, 0x0138},	//TVAR_ash_pGAS[430] //
		{0x0F12, 0x00FA},	//TVAR_ash_pGAS[431] //
		{0x0F12, 0x00D2},	//TVAR_ash_pGAS[432] //
		{0x0F12, 0x00B5},	//TVAR_ash_pGAS[433] //
		{0x0F12, 0x00A4},	//TVAR_ash_pGAS[434] //
		{0x0F12, 0x009E},	//TVAR_ash_pGAS[435] //
		{0x0F12, 0x00A7},	//TVAR_ash_pGAS[436] //
		{0x0F12, 0x00BC},	//TVAR_ash_pGAS[437] //
		{0x0F12, 0x00DC},	//TVAR_ash_pGAS[438] //
		{0x0F12, 0x0106},	//TVAR_ash_pGAS[439] //
		{0x0F12, 0x0147},	//TVAR_ash_pGAS[440] //
		{0x0F12, 0x0199},	//TVAR_ash_pGAS[441] //
		{0x0F12, 0x0145},	//TVAR_ash_pGAS[442] //
		{0x0F12, 0x0101},	//TVAR_ash_pGAS[443] //
		{0x0F12, 0x00CA},	//TVAR_ash_pGAS[444] //
		{0x0F12, 0x00A0},	//TVAR_ash_pGAS[445] //
		{0x0F12, 0x0081},	//TVAR_ash_pGAS[446] //
		{0x0F12, 0x006D},	//TVAR_ash_pGAS[447] //
		{0x0F12, 0x0069},	//TVAR_ash_pGAS[448] //
		{0x0F12, 0x0072},	//TVAR_ash_pGAS[449] //
		{0x0F12, 0x0089},	//TVAR_ash_pGAS[450] //
		{0x0F12, 0x00AD},	//TVAR_ash_pGAS[451] //
		{0x0F12, 0x00DA},	//TVAR_ash_pGAS[452] //
		{0x0F12, 0x0117},	//TVAR_ash_pGAS[453] //
		{0x0F12, 0x0160},	//TVAR_ash_pGAS[454] //
		{0x0F12, 0x0117},	//TVAR_ash_pGAS[455] //
		{0x0F12, 0x00D7},	//TVAR_ash_pGAS[456] //
		{0x0F12, 0x009D},	//TVAR_ash_pGAS[457] //
		{0x0F12, 0x0070},	//TVAR_ash_pGAS[458] //
		{0x0F12, 0x0050},	//TVAR_ash_pGAS[459] //
		{0x0F12, 0x003F},	//TVAR_ash_pGAS[460] //
		{0x0F12, 0x0038},	//TVAR_ash_pGAS[461] //
		{0x0F12, 0x0041},	//TVAR_ash_pGAS[462] //
		{0x0F12, 0x0057},	//TVAR_ash_pGAS[463] //
		{0x0F12, 0x0079},	//TVAR_ash_pGAS[464] //
		{0x0F12, 0x00AD},	//TVAR_ash_pGAS[465] //
		{0x0F12, 0x00EA},	//TVAR_ash_pGAS[466] //
		{0x0F12, 0x0129},	//TVAR_ash_pGAS[467] //
		{0x0F12, 0x00F4},	//TVAR_ash_pGAS[468] //
		{0x0F12, 0x00B7},	//TVAR_ash_pGAS[469] //
		{0x0F12, 0x007C},	//TVAR_ash_pGAS[470] //
		{0x0F12, 0x004E},	//TVAR_ash_pGAS[471] //
		{0x0F12, 0x002F},	//TVAR_ash_pGAS[472] //
		{0x0F12, 0x001D},	//TVAR_ash_pGAS[473] //
		{0x0F12, 0x0018},	//TVAR_ash_pGAS[474] //
		{0x0F12, 0x001F},	//TVAR_ash_pGAS[475] //
		{0x0F12, 0x0033},	//TVAR_ash_pGAS[476] //
		{0x0F12, 0x0056},	//TVAR_ash_pGAS[477] //
		{0x0F12, 0x0088},	//TVAR_ash_pGAS[478] //
		{0x0F12, 0x00C6},	//TVAR_ash_pGAS[479] //
		{0x0F12, 0x0104},	//TVAR_ash_pGAS[480] //
		{0x0F12, 0x00E2},	//TVAR_ash_pGAS[481] //
		{0x0F12, 0x00A5},	//TVAR_ash_pGAS[482] //
		{0x0F12, 0x0066},	//TVAR_ash_pGAS[483] // 																	   
		{0x0F12, 0x0038},	//TVAR_ash_pGAS[484] // 																	   
		{0x0F12, 0x001A},	//TVAR_ash_pGAS[485] // 																	   
		{0x0F12, 0x0009},	//TVAR_ash_pGAS[486] // 																	   
		{0x0F12, 0x0006},	//TVAR_ash_pGAS[487] // 																	   
		{0x0F12, 0x000C},	//TVAR_ash_pGAS[488] // 																	   
		{0x0F12, 0x001E},	//TVAR_ash_pGAS[489] // 																	   
		{0x0F12, 0x003F},	//TVAR_ash_pGAS[490] // 																	   
		{0x0F12, 0x006F},	//TVAR_ash_pGAS[491] // 																	   
		{0x0F12, 0x00B2},	//TVAR_ash_pGAS[492] // 																	   
		{0x0F12, 0x00EE},	//TVAR_ash_pGAS[493] // 																	   
		{0x0F12, 0x00DD},	//TVAR_ash_pGAS[494] // 																	   
		{0x0F12, 0x009E},	//TVAR_ash_pGAS[495] // 																	   
		{0x0F12, 0x0060},	//TVAR_ash_pGAS[496] // 																	   
		{0x0F12, 0x0030},	//TVAR_ash_pGAS[497] // 																	   
		{0x0F12, 0x0013},	//TVAR_ash_pGAS[498] // 																	   
		{0x0F12, 0x0004},	//TVAR_ash_pGAS[499] // 																	   
		{0x0F12, 0x0000},	//TVAR_ash_pGAS[500] // 																	   
		{0x0F12, 0x0005},	//TVAR_ash_pGAS[501] // 																	   
		{0x0F12, 0x0017},	//TVAR_ash_pGAS[502] // 																	   
		{0x0F12, 0x0036},	//TVAR_ash_pGAS[503] // 																	   
		{0x0F12, 0x0066},	//TVAR_ash_pGAS[504] // 																	   
		{0x0F12, 0x00A7},	//TVAR_ash_pGAS[505] // 																	   
		{0x0F12, 0x00E4},	//TVAR_ash_pGAS[506] // 																	   
		{0x0F12, 0x00E5},	//TVAR_ash_pGAS[507] // 																	   
		{0x0F12, 0x00A6},	//TVAR_ash_pGAS[508] // 																	   
		{0x0F12, 0x0067},	//TVAR_ash_pGAS[509] // 																	   
		{0x0F12, 0x0039},	//TVAR_ash_pGAS[510] // 																	   
		{0x0F12, 0x001B},	//TVAR_ash_pGAS[511] // 																	   
		{0x0F12, 0x000B},	//TVAR_ash_pGAS[512] // 																	   
		{0x0F12, 0x0006},	//TVAR_ash_pGAS[513] // 																	   
		{0x0F12, 0x000B},	//TVAR_ash_pGAS[514] // 																	   
		{0x0F12, 0x001C},	//TVAR_ash_pGAS[515] // 																	   
		{0x0F12, 0x003B},	//TVAR_ash_pGAS[516] // 																	   
		{0x0F12, 0x006B},	//TVAR_ash_pGAS[517] // 																	   
		{0x0F12, 0x00AC},	//TVAR_ash_pGAS[518] // 																	   
		{0x0F12, 0x00E6},	//TVAR_ash_pGAS[519] // 																	   
		{0x0F12, 0x00F9},	//TVAR_ash_pGAS[520] // 																	   
		{0x0F12, 0x00BC},	//TVAR_ash_pGAS[521] // 																	   
		{0x0F12, 0x007E},	//TVAR_ash_pGAS[522] // 																	   
		{0x0F12, 0x0051},	//TVAR_ash_pGAS[523] // 																	   
		{0x0F12, 0x0030},	//TVAR_ash_pGAS[524] // 																	   
		{0x0F12, 0x001E},	//TVAR_ash_pGAS[525] // 																	   
		{0x0F12, 0x0018},	//TVAR_ash_pGAS[526] // 																	   
		{0x0F12, 0x001E},	//TVAR_ash_pGAS[527] // 																	   
		{0x0F12, 0x002F},	//TVAR_ash_pGAS[528] // 																	   
		{0x0F12, 0x0050},	//TVAR_ash_pGAS[529] // 																	   
		{0x0F12, 0x007E},	//TVAR_ash_pGAS[530] // 																	   
		{0x0F12, 0x00BD},	//TVAR_ash_pGAS[531] // 																	   
		{0x0F12, 0x00F7},	//TVAR_ash_pGAS[532] // 																	   
		{0x0F12, 0x011A},	//TVAR_ash_pGAS[533] // 																	   
		{0x0F12, 0x00DF},	//TVAR_ash_pGAS[534] // 																	   
		{0x0F12, 0x00A5},	//TVAR_ash_pGAS[535] // 																	   
		{0x0F12, 0x0076},	//TVAR_ash_pGAS[536] // 																	   
		{0x0F12, 0x0054},	//TVAR_ash_pGAS[537] // 																	   
		{0x0F12, 0x0041},	//TVAR_ash_pGAS[538] // 																	   
		{0x0F12, 0x003A},	//TVAR_ash_pGAS[539] // 																	   
		{0x0F12, 0x003E},	//TVAR_ash_pGAS[540] // 																	   
		{0x0F12, 0x0051},	//TVAR_ash_pGAS[541] // 																	   
		{0x0F12, 0x0073},	//TVAR_ash_pGAS[542] // 																	   
		{0x0F12, 0x00A1},	//TVAR_ash_pGAS[543] // 																	   
		{0x0F12, 0x00DB},	//TVAR_ash_pGAS[544] // 																	   
		{0x0F12, 0x0115},	//TVAR_ash_pGAS[545] // 																	   
		{0x0F12, 0x0157},	//TVAR_ash_pGAS[546] // 																	   
		{0x0F12, 0x0112},	//TVAR_ash_pGAS[547] // 																	   
		{0x0F12, 0x00DA},	//TVAR_ash_pGAS[548] // 																	   
		{0x0F12, 0x00AE},	//TVAR_ash_pGAS[549] // 																	   
		{0x0F12, 0x008D},	//TVAR_ash_pGAS[550] // 																	   
		{0x0F12, 0x0078},	//TVAR_ash_pGAS[551] // 																	   
		{0x0F12, 0x006F},	//TVAR_ash_pGAS[552] // 																	   
		{0x0F12, 0x0074},	//TVAR_ash_pGAS[553] // 																	   
		{0x0F12, 0x0087},	//TVAR_ash_pGAS[554] // 																	   
		{0x0F12, 0x00A7},	//TVAR_ash_pGAS[555] // 																	   
		{0x0F12, 0x00D4},	//TVAR_ash_pGAS[556] // 																	   
		{0x0F12, 0x010E},	//TVAR_ash_pGAS[557] // 																	   
		{0x0F12, 0x014F},	//TVAR_ash_pGAS[558] // 																	   
		{0x0F12, 0x0195},	//TVAR_ash_pGAS[559] // 																	   
		{0x0F12, 0x0147},	//TVAR_ash_pGAS[560] // 																	   
		{0x0F12, 0x0111},	//TVAR_ash_pGAS[561] // 																	   
		{0x0F12, 0x00E6},	//TVAR_ash_pGAS[562] // 																	   
		{0x0F12, 0x00C9},	//TVAR_ash_pGAS[563] // 																	   
		{0x0F12, 0x00B4},	//TVAR_ash_pGAS[564] // 																	   
		{0x0F12, 0x00AA},	//TVAR_ash_pGAS[565] // 																	   
		{0x0F12, 0x00AE},	//TVAR_ash_pGAS[566] // 																	   
		{0x0F12, 0x00C0},	//TVAR_ash_pGAS[567] // 																	   
		{0x0F12, 0x00DF},	//TVAR_ash_pGAS[568] // 																	   
		{0x0F12, 0x0106},	//TVAR_ash_pGAS[569] // 																	   
		{0x0F12, 0x0143},	//TVAR_ash_pGAS[570] // 																	   
		{0x0F12, 0x018F},	//TVAR_ash_pGAS[571] // 																	   
							 
		{0x002A, 0x074E},		
		{0x0F12, 0x0001},	//ash_bLumaMode //																					
		{0x002A, 0x0D30},								
		{0x0F12, 0x02A8},	//awbb_GLocu // 	   
		{0x0F12, 0x0347},	//awbb_GLocuSB //	   
														
		{0x002A, 0x06B8},								
		{0x0F12, 0x00C0},	//TVAR_ash_AwbashCord[0] // 																	   
		{0x0F12, 0x00E0},	//TVAR_ash_AwbashCord[1] // 																	   
		{0x0F12, 0x0120},	//TVAR_ash_AwbashCord[2] // 																	   
		{0x0F12, 0x0124},	//TVAR_ash_AwbashCord[3] // 																	   
		{0x0F12, 0x0156},	//TVAR_ash_AwbashCord[4] // 																	   
		{0x0F12, 0x017F},	//TVAR_ash_AwbashCord[5] // 																	   
		{0x0F12, 0x018F},	//TVAR_ash_AwbashCord[6] // 																	   
														
														
		{0x002A, 0x0664},								
		{0x0F12, 0x013E},	//seti_uContrastCenter //																		   
														
														
		{0x002A, 0x06C6},								
		{0x0F12, 0x010B},	//ash_CGrasalphaS[0] //
		{0x0F12, 0x0103},	//ash_CGrasalphaS[1] //
		{0x0F12, 0x00FC},	//ash_CGrasalphaS[2] //
		{0x0F12, 0x010C},	//ash_CGrasalphaS[3] //
							 
		{0x002A, 0x0C48},	 
		{0x0F12, 0x03C8}, //03C9	//awbb_IndoorGrZones_m_BGrid[0] //																				
		{0x0F12, 0x03DE}, //03DE	//awbb_IndoorGrZones_m_BGrid[1] //																		   
		{0x0F12, 0x0372}, //0372	//awbb_IndoorGrZones_m_BGrid[2] //																		   
		{0x0F12, 0x03EA}, //03EA	//awbb_IndoorGrZones_m_BGrid[3] //																		   
		{0x0F12, 0x0336}, //0336	//awbb_IndoorGrZones_m_BGrid[4] //																		   
		{0x0F12, 0x03DE}, //03DE	//awbb_IndoorGrZones_m_BGrid[5] //																		   
		{0x0F12, 0x0302}, //0302	//awbb_IndoorGrZones_m_BGrid[6] //																		   
		{0x0F12, 0x03A2}, //03A2	//awbb_IndoorGrZones_m_BGrid[7] //																		   
		{0x0F12, 0x02C8}, //02c8	//awbb_IndoorGrZones_m_BGrid[8] //																		   
		{0x0F12, 0x036C}, //0368	//awbb_IndoorGrZones_m_BGrid[9] //																		   
		{0x0F12, 0x0292}, //0292	//awbb_IndoorGrZones_m_BGrid[10] // 																	   
		{0x0F12, 0x0340}, //033A	//awbb_IndoorGrZones_m_BGrid[11] // 																	   
		{0x0F12, 0x0276}, //0262	//awbb_IndoorGrZones_m_BGrid[12] // 																	   
		{0x0F12, 0x0318}, //0306	//awbb_IndoorGrZones_m_BGrid[13] // 																	   
		{0x0F12, 0x025A}, //0250	//awbb_IndoorGrZones_m_BGrid[14] // 																	   
		{0x0F12, 0x02F4}, //02C2	//awbb_IndoorGrZones_m_BGrid[15] // 																	   
		{0x0F12, 0x0246}, //023A	//awbb_IndoorGrZones_m_BGrid[16] // 																	   
		{0x0F12, 0x02D6}, //02A2	//awbb_IndoorGrZones_m_BGrid[17] // 																	   
		{0x0F12, 0x0232}, //0228	//awbb_IndoorGrZones_m_BGrid[18] // 																	   
		{0x0F12, 0x02B6}, //0298	//awbb_IndoorGrZones_m_BGrid[19] // 																	   
		{0x0F12, 0x021E}, //0210	//awbb_IndoorGrZones_m_BGrid[20] // 																	   
		{0x0F12, 0x0298}, //029C	//awbb_IndoorGrZones_m_BGrid[21] // 																	   
		{0x0F12, 0x0208}, //01FE	//awbb_IndoorGrZones_m_BGrid[22] // 																	   
		{0x0F12, 0x027E}, //0292	//awbb_IndoorGrZones_m_BGrid[23] // 																	   
		{0x0F12, 0x01EE}, //01EE	//awbb_IndoorGrZones_m_BGrid[24] // 																	   
		{0x0F12, 0x0264}, //0278	//awbb_IndoorGrZones_m_BGrid[25] // 																	   
		{0x0F12, 0x01F0}, //01F2	//awbb_IndoorGrZones_m_BGrid[26] // 																	   
		{0x0F12, 0x0248}, //0268	//awbb_IndoorGrZones_m_BGrid[27] // 																	   
		{0x0F12, 0x0000}, //0200	//awbb_IndoorGrZones_m_BGrid[28] // 																	   
		{0x0F12, 0x0000}, //0246	//awbb_IndoorGrZones_m_BGrid[29] // 																	   
		{0x0F12, 0x0000},	//awbb_IndoorGrZones_m_BGrid[30] // 																	   
		{0x0F12, 0x0000},	//awbb_IndoorGrZones_m_BGrid[31] // 																	   
		{0x0F12, 0x0000},	//awbb_IndoorGrZones_m_BGrid[32] // 																	   
		{0x0F12, 0x0000},	//awbb_IndoorGrZones_m_BGrid[33] // 																	   
		{0x0F12, 0x0000},	//awbb_IndoorGrZones_m_BGrid[34] // 																	   
		{0x0F12, 0x0000},	//awbb_IndoorGrZones_m_BGrid[35] // 																	   
		{0x0F12, 0x0000},	//awbb_IndoorGrZones_m_BGrid[36] // 																	   
		{0x0F12, 0x0000},	//awbb_IndoorGrZones_m_BGrid[37] // 																	   
		{0x0F12, 0x0000},	//awbb_IndoorGrZones_m_BGrid[38] // 																	   
		{0x0F12, 0x0000},	//awbb_IndoorGrZones_m_BGrid[39] // 																	   
																
		{0x0F12, 0x0005},	//awbb_IndoorGrZones_m_Gridstep //	
		
		{0x002A, 0x0C9C},
		{0x0F12, 0x000E},
		{0x002A, 0x0CA0},										
		{0x0F12, 0x0108},	//awbb_IndoorGrZones_m_Boffs //
																
		{0x002A, 0x0CE0},										
		{0x0F12, 0x03D4},	//awbb_LowBrGrZones_m_BGrid[0] //																		   
		{0x0F12, 0x043E},	//awbb_LowBrGrZones_m_BGrid[1] //																		   
		{0x0F12, 0x035C},	//awbb_LowBrGrZones_m_BGrid[2] //																		   
		{0x0F12, 0x0438},	//awbb_LowBrGrZones_m_BGrid[3] //																		   
		{0x0F12, 0x02F0},	//awbb_LowBrGrZones_m_BGrid[4] //																		   
		{0x0F12, 0x042D},	//awbb_LowBrGrZones_m_BGrid[5] //																		   
		{0x0F12, 0x029A},	//awbb_LowBrGrZones_m_BGrid[6] //																		   
		{0x0F12, 0x03EF},	//awbb_LowBrGrZones_m_BGrid[7] //																		   
		{0x0F12, 0x025E},	//awbb_LowBrGrZones_m_BGrid[8] //																		   
		{0x0F12, 0x0395},	//awbb_LowBrGrZones_m_BGrid[9] //																		   
		{0x0F12, 0x022E},	//awbb_LowBrGrZones_m_BGrid[10] //																		   
		{0x0F12, 0x0346},	//awbb_LowBrGrZones_m_BGrid[11] //																		   
		{0x0F12, 0x0200},	//awbb_LowBrGrZones_m_BGrid[12] //																		   
		{0x0F12, 0x02F6},	//awbb_LowBrGrZones_m_BGrid[13] //																		   
		{0x0F12, 0x01CE},	//awbb_LowBrGrZones_m_BGrid[14] //																		   
		{0x0F12, 0x02C8},	//awbb_LowBrGrZones_m_BGrid[15] //																		   
		{0x0F12, 0x01BB},	//awbb_LowBrGrZones_m_BGrid[16] //																		   
		{0x0F12, 0x0287},	//awbb_LowBrGrZones_m_BGrid[17] //																		   
		{0x0F12, 0x01E2},	//awbb_LowBrGrZones_m_BGrid[18] //																		   
		{0x0F12, 0x0239},	//awbb_LowBrGrZones_m_BGrid[19] //																		   
		{0x0F12, 0x0000},	//awbb_LowBrGrZones_m_BGrid[20] //																		   
		{0x0F12, 0x0000},	//awbb_LowBrGrZones_m_BGrid[21] //																		   
		{0x0F12, 0x0000},	//awbb_LowBrGrZones_m_BGrid[22] //																		   
		{0x0F12, 0x0000},	//awbb_LowBrGrZones_m_BGrid[23] //																		   
							 
		{0x0F12, 0x0006},	//awbb_LowBrGrZones_m_Gridstep //																			
		{0x002A, 0x0D18},									  
		{0x0F12, 0x00AE},	//awbb_LowBrGrZones_m_Boff //
		
		{0x002A, 0x0CA4},	 
		{0x0F12, 0x026E},	//0294//0286//02C2//awbb_OutdoorGrZones_m_BGrid[0] //																				
		{0x0F12, 0x02A4},	//02CB//02BD//02E0//awbb_OutdoorGrZones_m_BGrid[1] //																		  
		{0x0F12, 0x0262},	//027A//026C//0278//awbb_OutdoorGrZones_m_BGrid[2] //																		  
		{0x0F12, 0x02A8},	//02D7//02C9//02BC//awbb_OutdoorGrZones_m_BGrid[3] //																		  
		{0x0F12, 0x0256},	//0266//0258//025A//awbb_OutdoorGrZones_m_BGrid[4] //																		  
		{0x0F12, 0x02AE},	//02BF//02B1//02A2//awbb_OutdoorGrZones_m_BGrid[5] //																		  
		{0x0F12, 0x0248},	//0252//0244//024A//awbb_OutdoorGrZones_m_BGrid[6] //																		  
		{0x0F12, 0x02A4},	//02A8//029A//0288//awbb_OutdoorGrZones_m_BGrid[7] //																		  
		{0x0F12, 0x023E},	//023E//0230//0240//awbb_OutdoorGrZones_m_BGrid[8] //																		  
		{0x0F12, 0x029A},	//028F//0281//0278//awbb_OutdoorGrZones_m_BGrid[9] //																		  
		{0x0F12, 0x023A},	//0239//022B//023E//awbb_OutdoorGrZones_m_BGrid[10] //																		  
		{0x0F12, 0x0290},	//027A//026C//0254//awbb_OutdoorGrZones_m_BGrid[11] //																		  
		{0x0F12, 0x023A},	//024A//023C//0000//awbb_OutdoorGrZones_m_BGrid[12] //																		  
		{0x0F12, 0x027E},	//0260//0252//0000//awbb_OutdoorGrZones_m_BGrid[13] //																		  
		{0x0F12, 0x0244},	//0000//0000//0000//awbb_OutdoorGrZones_m_BGrid[14] //																		  
		{0x0F12, 0x0266},	//0000//0000//0000//awbb_OutdoorGrZones_m_BGrid[15] //																		  
		{0x0F12, 0x0000},	//0000//0000//0000//awbb_OutdoorGrZones_m_BGrid[16] //																		  
		{0x0F12, 0x0000},	//0000//0000//0000//awbb_OutdoorGrZones_m_BGrid[17] //																		  
		{0x0F12, 0x0000},	//0000//0000//0000//awbb_OutdoorGrZones_m_BGrid[18] //																		  
		{0x0F12, 0x0000},	//0000//0000//0000//awbb_OutdoorGrZones_m_BGrid[19] //																		  
		{0x0F12, 0x0000},	//0000//0000//0000//awbb_OutdoorGrZones_m_BGrid[20] //																		  
		{0x0F12, 0x0000},	//0000//0000//0000//awbb_OutdoorGrZones_m_BGrid[21] //																		  
		{0x0F12, 0x0000},	//0000//0000//0000//awbb_OutdoorGrZones_m_BGrid[22] //																		  
		{0x0F12, 0x0000},	//0000//0000//0000//awbb_OutdoorGrZones_m_BGrid[23] //																		  
		
		{0x0F12, 0x0004},	//awbb_OutdoorGrZones_m_Gridstep // 	  
		{0x002A, 0x0CD8},														   
		{0x0F12, 0x0008},				
		{0x002A, 0x0CDC},														   
		{0x0F12, 0x0204},	//awbb_OutdoorGrZones_m_Boff // 				  
		{0x002A, 0x0D1C},														   
		{0x0F12, 0x037C},	//awbb_CrclLowT_R_c //							  
		{0x002A, 0x0D20},														   
		{0x0F12, 0x0157},	//awbb_CrclLowT_B_c //							  
		{0x002A, 0x0D24},														   
		{0x0F12, 0x3EB8},	//awbb_CrclLowT_Rad_c // 
		
		{0x002A, 0x0D2C},	 
		{0x0F12, 0x013D},	//awbb_IntcR // 																		
		{0x0F12, 0x011E},	//awbb_IntcB // 																	   
		{0x002A, 0x0D46},	 
		{0x0F12, 0x04C0},	//0554//055D//0396//04A2//awbb_MvEq_RBthresh // 																		
							 
							 
							 
		{0x002A, 0x0D28},	 //wp outdoor
		{0x0F12, 0x0270},																			
		{0x0F12, 0x0240},																			
																			
												
		{0x002A, 0x0D5C},	 
		{0x0F12, 0x7FFF},																			
		{0x0F12, 0x0050},																			
							 
		{0x002A, 0x2316},	 
		{0x0F12, 0x0006},																			
							 
		{0x002A, 0x0E44},	 
		{0x0F12, 0x05A5},																			
		{0x0F12, 0x0400},																			
		{0x0F12, 0x063C},																			
							 
		{0x002A, 0x0E36},	 
		{0x0F12, 0x0028},	 //R OFFSET 																	  
		{0x0F12, 0xFFD8},	 //B OFFSET 																	  
		{0x0F12, 0x0000},	 //G OFFSET 																	  
		
		{0x002A, 0x0DD4},	 
		{0x0F12, 0x0000},	//awbb_GridCorr_R[0] // 																				
		{0x0F12, 0xFFA6},	//awbb_GridCorr_R[1] //    
		{0x0F12, 0x0000},	//awbb_GridCorr_R[2] //    
		{0x0F12, 0xFFCE},	//awbb_GridCorr_R[3] //    
		{0x0F12, 0xFFB0},	//awbb_GridCorr_R[4] //    
		{0x0F12, 0x0064},	//awbb_GridCorr_R[5] //    
				            	   
		{0x0F12, 0x0000},	//awbb_GridCorr_R[6] //    
		{0x0F12, 0xFFA6},	//awbb_GridCorr_R[7] //    
		{0x0F12, 0x0000},	//awbb_GridCorr_R[8] //    
		{0x0F12, 0xFFCE},	//awbb_GridCorr_R[9] //    
		{0x0F12, 0xFFB0},	//awbb_GridCorr_R[10] //   
		{0x0F12, 0x0064},	//awbb_GridCorr_R[11] //   
				            	   
		{0x0F12, 0x0000},	//awbb_GridCorr_R[12] //   
		{0x0F12, 0xFFA6},	//awbb_GridCorr_R[13] //   
		{0x0F12, 0x0000},	//awbb_GridCorr_R[14] //   
		{0x0F12, 0xFFCE},	//awbb_GridCorr_R[15] //   
		{0x0F12, 0xFFB0},	//awbb_GridCorr_R[16] //   
		{0x0F12, 0x0064},	//awbb_GridCorr_R[17] //   
				            	   
		{0x0F12, 0x0000},	//awbb_GridCorr_B[0] ////																		 
		{0x0F12, 0x0000},	//awbb_GridCorr_B[1] // 																		 
		{0x0F12, 0x0032},	//awbb_GridCorr_B[2] // 																		 
		{0x0F12, 0x0000},	//awbb_GridCorr_B[3] // 																		 
		{0x0F12, 0x003C},	//awbb_GridCorr_B[4] // 																		 
		{0x0F12, 0xFFC0},	//awbb_GridCorr_B[5] // 																		 
				               
		{0x0F12, 0x0000},	//awbb_GridCorr_B[6] // 																		 
		{0x0F12, 0x0000},	//awbb_GridCorr_B[7] // 																		 
		{0x0F12, 0x0032},	//awbb_GridCorr_B[8] // 																		 
		{0x0F12, 0x0000},	//awbb_GridCorr_B[9] // 																		 
		{0x0F12, 0x003C},	//awbb_GridCorr_B[10] //																		 
		{0x0F12, 0xFFC0},	//awbb_GridCorr_B[11] //																		 
				               
		{0x0F12, 0x0000},	//awbb_GridCorr_B[12] //																		 
		{0x0F12, 0x0000},	//awbb_GridCorr_B[13] //																		 
		{0x0F12, 0x0032},	//awbb_GridCorr_B[14] //																		 
		{0x0F12, 0x0000},	//awbb_GridCorr_B[15] //																		 
		{0x0F12, 0x003C},	//awbb_GridCorr_B[16] //   
		{0x0F12, 0xFFC0},	//awbb_GridCorr_B[17] //				 
						
		{0x0F12, 0x02D9},	//awbb_GridConst_1[0] //																	 
		{0x0F12, 0x0357},	//awbb_GridConst_1[1] //																	 
		{0x0F12, 0x03D1},	//awbb_GridConst_1[2] //																	 
						
						
		{0x0F12, 0x0DF6},	//0E4F//0DE9//0DE9//awbb_GridConst_2[0] //													 
		{0x0F12, 0x0ED8},	//0EDD//0EDD//0EDD//awbb_GridConst_2[1] //													 
		{0x0F12, 0x0F51},	//0F42//0F42//0F42//awbb_GridConst_2[2] //													 
		{0x0F12, 0x0F5C},	//0F4E//0F4E//0F54//awbb_GridConst_2[3] //													 
		{0x0F12, 0x0F8F},	//0F99//0F99//0FAE//awbb_GridConst_2[4] //													 
		{0x0F12, 0x1006},	//1006//1006//1011//awbb_GridConst_2[5] //													 
						
		{0x0F12, 0x00AC},	//00BA//awbb_GridCoeff_R_1																	 
		{0x0F12, 0x00BD},	//00AF//awbb_GridCoeff_B_1																	 
		{0x0F12, 0x0049},	//0049//awbb_GridCoeff_R_2																	 
		{0x0F12, 0x00F5},	//00F5//awbb_GridCoeff_B_2																	 
						
		{0x002A, 0x0E4A},						  
		{0x0F12, 0x0002},	//awbb_GridEnable// 																		 
						
		{0x002A, 0x051A},						  
		{0x0F12, 0x010E},	//lt_uLimitHigh//
		{0x0F12, 0x00F5},	//lt_uLimitLow// 
						
						
		{0x002A, 0x0F76},						  
		{0x0F12, 0x0007},	//ae_statmode BLC off : 0x0F, on : 0x0D//  illumType On : 07 , Off : 0F
						
		{0x002A, 0x1034},						  
		{0x0F12, 0x00C0},	//saRR_IllumType[0] //																		 
		{0x0F12, 0x00E0},	//saRR_IllumType[1] //																		 
		{0x0F12, 0x0104},	//saRR_IllumType[2] //																		 
		{0x0F12, 0x0129},	//saRR_IllumType[3] //																		 
		{0x0F12, 0x0156},	//saRR_IllumType[4] //																		 
		{0x0F12, 0x017F},	//saRR_IllumType[5] //																		 
		{0x0F12, 0x018F},	//saRR_IllumType[6] //																		 
						
						
		{0x0F12, 0x0120},	//saRR_IllumTypeF[0] // 																	 
		{0x0F12, 0x0120},	//saRR_IllumTypeF[1] // 																	 
		{0x0F12, 0x0120},	//saRR_IllumTypeF[2] // 																	 
		{0x0F12, 0x0100},	//saRR_IllumTypeF[3] // 																	 
		{0x0F12, 0x0100},	//saRR_IllumTypeF[4] // 																	 
		{0x0F12, 0x0100},	//saRR_IllumTypeF[5] // 																	 
		{0x0F12, 0x0100},	//saRR_IllumTypeF[6] // 																	 
							 
							 
							 
		{0x002A, 0x3288},	//saRR_usDualGammaLutRGBIndoor	//																							
		{0x0F12, 0x0000}, //	saRR_usDualGammaLutRGBIndoor[0] //[0] //												 
		{0x0F12, 0x0008}, //  saRR_usDualGammaLutRGBIndoor[0] //[1] //														 
		{0x0F12, 0x0013}, //  saRR_usDualGammaLutRGBIndoor[0] //[2] //														 
		{0x0F12, 0x002C}, //  saRR_usDualGammaLutRGBIndoor[0] //[3] //														 
		{0x0F12, 0x0062}, //  saRR_usDualGammaLutRGBIndoor[0] //[4] //														 
		{0x0F12, 0x00CD}, //  saRR_usDualGammaLutRGBIndoor[0] //[5] //														 
		{0x0F12, 0x0129}, //  saRR_usDualGammaLutRGBIndoor[0] //[6] //														 
		{0x0F12, 0x0151}, //  saRR_usDualGammaLutRGBIndoor[0] //[7] //														 
		{0x0F12, 0x0174}, //  saRR_usDualGammaLutRGBIndoor[0] //[8] //														 
		{0x0F12, 0x01AA}, //  saRR_usDualGammaLutRGBIndoor[0] //[9] //														 
		{0x0F12, 0x01D7}, //  saRR_usDualGammaLutRGBIndoor[0] //[10] // 													 
		{0x0F12, 0x01FE}, //  saRR_usDualGammaLutRGBIndoor[0] //[11] // 													 
		{0x0F12, 0x0221}, //  saRR_usDualGammaLutRGBIndoor[0] //[12] // 													 
		{0x0F12, 0x025D}, //  saRR_usDualGammaLutRGBIndoor[0] //[13] // 													 
		{0x0F12, 0x0291}, //  saRR_usDualGammaLutRGBIndoor[0] //[14] // 													 
		{0x0F12, 0x02EB}, //  saRR_usDualGammaLutRGBIndoor[0] //[15] // 													 
		{0x0F12, 0x033A}, //  saRR_usDualGammaLutRGBIndoor[0] //[16] // 													 
		{0x0F12, 0x0380}, //  saRR_usDualGammaLutRGBIndoor[0] //[17] // 													 
		{0x0F12, 0x03C2}, //  saRR_usDualGammaLutRGBIndoor[0] //[18] // 													 
		{0x0F12, 0x03FF}, //  saRR_usDualGammaLutRGBIndoor[0] //[19] // 													 
		{0x0F12, 0x0000}, //  saRR_usDualGammaLutRGBIndoor[1] //[0] //														 
		{0x0F12, 0x0008}, //  saRR_usDualGammaLutRGBIndoor[1] //[1] //														 
		{0x0F12, 0x0013}, //  saRR_usDualGammaLutRGBIndoor[1] //[2] //														 
		{0x0F12, 0x002C}, //  saRR_usDualGammaLutRGBIndoor[1] //[3] //														 
		{0x0F12, 0x0062}, //  saRR_usDualGammaLutRGBIndoor[1] //[4] //														 
		{0x0F12, 0x00CD}, //  saRR_usDualGammaLutRGBIndoor[1] //[5] //														 
		{0x0F12, 0x0129}, //  saRR_usDualGammaLutRGBIndoor[1] //[6] //														 
		{0x0F12, 0x0151}, //  saRR_usDualGammaLutRGBIndoor[1] //[7] //														 
		{0x0F12, 0x0174}, //  saRR_usDualGammaLutRGBIndoor[1] //[8] //														 
		{0x0F12, 0x01AA}, //  saRR_usDualGammaLutRGBIndoor[1] //[9] //														 
		{0x0F12, 0x01D7}, //  saRR_usDualGammaLutRGBIndoor[1] //[10] // 													 
		{0x0F12, 0x01FE}, //  saRR_usDualGammaLutRGBIndoor[1] //[11] // 													 
		{0x0F12, 0x0221}, //  saRR_usDualGammaLutRGBIndoor[1] //[12] // 													 
		{0x0F12, 0x025D}, //  saRR_usDualGammaLutRGBIndoor[1] //[13] // 													 
		{0x0F12, 0x0291}, //  saRR_usDualGammaLutRGBIndoor[1] //[14] // 													 
		{0x0F12, 0x02EB}, //  saRR_usDualGammaLutRGBIndoor[1] //[15] // 													 
		{0x0F12, 0x033A}, //  saRR_usDualGammaLutRGBIndoor[1] //[16] // 													 
		{0x0F12, 0x0380}, //  saRR_usDualGammaLutRGBIndoor[1] //[17] // 													 
		{0x0F12, 0x03C2}, //  saRR_usDualGammaLutRGBIndoor[1] //[18] // 													 
		{0x0F12, 0x03FF}, //  saRR_usDualGammaLutRGBIndoor[1] //[19] // 													 
		{0x0F12, 0x0000}, //  saRR_usDualGammaLutRGBIndoor[2] //[0] //														 
		{0x0F12, 0x0008}, //  saRR_usDualGammaLutRGBIndoor[2] //[1] //														 
		{0x0F12, 0x0013}, //  saRR_usDualGammaLutRGBIndoor[2] //[2] //														 
		{0x0F12, 0x002C}, //  saRR_usDualGammaLutRGBIndoor[2] //[3] //														 
		{0x0F12, 0x0062}, //  saRR_usDualGammaLutRGBIndoor[2] //[4] //														 
		{0x0F12, 0x00CD}, //  saRR_usDualGammaLutRGBIndoor[2] //[5] //														 
		{0x0F12, 0x0129}, //  saRR_usDualGammaLutRGBIndoor[2] //[6] //														 
		{0x0F12, 0x0151}, //  saRR_usDualGammaLutRGBIndoor[2] //[7] //														 
		{0x0F12, 0x0174}, //  saRR_usDualGammaLutRGBIndoor[2] //[8] //														 
		{0x0F12, 0x01AA}, //  saRR_usDualGammaLutRGBIndoor[2] //[9] //														 
		{0x0F12, 0x01D7}, //  saRR_usDualGammaLutRGBIndoor[2] //[10] // 													 
		{0x0F12, 0x01FE}, //  saRR_usDualGammaLutRGBIndoor[2] //[11] // 													 
		{0x0F12, 0x0221}, //  saRR_usDualGammaLutRGBIndoor[2] //[12] // 													 
		{0x0F12, 0x025D}, //  saRR_usDualGammaLutRGBIndoor[2] //[13] // 													 
		{0x0F12, 0x0291}, //  saRR_usDualGammaLutRGBIndoor[2] //[14] // 													 
		{0x0F12, 0x02EB}, //  saRR_usDualGammaLutRGBIndoor[2] //[15] // 													 
		{0x0F12, 0x033A}, //  saRR_usDualGammaLutRGBIndoor[2] //[16] // 													 
		{0x0F12, 0x0380}, //  saRR_usDualGammaLutRGBIndoor[2] //[17] // 													 
		{0x0F12, 0x03C2}, //  saRR_usDualGammaLutRGBIndoor[2] //[18] // 													 
		{0x0F12, 0x03FF}, //  saRR_usDualGammaLutRGBIndoor[2] //[19] // 													 
													
													
		{0x0F12, 0x0000},	//			saRR_usDualGammaLutRGBOutdoor[0] //[0] //
		{0x0F12, 0x0008},	//	saRR_usDualGammaLutRGBOutdoor[0] //[1] //												 
		{0x0F12, 0x0013},	//	saRR_usDualGammaLutRGBOutdoor[0] //[2] //												 
		{0x0F12, 0x002C},	//	saRR_usDualGammaLutRGBOutdoor[0] //[3] //												 
		{0x0F12, 0x0062},	//	saRR_usDualGammaLutRGBOutdoor[0] //[4] //												 
		{0x0F12, 0x00CD},	//	saRR_usDualGammaLutRGBOutdoor[0] //[5] //												 
		{0x0F12, 0x0129},	//	saRR_usDualGammaLutRGBOutdoor[0] //[6] //												 
		{0x0F12, 0x0151},	//	saRR_usDualGammaLutRGBOutdoor[0] //[7] //												 
		{0x0F12, 0x0174},	//	saRR_usDualGammaLutRGBOutdoor[0] //[8] //												 
		{0x0F12, 0x01AA},	//	saRR_usDualGammaLutRGBOutdoor[0] //[9] //												 
		{0x0F12, 0x01D7},	//	saRR_usDualGammaLutRGBOutdoor[0] //[10] //												 
		{0x0F12, 0x01FE},	//	saRR_usDualGammaLutRGBOutdoor[0] //[11] //												 
		{0x0F12, 0x0221},	//	saRR_usDualGammaLutRGBOutdoor[0] //[12] //												 
		{0x0F12, 0x025D},	//	saRR_usDualGammaLutRGBOutdoor[0] //[13] //												 
		{0x0F12, 0x0291},	//	saRR_usDualGammaLutRGBOutdoor[0] //[14] //												 
		{0x0F12, 0x02EB},	//	saRR_usDualGammaLutRGBOutdoor[0] //[15] //												 
		{0x0F12, 0x033A},	//	saRR_usDualGammaLutRGBOutdoor[0] //[16] //												 
		{0x0F12, 0x0380},	//	saRR_usDualGammaLutRGBOutdoor[0] //[17] //												 
		{0x0F12, 0x03C2},	//	saRR_usDualGammaLutRGBOutdoor[0] //[18] //												 
		{0x0F12, 0x03FF},	//	saRR_usDualGammaLutRGBOutdoor[0] //[19] //												 
		{0x0F12, 0x0000},	//	saRR_usDualGammaLutRGBOutdoor[1] //[0] //												 
		{0x0F12, 0x0008},	//	saRR_usDualGammaLutRGBOutdoor[1] //[1] //												 
		{0x0F12, 0x0013},	//	saRR_usDualGammaLutRGBOutdoor[1] //[2] //												 
		{0x0F12, 0x002C},	//	saRR_usDualGammaLutRGBOutdoor[1] //[3] //												 
		{0x0F12, 0x0062},	//	saRR_usDualGammaLutRGBOutdoor[1] //[4] //												 
		{0x0F12, 0x00CD},	//	saRR_usDualGammaLutRGBOutdoor[1] //[5] //												 
		{0x0F12, 0x0129},	//	saRR_usDualGammaLutRGBOutdoor[1] //[6] //												 
		{0x0F12, 0x0151},	//	saRR_usDualGammaLutRGBOutdoor[1] //[7] //												 
		{0x0F12, 0x0174},	//	saRR_usDualGammaLutRGBOutdoor[1] //[8] //												 
		{0x0F12, 0x01AA},	//	saRR_usDualGammaLutRGBOutdoor[1] //[9] //												 
		{0x0F12, 0x01D7},	//	saRR_usDualGammaLutRGBOutdoor[1] //[10] //												 
		{0x0F12, 0x01FE},	//	saRR_usDualGammaLutRGBOutdoor[1] //[11] //												 
		{0x0F12, 0x0221},	//	saRR_usDualGammaLutRGBOutdoor[1] //[12] //												 
		{0x0F12, 0x025D},	//	saRR_usDualGammaLutRGBOutdoor[1] //[13] //												 
		{0x0F12, 0x0291},	//	saRR_usDualGammaLutRGBOutdoor[1] //[14] //												 
		{0x0F12, 0x02EB},	//	saRR_usDualGammaLutRGBOutdoor[1] //[15] //												 
		{0x0F12, 0x033A},	//	saRR_usDualGammaLutRGBOutdoor[1] //[16] //												 
		{0x0F12, 0x0380},	//	saRR_usDualGammaLutRGBOutdoor[1] //[17] //												 
		{0x0F12, 0x03C2},	//	saRR_usDualGammaLutRGBOutdoor[1] //[18] //												 
		{0x0F12, 0x03FF},	//	saRR_usDualGammaLutRGBOutdoor[1] //[19] //												 
		{0x0F12, 0x0000},	//	saRR_usDualGammaLutRGBOutdoor[2] //[0] //												 
		{0x0F12, 0x0008},	//	saRR_usDualGammaLutRGBOutdoor[2] //[1] //												 
		{0x0F12, 0x0013},	//	saRR_usDualGammaLutRGBOutdoor[2] //[2] //												 
		{0x0F12, 0x002C},	//	saRR_usDualGammaLutRGBOutdoor[2] //[3] //												 
		{0x0F12, 0x0062},	//	saRR_usDualGammaLutRGBOutdoor[2] //[4] //												 
		{0x0F12, 0x00CD},	//	saRR_usDualGammaLutRGBOutdoor[2] //[5] //												 
		{0x0F12, 0x0129},	//	saRR_usDualGammaLutRGBOutdoor[2] //[6] //												 
		{0x0F12, 0x0151},	//	saRR_usDualGammaLutRGBOutdoor[2] //[7] //												 
		{0x0F12, 0x0174},	//	saRR_usDualGammaLutRGBOutdoor[2] //[8] //												 
		{0x0F12, 0x01AA},	//	saRR_usDualGammaLutRGBOutdoor[2] //[9] //												 
		{0x0F12, 0x01D7},	//	saRR_usDualGammaLutRGBOutdoor[2] //[10] //												 
		{0x0F12, 0x01FE},	//	saRR_usDualGammaLutRGBOutdoor[2] //[11] //												 
		{0x0F12, 0x0221},	//	saRR_usDualGammaLutRGBOutdoor[2] //[12] //												 
		{0x0F12, 0x025D},	//	saRR_usDualGammaLutRGBOutdoor[2] //[13] //												 
		{0x0F12, 0x0291},	//	saRR_usDualGammaLutRGBOutdoor[2] //[14] //												 
		{0x0F12, 0x02EB},	//	saRR_usDualGammaLutRGBOutdoor[2] //[15] //												 
		{0x0F12, 0x033A},	//	saRR_usDualGammaLutRGBOutdoor[2] //[16] //												 
		{0x0F12, 0x0380},	//	saRR_usDualGammaLutRGBOutdoor[2] //[17] //												 
		{0x0F12, 0x03C2},	//	saRR_usDualGammaLutRGBOutdoor[2] //[18] //												 
		{0x0F12, 0x03FF},	//	saRR_usDualGammaLutRGBOutdoor[2] //[19] //												 
													
													
		{0x002A, 0x06A6},							
		{0x0F12, 0x00C0},	//saRR_AwbCcmCord[0] // 					 
		{0x0F12, 0x00F8}, //00E0	//saRR_AwbCcmCord[1] // 					 
		{0x0F12, 0x0110},	//saRR_AwbCcmCord[2] // 					 
		{0x0F12, 0x0139},	//saRR_AwbCcmCord[3] // 					 
		{0x0F12, 0x0166},	//saRR_AwbCcmCord[4] // 					 
		{0x0F12, 0x019F},	//saRR_AwbCcmCord[5] // 					 
							 
		{0x002A, 0x33A4},	 
		{0x0F12, 0x0137},	  //TVAR_wbt_pBaseCcmS[0] //														   
		{0x0F12, 0xFF76},	//TVAR_wbt_pBaseCcmS[1] //														  
		{0x0F12, 0xFF9F},	//TVAR_wbt_pBaseCcmS[2] //														  
		{0x0F12, 0xFFA6},	//TVAR_wbt_pBaseCcmS[3] //														  
		{0x0F12, 0x00FA},	//TVAR_wbt_pBaseCcmS[4] //														  
		{0x0F12, 0xFF6C},	//TVAR_wbt_pBaseCcmS[5] //														  
		{0x0F12, 0xFFFC},	//TVAR_wbt_pBaseCcmS[6] //														  
		{0x0F12, 0x0004},	//TVAR_wbt_pBaseCcmS[7] //														  
		{0x0F12, 0x0192},	//TVAR_wbt_pBaseCcmS[8] //														  
		{0x0F12, 0x0162},	//TVAR_wbt_pBaseCcmS[9] //														  
		{0x0F12, 0x0073},	//TVAR_wbt_pBaseCcmS[10] // 													  
		{0x0F12, 0xFDDC},	//TVAR_wbt_pBaseCcmS[11] // 													  
		{0x0F12, 0x0117},	//TVAR_wbt_pBaseCcmS[12] // 													  
		{0x0F12, 0xFEF8},	//TVAR_wbt_pBaseCcmS[13] // 													  
		{0x0F12, 0x0174},	//TVAR_wbt_pBaseCcmS[14] // 													  
		{0x0F12, 0xFF7E},	//TVAR_wbt_pBaseCcmS[15] // 													  
		{0x0F12, 0x00F9},	//TVAR_wbt_pBaseCcmS[16] // 													  
		{0x0F12, 0x00E3},	//TVAR_wbt_pBaseCcmS[17] // 													  
			 
		{0x0F12, 0x0137},	//TVAR_wbt_pBaseCcmS[18] // 													  
		{0x0F12, 0xFF76},	//TVAR_wbt_pBaseCcmS[19] // 													  
		{0x0F12, 0xFF9F},	//TVAR_wbt_pBaseCcmS[20] // 													  
		{0x0F12, 0xFFA6},	//TVAR_wbt_pBaseCcmS[21] // 													  
		{0x0F12, 0x00FA},	//TVAR_wbt_pBaseCcmS[22] // 													  
		{0x0F12, 0xFF6C},	//TVAR_wbt_pBaseCcmS[23] // 													  
		{0x0F12, 0xFFFC},	//TVAR_wbt_pBaseCcmS[24] // 													  
		{0x0F12, 0x0004},	//TVAR_wbt_pBaseCcmS[25] // 													  
		{0x0F12, 0x0192},	//TVAR_wbt_pBaseCcmS[26] // 													  
		{0x0F12, 0x0162},	//TVAR_wbt_pBaseCcmS[27] // 													  
		{0x0F12, 0x0073},	//TVAR_wbt_pBaseCcmS[28] // 													  
		{0x0F12, 0xFDDC},	//TVAR_wbt_pBaseCcmS[29] // 													  
		{0x0F12, 0x0117},	//TVAR_wbt_pBaseCcmS[30] // 													  
		{0x0F12, 0xFEF8},	//TVAR_wbt_pBaseCcmS[31] // 													  
		{0x0F12, 0x0174},	//TVAR_wbt_pBaseCcmS[32] // 													  
		{0x0F12, 0xFF7E},	//TVAR_wbt_pBaseCcmS[33] // 													  
		{0x0F12, 0x00F9},	//TVAR_wbt_pBaseCcmS[34] // 													  
		{0x0F12, 0x00E3},	//TVAR_wbt_pBaseCcmS[35] // 													  
			
		{0x0F12, 0x01E0},	//TVAR_wbt_pBaseCcmS[36] // 													  
		{0x0F12, 0xFFBC},	//TVAR_wbt_pBaseCcmS[37] // 													  
		{0x0F12, 0xFFF9},	//TVAR_wbt_pBaseCcmS[38] // 													  
		{0x0F12, 0xFF36},	//TVAR_wbt_pBaseCcmS[39] // 													  
		{0x0F12, 0x0028},	//TVAR_wbt_pBaseCcmS[40] // 													  
		{0x0F12, 0xFF46},	//TVAR_wbt_pBaseCcmS[41] // 													  
		{0x0F12, 0x0000},	//TVAR_wbt_pBaseCcmS[42] // 													  
		{0x0F12, 0x0032},	//TVAR_wbt_pBaseCcmS[43] // 													  
		{0x0F12, 0x0156},	//TVAR_wbt_pBaseCcmS[44] // 													  
		{0x0F12, 0x00F8},	//TVAR_wbt_pBaseCcmS[45] // 													  
		{0x0F12, 0x0091},	//TVAR_wbt_pBaseCcmS[46] // 													  
		{0x0F12, 0xFEF0},	//TVAR_wbt_pBaseCcmS[47] // 													  
		{0x0F12, 0x0165},	//TVAR_wbt_pBaseCcmS[48] // 													  
		{0x0F12, 0xFF34},	//TVAR_wbt_pBaseCcmS[49] // 													  
		{0x0F12, 0x0119},	//TVAR_wbt_pBaseCcmS[50] // 													  
		{0x0F12, 0xFFA9},	//TVAR_wbt_pBaseCcmS[51] // 													  
		{0x0F12, 0x00DB},	//TVAR_wbt_pBaseCcmS[52] // 													  
		{0x0F12, 0x00AD},	//TVAR_wbt_pBaseCcmS[53] // 																	   
		   
		{0x0F12, 0x01E0},  //01FA	//TVAR_wbt_pBaseCcmS[54] // 													  
		{0x0F12, 0xFFBC},  //FF9B	//TVAR_wbt_pBaseCcmS[55] // 													  
		{0x0F12, 0xFFF9},  //FFFF	//TVAR_wbt_pBaseCcmS[56] // 													  
		{0x0F12, 0xFF36},  //FE9F	//TVAR_wbt_pBaseCcmS[57] // 													  
		{0x0F12, 0x0028},  //010F	//TVAR_wbt_pBaseCcmS[58] // 													  
		{0x0F12, 0xFF46},  //FEF5	//TVAR_wbt_pBaseCcmS[59] // 													  
		{0x0F12, 0x0000},  //FFD2	//TVAR_wbt_pBaseCcmS[60] // 													  
		{0x0F12, 0x0032},  //0015	//TVAR_wbt_pBaseCcmS[61] // 													  
		{0x0F12, 0x0156},  //01A1	//TVAR_wbt_pBaseCcmS[62] // 													  
		{0x0F12, 0x00F8},  //0111	//TVAR_wbt_pBaseCcmS[63] // 													  
		{0x0F12, 0x0091},  //009D	//TVAR_wbt_pBaseCcmS[64] // 													  
		{0x0F12, 0xFEF0},  //FECB	//TVAR_wbt_pBaseCcmS[65] // 													  
		{0x0F12, 0x0165},  //01FC	//TVAR_wbt_pBaseCcmS[66] // 													  
		{0x0F12, 0xFF34},  //FF99	//TVAR_wbt_pBaseCcmS[67] // 													  
		{0x0F12, 0x0119},  //01A9	//TVAR_wbt_pBaseCcmS[68] // 													  
		{0x0F12, 0xFFA9},  //FF26	//TVAR_wbt_pBaseCcmS[69] // 													  
		{0x0F12, 0x00DB},  //012B	//TVAR_wbt_pBaseCcmS[70] // 													  
		{0x0F12, 0x00AD},  //00DF	//TVAR_wbt_pBaseCcmS[71] //    
			  
		{0x0F12, 0x01DD},  //01E2	//TVAR_wbt_pBaseCcmS[72] // 												
		{0x0F12, 0xFFA5},  //FF9A	//TVAR_wbt_pBaseCcmS[73] // 												
		{0x0F12, 0x0001},  //FFE7	//TVAR_wbt_pBaseCcmS[74] // 												
		{0x0F12, 0xFF21},  //FE9F	//TVAR_wbt_pBaseCcmS[75] // 												
		{0x0F12, 0x002A},  //010F	//TVAR_wbt_pBaseCcmS[76] // 												
		{0x0F12, 0xFF53},  //FEF5	//TVAR_wbt_pBaseCcmS[77] // 												
		{0x0F12, 0x0025},  //FFD2	//TVAR_wbt_pBaseCcmS[78] // 												
		{0x0F12, 0xFFFC},  //FFFE	//TVAR_wbt_pBaseCcmS[79] // 												
		{0x0F12, 0x0163},  //01B7	//TVAR_wbt_pBaseCcmS[80] // 												
		{0x0F12, 0x00B5},  //00E8	//TVAR_wbt_pBaseCcmS[81] // 												
		{0x0F12, 0x00CA},  //0095	//TVAR_wbt_pBaseCcmS[82] // 												
		{0x0F12, 0xFEF7},  //FF0D	//TVAR_wbt_pBaseCcmS[83] // 												
		{0x0F12, 0x010D},  //0182	//TVAR_wbt_pBaseCcmS[84] // 												
		{0x0F12, 0xFF40},  //FF29	//TVAR_wbt_pBaseCcmS[85] // 												
		{0x0F12, 0x0123},  //0146	//TVAR_wbt_pBaseCcmS[86] // 												
		{0x0F12, 0xFF8F},  //FF26	//TVAR_wbt_pBaseCcmS[87] // 												
		{0x0F12, 0x00BB},  //012B	//TVAR_wbt_pBaseCcmS[88] // 												
		{0x0F12, 0x00E3},  //00DF	//TVAR_wbt_pBaseCcmS[89] // 																		
		
		{0x0F12, 0x0218},  //01E2	//TVAR_wbt_pBaseCcmS[90] // 												
		{0x0F12, 0xFF7F},  //FF9A	//TVAR_wbt_pBaseCcmS[91] // 												
		{0x0F12, 0xFFF6},  //FFE7	//TVAR_wbt_pBaseCcmS[92] // 												
		{0x0F12, 0xFEFD},  //FE9F	//TVAR_wbt_pBaseCcmS[93] // 												
		{0x0F12, 0x006D},  //010F	//TVAR_wbt_pBaseCcmS[94] // 												
		{0x0F12, 0xFF3C},  //FEF5	//TVAR_wbt_pBaseCcmS[95] // 												
		{0x0F12, 0x000F},  //FFD2	//TVAR_wbt_pBaseCcmS[96] // 												
		{0x0F12, 0xFFE6},  //FFFE	//TVAR_wbt_pBaseCcmS[97] // 												
		{0x0F12, 0x0199},  //01B7	//TVAR_wbt_pBaseCcmS[98] // 												
		{0x0F12, 0x00D6},  //00E8	//TVAR_wbt_pBaseCcmS[99] // 												
		{0x0F12, 0x00E3},  //0095	//TVAR_wbt_pBaseCcmS[100] //												
		{0x0F12, 0xFEC5},  //FF0D	//TVAR_wbt_pBaseCcmS[101] //												
		{0x0F12, 0x0222},  //0182	//TVAR_wbt_pBaseCcmS[102] //												
		{0x0F12, 0xFF27},  //FF29	//TVAR_wbt_pBaseCcmS[103] //												
		{0x0F12, 0x01A8},  //0146	//TVAR_wbt_pBaseCcmS[104] //												
		{0x0F12, 0xFF14},  //FF26	//TVAR_wbt_pBaseCcmS[105] //												
		{0x0F12, 0x00E2},  //012B	//TVAR_wbt_pBaseCcmS[106] //												
		{0x0F12, 0x013F},  //00DF	//TVAR_wbt_pBaseCcmS[107] //												
											   
		{0x002A, 0x3380},  //12시최적	  
		{0x0F12, 0x01CB},  //0223	//0223	//01F3	//01F3	//TVAR_wbt_pOutdoorCcm[0] //  
		{0x0F12, 0xFFAF},  //FF7C	//FF7C	//FFA4	//FFA4	//TVAR_wbt_pOutdoorCcm[1] //  
		{0x0F12, 0xFFEA},  //FFC5	//FFC5	//FFE4	//FFE4	//TVAR_wbt_pOutdoorCcm[2] //  
		{0x0F12, 0xFEEA},  //FE3D	//FE3D	//FE3D	//FE23	//TVAR_wbt_pOutdoorCcm[3] //  
		{0x0F12, 0x00F2},  //0158	//0158	//0158	//017D	//TVAR_wbt_pOutdoorCcm[4] //  
		{0x0F12, 0xFEBC},  //FF03	//FF03	//FF03	//FEF9	//TVAR_wbt_pOutdoorCcm[5] //  
		{0x0F12, 0x0007},  //FF9F	//FF9F	//FF9F	//FF9F	//TVAR_wbt_pOutdoorCcm[6] //  
		{0x0F12, 0xFFD6},  //0011	//0011	//0011	//0011	//TVAR_wbt_pOutdoorCcm[7] //  
		{0x0F12, 0x01F2},  //0237	//0237	//0237	//0237	//TVAR_wbt_pOutdoorCcm[8] //  
		{0x0F12, 0x0101},  //00EB	//00D1	//012A	//0143	//TVAR_wbt_pOutdoorCcm[9] //  
		{0x0F12, 0x0116},  //012A	//0125	//00CA	//00F6	//TVAR_wbt_pOutdoorCcm[10] // 
		{0x0F12, 0xFF00},  //FF02	//FEF5	//FEF6	//FEB1	//TVAR_wbt_pOutdoorCcm[11] // 
		{0x0F12, 0x01C5},  //01C5	//01C5	//01C5	//01C5	//TVAR_wbt_pOutdoorCcm[12] // 
		{0x0F12, 0xFF80},  //FF80	//FF80	//FF80	//FF80	//TVAR_wbt_pOutdoorCcm[13] // 
		{0x0F12, 0x019D},  //019D	//019D	//019D	//019D	//TVAR_wbt_pOutdoorCcm[14] // 
		{0x0F12, 0xFFB5},  //FE7A	//FE7A	//FE7A	//FE7A	//TVAR_wbt_pOutdoorCcm[15] // 
		{0x0F12, 0x00FA},  //0179	//0179	//0179	//0179	//TVAR_wbt_pOutdoorCcm[16] // 
		{0x0F12, 0x00BE},  //0179	//0179	//0179	//0179	//TVAR_wbt_pOutdoorCcm[17] // 
						   
								
								
		{0x002A, 0x0764},		
		{0x0F12, 0x0049},	//afit_uNoiseIndInDoor[0] //																		 
		{0x0F12, 0x005F},	//afit_uNoiseIndInDoor[1] //																		 
		{0x0F12, 0x00CB},	//afit_uNoiseIndInDoor[2] // 203//																	 
		{0x0F12, 0x01E0},	//afit_uNoiseIndInDoor[3] // Indoor_NB below 1500 _Noise index 300-400d //							 
		{0x0F12, 0x0220},	//afit_uNoiseIndInDoor[4] // DNP NB 4600 _ Noisenidex :560d-230h // 								 
								
								
		{0x002A, 0x07C4},		
		{0x0F12, 0x0034},	//700007C4 //TVAR_afit_pBaseValS[0] // AFIT16_BRIGHTNESS																				  
		{0x0F12, 0x0000},	//700007C6 //TVAR_afit_pBaseValS[1] // AFIT16_CONTRAST																					  
		{0x0F12, 0x0020},	//700007C8 //TVAR_afit_pBaseValS[2] // AFIT16_SATURATION																				  
		{0x0F12, 0xFFD6},	//700007CA //TVAR_afit_pBaseValS[3] // AFIT16_SHARP_BLUR																				  
		{0x0F12, 0x0000},	//700007CC //TVAR_afit_pBaseValS[4] // AFIT16_GLAMOUR																					  
		{0x0F12, 0x00C1},	//700007CE //TVAR_afit_pBaseValS[5] // AFIT16_sddd8a_edge_high																			  
		{0x0F12, 0x03FF},	//700007D0 //TVAR_afit_pBaseValS[6] // AFIT16_Demosaicing_iSatVal																		  
		{0x0F12, 0x009C},	//700007D2 //TVAR_afit_pBaseValS[7] // AFIT16_Sharpening_iReduceEdgeThresh																  
		{0x0F12, 0x0251},	//700007D4 //TVAR_afit_pBaseValS[8] // AFIT16_demsharpmix1_iRGBOffset																	  
		{0x0F12, 0x03FF},	//700007D6 //TVAR_afit_pBaseValS[9] // AFIT16_demsharpmix1_iDemClamp																	  
		{0x0F12, 0x000C},	//700007D8 //TVAR_afit_pBaseValS[10] //AFIT16_demsharpmix1_iLowThreshold																  
		{0x0F12, 0x0010},	//700007DA //TVAR_afit_pBaseValS[11] //AFIT16_demsharpmix1_iHighThreshold																  
		{0x0F12, 0x012C},	//700007DC //TVAR_afit_pBaseValS[12] //AFIT16_demsharpmix1_iLowBright																	  
		{0x0F12, 0x03E8},	//700007DE //TVAR_afit_pBaseValS[13] //AFIT16_demsharpmix1_iHighBright																	  
		{0x0F12, 0x0046},	//700007E0 //TVAR_afit_pBaseValS[14] //AFIT16_demsharpmix1_iLowSat																		  
		{0x0F12, 0x005A},	//700007E2 //TVAR_afit_pBaseValS[15] //AFIT16_demsharpmix1_iHighSat 																	  
		{0x0F12, 0x0070},	//700007E4 //TVAR_afit_pBaseValS[16] //AFIT16_demsharpmix1_iTune																		  
		{0x0F12, 0x0000},	//700007E6 //TVAR_afit_pBaseValS[17] //AFIT16_demsharpmix1_iHystThLow																	  
		{0x0F12, 0x0000},	//700007E8 //TVAR_afit_pBaseValS[18] //AFIT16_demsharpmix1_iHystThHigh																	  
		{0x0F12, 0x01AA},	//700007EA //TVAR_afit_pBaseValS[19] //AFIT16_demsharpmix1_iHystCenter																	  
		{0x0F12, 0x003C},	//700007EC //TVAR_afit_pBaseValS[20] //AFIT16_YUV422_DENOISE_iUVLowThresh																  
		{0x0F12, 0x003C},	//700007EE //TVAR_afit_pBaseValS[21] //AFIT16_YUV422_DENOISE_iUVHighThresh																  
		{0x0F12, 0x0000},	//700007F0 //TVAR_afit_pBaseValS[22] //AFIT16_YUV422_DENOISE_iYLowThresh																  
		{0x0F12, 0x0000},	//700007F2 //TVAR_afit_pBaseValS[23] //AFIT16_YUV422_DENOISE_iYHighThresh																  
		{0x0F12, 0x003E},	//700007F4 //TVAR_afit_pBaseValS[24] //AFIT16_Sharpening_iLowSharpClamp 																  
		{0x0F12, 0x0008},	//700007F6 //TVAR_afit_pBaseValS[25] //AFIT16_Sharpening_iHighSharpClamp																  
		{0x0F12, 0x003C},	//700007F8 //TVAR_afit_pBaseValS[26] //AFIT16_Sharpening_iLowSharpClamp_Bin 															  
		{0x0F12, 0x001E},	//700007FA //TVAR_afit_pBaseValS[27] //AFIT16_Sharpening_iHighSharpClamp_Bin															  
		{0x0F12, 0x003C},	//700007FC //TVAR_afit_pBaseValS[28] //AFIT16_Sharpening_iLowSharpClamp_sBin															  
		{0x0F12, 0x001E},	//700007FE //TVAR_afit_pBaseValS[29] //AFIT16_Sharpening_iHighSharpClamp_sBin															  
		{0x0F12, 0x0A24},	//70000800 //TVAR_afit_pBaseValS[30] //AFIT8_sddd8a_edge_low [7:0],   AFIT8_sddd8a_repl_thresh [15:8]									  
		{0x0F12, 0x1701},	//70000802 //TVAR_afit_pBaseValS[31] //AFIT8_sddd8a_repl_force [7:0],  AFIT8_sddd8a_sat_level [15:8]									  
		{0x0F12, 0x0229},	//70000804 //TVAR_afit_pBaseValS[32] //AFIT8_sddd8a_sat_thr[7:0],  AFIT8_sddd8a_sat_mpl [15:8]											  
		{0x0F12, 0x1403},	//70000806 //TVAR_afit_pBaseValS[33] //AFIT8_sddd8a_sat_noise[7:0],  AFIT8_sddd8a_iMaxSlopeAllowed [15:8]								  
		{0x0F12, 0x0000},	//70000808 //TVAR_afit_pBaseValS[34] //AFIT8_sddd8a_iHotThreshHigh[7:0],  AFIT8_sddd8a_iHotThreshLow [15:8] 							  
		{0x0F12, 0x0000},	//7000080A //TVAR_afit_pBaseValS[35] //AFIT8_sddd8a_iColdThreshHigh[7:0],  AFIT8_sddd8a_iColdThreshLow [15:8]							  
		{0x0F12, 0x0000},	//7000080C //TVAR_afit_pBaseValS[36] //AFIT8_sddd8a_AddNoisePower1[7:0],  AFIT8_sddd8a_AddNoisePower2 [15:8]							  
		{0x0F12, 0x00FF},	//7000080E //TVAR_afit_pBaseValS[37] //AFIT8_sddd8a_iSatSat[7:0],	AFIT8_sddd8a_iRadialTune [15:8] 									  
		{0x0F12, 0x045A},	//70000810 //TVAR_afit_pBaseValS[38] //AFIT8_sddd8a_iRadialLimit [7:0],   AFIT8_sddd8a_iRadialPower [15:8]								  
		{0x0F12, 0x1414},	//70000812 //TVAR_afit_pBaseValS[39] //AFIT8_sddd8a_iLowMaxSlopeAllowed [7:0],	AFIT8_sddd8a_iHighMaxSlopeAllowed [15:8]				  
		{0x0F12, 0x0301},	//70000814 //TVAR_afit_pBaseValS[40] //AFIT8_sddd8a_iLowSlopeThresh[7:0],	AFIT8_sddd8a_iHighSlopeThresh [15:8]						  
		{0x0F12, 0xFF07},	//70000816 //TVAR_afit_pBaseValS[41] //AFIT8_sddd8a_iSquaresRounding [7:0],   AFIT8_Demosaicing_iCentGrad [15:8]						  
		{0x0F12, 0x081E},	//70000818 //TVAR_afit_pBaseValS[42] //AFIT8_Demosaicing_iMonochrom [7:0],	 AFIT8_Demosaicing_iDecisionThresh [15:8]					  
		{0x0F12, 0x0A14},	//7000081A //TVAR_afit_pBaseValS[43] //AFIT8_Demosaicing_iDesatThresh [7:0],   AFIT8_Demosaicing_iEnhThresh [15:8]						  
		{0x0F12, 0x0F0F},	//7000081C //TVAR_afit_pBaseValS[44] //AFIT8_Demosaicing_iGRDenoiseVal [7:0],	AFIT8_Demosaicing_iGBDenoiseVal [15:8]					  
		{0x0F12, 0x0A00},	//7000081E //TVAR_afit_pBaseValS[45] //AFIT8_Demosaicing_iNearGrayDesat[7:0],	AFIT8_Demosaicing_iDFD_ReduceCoeff [15:8]				  
		{0x0F12, 0x0032},	//70000820 //TVAR_afit_pBaseValS[46] //AFIT8_Sharpening_iMSharpen [7:0],   AFIT8_Sharpening_iMShThresh [15:8]							  
		{0x0F12, 0x000E},	//70000822 //TVAR_afit_pBaseValS[47] //AFIT8_Sharpening_iWSharpen [7:0],   AFIT8_Sharpening_iWShThresh [15:8]							  
		{0x0F12, 0x0002},	//70000824 //TVAR_afit_pBaseValS[48] //AFIT8_Sharpening_nSharpWidth [7:0],	 AFIT8_Sharpening_iReduceNegative [15:8]					  
		{0x0F12, 0x00FF},	//70000826 //TVAR_afit_pBaseValS[49] //AFIT8_Sharpening_iShDespeckle [7:0],  AFIT8_demsharpmix1_iRGBMultiplier [15:8]					  
		{0x0F12, 0x1102},	//70000828 //TVAR_afit_pBaseValS[50] //AFIT8_demsharpmix1_iFilterPower [7:0],  AFIT8_demsharpmix1_iBCoeff [15:8]						  
		{0x0F12, 0x001B},	//7000082A //TVAR_afit_pBaseValS[51] //AFIT8_demsharpmix1_iGCoeff [7:0],   AFIT8_demsharpmix1_iWideMult [15:8]							  
		{0x0F12, 0x0900},	//7000082C //TVAR_afit_pBaseValS[52] //AFIT8_demsharpmix1_iNarrMult [7:0],	 AFIT8_demsharpmix1_iHystFalloff [15:8] 					  
		{0x0F12, 0x0600},	//7000082E //TVAR_afit_pBaseValS[53] //AFIT8_demsharpmix1_iHystMinMult [7:0],	AFIT8_demsharpmix1_iHystWidth [15:8]					  
		{0x0F12, 0x0504},	//70000830 //TVAR_afit_pBaseValS[54] //AFIT8_demsharpmix1_iHystFallLow [7:0],	AFIT8_demsharpmix1_iHystFallHigh [15:8] 				  
		{0x0F12, 0x0306},	//70000832 //TVAR_afit_pBaseValS[55] //AFIT8_demsharpmix1_iHystTune [7:0],	* AFIT8_YUV422_DENOISE_iUVSupport [15:8]					  
		{0x0F12, 0x4603},	//70000834 //TVAR_afit_pBaseValS[56] //AFIT8_YUV422_DENOISE_iYSupport [7:0],   AFIT8_byr_cgras_iShadingPower [15:8] 					  
		{0x0F12, 0x0480},	//70000836 //TVAR_afit_pBaseValS[57] //AFIT8_RGBGamma2_iLinearity [7:0],  AFIT8_RGBGamma2_iDarkReduce [15:8]							  
		{0x0F12, 0x1080},	//70000838 //TVAR_afit_pBaseValS[58] //AFIT8_ccm_oscar_iSaturation[7:0],   AFIT8_RGB2YUV_iYOffset [15:8]								  
		{0x0F12, 0x0080},	//7000083A //TVAR_afit_pBaseValS[59] //AFIT8_RGB2YUV_iRGBGain [7:0],   AFIT8_RGB2YUV_iSaturation [15:8] 								  
		{0x0F12, 0x0101},	//7000083C //TVAR_afit_pBaseValS[60] //AFIT8_sddd8a_iClustThresh_H [7:0],  AFIT8_sddd8a_iClustThresh_C [15:8]							  
		{0x0F12, 0x0707},	//7000083E //TVAR_afit_pBaseValS[61] //AFIT8_sddd8a_iClustMulT_H [7:0],   AFIT8_sddd8a_iClustMulT_C [15:8]								  
		{0x0F12, 0x4601},	//70000840 //TVAR_afit_pBaseValS[62] //AFIT8_sddd8a_nClustLevel_H [7:0],   AFIT8_sddd8a_DispTH_Low [15:8]								  
		{0x0F12, 0x8144},	//70000842 //TVAR_afit_pBaseValS[63] //AFIT8_sddd8a_DispTH_High [7:0],	 AFIT8_sddd8a_iDenThreshLow [15:8]								  
		{0x0F12, 0x5058},	//70000844 //TVAR_afit_pBaseValS[64] //AFIT8_sddd8a_iDenThreshHigh[7:0],   AFIT8_Demosaicing_iEdgeDesat [15:8]							  
		{0x0F12, 0x0500},	//70000846 //TVAR_afit_pBaseValS[65] //AFIT8_Demosaicing_iEdgeDesatThrLow [7:0],   AFIT8_Demosaicing_iEdgeDesatThrHigh [15:8]			  
		{0x0F12, 0x0003},	//70000848 //TVAR_afit_pBaseValS[66] //AFIT8_Demosaicing_iEdgeDesatLimit[7:0],	AFIT8_Demosaicing_iDemSharpenLow [15:8] 				  
		{0x0F12, 0x5400},	//7000084A //TVAR_afit_pBaseValS[67] //AFIT8_Demosaicing_iDemSharpenHigh[7:0],	 AFIT8_Demosaicing_iDemSharpThresh [15:8]				  
		{0x0F12, 0x0714},	//7000084C //TVAR_afit_pBaseValS[68] //AFIT8_Demosaicing_iDemShLowLimit [7:0],	 AFIT8_Demosaicing_iDespeckleForDemsharp [15:8] 		  
		{0x0F12, 0x32FF},	//7000084E //TVAR_afit_pBaseValS[69] //AFIT8_Demosaicing_iDemBlurLow[7:0],	 AFIT8_Demosaicing_iDemBlurHigh [15:8]						  
		{0x0F12, 0x5A04},	//70000850 //TVAR_afit_pBaseValS[70] //AFIT8_Demosaicing_iDemBlurRange[7:0],   AFIT8_Sharpening_iLowSharpPower [15:8]					  
		{0x0F12, 0x201E},	//70000852 //TVAR_afit_pBaseValS[71] //AFIT8_Sharpening_iHighSharpPower[7:0],	AFIT8_Sharpening_iLowShDenoise [15:8]					  
		{0x0F12, 0x4012},	//70000854 //TVAR_afit_pBaseValS[72] //AFIT8_Sharpening_iHighShDenoise [7:0],	AFIT8_Sharpening_iReduceEdgeMinMult [15:8]				  
		{0x0F12, 0x0204},	//70000856 //TVAR_afit_pBaseValS[73] //AFIT8_Sharpening_iReduceEdgeSlope [7:0],  AFIT8_demsharpmix1_iWideFiltReduce [15:8]				  
		{0x0F12, 0x1403},	//70000858 //TVAR_afit_pBaseValS[74] //AFIT8_demsharpmix1_iNarrFiltReduce [7:0],  AFIT8_sddd8a_iClustThresh_H_Bin [15:8]				  
		{0x0F12, 0x0114},	//7000085A //TVAR_afit_pBaseValS[75] //AFIT8_sddd8a_iClustThresh_C_Bin [7:0],	AFIT8_sddd8a_iClustMulT_H_Bin [15:8]					  
		{0x0F12, 0x0101},	//7000085C //TVAR_afit_pBaseValS[76] //AFIT8_sddd8a_iClustMulT_C_Bin [7:0],   AFIT8_sddd8a_nClustLevel_H_Bin [15:8] 					  
		{0x0F12, 0x4446},	//7000085E //TVAR_afit_pBaseValS[77] //AFIT8_sddd8a_DispTH_Low_Bin [7:0],	AFIT8_sddd8a_DispTH_High_Bin [15:8] 						  
		{0x0F12, 0x646E},	//70000860 //TVAR_afit_pBaseValS[78] //AFIT8_sddd8a_iDenThreshLow_Bin [7:0],   AFIT8_sddd8a_iDenThreshHigh_Bin [15:8]					  
		{0x0F12, 0x0028},	//70000862 //TVAR_afit_pBaseValS[79] //AFIT8_Demosaicing_iEdgeDesat_Bin[7:0],	AFIT8_Demosaicing_iEdgeDesatThrLow_Bin [15:8]			  
		{0x0F12, 0x030A},	//70000864 //TVAR_afit_pBaseValS[80] //AFIT8_Demosaicing_iEdgeDesatThrHigh_Bin [7:0],  AFIT8_Demosaicing_iEdgeDesatLimit_Bin [15:8] 	  
		{0x0F12, 0x0000},	//70000866 //TVAR_afit_pBaseValS[81] //AFIT8_Demosaicing_iDemSharpenLow_Bin [7:0],	AFIT8_Demosaicing_iDemSharpenHigh_Bin [15:8]		  
		{0x0F12, 0x141E},	//70000868 //TVAR_afit_pBaseValS[82] //AFIT8_Demosaicing_iDemSharpThresh_Bin [7:0],  AFIT8_Demosaicing_iDemShLowLimit_Bin [15:8]		  
		{0x0F12, 0xFF07},	//7000086A //TVAR_afit_pBaseValS[83] //AFIT8_Demosaicing_iDespeckleForDemsharp_Bin [7:0],  AFIT8_Demosaicing_iDemBlurLow_Bin [15:8] 	  
		{0x0F12, 0x0432},	//7000086C //TVAR_afit_pBaseValS[84] //AFIT8_Demosaicing_iDemBlurHigh_Bin [7:0],  AFIT8_Demosaicing_iDemBlurRange_Bin [15:8]			  
		{0x0F12, 0x0000},	//7000086E //TVAR_afit_pBaseValS[85] //AFIT8_Sharpening_iLowSharpPower_Bin [7:0],  AFIT8_Sharpening_iHighSharpPower_Bin [15:8]			  
		{0x0F12, 0x0F0F},	//70000870 //TVAR_afit_pBaseValS[86] //AFIT8_Sharpening_iLowShDenoise_Bin [7:0],  AFIT8_Sharpening_iHighShDenoise_Bin [15:8]			  
		{0x0F12, 0x0440},	//70000872 //TVAR_afit_pBaseValS[87] //AFIT8_Sharpening_iReduceEdgeMinMult_Bin [7:0],  AFIT8_Sharpening_iReduceEdgeSlope_Bin [15:8] 	  
		{0x0F12, 0x0302},	//70000874 //TVAR_afit_pBaseValS[88] //AFIT8_demsharpmix1_iWideFiltReduce_Bin [7:0],  AFIT8_demsharpmix1_iNarrFiltReduce_Bin [15:8] 	  
		{0x0F12, 0x1414},	//70000876 //TVAR_afit_pBaseValS[89] //AFIT8_sddd8a_iClustThresh_H_sBin[7:0],	AFIT8_sddd8a_iClustThresh_C_sBin [15:8] 				  
		{0x0F12, 0x0101},	//70000878 //TVAR_afit_pBaseValS[90] //AFIT8_sddd8a_iClustMulT_H_sBin [7:0],   AFIT8_sddd8a_iClustMulT_C_sBin [15:8]					  
		{0x0F12, 0x4601},	//7000087A //TVAR_afit_pBaseValS[91] //AFIT8_sddd8a_nClustLevel_H_sBin [7:0],	AFIT8_sddd8a_DispTH_Low_sBin [15:8] 					  
		{0x0F12, 0x6E44},	//7000087C //TVAR_afit_pBaseValS[92] //AFIT8_sddd8a_DispTH_High_sBin [7:0],   AFIT8_sddd8a_iDenThreshLow_sBin [15:8]					  
		{0x0F12, 0x2864},	//7000087E //TVAR_afit_pBaseValS[93] //AFIT8_sddd8a_iDenThreshHigh_sBin[7:0],	AFIT8_Demosaicing_iEdgeDesat_sBin [15:8]				  
		{0x0F12, 0x0A00},	//70000880 //TVAR_afit_pBaseValS[94] //AFIT8_Demosaicing_iEdgeDesatThrLow_sBin [7:0],  AFIT8_Demosaicing_iEdgeDesatThrHigh_sBin [15:8]	  
		{0x0F12, 0x0003},	//70000882 //TVAR_afit_pBaseValS[95] //AFIT8_Demosaicing_iEdgeDesatLimit_sBin [7:0],  AFIT8_Demosaicing_iDemSharpenLow_sBin [15:8]		  
		{0x0F12, 0x1E00},	//70000884 //TVAR_afit_pBaseValS[96] //AFIT8_Demosaicing_iDemSharpenHigh_sBin [7:0],  AFIT8_Demosaicing_iDemSharpThresh_sBin [15:8] 	  
		{0x0F12, 0x0714},	//70000886 //TVAR_afit_pBaseValS[97] //AFIT8_Demosaicing_iDemShLowLimit_sBin [7:0],  AFIT8_Demosaicing_iDespeckleForDemsharp_sBin [15:8]  
		{0x0F12, 0x32FF},	//70000888 //TVAR_afit_pBaseValS[98] //AFIT8_Demosaicing_iDemBlurLow_sBin [7:0],  AFIT8_Demosaicing_iDemBlurHigh_sBin [15:8]			  
		{0x0F12, 0x0004},	//7000088A //TVAR_afit_pBaseValS[99] //AFIT8_Demosaicing_iDemBlurRange_sBin [7:0],	AFIT8_Sharpening_iLowSharpPower_sBin [15:8] 		  
		{0x0F12, 0x0F00},	//7000088C //TVAR_afit_pBaseValS[100] /AFIT8_Sharpening_iHighSharpPower_sBin [7:0],  AFIT8_Sharpening_iLowShDenoise_sBin [15:8] 		 /
		{0x0F12, 0x400F},	//7000088E //TVAR_afit_pBaseValS[101] /AFIT8_Sharpening_iHighShDenoise_sBin [7:0],	AFIT8_Sharpening_iReduceEdgeMinMult_sBin [15:8] 	 /
		{0x0F12, 0x0204},	//70000890 //TVAR_afit_pBaseValS[102] /AFIT8_Sharpening_iReduceEdgeSlope_sBin [7:0],  AFIT8_demsharpmix1_iWideFiltReduce_sBin [15:8]	 /
		{0x0F12, 0x0003},	//70000892 //TVAR_afit_pBaseValS[103] /AFIT8_demsharpmix1_iNarrFiltReduce_sBin [7:0]													 /
		{0x0F12, 0x0000},	//70000894 //TVAR_afit_pBaseValS[104] /AFIT16_BRIGHTNESS																				 /
		{0x0F12, 0x0000},	//70000896 //TVAR_afit_pBaseValS[105] /AFIT16_CONTRAST																					 /
		{0x0F12, 0x0020},	//70000898 //TVAR_afit_pBaseValS[106] /AFIT16_SATURATION																				 /
		{0x0F12, 0xFFD6},	//7000089A //TVAR_afit_pBaseValS[107] /AFIT16_SHARP_BLUR																				 /
		{0x0F12, 0x0000},	//7000089C //TVAR_afit_pBaseValS[108] /AFIT16_GLAMOUR																					 /
		{0x0F12, 0x00C1},	//7000089E //TVAR_afit_pBaseValS[109] /AFIT16_sddd8a_edge_high																			 /
		{0x0F12, 0x03FF},	//700008A0 //TVAR_afit_pBaseValS[110] /AFIT16_Demosaicing_iSatVal																		 /
		{0x0F12, 0x009C},	//700008A2 //TVAR_afit_pBaseValS[111] /AFIT16_Sharpening_iReduceEdgeThresh																 /
		{0x0F12, 0x0251},	//700008A4 //TVAR_afit_pBaseValS[112] /AFIT16_demsharpmix1_iRGBOffset																	 /
		{0x0F12, 0x03FF},	//700008A6 //TVAR_afit_pBaseValS[113] /AFIT16_demsharpmix1_iDemClamp																	 /
		{0x0F12, 0x000C},	//700008A8 //TVAR_afit_pBaseValS[114] /AFIT16_demsharpmix1_iLowThreshold																 /
		{0x0F12, 0x0010},	//700008AA //TVAR_afit_pBaseValS[115] /AFIT16_demsharpmix1_iHighThreshold																 /
		{0x0F12, 0x012C},	//700008AC //TVAR_afit_pBaseValS[116] /AFIT16_demsharpmix1_iLowBright																	 /
		{0x0F12, 0x03E8},	//700008AE //TVAR_afit_pBaseValS[117] /AFIT16_demsharpmix1_iHighBright																	 /
		{0x0F12, 0x0046},	//700008B0 //TVAR_afit_pBaseValS[118] /AFIT16_demsharpmix1_iLowSat																		 /
		{0x0F12, 0x005A},	//700008B2 //TVAR_afit_pBaseValS[119] /AFIT16_demsharpmix1_iHighSat 																	 /
		{0x0F12, 0x0070},	//700008B4 //TVAR_afit_pBaseValS[120] /AFIT16_demsharpmix1_iTune																		 /
		{0x0F12, 0x0000},	//700008B6 //TVAR_afit_pBaseValS[121] /AFIT16_demsharpmix1_iHystThLow																	 /
		{0x0F12, 0x0000},	//700008B8 //TVAR_afit_pBaseValS[122] /AFIT16_demsharpmix1_iHystThHigh																	 /
		{0x0F12, 0x01AE},	//700008BA //TVAR_afit_pBaseValS[123] /AFIT16_demsharpmix1_iHystCenter																	 /
		{0x0F12, 0x001E},	//700008BC //TVAR_afit_pBaseValS[124] /AFIT16_YUV422_DENOISE_iUVLowThresh																 /
		{0x0F12, 0x001E},	//700008BE //TVAR_afit_pBaseValS[125] /AFIT16_YUV422_DENOISE_iUVHighThresh																 /
		{0x0F12, 0x0000},	//700008C0 //TVAR_afit_pBaseValS[126] /AFIT16_YUV422_DENOISE_iYLowThresh																 /
		{0x0F12, 0x0000},	//700008C2 //TVAR_afit_pBaseValS[127] /AFIT16_YUV422_DENOISE_iYHighThresh																 /
		{0x0F12, 0x003E},	//700008C4 //TVAR_afit_pBaseValS[128] /AFIT16_Sharpening_iLowSharpClamp 																 /
		{0x0F12, 0x0008},	//700008C6 //TVAR_afit_pBaseValS[129] /AFIT16_Sharpening_iHighSharpClamp																 /
		{0x0F12, 0x003C},	//700008C8 //TVAR_afit_pBaseValS[130] /AFIT16_Sharpening_iLowSharpClamp_Bin 															 /
		{0x0F12, 0x001E},	//700008CA //TVAR_afit_pBaseValS[131] /AFIT16_Sharpening_iHighSharpClamp_Bin															 /
		{0x0F12, 0x003C},	//700008CC //TVAR_afit_pBaseValS[132] /AFIT16_Sharpening_iLowSharpClamp_sBin															 /
		{0x0F12, 0x001E},	//700008CE //TVAR_afit_pBaseValS[133] /AFIT16_Sharpening_iHighSharpClamp_sBin															 /
		{0x0F12, 0x0A24},	//700008D0 //TVAR_afit_pBaseValS[134] /AFIT8_sddd8a_edge_low [7:0],   AFIT8_sddd8a_repl_thresh [15:8]									 /
		{0x0F12, 0x1701},	//700008D2 //TVAR_afit_pBaseValS[135] /AFIT8_sddd8a_repl_force [7:0],  AFIT8_sddd8a_sat_level [15:8]									 /
		{0x0F12, 0x0229},	//700008D4 //TVAR_afit_pBaseValS[136] /AFIT8_sddd8a_sat_thr[7:0],  AFIT8_sddd8a_sat_mpl [15:8]											 /
		{0x0F12, 0x1403},	//700008D6 //TVAR_afit_pBaseValS[137] /AFIT8_sddd8a_sat_noise[7:0],  AFIT8_sddd8a_iMaxSlopeAllowed [15:8]								 /
		{0x0F12, 0x0000},	//700008D8 //TVAR_afit_pBaseValS[138] /AFIT8_sddd8a_iHotThreshHigh[7:0],  AFIT8_sddd8a_iHotThreshLow [15:8] 							 /
		{0x0F12, 0x0000},	//700008DA //TVAR_afit_pBaseValS[139] /AFIT8_sddd8a_iColdThreshHigh[7:0],  AFIT8_sddd8a_iColdThreshLow [15:8]							 /
		{0x0F12, 0x0000},	//700008DC //TVAR_afit_pBaseValS[140] /AFIT8_sddd8a_AddNoisePower1[7:0],  AFIT8_sddd8a_AddNoisePower2 [15:8]							 /
		{0x0F12, 0x00FF},	//700008DE //TVAR_afit_pBaseValS[141] /AFIT8_sddd8a_iSatSat[7:0],	AFIT8_sddd8a_iRadialTune [15:8] 									 /
		{0x0F12, 0x045A},	//700008E0 //TVAR_afit_pBaseValS[142] /AFIT8_sddd8a_iRadialLimit [7:0],   AFIT8_sddd8a_iRadialPower [15:8]								 /
		{0x0F12, 0x1414},	//700008E2 //TVAR_afit_pBaseValS[143] /AFIT8_sddd8a_iLowMaxSlopeAllowed [7:0],	AFIT8_sddd8a_iHighMaxSlopeAllowed [15:8]				 /
		{0x0F12, 0x0301},	//700008E4 //TVAR_afit_pBaseValS[144] /AFIT8_sddd8a_iLowSlopeThresh[7:0],	AFIT8_sddd8a_iHighSlopeThresh [15:8]						 /
		{0x0F12, 0xFF07},	//700008E6 //TVAR_afit_pBaseValS[145] /AFIT8_sddd8a_iSquaresRounding [7:0],   AFIT8_Demosaicing_iCentGrad [15:8]						 /
		{0x0F12, 0x081E},	//700008E8 //TVAR_afit_pBaseValS[146] /AFIT8_Demosaicing_iMonochrom [7:0],	 AFIT8_Demosaicing_iDecisionThresh [15:8]					 /
		{0x0F12, 0x0A14},	//700008EA //TVAR_afit_pBaseValS[147] /AFIT8_Demosaicing_iDesatThresh [7:0],   AFIT8_Demosaicing_iEnhThresh [15:8]						 /
		{0x0F12, 0x0F0F},	//700008EC //TVAR_afit_pBaseValS[148] /AFIT8_Demosaicing_iGRDenoiseVal [7:0],	AFIT8_Demosaicing_iGBDenoiseVal [15:8]					 /
		{0x0F12, 0x0A00},	//700008EE //TVAR_afit_pBaseValS[149] /AFIT8_Demosaicing_iNearGrayDesat[7:0],	AFIT8_Demosaicing_iDFD_ReduceCoeff [15:8]				 /
		{0x0F12, 0x0032},	//700008F0 //TVAR_afit_pBaseValS[150] /AFIT8_Sharpening_iMSharpen [7:0],   AFIT8_Sharpening_iMShThresh [15:8]							 /
		{0x0F12, 0x000E},	//700008F2 //TVAR_afit_pBaseValS[151] /AFIT8_Sharpening_iWSharpen [7:0],   AFIT8_Sharpening_iWShThresh [15:8]							 /
		{0x0F12, 0x0002},	//700008F4 //TVAR_afit_pBaseValS[152] /AFIT8_Sharpening_nSharpWidth [7:0],	 AFIT8_Sharpening_iReduceNegative [15:8]					 /
		{0x0F12, 0x00FF},	//700008F6 //TVAR_afit_pBaseValS[153] /AFIT8_Sharpening_iShDespeckle [7:0],  AFIT8_demsharpmix1_iRGBMultiplier [15:8]					 /
		{0x0F12, 0x1102},	//700008F8 //TVAR_afit_pBaseValS[154] /AFIT8_demsharpmix1_iFilterPower [7:0],  AFIT8_demsharpmix1_iBCoeff [15:8]						 /
		{0x0F12, 0x001B},	//700008FA //TVAR_afit_pBaseValS[155] /AFIT8_demsharpmix1_iGCoeff [7:0],   AFIT8_demsharpmix1_iWideMult [15:8]							 /
		{0x0F12, 0x0900},	//700008FC //TVAR_afit_pBaseValS[156] /AFIT8_demsharpmix1_iNarrMult [7:0],	 AFIT8_demsharpmix1_iHystFalloff [15:8] 					 /
		{0x0F12, 0x0600},	//700008FE //TVAR_afit_pBaseValS[157] /AFIT8_demsharpmix1_iHystMinMult [7:0],	AFIT8_demsharpmix1_iHystWidth [15:8]					 /
		{0x0F12, 0x0504},	//70000900 //TVAR_afit_pBaseValS[158] /AFIT8_demsharpmix1_iHystFallLow [7:0],	AFIT8_demsharpmix1_iHystFallHigh [15:8] 				 /
		{0x0F12, 0x0306},	//70000902 //TVAR_afit_pBaseValS[159] /AFIT8_demsharpmix1_iHystTune [7:0],	* AFIT8_YUV422_DENOISE_iUVSupport [15:8]					 /
		{0x0F12, 0x4603},	//70000904 //TVAR_afit_pBaseValS[160] /AFIT8_YUV422_DENOISE_iYSupport [7:0],   AFIT8_byr_cgras_iShadingPower [15:8] 					 /
		{0x0F12, 0x0480},	//70000906 //TVAR_afit_pBaseValS[161] /AFIT8_RGBGamma2_iLinearity [7:0],  AFIT8_RGBGamma2_iDarkReduce [15:8]							 /
		{0x0F12, 0x1080},	//70000908 //TVAR_afit_pBaseValS[162] /AFIT8_ccm_oscar_iSaturation[7:0],   AFIT8_RGB2YUV_iYOffset [15:8]								 /
		{0x0F12, 0x0080},	//7000090A //TVAR_afit_pBaseValS[163] /AFIT8_RGB2YUV_iRGBGain [7:0],   AFIT8_RGB2YUV_iSaturation [15:8] 								 /
		{0x0F12, 0x0101},	//7000090C //TVAR_afit_pBaseValS[164] /AFIT8_sddd8a_iClustThresh_H [7:0],  AFIT8_sddd8a_iClustThresh_C [15:8]							 /
		{0x0F12, 0x0707},	//7000090E //TVAR_afit_pBaseValS[165] /AFIT8_sddd8a_iClustMulT_H [7:0],   AFIT8_sddd8a_iClustMulT_C [15:8]								 /
		{0x0F12, 0x1E01},	//70000910 //TVAR_afit_pBaseValS[166] /AFIT8_sddd8a_nClustLevel_H [7:0],   AFIT8_sddd8a_DispTH_Low [15:8]								 /
		{0x0F12, 0x811E},	//70000912 //TVAR_afit_pBaseValS[167] /AFIT8_sddd8a_DispTH_High [7:0],	 AFIT8_sddd8a_iDenThreshLow [15:8]								 /
		{0x0F12, 0x5058},	//70000914 //TVAR_afit_pBaseValS[168] /AFIT8_sddd8a_iDenThreshHigh[7:0],   AFIT8_Demosaicing_iEdgeDesat [15:8]							 /
		{0x0F12, 0x0500},	//70000916 //TVAR_afit_pBaseValS[169] /AFIT8_Demosaicing_iEdgeDesatThrLow [7:0],   AFIT8_Demosaicing_iEdgeDesatThrHigh [15:8]			 /
		{0x0F12, 0x0004},	//70000918 //TVAR_afit_pBaseValS[170] /AFIT8_Demosaicing_iEdgeDesatLimit[7:0],	AFIT8_Demosaicing_iDemSharpenLow [15:8] 				 /
		{0x0F12, 0x3C0A},	//7000091A //TVAR_afit_pBaseValS[171] /AFIT8_Demosaicing_iDemSharpenHigh[7:0],	 AFIT8_Demosaicing_iDemSharpThresh [15:8]				 /
		{0x0F12, 0x0714},	//7000091C //TVAR_afit_pBaseValS[172] /AFIT8_Demosaicing_iDemShLowLimit [7:0],	 AFIT8_Demosaicing_iDespeckleForDemsharp [15:8] 		 /
		{0x0F12, 0x3214},	//7000091E //TVAR_afit_pBaseValS[173] /AFIT8_Demosaicing_iDemBlurLow[7:0],	 AFIT8_Demosaicing_iDemBlurHigh [15:8]						 /
		{0x0F12, 0x5A03},	//70000920 //TVAR_afit_pBaseValS[174] /AFIT8_Demosaicing_iDemBlurRange[7:0],   AFIT8_Sharpening_iLowSharpPower [15:8]					 /
		{0x0F12, 0x121E},	//70000922 //TVAR_afit_pBaseValS[175] /AFIT8_Sharpening_iHighSharpPower[7:0],	AFIT8_Sharpening_iLowShDenoise [15:8]					 /
		{0x0F12, 0x4012},	//70000924 //TVAR_afit_pBaseValS[176] /AFIT8_Sharpening_iHighShDenoise [7:0],	AFIT8_Sharpening_iReduceEdgeMinMult [15:8]				 /
		{0x0F12, 0x0604},	//70000926 //TVAR_afit_pBaseValS[177] /AFIT8_Sharpening_iReduceEdgeSlope [7:0],  AFIT8_demsharpmix1_iWideFiltReduce [15:8]				 /
		{0x0F12, 0x1E06},	//70000928 //TVAR_afit_pBaseValS[178] /AFIT8_demsharpmix1_iNarrFiltReduce [7:0],  AFIT8_sddd8a_iClustThresh_H_Bin [15:8]				 /
		{0x0F12, 0x011E},	//7000092A //TVAR_afit_pBaseValS[179] /AFIT8_sddd8a_iClustThresh_C_Bin [7:0],	AFIT8_sddd8a_iClustMulT_H_Bin [15:8]					 /
		{0x0F12, 0x0101},	//7000092C //TVAR_afit_pBaseValS[180] /AFIT8_sddd8a_iClustMulT_C_Bin [7:0],   AFIT8_sddd8a_nClustLevel_H_Bin [15:8] 					 /
		{0x0F12, 0x3A3C},	//7000092E //TVAR_afit_pBaseValS[181] /AFIT8_sddd8a_DispTH_Low_Bin [7:0],	AFIT8_sddd8a_DispTH_High_Bin [15:8] 						 /
		{0x0F12, 0x585A},	//70000930 //TVAR_afit_pBaseValS[182] /AFIT8_sddd8a_iDenThreshLow_Bin [7:0],   AFIT8_sddd8a_iDenThreshHigh_Bin [15:8]					 /
		{0x0F12, 0x0028},	//70000932 //TVAR_afit_pBaseValS[183] /AFIT8_Demosaicing_iEdgeDesat_Bin[7:0],	AFIT8_Demosaicing_iEdgeDesatThrLow_Bin [15:8]			 /
		{0x0F12, 0x030A},	//70000934 //TVAR_afit_pBaseValS[184] /AFIT8_Demosaicing_iEdgeDesatThrHigh_Bin [7:0],  AFIT8_Demosaicing_iEdgeDesatLimit_Bin [15:8] 	 /
		{0x0F12, 0x0000},	//70000936 //TVAR_afit_pBaseValS[185] /AFIT8_Demosaicing_iDemSharpenLow_Bin [7:0],	AFIT8_Demosaicing_iDemSharpenHigh_Bin [15:8]		 /
		{0x0F12, 0x141E},	//70000938 //TVAR_afit_pBaseValS[186] /AFIT8_Demosaicing_iDemSharpThresh_Bin [7:0],  AFIT8_Demosaicing_iDemShLowLimit_Bin [15:8]		 /
		{0x0F12, 0xFF07},	//7000093A //TVAR_afit_pBaseValS[187] /AFIT8_Demosaicing_iDespeckleForDemsharp_Bin [7:0],  AFIT8_Demosaicing_iDemBlurLow_Bin [15:8] 	 /
		{0x0F12, 0x0432},	//7000093C //TVAR_afit_pBaseValS[188] /AFIT8_Demosaicing_iDemBlurHigh_Bin [7:0],  AFIT8_Demosaicing_iDemBlurRange_Bin [15:8]			 /
		{0x0F12, 0x0000},	//7000093E //TVAR_afit_pBaseValS[189] /AFIT8_Sharpening_iLowSharpPower_Bin [7:0],  AFIT8_Sharpening_iHighSharpPower_Bin [15:8]			 /
		{0x0F12, 0x0F0F},	//70000940 //TVAR_afit_pBaseValS[190] /AFIT8_Sharpening_iLowShDenoise_Bin [7:0],  AFIT8_Sharpening_iHighShDenoise_Bin [15:8]			 /
		{0x0F12, 0x0440},	//70000942 //TVAR_afit_pBaseValS[191] /AFIT8_Sharpening_iReduceEdgeMinMult_Bin [7:0],  AFIT8_Sharpening_iReduceEdgeSlope_Bin [15:8] 	 /
		{0x0F12, 0x0302},	//70000944 //TVAR_afit_pBaseValS[192] /AFIT8_demsharpmix1_iWideFiltReduce_Bin [7:0],  AFIT8_demsharpmix1_iNarrFiltReduce_Bin [15:8] 	 /
		{0x0F12, 0x1E1E},	//70000946 //TVAR_afit_pBaseValS[193] /AFIT8_sddd8a_iClustThresh_H_sBin[7:0],	AFIT8_sddd8a_iClustThresh_C_sBin [15:8] 				 /
		{0x0F12, 0x0101},	//70000948 //TVAR_afit_pBaseValS[194] /AFIT8_sddd8a_iClustMulT_H_sBin [7:0],   AFIT8_sddd8a_iClustMulT_C_sBin [15:8]					 /
		{0x0F12, 0x3C01},	//7000094A //TVAR_afit_pBaseValS[195] /AFIT8_sddd8a_nClustLevel_H_sBin [7:0],	AFIT8_sddd8a_DispTH_Low_sBin [15:8] 					 /
		{0x0F12, 0x5A3A},	//7000094C //TVAR_afit_pBaseValS[196] /AFIT8_sddd8a_DispTH_High_sBin [7:0],   AFIT8_sddd8a_iDenThreshLow_sBin [15:8]					 /
		{0x0F12, 0x2858},	//7000094E //TVAR_afit_pBaseValS[197] /AFIT8_sddd8a_iDenThreshHigh_sBin[7:0],	AFIT8_Demosaicing_iEdgeDesat_sBin [15:8]				 /
		{0x0F12, 0x0A00},	//70000950 //TVAR_afit_pBaseValS[198] /AFIT8_Demosaicing_iEdgeDesatThrLow_sBin [7:0],  AFIT8_Demosaicing_iEdgeDesatThrHigh_sBin [15:8]	 /
		{0x0F12, 0x0003},	//70000952 //TVAR_afit_pBaseValS[199] /AFIT8_Demosaicing_iEdgeDesatLimit_sBin [7:0],  AFIT8_Demosaicing_iDemSharpenLow_sBin [15:8]		 /
		{0x0F12, 0x1E00},	//70000954 //TVAR_afit_pBaseValS[200] /AFIT8_Demosaicing_iDemSharpenHigh_sBin [7:0],  AFIT8_Demosaicing_iDemSharpThresh_sBin [15:8] 	 /
		{0x0F12, 0x0714},	//70000956 //TVAR_afit_pBaseValS[201] /AFIT8_Demosaicing_iDemShLowLimit_sBin [7:0],  AFIT8_Demosaicing_iDespeckleForDemsharp_sBin [15:8] /
		{0x0F12, 0x32FF},	//70000958 //TVAR_afit_pBaseValS[202] /AFIT8_Demosaicing_iDemBlurLow_sBin [7:0],  AFIT8_Demosaicing_iDemBlurHigh_sBin [15:8]			 /
		{0x0F12, 0x0004},	//7000095A //TVAR_afit_pBaseValS[203] /AFIT8_Demosaicing_iDemBlurRange_sBin [7:0],	AFIT8_Sharpening_iLowSharpPower_sBin [15:8] 		 /
		{0x0F12, 0x0F00},	//7000095C //TVAR_afit_pBaseValS[204] /AFIT8_Sharpening_iHighSharpPower_sBin [7:0],  AFIT8_Sharpening_iLowShDenoise_sBin [15:8] 		 /
		{0x0F12, 0x400F},	//7000095E //TVAR_afit_pBaseValS[205] /AFIT8_Sharpening_iHighShDenoise_sBin [7:0],	AFIT8_Sharpening_iReduceEdgeMinMult_sBin [15:8] 	 /
		{0x0F12, 0x0204},	//70000960 //TVAR_afit_pBaseValS[206] /AFIT8_Sharpening_iReduceEdgeSlope_sBin [7:0],  AFIT8_demsharpmix1_iWideFiltReduce_sBin [15:8]	 /
		{0x0F12, 0x0003},	//70000962 //TVAR_afit_pBaseValS[207] /AFIT8_demsharpmix1_iNarrFiltReduce_sBin [7:0]													 /
		{0x0F12, 0x0000},	//70000964 //TVAR_afit_pBaseValS[208] /AFIT16_BRIGHTNESS																				 /
		{0x0F12, 0x0000},	//70000966 //TVAR_afit_pBaseValS[209] /AFIT16_CONTRAST																					 /
		{0x0F12, 0x0020},	//70000968 //TVAR_afit_pBaseValS[210] /AFIT16_SATURATION																				 /
		{0x0F12, 0x0000},	//7000096A //TVAR_afit_pBaseValS[211] /AFIT16_SHARP_BLUR																				 /
		{0x0F12, 0x0000},	//7000096C //TVAR_afit_pBaseValS[212] /AFIT16_GLAMOUR																					 /
		{0x0F12, 0x00C1},	//7000096E //TVAR_afit_pBaseValS[213] /AFIT16_sddd8a_edge_high																			 /
		{0x0F12, 0x03FF},	//70000970 //TVAR_afit_pBaseValS[214] /AFIT16_Demosaicing_iSatVal																		 /
		{0x0F12, 0x009C},	//70000972 //TVAR_afit_pBaseValS[215] /AFIT16_Sharpening_iReduceEdgeThresh																 /
		{0x0F12, 0x0251},	//70000974 //TVAR_afit_pBaseValS[216] /AFIT16_demsharpmix1_iRGBOffset																	 /
		{0x0F12, 0x03FF},	//70000976 //TVAR_afit_pBaseValS[217] /AFIT16_demsharpmix1_iDemClamp																	 /
		{0x0F12, 0x000C},	//70000978 //TVAR_afit_pBaseValS[218] /AFIT16_demsharpmix1_iLowThreshold																 /
		{0x0F12, 0x0010},	//7000097A //TVAR_afit_pBaseValS[219] /AFIT16_demsharpmix1_iHighThreshold																 /
		{0x0F12, 0x012C},	//7000097C //TVAR_afit_pBaseValS[220] /AFIT16_demsharpmix1_iLowBright																	 /
		{0x0F12, 0x03E8},	//7000097E //TVAR_afit_pBaseValS[221] /AFIT16_demsharpmix1_iHighBright																	 /
		{0x0F12, 0x0046},	//70000980 //TVAR_afit_pBaseValS[222] /AFIT16_demsharpmix1_iLowSat																		 /
		{0x0F12, 0x005A},	//70000982 //TVAR_afit_pBaseValS[223] /AFIT16_demsharpmix1_iHighSat 																	 /
		{0x0F12, 0x0070},	//70000984 //TVAR_afit_pBaseValS[224] /AFIT16_demsharpmix1_iTune																		 /
		{0x0F12, 0x0000},	//70000986 //TVAR_afit_pBaseValS[225] /AFIT16_demsharpmix1_iHystThLow																	 /
		{0x0F12, 0x0000},	//70000988 //TVAR_afit_pBaseValS[226] /AFIT16_demsharpmix1_iHystThHigh																	 /
		{0x0F12, 0x0226},	//7000098A //TVAR_afit_pBaseValS[227] /AFIT16_demsharpmix1_iHystCenter																	 /
		{0x0F12, 0x001E},	//7000098C //TVAR_afit_pBaseValS[228] /AFIT16_YUV422_DENOISE_iUVLowThresh																 /
		{0x0F12, 0x001E},	//7000098E //TVAR_afit_pBaseValS[229] /AFIT16_YUV422_DENOISE_iUVHighThresh																 /
		{0x0F12, 0x0000},	//70000990 //TVAR_afit_pBaseValS[230] /AFIT16_YUV422_DENOISE_iYLowThresh																 /
		{0x0F12, 0x0000},	//70000992 //TVAR_afit_pBaseValS[231] /AFIT16_YUV422_DENOISE_iYHighThresh																 /
		{0x0F12, 0x004E},	//70000994 //TVAR_afit_pBaseValS[232] /AFIT16_Sharpening_iLowSharpClamp 																 /
		{0x0F12, 0x0000},	//70000996 //TVAR_afit_pBaseValS[233] /AFIT16_Sharpening_iHighSharpClamp																 /
		{0x0F12, 0x003C},	//70000998 //TVAR_afit_pBaseValS[234] /AFIT16_Sharpening_iLowSharpClamp_Bin 															 /
		{0x0F12, 0x001E},	//7000099A //TVAR_afit_pBaseValS[235] /AFIT16_Sharpening_iHighSharpClamp_Bin															 /
		{0x0F12, 0x003C},	//7000099C //TVAR_afit_pBaseValS[236] /AFIT16_Sharpening_iLowSharpClamp_sBin															 /
		{0x0F12, 0x001E},	//7000099E //TVAR_afit_pBaseValS[237] /AFIT16_Sharpening_iHighSharpClamp_sBin															 /
		{0x0F12, 0x0A24},	//700009A0 //TVAR_afit_pBaseValS[238] /AFIT8_sddd8a_edge_low [7:0],   AFIT8_sddd8a_repl_thresh [15:8]									 /
		{0x0F12, 0x1701},	//700009A2 //TVAR_afit_pBaseValS[239] /AFIT8_sddd8a_repl_force [7:0],  AFIT8_sddd8a_sat_level [15:8]									 /
		{0x0F12, 0x0229},	//700009A4 //TVAR_afit_pBaseValS[240] /AFIT8_sddd8a_sat_thr[7:0],  AFIT8_sddd8a_sat_mpl [15:8]											 /
		{0x0F12, 0x1403},	//700009A6 //TVAR_afit_pBaseValS[241] /AFIT8_sddd8a_sat_noise[7:0],  AFIT8_sddd8a_iMaxSlopeAllowed [15:8]								 /
		{0x0F12, 0x0000},	//700009A8 //TVAR_afit_pBaseValS[242] /AFIT8_sddd8a_iHotThreshHigh[7:0],  AFIT8_sddd8a_iHotThreshLow [15:8] 							 /
		{0x0F12, 0x0000},	//700009AA //TVAR_afit_pBaseValS[243] /AFIT8_sddd8a_iColdThreshHigh[7:0],  AFIT8_sddd8a_iColdThreshLow [15:8]							 /
		{0x0F12, 0x0906},	//700009AC //TVAR_afit_pBaseValS[244] /AFIT8_sddd8a_AddNoisePower1[7:0],  AFIT8_sddd8a_AddNoisePower2 [15:8]							 /
		{0x0F12, 0x00FF},	//700009AE //TVAR_afit_pBaseValS[245] /AFIT8_sddd8a_iSatSat[7:0],	AFIT8_sddd8a_iRadialTune [15:8] 									 /
		{0x0F12, 0x045A},	//700009B0 //TVAR_afit_pBaseValS[246] /AFIT8_sddd8a_iRadialLimit [7:0],   AFIT8_sddd8a_iRadialPower [15:8]								 /
		{0x0F12, 0x1414},	//700009B2 //TVAR_afit_pBaseValS[247] /AFIT8_sddd8a_iLowMaxSlopeAllowed [7:0],	AFIT8_sddd8a_iHighMaxSlopeAllowed [15:8]				 /
		{0x0F12, 0x0301},	//700009B4 //TVAR_afit_pBaseValS[248] /AFIT8_sddd8a_iLowSlopeThresh[7:0],	AFIT8_sddd8a_iHighSlopeThresh [15:8]						 /
		{0x0F12, 0xFF07},	//700009B6 //TVAR_afit_pBaseValS[249] /AFIT8_sddd8a_iSquaresRounding [7:0],   AFIT8_Demosaicing_iCentGrad [15:8]						 /
		{0x0F12, 0x081E},	//700009B8 //TVAR_afit_pBaseValS[250] /AFIT8_Demosaicing_iMonochrom [7:0],	 AFIT8_Demosaicing_iDecisionThresh [15:8]					 /
		{0x0F12, 0x0A14},	//700009BA //TVAR_afit_pBaseValS[251] /AFIT8_Demosaicing_iDesatThresh [7:0],   AFIT8_Demosaicing_iEnhThresh [15:8]						 /
		{0x0F12, 0x0F0F},	//700009BC //TVAR_afit_pBaseValS[252] /AFIT8_Demosaicing_iGRDenoiseVal [7:0],	AFIT8_Demosaicing_iGBDenoiseVal [15:8]					 /
		{0x0F12, 0x0A00},	//700009BE //TVAR_afit_pBaseValS[253] /AFIT8_Demosaicing_iNearGrayDesat[7:0],	AFIT8_Demosaicing_iDFD_ReduceCoeff [15:8]				 /
		{0x0F12, 0x0090},	//700009C0 //TVAR_afit_pBaseValS[254] /AFIT8_Sharpening_iMSharpen [7:0],   AFIT8_Sharpening_iMShThresh [15:8]							 /
		{0x0F12, 0x000A},	//700009C2 //TVAR_afit_pBaseValS[255] /AFIT8_Sharpening_iWSharpen [7:0],   AFIT8_Sharpening_iWShThresh [15:8]							 /
		{0x0F12, 0x0002},	//700009C4 //TVAR_afit_pBaseValS[256] /AFIT8_Sharpening_nSharpWidth [7:0],	 AFIT8_Sharpening_iReduceNegative [15:8]					 /
		{0x0F12, 0x00FF},	//700009C6 //TVAR_afit_pBaseValS[257] /AFIT8_Sharpening_iShDespeckle [7:0],  AFIT8_demsharpmix1_iRGBMultiplier [15:8]					 /
		{0x0F12, 0x1102},	//700009C8 //TVAR_afit_pBaseValS[258] /AFIT8_demsharpmix1_iFilterPower [7:0],  AFIT8_demsharpmix1_iBCoeff [15:8]						 /
		{0x0F12, 0x001B},	//700009CA //TVAR_afit_pBaseValS[259] /AFIT8_demsharpmix1_iGCoeff [7:0],   AFIT8_demsharpmix1_iWideMult [15:8]							 /
		{0x0F12, 0x0900},	//700009CC //TVAR_afit_pBaseValS[260] /AFIT8_demsharpmix1_iNarrMult [7:0],	 AFIT8_demsharpmix1_iHystFalloff [15:8] 					 /
		{0x0F12, 0x0600},	//700009CE //TVAR_afit_pBaseValS[261] /AFIT8_demsharpmix1_iHystMinMult [7:0],	AFIT8_demsharpmix1_iHystWidth [15:8]					 /
		{0x0F12, 0x0504},	//700009D0 //TVAR_afit_pBaseValS[262] /AFIT8_demsharpmix1_iHystFallLow [7:0],	AFIT8_demsharpmix1_iHystFallHigh [15:8] 				 /
		{0x0F12, 0x0306},	//700009D2 //TVAR_afit_pBaseValS[263] /AFIT8_demsharpmix1_iHystTune [7:0],	* AFIT8_YUV422_DENOISE_iUVSupport [15:8]					 /
		{0x0F12, 0x4602},	//700009D4 //TVAR_afit_pBaseValS[264] /AFIT8_YUV422_DENOISE_iYSupport [7:0],   AFIT8_byr_cgras_iShadingPower [15:8] 					 /
		{0x0F12, 0x0880},	//700009D6 //TVAR_afit_pBaseValS[265] /AFIT8_RGBGamma2_iLinearity [7:0],  AFIT8_RGBGamma2_iDarkReduce [15:8]							 /
		{0x0F12, 0x0080},	//700009D8 //TVAR_afit_pBaseValS[266] /AFIT8_ccm_oscar_iSaturation[7:0],   AFIT8_RGB2YUV_iYOffset [15:8]								 /
		{0x0F12, 0x0080},	//700009DA //TVAR_afit_pBaseValS[267] /AFIT8_RGB2YUV_iRGBGain [7:0],   AFIT8_RGB2YUV_iSaturation [15:8] 								 /
		{0x0F12, 0x0101},	//700009DC //TVAR_afit_pBaseValS[268] /AFIT8_sddd8a_iClustThresh_H [7:0],  AFIT8_sddd8a_iClustThresh_C [15:8]							 /
		{0x0F12, 0x0707},	//700009DE //TVAR_afit_pBaseValS[269] /AFIT8_sddd8a_iClustMulT_H [7:0],   AFIT8_sddd8a_iClustMulT_C [15:8]								 /
		{0x0F12, 0x1E01},	//700009E0 //TVAR_afit_pBaseValS[270] /AFIT8_sddd8a_nClustLevel_H [7:0],   AFIT8_sddd8a_DispTH_Low [15:8]								 /
		{0x0F12, 0x3C1E},	//700009E2 //TVAR_afit_pBaseValS[271] /AFIT8_sddd8a_DispTH_High [7:0],	 AFIT8_sddd8a_iDenThreshLow [15:8]								 /
		{0x0F12, 0x5028},	//700009E4 //TVAR_afit_pBaseValS[272] /AFIT8_sddd8a_iDenThreshHigh[7:0],   AFIT8_Demosaicing_iEdgeDesat [15:8]							 /
		{0x0F12, 0x0500},	//700009E6 //TVAR_afit_pBaseValS[273] /AFIT8_Demosaicing_iEdgeDesatThrLow [7:0],   AFIT8_Demosaicing_iEdgeDesatThrHigh [15:8]			 /
		{0x0F12, 0x1A04},	//700009E8 //TVAR_afit_pBaseValS[274] /AFIT8_Demosaicing_iEdgeDesatLimit[7:0],	AFIT8_Demosaicing_iDemSharpenLow [15:8] 				 /
		{0x0F12, 0x280A},	//700009EA //TVAR_afit_pBaseValS[275] /AFIT8_Demosaicing_iDemSharpenHigh[7:0],	 AFIT8_Demosaicing_iDemSharpThresh [15:8]				 /
		{0x0F12, 0x080C},	//700009EC //TVAR_afit_pBaseValS[276] /AFIT8_Demosaicing_iDemShLowLimit [7:0],	 AFIT8_Demosaicing_iDespeckleForDemsharp [15:8] 		 /
		{0x0F12, 0x1414},	//700009EE //TVAR_afit_pBaseValS[277] /AFIT8_Demosaicing_iDemBlurLow[7:0],	 AFIT8_Demosaicing_iDemBlurHigh [15:8]						 /
		{0x0F12, 0x6A03},	//700009F0 //TVAR_afit_pBaseValS[278] /AFIT8_Demosaicing_iDemBlurRange[7:0],   AFIT8_Sharpening_iLowSharpPower [15:8]					 /
		{0x0F12, 0x121E},	//700009F2 //TVAR_afit_pBaseValS[279] /AFIT8_Sharpening_iHighSharpPower[7:0],	AFIT8_Sharpening_iLowShDenoise [15:8]					 /
		{0x0F12, 0x4012},	//700009F4 //TVAR_afit_pBaseValS[280] /AFIT8_Sharpening_iHighShDenoise [7:0],	AFIT8_Sharpening_iReduceEdgeMinMult [15:8]				 /
		{0x0F12, 0x0604},	//700009F6 //TVAR_afit_pBaseValS[281] /AFIT8_Sharpening_iReduceEdgeSlope [7:0],  AFIT8_demsharpmix1_iWideFiltReduce [15:8]				 /
		{0x0F12, 0x2806},	//700009F8 //TVAR_afit_pBaseValS[282] /AFIT8_demsharpmix1_iNarrFiltReduce [7:0],  AFIT8_sddd8a_iClustThresh_H_Bin [15:8]				 /
		{0x0F12, 0x0128},	//700009FA //TVAR_afit_pBaseValS[283] /AFIT8_sddd8a_iClustThresh_C_Bin [7:0],	AFIT8_sddd8a_iClustMulT_H_Bin [15:8]					 /
		{0x0F12, 0x0101},	//700009FC //TVAR_afit_pBaseValS[284] /AFIT8_sddd8a_iClustMulT_C_Bin [7:0],   AFIT8_sddd8a_nClustLevel_H_Bin [15:8] 					 /
		{0x0F12, 0x2224},	//700009FE //TVAR_afit_pBaseValS[285] /AFIT8_sddd8a_DispTH_Low_Bin [7:0],	AFIT8_sddd8a_DispTH_High_Bin [15:8] 						 /
		{0x0F12, 0x3236},	//70000A00 //TVAR_afit_pBaseValS[286] /AFIT8_sddd8a_iDenThreshLow_Bin [7:0],   AFIT8_sddd8a_iDenThreshHigh_Bin [15:8]					 /
		{0x0F12, 0x0028},	//70000A02 //TVAR_afit_pBaseValS[287] /AFIT8_Demosaicing_iEdgeDesat_Bin[7:0],	AFIT8_Demosaicing_iEdgeDesatThrLow_Bin [15:8]			 /
		{0x0F12, 0x030A},	//70000A04 //TVAR_afit_pBaseValS[288] /AFIT8_Demosaicing_iEdgeDesatThrHigh_Bin [7:0],  AFIT8_Demosaicing_iEdgeDesatLimit_Bin [15:8] 	 /
		{0x0F12, 0x0410},	//70000A06 //TVAR_afit_pBaseValS[289] /AFIT8_Demosaicing_iDemSharpenLow_Bin [7:0],	AFIT8_Demosaicing_iDemSharpenHigh_Bin [15:8]		 /
		{0x0F12, 0x141E},	//70000A08 //TVAR_afit_pBaseValS[290] /AFIT8_Demosaicing_iDemSharpThresh_Bin [7:0],  AFIT8_Demosaicing_iDemShLowLimit_Bin [15:8]		 /
		{0x0F12, 0xFF07},	//70000A0A //TVAR_afit_pBaseValS[291] /AFIT8_Demosaicing_iDespeckleForDemsharp_Bin [7:0],  AFIT8_Demosaicing_iDemBlurLow_Bin [15:8] 	 /
		{0x0F12, 0x0432},	//70000A0C //TVAR_afit_pBaseValS[292] /AFIT8_Demosaicing_iDemBlurHigh_Bin [7:0],  AFIT8_Demosaicing_iDemBlurRange_Bin [15:8]			 /
		{0x0F12, 0x4050},	//70000A0E //TVAR_afit_pBaseValS[293] /AFIT8_Sharpening_iLowSharpPower_Bin [7:0],  AFIT8_Sharpening_iHighSharpPower_Bin [15:8]			 /
		{0x0F12, 0x0F0F},	//70000A10 //TVAR_afit_pBaseValS[294] /AFIT8_Sharpening_iLowShDenoise_Bin [7:0],  AFIT8_Sharpening_iHighShDenoise_Bin [15:8]			 /
		{0x0F12, 0x0440},	//70000A12 //TVAR_afit_pBaseValS[295] /AFIT8_Sharpening_iReduceEdgeMinMult_Bin [7:0],  AFIT8_Sharpening_iReduceEdgeSlope_Bin [15:8] 	 /
		{0x0F12, 0x0302},	//70000A14 //TVAR_afit_pBaseValS[296] /AFIT8_demsharpmix1_iWideFiltReduce_Bin [7:0],  AFIT8_demsharpmix1_iNarrFiltReduce_Bin [15:8] 	 /
		{0x0F12, 0x2828},	//70000A16 //TVAR_afit_pBaseValS[297] /AFIT8_sddd8a_iClustThresh_H_sBin[7:0],	AFIT8_sddd8a_iClustThresh_C_sBin [15:8] 				 /
		{0x0F12, 0x0101},	//70000A18 //TVAR_afit_pBaseValS[298] /AFIT8_sddd8a_iClustMulT_H_sBin [7:0],   AFIT8_sddd8a_iClustMulT_C_sBin [15:8]					 /
		{0x0F12, 0x2401},	//70000A1A //TVAR_afit_pBaseValS[299] /AFIT8_sddd8a_nClustLevel_H_sBin [7:0],	AFIT8_sddd8a_DispTH_Low_sBin [15:8] 					 /
		{0x0F12, 0x3622},	//70000A1C //TVAR_afit_pBaseValS[300] /AFIT8_sddd8a_DispTH_High_sBin [7:0],   AFIT8_sddd8a_iDenThreshLow_sBin [15:8]					 /
		{0x0F12, 0x2832},	//70000A1E //TVAR_afit_pBaseValS[301] /AFIT8_sddd8a_iDenThreshHigh_sBin[7:0],	AFIT8_Demosaicing_iEdgeDesat_sBin [15:8]				 /
		{0x0F12, 0x0A00},	//70000A20 //TVAR_afit_pBaseValS[302] /AFIT8_Demosaicing_iEdgeDesatThrLow_sBin [7:0],  AFIT8_Demosaicing_iEdgeDesatThrHigh_sBin [15:8]	 /
		{0x0F12, 0x1003},	//70000A22 //TVAR_afit_pBaseValS[303] /AFIT8_Demosaicing_iEdgeDesatLimit_sBin [7:0],  AFIT8_Demosaicing_iDemSharpenLow_sBin [15:8]		 /
		{0x0F12, 0x1E04},	//70000A24 //TVAR_afit_pBaseValS[304] /AFIT8_Demosaicing_iDemSharpenHigh_sBin [7:0],  AFIT8_Demosaicing_iDemSharpThresh_sBin [15:8] 	 /
		{0x0F12, 0x0714},	//70000A26 //TVAR_afit_pBaseValS[305] /AFIT8_Demosaicing_iDemShLowLimit_sBin [7:0],  AFIT8_Demosaicing_iDespeckleForDemsharp_sBin [15:8] /
		{0x0F12, 0x32FF},	//70000A28 //TVAR_afit_pBaseValS[306] /AFIT8_Demosaicing_iDemBlurLow_sBin [7:0],  AFIT8_Demosaicing_iDemBlurHigh_sBin [15:8]			 /
		{0x0F12, 0x5004},	//70000A2A //TVAR_afit_pBaseValS[307] /AFIT8_Demosaicing_iDemBlurRange_sBin [7:0],	AFIT8_Sharpening_iLowSharpPower_sBin [15:8] 		 /
		{0x0F12, 0x0F40},	//70000A2C //TVAR_afit_pBaseValS[308] /AFIT8_Sharpening_iHighSharpPower_sBin [7:0],  AFIT8_Sharpening_iLowShDenoise_sBin [15:8] 		 /
		{0x0F12, 0x400F},	//70000A2E //TVAR_afit_pBaseValS[309] /AFIT8_Sharpening_iHighShDenoise_sBin [7:0],	AFIT8_Sharpening_iReduceEdgeMinMult_sBin [15:8] 	 /
		{0x0F12, 0x0204},	//70000A30 //TVAR_afit_pBaseValS[310] /AFIT8_Sharpening_iReduceEdgeSlope_sBin [7:0],  AFIT8_demsharpmix1_iWideFiltReduce_sBin [15:8]	 /
		{0x0F12, 0x0003},	//70000A32 //TVAR_afit_pBaseValS[311] /AFIT8_demsharpmix1_iNarrFiltReduce_sBin [7:0]													 /
		{0x0F12, 0x0000},	//70000A34 //TVAR_afit_pBaseValS[312] /AFIT16_BRIGHTNESS																				 /
		{0x0F12, 0x0000},	//70000A36 //TVAR_afit_pBaseValS[313] /AFIT16_CONTRAST																					 /
		{0x0F12, 0x0020},	//70000A38 //TVAR_afit_pBaseValS[314] /AFIT16_SATURATION																				 /
		{0x0F12, 0x0000},	//70000A3A //TVAR_afit_pBaseValS[315] /AFIT16_SHARP_BLUR																				 /
		{0x0F12, 0x0000},	//70000A3C //TVAR_afit_pBaseValS[316] /AFIT16_GLAMOUR																					 /
		{0x0F12, 0x00C1},	//70000A3E //TVAR_afit_pBaseValS[317] /AFIT16_sddd8a_edge_high																			 /
		{0x0F12, 0x03FF},	//70000A40 //TVAR_afit_pBaseValS[318] /AFIT16_Demosaicing_iSatVal																		 /
		{0x0F12, 0x009C},	//70000A42 //TVAR_afit_pBaseValS[319] /AFIT16_Sharpening_iReduceEdgeThresh																 /
		{0x0F12, 0x0251},	//70000A44 //TVAR_afit_pBaseValS[320] /AFIT16_demsharpmix1_iRGBOffset																	 /
		{0x0F12, 0x03FF},	//70000A46 //TVAR_afit_pBaseValS[321] /AFIT16_demsharpmix1_iDemClamp																	 /
		{0x0F12, 0x000C},	//70000A48 //TVAR_afit_pBaseValS[322] /AFIT16_demsharpmix1_iLowThreshold																 /
		{0x0F12, 0x0010},	//70000A4A //TVAR_afit_pBaseValS[323] /AFIT16_demsharpmix1_iHighThreshold																 /
		{0x0F12, 0x00C8},	//70000A4C //TVAR_afit_pBaseValS[324] /AFIT16_demsharpmix1_iLowBright																	 /
		{0x0F12, 0x03E8},	//70000A4E //TVAR_afit_pBaseValS[325] /AFIT16_demsharpmix1_iHighBright																	 /
		{0x0F12, 0x0046},	//70000A50 //TVAR_afit_pBaseValS[326] /AFIT16_demsharpmix1_iLowSat																		 /
		{0x0F12, 0x0050},	//70000A52 //TVAR_afit_pBaseValS[327] /AFIT16_demsharpmix1_iHighSat 																	 /
		{0x0F12, 0x0070},	//70000A54 //TVAR_afit_pBaseValS[328] /AFIT16_demsharpmix1_iTune																		 /
		{0x0F12, 0x0000},	//70000A56 //TVAR_afit_pBaseValS[329] /AFIT16_demsharpmix1_iHystThLow																	 /
		{0x0F12, 0x0000},	//70000A58 //TVAR_afit_pBaseValS[330] /AFIT16_demsharpmix1_iHystThHigh																	 /
		{0x0F12, 0x0226},	//70000A5A //TVAR_afit_pBaseValS[331] /AFIT16_demsharpmix1_iHystCenter																	 /
		{0x0F12, 0x0014},	//70000A5C //TVAR_afit_pBaseValS[332] /AFIT16_YUV422_DENOISE_iUVLowThresh																 /
		{0x0F12, 0x0014},	//70000A5E //TVAR_afit_pBaseValS[333] /AFIT16_YUV422_DENOISE_iUVHighThresh																 /
		{0x0F12, 0x0000},	//70000A60 //TVAR_afit_pBaseValS[334] /AFIT16_YUV422_DENOISE_iYLowThresh																 /
		{0x0F12, 0x0000},	//70000A62 //TVAR_afit_pBaseValS[335] /AFIT16_YUV422_DENOISE_iYHighThresh																 /
		{0x0F12, 0x004E},	//70000A64 //TVAR_afit_pBaseValS[336] /AFIT16_Sharpening_iLowSharpClamp 																 /
		{0x0F12, 0x0000},	//70000A66 //TVAR_afit_pBaseValS[337] /AFIT16_Sharpening_iHighSharpClamp																 /
		{0x0F12, 0x002D},	//70000A68 //TVAR_afit_pBaseValS[338] /AFIT16_Sharpening_iLowSharpClamp_Bin 															 /
		{0x0F12, 0x0019},	//70000A6A //TVAR_afit_pBaseValS[339] /AFIT16_Sharpening_iHighSharpClamp_Bin															 /
		{0x0F12, 0x002D},	//70000A6C //TVAR_afit_pBaseValS[340] /AFIT16_Sharpening_iLowSharpClamp_sBin															 /
		{0x0F12, 0x0019},	//70000A6E //TVAR_afit_pBaseValS[341] /AFIT16_Sharpening_iHighSharpClamp_sBin															 /
		{0x0F12, 0x0A24},	//70000A70 //TVAR_afit_pBaseValS[342] /AFIT8_sddd8a_edge_low [7:0],   AFIT8_sddd8a_repl_thresh [15:8]									 /
		{0x0F12, 0x1701},	//70000A72 //TVAR_afit_pBaseValS[343] /AFIT8_sddd8a_repl_force [7:0],  AFIT8_sddd8a_sat_level [15:8]									 /
		{0x0F12, 0x0229},	//70000A74 //TVAR_afit_pBaseValS[344] /AFIT8_sddd8a_sat_thr[7:0],  AFIT8_sddd8a_sat_mpl [15:8]											 /
		{0x0F12, 0x1403},	//70000A76 //TVAR_afit_pBaseValS[345] /AFIT8_sddd8a_sat_noise[7:0],  AFIT8_sddd8a_iMaxSlopeAllowed [15:8]								 /
		{0x0F12, 0x0000},	//70000A78 //TVAR_afit_pBaseValS[346] /AFIT8_sddd8a_iHotThreshHigh[7:0],  AFIT8_sddd8a_iHotThreshLow [15:8] 							 /
		{0x0F12, 0x0000},	//70000A7A //TVAR_afit_pBaseValS[347] /AFIT8_sddd8a_iColdThreshHigh[7:0],  AFIT8_sddd8a_iColdThreshLow [15:8]							 /
		{0x0F12, 0x0906},	//70000A7C //TVAR_afit_pBaseValS[348] /AFIT8_sddd8a_AddNoisePower1[7:0],  AFIT8_sddd8a_AddNoisePower2 [15:8]							 /
		{0x0F12, 0x00FF},	//70000A7E //TVAR_afit_pBaseValS[349] /AFIT8_sddd8a_iSatSat[7:0],	AFIT8_sddd8a_iRadialTune [15:8] 									 /
		{0x0F12, 0x045A},	//70000A80 //TVAR_afit_pBaseValS[350] /AFIT8_sddd8a_iRadialLimit [7:0],   AFIT8_sddd8a_iRadialPower [15:8]								 /
		{0x0F12, 0x1414},	//70000A82 //TVAR_afit_pBaseValS[351] /AFIT8_sddd8a_iLowMaxSlopeAllowed [7:0],	AFIT8_sddd8a_iHighMaxSlopeAllowed [15:8]				 /
		{0x0F12, 0x0301},	//70000A84 //TVAR_afit_pBaseValS[352] /AFIT8_sddd8a_iLowSlopeThresh[7:0],	AFIT8_sddd8a_iHighSlopeThresh [15:8]						 /
		{0x0F12, 0xFF07},	//70000A86 //TVAR_afit_pBaseValS[353] /AFIT8_sddd8a_iSquaresRounding [7:0],   AFIT8_Demosaicing_iCentGrad [15:8]						 /
		{0x0F12, 0x081E},	//70000A88 //TVAR_afit_pBaseValS[354] /AFIT8_Demosaicing_iMonochrom [7:0],	 AFIT8_Demosaicing_iDecisionThresh [15:8]					 /
		{0x0F12, 0x0A14},	//70000A8A //TVAR_afit_pBaseValS[355] /AFIT8_Demosaicing_iDesatThresh [7:0],   AFIT8_Demosaicing_iEnhThresh [15:8]						 /
		{0x0F12, 0x0F0F},	//70000A8C //TVAR_afit_pBaseValS[356] /AFIT8_Demosaicing_iGRDenoiseVal [7:0],	AFIT8_Demosaicing_iGBDenoiseVal [15:8]					 /
		{0x0F12, 0x0A01},	//70000A8E //TVAR_afit_pBaseValS[357] /AFIT8_Demosaicing_iNearGrayDesat[7:0],	AFIT8_Demosaicing_iDFD_ReduceCoeff [15:8]				 /
		{0x0F12, 0x0090},	//70000A90 //TVAR_afit_pBaseValS[358] /AFIT8_Sharpening_iMSharpen [7:0],   AFIT8_Sharpening_iMShThresh [15:8]							 /
		{0x0F12, 0x000A},	//70000A92 //TVAR_afit_pBaseValS[359] /AFIT8_Sharpening_iWSharpen [7:0],   AFIT8_Sharpening_iWShThresh [15:8]							 /
		{0x0F12, 0x0001},	//70000A94 //TVAR_afit_pBaseValS[360] /AFIT8_Sharpening_nSharpWidth [7:0],	 AFIT8_Sharpening_iReduceNegative [15:8]					 /
		{0x0F12, 0x00FF},	//70000A96 //TVAR_afit_pBaseValS[361] /AFIT8_Sharpening_iShDespeckle [7:0],  AFIT8_demsharpmix1_iRGBMultiplier [15:8]					 /
		{0x0F12, 0x1002},	//70000A98 //TVAR_afit_pBaseValS[362] /AFIT8_demsharpmix1_iFilterPower [7:0],  AFIT8_demsharpmix1_iBCoeff [15:8]						 /
		{0x0F12, 0x001E},	//70000A9A //TVAR_afit_pBaseValS[363] /AFIT8_demsharpmix1_iGCoeff [7:0],   AFIT8_demsharpmix1_iWideMult [15:8]							 /
		{0x0F12, 0x0900},	//70000A9C //TVAR_afit_pBaseValS[364] /AFIT8_demsharpmix1_iNarrMult [7:0],	 AFIT8_demsharpmix1_iHystFalloff [15:8] 					 /
		{0x0F12, 0x0600},	//70000A9E //TVAR_afit_pBaseValS[365] /AFIT8_demsharpmix1_iHystMinMult [7:0],	AFIT8_demsharpmix1_iHystWidth [15:8]					 /
		{0x0F12, 0x0504},	//70000AA0 //TVAR_afit_pBaseValS[366] /AFIT8_demsharpmix1_iHystFallLow [7:0],	AFIT8_demsharpmix1_iHystFallHigh [15:8] 				 /
		{0x0F12, 0x0307},	//70000AA2 //TVAR_afit_pBaseValS[367] /AFIT8_demsharpmix1_iHystTune [7:0],	* AFIT8_YUV422_DENOISE_iUVSupport [15:8]					 /
		{0x0F12, 0x5002},	//70000AA4 //TVAR_afit_pBaseValS[368] /AFIT8_YUV422_DENOISE_iYSupport [7:0],   AFIT8_byr_cgras_iShadingPower [15:8] 					 /
		{0x0F12, 0x0080},	//70000AA6 //TVAR_afit_pBaseValS[369] /AFIT8_RGBGamma2_iLinearity [7:0],  AFIT8_RGBGamma2_iDarkReduce [15:8]							 /
		{0x0F12, 0x0080},	//70000AA8 //TVAR_afit_pBaseValS[370] /AFIT8_ccm_oscar_iSaturation[7:0],   AFIT8_RGB2YUV_iYOffset [15:8]								 /
		{0x0F12, 0x0080},	//70000AAA //TVAR_afit_pBaseValS[371] /AFIT8_RGB2YUV_iRGBGain [7:0],   AFIT8_RGB2YUV_iSaturation [15:8] 								 /
		{0x0F12, 0x0101},	//70000AAC //TVAR_afit_pBaseValS[372] /AFIT8_sddd8a_iClustThresh_H [7:0],  AFIT8_sddd8a_iClustThresh_C [15:8]							 /
		{0x0F12, 0x0707},	//70000AAE //TVAR_afit_pBaseValS[373] /AFIT8_sddd8a_iClustMulT_H [7:0],   AFIT8_sddd8a_iClustMulT_C [15:8]								 /
		{0x0F12, 0x1E01},	//70000AB0 //TVAR_afit_pBaseValS[374] /AFIT8_sddd8a_nClustLevel_H [7:0],   AFIT8_sddd8a_DispTH_Low [15:8]								 /
		{0x0F12, 0x2A1E},	//70000AB2 //TVAR_afit_pBaseValS[375] /AFIT8_sddd8a_DispTH_High [7:0],	 AFIT8_sddd8a_iDenThreshLow [15:8]								 /
		{0x0F12, 0x5020},	//70000AB4 //TVAR_afit_pBaseValS[376] /AFIT8_sddd8a_iDenThreshHigh[7:0],   AFIT8_Demosaicing_iEdgeDesat [15:8]							 /
		{0x0F12, 0x0500},	//70000AB6 //TVAR_afit_pBaseValS[377] /AFIT8_Demosaicing_iEdgeDesatThrLow [7:0],   AFIT8_Demosaicing_iEdgeDesatThrHigh [15:8]			 /
		{0x0F12, 0x1A04},	//70000AB8 //TVAR_afit_pBaseValS[378] /AFIT8_Demosaicing_iEdgeDesatLimit[7:0],	AFIT8_Demosaicing_iDemSharpenLow [15:8] 				 /
		{0x0F12, 0x280A},	//70000ABA //TVAR_afit_pBaseValS[379] /AFIT8_Demosaicing_iDemSharpenHigh[7:0],	 AFIT8_Demosaicing_iDemSharpThresh [15:8]				 /
		{0x0F12, 0x080C},	//70000ABC //TVAR_afit_pBaseValS[380] /AFIT8_Demosaicing_iDemShLowLimit [7:0],	 AFIT8_Demosaicing_iDespeckleForDemsharp [15:8] 		 /
		{0x0F12, 0x1414},	//70000ABE //TVAR_afit_pBaseValS[381] /AFIT8_Demosaicing_iDemBlurLow[7:0],	 AFIT8_Demosaicing_iDemBlurHigh [15:8]						 /
		{0x0F12, 0x6A03},	//70000AC0 //TVAR_afit_pBaseValS[382] /AFIT8_Demosaicing_iDemBlurRange[7:0],   AFIT8_Sharpening_iLowSharpPower [15:8]					 /
		{0x0F12, 0x121E},	//70000AC2 //TVAR_afit_pBaseValS[383] /AFIT8_Sharpening_iHighSharpPower[7:0],	AFIT8_Sharpening_iLowShDenoise [15:8]					 /
		{0x0F12, 0x4012},	//70000AC4 //TVAR_afit_pBaseValS[384] /AFIT8_Sharpening_iHighShDenoise [7:0],	AFIT8_Sharpening_iReduceEdgeMinMult [15:8]				 /
		{0x0F12, 0x0604},	//70000AC6 //TVAR_afit_pBaseValS[385] /AFIT8_Sharpening_iReduceEdgeSlope [7:0],  AFIT8_demsharpmix1_iWideFiltReduce [15:8]				 /
		{0x0F12, 0x3C06},	//70000AC8 //TVAR_afit_pBaseValS[386] /AFIT8_demsharpmix1_iNarrFiltReduce [7:0],  AFIT8_sddd8a_iClustThresh_H_Bin [15:8]				 /
		{0x0F12, 0x013C},	//70000ACA //TVAR_afit_pBaseValS[387] /AFIT8_sddd8a_iClustThresh_C_Bin [7:0],	AFIT8_sddd8a_iClustMulT_H_Bin [15:8]					 /
		{0x0F12, 0x0101},	//70000ACC //TVAR_afit_pBaseValS[388] /AFIT8_sddd8a_iClustMulT_C_Bin [7:0],   AFIT8_sddd8a_nClustLevel_H_Bin [15:8] 					 /
		{0x0F12, 0x1C1E},	//70000ACE //TVAR_afit_pBaseValS[389] /AFIT8_sddd8a_DispTH_Low_Bin [7:0],	AFIT8_sddd8a_DispTH_High_Bin [15:8] 						 /
		{0x0F12, 0x1E22},	//70000AD0 //TVAR_afit_pBaseValS[390] /AFIT8_sddd8a_iDenThreshLow_Bin [7:0],   AFIT8_sddd8a_iDenThreshHigh_Bin [15:8]					 /
		{0x0F12, 0x0028},	//70000AD2 //TVAR_afit_pBaseValS[391] /AFIT8_Demosaicing_iEdgeDesat_Bin[7:0],	AFIT8_Demosaicing_iEdgeDesatThrLow_Bin [15:8]			 /
		{0x0F12, 0x030A},	//70000AD4 //TVAR_afit_pBaseValS[392] /AFIT8_Demosaicing_iEdgeDesatThrHigh_Bin [7:0],  AFIT8_Demosaicing_iEdgeDesatLimit_Bin [15:8] 	 /
		{0x0F12, 0x0214},	//70000AD6 //TVAR_afit_pBaseValS[393] /AFIT8_Demosaicing_iDemSharpenLow_Bin [7:0],	AFIT8_Demosaicing_iDemSharpenHigh_Bin [15:8]		 /
		{0x0F12, 0x0E14},	//70000AD8 //TVAR_afit_pBaseValS[394] /AFIT8_Demosaicing_iDemSharpThresh_Bin [7:0],  AFIT8_Demosaicing_iDemShLowLimit_Bin [15:8]		 /
		{0x0F12, 0xFF06},	//70000ADA //TVAR_afit_pBaseValS[395] /AFIT8_Demosaicing_iDespeckleForDemsharp_Bin [7:0],  AFIT8_Demosaicing_iDemBlurLow_Bin [15:8] 	 /
		{0x0F12, 0x0432},	//70000ADC //TVAR_afit_pBaseValS[396] /AFIT8_Demosaicing_iDemBlurHigh_Bin [7:0],  AFIT8_Demosaicing_iDemBlurRange_Bin [15:8]			 /
		{0x0F12, 0x4052},	//70000ADE //TVAR_afit_pBaseValS[397] /AFIT8_Sharpening_iLowSharpPower_Bin [7:0],  AFIT8_Sharpening_iHighSharpPower_Bin [15:8]			 /
		{0x0F12, 0x150C},	//70000AE0 //TVAR_afit_pBaseValS[398] /AFIT8_Sharpening_iLowShDenoise_Bin [7:0],  AFIT8_Sharpening_iHighShDenoise_Bin [15:8]			 /
		{0x0F12, 0x0440},	//70000AE2 //TVAR_afit_pBaseValS[399] /AFIT8_Sharpening_iReduceEdgeMinMult_Bin [7:0],  AFIT8_Sharpening_iReduceEdgeSlope_Bin [15:8] 	 /
		{0x0F12, 0x0302},	//70000AE4 //TVAR_afit_pBaseValS[400] /AFIT8_demsharpmix1_iWideFiltReduce_Bin [7:0],  AFIT8_demsharpmix1_iNarrFiltReduce_Bin [15:8] 	 /
		{0x0F12, 0x3C3C},	//70000AE6 //TVAR_afit_pBaseValS[401] /AFIT8_sddd8a_iClustThresh_H_sBin[7:0],	AFIT8_sddd8a_iClustThresh_C_sBin [15:8] 				 /
		{0x0F12, 0x0101},	//70000AE8 //TVAR_afit_pBaseValS[402] /AFIT8_sddd8a_iClustMulT_H_sBin [7:0],   AFIT8_sddd8a_iClustMulT_C_sBin [15:8]					 /
		{0x0F12, 0x1E01},	//70000AEA //TVAR_afit_pBaseValS[403] /AFIT8_sddd8a_nClustLevel_H_sBin [7:0],	AFIT8_sddd8a_DispTH_Low_sBin [15:8] 					 /
		{0x0F12, 0x221C},	//70000AEC //TVAR_afit_pBaseValS[404] /AFIT8_sddd8a_DispTH_High_sBin [7:0],   AFIT8_sddd8a_iDenThreshLow_sBin [15:8]					 /
		{0x0F12, 0x281E},	//70000AEE //TVAR_afit_pBaseValS[405] /AFIT8_sddd8a_iDenThreshHigh_sBin[7:0],	AFIT8_Demosaicing_iEdgeDesat_sBin [15:8]				 /
		{0x0F12, 0x0A00},	//70000AF0 //TVAR_afit_pBaseValS[406] /AFIT8_Demosaicing_iEdgeDesatThrLow_sBin [7:0],  AFIT8_Demosaicing_iEdgeDesatThrHigh_sBin [15:8]	 /
		{0x0F12, 0x1403},	//70000AF2 //TVAR_afit_pBaseValS[407] /AFIT8_Demosaicing_iEdgeDesatLimit_sBin [7:0],  AFIT8_Demosaicing_iDemSharpenLow_sBin [15:8]		 /
		{0x0F12, 0x1402},	//70000AF4 //TVAR_afit_pBaseValS[408] /AFIT8_Demosaicing_iDemSharpenHigh_sBin [7:0],  AFIT8_Demosaicing_iDemSharpThresh_sBin [15:8] 	 /
		{0x0F12, 0x060E},	//70000AF6 //TVAR_afit_pBaseValS[409] /AFIT8_Demosaicing_iDemShLowLimit_sBin [7:0],  AFIT8_Demosaicing_iDespeckleForDemsharp_sBin [15:8] /
		{0x0F12, 0x32FF},	//70000AF8 //TVAR_afit_pBaseValS[410] /AFIT8_Demosaicing_iDemBlurLow_sBin [7:0],  AFIT8_Demosaicing_iDemBlurHigh_sBin [15:8]			 /
		{0x0F12, 0x5204},	//70000AFA //TVAR_afit_pBaseValS[411] /AFIT8_Demosaicing_iDemBlurRange_sBin [7:0],	AFIT8_Sharpening_iLowSharpPower_sBin [15:8] 		 /
		{0x0F12, 0x0C40},	//70000AFC //TVAR_afit_pBaseValS[412] /AFIT8_Sharpening_iHighSharpPower_sBin [7:0],  AFIT8_Sharpening_iLowShDenoise_sBin [15:8] 		 /
		{0x0F12, 0x4015},	//70000AFE //TVAR_afit_pBaseValS[413] /AFIT8_Sharpening_iHighShDenoise_sBin [7:0],	AFIT8_Sharpening_iReduceEdgeMinMult_sBin [15:8] 	 /
		{0x0F12, 0x0204},	//70000B00 //TVAR_afit_pBaseValS[414] /AFIT8_Sharpening_iReduceEdgeSlope_sBin [7:0],  AFIT8_demsharpmix1_iWideFiltReduce_sBin [15:8]	 /
		{0x0F12, 0x0003},	//70000B02 //TVAR_afit_pBaseValS[415] /AFIT8_demsharpmix1_iNarrFiltReduce_sBin [7:0]													 /
		{0x0F12, 0x0000},	//70000B04 //TVAR_afit_pBaseValS[416] /AFIT16_BRIGHTNESS																				 /
		{0x0F12, 0x0000},	//70000B06 //TVAR_afit_pBaseValS[417] /AFIT16_CONTRAST																					 /
		{0x0F12, 0x0020},	//70000B08 //TVAR_afit_pBaseValS[418] /AFIT16_SATURATION																				 /
		{0x0F12, 0x0000},	//70000B0A //TVAR_afit_pBaseValS[419] /AFIT16_SHARP_BLUR																				 /
		{0x0F12, 0x0000},	//70000B0C //TVAR_afit_pBaseValS[420] /AFIT16_GLAMOUR																					 /
		{0x0F12, 0x00C1},	//70000B0E //TVAR_afit_pBaseValS[421] /AFIT16_sddd8a_edge_high																			 /
		{0x0F12, 0x03FF},	//70000B10 //TVAR_afit_pBaseValS[422] /AFIT16_Demosaicing_iSatVal																		 /
		{0x0F12, 0x009C},	//70000B12 //TVAR_afit_pBaseValS[423] /AFIT16_Sharpening_iReduceEdgeThresh																 /
		{0x0F12, 0x0251},	//70000B14 //TVAR_afit_pBaseValS[424] /AFIT16_demsharpmix1_iRGBOffset																	 /
		{0x0F12, 0x03FF},	//70000B16 //TVAR_afit_pBaseValS[425] /AFIT16_demsharpmix1_iDemClamp																	 /
		{0x0F12, 0x000C},	//70000B18 //TVAR_afit_pBaseValS[426] /AFIT16_demsharpmix1_iLowThreshold																 /
		{0x0F12, 0x0010},	//70000B1A //TVAR_afit_pBaseValS[427] /AFIT16_demsharpmix1_iHighThreshold																 /
		{0x0F12, 0x0032},	//70000B1C //TVAR_afit_pBaseValS[428] /AFIT16_demsharpmix1_iLowBright																	 /
		{0x0F12, 0x028A},	//70000B1E //TVAR_afit_pBaseValS[429] /AFIT16_demsharpmix1_iHighBright																	 /
		{0x0F12, 0x0032},	//70000B20 //TVAR_afit_pBaseValS[430] /AFIT16_demsharpmix1_iLowSat																		 /
		{0x0F12, 0x01F4},	//70000B22 //TVAR_afit_pBaseValS[431] /AFIT16_demsharpmix1_iHighSat 																	 /
		{0x0F12, 0x0070},	//70000B24 //TVAR_afit_pBaseValS[432] /AFIT16_demsharpmix1_iTune																		 /
		{0x0F12, 0x0000},	//70000B26 //TVAR_afit_pBaseValS[433] /AFIT16_demsharpmix1_iHystThLow																	 /
		{0x0F12, 0x0000},	//70000B28 //TVAR_afit_pBaseValS[434] /AFIT16_demsharpmix1_iHystThHigh																	 /
		{0x0F12, 0x01AA},	//70000B2A //TVAR_afit_pBaseValS[435] /AFIT16_demsharpmix1_iHystCenter																	 /
		{0x0F12, 0x003C},	//70000B2C //TVAR_afit_pBaseValS[436] /AFIT16_YUV422_DENOISE_iUVLowThresh																 /
		{0x0F12, 0x0050},	//70000B2E //TVAR_afit_pBaseValS[437] /AFIT16_YUV422_DENOISE_iUVHighThresh																 /
		{0x0F12, 0x0000},	//70000B30 //TVAR_afit_pBaseValS[438] /AFIT16_YUV422_DENOISE_iYLowThresh																 /
		{0x0F12, 0x0000},	//70000B32 //TVAR_afit_pBaseValS[439] /AFIT16_YUV422_DENOISE_iYHighThresh																 /
		{0x0F12, 0x0044},	//70000B34 //TVAR_afit_pBaseValS[440] /AFIT16_Sharpening_iLowSharpClamp 																 /
		{0x0F12, 0x0014},	//70000B36 //TVAR_afit_pBaseValS[441] /AFIT16_Sharpening_iHighSharpClamp																 /
		{0x0F12, 0x0046},	//70000B38 //TVAR_afit_pBaseValS[442] /AFIT16_Sharpening_iLowSharpClamp_Bin 															 /
		{0x0F12, 0x0019},	//70000B3A //TVAR_afit_pBaseValS[443] /AFIT16_Sharpening_iHighSharpClamp_Bin															 /
		{0x0F12, 0x0046},	//70000B3C //TVAR_afit_pBaseValS[444] /AFIT16_Sharpening_iLowSharpClamp_sBin															 /
		{0x0F12, 0x0019},	//70000B3E //TVAR_afit_pBaseValS[445] /AFIT16_Sharpening_iHighSharpClamp_sBin															 /
		{0x0F12, 0x0A24},	//70000B40 //TVAR_afit_pBaseValS[446] /AFIT8_sddd8a_edge_low [7:0],   AFIT8_sddd8a_repl_thresh [15:8]									 /
		{0x0F12, 0x1701},	//70000B42 //TVAR_afit_pBaseValS[447] /AFIT8_sddd8a_repl_force [7:0],  AFIT8_sddd8a_sat_level [15:8]									 /
		{0x0F12, 0x0229},	//70000B44 //TVAR_afit_pBaseValS[448] /AFIT8_sddd8a_sat_thr[7:0],  AFIT8_sddd8a_sat_mpl [15:8]											 /
		{0x0F12, 0x0503},	//70000B46 //TVAR_afit_pBaseValS[449] /AFIT8_sddd8a_sat_noise[7:0],  AFIT8_sddd8a_iMaxSlopeAllowed [15:8]								 /
		{0x0F12, 0x080F},	//70000B48 //TVAR_afit_pBaseValS[450] /AFIT8_sddd8a_iHotThreshHigh[7:0],  AFIT8_sddd8a_iHotThreshLow [15:8] 							 /
		{0x0F12, 0x0808},	//70000B4A //TVAR_afit_pBaseValS[451] /AFIT8_sddd8a_iColdThreshHigh[7:0],  AFIT8_sddd8a_iColdThreshLow [15:8]							 /
		{0x0F12, 0x0000},	//70000B4C //TVAR_afit_pBaseValS[452] /AFIT8_sddd8a_AddNoisePower1[7:0],  AFIT8_sddd8a_AddNoisePower2 [15:8]							 /
		{0x0F12, 0x00FF},	//70000B4E //TVAR_afit_pBaseValS[453] /AFIT8_sddd8a_iSatSat[7:0],	AFIT8_sddd8a_iRadialTune [15:8] 									 /
		{0x0F12, 0x012D},	//70000B50 //TVAR_afit_pBaseValS[454] /AFIT8_sddd8a_iRadialLimit [7:0],   AFIT8_sddd8a_iRadialPower [15:8]								 /
		{0x0F12, 0x1414},	//70000B52 //TVAR_afit_pBaseValS[455] /AFIT8_sddd8a_iLowMaxSlopeAllowed [7:0],	AFIT8_sddd8a_iHighMaxSlopeAllowed [15:8]				 /
		{0x0F12, 0x0301},	//70000B54 //TVAR_afit_pBaseValS[456] /AFIT8_sddd8a_iLowSlopeThresh[7:0],	AFIT8_sddd8a_iHighSlopeThresh [15:8]						 /
		{0x0F12, 0xFF07},	//70000B56 //TVAR_afit_pBaseValS[457] /AFIT8_sddd8a_iSquaresRounding [7:0],   AFIT8_Demosaicing_iCentGrad [15:8]						 /
		{0x0F12, 0x061E},	//70000B58 //TVAR_afit_pBaseValS[458] /AFIT8_Demosaicing_iMonochrom [7:0],	 AFIT8_Demosaicing_iDecisionThresh [15:8]					 /
		{0x0F12, 0x0A1E},	//70000B5A //TVAR_afit_pBaseValS[459] /AFIT8_Demosaicing_iDesatThresh [7:0],   AFIT8_Demosaicing_iEnhThresh [15:8]						 /
		{0x0F12, 0x0606},	//70000B5C //TVAR_afit_pBaseValS[460] /AFIT8_Demosaicing_iGRDenoiseVal [7:0],	AFIT8_Demosaicing_iGBDenoiseVal [15:8]					 /
		{0x0F12, 0x0A03},	//70000B5E //TVAR_afit_pBaseValS[461] /AFIT8_Demosaicing_iNearGrayDesat[7:0],	AFIT8_Demosaicing_iDFD_ReduceCoeff [15:8]				 /
		{0x0F12, 0x378B},	//70000B60 //TVAR_afit_pBaseValS[462] /AFIT8_Sharpening_iMSharpen [7:0],   AFIT8_Sharpening_iMShThresh [15:8]							 /
		{0x0F12, 0x1028},	//70000B62 //TVAR_afit_pBaseValS[463] /AFIT8_Sharpening_iWSharpen [7:0],   AFIT8_Sharpening_iWShThresh [15:8]							 /
		{0x0F12, 0x0001},	//70000B64 //TVAR_afit_pBaseValS[464] /AFIT8_Sharpening_nSharpWidth [7:0],	 AFIT8_Sharpening_iReduceNegative [15:8]					 /
		{0x0F12, 0x00FF},	//70000B66 //TVAR_afit_pBaseValS[465] /AFIT8_Sharpening_iShDespeckle [7:0],  AFIT8_demsharpmix1_iRGBMultiplier [15:8]					 /
		{0x0F12, 0x1002},	//70000B68 //TVAR_afit_pBaseValS[466] /AFIT8_demsharpmix1_iFilterPower [7:0],  AFIT8_demsharpmix1_iBCoeff [15:8]						 /
		{0x0F12, 0x001E},	//70000B6A //TVAR_afit_pBaseValS[467] /AFIT8_demsharpmix1_iGCoeff [7:0],   AFIT8_demsharpmix1_iWideMult [15:8]							 /
		{0x0F12, 0x0900},	//70000B6C //TVAR_afit_pBaseValS[468] /AFIT8_demsharpmix1_iNarrMult [7:0],	 AFIT8_demsharpmix1_iHystFalloff [15:8] 					 /
		{0x0F12, 0x0600},	//70000B6E //TVAR_afit_pBaseValS[469] /AFIT8_demsharpmix1_iHystMinMult [7:0],	AFIT8_demsharpmix1_iHystWidth [15:8]					 /
		{0x0F12, 0x0504},	//70000B70 //TVAR_afit_pBaseValS[470] /AFIT8_demsharpmix1_iHystFallLow [7:0],	AFIT8_demsharpmix1_iHystFallHigh [15:8] 				 /
		{0x0F12, 0x0307},	//70000B72 //TVAR_afit_pBaseValS[471] /AFIT8_demsharpmix1_iHystTune [7:0],	* AFIT8_YUV422_DENOISE_iUVSupport [15:8]					 /
		{0x0F12, 0x5001},	//70000B74 //TVAR_afit_pBaseValS[472] /AFIT8_YUV422_DENOISE_iYSupport [7:0],   AFIT8_byr_cgras_iShadingPower [15:8] 					 /
		{0x0F12, 0x0080},	//70000B76 //TVAR_afit_pBaseValS[473] /AFIT8_RGBGamma2_iLinearity [7:0],  AFIT8_RGBGamma2_iDarkReduce [15:8]							 /
		{0x0F12, 0x0080},	//70000B78 //TVAR_afit_pBaseValS[474] /AFIT8_ccm_oscar_iSaturation[7:0],   AFIT8_RGB2YUV_iYOffset [15:8]								 /
		{0x0F12, 0x0080},	//70000B7A //TVAR_afit_pBaseValS[475] /AFIT8_RGB2YUV_iRGBGain [7:0],   AFIT8_RGB2YUV_iSaturation [15:8] 								 /
		{0x0F12, 0x5050},	//70000B7C //TVAR_afit_pBaseValS[476] /AFIT8_sddd8a_iClustThresh_H [7:0],  AFIT8_sddd8a_iClustThresh_C [15:8]							 /
		{0x0F12, 0x0101},	//70000B7E //TVAR_afit_pBaseValS[477] /AFIT8_sddd8a_iClustMulT_H [7:0],   AFIT8_sddd8a_iClustMulT_C [15:8]								 /
		{0x0F12, 0x3201},	//70000B80 //TVAR_afit_pBaseValS[478] /AFIT8_sddd8a_nClustLevel_H [7:0],   AFIT8_sddd8a_DispTH_Low [15:8]								 /
		{0x0F12, 0x1832},	//70000B82 //TVAR_afit_pBaseValS[479] /AFIT8_sddd8a_DispTH_High [7:0],	 AFIT8_sddd8a_iDenThreshLow [15:8]								 /		  iden 12
		{0x0F12, 0x210C},	//70000B84 //TVAR_afit_pBaseValS[480] /AFIT8_sddd8a_iDenThreshHigh[7:0],   AFIT8_Demosaicing_iEdgeDesat [15:8]							 /
		{0x0F12, 0x0A00},	//70000B86 //TVAR_afit_pBaseValS[481] /AFIT8_Demosaicing_iEdgeDesatThrLow [7:0],   AFIT8_Demosaicing_iEdgeDesatThrHigh [15:8]			 /
		{0x0F12, 0x1E04},	//70000B88 //TVAR_afit_pBaseValS[482] /AFIT8_Demosaicing_iEdgeDesatLimit[7:0],	AFIT8_Demosaicing_iDemSharpenLow [15:8] 				 /	  1E demhsharplow
		{0x0F12, 0x0A08},	//70000B8A //TVAR_afit_pBaseValS[483] /AFIT8_Demosaicing_iDemSharpenHigh[7:0],	 AFIT8_Demosaicing_iDemSharpThresh [15:8]				 /
		{0x0F12, 0x070C},	//70000B8C //TVAR_afit_pBaseValS[484] /AFIT8_Demosaicing_iDemShLowLimit [7:0],	 AFIT8_Demosaicing_iDespeckleForDemsharp [15:8] 		 /	  c8 clamp
		{0x0F12, 0x3264},	//70000B8E //TVAR_afit_pBaseValS[485] /AFIT8_Demosaicing_iDemBlurLow[7:0],	 AFIT8_Demosaicing_iDemBlurHigh [15:8]						 /
		{0x0F12, 0x5A02},	//70000B90 //TVAR_afit_pBaseValS[486] /AFIT8_Demosaicing_iDemBlurRange[7:0],   AFIT8_Sharpening_iLowSharpPower [15:8]					 /
		{0x0F12, 0x1040},	//70000B92 //TVAR_afit_pBaseValS[487] /AFIT8_Sharpening_iHighSharpPower[7:0],	AFIT8_Sharpening_iLowShDenoise [15:8]					 /
		{0x0F12, 0x4012},	//70000B94 //TVAR_afit_pBaseValS[488] /AFIT8_Sharpening_iHighShDenoise [7:0],	AFIT8_Sharpening_iReduceEdgeMinMult [15:8]				 /
		{0x0F12, 0x0604},	//70000B96 //TVAR_afit_pBaseValS[489] /AFIT8_Sharpening_iReduceEdgeSlope [7:0],  AFIT8_demsharpmix1_iWideFiltReduce [15:8]				 /
		{0x0F12, 0x4606},	//70000B98 //TVAR_afit_pBaseValS[490] /AFIT8_demsharpmix1_iNarrFiltReduce [7:0],  AFIT8_sddd8a_iClustThresh_H_Bin [15:8]				 /
		{0x0F12, 0x0146},	//70000B9A //TVAR_afit_pBaseValS[491] /AFIT8_sddd8a_iClustThresh_C_Bin [7:0],	AFIT8_sddd8a_iClustMulT_H_Bin [15:8]					 /
		{0x0F12, 0x0101},	//70000B9C //TVAR_afit_pBaseValS[492] /AFIT8_sddd8a_iClustMulT_C_Bin [7:0],   AFIT8_sddd8a_nClustLevel_H_Bin [15:8] 					 /
		{0x0F12, 0x1C18},	//70000B9E //TVAR_afit_pBaseValS[493] /AFIT8_sddd8a_DispTH_Low_Bin [7:0],	AFIT8_sddd8a_DispTH_High_Bin [15:8] 						 /
		{0x0F12, 0x1819},	//70000BA0 //TVAR_afit_pBaseValS[494] /AFIT8_sddd8a_iDenThreshLow_Bin [7:0],   AFIT8_sddd8a_iDenThreshHigh_Bin [15:8]					 /
		{0x0F12, 0x0028},	//70000BA2 //TVAR_afit_pBaseValS[495] /AFIT8_Demosaicing_iEdgeDesat_Bin[7:0],	AFIT8_Demosaicing_iEdgeDesatThrLow_Bin [15:8]			 /
		{0x0F12, 0x030A},	//70000BA4 //TVAR_afit_pBaseValS[496] /AFIT8_Demosaicing_iEdgeDesatThrHigh_Bin [7:0],  AFIT8_Demosaicing_iEdgeDesatLimit_Bin [15:8] 	 /
		{0x0F12, 0x0514},	//70000BA6 //TVAR_afit_pBaseValS[497] /AFIT8_Demosaicing_iDemSharpenLow_Bin [7:0],	AFIT8_Demosaicing_iDemSharpenHigh_Bin [15:8]		 /
		{0x0F12, 0x0C14},	//70000BA8 //TVAR_afit_pBaseValS[498] /AFIT8_Demosaicing_iDemSharpThresh_Bin [7:0],  AFIT8_Demosaicing_iDemShLowLimit_Bin [15:8]		 /
		{0x0F12, 0xFF05},	//70000BAA //TVAR_afit_pBaseValS[499] /AFIT8_Demosaicing_iDespeckleForDemsharp_Bin [7:0],  AFIT8_Demosaicing_iDemBlurLow_Bin [15:8] 	 /
		{0x0F12, 0x0432},	//70000BAC //TVAR_afit_pBaseValS[500] /AFIT8_Demosaicing_iDemBlurHigh_Bin [7:0],  AFIT8_Demosaicing_iDemBlurRange_Bin [15:8]			 /
		{0x0F12, 0x4052},	//70000BAE //TVAR_afit_pBaseValS[501] /AFIT8_Sharpening_iLowSharpPower_Bin [7:0],  AFIT8_Sharpening_iHighSharpPower_Bin [15:8]			 /
		{0x0F12, 0x1514},	//70000BB0 //TVAR_afit_pBaseValS[502] /AFIT8_Sharpening_iLowShDenoise_Bin [7:0],  AFIT8_Sharpening_iHighShDenoise_Bin [15:8]			 /
		{0x0F12, 0x0440},	//70000BB2 //TVAR_afit_pBaseValS[503] /AFIT8_Sharpening_iReduceEdgeMinMult_Bin [7:0],  AFIT8_Sharpening_iReduceEdgeSlope_Bin [15:8] 	 /
		{0x0F12, 0x0302},	//70000BB4 //TVAR_afit_pBaseValS[504] /AFIT8_demsharpmix1_iWideFiltReduce_Bin [7:0],  AFIT8_demsharpmix1_iNarrFiltReduce_Bin [15:8] 	 /
		{0x0F12, 0x4646},	//70000BB6 //TVAR_afit_pBaseValS[505] /AFIT8_sddd8a_iClustThresh_H_sBin[7:0],	AFIT8_sddd8a_iClustThresh_C_sBin [15:8] 				 /
		{0x0F12, 0x0101},	//70000BB8 //TVAR_afit_pBaseValS[506] /AFIT8_sddd8a_iClustMulT_H_sBin [7:0],   AFIT8_sddd8a_iClustMulT_C_sBin [15:8]					 /
		{0x0F12, 0x1801},	//70000BBA //TVAR_afit_pBaseValS[507] /AFIT8_sddd8a_nClustLevel_H_sBin [7:0],	AFIT8_sddd8a_DispTH_Low_sBin [15:8] 					 /
		{0x0F12, 0x191C},	//70000BBC //TVAR_afit_pBaseValS[508] /AFIT8_sddd8a_DispTH_High_sBin [7:0],   AFIT8_sddd8a_iDenThreshLow_sBin [15:8]					 /
		{0x0F12, 0x2818},	//70000BBE //TVAR_afit_pBaseValS[509] /AFIT8_sddd8a_iDenThreshHigh_sBin[7:0],	AFIT8_Demosaicing_iEdgeDesat_sBin [15:8]				 /
		{0x0F12, 0x0A00},	//70000BC0 //TVAR_afit_pBaseValS[510] /AFIT8_Demosaicing_iEdgeDesatThrLow_sBin [7:0],  AFIT8_Demosaicing_iEdgeDesatThrHigh_sBin [15:8]	 /
		{0x0F12, 0x1403},	//70000BC2 //TVAR_afit_pBaseValS[511] /AFIT8_Demosaicing_iEdgeDesatLimit_sBin [7:0],  AFIT8_Demosaicing_iDemSharpenLow_sBin [15:8]		 /
		{0x0F12, 0x1405},	//70000BC4 //TVAR_afit_pBaseValS[512] /AFIT8_Demosaicing_iDemSharpenHigh_sBin [7:0],  AFIT8_Demosaicing_iDemSharpThresh_sBin [15:8] 	 /
		{0x0F12, 0x050C},	//70000BC6 //TVAR_afit_pBaseValS[513] /AFIT8_Demosaicing_iDemShLowLimit_sBin [7:0],  AFIT8_Demosaicing_iDespeckleForDemsharp_sBin [15:8] /
		{0x0F12, 0x32FF},	//70000BC8 //TVAR_afit_pBaseValS[514] /AFIT8_Demosaicing_iDemBlurLow_sBin [7:0],  AFIT8_Demosaicing_iDemBlurHigh_sBin [15:8]			 /
		{0x0F12, 0x5204},	//70000BCA //TVAR_afit_pBaseValS[515] /AFIT8_Demosaicing_iDemBlurRange_sBin [7:0],	AFIT8_Sharpening_iLowSharpPower_sBin [15:8] 		 /
		{0x0F12, 0x1440},	//70000BCC //TVAR_afit_pBaseValS[516] /AFIT8_Sharpening_iHighSharpPower_sBin [7:0],  AFIT8_Sharpening_iLowShDenoise_sBin [15:8] 		 /
		{0x0F12, 0x4015},	//70000BCE //TVAR_afit_pBaseValS[517] /AFIT8_Sharpening_iHighShDenoise_sBin [7:0],	AFIT8_Sharpening_iReduceEdgeMinMult_sBin [15:8] 	 /
		{0x0F12, 0x0204},	//70000BD0 //TVAR_afit_pBaseValS[518] /AFIT8_Sharpening_iReduceEdgeSlope_sBin [7:0],  AFIT8_demsharpmix1_iWideFiltReduce_sBin [15:8]	 /
		{0x0F12, 0x0003},	//70000BD2 //TVAR_afit_pBaseValS[519] /AFIT8_demsharpmix1_iNarrFiltReduce_sBin [7:0]													 /
								
		{0x0F12, 0x7DFA},	//afit_pConstBaseValS[0] // 																		 
		{0x0F12, 0xFFBD},	//afit_pConstBaseValS[1] // 																		 
		{0x0F12, 0x26FE},	//afit_pConstBaseValS[2] // 																		 
		{0x0F12, 0xF7BC},	//afit_pConstBaseValS[3] // 																		 
		{0x0F12, 0x7E06},	//afit_pConstBaseValS[4] // 																		 
		{0x0F12, 0x00D3},	//afit_pConstBaseValS[5] // 																		 
		
							 
		{0x002A, 0x2CE8},	 
		{0x0F12, 0x0007},	// Modify LSB to control AWBB_YThreshLow // 																			
		{0x0F12, 0x00E2},	//										   
		{0x0F12, 0x0005},	// Modify LSB to control AWBB_YThreshLowBrLow// 																	   
		{0x0F12, 0x00E2},	//										   
																			
																						  
		{0x002A, 0x0F7E},	 //ae weight	//								  
		{0x0F12, 0x0000},  //0000  //0000
		{0x0F12, 0x0000},  //0000  //0000
		{0x0F12, 0x0000},  //0000  //0000
		{0x0F12, 0x0000},  //0000  //0000					  
		{0x0F12, 0x0101},  //0101  //0101
		{0x0F12, 0x0101},  //0101  //0101
		{0x0F12, 0x0101},  //0101  //0101
		{0x0F12, 0x0101},  //0101  //0101					  
		{0x0F12, 0x0101},  //0101  //0101
		{0x0F12, 0x0201},  //0201  //0201
		{0x0F12, 0x0102},  //0102  //0102
		{0x0F12, 0x0101},  //0101  //0101					  
		{0x0F12, 0x0101},  //0101  //0101
		{0x0F12, 0x0202},  //0202  //0202
		{0x0F12, 0x0202},  //0202  //0202
		{0x0F12, 0x0101},  //0101  //0101					  
		{0x0F12, 0x0101},  //0101  //0101
		{0x0F12, 0x0202},  //0202  //0202
		{0x0F12, 0x0202},  //0202  //0202
		{0x0F12, 0x0101},  //0101  //0101					  
		{0x0F12, 0x0201},  //0101  //0101
		{0x0F12, 0x0202},  //0202  //0202
		{0x0F12, 0x0202},  //0202  //0202
		{0x0F12, 0x0102},  //0101  //0101					  
		{0x0F12, 0x0201},  //0101  //0201
		{0x0F12, 0x0202},  //0202  //0202
		{0x0F12, 0x0202},  //0202  //0202
		{0x0F12, 0x0102},  //0101  //0102					  
		{0x0F12, 0x0101},  //0101  //0201
		{0x0F12, 0x0101},  //0101  //0202
		{0x0F12, 0x0101},  //0101  //0202
		{0x0F12, 0x0101},  //0101  //0102
							 
									
		{0x002A, 0x01CC},	 
		{0x0F12, 0x5DC0},	//REG_TC_IPRM_InClockLSBs//input clock=24MHz																									
		{0x0F12, 0x0000},	 //REG_TC_IPRM_InClockMSBs											 
		{0x002A, 0x01EE},																		 
		{0x0F12, 0x0003},	//REG_TC_IPRM_UseNPviClocks 									
					 
		{0x002A, 0x01F6},																		 
		{0x0F12, 0x1F40},	 //REG_TC_IPRM_OpClk4KHz_0						2	700001F6		 
		{0x0F12, 0x32A8},	//3A88	  //REG_TC_IPRM_MinOutRate4KHz_0				2	700001F8																		
		{0x0F12, 0x32E8},	//3AA8	  //REG_TC_IPRM_MaxOutRate4KHz_0						
							 
		{0x0F12, 0x1F40},	 //REG_TC_IPRM_OpClk4KHz_1						2	700001FC	
		{0x0F12, 0x2ea0},	 //REG_TC_IPRM_MinOutRate4KHz_1 				2	700001FE 
		{0x0F12, 0x2f00},	 //REG_TC_IPRM_MaxOutRate4KHz_1 				2	70000200 
				 
		{0x0F12, 0x38A4},	 //REG_TC_IPRM_OpClk4KHz_2						2	70000202 
		{0x0F12, 0x37A4},	 //REG_TC_IPRM_MinOutRate4KHz_2 				2	70000204 
		{0x0F12, 0x39A4},	 //REG_TC_IPRM_MaxOutRate4KHz_2 				2	70000206 
				 
		{0x002A, 0x0208},																 
		{0x0F12, 0x0001},	   //REG_TC_IPRM_InitParamsUpdated							 
		
		{0xffff, 0x0064},
		
		//{0xffff, 0xffff},  //sleep(100)					  
		
		   
		{0x0028, 0x7000},		
		{0x002A, 0x026C},	//Normal preview 15fps
		{0x0F12, 0x0280},	//REG_0TC_PCFG_usWidth							2	7000026C			//							  
		{0x0F12, 0x01E0},	//REG_0TC_PCFG_usHeight 						2	7000026E	//										   
		{0x0F12, 0x0005},	//REG_0TC_PCFG_Format							2	70000270	//										   
		{0x0F12, 0x39A4},	//3AA8//REG_0TC_PCFG_usMaxOut4KHzRate				2	70000272  //							  
		{0x0F12, 0x37A4},	//3A88//REG_0TC_PCFG_usMinOut4KHzRate				2	70000274  //							  
		{0x0F12, 0x0100},	//REG_0TC_PCFG_OutClkPerPix88					2	70000276	//										   
		{0x0F12, 0x0800},	//REG_0TC_PCFG_uMaxBpp88						2	70000278	//										   
		{0x0F12, 0x0092},	//REG_0TC_PCFG_PVIMask							2	7000027A	//92  (1) PCLK inversion  (4)1b_C first (5) UV First									   
		{0x0F12, 0x0010},	//REG_0TC_PCFG_OIFMask							2	7000027C	//										   
		{0x0F12, 0x01E0},	//REG_0TC_PCFG_usJpegPacketSize 				2	7000027E	//										   
		{0x0F12, 0x0000},	//REG_0TC_PCFG_usJpegTotalPackets				2	70000280	//										   
		{0x0F12, 0x0002},	//REG_0TC_PCFG_uClockInd						2	70000282	//										   
		{0x0F12, 0x0000},	//REG_0TC_PCFG_usFrTimeType 					2	70000284	//										   
		{0x0F12, 0x0001},	//REG_0TC_PCFG_FrRateQualityType				2	70000286	//										   
		{0x0F12, 0x03E8},//0535 014E	//REG_0TC_PCFG_usMaxFrTimeMsecMult10			2	70000288	//										   
		{0x0F12, 0x01F4},//01F4 014E	//REG_0TC_PCFG_usMinFrTimeMsecMult10			2	7000028A	//										   
		{0x0F12, 0x0000},	//REG_0TC_PCFG_bSmearOutput 					2	7000028C	//										   
		{0x0F12, 0x0000},	//REG_0TC_PCFG_sSaturation						2	7000028E	//										   
		{0x0F12, 0x0000},	//REG_0TC_PCFG_sSharpBlur						2	70000290	//										   
		{0x0F12, 0x0000},	//REG_0TC_PCFG_sColorTemp						2	70000292	//										   
		{0x0F12, 0x0000},	//REG_0TC_PCFG_uDeviceGammaIndex				2	70000294	//										   
		{0x0F12, 0x0003},	//01 REG_0TC_PCFG_uPrevMirror						2	70000296	//										   
		{0x0F12, 0x0003},	//01 REG_0TC_PCFG_uCaptureMirror					2	70000298	//										   
		{0x0F12, 0x0000},	//REG_0TC_PCFG_uRotation						2	7000029A	// 
							
							//comcoder frame fix 15 									   
		{0x0F12, 0x0400},	//REG_1TC_PCFG_usWidth							2	7000029C	//										   
		{0x0F12, 0x0300},	//REG_1TC_PCFG_usHeight 						2	7000029E	//										   
		{0x0F12, 0x0005},	//REG_1TC_PCFG_Format							2	700002A0	//										   
		{0x0F12, 0x32E8},	//REG_1TC_PCFG_usMaxOut4KHzRate 				2	700002A2	//										   
		{0x0F12, 0x32a8},	//REG_1TC_PCFG_usMinOut4KHzRate 				2	700002A4	//										   
		{0x0F12, 0x0100},	//REG_1TC_PCFG_OutClkPerPix88					2	700002A6	//										   
		{0x0F12, 0x0800},	//REG_1TC_PCFG_uMaxBpp88						2	700002A8	//										   
		{0x0F12, 0x0092},	//REG_1TC_PCFG_PVIMask							2	700002AA	//										   
		{0x0F12, 0x0010},	//REG_1TC_PCFG_OIFMask							2	700002AC	//										   
		{0x0F12, 0x01E0},	//REG_1TC_PCFG_usJpegPacketSize 				2	700002AE	//										   
		{0x0F12, 0x0000},	//REG_1TC_PCFG_usJpegTotalPackets				2	700002B0	//										   
		{0x0F12, 0x0000},	//REG_1TC_PCFG_uClockInd						2	700002B2	//										   
		{0x0F12, 0x0000},	//REG_1TC_PCFG_usFrTimeType 					2	700002B4	//										   
		{0x0F12, 0x0001},	//REG_1TC_PCFG_FrRateQualityType				2	700002B6	//										   
		{0x0F12, 0x029a},	//REG_1TC_PCFG_usMaxFrTimeMsecMult10			2	700002B8	//										   
		{0x0F12, 0x029a},	//REG_1TC_PCFG_usMinFrTimeMsecMult10			2	700002BA	//										   
		{0x0F12, 0x0000},	//REG_1TC_PCFG_bSmearOutput 					2	700002BC	//										   
		{0x0F12, 0x0000},	//REG_1TC_PCFG_sSaturation						2	700002BE	//										   
		{0x0F12, 0x0000},	//REG_1TC_PCFG_sSharpBlur						2	700002C0	//										   
		{0x0F12, 0x0000},	//REG_1TC_PCFG_sColorTemp						2	700002C2	//										   
		{0x0F12, 0x0000},	//REG_1TC_PCFG_uDeviceGammaIndex				2	700002C4	//										   
		{0x0F12, 0x0003},	//REG_1TC_PCFG_uPrevMirror						2	700002C6	//										   
		{0x0F12, 0x0003},	//REG_1TC_PCFG_uCaptureMirror					2	700002C8	//										   
		{0x0F12, 0x0000},	//REG_1TC_PCFG_uRotation						2	700002CA	// 
											
								 
		{0x002A, 0x035C},	//Normal capture //    
		{0x0F12, 0x0000},	//REG_0TC_CCFG_uCaptureMode 					2	7000035C						// 
		{0x0F12, 0x0800},	//REG_0TC_CCFG_usWidth							2	7000035E		  //								   
		{0x0F12, 0x0600},	//REG_0TC_CCFG_usHeight 						2	70000360		  //								   
		{0x0F12, 0x0005},	//REG_0TC_CCFG_Format							2	70000362		  //								   
		{0x0F12, 0x32E8},	//2f00//3AA8//REG_0TC_CCFG_usMaxOut4KHzRate 				2	70000364  //					  
		{0x0F12, 0x32a8},	//2ea0//3A88//REG_0TC_CCFG_usMinOut4KHzRate 				2	70000366  //					  
		{0x0F12, 0x0100},	//REG_0TC_CCFG_OutClkPerPix88					2	70000368		  //								   
		{0x0F12, 0x0800},	//REG_0TC_CCFG_uMaxBpp88						2	7000036A		  //								   
		{0x0F12, 0x0092},	//REG_0TC_CCFG_PVIMask							2	7000036C		  //								   
		{0x0F12, 0x0010},	//REG_0TC_CCFG_OIFMask							2	7000036E		  //								   
		{0x0F12, 0x01E0},	//REG_0TC_CCFG_usJpegPacketSize 				2	70000370		  //								   
		{0x0F12, 0x0000},	//REG_0TC_CCFG_usJpegTotalPackets				2	70000372		  //								   
		{0x0F12, 0x0000},	//REG_0TC_CCFG_uClockInd						2	70000374		  //								   
		{0x0F12, 0x0000},	//REG_0TC_CCFG_usFrTimeType 					2	70000376		  //								   
		{0x0F12, 0x0002},	//REG_0TC_CCFG_FrRateQualityType				2	70000378		  //								   
		{0x0F12, 0x0535},	//REG_0TC_CCFG_usMaxFrTimeMsecMult10			2	7000037A		  //								   
		{0x0F12, 0x0535},	//REG_0TC_CCFG_usMinFrTimeMsecMult10			2	7000037C		  //								   
		{0x0F12, 0x0000},	//REG_0TC_CCFG_bSmearOutput 					2	7000037E		  //								   
		{0x0F12, 0x0000},	//REG_0TC_CCFG_sSaturation						2	70000380		  //								   
		{0x0F12, 0x0000},	//REG_0TC_CCFG_sSharpBlur						2	70000382		  //								   
		{0x0F12, 0x0000},	//REG_0TC_CCFG_sColorTemp						2	70000384		  //								   
		{0x0F12, 0x0000},	//REG_0TC_CCFG_uDeviceGammaIndex				2	70000386		  //								   
					 
							// Low_lux capture//	
		{0x0F12, 0x0000},	//REG_1TC_CCFG_uCaptureMode 					2	70000388						// 
		{0x0F12, 0x0800},	//REG_1TC_CCFG_usWidth							2	7000038A		  //								   
		{0x0F12, 0x0600},	//REG_1TC_CCFG_usHeight 						2	7000038C		  //								   
		{0x0F12, 0x0005},	//REG_1TC_CCFG_Format							2	7000038E		  //								   
		{0x0F12, 0x32E8},	//REG_1TC_CCFG_usMaxOut4KHzRate 				2	70000390		  //								   
		{0x0F12, 0x32a8},	//REG_1TC_CCFG_usMinOut4KHzRate 				2	70000392		  //								   
		{0x0F12, 0x0100},	//REG_1TC_CCFG_OutClkPerPix88					2	70000394		  //								   
		{0x0F12, 0x0800},	//REG_1TC_CCFG_uMaxBpp88						2	70000396		  //								   
		{0x0F12, 0x0092},	//REG_1TC_CCFG_PVIMask							2	70000398		  //								   
		{0x0F12, 0x0010},	//REG_1TC_CCFG_OIFMask							2	7000039A		  //								   
		{0x0F12, 0x01E0},	//REG_1TC_CCFG_usJpegPacketSize 				2	7000039C		  //								   
		{0x0F12, 0x0000},	//REG_1TC_CCFG_usJpegTotalPackets				2	7000039E		  //								   
		{0x0F12, 0x0000},	//REG_1TC_CCFG_uClockInd						2	700003A0		  //								   
		{0x0F12, 0x0000},	//REG_1TC_CCFG_usFrTimeType 					2	700003A2		  //								   
		{0x0F12, 0x0002},	//REG_1TC_CCFG_FrRateQualityType				2	700003A4		  //								   
		{0x0F12, 0x0535},	//REG_1TC_CCFG_usMaxFrTimeMsecMult10			2	700003A6		  //								   
		{0x0F12, 0x0535},	//REG_1TC_CCFG_usMinFrTimeMsecMult10			2	700003A8		  //								   
		{0x0F12, 0x0000},	//REG_1TC_CCFG_bSmearOutput 					2	700003AA		  //								   
		{0x0F12, 0x0000},	//REG_1TC_CCFG_sSaturation						2	700003AC		  //								   
		{0x0F12, 0x0000},	//REG_1TC_CCFG_sSharpBlur						2	700003AE		  //								   
		{0x0F12, 0x0000},	//REG_1TC_CCFG_sColorTemp						2	700003B0		  //								   
		{0x0F12, 0x0000},	//REG_1TC_CCFG_uDeviceGammaIndex				2	700003B2		  //					
							  
		{0x002A, 0x0208},		 
		{0x0F12, 0x0001},	   //REG_TC_IPRM_InitParamsUpdated//																	   
								 
		{0x002A, 0x023C},	
		{0x0F12, 0x0000},	//32MHz Sys Clock// 
		{0x002A, 0x0244},					   
		{0x0F12, 0x0000},		
		{0x002A, 0x0240}, 
		{0x0F12, 0x0001}, 
		{0x002A, 0x0230}, 
		{0x0F12, 0x0001}, 
		{0x002A, 0x023E}, 
		{0x0F12, 0x0001}, 
		{0x002A, 0x0246}, 
		{0x0F12, 0x0001}, 
		{0x002A, 0x0220}, 
		{0x0F12, 0x0001}, 
		{0x0F12, 0x0001}, 
						  
		{0x1000, 0x0001},	// Set host interrupt so main start run//												   
		{0xffff, 0x0064},
		
		//{0xffff, 0xffff},  //sleep(100)  
							 
							 
		{0x002A, 0x0DA0},	 
		{0x0F12, 0x0005},	 
							 
		{0x002A, 0x0D88},	 
		{0x0F12, 0x0038},	 
		{0x0F12, 0x0074},	 
		{0x0F12, 0xFFF1},	 
		{0x0F12, 0x00BF},	 
		{0x0F12, 0xFF9B},	 
		{0x0F12, 0x00DB},	 
		{0x0F12, 0xFF56},	 
		{0x0F12, 0x00F0},	 
		{0x0F12, 0xFEFF},	 
		{0x0F12, 0x010F},	 
		{0x0F12, 0x0E74},	 
		{0x002A, 0x0DA8},	 
		{0x0F12, 0x0BB8},	 //NB 3000
		{0x002A, 0x0DA4},	 
		{0x0F12, 0x274E},	 
		
		{0x002A, 0x0DCA},
		{0x0F12, 0x0030},
		
		{0x002A, 0x3286},
		{0x0F12, 0x0001}, //Pre/Post gamma on(원복) 	  
		
		{0x002A, 0x032C},
		{0x0F12, 0xAAAA},  //ESD Check			   
		
		
		{0x002A, 0x032E},
		{0x0F12, 0xFFFE},	 //HighLux over this NB
		{0x0F12, 0x0000},	
		
		{0x0F12, 0x001C},	 //LowLux under this NB
		{0x0F12, 0x0000}, 

#endif
};

LOCAL const SENSOR_REG_T s5k5cagx_640X480[]=
	#if 0
{
    {SENSOR_WRITE_DELAY, 10},
    {0xFCFC, 0xD000},
    {0x0010, 0x0001},	
    {0x1030, 0x0000},	
    {0x0014, 0x0001},	
    {SENSOR_WRITE_DELAY, 0x000A},
    {0x0028, 0x7000},
    {0x002A, 0x2CF8},
    {0x0F12, 0xB570},
    {0x0F12, 0x4927},
    {0x0F12, 0x20C0},
    {0x0F12, 0x8048},
    {0x0F12, 0x4C25},
    {0x0F12, 0x4926},
    {0x0F12, 0x3420},
    {0x0F12, 0x83A1},
    {0x0F12, 0x1D09},
    {0x0F12, 0x83E1},
    {0x0F12, 0x4922},
    {0x0F12, 0x3140},
    {0x0F12, 0x8048},
    {0x0F12, 0x4D21},
    {0x0F12, 0x4822},
    {0x0F12, 0x3560},
    {0x0F12, 0x83A8},
    {0x0F12, 0x1D00},
    {0x0F12, 0x83E8},
    {0x0F12, 0x4821},
    {0x0F12, 0x2201},
    {0x0F12, 0x2140},
    {0x0F12, 0xF000},
    {0x0F12, 0xFA24},
    {0x0F12, 0x481F},
    {0x0F12, 0x8360},
    {0x0F12, 0x481C},
    {0x0F12, 0x1F00},
    {0x0F12, 0x8368},
    {0x0F12, 0x481E},
    {0x0F12, 0x4918},
    {0x0F12, 0x8802},
    {0x0F12, 0x3980},
    {0x0F12, 0x804A},
    {0x0F12, 0x8842},
    {0x0F12, 0x808A},
    {0x0F12, 0x8882},
    {0x0F12, 0x80CA},
    {0x0F12, 0x88C2},
    {0x0F12, 0x810A},
    {0x0F12, 0x8902},
    {0x0F12, 0x4919},
    {0x0F12, 0x80CA},
    {0x0F12, 0x8942},
    {0x0F12, 0x814A},
    {0x0F12, 0x8982},
    {0x0F12, 0x830A},
    {0x0F12, 0x89C2},
    {0x0F12, 0x834A},
    {0x0F12, 0x8A00},
    {0x0F12, 0x4915},
    {0x0F12, 0x8188},
    {0x0F12, 0x4915},
    {0x0F12, 0x4816},
    {0x0F12, 0xF000},
    {0x0F12, 0xFA0C},
    {0x0F12, 0x4915},
    {0x0F12, 0x4816},
    {0x0F12, 0x63C1},
    {0x0F12, 0x4916},
    {0x0F12, 0x6301},
    {0x0F12, 0x4916},
    {0x0F12, 0x3040},
    {0x0F12, 0x6181},
    {0x0F12, 0x4915},
    {0x0F12, 0x4816},
    {0x0F12, 0xF000},
    {0x0F12, 0xFA00},
    {0x0F12, 0x4915},
    {0x0F12, 0x4816},
    {0x0F12, 0xF000},
    {0x0F12, 0xF9FC},
    {0x0F12, 0x4915},
    {0x0F12, 0x4816},
    {0x0F12, 0xF000},
    {0x0F12, 0xF9F8},
    {0x0F12, 0xBC70},
    {0x0F12, 0xBC08},
    {0x0F12, 0x4718},
    {0x0F12, 0x0000},
    {0x0F12, 0x1100},
    {0x0F12, 0xD000},
    {0x0F12, 0x267C},
    {0x0F12, 0x0000},
    {0x0F12, 0x2CE8},
    {0x0F12, 0x0000},
    {0x0F12, 0x1102},
    {0x0F12, 0x0000},
    {0x0F12, 0x6A02},
    {0x0F12, 0x0000},
    {0x0F12, 0x3364},
    {0x0F12, 0x7000},
    {0x0F12, 0xF400},
    {0x0F12, 0xD000},
    {0x0F12, 0xF520},
    {0x0F12, 0xD000},
    {0x0F12, 0x2DE9},
    {0x0F12, 0x7000},
    {0x0F12, 0x89A9},
    {0x0F12, 0x0000},
    {0x0F12, 0x2E3B},
    {0x0F12, 0x7000},
    {0x0F12, 0x0080},
    {0x0F12, 0x7000},
    {0x0F12, 0x2EE9},
    {0x0F12, 0x7000},
    {0x0F12, 0x2EFD},
    {0x0F12, 0x7000},
    {0x0F12, 0x2ECD},
    {0x0F12, 0x7000},
    {0x0F12, 0x013D},
    {0x0F12, 0x0001},
    {0x0F12, 0x2F83},
    {0x0F12, 0x7000},
    {0x0F12, 0xD789},
    {0x0F12, 0x0000},
    {0x0F12, 0x2FDB},
    {0x0F12, 0x7000},
    {0x0F12, 0x6D1B},
    {0x0F12, 0x0000},
    {0x0F12, 0xB570},
    {0x0F12, 0x6804},
    {0x0F12, 0x6845},
    {0x0F12, 0x6881},
    {0x0F12, 0x6840},
    {0x0F12, 0x2900},
    {0x0F12, 0x6880},
    {0x0F12, 0xD007},
    {0x0F12, 0x49C8},
    {0x0F12, 0x8949},
    {0x0F12, 0x084A},
    {0x0F12, 0x1880},
    {0x0F12, 0xF000},
    {0x0F12, 0xF9C6},
    {0x0F12, 0x80A0},
    {0xFFFF, 10},
    {0x0F12, 0xE000},
    {0x0F12, 0x80A0},
    {0x0F12, 0x88A0},
    {0x0F12, 0x2800},
    {0x0F12, 0xD010},
    {0x0F12, 0x68A9},
    {0x0F12, 0x6828},
    {0x0F12, 0x084A},
    {0x0F12, 0x1880},
    {0x0F12, 0xF000},
    {0x0F12, 0xF9BA},
    {0x0F12, 0x8020},
    {0x0F12, 0x1D2D},
    {0x0F12, 0xCD03},
    {0x0F12, 0x084A},
    {0x0F12, 0x1880},
    {0x0F12, 0xF000},
    {0x0F12, 0xF9B3},
    {0x0F12, 0x8060},
    {0x0F12, 0xBC70},
    {0x0F12, 0xBC08},
    {0x0F12, 0x4718},
    {0x0F12, 0x2000},
    {0x0F12, 0x8060},
    {0x0F12, 0x8020},
    {0x0F12, 0xE7F8},
    {0x0F12, 0xB5F8},
    {0x0F12, 0x0004},
    {0x0F12, 0x48B8},
    {0x0F12, 0x237D},
    {0x0F12, 0x8B00},
    {0x0F12, 0x4FB7},
    {0x0F12, 0x015B},
    {0x0F12, 0x4358},
    {0x0F12, 0x8A39},
    {0x0F12, 0x084A},
    {0x0F12, 0x1880},
    {0x0F12, 0xF000},
    {0x0F12, 0xF99E},
    {0x0F12, 0x210F},
    {0x0F12, 0xF000},
    {0x0F12, 0xF9A1},
    {0x0F12, 0x49B3},
    {0x0F12, 0x8C49},
    {0x0F12, 0x090E},
    {0x0F12, 0x0136},
    {0x0F12, 0x4306},
    {0x0F12, 0x49B1},
    {0x0F12, 0x2C00},
    {0x0F12, 0xD004},
    {0x0F12, 0x2201},
    {0x0F12, 0x0030},
    {0x0F12, 0x0252},
    {0x0F12, 0x4310},
    {0x0F12, 0x8108},
    {0x0F12, 0x48AE},
    {0x0F12, 0x2C00},
    {0x0F12, 0x8D00},
    {0x0F12, 0xD001},
    {0x0F12, 0x2501},
    {0x0F12, 0xE000},
    {0x0F12, 0x2500},
    {0x0F12, 0x49AA},
    {0x0F12, 0x4328},
    {0x0F12, 0x8008},
    {0x0F12, 0x207D},
    {0x0F12, 0x00C0},
    {0x0F12, 0xF000},
    {0x0F12, 0xF98E},
    {0x0F12, 0x2C00},
    {0x0F12, 0x49A6},
    {0x0F12, 0x0328},
    {0x0F12, 0x4330},
    {0x0F12, 0x8108},
    {0x0F12, 0x48A1},
    {0x0F12, 0x2C00},
    {0x0F12, 0x8AC0},
    {0x0F12, 0x01AA},
    {0x0F12, 0x4310},
    {0x0F12, 0x8088},
    {0x0F12, 0x8A39},
    {0x0F12, 0x48A2},
    {0x0F12, 0xF000},
    {0x0F12, 0xF971},
    {0x0F12, 0x49A2},
    {0x0F12, 0x8809},
    {0x0F12, 0x4348},
    {0x0F12, 0x0400},
    {0x0F12, 0x0C00},
    {0x0F12, 0xF000},
    {0x0F12, 0xF978},
    {0x0F12, 0x0020},
    {0x0F12, 0xF000},
    {0x0F12, 0xF97D},
    {0x0F12, 0x489E},
    {0x0F12, 0x7004},
    {0x0F12, 0xBCF8},
    {0x0F12, 0xBC08},
    {0x0F12, 0x4718},
    {0x0F12, 0xB510},
    {0x0F12, 0x0004},
    {0x0F12, 0xF000},
    {0x0F12, 0xF97C},
    {0x0F12, 0x6020},
    {0x0F12, 0x499A},
    {0x0F12, 0x8B49},
    {0x0F12, 0x0789},
    {0x0F12, 0xD001},
    {0x0F12, 0x0040},
    {0x0F12, 0x6020},
    {0x0F12, 0xBC10},
    {0x0F12, 0xBC08},
    {0x0F12, 0x4718},
    {0x0F12, 0xB510},
    {0x0F12, 0xF000},
    {0x0F12, 0xF977},
    {0x0F12, 0x4895},
    {0x0F12, 0x498B},
    {0x0F12, 0x8880},
    {0x0F12, 0x0600},
    {0x0F12, 0x1600},
    {0x0F12, 0x8348},
    {0x0F12, 0xE7F2},
    {0x0F12, 0xB5F8},
    {0x0F12, 0x000F},
    {0x0F12, 0x4D8B},
    {0x0F12, 0x3520},
    {0x0F12, 0x2400},
    {0x0F12, 0x572C},
    {0x0F12, 0x0039},
    {0x0F12, 0xF000},
    {0x0F12, 0xF96F},
    {0x0F12, 0x9000},
    {0x0F12, 0x2600},
    {0x0F12, 0x57AE},
    {0x0F12, 0x4D82},
    {0x0F12, 0x42A6},
    {0x0F12, 0xD01B},
    {0x0F12, 0x4C8A},
    {0x0F12, 0x8AE0},
    {0x0F12, 0x2800},
    {0x0F12, 0xD013},
    {0x0F12, 0x4883},
    {0x0F12, 0x8A01},
    {0x0F12, 0x8B80},
    {0x0F12, 0x4378},
    {0x0F12, 0xF000},
    {0x0F12, 0xF931},
    {0x0F12, 0x89A1},
    {0x0F12, 0x1A41},
    {0x0F12, 0x4884},
    {0x0F12, 0x3820},
    {0x0F12, 0x8AC0},
    {0x0F12, 0x4348},
    {0x0F12, 0x17C1},
    {0x0F12, 0x0D89},
    {0x0F12, 0x1808},
    {0x0F12, 0x1280},
    {0xFFFF, 10},	    
    {0x0F12, 0x8B69},
    {0x0F12, 0x1A08},
    {0x0F12, 0x8368},
    {0x0F12, 0xE003},
    {0x0F12, 0x88A0},
    {0x0F12, 0x0600},
    {0x0F12, 0x1600},
    {0x0F12, 0x8368},
    {0x0F12, 0x201A},
    {0x0F12, 0x5E28},
    {0x0F12, 0x42B0},
    {0x0F12, 0xD011},
    {0x0F12, 0xF000},
    {0x0F12, 0xF94F},
    {0x0F12, 0x1D40},
    {0x0F12, 0x00C3},
    {0x0F12, 0x1A18},
    {0x0F12, 0x214B},
    {0x0F12, 0xF000},
    {0x0F12, 0xF913},
    {0x0F12, 0x211F},
    {0x0F12, 0xF000},
    {0x0F12, 0xF916},
    {0x0F12, 0x211A},
    {0x0F12, 0x5E69},
    {0x0F12, 0x0FC9},
    {0x0F12, 0x0149},
    {0x0F12, 0x4301},
    {0x0F12, 0x4873},
    {0x0F12, 0x81C1},
    {0x0F12, 0x9800},
    {0x0F12, 0xE7A1},
    {0x0F12, 0xB570},
    {0x0F12, 0x6805},
    {0x0F12, 0x2404},
    {0x0F12, 0xF000},
    {0x0F12, 0xF940},
    {0x0F12, 0x2800},
    {0x0F12, 0xD103},
    {0x0F12, 0xF000},
    {0xFFFF, 10},    
    {0x0F12, 0xF944},
    {0x0F12, 0x2800},
    {0x0F12, 0xD000},
    {0x0F12, 0x2400},
    {0x0F12, 0x3540},
    {0x0F12, 0x88E8},
    {0x0F12, 0x0500},
    {0x0F12, 0xD403},
    {0x0F12, 0x486A},
    {0x0F12, 0x89C0},
    {0x0F12, 0x2800},
    {0x0F12, 0xD002},
    {0x0F12, 0x2008},
    {0x0F12, 0x4304},
    {0x0F12, 0xE001},
    {0x0F12, 0x2010},
    {0x0F12, 0x4304},
    {0x0F12, 0x4866},
    {0x0F12, 0x8B80},
    {0x0F12, 0x0700},
    {0x0F12, 0x0F81},
    {0x0F12, 0x2001},
    {0x0F12, 0x2900},
    {0x0F12, 0xD000},
    {0x0F12, 0x4304},
    {0x0F12, 0x4963},
    {0x0F12, 0x8B0A},
    {0x0F12, 0x42A2},
    {0x0F12, 0xD004},
    {0x0F12, 0x0762},
    {0x0F12, 0xD502},
    {0x0F12, 0x4A60},
    {0x0F12, 0x3220},
    {0x0F12, 0x8110},
    {0x0F12, 0x830C},
    {0x0F12, 0xE728},
    {0x0F12, 0xB5F8},
    {0x0F12, 0x2600},
    {0x0F12, 0x4C5E},
    {0x0F12, 0x495E},
    {0x0F12, 0x8B20},
    {0x0F12, 0x2800},
    {0x0F12, 0xD101},
    {0x0F12, 0x2001},
    {0x0F12, 0x6088},
    {0x0F12, 0x485C},
    {0x0F12, 0x4D5B},
    {0x0F12, 0x6028},
    {0x0F12, 0x3080},
    {0x0F12, 0x6068},
    {0x0F12, 0x4858},
    {0x0F12, 0x2100},
    {0x0F12, 0x3840},
    {0x0F12, 0x6101},
    {0x0F12, 0x60C1},
    {0x0F12, 0xF000},
    {0x0F12, 0xF914},
    {0x0F12, 0x68A8},
    {0x0F12, 0x4F57},
    {0x0F12, 0x2800},
    {0x0F12, 0xD025},
    {0x0F12, 0x4844},
    {0x0F12, 0x4D53},
    {0x0F12, 0x8A80},
    {0x0F12, 0x6128},
    {0x0F12, 0x8B20},
    {0x0F12, 0x2120},
    {0x0F12, 0xF000},
    {0x0F12, 0xF8C0},
    {0x0F12, 0x60E8},
    {0x0F12, 0x2600},
    {0x0F12, 0x616E},
    {0x0F12, 0x2400},
    {0x0F12, 0xE013},
    {0x0F12, 0x4950},
    {0x0F12, 0x0060},
    {0x0F12, 0x1841},
    {0x0F12, 0x2020},
    {0x0F12, 0x5E08},
    {0x0F12, 0x8BF9},
    {0x0F12, 0x084A},
    {0x0F12, 0x1880},
    {0x0F12, 0xF000},
    {0x0F12, 0xF8AB},
    {0x0F12, 0x682B},
    {0x0F12, 0x0202},
    {0x0F12, 0x00A1},
    {0x0F12, 0x505A},
    {0x0F12, 0x0002},
    {0x0F12, 0x4342},
    {0x0F12, 0x0210},
    {0x0F12, 0x686A},
    {0x0F12, 0x5050},
    {0x0F12, 0x1C64},
    {0x0F12, 0x68E8},
    {0x0F12, 0x4284},
    {0x0F12, 0xD3E8},
    {0x0F12, 0x60AE},
    {0x0F12, 0xE049},
    {0x0F12, 0x2400},
    {0x0F12, 0x4844},
    {0x0F12, 0x9000},
    {0x0F12, 0xE033},
    {0x0F12, 0x9800},
    {0x0F12, 0x8845},
    {0x0F12, 0x4940},
    {0x0F12, 0x0060},
    {0x0F12, 0x1841},
    {0x0F12, 0x2020},
    {0x0F12, 0x5E08},
    {0x0F12, 0x8BF9},
    {0x0F12, 0x084A},
    {0x0F12, 0x1880},
    {0x0F12, 0xF000},
    {0x0F12, 0xF88C},
    {0x0F12, 0x4A38},
    {0x0F12, 0x00A1},
    {0x0F12, 0x6812},
    {0x0F12, 0x4696},
    {0x0F12, 0x5852},
    {0x0F12, 0x436A},
    {0x0F12, 0x3280},
    {0x0F12, 0x0A13},
    {0x0F12, 0x22FF},
    {0x0F12, 0x3201},
    {0x0F12, 0x1B52},
    {0x0F12, 0x4694},
    {0x0F12, 0x4342},
    {0x0F12, 0x189B},
    {0x0F12, 0x4672},
    {0x0F12, 0x5053},
    {0x0F12, 0x0003},
    {0x0F12, 0x4343},
    {0x0F12, 0x4662},
    {0x0F12, 0x4353},
    {0x0F12, 0x482E},
    {0x0F12, 0x6840},
    {0x0F12, 0x5842},
    {0x0F12, 0x436A},
    {0x0F12, 0x3280},
    {0x0F12, 0x0A12},
    {0x0F12, 0x189A},
    {0x0F12, 0x5042},
    {0x0F12, 0x482A},
    {0x0F12, 0x6800},
    {0x0F12, 0x5840},
    {0x0F12, 0x0001},
    {0x0F12, 0x4341},
    {0x0F12, 0x3180},
    {0x0F12, 0x0A08},
    {0x0F12, 0x4290},
    {0x0F12, 0xD801},
    {0x0F12, 0x1A10},
    {0x0F12, 0x1986},
    {0x0F12, 0x1C64},
    {0x0F12, 0x4824},
    {0x0F12, 0x68C0},
    {0x0F12, 0x4284},
    {0x0F12, 0xD3C7},
    {0x0F12, 0x4C22},
    {0x0F12, 0x68E1},
    {0x0F12, 0x0848},
    {0x0F12, 0x1980},
    {0x0F12, 0xF000},
    {0x0F12, 0xF85A},
    {0x0F12, 0x3008},
    {0x0F12, 0x0900},
    {0x0F12, 0x6921},
    {0x0F12, 0x4288},
    {0x0F12, 0xD902},
    {0x0F12, 0x9800},
    {0x0F12, 0x8800},
    {0x0F12, 0x6160},
    {0x0F12, 0x491B},
    {0x0F12, 0x6948},
    {0x0F12, 0x2800},
    {0x0F12, 0xDD01},
    {0x0F12, 0x1E40},
    {0x0F12, 0x6148},
    {0x0F12, 0x4809},
    {0x0F12, 0x8A80},
    {0x0F12, 0x2800},
    {0x0F12, 0xD00C},
    {0x0F12, 0x4A19},
    {0x0F12, 0x7B90},
    {0x0F12, 0x2802},
    {0x0F12, 0xD001},
    {0x0F12, 0x2803},
    {0x0F12, 0xD106},
    {0x0F12, 0x6949},
    {0x0F12, 0x2900},
    {0x0F12, 0xDD03},
    {0x0F12, 0x2100},
    {0x0F12, 0x0040},
    {0x0F12, 0x1880},
    {0x0F12, 0x8081},
    {0x0F12, 0xE6D4},
    {0x0F12, 0x0C3C},
    {0x0F12, 0x7000},
    {0x0F12, 0x3364},
    {0x0F12, 0x7000},
    {0x0F12, 0x1D6C},
    {0x0F12, 0x7000},
    {0x0F12, 0x167C},
    {0x0F12, 0x7000},
    {0x0F12, 0xF400},
    {0x0F12, 0xD000},
    {0x0F12, 0x2C2C},
    {0x0F12, 0x7000},
    {0x0F12, 0x40A0},
    {0x0F12, 0x00DD},
    {0x0F12, 0xF520},
    {0x0F12, 0xD000},
    {0x0F12, 0x2C29},
    {0x0F12, 0x7000},
    {0x0F12, 0x1A54},
    {0x0F12, 0x7000},
    {0x0F12, 0x1564},
    {0x0F12, 0x7000},
    {0x0F12, 0xF2A0},
    {0x0F12, 0xD000},
    {0x0F12, 0x2894},
    {0x0F12, 0x7000},
    {0x0F12, 0x1224},
    {0x0F12, 0x7000},
    {0x0F12, 0xB000},
    {0x0F12, 0xD000},
    {0x0F12, 0x1E8C},
    {0x0F12, 0x7000},
    {0x0F12, 0x3240},
    {0x0F12, 0x7000},
    {0x0F12, 0x1EBC},
    {0x0F12, 0x7000},
    {0x0F12, 0x2050},
    {0x0F12, 0x7000},
    {0x0F12, 0x1D8C},
    {0x0F12, 0x7000},
    {0x0F12, 0x10E0},
    {0x0F12, 0x7000},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0x38E5},
    {0x0F12, 0x0000},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0x1A3F},
    {0x0F12, 0x0001},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xF004},
    {0x0F12, 0xE51F},
    {0xFFFF, 10},	    
    {0x0F12, 0x1F48},
    {0x0F12, 0x0001},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0x36ED},
    {0x0F12, 0x0000},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0xF53F},
    {0x0F12, 0x0000},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0xF5D9},
    {0x0F12, 0x0000},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0x013D},
    {0x0F12, 0x0001},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0xF5C9},
    {0x0F12, 0x0000},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0xFAA9},
    {0x0F12, 0x0000},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0x36DD},
    {0x0F12, 0x0000},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0xD771},
    {0x0F12, 0x0000},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0xD75B},
    {0x0F12, 0x0000},
    {0x0F12, 0x4778},
    {0x0F12, 0x46C0},
    {0x0F12, 0xC000},
    {0x0F12, 0xE59F},
    {0x0F12, 0xFF1C},
    {0x0F12, 0xE12F},
    {0x0F12, 0x6D1B},
    {0x0F12, 0x0000},
    {0x0F12, 0x82FA},
    {0x0F12, 0x0000},
    {0x002A, 0x021A},
    {0x0F12, 0x0000},
    {0x002A, 0x157A},
    {0x0F12, 0x0001},
    {0x002A, 0x1578},
    {0x0F12, 0x0001},
    {0x002A, 0x1576},
    {0x0F12, 0x0020},
    {0x002A, 0x1574},
    {0x0F12, 0x0006},
    {0x002A, 0x156E},
    {0x0F12, 0x0001},	
    {0x002A, 0x1568},
    {0x0F12, 0x00FC},
    {0x002A, 0x155A},
    {0x0F12, 0x01CC},	
    {0x002A, 0x157E},
    {0x0F12, 0x0C80},	
    {0x0F12, 0x0578},	
    {0x002A, 0x157C},
    {0x0F12, 0x0190},	
    {0x002A, 0x1570},
    {0x0F12, 0x00A0},	
    {0x0F12, 0x0010},	
    {0x002A, 0x12C4},
    {0x0F12, 0x006A},	
    {0x002A, 0x12C8},
    {0x0F12, 0x08AC},	
    {0x0F12, 0x0050},	
    {0x002A, 0x1696},
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x00C6},	
    {0x0F12, 0x00C6},	
    {0x002A, 0x12B8},
    {0x0F12, 0x1000},	
    {0x002A, 0x1690},
    {0x0F12, 0x0001},	
    {0x002A, 0x12B0},
    {0x0F12, 0x0055},	
    {0x0F12, 0x005A},	
    {0x002A, 0x337A},
    {0x0F12, 0x0006},	
    {0x0F12, 0x0068},	
    {0x002A, 0x169E},
    {0x0F12, 0x0007},	
    {0x002A, 0x336C},
    {0x0F12, 0x1000},	
    {0x0F12, 0x6998},	
    {0x0F12, 0x0078},	
    {0x0F12, 0x04FE},	
    {0x0F12, 0x8800},	

    {0x002A, 0x3364},
    {0x0F12, 0x0155},	
    {0x0F12, 0x0155},	
    {0x0F12, 0x1555},	
    {0x0F12, 0x0FFF},	

//Asserting CDS pointers
// Conditions: 10bit, AD
    {0x002A, 0x12D2},
    {0x0F12, 0x0003},	
    {0x002A, 0x12DA},
    {0x0F12, 0x0884},	
    {0x002A, 0x12E2},
    {0x0F12, 0x0001},	
    {0x002A, 0x12EA},
    {0x0F12, 0x0885},	
    {0x002A, 0x12F2},
    {0x0F12, 0x0001},	
    {0x002A, 0x12FA},
    {0x0F12, 0x0885},	
    {0x002A, 0x1302},
    {0x0F12, 0x0006},	
    {0x002A, 0x130A},
    {0x0F12, 0x0881},	
    {0x002A, 0x1312},
    {0x0F12, 0x0006},	
    {0x002A, 0x131A},
    {0x0F12, 0x0881},	
    {0x002A, 0x1322},
    {0x0F12, 0x03A2},	
    {0x002A, 0x132A},
    {0x0F12, 0x03F2},	
    {0x002A, 0x1332},
    {0x0F12, 0x03A2},	
    {0x002A, 0x133A},
    {0x0F12, 0x03F2},	
    {0x002A, 0x1342},
    {0x0F12, 0x0002},	
    {0x002A, 0x134A},
    {0x0F12, 0x003C},	
    {0x002A, 0x1352},
    {0x0F12, 0x01D3},	
    {0x002A, 0x135A},
    {0x0F12, 0x020B},	
    {0x002A, 0x1362},
    {0x0F12, 0x0002},	
    {0x002A, 0x136A},
    {0x0F12, 0x0419},	
    {0x002A, 0x1372},
    {0x0F12, 0x0630},	
    {0x002A, 0x137A},
    {0x0F12, 0x0668},	
    {0x002A, 0x1382},
    {0x0F12, 0x0001},	
    {0x002A, 0x138A},
    {0x0F12, 0x03A2},	
    {0x002A, 0x1392},
    {0x0F12, 0x0000},	
    {0x002A, 0x139A},
    {0x0F12, 0x0000},	
    {0x002A, 0x13A2},
    {0x0F12, 0x003D},	
    {0x002A, 0x13AA},
    {0x0F12, 0x01D0},	
    {0x002A, 0x13B2},
    {0x0F12, 0x020C},	
    {0x002A, 0x13BA},
    {0x0F12, 0x039F},	
    {0x002A, 0x13C2},
    {0x0F12, 0x041A},	
    {0x002A, 0x13CA},
    {0x0F12, 0x062D},	
    {0x002A, 0x13D2},
    {0x0F12, 0x0669},	
    {0x002A, 0x13DA},
    {0x0F12, 0x087C},	
    {0x002A, 0x13E2},
    {0x0F12, 0x0040},	
    {0x002A, 0x13EA},
    {0x0F12, 0x01D0},	
    {0x002A, 0x13F2},
    {0x0F12, 0x020F},	
    {0x002A, 0x13FA},
    {0x0F12, 0x039F},	
    {0x002A, 0x1402},
    {0x0F12, 0x041D},	
    {0x002A, 0x140A},
    {0x0F12, 0x062D},	
    {0x002A, 0x1412},
    {0x0F12, 0x066C},	
    {0x002A, 0x141A},
    {0x0F12, 0x087C},	
    {0x002A, 0x1422},
    {0x0F12, 0x0040},	
    {0x002A, 0x142A},
    {0x0F12, 0x01D0},	
    {0x002A, 0x1432},
    {0x0F12, 0x020F},	
    {0x002A, 0x143A},
    {0x0F12, 0x039F},	
    {0x002A, 0x1442},
    {0x0F12, 0x041D},	
    {0x002A, 0x144A},
    {0x0F12, 0x062D},	
    {0x002A, 0x1452},
    {0x0F12, 0x066C},	
    {0x002A, 0x145A},
    {0x0F12, 0x087C},	
    {0x002A, 0x1462},
    {0x0F12, 0x003D},	
    {0x002A, 0x146A},
    {0x0F12, 0x01D2},	
    {0x002A, 0x1472},
    {0x0F12, 0x020C},	
    {0x002A, 0x147A},
    {0x0F12, 0x03A1},	
    {0x002A, 0x1482},
    {0x0F12, 0x041A},	
    {0x002A, 0x148A},
    {0x0F12, 0x062F},	
    {0x002A, 0x1492},
    {0x0F12, 0x0669},	
    {0x002A, 0x149A},
    {0x0F12, 0x087E},	
    {0x002A, 0x14A2},
    {0x0F12, 0x03A2},	
    {0x002A, 0x14AA},
    {0x0F12, 0x03AF},	
    {0x002A, 0x14B2},
    {0x0F12, 0x0000},	
    {0x002A, 0x14BA},
    {0x0F12, 0x0000},	
    {0x002A, 0x14C2},
    {0x0F12, 0x0000},	
    {0x002A, 0x14CA},
    {0x0F12, 0x0000},	
    {0x002A, 0x14D2},
    {0x0F12, 0x0000},	
    {0x002A, 0x14DA},
    {0x0F12, 0x0000},	
    {0x002A, 0x14E2},
    {0x0F12, 0x03AA},	
    {0x002A, 0x14EA},
    {0x0F12, 0x03B7},	
    {0x002A, 0x14F2},
    {0x0F12, 0x0000},	
    {0x002A, 0x14FA},
    {0x0F12, 0x0000},	
    {0x002A, 0x1502},
    {0x0F12, 0x0000},	
    {0x002A, 0x150A},
    {0x0F12, 0x0000},	
    {0x002A, 0x1512},
    {0x0F12, 0x0000},	
    {0x002A, 0x151A},
    {0x0F12, 0x0000},	
    {0x002A, 0x1522},
    {0x0F12, 0x0001},	
    {0x002A, 0x152A},
    {0x0F12, 0x000F},	
    {0x002A, 0x1532},
    {0x0F12, 0x05AD},	
    {0xFFFF, 10},	    
    {0x002A, 0x153A},
    {0x0F12, 0x062F},	
    {0x002A, 0x1542},
    {0x0F12, 0x07FC},	
    {0x002A, 0x154A},
    {0x0F12, 0x0000},	

//Asserting CDS pointers
// Conditions: 10bit, AD
    {0x002A, 0x12D4},
    {0x0F12, 0x0003},	
    {0x002A, 0x12DC},
    {0x0F12, 0x08CF},	
    {0x002A, 0x12E4},
    {0x0F12, 0x0001},	
    {0x002A, 0x12EC},
    {0x0F12, 0x0467},	
    {0x002A, 0x12F4},
    {0x0F12, 0x046A},	
    {0x002A, 0x12FC},
    {0x0F12, 0x08D0},	
    {0x002A, 0x1304},
    {0x0F12, 0x0020},	
    {0x002A, 0x130C},
    {0x0F12, 0x0463},	
    {0x002A, 0x1314},
    {0x0F12, 0x0489},	
    {0x002A, 0x131C},
    {0x0F12, 0x08CC},	
    {0x002A, 0x1324},
    {0x0F12, 0x01D3},	
    {0x002A, 0x132C},
    {0x0F12, 0x0223},	
    {0x002A, 0x1334},
    {0x0F12, 0x063C},	
    {0x002A, 0x133C},
    {0x0F12, 0x068C},	
    {0x002A, 0x1344},
    {0x0F12, 0x0002},	
    {0x002A, 0x134C},
    {0x0F12, 0x003C},	
    {0x002A, 0x1354},
    {0x0F12, 0x01D3},	
    {0x002A, 0x135C},
    {0x0F12, 0x024A},	
    {0x002A, 0x1364},
    {0x0F12, 0x046B},	
    {0x002A, 0x136C},
    {0x0F12, 0x04A5},	
    {0x002A, 0x1374},
    {0x0F12, 0x063C},	
    {0x002A, 0x137C},
    {0x0F12, 0x06B3},	
    {0x002A, 0x1384},
    {0x0F12, 0x0001},	
    {0x002A, 0x138C},
    {0x0F12, 0x01D3},	
    {0x002A, 0x1394},
    {0x0F12, 0x0461},	
    {0x002A, 0x139C},
    {0x0F12, 0x063C},	
    {0x002A, 0x13A4},
    {0x0F12, 0x003D},	
    {0x002A, 0x13AC},
    {0x0F12, 0x01D0},	
    {0x002A, 0x13B4},
    {0x0F12, 0x024B},	
    {0x002A, 0x13BC},
    {0x0F12, 0x045E},	
    {0x002A, 0x13C4},
    {0x0F12, 0x04A6},	
    {0x002A, 0x13CC},
    {0x0F12, 0x0639},	
    {0x002A, 0x13D4},
    {0x0F12, 0x06B4},	
    {0x002A, 0x13DC},
    {0x0F12, 0x08C7},	
    {0x002A, 0x13E4},
    {0x0F12, 0x0040},	
    {0x002A, 0x13EC},
    {0x0F12, 0x01D0},	
    {0x002A, 0x13F4},
    {0x0F12, 0x024E},	
    {0x002A, 0x13FC},
    {0x0F12, 0x045E},	
    {0x002A, 0x1404},
    {0x0F12, 0x04A9},	
    {0x002A, 0x140C},
    {0x0F12, 0x0639},	
    {0x002A, 0x1414},
    {0x0F12, 0x06B7},	
    {0x002A, 0x141C},
    {0x0F12, 0x08C7},	
    {0x002A, 0x1424},
    {0x0F12, 0x0040},	
    {0x002A, 0x142C},
    {0x0F12, 0x01D0},	
    {0x002A, 0x1434},
    {0x0F12, 0x024E},	
    {0x002A, 0x143C},
    {0x0F12, 0x045E},	
    {0x002A, 0x1444},
    {0x0F12, 0x04A9},	
    {0x002A, 0x144C},
    {0x0F12, 0x0639},	
    {0x002A, 0x1454},
    {0x0F12, 0x06B7},	
    {0x002A, 0x145C},
    {0x0F12, 0x08C7},	
    {0x002A, 0x1464},
    {0x0F12, 0x003D},	
    {0x002A, 0x146C},
    {0x0F12, 0x01D2},	
    {0x002A, 0x1474},
    {0x0F12, 0x024B},	
    {0x002A, 0x147C},
    {0x0F12, 0x0460},	
    {0x002A, 0x1484},
    {0x0F12, 0x04A6},	
    {0x002A, 0x148C},
    {0x0F12, 0x063B},	
    {0x002A, 0x1494},
    {0x0F12, 0x06B4},	
    {0x002A, 0x149C},
    {0x0F12, 0x08C9},	
    {0x002A, 0x14A4},
    {0x0F12, 0x01D3},	
    {0x002A, 0x14AC},
    {0x0F12, 0x01E0},	
    {0x002A, 0x14B4},
    {0x0F12, 0x0461},	
    {0x002A, 0x14BC},
    {0x0F12, 0x046E},	
    {0x002A, 0x14C4},
    {0x0F12, 0x063C},	
    {0x002A, 0x14CC},
    {0x0F12, 0x0649},	
    {0x002A, 0x14D4},
    {0x0F12, 0x0000},	
    {0x002A, 0x14DC},
    {0x0F12, 0x0000},	
    {0x002A, 0x14E4},
    {0x0F12, 0x01DB},	
    {0x002A, 0x14EC},
    {0x0F12, 0x01E8},	
    {0x002A, 0x14F4},
    {0x0F12, 0x0469},	
    {0x002A, 0x14FC},
    {0x0F12, 0x0476},	
    {0x002A, 0x1504},
    {0x0F12, 0x0644},	
    {0x002A, 0x150C},
    {0x0F12, 0x0651},	
    {0x002A, 0x1514},
    {0x0F12, 0x0000},	
    {0x002A, 0x151C},
    {0x0F12, 0x0000},	
    {0x002A, 0x1524},
    {0x0F12, 0x0001},	
    {0x002A, 0x152C},
    {0x0F12, 0x000F},	
    {0x002A, 0x1534},
    {0x0F12, 0x03DE},	
    {0x002A, 0x153C},
    {0x0F12, 0x0460},	
    {0x002A, 0x1544},
    {0x0F12, 0x0847},	
    {0x002A, 0x154C},
    {0x0F12, 0x0000},	

//Asserting CDS pointers
// Conditions: 10bit, AD

    {0x002A, 0x12D6},
    {0x0F12, 0x0003},	
    {0x002A, 0x12DE},
    {0x0F12, 0x0500},	
    {0x002A, 0x12E6},
    {0x0F12, 0x0001},	
    {0x002A, 0x12EE},
    {0x0F12, 0x0501},	
    {0x002A, 0x12F6},
    {0x0F12, 0x0001},	
    {0x002A, 0x12FE},
    {0x0F12, 0x0501},	
    {0x002A, 0x1306},
    {0x0F12, 0x0006},	
    {0x002A, 0x130E},
    {0x0F12, 0x04FD},	
    {0x002A, 0x1316},
    {0x0F12, 0x0006},	
    {0x002A, 0x131E},
    {0x0F12, 0x04FD},	
    {0x002A, 0x1326},
    {0x0F12, 0x01E0},	
    {0x002A, 0x132E},
    {0x0F12, 0x0230},	
    {0x002A, 0x1336},
    {0x0F12, 0x01E0},	
    {0x002A, 0x133E},
    {0x0F12, 0x0230},	
    {0x002A, 0x1346},
    {0x0F12, 0x0002},	
    {0x002A, 0x134E},
    {0x0F12, 0x003C},	
    {0x002A, 0x1356},
    {0x0F12, 0x00F2},	
    {0x002A, 0x135E},
    {0x0F12, 0x012A},	
    {0x002A, 0x1366},
    {0x0F12, 0x0002},	
    {0x002A, 0x136E},
    {0x0F12, 0x0257},	
    {0x002A, 0x1376},
    {0x0F12, 0x038D},	
    {0x002A, 0x137E},
    {0x0F12, 0x03C5},	
    {0x002A, 0x1386},
    {0x0F12, 0x0001},	
    {0x002A, 0x138E},
    {0x0F12, 0x01E0},	
    {0x002A, 0x1396},
    {0x0F12, 0x0000},	
    {0x002A, 0x139E},
    {0x0F12, 0x0000},	
    {0x002A, 0x13A6},
    {0x0F12, 0x003D},	
    {0x002A, 0x13AE},
    {0x0F12, 0x00EF},	
    {0x002A, 0x13B6},
    {0x0F12, 0x012B},	
    {0x002A, 0x13BE},
    {0x0F12, 0x01DD},	
    {0x002A, 0x13C6},
    {0x0F12, 0x0258},	
    {0x002A, 0x13CE},
    {0x0F12, 0x038A},	
    {0x002A, 0x13D6},
    {0x0F12, 0x03C6},	
    {0x002A, 0x13DE},
    {0x0F12, 0x04F8},	
    {0x002A, 0x13E6},
    {0x0F12, 0x0040},	
    {0x002A, 0x13EE},
    {0x0F12, 0x00EF},	
    {0x002A, 0x13F6},
    {0x0F12, 0x012E},	
    {0x002A, 0x13FE},
    {0x0F12, 0x01DD},	
    {0x002A, 0x1406},
    {0x0F12, 0x025B},	
    {0x002A, 0x140E},
    {0x0F12, 0x038A},	
    {0x002A, 0x1416},
    {0x0F12, 0x03C9},	
    {0x002A, 0x141E},
    {0x0F12, 0x04F8},	
    {0x002A, 0x1426},
    {0x0F12, 0x0040},	
    {0x002A, 0x142E},
    {0x0F12, 0x00EF},	
    {0x002A, 0x1436},
    {0x0F12, 0x012E},	
    {0x002A, 0x143E},
    {0x0F12, 0x01DD},	
    {0x002A, 0x1446},
    {0x0F12, 0x025B},	
    {0x002A, 0x144E},
    {0x0F12, 0x038A},	
    {0x002A, 0x1456},
    {0x0F12, 0x03C9},	
    {0x002A, 0x145E},
    {0x0F12, 0x04F8},	
    {0x002A, 0x1466},
    {0x0F12, 0x003D},	
    {0x002A, 0x146E},
    {0x0F12, 0x00F1},	
    {0x002A, 0x1476},
    {0x0F12, 0x012B},	
    {0x002A, 0x147E},
    {0x0F12, 0x01DF},	
    {0x002A, 0x1486},
    {0x0F12, 0x0258},	
    {0x002A, 0x148E},
    {0x0F12, 0x038C},	
    {0x002A, 0x1496},
    {0x0F12, 0x03C6},	
    {0x002A, 0x149E},
    {0x0F12, 0x04FA},	
    {0x002A, 0x14A6},
    {0x0F12, 0x01E0},	
    {0x002A, 0x14AE},
    {0x0F12, 0x01ED},	
    {0x002A, 0x14B6},
    {0x0F12, 0x0000},	
    {0x002A, 0x14BE},
    {0x0F12, 0x0000},	
    {0x002A, 0x14C6},
    {0x0F12, 0x0000},	
    {0xFFFF, 10},	
    
    {0x002A, 0x14CE},
    {0x0F12, 0x0000},	
    {0x002A, 0x14D6},
    {0x0F12, 0x0000},	
    {0x002A, 0x14DE},
    {0x0F12, 0x0000},	
    {0x002A, 0x14E6},
    {0x0F12, 0x01E8},	
    {0x002A, 0x14EE},
    {0x0F12, 0x01F5},	
    {0x002A, 0x14F6},
    {0x0F12, 0x0000},	
    {0x002A, 0x14FE},
    {0x0F12, 0x0000},	
    {0x002A, 0x1506},
    {0x0F12, 0x0000},	
    {0x002A, 0x150E},
    {0x0F12, 0x0000},	
    {0x002A, 0x1516},
    {0x0F12, 0x0000},	
    {0x002A, 0x151E},
    {0x0F12, 0x0000},	
    {0x002A, 0x1526},
    {0x0F12, 0x0001},	
    {0x002A, 0x152E},
    {0x0F12, 0x000F},	
    {0x002A, 0x1536},
    {0x0F12, 0x030A},	
    {0x002A, 0x153E},
    {0x0F12, 0x038C},	
    {0x002A, 0x1546},
    {0x0F12, 0x0478},	
    {0x002A, 0x154E},
    {0x0F12, 0x0000},	

//Asserting CDS pointers
// Conditions: 10bit, AD

    {0x002A, 0x12D8},
    {0x0F12, 0x0003},	
    {0x002A, 0x12E0},
    {0x0F12, 0x054B},	
    {0x002A, 0x12E8},
    {0x0F12, 0x0001},	
    {0x002A, 0x12F0},
    {0x0F12, 0x02A5},	
    {0x002A, 0x12F8},
    {0x0F12, 0x02A8},	
    {0x002A, 0x1300},
    {0x0F12, 0x054C},	
    {0x002A, 0x1308},
    {0x0F12, 0x0020},	
    {0x002A, 0x1310},
    {0x0F12, 0x02A1},	
    {0x002A, 0x1318},
    {0x0F12, 0x02C7},	
    {0x002A, 0x1320},
    {0x0F12, 0x0548},	
    {0x002A, 0x1328},
    {0x0F12, 0x00F2},	
    {0x002A, 0x1330},
    {0x0F12, 0x0142},	
    {0x002A, 0x1338},
    {0x0F12, 0x0399},	
    {0x002A, 0x1340},
    {0x0F12, 0x03E9},	
    {0x002A, 0x1348},
    {0x0F12, 0x0002},	
    {0x002A, 0x1350},
    {0x0F12, 0x003C},	
    {0x002A, 0x1358},
    {0x0F12, 0x00F2},	
    {0x002A, 0x1360},
    {0x0F12, 0x0169},	
    {0x002A, 0x1368},
    {0x0F12, 0x02A9},	
    {0x002A, 0x1370},
    {0x0F12, 0x02E3},	
    {0x002A, 0x1378},
    {0x0F12, 0x0399},	
    {0x002A, 0x1380},
    {0x0F12, 0x0410},	
    {0x002A, 0x1388},
    {0x0F12, 0x0001},	
    {0x002A, 0x1390},
    {0x0F12, 0x00F2},	
    {0x002A, 0x1398},
    {0x0F12, 0x029F},	
    {0x002A, 0x13A0},
    {0x0F12, 0x0399},	
    {0x002A, 0x13A8},
    {0x0F12, 0x003D},	
    {0x002A, 0x13B0},
    {0x0F12, 0x00EF},	
    {0x002A, 0x13B8},
    {0x0F12, 0x016A},	
    {0x002A, 0x13C0},
    {0x0F12, 0x029C},	
    {0x002A, 0x13C8},
    {0x0F12, 0x02E4},	
    {0x002A, 0x13D0},
    {0x0F12, 0x0396},	
    {0x002A, 0x13D8},
    {0x0F12, 0x0411},	
    {0x002A, 0x13E0},
    {0x0F12, 0x0543},	
    {0x002A, 0x13E8},
    {0x0F12, 0x0040},	
    {0x002A, 0x13F0},
    {0x0F12, 0x00EF},	
    {0x002A, 0x13F8},
    {0x0F12, 0x016D},	
    {0x002A, 0x1400},
    {0x0F12, 0x029C},	
    {0x002A, 0x1408},
    {0x0F12, 0x02E7},	
    {0x002A, 0x1410},
    {0x0F12, 0x0396},	
    {0x002A, 0x1418},
    {0x0F12, 0x0414},	
    {0x002A, 0x1420},
    {0x0F12, 0x0543},	
    {0x002A, 0x1428},
    {0x0F12, 0x0040},	
    {0x002A, 0x1430},
    {0x0F12, 0x00EF},	
    {0x002A, 0x1438},
    {0x0F12, 0x016D},	
    {0x002A, 0x1440},
    {0x0F12, 0x029C},	
    {0x002A, 0x1448},
    {0x0F12, 0x02E7},	
    {0x002A, 0x1450},
    {0x0F12, 0x0396},	
    {0x002A, 0x1458},
    {0x0F12, 0x0414},	
    {0x002A, 0x1460},
    {0x0F12, 0x0543},	
    {0x002A, 0x1468},
    {0x0F12, 0x003D},	
    {0x002A, 0x1470},
    {0x0F12, 0x00F1},	
    {0x002A, 0x1478},
    {0x0F12, 0x016A},	
    {0x002A, 0x1480},
    {0x0F12, 0x029E},	
    {0x002A, 0x1488},
    {0x0F12, 0x02E4},	
    {0x002A, 0x1490},
    {0x0F12, 0x0398},	
    {0x002A, 0x1498},
    {0x0F12, 0x0411},	
    {0x002A, 0x14A0},
    {0x0F12, 0x0545},	
    {0x002A, 0x14A8},
    {0x0F12, 0x00F2},	
    {0x002A, 0x14B0},
    {0x0F12, 0x00FF},	
    {0x002A, 0x14B8},
    {0x0F12, 0x029F},	
    {0x002A, 0x14C0},
    {0x0F12, 0x02AC},	
    {0x002A, 0x14C8},
    {0x0F12, 0x0399},	
    {0x002A, 0x14D0},
    {0x0F12, 0x03A6},	
    {0x002A, 0x14D8},
    {0x0F12, 0x0000},	
    {0x002A, 0x14E0},
    {0x0F12, 0x0000},	
    {0x002A, 0x14E8},
    {0x0F12, 0x00FA},	
    {0x002A, 0x14F0},
    {0x0F12, 0x0107},	
    {0x002A, 0x14F8},
    {0x0F12, 0x02A7},	
    {0x002A, 0x1500},
    {0x0F12, 0x02B4},	
    {0x002A, 0x1508},
    {0x0F12, 0x03A1},	
    {0x002A, 0x1510},
    {0x0F12, 0x03AE},	
    {0x002A, 0x1518},
    {0x0F12, 0x0000},	
    {0x002A, 0x1520},
    {0x0F12, 0x0000},	
    {0x002A, 0x1528},
    {0x0F12, 0x0001},	
    {0x002A, 0x1530},
    {0x0F12, 0x000F},	
    {0x002A, 0x1538},
    {0x0F12, 0x021C},	
    {0x002A, 0x1540},
    {0x0F12, 0x029E},	
    {0x002A, 0x1548},
    {0x0F12, 0x04C3},	
    {0x002A, 0x1550},
    {0x0F12, 0x0000},	

//======================
// ISP-FE Setting
//======================
    {0x002A, 0x158A},
    {0x0F12, 0xEAF0},
    {0x002A, 0x15C6},
    {0x0F12, 0x0020},
    {0x0F12, 0x0060},
    {0x002A, 0x15BC},
    {0x0F12, 0x0200},

//Analog Offset for MSM
    {0x002A, 0x1608},
    {0x0F12, 0x0100},	
    {0x0F12, 0x0100},	
    {0x0F12, 0x0100},	
    {0x0F12, 0x0100},	

//======================
// ISP-FE Setting END
//======================
//======================
// SET JPEG & SPOOF
//======================
    {0x002A, 0x0454},
    {0x0F12, 0x0055},	
//======================
// SET THUMBNAIL
// # Foramt : RGB565
// # Size: VGA
//======================
    {0x0F12, 0x0000},	
    {0x0F12, 0x0140},	
    {0x0F12, 0x00F0},	
    {0x0F12, 0x0000},	

//======================
// SET AE
//======================
// AE target
    {0x002A, 0x0F70},
    {0x0F12, 0x0040},	
// AE mode
    {0x002A, 0x0F76},
    {0x0F12, 0x000F},	
// AE weight
    {0x002A, 0x0F7E},
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0303},	
    {0x0F12, 0x0303},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0303},	
    {0x0F12, 0x0303},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0303},	
    {0x0F12, 0x0303},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0303},	
    {0x0F12, 0x0303},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0101},	

//======================
// SET FLICKER
//======================
    {0x002A, 0x0C18},
    {0x0F12, 0x0001},	
    {0x002A, 0x04D2},
    {0x0F12, 0x067F},

//======================
// SET GAS
//======================
// GAS alpha
// R, Gr, Gb, B per ligh
    {0x002A, 0x06CE},   
    {0x0F12, 0x00F0},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x00F0},   
    {0xFFFF, 10},	
    
    {0x0F12, 0x0113},   
    {0x0F12, 0x011D},   
    {0x0F12, 0x0190},   
    {0x0F12, 0x00F0},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x00D0},   
    {0x0F12, 0x00F0},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x00F0},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0120},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x00A0},   
    {0x0F12, 0x00F8},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
    {0x0F12, 0x0100},   
// GAS beta
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0010},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	

// Parabloic function
    {0x002A, 0x075A},
    {0x0F12, 0x0000},	
    {0x0F12, 0x0400},	
    {0x0F12, 0x0300},	
    {0x0F12, 0x0010},	
    {0x0F12, 0x0011},	

//ash_CGrasAlphas
    {0x002A, 0x06C6},
    {0x0F12, 0x010B},	
    {0x0F12, 0x0103},	
    {0x0F12, 0x00FC},	
    {0x0F12, 0x010C},	

    {0x002A, 0x074E},
    {0x0F12, 0x0001},	
    {0x002A, 0x0D30},
    {0x0F12, 0x02A8},	
    {0x0F12, 0x0347},	

// GAS LUT start address
    {0x002A, 0x0754},
    {0x0F12, 0x347C},
    {0x0F12, 0x7000},
// GAS LUT
    {0x002A, 0x347C},
    {0x0F12, 0x01D5},	
    {0x0F12, 0x018A},	
    {0x0F12, 0x014B},	
    {0x0F12, 0x010E},	
    {0x0F12, 0x00E6},	
    {0x0F12, 0x00D3},	
    {0x0F12, 0x00D0},	
    {0x0F12, 0x00DE},	
    {0x0F12, 0x00FD},	
    {0x0F12, 0x013A},	
    {0x0F12, 0x0180},	
    {0x0F12, 0x01D2},	
    {0x0F12, 0x0259},	
    {0x0F12, 0x0196},	
    {0x0F12, 0x014E},	
    {0x0F12, 0x0104},	
    {0x0F12, 0x00CB},	
    {0x0F12, 0x00A0},	
    {0x0F12, 0x008B},	
    {0x0F12, 0x0087},	
    {0x0F12, 0x0096},	
    {0x0F12, 0x00BB},	
    {0x0F12, 0x00F6},	
    {0x0F12, 0x013D},	
    {0x0F12, 0x0196},	
    {0x0F12, 0x01F8},	
    {0x0F12, 0x0162},	
    {0x0F12, 0x0116},	
    {0x0F12, 0x00C3},	
    {0x0F12, 0x008D},	
    {0x0F12, 0x0064},	
    {0x0F12, 0x004E},	
    {0x0F12, 0x004A},	
    {0x0F12, 0x0059},	
    {0x0F12, 0x007C},	
    {0x0F12, 0x00B3},	
    {0x0F12, 0x00FE},	
    {0x0F12, 0x0162},	
    {0x0F12, 0x01BF},	
    {0x0F12, 0x013E},	
    {0x0F12, 0x00E9},	
    {0x0F12, 0x009A},	
    {0x0F12, 0x0060},	
    {0x0F12, 0x0038},	
    {0x0F12, 0x0023},	
    {0x0F12, 0x001F},	
    {0x0F12, 0x002E},	
    {0x0F12, 0x0051},	
    {0x0F12, 0x0088},	
    {0x0F12, 0x00D5},	
    {0x0F12, 0x0140},	
    {0x0F12, 0x01A1},	
    {0x0F12, 0x0129},	
    {0x0F12, 0x00CE},	
    {0x0F12, 0x007E},	
    {0x0F12, 0x0046},	
    {0x0F12, 0x001E},	
    {0x0F12, 0x000A},	
    {0x0F12, 0x0006},	
    {0x0F12, 0x0016},	
    {0x0F12, 0x0038},	
    {0x0F12, 0x0072},	
    {0x0F12, 0x00C2},	
    {0x0F12, 0x012D},	
    {0x0F12, 0x0197},	
    {0x0F12, 0x0124},	
    {0x0F12, 0x00C7},	
    {0x0F12, 0x0075},	
    {0x0F12, 0x003E},	
    {0x0F12, 0x0018},	
    {0x0F12, 0x0003},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0010},	
    {0x0F12, 0x0033},	
    {0x0F12, 0x006E},	
    {0x0F12, 0x00C0},	
    {0x0F12, 0x0131},	
    {0x0F12, 0x019F},	
    {0x0F12, 0x0136},	
    {0x0F12, 0x00D7},	
    {0x0F12, 0x0085},	
    {0x0F12, 0x004B},	
    {0x0F12, 0x0024},	
    {0x0F12, 0x000E},	
    {0x0F12, 0x000C},	
    {0x0F12, 0x001D},	
    {0x0F12, 0x0040},	
    {0x0F12, 0x007B},	
    {0x0F12, 0x00CE},	
    {0x0F12, 0x0147},	
    {0x0F12, 0x01B4},	
    {0x0F12, 0x015B},	
    {0x0F12, 0x00FE},	
    {0x0F12, 0x00A6},	
    {0x0F12, 0x0069},	
    {0x0F12, 0x0041},	
    {0x0F12, 0x002C},	
    {0x0F12, 0x002A},	
    {0x0F12, 0x003C},	
    {0x0F12, 0x0060},	
    {0x0F12, 0x009C},	
    {0x0F12, 0x00F2},	
    {0x0F12, 0x0169},	
    {0x0F12, 0x01D3},	
    {0x0F12, 0x0190},	
    {0x0F12, 0x0137},	
    {0x0F12, 0x00DB},	
    {0x0F12, 0x009E},	
    {0x0F12, 0x0072},	
    {0x0F12, 0x005D},	
    {0x0F12, 0x005C},	
    {0x0F12, 0x006F},	
    {0x0F12, 0x0099},	
    {0x0F12, 0x00D8},	
    {0x0F12, 0x0133},	
    {0x0F12, 0x01A4},	
    {0x0F12, 0x020B},	
    {0x0F12, 0x01D9},	
    {0x0F12, 0x0185},	
    {0x0F12, 0x0133},	
    {0x0F12, 0x00ED},	
    {0x0F12, 0x00BD},	
    {0x0F12, 0x00A9},	
    {0x0F12, 0x00A6},	
    {0x0F12, 0x00BA},	
    {0x0F12, 0x00E7},	
    {0x0F12, 0x0132},	
    {0x0F12, 0x018B},	
    {0x0F12, 0x01F1},	
    {0x0F12, 0x0272},	
    {0x0F12, 0x022D},	
    {0x0F12, 0x01D6},	
    {0x0F12, 0x0188},	
    {0x0F12, 0x0147},	
    {0x0F12, 0x0119},	
    {0x0F12, 0x0107},	
    {0x0F12, 0x0105},	
    {0x0F12, 0x0119},	
    {0x0F12, 0x0146},	
    {0x0F12, 0x018D},	
    {0x0F12, 0x01E1},	
    {0x0F12, 0x0248},	
    {0x0F12, 0x02F2},	
    {0x0F12, 0x019B},	
    {0x0F12, 0x0156},	
    {0x0F12, 0x011B},	
    {0x0F12, 0x00E6},	
    {0x0F12, 0x00C0},	
    {0x0F12, 0x00AD},	
    {0x0F12, 0x00A8},	
    {0x0F12, 0x00B4},	
    {0x0F12, 0x00D1},	
    {0x0F12, 0x0104},	
    {0x0F12, 0x013C},	
    {0x0F12, 0x017F},	
    {0x0F12, 0x01F1},	
    {0x0F12, 0x0165},	
    {0x0F12, 0x011F},	
    {0x0F12, 0x00DE},	
    {0x0F12, 0x00AC},	
    {0x0F12, 0x0084},	
    {0x0F12, 0x0070},	
    {0x0F12, 0x006C},	
    {0x0F12, 0x0079},	
    {0x0F12, 0x0099},	
    {0x0F12, 0x00CA},	
    {0x0F12, 0x0102},	
    {0x0F12, 0x014C},	
    {0x0F12, 0x01A0},	
    {0x0F12, 0x0139},	
    {0x0F12, 0x00EF},	
    {0x0F12, 0x00A7},	
    {0x0F12, 0x0079},	
    {0x0F12, 0x0053},	
    {0x0F12, 0x003F},	
    {0x0F12, 0x003B},	
    {0x0F12, 0x0048},	
    {0x0F12, 0x0067},	
    {0x0F12, 0x0093},	
    {0x0F12, 0x00D0},	
    {0x0F12, 0x0123},	
    {0x0F12, 0x0173},	
    {0x0F12, 0x0119},	
    {0x0F12, 0x00C9},	
    {0x0F12, 0x0085},	
    {0x0F12, 0x0054},	
    {0x0F12, 0x0030},	
    {0x0F12, 0x001C},	
    {0x0F12, 0x0018},	
    {0x0F12, 0x0026},	
    {0x0F12, 0x0045},	
    {0x0F12, 0x0072},	
    {0x0F12, 0x00AF},	
    {0x0F12, 0x0106},	
    {0x0F12, 0x015B},	
    {0x0F12, 0x0104},	
    {0x0F12, 0x00B1},	
    {0x0F12, 0x006D},	
    {0x0F12, 0x003E},	
    {0x0F12, 0x001A},	
    {0x0F12, 0x0007},	
    {0x0F12, 0x0004},	
    {0x0F12, 0x0013},	
    {0x0F12, 0x0032},	
    {0x0F12, 0x0061},	
    {0x0F12, 0x00A0},	
    {0x0F12, 0x00F8},	
    {0x0F12, 0x0152},	
    {0x0F12, 0x00FE},	
    {0x0F12, 0x00A9},	
    {0xFFFF, 10},	
    
    {0x0F12, 0x0065},	
    {0x0F12, 0x0037},	
    {0x0F12, 0x0015},	
    {0x0F12, 0x0002},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0010},	
    {0x0F12, 0x002F},	
    {0x0F12, 0x005F},	
    {0x0F12, 0x00A0},	
    {0x0F12, 0x00FC},	
    {0x0F12, 0x0159},	
    {0x0F12, 0x010C},	
    {0x0F12, 0x00B5},	
    {0x0F12, 0x0072},	
    {0x0F12, 0x0042},	
    {0x0F12, 0x001F},	
    {0x0F12, 0x000D},	
    {0x0F12, 0x000C},	
    {0x0F12, 0x001C},	
    {0x0F12, 0x003C},	
    {0x0F12, 0x006B},	
    {0x0F12, 0x00AE},	
    {0x0F12, 0x010E},	
    {0x0F12, 0x016C},	
    {0x0F12, 0x012C},	
    {0x0F12, 0x00D6},	
    {0x0F12, 0x008E},	
    {0x0F12, 0x005B},	
    {0x0F12, 0x0039},	
    {0x0F12, 0x0027},	
    {0x0F12, 0x0026},	
    {0x0F12, 0x0038},	
    {0x0F12, 0x0057},	
    {0x0F12, 0x0087},	
    {0x0F12, 0x00CC},	
    {0x0F12, 0x012C},	
    {0x0F12, 0x0186},	
    {0x0F12, 0x0159},	
    {0x0F12, 0x0108},	
    {0x0F12, 0x00BB},	
    {0x0F12, 0x0089},	
    {0x0F12, 0x0064},	
    {0x0F12, 0x0052},	
    {0x0F12, 0x0052},	
    {0x0F12, 0x0064},	
    {0x0F12, 0x0089},	
    {0x0F12, 0x00BC},	
    {0x0F12, 0x0104},	
    {0x0F12, 0x0160},	
    {0x0F12, 0x01BA},	
    {0x0F12, 0x0198},	
    {0x0F12, 0x014B},	
    {0x0F12, 0x0108},	
    {0x0F12, 0x00CC},	
    {0x0F12, 0x00A1},	
    {0x0F12, 0x0090},	
    {0x0F12, 0x0090},	
    {0x0F12, 0x00A3},	
    {0x0F12, 0x00CB},	
    {0x0F12, 0x0109},	
    {0x0F12, 0x0150},	
    {0x0F12, 0x01A3},	
    {0x0F12, 0x0211},	
    {0x0F12, 0x01E3},	
    {0x0F12, 0x0192},	
    {0x0F12, 0x014E},	
    {0x0F12, 0x0115},	
    {0x0F12, 0x00ED},	
    {0x0F12, 0x00DC},	
    {0x0F12, 0x00DC},	
    {0x0F12, 0x00F0},	
    {0x0F12, 0x011A},	
    {0x0F12, 0x0158},	
    {0x0F12, 0x019C},	
    {0x0F12, 0x01F1},	
    {0x0F12, 0x027E},	
    {0x0F12, 0x01A1},	
    {0x0F12, 0x015A},	
    {0x0F12, 0x011E},	
    {0x0F12, 0x00E7},	
    {0x0F12, 0x00C1},	
    {0x0F12, 0x00B1},	
    {0x0F12, 0x00B0},	
    {0x0F12, 0x00C2},	
    {0x0F12, 0x00E5},	
    {0x0F12, 0x011D},	
    {0x0F12, 0x015B},	
    {0x0F12, 0x01A2},	
    {0x0F12, 0x0217},	
    {0x0F12, 0x016B},	
    {0x0F12, 0x0123},	
    {0x0F12, 0x00E0},	
    {0x0F12, 0x00AD},	
    {0x0F12, 0x0087},	
    {0x0F12, 0x0075},	
    {0x0F12, 0x0074},	
    {0x0F12, 0x0085},	
    {0x0F12, 0x00AB},	
    {0x0F12, 0x00E1},	
    {0x0F12, 0x011E},	
    {0x0F12, 0x016B},	
    {0x0F12, 0x01C2},	
    {0x0F12, 0x013F},	
    {0x0F12, 0x00F3},	
    {0x0F12, 0x00AA},	
    {0x0F12, 0x007B},	
    {0x0F12, 0x0056},	
    {0x0F12, 0x0043},	
    {0x0F12, 0x0042},	
    {0x0F12, 0x0052},	
    {0x0F12, 0x0076},	
    {0x0F12, 0x00A6},	
    {0x0F12, 0x00E6},	
    {0x0F12, 0x013D},	
    {0x0F12, 0x018F},	
    {0x0F12, 0x0120},	
    {0x0F12, 0x00CE},	
    {0x0F12, 0x0089},	
    {0x0F12, 0x0057},	
    {0x0F12, 0x0033},	
    {0x0F12, 0x001F},	
    {0x0F12, 0x001D},	
    {0x0F12, 0x002D},	
    {0x0F12, 0x004F},	
    {0x0F12, 0x0080},	
    {0x0F12, 0x00C0},	
    {0x0F12, 0x011A},	
    {0x0F12, 0x0170},	
    {0x0F12, 0x010D},	
    {0x0F12, 0x00B8},	
    {0x0F12, 0x0073},	
    {0x0F12, 0x0043},	
    {0x0F12, 0x001E},	
    {0x0F12, 0x000A},	
    {0x0F12, 0x0007},	
    {0x0F12, 0x0018},	
    {0x0F12, 0x0039},	
    {0x0F12, 0x006A},	
    {0x0F12, 0x00AB},	
    {0x0F12, 0x0105},	
    {0x0F12, 0x0161},	
    {0x0F12, 0x010A},	
    {0x0F12, 0x00B3},	
    {0x0F12, 0x006E},	
    {0x0F12, 0x003E},	
    {0x0F12, 0x001A},	
    {0x0F12, 0x0005},	
    {0x0F12, 0x0002},	
    {0x0F12, 0x0012},	
    {0x0F12, 0x0033},	
    {0x0F12, 0x0063},	
    {0x0F12, 0x00A5},	
    {0x0F12, 0x0101},	
    {0x0F12, 0x0161},	
    {0x0F12, 0x011C},	
    {0x0F12, 0x00C2},	
    {0x0F12, 0x007E},	
    {0x0F12, 0x004C},	
    {0x0F12, 0x0026},	
    {0x0F12, 0x0010},	
    {0x0F12, 0x000D},	
    {0x0F12, 0x001D},	
    {0x0F12, 0x003B},	
    {0x0F12, 0x006A},	
    {0x0F12, 0x00AD},	
    {0x0F12, 0x010D},	
    {0x0F12, 0x016D},	
    {0x0F12, 0x013F},	
    {0x0F12, 0x00E7},	
    {0x0F12, 0x009D},	
    {0x0F12, 0x0067},	
    {0x0F12, 0x0041},	
    {0x0F12, 0x002C},	
    {0x0F12, 0x0027},	
    {0x0F12, 0x0037},	
    {0x0F12, 0x0054},	
    {0x0F12, 0x0083},	
    {0x0F12, 0x00C6},	
    {0x0F12, 0x0125},	
    {0x0F12, 0x0181},	
    {0x0F12, 0x0171},	
    {0x0F12, 0x011C},	
    {0x0F12, 0x00CD},	
    {0x0F12, 0x0099},	
    {0x0F12, 0x006E},	
    {0x0F12, 0x0058},	
    {0x0F12, 0x0054},	
    {0x0F12, 0x0061},	
    {0x0F12, 0x0083},	
    {0x0F12, 0x00B3},	
    {0x0F12, 0x00F9},	
    {0x0F12, 0x0154},	
    {0x0F12, 0x01B1},	
    {0x0F12, 0x01B2},	
    {0x0F12, 0x0162},	
    {0x0F12, 0x011C},	
    {0x0F12, 0x00DE},	
    {0x0F12, 0x00AF},	
    {0x0F12, 0x0099},	
    {0x0F12, 0x0092},	
    {0x0F12, 0x009F},	
    {0x0F12, 0x00C3},	
    {0x0F12, 0x00FC},	
    {0x0F12, 0x0141},	
    {0x0F12, 0x0193},	
    {0x0F12, 0x0206},	
    {0x0F12, 0x0203},	
    {0x0F12, 0x01AF},	
    {0x0F12, 0x016A},	
    {0x0F12, 0x012C},	
    {0x0F12, 0x00FF},	
    {0x0F12, 0x00E8},	
    {0x0F12, 0x00E2},	
    {0x0F12, 0x00EE},	
    {0x0F12, 0x0112},	
    {0x0F12, 0x014B},	
    {0x0F12, 0x018C},	
    {0x0F12, 0x01E2},	
    {0x0F12, 0x0274},	
    {0x0F12, 0x0169},	
    {0x0F12, 0x012F},	
    {0x0F12, 0x00FB},	
    {0x0F12, 0x00CC},	
    {0x0F12, 0x00AE},	
    {0x0F12, 0x00A2},	
    {0x0F12, 0x00A4},	
    {0x0F12, 0x00B3},	
    {0x0F12, 0x00D2},	
    {0x0F12, 0x0105},	
    {0x0F12, 0x013D},	
    {0x0F12, 0x0180},	
    {0x0F12, 0x01E0},	
    {0x0F12, 0x0133},	
    {0x0F12, 0x00FB},	
    {0x0F12, 0x00C1},	
    {0x0F12, 0x0096},	
    {0x0F12, 0x0077},	
    {0x0F12, 0x006B},	
    {0x0F12, 0x006C},	
    {0x0F12, 0x007B},	
    {0x0F12, 0x009D},	
    {0x0F12, 0x00CC},	
    {0x0F12, 0x0103},	
    {0x0F12, 0x014A},	
    {0x0F12, 0x0196},	
    {0x0F12, 0x0106},	
    {0x0F12, 0x00CA},	
    {0x0F12, 0x008B},	
    {0x0F12, 0x0066},	
    {0x0F12, 0x0049},	
    {0x0F12, 0x003D},	
    {0x0F12, 0x003C},	
    {0x0F12, 0x004B},	
    {0x0F12, 0x006A},	
    {0x0F12, 0x0094},	
    {0x0F12, 0x00CE},	
    {0x0F12, 0x011A},	
    {0x0F12, 0x0166},	
    {0x0F12, 0x00E7},	
    {0x0F12, 0x00A5},	
    {0x0F12, 0x006B},	
    {0x0F12, 0x0043},	
    {0x0F12, 0x0028},	
    {0x0F12, 0x001B},	
    {0x0F12, 0x001A},	
    {0x0F12, 0x0028},	
    {0x0F12, 0x0044},	
    {0x0F12, 0x006E},	
    {0x0F12, 0x00A6},	
    {0x0F12, 0x00F6},	
    {0x0F12, 0x0143},	
    {0x0F12, 0x00D3},	
    {0x0F12, 0x008F},	
    {0x0F12, 0x0054},	
    {0x0F12, 0x002E},	
    {0x0F12, 0x0013},	
    {0x0F12, 0x0006},	
    {0x0F12, 0x0005},	
    {0x0F12, 0x0012},	
    {0x0F12, 0x002E},	
    {0x0F12, 0x0057},	
    {0x0F12, 0x0090},	
    {0x0F12, 0x00E0},	
    {0x0F12, 0x0130},	
    {0x0F12, 0x00CE},	
    {0x0F12, 0x0088},	
    {0x0F12, 0x004E},	
    {0x0F12, 0x0029},	
    {0x0F12, 0x000E},	
    {0x0F12, 0x0001},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x000C},	
    {0x0F12, 0x0026},	
    {0x0F12, 0x004E},	
    {0x0F12, 0x0088},	
    {0x0F12, 0x00DB},	
    {0x0F12, 0x012E},	
    {0x0F12, 0x00DD},	
    {0xFFFF, 10},	
    
    {0x0F12, 0x0094},	
    {0x0F12, 0x005C},	
    {0x0F12, 0x0034},	
    {0x0F12, 0x0019},	
    {0x0F12, 0x000B},	
    {0x0F12, 0x000A},	
    {0x0F12, 0x0016},	
    {0x0F12, 0x002D},	
    {0x0F12, 0x0055},	
    {0x0F12, 0x008E},	
    {0x0F12, 0x00E3},	
    {0x0F12, 0x0137},	
    {0x0F12, 0x00FB},	
    {0x0F12, 0x00B4},	
    {0x0F12, 0x0078},	
    {0x0F12, 0x004D},	
    {0x0F12, 0x0032},	
    {0x0F12, 0x0024},	
    {0x0F12, 0x0022},	
    {0x0F12, 0x002E},	
    {0x0F12, 0x0045},	
    {0x0F12, 0x006B},	
    {0x0F12, 0x00A5},	
    {0x0F12, 0x00F9},	
    {0x0F12, 0x014C},	
    {0x0F12, 0x0128},	
    {0x0F12, 0x00E5},	
    {0x0F12, 0x00A4},	
    {0x0F12, 0x007B},	
    {0x0F12, 0x005C},	
    {0x0F12, 0x004D},	
    {0x0F12, 0x004C},	
    {0x0F12, 0x0056},	
    {0x0F12, 0x0071},	
    {0x0F12, 0x0099},	
    {0x0F12, 0x00D5},	
    {0x0F12, 0x0126},	
    {0x0F12, 0x017A},	
    {0x0F12, 0x0168},	
    {0x0F12, 0x0128},	
    {0x0F12, 0x00EE},	
    {0x0F12, 0x00BC},	
    {0x0F12, 0x0099},	
    {0x0F12, 0x008A},	
    {0x0F12, 0x0087},	
    {0x0F12, 0x0091},	
    {0x0F12, 0x00AE},	
    {0x0F12, 0x00DE},	
    {0x0F12, 0x0119},	
    {0x0F12, 0x0164},	
    {0x0F12, 0x01CB},	
    {0x0F12, 0x01B1},	
    {0x0F12, 0x0171},	
    {0x0F12, 0x0136},	
    {0x0F12, 0x0106},	
    {0x0F12, 0x00E5},	
    {0x0F12, 0x00D6},	
    {0x0F12, 0x00D2},	
    {0x0F12, 0x00DC},	
    {0x0F12, 0x00F9},	
    {0x0F12, 0x0129},	
    {0x0F12, 0x0162},	
    {0x0F12, 0x01B2},	
    {0x0F12, 0x0223},	

//	param_start	TVAR_ash
    {0x002A, 0x06B8},
    {0x0F12, 0x00DE},	
    {0x0F12, 0x00F7},	
    {0x0F12, 0x012B},   
    {0x0F12, 0x0138},	
    {0x0F12, 0x016B},	
    {0x0F12, 0x0198},	
    {0x0F12, 0x01A0},	

//======================
// SET CCM
//======================
    {0x002A, 0x06A6},
    {0x0F12, 0x00ED},   
    {0x0F12, 0x00F7},   
    {0x0F12, 0x012B},   
    {0x0F12, 0x0136},   
    {0x0F12, 0x016B},	
    {0x0F12, 0x0198},   

// CCM start address // 
    {0x002A, 0x0698},
    {0x0F12, 0x33A4},
    {0x0F12, 0x7000},

// Horizon
    {0x002A, 0x33A4},
    {0x0F12, 0x0152},	
    {0x0F12, 0xFF94},	
    {0x0F12, 0xFFD6},	
    {0x0F12, 0xFE95},	
    {0x0F12, 0x012B},	
    {0x0F12, 0xFEF0},	
    {0x0F12, 0xFFF8},	
    {0x0F12, 0xFFFF},	
    {0x0F12, 0x01D8},	
    {0x0F12, 0x00C7},	
    {0x0F12, 0x00C0},	
    {0x0F12, 0xFEE7},	
    {0x0F12, 0x0206},	
    {0x0F12, 0xFF70},	
    {0x0F12, 0x01AC},	
    {0x0F12, 0xFF02},	
    {0x0F12, 0x0132},	
    {0x0F12, 0x00E0},	

    {0x0F12, 0x0185},   
    {0x0F12, 0xFF56},   
    {0x0F12, 0xFFEA},   
    {0x0F12, 0xFE40},   
    {0x0F12, 0x01EF},   
    {0x0F12, 0xFE85},   
    {0x0F12, 0xFF14},   
    {0x0F12, 0xFFB5},   
    {0x0F12, 0x0307},   
    {0x0F12, 0x01B4},   
    {0x0F12, 0x0134},   
    {0x0F12, 0xFD8E},   
    {0x0F12, 0x01EA},   
    {0x0F12, 0xFF43},   
    {0x0F12, 0x01FE},   
    {0x0F12, 0xFE48},   
    {0x0F12, 0x0274},   
    {0x0F12, 0x0061},   

//TL84 robin 0914
    {0x0F12, 0x01A7},   
    {0x0F12, 0xFF54},   
    {0x0F12, 0xFFF8},   
    {0x0F12, 0xFEA0},   
    {0x0F12, 0x0181},   
    {0x0F12, 0xFEBB},   
    {0x0F12, 0xFF4D},   
    {0x0F12, 0x0010},   
    {0x0F12, 0x028E},   
    {0x0F12, 0x0193},   
    {0x0F12, 0x00E6},   
    {0x0F12, 0xFE29},   
    {0x0F12, 0x0294},   
    {0x0F12, 0xFF33},   
    {0x0F12, 0x0191},   
    {0x0F12, 0xFE58},   
    {0x0F12, 0x0213},   
    {0x0F12, 0x00E7},   

    {0x0F12, 0x0141},	
    {0x0F12, 0xFF98},	
    {0x0F12, 0xFFCD},	
    {0x0F12, 0xFE80},	
    {0x0F12, 0x0105},	
    {0x0F12, 0xFF03},	
    {0x0F12, 0xFFF9},	
    {0x0F12, 0xFFDB},	
    {0x0F12, 0x01BB},	
    {0x0F12, 0x00C3},	
    {0x0F12, 0x0094},	
    {0x0F12, 0xFF00},	
    {0x0F12, 0x0210},	
    {0x0F12, 0xFF74},	
    {0x0F12, 0x0188},	
    {0x0F12, 0xFEFE},	
    {0x0F12, 0x0113},	
    {0x0F12, 0x00EB},	

//D50
    {0x0F12, 0x01D4},   
    {0x0F12, 0xFF79},   
    {0x0F12, 0xFFE1},   
    {0x0F12, 0xFF1F},   
    {0x0F12, 0x020C},   
    {0x0F12, 0xFF23},   
    {0x0F12, 0xFF8D},   
    {0x0F12, 0xFFC3},   
    {0x0F12, 0x0238},   
    {0x0F12, 0x0131},   
    {0x0F12, 0x0130},   
    {0x0F12, 0xFF1C},   
    {0x0F12, 0x019E},   
    {0x0F12, 0xFF21},   
    {0x0F12, 0x01D0},   
    {0x0F12, 0xFEF0},   
    {0x0F12, 0x01E0},   
    {0x0F12, 0x0108},   

//D65 robin 0914      
    {0x0F12, 0x01D4},   
    {0x0F12, 0xFF79},   
    {0x0F12, 0xFFE1},   
    {0x0F12, 0xFF1F},   
    {0x0F12, 0x020C},   
    {0x0F12, 0xFF23},   
    {0x0F12, 0xFF8D},   
    {0x0F12, 0xFFC3},   
    {0x0F12, 0x0238},   
    {0x0F12, 0x0131},   
    {0x0F12, 0x0130},   
    {0x0F12, 0xFF1C},   
    {0x0F12, 0x019E},   
    {0x0F12, 0xFF21},   
    {0x0F12, 0x01D0},   
    {0x0F12, 0xFEF0},   
    {0x0F12, 0x01E0},   
    {0x0F12, 0x0108},   

// Outdoor CCM address /
    {0x002A, 0x06A0},
    {0x0F12, 0x3380},
    {0x0F12, 0x7000},
// Outdoor CCM


    {0x002A, 0x3380},
    {0x0F12, 0x01D7},	
    {0x0F12, 0xFF96},	
    {0x0F12, 0xFFB5},	
    {0x0F12, 0xFE97},	
    {0x0F12, 0x029F},	
    {0x0F12, 0xFEE4},	
    {0x0F12, 0xFFB6},	
    {0x0F12, 0x001B},	
    {0x0F12, 0x01CA},	
    {0x0F12, 0x0124},	
    {0x0F12, 0x00EC},	
    {0x0F12, 0xFF45},	
    {0x0F12, 0x00BD},	
    {0x0F12, 0xFF90},	
    {0x0F12, 0x014D},	
    {0x0F12, 0xFECF},	
    {0x0F12, 0x01B3},	
    {0x0F12, 0x0105},	
//======================
// SET AWB
//======================
// Indoor boundary
    {0x002A, 0x0C48},
    {0x0F12, 0x03C9},	
    {0x0F12, 0x040A},	
    {0x0F12, 0x0376},	
    {0x0F12, 0x0405},	
    {0x0F12, 0x0331},	
    {0x0F12, 0x0400},	
    {0x0F12, 0x0300},	
    {0x0F12, 0x03DF},	
    {0x0F12, 0x02D9},	
    {0x0F12, 0x0392},	
    {0x0F12, 0x02B1},	
    {0x0F12, 0x036D},	
    {0x0F12, 0x028C},	
    {0x0F12, 0x0342},	
    {0x0F12, 0x0270},	
    {0x0F12, 0x0322},	
    {0x0F12, 0x0256},	
    {0x0F12, 0x0300},	
    {0x0F12, 0x023A},	
    {0x0F12, 0x02E2},	
    {0x0F12, 0x0228},	
    {0x0F12, 0x02CC},	
    {0x0F12, 0x0210},	
    {0x0F12, 0x02B5},	
    {0x0F12, 0x01FE},	
    {0x0F12, 0x02A2},	
    {0x0F12, 0x01EE},	
    {0x0F12, 0x028A},	
    {0x0F12, 0x01F4},	
    {0x0F12, 0x0270},	
    {0x0F12, 0x0202},	
    {0x0F12, 0x0247},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0005},	
    {0x002A, 0x0CA0},
    {0x0F12, 0x00E8},	

// Outdoor boundary
    {0x002A, 0x0CA4},
    {0x0F12, 0x026F},	
    {0x0F12, 0x02A4},	
    {0x0F12, 0x0256},	
    {0x0F12, 0x02AE},	
    {0x0F12, 0x023C},	
    {0x0F12, 0x029D},	
    {0x0F12, 0x022A},	
    {0x0F12, 0x0286},	
    {0x0F12, 0x0245},	
    {0x0F12, 0x026F},	
    {0x0F12, 0x0000},	
    {0xFFFF, 10},	
    
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0005},	
    {0x002A, 0x0CDC},
    {0x0F12, 0x0204},	

//Outdoordetector
    {0x002A, 0x0DA0},
    {0x0F12, 0x0005},
    {0x002A, 0x0D88},
    {0x0F12, 0x0048},
    {0x0F12, 0x0084},
    {0x0F12, 0x0001},
    {0x0F12, 0x00CF},
    {0x0F12, 0xFFAB},
    {0x0F12, 0x00EB},
    {0x0F12, 0xFF66},
    {0x0F12, 0x0100},
    {0x0F12, 0xFF0F},
    {0x0F12, 0x011F},
    {0x0F12, 0x0E74},
    {0x002A, 0x0DA8},
    {0x0F12, 0x1701},
    {0x002A, 0x0DA4},
    {0x0F12, 0x0691},

// LowBr boundary
    {0x002A, 0x0CE0}, 
    {0x0F12, 0x03C6},   
    {0x0F12, 0x03F1},   
    {0x0F12, 0x0340},   
    {0x0F12, 0x03F6},   
    {0x0F12, 0x02DA},   
    {0x0F12, 0x03B6},   
    {0x0F12, 0x028B},   
    {0x0F12, 0x037A},   
    {0x0F12, 0x0245},   
    {0x0F12, 0x0328},   
    {0x0F12, 0x020A},   
    {0x0F12, 0x02E2},   
    {0x0F12, 0x01EB},   
    {0x0F12, 0x02B8},   
    {0x0F12, 0x01D1},   
    {0x0F12, 0x0296},   
    {0x0F12, 0x01BF},   
    {0x0F12, 0x0270},   
    {0x0F12, 0x01BD},   
    {0x0F12, 0x024E},   
    {0x0F12, 0x01E2},   
    {0x0F12, 0x020C},   
    {0x0F12, 0x0000},   
    {0x0F12, 0x0000},   
    {0x0F12, 0x0006},   
    {0x002A, 0x0D18},   
    {0x0F12, 0x00FE},   

// AWB ETC
    {0x002A, 0x0D1C},
    {0x0F12, 0x037C},	
    {0x002A, 0x0D20},
    {0x0F12, 0x0157},	
    {0x002A, 0x0D24},
    {0x0F12, 0x3EB8},	

    {0x002A, 0x0D2C},
    {0x0F12, 0x013D},	
    {0x0F12, 0x011E},	

    {0x002A, 0x0D28},
    {0x0F12, 0x0290},	
    {0x0F12, 0x0240},	

    {0x002A, 0x0D5C},
    {0x0F12, 0x7FFF},	
    {0x0F12, 0x0050},	

    {0x002A, 0x0D46},
    {0x0F12, 0x0546},	

// AWB initial point
    {0x002A, 0x0E44},
    {0x0F12, 0x053C},	
    {0x0F12, 0x0400},	
    {0x0F12, 0x055C},	
// Set AWB global offset
    {0x002A, 0x0E36},
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	

//======================
// SET GRID OFFSET
//======================
// Not used
    {0x002A, 0x0E4A},
    {0x0F12, 0x0002},	

    {0x002A, 0x0DD4},   
    {0x0F12, 0x0032},   
    {0x0F12, 0x0032},   
    {0x0F12, 0x0032},   
    {0x0F12, 0x001E},   
    {0x0F12, 0x001E},   
    {0x0F12, 0x0064},   
    {0x0F12, 0x0032},   
    {0x0F12, 0x0032},   
    {0x0F12, 0x0032},   
    {0x0F12, 0x001E},   
    {0x0F12, 0x001E},   
    {0x0F12, 0x0064},   
    {0x0F12, 0x0032},   
    {0x0F12, 0x0032},   
    {0x0F12, 0x0032},   
    {0x0F12, 0x001E},   
    {0x0F12, 0x001E},   
    {0x0F12, 0x0064},   
    {0x0F12, 0xFFE2},   
    {0x0F12, 0x0000},   
    {0x0F12, 0x0014},   
    {0x0F12, 0x001E},   
    {0x0F12, 0xFFCE},   
    {0x0F12, 0xFFCE},   
    {0x0F12, 0xFFE2},   
    {0x0F12, 0x0000},   
    {0x0F12, 0x0014},   
    {0x0F12, 0x001E},   
    {0x0F12, 0xFFCE},   
    {0x0F12, 0xFFCE},   
    {0x0F12, 0xFFE2},   
    {0x0F12, 0x0000},   
    {0x0F12, 0x0014},   
    {0x0F12, 0x001E},   
    {0x0F12, 0xFFCE},   
    {0x0F12, 0xFFCE},   

    {0x0F12, 0x02D9},	
    {0x0F12, 0x0357},	
    {0x0F12, 0x03D1},	

    {0x0F12, 0x0DF6},	
    {0x0F12, 0x0EB9},	
    {0x0F12, 0x0F42},	
    {0x0F12, 0x0F4E},	
    {0x0F12, 0x0F99},	
    {0x0F12, 0x1006},	

    {0x0F12, 0x00AC},	
    {0x0F12, 0x00BD},	
    {0x0F12, 0x0049},	
    {0x0F12, 0x00F5},	


//======================
// SET GAMMA
//======================
//Our //old	//STW
    {0x002A, 0x05A0},
    {0x0F12, 0x0000},	
    {0x0F12, 0x0008},	
    {0x0F12, 0x0013},	
    {0x0F12, 0x002C},	
    {0x0F12, 0x0061},	
    {0x0F12, 0x00C8},	
    {0x0F12, 0x0113},	
    {0x0F12, 0x0132},	
    {0x0F12, 0x014C},	
    {0x0F12, 0x0179},	
    {0x0F12, 0x01A4},	
    {0x0F12, 0x01CD},	
    {0x0F12, 0x01F4},	
    {0x0F12, 0x0239},	
    {0x0F12, 0x0278},	
    {0x0F12, 0x02E0},	
    {0x0F12, 0x0333},	
    {0x0F12, 0x037B},	
    {0x0F12, 0x03BF},	
    {0x0F12, 0x03FF},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0008},	
    {0x0F12, 0x0013},	
    {0x0F12, 0x002C},	
    {0x0F12, 0x0061},	
    {0x0F12, 0x00C8},	
    {0x0F12, 0x0113},	
    {0x0F12, 0x0132},	
    {0x0F12, 0x014C},	
    {0x0F12, 0x0179},	
    {0x0F12, 0x01A4},	
    {0x0F12, 0x01CD},	
    {0x0F12, 0x01F4},	
    {0x0F12, 0x0239},	
    {0x0F12, 0x0278},	
    {0x0F12, 0x02E0},	
    {0x0F12, 0x0333},	
    {0x0F12, 0x037B},	
    {0x0F12, 0x03BF},	
    {0x0F12, 0x03FF},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0008},	
    {0x0F12, 0x0013},	
    {0x0F12, 0x002C},	
    {0x0F12, 0x0061},	
    {0x0F12, 0x00C8},	
    {0x0F12, 0x0113},	
    {0x0F12, 0x0132},	
    {0x0F12, 0x014C},	
    {0x0F12, 0x0179},	
    {0x0F12, 0x01A4},	
    {0x0F12, 0x01CD},	
    {0x0F12, 0x01F4},	
    {0x0F12, 0x0239},	
    {0x0F12, 0x0278},	
    {0x0F12, 0x02E0},	
    {0x0F12, 0x0333},	
    {0x0F12, 0x037B},	
    {0x0F12, 0x03BF},	
    {0x0F12, 0x03FF},	

    {0x002A, 0x1034},
    {0x0F12, 0x00C0},	
    {0x0F12, 0x00E0},	
    {0x0F12, 0x00F0},	
    {0x0F12, 0x0129},	
    {0x0F12, 0x0156},	
    {0x0F12, 0x017F},	
    {0x0F12, 0x018F},	

    {0x0F12, 0x0120},	
    {0x0F12, 0x0120},	
    {0x0F12, 0x0120},	
    {0x0F12, 0x0100},	
    {0x0F12, 0x0100},	
    {0x0F12, 0x0100},	
    {0x0F12, 0x0100},	

//======================
// SET AFIT
//======================
// Noise index
    {0x002A, 0x0764}, 
    {0x0F12, 0x0041},  
    {0x0F12, 0x0063},  
    {0x0F12, 0x00BB},  
    {0x0F12, 0x0193},  
    {0x0F12, 0x02BC},  
// AFIT table start addr
    {0x002A, 0x0770}, 
    {0x0F12, 0x07C4}, 
    {0x0F12, 0x7000}, 
// AFIT table (Variables
    {0x002A, 0x07C4}, 
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x00C4}, 	
    {0x0F12, 0x03FF}, 	
    {0x0F12, 0x009C}, 	
    {0x0F12, 0x017C}, 	
    {0x0F12, 0x03FF}, 	
    {0x0F12, 0x000C}, 	
    {0x0F12, 0x0010}, 	
    {0x0F12, 0x0104}, 	
    {0x0F12, 0x03E8}, 	
    {0x0F12, 0x0023}, 	
    {0x0F12, 0x012C}, 	
    {0x0F12, 0x0070}, 	
    {0x0F12, 0x0010}, 	
    {0x0F12, 0x0010}, 	
    {0x0F12, 0x01AA}, 	
    {0x0F12, 0x0064}, 	
    {0x0F12, 0x0064}, 	
    {0x0F12, 0x000A}, 	
    {0x0F12, 0x000A}, 	
    {0x0F12, 0x003C}, 	
    {0x0F12, 0x0024}, 	
    {0x0F12, 0x002A}, 	
    {0x0F12, 0x0024}, 	
    {0x0F12, 0x002A}, 	
    {0x0F12, 0x0024}, 	
    {0x0F12, 0x0A24}, 	
    {0x0F12, 0x1701}, 	
    {0x0F12, 0x0229}, 	
    {0x0F12, 0x1403}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0xFFFF, 10},	
    
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x00FF}, 	
    {0x0F12, 0x043B}, 	
    {0x0F12, 0x1414}, 	
    {0x0F12, 0x0301}, 	
    {0x0F12, 0xFF07}, 	
    {0x0F12, 0x051E}, 	
    {0x0F12, 0x0A1E}, 	
    {0x0F12, 0x0F0F}, 	
    {0x0F12, 0x0A05}, 	
    {0x0F12, 0x0A3C}, 	
    {0x0F12, 0x0A28}, 	
    {0x0F12, 0x0002}, 	
    {0x0F12, 0x00FF}, 	
    {0x0F12, 0x1002}, 	
    {0x0F12, 0x001D}, 	
    {0x0F12, 0x0900}, 	
    {0x0F12, 0x0600}, 	
    {0x0F12, 0x0504}, 	
    {0x0F12, 0x0305}, 	
    {0x0F12, 0x3C03}, 	
    {0x0F12, 0x006E}, 	
    {0x0F12, 0x0078}, 	
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x1414}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x5002}, 	
    {0x0F12, 0x7850}, 	
    {0x0F12, 0x2878}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x1403}, 	
    {0x0F12, 0x1E0C}, 	
    {0x0F12, 0x070A}, 	
    {0x0F12, 0x32FF}, 	
    {0x0F12, 0x5004}, 	
    {0x0F12, 0x0F40}, 	
    {0x0F12, 0x400F}, 	
    {0x0F12, 0x0204}, 	
    {0x0F12, 0x1E03}, 	
    {0x0F12, 0x011E}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x5050}, 	
    {0x0F12, 0x7878}, 	
    {0x0F12, 0x0028}, 	
    {0x0F12, 0x030A}, 	
    {0x0F12, 0x0714}, 	
    {0x0F12, 0x0A1E}, 	
    {0x0F12, 0xFF07}, 	
    {0x0F12, 0x0432}, 	
    {0x0F12, 0x4050}, 	
    {0x0F12, 0x0F0F}, 	
    {0x0F12, 0x0440}, 	
    {0x0F12, 0x0302}, 	
    {0x0F12, 0x1E1E}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x5001}, 	
    {0x0F12, 0x7850}, 	
    {0x0F12, 0x2878}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x1403}, 	
    {0x0F12, 0x1E07}, 	
    {0x0F12, 0x070A}, 	
    {0x0F12, 0x32FF}, 	
    {0x0F12, 0x5004}, 	
    {0x0F12, 0x0F40}, 	
    {0x0F12, 0x400F}, 	
    {0x0F12, 0x0204}, 	
    {0x0F12, 0x0003}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x00C4}, 	
    {0x0F12, 0x03FF}, 	
    {0x0F12, 0x009C}, 	
    {0x0F12, 0x017C}, 	
    {0x0F12, 0x03FF}, 	
    {0x0F12, 0x000C}, 	
    {0x0F12, 0x0010}, 	
    {0x0F12, 0x0104}, 	
    {0x0F12, 0x03E8}, 	
    {0x0F12, 0x0023}, 	
    {0x0F12, 0x012C}, 	
    {0x0F12, 0x0070}, 	
    {0x0F12, 0x0008}, 	
    {0x0F12, 0x0008}, 	
    {0x0F12, 0x01AA}, 	
    {0x0F12, 0x003C}, 	
    {0x0F12, 0x003C}, 	
    {0x0F12, 0x0005}, 	
    {0x0F12, 0x0005}, 	
    {0x0F12, 0x0050}, 	
    {0x0F12, 0x0024}, 	
    {0x0F12, 0x002A}, 	
    {0x0F12, 0x0024}, 	
    {0x0F12, 0x002A}, 	
    {0x0F12, 0x0024}, 	
    {0x0F12, 0x0A24}, 	
    {0x0F12, 0x1701}, 	
    {0x0F12, 0x0229}, 	
    {0x0F12, 0x1403}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x00FF}, 	
    {0x0F12, 0x043B}, 	
    {0x0F12, 0x1414}, 	
    {0x0F12, 0x0301}, 	
    {0x0F12, 0xFF07}, 	
    {0x0F12, 0x051E}, 	
    {0x0F12, 0x0A1E}, 	
    {0x0F12, 0x0F0F}, 	
    {0x0F12, 0x0A03}, 	
    {0x0F12, 0x0A3C}, 	
    {0x0F12, 0x0A28}, 	
    {0x0F12, 0x0002}, 	
    {0x0F12, 0x00FF}, 	
    {0x0F12, 0x1002}, 	
    {0x0F12, 0x001D}, 	
    {0x0F12, 0x0900}, 	
    {0x0F12, 0x0600}, 	
    {0x0F12, 0x0504}, 	
    {0x0F12, 0x0305}, 	
    {0x0F12, 0x4603}, 	
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x1919}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x3C02}, 	
    {0x0F12, 0x553C}, 	
    {0x0F12, 0x2855}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x1403}, 	
    {0x0F12, 0x1E0C}, 	
    {0x0F12, 0x070A}, 	
    {0x0F12, 0x32FF}, 	
    {0x0F12, 0x5004}, 	
    {0x0F12, 0x0F40}, 	
    {0x0F12, 0x400F}, 	
    {0x0F12, 0x0204}, 	
    {0x0F12, 0x1E03}, 	
    {0x0F12, 0x011E}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x3232}, 	
    {0x0F12, 0x3C3C}, 	
    {0x0F12, 0x0028}, 	
    {0x0F12, 0x030A}, 	
    {0x0F12, 0x0714}, 	
    {0x0F12, 0x0A1E}, 	
    {0x0F12, 0xFF07}, 	
    {0x0F12, 0x0432}, 	
    {0x0F12, 0x4050}, 	
    {0x0F12, 0x0F0F}, 	
    {0x0F12, 0x0440}, 	
    {0x0F12, 0x0302}, 	
    {0x0F12, 0x1E1E}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x3201}, 	
    {0x0F12, 0x3C32}, 	
    {0x0F12, 0x283C}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x1403}, 	
    {0x0F12, 0x1E07}, 	
    {0x0F12, 0x070A}, 	
    {0x0F12, 0x32FF}, 	
    {0x0F12, 0x5004}, 	
    {0x0F12, 0x0F40}, 	
    {0x0F12, 0x400F}, 	
    {0x0F12, 0x0204}, 	
    {0x0F12, 0x0003}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x00C4}, 	
    {0x0F12, 0x03FF}, 	
    {0x0F12, 0x009C}, 	
    {0x0F12, 0x017C}, 	
    {0x0F12, 0x03FF}, 	
    {0x0F12, 0x000C}, 	
    {0x0F12, 0x0010}, 	
    {0x0F12, 0x0104}, 	
    {0x0F12, 0x03E8}, 	
    {0x0F12, 0x0023}, 	
    {0x0F12, 0x012C}, 	
    {0x0F12, 0x0070}, 	
    {0x0F12, 0x0004}, 	
    {0x0F12, 0x0004}, 	
    {0x0F12, 0x01AA}, 	
    {0x0F12, 0x001E}, 	
    {0x0F12, 0x001E}, 	
    {0x0F12, 0x0005}, 	
    {0x0F12, 0x0005}, 	
    {0x0F12, 0x0064}, 	
    {0x0F12, 0x0024}, 	
    {0x0F12, 0x002A}, 	
    {0x0F12, 0x0024}, 	
    {0x0F12, 0x002A}, 	
    {0x0F12, 0x0024}, 	
    {0x0F12, 0x0A24}, 	
    {0x0F12, 0x1701}, 	
    {0x0F12, 0x0229}, 	
    {0x0F12, 0x1403}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x00FF}, 	
    {0x0F12, 0x043B}, 	
    {0x0F12, 0x1414}, 	
    {0x0F12, 0x0301}, 	
    {0x0F12, 0xFF07}, 	
    {0x0F12, 0x051E}, 	
    {0x0F12, 0x0A1E}, 	
    {0x0F12, 0x0F0F}, 	
    {0x0F12, 0x0A03}, 	
    {0x0F12, 0x0A3C}, 	
    {0x0F12, 0x0528}, 	
    {0x0F12, 0x0002}, 	
    {0x0F12, 0x00FF}, 	
    {0x0F12, 0x1002}, 	
    {0x0F12, 0x001D}, 	
    {0x0F12, 0x0900}, 	
    {0x0F12, 0x0600}, 	
    {0x0F12, 0x0504}, 	
    {0x0F12, 0x0305}, 	
 ///* <BU5D05950 zhangsh
    {0x0F12, 0x7903}, 	
 ///* BU5D05950 zhangshe
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x2323}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x2A02}, 	
    {0x0F12, 0x462A}, 	
    {0x0F12, 0x2846}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x1403}, 	
    {0x0F12, 0x1E0C}, 	
    {0x0F12, 0x070A}, 	
    {0x0F12, 0x32FF}, 	
    {0x0F12, 0x5A04}, 	
    {0x0F12, 0x0F40}, 	
    {0x0F12, 0x400F}, 	
    {0x0F12, 0x0204}, 	
    {0x0F12, 0x2303}, 	
    {0x0F12, 0x0123}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x262A}, 	
    {0x0F12, 0x2C2C}, 	
    {0x0F12, 0x0028}, 	
    {0x0F12, 0x030A}, 	
    {0x0F12, 0x0714}, 	
    {0x0F12, 0x0A1E}, 	
    {0x0F12, 0xFF07}, 	
    {0x0F12, 0x0432}, 	
    {0x0F12, 0x4050}, 	
    {0x0F12, 0x0F0F}, 	
    {0x0F12, 0x0440}, 	
    {0x0F12, 0x0302}, 	
    {0x0F12, 0x2323}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x2A01}, 	
    {0x0F12, 0x2C26}, 	
    {0x0F12, 0x282C}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x1403}, 	
    {0x0F12, 0x1E07}, 	
    {0x0F12, 0x070A}, 	
    {0x0F12, 0x32FF}, 	
    {0x0F12, 0x5004}, 	
    {0x0F12, 0x0F40}, 	
    {0x0F12, 0x400F}, 	
    {0x0F12, 0x0204}, 	
    {0x0F12, 0x0003}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x00C4}, 	
    {0x0F12, 0x03FF}, 	
    {0x0F12, 0x009C}, 	
    {0x0F12, 0x017C}, 	
    {0x0F12, 0x03FF}, 	
    {0x0F12, 0x000C}, 	
    {0x0F12, 0x0010}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0023}, 	
    {0x0F12, 0x012C}, 	
    {0x0F12, 0x0070}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x01AA}, 	
    {0xFFFF, 10},	    
    {0x0F12, 0x001E}, 	
    {0x0F12, 0x001E}, 	
    {0x0F12, 0x000A}, 	
    {0x0F12, 0x000A}, 	
    {0x0F12, 0x00E6}, 	
    {0x0F12, 0x0032}, 	
    {0x0F12, 0x0032}, 	
    {0x0F12, 0x0028}, 	
    {0x0F12, 0x0032}, 	
    {0x0F12, 0x0028}, 	
    {0x0F12, 0x0A24}, 	
    {0x0F12, 0x1701}, 	
    {0x0F12, 0x0229}, 	
    {0x0F12, 0x1403}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0504}, 	
    {0x0F12, 0x00FF}, 	
    {0x0F12, 0x043B}, 	
    {0x0F12, 0x1414}, 	
    {0x0F12, 0x0301}, 	
    {0x0F12, 0xFF07}, 	
    {0x0F12, 0x051E}, 	
    {0x0F12, 0x0A1E}, 	
    {0x0F12, 0x0F0F}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x0A3C}, 	
    {0x0F12, 0x0532}, 	
    {0x0F12, 0x0002}, 	
    {0x0F12, 0x00FF}, 	
    {0x0F12, 0x1002}, 	
    {0x0F12, 0x001D}, 	
    {0x0F12, 0x0900}, 	
    {0x0F12, 0x0600}, 	
    {0x0F12, 0x0504}, 	
    {0x0F12, 0x0305}, 	
 ///* <BU5D05950 zhangsh
    {0x0F12, 0x8902}, 	
 ///* BU5D05950 zhangshe
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x2328}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x2A02}, 	
    {0x0F12, 0x2628}, 	
    {0x0F12, 0x2826}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x1903}, 	
    {0x0F12, 0x1E0F}, 	
    {0x0F12, 0x070A}, 	
    {0x0F12, 0x32FF}, 	
    {0x0F12, 0x7804}, 	
    {0x0F12, 0x0F40}, 	
    {0x0F12, 0x400F}, 	
    {0x0F12, 0x0204}, 	
    {0x0F12, 0x2803}, 	
    {0x0F12, 0x0123}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x2024}, 	
    {0x0F12, 0x1C1C}, 	
    {0x0F12, 0x0028}, 	
    {0x0F12, 0x030A}, 	
    {0x0F12, 0x0A0A}, 	
    {0x0F12, 0x0A2D}, 	
    {0x0F12, 0xFF07}, 	
    {0x0F12, 0x0432}, 	
    {0x0F12, 0x4050}, 	
    {0x0F12, 0x0F0F}, 	
    {0x0F12, 0x0440}, 	
    {0x0F12, 0x0302}, 	
    {0x0F12, 0x2328}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x3C01}, 	
    {0x0F12, 0x1C3C}, 	
    {0x0F12, 0x281C}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x0A03}, 	
    {0x0F12, 0x2D0A}, 	
    {0x0F12, 0x070A}, 	
    {0x0F12, 0x32FF}, 	
    {0x0F12, 0x5004}, 	
    {0x0F12, 0x0F40}, 	
    {0x0F12, 0x400F}, 	
    {0x0F12, 0x0204}, 	
    {0x0F12, 0x0003}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x002B}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x00C4}, 	
    {0x0F12, 0x03FF}, 	
    {0x0F12, 0x009C}, 	
    {0x0F12, 0x017C}, 	
    {0x0F12, 0x03FF}, 	
    {0x0F12, 0x000C}, 	
    {0x0F12, 0x0010}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x003C}, 	
    {0x0F12, 0x006F}, 	
    {0x0F12, 0x0070}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x01AA}, 	
    {0x0F12, 0x0014}, 	
    {0x0F12, 0x0014}, 	
    {0x0F12, 0x000A}, 	
    {0x0F12, 0x000A}, 	
    {0x0F12, 0x0122}, 	
    {0x0F12, 0x003C}, 	
    {0x0F12, 0x0032}, 	
    {0x0F12, 0x0023}, 	
    {0x0F12, 0x0023}, 	
    {0x0F12, 0x0032}, 	
    {0x0F12, 0x0A24}, 	
    {0x0F12, 0x1701}, 	
    {0x0F12, 0x0229}, 	
    {0x0F12, 0x1403}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0505}, 	
    {0x0F12, 0x00FF}, 	
    {0x0F12, 0x043B}, 	
    {0x0F12, 0x1414}, 	
    {0x0F12, 0x0301}, 	
    {0x0F12, 0xFF07}, 	
    {0x0F12, 0x051E}, 	
    {0x0F12, 0x0A1E}, 	
    {0x0F12, 0x0000}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x0A3C}, 	
    {0x0F12, 0x0532}, 	
    {0x0F12, 0x0002}, 	
    {0x0F12, 0x0096}, 	
    {0x0F12, 0x1002}, 	
    {0x0F12, 0x001E}, 	
    {0x0F12, 0x0900}, 	
    {0x0F12, 0x0600}, 	
    {0x0F12, 0x0504}, 	
    {0x0F12, 0x0305}, 	
// ///* <BU5D05950 zhang
    {0x0F12, 0x8002}, 	
// ///* BU5D05950 zhangs
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x0080}, 	
    {0x0F12, 0x5050}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x1C02}, 	
    {0x0F12, 0x191C}, 	
    {0x0F12, 0x2819}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x1E03}, 	
    {0x0F12, 0x1E0F}, 	
    {0x0F12, 0x0508}, 	
    {0x0F12, 0x32FF}, 	
    {0x0F12, 0x8204}, 	
    {0x0F12, 0x1448}, 	
    {0x0F12, 0x4015}, 	
    {0x0F12, 0x0204}, 	
    {0x0F12, 0x5003}, 	
    {0x0F12, 0x0150}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x1E1E}, 	
    {0x0F12, 0x1212}, 	
    {0x0F12, 0x0028}, 	
    {0x0F12, 0x030A}, 	
    {0x0F12, 0x0A10}, 	
    {0x0F12, 0x0819}, 	
    {0x0F12, 0xFF05}, 	
    {0x0F12, 0x0432}, 	
    {0x0F12, 0x4052}, 	
    {0x0F12, 0x1514}, 	
    {0x0F12, 0x0440}, 	
    {0x0F12, 0x0302}, 	
    {0x0F12, 0x5050}, 	
    {0x0F12, 0x0101}, 	
    {0x0F12, 0x1E01}, 	
    {0x0F12, 0x121E}, 	
    {0x0F12, 0x2812}, 	
    {0x0F12, 0x0A00}, 	
    {0x0F12, 0x1003}, 	
    {0x0F12, 0x190A}, 	
    {0x0F12, 0x0508}, 	
    {0x0F12, 0x32FF}, 	
    {0x0F12, 0x5204}, 	
    {0x0F12, 0x1440}, 	
    {0x0F12, 0x4015}, 	
    {0x0F12, 0x0204}, 	
    {0x0F12, 0x0003}, 	
// AFIT table (Constants
    {0x0F12, 0x7F7A}, 	
    {0x0F12, 0x7F9D}, 	
    {0x0F12, 0xBEFC}, 	
    {0x0F12, 0xF7BC}, 	
    {0x0F12, 0x7E06}, 	
    {0x0F12, 0x0053}, 	
// Update Changed Regist
    {0x002A, 0x0664}, 
    {0x0F12, 0x013E},  

    {0x002A, 0x04D6}, 
    {0x0F12, 0x0001},  
    {0x0028, 0xD000}, 
    {0x002A, 0x1102}, 
    {0x0F12, 0x00C0},  
    {0x002A, 0x113C}, 
    {0x0F12, 0x267C},  
    {0x0F12, 0x2680},  
    {0x002A, 0x1142},  
    {0x0F12, 0x00C0},  
    {0x002A, 0x117C},  
    {0x0F12, 0x2CE8},  
    {0x0F12, 0x2CeC},  
// Fill RAM with altern
    {0x0028, 0x7000},  
    {0x002A, 0x2CE8},  
    {0x0F12, 0x0007},  
    {0x0F12, 0x00e2},  
    {0x0F12, 0x0005},  
    {0x0F12, 0x00e2},  
// Update T&P tuning pa
    {0x002A, 0x337A}, 
    {0x0F12, 0x0006},  

///////////////////////
// AFC - fix if ( G_AFC
// Fill RAM with altern
    {0x002A, 0x2CE6},
    {0x0F12, 0x220A},	
    {0x002A, 0x3378},
    {0x0F12, 0x0030},	
    {0x002A, 0x10E2},
    {0x0F12, 0x00C0},	
    {0x002A, 0x10E0},
    {0x0F12, 0x0008},	
// force zeroing of "G_A
    {0x002A, 0x0C1E},
    {0x0F12, 0x0018},	
////////////////////////

//======================
// SET PLL
//======================
// How to set
// 1. MCLK
// 2. System CLK
// 3. PCLK
//======================
// Set input CLK // 24MH

    {0x0028, 0x7000},
    {0x002A, 0x01CC},
    {0x0F12, 0x5DC0},	
    {0x0F12, 0x0000},	
    {0x002A, 0x01EE},
    {0x0F12, 0x0002},	
// Set system CLK
    {0x002A, 0x01F6},
    {0x0F12, 0x2904},	
// Set pixel CLK // 22MH
    {0x0F12, 0x157C},	
    {0x0F12, 0x157C},	
// Set system CLK
    {0x0F12, 0x2904},	
// Set pixel CLK // 24MH
    {0x0F12, 0x1770},	
    {0x0F12, 0x1770},	
// Update PLL
    {0x002A, 0x0208},
    {0x0F12, 0x0001},	

//======================
// Frame rate setting
//======================
// How to set
// 1. Exposure value
// dec2hex((1 / (frame r
// 2. Analog Digital gai
// dec2hex((Analog gain 
//======================
// Set preview exposure 
    {0x002A, 0x0530},
    {0x0F12, 0x5DC0},	
    {0x0F12, 0x0000},
    {0x0F12, 0x6D60},	
    {0x0F12, 0x0000},
    {0x002A, 0x167C},
    {0x0F12, 0x9C40},	
    {0x0F12, 0x0000},
    {0x0F12, 0xBB80},   
    {0x0F12, 0x0000},

// Set capture exposure 
    {0x002A, 0x0538},
    {0x0F12, 0x5DC0},	
    {0x0F12, 0x0000},
    {0xFFFF, 10},	
    
    {0x0F12, 0x6D60},	
    {0x0F12, 0x0000},
    {0x002A, 0x1684},
    {0x0F12, 0x9C40},	
    {0x0F12, 0x0000},
    {0x0F12, 0xBB80},   
    {0x0F12, 0x0001},

// Set gain
    {0x002A, 0x0540},
    {0x0F12, 0x0150},	
    {0x0F12, 0x0280},	
    {0x002A, 0x168C},
    {0x0F12, 0x02A0},	
    {0x0F12, 0x0700},	
    {0x002A, 0x0544},
    {0x0F12, 0x0100},	
    {0x0F12, 0x1000},	
    {0x002A, 0x1694},
    {0x0F12, 0x0001},	
    {0x002A, 0x051A},
    {0x0F12, 0x0111},	
    {0x0F12, 0x00F0},	

//======================
// SET PREVIEW CONFIGURA
//======================
    {0x002A, 0x026C},
    {0x0F12, 0x0320},	
    {0x0F12, 0x0258},	
    {0x0F12, 0x0005},	
    {0x0F12, 0x157C},	
    {0x0F12, 0x157C},	
    {0x0F12, 0x0100},	
    {0x0F12, 0x0800},	
    {0x0F12, 0x0052},	
    {0x0F12, 0x4000},	
    {0x0F12, 0x0400},	
    {0x0F12, 0x0258},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x03E8},	
    {0x0F12, 0x029A},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	

//======================
// APPLY PREVIEW CONFIGU
//======================
    {0x002A, 0x023C},
    {0x0F12, 0x0000},	
    {0x002A, 0x0240},
    {0x0F12, 0x0001},	
    {0x002A, 0x0230},
    {0x0F12, 0x0001},	
    {0x002A, 0x023E},
    {0x0F12, 0x0001},	
    {0x002A, 0x0220},
    {0x0F12, 0x0001},	
    {0x0F12, 0x0001},	
   
    {0x0028, 0xD000},
    {0x002A, 0x1000},
    {0x0F12, 0x0001},
    {0x0028, 0x7000},  


///////////////////////////////////////////////////////////////s5k5cagx_640X480
    {0x002A, 0x026C},
//    {0x0F12, 0x0320},	 //800X600
//    {0x0F12, 0x0258},	
    {0x0F12, 0x0280}, //640X480	
    {0x0F12, 0x01E0},
    {0x0F12, 0x0005},	
    {0x0F12, 0x157C},	
    {0x0F12, 0x157C},	
    {0x0F12, 0x0100},	
    {0x0F12, 0x0800},	
    {0x0F12, 0x0052},	
    {0x0F12, 0x4000},	
    {0x0F12, 0x0400},	
    {0x0F12, 0x0258},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x03E8},	
    {0x0F12, 0x029A},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x0F12, 0x0000},	
    {0x002A, 0x023C},
    {0x0F12, 0x0000},	
    {0x002A, 0x0240},
    {0x0F12, 0x0001},	
    {0x002A, 0x0230},
    {0x0F12, 0x0001},	
    {0x002A, 0x023E},
    {0x0F12, 0x0001},	
    {0x002A, 0x0220},
    {0x0F12, 0x0001},	
    {0x0F12, 0x0001},	
};
	#else
		{
		//<CAMTUNING_PREVIEW>
		{0xFCFC, 0xD000},																				   
		{0x0028, 0x7000},
		{0x002A, 0x023C}, //Normal Preview//
		{0x0F12, 0x0000}, //config 0 // 												 
		{0x002A, 0x0240}, 
		{0x0F12, 0x0001},	
		{0x002A, 0x0230}, 
		{0x0F12, 0x0001},			   
		{0x002A, 0x023E}, 
		{0x0F12, 0x0001},
		{0x002A, 0x0220}, 
		{0x0F12, 0x0001},	
		{0x002A, 0x0222}, 
		{0x0F12, 0x0001},

	};
	#endif

LOCAL const SENSOR_REG_T s5k5cagx_1280X960[] =
{
#if 0
    {0x002A, 0x03B4},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0500},  
    {0x0F12, 0x03C0},  
    {0x0F12, 0x0009},  
    {0x0F12, 0x1770},  
    {0x0F12, 0x1770},  
    {0x0F12, 0x0100},  
    {0x0F12, 0x0800},  
    {0x0F12, 0x0052},  
    {0x0F12, 0x0050},  
    {0x0F12, 0x01E0},  
    {0x0F12, 0x0250},   
    {0x0F12, 0x0001},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0002},  
    {0x0F12, 0x07D0},  
    {0x0F12, 0x07D0},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0028, 0x7000},
    {0x002A, 0x0244},
    {0x0F12, 0x0002},	
    {0x002A, 0x0230},
    {0x0F12, 0x0001},	
    {0x002A, 0x0246},
    {0x0F12, 0x0001},	
    {0x002A, 0x0224},
    {0x0F12, 0x0001},	
    {0x0F12, 0x0001},	
#else
//<CAMTUNING_SNAPSHOT>
{0xFCFC, 0xD000},   
{0x0028, 0x7000}, 
{0x002A, 0x035E},  
{0x0F12, 0x0500},   //REG_0TC_CCFG_usWidth                      	2   7000035E          //                                   
{0x0F12, 0x0400},   //REG_0TC_CCFG_usHeight                     	2   70000360          //                                   
{0x002A, 0x0208},                                                                
{0x0F12, 0x0001},      //REG_TC_IPRM_InitParamsUpdated 
                                                    
{0x002A, 0x0244}, //Normal capture//
{0x0F12, 0x0000}, //config 0 //
{0x002A, 0x0230}, 
{0x0F12, 0x0001},	
{0x002A, 0x0246}, 
{0x0F12, 0x0001},              
{0x002A, 0x0224}, 
{0x0F12, 0x0001},	
{0x0F12, 0x0001},     
	{0xffff, 0x00A0},

#endif
}; 
 
LOCAL const SENSOR_REG_T s5k5cagx_1600X1200[] =
{ 
#if 0
    {0x002A, 0x0388},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0640},  
    {0x0F12, 0x04B0},  
    {0x0F12, 0x0009},  
    {0x0F12, 0x1770},  
    {0x0F12, 0x1770},  
    {0x0F12, 0x0100},  
    {0x0F12, 0x0800},  
    {0x0F12, 0x0052},  
    {0x0F12, 0x0050},  
    {0x0F12, 0x01E0},  
    {0x0F12, 0x0320},   
    {0x0F12, 0x0001},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0002},  
    {0x0F12, 0x07D0},  
    {0x0F12, 0x07D0},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0028, 0x7000},
    {0x002A, 0x0244},
    {0x0F12, 0x0001},	
    {0x002A, 0x0230},
    {0x0F12, 0x0001},	
    {0x002A, 0x0246},
    {0x0F12, 0x0001},	
    {0x002A, 0x0224},
    {0x0F12, 0x0001},	
    {0x0F12, 0x0001},	
  #else
  //<CAMTUNING_SNAPSHOT>
  {0xFCFC, 0xD000},   
  {0x0028, 0x7000}, 
  {0x002A, 0x035E},  
  {0x0F12, 0x0640},   //REG_0TC_CCFG_usWidth						  2   7000035E			//									 
  {0x0F12, 0x04B0},   //REG_0TC_CCFG_usHeight						  2   70000360			//									 
  {0x002A, 0x0208}, 															   
  {0x0F12, 0x0001}, 	 //REG_TC_IPRM_InitParamsUpdated 
													  
  {0x002A, 0x0244}, //Normal capture//
  {0x0F12, 0x0000}, //config 0 //
  {0x002A, 0x0230}, 
  {0x0F12, 0x0001},   
  {0x002A, 0x0246}, 
  {0x0F12, 0x0001}, 			 
  {0x002A, 0x0224}, 
  {0x0F12, 0x0001},   
  {0x0F12, 0x0001}, 	
  {0xffff, 0x00A0},

  #endif
}; 

LOCAL const SENSOR_REG_T s5k5cagx_2048X1536[] =
{
#if 0
    {0x002A, 0x035C},
    {0x0F12, 0x0000},  
    {0x0F12, 0x0800},  
    {0x0F12, 0x0600},  
    {0x0F12, 0x0009},  
    {0x0F12, 0x1770},  
    {0x0F12, 0x1770},  
    {0x0F12, 0x0100},  
    {0x0F12, 0x0800},  
    {0x0F12, 0x0052},  
    {0x0F12, 0x0050},  
    {0x0F12, 0x01E0},  
    {0x0F12, 0x047E},    
    {0x0F12, 0x0001},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0002},  
    {0x0F12, 0x07D0},  
    {0x0F12, 0x07D0},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0F12, 0x0000},  
    {0x0028, 0x7000},
    {0x002A, 0x0244},
    {0x0F12, 0x0000},	
    {0x002A, 0x0230},
    {0x0F12, 0x0001},	
    {0x002A, 0x0246},
    {0x0F12, 0x0001},	
    {0x002A, 0x0224},
    {0x0F12, 0x0001},	
    {0x0F12, 0x0001},	
 #else
 {0xFCFC, 0xD000},	 
 {0x0028, 0x7000}, 
 {0x002A, 0x035E},	
 {0x0F12, 0x0800},	 //REG_0TC_CCFG_usWidth 						 2	 7000035E		   //									
 {0x0F12, 0x0600},	 //REG_0TC_CCFG_usHeight						 2	 70000360		   //									
 {0x002A, 0x0208},																  
 {0x0F12, 0x0001},		//REG_TC_IPRM_InitParamsUpdated 
													 
 {0x002A, 0x0244}, //Normal capture//
 {0x0F12, 0x0000}, //config 0 //
 {0x002A, 0x0230}, 
 {0x0F12, 0x0001},	 
 {0x002A, 0x0246}, 
 {0x0F12, 0x0001},				
 {0x002A, 0x0224}, 
 {0x0F12, 0x0001},	 
 {0x0F12, 0x0001},
 {0xffff, 0x00A0},

 #endif
};

LOCAL SENSOR_REG_TAB_INFO_T s_s5k5cagx_resolution_Tab_YUV[]=
{    
    // COMMON INIT
        //{ADDR_AND_LEN_OF_ARRAY(s5k5cagx_common),   0, 0, 24, SENSOR_IMAGE_FORMAT_YUV422}, 
        {PNULL,   0, 0, 24, SENSOR_IMAGE_FORMAT_YUV422}, 
    
    // YUV422 PREVIEW 1 
    {ADDR_AND_LEN_OF_ARRAY(s5k5cagx_640X480),   640,  480,   24, SENSOR_IMAGE_FORMAT_YUV422}, 
    {ADDR_AND_LEN_OF_ARRAY(s5k5cagx_1280X960),  1280, 960,   24, SENSOR_IMAGE_FORMAT_YUV422}, 
    {ADDR_AND_LEN_OF_ARRAY(s5k5cagx_1600X1200), 1600, 1200,  24, SENSOR_IMAGE_FORMAT_YUV422}, 
    {ADDR_AND_LEN_OF_ARRAY(s5k5cagx_2048X1536), 2048, 1536,  24, SENSOR_IMAGE_FORMAT_YUV422},
    
    // YUV422 PREVIEW 2 
    {PNULL, 0, 0, 0, 0, 0}, 
    {PNULL, 0, 0, 0, 0, 0},
    {PNULL, 0, 0, 0, 0, 0},
    {PNULL, 0, 0, 0, 0, 0}
};
#if 0
LOCAL SENSOR_TRIM_T s_s5k5cagx_Resolution_Trim_Tab[]=
{    
    // COMMON INIT
    {0, 0, 640, 480, 0, 0},
    
    // YUV422 PREVIEW 1    
    {0, 0, 640, 480, 68, 56},
    {0, 0, 1280, 960, 122, 42},
    {0, 0, 1600, 1200, 122, 42},
    {0, 0, 2048, 1536, 122, 42},
    
    // YUV422 PREVIEW 2 
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}
};
#endif
//LOCAL EXIF_SPEC_PIC_TAKING_COND_T s_s5k5cagx_exif={0x00};

LOCAL SENSOR_IOCTL_FUNC_TAB_T s_s5k5cagx_ioctl_func_tab = 
{

    // Internal 
    _s5k5cagx_Reset,
    _s5k5cagx_PowerOn,
    PNULL,
    _s5k5cagx_Identify,

    PNULL,              // write register
    PNULL,              // read  register    
    _s5k5cagx_InitExt,
    PNULL,              //_s5k5cagx_GetResolutionTrimTab,

    // External
    PNULL,
    PNULL,
    PNULL,

    _s5k5cagx_set_brightness,
    _s5k5cagx_set_contrast,
    PNULL,
    _s5k5cagx_set_saturation,

    _s5k5cagx_set_work_mode,    
    _s5k5cagx_set_image_effect,

    _s5k5cagx_BeforeSnapshot,
    _s5k5cagx_after_snapshot,
    PNULL,
    PNULL,
    PNULL,
    PNULL,
    PNULL,
    PNULL,
    PNULL,
    PNULL,
    PNULL,
    _s5k5cagx_set_awb,
    PNULL,
    PNULL,
    _s5k5cagx_set_ev,
    _s5k5cagx_check_image_format_support,
    PNULL,
    PNULL,
    PNULL,              //_s5k5cagx_GetExifInfo,
    PNULL,
    PNULL,
    PNULL,
    _s5k5cagx_pick_out_jpeg_stream,
    PNULL
    };

/**---------------------------------------------------------------------------*
 **                         Global Variables                                  *
 **---------------------------------------------------------------------------*/
SENSOR_INFO_T g_s5k5cagx_yuv_info =
{
    s5k5cagx_I2C_ADDR_W,            // salve i2c write address
    s5k5cagx_I2C_ADDR_R,            // salve i2c read address
    
    SENSOR_I2C_REG_16BIT|\
    SENSOR_I2C_VAL_16BIT|\
    SENSOR_I2C_FREQ_400,           // bit0: 0: i2c register value is 8 bit, 1: i2c register value is 16 bit
                                    // bit1: 0: i2c register addr  is 8 bit, 1: i2c register addr  is 16 bit
                                    // other bit: reseved
    SENSOR_HW_SIGNAL_PCLK_P|\
    SENSOR_HW_SIGNAL_VSYNC_P|\
    SENSOR_HW_SIGNAL_HSYNC_P,       // bit0: 0:negative; 1:positive -> polarily of pixel clock
                                    // bit2: 0:negative; 1:positive -> polarily of horizontal synchronization signal
                                    // bit4: 0:negative; 1:positive -> polarily of vertical synchronization signal
                                    // other bit: reseved                                            
                                            
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
    0,
        
    0x77777,                    // bit[0:7]: count of step in brightness, contrast, sharpness, saturation
                                    // bit[8:31] reseved

    SENSOR_LOW_PULSE_RESET,         // reset pulse level

    200,                             // reset pulse width(ms)
    SENSOR_HIGH_LEVEL_PWDN,         // 1: high level valid; 0: low level valid
    
    1,                              // count of identify code
    {{0x0040, 0x05CA},                 // supply two code to identify sensor.
    {0x0042, 0x00B0}},                 // for Example: index = 0-> Device id, index = 1 -> version id                                            
                                            
    SENSOR_AVDD_2800MV,             // voltage of avdd    
    
    2048,                           // max width of source image
    1536,                           // max height of source image
    "s5k5cagx",                     // name of sensor                                                

    SENSOR_IMAGE_FORMAT_MAX,        // define in SENSOR_IMAGE_FORMAT_E enum,SENSOR_IMAGE_FORMAT_MAX
                                    // if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T

    SENSOR_IMAGE_PATTERN_YUV422_UYVY,   // pattern of input image form sensor;            

    s_s5k5cagx_resolution_Tab_YUV,  // point to resolution table information structure
    &s_s5k5cagx_ioctl_func_tab,     // point to ioctl function table
            
    PNULL,                          // information and table about Rawrgb sensor
    PNULL,                          // extend information about sensor
    SENSOR_AVDD_2800MV,             // iovdd
    SENSOR_AVDD_1800MV,             // dvdd
    1,                              // skip frame num before preview 
    2,                              // skip frame num before capture        
    0,                              // deci frame num during preview    
    0,                              // deci frame num during video preview      

    0,                              // threshold enable        
    0,                              // threshold mode     
    0,                              // threshold start postion    
    0,                              // threshold end postion 
    0
};

/******************************************************************************/
// Description: sensor s5k5cagx reset
// Global resource dependence: 
// Author: 
// Note:
/******************************************************************************/
LOCAL uint32_t _s5k5cagx_Reset(uint32_t level)
{
	int err = 0xff;
	SENSOR_IOCTL_FUNC_PTR	reset_func;
	printk("_s5k5cagx_Reset start.\n");
		
	err = gpio_request(72,"ccirrst");
	if (err) {
		printk("_s5k5cagx_Reset failed requesting err=%d\n", err);
		return ;
	}
	gpio_direction_output(72,level);	
	gpio_set_value(72,level);
	msleep(2);
	gpio_set_value(72,!level);		
         msleep(2);
	gpio_free(72);
	return 0;
}

BOOLEAN s_s5k5cagx_pwr_flag = SENSOR_FALSE;
#if 0
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: 
// Note:
/******************************************************************************/
LOCAL uint32_t _s5k5cagx_GetExifInfo(uint32_t param)
{
	//return (uint32_t)&s_s5k5cagx_exif;
	return 0;
}
#endif
/******************************************************************************/
// Description: get s5k5cagx rssolution trim tab
// Global resource dependence: 
// Author: 
// Note:
/******************************************************************************/
LOCAL uint32_t _s5k5cagx_InitExifInfo(void)
{
#if 0
    EXIF_SPEC_PIC_TAKING_COND_T* exif_ptr=&s_s5k5cagx_exif;

    SENSOR_PRINT("SENSOR: _s5k5cagx_InitExifInfo\n");

    exif_ptr->valid.FNumber=1;
    exif_ptr->FNumber.numerator=14;
    exif_ptr->FNumber.denominator=5;
    
    exif_ptr->valid.ExposureProgram=1;
    exif_ptr->ExposureProgram=0x04;

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
#if 0
/******************************************************************************/
// Description: get s5k5cagx rssolution trim tab
// Global resource dependence: 
// Author: 
// Note:
/******************************************************************************/
LOCAL uint32_t _s5k5cagx_GetResolutionTrimTab(uint32_t param)
{
    return (uint32_t)s_s5k5cagx_Resolution_Trim_Tab;
}
#endif
/******************************************************************************/
// Description: sensor s5k5cagx power on/down sequence
// Global resource dependence: 
// Author: 
// Note:
/******************************************************************************/
LOCAL uint32_t _s5k5cagx_PowerOn(uint32_t power_on)
{
	SENSOR_AVDD_VAL_E dvdd_val=g_s5k5cagx_yuv_info.dvdd_val;
	SENSOR_AVDD_VAL_E avdd_val=g_s5k5cagx_yuv_info.avdd_val;
	SENSOR_AVDD_VAL_E iovdd_val=g_s5k5cagx_yuv_info.iovdd_val;  
//	BOOLEAN power_down=g_s5k5cagx_yuv_info.power_down_level;        
	BOOLEAN reset_level=g_s5k5cagx_yuv_info.reset_pulse_level;
	uint32_t reset_width=g_s5k5cagx_yuv_info.reset_pulse_width;     
	   
	BOOLEAN power_down = (BOOLEAN)g_s5k5cagx_yuv_info.power_down_level;
	
	SENSOR_PRINT("SENSOR: _s5k5cagx_PowerOn(1:on, 0:off): Entry .\n");

	if(SENSOR_TRUE==power_on)
	{            
		SENSOR_PRINT("SENSOR: _s5k5cagx_PowerOn.\n");

		SENSOR_PRINT("SENSOR: _s5k5cagx_PowerOn dvdd/avdd/iovdd = {%d/%d/%d}.\n", 
		dvdd_val, avdd_val, iovdd_val);

		Sensor_SetVoltage(dvdd_val, avdd_val, iovdd_val);

		Sensor_PowerDown((BOOLEAN)!power_down);

		// Open Mclk in default frequency
		Sensor_SetMCLK(24);   

		msleep(20);
		Sensor_Reset(reset_level);		
		msleep(20);
		
	
	}
	else
	{
		SENSOR_PRINT("SENSOR: _s5k5cagx_PowerDown\n");
	  Sensor_PowerDown(power_down);
		Sensor_SetMCLK(SENSOR_DISABLE_MCLK);           
		Sensor_SetVoltage(SENSOR_AVDD_CLOSED, SENSOR_AVDD_CLOSED, SENSOR_AVDD_CLOSED);       
		//s_s5k5cagx_resolution_Tab_YUV[SENSOR_MODE_PREVIEW_ONE].sensor_reg_tab_ptr = (SENSOR_REG_T*)s5k5cagx_640X480; 
		//s_s5k5cagx_resolution_Tab_YUV[SENSOR_MODE_PREVIEW_ONE].reg_count = NUMBER_OF_ARRAY(s5k5cagx_640X480);     

		//msleep(20);
	}

	SENSOR_PRINT("SENSOR: _s5k5cagx_Power_On(1:on, 0:off): %d.\n", power_on);    
	SENSOR_PRINT("SENSOR: _s5k5cagx_PowerOn(1:on, 0:off): End.\n");

	return SENSOR_SUCCESS;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: 
// Note:
//        
/******************************************************************************/
LOCAL uint32_t _s5k5cagx_Identify(uint32_t param)
{
#define s5k5cagx_PID_VALUE    0x05CA    
#define s5k5cagx_PID_ADDR     0x0040
#define s5k5cagx_VER_VALUE    0x00B0    
#define s5k5cagx_VER_ADDR     0x0042
//Sensor_PowerDown(0);
	uint16_t pid_value = 0x00;
	uint32_t ret_value = SENSOR_FAIL;

	Sensor_WriteReg(0x0028, 0xD000);
	//Sensor_WriteReg(0x002A, 0x1006);
//while(1);
{

		printk("s5k5cagx_Identify Entry.\n");
	Sensor_WriteReg(0x0028, 0xD000);
}
	SENSOR_PRINT("s5k5cagx_Identify Entry.\n");
	pid_value = Sensor_ReadReg(0x1006);
	printk("s5k5cagx_Identify 0xD0001006 PID = 0x%x.\n", pid_value);
//			msleep(10);
	Sensor_WriteReg(0x002C, 0x0000);
	Sensor_WriteReg(0x002E, 0x0040);
	SENSOR_PRINT("s5k5cagx_Identify Entry\n");
	pid_value = Sensor_ReadReg(0x0F12);
	printk("s5k5cagx_Identify 0x00000040 PID = 0x%x.\n", pid_value);
//}
	if(s5k5cagx_PID_VALUE != pid_value)
	{
		printk("That is not s5k5cagx sensor ! error!\n");   
		return ret_value;
	}

	printk("That is s5k5cagx sensor !\n");        
	SENSOR_PRINT("s5k5cagx_Identify End\n");
	_s5k5cagx_InitExifInfo();

	return SENSOR_SUCCESS;
}

/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author: 
// Note:
//
/******************************************************************************/
LOCAL const SENSOR_REG_T s5k5cagx_brightness_tab[][4]=
{
    {//level -3
        {0x0028, 0x7000},  {0x002A ,0x020C}, {0x0F12 ,0xFF81}, {0xffff, 0xffff}
    },
    {//level -2
         {0x0028, 0x7000}, {0x002A ,0x020C}, {0x0F12 ,0xFFAC}, {0xffff, 0xffff}
    },
    {//level -1
        {0x0028, 0x7000},  {0x002A ,0x020C}, {0x0F12 ,0xFFD5}, {0xffff, 0xffff}
    },
    {//level 0
        {0x0028, 0x7000},  {0x002A ,0x020C}, {0x0F12 ,0x0000}, {0xffff, 0xffff}
    },
    {//level 1
        {0x0028, 0x7000},  {0x002A ,0x020C}, {0x0F12 ,0x002B}, {0xffff, 0xffff}
    },
    {//level 2
        {0x0028, 0x7000},  {0x002A ,0x020C}, {0x0F12 ,0x0057}, {0xffff, 0xffff}
    },
    {//level 3
         {0x0028, 0x7000}, {0x002A ,0x020C}, {0x0F12 ,0x007F}, {0xffff, 0xffff}
    }      
};

LOCAL uint32_t _s5k5cagx_set_brightness(uint32_t level)
{
	uint16_t i=0x00;
	SENSOR_REG_T_PTR sensor_reg_ptr=(SENSOR_REG_T_PTR)s5k5cagx_brightness_tab[level];
	if(level>6)
		return 0;
	//    SCI_ASSERT(PNULL!=sensor_reg_ptr);

	for(i=0x00; (0xffff!=sensor_reg_ptr[i].reg_addr)||(0xffff != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

	SENSOR_PRINT("SENSOR: s5k5cagx_set_brightness = 0x%02x.\n", level);
	return 0;
}

/******************************************************************************/
// Description: set contrast
// Global resource dependence: 
// Author: 
// Note:
//
/******************************************************************************/
LOCAL const SENSOR_REG_T s5k5cagx_contrast_tab[][4]=
{
    {//level -3
         {0x0028, 0x7000}, {0x002A ,0x020E}, {0x0F12 ,0xFF81}, {0xffff, 0xffff}
    },
    {//level -2
         {0x0028, 0x7000}, {0x002A ,0x020E}, {0x0F12 ,0xFFAC}, {0xffff, 0xffff}
    },
    {//level -1
         {0x0028, 0x7000}, {0x002A ,0x020E}, {0x0F12 ,0xFFD5}, {0xffff, 0xffff}
    },
    {//level 0
         {0x0028, 0x7000}, {0x002A ,0x020E}, {0x0F12 ,0x0000}, {0xffff, 0xffff}
    },
    {//level 1
         {0x0028, 0x7000}, {0x002A ,0x020E}, {0x0F12 ,0x002B}, {0xffff, 0xffff}
    },
    {//level 2
         {0x0028, 0x7000}, {0x002A ,0x020E}, {0x0F12 ,0x0057}, {0xffff, 0xffff}
    },
    {//level 3
         {0x0028, 0x7000}, {0x002A ,0x020E}, {0x0F12 ,0x007F}, {0xffff, 0xffff}
    }
};
    
LOCAL uint32_t _s5k5cagx_set_contrast(uint32_t level)
{
	uint16_t i=0x00;
	SENSOR_REG_T_PTR sensor_reg_ptr=(SENSOR_REG_T_PTR)s5k5cagx_contrast_tab[level];
	if(level>6)
		return 0;
	//   SCI_ASSERT(PNULL!=sensor_reg_ptr);

	for(i=0x00; (0xffff!=sensor_reg_ptr[i].reg_addr)||(0xffff != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

	//    Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_CONTRAST, (uint32)level);

	SENSOR_PRINT("SENSOR: s5k5cagx_set_contrast = 0x%02x.\n", level);
	return 0;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: 
// Note:
//        
/******************************************************************************/
LOCAL const SENSOR_REG_T s5k5cagx_sharpness_tab[][3]=
{
    {//level -3
        {0x002A ,0x0212}, {0x0F12 ,0xFF81}, {0xffff, 0xffff}
    },
    {//level -2
        {0x002A ,0x0212}, {0x0F12 ,0xFFAC}, {0xffff, 0xffff}
    },
    {//level -1
        {0x002A ,0x0212}, {0x0F12 ,0xFFD5}, {0xffff, 0xffff}
    },
    {//level 0
        {0x002A ,0x0212}, {0x0F12 ,0x0000}, {0xffff, 0xffff}
    },
    {//level 1
        {0x002A ,0x0212}, {0x0F12 ,0x002B}, {0xffff, 0xffff}
    },
    {//level 2
        {0x002A ,0x0212}, {0x0F12 ,0x0057}, {0xffff, 0xffff}
    },
    {//level 3
        {0x002A ,0x0212}, {0x0F12 ,0x007F}, {0xffff, 0xffff}
    }
};
#if 0
LOCAL uint32_t _s5k5cagx_set_sharpness(uint32_t level)
{
	uint16_t i=0x00;
	SENSOR_REG_T_PTR sensor_reg_ptr=(SENSOR_REG_T_PTR)s5k5cagx_sharpness_tab[level];

	//    SCI_ASSERT(PNULL!=sensor_reg_ptr);

	for(i=0x00; (0xffff!=sensor_reg_ptr[i].reg_addr)||(0xffff != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

	//   Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_SHARPNESS, (uint32)level);

	SENSOR_PRINT("SENSOR: s5k5cagx_set_sharpness = 0x%02x.\n", level);
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
LOCAL const SENSOR_REG_T s5k5cagx_saturation_tab[][3]=
{
    {//level -3
        {0x002A ,0x0210}, {0x0F12 ,0xFF81}, {0xffff, 0xffff}
    },
    {//level -2
        {0x002A ,0x0210}, {0x0F12 ,0xFFAC}, {0xffff, 0xffff}
    },
    {//level -1
        {0x002A ,0x0210}, {0x0F12 ,0xFFD5}, {0xffff, 0xffff}
    },
    {//level 0
        {0x002A ,0x0210}, {0x0F12 ,0x0000}, {0xffff, 0xffff}
    },
    {//level 1
        {0x002A ,0x0210}, {0x0F12 ,0x002B}, {0xffff, 0xffff}
    },
    {//level 2
        {0x002A ,0x0210}, {0x0F12 ,0x0057}, {0xffff, 0xffff}
    },
    {//level 3
        {0x002A ,0x0210}, {0x0F12 ,0x007F}, {0xffff, 0xffff}
    }
};
    
LOCAL uint32_t _s5k5cagx_set_saturation(uint32_t level)
{
	uint16_t i=0x00;
	SENSOR_REG_T_PTR sensor_reg_ptr=(SENSOR_REG_T_PTR)s5k5cagx_saturation_tab[level];
	if(level>6)
		return 0;
	//    SCI_ASSERT(PNULL!=sensor_reg_ptr);

	for(i=0x00; (0xffff!=sensor_reg_ptr[i].reg_addr)||(0xffff != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}
	SENSOR_PRINT("SENSOR: s5k5cagx_set_saturation = 0x%02x.\n", level);
	return 0;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: 
// Note:
//        
/******************************************************************************/
LOCAL const SENSOR_REG_T s5k5cagx_image_effect_tab[][14]=
{
    // effect normal        --effect normal
    {
	{0x0028, 0x7000}, 
	{0x002A, 0x3286}, 
	{0x0F12, 0x0001}, 
	{0x002A, 0x021E}, 
	{0x0F12, 0x0000}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff}		
    },
    // effect monochrome    --effect BLACKWHITE
    {
	{0x0028, 0x7000}, 
	{0x002A, 0x021E}, 
	{0x0F12, 0x0001}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},		
    },
    {
	{0x0028, 0x7000}, 
	{0x002A, 0x3286}, 
	{0x0F12, 0x0001}, 
	{0x002A, 0x021E}, 
	{0x0F12, 0x0000}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff}			
    },
        {
	{0x0028, 0x7000}, 
	{0x002A, 0x3286}, 
	{0x0F12, 0x0001}, 
	{0x002A, 0x021E}, 
	{0x0F12, 0x0000}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff}			
    },
        {
	{0x0028, 0x7000}, 
	{0x002A, 0x3286}, 
	{0x0F12, 0x0001}, 
	{0x002A, 0x021E}, 
	{0x0F12, 0x0000}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff}			
    },
    // effect         
    {
	{0x0028, 0x7000}, 
	{0x002A, 0x021E}, 
	{0x0F12, 0x0004}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},	
    },
        // effect
    {
	{0x0028, 0x7000}, 
	{0x002A, 0x021E}, 
	{0x0F12, 0x0003}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
    },
    /*
    // effect  aqua         --effect  BLUE
    {
	{0x0028, 0x7000}, 
	{0x002A, 0x021E}, 
	{0x0F12, 0x0005}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},	/*
/*
	{0x002A, 0x021E},
	{0x0F12, 0x0000},
	{0x002A, 0x04D2},
	{0x0F12, 0x0677},
	{0x002A, 0x04A0},
	{0x0F12, 0x0100},
	{0x0F12, 0x0001},
	{0x002A, 0x04A4},
	{0x0F12, 0x0100},
	{0x0F12, 0x0001},
	{0x002A, 0x04A8},
	{0x0F12, 0x0200},
	{0x0F12, 0x0001},
	{0xffff, 0xffff}

    },
    // effect  Sepia
    {
	{0x0028, 0x7000}, 
	{0x002A, 0x021E}, 
	{0x0F12, 0x0004}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},	

	{0x002A, 0x021E},
	{0x0F12, 0x0000},
	{0x002A, 0x04D2},
	{0x0F12, 0x0677},
	{0x002A, 0x04A0},
	{0x0F12, 0x0200},
	{0x0F12, 0x0001},
	{0x002A, 0x04A4},
	{0x0F12, 0x0200},
	{0x0F12, 0x0001},
	{0x002A, 0x04A8},
	{0x0F12, 0x0100},
	{0x0F12, 0x0001},

	{0xffff, 0xffff}
    },  
    // effect NEGATIVE
    {         
	{0x0028, 0x7000},
	{0x002A, 0x021E}, 
	{0x0F12, 0x0002}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff}		
    },    
    //effect Brown
    {
	{0x0028, 0x7000}, 
	{0x002A, 0x021E}, 
	{0x0F12, 0x0004}, 
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff},
	{0xffff, 0xffff}		
    },*/
};

LOCAL uint32_t _s5k5cagx_set_image_effect(uint32_t effect_type)
{
	uint16_t i=0x00;
	SENSOR_REG_T_PTR sensor_reg_ptr=(SENSOR_REG_T_PTR)s5k5cagx_image_effect_tab[effect_type];
	if(effect_type>7)
		return 0;
	//  SCI_ASSERT(PNULL!=sensor_reg_ptr);

	for(i=0x00; (0xffff!=sensor_reg_ptr[i].reg_addr)||(0xffff != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

	SENSOR_PRINT("SENSOR: s5k5cagx_set_image_effect = 0x%02x.\n", effect_type);
	return 0;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: 
// Note:
//        
/******************************************************************************/
LOCAL const SENSOR_REG_T s5k5cagx_ev_tab[][4]=
{
    {//level -3
	{0x0028, 0x7000},{0x002A, 0x0F70},{0x0F12, 0x000C}, {0xffff, 0xffff}
    },
    {//level -2
	{0x0028, 0x7000},{0x002A, 0x0F70},{0x0F12, 0x001E}, {0xffff, 0xffff}
    },
    {//level -1
	{0x0028, 0x7000},{0x002A, 0x0F70},{0x0F12, 0x0030}, {0xffff, 0xffff}
    },
    {//level 0
	{0x0028, 0x7000},{0x002A, 0x0F70},{0x0F12, 0x0042}, {0xffff, 0xffff}
    },
    {//level 1
	{0x0028, 0x7000},{0x002A, 0x0F70},{0x0F12, 0x0054}, {0xffff, 0xffff}
    },
    {//level 2
	{0x0028, 0x7000},{0x002A, 0x0F70},{0x0F12, 0x0066}, {0xffff, 0xffff}
    },
    {//level 3
	{0x0028, 0x7000},{0x002A, 0x0F70},{0x0F12, 0x0078}, {0xffff, 0xffff}
    }
};

LOCAL uint32_t _s5k5cagx_set_ev(uint32_t level)
{    
	SENSOR_REG_T_PTR sensor_reg_ptr=(SENSOR_REG_T_PTR)s5k5cagx_ev_tab[level];
	uint16_t i=0x00;
	if(level>6)
		return 0;
	//   SCI_ASSERT(PNULL!=sensor_reg_ptr);

	for(i=0x00; (0xffff!=sensor_reg_ptr[i].reg_addr)||(0xffff != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

	SENSOR_PRINT("SENSOR: s5k5cagx_set_ev = 0x%02x.\n", level);

	return 0;
}

#if 0
/******************************************************************************/
// Description: set video mode(nand)
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
LOCAL const SENSOR_REG_T s5k5cagx_video_mode_nand_tab[][56]=
{
    //DC mode
    {
			{0x0028, 0x7000},		
			{0x002A, 0x026C},	//Normal preview 15fps
			{0x0F12, 0x0280},	//REG_0TC_PCFG_usWidth							2	7000026C			//							  
			{0x0F12, 0x01E0},	//REG_0TC_PCFG_usHeight 						2	7000026E	//										   
			{0x0F12, 0x0005},	//REG_0TC_PCFG_Format							2	70000270	//										   
			{0x0F12, 0x39A4},	//3AA8//REG_0TC_PCFG_usMaxOut4KHzRate				2	70000272  //							  
			{0x0F12, 0x37A4},	//3A88//REG_0TC_PCFG_usMinOut4KHzRate				2	70000274  //							  
			{0x0F12, 0x0100},	//REG_0TC_PCFG_OutClkPerPix88					2	70000276	//										   
			{0x0F12, 0x0800},	//REG_0TC_PCFG_uMaxBpp88						2	70000278	//										   
			{0x0F12, 0x0092},	//REG_0TC_PCFG_PVIMask							2	7000027A	//92  (1) PCLK inversion  (4)1b_C first (5) UV First									   
			{0x0F12, 0x0010},	//REG_0TC_PCFG_OIFMask							2	7000027C	//										   
			{0x0F12, 0x01E0},	//REG_0TC_PCFG_usJpegPacketSize 				2	7000027E	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_usJpegTotalPackets				2	70000280	//										   
			{0x0F12, 0x0002},	//REG_0TC_PCFG_uClockInd						2	70000282	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_usFrTimeType 					2	70000284	//										   
			{0x0F12, 0x0001},	//REG_0TC_PCFG_FrRateQualityType				2	70000286	//										   
			{0x0F12, 0x014E},	//REG_0TC_PCFG_usMaxFrTimeMsecMult10			2	70000288	//										   
			{0x0F12, 0x014E},	//REG_0TC_PCFG_usMinFrTimeMsecMult10			2	7000028A	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_bSmearOutput 					2	7000028C	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_sSaturation						2	7000028E	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_sSharpBlur						2	70000290	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_sColorTemp						2	70000292	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_uDeviceGammaIndex				2	70000294	//										   
			{0x0F12, 0x0003},	//01 REG_0TC_PCFG_uPrevMirror						2	70000296	//										   
			{0x0F12, 0x0003},	//01 REG_0TC_PCFG_uCaptureMirror					2	70000298	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_uRotation	


			{0x002A, 0x023c},                                         
            {0x0F12, 0x0000}, 
            {0x002A, 0x0240},                                         
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x0230},                                              
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x023E},                                           
            {0x0F12, 0x0001},
            {0x002A, 0x0220},
            {0x0F12, 0x0001},
            {0x0F12, 0x0001}, 
            {0xffff, 0xffff}
    },
    //video mode
    {
			{0x0028, 0x7000},		
			{0x002A, 0x026C},	//Normal preview 15fps
			{0x0F12, 0x0280},	//REG_0TC_PCFG_usWidth							2	7000026C			//							  
			{0x0F12, 0x01E0},	//REG_0TC_PCFG_usHeight 						2	7000026E	//										   
			{0x0F12, 0x0005},	//REG_0TC_PCFG_Format							2	70000270	//										   
			{0x0F12, 0x39A4},	//3AA8//REG_0TC_PCFG_usMaxOut4KHzRate				2	70000272  //							  
			{0x0F12, 0x37A4},	//3A88//REG_0TC_PCFG_usMinOut4KHzRate				2	70000274  //							  
			{0x0F12, 0x0100},	//REG_0TC_PCFG_OutClkPerPix88					2	70000276	//										   
			{0x0F12, 0x0800},	//REG_0TC_PCFG_uMaxBpp88						2	70000278	//										   
			{0x0F12, 0x0092},	//REG_0TC_PCFG_PVIMask							2	7000027A	//92  (1) PCLK inversion  (4)1b_C first (5) UV First									   
			{0x0F12, 0x0010},	//REG_0TC_PCFG_OIFMask							2	7000027C	//										   
			{0x0F12, 0x01E0},	//REG_0TC_PCFG_usJpegPacketSize 				2	7000027E	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_usJpegTotalPackets				2	70000280	//										   
			{0x0F12, 0x0002},	//REG_0TC_PCFG_uClockInd						2	70000282	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_usFrTimeType 					2	70000284	//										   
			{0x0F12, 0x0001},	//REG_0TC_PCFG_FrRateQualityType				2	70000286	//										   
			{0x0F12, 0x014E},	//REG_0TC_PCFG_usMaxFrTimeMsecMult10			2	70000288	//										   
			{0x0F12, 0x014E},	//REG_0TC_PCFG_usMinFrTimeMsecMult10			2	7000028A	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_bSmearOutput 					2	7000028C	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_sSaturation						2	7000028E	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_sSharpBlur						2	70000290	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_sColorTemp						2	70000292	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_uDeviceGammaIndex				2	70000294	//										   
			{0x0F12, 0x0003},	//01 REG_0TC_PCFG_uPrevMirror						2	70000296	//										   
			{0x0F12, 0x0003},	//01 REG_0TC_PCFG_uCaptureMirror					2	70000298	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_uRotation		
    {0x002A, 0x023C},
    {0x0F12, 0x0000},	
    {0x002A, 0x0240},
    {0x0F12, 0x0001},	
    {0x002A, 0x0230},
    {0x0F12, 0x0001},	
    {0x002A, 0x023E},
    {0x0F12, 0x0001},	
    {0x002A, 0x0220},
    {0x0F12, 0x0001},	
    {0x0F12, 0x0001},
    {0xffff, 0xffff}	
    },
};
/******************************************************************************/
// Description: set video mode(nor)
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
//LOCAL const SENSOR_REG_T s5k5cagx_video_mode_nor_tab[][56]=
//{

//}; 

//#define s5k5cagx_video_mode_wqvga_nand_tab s5k5cagx_video_mode_nor_tab

LOCAL uint32_t _s5k5cagx_set_video_mode(uint32_t mode)
{
#if 0
	uint16_t i=0x00;
	//SENSOR_REG_T_PTR sensor_reg_ptr = PNULL;

	//if(CHIP_DetectMemType())
//#ifdef MAINLCD_SIZE_240X400
//	sensor_reg_ptr = (SENSOR_REG_T*)s5k5cagx_video_mode_wqvga_nand_tab[mode];
//#else
	SENSOR_REG_T_PTR sensor_reg_ptr = (SENSOR_REG_T_PTR)s5k5cagx_video_mode_nand_tab[mode];
//#endif
	//else
//#ifdef MAINLCD_SIZE_240X400
//	SENSOR_PRINT("Not support NOR wqvga yet");
//#else
//	sensor_reg_ptr = (SENSOR_REG_T*)ov3660_video_mode_nor_tab[mode];
//#endif

//	SCI_ASSERT(PNULL != sensor_reg_ptr);

	for(i = 0x00; (0xffff != sensor_reg_ptr[i].reg_addr) || (0xffff != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}
	SENSOR_PRINT("SENSOR: _s5k5cagx_set_video_mode = 0x%02x.\n", mode);
#endif

	return 0;
}

#endif

/******************************************************************************/
// Description: set wb mode 
// Global resource dependence: 
// Author: 
// Note:
//        
/******************************************************************************/
LOCAL const SENSOR_REG_T s5k5cagx_awb_tab[][14]=
{
    //AUTO
    {
 	{0xFCFC, 0xD000},    	
	{0x0028, 0x7000}, 
	{0x002A ,0x246E}, {0x0F12 ,0x0001}, {0xffff, 0xffff},
	{0xffff, 0xffff}, {0xffff, 0xffff}, {0xffff, 0xffff},
	{0xffff, 0xffff}, {0xffff, 0xffff}, {0xffff, 0xffff},
	{0xffff, 0xffff}, {0xffff, 0xffff}, {0xffff, 0xffff},
    },    
    //INCANDESCENCE:

        {
 	{0xFCFC, 0xD000},    	
	{0x0028, 0x7000}, 
	{0x002A ,0x246E}, {0x0F12 ,0x0000}, {0x002a ,0x04A0}, 
	{0x0F12 ,0x0400}, {0x0F12 ,0x0001}, {0x0F12 ,0x0400}, 
	{0x0F12 ,0x0001}, {0x0F12 ,0x0940}, {0x0F12 ,0x0001}, 
	{0xffff ,0xffff}, {0xffff ,0xffff}, {0xffff, 0xffff}                 
    },
    {
 	{0xFCFC, 0xD000},    	
	{0x0028, 0x7000}, 
	{0x002A ,0x246E}, {0x0F12 ,0x0001}, {0xffff, 0xffff},
	{0xffff, 0xffff}, {0xffff, 0xffff}, {0xffff, 0xffff},
	{0xffff, 0xffff}, {0xffff, 0xffff}, {0xffff, 0xffff},
	{0xffff, 0xffff}, {0xffff, 0xffff}, {0xffff, 0xffff},
    },    
    {
 	{0xFCFC, 0xD000},    	
	{0x0028, 0x7000}, 
	{0x002A ,0x246E}, {0x0F12 ,0x0001}, {0xffff, 0xffff},
	{0xffff, 0xffff}, {0xffff, 0xffff}, {0xffff, 0xffff},
	{0xffff, 0xffff}, {0xffff, 0xffff}, {0xffff, 0xffff},
	{0xffff, 0xffff}, {0xffff, 0xffff}, {0xffff, 0xffff},
    },    
        //SUN:
    {
 	{0xFCFC, 0xD000},    	
	{0x0028, 0x7000}, 
	{0x002A ,0x246E}, {0x0F12 ,0x0000}, {0x002a ,0x04A0},
	{0x0F12 ,0x0575}, {0x0F12 ,0x0001}, {0x0F12 ,0x0400}, 
	{0x0F12 ,0x0001}, {0x0F12 ,0x0800}, {0x0F12 ,0x0001}, 
	{0xffff ,0xffff}, {0xffff ,0xffff}, {0xffff, 0xffff}
    },
    //U30(auto)
    {
 	{0xFCFC, 0xD000},    	
	{0x0028, 0x7000}, 
	{0x002A ,0x246E}, {0x0F12 ,0x0000}, {0x002a ,0x04A0}, 
	{0x0F12 ,0x05E0}, {0x0F12 ,0x0001}, {0x0F12 ,0x0400}, 
	{0x0F12 ,0x0001}, {0x0F12 ,0x0530}, {0x0F12 ,0x0001}, 
	{0xffff ,0xffff}, {0xffff ,0xffff}, {0xffff, 0xffff}           
    },  

    //CLOUD:
    {
 	{0xFCFC, 0xD000},    	
	{0x0028, 0x7000}, 
	{0x002A ,0x246E}, {0x0F12 ,0x0000}, {0x002a ,0x04A0}, 
	{0x0F12 ,0x0740}, {0x0F12 ,0x0001}, {0x0F12 ,0x0400}, 
	{0x0F12 ,0x0001}, {0x0F12 ,0x0460}, {0x0F12 ,0x0001}, 
	{0xffff ,0xffff}, {0xffff ,0xffff}, {0xffff, 0xffff} 
    },
    /*
    //CWF equal to flourescent(auto)
    {
	{0x0028, 0x7000}, 
	{0x002A ,0x04D2}, {0x0F12 ,0x0677}, {0x002A ,0x04A0}, 
	{0x0F12 ,0x0100}, {0x0F12 ,0x0001}, {0x002A ,0x04A4}, 
	{0x0F12 ,0x0100}, {0x0F12 ,0x0001}, {0x002A ,0x04A8}, 
	{0x0F12 ,0x021E}, {0x0F12 ,0x0001}, {0xffff, 0xffff}           
    },    
    //FLUORESCENT:
    {
	{0x0028, 0x7000}, 
	{0x002A ,0x04D2}, {0x0F12 ,0x0677}, {0x002A ,0x04A0},
	{0x0F12 ,0x0145}, {0x0F12 ,0x0001}, {0x002A ,0x04A4}, 
	{0x0F12 ,0x0100}, {0x0F12 ,0x0001}, {0x002A ,0x04A8}, 
	{0x0F12 ,0x01EB}, {0x0F12 ,0x0001}, {0xffff, 0xffff}
    }*/
};

LOCAL uint32_t _s5k5cagx_set_awb(uint32_t mode)
{   
	uint16_t i=0x00;
	SENSOR_REG_T_PTR sensor_reg_ptr=(SENSOR_REG_T_PTR)s5k5cagx_awb_tab[mode];
	if(mode>6)
		return 0;
	//  SCI_ASSERT(PNULL!=sensor_reg_ptr);

	for(i=0; (0xffff!=sensor_reg_ptr[i].reg_addr)||(0xffff != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

	//   Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_LIGHTSOURCE, (uint32)mode);

	SENSOR_PRINT("SENSOR: s5k5cagx_set_awb = 0x%02x\n", mode);
	return 0;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: 
// Note:
//        mode 0:normal;   1:night 
/******************************************************************************/
LOCAL const SENSOR_REG_T s5k5cagx_work_mode_tab[][100]=
{
    {    //normal fix 15fps
            {0x0028, 0x7000},   
            {0x002A, 0x0288},			
            {0x0F12, 0x03E8}, //0535                   
            {0x0F12, 0x01F4},
            
            {0x002A, 0x020C},
            {0x0F12, 0x0000},// Brightness for controling the EV//
            {0x002A, 0x0210},
            {0x0F12, 0x0000},// Saturation//
            {0x0F12, 0x0000},// Blur_Sharpness //     
            
            {0x002A, 0x12B8},  
            {0x0F12, 0x1000},  
            {0x002A, 0x0530},   	                                                    
            {0x0F12, 0x3415},   //lt_uMaxExp1	32 30ms  9~10ea// 15fps  // 
            {0x002A, 0x0534},                                                               
            {0x0F12, 0x682A},   //lt_uMaxExp2	67 65ms	18~20ea // 7.5fps //
            {0x002A, 0x167C},                                                               
            {0x0F12, 0x8235},   //9C40//MaxExp3  83 80ms  24~25ea //                 
            {0x002A, 0x1680},                                                               
            {0x0F12, 0xc350},   //MaxExp4   125ms  38ea //                                   
            {0x002A, 0x0538},                                                               
            {0x0F12, 0x3415},   // 15fps //                                                 
            {0x002A, 0x053C},                                                               
            {0x0F12, 0x682A},   // 7.5fps //                                                
            {0x002A, 0x1684},                                                               
            {0x0F12, 0x8235},   //CapMaxExp3 //                                             
            {0x002A, 0x1688},                                                               
            {0x0F12, 0xC350},   //CapMaxExp4 //                                             
            {0x002A, 0x0540},                                                               
            {0x0F12, 0x01B3},    //0170//0150//lt_uMaxAnGain1_700lux//                                              
            {0x0F12, 0x01B3},   //0200//0400//lt_uMaxAnGain2_400lux//                              
            {0x002A, 0x168C},                                                               
            {0x0F12, 0x02A0},   //0300//MaxAnGain3_200lux//                                       
            {0x0F12, 0x0710},   //MaxAnGain4 //    
            {0x002A, 0x0544},  
            {0x0F12, 0x0100},
            {0x0F12, 0x8000},
            {0x002A, 0x04B4},  
            {0x0F12, 0x0001},
            {0x0F12, 0x0064},
            {0x0F12, 0x0001},
      
            {0x002A, 0x023c},                                         
            {0x0F12, 0x0000}, 
            {0x002A, 0x0240},                                         
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x0230},                                              
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x023E},                                           
            {0x0F12, 0x0001},
            {0x002A, 0x0220},
            {0x0F12, 0x0001},
            {0x0F12, 0x0001}, 
            {0xffff, 0xffff}
    },
    {    //night 
            {0x0028, 0x7000},   
            {0x002A, 0x0288},			
            {0x0F12, 0x03E8},                    
            {0x0F12, 0x029A},
            
            {0x002A, 0x020C},
            {0x0F12, 0x0000},// Brightness for controling the EV//
            {0x002A, 0x0210},
            {0x0F12, 0x0000},// Saturation//
            {0x0F12, 0x0000},// Blur_Sharpness //     
            
            {0x002A, 0x12B8},  
            {0x0F12, 0x1000},  
            {0x002A, 0x0530},   	                                                    
            {0x0F12, 0x3415},   //lt_uMaxExp1	32 30ms  9~10ea// 15fps  // 
            {0x002A, 0x0534},                                                               
            {0x0F12, 0x682A},   //lt_uMaxExp2	67 65ms	18~20ea // 7.5fps //
            {0x002A, 0x167C},                                                               
            {0x0F12, 0x8235},   //9C40//MaxExp3  83 80ms  24~25ea //                 
            {0x002A, 0x1680},                                                               
            {0x0F12, 0xc350},   //MaxExp4   125ms  38ea //                                   
            {0x002A, 0x0538},                                                               
            {0x0F12, 0x3415},   // 15fps //                                                 
            {0x002A, 0x053C},                                                               
            {0x0F12, 0x682A},   // 7.5fps //                                                
            {0x002A, 0x1684},                                                               
            {0x0F12, 0x8235},   //CapMaxExp3 //                                             
            {0x002A, 0x1688},                                                               
            {0x0F12, 0xC350},   //CapMaxExp4 //                                             
            {0x002A, 0x0540},                                                               
            {0x0F12, 0x01B3},    //0170//0150//lt_uMaxAnGain1_700lux//                                              
            {0x0F12, 0x01B3},   //0200//0400//lt_uMaxAnGain2_400lux//                              
            {0x002A, 0x168C},                                                               
            {0x0F12, 0x02A0},   //0300//MaxAnGain3_200lux//                                       
            {0x0F12, 0x0710},   //MaxAnGain4 //    
            {0x002A, 0x0544},  
            {0x0F12, 0x0100},
            {0x0F12, 0x8000},
            {0x002A, 0x04B4},  
            {0x0F12, 0x0001},
            {0x0F12, 0x0064},
            {0x0F12, 0x0001},
      
            {0x002A, 0x023c},                                         
            {0x0F12, 0x0000}, 
            {0x002A, 0x0240},                                         
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x0230},                                              
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x023E},                                           
            {0x0F12, 0x0001},
            {0x002A, 0x0220},
            {0x0F12, 0x0001},
            {0x0F12, 0x0001}, //clear setting of night/sport

    	
            {0x0028, 0x7000},   
            {0x002A, 0x1680},                                                                       
            {0x0F12, 0x1A80},  
            {0x0F12, 0x0006}, 
            {0x002A, 0x1688},                                                                       
            {0x0F12, 0x1A80},  
            {0x0F12, 0x0006}, 
                                                                    
            {0x002A, 0x0288},                                                                       
            {0x0F12, 0x0535},    //0x0535 // 7.5fps // 
            {0x0F12, 0x0535},    //0x0535 // 7.5fps // 
            {0x002A, 0x023c},                                         
            {0x0F12, 0x0000}, 
            {0x002A, 0x0240},                                         
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x0230},                                              
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x023E},                                           
            {0x0F12, 0x0001},
            {0x002A, 0x0220},                                           
            {0x0F12, 0x0001},
            {0x0F12, 0x0001},  
            {0xffff, 0xffff}
    },
    //SCENE_PORTRAIT 
		{
            {0x0028, 0x7000},   
            {0x002A, 0x0288},			
            {0x0F12, 0x03E8},                    
            {0x0F12, 0x029A},
            
            {0x002A, 0x020C},
            {0x0F12, 0x0000},// Brightness for controling the EV//
            {0x002A, 0x0210},
            {0x0F12, 0x0000},// Saturation//
            {0x0F12, 0x0000},// Blur_Sharpness //     
            
            {0x002A, 0x12B8},  
            {0x0F12, 0x1000},  
            {0x002A, 0x0530},   	                                                    
            {0x0F12, 0x3415},   //lt_uMaxExp1	32 30ms  9~10ea// 15fps  // 
            {0x002A, 0x0534},                                                               
            {0x0F12, 0x682A},   //lt_uMaxExp2	67 65ms	18~20ea // 7.5fps //
            {0x002A, 0x167C},                                                               
            {0x0F12, 0x8235},   //9C40//MaxExp3  83 80ms  24~25ea //                 
            {0x002A, 0x1680},                                                               
            {0x0F12, 0xc350},   //MaxExp4   125ms  38ea //                                   
            {0x002A, 0x0538},                                                               
            {0x0F12, 0x3415},   // 15fps //                                                 
            {0x002A, 0x053C},                                                               
            {0x0F12, 0x682A},   // 7.5fps //                                                
            {0x002A, 0x1684},                                                               
            {0x0F12, 0x8235},   //CapMaxExp3 //                                             
            {0x002A, 0x1688},                                                               
            {0x0F12, 0xC350},   //CapMaxExp4 //                                             
            {0x002A, 0x0540},                                                               
            {0x0F12, 0x01B3},    //0170//0150//lt_uMaxAnGain1_700lux//                                              
            {0x0F12, 0x01B3},   //0200//0400//lt_uMaxAnGain2_400lux//                              
            {0x002A, 0x168C},                                                               
            {0x0F12, 0x02A0},   //0300//MaxAnGain3_200lux//                                       
            {0x0F12, 0x0710},   //MaxAnGain4 //    
            {0x002A, 0x0544},  
            {0x0F12, 0x0100},
            {0x0F12, 0x8000},
            {0x002A, 0x04B4},  
            {0x0F12, 0x0001},
            {0x0F12, 0x0064},
            {0x0F12, 0x0001},
      
            {0x002A, 0x023c},                                         
            {0x0F12, 0x0000}, 
            {0x002A, 0x0240},                                         
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x0230},                                              
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x023E},                                           
            {0x0F12, 0x0001},
            {0x002A, 0x0220},
            {0x0F12, 0x0001},
            {0x0F12, 0x0001}, //clear setting of night/sport
            			
					{0xFCFC, 0xD000},
					{0x0028, 0x7000},
					{0x002A, 0x020C},
					{0x0F12, 0x0000},
					{0x002A, 0x0210},
					{0x0F12, 0x0000},
					{0x0F12, 0xFFF6},
            {0xffff, 0xffff}
		},
		//LANDSCAPE
		{
            {0x0028, 0x7000},   
            {0x002A, 0x0288},			
            {0x0F12, 0x03E8},                    
            {0x0F12, 0x029A},
            
            {0x002A, 0x020C},
            {0x0F12, 0x0000},// Brightness for controling the EV//
            {0x002A, 0x0210},
            {0x0F12, 0x0000},// Saturation//
            {0x0F12, 0x0000},// Blur_Sharpness //     
            
            {0x002A, 0x12B8},  
            {0x0F12, 0x1000},  
            {0x002A, 0x0530},   	                                                    
            {0x0F12, 0x3415},   //lt_uMaxExp1	32 30ms  9~10ea// 15fps  // 
            {0x002A, 0x0534},                                                               
            {0x0F12, 0x682A},   //lt_uMaxExp2	67 65ms	18~20ea // 7.5fps //
            {0x002A, 0x167C},                                                               
            {0x0F12, 0x8235},   //9C40//MaxExp3  83 80ms  24~25ea //                 
            {0x002A, 0x1680},                                                               
            {0x0F12, 0xc350},   //MaxExp4   125ms  38ea //                                   
            {0x002A, 0x0538},                                                               
            {0x0F12, 0x3415},   // 15fps //                                                 
            {0x002A, 0x053C},                                                               
            {0x0F12, 0x682A},   // 7.5fps //                                                
            {0x002A, 0x1684},                                                               
            {0x0F12, 0x8235},   //CapMaxExp3 //                                             
            {0x002A, 0x1688},                                                               
            {0x0F12, 0xC350},   //CapMaxExp4 //                                             
            {0x002A, 0x0540},                                                               
            {0x0F12, 0x01B3},    //0170//0150//lt_uMaxAnGain1_700lux//                                              
            {0x0F12, 0x01B3},   //0200//0400//lt_uMaxAnGain2_400lux//                              
            {0x002A, 0x168C},                                                               
            {0x0F12, 0x02A0},   //0300//MaxAnGain3_200lux//                                       
            {0x0F12, 0x0710},   //MaxAnGain4 //    
            {0x002A, 0x0544},  
            {0x0F12, 0x0100},
            {0x0F12, 0x8000},
            {0x002A, 0x04B4},  
            {0x0F12, 0x0001},
            {0x0F12, 0x0064},
            {0x0F12, 0x0001},
      
            {0x002A, 0x023c},                                         
            {0x0F12, 0x0000}, 
            {0x002A, 0x0240},                                         
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x0230},                                              
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x023E},                                           
            {0x0F12, 0x0001},
            {0x002A, 0x0220},
            {0x0F12, 0x0001},
            {0x0F12, 0x0001}, //clear setting of night/sport
            			
					{0xFCFC,0xD000},
					{0x0028,0x7000},
					{0x002A,0x020C},
					{0x0F12,0x0000},
					{0x002A,0x0210},
					{0x0F12,0x001E},
					{0x0F12,0x000A},
            {0xffff, 0xffff}
		},
		//sport
		{
            {0x0028, 0x7000},   
            {0x002A, 0x0288},			
            {0x0F12, 0x03E8},                    
            {0x0F12, 0x029A},
            
            {0x002A, 0x020C},
            {0x0F12, 0x0000},// Brightness for controling the EV//
            {0x002A, 0x0210},
            {0x0F12, 0x0000},// Saturation//
            {0x0F12, 0x0000},// Blur_Sharpness //     
            
            {0x002A, 0x12B8},  
            {0x0F12, 0x1000},  
            {0x002A, 0x0530},   	                                                    
            {0x0F12, 0x3415},   //lt_uMaxExp1	32 30ms  9~10ea// 15fps  // 
            {0x002A, 0x0534},                                                               
            {0x0F12, 0x682A},   //lt_uMaxExp2	67 65ms	18~20ea // 7.5fps //
            {0x002A, 0x167C},                                                               
            {0x0F12, 0x8235},   //9C40//MaxExp3  83 80ms  24~25ea //                 
            {0x002A, 0x1680},                                                               
            {0x0F12, 0xc350},   //MaxExp4   125ms  38ea //                                   
            {0x002A, 0x0538},                                                               
            {0x0F12, 0x3415},   // 15fps //                                                 
            {0x002A, 0x053C},                                                               
            {0x0F12, 0x682A},   // 7.5fps //                                                
            {0x002A, 0x1684},                                                               
            {0x0F12, 0x8235},   //CapMaxExp3 //                                             
            {0x002A, 0x1688},                                                               
            {0x0F12, 0xC350},   //CapMaxExp4 //                                             
            {0x002A, 0x0540},                                                               
            {0x0F12, 0x01B3},    //0170//0150//lt_uMaxAnGain1_700lux//                                              
            {0x0F12, 0x01B3},   //0200//0400//lt_uMaxAnGain2_400lux//                              
            {0x002A, 0x168C},                                                               
            {0x0F12, 0x02A0},   //0300//MaxAnGain3_200lux//                                       
            {0x0F12, 0x0710},   //MaxAnGain4 //    
            {0x002A, 0x0544},  
            {0x0F12, 0x0100},
            {0x0F12, 0x8000},
            {0x002A, 0x04B4},  
            {0x0F12, 0x0001},
            {0x0F12, 0x0064},
            {0x0F12, 0x0001},
      
            {0x002A, 0x023c},                                         
            {0x0F12, 0x0000}, 
            {0x002A, 0x0240},                                         
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x0230},                                              
            {0x0F12, 0x0001},                                                                       
            {0x002A, 0x023E},                                           
            {0x0F12, 0x0001},
            {0x002A, 0x0220},
            {0x0F12, 0x0001},
            {0x0F12, 0x0001}, //clear setting of night/sport
            			
					{0xfcfc,0xd000},
					{0x0028,0x7000},
					{0x002A,0x12B8},  
					{0x0F12,0x2000}, 
					{0x002A,0x0530},   	                                                    
					{0x0F12,0x36B0},   //lt_uMaxExp1	32 30ms  9~10ea// 15fps  // 
					{0x002A,0x0534},                                                               
					{0x0F12,0x36B0},   //lt_uMaxExp2	67 65ms	18~20ea // 7.5fps //
					{0x002A,0x167C},                                                               
					{0x0F12,0x36B0},   //9C40//MaxExp3  83 80ms  24~25ea //                 
					{0x002A,0x1680},                                                               
					{0x0F12,0x36B0},   //MaxExp4   125ms  38ea //                                   
					{0x002A,0x0538},                                                               
					{0x0F12,0x36B0},   // 15fps //                                                 
					{0x002A,0x053C},                                                               
					{0x0F12,0x36B0},   // 7.5fps //                                                
					{0x002A,0x1684},                                                               
					{0x0F12,0x36B0},   //CapMaxExp3 //                                             
					{0x002A,0x1688},                                                               
					{0x0F12,0x36B0},   //CapMaxExp4 //                                             
					{0x002A,0x0540},                                                               
					{0x0F12,0x0200},    //0170//0150//lt_uMaxAnGain1_700lux//                                              
					{0x0F12,0x0200},   //0200//0400//lt_uMaxAnGain2_400lux//                              
					{0x002A,0x168C},                                                               
					{0x0F12,0x0200},   //0300//MaxAnGain3_200lux//                                       
					{0x0F12,0x0200},   //MaxAnGain4 //    
					{0x002A,0x0544},  
					{0x0F12,0x0100},
					{0x0F12,0x8000},
					{0x002A,0x04B4},  
					{0x0F12,0x0001},
					{0x0F12,0x00C8},
					{0x0F12,0x0001},
            {0xffff, 0xffff}
		},
		
};

LOCAL uint32_t _s5k5cagx_set_work_mode(uint32_t mode)
{
	uint16_t i=0x00;
	SENSOR_REG_T_PTR sensor_reg_ptr=(SENSOR_REG_T_PTR)s5k5cagx_work_mode_tab[mode];
	if(mode>4)
		return 0;
//	SCI_ASSERT(PNULL != sensor_reg_ptr);

	for(i=0; (0xffff!=sensor_reg_ptr[i].reg_addr)||(0xffff != sensor_reg_ptr[i].reg_value); i++)
	{
		Sensor_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

	//   Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_SCENECAPTURETYPE, (uint32)mode);

	SENSOR_PRINT("SENSOR: set_work_mode: mode = %d.\n", mode);
	return 0;
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: 
// Note:
//        
/******************************************************************************/
LOCAL uint32_t _s5k5cagx_BeforeSnapshot(uint32_t param)
{
	SENSOR_PRINT("_s5k5cagx_BeforeSnapshot =%d.\n",param);    

	Sensor_SetMode(param);

	return SENSOR_SUCCESS;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: 
// Note:
//        
/******************************************************************************/
LOCAL uint32_t _s5k5cagx_check_image_format_support(uint32_t param) 
{
	uint32_t ret_val = SENSOR_FAIL;

	switch(param)
	{
		case SENSOR_IMAGE_FORMAT_YUV422:
			ret_val = SENSOR_SUCCESS;
			break;
		case SENSOR_IMAGE_FORMAT_JPEG:
			ret_val = SENSOR_SUCCESS;
			break;
		default:
			SENSOR_PRINT("s5k5cagx only support SENSOR_IMAGE_FORMAT_JPEG & SENSOR_IMAGE_FORMAT_YUV422, input is %d\n",param);    
			break;
	}

	return ret_val;
}
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: 
// Note:
//        
/******************************************************************************/
LOCAL uint32_t _s5k5cagx_pick_out_jpeg_stream(uint32_t param)
{
#if 0
	uint8_t* p_frame=((DCAMERA_SNAPSHOT_RETURN_PARAM_T*)param)->return_data_addr;
	uint32_t buf_len=((DCAMERA_SNAPSHOT_RETURN_PARAM_T*)param)->return_data_len;
	uint32_t i=0x00;
    
    	SENSOR_PRINT("s5k5cagx: jpeg capture head: 0x%x, 0x%x.\n",*((uint8*)p_frame), *((uint8*)p_frame + 1));

	/* Find the tail position */
	for(i=0x00; i<buf_len; i++)
	{
#define TAIL_VAL 0xffd9
		uint8_t* p_cur_val = (uint8*)p_frame;
		uint16_t tail_val = ((p_cur_val[i]<<8) | p_cur_val[i+1]);
		if(TAIL_VAL == tail_val)
		{
			i += 2;
			break;
		}
	}

	    /* check if the tail is found */
	if(i < buf_len)
	{
		SENSOR_PRINT("s5k5cagx: Found the jpeg tail at %d: 0x%x 0x%x.\n", i+1, *((uint8*)p_frame + i), *((uint8*)p_frame + i + 1));
	}
	else
	{
		SENSOR_PRINT("s5k5cagx: can not find the jpeg tail: %d.\n", i);
		i=0x00;
	}
	return i;
#endif
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
LOCAL uint32_t _s5k5cagx_chang_image_format(uint32_t param)
{
	uint32_t ret_val = SENSOR_FAIL;
	SENSOR_REG_TAB_INFO_T st_yuv422_reg_table_info = { ADDR_AND_LEN_OF_ARRAY(s5k5cagx_640X480),0,0,0,0};

	switch(param)
	{
		case SENSOR_IMAGE_FORMAT_YUV422:
			SENSOR_PRINT("s5k5cagx  chang_image_format  YUV422.\n");    
			ret_val = Sensor_SendRegTabToSensor(&st_yuv422_reg_table_info);
			break;
		case SENSOR_IMAGE_FORMAT_JPEG:
			SENSOR_PRINT("s5k5cagx  chang_image_format  jpg.\n");    
			ret_val = SENSOR_FAIL;//Sensor_SendRegTabToSensor(&st_jpeg_reg_table_info);
			break;
		default:
			SENSOR_PRINT ("s5k5cagx only support SENSOR_IMAGE_FORMAT_JPEG &SENSOR_IMAGE_FORMAT_YUV422, input is %d.\n", param);
			break;
	}

	return ret_val;
}
#endif
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: 
// Note:
//        
/******************************************************************************/
LOCAL const SENSOR_REG_T s5k5cagx_preview[]=
{
    
			{0x0028, 0x7000},		
			{0x002A, 0x026C},	//Normal preview 15fps
			{0x0F12, 0x0280},	//REG_0TC_PCFG_usWidth							2	7000026C			//							  
			{0x0F12, 0x01E0},	//REG_0TC_PCFG_usHeight 						2	7000026E	//										   
			{0x0F12, 0x0005},	//REG_0TC_PCFG_Format							2	70000270	//										   
			{0x0F12, 0x39A4},	//3AA8//REG_0TC_PCFG_usMaxOut4KHzRate				2	70000272  //							  
			{0x0F12, 0x37A4},	//3A88//REG_0TC_PCFG_usMinOut4KHzRate				2	70000274  //							  
			{0x0F12, 0x0100},	//REG_0TC_PCFG_OutClkPerPix88					2	70000276	//										   
			{0x0F12, 0x0800},	//REG_0TC_PCFG_uMaxBpp88						2	70000278	//										   
			{0x0F12, 0x0092},	//REG_0TC_PCFG_PVIMask							2	7000027A	//92  (1) PCLK inversion  (4)1b_C first (5) UV First									   
			{0x0F12, 0x0010},	//REG_0TC_PCFG_OIFMask							2	7000027C	//										   
			{0x0F12, 0x01E0},	//REG_0TC_PCFG_usJpegPacketSize 				2	7000027E	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_usJpegTotalPackets				2	70000280	//										   
			{0x0F12, 0x0002},	//REG_0TC_PCFG_uClockInd						2	70000282	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_usFrTimeType 					2	70000284	//										   
			{0x0F12, 0x0001},	//REG_0TC_PCFG_FrRateQualityType				2	70000286	//										   
			{0x0F12, 0x03E8},//0535	//REG_0TC_PCFG_usMaxFrTimeMsecMult10			2	70000288	//										   
			{0x0F12, 0x01F4},	//REG_0TC_PCFG_usMinFrTimeMsecMult10			2	7000028A	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_bSmearOutput 					2	7000028C	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_sSaturation						2	7000028E	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_sSharpBlur						2	70000290	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_sColorTemp						2	70000292	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_uDeviceGammaIndex				2	70000294	//										   
			{0x0F12, 0x0003},	//01 REG_0TC_PCFG_uPrevMirror						2	70000296	//										   
			{0x0F12, 0x0003},	//01 REG_0TC_PCFG_uCaptureMirror					2	70000298	//										   
			{0x0F12, 0x0000},	//REG_0TC_PCFG_uRotation		
    {0x002A, 0x023C},
    {0x0F12, 0x0000},	
    {0x002A, 0x0240},
    {0x0F12, 0x0001},	
    {0x002A, 0x0230},
    {0x0F12, 0x0001},	
    {0x002A, 0x023E},
    {0x0F12, 0x0001},	
    {0x002A, 0x0220},
    {0x0F12, 0x0001},	
    {0x0F12, 0x0001},	
};

LOCAL uint32_t _s5k5cagx_after_snapshot(uint32_t param)
{
	s_s5k5cagx_resolution_Tab_YUV[SENSOR_MODE_PREVIEW_ONE].sensor_reg_tab_ptr = (SENSOR_REG_T*)s5k5cagx_preview; 
	s_s5k5cagx_resolution_Tab_YUV[SENSOR_MODE_PREVIEW_ONE].reg_count = NUMBER_OF_ARRAY(s5k5cagx_preview);     

	SENSOR_PRINT("_s5k5cagx_after_snapshot =%d.\n",param);    

	Sensor_SetMode(param);
	return SENSOR_SUCCESS;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: Baggio.he
// Note:
//        
/******************************************************************************/
LOCAL uint32_t _s5k5cagx_InitExt(uint32_t param)
{
	uint32_t              rtn = SENSOR_SUCCESS;
	int ret = 0;	
	uint32_t              i = 0;
	uint32_t              written_num = 0;
	uint16_t              wr_reg = 0;  
	uint16_t              wr_val = 0;      
	uint32_t              wr_num_once = 0;
	uint32_t              wr_num_once_ret = 0;
	uint32_t              init_table_size = NUMBER_OF_ARRAY(s5k5cagx_common);
	SENSOR_REG_T_PTR    p_reg_table = s5k5cagx_common;
	uint8_t               *p_reg_val_tmp = 0;
	struct i2c_msg msg_w;
	struct i2c_client *i2c_client = Sensor_GetI2CClien();
 	struct timeval time1;
	do_gettimeofday(&time1);
	printk("_s5k5cagx_InitExt start use new time sec: %ld, usec: %ld.\n", time1.tv_sec,time1.tv_usec);   
	//printk("SENSOR: _s5k5cagx_InitExt"); 
	if(0 == i2c_client)
	{
		printk("SENSOR: _s5k5cagx_InitExt:error,i2c_client is NULL!.\n");
	}
	p_reg_val_tmp = (uint8_t*)kzalloc(init_table_size*sizeof(uint16_t), GFP_KERNEL);        
	while(written_num < init_table_size)
	{
		wr_reg = p_reg_table[written_num].reg_addr;
		wr_val = p_reg_table[written_num].reg_value;
		if(SENSOR_WRITE_DELAY == wr_reg)
		{
			if(wr_val >= 10)
			{
				msleep(wr_val);
			}
			else
			{
				mdelay(wr_val);
			}
		}
		else
		{
			p_reg_val_tmp[0] = (uint8)((wr_reg >> 8) & 0xFF);
			p_reg_val_tmp[1] = (uint8)(wr_reg & 0xFF);       
			p_reg_val_tmp[2] = (uint8)((wr_val >> 8) & 0xFF);
			p_reg_val_tmp[3] = (uint8)(wr_val & 0xFF);        
			wr_num_once = 2;
			for(i = written_num + 1; i< init_table_size; i++)
			{
				if(p_reg_table[i].reg_addr != wr_reg)   
				{
					break;
				}
				else
				{
					wr_val = p_reg_table[i].reg_value;
					p_reg_val_tmp[2*wr_num_once] = (uint8)((wr_val >> 8) & 0xFF);
					p_reg_val_tmp[2*wr_num_once+1] = (uint8)(wr_val & 0xFF);        
					wr_num_once ++;
					if(wr_num_once >= I2C_WRITE_BURST_LENGTH)
					{
						break;
					}
				}
			}		
			msg_w.addr = i2c_client->addr;
			msg_w.flags = 0;
			msg_w.buf = p_reg_val_tmp;
			msg_w.len = (uint32)(wr_num_once*2);
			ret = i2c_transfer(i2c_client->adapter, &msg_w, 1);			
			if(ret!=1)
			{
				printk("SENSOR: _s5k5cagx_InitExt, i2c write once error");
				rtn = 1;
				break;
			}
			else
			{
#if 0			
				SENSOR_PRINT("SENSOR: _s5k5cagx_InitExt, i2c write once from %d {0x%x 0x%x}, total %d registers {0x%x 0x%x}\n",
				      written_num,cmd[0],cmd[1],wr_num_once,p_reg_val_tmp[0],p_reg_val_tmp[1]);  
				if(wr_num_once > 1)
				{
					SENSOR_PRINT("SENSOR: _s5k5cagx_InitExt, val {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x}.\n",
				          p_reg_val_tmp[0],p_reg_val_tmp[1],p_reg_val_tmp[2],p_reg_val_tmp[3],
				          p_reg_val_tmp[4],p_reg_val_tmp[5],p_reg_val_tmp[6],p_reg_val_tmp[7],
				          p_reg_val_tmp[8],p_reg_val_tmp[9],p_reg_val_tmp[10],p_reg_val_tmp[11]);                      

				}
#endif
			}
		}
		written_num += wr_num_once-1;
	}
	do_gettimeofday(&time1);
	printk("_s5k5cagx_InitExt end use new time sec: %ld, usec: %ld.\n", time1.tv_sec,time1.tv_usec);   
    //printk("SENSOR: _s5k5cagx_InitExt, success");
    kfree(p_reg_val_tmp);
    return rtn;
}


#if 0
/******************************************************************************/
// Description:
// Global resource dependence: 
// Author: Baggio.he
// Note:
//        
/******************************************************************************/
LOCAL uint32_t _s5k5cagx_ExtFunc(uint32_t ctl_param)
{
	uint32_t                  rtn=SENSOR_SUCCESS;
	SENSOR_EXT_FUN_T_PTR    ext_ptr=(SENSOR_EXT_FUN_T_PTR)ctl_param;

	SENSOR_PRINT("SENSOR: _s5k5cagx_ExtFunc cmd:0x%x.\n",ext_ptr->cmd);    

	switch(ext_ptr->cmd)
	{
		case SENSOR_EXT_FUNC_INIT:
			rtn=_s5k5cagx_InitExt(ctl_param);        
			break;
		default :            
			SENSOR_PRINT("SENSOR: _s5k5cagx_ExtFunc unsupported command.\n");    
			rtn = SENSOR_FAIL;                
			break;
	}

	return rtn;
}
#endif
LOCAL uint32_t _s5k5cagx_EnterSleep(uint32_t pwd_level)
{
	//    GPIO_SetSensorPwdn((BOOLEAN)pwd_level);
	msleep(20);
	if(pwd_level == g_s5k5cagx_yuv_info.power_down_level)
	{
		s_s5k5cagx_resolution_Tab_YUV[SENSOR_MODE_PREVIEW_ONE].sensor_reg_tab_ptr = (SENSOR_REG_T*)s5k5cagx_640X480; 
		s_s5k5cagx_resolution_Tab_YUV[SENSOR_MODE_PREVIEW_ONE].reg_count = NUMBER_OF_ARRAY(s5k5cagx_640X480);     
	}
	return 0;
}
struct sensor_drv_cfg sensor_s5k5cagx = {
        .sensor_pos = CONFIG_DCAM_SENSOR_POS_S5K5CAGX,
        .sensor_name = "s5k5cagx",
        .driver_info = &g_s5k5cagx_yuv_info,
};

static int __init sensor_s5k5cagx_init(void)
{
        return dcam_register_sensor_drv(&sensor_s5k5cagx);
}
subsys_initcall(sensor_s5k5cagx_init);
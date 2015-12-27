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
#include <linux/delay.h>
#include <linux/kernel.h>
/**---------------------------------------------------------------------------*
 **                     Local Function Prototypes                             *
 **---------------------------------------------------------------------------*/
LOCAL uint32_t _nmi600_Identify(uint32_t param);
LOCAL uint32_t Set_NMI600_Brightness(uint32_t level);
LOCAL uint32_t Set_NMI600_Contrast(uint32_t level);
LOCAL uint32_t Set_NMI600_Preview_Mode(uint32_t preview_mode);
LOCAL uint32_t Set_NMI600_Image_Effect(uint32_t effect_type);
LOCAL uint32_t NMI600_Before_Snapshot(uint32_t sensor_snapshot_mode);
LOCAL uint32_t NMI600_After_Snapshot(uint32_t para);
LOCAL uint32_t Set_NMI600_AWB(uint32_t mode);
LOCAL uint32_t Set_NMI600_Ev(uint32_t level);
LOCAL uint32_t NMI600_Change_Image_Format(uint32_t param);
LOCAL uint32_t Set_NMI600_Anti_Flicker(uint32_t mode);
LOCAL uint32_t Set_NMI600_Video_Mode(uint32_t mode);
/**---------------------------------------------------------------------------*
 **                         Local Variables                                  *
 **---------------------------------------------------------------------------*/
//LOCAL ATV_INFO_T s_atv_info= {0x00};
//LOCAL ATV_INFO_T_PTR s_atv_info_ptr=&s_atv_info;
//LOCAL uint16_t s_emc_param[2]= {0xf140, 0xf0f0};
LOCAL SENSOR_REG_TAB_INFO_T s_nmi600_resolution_Tab_YUV[]=
{
        // COMMON INIT
        { PNULL, 0, 0, 0, 24, SENSOR_IMAGE_FORMAT_YUV422},//320,240??
        // YUV422 PREVIEW 1
        { PNULL, 0, 320, 240, 24, SENSOR_IMAGE_FORMAT_YUV422},//320,240
        { PNULL, 0, 0, 0, 0, 0},
        { PNULL, 0, 0, 0, 0, 0},
        { PNULL, 0, 0, 0, 0, 0},
        // YUV422 PREVIEW 2
        { PNULL, 0, 0, 0, 0, 0},
        { PNULL, 0, 0, 0, 0, 0},
        { PNULL, 0, 0, 0, 0, 0},
        { PNULL, 0, 0, 0, 0, 0}
};

/*LOCAL SENSOR_TRIM_T s_nmi600_resolution_trim_tab[]=
{
    // COMMON INIT
    {0, 0, 0, 0},

    // YUV422 PREVIEW 1
    {0, 0, 320, 240},
    {0, 0, 0, 0},

    {0, 0, 0, 0},
    {0, 0, 0, 0},

    // YUV422 PREVIEW 2
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
};*/

LOCAL SENSOR_IOCTL_FUNC_TAB_T s_nmi600_ioctl_func_tab =
{
        // Internal
        PNULL,
        PNULL,//_nmi600_PowerOnSequence,////////////
        PNULL,
        _nmi600_Identify,
        PNULL,          // write register
        PNULL,          // read  register
        // Custom function
        PNULL,//_nmi600_IOCTL,///////////////
        PNULL,//_nmi600_GetResolutionTrimTab,/////////////

        // External
        PNULL,
        PNULL,
        PNULL,


        Set_NMI600_Brightness,//PNULL,
        Set_NMI600_Contrast,//PNULL,
        PNULL,
        PNULL,
        Set_NMI600_Preview_Mode,//PNULL,
        Set_NMI600_Image_Effect,//PNULL,
        NMI600_Before_Snapshot,//PNULL,
        NMI600_After_Snapshot,//PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        PNULL,
        Set_NMI600_AWB,//PNULL,
        PNULL,
        PNULL,
        Set_NMI600_Ev,//PNULL,
        PNULL,
        NMI600_Change_Image_Format,//PNULL,
        PNULL,
        PNULL,
        PNULL,
        Set_NMI600_Anti_Flicker,//PNULL,
        Set_NMI600_Video_Mode,//PNULL,
        PNULL,
};

/**---------------------------------------------------------------------------*
 **                         Global Variables                                  *
 **---------------------------------------------------------------------------*/
SENSOR_INFO_T g_nmi600_yuv_info =
{
        0x60,               // salve i2c write address
        0x60,               // salve i2c read address

        SENSOR_I2C_REG_16BIT,           // bit0: 0: i2c register value is 8 bit, 1: i2c register value is 16 bit
                                    // bit1: 0: i2c register addr  is 8 bit, 1: i2c register addr  is 16 bit
                                    // other bit: reseved
        SENSOR_HW_SIGNAL_PCLK_N | SENSOR_HW_SIGNAL_VSYNC_N | SENSOR_HW_SIGNAL_HSYNC_P,
        //SENSOR_HW_SIGNAL_VSYNC_N | \      //SENSOR_HW_SIGNAL_VSYNC_N
        //SENSOR_HW_SIGNAL_HSYNC_P,       // bit0: 0:negative; 1:positive -> polarily of pixel clock
                                    // bit2: 0:negative; 1:positive -> polarily of horizontal synchronization signal
                                    // bit4: 0:negative; 1:positive -> polarily of vertical synchronization signal
                                    // other bit: reseved
        // environment mode
        0x00,
        // image effect
        0x00,
        // while balance mode
        0x00,
        0x00,                       // brightness/contrast/sharpness/saturation/EV
        SENSOR_LOW_PULSE_RESET,         // reset pulse level
        200,                                // reset pulse width(ms)
        SENSOR_LOW_LEVEL_PWDN,     // 1: high level valid; 0: low level valid

        0x00,                               // count of identify code
        {{0x00, 0x00},                     // supply two code to identify sensor.
        {0x00, 0x00}},                     // for Example: index = 0-> Device id, index = 1 -> version id

        SENSOR_AVDD_2800MV,             // voltage of avdd

        320,                            // max width of source image
        240,                            // max height of source image
        "nmi600",                      // name of sensor

        SENSOR_IMAGE_FORMAT_YUV422,     // define in SENSOR_IMAGE_FORMAT_E enum,
        // if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T
        SENSOR_IMAGE_PATTERN_YUV422_UYVY,   // pattern of input image form sensor;

        s_nmi600_resolution_Tab_YUV,   // point to resolution table information structure
        &s_nmi600_ioctl_func_tab,      // point to ioctl function table

        PNULL,                          // information and table about Rawrgb sensor
        PNULL,                          // extend information about sensor
        SENSOR_AVDD_1800MV,                     // iovdd
        SENSOR_AVDD_1800MV,                      // dvdd
        4,                     // skip frame num before preview
        3,                     // skip frame num before capture
        0,                     // deci frame num during preview
        2,                     // deci frame num during video preview
        0,                     // threadhold eb
        1,                     // threadhold mode
        0x1d0,                  // threadhold st
        0x2d0,                  // threadhold en
        0
};
EXPORT_SYMBOL_GPL(g_nmi600_yuv_info);

LOCAL uint32_t Set_NMI600_Brightness(uint32_t level)
{
        printk("%s\n",__func__);
        return 0;
}
LOCAL uint32_t Set_NMI600_Contrast(uint32_t level)
{
        printk("%s\n",__func__);
        return 0;
}
LOCAL uint32_t Set_NMI600_Preview_Mode(uint32_t preview_mode)
{
        printk("nmi:%d,%s\n",preview_mode,__func__);
        //Sensor_SetMode(1);/////////////////
        return 0;
}
LOCAL uint32_t Set_NMI600_Image_Effect(uint32_t effect_type)
{
        printk("%s\n",__func__);
        return 0;
}
LOCAL uint32_t NMI600_Before_Snapshot(uint32_t sensor_snapshot_mode)
{
        printk("%d,%s\n",sensor_snapshot_mode,__func__);
        Sensor_SetMode(sensor_snapshot_mode);	
        return 0;
}
LOCAL uint32_t NMI600_After_Snapshot(uint32_t para)
{	
        //NMI600_Change_Image_Format(SENSOR_IMAGE_FORMAT_YUV422);
        printk("%s\n",__func__);
        return 0;
}
LOCAL uint32_t Set_NMI600_AWB(uint32_t mode)
{
        printk("%s\n",__func__);	
        return 0;
}
LOCAL uint32_t Set_NMI600_Ev(uint32_t level)
{
        printk("%s\n",__func__);
        return 0;
}
LOCAL uint32_t NMI600_Change_Image_Format(uint32_t param)
{
        printk("%s\n",__func__);
        return 0;
}
LOCAL uint32_t Set_NMI600_Anti_Flicker(uint32_t mode)
{ 
        printk("%s\n",__func__);
        return 0;
}
LOCAL uint32_t Set_NMI600_Video_Mode(uint32_t mode)
{
        printk("%s\n",__func__);
        return 0;
}	
/**---------------------------------------------------------------------------*
 **                             Function  Definitions
 **---------------------------------------------------------------------------*/
/******************************************************************************/
// Description: nmi600 get infor
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL ATV_INFO_T_PTR _nmi600_GetInfo(void)
{
    return s_atv_info_ptr;
}*/

/******************************************************************************/
// Description: get ov7670 rssolution trim tab
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32_t _nmi600_GetResolutionTrimTab(uint32_t param)
{
    return(uint32_t) s_nmi600_resolution_trim_tab;
}*/

/******************************************************************************/
// Description: nmi600 init channel
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_InitChannel(void)
{
    uint32 rtn=SCI_SUCCESS;
    ATV_INFO_T_PTR atv_info_ptr=_nmi600_GetInfo();
    uint32 CurrRegion=nChina;

    SCI_TRACE_LOW("SENSOR: _nmi600_InitChannel");

    switch(atv_info_ptr->atv_cur_region)
    {
        case ATV_REGION_ARGENTINA:
        case ATV_REGION_PARAGURY:
        case ATV_REGION_URUGUAY:
            CurrRegion=nArgentina;
            break;

        case ATV_REGION_BRAZIL:
            CurrRegion=nBrazil;
            break;

        case ATV_REGION_UK:
        case ATV_REGION_HONGKONG:
        case ATV_REGION_SOUTHAFRICA:
            CurrRegion=nUk;
            break;

        case ATV_REGION_AFGHANISTAN:
        case ATV_REGION_CAMBODIA:
        case ATV_REGION_INDIA:	
        case ATV_REGION_INDONESIA:
        case ATV_REGION_MALAYSIA:
        case ATV_REGION_LAOS:
        case ATV_REGION_IRAN:
        case ATV_REGION_SANDIARABIA:
        case ATV_REGION_IRAQ:
        case ATV_REGION_LEBANON:	
        case ATV_REGION_PAKISTAN:
        case ATV_REGION_SINGAPORE:
        case ATV_REGION_UAE:
        case ATV_REGION_THAILAND:
        case ATV_REGION_TURKEY:
        case ATV_REGION_AUSTRALIA:
        case ATV_REGION_WESTERNEUROPE:
        case ATV_REGION_SPAIN:
        case ATV_REGION_PORTUGAL:
        case ATV_REGION_EGYPT:
        case ATV_REGION_LIBYA:
        case ATV_REGION_GERMANY:
            CurrRegion=nAfghanistan;
            break;

        case ATV_REGION_TUNISIA:
            CurrRegion=nTunisia;
            break;

        case ATV_REGION_CONGO:
        case ATV_REGION_RUSSIA:
        case ATV_REGION_JAPAN:
            CurrRegion=nCongo;
            break;

        case ATV_REGION_VIETNAM:
        case ATV_REGION_BULGARIA:
        case ATV_REGION_HUNGRY:
        case ATV_REGION_POLAND:
        case ATV_REGION_ROMANIA:
            CurrRegion=nVietnam;
            break;

        case ATV_REGION_CHINA:
            CurrRegion=nChina;
            break;

        case ATV_REGION_SHENZHEN:
            CurrRegion=nChinaSz;
            break;

        case ATV_REGION_USA:
        case ATV_REGION_CANADA:
        case ATV_REGION_TAIWAN:
        case ATV_REGION_CHILE:
        case ATV_REGION_PHILIPPINES:
        //case ATV_REGION_COLUMBIA:
        case ATV_REGION_VENEZUELA:
        //case ATV_REGION_BOLIVIA:
        case ATV_REGION_MEXICO:
        case ATV_REGION_KOREA:
        case ATV_REGION_MYANMAR:
            CurrRegion=nUsa;
            break;		

        default:
            CurrRegion=nChina;
            break;
    }

    atv_nmi600_set_region(CurrRegion);
    atv_nmi_init_scan_param(CurrRegion);

    return rtn;
}*/

/******************************************************************************/
// Description: sensor nmi600 power on/down sequence
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_PowerOnSequence(uint32 power_on)
{
    SENSOR_AVDD_VAL_E dvdd_val=g_nmi600_yuv_info.dvdd_val;
    SENSOR_AVDD_VAL_E avdd_val=g_nmi600_yuv_info.avdd_val;
    SENSOR_AVDD_VAL_E iovdd_val=g_nmi600_yuv_info.iovdd_val;
    BOOLEAN reset_level=g_nmi600_yuv_info.reset_pulse_level;
    uint32 reset_width=g_nmi600_yuv_info.reset_pulse_width;

    SCI_TRACE_LOW("ATV: _nmi600_Power_On(1:on, 0:off): %d", power_on);

    if(SCI_TRUE==power_on)
    {
        SCI_Sleep(20);
        Sensor_SetVoltage(dvdd_val, avdd_val, iovdd_val);
        SCI_Sleep(100);
        Sensor_SetResetLevel(reset_level);
        SCI_Sleep(reset_width);
        Sensor_SetResetLevel((BOOLEAN)!reset_level);
        SCI_Sleep(100);
    }
    else
    {
        SCI_Sleep(20);
        Sensor_SetResetLevel(reset_level);
        SCI_Sleep(20);
        Sensor_SetVoltage(SENSOR_AVDD_CLOSED, SENSOR_AVDD_CLOSED, SENSOR_AVDD_CLOSED);
        SCI_Sleep(20);
    }

    SCI_TRACE_LOW("ATV: _nmi600_Power_On(1:on, 0:off) end : %d", power_on);

    return SCI_SUCCESS;
}*/

/******************************************************************************/
// Description: nmi600 identify
// Global resource dependence: 
// Author:
// Note:
/******************************************************************************/
LOCAL uint32_t _nmi600_Identify(uint32_t param)
{
    /*uint32 rtn=SCI_SUCCESS;

    SCI_TRACE_LOW("ATV: _nmi600_Identify");

    if(SCI_TRUE!=NMI_ReadChipID())
    {
        SCI_TRACE_LOW("ATV: the atv is not nmi600");
        rtn=SCI_ERROR;
    }
    else
    {
        SCI_TRACE_LOW("ATV: the atv is nmi600");
    }

    return rtn;*/
        printk("nmi:_nmi600_Identify,called\n");
        return 0;
}
struct sensor_drv_cfg sensor_nmi601 = {
        .sensor_pos = 0,
        .sensor_name = "nmi601",
        .driver_info = &g_nmi600_yuv_info,
};

static int __init sensor_nmi601_init(void)
{
        return dcam_register_sensor_drv(&sensor_nmi601);
}

subsys_initcall(sensor_nmi601_init);
/******************************************************************************/
// Description: nmi600 init
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_Init(uint32 param)
{
    ATV_INFO_T_PTR atv_info_ptr=_nmi600_GetInfo();

    SCI_TraceLow("ATV:_nmi600_Init");

    atv_info_ptr->atv_cur_region=ATV_REGION_MAX;
    atv_info_ptr->atv_cur_chn=0xffffffff;
    atv_info_ptr->cur_volume_level=ATV_VOLUME_MAX;

    atv_nmi600_init();

    return SCI_SUCCESS;
}*/

/******************************************************************************/
// Description: nmi600 Sleep
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_Sleep(uint32 sleep_enter)
{
    SCI_TraceLow("ATV:_nmi600_Sleep");

    if(SCI_TRUE==sleep_enter)
    {

    }
    else if(SCI_FALSE==sleep_enter)
    {

    }

    return SCI_SUCCESS;
}*/

/******************************************************************************/
// Description: nmi600 init scan channel
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_ScanChnInit(uint32 param)
{
    ATV_INFO_T_PTR atv_info_ptr=_nmi600_GetInfo();

    atv_nmi_init_scan_param(atv_info_ptr->atv_cur_region);

    return SCI_SUCCESS;
}*/

/******************************************************************************/
// Description: nmi600 scan channel
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_ScanChn(uint32 param)
{
    uint32 rtn=SCI_FALSE;
    uint32 chn_id=0x00;
    uint32 is_valid=0x00;

    SCI_TraceLow("ATV:_nmi600_ScanChn");

    if(SCI_TRUE!=atv_nmi_scan_chn(&chn_id, &is_valid))
    {
        rtn=((chn_id<<0x10) &0xffff0000) |((is_valid&0x01) <<0x08);
    }
    else
    {
        rtn=SCI_TRUE;
    }

    return rtn;
}*/

/******************************************************************************/
// Description: nmi600 set channel
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_SetChn(uint32 param)
{
    uint32 rtn=SCI_SUCCESS;
    uint32 cur_volume_level=0x00;
    ATV_INFO_T_PTR atv_info_ptr=_nmi600_GetInfo();

    SCI_TraceLow("ATV:_nmi600_SetChn : 0x%x", param);

    if(atv_info_ptr->atv_cur_chn==param)
    {
        return SCI_SUCCESS;
    }

    //turn off audio
    cur_volume_level=atv_info_ptr->cur_volume_level;
    _nmi600_SetVolume(atv_info_ptr->cur_volume_level&0xffffff00);

    atv_info_ptr->atv_cur_chn=param;
    atv_nmi600_fast_set_channel(param);
    SCI_Sleep(200);

    //turn on audio
    _nmi600_SetVolume(cur_volume_level);

    return rtn;
}*/

/******************************************************************************/
// Description: nmi600 get all chn num
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_GetAllChnNum(void)
{
    uint32 chn_num=0x00;

    SCI_TraceLow("ATV:_nmi600_GetAllChnNum");
    
    chn_num=NMIAPP_GetTotalSearchNum();

    return chn_num;
}*/

/******************************************************************************/
// Description: nmi600 set volume
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_SetVolume(uint32 param)
{
    uint32 volume_level=param&0xff;
    ATV_INFO_T_PTR atv_info_ptr=_nmi600_GetInfo();

    SCI_TraceLow("ATV:_nmi600_SetVolume : 0x0%x", param);

    if(atv_info_ptr->cur_volume_level==volume_level)
    {
        return SCI_SUCCESS;
    }

    if(ATV_VOLUME_0==volume_level)
    {
        volume_level=ATV_VOLUME_0;
    }

    if(ATV_VOLUME_9<volume_level)
    {
        volume_level=ATV_VOLUME_9;
    }

    if(volume_level==ATV_VOLUME_0)
    {
        atv_nmi600_set_volume(ATV_VOLUME_0);
        atv_nmi600_set_mute(SCI_TRUE);
    }
    else
    {
        atv_nmi600_set_volume(ATV_VOLUME_6);
        atv_nmi600_set_mute(SCI_FALSE);
    }

    atv_info_ptr->cur_volume_level=volume_level;

    return SCI_SUCCESS;
}*/

/******************************************************************************/
// Description: nmi600 set region
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_SetRegion(uint32 param)
{
    uint32 rtn=SCI_SUCCESS;
    ATV_INFO_T_PTR atv_info_ptr=_nmi600_GetInfo();

    SCI_TraceLow("ATV:_nmi600_SetRegion");

    switch(param)
    {
        case ATV_REGION_CHINA:
        case ATV_REGION_HONGKONG:
        case ATV_REGION_SHENZHEN:
        case ATV_REGION_CAMBODIA:
        case ATV_REGION_CANADA:
        case ATV_REGION_CHILE:
        case ATV_REGION_AFGHANISTAN:
        case ATV_REGION_ARGENTINA:
        case ATV_REGION_BRAZIL:
        case ATV_REGION_INDONESIA:
        case ATV_REGION_JAPAN:
        case ATV_REGION_KOREA:
        case ATV_REGION_LAOS:
        case ATV_REGION_MALAYSIA:
        case ATV_REGION_MEXICO:
        case ATV_REGION_MYANMAR:
        case ATV_REGION_PHILIPPINES:
        case ATV_REGION_SINGAPORE:
        case ATV_REGION_SOUTHAFRICA:
        case ATV_REGION_TAIWAN:
        case ATV_REGION_THAILAND:
        case ATV_REGION_TURKEY:
        case ATV_REGION_UAE:
        case ATV_REGION_UK:
        case ATV_REGION_USA:
        case ATV_REGION_VENEZUELA:
        case ATV_REGION_VIETNAM:
        case ATV_REGION_WESTERNEUROPE:
        case ATV_REGION_SPAIN:
        case ATV_REGION_PORTUGAL:
        case ATV_REGION_PAKISTAN:
        case ATV_REGION_INDIA:
        case ATV_REGION_AUSTRALIA:
        case ATV_REGION_PARAGURY:
        case ATV_REGION_URUGUAY:
        case ATV_REGION_BULGARIA:
        case ATV_REGION_CONGO:
        case ATV_REGION_EGYPT:
        case ATV_REGION_GERMANY:
        case ATV_REGION_IRAQ:
        case ATV_REGION_IRAN:
        case ATV_REGION_HUNGRY:
        case ATV_REGION_LIBYA:
        case ATV_REGION_LEBANON:
        case ATV_REGION_POLAND:
        case ATV_REGION_ROMANIA:
        case ATV_REGION_SANDIARABIA:
        case ATV_REGION_TUNISIA:
        case ATV_REGION_RUSSIA:
        {
            atv_info_ptr->atv_cur_region=param;
            _nmi600_InitChannel();
            break;
        }

        default :
        {
            atv_info_ptr->atv_cur_region=ATV_REGION_MAX;
            rtn=SCI_ERROR;
            break;
        }
    }

    return rtn;
}*/

/******************************************************************************/
// Description: nmi600 get rssi
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_GetRssi(void)
{
    uint16 rssi=0x00;
    uint32 rtn_rssi;

    SCI_TraceLow("ATV:_nmi600_GetRssi");

    atv_nmi600_get_rssi(&rssi);

    switch(rssi)
    {
        case 1:
        case 2:
        {
            rtn_rssi=ATV_RSSI_0;
            break;
        }
        case 3:
        {
            rtn_rssi=ATV_RSSI_1;
            break;
        }
        case 4:
        {
            rtn_rssi=ATV_RSSI_2;
            break;
        }
        case 5:
        case 6:
        {
            rtn_rssi=ATV_RSSI_3;
            break;
        }
        case 7:
        case 8:
        {
            rtn_rssi=ATV_RSSI_4;
            break;
        }
        default:
            rtn_rssi=ATV_RSSI_4;
            break;
    }

    return rtn_rssi;
}*/

/******************************************************************************/
// Description: nmi600 is ntsc mode
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*BOOLEAN _nmi600_IsNTSCMode(void)
{

    BOOLEAN Is_NTSCMode=SCI_TRUE;
    ATV_INFO_T_PTR atv_info_ptr=_nmi600_GetInfo();

    SCI_TraceLow("ATV:_nmi600_IsNTSCMode");

    switch(atv_info_ptr->atv_cur_region)
    {
        case ATV_REGION_BRAZIL://video size is same as NTSC
        case ATV_REGION_JAPAN:
        case ATV_REGION_USA:
        case ATV_REGION_CANADA:
        case ATV_REGION_KOREA:
        case ATV_REGION_TAIWAN:
        case ATV_REGION_MEXICO:
        case ATV_REGION_CHILE:
        case ATV_REGION_VENEZUELA:
        case ATV_REGION_PHILIPPINES:
            Is_NTSCMode=SCI_TRUE;
            break;

        default:
            Is_NTSCMode=SCI_FALSE;
            break;
    }

    return Is_NTSCMode;
}*/

/******************************************************************************/
// Description: nmi600 get emc param
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_GetEmcParam(void)
{
    uint32 emc_param=0x00;

    emc_param=(s_emc_param[0]<<0x10)&0xffff0000;
    emc_param|=s_emc_param[1]&0xffff;

    return emc_param;
}*/

/******************************************************************************/
// Description: nmi600 close
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 _nmi600_Close(void)
{
    uint32 rtn=SCI_SUCCESS;
    ATV_INFO_T_PTR atv_info_ptr=_nmi600_GetInfo();

    SCI_TraceLow("ATV:_nmi600_Close");

    atv_nmi600_power_off();

    atv_info_ptr->atv_cur_region=ATV_REGION_MAX;
    atv_info_ptr->atv_cur_chn=0xffffffff;
    atv_info_ptr->cur_volume_level=ATV_VOLUME_MAX;

    return rtn;
}*/

/******************************************************************************/
// Description: nmi600 iic write
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 nmi600_IICWriteReg(uint16 addr, uint16 data)
{
    //TLGI2C_WriteReg(0x00, (uint32)addr, data);

    SCI_TRACE_LOW("ATV: nmi600_WriteReg reg/value(0x%x,0x%x) !!", addr, data);

    return SCI_SUCCESS;
}*/

/******************************************************************************/
// Description: nmi600 iic read
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32 nmi600_IICReadReg(uint16 addr, uint16 *data)
{
    //TLGI2C_ReadReg(0x00, (uint32)addr, data);

    SCI_TRACE_LOW("ATV: nmi600_ReadReg reg/value(0x%x,0x%x) !!", addr, *data);

    return SCI_SUCCESS;
}*/

/******************************************************************************/
// Description: nmi600 IOCTL
// Global resource dependence:
// Author:
// Note:
/******************************************************************************/
/*LOCAL uint32_t _nmi600_IOCTL(uint32_t ctl_param)
{
    uint32_t rtn=0;
    uint32_t cmd=(ctl_param>>0x10) &0xffff;
    uint32_t param=ctl_param&0xffff;

    printk("nmi:ATV:_nmi600_IOCTL cmd:0x%x, param:0x%x",cmd,param);

    switch(cmd)
    {
        case ATV_CMD_CHIP_INIT:
        {
            rtn=_nmi600_Init(param);
            break;
        }
        case ATV_CMD_CHIP_SLEEP:
        {
            rtn=_nmi600_Sleep(param);
            break;
        }
        case ATV_CMD_SCAN_INIT:
        {
            rtn=_nmi600_ScanChnInit(param);
            break;
        }
        case ATV_CMD_SCAN_CHN:
        {
            rtn=_nmi600_ScanChn(param);
            break;
        }
        case ATV_CMD_SET_CHN:
        {
            rtn=_nmi600_SetChn(param);
            break;
        }
        case ATV_CMD_SET_VOLUME:
        {
            rtn=_nmi600_SetVolume(param);
            break;
        }
        case ATV_CMD_SET_REGION:
        {
            rtn=_nmi600_SetRegion(param);
            break;
        }
        case ATV_CMD_GET_RSSI:
        {
            rtn=_nmi600_GetRssi();
            break;
        }
        case ATV_CMD_GET_ALL_CHN_NUM:
        {
            rtn=_nmi600_GetAllChnNum();
            break;
        }
        case ATV_CMD_GET_IS_NTSC:
        {
            rtn=_nmi600_IsNTSCMode();
            break;
        }
        case ATV_CMD_GET_EMC:
        {
            rtn=_nmi600_GetEmcParam();
            break;
        }
        case ATV_CMD_GET_INFO:
        {
            rtn=(uint32)_nmi600_GetInfo();
            break;
        }        
        case ATV_CMD_CLOSE:
        {
            rtn=_nmi600_Close();
            break;
        }
        default :
            break;
    }

    return rtn;	
	
}*/

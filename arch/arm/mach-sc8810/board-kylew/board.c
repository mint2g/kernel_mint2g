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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/input.h>

#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/time.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c/pixcir_i2c_ts.h>
#include <linux/i2c/al3006_pls.h>
#include <linux/i2c/lis3dh.h>
#include <linux/akm8975.h>
#include <linux/spi/spi.h>
#include <mach/globalregs.h>
#include <mach/board.h>
#include <sound/audio_pa.h>
#include "../devices.h"
#include <linux/ktd253b_bl.h>

#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <mach/gpio.h>
#include <mach/serial_sprd.h>
#include <mach/sys_debug.h>
#if defined(CONFIG_SS6000_CHARGER)
#include <linux/ss6000_charger.h>
#endif
#if defined(CONFIG_BQ24157_CHARGER)
#include <mach/bq24157_charger.h>
#endif
#if defined(CONFIG_STC3115_FUELGAUGE)
#include <linux/stc3115_battery.h>
#endif

#include <gps/gpsctl.h>
#include <mach/pinmap.h>
#include <mach/adc.h>
#include <linux/headset.h>

#if defined(CONFIG_SPA)
#include <linux/power/spa.h>
#endif

#if defined(CONFIG_SENSORS_GP2AP002)
#include <linux/gp2ap002_dev.h>
#endif

#if defined  (CONFIG_SENSORS_BMA2X2) || defined (CONFIG_SENSORS_BMC150)
#include <linux/bst_sensor_common.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_MMS134S_TS)
#include <linux/input/mms134s_ts.h>
#endif
extern void __init sc8810_reserve(void);
extern void __init sc8810_map_io(void);
extern void __init sc8810_init_irq(void);
extern void __init sc8810_timer_init(void);
extern void __init regulator_add_devices(void);
extern void __init sc8810_clock_init(void);
#ifdef CONFIG_ANDROID_RAM_CONSOLE
extern int __init sprd_ramconsole_init(void);
#endif
extern int sci_adc_get_value(unsigned chan, int scale);
static struct platform_device rfkill_device;
static struct platform_device brcm_bluesleep_device;
static struct platform_device kb_backlight_device;

static struct platform_gpsctl_data pdata_gpsctl = {
	.reset_pin = GPIO_GPS_RESET,
	.onoff_pin = GPIO_GPS_ONOFF,
	.clk_type  = "clk_aux0",
/*	.pwr_type  = "pwr_ldo",*/
};

static struct platform_device  gpsctl_dev = {
	.name               = "gpsctl",
	.dev.platform_data  = &pdata_gpsctl,
};

static unsigned int modem_detect_gpio = GPIO_MODEM_DETECT;
#if defined  (CONFIG_SS_VIBRATOR)
struct platform_device vibrator_device = {
	.name = "vibrator",
	.id = 0,
	.dev = {
		.platform_data = "VDDWIF0",
		},
};
#endif
#if defined(CONFIG_SS6000_CHARGER)
static struct ss6000_platform_data ss6000_info = {
	.en_set = CHARGERIC_CHG_EN_GPIO,
	.pgb = CHARGERIC_TA_INT_GPIO,
	.chgsb = CHARGERIC_CHG_INT_GPIO,
};

static struct platform_device ss6000_charger = {
	.name = "ss6000_charger",
	.id = -1,
	.dev = {
		.platform_data = &ss6000_info,
		},
};
#endif

#if defined(CONFIG_STC3115_FUELGAUGE)

#ifdef CONFIG_CHARGER_RT9532
extern int rt9532_get_charger_online(void);
#endif

static int null_fn(void)
{
        return 0;                // for discharging status
}

static int Temperature_fn(void)
{
	return (25);
}

static struct stc311x_platform_data stc3115_data = {
	.battery_online = NULL,
#ifdef CONFIG_CHARGER_RT9532
	.charger_online = rt9532_get_charger_online, 	// used in stc311x_get_status()
#else
	.charger_online = NULL, 	// used in stc311x_get_status()
#endif
	.charger_enable = NULL,		// used in stc311x_get_status()
	.power_supply_register = NULL,
	.power_supply_unregister = NULL,

	.Vmode = 0,		/*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
	.Alm_SOC = 15,		/* SOC alm level % */
	.Alm_Vbat = 3400,	/* Vbat alm level mV */
	.CC_cnf = 244,      /* nominal CC_cnf, coming from battery characterisation*/
  	.VM_cnf = 290,      /* nominal VM cnf , coming from battery characterisation*/
	.Cnom = 1200,       /* nominal capacity in mAh, coming from battery characterisation*/
	.Rsense = 10,		/* sense resistor mOhms */
	.RelaxCurrent = 120, /* current for relaxation in mA (< C/20) */
	.Adaptive = 1,		/* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

	.CapDerating[6] = 277,   /* capacity derating in 0.1%, for temp = -20 C */
  	.CapDerating[5] = 82,   /* capacity derating in 0.1%, for temp = -10 C */
	.CapDerating[4] = 23,   /* capacity derating in 0.1%, for temp = 0 C */
	.CapDerating[3] = 19,   /* capacity derating in 0.1%, for temp = 10 C */
	.CapDerating[2] = 0,   /* capacity derating in 0.1%, for temp = 25 C */
	.CapDerating[1] = -2,   /* capacity derating in 0.1%, for temp = 40 C */
	.CapDerating[0] = -2,   /* capacity derating in 0.1%, for temp = 60 C */

	.OCVOffset[15] = -22,    /* OCV curve adjustment */
	.OCVOffset[14] = -9,   /* OCV curve adjustment */
	.OCVOffset[13] = -15,    /* OCV curve adjustment */
	.OCVOffset[12] = -2,    /* OCV curve adjustment */
	.OCVOffset[11] = 0,	/* OCV curve adjustment */
	.OCVOffset[10] = -2,    /* OCV curve adjustment */
	.OCVOffset[9] = -26,     /* OCV curve adjustment */
	.OCVOffset[8] = -6,      /* OCV curve adjustment */
	.OCVOffset[7] = -7,      /* OCV curve adjustment */
	.OCVOffset[6] = -14,    /* OCV curve adjustment */
	.OCVOffset[5] = -23,    /* OCV curve adjustment */
	.OCVOffset[4] = -46,     /* OCV curve adjustment */
	.OCVOffset[3] = -27,    /* OCV curve adjustment */
	.OCVOffset[2] = -34,     /* OCV curve adjustment */
	.OCVOffset[1] = -125,    /* OCV curve adjustment */
	.OCVOffset[0] = -68,     /* OCV curve adjustment */
		
	.OCVOffset2[15] = -58,    /* OCV curve adjustment */
	.OCVOffset2[14] = -37,   /* OCV curve adjustment */
	.OCVOffset2[13] = -21,    /* OCV curve adjustment */
	.OCVOffset2[12] = -14,    /* OCV curve adjustment */
	.OCVOffset2[11] = -6,    /* OCV curve adjustment */
	.OCVOffset2[10] = -16,    /* OCV curve adjustment */
	.OCVOffset2[9] = -6,     /* OCV curve adjustment */
	.OCVOffset2[8] = 4,      /* OCV curve adjustment */
	.OCVOffset2[7] = 9,      /* OCV curve adjustment */
	.OCVOffset2[6] = 11,    /* OCV curve adjustment */
	.OCVOffset2[5] = 24,    /* OCV curve adjustment */
	.OCVOffset2[4] = 7,     /* OCV curve adjustment */
	.OCVOffset2[3] = 28,    /* OCV curve adjustment */
	.OCVOffset2[2] = 89,     /* OCV curve adjustment */
	.OCVOffset2[1] = 94,    /* OCV curve adjustment */
	.OCVOffset2[0] = 0,     /* OCV curve adjustment */

	/*if the application temperature data is preferred than the STC3115 temperature */
	.ExternalTemperature = Temperature_fn,	/*External temperature fonction, return C */
	.ForceExternalTemperature = 0,	/* 1=External temperature, 0=STC3115 temperature */

};
#endif

#if defined(CONFIG_SEC_CHARGING_FEATURE)
#include <linux/wakelock.h>
#include <linux/spa_power.h>
#include <linux/spa_agent.h>

/* Samsung charging feature
 +++ for board files, it may contain changeable values */
static struct spa_temp_tb batt_temp_tb[] = {
	{869, -300},			/* -30 */
	{769, -200},			/* -20 */
	{643, -100},            /* -10 */
	{568, -50},				/* -5  */
	{509,   0},             /* 0   */
	{382,  100},            /* 10  */
	{275,  200},            /* 20  */
	{231,  250},            /* 25  */
	{196,  300},            /* 30  */
	{138,  400},            /* 40  */
	{95 ,  500},            /* 50  */
	{68 ,  600},            /* 60  */
	{54 ,  650},            /* 65  */
	{46 ,  700},			/* 70  */
	{34 ,  800},			/* 80  */
};

struct spa_power_data spa_power_pdata = {
	.charger_name = "spa_agent_chrg",
	.batt_cell_name = "SDI_SDI",
	.eoc_current = 100, //not used
	.recharge_voltage = 4150,
	.charging_cur_usb = 400, //not used
	.charging_cur_wall = 600, //not used
	.suspend_temp_hot = 600,
	.recovery_temp_hot = 400,
	.suspend_temp_cold = -50,
	.recovery_temp_cold = 0,
	.charge_timer_limit = CHARGE_TIMER_6HOUR,
	.batt_temp_tb = &batt_temp_tb[0],
	.batt_temp_tb_len = ARRAY_SIZE(batt_temp_tb),
};
EXPORT_SYMBOL(spa_power_pdata);

static struct platform_device spa_power_device = {
	.name = "spa_power",
	.id = -1,
	.dev.platform_data = &spa_power_pdata,
};

static struct platform_device spa_agent_device={
	.name = "spa_agent",
	.id=-1,
};

static int spa_power_init(void)
{
	platform_device_register(&spa_agent_device);
	platform_device_register(&spa_power_device);
	return 0;
}
#endif

/* sprd MODEM interface platform data */
//#if defined(CONFIG_MODEM_INTF)
#include <mach/modem_interface.h>
struct modem_intf_platform_data modem_interface = {
        .dev_type               = MODEM_DEV_SDIO,
        .modem_dev_parameter    = NULL,
	.modem_power_gpio       =  GPIO_MODEM_POWER,
        .modem_boot_gpio        = GPIO_MODEM_BOOT,
        .modem_crash_gpio       = GPIO_MODEM_CRASH,
};

static struct platform_device modem_interface_device = {
       .name   = "modem_interface",
       .id     = -1,
       .dev    = {
               .platform_data  = &modem_interface,
       },
};

static struct resource ipc_sdio_resources[] = {
	[0] = {
		.start = SPRD_SPI0_PHYS,
		.end = SPRD_SPI0_PHYS + SZ_4K - 1,

	    .flags = IORESOURCE_MEM,
	       },
};

static struct platform_device ipc_sdio_device = {
	.name = "ipc_sdio",
	.id = 0,
	.num_resources = ARRAY_SIZE(ipc_sdio_resources),
	.resource = ipc_sdio_resources,
};

#if defined(CONFIG_SPA)
static struct spa_platform_data spa_info = {
	.use_fuelgauge = 0,
	.battery_capacity = 1200,
	.VF_low = 100,
	.VF_high = 600,
};

static struct platform_device Sec_BattMonitor = {
	.name = "Sec_BattMonitor",
	.id = -1,
	.dev = {
		.platform_data = &spa_info,
		},
};
#endif
static struct platform_device *devices[] __initdata = {
	&sprd_serial_device0,
	&sprd_serial_device1,
	&sprd_serial_device2,
	&sprd_device_rtc,
	&sprd_nand_device,
	&sprd_lcd_device0,
	&sprd_backlight_device,
//#if defined(CONFIG_SPA)
//	&Sec_BattMonitor,
//#else
//	&sprd_battery_device,
//#endif
	&sprd_i2c_device0,
	&sprd_i2c_device1,
	&sprd_i2c_device2,
	&sprd_i2c_device3,
	&sprd_spi0_device,
	&sprd_spi1_device,
	&sprd_keypad_device,
#if (defined(CONFIG_SND_SPRD_SOC_SC881X) || defined(CONFIG_SND_SPRD_SOC_KYLEW))
	&sprd_audio_platform_vbc_pcm_device,
	&sprd_audio_cpu_dai_vaudio_device,
	&sprd_audio_cpu_dai_vbc_device,
	&sprd_audio_codec_dolphin_device,
#else
	&sprd_audio_soc_device,
	&sprd_audio_soc_vbc_device,
	&sprd_audio_vbc_device,
#endif
	&sprd_battery_device,
#ifdef CONFIG_ANDROID_PMEM
	&sprd_pmem_device,
	&sprd_pmem_adsp_device,
#endif
#ifdef CONFIG_ION
	&sprd_ion_dev,
#endif
	&sprd_sdio1_device,
	&sprd_sdio0_device,
	&sprd_vsp_device,
	&sprd_dcam_device,
	&sprd_scale_device,
	&sprd_rotation_device,
	&rfkill_device,
	&brcm_bluesleep_device,
	&kb_backlight_device,
	&gpsctl_dev,
#if defined  (CONFIG_SS_VIBRATOR)
	&vibrator_device,
#endif

#if defined(CONFIG_SS6000_CHARGER)
	&ss6000_charger,
#endif
        &modem_interface_device,
        &ipc_sdio_device,
};
/* BT suspend/resume */
static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= GPIO_BT2AP_WAKE,
		.end	= GPIO_BT2AP_WAKE,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= GPIO_AP2BT_WAKE,
		.end	= GPIO_AP2BT_WAKE,
		.flags	= IORESOURCE_IO,
	},
};

static struct platform_device brcm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

/* RFKILL */
static struct resource rfkill_resources[] = {
	{
		.name   = "bt_power",
		.start  = GPIO_BT_POWER,
		.end    = GPIO_BT_POWER,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "bt_reset",
		.start  = GPIO_BT_RESET,
		.end    = GPIO_BT_RESET,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device rfkill_device = {
     .name = "rfkill",
     .id = -1,
     .num_resources  = ARRAY_SIZE(rfkill_resources),
     .resource   = rfkill_resources,
};


/* keypad backlight */
static struct platform_device kb_backlight_device = {
	.name           = "keyboard-backlight",
	.id             =  -1,
};

static struct platform_ktd253b_backlight_data cat4253b_data = {
	.max_brightness = 255,
	.dft_brightness = 50,
	.ctrl_pin = GPIO_BK,
};


static struct sys_timer sc8810_timer = {
	.init = sc8810_timer_init,
};

static int calibration_mode = false;
static int __init calibration_start(char *str)
{
	if(str)
		pr_info("modem calibartion:%s\n", str);
	calibration_mode = true;
	return 1;
}
__setup("calibration=", calibration_start);

int in_calibration(void)
{
	return (calibration_mode == true);
}

EXPORT_SYMBOL(in_calibration);

static void __init sprd_add_otg_device(void)
{
	/*
	 * if in calibrtaion mode, we do nothing, modem will handle everything
	 */
	if (calibration_mode)
		return;
	platform_device_register(&sprd_otg_device);
}

static struct i2c_gpio_platform_data pdata_gpio_i2c_p4 = {
	.sda_pin = GPIO_I2C_SDA,	/*  SIMCLK3 */
	.sda_is_open_drain = 0,
	.scl_pin = GPIO_I2C_SCL,	/*  SIMCLK2 */
	.scl_is_open_drain = 0,
	.udelay = 2,		/* ~100 kHz */
	.timeout = 20,
};

static struct platform_device device_data_gpio_i2c_p4 = {
	.name = "i2c-gpio",
	.id = 4,
	.dev.platform_data = &pdata_gpio_i2c_p4,
};

#if defined  (CONFIG_SENSORS_GP2AP002)
#define PROXI_INT_GPIO_PIN      (94)
#define PROXI_POWER_GPIO_PIN      (92)
static struct gp2ap002_platform_data gp2ap002_platform_data = {
	.irq_gpio = PROXI_INT_GPIO_PIN,
	//.irq = gpio_to_irq(PROXI_INT_GPIO_PIN),
	.power = PROXI_POWER_GPIO_PIN,
};
#endif


#if defined  (CONFIG_SENSORS_BMA2X2)
static struct bosch_sensor_specific bss_bma2x2 = {
	.name = "bma2x2" ,
	.place = 6,
};
#elif defined  (CONFIG_SENSORS_BMC150)
static struct bosch_sensor_specific bss_bma2x2 = {
	.name = "bma2x2" ,
	.place = 4,
};
static struct bosch_sensor_specific bss_bmm050 = {
	.name = "bmm050" ,
        .place = 4,
};
#endif

static struct serial_data plat_data0 = {
	.wakeup_type = BT_NO_WAKE_UP,
	.clk = 48000000,
};
static struct serial_data plat_data1 = {
	.wakeup_type = BT_NO_WAKE_UP,
	.clk = 26000000,
};
static struct serial_data plat_data2 = {
	.wakeup_type = BT_NO_WAKE_UP,
	.clk = 26000000,
};

static struct pixcir_ts_platform_data pixcir_ts_info = {
	.irq_gpio_number	= GPIO_TOUCH_IRQ,
	.reset_gpio_number	= GPIO_TOUCH_RESET,
};

static struct al3006_pls_platform_data al3006_pls_info = {
	.irq_gpio_number	= GPIO_PLSENSOR_IRQ,
};

static struct i2c_board_info i2c2_boardinfo[] = {
	{
		I2C_BOARD_INFO(PIXICR_DEVICE_NAME, 0x5C),
		.platform_data = &pixcir_ts_info,
	}
};

static struct lis3dh_acc_platform_data lis3dh_plat_data = {
	.poll_interval = 100,
	.min_interval = 100,
	.g_range = LIS3DH_ACC_G_2G,
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 1
};

struct akm8975_platform_data akm8975_platform_d = {
	.mag_low_x = -20480,
	.mag_high_x = 20479,
	.mag_low_y = -20480,
	.mag_high_y = 20479,
	.mag_low_z = -20480,
	.mag_high_z = 20479,
};

static struct i2c_board_info __initdata i2c3_boardinfo[] = {
#if defined(CONFIG_MUSB_FSA880)
	{
	 I2C_BOARD_INFO("fsa880", 0x25),
	 },
#endif
#if defined(CONFIG_BQ27425_FUELGAUGE)
	{
	 I2C_BOARD_INFO("bq27425_firmware", 0xB),
	 },
	{
	 I2C_BOARD_INFO("bq27425_fuelgauge", 0x55),
	 .irq = GPIO_BQ27425_LOW_BAT,
	 },
#endif
#if defined(CONFIG_BQ24157_CHARGER)
	{
	 I2C_BOARD_INFO("bq24157_6A", 0x6A),
	 .platform_data = &bq24157_charger_info,
	 },
	{
	 I2C_BOARD_INFO("bq24157_6B", 0x6B),
	 .platform_data = &bq24157_charger_info,
	 },
#endif
#if defined(CONFIG_STC3115_FUELGAUGE)
	{
	 I2C_BOARD_INFO("stc3115", 0x70),
	 .platform_data = &stc3115_data,
	 },
#endif
};

static struct i2c_board_info i2c1_boardinfo[] = {
	{I2C_BOARD_INFO("sensor_main",0x3C),},
	{I2C_BOARD_INFO("sensor_sub",0x21),},
};

#if defined  (CONFIG_TOUCHSCREEN_TMA140)
static struct i2c_board_info __initdata i2c_boardinfo_p4[] = {
	{I2C_BOARD_INFO("synaptics-rmi-ts", 0x20),},
};
#elif defined(CONFIG_TOUCHSCREEN_SILABS_F760)
static struct i2c_board_info __initdata i2c_boardinfo_p4[] = {
       {
               I2C_BOARD_INFO("silabs-f760", 0x20),
			   .irq = 17,
       },
};
#elif defined(CONFIG_TOUCHSCREEN_MMS134S_TS)
static struct i2c_board_info __initdata i2c_boardinfo_p4[] = {
	{
	 I2C_BOARD_INFO("mms_ts", 0x48),
	 .irq = GPIO_I2C_INT,
	 },
};
#else
static struct i2c_board_info __initdata i2c_boardinfo_p4[] = {
	{I2C_BOARD_INFO("zinitix_isp", 0x50),},
	{I2C_BOARD_INFO("Zinitix_tsp", 0x20),},
};
#endif

static struct i2c_board_info i2c0_boardinfo[] = {
#if defined  (CONFIG_SENSORS_K3DH)
	{
	 I2C_BOARD_INFO("k3dh", 0x19),
	 },
#endif

#if defined (CONFIG_SENSORS_BMA2X2)
	{
		I2C_BOARD_INFO("bma2x2", 0x18),
		.platform_data = &bss_bma2x2
	 },
#elif defined(CONFIG_SENSORS_BMC150)
	{
		I2C_BOARD_INFO("bma2x2", 0x10),
		.platform_data = &bss_bma2x2
	 },
	{
		I2C_BOARD_INFO("bmm050", 0x12),
		.platform_data = &bss_bmm050,
	 },
#endif

#if defined  (CONFIG_SENSORS_HSCD)
	{
		I2C_BOARD_INFO("hscd_i2c", 0x0c),
	},
#endif
    
#if defined  (CONFIG_SENSORS_GP2AP002)
	{
		I2C_BOARD_INFO("gp2ap002", 0x44),
		.platform_data = &gp2ap002_platform_data,
	 },
#endif

};

/* config I2C2 SDA/SCL to SIM2 pads */
static void sprd8810_i2c2sel_config(void)
{
	sprd_greg_set_bits(REG_TYPE_GLOBAL, PINCTRL_I2C2_SEL, GR_PIN_CTL);
}

static int sc8810_add_i2c_devices(void)
{
	sprd8810_i2c2sel_config();
	platform_device_register(&device_data_gpio_i2c_p4);
	i2c_register_board_info(4, i2c_boardinfo_p4, ARRAY_SIZE(i2c_boardinfo_p4));
	printk("alex:sc8810_add_i2c_device:%d,%d \n",ARRAY_SIZE(i2c2_boardinfo),ARRAY_SIZE(i2c1_boardinfo));
	//i2c_register_board_info(2, i2c2_boardinfo, ARRAY_SIZE(i2c2_boardinfo));
	i2c_register_board_info(0, i2c0_boardinfo, ARRAY_SIZE(i2c0_boardinfo));
	i2c_register_board_info(1, i2c1_boardinfo, ARRAY_SIZE(i2c1_boardinfo));
	i2c_register_board_info(3, i2c3_boardinfo, ARRAY_SIZE(i2c3_boardinfo));
	return 0;
}

struct platform_device audio_pa_amplifier_device = {
	.name = "speaker-pa",
	.id = -1,
};

static int audio_pa_amplifier_speaker(int cmd, void *data)
{
	int ret = 0;
	if (cmd < 0) {
		/* get speaker amplifier status : enabled or disabled */
		ret = 0;
	} else {
		/* set speaker amplifier */
	}
	return ret;
}

static int audio_pa_amplifier_headset_init(void)
{
	if (gpio_request(HEADSET_PA_CTL_GPIO, "headset outside pa")) {
		pr_err("failed to alloc gpio %d\n", HEADSET_PA_CTL_GPIO);
		return -1;
	}
	gpio_direction_output(HEADSET_PA_CTL_GPIO, 0);
	return 0;
}

static int audio_pa_amplifier_headset(u32 cmd, void *data)
{
	gpio_direction_output(HEADSET_PA_CTL_GPIO, cmd);
	return 0;
}

static _audio_pa_control audio_pa_control = {
	.speaker = {
		.init = NULL,
		.control = NULL,
	},
	.earpiece = {
		.init = NULL,
		.control = NULL,
	},
	.headset = {
		    .init = audio_pa_amplifier_headset_init,
		    .control = audio_pa_amplifier_headset,
		    },
};
static unsigned int headset_get_button_code_board_method(int v)
{
	static struct headset_adc_range {
		int min;
		int max;
		int code;
	} adc_range[] = {
		{
		0x0000, 0x0050, KEY_MEDIA}, {
		0x0050, 0x0100, KEY_VOLUMEUP}, {
	0x0100, 0x0200, KEY_VOLUMEDOWN},};

	int adc_value = sci_adc_get_value(ADC_CHANNEL_TEMP, true);
	pr_info("*********SYP  adc value : %x ******* \n", adc_value);
	{
		adc_value = sci_adc_get_value(1, false);
		pr_info("********* adc value1 : %x ******* \n", adc_value);
	}
	int i;
	for (i = 0; i < ARRY_SIZE(adc_range); i++)
		if (adc_value >= adc_range[i].min
		    && adc_value < adc_range[i].max)
			return adc_range[i].code;
	return KEY_RESERVED;
}

static unsigned int headset_map_code2push_code_board_method(unsigned int code,
							    int push_type)
{
	return code;
}

struct platform_device headset_get_button_code_board_method_device = {
	.name = "headset-button",
	.id = -1,
};

struct _headset_button headset_button = {
	.cap = {
		{EV_KEY, KEY_MEDIA},
		{EV_KEY, KEY_VOLUMEUP},
		{EV_KEY, KEY_VOLUMEDOWN},
		},
	.headset_get_button_code_board_method =
	    headset_get_button_code_board_method,
	.headset_map_code2push_code_board_method =
	    headset_map_code2push_code_board_method,
};
static int spi_cs_gpio_map[][2] = {
	{SPI0_CMMB_CS_GPIO,  0},
	{SPI1_WIFI_CS_GPIO,  0},
};

static struct spi_board_info spi_boardinfo[] = {
	{
		.modalias = "cmmb-dev",
		.bus_num = 0,
		.chip_select = 0,
		.max_speed_hz = 8 * 1000 * 1000,
		.mode = SPI_CPOL | SPI_CPHA,
	},
	{
		.modalias = "wlan_spi",
		.bus_num = 1,
		.chip_select = 0,
		.max_speed_hz = 48 * 1000 * 1000,
		.mode = SPI_CPOL | SPI_CPHA,
	},
};

static void sprd_spi_init(void)
{
	int busnum, cs, gpio;
	int i;

	struct spi_board_info *info = spi_boardinfo;

	for (i = 0; i < ARRAY_SIZE(spi_boardinfo); i++) {
		busnum = info[i].bus_num;
		cs = info[i].chip_select;
		gpio   = spi_cs_gpio_map[busnum][cs];

		info[i].controller_data = (void *)gpio;
	}

        spi_register_board_info(info, ARRAY_SIZE(spi_boardinfo));
}


static int sc8810_add_misc_devices(void)
{
	if (audio_pa_control.speaker.control || audio_pa_control.earpiece.control || \
		audio_pa_control.headset.control) {
		platform_set_drvdata(&audio_pa_amplifier_device, &audio_pa_control);
		if (platform_device_register(&audio_pa_amplifier_device))
			pr_err("faile to install audio_pa_amplifier_device\n");
	}
	platform_set_drvdata(&headset_get_button_code_board_method_device,
			     &headset_button);
	if (platform_device_register
	    (&headset_get_button_code_board_method_device))
		pr_err
		    ("faile to install headset_get_button_code_board_method_device\n");
	return 0;
}

static void __init sc8810_init_machine(void)
{
	regulator_add_devices();
	sprd_add_otg_device();

	platform_device_add_data(&sprd_sdio0_device, &modem_detect_gpio, sizeof(modem_detect_gpio));
	platform_device_add_data(&sprd_backlight_device, &cat4253b_data, sizeof(cat4253b_data));
	platform_device_add_data(&sprd_serial_device0,(const void*)&plat_data0,sizeof(plat_data0));
	platform_device_add_data(&sprd_serial_device1,(const void*)&plat_data1,sizeof(plat_data1));
	platform_device_add_data(&sprd_serial_device2,(const void*)&plat_data2,sizeof(plat_data2));
	platform_add_devices(devices, ARRAY_SIZE(devices));

#if defined(CONFIG_SEC_CHARGING_FEATURE)	
	spa_power_init();
#endif

	sc8810_add_i2c_devices();
	sc8810_add_misc_devices();
#if defined(CONFIG_TOUCHSCREEN_MMS134S_TS)
	mms134s_ts_init();
#endif
	sprd_spi_init();
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	sprd_ramconsole_init();
#endif
	sys_debug_init();
}

static void __init sc8810_fixup(struct machine_desc *desc, struct tag *tag,
		char **cmdline, struct meminfo *mi)
{
	struct tag *t = tag;

#ifdef CONFIG_NKERNEL
	/* manipulate cmdline if not in calibration mode, for mfserial */
	for (; t->hdr.size; t = (struct tag *)((__u32 *) (t) + (t)->hdr.size)) {
		if (t->hdr.tag == ATAG_CMDLINE) {
			char *p, *c;
			c = (char *)t->u.cmdline.cmdline;
			if (strstr(c, "calibration=") == NULL) {
				p = strstr(c, "console=");
				/* break it, if exists */
				if (p)
					*p = 'O';
				/* add our kernel parameters */
				strcat(c, "console=ttyS1,115200n8 loglevel=8");
			}
			break;
		}
	}
#endif
}

static void __init sc8810_init_early(void)
{
	/* earlier init request than irq and timer */
	sc8810_clock_init();

}

MACHINE_START(SC8810OPENPHONE, "SP8810")
	.reserve	= sc8810_reserve,
	.map_io		= sc8810_map_io,
	.init_irq	= sc8810_init_irq,
	.timer		= &sc8810_timer,
	.init_machine	= sc8810_init_machine,
	.fixup		= sc8810_fixup,
	.init_early	= sc8810_init_early,
MACHINE_END

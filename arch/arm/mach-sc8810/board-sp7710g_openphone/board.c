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

#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/time.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include <linux/i2c.h>
#include <linux/i2c/pixcir_i2c_ts.h>
#include <linux/i2c/al3006_pls.h>
#include <linux/i2c/lis3dh.h>
#include <linux/akm8975.h>
#include <linux/spi/spi.h>
#include <mach/globalregs.h>
#include <mach/board.h>
#include <sound/audio_pa.h>
#include "../devices.h"
#include <mach/serial_sprd.h>
#include <gps/gpsctl.h>

extern void __init sc8810_reserve(void);
extern void __init sc8810_map_io(void);
extern void __init sc8810_init_irq(void);
extern void __init sc8810_timer_init(void);
extern void __init regulator_add_devices(void);
extern void __init sc8810_clock_init(void);
#ifdef CONFIG_ANDROID_RAM_CONSOLE
extern int __init sprd_ramconsole_init(void);
#endif

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
#include <mach/modem_interface.h>
struct modem_intf_platform_data modem_interface = {
        .dev_type               = MODEM_DEV_SDIO,
        .modem_dev_parameter    = NULL,
        .modem_power_gpio       = GPIO_INVALID,
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

static struct platform_device *devices[] __initdata = {
	&sprd_serial_device0,
	&sprd_serial_device1,
	&sprd_serial_device2,
	&sprd_serial_device3,
	&sprd_device_rtc,
	&sprd_nand_device,
	&sprd_lcd_device1,
	&sprd_backlight_device,
	&sprd_i2c_device0,
	&sprd_i2c_device1,
	&sprd_i2c_device2,
	&sprd_spi0_device,
	&sprd_spi1_device,
	&sprd_keypad_device,
#ifdef CONFIG_SND_SPRD_SOC_SC881X
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
	&sprd_emmc0_device,
	&sprd_sdio0_device,
	&sprd_sdio1_device,
	&sprd_sdio2_device,
	&sprd_vsp_device,
	&sprd_dcam_device,
	&sprd_scale_device,
	&sprd_rotation_device,
	&kb_backlight_device,
	&gpsctl_dev,

        &modem_interface_device,
        &ipc_sdio_device,
};

/* keypad backlight */
static struct platform_device kb_backlight_device = {
	.name           = "keyboard-backlight",
	.id             =  -1,
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

static struct serial_data plat_data3 = {
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
	},
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

static struct i2c_board_info i2c1_boardinfo[] = {
	{I2C_BOARD_INFO("sensor_main",0x3C),},
	{I2C_BOARD_INFO("sensor_sub",0x21),},
};

static struct i2c_board_info i2c0_boardinfo[] = {
	{ I2C_BOARD_INFO(LIS3DH_ACC_I2C_NAME, LIS3DH_ACC_I2C_ADDR),
	  .platform_data = &lis3dh_plat_data,
	},
	{ I2C_BOARD_INFO(AKM8975_I2C_NAME,    AKM8975_I2C_ADDR),
	  .platform_data = &akm8975_platform_d,
	},
	{ I2C_BOARD_INFO(AL3006_PLS_DEVICE,   AL3006_PLS_ADDRESS),
	  .platform_data = &al3006_pls_info,
	},
};

/* config I2C2 SDA/SCL to SIM2 pads */
static void sprd8810_i2c2sel_config(void)
{
	sprd_greg_set_bits(REG_TYPE_GLOBAL, PINCTRL_I2C2_SEL, GR_PIN_CTL);
}

static int sc8810_add_i2c_devices(void)
{
	sprd8810_i2c2sel_config();
	i2c_register_board_info(2, i2c2_boardinfo, ARRAY_SIZE(i2c2_boardinfo));
	i2c_register_board_info(1, i2c1_boardinfo, ARRAY_SIZE(i2c1_boardinfo));
	i2c_register_board_info(0, i2c0_boardinfo, ARRAY_SIZE(i2c0_boardinfo));
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
		.init = NULL,
		.control = NULL,
	},
};

static int spi_cs_gpio_map[][2] = {
    {SPI0_CMMB_CS_GPIO,  0},
} ;

static struct spi_board_info spi_boardinfo[] = {
	{
	.modalias = "cmmb-dev",
	.bus_num = 0,
	.chip_select = 0,
	.max_speed_hz = 1000 * 1000,
	.mode = SPI_CPOL | SPI_CPHA,
	}
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
	return 0;
}

static void __init sc8810_init_machine(void)
{
	regulator_add_devices();
	sprd_add_otg_device();
	platform_device_add_data(&sprd_serial_device0,(const void*)&plat_data0,sizeof(plat_data0));
	platform_device_add_data(&sprd_serial_device1,(const void*)&plat_data1,sizeof(plat_data1));
	platform_device_add_data(&sprd_serial_device2,(const void*)&plat_data2,sizeof(plat_data2));
	platform_device_add_data(&sprd_serial_device3,(const void*)&plat_data3,sizeof(plat_data3));
	platform_add_devices(devices, ARRAY_SIZE(devices));
	platform_device_add_data(&sprd_sdio2_device, &modem_detect_gpio,
				 sizeof(modem_detect_gpio));
	sc8810_add_i2c_devices();
	sc8810_add_misc_devices();
	sprd_spi_init();
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	sprd_ramconsole_init();
#endif
}

static void __init sc8810_fixup(struct machine_desc *desc, struct tag *tag,
		char **cmdline, struct meminfo *mi)
{
}

static void __init sc8810_init_early(void)
{
	/* earlier init request than irq and timer */
	sc8810_clock_init();

}

MACHINE_START(SC8810OPENPHONE, "sp7710g2")
	.reserve	= sc8810_reserve,
	.map_io		= sc8810_map_io,
	.init_irq	= sc8810_init_irq,
	.timer		= &sc8810_timer,
	.init_machine	= sc8810_init_machine,
	.fixup		= sc8810_fixup,
	.init_early	= sc8810_init_early,
MACHINE_END

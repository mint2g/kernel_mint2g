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

#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/time.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include <linux/i2c.h>
#include <linux/i2c/qt602240_ts.h>
#include <linux/i2c/ltr_558als.h>
#include <linux/i2c/lis3dh.h>
#include <linux/akm8975.h>
#include <linux/spi/spi.h>
#include <mach/globalregs.h>
#include <mach/board.h>
#include <sound/audio_pa.h>
#include "../devices.h"
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <mach/gpio.h>
#include <linux/spi/mxd_cmmb_026x.h>

extern void __init sc8810_reserve(void);
extern void __init sc8810_map_io(void);
extern void __init sc8810_init_irq(void);
extern void __init sc8810_timer_init(void);
extern void __init regulator_add_devices(void);
extern void __init sc8810_clock_init(void);
#ifdef CONFIG_ANDROID_RAM_CONSOLE
extern int __init sprd_ramconsole_init(void);
#endif

static struct platform_device rfkill_device;
static struct platform_device brcm_bluesleep_device;
static struct platform_device kb_backlight_device;
/* Control ldo for brcm chip according to HW design */
static struct regulator *wlan_regulator_18=NULL;


/* Control ldo for maxscend cmmb chip according to HW design */
static struct regulator *cmmb_regulator_1v8 = NULL;

static struct platform_device *devices[] __initdata = {
	&sprd_serial_device0,
	&sprd_serial_device1,
	&sprd_serial_device2,
	&sprd_device_rtc,
	&sprd_nand_device,
	&sprd_lcd_device1,
	&sprd_backlight_device,
	&sprd_i2c_device0,
	&sprd_i2c_device1,
	&sprd_i2c_device2,
	&sprd_i2c_device3,
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
	&sprd_sdio1_device,
	&sprd_sdio0_device,
	&sprd_vsp_device,
	&sprd_dcam_device,
	&sprd_scale_device,
	&sprd_rotation_device,
	&rfkill_device,
	&brcm_bluesleep_device,
	&kb_backlight_device,
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
	.num_resources	= ARRAY_SIZE(rfkill_resources),
	.resource	= rfkill_resources,
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

static struct qt602240_platform_data qt602240_ts_info = {
	.irq_gpio_number	= EIC_TOUCH_IRQ,
	.reset_gpio_number	= GPIO_TOUCH_RESET,
};

static struct ltr558_pls_platform_data ltr558_pls_info = {
	.irq_gpio_number	= GPIO_PLSENSOR_IRQ,
};

static struct i2c_board_info i2c2_boardinfo[] = {
	{
		I2C_BOARD_INFO("qt602240_ts", 0x4a),
		.platform_data = &qt602240_ts_info,
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
	{ I2C_BOARD_INFO(LTR558_I2C_NAME,  LTR558_I2C_ADDR),
	  .platform_data = &ltr558_pls_info,
	},
	{ I2C_BOARD_INFO("ewtsa",  0x68),
	},
};

static struct i2c_board_info  i2c3_boardinfo[] = {
	{I2C_BOARD_INFO("gpsi2c", 0x1fa),
	.flags=I2C_M_TEN,
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

static void mxd_cmmb_poweron(void)
{
        regulator_set_voltage(cmmb_regulator_1v8, 1700000, 1800000);
        regulator_disable(cmmb_regulator_1v8);
        msleep(3);
        regulator_enable(cmmb_regulator_1v8);
        msleep(5);

        /* enable 26M external clock */
        gpio_direction_output(GPIO_CMMB_26M_CLK_EN, 1);
}

static void mxd_cmmb_poweroff(void)
{
        regulator_disable(cmmb_regulator_1v8);
        gpio_direction_output(GPIO_CMMB_26M_CLK_EN, 0);
}

static int mxd_cmmb_init(void)
{
         int ret=0;
         ret = gpio_request(GPIO_CMMB_26M_CLK_EN,   "MXD_CMMB_CLKEN");
         if (ret)
         {
                   pr_debug("mxd spi req gpio clk en err!\n");
                   goto err_gpio_init;
         }
         gpio_direction_output(GPIO_CMMB_26M_CLK_EN, 0);
         cmmb_regulator_1v8 = regulator_get(NULL, REGU_NAME_CMMBIO);
         return 0;

err_gpio_init:
	 gpio_free(GPIO_CMMB_26M_CLK_EN);
         return ret;
}

static struct mxd_cmmb_026x_platform_data mxd_plat_data = {
	.poweron  = mxd_cmmb_poweron,
	.poweroff = mxd_cmmb_poweroff,
	.init     = mxd_cmmb_init,
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
		.platform_data = &mxd_plat_data,
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
	return 0;
}

/*disable busmonitor*/
static void disable_bm(void)
{
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,
		AHB_CTL0_BM0_EN | AHB_CTL0_BM1_EN | AHB_CTL0_NFC_EN | AHB_CTL0_AXIBUSMON0_EN | AHB_CTL0_AXIBUSMON1_EN,
		AHB_CTL0);
}

static void __init sc8810_init_machine(void)
{
	int clk;
	regulator_add_devices();
	sprd_add_otg_device();
	platform_add_devices(devices, ARRAY_SIZE(devices));
	clk=48000000;
	platform_device_add_data(&sprd_serial_device0,(const void*)&clk,sizeof(int));
	clk=26000000;
	platform_device_add_data(&sprd_serial_device1,(const void*)&clk,sizeof(int));
	platform_device_add_data(&sprd_serial_device2,(const void*)&clk,sizeof(int));
	sc8810_add_i2c_devices();
	sc8810_add_misc_devices();
	sprd_spi_init();
	disable_bm();
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

MACHINE_START(SC8810OPENPHONE, "SP8810")
	.reserve	= sc8810_reserve,
	.map_io		= sc8810_map_io,
	.init_irq	= sc8810_init_irq,
	.timer		= &sc8810_timer,
	.init_machine	= sc8810_init_machine,
	.fixup		= sc8810_fixup,
	.init_early	= sc8810_init_early,
MACHINE_END

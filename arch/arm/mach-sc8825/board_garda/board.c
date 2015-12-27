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
#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/localtimer.h>

#include <mach/hardware.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
//#include <mach/globalregs.h>
#include <mach/board.h>
#include <mach/serial_sprd.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include "../devices.h"
#include <linux/ktd253b_bl.h>
#include <sound/audio_pa.h>
#include <linux/headset.h>
#include <gps/gpsctl.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>

#include <mach/sci.h>
#include <mach/hardware.h>
#include <mach/regs_glb.h>
#include <mach/regs_ahb.h>


extern void __init sc8825_reserve(void);
extern void __init sci_map_io(void);
extern void __init sc8825_init_irq(void);
extern void __init sc8825_timer_init(void);
extern int __init sc8825_regulator_init(void);
extern int __init sci_clock_init(void);
#ifdef CONFIG_ANDROID_RAM_CONSOLE
extern int __init sprd_ramconsole_init(void);
#endif

static struct platform_device rfkill_device;
static struct platform_device brcm_bluesleep_device;
static struct platform_device gpio_button_device;

static struct platform_gpsctl_data pdata_gpsctl = {
	.reset_pin = GPIO_GPS_RESET,
	.onoff_pin = GPIO_GPS_ONOFF,
	.clk_type = "clk_aux0",
	.pwr_type = "vdd18",
};

static struct platform_device gpsctl_dev = {
	.name = "gpsctl",
	.dev.platform_data = &pdata_gpsctl,
};

static struct platform_device *devices[] __initdata = {
	&sprd_serial_device0,
	&sprd_serial_device1,
	&sprd_serial_device2,
	&sprd_device_rtc,
	&sprd_nand_device,
	&sprd_lcd_device0,
	&sprd_backlight_device,
	&sprd_i2c_device0,
	&sprd_i2c_device1,
	&sprd_i2c_device2,
	&sprd_i2c_device3,
	&sprd_spi0_device,
	&sprd_spi1_device,
	&sprd_spi2_device,
	&sprd_keypad_device,
	&sprd_audio_platform_vbc_pcm_device,
	&sprd_audio_cpu_dai_vaudio_device,
	&sprd_audio_cpu_dai_vbc_device,
	&sprd_audio_codec_sprd_codec_device,
	&sprd_battery_device,
#ifdef CONFIG_ANDROID_PMEM
	&sprd_pmem_device,
	&sprd_pmem_adsp_device,
#endif
#ifdef CONFIG_ION
	&sprd_ion_dev,
#endif
	&sprd_emmc_device,
	&sprd_sdio0_device,
	&sprd_sdio1_device,
	&sprd_sdio2_device,
	&sprd_vsp_device,
	&sprd_dcam_device,
	&sprd_scale_device,
	&sprd_rotation_device,
	&sprd_sensor_device,
	&sprd_ahb_bm0_device,
	&sprd_ahb_bm1_device,
	&sprd_ahb_bm2_device,
	&sprd_ahb_bm3_device,
	&sprd_ahb_bm4_device,
	&sprd_axi_bm0_device,
	&sprd_axi_bm1_device,
	&sprd_axi_bm2_device,
	&rfkill_device,
	&brcm_bluesleep_device,
	&gpsctl_dev,
	&gpio_button_device,
#if defined(CONFIG_SPRD_PEER_STATE)
	&sprd_peer_state_device,
#endif
};

static struct platform_ktd253b_backlight_data ktd253b_data = {
       .max_brightness = 255,
       .dft_brightness = 50,
       .ctrl_pin       = GPIO_BK,
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

static struct sys_timer sc8825_timer = {
	.init = sc8825_timer_init,
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
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 48000000,
};
static struct serial_data plat_data1 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};
static struct serial_data plat_data2 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};

static struct i2c_board_info i2c2_boardinfo[] = {
	{
	//	I2C_BOARD_INFO(FT5206_TS_DEVICE, FT5206_TS_ADDR),
	//	.platform_data = &ft5x0x_ts_info,
	},
};

static struct i2c_board_info i2c1_boardinfo[] = {
	{I2C_BOARD_INFO("sensor_main",0x3C),},
	{I2C_BOARD_INFO("sensor_sub",0x21),},
};

static struct i2c_board_info i2c0_boardinfo[] = {
	{I2C_BOARD_INFO("zinitix_isp", 0x50),},
	{I2C_BOARD_INFO("Zinitix_tsp", 0x20),},
};

static int sc8810_add_i2c_devices(void)
{
	i2c_register_board_info(2, i2c2_boardinfo, ARRAY_SIZE(i2c2_boardinfo));
	i2c_register_board_info(1, i2c1_boardinfo, ARRAY_SIZE(i2c1_boardinfo));
	i2c_register_board_info(0, i2c0_boardinfo, ARRAY_SIZE(i2c0_boardinfo));
	return 0;
}

struct platform_device audio_pa_amplifier_device = {
	.name = "speaker-pa",
	.id = -1,
};

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

static int spi_cs_gpio_map[][2] = {
    {SPI0_CMMB_CS_GPIO,  0},
    {SPI0_CMMB_CS_GPIO,  0},
    {SPI0_CMMB_CS_GPIO,  0},
} ;

static struct spi_board_info spi_boardinfo[] = {
	{
	.modalias = "spidev",
	.bus_num = 0,
	.chip_select = 0,
	.max_speed_hz = 1000 * 1000,
	.mode = SPI_CPOL | SPI_CPHA,
	},
	{
	.modalias = "spidev",
	.bus_num = 1,
	.chip_select = 0,
	.max_speed_hz = 1000 * 1000,
	.mode = SPI_CPOL | SPI_CPHA,
	},
	{
	.modalias = "spidev",
	.bus_num = 2,
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
	if (audio_pa_control.speaker.control
	    || audio_pa_control.earpiece.control
	    || audio_pa_control.headset.control) {
		platform_set_drvdata(&audio_pa_amplifier_device,
				     &audio_pa_control);
		if (platform_device_register(&audio_pa_amplifier_device))
			pr_err("faile to install audio_pa_amplifier_device\n");
	}
	return 0;
}

const char * sc8825_regulator_map[] = {
	/*supply source, consumer0, consumer1, ..., NULL */
	"vdd28", "iic_vdd", "ctp_vdd", NULL,
	"vddsd0", "tflash_vcc", NULL,
	"vddsim0", "nfc_vcc", NULL,
	"vddsim1", "lcd_vcc", NULL,
	NULL,
};

int __init sc8825_regulator_init(void)
{
	static struct platform_device sc8825_regulator_device = {
		.name 	= "sprd-regulator",
		.id	= -1,
		.dev = {.platform_data = sc8825_regulator_map},
	};
	return platform_device_register(&sc8825_regulator_device);
}

int __init sc8825_clock_init_early(void)
{
	pr_info("ahb ctl0 %08x, ctl2 %08x glb gen0 %08x gen1 %08x clk_en %08x\n",
		sci_glb_raw_read(REG_AHB_AHB_CTL0),
		sci_glb_raw_read(REG_AHB_AHB_CTL2),
		sci_glb_raw_read(REG_GLB_GEN0),
		sci_glb_raw_read(REG_GLB_GEN1),
		sci_glb_raw_read(REG_GLB_CLK_EN));
	/* FIXME: Force disable all unused clocks */
	sci_glb_clr(REG_AHB_AHB_CTL0,
		BIT_AXIBUSMON2_EB	|
		BIT_AXIBUSMON1_EB	|
		BIT_AXIBUSMON0_EB	|
//		BIT_EMC_EB       	|
//		BIT_AHB_ARCH_EB  	|
//		BIT_SPINLOCK_EB  	|
		BIT_SDIO2_EB     	|
		BIT_EMMC_EB      	|
//		BIT_DISPC_EB     	|
		BIT_G3D_EB       	|
		BIT_SDIO1_EB     	|
		BIT_DRM_EB       	|
		BIT_BUSMON4_EB   	|
		BIT_BUSMON3_EB   	|
		BIT_BUSMON2_EB   	|
		BIT_ROT_EB       	|
		BIT_VSP_EB       	|
		BIT_ISP_EB       	|
		BIT_BUSMON1_EB   	|
		BIT_DCAM_MIPI_EB 	|
		BIT_CCIR_EB      	|
		BIT_NFC_EB       	|
		BIT_BUSMON0_EB   	|
//		BIT_DMA_EB       	|
//		BIT_USBD_EB      	|
		BIT_SDIO0_EB     	|
//		BIT_LCDC_EB      	|
		BIT_CCIR_IN_EB   	|
		BIT_DCAM_EB      	|
		0);
	sci_glb_clr(REG_AHB_AHB_CTL2,
//		BIT_DISPMTX_CLK_EN	|
		BIT_MMMTX_CLK_EN    |
//		BIT_DISPC_CORE_CLK_EN|
//		BIT_LCDC_CORE_CLK_EN|
		BIT_ISP_CORE_CLK_EN |
		BIT_VSP_CORE_CLK_EN |
		BIT_DCAM_CORE_CLK_EN|
		0);
	sci_glb_clr(REG_AHB_AHB_CTL3,
//		BIT_CLK_ULPI_EN		|
//		BIT_CLK_USB_REF_EN	|
		0);
	sci_glb_clr(REG_GLB_GEN0,
		BIT_IC3_EB          |
		BIT_IC2_EB          |
		BIT_IC1_EB          |
//		BIT_RTC_TMR_EB      |
//		BIT_RTC_SYST0_EB    |
		BIT_RTC_KPD_EB      |
		BIT_IIS1_EB         |
//		BIT_RTC_EIC_EB      |
		BIT_UART2_EB        |
//		BIT_UART1_EB        |
		BIT_UART0_EB        |
//		BIT_SYST0_EB        |
		BIT_SPI1_EB         |
		BIT_SPI0_EB         |
//		BIT_SIM1_EB         |
//		BIT_EPT_EB          |
		BIT_CCIR_MCLK_EN    |
//		BIT_PINREG_EB       |
		BIT_IIS0_EB         |
//		BIT_MCU_DSP_RST		|
//		BIT_EIC_EB     		|
		BIT_KPD_EB     		|
		BIT_EFUSE_EB   		|
//		BIT_ADI_EB     		|
//		BIT_GPIO_EB    		|
		BIT_I2C0_EB    		|
//		BIT_SIM0_EB    		|
//		BIT_TMR_EB     		|
		BIT_SPI2_EB    		|
		BIT_UART3_EB   		|
		0);
	sci_glb_clr(REG_AHB_CA5_CFG,
//		BIT_CA5_CLK_DBG_EN	|
		0);
	sci_glb_clr(REG_GLB_GEN1,
		BIT_AUDIF_AUTO_EN	|
		BIT_VBC_EN			|
		BIT_AUD_TOP_EB		|
		BIT_AUD_IF_EB		|
		BIT_CLK_AUX1_EN		|
		BIT_CLK_AUX0_EN		|
		0);
	sci_glb_clr(REG_GLB_CLK_EN,
		BIT_PWM3_EB			|
//		BIT_PWM2_EB			|
		BIT_PWM1_EB			|
//		BIT_PWM0_EB			|
		0);

	sci_glb_clr(REG_GLB_PCTRL,
	//		BIT_MCU_MPLL_EN 	|
	//		BIT_MCU_TDPLL_EN	|
	//		BIT_MCU_DPLL_EN 	|
			BIT_MCU_GPLL_EN);	/* clk_gpu */

	sci_glb_set(REG_GLB_TD_PLL_CTL,
	//		BIT_TDPLL_DIV2OUT_FORCE_PD	|	/* clk_384m */
	//		BIT_TDPLL_DIV3OUT_FORCE_PD	|	/* clk_256m */
	//		BIT_TDPLL_DIV4OUT_FORCE_PD	|	/* clk_192m */
	//		BIT_TDPLL_DIV5OUT_FORCE_PD	|	/* clk_153p6m */
			0);

	printk("sc8825 clock module early init ok\n");
	return 0;
}


static struct gpio_keys_button gpio_buttons[] = {
	{
		.gpio		= GPIO_HOME_KEY,
		.code		= KEY_HOMEPAGE,
		.desc		= "Home Key",
		.active_low	= 1,
	},
};

static struct gpio_keys_platform_data gpio_button_data = {
	.buttons	= gpio_buttons,
	.nbuttons	= ARRAY_SIZE(gpio_buttons),
};

static struct platform_device gpio_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &gpio_button_data,
	}
};

static void __init sc8825_init_machine(void)
{
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	sprd_ramconsole_init();
#endif
	sci_adc_init((void __iomem *)ADC_BASE);
	sc8825_regulator_init();
	sprd_add_otg_device();
	platform_device_add_data(&sprd_serial_device0,(const void*)&plat_data0,sizeof(plat_data0));
	platform_device_add_data(&sprd_serial_device1,(const void*)&plat_data1,sizeof(plat_data1));
	platform_device_add_data(&sprd_serial_device2,(const void*)&plat_data2,sizeof(plat_data2));
	platform_device_add_data(&sprd_backlight_device,&ktd253b_data,sizeof(ktd253b_data));
	platform_add_devices(devices, ARRAY_SIZE(devices));
	sc8810_add_i2c_devices();
	sc8810_add_misc_devices();
	sprd_spi_init();
}

extern void sc8825_enable_timer_early(void);
static void __init sc8825_init_early(void)
{
	/* earlier init request than irq and timer */
	sc8825_clock_init_early();
	sc8825_enable_timer_early();
	sci_adi_init();
}

/*
 * Setup the memory banks.
 */
 
static void __init sc8825_fixup(struct machine_desc *desc,
	struct tag *tags, char **cmdline, struct meminfo *mi)
{
}

MACHINE_START(SC8825OPENPHONE, "sc8825")	
	.reserve	= sc8825_reserve,
	.map_io		= sci_map_io,
	.fixup		= sc8825_fixup,
	.init_early	= sc8825_init_early,
	.init_irq	= sc8825_init_irq,
	.timer		= &sc8825_timer,
	.init_machine	= sc8825_init_machine,
MACHINE_END




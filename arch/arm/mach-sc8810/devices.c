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
#include <linux/platform_device.h>
// #include <linux/android_pmem.h>
#include <linux/ion.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/board.h>
#include "devices.h"

static struct resource sprd_serial_resources0[] = {
	[0] = {
		.start = SPRD_SERIAL0_BASE,
		.end = SPRD_SERIAL0_BASE + SPRD_SERIAL0_SIZE-1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER0_INT,
		.end = IRQ_SER0_INT,
		.flags = IORESOURCE_IRQ,
	},
};
struct platform_device sprd_serial_device0 = {
	.name           = "serial_sprd",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources0),
	.resource       = sprd_serial_resources0,
};

static struct resource sprd_serial_resources1[] = {
	[0] = {
		.start = SPRD_SERIAL1_BASE,
		.end = SPRD_SERIAL1_BASE + SPRD_SERIAL1_SIZE-1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER1_INT,
		.end = IRQ_SER1_INT,
		.flags = IORESOURCE_IRQ,
	},
};
struct platform_device sprd_serial_device1 = {
	.name           = "serial_sprd",
	.id             =  1,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources1),
	.resource       = sprd_serial_resources1,
};

static struct resource sprd_serial_resources2[] = {
	[0] = {
		.start = SPRD_SERIAL2_BASE,
		.end = SPRD_SERIAL2_BASE + SPRD_SERIAL2_SIZE-1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER2_INT,
		.end = IRQ_SER2_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_serial_device2 = {
	.name           = "serial_sprd",
	.id             =  2,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources2),
	.resource       = sprd_serial_resources2,
};

#ifdef CONFIG_ARCH_SC7710
static struct resource sprd_serial_resources3[] = {
	[0] = {
		.start = SPRD_SERIAL3_BASE,
		.end = SPRD_SERIAL3_BASE + SPRD_SERIAL3_SIZE-1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER3_INT,
		.end = IRQ_SER3_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_serial_device3 = {
	.name           = "serial_sprd",
	.id             =  3,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources3),
	.resource       = sprd_serial_resources3,
};
#endif

static struct resource resources_rtc[] = {
	[0] = {
		.start	= IRQ_ANA_RTC_INT,
		.end	= IRQ_ANA_RTC_INT,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device sprd_device_rtc= {
	.name	= "sprd_rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_rtc),
	.resource	= resources_rtc,
};

static struct resource sprd_nand_resources[] = {
	[0] = {
		.start	= 7,
		.end = 7,
		.flags	= IORESOURCE_DMA,
	},
	[1] = {
		.start	= SPRD_NAND_BASE,
		.end = SPRD_NAND_BASE + SPRD_NAND_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device sprd_nand_device = {
	.name		= "sprd-nand",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(sprd_nand_resources),
	.resource	= sprd_nand_resources,
};

static struct resource sprd_lcd_resources[] = {
	[0] = {
		.start = SPRD_LCDC_BASE,
		.end = SPRD_LCDC_BASE + SPRD_LCDC_SIZE - 1,
		.name = "lcd_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_LCDC_INT,
		.end = IRQ_LCDC_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_lcd_device0 = {
	.name           = "sprd_fb",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_lcd_resources),
	.resource       = sprd_lcd_resources,
};
struct platform_device sprd_lcd_device1 = {
	.name           = "sprd_fb",
	.id             =  1,
	.num_resources  = ARRAY_SIZE(sprd_lcd_resources),
	.resource       = sprd_lcd_resources,
};

static struct resource sprd_otg_resource[] = {
	[0] = {
		.start = SPRD_USB_BASE,
		.end   = SPRD_USB_BASE + SPRD_USB_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_USBD_INT,
		.end   = IRQ_USBD_INT,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device sprd_otg_device = {
	.name		= "dwc_otg",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(sprd_otg_resource),
	.resource	= sprd_otg_resource,
};

struct platform_device sprd_backlight_device = {
	.name           = "panel",
	.id             =  -1,
};

static struct resource sprd_i2c_resources0[] = {
	[0] = {
		.start = SPRD_I2C0_BASE,
		.end   = SPRD_I2C0_BASE + SPRD_I2C0_SIZE -1,
		.name  = "i2c0_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C0_INT,
		.end   = IRQ_I2C0_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_i2c_device0 = {
	.name           = "sc8810-i2c",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_i2c_resources0),
	.resource       = sprd_i2c_resources0,
};


static struct resource sprd_i2c_resources1[] = {
	[0] = {
		.start = SPRD_I2C1_BASE,
		.end   = SPRD_I2C1_BASE + SPRD_I2C1_SIZE -1,
		.name  = "i2c1_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C1_INT,
		.end   = IRQ_I2C1_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_i2c_device1 = {
	.name           = "sc8810-i2c",
	.id             =  1,
	.num_resources  = ARRAY_SIZE(sprd_i2c_resources1),
	.resource       = sprd_i2c_resources1,
};


static struct resource sprd_i2c_resources2[] = {
	[0] = {
		.start = SPRD_I2C2_BASE,
		.end   = SPRD_I2C2_BASE + SPRD_I2C2_SIZE -1,
		.name  = "i2c2_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C2_INT,
		.end   = IRQ_I2C2_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_i2c_device2 = {
	.name           = "sc8810-i2c",
	.id             =  2,
	.num_resources  = ARRAY_SIZE(sprd_i2c_resources2),
	.resource       = sprd_i2c_resources2,
};


static struct resource sprd_i2c_resources3[] = {
	[0] = {
		.start = SPRD_I2C3_BASE,
		.end   = SPRD_I2C3_BASE + SPRD_I2C3_SIZE -1,
		.name  = "i2c3_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C3_INT,
		.end   = IRQ_I2C3_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_i2c_device3 = {
	.name           = "sc8810-i2c",
	.id             =  3,
	.num_resources  = ARRAY_SIZE(sprd_i2c_resources3),
	.resource       = sprd_i2c_resources3,
};

/* 8810 SPI devices.  */
static struct resource spi0_resources[] = {
    [0] = {
        .start = SPRD_SPI0_BASE,
        .end = SPRD_SPI0_BASE + SPRD_SPI0_SIZE - 1,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = IRQ_SPI0_INT,
        .end = IRQ_SPI0_INT,
        .flags = IORESOURCE_IRQ,
    },
};


static struct resource spi1_resources[] = {
    [0] = {
        .start = SPRD_SPI1_BASE,
        .end = SPRD_SPI1_BASE + SPRD_SPI1_SIZE - 1,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = IRQ_SPI1_INT,
        .end = IRQ_SPI1_INT,
        .flags = IORESOURCE_IRQ,
    },
};


struct platform_device sprd_spi0_device = {
    .name = "sprd_spi",
    .id = 0,
    .resource = spi0_resources,
    .num_resources = ARRAY_SIZE(spi0_resources),
};

struct platform_device sprd_spi1_device = {
    .name = "sprd_spi",
    .id = 1,
    .resource = spi1_resources,
    .num_resources = ARRAY_SIZE(spi1_resources),
};


static struct resource sprd_keypad_resources[] = {
        {
                .start = IRQ_KPD_INT,
                .end = IRQ_KPD_INT,
                .flags = IORESOURCE_IRQ,
        },
};

struct platform_device sprd_keypad_device = {
        .name           = "sprd-keypad",
        .id             = -1,
        .num_resources  = ARRAY_SIZE(sprd_keypad_resources),
        .resource       = sprd_keypad_resources,
};

struct platform_device sprd_audio_soc_device = {
	.name           = "sc88xx-pcm-audio",
	.id             =  -1,
};

struct platform_device sprd_audio_soc_vbc_device = {
	.name           = "sc88xx-vbc",
	.id             =  -1,
};

struct platform_device sprd_audio_vbc_device = {
	.name           = "vbc-codec",
	.id             =  -1,
};

#if defined(CONFIG_SND_SPRD_SOC_SC881X) || defined(CONFIG_SND_SPRD_SOC_KYLEW) || defined(CONFIG_SND_SPRD_SOC_MINT)
struct platform_device sprd_audio_platform_vbc_pcm_device = {
	.name           = "sprd-vbc-pcm-audio",
	.id             =  -1,
};

struct platform_device sprd_audio_cpu_dai_vaudio_device = {
	.name           = "vaudio",
	.id             =  -1,
};

struct platform_device sprd_audio_cpu_dai_vbc_device = {
	.name           = "vbc",
	.id             =  -1,
};

struct platform_device sprd_audio_codec_dolphin_device = {
	.name           = "dolphin",
	.id             =  -1,
};
#endif

static void platform_sprd_battery_release(struct device * dev)
{
    return ;
}

static struct resource sprd_battery_resources[] = {
        [0] = {
                .start = EIC_CHARGER_DETECT,
                .end = EIC_CHARGER_DETECT,
                .flags = IORESOURCE_IO,
        }
};

struct platform_device sprd_battery_device = {
        .name           = "sprd-battery",
        .id             =  0,
        .num_resources  = ARRAY_SIZE(sprd_battery_resources),
        .resource       = sprd_battery_resources,
	.dev = {
		.release = platform_sprd_battery_release,
	}
};

struct platform_device sprd_vsp_device = {
	.name	= "sprd_vsp",
	.id	= -1,
};

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data sprd_pmem_pdata = {
	.name = "pmem",
	.start = SPRD_PMEM_BASE,
	.size = SPRD_PMEM_SIZE,
	.no_allocator = 0,
	.cached = 1,
};

static struct android_pmem_platform_data sprd_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.start = SPRD_PMEM_ADSP_BASE,
	.size = SPRD_PMEM_ADSP_SIZE,
	.no_allocator = 0,
	.cached = 1,
};

struct platform_device sprd_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = {.platform_data = &sprd_pmem_pdata},
};

struct platform_device sprd_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = {.platform_data = &sprd_pmem_adsp_pdata},
};
#endif

#ifdef CONFIG_ION
static struct ion_platform_data ion_pdata = {
#if CONFIG_SPRD_ION_OVERLAY_SIZE
	.nr = 2,
#else
	.nr = 1,
#endif
	.heaps = {
		{
			.id	= ION_HEAP_TYPE_CARVEOUT,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= "ion_carveout_heap",
			.base   = SPRD_ION_BASE,
			.size   = SPRD_ION_SIZE,
		},
#if CONFIG_SPRD_ION_OVERLAY_SIZE
		{
			.id	= ION_HEAP_TYPE_CARVEOUT + 1,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= "ion_carveout_heap_overlay",
			.base   = SPRD_ION_OVERLAY_BASE,
			.size   = SPRD_ION_OVERLAY_SIZE,
		},
#endif
	}
};

struct platform_device sprd_ion_dev = {
	.name = "ion-sprd",
	.id = -1,
	.dev = { .platform_data = &ion_pdata },
};
#endif

static struct resource sprd_dcam_resources[] = {
	{
		.start	= SPRD_ISP_BASE,
		.end	= SPRD_ISP_BASE + SPRD_ISP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= IRQ_ISP_INT,
		.end	= IRQ_ISP_INT,
		.flags	= IORESOURCE_IRQ,
	},
};
struct platform_device sprd_dcam_device = {
	.name		= "sprd_dcam",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(sprd_dcam_resources),
	.resource	= sprd_dcam_resources,
};
struct platform_device sprd_scale_device = {
	.name	= "sprd_scale",
	.id	= -1,
};

struct platform_device sprd_rotation_device = {
	.name	= "sprd_rotation",
	.id	= -1,
};

static struct resource sprd_sdio0_resources[] = {
	[0] = {
		.start = SPRD_SDIO0_BASE,
		.end = SPRD_SDIO0_BASE + SPRD_SDIO0_SIZE-1,
		.name = "sdio0_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SDIO0_INT,
		.end = IRQ_SDIO0_INT,
		.flags = IORESOURCE_IRQ,
	}
};
struct platform_device sprd_sdio0_device = {
	.name           = "sprd-sdhci",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_sdio0_resources),
	.resource       = sprd_sdio0_resources,
};

static struct resource sprd_sdio1_resources[] = {
	[0] = {
		.start = SPRD_SDIO1_BASE,
		.end = SPRD_SDIO1_BASE + SPRD_SDIO1_SIZE-1,
		.name = "sdio1_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SDIO1_INT,
		.end = IRQ_SDIO1_INT,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device sprd_sdio1_device = {
	.name           = "sprd-sdhci",
	.id             =  1,
	.num_resources  = ARRAY_SIZE(sprd_sdio1_resources),
	.resource       = sprd_sdio1_resources,
};

#ifdef CONFIG_ARCH_SC7710
static struct resource sprd_sdio2_resources[] = {
	[0] = {
		.start = SPRD_SDIO2_BASE,
		.end = SPRD_SDIO2_BASE + SPRD_SDIO2_SIZE-1,
		.name = "sdio2_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SDIO2_INT,
		.end = IRQ_SDIO2_INT,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device sprd_sdio2_device = {
	.name           = "sprd-sdhci",
	.id             =  2,
	.num_resources  = ARRAY_SIZE(sprd_sdio2_resources),
	.resource       = sprd_sdio2_resources,
};

static struct resource sprd_emmc0_resources[] = {
	[0] = {
		.start = SPRD_EMMC_BASE,
		.end = SPRD_EMMC_BASE + SPRD_EMMC_SIZE-1,
		.name = "emmc0_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_EMMC_INT,
		.end = IRQ_EMMC_INT,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device sprd_emmc0_device = {
	.name           = "sprd-sdhci",
	.id             =  3,
	.num_resources  = ARRAY_SIZE(sprd_emmc0_resources),
	.resource       = sprd_emmc0_resources,
};


#endif

static struct resource sprd_tp_resources[] = {
        {
                .start  = (SPRD_MISC_BASE +0x280),
                .end    = (SPRD_MISC_BASE + 0x280+0x44),
                .flags  = IORESOURCE_MEM,
        },
        {
                .start  = IRQ_ANA_TPC_INT,
                .end    = IRQ_ANA_TPC_INT,
                .flags  = IORESOURCE_IRQ,
        },
};

struct platform_device sprd_tp_device = {
        .name           = "sprd-tp",
        .id             = 0,
        .num_resources  = ARRAY_SIZE(sprd_tp_resources),
        .resource       = sprd_tp_resources,
};

struct platform_device sprd_peer_state_device = {
        .name           = "peer_state",
        .id             = -1,
};

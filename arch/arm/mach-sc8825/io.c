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

#include <asm/io.h>
#include <asm/page.h>
#include <asm/mach/map.h>
#include <mach/hardware.h>

#define SPRD_DEVICE(name) { \
	.virtual = SPRD_##name##_BASE, \
	.pfn = __phys_to_pfn(SPRD_##name##_PHYS), \
	.length = SPRD_##name##_SIZE, \
	.type = MT_DEVICE_NONSHARED, \
	}
#define SPRD_IRAM(name) { \
	.virtual = SPRD_##name##_BASE, \
	.pfn = __phys_to_pfn(SPRD_##name##_PHYS), \
	.length = SPRD_##name##_SIZE, \
	.type = MT_MEMORY, \
	}

static struct map_desc sprd_io_desc[] __initdata = {	
	SPRD_DEVICE(CORESIGHT),
	SPRD_DEVICE(A5MP),	
	SPRD_DEVICE(MALI),
	SPRD_DEVICE(DMA0),
	SPRD_DEVICE(DCAM),	
	SPRD_DEVICE(USB),
	
	SPRD_DEVICE(SDIO0),
	SPRD_DEVICE(SDIO1),	
	SPRD_DEVICE(SDIO2),

	SPRD_DEVICE(LCDC),	
	SPRD_DEVICE(ROTO),	
	SPRD_DEVICE(AHB),	
	SPRD_DEVICE(DRM),
	SPRD_DEVICE(MEA),//VSP
	SPRD_DEVICE(EMMC),
	SPRD_DEVICE(DISPLAY),	
	SPRD_DEVICE(NFC),
	SPRD_DEVICE(GPTIMER),
	SPRD_DEVICE(ISP),
	SPRD_DEVICE(GREG),
	SPRD_DEVICE(PIN),
	SPRD_DEVICE(EPT),
	SPRD_DEVICE(INTC0),
	SPRD_DEVICE(I2C0),	
	SPRD_DEVICE(BM0),
	SPRD_DEVICE(AXIBM0),
	SPRD_DEVICE(HWLOCK),
	SPRD_DEVICE(ADI),

	SPRD_DEVICE(UART0),
	SPRD_DEVICE(UART1),
	SPRD_DEVICE(UART2),	
	SPRD_DEVICE(UART3), 

	SPRD_DEVICE(TDPROC),
	
	SPRD_DEVICE(SIM0),
	SPRD_DEVICE(SIM1),	
	SPRD_DEVICE(KPD),
	
	SPRD_DEVICE(SYSCNT),
	
	SPRD_DEVICE(PWM),
	
	SPRD_DEVICE(EFUSE),
	SPRD_DEVICE(GPIO),
	SPRD_DEVICE(EIC),	
	SPRD_DEVICE(IPI),

	SPRD_DEVICE(IIS0),
	SPRD_DEVICE(IIS1),

	SPRD_DEVICE(SPI0),
	SPRD_DEVICE(SPI1),
	SPRD_DEVICE(SPI2),
	SPRD_DEVICE(MIPI_DSIC),

	SPRD_DEVICE(LPDDR2C),
	
	SPRD_DEVICE(L2),
	SPRD_IRAM(IRAM),

	SPRD_DEVICE(CSI)
};

void __init sci_map_io(void)
{
	iotable_init(sprd_io_desc, ARRAY_SIZE(sprd_io_desc));
}

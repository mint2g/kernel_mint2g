/*
 * linux/arch/arm/mach-ne1/ne1tb.c
 *
 * Copyright (C) NEC Electronics Corporation 2007, 2008
 *
 * This file is based on arch/arm/mach-realview/ne1tb.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>
#include <linux/amba/bus.h>
#include <linux/irq.h>

#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/leds.h>
#include <asm/mach-types.h>
#include <asm/hardware/gic.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

// #include <asm/mach/mmc.h>

#include <mach/irqs.h>
#include <mach/ne1_sysctrl.h>

#include "core.h"
#include "clock.h"

#ifdef    CONFIG_NKERNEL

#include <nk/nkern.h>

extern void nk_ddi_init(void);

/*
 * Generic OSware timer.
 */
#ifdef CONFIG_GENERIC_CLOCKEVENTS
extern struct sys_timer nk_vtick_timer;
#else
extern struct sys_timer osware_timer;
#endif

#endif /* CONFIG_NKERNEL */

unsigned int ne1_get_boot_id(void)
{
	return readl(IO_ADDRESS(NE1_BASE_SYSCTRL + SYSCTRL_BOOTID));
}

unsigned int ne1_get_board_id(void)
{
	return ((ne1_get_boot_id() >> 25) & 0x7);
}


static struct map_desc ne1_io_desc[] __initdata = {

	/* Map I/O memory for the NaviEngine1 embedded controllers */
	{
		.virtual	= IO_ADDRESS(NE1_BASE_INTERNAL_IO),
		.pfn		= __phys_to_pfn(NE1_BASE_INTERNAL_IO),
		.length		= SZ_1M,
		.type		= MT_DEVICE,
	},
	/* Map MPCore private region */
	{
		.virtual	= IO_ADDRESS(NE1_BASE_MPCORE_PRIVATE),
		.pfn		= __phys_to_pfn(NE1_BASE_MPCORE_PRIVATE),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	},
	/* Map NE1 local memory */
	{
		.virtual	= IO_ADDRESS(NE1_BASE_LMEM),
		.pfn		= __phys_to_pfn(NE1_BASE_LMEM),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
#if defined(CONFIG_PCI)
	/* Map NE1 PCI */
	{
		.virtual	= IO_ADDRESS(NE1_BASE_PCI),
		.pfn		= __phys_to_pfn(NE1_BASE_PCI),
		.length		= SZ_128M,
		.type		= MT_DEVICE,
	},
#endif
	/* Map NE1-xB LAN9118 */
	{
		.virtual	= IO_ADDRESS(NE1xB_BASE_NIC),
		.pfn		= __phys_to_pfn(NE1xB_BASE_NIC),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	},
#if defined(CONFIG_MACH_NE1TB)
	/* Map NE1-TB FPGA, including RTC */
	{
		.virtual	= IO_ADDRESS(NE1TB_BASE_FPGA),
		.pfn		= __phys_to_pfn(NE1TB_BASE_FPGA),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
#endif
#if defined(CONFIG_MACH_NE1DB)
	/* Map NE1-DB RTC */
	{
		.virtual	= IO_ADDRESS(NE1DB_BASE_RTC_EX),
		.pfn		= __phys_to_pfn(NE1DB_BASE_RTC_EX),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	},
	/* Map NE1-DB on-board UART */
	{
		.virtual	= IO_ADDRESS(NE1DB_BASE_UART_EX),
		.pfn		= __phys_to_pfn(NE1DB_BASE_UART_EX),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	},
#endif
};

static void __init ne1_map_io(void)
{
	iotable_init(ne1_io_desc, ARRAY_SIZE(ne1_io_desc));
}


// AMBA_DEVICE(uart0, "UART0", UART_0, NULL);
AMBA_DEVICE(uart1, "UART1", UART_1, NULL);
AMBA_DEVICE(uart2, "UART2", UART_2, NULL);

static struct amba_device *amba_devs[] __initdata = {
//	&uart0_device,
	&uart1_device,
	&uart2_device,
};

static void __init gic_init_irq(void)
{
#ifndef   CONFIG_NKERNEL
	/*
	 * Initialize MPCore GIC, GPIO
	 */
	gic_init(0, 29, __io_address(NE1_GIC_DIST_BASE),
		 __io_address(NE1_GIC_CPU_BASE));
	gpio_init(0, __io_address(NE1_BASE_GPIO), IRQ_GPIO_BASE);
	gpio_cascade_irq(0, IRQ_GPIO);
#else  /* CONFIG_NKERNEL */

	gic_init(0, 29, __io_address(NE1_GIC_DIST_BASE),
		 __io_address(NE1_GIC_CPU_BASE));
	nk_ddi_init();
	gpio_init(0, __io_address(NE1_BASE_GPIO), IRQ_GPIO_BASE);
#endif /* CONFIG_NKERNEL */
}

static void __init ne1_init(void)
{
	int i;

	ne1_core_init();

	for (i = 0; i < ARRAY_SIZE(amba_devs); i++) {
		struct amba_device *d = amba_devs[i];
		amba_device_register(d, &iomem_resource);
	}

#ifdef CONFIG_LEDS
	leds_event = ne1tb_leds_event;
#endif
}

MACHINE_START(NE1BOARD, "NE1")
#ifndef CONFIG_NKERNEL
	.phys_io	= NE1_BASE_UART_0,
	.io_pg_offst	= (IO_ADDRESS(NE1_BASE_UART_0) >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
#endif
	.map_io		= ne1_map_io,
	.init_irq	= gic_init_irq,
#ifdef CONFIG_NKERNEL
#ifdef CONFIG_GENERIC_CLOCKEVENTS
	.timer		= &nk_vtick_timer,
#else
	.timer		= &osware_timer,
#endif
#else
	.timer		= &ne1_timer,
#endif
	.init_machine	= ne1_init,
MACHINE_END



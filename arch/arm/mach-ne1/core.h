/*
 * linux/arch/arm/mach-ne1/core.h
 *
 * Copyright (C) NEC Electronics Corporation 2007, 2008
 *
 * This file is based on arch/arm/mach-realview/core.h
 *
 * Copyright (C) 2004 ARM Limited
 * Copyright (C) 2000 Deep Blue Solutions Ltd
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

#ifndef __ARCH_CORE_H
#define __ARCH_CORE_H

#include <linux/amba/bus.h>

#include <asm/leds.h>
#include <asm/io.h>


extern struct sys_timer ne1_timer;

#define AMBA_DEVICE(name,busid,base,plat)			\
static struct amba_device name##_device = {			\
	.dev	= {						\
		.coherent_dma_mask = ~0,			\
		.init_name = busid,				\
		.platform_data = plat,				\
	},							\
	.res	= {						\
		.start	= NE1_BASE_##base,			\
		.end	= (NE1_BASE_##base) + SZ_4K - 1,	\
		.flags	= IORESOURCE_MEM,			\
	},							\
	.dma_mask	= ~0,					\
	.irq		= base##_IRQ,				\
/*	.dma		= base##_DMA,	*/			\
}

#define UART_0_IRQ		{ IRQ_UART_0, NO_IRQ }
#define UART_0_DMA		{ 0, 0 }
#define UART_1_IRQ		{ IRQ_UART_1, NO_IRQ }
#define UART_1_DMA		{ 0, 0 }
#define UART_2_IRQ		{ IRQ_UART_2, NO_IRQ }
#define UART_2_DMA		{ 0, 0 }

extern void ne1_core_init(void);

#if defined(CONFIG_NE1_USB)
extern struct platform_device ne1xb_ohci_device;
extern struct platform_device ne1xb_ehci_device;
#endif

extern void ne1tb_leds_event(led_event_t ledevt);


#endif /* __ARCH_CORE_H */

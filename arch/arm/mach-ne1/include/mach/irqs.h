/*
 * linux/include/asm-arm/arch-ne1/irqs.h
 *
 * Copyright (C) NEC Electronics Corporation 2007, 2008
 *
 * This file is based on include/asm-arm/arch-realview/irqs.h
 *
 * Copyright (C) 2003 ARM Limited
 * Copyright (C) 2000 Deep Blue Solutions Ltd.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
 */

#ifndef __ASM_ARCH_IRQS_H
#define __ASM_ARCH_IRQS_H

#include <linux/compiler.h>
#include <linux/cpumask.h>
#include <mach/platform.h>


#define IRQ_LOCALTIMER			INT_PTMR
#define IRQ_LOCALWDOG			INT_PWDT

#define IRQ_UART_0			INT_UART_0
#define IRQ_UART_1			INT_UART_1
#define IRQ_UART_2			INT_UART_2

#define IRQ_TIMER_0			INT_TIMER_0

#define	IRQ_ETH				INT_NIC

#define	IRQ_PCI_INTC			INT_PCI_INTC

#define	IRQ_GPIO			INT_GPIO
#define	IRQ_GPIO_BASE			INT_NE1_MAXIMUM

#define	IRQ_OHCI			INT_USBH_INTA
#define	IRQ_EHCI			INT_USBH_INTB

#define IRQ_MMC				INT_SD_0

#define IRQ_DMAC32_END_0	INT_DMAC32_END_0
#define IRQ_DMAC32_ERR_0	INT_DMAC32_ERR_0
#define IRQ_DMAC32_END_1	INT_DMAC32_END_1
#define IRQ_DMAC32_ERR_1	INT_DMAC32_ERR_1
#define IRQ_DMAC32_END_2	INT_DMAC32_END_2
#define IRQ_DMAC32_ERR_2	INT_DMAC32_ERR_2
#define IRQ_DMAC32_END_3	INT_DMAC32_END_3
#define IRQ_DMAC32_ERR_3	INT_DMAC32_ERR_3
#define IRQ_DMAC32_END_4	INT_DMAC32_END_4
#define IRQ_DMAC32_ERR_4	INT_DMAC32_ERR_4
#define IRQ_DMAC_EXBUS_END	INT_DMAC_EXBUS_END
#define IRQ_DMAC_EXBUS_ERR	INT_DMAC_EXBUS_ERR
#define IRQ_DMAC_AXI_END	INT_DMAC_AXI_END
#define IRQ_DMAC_AXI_ERR	INT_DMAC_AXI_ERR

#define IRQ_RTC				INT_RTC

#define NR_IRQS				INT_MAXIMUM


void gpio_cascade_irq(unsigned int gpio_nr, unsigned int irq);
void gpio_init(unsigned int gpio_nr, void __iomem *base, unsigned int irq_offset);


#endif /* __ASM_ARCH_IRQS_H */


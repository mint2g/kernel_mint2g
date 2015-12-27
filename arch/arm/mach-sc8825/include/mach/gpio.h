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

#ifndef __ASM_ARM_ARCH_GPIO_H
#define __ASM_ARM_ARCH_GPIO_H

/*
 * SC8825 GPIO&EIC bank and number summary:
 *
 * Bank	  From	  To	NR	Type
 * 1	  0   ~	  15		16	EIC
 * 2	  16  ~	  271	256	GPIO
 * 3	  272 ~	  287	16	ANA EIC
 * 4	  288 ~	  319	32	ANA GPIO
 */

#define	D_EIC_START		0
#define	D_EIC_NR		16

#define	D_GPIO_START	( D_EIC_START + D_EIC_NR )
#define	D_GPIO_NR		256

#define	A_EIC_START		( D_GPIO_START + D_GPIO_NR )
#define	A_EIC_NR		16

#define	A_GPIO_START	( A_EIC_START + A_EIC_NR )
#define	A_GPIO_NR		32

#define ARCH_NR_GPIOS	( D_EIC_NR + D_GPIO_NR + A_EIC_NR + A_GPIO_NR )

#include <asm-generic/gpio.h>
#include <mach/irqs.h>

#define gpio_get_value  __gpio_get_value
#define gpio_set_value  __gpio_set_value
#define gpio_cansleep   __gpio_cansleep
#define gpio_to_irq     __gpio_to_irq

static inline int irq_to_gpio(int irq)
{
	return irq - GPIO_IRQ_START;
}

#endif

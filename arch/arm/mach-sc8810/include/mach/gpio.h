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

#ifdef CONFIG_ARCH_SC7710
#define ARCH_NR_GPIOS		320
#endif

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

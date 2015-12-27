/*
 *  File Name       : linux/include/asm-arm/arch-ne1/gpio.h
 *  Function        : gpio
 *  Release Version : Ver 1.00
 *  Release Date    : 2008/06/11
 *
 *  Copyright (C) NEC Electronics Corporation 2007
 *
 *
 *  This program is free software;you can redistribute it and/or modify it under the terms of
 *  the GNU General Public License as published by Free Softwere Foundation; either version 2
 *  of License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warrnty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program;
 *  If not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 *  MA 02111-1307, USA.
 *
 */
#ifndef __ASM_ARM_ARCH_NE1_GPIO_H
#define __ASM_ARM_ARCH_NE1_GPIO_H

#include <asm/errno.h>
#include <asm/io.h>
#include <mach/irqs.h>
#include <mach/hardware.h>

/* GPIO port */

/* GIO Registers */
#define VA_GPIO		IO_ADDRESS(NE1_BASE_GPIO)

#define	GPIO_PCE		0x00	/* set output [R/W]*/
#define	GPIO_PCD		0x04	/* set input [W]*/
#define	GPIO_SCH		0x08	/* data output(16-31) [W]*/
#define	GPIO_SCL		0x0c	/* data output( 0-15) [W]*/
#define	GPIO_PV			0x10	/* data monitor [R] */
#define	GPIO_INT		0x14	/* interrupt status [R/W]*/
#define	GPIO_INE		0x18	/* interrupt unmask [R/W]*/
#define	GPIO_IND		0x1c	/* interrupt mask [W] */
#define	GPIO_INH		0x20	/* interrupt hold [R/W] */
#define	GPIO_IM0		0x24	/* interrupt detect control ( 0- 7) [R/W] */
#define	GPIO_IM1		0x28	/* interrupt detect control ( 8-15) [R/W] */
#define	GPIO_IM2		0x2c	/* interrupt detect control (16-23) [R/W] */
#define	GPIO_IM3		0x30	/* interrupt detect control (24-31) [R/W] */
#define	GPIO_IPOLINV	0x38	/* input polarity invert [R/W] */
#define	GPIO_IPOLRST	0x3c	/* input polarity reset [W] */
#define	GPIO_OPOLINV	0x40	/* output polarity invert [R/W] */
#define	GPIO_OPOLRST	0x44	/* output polarity reset [W] */

/* GPIO number */
#define GPIO_P0		0
#define GPIO_P1		1
#define GPIO_P2		2
#define GPIO_P3		3
#define GPIO_P4		4
#define GPIO_P5		5
#define GPIO_P6		6
#define GPIO_P7		7
#define GPIO_P8		8
#define GPIO_P9		9
#define GPIO_P10	10
#define GPIO_P11	11
#define GPIO_P12	12
#define GPIO_P13	13
#define GPIO_P14	14
#define GPIO_P15	15
#define GPIO_P16	16
#define GPIO_P17	17
#define GPIO_P18	18
#define GPIO_P19	19
#define GPIO_P20	20
#define GPIO_P21	21
#define GPIO_P22	22
#define GPIO_P23	23
#define GPIO_P24	24
#define GPIO_P25	25
#define GPIO_P26	26
#define GPIO_P27	27
#define GPIO_P28	28
#define GPIO_P29	29
#define GPIO_P30	30
#define GPIO_P31	31
#define GPIO_LAST	GPIO_P31

/* board specific */
#define GPIO_UART1_DSR	GPIO_P8		/* I */
#define GPIO_UART1_DTR	GPIO_P9		/* O */
#define GPIO_UART1_DCD	GPIO_P10	/* I */
#define GPIO_CKERST		GPIO_P11	/* O */
#define GPIO_CSI0_CS1	GPIO_P12	/* O */
#define GPIO_CSI1_CS1	GPIO_P13	/* O */
#define GPIO_CSI0_CS2	GPIO_P14	/* O */
#define GPIO_CSI1_CS2	GPIO_P15	/* O */
#define GPIO_PCI_INTC	GPIO_P16	/* I */
#define GPIO_PCI_INTB	GPIO_P17	/* I */
#define GPIO_PCI_INTA	GPIO_P18	/* I */
#define GPIO_NIC		GPIO_P20	/* I */
#define GPIO_ETRON		GPIO_P21	/* I */
#define GPIO_KEY		GPIO_P22	/* I */
#define GPIO_RTC		GPIO_P23	/* I */
#define GPIO_LINT		GPIO_P24	/* I */
#define GPIO_AUDIO_RESET GPIO_P25	/* O */
#define GPIO_SW0		GPIO_P27	/* I */
#define GPIO_SW1		GPIO_P28	/* I */


static inline int __gpio_set_direction(unsigned gpio, int is_input)
{
	unsigned int reg = is_input ? (VA_GPIO + GPIO_PCD) : (VA_GPIO + GPIO_PCE);

	writel(1U << (gpio & 0x1f), reg);

	return 0;
}

static inline void __gpio_set_value(unsigned gpio, int val)
{
	unsigned int reg_val;

	reg_val = 1U << (gpio & 0xf);
	if (val == 0) reg_val <<= 16;

	if (gpio < 16) {
		writel(reg_val, (VA_GPIO + GPIO_SCL));
	} else {
		writel(reg_val, (VA_GPIO + GPIO_SCH));
	}
}

static inline int __gpio_get_value(unsigned gpio)
{
	unsigned int val = readl(VA_GPIO + GPIO_PV);

	return (val & (1U << (gpio & 0x1f))) ? 1 : 0;
}

static inline int gpio_direction_input(unsigned gpio)
{
	if (gpio <= GPIO_LAST) {
		return __gpio_set_direction(gpio, 1);
	} else {
		return -EINVAL;
	}
}

static inline int gpio_direction_output(unsigned gpio, int value)
{
	if (gpio <= GPIO_LAST) {
		__gpio_set_value(gpio, value);
		return __gpio_set_direction(gpio, 0);
	} else {
		return -EINVAL;
	}
}

static inline int gpio_get_value(unsigned gpio)
{
	if (gpio <= GPIO_LAST) {
		return __gpio_get_value(gpio);
	} else {
		return -EINVAL;
	}
}

static inline void gpio_set_value(unsigned gpio, int value)
{
	if (gpio <= GPIO_LAST) {
		__gpio_set_value(gpio, value);
	}
}

#include <asm-generic/gpio.h>	/* cansleep wrappers */

static inline int gpio_to_irq(unsigned gpio)
{
	if (gpio <= GPIO_LAST) {
		return gpio + INT_GPIO_BASE;
	} else {
		return -EINVAL;
	}
}

static inline int irq_to_gpio(unsigned irq)
{
	if ((irq >= INT_GPIO_BASE) && (irq <= INT_GPIO_LAST)) {
		return irq - INT_GPIO_BASE;
	} else {
		return -EINVAL;
	}
}

static inline int gpio_request(unsigned gpio, const char *label)
{
	if (gpio <= GPIO_LAST) {
		return 0;
	} else {
		return -EINVAL;
	}
}

static void inline gpio_free(unsigned gpio)
{
}

#endif	/* __ASM_ARM_ARCH_NE1_GPIO_H */

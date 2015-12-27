/*
 * linux/arch/arm/mach-ne1/ne1tb.c
 *
 * Copyright (C) NEC Electronics Corporation 2007, 2008
 *
 * This file is based on arch/arm/common/gic.c
 *
 * Copyright (C) 2002 ARM Limited, All Rights Reserved.
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
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>

#include <asm/gpio.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <asm/hardware/gic.h>

#ifdef CONFIG_NKERNEL

#include <nk/nkern.h>

#define NKCTX()		os_ctx

#endif

/*
 * NaviEngine1 GPIO interrupt support
 */

#define MAX_GPIO_NR		1

#if defined(CONFIG_MACH_NE1TB)
static const unsigned int gpio_mode_ne1tb[8] = {
	(((unsigned int)(~0x1a00ffff)) | 0x1a00),			/* SCL */
	(((unsigned int)(~0xffbfffff)) | 0xffbf),			/* SCH */
	0x0200fa00,							/* PCE */
	0x00000000, 0x00000202, 0x34333333, 0x33333303,			/* IM0-IM3 */
	0x00000500							/* INH */
};
#endif

#if defined(CONFIG_MACH_NE1DB)
static const unsigned int gpio_mode_ne1db[8] = {
	(((unsigned int)(~0x1fe0ffff) | 0x1fe0),			/* SCL */
	(((unsigned int)(~0xffbfffff) | 0xffbf),			/* SCH */
	0x0200fa90,							/* PCE */
	0x00000000, 0x00000202, 0x34333333, 0x33333303,			/* IM0-IM3 */
	0x00000500							/* INH */
};
#endif

static inline unsigned int gpio_irq(unsigned int irq)
{
	return (irq - IRQ_GPIO_BASE);
}

#if !defined(CONFIG_NKERNEL)

static void gpio_ack_irq(struct irq_data* d)
{
	u32 mask = 1 << gpio_irq(d->irq);

	writel(mask, (VA_GPIO + GPIO_INT));
}

static void gpio_mask_irq(struct irq_data* d)
{
	u32 mask = 1 << gpio_irq(d->irq);

	writel(mask, (VA_GPIO + GPIO_IND));
}

static void gpio_unmask_irq(struct irq_data* d)
{
	u32 mask = 1 << gpio_irq(d->irq);

	writel(mask, (VA_GPIO + GPIO_INE));
}

static int gpio_set_irq_type(struct irq_data* d, unsigned int type);

static struct irq_chip gpio_chip_ack = {
	.name		= "GIO-edge",
	.irq_ack	= gpio_ack_irq,
	.irq_mask	= gpio_mask_irq,
	.irq_unmask	= gpio_unmask_irq,
#ifdef CONFIG_SMP
	.irq_set_affinity	= NULL,
#endif
	.irq_disable  = gpio_mask_irq,
	.irq_set_type = gpio_set_irq_type,
};

static struct irq_chip gpio_chip = {
	.name		= "GIO-level",
	.irq_ack	= gpio_mask_irq,
	.irq_mask	= gpio_mask_irq,
	.irq_mask_ack	= gpio_mask_irq,
	.irq_unmask	= gpio_unmask_irq,
#ifdef CONFIG_SMP
	.irq_set_affinity	= NULL,
#endif
	.irq_disable  = gpio_mask_irq,
	.irq_set_type = gpio_set_irq_type,
};

#else /* CONFIG_NKERNEL */

extern void __nk_xirq_startup  (struct irq_data* d);
extern void __nk_xirq_shutdown (struct irq_data* d);

static int gpio_set_irq_type(struct irq_data* d, unsigned int type);

    static unsigned int
nk_gpio_startup_irq (struct irq_data* d)
{
    NKCTX()->smp_irq_connect(d->irq);
    __nk_xirq_startup(d);
    return 0;
}

    static void
nk_gpio_shutdown_irq (struct irq_data* d)
{	
    __nk_xirq_shutdown(d);
    NKCTX()->smp_irq_disconnect(d->irq);
}

    static void
nk_gpio_enable_irq (struct irq_data* d)
{
    NKCTX()->smp_irq_unmask(d->irq);
}

    static void
nk_gpio_disable_irq (struct irq_data* d)
{
    NKCTX()->smp_irq_mask(d->irq);
}

    static void
nk_gpio_eoi_irq (struct irq_data* d)
{
	NKCTX()->smp_irq_eoi(d->irq);
}

static struct irq_chip nk_gpio_chip = {
	.name		= "GPIO",
	.irq_startup	= nk_gpio_startup_irq,
	.irq_shutdown	= nk_gpio_shutdown_irq,
	.irq_enable	= nk_gpio_enable_irq,
	.irq_disable	= nk_gpio_disable_irq,
	.irq_eoi	= nk_gpio_eoi_irq,
#ifdef CONFIG_SMP
	.irq_set_affinity	= NULL,
#endif
	.irq_set_type   = gpio_set_irq_type,
};

#endif /* CONFIG_NKERNEL */

static int gpio_set_irq_type(struct irq_data* data, unsigned int type)
{
	unsigned int irq = data->irq;
	unsigned int pin = gpio_irq(irq);
	unsigned int mode = 0, level_flag = 0;
	unsigned int im_addr, im_shift, im;
	unsigned int inh;
	struct irq_desc *desc;

	printk("type GPIO=%d type=%x\n", pin, type);

	if ((irq < INT_GPIO_BASE) || (INT_GPIO_LAST < irq)) {
		return -EINVAL;
	}

	desc = irq_desc + irq;

	switch (type & IRQ_TYPE_SENSE_MASK) {
	case IRQ_TYPE_EDGE_RISING:
		mode = 0;
#ifndef CONFIG_NKERNEL
		data->chip = &gpio_chip_ack;
		desc->handle_irq = handle_edge_irq;
#else
		data->chip = &nk_gpio_chip;
		desc->handle_irq = handle_fasteoi_irq;
#endif
		break;
	case IRQ_TYPE_EDGE_FALLING:
		mode = 1;
#ifndef CONFIG_NKERNEL
		data->chip = &gpio_chip_ack;
		desc->handle_irq = handle_edge_irq;
#else
		data->chip = &nk_gpio_chip;
		desc->handle_irq = handle_fasteoi_irq;
#endif
		break;
	case IRQ_TYPE_EDGE_BOTH:
		mode = 2;
#ifndef CONFIG_NKERNEL
		data->chip = &gpio_chip_ack;
		desc->handle_irq = handle_edge_irq;
#else
		data->chip = &nk_gpio_chip;
		desc->handle_irq = handle_fasteoi_irq;
#endif
		break;
	case IRQ_TYPE_LEVEL_LOW:
		mode = 3;
#ifndef CONFIG_NKERNEL
		data->chip = &gpio_chip;
		desc->handle_irq = handle_level_irq;
#else
		data->chip = &nk_gpio_chip;
		desc->handle_irq = handle_fasteoi_irq;
#endif
		level_flag = 1;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		mode = 4;
#ifndef CONFIG_NKERNEL
		data->chip = &gpio_chip;
		desc->handle_irq = handle_level_irq;
#else
		data->chip = &nk_gpio_chip;
		desc->handle_irq = handle_fasteoi_irq;
#endif
		level_flag = 1;
		break;
	default:
		return -EINVAL;
	}

	im_addr = (unsigned int)VA_GPIO + GPIO_IM0;
	im_addr += (pin / 8) << 2;
	im_shift = (pin & 0x7) << 2;

	im = readl(im_addr);
	im &= ~(0xfU << im_shift);
	im |= (mode << im_shift);
#ifndef CONFIG_NKERNEL
	writel(im, im_addr);
#else
        if (im != readl(im_addr)) {
		printk(KERN_WARNING "gpio: changing interrupt sensitivity\n");
	}
#endif

	inh = readl(VA_GPIO + GPIO_INH);

	if (level_flag == 1) {
		inh &= ~(1U << pin);
	} else {
		inh |= (1U << pin);
	}

#ifndef CONFIG_NKERNEL
	writel(inh, VA_GPIO + GPIO_INH);
#else
        if (inh != readl(VA_GPIO + GPIO_INH)) {
		printk(KERN_WARNING "gpio: changing interrupt sensitivity\n");
	}
#endif

	return 0;
}

    void __init
gpio_init (unsigned int gpio_nr, void __iomem *base, unsigned int irq_offset)
{
	unsigned int max_irq, i, gpio_irq, mode, inh=0;
	const unsigned int *gpio_mode;

	if (gpio_nr >= MAX_GPIO_NR)
		BUG();

	/*
	 * initialize GPIO
	 */
	gpio_mode = gpio_mode_ne1tb;

#ifndef CONFIG_NKERNEL
	writel(0xffffffff, (base + GPIO_PCD));
	writel(0xffffffff, (base + GPIO_IPOLRST));
	writel(0xffffffff, (base + GPIO_OPOLRST));
	writel(gpio_mode[0], (base + GPIO_SCL));
	writel(gpio_mode[1], (base + GPIO_SCH));
	writel(gpio_mode[2], (base + GPIO_PCE));

	writel(0xffffffff, (base + GPIO_IND));
	writel(gpio_mode[3], (base + GPIO_IM0));
	writel(gpio_mode[4], (base + GPIO_IM1));
	writel(gpio_mode[5], (base + GPIO_IM2));
	writel(gpio_mode[6], (base + GPIO_IM3));
	writel(gpio_mode[7], (base + GPIO_INH));

	writel(0xffffffff, (base + GPIO_IPOLRST));
	writel(0x00000000, (base + GPIO_IPOLINV));

	writel(0xffffffff, (base + GPIO_INT));
#endif

	/*
	 * setup GPIO IRQ
	 */
	max_irq = 32;

	for (i = irq_offset, gpio_irq = 0; i < irq_offset + max_irq; i++, gpio_irq++) {
		mode = gpio_mode[3 + (gpio_irq / 8)];
		mode = (mode >> (4 * (gpio_irq & 0x7))) & 0xf;

		switch (mode) {
		case 0:
		case 1:
		case 2:
#ifndef CONFIG_NKERNEL
			irq_set_chip(i, &gpio_chip_ack);
#else
		        irq_set_chip(i, &nk_gpio_chip);
#endif
			irq_set_handler(i, handle_edge_irq);
			inh |= (1U << gpio_irq);
			break;
		default:
#ifndef CONFIG_NKERNEL
			irq_set_chip(i, &gpio_chip);
			irq_set_handler(i, handle_level_irq);
#else
		        irq_set_chip(i, &nk_gpio_chip);
			irq_set_handler(i, handle_fasteoi_irq);
#endif
			break;
		}
		set_irq_flags(i, IRQF_VALID | IRQF_PROBE);
	}

#ifndef CONFIG_NKERNEL
	writel(inh, base + GPIO_INH);
#endif
}

#if !defined(CONFIG_NKERNEL)

static void gpio_handle_cascade_irq(unsigned int irq, struct irq_desc *desc)
{
	struct irq_data* data = irq_get_irq_data(irq);
	struct irq_chip *chip;
	unsigned int cascade_irq, gpio_irq, mask;
	unsigned long status;

	if (!data)
		BUG();

	chip = irq_data_get_irq_chip(data);
	/* GIC: disable (mask) and send EOI */
	chip->irq_ack(data);

	status = readl(VA_GPIO + GPIO_INT);
	status &= readl(VA_GPIO + GPIO_INE);

	for (gpio_irq = 0, mask = 1; gpio_irq < 32; gpio_irq++, mask <<= 1) {
		if (status & mask) {
			cascade_irq = gpio_irq + IRQ_GPIO_BASE;
			generic_handle_irq(cascade_irq);
		}
	}

	/* GIC: enable (unmask) */
	chip->irq_unmask(data);
}

void __init gpio_cascade_irq(unsigned int gpio_nr, unsigned int irq)
{
	if (gpio_nr >= MAX_GPIO_NR)
		BUG();
	irq_set_chained_handler(irq, gpio_handle_cascade_irq);
}

#endif /* CONFIG_NKERNEL */


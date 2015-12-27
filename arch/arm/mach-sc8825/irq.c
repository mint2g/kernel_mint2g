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
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <asm/hardware/gic.h>

#include <mach/hardware.h>
#include <mach/irqs.h>

/* general interrupt registers */
#define	INTC0_REG(off)		(SPRD_INTC0_BASE + (off))
#define	INTCV0_IRQ_MSKSTS	INTC0_REG(0x0000)
#define	INTCV0_IRQ_RAW		INTC0_REG(0x0004)
#define	INTCV0_IRQ_EN		INTC0_REG(0x0008)
#define	INTCV0_IRQ_DIS		INTC0_REG(0x000C)
#define	INTCV0_IRQ_SOFT		INTC0_REG(0x0010)

#define	INTC1_REG(off)		(SPRD_INTC0_BASE + 0x1000 + (off))
#define	INTCV1_IRQ_MSKSTS	INTC1_REG(0x0000)
#define	INTCV1_IRQ_RAW		INTC1_REG(0x0004)
#define	INTCV1_IRQ_EN		INTC1_REG(0x0008)
#define	INTCV1_IRQ_DIS		INTC1_REG(0x000C)
#define	INTCV1_IRQ_SOFT		INTC1_REG(0x0010)

#define INTC1_IRQ_NUM_MIN	(32)
#define INTC_NUM_MAX		(61)

static void sci_irq_eoi(struct irq_data *data)
{
	/* nothing to do... */
}

#ifdef CONFIG_PM
static int sci_set_wake(struct irq_data *d, unsigned int on)
{
	return 0;
}

#else
#define sci_set_wake	NULL
#endif

static void sci_irq_mask(struct irq_data *data)
{
	unsigned int irq = SCI_GET_INTC_IRQ(data->irq);
	if (irq <= INTC_NUM_MAX) {
		if (irq >= INTC1_IRQ_NUM_MIN) {
			__raw_writel(1 << (irq - INTC1_IRQ_NUM_MIN),
				     INTCV1_IRQ_DIS);
		} else {
			__raw_writel(1 << irq, INTCV0_IRQ_DIS);
		}
	}
}

static void sci_irq_unmask(struct irq_data *data)
{
	unsigned int irq = SCI_GET_INTC_IRQ(data->irq);
	if (irq <= INTC_NUM_MAX) {
		if (irq >= INTC1_IRQ_NUM_MIN) {
			__raw_writel(1 << (irq - INTC1_IRQ_NUM_MIN),
				     INTCV1_IRQ_EN);
		} else {
			__raw_writel(1 << irq, INTCV0_IRQ_EN);
		}
	}
}

void __init sc8825_init_irq(void)
{
#ifdef CONFIG_NKERNEL
	unsigned int val;
	extern void nk_ddi_init(void);
	nk_ddi_init();
#endif
	gic_init(0, 29, (void __iomem *)SC8825_VA_GIC_DIS,
		 (void __iomem *)SC8825_VA_GIC_CPU);
	gic_arch_extn.irq_eoi = sci_irq_eoi;
	gic_arch_extn.irq_mask = sci_irq_mask;
	gic_arch_extn.irq_unmask = sci_irq_unmask;
	gic_arch_extn.irq_set_wake = sci_set_wake;
	ana_init_irq();
#ifdef CONFIG_NKERNEL
	/*
	 *  gic clock will be stopped after 2 cores enter standby in the same time,
	 * dsp assert if IRQ_DSP0_INT and IRQ_DSP1_INT are disabled. so enable IRQ_DSP0_INT
	 * and IRQ_DSP1_INT in INTC0 here.
	 */
	val = __raw_readl(INTCV0_IRQ_EN);
	val |= (SCI_INTC_IRQ_BIT(IRQ_DSP0_INT) | SCI_INTC_IRQ_BIT(IRQ_DSP1_INT) | SCI_INTC_IRQ_BIT(IRQ_EPT_INT));
	val |= (SCI_INTC_IRQ_BIT(IRQ_SIM0_INT) | SCI_INTC_IRQ_BIT(IRQ_SIM1_INT));
	val |= (SCI_INTC_IRQ_BIT(IRQ_TIMER0_INT));
	__raw_writel(val, INTCV0_IRQ_EN);
	/*disable legacy interrupt*/
	__raw_writel(1<<31, SC8825_VA_GIC_DIS + 0x180);
	__raw_writel(1<<28, SC8825_VA_GIC_DIS + 0x180);
#endif

}


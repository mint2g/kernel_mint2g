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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/irqflags.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <asm/delay.h>

#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <mach/adi.h>
#include <mach/irqs.h>

#ifdef CONFIG_NKERNEL
#include <nk/nkern.h>
#define CONFIG_NKERNEL_NO_SHARED_IRQ
#endif

#define CTL_GLB_BASE			( SPRD_GREG_BASE )
#define CTL_ADI_BASE			( SPRD_MISC_BASE )

/* registers definitions for controller CTL_ADI */
#define REG_ADI_CTRL0                   (CTL_ADI_BASE + 0x0004)
#define REG_ADI_CHNL_PRI                (CTL_ADI_BASE + 0x0008)
#define REG_ADI_RD_CMD                  (CTL_ADI_BASE + 0x0024)
#define REG_ADI_RD_DATA                 (CTL_ADI_BASE + 0x0028)
#define REG_ADI_FIFO_STS                (CTL_ADI_BASE + 0x002c)

/* bits definitions for register REG_ADI_CTRL0 */
#define BIT_ARM_SCLK_EN                 ( BIT(1) )

/* bits definitions for register REG_ADI_CHNL_PRI */
#define BITS_PD_WR_PRI(_x_)             ( (_x_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_RFT_WR_PRI(_x_)            ( (_x_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_DSP_RD_PRI(_x_)            ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_DSP_WR_PRI(_x_)            ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ARM_RD_PRI(_x_)            ( (_x_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_ARM_WR_PRI(_x_)            ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_STC_WR_PRI(_x_)            ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_INT_STEAL_PRI(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ADI_RD_CMD */
#define BIT_RD_CMD_BUSY                 ( BIT(31) )

/* bits definitions for register REG_ADI_RD_DATA */
#define SHIFT_RD_ADDR                   ( 16 )
#define MASK_RD_ADDR                    ( 0x7FF )
#define	VALU_TO_ADDR(_x_)		( ((_x_) >> SHIFT_RD_ADDR) & MASK_RD_ADDR )

#define SHIFT_RD_VALU                   ( 0 )
#define MASK_RD_VALU                    ( 0xFFFF )

/* bits definitions for register REG_ADI_FIFO_STS */
#define BIT_FIFO_FULL                   ( BIT(11) )
#define BIT_FIFO_EMPTY                  ( BIT(10) )

/* vars definitions for controller CTL_ADI */
#define ADI_CHNL_PRI_LOWEST             ( 0 )
#define ADI_CHNL_PRI_LOWER              ( 1 )
#define ADI_CHNL_PRI_HIGHER             ( 2 )
#define ADI_CHNL_PRI_HIGHEST            ( 3 )

#ifdef CONFIG_NKERNEL
#define sci_adi_lock()				\
		unsigned long flags;		\
		flags = hw_local_irq_save()
#define sci_adi_unlock()			\
		hw_local_irq_restore(flags)

#else
#define sci_adi_lock()				\
		unsigned long flags;		\
		local_irq_save(flags)
#define sci_adi_unlock()			\
		local_irq_restore(flags)

#endif

static int sci_adi_channel_priority(void)
{
	uint32_t value;

	value = __raw_readl(REG_ADI_CTRL0);
	value &= ~BIT_ARM_SCLK_EN;
	__raw_writel(value, REG_ADI_CTRL0);

	value = __raw_readl(REG_ADI_CHNL_PRI);
	value |= BITS_PD_WR_PRI(1) | BITS_RFT_WR_PRI(1) |
	    BITS_DSP_RD_PRI(0) | BITS_DSP_WR_PRI(0) |
	    BITS_ARM_RD_PRI(0) | BITS_ARM_WR_PRI(0) |
	    BITS_STC_WR_PRI(1) | BITS_INT_STEAL_PRI(0);
	__raw_writel(value, REG_ADI_CHNL_PRI);

	/* TODO: How about CMMB_WR_PRI? */

	return 0;
}

static int sci_adi_ready(void)
{
	int cnt = 1000;
	while (!(__raw_readl(REG_ADI_FIFO_STS) & BIT_FIFO_EMPTY) && cnt--) {
		udelay(1);
	}

	WARN(cnt == 0, "ADI WAIT timeout!!!");

	return 0;
}

#define ANA_VIRT_BASE			( SPRD_MISC_BASE )
#define ANA_PHYS_BASE			( SPRD_MISC_PHYS )

int sci_adi_read(u32 reg)
{
	unsigned long val;
	int cnt = 1000;

	sci_adi_lock();
	sci_adi_ready();

	/* send physical address */
	reg = reg - ANA_VIRT_BASE + ANA_PHYS_BASE;
	__raw_writel(reg, REG_ADI_RD_CMD);

	/*
	 * wait read operation complete, RD_data[31] will be
	 * cleared after the read operation complete
	 */
	do {
		val = __raw_readl(REG_ADI_RD_DATA);
	} while ((val & BIT_RD_CMD_BUSY) && cnt--);

	sci_adi_unlock();

	WARN(cnt == 0, "ADI READ timeout!!!");

	/* val high part should be the address of the last read operation */
	BUG_ON(VALU_TO_ADDR(val) != (reg & MASK_RD_ADDR));

	return (val & MASK_RD_VALU);
}

int sci_adi_raw_write(u32 reg, u16 val)
{
	sci_adi_lock();
	sci_adi_ready();

	__raw_writel(val, reg);

	sci_adi_unlock();

	return 0;
}

int sci_adi_write(u32 reg, u16 val, u16 msk)
{
	sci_adi_lock();

	sci_adi_raw_write(reg, (sci_adi_read(reg) & ~msk) | val);

	sci_adi_unlock();
	return 0;
}

int sci_adi_set(u32 reg, u16 bits)
{
	sci_adi_lock();

	sci_adi_raw_write(reg, (u16) sci_adi_read(reg) | bits);

	sci_adi_unlock();

	return 0;
}

int sci_adi_clr(u32 reg, u16 bits)
{
	sci_adi_lock();

	sci_adi_raw_write(reg, (u16) sci_adi_read(reg) & ~bits);

	sci_adi_unlock();
	return 0;
}

EXPORT_SYMBOL(sci_adi_read);
EXPORT_SYMBOL(sci_adi_raw_write);
EXPORT_SYMBOL(sci_adi_write);
EXPORT_SYMBOL(sci_adi_set);
EXPORT_SYMBOL(sci_adi_clr);


#ifndef CONFIG_NKERNEL

/* Analog Die interrupt registers */
#define ANA_CTL_INT_BASE		( SPRD_MISC_BASE + 0x380 )

/* registers definitions for controller ANA_CTL_INT */
#define ANA_REG_INT_MASK_STATUS         (ANA_CTL_INT_BASE + 0x0000)
#define ANA_REG_INT_RAW_STATUS          (ANA_CTL_INT_BASE + 0x0004)
#define ANA_REG_INT_EN                  (ANA_CTL_INT_BASE + 0x0008)
#define ANA_REG_INT_MASK_STATUS_SYNC    (ANA_CTL_INT_BASE + 0x000c)

/* bits definitions for register REG_INT_MASK_STATUS */
#define BIT_ANA_CHGRWDG_INT             ( BIT(6) )
#define BIT_ANA_EIC_INT                 ( BIT(5) )
#define BIT_ANA_TPC_INT                 ( BIT(4) )
#define BIT_ANA_WDG_INT                 ( BIT(3) )
#define BIT_ANA_RTC_INT                 ( BIT(2) )
#define BIT_ANA_GPIO_INT                ( BIT(1) )
#define BIT_ANA_ADC_INT                 ( BIT(0) )

/* vars definitions for controller ANA_CTL_INT */
#define MASK_ANA_INT                    ( 0x7F )

void sprd_ack_ana_irq(struct irq_data *data)
{
	/* nothing to do... */
}

static void sprd_mask_ana_irq(struct irq_data *data)
{
	int offset = data->irq - IRQ_ANA_INT_START;
	pr_debug("%s %d\n", __FUNCTION__, data->irq);
	sci_adi_clr(ANA_REG_INT_EN, BIT(offset) & MASK_ANA_INT);
}

static void sprd_unmask_ana_irq(struct irq_data *data)
{
	int offset = data->irq - IRQ_ANA_INT_START;
	pr_debug("%s %d\n", __FUNCTION__, data->irq);
	sci_adi_set(ANA_REG_INT_EN, BIT(offset) & MASK_ANA_INT);
}

static struct irq_chip sprd_muxed_ana_chip = {
	.name		= "irq-ANA",
	.irq_ack	= sprd_ack_ana_irq,
	.irq_mask	= sprd_mask_ana_irq,
	.irq_unmask	= sprd_unmask_ana_irq,
};

static void sprd_muxed_ana_handler(unsigned int irq, struct irq_desc *desc)
{
	uint32_t irq_ana, status;
	int i;

	status = sci_adi_read(ANA_REG_INT_MASK_STATUS) & MASK_ANA_INT;
	pr_debug("%s %d 0x%08x\n", __FUNCTION__, irq, status);
	while (status) {
		i = __ffs(status);
		status &= ~(1 << i);
		irq_ana = IRQ_ANA_INT_START + i;
		pr_debug("%s generic_handle_irq %d\n",
			__FUNCTION__, irq_ana);
		generic_handle_irq(irq_ana);
	}
}

static void __init ana_init_irq(void)
{
	int n;

	irq_set_chained_handler(IRQ_ANA_INT, sprd_muxed_ana_handler);
	for (n = IRQ_ANA_INT_START; n < IRQ_ANA_INT_START + NR_ANA_IRQS; n++) {
		irq_set_chip_and_handler(n, &sprd_muxed_ana_chip,
					 handle_level_irq);
		set_irq_flags(n, IRQF_VALID);
	}
}

#else /* CONFIG_NKERNEL */

extern NkDevXPic*   nkxpic;		/* virtual XPIC device */
extern NkOsId       nkxpic_owner;	/* owner of the virtual XPIC device */
extern NkOsMask     nkosmask;		/* my OS mask */

extern void __nk_xirq_startup  (struct irq_data* d);
extern void __nk_xirq_shutdown (struct irq_data* d);

static unsigned int nk_startup_irq (struct irq_data *data)
{
	__nk_xirq_startup(data);
#ifdef CONFIG_NKERNEL_NO_SHARED_IRQ
	nkxpic->irq[data->irq].os_enabled  = nkosmask;
#else
	nkxpic->irq[data->irq].os_enabled |= nkosmask;
#endif
	nkops.nk_xirq_trigger(nkxpic->xirq, nkxpic_owner);

	return 0;
}

static void nk_shutdown_irq (struct irq_data *data)
{
	__nk_xirq_shutdown(data);
#ifdef CONFIG_NKERNEL_NO_SHARED_IRQ
	nkxpic->irq[data->irq].os_enabled  = 0;
#else
	nkxpic->irq[irq].os_enabled &= ~nkosmask;
#endif
	nkops.nk_xirq_trigger(nkxpic->xirq, nkxpic_owner);
}

static void nk_sprd_ack_ana_irq(struct irq_data *data)
{
	/* nothing to do... */
}

static void nk_sprd_mask_ana_irq(struct irq_data *data)
{
	/* nothing to do... */
}

static void nk_sprd_unmask_ana_irq(struct irq_data *data)
{
	nkops.nk_xirq_trigger(data->irq, nkxpic_owner);
}

static struct irq_chip nk_sprd_muxed_ana_chip = {
	.name		= "irq-ANA",
	.irq_ack	= nk_sprd_ack_ana_irq,
	.irq_mask	= nk_sprd_mask_ana_irq,
	.irq_unmask	= nk_sprd_unmask_ana_irq,
	.irq_startup	= nk_startup_irq,
	.irq_shutdown	= nk_shutdown_irq,
};

static void __init ana_init_irq(void)
{
	int n;

	for (n = IRQ_ANA_ADC_INT; n < IRQ_ANA_ADC_INT+ NR_ANA_IRQS; ++n) {
		irq_set_chip_and_handler(n, &nk_sprd_muxed_ana_chip,
				handle_level_irq);
		set_irq_flags(n, IRQF_VALID);
	}
}

#endif /* CONFIG_NKERNEL */

extern void sci_adc_enable(void);

static int __init adi_init(void)
{
	/* enable adi in global regs */
	sprd_greg_set_bits(REG_TYPE_GLOBAL, GEN0_ADI_EN, GR_GEN0);

	/* reset adi */
	sprd_greg_set_bits(REG_TYPE_GLOBAL, SWRST_ADI_RST, GR_SOFT_RST);
	udelay(2);
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, SWRST_ADI_RST, GR_SOFT_RST);

	/* set channel priority */
	sci_adi_channel_priority();

	/* init analogue die irq */
	ana_init_irq();

	/* enable adc */
	sci_adc_enable();

	return 0;
}

arch_initcall(adi_init);

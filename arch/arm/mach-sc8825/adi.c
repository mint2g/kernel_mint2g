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
#include <linux/hwspinlock.h>
#include <asm/delay.h>

#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <mach/adi.h>
#include <mach/irqs.h>
#include <mach/sci.h>
#include <mach/regs_glb.h>
#include <mach/arch_lock.h>

#define CTL_ADI_BASE			( SPRD_MISC_BASE )

/* registers definitions for controller CTL_ADI */
#define REG_ADI_CLKDIV					(CTL_ADI_BASE + 0x00)
#define REG_ADI_CTRL0					(CTL_ADI_BASE + 0x04)
#define REG_ADI_CHNL_PRI				(CTL_ADI_BASE + 0x08)
#define REG_ADI_INT_RAW					(CTL_ADI_BASE + 0x10)
#define REG_ADI_RD_CMD					(CTL_ADI_BASE + 0x24)
#define REG_ADI_RD_DATA					(CTL_ADI_BASE + 0x28)
#define REG_ADI_FIFO_STS				(CTL_ADI_BASE + 0x2c)

/* bits definitions for register REG_ADI_CTRL0 */
#define BIT_ARM_SCLK_EN                 ( BIT(1) )
#define BITS_CMMB_WR_PRI			( (1) << 4 & (BIT(4)|BIT(5)) )

/* bits definitions for register REG_ADI_CHNL_PRI */
#define BITS_PD_WR_PRI             ( (1) << 14 & (BIT(14)|BIT(15)) )
#define BITS_RFT_WR_PRI       	   ( (1) << 12 & (BIT(12)|BIT(13)) )
#define BITS_DSP_RD_PRI            ( (2) << 10 & (BIT(10)|BIT(11)) )
#define BITS_DSP_WR_PRI            ( (2) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ARM_RD_PRI            ( (3) << 6 & (BIT(6)|BIT(7)) )
#define BITS_ARM_WR_PRI            ( (3) << 4 & (BIT(4)|BIT(5)) )
#define BITS_STC_WR_PRI            ( (1) << 2 & (BIT(2)|BIT(3)) )
#define BITS_INT_STEAL_PRI         ( (3) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ADI_RD_CMD */
#define BIT_RD_CMD_BUSY                 ( BIT(31) )

/* bits definitions for register REG_ADI_RD_DATA */
#define SHIFT_RD_ADDR                   ( 16 )
#define MASK_RD_ADDR                    ( 0x7FF )
#define	TO_ADDR(_x_)		( ((_x_) >> SHIFT_RD_ADDR) & MASK_RD_ADDR )

#define SHIFT_RD_VALU                   ( 0 )
#define MASK_RD_VALU                    ( 0xFFFF )

/* bits definitions for register REG_ADI_FIFO_STS */
#define BIT_FIFO_FULL                   ( BIT(11) )
#define FIFO_IS_FULL()	(__raw_readl(REG_ADI_FIFO_STS) & BIT_FIFO_FULL)

#define BIT_FIFO_EMPTY                  ( BIT(10) )

#ifdef CONFIG_NKERNEL
static DEFINE_SPINLOCK(adi_lock);
static void sci_adi_lock(unsigned long *flags, unsigned long *hw_flags)
{
	spin_lock_irqsave(&adi_lock, *flags);
	*hw_flags = hw_local_irq_save();
	if (arch_get_hwlock(HWLOCK_ADI))
		WARN_ON(IS_ERR_VALUE(hwspin_lock_timeout(arch_get_hwlock(HWLOCK_ADI), -1)));
	else
		arch_hwlock_fast(HWLOCK_ADI);
}
static void sci_adi_unlock(unsigned long *flags, unsigned long *hw_flags)
{
	if (arch_get_hwlock(HWLOCK_ADI))
		hwspin_unlock(arch_get_hwlock(HWLOCK_ADI));
	else
		arch_hwunlock_fast(HWLOCK_ADI);
	hw_local_irq_restore(*hw_flags);
	spin_unlock_irqrestore(&adi_lock, *flags);
}
#else
/*FIXME:If we have not hwspinlock , we need use spinlock to do it*/
static void sci_adi_lock(unsigned long *flags, unsigned long *hw_flags)
{
	if (arch_get_hwlock(HWLOCK_ADI))
		WARN_ON(IS_ERR_VALUE(hwspin_lock_timeout_irqsave(arch_get_hwlock(HWLOCK_ADI), -1, flags)));
	else
		arch_hwlock_fast(HWLOCK_ADI);
}
static void sci_adi_unlock(unsigned long  *flags, unsigned long  *hw_flags)
{
	if (arch_get_hwlock(HWLOCK_ADI))
		hwspin_unlock_irqrestore(arch_get_hwlock(HWLOCK_ADI), flags);
	else
		arch_hwunlock_fast(HWLOCK_ADI);
}
#endif

static int sci_adi_fifo_drain(void)
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
#define ANA_ADDR_SIZE			(SZ_4K)
#define ADDR_VERIFY(_X_)	do { \
	BUG_ON((_X_) < ANA_VIRT_BASE || (_X_) > (ANA_VIRT_BASE + ANA_ADDR_SIZE));} while (0)
static u32 __sci_adi_translate_addr(u32 regvddr)
{
	regvddr = regvddr - ANA_VIRT_BASE + ANA_PHYS_BASE;
	return regvddr;
}

static int __sci_adi_read(u32 regPddr)
{
	unsigned long val;
	int cnt = 2000;

	while (FIFO_IS_FULL() && cnt--) {
		udelay(1);
	}
	WARN(cnt == 0, "ADI WAIT timeout!!!");
	__raw_writel(regPddr, REG_ADI_RD_CMD);

	/*
	 * wait read operation complete, RD_data[31] will be
	 * cleared after the read operation complete
	 */
	do {
		val = __raw_readl(REG_ADI_RD_DATA);
	} while ((val & BIT_RD_CMD_BUSY) && cnt--);

	WARN(cnt == 0, "ADI READ timeout!!!");
	/* val high part should be the address of the last read operation */
	BUG_ON(TO_ADDR(val) != (regPddr & MASK_RD_ADDR));

	return (val & MASK_RD_VALU);
}

int sci_adi_read(u32 reg)
{
	unsigned long val;
	unsigned long flags, hw_flags;
	ADDR_VERIFY(reg);
	reg = __sci_adi_translate_addr(reg);
	sci_adi_lock(&flags, &hw_flags);
	val = __sci_adi_read(reg);
	sci_adi_unlock(&flags, &hw_flags);
	return val;
}

EXPORT_SYMBOL(sci_adi_read);

#define CACHE_SIZE	(16)
struct sci_data {
	u32 reg;
	u16 val;
} sci_data_array[CACHE_SIZE];
struct sci_data *head_p = &sci_data_array[0];
struct sci_data *tail_p = &sci_data_array[0];
static u32 data_in_cache = 0;
#define HEAD_ADD	(1)
#define TAIL_ADD	(0)
static void __p_add(struct sci_data **p, u32 isHead)
{
	if (++(*p) > &sci_data_array[CACHE_SIZE - 1])
		(*p) = &sci_data_array[0];
	if (head_p == tail_p) {
		if (isHead == HEAD_ADD) {
			data_in_cache = 0;
		} else {
			data_in_cache = CACHE_SIZE;
		}
	} else {
		data_in_cache = 2;
	}
}

static int __sci_adi_write(u32 reg, u16 val, u32 sync)
{
	tail_p->reg = reg;
	tail_p->val = val;
	__p_add(&tail_p, TAIL_ADD);
	while (!FIFO_IS_FULL() && (data_in_cache != 0)) {
		__raw_writel(head_p->val, head_p->reg);
		__p_add(&head_p, HEAD_ADD);
	}

	if (sync || data_in_cache == CACHE_SIZE) {
		sci_adi_fifo_drain();
		while (data_in_cache != 0) {
			while(FIFO_IS_FULL()){
				cpu_relax();
			}
			__raw_writel(head_p->val, head_p->reg);
			__p_add(&head_p, HEAD_ADD);
		}
		sci_adi_fifo_drain();
	}

	return 0;
}

int sci_adi_write_fast(u32 reg, u16 val, u32 sync)
{
	unsigned long flags, hw_flags;
	ADDR_VERIFY(reg);
	sci_adi_lock(&flags, &hw_flags);
	__sci_adi_write(reg, val, sync);
	sci_adi_unlock(&flags, &hw_flags);
	return 0;
}

EXPORT_SYMBOL(sci_adi_write_fast);

int sci_adi_write(u32 reg, u16 or_val, u16 clear_msk)
{
	unsigned long flags, hw_flags;
	
	ADDR_VERIFY(reg);
	sci_adi_lock(&flags, &hw_flags);
	__sci_adi_write(reg,
			(__sci_adi_read(__sci_adi_translate_addr(reg)) &
			 ~clear_msk) | or_val, 1);
	sci_adi_unlock(&flags, &hw_flags);
	return 0;
}

EXPORT_SYMBOL(sci_adi_write);

static void __init __sci_adi_init(void)
{
	uint32_t value;

	value = __raw_readl(REG_ADI_CTRL0);
	value &= ~BIT_ARM_SCLK_EN;
	value |= BITS_CMMB_WR_PRI;
	__raw_writel(value, REG_ADI_CTRL0);

	value = __raw_readl(REG_ADI_CHNL_PRI);
	value |= BITS_PD_WR_PRI | BITS_RFT_WR_PRI |
	    BITS_DSP_RD_PRI | BITS_DSP_WR_PRI |
	    BITS_ARM_RD_PRI | BITS_ARM_WR_PRI |
	    BITS_STC_WR_PRI | BITS_INT_STEAL_PRI;
	__raw_writel(value, REG_ADI_CHNL_PRI);
}

int __init sci_adi_init(void)
{
	/* enable adi in global regs */
	sci_glb_set(REG_GLB_GEN0,BIT_ADI_EB);

	/* reset adi */
	sci_glb_set(REG_GLB_SOFT_RST,BIT_ADI_RST);
	udelay(2);
	sci_glb_clr(REG_GLB_SOFT_RST,BIT_ADI_RST);

	__sci_adi_init();

	return 0;
}


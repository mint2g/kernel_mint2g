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

#include <asm/io.h>
#include <mach/globalregs.h>
#include <linux/spinlock.h>
#include <linux/module.h>

static uint32_t globalregs[] = {
	SPRD_GREG_BASE,
	SPRD_AHB_BASE + 0x200,
};

static spinlock_t lock = __SPIN_LOCK_UNLOCKED(lock);

int32_t sprd_greg_read(uint32_t type, uint32_t reg_offset)
{
	return __raw_readl(globalregs[type] + reg_offset);
}

void sprd_greg_write(uint32_t type, uint32_t value, uint32_t reg_offset)
{
	__raw_writel(value, globalregs[type] + reg_offset);
}

void sprd_greg_set_bits(uint32_t type, uint32_t bits, uint32_t reg_offset)
{
	int value;
	unsigned long flags;

#ifndef CONFIG_NKERNEL
	spin_lock_irqsave(&lock, flags);
#else
	flags = hw_local_irq_save();
#endif
	value = __raw_readl(globalregs[type] + reg_offset);
	value |= bits;
	__raw_writel(value, globalregs[type] + reg_offset);
#ifndef CONFIG_NKERNEL
	spin_unlock_irqrestore(&lock, flags);
#else
	hw_local_irq_restore(flags);
#endif
}

void sprd_greg_clear_bits(uint32_t type, uint32_t bits, uint32_t reg_offset)
{
	int value;
	unsigned long flags;

#ifndef CONFIG_NKERNEL
	spin_lock_irqsave(&lock, flags);
#else
	flags = hw_local_irq_save();
#endif
	value = __raw_readl(globalregs[type] + reg_offset);
	value &= ~bits;
	__raw_writel(value, globalregs[type] + reg_offset);
#ifndef CONFIG_NKERNEL
	spin_unlock_irqrestore(&lock, flags);
#else
	hw_local_irq_restore(flags);
#endif
}

EXPORT_SYMBOL(sprd_greg_read);
EXPORT_SYMBOL(sprd_greg_write);
EXPORT_SYMBOL(sprd_greg_set_bits);
EXPORT_SYMBOL(sprd_greg_clear_bits);

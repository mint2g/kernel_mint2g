/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/hwspinlock.h>
#include <linux/io.h>

#include <mach/sci.h>
#include <mach/hardware.h>
#include <mach/regs_glb.h>
#include <mach/arch_lock.h>

#ifdef CONFIG_NKERNEL
static DEFINE_SPINLOCK(glb_lock);
static void sci_glb_lock(unsigned long *flags, unsigned long *hw_flags)
{
	spin_lock_irqsave(&glb_lock, *flags);
	*hw_flags = hw_local_irq_save();
	if (arch_get_hwlock(HWLOCK_GLB))
		WARN_ON(IS_ERR_VALUE(hwspin_lock_timeout(arch_get_hwlock(HWLOCK_GLB), -1)));
	else
		arch_hwlock_fast(HWLOCK_GLB);
}

static void sci_glb_unlock(unsigned long *flags, unsigned long *hw_flags)
{
	if (arch_get_hwlock(HWLOCK_GLB))
		hwspin_unlock(arch_get_hwlock(HWLOCK_GLB));
	else
		arch_hwunlock_fast(HWLOCK_GLB);
	hw_local_irq_restore(*hw_flags);
	spin_unlock_irqrestore(&glb_lock, *flags);
}
#else
/*FIXME:If we have not hwspinlock , we need use spinlock to do it*/
static void sci_glb_lock(unsigned long *flags, unsigned long *hw_flags)
{
	if (arch_get_hwlock(HWLOCK_GLB))
		WARN_ON(IS_ERR_VALUE(hwspin_lock_timeout_irqsave(arch_get_hwlock(HWLOCK_GLB), -1, flags)));
	else
		arch_hwlock_fast(HWLOCK_GLB);
}

static void sci_glb_unlock(unsigned long *flags, unsigned long *hw_flags)
{
	if (arch_get_hwlock(HWLOCK_GLB))
		hwspin_unlock_irqrestore(arch_get_hwlock(HWLOCK_GLB), flags);
	else
		arch_hwunlock_fast(HWLOCK_GLB);
}
#endif

u32 sci_glb_read(u32 reg, u32 msk)
{
	return __raw_readl(reg) & msk;
}

int sci_glb_write(u32 reg, u32 val, u32 msk)
{
	unsigned long flags, hw_flags;
	sci_glb_lock(&flags, &hw_flags);
	__raw_writel((__raw_readl(reg) & ~msk) | val, reg);
	sci_glb_unlock(&flags, &hw_flags);
	return 0;
}

static int __is_glb(u32 reg)
{
	return rounddown(reg, SZ_64K) == rounddown(REGS_GLB_BASE, SZ_64K) ||
	    rounddown(reg, SZ_64K) == rounddown(REGS_AHB_BASE, SZ_64K);
}

int sci_glb_set(u32 reg, u32 bit)
{
	if (__is_glb(reg))
		__raw_writel(bit, REG_GLB_SET(reg));
	else
		WARN_ON(1);
	return 0;
}

int sci_glb_clr(u32 reg, u32 bit)
{
	if (__is_glb(reg))
		__raw_writel(bit, REG_GLB_CLR(reg));
	else
		WARN_ON(1);
	return 0;
}

EXPORT_SYMBOL(sci_glb_read);
EXPORT_SYMBOL(sci_glb_write);
EXPORT_SYMBOL(sci_glb_set);
EXPORT_SYMBOL(sci_glb_clr);


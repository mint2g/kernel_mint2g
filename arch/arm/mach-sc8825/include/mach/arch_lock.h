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

#ifndef __ARCH_LOCK_H
#define __ARCH_LOCK_H

#include <linux/hwspinlock.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <mach/regs_ahb.h>
#include <mach/sci.h>

#define SHIFT_ID(_X_) (1<<(_X_))

#define	arch_get_hwlock(_ID_)	(hwlocks[_ID_])
extern struct hwspinlock *hwlocks[];

//Configs lock id
#define HWSPINLOCK_WRITE_KEY	(0x1)	/*processor specific write lock id */
#define HWLOCK_ADDR(_X_)	(SPRD_HWLOCK_BASE + (0x80 + 0x4*(_X_)))
#define HWSPINLOCK_NOTTAKEN		(0x524c534c)	/*free: RLSL */

#define HWSPINLOCK_ID_TOTAL_NUMS	(32)
#define HWLOCK_ADI	(0)
#define HWLOCK_GLB	(1)
#define HWLOCK_AGPIO	(2)
#define HWLOCK_AEIC	(3)
#define HWLOCK_ADC	(4)
#define HWLOCK_CACHE	(5)

static inline int arch_hwlocks_implemented(void)
{
	return (SHIFT_ID(HWLOCK_ADI) | SHIFT_ID(HWLOCK_GLB) |
		SHIFT_ID(HWLOCK_AGPIO) | SHIFT_ID(HWLOCK_AEIC) |
		SHIFT_ID(HWLOCK_ADC) | SHIFT_ID(HWLOCK_CACHE));
}

static inline int arch_hwlock_fast_trylock(unsigned int lock_id)
{
	unsigned long addr = HWLOCK_ADDR(lock_id);
	sci_glb_set(REG_AHB_AHB_CTL0, BIT_SPINLOCK_EB);
	if (HWSPINLOCK_NOTTAKEN == __raw_readl(addr)) {
		__raw_writel(HWSPINLOCK_WRITE_KEY, addr);
		if (HWSPINLOCK_WRITE_KEY == __raw_readl(addr)) {
			dsb();
			return 1;
		}
	}

	return 0;
}
static inline void arch_hwlock_fast_unlock(unsigned int lock_id)
{
	unsigned long addr = HWLOCK_ADDR(lock_id);
	dsb();
	__raw_writel(HWSPINLOCK_NOTTAKEN, addr);
}

#define arch_hwlock_fast(_LOCK_ID_) do { \
	while (!arch_hwlock_fast_trylock(_LOCK_ID_)) \
	cpu_relax();} while (0)

#define arch_hwunlock_fast(_LOCK_ID_) do { \
				arch_hwlock_fast_unlock(_LOCK_ID_);} while (0)

#endif

/*
 * SPRD hardware spinlock driver
 *
 * Copyright (C) 2012 Spreadtrum  - http://www.spreadtrum.com
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com
 *
 * Contact: steve.zhan <steve.zhan@spreadtrum.com>
 *        
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hwspinlock.h>
#include <linux/platform_device.h>

#include <mach/sci.h>
#include <mach/hardware.h>
#include <mach/regs_ahb.h>
#include <mach/arch_lock.h>

#include "hwspinlock_internal.h"


#define HWSPINLOCK_MAX_NUM	(32)

#define HWSPINLOCK_NOTTAKEN		(0x524c534c)	/*free: RLSL */
#define HWSPINLOCK_ENABLE_CLEAR		(0x454e434c)

#define HWSPINLOCK_CLEAR		(0x0)
#define HWSPINLOCK_TTLSTS		(0x4)
#define HWSPINLOCK_DTLSTL		(0x8)
#define HWSPINLOCK_CLEAREN		(0xc)

#define HWSPINLOCK_TOKEN(_X_)	(0x80 + 0x4*(_X_))

#define THIS_PROCESSOR_KEY	(HWSPINLOCK_WRITE_KEY)	/*first processor */
static void __iomem *hwspinlock_base;

#define SPINLOCKS_BUSY()	(readl(hwspinlock_base + HWSPINLOCK_TTLSTS))
#define SPINLOCKS_DETAIL_STATUS(_X_)	(_X_ = readl(hwspinlock_base + HWSPINLOCK_DTLSTL))
#define SPINLOCKS_ENABLE_CLEAR()	writel(HWSPINLOCK_ENABLE_CLEAR,hwspinlock_base + HWSPINLOCK_CLEAREN)
#define SPINLOCKS_DISABLE_CLEAR()	writel(~HWSPINLOCK_ENABLE_CLEAR, hwspinlock_base + HWSPINLOCK_CLEAREN)

#define to_sprd_hwspinlock(lock)	\
	container_of(lock, struct sprd_hwspinlock, lock)

struct sprd_hwspinlock {
	struct hwspinlock lock;
	void __iomem *addr;
};

struct sprd_hwspinlock_state {
	int num_locks;		/* Total number of locks in system */
	void __iomem *io_base;	/* Mapped base address */
};

static int hwspinlock_isbusy(unsigned int lockid)
{
	unsigned int status = 0;
	SPINLOCKS_DETAIL_STATUS(status);
	return ((status & (1 << lockid)) ? 1 : 0);
}

static int hwspinlocks_isbusy(void)
{
	return ((SPINLOCKS_BUSY())? 1 : 0);
}

static unsigned int do_lock_key(void *k)
{
	unsigned int key = THIS_PROCESSOR_KEY;
	return key;
}

static int sprd_hwspinlock_trylock(struct hwspinlock *lock)
{
	struct sprd_hwspinlock *sprd_lock = to_sprd_hwspinlock(lock);
	unsigned int key = do_lock_key(sprd_lock->addr);

	if (!(key ^ HWSPINLOCK_NOTTAKEN))
		BUG_ON(1);

	if (HWSPINLOCK_NOTTAKEN == readl(sprd_lock->addr)) {
		writel(key, sprd_lock->addr);
		return (key == readl(sprd_lock->addr));
	} else {
		return 0;
	}
}

static void sprd_hwspinlock_unlock(struct hwspinlock *lock)
{
	int key;
	struct sprd_hwspinlock *sprd_lock = to_sprd_hwspinlock(lock);
	key = readl(sprd_lock->addr);
	if (!(key ^ HWSPINLOCK_NOTTAKEN))
		BUG_ON(1);
	writel(HWSPINLOCK_NOTTAKEN, sprd_lock->addr);
}

/*
 * relax the SPRD interconnect while spinning on it.
 *
 * The specs recommended that the retry delay time will be
 * just over half of the time that a requester would be
 * expected to hold the lock.
 *
 * The number below is taken from an hardware specs example,
 * obviously it is somewhat arbitrary.
 */
static void sprd_hwspinlock_relax(struct hwspinlock *lock)
{
	ndelay(10);
}

static const struct hwspinlock_ops sprd_hwspinlock_ops = {
	.trylock = sprd_hwspinlock_trylock,
	.unlock = sprd_hwspinlock_unlock,
	.relax = sprd_hwspinlock_relax,
};

static void hwspinlock_clear(unsigned int lockid)
{
	/*setting the abnormal clear bit to 1 makes the corresponding
	  *lock to Not Taken state 
	  */
	SPINLOCKS_ENABLE_CLEAR();
	writel(1<<lockid, hwspinlock_base + HWSPINLOCK_CLEAR);
	SPINLOCKS_DISABLE_CLEAR();
}

static void hwspinlock_clear_all(void)
{
	unsigned int lockid = 0;
	/*setting the abnormal clear bit to 1 makes the corresponding
	  *lock to Not Taken state
	  */
	SPINLOCKS_ENABLE_CLEAR();
	do {
		writel(1<<lockid, hwspinlock_base + HWSPINLOCK_CLEAR);
	} while (lockid++ < HWSPINLOCK_MAX_NUM);
	SPINLOCKS_DISABLE_CLEAR();
}

static int __devinit sprd_hwspinlock_probe(struct platform_device *pdev)
{
	struct sprd_hwspinlock *sprd_lock;
	struct sprd_hwspinlock_state *state;
	struct hwspinlock *lock;
	struct resource *res;
	int i, ret;

	sci_glb_set(REG_AHB_AHB_CTL0, BIT_SPINLOCK_EB);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	if (!res->start) {
		ret = -ENOMEM;
		goto free_state;
	}

	state->num_locks = HWSPINLOCK_MAX_NUM;
	state->io_base = (void __iomem *)res->start;
	hwspinlock_base = state->io_base;

	platform_set_drvdata(pdev, state);

	/*
	 * runtime PM will make sure the clock of this module is
	 * enabled if at least one lock is requested
	 */
	pm_runtime_enable(&pdev->dev);

	for (i = 0; i < state->num_locks; i++) {
		sprd_lock = kzalloc(sizeof(*sprd_lock), GFP_KERNEL);
		if (!sprd_lock) {
			ret = -ENOMEM;
			goto free_locks;
		}

		sprd_lock->lock.dev = &pdev->dev;
		sprd_lock->lock.owner = THIS_MODULE;
		sprd_lock->lock.id = i;
		sprd_lock->lock.ops = &sprd_hwspinlock_ops;
		sprd_lock->addr =
		    (void __iomem *)(res->start + HWSPINLOCK_TOKEN(i));

		ret = hwspin_lock_register(&sprd_lock->lock);
		if (ret) {
			kfree(sprd_lock);
			goto free_locks;
		}
	}

	printk("sprd_hwspinlock_probe ok\n");
	return 0;

free_locks:
	while (--i >= 0) {
		lock = hwspin_lock_unregister(i);
		/* this should't happen, but let's give our best effort */
		if (!lock) {
			dev_err(&pdev->dev, "%s: cleanups failed\n", __func__);
			continue;
		}
		sprd_lock = to_sprd_hwspinlock(lock);
		kfree(sprd_lock);
	}

	pm_runtime_disable(&pdev->dev);

free_state:
	kfree(state);
	return ret;
}

static int sprd_hwspinlock_remove(struct platform_device *pdev)
{
	struct sprd_hwspinlock_state *state = platform_get_drvdata(pdev);
	struct hwspinlock *lock;
	struct sprd_hwspinlock *sprd_lock;
	int i;

	for (i = 0; i < state->num_locks; i++) {
		lock = hwspin_lock_unregister(i);
		/* this shouldn't happen at this point. if it does, at least
		 * don't continue with the remove */
		if (!lock) {
			dev_err(&pdev->dev, "%s: failed on %d\n", __func__, i);
			return -EBUSY;
		}

		sprd_lock = to_sprd_hwspinlock(lock);
		kfree(sprd_lock);
	}

	pm_runtime_disable(&pdev->dev);
	kfree(state);

	return 0;
}

static struct platform_driver sprd_hwspinlock_driver = {
	.probe = sprd_hwspinlock_probe,
	.remove = sprd_hwspinlock_remove,
	.driver = {
		   .name = "sprd_hwspinlock",
		   },
};

static int __init sprd_hwspinlock_init(void)
{
	return platform_driver_register(&sprd_hwspinlock_driver);
}

/* board init code might need to reserve hwspinlocks for predefined purposes */
postcore_initcall(sprd_hwspinlock_init);

static void __exit sprd_hwspinlock_exit(void)
{
	platform_driver_unregister(&sprd_hwspinlock_driver);
}

module_exit(sprd_hwspinlock_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hardware spinlock driver for Spreadtrum");
MODULE_AUTHOR("steve.zhan <steve.zhan@spreadtrum.com>");

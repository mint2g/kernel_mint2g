/* linux/arch/arm/mach-sc8825/platsmp.c
 * 
 * Copyright (c) 2010-2012 Spreadtrum Co., Ltd.
 *		http://www.spreadtrum.com
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Cloned from linux/arch/arm/mach-vexpress/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/smp_scu.h>
#include <asm/unified.h>

#include <mach/hardware.h>

//#include <plat/cpu.h>

#ifndef CONFIG_NKERNEL
extern void sc8825_secondary_startup(void);

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen"
 */
volatile int  pen_release = -1;
#else
extern volatile int __cpuinitdata pen_release;
#endif


#if !defined(CONFIG_NKERNEL) || defined(CONFIG_NKERNEL_PM_MASTER)
/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void __cpuinit write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static DEFINE_SPINLOCK(boot_lock);

#define HOLDING_PEN_VALUE_DEFAULT	(0xaa55abcd)
#define HOLDING_PEN_VALUE_FROM_SLEEP	(0x1234abcd)

static void write_reg_pen(unsigned int val)
{
	*(volatile int*)(HOLDING_PEN_VADDR) = val;
}


#ifdef CONFIG_NKERNEL_PM_MASTER
void __cpuinit phys_platform_secondary_init(unsigned int cpu)
#else
void __cpuinit platform_secondary_init(unsigned int cpu)
#endif
{
	/*
	 * if any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
	gic_secondary_init(0);

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */

	write_reg_pen(HOLDING_PEN_VALUE_DEFAULT);
	write_pen_release(-1);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
#ifdef CONFIG_NKERNEL_PM_MASTER
	hw_local_irq_enable();
#endif
}

#ifdef CONFIG_NKERNEL_PM_MASTER
int __cpuinit phys_boot_secondary(unsigned int cpu, struct task_struct *idle)
#else
int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
#endif
{
	unsigned long timeout;

#ifdef	CONFIG_NKERNEL
	if (!cpu || (cpu >= VCPU()->maxvcpus))
		__WARN();
#endif

	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 */
	write_reg_pen(1 << (cpu * 4));
	write_pen_release(cpu);

	/* use ipi to wake up cpu1 and wait for pen released */
	gic_raise_softirq(cpumask_of(cpu), 15);
	timeout = jiffies + (1 * HZ);

	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == -1)
			break;

		udelay(10);
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return pen_release != -1 ? -ENOSYS : 0;
}
#endif

#ifndef CONFIG_NKERNEL
static void __iomem *scu_base_addr(void)
{
	return (void __iomem *)(SPRD_A5MP_BASE);
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */

void __init smp_init_cpus(void)
{
	void __iomem *scu_base = scu_base_addr();
	unsigned int i, ncores;

	ncores = scu_base ? scu_get_core_count(scu_base) : 1;

	/* sanity check */
	if (ncores > NR_CPUS) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
			ncores, NR_CPUS);
		ncores = NR_CPUS;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

	set_smp_cross_call(gic_raise_softirq);
}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;

	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time.
	 */
	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

	scu_enable(scu_base_addr());

	/*
	 * Write the address of secondary startup into the
	 * system-wide flags register. The boot monitor waits
	 * until it receives a soft interrupt, and then the
	 * secondary CPU branches to this address.
	 */
	__raw_writel(BSYM(virt_to_phys(sc8825_secondary_startup)),
			CPU1_JUMP_VADDR);
}
#endif

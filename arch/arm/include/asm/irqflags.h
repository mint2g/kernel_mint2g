/*
 *  Copyright (C) 2011, Red Bend Ltd.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  You should have received a copy of the GNU General Public License Version 2
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ASM_ARM_IRQFLAGS_H
#define __ASM_ARM_IRQFLAGS_H

#ifdef __KERNEL__

#include <asm/ptrace.h>

/*
 * CPU interrupt mask handling.
 */
#if __LINUX_ARM_ARCH__ >= 6

static inline unsigned long hw_local_irq_save(void)
{
	unsigned long flags;

	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_save\n"
		"	cpsid	i"
		: "=r" (flags) : : "memory", "cc");
	return flags;
}

static inline void hw_local_irq_enable(void)
{
	asm volatile(
		"	cpsie i			@ arch_local_irq_enable"
		:
		:
		: "memory", "cc");
}

static inline void hw_local_irq_disable(void)
{
	asm volatile(
		"	cpsid i			@ arch_local_irq_disable"
		:
		:
		: "memory", "cc");
}

#define local_fiq_enable()  __asm__("cpsie f	@ __stf" : : : "memory", "cc")
#define local_fiq_disable() __asm__("cpsid f	@ __clf" : : : "memory", "cc")
#else

/*
 * Save the current interrupt enable state & disable IRQs
 */
static inline unsigned long hw_local_irq_save(void)
{
	unsigned long flags, temp;

	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_save\n"
		"	orr	%1, %0, #128\n"
		"	msr	cpsr_c, %1"
		: "=r" (flags), "=r" (temp)
		:
		: "memory", "cc");
	return flags;
}

/*
 * Enable IRQs
 */
static inline void hw_local_irq_enable(void)
{
	unsigned long temp;
	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_enable\n"
		"	bic	%0, %0, #128\n"
		"	msr	cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

/*
 * Disable IRQs
 */
static inline void hw_local_irq_disable(void)
{
	unsigned long temp;
	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_disable\n"
		"	orr	%0, %0, #128\n"
		"	msr	cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

/*
 * Enable FIQs
 */
#define local_fiq_enable()					\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ stf\n"		\
"	bic	%0, %0, #64\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory", "cc");					\
	})

/*
 * Disable FIQs
 */
#define local_fiq_disable()					\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ clf\n"		\
"	orr	%0, %0, #64\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory", "cc");					\
	})

#endif

/*
 * Save the current interrupt enable state.
 */
static inline unsigned long hw_local_save_flags(void)
{
	unsigned long flags;
	asm volatile(
		"	mrs	%0, cpsr	@ local_save_flags"
		: "=r" (flags) : : "memory", "cc");
	return flags;
}

/*
 * restore saved IRQ & FIQ state
 */
static inline void hw_local_irq_restore(unsigned long flags)
{
	asm volatile(
		"	msr	cpsr_c, %0	@ local_irq_restore"
		:
		: "r" (flags)
		: "memory", "cc");
}

#ifndef	CONFIG_NKERNEL

#define arch_local_irq_save	hw_local_irq_save
#define arch_local_irq_enable	hw_local_irq_enable
#define arch_local_irq_disable	hw_local_irq_disable
#define arch_local_save_flags	hw_local_save_flags
#define arch_local_irq_restore	hw_local_irq_restore

static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return flags & PSR_I_BIT;
}

#else	/* CONFIG_NKERNEL */

unsigned int _irq_save(void);
void _irq_set(void);
unsigned int _save_flags(void);
unsigned int _irq_restore(unsigned int);
unsigned int _irq_pending(void);

/*
 * Save the current interrupt enable state & disable IRQs
 */
static inline unsigned long arch_local_irq_save(void)
{
	return _irq_save();
}

/*
 * Enable IRQs
 */
static inline void arch_local_irq_enable(void)
{
	_irq_set();
}

/*
 * Disable IRQs
 */
static inline void arch_local_irq_disable(void)
{
	(void) _irq_save();
}

/*
 * Save the current interrupt enable state.
 */
static inline unsigned long arch_local_save_flags(void)
{
	return _save_flags();
}

/*
 * restore saved IRQ & FIQ state
 */
static inline void arch_local_irq_restore(unsigned long flags)
{
	_irq_restore(flags);
}

static inline unsigned int arch_local_irq_pending(void)
{
	return _irq_pending();
}

static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return !(flags & __VEX_IRQ_FLAG);
}

#endif /* CONFIG_NKERNEL */

#endif
#endif

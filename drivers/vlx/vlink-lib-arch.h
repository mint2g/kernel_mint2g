/*****************************************************************************
 *                                                                           *
 *  Component: VLX VLink Wrapper Library.                                    *
 *             Optimized and architecture-specific routines.                 *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  This program is free software: you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License Version 2           *
 *  as published by the Free Software Foundation.                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  Version 2 along with this program.                                       *
 *  If not, see <http://www.gnu.org/licenses/>.                              *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VLINK_LIB_ARCH_H
#define _VLX_VLINK_LIB_ARCH_H

#include <stddef.h>
#include <linux/types.h>
#include <asm/atomic.h>

#undef __ARCH_HAVE_FAST_ATOMIC_DEC
#undef __ARCH_HAVE_FAST_ATOMIC_INC
#undef __ARCH_HAVE_FAST_ATOMIC_INC_COND

#if defined(CONFIG_ARM) && !defined(CONFIG_SMP)

#if __LINUX_ARM_ARCH__ < 6

#ifdef CONFIG_NKERNEL

#include <linux/irqflags.h>

#define __ARCH_HAVE_FAST_ATOMIC_DEC

#define fast_atomic_dec(pvar)						\
    do {								\
	unsigned long _cpsr;						\
									\
	hw_local_irq_save(_cpsr);					\
	(pvar)->counter--;						\
	hw_local_irq_restore(_cpsr);					\
    } while (0)

#define __ARCH_HAVE_FAST_ATOMIC_INC

#define fast_atomic_inc(pvar)						\
    do {								\
	unsigned long _cpsr;						\
									\
	hw_local_irq_save(_cpsr);					\
	(pvar)->counter++;						\
	hw_local_irq_restore(_cpsr);					\
    } while (0)

#define __ARCH_HAVE_FAST_ATOMIC_INC_COND

#define fast_atomic_inc_cond(pvar,pcond,condval)			\
    ({									\
	unsigned long _cpsr;						\
	unsigned long _res;						\
	unsigned long _cnt;						\
									\
	hw_local_irq_save(_cpsr);					\
									\
	_cnt = (pvar)->counter;						\
	_res = *(pcond) - (condval);					\
	if (_res == 0) {						\
	    (pvar)->counter = ++_cnt;					\
	}								\
									\
	hw_local_irq_restore(_cpsr);					\
									\
	(_res);								\
    })

#else /* !CONFIG_NKERNEL */

/* VLX Isolator support TBD */

#endif /* !CONFIG_NKERNEL */

#else /* __LINUX_ARM_ARCH__ >= 6 */

#define __ARCH_HAVE_FAST_ATOMIC_INC_COND

#define fast_atomic_inc_cond(pvar,pcond,condval)			\
    ({									\
	unsigned long _tmp;						\
	unsigned long _res;						\
									\
        __asm__ __volatile__(						\
	    "1:	ldr	%0, [%3]	\n"				\
	    "	ldrex	%1, [%2]	\n"				\
	    "	subs	%0, %0, %4	\n"				\
	    "	add	%1, %1, #1	\n"				\
	    "	bne	2f		\n"				\
	    "	strex	%0, %1, [%2]	\n"				\
	    "	teq	%0, #0		\n"				\
	    "	bne	1b		\n"				\
	    "2:				\n"				\
	    : "=&r" (_res), "=&r" (_tmp)				\
	    : "r" (&(pvar)->counter), "r" (pcond), "i" (condval)	\
	    : "memory", "cc");						\
									\
	(_res);								\
    })

#endif /* __LINUX_ARM_ARCH__ >= 6 */

#endif /* CONFIG_ARM && !CONFIG_SMP */

#ifdef __ARCH_HAVE_FAST_ATOMIC_DEC
#define vlink_atomic_dec fast_atomic_dec
#else
#define vlink_atomic_dec atomic_dec
#endif

#ifdef __ARCH_HAVE_FAST_ATOMIC_INC
#define vlink_atomic_inc fast_atomic_inc
#else
#define vlink_atomic_inc atomic_inc
#endif

#endif /* _VLX_VLINK_LIB_ARCH_H */

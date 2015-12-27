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

#ifndef __ASM_ARM_IRQ_H
#define __ASM_ARM_IRQ_H

#include <mach/irqs.h>

#ifndef irq_canonicalize
#define irq_canonicalize(i)	(i)
#endif

#define NR_IRQS_LEGACY	16

#ifdef CONFIG_NKERNEL

#ifndef NR_IRQS
.error	"NR_IRQS must be defined in <mach/irqs.h>"
#endif

#include <asm/nk/f_nk.h>
#undef	NR_IRQS
#define NR_IRQS	NK_XIRQ_LIMIT

#endif	/* CONFIG_NKERNEL */

/*
 * Use this value to indicate lack of interrupt
 * capability
 */
#ifndef NO_IRQ
#define NO_IRQ	((unsigned int)(-1))
#endif

#ifndef __ASSEMBLY__
struct irqaction;
struct pt_regs;
extern void migrate_irqs(void);

extern void asm_do_IRQ(unsigned int, struct pt_regs *);
void init_IRQ(void);

void arch_trigger_all_cpu_backtrace(void);
#define arch_trigger_all_cpu_backtrace arch_trigger_all_cpu_backtrace

#endif

#endif

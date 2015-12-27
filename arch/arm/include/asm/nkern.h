/*
 ****************************************************************
 *
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
 *
 *  #ident  "@(#)nkern.h 1.1     07/10/18 Red Bend"
 *
 *  Contributor(s):
 *    Guennadi Maslov (guennadi.maslov@redbend.com)
 *
 ****************************************************************
 */

#ifndef __NK_NKERN_H
#define __NK_NKERN_H

#include <asm/nk/f_nk.h>

#define	__NK_HARD_LOCK_IRQ_SAVE(x, flag)       ((flag) = hw_local_irq_save())
#define	__NK_HARD_UNLOCK_IRQ_RESTORE(x, flag)  hw_local_irq_restore(flag)

#ifdef __ARMEB__
#define	__VEX_IRQ_BYTE		(3 - (NK_VEX_IRQ_BIT >> 3))
#else
#define	__VEX_IRQ_BYTE		(NK_VEX_IRQ_BIT >> 3)
#endif
#define	__VEX_IRQ_BIT		(NK_VEX_IRQ_BIT & 0x7)
#define	__VEX_IRQ_FLAG		(1 << __VEX_IRQ_BIT)

#define	DOMAIN_NKVEC	15	/* domain used to map the NK vectors page */

#ifdef  __ASSEMBLY__

#define	__VEX_IRQ_CTX_PEN	(ctx_pending_off + __VEX_IRQ_BYTE)
#define	__VEX_IRQ_CTX_ENA	(ctx_enabled_off + __VEX_IRQ_BYTE)
#define	__VEX_IRQ_CTX_E2P	(__VEX_IRQ_CTX_PEN - __VEX_IRQ_CTX_ENA)
#define	__VEX_IRQ_CTX_P2E	(__VEX_IRQ_CTX_ENA - __VEX_IRQ_CTX_PEN)

#endif

	/*
	 * In the thread context, the pending irq bitmask is saved in
	 * unused bits of CPRS image (bits 16-23).
	 */
#define	NK_VPSR_SHIFT	23

#ifndef __ASSEMBLY__

#ifdef CONFIG_SMP

    static inline NkOsCtx*
VCPU (void)
{
    NkOsCtx* vcpu;
    __asm__ __volatile__ (
        "mrc	p15, 0, %0, c13, c0, 4"
	: "=r" (vcpu) :: "memory", "cc");
    return vcpu;
}

#define	os_ctx  	VCPU()
#define  __irq_enabled	(((nku8_f*)&(VCPU()->enabled)) + __VEX_IRQ_BYTE)

#else

extern NkOsCtx* os_ctx;	 	/* pointer to OS context */
extern nku8_f*  __irq_enabled;  /* points to the enabled IRQ flag */

#define	VCPU() 	os_ctx

#endif


#endif

#endif

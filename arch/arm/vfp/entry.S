/*
 *  linux/arch/arm/vfp/entry.S
 *
 *  Copyright (C) 2004 ARM Limited.
 *  Written by Deep Blue Solutions Limited.
 *  Copyright (C) 2011, Red Bend Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Basic entry code, called from the kernel's undefined instruction trap.
 *  r0  = faulted instruction
 *  r2  = faulted PC+4
 *  r9  = successful return
 *  r10 = thread_info structure
 *  lr  = failure return
 */
#include <asm/thread_info.h>
#include <asm/vfpmacros.h>

#include "../kernel/entry-header.S"

ENTRY(do_vfp)
#ifdef CONFIG_NKERNEL
#ifdef CONFIG_SMP
	get_os_ctx r1
#else
	ldr	r1, .os_ctx		@ get os ctx
	ldr	r1, [r1]
#endif
	ldr	r3, [r1, #ctx_vfp_owned_off]	@ check if we already own vfp
	cmp	r3, #0
	bne	1f

	sub	r2, r2, #4		@ vfp was disabled so
	str	r2, [ sp, #S_PC ]	@ bounce exception


	ldr	r3, [r1, #ctx_vfp_get]	@ if we don't have just acquire
	mov	r0, r1
	mov	lr, r9			@ restore vfp context
	mov	pc, r3			@ and re-issue the instruction
1:
#endif
#ifdef CONFIG_PREEMPT
	ldr	r4, [r10, #TI_PREEMPT]	@ get preempt count
	add	r11, r4, #1		@ increment it
	str	r11, [r10, #TI_PREEMPT]
#endif
	enable_irq
	str	r2, [sp, #S_PC]		@ update regs->ARM_pc for Thumb 2 case
 	ldr	r4, .LCvfp
	ldr	r11, [r10, #TI_CPU]	@ CPU number
	add	r10, r10, #TI_VFPSTATE	@ r10 = workspace
	ldr	pc, [r4]		@ call VFP entry point
ENDPROC(do_vfp)

ENTRY(vfp_null_entry)
#ifdef CONFIG_PREEMPT
	get_thread_info	r10
	ldr	r4, [r10, #TI_PREEMPT]	@ get preempt count
	sub	r11, r4, #1		@ decrement it
	str	r11, [r10, #TI_PREEMPT]
#endif
	mov	pc, lr
ENDPROC(vfp_null_entry)

	.align	2
.LCvfp:
	.word	vfp_vector

#if defined(CONFIG_NKERNEL) && !defined(CONFIG_SMP)
.os_ctx:
	.word	os_ctx
#endif

@ This code is called if the VFP does not exist. It needs to flag the
@ failure to the VFP initialisation code.

	__INIT
ENTRY(vfp_testing_entry)
#ifdef CONFIG_PREEMPT
	get_thread_info	r10
	ldr	r4, [r10, #TI_PREEMPT]	@ get preempt count
	sub	r11, r4, #1		@ decrement it
	str	r11, [r10, #TI_PREEMPT]
#endif
	ldr	r0, VFP_arch_address
	str	r5, [r0]		@ known non-zero value
	mov	pc, r9			@ we have handled the fault
ENDPROC(vfp_testing_entry)

	.align	2
VFP_arch_address:
	.word	VFP_arch

	__FINIT

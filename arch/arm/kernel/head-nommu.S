/*
 *  linux/arch/arm/kernel/head-nommu.S
 *
 *  Copyright (C) 1994-2002 Russell King
 *  Copyright (C) 2003-2006 Hyok S. Choi
 *  Copyright (C) 2011, Red Bend Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Common kernel startup code (non-paged MM)
 *
 */
#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/assembler.h>
#include <asm/ptrace.h>
#include <asm/asm-offsets.h>
#include <asm/thread_info.h>
#include <asm/system.h>

#ifdef CONFIG_NKERNEL
#include <asm/nkern.h>
#endif

#ifdef CONFIG_RELOCATABLE_NKERNEL
/*
 * Initially vmlinux is linked as a relocatable file in the
 * nanokernel environment, later it is relinked by mkimage
 * as an absolute file. Because of that ALIGN  directives
 * outside of any section in vmlinux.lds script have no effect.
 * We declare all sections with nontrivial alignment here,
 * in order to guarantee their alignment.
 */
		.section "__ex_table", #alloc
		.align	5

		.section ".end_of_text", #alloc
		.align	2

		.section ".end_of_init", #alloc
		.align	12

		.section ".text_data_barrier", #alloc
		.align	13

		.section ".data..init_task", #alloc
		.align	13

		.section ".data..cacheline_aligned", #alloc
		.align	5
#endif /* CONFIG_RELOCATABLE_NKERNEL */

/*
 * Kernel startup entry point.
 * ---------------------------
 *
 * This is normally called from the decompressor code.  The requirements
 * are: MMU = off, D-cache = off, I-cache = dont care, r0 = 0,
 * r1 = machine nr.
 *
 * See linux/arch/arm/tools/mach-types for the complete list of machine
 * numbers for r1.
 *
 */
	__HEAD
ENTRY(stext)
#ifndef CONFIG_NKERNEL
	setmode	PSR_F_BIT | PSR_I_BIT | SVC_MODE, r9 @ ensure svc mode
						@ and irqs disabled
#else
	mov	r12, sp				@ save os_ctx in r12 (unused)
@
@	set an initial sp and store os_ctx at its location,
@	so printnk will work until setting sp to __mmap_switched_data
@	below. note that __mmap_switched will erase bss,
@	so stack contents and os_ctx pointer will be lost.
@	__mmap_switched reinitializes sp and os_ctx pointer
@	after bss erasing.
@
	ldr	sp, __mmap_switched_data + 32	@ set an initial sp
	ldr	r9, _os_ctx			@ store os_ctx pointer
	str	r12, [r9]			@ at its location
#ifdef CONFIG_MACH_ARM_OSWARE
	ldr	r1, _mach_type			@ set arch_id to ARM_OSWARE
#else
	ldr	r1, [r12, #ctx_arch_id_off]	@ load arch_id from os_ctx
#endif
	mov	r0, #0				@ clear r0 to fulfill
						@ boot requirements
#endif /* CONFIG_NKERNEL */

#ifndef CONFIG_CPU_CP15
	ldr	r9, =CONFIG_PROCESSOR_ID
#else
	mrc	p15, 0, r9, c0, c0		@ get processor id
#endif
	bl	__lookup_processor_type		@ r5=procinfo r9=cpuid
	movs	r10, r5				@ invalid processor (r5=0)?
	beq	__error_p				@ yes, error 'p'

	adr	lr, BSYM(__after_proc_init)	@ return (PIC) address
 ARM(	add	pc, r10, #PROCINFO_INITFUNC	)
#ifdef CONFIG_NKERNEL
 THUMB ( .error "not supported yet - r12 is not free if CONFIG_NKERNEL" )
#else
 THUMB(	add	r12, r10, #PROCINFO_INITFUNC	)
 THUMB(	mov	pc, r12				)
#endif
ENDPROC(stext)

/*
 * Set the Control Register and Read the process ID.
 */
__after_proc_init:
#ifdef CONFIG_CPU_CP15
	/*
	 * CP15 system control register value returned in r0 from
	 * the CPU init function.
	 */
#ifdef CONFIG_ALIGNMENT_TRAP
	orr	r0, r0, #CR_A
#else
	bic	r0, r0, #CR_A
#endif
#ifdef CONFIG_CPU_DCACHE_DISABLE
	bic	r0, r0, #CR_C
#endif
#ifdef CONFIG_CPU_BPREDICT_DISABLE
	bic	r0, r0, #CR_Z
#endif
#ifdef CONFIG_CPU_ICACHE_DISABLE
	bic	r0, r0, #CR_I
#endif
#ifdef CONFIG_CPU_HIGH_VECTOR
	orr	r0, r0, #CR_V
#else
	bic	r0, r0, #CR_V
#endif
	mcr	p15, 0, r0, c1, c0, 0		@ write control reg
#endif /* CONFIG_CPU_CP15 */

	b	__mmap_switched			@ clear the BSS and jump
						@ to start_kernel
ENDPROC(__after_proc_init)
	.ltorg

#include "head-common.S"

/*
 * sc8825 standby mode
 *
 * Author: Wang Liwei
 *
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/smp_scu.h>
#include <mach/hardware.h>
#include <mach/regs_glb.h>
/*
#include <mach/common.h>
*/
#define CPU_SAVED_STATE_SIZE     (4*11 + 4*10)
#define CPU_SAVED_MOD_STATE_SIZE (4*2)
#define CPU_NUM                  (2)
#define EMC_REINIT_OFFSET	 (0x2000)
#define REG_HOLDING_PEN_PHY_ADDR (SPRD_AHB_PHYS + 0x240)
#define REG_CPU1_JUMP_PHY_ADDR	 (SPRD_AHB_PHYS + 0x248)
#define REG_GLB_GEN0_PHY_ADDR	 (SPRD_GREG_PHYS + 0x8)
#define REG_GIC_ICR_PHY_ADDR	 (SPRD_A5MP_PHYS + 0x100)
#define REG_GIC_IPMR_PHY_ADDR	 (SPRD_A5MP_PHYS + 0x104)
#define UART1_EN		 (0X1 << 21)

	.macro	addruart,rx
	@ see if the MMU is enabled and select appropriate base address
	mrc	p15, 0, \rx, c1, c0
	tst	\rx, #1
	ldreq	\rx, =SPRD_UART1_PHYS
	ldrne	\rx, =SPRD_UART1_BASE
	.endm

	.macro	senduart,rd,rx
	and	\rd,\rd,#0xFF
	str	\rd, [\rx, #0x00]
	.endm

	.macro	waituart,rd,rx
	@ wait for TX_READY
1:	ldr	\rd, [\rx, #0x0C]
	mov	\rd,\rd,lsr #8
	and	\rd,\rd,#0xFF
	teq	\rd, #0x00
	bne	1b
	.endm

	.macro	busyuart,rd,rx
	@ wait for TX_READY
2:	ldr	\rd, [\rx, #0x0C]
	mov	\rd,\rd,lsr #8
	and	\rd,\rd,#0xFF
	teq	\rd, #0x00
	bne	2b
	.endm

	.text
	.align 12

ENTRY(sc8825_standby_iram)
#if 0
	/*debug info starts*/
	stmfd sp!, {r4-r12, lr}

	ldr		r1, =REG_GLB_GEN0
	ldr		r2, [r1, #0]
	#orr		r2, r2, #(0x1 << 21)
	orr		r2, r2, #GEN0_UART1_EN
	str		r2, [r1, #0]

	mov r0, #'@'
	bl printch

	mov r0, #'>'
	bl printch

	ldr r0, =0x2ff
	bl my_delay

	ldr		r1, =REG_GLB_GEN0
	ldr		r2, [r1, #0]
	#bic		r2, r2, #(0x1 << 21)
	bic		r2, r2, #GEN0_UART1_EN
	str		r2, [r1, #0]
	ldmfd sp!, {r4-r12, lr}
	/*debug info ends*/
#endif
sprd_pm_mapped_va:
	ldr r0, =sprd_pm_va_to_iram
	ldr r1, =sc8825_standby_iram
	ldr r2, =iram_start
	ldr r3, [r2, #0]
	sub r0, r0, r1
	add r0, r0, r3
	mov pc, r0
sprd_pm_va_to_iram:
	/* Run inside IRAM now. */
	nop
	nop
	nop

	dsb
	isb
	/*
	   ldr r1, =REG_AHB_AHB_PAUSE
	   ldr r2, [r1]
	   orr r2, r2, #0x06
	   str r2, [r1, #0]

	   mov r0, #0x20
	   1:  sub	r0, r0, #0x1
	   cmp	r0, #0x0
	   bne  1b
	*/
	wfi
#if 0
1:	b 1b
	/*
	 * TODO: EMC must be re-initialized if its own DLL stopped
	 * code of function sc8825_emc_reinit() has been copied to (iram_start+8K)
	*/
	stmfd sp!, {r4-r12, lr}
	mov r0, #0
	blx r2
	ldmfd sp!, {r4-r12, lr}
	/*add sp, sp, #0x4*/
	ldr sp, [sp]
	
	/*debug info ends*/
	ldr r0, =0xeb320008 /* GEN0 */
	ldr r1, =0x00700001 /* enable uart1 */
	str r1, [r0]
	ldr r0, =0xeb272000 /* uart1 tx_fifo */
	mov r1, #'>'
1:	str r1, [r0]
	b 1b
	/*debug info ends*/
#endif
	nop
	nop
	nop
	mov pc, lr
	nop
	nop

ENDPROC(sc8825_standby_iram)


ENTRY(sc8825_standby_exit_iram)

	mrs r0, cpsr
	and r0, r0, #0xff
	mov r6, r0
	ldr r1, =0x00d3
	cmp r0, r1
	msr cpsr_c, r1
	#blne dead
/*
	adr r0, sleep_asm_counter
	ldr r1, [r0]
	add r1, r1, #1
	str r1, [r0]
	cmp r1, #32
	bleq dead
*/

	ldr r0, =0x0020
	bl my_delay

#if 0
	/*debug info starts*/
	/* enable uart1 clock to print some messages. */
	ldr		r1, =REG_GLB_GEN0_PHY_ADDR
	ldr		r2, [r1, #0]
	orr		r2, r2, #UART1_EN	
	str		r2, [r1, #0]
	ldr r0, =0x44000000
	mov r1, #'>'
1:	str r1, [r0]
	b 1b

	ldr r0, =0x20
	bl my_delay

	mov r0, #'#'
	bl printch

	mov r0, #'<'
	bl printch

	mov r0, #'\r'
	bl printch

	mov r0, #'\n'
	bl printch
	/*debug info ends*/

#endif
	mrc	p15, 0, r4, c0, c0, 5
	ands	r4, r4, #0xf
	cmp	r4, #0
	adreq	r5, emc_repower
	moveq	pc, r5
hold_secondary_cores:
	ldr	r1, =REG_GIC_ICR_PHY_ADDR
	ldr     r2, =0x1
	str	r2, [r1]
	/* set cpu interface priority.
	*  0xf0 is default value in kernel source code
	*/
	ldr	r1, =REG_GIC_IPMR_PHY_ADDR
	ldr     r2, =0xf0
	str	r2, [r1]
	wfine
	ldr	sp, =0xc0000000 /*should be deleted*/
	ldr     r1, =REG_HOLDING_PEN_PHY_ADDR
	ldr     r3, [r1]
	and     r3, #0x1
	ldr     r2, =0x1
	cmp     r3, r2
	adr	r3, hold_secondary_cores
	bxne	r3
	adr	r1, context_restore
	bx      r1

emc_repower:
	/*
	*   EMC should be re-initialized after power on. The code has been copy
	* to address (0x00004000 + 8K).
	*   After remapped( AHB_REMAP = 0x1 ), iram physical address is 0x00000000
	*/	
	ldr r0, =SPRD_IRAM_PHYS
	ldr r1, =EMC_REINIT_OFFSET
	add r1, r1, r0
	cpy sp, r1
	mov r0, #0x1	
	stmfd   sp!, {lr}
	blx r1	
	ldmfd   sp!, {lr}


	/*
	*  convert to physical address,
	*  need to find a more elegent way.
	*/
context_restore:
	ldr	r0, =sp_pm_collapse_exit
	ldr	r1, =0xc0000000
	sub	r0, r0, r1
	ldr	r1, =0x80000000
	add	r0, r0, r1 
	mov pc, r0
ENDPROC(sc8825_standby_exit_iram)


ENTRY(printascii_phy)
		addruart r3
		b	2f
1:		waituart r2, r3
		senduart r1, r3
		busyuart r2, r3
		teq	r1, #'\n'
		moveq	r1, #'\r'
		beq	1b
2:		teq	r0, #0
		ldrneb	r1, [r0], #1
		teqne	r1, #0
		bne	1b
		mov	pc, lr
ENDPROC(printascii_phy)

printch:
		addruart r3
		mov	r1, r0
		mov	r0, #0
		b	1b

ENTRY(my_delay)
1:  sub	r0, r0, #0x1
    cmp	r0, #0x0
    bne  1b
    mov pc, lr
ENDPROC(my_delay)

ENTRY(dead)
	ldr		r1, =REG_GLB_GEN0_PHY_ADDR
	ldr		r2, [r1, #0]
	orr		r2, r2, #UART1_EN
	str		r2, [r1, #0]

	ldr r0, =0x20
	bl my_delay

1:	adr	r0, str_a1
	bl	printascii_phy

	ldr r0, =0x000fffff
	bl my_delay

	b 1b
ENDPROC(dead)

	.globl sleep_asm_counter
sleep_asm_counter:
	.long	0x0


str_a1:	.asciz	"dead!"

ENTRY(sc8825_standby_iram_end)


/*
 * r0: cpu id
 * r1: 
 */
ENTRY(sp_pm_collapse)
	mov	r2, #CPU_SAVED_MOD_STATE_SIZE
	mul	r3, r0, r2

	msr	cpsr_c, #(PSR_F_BIT | PSR_I_BIT | IRQ_MODE)
	ldr     r2, =saved_state_irq
	add	r2, r2, r3
	stmia   r2!, {sp, lr}

	msr	cpsr_c, #(PSR_F_BIT | PSR_I_BIT | ABT_MODE)
	ldr     r2, =saved_state_abt
	add	r2, r2, r3
	stmia   r2!, {sp, lr}


	msr	cpsr_c, #(PSR_F_BIT | PSR_I_BIT | UND_MODE)
	ldr     r2, =saved_state_und
	add	r2, r2, r3
	stmia   r2!, {sp, lr}

/*
	msr	cpsr_c, #(PSR_F_BIT | PSR_I_BIT | SYSTEM_MODE)
	ldr     r0, =saved_state_sys
	stmia   r0!, {sp, lr}
*/

	msr	cpsr_c, #(PSR_F_BIT | PSR_I_BIT | SVC_MODE)


	mov	r2, #CPU_SAVED_STATE_SIZE
	mul	r3, r0, r2
	ldr     r2, =saved_state
	add	r2, r2, r3
	stmia   r2!, {r4-r14}

	mrc     p15, 0, r3, c1, c0, 0 /* MMU control */
	mrc     p15, 0, r4, c2, c0, 0 /* TTBR0 */
	mrc     p15, 0, r5, c3, c0, 0 /* dacr */
	mrc     p15, 0, r6, c13, c0, 4 /* wong, TPIDRPRW, thread-id */
	mrc     p15, 0, r7, c10, c2, 0 /* PRRR */
	mrc     p15, 0, r8, c10, c2, 1 /* NMRR */
	mrc     p15, 0, r9, c1, c0, 1 /* ACTLR */
	stmia   r2!, {r3-r9}

	mrc     p15, 0, r3, c2, c0, 1 /* TTBR1 */
	mrc     p15, 0, r4, c13, c0, 3 /* TPIDRURO */
	mrc     p15, 0, r5, c13, c0, 1 /* context ID */
	stmia   r2!, {r3-r5}

#if 0
/*
* here is omap2 sample code
*/
	#mrc     p15, 0, r4, c1, c0, 0 /* MMU control */
	#mrc     p15, 0, r5, c1, c0, 1 /* ACTLR */
	mrc     p15, 0, r6, c1, c0, 2 /* CPACR */
	#mrc     p15, 0, r7, c2, c0, 0 /* TTBR0 */
	#mrc     p15, 0, r8, c2, c0, 1 /* TTBR1 */
	mrc     p15, 0, r9, c2, c0, 2 /* TTBCR */
	#mrc     p15, 0, r10, c3, c0, 0 /* dacr */

	#mrc     p15, 0, r4, c10, c2, 0 /* PRRR */
	#mrc     p15, 0, r5, c10, c2, 1 /* NMRR */
	mrc     p15, 0, r6, c12, c0, 0 /* Secure or NS VBAR */
	#mrc     p15, 0, r7, c13, c0, 1 /* context ID */
	mrc     p15, 0, r8, c13, c0, 2 /* User r/w thread ID */
	#mrc     p15, 0, r9, c13, c0, 3 /* TPIDRURO */
#endif

#if 0
	bl		outer_cache_poweroff
#endif
	bl      v7_flush_dcache_all

	mrc     p15, 0, r4, c1, c0, 0    /* read current CR    */
	bic     r0, r4, #(1 << 2)        /* clear dcache bit   */
	bic     r0, r0, #(1 << 12)       /* clear icache bit   */
	mcr     p15, 0, r0, c1, c0, 0    /* disable d/i cache  */
	isb

	/* power off SCU */
	/* save scu configuration registers ?????
	*
	*  FIXME: r1 is save_state before, replaced by SCU_PM_POWEROFF now
	*/
#if 0
	bl	sprd_get_scu_base
	mov 	r1, #SCU_PM_POWEROFF
	bl	scu_power_mode
#endif
	bl sc8825_standby_iram

	mcr     p15, 0, r4, c1, c0, 0    /* restore d/i cache  */
	isb

	/* get cpuid */
	mrc	p15, 0, r0, c0, c0, 5
	and	r0, r0, #0x0f
	mov	r2, #CPU_SAVED_STATE_SIZE
	mul	r3, r0, r2
	ldr     r2, =saved_state         /* restore registers */
	add	r2, r2, r3
	ldmfd   r2, {r4-r14}

	mov     r0, #0                   /* return 0, power collapse failed */
	bx      lr

ENTRY(sp_pm_collapse_exit)
	msr	cpsr_c, #(PSR_F_BIT | PSR_I_BIT | IRQ_MODE)
	ldr     r1, =saved_state_irq
	mrc	p15, 0, r4, c0, c0, 5
	ands	r4, r4, #0xf
	addne	r1, r1, #CPU_SAVED_MOD_STATE_SIZE
	ldr     r2, =sp_pm_collapse
	adr     r3, sp_pm_collapse
	add     r1, r1, r3
	sub     r0, r1, r2
	ldmfd   r0, {sp, lr}

	msr	cpsr_c, #(PSR_F_BIT | PSR_I_BIT | ABT_MODE)
	ldr     r1, =saved_state_abt
	mrc	p15, 0, r4, c0, c0, 5
	ands	r4, r4, #0xf
	addne	r1, r1, #CPU_SAVED_MOD_STATE_SIZE
	ldr     r2, =sp_pm_collapse
	adr     r3, sp_pm_collapse
	add     r1, r1, r3
	sub     r0, r1, r2
	ldmfd   r0, {sp, lr}

	msr	cpsr_c, #(PSR_F_BIT | PSR_I_BIT | UND_MODE)
	ldr     r1, =saved_state_und
	mrc	p15, 0, r4, c0, c0, 5
	ands	r4, r4, #0xf
	addne	r1, r1, #CPU_SAVED_MOD_STATE_SIZE
	ldr     r2, =sp_pm_collapse
	adr     r3, sp_pm_collapse
	add     r1, r1, r3
	sub     r0, r1, r2
	ldmfd   r0, {sp, lr}
/*
	msr	cpsr_c, #(PSR_F_BIT | PSR_I_BIT | SYSTEM_MODE)
	ldr     r1, =saved_state_sys
	ldr     r2, =sp_pm_collapse
	adr     r3, sp_pm_collapse
	add     r1, r1, r3
	sub     r0, r1, r2
	ldmfd   r0, {sp, lr}
*/
	msr	cpsr_c, #(PSR_F_BIT | PSR_I_BIT | SVC_MODE)

#if 0
	bl	__cpu_early_init
#endif
	ldr     r1, =saved_state_end
	mrc	p15, 0, r4, c0, c0, 5
	ands	r4, r4, #0xf
	subeq	r1, r1, #CPU_SAVED_STATE_SIZE
	/* subeq	r1, r1, #CPU_SAVED_STATE_SIZE */
	ldr     r2, =sp_pm_collapse_exit
	adr     r3, sp_pm_collapse_exit
	add     r1, r1, r3
	sub     r1, r1, r2

	ldmdb   r1!, {r2-r11}
	mcr     p15, 0, r3, c2, c0, 0 /* TTBR0 */
	mcr     p15, 0, r4, c3, c0, 0 /* dacr */
	mcr     p15, 0, r5, c13, c0, 4 /* TPIDRPRW, thread-id, wong */
	mcr     p15, 0, r6, c10, c2, 0 /* PRRR */
	mcr     p15, 0, r7, c10, c2, 1 /* NMRR */
	mcr     p15, 0, r8, c1, c0, 1 /* ACTLR */
	mcr     p15, 0, r9, c2, c0, 1 /* TTBR1 */
	mcr     p15, 0, r10, c13, c0, 3 /* TPIDRURO */
	mcr     p15, 0, r11, c13, c0, 1 /* context ID */
	isb
	ldmdb   r1!, {r4-r14}

	/* Add 1:1 map in the PMD to allow smooth switch when turning on MMU */
	and     r3, r3, #~0x7F  /* mask off lower 7 bits of TTB */
	adr     r0, sp_pm_mapped_pa /* get address of the mapped instr */
	lsr     r1, r0, #20     /* get the addr range of addr in MB */
	lsl     r1, r1, #2      /* multiply by 4 to get to the pg index */
	add     r3, r3, r1      /* pgd + pgd_index(addr) */
	ldr     r1, [r3]        /* save current entry to r1 */
	lsr     r0, #20         /* align current addr to 1MB boundary */
	lsl     r0, #20
	/* Create new entry for this 1MB page */
	orr     r0, r0, #0x4     /* PMD_SECT_BUFFERED */
	orr     r0, r0, #0x400   /* PMD_SECT_AP_WRITE */
	orr     r0, r0, #0x2     /* PMD_TYPE_SECT|PMD_DOMAIN(DOMAIN_KERNEL) */
	str     r0, [r3]         /* put new entry into the MMU table */
	mcr     p15, 0, r3, c7, c10, 1  /* flush_pmd */
	dsb
	isb
	mcr     p15, 0, r2, c1, c0, 0   /* MMU control */
	isb
sp_pm_mapped_pa:
	/* Switch to virtual */
	adr     r2, sp_pm_pa_to_va
	ldr     r0, =sp_pm_pa_to_va
	mov     pc, r0
sp_pm_pa_to_va:
	sub     r0, r0, r2
	/* Restore r1 in MMU table */
	add     r3, r3, r0
	str     r1, [r3]
	mcr     p15, 0, r3, c7, c10, 1  /* flush_pmd */
	dsb
	isb
	mcr     p15, 0, r3, c8, c7, 0   /* UTLBIALL */
	mcr     p15, 0, r3, c7, c5, 6   /* BPIALL */
	dsb
	isb

#if 1
	/* power on SCU */
	stmfd   sp!, {lr}
	bl	sprd_get_scu_base
	ldmfd   sp!, {lr}
	mov 	r1, #SCU_PM_NORMAL
	stmfd   sp!, {lr}
	bl	scu_power_mode
	ldmfd   sp!, {lr}
#endif
	stmfd   sp!, {lr}
	bl      v7_flush_kern_cache_all
	ldmfd   sp!, {lr}
	mov     r0, #1
#if 0
secondary_cores:
	mrc	p15, 0, r1, c0, c0, 5
	ands	r1, r1, #0xf
	cmp	r1, #0
        ldr     r1, =REG_CPU1_JUMP_PHY_ADDR
        ldr     r1, [r1]
        bxne	r1
#endif		
core0:
#1:	b	1b
	bx      lr
	nop
	nop
	nop
	nop
	nop



/*
	.data
*/
saved_state:
#if (CONFIG_NR_CPUS >= 2)
	.space  CPU_SAVED_STATE_SIZE * CPU_NUM
#else
	.space  4 * 11  /* r4-14 */
	.space  4 * 10  /* cp15 registers */
#endif
saved_state_end:


saved_state_irq:
#if (CONFIG_NR_CPUS >= 2)
	.space  CPU_SAVED_MOD_STATE_SIZE * CPU_NUM
#else
	.space  4 * 2 /* sp,  lr */
#endif
saved_state_irq_end:

saved_state_abt:
#if (CONFIG_NR_CPUS >= 2)
	.space  CPU_SAVED_MOD_STATE_SIZE * CPU_NUM
#else
	.space  4 * 2 /* sp,  lr */
#endif
saved_state_abt_end:

saved_state_und:
#if (CONFIG_NR_CPUS >= 2)
	.space  CPU_SAVED_MOD_STATE_SIZE * CPU_NUM
#else
	.space  4 * 2 /* sp,  lr */
#endif
saved_state_und_end:

saved_state_sys:
#if (CONFIG_NR_CPUS >= 2)
	.space  CPU_SAVED_MOD_STATE_SIZE * CPU_NUM
#else
	.space  4 * 2 /* sp,  lr */
#endif
saved_state_sys_end:

ENTRY(sp_pm_collapse_end)

/*
 * sc8810 standby mode
 *
 * Author: Wang Liwei
 *
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <mach/hardware.h>

	.macro	addruart,rx
	@ see if the MMU is enabled and select appropriate base address
	mrc	p15, 0, \rx, c1, c0
	tst	\rx, #1
	ldreq	\rx, =SPRD_SERIAL1_PHYS
	ldrne	\rx, =SPRD_SERIAL1_BASE
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

ENTRY(sc8810_standby_iram)
#if 0
	/*debug info starts*/
	stmfd sp!, {r4-r12, lr}

	ldr		r1, =0xe0033008
	ldr		r2, [r1, #0]
	orr		r2, r2, #(0x1 << 21)
	str		r2, [r1, #0]

     mov r0, #'@'
     bl printch

     mov r0, #'>'
     bl printch

	ldr r0, =0x2ff
	bl my_delay

	ldr		r1, =0xe0033008
	ldr		r2, [r1, #0]
	bic		r2, r2, #(0x1 << 21)
	str		r2, [r1, #0]
	ldmfd sp!, {r4-r12, lr}
	/*debug info ends*/
#endif

sprd_pm_mapped_va:
	ldr r0, =sprd_pm_va_to_iram
	ldr r1, =sc8810_standby_iram
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
	ldr r1, =AHB_PAUSE
	ldr r2, [r1]
	orr r2, r2, #0x06
	str r2, [r1, #0]

	mov r0, #0x20
1:  sub	r0, r0, #0x1
    cmp	r0, #0x0
    bne  1b
*/

	wfi
	nop
	nop
	nop
    mov pc, lr
    nop
    nop

ENDPROC(sc8810_standby_iram)


ENTRY(sc8810_standby_exit_iram)

	mrs r0, cpsr
	and r0, r0, #0xff
	mov r6, r0
	ldr r1, =0x00d3
	cmp r0, r1
	blne dead
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
	ldr		r1, =0x8b000008	/*physical address. */
	ldr		r2, [r1, #0]
	orr		r2, r2, #(0x1 << 21)
	str		r2, [r1, #0]

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

	ldr	r0, =sp_pm_collapse_exit
	/*
	convert to physical address,
	need to find a more elegent way.
	*/
	ldr	r1, =0xc0000000
	sub	r0, r0, r1
	mov pc, r0
ENDPROC(sc8810_standby_exit_iram)


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
	ldr		r1, =0x8b000008	/*physical address. */
	ldr		r2, [r1, #0]
	orr		r2, r2, #(0x1 << 21)
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

ENTRY(sc8810_standby_iram_end)


ENTRY(sp_pm_collapse)
	msr		cpsr_c, #(PSR_F_BIT | PSR_I_BIT | IRQ_MODE)
	ldr     r0, =saved_state_irq
	stmia   r0!, {sp, lr}

	msr		cpsr_c, #(PSR_F_BIT | PSR_I_BIT | ABT_MODE)
	ldr     r0, =saved_state_abt
	stmia   r0!, {sp, lr}


	msr		cpsr_c, #(PSR_F_BIT | PSR_I_BIT | UND_MODE)
	ldr     r0, =saved_state_und
	stmia   r0!, {sp, lr}

/*
	msr		cpsr_c, #(PSR_F_BIT | PSR_I_BIT | SYSTEM_MODE)
	ldr     r0, =saved_state_sys
	stmia   r0!, {sp, lr}
*/

	msr		cpsr_c, #(PSR_F_BIT | PSR_I_BIT | SVC_MODE)


	ldr     r0, =saved_state
	stmia   r0!, {r4-r14}
	mrc     p15, 0, r1, c1, c0, 0 /* MMU control */
	mrc     p15, 0, r2, c2, c0, 0 /* TTBR0 */
	mrc     p15, 0, r3, c3, c0, 0 /* dacr */
#if 0
	mrc     p15, 3, r4, c15, c0, 3 /* L2CR1 is the L2 cache control reg 1 */
#endif
	mrc     p15, 0, r5, c10, c2, 0 /* PRRR */
	mrc     p15, 0, r6, c10, c2, 1 /* NMRR */
	mrc     p15, 0, r7, c1, c0, 1 /* ACTLR */
	mrc     p15, 0, r8, c2, c0, 1 /* TTBR1 */
	mrc     p15, 0, r9, c13, c0, 3 /* TPIDRURO */
	mrc     p15, 0, ip, c13, c0, 1 /* context ID */
	stmia   r0!, {r1-r9, ip}
#if 0
	bl		outer_cache_poweroff
#endif
	bl      v7_flush_dcache_all

	mrc     p15, 0, r4, c1, c0, 0    /* read current CR    */
	bic     r0, r4, #(1 << 2)        /* clear dcache bit   */
	bic     r0, r0, #(1 << 12)       /* clear icache bit   */
	mcr     p15, 0, r0, c1, c0, 0    /* disable d/i cache  */

	bl sc8810_standby_iram

	mcr     p15, 0, r4, c1, c0, 0    /* restore d/i cache  */
	isb

	ldr     r0, =saved_state         /* restore registers */
	ldmfd   r0, {r4-r14}
	mov     r0, #0                   /* return power collapse failed */
	bx      lr

ENTRY(sp_pm_collapse_exit)
	msr		cpsr_c, #(PSR_F_BIT | PSR_I_BIT | IRQ_MODE)
	ldr     r1, =saved_state_irq
	ldr     r2, =sp_pm_collapse
	adr     r3, sp_pm_collapse
	add     r1, r1, r3
	sub     r0, r1, r2
	ldmfd   r0, {sp, lr}

	msr		cpsr_c, #(PSR_F_BIT | PSR_I_BIT | ABT_MODE)
	ldr     r1, =saved_state_abt
	ldr     r2, =sp_pm_collapse
	adr     r3, sp_pm_collapse
	add     r1, r1, r3
	sub     r0, r1, r2
	ldmfd   r0, {sp, lr}

	msr		cpsr_c, #(PSR_F_BIT | PSR_I_BIT | UND_MODE)
	ldr     r1, =saved_state_und
	ldr     r2, =sp_pm_collapse
	adr     r3, sp_pm_collapse
	add     r1, r1, r3
	sub     r0, r1, r2
	ldmfd   r0, {sp, lr}
/*
	msr		cpsr_c, #(PSR_F_BIT | PSR_I_BIT | SYSTEM_MODE)
	ldr     r1, =saved_state_sys
	ldr     r2, =sp_pm_collapse
	adr     r3, sp_pm_collapse
	add     r1, r1, r3
	sub     r0, r1, r2
	ldmfd   r0, {sp, lr}
*/
	msr		cpsr_c, #(PSR_F_BIT | PSR_I_BIT | SVC_MODE)

#if 0
	bl	__cpu_early_init
#endif
	ldr     r1, =saved_state_end
	ldr     r2, =sp_pm_collapse_exit
	adr     r3, sp_pm_collapse_exit
	add     r1, r1, r3
	sub     r1, r1, r2

	ldmdb   r1!, {r2-r11}
	mcr     p15, 0, r4, c3, c0, 0 /* dacr */
	mcr     p15, 0, r3, c2, c0, 0 /* TTBR0 */
#if 0
	mcr     p15, 3, r5, c15, c0, 3 /* L2CR1 */
#endif
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
	stmfd   sp!, {lr}
	bl      v7_flush_kern_cache_all
	ldmfd   sp!, {lr}
	mov     r0, #1
	bx      lr
	nop
	nop
	nop
	nop
	nop
1:	b       1b


/*
	.data
*/

saved_state:
	.space  4 * 11 /* r4-14 */
	.space  4 * 10  /* cp15 */
saved_state_end:


saved_state_irq:
	.space  4 * 2 /* sp,  lr */
saved_state_irq_end:

saved_state_abt:
	.space  4 * 2 /* sp,  lr */
saved_state_abt_end:

saved_state_und:
	.space  4 * 2 /* sp,  lr */
saved_state_und_end:

saved_state_sys:
	.space  4 * 2 /* sp,  lr */
saved_state_sys_end:

ENTRY(sp_pm_collapse_end)



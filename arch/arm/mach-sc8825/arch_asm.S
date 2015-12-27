/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/vfpmacros.h>

ENTRY(arch_init_neon)
	mrc     p15, 0, r0, c1, c0, 2	/*read CACR into r1*/
	orr      r0, r0, #(0xf << 20)    /*enable full access for p10 & p11*/
	mcr    p15, 0, r0, c1, c0, 2      /*write back into CACR */
	dsb

	/*enable VFP*/
	mov     r0, #0x40000000
	/*fmxr    fpexc, r0 TODO*/
	dsb

	bx      lr

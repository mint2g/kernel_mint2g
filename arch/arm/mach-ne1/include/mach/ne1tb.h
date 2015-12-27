/*
 * linux/asm/arch/ne1tb_fpga.h
 *
 * Copyright (C) NEC Electronics Corporation 2007, 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ASM_ARCH_NE1TB_FPGA_H
#define __ASM_ARCH_NE1TB_FPGA_H


/* FPGA_RTC (base: NE1TB_BASE_RTC) */
#define	RTC_CLKDIV				0x000
#define	RTC_ADR					0x008
#define	RTC_CTRL				0x010
#define	RTC_STS					0x018
#define	RTC_RB					0x020
#define	RTC_WB					0x028

/* FPGA_ETRON (base: NE1TB_BASE_ETRON) */
#define	ETRON_SMR				0x000
#define	ETRON_BRR				0x002
#define	ETRON_SCR				0x004
#define	ETRON_TDR				0x006
#define	ETRON_SSR				0x008
#define	ETRON_RDR				0x00a
#define	ETRON_CMR				0x00c

/* FPGA_MSTLCD01 (base: NE1TB_BASE_MSTLCD01) */
#define	MSTLCD01_LCD_CTRL			0x000
#define	MSTLCD01_LCD_INT			0x008
#define	MSTLCD01_KEY_RL				0x010
#define	MSTLCD01_KEY_SL				0x018
#define	MSTLCD01_TSP_CLKDIV			0x200
#define	MSTLCD01_TSP_START			0x208
#define	MSTLCD01_TSP_XRB			0x210
#define	MSTLCD01_TSP_YRB			0x218

/* FPGA (base: NE1TB_BASE_FPGA) */
#define	FPGA_PCADDRL				0x800
#define	FPGA_PCADDRH				0x802
#define	FPGA_LINT				0x804
#define	FPGA_DET				0x806

#define	FPGA_IDMODEL				0x810
#define	FPGA_IDMODEH				0x810

#define	FPGA_FIO_SEL				0xa00
#define	FPGA_FIO_IN				0xa02
#define	FPGA_FIO_OUT				0xa04
#define	FPGA_FLED				0xa06

#define	FPGA_FG_VER				0xf00
#define	FPGA_FG_TST				0xf08
#define	FPGA_SPDWN				0xf10


#define	FLED_LED0				0x0001
#define	FLED_LED1				0x0002
#define	FLED_LED2				0x0004
#define	FLED_LED3				0x0008


#endif /* __ASM_ARCH_NE1TB_FPGA_H */


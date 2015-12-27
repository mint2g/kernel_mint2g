/*
 * linux/include/asm/arch/ne1_sysctrl.h
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

#ifndef __ASM_ARCH_NE1_SYSCTRL_H
#define __ASM_ARCH_NE1_SYSCTRL_H

#include <mach/hardware.h>

#define SYSCTRL_BASE	IO_ADDRESS(NE1_BASE_SYSCTRL)

#define	SYSCTRL_MEMO				0x000		/* memo */
#define	SYSCTRL_BOOTID				0x004		/* boot ID */
#define	SYSCTRL_RSTOUT				0x008		/* reset out */
#define	SYSCTRL_RSTSTS				0x00c		/* reset status */
#define	SYSCTRL_SRST_CPU			0x010		/* cpu software reset */
#define	SYSCTRL_SRST_PERI			0x014		/* peripheral software reset */
#define	SYSCTRL_RSTOUT_TMR			0x018		/* reset out */

#define	SYSCTRL_CLKMSK				0x080		/* clock mask */
#define	SYSCTRL_CLKDIV_DISP			0x08c		/* divide DISP DCLKI */
#define	SYSCTRL_CLKDIV_SPDIF		0x090		/* divide SPDIF CLKO */
#define	SYSCTRL_CLKDIV_I2S			0x094		/* divide I2S MSCLK */

#define	SYSCTRL_FIQMSK				0x100		/* MPCore FIQ mask */
#define	SYSCTRL_AXICTRL				0x104		/* MPCore AXI port control */
#define	SYSCTRL_MPCSTS				0x108		/* MPCore status */
#define	SYSCTRL_MPCSTS_MON			0x10c		/* MPCore status monitor */

#define	SYSCTRL_PINMUX				0x18c		/* PINMUX GPIO */

/* CLKMASK register bit */
#define SYSCTRL_SRST_PERI_PERI	(1 << 16)
#define SYSCTRL_SRST_PERI_VIDEO	(1 << 2)
#define SYSCTRL_SRST_PERI_DISP	(1 << 1)
#define SYSCTRL_SRST_PERI_SGX	(1 << 0)

/* CLKMASK register bit */
#define SYSCTRL_CLKMSK_SPDCLKO_IN	(1 << 22)
#define SYSCTRL_CLKMSK_I2S3			(1 << 21)
#define SYSCTRL_CLKMSK_I2S2			(1 << 20)
#define SYSCTRL_CLKMSK_I2S1			(1 << 19)
#define SYSCTRL_CLKMSK_I2S0			(1 << 18)
#define SYSCTRL_CLKMSK_SPDCLKO		(1 << 17)
#define SYSCTRL_CLKMSK_SPDMCLK		(1 << 16)
#define SYSCTRL_CLKMSK_PCI			(1 << 9)
#define SYSCTRL_CLKMSK_I2C			(1 << 8)
#define SYSCTRL_CLKMSK_DISP			(1 << 1)
#define SYSCTRL_CLKMSK_IDE			(1 << 0)

#endif /* __ASM_ARCH_NE1_SYSCTRL_H */


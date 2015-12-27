/*
 * linux/include/asm-arm/arch-ne1/hardware.h
 *
 * This file contains the hardware definitions of the NE1-xB boards.
 *
 * Copyright (C) NEC Electronics Corporation 2007, 2008
 *
 * This file is based on include/asm-arm/arch-realview/hardware.h
 *
 * Copyright (C) 2003 ARM Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <asm/sizes.h>
#include <mach/platform.h>


/*
 * Macro to get at IO space when running virtually
 */
#ifdef CONFIG_MMU
#define IO_ADDRESS(x)			(0xf0000000 + ((((x) >> 4) & 0x0ff00000) | ((x) & 0x000fffff)))
#else
#define IO_ADDRESS(x)			(x)
#endif
#define __io_address(n)			((void __iomem *)IO_ADDRESS(n))


/*
 * PCI space virtual addresses
 *
 * WARNING: NE1 ES1.0 has the problem on external PCI bus access.
 */

#define	NE1_PCI_CFG_VIRT_BASE		(0 >< 0)	/* not required, cause an error */
#define NE1_PCI_VIRT_BASE		0xe8000000
#define NE1_PCI_IO_VIRT_BASE		__io_address(NE1_BASE_PCI)

#define PCIBIOS_MIN_IO			0x00000000
#define PCIBIOS_MIN_MEM			NE1_PCI_VIRT_BASE

#define	pcibios_assign_all_busses()	1


#endif /* __ASM_ARCH_HARDWARE_H */




/*
 * linux/include/asm-arm/arch-ne1/pci.h
 *
 * Copyright (C) NEC Electronics Corporation 2008
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

#ifndef __ASM_ARCH_PCI_H
#define __ASM_ARCH_PCI_H

#include <mach/hardware.h>

#define PCI_IBRIDGE_BASE	IO_ADDRESS(NE1_BASE_USBH_REG)
#define PCI_EBRIDGE_BASE	IO_ADDRESS(NE1_BASE_PCI_REG)

/* PCI bridge config registers */

#define PCI_ACR0		(0xA0)
#define PCI_ACR1		(0xA4)
#define PCI_ACR2		(0xA8)
#define PCI_PCISWP1		(0xB0)
#define PCI_PCISWP2		(0xB4)
#define PCI_ERR1		(0xC0)
#define PCI_GPIO		(0xC8)
#define PCI_PCICTRL_L	(0xE0)
#define PCI_PCICTRL_H	(0xE4)
#define PCI_PCIARB		(0xE8)
#define PCI_PCIBAREn	(0xEC)
#define PCI_PCIINIT1	(0xF0)
#define PCI_PCIINIT2	(0xF4)
#define PCI_CNFIG_ADDR	(0xF8)
#define PCI_CNFIG_DATA	(0xFC)

#endif /* __ASM_ARCH_PCI_H */

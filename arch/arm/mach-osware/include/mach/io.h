/*
 *  linux/arch/arm/mach-osware/include/mach/io.h
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
 */

#ifndef __ASM_ARCH_OSWARE_IO_H
#define __ASM_ARCH_OSWARE_IO_H

#define IO_SPACE_LIMIT   0xffffffff

#define __io(a)          ((void __iomem *)(a))
#define __mem_pci(a)	 (a)
#define __mem_isa(a)	 (a)

#endif /* __ASM_ARCH_OSWARE_IO_H */

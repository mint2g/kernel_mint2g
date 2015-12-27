/*
 *  linux/arch/arm/mach-ne1/include/mach/memory.h
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

#ifndef __ASM_ARCH_OSWARE_MEMORY_H
#define __ASM_ARCH_OSWARE_MEMORY_H

/*
 * Physical DRAM offset.
 */
#define PHYS_OFFSET         0x80000000

/*
 * Bus address is physical address.
 */
#define __phys_to_bus(x)        (x)
#define __bus_to_phys(x)        (x)

#define __virt_to_bus(v)    __phys_to_bus(__virt_to_phys(v))
#define __bus_to_virt(b)    __phys_to_virt(__bus_to_phys(b))
#define __pfn_to_bus(p)     __phys_to_bus(__pfn_to_phys(p))
#define __bus_to_pfn(b)     __phys_to_pfn(__bus_to_phys(b))

#endif /* __ASM_ARCH_OSWARE_MEMORY_H */

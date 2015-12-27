/* arch/arm/mach-sc8800g/include/mach/io.h
 *
 * Copyright (C) 2010 Spreadtrum
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

/* physical offset of RAM */
#define PHYS_OFFSET		UL(0x0)

/* bus address and physical addresses are identical */
#define __virt_to_bus(x)	__virt_to_phys(x)
#define __bus_to_virt(x)	__phys_to_virt(x)

#define __pfn_to_bus(x) 	__pfn_to_phys(x)
#define __bus_to_pfn(x)		__phys_to_pfn(x)

#endif


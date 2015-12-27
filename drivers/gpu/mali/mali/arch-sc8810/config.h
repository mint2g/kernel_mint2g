/*
 * Copyright (C) 2010-2012 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __ARCH_CONFIG_H__
#define __ARCH_CONFIG_H__

#include "base.h"

/* Configuration for the PB platform with ZBT memory enabled */

static _mali_osk_resource_t arch_configuration [] =
{
#if 1
	{
		.type = PMU,
		.description = "Mali-300 PMU",
		.base = 0xA0012000,
		.irq = 25,
		.mmu_id = 0

	},
#endif
	{
		.type = MALI300GP,
		.description = "Mali-300 GP",
		.base = 0xA0010000,
		.irq = 25,
		.mmu_id = 1
	},
	{
		.type = MALI300PP,
		.base = 0xA0018000,
		.irq = 25,
		.description = "Mali-300 PP",
		.mmu_id = 2
	},
#if USING_MMU
	{
		.type = MMU,
		.base = 0xA0013000,
		.irq = 25,
		.description = "Mali-300 MMU for GP",
		.mmu_id = 1
	},
	{
		.type = MMU,
		.base = 0xA0014000,
		.irq = 25,
		.description = "Mali-300 MMU for PP",
		.mmu_id = 2
	},
#endif
#if 0
	{
		.type = MEMORY,
		.description = "Mali SDRAM remapped to baseboard",
		.cpu_usage_adjust = -0x50000000,
		.alloc_order = 0, /* Highest preference for this memory */
		.base = 0xD0000000,
		.size = 0x10000000,
		.flags = _MALI_CPU_WRITEABLE | _MALI_CPU_READABLE | _MALI_PP_READABLE | _MALI_PP_WRITEABLE |_MALI_GP_READABLE | _MALI_GP_WRITEABLE
	},
	{
		.type = MEMORY,
		.description = "Mali ZBT",
		.alloc_order = 5, /* Medium preference for this memory */
		.base = 0xe1000000,
		.size = 0x01000000,
		.flags = _MALI_CPU_WRITEABLE | _MALI_CPU_READABLE | _MALI_PP_READABLE | _MALI_PP_WRITEABLE |_MALI_GP_READABLE | _MALI_GP_WRITEABLE
	},
#endif
#if 0
	{
		.type = MEM_VALIDATION,
		.description = "Framebuffer",
		.base = 0xe0000000,
		.size = 0x01000000,
		.flags = _MALI_CPU_WRITEABLE | _MALI_CPU_READABLE | _MALI_MMU_WRITEABLE | _MALI_MMU_READABLE
	},
#endif
#if 0
	{
		.type = MEMORY,
		.description = "Videobuffer",
		.base = 0x04000000,
		.size = 0x02000000,
		.flags = _MALI_CPU_WRITEABLE | _MALI_CPU_READABLE | _MALI_PP_WRITEABLE | _MALI_PP_READABLE | _MALI_GP_WRITEABLE | _MALI_GP_READABLE
	},
#endif
#if 1
	{
		.type = OS_MEMORY,
		.description = "OS Memory",
//		.base = 0x00000000,
		.size = ARCH_MALI_MEMORY_SIZE_DEFAULT >> 1,
		.flags = _MALI_CPU_WRITEABLE | _MALI_CPU_READABLE | _MALI_MMU_WRITEABLE | _MALI_MMU_READABLE
	},
#endif
	{
		.type = MALI300L2,
		.base = 0xA0011000,
		.description = "Mali-300 L2 cache"
	},
};

#endif /* __ARCH_CONFIG_H__ */

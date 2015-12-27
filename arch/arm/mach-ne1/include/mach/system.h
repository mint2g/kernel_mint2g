/*
 *  linux/arch/arm/mach-ne1/include/mach/system.h
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

#ifndef __ASM_ARCH_OSWARE_SYSTEM_H
#define __ASM_ARCH_OSWARE_SYSTEM_H

static inline void arch_idle(void)
{
	cpu_do_idle();
}

static inline void arch_reset(char mode, const char* cmd)
{
}

#endif /* __ASM_ARCH_OSWARE_SYSTEM_H */

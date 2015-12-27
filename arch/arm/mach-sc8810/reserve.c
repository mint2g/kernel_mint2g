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

#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/page.h>
#include <asm/mach/map.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <linux/memblock.h>

#ifdef CONFIG_SC8810_DDR_6G
/*
	reserved memory from 0x30000000  - 0x3fffffff
	reserved memory from 0xe0000000  - 0xefffffff
	reserved method:
	For every continue 8K, that one 4k has been reserved by ASIC, so SW can choose continue 8k to 
	reserved. like this:
		0k-4k
		4k-8k ---> reserved
		8k-12k---> reserved
		12k-16k
		16k-20k
		20k-24k -->reserved
		24k-28k -->reserved
*/
static int __init __reserve_memblock(phys_addr_t addr_base, phys_addr_t size)
{
	phys_addr_t offset = SZ_4K;
	phys_addr_t i = 0;
	for (; i < size; i+= SZ_16K ) {
#if 0 //ignore this to decrease power on time(nearly 8~9 seconds)
/*FIXME: for optimize, we can ignor this*/
		if (memblock_is_region_reserved(addr_base + i + offset, SZ_8K)){
			BUG_ON(1);
			return -EBUSY;
		}
#endif
		if (memblock_reserve(addr_base + i + offset, SZ_8K))
			return -ENOMEM;         
	}

	return 0;
}

static int __init sc8810_6Gb_reserve_memblock(void)
{
	int ret;
	if (ret = __reserve_memblock(0x30000000, SZ_256M))
		return ret;

	if (ret = __reserve_memblock(0xe0000000, SZ_256M))
		return ret;     

	return ret;
}
#endif


int __init sc8810_pmem_reserve_memblock(void)
{
	if (memblock_is_region_reserved(SPRD_PMEM_BASE, SPRD_IO_MEM_SIZE))
		return -EBUSY;
	if (memblock_reserve(SPRD_PMEM_BASE, SPRD_IO_MEM_SIZE))
		return -ENOMEM;
	return 0;
}

#ifdef CONFIG_ANDROID_RAM_CONSOLE
int __init sc8810_ramconsole_reserve_memblock(void)
{
	if (memblock_is_region_reserved(SPRD_RAM_CONSOLE_START, SPRD_RAM_CONSOLE_SIZE))
		return -EBUSY;
	if (memblock_reserve(SPRD_RAM_CONSOLE_START, SPRD_RAM_CONSOLE_SIZE))
		return -ENOMEM;
	return 0;
}
#endif

void __init sc8810_reserve(void)
{
	int ret;
	if (ret = sc8810_pmem_reserve_memblock())
		pr_err("Fail to reserve mem for pmem. errno=%d\n", ret);

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	if (ret = sc8810_ramconsole_reserve_memblock())
		pr_err("Fail to reserve mem for pmem. errno=%d\n", ret);
#endif

#ifdef CONFIG_SC8810_DDR_6G
	if (ret = sc8810_6Gb_reserve_memblock())
		pr_err("Fail to reserve mem for 6Gb. errno=%d\n", ret);
#endif
}

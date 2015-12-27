/*
 * Copyright (C) 2010-2011 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/system.h>
#include <asm/io.h>

#include <mach/globalregs.h>

#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_platform.h"

_mali_osk_errcode_t mali_platform_init(void)
{
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, BIT(23), GR_G3D_PWR_CTRL);
	udelay(100);
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, BIT(21), AHB_CTL0);
	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, BIT(21), AHB_CTL0);
	sprd_greg_set_bits(REG_TYPE_GLOBAL, BIT(23), GR_G3D_PWR_CTRL);
	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_power_mode_change(mali_power_mode power_mode)
{
	switch(power_mode)
	{
	case MALI_POWER_MODE_ON:
		sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, BIT(21), AHB_CTL0);
		udelay(100);
		//Confirmed to remove from SPRD, sejong123.park
		//pr_debug("mali: clock up done\n");
		break;
	case MALI_POWER_MODE_LIGHT_SLEEP:
		sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, BIT(21), AHB_CTL0);
		//pr_debug("mali: clock down done\n");
		break;
	case MALI_POWER_MODE_DEEP_SLEEP:
		break;
	};
	MALI_SUCCESS;
}

void mali_gpu_utilization_handler(u32 utilization)
{
}

void set_mali_parent_power_domain(void* dev)
{
}


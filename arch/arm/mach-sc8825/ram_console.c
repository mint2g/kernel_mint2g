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
#include <linux/platform_device.h>

#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <mach/board.h>

static struct platform_device *ram_console;

int __init sprd_ramconsole_init(void)
{
	int err;
	struct resource res = { .flags = IORESOURCE_MEM };

	if (!(ram_console = platform_device_alloc("ram_console", 0))) {
		pr_err("ram console Failed to allocate device \n");
		err = -ENOMEM;
		goto exit;
	}

	res.start = SPRD_RAM_CONSOLE_START;
	res.end = (SPRD_RAM_CONSOLE_START + SPRD_RAM_CONSOLE_SIZE - 1);
	pr_info("alloc resouce for ramconsole: start:%x, size:%d\n",
		res.start, SPRD_RAM_CONSOLE_SIZE);
	if ((err = platform_device_add_resources(ram_console, &res, 1))) {
		pr_err("ram console:Failed to add device resource (err = %d).\n", err);
		goto exit_device_put;
	}

	if ((err = platform_device_add(ram_console))) {
		pr_err("ram console: Failed to add device (err = %d).\n", err);
		goto exit_device_put;
	}

	return 0;
exit_device_put:
	platform_device_put(ram_console);
	ram_console = NULL;
exit:
	return err;
}


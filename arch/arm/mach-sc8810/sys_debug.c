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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/bug.h>
#include <mach/system.h>
#ifdef CONFIG_DEBUG_FS
extern void cp_abort(void);
extern void sprd_rtc_set_spg_counter(u16 value);
extern u16 sprd_rtc_get_spg_counter(void);
extern void sprd_rtc_hwrst_set(u16 value);
extern u16 sprd_rtc_hwrst_get(void);
extern int sec_debug_level(void);

static int debugfs_make_kernel_panic(void *data, u64 val)
{
	if (val > 0)         
	{
#ifdef CONFIG_SEC_DEBUG
	if (sec_debug_level())
               {
                       cp_abort();
               }
#else
                        arch_reset(0, 0);
                        while(1);
#endif
	}	
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(do_panic_fops, NULL, debugfs_make_kernel_panic,"%llu\n");

static int debugfs_set_rtc_spg_counter(void *data, u64 val)
{
	u16 i = val&0xffff;

	sprd_rtc_set_spg_counter(i);

	return 0;
}

static int debugfs_get_rtc_spg_counter(void *data, u64 *val)
{
	*val = sprd_rtc_get_spg_counter();

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(rtc_sgp_cnt_fops, debugfs_get_rtc_spg_counter, debugfs_set_rtc_spg_counter, "%llu\n");


static int debugfs_set_hwrst_rtc(void *data, u64 val)
{
	u16 i = val&0xffff;

	sprd_rtc_hwrst_set(i);

	return 0;
}

static int debugfs_get_hwrst_rtc(void *data, u64 *val)
{
	*val = sprd_rtc_hwrst_get();

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(hwrst_rtc_fops, debugfs_get_hwrst_rtc, debugfs_set_hwrst_rtc,"%llu\n");


void create_sys_debugfs(void)
{
	struct dentry *root = NULL;

	root = debugfs_create_dir("system", NULL);
	if (IS_ERR(root) || !root) {
		return;
	}

	if (!debugfs_create_file("dopanic", S_IWUSR, root, NULL, &do_panic_fops))
		goto err_create;

	if (!debugfs_create_file("rtc_spg_cnt", S_IWUSR|S_IRUGO, root, NULL, &rtc_sgp_cnt_fops))
		goto err_create;

	if (!debugfs_create_file("hwrst_rtc", S_IWUSR|S_IRUGO, root, NULL, &hwrst_rtc_fops))
		goto err_create;

	return;

err_create:
	debugfs_remove_recursive(root);
	pr_err("failed to create debugfs: do-panic\n");
}

#endif

void sys_debug_init(void)
{
#ifdef CONFIG_DEBUG_FS
	create_sys_debugfs();
#endif
}



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
#ifndef _ISP_CONTROL_H_
#define _ISP_CONTROL_H_
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <linux/file.h>
#include <linux/slab.h>

struct dcam_cnt_ctrl {
	uint32_t user_cnt;
	struct semaphore ctrl_sem;
	struct semaphore ctrl_path2_sem;
};

#define init_MUTEX(sem)		sema_init(sem, 1)

void dcam_inc_user_count(void);
void dcam_dec_user_count(void);
uint32_t dcam_get_user_count(void);
void dcam_clear_user_count(void);
void isp_get_path2(void);
void isp_put_path2(void);
void isp_mutex_init(void);
#endif

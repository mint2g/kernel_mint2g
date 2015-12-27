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
#include "isp_control.h"

static struct dcam_cnt_ctrl s_dcam_cnt_ctrl_info;
void dcam_inc_user_count(void)
{
	down(&s_dcam_cnt_ctrl_info.ctrl_sem);
	s_dcam_cnt_ctrl_info.user_cnt++;
	up(&s_dcam_cnt_ctrl_info.ctrl_sem);
	pr_debug("DCAM:dcam_inc_user_count: count: %d.\n",
		 s_dcam_cnt_ctrl_info.user_cnt);
}

void dcam_dec_user_count(void)
{
	down(&s_dcam_cnt_ctrl_info.ctrl_sem);
	if (s_dcam_cnt_ctrl_info.user_cnt >= 1)
		s_dcam_cnt_ctrl_info.user_cnt--;
	else
		s_dcam_cnt_ctrl_info.user_cnt = 0;
	up(&s_dcam_cnt_ctrl_info.ctrl_sem);
	pr_debug("DCAM:dcam_dec_user_count: count: %d.\n",
		 s_dcam_cnt_ctrl_info.user_cnt);
}

uint32_t dcam_get_user_count(void)
{
	uint32_t ret_cnt;

	down(&s_dcam_cnt_ctrl_info.ctrl_sem);
	ret_cnt = s_dcam_cnt_ctrl_info.user_cnt;
	up(&s_dcam_cnt_ctrl_info.ctrl_sem);
	pr_debug("DCAM:dcam_get_user_count: count: %d.\n", ret_cnt);

	return ret_cnt;
}

void dcam_clear_user_count(void)
{
	down(&s_dcam_cnt_ctrl_info.ctrl_sem);
	s_dcam_cnt_ctrl_info.user_cnt = 0;
	up(&s_dcam_cnt_ctrl_info.ctrl_sem);
}

void isp_get_path2(void)
{
	down(&s_dcam_cnt_ctrl_info.ctrl_path2_sem);
}

void isp_put_path2(void)
{
	up(&s_dcam_cnt_ctrl_info.ctrl_path2_sem);
}

void isp_mutex_init(void)
{
	init_MUTEX(&s_dcam_cnt_ctrl_info.ctrl_path2_sem);
	init_MUTEX(&s_dcam_cnt_ctrl_info.ctrl_sem);
	s_dcam_cnt_ctrl_info.user_cnt = 0;
}

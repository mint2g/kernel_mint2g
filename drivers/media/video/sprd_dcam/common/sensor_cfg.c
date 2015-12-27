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
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <linux/list.h>
#include "sensor_drv.h"
#include "sensor_cfg.h"

static LIST_HEAD(main_sensor_info_list);	/*for back camera */
static LIST_HEAD(sub_sensor_info_list);	/*for front camera */
static LIST_HEAD(atv_info_list);	/*for atv */
static DEFINE_MUTEX(sensor_mutex);

int dcam_register_sensor_drv(struct sensor_drv_cfg *cfg)
{
	printk(KERN_INFO "Sensor driver is %s.\n", cfg->sensor_name);
	mutex_lock(&sensor_mutex);
	if (cfg->sensor_pos == 1) {
		list_add_tail(&cfg->list, &main_sensor_info_list);
	} else if (cfg->sensor_pos == 2) {
		list_add_tail(&cfg->list, &sub_sensor_info_list);
	} else if (cfg->sensor_pos == 3) {
		list_add_tail(&cfg->list, &main_sensor_info_list);
		list_add_tail(&cfg->list, &sub_sensor_info_list);
	} else {
		list_add_tail(&cfg->list, &atv_info_list);
	}
	mutex_unlock(&sensor_mutex);

	return 0;
}

struct list_head *Sensor_GetList(SENSOR_ID_E sensor_id)
{
	struct list_head *sensor_list = 0;

	pr_debug("sensor cfg:Sensor_GetList,id=%d.\n", sensor_id);
	switch (sensor_id) {
	case SENSOR_MAIN:
		sensor_list = &main_sensor_info_list;
		break;
	case SENSOR_SUB:
		sensor_list = &sub_sensor_info_list;
		break;
	case SENSOR_ATV:
		sensor_list = &atv_info_list;
		break;
	default:
		printk("sensor cfg:Sensor_GetList fail!\n");
		break;
	}

	return sensor_list;
}

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


#ifndef __LINUX_GPSCTL_H__
#define __LINUX_GPSCTL_H__

#include <linux/ioctl.h>

#define GPSCTL_IOM            'g'

#define GPSCTL_IOC_SET_POWER           _IOW(GPSCTL_IOM, 0x00, short)
#define GPSCTL_IOC_SET_CLK             _IOW(GPSCTL_IOM, 0x01, short)
#define GPSCTL_IOC_RESET               _IOW(GPSCTL_IOM, 0x02, short)
#define GPSCTL_IOC_ONOFF               _IOW(GPSCTL_IOM, 0x03, short)

struct platform_gpsctl_data {
	unsigned int reset_pin;
	unsigned int onoff_pin;
	unsigned int power_pin;
	char *clk_type;
	char *pwr_type;
};

#endif

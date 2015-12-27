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

#ifndef __GPIO_Z788_H__
#define __GPIO_Z788_H__

#ifndef __ASM_ARCH_BOARD_H
#error  "Don't include this file directly, include <mach/board.h>"
#endif

/*
 * GPIO NR:
 *   0   - 15  : D-Die EIC
 *   16  - 159 : D-Die GPIO
 *   160 - 175 : A-Die EIC
 *   176 - 207 : A-Die GPIO
 */
#define GPIO_TOUCH_RESET	59
#define GPIO_TOUCH_IRQ		92
#define GPIO_PLSENSOR_IRQ	91
#define MSENSOR_DRDY_GPIO      28

#define GPIO_MAIN_SENSOR_PWN    73
#define GPIO_SUB_SENSOR_PWN       74

#define GPIO_BT_RESET       90
#define GPIO_BT_POWER       90
#define GPIO_BT2AP_WAKE     101
#define GPIO_AP2BT_WAKE     103

#define GPIO_WIFI_SHUTDOWN	140
#define GPIO_WIFI_IRQ		142
#define EIC_CHARGER_DETECT	162
#define EIC_KEY_POWER		163
#define SPI0_CMMB_CS_GPIO  	32
#define GPIO_SENSOR_RESET	72

#define GPIO_GPS_RESET          26
#define GPIO_GPS_ONOFF          95
#define GPIO_GPS_POWER          137

#endif

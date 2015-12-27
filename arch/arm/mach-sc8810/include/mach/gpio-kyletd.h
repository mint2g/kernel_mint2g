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

#ifndef __GPIO_SC8810_H__
#define __GPIO_SC8810_H__

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
#define GPIO_I2C_SCL            18
#define GPIO_I2C_SDA            59

#define GPIO_SENSOR_RESET	72
#define GPIO_MAIN_SENSOR_PWN    73
#define GPIO_SUB_SENSOR_PWN     74

#define GPIO_BT_RESET       	140
#define GPIO_BT_POWER       	141
#define GPIO_BT2AP_WAKE     	25
#define GPIO_AP2BT_WAKE     	26

#define GPIO_WIFI_SHUTDOWN	144
#define GPIO_WIFI_IRQ		139

#define EIC_CHARGER_DETECT	162
#define EIC_KEY_POWER		163

#define SPI0_WIFI_CS_GPIO  	32	
#define SPI0_WIFI_IRQ_GPIO  	33	
#define SPI1_CMMB_CS_GPIO  	44

#define GPIO_BK		        138

#define GPIO_SDIO_DETECT	137

#define HEADSET_DETECT_GPIO	0
#define HEADSET_PA_CTL_GPIO	93
#define GPIO_GPS_RESET          28
#define GPIO_GPS_ONOFF          27


#define GPIO_CMMB_RESET         67
#define GPIO_CMMB_INT           65
#define GPIO_CMMB_EN            66
#define GPIO_BQ27425_LOW_BAT	68

#define GPIO_GPS_RESET          28
#define GPIO_GPS_ONOFF          27
#endif

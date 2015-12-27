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

#define GPIO_INVALID		0xFFFFFFFF

/*
 * GPIO NR:
 *   0   - 15  : D-Die EIC
 *   16  - 159 : D-Die GPIO
 *   160 - 175 : A-Die EIC
 *   176 - 207 : A-Die GPIO
 */

#define GPIO_BT_RESET       90
#define GPIO_BT_POWER       136
#define GPIO_BT2AP_WAKE     94
#define GPIO_AP2BT_WAKE     140

#define GPIO_WIFI_SHUTDOWN	137
#define GPIO_WIFI_IRQ		142
#define GPIO_TOUCH_RESET	16
#define GPIO_TOUCH_IRQ		17
#define GPIO_PLSENSOR_IRQ	28
#define MSENSOR_DRDY_GPIO       97

#define EIC_CHARGER_DETECT	162
#define EIC_KEY_POWER		163

#define SPI0_CMMB_CS_GPIO  	32
#define SPI1_WIFI_CS_GPIO  	44
#define GPIO_SENSOR_RESET	72
#define GPIO_MAIN_SENSOR_PWN    73
#define GPIO_SUB_SENSOR_PWN     74

#define HEADSET_DETECT_GPIO	141
#define HEADSET_PA_CTL_GPIO	93
#define GPIO_GPS_RESET          26
#define GPIO_GPS_ONOFF          27

#define GPIO_BK		143


#define GPIO_AP_TO_CP_RTS	38	/*ap REG_PIN_MTRST_N*/ /*cp gpio 73*/
#define GPIO_CP_TO_AP_RDY	37	/*ap REG_PIN_MTMS*/ /*cp gpio 92*/
#define GPIO_CP_TO_AP_RTS	99	/*ap REG_PIN_RFCTL9*/ /*cp gpio 87*/
#define GPIO_AP_STATUS		25	/*ap REG_PIN_KEYOUT5*/ /*cp gpio 105*/

#define GPIO_MODEM_POWER	106	/*ap REG_PIN_XTL_EN*/
#define GPIO_MODEM_DETECT	36	/*ap REG_PIN_MTCK*/ /*cp gpio 94*/
#define GPIO_MODEM_BOOT		92	/*ap REG_PIN_RFCTL2*/ /*cp gpio 86*/
#define GPIO_MODEM_CRASH	40	/*ap REG_PIN_EMRST_N*/ /*cp gpio 85*/

#endif

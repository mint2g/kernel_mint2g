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

#define GPIO_TOUCH_RESET	59
#define GPIO_TOUCH_IRQ		60
#define GPIO_PLSENSOR_IRQ	28
#define MSENSOR_DRDY_GPIO       97

#define EIC_CHARGER_DETECT	162
#define EIC_KEY_POWER		163

#define SPI0_CMMB_CS_GPIO  	32

#define GPIO_SENSOR_RESET	72
#define GPIO_MAIN_SENSOR_PWN    73
#define GPIO_SUB_SENSOR_PWN     74

#define GPIO_GPS_RESET          26
#define GPIO_GPS_ONOFF          27


/*ap: gpio208-223*/
/*cp: gpio 0 - 15*/
#define GPIO_AP_TO_CP_RTS	208	/*cp gpio 0*/
#define GPIO_CP_TO_AP_RDY	209	/*cp gpio 1*/
#define GPIO_CP_TO_AP_RTS	210	/*cp gpio 2*/
#define GPIO_AP_STATUS		211	/*cp gpio 3*/

#define GPIO_MODEM_DETECT	212	/*cp gpio 4*/
#define GPIO_MODEM_BOOT		213	/*cp gpio 5*/
#define GPIO_MODEM_CRASH	214	/*cp gpio 6*/

#endif

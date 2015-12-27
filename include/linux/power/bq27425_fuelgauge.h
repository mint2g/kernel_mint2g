/*
 * Copyright (C) 2011, SAMSUNG Corporation.
 * Author: YongTaek Lee  <ytk.lee@samsung.com> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BQ27425_FUELGAUGE_H
#define __BQ27425_FUELGAUGE_H

/* BQ27425 register map */
#define	BQ27425_CNTL		0x0
#define BQ27425_TEMP		0x2
#define BQ27425_VOLT		0x4
#define BQ27425_FLAGS		0x6
#define	BQ27425_NAC		0x8
#define BQ27425_FAC		0xA
#define BQ27425_RM		0xC
#define	BQ27425_FCC		0xE
#define	BQ27425_AI		0x10
#define	BQ27425_SI		0x12
#define	BQ27425_MLI		0x14
#define	BQ27425_AP		0x18
#define	BQ27425_SOC		0x1C
#define	BQ27425_ITEMP		0x1E
#define	BQ27425_SOH		0x20 

#define BQ27425_DATA_FLASH_CLASS	0x3E
#define BQ27425_DATA_FLASH_BLOCK	0x3F

#define BQ27425_DATA_FLASH_DEFAULT_OFFSET	0x40

#define BQ27425_FIRMWARE_NUMBER			0x3A
#define BQ27425_FIRMWARE_NUMBER_OFFSET		0x0
#define BQ27425_OP_CONFIG		0x52
#define	BQ27425_OP_CONFIG_OFFSET	0x5

#define BQ27425_BLOCK_DATA_CHECKSUM	0x60
#define BQ27425_BLOCK_DATA_CONTROL	0x61

#define BQ27425_WRITE_VERIFY	0x66

#define BQ27425_SAMSUNG_OPCONFIG	0xA1FC

#define BQ27425_MAX_VOLTAGE	4300	/* 4.3V */ 

#define BQ27425_RESET_DELAY	3000	/* 3 seconds needed to rest fuel gauge */

#if defined(CONFIG_BQ27425_READ_VF)
/* BQ27425_FLAGS register */
#define BQ27425_FLAGS_BAT_DET	(1 << 3)

#define BQ27425_NO_BATTERY	4096
#define BQ27425_WITH_BATTERY	300			
#endif 

#if defined(CONFIG_BQ27425_SOC_COMPENSATION_FOR_DISCHARGING)
#define	BQ27425_SOC_COMPENSATION_THRESHOLD_FOR_DISCHARGING	10	/* less than 10% */
#endif

#endif

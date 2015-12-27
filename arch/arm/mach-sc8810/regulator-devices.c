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

#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>

#include <mach/regulator.h>

#include "regulator-devices.h"

static struct regulator_init_status regulators_init_status[];


/*	[ VDDARM ]
	DCDC		: 1.2V Power supply for ARM
	Fix consumers	: ARM Core
	after reset	: ON
*/
static struct regulator_consumer_supply vddarm_consumers[] = {
	CONSUMERS_VDDARM
};

static struct regulator_init_data vddarm_data = {
	.constraints	= {
		.min_uV	= 550000,
		.max_uV	= 1400000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddarm_consumers),
	.consumer_supplies		= vddarm_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDARM],
};


/*	[ VDD25 ]
	LDO		: 2.5V Power supply for PLL and Efuse
	Fix consumers	: PLL and Efuse
	after reset	: ON
*/
static struct regulator_consumer_supply vdd25_consumers[] = {
	CONSUMERS_VDD25
};

static struct regulator_init_data vdd25_data = {
	.constraints	= {
		.min_uV	= 2400000,
		.max_uV	= 3100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vdd25_consumers),
	.consumer_supplies		= vdd25_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDD25],
};


/*	[ VDD18 ]
	LDO		: 1.8V Power supply for IO
	Ref consumers	: IO/ NAND Flash/LCM
	after reset	: ON
*/
static struct regulator_consumer_supply vdd18_consumers[] = {
	CONSUMERS_VDD18
};

static struct regulator_init_data vdd18_data = {
	.constraints	= {
		.min_uV	= 1150000,
		.max_uV	= 2900000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vdd18_consumers),
	.consumer_supplies		= vdd18_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDD18],
};


/*	[ VDD28 ]
	LDO		: 2.8V Power supply for IO
	Ref consumers	: IO/ NAND Flash/LCM
	after reset	: ON
*/
static struct regulator_consumer_supply vdd28_consumers[] = {
	CONSUMERS_VDD28
};

static struct regulator_init_data vdd28_data = {
	.constraints	= {
		.min_uV	= 1750000,
		.max_uV	= 3100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vdd28_consumers),
	.consumer_supplies		= vdd28_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDD28],
};


/*	[ AVDDBB ]
	LDO		: 3.0V Power supply for Analog base-band RX/TX
	Fix consumers	: BB circuit: for example ADC/DAC/APC
	after reset	: ON
*/
static struct regulator_consumer_supply avddbb_consumers[] = {
	CONSUMERS_AVDDBB
};

static struct regulator_init_data avddbb_data = {
	.constraints	= {
		.min_uV	= 2700000,
		.max_uV	= 3200000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(avddbb_consumers),
	.consumer_supplies		= avddbb_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_AVDDBB],
};


/*	[ VDDRF0 ]
	LDO		: 2.85V Power supply for RF
	Fix consumers	: RF/TCXO
	after reset	: ON
*/
static struct regulator_consumer_supply vddrf0_consumers[] = {
	CONSUMERS_VDDRF0
};

static struct regulator_init_data vddrf0_data = {
	.constraints	= {
		.min_uV	= 1750000,
		.max_uV	= 3050000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddrf0_consumers),
	.consumer_supplies		= vddrf0_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDRF0],
};


/*	[ VDDRF1 ]
	LDO		: 2.85V Power supply for RF
	Ref consumers	: external terminal
	after reset	: OFF
*/
static struct regulator_consumer_supply vddrf1_consumers[] = {
	CONSUMERS_VDDRF1
};

static struct regulator_init_data vddrf1_data = {
	.constraints	= {
		.min_uV	= 1750000,
		.max_uV	= 3050000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddrf1_consumers),
	.consumer_supplies		= vddrf1_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDRF1],
};


/*	[ VDDMEM ]
	LDO		: 1.8V Power supply for SRAM/SDRAM memories
	Ref consumers	: SDRAM
	after reset	: ON
*/
static struct regulator_consumer_supply vddmem_consumers[] = {
	CONSUMERS_VDDMEM
};

static struct regulator_init_data vddmem_data = {
	.constraints	= {
		.min_uV	= 1700000,
		.max_uV	= 1900000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddmem_consumers),
	.consumer_supplies		= vddmem_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDMEM],
};


/*	[ VDDCORE ]
	DCDC		: 1.1V Power supply for core
	Fix consumers	: Digital core
	after reset	: ON
*/
static struct regulator_consumer_supply vddcore_consumers[] = {
	CONSUMERS_VDDCORE
};

static struct regulator_init_data vddcore_data = {
	.constraints	= {
		.min_uV	= 550000,
		.max_uV	= 1400000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddcore_consumers),
	.consumer_supplies		= vddcore_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDCORE],
};


/*	[ LDO_BG ]
	LDO		: Power supply for Band-Gap
	Fix consumers	: Band-Gap
	after reset	: OFF
*/
static struct regulator_consumer_supply ldobg_consumers[] = {
	CONSUMERS_LDO_BG
};

static struct regulator_init_data ldobg_data = {
	.constraints	= {
		.min_uV	= 550000,
		.max_uV	= 1400000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(ldobg_consumers),
	.consumer_supplies		= ldobg_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_LDO_BG],
};


/*	[ AVDDVB ]
	LDO		: 3.3V Power supply for Analog voice-band
	Fix consumers	: VB analog/VB output
	after reset	: OFF
*/
static struct regulator_consumer_supply avddvb_consumers[] = {
	CONSUMERS_AVDDVB
};

static struct regulator_init_data avddvb_data = {
	.constraints	= {
		.min_uV	= 2800000,
		.max_uV	= 3500000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(avddvb_consumers),
	.consumer_supplies		= avddvb_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_AVDDVB],
};


/*	[ VDDCAMDA ]
	LDO		: 2.8V Power supply for Digital Camera
	Ref consumers	: external sensor
	after reset	: OFF
*/
static struct regulator_consumer_supply vddcamda_consumers[] = {
	CONSUMERS_VDDCAMDA
};

static struct regulator_init_data vddcamda_data = {
	.constraints	= {
		.min_uV	= 1700000,
		.max_uV	= 3100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddcamda_consumers),
	.consumer_supplies		= vddcamda_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDCAMDA],
};


/*	[ VDDCAMD1 ]
	LDO		: 2.8V Power supply for Digital Camera
	Ref consumers	: external sensor
	after reset	: OFF
*/
static struct regulator_consumer_supply vddcamd1_consumers[] = {
	CONSUMERS_VDDCAMD1
};

static struct regulator_init_data vddcamd1_data = {
	.constraints	= {
		.min_uV	= 1150000,
		.max_uV	= 3400000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddcamd1_consumers),
	.consumer_supplies		= vddcamd1_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDCAMD1],
};


/*	[ VDDCAMD0 ]
	LDO		: 1.8V Power supply for Digital Camera
	Ref consumers	: external sensor
	after reset	: OFF
*/
static struct regulator_consumer_supply vddcamd0_consumers[] = {
	CONSUMERS_VDDCAMD0
};

static struct regulator_init_data vddcamd0_data = {
	.constraints	= {
		.min_uV	= 1250000,
		.max_uV	= 2900000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddcamd0_consumers),
	.consumer_supplies		= vddcamd0_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDCAMD0],
};


/*	[ VDDSIM1 ]
	LDO		: 1.8V Power supply for SIM card
	Ref consumers	: SIM card 1
	after reset	: OFF
*/
static struct regulator_consumer_supply vddsim1_consumers[] = {
	CONSUMERS_VDDSIM1
};

static struct regulator_init_data vddsim1_data = {
	.constraints	= {
		.min_uV	= 1700000,
		.max_uV	= 3200000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddsim1_consumers),
	.consumer_supplies		= vddsim1_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDSIM1],
};


/*	[ VDDSIM0 ]
	LDO		: 1.8V Power supply for SIM card
	Ref consumers	: SIM card 0
	Status after reset	: ON
*/
static struct regulator_consumer_supply vddsim0_consumers[] = {
	CONSUMERS_VDDSIM0
};

static struct regulator_init_data vddsim0_data = {
	.constraints	= {
		.min_uV	= 1700000,
		.max_uV	= 3200000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddsim0_consumers),
	.consumer_supplies		= vddsim0_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDSIM0],
};


/*	[ VDDSD0 ]
	LDO		: 2.8V Power supply for SD card
	Ref consumers	: SD card
	after reset	: OFF
*/
static struct regulator_consumer_supply vddsd0_consumers[] = {
	CONSUMERS_VDDSD0
};

static struct regulator_init_data vddsd0_data = {
	.constraints	= {
		.min_uV	= 1700000,
		.max_uV	= 3100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddsd0_consumers),
	.consumer_supplies		= vddsd0_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDSD0],
};


/*	[ VDDUSB ]
	LDO		: 3.3V Power supply for USB
	Fix consumers	: USB IP
	after reset	: OFF
*/
static struct regulator_consumer_supply vddusb_consumers[] = {
	CONSUMERS_VDDUSB
};

static struct regulator_init_data vddusb_data = {
	.constraints	= {
		.min_uV	= 3000000,
		.max_uV	= 3500000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddusb_consumers),
	.consumer_supplies		= vddusb_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDUSB],
};


/*	[ VDDUSBD ]
	LDO		: Power supply for USB Digital Core
	Fix consumers	: USB Digital Core
	after reset	: ON
*/
static struct regulator_consumer_supply vddusbd_consumers[] = {
	CONSUMERS_VDDUSBD
};

static struct regulator_init_data vddusbd_data = {
	.constraints	= {
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddusbd_consumers),
	.consumer_supplies		= vddusbd_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDUSBD],
};


/*	[ VDDSIM3 ]
	LDO		: 1.8V Power supply for SIM card
	Ref consumers	: SIM Card or CMMB RF
	after reset	: OFF
*/
static struct regulator_consumer_supply vddsim3_consumers[] = {
	CONSUMERS_VDDSIM3
};

static struct regulator_init_data vddsim3_data = {
	.constraints	= {
		.min_uV	= 1300000,
		.max_uV	= 3100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddsim3_consumers),
	.consumer_supplies		= vddsim3_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDSIM3],
};


/*	[ VDDSIM2 ]
	LDO		: 1.8V Power supply for SIM card
	Ref consumers	: SIM Card or CMMB RF
	after reset	: OFF
*/
static struct regulator_consumer_supply vddsim2_consumers[] = {
	CONSUMERS_VDDSIM2
};

static struct regulator_init_data vddsim2_data = {
	.constraints	= {
		.min_uV	= 1300000,
		.max_uV	= 3100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddsim2_consumers),
	.consumer_supplies		= vddsim2_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDSIM2],
};


/*	[ VDDWIF1 ]
	LDO		: 3.3V Power supply for external Wif or other application
	Ref consumers	: external Wifi terminal
	after reset	: OFF
*/
static struct regulator_consumer_supply vddwif1_consumers[] = {
	CONSUMERS_VDDWIF1
};

static struct regulator_init_data vddwif1_data = {
	.constraints	= {
		.min_uV	= 1100000,
		.max_uV	= 3450000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddwif1_consumers),
	.consumer_supplies		= vddwif1_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDWIF1],
};


/*	[ VDDWIF0 ]
	LDO		: 3.3V Power supply for external Wif or other application
	Ref consumers	: external Wifi terminal
	after reset	: OFF
*/
static struct regulator_consumer_supply vddwif0_consumers[] = {
	CONSUMERS_VDDWIF0
};

static struct regulator_init_data vddwif0_data = {
	.constraints	= {
		.min_uV	= 1100000,
		.max_uV	= 3450000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddwif0_consumers),
	.consumer_supplies		= vddwif0_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDWIF0],
};


/*	[ VDDSD1 ]
	LDO		: 2.8V Power supply for SD card
	Ref consumers	: SD card
	after reset	: OFF
*/
static struct regulator_consumer_supply vddsd1_consumers[] = {
	CONSUMERS_VDDSD1
};

static struct regulator_init_data vddsd1_data = {
	.constraints	= {
		.min_uV	= 1700000,
		.max_uV	= 3100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddsd1_consumers),
	.consumer_supplies		= vddsd1_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDSD1],
};


/*	[ VDDRTC ]
	LDO		: 2.8V Power supply for RTC
	Ref consumers	: small battery and RTC
	after reset	: ON
*/
static struct regulator_consumer_supply vddrtc_consumers[] = {
	CONSUMERS_VDDRTC
};

static struct regulator_init_data vddrtc_data = {
	.constraints	= {
		.min_uV	= 2450000,
		.max_uV	= 3350000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(vddrtc_consumers),
	.consumer_supplies		= vddrtc_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_VDDRTC],
};


/*	[ DVDD18 ]
	LDO		: 1.8V Power supply for Analog
	Fix consumers	: Analog
	after reset	: ON
*/
static struct regulator_consumer_supply dvdd18_consumers[] = {
	CONSUMERS_DVDD18
};

static struct regulator_init_data dvdd18_data = {
	.constraints	= {
		.min_uV	= 550000,
		.max_uV	= 1400000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(dvdd18_consumers),
	.consumer_supplies		= dvdd18_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_DVDD18],
};


/*	[ LDO_PA ]
	LDO		: 3.3V Power supply for Audio PA [ Internal ]
	Fix consumers	: Audio_PA
	after reset	: OFF
*/
static struct regulator_consumer_supply ldopa_consumers[] = {
	CONSUMERS_LDO_PA
};

static struct regulator_init_data ldopa_data = {
	.constraints	= {
		.min_uV	= 2700000,
		.max_uV	= 3600000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE,
	},

	.num_consumer_supplies		= ARRAY_SIZE(ldopa_consumers),
	.consumer_supplies		= ldopa_consumers,

	.regulator_init			= sc8810_regulator_init_reg,
	.driver_data			= &regulators_init_status[LDO_LDO_PA],
};


#define	REGULATOR_DEV(_id, _data)		\
	{					\
		.name 	= "regulator-sc8810",	\
		.id	= _id,			\
		.dev = {			\
			.platform_data = &_data,\
		},				\
	}

#define	REGULATOR_DEBUG_DEV(_id, _name)		\
	{					\
		.name 	= "reg-virt-consumer",	\
		.id	= _id,			\
		.dev = {			\
			.platform_data = _name,	\
		},				\
	}

static struct platform_device regulator_devices[] = {

	REGULATOR_DEV( LDO_VDDARM,		vddarm_data	),
	REGULATOR_DEV( LDO_VDD25,		vdd25_data	),
	REGULATOR_DEV( LDO_VDD18,		vdd18_data	),
	REGULATOR_DEV( LDO_VDD28,		vdd28_data	),
	REGULATOR_DEV( LDO_AVDDBB,		avddbb_data	),
	REGULATOR_DEV( LDO_VDDRF0,		vddrf0_data	),
	REGULATOR_DEV( LDO_VDDRF1,		vddrf1_data	),
	REGULATOR_DEV( LDO_VDDMEM,		vddmem_data	),
	REGULATOR_DEV( LDO_VDDCORE,		vddcore_data	),
	REGULATOR_DEV( LDO_LDO_BG,		ldobg_data	),
	REGULATOR_DEV( LDO_AVDDVB,		avddvb_data	),
	REGULATOR_DEV( LDO_VDDCAMDA,		vddcamda_data	),
	REGULATOR_DEV( LDO_VDDCAMD1,		vddcamd1_data	),
	REGULATOR_DEV( LDO_VDDCAMD0,		vddcamd0_data	),
	REGULATOR_DEV( LDO_VDDSIM1,		vddsim1_data	),
	REGULATOR_DEV( LDO_VDDSIM0,		vddsim0_data	),
	REGULATOR_DEV( LDO_VDDSD0,		vddsd0_data	),
	REGULATOR_DEV( LDO_VDDUSB,		vddusb_data	),
	REGULATOR_DEV( LDO_VDDUSBD,		vddusbd_data	),
	REGULATOR_DEV( LDO_VDDSIM3,		vddsim3_data	),
	REGULATOR_DEV( LDO_VDDSIM2,		vddsim2_data	),
	REGULATOR_DEV( LDO_VDDWIF1,		vddwif1_data	),
	REGULATOR_DEV( LDO_VDDWIF0,		vddwif0_data	),
	REGULATOR_DEV( LDO_VDDSD1,		vddsd1_data	),
	REGULATOR_DEV( LDO_VDDRTC,		vddrtc_data	),
	REGULATOR_DEV( LDO_DVDD18,		dvdd18_data	),
	REGULATOR_DEV( LDO_LDO_PA,		ldopa_data	),

/*Platform devices define for regulator vitrual dev debug for sysfs*/
#ifdef CONFIG_REGULATOR_VIRTUAL_CONSUMER
	REGULATOR_DEBUG_DEV( LDO_VDDARM,	"VDDARM"	),
	REGULATOR_DEBUG_DEV( LDO_VDD25,		"VDD25"		),
	REGULATOR_DEBUG_DEV( LDO_VDD18,		"VDD18"		),
	REGULATOR_DEBUG_DEV( LDO_VDD28,		"VDD28"		),
	REGULATOR_DEBUG_DEV( LDO_AVDDBB,	"AVDDBB"	),
	REGULATOR_DEBUG_DEV( LDO_VDDRF0,	"VDDRF0"	),
	REGULATOR_DEBUG_DEV( LDO_VDDRF1,	"VDDRF1"	),
	REGULATOR_DEBUG_DEV( LDO_VDDMEM,	"VDDMEM"	),
	REGULATOR_DEBUG_DEV( LDO_VDDCORE,	"VDDCORE"	),
	REGULATOR_DEBUG_DEV( LDO_LDO_BG,	"LDO_BG"	),
	REGULATOR_DEBUG_DEV( LDO_AVDDVB,	"AVDDVB"	),
	REGULATOR_DEBUG_DEV( LDO_VDDCAMDA,	"VDDCAMDA"	),
	REGULATOR_DEBUG_DEV( LDO_VDDCAMD1,	"VDDCAMD1"	),
	REGULATOR_DEBUG_DEV( LDO_VDDCAMD0,	"VDDCAMD0"	),
	REGULATOR_DEBUG_DEV( LDO_VDDSIM1,	"VDDSIM1"	),
	REGULATOR_DEBUG_DEV( LDO_VDDSIM0,	"VDDSIM0"	),
	REGULATOR_DEBUG_DEV( LDO_VDDSD0,	"VDDSD0"	),
	REGULATOR_DEBUG_DEV( LDO_VDDUSB,	"VDDUSB"	),
	REGULATOR_DEBUG_DEV( LDO_VDDUSBD,	"VDDUSBD"	),
	REGULATOR_DEBUG_DEV( LDO_VDDSIM3,	"VDDSIM3"	),
	REGULATOR_DEBUG_DEV( LDO_VDDSIM2,	"VDDSIM2"	),
	REGULATOR_DEBUG_DEV( LDO_VDDWIF1,	"VDDWIF1"	),
	REGULATOR_DEBUG_DEV( LDO_VDDWIF0,	"VDDWIF0"	),
	REGULATOR_DEBUG_DEV( LDO_VDDSD1,	"VDDSD1"	),
	REGULATOR_DEBUG_DEV( LDO_VDDRTC,	"VDDRTC"	),
	REGULATOR_DEBUG_DEV( LDO_DVDD18,	"DVDD18"	),
	REGULATOR_DEBUG_DEV( LDO_LDO_PA,	"LDO_PA"	),
#endif
};

void __init regulator_add_devices(void)
{
	int i = 0;
	for(i=0 ; i<ARRAY_SIZE(regulator_devices); i++)
	{
		platform_device_register(&(regulator_devices[i]));
	}
}


static struct regulator_init_status regulators_init_status[] =
{
	REGU_INIT_VDDARM,
	REGU_INIT_VDD25,
	REGU_INIT_VDD18,
	REGU_INIT_VDD28,
	REGU_INIT_AVDDBB,
	REGU_INIT_VDDRF0,
	REGU_INIT_VDDRF1,
	REGU_INIT_VDDMEM,
	REGU_INIT_VDDCORE,
	REGU_INIT_LDO_BG,
	REGU_INIT_AVDDVB,
	REGU_INIT_VDDCAMDA,
	REGU_INIT_VDDCAMD1,
	REGU_INIT_VDDCAMD0,
	REGU_INIT_VDDSIM1,
	REGU_INIT_VDDSIM0,
	REGU_INIT_VDDSD0,
	REGU_INIT_VDDUSB,
	REGU_INIT_VDDUSBD,
	REGU_INIT_VDDSIM3,
	REGU_INIT_VDDSIM2,
	REGU_INIT_VDDWIF1,
	REGU_INIT_VDDWIF0,
	REGU_INIT_VDDSD1,
	REGU_INIT_VDDRTC,
	REGU_INIT_DVDD18,
	REGU_INIT_LDO_PA,
};



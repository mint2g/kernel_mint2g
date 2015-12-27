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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pm.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <mach/hardware.h>
#include <mach/regulator.h>
#include <mach/globalregs.h>

/* system register interface dependence*/
/*#define REGULATOR_SYSFS_DEBUG_WITHOUT_ADI*/
#ifdef REGULATOR_SYSFS_DEBUG_WITHOUT_ADI
#include "regulator_debug_noadi.h"
#else
#include <mach/adi.h>
#endif


/*sc8810 ldo register*/
#define	LDO_REG_BASE		( SPRD_MISC_BASE+ 0x600	)

#define	LDO_PD_SET		( LDO_REG_BASE	+ 0x8 )
#define	LDO_PD_RST		( LDO_REG_BASE	+ 0xc )
#define	LDO_PD_CTRL0		( LDO_REG_BASE	+ 0x10 )
#define	LDO_PD_CTRL1		( LDO_REG_BASE	+ 0x14 )

#define	LDO_VCTRL0		( LDO_REG_BASE	+ 0x18 )
#define	LDO_VCTRL1		( LDO_REG_BASE	+ 0x1C )
#define	LDO_VCTRL2		( LDO_REG_BASE	+ 0x20 )
#define	LDO_VCTRL3		( LDO_REG_BASE	+ 0x24 )
#define	LDO_VCTRL4		( LDO_REG_BASE	+ 0x28 )

#define	LDO_SLP_CTRL0		( LDO_REG_BASE	+ 0x2C )
#define	LDO_SLP_CTRL1		( LDO_REG_BASE	+ 0x30 )
#define	LDO_SLP_CTRL2		( LDO_REG_BASE	+ 0x34 )
#define	LDO_DCDC_CTRL		( LDO_REG_BASE	+ 0x38 )
#define	LDO_VARM_CTRL		( LDO_REG_BASE	+ 0x44 )

#define	LDO_PA_CTRL1		( LDO_REG_BASE	+ 0X7C )

#define	LDO_PD_SET_MSK		( 0x3FF	)
#define	LDO_PD_CTL_MSK		( 0x5555)
#define	LDO_PA_OFF_BIT		( BIT(9)	)
#define	LDO_FUNC_RESERVED	( 0xFFFF)
#define	LDO_NA			( LDO_FUNC_RESERVED )


/*sc8810 ldo register ops*/
#ifndef	LDO_REG_OR
#define	LDO_REG_OR(_r, _b)	sci_adi_set(_r, _b)
#endif

#ifndef	LDO_REG_BIC
#define	LDO_REG_BIC(_r, _b)	sci_adi_clr(_r, _b)
#endif

#ifndef	LDO_REG_GET
#define	LDO_REG_GET(_r)		sci_adi_read(_r)
#endif

#ifndef	LDO_REG_SET
#define	LDO_REG_SET(_r, _v)	sci_adi_raw_write(_r, _v)
#endif


/*sc8810 ldo dc-specifications*/
static const int LDO_VDDARM_voltage_table[] = {
	650000,
	700000,
	800000,
	900000,
	1000000,
	1100000,
	1200000,
	1300000,
};

static const int LDO_VDD25_voltage_table[] = {
	2500000,
	2750000,
	3000000,
	2900000,
};

static const int LDO_VDD18_voltage_table[] = {
	1800000,
	2800000,
	1500000,
	1200000,
};

static const int LDO_VDD28_voltage_table[] = {
	2800000,
	3000000,
	2650000,
	1800000,
};

static const int LDO_AVDDBB_voltage_table[] = {
	3000000,
	3100000,
	2900000,
	2800000,
};

static const int LDO_VDDRF0_voltage_table[] = {
	2850000,
	2950000,
	2750000,
	1800000,
};

static const int LDO_VDDRF1_voltage_table[] = {
	2850000,
	2950000,
	2500000,
	1800000,
};

static const int LDO_VDDMEM_voltage_table[] = {
	1800000,
};

static const int LDO_VDDCORE_voltage_table[] = {
	650000,
	700000,
	800000,
	900000,
	1000000,
	1100000,
	1200000,
	1300000,
};

static const int LDO_LDO_BG_voltage_table[] = {
	650000,
	700000,
	800000,
	900000,
	1000000,
	1100000,
	1200000,
	1300000,
};

static const int LDO_AVDDVB_voltage_table[] = {
	3300000,
	3400000,
	3200000,
	2900000,
};

static const int LDO_VDDCAMDA_voltage_table[] = {
	2800000,
	3000000,
	2500000,
	1800000,
};

static const int LDO_VDDCAMD1_voltage_table[] = {
	2800000,
	3300000,
	1800000,
	1200000,
};

static const int LDO_VDDCAMD0_voltage_table[] = {
	1800000,
	2800000,
	1500000,
	1300000,
};

static const int LDO_VDDSIM1_voltage_table[] = {
	1800000,
	2900000,
	3000000,
	3100000,
};

static const int LDO_VDDSIM0_voltage_table[] = {
	1800000,
	2900000,
	3000000,
	3100000,
};

static const int LDO_VDDSD0_voltage_table[] = {
	2800000,
	3000000,
	2500000,
	1800000,
};

static const int LDO_VDDUSB_voltage_table[] = {
	3300000,
	3400000,
	3200000,
	3100000,
};

static const int LDO_VDDUSBD_voltage_table[] = {
	/*nothing in spec*/
};

static const int LDO_VDDSIM3_voltage_table[] = {
	2800000,
	3000000,
	1800000,
	1200000,
};

static const int LDO_VDDSIM2_voltage_table[] = {
	2800000,
	3000000,
	1800000,
	1200000,
};

static const int LDO_VDDWIF1_voltage_table[] = {
	2800000,
	3300000,
	1800000,
	1200000,
};

static const int LDO_VDDWIF0_voltage_table[] = {
	2800000,
	3300000,
	1800000,
	1200000,
};

static const int LDO_VDDSD1_voltage_table[] = {
	2800000,
	3000000,
	2500000,
	1800000,
};

static const int LDO_VDDRTC_voltage_table[] = {
	2600000,
	2800000,
	3000000,
	3200000,
};

static const int LDO_DVDD18_voltage_table[] = {
	650000,
	700000,
	800000,
	900000,
	1000000,
	1100000,
	1200000,
	1300000,
};

static const int LDO_LDO_PA_voltage_table[] = {
	2900000,
	3000000,
	3100000,
	3200000,
	3300000,
	3400000,
	3500000,
	3600000,
};


/* sc8810 regulator struct for driver*/
struct sc8810_regulator {
	const unsigned int reg_off_addr;
	const unsigned int reg_off_bit;
	const unsigned int reg_on_addr;
	const unsigned int reg_on_bit;

	const unsigned int reg_level_addr;
	const unsigned int reg_level_start_bit;

	const unsigned int reg_sleep_addr;
	const unsigned int reg_sleep_bit;

	const unsigned int n_voltages;
	const int *voltage_table;
};

#define SC8810_REGU(_ldo, _rfa, _rfb, _rna, _rnb, _rla, _rlsb, _rsa, _rsb)		\
	[_ldo]	= {						\
		.reg_off_addr	= _rfa,				\
		.reg_off_bit	= _rfb,				\
		.reg_on_addr	= _rna,				\
		.reg_on_bit	= _rnb,				\
		.reg_level_addr	= _rla,				\
		.reg_level_start_bit	= _rlsb,		\
		.reg_sleep_addr	= _rsa,				\
		.reg_sleep_bit	= _rsb,				\
		.n_voltages	= ARRAY_SIZE(_ldo##_voltage_table),	\
		.voltage_table	= _ldo##_voltage_table,		\
	}

static struct sc8810_regulator sc8810_regulator_regs[] = {
	SC8810_REGU(	LDO_VDDARM,	LDO_PD_SET,	BIT(9),	LDO_PD_RST,	BIT(9),	LDO_VARM_CTRL,	BIT(0),	LDO_SLP_CTRL1,	BIT(4)	),
	SC8810_REGU(	LDO_VDD25,	LDO_PD_SET,	BIT(8),	LDO_PD_RST,	BIT(8),	LDO_VCTRL3,	BIT(8),	LDO_SLP_CTRL0,	BIT(13)	),
	SC8810_REGU(	LDO_VDD18,	LDO_PD_SET,	BIT(7),	LDO_PD_RST,	BIT(7),	LDO_VCTRL3,	BIT(4),	LDO_SLP_CTRL0,	BIT(12)	),
	SC8810_REGU(	LDO_VDD28,	LDO_PD_SET,	BIT(6),	LDO_PD_RST,	BIT(6),	LDO_VCTRL3,	BIT(0),	LDO_SLP_CTRL0,	BIT(11)	),
	SC8810_REGU(	LDO_AVDDBB,	LDO_PD_SET,	BIT(5),	LDO_PD_RST,	BIT(5),	LDO_VCTRL0,	BIT(12),	LDO_SLP_CTRL0,	BIT(10)	),
	SC8810_REGU(	LDO_VDDRF0,	LDO_PD_SET,	BIT(3),	LDO_PD_RST,	BIT(3),	LDO_VCTRL0,	BIT(4),	LDO_SLP_CTRL0,	BIT(0)	),
	SC8810_REGU(	LDO_VDDRF1,	LDO_PD_SET,	BIT(4),	LDO_PD_RST,	BIT(4),	LDO_VCTRL0,	BIT(8),	LDO_SLP_CTRL0,	BIT(1)	),
	SC8810_REGU(	LDO_VDDMEM,	LDO_PD_SET,	BIT(2),	LDO_PD_RST,	BIT(2),	LDO_NA,		LDO_NA,	LDO_NA,		LDO_NA	),
	SC8810_REGU(	LDO_VDDCORE,	LDO_PD_SET,	BIT(1),	LDO_PD_RST,	BIT(1),	LDO_DCDC_CTRL,	BIT(0),	LDO_NA,		LDO_NA	),
	SC8810_REGU(	LDO_LDO_BG,	LDO_PD_SET,	BIT(0),	LDO_PD_RST,	BIT(0),	LDO_DCDC_CTRL,	BIT(0),	LDO_NA,		LDO_NA	),
	SC8810_REGU(	LDO_AVDDVB,	LDO_PD_CTRL0,	BIT(14),	LDO_PD_CTRL0,	BIT(15),	LDO_VCTRL1,	BIT(8),	LDO_SLP_CTRL0,	BIT(8)	),
	SC8810_REGU(	LDO_VDDCAMDA,	LDO_PD_CTRL0,	BIT(12),	LDO_PD_CTRL0,	BIT(13),	LDO_VCTRL2,	BIT(8),	LDO_SLP_CTRL0,	BIT(7)	),
	SC8810_REGU(	LDO_VDDCAMD1,	LDO_PD_CTRL0,	BIT(10),	LDO_PD_CTRL0,	BIT(11),	LDO_VCTRL2,	BIT(4),	LDO_SLP_CTRL0,	BIT(6)	),
	SC8810_REGU(	LDO_VDDCAMD0,	LDO_PD_CTRL0,	BIT(8),	LDO_PD_CTRL0,	BIT(9),	LDO_VCTRL2,	BIT(0),	LDO_SLP_CTRL0,	BIT(5)	),
	SC8810_REGU(	LDO_VDDSIM1,	LDO_PD_CTRL0,	BIT(6),	LDO_PD_CTRL0,	BIT(7),	LDO_VCTRL1,	BIT(4),	LDO_SLP_CTRL0,	BIT(3)	),
	SC8810_REGU(	LDO_VDDSIM0,	LDO_PD_CTRL0,	BIT(4),	LDO_PD_CTRL0,	BIT(5),	LDO_VCTRL1,	BIT(0),	LDO_SLP_CTRL0,	BIT(2)	),
	SC8810_REGU(	LDO_VDDSD0,	LDO_PD_CTRL0,	BIT(2),	LDO_PD_CTRL0,	BIT(3),	LDO_VCTRL1,	BIT(12),	LDO_SLP_CTRL0,	BIT(9)	),
	SC8810_REGU(	LDO_VDDUSB,	LDO_PD_CTRL0,	BIT(0),	LDO_PD_CTRL0,	BIT(1),	LDO_VCTRL2,	BIT(12),	LDO_SLP_CTRL0,	BIT(4)	),
	SC8810_REGU(	LDO_VDDUSBD,	GR_CLK_GEN5,	BIT(9),	GR_CLK_GEN5,	BIT(9),	LDO_NA,		LDO_NA,	LDO_NA,		LDO_NA	),
	SC8810_REGU(	LDO_VDDSIM3,	LDO_PD_CTRL1,	BIT(8),	LDO_PD_CTRL1,	BIT(9),	LDO_VCTRL4,	BIT(12),	LDO_SLP_CTRL1,	BIT(3)	),
	SC8810_REGU(	LDO_VDDSIM2,	LDO_PD_CTRL1,	BIT(6),	LDO_PD_CTRL1,	BIT(7),	LDO_VCTRL4,	BIT(8),	LDO_SLP_CTRL1,	BIT(2)	),
	SC8810_REGU(	LDO_VDDWIF1,	LDO_PD_CTRL1,	BIT(4),	LDO_PD_CTRL1,	BIT(5),	LDO_VCTRL4,	BIT(4),	LDO_SLP_CTRL1,	BIT(1)	),
	SC8810_REGU(	LDO_VDDWIF0,	LDO_PD_CTRL1,	BIT(2),	LDO_PD_CTRL1,	BIT(3),	LDO_VCTRL4,	BIT(0),	LDO_SLP_CTRL1,	BIT(0)	),
	SC8810_REGU(	LDO_VDDSD1,	LDO_PD_CTRL1,	BIT(0),	LDO_PD_CTRL1,	BIT(1),	LDO_VCTRL3,	BIT(12),	LDO_SLP_CTRL0,	BIT(15)	),
	SC8810_REGU(	LDO_VDDRTC,	LDO_NA,		LDO_NA,	LDO_NA,		LDO_NA,	LDO_VCTRL0,	BIT(0),	LDO_NA,		LDO_NA	),
	SC8810_REGU(	LDO_DVDD18,	LDO_PD_SET,	BIT(1),	LDO_PD_RST,	BIT(1),	LDO_DCDC_CTRL,	BIT(0),	LDO_NA,		LDO_NA	),
	SC8810_REGU(	LDO_LDO_PA,	LDO_PA_CTRL1,	BIT(9),	LDO_PA_CTRL1,	BIT(8),	LDO_PA_CTRL1,	BIT(4),	LDO_NA,		LDO_NA	),
};


/*sc8810 ldo specific func*/
int sc8810_regulator_init_reg(void * driver_data)
{
	struct regulator_init_status *reg_init_status = driver_data;
	struct sc8810_regulator *reg = &sc8810_regulator_regs[reg_init_status->ldo_id];
	int val;
	/*init for power manager */
	if(LDO_VDDARM == reg_init_status->ldo_id){
		/*ARMDCDC_PWR_ON_DLY = 0x1, FSM_SLPPD_EN=1*/
		val = LDO_REG_GET(LDO_SLP_CTRL1);
		val &= 0x0fff;
		val |= 0x9000;
		LDO_REG_SET(LDO_SLP_CTRL1, val);
		/*ARMDCDC_ISO_OFF_NUM = 0x20 ARMDCDC_ISO_ON_NUM = 0x0f*/
		LDO_REG_SET(LDO_SLP_CTRL2, 0x0f20);
	}

	if(reg_init_status->ldo_id >= LDO_END_MAX)
		return -EINVAL;

	/*init status by custom cfg :on or off*/
	switch (reg_init_status->on_off) {
		case LDO_INIT_NA:
			break;
		case LDO_INIT_ON:
			if (reg->reg_on_addr == LDO_NA)
				break;

			/*special case for VDDUSBD turn on*/
			if (unlikely(reg_init_status->ldo_id == LDO_VDDUSBD)) {
				sprd_greg_clear_bits(REG_TYPE_GLOBAL, reg->reg_on_bit, reg->reg_on_addr);
			} else if (reg->reg_off_addr == reg->reg_on_addr) {

				unsigned int reg_val = 0;
				reg_val = LDO_REG_GET(reg->reg_on_addr);
				reg_val |= reg->reg_on_bit;
				reg_val &= ~(reg->reg_off_bit);
				LDO_REG_SET(reg->reg_on_addr, reg_val);
			} else {
				LDO_REG_BIC(reg->reg_off_addr, reg->reg_off_bit);
				LDO_REG_OR(reg->reg_on_addr, reg->reg_on_bit);
			}
			break;
		case LDO_INIT_OFF:
			if (reg->reg_off_addr == LDO_NA)
				return -EINVAL;

			/*special case for VDDUSBD turn off*/
			if (unlikely(reg_init_status->ldo_id == LDO_VDDUSBD)) {
				sprd_greg_set_bits(REG_TYPE_GLOBAL, reg->reg_off_bit, reg->reg_off_addr);
			} else if (reg->reg_off_addr == reg->reg_on_addr) {

				unsigned int reg_val = 0;
				reg_val = LDO_REG_GET(reg->reg_off_addr);
				reg_val |= reg->reg_off_bit;
				reg_val &= ~(reg->reg_on_bit);
				LDO_REG_SET(reg->reg_off_addr, reg_val);
			}
			else {
				LDO_REG_BIC(reg->reg_on_addr, reg->reg_on_bit);
				LDO_REG_OR(reg->reg_off_addr, reg->reg_off_bit);
			}
			break;
		default:
			return -EINVAL;
	}

	/*init status by custom cfg :voltage level*/
	if(reg_init_status->vol_uv == LDO_INIT_NA) {
	}
	else {
		/*func reserved, because don't need now*/
	}

	/*init status by custom cfg :autosleep on off*/
	switch (reg_init_status->autosleep) {
		case LDO_INIT_NA:
			break;
		case LDO_INIT_SLP_ON:
			if(( reg->reg_sleep_addr >= LDO_SLP_CTRL0)
				&&( reg->reg_sleep_addr <= LDO_SLP_CTRL2))
				LDO_REG_OR(reg->reg_sleep_addr, reg->reg_sleep_bit);
			else
				return -EINVAL;
			break;
		case LDO_INIT_SLP_OFF:
			if(( reg->reg_sleep_addr >= LDO_SLP_CTRL0)
				&&( reg->reg_sleep_addr <= LDO_SLP_CTRL2))
				LDO_REG_BIC(reg->reg_sleep_addr, reg->reg_sleep_bit);
			else
				break;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static void sc8810_regulator_turnoff_allldo(void)
{
	/*ture off all system cores ldo*/
	LDO_REG_SET (	LDO_PD_RST,	0	);
	LDO_REG_SET (	LDO_PD_SET,	LDO_PD_SET_MSK	);
	/*ture off audio pa ldo*/
	LDO_REG_OR (	LDO_PA_CTRL1,	LDO_PA_OFF_BIT	);
	/*ture off all modules ldo*/
	LDO_REG_SET (	LDO_PD_CTRL1,	LDO_PD_CTL_MSK	);
	LDO_REG_SET (	LDO_PD_CTRL0,	LDO_PD_CTL_MSK	);
}


/* standard regulator dev ops*/
static int sc8810_regulator_enable(struct regulator_dev *rdev)
{
	struct sc8810_regulator *reg = &sc8810_regulator_regs[rdev_get_id(rdev)];

	/*when no enable bit for reg, means already on*/
	if (reg->reg_on_addr == LDO_NA)
		return 0;

	/*special case for VDDUSBD turn on*/
	if (unlikely(rdev_get_id(rdev) == LDO_VDDUSBD)) {
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, reg->reg_on_bit, reg->reg_on_addr);
	}
	/*if-else becasue adi op*/
	else if (reg->reg_off_addr == reg->reg_on_addr) {
		unsigned int reg_val = 0;
		reg_val = LDO_REG_GET(reg->reg_on_addr);
		reg_val |= reg->reg_on_bit;
		reg_val &= ~(reg->reg_off_bit);
		LDO_REG_SET(reg->reg_on_addr, reg_val);
	}
	else {
		LDO_REG_BIC(reg->reg_off_addr, reg->reg_off_bit);
		LDO_REG_OR(reg->reg_on_addr, reg->reg_on_bit);
	}
	return 0;
}

static int sc8810_regulator_disable(struct regulator_dev *rdev)
{
	struct sc8810_regulator *reg = &sc8810_regulator_regs[rdev_get_id(rdev)];

	/*when no disable bit for reg, means can't off*/
	if (reg->reg_off_addr == LDO_NA)
		return -EINVAL;

	/*special case for VDDUSBD turn off*/
	if (unlikely(rdev_get_id(rdev) == LDO_VDDUSBD)) {
		sprd_greg_set_bits(REG_TYPE_GLOBAL, reg->reg_off_bit, reg->reg_off_addr);
	}
	/*if-else becasue adi op*/
	else if (reg->reg_off_addr == reg->reg_on_addr) {
		unsigned int reg_val = 0;
		reg_val = LDO_REG_GET(reg->reg_off_addr);
		reg_val |= reg->reg_off_bit;
		reg_val &= ~(reg->reg_on_bit);
		LDO_REG_SET(reg->reg_off_addr, reg_val);
	}
	else {
		LDO_REG_BIC(reg->reg_on_addr, reg->reg_on_bit);
		LDO_REG_OR(reg->reg_off_addr, reg->reg_off_bit);
	}
	return 0;
}

static int sc8810_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct sc8810_regulator *reg = &sc8810_regulator_regs[rdev_get_id(rdev)];
	unsigned int on;

	/*when no enable bit for reg, means on*/
	if(reg->reg_on_addr == LDO_NA)
		return 1;

	/*special case for VDDUSBD turn on*/
	if (unlikely(rdev_get_id(rdev) == LDO_VDDUSBD)) {
		on = (((sprd_greg_read(REG_TYPE_GLOBAL, reg->reg_on_addr)) & (reg->reg_on_bit)) == 0);
	} else {
		on= ((LDO_REG_GET(reg->reg_on_addr)) & (reg->reg_on_bit));
	}

	return (on != 0);
}

static int sc8810_regulator_set_voltage_sel(struct regulator_dev *rdev,
								unsigned selector)
{
	struct sc8810_regulator *reg = &sc8810_regulator_regs[rdev_get_id(rdev)];
	unsigned int val;
	unsigned int vctrl[4] = {0xa, 0x9, 0x6, 0x5};

	/*this branch should not be reached, if valid_ops_mask is right.*/
	if(reg->reg_level_addr == LDO_NA)
		return -EINVAL;

	/*some ldo's level reg is special to others*/
	if(unlikely(reg->reg_level_addr > LDO_VCTRL4)) {

		val = LDO_REG_GET(reg->reg_level_addr);
		val &= ~(((reg->reg_level_start_bit)<<3) - (reg->reg_level_start_bit));
		val |= (selector * (reg->reg_level_start_bit));
	}
	else {
		val = LDO_REG_GET(reg->reg_level_addr);
		val &= ~(((reg->reg_level_start_bit)<<4) - (reg->reg_level_start_bit));
		val |= (vctrl[selector] * (reg->reg_level_start_bit));
	}

	LDO_REG_SET(reg->reg_level_addr, val);

	return 0;
}

static int sc8810_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct sc8810_regulator *reg = &sc8810_regulator_regs[rdev_get_id(rdev)];
	unsigned int val;
	unsigned int sel;

	/*can't change voltage, return the only one val*/
	if(reg->reg_level_start_bit == LDO_NA)
		return 0;

	/*some ldo's level reg is special to others*/
	if(unlikely(reg->reg_level_addr > LDO_VCTRL4)) {

		val = LDO_REG_GET(reg->reg_level_addr);
		sel = (((val & (reg->reg_level_start_bit))?(BIT(0)):0) |
			((val & ((reg->reg_level_start_bit)<<1))?(BIT(1)):0) |
			((val & ((reg->reg_level_start_bit)<<2))?(BIT(2)):0));
	}
	else {
		val = LDO_REG_GET(reg->reg_level_addr);
		sel = (((val & (reg->reg_level_start_bit))?(BIT(0)):0) |
			((val & ((reg->reg_level_start_bit)<<2))?(BIT(1)):0));
	}
	return sel;
}

static int sc8810_regulator_list_voltage(struct regulator_dev *rdev,
							unsigned int index)
{
	struct sc8810_regulator *reg = &sc8810_regulator_regs[rdev_get_id(rdev)];

	return reg->voltage_table[index];

}

static int sc8810_regulator_set_mode(struct regulator_dev *rdev,
							unsigned int mode)
{
	struct sc8810_regulator *reg = &sc8810_regulator_regs[rdev_get_id(rdev)];

	if( mode == REGULATOR_MODE_NORMAL) {
		if(( reg->reg_sleep_addr >= LDO_SLP_CTRL0)
			&&( reg->reg_sleep_addr <= LDO_SLP_CTRL2))
			LDO_REG_BIC(reg->reg_sleep_addr, reg->reg_sleep_bit);
		else	/*if NA, only normal mode*/
			return 0;

	}
	else if( mode == REGULATOR_MODE_STANDBY) {
		if(( reg->reg_sleep_addr >= LDO_SLP_CTRL0)
			&&( reg->reg_sleep_addr <= LDO_SLP_CTRL2))
			LDO_REG_OR(reg->reg_sleep_addr, reg->reg_sleep_bit);
		else	/*if NA, only normal mode*/
			return -EINVAL;
	}
	else {	/*this branch should not be reached, if valid_ops_mask is right*/
		return -EINVAL;
	}
	return 0;

}

static unsigned int sc8810_regulator_get_mode (struct regulator_dev *rdev)
{
	struct sc8810_regulator *reg = &sc8810_regulator_regs[rdev_get_id(rdev)];
	unsigned int mode;
	unsigned int val;

	if(( reg->reg_sleep_addr >= LDO_SLP_CTRL0)
		&&( reg->reg_sleep_addr <= LDO_SLP_CTRL2)) {

		val = LDO_REG_GET(reg->reg_sleep_addr);
		mode = (val & (reg->reg_sleep_bit))?
				(REGULATOR_MODE_STANDBY) :
				(REGULATOR_MODE_NORMAL);
	} else {
	/*some ldos don't support auto power down when sleep*/
		mode = REGULATOR_MODE_NORMAL;
	}

	return mode;
}

static struct regulator_ops sc8810_regulator_ops = {
	.enable			= sc8810_regulator_enable,
	.disable		= sc8810_regulator_disable,
	.is_enabled		= sc8810_regulator_is_enabled,
	.set_voltage_sel	= sc8810_regulator_set_voltage_sel,
	.get_voltage_sel	= sc8810_regulator_get_voltage_sel,
	.list_voltage		= sc8810_regulator_list_voltage,
	.set_mode		= sc8810_regulator_set_mode,
	.get_mode		= sc8810_regulator_get_mode,
	.set_voltage		= NULL,	/*reserverd because set_voltage_sel is more simple*/
	.get_voltage		= NULL,	/*reserverd because get_voltage_sel is littte simple*/
};


/* standard regulator_desc define*/
#define DESC_REG(_desc_reg)						\
	[_desc_reg]		= {					\
		.name		= #_desc_reg,				\
		.id		= _desc_reg,				\
		.n_voltages	= ARRAY_SIZE(_desc_reg##_voltage_table),	\
		.ops		= &sc8810_regulator_ops,			\
		.type		= REGULATOR_VOLTAGE,			\
		.owner		= THIS_MODULE,				\
	}

static struct regulator_desc sc8810_regulators[] = {
	DESC_REG(LDO_VDDARM),
	DESC_REG(LDO_VDD25),
	DESC_REG(LDO_VDD18),
	DESC_REG(LDO_VDD28),
	DESC_REG(LDO_AVDDBB),
	DESC_REG(LDO_VDDRF0),
	DESC_REG(LDO_VDDRF1),
	DESC_REG(LDO_VDDMEM),
	DESC_REG(LDO_VDDCORE),
	DESC_REG(LDO_LDO_BG),
	DESC_REG(LDO_AVDDVB),
	DESC_REG(LDO_VDDCAMDA),
	DESC_REG(LDO_VDDCAMD1),
	DESC_REG(LDO_VDDCAMD0),
	DESC_REG(LDO_VDDSIM1),
	DESC_REG(LDO_VDDSIM0),
	DESC_REG(LDO_VDDSD0),
	DESC_REG(LDO_VDDUSB),
	DESC_REG(LDO_VDDUSBD),
	DESC_REG(LDO_VDDSIM3),
	DESC_REG(LDO_VDDSIM2),
	DESC_REG(LDO_VDDWIF1),
	DESC_REG(LDO_VDDWIF0),
	DESC_REG(LDO_VDDSD1),
	DESC_REG(LDO_VDDRTC),
	DESC_REG(LDO_DVDD18),
	DESC_REG(LDO_LDO_PA),
};


/* standard regulator driver for system mount*/
static int __devinit sc8810_regulator_probe(struct platform_device *pdev)
{
	struct regulator_dev *rdev;
	struct regulator_init_data *init_data = pdev->dev.platform_data;

	rdev = regulator_register(&sc8810_regulators[pdev->id], &pdev->dev,
				pdev->dev.platform_data, init_data->driver_data);
	if (IS_ERR(rdev))
		return PTR_ERR(rdev);

	platform_set_drvdata(pdev, rdev);

	return 0;
}

static int __devexit sc8810_regulator_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);

	regulator_unregister(rdev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver sc8810_regulator_driver = {
	.driver = {
		.name	= "regulator-sc8810",
		.owner	= THIS_MODULE,
	},
	.probe	= sc8810_regulator_probe,
	.remove	= __devexit_p(sc8810_regulator_remove),
};

static int __init sc8810_regulator_init(void)
{
	pm_power_off = sc8810_regulator_turnoff_allldo;

	return platform_driver_register(&sc8810_regulator_driver);
}
subsys_initcall(sc8810_regulator_init);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("yiyue.he <yiyue.he@spreadtrum.com>");
MODULE_DESCRIPTION("SPRD Regulator Driver");


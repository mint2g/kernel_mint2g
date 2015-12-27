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

#include <linux/reboot.h>
#include <linux/string.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <mach/globalregs.h>
#include <mach/regs_ahb.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include "sprd_8825_charge.h"
#include <mach/usb.h>
#include <linux/delay.h>
#include <mach/sci.h>

#define USB_DM_GPIO 215
#define USB_DP_GPIO 216
extern int sci_adc_get_value(unsigned chan, int scale);

uint16_t adc_voltage_table[2][2] = {
	{3750, 4200},
	{3210, 3600},
};

uint16_t voltage_capacity_table[][2] = {
	{4150, 100},
	{4060, 90},
	{3980, 80},
	{3900, 70},
	{3840, 60},
	{3800, 50},
	{3760, 40},
	{3730, 30},
	{3700, 20},
	{3650, 15},
	{3600, 5},
	{3400, 0},
};

uint16_t ac_charging_voltage_capacity_table[][2] = {
	{4210, 100},
	{4140, 90},
	{4100, 80},
	{4030, 70},
	{3980, 60},
	{3940, 50},
	{3900, 40},
	{3870, 30},
	{3840, 20},
	{3790, 15},
	{3740, 5},
	{3250, 0},
};

uint16_t usb_charging_voltage_capacity_table[][2] = {
	{4200, 100},
	{4120, 90},
	{4060, 80},
	{4000, 70},
	{3940, 60},
	{3900, 50},
	{3860, 40},
	{3830, 30},
	{3800, 20},
	{3750, 15},
	{3700, 5},
	{3250, 0},
};

#ifdef CONFIG_BATTERY_TEMP_DECT
int32_t temp_adc_table[][2] = {
	{900, 0x4E},
	{850, 0x59},
	{800, 0x67},
	{750, 0x77},
	{700, 0x89},
	{650, 0x9E},
	{600, 0xB8},
	{550, 0xD4},
	{500, 0xF3},
	{450, 0x119},
	{400, 0x13F},
	{350, 0x16B},
	{300, 0x199},
	{250, 0x1CA},
	{200, 0x1FD},
	{150, 0x22F},
	{100, 0x260},
	{50, 0x291},
	{0, 0x2BD},
	{-50, 0x2E5},
	{-100, 0x308},
	{-150, 0x324},
	{-200, 0x33F},
	{-250, 0x354},
	{-300, 0x364}
};

int sprd_adc_to_temp(struct sprd_battery_data *data, uint16_t adcvalue)
{
	int table_size = ARRAY_SIZE(temp_adc_table);
	int index;
	int result;
	int first, second;

	for (index = 0; index < table_size; index++) {
		if (index == 0 && adcvalue < temp_adc_table[0][1])
			break;
		if (index == table_size - 1
		    && adcvalue >= temp_adc_table[index][1])
			break;

		if (adcvalue >= temp_adc_table[index][1]
		    && adcvalue < temp_adc_table[index + 1][1])
			break;
	}

	if (index == 0) {
		first = 0;
		second = 1;
	} else if (index == table_size - 1) {
		first = table_size - 2;
		second = table_size - 1;
	} else {
		first = index;
		second = index + 1;
	}

	result =
	    (adcvalue - temp_adc_table[first][1]) * (temp_adc_table[first][0] -
						     temp_adc_table[second][0])
	    / (temp_adc_table[first][1] - temp_adc_table[second][1]) +
	    temp_adc_table[first][0];
	return result;
}
#endif

uint32_t sprd_adc_to_cur(struct sprd_battery_data * data, uint16_t voltage)
{
	uint16_t cur_type = data->cur_type;
	uint32_t bat_numerators, bat_denominators;

	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &bat_numerators,
			      &bat_denominators);

	return (((uint32_t) voltage * cur_type * bat_numerators) /
		VOL_TO_CUR_PARAM) / bat_denominators;
}

uint16_t sprd_bat_adc_to_vol(struct sprd_battery_data * data, uint16_t adcvalue)
{
	int32_t temp;
	unsigned long flag;
	spin_lock_irqsave(&(data->lock), flag);
	temp = adc_voltage_table[0][1] - adc_voltage_table[1][1];
	temp = temp * (adcvalue - adc_voltage_table[0][0]);
	temp = temp / (adc_voltage_table[0][0] - adc_voltage_table[1][0]);
	temp = temp + adc_voltage_table[0][1];
	spin_unlock_irqrestore(&data->lock, flag);
	return temp;
}

uint16_t sprd_charger_adc_to_vol(struct sprd_battery_data * data,
				 uint16_t adcvalue)
{
	uint32_t result;
	uint32_t vbat_vol = sprd_bat_adc_to_vol(data, adcvalue);
	uint32_t m, n;
	uint32_t bat_numerators, bat_denominators;
	uint32_t vchg_numerators, vchg_denominators;

	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &bat_numerators,
			      &bat_denominators);
	sci_adc_get_vol_ratio(ADC_CHANNEL_VCHG, 0, &vchg_numerators,
			      &vchg_denominators);

	///v1 = vbat_vol*0.268 = vol_bat_m * r2 /(r1+r2)
	n = bat_denominators * vchg_numerators;
	m = vbat_vol * bat_numerators * (vchg_denominators);
	result = (m + n / 2) / n;
	return result;

}

uint32_t sprd_vol_to_percent(struct sprd_battery_data * data, uint32_t voltage,
			     int update)
{
	uint16_t percentum;
	int32_t temp;
	uint16_t table_size;
	int pos = 0;
	static uint16_t pre_percentum = 0xffff;
	int is_charging = data->charging;
	int is_usb;

	if (data->usb_online)
		is_usb = 1;
	else
		is_usb = 0;

	if (update) {
		pre_percentum = 0xffff;
		return 0;
	}

	if (is_charging) {
		if (is_usb) {
			table_size =
			    ARRAY_SIZE(usb_charging_voltage_capacity_table);
			for (pos = table_size - 1; pos > 0; pos--) {
				if (voltage <
				    usb_charging_voltage_capacity_table[pos][0])
					break;
			}
			if (pos == table_size - 1) {
				percentum = 0;
			} else {
				temp =
				    usb_charging_voltage_capacity_table[pos][1]
				    - usb_charging_voltage_capacity_table[pos +
									  1][1];
				temp =
				    temp * (voltage -
					    usb_charging_voltage_capacity_table
					    [pos][0]);
				temp =
				    temp /
				    (usb_charging_voltage_capacity_table[pos][0]
				     - usb_charging_voltage_capacity_table[pos +
									   1]
				     [0]);
				temp =
				    temp +
				    usb_charging_voltage_capacity_table[pos][1];
				if (temp > 100)
					temp = 100;
				percentum = temp;
			}
		} else {
			table_size =
			    ARRAY_SIZE(ac_charging_voltage_capacity_table);
			for (pos = table_size - 1; pos > 0; pos--) {
				if (voltage <
				    ac_charging_voltage_capacity_table[pos][0])
					break;
			}
			if (pos == table_size - 1) {
				percentum = 0;
			} else {
				temp =
				    ac_charging_voltage_capacity_table[pos][1] -
				    ac_charging_voltage_capacity_table[pos +
								       1][1];
				temp =
				    temp * (voltage -
					    ac_charging_voltage_capacity_table
					    [pos][0]);
				temp =
				    temp /
				    (ac_charging_voltage_capacity_table[pos][0]
				     - ac_charging_voltage_capacity_table[pos +
									  1]
				     [0]);
				temp =
				    temp +
				    ac_charging_voltage_capacity_table[pos][1];
				if (temp > 100)
					temp = 100;
				percentum = temp;
			}
		}

		if (pre_percentum == 0xffff)
			pre_percentum = percentum;
		else if (pre_percentum > percentum)
			percentum = pre_percentum;
		else
			pre_percentum = percentum;

	} else {
		table_size = ARRAY_SIZE(voltage_capacity_table);
		for (pos = 0; pos < table_size - 1; pos++) {
			if (voltage > voltage_capacity_table[pos][0])
				break;
		}
		if (pos == 0) {
			percentum = 100;
		} else {
			temp =
			    voltage_capacity_table[pos][1] -
			    voltage_capacity_table[pos - 1][1];
			temp =
			    temp * (voltage - voltage_capacity_table[pos][0]);
			temp =
			    temp / (voltage_capacity_table[pos][0] -
				    voltage_capacity_table[pos - 1][0]);
			temp = temp + voltage_capacity_table[pos][1];
			if (temp < 0)
				temp = 0;
			percentum = temp;
		}

		if (pre_percentum == 0xffff)
			pre_percentum = percentum;
		else if (pre_percentum < percentum)
			percentum = pre_percentum;
		else
			pre_percentum = percentum;

	}

	return percentum;
}

void __weak udc_enable(void)
{
}

void __weak udc_phy_down(void)
{
}

void __weak udc_disable(void)
{
}

int sprd_charger_is_adapter(struct sprd_battery_data *data)
{
	uint32_t ret;
	volatile uint32_t i;
	unsigned long irq_flag = 0;

	mdelay(300);

	gpio_request(USB_DM_GPIO, "sprd_charge");
	gpio_request(USB_DP_GPIO, "sprd_charge");
	gpio_direction_input(USB_DM_GPIO);
	gpio_direction_input(USB_DP_GPIO);

	udc_enable();
	udc_phy_down();
	local_irq_save(irq_flag);

	sci_glb_clr(REG_AHB_USB_PHY_CTRL, (BIT_DMPULLDOWN | BIT_DPPULLDOWN));

	/* Identify USB charger */
	sci_glb_set(REG_AHB_USB_PHY_CTRL, BIT_DMPULLUP);
	mdelay(10);
	ret = gpio_get_value(USB_DM_GPIO);
	sci_glb_clr(REG_AHB_USB_PHY_CTRL, (BIT_DMPULLUP));
#if 0
	/* normal charger */
	if (ret) {
		/* Identify standard adapter */
		sci_glb_set(REG_AHB_USB_PHY_CTRL, BIT_DMPULLDOWN);
		for (i = 0; i < 1000; i++) {;
		}
		if (gpio_get_value(USB_DM_GPIO)
		    == gpio_get_value(USB_DP_GPIO)) {
			ret = 1;	/* adapter */
		} else {
			ret = 1;	/* non standard adapter */
		}
		sci_glb_clr(REG_AHB_USB_PHY_CTRL, (BIT_DMPULLDOWN));
	}
#endif
	local_irq_restore(irq_flag);
	udc_disable();
	gpio_free(USB_DM_GPIO);
	gpio_free(USB_DP_GPIO);
	return ret;
}

#define VPROG_RESULT_NUM 10
#define VBAT_RESULT_DELAY 10
int32_t sprd_get_vprog(struct sprd_battery_data * data)
{
	int i, temp;
	volatile int j;
	int32_t vprog_result[VPROG_RESULT_NUM];

	for (i = 0; i < VPROG_RESULT_NUM;) {
		vprog_result[i] = sci_adc_get_value(ADC_CHANNEL_PROG, false);
		if (vprog_result[i] < 0)
			continue;

		i++;
		for (j = VBAT_RESULT_DELAY - 1; j >= 0; j--) {
			;
		}
	}

	for (j = 1; j <= VPROG_RESULT_NUM - 1; j++) {
		for (i = 0; i < VPROG_RESULT_NUM - j; i++) {
			if (vprog_result[i] > vprog_result[i + 1]) {
				temp = vprog_result[i];
				vprog_result[i] = vprog_result[i + 1];
				vprog_result[i + 1] = temp;
			}
		}
	}

	return vprog_result[VPROG_RESULT_NUM / 2];
}

int32_t sprd_get_chg_current(struct sprd_battery_data * data)
{
	int32_t vbat, isense;
	int32_t cnt = 0;

	for (cnt = 0; cnt < 8; cnt++) {
		isense =
		    sprd_bat_adc_to_vol(data,
					sci_adc_get_value(ADC_CHANNEL_ISENSE,
							  false));
		vbat =
		    sprd_bat_adc_to_vol(data,
					sci_adc_get_value(ADC_CHANNEL_VBAT,
							  false));
		if (isense >= vbat) {
			break;
		}
	}
	if (isense > vbat) {
		return ((isense - vbat) * 10);	//(vol/0.1ohm)
	} else {
		printk(KERN_ERR
		       "chg_current err......................isense:%d..................vbat:%d\n",
		       isense, vbat);
		return 0;
	}
}

#define CHG_CTL             (ANA_GPIN_PG0_BASE + 0x0000)

void sprd_stop_charge(struct sprd_battery_data *data)
{
	sci_adi_set(ANA_CHGR_CTRL1, CHGR_PD_BIT);
}

void sprd_start_charge(struct sprd_battery_data *data)
{
	sci_adi_clr(ANA_CHGR_CTRL1, CHGR_PD_BIT);
}

void sprd_set_recharge(struct sprd_battery_data *data)
{
	sci_adi_set(ANA_CHGR_CTRL0, CHGR_RECHG_BIT);
}

void sprd_stop_recharge(struct sprd_battery_data *data)
{
	sci_adi_clr(ANA_CHGR_CTRL0, CHGR_RECHG_BIT);
}

void sprd_set_sw(struct sprd_battery_data *data, int switchpoint)
{
	BUG_ON(switchpoint > 31);
	sci_adi_write(ANA_CHGR_CTRL1,
		      ((switchpoint << CHGR_SW_POINT_SHIFT) &
		       CHGR_SW_POINT_MSK), (CHGR_SW_POINT_MSK));
}

uint32_t sprd_get_sw(struct sprd_battery_data *data)
{
	return ((sci_adi_read(ANA_CHGR_CTRL1) & CHGR_SW_POINT_MSK) >>
		CHGR_SW_POINT_SHIFT);
}

uint32_t sprd_adjust_sw(struct sprd_battery_data * data, bool up_or_down)
{
	uint8_t current_switchpoint;
	uint8_t shift_bit;
	uint8_t chg_switchpoint;

	chg_switchpoint =
	    (sci_adi_read(ANA_CHGR_CTRL1) & CHGR_SW_POINT_MSK) >>
	    CHGR_SW_POINT_SHIFT;
	shift_bit = chg_switchpoint >> 4;
	current_switchpoint = chg_switchpoint & 0x0F;

	if (up_or_down) {
		if (shift_bit > 0) {
			if (current_switchpoint < 0xF) {
				current_switchpoint += 1;
			}
		} else {
			if (current_switchpoint > 0) {
				current_switchpoint -= 1;
			} else {
				shift_bit = 1;
			}
		}
	} else {
		if (shift_bit > 0) {
			if (current_switchpoint > 0) {
				current_switchpoint -= 1;
			} else {
				shift_bit = 0;
			}
		} else {
			if (current_switchpoint < 0xF) {
				current_switchpoint += 1;
			}
		}
	}

	chg_switchpoint = (shift_bit << 4) | current_switchpoint;

	sci_adi_write(ANA_CHGR_CTRL1,
		      ((chg_switchpoint << CHGR_SW_POINT_SHIFT) &
		       CHGR_SW_POINT_MSK), (CHGR_SW_POINT_MSK));
	return chg_switchpoint;
}

void sprd_set_chg_cur(uint32_t chg_current)
{
	uint32_t temp;

	if (chg_current > SPRD_CHG_CUR_MAX) {
		chg_current = SPRD_CHG_CUR_MAX;
	}

	temp = ((chg_current - 300) / 100);

	sci_adi_write(ANA_CHGR_CTRL1,
		      (temp << CHGR_CHG_CUR_SHIFT) & CHGR_CHG_CUR_MSK,
		      CHGR_CHG_CUR_MSK);
}

void sprd_chg_init(void)
{
	sci_adi_write(ANA_CHGR_CTRL0, CHGR_CC_EN_BIT,
		      (CHGR_CC_EN_BIT | CHGR_CC_EN_RST_BIT));
	sci_adi_set(ANA_CHGR_CTRL1, CHGR_CURVE_SHARP_BIT);
}

/* TODO: put these struct into sprd_battery_data */
uint32_t temp_buf[CONFIG_AVERAGE_CNT];
uint32_t vprog_buf[CONFIG_AVERAGE_CNT];
uint32_t vbat_buf[CONFIG_AVERAGE_CNT];

uint32_t vbat_capacity_buff[VBAT_CAPACITY_BUFF_CNT];
uint32_t vchg_buf[_VCHG_BUF_SIZE];

void put_vbat_capacity_value(uint32_t vbat)
{
	int i;
	static int buff_pointer = 0;

	vbat_capacity_buff[buff_pointer] = vbat;
	buff_pointer++;
	if (VBAT_CAPACITY_BUFF_CNT == buff_pointer) {
		buff_pointer = 0;
	}
}

uint32_t get_vbat_capacity_value(void)
{
	unsigned long sum = 0;
	int i;
	for (i = 0; i < VBAT_CAPACITY_BUFF_CNT; i++)
		sum += vbat_capacity_buff[i];

	return sum / VBAT_CAPACITY_BUFF_CNT;
}

void put_temp_value(struct sprd_battery_data *data, uint32_t temp)
{
	int i;
	for (i = 0; i < CONFIG_AVERAGE_CNT - 1; i++) {
		temp_buf[i] = temp_buf[i + 1];
	}

	temp_buf[CONFIG_AVERAGE_CNT - 1] = temp;
}

uint32_t get_temp_value(struct sprd_battery_data *data)
{
	unsigned long sum = 0;
	int i;
	for (i = 0; i < CONFIG_AVERAGE_CNT; i++)
		sum += temp_buf[i];

	return sum / CONFIG_AVERAGE_CNT;
}

void put_vprog_value(struct sprd_battery_data *data, uint32_t vprog)
{
	int i;
	for (i = 0; i < CONFIG_AVERAGE_CNT - 1; i++) {
		vprog_buf[i] = vprog_buf[i + 1];
	}

	vprog_buf[i] = vprog;
}

uint32_t get_vprog_value(struct sprd_battery_data *data)
{
	int i, sum = 0;
	for (i = 0; i < CONFIG_AVERAGE_CNT; i++) {
		sum = sum + vprog_buf[i];
	}
	return sum / CONFIG_AVERAGE_CNT;
}

void update_vprog_value(struct sprd_battery_data *data, uint32_t vprog)
{
	int i;
	for (i = 0; i < CONFIG_AVERAGE_CNT; i++) {
		vprog_buf[i] = vprog;
	}
}

void put_vbat_value(struct sprd_battery_data *data, uint32_t vbat)
{
	int i;
	for (i = 0; i < CONFIG_AVERAGE_CNT - 1; i++) {
		vbat_buf[i] = vbat_buf[i + 1];
	}

	vbat_buf[CONFIG_AVERAGE_CNT - 1] = vbat;
}

uint32_t get_vbat_value(struct sprd_battery_data *data)
{
	unsigned long sum = 0;
	int i;
	for (i = 0; i < CONFIG_AVERAGE_CNT; i++)
		sum += vbat_buf[i];

	return sum / CONFIG_AVERAGE_CNT;
}

void update_vbat_value(struct sprd_battery_data *data, uint32_t vbat)
{
	int i;
	for (i = 0; i < CONFIG_AVERAGE_CNT; i++)
		vbat_buf[i] = vbat;
}

void put_vchg_value(uint32_t vchg)
{
	int i;

	for (i = 0; i < _VCHG_BUF_SIZE - 1; i++) {
		vchg_buf[i] = vchg_buf[i + 1];
	}
	vchg_buf[i] = vchg;
}

uint32_t get_vchg_value(void)
{
	int i, sum = 0;
	for (i = 0; i < _VCHG_BUF_SIZE; i++) {
		sum = sum + vchg_buf[i];
	}
	return sum / _VCHG_BUF_SIZE;
}

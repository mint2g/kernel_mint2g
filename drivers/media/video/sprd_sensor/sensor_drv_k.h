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
#ifndef _SENSOR_DRV_K_H_
#define _SENSOR_DRV_K_H_


#define SENSOER_VDD_1200MV	1200000
#define SENSOER_VDD_1300MV	1300000
#define SENSOER_VDD_1500MV	1500000
#define SENSOER_VDD_1800MV	1800000
#define SENSOER_VDD_2500MV	2500000
#define SENSOER_VDD_2800MV	2800000
#define SENSOER_VDD_3000MV	3000000
#define SENSOER_VDD_3300MV	3300000
#define SENSOER_VDD_3800MV	3800000

typedef enum {
	SENSOR_VDD_3800MV = 0,
	SENSOR_VDD_3000MV,
	SENSOR_VDD_2800MV,
	SENSOR_VDD_2500MV,
	SENSOR_VDD_1800MV,
	SENSOR_VDD_1500MV,
	SENSOR_VDD_1300MV,
	SENSOR_VDD_1200MV,
	SENSOR_VDD_CLOSED,
	SENSOR_VDD_UNUSED
} SENSOR_VDD_VAL_E;

struct sensor_i2c {
unsigned short id;
unsigned short addr;
uint32_t clock;
};

typedef struct sensor_reg_tag {
	uint16_t reg_addr;
	uint16_t reg_value;
} SENSOR_REG_T, *SENSOR_REG_T_PTR;

typedef struct sensor_reg_bits_tag {
	uint16_t reg_addr;
	uint16_t reg_value;
	uint32_t reg_bits;
} SENSOR_REG_BITS_T, *SENSOR_REG_BITS_T_PTR;

typedef struct sensor_reg_tab_tag {
	SENSOR_REG_T_PTR sensor_reg_tab_ptr;
	uint32_t reg_count;
	uint32_t reg_bits;
	uint32_t burst_mode;
} SENSOR_REG_TAB_T, *SENSOR_REG_TAB_PTR;


#define SENSOR_MAIN_I2C_NAME "sensor_main"
#define SENSOR_SUB_I2C_NAME "sensor_sub"
#define SENSOR_MAIN_I2C_ADDR 0x30
#define SENSOR_SUB_I2C_ADDR 0x21

#define SENSOR_I2C_ID			1


#define SENSOR_MAX_MCLK						96	// MHZ

#define SENOR_CLK_M_VALUE   1000000


#define GLOBAL_BASE SPRD_GREG_BASE	/*0xE0002E00UL <--> 0x8b000000 */
#define ARM_GLOBAL_REG_GEN0 GLOBAL_BASE + 0x008UL
#define ARM_GLOBAL_REG_GEN3 GLOBAL_BASE + 0x01CUL
#define ARM_GLOBAL_PLL_SCR GLOBAL_BASE + 0x070UL
#define GR_CLK_GEN5 GLOBAL_BASE + 0x07CUL

#define AHB_BASE SPRD_AHB_BASE	/*0xE000A000 <--> 0x20900000UL */
#define AHB_GLOBAL_REG_CTL0 AHB_BASE + 0x200UL
#define AHB_GLOBAL_REG_SOFTRST AHB_BASE + 0x210UL

#define PIN_CTL_BASE SPRD_CPC_BASE	/*0xE002F000<-->0x8C000000UL */
#define PIN_CTL_CCIRPD1 PIN_CTL_BASE + 0x344UL
#define PIN_CTL_CCIRPD0 PIN_CTL_BASE + 0x348UL

#define MISC_BASE SPRD_MISC_BASE	/*0xE0033000<-->0x82000000 */
#ifdef CONFIG_ARCH_SC8810
#define ANA_REG_BASE MISC_BASE + 0x600
#define ANA_LDO_PD_CTL ANA_REG_BASE + 0x10
#define ANA_LDO_VCTL2 ANA_REG_BASE + 0x20
#else
#define ANA_REG_BASE MISC_BASE + 0x480
#define ANA_LDO_PD_CTL ANA_REG_BASE + 0x10
#define ANA_LDO_VCTL2 ANA_REG_BASE + 0x1C
#endif

#define BOOLEAN 					char
#define SENSOR_IOC_MAGIC			'R'

#define SENSOR_IO_PD				_IOW(SENSOR_IOC_MAGIC, 0,  BOOLEAN)
#define SENSOR_IO_SET_AVDD			_IOW(SENSOR_IOC_MAGIC, 1,  uint32_t)
#define SENSOR_IO_SET_DVDD			_IOW(SENSOR_IOC_MAGIC, 2,  uint32_t)
#define SENSOR_IO_SET_IOVDD			_IOW(SENSOR_IOC_MAGIC, 3,  uint32_t)
#define SENSOR_IO_SET_MCLK			_IOW(SENSOR_IOC_MAGIC, 4,  uint32_t)
#define SENSOR_IO_RST				_IOW(SENSOR_IOC_MAGIC, 5,  uint32_t)
#define SENSOR_IO_I2C_INIT			_IOW(SENSOR_IOC_MAGIC, 6,  uint32_t)
#define SENSOR_IO_I2C_DEINIT		_IOW(SENSOR_IOC_MAGIC, 7,  uint32_t)
#define SENSOR_IO_SET_ID			_IOW(SENSOR_IOC_MAGIC, 8,  uint32_t)
#define SENSOR_IO_RST_LEVEL			_IOW(SENSOR_IOC_MAGIC, 9,  uint32_t)
#define SENSOR_IO_I2C_ADDR			_IOW(SENSOR_IOC_MAGIC, 10, uint16_t)
#define SENSOR_IO_I2C_READ			_IOWR(SENSOR_IOC_MAGIC, 11, SENSOR_REG_BITS_T)
#define SENSOR_IO_I2C_WRITE			_IOW(SENSOR_IOC_MAGIC, 12, SENSOR_REG_BITS_T)
#define SENSOR_IO_SET_FLASH			_IOW(SENSOR_IOC_MAGIC, 13, uint32_t)
#define SENSOR_IO_I2C_WRITE_REGS	_IOW(SENSOR_IOC_MAGIC, 14, SENSOR_REG_TAB_T)
#define SENSOR_IO_SET_CAMMOT		_IOW(SENSOR_IOC_MAGIC, 15,  uint32_t)
#define SENSOR_IO_SET_I2CCLOCK		_IOW(SENSOR_IOC_MAGIC, 16,  uint32_t)


#endif //_SENSOR_DRV_K_H_

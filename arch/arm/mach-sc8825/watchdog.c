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
#include <mach/hardware.h>
#include <mach/watchdog.h>
#include <mach/adi.h>
#include <linux/string.h>
#include <asm/bitops.h>

#define WDG_BASE            (SPRD_MISC_BASE + 0x40)
#define WDG_LOAD_LOW        (WDG_BASE + 0x00)
#define WDG_LOAD_HIGH       (WDG_BASE + 0x04)
#define WDG_CTRL            (WDG_BASE + 0x08)
#define WDG_INT_CLR         (WDG_BASE + 0x0C)
#define WDG_INT_RAW         (WDG_BASE + 0x10)
#define WDG_INT_MSK         (WDG_BASE + 0x14)
#define WDG_CNT_LOW         (WDG_BASE + 0x18)
#define WDG_CNT_HIGH        (WDG_BASE + 0x1C)
#define WDG_LOCK            (WDG_BASE + 0x20)

#define WDG_INT_EN_BIT          BIT(0)
#define WDG_CNT_EN_BIT          BIT(1)
#define WDG_INT_CLEAR_BIT       BIT(0)
#define WDG_LD_BUSY_BIT         BIT(4)

#define WDG_UNLOCK_KEY          0xE551

#define SPRD_ANA_BASE           (SPRD_MISC_BASE + 0x600)
#define ANA_REG_BASE            SPRD_ANA_BASE	/*  0x82000600 */
#define ANA_RST_STATUS          (ANA_REG_BASE + 0X88)
#define ANA_AGEN                (ANA_REG_BASE + 0x00)
#define AGEN_WDG_EN             BIT(2)
#define AGEN_RTC_ARCH_EN        BIT(8)
#define AGEN_RTC_WDG_EN         BIT(10)

#define WDG_CLK                 32768

#define ANA_WDG_LOAD_TIMEOUT_NUM    (10000)

#define WDG_LOAD_TIMER_VALUE(value) \
        do{\
                    uint32_t   cnt          =  0;\
                    sci_adi_raw_write( WDG_LOAD_HIGH, (uint16_t)(((value) >> 16 ) & 0xffff));\
                    sci_adi_raw_write( WDG_LOAD_LOW , (uint16_t)((value)  & 0xffff) );\
                    while((sci_adi_read(WDG_INT_RAW) & WDG_LD_BUSY_BIT) && ( cnt < ANA_WDG_LOAD_TIMEOUT_NUM )) cnt++;\
        }while(0)

#define HWRST_STATUS_RECOVERY (0x20)
#define HWRST_STATUS_NORMAL (0X40)
#define HWRST_STATUS_ALARM (0X50)
#define HWRST_STATUS_SLEEP (0X60)
#define HWRST_STATUS_FASTBOOT (0X30)

void sprd_set_reboot_mode(const char *cmd)
{
	if (cmd && !(strncmp(cmd, "recovery", 8))) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_RECOVERY);
	} else if (cmd && !strncmp(cmd, "alarm", 5)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_ALARM);
	} else if (cmd && !strncmp(cmd, "fastsleep", 9)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_SLEEP);
	} else if (cmd && !strncmp(cmd, "bootloader", 10)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_FASTBOOT);
	} else {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_NORMAL);
	}
}

void sprd_turnon_watchdog(unsigned int ms)
{
	uint32_t cnt;

	cnt = (ms * 1000) / WDG_CLK;
	/* turn on watch dog clock */
	sci_adi_set(ANA_AGEN, AGEN_WDG_EN | AGEN_RTC_ARCH_EN | AGEN_RTC_WDG_EN);
	sci_adi_raw_write(WDG_LOCK, WDG_UNLOCK_KEY);
	sci_adi_clr(WDG_CTRL, WDG_INT_EN_BIT);
	WDG_LOAD_TIMER_VALUE(cnt);
	sci_adi_set(WDG_CTRL, WDG_CNT_EN_BIT | BIT(3));
	sci_adi_raw_write(WDG_LOCK, (uint16_t) (~WDG_UNLOCK_KEY));
}

void sprd_turnoff_watchdog(void)
{
	sci_adi_clr(ANA_AGEN, AGEN_WDG_EN | AGEN_RTC_ARCH_EN | AGEN_RTC_WDG_EN);
	sci_adi_raw_write(WDG_LOCK, WDG_UNLOCK_KEY);
	sci_adi_clr(WDG_CTRL, WDG_INT_EN_BIT);
	sci_adi_raw_write(WDG_LOCK, (uint16_t) (~WDG_UNLOCK_KEY));
}

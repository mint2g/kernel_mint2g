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
#include <linux/init.h>
#include <linux/irqflags.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <mach/adi.h>

/* registers definitions for controller CTL_EFUSE */
#define REG_EFUSE_DATA_RD               (SPRD_EFUSE_BASE + 0x0000)
#define REG_EFUSE_DATA_WR               (SPRD_EFUSE_BASE + 0x0004)
#define REG_EFUSE_BLOCK_INDEX           (SPRD_EFUSE_BASE + 0x0008)
#define REG_EFUSE_MODE_CTRL             (SPRD_EFUSE_BASE + 0x000c)
#define REG_EFUSE_PGM_PARA              (SPRD_EFUSE_BASE + 0x0010)
#define REG_EFUSE_STATUS                (SPRD_EFUSE_BASE + 0x0014)
#define REG_EFUSE_BLK_FLAGS             (SPRD_EFUSE_BASE + 0x0018)
#define REG_EFUSE_BLK_CLR               (SPRD_EFUSE_BASE + 0x001c)
#define REG_EFUSE_MAGIC_NUMBER          (SPRD_EFUSE_BASE + 0x0020)

/* bits definitions for register REG_EFUSE_BLOCK_INDEX */
#define BITS_READ_INDEX(_x_)            ( (_x_) << 0 & 0x7 )
#define BITS_PGM_INDEX(_x_)             ( (_x_) << 16 & 0x70000 )

#define SHIFT_READ_INDEX                ( 0 )
#define MASK_READ_INDEX                 ( BIT(0)|BIT(1)|BIT(2) )

#define SHIFT_PGM_INDEX                 ( 16 )
#define MASK_PGM_INDEX                  ( BIT(16)|BIT(17)|BIT(18) )

/* bits definitions for register REG_EFUSE_MODE_CTRL */
#define BIT_PG_START                    ( BIT(0) )
#define BIT_RD_START                    ( BIT(1) )
#define BIT_STANDBY_START               ( BIT(2) )

/* bits definitions for register REG_EFUSE_PGM_PARA */
#define BITS_TPGM_TIME_CNT(_x_)         ( (_x_) << 0 & (0x1FF) )
#define BIT_CLK_EFS_EN                  ( BIT(28) )
#define BIT_EFUSE_VDD_ON                ( BIT(29) )
#define BIT_PCLK_DIV_EN                 ( BIT(30) )
#define BIT_PGM_EN                      ( BIT(31) )

/* bits definitions for register REG_EFUSE_STATUS */
#define BIT_PGM_BUSY                    ( BIT(0) )
#define BIT_READ_BUSY                   ( BIT(1) )
#define BIT_STANDBY_BUSY                ( BIT(2) )

/* bits definitions for register REG_EFUSE_BLK_FLAGS */
#define BIT_BLK0_PROT_FLAG              ( BIT(0) )
#define BIT_BLK1_PROT_FLAG              ( BIT(1) )
#define BIT_BLK2_PROT_FLAG              ( BIT(2) )
#define BIT_BLK3_PROT_FLAG              ( BIT(3) )
#define BIT_BLK4_PROT_FLAG              ( BIT(4) )
#define BIT_BLK5_PROT_FLAG              ( BIT(5) )
#define BIT_BLK6_PROT_FLAG              ( BIT(6) )
#define BIT_BLK7_PROT_FLAG              ( BIT(7) )

/* bits definitions for register REG_EFUSE_BLK_CLR */
#define BIT_BLK0_PROT_FLAG_CLR          ( BIT(0) )
#define BIT_BLK1_PROT_FLAG_CLR          ( BIT(1) )
#define BIT_BLK2_PROT_FLAG_CLR          ( BIT(2) )
#define BIT_BLK3_PROT_FLAG_CLR          ( BIT(3) )
#define BIT_BLK4_PROT_FLAG_CLR          ( BIT(4) )
#define BIT_BLK5_PROT_FLAG_CLR          ( BIT(5) )
#define BIT_BLK6_PROT_FLAG_CLR          ( BIT(6) )
#define BIT_BLK7_PROT_FLAG_CLR          ( BIT(7) )

/* bits definitions for register REG_EFUSE_MAGIC_NUMBER */
#define BITS_MAGIC_NUMBER(_x_)          ( (_x_) & 0xFFFF)

/* vars definitions for controller CTL_EFUSE */
#define PROT_LOCK                       ( BIT_31 )

#define CTL_EFUSE_BASE_PHYS			( 0x89000000 )
#define CTL_EFUSE_BASE				( ctl_efuse_base )

static spinlock_t lock = __SPIN_LOCK_UNLOCKED(lock);

static inline void __raw_bits_and(unsigned int v, unsigned int a)
{
	unsigned long flags;

#ifndef CONFIG_NKERNEL
		spin_lock_irqsave(&lock, flags);
#else
		flags = hw_local_irq_save();
#endif
	__raw_writel((__raw_readl(a) & v), a);
#ifndef CONFIG_NKERNEL
		spin_unlock_irqrestore(&lock, flags);
#else
		hw_local_irq_restore(flags);
#endif
}

static inline void __raw_bits_or(unsigned int v, unsigned int a)
{
	unsigned long flags;

#ifndef CONFIG_NKERNEL
			spin_lock_irqsave(&lock, flags);
#else
			flags = hw_local_irq_save();
#endif
	__raw_writel((__raw_readl(a) | v), a);
#ifndef CONFIG_NKERNEL
			spin_unlock_irqrestore(&lock, flags);
#else
			hw_local_irq_restore(flags);
#endif
}

static void __iomem *ctl_efuse_base = 0;
void sci_efuse_poweron(void)
{
	ctl_efuse_base = ioremap(CTL_EFUSE_BASE_PHYS, PAGE_SIZE);
	sprd_greg_set_bits(REG_TYPE_GLOBAL, GEN0_EFUSE_EN, GR_GEN0);
	__raw_bits_or(BIT_EFUSE_VDD_ON|BIT_CLK_EFS_EN, REG_EFUSE_PGM_PARA);
}

void sci_efuse_poweroff(void)
{
	__raw_bits_and(~(BIT_PGM_EN|BIT_EFUSE_VDD_ON|BIT_CLK_EFS_EN), REG_EFUSE_PGM_PARA);
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, GEN0_EFUSE_EN, GR_GEN0);
	if (ctl_efuse_base) {
		iounmap(ctl_efuse_base);
		ctl_efuse_base = 0;
	}
}

int sci_efuse_read(unsigned blk)
{
	int cnt = 100;

	BUG_ON(blk > (MASK_READ_INDEX >> SHIFT_READ_INDEX));

	__raw_writel(BITS_READ_INDEX(blk), REG_EFUSE_BLOCK_INDEX);
	__raw_writel(__raw_readl(REG_EFUSE_MODE_CTRL) | BIT_RD_START, REG_EFUSE_MODE_CTRL);

	while ((__raw_readl(REG_EFUSE_STATUS) & BIT_READ_BUSY) && cnt--);

	WARN_ON(cnt == 0);

	return __raw_readl(REG_EFUSE_DATA_RD);
}

int sci_efuse_program(unsigned blk, int data)
{
	return 0;
}

int sci_efuse_raw_write(unsigned blk, int data, u32 magic)
{
	return 0;
}

int sci_efuse_is_locked(unsigned blk)
{
	return 0;
}

int sci_efuse_lock(unsigned blk)
{
	return 0;
}

#define CAL_DATA_BLK	7
#define BASE_ADC_P0   785   //3.6V
#define BASE_ADC_P1   917   //4.2V
#define VOL_P0        3600
#define VOL_P1        4200
#define ADC_DATA_OFFSET 128
int sci_efuse_calibration_get(unsigned int* p_cal_data)
{
	int data;
	unsigned int cal_temp;
	unsigned short adc_temp;

	sci_efuse_poweron();
	data = sci_efuse_read(CAL_DATA_BLK);
	sci_efuse_poweroff();

	data &= ~(1 << 31);

	printk("sci_efuse_calibration data:0x%x\n",data);

	if((!data)||(p_cal_data == NULL))
	{
		return 0;
	}
	//adc 3.6V
	adc_temp = ((data>>8) & 0x00FF) + BASE_ADC_P0 - ADC_DATA_OFFSET;
	p_cal_data[1] = (VOL_P0)|(adc_temp << 16);

	//adc 4.2V
	adc_temp = (data & 0x00FF) + BASE_ADC_P1 - ADC_DATA_OFFSET;
	p_cal_data[0] = (VOL_P1)|(adc_temp << 16);

	return 1;
}


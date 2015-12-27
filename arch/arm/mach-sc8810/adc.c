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
#include <linux/module.h>
#include <linux/irqflags.h>
#include <linux/delay.h>

#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/adc.h>

#define ANA_CTL_ADC_BASE		(SPRD_MISC_BASE + 0x0300)

/* registers definitions for controller ANA_CTL_ADC */
#define ANA_REG_ADC_CTRL                (ANA_CTL_ADC_BASE + 0x0000)
#define ANA_REG_ADC_CS                  (ANA_CTL_ADC_BASE + 0x0004)
#define ANA_REG_ADC_TPC_CH_CTRL         (ANA_CTL_ADC_BASE + 0x0008)
#define ANA_REG_ADC_DAT                 (ANA_CTL_ADC_BASE + 0x000c)
#define ANA_REG_ADC_IE                  (ANA_CTL_ADC_BASE + 0x0010)
#define ANA_REG_ADC_IC                  (ANA_CTL_ADC_BASE + 0x0014)
#define ANA_REG_ADC_ISTS                (ANA_CTL_ADC_BASE + 0x0018)
#define ANA_REG_ADC_ISRC                (ANA_CTL_ADC_BASE + 0x001c)

/* bits definitions for register REG_ADC_CTRL */
#define BIT_ADC_STATUS                  ( BIT(4) )
#define BIT_HW_INT_EN                   ( BIT(3) )
#define BIT_TPC_CH_ON                   ( BIT(2) )
#define BIT_SW_CH_ON                    ( BIT(1) )
#define BIT_ADC_EN                      ( BIT(0) )

/* bits definitions for register REG_ADC_CS */
#define BIT_ADC_SLOW                    ( BIT(5) )
#define BIT_ADC_SCALE                   ( BIT(4) )
#define BITS_ADC_CS(_x_)                ( (_x_) & 0x0F )

#define SHIFT_ADC_CS                    ( 0 )
#define MASK_ADC_CS                     ( 0x0F )

/* bits definitions for register REG_ADC_TPC_CH_CTRL */
#define BITS_TPC_CH_DELAY(_x_)          ( (_x_) << 8 & 0xFF00 )
#define BITS_TPC_Y_CH(_x_)              ( (_x_) << 4 & 0xF0 )
#define BITS_TPC_X_CH(_x_)              ( (_x_) << 0 & 0x0F )

/* bits definitions for register REG_ADC_DAT */
#define BITS_ADC_DAT(_x_)               ( (_x_) << 0 & 0x3FF )

#define SHIFT_ADC_DAT                   ( 0 )
#define MASK_ADC_DAT                    ( 0x3FF )

/* bits definitions for register REG_ADC_IE */
#define BIT_ADC_IE                      ( BIT(0) )

/* bits definitions for register REG_ADC_IC */
#define BIT_ADC_IC                      ( BIT(0) )

/* bits definitions for register REG_ADC_ISTS */
#define BIT_ADC_MIS                     ( BIT(0) )

/* bits definitions for register REG_ADC_ISRC */
#define BIT_ADC_RIS                     ( BIT(0) )

/* adc global regs */
#define ANA_REG_GLB_APB_CLK_EN		(SPRD_MISC_BASE + 0x0600)
#define BIT_CLK_AUXAD_EN                ( BIT(14) )
#define BIT_CLK_AUXADC_EN               ( BIT(13) )
#define BIT_ADC_EB                      ( BIT(5) )

void sci_adc_enable(void)
{
	/* enable adc */
	sci_adi_set(ANA_REG_GLB_APB_CLK_EN,
			BIT_ADC_EB | BIT_CLK_AUXADC_EN | BIT_CLK_AUXAD_EN);
	sci_adi_set(ANA_CTL_ADC_BASE, BIT_ADC_EN);
}

#ifdef CONFIG_NKERNEL
#define sci_adc_lock()				\
		flags = hw_local_irq_save()
#define sci_adc_unlock()			\
		hw_local_irq_restore(flags)

#else
#define sci_adc_lock()
#define sci_adc_unlock()

#endif

int sci_adc_get_value(unsigned int channel, int scale)
{
	unsigned long flags;
	int ret, cnt = 12;

	BUG_ON(channel >= ADC_MAX);

	if (scale)
		scale = BIT_ADC_SCALE;

	sci_adc_lock();

	/* clear int */
	sci_adi_set(ANA_REG_ADC_IC, BIT_ADC_IC);

	sci_adi_raw_write(ANA_REG_ADC_CS, channel | scale);

	/* turn on sw channel */
	sci_adi_set(ANA_REG_ADC_CTRL, BIT_SW_CH_ON);

	/* wait adc complete */
	while (!(sci_adi_read(ANA_REG_ADC_ISRC) & BIT_ADC_RIS) && cnt--) {
		udelay(50);
	}

	WARN_ON(!cnt);

	ret = sci_adi_read(ANA_REG_ADC_DAT) & MASK_ADC_DAT;

	/* turn off sw channel */
	sci_adi_clr(ANA_REG_ADC_CTRL, BIT_SW_CH_ON);

	/* clear int */
	sci_adi_set(ANA_REG_ADC_IC, BIT_ADC_IC);

	sci_adc_unlock();

	return ret;
}

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

#ifndef __ASM_ARCH_SPRD_IRQS_H
#define __ASM_ARCH_SPRD_IRQS_H

#define NR_SCI_PHY_IRQS			(IRQ_GIC_START + 64)

#define IRQ_LOCALTIMER			(29)
#define IRQ_LOCALWDOG			(30)
#define IRQ_GIC_START			(32)

#define SCI_IRQ(_X_)			(IRQ_GIC_START + (_X_))
#define SCI_EXT_IRQ(_X_)		(NR_SCI_PHY_IRQS + (_X_))

#define IRQ_SPECIAL_LATCH		SCI_IRQ(0)
#define IRQ_SOFT_TRIGGED0_INT		SCI_IRQ(1)
#define IRQ_SER0_INT			SCI_IRQ(2)
#define IRQ_SER1_INT			SCI_IRQ(3)
#define IRQ_SER2_INT			SCI_IRQ(4)
#define IRQ_SER3_INT			SCI_IRQ(5)
#define IRQ_TIMER0_INT			SCI_IRQ(6)
#define IRQ_TIMER1_INT			SCI_IRQ(7)
#define IRQ_TIMER2_INT			SCI_IRQ(8)
#define IRQ_SYSTIME_INT			SCI_IRQ(9)
#define IRQ_GPIO_INT			SCI_IRQ(10)
#define IRQ_SPI0_INT			SCI_IRQ(11)
#define IRQ_SPI1_INT			SCI_IRQ(12)
#define IRQ_SPI2_INT			SCI_IRQ(13)
#define IRQ_KPD_INT			SCI_IRQ(14)
#define IRQ_SIM0_INT 			SCI_IRQ(15)
#define IRQ_SIM1_INT 			SCI_IRQ(16)
#define IRQ_I2C0_INT 			SCI_IRQ(17)
#define IRQ_I2C1_INT 			SCI_IRQ(18)
#define IRQ_I2C2_INT 			SCI_IRQ(19)
#define IRQ_I2C3_INT 			SCI_IRQ(20)
#define IRQ_EIC_INT			SCI_IRQ(21)
#define IRQ_EPT_INT			SCI_IRQ(22)
#define IRQ_IIS0_INT			SCI_IRQ(23)
#define IRQ_IIS1_INT			SCI_IRQ(24)
#define IRQ_ANA_INT			SCI_IRQ(25)
#define IRQ_DSP0_INT			SCI_IRQ(26)
#define IRQ_DSP1_INT			SCI_IRQ(27)
#define IRQ_DSPALL_INT			SCI_IRQ(28)
#define IRQ_LCX_SEM_INT			SCI_IRQ(29)
#define IRQ_CX_CR_INT			SCI_IRQ(30)
#define IRQ_REQ_AUD_INT			SCI_IRQ(31)
#define IRQ_SOFT_TRIGGED1_INT		SCI_IRQ(33)
#define IRQ_ADI_INT			SCI_IRQ(34)
#define IRQ_USBD_INT			SCI_IRQ(35)
#define IRQ_DMA_INT			SCI_IRQ(36)
#define IRQ_VBC_INT			SCI_IRQ(37)
#define IRQ_G3D_INT			SCI_IRQ(38)
#define IRQ_VSP_INT			SCI_IRQ(39)
#define IRQ_CSI_INT0			SCI_IRQ(40)
#define IRQ_CSI_INT1			SCI_IRQ(41)
#define IRQ_DSI_INT0			SCI_IRQ(42)
#define IRQ_DSI_INT1			SCI_IRQ(43)
#define IRQ_ISP_INT			SCI_IRQ(44)
#define IRQ_DCAM_INT			SCI_IRQ(45)
#define IRQ_DISPC_INT			SCI_IRQ(46)
#define IRQ_LCDC_INT			SCI_IRQ(47)
#define IRQ_NFC_INT			SCI_IRQ(48)
#define IRQ_DRM_INT			SCI_IRQ(49)
#define IRQ_SDIO0_INT			SCI_IRQ(50)
#define IRQ_SDIO1_INT			SCI_IRQ(51)
#define IRQ_SDIO2_INT			SCI_IRQ(52)
#define IRQ_EMMC_INT			SCI_IRQ(53)
#define IRQ_CA5L2CC_INT			SCI_IRQ(54)
#define IRQ_CA5PMU_NCT_INT0		SCI_IRQ(55)
#define IRQ_CA5PMU_NCT_INT1		SCI_IRQ(56)
#define IRQ_BM_INT			SCI_IRQ(57)
#define IRQ_AXIBM_INT			SCI_IRQ(58)
#define IRQ_CP2AP_INT0			SCI_IRQ(59)
#define IRQ_CP2AP_INT1			SCI_IRQ(60)
#define IRQ_CP_WDG_INT			SCI_IRQ(61)

/* translate gic irq number(user using ) to intc number */
#define SCI_GET_INTC_IRQ(_IRQ_NUM_)	((_IRQ_NUM_) - IRQ_GIC_START)
#define SCI_INTC_IRQ_BIT(_IRQ_NUM_)	((SCI_GET_INTC_IRQ(_IRQ_NUM_)<IRQ_GIC_START)	? \
					(1<<SCI_GET_INTC_IRQ(_IRQ_NUM_))		: \
					(1<<(SCI_GET_INTC_IRQ(_IRQ_NUM_)-IRQ_GIC_START)))

/* analog die interrupt number */
#define IRQ_ANA_ADC_INT			SCI_EXT_IRQ(0)
#define IRQ_ANA_GPIO_INT		SCI_EXT_IRQ(1)
#define IRQ_ANA_RTC_INT			SCI_EXT_IRQ(2)
#define IRQ_ANA_WDG_INT			SCI_EXT_IRQ(3)
#define IRQ_ANA_TPC_INT			SCI_EXT_IRQ(4)
#define IRQ_ANA_EIC_INT			SCI_EXT_IRQ(5)
#define IRQ_ANA_CHGRWDG_INT		SCI_EXT_IRQ(6)
#define IRQ_ANA_AUD_INT			SCI_EXT_IRQ(7)

#define IRQ_ANA_INT_START		IRQ_ANA_ADC_INT
#define NR_ANA_IRQS			(8)

/* sc8825 gpio&eic pin interrupt number, total is 320, which is bigger than 256 */
#define GPIO_IRQ_START			SCI_EXT_IRQ(8)

#define NR_GPIO_IRQS	( 320 )

#define NR_IRQS				(NR_SCI_PHY_IRQS + NR_ANA_IRQS + NR_GPIO_IRQS)

#ifndef __ASSEMBLY__
void __init ana_init_irq(void);
#endif

#endif

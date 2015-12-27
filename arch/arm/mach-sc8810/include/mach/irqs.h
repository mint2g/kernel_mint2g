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

/*
 * naming rule: name in chip spec plus  an "IRQ_" prefix
 * see spec 4.5.2
 */
#ifdef CONFIG_ARCH_SC7710
	/* general irq numbers */
#define NR_SPRD_IRQS		    48

#define IRQ_SER0_INT		    2
#define IRQ_CA5_L2CC_INT	    3
#define IRQ_CA5_PMU_INT	    3
#define IRQ_CA5_NCT_INT	    3
#define IRQ_SER2_INT		    4
#define IRQ_TIMER0_INT	    5
#define IRQ_TIMER1_INT	    6
#define IRQ_TIMER2_INT	    7
#define IRQ_GPIO_INT		    8	/* cascaded digital gpio irqs , eic_irq, eic2_irq */
#define IRQ_SPI0_INT		    9
#define IRQ_KPD_INT		    10
#define IRQ_I2C0_INT		    11
#define IRQ_SIM_INT		    12
#define IRQ_I2C2_INT		    14
#define IRQ_SER3_INT		    15
#define IRQ_SDIO2_INT	    16
#define IRQ_SYST_INT		    17
#define IRQ_AUD_INT		    18
#define IRQ_IIS0_INT		    19
#define IRQ_EMMC_INT		    20
#define IRQ_DMA_INT		    21
#define IRQ_VBC_INT		    22
#define IRQ_VSP_INT		    23
#define IRQ_ANA_INT		    24	/* cascaded analog chip irqs */
#define IRQ_G3D_INT		    25
#define IRQ_USBD_INT		    26
#define IRQ_ISP_INT		    27
#define IRQ_NLC_INT		    28
#define IRQ_LCDC_INT		    29
#define IRQ_DRM_INT		    30
#define IRQ_BM0_INT		    31
#define IRQ_BM1_INT		    31
#define IRQ_AXI_BUS_MON0	    31
#define IRQ_AXI_BUS_MON1	    31
#define IRQ_SER1_INT		    34
#define IRQ_SPI1_INT		    35
#define IRQ_SPI2_INT		    36
#define IRQ_SPI3_INT		    37
#define IRQ_I2C1_INT		    38
#define IRQ_I2C3_INT		    39
#define IRQ_I2C4_INT		    40
#define IRQ_DISPC_INT		    41
#define IRQ_IIS1_INT		    42
#define IRQ_IIS2_INT		    43
#define IRQ_SDIO0_INT	    44
#define IRQ_SDIO1_INT	    45
#define IRQ_EIC_INT	            46
#define IRQ_EIC_ASYNC_INT    46
#define IRQ_EIC_SYNC_INT      46
#define IRQ_EIC2_INT	            47
#define IRQ_EIC2_ASYNC_INT  47
#define IRQ_EIC2_SYNC_INT    47

/* all gpio/eic irq numbers in gpio_eic.c */
#define GPIO_IRQ_START			NR_SPRD_IRQS
#define NR_GPIO_IRQS			(224+32+16+16)	/* D-gpio  +  A-gpio  +  A-eic  +  D-eic  +  D-eic2 (?) */

/* analog die irq numbers in adi.c */
#define IRQ_ANA_INT_START		(NR_SPRD_IRQS + NR_GPIO_IRQS)
#define NR_ANA_IRQS			7

#define IRQ_ANA_ADC_INT		(IRQ_ANA_INT_START + 0)
#define IRQ_ANA_GPIO_INT		(IRQ_ANA_INT_START + 1)
#define IRQ_ANA_RTC_INT		(IRQ_ANA_INT_START + 2)
#define IRQ_ANA_WDG_INT		(IRQ_ANA_INT_START + 3)
#define IRQ_ANA_TPC_INT		(IRQ_ANA_INT_START + 4)
#define IRQ_ANA_EIC_INT		(IRQ_ANA_INT_START + 5)
#define IRQ_ANA_CHGRWDG_INT	(IRQ_ANA_INT_START + 6)


/* total irq numbers, the last one is reserved and used in gpio_eic.c */
#define NR_IRQS					(NR_SPRD_IRQS + NR_ANA_IRQS + NR_GPIO_IRQS)


#else
/* general irq numbers */
#define NR_SPRD_IRQS		32

#define IRQ_SLEEP_INT		0
#define IRQ_SOFT_TRIG_INT	1
#define IRQ_SER0_INT		2

#define IRQ_SER1_INT		3
#define IRQ_CA5_L2CC_INT	3
#define IRQ_CA5_PMU_INT		3
#define IRQ_CA5_NCT_INT		3

#define IRQ_SER2_INT		4

#define IRQ_TIMER0_INT		5
#define IRQ_TIMER1_INT		6

#define IRQ_TIMER2_INT		7
#define IRQ_GPIO_INT		8	/* cascaded digital gpio irqs */

#define IRQ_SPI0_INT		9
#define IRQ_SPI1_INT		9

#define IRQ_KPD_INT		10

#define IRQ_I2C0_INT		11
#define IRQ_I2C1_INT		11

#define IRQ_SIM_INT		12
#define IRQ_PIU_SER_INT		13

#define IRQ_PIU_CR_INT		14
#define IRQ_I2C2_INT		14
#define IRQ_I2C3_INT		14

#define IRQ_DSP0_INT		15
#define IRQ_DSP1_INT		16
#define IRQ_SYST_INT		17
#define IRQ_IIS0_INT		19
#define IRQ_IIS1_INT		19
#define IRQ_DSP_ICU_INT		20
#define IRQ_DMA_INT		21
#define IRQ_VBC_INT		22
#define IRQ_VSP_INT		23
#define IRQ_ANA_INT		24	/* cascaded analog chip irqs */

#define IRQ_ADI_INT		25
#define IRQ_GPU_INT		25

#define IRQ_USBD_INT		26
#define IRQ_ISP_INT		27
#define IRQ_NLC_INT		28
#define IRQ_LCDC_INT		29
#define IRQ_SDIO0_INT		30

#define IRQ_BM0_INT		31
#define IRQ_BM1_INT		31
#define IRQ_SDIO1_INT		31
#define IRQ_AXI_BUS_MON		31

/* analog die irq numbers in adi.c */
#define NR_ANA_IRQS		7

#define IRQ_ANA_INT_START	NR_SPRD_IRQS

#define IRQ_ANA_ADC_INT		(IRQ_ANA_INT_START + 0)
#define IRQ_ANA_GPIO_INT	(IRQ_ANA_INT_START + 1)
#define IRQ_ANA_RTC_INT		(IRQ_ANA_INT_START + 2)
#define IRQ_ANA_WDG_INT		(IRQ_ANA_INT_START + 3)
#define IRQ_ANA_TPC_INT		(IRQ_ANA_INT_START + 4)
#define IRQ_ANA_EIC_INT		(IRQ_ANA_INT_START + 5)
#define IRQ_ANA_CHGRWDG_INT	(IRQ_ANA_INT_START + 6)

/* all gpio/eic irq numbers in gpio_eic.c */
#define NR_GPIO_IRQS		(208)

#define GPIO_IRQ_START		(NR_SPRD_IRQS + NR_ANA_IRQS)

/* total irq numbers, the last one is reserved and used in gpio_eic.c */
#define NR_IRQS			(NR_SPRD_IRQS + NR_ANA_IRQS + NR_GPIO_IRQS + 1)

#endif
#endif

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

#ifndef __ASM_ARM_ARCH_GLOBALREGS_H
#define __ASM_ARM_ARCH_GLOBALREGS_H

#include <mach/hardware.h>

/* general global register offset */
#define GR_GEN0			0x0008
#define GR_PCTL			0x000C
#define GR_IRQ			0x0010
#define GR_ICLR			0x0014
#define GR_GEN1			0x0018
#define GR_GEN3			0x001C
#define GR_BOOT_FLAG		0x0020  /* GR_HWRST */
#define GR_MPLL_MN		0x0024
#define GR_PIN_CTL		0x0028
#define GR_GEN2			0x002C
#define GR_ARM_BOOT_ADDR	0x0030
#define GR_STC_STATE		0x0034
#define GR_DPLL_CTRL		0x0040
#define GR_BUSCLK		0x0044  /* GR_BUSCLK_ALM */
#define GR_ARCH_CTL		0x0048
#define GR_SOFT_RST		0x004C  /* GR_SOFT_RST */
#define GR_NFC_MEM_DLY		0x0058
#define GR_CLK_DLY		0x005C
#define GR_GEN4			0x0060
#define GR_POWCTL0		0x0068
#define GR_POWCTL1		0x006C
#define GR_PLL_SCR		0x0070
#define GR_CLK_EN		0x0074
#define GR_CLK_GEN5		0x007C
#define GR_GPU_PWR_CTRL		0x0080
#define GR_MM_PWR_CTRL		0x0084
#define GR_CEVA_RAM_TH_PWR_CTRL	0x0088
#define GR_GSM_PWR_CTRL		0x008C
#define GR_TD_PWR_CTRL		0x0090
#define GR_PERI_PWR_CTRL	0x0094
#define GR_CEVA_RAM_BH_PWR_CTRL	0x0098
#define GR_ARM_SYS_PWR_CTRL	0x009C
#define GR_G3D_PWR_CTRL		0x00A0

#define GR_SWRST		GR_SOFT_RST
#define GR_BUSCLK_ALM		GR_BUSCLK

/* the GEN0 register bit */
#define GEN0_TIMER_EN		BIT(2)
#define GEN0_SIM0_EN		BIT(3)
#define GEN0_I2C_EN		BIT(4)
#define GEN0_I2C0_EN		GEN0_I2C_EN
#define GEN0_GPIO_EN		BIT(5)
#define GEN0_ADI_EN		BIT(6)
#define GEN0_EFUSE_EN		BIT(7)
#define GEN0_KPD_EN		BIT(8)
#define GEN0_EIC_EN		BIT(9)
#define GEN0_MCU_DSP_RST	BIT(10)
#define GEN0_MCU_SOFT_RST	BIT(11)
#define GEN0_I2S_EN		BIT(12)
#define GEN0_I2S0_EN		GEN0_I2S_EN
#define GEN0_PIN_EN		BIT(13)
#define GEN0_CCIR_MCLK_EN	BIT(14)
#define GEN0_EPT_EN		BIT(15)
#define GEN0_SIM1_EN		BIT(16)
#define GEN0_SPI_EN		BIT(17)
#define GEN0_SPI0_EN		GEN0_SPI_EN
#define GEN0_SPI1_EN		BIT(18)
#define GEN0_SYST_EN		BIT(19)
#define GEN0_UART0_EN		BIT(20)
#define GEN0_UART1_EN		BIT(21)
#define GEN0_UART2_EN		BIT(22)
#define GEN0_VB_EN		BIT(23)
#define GEN0_GPIO_RTC_EN	BIT(24)
#define GEN0_EIC_RTC_EN		GEN0_GPIO_RTC_EN
#define GEN0_I2S1_EN		BIT(25)
#define GEN0_KPD_RTC_EN		BIT(26)
#define GEN0_SYST_RTC_EN	BIT(27)
#define GEN0_TMR_RTC_EN		BIT(28)
#define GEN0_I2C1_EN		BIT(29)
#define GEN0_I2C2_EN		BIT(30)
#define GEN0_I2C3_EN		BIT(31)

/* GR_PCTL */
#define IIS0_SEL		BIT(31)
#define IIS1_SEL		BIT(30)
#define MCU_MPLL_EN		BIT(1)

/* GR_GEN1 */
#define GEN1_MPLL_MN_EN		BIT(9)
#define GEN1_CLK_AUX0_EN	BIT(10)
#define GEN1_CLK_AUX1_EN	BIT(11)
#define GEN1_RTC_ARCH_EN	BIT(18)

/* the APB Soft Reset register bit */
#define SWRST_I2C_RST		BIT(0)
#define SWRST_KPD_RST		BIT(1)
#define SWRST_SIM0_RST		BIT(5)
#define SWRST_SIM1_RST		BIT(6)
#define SWRST_TIMER_RST		BIT(8)
#define SWRST_EPT_RST		BIT(10)
#define SWRST_UART0_RST		BIT(11)
#define SWRST_UART1_RST		BIT(12)
#define SWRST_UART2_RST		BIT(13)
#define SWRST_SPI0_RST		BIT(14)
#define SWRST_SPI1_RST		BIT(15)
#define SWRST_IIS_RST		BIT(16)
#define SWRST_IIS1_RST		BIT(17)
#define SWRST_SYST_RST		BIT(19)
#define SWRST_PINREG_RST	BIT(20)
#define SWRST_GPIO_RST		BIT(21)
#define SWRST_ADI_RST		BIT(22)
#define SWRST_VBC_RST		BIT(23)
#define SWRST_PWM0_RST		BIT(24)
#define SWRST_PWM1_RST		BIT(25)
#define SWRST_PWM2_RST		BIT(26)
#define SWRST_PWM3_RST		BIT(27)
#define SWRST_EFUSE_RST		BIT(28)

/* the ARM VB CTRL register bit */
#define ARM_VB_IIS_SEL		BIT(0)
#define ARM_VB_MCLKON		BIT(1)
#define ARM_VB_DA0ON		BIT(2)
#define ARM_VB_DA1ON		BIT(3)
#define ARM_VB_AD0ON		BIT(4)
#define ARM_VB_AD1ON		BIT(5)
#define ARM_VB_ANAON		BIT(6)
#define ARM_VB_ACC		BIT(7)
#define ARM_VB_ADCON		ARM_VB_AD0ON

/* the Interrupt control register bit */
#define IRQ_MCU_IRQ0		BIT(0)
#define IRQ_MCU_FRQ0		BIT(1)
#define IRQ_MCU_IRQ1		BIT(2)
#define IRQ_MCU_FRQ1		BIT(3)
#define IRQ_VBCAD_IRQ		BIT(5)
#define IRQ_VBCDA_IRQ		BIT(6)
#define IRQ_RFT_INT		BIT(12)

/* the Interrupt clear register bit */
#define ICLR_DSP_IRQ0_CLR	BIT(0)
#define ICLR_DSP_FRQ0_CLR	BIT(1)
#define ICLR_DSP_IRQ1_CLR	BIT(2)
#define ICLR_DSP_FIQ1_CLR	BIT(3)
#define ICLR_VBCAD_IRQ_CLR	BIT(5)
#define ICLR_VBCDA_IRQ_CLR	BIT(6)
#define ICLR_RFT_INT_CLR	BIT(12)

/* the Clock enable register bit */
#define CLK_PWM0_EN		BIT(21)
#define CLK_PWM1_EN		BIT(22)
#define CLK_PWM2_EN		BIT(23)
#define CLK_PWM3_EN		BIT(24)
#define CLK_PWM0_SEL		BIT(25)
#define CLK_PWM1_SEL		BIT(26)
#define CLK_PWM2_SEL		BIT(27)
#define CLK_PWM3_SEL		BIT(28)

/* POWER CTL1 */
#define POWCTL1_CONFIG		0x0423F91E  /* isolation number 1ms:30cycles */

/* bits definition for CLK_EN. */
#define	MCU_XTLEN_AUTOPD_EN	BIT(18)
#define	APB_PERI_FRC_CLP	BIT(19)

/* bits definition for GR_STC_STATE. */

#define	GR_EMC_STOP		BIT(0)
#define	GR_MCU_STOP		BIT(1)
#define	GR_DSP_STOP		BIT(2)

/* bits definition for GR_CLK_DLY. */
#define	GR_EMC_STOP_CH5		BIT(4)
#define	GR_EMC_STOP_CH4		BIT(5)
#define	GR_EMC_STOP_CH3		BIT(6)
#define	DSP_DEEP_STOP		BIT(9)
#define	DSP_SYS_STOP		BIT(10)
#define	DSP_AHB_STOP		BIT(11)
#define	DSP_MTX_STOP		BIT(12)
#define	DSP_CORE_STOP		BIT(13)

/* PIN_CTRL */
#define PINCTRL_I2C2_SEL	BIT(8)

/* ****************************************************************** */

/* AHB register offset */
#define AHB_CTL0		0x00
#define AHB_CTL1		0x04
#define AHB_CTL2		0x08
#define AHB_CTL3		0x0C
#define AHB_SOFT_RST		0x10
#define AHB_PAUSE		0x14
#define AHB_REMAP		0x18
#define AHB_ARM_CLK		0x24
#define AHB_SDIO_CTL		0x28
#define AHB_CTL4		0x2C
#define AHB_ENDIAN_SEL		0x30
#define AHB_STS			0x34
#define AHB_CA5_CFG		0x38
#define AHB_DSP_BOOT_EN		0x84
#define AHB_DSP_BOOT_VECTOR	0x88
#define AHB_DSP_RESET		0x8C
#define AHB_ENDIAN_EN		0x90
#define USB_PHY_CTRL		0xA0
#define USB_SPR_REG		0xC0

#define CHIP_ID			0x1FC

/* AHB_CTL0 bits */
#define AHB_CTL0_DCAM_EN	BIT(1)
#define AHB_CTL0_CCIR_EN	BIT(2)
#define AHB_CTL0_LCDC_EN	BIT(3)
#define AHB_CTL0_SDIO_EN	BIT(4)
#define AHB_CTL0_SDIO0_EN	AHB_CTL0_SDIO_EN
#define AHB_CTL0_USBD_EN	BIT(5)
#define AHB_CTL0_DMA_EN		BIT(6)
#define AHB_CTL0_BM0_EN		BIT(7)
#define AHB_CTL0_NFC_EN		BIT(8)
#define AHB_CTL0_BM1_EN		BIT(11)
#define AHB_CTL0_VSP_EN		BIT(13)
#define AHB_CTL0_ROT_EN		BIT(14)
#define AHB_CTL0_DRM_EN		BIT(18)
#define AHB_CTL0_SDIO1_EN	BIT(19)
#define AHB_CTL0_G2D_EN		BIT(20)
#define AHB_CTL0_G3D_EN		BIT(21)
#define AHB_CTL0_AHB_ARCH_EB	BIT(15)
#define AHB_CTL0_EMC_EN		BIT(28)
#define AHB_CTL0_AXIBUSMON0_EN	BIT(29)
#define AHB_CTL0_AXIBUSMON1_EN	BIT(30)

/* AHB_CTRL1 bits */
#define AHB_CTRL1_EMC_AUTO_GATE_EN 	BIT(8)
#define AHB_CTRL1_EMC_CH_AUTO_GATE_EN	BIT(9)
#define AHB_CTRL1_ARM_AUTO_GATE_EN	BIT(11)
#define AHB_CTRL1_AHB_AUTO_GATE_EN	BIT(12)
#define AHB_CTRL1_MCU_AUTO_GATE_EN	BIT(13)
#define AHB_CTRL1_MSTMTX_AUTO_GATE_EN	BIT(14)
#define AHB_CTRL1_ARMMTX_AUTO_GATE_EN	BIT(15)
#define AHB_CTRL1_ARM_DAHB_SLEEP_EN	BIT(16)
#define AHB_CTRL1_DCAM_BUF_SW		BIT(0)

/*AHB_SOFT_RST bits*/
#define AHB_SOFT_RST_CCIR_SOFT_RST		BIT(2)
#define AHB_SOFT_RST_DCAM_SOFT_RST	BIT(1)

/* USB_PHY_CTRL bits */
#define USB_DM_PULLUP_BIT	BIT(19)
#define USB_DP_PULLDOWN_BIT	BIT(20)
#define USB_DM_PULLDOWN_BIT	BIT(21)

/* bit definitions for register DSP_BOOT_EN */
#define	DSP_BOOT_ENABLE		BIT(0)

/* bit definitions for register DSP_RST */
#define	DSP_RESET		BIT(0)

/* bit definitions for register AHB_PAUSE */
#define	MCU_CORE_SLEEP		BIT(0)
#define	MCU_SYS_SLEEP_EN	BIT(1)
#define	MCU_DEEP_SLEEP_EN	BIT(2)
#define	APB_SLEEP		BIT(3)


/* bit definitions for register AHB_STS */
#define	EMC_STOP_CH0		BIT(0)
#define	EMC_STOP_CH1		BIT(1)
#define	EMC_STOP_CH2		BIT(2)
#define	EMC_STOP_CH3		BIT(3)
#define	EMC_STOP_CH4		BIT(4)
#define	EMC_STOP_CH5		BIT(5)
#define	EMC_STOP_CH6		BIT(6)
#define	EMC_STOP_CH7		BIT(7)
#define	EMC_STOP_CH8		BIT(8)
#define	ARMMTX_STOP_CH0		BIT(12)
#define	ARMMTX_STOP_CH1		BIT(13)
#define	ARMMTX_STOP_CH2		BIT(14)
#define	AHB_STS_EMC_STOP	BIT(16)
#define	AHB_STS_EMC_SLEEP	BIT(17)
#define	DMA_BUSY		BIT(18)
#define	DSP_MAHB_SLEEP_EN	BIT(19)
#define	APB_PRI_EN		BIT(20)

/*********************************************************************/
#define CHIP_ID_8810S		(0x88100001UL)	//Smic
/*********************************************************************/


#ifdef CONFIG_ARCH_SC7710
#define GR_MPLL_CTRL1		0x0038
#define GR_TDPLL_CTRL		0x003C
#define GR_CLK26M_ANA_CTRL    0x0078
#define GR_CLK_GEN6                    0x00A4
#define GR_PERI_SOFT_RST2        0x00A8
#define GR_AUD_CTRL                   0x00AC
#define GR_PIN_CTRL2                   0x00B0
#define GR_CLK_GEN7                    0x00B4
#define GR_IRAM_POW_CTRL        0x00B8
#define GR_MCU_SLP                      0x00BC
#define GR_DSP_SLP                       0x00C0
#define GR_MEM_SLP                      0x00C4
#define GR_JTAG_CTRL                  0x00C8
#define GR_PAD_SMT_EN0              0x00CC
#define GR_PAD_SMT_EN1              0x00D0
#define GR_PAD_SMT_EN2              0x00D4
#define GR_PAD_SMT_EN3              0x00D8
#define GR_PAD_SMT_EN4              0x00DC
#define GR_PAD_SMT_EN5              0x00E0

/* bits definitions for register REG_AHB_CTL0 */
#define BIT_DCAM_EN	        	BIT(1)
#define BIT_CCIR_EN	       	 	BIT(2)
#define BIT_LCDC_EN	        	BIT(3)
#define BIT_SDIO0_EN	        	BIT(4)
#define BIT_USBD_EN	        	BIT(5)
#define BIT_DMA_EN			BIT(6)
#define BIT_BM0_EN			BIT(7)
#define BIT_NFC_EN			BIT(8)
#define BIT_BM1_EN			BIT(11)
#define BIT_VSP_EN			BIT(13)
#define BIT_ROT_EN			BIT(14)
#define BIT_DRM_EN			BIT(18)
#define BIT_SDIO1_EN	        	BIT(19)
#define BIT_G2D_EN			BIT(20)
#define BIT_G3D_EN			BIT(21)
#define BIT_AHB_ARCH_EB		BIT(27)
#define BIT_EMC_EN			BIT(28)
#define BIT_AXIBUSMON0_EN	BIT(29)
#define BIT_AXIBUSMON1_EN	BIT(30)

/* bits definitions for register REG_AHB_CTRL1 */
#define BIT_DCAM_BUF_SW		        BIT(0)
#define BIT_DCAM_BUF_STATUS    	BIT(1)
#define BIT_EMC_AUTO_GATE_EN 	BIT(8)
#define BIT_EMC_CH_AUTO_GATE_EN	BIT(9)
#define BIT_ARM_AUTO_GATE_EN	BIT(11)
#define BIT_AHB_AUTO_GATE_EN	BIT(12)
#define BIT_MCU_AUTO_GATE_EN	BIT(13)
#define BIT_MSTMTX_AUTO_GATE_EN	BIT(14)
#define BIT_ARMMTX_AUTO_GATE_EN	BIT(15)
#define BIT_ARM_DAHB_SLEEP_EN	BIT(16)

/* bits definitions for register REG_AHB_CTRL2 */
#define BIT_FUNC_LOW_SPEED	        ( BIT(0) )
#define BITS_MCU_SHM0_CTRL(_x_)	( (_x_) << 3 & (BIT(3) | BIT(4)) )
#define MASK_MCU_SHM0_CTRL	        (BIT(3) | BIT(4))
#define SHIFT_MCU_SHM0_CTRL	        3

/* bits definitions for register REG_AHB_AHB_CTL3 */
#define BIT_CLK_ULPI_EN                 ( BIT(10) )
#define BIT_UTMI_SUSPEND_INV            ( BIT(9) )
#define BIT_UTMIFS_TX_EN_INV            ( BIT(8) )
#define BIT_CLK_UTMIFS_EN               ( BIT(7) )
#define BIT_CLK_USB_REF_EN              ( BIT(6) )
#define BIT_BUSMON_SEL1                 ( BIT(5) )
#define BIT_BUSMON_SEL0                 ( BIT(4) )
#define BIT_USB_M_HBIGENDIAN            ( BIT(2) )
#define BIT_USB_S_HBIGEIDIAN            ( BIT(1) )
#define BIT_CLK_USB_REF_SEL             ( BIT(0) )

/* bits definitions for register REG_AHB_SOFT_RST */
#define BIT_G3D_SOFT_RST                ( BIT(19) )
#define BIT_DBG_SOFT_RST                ( BIT(18) )
#define BIT_SD1_SOFT_RST                ( BIT(16) )
#define BIT_VSP_SOFT_RST                ( BIT(15) )
#define BIT_ADC_SOFT_RST                ( BIT(14) )
#define BIT_DRM_SOFT_RST                ( BIT(13) )
#define BIT_SD0_SOFT_RST                ( BIT(12) )
#define BIT_EMC_SOFT_RST                ( BIT(11) )
#define BIT_ROT_SOFT_RST                ( BIT(10) )
#define BIT_USBPHY_SOFT_RST           ( BIT(7) )
#define BIT_USBD_UTMI_SOFT_RST     ( BIT(6) )
#define BIT_NFC_SOFT_RST                ( BIT(5) )
#define BIT_LCDC_SOFT_RST               ( BIT(3) )
#define BIT_CCIR_SOFT_RST               ( BIT(2) )
#define BIT_DCAM_SOFT_RST              ( BIT(1) )
#define BIT_DMA_SOFT_RST                ( BIT(0) )

/* bit definitions for register AHB_PAUSE */
#define	BIT_MCU_CORE_SLEEP		BIT(0)
#define	BIT_MCU_SYS_SLEEP_EN	BIT(1)
#define	BIT_MCU_DEEP_SLEEP_EN	BIT(2)
#define	BIT_APB_SLEEP		        BIT(3)

/* bits definitions for register REG_AHB_REMAP */
#define BITS_ARM_RES_STRAPPIN(_x_)      ( (_x_) << 30 & (BIT(30)|BIT(31)) )
#define BIT_ARM_BIGEND_SEL       		( BIT(8) )
#define BIT_FUNC_TEST_MODE              ( BIT(7) )
#define BIT_ARM_BOOT_MD3                ( BIT(6) )
#define BIT_ARM_BOOT_MD2                ( BIT(5) )
#define BIT_ARM_BOOT_MD1                ( BIT(4) )
#define BIT_ARM_BOOT_MD0                ( BIT(3) )
#define BIT_USB_DLOAD_EN                 ( BIT(2) )
#define BIT_USB_HIGHSPEED_SEL		( BIT(1) )
#define BIT_REMAP                                ( BIT(0) )

/* bits definitions for register REG_AHB_SOFT2_RST */
#define BIT_DISPC_SOFT_RST              ( BIT(2) )
#define BIT_EMMC_SOFT_RST               ( BIT(1) )
#define BIT_SD2_SOFT_RST                 ( BIT(0) )

/* bits definitions for register REG_AHB_ARM_CLK */
#define BITS_AHB_DIV_INUSE(_x_)         ( (_x_) << 27 & (BIT(27)|BIT(28)|BIT(29)) )
#define BIT_AHB_ERR_YET                 ( BIT(26) )
#define BIT_AHB_ERR_CLR                 ( BIT(25) )
#define BITS_CLK_MCU_SEL(_x_)           ( (_x_) << 23 & (BIT(23)|BIT(24)) )
#define BITS_CLK_DBG_DIV(_x_)           ( (_x_) << 14 & (BIT(14)|BIT(15)|BIT(16)) )
#define BITS_CLK_EMC_SEL(_x_)           ( (_x_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_CLK_EMC_DIV(_x_)           ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CLK_AHB_DIV(_x_)           ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BIT_CLK_EMC_SYNC_SEL           ( BIT(3) )
#define BITS_CLK_ARM_DIV(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AHB_SDIO_CTRL */
#define BITS_SDIO1_SLOT_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_SDIO0_SLOT_SEL(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AHB_CTL4 */
#define BIT_RX_CLK_SEL_ARM              ( BIT(31) )
#define BIT_RX_CLK_INV_ARM              ( BIT(30) )
#define BIT_RX_INV                               ( BIT(29) )

/* bits definitions for register REG_AHB_CTL5 */
#define BIT_SDIO1_BIGEND_EN             ( BIT(11) )
#define BIT_SHRAM0_BIGEND_EN         ( BIT(9) )
#define BIT_BUSMON1_BIGEND_EN       ( BIT(8) )
#define BIT_BUSMON0_BIGEND_EN       ( BIT(7) )
#define BIT_ROT_BIGEND_EN                ( BIT(6) )
#define BIT_VSP_BIGEND_EN                 ( BIT(5) )
#define BIT_DCAM_BIGEND_EN             ( BIT(4) )
#define BIT_SDIO0_BIGEND_EN            ( BIT(3) )
#define BIT_LCDC_BIGEND_EN              ( BIT(2) )
#define BIT_NFC_BIGEND_EN                ( BIT(1) )
#define BIT_DMA_BIGEND_EN               ( BIT(0) )

/* bit definitions for register AHB_STATUS */
#define	BIT_EMC_STOP_CH0		BIT(0)
#define	BIT_EMC_STOP_CH1		BIT(1)
#define	BIT_EMC_STOP_CH2		BIT(2)
#define	BIT_EMC_STOP_CH3		BIT(3)
#define	BIT_EMC_STOP_CH4		BIT(4)
#define	BIT_EMC_STOP_CH5		BIT(5)
#define	BIT_EMC_STOP_CH6		BIT(6)
#define	BIT_EMC_STOP_CH7		BIT(7)
#define	BIT_EMC_STOP_CH8		BIT(8)
#define	BIT_ARMMTX_STOP_CH0	BIT(12)
#define	BIT_ARMMTX_STOP_CH1	BIT(13)
#define	BIT_ARMMTX_STOP_CH2	BIT(14)
#define	BIT_AHB_STS_EMC_STOP	BIT(16)
#define	BIT_AHB_STS_EMC_SLEEP	BIT(17)
#define	BIT_DMA_BUSY		        BIT(18)
#define	BIT_DSP_MAHB_SLEEP_EN	BIT(19)
#define	BIT_APB_PRI_EN		        BIT(20)

/* bits definitions for register REG_AHB_CA5_CFG */
#define BITS_CA5_CLK_AXI_DIV(_x_)   ( (_x_) << 11 & (BIT(11)|BIT(12)) )
#define BIT_CA5_CLK_DBG_EN_SEL      ( BIT(10) )
#define BIT_CA5_CLK_DBG_EN              ( BIT(9) )
#define BIT_CA5_DBGEN                        ( BIT(8) )
#define BIT_CA5_NIDEN                         ( BIT(7) )
#define BIT_CA5_SPIDEN                       ( BIT(6) )
#define BIT_CA5_SPNIDEN                    ( BIT(5) )
#define BIT_CA5_CPI15DISABLE           ( BIT(4) )
#define BIT_CA5_TEINIT                       ( BIT(3) )
#define BIT_CA5_L1RSTDISABLE          ( BIT(2) )
#define BIT_CA5_L2CFGEND                 ( BIT(1) )
#define BIT_CA5_L2SPNIDEN                ( BIT(0) )

/* bits definitions for register REG_AHB_CTL6 */
#define BITS_CLK_DISPCPLL_SEL(_x_)              ( (_x_) << 30 & (BIT(30)|BIT(31)) )
#define BITS_CLK_DISPC_DIV(_x_)              ( (_x_) << 27 & (BIT(27)|BIT(28)|BIT(29)) )
#define BITS_CLK_DISPC_DBIPLL_SEL(_x_)              ( (_x_) << 25 & (BIT(25)|BIT(26)) )
#define BITS_CLK_DISPC_DBI_DIV(_x_)              ( (_x_) << 22 & (BIT(22)|BIT(23)|BIT(24)) )
#define BITS_CLK_DISPC_DPIPLL_SEL(_x_)              ( (_x_) << 20 & (BIT(20)|BIT(21)) )
#define BITS_CLK_DISPC_DPI_DIV(_x_)              ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_SDIO2_SLOT_SEL(_x_)              ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_EMMC_SLOT_SEL        ( BIT(9) )
#define BIT_SDIO2_BIGEND_EN	( BIT(8) )
#define BIT_EMMC_BIGEND_EN	( BIT(7) )
#define BIT_SDIO2_EB                   ( BIT(2) )
#define BIT_EMMC_EB                    ( BIT(1) )
#define BIT_DISPC_EB                    ( BIT(0) )

/* bits definitions for register REG_AHB_DSP_JTAG_CTRL */
#define BIT_CEVA_SW_JTAG_ENA               ( BIT(8) )
#define BIT_STDO			( BIT(4) )
#define BIT_STCK			( BIT(3) )
#define BIT_STMS                  ( BIT(2) )
#define BIT_STDI                  	( BIT(1) )
#define BIT_STRTCK              ( BIT(0) )

/* bits definitions for register REG_AHB_DSP_BOOT_EN */
#define BIT_ASHB_ARMTODSP_EN_I            ( BIT(2) )
#define BIT_FRC_CLK_DSP_EN                     ( BIT(1) )
#define BIT_DSP_BOOT_EN                   	( BIT(0) )

/* bits definitions for register REG_AHB_DSP_RESET */
#define BIT_DSP_SYS_SRST                ( BIT(1) )
#define BIT_DSP_CORE_SRST_N        ( BIT(0) )

/* bits definitions for register REG_AHB_ENDIAN_EN */
#define BIT_AHB_BIGEND_PROT           ( BIT(31) )
#define BITS_BIGEND_PROT_VAL(_x_) ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)))
#define MASK_BIGEND_PROT_VAL        (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15))
#define SHIFT_BIGEND_PROT_VAL       0

/* bits definitions for register REG_AHB_USB_PHY_CTRL */
#define BIT_DMPULLUP			          ( BIT(21) )
#define BIT_DPPULLUP			          ( BIT(20) )
#define BIT_USB_DM_PULLUP		  ( BIT(19) )
#define BIT_COMMONONN		          ( BIT(18) )
#define BITS_TXHSXVTUNE(_x_)             ( (_x_) << 16 & (BIT(16)|BIT(17)))
#define BITS_TXVREFTUNE(_x_)             ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)))
#define BIT_TXPREEMPHASISTUNE         ( BIT(8) )
#define BITS_TXFSLSTUNE(_x_)             ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_SQRXTUNE(_x_)                ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

#ifdef AHB_CTL0_AHB_ARCH_EB
#undef AHB_CTL0_AHB_ARCH_EB
#endif

#define AHB_SOFT2_RST	        0x1C
#define AHB_CTL5			        0x30
#define AHB_CTL6			        0x3C
#define AHB_DSP_JTAG_CTRL	0x80

#define AHB_CTL0_AHB_ARCH_EB	BIT_AHB_ARCH_EB

/********************************************************************
**********************                                  ******************************
********************** GLOBAL REGISTERS ******************************
**********************                                  ******************************
********************************************************************/

/* bits definitions for register REG_GLB_PCTRL */
#define BIT_IIS0_CTL_SEL                ( BIT(31) )
#define BIT_IIS1_CTL_SEL                ( BIT(30) )
#define BITS_CLK_AUX1_DIV(_x_)          ( (_x_) << 22 & (BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)) )
#define BITS_STRAP_BITS(_x_)                ( (_x_) << 11 & (BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_ROM_FORCE_ON                ( BIT(10) )
/* All clock gatings will be invalid ,ad then all clock enable, for debug use */
#define BIT_CLK_ALL_EN                  ( BIT(9) )
/* Owner selection for UART1, <0: ARM control, 1: DSP control> */
#define BIT_UART1_CTL_SEL               ( BIT(8) )
#define BIT_ARM_JTAG_EN                 ( BIT(4) )
#define BIT_MCU_DPLL_EN                 ( BIT(3) )
#define BIT_MCU_TDPLL_EN                ( BIT(2) )
#define BIT_MCU_MPLL_EN                 ( BIT(1) )
/* MCU force deepsleep. for debug use. */
#define BIT_MCU_FORECE_DEEP_SLEEP       ( BIT(0) )

/* bits definitions for register REG_GLB_INT_CTRL */
#define BIT_MCU_IRQ0		BIT(0)
#define BIT_MCU_FRQ0		BIT(1)
#define BIT_MCU_IRQ1		BIT(2)
#define BIT_MCU_FRQ1		BIT(3)
#define BIT_VBCAD_IRQ		BIT(5)
#define BIT_VBCDA_IRQ		BIT(6)
#define BIT_RFT_INT		        BIT(12)

/* bits definitions for register REG_GLB_INT_CLR*/
#define BIT_DSP_IRQ0_CLR	BIT(0)
#define BIT_DSP_FRQ0_CLR	BIT(1)
#define BIT_DSP_IRQ1_CLR	BIT(2)
#define BIT_DSP_FIQ1_CLR	BIT(3)
#define BIT_VBCAD_IRQ_CLR	BIT(5)
#define BIT_VBCDA_IRQ_CLR	BIT(6)
#define BIT_RFT_INT_CLR	BIT(12)

/* bits definitions for register REG_GLB_GEN1 */
#define BIT_RTC_ARCH_EB                ( BIT(18) )
#define BIT_CLK_AUX1_EN                 ( BIT(11) )
#define BIT_CLK_AUX0_EN                 ( BIT(10) )
#define BIT_MPLL_CTL_WE                 ( BIT(9) )
#define BITS_CLK_AUX0_DIV(_x_)     ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_GLB_GEN3 */
#define BITS_CCIR_MCLK_DIV(_x_)         ( (_x_) << 24 & (BIT(24)|BIT(25)) )
#define BIT_JTAG_DAISY_EN               ( BIT(23) )
#define BITS_CLK_IIS1_DIV(_x_)          ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_BOND_OPT(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_GLB_BOOT_FLAG */
#define BITS_HWRST(_x_)                 ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

#define SHFT_HWRST                      ( 8 )
#define MASK_HWRST                      ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

/* bits definitions for register REG_GLB_M_PLL_CTL0 */
#define BIT_MOD_EN            ( BIT(25) )
#define BIT_SDM_EN            ( BIT(24) )
#define BITS_MPLL_NINT(_x_)            ( (_x_) << 18 & (BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_MPLL_REFIN(_x_)            ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_MPLL_LPF(_x_)              ( (_x_) << 13 & (BIT(13)|BIT(14)|BIT(15)) )
#define BITS_MPLL_IBIAS(_x_)            ( (_x_) << 11 & (BIT(11)|BIT(12)) )
#define BITS_MPLL_N(_x_)                ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

#define SHFT_MPLL_N                     ( 0 )
#define MASK_MPLL_N                     ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10) )

/* bits definitions for register REG_GLB_PINCTL */
#define BIT_DJTAG_PIN_IN_SEL           ( BIT(12) )
#define BIT_SPI1_PIN_IN_SEL               ( BIT(11) )
#define BIT_SPI0_PIN_IN_SEL               ( BIT(10) )
#define BIT_I2C2_PIN_IN_SEL               ( BIT(8) )
#define BIT_UART1_PIN_IN_SEL            ( BIT(6) )
#define BITS_I2C3_PIN_IN_SEL(_x_)             ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BIT_IF_SPR_IN                             ( BIT(2) )
#define BIT_IF_SPR_OE                            ( BIT(1) )
#define BIT_IF_SPR_OUT                         ( BIT(0) )

/* bits definitions for register REG_GLB_GEN2 */
#define BITS_CLK_IIS0_DIV(_x_)          ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_CLK_SPI0_DIV(_x_)          ( (_x_) << 21 & (BIT(21)|BIT(22)|BIT(23)) )
#define BITS_CLK_GPU_2X_DIV(_x_)         ( (_x_) << 17 & (BIT(17)|BIT(18)|BIT(19)) )
#define BITS_CLK_GPU_AXI_DIV(_x_)       ( (_x_) << 14 & (BIT(14)|BIT(15)|BIT(16)) )
#define BITS_CLK_SPI1_DIV(_x_)          ( (_x_) << 11 & (BIT(11)|BIT(12)|BIT(13)) )
#define BITS_CLK_NFC_DIV(_x_)           ( (_x_) << 6 & (BIT(6)|BIT(7)|BIT(8)) )
#define BITS_CLK_NFC_SEL(_x_)                ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_CLK_GPU_2X_SEL(_x_)         ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_CLK_GPU_AXI_SEL(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_GLB_ARMBOOT */
#define BITS_ARMBOOT_ADDR(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

#define SHFT_ARMBOOT_ADDR               ( 0 )
#define MASK_ARMBOOT_ADDR               ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

/* bits definitions for register REG_GLB_STC_DSP_ST */
#define BITS_STC_DSP_STATE(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_GLB_MPLL_CTRL1 */
#define BITS_MPLL_KINT(_x_)            ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_MPLL_KDELTA(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)) )

#define SHFT_MPLL_KDELTA                ( 0 )
#define MASK_MPLL_KDELTA               (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11))

#define SHFT_MPLL_KINT                     ( 12 )
#define MASK_MPLL_KINT                    (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31))

/* bits definitions for register REG_GLB_TD_PLL_CTL */
#define BIT_TDPLL_DIV2OUT_FORCE_PD      ( BIT(11) )
#define BIT_TDPLL_DIV3OUT_FORCE_PD      ( BIT(10) )
#define BIT_TDPLL_DIV4OUT_FORCE_PD      ( BIT(9) )
#define BIT_TDPLL_DIV5OUT_FORCE_PD      ( BIT(8) )
#define BITS_TDPLL_REFIN(_x_)           ( (_x_) << 5 & (BIT(5)|BIT(6)) )
#define BITS_TDPLL_LPF(_x_)             ( (_x_) << 2 & (BIT(2)|BIT(3)|BIT(4)) )
#define BITS_TDPLL_IBIAS(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_GLB_D_PLL_CTL */
#define BITS_DPLL_REFIN(_x_)           ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_DPLL_LPF(_x_)                ( (_x_) << 13 & (BIT(13)|BIT(14)|BIT(15)) )
#define BITS_DPLL_IBIAS(_x_)            ( (_x_) << 11 & (BIT(11)|BIT(12)) )
#define BITS_DPLL_N(_x_)                    ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

#define SHFT_DPLL_N                             ( 0 )
#define MASK_DPLL_N                            ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10) )

/* bits definitions for register REG_GLB_BUSCLK */
#define BITS_PWRON_DLY_CTRL(_x_)  ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)) )
#define BIT_ARM_VB_ACC                     ( BIT(7) )
#define BIT_ARM_VB_ANAON               ( BIT(6) )
#define BIT_ARM_VB_AD1ON               ( BIT(5) )
#define BIT_ARM_VB_AD0ON               ( BIT(4) )
#define BIT_ARM_VB_DA1ON               ( BIT(3) )
#define BIT_ARM_VB_DA0ON               ( BIT(2) )
#define BIT_ARM_VB_MCLKON             ( BIT(1) )
#define BIT_VBDA_IIS_DATA_SEL       ( BIT(0) )

/* bits definitions for register REG_GLB_ARCH */
#define BIT_ARCH_EB                     ( BIT(10) )

/* bits definitions for register REG_GLB_SOFT_RST */
#define BIT_UART3_RST               ( BIT(30) )
#define BIT_EIC_RST                     ( BIT(29) )
#define BIT_EFUSE_RST                ( BIT(28) )
#define BIT_PWM3_RST                 ( BIT(27) )
#define BIT_PWM2_RST                 ( BIT(26) )
#define BIT_PWM1_RST                 ( BIT(25) )
#define BIT_PWM0_RST                 ( BIT(24) )
#define BIT_VBC_RST                     ( BIT(23) )
#define BIT_ADI_RST                     ( BIT(22) )
#define BIT_GPIO_RST                   ( BIT(21) )
#define BIT_PINREG_RST              ( BIT(20) )
#define BIT_SYST0_RST                ( BIT(19) )
#define BIT_IIS1_RST                    ( BIT(17) )
#define BIT_IIS0_RST                    ( BIT(16) )
#define BIT_SPI1_RST                    ( BIT(15) )
#define BIT_SPI0_RST                    ( BIT(14) )
#define BIT_UART2_RST                ( BIT(13) )
#define BIT_UART1_RST                ( BIT(12) )
#define BIT_UART0_RST                ( BIT(11) )
#define BIT_EPT_RST                      ( BIT(10) )
#define BIT_TMR_RST                     ( BIT(8) )
#define BIT_AP2CP_RST                 ( BIT(6) )
#define BIT_SIM0_RST                    ( BIT(5) )
#define BIT_I2C3_RST                    ( BIT(4) )
#define BIT_I2C2_RST                    ( BIT(3) )
#define BIT_I2C1_RST                    ( BIT(2) )
#define BIT_KPD_RST                     ( BIT(1) )
#define BIT_I2C0_RST                    ( BIT(0) )

/* bits definitions for register REG_GLB_NFCMEMDLY */
#define BITS_NFC_MEM_DLY(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )

/* bits definitions for register REG_GLB_CLKDLY */
#define BITS_CLK_SPI1_SEL(_x_)              ( (_x_) << 30 & (BIT(30)|BIT(31)) )
#define BIT_CLK_ADI_EN_ARM                  ( BIT(29) )
#define BIT_CLK_ADI_SEL                           ( BIT(28) )
#define BITS_CLK_SPI0_SEL(_x_)              ( (_x_) << 26 & (BIT(26)|BIT(27)) )
#define BITS_CLK_UART2_SEL(_x_)          ( (_x_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_CLK_UART1_SEL(_x_)          ( (_x_) << 22 & (BIT(22)|BIT(23)) )
#define BITS_CLK_UART0_SEL(_x_)          ( (_x_) << 20 & (BIT(20)|BIT(21)) )
#define BITS_CLK_CCIR_DLY_SEL(_x_)    ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_CLK_APB_SEL(_x_)                ( (_x_) << 14 & (BIT(14)|BIT(15)) )
#define BIT_MCU_VBC_RST                          ( BIT(2) )
#define BIT_CHIP_SLEEP_REC_ARM          ( BIT(1) )
#define BIT_CHIP_SLP_ARM_CLR               ( BIT(0) )

/* bits definitions for register REG_GLB_GEN4 */
#define BIT_XTLBUF_WAIT_SEL                ( BIT(31) )
#define BIT_PLL_WAIT_SEL                        ( BIT(30) )
#define BIT_XTL_WAIT_SEL                        ( BIT(29) )
#define BITS_ARM_XTLBUF_WAIT(_x_)   ( (_x_) << 21 & (BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)) )
#define BITS_ARM_PLL_WAIT(_x_)          ( (_x_) << 13 & (BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BITS_ARM_XTL_WAIT(_x_)          ( (_x_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_CLK_LCDC_DIV(_x_)            ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_GLB_POWCTL0 */
#define BITS_ARM_PWR_ON_DLY(_x_)         ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_ARM_SLP_POWOFF_AUTO_EN ( BIT(23) )
#define BITS_ARM_ISO_ON_NUM(_x_)         ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ARM_ISO_OFF_NUM(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_ARM_PWR_ON_DLY              ( 24 )
#define MASK_ARM_PWR_ON_DLY             ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_ARM_ISO_ON_NUM              ( 8 )
#define MASK_ARM_ISO_ON_NUM             ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_ARM_ISO_OFF_NUM             ( 0 )
#define MASK_ARM_ISO_OFF_NUM            ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_POWCTL1 */
#define BIT_DSP_ROM_FORCE_PD             ( BIT(5) )
#define BIT_MCU_ROM_FORCE_PD            ( BIT(4) )
#define BIT_DSP_ROM_SLP_PD_EN            ( BIT(2) )
#define BIT_MCU_ROM_SLP_PD_EN           ( BIT(0) )

/* bits definitions for register REG_GLB_PLL_SCR */
#define BITS_CLK_CCIRPLL_SEL(_x_)     ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_CLK_IIS2PLL_SEL(_x_)       ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_CLK_IIS1PLL_SEL(_x_)       ( (_x_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_CLK_AUX1PLL_SEL(_x_)     ( (_x_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_CLK_AUX0PLL_SEL(_x_)     ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_CLK_IIS0PLL_SEL(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_CLK_LCDPLL_SEL(_x_)        ( (_x_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_CLK_DCAMPLL_SEL(_x_)    ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_CLK_VSPPLL_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )

/* bits definitions for register REG_GLB_CLK_EN */
#define BIT_CLK_PWM3_SEL                ( BIT(28) )
#define BIT_CLK_PWM2_SEL                ( BIT(27) )
#define BIT_CLK_PWM1_SEL                ( BIT(26) )
#define BIT_CLK_PWM0_SEL                ( BIT(25) )
#define BIT_PWM3_EB                     ( BIT(24) )
#define BIT_PWM2_EB                     ( BIT(23) )
#define BIT_PWM1_EB                     ( BIT(22) )
#define BIT_PWM0_EB                     ( BIT(21) )
#define BIT_APB_PERI_FRC_ON             ( BIT(20) )
#define BIT_APB_PERI_FRC_SLP            ( BIT(19) )
#define BIT_MCU_XTLEN_AUTOPD_EN  ( BIT(18) )
#define BITS_BUFON_CTRL(_x_)            ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BIT_CLK_TDCAL_EN                    ( BIT(9) )
#define BIT_CLK_TDFIR_EN                     ( BIT(7) )

/* bits definitions for register REG_GLB_CLK26M_ANA_CTL */
#define BITS_REC_CLK26MHZ_RESERVE(_x_)    ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BIT_REC_CLK26MHZ_CUR_SEL                ( BIT(4) )
#define BIT_REC_CLK26MHZ_BUF_AUTO_EN     ( BIT(3) )
#define BIT_REC_CLK26MHZ_BUF_FORCE_PD   ( BIT(2) )
#define BIT_CLK26M_ANA_SEL                               ( BIT(1) )
#define BIT_CLK26M_ANA_FORCE_EN                 ( BIT(0) )

/* bits definitions for register REG_GLB_CLK_GEN5 */
#define BITS_CLK_SDIO1PLL_SEL(_x_)      ( (_x_) << 19 & (BIT(19)|BIT(20)) )
#define BITS_CLK_SDIO0PLL_SEL(_x_)      ( (_x_) << 17 & (BIT(17)|BIT(18)) )
#define BIT_LDO_USB_PD                              ( BIT(9) )
#define BITS_CLK_UART2_DIV(_x_)            ( (_x_) << 6 & (BIT(6)|BIT(7)|BIT(8)) )
#define BITS_CLK_UART1_DIV(_x_)            ( (_x_) << 3 & (BIT(3)|BIT(4)|BIT(5)) )
#define BITS_CLK_UART0_DIV(_x_)            ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_GLB_GPU_PWR_CTRL */
#define BITS_GPU_PWR_ON_DLY(_x_)          ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_GPU_POW_FORCE_PD                ( BIT(23) )
#define BIT_GPU_SLP_POWOFF_AUTO_EN   ( BIT(22) )
#define BITS_GPU_ISO_ON_NUM(_x_)          ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_GPU_ISO_OFF_NUM(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_GPU_PWR_ON_DLY                  ( 24 )
#define MASK_GPU_PWR_ON_DLY                 ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_GPUMM_ISO_ON_NUM             ( 8 )
#define MASK_GPU_ISO_ON_NUM                  ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_GPUMM_ISO_OFF_NUM           ( 0 )
#define MASK_GPU_ISO_OFF_NUM                ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_MM_PWR_CTL */
#define BITS_MM_PWR_ON_DLY(_x_)          ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_MM_POW_FORCE_PD                ( BIT(23) )
#define BIT_MM_SLP_POWOFF_AUTO_EN   ( BIT(22) )
#define BITS_MM_ISO_ON_NUM(_x_)          ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_MM_ISO_OFF_NUM(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_MM_PWR_ON_DLY                  ( 24 )
#define MASK_MM_PWR_ON_DLY                 ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_MM_ISO_ON_NUM                  ( 8 )
#define MASK_MM_ISO_ON_NUM                 ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_MM_ISO_OFF_NUM                ( 0 )
#define MASK_MM_ISO_OFF_NUM               ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_CEVA_RAM_TH_PWR_CTL */
#define BITS_CEVA_RAM_TH_PWR_ON_DLY(_x_) ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_CEVA_RAM_TH_POW_FORCE_PD     ( BIT(23) )
#define BIT_CEVA_RAM_TH_SLP_POWOFF_AUTO_EN ( BIT(22) )
#define BITS_CEVA_RAM_TH_ISO_ON_NUM(_x_) ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_CEVA_RAM_TH_ISO_OFF_NUM(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_CEVA_RAM_TH_PWR_ON_DLY       ( 24 )
#define MASK_CEVA_RAM_TH_PWR_ON_DLY      ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_CEVA_RAM_TH_ISO_ON_NUM       ( 8 )
#define MASK_CEVA_RAM_TH_ISO_ON_NUM      ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_CEVA_RAM_TH_ISO_OFF_NUM      ( 0 )
#define MASK_CEVA_RAM_TH_ISO_OFF_NUM     ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_GSM_PWR_CTL */
#define BITS_GSM_PWR_ON_DLY(_x_)        ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_MCU_GSM_POW_FORCE_PD        ( BIT(23) )
#define BITS_GSM_ISO_ON_NUM(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_GSM_ISO_OFF_NUM(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_GSM_PWR_ON_DLY              ( 24 )
#define MASK_GSM_PWR_ON_DLY             ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_GSM_ISO_ON_NUM              ( 8 )
#define MASK_GSM_ISO_ON_NUM             ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_GSM_ISO_OFF_NUM             ( 0 )
#define MASK_GSM_ISO_OFF_NUM            ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_TD_PWR_CTL */
#define BITS_TD_PWR_ON_DLY(_x_)         ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_MCU_TD_POW_FORCE_PD         ( BIT(23) )
#define BITS_TD_ISO_ON_NUM(_x_)         ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_TD_ISO_OFF_NUM(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_TD_PWR_ON_DLY               ( 24 )
#define MASK_TD_PWR_ON_DLY              ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_TD_ISO_ON_NUM               ( 8 )
#define MASK_TD_ISO_ON_NUM              ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_TD_ISO_OFF_NUM              ( 0 )
#define MASK_TD_ISO_OFF_NUM             ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_PERI_PWR_CTL */
#define BITS_PERI_PWR_ON_DLY(_x_)       ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_PERI_POW_FORCE_PD           ( BIT(23) )
#define BIT_PERI_SLP_POWOFF_AUTO_EN     ( BIT(22) )
#define BITS_PERI_ISO_ON_NUM(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PERI_ISO_OFF_NUM(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_PERI_PWR_ON_DLY             ( 24 )
#define MASK_PERI_PWR_ON_DLY            ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_PERI_ISO_ON_NUM             ( 8 )
#define MASK_PERI_ISO_ON_NUM            ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_PERI_ISO_OFF_NUM           ( 0 )
#define MASK_PERI_ISO_OFF_NUM           ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )


/* bits definitions for register REG_GLB_CEVA_RAM_BH_PWR_CTRL */
#define BITS_CEVA_RAM_BH_PWR_ON_DLY(_x_) ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_CEVA_RAM_BH_POW_FORCE_PD     ( BIT(23) )
#define BIT_CEVA_RAM_BH_SLP_POWOFF_AUTO_EN ( BIT(22) )
#define BITS_CEVA_RAM_BH_ISO_ON_NUM(_x_) ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_CEVA_RAM_BH_ISO_OFF_NUM(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_CEVA_RAM_BH_PWR_ON_DLY       ( 24 )
#define MASK_CEVA_RAM_BH_PWR_ON_DLY      ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_CEVA_RAM_BH_ISO_ON_NUM       ( 8 )
#define MASK_CEVA_RAM_BH_ISO_ON_NUM      ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_CEVA_RAM_BH_ISO_OFF_NUM      ( 0 )
#define MASK_CEVA_RAM_BH_ISO_OFF_NUM     ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_ARM_SYS_PWR_CTL */
#define BITS_ARM_SYS_PWR_ON_DLY(_x_)    ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_ARM_SYS_POW_FORCE_PD        ( BIT(23) )
#define BIT_ARM_SYS_SLP_POWOFF_AUTO_EN  ( BIT(22) )
#define BITS_ARM_SYS_ISO_ON_NUM(_x_)    ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ARM_SYS_ISO_OFF_NUM(_x_)   ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_ARM_SYS_PWR_ON_DLY          ( 24 )
#define MASK_ARM_SYS_PWR_ON_DLY         ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_ARM_SYS_ISO_ON_NUM          ( 8 )
#define MASK_ARM_SYS_ISO_ON_NUM         ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_ARM_SYS_ISO_OFF_NUM         ( 0 )
#define MASK_ARM_SYS_ISO_OFF_NUM        ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_G3D_PWR_CTL */
#define BITS_G3D_PWR_ON_DLY(_x_)        ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_G3D_POW_FORCE_PD            ( BIT(23) )
#define BIT_G3D_SLP_POWOFF_AUTO_EN      ( BIT(22) )
#define BITS_G3D_ISO_ON_NUM(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_G3D_ISO_OFF_NUM(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_G3D_PWR_ON_DLY              ( 24 )
#define MASK_G3D_PWR_ON_DLY             ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_G3D_ISO_ON_NUM              ( 8 )
#define MASK_G3D_ISO_ON_NUM             ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_G3D_ISO_OFF_NUM             ( 0 )
#define MASK_G3D_ISO_OFF_NUM            ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_CLK_GEN6 */
#define BITS_CLK_SPI3_DIV(_x_)                              ( (_x_) << 29 & (BIT(29)|BIT(30)|BIT(31)) )
#define BITS_CLK_SPI3_SEL(_x_)                              ( (_x_) << 27 & (BIT(27)|BIT(28)) )
#define BITS_DMA_SDIO_SEL(_x_)                            ( (_x_) << 25 & (BIT(25)|BIT(26)) )
#define BIT_USB_HIGHSPEED_DISABLE_CP            ( BIT(24) )
#define BIT_USB_DLOAD_EN_CP                   ( BIT(23) )
#define BIT_EIC2_EB                                      ( BIT(22) )
#define BIT_RTC_EI2C_EB                             ( BIT(21) )
#define BIT_SPI3_EB                                      ( BIT(20) )
#define BIT_SPI2_EB                                      ( BIT(19) )
#define BITS_SPI2_DIV(_x_)                        ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)) )
#define BITS_SPI2_SEL(_x_)                        ( (_x_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_IIS2_DIV(_x_)                        ( (_x_) << 6 & (BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12))|BIT(13)) )
#define BIT_IIS2_CTRL_SEL                       ( BIT(5) )
#define BIT_IIS2_EB                                   ( BIT(4) )
#define BIT_DMA_UART_SEL                     ( BIT(3) )
#define BIT_DMA_SPI_SEL                         ( BIT(2) )
#define BITS_CLK_UART3_SEL(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_GLB_PERI_SOFT_RST2 */
#define BIT_AUDIF_SOFT_RST                                    ( BIT(5) )
#define BIT_AUDTOP_SOFT_RST                                ( BIT(4) )
#define BIT_EIC2_SOFT_RST                                      ( BIT(3) )
#define BIT_SPI3_SOFT_RST                                      ( BIT(2) )
#define BIT_SPI2_SOFT_RST                                      ( BIT(1) )
#define BIT_IIS2_SOFT_RST                                      ( BIT(0) )

/* bits definitions for register REG_GLB_AUD_CTRL */
#define BITS_AUD_AP_CP_MUX(_x_)                         ( (_x_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BIT_AUD_CLK_SEL                                          ( BIT(4) )
#define BIT_AUD_CTL_SEL                                          ( BIT(3) )
#define BIT_AUDIF_AUTO_EN                                    ( BIT(2) )
#define BITS_AUDIF_SEL(_x_)                                   ( (_x_) << 0 & (BIT(0)|BIT(1)) )

#define SHFT_AUD_AP_CP_MUX              ( 5 )
#define MASK_AUD_AP_CP_MUX             (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20))

/* bits definitions for register REG_GLB_PIN_CTRL2 */
#define BIT_CP_DTCK_PIN_IN_SEL                        ( BIT(31) )
#define BIT_CP_DTDI_PIN_IN_SEL                        ( BIT(30) )
#define BIT_CP_DTMS_PIN_IN_SEL                       ( BIT(29) )
#define BITS_CP_IIS1CLK_PIN_IN_SEL(_x_)       ( (_x_) << 27 & (BIT(27)|BIT(28)) )
#define BITS_CP_IIS1DI_PIN_IN_SEL(_x_)          ( (_x_) << 25 & (BIT(25)|BIT(26)) )
#define BITS_CP_IIS1LRCK_PIN_IN_SEL(_x_)     ( (_x_) << 23 & (BIT(23)|BIT(24)) )
#define BIT_CP_MTCK_PIN_IN_SEL                        ( BIT(22) )
#define BIT_CP_MTDI_PIN_IN_SEL                         ( BIT(21) )
#define BIT_CP_MTMS_PIN_IN_SEL                        ( BIT(20) )
#define BIT_CP_MTRST_N_PIN_IN_SEL                  ( BIT(19) )
#define BIT_IIS1CLK_PIN_IN_SEL                           ( BIT(18) )
#define BIT_IIS1DI_PIN_IN_SEL                              ( BIT(17) )
#define BIT_IIS1LRCK_PIN_IN_SEL                         ( BIT(16) )
#define BIT_U1RXD_PIN_IN_SEL                              ( BIT(15) )
#define BIT_VBC_AP_CP_MUX                                  ( BIT(14) )
#define BIT_SPI2_PIN_IN_SEL                                  ( BIT(13) )
#define BIT_CP_IIS1MCK_PIN_IN_SEL                     ( BIT(9) )
#define BIT_SPI3_PIN_IN_SEL                                  ( BIT(7) )
#define BIT_SIM0_AP_CP_MUX                                 ( BIT(3) )

/* bits definitions for register REG_GLB_CLK_GEN7 */
#define BITS_CLK_EMMCPLL_SEL(_x_)                    ( (_x_) << 23 & (BIT(23)|BIT(24)) )
#define BITS_CLK_SDIO2PLL_SEL(_x_)                   ( (_x_) << 21 & (BIT(21)|BIT(22)) )
#define BITS_FORCE_CP_DEEP_SLEEP(_x_)           ( (_x_) << 13 & (BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)) )
#define BITS_CLK_UART3_DIV(_x_)                        ( (_x_) << 10 & (BIT(10)|BIT(11)|BIT(12)) )
#define BITS_CCIR_CLK_MUX(_x_)                         ( (_x_) << 6 & (BIT(6)|BIT(7)) )
#define BIT_I2C4_EB                                                  ( BIT(3) )
#define BIT_AUD_IF_EB                                             ( BIT(2) )
#define BIT_AUD_TOP_EB                                         ( BIT(1) )
#define BIT_UART3_EB                                              ( BIT(0) )

/* bits definitions for register REG_GLB_IRAM_POW_CTRL */
#define BITS_IRAM_PWR_ON_DLY(_x_)        ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_IRAM_POW_FORCE_PD            ( BIT(23) )
#define BIT_IRAM_SLP_POWOFF_AUTO_EN      ( BIT(22) )
#define BITS_IRAM_ISO_ON_NUM(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_IRAM_ISO_OFF_NUM(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_IRAM_PWR_ON_DLY              ( 24 )
#define MASK_IRAM_PWR_ON_DLY             ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_IRAM_ISO_ON_NUM              ( 8 )
#define MASK_IRAM_ISO_ON_NUM             ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_IRAM_ISO_OFF_NUM             ( 0 )
#define MASK_IRAM_ISO_OFF_NUM            ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_MCU_SLP */
#define BITS_MCU_SLP_PD_EN(_x_)            ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)))

#define SHFT_MCU_SLP_PD_EN              ( 0 )
#define MASK_MCU_SLP_PD_EN             (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19))

/* bits definitions for register REG_GLB_DSP_SLP */
#define BITS_DSP_SLP_PD_EN(_x_)            ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)))

#define SHFT_DSP_SLP_PD_EN              ( 0 )
#define MASK_DSP_SLP_PD_EN             (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19))

/* bits definitions for register REG_GLB_MEM_SLP */
#define BITS_MEM_SLP_PD_EN(_x_)            ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)))

#define SHFT_MEM_SLP_PD_EN              ( 0 )
#define MASK_MEM_SLP_PD_EN             (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19))

/* bits definitions for register REG_GLB_JTAG_CTRL */
#define BIT_CP_ARM9_U1_MTCK_PIN_IN_SEL                   ( BIT(8) )
#define BIT_CP_ARM9_U1_MTDI_PIN_IN_SEL                    ( BIT(7) )
#define BIT_CP_ARM9_U1_MTMS_PIN_IN_SEL                   ( BIT(6) )
#define BIT_CP_ARM9_U1_MTRST_N_PIN_IN_SEL             ( BIT(5) )
#define BIT_CP_ARM9_U2_MTCK_PIN_IN_SEL                   ( BIT(4) )
#define BIT_CP_ARM9_U2_MTDI_PIN_IN_SEL                    ( BIT(3) )
#define BIT_CP_ARM9_U2_MTMS_PIN_IN_SEL                   ( BIT(2) )
#define BIT_CP_ARM9_U2_MTRST_N_PIN_IN_SEL             ( BIT(1) )
#define BIT_CP_ARM9_JTAG_CHAIN_EN                            ( BIT(0) )
#endif

/* ****************************************************************** */

/* global register types */
enum {
	REG_TYPE_GLOBAL = 0,
	REG_TYPE_AHB_GLOBAL,
	REG_TYPE_MAX
};

int32_t sprd_greg_read(uint32_t type, uint32_t reg_offset);
void sprd_greg_write(uint32_t type, uint32_t value, uint32_t reg_offset);
void sprd_greg_set_bits(uint32_t type, uint32_t bits, uint32_t reg_offset);
void sprd_greg_clear_bits(uint32_t type, uint32_t bits, uint32_t reg_offset);

#endif

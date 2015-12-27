/*
 * linux/include/asm-arm/arch-ne1/platform.h
 *
 * Copyright (C) NEC Electronics Corporation 2007, 2008
 *
 * This file is based on include/asm-arm/arch-realview/platform.h 
 *
 * Copyright (c) ARM Limited 2003.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
 */

#ifndef __ASM_ARCH_PLATFORM_H
#define __ASM_ARCH_PLATFORM_H


/* ---- NaviEngine1 Address Regions ---- */
#define NE1_BASE_PCI			UL(0x20000000)		/* PCI Window */
#define NE1_BASE_DDR2			UL(0x80000000)		/* RAM/DDR2 */
#define NE1_BASE_LMEM			UL(0xffffe000)		/* Local Memory */

/* ---- Details of NE1 Internal Region ---- */
#define NE1_BASE_INTERNAL_IO		UL(0x18000000)
#define NE1_BASE_SGX			UL(0x18000000)
#define NE1_BASE_DISP			UL(0x18010000)
#define NE1_BASE_VIDEO			UL(0x18014000)
#define NE1_BASE_DMAC64			UL(0x18015000)
#define NE1_BASE_SATA			UL(0x18016000)
#define NE1_BASE_CF_REG			UL(0x18018000)
#define NE1_BASE_NAND_REG		UL(0x18019000)
#define NE1_BASE_EXBUS_REG		UL(0x1801a000)
#define NE1_BASE_EXDMAC			UL(0x1801b000)
#define NE1_BASE_DMAC_0			UL(0x1801c000)
#define NE1_BASE_DMAC_1			UL(0x1801d000)
#define NE1_BASE_DMAC_2			UL(0x1801e000)
#define NE1_BASE_DMAC_3			UL(0x1801f000)
#define NE1_BASE_DMAC_4			UL(0x18020000)
#define NE1_BASE_DDR2_REG		UL(0x18021000)
#define NE1_BASE_PCI_REG		UL(0x18022000)
#define NE1_BASE_USBH_REG		UL(0x18023000)
#define NE1_BASE_USBH			UL(0x18024000)
#define NE1_BASE_ATA6_CS0		UL(0x18028000)
#define NE1_BASE_ATA6_CS1		UL(0x18029000)
#define NE1_BASE_SD			UL(0x18030000)
#define NE1_BASE_SPDIF			UL(0x18031000)
#define NE1_BASE_CSI_0			UL(0x18031400)
#define NE1_BASE_CSI_1			UL(0x18031800)
#define NE1_BASE_I2C			UL(0x18032000)
#define NE1_BASE_I2S_0			UL(0x18032400)
#define NE1_BASE_I2S_1			UL(0x18032800)
#define NE1_BASE_I2S_2			UL(0x18032c00)
#define NE1_BASE_I2S_3			UL(0x18033000)
#define NE1_BASE_UART_0			UL(0x18034000)
#define NE1_BASE_UART_1			UL(0x18034400)
#define NE1_BASE_UART_2			UL(0x18034800)
#define NE1_BASE_UART_3			UL(0x18034c00)
#define NE1_BASE_UART_4			UL(0x18035000)
#define NE1_BASE_UART_5			UL(0x18035400)
#define NE1_BASE_UART_6			UL(0x18035800)
#define NE1_BASE_UART_7			UL(0x18035c00)
#define NE1_BASE_TIMER_0		UL(0x18036000)
#define NE1_BASE_TIMER_1		UL(0x18036400)
#define NE1_BASE_TIMER_2		UL(0x18036800)
#define NE1_BASE_TIMER_3		UL(0x18036c00)
#define NE1_BASE_TIMER_4		UL(0x18037000)
#define NE1_BASE_TIMER_5		UL(0x18037400)
#define NE1_BASE_EWDT			UL(0x18037800)
#define NE1_BASE_SYSCTRL		UL(0x18037c00)
#define NE1_BASE_GPIO			UL(0x18038000)
#define NE1_BASE_PWM_0			UL(0x18039000)
#define NE1_BASE_PWM_1			UL(0x18039400)
#define NE1_BASE_PWM_2			UL(0x18039800)
#define NE1_BASE_PWM_3			UL(0x18039c00)
#define NE1_BASE_PWM_4			UL(0x1803a000)
#define NE1_BASE_PWM_5			UL(0x1803a400)
#define NE1_BASE_PWM_6			UL(0x1803a800)
#define NE1_BASE_PWM_7			UL(0x1803ac00)
#define NE1_BASE_DTVIF			UL(0x1804c000)
#define NE1_BASE_CFWIN_0		UL(0x18050000)
#define NE1_BASE_CFWIN_1		UL(0x18060000)
#define NE1_BASE_CFWIN_2		UL(0x18070000)

/* ---- Details of NE1 MPCore Private Region ---- */
#define NE1_BASE_MPCORE_PRIVATE		UL(0xc0000000)
#define NE1_BASE_SCU			UL(0xc0000000)
#define NE1_BASE_GIC_CPU		UL(0xc0000100)
#define NE1_BASE_PTMR			UL(0xc0000600)
#define NE1_BASE_PWDT			UL(0xc0000620)
#define NE1_BASE_GIC_DIST		UL(0xc0001000)

/* ---- Board Specific Base Addresses ---- */
#if defined(CONFIG_MACH_NE1TB) || defined(CONFIG_MACH_NE1DB)
#define NE1xB_BASE_EXBUS_CS0		UL(0x00000000)		/* ( 32MB/16) */
#define NE1xB_BASE_EXBUS_CS3		UL(0x04000000)		/* ( 32MB/16) */
#define NE1xB_BASE_EXBUS_CS2		UL(0x06000000)		/* (  4MB/32) */
#define NE1xB_BASE_EXBUS_CS1		UL(0x08000000)		/* (128MB/16) */
#define NE1xB_BASE_EXBUS_CS4		UL(0x10000000)		/* ( 64MB/16) */
#define NE1xB_BASE_EXBUS_CS5		UL(0x14000000)		/* ( 64MB/ 8) */

#define NE1xB_BASE_ROM			(NE1xB_BASE_EXBUS_CS0 + 0x00000)	/* ( 64MB/16) NOR Flash */

#define NE1xB_BASE_NIC			(NE1xB_BASE_EXBUS_CS3 + 0x00000)	/* ( 64KB/16) LAN9118 */
#endif

#if defined(CONFIG_MACH_NE1TB)
#define NE1TB_BASE_FPGA			(NE1xB_BASE_EXBUS_CS3 + 0x10000)	/* ( 64KB/16) FPGA */
#define NE1TB_BASE_RTC			(NE1TB_BASE_FPGA + 0x00000)		/* -- RTC (RV5C338A) */
#define NE1TB_BASE_ETRON		(NE1TB_BASE_FPGA + 0x00200)		/* -- eTRON */
#define NE1TB_BASE_MSTLCD01		(NE1TB_BASE_FPGA + 0x00400)		/* -- MSTLCD01 - LCD/Key/TSP */
#endif

#if defined(CONFIG_MACH_NE1DB)
#define NE1DB_BASE_ROM_EX		(NE1xB_BASE_EXBUS_CS1 + 0x00000)	/* (128MB/16) NOR Flash */

#define NE1DB_BASE_CORE_FPGA		(NE1xB_BASE_EXBUS_CS4 + 0x00000)	/* ( 64KB/ 8) CORE-FPGA */
#define NE1DB_BASE_IO_FPGA		(NE1xB_BASE_EXBUS_CS4 + 0x10000)	/* ( 64KB/ 8) IO-FPGA */
#define NE1DB_BASE_USBF			(NE1xB_BASE_EXBUS_CS4 + 0x20000)	/* ( 64KB/ 8) USBF */

#define NE1DB_BASE_ALPH_DISP		(NE1xB_BASE_EXBUS_CS5 + 0x00000)	/* ( 64KB/ 8) USBF */
#define NE1DB_BASE_RTC_EX		(NE1xB_BASE_EXBUS_CS5 + 0x10000)	/* ( 64KB/ 8) RTC (DS1511W) */
#define NE1DB_BASE_UART_EX		(NE1xB_BASE_EXBUS_CS5 + 0x20000)	/* ( 64KB/ 8) UART-EX */
#define NE1DB_BASE_I2C_EX		(NE1xB_BASE_EXBUS_CS5 + 0x30000)	/* ( 64KB/ 8) I2C-EX */
#endif


/* ---- NaviEngine1 INT ---- */
#define INT_IPI_0			0			/* inter-processor */
#define INT_IPI_1			1
#define INT_IPI_2			2
#define INT_IPI_3			3
#define INT_IPI_4			4
#define INT_IPI_5			5
#define INT_IPI_6			6
#define INT_IPI_7			7
#define INT_IPI_8			8
#define INT_IPI_9			9
#define INT_IPI_10			10
#define INT_IPI_11			11
#define INT_IPI_12			12
#define INT_IPI_13			13
#define INT_IPI_14			14
#define INT_IPI_15			15

#define INT_PTMR			29			/* MPCore private timer */
#define INT_PWDT			30			/* MPCore private watchdog */
#define INT_NIRQ			31			/* legacy nIRQ */

#define INT_EXBUS			32			/* ExBUS */
#define INT_I2C				33			/* I2C */
#define INT_CSI_0			34			/* CSI 0 */
#define INT_CSI_1			35			/* CSI 1 */
#define INT_TIMER_0			36			/* timer 0 */
#define INT_TIMER_1			37			/* timer 1 */
#define INT_TIMER_2			38			/* timer 2 */
#define INT_TIMER_3			39			/* timer 3 */
#define INT_TIMER_4			40			/* timer 4 */
#define INT_TIMER_5			41			/* timer 5 */
#define INT_PWM				42			/* PWM */
#define INT_SD_0			43			/* SD 0 */
#define INT_SD_1			44			/* SD 1 */
#define INT_CF				45			/* CF */
#define INT_NAND			46			/* NAND */
#define INT_MIF				47			/* MIF */
#define INT_DTV				48			/* DTV */
#define INT_SGX				49			/* SGX */
#define INT_DISP_CH0			50			/* DISP CH0 */
#define INT_DISP_CH1			51			/* DISP CH1 */
#define INT_DISP_CH2			52			/* DISP CH2 */
#define INT_VIDEO			53			/* VIDEO */
#define INT_SPDIF_0			54			/* SPDIF 0 */
#define INT_SPDIF_1			55			/* SPDIF 1 */
#define INT_I2S_0			56			/* I2S 0 */
#define INT_I2S_1			57			/* I2S 1 */
#define INT_I2S_2			58			/* I2S 2 */
#define INT_I2S_3			59			/* I2S 3 */
#define INT_APB				60			/* APB */
#define INT_AHB_BRIDGE_0		61			/* AHB bridge 0 */
#define INT_AHB_BRIDGE_1		62			/* AHB bridge 1 */
#define INT_AHB_BRIDGE_2		63			/* AHB bridge 2 */

#define INT_AXI				64			/* AXI */
#define INT_PCI_INT			65			/* PCI INT */
#define INT_PCI_SERRB			66			/* PCI SERRB */
#define INT_PCI_PERRB			67			/* PCI PERRB */
#define INT_EXPCI_INT			68			/* ExPCI INT */
#define INT_EXPCI_SERRB			69			/* ExPCI SERRB */
#define INT_EXPCI_PERRB			70			/* ExPCI PERRB */
#define INT_USBH_INTA			71			/* USBH INTA */
#define INT_USBH_INTB			72			/* USBH INTB */
#define INT_USBH_SMI			73			/* USBH SMI */
#define INT_USBH_PME			74			/* USBH PME */
#define INT_ATA6			75			/* ATA6 [L] */
#define INT_DMAC32_END_0		76			/* DMAC32 0 END */
#define INT_DMAC32_ERR_0		77			/* DMAC32 0 ERR */
#define INT_DMAC32_END_1		78			/* DMAC32 1 END */
#define INT_DMAC32_ERR_1		79			/* DMAC32 1 ERR */
#define INT_DMAC32_END_2		80			/* DMAC32 2 END */
#define INT_DMAC32_ERR_2		81			/* DMAC32 2 ERR */
#define INT_DMAC32_END_3		82			/* DMAC32 3 END */
#define INT_DMAC32_ERR_3		83			/* DMAC32 3 ERR */
#define INT_DMAC32_END_4		84			/* DMAC32 4 END */
#define INT_DMAC32_ERR_4		85			/* DMAC32 4 ERR */
#define INT_UART_0			86			/* UART 0 */
#define INT_UART_1			87			/* UART 1 */
#define INT_UART_2			88			/* UART 2 */
#define INT_UART_3			89			/* UART 3 */
#define INT_UART_4			90			/* UART 4 */
#define INT_UART_5			91			/* UART 5 */
#define INT_UART_6			92			/* UART 6 */
#define INT_UART_7			93			/* UART 7 */
#define INT_GPIO			94			/* GPIO */
#define INT_EWDT			95			/* eWDT */

#define INT_SATA			96			/* SATA */
#define INT_DMAC_AXI_END		97			/* DMAC AXI END */
#define INT_DMAC_AXI_ERR		98			/* DMAC AXI ERR */

#define INT_PMUIRQ_0			100			/* PMUIRQ[0] */
#define INT_PMUIRQ_1			101			/* PMUIRQ[1] */
#define INT_PMUIRQ_2			102			/* PMUIRQ[2] */
#define INT_PMUIRQ_3			103			/* PMUIRQ[3] */
#define INT_PMUIRQ_4			104			/* PMUIRQ[4] */
#define INT_PMUIRQ_5			105			/* PMUIRQ[5] */
#define INT_PMUIRQ_6			106			/* PMUIRQ[6] */
#define INT_PMUIRQ_7			107			/* PMUIRQ[7] */
#define INT_PMUIRQ_8			108			/* PMUIRQ[8] */
#define INT_PMUIRQ_9			109			/* PMUIRQ[9] */
#define INT_PMUIRQ_10			110			/* PMUIRQ[10] */
#define INT_PMUIRQ_11			111			/* PMUIRQ[11] */
#define INT_COMMRX_0			112			/* COMRX[0] */
#define INT_COMMRX_1			113			/* COMRX[1] */
#define INT_COMMRX_2			114			/* COMRX[2] */
#define INT_COMMRX_3			115			/* COMRX[3] */
#define INT_COMMTX_0			116			/* COMTX[0] */
#define INT_COMMTX_1			117			/* COMTX[1] */
#define INT_COMMTX_2			118			/* COMTX[2] */
#define INT_COMMTX_3			119			/* COMTX[3] */
#define INT_PWRCTLO_0			120			/* PWRCTLO0 */
#define INT_PWRCTLO_1			121			/* PWRCTLO1 */
#define INT_PWRCTLO_2			122			/* PWRCTLO2 */
#define INT_PWRCTLO_3			123			/* PWRCTLO3 */
#define INT_DMAC_EXBUS_END		124			/* DMAC AXI END */
#define INT_DMAC_EXBUS_ERR		125			/* DMAC AXI ERR */
#define INT_AHB_BRIDGE_3		126			/* AHB bridge 3 */
#define INT_TEST			127			/* test */

#define INT_NE1_MAXIMUM			128
#define INT_SPURIOUS			1023

#define INT_GPIO_BASE		INT_NE1_MAXIMUM
#define INT_GPIO_LAST		(INT_GPIO_BASE + 31)
#define INT_MAXIMUM			(INT_GPIO_LAST + 1)

/* ---- Board-level INT ---- */
#if defined(CONFIG_MACH_NE1TB) || defined(CONFIG_MACH_NE1DB)
#define INT_UART_1_DCD		(INT_GPIO_BASE + 10)
#define INT_PCI_INTA		(INT_GPIO_BASE + 16)
#define INT_PCI_INTB		(INT_GPIO_BASE + 17)
#define INT_PCI_INTC		(INT_GPIO_BASE + 18)
#define INT_PCI_INTD		(INT_GPIO_BASE + 19)
#define INT_NIC				(INT_GPIO_BASE + 20)
#define INT_ETRON			(INT_GPIO_BASE + 21)
#define INT_KEY				(INT_GPIO_BASE + 22)
#define INT_RTC				(INT_GPIO_BASE + 23)
#define INT_LINT			(INT_GPIO_BASE + 24)
#define INT_SW0				(INT_GPIO_BASE + 27)
#define INT_SW1				(INT_GPIO_BASE + 28)
#endif

/* for arch/arm/mach */
#define	NE1_SDRAM_BASE			NE1_BASE_DDR2
#define NE1_IPCI_BASE			NE1_BASE_USBH_REG
#define NE1_OHCI_BASE			NE1_BASE_USBH
#define NE1_EHCI_BASE			(NE1_BASE_USBH +0x1000)

#define NE1_TWD_BASE			(NE1_BASE_PTMR + 0x100)
#define NE1_TWD_SIZE			0x100
#define NE1_GIC_DIST_BASE		NE1_BASE_GIC_DIST
#define NE1_GIC_CPU_BASE		NE1_BASE_GIC_CPU

#define	NE1TB_FLASH_BASE		NE1xB_BASE_ROM
#define	NE1TB_FLASH_SIZE		0x04000000		/* 64 MB */

#define	NE1TB_NAND_SIZE			0x10000000		/* 256 MB */

#define	NE1xB_ETH_BASE			NE1xB_BASE_NIC

#define	NE1_PCI_NPMEM_BASE		NE1_BASE_PCI
#define	NE1_PCI_NPMEM_SIZE		0x04000000
#define	NE1_PCI_MEM_BASE		NE1_BASE_PCI
#define	NE1_PCI_MEM_SIZE		0
#define	NE1_PCI_IO_BASE			(NE1_PCI_NPMEM_BASE + NE1_PCI_NPMEM_SIZE)
#define	NE1_PCI_IO_SIZE			0x04000000


#endif /* __ASM_ARCH_PLATFORM_H */

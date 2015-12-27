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
#ifndef __SPI_SC88XX_H__
#define __SPI_SC88XX_H__

#include <linux/semaphore.h>
#include <mach/dma.h>

#define SPI_TXD                     0x0000
#define SPI_CLKD                    0x0004
#define SPI_CTL0                    0x0008
#define SPI_CTL1                    0x000c
#define SPI_CTL2                    0x0010
#define SPI_CTL3                    0x0014
#define SPI_CTL4                    0x0018
#define SPI_CTL5                    0x001c
#define SPI_INT_EN                  0x0020
#define SPI_INT_CLR                 0x0024
#define SPI_INT_RAW_STS             0x0028
#define SPI_INT_MASK_STS            0x002c
#define SPI_STS1                    0x0030
#define SPI_STS2                    0x0034
#define SPI_DSP_WAIT                0x0038
#define SPI_STS3                    0x003c
#define SPI_CTL6                    0x0040
#define SPI_STS4                    0x0044
#define SPI_FIFO_RST                0x0048

/* Bit define for register STS2 */
#define SPI_RX_FIFO_FULL            BIT(0)
#define SPI_RX_FIFO_EMPTY           BIT(1)
#define SPI_TX_FIFO_FULL            BIT(2)
#define SPI_TX_FIFO_EMPTY           BIT(3)
#define SPI_RX_FIFO_REALLY_FULL     BIT(4)
#define SPI_RX_FIFO_REALLY_EMPTY    BIT(5)
#define SPI_TX_FIFO_REALLY_FULL     BIT(6)
#define SPI_TX_FIFO_REALLY_EMPTY    BIT(7)
#define SPI_TX_BUSY                 BIT(8)
/* Bit define for register ctr1 */
#define SPI_RX_MODE                 BIT(12)
#define SPI_TX_MODE                 BIT(13)
/* Bit define for register ctr2 */
#define SPI_DMA_EN                  BIT(6)
/* Bit define for register ctr4 */
#define SPI_START_RX                BIT(9)

#define spi_writel(value, reg) \
    __raw_writel(value, (volatile unsigned char __force *)sprd_data->regs + reg)

#define spi_readl(reg) \
    __raw_readl((volatile unsigned char __force *)sprd_data->regs + reg)

#define spi_write_reg(reg, shift, val, mask) \
{ \
    unsigned long flags; \
    u32 tmp; \
    volatile void *regs = (volatile unsigned char __force *)sprd_data->regs + reg; \
    raw_local_irq_save(flags); \
    tmp = __raw_readl(regs); \
    tmp &= ~(mask<<shift); \
    tmp |= val << shift; \
    __raw_writel(tmp, regs); \
    raw_local_irq_restore(flags); \
}

#define spi_start() \
    do { \
        spi_write_reg(SPI_CTL1, 12, 0x03, 0x03); /* Enable SPI transmit and receive both mode */\
        sprd_dma_start2(DMA_SPI_TX, DMA_SPI_RX); \
    } while (0)

#define spi_start_tx() \
    do { \
        spi_write_reg(SPI_CTL1, 12, 0x02, 0x03); /* Only Enable SPI transmit mode */\
        sprd_dma_start(DMA_SPI_TX); \
    } while (0)

#define spi_start_rx(blocks) \
    do { \
        spi_write_reg(SPI_CTL1, 12, 0x01, 0x03); /* Only Enable SPI receive mode */\
        sprd_dma_start(DMA_SPI_RX); \
        /* spi_write_reg(SPI_CTL4,  9, 0x01, 0x01); */ \
        spi_writel((1 << 9) | blocks, SPI_CTL4); \
    } while (0)

#define spi_dma_start() \
    do { \
        spi_write_reg(SPI_CTL2,  6, 0x01, 0x01); \
    } while (0)

#define spi_dma_stop() \
    do { \
        spi_write_reg(SPI_CTL2,  6, 0x00, 0x01); \
    } while (0)

#define SPRD_SPI_DMA_MODE 1
#define SPRD_SPI_ONLY_RX_AND_TXRX_BUG_FIX 1
#define SPRD_SPI_ONLY_RX_AND_TXRX_BUG_FIX_IGNORE_ADDR ((void*)1)
#define SPRD_SPI_RX_WATERMARK_BUG_FIX 1
#define SPRD_SPI_RX_WATERMARK_MAX  0x1f

#define SPRD_SPI_BURST_SIZE_DEFAULT (1024*8)
#define sprd_spi_update_burst_size() \
    dma_desc->cfg &= ~DMA_CFG_BLOCK_LEN_MAX; \
    dma_desc->cfg |= len > SPRD_SPI_BURST_SIZE_DEFAULT ? SPRD_SPI_BURST_SIZE_DEFAULT:len; \

struct sprd_spi_data {
#define SPRD_SPI_DMA_BLOCK_MAX  (0x1ff)	/* ctrl4-[0:8] */
#define SPRD_SPI_BUFFER_SIZE (SPRD_SPI_BURST_SIZE_DEFAULT<(SPRD_SPI_DMA_BLOCK_MAX+1)*4 ? (SPRD_SPI_DMA_BLOCK_MAX+1)*4:SPRD_SPI_BURST_SIZE_DEFAULT)
	spinlock_t lock;
	struct list_head queue;
	struct spi_message *cspi_msg;
	struct spi_transfer *cspi_trans;
	int cspi_trans_num;
	int cspi_trans_len;
	struct spi_device *cspi;
	void *buffer;
	void *tx_buffer;
	void *rx_buffer;
	int rt_max;
	u8 *rx_ptr;
	dma_addr_t buffer_dma;
	dma_addr_t tx_buffer_dma;
	dma_addr_t rx_buffer_dma;

	int irq;
	struct platform_device *pdev;
	void __iomem *regs;
	int tx_rx_finish;

	u8 stopping;
	u8 cs_null;

	struct task_struct *spi_kthread;
	struct semaphore process_sem;
	struct semaphore process_sem_direct;
	int dma_started;
};

struct sprd_spi_controller_data {
	u32 cs_gpio;
	u32 clk_spi_and_div;
	u32 spi_clkd;
	u32 spi_ctl0;
	u32 data_width;		/* 1bit,2bit,...,8bits,...,16bits,...,32bits */
	u32 data_width_order;
	u32 data_max;
};

#endif


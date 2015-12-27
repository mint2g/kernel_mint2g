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
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/kthread.h>

#include <asm/io.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/hardware.h>
#include "spi_sc8810.h"

/* NOTE: only GPIO CS is supported yet*/
#define SPRD_SPI_CS_GPIO 1

void dump_buffer(unsigned char *buffer, int length)
{

	int i = 0;

	for (i = 0; i < length && i < 32 * 32; i++) {
		printk("%02X ", buffer[i]);
		if (i % 8 == 7)
			printk(" ");
		if (i % 32 == 31)
			printk("\n");
	}
	printk("\n\n");
}

static inline void cs_activate(struct sprd_spi_data *sprd_data,
			       struct spi_device *spi)
{
	if (sprd_data->cs_null) {
		struct sprd_spi_controller_data *sprd_ctrl_data =
		    spi->controller_data;

		spi_writel(sprd_ctrl_data->spi_clkd, SPI_CLKD);
		spi_writel(sprd_ctrl_data->spi_ctl0, SPI_CTL0);

		sprd_greg_set_bits(REG_TYPE_GLOBAL,
				   (sprd_ctrl_data->clk_spi_and_div & 0xffff) <<
				   21, GR_GEN2);

		sprd_greg_set_bits(REG_TYPE_GLOBAL,
				   (sprd_ctrl_data->clk_spi_and_div >> 16) <<
				   26, GR_CLK_DLY);

#if SPRD_SPI_CS_GPIO

		__gpio_set_value(sprd_ctrl_data->cs_gpio,
				 spi->mode & SPI_CS_HIGH);
#endif
		sprd_data->cspi = spi;

		sprd_data->cs_null = 0;
	}
}

static inline void cs_deactivate(struct sprd_spi_data *sprd_data,
				 struct spi_device *spi)
{
#if SPRD_SPI_CS_GPIO

	if (spi) {
		struct sprd_spi_controller_data *sprd_ctrl_data =
		    spi->controller_data;
		__gpio_set_value(sprd_ctrl_data->cs_gpio,
				 !(spi->mode & SPI_CS_HIGH));
	}
#else
	spi_bits_or(0x0F << 8, SPI_CTL0);
#endif
	sprd_data->cs_null = 1;
}


static int sprd_spi_direct_transfer_full_duplex(void *data_in,
						const void *data_out, int len,
						void *cookie, void *cookie2)
{
	int i, j, timeout, block, tlen;
#define MYLOCAL_TIMEOUT 0xff0000

#define FIFO_SIZE 1

	struct sprd_spi_data *sprd_data = cookie;

	/* Setup Full Duplex */
	spi_write_reg(SPI_CTL1, 12, 0x03, 0x03);

	j = 0;

	/* Repeat until done */
	for (i = 0, tlen = len; tlen; i++) {
		if (tlen > FIFO_SIZE) {
			block = FIFO_SIZE;
			tlen -= FIFO_SIZE;
		} else {
			block = tlen;
			tlen = 0;
		}

		/* Wait until tx fifo empty */
		for (timeout = 0;
		     !(spi_readl(SPI_STS2) & SPI_TX_FIFO_REALLY_EMPTY)
		     && timeout++ < MYLOCAL_TIMEOUT;) ;

		/* Write byte to tx fifo buffer */
		for (j = 0; j < block; ++j)
			spi_writel(((u8 *) data_out)[i * FIFO_SIZE + j],
				   SPI_TXD);

		/* Wait until tx fifo empty */
		for (timeout = 0;
		     !(spi_readl(SPI_STS2) & SPI_TX_FIFO_REALLY_EMPTY)
		     && timeout++ < MYLOCAL_TIMEOUT;) ;

		/* Wait until rx fifo not empty */
		for (timeout = 0;
		     (spi_readl(SPI_STS2) & SPI_RX_FIFO_REALLY_EMPTY)
		     && timeout++ < MYLOCAL_TIMEOUT;) ;

		/* Read as many bytes as were written */
		for (j = 0; j < block; ++j)
			((u8 *) data_in)[i * FIFO_SIZE + j] =
			    spi_readl(SPI_TXD);
	}

	return 0;
}

static int sprd_spi_direct_transfer(void *data_in, const void *data_out,
				    int len, void *cookie, void *cookie2)
{
	int i, timeout;
	u8 *data;

#define MYLOCAL_TIMEOUT 0xff0000

	struct sprd_spi_data *sprd_data = cookie;
	struct sprd_spi_controller_data *sprd_ctrl_data = cookie2;

	if ((data_in == NULL && data_out == NULL) || len == 0) {
		WARN(1,
		     "SPI transfer half duplex, rx %#010x, tx %#010x, len %d",
		     (int)data_in, (int)data_out, len);

		return -EINVAL;
	}

	if (data_in) {
		int block, tlen, j, block_bytes;

		spi_write_reg(SPI_CTL1, 12, 0x01, 0x03);	/* Only Enable SPI receive mode */

		if (likely(sprd_ctrl_data->data_width != 3)) {
			block_bytes =
			    SPRD_SPI_DMA_BLOCK_MAX <<
			    sprd_ctrl_data->data_width_order;
		} else
			block_bytes = SPRD_SPI_DMA_BLOCK_MAX * 3;

		for (i = 0, tlen = len; tlen;) {
			if (tlen > block_bytes) {
				block = SPRD_SPI_DMA_BLOCK_MAX;
				tlen -= block_bytes;
			} else {
				if (likely(sprd_ctrl_data->data_width != 3))
					block =
					    tlen >>
					    sprd_ctrl_data->data_width_order;
				else
					block =
					    tlen / sprd_ctrl_data->data_width;
				tlen = 0;
			}

			spi_writel(0x0000, SPI_CTL4);	/* stop only rx */
			spi_writel((1<<15) | (1 << 9) | block, SPI_CTL4);
			for (j = 0; j < block; j++) {
				for (timeout = 0;
				     (spi_readl(SPI_STS2) &
				      SPI_RX_FIFO_REALLY_EMPTY)
				     && timeout++ < MYLOCAL_TIMEOUT;) ;
				if (timeout >= MYLOCAL_TIMEOUT) {
					printk
					    ("Timeout spi_readl(SPI_STS2) & SPI_RX_FIFO_REALLY_EMPTY)\n");
					return -ENOPROTOOPT;
				}
				data = (u8 *) data_in + i;
				switch (sprd_ctrl_data->data_width) {
				case 1:
					((u8 *) data)[0] = spi_readl(SPI_TXD);
					i += 1;
					break;
				case 2:
					((u16 *) data)[0] = spi_readl(SPI_TXD);
					i += 2;
					break;
				case 4:
					((u32 *) data)[0] = spi_readl(SPI_TXD);
					i += 4;
					break;
				}
			}
		}
	}

	if (data_out) {
		spi_write_reg(SPI_CTL1, 12, 0x02, 0x03);	/* Only Enable SPI transmit mode */
		for (i = 0; i < len;) {
			for (timeout = 0;
			     (spi_readl(SPI_STS2) & SPI_TX_FIFO_FULL)
			     && timeout++ < MYLOCAL_TIMEOUT;) ;
			if (timeout >= MYLOCAL_TIMEOUT) {
				printk
				    ("Timeout spi_readl(SPI_STS2) & SPI_TX_FIFO_FULL)\n");
				return -ENOPROTOOPT;
			}
			data = (u8 *) data_out + i;
			switch (sprd_ctrl_data->data_width) {
			case 1:
				spi_writel(((u8 *) data)[0], SPI_TXD);
				i += 1;
				break;
			case 2:
				spi_writel(((u16 *) data)[0], SPI_TXD);
				i += 2;
				break;
			case 4:
				spi_writel(((u32 *) data)[0], SPI_TXD);
				i += 4;
				break;
			}
		}
		for (timeout = 0;
		     !(spi_readl(SPI_STS2) & SPI_TX_FIFO_REALLY_EMPTY)
		     && timeout++ < MYLOCAL_TIMEOUT;) ;
		if (timeout >= MYLOCAL_TIMEOUT) {
			printk
			    ("Timeout spi_readl(SPI_STS2) & SPI_TX_FIFO_REALLY_EMPTY)\n");
			return -ENOPROTOOPT;
		}
		for (timeout = 0; (spi_readl(SPI_STS2) & SPI_TX_BUSY)
		     && timeout++ < MYLOCAL_TIMEOUT;) ;
		if (timeout >= MYLOCAL_TIMEOUT) {
			printk("Timeout spi_readl(SPI_STS2) & SPI_TX_BUSY)\n");
			return -ENOPROTOOPT;
		}
	}

	return 0;
}

static int sprd_spi_direct_transfer_compact(struct spi_device *spi,
					    struct spi_message *msg)
{
	struct sprd_spi_controller_data *sprd_ctrl_data = spi->controller_data;
	struct sprd_spi_data *sprd_data = spi_master_get_devdata(spi->master);
	struct spi_transfer *cspi_trans;
	unsigned int cs_change = 1;

	down(&sprd_data->process_sem_direct);

	cspi_trans =
	    list_entry(msg->transfers.next, struct spi_transfer, transfer_list);

	cs_activate(sprd_data, spi);

	do {
		if (cs_change) {
			cs_activate(sprd_data, spi);
			cs_change = cspi_trans->cs_change;
		}

		if (cspi_trans->rx_buf != NULL && cspi_trans->tx_buf != NULL) {
			/* do full deplex transfer */
			msg->status =
			    sprd_spi_direct_transfer_full_duplex(cspi_trans->
								 rx_buf,
								 cspi_trans->
								 tx_buf,
								 cspi_trans->
								 len, sprd_data,
								 sprd_ctrl_data);
		} else {
			/* half deplex transfer */
			msg->status =
			    sprd_spi_direct_transfer(cspi_trans->rx_buf,
						     cspi_trans->tx_buf,
						     cspi_trans->len, sprd_data,
						     sprd_ctrl_data);
		}

		if (cs_change) {
			cs_deactivate(sprd_data, spi);
		}

		if (msg->status < 0)
			break;
		msg->actual_length += cspi_trans->len;
		if (msg->transfers.prev == &cspi_trans->transfer_list)
			break;
		cspi_trans =
		    list_entry(cspi_trans->transfer_list.next,
			       struct spi_transfer, transfer_list);
	} while (1);
	cs_deactivate(sprd_data, spi);

	up(&sprd_data->process_sem_direct);

	msg->complete(msg->context);

	return 0;
}

static int sprd_spi_transfer(struct spi_device *spi, struct spi_message *msg)
{
	return sprd_spi_direct_transfer_compact(spi, msg);
}


static irqreturn_t sprd_spi_interrupt(int irq, void *dev_id)
{
	struct spi_master *master = dev_id;
	struct sprd_spi_data *sprd_data = spi_master_get_devdata(master);

	return IRQ_HANDLED;
}


static int sprd_spi_setup(struct spi_device *spi)
{
	struct sprd_spi_data *sprd_data;
	struct sprd_spi_controller_data *sprd_ctrl_data = spi->controller_data;
	u8 bits = spi->bits_per_word;
	u32 clk_spi;
	u8 clk_spi_mode;
	u32 cs_gpio;
	u8 clk_spi_div;
	u32 spi_clkd;
	u32 spi_ctl0 = 0;
	int data_width = 0;
	int ret;

	sprd_data = spi_master_get_devdata(spi->master);

	if (sprd_data->stopping)
		return -ESHUTDOWN;
	if (spi->chip_select > spi->master->num_chipselect)
		return -EINVAL;
	if (bits > 32)
		return -EINVAL;
	if (bits == 32)
		bits = 0;

	switch (spi->master->bus_num) {
	case 0:
		clk_spi_mode =
		    (sprd_greg_read(REG_TYPE_GLOBAL, GR_CLK_DLY) >> 26) & 0x03;
		break;
	case 1:
		clk_spi_mode =
		    (sprd_greg_read(REG_TYPE_GLOBAL, GR_CLK_DLY) >> 30) & 0x03;
		break;
	}

	switch (clk_spi_mode) {
	case 0:
		clk_spi = 192 * 1000 * 1000;
		break;
	case 1:
		clk_spi = 153.6 * 1000 * 1000;
		break;
	case 2:
		clk_spi = 96 * 1000 * 1000;
		break;
	case 3:
		clk_spi = 26 * 1000 * 1000;
		break;
	}
	switch (spi->master->bus_num) {
	case 0:
		clk_spi_div =
		    (sprd_greg_read(REG_TYPE_GLOBAL, GR_GEN2) >> 21) & 0x07;
		break;
	case 1:
		clk_spi_div =
		    (sprd_greg_read(REG_TYPE_GLOBAL, GR_GEN2) >> 11) & 0x07;
		break;
	}

	clk_spi /= (clk_spi_div + 1);

	if (spi->max_speed_hz) {
		spi_clkd = clk_spi / (2 * spi->max_speed_hz) - 1;
	} else {
		spi_clkd = 0xffff;
	}
	if (spi_clkd < 0) {
		printk(KERN_WARNING "Warning: %s your spclk %d is so big!\n",
		       __func__, spi->max_speed_hz);
		spi_clkd = 0;
	}

	/* chipselect must have been muxed as GPIO (e.g. in board setup)
	 * and we assume cs_gpio real gpio number not exceeding 0xffff
	 */

	cs_gpio = (u32) spi->controller_data;
	if (cs_gpio < 0xffff) {
		sprd_ctrl_data = kzalloc(sizeof *sprd_ctrl_data, GFP_KERNEL);
		if (!sprd_ctrl_data)
			return -ENOMEM;

		ret = gpio_request(cs_gpio, dev_name(&spi->dev));
		if (ret) {
			printk(KERN_WARNING "%s[%s] cs_gpio %d is busy!",
			       spi->modalias, dev_name(&spi->dev), cs_gpio);
		}
		sprd_ctrl_data->cs_gpio = cs_gpio;
		spi->controller_data = sprd_ctrl_data;
		gpio_direction_output(cs_gpio, !(spi->mode & SPI_CS_HIGH));

	} else {

		unsigned long flags;
		spin_lock_irqsave(&sprd_data->lock, flags);
		if (sprd_data->cspi == spi)
			sprd_data->cspi = NULL;
		cs_deactivate(sprd_data, spi);
		spin_unlock_irqrestore(&sprd_data->lock, flags);
	}
	sprd_ctrl_data->clk_spi_and_div = clk_spi_div | (clk_spi_mode << 16);
	sprd_ctrl_data->spi_clkd = spi_clkd;

	if (spi->mode & SPI_CPHA)
		spi_ctl0 |= 0x01;
	else
		spi_ctl0 |= 0x02;

	if (spi->mode & SPI_CPOL)
		spi_ctl0 |= 1 << 13;
	else
		spi_ctl0 |= 0 << 13;

	spi_ctl0 |= bits << 2;
#if SPRD_SPI_CS_GPIO
	spi_ctl0 |= 0x0F << 8;
#else
	switch (spi->chip_select) {
	case 2:
	case 0:
		spi_ctl0 |= 0x0E << 8;
		break;
	case 3:
	case 1:
		spi_ctl0 |= 0x0D << 8;
		break;
	default:
		spi_ctl0 |= 0x0F << 8;
		break;
	}
#endif

	sprd_ctrl_data->spi_ctl0 = spi_ctl0;

	data_width = (sprd_ctrl_data->spi_ctl0 >> 2) & 0x1f;
	if (data_width == 0)
		data_width = 32;

	sprd_ctrl_data->data_width = (data_width + 7) / 8;
	sprd_ctrl_data->data_width_order = sprd_ctrl_data->data_width >> 1;

	return 0;
}

static void sprd_spi_cleanup(struct spi_device *spi)
{
	struct sprd_spi_data *sprd_data = spi_master_get_devdata(spi->master);
	struct sprd_spi_controller_data *sprd_ctrl_data = spi->controller_data;
	u32 cs_gpio = (u32) sprd_ctrl_data;
	unsigned long flags;

	if (cs_gpio < 0xffff)
		return;
	cs_gpio = sprd_ctrl_data->cs_gpio;

	spin_lock_irqsave(&sprd_data->lock, flags);
	if (sprd_data->cspi == spi) {
		sprd_data->cspi = NULL;
		cs_deactivate(sprd_data, spi);
	}
	spi->controller_data = (void *)cs_gpio;
	spin_unlock_irqrestore(&sprd_data->lock, flags);

	gpio_free(cs_gpio);
	kfree(sprd_ctrl_data);
}

static int __init sprd_spi_probe(struct platform_device *pdev)
{
	struct resource *regs;
	int irq, ret;
	struct spi_master *master;
	struct sprd_spi_data *sprd_data;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs)
		return -ENXIO;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = -ENOMEM;
	master = spi_alloc_master(&pdev->dev, sizeof *sprd_data);
	if (!master)
		goto out_free;

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_3WIRE;

	master->bus_num = pdev->id;
	master->num_chipselect = 2;
	master->setup = sprd_spi_setup;
	master->transfer = sprd_spi_transfer;
	master->cleanup = sprd_spi_cleanup;
	platform_set_drvdata(pdev, master);

	sprd_data = spi_master_get_devdata(master);

	spin_lock_init(&sprd_data->lock);
	INIT_LIST_HEAD(&sprd_data->queue);
	sprd_data->pdev = pdev;

	sprd_data->regs = (void *)regs->start;

	if (!sprd_data->regs)
		goto out_free_buffer;

	/* Initialize the hardware */
	switch (pdev->id) {
	case 0:
		sprd_greg_set_bits(REG_TYPE_GLOBAL, GEN0_SPI_EN, GR_GEN0);
		sprd_greg_set_bits(REG_TYPE_GLOBAL, SWRST_SPI0_RST,
				   GR_SOFT_RST);
		msleep(1);
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, SWRST_SPI0_RST,
				     GR_SOFT_RST);

		spi_writel(0, SPI_INT_EN);
		/* clk source selected to 96M */
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, (0x03 << 26), GR_CLK_DLY);
		sprd_greg_set_bits(REG_TYPE_GLOBAL, 2 << 26, GR_CLK_DLY);
		/*
		 * clk_spi_div sets to 1, so clk_spi=96M/2=48M,
		 * and clk_spi is SPI_CTL5 interval base clock.[luther.ge]
		 */
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, (0x07 << 21), GR_GEN2);
		break;
	case 1:
		sprd_greg_set_bits(REG_TYPE_GLOBAL, GEN0_SPI1_EN, GR_GEN0);
		sprd_greg_set_bits(REG_TYPE_GLOBAL, SWRST_SPI1_RST,
				   GR_SOFT_RST);
		msleep(1);
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, SWRST_SPI1_RST,
				     GR_SOFT_RST);
		spi_writel(0, SPI_INT_EN);
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, (0x03 << 30), GR_CLK_DLY);
		sprd_greg_set_bits(REG_TYPE_GLOBAL, 2 << 30, GR_CLK_DLY);
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, (0x07 << 11), GR_GEN2);
		break;
	}

	sprd_data->cs_null = 1;

	ret = spi_register_master(master);
	if (ret)
		goto out_reset_hw;

	sema_init(&sprd_data->process_sem_direct, 1);

	return 0;

out_reset_hw:
	free_irq(irq, master);
out_unmap_regs:
	iounmap(sprd_data->regs);
out_free_buffer:
out_free:
	spi_master_put(master);
	return ret;
}

static int __exit sprd_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct sprd_spi_data *sprd_data = spi_master_get_devdata(master);
	struct spi_message *msg;
	/* reset the hardware and block queue progress */
	spin_lock_irq(&sprd_data->lock);
	sprd_data->stopping = 1;
	spin_unlock_irq(&sprd_data->lock);

	/* Terminate remaining queued transfers */
	list_for_each_entry(msg, &sprd_data->queue, queue) {
		msg->status = -ESHUTDOWN;
		msg->complete(msg->context);
	}

	free_irq(sprd_data->irq, master);

	iounmap(sprd_data->regs);

	spi_unregister_master(master);

	sprd_greg_set_bits(REG_TYPE_GLOBAL, GEN0_SPI_EN, GR_GEN0);

	return 0;
}

#ifdef CONFIG_PM
static int sprd_spi_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int sprd_spi_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define	sprd_spi_suspend NULL
#define	sprd_spi_resume NULL
#endif

static struct platform_driver sprd_spi_driver = {
	.driver = {
		   .name = "sprd_spi",
		   .owner = THIS_MODULE,
		   },
	.suspend = sprd_spi_suspend,
	.resume = sprd_spi_resume,
	.remove = __exit_p(sprd_spi_remove),
};

static int __init sprd_spi_init(void)
{
	return platform_driver_probe(&sprd_spi_driver, sprd_spi_probe);
}

module_init(sprd_spi_init);

static void __exit sprd_spi_exit(void)
{
	platform_driver_unregister(&sprd_spi_driver);
}

module_exit(sprd_spi_exit);

MODULE_DESCRIPTION("SpreadTrum SC88XX Series SPI Controller driver");
MODULE_AUTHOR("Luther Ge <luther.ge@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sprd_spi");

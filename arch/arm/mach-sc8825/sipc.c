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
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/debugfs.h>

#include <mach/hardware.h>

#include <linux/sipc.h>
#include <linux/sipc_priv.h>

#define CPT_RING_ADDR		(0x80000000 + 28 * SZ_1M)

#define CPT_TXBUF_ADDR		(0)
#define CPT_TXBUF_SIZE		(SZ_1K)
#define CPT_RXBUF_ADDR		(CPT_TXBUF_SIZE)
#define CPT_RXBUF_SIZE		(SZ_1K)

#define CPT_RINGHDR		(CPT_TXBUF_SIZE + CPT_RXBUF_SIZE)
#define CPT_TXBUF_RDPTR		(CPT_RINGHDR + 0)
#define CPT_TXBUF_WRPTR		(CPT_RINGHDR + 4)
#define CPT_RXBUF_RDPTR		(CPT_RINGHDR + 8)
#define CPT_RXBUF_WRPTR		(CPT_RINGHDR + 12)

#define SMEM_CPT_ADDR		(0x80000000 + 24 * SZ_1M)
#define SMEM_CPT_SIZE		(4 * SZ_1M)

#define AP2CP_INT_CTRL		(SPRD_IPI_BASE + 0x00B8)
#define CP2AP_INT_CTRL		(SPRD_IPI_BASE + 0x00BC)

#define AP2CP_IRQ0_TRIG		0x01
#define AP2CP_FRQ0_TRIG		0x02
#define AP2CP_IRQ1_TRIG		0x04
#define AP2CP_FRQ1_TRIG		0x08

#define CP2AP_IRQ0_CLR		0x01
#define CP2AP_FRQ0_CLR		0x02
#define CP2AP_IRQ1_CLR		0x04
#define CP2AP_FRQ1_CLR		0x08

uint32_t cpt_rxirq_status(void)
{
	return 1;
}

void cpt_rxirq_clear(void)
{
	__raw_writel(CP2AP_IRQ0_CLR, CP2AP_INT_CTRL);
}

void cpt_txirq_trigger(void)
{
	__raw_writel(AP2CP_IRQ0_TRIG, AP2CP_INT_CTRL);
}

static struct smsg_ipc smsg_ipc_cpt = {
	.name = "sipc-td",
	.dst = SIPC_ID_CPT,

	.irq = IRQ_CP2AP_INT0,
	.rxirq_status = cpt_rxirq_status,
	.rxirq_clear = cpt_rxirq_clear,
	.txirq_trigger = cpt_txirq_trigger,
};

#ifdef CONFIG_DEBUG_FS
static u32 *txirq_trigger = (u32 *)AP2CP_INT_CTRL;
#endif

static int __init sipc_init(void)
{
	uint32_t base = (uint32_t)ioremap(CPT_RING_ADDR, SZ_4K);

	smsg_ipc_cpt.txbuf_size = CPT_TXBUF_SIZE / sizeof(struct smsg);
	smsg_ipc_cpt.txbuf_addr = base + CPT_TXBUF_ADDR;
	smsg_ipc_cpt.txbuf_rdptr = base + CPT_TXBUF_RDPTR;
	smsg_ipc_cpt.txbuf_wrptr = base + CPT_TXBUF_WRPTR;

	smsg_ipc_cpt.rxbuf_size = CPT_RXBUF_SIZE / sizeof(struct smsg);;
	smsg_ipc_cpt.rxbuf_addr = base + CPT_RXBUF_ADDR;
	smsg_ipc_cpt.rxbuf_rdptr = base + CPT_RXBUF_RDPTR;
	smsg_ipc_cpt.rxbuf_wrptr = base + CPT_RXBUF_WRPTR;

	smsg_ipc_create(SIPC_ID_CPT, &smsg_ipc_cpt);
	smem_init(SMEM_CPT_ADDR, SMEM_CPT_SIZE);

#ifdef CONFIG_DEBUG_FS
	debugfs_create_x32("txirq_trigger", S_IWUSR, NULL, txirq_trigger);
#endif

	return 0;
}

arch_initcall(sipc_init);

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC module driver");
MODULE_LICENSE("GPL");

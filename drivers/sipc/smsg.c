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
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>

#include <linux/sipc.h>
#include <linux/sipc_priv.h>

static struct smsg_ipc *smsg_ipcs[SIPC_ID_NR];

irqreturn_t smsg_irq_handler(int irq, void *dev_id)
{
	struct smsg_ipc *ipc = (struct smsg_ipc *)dev_id;
	struct smsg *msg;
	struct smsg_channel *ch;
	uint32_t rxpos, wr;

	if (ipc->rxirq_status()) {
		ipc->rxirq_clear();
	}

	while (readl(ipc->rxbuf_wrptr) != readl(ipc->rxbuf_rdptr)) {
		rxpos = (readl(ipc->rxbuf_rdptr) & (ipc->rxbuf_size - 1)) *
			sizeof (struct smsg) + ipc->rxbuf_addr;
		msg = (struct smsg *)rxpos;

		pr_debug("irq get smsg: wrptr=%d, rdptr=%d, rxpos=0x%08x\n",
			readl(ipc->rxbuf_wrptr), readl(ipc->rxbuf_rdptr), rxpos);
		pr_debug("irq read smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			msg->channel, msg->type, msg->flag, msg->value);

		if (msg->channel >= SMSG_CH_NR || msg->type >= SMSG_TYPE_NR) {
			/* invalid msg */
			printk(KERN_ERR "invalid smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
				msg->channel, msg->type, msg->flag, msg->value);

			/* update smsg rdptr */
			writel(readl(ipc->rxbuf_rdptr) + 1, ipc->rxbuf_rdptr);

			continue;
		}

		ch = ipc->channels[msg->channel];
		if (!ch) {
			if (ipc->states[msg->channel] == CHAN_STATE_UNUSED &&
					msg->type == SMSG_TYPE_OPEN &&
					msg->flag == SMSG_OPEN_MAGIC) {

				ipc->states[msg->channel] = CHAN_STATE_WAITING;
			} else {
				/* drop this bad msg since channel is not opened */
				printk(KERN_ERR "smsg channel %d not opened! "
					"drop smsg: type=%d, flag=0x%04x, value=0x%08x\n",
					msg->channel, msg->type, msg->flag, msg->value);
			}

			/* update smsg rdptr */
			writel(readl(ipc->rxbuf_rdptr) + 1, ipc->rxbuf_rdptr);

			continue;
		}

		if ((int)(readl(ch->wrptr) - readl(ch->rdptr)) >= SMSG_CACHE_NR) {
			/* msg cache is full, drop this msg */
			printk(KERN_ERR "smsg channel %d recv cache is full! "
				"drop smsg: type=%d, flag=0x%04x, value=0x%08x\n",
				msg->channel, msg->type, msg->flag, msg->value);
		} else {
			/* write smsg to cache */
			wr = readl(ch->wrptr) & (SMSG_CACHE_NR - 1);
			memcpy(&(ch->caches[wr]), msg, sizeof(struct smsg));
			writel(readl(ch->wrptr) + 1, ch->wrptr);
		}

		/* update smsg rdptr */
		writel(readl(ipc->rxbuf_rdptr) + 1, ipc->rxbuf_rdptr);

		wake_up_interruptible_all(&(ch->rxwait));
	}

	return IRQ_HANDLED;
}

int smsg_ipc_create(uint8_t dst, struct smsg_ipc *ipc)
{
	int rval;

	if (!ipc->irq_handler) {
		ipc->irq_handler = smsg_irq_handler;
	}

	mutex_init(&(ipc->txlock));
	smsg_ipcs[dst] = ipc;

	/* explicitly call irq handler in case of missing irq on boot */
	ipc->irq_handler(ipc->irq, ipc);

	/* register IPI irq */
	rval = request_irq(ipc->irq, ipc->irq_handler,
			0, ipc->name, ipc);
	if (rval != 0) {
		printk(KERN_ERR "Failed to request irq %s: %d\n",
				ipc->name, ipc->irq);
		return rval;
	}

	return 0;
}

int smsg_ipc_destroy(uint8_t dst)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];

	kthread_stop(ipc->thread);
	free_irq(ipc->irq, ipc);
	smsg_ipcs[dst] = NULL;

	return 0;
}

/* ****************************************************************** */

int smsg_ch_open(uint8_t dst, uint8_t channel, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch;
	struct smsg mopen, mrecv;
	uint32_t rval = 0;

	ch = kzalloc(sizeof(struct smsg_channel), GFP_KERNEL);
	if (!ch) {
		return -ENOMEM;
	}

	init_waitqueue_head(&(ch->rxwait));
	mutex_init(&(ch->rxlock));
	ipc->channels[channel] = ch;

	smsg_set(&mopen, channel, SMSG_TYPE_OPEN, SMSG_OPEN_MAGIC, 0);
	rval = smsg_send(dst, &mopen, timeout);
	if (rval != 0) {
		return rval;
	}

	/* open msg might be got before */
	if (ipc->states[channel] == CHAN_STATE_WAITING) {
		goto open_done;
	}

	smsg_set(&mrecv, channel, 0, 0, 0);
	rval = smsg_recv(dst, &mrecv, timeout);
	if (rval != 0) {
		ipc->channels[channel] = NULL;
		return rval;
	}

	if (mrecv.type != SMSG_TYPE_OPEN || mrecv.flag != SMSG_OPEN_MAGIC) {
		printk(KERN_ERR "Got bad open msg on channel %d-%d\n",
				dst, channel);
		ipc->channels[channel] = NULL;
		return -EIO;
	}

open_done:
	ipc->states[channel] = CHAN_STATE_OPENED;

	return 0;
}

int smsg_ch_close(uint8_t dst, uint8_t channel,  int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg mclose;

	smsg_set(&mclose, channel, SMSG_TYPE_CLOSE, SMSG_CLOSE_MAGIC, 0);
	smsg_send(dst, &mclose, timeout);

	kfree(ipc->channels[channel]);
	ipc->channels[channel] = NULL;

	return 0;
}

int smsg_send(uint8_t dst, struct smsg *msg, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	uint32_t txpos;
	int rval = 0;

	if (!ipc->channels[msg->channel]) {
		printk(KERN_ERR "channel %d not inited!\n", msg->channel);
		return -ENODEV;
	}

	if (ipc->states[msg->channel] != CHAN_STATE_OPENED &&
		msg->type != SMSG_TYPE_OPEN) {
		printk(KERN_ERR "channel %d not opened!\n", msg->channel);
		return -EINVAL;
	}

	pr_debug("smsg_send: dst=%d, channel=%d, timeout=%d\n",
			dst, msg->channel, timeout);
	pr_debug("send smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			msg->channel, msg->type, msg->flag, msg->value);

	if (timeout == 0) {
		if (!mutex_trylock(&(ipc->txlock))) {
			printk(KERN_INFO "send smsg busy!\n");
			return -EBUSY;
		}

		/* no wait */
		if ((int)(readl(ipc->txbuf_wrptr) -
			readl(ipc->txbuf_rdptr)) >= ipc->txbuf_size) {

			printk(KERN_WARNING "smsg txbuf is full!\n");
			rval = -EBUSY;

			goto send_failed;
		}
	} else {
		mutex_lock(&(ipc->txlock));

		/* wait timeout */
		if (timeout < 0) {
			timeout =  3600 * 1000;	/* 1 hour */
		}

		while ((int)(readl(ipc->txbuf_wrptr) -
			readl(ipc->txbuf_rdptr)) >= ipc->txbuf_size) {
			if (timeout < 0) {
				printk(KERN_WARNING "smsg txbuf is full, timeout!\n");
				rval = -ETIME;

				goto send_failed;
			}
			msleep(10);
			timeout -= 10;
		}
	}

	/* calc txpos and write smsg */
	txpos = (readl(ipc->txbuf_wrptr) & (ipc->txbuf_size - 1)) *
		sizeof(struct smsg) + ipc->txbuf_addr;
	memcpy((void *)txpos, msg, sizeof(struct smsg));

	pr_debug("write smsg: wrptr=%d, rdptr=%d, txpos=0x%08x\n",
			readl(ipc->txbuf_wrptr),
			readl(ipc->txbuf_rdptr), txpos);

	/* update wrptr */
	writel(readl(ipc->txbuf_wrptr) + 1, ipc->txbuf_wrptr);
	ipc->txirq_trigger();

send_failed:
	mutex_unlock(&(ipc->txlock));

	return rval;
}

int smsg_recv(uint8_t dst, struct smsg *msg, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch = ipc->channels[msg->channel];
	uint32_t rd;
	int rval = 0;

	if (!ch) {
		printk(KERN_ERR "channel %d not opened!\n", msg->channel);
		return -ENODEV;
	}

	pr_debug("smsg_recv: dst=%d, channel=%d, timeout=%d\n",
			dst, msg->channel, timeout);

	if (timeout == 0) {
		if (!mutex_trylock(&(ch->rxlock))) {
			printk(KERN_INFO "recv smsg busy!\n");
			return -EBUSY;
		}

		/* no wait */
		if (readl(ch->wrptr) == readl(ch->rdptr)) {
			printk(KERN_WARNING "smsg rx cache is empty!\n");
			rval = -ENODATA;

			goto recv_failed;
		}
	} else if (timeout < 0) {
		mutex_lock(&(ch->rxlock));

		/* wait forever */
		rval = wait_event_interruptible(ch->rxwait,
				readl(ch->wrptr) != readl(ch->rdptr));
		if (rval < 0) {
			printk(KERN_WARNING "smsg_recv wait interrupted!\n");

			goto recv_failed;
		}
	} else {
		mutex_lock(&(ch->rxlock));

		/* wait timeout */
		rval = wait_event_interruptible_timeout(ch->rxwait,
			readl(ch->wrptr) != readl(ch->rdptr), timeout);
		if (rval < 0) {
			printk(KERN_WARNING "smsg_recv wait interrupted!\n");

			goto recv_failed;
		} else if (rval == 0) {
			printk(KERN_WARNING "smsg_recv wait timeout!\n");
			rval = -ETIME;

			goto recv_failed;
		}
	}

	/* read smsg from cache */
	rd = readl(ch->rdptr) & (SMSG_CACHE_NR - 1);
	memcpy(msg, &(ch->caches[rd]), sizeof(struct smsg));
	writel(readl(ch->rdptr) + 1, ch->rdptr);

	pr_debug("read smsg: wrptr=%d, rdptr=%d, rd=%d\n",
			readl(ch->wrptr), readl(ch->rdptr), rd);
	pr_debug("recv smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			msg->channel, msg->type, msg->flag, msg->value);

recv_failed:
	mutex_unlock(&(ch->rxlock));

	return rval;
}

EXPORT_SYMBOL(smsg_ch_open);
EXPORT_SYMBOL(smsg_ch_close);
EXPORT_SYMBOL(smsg_send);
EXPORT_SYMBOL(smsg_recv);

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SMSG driver");
MODULE_LICENSE("GPL");

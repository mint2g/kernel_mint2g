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
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>

#include <linux/sipc.h>
#include "sblock.h"

static struct sblock_mgr *sblocks[SIPC_ID_NR][SMSG_CH_NR];

static void __sblock_put(struct sblock_mgr *sblock, uint32_t addr)
{
	void* virt_addr;
	uint32_t index;
	spin_lock(&sblock->ring->plock);
	virt_addr = addr - sblock->smem_addr + sblock->smem_virt;
	index = (virt_addr - sblock->smem_virt) / sblock->ring->header->txblk_size;
	list_add(&sblock->ring->txunits[index].list, &sblock->ring->txpool);
	spin_unlock(&sblock->ring->plock);
}

static int sblock_thread(void *data)
{
	struct sblock_mgr *sblock = data;
	struct smsg mcmd, mrecv;
	int rval;
	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	/* since the channel open may hang, we call it in the sblock thread */
	rval = smsg_ch_open(sblock->dst, sblock->channel, -1);
	if (rval != 0) {
		printk(KERN_ERR "Failed to open channel %d\n", sblock->channel);
		return rval;
	}

	/* handle the sblock events */
	while (!kthread_should_stop()) {
		/* monitor sblock recv smsg */
		smsg_set(&mrecv, sblock->channel, 0, 0, 0);
		smsg_recv(sblock->dst, &mrecv, -1);

		pr_debug("sblock thread recv msg: dst=%d, channel=%d, "
				"type=%d, flag=0x%04x, value=0x%08x\n",
				sblock->dst, sblock->channel,
				mrecv.type, mrecv.flag, mrecv.value);

		switch (mrecv.type) {
		case SMSG_TYPE_OPEN:
			/* handle channel recovery */
			smsg_open_ack(sblock->dst, sblock->channel);
			break;
		case SMSG_TYPE_CMD:
			/* respond cmd done for sblock init */
			WARN_ON(mrecv.flag != SMSG_CMD_SBLOCK_INIT);
			smsg_set(&mcmd, sblock->channel, SMSG_TYPE_DONE,
					SMSG_DONE_SBLOCK_INIT, sblock->smem_addr);
			smsg_send(sblock->dst, &mcmd, -1);
			if (sblock->handler) {
				sblock->handler(SBLOCK_NOTIFY_STATUS, sblock->data);
			}
			sblock->state = SBLOCK_STATE_READY;
			break;
		case SMSG_TYPE_EVENT:
			/* handle sblock send/release events */
			switch (mrecv.flag) {
			case SMSG_EVENT_SBLOCK_SEND:
				wake_up_interruptible_all(&sblock->ring->recvwait);
				if (sblock->handler) {
					sblock->handler(SBLOCK_NOTIFY_RECV, sblock->data);
				}
				break;
			case SMSG_EVENT_SBLOCK_RELEASE:
				__sblock_put(sblock, mrecv.value);
				wake_up_interruptible_all(&(sblock->ring->getwait));
				if (sblock->handler) {
					sblock->handler(SBLOCK_NOTIFY_GET, sblock->data);
				}
				break;
			default:
				rval = 1;
				break;
			}
			break;
		default:
			rval = 1;
			break;
		};
		if (rval) {
			printk(KERN_WARNING "non-handled sblock msg: %d-%d, %d, %d, %d\n",
					sblock->dst, sblock->channel,
					mrecv.type, mrecv.flag, mrecv.value);
			rval = 0;
		}
	}

	return rval;
}

int sblock_create(uint8_t dst, uint8_t channel,
		uint32_t txblocknum, uint32_t txblocksize,
		uint32_t rxblocknum, uint32_t rxblocksize)
{
	struct sblock_mgr *sblock;
	volatile struct sblock_ring_header *ringhd;
	uint32_t hsize;
	int i;

	sblock = kzalloc(sizeof(struct sblock_mgr) , GFP_KERNEL);
	if (!sblock) {
		return -ENOMEM;
	}

	sblock->state = SBLOCK_STATE_IDLE;
	sblock->dst = dst;
	sblock->channel = channel;
	sblock->txblksz = txblocksize;
	sblock->rxblksz = rxblocksize;

	/* allocate smem */
	hsize = sizeof(struct sblock_ring_header);
	sblock->smem_size = hsize +
		txblocknum * txblocksize + rxblocknum * rxblocksize +
		(txblocknum + rxblocknum) * sizeof(struct sblock_blks);
	sblock->smem_addr = smem_alloc(sblock->smem_size);
	if (!sblock->smem_addr) {
		printk(KERN_ERR "Failed to allocate smem for sblock\n");
		kfree(sblock);
		return -ENOMEM;
	}
	sblock->smem_virt = ioremap(sblock->smem_addr, sblock->smem_size);
	if (!sblock->smem_virt) {
		printk(KERN_ERR "Failed to map smem for sblock\n");
		smem_free(sblock->smem_addr, sblock->smem_size);
		kfree(sblock);
		return -EFAULT;
	}

	/* initialize ring and header */
	sblock->ring = kzalloc(sizeof(struct sblock_ring), GFP_KERNEL);
	if (!sblock->ring) {
		printk(KERN_ERR "Failed to allocate ring for sblock\n");
		iounmap(sblock->smem_virt);
		smem_free(sblock->smem_addr, sblock->smem_size);
		kfree(sblock);
		return -ENOMEM;
	}
	ringhd = (volatile struct sblock_ring_header *)(sblock->smem_virt);
	ringhd->txblk_addr = sblock->smem_addr + hsize;
	ringhd->txblk_count = txblocknum;
	ringhd->txblk_size = txblocksize;
	ringhd->txblk_rdptr = 0;
	ringhd->txblk_wrptr = 0;
	ringhd->txblk_blks = sblock->smem_addr + hsize +
		txblocknum * txblocksize + rxblocknum * rxblocksize;
	ringhd->rxblk_addr = ringhd->txblk_addr + txblocknum * txblocksize;
	ringhd->rxblk_count = rxblocknum;
	ringhd->rxblk_size = rxblocksize;
	ringhd->rxblk_rdptr = 0;
	ringhd->rxblk_wrptr = 0;
	ringhd->rxblk_blks = ringhd->txblk_blks + txblocknum * sizeof(struct sblock_blks);

	sblock->ring->header = sblock->smem_virt;
	sblock->ring->txblk_virt = sblock->smem_virt +
		(ringhd->txblk_addr - sblock->smem_addr);
	sblock->ring->txblks = sblock->smem_virt +
		(ringhd->txblk_blks - sblock->smem_addr);
	sblock->ring->rxblk_virt = sblock->smem_virt +
		(ringhd->rxblk_addr - sblock->smem_addr);
	sblock->ring->rxblks = sblock->smem_virt +
		(ringhd->rxblk_blks - sblock->smem_addr);

	sblock->ring->txunits = kzalloc(sizeof(struct sblock_txunit) * txblocknum, GFP_KERNEL);
	if (!sblock->ring->txunits) {
		printk(KERN_ERR "Failed to allocate txunits for sblock\n");
		kfree(sblock->ring);
		iounmap(sblock->smem_virt);
		smem_free(sblock->smem_addr, sblock->smem_size);
		kfree(sblock);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&sblock->ring->txpool);
	for (i = 0; i < txblocknum; i++) {
		sblock->ring->txunits[i].addr = sblock->ring->txblk_virt + i * txblocksize;
		list_add_tail(&sblock->ring->txunits[i].list, &sblock->ring->txpool);
	}

	init_waitqueue_head(&sblock->ring->getwait);
	init_waitqueue_head(&sblock->ring->recvwait);
	mutex_init(&sblock->ring->txlock);
	mutex_init(&sblock->ring->rxlock);
	spin_lock_init(&sblock->ring->plock);

	sblock->thread = kthread_create(sblock_thread, sblock,
			"sblock-%d-%d", dst, channel);
	if (IS_ERR(sblock->thread)) {
		printk(KERN_ERR "Failed to create kthread: sblock-%d-%d\n", dst, channel);
		kfree(sblock->ring->txunits);
		kfree(sblock->ring);
		iounmap(sblock->smem_virt);
		smem_free(sblock->smem_addr, sblock->smem_size);
		kfree(sblock);
		return PTR_ERR(sblock->thread);
	}

	sblocks[dst][channel]=sblock;
	wake_up_process(sblock->thread);

	return 0;
}

void sblock_destroy(uint8_t dst, uint8_t channel)
{
	struct sblock_mgr *sblock = sblocks[dst][channel];

	sblock->state = SBLOCK_STATE_IDLE;
	kthread_stop(sblock->thread);

	kfree(sblock->ring->txunits);
	kfree(sblock->ring);
	iounmap(sblock->smem_virt);
	smem_free(sblock->smem_addr, sblock->smem_size);
	kfree(sblock);

	sblocks[dst][channel]=NULL;
}

int sblock_register_notifier(uint8_t dst, uint8_t channel,
		void (*handler)(int event, void *data), void *data)
{
	struct sblock_mgr *sblock = sblocks[dst][channel];

	if (!sblock) {
		printk(KERN_ERR "sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	if (sblock->handler) {
		printk(KERN_ERR "sblock handler already registered\n");
		return -EBUSY;
	}

	sblock->handler = handler;
	sblock->data = data;

	return 0;
}

int sblock_get(uint8_t dst, uint8_t channel, struct sblock *blk, int timeout)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	struct list_head *head;
	struct sblock_txunit *txunit;
	int rval = 0;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		printk(KERN_ERR "sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	ring = sblock->ring;
	ringhd = ring->header;
	head = &sblock->ring->txpool;

	if (list_empty(head)) {
		if (timeout == 0) {
			/* no wait */
			printk(KERN_WARNING "sblock_get %d-%d is empty!\n",
				dst, channel);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(ring->getwait, !list_empty(head));
			if (rval < 0) {
				printk(KERN_WARNING "sblock_get wait interrupted!\n");
			}
		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(ring->getwait,
					!list_empty(head), timeout);
			if (rval < 0) {
				printk(KERN_WARNING "sblock_get wait interrupted!\n");
			} else if (rval == 0) {
				printk(KERN_WARNING "sblock_get wait timeout!\n");
				rval = -ETIME;
			}
		}
	}

	if (rval) {
		return rval;
	}

	/* multi-gotter may cause got failure */
	spin_lock(&ring->plock);
	if (!list_empty(head)) {
		txunit = list_entry(head->next, struct sblock_txunit, list);
		blk->addr = txunit->addr;
		blk->length = sblock->txblksz;
		list_del(head->next);
	} else {
		rval = -EAGAIN;
	}
	spin_unlock(&ring->plock);

	return rval;
}

int sblock_send(uint8_t dst, uint8_t channel, struct sblock *blk)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	struct smsg mevt;
	int txpos;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		printk(KERN_ERR "sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	pr_debug("sblock_send: dst=%d, channel=%d, addr=%p, len=%d\n",
			dst, channel, blk->addr, blk->length);

	ring = sblock->ring;
	ringhd = ring->header;

	mutex_lock(&ring->txlock);

	txpos = ringhd->txblk_wrptr % ringhd->txblk_count;
	ring->txblks[txpos].addr = blk->addr - sblock->smem_virt + sblock->smem_addr;
	ring->txblks[txpos].length = blk->length;
	pr_debug("sblock_send: wrptr=%d, txpos=%d, addr=%x\n",
			ringhd->txblk_wrptr, txpos, ring->txblks[txpos].addr);
	ringhd->txblk_wrptr = ringhd->txblk_wrptr + 1;
	smsg_set(&mevt, channel, SMSG_TYPE_EVENT, SMSG_EVENT_SBLOCK_SEND, 0);
	smsg_send(dst, &mevt, -1);

	mutex_unlock(&ring->txlock);

	return 0;
}

int sblock_receive(uint8_t dst, uint8_t channel, struct sblock *blk, int timeout)
{
	struct sblock_mgr *sblock = sblocks[dst][channel];
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	int rxpos, rval = 0;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		printk(KERN_ERR "sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	ring = sblock->ring;
	ringhd = ring->header;

	pr_debug("sblock_receive: dst=%d, channel=%d, timeout=%d\n",
			dst, channel, timeout);
	pr_debug("sblock_receive: wrptr=%d, rdptr=%d",
			ringhd->rxblk_wrptr, ringhd->rxblk_rdptr);

	if (ringhd->rxblk_wrptr == ringhd->rxblk_rdptr) {
		if (timeout == 0) {
			/* no wait */
			printk(KERN_WARNING "sblock_receive %d-%d is empty!\n",
				dst, channel);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(ring->recvwait,
				ringhd->rxblk_wrptr != ringhd->rxblk_rdptr);
			if (rval < 0) {
				printk(KERN_WARNING "sblock_receive wait interrupted!\n");
			}
		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(ring->recvwait,
				ringhd->rxblk_wrptr != ringhd->rxblk_rdptr, timeout);
			if (rval < 0) {
				printk(KERN_WARNING "sblock_receive wait interrupted!\n");
			} else if (rval == 0) {
				printk(KERN_WARNING "sblock_receive wait timeout!\n");
				rval = -ETIME;
			}
		}
	}

	if (rval) {
		return rval;
	}

	/* multi-receiver may cause recv failure */
	mutex_lock(&ring->rxlock);
	if (ringhd->rxblk_wrptr != ringhd->rxblk_rdptr){
		rxpos = ringhd->rxblk_rdptr % ringhd->rxblk_count;
		blk->addr = ring->rxblks[rxpos].addr - sblock->smem_addr + sblock->smem_virt;
		blk->length = ring->rxblks[rxpos].length;
		ringhd->rxblk_rdptr = ringhd->rxblk_rdptr + 1;
		pr_debug("sblock_receive: rxpos=%d, addr=%p, len=%d\n",
			rxpos, blk->addr, blk->length);
	} else {
		rval = -EAGAIN;
	}
	mutex_unlock(&ring->rxlock);

	return rval;
}

int sblock_release(uint8_t dst, uint8_t channel, struct sblock *blk)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	struct smsg mevt;
	uint32_t addr;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		printk(KERN_ERR "sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	pr_debug("sblock_release: dst=%d, channel=%d, addr=%p, len=%d\n",
			dst, channel, blk->addr, blk->length);

	ring = sblock->ring;
	ringhd = ring->header;

	addr = blk->addr - sblock->smem_virt + sblock->smem_addr;
	pr_debug("sblock_release: addr=%x\n", addr);

	/* send released rx block index */
	smsg_set(&mevt, channel, SMSG_TYPE_EVENT, SMSG_EVENT_SBLOCK_RELEASE, addr);
	smsg_send(dst, &mevt, -1);

	return 0;
}

EXPORT_SYMBOL(sblock_create);
EXPORT_SYMBOL(sblock_destroy);
EXPORT_SYMBOL(sblock_register_notifier);
EXPORT_SYMBOL(sblock_get);
EXPORT_SYMBOL(sblock_send);
EXPORT_SYMBOL(sblock_receive);
EXPORT_SYMBOL(sblock_release);

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SBLOCK driver");
MODULE_LICENSE("GPL");

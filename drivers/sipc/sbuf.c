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
#include <asm/uaccess.h>

#include <linux/sipc.h>
#include "sbuf.h"

static struct sbuf_mgr *sbufs[SIPC_ID_NR][SMSG_CH_NR];

static int sbuf_thread(void *data)
{
	struct sbuf_mgr *sbuf = data;
	struct smsg mcmd, mrecv;
	int rval, bufid;
	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	/* since the channel open may hang, we call it in the sbuf thread */
	rval = smsg_ch_open(sbuf->dst, sbuf->channel, -1);
	if (rval != 0) {
		printk(KERN_ERR "Failed to open channel %d\n", sbuf->channel);
		return rval;
	}

	/* sbuf init done, handle the ring rx events */
	while (!kthread_should_stop()) {
		/* monitor sbuf rdptr/wrptr update smsg */
		smsg_set(&mrecv, sbuf->channel, 0, 0, 0);
		smsg_recv(sbuf->dst, &mrecv, -1);

		pr_debug("sbuf thread recv msg: dst=%d, channel=%d, "
				"type=%d, flag=0x%04x, value=0x%08x\n",
				sbuf->dst, sbuf->channel,
				mrecv.type, mrecv.flag, mrecv.value);

		switch (mrecv.type) {
		case SMSG_TYPE_OPEN:
			/* handle channel recovery */
			smsg_open_ack(sbuf->dst, sbuf->channel);
			break;
		case SMSG_TYPE_CMD:
			/* respond cmd done for sbuf init */
			WARN_ON(mrecv.flag != SMSG_CMD_SBUF_INIT);
			smsg_set(&mcmd, sbuf->channel, SMSG_TYPE_DONE,
					SMSG_DONE_SBUF_INIT, sbuf->smem_addr);
			smsg_send(sbuf->dst, &mcmd, -1);
			sbuf->state = SBUF_STATE_READY;
			break;
		case SMSG_TYPE_EVENT:
			bufid = mrecv.value;
			WARN_ON(bufid >= sbuf->ringnr);
			switch (mrecv.flag) {
			case SMSG_EVENT_SBUF_RDPTR:
				wake_up_interruptible_all(&(sbuf->rings[bufid].txwait));
				break;
			case SMSG_EVENT_SBUF_WRPTR:
				wake_up_interruptible_all(&(sbuf->rings[bufid].rxwait));
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
			printk(KERN_WARNING "non-handled sbuf msg: %d-%d, %d, %d, %d\n",
					sbuf->dst, sbuf->channel,
					mrecv.type, mrecv.flag, mrecv.value);
			rval = 0;
		}
	}

	return 0;
}

int sbuf_create(uint8_t dst, uint8_t channel, uint32_t bufnum,
		uint32_t txbufsize, uint32_t rxbufsize)
{
	struct sbuf_mgr *sbuf;
	volatile struct sbuf_smem_header *smem;
	volatile struct sbuf_ring_header *ringhd;
	int hsize, i;

	sbuf = kzalloc(sizeof(struct sbuf_mgr), GFP_KERNEL);
	if (!sbuf) {
		printk(KERN_ERR "Failed to allocate mgr for sbuf\n");
		return -ENOMEM;
	}

	sbuf->state = SBUF_STATE_IDLE;
	sbuf->dst = dst;
	sbuf->channel = channel;
	sbuf->ringnr = bufnum;

	/* allocate smem */
	hsize = sizeof(struct sbuf_smem_header) + sizeof(struct sbuf_ring_header) * bufnum;
	sbuf->smem_size = hsize + (txbufsize + rxbufsize) * bufnum;
	sbuf->smem_addr = smem_alloc(sbuf->smem_size);
	if (!sbuf->smem_addr) {
		printk(KERN_ERR "Failed to allocate smem for sbuf\n");
		kfree(sbuf);
		return -ENOMEM;
	}
	sbuf->smem_virt = ioremap(sbuf->smem_addr, sbuf->smem_size);
	if (!sbuf->smem_virt) {
		printk(KERN_ERR "Failed to map smem for sbuf\n");
		smem_free(sbuf->smem_addr, sbuf->smem_size);
		kfree(sbuf);
		return -EFAULT;
	}

	/* allocate rings description */
	sbuf->rings = kzalloc(sizeof(struct sbuf_ring) * bufnum, GFP_KERNEL);
	if (!sbuf->rings) {
		printk(KERN_ERR "Failed to allocate rings for sbuf\n");
		iounmap(sbuf->smem_virt);
		smem_free(sbuf->smem_addr, sbuf->smem_size);
		kfree(sbuf);
		return -ENOMEM;
	}

	/* initialize all ring bufs */
	smem = (volatile struct sbuf_smem_header *)sbuf->smem_virt;
	smem->ringnr = bufnum;
	for (i = 0; i < bufnum; i++) {
		ringhd = (volatile struct sbuf_ring_header *)&(smem->headers[i]);
		ringhd->txbuf_addr = sbuf->smem_addr + hsize +
				(txbufsize + rxbufsize) * i;
		ringhd->txbuf_size = txbufsize;
		ringhd->txbuf_rdptr = 0;
		ringhd->txbuf_wrptr = 0;
		ringhd->rxbuf_addr = smem->headers[i].txbuf_addr + txbufsize;
		ringhd->rxbuf_size = rxbufsize;
		ringhd->rxbuf_rdptr = 0;
		ringhd->rxbuf_wrptr = 0;

		sbuf->rings[i].header = ringhd;
		sbuf->rings[i].txbuf_virt = sbuf->smem_virt + hsize +
				(txbufsize + rxbufsize) * i;
		sbuf->rings[i].rxbuf_virt = sbuf->rings[i].txbuf_virt + txbufsize;
		init_waitqueue_head(&(sbuf->rings[i].txwait));
		init_waitqueue_head(&(sbuf->rings[i].rxwait));
		mutex_init(&(sbuf->rings[i].txlock));
		mutex_init(&(sbuf->rings[i].rxlock));
	}

	sbuf->thread = kthread_create(sbuf_thread, sbuf,
			"sbuf-%d-%d", dst, channel);
	if (IS_ERR(sbuf->thread)) {
		printk(KERN_ERR "Failed to create kthread: sbuf-%d-%d\n", dst, channel);
		kfree(sbuf->rings);
		iounmap(sbuf->smem_virt);
		smem_free(sbuf->smem_addr, sbuf->smem_size);
		kfree(sbuf);

		return PTR_ERR(sbuf->thread);
	}

	sbufs[dst][channel] = sbuf;
	wake_up_process(sbuf->thread);

	return 0;
}

void sbuf_destroy(uint8_t dst, uint8_t channel)
{
	struct sbuf_mgr *sbuf = sbufs[dst][channel];

	sbuf->state = SBUF_STATE_IDLE;
	kthread_stop(sbuf->thread);

	kfree(sbuf->rings);
	iounmap(sbuf->smem_virt);
	smem_free(sbuf->smem_addr, sbuf->smem_size);
	kfree(sbuf);

	sbufs[dst][channel] = NULL;
}

int sbuf_write(uint8_t dst, uint8_t channel, uint32_t bufid,
		void *buf, uint32_t len, int timeout)
{
	struct sbuf_mgr *sbuf = sbufs[dst][channel];
	struct sbuf_ring *ring = &(sbuf->rings[bufid]);
	volatile struct sbuf_ring_header *ringhd = ring->header;
	struct smsg mevt;
	void *txpos;
	int rval, left, tail, txsize;

	if (!sbuf) {
		return -ENODEV;
	}
	if (sbuf->state != SBUF_STATE_READY) {
		printk(KERN_ERR "sbuf-%d-%d not ready to write!\n", dst, channel);
		return -ENODEV;
	}

	pr_debug("sbuf_write: dst=%d, channel=%d, bufid=%d, len=%d, timeout=%d\n",
			dst, channel, bufid, len, timeout);
	pr_debug("sbuf_write: wrptr=%d, rdptr=%d",
			ringhd->txbuf_wrptr, ringhd->txbuf_rdptr);

	rval = 0;
	left = len;

	if (timeout) {
		mutex_lock(&ring->txlock);
	} else {
		if (!mutex_trylock(&(ring->txlock))) {
			printk(KERN_INFO "sbuf_write busy!\n");
			return -EBUSY;
		}
	}

	if (timeout == 0) {
		/* no wait */
		if ((int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) >=
				ringhd->txbuf_size) {
			printk(KERN_WARNING "sbuf %d-%d ring %d txbuf is full!\n",
				dst, channel, bufid);
			rval = -EBUSY;
		}
	} else if (timeout < 0) {
		/* wait forever */
		rval = wait_event_interruptible(ring->txwait,
			(int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) <
			ringhd->txbuf_size);
		if (rval < 0) {
			printk(KERN_WARNING "sbuf_write wait interrupted!\n");
		}
	} else {
		/* wait timeout */
		rval = wait_event_interruptible_timeout(ring->txwait,
			(int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) <
			ringhd->txbuf_size, timeout);
		if (rval < 0) {
			printk(KERN_WARNING "sbuf_write wait interrupted!\n");
		} else if (rval == 0) {
			printk(KERN_WARNING "sbuf_write wait timeout!\n");
			rval = -ETIME;
		}
	}

	while (left && (int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) < ringhd->txbuf_size) {
		/* calc txpos & txsize */
		txpos = ring->txbuf_virt + ringhd->txbuf_wrptr % ringhd->txbuf_size;
		txsize = ringhd->txbuf_size - (int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr);
		txsize = min(txsize, left);

		tail = txpos + txsize - (ring->txbuf_virt + ringhd->txbuf_size);
		if (tail > 0) {
			/* ring buffer is rounded */
			if ((uint32_t)buf > TASK_SIZE) {
				memcpy(txpos, buf, txsize - tail);
				memcpy(ring->txbuf_virt, buf + txsize - tail, tail);
			} else {
				if(copy_from_user(txpos, (void __user *)buf, txsize - tail) ||
				    copy_from_user(ring->txbuf_virt,
				    (void __user *)(buf + txsize - tail), tail)) {
					printk(KERN_ERR "sbuf_write: failed to copy from user!\n");
					rval = -EFAULT;
					break;
				}
			}
		} else {
			if ((uint32_t)buf > TASK_SIZE) {
				memcpy(txpos, buf, txsize);
			} else {
				/* handle the user space address */
				if(copy_from_user(txpos, (void __user *)buf, txsize)) {
					printk(KERN_ERR "sbuf_write: failed to copy from user!\n");
					rval = -EFAULT;
					break;
				}
			}
		}


		pr_debug("sbuf_write: txpos=%p, txsize=%d\n", txpos, txsize);

		/* update tx wrptr */
		ringhd->txbuf_wrptr = ringhd->txbuf_wrptr + txsize;
		smsg_set(&mevt, channel, SMSG_TYPE_EVENT, SMSG_EVENT_SBUF_WRPTR, bufid);
		smsg_send(dst, &mevt, -1);

		left -= txsize;
		buf += txsize;
	}

	mutex_unlock(&ring->txlock);

	pr_debug("sbuf_write done: len=%d\n", len - left);

	if (len == left) {
		return rval;
	} else {
		return (len - left);
	}
}

int sbuf_read(uint8_t dst, uint8_t channel, uint32_t bufid,
		void *buf, uint32_t len, int timeout)
{
	struct sbuf_mgr *sbuf = sbufs[dst][channel];
	struct sbuf_ring *ring = &(sbuf->rings[bufid]);
	volatile struct sbuf_ring_header *ringhd = ring->header;
	struct smsg mevt;
	void *rxpos;
	int rval, left, tail, rxsize;

	if (!sbuf) {
		return -ENODEV;
	}
	if (sbuf->state != SBUF_STATE_READY) {
		printk(KERN_ERR "sbuf-%d-%d not ready to read!\n", dst, channel);
		return -ENODEV;
	}

	pr_debug("sbuf_read: dst=%d, channel=%d, bufid=%d, len=%d, timeout=%d\n",
			dst, channel, bufid, len, timeout);
	pr_debug("sbuf_read: wrptr=%d, rdptr=%d",
			ringhd->rxbuf_wrptr, ringhd->rxbuf_rdptr);

	rval = 0;
	left = len;

	if (timeout) {
		mutex_lock(&ring->rxlock);
	} else {
		if (!mutex_trylock(&(ring->rxlock))) {
			printk(KERN_INFO "sbuf_read busy!\n");
			return -EBUSY;
		}
	}

	if (ringhd->rxbuf_wrptr == ringhd->rxbuf_rdptr) {
		if (timeout == 0) {
			/* no wait */
			printk(KERN_WARNING "sbuf %d-%d ring %d rxbuf is empty!\n",
				dst, channel, bufid);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(ring->rxwait,
				ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr);
			if (rval < 0) {
				printk(KERN_WARNING "sbuf_read wait interrupted!\n");
			}
		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(ring->rxwait,
				ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr, timeout);
			if (rval < 0) {
				printk(KERN_WARNING "sbuf_read wait interrupted!\n");
			} else if (rval == 0) {
				printk(KERN_WARNING "sbuf_read wait timeout!\n");
				rval = -ETIME;
			}
		}
	}

	while (left && (ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr)) {
		/* calc rxpos & rxsize */
		rxpos = ring->rxbuf_virt + ringhd->rxbuf_rdptr % ringhd->rxbuf_size;
		rxsize = (int)(ringhd->rxbuf_wrptr - ringhd->rxbuf_rdptr);
		/* check overrun */
		WARN_ON(rxsize > ringhd->rxbuf_size);
		rxsize = min(rxsize, left);

		pr_debug("sbuf_read: buf=%p, rxpos=%p, rxsize=%d\n", buf, rxpos, rxsize);

		tail = rxpos + rxsize - (ring->rxbuf_virt + ringhd->rxbuf_size);
		if (tail > 0) {
			/* ring buffer is rounded */
			if ((uint32_t)buf > TASK_SIZE) {
				memcpy(buf, rxpos, rxsize - tail);
				memcpy(buf + rxsize - tail, ring->rxbuf_virt, tail);
			} else {
				/* handle the user space address */
				if(copy_to_user((void __user *)buf, rxpos, rxsize - tail) ||
				    copy_to_user((void __user *)(buf + rxsize - tail),
				    ring->rxbuf_virt, tail)) {
					printk(KERN_ERR "sbuf_read: failed to copy to user!\n");
					rval = -EFAULT;
					break;
				}
			}
		} else {
			if ((uint32_t)buf > TASK_SIZE) {
				memcpy(buf, rxpos, rxsize);
			} else {
				/* handle the user space address */
				if (copy_to_user((void __user *)buf, rxpos, rxsize)) {
					printk(KERN_ERR "sbuf_read: failed to copy to user!\n");
					rval = -EFAULT;
					break;
				}
			}
		}

		/* update rx rdptr */
		ringhd->rxbuf_rdptr = ringhd->rxbuf_rdptr + rxsize;
		smsg_set(&mevt, channel, SMSG_TYPE_EVENT, SMSG_EVENT_SBUF_RDPTR, bufid);
		smsg_send(dst, &mevt, -1);

		left -= rxsize;
		buf += rxsize;
	}

	mutex_unlock(&ring->rxlock);

	pr_debug("sbuf_read done: len=%d", len - left);

	if (len == left) {
		return rval;
	} else {
		return (len - left);
	}
}

int sbuf_poll_wait(uint8_t dst, uint8_t channel, uint32_t bufid,
		struct file *filp, poll_table *wait)
{
	struct sbuf_mgr *sbuf = sbufs[dst][channel];
	struct sbuf_ring *ring = &(sbuf->rings[bufid]);
	volatile struct sbuf_ring_header *ringhd = ring->header;
	unsigned int mask = 0;

	if (!sbuf) {
		return -ENODEV;
	}
	if (sbuf->state != SBUF_STATE_READY) {
		printk(KERN_ERR "sbuf-%d-%d not ready to poll !\n", dst, channel);
		return -ENODEV;
	}

	poll_wait(filp, &ring->txwait, wait);
	poll_wait(filp, &ring->rxwait, wait);

	if (ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr) {
		mask |= POLLIN | POLLRDNORM;
	}

	if (ringhd->txbuf_wrptr - ringhd->txbuf_rdptr < ringhd->txbuf_size) {
		mask |= POLLOUT | POLLWRNORM;
	}

	return mask;
}

int sbuf_status(uint8_t dst, uint8_t channel)
{
	struct sbuf_mgr *sbuf = sbufs[dst][channel];

	if (!sbuf) {
		return -ENODEV;
	}
	if (sbuf->state != SBUF_STATE_READY) {
		return -ENODEV;
	}

	return 0;
}

EXPORT_SYMBOL(sbuf_create);
EXPORT_SYMBOL(sbuf_destroy);
EXPORT_SYMBOL(sbuf_write);
EXPORT_SYMBOL(sbuf_read);
EXPORT_SYMBOL(sbuf_poll_wait);
EXPORT_SYMBOL(sbuf_status);

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SBUF driver");
MODULE_LICENSE("GPL");

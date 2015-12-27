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

#ifndef __SBLOCK_H
#define __SBLOCK_H

/* flag for CMD/DONE msg type */
#define SMSG_CMD_SBLOCK_INIT		0x0001
#define SMSG_DONE_SBLOCK_INIT		0x0002

/* flag for EVENT msg type */
#define SMSG_EVENT_SBLOCK_SEND		0x0001
#define SMSG_EVENT_SBLOCK_RELEASE	0x0002

#define SBLOCK_STATE_IDLE		0
#define SBLOCK_STATE_READY		1

struct sblock_blks {
	uint32_t		addr; /*phy address*/
	uint32_t		length;
};

struct sblock_txunit {
	void*			addr; /*virt address*/
	struct list_head	list;
};

/* ring block header */
struct sblock_ring_header {
	/* send-block info */
	uint32_t		txblk_addr;
	uint32_t		txblk_count;
	uint32_t		txblk_size;
	uint32_t		txblk_blks;
	uint32_t		txblk_rdptr;
	uint32_t		txblk_wrptr;

	/* recv-block info */
	uint32_t		rxblk_addr;
	uint32_t		rxblk_count;
	uint32_t		rxblk_size;
	uint32_t		rxblk_blks;
	uint32_t		rxblk_rdptr;
	uint32_t		rxblk_wrptr;
};

struct sblock_ring {
	struct sblock_ring_header	*header;
	void			*txblk_virt; /* virt of header->txblk_addr */
	void			*rxblk_virt; /* virt of header->rxblk_addr */
	struct sblock_blks	*txblks;     /* virt of header->txblk_blks */
	struct sblock_blks	*rxblks;     /* virt of header->rxblk_blks */

	struct sblock_txunit	*txunits;    /* txblk units pool */
	struct list_head	txpool;
	spinlock_t		plock;

	struct mutex		txlock;
	struct mutex		rxlock;

	wait_queue_head_t	getwait;
	wait_queue_head_t	recvwait;
};

struct sblock_mgr {
	uint8_t			dst;
	uint8_t			channel;
	uint32_t		state;

	void			*smem_virt;
	uint32_t		smem_addr;
	uint32_t		smem_size;

	uint32_t		txblksz;
	uint32_t		rxblksz;

	struct sblock_ring	*ring;
	struct task_struct	*thread;

	void			(*handler)(int event, void *data);
	void			*data;
};

#endif

/*
 ****************************************************************
 *
 *  Component: VLX VMQ driver implementation
 *
 *  Copyright (C) 2011, Red Bend Ltd.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  You should have received a copy of the GNU General Public License Version 2
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *  Contributor(s):
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

/*----- System header files -----*/

#include <linux/version.h>
#include <linux/module.h>	/* __exit, __init */
#include <linux/sched.h>	/* TASK_INTERRUPTIBLE */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <nk/nkern.h>

/*----- Local configuration -----*/

#if 0
#define VMQ_DEBUG
#endif

#if 0
#define VMQ_ODEBUG		/* Activates old/noisy debug traces */
#endif

/*----- Local header files -----*/

#include "vlx-vmq.h"

/*----- Tracing -----*/

#define WTRACE(x...)	printk (KERN_WARNING "VMQ: " x)
#define ETRACE(x...)	printk (KERN_ERR     "VMQ: " x)

#ifdef VMQ_DEBUG
#define DTRACE(_f, _a...) \
	printk ("(%d) %s: " _f, current->tgid, __func__, ## _a)
#define VMQ_BUG_ON(x) \
	do { \
	    if (x) { \
		ETRACE ("Unexpected '%s' in %s in %s() line %d\n", \
			#x, __FILE__, __func__, __LINE__); \
		BUG(); \
	    } \
	} while (0)
#else
#define DTRACE(x...)
#define VMQ_BUG_ON(x)
#endif

#ifdef VMQ_ODEBUG
#define OTRACE(x...)	DTRACE(x)
#else
#define OTRACE(x...)
#endif

/*----- Locking -----*/

    /* Locking cannot be nested */

#define VMQ_LOCK(_spinlock, _flags) \
    do { \
	OTRACE ("lock\n"); \
	spin_lock_irqsave ((_spinlock), (_flags)); \
    } while (0)

#define VMQ_UNLOCK(_spinlock, _flags) \
    do { \
	OTRACE ("unlock\n"); \
	spin_unlock_irqrestore ((_spinlock), (_flags)); \
    } while (0)

/*----- Version compatibility functions -----*/

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,7)
    static inline void*
kzalloc (size_t size, unsigned flags)
{
    void* ptr = kmalloc (size, flags);
    if (ptr) {
	memset (ptr, 0, size);
    }
    return ptr;
}
#endif

#ifndef container_of
#define container_of(ptr, type, member) \
	((type*)((char*)(ptr)-(unsigned long)(&((type*) 0)->member)))
#endif

#ifndef list_for_each_entry
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		     prefetch(pos->member.next);			\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, typeof(*pos), member),	\
		     prefetch(pos->member.next))
#endif

#ifndef list_for_each_entry_type
#define list_for_each_entry_type(pos, type, head, member) \
	list_for_each_entry(pos, head, member)
#endif

/*----- Shared data types -----*/

#define VMQ_ALIGN __attribute__ ((aligned (L1_CACHE_BYTES)))

    /* Header located at pmem start */

typedef struct {
    volatile nku32_f	unsafe_p_idx;		/* producer index */
    volatile nku32_f	unsafe_c_idx;		/* consumer index */
    volatile nku8_f	unsafe_stopped;
    volatile nku8_f	unused1 [3];
    volatile nku32_f	unused2;
} VMQ_ALIGN vmq_head;

typedef char vmq_check1_t [sizeof (vmq_head) % L1_CACHE_BYTES ? -1 : 1];

    /* One index slot */

typedef struct {
    volatile nku32_f	unsafe_ss_number;
} vmq_is;

#define VMQ_PMEM_ID	0
#define VMQ_RXIRQ_ID	1
#define VMQ_TXIRQ_ID	2

typedef struct {
    vmq_head	head;
    vmq_is	is [1] VMQ_ALIGN;
	/* Short slots */
	/* Possible padding */
	/* Long slots */
} vmq_pmem;

typedef char vmq_check2_t [sizeof (vmq_pmem) % L1_CACHE_BYTES ? -1 : 1];

#define VMQ_ROUNDUP(x,y)	((((x)+(y)-1) / (y)) * (y))
#define VMQ_SIZEOF_VMQ_PMEM \
	(sizeof (vmq_pmem) - VMQ_ROUNDUP (sizeof (vmq_is), L1_CACHE_BYTES))

typedef char vmq_check3_t [VMQ_SIZEOF_VMQ_PMEM % L1_CACHE_BYTES ? -1 : 1];

/*----- Helper macros -----*/

    /*
     *  Typecast allows to deal with 64 bit machines and situations
     *  where the producer has already overflown 32 bits and the
     *  consumer not yet.
     */

#define VMQ_UNSAFE_HEAD_SPACE(_head) \
    (nku32_f) ((_head)->unsafe_p_idx - (_head)->unsafe_c_idx)

#define VMQ_UNSAFE_HEAD_SPACE_OK(_xx) \
    (VMQ_UNSAFE_HEAD_SPACE (&(_xx)->pmem->head) <= (_xx)->config.msg_count)

#define VMQ_UNSAFE_HEAD_SPACE_ASSERT(_xx) \
    do {VMQ_BUG_ON (!VMQ_UNSAFE_HEAD_SPACE_OK(_xx));} while (0)

#define VMQ_SAFE_HEAD_SPACE(_xx) \
    (nku32_f) ((_xx)->safe_p_idx - (_xx)->safe_c_idx)

#define VMQ_SAFE_HEAD_SPACE_OK(_xx) \
    (VMQ_SAFE_HEAD_SPACE (_xx) <= (_xx)->config.msg_count)

#define VMQ_SAFE_XX_IS_FULL(_xx) \
    ((nku32_f) ((_xx)->safe_p_idx - (_xx)->last_x_idx) >= \
    (_xx)->config.msg_count)

#define VMQ_UNSAFE_C_PENDING(_xx) \
    (nku32_f) ((_xx)->pmem->head.unsafe_c_idx - (_xx)->last_x_idx)

#define VMQ_SAFE_C_PENDING(_xx) \
    (nku32_f) ((_xx)->safe_c_idx - (_xx)->last_x_idx)

#define VMQ_UNSAFE_P_PENDING(_xx) \
    (nku32_f) ((_xx)->pmem->head.unsafe_p_idx - (_xx)->last_x_idx)

#define VMQ_SAFE_P_PENDING(_xx) \
    (nku32_f) ((_xx)->safe_p_idx - (_xx)->last_x_idx)

    /* Macros for Index slots */

#define VMQ_TX_INDEX_SAFE_P_IDX(_xx) \
    ((_xx)->pmem->is + ((_xx)->safe_p_idx & (_xx)->ring_index_mask))

#define VMQ_RX_INDEX_SAFE_C_IDX(_xx) \
    ((_xx)->pmem->is + ((_xx)->safe_c_idx & (_xx)->ring_index_mask))

#define VMQ_XX_INDEX_LAST_X_IDX(_xx) \
    ((_xx)->pmem->is + ((_xx)->last_x_idx & (_xx)->ring_index_mask))

    /* Macros for Short slots */

#define VMQ_SHORT_SLOT(_xx, slot) \
	((_xx)->ss_area + (_xx)->config.msg_max * (slot))
#define VMQ_SHORT_SLOT_NUM(_xx,_ss) \
	(((char*)(_ss) - (_xx)->ss_area)) / (_xx)->config.msg_max

    /* Macros for Long slots */

#define VMQ_LONG_SLOT(_xx, slot) \
	((_xx)->ls_area + (_xx)->config.data_max * (slot))
#define VMQ_LONG_SLOT_NUM(_xx, _ls) \
	(((char*)(_ls) - (_xx)->ls_area)) / (_xx)->config.data_max

/*----- Generic functions -----*/

    static inline void
vmq_sysconf_trigger (NkOsId osid)
{
    DTRACE ("os %d\n", osid);
    nkops.nk_xirq_trigger (NK_XIRQ_SYSCONF, osid);
}

/*----- Generic channel management -----*/

typedef struct {
    NkXIrq		local_xirq;
    unsigned		local_xirqs_received;
    NkXIrqId		xid;		/* Handler id */
    NkXIrq		peer_xirq;
    unsigned		peer_xirqs_sent;
    NkDevVlink*		vlink;
    vmq_pmem*		pmem;
    NkPhAddr		paddr;
    _Bool		aborted;
    nku32_f		last_x_idx;	/* last producer/consumer index */
    nku32_f		safe_p_idx;
    nku32_f		safe_c_idx;
    vmq_xx_config_t	config;
    unsigned		ring_index_mask;
    char*		ss_area;	/* Inside pmem */
    char*		ls_area;	/* Inside pmem */
    unsigned		ss_checked_out;
    unsigned		ls_checked_out;
    spinlock_t*		spinlock;
    _Bool		need_flush;
	/* Statistics */
    unsigned		waits;
    unsigned		out_of_msgs;
    unsigned		self_xirqs;
} vmq_xx;

    /* VMQ_LOCK must be taken on entry */

    static void
vmq_xx_abort (vmq_xx* xx, _Bool server)
{
    xx->aborted = true;
    if (server) {
	xx->vlink->s_state = NK_DEV_VLINK_OFF;
	vmq_sysconf_trigger (xx->vlink->c_id);
    } else {
	xx->vlink->c_state = NK_DEV_VLINK_OFF;
	vmq_sysconf_trigger (xx->vlink->s_id);
    }
}

    /* Performed in sender. Only unsafe_c_idx can have changed */

    static signed
vmq_xx_return_msg_receive (vmq_xx* xx, void** msg,
			   const struct list_head* ss_heads)
{
    vmq_head*		head = &xx->pmem->head;
    unsigned long	flags;

    VMQ_LOCK (xx->spinlock, flags);
    OTRACE ("pending %d\n", VMQ_UNSAFE_C_PENDING (xx));
	/* Sample unsafe value before checking it */
    xx->safe_c_idx = head->unsafe_c_idx;
    if (!VMQ_SAFE_HEAD_SPACE_OK (xx)) {
	vmq_xx_abort (xx, false);
	VMQ_UNLOCK (xx->spinlock, flags);
	return -ESTALE;
    }
    rmb();
    if (VMQ_SAFE_C_PENDING (xx) > 0) {
	const unsigned ss_number =
	    VMQ_XX_INDEX_LAST_X_IDX (xx)->unsafe_ss_number;

	if (ss_number >= xx->config.msg_count ||
		!list_empty (ss_heads + ss_number)) {
	    vmq_xx_abort (xx, false);
	    VMQ_UNLOCK (xx->spinlock, flags);
	    return -ESTALE;
	}
	*msg = VMQ_SHORT_SLOT (xx, ss_number);
	OTRACE ("msg %p p_idx %d/%d last_x_idx %d c_idx %d/%d\n", *msg,
		head->unsafe_p_idx, xx->safe_p_idx, xx->last_x_idx,
		head->unsafe_c_idx, xx->safe_c_idx);
	++xx->last_x_idx;
	++xx->ss_checked_out;
	VMQ_UNLOCK (xx->spinlock, flags);
	return 0;
    }
    VMQ_UNLOCK (xx->spinlock, flags);
    return -EAGAIN;
}

    /* VMQ_LOCK must be taken */

    static inline _Bool
vmq_xx_ring_is_full (vmq_xx* xx)
{
    return VMQ_SAFE_XX_IS_FULL (xx);
}

    /* VMQ_LOCK must be taken */

    static inline _Bool
vmq_xx_all_returned (vmq_xx* xx)
{
    return !xx->ss_checked_out && !xx->ls_checked_out;
}

static void vmq_tx_sysconf_notify (vmq_xx* xx);
static void vmq_rx_sysconf_notify (vmq_xx* xx);

    static inline void
vmq_xx_xirq_trigger_server (vmq_xx* xx)
{
    OTRACE ("xirq %d os %d\n", xx->peer_xirq, xx->vlink->s_id);
    ++xx->peer_xirqs_sent;
    nkops.nk_xirq_trigger (xx->peer_xirq, xx->vlink->s_id);
}

    static inline void
vmq_xx_xirq_trigger_client (vmq_xx* xx)
{
    ++xx->peer_xirqs_sent;
    nkops.nk_xirq_trigger (xx->peer_xirq, xx->vlink->c_id);
}

    /* Only called by vmq_msg_send() */

    static void
vmq_xx_msg_send (vmq_xx* xx, void* msg, _Bool flush)
{
    vmq_pmem*		xx_pmem = xx->pmem;
    const unsigned	ss_number = VMQ_SHORT_SLOT_NUM (xx, msg);
    unsigned long	flags;

    VMQ_BUG_ON (ss_number >= xx->config.msg_count);
    VMQ_LOCK (xx->spinlock, flags);
    --xx->ss_checked_out;
    if (xx->aborted) {
	const _Bool sysconf_notify = vmq_xx_all_returned (xx);

	VMQ_UNLOCK (xx->spinlock, flags);
	    /* We do not wake-up anybody, this is done by (*link_off)() */
	if (sysconf_notify) {
	    vmq_tx_sysconf_notify (xx);
	}
	return;
    }
	/* Write ss_number at position safe_p_idx */
    VMQ_TX_INDEX_SAFE_P_IDX (xx)->unsafe_ss_number = ss_number;
    wmb();
    xx_pmem->head.unsafe_p_idx = ++xx->safe_p_idx;
    if (vmq_xx_ring_is_full (xx)) {
	xx_pmem->head.unsafe_stopped = 1;
    }
    xx->need_flush = !flush;
    VMQ_UNLOCK (xx->spinlock, flags);
    if (flush) {
	vmq_xx_xirq_trigger_server (xx);
    }
}

    static inline void
vmq_xx_msg_send_flush (vmq_xx* xx)
{
    _Bool		flush;
    unsigned long	flags;

    VMQ_LOCK (xx->spinlock, flags);
    flush = xx->need_flush;
    xx->need_flush = false;
    VMQ_UNLOCK (xx->spinlock, flags);
    if (flush) {
	vmq_xx_xirq_trigger_server (xx);
    }
}

    /* Only called by vmq_msg_free() and vmq_msg_return() */

    static void
vmq_xx_msg_free (vmq_xx* xx, void* msg, _Bool signal)
{
    vmq_pmem*	xx_pmem = xx->pmem;
    vmq_head*	head	= &xx_pmem->head;
	/*
	 *  Convert msg pointer to a ss_number
	 *  and place it in index at slot head.c_idx.
	 */
    const unsigned	ss_number = VMQ_SHORT_SLOT_NUM (xx, msg);
    unsigned long	flags;

    VMQ_BUG_ON (ss_number >= xx->config.msg_count);
    OTRACE ("msg %p p_idx %d/%d last_x_idx %d c_idx %d/%d slot %d\n", msg,
	    head->unsafe_p_idx, xx->safe_p_idx, xx->last_x_idx,
	    head->unsafe_c_idx, xx->safe_c_idx, ss_number);
    VMQ_LOCK (xx->spinlock, flags);
    --xx->ss_checked_out;
    if (xx->aborted) {
	const _Bool sysconf_notify = vmq_xx_all_returned (xx);

	VMQ_UNLOCK (xx->spinlock, flags);
	    /* We do not wake-up anybody, this is done by (*link_off)() */
	if (sysconf_notify) {
	    vmq_rx_sysconf_notify (xx);
	}
	return;
    }
    VMQ_RX_INDEX_SAFE_C_IDX (xx)->unsafe_ss_number = ss_number;
    wmb();
    head->unsafe_c_idx = ++xx->safe_c_idx;
    VMQ_UNLOCK (xx->spinlock, flags);

	/* Send tx xirq if producer ring was stopped (full) */
    if (head->unsafe_stopped || signal) {
	vmq_xx_xirq_trigger_client (xx);
    }
}

    /* Performed in receiver. Only unsafe_p_idx can have changed */

    static signed
vmq_xx_msg_receive (vmq_xx* xx, void** msg)
{
    vmq_head*		head = &xx->pmem->head;
    unsigned long	flags;

    VMQ_LOCK (xx->spinlock, flags);
    OTRACE ("pending %d\n", VMQ_UNSAFE_P_PENDING (xx));
	/* Sample unsafe value before checking it */
    xx->safe_p_idx = head->unsafe_p_idx;
    if (!VMQ_SAFE_HEAD_SPACE_OK (xx)) {
	vmq_xx_abort (xx, true);
	VMQ_UNLOCK (xx->spinlock, flags);
	return -ESTALE;
    }
    rmb();
    if (VMQ_SAFE_P_PENDING (xx) > 0) {
	const unsigned ss_number =
	    VMQ_XX_INDEX_LAST_X_IDX (xx)->unsafe_ss_number;

	if (ss_number >= xx->config.msg_count) {
	    vmq_xx_abort (xx, true);
	    VMQ_UNLOCK (xx->spinlock, flags);
	    return -ESTALE;
	}
	*msg = VMQ_SHORT_SLOT (xx, ss_number);
	OTRACE ("msg %p p_idx %d/%d last_x_idx %d c_idx %d/%d\n", *msg,
		head->unsafe_p_idx, xx->safe_p_idx, xx->last_x_idx,
		head->unsafe_c_idx, xx->safe_c_idx);
	++xx->last_x_idx;
	++xx->ss_checked_out;
	VMQ_UNLOCK (xx->spinlock, flags);
	return 0;
    }
    VMQ_UNLOCK (xx->spinlock, flags);

	/* Send tx xirq if producer ring was stopped (full) */
    if (head->unsafe_stopped) {
	vmq_xx_xirq_trigger_client (xx);
    }
    return -EAGAIN;
}

    static size_t
vmq_xx_config_ss_total (const vmq_xx_config_t* config)
{
    unsigned ss_total = VMQ_SIZEOF_VMQ_PMEM +
	config->msg_count * (sizeof (vmq_is) + config->msg_max);

    if (config->data_max >= PAGE_SIZE && !(config->data_max % PAGE_SIZE)) {
	DTRACE ("Rounding up ss_total from 0x%x to 0x%x\n",
		ss_total, (unsigned) VMQ_ROUNDUP (ss_total, PAGE_SIZE));
	ss_total = VMQ_ROUNDUP (ss_total, PAGE_SIZE);
    }
    return ss_total;
}

    static inline unsigned
vmq_xx_config_ls_total (const vmq_xx_config_t* config)
{
    return config->data_count * config->data_max;
}

    static inline size_t
vmq_xx_config_pmem_size (const vmq_xx_config_t* config)
{
    return vmq_xx_config_ss_total (config) + vmq_xx_config_ls_total (config);
}

    static void
vmq_xx_finish (vmq_xx* xx)
{
    if (xx->xid) {
	nkops.nk_xirq_detach (xx->xid);
	xx->xid = 0;
    }
    if (xx->pmem) {
	nkops.nk_mem_unmap (xx->pmem, xx->paddr,
			    vmq_xx_config_pmem_size (&xx->config));
	xx->pmem = NULL;
    }
}

    static signed
vmq_xx_init (vmq_xx* xx, NkDevVlink* vlink, _Bool tx,
	     const vmq_xx_config_t* config, spinlock_t* spinlock)
{
    size_t	pmem_size;
    signed	diag;

    xx->config = *config;
    xx->config.msg_max  = VMQ_ROUNDUP (xx->config.msg_max,  L1_CACHE_BYTES);
    xx->config.data_max = VMQ_ROUNDUP (xx->config.data_max, L1_CACHE_BYTES);
    xx->spinlock = spinlock;

    if (config->msg_count <= 0 ||
	    (config->msg_count & (config->msg_count-1))) {
	ETRACE ("OS %d->OS %d link %d %s invalid msg_count %d.\n",
		vlink->c_id, vlink->s_id, vlink->link,
		tx ? "client" : "server", config->msg_count);
	return -EINVAL;
    }
    xx->ring_index_mask = config->msg_count - 1;

    pmem_size = vmq_xx_config_pmem_size (&xx->config);
    xx->vlink = vlink;
    xx->paddr = nkops.nk_pmem_alloc (nkops.nk_vtop (vlink), VMQ_PMEM_ID,
				     pmem_size);
    if (!xx->paddr) {
	ETRACE ("OS %d->OS %d link %d %s pmem alloc failed (%d bytes).\n",
		vlink->c_id, vlink->s_id, vlink->link,
		tx ? "client" : "server", pmem_size);
	return -ENOMEM;
    }
    xx->pmem = (vmq_pmem*) nkops.nk_mem_map (xx->paddr, pmem_size);
    if (!xx->pmem) {
	ETRACE ("Error while mapping\n");
	return -EAGAIN;
    }
	/* Check if last byte is readable */
    diag = *((char*) xx->pmem + pmem_size - 1);
    (void) diag;

    xx->ss_area = (char*) &xx->pmem->is [xx->config.msg_count];
    xx->ls_area = (char*) xx->pmem + vmq_xx_config_ss_total (&xx->config);
	/*
	 *  phys/virt
	 *  +------+-------+-------+---------+------+
	 *  | HEAD | INDEX | SHORT | PADDING | LONG |
	 *  +------+-------+-------+---------+------+
	 *  <----------------bytes------------------>
	 */
    DTRACE ("%s: bytes 0x%x phys %llx virt %p\n",
	    tx ? "tx" : "rx", pmem_size,
	    (long long) xx->paddr, xx->pmem);
    DTRACE ("0x%x + 0x%x + 0x%x + 0x%x + 0x%x\n",
	    sizeof ((vmq_pmem*) 0)->head,
	    xx->config.msg_count * sizeof (vmq_is),
	    xx->config.msg_count * xx->config.msg_max,
	    xx->ls_area - VMQ_SHORT_SLOT (xx, xx->config.msg_count),
	    vmq_xx_config_ls_total (&xx->config));

    xx->local_xirq = nkops.nk_pxirq_alloc (nkops.nk_vtop (vlink),
					   tx ? VMQ_TXIRQ_ID : VMQ_RXIRQ_ID,
					   tx ? vlink->c_id : vlink->s_id, 1);
    if (!xx->local_xirq) {
	ETRACE ("OS %d->OS %d link %d server pxirq alloc failed.\n",
		vlink->c_id, vlink->s_id, vlink->link);
	diag = -ENOMEM;
	goto error;
    }
    xx->peer_xirq = nkops.nk_pxirq_alloc (nkops.nk_vtop (vlink),
					  tx ? VMQ_RXIRQ_ID : VMQ_TXIRQ_ID,
					  tx ? vlink->s_id : vlink->c_id, 1);
    if (!xx->peer_xirq) {
	ETRACE ("OS %d->OS %d link %d client pxirq alloc failed.\n",
		vlink->c_id, vlink->s_id, vlink->link);
	diag = -ENOMEM;
	goto error;
    }
    return 0;

error:
    vmq_xx_finish (xx);
    return diag;
}

    static inline NkPhAddr
vmq_xx_pls_area (vmq_xx* xx)
{
    return xx->paddr + xx->ls_area - (char*) xx->pmem;
}

    static signed
vmq_xx_start (vmq_xx* xx, NkXIrqHandler hdl)
{
    xx->xid = nkops.nk_xirq_attach (xx->local_xirq, hdl, xx);
    if (!xx->xid) {
	ETRACE ("OS %d->OS %d link %d server cannot attach xirq handler.\n",
		xx->vlink->c_id, xx->vlink->s_id, xx->vlink->link);
	return -ENOMEM;
    }
    return 0;
}

    static inline void
vmq_xx_reset_rx (vmq_xx* xx)
{
    DTRACE ("\n");
    xx->last_x_idx		= 0;
    xx->safe_c_idx		= 0;
    xx->pmem->head.unsafe_c_idx	= 0;
}

    static inline void
vmq_xx_reset_tx (vmq_xx* xx)
{
    DTRACE ("\n");
    xx->last_x_idx		= 0;
    xx->safe_p_idx		= 0;
    xx->pmem->head.unsafe_p_idx	= 0;
    xx->pmem->head.unsafe_stopped	= 0;
}

    static inline _Bool
vmq_xx_vlink_on (vmq_xx* xx)
{
    return xx->vlink->c_state == NK_DEV_VLINK_ON &&
	   xx->vlink->s_state == NK_DEV_VLINK_ON;
}

/*----- Transmission channel: management -----*/

typedef struct {
    vmq_xx		xx;
    struct list_head	free_ss;
    struct list_head*	ss_heads;
    struct list_head	free_ls;
    struct list_head*	ls_heads;
    wait_queue_head_t	slots_wait_queue;
} vmq_tx;

    /* VMQ_LOCK should be taken */

    static inline void
vmq_tx_slots_freed (vmq_tx* tx)
{
    OTRACE ("\n");
    wake_up (&tx->slots_wait_queue);
}

    /*
     *  VMQ_LOCK must be taken.
     *  Only called by vmq_tx_return_msg_free() and vmq_tx_slots_init().
     */

    static inline void
vmq_tx_enqueue_ss (vmq_tx* tx, unsigned ss_number)
{
    VMQ_BUG_ON (ss_number >= tx->xx.config.msg_count);
    list_add (tx->ss_heads + ss_number, &tx->free_ss);
}

    /*
     *  VMQ_LOCK must be taken
     *  Only called by vmq_tx_msg_allocate().
     */

    static inline char*
vmq_tx_dequeue_ss (vmq_tx* tx)
{
    struct list_head* lh = tx->free_ss.next;

    VMQ_BUG_ON (list_empty (&tx->free_ss));
    VMQ_BUG_ON ((unsigned) (lh - tx->ss_heads) >= tx->xx.config.msg_count);
    list_del_init (lh);
	/* Now list_empty(lh) is true */
    VMQ_BUG_ON (tx->xx.ss_checked_out >= tx->xx.config.msg_count);
    ++tx->xx.ss_checked_out;
    return VMQ_SHORT_SLOT (&tx->xx, lh - tx->ss_heads);
}

    /* Only called by vmq_tx_notify() and vmq_return_msg_free() */

    static void
vmq_tx_return_msg_free (vmq_tx* tx, void* msg)
{
    _Bool		sysconf_notify;
    unsigned long	flags;

    VMQ_LOCK (tx->xx.spinlock, flags);
    vmq_tx_enqueue_ss (tx, VMQ_SHORT_SLOT_NUM (&tx->xx, msg));
    --tx->xx.ss_checked_out;
    sysconf_notify = tx->xx.aborted && vmq_xx_all_returned (&tx->xx);
    VMQ_UNLOCK (tx->xx.spinlock, flags);
    if (sysconf_notify) {
	vmq_tx_sysconf_notify (&tx->xx);
    }
}

    /* VMQ_LOCK must be taken */

    static inline void
vmq_tx_enqueue_ls (vmq_tx* tx, unsigned ls_number)
{
    VMQ_BUG_ON (ls_number >= tx->xx.config.data_count);
    list_add (tx->ls_heads + ls_number, &tx->free_ls);
}

    /* VMQ_LOCK must be taken */

    static inline nku32_f
vmq_tx_dequeue_ls (vmq_tx* tx)
{
    struct list_head* lh = tx->free_ls.next;

    list_del (lh);
    ++tx->xx.ls_checked_out;
    DTRACE ("data_offset %d\n",
	    VMQ_LONG_SLOT (&tx->xx, lh - tx->ls_heads) - tx->xx.ls_area);
    return VMQ_LONG_SLOT (&tx->xx, lh - tx->ls_heads) - tx->xx.ls_area;
}

static void vmq_tx_notify (vmq_tx* tx);

    static void
vmq_tx_hdl (void* cookie, NkXIrq xirq)
{
    vmq_tx*	tx = (vmq_tx*) cookie;

    (void) xirq;
    ++tx->xx.local_xirqs_received;
	/*
	 * Do not access PMEM if not initialized yet. This silences
	 * valgrind errors "Conditional jump or move depends on
	 * uninitialised value(s)" and "Use of uninitialised value
	 * of size 4".
	 */
    if (!vmq_xx_vlink_on (&tx->xx)) {
	DTRACE ("Ignoring xirq %d from %d because vlink not On\n",
		xirq, tx->xx.vlink->s_id);
	return;
    }
    OTRACE ("xirq %d pending %d\n", xirq, VMQ_UNSAFE_C_PENDING (&tx->xx));
    vmq_tx_notify (tx);
    if (tx->xx.pmem->head.unsafe_stopped) {
	unsigned long flags;

	VMQ_LOCK (tx->xx.spinlock, flags);
	if (!vmq_xx_ring_is_full (&tx->xx)) {
	    tx->xx.pmem->head.unsafe_stopped = 0;
		/* wake sending */
	    vmq_tx_slots_freed (tx);
	}
	VMQ_UNLOCK (tx->xx.spinlock, flags);
    }
}

    static inline signed
vmq_tx_msg_allocate (vmq_tx* tx, unsigned data_len, void** msg,
		     unsigned* data_offset, _Bool nonblocking)
{
    vmq_pmem*		tx_pmem = tx->xx.pmem;
    vmq_head*		head    = &tx_pmem->head;
    unsigned long	flags;

    VMQ_BUG_ON ((data_len && !data_offset) || (!data_len && data_offset));
    if (!data_offset) data_len = 0;
    OTRACE ("data_len %d\n", data_len);
    if (data_len > tx->xx.config.data_max) {
	ETRACE ("Data size %u is too large for tx.\n", data_len);
	return -E2BIG;
    }
    while (true) {
	_Bool flush;

	if (tx->xx.vlink->s_state != NK_DEV_VLINK_ON) {
		/*
		 *  This trace can be issued a great many times before
		 *  the handshake is finally performed to turn the
		 *  device to aborted state (this depends on the
		 *  priority of sysconf thread versus request threads)
		 *  and before link users notices the device is definitely
		 *  lost, so do not flood the console with warnings
		 *  during normal usage.
		 */
	    DTRACE ("Peer driver %d not ready\n", tx->xx.vlink->s_id);
	    return -EAGAIN;
	}
	VMQ_LOCK (tx->xx.spinlock, flags);
	OTRACE ("tx: p_idx %d/%d c_idx %d/%d last_x_idx %d\n",
		head->unsafe_p_idx, tx->xx.safe_p_idx,
		head->unsafe_c_idx, tx->xx.safe_c_idx, tx->xx.last_x_idx);
	if (tx->xx.aborted) {
	    DTRACE ("tx aborted\n");
	    VMQ_UNLOCK (tx->xx.spinlock, flags);
	    return -ECONNABORTED;
	}
	VMQ_UNSAFE_HEAD_SPACE_ASSERT (&tx->xx);
	DTRACE ("c-pending %d\n", VMQ_UNSAFE_C_PENDING (&tx->xx));
	    /*
	     * Instead of checking !list_empty(free_ss) we used to check
	     * !vmq_xx_ring_is_full(tx) here, but this was not correct, as
	     * we can have a situation where there is place in the ring,
	     * but messages are still handled by the caller.
	     */
	if (!list_empty (&tx->free_ss) &&
		(!data_len || !list_empty (&tx->free_ls))) {
	    VMQ_BUG_ON (vmq_xx_ring_is_full (&tx->xx));
	    break;
	}
	if (VMQ_UNSAFE_C_PENDING (&tx->xx)) {
	    DTRACE ("out of msgs, but have c-pending\n");
		/*
		 * Trigger an xirq to ourselves to execute vmq_tx_hdl()
		 * and return pending freed messages back to free_ss
		 * list in proper context.
		 */
	    nkops.nk_xirq_trigger (tx->xx.local_xirq, tx->xx.vlink->c_id);
	    ++tx->xx.self_xirqs;
	} else {
	    head->unsafe_stopped = 1;
	    DTRACE ("tx ring is full\n");
	}
	flush = tx->xx.need_flush;
	tx->xx.need_flush = false;
	if (flush) {
	    vmq_xx_xirq_trigger_server (&tx->xx);
	}
	if (nonblocking) {
	    ++tx->xx.out_of_msgs;
	    VMQ_UNLOCK (tx->xx.spinlock, flags);
	    return -EAGAIN;
	}
	{
	    DECLARE_WAITQUEUE (wait, current);

	    set_current_state (TASK_INTERRUPTIBLE);
	    add_wait_queue (&tx->slots_wait_queue, &wait);
	    ++tx->xx.waits;
	    VMQ_UNLOCK (tx->xx.spinlock, flags);
	    schedule();
	    remove_wait_queue (&tx->slots_wait_queue, &wait);
	}
	if (signal_pending (current)) {
	    DTRACE ("tx signal pending\n");
	    return -EINTR;
	}
    }
    *msg = vmq_tx_dequeue_ss (tx);
    if (data_offset) {
	*data_offset = data_len ? vmq_tx_dequeue_ls (tx) : 0;
    }
    VMQ_UNLOCK (tx->xx.spinlock, flags);
    OTRACE ("msg %p\n", *msg);
    return 0;
}

    /*
     *  VMQ_LOCK must NOT be taken.
     *  Only called from vmq_data_free().
     */

    static inline void
vmq_tx_data_free (vmq_tx* tx, unsigned data_offset)
{
    char*		ls = tx->xx.ls_area + data_offset;
    const unsigned	ls_number = VMQ_LONG_SLOT_NUM (&tx->xx, ls);
    _Bool		sysconf_notify;
    unsigned long	flags;

    VMQ_LOCK (tx->xx.spinlock, flags);
    vmq_tx_enqueue_ls (tx, ls_number);
    VMQ_BUG_ON (!tx->xx.ls_checked_out);
    --tx->xx.ls_checked_out;
    sysconf_notify = tx->xx.aborted && vmq_xx_all_returned (&tx->xx);
    vmq_tx_slots_freed (tx);
    VMQ_UNLOCK (tx->xx.spinlock, flags);
    if (sysconf_notify) {
	vmq_tx_sysconf_notify (&tx->xx);
    }
}

    /* Called by vmq_tx_init() */
    /* Called by vmq_tx_vlink_off_completed */

    static void
vmq_tx_slots_init (vmq_tx* tx)
{
    unsigned slot;

    DTRACE ("\n");
    INIT_LIST_HEAD (&tx->free_ss);
    INIT_LIST_HEAD (&tx->free_ls);
    for (slot = 0; slot < tx->xx.config.msg_count; ++slot) {
	vmq_tx_enqueue_ss (tx, slot);
    }
    for (slot = 0; slot < tx->xx.config.data_count; ++slot) {
	vmq_tx_enqueue_ls (tx, slot);
    }
    VMQ_BUG_ON (tx->xx.ss_checked_out);
    VMQ_BUG_ON (tx->xx.ls_checked_out);
}

    /* VMQ_LOCK must NOT be taken */

    static _Bool
vmq_tx_abort (vmq_tx* tx, void* cookie)
{
    unsigned long flags;

    (void) cookie;
    VMQ_LOCK (tx->xx.spinlock, flags);
    tx->xx.aborted = true;
    vmq_tx_slots_freed (tx);	/* Unblock waiters */
    VMQ_UNLOCK (tx->xx.spinlock, flags);
    return false;
}

    static _Bool
vmq_tx_handshake (vmq_tx* tx)
{
    volatile int*	my_state   = &tx->xx.vlink->c_state;
    volatile int*	peer_state = &tx->xx.vlink->s_state;
    _Bool		need_sysconf = false;

    DTRACE ("entry %d->%d\n", *my_state, *peer_state);

    switch (*my_state) {
    case NK_DEV_VLINK_OFF:
	if (*peer_state != NK_DEV_VLINK_ON) {
	    vmq_xx_reset_tx (&tx->xx);
	    *my_state = NK_DEV_VLINK_RESET;
	    need_sysconf = true;
	}
	break;
    case NK_DEV_VLINK_RESET:
	if (*peer_state != NK_DEV_VLINK_OFF) {
	    *my_state = NK_DEV_VLINK_ON;
	    need_sysconf = true;
	}
	break;
    case NK_DEV_VLINK_ON:
	if (*peer_state == NK_DEV_VLINK_OFF) {
	    vmq_tx_abort (tx, NULL);
	}
	break;
    }
    DTRACE ("exit %d->%d\n", *my_state, *peer_state);
    return need_sysconf;
}

    static void
vmq_tx_vlink_off_completed (vmq_tx* tx)
{
    if (tx->xx.vlink->s_state == NK_DEV_VLINK_OFF) {
	vmq_xx_reset_tx (&tx->xx);
	vmq_tx_slots_init (tx);
	tx->xx.vlink->c_state = NK_DEV_VLINK_RESET;
	tx->xx.aborted = false;
	DTRACE ("exit %d->%d\n", tx->xx.vlink->c_state, tx->xx.vlink->s_state);
    }
}

    static void
vmq_tx_finish (vmq_tx* tx)
{
    tx->xx.vlink->c_state = NK_DEV_VLINK_OFF;
    kfree (tx->ss_heads);
    kfree (tx->ls_heads);
    vmq_xx_finish (&tx->xx);
}

    static signed
vmq_tx_init (vmq_tx* tx, NkDevVlink* tx_vlink, const vmq_xx_config_t* config,
	     spinlock_t* spinlock)
{
    signed diag;

    DTRACE ("\n");
    diag = vmq_xx_init (&tx->xx, tx_vlink, true /*tx*/, config, spinlock);
    if (diag) return diag;	/* Error message already issued */

    tx->ss_heads = (struct list_head*)
	kzalloc (config->msg_count * sizeof (struct list_head), GFP_KERNEL);
    if (!tx->ss_heads) {
	ETRACE ("Could not allocate memory for vmq tx.\n");
	vmq_tx_finish (tx);
	return -ENOMEM;
    }
    if (config->data_count) {
	tx->ls_heads = (struct list_head*)
	    kzalloc (config->data_count * sizeof (struct list_head),
		     GFP_KERNEL);
	if (!tx->ls_heads) {
	    ETRACE ("Could not allocate memory for vmq tx.\n");
	    vmq_tx_finish (tx);
	    return -ENOMEM;
	}
    }
    vmq_tx_slots_init (tx);
    init_waitqueue_head (&tx->slots_wait_queue);
    return 0;
}

/*----- Reception channel: management -----*/

typedef struct {
    vmq_xx		xx;
} vmq_rx;

static void vmq_rx_notify (vmq_rx* rx);

    static void
vmq_rx_hdl (void* cookie, NkXIrq xirq)
{
    vmq_rx* rx = (vmq_rx*) cookie;

    (void) xirq;
    ++rx->xx.local_xirqs_received;
	/*
	 * Do not access PMEM if not initialized yet. This silences
	 * valgrind errors "Conditional jump or move depends on
	 * uninitialised value(s)" and "Use of uninitialised value
	 * of size 4".
	 */
    if (!vmq_xx_vlink_on (&rx->xx)) {
	DTRACE ("Ignoring xirq %d from %d because vlink not On\n",
		xirq, rx->xx.vlink->c_id);
	return;
    }
    OTRACE ("xirq %d pending %d\n", xirq, VMQ_UNSAFE_P_PENDING (&rx->xx));
    vmq_rx_notify (rx);
}

    static _Bool
vmq_rx_abort (vmq_rx* rx, void* cookie)
{
    (void) cookie;
    rx->xx.aborted = true;
    return false;
}

    static _Bool
vmq_rx_handshake (vmq_rx* rx)
{
    volatile int*	my_state   = &rx->xx.vlink->s_state;
    volatile int*	peer_state = &rx->xx.vlink->c_state;
    _Bool		need_sysconf = false;

    DTRACE ("entry %d->%d\n", *peer_state, *my_state);

    switch (*my_state) {
    case NK_DEV_VLINK_OFF:
	if (*peer_state != NK_DEV_VLINK_ON) {
	    vmq_xx_reset_rx (&rx->xx);
	    *my_state = NK_DEV_VLINK_RESET;
	    need_sysconf = true;
	}
	break;
    case NK_DEV_VLINK_RESET:
	if (*peer_state != NK_DEV_VLINK_OFF) {
	    *my_state = NK_DEV_VLINK_ON;
	    need_sysconf = true;
	}
	break;
    case NK_DEV_VLINK_ON:
	if (*peer_state == NK_DEV_VLINK_OFF) {
	    vmq_rx_abort (rx, NULL);
	}
	break;
    }
    DTRACE ("exit %d->%d\n", *peer_state, *my_state);
    return need_sysconf;
}

    static void
vmq_rx_vlink_off_completed (vmq_rx* rx)
{
    if (rx->xx.vlink->c_state == NK_DEV_VLINK_OFF) {
	vmq_xx_reset_rx (&rx->xx);
	rx->xx.vlink->s_state = NK_DEV_VLINK_RESET;
	rx->xx.aborted = false;
	DTRACE ("exit %d->%d\n", rx->xx.vlink->c_state, rx->xx.vlink->s_state);
    }
}

    static void
vmq_rx_finish (vmq_rx* rx)
{
    rx->xx.vlink->s_state = NK_DEV_VLINK_OFF;
    vmq_xx_finish (&rx->xx);
}

    static signed
vmq_rx_init (vmq_rx* rx, NkDevVlink* rx_vlink, const vmq_xx_config_t* config,
	     spinlock_t* spinlock)
{
    return vmq_xx_init (&rx->xx, rx_vlink, false /*!tx*/, config, spinlock);
}

/*----- Transmission channel: API -----*/

struct vmq_link_t {
    vmq_link_public_t		public2;	/* Must be first */
    struct list_head		link;
    vmq_tx			tx;
    vmq_rx			rx;
    struct vmq_links_t*		links;
    const vmq_callbacks_t*	callbacks;
    signed			is_on;		/* -1 if still unknown */
};

    static void
vmq_tx_notify (vmq_tx* tx)
{
    vmq_link_t* link2 = container_of (tx, vmq_link_t, tx);

    if (link2->callbacks->return_notify) {
	link2->callbacks->return_notify (link2);
    } else {
	void* msg;

	while (!vmq_xx_return_msg_receive (&tx->xx, &msg, tx->ss_heads)) {
	    vmq_tx_return_msg_free (tx, msg);
	}
    }
}

    signed
vmq_msg_allocate_ex (vmq_link_t* link2, unsigned data_len, void** msg,
		     unsigned* data_offset, _Bool nonblocking)
{
    return vmq_tx_msg_allocate (&link2->tx, data_len, msg, data_offset,
				nonblocking);
}

    void
vmq_msg_send (vmq_link_t* link2, void* msg)
{
    vmq_xx_msg_send (&link2->tx.xx, msg, true);
}

    void
vmq_msg_send_async (vmq_link_t* link2, void* msg)
{
    vmq_xx_msg_send (&link2->tx.xx, msg, false);
}

    void
vmq_msg_send_flush (vmq_link_t* link2)
{
    vmq_xx_msg_send_flush (&link2->tx.xx);
}

    void
vmq_data_free (vmq_link_t* link2, unsigned data_offset)
{
    vmq_tx_data_free (&link2->tx, data_offset);
}

    signed
vmq_return_msg_receive (vmq_link_t* link2, void** msg)
{
    return vmq_xx_return_msg_receive (&link2->tx.xx, msg, link2->tx.ss_heads);
}

    void
vmq_return_msg_free (vmq_link_t* link2, void* msg)
{
    vmq_tx_return_msg_free (&link2->tx, msg);
}

    unsigned
vmq_msg_slot (vmq_link_t* link2, const void* msg)
{
    return VMQ_SHORT_SLOT_NUM (&link2->tx.xx, msg);
}

/*----- Reception channel: API -----*/

    static void
vmq_rx_notify (vmq_rx* rx)
{
    vmq_link_t* link2 = container_of (rx, vmq_link_t, rx);

    if (link2->callbacks->receive_notify) {
	link2->callbacks->receive_notify (link2);
    }
}

    signed
vmq_msg_receive (vmq_link_t* link2, void** msg)
{
    return vmq_xx_msg_receive (&link2->rx.xx, msg);
}

    void
vmq_msg_free (vmq_link_t* link2, void* msg)
{
    vmq_xx_msg_free (&link2->rx.xx, msg, false);
}

    void
vmq_msg_return (vmq_link_t* link2, void* msg)
{
    vmq_xx_msg_free (&link2->rx.xx, msg, true);
}

    _Bool
vmq_data_offset_ok (vmq_link_t* link2, unsigned data_offset)
{
    return data_offset < vmq_xx_config_ls_total (&link2->rx.xx.config);
}

/*----- Link status changes -----*/

    /* Called from client. VMQ_LOCK not taken */

    static void
vmq_link_off_completed (vmq_link_t* link2)
{
	/*
	 *  This routine can be called following
	 *  init time changes in any channel. Do not
	 *  reset other side variables in this case.
	 */
    vmq_rx_vlink_off_completed (&link2->rx);
    vmq_tx_vlink_off_completed (&link2->tx);
    vmq_sysconf_trigger (link2->tx.xx.vlink->s_id);
}

struct vmq_links_t {
    vmq_links_public_t		public2;	/* Must be first */
    struct list_head		links;
    spinlock_t			spinlock;
    const vmq_callbacks_t*	callbacks;
    NkXIrqId			sysconf_id;
    char*			proc_name;
    struct proc_dir_entry*	proc;
};

    _Bool
vmq_links_iterate (vmq_links_t* links, _Bool (*func)(vmq_link_t*, void*),
		    void* cookie)
{
    vmq_link_t* link2;

    list_for_each_entry_type (link2, vmq_link_t, &links->links, link) {
	if (func (link2, cookie)) return true;
    }
    return false;
}

    /* Only called from vmq_links_abort() */

    static _Bool
vmq_link_abort_tx (vmq_link_t* link2, void* cookie)
{
    return vmq_tx_abort (&link2->tx, cookie);
}

    void
vmq_links_abort (vmq_links_t* links)
{
    DTRACE ("\n");
    vmq_links_iterate (links, vmq_link_abort_tx, NULL);
}

    static _Bool
vmq_link_on (vmq_link_t* link2)
{
    return vmq_xx_vlink_on (&link2->rx.xx) &&
	   vmq_xx_vlink_on (&link2->tx.xx);
}

    /* BASE: Only called from vmq_links_sysconf() */

    static _Bool
vmq_link_sysconf (vmq_link_t* link2, void* cookie)
{
    _Bool		completed = false;
    unsigned long	flags;

    (void) cookie;
    VMQ_LOCK (link2->tx.xx.spinlock, flags);
    if (link2->tx.xx.aborted || link2->rx.xx.aborted) {
	if (vmq_xx_all_returned (&link2->tx.xx) &&
	    vmq_xx_all_returned (&link2->rx.xx)) {
	    vmq_link_off_completed (link2);
	    completed = true;
	}
    }
    VMQ_UNLOCK (link2->tx.xx.spinlock, flags);
    if (completed) {
	if (link2->callbacks->link_off_completed) {
	    link2->callbacks->link_off_completed (link2);
	}
    }
    if (vmq_link_on (link2)) {
	if (link2->is_on != 1) {		/* -1 or 0 */
	    link2->is_on = 1;
	    if (link2->callbacks->link_on) {
		link2->callbacks->link_on (link2);
	    }
	}
    } else {
	if (link2->is_on != 0) {		/* -1 or 1 */
	    link2->is_on = 0;
	    if (link2->callbacks->link_off) {
		link2->callbacks->link_off (link2);
	    }
	}
    }
    return false;
}

    /* BASE: Called from client following sysconf_notify() */

    void
vmq_links_sysconf (vmq_links_t* links)
{
    DTRACE ("%p\n", links);
    vmq_links_iterate (links, vmq_link_sysconf, NULL);
}

    /* INTR: Only called from vmq_sysconf_hdl() */

    static _Bool
vmq_link_sysconf_hdl (vmq_link_t* link2, void* cookie)
{
    unsigned changed = vmq_rx_handshake (&link2->rx);

    (void) cookie;
    changed |= vmq_tx_handshake (&link2->tx);
    if (changed) {
	vmq_sysconf_trigger (link2->tx.xx.vlink->s_id);
    }
    return false;
}

    static void
vmq_tx_sysconf_notify (vmq_xx* xx)
{
    vmq_link_t* link2 = container_of (xx, vmq_link_t, tx.xx);

    link2->callbacks->sysconf_notify (link2->links);
}

    static void
vmq_rx_sysconf_notify (vmq_xx* xx)
{
    vmq_link_t* link2 = container_of (xx, vmq_link_t, rx.xx);

    link2->callbacks->sysconf_notify (link2->links);
}

    /* xirq handler */

    static void
vmq_sysconf_hdl (void* cookie, NkXIrq xirq)
{
    vmq_links_t* links = (vmq_links_t*) cookie;

    (void) xirq;
    DTRACE ("cookie %p xirq %d\n", cookie, xirq);
    vmq_links_iterate (links, vmq_link_sysconf_hdl, NULL);
    links->callbacks->sysconf_notify (links);
    DTRACE ("finished\n");
}

/*----- Support for /proc/nk/vmq.<vlink_name> -----*/

    static void
vmq_proc_xx (struct seq_file* seq, const char* name, const vmq_xx* xx)
{
    seq_printf (seq,
		"%s: %6x %4x %6x %4x %2d %2d %2d %3d %3d %5u %5u %3u %3u %3u\n",
		name,
		xx->config.msg_count,  xx->config.msg_max,
		xx->config.data_count, xx->config.data_max,
		xx->aborted, xx->ss_checked_out, xx->ls_checked_out,
		xx->local_xirq, xx->peer_xirq,
		xx->local_xirqs_received, xx->peer_xirqs_sent,
		xx->waits, xx->out_of_msgs, xx->self_xirqs);
}

    static int
vmq_proc_show (struct seq_file* seq, void* v)
{
    const vmq_links_t* links = (const vmq_links_t*) seq->private;
    vmq_link_t* link2;

    (void) v;
    list_for_each_entry_type (link2, vmq_link_t, &links->links, link) {
	seq_printf (seq, "     Loc Rem DataMax MsgMax Info\n");
	seq_printf (seq, "Pub: %3d %3d %7x %6x %s|%s\n",
		    link2->public2.local_osid, link2->public2.peer_osid,
		    link2->public2.data_max, link2->public2.msg_max,
		    link2->public2.rx_s_info ?
		    link2->public2.rx_s_info : "",
		    link2->public2.tx_s_info ?
		    link2->public2.tx_s_info : "");
	seq_printf (seq,
		    "    MCount MMax DCount DMax Ab MO DO  XL  XP XRecv "
		    "XSent Wai OOM SXI\n");
	vmq_proc_xx (seq, "TX", &link2->tx.xx);
	vmq_proc_xx (seq, "RX", &link2->rx.xx);
    }
    return 0;
}

    static int
vmq_proc_open (struct inode* inode, struct file* file)
{
    return single_open (file, vmq_proc_show, PDE (inode)->data);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,15)
static struct file_operations vmq_proc_fops =
#else
static const struct file_operations vmq_proc_fops =
#endif
{
    .owner	= THIS_MODULE,
    .open	= vmq_proc_open,
    .read	= seq_read,
    .llseek	= seq_lseek,
    .release	= single_release,
};

    static void
vmq_proc_init (vmq_links_t* links, const char* vlink_name,
	       const _Bool is_frontend)
{
    const size_t len = sizeof "nk/vmq.-fe" + strlen (vlink_name);

    if (list_empty (&links->links)) return;
    links->proc_name = kmalloc (len, GFP_KERNEL);
    if (!links->proc_name) return;
    snprintf (links->proc_name, len, "nk/vmq.%s-%ce", vlink_name,
	      is_frontend ? 'f' : 'b');
    links->proc = create_proc_entry (links->proc_name, 0, NULL);
    if (!links->proc) {
	WTRACE ("Could not create /proc/%s\n", links->proc_name);
	return;
    }
    links->proc->proc_fops  = &vmq_proc_fops;
    links->proc->data       = links;
}

    static void
vmq_proc_exit (vmq_links_t* links)
{
    if (links->proc) {
	remove_proc_entry (links->proc_name, NULL);
    }
    kfree (links->proc_name);
}

/*----- Initialization and shutdown code -----*/

    /* Only called by vmq_init_links() */

    static NkDevVlink*
vmq_find_pair_vlink (NkDevVlink* l, const char* vlink_name)
{
    NkPhAddr    plink = 0;

    DTRACE ("\n");
    while ((plink = nkops.nk_vlink_lookup (vlink_name, plink)) != 0) {
	NkDevVlink* vlink = (NkDevVlink*) nkops.nk_ptov (plink);

	if ((vlink != l) &&
	    (vlink->s_id == l->c_id) &&
	    (vlink->c_id == l->s_id) &&
	    (vlink->link == l->link)) {
	    return vlink;
	}
    }
    return NULL;
}

    /* Only called by vmq_vlink_in_use() */

    static _Bool
vmq_link_match (vmq_link_t* link2, void* cookie)
{
    DTRACE ("rx link %d %s use\n", link2->rx.xx.vlink->link,
	    link2->rx.xx.vlink->link == *(int*) cookie ? "in" : "not in");
    return link2->rx.xx.vlink->link == *(int*) cookie;
}

    /* Only called by vmq_init_links() */

    static _Bool
vmq_vlink_in_use (vmq_links_t* links, NkDevVlink* vlink)
{
    DTRACE ("\n");
    return vmq_links_iterate (links, vmq_link_match, &vlink->link);
}

    /* Only called by vmq_init_links() */

    static signed
vmq_link_create (vmq_links_t* links, NkDevVlink* rx_vlink,
		 NkDevVlink* tx_vlink, const vmq_xx_config_t* tx_config,
		 const vmq_xx_config_t* rx_config)
{
    vmq_link_t*		link2;
    signed		diag;

    DTRACE ("\n");
    link2 = (vmq_link_t*) kzalloc (sizeof *link2, GFP_KERNEL);
    if (!link2) {
	ETRACE ("Could not allocate memory for vmq link.\n");
	return -ENOMEM;
    }
    if (rx_vlink->s_info) {
	link2->public2.rx_s_info = (char*) nkops.nk_ptov (rx_vlink->s_info);
	DTRACE ("rx_s_info '%s'\n", link2->public2.rx_s_info);
    }
    if (tx_vlink->s_info) {
	link2->public2.tx_s_info = (char*) nkops.nk_ptov (tx_vlink->s_info);
	DTRACE ("tx_s_info '%s'\n", link2->public2.tx_s_info);
    }
    if (!tx_config) {
	DTRACE ("no static tx config\n");
	if (!links->callbacks->get_tx_config) {
	    ETRACE ("No tx_config and no get_tx_config callback provided\n");
	    return -EINVAL;
	}
	tx_config = links->callbacks->get_tx_config (link2,
						     link2->public2.tx_s_info);
	if (!tx_config) {
	    DTRACE ("get_tx_config callback failed\n");
	    kfree (link2);
	    return -EINVAL;
	}
	if (tx_config == VMQ_XX_CONFIG_IGNORE_VLINK) {
	    DTRACE ("ignoring tx vlink %d as requested\n", tx_vlink->link);
	    kfree (link2);
	    return 0;
	}
	DTRACE ("using dynamic tx config\n");
    }
    if (!rx_config) {
	DTRACE ("no static rx config\n");
	if (!links->callbacks->get_rx_config) {
	    ETRACE ("No rx_config and no get_rx_config callback provided\n");
	    return -EINVAL;
	}
	rx_config = links->callbacks->get_rx_config (link2,
						     link2->public2.rx_s_info);
	if (!rx_config) {
	    DTRACE ("get_rx_config callback failed\n");
	    kfree (link2);
	    return -EINVAL;
	}
	if (rx_config == VMQ_XX_CONFIG_IGNORE_VLINK) {
	    DTRACE ("ignoring rx vlink %d as requested\n", rx_vlink->link);
	    kfree (link2);
	    return 0;
	}
	DTRACE ("using dynamic rx config\n");
    }
	/* Callback can be called immediately */
	/* Update: no more, since the intro of vmq_links_start() */
    link2->callbacks = links->callbacks;
    link2->links     = links;
    link2->is_on     = -1;	/* Forces link_on/off callback calling */

    diag = vmq_tx_init (&link2->tx, tx_vlink, tx_config, &links->spinlock);
    if (diag) {		/* Error message already issued */
	kfree (link2);
	return diag;
    }
    diag = vmq_rx_init (&link2->rx, rx_vlink, rx_config, &links->spinlock);
    if (diag) {		/* Error message already issued */
	vmq_tx_finish (&link2->tx);
	kfree (link2);
	return diag;
    }
	/*
	 * vdev=(...,linkid|....s_info...)
	 */
    link2->public2.local_osid    = link2->rx.xx.vlink->s_id;
    link2->public2.peer_osid     = link2->tx.xx.vlink->s_id;
    link2->public2.rx_data_area  = link2->rx.xx.ls_area;
    link2->public2.tx_data_area  = link2->tx.xx.ls_area;
    link2->public2.data_max      = link2->tx.xx.config.data_max;
    link2->public2.msg_max       = link2->tx.xx.config.msg_max;
    link2->public2.ptx_data_area = vmq_xx_pls_area (&link2->tx.xx);

    list_add (&link2->link, &links->links);
    return 0;
}

    /* Only called externally */

    signed
vmq_links_init_ex (vmq_links_t** result, const char* vlink_name,
		   const vmq_callbacks_t* callbacks,
		   const vmq_xx_config_t* tx_config,
		   const vmq_xx_config_t* rx_config, void* priv,
		   _Bool is_frontend)
{
    const NkOsId myid = nkops.nk_id_get();
    vmq_links_t* links;
    NkPhAddr plink = 0;
    signed diag;

    DTRACE ("\n");
    *result = NULL;	/* Make sure it is NULL on error */
    if (!callbacks->sysconf_notify) {
	ETRACE ("The sysconf notify callback is mandatory.\n");
	return -EINVAL;
    }
    links = (vmq_links_t*) kzalloc (sizeof *links, GFP_KERNEL);
    if (!links) {
	ETRACE ("Could not allocate vmtd_links descriptor.\n");
	diag = -ENOMEM;
	goto error;
    }
    links->public2.priv = priv;
    INIT_LIST_HEAD (&links->links);
    spin_lock_init(&links->spinlock);
    links->callbacks = callbacks;
    while ((plink = nkops.nk_vlink_lookup (vlink_name, plink)) != 0) {
	NkDevVlink* rx_vlink = (NkDevVlink*) nkops.nk_ptov (plink);

	DTRACE ("rx_vlink with tag %d s_id %d c_id %d\n",
		rx_vlink->link, rx_vlink->s_id, rx_vlink->c_id);
	if (rx_vlink->s_id == myid && !vmq_vlink_in_use (links, rx_vlink)) {
	    NkDevVlink* tx_vlink = vmq_find_pair_vlink (rx_vlink, vlink_name);

	    if (tx_vlink) {
		DTRACE ("rx_plink %x rx_vlink %p tx_vlink %p\n",
			plink, rx_vlink, tx_vlink);
		DTRACE ("tx_vlink with tag %d s_id %d c_id %d\n",
			tx_vlink->link, tx_vlink->s_id, tx_vlink->c_id);
		if (is_frontend && rx_vlink->s_id == rx_vlink->c_id) {
		    DTRACE ("reversed link create\n");
		    diag = vmq_link_create (links, tx_vlink, rx_vlink,
					    tx_config, rx_config);
		} else {
		    DTRACE ("direct link create\n");
		    diag = vmq_link_create (links, rx_vlink, tx_vlink,
					    tx_config, rx_config);
		}
		if (diag) {	/* Error message already issued */
		    goto error;
		}
	    }
	}
    }
    *result = links;
    vmq_proc_init (links, vlink_name, is_frontend);
    return 0;

error:
    vmq_links_finish (links);
    return diag;
}

    static _Bool
vmq_link_start (vmq_link_t* link2, void* cookie)
{
    signed diag;

    DTRACE ("\n");
    diag = vmq_xx_start (&link2->tx.xx, vmq_tx_hdl);
    if (diag) {		/* Error message already issued */
	*(signed*) cookie = diag;
	return true;	/* Interrupt iteration */
    }
    diag = vmq_xx_start (&link2->rx.xx, vmq_rx_hdl);
    if (diag) {		/* Error message already issued */
	*(signed*) cookie = diag;
	return true;	/* Interrupt iteration */
    }
    return false;
}

    signed
vmq_links_start (vmq_links_t* links)
{
    signed diag;

    DTRACE ("\n");
    if (vmq_links_iterate (links, vmq_link_start, &diag)) return diag;
    links->sysconf_id = nkops.nk_xirq_attach (NK_XIRQ_SYSCONF,
					      vmq_sysconf_hdl, links);
    if (!links->sysconf_id) {
	ETRACE ("Cannot attach sysconf handler\n");
	return -EAGAIN;
    }
	/*
	 *  Trigger a sysconf to ourselves rather than simply
	 *  calling "vmq_sysconf_hdl(links,0);" because this
	 *  way we get the proper environment in the handler
	 *  and avoid concurrency with the real handler.
	 */
    vmq_sysconf_trigger (nkops.nk_id_get());
    return 0;
}

    /* Only called by vmq_links_finish() */

    static _Bool
vmq_link_destroy (vmq_link_t* link2, void* cookie)
{
    (void) cookie;
    vmq_rx_finish (&link2->rx);
    vmq_tx_finish (&link2->tx);
    vmq_sysconf_trigger (link2->tx.xx.vlink->s_id);
    list_del (&link2->link);
    kfree (link2);
    return true;	/* Abort list scanning */
}

    /* Only called externally */

    void
vmq_links_finish (vmq_links_t* links)
{
    DTRACE ("\n");
    if (links) {
	vmq_proc_exit (links);
	if (links->sysconf_id) {
	    nkops.nk_xirq_detach (links->sysconf_id);
	}
	while (vmq_links_iterate (links, vmq_link_destroy, NULL)) {}
	kfree (links);
    }
}

/*----- Module description -----*/

EXPORT_SYMBOL (vmq_data_free);
EXPORT_SYMBOL (vmq_data_offset_ok);
EXPORT_SYMBOL (vmq_links_abort);
EXPORT_SYMBOL (vmq_links_finish);
EXPORT_SYMBOL (vmq_links_init_ex);
EXPORT_SYMBOL (vmq_links_iterate);
EXPORT_SYMBOL (vmq_links_start);
EXPORT_SYMBOL (vmq_links_sysconf);
EXPORT_SYMBOL (vmq_msg_allocate_ex);
EXPORT_SYMBOL (vmq_msg_free);
EXPORT_SYMBOL (vmq_msg_receive);
EXPORT_SYMBOL (vmq_msg_return);
EXPORT_SYMBOL (vmq_msg_send);
EXPORT_SYMBOL (vmq_msg_send_async);
EXPORT_SYMBOL (vmq_msg_send_flush);
EXPORT_SYMBOL (vmq_msg_slot);
EXPORT_SYMBOL (vmq_return_msg_free);
EXPORT_SYMBOL (vmq_return_msg_receive);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_DESCRIPTION ("VLX VMQ communications driver");

/*----- End of file -----*/


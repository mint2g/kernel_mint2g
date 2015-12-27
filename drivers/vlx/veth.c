/*
 ****************************************************************
 *
 *  Component: VLX virtual Ethernet driver
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
 *    Christophe Augier (christophe.augier@redbend.com)
 *    Pascal Piovesan (pascal.piovesan@redbend.com)
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

/*----- System header files -----*/

#include <linux/module.h>
#include <linux/init.h>
#include <asm/atomic.h>
#include <asm/bitops.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/rtnetlink.h>
#include <linux/skbuff.h>
#include <nk/nkern.h>

/*----- Local configuration -----*/

#if 0
#define VETH_DEBUG
#endif

#if 0
#define VETH_ODEBUG
#endif

#define VETH_MAX	4

    /*
     * These values have to be the same on both vlink sides.
     */
#define VETH_L1_CACHE_BYTES	L1_CACHE_BYTES
#ifdef CONFIG_NKERNEL_VETH_RING_SIZE
#define VETH_RING_SIZE	    CONFIG_NKERNEL_VETH_RING_SIZE
#else
#define VETH_RING_SIZE	    64
#endif

/*----- Tracing -----*/

#define VETH_CONT(x...)	printk (x)
#define VETH_BARE(x...)	printk (KERN_NOTICE                    x)
#define VETH_INFO(x...)	printk (KERN_NOTICE  "VETH: "          x)
#define VETH_ERR(x...)	printk (KERN_ERR     "VETH: Error: "   x)

#ifdef VETH_DEBUG
#define VETH_DTRACE(f,a...)	VETH_INFO ("%s: " f, __func__, ##a)
#define VETH_DTRACE_CONT(x...)	VETH_CONT (x)
#else
#define VETH_DTRACE(x...)
#define VETH_DTRACE_CONT(x...)
#endif

#ifdef VETH_ODEBUG
#define VETH_OTRACE(f,a...)	VETH_INFO ("%s: " f, __func__, ##a)
#else
#define VETH_OTRACE(f,a...)
#endif

/*----- Driver definitions -----*/

typedef struct net_device_stats veth_stats;

    /*
     * The communication relies on a data ring with VETH_RING_SIZE
     * slots. The ring descriptor and the data slots are all allocated
     * in the same shared memory segment. For performance, it is needed
     * that data copied in the ring be aligned on cache lines. Therefore
     * each slot, IP header and shared info structure are aligned.
     */
typedef struct {
    volatile nku32_f	p_idx;		/* producer index */
    volatile nku32_f	freed_idx;	/* freed slot index */
    volatile nku32_f	c_idx;		/* consumer index */
    volatile nku8_f	stopped;	/* states: started/stopped */
    nku16_f		size_unused;	/* size of ring (number of slots) */
} VEthRingDesc;

struct VEthLink;

typedef struct {
    nku32_f		len;
    struct VEthLink*	link_unused;
    void*		data;
} VEthSlotDesc;

#if VETH_RING_SIZE & (VETH_RING_SIZE-1)
#error VETH_RING_SIZE is not a power of 2
#endif

#define RING_ALIGN(x) \
	(((x) + (VETH_L1_CACHE_BYTES -1)) & ~(VETH_L1_CACHE_BYTES -1))
#define RING_INDEX_MASK		(VETH_RING_SIZE -1)

    /*
     * The skb_shared_info structure is located at the end of the skb data.
     * The size of this structure may vary, depending on the Linux version.
     * Because the SLOT_SIZE must always be the same, we cannot use
     * sizeof, and therefore a fixed value is given and a test is done
     * to verify that this value is great enough.
     */
#define SKB_SHINFO_SIZE     0x200

#define VETH_STATIC_ASSERT(x) \
    extern char veth_static_assert [(x) ? 1 : -1]

VETH_STATIC_ASSERT (sizeof (struct skb_shared_info) <= SKB_SHINFO_SIZE);

#define SLOT_HLEN_SIZE      RING_ALIGN (ETH_HLEN)
#define SLOT_HLEN_PAD       (SLOT_HLEN_SIZE - ETH_HLEN)
#define SLOT_DATA_SIZE      RING_ALIGN (ETH_HLEN + ETH_DATA_LEN + \
					SKB_SHINFO_SIZE)
#define DATA_SIZE           (VETH_RING_SIZE * (SLOT_HLEN_SIZE + SLOT_DATA_SIZE))

#define SLOT_DESC_SIZE	    RING_ALIGN (sizeof (VEthSlotDesc))
#define DESC_SIZE	    (VETH_RING_SIZE * SLOT_DESC_SIZE)

#define RING_DESC_SIZE	    RING_ALIGN (sizeof (VEthRingDesc))

#define PMEM_SIZE	    (RING_DESC_SIZE + DESC_SIZE + DATA_SIZE)

#define RING_P_ROOM(rng)     (VETH_RING_SIZE - ((rng)->p_idx - (rng)->freed_idx))
#define RING_IS_FULL(rng)    (((rng)->p_idx - (rng)->freed_idx) >= VETH_RING_SIZE)
#define RING_IS_EMPTY(rng)   ((rng)->p_idx == (rng)->freed_idx)
#define RING_C_ROOM(rng)     ((rng)->p_idx - (rng)->c_idx)

typedef struct {
    NkOsId	osid;
    NkXIrq	rx_xirq;	/* store rx xirq number */
    NkXIrqId	rx_xid;		/* rx xirq handler id */
    NkXIrq	tx_ready_xirq;	/* store tx_ready xirq number */
    NkXIrqId	tx_ready_xid;	/* tx_ready xirq handler id */
} VEthLocal;

typedef struct {
    NkOsId	osid;
    NkXIrq	rx_xirq;	/* xirq to send to peer OS */
    NkXIrq	tx_ready_xirq;	/* xirq to send to peer OS */
} VEthPeer;

struct VEth;

    /*
     * Local data structure for each
     * connection between this OS and
     * a peer OS.
     */
typedef struct VEthLink {
    NkDevVlink*   rx_link;	/* RX vlink */
    VEthRingDesc* rx_ring;	/* RX ring */

    NkDevVlink*   tx_link;	/* TX vlink */
    VEthRingDesc* tx_ring;	/* TX ring */

    struct VEth*  veth;
    VEthLocal     local;
    VEthPeer      peer;

    _Bool         enabled;
} VEthLink;

    /*
     * Device instance data.
     */
typedef struct VEth {
    veth_stats		stats;	/* net statistics     */
    VEthLink		link;	/* link with peer OS data */
    struct net_device*	netdev;	/* Linux net device   */
} VEth;

static VEth*		veth_devices [VETH_MAX];
static unsigned int	veth_devices_num;
static NkXIrqId		veth_sysconf_id;

#define VETH_PMEM_ID	4
#define VETH_RXIRQ_ID	6
#define VETH_TXIRQ_ID	7

/*----- Data transfer -----*/

    /*
     * Helper functions to push/pull data in rx or tx rings.
     */

    static int
veth_ring_push_data (VEthRingDesc* ring, const nku8_f* src,
		     const unsigned int len)
{
    const int		tmp = ring->p_idx - ring->c_idx;
    VEthSlotDesc*	sd;
    nku8_f*		dst;

    if ((unsigned) tmp > VETH_RING_SIZE) {
	VETH_ERR ("tx ring corrupted\n");
	return -EINVAL;
    }
    dst = (nku8_f*) ring + RING_DESC_SIZE;
    dst += (ring->p_idx & RING_INDEX_MASK) * SLOT_DESC_SIZE;

    sd = (VEthSlotDesc*) dst;
    sd->len = len;
    VETH_OTRACE ("%p -> %p\n", src, sd->data);
    memcpy ((char*) sd->data + SLOT_HLEN_PAD, src, len);

    ring->p_idx++;
    return 0;
}

#ifdef CONFIG_SKB_DESTRUCTOR
    /*
     * This code needs a modification in Linux sk_buff management.
     * Files net/core/skbuff.c and include/linux/skbuff.h are modified
     * to provide a destructor handler specific to the data buffer in
     * each skb.
     * Thanks to this feature, it is possible to avoid a second copy
     * when receiving data. Instead, we link the buffer from the shared
     * memory to the skb structure and when the last skb referencing
     * this buffer is freed, we can free the buffer.
     */
    static void
veth_free_buffer (void* data, VEthLink* link)
{
    VEthSlotDesc* sd;
    VEthRingDesc* rx_ring = link->rx_ring;
    nku8_f*	  sptr;

    VETH_OTRACE ("%d %d %d\n",
		 rx_ring->p_idx, rx_ring->freed_idx, rx_ring->stopped);

    sptr  = (nku8_f*) rx_ring + RING_DESC_SIZE;
    sptr += (rx_ring->freed_idx & RING_INDEX_MASK) * SLOT_DESC_SIZE;
    sd = (VEthSlotDesc*) sptr;
    sd->data = data;

    rx_ring->freed_idx++;
    if (rx_ring->stopped) {
	nkops.nk_xirq_trigger (link->peer.tx_ready_xirq, link->peer.osid);
    }
}

    static void
veth_free_skb (struct skb_shared_info* shinfo, void* cookie)
{
    veth_free_buffer (skb_shinfo_to_head (shinfo), cookie);
}

    /*
     * Allocate an sk_buff structure for use in rx handler. This
     * function is partly copied from alloc_skb in net/core/skbuff.c
     */

    static inline struct sk_buff*
veth_alloc_skb (VEthLink* link)
{
    const int		tmp = link->rx_ring->p_idx - link->rx_ring->c_idx;
    struct skb_shared_info* shinfo;
    struct sk_buff*	skb;
    nku32_f		size;
    VEthSlotDesc*	sd;
    nku8_f*		src;

    if ((unsigned) tmp > VETH_RING_SIZE) {
	VETH_ERR ("rx ring corrupted\n");
	return NULL;
    }
    skb = ___alloc_skb (GFP_ATOMIC, -1);
    if (!skb) return NULL;

    memset (skb, 0, offsetof (struct sk_buff, tail));
    atomic_set (&skb->users, 1);

    src = (nku8_f*) link->rx_ring + RING_DESC_SIZE;
    src += (link->rx_ring->c_idx & RING_INDEX_MASK) * SLOT_DESC_SIZE;
    link->rx_ring->c_idx++;
    sd = (VEthSlotDesc*) src;
	/*
	 * Get size and save link address and index for later
	 * use in veth_free_skb. No need to check size coherency
	 * as it will later be done in skb_put().
	 */
    size      = sd->len;

    skb->len  = 0;
    skb->head = sd->data;
    skb->data = sd->data;
    VETH_OTRACE ("%p\n", sd->data);
    skb_reset_tail_pointer (skb);
    skb->end  = skb->tail + RING_ALIGN (SLOT_HLEN_PAD + size);
    skb->truesize = sizeof (struct sk_buff) + RING_ALIGN (SLOT_HLEN_PAD + size);

    shinfo		= skb_shinfo (skb);
    atomic_set (&shinfo->dataref, 1);
    shinfo->nr_frags    = 0;
    shinfo->gso_size    = 0;
    shinfo->gso_segs    = 0;
    shinfo->gso_type    = 0;
    shinfo->ip6_frag_id = 0;
    shinfo->frag_list   = NULL;
    shinfo->destructor  = veth_free_skb;
    shinfo->cookie      = link;
    shinfo->orig	= NULL;
    shinfo->len		= skb_end_pointer (skb) - skb->head;

    skb_reserve (skb, SLOT_HLEN_PAD);
    skb_put (skb, size);
    return skb;
}
#else	/* not CONFIG_SKB_DESTRUCTOR */

    /* Returned length comprises the 14 byte frame header */

    static int
veth_ring_pull_data (VEthRingDesc* ring, nku8_f* dst)
{
    const int		tmp = ring->p_idx - ring->c_idx;
    VEthSlotDesc*	sd;
    nku8_f*		src;
    unsigned int	len;

    if ((unsigned) tmp > VETH_RING_SIZE) {
	VETH_ERR ("rx ring corrupted\n");
	return -EINVAL;
    }
    src = (nku8_f*) ring + RING_DESC_SIZE;
    src += (ring->c_idx & RING_INDEX_MASK) * SLOT_DESC_SIZE;

    sd = (VEthSlotDesc*) src;
    len = sd->len;
    VETH_OTRACE ("%p <- %p\n", dst, sd->data);
    memcpy (dst, (char*) sd->data + SLOT_HLEN_PAD, len);

    ring->c_idx++;
    ring->freed_idx++;

    return len;
}
#endif	/* not CONFIG_SKB_DESTRUCTOR */

    /*
     * Rx xirq handler to receive frames as a rx_ring consumer
     */

    static void
veth_rx_xirq (void* cookie, NkXIrq xirq)
{
    VEthLink*		link = (VEthLink*) cookie;
    VEth*		veth = link->veth;
    struct net_device*	netdev  = veth->netdev;
    VEthRingDesc*	rx_ring = link->rx_ring;
    struct sk_buff*	skb;
#ifndef CONFIG_SKB_DESTRUCTOR
    int			len;
#endif

    while (RING_C_ROOM (rx_ring) > 0) {
	    /*
	     * Check the peer state and account
	     * error if it is not ON.
	     */
	if (link->rx_link->c_state != NK_DEV_VLINK_ON) {
	    VETH_DTRACE ("peer driver not ready\n");
	    netif_carrier_off (veth->netdev);
	    veth->stats.rx_errors++;
	    return;
	}
#ifdef CONFIG_SKB_DESTRUCTOR
	skb  = veth_alloc_skb (link);
	if (!skb) {
	    nku8_f* src = (nku8_f*) rx_ring + RING_DESC_SIZE;

	    src += (rx_ring->c_idx & RING_INDEX_MASK) * SLOT_DESC_SIZE;
	    rx_ring->c_idx++;
	    veth_free_buffer (src, link);
	    veth->stats.rx_dropped++;
	    continue;
	}
#else
	skb = dev_alloc_skb (ETH_FRAME_LEN); // over allocate (1514)
	if (!skb) {
	    rx_ring->c_idx++;
	    rx_ring->freed_idx++;
	    veth->stats.rx_dropped++;
	    continue;
	}
	len = veth_ring_pull_data (rx_ring, skb->data);
	if (len < 0) {
		/* Error message already issued */
	    dev_kfree_skb_any (skb);
	    rx_ring->c_idx++;
	    rx_ring->freed_idx++;
	    veth->stats.rx_errors++;
	    continue;
	}
	skb_put (skb, len);
#endif
	skb->dev       = veth->netdev;
	skb->protocol  = eth_type_trans (skb, veth->netdev);
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	veth->stats.rx_packets++;
	veth->stats.rx_bytes += skb->len;

	netif_rx (skb);
    }
    netdev->last_rx = jiffies;

#ifndef CONFIG_SKB_DESTRUCTOR
	/* Send tx ready xirq if producer ring was stopped (full) */
    if (rx_ring->stopped) {
	nkops.nk_xirq_trigger (link->peer.tx_ready_xirq, link->peer.osid);
    }
#endif
}

    /*
     * Tx_ready xirq handler.
     */

    static void
veth_tx_ready_xirq (void* cookie, NkXIrq xirq)
{
    VEthLink*		link    = (VEthLink*) cookie;
    VEth*		veth    = link->veth;
    VEthRingDesc*	tx_ring = link->tx_ring;

    (void) xirq;
    if (link->tx_link->c_state != NK_DEV_VLINK_ON) {
	VETH_OTRACE ("Ignoring xirq %d from %d because vlink not On\n",
		     xirq, link->tx_link->s_id);
	return;
    }
    if (tx_ring->stopped && !RING_IS_FULL (tx_ring)) {
	tx_ring->stopped = 0;
	netif_wake_queue (veth->netdev);
    }
}

    static _Bool
veth_link_is_up (const VEthLink* link)
{
    return link->rx_link->c_state == NK_DEV_VLINK_ON &&
	   link->rx_link->s_state == NK_DEV_VLINK_ON &&
	   link->tx_link->c_state == NK_DEV_VLINK_ON &&
	   link->tx_link->s_state == NK_DEV_VLINK_ON;
}

    static void
veth_link_state_notify (const VEthLink* link)
{
    VEth* veth = link->veth;

    VETH_DTRACE ("is_up %d carrier_ok %d\n",
		 veth_link_is_up (link), netif_carrier_ok (veth->netdev));
    if (veth_link_is_up (link)) {
        if (!netif_carrier_ok (veth->netdev)) {
	    VETH_BARE ("%s: link on (OS#%d <-> OS#%d).\n", veth->netdev->name,
		       link->local.osid, link->peer.osid);
	    netif_carrier_on (veth->netdev);
        }
    } else {
        if (netif_carrier_ok (veth->netdev)) {
	    VETH_BARE ("%s: link off (OS#%d <-> OS#%d).\n", veth->netdev->name,
		       link->local.osid, link->peer.osid);
	    netif_carrier_off (veth->netdev);
	}
    }
}

    /*
     * Send sysconf xirq to guest.
     */
    static inline void
veth_sysconf_trigger (const NkOsId osid)
{
    VETH_DTRACE ("osid %d\n", osid);
    nkops.nk_xirq_trigger (NK_XIRQ_SYSCONF, osid);
}

/*----- Public functions -----*/

    static int
veth_ndo_open (struct net_device* dev)
{
    VEth* veth = netdev_priv (dev);

    VETH_DTRACE ("%s\n", dev->name);
	/* Reset stats */
    memset (&veth->stats, 0, sizeof veth->stats);
    netif_start_queue (dev);
    return 0;
}

    static int
veth_ndo_close (struct net_device* dev)
{
    VETH_DTRACE ("%s\n", dev->name);
    netif_stop_queue (dev);
    return 0;
}

    static int
veth_ndo_start_xmit (struct sk_buff* skb, struct net_device* dev)
{
    VEth*         veth    = netdev_priv (dev);
    VEthLink*     link    = &veth->link;
    VEthRingDesc* tx_ring = link->tx_ring;

    VETH_OTRACE ("%s\n", dev->name);
	/*
	 * Peer OS Link is not ready, set link down
	 * and account error.
	 */
    if (link->tx_link->s_state != NK_DEV_VLINK_ON) {
	VETH_DTRACE ("%s: xmit, peer %d not ready\n",
		     dev->name, link->peer.osid);
	netif_carrier_off (dev);
	veth->stats.tx_carrier_errors++;
	dev_kfree_skb_any (skb);
	return NETDEV_TX_OK;
    }
	/*
	 * Interface is overrunning.
	 */
    if (RING_IS_FULL (tx_ring)) {
	tx_ring->stopped = 1;
	netif_stop_queue (dev);
	veth->stats.tx_fifo_errors++;
	dev_kfree_skb_any (skb);
	return NETDEV_TX_BUSY;
    }
	/*
	 * Everything is OK, start xmit.
	 */
    if (veth_ring_push_data (tx_ring, skb->data, skb->len)) {
	netif_stop_queue (dev);
	veth->stats.tx_fifo_errors++;
	dev_kfree_skb_any (skb);
	return NETDEV_TX_BUSY;
    }
	/*
	 * Statistics.
	 */
    veth->stats.tx_bytes += skb->len;
    veth->stats.tx_packets++;
    dev->trans_start = jiffies;

    dev_kfree_skb_any (skb);
	/*
	 * Ring is full, stop interface and avoid dropping packets
	 */
    if (RING_IS_FULL (tx_ring)) {
	tx_ring->stopped = 1;
	netif_stop_queue (dev);
    }
    nkops.nk_xirq_trigger (link->peer.rx_xirq, link->peer.osid);
    return NETDEV_TX_OK;
}

    static struct net_device_stats*
veth_ndo_get_stats (struct net_device* dev)
{
    VEth* veth = netdev_priv (dev);

    VETH_DTRACE ("%s\n", dev->name);
    return &(veth->stats);
}

    static void
veth_ndo_set_multicast_list (struct net_device* dev)
{
    VETH_DTRACE ("%s\n", dev->name);
}

    /* TX timeout, wake queue up and account error */

    static void
veth_ndo_tx_timeout (struct net_device* dev)
{
    VEth*         veth    = netdev_priv (dev);
    VEthLink*     link    = &veth->link;
    VEthRingDesc* tx_ring = link->tx_ring;

    VETH_DTRACE ("%s\n", dev->name);
    veth->stats.tx_errors++;
	/*
	 * If ring is full tell peer OS there is something
	 * to consume. Otherwise, wake up interface.
	 */
    if (RING_IS_FULL (tx_ring)) {
	nkops.nk_xirq_trigger (link->peer.rx_xirq, link->peer.osid);
    } else {
	tx_ring->stopped = 0;
        netif_wake_queue (dev);
    }
}

/*----- Initialization and reconfiguration -----*/

    /*
     * veth_link initialization is done in 4 steps:
     * 1) Find the corresponding vlink (vlink where we are client).
     * 2) Allocate a communication ring per vlink.
     * 3) Allocate and attach xirqs.
     * 4) Send sysconf xirq to peer OS to start handshake
     *    (handshake is continued in sysconf handler).
     */
    static void
veth_link_reset_rx (VEthLink* link)
{
    link->rx_ring->c_idx     = 0;
    link->rx_ring->freed_idx = 0;
}

    static void
veth_link_reset_tx (VEthLink* link)
{
    link->tx_ring->p_idx   = 0;
    link->tx_ring->stopped = 0;
}

    /*
     * Handshake function to update link states.
     */
    static _Bool
veth_handshake_rx (VEthLink* link)
{
    volatile int* my_state   = &link->rx_link->s_state;
    const int     peer_state =  link->rx_link->c_state;
    _Bool         need_sysconf = 0;

    VETH_DTRACE ("%d+%d => ", peer_state, *my_state);

    switch (*my_state) {
    case NK_DEV_VLINK_OFF:
	if (peer_state != NK_DEV_VLINK_ON) {
	    veth_link_reset_rx (link);
	    *my_state = NK_DEV_VLINK_RESET;
	    need_sysconf = 1;
	}
	break;

    case NK_DEV_VLINK_RESET:
	if (peer_state != NK_DEV_VLINK_OFF) {
	    *my_state = NK_DEV_VLINK_ON;
	    need_sysconf = 1;
	}
	break;

    case NK_DEV_VLINK_ON:
	if (peer_state == NK_DEV_VLINK_OFF) {
	    veth_link_reset_rx (link);
	    *my_state = NK_DEV_VLINK_RESET;
	    need_sysconf = 1;
	}
	break;
    }
    VETH_DTRACE_CONT ("%d+%d\n", link->rx_link->c_state,
				 link->rx_link->s_state);
    return need_sysconf;
}

    static _Bool
veth_handshake_tx (VEthLink* link)
{
    volatile int* my_state   = &link->tx_link->c_state;
    const int     peer_state =  link->tx_link->s_state;
    _Bool         need_sysconf = 0;

    VETH_DTRACE ("%d+%d => ", *my_state, peer_state);

    switch (*my_state) {
    case NK_DEV_VLINK_OFF:
	if (peer_state != NK_DEV_VLINK_ON) {
	    veth_link_reset_tx (link);
	    *my_state = NK_DEV_VLINK_RESET;
	    need_sysconf = 1;
	}
	break;

    case NK_DEV_VLINK_RESET:
	if (peer_state != NK_DEV_VLINK_OFF) {
	    *my_state = NK_DEV_VLINK_ON;
	    need_sysconf = 1;
	}
	break;

    case NK_DEV_VLINK_ON:
	if (peer_state == NK_DEV_VLINK_OFF) {
	    veth_link_reset_tx (link);
	    *my_state = NK_DEV_VLINK_RESET;
	    need_sysconf = 1;
	}
	break;
    }
    VETH_DTRACE_CONT ("%d+%d\n", link->tx_link->c_state,
				 link->tx_link->s_state);
    return need_sysconf;
}

    /*
     * Sysconf xirq handler. This function triggers handshakes for each link.
     */
    static void
veth_sysconf_xirq (void* cookie, NkXIrq xirq)
{
    unsigned int minor;

    (void) cookie;
    (void) xirq;
	/* Start handshake or change server states according to peer states */
    for (minor = 0; minor < veth_devices_num; minor++) {
	VEth*     veth         = veth_devices [minor];
	VEthLink* link         = &veth->link;
	int	  need_sysconf = 0;

	if (link->enabled) {
	    need_sysconf  = veth_handshake_rx (link);
	    need_sysconf |= veth_handshake_tx (link);

	    if (need_sysconf) {
		veth_sysconf_trigger (link->peer.osid);
	    }
		/*
		 *  We need to notify the new state always, and not
		 *  only if a sysconf was just sent, because if the
		 *  peer goes On second, then the transition from
		 *  link-off to link-on will not be preceeded with
		 *  a sysconf sending.
		 */
	    veth_link_state_notify (link);
	}
    }
}

    static void
veth_rx_ring_data_init (VEthRingDesc* ring)
{
    nku8_f*	sptr = (nku8_f*) ring + RING_DESC_SIZE;
    nku8_f*	dptr = sptr + DESC_SIZE;
    unsigned	i;

    for (i = 0; i < VETH_RING_SIZE; i++) {
	VEthSlotDesc* sd = (VEthSlotDesc*) sptr;

	sd->data = dptr;
	sptr += SLOT_DESC_SIZE;
	dptr += SLOT_DATA_SIZE;
    }
}

    /*
     * Allocate communication rings (ring, shared memory, xirq).
     * Communication rings may have already been allocated by the
     * peer OS, but this is managed transparently by NKDDI.
     */
    static int
veth_alloc_link_resources (VEthLink* link)
{
    NkDevVlink*		rx_link = link->rx_link;
    NkDevVlink*		tx_link = link->tx_link;
    VEthRingDesc*	rx_ring;
    VEthRingDesc*	tx_ring;
    NkPhAddr		paddr;

	/*
	 * Allocate RX vlink resources.
	 */
	/* Allocate persistent shared memory */
    paddr = nkops.nk_pmem_alloc (nkops.nk_vtop (rx_link), VETH_PMEM_ID,
				 PMEM_SIZE);
    if (!paddr) {
	VETH_ERR ("OS#%d->OS#%d link=%d server pmem alloc failed (%d bytes).\n",
		  rx_link->c_id, rx_link->s_id, rx_link->link, PMEM_SIZE);
	return -ENOMEM;
    }
    rx_ring = (VEthRingDesc*) nkops.nk_mem_map (paddr, PMEM_SIZE);
    if (!rx_ring) {
	VETH_ERR ("cannot map pmem\n");
	return -ENOMEM;
    }
    rx_ring->p_idx = 0;
    veth_rx_ring_data_init (rx_ring);

    link->rx_ring    = rx_ring;
    link->local.osid = rx_link->s_id;

	/* Allocate local rx xirq */
    link->local.rx_xirq = nkops.nk_pxirq_alloc (nkops.nk_vtop (rx_link),
						VETH_RXIRQ_ID,
						link->local.osid, 1);
    if (!link->local.rx_xirq) {
	VETH_ERR ("OS#%d->OS#%d link=%d server pxirq alloc failed.\n",
		  rx_link->c_id, rx_link->s_id, rx_link->link);
	return -ENOMEM;
    }
	/* Allocate local tx_ready xirq */
    link->local.tx_ready_xirq = nkops.nk_pxirq_alloc (nkops.nk_vtop (tx_link),
						      VETH_TXIRQ_ID,
						      link->local.osid, 1);
    if (!link->local.tx_ready_xirq) {
	VETH_ERR ("OS#%d->OS#%d link=%d server pxirq alloc failed.\n",
		  rx_link->c_id, rx_link->s_id, rx_link->link);
	return -ENOMEM;
    }
	/*
	 * Allocate TX vlink resources.
	 */
	/* Allocate persistent shared memory */
    paddr = nkops.nk_pmem_alloc (nkops.nk_vtop (tx_link), VETH_PMEM_ID,
				 PMEM_SIZE);
    if (!paddr) {
	VETH_ERR ("OS#%d->OS#%d link=%d client pmem alloc failed (%d bytes).\n",
		  tx_link->c_id, tx_link->s_id, tx_link->link, PMEM_SIZE);
	return -ENOMEM;
    }
    tx_ring = (VEthRingDesc*) nkops.nk_mem_map (paddr, PMEM_SIZE);
    if (!tx_ring) {
	VETH_ERR ("cannot map pmem\n");
	return -ENOMEM;
    }
    tx_ring->c_idx     = 0;
    tx_ring->freed_idx = 0;

    link->tx_ring   = tx_ring;
    link->peer.osid = tx_link->s_id;

	/* Allocate peer rx xirq */
    link->peer.rx_xirq = nkops.nk_pxirq_alloc (nkops.nk_vtop (tx_link),
					       VETH_RXIRQ_ID,
					       link->peer.osid, 1);
    if (!link->peer.rx_xirq) {
	VETH_ERR ("OS#%d->OS#%d link=%d client pxirq alloc failed.\n",
		  tx_link->c_id, tx_link->s_id, tx_link->link);
	return -ENOMEM;
    }
	/* Allocate peer tx_ready xirq */
    link->peer.tx_ready_xirq = nkops.nk_pxirq_alloc (nkops.nk_vtop (rx_link),
						     VETH_TXIRQ_ID,
						     link->peer.osid, 1);
    if (!link->peer.tx_ready_xirq) {
	VETH_ERR ("OS#%d->OS#%d link=%d client pxirq alloc failed.\n",
		  rx_link->c_id, rx_link->s_id, rx_link->link);
	return -ENOMEM;
    }
    return 0;
}

    static int
veth_attach_handlers (VEthLink* link)
{
    NkDevVlink*	rx_link = link->rx_link;
    NkDevVlink*	tx_link = link->tx_link;

    (void) rx_link;
    (void) tx_link;
	/* Attach local rx_xirq handler */
    link->local.rx_xid = nkops.nk_xirq_attach (link->local.rx_xirq,
					       veth_rx_xirq, link);
    if (!link->local.rx_xid) {
	VETH_ERR ("OS#%d->OS#%d link=%d server cannot attach xirq handler.\n",
		  rx_link->c_id, rx_link->s_id, rx_link->link);
	return -ENOMEM;
    }
	/* Attach local tx_ready_xirq handler */
    link->local.tx_ready_xid = nkops.nk_xirq_attach (link->local.tx_ready_xirq,
						     veth_tx_ready_xirq,
						     link);
    if (!link->local.tx_ready_xid) {
	VETH_ERR ("OS#%d->OS#%d link=%d server cannot attach xirq handler.\n",
		  rx_link->c_id, rx_link->s_id, rx_link->link);
	return -ENOMEM;
    }
    return 0;
}

    /*
     * Parse MAC address in VLX command line:
     * vdev=(veth,linkid|xx:xx:xx:xx:xx:xx)
     */
    static int
veth_parse_mac_address (VEth* veth, NkDevVlink* vlink)
{
    char	tmp_addr[6];
    const char*	opt;
    char*	end;
    unsigned	i;
       /*
	* Set MAC address to default 00:00:00:00:link:osid
	*/
    veth->netdev->dev_addr[4] = vlink->link;
    veth->netdev->dev_addr[5] = nkops.nk_id_get();
	/*
	 * Parse vlink s_info field to find
	 * MAC address.
	 */
    if (!vlink->s_info) {
	VETH_DTRACE ("using default MAC address\n");
	return 0;
    }
    opt = (const char*) nkops.nk_ptov (vlink->s_info);
    for (i = 0 ; i < 5 ; i++) {
	tmp_addr[i] = simple_strtoul (opt, &end, 16);
	if ((end == opt) || (*end != ':')) {
	    VETH_ERR ("cannot parse MAC address: %s\n", end);
	    return -EINVAL;
	}
	opt = end + 1; /* skip colon */
    }
    tmp_addr[i] = simple_strtoul (opt, &end, 16);
    if (end == opt) {
	VETH_ERR ("cannot parse MAC address: %s\n", end);
	return -EINVAL;
    }
    VETH_BARE ("%s: setting MAC address to %02x:%02x:%02x:%02x:%02x:%02x.\n",
	       veth->netdev->name,
	       tmp_addr[0], tmp_addr[1], tmp_addr[2],
	       tmp_addr[3], tmp_addr[4], tmp_addr[5]);
    for (i = 0; i < 6; i++) {
	veth->netdev->dev_addr[i] = tmp_addr[i];
    }
    return 0;
}

    static void
veth_dev_free (VEth* veth)
{
    unregister_netdev (veth->netdev);
    free_netdev (veth->netdev);		/* This frees "veth" too */
}

#ifndef SET_MODULE_OWNER
#define SET_MODULE_OWNER(dev) do { } while (0)
#endif

#ifdef HAVE_NET_DEVICE_OPS
static const struct net_device_ops veth_netdev_ops = {
    .ndo_open			= veth_ndo_open,
    .ndo_stop			= veth_ndo_close,
    .ndo_start_xmit		= veth_ndo_start_xmit,
    .ndo_get_stats		= veth_ndo_get_stats,
    .ndo_set_multicast_list	= veth_ndo_set_multicast_list,
    .ndo_tx_timeout		= veth_ndo_tx_timeout
};
#endif

    /*
     * Allocate VEth structure.
     * Allocate Linux net_device structure.
     */
    static int
veth_dev_create (NkDevVlink* rx_link, NkDevVlink* tx_link)
{
    struct net_device*	netdev;
    VEth*		veth;
    int			res;
    char ifname[IFNAMSIZ];

	/*
	 * If creation fails before setting veth->enabled,
	 * cleanup must be done here.
	 */
    if (veth_devices_num >= VETH_MAX) {
	VETH_ERR ("too many veth devices.\n");
	return -EINVAL;
    }

#if defined(CONFIG_NKERNEL_VETH_IFNAME)
    if (!strcmp("rmnet", CONFIG_NKERNEL_VETH_IFNAME))
        strcpy(ifname, "rmnet%d");
    else
        strcpy(ifname, "veth%d");
#else
    strcpy(ifname, "veth%d");
#endif

    netdev = alloc_netdev (sizeof (VEth), ifname, ether_setup);
    if (!netdev) {
	VETH_ERR ("alloc_netdev() failed.\n");
	return -ENOMEM;
    }
    veth		  = netdev_priv (netdev);
    veth->netdev	  = netdev;

    veth->link.veth       = veth;
    veth->link.enabled    = 0;
    veth->link.local.osid = nkops.nk_id_get();
    veth->link.rx_link    = rx_link;
    veth->link.tx_link    = tx_link;

    SET_MODULE_OWNER (netdev);
#ifdef HAVE_NET_DEVICE_OPS
    netdev->netdev_ops         = &veth_netdev_ops;
#else
    netdev->open               = veth_ndo_open;
    netdev->stop               = veth_ndo_close;
    netdev->hard_start_xmit    = veth_ndo_start_xmit;
    netdev->get_stats          = veth_ndo_get_stats;
    netdev->set_multicast_list = veth_ndo_set_multicast_list;
    netdev->tx_timeout         = veth_ndo_tx_timeout;
#endif
    netdev->watchdog_timeo     = 3*HZ;
    netdev->irq                = 0;
    netdev->dma                = 0;

	/* register new Ethernet interface */
    if ((res = register_netdev (netdev))) {
	VETH_ERR ("%s: register_netdev() failed (%d)\n", netdev->name, res);
	free_netdev (netdev);
	return res;
    }
	/* set link as disconnected */
    netif_carrier_off (netdev);

    res = veth_parse_mac_address (veth, rx_link);
    if (res) {
	veth_dev_free (veth);
	return res;	/* Error message already issued */
    }
    res = veth_alloc_link_resources (&veth->link);
    if (res) {
	veth_dev_free (veth);
	return res;	/* Error message already issued */
    }
	/*
	 * This device has all resources allocated.
	 */
    veth_devices [veth_devices_num] = veth;
    veth_devices_num++;
    veth->link.enabled = 1;
	/*
	 * Device is now "enabled", so veth_module_cleanup()
	 * will clean subsequent errors, if any.
	 */
    res = veth_attach_handlers (&veth->link);
    if (res) return res;	/* Error message already issued */

    veth_sysconf_xirq (0, 0);
    return 0;
}

    static NkDevVlink*
veth_find_pair_vlink (NkDevVlink* l)
{
    NkPhAddr    plink = 0;

    while ((plink = nkops.nk_vlink_lookup ("veth", plink)) != 0) {
	NkDevVlink* vlink = (NkDevVlink*) nkops.nk_ptov (plink);

	if ((vlink != l) &&
	    (vlink->s_id == l->c_id) &&
	    (vlink->c_id == l->s_id) &&
	    (vlink->link == l->link)) {
		/*
		 * We found the vlink linking the other way.
		 */
	    return vlink;
	}
    }
    return NULL;
}

    static int
veth_vlink_in_use (NkDevVlink* vlink)
{
    unsigned int minor;

    for (minor = 0; minor < veth_devices_num; minor++) {
	if (veth_devices [minor]->link.rx_link->link == vlink->link) {
	    return 1;
	}
    }
    return 0;
}

static void veth_module_cleanup (void);

    static int __init
veth_module_init (void)
{
    const NkOsId	myid = nkops.nk_id_get();
    unsigned		device_count = 0;
    NkPhAddr		plink = 0;
    NkDevVlink*		rx_link = NULL;
    NkDevVlink*		tx_link = NULL;
	/*
	 * Attach sysconf handler.
	 */
    veth_sysconf_id = nkops.nk_xirq_attach (NK_XIRQ_SYSCONF,
					    veth_sysconf_xirq, 0);
    if (!veth_sysconf_id) {
	VETH_ERR ("cannot attach sysconf handler\n");
	return -ENOMEM;
    }
	/*
	 * Probe bidirectional vlinks and create veth devices.
	 */
    while ((plink = nkops.nk_vlink_lookup ("veth", plink)) != 0) {
	NkDevVlink* vlink = (NkDevVlink*) nkops.nk_ptov (plink);

	if (vlink->s_id == myid && !veth_vlink_in_use (vlink)) {
		/*
		 * We found the first vlink (rx_link), now
		 * find the vlink linking the other way.
		 */
	    rx_link = vlink;
	    tx_link = veth_find_pair_vlink (rx_link);
	    if (tx_link) {
		int res;
		    /*
		     * We found a bidirectional link, now
		     * we can setup a veth device.
		     */
		res = veth_dev_create (rx_link, tx_link);
		if (res) {
			/* Error message already issued */
		    veth_module_cleanup();
		    return res;
		}
		++device_count;
	    }
	}
    }
    VETH_INFO ("%u device(s)\n", device_count);
    return 0;
}

    /*
     * Cleanup virtual Ethernet device driver.
     */

    static void
veth_module_cleanup (void)
{
    unsigned int minor;

    if (veth_sysconf_id) {
	nkops.nk_xirq_detach (veth_sysconf_id);
    }
    for (minor = 0; minor < veth_devices_num; minor++) {
	VEth*		veth = veth_devices [minor];
	VEthLink*	link = &veth->link;

	if (link->enabled) {
	    link->rx_link->s_state = NK_DEV_VLINK_OFF;
	    link->tx_link->c_state = NK_DEV_VLINK_OFF;
	    if (link->local.rx_xid) {
		nkops.nk_xirq_detach (link->local.rx_xid);
	    }
	    if (link->local.tx_ready_xid) {
		nkops.nk_xirq_detach (link->local.tx_ready_xid);
	    }
	    veth_sysconf_trigger (link->peer.osid);
	    veth_dev_free (veth);
	}
    }
    veth_devices_num = 0;
}

    static void __exit
veth_module_exit (void)
{
    veth_module_cleanup();
}

MODULE_DESCRIPTION ("VLX virtual Ethernet device driver");
MODULE_AUTHOR ("Christophe Augier <christophe.augier@redbend.com>");
MODULE_AUTHOR ("Pascal Piovesan <pascal.piovesan@redbend.com>");
MODULE_AUTHOR ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_LICENSE ("GPL");

module_init (veth_module_init);
module_exit (veth_module_exit);

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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <asm/byteorder.h>
#include <linux/tty.h>

/* debugging macros */
#define SETH_INFO(x...)		pr_info("SETH: " x)
#define SETH_DEBUG(x...)	pr_debug("SETH: " x)
#define SETH_ERR(x...)		pr_err("SETH: Error: " x)


#define SETH_ETH_ALEN		ETH_ALEN
#define SETH_ETH_HDR_SIZE	ETH_HLEN
#define SETH_ETH_SIZE		ETH_FRAME_LEN
#define MAX_PART_PKT_SIZE	2500

#define SETH_IPV6_VER 0x6
#define SETH_IPV4_VER 0x4

typedef enum {
	SETH_FULL_PACKET,
	SETH_PARTIAL_PACKET,
	SETH_PARTIAL_HEADER,
} SETH_PAST_STATE;

typedef struct {
	SETH_PAST_STATE state;
	char buf[MAX_PART_PKT_SIZE];
	int size;
	int type;
} SETH_PAST_PACKET;

/*
 * Device instance data.
 */
typedef struct SEth {
	struct net_device_stats stats;	/* net statistics     */
	struct net_device*	netdev;	/* Linux net device   */
	unsigned int		tty_index;
	SETH_PAST_PACKET	past_packet;
} SEth;

#define SETH_MAX	2
static SEth 		*seth_devices[SETH_MAX];
static unsigned int	seth_devices_num = 0;

#define TTY_INDEX_BASE 13
extern int seth_mux_write(struct tty_struct *tty, const unsigned char *buf, int count);

void seth_restart_queue(int tty_index)
{
	int index;

	if(tty_index < TTY_INDEX_BASE || tty_index >= (TTY_INDEX_BASE + SETH_MAX)) {
		SETH_ERR ("seth_restart_queue tty index %d is invalid\n", tty_index);
		return;
	}

	index = tty_index - TTY_INDEX_BASE;

	if(seth_devices[index] && seth_devices[index]->netdev)
		netif_wake_queue(seth_devices[index]->netdev);

	SETH_DEBUG ("seth_restart_queue on %s\n", seth_devices[index]->netdev->name);
}

static int seth_statistic_packets(struct ethhdr *hdr, int len)
{
	if(len >= SETH_ETH_HDR_SIZE && hdr->h_proto == htons(ETH_P_ARP)) {
		return 0;
	}
	return 1;
}

static void seth_pull_data(SEth *seth, int type, void *buf, int size)
{
	struct sk_buff *skb;
	void *ptr = NULL;
	struct ethhdr *header;
	char eth_hdr[] = {0xB6,0x91,0x24,0xa8,0x14,0x72,
			0xb6,0x91,0x24,0xa8,0x14,0x72,
			0x08,0x0};

	size += SETH_ETH_HDR_SIZE;
	if (size > SETH_ETH_SIZE) {
		SETH_ERR ("packet exceed max eth size, discard %d pkt.\n", size);
		ptr = 0;
		return;
	} else {
		skb = dev_alloc_skb(size + NET_IP_ALIGN);
		if (skb == NULL) {
			seth->netdev->stats.rx_dropped++;
			SETH_ERR ("allocate skb failed.\n");
			return;
		}
		skb->dev = seth->netdev;
		skb_reserve(skb, NET_IP_ALIGN);
		ptr = skb_put(skb, size);

		/* adding eth header */
		header = (struct ethhdr *)eth_hdr;
		/* type */
		if (type == SETH_IPV6_VER) {
			header->h_proto = 0x08DD;
			header->h_proto = htons(header->h_proto);
		}
		/* dest addr */
		memcpy((void *)header->h_dest,
				(void*)seth->netdev->dev_addr,
				sizeof(header->h_dest));
		memcpy((void *)ptr,
				(void *)header,
				sizeof(struct ethhdr));

		memcpy(ptr + SETH_ETH_HDR_SIZE, buf, size - SETH_ETH_HDR_SIZE);
		skb->protocol = eth_type_trans(skb, seth->netdev);
		if(seth_statistic_packets((struct ethhdr*)ptr, skb->len)) {
			seth->stats.rx_packets++;
			seth->stats.rx_bytes += skb->len;
		}

		SETH_DEBUG ("seth_pull_data: netif_rx skb\n");
		netif_rx(skb);
	}
}

void seth_rx_data(int tty_index, __u8 *uih_data_start, __u32 uih_len)
{
	void *buf = NULL;
	int i, tot_sz;
	SEth* seth = NULL;
	SETH_PAST_STATE state;

	for(i = 0; i < SETH_MAX; i ++) {
		seth = seth_devices[i];
		if (!seth) {
			SETH_ERR ("seth%d received data but it doesn't exist.\n", i);
			return;
		}

		if(seth->tty_index == tty_index) {
			SETH_DEBUG ("%s receive %d bytes data from mux%d\n",
					seth->netdev->name, uih_len, tty_index);
			break;
		}
	}

	if(i >= SETH_MAX) {
		SETH_ERR ("receive data from not exist net devices.\n");
		return;
	}

	buf = uih_data_start + SETH_ETH_HDR_SIZE;
	tot_sz = uih_len - SETH_ETH_HDR_SIZE;

	SETH_DEBUG ("after remove header, tot_sz = %d\n", tot_sz);

	if (tot_sz <= 0) {
		SETH_ERR ("received uih data length < ethernet header!\n");
		return;
	}

	state = seth->past_packet.state;
	switch (state) {
		case SETH_FULL_PACKET:
			/* no need to do anything */
			break;

		case SETH_PARTIAL_PACKET:
		{
			SETH_PAST_PACKET *p_packet = &seth->past_packet;
			void *ip_hdr = (void *)seth->past_packet.buf;
			int sz, copy_size;

			SETH_DEBUG ("handle partial packet.\n");

			if (p_packet->type == SETH_IPV4_VER) {
				sz = ntohs(((struct iphdr*) ip_hdr)->tot_len);
			} else if (p_packet->type == SETH_IPV6_VER) {
				sz = ntohs(((struct ipv6hdr*) ip_hdr)->payload_len) + sizeof(struct ipv6hdr);
			} else {
				SETH_ERR ("Invalid past packet type(PP): %d\n", p_packet->type);
				p_packet->state = SETH_FULL_PACKET;
				return;
			}
			if(sz > MAX_PART_PKT_SIZE)
				sz = MAX_PART_PKT_SIZE;
			copy_size = sz - p_packet->size;
			if(copy_size <= 0) {
				SETH_ERR ("Invalid past packet(PP): <total %d>:<received %d>\n",
					sz, p_packet->size);
				p_packet->state = SETH_FULL_PACKET;
				return;
			}

			if (tot_sz >= copy_size) {
				/* received data size >= copy size*/
				if(p_packet->size + copy_size <= MAX_PART_PKT_SIZE)
					memcpy(p_packet->buf + p_packet->size, buf, copy_size);
			} else {
				/* copy whatever if received size < packet size */
				if(p_packet->size + tot_sz <= MAX_PART_PKT_SIZE)
					memcpy(p_packet->buf + p_packet->size, buf, tot_sz);
				p_packet->size += tot_sz;
				return;
			}

			seth_pull_data(seth, p_packet->type, (void*)p_packet->buf, sz);

			buf = buf + copy_size;
			tot_sz = tot_sz - copy_size;
			p_packet->state = SETH_FULL_PACKET;
			break;
		}
		case SETH_PARTIAL_HEADER:
		{
			SETH_PAST_PACKET *p_packet = &seth->past_packet;
			void *ip_hdr = (void *)seth->past_packet.buf;
			int sz, hdr_size, copy_size;

			SETH_DEBUG ("handle partial header.\n");

			if (p_packet->type == SETH_IPV4_VER) {
				hdr_size = sizeof(struct iphdr);
			} else if (p_packet->type  == SETH_IPV6_VER) {
				hdr_size = sizeof(struct ipv6hdr);
			} else {
				SETH_ERR ("Invalid past packet type(PH): %d\n", p_packet->type);
				p_packet->state = SETH_FULL_PACKET;
				return;
			}

			copy_size = hdr_size - p_packet->size;
			if(copy_size <= 0) {
				SETH_ERR ("Invalid past packet(PH): <hdr %d>:<received %d>\n",
					hdr_size, p_packet->size);
				p_packet->state = SETH_FULL_PACKET;
				return;
			}

			if(tot_sz >= copy_size) {
				/* received data size >= copy size*/
				memcpy(p_packet->buf + p_packet->size, buf, copy_size);
			} else {
				/* copy whatever if received size < packet size */
				memcpy(p_packet->buf + p_packet->size, buf, tot_sz);
				SETH_DEBUG ("Still partial header \n");
				p_packet->size += tot_sz;
				return;
			}

			buf = buf + copy_size;
			tot_sz = tot_sz - copy_size;
			p_packet->size += copy_size;

			if (p_packet->type == SETH_IPV4_VER) {
				sz = ntohs(((struct iphdr*) ip_hdr)->tot_len);
			} else if (p_packet->type == SETH_IPV6_VER) {
				sz = ntohs(((struct ipv6hdr*) ip_hdr)->payload_len) + sizeof(struct ipv6hdr);
			} else {
				SETH_ERR ("Invalid past packet type(PH-PP): %d\n", p_packet->type);
				p_packet->state = SETH_FULL_PACKET;
				return;
			}

			copy_size = sz - p_packet->size;
			if(copy_size <= 0) {
				SETH_ERR ("Invalid past packet(PH-PP): <total %d>:<received %d>\n",
					sz, p_packet->size);
				p_packet->state = SETH_FULL_PACKET;
				return;
			}

			/* received data size > copy size */
			if (tot_sz >= copy_size) {
				memcpy(p_packet->buf + p_packet->size, buf, copy_size);
			} else {
				/* copy whatever if received data size < packet size */
				memcpy(p_packet->buf + p_packet->size, buf, tot_sz);
				p_packet->size += tot_sz;
				p_packet->state = SETH_PARTIAL_PACKET;
				return;
			}

			seth_pull_data(seth, p_packet->type, (void *)p_packet->buf, sz);

			buf = buf + copy_size;
			tot_sz = tot_sz - copy_size;
			break;
		}
		default:
			SETH_ERR ("Invalid past packet state %d\n", (int)seth->past_packet.state);
			seth->past_packet.state = SETH_FULL_PACKET;
			break;
	}

	while (tot_sz > 0) {
		SETH_PAST_PACKET *p_packet = &seth->past_packet;
		int ver, hdr_size, data_size;
		void *ip_hdr = (void *)buf;

		ver = (((char *)buf)[0] & 0xF0) >> 4;

		if (ver == SETH_IPV4_VER) {
			hdr_size = sizeof(struct iphdr);
		} else if (ver == SETH_IPV6_VER) {
			hdr_size = sizeof(struct ipv6hdr);
		} else {
                    SETH_ERR ("check ip type error, type = %d\n", ver);
			p_packet->state = SETH_FULL_PACKET;
			break;
		}

		if (tot_sz < SETH_ETH_ALEN) {
			SETH_ERR ("buf size is insufficient to decode pkt length\n");
			p_packet->state = SETH_FULL_PACKET;
			return;
		}

		if (tot_sz < hdr_size) {
			p_packet->state = SETH_PARTIAL_HEADER;
			p_packet->size = tot_sz;
			memcpy(p_packet->buf, buf, tot_sz);
			p_packet->type = ver;
			SETH_DEBUG ("partial header packet copied, header_size=%d, received = %d",
					hdr_size, tot_sz);
			return;
		}

		if (ver == SETH_IPV4_VER) {
			data_size = ntohs(((struct iphdr*) ip_hdr)->tot_len);
		} else if (ver == SETH_IPV6_VER) {
			data_size = ntohs(((struct ipv6hdr*) ip_hdr)->payload_len) + sizeof(struct ipv6hdr);
		} else {
			SETH_ERR ("check ip type invalid: %d\n", ver);
			p_packet->state = SETH_FULL_PACKET;
			return;
		}

		if(data_size > MAX_PART_PKT_SIZE)
			data_size = MAX_PART_PKT_SIZE;

		if (tot_sz < data_size) {
			p_packet->state = SETH_PARTIAL_PACKET;
			p_packet->size = tot_sz;
			memcpy(p_packet->buf, buf, tot_sz);
			p_packet->type = ver;
			SETH_DEBUG ("partial data packet copied, total=%d,received=%d\n",
					data_size, tot_sz);
			return;
		}

		seth_pull_data(seth, ver, buf, data_size);

		tot_sz = tot_sz - data_size;
		buf = buf + data_size;

		SETH_DEBUG ("looping for another packet, tot_sz = %d\n", tot_sz);
	}

	seth->past_packet.state = SETH_FULL_PACKET;
}

/*
 * Open interface
 */
static int seth_open (struct net_device *dev)
{
	SEth* seth = netdev_priv(dev);

	/* Reset stats */
	memset(&seth->stats, 0, sizeof(seth->stats));

	seth->past_packet.size = 0;
	seth->past_packet.state = SETH_FULL_PACKET;
	memset(seth->past_packet.buf, 0, sizeof(seth->past_packet.buf));

	seth->netdev->flags &= ~IFF_MULTICAST;
	seth->netdev->flags &= ~IFF_BROADCAST;

	netif_start_queue(dev);

	SETH_INFO("%s is opened!\n", dev->name);

	return 0;
}

/*
 * Close interface
 */
static int seth_close (struct net_device *dev)
{
	netif_stop_queue(dev);

	SETH_INFO("%s is closed!\n", dev->name);

	return 0;
}

static int seth_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	SEth* seth = netdev_priv(dev);
	struct tty_struct tty;
	char shortpkt[ETH_ZLEN];
	int ret;

	SETH_DEBUG ("%s: ready to send %d byte packet of type %x.\n",
		dev->name, skb->len,
		(skb->data[ETH_ALEN+ETH_ALEN] << 8) | skb->data[ETH_ALEN+ETH_ALEN+1]);

	if((skb->len - SETH_ETH_HDR_SIZE) <= 0) {
		SETH_INFO ("Got only header, return.\n");
		dev_kfree_skb_irq(skb);
		netif_wake_queue(dev);
		ret = NETDEV_TX_OK;
		goto quit_xmit;
	}

	if(skb->len < ETH_ZLEN) {
		memset(shortpkt, 0, ETH_ZLEN);
		memcpy(shortpkt, skb->data, ETH_ZLEN);
		skb->data = shortpkt;
		skb->len = ETH_ZLEN;
	}
	/* info the uplayer stop send */
	netif_stop_queue(dev);

	tty.index = seth->tty_index;

	seth_mux_write(&tty, skb->data + SETH_ETH_HDR_SIZE, skb->len - SETH_ETH_HDR_SIZE);

	SETH_DEBUG ("%s: sent data on mux:%d", dev->name, tty.index);

	if(seth_statistic_packets((struct ethhdr*)skb->data, skb->len)) {
		seth->stats.tx_packets++;
		seth->stats.tx_bytes += skb->len;
	}
	dev->trans_start = jiffies;

	ret = NETDEV_TX_OK;
	dev_kfree_skb_irq(skb);

quit_xmit:
	return ret;
}

static struct net_device_stats * seth_get_stats(struct net_device *dev)
{
	SEth * seth = netdev_priv(dev);
	return &(seth->stats);
}

static void seth_set_multicast_list(struct net_device *dev)
{
}

static void seth_tx_timeout(struct net_device *dev)
{
	SETH_INFO ("seth_tx_timeout()\n");
	netif_wake_queue(dev);
}

static struct net_device_ops seth_ops = {
	.ndo_open = seth_open,
	.ndo_stop = seth_close,
	.ndo_start_xmit = seth_start_xmit,
	.ndo_get_stats = seth_get_stats,
	.ndo_set_multicast_list = seth_set_multicast_list,
	.ndo_tx_timeout = seth_tx_timeout,
};

static int __devinit seth_dev_create(unsigned int device_count)
{
	struct net_device* netdev;
	SEth* seth;
	int ret;

	netdev = alloc_netdev(sizeof(SEth), "veth%d", ether_setup);
	if (!netdev) {
		SETH_ERR ("alloc_netdev() failed.\n");
		return -ENOMEM;
	}
	seth = netdev_priv (netdev);
	seth->netdev = netdev;
	seth->tty_index = TTY_INDEX_BASE + device_count;

#ifdef HAVE_NET_DEVICE_OPS
	netdev->netdev_ops = &seth_ops;
#else
	netdev->open = seth_open;
	netdev->stop = seth_close;
	netdev->hard_start_xmit = seth_start_xmit;
	netdev->get_stats = seth_get_stats;
	netdev->set_multicast_list = seth_set_multicast_list;
	netdev->tx_timeout = seth_tx_timeout;
#endif
	netdev->watchdog_timeo = 3*HZ;
	netdev->irq = 0;
	netdev->dma = 0;

	netdev->flags &= ~IFF_MULTICAST;
	netdev->flags &= ~IFF_BROADCAST;
	netdev->flags |= IFF_NOARP;

	random_ether_addr(netdev->dev_addr);

	/* register new Ethernet interface */
	if ((ret = register_netdev (netdev))) {
		SETH_ERR ("register_netdev() failed (%d)\n", ret);
		free_netdev(netdev);
		return ret;
	}

	seth_devices[seth_devices_num++] = seth;

	return 0;
}

/*
 * Cleanup Ethernet device driver.
 */
static void
seth_module_cleanup (void)
{
	SEth* seth;
	int i;

	for (i = 0; i < seth_devices_num; i++) {
		seth = seth_devices[i];
		if(seth) {
			unregister_netdev(seth->netdev);

			free_netdev(seth->netdev);
		}
	}
	seth_devices_num = 0;
}

static int __init
seth_module_init (void)
{
	unsigned int i;
	int res;

	for(i = 0; i < SETH_MAX; i++) {
		seth_devices[i] = NULL;
		res = seth_dev_create(i);
		if (res) {
			SETH_ERR ("seth%d dev create failed!\n", i);
			seth_module_cleanup();
			return res;
		}
	}
	return 0;
}

static void __exit
seth_module_exit (void)
{
    seth_module_cleanup();
}

module_init (seth_module_init);
module_exit (seth_module_exit);

MODULE_DESCRIPTION ("Spreadtrum Ethernet device driver");
MODULE_LICENSE ("GPL");

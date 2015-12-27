/*
 ****************************************************************
 *
 *  Component: VLX VMQ driver interface
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

#ifndef VLX_VMQ_H
#define VLX_VMQ_H

#include <nk/nkern.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION (2,6,18)
#define false	0
#define true	1
#endif

typedef struct vmq_link_t  vmq_link_t;
typedef struct vmq_links_t vmq_links_t;

typedef struct {
    void*	priv;		/* Must be first */
    NkOsId	local_osid;
    NkOsId	peer_osid;
    char*	rx_s_info;
    char*	tx_s_info;
    char*	rx_data_area;
    char*	tx_data_area;
    unsigned	data_max;
    unsigned	msg_max;
    NkPhAddr	ptx_data_area;
} vmq_link_public_t;

typedef struct {
    void*	priv;		/* Must be first */
} vmq_links_public_t;

    /* Link control callbacks */

typedef struct {
    unsigned	msg_count;
    unsigned	msg_max;
    unsigned	data_count;
    unsigned	data_max;
} vmq_xx_config_t;

#define VMQ_XX_CONFIG_IGNORE_VLINK	((vmq_xx_config_t*) 1)

typedef struct {
    void (*link_on)		(vmq_link_t*);
    void (*link_off)		(vmq_link_t*);
    void (*link_off_completed)	(vmq_link_t*);
    void (*sysconf_notify)	(vmq_links_t*);
    void (*receive_notify)	(vmq_link_t*);
    void (*return_notify)	(vmq_link_t*);
    const vmq_xx_config_t*
	 (*get_tx_config)	(vmq_link_t*, const char* tx_s_info);
    const vmq_xx_config_t*
	 (*get_rx_config)	(vmq_link_t*, const char* rx_s_info);
} vmq_callbacks_t;

#ifndef __must_check
    /* Does not exist e.g. in 2.6.0 */
#define __must_check
#endif

    /* Communication functions */
static signed
	vmq_msg_allocate	(vmq_link_t*, unsigned data_len, void** msg,
				 unsigned* data_offset) __must_check;
signed	vmq_msg_allocate_ex	(vmq_link_t*, unsigned data_len, void** msg,
				 unsigned* data_offset, _Bool nonblocking)
				 __must_check;
void	vmq_msg_send		(vmq_link_t*, void* msg);
void	vmq_msg_send_async	(vmq_link_t*, void* msg);
void	vmq_msg_send_flush	(vmq_link_t*);
signed	vmq_msg_receive		(vmq_link_t*, void** msg) __must_check;
void	vmq_msg_free		(vmq_link_t*, void* msg);
void	vmq_msg_return		(vmq_link_t*, void* msg);
unsigned
	vmq_msg_slot		(vmq_link_t*, const void* msg) __must_check;
_Bool	vmq_data_offset_ok	(vmq_link_t*, unsigned data_offset)
				 __must_check;
void	vmq_data_free		(vmq_link_t*, unsigned data_offset);
signed	vmq_return_msg_receive	(vmq_link_t* link2, void** msg)
				 __must_check;
void	vmq_return_msg_free	(vmq_link_t* link2, void* msg);

    /* Link control functions */
static signed
	vmq_links_init		(vmq_links_t**, const char* vlink_name,
				 const vmq_callbacks_t*,
				 const vmq_xx_config_t* tx_config,
				 const vmq_xx_config_t* rx_config)
				 __must_check;
signed	vmq_links_init_ex	(vmq_links_t**, const char* vlink_name,
				 const vmq_callbacks_t*,
				 const vmq_xx_config_t* tx_config,
				 const vmq_xx_config_t* rx_config, void* priv,
				 _Bool is_frontend) __must_check;
signed	vmq_links_start		(vmq_links_t* links);
void	vmq_links_finish	(vmq_links_t*);
_Bool	vmq_links_iterate	(vmq_links_t*, _Bool (*func)(vmq_link_t*,
				 void*), void* cookie);
void	vmq_links_sysconf	(vmq_links_t*);
void	vmq_links_abort		(vmq_links_t*);

    static inline signed
vmq_msg_allocate (vmq_link_t* link2, unsigned data_len, void** msg,
		  unsigned* data_offset)
{
    return vmq_msg_allocate_ex (link2, data_len, msg, data_offset,
				0 /*!nonblocking*/);
}

    static inline signed
vmq_links_init (vmq_links_t** links, const char* vlink_name,
		const vmq_callbacks_t* callbacks,
		const vmq_xx_config_t* tx_config,
		const vmq_xx_config_t* rx_config)
{
    return vmq_links_init_ex (links, vlink_name, callbacks, tx_config,
			      rx_config, 0, false);
}

    static inline NkOsId
vmq_peer_osid (const vmq_link_t* link2)
{
    return ((vmq_link_public_t*) link2)->peer_osid;
}

    static inline const char*
vmq_link_rx_s_info (const vmq_link_t* link2)
{
    return ((vmq_link_public_t*) link2)->rx_s_info;
}

#define vmq_link_s_info	vmq_link_rx_s_info

    static inline const char*
vmq_link_tx_s_info (const vmq_link_t* link2)
{
    return ((vmq_link_public_t*) link2)->tx_s_info;
}

    static inline char*
vmq_rx_data_area (const vmq_link_t* link2)
{
    return ((vmq_link_public_t*) link2)->rx_data_area;
}

    static inline char*
vmq_tx_data_area (const vmq_link_t* link2)
{
    return ((vmq_link_public_t*) link2)->tx_data_area;
}

    static inline unsigned
vmq_data_max (const vmq_link_t* link2)
{
    return ((vmq_link_public_t*) link2)->data_max;
}

    static inline unsigned
vmq_msg_max (const vmq_link_t* link2)
{
    return ((vmq_link_public_t*) link2)->msg_max;
}

    static inline NkPhAddr
vmq_ptx_data_area (const vmq_link_t* link2)
{
    return ((vmq_link_public_t*) link2)->ptx_data_area;
}

#endif


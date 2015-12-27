/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual User MEMory Buffers (VUMEM).                      *
 *             VUMEM front-end/back-end communication protocol definitions.  *
 *                                                                           *
 *  This file provides definitions used for the communication protocol       *
 *  between the VUMEM front-end and back-end kernel drivers.                 *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  This program is free software: you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License Version 2           *
 *  as published by the Free Software Foundation.                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  Version 2 along with this program.                                       *
 *  If not, see <http://www.gnu.org/licenses/>.                              *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VUMEM_PROTO_H
#define _VLX_VUMEM_PROTO_H

#include <vlx/vumem/client-drv.h>
#include <vlx/vumem/server-drv.h>

#define VUMEM_RESC_PDEV			0	/* 1 pdev chunk   */
#define VUMEM_RESC_XIRQ_CLT		1	/* 2 client xirqs */
#define VUMEM_RESC_XIRQ_SRV		2	/* 2 server xirqs */

#define VUMEM_SESSION_NONE		0

typedef nku32_f VumemSessionId;

/*
 *
 */
typedef struct VumemRpcShm {
    nku32_f                   reqId;
    VumemSessionId            reqSessionId;
    nku32_f                   rspId;
    VumemSessionId            rspSessionId;
    VumemSize                 size;
    nku32_f                   err;
    nku8_f                    data[];
} VumemRpcShm;

/*
 * Client->server internal commands.
 */
#define VUMEM_CMD_SESSION_CREATE		(VUMEM_CMD_NR + 1)
#define VUMEM_CMD_BUFFER_CACHE_CTL		(VUMEM_CMD_NR + 2)

/*
 * Server->client internal commands.
 */
#define VUMEM_CMD_BUFFER_MAPPINGS_REVOKE	(VUMEM_CMD_NR + 8)

typedef struct VumemBufferCacheCtlIn {
    VumemBufferId             bufferId;
    VumemCacheOp              cacheOp;
} VumemBufferCacheCtlIn;

typedef struct VumemDrvReq {
    VumemReqHeader            header;
    union {
	VumemBufferCacheCtlIn bufferCacheCtl;
    };
} VumemDrvReq;

typedef struct VumemDrvResp {
} VumemDrvResp;

#endif /* _VLX_VUMEM_PROTO_H */

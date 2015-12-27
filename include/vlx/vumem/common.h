/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual User MEMory Buffers (VUMEM). .                    *
 *             Common definitions for client and server VUMEM entities.      *
 *                                                                           *
 *  This file provides definitions that are shared by client and server      *
 *  VUMEM libraries as well as by kernel front-end and back-end drivers.     *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VUMEM_COMMON_H
#define _VLX_VUMEM_COMMON_H

#include <vlx/vumem/types.h>

#ifndef ESESSIONBROKEN
#define ESESSIONBROKEN				EPIPE
#endif

/*
 * Buffer size expressed in bytes.
 */
typedef nku32_f VumemSize;

/*
 * Global buffer identifier.
 *
 * Such identifier enables to name a buffer in a global and unique way across
 * the platform. This identifier is created by the entity that performs the
 * actual buffer allocation, that is by the server which handles vumem buffer
 * allocation requests.
 */
typedef nku32_f VumemBufferId;

#ifndef _VLX_DEF_CLIENTID
#define _VLX_DEF_CLIENTID

/*
 * Global identifier of a virtual machine (guest OS).
 */
typedef nku32_f VmId;

/*
 * Identifier of an application within a virtual machine (guest OS).
 */
typedef nku32_f AppId;

/*
 * Global identifier of a client application of a virtual service.
 */
typedef struct ClientId {
    VmId  vmId;
    AppId appId;
} ClientId;

#endif /* _VLX_DEF_CLIENTID */

/*
 * This identifier enables to name the client of a VUMEM session in a global
 * and unique way across the platform.
 */
typedef ClientId VumemClientId;

/*
 *
 */
typedef nku32_f VumemBufferAttr;

#define VUMEM_CACHE_NONE			0
#define VUMEM_CACHE_WRITECOMBINE		1
#define VUMEM_CACHE_WRITETHROUGH		2
#define VUMEM_CACHE_WRITEBACK			3
#define VUMEM_CACHE_WRITEALLOC			4
#define VUMEM_CACHE_MASK			0xf

#define VUMEM_ATTR_IS_MAPPABLE			(1 << 0)
#define VUMEM_ATTR_CACHE_POLICY(c)		((c) << 16)
#define VUMEM_ATTR_CACHE_POLICY_GET(a)		(((a)>>16) & VUMEM_CACHE_MASK)
#define VUMEM_ATTR_NEED_CACHE_SYNC_MEM		(1 << 15)
#define VUMEM_ATTR_NEED_CACHE_SYNC_DEV		(1 << 14)
#define VUMEM_ATTR_CACHE_OUTER_ENABLED		(1 << 13)

/*
 *
 */
#define VUMEM_OPAQUE_REQUEST_SIZE_MAX		128

/*
 *
 */

#define VUMEM_PROF_GEN_STAT_GET			1
#define VUMEM_PROF_RESET			2

#define VUMEM_PROF_BUFFER_ALLOC			1
#define VUMEM_PROF_BUFFER_FREE			2
#define VUMEM_PROF_BUFFER_MAP			3
#define VUMEM_PROF_BUFFER_REGISTER		4
#define VUMEM_PROF_BUFFER_UNREGISTER		5
#define VUMEM_PROF_BUFFER_CACHE_CTL		6
#define VUMEM_PROF_BUFFER_CACHE_CTL_REMOTE	7
#define VUMEM_PROF_BUFFER_CTL_REMOTE		8
#define VUMEM_PROF_RPC_CALLS			9
#define VUMEM_PROF_SESSIONS			10
#define VUMEM_PROF_BUFFERS			11
#define VUMEM_PROF_BUFFER_MAPPINGS		12
#define VUMEM_PROF_BUFFER_SINGLE_SIZE_MAX	13
#define VUMEM_PROF_BUFFER_ALLOCATED_SIZE	14
#define VUMEM_PROF_BUFFER_ALLOCATED_SIZE_MAX	15

typedef struct VumemStat {
    nku32_f           id;
    nku32_f           val;
} VumemStat;

typedef struct VumemProfOp {
    nku32_f           cmd;
    nku32_f           count;
    VumemStat*        stats;
} VumemProfOp;

/*
 *
 */
#define VUMEM_IOCTL(c, s)			_IOC(_IOC_NONE,'Q',c,s)
#define VUMEM_IOCTL_CLT_CMD_MAX			8
#define VUMEM_IOCTL_SRV_CMD_MAX			8
#define VUMEM_IOCTL_CMD_MAX			(VUMEM_IOCTL_CLT_CMD_MAX + \
						 VUMEM_IOCTL_SRV_CMD_MAX)
#endif /* _VLX_VUMEM_COMMON_H */

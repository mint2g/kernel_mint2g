/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual User MEMory Buffers (VUMEM).                      *
 *             Definitions for client VUMEM entities.                        *
 *                                                                           *
 *  This file provides definitions that are shared by the client library     *
 *  and the kernel front-end driver.                                         *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VUMEM_CLIENT_DRV_H
#define _VLX_VUMEM_CLIENT_DRV_H

#include <vlx/vumem/common.h>

/*
 *
 */

typedef struct VumemBufferAllocCmd {
    /* In */
    unsigned int    session_fd;
    void*           alloc;
    VumemSize       size;
    /* Out */
    VumemSize       buffer_size;
    VumemBufferId   buffer_id;
    VumemBufferAttr buffer_attr;
} VumemBufferAllocCmd;

/*
 *
 */

typedef nku32_f VumemCacheOp;

#define VUMEM_CACHE_SYNC_TO_MEMORY	(1 << 0)
#define VUMEM_CACHE_SYNC_FROM_MEMORY	(1 << 1)
#define VUMEM_CACHE_SYNC_MEMORY		(VUMEM_CACHE_SYNC_TO_MEMORY |	\
					 VUMEM_CACHE_SYNC_FROM_MEMORY)

#define VUMEM_CACHE_SYNC_TO_DEVICE	(1 << 2)
#define VUMEM_CACHE_SYNC_FROM_DEVICE	(1 << 3)
#define VUMEM_CACHE_SYNC_DEVICE		(VUMEM_CACHE_SYNC_TO_DEVICE |	\
					 VUMEM_CACHE_SYNC_FROM_DEVICE)

#define VUMEM_CACHE_LOCAL		(1 << 4)

/*
 *
 */

typedef nku32_f VumemCtlOp;

#define VUMEM_CTL_REMOTE		1
#define VUMEM_CTL_IS_ALLOCATOR		2
#define VUMEM_CTL_IS_MAPPABLE		3
#define VUMEM_CTL_PRIVATE_GET		4
#define VUMEM_CTL_PRIVATE_SET		5

#define VUMEM_CLT_IOCTL(c, s)			\
    VUMEM_IOCTL((c) + 0, s)

#define VUMEM_IOCTL_SESSION_CREATE		VUMEM_CLT_IOCTL(0, 0)
#define VUMEM_IOCTL_SESSION_MASTER_CREATE	VUMEM_CLT_IOCTL(0, 1)
#define VUMEM_IOCTL_BUFFER_ALLOC		VUMEM_CLT_IOCTL(1, 0)
#define VUMEM_IOCTL_BUFFER_REGISTER		VUMEM_CLT_IOCTL(2, 0)
#define VUMEM_IOCTL_BUFFER_UNREGISTER		VUMEM_CLT_IOCTL(3, 0)
#define VUMEM_IOCTL_BUFFER_CACHE_CTL(op)	VUMEM_CLT_IOCTL(4, op)
#define VUMEM_IOCTL_BUFFER_CTL_REMOTE(sz)	VUMEM_CLT_IOCTL(5, sz)
#define VUMEM_IOCTL_PROF_OP			VUMEM_CLT_IOCTL(7, 0)

#endif /* _VLX_VUMEM_CLIENT_DRV_H */

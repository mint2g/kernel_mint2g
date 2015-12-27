/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual User MEMory Buffers (VUMEM).                      *
 *             Definitions for server VUMEM entities.                        *
 *                                                                           *
 *  This file provides definitions that are shared by the server library     *
 *  and the kernel back-end driver.                                          *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VUMEM_SERVER_DRV_H
#define _VLX_VUMEM_SERVER_DRV_H

#include <vlx/vumem/common.h>

/*
 * VUMEM client/server request commands.
 *
 * These commands correspond to the primitives of the client VUMEM library.
 */
typedef nku32_f VumemCmd;

#define VUMEM_CMD_BUFFER_ALLOC		1
#define VUMEM_CMD_BUFFER_FREE		2
#define VUMEM_CMD_BUFFER_REGISTER	3
#define VUMEM_CMD_BUFFER_UNREGISTER	4
#define VUMEM_CMD_BUFFER_CTL		5
#define VUMEM_CMD_NR			5

/*
 * Input parameters of the "BUFFER_ALLOC" command.
 */
typedef struct VumemBufferAllocIn {
    VumemBufferId bufferId;
    char          alloc[VUMEM_OPAQUE_REQUEST_SIZE_MAX];
    VumemSize     size;
} VumemBufferAllocIn;

/*
 * Input parameters of the "BUFFER_FREE" command.
 */
typedef struct VumemBufferFreeIn {
    VumemBufferId bufferId;
} VumemBufferFreeIn;

/*
 * Input parameters of the "BUFFER_REGISTER" command.
 */
typedef struct VumemBufferRegisterIn {
    VumemBufferId bufferId;
} VumemBufferRegisterIn;

/*
 * Input parameters of the "BUFFER_UNREGISTER" command.
 */
typedef struct VumemBufferUnregisterIn {
    VumemBufferId bufferId;
} VumemBufferUnregisterIn;

/*
 * Input parameters of the "BUFFER_CTL" command.
 */
typedef struct VumemBufferCtlIn {
    VumemBufferId bufferId;
    char          ctl[VUMEM_OPAQUE_REQUEST_SIZE_MAX];
    VumemSize     size;
} VumemBufferCtlIn;

/*
 * VUMEM client/server request header.
 */
typedef struct VumemReqHeader {
    VumemClientId               clId;		/* Client ID of the session */
    VumemCmd                    cmd;		/* Command */
} VumemReqHeader;

/*
 * VUMEM client/server request.
 */
typedef struct VumemReq {
    VumemReqHeader              header;		/* Request header */
    union {
	VumemBufferAllocIn      bufferAlloc;
	VumemBufferFreeIn       bufferFree;
	VumemBufferRegisterIn   bufferRegister;
	VumemBufferUnregisterIn bufferUnregister;
	VumemBufferCtlIn        bufferCtl;
   };						/* Input parameters */
} VumemReq;

/*
 * VUMEM buffer chunk description.
 *
 * A buffer chunk is a contiguous physical memory block. A buffer is
 * potentially made of multiple non-contiguous chunks.
 *
 * pfn  : chunk physical page frame number
 * size : chunk size
 */
typedef unsigned long VumemBufferChunkPfn;
typedef VumemSize     VumemBufferChunkSize;

typedef struct VumemBufferChunk {
    VumemBufferChunkPfn  pfn;
    VumemBufferChunkSize size;
} VumemBufferChunk;

/*
 * VUMEM buffer layout description.
 *
 * A VUMEM buffer layout describes the physical memory of a buffer and the
 * way that memory should be mapped.
 * A VUMEM buffer can be made of multiple non-contiguous physical chunks.
 */

typedef struct VumemBufferLayout {
    VumemSize        size;		/* Total buffer size */
    VumemBufferAttr  attr;		/* Buffer mapping attributes */
    unsigned int     chunks_nr;		/* Number of buffer chunks */
    VumemBufferChunk chunks[0];		/* Array of buffer chunks */
} VumemBufferLayout;

/*
 * VUMEM buffer virtual region description.
 */
typedef struct VumemBufferMap {
    void*     vaddr;			/* Virtual start address */
    VumemSize size;			/* Size of the virtual region */
} VumemBufferMap;

#define VUMEM_BUFFER_NO_VADDR		((void*)-1L)

/*
 * Result of the "BUFFER_ALLOC" command.
 *
 * The server that is connected to the VUMEM back-end drivers directly handles
 * client/server requests and compute responses that are forwarded by the
 * back-end kernel driver to the front-end kernel driver.
 * All responses are forwarded "as is" except the response of the
 * "BUFFER_ALLOC" command which is converted by the back-end kernel driver
 * before being sent to the front-end kernel driver.
 *
 * The server provides the virtual region description of the allocated buffer
 * in the response. The back-end kernel driver converts this description
 * into a buffer layout description. The buffer layout description will enable
 * the front-end kernel driver to map the buffer in the front-end client.
 */
typedef struct VumemBufferAllocOut {
    VumemBufferId         bufferId;
    char                  alloc[VUMEM_OPAQUE_REQUEST_SIZE_MAX];
    union {
	VumemBufferMap    map;
	VumemBufferLayout layout;
    };	/* must be the last */
} VumemBufferAllocOut;

/*
 * Result of the "BUFFER_CTL" command.
 */
typedef struct VumemBufferCtlOut {
    char ctl[VUMEM_OPAQUE_REQUEST_SIZE_MAX];
} VumemBufferCtlOut;


/*
 * VUMEM client/server response.
 */
typedef struct VumemResp {
    unsigned int            retVal;
    union {
	VumemBufferAllocOut bufferAlloc;
	VumemBufferCtlOut   bufferCtl;
    };
} VumemResp;

/*
 *
 */

#define VUMEM_SRV_IOCTL(c, s)			\
    VUMEM_IOCTL((c) + VUMEM_IOCTL_CLT_CMD_MAX, s)

#define VUMEM_IOCTL_RECEIVE			VUMEM_SRV_IOCTL(0, 0)
#define VUMEM_IOCTL_RESPOND			VUMEM_SRV_IOCTL(1, 0)

#endif /* _VLX_VUMEM_SERVER_DRV_H */

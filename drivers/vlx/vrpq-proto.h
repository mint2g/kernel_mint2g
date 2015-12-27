/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Remote Procedure Queue (VRPQ).                    *
 *             VRPQ front-end/back-end communication protocol definitions.   *
 *                                                                           *
 *  This file provides definitions used for the communication protocol       *
 *  between the VRPQ front-end and back-end kernel drivers.                  *
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

#ifndef _VLX_VRPQ_PROTO_H
#define _VLX_VRPQ_PROTO_H

#include <vlx/vrpq/client-drv.h>
#include <vlx/vrpq/server-drv.h>

/*
 *
 */

#define VRPQ_SESSION_MAX		1024
#define VRPQ_CHAN_MAX			1024
#define VRPQ_CHAN_MASK			(VRPQ_CHAN_MAX - 1)
#define VRPQ_RING_REQS_MIN		256
#define VRPQ_RING_RESPS_MAX		VRPQ_CHAN_MAX
#define VRPQ_CALLS_MIN			16
#define VRPQ_CALLS_MAX			(VRPQ_RING_RESPS_MAX - 8)
#define VRPQ_ADM_CALLS_MAX		128
#define VRPQ_ADM_PARAM_CHUNK_SIZE	64
#define VRPQ_ADM_PARAM_TOTAL_SIZE	((VRPQ_ADM_CALLS_MAX + 1) *	\
					 VRPQ_ADM_PARAM_CHUNK_SIZE)
#define VRPQ_CHAN_REQS_MAX		512
#define VRPQ_CHAN_REQS_MASK		(VRPQ_CHAN_REQS_MAX - 1)

#define VRPQ_RESC_PMEM			0	/* 1 pmem chunk   */
#define VRPQ_RESC_XIRQ_CLT		1	/* 2 client xirqs */
#define VRPQ_RESC_XIRQ_SRV		2	/* 2 server xirqs */

/*
 * Message identifier for a procedure request.
 */
typedef nku32_f VrpqMsgId;

/*
 * Message cookie used by the front-end as a reference to a synchronization
 * object. This is used to awake a front-end thread that is waiting for a
 * response message (vrpq call case).
 */
typedef nku32_f VrpqMsgCookie;

/*
 * Global identifier of a client/server session.
 *
 * Such identifier is shared by the front-end and the back-end drivers.
 * Client and server kernel drivers are in charge of converting local
/ * session handles ('VrpqSessionHandle') from/to global session identifiers
 * ('VrpqSessionId').
 */
typedef nku32_f VrpqSessionId;

/*
 * Global identifier of a bidirectional client/server communication channel.
 *
 * Such identifier is shared by the front-end and the back-end drivers.
 * Client and server kernel drivers are in charge of converting local
 * channel handles ('VrpqChanHandle') from/to global channel identifiers
 * ('VrpqChanId').
 */
typedef nku32_f VrpqChanId;

/*
 * Message flags for a procedure request.
 *
 * A typical usage of these flags is to indicate if the corresponding
 * procedure request has been fully handled and thus if the associated
 * resources can be freed.
 */
typedef union {
    nku32_f     all;
    struct {
	nku8_f  inuse;
	nku8_f  callState;
	nku8_f  callProcess;
    };
} VrpqMsgFlags;

#define VRPQ_CALL_READY			1
#define VRPQ_CALL_CANCELED		2
#define VRPQ_CALL_HANDLING		1
#define VRPQ_CALL_HANDLED		2

#define VRPQ_REQ_IS_CALL(f)		((f).callState)

/*
 * Free-run ring message index.
 */
typedef nku32_f VrpqRingIdx;

/*
 * Message of a procedure request.
 *
 * Such message is placed in a 'requests' ring shared between the front-end
 * and the back-end.
 */
typedef struct VrpqReqMsg {
    VrpqMsgId             msgId;
    VrpqMsgCookie         cookie;
    VrpqChanId            chanId;
    volatile VrpqMsgFlags flags;
    VrpqProcReq           procReq;
    nku32_f               pad[1];
} VrpqReqMsg;	/* 32 bytes */

/*
 * Message of a procedure response.
 *
 * Such message is placed in a 'response' ring shared between the front-end
 * and the back-end.
 */
typedef struct VrpqRspMsg {
    VrpqRingIdx           reqIdx;
    nku32_f               retVal;
} VrpqRspMsg;	/* 8 bytes */

/*
 *
 */
typedef struct VrpqReqRingGbl {
    volatile VrpqRingIdx  headIdx;
    volatile nku32_f      fullFlag;
} VrpqReqRingGbl;

/*
 *
 */
typedef struct VrpqReqRing {
    VrpqReqRingGbl*       global;
    volatile VrpqRingIdx  headIdx;
    volatile VrpqRingIdx  tailIdx;
    VrpqRingIdx           modMask;
    VrpqReqMsg*           reqs;
} VrpqReqRing;

#define VRPQ_REQ_RING_IS_FULL(h, t, m)			\
    ((h) - (t) >= (m))

#define VRPQ_REQ_RING_IS_EMPTY(h, t)			\
    ((t) == (h))

#define VRPQ_REQ_RING_IS_OVERFLOW(h, t, m)		\
    ((h) - (t) > (m))

/*
 *
 */
typedef struct VrpqRspRingGbl {
    volatile VrpqRingIdx  headIdx;
    volatile VrpqRingIdx  tailIdx;
} VrpqRspRingGbl;

/*
 *
 */
typedef struct VrpqRspRing {
    VrpqRspRingGbl*       global;
    volatile VrpqRingIdx  headIdx;
    volatile VrpqRingIdx  tailIdx;
    VrpqRingIdx           modMask;
    VrpqRspMsg*           resps;
} VrpqRspRing;

#define VRPQ_RSP_RING_IS_FULL(h, t, m)			\
    ((h) - (t) >= (m))

#define VRPQ_RSP_RING_IS_EMPTY(h, t)			\
    ((t) == (h))

#define VRPQ_RSP_RING_IS_OVERFLOW(h, t, m)		\
    ((h) - (t) > (m))

/*
 * Parameters chunk flags.
 */
typedef union {
    nku32_f     all;
    struct {
	nku8_f  inuse;
    };
} VrpqChunkFlags;

/*
 * Description of a chunk of shared memory containing a set of input or output
 * parameters.
 */
typedef struct VrpqParamChunk {
    VrpqSize              size;		/* Size of chunk data in bytes */
    VrpqChunkFlags        flags;	/* Free or allocated */
    VrpqParamInfo         paramInfo[0];
} VrpqParamChunk;

/*
 * Meta-data describing a ring of shared chunks of input/output parameters.
 */
typedef struct VrpqParamRing {
    VrpqParamChunk*       head;
    VrpqParamChunk*       tail;
    VrpqSize              size;		/* Size of ring expressed in bytes */
    VrpqParamChunk*       params;
    VrpqParamChunk*       paramsLimit;
} VrpqParamRing;

#define VRPQ_PARAM_CHUNK_ALIGN_SIZE	32
#define VRPQ_PARAM_CHUNK_ALIGN_MASK	(~(VRPQ_PARAM_CHUNK_ALIGN_SIZE - 1))

#define VRPQ_PARAM_CHUNK_ALIGN(x)					\
    (((x) + VRPQ_PARAM_CHUNK_ALIGN_SIZE - 1) & VRPQ_PARAM_CHUNK_ALIGN_MASK)

#define VRPQ_PARAM_CHUNK_SIZE(s)					\
    (VRPQ_PARAM_CHUNK_ALIGN((s) + sizeof(VrpqParamChunk)))

#define VRPQ_PARAM_CHUNK_SIZE_SAFE(s)					\
    ({									\
	VrpqSize _s = (s);						\
	if ((_s &= VRPQ_PARAM_CHUNK_ALIGN_MASK) == 0) {			\
	    _s = VRPQ_PARAM_CHUNK_SIZE(0);				\
	}								\
	(_s);								\
    })

#define VRPQ_PARAM_CHUNK_NEXT(c, s)					\
    ((VrpqParamChunk*)(((nku8_f*)(c)) + (s)))

#define VRPQ_PARAM_CHUNK_TO_PINFO(c)					\
    ((c)->paramInfo)

#define VRPQ_PARAM_PINFO_TO_CHUNK(d)					\
    ((VrpqParamChunk*)(((unsigned long)(d)) & VRPQ_PARAM_CHUNK_ALIGN_MASK))

#define VRPQ_PARAM_DATA_TO_CHUNK(d)					\
    VRPQ_PARAM_PINFO_TO_CHUNK(d)

#define VRPQ_PARAM_FREE(_pi)						\
    do {								\
	VRPQ_PARAM_PINFO_TO_CHUNK(_pi)->flags.inuse = 0;		\
    } while (0)

#define VRPQ_PARAM_RECYCLE(_off, _pb ,_ps)				\
    do {								\
	VrpqParamChunk* _chunk;						\
	if ((_off) < (_ps)) {						\
	    _chunk = VRPQ_PARAM_DATA_TO_CHUNK(((nku8_f*)(_pb))+(_off)); \
	    _chunk->flags.inuse = 0;					\
	}								\
    } while (0)

#define VRPQ_BYTE_FLAGS_INIT(b0,b1,b2,b3)				\
    ({									\
	union {								\
	    nku32_f all;						\
	    nku8_f  byte[4];						\
	} _f = {							\
	    .byte[0] = (b0),						\
	    .byte[1] = (b1),						\
	    .byte[2] = (b2),						\
	    .byte[3] = (b3),						\
	};								\
	(_f.all);							\
    })

/*
 *
 */
typedef struct VrpqPmemLayout {
    void*           pmemVaddr;
    VrpqSize        pmemSize;
    VrpqRingIdx     reqCount;
    VrpqSize        reqSize;
    VrpqReqRingGbl* reqRingGbl;
    VrpqReqMsg*     reqs;
    VrpqRingIdx     rspCount;
    VrpqSize        rspSize;
    VrpqRspRingGbl* rspRingGbl;
    VrpqRspMsg*     resps;
    VrpqSize        usrParamSize;
    VrpqParamChunk* usrParams;
    VrpqSize        admParamSize;
    VrpqParamChunk* admParams;
} VrpqPmemLayout;

#define VRPQ_PMEM_ALIGN_DEFAULT		64

#define VRPQ_PMEM_NEXT_ALIGN(adr, sz, align)				\
    VRPQ_ROUND_UP((((nku8_f*)(adr)) + (sz)), align, void*)

#define VRPQ_PMEM_NEXT(adr, sz)						\
    VRPQ_PMEM_NEXT_ALIGN(adr, sz, VRPQ_PMEM_ALIGN_DEFAULT)

/*
 * Internal Procedure requests.
 *
 * Functions of the VRPQ_GRP_CONTROL group.
 */
#define VRPQ_CTRL_SESSION_CREATE	1
#define VRPQ_CTRL_SESSION_DESTROY	2
#define VRPQ_CTRL_CHAN_CREATE		3
#define VRPQ_CTRL_CHAN_DESTROY		4

typedef struct VrpqSessionCreateIn {
    nku32_f       appId;
} VrpqSessionCreateIn;

typedef struct VrpqSessionCreateOut {
    VrpqSessionId sessionId;
} VrpqSessionCreateOut;

typedef struct VrpqSessionDestroyIn {
    VrpqSessionId sessionId;
} VrpqSessionDestroyIn;

typedef struct VrpqChanCreateIn {
    VrpqSessionId sessionId;
} VrpqChanCreateIn;

typedef struct VrpqChanCreateOut {
    VrpqChanId    chanId;
} VrpqChanCreateOut;

typedef struct VrpqChanDestroyIn {
    VrpqChanId    chanId;
} VrpqChanDestroyIn;

#endif /* _VLX_VRPQ_PROTO_H */

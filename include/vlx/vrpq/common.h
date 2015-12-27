
/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Remote Procedure Queue (VRPQ).                    *
 *             Common definitions for client and server VRPQ entities.       *
 *                                                                           *
 *  This file provides definitions that are shared by client and server      *
 *  VRPQ libraries as well as by kernel front-end and back-end drivers.      *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VRPQ_COMMON_H
#define _VLX_VRPQ_COMMON_H

#include <vlx/vrpq/types.h>

/*
 * Special VRPQ error codes:
 *
 * EREQSIZE:       procedure request is too big to fit in shared memory
 * ECHANINVALID:   channel is invalid
 * ECHANBROKEN:    channel has been destroyed
 * EBADREQ:        invalid procedure request
 */
#define EREQSIZE	EMSGSIZE
#define ECHANINVALID	EBADF
#ifndef ESESSIONBROKEN
#define ESESSIONBROKEN	EPIPE
#endif
#define ECHANBROKEN	EPIPE
#define EBADREQ		EBADR

/*
 * Size expressed in bytes.
 */
typedef nku32_f VrpqSize;

/*
 * Local client/server session handle.
 *
 * A session is a limited period of time during which a client is connected
 * to a server in order to communicate. Client/server messages are exchanged
 * through communication channels (see below). Channels and other communication
 * resources are attached to a particular session and exist as long as this
 * session exists.
 * Such session handle is local to the user entity.
 */
typedef void* VrpqSessionHandle;

/*
 * Wildcard enabling a server to accept channels from any sessions.
 */
#define VRPQ_SESSION_ANY		0xffffffff

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
 * This identifier enables to name the client of a VRPQ session in a global
 * and unique way across the platform.
 */
typedef ClientId VrpqClientId;

/*
 * Local handle of a bidirectional client/server communication channel.
 *
 * Such handle is local to the user entity.
 */
typedef void* VrpqChanHandle;

/*
 * Procedure identifier.
 *
 * It is composed of a group number and a function number.
 * The group number is encoded in the high 16-bits whereas the function
 * number resides in the low 16-bits.
 */
typedef nku32_f VrpqProcId;

/*
 *
 */
#define VRPQ_PARAM_ALIGN_SIZE		8

/*
 * Parameter reference.
 */
typedef struct VrpqParamRef {
        union {
                VrpqSize      size;
                unsigned long pad;
        };
        union {
                void*         uptr;
                VrpqSize      offset;
        };
} VrpqParamRef;	/* 8 or 16 bytes */

/*
 * The group number '0' is reserved for internal (driver level) purpose.
 * For instance, it is used by the front-end driver for requesting the
 * back-end driver to create/destroy channels.
 */
#define VRPQ_GRP_CONTROL		0

/*
 * Helper macros for encoding/decoding the procedure identifier.
 */
#define VRPQ_PROC_ID(g,f)	  ((((nku32_f)(g)) << 16) | ((nku32_f)(f)))
#define VRPQ_PROC_ID_GRP_GET(id)  (((nku32_f)(id)) >> 16)
#define VRPQ_PROC_ID_FUNC_GET(id) (((nku32_f)(id)) & ((1 << 16) - 1))

/*
 * Helper that rounds up a value according to a given alignment.
 */
#define VRPQ_ROUND_UP(val, align, type)					   \
    ({									   \
	(type)((((unsigned long)(val)) + ((align) - 1)) & ~((align) - 1)); \
    })

/*
 *
 */

#define VRPQ_PROF_GEN_STAT_GET			1
#define VRPQ_PROF_PROC_STAT_GET			2
#define VRPQ_PROF_PROC_STAT_GET_ALL		3
#define VRPQ_PROF_RESET				4

#define VRPQ_PROF_ADM_CALLS			1
#define VRPQ_PROF_SESSION_CREATE		2
#define VRPQ_PROF_SESSION_DESTROY		3
#define VRPQ_PROF_CHAN_CREATE			4
#define VRPQ_PROF_CHAN_DESTROY			5
#define VRPQ_PROF_NOTIFY			6
#define VRPQ_PROF_POST_MULTI_CALL		7
#define VRPQ_PROF_POSTS_PER_CALL		8
#define VRPQ_PROF_POSTS_PER_CALL_MAX		9
#define VRPQ_PROF_POST_MULTI_NOTIFY		10
#define VRPQ_PROF_POSTS_PER_NOTIFY		11
#define VRPQ_PROF_POSTS_PER_NOTIFY_MAX		12
#define VRPQ_PROF_POST_MULTI			13
#define VRPQ_PROF_GROUPED_POSTS			14
#define VRPQ_PROF_GROUPED_POSTS_MAX		15
#define VRPQ_PROF_RESPONSES_INTR		16
#define VRPQ_PROF_PARAM_ALLOC_FAST		17
#define VRPQ_PROF_PARAM_ALLOC_SLOW		18
#define VRPQ_PROF_IN_PARAM_REF			19
#define VRPQ_PROF_PARAM_SINGLE_SZ_MAX		20
#define VRPQ_PROF_PARAM_ALLOCATED_SZ		21
#define VRPQ_PROF_PARAM_ALLOCATED_SZ_MAX	22
#define VRPQ_PROF_REQS_NR			23
#define VRPQ_PROF_REQS_NR_MAX			24
#define VRPQ_PROF_REQS_FREE			25
#define VRPQ_PROF_REQS_PER_FREE			26
#define VRPQ_PROF_REQ_RING_FULL			27
#define VRPQ_PROF_PARAM_RING_FULL		28
#define VRPQ_PROF_WAIT_FOR_SPACE_INTR		29
#define VRPQ_PROF_PROC_STAT_TBL			30

typedef struct VrpqStat {
    nku32_f           id;
    nku32_f           val;
} VrpqStat;

typedef struct VrpqProfOp {
    nku32_f           cmd;
    nku32_f           count;
    VrpqStat*         stats;
} VrpqProfOp;

/*
 *
 */
#define VRPQ_IOCTL(c, s)			_IOC(_IOC_NONE,'Q',c,s)
#define VRPQ_IOCTL_CLT_CMD_MAX			8
#define VRPQ_IOCTL_SRV_CMD_MAX			8
#define VRPQ_IOCTL_CMD_MAX			(VRPQ_IOCTL_CLT_CMD_MAX + \
						 VRPQ_IOCTL_SRV_CMD_MAX)

#endif /* _VLX_VRPQ_COMMON_H */

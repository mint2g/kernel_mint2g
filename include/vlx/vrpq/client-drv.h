/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Remote Procedure Queue (VRPQ).                    *
 *             Definitions for client VRPQ entities.                         *
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

#ifndef _VLX_VRPQ_CLIENT_DRV_H
#define _VLX_VRPQ_CLIENT_DRV_H

#include <vlx/vrpq/common.h>

/*
 * Pointer to a buffer of input parameters.
 *
 * For each procedure, the client VRPQ library provides the caller with a
 * dedicated memory buffer into which procedure input parameters must be
 * serialized. Such serialized parameters will be made available to the
 * server which will decode (un-serialize) them in order to call the
 * appropriate procedure routine.
 *
 * The 'vrpqParamInAlloc()' service is used by the caller of the client VRPQ
 * library to allocate a buffer that should hold serialized input parameters.
 *
 * It is up to the client VRPQ library to decide where such buffers are
 * actually allocated. If possible, Those buffers are directly allocated
 * in a memory area that is shared with the server. In this case, when the
 * input parameters are serialized (and put into the buffer), they are
 * automatically made available to the server. No extra copy is needed to
 * bring them to the server.
 * If there is no shared memory available in user land between the client
 * and the server entities, the input parameters buffers are allocated in
 * the client memory (user space). Input parameters that are serialized into
 * such buffers need to be copied by the kernel driver in order to be brought
 * to the server.
 *
 * Because input parameters can be copied by the kernel driver (in addition
 * to the serialization made in user space), the client VRPQ library provides
 * an optimization for input parameters that have significant memory footprint:
 * those parameters can be specified as references during the serialization
 * process. They will actually be serialized when the procedure request will
 * be issued. This enables to merge the (potential) kernel driver extra copy 
 * and the user serialization into an unique serialization operation.
 *
 * It is up to the caller to decide which input parameter is directly
 * serialized (and potentially recopied later) and which is provided as a
 * reference (and thus serialized later).
 * When input parameters are directly serialized by the (user-land) caller
 * of the client VRPQ library, the cost of the potential extra copy is
 * merely the cost of a secure copy operation (copyin).
 * When input parameters are provided as references, there is just one
 * secure copy operation per parameter, but the parameter references can
 * be considered as meta-data whose management has an extra cost.
 * Consequently, choosing between directly serializing an input parameter
 * or giving it as a reference is a tradeoff between the cost of a potential
 * extra copy (which exists only if the parameters buffer is not shared by
 * the server) and the cost of adding and managing meta-data.
 * Usually only parameters having a significant size (several dozen of bytes)
 * should be given as references during the serialization process.
 *
 * For each procedure, it is possible to mix input parameters which are
 * directly serialized and those which are provided as references.
 * The input parameters buffer, allocated by the 'vrpqParamInAlloc()'
 * service, must be filled as follow:
 *
 *    +----------------------------------+----+----+----+
 *    |                                  |Size|Size|Size|
 *    |                                  |Ptr |Ptr |Ptr |
 *    +----------------------------------+----+----+----+
 *    \________________  _______________/\______  ______/
 *                     \/                       \/
 *             Serialized (opaque)        Input parameters
 *              input parameters          provided as ref.
 *
 * Serialized (opaque) parameters are placed first in the buffer and are
 * followed by parameter references if any. Each parameter reference is made
 * of the size of the parameter content (expressed in bytes) and a pointer to
 * this content.
 * Note that only parameters which are available as contiguous data in
 * memory (typically buffers or structures without embedded pointers) can
 * be provided as references.
 *
 * Serialized parameters are placed into the buffer using dedicated encoding
 * routines (see 'VRPQ_PARAM_*' macros).
 */
typedef void* VrpqParamInPtr;

/*
 * Output parameters.
 *
 * For each procedure, the client VRPQ library provides the caller with a
 * dedicated memory buffer into which procedure output parameters must be
 * described.
 *
 * The 'vrpqParamOutAlloc()' service is used by the caller of the client
 * VRPQ library to allocate a buffer that should hold an output parameters
 * description.
 *
 * The output parameters buffer must be filled as follow:
 *
 *    +----+----+----+
 *    |Size|Size|Size|
 *    |Ptr |Ptr |Ptr |
 *    +----+----+----+
 *
 * Each output parameter is described by a reference made of a size and a
 * pointer. The size, expressed in bytes, gives the amount of space available
 * at the address pointed to by ptr where the output parameter data should be
 * placed.
 */
typedef VrpqParamRef* VrpqParamOutPtr;

/*
 *
 */

#define VRPQ_SERIAL_DIRECT_PARAMS_SIZE_MAX	65536
#define VRPQ_IN_REFS_MAX			32
#define VRPQ_OUT_REFS_MAX			32
#define VRPQ_MULTI_PROCS_MAX			32

typedef struct VrpqParamPostInfo {
    nku16_f               inRefOffset;
    nku16_f               inRefCount;
    VrpqSize              inFullSize;
    void*                 in[0];
} VrpqParamPostInfo;	/* 8 bytes */

typedef struct VrpqParamCallInfo {
    nku16_f               inRefOffset;
    nku16_f               inRefCount;
    VrpqSize              inFullSize;
    nku16_f               outRefOffset;
    nku16_f               outRefCount;
    VrpqSize              fullSize;
    void*                 in[0];
} VrpqParamCallInfo;	/* 16 bytes */

typedef union VrpqParamInfo {
    VrpqParamPostInfo     post;
    VrpqParamCallInfo     call;
} VrpqParamInfo;

typedef struct VrpqProcInfo {
    VrpqProcId            procId;
    VrpqParamInfo*        paramInfo;
    VrpqSize              paramCopySize;
    VrpqSize              paramFullSize;
} VrpqProcInfo;

#define VRPQ_CLT_IOCTL(c, s)			\
    VRPQ_IOCTL((c) + 0, s)

#define VRPQ_IOCTL_SESSION_CREATE		VRPQ_CLT_IOCTL(0, 0)
#define VRPQ_IOCTL_CHAN_CREATE			VRPQ_CLT_IOCTL(1, 0)
#define VRPQ_IOCTL_NOTIFY			VRPQ_CLT_IOCTL(2, 0)
#define VRPQ_IOCTL_POST_MULTI(c)		VRPQ_CLT_IOCTL(3, c)
#define VRPQ_IOCTL_POST_MULTI_NOTIFY(c)		VRPQ_CLT_IOCTL(4, c)
#define VRPQ_IOCTL_POST_MULTI_CALL(c)		VRPQ_CLT_IOCTL(5, c)
#define VRPQ_IOCTL_PROF_OP			VRPQ_CLT_IOCTL(7, 0)

#endif /* _VLX_VRPQ_CLIENT_DRV_H */

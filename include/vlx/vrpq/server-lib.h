/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Remote Procedure Queue (VRPQ).                    *
 *             Server VRPQ library interface.                                *
 *                                                                           *
 *  This file provides definitions and prototypes that are exported by the   *
 *  server VRPQ library.                                                     *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VRPQ_SERVER_LIB_H
#define _VLX_VRPQ_SERVER_LIB_H

#include <vlx/vrpq/server-drv.h>

/*
 *
 */
#define VRPQ_PARAM_ADDR(paramOffset, shmBase)		\
    ((void*)(((nku8_f*) (shmBase)) + (paramOffset)))

/*
 * Decode (un-serialize) a basic input parameter.
 *
 * The input parameter must be of any of the basic types: char, short, int,
 * long, long long...
 *
 * buffer: the input parameter buffer residing in the shared memory.
 * offset: the offset in the input parameter buffer where the basic parameter
 *         is located.
 *         IMPORTANT: the offset must be aligned on the size of the type of
 *         the parameter.
 * type  : the type of the parameter.
 */
#define VRPQ_PARAM_READ(buffer, offset, type)				\
    ({									\
	(void)(((void*)0) == (buffer));					\
	*((type*)(((unsigned char*) (buffer)) + (offset)));		\
    })

/*
 * Decode (un-serialize) an input data buffer.
 *
 * buffer: the input parameter buffer residing in the shared memory.
 * offset: the offset in the input parameter buffer where the data are located.
 * data  : the destination address.
 * len   : the data length.
 */
#define VRPQ_PARAM_BUF_READ(buffer, offset, data, len)			\
    do {								\
	(void)(((void*)0) == (buffer));					\
	memcpy(data, ((unsigned char*) (buffer)) + (offset), len);	\
    } while (0)
	
/*
 * Decode (un-serialize) an input null-terminated string.
 *
 * buffer: the input parameter buffer residing in the shared memory.
 * offset: the offset in the input parameter buffer where the string is
 *         located.
 * str   : the destination string address.
 */
#define VRPQ_PARAM_STR_READ(buffer, offset, str)			\
    do {								\
	(void)(((void*)0) == (buffer));					\
	strcpy(str, ((const char*) (buffer)) + (offset));		\
    } while (0)

/*
 * Get a reference to an "opaque" input parameter.
 *
 * This macro enables to use an input parameter "in place" in the 
 * client/server shared memory. The input parameter must have been
 * serialized as an "opaque" parameter by the client (using
 * 'VRPQ_PARAM_ARRAY_WRITE', 'VRPQ_PARAM_BUF_WRITE' or 'VRPQ_PARAM_STR_WRITE').
 *
 * buffer: the input parameter buffer residing in the shared memory.
 * offset: the offset in the input parameter buffer where the data are located.
 * type  : the type of the referenced input parameter.
 */
#define VRPQ_PARAM_IN_PLACE(buffer, offset, type)			\
    ({									\
	(void)(((void*)0) == (buffer));					\
	(type)(((unsigned char*) (buffer)) + (offset));			\
    })

/*
 * 
 */
#define VRPQ_PARAM_REF_IN_PLACE(buffer, __offset, type, __size, error)	\
    ({									\
	VrpqParamRef* __ref =						\
	    (VrpqParamRef*)(((unsigned char*) (buffer)) + (__offset));	\
	VrpqSize __rsize =__ref->size;					\
	type     __data;						\
	(void)(((void*)0) == (buffer));					\
	if (__rsize != (__size)) (error)++;				\
	if (!__rsize) {							\
	    __data = NULL;						\
	} else {							\
	    __data = (type)(((unsigned char*)(__ref)) + __ref->offset);	\
	}								\
	__data;								\
    })

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Wait for the notification of a session creation performed by a client
 * application.
 *
 * devName: the vrpq device name.
 * session: a handle that describes the newly created session is returned.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqSessionAccept (const char* devName, VrpqSessionHandle* session);

/*
 * Destroy a session previously obtained by 'vrpqSessionAccept()'.
 *
 * session: session handle.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqSessionDestroy (VrpqSessionHandle session);

/*
 * Get the ID of the client (client application identifier global across the
 * platform) that created a session.
 *
 * session : session handle.
 * clientId: the global client ID is returned.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqClientIdGet (VrpqSessionHandle session, VrpqClientId* clientId);

/*
 * Request the server kernel driver to map in the current process address
 * space the shared memory that is used during a client/server session.
 *
 * The shared memory contains procedure request descriptors and input/output
 * parameters that pass through the channels instantiated within the given
 * session.
 *
 * session: session handle or 'VRPQ_SESSION_ANY'.
 * addr:    base address
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqShmMap (VrpqSessionHandle session, void** addr);

/*
 * Wait for the notification of a channel creation performed by a client
 * application.
 *
 * The caller specifies the session from which it wants to accept channels.
 * Only the channels that have been created for the given session will be
 * notified to the caller. If the caller wants to accept channels from any
 * sessions, it should use the 'VRPQ_SESSION_ANY' wildcard.
 *
 * session: session handle or 'VRPQ_SESSION_ANY'.
 * chan   : a handle that describes the newly created channel is returned.
 *          It must be specified in subsequent VRPQ calls.
 *
 * The server should use this function to be notified that a client application
 * has requested the creation of a communication channel. When the server is
 * awaken and returns from this call, it should create a new server instance
 * in order to serve the requests which pass through the newly created channel.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqChanAccept (VrpqSessionHandle session, VrpqChanHandle* chan);

/*
 * Destroy a channel previously obtained by 'vrpqChanAccept()'.
 *
 * chan: channel handle.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqChanDestroy (VrpqChanHandle chan);

/*
 * Receive incoming client procedure requests.
 *
 * This function enables to receive multiple requests in a single call.
 * If there is no pending request in the channel, this function will block
 * until new requests are available.
 * Each procedure request is described by a 'VrpqProcReq' descriptor which
 * is located in the client/server shared memory and whose pointer is store
 * by the server kernel driver in the 'reqs' array.
 * Input and output parameters buffers also reside in this shared memory and
 * are described in the 'VrpqProcReq' descriptor.
 *
 * chan : channel handle.
 * reqs : array of pointers to incoming procedure requests (allocated by the
 *        caller).
 * count: size of the array.
 * avail: number of available incoming procedure requests (filled in by the
 *        server kernel driver).
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqReceive (VrpqChanHandle chan, VrpqProcReq** reqs, int count,
		 int* avail);

/*
 * Notify the server kernel driver that the last synchronous procedure
 * request has been handled.
 *
 * chan  : channel handle.
 * reason: exit status.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqCallSetReason (VrpqChanHandle chan, int reason);

/*
 * Ask the server kernel driver for the sizes of the local buffers that need
 * to be allocated for receiving big procedure requests.
 *
 * Input and output parameters pass through the client/server shared memory
 * following the 'VrpqShmParamInPtr' and 'VrpqShmParamOutPtr' layouts.
 * There is a single buffer for the input parameters and a single buffer
 * for the output parameters. In these buffers, parameters are serialized,
 * that is placed one behind the other.
 * Using the client/server shared memory enables to avoid extra copies.
 * It may happen that a parameters buffer does not fit in the client/server
 * shared memory. In this case, the 'vrpqReceive()' function returns EREQSIZE.
 * In order to receive such a big procedure request, the daemon should
 * allocate the parameters buffers in its own memory and call the server
 * kernel driver again using the 'vrpqLocalBufReceive()' function.
 * The 'vrpqLocalBufInfo()' function is used by the daemon to know the size
 * of the parameters buffers it should allocate.
 * The daemon is likely to allocate the input parameters buffer, the output
 * one or both.
 * When a big procedure request is received using this mechanism, an extra
 * copy is performed (in order to copy the parameters from/to the local daemon
 * buffers).
 *
 * chan   : channel handle.
 * inSize : size of the input parameters buffer (returned by the server
 *          kernel driver).
 *          a size of 0 means no local input parameters buffer is needed.
 * outSize: size of the output parameters buffer (returned by the server
 *          kernel driver).
 *          a size of 0 means no local output parameters buffer is needed.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqLocalBufInfo (VrpqChanHandle chan, VrpqSize* inSize,
		      VrpqSize* outSize);

/*
 * Receive a big procedure request using local parameters buffers allocated
 * by the caller.
 *
 * It may happen that a parameters buffer does not fit in the client/server
 * shared memory (see 'vrpqLocalBufInfo()' for details).
 * In this case, the corresponding procedure request cannot be received using
 * 'vrpqReceive()'. It must be received using 'vrpqLocalBufReceive()' instead.
 * The daemon should first call 'vrpqLocalBufInfo()' to know the size of the
 * parameters buffers, then it should allocate those buffers and finally,
 * it should call 'vrpqLocalBufReceive()' providing the allocated buffers.
 * The server kernel driver is in charge of transporting the big procedure
 * request and copying the parameters from/to the local daemon buffers.
 * When returning from this function, the output parameters buffer is
 * formatted as detailed in the 'VrpqShmParamExt' comments.
 *
 * chan: channel handle.
 * req : received procedure request (returned by the server kernel driver).
 * in  : input parameters (allocated by the caller and filled in by the 
 *       VRPQ layer).
 * out : output parameters (allocated by the caller and formatted by the 
 *       VRPQ layer).
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqLocalBufReceive (VrpqChanHandle chan, VrpqProcReq* req,
			 VrpqShmParamInPtr in, VrpqShmParamOutPtr out);

#ifdef __cplusplus
}
#endif

#endif /* _VLX_VRPQ_SERVER_LIB_H */

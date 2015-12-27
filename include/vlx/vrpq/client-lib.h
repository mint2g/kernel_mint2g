/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Remote Procedure Queue (VRPQ).                    *
 *             Client VRPQ library interface.                                *
 *                                                                           *
 *  This file provides definitions and prototypes that are exported by the   *
 *  client VRPQ library.                                                     *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VRPQ_CLIENT_LIB_H
#define _VLX_VRPQ_CLIENT_LIB_H

#include <vlx/vrpq/client-drv.h>

/*
 * Encode (serialize) a basic input parameter.
 *
 * The input parameter must be of any of the basic types: char, short, int,
 * long, long long...
 *
 * buffer: the input parameter buffer, as returned by 'vrpqParamInAlloc()'.
 * offset: the offset in the input parameter buffer where the parameter
 *         should be placed.
 *         IMPORTANT: the offset must be aligned on the size of the type of
 *         the parameter.
 * type  : the type of the parameter.
 * data  : the parameter value.
 */
#define VRPQ_PARAM_WRITE(buffer, offset, type, data)			\
    do {								\
	(void)(((void*)0) == (buffer));					\
	*((type*)(((unsigned char*) (buffer)) + (offset))) = (data);	\
    } while (0)

/*
 * Encode (serialize) one element of an array.
 *
 * If a buffer is an array, it can be more efficient to serialize
 * all elements rather than using memcpy (through 'VRPQ_PARAM_BUF_WRITE').
 *
 * buffer: the input parameter buffer, as returned by 'vrpqParamInAlloc()'.
 * offset: the offset in the input parameter buffer where the array should
 *         be placed.
 * type  : the type of the array element.
 * data  : the array address.
 * index : the index of the element to be serialized.
 */
#define VRPQ_PARAM_ARRAY_WRITE(buffer, offset, type, data, index)	\
    do {								\
	(void)(((void*)0) == (buffer));					\
	((type*) (((unsigned char*) (buffer)) + (offset)))[index] =	\
	    ((type*) (data))[index];					\
    } while (0)

/*
 * Encode (serialize) an input data buffer.
 *
 * buffer: the input parameter buffer, as returned by 'vrpqParamInAlloc()'.
 * offset: the offset in the input parameter buffer where the data should
 *         be placed.
 * data  : the data address.
 * len   : the data length.
 */
#define VRPQ_PARAM_BUF_WRITE(buffer, offset, data, len)			\
    do {								\
	(void)(((void*)0) == (buffer));					\
	memcpy(((unsigned char*) (buffer)) + (offset), data, len);	\
    } while (0)

/*
 * Encode (serialize) an input null-terminated string.
 *
 * buffer: the input parameter buffer, as returned by 'vrpqParamInAlloc()'.
 * offset: the offset in the input parameter buffer where the string should
 *         be placed.
 * str   : the string address.
 */
#define VRPQ_PARAM_STR_WRITE(buffer, offset, str)			\
    do {								\
	(void)(((void*)0) == (buffer));					\
	strcpy(((char*) (buffer)) + (offset), str);			\
    } while (0)

/*
 * Encode an input parameter reference.
 *
 * buffer: the input parameter buffer, as returned by 'vrpqParamInAlloc()'.
 * offset: the offset in the input parameter buffer where the reference
 *         should be placed.
 * size  : the size (in bytes) of the parameter content.
 * ptr   : the address of the parameter content.
 */
#define VRPQ_PARAM_REF_WRITE(buffer, offset, __size, __ptr)		\
    do {								\
	VrpqParamRef* __ref =						\
	    (VrpqParamRef*)(((unsigned char*) (buffer)) + (offset));	\
	(void)(((void*)0) == (buffer));					\
	(void)(((void*)0) == (__ptr));					\
	__ref->size = __size;						\
	__ref->uptr = (void*)(__ptr);					\
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Create a client/server session.
 *
 * A session is used as a reference to a client/server connection that
 * enables the creation and use of communication channels.
 *
 * devName: the vrpq device name.
 * session: a handle that describes the newly created session is returned.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqSessionCreate (const char* devName, VrpqSessionHandle* session);

/*
 * Destroy a session previously created by 'vrpqSessionCreate()'.
 *
 * session: session handle.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqSessionDestroy (VrpqSessionHandle session);

/*
 * Create a bidirectional communication channel for a particular client/server
 * session.
 *
 * A channel can be used by multiple concurrent threads. Those threads must
 * belong to the client of the underlying session.
 *
 * session: session handle.
 * chan   : a handle that describes the newly created channel is returned.
 *          It must be specified in subsequent VRPQ calls.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqChanCreate (VrpqSessionHandle session, VrpqChanHandle* chan);

/*
 * Destroy a channel previously created by 'vrpqChanCreate()'.
 *
 * chan: channel handle.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqChanDestroy (VrpqChanHandle chan);

/*
 *
 */
VrpqParamInPtr vrpqParamPostAlloc (VrpqChanHandle chan, VrpqSize size,
				   unsigned int refCount, VrpqSize refSize);

/*
 *
 */
int vrpqParamCallAlloc (VrpqChanHandle chan, VrpqSize inSize,
			unsigned int inRefCount, VrpqSize inRefSize,
			unsigned int outCount, VrpqSize outSize,
			VrpqParamInPtr *pin, VrpqParamOutPtr *pout);

/*
 * Send a notification to the channel server in order to serve the pending
 * asynchronous procedure requests (those which have been posted).
 *
 * chan: channel handle.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqNotify (VrpqChanHandle chan);

/*
 * Post an asynchronous procedure request to the channel.
 *
 * The procedure is queued but no explicit notification is sent to the server.
 * The server is free to handle the procedure or not.
 * The 'vrpqPost()' function returns immediately, there is no output
 * parameters.
 * Exceptionally, the 'vrpqPost()' function may block if there is not enough
 * space in the client/server shared memory (the message ring or the input
 * parameters pool is full). In this case, the client waits for the server
 * to consume some requests and thus to free the corresponding resources.
 * The 'vrpqPost()' function may also block if the input parameters are too
 * big and require to be transported in multiple messages.
 *
 * This primitive is free to buffer requests at user level in order to 
 * perform multiple procedure requests in a single kernel invocation.
 * Requests that have been buffered at user level are actually put in the
 * client/server shared memory at kernel level if one of the four following
 * conditions is met:
 * - the user level buffer is full.
 * - the 'vrpqNotify()' primitive is called.
 * - the 'vrpqPostAndNotify()' primitive is called.
 * - the 'vrpqCall primitive()' is called.
 *
 * Calling 'vrpqPost()' automatically frees the buffer of input parameters
 * allocated by 'vrpqParamInAlloc()'.
 *
 * chan  : channel handle.
 * procId: procedure ID (group + function).
 * in    : pointer to a buffer of serialized input parameters.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqPost (VrpqChanHandle chan, VrpqProcId procId, VrpqParamInPtr in);

/*
 * Post an asynchronous procedure request to the channel and notify the
 * server.
 *
 * See vrpqPost for usage.
 */
int vrpqPostAndNotify (VrpqChanHandle chan, VrpqProcId procId,
		       VrpqParamInPtr in);

/*
 * Send a synchronous procedure request to the channel and wait for the
 * corresponding result.
 *
 * Any asynchronous procedures that have been posted previously are guaranteed
 * to be handled by the server prior to this call (in the same order as they
 * have been issued).
 *
 * Calling 'vrpqCall()' automatically frees the buffer of input parameters
 * allocated by 'vrpqParamInAlloc()' and the buffer of output parameters
 * allocated by 'vrpqParamOutAlloc()'.
 *
 * chan   : channel handle.
 * procId : procedure ID (group + function).
 * in     : pointer to a buffer of serialized input parameters.
 * out    : pointer to a description of output parameters.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vrpqCall (VrpqChanHandle chan, VrpqProcId procId,
	      VrpqParamInPtr in, VrpqParamOutPtr out);

/*
 *
 */
int vrpqProf (const char* devName, int cmd, VrpqStat* stats, int count);

#ifdef __cplusplus
}
#endif

#endif /* _VLX_VRPQ_CLIENT_LIB_H */

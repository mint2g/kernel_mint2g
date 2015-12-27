/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual User MEMory Buffers (VUMEM).                      *
 *             Client VUMEM library interface.                               *
 *                                                                           *
 *  This file provides definitions and prototypes that are exported by the   *
 *  client VUMEM library.                                                    *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VUMEM_CLIENT_LIB_H
#define _VLX_VUMEM_CLIENT_LIB_H

#include <vlx/vumem/client-drv.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Local client/server session handle.
 *
 * A session is a limited period of time during which a client is connected
 * to a server in order to communicate. Client/server messages are exchanged
 * through an implicit (to the session) and unique communication channel.
 * Resources created in the context of a session exist as long as the session
 * is alive. Such resources are freed upon session termination.
 * Such session handle is local to the user entity.
 */
typedef void* VumemSessionHandle;

/*
 * Local buffer handle.
 *
 * Such handle enables to locally control a buffer that has been remotely
 * allocated.
 */
typedef void* VumemBufferHandle;

/*
 * Create a client/server session.
 *
 * A session is used as a reference to a client/server connection that
 * enables the exchange of messages and the allocation of resources relative
 * to the session.
 *
 * devName: the vumem device name.
 * session: a handle that describes the newly created session is returned.
 *
 * This operation is handled locally by the VUMEM front-end driver and thus
 * does not cause a message to be sent to the VUMEM back-end driver.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemSessionCreate (const char* devName, VumemSessionHandle* session);

/*
 * Create a master client/server session.
 *
 * The following rules apply for each operating system:
 * - If there is no master session, all sessions are allowed to perform buffer
 *   allocations.
 * - If a particular client wants to control buffer allocations, it has to
 *   create a master session. Then only the client of the master session
 *   will be allowed to perform buffer allocations.
 *   The client that holds the master session and thus that control buffer
 *   allocations has to transfer buffer handles to processes that want to
 *   use those buffers.
 *
 * devName: the vumem device name.
 * session: a handle that describes the newly created session is returned.
 *
 * This operation is handled locally by the VUMEM front-end driver and thus
 * does not cause a message to be sent to the VUMEM back-end driver.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemSessionMasterCreate (const char* devName,
			      VumemSessionHandle* session);

/*
 * Destroy a session previously created by vumemSessionCreate or by
 * vumemSessionMasterCreate.
 *
 * session: session handle.
 *
 * This operation may cause messages to be sent to the VUMEM back-end driver
 * and thus to the server that is connected to that back-end driver.
 * In that case, this primitive blocks until the response has been received
 * from the server.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemSessionDestroy (VumemSessionHandle session);

/*
 * Allocate a buffer.
 *
 * The buffer allocation request is described by the size and the usage of the
 * buffer. The buffer usage depicts how the application will use that buffer.
 * It enables the server which performs the buffer allocation to know the
 * properties of the buffer memory and thus to perform the allocation in the
 * appropriate memory pool.
 *
 * If there is a master session, only the client of that session can allocate
 * buffers. Otherwise, any session client can do buffer allocations.
 *
 * A buffer is bound to the session which has been used to allocate it
 * (referred to as the "allocation session" in the following).
 * Consequently, a buffer is automatically freed when the underlying
 * "allocation session" is destroyed.
 *
 * A buffer can always be mapped or unmapped by the client which has
 * allocated it. But a buffer can also be made available to another client,
 * so that this new client can map or unmap the buffer as well.
 *
 * Transferring a buffer from a client to another client is done by
 * transferring its handle. Such operation is not obvious because it
 * requires the handle to be usable in the context of the new client.
 * On Android system, a buffer handle contains a file descriptor which
 * represents the entry point to the VUMEM driver for that buffer.
 * In this case, transferring a handle between two clients means transferring
 * the underlying file descriptor between two processes.
 * On Android, such operation is performed by the IPC service with the
 * assistance of the binder (with some services provided by the kernel).
 *
 * Once a buffer handle has been transferred, the new client has an access
 * point to the VUMEM driver for using that buffer. But this new client
 * cannot map or unmap the newly transferred buffer until it has registered
 * this buffer into its own session. This operation is achieved by the
 * vumemBufferRegister primitive.
 * Once a buffer has been made available to a new client, both the client
 * of the "allocation session" and the new client (which has registered
 * the buffer) can map and unmap the buffer. This buffer is thus shared
 * by these two clients.
 *
 * But only the client that has allocated a buffer can actually free it,
 * by an explicit call to vumemBufferFree or implicitly through a session
 * termination.
 * Consequently, a buffer that have been registered by a client (and not
 * allocated) cannot be freed by such client.
 *
 * It is guaranteed that when a buffer is freed (explicitly or implicitly),
 * it cannot remain mapped anymore. It means that all mappings that
 * were still referring this buffer are removed authoritatively (mapping
 * revocation).
 * 
 * This operation causes a message to be sent to the VUMEM back-end driver
 * and thus to the server that is connected to that back-end driver.
 * This primitive blocks until the result has been received from the server.
 *
 * session: session handle.
 * alloc  : opaque allocation request that will be transfered to the server.
 * size   : allocation request size in bytes.
 * buffer : a handle that describes the newly allocated buffer is returned.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemBufferAlloc (VumemSessionHandle session, void* alloc,
		      VumemSize size, VumemBufferHandle* buffer);

/*
 * Register a foreign buffer in the session of the current client.
 *
 * A foreign buffer is a buffer that has not been allocated by the current
 * client but which has been transferred to it.
 * Registering a foreign (or transferred) buffer in the session of the
 * current client is required for being able to map or unmap it.
 * A buffer cannot be freed by the client which registered it. Only the
 * client which allocated it can free it.
 * A buffer should be unmapped by the "registerer" client before it is freed
 * by the "allocator" client. Otherwise, the free operation will cause all
 * existing mappings to be revoked authoritatively (and potentially will
 * cause the "registerer" client to fault).
 *
 * buffer : transferred buffer handle.
 * session: session handle.
 *
 * This operation causes a message to be sent to the VUMEM back-end driver
 * and thus to the server that is connected to that back-end driver.
 * This primitive blocks until the response has been received from the server.
 *
 * The server is informed that a new client ID has requested access to
 * the buffer. The server can for instance maintain an access control list
 * (ACL) for each buffer. This can be used to request operations on a buffer
 * through its global ID rather than through a local handle. A local buffer
 * handle is an unforgeable reference (capacity) whereas a global buffer ID
 * is a forgeable reference and thus require ACL management.
 * Such global buffer ID usage is not described here but could be an
 * extension of the VUMEM API or could be a service provided by the server
 * connected to the VUMEM back-end driver.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemBufferRegister (VumemSessionHandle session, VumemBufferHandle buffer);

/*
 * Unregister a buffer previously registered by vumemBufferRegister.
 *
 * Only the "registerer" client can actually unregister a buffer (and remove
 * its access permissions from the server).
 *
 * This operation causes a message to be sent to the VUMEM back-end driver
 * and thus to the server that is connected to that back-end driver.
 * This primitive blocks until the response has been received from the server.
 *
 * buffer: buffer handle.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemBufferUnregister (VumemBufferHandle buffer);

/*
 * Free a buffer previously allocated by vumemBufferAlloc.
 *
 * Only the "allocator" client can actually free a buffer (and give the
 * buffer memory back to the server).
 *
 * An "registerer" client can call the vumemBufferFree primitive in order
 * to indicate it will no more use the buffer. In this case, only the
 * buffer handle is freed. If the buffer was mapped, the mapping is not
 * removed. It will be revoked when the "allocator" client actually
 * frees the buffer.
 *
 * After the buffer has been freed, it is guaranteed that all existing
 * mappings to the buffers are revoked.
 *
 * This operation causes a message to be sent to the VUMEM back-end driver
 * and thus to the server that is connected to that back-end driver.
 * This primitive blocks until the response has been received from the server.
 *
 * buffer: buffer handle.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemBufferFree (VumemBufferHandle buffer);

/*
 * Query the global ID of a buffer.
 *
 * Each buffer is identified by a global and unique ID across the platform.
 * It enables any component to reference that buffer.
 * This primitive allows to retrieve the global ID of a buffer whose local
 * handle is available.
 *
 * This operation is handled locally by the VUMEM front-end driver and thus
 * does not cause a message to be sent to the VUMEM back-end driver.
 *
 * buffer  : buffer handle.
 * bufferId: the global buffer ID is returned.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemBufferIdGet (VumemBufferHandle buffer, VumemBufferId* bufferId);

/*
 * Map a buffer in the current process address space.
 *
 * A buffer can be mapped by an "allocator" or an "registerer" client.
 *
 * This operation is handled locally by the VUMEM front-end driver and thus
 * does not cause a message to be sent to the VUMEM back-end driver.
 *
 * buffer: buffer handle.
 * vaddr : the virtual address the buffer is mapped to is returned.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemBufferMap (VumemBufferHandle buffer, void** vaddr);

/*
 * Unmap a buffer previously mapped by vumemBufferMap.
 *
 * This operation is handled locally by the VUMEM front-end driver and thus
 * does not cause a message to be sent to the VUMEM back-end driver.
 *
 * buffer: buffer handle.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemBufferUnmap (VumemBufferHandle buffer);

/*
 * Perform a cache maintenance operation, such as writing the buffer data
 * back to the physical memory.
 *
 * This operation is handled locally by the VUMEM front-end driver and thus
 * does not cause a message to be sent to the VUMEM back-end driver.
 *
 * buffer : buffer handle.
 * cacheOp: cache operation (VUMEM_CACHE_*).
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemBufferCacheCtl (VumemBufferHandle buffer, VumemCacheOp cacheOp);

/*
 *
 */
int vumemBufferCtl (VumemBufferHandle buffer, VumemCtlOp cmd, void* arg,
		    VumemSize size);

/*
 *
 */
int vumemProf (const char* devName, int cmd, VumemStat* stats, int count);

#ifdef __cplusplus
}
#endif

#endif /* _VLX_VUMEM_CLIENT_LIB_H */

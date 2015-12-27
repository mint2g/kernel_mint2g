/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual User MEMory Buffers (VUMEM).                      *
 *             Server VUMEM library interface.                               *
 *                                                                           *
 *  This file provides definitions and prototypes that are exported by the   *
 *  server VUMEM library.                                                    *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VUMEM_SERVER_LIB_H
#define _VLX_VUMEM_SERVER_LIB_H

#include <vlx/vumem/server-drv.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Local server handle.
 *
 * The VUMEM driver is a generic driver that can be used for different
 * kinds of user memory management. For instance, VGRALLOC will be implemented
 * on top of VUMEM in order to provide VOpenGL ES with graphics memory
 * management. VVIDEO could be provided with video memory management using
 * VUMEM too.
 * The VUMEM architecture imposes that only one "/dev/vumem" file is created
 * for each kind of user memory management (in both the front-end and the
 * back-end guest OSes). For instance, "/dev/vumem-vgralloc" and
 * "/dev/vumem-vvideo" could be used.
 * The VUMEM architecture also imposes that only one server is connected
 * to each "/dev/vumem" file and thus serves the corresponding kind of
 * user memory management.
 *
 * The local server handle represents the connection of one server to
 * a particular "/dev/vumem" file, and thus to all clients of the
 * corresponding user memory management kind.
 */
typedef void* VumemServerHandle;

/*
 * Create a local server handle.
 *
 * devName: the vumem device name.
 * server : a handle that describes the server connection is returned.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemServerCreate (const char* devName, VumemServerHandle* server);

/*
 * Destroy a local server handle created by vumemServerCreate.
 *
 * server: server handle.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemServerDestroy (VumemServerHandle server);

/*
 * Receive a client request.
 *
 * Client requests are emitted by the front-end kernel driver. They
 * correspond to the services provided by the client library.
 *
 * Each request contains:
 * - the client ID that created the session used for calling the library.
 * - the command that corresponds to the primitive called in the library.
 * - the input parameters of that command.
 *
 * This primitive blocks until a new client request is received.
 *
 * server: server handle.
 * req   : client request.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemReceive (VumemServerHandle server, VumemReq* req);

/*
 * Send a server response.
 *
 * The server sends a response to the back-end kernel driver which forwards
 * it to the front-end kernel driver. The latter returns to the client
 * library to provide expected results.
 *
 * Each response contains:
 * - a generic return value.
 * - the specific result of the command.
 *
 * server: server handle.
 * resp  : server response.
 *
 * On success, zero is returned. On error, an error code is returned.
 */
int vumemRespond (VumemServerHandle server, VumemResp* resp);

#ifdef __cplusplus
}
#endif

#endif /* _VLX_VUMEM_SERVER_LIB_H */

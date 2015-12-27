/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Remote Procedure Queue (VRPQ).                    *
 *             Definitions for server VRPQ entities.                         *
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

#ifndef _VLX_VRPQ_SERVER_DRV_H
#define _VLX_VRPQ_SERVER_DRV_H

#include <vlx/vrpq/common.h>

/*
 * Extended parameter as seen through the client/server shared memory.
 *
 * Extended parameters are installed by the VRPQ layer into the client-
 * -server shared memory from parameter references provided by the client.
 * Parameter references are converted into extended parameters as follows:
 *
 *    +----+----+----+
 *    |Size|Size|Size|
 *    |Ptr |Ptr |Ptr |
 *    +----+----+----+
 *          ||
 *          ||
 *          \/
 *    +----+--....--+----+--....--+----+--....--+
 *    |Size|  Data  |Size|  Data  |Size|  Data  |
 *    +----+--....--+----+--....--+----+--....--+
 *
 * Each extended parameter is composed of a size followed by the content of
 * the parameter.
 */
typedef struct VrpqShmParamExt {
    VrpqSize size;    /* Size of parameter in bytes */
    nku32_f  pad;
    nku8_f   val[0];
} VrpqShmParamExt;

/*
 * Pointer to a buffer of input parameters, as seen through the client/server
 * shared memory.
 *
 * Input parameters are serialized in the client/server shared memory as
 * follows:
 *
 *   +--------------------------+----+--....--+----+--....--+----+--....--+
 *   |                          |Size|  Data  |Size|  Data  |Size|  Data  |
 *   +--------------------------+----+--....--+----+--....--+----+--....--+
 *   \____________  ___________/\____________________  ___________________/
 *                \/                                 \/
 *        Serialized (opaque)            Extended input parameters
 *         input parameters     (were provided as references by the client)
 *
 * "Opaque" input parameters are formatted by the caller of the client VRPQ
 * library whereas extended input parameters are formatted by the VRPQ layer
 * from parameter references provided by the client.
 */
typedef void* VrpqShmParamInPtr;

/*
 * Pointer to a buffer of output parameters, as seen through the client/server
 * shared memory.
 *
 * Output parameters are placed in the client/server shared memory as follows:
 *
 *    +----+----+...+----+--------------.....-------+
 *    |Size|Size|   |Size| Data | Data |     | Data |
 *    |Ptr |Ptr |   |Ptr |      |      |     |      |
 *    +----+----+...+----+--------------.....-------+
 *     \________  ______/\____________  ____________/
 *              \/                    \/
 *      Output parameters       Output parameters
 *         references                 data
 *
 * Output parameters references are provided and installed in the client/server
 * shared memory by the client. The output parameters data are then placed
 * by the server into the client/server shared memory right after the
 * references. These data will be copied back to the client memory using
 * the references.
 */
typedef VrpqParamRef* VrpqShmParamOutPtr;

/*
 * Procedure request descriptor, as seen through the client/server shared
 * memory.
 *
 * The client kernel driver builds a descriptor for each procedure request
 * and sends it to the server kernel driver through a shared ring.
 * When a request is received, the server kernel driver notifies the server
 * daemon and provides it with a pointer to the request descriptor in the
 * shared ring.
 * It means that the user space daemon has direct access (read-only) to the
 * shared ring and that extra copies are avoided.
 *
 * The procedure request descriptor is made of:
 * - the procedure ID (group + function).
 * - a reference to the buffer containing the serialized input parameters
 *   (residing in the shared memory directly accessible by the user space
 *   daemon).
 * - a reference to the buffer where the output parameters should be written
 *   (residing in the shared memory directly accessible by the user space
 *   daemon).
 */
typedef struct VrpqProcReq {
    VrpqProcId procId;
    VrpqSize   inOffset;
    VrpqSize   outOffset;
} VrpqProcReq;	/* 12 bytes */

/*
 *
 */

#define VRPQ_SRV_IOCTL(c, s)			\
    VRPQ_IOCTL((c) + VRPQ_IOCTL_CLT_CMD_MAX, s)

#define VRPQ_IOCTL_SESSION_ACCEPT		VRPQ_SRV_IOCTL(0, 0)
#define VRPQ_IOCTL_CHAN_ACCEPT			VRPQ_SRV_IOCTL(1, 0)
#define VRPQ_IOCTL_SHM_MAP			VRPQ_SRV_IOCTL(2, 0)
#define VRPQ_IOCTL_CLIENT_ID_GET		VRPQ_SRV_IOCTL(3, 0)
#define VRPQ_IOCTL_RECEIVE(c)			VRPQ_SRV_IOCTL(4, c)
#define VRPQ_IOCTL_CALL_SET_REASON		VRPQ_SRV_IOCTL(5, 0)

#endif /* _VLX_VRPQ_SERVER_DRV_H */

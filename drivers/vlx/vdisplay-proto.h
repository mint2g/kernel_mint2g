/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Display (VDISP).                                  *
 *             VDISP front-end/back-end communication protocol definitions.  *
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
 *    Chi Dat Truong    <chidat.truong@redbend.com>                          *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VDISP_PROTO_H
#define _VLX_VDISP_PROTO_H

#include <vlx/vdisplay/client-drv.h>
#include <vlx/vdisplay/server-drv.h>

#define VDISP_RESC_PDEV         0        /* 1 pdev chunk  */
#define VDISP_RESC_XIRQ_CLT     1        /* 1 client xirq */
#define VDISP_RESC_XIRQ_SRV     2        /* 1 server xirq */

/* Extended RPC commands only used by DRV */
#define VDISP_CMD_SET_POWER     (VDISP_CMD_NR + 1)
#define VDISP_CMD_GET_PROPERTY  (VDISP_CMD_NR + 2)

/*
 *
 */
typedef struct VdispRpcShm {
    nku32_f             reqId;
    nku32_f             rspId;
    VdispSize           size;
    nku32_f             err;
    nku8_f              data[];
} VdispRpcShm;

typedef struct VdispDrvReq {
    VdispReqHeader       header;
    union {
        PanelPowerState  screenPowerState;
    };
} VdispDrvReq;

typedef struct VdispDrvResp {
    unsigned int         retVal;
    union {
        DisplayProperty  displayProperty;
    };
} VdispDrvResp;

#endif /* _VLX_VDISP_PROTO_H */

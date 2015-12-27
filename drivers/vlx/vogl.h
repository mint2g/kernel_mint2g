/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual OpenGL ES (VOGL).                                 *
 *             Kernel-specific VOGL front-end/back-end drivers definitions.  *
 *                                                                           *
 *  This file provides definitions that enable to implement the VOGL         *
 *  front-end and back-end drivers in the Linux kernel.                      *
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

#ifndef _VLX_VOGL_H
#define _VLX_VOGL_H

#include "vrpq.h"
#include "vumem.h"

#define VOGL_CLT_MAJOR	230
#define VOGL_SRV_MAJOR	232

#define VOGL_VRPQ_PMEM_SIZE_DFLT	(7 * 1024 * 1024)
#define VOGL_VRPQ_REQS_DEFLT		(4 * 1024)
#define VOGL_VUMEM_PDEV_SIZE_DFLT	(16 * 1024)

extern int vogl_vrpq_prop_get  (Vlink* vlink, unsigned int type, void* prop);
extern int vogl_vumem_prop_get (Vlink* vlink, unsigned int type, void* prop);

#endif /* _VLX_VOGL_H */

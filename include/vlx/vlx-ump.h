/*
 ****************************************************************
 *
 *  Component: VLX User Mode virtual driver Proxy driver
 *
 *  Copyright (C) 2011, Red Bend Ltd.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  You should have received a copy of the GNU General Public License Version 2
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *  Contributor(s):
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

#ifndef VLX_UMP_H
#define VLX_UMP_H

#include <asm/ioctl.h>

#define UMPIOC_GET_VERSION	_IO('y',   1)	/* obsolete */
#define UMPIOC_READ_NKOSCTX	_IO('y',   2)	/* obsolete */
#define UMPIOC_FIND_OSCTX_BANK	_IOW('y',  3, ump_ioctl_t)
#define UMPIOC_READ_VIRT	_IOR('y',  4, ump_ioctl_t)	/* obsolete */
#define UMPIOC_FIND_OSCTX	_IOW('y',  5, ump_ioctl_t)
#define UMPIOC_VTOP		_IOWR('y', 6, ump_ioctl_t)
#define UMPIOC_AWAIT_XIRQ	_IO('y',   7)
#define UMPIOC_XIRQ_ATTACH	_IO('y',   8)
#define UMPIOC_XIRQ_DETACH	_IO('y',   9)
#define UMPIOC_PDEV_ALLOC	_IOWR('y', 10, ump_resource_t)
#define UMPIOC_PMEM_ALLOC	_IOWR('y', 11, ump_resource_t)
#define UMPIOC_PXIRQ_ALLOC	_IOWR('y', 12, ump_resource_t)
#define UMPIOC_DEV_ALLOC	_IOWR('y', 13, ump_ioctl_t)
#define UMPIOC_DEV_ADD		_IO('y',   14)
#define UMPIOC_XIRQ_ALLOC	_IO('y',   15)
#define UMPIOC_XIRQ_TRIGGER	_IO('y',   16)
#define UMPIOC_XIRQ_MASK	_IO('y',   17)
#define UMPIOC_XIRQ_UNMASK	_IO('y',   18)
#define UMPIOC_ATOMIC_CLEAR	_IOR('y',  19, ump_ioctl_t)
#define UMPIOC_ATOMIC_CLEAR_AND_TEST	_IOWR('y', 20, ump_ioctl_t)
#define UMPIOC_ATOMIC_SET	_IOR('y',  21, ump_ioctl_t)
#define UMPIOC_ATOMIC_SUB	_IOR('y',  22, ump_ioctl_t)
#define UMPIOC_ATOMIC_SUB_AND_TEST	_IOWR('y', 23, ump_ioctl_t)
#define UMPIOC_ATOMIC_ADD	_IOR('y',  24, ump_ioctl_t)
#define UMPIOC_MEM_COPY_TO	_IOR('y',  25, ump_ioctl_t)
#define UMPIOC_MEM_COPY_FROM	_IOR('y',  26, ump_ioctl_t)
#define UMPIOC_SMP_TIME		_IOW('y',  27, ump_ioctl_t)
#define UMPIOC_SMP_TIME_HZ	_IO('y',   28)
#define UMPIOC_NKDDI_VERSION	_IO('y',   29)
#define UMPIOC_MAX			   30

#define UMPIOC_NAMES {\
    "Invalid",         "GetVersion",     "ReadNkOsCtx", \
    "FindOsCtxBank",   "ReadVirt",       "FindOsCtx", \
    "VtoP",            "AwaitXirq",      "XirqAttach", \
    "XirqDetach",      "PdevAlloc",      "PmemAlloc", \
    "PxirqAlloc",      "DevAlloc",       "DevAdd", \
    "XirqAlloc",       "XirqTrigger",    "XirqMask", \
    "XirqUnmask",      "AtomicClear",    "AtoClearAndTest", \
    "AtomicSet",       "AtomicSub",      "AtoSubAndTest", \
    "AtomicAdd",       "MemCopyTo",      "MemCopyFrom", \
    "SmpTime",         "SmpTimeHz",      "NkDdiVersion"}

typedef struct {
    unsigned long	addr;
    unsigned int	size;
    void*		buf;
} ump_ioctl_t;

typedef struct {
    NkPhAddr	vlink;
    NkResource	nkresrc;
} ump_resource_t;

#endif

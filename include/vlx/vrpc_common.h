/*
 ****************************************************************
 *
 *  Component: VLX virtual remote procedure call driver interface
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
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *
 ****************************************************************
 */

#ifndef _VRPC_COMMON_H_
#define _VRPC_COMMON_H_

typedef struct vrpc_pmem_t {
    nku32_f req;	/* request counter */
    nku32_f ack;	/* acknowledge counter */
    nku32_f size;	/* size of RPC in/out data */
    nku32_f data[];	/* RPC data */
} vrpc_pmem_t;

#define	VRPC_PMEM_DEF_SIZE	1024

#endif

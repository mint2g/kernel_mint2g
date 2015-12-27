/*
 ****************************************************************
 *
 *  Component: VLX generic nano-kernel interface (for device drivers)
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

#ifndef	_D_NKERN_H
#define	_D_NKERN_H

#include <asm/nk/nk_f.h>
#include <nk/nk.h>
#include <nk/nkdev.h>

extern NkDevOps nkops;	/* Nano-Kernel DDI Operations */

extern void printnk (const char* fmt, ...);

#endif

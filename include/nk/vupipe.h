/*
 ****************************************************************
 *
 *  Component: VLX virtual user-space pipe driver
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
 ****************************************************************
 */

#ifndef _VUPIPE_H
#define _VUPIPE_H

#include <asm/io.h>

#define PIOC_READ_NOTIFY     	_IOW('y', 1, int)
#define PIOC_WRITE_NOTIFY  	_IOW('y', 2, int)
#define PIOC_AVAILABLE_DATA 	_IOR('y', 3, int)
#define PIOC_AVAILABLE_ROOM 	_IOR('y', 4, int)
#define PIOC_GET_BUFFER_SIZE	_IOR('y', 5, int)
#define PIOC_GET_OFFSET		_IOR('y', 6, int)

#endif

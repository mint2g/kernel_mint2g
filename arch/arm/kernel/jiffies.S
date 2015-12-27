/*
 *
 *  Copyright (C) 2004-2011 RedBend.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

	.data

	.globl	jiffies_64
	.globl	jiffies

	.align	6
jiffies_64:
#ifndef __ARMEB__
jiffies:
#endif
	.space	4
#ifdef __ARMEB__
jiffies:
#endif
	.space	4


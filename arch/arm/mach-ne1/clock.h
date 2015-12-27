/*
 * linux/arch/arm/mach-ne1/clock.h
 *
 * Copyright (C) NEC Electronics Corporation 2007, 2008
 *
 * This file is based on arch/arm/mach-realview/clock.h
 *
 * Copyright (C) 2004 ARM Limited.
 * Written by Deep Blue Solutions Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ARCH_CLOCK_H
#define __ARCH_CLOCK_H


struct module;
struct ne1_clock_params;

struct clk {
	struct list_head node;
	unsigned long rate;
	struct module *owner;
	const char *name;
	struct clk *parent;
	unsigned int bit;
	unsigned int div_reg;
};

int clk_register(struct clk *clk);
void clk_unregister(struct clk *clk);


#endif /* __ARCH_CLOCK_H */

/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARCH_ARM_SC8810_CLOCK_SC8800G2_H
#define __ARCH_ARM_SC8810_CLOCK_SC8800G2_H

extern const struct clkops clkops_null;

int sc88xx_clksel_rournd_rate_clkr(struct clk *clk, unsigned long target_rate,
					const struct clksel_rate **clkrp, u32 *valid_rate);
long sc88xx_clksel_round_rate(struct clk *clk, unsigned long target_rate);
int __init sc8810_clock_init(void);
int sc88xx_set_rate_generic(struct clk *clk, unsigned long rate);
unsigned long sc88xx_recalc_generic(struct clk *clk);
int sc8810_get_clock_status(void);
int sc8810_get_clock_info(void);

#endif

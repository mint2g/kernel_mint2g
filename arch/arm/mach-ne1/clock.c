/*
 * linux/arch/arm/mach-ne1/clock.c
 *
 * Copyright (C) NEC Electronics Corporation 2007, 2008
 *
 * This file is based on arch/arm/mach-realview/clock.c
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/mutex.h>

// #include <asm/semaphore.h>

#include <asm/io.h>
#include <mach/ne1_sysctrl.h>

#include "clock.h"


static LIST_HEAD(clocks);
static DEFINE_MUTEX(clocks_mutex);
static DEFINE_SPINLOCK(clocks_lock);

struct clk *clk_get(struct device *dev, const char *id)
{
	struct clk *p, *clk = ERR_PTR(-ENOENT);

	mutex_lock(&clocks_mutex);
	list_for_each_entry(p, &clocks, node) {
		if (strcmp(id, p->name) == 0 && try_module_get(p->owner)) {
			clk = p;
			break;
		}
	}
	mutex_unlock(&clocks_mutex);

	return clk;
}
EXPORT_SYMBOL(clk_get);

void clk_put(struct clk *clk)
{
	if (clk && !IS_ERR(clk))
		module_put(clk->owner);
}
EXPORT_SYMBOL(clk_put);

int clk_enable(struct clk *clk)
{
	unsigned int val;
	unsigned long flags;

	if (!clk)
		return -EINVAL;

	if (clk->bit) {
		spin_lock_irqsave(&clocks_lock, flags);
		val = readl(SYSCTRL_BASE + SYSCTRL_CLKMSK);
		writel(val & ~(clk->bit), SYSCTRL_BASE + SYSCTRL_CLKMSK);
		spin_unlock_irqrestore(&clocks_lock, flags);
	}
	return 0;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	unsigned int val;
	unsigned long flags;

	if (!clk)
		return;

	if (clk->bit) {
		spin_lock_irqsave(&clocks_lock, flags);
		val = readl(SYSCTRL_BASE + SYSCTRL_CLKMSK);
		writel(val | clk->bit, SYSCTRL_BASE + SYSCTRL_CLKMSK);
		spin_unlock_irqrestore(&clocks_lock, flags);
	}
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	return rate;
}
EXPORT_SYMBOL(clk_round_rate);

#define LCD_BASE_CLOCK	400000000
static int set_display_div(unsigned int rate)
{
	unsigned int div;

	if (rate > (((LCD_BASE_CLOCK/5) + (LCD_BASE_CLOCK/6)) / 2)) {
		div = 0;
	} else if (rate > (((LCD_BASE_CLOCK/6) + (LCD_BASE_CLOCK/7)) / 2)) {
		div = 1;
	} else if (rate > (((LCD_BASE_CLOCK/7) + (LCD_BASE_CLOCK/8)) / 2)) {
		div = 2;
	} else if (rate > (((LCD_BASE_CLOCK/8) + (LCD_BASE_CLOCK/9)) / 2)) {
		div = 3;
	} else if (rate > (((LCD_BASE_CLOCK/9) + (LCD_BASE_CLOCK/10)) / 2)) {
		div = 4;
	} else if (rate > (((LCD_BASE_CLOCK/10) + (LCD_BASE_CLOCK/11)) / 2)) {
		div = 5;
	} else if (rate > (((LCD_BASE_CLOCK/11) + (LCD_BASE_CLOCK/12)) / 2)) {
		div = 6;
	} else if (rate > (((LCD_BASE_CLOCK/12) + (LCD_BASE_CLOCK/13)) / 2)) {
		div = 7;
	} else if (rate > (((LCD_BASE_CLOCK/13) + (LCD_BASE_CLOCK/14)) / 2)) {
		div = 8;
	} else if (rate > (((LCD_BASE_CLOCK/14) + (LCD_BASE_CLOCK/15)) / 2)) {
		div = 9;
	} else if (rate > (((LCD_BASE_CLOCK/15) + (LCD_BASE_CLOCK/16)) / 2)) {
		div = 10;
	} else if (rate > (((LCD_BASE_CLOCK/16) + (LCD_BASE_CLOCK/17)) / 2)) {
		div = 11;
	} else if (rate > (((LCD_BASE_CLOCK/17) + (LCD_BASE_CLOCK/18)) / 2)) {
		div = 12;
	} else if (rate > (((LCD_BASE_CLOCK/18) + (LCD_BASE_CLOCK/19)) / 2)) {
		div = 13;
	} else if (rate > (((LCD_BASE_CLOCK/19) + (LCD_BASE_CLOCK/10)) / 2)) {
		div = 14;
	} else {
		div = 15;
	}
	writel(div, SYSCTRL_BASE + SYSCTRL_CLKDIV_DISP);

	return LCD_BASE_CLOCK / (div + 5);
}

#define SPDIF_BASE_CLOCK	48000000
static int set_spdif_div(unsigned int rate)
{
	unsigned int div;

	if (rate > ((SPDIF_BASE_CLOCK + (SPDIF_BASE_CLOCK/2)) / 2)) {
		div = 0;
	} else if (rate > (((SPDIF_BASE_CLOCK/2) + (SPDIF_BASE_CLOCK/4)) / 2)) {
		div = 1;
	} else if (rate > (((SPDIF_BASE_CLOCK/4) + (SPDIF_BASE_CLOCK/6)) / 2)) {
		div = 2;
	} else if (rate > (((SPDIF_BASE_CLOCK/6) + (SPDIF_BASE_CLOCK/8)) / 2)) {
		div = 3;
	} else {
		div = 4;
	}

	writel(div, SYSCTRL_BASE + SYSCTRL_CLKDIV_SPDIF);
	return SPDIF_BASE_CLOCK / (div + 1);
}

static int set_i2s_div(unsigned int rate, int num)
{
	unsigned int div, val;

	switch (rate) {
	case 36864000: div = 0; break;
	case 24576000: div = 1; break;
	case 18432000: div = 2; break;
	case 33868800: div = 4; break;
	case 22579200: div = 5; break;
	case 16934400: div = 6; break;
	default: return -EINVAL;
	}

	val = readl(SYSCTRL_BASE + SYSCTRL_CLKDIV_I2S);
	val &= ~(0xf << (num * 8));
	div <<= (num*8);
	writel(val | div, SYSCTRL_BASE + SYSCTRL_CLKDIV_I2S);

	return rate;
}


static int set_div(struct clk *clk, unsigned long rate)
{
	unsigned int reg = clk->div_reg;
	int new_rate;
	int num;

	switch (reg) {
	case SYSCTRL_BASE + SYSCTRL_CLKDIV_DISP:
		new_rate = set_display_div(rate);
		break;
	case SYSCTRL_BASE + SYSCTRL_CLKDIV_SPDIF:
		new_rate = set_spdif_div(rate);
		break;
	case SYSCTRL_BASE + SYSCTRL_CLKDIV_I2S:
		if (strcmp(clk->name, "I2S0") == 0) {
			num = 0;
		} else if (strcmp(clk->name, "I2S1") == 0) {
			num = 1;
		} else if (strcmp(clk->name, "I2S2") == 0) {
			num = 2;
		} else {
			num = 3;
		}
		new_rate = set_i2s_div(rate, num);
		break;
	default:
		return -EIO;
	}

	if (new_rate < 0) {
		return -EINVAL;
	}
	clk->rate = new_rate;

	return 0;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret;
	unsigned long flags;

	if (!clk)
		return -EINVAL;

	if (clk->div_reg) {
		spin_lock_irqsave(&clocks_lock, flags);
		ret = set_div(clk, rate);
		spin_unlock_irqrestore(&clocks_lock, flags);
	} else {
		ret = -EIO;
	}

	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

struct clk *clk_get_parent(struct clk *clk)
{
	struct clk * ret = NULL;

	if (clk == NULL || IS_ERR(clk))
		return ret;

	if (clk->parent != NULL) {
		ret = clk->parent;
	}
	return ret;
}
EXPORT_SYMBOL(clk_get_parent);


static struct clk pll1_clk = {	/* PLL1 clock */
	.name	= "PLL1",
	.rate	= 399996000,
	.bit	= 0,
};
static struct clk pll2_clk = {	/* PLL2 clock */
	.name	= "PLL2",
	.rate	= 266664000,
	.bit	= 0,
};

static struct clk mpcore_clk_clk = {	/* MPCore global clock */
	.name	= "MPCORE_CLK",
	.rate	= 399996000,
	.bit	= 0,
	.parent	= &pll1_clk,
};

static struct clk ddr2_mclk_clk = {	/* DDR2 memory clock */
	.name	= "DDR2_MCLK",
	.rate	= 266664000,
	.bit	= 0,
	.parent	= &pll2_clk,
};

static struct clk aclk_clk = {		/* AXI/AHB bus clock */
	.name	= "ACLK",
	.rate	= 133332000,
	.bit	= 0,
	.parent	= &pll2_clk,
};

static struct clk pclk_clk = {		/* APB bus clock */
	.name	= "PCLK",
	.rate	= 66666000,
	.bit	= 0,
	.parent	= &pll2_clk,
};

static struct clk pci_clk_clk = {	/* PCI bus clock */
	.name	= "PCI_CLK",
	.rate	= 33333000,
	.bit	= SYSCTRL_CLKMSK_PCI,
	.parent	= &pll2_clk,
};

static struct clk ide_clk = {	/* IDE clock */
	.name	= "IDE",
	.rate	= 100000000,
	.bit	= SYSCTRL_CLKMSK_IDE,
	.parent	= &pll1_clk,
};
static struct clk disp_clk = {	/* Display DOT clock */
	.name	= "DISP",
	.rate	= 66666666,
	.bit	= SYSCTRL_CLKMSK_DISP,
	.div_reg = SYSCTRL_BASE + SYSCTRL_CLKDIV_DISP,
	.parent	= &pll1_clk,
};
static struct clk i2c_clk = {	/* I2C clock */
	.name	= "I2C",
	.rate	= 4166000,
	.bit	= SYSCTRL_CLKMSK_I2C,
	.parent	= &pll2_clk,
};
static struct clk spdmclk_clk = {	/* SPDIF clock */
	.name	= "SPDMCLK",
	.rate	= 36864000,
	.bit	= SYSCTRL_CLKMSK_SPDMCLK,
};
static struct clk spdclko_clk = {	/* SPDIF clock */
	.name	= "SPDCLKO",
	.rate	= 48000000,
	.bit	= SYSCTRL_CLKMSK_SPDCLKO,
	.div_reg = SYSCTRL_BASE + SYSCTRL_CLKDIV_SPDIF,
};
static struct clk spdclko_in_clk = {	/* SPDIF clock */
	.name	= "SPDCLKO_IN",
	.rate	= 48000000,
	.bit	= SYSCTRL_CLKMSK_SPDCLKO_IN,
};
static struct clk i2s0_clk = {	/* I2S clock */
	.name	= "I2S0",
	.rate	= 36864000,
	.bit	= SYSCTRL_CLKMSK_I2S0,
	.div_reg = SYSCTRL_BASE + SYSCTRL_CLKDIV_I2S,
};
static struct clk i2s1_clk = {	/* clock */
	.name	= "I2S1",
	.rate	= 36864000,
	.bit	= SYSCTRL_CLKMSK_I2S1,
	.div_reg = SYSCTRL_BASE + SYSCTRL_CLKDIV_I2S,
};
static struct clk i2s2_clk = {	/* clock */
	.name	= "I2S2",
	.rate	= 36864000,
	.bit	= SYSCTRL_CLKMSK_I2S2,
	.div_reg = SYSCTRL_BASE + SYSCTRL_CLKDIV_I2S,
};
static struct clk i2s3_clk = {	/* clock */
	.name	= "I2S3",
	.rate	= 36864000,
	.bit	= SYSCTRL_CLKMSK_I2S3,
	.div_reg = SYSCTRL_BASE + SYSCTRL_CLKDIV_I2S,
};


int clk_register(struct clk *clk)
{
	mutex_lock(&clocks_mutex);
	list_add(&clk->node, &clocks);
	mutex_unlock(&clocks_mutex);
	return 0;
}
EXPORT_SYMBOL(clk_register);

void clk_unregister(struct clk *clk)
{
	mutex_lock(&clocks_mutex);
	list_del(&clk->node);
	mutex_unlock(&clocks_mutex);
}
EXPORT_SYMBOL(clk_unregister);

static int __init clk_init(void)
{
	clk_register(&pll1_clk);
	clk_register(&pll2_clk);
	clk_register(&mpcore_clk_clk);
	clk_register(&ddr2_mclk_clk);
	clk_register(&aclk_clk);
	clk_register(&pclk_clk);
	clk_register(&pci_clk_clk);
	clk_register(&ide_clk);
	clk_register(&disp_clk);
	clk_register(&i2c_clk);
	clk_register(&spdmclk_clk);
	clk_register(&spdclko_clk);
	clk_register(&spdclko_in_clk);
	clk_register(&i2s0_clk);
	clk_register(&i2s1_clk);
	clk_register(&i2s2_clk);
	clk_register(&i2s3_clk);

	return 0;
}
arch_initcall(clk_init);


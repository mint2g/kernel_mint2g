/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/cpufreq.h>

#include <mach/sci.h>
#include <mach/hardware.h>
#include <mach/regs_glb.h>
#include <mach/regs_ahb.h>

#include "clock.h"
#include "mach/__clock_tree.h"

const u32 __clkinit0 __clkinit_begin = 0xeeeebbbb;
const u32 __clkinit2 __clkinit_end = 0xddddeeee;

DEFINE_SPINLOCK(clocks_lock);

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	if (IS_ERR_OR_NULL(clk))
		return -EINVAL;

	clk_enable(clk->parent);

	spin_lock_irqsave(&clocks_lock, flags);
	if ((clk->usage++) == 0 && clk->enable)
		(clk->enable) (clk, 1, &flags);
	spin_unlock_irqrestore(&clocks_lock, flags);
	debug0("clk %p, usage %d\n", clk, clk->usage);
	return 0;
}

EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	unsigned long flags;
	if (IS_ERR_OR_NULL(clk))
		return;

	spin_lock_irqsave(&clocks_lock, flags);
	if ((--clk->usage) == 0 && clk->enable)
		(clk->enable) (clk, 0, &flags);
	if (WARN_ON(clk->usage < 0)) {
		clk->usage = 0;	/* FIXME: force reset clock refcnt */
		spin_unlock_irqrestore(&clocks_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&clocks_lock, flags);
	debug0("clk %p, usage %d\n", clk, clk->usage);
	clk_disable(clk->parent);
}

EXPORT_SYMBOL(clk_disable);

/**
 * clk_force_disable - force disable clock output
 * @clk: clock source
 *
 * Forcibly disable the clock output.
 * NOTE: this *will* disable the clock output even if other consumer
 * devices have it enabled. This should be used for situations when device
 * suspend or damage will likely occur if the devices is not disabled.
 */
void clk_force_disable(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk))
		return;

	debug("clk %p, usage %d\n", clk, clk->usage);
	while (clk->usage > 0) {
		clk_disable(clk);
	}
}

EXPORT_SYMBOL(clk_force_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	debug0("clk %p, rate %lu\n", clk, IS_ERR_OR_NULL(clk) ? -1 : clk->rate);
	if (IS_ERR_OR_NULL(clk))
		return 0;

/*
	if (clk->rate != 0)
		return clk->rate;
*/

	if (clk->ops != NULL && clk->ops->get_rate != NULL)
		return (clk->ops->get_rate) (clk);

	if (clk->parent != NULL)
		return clk_get_rate(clk->parent);

	return clk->rate;
}

EXPORT_SYMBOL(clk_get_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (!IS_ERR_OR_NULL(clk) && clk->ops && clk->ops->round_rate)
		return (clk->ops->round_rate) (clk, rate);

	return rate;
}

EXPORT_SYMBOL(clk_round_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret;
	unsigned long flags;
	debug0("clk %p, rate %lu\n", clk, rate);
	if (IS_ERR_OR_NULL(clk) || rate == 0)
		return -EINVAL;

	/* We do not default just do a clk->rate = rate as
	 * the clock may have been made this way by choice.
	 */

	//WARN_ON(clk->ops == NULL);
	//WARN_ON(clk->ops && clk->ops->set_rate == NULL);

	if (clk->ops == NULL || clk->ops->set_rate == NULL)
		return -EINVAL;

	spin_lock_irqsave(&clocks_lock, flags);
	ret = (clk->ops->set_rate) (clk, rate);
	spin_unlock_irqrestore(&clocks_lock, flags);
	return ret;
}

EXPORT_SYMBOL(clk_set_rate);

struct clk *clk_get_parent(struct clk *clk)
{
	return clk->parent;
}

EXPORT_SYMBOL(clk_get_parent);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = -EACCES;
	unsigned long flags;
	struct clk *old_parent = clk_get_parent(clk);
	debug0("clk %p, parent %p <<< %p\n", clk, parent, old_parent);
	if (IS_ERR_OR_NULL(clk) || IS_ERR(parent))
		return -EINVAL;

	spin_lock_irqsave(&clocks_lock, flags);
	if (clk->ops && clk->ops->set_parent)
		ret = (clk->ops->set_parent) (clk, parent);
	spin_unlock_irqrestore(&clocks_lock, flags);

#if defined(CONFIG_DEBUG_FS)
	/* FIXME: call debugfs_rename() out of spin lock,
	 * maybe not match with the real parent-child relationship
	 * in some extreme scenes.
	 */
	if (0 == ret && old_parent && old_parent->dent && clk->dent
	    && parent && parent->dent) {
		debug0("directory dentry move %s to %s\n",
		       old_parent->regs->name, parent->regs->name);
		debugfs_rename(old_parent->dent, clk->dent,
			       parent->dent, clk->regs->name);
	}
#endif
	return ret;
}

EXPORT_SYMBOL(clk_set_parent);

static int sci_clk_enable(struct clk *c, int enable, unsigned long *pflags)
{
	debug("clk %p (%s) enb %08x, %s\n", c, c->regs->name,
	      c->regs->enb.reg, enable ? "enable" : "disable");

	BUG_ON(!c->regs->enb.reg);
	if (c->regs->enb.reg & 1)
		enable = !enable;

	if (!c->regs->enb.mask) {	/* enable matrix clock */
		if (pflags)
			spin_unlock_irqrestore(&clocks_lock, *pflags);
		if (enable)
			clk_enable((struct clk *)c->regs->enb.reg);
		else
			clk_disable((struct clk *)c->regs->enb.reg);
		if (pflags)
			spin_lock_irqsave(&clocks_lock, *pflags);
	} else {
		if (enable)
			sci_glb_set(c->regs->enb.reg & ~1, c->regs->enb.mask);
		else
			sci_glb_clr(c->regs->enb.reg & ~1, c->regs->enb.mask);
	}
	return 0;
}

static int sci_clk_is_enable(struct clk *c)
{
	int enable;

	debug0("clk %p (%s) enb %08x\n", c, c->regs->name, c->regs->enb.reg);

	BUG_ON(!c->regs->enb.reg);
	if (!c->regs->enb.mask) {	/* check matrix clock */
		enable = ! !sci_clk_is_enable((struct clk *)c->regs->enb.reg);
	} else {
		enable =
		    ! !sci_glb_read(c->regs->enb.reg & ~1, c->regs->enb.mask);
	}

	if (c->regs->enb.reg & 1)
		enable = !enable;
	return enable;
}

static int sci_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 div, div_shift;
	debug("clk %p (%s) set rate %lu\n", c, c->regs->name, rate);
	rate = clk_round_rate(c, rate);
	div = clk_get_rate(c->parent) / rate - 1;	//FIXME:
	div_shift = __ffs(c->regs->div.mask);
	debug0("clk %p (%s) pll div reg %08x, val %08x mask %08x\n", c,
	       c->regs->name, c->regs->div.reg, div << div_shift,
	       c->regs->div.mask);
	sci_glb_write(c->regs->div.reg, div << div_shift, c->regs->div.mask);

	c->rate = 0;		/* FIXME: auto update all children after new rate if need */
	return 0;
}

static unsigned long sci_clk_get_rate(struct clk *c)
{
	u32 div = 0, div_shift;
	unsigned long rate;
	div_shift = __ffs(c->regs->div.mask);
	debug0("clk %p (%s) div reg %08x, shift %u msk %08x\n", c,
	       c->regs->name, c->regs->div.reg, div_shift, c->regs->div.mask);
	rate = clk_get_rate(c->parent);

	if (c->regs->div.reg)
		div = sci_glb_read(c->regs->div.reg,
				   c->regs->div.mask) >> div_shift;
	debug0("clk %p (%s) parent rate %lu, div %u\n", c, c->regs->name, rate,
	       div + 1);
	c->rate = rate = rate / (div + 1);	//FIXME:
	debug0("clk %p (%s) get real rate %lu\n", c, c->regs->name, rate);
	return rate;
}

#define SHFT_PLL_REFIN                 ( 16 )
#define MASK_PLL_REFIN                 ( BIT(16)|BIT(17) )
static unsigned long sci_pll_get_refin_rate(struct clk *c)
{
	int i;
	const unsigned long refin[4] = { 2, 4, 4, 13 };	/* default refin 4M */
	i = sci_glb_read(c->regs->div.reg, MASK_PLL_REFIN) >> SHFT_PLL_REFIN;
	debug0("pll %p (%s) refin %d\n", c, c->regs->name, i);
	return refin[i] * 1000000;
}

static unsigned long sci_pll_get_rate(struct clk *c)
{
	u32 mn = 1, mn_shift;
	unsigned long rate;
	mn_shift = __ffs(c->regs->div.mask);
	debug0("pll %p (%s) mn reg %08x, shift %u msk %08x\n", c, c->regs->name,
	       c->regs->div.reg, mn_shift, c->regs->div.mask);
	rate = clk_get_rate(c->parent);
	if (0 == c->regs->div.reg) ;
	else if (c->regs->div.reg < MAX_DIV) {
		mn = c->regs->div.reg;
		if (mn)
			rate = rate / mn;
	} else {
		rate = sci_pll_get_refin_rate(c);
		mn = sci_glb_read(c->regs->div.reg,
				  c->regs->div.mask) >> mn_shift;
		if (mn)
			rate = rate * mn;
	}
	c->rate = rate;
	debug0("pll %p (%s) get real rate %lu\n", c, c->regs->name, rate);
	return rate;
}

static unsigned long sci_clk_round_rate(struct clk *c, unsigned long rate)
{
	debug0("clk %p (%s) round rate %lu\n", c, c->regs->name, rate);
	return rate;
}

static int sci_clk_set_parent(struct clk *c, struct clk *parent)
{
	int i;
	debug0("clk %p (%s) parent %p (%s)\n", c, c->regs->name,
	       parent, parent ? parent->regs->name : 0);

	for (i = 0; i < c->regs->nr_sources; i++) {
		if (c->regs->sources[i] == parent) {
			u32 sel_shift = __ffs(c->regs->sel.mask);
			debug0("pll sel reg %08x, val %08x, msk %08x\n",
			       c->regs->sel.reg, i << sel_shift,
			       c->regs->sel.mask);
			if (c->regs->sel.reg)
				sci_glb_write(c->regs->sel.reg, i << sel_shift,
					      c->regs->sel.mask);
			c->parent = parent;
			if (c->ops)
				c->rate = 0;	/* FIXME: auto update clock rate after new parent */
			return 0;
		}
	}

	WARN_ON(1);
	return -EINVAL;
}

static int sci_clk_get_parent(struct clk *c)
{
	int i = 0;
	u32 sel_shift = __ffs(c->regs->sel.mask);
	debug0("pll sel reg %08x, val %08x, msk %08x\n",
	       c->regs->sel.reg, i << sel_shift, c->regs->sel.mask);
	if (c->regs->sel.reg) {
		i = sci_glb_read(c->regs->sel.reg,
				 c->regs->sel.mask) >> sel_shift;
	}
	return i;
}

static struct clk_ops generic_clk_ops = {
	.set_rate = sci_clk_set_rate,
	.get_rate = sci_clk_get_rate,
	.round_rate = sci_clk_round_rate,
	.set_parent = sci_clk_set_parent,
};

static struct clk_ops generic_pll_ops = {
	.set_rate = 0,
	.get_rate = sci_pll_get_rate,
	.round_rate = 0,
	.set_parent = sci_clk_set_parent,
};

/* debugfs support to trace clock tree hierarchy and attributes */
#if defined(CONFIG_DEBUG_FS)
static struct dentry *clk_debugfs_root;
static int __init clk_debugfs_register(struct clk *c)
{
	char name[NAME_MAX], *p = name;
	p += sprintf(p, "%s", c->regs->name);

	if (IS_ERR_OR_NULL((c->dent =
			    debugfs_create_dir(name,
					       c->parent ? c->parent->dent :
					       clk_debugfs_root))))
		goto err_exit;
	if (IS_ERR_OR_NULL(debugfs_create_u32
			   ("usecount", S_IRUGO, c->dent, (u32 *) & c->usage)))
		goto err_exit;
	if (IS_ERR_OR_NULL(debugfs_create_u32
			   ("rate", S_IRUGO, c->dent, (u32 *) & c->rate)))
		goto err_exit;
	return 0;
err_exit:
	if (c->dent)
		debugfs_remove_recursive(c->dent);
	return -ENOMEM;
}
#endif

static __init int __clk_is_dummy_pll(struct clk *c)
{
	return (c->regs->enb.reg & 1) || strstr(c->regs->name, "pll");
}

int __init sci_clk_register(struct clk_lookup *cl)
{
	struct clk *c = cl->clk;

	if (c->ops == NULL) {
		c->ops = &generic_clk_ops;
		if (c->rate)	/* fixed OSC */
			c->ops = NULL;
		else if ((c->regs->div.reg >= 0 && c->regs->div.reg < MAX_DIV)
			 || strstr(c->regs->name, "pll")) {
			c->ops = &generic_pll_ops;
		}
	}

	debug0
	    ("clk %p (%s) rate %lu ops %p enb %08x sel %08x div %08x nr_sources %u\n",
	     c, c->regs->name, c->rate, c->ops, c->regs->enb.reg,
	     c->regs->sel.reg, c->regs->div.reg, c->regs->nr_sources);

	if (c->enable == NULL && c->regs->enb.reg) {
		c->enable = sci_clk_enable;
		/* FIXME: dummy update some pll clocks usage */
		if (sci_clk_is_enable(c) && __clk_is_dummy_pll(c)) {
			clk_enable(c);
		}
	}

	if (!c->rate) {		/* FIXME: dummy update clock parent and rate */
		clk_set_parent(c, c->regs->sources[sci_clk_get_parent(c)]);
		/* clk_set_rate(c, clk_get_rate(c)); */
	}

	clkdev_add(cl);

#if defined(CONFIG_DEBUG_FS)
	clk_debugfs_register(c);
#endif
	return 0;
}

static int __init sci_clock_dump(void)
{
	struct clk_lookup *cl = (struct clk_lookup *)(&__clkinit_begin + 1);
	while (cl < (struct clk_lookup *)&__clkinit_end) {
		struct clk *c = cl->clk;
		struct clk *p = clk_get_parent(c);
		printk
		    ("@@@clock[%s] is %sactive, usage %d, rate %lu, parent[%s]\n",
		     c->regs->name,
		     (c->enable == NULL || sci_clk_is_enable(c)) ? "" : "in",
		     c->usage, clk_get_rate(c), p ? p->regs->name : "none");
		cl++;
	}
	return 0;
}

static int
__clk_cpufreq_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;

	printk("%s (%u) dump cpu freq (%u %u %u %u)\n",
	       __func__, (unsigned int)val,
	       freq->cpu, freq->old, freq->new, (unsigned int)freq->flags);

	return 0;
}

static struct notifier_block __clk_cpufreq_notifier_block = {
	.notifier_call = __clk_cpufreq_notifier
};

int __init sci_clock_init(void)
{

#if defined(CONFIG_DEBUG_FS)
	clk_debugfs_root = debugfs_create_dir("clock", NULL);
	if (IS_ERR_OR_NULL(clk_debugfs_root))
		return -ENOMEM;
#endif

	/* register all clock sources */
	{
		struct clk_lookup *cl =
		    (struct clk_lookup *)(&__clkinit_begin + 1);
		debug0("%p (%x) -- %p -- %p (%x)\n",
		       &__clkinit_begin, __clkinit_begin, cl, &__clkinit_end,
		       __clkinit_end);
		while (cl < (struct clk_lookup *)&__clkinit_end) {
			sci_clk_register(cl);
			cl++;
		}
	}

	/* keep track of cpu frequency transitions */
	cpufreq_register_notifier(&__clk_cpufreq_notifier_block,
				  CPUFREQ_TRANSITION_NOTIFIER);
	return 0;
}

arch_initcall(sci_clock_init);
late_initcall_sync(sci_clock_dump);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum Clock Driver");
MODULE_AUTHOR("robot <zhulin.lian@spreadtrum.com>");

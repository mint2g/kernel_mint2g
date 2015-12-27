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

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/seq_file.h>
#include "clock_common.h"

static DEFINE_MUTEX(clocks_mutex);
static DEFINE_SPINLOCK(clockfw_lock);
static LIST_HEAD(root_clks);


static struct clk_functions *arch_clock;

/*---------------------------------------------------------
 *
 * platform-dependent functions.
 *
 ---------------------------------------------------------*/
void clk_reparent(struct clk *child, struct clk *parent)
{
	list_del_init(&child->sibling);
	if (parent)
		list_add(&child->sibling, &parent->children);
	child->parent = parent;
}

void sc88xx_init_clksel_parent(struct clk *clk)
{
	const struct clksel *clks;
	u32 r, found = 0;

	if (!clk->clksel_reg || !clk->clksel_mask) {
		printk("clock[%s]: parent can't be changed!\n", clk->name);
		return;
	}

	r = __raw_readl(clk->clksel_reg) & clk->clksel_mask;
	r >>= __ffs(clk->clksel_mask);

	for (clks = clk->clksel; clks->parent && !found; clks++) {
		if (clks->val == r) {
			if (clk->parent != clks->parent) {
				printk("clock: set [%s]'s parent from [%s] to [%s].\n",
					clk->name, clk->parent->name, clks->parent->name);
				clk_reparent(clk, clks->parent);
				if (clk->recalc)
					clk->rate = clk->recalc(clk);
				propagate_rate(clk);
			}
			found = 1;
		}
	}
	if (!found)
		printk("clock: Can find parent for clock [%s].\n", clk->name);
}

int clk_register(struct clk *clk)
{
	if ((NULL == clk) || IS_ERR(clk))
		return -EINVAL;

	if (clk->node.next || clk->node.prev)
		return 0;

	mutex_lock(&clocks_mutex);

	/* handle sibling. */
	if (clk->parent)
		list_add(&clk->sibling, &clk->parent->children);
	else
		list_add(&clk->sibling, &root_clks);

	if (clk->init)
		clk->init(clk);

	mutex_unlock(&clocks_mutex);

	return 0;
}

void clk_unregister(struct clk *clk)
{
	if ((NULL == clk) || IS_ERR(clk))
		return;

	mutex_lock(&clocks_mutex);
	list_del(&clk->sibling);
	list_del(&clk->node);
	mutex_unlock(&clocks_mutex);
}

/* propagate rate to children. */
void propagate_rate(struct clk *tclk)
{
	struct clk *clkp;

	list_for_each_entry(clkp, &tclk->children, sibling) {
		if (clkp->recalc)
			clkp->rate = clkp->recalc(clkp);
		propagate_rate(clkp);
	}
}

void recalculate_root_clocks(void)
{
	struct clk *clkp;

	list_for_each_entry(clkp, &root_clks, sibling) {
		if (clkp->recalc)
			clkp->rate = clkp->recalc(clkp);
		propagate_rate(clkp);
	}
}

void clk_preinit(struct clk *clk)
{
	INIT_LIST_HEAD(&clk->children);
}
int __init clk_init(struct clk_functions *custom_clocks)
{
	if (!custom_clocks) {
		printk("Not valid custom clock\n");
		BUG();
	}
	arch_clock = custom_clocks;
	return 0;
}

/*---------------------------------------------------------
 *
 *  APIs of clock framework.
 *
 ---------------------------------------------------------*/
unsigned long clk_get_rate(struct clk *clk)
{
	unsigned long flags;
	unsigned long ret = -EINVAL;

	if ((NULL == clk) || IS_ERR(clk))
		return ret;

	spin_lock_irqsave(&clockfw_lock, flags);
	ret = clk->rate;
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_get_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	unsigned long ret = -EINVAL;

	if ((NULL == clk) || IS_ERR(clk))
		return ret;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (arch_clock->clk_round_rate)
			ret = arch_clock->clk_round_rate(clk, rate);
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	unsigned long ret = -EINVAL;
	unsigned long rate_before, rate_after;

	if ((NULL == clk) || IS_ERR(clk))
		return ret;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (arch_clock->clk_set_rate)
			ret = arch_clock->clk_set_rate(clk, rate);
	if (0 == ret) {
		rate_after = rate_before = clk->rate;
		if (clk->recalc) {
			clk->rate = clk->recalc(clk);
			rate_after = clk->rate;
		}
		if (rate_before != rate_after) {
			printk("clock[%s]: Rates are not equal after recalc(%lu --> %lu).\n",
				clk->name, rate_before, rate_after);
			ret = -EINVAL;
		}
		propagate_rate(clk);
	}

	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;

}
EXPORT_SYMBOL(clk_set_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	unsigned long flags;
	unsigned long ret = -EINVAL;

	if ((NULL == clk) || IS_ERR(clk))
		return ret;

	if ((NULL == parent) || IS_ERR(parent))
		return ret;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (0 == clk->usecount) {
		if (arch_clock->clk_set_parent)
			ret = arch_clock->clk_set_parent(clk, parent);
		if (0 == ret) {
			if (clk->recalc)
				clk->rate = clk->recalc(clk);
			propagate_rate(clk);
		}
	}
	else
		ret = -EBUSY;

	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;

}
EXPORT_SYMBOL(clk_set_parent);


struct clk *clk_get_parent(struct clk *clk)
{
	return clk->parent;
}
EXPORT_SYMBOL(clk_get_parent);


int clk_enable(struct clk *clk)
{
	unsigned long flags;
	int ret = -EINVAL;

	if ((NULL == clk) || IS_ERR(clk))
		return -EINVAL;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (arch_clock->clk_enable)
		ret = arch_clock->clk_enable(clk);
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_enable);


void clk_disable(struct clk *clk)
{
	unsigned long flags;

	if ((NULL == clk) || IS_ERR(clk))
		return;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (clk->usecount == 0) {
		/*
		printk("Trying to disable clock [%s] with 0 usecount\n",
				clk->name);
		WARN_ON(1);
		*/
		goto out;
	}
	if (arch_clock->clk_disable)
		arch_clock->clk_disable(clk);
out:
	spin_unlock_irqrestore(&clockfw_lock, flags);
}
EXPORT_SYMBOL(clk_disable);

static void _clock_dump(struct list_head * lhead, struct seq_file *s)
{
	struct clk *p ,*cp;
	list_for_each_entry(p, lhead, sibling){
		seq_printf(s, "%-16s%-16s%-16ld%2d              %08x        \n",
			p->name, p->parent?p->parent->name:"NONE", p->rate, p->usecount, p->flags);
		if(p->children.next != &p->children){
			/*
			seq_printf(s, "children :");
			list_for_each_entry(cp, &p->children, sibling){
				seq_printf(s, " %s", cp->name);
			}
			seq_printf(s, "\n");
			*/
			_clock_dump(&p->children, s);
		}
	}
	return ;
}

void clock_dump(struct seq_file *s)
{
	seq_printf(s, "NAME            PARENT          RATE            COUNT           FLAGS\n");
	_clock_dump(&root_clks, s);
	return ;
}


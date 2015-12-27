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

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/clk.h>
#include <asm/uaccess.h>
#include <asm/string.h>

extern void clock_dump(struct seq_file *s);
extern void sc8810_clock_modem_dump(struct seq_file *s);

static struct dentry * dentry_debug_root = NULL;

static ssize_t clock_action_write(struct file *file, const char __user *buf,
	size_t count, loff_t *ppos)
{
	struct clk *clkp, *clkpp;
	unsigned long val;
	char cmd[256];
	int i = 0,ret = 0;
	char *str, *name, *action, *para, *temp;
	size_t cmdsize = count < 255 ? count: 255;

	if(cmdsize == 0)
		return -1;
	i = copy_from_user(cmd, buf, cmdsize);
	if(i < 0){
		printk(KERN_WARNING"clock debug: copy error i = %d\n", i);
		return -1;
	}
	cmd[cmdsize] = 0;
	str = cmd;
	do{
		temp = strsep(&str, " \t\n\r");
		if(temp == NULL || *temp == 0)
			continue;
		switch(i)
		{
		case 0:
			name = temp;
			break;
		case 1:
			action = temp;
			break;
		case 2:
			para = temp;
			break;
		default:
			printk(KERN_WARNING"clock debug: too many parameters\n");
			break;
		}
		i++;
	}while(str);

	if(i != 3){
		printk(KERN_WARNING"clock debug: parameters count should be 3\n");
		return -1;
	}
	clkp = clk_get(NULL, name);
	if(clkp == NULL || IS_ERR(clkp)){
		printk(KERN_WARNING"clock debug: no clock %s\n", name);
		return -1;
	}

	if(!strcmp(action, "enable")){
		val = simple_strtoul(para, NULL, 10);
		if(val)
			ret = clk_enable(clkp);
		else
			clk_disable(clkp);
		printk(KERN_INFO"clock debug: is %s ret = %d\n", val?"enable":"disable", ret);
	}else if(!strcmp(action, "setrate")){
		val = simple_strtoul(para, NULL, 10);
		ret = clk_set_rate(clkp, val);
		printk(KERN_INFO"clock debug: %s set rate to %ld ret = %d\n", name, val, ret);
	}else if(!strcmp(action, "reparent")){
		clkpp = clk_get(NULL, para);
		if(clkpp == NULL || IS_ERR(clkpp)){
			printk(KERN_WARNING"clock debug: no clock #%s#\n", para);
			clk_put(clkp);
			return -1;
		}
		ret = clk_set_parent(clkp, clkpp);
		clk_put(clkpp);
		printk(KERN_INFO"clock debug: action reparent to %s ret = %d\n", para, ret);
	}else{
		printk(KERN_WARNING"clock debug: no atction!!!!\n");
	}
	clk_put(clkp);
	return cmdsize;
}

static ssize_t clock_action_read(struct file *filp, char __user *buf,
        size_t count, loff_t *ppos)
{
	return -1;
}

static int clock_action_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static const struct file_operations clock_action_fops = {
	.open		= clock_action_open,
	.read		= clock_action_read,
	.write		= clock_action_write
};

static int clock_dump_show(struct seq_file *s, void *data)
{
	seq_printf(s, "clock info dump\n");
	seq_printf(s, "linux clock------------------\n");
	clock_dump(s);
	seq_printf(s, "modem clock------------------\n");
	sc8810_clock_modem_dump(s);
	seq_printf(s, "--------------------------------\n");
	return 0;
}

static int clock_dump_open(struct inode *inode, struct file *file)
{
        return single_open(file, clock_dump_show, NULL);
}

static const struct file_operations clock_dump_fops = {
	.open		= clock_dump_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init clock_debug_init(void)
{
	struct dentry *d;
	dentry_debug_root = debugfs_create_dir("clock", NULL);
	if (IS_ERR(dentry_debug_root) || !dentry_debug_root) {
		pr_err("!!!powermanager Failed to create debugfs directory\n");
		dentry_debug_root = NULL;
		return -ENOMEM;
	}
	d = debugfs_create_file("dump", 0744, dentry_debug_root, NULL,
		&clock_dump_fops);
	if (!d) {
		pr_err("Failed to create clock debug file\n");
		return -ENOMEM;
	}
	d = debugfs_create_file("action", 0744, dentry_debug_root, NULL,
		&clock_action_fops);
	if (!d) {
		pr_err("Failed to create clock debug file\n");
		return -ENOMEM;
	}
	return 0;
}

late_initcall(clock_debug_init);

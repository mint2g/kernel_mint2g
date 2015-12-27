/*
 ****************************************************************
 *
 *  Component: VLX console history access driver
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
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

/*----- Header files -----*/

#include <linux/version.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>

#include <nk/nkern.h>

/*----- Local configuration -----*/

#if 1
#define HISTORY_DEBUG
#endif

/*----- Traces -----*/

#define HISTORY_MSG		"VLX-HISTORY: "

#define TRACE(x...)	printk (KERN_NOTICE  HISTORY_MSG x)
#define WTRACE(x...)	printk (KERN_WARNING HISTORY_MSG "Warning: " x)
#define ETRACE(x...)	printk (KERN_ERR     HISTORY_MSG "Error: " x)

#ifdef HISTORY_DEBUG
#define DTRACE(x...)	printk (KERN_CRIT   HISTORY_MSG x)
#else
#define DTRACE(x...)
#endif

/*----- Implementation -----*/

#define HISTORY_NAME	"vlx-history"
#define HISTORY_POS(x)	(*(unsigned*) &((x)->private_data))

static unsigned history_start;

    static int
history_open (struct inode* inode, struct file* file)
{
	/* Mark file as non-seekable. Never fails */
    nonseekable_open (inode, file);
	/* Find first valid offset */
    while (!os_ctx->hgetc (os_ctx, history_start)) ++history_start;
	/* Set reading position to it */
    HISTORY_POS (file) = history_start;
    return 0;
}

    static ssize_t
history_read (struct file* file, char __user* buf, size_t count, loff_t* ppos)
{
    char __user* p = buf;

    for (;;) {
	while (count) {
	    int ch = os_ctx->hgetc (os_ctx, HISTORY_POS (file));

	    if (ch < 0) break;
	    ++HISTORY_POS (file);
	    if (!ch) {
		    /* Update history start for the future */
		history_start = HISTORY_POS (file);
		continue;
	    }
	    if (__put_user ((char) ch, p)) return -EFAULT;
	    ++p;
	    --count;
	}
	if (p - buf > 0) break;
	set_current_state (TASK_INTERRUPTIBLE);
	schedule_timeout (HZ/2);
        if (signal_pending (current)) {
	    DTRACE ("interrupted by signal\n");
	    return -EINTR;
	}
    }
    return p - buf;
}

static const struct file_operations history_fops = {
    .owner	= THIS_MODULE,
    .open	= history_open,
    .read	= history_read
};

static struct miscdevice history_miscdevice = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = HISTORY_NAME,
    .fops  = &history_fops
};

/*----- Initialization and termination -----*/

    static int
history_module_init (void)
{
    int	diag;

    if ((diag = misc_register (&history_miscdevice)) != 0) {
	ETRACE ("cannot register misc device (%d)\n", diag);
	return diag;
    }
    TRACE ("module loaded\n");
    return 0;
}

    static void
history_module_exit (void)
{
    misc_deregister (&history_miscdevice);
    TRACE ("module unloaded\n");
}

/*----- Module glue -----*/

module_init (history_module_init);
module_exit (history_module_exit);

MODULE_DESCRIPTION ("VLX console history access driver");
MODULE_AUTHOR      ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_LICENSE     ("GPL");

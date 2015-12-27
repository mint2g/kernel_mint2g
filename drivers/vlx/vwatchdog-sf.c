/*
 ****************************************************************
 *
 *  Component: VLX virtual watchdog timer client driver
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
 *    Pascal Piovesan (pascal.piovesan@redbend.com)
 *    Chi Dat Truong (chidat.truong@redbend.com)
 *
 ****************************************************************
 */

#include <linux/version.h>   /* LINUX_VERSION_CODE */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/reboot.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <nk/nkern.h>

#if 0
#define DTRACE(fmt, args...)  \
	printk(KERN_CRIT PFX "%s(%d): " fmt,  \
	__FUNCTION__ , __LINE__ , ## args);
#else
#define DTRACE(fmt, args...)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
extern char saved_command_line[];
#endif

MODULE_DESCRIPTION("VLX virtual watchdog timer front-end driver");
MODULE_AUTHOR("Pascal Piovesan <pascal.piovesan@redbend.com>");
MODULE_LICENSE("GPL");

#define PFX		"VWDT: "
#define TIMER_MARGIN	60		/* (secs) Default is 1 minute */

static int soft_margin = TIMER_MARGIN;	/* in seconds */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
MODULE_PARM(soft_margin,"i");
#else
module_param(soft_margin, int, 0);
#endif

MODULE_PARM_DESC(soft_margin, " integer\n\t\t"
		 "  virtual watchdog margin in second ("
		 __MODULE_STRING(TIMER_MARGIN) ")");

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
MODULE_PARM(nowayout,"i");
#else
module_param(nowayout, int, 0444);
#endif

static int _firstwaittime = 60;   /* wait time for first sync */
MODULE_PARM_DESC(nowayout, " integer\n\t\t"
		 "  watchdog cannot be stopped once started ("
		 __MODULE_STRING(CONFIG_WATCHDOG_NOWAYOUT)")");

static unsigned long timer_alive;
static NkDevWdt*     nkwdt;
static NkOsId        nkwdt_owner;

static int nk_wdt_lookup (void);

static char banner[] __initdata = KERN_INFO
                    "VLX virtual watchdog timer started (margin %d sec)\n";

	static int
nksoftdog_open(struct inode *inode, struct file *file)
{
	NkOsId   id   = nkops.nk_id_get();
	NkOsMask mask = nkops.nk_bit2mask(id);

	if(test_and_set_bit(0, &timer_alive)) {
		return -EBUSY;
	}
	if (nowayout) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
		MOD_INC_USE_COUNT;
#else
		__module_get(THIS_MODULE);
#endif
	}
	/*
	 * Start the virtual watchdog
	 */
	nkops.nk_atomic_set(&(nkwdt->enabled), mask);
	nkops.nk_xirq_trigger(nkwdt->xirq, nkwdt_owner);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	return 0;
#else
	return nonseekable_open(inode, file);
#endif
}

	static int
nksoftdog_release(struct inode *inode, struct file *file)
{
	NkOsId   id   = nkops.nk_id_get();
	NkOsMask mask = nkops.nk_bit2mask(id);

	if (!nowayout) {
		/*
		 * Stop the virtual watchdog
		 */
		nkops.nk_atomic_clear(&(nkwdt->enabled), mask);
		nkops.nk_xirq_trigger(nkwdt->xirq, nkwdt_owner);
		clear_bit(0, &timer_alive);
		return 0;
	} else {
		printk(KERN_CRIT PFX "WDT device closed unexpectedly.  WDT will not stop!\n");
		return 1;
	}
}

	static ssize_t
nksoftdog_write(struct file *file, const char *data, size_t len, loff_t *ppos)
{
	NkOsId   id   = nkops.nk_id_get();
	NkOsMask mask = nkops.nk_bit2mask(id);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	/* Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos) {
		return -ESPIPE;
	}
#endif

	/*
	 * Pat the watchdog.
	 */
	if(len) {
		DTRACE("Pat the watchdog\n");
		nkops.nk_atomic_set(&(nkwdt->pat), mask);
		return len;
	}
	return 0;
}

	static int
nksoftdog_ioctl(struct inode *inode, struct file *file,
                       unsigned int cmd, unsigned long arg)
{
	NkOsId   id   = nkops.nk_id_get();
	NkOsMask mask = nkops.nk_bit2mask(id);
	int      new_margin;
	int      old_margin = soft_margin;
	static struct watchdog_info ident = {
	        WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE,
	        0,
	        "VLX virtual watchdog timer"
	};

	if (!nkwdt) return -ENOTTY;
	switch (cmd) {
	default:
		return -ENOTTY;
	case WDIOC_GETSUPPORT:
		if(copy_to_user((struct watchdog_info *)arg,
			&ident, sizeof(ident))) {
			return -EFAULT;
	    }
		return 0;
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0,(int *)arg);
	case WDIOC_KEEPALIVE:
		nkops.nk_atomic_set(&(nkwdt->pat), mask);
		return 0;
	case WDIOC_SETTIMEOUT:
		if (get_user(new_margin, (int *)arg)) {
			return -EFAULT;
		}
		if (new_margin < 1 || nowayout) {
			return -EINVAL;
		}
		/* Stop the virtual watchdog */
		nkops.nk_atomic_clear(&(nkwdt->enabled), mask);
		nkops.nk_xirq_trigger(nkwdt->xirq, nkwdt_owner);
		/* Request for a new watchdog */
		soft_margin = new_margin;
		if (nk_wdt_lookup() < 0) {
			/* Retry to connect to the old watchdog */
			soft_margin = old_margin;
			if (nk_wdt_lookup() < 0) {
				return -EIO;
			}
			/* Restart the old virtual watchdog */
			nkops.nk_atomic_set(&(nkwdt->enabled), mask);
			nkops.nk_xirq_trigger(nkwdt->xirq, nkwdt_owner);
			return -EINVAL;
		}
		printk(PFX "VLX virtual watchdog timer started (margin %d sec)\n", soft_margin);
		/* Start the new virtual watchdog */
		nkops.nk_atomic_set(&(nkwdt->enabled), mask);
		nkops.nk_xirq_trigger(nkwdt->xirq, nkwdt_owner);
		/* Fall */
	case WDIOC_GETTIMEOUT:
	    return put_user(soft_margin, (int *)arg);
	}
}

	static int
nk_wdt_lookup (void)
{
	NkPhAddr pdev = 0;
	int      waiting;
	DECLARE_WAIT_QUEUE_HEAD(device_wait);

	for (waiting = 0; waiting < _firstwaittime; waiting++) {
		while ((pdev = nkops.nk_dev_lookup_by_type(NK_DEV_ID_WDT, pdev))) {
			unsigned long flags;
			NkDevDesc* vdev = (NkDevDesc*)nkops.nk_ptov(pdev);
			nkwdt = (NkDevWdt*)nkops.nk_ptov(vdev->dev_header);
			nkwdt_owner = (vdev->dev_owner ? vdev->dev_owner : NK_OS_PRIM);
#ifdef CONFIG_ARM
			__NK_HARD_LOCK_IRQ_SAVE(&(vdev->dev_lock), flags);
#else
			flags = __NK_HARD_LOCK_IRQ_SAVE(&(vdev->dev_lock));
#endif
			if (!nkwdt->period) {
				nkwdt->period = soft_margin * 1000;
			}
			if (nkwdt->period != soft_margin * 1000) {
				nkwdt = 0;
	    	}
			__NK_HARD_UNLOCK_IRQ_RESTORE(&(vdev->dev_lock), flags);
			if (nkwdt) {
	    		printk(PFX "Virtual watchdog device detected.\n");
				_firstwaittime = 1;
				return 0;
			}
		}
		if (!waiting) {
			printk(PFX "Waiting for a virtual watchdog device ...\n");
		}
		sleep_on_timeout(&device_wait, HZ/2);
	}
	printk(PFX "No virtual watchdog device found.\n");
	return -1;
}

static struct file_operations nksoftdog_fops = {
	owner:	THIS_MODULE,
	write:	nksoftdog_write,
	ioctl:	nksoftdog_ioctl,
	open:	nksoftdog_open,
	release:	nksoftdog_release,
};

static struct miscdevice nksoftdog_miscdev = {
	minor:	WATCHDOG_MINOR,
	name:	"watchdog",
	fops:	&nksoftdog_fops,
};

	static int
__init watchdog_init(void)
{
	int    res;
	char*  cmdline;
	NkOsId id = nkops.nk_id_get();

	/*
	 * Find a device ...
	 */
	if (nk_wdt_lookup() == 0) {
		res = misc_register(&nksoftdog_miscdev);
		if (res) {
			return res;
		}
		printk(banner, soft_margin);
		nkwdt->vex_addr[id] = nkops.nk_vex_addr(0);
		nkwdt->vex_mask[id] = 0;
		/*
		 * Parse kernel command line options ...
		 */
		cmdline = saved_command_line;
		if ((cmdline = strstr(cmdline, "vwatchdog-intr="))) {
			cmdline += 15;
			nkwdt->vex_mask[id] = simple_strtol(cmdline, 0, 0);
		}
	}
	return 0;
}

	static void
__exit watchdog_exit(void)
{
	if (nkwdt) {
		misc_deregister(&nksoftdog_miscdev);
	}
}

module_init(watchdog_init);
module_exit(watchdog_exit);

/*
 ****************************************************************
 *
 *  Component: VLX User Mode virtual driver Proxy driver
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
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/sched.h>	/* TASK_INTERRUPTIBLE */
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>

#include <nk/nkern.h>
#include <vlx/vlx-ump.h>

/*----- Local configuration -----*/

#if 0
#define UMP_DEBUG
#endif

/*----- Traces -----*/

#define UMP_MSG		"UMP: "

#define TRACE(x...)	printk (KERN_NOTICE  UMP_MSG x)
#define WTRACE(x...)	printk (KERN_WARNING UMP_MSG "Warning: " x)
#define ETRACE(x...)	printk (KERN_ERR     UMP_MSG "Error: " x)

#ifdef UMP_DEBUG
#define DTRACE(x...)	printk (KERN_CRIT   UMP_MSG x)
#else
#define DTRACE(x...)
#endif

/*----- Implementation -----*/

#define UMP_NAME	"vlx-ump"
#define UMP_STATIC_ASSERT(x)	extern char ump_static_assert [(x) ? 1 : -1]

typedef struct ump_vlink_t {
    NkPhAddr		pvlink;
    struct ump_vlink_t*	next;
} ump_vlink_t;

typedef struct {
    struct list_head	link;		/* list of all files */
    struct mutex	mutex;		/* mutual exclusion lock for all ops */
    wait_queue_head_t	wait;		/* waiting queue for all ops */
    pid_t		pid;		/* opener */
    ump_vlink_t*	vlinks;		/* vlinks managed */
    NkXIrqMask		xpending;	/* pending XIRQs */
    NkXIrqId		xids [NK_XIRQ_LIMIT];
    NkXIrqMask		xmasked;	/* masked XIRQs */
    char		name [32];	/* name of opening process */
    spinlock_t		spinlock;	/* sync with xirq thread */
	/* Statistics */
    struct {
	unsigned	ioctls;
	unsigned	ioctl_counters [UMPIOC_MAX];
	unsigned	xirqs;
	unsigned	spurious_wakeups;
    } stats;
} ump_file_t;

#define UMP_FOR_ALL_VLINKS(f,v) \
	for ((v) = (f)->vlinks; (v); (v) = (v)->next)

static struct {
    struct mutex	mutex;	/* mutual exclusion lock for all ops */
    struct list_head	files;	/* list of open files */
    unsigned		users;
    _Bool		proc_exists;/* /proc/nk/vlx_ump created */
    struct {
	unsigned	opens;	/* usage counter */
	unsigned	ioctls;	/* usage counter */
	unsigned	ioctl_counters [UMPIOC_MAX];
	unsigned	xirqs;	/* statistics */
	unsigned	spurious_wakeups;
    } stats;
} ump;

/*----- Interrupt management -----*/

#if (__LINUX_ARM_ARCH__ >= 6)
    static void
ump_smp_set (volatile nku32_f* addr, nku32_f data)
{
    nku32_f tmp, tmp2;
    __asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	orr	%0, %0, %3\n"
"	strex	%1, %0, [%2]\n"
"	teq	%1, #0\n"
"	bne	1b"
    : "=&r" (tmp), "=&r" (tmp2)
    : "r" (addr), "Ir" (data)
    : "cc");
}
#else
    static inline void
ump_smp_set (volatile nku32_f* addr, nku32_f data)
{
    *addr |= data;
}
#endif

#define UMP_L1_BIT(x)	((x) >> 5)
#define UMP_L2_BIT(x)	((x) & 0x1f)
#define UMP_BIT2MASK(x)	(1 << (x))

    static inline void
ump_xirq_pmask_set (ump_file_t* f, NkXIrq xirq)
{
    const nku32_f l1bit = UMP_L1_BIT (xirq);
    const nku32_f l2bit = UMP_L2_BIT (xirq);
    unsigned long flags;

    spin_lock_irqsave (&f->spinlock, flags);
    ump_smp_set (&f->xpending.lvl2 [l1bit], UMP_BIT2MASK (l2bit));
    ump_smp_set (&f->xpending.lvl1,         UMP_BIT2MASK (l1bit));
    spin_unlock_irqrestore (&f->spinlock, flags);
}

    static inline void
ump_xirq_mask_set (NkXIrqMask* xmask, const NkXIrq xirq)
{
    xmask->lvl2 [UMP_L1_BIT (xirq)] |= UMP_BIT2MASK (UMP_L2_BIT (xirq));
}

    static inline _Bool
ump_xirq_mask_test (const NkXIrqMask* xmask, const NkXIrq xirq)
{
    return xmask->lvl2 [UMP_L1_BIT (xirq)] &
			UMP_BIT2MASK (UMP_L2_BIT (xirq));
}

    static inline void
ump_xirq_mask_clear (NkXIrqMask* xmask, const NkXIrq xirq)
{
    xmask->lvl2 [UMP_L1_BIT (xirq)] &= ~UMP_BIT2MASK (UMP_L2_BIT (xirq));
}

    static void
ump_xirq_hdl (void* cookie, NkXIrq xirq)
{
    ump_file_t*	f = (ump_file_t*) cookie;

    ++f->stats.xirqs;
    ump_xirq_pmask_set (f, xirq);
    wake_up_interruptible (&f->wait);
}

#define	UMP_XIRQ(l1,l2)	(((l1) << 5) + (l2))
#define UMP_NO_XIRQ	((NkXIrq) -1)

    /*
     * If we found something at level 2, we try to clear
     * level 1 too before returning, so that we do not
     * get spurious wakeups the next time we sleep,
     * because only level 1 bits are tested.
     */

    static NkXIrq
ump_xirq_pmask_get_next (ump_file_t* f)
{
    NkXIrq xirq = UMP_NO_XIRQ;

    while (f->xpending.lvl1) {
	const nku32_f l1bit = nkops.nk_mask2bit (f->xpending.lvl1);
	const nku32_f l2    = f->xpending.lvl2 [l1bit];

	if (l2) {
	    if (xirq == UMP_NO_XIRQ) {
		const nku32_f l2bit  = nkops.nk_mask2bit (l2);
		const nku32_f l2mask = UMP_BIT2MASK (l2bit);

		nkops.nk_atomic_clear (&f->xpending.lvl2 [l1bit], l2mask);
		xirq = UMP_XIRQ (l1bit, l2bit);
		    /* Try to clear level 1 now */
		continue;
	    }
		/* Still something pending in level 2 (and level 1) */
	    return xirq;
	}
	nkops.nk_atomic_clear (&f->xpending.lvl1, UMP_BIT2MASK (l1bit));
	if (xirq != UMP_NO_XIRQ) break;
    }
    return xirq;
}

    static inline _Bool
ump_pending_xirqs (const ump_file_t* f)
{
    return f->xpending.lvl1 != 0;
}

/*----- vlink tracking -----*/

    /* The mutex must be taken */

    static void
ump_vlinks_add (ump_file_t* f, NkPhAddr pvlink)
{
    NkDevVlink* vlink = (NkDevVlink*) nkops.nk_ptov (pvlink);
    ump_vlink_t* curr;

    (void) vlink;
    UMP_FOR_ALL_VLINKS (f, curr) {
	if (curr->pvlink == pvlink) {
	    DTRACE ("vlink %s,%d already added\n", vlink->name, vlink->link);
	    return;
	}
    }
    curr = (ump_vlink_t*) kmalloc (sizeof *curr, GFP_KERNEL);
    if (!curr) {
	WTRACE ("Could not save ownership for vlink %s,%d\n",
		vlink->name, vlink->link);
	return;
    }
    curr->pvlink   = pvlink;
    curr->next     = f->vlinks;
    f->vlinks = curr;
}

    /* The mutex must be taken */

    static void
ump_vlinks_reset (ump_file_t* f)
{
    const NkOsId osid = nkops.nk_id_get();

    while (f->vlinks) {
	ump_vlink_t* curr = f->vlinks;
	NkDevVlink* vlink = (NkDevVlink*) nkops.nk_ptov (curr->pvlink);

	if (vlink->c_id == osid && vlink->c_state != NK_DEV_VLINK_OFF) {
	    WTRACE ("Resetting client side for vlink %s,%d (pid %d)\n",
		    vlink->name, vlink->link, f->pid);
	    vlink->c_state = NK_DEV_VLINK_OFF;
	    nkops.nk_xirq_trigger (NK_XIRQ_SYSCONF, vlink->s_id);
	}
	if (vlink->s_id == osid && vlink->s_state != NK_DEV_VLINK_OFF) {
	    WTRACE ("Resetting server side for vlink %s,%d (pid %d)\n",
		    vlink->name, vlink->link, f->pid);
	    vlink->s_state = NK_DEV_VLINK_OFF;
	    nkops.nk_xirq_trigger (NK_XIRQ_SYSCONF, vlink->c_id);
	}
	f->vlinks = f->vlinks->next;
	kfree (curr);
    }
}

/*----- Helpers -----*/

typedef unsigned long	VmAddr;
typedef unsigned long	VmSize;
typedef unsigned int	BankType;

typedef struct BankDesc {
    char*    id;	/* name string */
    BankType type;	/* bank's type */
    char*    fs;	/* file system type */
    VmAddr   vaddr;	/* address required for the bank's image */
    VmSize   size;	/* bank's size (actually used size) */
    VmSize   cfgSize;	/* configured bank's size (max size) */
} BankDesc;

    static const BankDesc*
ump_find_bank (const char* id)
{
    NkBootInfo		boot_info;
    NkPhAddr		pboot_info = nkops.nk_vtop (&boot_info);
    NkBankInfo		bank_info;
	/*
	 *  If different compilers are used for Linux and for the
	 *  nanokernel, the definition of the boot_info.ctrl enumeration
	 *  field might not be the same. Linux compiler might consider
	 *  that it is 1 byte wide and pad it to 4 bytes, while the
	 *  nanokernel code looks at all 4 bytes. Hence, unless memory
	 *  is cleared around, the nanokernel will not recognize the
	 *  ctrl code.
	 */
    memset (&boot_info, 0, sizeof boot_info);

	/* Obtain bank descriptors */
    boot_info.ctrl = INFO_BOOTCONF;
    boot_info.osid = 0;	/* Not actually used */
    boot_info.data = nkops.nk_vtop (&bank_info);
    if (os_ctx->binfo (os_ctx, pboot_info) < 0) {
	return NULL;
    }
    {
	const BankDesc*	bank = (const BankDesc*)nkops.nk_ptov (bank_info.banks);
	int		i;

	for (i = 0; i < bank_info.numBanks; i++, bank++) {
	    if (bank->id && !strcmp (bank->id, id)) {
		return bank;
	    }
	}
    }
    return NULL;
}

/*----- Device API implementation -----*/

    static int
ump_open (struct inode* inode, struct file* file)
{
    ump_file_t*	f = (ump_file_t*) kzalloc (sizeof *f, GFP_KERNEL);

    if (!f) {
	ETRACE ("out of memory for file\n");
	return -ENOMEM;
    }
    INIT_LIST_HEAD (&f->link);
    mutex_init (&f->mutex);
    init_waitqueue_head (&f->wait);
    f->pid = current->pid;
    snprintf (f->name, sizeof f->name, "%.20s(%d)", current->comm, f->pid);
    spin_lock_init(&f->spinlock);
	/* Mark file as non-seekable. Never fails */
    nonseekable_open (inode, file);
    file->private_data = f;
    mutex_lock (&ump.mutex);
    ++ump.stats.opens;
	/* Counter-intuitive argument order, list head last */
    list_add (&f->link, &ump.files);
    ++ump.users;
    mutex_unlock (&ump.mutex);
    DTRACE ("ump_open OK from pid %d\n", f->pid);
    return 0;
}

    /*
     * ump_release implements release (close) operation
     */
    static int
ump_release (struct inode* inode, struct file* file)
{
    ump_file_t*	f = (ump_file_t*) file->private_data;
    NkXIrq	xirq;
    unsigned	i;

    (void) inode;
    mutex_lock (&f->mutex);
    for (xirq = 0; xirq < (NkXIrq) os_ctx->xirq_free; ++xirq) {
	if (ump_xirq_mask_test (&f->xmasked, xirq)) {
	    nkops.nk_xirq_unmask (xirq);
	    ump_xirq_mask_clear (&f->xmasked, xirq);
	    ++f->stats.ioctl_counters [_IOC_NR (UMPIOC_XIRQ_UNMASK)];
	    WTRACE ("unmasking xirq %d (pid %d)\n", xirq, f->pid);
	}
	if (f->xids [xirq]) {
	    nkops.nk_xirq_detach (f->xids [xirq]);
	    f->xids [xirq] = 0;
	    ++f->stats.ioctl_counters [_IOC_NR (UMPIOC_XIRQ_DETACH)];
	    WTRACE ("freeing xirq %d (pid %d)\n", xirq, f->pid);
	}
    }
    ump_vlinks_reset (f);
    mutex_unlock (&f->mutex);

    mutex_lock (&ump.mutex);
    list_del (&f->link);
    --ump.users;
    ump.stats.ioctls += f->stats.ioctls;
    for (i = 0; i < UMPIOC_MAX; ++i) {
	if (!f->stats.ioctl_counters [i]) continue;
	ump.stats.ioctl_counters [i] += f->stats.ioctl_counters [i];
    }
    ump.stats.xirqs += f->stats.xirqs;
    ump.stats.spurious_wakeups += f->stats.spurious_wakeups;
    mutex_unlock (&ump.mutex);
    kfree (f);
    DTRACE ("ump_release OK\n");
    return 0;
}

static const char* const ump_ioctl_names[] = UMPIOC_NAMES;

    static long
ump_ioctl (struct file* file, unsigned int cmd, unsigned long arg)
{
    ump_file_t*		f = (ump_file_t*) file->private_data;
    const unsigned	cmd_nr = _IOC_NR (cmd);
    int			diag = 0;
    union {
	ump_ioctl_t	ioctl;
	ump_resource_t	resrc;
    } u;

    ++f->stats.ioctls;
    DTRACE ("ioctl: cmd %x (%s) arg %lx\n",
	    cmd, ump_ioctl_names [cmd_nr % UMPIOC_MAX], arg);
    if (cmd_nr >= UMPIOC_MAX) {
	ETRACE ("invalid ioctl 0x%x number %d\n", cmd, cmd_nr);
	return -EINVAL;
    }
    if (_IOC_SIZE (cmd) > sizeof (u)) {
	ETRACE ("invalid ioctl 0x%x size %x\n", cmd, _IOC_SIZE (cmd));
	return -EINVAL;
    }
    ++f->stats.ioctl_counters [cmd_nr];
    if (_IOC_DIR (cmd) & _IOC_READ) {
	diag = copy_from_user (&u, (void __user*) arg, _IOC_SIZE (cmd));
	DTRACE ("arg-in: diag %d buf %p addr %lx size %x\n",
		diag, u.ioctl.buf, u.ioctl.addr, u.ioctl.size);
	if (diag) return -EFAULT;
    } else if (_IOC_DIR (cmd) & _IOC_WRITE) {
	    /* Do not allow callers to read kernel stack */
	memset (&u, 0, _IOC_SIZE (cmd));
    }
    if (mutex_lock_interruptible (&f->mutex)) {
	ETRACE ("lock wait interrupted\n");
	return -EINTR;
    }
    switch (cmd) {
    case UMPIOC_GET_VERSION:
	diag = sizeof *os_ctx;
	DTRACE ("get_version: diag %d\n", diag);
	break;

    case UMPIOC_READ_NKOSCTX:
	diag = copy_to_user ((NkOsCtx __user*) arg, os_ctx, sizeof *os_ctx);
	DTRACE ("read_nkosctx: diag %d\n", diag);
	if (diag) {
	    diag = -EFAULT;
	}
	break;

    case UMPIOC_FIND_OSCTX_BANK: {
	const BankDesc*	bank;

	bank = ump_find_bank ("nkernel_osctx_bank");
	if (!bank) {
	    ETRACE ("did not find osctx bank\n");
	    diag = -ESRCH;
	    break;
	}
	u.ioctl.addr = nkops.nk_vtop ((void*) bank->vaddr);
	u.ioctl.size = bank->cfgSize;
	break;
    }
    case UMPIOC_READ_VIRT:
	DTRACE ("at addr: %lx %lx\n", ((long*) u.ioctl.addr) [0],
		((long*) u.ioctl.addr) [1]);
	diag = copy_to_user ((void __user*) u.ioctl.buf, (void*) u.ioctl.addr,
			     u.ioctl.size);
	DTRACE ("read_virt: copy diag %d\n", diag);
	if (diag) {
	    diag = -EFAULT;
	    break;
	}
	break;

    case UMPIOC_FIND_OSCTX:
	u.ioctl.addr = nkops.nk_vtop (os_ctx);
	u.ioctl.size = sizeof *os_ctx;
	break;

    case UMPIOC_VTOP:
	u.ioctl.addr = nkops.nk_vtop (u.ioctl.buf);
	break;

    case UMPIOC_AWAIT_XIRQ:
	mutex_unlock (&f->mutex);
	while (1) {
	    diag = wait_event_interruptible (f->wait, ump_pending_xirqs (f));
	    if (diag) break;
	    diag = ump_xirq_pmask_get_next (f);
	    if (diag != -1) break;
	    ++f->stats.spurious_wakeups;
	}
	if (mutex_lock_interruptible (&f->mutex)) {
	    ETRACE ("lock wait interrupted\n");
	    return -EINTR;
	}
	break;

    case UMPIOC_XIRQ_ATTACH:
	if (arg < NK_XIRQ_SYSCONF || arg >= (unsigned long) os_ctx->xirq_free) {
	    ETRACE ("xirq_attach(%ld)\n", arg);
	    diag = -EINVAL;
	    break;
	}
	if (f->xids [arg]) {
	    ETRACE ("xirq %d already attached\n", (int) arg);
	    diag = -EINVAL;
	    break;
	}
	f->xids [arg] = nkops.nk_xirq_attach (arg, ump_xirq_hdl, f);
	if (!f->xids [arg]) {
	    ETRACE ("nk_xirq_attach_xirq(%d) failed\n", (int) arg);
	    diag = -ENOMEM;
	} else {
	    DTRACE ("attach_xirq(%d): diag %d\n", (int) arg, diag);
	}
	break;

    case UMPIOC_XIRQ_DETACH:
	if (arg < NK_XIRQ_SYSCONF || arg >= (unsigned long) os_ctx->xirq_free) {
	    ETRACE ("xirq_detach(%ld)\n", arg);
	    diag = -EINVAL;
	    break;
	}
	if (!f->xids [arg]) {
	    ETRACE ("xirq %d not attached\n", (int) arg);
	    diag = -EINVAL;
	    break;
	}
	nkops.nk_xirq_detach (f->xids [arg]);
	f->xids [arg] = (NkXIrqId) 0;
	DTRACE ("detach_xirq %d\n", (int) arg);
	break;

    case UMPIOC_PDEV_ALLOC:
	ump_vlinks_add (f, u.resrc.vlink);
	u.resrc.nkresrc.r.pdev.addr = nkops.nk_pdev_alloc
	    (u.resrc.vlink, u.resrc.nkresrc.id,
	     u.resrc.nkresrc.r.pdev.size);
	break;

    case UMPIOC_PMEM_ALLOC:
	ump_vlinks_add (f, u.resrc.vlink);
	u.resrc.nkresrc.r.pmem.addr = nkops.nk_pmem_alloc
	    (u.resrc.vlink, u.resrc.nkresrc.id,
	     u.resrc.nkresrc.r.pmem.size);
	break;

    case UMPIOC_PXIRQ_ALLOC:
	ump_vlinks_add (f, u.resrc.vlink);
	u.resrc.nkresrc.r.pxirq.base = nkops.nk_pxirq_alloc
	    (u.resrc.vlink, u.resrc.nkresrc.id,
	     u.resrc.nkresrc.r.pxirq.osid,
	     u.resrc.nkresrc.r.pxirq.numb);
	break;

    case UMPIOC_DEV_ALLOC:
	u.ioctl.addr = nkops.nk_dev_alloc (u.ioctl.size);
	break;

    case UMPIOC_DEV_ADD:
	nkops.nk_dev_add (arg);
	break;

    case UMPIOC_XIRQ_ALLOC:
	diag = nkops.nk_xirq_alloc (arg);
	break;

    case UMPIOC_XIRQ_TRIGGER:
	nkops.nk_xirq_trigger (arg >> 16 /*xirq*/, arg & 0xFFFF /*osid*/);
	break;

    case UMPIOC_XIRQ_MASK:
	if (arg < NK_XIRQ_SYSCONF || arg >= (unsigned long) os_ctx->xirq_free) {
	    ETRACE ("xirq_mask(%ld)\n", arg);
	    diag = -EINVAL;
	    break;
	}
	if (ump_xirq_mask_test (&f->xmasked, arg)) {
	    ETRACE ("xirq %ld already masked\n", arg);
	    diag = -EINVAL;
	    break;
	}
	ump_xirq_mask_set (&f->xmasked, arg);
	nkops.nk_xirq_mask (arg);
	break;

    case UMPIOC_XIRQ_UNMASK:
	if (arg < NK_XIRQ_SYSCONF || arg >= (unsigned long) os_ctx->xirq_free) {
	    ETRACE ("xirq_unmask(%ld)\n", arg);
	    diag = -EINVAL;
	    break;
	}
	if (!ump_xirq_mask_test (&f->xmasked, arg)) {
	    ETRACE ("xirq %ld not masked\n", arg);
	    diag = -EINVAL;
	    break;
	}
	ump_xirq_mask_clear (&f->xmasked, arg);
	nkops.nk_xirq_unmask (arg);
	break;

    case UMPIOC_ATOMIC_CLEAR:
	nkops.nk_atomic_clear ((volatile nku32_f*) u.ioctl.buf, u.ioctl.addr);
	break;

    case UMPIOC_ATOMIC_CLEAR_AND_TEST:
	u.ioctl.addr = nkops.nk_clear_and_test ((volatile nku32_f*) u.ioctl.buf,
					     u.ioctl.addr);
	break;

    case UMPIOC_ATOMIC_SET:
	nkops.nk_atomic_set ((volatile nku32_f*) u.ioctl.buf, u.ioctl.addr);
	break;

    case UMPIOC_ATOMIC_SUB:
	nkops.nk_atomic_sub ((volatile nku32_f*) u.ioctl.buf, u.ioctl.addr);
	break;

    case UMPIOC_ATOMIC_SUB_AND_TEST:
	u.ioctl.addr = nkops.nk_sub_and_test ((volatile nku32_f*) u.ioctl.buf,
					   u.ioctl.addr);
	break;

    case UMPIOC_ATOMIC_ADD:
	nkops.nk_atomic_add ((volatile nku32_f*) u.ioctl.buf, u.ioctl.addr);
	break;

    case UMPIOC_MEM_COPY_TO:
	diag = copy_from_user (nkops.nk_ptov (u.ioctl.addr), u.ioctl.buf,
			       u.ioctl.size);
	if (diag) {
	    u.ioctl.size = 0;
	}
	break;

    case UMPIOC_MEM_COPY_FROM:
	diag = copy_to_user (u.ioctl.buf, nkops.nk_ptov (u.ioctl.addr),
			     u.ioctl.size);
	if (diag) {
	    u.ioctl.size = 0;
	}
	break;

    case UMPIOC_SMP_TIME: {
	NkTime t = os_ctx->smp_time();

	u.ioctl.addr = t >> 32;
	u.ioctl.size = t & 0xffffffff;
	break;
    }
    case UMPIOC_SMP_TIME_HZ:
	diag = os_ctx->smp_time_hz();
	break;

    case UMPIOC_NKDDI_VERSION:
	diag = nkops.nk_version;
	break;

    default:
	ETRACE ("no ioctl 0x%x\n", cmd);
	diag = -EINVAL;
	break;
    }
    mutex_unlock (&f->mutex);

    if (!diag && _IOC_DIR (cmd) & _IOC_WRITE) {
	diag = copy_to_user ((void __user*) arg, &u, _IOC_SIZE (cmd));
	DTRACE ("arg-out: desc diag %d\n", diag);
	if (diag) return -EFAULT;
    }
    return diag;
}

    /*
     * ump_mmap maps areas into user space
     */
    static int
ump_mmap (struct file* file, struct vm_area_struct* vma)
{
    const size_t	size    = vma->vm_end - vma->vm_start;
    int			diag;

    (void) file;
	/*
	 * Check mmap parameters
	 */
    DTRACE ("mmap: end %lx start %lx pgoff %lx page_prot %lx\n",
	    vma->vm_end, vma->vm_start, vma->vm_pgoff, vma->vm_page_prot);
	/* Memory must be mapped cacheable, just like other guests do it */
    diag = io_remap_pfn_range (vma, vma->vm_start, vma->vm_pgoff,
			       size, vma->vm_page_prot);
    DTRACE ("remap: diag %d\n", diag);
    return diag;
}

static const struct file_operations ump_fops = {
    .owner	= THIS_MODULE,
    .open	= ump_open,
    .unlocked_ioctl = ump_ioctl,
    .mmap	= ump_mmap,
    .release	= ump_release
};

static struct miscdevice ump_miscdevice = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = UMP_NAME,
    .fops  = &ump_fops
};

/*----- /proc/nk/vlx-ump -----*/

    static void
ump_proc_xids (struct seq_file* seq, const NkXIrqId* xids)
{
    _Bool	had_any = 0;
    NkXIrq	xirq;

    for (xirq = 0; xirq < (NkXIrq) os_ctx->xirq_free; ++xirq) {
	if (!xids [xirq]) continue;
	seq_printf (seq, " %d", xirq);
	had_any = 1;
    }
    if (!had_any) {
	seq_printf (seq, " None");
    }
    seq_printf (seq, "\n");
}

    static void
ump_proc_xmask (struct seq_file* seq, const NkXIrqMask* xmask)
{
    _Bool	had_any = 0;
    NkXIrq	xirq;

    for (xirq = 0; xirq < (NkXIrq) os_ctx->xirq_free; ++xirq) {
	if (!ump_xirq_mask_test (xmask, xirq)) continue;
	seq_printf (seq, " %d", xirq);
	had_any = 1;
    }
    if (!had_any) {
	seq_printf (seq, " None");
    }
    seq_printf (seq, "\n");
}

    static void
ump_seq_proc_file (struct seq_file* seq, const ump_file_t* f)
{
    const NkOsId	osid = nkops.nk_id_get();
    _Bool		had_vlinks = 0;
    const ump_vlink_t*	curr;

    seq_printf (seq, "PID-- Ioctls- Xirqs-- Spurio Name----\n");
    seq_printf (seq, "%5d %7d %7d %6d %s\n",
		f->pid, f->stats.ioctls, f->stats.xirqs,
		f->stats.spurious_wakeups, f->name);
    if (f->stats.ioctls) {
	unsigned i;

	seq_printf (seq, "Ioctls:");
	for (i = 0; i < UMPIOC_MAX; ++i) {
	    if (!f->stats.ioctl_counters [i]) continue;
	    seq_printf (seq, " %s:%d", ump_ioctl_names [i],
			f->stats.ioctl_counters [i]);
	}
	seq_printf (seq, "\n");
    }
    seq_printf (seq, "Attached vlinks:");
    UMP_FOR_ALL_VLINKS (f, curr) {
	const NkDevVlink* vlink = (NkDevVlink*) nkops.nk_ptov (curr->pvlink);

	if (vlink->s_id == osid) {
	    seq_printf (seq, " %s,%d (server)", vlink->name, vlink->link);
	    had_vlinks = 1;
	}
	if (vlink->c_id == osid) {
	    seq_printf (seq, " %s,%d (client)", vlink->name, vlink->link);
	    had_vlinks = 1;
	}
    }
    if (!had_vlinks) {
	seq_printf (seq, " None");
    }
    seq_printf (seq, "\nAttached XIRQs:");
    ump_proc_xids (seq, f->xids);
    seq_printf (seq, "Pending  XIRQs:");
    ump_proc_xmask (seq, &f->xpending);
    seq_printf (seq, "Masked XIRQs:");
    ump_proc_xmask (seq, &f->xmasked);
}

    static int
ump_seq_proc_show (struct seq_file* seq, void* v)
{
    ump_file_t*	f;

    (void) v;
    mutex_lock (&ump.mutex);
    list_for_each_entry (f, &ump.files, link) {
	mutex_lock (&f->mutex);
	ump_seq_proc_file (seq, f);
	mutex_unlock (&f->mutex);
    }
    seq_printf (seq, "Us Opns Ioctls- Xirqs-- Spurio\n");
    seq_printf (seq, "%2d %4d %7d %7d %6d\n", ump.users,
		ump.stats.opens, ump.stats.ioctls, ump.stats.xirqs,
		ump.stats.spurious_wakeups);
    if (ump.stats.ioctls) {
	unsigned i;

	seq_printf (seq, "Ioctls:");
	for (i = 0; i < UMPIOC_MAX; ++i) {
	    if (!ump.stats.ioctl_counters [i]) continue;
	    seq_printf (seq, " %s:%d", ump_ioctl_names [i],
			ump.stats.ioctl_counters [i]);
	}
	seq_printf (seq, "\n");
    }
    mutex_unlock (&ump.mutex);
    return 0;
}

    static int
ump_proc_open (struct inode* inode, struct file* file)
{
    return single_open (file, ump_seq_proc_show, PDE (inode)->data);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,15)
static struct file_operations ump_proc_fops =
#else
static const struct file_operations ump_proc_fops =
#endif
{
    .owner	= THIS_MODULE,
    .open	= ump_proc_open,
    .read	= seq_read,
    .llseek	= seq_lseek,
    .release	= single_release,
};

    static int __init
ump_proc_init (void)
{
    struct proc_dir_entry* ent = create_proc_entry ("nk/" UMP_NAME, 0, NULL);

    if (!ent) {
	ETRACE ("Could not create /proc/nk/" UMP_NAME "\n");
	return -ENOMEM;
    }
    ent->proc_fops  = &ump_proc_fops;
    ent->data       = NULL;
    ump.proc_exists = 1;
    return 0;
}

    static void
ump_proc_exit (void)
{
    if (ump.proc_exists) {
	remove_proc_entry ("nk/" UMP_NAME, NULL);
    }
}

/*----- Initialization and termination -----*/

    static void
ump_module_cleanup (void)
{
    ump_proc_exit();
    misc_deregister (&ump_miscdevice);
}

    static int
ump_module_init (void)
{
    int	diag;

    INIT_LIST_HEAD (&ump.files);
    mutex_init (&ump.mutex);
    if ((diag = misc_register (&ump_miscdevice)) != 0) {
	ETRACE ("cannot register misc device (%d)\n", diag);
	return diag;
    }
    ump_proc_init();
    TRACE ("module loaded\n");
    return 0;
}

    static void
ump_module_exit (void)
{
    ump_module_cleanup();
    TRACE ("module unloaded\n");
}

/*----- Module glue -----*/

module_init (ump_module_init);
module_exit (ump_module_exit);

MODULE_DESCRIPTION ("VLX User Mode Virtual Driver proxy driver");
MODULE_AUTHOR      ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_LICENSE     ("GPL");

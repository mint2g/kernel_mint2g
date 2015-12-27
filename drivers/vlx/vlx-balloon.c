/*
 ****************************************************************
 *
 *  Component: VLX memory balloon driver
 *
 *  Copyright (C) 2012, Red Bend Ltd.
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
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *
 ****************************************************************
 */

/******************************************************************************
 * balloon.c
 *
 * Xen balloon driver - enables returning/claiming memory to/from Xen.
 *
 * Copyright (c) 2003, B Dragovic
 * Copyright (c) 2003-2004, M Williamson, K Fraser
 * Copyright (c) 2005 Dan M. Smith, IBM Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation; or, when distributed
 * separately from the Linux kernel or incorporated into other
 * software packages, subject to the following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/sysdev.h>
#include <linux/gfp.h>
#include <linux/oom.h>

#include <nk/nkern.h>
#include <vlx/vballoon_common.h>

extern NkMapDesc  nk_maps[];
extern int        nk_maps_max;

#define TRACE(format, args...)	\
	printk(KERN_INFO "vlx_balloon: " format, ## args)

#if 0
#define DTRACE(format, args...)	\
	TRACE("%s(%d): " format, __FUNCTION__, __LINE__, ## args)
#else
#define DTRACE(format, args...)
#endif

#define PAGES2KB(_p) ((_p)<<(PAGE_SHIFT-10))

#define BALLOON_CLASS_NAME "vlx_memory"

static struct notifier_block balloon_oom_notifier_block;

struct balloon_stats {
	/* We aim for 'current allocation' == 'target allocation'. */
    unsigned long current_pages;
    unsigned long target_pages;

	/*
	 * Drivers may alter the memory reservation independently, but they
	 * must inform the balloon driver so we avoid hitting the hard limit.
	 */
    unsigned long driver_pages;

	/* Number of pages in the resident memory */
    unsigned long resident_pages;

	/* Number of pages in the balloons memory */
    unsigned long balloon_pages;

	/* Initial number of pages in the balloon */
    unsigned long balloon_init_pages;

	/* Minimum number of pages in the balloon */
    unsigned long balloon_min_pages;

	/* Maximum number of pages in the balloon */
    unsigned long balloon_max_pages;

    unsigned long shrink_limit_pages;

    long last_credit;	/* last calculated credit */
    int  sleep_time;	/* current sleep time in seconds */

	/* vballoon related data */
    NkPhAddr         plink;  /* server vLINK physical address */
    NkDevVlink*      vlink;  /* server vLINK virtual address */
    NkPhAddr         pmem;   /* shared PMEM physical address */
    vballoon_pmem_t* vmem;   /* shared PMEM virtual address */
    NkXIrqId         xirq;   /* notification XIRQ ID */
    NkXIrqId         sirq;   /* SYSCONF XIRQ ID */
};

static DEFINE_MUTEX(balloon_mutex);

static struct sys_device balloon_sysdev;

static int register_balloon(struct sys_device *sysdev);

/*
 * Protects atomic reservation decrease/increase against concurrent increases.
 * Also protects non-atomic updates of current_pages and driver_pages, and
 * balloon lists.
 */
static DEFINE_SPINLOCK(balloon_lock);

static struct balloon_stats balloon;

/* We increase/decrease in batches which fit in a page */
static nku32_f frame_list[PAGE_SIZE / sizeof(nku32_f)];

/* Main work function, always executed in process context. */
static void balloon_process(struct work_struct *work);
static DECLARE_WORK(balloon_worker, balloon_process);
static struct timer_list balloon_timer;
static LIST_HEAD(balloon_resident_pages);
static DEFINE_SPINLOCK(balloon_resident_lock);
static int balloon_aborted;

/* When ballooning out (allocating memory to return to VLX) we don't really
   want the kernel to try too hard since that can trigger the oom killer. */
#define GFP_BALLOON \
	(GFP_HIGHUSER | __GFP_NOWARN | __GFP_NORETRY | __GFP_NOMEMALLOC)

	/* balloon_append: add the given page to the balloon. */
    static void
balloon_append (struct page *page)
{
    balloon.balloon_pages++;

    if (balloon.vmem) {
	balloon.vmem->balloon_pages = balloon.balloon_pages;
	wmb();
    }
}

    /* balloon_retrieve: rescue a page from the balloon, if it is not empty. */
    static void
balloon_retrieve (struct page *page)
{
    balloon.balloon_pages--;

    if (balloon.vmem) {
	balloon.vmem->balloon_pages = balloon.balloon_pages;
	wmb();
    }
}

    static int
is_balloon_page (struct page* page)
{
    NkMapDesc*    map   = nk_maps;
    NkMapDesc*    limit = nk_maps + nk_maps_max;
    unsigned long paddr = page_to_phys(page);

    while (map < limit) {
	if ((map->pstart <= paddr) && (paddr <= map->plimit)) {
	    return (map->mem_type  == NK_MD_RAM) &&
		   (map->mem_owner == NK_OS_ANON);
	}
	map++;
    }

    return 0;
}

    static unsigned long
balloon_pages (void)
{
    NkMapDesc*    map   = nk_maps;
    NkMapDesc*    limit = nk_maps + nk_maps_max;
    unsigned long count = 0;

    while (map < limit) {
	if ((map->mem_type == NK_MD_RAM) && (map->mem_owner == NK_OS_ANON)) {
	    count += (map->plimit - map->pstart + 1) >> PAGE_SHIFT;
	}
	map++;
    }

    return count;
}

    static void
scrub_page (struct page *page)
{
#ifdef CONFIG_VLX_BALLOON_SCRUB_PAGES
    clear_highpage(page);
#endif
}

    static void
balloon_alarm(unsigned long unused)
{
    schedule_work(&balloon_worker);
}

    static struct page*
balloon_alloc_page (void)
{
    return alloc_page(GFP_BALLOON);
}

    static void
balloon_free_page (struct page* page)
{
    ClearPageReserved(page);
    init_page_count(page);
    __free_page(page);
}

    static int
increase_reservation (unsigned long nr_pages)
{
    unsigned long  i, flags;
    unsigned long  count;

    if (nr_pages > ARRAY_SIZE(frame_list)) {
	nr_pages = ARRAY_SIZE(frame_list);
    }

    spin_lock_irqsave(&balloon_lock, flags);

    count = nkops.nk_balloon_ctrl(NK_BALLOON_ALLOC, frame_list, nr_pages);

    balloon.current_pages += count;

    if (balloon.vmem) {
	balloon.vmem->current_pages = balloon.current_pages;
	wmb();
    }

    spin_unlock_irqrestore(&balloon_lock, flags);

    for (i = 0; i < count; i++) {
	struct page* page = pfn_to_page(frame_list[i]);

	balloon_retrieve(page);

		/* Relinquish the page back to the allocator. */
	balloon_free_page(page);
    }

    return (count != nr_pages);
}

    static void
add_resident_page (struct page* page)
{
    unsigned long flags;
    spin_lock_irqsave(&balloon_resident_lock, flags);
    list_add(&page->lru, &balloon_resident_pages);
    balloon.driver_pages++; 
    spin_unlock_irqrestore(&balloon_resident_lock, flags);
}

    static unsigned long
free_resident_pages (void)
{
    unsigned long flags;
    struct page*  page;
    unsigned long freed = 0;

    spin_lock_irqsave(&balloon_resident_lock, flags);

    while (!list_empty(&balloon_resident_pages)) {
	page = list_entry(balloon_resident_pages.next, struct page, lru);
	list_del(&page->lru);
	freed++;
	balloon.driver_pages--; 
	spin_unlock_irqrestore(&balloon_resident_lock, flags);

		/* Relinquish the page back to the allocator. */
	balloon_free_page(page);

	spin_lock_irqsave(&balloon_resident_lock, flags);
    }      

    spin_unlock_irqrestore(&balloon_resident_lock, flags);

    return freed;
}

    static int
have_resident_pages (void)
{
    return (balloon.driver_pages > 0); 
}

    static int
decrease_reservation (unsigned long nr_pages)
{
    unsigned long flags;
    struct page*  page;
    unsigned long count = 0;
    int           need_sleep = 0;

    if (nr_pages > ARRAY_SIZE(frame_list)) {
	nr_pages = ARRAY_SIZE(frame_list);
    }

    while (count < nr_pages) {
	if (!(page = balloon_alloc_page()) || balloon_aborted) {
	    need_sleep = 1;
	    break;
	}

	if (is_balloon_page(page)) {
	    scrub_page(page);
	    frame_list[count++] = page_to_pfn(page);
	    balloon_append(page);
	} else {
	    add_resident_page(page);
	}
    }

    free_resident_pages();

    spin_lock_irqsave(&balloon_lock, flags);

    nkops.nk_balloon_ctrl(NK_BALLOON_FREE, frame_list, count);

    balloon.current_pages -= count;

    if (balloon.vmem) {
	balloon.vmem->current_pages = balloon.current_pages;
	wmb();
    }

    spin_unlock_irqrestore(&balloon_lock, flags);

    balloon_aborted = 0;

    return need_sleep;
}

    static void
abort_reservation (unsigned long* freed)
{
    *freed = have_resident_pages();
    DTRACE("Abort -> [%ld/%ld]\n",
	   global_page_state(NR_FREE_PAGES),
	   global_page_state(NR_FILE_PAGES) - global_page_state(NR_SHMEM));
    balloon_aborted = 1;
    free_resident_pages();
}

    static long
current_credit (void)
{
    unsigned long target;
    long          credit;
    long          rcredit;

    target = balloon.target_pages;

    target = min(target, balloon.current_pages + balloon.balloon_pages);

    target = max(target, balloon.resident_pages);

    credit = target - balloon.current_pages;

    rcredit = credit;

    if (credit && (credit == balloon.last_credit)) {
	balloon.sleep_time++;
	if (balloon.sleep_time > 3) {
	    credit = 0;
	    balloon.sleep_time = 1;
	}
    } else {
	balloon.sleep_time = 1;
    }

    balloon.last_credit = credit;

    if (!credit) {
	DTRACE("[%ld %ld %ld %ld -> %ld]\n",
	       balloon.target_pages,
	       balloon.current_pages,
	       balloon.resident_pages,
	       balloon.current_pages + balloon.balloon_pages,
	       rcredit);
    }

    return credit;
}

/*
 * We avoid multiple worker processes conflicting via the balloon mutex.
 * We may of course race updates of the target counts (which are protected
 * by the balloon lock), or with changes to the hard limit, but we will
 * recover from these in time.
 */
    static void
balloon_process (struct work_struct *work)
{
    int need_sleep = 0;
    long credit = 0;

    mutex_lock(&balloon_mutex);

    do {
	credit = current_credit();
	if (credit > 0)
	    need_sleep = (increase_reservation(credit) != 0);
	if (credit < 0)
	    need_sleep = (decrease_reservation(-credit) != 0);

#ifndef CONFIG_PREEMPT
	if (need_resched())
	    schedule();
#endif
    } while (credit && !need_sleep);

	/* Schedule more work if there is some still to be done. */
    if (credit) {
	mod_timer(&balloon_timer, jiffies + (HZ * balloon.sleep_time));
    }

    mutex_unlock(&balloon_mutex);
}

/* Sets new target, and kicks off processing. */
    static void
balloon_set_new_target (unsigned long target)
{
	/* No need for lock. Not read-modify-write updates. */
    balloon.target_pages = target;
    schedule_work(&balloon_worker);
}

    static int
balloon_oom_notifier_call (struct notifier_block* block,
			   unsigned long          unused,
			   void*                  freed)
{
    abort_reservation(freed);
    return 0;
}

    static void
vballoon_sysconf_post (void)
{
    if (balloon.vmem) {
        nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, balloon.vlink->c_id);
    }
}

    static void
vballoon_pmem_init (void)
{
    if (balloon.vmem) {
	balloon.vmem->current_pages      = balloon.current_pages;
	balloon.vmem->resident_pages     = balloon.resident_pages;
	balloon.vmem->balloon_pages      = balloon.balloon_pages;
	balloon.vmem->balloon_init_pages = balloon.balloon_init_pages;
	balloon.vmem->balloon_min_pages  = balloon.balloon_min_pages;
	balloon.vmem->balloon_max_pages  = balloon.balloon_max_pages;
	wmb();
    }
}

    static int
vballoon_handshake (void)
{
    volatile int* my_state;
    int           peer_state;

    my_state   = &balloon.vlink->s_state;
    peer_state = balloon.vlink->c_state;

    switch (*my_state) {
        case NK_DEV_VLINK_OFF:
            if (peer_state != NK_DEV_VLINK_ON) {
                *my_state = NK_DEV_VLINK_RESET;
                vballoon_sysconf_post();
            }
            break;
        case NK_DEV_VLINK_RESET:
            if (peer_state != NK_DEV_VLINK_OFF) {
		vballoon_pmem_init();
                *my_state = NK_DEV_VLINK_ON;
                vballoon_sysconf_post();
            }
            break;
        case NK_DEV_VLINK_ON:
            if (peer_state == NK_DEV_VLINK_OFF) {
                *my_state = NK_DEV_VLINK_RESET;
                vballoon_sysconf_post();
            }
            break;
    }

    return (*my_state  == NK_DEV_VLINK_ON) &&
           (peer_state == NK_DEV_VLINK_ON);
}


    static void
vballoon_sysconf_handler (void* cookie, NkXIrq xirq)
{
    vballoon_handshake();
    (void)cookie; (void)xirq;
}

    static void
vballoon_xirq_handler (void* cookie, NkXIrq xirq)
{
    if (balloon.vmem) {
	    /* No need for lock. Not read-modify-write updates. */
        balloon.target_pages = balloon.vmem->target_pages;
	schedule_work(&balloon_worker);
    }
    (void)cookie; (void)xirq;
}

    static void
vballoon_fini (void)
{
    if (balloon.sirq) {
	nkops.nk_xirq_detach(balloon.sirq);
	balloon.sirq = 0;
    }

    if (balloon.xirq) {
	nkops.nk_xirq_detach(balloon.xirq);
	balloon.xirq = 0;
    }

    if (balloon.vmem) {
	nkops.nk_mem_unmap(balloon.vmem, balloon.pmem,
			   sizeof(vballoon_pmem_t));
	balloon.vmem = 0;
    }
}

    //
    // Conversion from character string to integer number
    //
    static const char*
_a2ui (const char* s, unsigned int* i)
{
    unsigned int xi = 0;
    char         c  = *s;

    while (('0' <= c) && (c <= '9')) {
	xi = xi * 10 + (c - '0');
	c = *(++s);
    }

    if        ((*s == 'K') || (*s == 'k')) {
	xi *= 1024;
	s  += 1;
    } else if ((*s == 'M') || (*s == 'm')) {
	xi *= (1024*1024);
	s  += 1;
    }

    *i = xi;

    return s;
}

    static void
vballoon_set_limits (void)
{
    unsigned int pages;
    const char*  info;

    if (!balloon.vlink || !balloon.vlink->s_info) {
	return;
    }

    info = (char*)nkops.nk_ptov(balloon.vlink->s_info);

    info = _a2ui(info, &pages);
    balloon.balloon_min_pages = pages >> PAGE_SHIFT;
    if (*info == ',') {
        info = _a2ui(info+1, &pages);
        balloon.balloon_init_pages = pages >> PAGE_SHIFT;
        if (*info == ',') {
	    info = _a2ui(info+1, &pages);
	    if (pages) {
	        balloon.balloon_max_pages = pages >> PAGE_SHIFT;
	    }
            if (*info == ',') {
	        info = _a2ui(info+1, &pages);
	        balloon.shrink_limit_pages = pages >> PAGE_SHIFT;
	    }
	}
    }

    if (balloon.balloon_min_pages > balloon.balloon_max_pages) {
	balloon.balloon_min_pages = balloon.balloon_max_pages;
    }

    if (balloon.balloon_init_pages < balloon.balloon_min_pages) {
	balloon.balloon_init_pages = balloon.balloon_min_pages;
    }

    if (balloon.balloon_init_pages > balloon.balloon_max_pages) {
	balloon.balloon_init_pages = balloon.balloon_max_pages;
    }
}

    static int
vballoon_init (void)
{
    NkXIrq   xirq;
    NkOsId   myid  = nkops.nk_id_get();
    NkPhAddr plink = 0;

    while ((plink = nkops.nk_vlink_lookup("vballoon", plink)) != 0) {
	NkDevVlink* vlink = (NkDevVlink*) nkops.nk_ptov(plink);
	if (vlink->s_id == myid) {
	    if (balloon.vlink) {
		printk(KERN_ERR "multiple vballoon server vLINKs detected\n");
		return -EINVAL;
	    }
	    balloon.plink = plink;
	    balloon.vlink = vlink;
	}
    }

    if (!balloon.vlink) {
	return 0;
    }

    balloon.pmem = nkops.nk_pmem_alloc(balloon.plink, 0,
				       sizeof(vballoon_pmem_t));
    if (!balloon.pmem) {
	printk(KERN_ERR "unable to allocate vballoon PMEM region\n");
	return -ENOMEM;
    }
    balloon.vmem = nkops.nk_mem_map(balloon.pmem, sizeof(vballoon_pmem_t));
    if (!balloon.vmem) {
	printk(KERN_ERR "unable to map vballoon PMEM region\n");
	return -ENOMEM;
    }

    xirq = nkops.nk_pxirq_alloc(balloon.plink, 0, myid, 1);
    if (!xirq) {
	printk(KERN_ERR "unable to allocate vballoon XIRQ\n");
	vballoon_fini();
	return -ENOMEM;
    }

    balloon.xirq = nkops.nk_xirq_attach(xirq, vballoon_xirq_handler, 0);
    if (!balloon.xirq) {
	printk(KERN_ERR "unable to attach the vballoon XIRQ handler\n");
	vballoon_fini();
	return -ENOMEM;
    }

    balloon.sirq = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF,
					vballoon_sysconf_handler, 0);
    if (!balloon.sirq) {
	printk(KERN_ERR "unable to attach the vballoon SYSCONF handler\n");
	vballoon_fini();
	return -ENOMEM;
    }

    vballoon_set_limits();

    vballoon_handshake();

    return 0;
}

    static int
balloon_lowmem_shrink (struct shrinker* s, struct shrink_control* sc)
{
    unsigned long freed;
    unsigned long free = global_page_state(NR_FREE_PAGES);
    unsigned long file = global_page_state(NR_FILE_PAGES) -
		         global_page_state(NR_SHMEM);

    if ((free >= balloon.shrink_limit_pages) ||
	(file >= balloon.shrink_limit_pages)) {
	return (sc->nr_to_scan <= 0 ? 0 : -1);
    }

    if (sc->nr_to_scan <= 0) {
	return balloon.driver_pages;
    }

    abort_reservation(&freed);

    return (freed ? 0 : -1);
}

static struct shrinker balloon_lowmem_shrinker = {
	.shrink = balloon_lowmem_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

    static int __init
balloon_init (void)
{
    if (nkops.nk_balloon_ctrl(NK_BALLOON_STATUS, 0, 0) == NK_BALLOON_DISABLED){
	return 0;
    }

    balloon.balloon_pages      = balloon_pages();
    balloon.current_pages      = totalram_pages - balloon.balloon_pages;
    balloon.target_pages       = balloon.current_pages;
    balloon.resident_pages     = balloon.current_pages;
    balloon.driver_pages       = 0;
    balloon.last_credit        = 0;
    balloon.sleep_time         = 1;
    balloon.balloon_init_pages = 0;
    balloon.balloon_min_pages  = 0;
    balloon.balloon_max_pages  = balloon.balloon_pages;
    balloon.shrink_limit_pages = 0;

    pr_info("vlx_balloon: Initialising balloon driver (%ld pages)\n",
	    balloon.balloon_pages);

    if (!balloon.balloon_pages) {
	return 0;
    }

    init_timer(&balloon_timer);
    balloon_timer.data = 0;
    balloon_timer.function = balloon_alarm;

    balloon_oom_notifier_block.notifier_call = balloon_oom_notifier_call;
    register_oom_notifier(&balloon_oom_notifier_block);

    register_shrinker(&balloon_lowmem_shrinker);

    register_balloon(&balloon_sysdev);

    vballoon_init();

    if (balloon.balloon_init_pages) {
	balloon_set_new_target(balloon.resident_pages +
			       balloon.balloon_init_pages);
    }

    return 0;
}


#define BALLOON_SHOW(name, format, args...)				\
	    static ssize_t						\
        show_##name(struct sys_device *dev,				\
		    struct sysdev_attribute *attr,			\
		    char *buf)						\
	{								\
	    return sprintf(buf, format, ##args);			\
	}								\
	static SYSDEV_ATTR(name, S_IRUGO, show_##name, NULL)

BALLOON_SHOW(current_kb,      "%lu\n", PAGES2KB(balloon.current_pages));
BALLOON_SHOW(balloon_kb,      "%lu\n", PAGES2KB(balloon.balloon_pages));
BALLOON_SHOW(resident_kb,     "%lu\n", PAGES2KB(balloon.resident_pages));
BALLOON_SHOW(balloon_min_kb,  "%lu\n", PAGES2KB(balloon.balloon_min_pages));
BALLOON_SHOW(balloon_max_kb,  "%lu\n", PAGES2KB(balloon.balloon_max_pages));
BALLOON_SHOW(balloon_init_kb, "%lu\n", PAGES2KB(balloon.balloon_init_pages));

    static ssize_t
show_target_kb (struct sys_device *dev,
		struct sysdev_attribute *attr,
		char *buf)
{
    return sprintf(buf, "%lu\n", PAGES2KB(balloon.target_pages));
}

    static ssize_t
store_target_kb(struct sys_device *dev,
	       struct sysdev_attribute *attr,
	       const char *buf,
	       size_t count)
{
    char *endchar;
    unsigned long long target_bytes;

    if (!capable(CAP_SYS_ADMIN))
	return -EPERM;

    target_bytes = simple_strtoull(buf, &endchar, 0) * 1024;

    balloon_set_new_target(target_bytes >> PAGE_SHIFT);

    return count;
}

static SYSDEV_ATTR(target_kb, S_IRUGO | S_IWUSR,
		   show_target_kb, store_target_kb);


    static ssize_t
show_target (struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
    return sprintf(buf, "%llu\n",
	           (unsigned long long)balloon.target_pages
   	           << PAGE_SHIFT);
}

    static ssize_t
store_target (struct sys_device *dev,
	      struct sysdev_attribute *attr,
	      const char *buf,
	      size_t count)
{
    char *endchar;
    unsigned long long target_bytes;

    if (!capable(CAP_SYS_ADMIN))
	return -EPERM;

    target_bytes = memparse(buf, &endchar);

    balloon_set_new_target(target_bytes >> PAGE_SHIFT);

    return count;
}

static SYSDEV_ATTR(target, S_IRUGO | S_IWUSR,
		   show_target, store_target);

static struct sysdev_attribute *balloon_attrs[] = {
    &attr_target_kb,
    &attr_target,
};

static struct attribute *balloon_info_attrs[] = {
    &attr_current_kb.attr,
    &attr_balloon_kb.attr,
    &attr_resident_kb.attr,
    &attr_balloon_min_kb.attr,
    &attr_balloon_max_kb.attr,
    &attr_balloon_init_kb.attr,
    NULL
};

static struct attribute_group balloon_info_group = {
    .name = "info",
    .attrs = balloon_info_attrs,
};

static struct sysdev_class balloon_sysdev_class = {
    .name = BALLOON_CLASS_NAME,
};

static int register_balloon(struct sys_device *sysdev)
{
    int i, error;

    error = sysdev_class_register(&balloon_sysdev_class);
    if (error)
	return error;

    sysdev->id = 0;
    sysdev->cls = &balloon_sysdev_class;

    error = sysdev_register(sysdev);
    if (error) {
	sysdev_class_unregister(&balloon_sysdev_class);
	return error;
    }

    for (i = 0; i < ARRAY_SIZE(balloon_attrs); i++) {
	error = sysdev_create_file(sysdev, balloon_attrs[i]);
	if (error)
	    goto fail;
    }

    error = sysfs_create_group(&sysdev->kobj, &balloon_info_group);
    if (error)
	goto fail;

    return 0;

fail:
    while (--i >= 0)
	sysdev_remove_file(sysdev, balloon_attrs[i]);
    sysdev_unregister(sysdev);
    sysdev_class_unregister(&balloon_sysdev_class);
    return error;
}

core_initcall(balloon_init);

MODULE_LICENSE("GPL");

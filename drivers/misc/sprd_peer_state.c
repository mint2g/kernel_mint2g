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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <nk/nkern.h>

/* AP States */
#define AP_STAT_NORMAL 0
#define AP_STAT_DIM    1
#define AP_STAT_SLEEP  2

struct peer_state {
	int *state;
	int state_before_suspend;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static struct peer_state peer_state;

static const char vlink_name[] = "peer_state";

static NkDevVlink* peer_state_vlink_lookup(void)
{
	NkDevVlink* vlink;
	NkPhAddr    plink = 0;

	plink = nkops.nk_vlink_lookup(vlink_name, plink);
	if (0 == plink) {
		printk("Can't find the vlink [%s]!\n", vlink_name);
		return NULL;
	}
	vlink = nkops.nk_ptov(plink);
	pr_err("peer_state: vlink info: s_id = %d, c_id = %d.\n",
			vlink->s_id, vlink->c_id);
	return vlink;
}

void *alloc_pmem(NkDevVlink* vlink, unsigned int size)
{
	void *pmem = NULL;

	NkPhAddr paddr;
	NkPhAddr plink = nkops.nk_vtop(vlink);

	/* Allocate persistent shared memory */
	paddr  = nkops.nk_pmem_alloc(plink, 0, size);
	if (paddr == 0)
		return NULL;

	pmem = (void *) nkops.nk_mem_map(paddr, size);
	if (pmem == 0) {
		printk("error while mapping\n");
	}
	return pmem;
}

static void set_state(struct peer_state *dev, int stat)
{
	*(dev->state) = stat;
}

static int get_state(struct peer_state *dev)
{
	return *(dev->state);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void peer_state_early_suspend (struct early_suspend* es)
{
	set_state(&peer_state, AP_STAT_DIM);
	pr_err("%s state(%x): %d\n", __func__, (unsigned int)peer_state.state,
			*peer_state.state);
}

static void peer_state_late_resume (struct early_suspend* es)
{
	set_state(&peer_state, AP_STAT_NORMAL);
	pr_err("%s state(%x): %d\n", __func__, (unsigned int)peer_state.state,
			*peer_state.state);
}
#endif

static int peer_state_suspend(struct platform_device *pdev, pm_message_t state)
{
	peer_state.state_before_suspend = get_state(&peer_state);
	set_state(&peer_state, AP_STAT_SLEEP);
	pr_err("%s state(%x): %d\n", __func__, (unsigned int)peer_state.state,
			*peer_state.state);
	return 0;
}

static int peer_state_resume(struct platform_device *pdev)
{
	set_state(&peer_state, peer_state.state_before_suspend);
	pr_err("%s state(%x): %d\n", __func__, (unsigned int)peer_state.state,
			*peer_state.state);
	return 0;
}

static int peer_state_probe(struct platform_device *pdev)
{

	NkDevVlink* vlink = peer_state_vlink_lookup();
	if (vlink == 0)
		return -1;

	peer_state.state = alloc_pmem(vlink, 4);
	if (peer_state.state == NULL)
		return -ENOMEM;

	pr_err("%s state(%x): %d\n", __func__, (unsigned int)peer_state.state,
			*peer_state.state);
#ifdef CONFIG_HAS_EARLYSUSPEND
	peer_state.early_suspend.suspend = peer_state_early_suspend;
	peer_state.early_suspend.resume  = peer_state_late_resume;
	peer_state.early_suspend.level   = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&peer_state.early_suspend);
#endif

	return 0;
}

static struct platform_driver peer_state_driver = {
	.probe = peer_state_probe,
	.suspend = peer_state_suspend,
	.resume = peer_state_resume,
	.driver = {
		.name = "peer_state",
		.owner = THIS_MODULE,
	},
};

static int __init peer_state_init(void)
{
	return platform_driver_register(&peer_state_driver);
}

static void __init peer_state_exit(void)
{
	return platform_driver_unregister(&peer_state_driver);
}

module_init(peer_state_init);
module_exit(peer_state_exit);


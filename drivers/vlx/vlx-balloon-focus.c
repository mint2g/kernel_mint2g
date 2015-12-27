/*
 ****************************************************************
 *
 *  Component: VLX memory balloon client driver
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <nk/nkern.h>
#include <vlx/vballoon_common.h>

    extern int
focus_register_client (struct notifier_block* nb);

    extern int
focus_unregister_client (struct notifier_block* nb);

#define TRACE(format, args...)	\
	printk(KERN_INFO "vlx_balloon_focus: " format, ## args)

#define ETRACE(format, args...)	\
	printk(KERN_ERR "vlx_balloon_focus: " format, ## args)

typedef struct vballoon_t {
    NkPhAddr           plink;	/* server vLINK physical address */
    NkDevVlink*        vlink;	/* server vLINK virtual address */
    NkPhAddr           pmem;	/* shared PMEM physical address */
    vballoon_pmem_t*   vmem;	/* shared PMEM virtual address */
    NkXIrq             xirq;	/* notification XIRQ */
    struct vballoon_t* next;	/* next vballoon server descriptor */
} vballoon_t;

static vballoon_t* vballoons;	/* list of all vballoon servers */
static NkOsId      focus;	/* VM in focus */
static NkXIrqId    sc_id;	/* SYSCONF XIRQ ID */

static DEFINE_SPINLOCK(vballoon_lock);

static struct notifier_block focus_nb;	/* focus change notification block */

    static void
vballoon_sysconf_post (vballoon_t* vballoon)
{
    wmb();
    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, vballoon->vlink->s_id);
}

    static int
vballoon_handshake (vballoon_t* vballoon)
{
    volatile int* my_state;
    int           peer_state;

    my_state   = &vballoon->vlink->c_state;
    peer_state = vballoon->vlink->s_state;

    switch (*my_state) {
        case NK_DEV_VLINK_OFF:
            if (peer_state != NK_DEV_VLINK_ON) {
                *my_state = NK_DEV_VLINK_RESET;
                vballoon_sysconf_post(vballoon);
            }
            break;
        case NK_DEV_VLINK_RESET:
            if (peer_state != NK_DEV_VLINK_OFF) {
		vballoon->vmem->target_pages = 0;
		wmb();
                *my_state = NK_DEV_VLINK_ON;
                vballoon_sysconf_post(vballoon);
            }
            break;
        case NK_DEV_VLINK_ON:
            if (peer_state == NK_DEV_VLINK_OFF) {
                *my_state = NK_DEV_VLINK_RESET;
                vballoon_sysconf_post(vballoon);
            }
            break;
    }

    return (*my_state  == NK_DEV_VLINK_ON) &&
           (peer_state == NK_DEV_VLINK_ON);
}

    static void
vballoon_set_target (vballoon_t* vballoon, int focus)
{
    nku32_f pages;

    if (focus) {
	pages = vballoon->vmem->resident_pages +
		vballoon->vmem->balloon_max_pages; 
    } else {
	pages = vballoon->vmem->resident_pages +
		vballoon->vmem->balloon_min_pages; 
    }
    vballoon->vmem->target_pages = pages;
    wmb();

    nkops.nk_xirq_trigger(vballoon->xirq, vballoon->vlink->s_id);
}

    static void
vballoon_focus_set (void)
{
    unsigned long flags;
    vballoon_t*   vballoon;
    vballoon_t*   fballoon = 0;

    spin_lock_irqsave(&vballoon_lock, flags);

    vballoon = vballoons;
    while (vballoon) {
	if (vballoon_handshake(vballoon) && (vballoon->vlink->s_id == focus)) {
	    fballoon = vballoon;
	}
	vballoon = vballoon->next;
    }

    if (fballoon) {
        vballoon = vballoons;
        while (vballoon) {
	    if (vballoon_handshake(vballoon) && (vballoon != fballoon)) {
	        vballoon_set_target(vballoon, 0);
	    }
	    vballoon = vballoon->next;
        }
 
        vballoon_set_target(fballoon, 1);
    }

    spin_unlock_irqrestore(&vballoon_lock, flags);
}

    static int
vballoon_focus_handler (struct notifier_block* nb,
		        unsigned long          event,
		        void*                  data)
{
    (void)nb; (void)data;
    focus = (NkOsId)event;
    vballoon_focus_set();
    return NOTIFY_DONE;
}

    static void
vballoon_sysconf_handler (void* cookie, NkXIrq xirq)
{
    (void)cookie; (void)xirq;
    vballoon_focus_set();
}

    static void
vballoon_server_fini (vballoon_t* vballoon)
{
    vballoon->vlink->s_state = NK_DEV_VLINK_OFF;

    vballoon_sysconf_post(vballoon);

    if (vballoon->vmem) {
	nkops.nk_mem_unmap(vballoon->vmem, vballoon->pmem,
			   sizeof(vballoon_pmem_t));
	vballoon->vmem = 0;
    }
}

    static int
vballoon_server_init (vballoon_t* vballoon)
{
    vballoon->pmem = nkops.nk_pmem_alloc(vballoon->plink, 0,
				         sizeof(vballoon_pmem_t));
    if (!vballoon->pmem) {
	ETRACE("unable to allocate vballoon PMEM region\n");
	return -ENOMEM;
    }

    vballoon->vmem = nkops.nk_mem_map(vballoon->pmem, sizeof(vballoon_pmem_t));
    if (!vballoon->vmem) {
	ETRACE("unable to map vballoon PMEM region\n");
	return -ENOMEM;
    }

    vballoon->xirq = nkops.nk_pxirq_alloc(vballoon->plink, 0, 
					  vballoon->vlink->s_id, 1);
    if (!vballoon->xirq) {
	ETRACE("unable to allocate vballoon XIRQ\n");
	vballoon_server_fini(vballoon);
	return -ENOMEM;
    }

    return 0;
}

    static int
vballoon_server_alloc (NkPhAddr plink, NkDevVlink* vlink)
{
    vballoon_t* vballoon;
    int         res;

    vballoon = (vballoon_t*)kzalloc(sizeof(vballoon_t), GFP_KERNEL);
    if (!vballoon) {
	ETRACE("unable to allocate vballoon descriptor\n");
	return -ENOMEM;
    }

    vballoon->plink = plink;
    vballoon->vlink = vlink;

    res = vballoon_server_init(vballoon);
    if (res) {
	kfree(vballoon);
  	return res;
    }

    vballoon->next = vballoons;
    vballoons      = vballoon;

    return 0;
}

    static void
vballoon_focus_exit (void)
{
    if (focus_nb.notifier_call) {
	focus_unregister_client(&focus_nb);
	focus_nb.notifier_call = 0;
    }

    if (sc_id) {
	nkops.nk_xirq_detach(sc_id);
	sc_id = 0;
    }

    while (vballoons) {
	vballoon_t* vballoon = vballoons;
	vballoons = vballoon->next;
	vballoon_server_fini(vballoon);
	kfree(vballoon);
    }
}

    static int
vballoon_focus_init (void)
{
    int      res;
    NkOsId   myid  = nkops.nk_id_get();
    NkPhAddr plink = 0;

    pr_info("vlx_balloon_focus: Initialising balloon focus driver.\n");

    while ((plink = nkops.nk_vlink_lookup("vballoon", plink)) != 0) {
	NkDevVlink* vlink = (NkDevVlink*)nkops.nk_ptov(plink);
	if (vlink->c_id == myid) {
	    int res = vballoon_server_alloc(plink, vlink);
	    if (res) {
	  	return res;
	    }
	}
    }

    if (!vballoons) {
	return 0;
    }

    sc_id = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF, vballoon_sysconf_handler, 0);
    if (!sc_id) {
	ETRACE("unable to attach the vballoon SYSCONF handler\n");
	vballoon_focus_exit();
	return -ENOMEM;
    }

    focus_nb.notifier_call = vballoon_focus_handler;
    res = focus_register_client(&focus_nb);
    if (res) {
	ETRACE("unable to attach focus handler\n");
	focus_nb.notifier_call = 0;
	vballoon_focus_exit();
	return res;
    }

#if 0
	/* switch balloon focus to the current VM */
    focus = myid;

    vballoon_focus_set();
#endif

    return 0;
}

module_init(vballoon_focus_init);
module_exit(vballoon_focus_exit);

MODULE_LICENSE("GPL");

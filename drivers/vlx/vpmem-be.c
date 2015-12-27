/*
 ****************************************************************
 *
 *  Component: VLX virtual Android Physical Memory backend driver
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
 *    Christophe Lizzi (Christophe.Lizzi@redbend.com)
 *    Vladimir Grouzdev (Vladimir.Grouzdev@redbend.com)
 *
 ****************************************************************
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/slab.h>

#undef VPMEM_DEBUG

#define VPMEM_DRV_NAME "vpmem-be"

#include "vlx/vpmem_common.h"
#include "vpmem.h"

    static int __init
vpmem_dev_init (vpmem_dev_t* vpmem)
{
    NkPhAddr       pdev;
    vpmem_shdev_t* shdev;
    vpmem_dev_t*   peer;

    pdev = nkops.nk_pdev_alloc(vpmem->plink, 0, sizeof(vpmem_shdev_t));
    if (!pdev) {
	ETRACE("vpmem %s: nk_pdev_alloc(%d) failed\n",
	       vpmem->name, sizeof(vpmem_shdev_t));
	return -ENOMEM;
    }
    shdev = (vpmem_shdev_t*)nkops.nk_ptov(pdev);

    vpmem->info = nkops.nk_ptov(vpmem->vlink->s_info);
    if (!vpmem->info || !*vpmem->info) {
	vpmem->info = "vpmem";
    }

    vpmem_info_name(vpmem->info, vpmem->name, sizeof(vpmem->name));

    peer = vpmem_dev_peer(vpmem);
    if (peer) {
	vpmem->pmem_phys = peer->pmem_phys;
        vpmem->pmem_size = peer->pmem_size;
    } else {
        vpmem->pmem_size = vpmem_info_size(vpmem->info);
        vpmem->pmem_phys = vpmem_info_base(vpmem->info);
	if (!vpmem->pmem_phys) {
            vpmem->pmem_phys = nkops.nk_pmem_alloc(vpmem->plink, 0,
					           vpmem->pmem_size);
	    if (!vpmem->pmem_phys) {
	        ETRACE("vpmem %s: nk_pmem_alloc(%d bytes) failed\n",
	               vpmem->name, vpmem->pmem_size);
	        return -ENOMEM;
	    }
	}
    }

    shdev->base = vpmem->pmem_phys;
    shdev->size = vpmem->pmem_size;

    vpmem->vlink->s_state = NK_DEV_VLINK_ON;

    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, vpmem->vlink->c_id);

    DTRACE1("%s [0x%x..0x%x] initialized\n",
	    vpmem->name, vpmem->pmem_phys,
	    vpmem->pmem_phys + vpmem->pmem_size);

    return 0;
}

    static void __exit
vpmem_dev_exit (vpmem_dev_t* vpmem)
{
    vpmem_unmap(vpmem);

    DTRACE1("%s [0x%x..0x%x] removed\n",
	    vpmem->name, vpmem->pmem_phys,
	    vpmem->pmem_phys + vpmem->pmem_size);

    vpmem->vlink->s_state = NK_DEV_VLINK_OFF;

    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, vpmem->vlink->c_id);
}

    static int __init
vpmem_init (void)
{
    return vpmem_module_init(0, vpmem_dev_init);
}

    static void __exit
vpmem_exit (void)
{
    vpmem_module_exit(vpmem_dev_exit);
}

module_init(vpmem_init);
module_exit(vpmem_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("VLX virtual Android Physical Memory back-end");

/*
 ****************************************************************
 *
 *  Component: VLX virtual Android Physical Memory frontend driver
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
#include <linux/delay.h>

#undef VPMEM_DEBUG

#define VPMEM_DRV_NAME "vpmem-fe"

#include "vlx/vpmem_common.h"
#include "vpmem.h"

#ifndef CONFIG_ANDROID_PMEM
#error "Android Physical Memory (pmem) driver is not configured"
#endif

// Android Physical Memory driver entry points.
extern int vpmem_pmem_probe (struct platform_device *pdev);
extern int vpmem_pmem_remove(struct platform_device *pdev);

    static int __init
vpmem_dev_init (vpmem_dev_t* vpmem)
{
    NkPhAddr       pdev;
    vpmem_shdev_t* shdev;
    int            err;

    vpmem->info = nkops.nk_ptov(vpmem->vlink->c_info);
    if (!vpmem->info || !*vpmem->info) {
	vpmem->info = "vpmem";
    }

    vpmem_info_name(vpmem->info, vpmem->name, sizeof(vpmem->name));

    vpmem->id = vpmem_info_id(vpmem->info);

	// Wait for the back-end driver...
	//
	// VG_FIXME:
	//     This is a hack... we normally should create an
	//     initialization thread doing this job. The thread could
	//     be awaken from the handshake handler in order to create a
	//     new Linux front-end pmem device once the back-end device
	//     has transited to the "on" state.
	//
    while (vpmem->vlink->s_state != NK_DEV_VLINK_ON) {
	DTRACE1("%s waiting for the back-end driver...\n", vpmem->name);
	msleep_interruptible(250);
    }

    pdev = nkops.nk_pdev_alloc(vpmem->plink, 0, sizeof(vpmem_shdev_t));
    if (!pdev) {
	ETRACE("vpmem %s: nk_pdev_alloc(%d) failed\n",
	        vpmem->name, sizeof(vpmem_shdev_t));
	return -ENOMEM;
    }
    shdev = (vpmem_shdev_t*)nkops.nk_ptov(pdev);
    vpmem->pmem_phys = shdev->base;
    vpmem->pmem_size = shdev->size;

        // Linux platform device
    vpmem->plat_dev.name              = "android_pmem";
    vpmem->plat_dev.id                = vpmem->id;
    vpmem->plat_dev.dev.platform_data = &vpmem->plat_data;

        // Android pmem platform data
    vpmem->plat_data.name         = vpmem->name;
    vpmem->plat_data.start        = vpmem->pmem_phys;
    vpmem->plat_data.size         = vpmem->pmem_size;
    vpmem->plat_data.no_allocator = 1;
    vpmem->plat_data.cached       = 1;
    vpmem->plat_data.buffered     = 0;

    err = platform_device_register(&vpmem->plat_dev);
    if (err != 0) {
	ETRACE("vpmem %s: platform_device_register() failed, err %d\n",
	       vpmem->name, err);
	return err;
    }

    DTRACE1("%s [0x%x..0x%x] initialized\n",
 	    vpmem->name, vpmem->pmem_phys,
	    vpmem->pmem_phys + vpmem->pmem_size);

    return 0;
}

    static void __exit
vpmem_dev_exit (vpmem_dev_t* vpmem)
{
    platform_device_unregister(&vpmem->plat_dev);

    vpmem_unmap(vpmem);

    DTRACE1("%s [0x%x..0x%x] removed\n",
	    vpmem->name, vpmem->pmem_phys,
	    vpmem->pmem_phys + vpmem->pmem_size);
}

    static int __init
vpmem_init (void)
{
    return vpmem_module_init(1, vpmem_dev_init);
}

    static void __exit
vpmem_exit (void)
{
    vpmem_module_exit(vpmem_dev_exit);
}

module_init(vpmem_init);
module_exit(vpmem_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("VLX virtual Android Physical Memory front-end");

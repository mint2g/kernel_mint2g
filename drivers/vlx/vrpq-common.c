/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Remote Procedure Queue (VRPQ).                    *
 *             VRPQ front-end/back-end kernel driver common services.        *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  This program is free software: you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License Version 2           *
 *  as published by the Free Software Foundation.                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  Version 2 along with this program.                                       *
 *  If not, see <http://www.gnu.org/licenses/>.                              *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "vrpq.h"
#include "pmem-user.h"

#define VLX_SERVICES_THREADS
#include <linux/version.h>
#include <linux/kthread.h>
#include "vlx-services.c"

static LIST_HEAD(vrpq_drvs);
static DEFINE_SPINLOCK(vrpq_lock);

    /*
     *
     */
    VrpqDrv*
vrpq_drv_find (unsigned int major)
{
    VrpqDrv* vrpq_drv;

    list_for_each_entry(vrpq_drv, &vrpq_drvs, link) {
	if (vrpq_drv->chrdev_major == major) {
	    return vrpq_drv;
	}
    }
    return NULL;
}

    /*
     *
     */
    VrpqDev*
vrpq_dev_find (unsigned int major, unsigned int minor)
{
    VrpqDrv* vrpq_drv;
    VrpqDev* vrpq_dev;

    vrpq_drv = vrpq_drv_find(major);
    if (!vrpq_drv) {
	return NULL;
    }

    if (minor >= vrpq_drv->parent_drv->nr_units) {
	return NULL;
    }

    vrpq_dev = &vrpq_drv->devs[minor];
    if (!vrpq_dev->gen.enabled) {
	return NULL;
    }

    return vrpq_dev;
}

    /*
     *
     */
    void
vrpq_gen_drv_cleanup (VrpqDrv* vrpq_drv)
{
    if (vrpq_drv->class) {
	class_destroy(vrpq_drv->class);
	vrpq_drv->class = NULL;
    }

    if (vrpq_drv->chrdev_major) {
	spin_lock(&vrpq_lock);
	list_del(&vrpq_drv->link);
	spin_unlock(&vrpq_lock);
	unregister_chrdev(vrpq_drv->major, vrpq_drv->name);
	vrpq_drv->chrdev_major = 0;
    }

    if (vrpq_drv->devs) {
	kfree(vrpq_drv->devs);
	vrpq_drv->devs = NULL;
    }
}

    /*
     *
     */
    int
vrpq_gen_drv_init (VrpqDrv* vrpq_drv)
{
    VlinkDrv* parent_drv = vrpq_drv->parent_drv;
    int       diag;

    vrpq_drv->devs = (VrpqDev*) kzalloc(parent_drv->nr_units * sizeof(VrpqDev),
					GFP_KERNEL);
    if (!vrpq_drv->devs) {
	VRPQ_ERROR("%s: cannot allocate device descriptors (%d bytes)\n",
		   vrpq_drv->name, parent_drv->nr_units * sizeof(VrpqDev));
	return -ENOMEM;
    }

    if (vrpq_drv_find(vrpq_drv->major)) {
	VRPQ_ERROR("%s: major number '%d' already used\n",
		   vrpq_drv->name, vrpq_drv->major);
	return -EBUSY;
    }

    diag = register_chrdev(vrpq_drv->major,
			   vrpq_drv->name,
			   vrpq_drv->fops);
    if ((vrpq_drv->major && diag) || (!vrpq_drv->major && diag <= 0)) {
	VRPQ_ERROR("%s: cannot register major number '%d'\n",
		   vrpq_drv->name, vrpq_drv->major);
	return diag;
    }

    spin_lock(&vrpq_lock);
    list_add(&vrpq_drv->link, &vrpq_drvs);
    spin_unlock(&vrpq_lock);
    vrpq_drv->chrdev_major = vrpq_drv->major ? vrpq_drv->major : diag;

    vrpq_drv->class = class_create(THIS_MODULE, vrpq_drv->name);
    if (IS_ERR(vrpq_drv->class)) {
	VRPQ_ERROR("%s: cannot create the device class\n", vrpq_drv->name);
	diag = PTR_ERR(vrpq_drv->class);
	vrpq_drv->class = NULL;
	return diag;
    }

    return 0;
}

    /*
     *
     */
    void
vrpq_gen_dev_cleanup (VrpqGenDev* dev)
{
    VrpqDrv*     vrpq_drv = dev->vrpq_drv;
    Vlink*       vlink    = dev->vlink;
    unsigned int i;

    for (i = 0; i < dev->xirq_id_nr; i++) {
	nkops.nk_xirq_detach(dev->xirq_id[i]);
	dev->xirq_id[i] = 0;
    }
    dev->xirq_id_nr = 0;

    if (dev->class_dev) {
	device_destroy(vrpq_drv->class,
		       MKDEV(vrpq_drv->major, vlink->unit));
	dev->class_dev = NULL;
    }
}

    /*
     *
     */
    static int
vrpq_pmem_setup (VrpqGenDev* dev)
{
    VrpqDrv*        vrpq_drv = dev->vrpq_drv;
    Vlink*          vlink    = dev->vlink;
    NkPhAddr        plink    = nkops.nk_vtop(vlink->nk_vlink);
    VrpqPmemLayout* layout   = &dev->pmem_layout;
    NkPhAddr        paddr;
    NkPhAddr        pend;
    NkPhSize        sz;
    nku8_f*         vpmem;
    VrpqRingIdx     reqs_nr;
    int             diag;

    diag = vrpq_drv->prop_get(vlink, VRPQ_PROP_PMEM_SIZE, &sz);
    if (diag) {
	VLINK_ERROR(vlink, "cannot get the pmem size\n");
	return diag;
    }

    paddr = nkops.nk_pmem_alloc(plink,
				vrpq_drv->resc_id_base +
				VRPQ_RESC_PMEM,
				sz);
    if (!paddr) {
	VLINK_ERROR(vlink, "cannot allocate %d bytes of pmem\n", sz);
	return -ENOMEM;
    }

    diag = vrpq_drv->prop_get(vlink, VRPQ_PROP_RING_REQS_MAX, &reqs_nr);
    if (diag) {
	VLINK_ERROR(vlink, "cannot get the maximum number of requests "
		    "in the ring\n");
	return diag;
    }

    if (reqs_nr < VRPQ_RING_REQS_MIN) {
	VLINK_ERROR(vlink, "the maximum number of requests in the ring "
		    "is %d whereas it should be at least %d\n",
		    reqs_nr, (int)VRPQ_RING_REQS_MIN);
	return -EINVAL;
    }

    if (reqs_nr & (reqs_nr - 1)) {   /* is reqs_nr a power of 2 ? */
	VLINK_ERROR(vlink, "the maximum number of requests in the ring "
		    "is not a power of 2: %d\n", reqs_nr);
	return -EINVAL;
    }

    pend  = (paddr + sz) & ~(PMEM_USER_PERF_ALIGN - 1);
    paddr = (paddr + PMEM_USER_PERF_ALIGN - 1) & ~(PMEM_USER_PERF_ALIGN - 1);
    sz    = pend - paddr;

    dev->pmem_paddr = paddr;
    vpmem           = nkops.nk_mem_map(dev->pmem_paddr, sz);

    layout->pmemVaddr    = vpmem;
    layout->pmemSize     = sz;
    layout->reqCount     = reqs_nr;
    layout->reqSize      = layout->reqCount * sizeof(*layout->reqs);
    layout->rspCount     = VRPQ_RING_RESPS_MAX;
    layout->rspSize      = layout->rspCount * sizeof(*layout->resps);
    layout->admParamSize = VRPQ_ADM_PARAM_TOTAL_SIZE;

    layout->reqRingGbl   = VRPQ_PMEM_NEXT(vpmem, 0);
    layout->rspRingGbl   = VRPQ_PMEM_NEXT(layout->reqRingGbl,
					  sizeof(*layout->reqRingGbl));
    layout->admParams    = VRPQ_PMEM_NEXT(layout->rspRingGbl,
					  sizeof(*layout->rspRingGbl));
    layout->resps        = VRPQ_PMEM_NEXT(layout->admParams,
					  layout->admParamSize);
    layout->reqs         = VRPQ_PMEM_NEXT_ALIGN(layout->resps,
						layout->rspSize,
						PAGE_SIZE);
    layout->usrParams    = VRPQ_PMEM_NEXT_ALIGN(layout->reqs,
						layout->reqSize,
						PAGE_SIZE);
    layout->usrParamSize = (sz + (VrpqSize)(((nku8_f*)vpmem) -
					    ((nku8_f*)layout->usrParams)));

    if (((long)layout->usrParamSize) < layout->reqSize) {
	VLINK_ERROR(vlink, "not enough pmem for in/out params: %d bytes\n",
		    layout->usrParamSize);
	return -EINVAL;
    }

    VLINK_DTRACE(vlink, "pmem:             [0x%p-0x%p]\n",
		 vpmem, vpmem + sz);
    VLINK_DTRACE(vlink, "req ring global:  [0x%p-0x%p]\n",
		 layout->reqRingGbl, layout->reqRingGbl + 1);
    VLINK_DTRACE(vlink, "resp ring global: [0x%p-0x%p]\n",
		 layout->rspRingGbl, layout->rspRingGbl + 1);
    VLINK_DTRACE(vlink, "admin params:     [0x%p-0x%p]\n",
		 layout->admParams,
		 (nku8_f*) layout->admParams + layout->admParamSize);
    VLINK_DTRACE(vlink, "resps:            [0x%p-0x%p]\n",
		 layout->resps, layout->resps + layout->rspCount);
    VLINK_DTRACE(vlink, "reqs:             [0x%p-0x%p]\n",
		 layout->reqs, layout->reqs + layout->reqCount);
    VLINK_DTRACE(vlink, "user params:      [0x%p-0x%p]\n",
		 layout->usrParams,
		 (nku8_f*) layout->usrParams + layout->usrParamSize);

    return 0;
}

    /*
     *
     */
    int
vrpq_gen_dev_init (VrpqGenDev* dev)
{
    VrpqDrv* vrpq_drv = dev->vrpq_drv;
    Vlink*   vlink    = dev->vlink;
    NkPhAddr plink    = nkops.nk_vtop(vlink->nk_vlink);
    int      diag;

    if ((diag = vrpq_pmem_setup(dev)) != 0) {
	return diag;
    }

    dev->cxirqs = nkops.nk_pxirq_alloc(plink,
				       vrpq_drv->resc_id_base +
				       VRPQ_RESC_XIRQ_CLT,
				       vlink->nk_vlink->c_id,
				       2);
    if (!dev->cxirqs) {
	VLINK_ERROR(vlink, "cannot allocate the client xirqs\n");
	return -ENOMEM;
    }

    dev->sxirqs = nkops.nk_pxirq_alloc(plink,
				       vrpq_drv->resc_id_base +
				       VRPQ_RESC_XIRQ_SRV,
				       vlink->nk_vlink->s_id,
				       2);
    if (!dev->sxirqs) {
	VLINK_ERROR(vlink, "cannot allocate the server xirqs\n");
	return -ENOMEM;
    }

    if ((diag = vlink_ops_register(vlink,
				   vrpq_drv->vops,
				   dev)) != 0) {
	VLINK_ERROR(vlink, "cannot register vlink callback routines\n");
	return diag;
    }

    dev->class_dev = device_create(vrpq_drv->class,
				   NULL,
				   MKDEV(vrpq_drv->major, vlink->unit),
				   NULL,
				   "%s%d",
				   vrpq_drv->name,
				   vlink->unit);
    if (IS_ERR(dev->class_dev)) {
	VLINK_ERROR(vlink, "cannot create the '%s%d' device\n",
		    vrpq_drv->name, vlink->unit);
	diag = PTR_ERR(dev->class_dev);
	dev->class_dev = NULL;
	return diag;
    }

    return 0;
}

    /*
     *
     */
    void*
vrpq_thread_create (VrpqGenDev* dev, int(*f)(void*), void* cookie)
{
    Vlink*        vlink = dev->vlink;
    vlx_thread_t* thread;
    int           diag;
    unsigned int  len;
    char*         name;

    len = NK_DEV_VLINK_NAME_LIMIT + sizeof("-xxx") + 4;

    thread = (vlx_thread_t*) kzalloc(sizeof(vlx_thread_t) + len, GFP_KERNEL);
    if (!thread) {
	VLINK_ERROR(vlink,
		    "cannot allocate a vlx_thread_t struct (%d bytes)\n",
		    sizeof(vlx_thread_t) + len);
	return ERR_PTR(-ENOMEM);
    }

    name = ((char*) thread) + sizeof(vlx_thread_t);
    sprintf(name, "%s-%d", dev->vrpq_drv->name, vlink->nk_vlink->link);

    diag = vlx_thread_start(thread, f, cookie, name);
    if (diag) {
	VLINK_ERROR(vlink, "cannot create a VRPQ kernel thread\n");
	kfree(thread);
	return ERR_PTR(diag);
    }

    return (void*) thread;
}

    /*
     *
     */
    void
vrpq_thread_delete (void** thread)
{
    if (*thread) {
	vlx_thread_join(*thread);
	kfree(*thread);
	*thread = NULL;
    }
}

    /*
     *
     */
    static void
vrpq_xirq_fake_handler (void* cookie, NkXIrq xirq)
{
}

    /*
     *
     */
    int
vrpq_xirq_attach (VrpqGenDev* dev, NkXIrq xirq, NkXIrqHandler hdl)
{
    Vlink*       vlink = dev->vlink;
    unsigned int idx;
    NkXIrqId     fake_id;

    idx = dev->xirq_id_nr;

    VLINK_ASSERT(idx < ARRAY_SIZE(dev->xirq_id));
    VLINK_ASSERT(!dev->xirq_id[idx]);

    fake_id = nkops.nk_xirq_attach(xirq, vrpq_xirq_fake_handler, dev);
    if (!fake_id) {
	VLINK_ERROR(vlink, "cannot attach fake handler for xirq %d\n", xirq);
	return -ENOMEM;
    }

    nkops.nk_xirq_mask(xirq);
    dev->xirq_id[idx] = nkops.nk_xirq_attach(xirq, hdl, dev);
    nkops.nk_xirq_detach(fake_id);

    if (!dev->xirq_id[idx]) {
	VLINK_ERROR(vlink, "cannot attach handler for xirq %d\n", xirq);
	return -ENOMEM;
    }

    dev->xirq_id_nr++;
    return 0;
}

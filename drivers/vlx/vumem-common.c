/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual User MEMory Buffers (VUMEM).                      *
 *             VUMEM front-end/back-end kernel driver common services.       *
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
#include <linux/sched.h>
#include "vumem.h"

#define VLX_SERVICES_THREADS
#include <linux/version.h>
#include <linux/kthread.h>
#include "vlx-services.c"

static LIST_HEAD(vumem_drvs);
static DEFINE_SPINLOCK(vumem_lock);

    /*
     *
     */
    VumemDrv*
vumem_drv_find (unsigned int major)
{
    VumemDrv* vumem_drv;

    list_for_each_entry(vumem_drv, &vumem_drvs, link) {
	if (vumem_drv->chrdev_major == major) {
	    return vumem_drv;
	}
    }
    return NULL;
}

    /*
     *
     */
    VumemDev*
vumem_dev_find (unsigned int major, unsigned int minor)
{
    VumemDrv* vumem_drv;
    VumemDev* vumem_dev;

    vumem_drv = vumem_drv_find(major);
    if (!vumem_drv) {
	return NULL;
    }

    if (minor >= vumem_drv->parent_drv->nr_units) {
	return NULL;
    }

    vumem_dev = &vumem_drv->devs[minor];
    if (!vumem_dev->gen.enabled) {
	return NULL;
    }

    return vumem_dev;
}

    /*
     *
     */
    void
vumem_gen_drv_cleanup (VumemDrv* vumem_drv)
{
    if (vumem_drv->class) {
	class_destroy(vumem_drv->class);
	vumem_drv->class = NULL;
    }

    if (vumem_drv->chrdev_major) {
	spin_lock(&vumem_lock);
	list_del(&vumem_drv->link);
	spin_unlock(&vumem_lock);
	unregister_chrdev(vumem_drv->major, vumem_drv->name);
	vumem_drv->chrdev_major = 0;
    }

    if (vumem_drv->devs) {
	kfree(vumem_drv->devs);
	vumem_drv->devs = NULL;
    }
}

    /*
     *
     */
    int
vumem_gen_drv_init (VumemDrv* vumem_drv)
{
    VlinkDrv* parent_drv = vumem_drv->parent_drv;
    int       diag;

    vumem_drv->devs = (VumemDev*) kzalloc((parent_drv->nr_units *
					   sizeof(VumemDev)),
					  GFP_KERNEL);
    if (!vumem_drv->devs) {
	VUMEM_ERROR("%s: cannot allocate device descriptors (%d bytes)\n",
		    vumem_drv->name, parent_drv->nr_units * sizeof(VumemDev));
	return -ENOMEM;
    }

    if (vumem_drv_find(vumem_drv->major)) {
	VUMEM_ERROR("%s: major number '%d' already used\n",
		    vumem_drv->name, vumem_drv->major);
	return -EBUSY;
    }

    diag = register_chrdev(vumem_drv->major,
			   vumem_drv->name,
			   vumem_drv->fops);
    if ((vumem_drv->major && diag) || (!vumem_drv->major && diag <= 0)) {
	VUMEM_ERROR("%s: cannot register major number '%d'\n",
		    vumem_drv->name, vumem_drv->major);
	return diag;
    }

    spin_lock(&vumem_lock);
    list_add(&vumem_drv->link, &vumem_drvs);
    spin_unlock(&vumem_lock);
    vumem_drv->chrdev_major = vumem_drv->major ? vumem_drv->major : diag;

    vumem_drv->class = class_create(THIS_MODULE, vumem_drv->name);
    if (IS_ERR(vumem_drv->class)) {
	VUMEM_ERROR("%s: cannot create the device class\n", vumem_drv->name);
	diag = PTR_ERR(vumem_drv->class);
	vumem_drv->class = NULL;
	return diag;
    }

    return 0;
}

    /*
     *
     */
    void
vumem_gen_dev_cleanup (VumemGenDev* dev)
{
    VumemDrv* vumem_drv = dev->vumem_drv;
    Vlink*    vlink     = dev->vlink;
    unsigned int i;

    for (i = 0; i < dev->xirq_id_nr; i++) {
	nkops.nk_xirq_detach(dev->xirq_id[i]);
	dev->xirq_id[i] = 0;
    }
    dev->xirq_id_nr = 0;

    if (dev->class_dev) {
	device_destroy(vumem_drv->class,
		       MKDEV(vumem_drv->major, vlink->unit));
	dev->class_dev = NULL;
    }
}

    /*
     *
     */
    int
vumem_gen_dev_init (VumemGenDev* dev)
{
    VumemDrv* vumem_drv = dev->vumem_drv;
    Vlink*    vlink     = dev->vlink;
    NkPhAddr  plink     = nkops.nk_vtop(vlink->nk_vlink);
    NkPhSize  sz;
    int       diag;

    diag = vumem_drv->prop_get(vlink, VUMEM_PROP_PDEV_SIZE, &sz);
    if (diag) {
	VLINK_ERROR(vlink, "cannot get the pdev size\n");
	return diag;
    }

    dev->pdev_paddr = nkops.nk_pdev_alloc(plink,
					  vumem_drv->resc_id_base +
					  VUMEM_RESC_PDEV,
					  sz);
    if (!dev->pdev_paddr) {
	VLINK_ERROR(vlink, "cannot allocate %d bytes of pdev\n", sz);
	return -ENOMEM;
    }

    dev->pdev_vaddr = nkops.nk_ptov(dev->pdev_paddr);
    dev->pdev_size  = sz;

    dev->cxirqs = nkops.nk_pxirq_alloc(plink,
				       vumem_drv->resc_id_base +
				       VUMEM_RESC_XIRQ_CLT,
				       vlink->nk_vlink->c_id,
				       2);
    if (!dev->cxirqs) {
	VLINK_ERROR(vlink, "cannot allocate the client xirqs\n");
	return -ENOMEM;
    }

    dev->sxirqs = nkops.nk_pxirq_alloc(plink,
				       vumem_drv->resc_id_base +
				       VUMEM_RESC_XIRQ_SRV,
				       vlink->nk_vlink->s_id,
				       2);
    if (!dev->sxirqs) {
	VLINK_ERROR(vlink, "cannot allocate the server xirqs\n");
	return -ENOMEM;
    }

    if ((diag = vlink_ops_register(vlink,
				   vumem_drv->vops,
				   dev)) != 0) {
	VLINK_ERROR(vlink, "cannot register vlink callback routines\n");
	return diag;
    }

    dev->class_dev = device_create(vumem_drv->class,
				   NULL,
				   MKDEV(vumem_drv->major, vlink->unit),
				   NULL,
				   "%s%d",
				   vumem_drv->name,
				   vlink->unit);
    if (IS_ERR(dev->class_dev)) {
	VLINK_ERROR(vlink, "cannot create the '%s%d' device\n",
		    vumem_drv->name, vlink->unit);
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
vumem_thread_create (VumemGenDev* dev, int(*f)(void*), void* cookie)
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
    sprintf(name, "%s-%d", dev->vumem_drv->name, vlink->nk_vlink->link);

    diag = vlx_thread_start(thread, f, cookie, name);
    if (diag) {
	VLINK_ERROR(vlink, "cannot create a VUMEM kernel thread\n");
	kfree(thread);
	return ERR_PTR(diag);
    }

    return (void*) thread;
}

    /*
     *
     */
    void
vumem_thread_delete (void** thread)
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
vumem_xirq_fake_handler (void* cookie, NkXIrq xirq)
{
}

    /*
     *
     */
    int
vumem_xirq_attach (VumemGenDev* dev, NkXIrq xirq, NkXIrqHandler hdl,
		   void* cookie)
{
    Vlink*       vlink = dev->vlink;
    unsigned int idx;
    NkXIrqId     fake_id;

    idx = dev->xirq_id_nr;

    VLINK_ASSERT(idx < ARRAY_SIZE(dev->xirq_id));
    VLINK_ASSERT(!dev->xirq_id[idx]);

    fake_id = nkops.nk_xirq_attach(xirq, vumem_xirq_fake_handler, cookie);
    if (!fake_id) {
	VLINK_ERROR(vlink, "cannot attach fake handler for xirq %d\n", xirq);
	return -ENOMEM;
    }

    nkops.nk_xirq_mask(xirq);
    dev->xirq_id[idx] = nkops.nk_xirq_attach(xirq, hdl, cookie);
    nkops.nk_xirq_detach(fake_id);

    if (!dev->xirq_id[idx]) {
	VLINK_ERROR(vlink, "cannot attach handler for xirq %d\n", xirq);
	return -ENOMEM;
    }

    dev->xirq_id_nr++;
    return 0;
}

    /*
     *
     */
    void
vumem_rpc_wakeup (VumemRpc* rpc)
{
    wake_up(&rpc->wait);
}

    /*
     *
     */
    static void
vumem_rpc_handler (void* cookie, NkXIrq xirq)
{
    vumem_rpc_wakeup((VumemRpc*) cookie);
}

    /*
     *
     */
    int
vumem_rpc_setup (VumemGenDev* dev, VumemRpc* rpc, int type, int chan)
{
    NkXIrq    xirqs;
    NkXIrq    xirqs_peer;
    NkXIrq    xirq_off;
    VumemSize pdev_off;

    xirqs          = dev->vlink->server ? dev->sxirqs : dev->cxirqs;
    xirqs_peer     = dev->vlink->server ? dev->cxirqs : dev->sxirqs;
    xirq_off       = (chan == VUMEM_RPC_CHAN_REQS) ? 0 : 1;
    pdev_off       = (chan == VUMEM_RPC_CHAN_REQS) ? 0 : (dev->pdev_size / 2);

    rpc->type      = type;
    rpc->chan      = chan;
    rpc->shm       = (VumemRpcShm*) (dev->pdev_vaddr + pdev_off);
    rpc->size_max  = (dev->pdev_size / 2) - sizeof(VumemRpcShm);
    rpc->xirq      = xirqs + xirq_off;
    rpc->xirq_peer = xirqs_peer + xirq_off;
    rpc->osid_peer = dev->vlink->id_peer;

    init_waitqueue_head(&rpc->wait);
    sema_init(&rpc->lock, 1);

    return vumem_xirq_attach(dev, rpc->xirq, vumem_rpc_handler, rpc);
}

    /*
     *
     */
    void
vumem_rpc_reset (VumemRpc* rpc)
{
    if (rpc->type == VUMEM_RPC_TYPE_CLIENT) {
	rpc->shm->reqSessionId = VUMEM_SESSION_NONE;
	rpc->shm->reqId        = 0;
	rpc->shm->size         = 0;
	rpc->shm->err          = 0;
    } else {
	VLINK_ASSERT(rpc->type == VUMEM_RPC_TYPE_SERVER);
	rpc->shm->rspSessionId = VUMEM_SESSION_NONE;
	rpc->shm->rspId        = 0;
    }
    rpc->aborted = 0;
}

    /*
     *
     */
    void
vumem_rpc_session_init (VumemRpcSession* rpc_session,
			VumemRpc*        rpc,
			VlinkSession*    vls)
{
    rpc_session->id  = VUMEM_SESSION_NONE;
    rpc_session->rpc = rpc;
    rpc_session->vls = vls;
}

    /*
     *
     */
    void*
vumem_rpc_req_alloc (VumemRpcSession* rpc_session, VumemSize size, int flags)
{
    VumemRpc* rpc = rpc_session->rpc;

    VLINK_DASSERT(rpc->type == VUMEM_RPC_TYPE_CLIENT);
    VLINK_DASSERT(!rpc->aborted);

    if (size > rpc->size_max) {
	return ERR_PTR(-EMSGSIZE);
    }

    if (flags & VUMEM_RPC_FLAGS_INTR) {
	if (down_interruptible(&rpc->lock)) {
	    return ERR_PTR(-ERESTARTSYS);
	}
    } else {
	down(&rpc->lock);
    }

    if (rpc->aborted) {
	vumem_rpc_call_end(rpc_session);
	return ERR_PTR(-ESESSIONBROKEN);
    }

    VLINK_DASSERT(rpc->shm->reqId == rpc->shm->rspId);

    rpc->shm->size = size;

    return rpc->shm->data;
}

    /*
     *
     */
    static void*
vumem_rpc_msg_get (VumemRpcSession* rpc_session, VumemSize* psize)
{
    VumemRpc* rpc = rpc_session->rpc;
    VumemSize size;
    int       err;

    if (VLINK_SESSION_IS_ABORTED(rpc_session->vls)) {
	rpc->aborted = 1;
	return ERR_PTR(-ESESSIONBROKEN);
    }

    smp_mb();

    err = ACCESS_ONCE(rpc->shm->err);
    if (err) {
	if (!IS_ERR(ERR_PTR(err))) {
	    return ERR_PTR(-EINVAL);
	}
	return ERR_PTR(err);
    }

    size = ACCESS_ONCE(rpc->shm->size);
    if (size > rpc->size_max) {
	return ERR_PTR(-EMSGSIZE);
    }

    if (psize) {
	*psize = size;
    }
    return rpc->shm->data;
}

    /*
     *
     */
    void*
vumem_rpc_call (VumemRpcSession* rpc_session, VumemSize* psize)
{
    VumemRpc* rpc = rpc_session->rpc;

    VLINK_DASSERT(rpc->type == VUMEM_RPC_TYPE_CLIENT);
    VLINK_DASSERT(rpc->shm->reqId == rpc->shm->rspId);

    rpc->shm->reqSessionId = rpc_session->id;
    rpc->shm->err          = 0;
    smp_mb();
    rpc->shm->reqId        = ACCESS_ONCE(rpc->shm->rspId) + 1;

    nkops.nk_xirq_trigger(rpc->xirq_peer, rpc->osid_peer);

    wait_event(rpc->wait,
	       ((ACCESS_ONCE(rpc->shm->reqId) ==
		 ACCESS_ONCE(rpc->shm->rspId)) ||
		VLINK_SESSION_IS_ABORTED(rpc_session->vls)));

    if ((rpc_session->id == VUMEM_SESSION_NONE) &&
	(rpc->shm->reqId == rpc->shm->rspId)) {
	rpc_session->id = rpc->shm->rspSessionId;
    }

    return vumem_rpc_msg_get(rpc_session, psize);
}

    /*
     *
     */
    void
vumem_rpc_call_end (VumemRpcSession* rpc_session)
{
    VumemRpc* rpc = rpc_session->rpc;

    VLINK_DASSERT(rpc->type == VUMEM_RPC_TYPE_CLIENT);

    if (!rpc->aborted) {
	VLINK_DASSERT(rpc->shm->reqId == rpc->shm->rspId);
    }

    up(&rpc->lock);
}

    /*
     *
     */
    void*
vumem_rpc_receive (VumemRpcSession* rpc_session, VumemSize* psize, int flags)
{
    VumemRpc* rpc = rpc_session->rpc;
    void*     data;
    int       diag;

    VLINK_DASSERT(rpc->type == VUMEM_RPC_TYPE_SERVER);
    VLINK_DASSERT(!rpc->aborted);

    for (;;) {

	if (flags & VUMEM_RPC_FLAGS_INTR) {
	    if (down_interruptible(&rpc->lock)) {
		data = ERR_PTR(-ERESTARTSYS);
		break;
	    }

	    diag = wait_event_interruptible(rpc->wait,
			((ACCESS_ONCE(rpc->shm->reqId) !=
			  ACCESS_ONCE(rpc->shm->rspId)) ||
			 VLINK_SESSION_IS_ABORTED(rpc_session->vls)));
	    if (diag) {
		vumem_rpc_receive_end(rpc_session);
		data = ERR_PTR(diag);
		break;
	    }
	} else {
	    down(&rpc->lock);

	    wait_event(rpc->wait,
		       ((ACCESS_ONCE(rpc->shm->reqId) !=
			 ACCESS_ONCE(rpc->shm->rspId)) ||
			VLINK_SESSION_IS_ABORTED(rpc_session->vls)));
	}

	data = vumem_rpc_msg_get(rpc_session, psize);
	if (IS_ERR(data)) {
	    vumem_rpc_respond(rpc_session, PTR_ERR(data));
	    break;
	}

	if ((rpc->shm->reqSessionId == rpc_session->id)    ||
	    (rpc->shm->reqSessionId == VUMEM_SESSION_NONE) ||
	    (rpc_session->id        == VUMEM_SESSION_NONE)) {
	    break;
	}

	vumem_rpc_respond(rpc_session, -ESESSIONBROKEN);
    }

    return data;
}

    /*
     *
     */
    void*
vumem_rpc_resp_alloc (VumemRpcSession* rpc_session, VumemSize size)
{
    VumemRpc* rpc = rpc_session->rpc;

    VLINK_DASSERT(rpc->type == VUMEM_RPC_TYPE_SERVER);
    VLINK_DASSERT(!rpc->aborted);
    VLINK_DASSERT(rpc->shm->reqId != rpc->shm->rspId);

    if (size > rpc->size_max) {
	return ERR_PTR(-EMSGSIZE);
    }
    rpc->shm->size = size;
    return rpc->shm->data;
}

    /*
     *
     */
    void
vumem_rpc_respond (VumemRpcSession* rpc_session, int err)
{
    VumemRpc* rpc = rpc_session->rpc;

    VLINK_DASSERT(rpc->type == VUMEM_RPC_TYPE_SERVER);

    if (!rpc->aborted) {

	VLINK_DASSERT(rpc->shm->reqId != rpc->shm->rspId);

	rpc->shm->rspSessionId = rpc_session->id;
	rpc->shm->err          = err;
	smp_mb();
	rpc->shm->rspId = ACCESS_ONCE(rpc->shm->reqId);

	nkops.nk_xirq_trigger(rpc->xirq_peer, rpc->osid_peer);
    }

    vumem_rpc_receive_end(rpc_session);
}

    /*
     *
     */
    void
vumem_rpc_receive_end (VumemRpcSession* rpc_session)
{
    VumemRpc* rpc = rpc_session->rpc;

    VLINK_DASSERT(rpc->type == VUMEM_RPC_TYPE_SERVER);

    up(&rpc->lock);
}

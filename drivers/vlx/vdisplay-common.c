/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Display (VDISP).                                  *
 *             VDISP front-end/back-end kernel driver common services.       *
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
 *    Chi Dat Truong    <chidat.truong@redbend.com>                          *
 *                                                                           *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include "vdisplay.h"

    /*
     *
     */
    void
vdisp_gen_drv_cleanup (VdispGenDrv* drv)
{
    if (drv->devs) {
        kfree(drv->devs);
        drv->devs = NULL;
    }
}

    /*
     *
     */
    int
vdisp_gen_drv_init (VdispGenDrv* drv)
{
    VlinkDrv* vlink_drv = drv->vlink_drv;

    drv->devs = (VdispDev*) kzalloc((vlink_drv->nr_units * sizeof(VdispDev)),
                                    GFP_KERNEL);
    if (!drv->devs) {
        VDISP_ERROR("%s: cannot allocate device descriptors (%d bytes)\n",
                    drv->name, vlink_drv->nr_units * sizeof(VdispDev));
        return -ENOMEM;
    }

    return 0;
}

    /*
     *
     */
    void
vdisp_gen_dev_cleanup (VdispGenDev* dev)
{
    unsigned int i;

    for (i = 0; i < dev->xirq_id_nr; i++) {
        nkops.nk_xirq_detach(dev->xirq_id[i]);
        dev->xirq_id[i] = 0;
    }
    dev->xirq_id_nr = 0;
}

    /*
     *
     */
    int
vdisp_gen_dev_init (VdispGenDev* dev)
{
    VdispGenDrv* drv   = &dev->drv->gen;
    Vlink*       vlink = dev->vlink;
    NkPhAddr     plink = nkops.nk_vtop(vlink->nk_vlink);
    NkPhSize     sz;
    int          diag;

    diag = drv->prop_get(vlink, VDISP_PROP_PDEV_SIZE, &sz);
    if (diag) {
        VLINK_ERROR(vlink, "cannot get the pdev size\n");
        return diag;
    }

    dev->pdev_paddr = nkops.nk_pdev_alloc(plink,
                                          drv->resc_id_base + VDISP_RESC_PDEV,
                                          sz);
    if (!dev->pdev_paddr) {
        VLINK_ERROR(vlink, "cannot allocate %d bytes of pdev\n", sz);
        return -ENOMEM;
    }

    dev->pdev_vaddr = nkops.nk_ptov(dev->pdev_paddr);
    dev->pdev_size  = sz;

    dev->cxirqs = nkops.nk_pxirq_alloc(plink,
                                       drv->resc_id_base + VDISP_RESC_XIRQ_CLT,
                                       vlink->nk_vlink->c_id,
                                       1);
    if (!dev->cxirqs) {
        VLINK_ERROR(vlink, "cannot allocate the client xirq\n");
        return -ENOMEM;
    }

    dev->sxirqs = nkops.nk_pxirq_alloc(plink,
                                       drv->resc_id_base + VDISP_RESC_XIRQ_SRV,
                                       vlink->nk_vlink->s_id,
                                       1);
    if (!dev->sxirqs) {
        VLINK_ERROR(vlink, "cannot allocate the server xirq\n");
        return -ENOMEM;
    }

    if ((diag = vlink_ops_register(vlink, drv->vops, dev)) != 0) {
        VLINK_ERROR(vlink, "cannot register vlink callback routines\n");
        return diag;
    }

    return 0;
}

    /*
     *
     */
    static void
vdisp_xirq_fake_handler (void* cookie, NkXIrq xirq)
{
}

    /*
     *
     */
    int
vdisp_xirq_attach (VdispGenDev* dev, NkXIrq xirq, NkXIrqHandler hdl,
                   void* cookie)
{
    Vlink*       vlink = dev->vlink;
    unsigned int idx;
    NkXIrqId     fake_id;

    idx = dev->xirq_id_nr;

    VLINK_ASSERT(idx < ARRAY_SIZE(dev->xirq_id));
    VLINK_ASSERT(!dev->xirq_id[idx]);

    fake_id = nkops.nk_xirq_attach(xirq, vdisp_xirq_fake_handler, cookie);
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
vdisp_rpc_wakeup (VdispRpc* rpc)
{
    wake_up(&rpc->wait);
}

    /*
     *
     */
    static void
vdisp_rpc_handler (void* cookie, NkXIrq xirq)
{
    vdisp_rpc_wakeup((VdispRpc*) cookie);
}

    /*
     *
     */
    int
vdisp_rpc_setup (VdispGenDev* dev, VdispRpc* rpc, int type)
{
    NkXIrq    xirqs;
    NkXIrq    xirqs_peer;

    xirqs          = dev->vlink->server ? dev->sxirqs : dev->cxirqs;
    xirqs_peer     = dev->vlink->server ? dev->cxirqs : dev->sxirqs;

    rpc->type      = type;
    rpc->shm       = (VdispRpcShm*) (dev->pdev_vaddr);
    rpc->size_max  = (dev->pdev_size / 2) - sizeof(VdispRpcShm);
    rpc->xirq      = xirqs;
    rpc->xirq_peer = xirqs_peer;
    rpc->osid_peer = dev->vlink->id_peer;

    init_waitqueue_head(&rpc->wait);
    sema_init(&rpc->lock, 1);

    return vdisp_xirq_attach(dev, rpc->xirq, vdisp_rpc_handler, rpc);
}

    /*
     *
     */
    void
vdisp_rpc_reset (VdispRpc* rpc)
{
    if (rpc->type == VDISP_RPC_TYPE_CLIENT) {
        rpc->shm->reqId = 0;
        rpc->shm->size  = 0;
        rpc->shm->err   = 0;
    } else {
        VLINK_ASSERT(rpc->type == VDISP_RPC_TYPE_SERVER);
        rpc->shm->rspId = 0;
    }
    rpc->aborted = 0;
}

    /*
     *
     */
    void*
vdisp_rpc_req_alloc (VdispRpc* rpc, VdispSize size)
{
    VLINK_ASSERT(rpc->type == VDISP_RPC_TYPE_CLIENT);
    VLINK_DASSERT(!rpc->aborted);

    if (size > rpc->size_max) {
        return ERR_PTR(-EMSGSIZE);
    }

    if (down_interruptible(&rpc->lock)) {
        return ERR_PTR(-ERESTARTSYS);
    }

    if (rpc->aborted) {
        up(&rpc->lock);
        return ERR_PTR(-ESESSIONBROKEN);
    }

    VLINK_ASSERT(rpc->shm->reqId == rpc->shm->rspId);

    rpc->shm->size = size;
    return rpc->shm->data;
}

    /*
     *
     */
    static void*
vdisp_rpc_msg_get (VdispRpc* rpc, VlinkSession* vls, VdispSize* psize)
{
    VdispSize size;
    int       err;

    if (VLINK_SESSION_IS_ABORTED(vls)) {
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
vdisp_rpc_call (VdispRpc* rpc, VlinkSession* vls, VdispSize* psize)
{
    VLINK_ASSERT(rpc->type == VDISP_RPC_TYPE_CLIENT);
    VLINK_ASSERT(rpc->shm->reqId == rpc->shm->rspId);

    rpc->shm->err = 0;

    smp_mb();

    rpc->shm->reqId = ACCESS_ONCE(rpc->shm->rspId) + 1;

    nkops.nk_xirq_trigger(rpc->xirq_peer, rpc->osid_peer);

    wait_event(rpc->wait,
               ((ACCESS_ONCE(rpc->shm->reqId) ==
                 ACCESS_ONCE(rpc->shm->rspId)) ||
                VLINK_SESSION_IS_ABORTED(vls)));

    return vdisp_rpc_msg_get(rpc, vls, psize);
}

    /*
     *
     */
    void
vdisp_rpc_call_end (VdispRpc* rpc)
{
    VLINK_ASSERT(rpc->type == VDISP_RPC_TYPE_CLIENT);

    if (!rpc->aborted) {
        VLINK_ASSERT(rpc->shm->reqId == rpc->shm->rspId);
    }

    up(&rpc->lock);
}

    /*
     *
     */
    void
vdisp_rpc_respond (VdispRpc* rpc, int err)
{
    VLINK_ASSERT(rpc->type == VDISP_RPC_TYPE_SERVER);

    if (!rpc->aborted) {

        VLINK_ASSERT(rpc->shm->reqId != rpc->shm->rspId);

        rpc->shm->err = err;

        smp_mb();

        rpc->shm->rspId = ACCESS_ONCE(rpc->shm->reqId);

        nkops.nk_xirq_trigger(rpc->xirq_peer, rpc->osid_peer);
    }

    up(&rpc->lock);
}

    /*
     *
     */
    void*
vdisp_rpc_receive (VdispRpc* rpc, VlinkSession* vls, VdispSize* psize)
{
    void* data;
    int   diag;

    VLINK_ASSERT(rpc->type == VDISP_RPC_TYPE_SERVER);
    VLINK_DASSERT(!rpc->aborted);

    if (down_interruptible(&rpc->lock)) {
        return ERR_PTR(-ERESTARTSYS);
    }

    diag = wait_event_interruptible(rpc->wait,
                                    ((ACCESS_ONCE(rpc->shm->reqId) !=
                                      ACCESS_ONCE(rpc->shm->rspId)) ||
                                     VLINK_SESSION_IS_ABORTED(vls)));
    if (diag) {
        up(&rpc->lock);
        return ERR_PTR(diag);
    }

    data = vdisp_rpc_msg_get(rpc, vls, psize);
    if (IS_ERR(data)) {
        vdisp_rpc_respond(rpc, PTR_ERR(data));
    }

    return data;
}

    /*
     *
     */
    void*
vdisp_rpc_resp_alloc (VdispRpc* rpc, VdispSize size)
{
    VLINK_ASSERT(rpc->type == VDISP_RPC_TYPE_SERVER);
    VLINK_DASSERT(!rpc->aborted);
    VLINK_ASSERT(rpc->shm->reqId != rpc->shm->rspId);

    if (size > rpc->size_max) {
        return ERR_PTR(-EMSGSIZE);
    }
    rpc->shm->size = size;
    return rpc->shm->data;
}

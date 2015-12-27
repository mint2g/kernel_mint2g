/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual User MEMory Buffers (VUMEM).                      *
 *             VUMEM backend kernel driver implementation.                   *
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
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/poll.h>
#include "vumem.h"

MODULE_DESCRIPTION("VUMEM Back-End Driver");
MODULE_AUTHOR("Sebastien Laborie <sebastien.laborie@redbend.com>");
MODULE_LICENSE("GPL");

    /*
     *
     */
static struct {
    atomic_t         id;
    struct list_head free;
    spinlock_t       lock;
} buffer_sids = {
    .id   = ATOMIC_INIT(0),
    .free = LIST_HEAD_INIT(buffer_sids.free),
    .lock = __SPIN_LOCK_UNLOCKED(buffer_sids.lock),
};

    /*
     *
     */
    static noinline VumemSrvBufferId*
vumem_buffer_sid_alloc (VumemSrvSession* session)
{
    VumemSrvBufferId* sid = NULL;

    spin_lock(&buffer_sids.lock);
    if (!list_empty(&buffer_sids.free)) {
	sid = list_first_entry(&buffer_sids.free, VumemSrvBufferId, link);
	list_del(&sid->link);
    }
    spin_unlock(&buffer_sids.lock);

    if (!sid) {
	sid = kzalloc(sizeof(VumemSrvBufferId), GFP_KERNEL);
	if (sid) {
	    sid->id = atomic_inc_return(&buffer_sids.id);
	}
    }

    return sid;
}

    /*
     *
     */
    static noinline void
vumem_buffer_sid_free (VumemSrvBufferId* sid)
{
    spin_lock(&buffer_sids.lock);
    list_add(&sid->link, &buffer_sids.free);
    spin_unlock(&buffer_sids.lock);    
}

    /*
     *
     */
    static noinline void
vumem_buffer_add (VumemSrvSession* session, VumemSrvBuffer* buffer)
{
    list_add(&buffer->link, &session->buffers);
}

    /*
     *
     */
    static noinline void
vumem_buffer_del (VumemSrvSession* session, VumemSrvBuffer* buffer)
{
    list_del(&buffer->link);
}

    /*
     *
     */
    static noinline VumemSrvBuffer*
vumem_buffer_get (VumemSrvSession* session, VumemBufferId id)
{
    VumemSrvBuffer* buffer;

    list_for_each_entry(buffer, &session->buffers, link) {
	if (buffer->sid->id == id) {
	    return buffer;
	}
    }

    return NULL;
}

    /*
     *
     */
    static noinline int
vumem_buffer_init (VumemSrvSession* session, VumemBufferAllocOut* alloc_out,
		   VumemSrvBuffer** pbuffer)
{
    VumemBufferMap*        map = &alloc_out->map;
    VumemSrvBuffer*        buffer;
    VumemBufferLayout*     layout;
    struct mm_struct*      mm;
    struct vm_area_struct* vma;
    unsigned long          vastart;
    unsigned long          vaend;
    unsigned long          va;
    VumemSize              size;
    unsigned int           pages_nr;
    struct page*           page;
    unsigned long          pfn;
    unsigned int           chunks_nr;
    VumemSize              chunk_sz;
    unsigned int           i;
    int                    diag;

    if (alloc_out->bufferId != session->buffer_sid->id) {
	return -EINVAL;
    }

    vastart = (unsigned long) map->vaddr;
    size    = PAGE_ALIGN(map->size);

    if (vastart == (unsigned long) VUMEM_BUFFER_NO_VADDR) {
	buffer = kmalloc(sizeof(VumemSrvBuffer), GFP_KERNEL);
	if (!buffer) {
	    return -ENOMEM;
	}
	buffer->sid         = session->buffer_sid;
	layout              = &buffer->layout;
	layout->size        = size;
	layout->attr        = 0;
	layout->chunks_nr   = 0;
	*pbuffer            = buffer;
	session->buffer_sid = NULL;
	return 0;
    }

    vaend = vastart + size;

    if (size    >  TASK_SIZE        ||
	vastart >  TASK_SIZE - size ||
	vastart & ~PAGE_MASK) {
	VLINK_DTRACE(session->dev->gen_dev.vlink,
		     "invalid buffer virtual area: start=0x%lx end=0x%lx\n",
		     vastart, vaend);
	return -EINVAL;
    }

    mm = current->mm;

    down_read(&mm->mmap_sem);

    vma = find_vma_intersection(mm, vastart, vaend);
    if (!vma || vastart < vma->vm_start || vaend > vma->vm_end) {
	if (!vma) {
	    VLINK_DTRACE(session->dev->gen_dev.vlink,
			 "cannot get buffer vma: start=0x%lx end=0x%lx\n",
			 vastart, vaend);
	} else {
	    VLINK_DTRACE(session->dev->gen_dev.vlink,
			 "invalid buffer vma: start=0x%lx end=0x%lx "
			 "vm_start=0x%lx vm_end=0x%lx\n",
			 vastart, vaend, vma->vm_start, vma->vm_end);
	}
	diag = -EINVAL;
	goto out_unlock_mmap;
    }

    pages_nr = size / PAGE_SIZE;

    buffer = kmalloc(sizeof(VumemSrvBuffer) +
		     (pages_nr * sizeof(VumemBufferChunk)),
		     GFP_KERNEL);
    if (!buffer) {
	diag = -ENOMEM;
	goto out_unlock_mmap;
    }

    buffer->sid = session->buffer_sid;
    layout      = &buffer->layout;

    for (i = chunks_nr = chunk_sz = 0, va = vastart;
	 i < pages_nr;
	 i++, va += PAGE_SIZE, chunk_sz += PAGE_SIZE) {

	if (vma->vm_flags & (VM_IO | VM_PFNMAP)) {
	    diag = follow_pfn(vma, va, &pfn);
	} else {
	    diag = get_user_pages(current, mm, va, 1, 1, 0, &page, NULL);
	    if (diag == 1) {
		pfn = page_to_pfn(page);
		put_page(page);
		diag = 0;
	    } else if (diag == 0) {
		diag = -EINVAL;
	    }
	}
	if (diag) {
	    VLINK_DTRACE(session->dev->gen_dev.vlink,
			 "buffer pfn cannot be determined: "
			 "va=0x%lx vm_flags=0x%lx diag=%d\n",
			 va, vma->vm_flags, diag);
	    goto out_free_buffer;
	}

	if (i == 0) {
	    layout->chunks[0].pfn = pfn;
	    continue;
	}

	if (layout->chunks[chunks_nr].pfn + (chunk_sz >> PAGE_SHIFT) != pfn) {
	    layout->chunks[chunks_nr].size = chunk_sz;
	    layout->chunks[++chunks_nr].pfn = pfn;
	    chunk_sz = 0;
	}
    }
    layout->chunks[chunks_nr++].size = chunk_sz;

    layout->size      = size;
    layout->chunks_nr = chunks_nr;
    layout->attr      = 0;

    if (chunks_nr) {
	layout->attr |= VUMEM_ATTR_IS_MAPPABLE;
	layout->attr |= vumem_cache_attr_get(vma->vm_page_prot);
    }

    *pbuffer            = buffer;
    session->buffer_sid = NULL;

    diag = 0;
    goto out_unlock_mmap;

out_free_buffer:
    kfree(buffer);
out_unlock_mmap:
    up_read(&mm->mmap_sem);
    return diag;
}

    /*
     *
     */
    static noinline void
vumem_buffer_free (VumemSrvSession* session, VumemSrvBuffer* buffer)
{
    vumem_buffer_del(session, buffer);
    vumem_buffer_sid_free(buffer->sid);
    kfree(buffer);
}

    /*
     *
     */
    static noinline void
vumem_buffers_free (VumemSrvSession* session)
{
    VumemSrvBuffer* buffer;
    VumemSrvBuffer* buffer_next;

    list_for_each_entry_safe(buffer, buffer_next, &session->buffers, link) {
	vumem_buffer_free(session, buffer);
    }
}

    /*
     *
     */
     static int
vumem_respond (VumemSrvSession*  session,
	       VumemResp __user* uresp,
	       unsigned int      unused)
{
    VumemResp       kresp;
    VumemResp*      resp;
    VumemSrvBuffer* buffer = NULL;
    VumemSize       size;
    int             diag;
    int             retval;

    if (!session->rpc_cmd) {
	return -EINVAL;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
	retval = diag = -ESESSIONBROKEN;
	goto out_resp_err;
    }

    if (copy_from_user(&kresp, uresp, sizeof(VumemResp))) {
	retval = diag = -EFAULT;
	goto out_resp_err;
    }

    diag   = 0;
    retval = -kresp.retVal;
    if (retval != 0) {
	if (!IS_ERR(ERR_PTR(retval))) {
	    retval = diag = -EINVAL;
	}
	goto out_resp_err;
    }

    size = sizeof(VumemResp);

    if (session->rpc_cmd == VUMEM_CMD_BUFFER_ALLOC) {
	diag = vumem_buffer_init(session, &kresp.bufferAlloc, &buffer);
	if (diag) {
	    retval = diag;
	    goto out_resp_err;
	}
	size += buffer->layout.chunks_nr * sizeof(VumemBufferChunk);
    }

    resp = vumem_rpc_resp_alloc(&session->rpc_session, size);
    if (IS_ERR(resp)) {
	retval = diag = PTR_ERR(resp);
	goto out_resp_err;
    }

    memcpy(resp, &kresp, sizeof(VumemResp));

    if (session->rpc_cmd == VUMEM_CMD_BUFFER_ALLOC) {
	memcpy(&resp->bufferAlloc.layout, &buffer->layout,
	       sizeof(VumemBufferLayout) +
	       buffer->layout.chunks_nr * sizeof(VumemBufferChunk));
	vumem_buffer_add(session, buffer);
    }

    vumem_rpc_respond(&session->rpc_session, retval);

    goto out_session_leave;

out_resp_err:
    if (VLINK_SESSION_IS_ABORTED(session->vls)) {
	vumem_rpc_receive_end(&session->rpc_session);
    } else {
	vumem_rpc_respond(&session->rpc_session, retval);
    }

    if (buffer) {
	vumem_buffer_sid_free(buffer->sid);
	kfree(buffer);
    } else if (session->buffer_sid) {
	vumem_buffer_sid_free(session->buffer_sid);
	session->buffer_sid = NULL;
    }

out_session_leave:
    vlink_session_leave(session->vls);
    session->rpc_cmd = 0;
    return diag;
}

    /*
     *
     */
     static int
vumem_receive (VumemSrvSession* session,
	       VumemReq __user* ureq,
	       unsigned int     unused)
{
    VumemSrvBuffer* buffer;
    VumemReqHeader* hdr;
    VumemReq*       req;
    VumemDrvReq*    drv_req;
    VumemDrvResp*   drv_resp;
    VumemSize       size;
    int             diag;

    if (session->rpc_cmd) {
	return -EINVAL;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
	diag = -ESESSIONBROKEN;
	goto out_session_leave;
    }

receive_again:

    hdr = vumem_rpc_receive(&session->rpc_session,
			    &size,
			    VUMEM_RPC_FLAGS_INTR);
    if (IS_ERR(hdr)) {
	diag = PTR_ERR(hdr);
	goto out_session_leave;
    }

    switch (hdr->cmd) {

    case VUMEM_CMD_BUFFER_ALLOC:
    {
	VumemSrvBufferId* sid;

	sid = vumem_buffer_sid_alloc(session);
	if (!sid) {
	    diag = -ENOMEM;
	    goto out_resp_err;
	}
	req                       = (VumemReq*) hdr;
	req->bufferAlloc.bufferId = sid->id;
	session->buffer_sid       = sid;
    }
    break;

    case VUMEM_CMD_BUFFER_FREE:
    {
	req    = (VumemReq*) hdr;
	buffer = vumem_buffer_get(session, req->bufferFree.bufferId);
	if (!buffer) {
	    diag = -EINVAL;
	    goto out_resp_err;
	} else {
	    vumem_buffer_free(session, buffer);
	}
    }
    break;

    case VUMEM_CMD_SESSION_CREATE:
    {
	drv_resp = vumem_rpc_resp_alloc(&session->rpc_session,
					sizeof(VumemDrvResp));
	if (IS_ERR(drv_resp)) {
	    diag = PTR_ERR(drv_resp);
	} else {
	    diag = 0;
	}
	vumem_rpc_respond(&session->rpc_session, diag);
	goto receive_again;
    }

    case VUMEM_CMD_BUFFER_CACHE_CTL:
    {
	drv_req = (VumemDrvReq*) hdr;
	buffer  = vumem_buffer_get(session, drv_req->bufferCacheCtl.bufferId);
	if (buffer) {
	    diag = vumem_cache_op(drv_req->bufferCacheCtl.cacheOp,
				  &buffer->layout,
				  VUMEM_BUFFER_NO_VADDR);
	} else {
	    diag = -EINVAL;
	}
	if (!diag) {
	    drv_resp = vumem_rpc_resp_alloc(&session->rpc_session,
					    sizeof(VumemDrvResp));
	    if (IS_ERR(drv_resp)) {
		diag = PTR_ERR(drv_resp);
	    }
	}
	vumem_rpc_respond(&session->rpc_session, diag);
	goto receive_again;
    }

    }

    if (copy_to_user(ureq, hdr, size)) {
	diag = -EFAULT;
	goto out_resp_err;
    }

    diag = 0;
    session->rpc_cmd = hdr->cmd;
    goto out_session_leave;

out_resp_err:
    if (session->buffer_sid) {
	vumem_buffer_sid_free(session->buffer_sid);
	session->buffer_sid = NULL;
    }
    vumem_rpc_respond(&session->rpc_session, diag);

out_session_leave:
    vlink_session_leave(session->vls);
    return diag;
}

    /*
     *
     */
     static void
vumem_session_init (VumemSrvSession* session, VumemSrvDev* dev)
{
    memset(session, 0, sizeof(VumemSrvSession));
    session->dev = dev;
    atomic_set(&session->excl, 0);
    INIT_LIST_HEAD(&session->buffers);
}

    /*
     *
     */
     static void
vumem_srv_rpc_session_init (VumemSrvSession* session)
{
    VumemSrvDev* dev = session->dev;

    vumem_rpc_session_init(&session->rpc_session,
			   &dev->rpc_srv,
			   session->vls);

    while (++dev->session_id == VUMEM_SESSION_NONE);
    session->rpc_session.id = dev->session_id;
}

    /*
     *
     */
     static int
vumem_session_create (VumemSrvFile* vumem_file)
{
    VumemSrvDev*     dev     = vumem_file->dev;
    Vlink*           vlink   = dev->gen_dev.vlink;
    VumemSrvSession* session = &vumem_file->session;
    int              diag;

    vumem_session_init(session, dev);

    diag = vlink_session_create(vlink,
				NULL,
				NULL,
				&session->vls);

    if (!diag) {
	vlink_session_leave(session->vls);
	vumem_srv_rpc_session_init(session);
    }

    return diag;
}

    /*
     *
     */
     static int
vumem_session_destroy (VumemSrvSession* session)
{
    VumemRpcSession rpc_session;
    VumemReqHeader* hdr;
    VumemDrvResp*   resp;
    int             diag;

    if (vlink_session_enter_and_test_alive(session->vls)) {
	diag = -ESESSIONBROKEN;
	goto out_session_leave;
    }

    vumem_rpc_session_init(&rpc_session, &session->dev->rpc_clt, session->vls);

    hdr = vumem_rpc_req_alloc(&rpc_session, sizeof(VumemDrvReq), 0);
    if (IS_ERR(hdr)) {
	diag = PTR_ERR(hdr);
	goto out_session_leave;
    }

    hdr->clId.vmId  = session->dev->gen_dev.vlink->id;
    hdr->clId.appId = 0;
    hdr->cmd        = VUMEM_CMD_BUFFER_MAPPINGS_REVOKE;

    resp = vumem_rpc_call(&rpc_session, NULL);
    if (IS_ERR(resp)) {
	diag = PTR_ERR(resp);
    } else {
	diag = 0;
    }

    vumem_rpc_call_end(&rpc_session);

out_session_leave:
    if (session->rpc_cmd) {
	if (VLINK_SESSION_IS_ABORTED(session->vls)) {
	    vumem_rpc_receive_end(&session->rpc_session);
	} else {
	    vumem_rpc_respond(&session->rpc_session, -ESESSIONBROKEN);
	}
    }

    vlink_session_leave(session->vls);

    if (session->buffer_sid) {
	vumem_buffer_sid_free(session->buffer_sid);
    }

    vumem_buffers_free(session);

    vlink_session_destroy(session->vls);

    return diag;
}

    /*
     *
     */
    static int
vumem_srv_open (struct inode* inode, struct file* file)
{
    unsigned int  minor = iminor(file->f_path.dentry->d_inode);
    unsigned int  major = imajor(file->f_path.dentry->d_inode);
    VumemDev*     vumem_gendev;
    VumemSrvDev*  vumem_dev;
    VumemSrvFile* vumem_file;
    int           diag;

    vumem_gendev = vumem_dev_find(major, minor);
    if (!vumem_gendev) {
	return -ENXIO;
    }
    vumem_dev = &vumem_gendev->srv;

    if (atomic_xchg(&vumem_dev->excl, 1) != 0) {
	return -EBUSY;
    }

    vumem_file = (VumemSrvFile*) kzalloc(sizeof(VumemSrvFile), GFP_KERNEL);
    if (!vumem_file) {
	return -ENOMEM;
    }

    vumem_file->dev    = vumem_dev;
    file->private_data = vumem_file;

    diag = vumem_session_create(vumem_file);
    if (diag) {
	kfree(vumem_file);
    }

    return diag;
}

    /*
     *
     */
    static int
vumem_srv_release (struct inode* inode, struct file* file)
{
    VumemSrvFile* vumem_file = file->private_data;
    VumemSrvDev*  vumem_dev  = vumem_file->dev;
    int           diag;

    diag = vumem_session_destroy(&vumem_file->session);

    kfree(vumem_file);

    smp_mb();
    atomic_set(&vumem_dev->excl, 0);

    return diag;
}

    /*
     *
     */
static int vumem_bad_ioctl (struct VumemSrvFile* vumem_file, void* arg,
			    unsigned int sz)
{
    VLINK_ERROR(vumem_file->dev->gen_dev.vlink, "invalid ioctl command\n");
    return -EINVAL;
}

    /*
     *
     */
static VumemSrvIoctl vumem_srv_ioctl_ops[VUMEM_IOCTL_CMD_MAX] = {
    (VumemSrvIoctl)vumem_bad_ioctl,		/*  0 - SESSION_CREATE       */
    (VumemSrvIoctl)vumem_bad_ioctl,		/*  1 - BUFFER_ALLOC         */
    (VumemSrvIoctl)vumem_bad_ioctl,		/*  2 - BUFFER_REGISTER      */
    (VumemSrvIoctl)vumem_bad_ioctl,		/*  3 - BUFFER_UNREGISTER    */
    (VumemSrvIoctl)vumem_bad_ioctl,		/*  4 - BUFFER_CACHE_FLUSH   */
    (VumemSrvIoctl)vumem_bad_ioctl,		/*  5 - Unused               */
    (VumemSrvIoctl)vumem_bad_ioctl,		/*  6 - Unused               */
    (VumemSrvIoctl)vumem_bad_ioctl,		/*  7 - Unused               */
    (VumemSrvIoctl)vumem_receive,		/*  8 - RECEIVE              */
    (VumemSrvIoctl)vumem_respond,		/*  9 - RESPOND              */
    (VumemSrvIoctl)vumem_bad_ioctl,		/* 10 - Unused               */
    (VumemSrvIoctl)vumem_bad_ioctl,		/* 11 - Unused               */
    (VumemSrvIoctl)vumem_bad_ioctl,		/* 12 - Unused               */
    (VumemSrvIoctl)vumem_bad_ioctl,		/* 13 - Unused               */
    (VumemSrvIoctl)vumem_bad_ioctl,		/* 14 - Unused               */
    (VumemSrvIoctl)vumem_bad_ioctl,		/* 15 - Unused               */
};

    /*
     *
     */
    static long
vumem_srv_ioctl (struct file* file, unsigned int cmd, unsigned long arg)
{
    VumemSrvFile*    vumem_file = file->private_data;
    VumemSrvSession* session    = &vumem_file->session;
    unsigned int     nr;
    unsigned int     sz;
    int              diag;

    if (atomic_xchg(&session->excl, 1) != 0) {
	return -EBUSY;
    }

    nr = _IOC_NR(cmd) & (VUMEM_IOCTL_CMD_MAX - 1);
    sz = _IOC_SIZE(cmd);

    diag = vumem_srv_ioctl_ops[nr](session, (void*)arg, sz);

    smp_mb();
    atomic_set(&session->excl, 0);

    return diag;
}

    /*
     *
     */
    static ssize_t
vumem_srv_read (struct file* file, char __user* ubuf, size_t cnt, loff_t* ppos)
{
    return -EINVAL;
}

    /*
     *
     */
    static ssize_t
vumem_srv_write (struct file* file, const char __user* ubuf,
		 size_t count, loff_t* ppos)
{
    return -EINVAL;
}

    /*
     *
     */
    static unsigned int
vumem_srv_poll(struct file* file, poll_table* wait)
{
    return -ENOSYS;
}

    /*
     *
     */
static struct file_operations vumem_srv_fops = {
    .owner	= THIS_MODULE,
    .open	= vumem_srv_open,
    .read	= vumem_srv_read,
    .write	= vumem_srv_write,
    .unlocked_ioctl = vumem_srv_ioctl,
    .release	= vumem_srv_release,
    .poll	= vumem_srv_poll,
};

    /*
     *
     */
    static int
vumem_srv_vlink_abort (Vlink* vlink, void* cookie)
{
    VumemSrvDev* dev = (VumemSrvDev*) cookie;

    VLINK_DTRACE(vlink, "vumem_srv_vlink_abort called\n");

    vumem_rpc_wakeup(&dev->rpc_clt);
    vumem_rpc_wakeup(&dev->rpc_srv);

    return 0;
}

    /*
     *
     */
    static int
vumem_srv_vlink_reset (Vlink* vlink, void* cookie)
{
    VumemSrvDev* dev = (VumemSrvDev*) cookie;

    VLINK_DTRACE(vlink, "vumem_srv_vlink_reset called\n");

    vumem_rpc_reset(&dev->rpc_clt);
    vumem_rpc_reset(&dev->rpc_srv);


    return 0;
}

    /*
     *
     */
    static int
vumem_srv_vlink_start (Vlink* vlink, void* cookie)
{
    VumemSrvDev* dev = (VumemSrvDev*) cookie;

    VLINK_DTRACE(vlink, "vumem_srv_vlink_start called\n");

    nkops.nk_xirq_unmask(dev->gen_dev.sxirqs);
    nkops.nk_xirq_unmask(dev->gen_dev.sxirqs + 1);

    return 0;
}

    /*
     *
     */
    static int
vumem_srv_vlink_stop (Vlink* vlink, void* cookie)
{
    VumemSrvDev* dev = (VumemSrvDev*) cookie;

    VLINK_DTRACE(vlink, "vumem_srv_vlink_stop called\n");

    nkops.nk_xirq_mask(dev->gen_dev.sxirqs);
    nkops.nk_xirq_mask(dev->gen_dev.sxirqs + 1);

    return 0;
}

    /*
     *
     */
    static int
vumem_srv_vlink_cleanup (Vlink* vlink, void* cookie)
{
    VumemSrvDev* dev = (VumemSrvDev*) cookie;

    VLINK_DTRACE(vlink, "vumem_srv_vlink_cleanup called\n");

    dev->gen_dev.enabled = 0;

    vlink_sessions_cancel(vlink);

    vumem_gen_dev_cleanup(&dev->gen_dev);

    return 0;
}

    /*
     *
     */
static VlinkOpDesc vumem_srv_vlink_ops[] = {
    { VLINK_OP_RESET,   vumem_srv_vlink_reset   },
    { VLINK_OP_START,   vumem_srv_vlink_start   },
    { VLINK_OP_ABORT,   vumem_srv_vlink_abort   },
    { VLINK_OP_STOP,    vumem_srv_vlink_stop    },
    { VLINK_OP_CLEANUP, vumem_srv_vlink_cleanup },
    { 0,                NULL                    },
};

    /*
     *
     */
    int
vumem_srv_vlink_init (VumemDrv* vumem_drv, Vlink* vlink)
{
    VumemSrvDev* dev = &vumem_drv->devs[vlink->unit].srv;
    int          diag;

    VLINK_DTRACE(vlink, "vumem_srv_vlink_init called\n");

    dev->gen_dev.vumem_drv = vumem_drv;
    dev->gen_dev.vlink     = vlink;

    if ((diag = vumem_gen_dev_init(&dev->gen_dev)) != 0) {
	vumem_srv_vlink_cleanup(vlink, dev);
	return diag;
    }

    dev->session_id = VUMEM_SESSION_NONE;

    atomic_set(&dev->excl, 0);

    diag = vumem_rpc_setup(&dev->gen_dev, &dev->rpc_clt,
			   VUMEM_RPC_TYPE_CLIENT,
			   VUMEM_RPC_CHAN_ABORT);
    if (diag) {
	vumem_srv_vlink_cleanup(vlink, dev);
	return diag;
    }

    diag = vumem_rpc_setup(&dev->gen_dev, &dev->rpc_srv,
			   VUMEM_RPC_TYPE_SERVER,
			   VUMEM_RPC_CHAN_REQS);
    if (diag) {
	vumem_srv_vlink_cleanup(vlink, dev);
	return diag;
    }

    dev->gen_dev.enabled = 1;

    return 0;
}

    /*
     *
     */
    int
vumem_srv_drv_init (VlinkDrv* parent_drv, VumemDrv* vumem_drv)
{
    int diag;

    vumem_drv->parent_drv   = parent_drv;
    vumem_drv->vops         = vumem_srv_vlink_ops;
    vumem_drv->fops         = &vumem_srv_fops;
    vumem_drv->chrdev_major = 0;
    vumem_drv->class        = NULL;
    vumem_drv->devs         = NULL;

    if ((diag = vumem_gen_drv_init(vumem_drv)) != 0) {
	vumem_srv_drv_cleanup(vumem_drv);
	return diag;
    }

    return 0;
}

    /*
     *
     */
    void
vumem_srv_drv_cleanup (VumemDrv* vumem_drv)
{
    vumem_gen_drv_cleanup(vumem_drv);
}

    /*
     *
     */
    static int
vumem_be_module_init (void)
{
    return 0;
}

    /*
     *
     */
    static void
vumem_be_module_exit (void)
{

}

module_init(vumem_be_module_init);
module_exit(vumem_be_module_exit);

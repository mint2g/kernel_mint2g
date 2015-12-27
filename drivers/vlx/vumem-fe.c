/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual User MEMory Buffers (VUMEM).                      *
 *             VUMEM frontend kernel driver implementation.                  *
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
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/poll.h>
#include "vumem.h"

MODULE_DESCRIPTION("VUMEM Front-End Driver");
MODULE_AUTHOR("Sebastien Laborie <sebastien.laborie@redbend.com>");
MODULE_LICENSE("GPL");

    /*
     *
     */
    static void*
vumem_clt_rpc_req_alloc (VumemCltSession* session, VumemCmd cmd, int flags)
{
    VumemSize       size;
    VumemReqHeader* hdr;

    if (cmd > VUMEM_CMD_NR) {
	size = sizeof(VumemDrvReq);
    } else {
	size = sizeof(VumemReq);
    }

    hdr = vumem_rpc_req_alloc(&session->rpc_session, size, flags);
    if (IS_ERR(hdr)) {
	return hdr;
    }

    hdr->clId.vmId  = session->dev->gen_dev.vlink->id;
    hdr->clId.appId = session->app_id;
    hdr->cmd        = cmd;

    return hdr;
}

    /*
     *
     */
    static inline void*
vumem_clt_rpc_call (VumemCltSession* session, VumemSize* rsize)
{
    VumemRpcSession* rpc_session = &session->rpc_session;
    void*            resp;

    resp = vumem_rpc_call(rpc_session, rsize);
    if (!IS_ERR(resp)) {
	VUMEM_PROF_ATOMIC_INC(session->dev, rpc_calls);
    }

    return resp;
}

    /*
     *
     */
    static inline void
vumem_xlink_free (VumemXLink* xlink)
{
    kfree(xlink);
}

    /*
     *
     */
    static inline VumemXLink*
vumem_xlink_alloc (void)
{
    return (VumemXLink*) kmalloc(sizeof(VumemXLink), GFP_KERNEL);
}

    /*
     *
     */
    static inline void
vumem_xlist_init (VumemXListHead* list, spinlock_t* lock, unsigned int nr)
{
    INIT_LIST_HEAD(&list->head);
    list->lock = lock;
    list->nr   = nr;
}

    /*
     *
     */
    static void
vumem_xlink_add (VumemXLink*     xlink,
		 VumemXListHead* list1,
		 VumemXListHead* list2)
{
    VLINK_DASSERT(list1->nr != list2->nr);

    spin_lock(list1->lock);

    list_add (&xlink->link[list1->nr], &list1->head);
    list_add (&xlink->link[list2->nr], &list2->head);

    spin_unlock(list1->lock);
}

    /*
     *
     */
    static VumemXLink*
vumem_xlink_del_first (VumemXListHead* list)
{
    VumemXLink* xlink = NULL;

    spin_lock(list->lock);

    if (!list_empty(&list->head)) {
	xlink = list_first_entry(&list->head, VumemXLink, link[list->nr]);
	list_del(&xlink->link[0]);
	list_del(&xlink->link[1]);
    }

    spin_unlock(list->lock);

    return xlink;
}

    /*
     *
     */
    static VumemXLink*
vumem_xlink_del_by_item (VumemXListHead* list, void* item)
{
    VumemXLink* xlink = NULL;

    spin_lock(list->lock);

    list_for_each_entry(xlink, &list->head, link[list->nr]) {
	if (xlink->item[list->nr] == item) {
	    list_del(&xlink->link[0]);
	    list_del(&xlink->link[1]);
	    break;
	}
    }

    spin_unlock(list->lock);

    return xlink;
}

static inline int  vumem_session_get (VumemCltSession* session);
static inline void vumem_session_put (VumemCltSession* session);
static inline void vumem_buffer_put  (VumemCltBuffer* buffer);
static inline int  vumem_buffer_get  (VumemCltBuffer* buffer);

#define VUMEM_LINK_BUFFER_AND_SESSION(x, s, b)				\
    do {								\
	(x)->item[(s)->xlink_buffers.nr]  = b;				\
	(x)->item[(b)->xlink_sessions.nr] = s;				\
	vumem_xlink_add(x,						\
			&((s)->xlink_buffers),				\
			&((b)->xlink_sessions));			\
	vumem_buffer_get(b);						\
	vumem_session_get(s);						\
    } while (0)

#define VUMEM_UNLINK_ANY_SESSION_FROM_BUFFER(b)				\
    ({									\
	VumemXLink*      xlink;						\
	VumemCltSession* session = NULL;				\
	xlink = vumem_xlink_del_first(&((b)->xlink_sessions));		\
	if (xlink) {							\
	    session = xlink->item[(b)->xlink_sessions.nr];		\
	    vumem_xlink_free(xlink);					\
	    vumem_buffer_put(b);					\
	}								\
	(session);							\
    })

#define VUMEM_UNLINK_ANY_BUFFER_FROM_SESSION(s)				\
    ({									\
	VumemXLink*     xlink;						\
	VumemCltBuffer* buffer = NULL;					\
	xlink = vumem_xlink_del_first(&((s)->xlink_buffers));		\
	if (xlink) {							\
	    buffer = xlink->item[(s)->xlink_buffers.nr];		\
	    vumem_xlink_free(xlink);					\
	    vumem_session_put(s);					\
	}								\
	(buffer);							\
    })

#define VUMEM_UNLINK_SESSION_FROM_BUFFER(b, s)				\
    ({									\
	VumemXLink* xlink;						\
	xlink = vumem_xlink_del_by_item(&((b)->xlink_sessions), s);	\
	if (xlink) {							\
	    vumem_xlink_free(xlink);					\
	    vumem_session_put(session);					\
	    vumem_buffer_put(buffer);					\
	}								\
	(xlink != 0);							\
    })

    /*
     *
     */
     static void
vumem_session_init (VumemCltSession* session, VumemCltDev* dev)
{
    memset(session, 0, sizeof(VumemCltSession));

    session->dev    = dev;
    session->app_id = task_tgid_vnr(current);

    atomic_set(&session->refcount, 1);
    vumem_xlist_init(&session->xlink_buffers, &dev->xlink_lock, 1);
}

    /*
     *
     */
     static int
vumem_session_create (VumemCltFile* vumem_file,
		      void*         unused,
		      unsigned int  master)
{
    VumemCltDev*     dev     = vumem_file->dev;
    Vlink*           vlink   = dev->gen_dev.vlink;
    VumemCltSession* session = &vumem_file->session;
    VumemDrvReq*     req;
    VumemDrvResp*    resp;
    int              diag;

    if (down_interruptible(&vumem_file->lock)) {
	return -ERESTARTSYS;
    }

    if (vumem_file->type != VUMEM_FILE_NONE) {
	diag = -EINVAL;
	goto out_unlock;
    }

    vumem_session_init(session, dev);

    if (master) {
	spin_lock(&dev->session_lock);
	if (dev->msession) {
	    diag            = -EBUSY;
	} else {
	    dev->msession   = session;
	    session->master = 1;
	    diag            = 0;
	}
	spin_unlock(&dev->session_lock);
	if (diag) {
	    goto out_unlock;
	}
    }

    do {

	diag = vlink_session_create(vlink,
				    NULL,
				    NULL,
				    &session->vls);
	if (diag) {
	    break;
	}

	vumem_rpc_session_init(&session->rpc_session,
			       &session->dev->rpc_clt,
			       session->vls);

	req = vumem_clt_rpc_req_alloc(session,
				      VUMEM_CMD_SESSION_CREATE,
				      VUMEM_RPC_FLAGS_INTR);
	if (IS_ERR(req)) {
	    diag = PTR_ERR(req);
	} else {
	    resp = vumem_clt_rpc_call(session, NULL);
	    if (IS_ERR(resp)) {
		diag = PTR_ERR(resp);
	    }
	    vumem_rpc_call_end(&session->rpc_session);
	}

	vlink_session_leave(session->vls);

	if (diag) {
	    vlink_session_destroy(session->vls);
	}

    } while (diag == -ESESSIONBROKEN);

    if (diag) {
	if (master) {
	    spin_lock(&dev->session_lock);
	    dev->msession = NULL;
	    spin_unlock(&dev->session_lock);
	}
    } else {
	spin_lock(&dev->session_lock);
	list_add(&session->link, &dev->sessions);
	spin_unlock(&dev->session_lock);

	smp_mb();
	vumem_file->type = VUMEM_FILE_SESSION;

	VUMEM_PROF_ATOMIC_INC(dev, sessions);
    }

out_unlock:
    up(&vumem_file->lock);

    return diag;
}

    /*
     *
     */
     static noinline void
vumem_session_destroy (VumemCltSession* session)
{
    VumemCltDev* dev = session->dev;

    spin_lock(&dev->session_lock);
    list_del(&session->link);
    spin_unlock(&dev->session_lock);

    vlink_session_destroy(session->vls);

    kfree(VUMEM_SESSION_TO_FILE(session));

    VUMEM_PROF_ATOMIC_DEC(dev, sessions);
}

    /*
     *
     */
    static inline int
vumem_session_get (VumemCltSession* session)
{
    if (!atomic_inc_not_zero(&session->refcount)) {
	return 1;
    }
    return 0;
}

    /*
     *
     */
    static inline void
vumem_session_put (VumemCltSession* session)
{
    if (atomic_dec_and_test(&session->refcount)) {
	vumem_session_destroy(session);
    }
}

static int vumem_buffer_do_unregister (VumemCltBuffer* buffer,
				       VumemCltSession* session);

    /*
     *
     */
     static noinline void
vumem_session_release (VumemCltSession* session)
{
    VumemCltDev*    dev = session->dev;
    VumemCltBuffer* buffer;

    while ((buffer = VUMEM_UNLINK_ANY_BUFFER_FROM_SESSION(session)) != NULL) {
	vumem_buffer_do_unregister(buffer, session);
	vumem_buffer_put(buffer);
    }

    spin_lock(&dev->session_lock);
    if (dev->msession == session) {
	dev->msession   = NULL;
	session->master = 0;
    }
    spin_unlock(&dev->session_lock);

    session->app_id = 0;

    vumem_session_put(session);
}

    /*
     *
     */
    static int
vumem_session_from_fd (int fd, VumemCltDev* dev, VumemCltSession** psession,
		       int* fput_needed)
{
    VumemCltFile*    vumem_file;
    VumemCltSession* session;
    struct file*     filp;
    unsigned int     minor;
    unsigned int     major;
    int              diag;

    filp = fget_light(fd, fput_needed);
    if (!filp) {
	return -EBADF;
    }

    major = imajor(filp->f_path.dentry->d_inode);
    minor = iminor(filp->f_path.dentry->d_inode);

    if (dev != (VumemCltDev*) vumem_dev_find(major, minor)) {
	diag = -EINVAL;
	goto out_fput;
    }

    vumem_file = (VumemCltFile*) filp->private_data;
    if (!vumem_file) {
	diag = -EINVAL;
	goto out_fput;
    }

    if (vumem_file->type != VUMEM_FILE_SESSION) {
	diag = -EINVAL;
	goto out_fput;
    }

    smp_mb();

    session = &vumem_file->session;

    if (session->aborted) {
	diag = -ESESSIONBROKEN;
	goto out_fput;
    }

    if (vumem_session_get(session)) {
	diag = -EINVAL;
	goto out_fput;
    }

    *psession = session;

    return 0;

out_fput:
    fput_light(filp, *fput_needed);

    return diag;
}

    /*
     *
     */
     static noinline int
vumem_buffer_cache_ctl_remote (VumemCltBuffer* buffer, VumemCacheOp cache_op)
{
    VumemCltSession* session = buffer->session;
    VumemDrvReq*     req;
    VumemDrvResp*    resp;
    int              diag;

    if (session->aborted) {
	return -ESESSIONBROKEN;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
	diag = -ESESSIONBROKEN;
	goto out_session_leave;
    }

    req = vumem_clt_rpc_req_alloc(session,
				  VUMEM_CMD_BUFFER_CACHE_CTL,
				  VUMEM_RPC_FLAGS_INTR);
    if (IS_ERR(req)) {
	diag = PTR_ERR(req);
	goto out_session_leave;
    }

    req->bufferCacheCtl.bufferId = buffer->id;
    req->bufferCacheCtl.cacheOp  = cache_op;

    resp = vumem_clt_rpc_call(session, NULL);
    if (IS_ERR(resp)) {
	diag = PTR_ERR(resp);
	goto out_rpc_end;
    }

    VUMEM_PROF_ATOMIC_INC(session->dev, buffer_cache_ctl_rem);

    diag = 0;

out_rpc_end:
    vumem_rpc_call_end(&session->rpc_session);

out_session_leave:
    vlink_session_leave(session->vls);
    return diag;
}

static unsigned long vumem_buffer_map_vaddr (VumemCltBuffer* buffer,
					     struct mm_struct* mm);

    /*
     *
     */
     static int
vumem_buffer_cache_ctl (VumemCltFile* vumem_file,
			void*         vaddr,
			VumemCacheOp  cache_op)
{
    VumemCltSession*   session;
    VumemCltBuffer*    buffer;
    VumemBufferLayout* layout;
    struct mm_struct*  mm = current->mm;
    int                diag;

    if (vumem_file->type != VUMEM_FILE_BUFFER) {
	return -EINVAL;
    }

    smp_mb();

    buffer  = &vumem_file->buffer;
    session = buffer->session;
    layout  = buffer->layout;

    if (((unsigned long) vaddr == (unsigned long) -1UL) ||
	!layout || !(layout->attr & VUMEM_ATTR_IS_MAPPABLE)) {
	return -EINVAL;
    }

    down_read(&mm->mmap_sem);

    if (vumem_buffer_map_vaddr(buffer, mm) != (unsigned long) vaddr) {
	diag = -EINVAL;
	goto err_unlock;
    }

    if (session->aborted) {
	diag = -ESESSIONBROKEN;
	goto err_unlock;
    }

    diag = vumem_cache_op(cache_op, layout, vaddr);

    up_read(&mm->mmap_sem);

    if (diag == -ENOSYS) {
	diag = vumem_buffer_cache_ctl_remote(buffer, cache_op);
    }

    if (!diag) {
	VUMEM_PROF_ATOMIC_INC(session->dev, buffer_cache_ctl);
    }

    return diag;

err_unlock:
    up_read(&mm->mmap_sem);
    return diag;
}

    /*
     *
     */
     static int
vumem_buffer_ctl_remote (VumemCltFile* vumem_file,
			 void __user*  uctl,
			 VumemCacheOp  size)
{
    VumemCltBuffer*  buffer;
    VumemCltSession* session;
    VumemCltDev*     dev;
    VumemReq*        req;
    VumemResp*       resp;
    int              diag;

    if (vumem_file->type != VUMEM_FILE_BUFFER) {
	return -EINVAL;
    }

    smp_mb();

    buffer  = &vumem_file->buffer;
    session = buffer->session;
    dev     = session->dev;

    if (session->aborted) {
	return -ESESSIONBROKEN;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
	diag = -ESESSIONBROKEN;
	goto out_session_leave;
    }

    if (size > VUMEM_OPAQUE_REQUEST_SIZE_MAX) {
	diag = -EINVAL;
	goto out_session_leave;
    }

    req = vumem_clt_rpc_req_alloc(session,
				  VUMEM_CMD_BUFFER_CTL, 
				  VUMEM_RPC_FLAGS_INTR);
    if (IS_ERR(req)) {
	diag = PTR_ERR(req);
	goto out_session_leave;
    }

    req->bufferCtl.bufferId = buffer->id;
    req->bufferCtl.size     = size;
    
    if (copy_from_user(req->bufferCtl.ctl, uctl, size)) {
	diag = -EFAULT;
	goto out_rpc_end;
    }

    resp = vumem_clt_rpc_call(session, NULL);
    if (IS_ERR(resp)) {
	diag = PTR_ERR(resp);
	goto out_rpc_end;
    }

    if (copy_to_user(uctl, resp->bufferCtl.ctl, size)) {
	diag = -EFAULT;
    } else {
	diag = 0;
    }

    VUMEM_PROF_ATOMIC_INC(dev, buffer_ctl_remote);
 
out_rpc_end:
    vumem_rpc_call_end(&session->rpc_session);

out_session_leave:
    vlink_session_leave(session->vls);

    return diag;
}

    /*
     *
     */
     static int
vumem_buffer_register (VumemCltFile* vumem_file,
		       unsigned int  session_fd,
		       unsigned int  unused)
{
    VumemCltBuffer*  buffer;
    VumemCltSession* session;
    int              fput_needed;
    VumemXLink*      xlink;
    VumemCltDev*     dev;
    VumemReq*        req;
    VumemResp*       resp;
    int              diag;

    if (vumem_file->type != VUMEM_FILE_BUFFER) {
	return -EINVAL;
    }

    smp_mb();

    buffer = &vumem_file->buffer;
    dev    = buffer->session->dev;

    if (buffer->session->aborted) {
	return -ESESSIONBROKEN;
    }

    diag = vumem_session_from_fd(session_fd, dev, &session, &fput_needed);
    if (diag) {
	return diag;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
	diag = -ESESSIONBROKEN;
	goto out_session_leave;
    }

    xlink = vumem_xlink_alloc();
    if (!xlink) {
	diag = -ENOMEM;
	goto out_session_leave;
    }

    req = vumem_clt_rpc_req_alloc(session,
				  VUMEM_CMD_BUFFER_REGISTER,
				  VUMEM_RPC_FLAGS_INTR);
    if (IS_ERR(req)) {
	diag = PTR_ERR(req);
	goto out_xlink_free;
    }

    req->bufferRegister.bufferId = buffer->id;

    resp = vumem_clt_rpc_call(session, NULL);
    if (IS_ERR(resp)) {
	diag = PTR_ERR(resp);
	goto out_rpc_end;
    }

    VUMEM_LINK_BUFFER_AND_SESSION(xlink, session, buffer);

    VUMEM_PROF_ATOMIC_INC(dev, buffer_register);

    diag = 0;

out_rpc_end:
    vumem_rpc_call_end(&session->rpc_session);

out_xlink_free:
    if (diag) {
	vumem_xlink_free(xlink);
    }

out_session_leave:
    vlink_session_leave(session->vls);

    vumem_session_put(session);

    fput_light(VUMEM_SESSION_TO_FILE(session)->filp, fput_needed);

    return diag;
}

    /*
     *
     */
    static int
vumem_buffer_do_unregister (VumemCltBuffer* buffer, VumemCltSession* session)
{
    VumemReq*  req;
    VumemResp* resp;
    int        diag;

    if (session->aborted) {
	return -ESESSIONBROKEN;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
	diag = -ESESSIONBROKEN;
	goto out_session_leave;
    }

    req = vumem_clt_rpc_req_alloc(session, VUMEM_CMD_BUFFER_UNREGISTER, 0);
    if (IS_ERR(req)) {
	diag = PTR_ERR(req);
	goto out_session_leave;
    }

    req->bufferUnregister.bufferId = buffer->id;

    resp = vumem_clt_rpc_call(session, NULL);
    if (IS_ERR(resp)) {
	diag = PTR_ERR(resp);
	goto out_rpc_end;
    }

    VUMEM_PROF_ATOMIC_INC(session->dev, buffer_unregister);

    diag = 0;

out_rpc_end:
    vumem_rpc_call_end(&session->rpc_session);

out_session_leave:
    vlink_session_leave(session->vls);

    return diag;
}
			 
    /*
     *
     */
     static int
vumem_buffer_unregister (VumemCltFile* vumem_file,
			 unsigned int  session_fd,
			 unsigned int  unused)
{
    VumemCltBuffer*  buffer;
    VumemCltSession* session;
    int              fput_needed;
    int              diag;

    if (vumem_file->type != VUMEM_FILE_BUFFER) {
	return -EINVAL;
    }

    smp_mb();

    buffer = &vumem_file->buffer;

    if (buffer->session->aborted) {
	return -ESESSIONBROKEN;
    }

    diag = vumem_session_from_fd(session_fd, buffer->session->dev, &session,
				 &fput_needed);
    if (diag) {
	return diag;
    }

    if (VUMEM_UNLINK_SESSION_FROM_BUFFER(buffer, session)) {
	diag = vumem_buffer_do_unregister(buffer, session);
    } else {
	diag = -EINVAL;
    }

    vumem_session_put(session);

    fput_light(VUMEM_SESSION_TO_FILE(session)->filp, fput_needed);

    return diag;
}

    /*
     *
     */
     static void
vumem_buffer_init (VumemCltBuffer* buffer, VumemCltSession* session)
{
    memset(buffer, 0, sizeof(VumemCltBuffer));
    buffer->session = session;
    atomic_set(&buffer->refcount, 1);
    vumem_xlist_init(&buffer->xlink_sessions, &session->dev->xlink_lock, 0);
    INIT_LIST_HEAD(&buffer->buffer_maps);
}

    /*
     *
     */
     static int
vumem_buffer_alloc (VumemCltFile*               vumem_file,
		    VumemBufferAllocCmd __user* ucmd,
		    unsigned int                unused)
{
    VumemCltDev*        dev;
    VumemCltSession*    session;
    VumemCltBuffer*     buffer = &vumem_file->buffer;
    int                 fput_needed;
    VumemXLink*         xlink;
    struct page*        garbage_page;
    VumemBufferLayout*  layout;
    VumemReq*           req;
    VumemResp*          resp;
    VumemSize           resp_size;
    VumemSize           layout_size;
    VumemBufferAllocCmd cmd;
    int                 diag;

    if (down_interruptible(&vumem_file->lock)) {
	return -ERESTARTSYS;
    }

    if (vumem_file->type != VUMEM_FILE_NONE) {
	diag = -EINVAL;
	goto out_unlock;
    }

    if (copy_from_user(&cmd, ucmd, sizeof(VumemBufferAllocCmd))) {
	diag = -EFAULT;
	goto out_unlock;
    }

    dev = vumem_file->dev;

    diag = vumem_session_from_fd(cmd.session_fd, dev, &session, &fput_needed);
    if (diag) {
	goto out_unlock;
    }

    if (dev->msession && !session->master) {
	diag = -EPERM;
	goto err_session_put;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
	diag = -ESESSIONBROKEN;
	goto err_session_leave;
    }

    if (cmd.size > VUMEM_OPAQUE_REQUEST_SIZE_MAX) {
	diag = -EINVAL;
	goto err_session_leave;
    }

    xlink = vumem_xlink_alloc();
    if (!xlink) {
	diag = -ENOMEM;
	goto err_session_leave;
    }

    garbage_page = alloc_page(GFP_KERNEL);
    if (!garbage_page) {
	diag = -ENOMEM;
	goto err_xlink_free;
    }

    vumem_buffer_init(buffer, session);
    buffer->garbage_pfn = page_to_pfn(garbage_page);

    req = vumem_clt_rpc_req_alloc(session,
				  VUMEM_CMD_BUFFER_ALLOC,
				  VUMEM_RPC_FLAGS_INTR);
    if (IS_ERR(req)) {
	diag = PTR_ERR(req);
	goto err_page_free;
    }

    req->bufferAlloc.size = cmd.size;

    if (copy_from_user(req->bufferAlloc.alloc, cmd.alloc, cmd.size)) {
	diag = -EFAULT;
	goto err_rpc_end;
    }

    resp = vumem_clt_rpc_call(session, &resp_size);
    if (IS_ERR(resp)) {
	diag = PTR_ERR(resp);
	goto err_rpc_end;
    }

    buffer->id  = resp->bufferAlloc.bufferId;

    layout_size = (sizeof(VumemBufferLayout) +
		   resp->bufferAlloc.layout.chunks_nr *
		   sizeof(VumemBufferChunk));

    diag = 0;
    if (layout_size < resp_size) {
	layout = (void*) kzalloc(layout_size, GFP_KERNEL);
	if (layout) {
	    buffer->layout = layout;
	    memcpy(layout, &resp->bufferAlloc.layout, layout_size);
	} else {
	    diag = -ENOMEM;
	}
    } else {
	diag = -EINVAL;
    }

    VUMEM_LINK_BUFFER_AND_SESSION(xlink, session, buffer);

    spin_lock(&dev->buffer_lock);
    list_add(&buffer->link, &dev->buffers);
    spin_unlock(&dev->buffer_lock);

    smp_mb();
    vumem_file->type = VUMEM_FILE_BUFFER;

    VUMEM_PROF_ATOMIC_INC(dev, buffer_alloc);
    VUMEM_PROF_ATOMIC_INC(dev, buffers);

    if (diag) {
	goto out_rpc_end;
    }

    VUMEM_PROF_ATOMIC_ADD(dev, buffer_allocated, layout->size);
    VUMEM_PROF_ATOMIC_MAX(dev, buffer_allocated_max,
			  VUMEM_PROF_ATOMIC_READ(dev, buffer_allocated));
    VUMEM_PROF_ATOMIC_MAX(dev, buffer_size_max, layout->size);

    cmd.buffer_id   = buffer->id;
    cmd.buffer_size = layout->size;
    cmd.buffer_attr = layout->attr;

    if (copy_to_user(ucmd, &cmd, sizeof(VumemBufferAllocCmd))) {
	diag = -EFAULT;
	goto out_rpc_end;
    }

    if (copy_to_user(cmd.alloc, resp->bufferAlloc.alloc, cmd.size)) {
	diag = -EFAULT;
    }

out_rpc_end:
    vumem_rpc_call_end(&session->rpc_session);

    vlink_session_leave(session->vls);

out_fput:
    fput_light(VUMEM_SESSION_TO_FILE(session)->filp, fput_needed);

out_unlock:
    up(&vumem_file->lock);

    return diag;

err_rpc_end:
    vumem_rpc_call_end(&session->rpc_session);

err_page_free:
    __free_page(garbage_page);

err_xlink_free:
    vumem_xlink_free(xlink);

err_session_leave:
    vlink_session_leave(session->vls);

err_session_put:
    vumem_session_put(session);

    goto out_fput;
}

    /*
     *
     */
     static int
vumem_buffer_destroy (VumemCltBuffer* buffer)
{
    VumemCltSession* session = buffer->session;
    VumemCltDev*     dev     = session->dev;
    VumemReq*        req;
    VumemResp*       resp;
    int              diag;

    spin_lock(&dev->buffer_lock);
    list_del(&buffer->link);
    spin_unlock(&dev->buffer_lock);

    if (session->aborted) {
	diag = -ESESSIONBROKEN;
	goto out_session_put;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
	diag = -ESESSIONBROKEN;
	goto out_session_leave;
    }

    req = vumem_clt_rpc_req_alloc(session, VUMEM_CMD_BUFFER_FREE, 0);
    if (IS_ERR(req)) {
	diag = PTR_ERR(req);
	goto out_session_leave;
    }

    req->bufferFree.bufferId = buffer->id;

    resp = vumem_clt_rpc_call(session, NULL);
    if (IS_ERR(resp)) {
	diag = PTR_ERR(resp);
	goto out_rpc_end;
    }

    VUMEM_PROF_ATOMIC_INC(session->dev, buffer_free);

    diag = 0;

out_rpc_end:
    vumem_rpc_call_end(&session->rpc_session);

out_session_leave:
    vlink_session_leave(session->vls);

out_session_put:
    vumem_session_put(session);

    if (buffer->layout) {
	VUMEM_PROF_ATOMIC_SUB(dev, buffer_allocated, buffer->layout->size);
	kfree(buffer->layout);
    }

    __free_page(pfn_to_page(buffer->garbage_pfn));

    kfree(VUMEM_BUFFER_TO_FILE(buffer));

    VUMEM_PROF_ATOMIC_DEC(dev, buffers);

    return diag;
}

    /*
     *
     */
    static inline int
vumem_buffer_get (VumemCltBuffer* buffer)
{
    if (!atomic_inc_not_zero(&buffer->refcount)) {
	return 1;
    }
    return 0;
}

    /*
     *
     */
    static inline void
vumem_buffer_put (VumemCltBuffer* buffer)
{
    if (atomic_dec_and_test(&buffer->refcount)) {
	vumem_buffer_destroy(buffer);
    }
}

    /*
     *
     */
    static void
vumem_buffer_release (VumemCltBuffer* buffer)
{
    VumemCltSession* session;

    while ((session = VUMEM_UNLINK_ANY_SESSION_FROM_BUFFER(buffer)) != NULL) {
	vumem_buffer_do_unregister(buffer, session);
	vumem_session_put(session);
    }

    vumem_buffer_put(buffer);
}

    /*
     *
     */
    static VumemCltBufferMap*
vumem_buffer_map_alloc (void)
{
    return kmalloc(sizeof(VumemCltBufferMap), GFP_KERNEL);
}

    /*
     *
     */
    static void
vumem_buffer_map_free (VumemCltBufferMap* buffer_map)
{
    kfree(buffer_map);
}

    /*
     *
     */
    static void
vumem_buffer_map_register (VumemCltBufferMap*     buffer_map,
			   VumemCltBuffer*        buffer,
			   struct vm_area_struct* vma)
{
    VumemCltDev* dev = buffer->session->dev;

    buffer_map->buffer   = buffer;
    buffer_map->vma      = vma;
    vma->vm_private_data = buffer_map;

    spin_lock(&dev->buffer_map_lock);
    list_add(&buffer_map->link_all, &dev->buffer_maps);
    list_add(&buffer_map->link_buffer, &buffer->buffer_maps);
    spin_unlock(&dev->buffer_map_lock);

    VUMEM_PROF_ATOMIC_INC(dev, buffer_mappings);
}

    /*
     *
     */
    static void
vumem_buffer_map_destroy (VumemCltBufferMap* buffer_map)
{
    VumemCltDev* dev = buffer_map->buffer->session->dev;

    spin_lock(&dev->buffer_map_lock);
    list_del(&buffer_map->link_all);
    list_del(&buffer_map->link_buffer);
    spin_unlock(&dev->buffer_map_lock);

    if (buffer_map->vma) {
	buffer_map->vma->vm_private_data = NULL;
	buffer_map->vma                  = NULL;
    }

    if (!buffer_map->revoked) {
	vumem_buffer_map_free(buffer_map);
    }

    VUMEM_PROF_ATOMIC_DEC(dev, buffer_mappings);
}

    /*
     *
     */
    static unsigned long
vumem_buffer_map_vaddr (VumemCltBuffer* buffer, struct mm_struct* mm)
{
    VumemCltDev*           dev = buffer->session->dev;
    VumemCltBufferMap*     buffer_map;
    struct vm_area_struct* vma;
    unsigned long          vaddr = (unsigned long) -1UL;

    spin_lock(&dev->buffer_map_lock);

    list_for_each_entry(buffer_map, &buffer->buffer_maps, link_buffer) {
	vma = buffer_map->vma;
	if (vma && vma->vm_mm == mm) {
	    vaddr = vma->vm_start;
	    break;
	}
    }

    spin_unlock(&dev->buffer_map_lock);

    return vaddr;
}

    /*
     *
     */
    static inline int
vumem_buffer_map_exist (VumemCltBuffer* buffer, struct mm_struct* mm)
{
    return (vumem_buffer_map_vaddr(buffer, mm) != (unsigned long) -1UL);
}

    /*
     *
     */
    static void
vumem_vma_unap (struct vm_area_struct* vma)
{
    zap_page_range(vma, vma->vm_start, vma->vm_end - vma->vm_start, NULL);
}

    /*
     *
     */
    static void
vumem_clt_vm_open (struct vm_area_struct* vma)
{
    VumemCltBufferMap* buffer_map = vma->vm_private_data;
    VumemCltBuffer*    buffer;

    if (!buffer_map) {
	return;
    }

    buffer = buffer_map->buffer;

    if (buffer_map->vma != vma) {
	vumem_vma_unap(vma);
	vma->vm_private_data = NULL;
    }

    vumem_vma_unap(buffer_map->vma);
    vumem_buffer_map_destroy(buffer_map);
}

    /*
     *
     */
    static void
vumem_clt_vm_close (struct vm_area_struct* vma)
{
    VumemCltBufferMap* buffer_map = vma->vm_private_data;

    if (!buffer_map) {
	return;
    }

    vumem_buffer_map_destroy(buffer_map);

//    vumem_cache_clean_range((void*) area->vm_start, (void*) area->vm_end);
}

    /*
     *
     */
    static int
vumem_clt_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    VumemCltFile*   vumem_file = vma->vm_file->private_data;
    VumemCltBuffer* buffer     = &vumem_file->buffer;

    if (buffer->session->aborted) {
	(void) vm_insert_pfn(vma,
			     (unsigned long) vmf->virtual_address,
			     buffer->garbage_pfn);
	return VM_FAULT_NOPAGE;
    }

    return VM_FAULT_SIGBUS;
}

    /*
     *
     */
static struct vm_operations_struct vumem_clt_vm_ops = {
    .open  = vumem_clt_vm_open,
    .close = vumem_clt_vm_close,
    .fault = vumem_clt_vm_fault,
};

    /*
     *
     */
    static int
vumem_clt_mmap (struct file *file, struct vm_area_struct *vma)
{
    VumemCltFile*      vumem_file = file->private_data;
    VumemCltDev*       dev;
    VumemCltBuffer*    buffer;
    VumemCltBufferMap* buffer_map;
    VumemCltSession*   session;
    VumemBufferLayout* layout;
    VumemSize          size;
    int                cache_policy;
    int                diag;
    unsigned int       i;

    if (vumem_file->type != VUMEM_FILE_BUFFER) {
	return -EINVAL;
    }

    smp_mb();

    buffer  = &vumem_file->buffer;
    session = buffer->session;
    layout  = buffer->layout;
    dev     = session->dev;

    if (!layout || !layout->chunks_nr) {
	return -EINVAL;
    }

    if (((vma->vm_end - vma->vm_start) != layout->size) ||
	vma->vm_pgoff                                   ||
	!(vma->vm_flags & VM_READ)                      ||
	!(vma->vm_flags & VM_WRITE)                     ||
	!(vma->vm_flags & VM_SHARED)) {
	VLINK_DTRACE(session->dev->gen_dev.vlink,
		     "invalid mmap vma: vm_start=0x%lx vm_end=0x%lx "
		     "size=0x%x vm_flags=0x%x vm_pgoff=0x%lx\n",
		     vma->vm_start, vma->vm_end,
		     layout->size, (int)vma->vm_flags, vma->vm_pgoff);
	return -EINVAL;
    }

    if (vumem_buffer_map_exist(buffer, vma->vm_mm)) {
	VLINK_DTRACE(session->dev->gen_dev.vlink,
		     "buffer '0x%x' is already mapped in mm 0x%p\n",
		     buffer->id, vma->vm_mm);
	return -EBUSY;
    }

    buffer_map = vumem_buffer_map_alloc();
    if (!buffer_map) {
	return -ENOMEM;
    }

    vma->vm_ops    = &vumem_clt_vm_ops;
    vma->vm_flags |= (VM_IO | VM_PFNMAP | VM_DONTCOPY | VM_DONTEXPAND);

    cache_policy = VUMEM_ATTR_CACHE_POLICY_GET(layout->attr);
    vma->vm_page_prot = vumem_cache_prot_get(vma->vm_page_prot, cache_policy);

    vumem_buffer_map_register(buffer_map, buffer, vma);

    smp_mb();

    if (session->aborted || VLINK_SESSION_IS_ABORTED(session->vls)) {
	diag = -ESESSIONBROKEN;
	goto out;
    }

    for (size = diag = i = 0; i < layout->chunks_nr && !diag; i++) {

	diag = remap_pfn_range(vma,
			       vma->vm_start + size,
			       layout->chunks[i].pfn,
			       layout->chunks[i].size,
			       vma->vm_page_prot);
	if (diag) {
	    vumem_vma_unap(vma);
	    VLINK_DTRACE(session->dev->gen_dev.vlink,
			 "cannot map buffer chunk: va=0x%lx pfn=0x%lx "
			 "size=0x%x\n", vma->vm_start + size,
			 layout->chunks[i].pfn, layout->chunks[i].size);
	}

	size += layout->chunks[i].size;
    }

out:
    if (diag) {
	vumem_buffer_map_destroy(buffer_map);
    } else {
	VUMEM_PROF_ATOMIC_INC(session->dev, buffer_map);
    }

    return diag;
}

    /*
     *
     */
    static noinline int
vumem_buffer_mmap_sem_lock (struct mm_struct* buffer_mm)
{
    struct task_struct* p;
    struct mm_struct*   mm = NULL;

    read_lock(&tasklist_lock);
    
    for_each_process(p) {

	mm = get_task_mm(p);
	if (mm) {
	    if (mm == buffer_mm) {
		break;
	    }
	    mmput(mm);
	}
    }

    read_unlock(&tasklist_lock);

    if (mm == buffer_mm) {
	down_write(&mm->mmap_sem);
	return 1;
    }

    return 0;
}

    /*
     *
     */
    static noinline VumemCltBufferMap*
vumem_buffer_map_get (VumemCltDev* dev)
{
    VumemCltBufferMap* buffer_map;
    struct mm_struct*  buffer_mm;

    /*
     * Fast path (no 'mmap_sem' lock contention).
     */
    spin_lock(&dev->buffer_map_lock);
    
    list_for_each_entry(buffer_map, &dev->buffer_maps, link_all) {
	if (down_write_trylock(&buffer_map->vma->vm_mm->mmap_sem)) {
	    spin_unlock(&dev->buffer_map_lock);
	    return buffer_map;
	}
    }

    spin_unlock(&dev->buffer_map_lock);

    /*
     * Slow path ('mmap_sem' lock contention).
     */
    for (buffer_map = NULL; buffer_map == NULL;) {

	spin_lock(&dev->buffer_map_lock);

	if (list_empty(&dev->buffer_maps)) {
	    spin_unlock(&dev->buffer_map_lock);
	    break;
	}

	buffer_map = list_first_entry(&dev->buffer_maps,
				      VumemCltBufferMap,
				      link_all);
	buffer_map->revoked = 1;
	buffer_mm           = buffer_map->vma->vm_mm;

	spin_unlock(&dev->buffer_map_lock);

	if (vumem_buffer_mmap_sem_lock(buffer_mm)) {
	    if (buffer_map->vma) {
		/*
		 * If buffer_map->vma is not null, then the buffer mapping
		 * still exists.
		 * We hold the mmap_sem lock here, thus we are guaranteed
		 * that the buffer mapping as well as the corresponding mm
		 * will survive until we release the mmap_sem lock.
		 */
		buffer_map->revoked = 0;
	    } else {
		/*
		 * The buffer mapping has gone away before we were able to
		 * lock the mmap_sem of the corresponding mm.
		 */
		up_write(&buffer_mm->mmap_sem);
		vumem_buffer_map_free(buffer_map);
		buffer_map = NULL;
	    }
	    mmput(buffer_mm);
	} else {
	    /*
	     * The buffer mapping as well as the corresponding mm have gone
	     * away and thus we were not able to 'get' that mm.
	     */
	    VLINK_ASSERT(!buffer_map->vma);
	    vumem_buffer_map_free(buffer_map);
	    buffer_map = NULL;
	}
    }

    return buffer_map;
}

    /*
     *
     */
    static noinline void
vumem_buffers_abort (VumemCltDev* dev)
{
    VumemCltSession*   session;
    VumemCltBufferMap* buffer_map;
    VumemCltBuffer*    buffer;
    struct mm_struct*  buffer_mm;

    spin_lock(&dev->session_lock);
    list_for_each_entry(session, &dev->sessions, link) {
	session->aborted = 1;
    }
    spin_unlock(&dev->session_lock);

    smp_mb();

    while ((buffer_map = vumem_buffer_map_get(dev)) != NULL) {
	buffer    = buffer_map->buffer;
	buffer_mm = buffer_map->vma->vm_mm;

	if (!buffer->garbage_clean) {
	    memset(__va(buffer->garbage_pfn << PAGE_SHIFT), 0, PAGE_SIZE);
	    buffer->garbage_clean = 1;
	}

	vumem_vma_unap(buffer_map->vma);
	vumem_buffer_map_destroy(buffer_map);

	up_write(&buffer_mm->mmap_sem);
    }
}

    /*
     *
     */
    static int
vumem_thread (void* arg)
{
    VumemCltDev*    dev   = (VumemCltDev*) arg;
    Vlink*          vlink = dev->gen_dev.vlink;
    VlinkSession*   vls;
    VumemRpcSession rpc_session;
    VumemReqHeader* hdr;
    VumemDrvReq*    drv_req;
    VumemDrvResp*   drv_resp;
    VumemSize       size;
    int             diag;

    do {

	diag = vlink_session_create(vlink, NULL, NULL, &vls);
	if (diag) {
	    break;
	}

	vumem_rpc_session_init(&rpc_session, &dev->rpc_srv, vls);

	for (diag = 0; !diag; ) {

	    hdr = vumem_rpc_receive(&rpc_session, &size, 0);
	    if (IS_ERR(hdr)) {
		diag = PTR_ERR(hdr);
		break;
	    }

	    switch (hdr->cmd) {

	    case VUMEM_CMD_BUFFER_MAPPINGS_REVOKE:
	    {
		drv_req = (VumemDrvReq*) hdr;

		vumem_buffers_abort(dev);

		drv_resp = vumem_rpc_resp_alloc(&rpc_session,
						sizeof(VumemDrvResp));
		if (IS_ERR(drv_resp)) {
		    diag = PTR_ERR(drv_resp);
		}
		vumem_rpc_respond(&rpc_session, diag);
	    }
	    break;

	    default:
		diag = -EINVAL;
	    }

	}

	vlink_session_leave(vls);
	vlink_session_destroy(vls);

	if (diag != -ESESSIONBROKEN) {
	    set_current_state(TASK_UNINTERRUPTIBLE);
	    schedule_timeout(HZ/10);
	}

    } while (1);

    return diag;
}

#ifdef CONFIG_VUMEM_PROFILE

#define __PROF_IDX(v)	(offsetof(VumemCltStats, v) / sizeof(atomic_t))

static struct {
    nku32_f id;
    nku32_f idx;
} vumem_prof_map[] = {
    { VUMEM_PROF_BUFFER_ALLOC,		    __PROF_IDX(buffer_alloc)	     },
    { VUMEM_PROF_BUFFER_FREE,		    __PROF_IDX(buffer_free)	     },
    { VUMEM_PROF_BUFFER_MAP,		    __PROF_IDX(buffer_map)	     },
    { VUMEM_PROF_BUFFER_REGISTER,	    __PROF_IDX(buffer_register)	     },
    { VUMEM_PROF_BUFFER_UNREGISTER,	    __PROF_IDX(buffer_unregister)    },
    { VUMEM_PROF_BUFFER_CACHE_CTL,	    __PROF_IDX(buffer_cache_ctl)     },
    { VUMEM_PROF_BUFFER_CACHE_CTL_REMOTE,   __PROF_IDX(buffer_cache_ctl_rem) },
    { VUMEM_PROF_BUFFER_CTL_REMOTE,	    __PROF_IDX(buffer_ctl_remote)    },
    { VUMEM_PROF_RPC_CALLS,		    __PROF_IDX(rpc_calls)	     },
    { VUMEM_PROF_SESSIONS,		    __PROF_IDX(sessions)	     },
    { VUMEM_PROF_BUFFERS,		    __PROF_IDX(buffers)		     },
    { VUMEM_PROF_BUFFER_MAPPINGS,	    __PROF_IDX(buffer_mappings)	     },
    { VUMEM_PROF_BUFFER_SINGLE_SIZE_MAX,    __PROF_IDX(buffer_size_max)	     },
    { VUMEM_PROF_BUFFER_ALLOCATED_SIZE,	    __PROF_IDX(buffer_allocated)     },
    { VUMEM_PROF_BUFFER_ALLOCATED_SIZE_MAX, __PROF_IDX(buffer_allocated_max) },
    { -1,				    -1				     },
};

    /*
     *
     */
     static int
vumem_prof_op (VumemCltFile*       vumem_file,
	       VumemProfOp __user* uop,
	       unsigned int        sz)
{
    VumemCltDev*      dev = vumem_file->dev;
    VumemProfOp       op;
    VumemStat __user* ustats;
    unsigned int      count;

    if (copy_from_user(&op, uop, sizeof(VumemProfOp))) {
	return -EFAULT;
    }

    count  = op.count;
    ustats = op.stats;

    switch (op.cmd) {

    case VUMEM_PROF_GEN_STAT_GET:
    {
	nku32_f      id;
	nku32_f      val;
	unsigned int i;

	while (count) {

	    if (get_user(id, &ustats->id)) {
		return -EFAULT;
	    }

	    for (i = 0;
		 (vumem_prof_map[i].id != -1) && (vumem_prof_map[i].id != id);
		 i++);

	    if (vumem_prof_map[i].id == -1) {
		return -EINVAL;
	    }

	    val = atomic_read(&dev->stats.counts[vumem_prof_map[i].idx]);

	    if (put_user(val, &ustats->val)) {
		return -EFAULT;
	    }

	    ustats++;
	    count--;

	}

	return 0;
    }

    case VUMEM_PROF_RESET:
    {
	unsigned int i;

	for (i = 0; (vumem_prof_map[i].id != -1); i++) {
	    int id = vumem_prof_map[i].id;
	    if ((id == VUMEM_PROF_BUFFER_ALLOCATED_SIZE) ||
		(id == VUMEM_PROF_SESSIONS) ||
		(id == VUMEM_PROF_BUFFERS) ||
		(id == VUMEM_PROF_BUFFER_MAPPINGS)) {
		continue;
	    }
	    atomic_set(&dev->stats.counts[vumem_prof_map[i].idx], 0);
	}

	return 0;
    }

    }

    return -EINVAL;
}

    /*
     *
     */
    static void
vumem_prof_init (VumemCltDev* dev)
{
}

#else  /* CONFIG_VUMEM_PROFILE */

    /*
     *
     */
     static int
vumem_prof_op (VumemCltFile*       vumem_file,
	       VumemProfOp __user* uop,
	       unsigned int        sz)
{
    return -ENOSYS;
}

#endif /* CONFIG_VUMEM_PROFILE */

    /*
     *
     */
    static int
vumem_clt_open (struct inode* inode, struct file* file)
{
    unsigned int  minor = iminor(file->f_path.dentry->d_inode);
    unsigned int  major = imajor(file->f_path.dentry->d_inode);
    VumemDev*     vumem_gendev;
    VumemCltDev*  vumem_dev;
    VumemCltFile* vumem_file;

    vumem_gendev = vumem_dev_find(major, minor);
    if (!vumem_gendev) {
	return -ENXIO;
    }
    vumem_dev = &vumem_gendev->clt;

    vumem_file = (VumemCltFile*) kzalloc(sizeof(VumemCltFile), GFP_KERNEL);
    if (!vumem_file) {
	return -ENOMEM;
    }

    vumem_file->type   = VUMEM_FILE_NONE;
    vumem_file->filp   = file;
    vumem_file->dev    = vumem_dev;
    sema_init(&vumem_file->lock, 1);
    file->private_data = vumem_file;

    return 0;
}

    /*
     *
     */
    static int
vumem_clt_release (struct inode* inode, struct file* file)
{
    VumemCltFile* vumem_file = file->private_data;
    int           diag       = 0;

    if (vumem_file->type == VUMEM_FILE_SESSION) {
	vumem_session_release(&vumem_file->session);
    } else if (vumem_file->type == VUMEM_FILE_BUFFER) {
	vumem_buffer_release(&vumem_file->buffer);
    } else {
	kfree(vumem_file);
    }

    return diag;
}

    /*
     *
     */
static int vumem_bad_ioctl (struct VumemCltFile* vumem_file, void* arg,
			    unsigned int sz)
{
    VLINK_ERROR(vumem_file->dev->gen_dev.vlink, "invalid ioctl command\n");
    return -EINVAL;
}

    /*
     *
     */
static VumemCltIoctl vumem_clt_ioctl_ops[VUMEM_IOCTL_CMD_MAX] = {
    (VumemCltIoctl)vumem_session_create,	/*  0 - SESSION_CREATE       */
    (VumemCltIoctl)vumem_buffer_alloc,		/*  1 - BUFFER_ALLOC         */
    (VumemCltIoctl)vumem_buffer_register,	/*  2 - BUFFER_REGISTER      */
    (VumemCltIoctl)vumem_buffer_unregister,	/*  3 - BUFFER_UNREGISTER    */
    (VumemCltIoctl)vumem_buffer_cache_ctl,	/*  4 - BUFFER_CACHE_CTL     */
    (VumemCltIoctl)vumem_buffer_ctl_remote,	/*  5 - BUFFER_CTL_REMOTE    */
    (VumemCltIoctl)vumem_bad_ioctl,		/*  6 - Unused               */
    (VumemCltIoctl)vumem_prof_op,		/*  7 - PROF_OP              */
    (VumemCltIoctl)vumem_bad_ioctl,		/*  8 - SERVER_RECEIVE       */
    (VumemCltIoctl)vumem_bad_ioctl,		/*  9 - SERVER_RESPOND       */
    (VumemCltIoctl)vumem_bad_ioctl,		/* 10 - Unused               */
    (VumemCltIoctl)vumem_bad_ioctl,		/* 11 - Unused               */
    (VumemCltIoctl)vumem_bad_ioctl,		/* 12 - Unused               */
    (VumemCltIoctl)vumem_bad_ioctl,		/* 13 - Unused               */
    (VumemCltIoctl)vumem_bad_ioctl,		/* 14 - Unused               */
    (VumemCltIoctl)vumem_bad_ioctl,		/* 15 - Unused               */
};

    /*
     *
     */
    static int
vumem_clt_ioctl (struct inode* inode, struct file* file,
		 unsigned int cmd, unsigned long arg)
{
    VumemCltFile* vumem_file = file->private_data;
    unsigned int  nr;
    unsigned int  sz;

    nr = _IOC_NR(cmd) & (VUMEM_IOCTL_CMD_MAX - 1);
    sz = _IOC_SIZE(cmd);

    return vumem_clt_ioctl_ops[nr](vumem_file, (void*)arg, sz);
}

    /*
     *
     */
    static ssize_t
vumem_clt_read (struct file* file, char __user* ubuf, size_t cnt, loff_t* ppos)
{
    return -EINVAL;
}

    /*
     *
     */
    static ssize_t
vumem_clt_write (struct file* file, const char __user* ubuf,
		 size_t count, loff_t* ppos)
{
    return -EINVAL;
}

    /*
     *
     */
    static unsigned int
vumem_clt_poll(struct file* file, poll_table* wait)
{
    return -ENOSYS;
}

    /*
     *
     */
static struct file_operations vumem_clt_fops = {
    .owner	= THIS_MODULE,
    .open	= vumem_clt_open,
    .read	= vumem_clt_read,
    .write	= vumem_clt_write,
    .ioctl      = vumem_clt_ioctl,
    .mmap       = vumem_clt_mmap,
    .release	= vumem_clt_release,
    .poll	= vumem_clt_poll,
    .llseek	= no_llseek,
};

    /*
     *
     */
    static int
vumem_clt_vlink_abort (Vlink* vlink, void* cookie)
{
    VumemCltDev* dev = (VumemCltDev*) cookie;

    VLINK_DTRACE(vlink, "vumem_clt_vlink_abort called\n");

    vumem_rpc_wakeup(&dev->rpc_clt);
    vumem_rpc_wakeup(&dev->rpc_srv);

    return 0;
}

    /*
     *
     */
    static int
vumem_clt_vlink_reset (Vlink* vlink, void* cookie)
{
    VumemCltDev* dev = (VumemCltDev*) cookie;

    VLINK_DTRACE(vlink, "vumem_clt_vlink_reset called\n");

    vumem_rpc_reset(&dev->rpc_clt);
    vumem_rpc_reset(&dev->rpc_srv);

    return 0;
}

    /*
     *
     */
    static int
vumem_clt_vlink_start (Vlink* vlink, void* cookie)
{
    VumemCltDev* dev = (VumemCltDev*) cookie;

    VLINK_DTRACE(vlink, "vumem_clt_vlink_start called\n");

    nkops.nk_xirq_unmask(dev->gen_dev.cxirqs);
    nkops.nk_xirq_unmask(dev->gen_dev.cxirqs + 1);

    return 0;
}

    /*
     *
     */
    static int
vumem_clt_vlink_stop (Vlink* vlink, void* cookie)
{
    VumemCltDev* dev = (VumemCltDev*) cookie;

    VLINK_DTRACE(vlink, "vumem_clt_vlink_stop called\n");

    nkops.nk_xirq_mask(dev->gen_dev.cxirqs);
    nkops.nk_xirq_mask(dev->gen_dev.cxirqs + 1);

    vumem_buffers_abort(dev);

    return 0;
}

    /*
     *
     */
    static int
vumem_clt_vlink_cleanup (Vlink* vlink, void* cookie)
{
    VumemCltDev* dev = (VumemCltDev*) cookie;

    VLINK_DTRACE(vlink, "vumem_clt_vlink_cleanup called\n");

    dev->gen_dev.enabled = 0;

    vlink_sessions_cancel(vlink);

    vumem_thread_delete(&dev->thread);

    vumem_gen_dev_cleanup(&dev->gen_dev);

    return 0;
}

    /*
     *
     */
static VlinkOpDesc vumem_clt_vlink_ops[] = {
    { VLINK_OP_RESET,   vumem_clt_vlink_reset   },
    { VLINK_OP_START,   vumem_clt_vlink_start   },
    { VLINK_OP_ABORT,   vumem_clt_vlink_abort   },
    { VLINK_OP_STOP,    vumem_clt_vlink_stop    },
    { VLINK_OP_CLEANUP, vumem_clt_vlink_cleanup },
    { 0,                NULL                    },
};

    /*
     *
     */
    int
vumem_clt_vlink_init (VumemDrv* vumem_drv, Vlink* vlink)
{
    VumemCltDev* dev = &vumem_drv->devs[vlink->unit].clt;
    void*        thread;
    int          diag;

    VLINK_DTRACE(vlink, "vumem_clt_vlink_init called\n");

    dev->gen_dev.vumem_drv = vumem_drv;
    dev->gen_dev.vlink     = vlink;

    if ((diag = vumem_gen_dev_init(&dev->gen_dev)) != 0) {
	vumem_clt_vlink_cleanup(vlink, dev);
	return diag;
    }

    spin_lock_init(&dev->xlink_lock);
    spin_lock_init(&dev->buffer_map_lock);
    spin_lock_init(&dev->session_lock);
    spin_lock_init(&dev->buffer_lock);

    INIT_LIST_HEAD(&dev->buffer_maps);
    INIT_LIST_HEAD(&dev->buffers);
    INIT_LIST_HEAD(&dev->sessions);

    diag = vumem_rpc_setup(&dev->gen_dev, &dev->rpc_clt,
			   VUMEM_RPC_TYPE_CLIENT,
			   VUMEM_RPC_CHAN_REQS);
    if (diag) {
	vumem_clt_vlink_cleanup(vlink, dev);
	return diag;
    }

    diag = vumem_rpc_setup(&dev->gen_dev, &dev->rpc_srv,
			   VUMEM_RPC_TYPE_SERVER,
			   VUMEM_RPC_CHAN_ABORT);
    if (diag) {
	vumem_clt_vlink_cleanup(vlink, dev);
	return diag;
    }

    thread = vumem_thread_create(&dev->gen_dev, vumem_thread, dev);
    if (IS_ERR(thread)) {
	vumem_clt_vlink_cleanup(vlink, dev);
	return PTR_ERR(thread);
    }
    dev->thread = thread;

    VUMEM_PROF_INIT(dev);

    dev->gen_dev.enabled = 1;

    return 0;
}

    /*
     *
     */
    int
vumem_clt_drv_init (VlinkDrv* parent_drv, VumemDrv* vumem_drv)
{
    int diag;

    vumem_drv->parent_drv   = parent_drv;
    vumem_drv->vops         = vumem_clt_vlink_ops;
    vumem_drv->fops         = &vumem_clt_fops;
    vumem_drv->chrdev_major = 0;
    vumem_drv->class        = NULL;
    vumem_drv->devs         = NULL;

    if ((diag = vumem_gen_drv_init(vumem_drv)) != 0) {
	vumem_clt_drv_cleanup(vumem_drv);
	return diag;
    }

    return 0;
}

    /*
     *
     */
    void
vumem_clt_drv_cleanup (VumemDrv* vumem_drv)
{
    vumem_gen_drv_cleanup(vumem_drv);
}

    /*
     *
     */
    static int
vumem_fe_module_init (void)
{
    return 0;
}

    /*
     *
     */
    static void
vumem_fe_module_exit (void)
{

}

module_init(vumem_fe_module_init);
module_exit(vumem_fe_module_exit);

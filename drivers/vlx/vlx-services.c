/*
 ****************************************************************
 *
 *  Component: VLX driver services
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
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

#ifdef VLX_SERVICES_THREADS

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
    /* This include exists in 2.6.6 but functions are not yet exported */
#include <linux/kthread.h>
#endif

    /*
     *  On kernels starting with 2.6.7, we use the kthread_run() API
     *  instead of kernel_thread(), except if the thread is supposed
     *  to suicide, which cannot be implemented using kthreads.
     *  It would be simpler to just use kernel_thread() all the
     *  time...
     */

typedef struct {
    int			(*func) (void*);
    void*		arg;
    const char*		name;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
    _Bool		is_kernel_thread;
#endif
    union {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
	struct {
	    struct task_struct*	desc;
	} kthread;
#endif
	struct {
	    pid_t		pid;
	    struct completion	completion;
	} kernel_thread;
    } u;
} vlx_thread_t;

    static int
vlx_thread_entry (void* arg)
{
    vlx_thread_t*	thread = arg;
    int			diag;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
    if (thread->is_kernel_thread) {
	daemonize (thread->name);
    }
#else
    daemonize (thread->name);
#endif
#else
    daemonize();
    snprintf (current->comm, sizeof current->comm, thread->name);
#endif

    diag = thread->func (thread->arg);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
	/*
	 *  The thread function has finished before kthread_stop() has
	 *  been called. If we exited here immediately, struct task_struct
	 *  "desc" would be deallocated before vlx_thread_join() has used it
	 *  to call kthread_stop(). Therefore, we must await this call here.
	 */
    if (!thread->is_kernel_thread) {
	while (!kthread_should_stop()) {
	    set_current_state (TASK_INTERRUPTIBLE);
	    schedule_timeout (1);
	}
	return diag;
    }
#endif
    complete_and_exit (&thread->u.kernel_thread.completion, diag);
    /*NOTREACHED*/
    return diag;
}

    static void
vlx_thread_join (vlx_thread_t* thread)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
    if (!thread->is_kernel_thread) {
	if (thread->u.kthread.desc) {
	    kthread_stop (thread->u.kthread.desc);
	}
	return;
    }
#endif
    if (thread->u.kernel_thread.pid > 0) {
	    /* On 2.4.20, it is a void function */
	wait_for_completion (&thread->u.kernel_thread.completion);
    }
}

    static int
vlx_thread_start_ex (vlx_thread_t* thread, int (*func) (void*), void* arg,
		     const char* name, _Bool will_suicide)
{
    (void) will_suicide;
    thread->func = func;
    thread->arg  = arg;
    thread->name = name;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
    thread->is_kernel_thread = will_suicide;
    if (!will_suicide) {
	thread->u.kthread.desc = kthread_run (vlx_thread_entry, thread, name);
	if (IS_ERR (thread->u.kthread.desc)) {
	    return PTR_ERR (thread->u.kthread.desc);
	}
	return 0;
    }
#endif
    init_completion (&thread->u.kernel_thread.completion);
    thread->u.kernel_thread.pid = kernel_thread (vlx_thread_entry, thread, 0);
    if (thread->u.kernel_thread.pid < 0) {
	return thread->u.kernel_thread.pid;
    }
    if (!thread->u.kernel_thread.pid) {
	return -EAGAIN;
    }
    return 0;
}

    static inline int
vlx_thread_start (vlx_thread_t* thread, int (*func) (void*), void* arg,
		  const char* name)
{
    return vlx_thread_start_ex (thread, func, arg, name, 0 /*!will_suicide*/);
}

#endif	/* VLX_SERVICES_THREADS */

#ifdef VLX_SERVICES_PROC_NK

    static int
vlx_proc_dir_match (struct proc_dir_entry* dir, const char* name)
{
    const unsigned namelen = strlen (name);

    if (!dir->low_ino) {
	return 0;
    }
    if (dir->namelen != namelen) {
	return 0;
    }
    return !memcmp (name, dir->name, namelen);
}

    static struct proc_dir_entry*
vlx_proc_dir_lookup (struct proc_dir_entry* dir, const char* name)
{
    while (dir && !vlx_proc_dir_match (dir, name)) {
	dir = dir->next;
    }
    return dir;
}

    /*
     *  Starting with kernel 2.6.27, proc_root is no more exported
     *  and no more present in proc_fs.h, but the VLX-specific kernel
     *  still offers it.
     */
extern struct proc_dir_entry proc_root;

    static struct proc_dir_entry*
vlx_proc_nk_lookup (void)
{
    return vlx_proc_dir_lookup (proc_root.subdir, "nk");
}

#endif	/* VLX_SERVICES_PROC_NK */

#ifdef VLX_SERVICES_SEQ_FILE

#include <linux/seq_file.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) && \
    LINUX_VERSION_CODE != KERNEL_VERSION(2,4,21) && \
    LINUX_VERSION_CODE != KERNEL_VERSION(2,4,25)

    static void*
single_start (struct seq_file *p, loff_t *pos)
{
    return NULL + (*pos == 0);
}

    static void*
single_next (struct seq_file *p, void *v, loff_t *pos)
{
    ++*pos;
    return NULL;
}

    static void
single_stop (struct seq_file *p, void *v)
{
}

    static int
single_open (struct file *file, int (*show)(struct seq_file *, void *),
	     void *data)
{
    struct seq_operations *op = kmalloc (sizeof *op, GFP_KERNEL);
    int res = -ENOMEM;

    if (op) {
	op->start = single_start;
	op->next = single_next;
	op->stop = single_stop;
	op->show = show;
	res = seq_open (file, op);
	if (!res) {
	    ((struct seq_file*) file->private_data)->private = data;
	} else {
	    kfree (op);
	}
    }
    return res;
}

    static int
single_release (struct inode *inode, struct file *file)
{
    struct seq_operations *op = ((struct seq_file*) file->private_data)->op;
    int res = seq_release (inode, file);

    kfree (op);
    return res;
}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) */
#endif /* VLX_SERVICES_SEQ_FILE */

#ifdef VLX_SERVICES_UEVENT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)

#include <net/sock.h>
#include <linux/netlink.h>

typedef struct vlx_uevent_t vlx_uevent_t;

struct vlx_uevent_t {
    struct socket*	socket;
    void		(*data_ready) (vlx_uevent_t*, int bytes);
};

    static void
vlx_uevent_sock_data_ready (struct sock* sk, int bytes)
{
    vlx_uevent_t* ue = (vlx_uevent_t*) sk->sk_user_data;

    ue->data_ready (ue, bytes);
}

    static int
vlx_uevent_recv (vlx_uevent_t* ue, char* buf, size_t buflen)
{
    struct msghdr	mh;
    struct kvec		iov;
    int			diag;

    if (buflen <= 0) return -EINVAL;
    memset (&mh, 0, sizeof mh);
    mh.msg_iovlen = 1;
    mh.msg_iov    = (struct iovec*) &iov;
    iov.iov_len   = buflen - 1;	/* Space for terminating '\0' */
    iov.iov_base  = buf;

    diag = kernel_recvmsg (ue->socket, &mh, &iov, 1, buflen, MSG_DONTWAIT);
    if (diag < 0) return diag;
    buf [diag] = '\0';		/* Not really necessary */
    return diag;
}

    static void
vlx_uevent_exit (vlx_uevent_t* ue)
{
    if (ue->socket) {
	sock_release (ue->socket);
	ue->socket = 0;
    }
}

    static int
vlx_uevent_init (vlx_uevent_t* ue, void (*data_ready) (vlx_uevent_t*, int))
{
    signed		diag;
    struct sockaddr_nl	sanl;

    diag = sock_create_kern (PF_NETLINK, SOCK_DGRAM,
			     NETLINK_KOBJECT_UEVENT /*unit*/, &ue->socket);
    if (diag) {
	ue->socket = 0;	/* Just in case it was somehow set */
	return diag;
    }
    if (ue->socket->sk->sk_user_data) {
	vlx_uevent_exit (ue);
	return -EBUSY;
    }
    ue->data_ready                = data_ready;
    ue->socket->sk->sk_data_ready = vlx_uevent_sock_data_ready;
    ue->socket->sk->sk_user_data  = ue;

    memset (&sanl, 0, sizeof sanl);
    sanl.nl_family = AF_NETLINK;
    sanl.nl_groups = 1;		/* bit 0 must be set */

    diag = kernel_bind (ue->socket, (struct sockaddr*) &sanl, sizeof sanl);
    if (diag) {
	vlx_uevent_exit (ue);
	return diag;
    }
    return 0;
}
#endif
#endif	/* VLX_SERVICES_UEVENT */


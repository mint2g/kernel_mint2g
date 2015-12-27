/*****************************************************************************
 *                                                                           *
 *  Component: VLX VLink Wrapper Library.                                    *
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

//#define VLINK_DEBUG

#include "vlink-lib.h"
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>

#define VLX_SERVICES_THREADS
#include <linux/version.h>
#include <linux/kthread.h>
#include "vlx-services.c"

#define VLINK_LIB_PRINTK(ll, m...)		\
    printk(ll "vlink-lib: " m)

#define VLINK_LIB_ERROR(m...)			\
    VLINK_LIB_PRINTK(KERN_ERR, m)

#define VLINK_LIB_WARN(m...)			\
    VLINK_LIB_PRINTK(KERN_WARNING, m)

#ifdef VLINK_DEBUG
#define VLINK_LIB_DTRACE(m...)			\
    VLINK_LIB_PRINTK(KERN_DEBUG, m)
#else
#define VLINK_LIB_DTRACE(m...)
#endif

#define VLINK_LIB_FUNC_ENTER()			\
    VLINK_LIB_DTRACE(">>> %s\n", __FUNCTION__)

#define VLINK_LIB_FUNC_LEAVE()			\
    VLINK_LIB_DTRACE("<<< %s\n", __FUNCTION__)

    /*
     *
     */
    int
vlink_op_register (Vlink* vlink, unsigned int idx, VlinkOp op, void* cookie)
{
    VlinkOpWrap* wrap;

    wrap = (VlinkOpWrap*) kzalloc(sizeof(VlinkOpWrap), GFP_KERNEL);
    if (!wrap) {
	VLINK_ERROR(vlink, "cannot allocate VlinkOpWrap struct (%d bytes)\n",
		    sizeof(VlinkOpWrap));
	return -ENOMEM;
    }

    wrap->op     = op;
    wrap->cookie = cookie;

    VLINK_ASSERT(idx < VLINK_OP_NR);
    VLINK_ASSERT(op);
    list_add(&wrap->link, &(vlink->ops[idx]));

    return 0;
}

    /*
     *
     */
    int
vlink_ops_register (Vlink* vlink, VlinkOpDesc* ops, void* cookie)
{
    VlinkOpDesc* desc;
    int          diag = 0;

    for (desc = ops; desc->op != NULL; desc++) {
	diag = vlink_op_register(vlink, desc->idx, desc->op, cookie);
	if (diag) {
	    break;
	}
    }

    return diag;
}

    /*
     *
     */
    void
vlink_op_perform (Vlink* vlink, unsigned int idx)
{
#ifdef VLINK_DEBUG
    static const char* op_str[] = {
	"RESET",
	"START",
	"ABORT",
	"STOP",
	"CLEANUP",
    };
#endif
    VlinkOpWrap* wrap;

    list_for_each_entry(wrap, &(vlink->ops[idx]), link) {
	VLINK_DTRACE_ENTER(vlink, "%s (0x%p)\n", op_str[idx], wrap->cookie);
	wrap->op(vlink, wrap->cookie);
	VLINK_DTRACE_LEAVE(vlink, "%s (0x%p)\n", op_str[idx], wrap->cookie);
    }
}

    /*
     *
     */
    static void
vlink_ops_cleanup (Vlink* vlink)
{
    unsigned int i;
    VlinkOpWrap* wrap;

    for (i = 0; i < VLINK_OP_NR; i++) {
	while (!list_empty(&(vlink->ops[i]))) {
	    wrap = list_first_entry(&(vlink->ops[i]), VlinkOpWrap, link);
	    list_del(&wrap->link);
	    kfree(wrap);
	}
    }
}

    /*
     *
     */
    static inline void
vlink_admin_event_post (Vlink* vlink, unsigned int event_mask)
{
    unsigned int event;
    unsigned int oevent;

    oevent = atomic_read(&vlink->admin_event);
    while ((event = atomic_cmpxchg(&vlink->admin_event,
				   oevent,
				   oevent | event_mask)) != oevent) {
	oevent = event;
    }

    wake_up(&vlink->admin_event_wait);
}

    /*
     *
     */
    static inline int
vlink_admin_wait_for_event (Vlink* vlink)
{
    wait_event_interruptible(vlink->admin_event_wait,
			     atomic_read(&vlink->admin_event) != 0);
    return atomic_xchg(&vlink->admin_event, 0);
}

    /*
     *
     */
    static inline void
vlink_admin_complete (Vlink* vlink)
{
    wake_up(&vlink->admin_comp_wait);
}

    /*
     *
     */
    static inline int
vlink_admin_wait_for_completion (Vlink* vlink)
{
    return wait_event_interruptible(vlink->admin_comp_wait,
				    VLINK_HIT_STATE(vlink));
}

    /*
     *
     */
    static void
vlink_sysconf_hdl (void* cookie, NkXIrq xirq)
{
    VlinkDrv* drv = (VlinkDrv*) cookie;
    Vlink*    vlink;

    VLINK_LIB_FUNC_ENTER();

    list_for_each_entry(vlink, &drv->vlinks, link) {
	if (VLINK_IS_UNUSABLE(vlink) || VLINK_IS_TERMINATED(vlink)) {
	    continue;
	}
	vlink_admin_event_post(vlink, VLINK_ADMIN_RECONF);
    }

    VLINK_LIB_FUNC_LEAVE();
}

    /*
     *
     */
    static inline void
vlink_peer_notify (Vlink* vlink)
{
    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, vlink->id_peer);
}

#ifdef VLINK_DEBUG
    /*
     *
     */
    static const char*
vlink_admin_event_str (unsigned int event)
{
    static const char* event_str[] = {
	"NONE",
	"RECONF",
	"EXIT",
	"RECONF|EXIT",
    };
    VLINK_ASSERT(event < ARRAY_SIZE(event_str));
    return event_str[event];
}

    /*
     *
     */
    static const char*
vlink_state_str (unsigned int state)
{
    static const char* state_str[] = {
	"CLEAN",
	"STOPPED",
	"RESET",
	"STARTED",
	"UP",
	"ABORTED",
    };
    VLINK_ASSERT(state < ARRAY_SIZE(state_str));
    return state_str[state];
}

    /*
     *
     */
    static const char*
vlink_nk_state_str (unsigned int nk_state)
{
    static const char* nk_state_str[] = {
	"OFF",
	"RESET",
	"ON",
    };
    VLINK_ASSERT(nk_state < ARRAY_SIZE(nk_state_str));
    return nk_state_str[nk_state];
}

    /*
     *
     */
    void
vlink_dump (Vlink* vlink)
{
    VLINK_LIB_DTRACE("--------------------------------------\n");
    VLINK_LIB_DTRACE("Vlink %s [link#%d OS#%d %s OS#%d]\n",
		     vlink->nk_vlink->name,
		     vlink->nk_vlink->link,
		     vlink->id,
		     vlink->server ? "<-" : "->",
		     vlink->id_peer);

    VLINK_LIB_DTRACE("Driver .......... %s\n", vlink->drv->name);
    VLINK_LIB_DTRACE("Type ............ %s\n",
		     vlink->server ? "server" : "client");
    VLINK_LIB_DTRACE("My ID ........... %d\n", vlink->id);
    VLINK_LIB_DTRACE("Peer ID ......... %d\n", vlink->id_peer);
    VLINK_LIB_DTRACE("Unit ............ %d\n", vlink->unit);
    VLINK_LIB_DTRACE("State ........... %s\n", vlink_state_str(vlink->state));
    VLINK_LIB_DTRACE("Target state .... %s\n",
		     vlink_state_str(vlink->state_target));
    VLINK_LIB_DTRACE("My NK state ..... %s\n",
		     vlink_nk_state_str(*vlink->nk_state));
    VLINK_LIB_DTRACE("Peer NK state ... %s\n",
		     vlink_nk_state_str(*vlink->nk_state_peer));
    VLINK_LIB_DTRACE("Users ........... %d\n", atomic_read(&vlink->users));
    VLINK_LIB_DTRACE("Sessions ........ %d\n",
		     atomic_read(&vlink->sessions_count));
    VLINK_LIB_DTRACE("Admin events .... %s\n",
		     vlink_admin_event_str(atomic_read(&vlink->admin_event)));
}

#endif

    /*
     *
     */
    static inline void
vlink_state_print (Vlink* vlink)
{
    VLINK_DTRACE(vlink, "my state is %s (%s), peer is %s\n",
		 vlink_nk_state_str(*vlink->nk_state),
		 vlink_state_str(vlink->state),
		 vlink_nk_state_str(*vlink->nk_state_peer));
}

    /*
     *
     */
    static void
vlink_state_update (Vlink* vlink, unsigned int state)
{
    static const unsigned int nk_states[] = {
	NK_DEV_VLINK_OFF,	/* VLINK_CLEAN   */
	NK_DEV_VLINK_OFF,	/* VLINK_STOPPED */
	NK_DEV_VLINK_RESET,	/* VLINK_RESET   */
	NK_DEV_VLINK_ON,	/* VLINK_STARTED */
	NK_DEV_VLINK_ON,	/* VLINK_UP      */
	NK_DEV_VLINK_ON,	/* VLINK_ABORTED */
    };

    unsigned long flags;
    unsigned int  nk_state = *vlink->nk_state;

    spin_lock_irqsave(&vlink->state_lock, flags);
    vlink->state     = state;
    *vlink->nk_state = nk_states[state];
    spin_unlock_irqrestore(&vlink->state_lock, flags);

    /*
     * Ensure the new state is commited before doing anything else.
     */
    smp_mb();

    vlink_state_print(vlink);

    if (nk_state != *vlink->nk_state) {
	vlink_peer_notify(vlink);
    }
}

    /*
     *
     */
    static void
vlink_abort (Vlink* vlink)
{
    VlinkSession* session;
    unsigned int  secs;
    unsigned long jiffies_count;
    unsigned long jiffies_wait;
    unsigned long jiffies_limit;

    VLINK_FUNC_ENTER(vlink);

    VLINK_ASSERT(VLINK_IS_STARTED(vlink) &&
		 ((*vlink->nk_state_peer == NK_DEV_VLINK_OFF) ||
		  VLINK_TERMINATE(vlink)));

    /*
     * The only way to modify the state of a started vlink (VLINK_UP or
     * VLINK_STARTED) is to call this abort routine.
     * Switching the vlink state to the aborted state (VLINK_ABORTED)
     * is protected by 'sessions_lock'.
     */
    down(&vlink->sessions_lock);

    vlink_state_update(vlink, VLINK_ABORTED);

    list_for_each_entry(session, &vlink->sessions, link) {
	if (session->state == VLINK_SESSION_ALIVE) {
	    session->state = VLINK_SESSION_ABORTED;
	    if (session->op_abort) {
		session->op_abort(session);
	    }
	}
    }

    vlink_op_perform(vlink, VLINK_OP_ABORT);

    up(&vlink->sessions_lock);

    jiffies_count = 0;
    jiffies_wait  = 1;
    jiffies_limit = HZ;
    secs          = 0;
    while (atomic_read(&vlink->users) > 0) {

	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(jiffies_wait);

	jiffies_count += jiffies_wait;

	if (jiffies_count >= jiffies_limit) {
	    secs += (jiffies_count / HZ);
	    jiffies_count = 0;
	    if (jiffies_wait == 1) {
		jiffies_wait = HZ/10;
		continue;
	    } else if (jiffies_wait == HZ/10) {
		jiffies_wait = HZ;
		continue;
	    }
	    VLINK_DTRACE(vlink, "aborted vlink has still %d users after %d "
			 "seconds!\n", atomic_read(&vlink->users), secs);
	    jiffies_limit *= 2;
	}
    }

    VLINK_FUNC_LEAVE(vlink);
}

    /*
     *
     */
    static void
vlink_handshake_up (Vlink* vlink)
{
    switch (vlink->state) {

    case VLINK_STOPPED:
    {
	VLINK_ASSERT(*vlink->nk_state == NK_DEV_VLINK_OFF);

	if (*vlink->nk_state_peer != NK_DEV_VLINK_ON) {

	    vlink_op_perform(vlink, VLINK_OP_RESET);

	    vlink_state_update(vlink, VLINK_RESET);
       }

	break;
    }

    case VLINK_RESET:
    {
	VLINK_ASSERT(*vlink->nk_state == NK_DEV_VLINK_RESET);

	if (*vlink->nk_state_peer != NK_DEV_VLINK_OFF) {

	    vlink_op_perform(vlink, VLINK_OP_START);

	    vlink_state_update(vlink, VLINK_STARTED);
	}

	break;
    }

    case VLINK_STARTED:
    {
	VLINK_ASSERT(*vlink->nk_state == NK_DEV_VLINK_ON);

	if (*vlink->nk_state_peer == NK_DEV_VLINK_ON) {

	    vlink_state_update(vlink, VLINK_UP);
	    break;
	}

	/* Fall through */
    }

    case VLINK_UP:
    {
	VLINK_ASSERT(*vlink->nk_state == NK_DEV_VLINK_ON);

	if (*vlink->nk_state_peer == NK_DEV_VLINK_OFF) {

	    vlink_abort(vlink);
	    vlink_op_perform(vlink, VLINK_OP_STOP);

	    vlink_state_update(vlink, VLINK_STOPPED);
	}

	break;
    }

    default:
	BUG();
    }
}

    /*
     *
     */
    static void
vlink_handshake_down (Vlink* vlink)
{
    if (VLINK_IS_STARTED(vlink)) {
	vlink_abort(vlink);
	vlink_op_perform(vlink, VLINK_OP_STOP);
    } else {
	VLINK_ASSERT(VLINK_IS_USABLE(vlink));
    }
    vlink_state_update(vlink, VLINK_STOPPED);
}

    /*
     *
     */
    static void
vlink_handshake (Vlink* vlink)
{
    if (VLINK_STARTUP(vlink)) {
	vlink_handshake_up(vlink);
    } else {
	vlink_handshake_down(vlink);
    }
}

    /*
     *
     */
    static int
vlink_admin_thread (void* arg)
{
    Vlink*       vlink = (Vlink*) arg;
    unsigned int state;
    unsigned int event;

    VLINK_FUNC_ENTER(vlink);

    for (;;) {
	event = vlink_admin_wait_for_event(vlink);

	VLINK_FUNC_MSG(vlink, "event %s\n", vlink_admin_event_str(event));

	if (event == VLINK_ADMIN_EXIT) {
	    break;
	}

	for (;;) {
	    state = vlink->state;
	    vlink_handshake(vlink);
	    if (vlink->state == state) {
		break;
	    }
	}

	if (VLINK_HIT_STATE(vlink)) {
	    vlink_admin_complete(vlink);
	    if (event & VLINK_ADMIN_EXIT) {
		break;
	    }
	}
    }

    VLINK_FUNC_LEAVE(vlink);

    return 0;
}

    /*
     *
     */
    static inline void
vlink_session_inc (Vlink* vlink)
{
    atomic_inc(&vlink->sessions_count);
    /*
     * This barrier ensures vlink->sessions_count is actually incremented
     * before starting executing the code that follows
     * (see vlink_session_create()).
     */
    smp_mb__after_atomic_inc();
}

    /*
     *
     */
    static inline void
vlink_session_dec (Vlink* vlink)
{
    if (atomic_dec_and_test(&vlink->sessions_count)) {
	wake_up(&vlink->sessions_end_wait);
    }
}

    /*
     *
     */
    int
vlink_session_create (Vlink* vlink, void* private, VlinkSessionOp op_abort,
		      VlinkSession** psession)
{
    VlinkSession* session;
    int           diag;

    VLINK_FUNC_ENTER(vlink);

    *psession = NULL;

    if (down_interruptible(&vlink->sessions_start_lock)) {
	diag = -ERESTARTSYS;
	goto out_func_leave;
    }

    vlink_session_inc(vlink);

    /*
     * Test the vlink state after vlink->sessions_count has been incremented
     * (relies on a barrier in vlink_session_inc()) and return if the
     * vlink cannot be used.
     * The vlink_drv_shutdown() routine does the opposite: it updates the
     * vlink state first and then wait for vlink->sessions_count to go down
     * to 0.
     * This mechanism ensures that after a shutdown operation has been
     * triggered, all started vlink sessions are awaited for completion
     * (by calling vlink_session_dec()) and no new vlink session can start.
     * This enables to synchronize vlink usages with vlink shutdowns.
     */
    if (VLINK_IS_UNUSABLE(vlink) || VLINK_TERMINATE(vlink)) {
	vlink_session_dec(vlink);
	diag = -ENXIO;
	goto out_unlock;
    }

    session = (VlinkSession*) kzalloc(sizeof(VlinkSession), GFP_KERNEL);
    if (!session) {
	VLINK_ERROR(vlink, "cannot allocate VlinkSession struct (%d bytes)\n",
		    sizeof(VlinkSession));
	vlink_session_dec(vlink);
	diag = -ENOMEM;
	goto out_unlock;
    }

    session->state    = VLINK_SESSION_NEW;
    session->vlink    = vlink;
    session->private  = private;
    session->op_abort = op_abort;

    do {

	if (!VLINK_IS_UP(vlink)) {
	    vlink_admin_event_post(vlink, VLINK_ADMIN_RECONF);
	    if ((diag = vlink_admin_wait_for_completion(vlink)) != 0) {
		continue;
	    }
	}

	if (down_interruptible(&vlink->sessions_lock)) {
	    diag = -ERESTARTSYS;
	    continue;
	}

	if (VLINK_IS_UP(vlink)) {

	    /*
	     * vlink->state cannot change from VLINK_UP to another
	     * value in this critical section. 'sessions_lock' protects
	     * us against an asynchronous abort (see vlink_abort()).
	     */

	    session->state = VLINK_SESSION_ALIVE;
	    vlink_session_get(session);
	    vlink_session_enter(session);
	    atomic_set(&session->entered, 1);
	    /*
	     * No need of a barrier here, since the access to a list of
	     * sessions is always protected by the sessions_lock semaphore.
	     */
	    list_add(&session->link, &vlink->sessions);

	    diag = 0;
	} else {
	    if (VLINK_TERMINATE(vlink)) {
		diag = -ENXIO;
	    } else {
		diag = -EAGAIN;
	    }
	}

	up(&vlink->sessions_lock);

    } while (diag == -EAGAIN);

    if (diag) {
	vlink_session_release(session);
    } else {
	*psession = session;
    }

out_unlock:
    up(&vlink->sessions_start_lock);

out_func_leave:
    VLINK_FUNC_LEAVE(vlink);

    return diag;
}

    /*
     *
     */
    void
vlink_session_release (VlinkSession* session)
{
    Vlink* vlink = session->vlink;

    VLINK_FUNC_ENTER(vlink);

    VLINK_ASSERT(atomic_read(&session->refcount) == 0);

    if (session->state != VLINK_SESSION_NEW) {
	down(&vlink->sessions_lock);
	list_del(&session->link);
	up(&vlink->sessions_lock);
    }

    vlink_session_dec(vlink);

    kfree(session);

    VLINK_FUNC_LEAVE(vlink);
}

    /*
     *
     */
    static int
vlink_create (VlinkDrv* drv, NkDevVlink* nk_vlink, unsigned int type)
{
    Vlink*       vlink;
    unsigned int i;

    vlink = (Vlink*) kzalloc(sizeof(Vlink), GFP_KERNEL);
    if (!vlink) {
	VLINK_LIB_ERROR("%s: cannot allocate a Vlink struct (%d bytes)\n",
			drv->name, sizeof(Vlink));
	return -ENOMEM;
    }

    if (type == VLINK_DRV_TYPE_SERVER) {
	vlink->server        = 1;
	vlink->unit          = drv->nr_servers++;
	vlink->id            = nk_vlink->s_id;
	vlink->id_peer       = nk_vlink->c_id;
	vlink->nk_state      = &nk_vlink->s_state;
	vlink->nk_state_peer = &nk_vlink->c_state;
    } else {
	vlink->server        = 0;
	vlink->unit          = drv->nr_clients++;
	vlink->id            = nk_vlink->c_id;
	vlink->id_peer       = nk_vlink->s_id;
	vlink->nk_state      = &nk_vlink->c_state;
	vlink->nk_state_peer = &nk_vlink->s_state;
    }

    vlink->drv          = drv;
    vlink->nk_vlink     = nk_vlink;
    vlink->sym_vlink    = NULL;
    vlink->state_target = VLINK_UP;
    vlink->admin_thread = NULL;

    atomic_set(&vlink->admin_event, 0);
    atomic_set(&vlink->users, 0);
    atomic_set(&vlink->sessions_count, 0);

    spin_lock_init(&vlink->state_lock);
    init_waitqueue_head(&vlink->admin_event_wait);
    init_waitqueue_head(&vlink->admin_comp_wait);
    init_waitqueue_head(&vlink->sessions_end_wait);
    sema_init(&vlink->sessions_start_lock, 0);
    sema_init(&vlink->sessions_lock,  1);

    INIT_LIST_HEAD(&vlink->sessions);

    for (i = 0; i < VLINK_OP_NR; i++) {
	INIT_LIST_HEAD(&(vlink->ops[i]));
    }

    VLINK_ASSERT(*vlink->nk_state == NK_DEV_VLINK_OFF);
    vlink_state_update(vlink, VLINK_CLEAN);

    list_add(&vlink->link, &drv->vlinks);

    return 0;
}

    /*
     *
     */
    static void
vlink_destroy (Vlink* vlink)
{
    kfree(vlink);
}

    /*
     *
     */
    static int
vlink_thread_create (Vlink* vlink)
{
    vlx_thread_t* thread;
    int           diag;
    unsigned int  len;
    char*         name;

    len = NK_DEV_VLINK_NAME_LIMIT + sizeof("-xxx-xxx") + 4;

    thread = (vlx_thread_t*) kzalloc(sizeof(vlx_thread_t) + len, GFP_KERNEL);
    if (!thread) {
	VLINK_ERROR(vlink,
		    "cannot allocate a vlx_thread_t struct (%d bytes)\n",
		    sizeof(vlx_thread_t) + len);
	return -ENOMEM;
    }

    name = ((char*) thread) + sizeof(vlx_thread_t);
    sprintf(name, "%s-%s-%d",
	    vlink->nk_vlink->name,
	    vlink->server ? "srv" : "clt",
	    vlink->nk_vlink->link);

    diag = vlx_thread_start(thread, vlink_admin_thread, vlink, name);
    if (diag) {
	VLINK_ERROR(vlink, "cannot create an admin thread\n");
	kfree(thread);
	return diag;
    }

    vlink->admin_thread = thread;

    return 0;
}

    /*
     *
     */
    static void
vlink_thread_destroy (Vlink* vlink)
{
    if (vlink->admin_thread) {
	vlx_thread_join(vlink->admin_thread);
	kfree(vlink->admin_thread);
	vlink->admin_thread = NULL;
    }
}

    /*
     *
     */
    static int
vlink_attach_sym_vlinks (VlinkDrv* drv)
{
    Vlink* svlink;
    Vlink* cvlink;
    Vlink* vlink;
    int    diag = 0;

    list_for_each_entry(svlink, &drv->vlinks, link) {
	if ((!svlink->server) || (svlink->sym_vlink)) {
	    continue;
	}
	list_for_each_entry(cvlink, &drv->vlinks, link) {
	    if ((cvlink->server) || (cvlink->sym_vlink)) {
		continue;
	    }
	    if ((svlink->nk_vlink->link == cvlink->nk_vlink->link) &&
		(svlink->id             == cvlink->id            ) &&
		(svlink->id_peer        == cvlink->id_peer       )) {
		svlink->sym_vlink = cvlink;
		cvlink->sym_vlink = svlink;
		cvlink->unit      = svlink->unit;
		break;
	    }
	}
    }

    list_for_each_entry(vlink, &drv->vlinks, link) {
	if (!vlink->sym_vlink) {
	    VLINK_ERROR(vlink, "symmetric vlink is missing\n");
	    diag = -EINVAL;
	}
    }

    return diag;
}

    /*
     *
     */
    int
vlink_drv_probe (VlinkDrv* drv)
{
    NkDevVlink* nk_vlink;
    NkPhAddr    nk_plink = 0;
    NkOsId      my_id    = nkops.nk_id_get();
    unsigned    flags    = drv->flags;
    int         diag     = 0;

    VLINK_LIB_FUNC_ENTER();

    drv->state      = VLINK_DRV_PROBED;
    drv->nr_units   = 0;
    drv->nr_clients = 0;
    drv->nr_servers = 0;
    drv->sysconf_id = 0;
    INIT_LIST_HEAD(&drv->vlinks);

    while ((nk_plink = nkops.nk_vlink_lookup(drv->name, nk_plink)) != 0) {

	nk_vlink = (NkDevVlink*) nkops.nk_ptov(nk_plink);

	if ((nk_vlink->s_id == my_id) && (flags & VLINK_DRV_TYPE_SERVER)) {
	    diag = vlink_create(drv, nk_vlink, VLINK_DRV_TYPE_SERVER);
	    if (diag) {
		break;
	    }
	}
	if ((nk_vlink->c_id == my_id) && (flags & VLINK_DRV_TYPE_CLIENT)) {
	    diag = vlink_create(drv, nk_vlink, VLINK_DRV_TYPE_CLIENT);
	    if (diag) {
		break;
	    }
	}
    }

    if (!diag && VLINK_DRV_IS_SYMMETRIC(drv)) {
	diag = vlink_attach_sym_vlinks(drv);
    }

    if (diag) {
	vlink_drv_cleanup(drv);
    } else {
	drv->nr_units = (flags & VLINK_DRV_TYPE_SERVER ?
			 drv->nr_servers : drv->nr_clients);
    }

    VLINK_LIB_FUNC_LEAVE();

    return diag;
}

    /*
     *
     */
    int
vlink_drv_startup (VlinkDrv* drv)
{
    Vlink* vlink;
    int    diag;

    VLINK_LIB_FUNC_ENTER();

    VLINK_ASSERT(drv->state == VLINK_DRV_PROBED);
    VLINK_ASSERT(drv->init);
    VLINK_ASSERT(drv->vlink_init);

    if (!drv->nr_units) {
	VLINK_LIB_WARN("%s: no vlink has been probed\n", drv->name);
	VLINK_LIB_FUNC_LEAVE();
	return 0;
    }

    drv->sysconf_id = nkops.nk_xirq_attach (NK_XIRQ_SYSCONF,
					    vlink_sysconf_hdl,
					    drv);
    if (!drv->sysconf_id) {
	VLINK_LIB_ERROR("%s: cannot attach sysconf xirq\n", drv->name);
	VLINK_LIB_FUNC_LEAVE();
	return -ENOMEM;
    }

    if ((diag = drv->init(drv)) != 0) {
	VLINK_LIB_FUNC_LEAVE();
	return diag;
    }

    list_for_each_entry(vlink, &drv->vlinks, link) {

	VLINK_DTRACE(vlink, "initializing vlink...\n");

	if (vlink_thread_create(vlink)) {
	    continue;
	}

	if (vlink->drv->vlink_init(vlink)) {
	    VLINK_ERROR(vlink, "cannot initialize vlink\n");
	    vlink_admin_event_post(vlink, VLINK_ADMIN_EXIT);
	    vlink_thread_destroy(vlink);
	    continue;
	}

	vlink_state_update(vlink, VLINK_STOPPED);
    }

    list_for_each_entry(vlink, &drv->vlinks, link) {
	vlink_sessions_start(vlink);
	if (VLINK_IS_USABLE(vlink)) {
	    vlink_admin_event_post(vlink, VLINK_ADMIN_RECONF);
	}
    }

    drv->state = VLINK_DRV_STARTED;

    VLINK_LIB_FUNC_LEAVE();

    return 0;
}

    /*
     *
     */
    int
vlink_drv_shutdown (VlinkDrv* drv)
{
    Vlink* vlink;

    VLINK_LIB_FUNC_ENTER();

    VLINK_ASSERT(drv->state == VLINK_DRV_STARTED);

    list_for_each_entry(vlink, &drv->vlinks, link) {
	if (VLINK_IS_UNUSABLE(vlink) || VLINK_IS_TERMINATED(vlink)) {
	    continue;
	}
	vlink->state_target = VLINK_STOPPED;
	vlink_admin_event_post(vlink, VLINK_ADMIN_RECONF);
	vlink_admin_wait_for_completion(vlink);
    }

    list_for_each_entry(vlink, &drv->vlinks, link) {
	if (VLINK_IS_USABLE(vlink)) {
	    VLINK_ASSERT(VLINK_IS_TERMINATED(vlink));
	    wait_event(vlink->sessions_end_wait,
		       atomic_read(&vlink->sessions_count) == 0);
	}
    }

    drv->state = VLINK_DRV_STOPPED;

    VLINK_LIB_FUNC_LEAVE();

    return 0;
}

    /*
     *
     */
    void
vlink_drv_cleanup (VlinkDrv* drv)
{
    Vlink* vlink;

    VLINK_LIB_FUNC_ENTER();

    VLINK_ASSERT(drv->state != VLINK_DRV_STARTED);
    VLINK_ASSERT(drv->cleanup);

    if (drv->sysconf_id) {
	nkops.nk_xirq_detach(drv->sysconf_id);
	drv->sysconf_id = 0;
    }

    list_for_each_entry(vlink, &drv->vlinks, link) {

	if (vlink->state == VLINK_STOPPED) {
	    vlink_admin_event_post(vlink, VLINK_ADMIN_EXIT);
	    vlink_thread_destroy(vlink);
	    vlink_op_perform(vlink, VLINK_OP_CLEANUP);
	    vlink_state_update(vlink, VLINK_CLEAN);
	} else {
	    VLINK_ASSERT(vlink->state == VLINK_CLEAN);
	}
    }

    if (drv->state == VLINK_DRV_STOPPED) {
	drv->cleanup(drv);
    }

    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(HZ/10);

    while (!list_empty(&drv->vlinks)) {
	vlink = list_first_entry(&drv->vlinks, Vlink, link);
	list_del(&vlink->link);

	vlink_ops_cleanup(vlink);

	VLINK_ASSERT(vlink->state == VLINK_CLEAN);
	VLINK_ASSERT(atomic_read(&vlink->users) == 0);
	VLINK_ASSERT(atomic_read(&vlink->sessions_count) == 0);
	VLINK_ASSERT(list_empty(&vlink->sessions));

	vlink_destroy(vlink);
    }

    drv->nr_units = drv->nr_clients = drv->nr_servers = 0;
    drv->state = VLINK_DRV_CLEAN;

    VLINK_LIB_FUNC_LEAVE();
}

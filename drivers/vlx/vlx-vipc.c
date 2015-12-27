/*
 ****************************************************************
 *
 *  Component: VLX Virtual IPC driver
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

/*----- System includes -----*/

#include <linux/sched.h>
#include "vlx-vipc.h"

/*----- Local configuration -----*/

#if 0
#define VIPC_DEBUG
#endif

#if 0
#define VIPC_SAFE_COOKIES
#endif

/*----- Tracing -----*/

#undef TRACE
#undef ETRACE
#undef DTRACE

#define TRACE(x...)	printk (KERN_NOTICE "VIPC: " x)
#define ETRACE(x...)	printk (KERN_ERR "VIPC: " x)

#ifdef VIPC_DEBUG
#define DTRACE(format, args...)	\
	printk ("(%d) %s: " format, current->tgid, __func__, ##args)
#else
#define DTRACE(x...)
#endif

/*----- Version compatibility functions -----*/

#ifndef list_for_each_entry
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		     prefetch(pos->member.next);			\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, typeof(*pos), member),	\
		     prefetch(pos->member.next))
#endif

/*----- VIPC list -----*/ 
    
#ifdef VIPC_SAFE_COOKIES
#define VIPC_SET_COOKIE(list,place,object) \
    do {(place) = (object)->cookie = ++list->last_cookie;} while (0)
#else
typedef union {
    nku64_f	data;
    void*	ptr;
} vipc_cookie_t;

#define VIPC_SET_COOKIE(list,place,object) \
    do {(place) = 0; VIPC_COOKIE (place) = (object);} while (0)
#define VIPC_COOKIE(cookie)	((vipc_cookie_t*) &(cookie))->ptr
#endif

    void
vipc_list_init (vipc_list_t* list)
{
#ifdef VIPC_SAFE_COOKIES
    struct timeval tv;

    do_gettimeofday(&tv);
    list->last_cookie = (nku64_f) tv.tv_sec << 32;
#endif
    INIT_LIST_HEAD (&list->list);
    spin_lock_init (&list->spinlock);
}

/*----- VIPC waiter -----*/

    void
vipc_waiter_init (vipc_waiter_t* waiter)
{
    INIT_LIST_HEAD (&waiter->list);
#ifdef VIPC_SAFE_COOKIES
    waiter->cookie = 0;
#endif
    waiter->detached = waiter->occurred = 0;
    waiter->task = current;
}

    void
vipc_list_add (vipc_list_t* list, nku64_f* cookie, vipc_waiter_t* waiter)
{
    unsigned long spinlock_flags;

    VIPC_LIST_LOCK (list, spinlock_flags);
    VIPC_SET_COOKIE (list, *cookie, waiter);
    list_add (&waiter->list, &list->list);
    VIPC_LIST_UNLOCK (list, spinlock_flags);
}

    void
vipc_list_del (vipc_list_t* list, vipc_waiter_t* waiter)
{
    unsigned long spinlock_flags;

    VIPC_LIST_LOCK (list, spinlock_flags);
    if (!waiter->detached) {
	list_del (&waiter->list);
    }
    VIPC_LIST_UNLOCK (list, spinlock_flags);
}

#ifdef VIPC_SAFE_COOKIES
    vipc_waiter_t*
vipc_list_cookie_to_waiter (vipc_list_t* list, nku64_f cookie)
{
    unsigned long spinlock_flags;
    vipc_waiter_t* waiter;

    VIPC_LIST_LOCK (list, spinlock_flags);
    list_for_each_entry (waiter, &list->list, list) {
	if (waiter->cookie == cookie) {
	    waiter->cookie = 0;	/* Prevent stale/duplicate replies */
	    VIPC_LIST_UNLOCK (list, spinlock_flags);
	    return waiter;
	}
    }
    VIPC_LIST_UNLOCK (list, spinlock_flags);
    ETRACE ("Invalid waiter cookie %llx\n", cookie);
    return NULL;
}
#else
    vipc_waiter_t*
vipc_list_cookie_to_waiter (vipc_list_t* list, nku64_f cookie)
{
    (void) list;
    return cookie ? VIPC_COOKIE (cookie) : NULL;
}
#endif

    void
vipc_list_wait (vipc_list_t* list, vipc_waiter_t* waiter)
{
	/* This #ifdef is probably no more necessary */
#ifdef VIPC_SAFE_COOKIES
    set_current_state (TASK_INTERRUPTIBLE);
#else
    set_current_state (TASK_UNINTERRUPTIBLE);
#endif
	/* On SMP, we would need more sync here */
    while (!waiter->occurred) {
	schedule();
	    /* This #ifdef is probably no more necessary */
#ifdef VIPC_SAFE_COOKIES
	set_current_state (TASK_INTERRUPTIBLE);
#else
	set_current_state (TASK_UNINTERRUPTIBLE);
#endif
    }
	/* Set thread state back to running if while() was never entered */
    set_current_state (TASK_RUNNING);
    vipc_list_del (list, waiter);
}

/*----- VIPC VMQ -----*/ 

    /* Executes in interrupt context */

    int
vipc_ctx_process_reply (vipc_ctx_t* ctx, nku64_f* reply)
{
    vipc_result_t* result;

    DTRACE ("link %d cookie %llx\n", vmq_peer_osid (ctx->link), *reply);
    if ((result = vipc_ctx_cookie_to_result (ctx, *reply)) == NULL) {
	ETRACE ("Got invalid cookie %llx\n", *reply);
	return -1;
    }
    result->reply = reply;
    vipc_waiter_wakeup (&result->waiter);
    return 0;
}

    /*
     *   IN: have req
     *  OUT: req released, may have reply
     */

    nku64_f*
vipc_ctx_call (vipc_ctx_t* ctx, nku64_f* req)
{
    vipc_result_t result;

    vipc_result_init (&result, ctx);
    vipc_list_add (&ctx->list, req, &result.waiter);
    vmq_msg_send (ctx->link, req);
    vipc_list_wait (&ctx->list, &result.waiter);
    return result.reply;
}

    void
vipc_ctx_abort_calls (vipc_ctx_t* ctx)
{
    unsigned long spinlock_flags;
    vipc_result_t* result;

    DTRACE ("\n");
    VIPC_LIST_LOCK (&ctx->list, spinlock_flags);
    list_for_each_entry (result, &ctx->list.list, waiter.list) {
	DTRACE ("aborting %llx\n", result->waiter.cookie);
	result->reply = NULL;
	vipc_waiter_wakeup (&result->waiter);
    }
    VIPC_LIST_UNLOCK (&ctx->list, spinlock_flags);
}

/*----- Module description -----*/

EXPORT_SYMBOL (vipc_ctx_abort_calls);
EXPORT_SYMBOL (vipc_ctx_call);
EXPORT_SYMBOL (vipc_ctx_process_reply);
EXPORT_SYMBOL (vipc_list_add);
EXPORT_SYMBOL (vipc_list_cookie_to_waiter);
EXPORT_SYMBOL (vipc_list_del);
EXPORT_SYMBOL (vipc_list_init);
EXPORT_SYMBOL (vipc_list_wait);
EXPORT_SYMBOL (vipc_waiter_init);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Adam Mirowski <adam.mirowski@redbend.com>");
MODULE_DESCRIPTION ("VLX VIPC communications driver");

/*----- End of file -----*/

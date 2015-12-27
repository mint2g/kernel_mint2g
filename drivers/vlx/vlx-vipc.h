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

#ifndef VLX_VIPC_H
#define VLX_VIPC_H

/*----- System includes -----*/

#include <linux/version.h>
#include <linux/module.h>       /* __exit, __init */
#include "vlx-vmq.h"

/*----- VIPC list -----*/ 
    
    /* List protected by a lock */
typedef struct {
    struct list_head    list;
    spinlock_t		spinlock;
    nku64_f		last_cookie;	/* For safe cookies only */
} vipc_list_t;

#define VIPC_LIST_LOCK(_list, _flags) \
    spin_lock_irqsave (&(_list)->spinlock, _flags)

#define VIPC_LIST_UNLOCK(_list, _flags) \
    spin_unlock_irqrestore (&(_list)->spinlock, _flags)

void	vipc_list_init		(vipc_list_t*);

/*----- VIPC waiter -----*/

    /* List is useful for safe cookies and for aborting */
typedef struct {
    struct list_head	list;
    nku64_f		cookie;		/* For safe cookies only */
    volatile _Bool	detached;
    volatile _Bool	occurred;
    struct task_struct*	task;
} vipc_waiter_t;

    static void inline
vipc_waiter_wakeup (vipc_waiter_t* waiter)
{
    waiter->occurred = 1;
    wake_up_process (waiter->task);
}

    /* List must be locked */

    static void inline
vipc_waiter_detach (vipc_waiter_t* waiter)
{
    list_del (&waiter->list);
    waiter->detached = 1;
}

void	vipc_waiter_init	(vipc_waiter_t*);
void	vipc_list_add		(vipc_list_t*, nku64_f* cookie, vipc_waiter_t*);
void	vipc_list_del		(vipc_list_t*, vipc_waiter_t*);
vipc_waiter_t*
	vipc_list_cookie_to_waiter (vipc_list_t*, nku64_f cookie);
void	vipc_list_wait		(vipc_list_t*, vipc_waiter_t*);

/*----- VIPC VMQ -----*/ 

typedef struct {
    vipc_list_t		list;	/* Must be first */
    vmq_link_t*		link;
} vipc_ctx_t;

    static inline void
vipc_ctx_init (vipc_ctx_t* ctx, vmq_link_t* link)
{
    vipc_list_init (&ctx->list);
    ctx->link = link;
}

typedef struct {
    vipc_waiter_t	waiter;	/* Must be first */
    nku64_f*		reply;
    vipc_ctx_t*		ctx;
} vipc_result_t;

    static inline void
vipc_result_init (vipc_result_t* result, vipc_ctx_t* ctx)
{
    vipc_waiter_init (&result->waiter);
    result->reply = NULL;	/* For aborts (oom-killer) */
    result->ctx = ctx;
}

    static inline vipc_result_t*
vipc_ctx_cookie_to_result (vipc_ctx_t* ctx, nku64_f cookie)
{
    return (vipc_result_t*) vipc_list_cookie_to_waiter (&ctx->list, cookie);
}

    /* Executes in interrupt context */
int	vipc_ctx_process_reply	(vipc_ctx_t*, nku64_f* reply);

    /*
     *   IN: have req
     *  OUT: req released, may have reply
     */
nku64_f*
	vipc_ctx_call		(vipc_ctx_t*, nku64_f* request);
void	vipc_ctx_abort_calls	(vipc_ctx_t*);

#endif	/* VLX_VIPC_H */


/*
 ****************************************************************
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
 ****************************************************************
 */

/*! \file vuart.c
 *  \brief Implement virtual UART driver on top of nkdki interface
 */ 

#ifdef __KERNEL__
#define VUART_INTR_MASK(flag) local_irq_save(flag)
#define VUART_INTR_UNMASK(flag) local_irq_restore(flag)
#define PRINT	printk
#endif 

#ifdef NU_OSWARE
#include <osware/osware.h>
#include <osware/ring.h>
#define VUART_INTR_MASK(flag) imsIntrMask_f()
#define VUART_INTR_UNMASK(flag) imsIntrUnmask_f()
#define PRINT	printnk
#endif

#ifndef VUART_INTR_MASK
#include <f_nk.h>
#include <nk.h>
#include <nkdev.h>
#define VUART_INTR_MASK(flag) imsIntrMask_f()
#define VUART_INTR_UNMASK(flag) imsIntrUnmask_f()
#define PRINT	printnk
#endif

#ifndef VUART_H
#include "vuart.h"
#endif

#define MAX_RING_DESC 32

extern NkDevOps nkops;
extern char*	strncpy(char*, const char*, unsigned int);


/*!  
 * Ring descriptor : used to store transmission/reception buffer
 */
typedef struct RingDesc {
	/*! Physical address of the buffer */
    NkPhAddr		paddr; 

	/*! 
	 * size of the buffer with some subtility
	 * size == 0  means ring desc is free 
         * size == 1  means ring desc is allocated
	 * size == 2  means the size to send is 0 
	 */
    nku16_f		size;

	/*! real size - the size which has been allocated */
    nku16_f		rsize; 
} RingDesc;

/*!
 * Memory allocator descriptor
 * In this first simplified version we just keep a current pointer on the
 * free buffer
 */
typedef struct  {
	/*! current pointer on free area */
    NkPhAddr	start; 
} AllocDesc;



/*!
 * This describes all information related to event invocation. There 
 * is one event structure per site.
 */
typedef struct {
	/*! The handler itself */
    NkVuartEventHandler	hdl;

	/*! opaque data passed to event handler */
    void*		cookie;

    NkDevVuart*		vuart;

	/*!
	 * This indicates if we want to be notified when the event is posted
	 */
    char		subscribe;

	/*! 
	 * this a boolean value which indicates if the event has been posted
	 */
    char		pending;

	/*
	 * fillers
	 */
    char		filler_1;
    char		filler_2;

	/*!
	 * This indicates the irq used to handle this interrupt
	 */
    NkXIrq		irq;

	/*!
	 * Remote os id.
	 */
    NkOsId		pid;

	/*!
	 * parameter 
	 */
    NkVuartParam	param;
} Event;

/*!
 * The whole structure Virtual Uart structure.
 */

/*!
 * it includes :
 *	- generic device header
 *	- ring header
 *	- ring descriptors
 *	- device name
 *	- event structure
 */

struct NkDevVuart {

	/*!
	 * Generic device structure 
	 */
    NkDevDesc	dev;

	/*!
	 * configuration parameter
	 */
    NkVuartConfig	config;

	/*!
	 * Ring descriptor a la Vladimir 
	 * one ring for each communication way.
	 */
    NkDevRing	ring[2];


	/*!
	 * Event handling related information one per site
	 */
    Event	events[2][NK_VUART_EVENT_MAX];

	/*!
	 * Allocator descriptor to allocate communication buffer.
	 * Note that if buffer are external this should be not 
	 */
    AllocDesc	allocator[2];

	/*!
	 * Ring descriptor for emission and reception.
	 */
    RingDesc	descs[2][MAX_RING_DESC];

	/*!
	 * device name 
	 */
    char	name[8];

	/*!
	 * break signal
	 */
    NkVuartParam	param;

	/*!
	 * boolean indicating if the device is opened.
	 */
    char	opened[2];

	/*!
	 * get ctrl
	 */
    NkVuartModemSignal	sigs[2];
};


static NkOsId	los;
static void*	ptov;


#define assert(A)	\
	if (!(A)) { PRINT("%s:%d: assert failed\n", __FILE__, __LINE__); }

#define precond(A)	\
	if (!(A)) { PRINT("%s: pre-condition failure\n", __func__); }


#ifdef DBG_VUART
int	vuart_tracecontrol = 1;
#define _PRINT(LEVEL, ...) \
	if ((1 << (LEVEL)) & vuart_tracecontrol) PRINT(__VA_ARGS__)

#define trace(LEVEL, ...)   \
	_PRINT(LEVEL, "vuart" __VA_ARGS__)

#define prolog(A, ...)						\
	_PRINT(1, "%d>%s" A, los, __func__,  __VA_ARGS__)

#define epilog(A, ...)						\
	_PRINT(1, "%d<%s" A, los, __func__,  __VA_ARGS__)

#define xintrprolog(A, ...)						\
	_PRINT(3, "%d>%s" A, los, __func__,  __VA_ARGS__)

#define xintrepilog(A, ...)						\
	_PRINT(3, "%d<%s" A, los, __func__,  __VA_ARGS__)

#define logemitbuffer(A, B) \
    if (vuart_tracecontrol & 4) { PRINT("%d>", los);_dumparray(A, B); }

#define logrecvbuffer(A, B) \
    if (vuart_tracecontrol & 4) { PRINT("%d<", los);_dumparray(A, B); }


    /*
     * used to dump message
     */
    static void
_dumparray(char* b, int size)
{
    static char ch[17] = "0123456789abcdef";
    static char line[80];
    int		i, j;
    i = 0; j = 0;
    while ( i < size ) {
	if (b[i] >= 0x20 && b[i] < 127) {
	    line[j] = b[i]; 
	    j++;
	} else {
	    line[j] = ':';
	    line[j + 1] = ch[(b[i] >> 4) & 0xf];
	    line[j + 2] = ch[b[i] & 0xf];
	    j += 3;
	}

	i++;
	if (j >= sizeof(line) - 5) {
	    line[ j ] = '\n';
	    line[ j + 1] = 0;
	    PRINT("%s", line);
	    j = 0;
	}
    }

    line[ j ] = '\n';
    line[ j + 1 ] = 0;
    PRINT("%s", line);
}

#else
#define prolog(...)
#define epilog(...)
#define xintrprolog(...)
#define xintrepilog(...)
#define trace(...)
#define trace(...)
#define logemitbuffer(A, B)
#define logrecvbuffer(A, B) 
#endif

#define warn(...)	PRINT("%s:",__func__) ; PRINT( __VA_ARGS__);



/*
 * misc macro used for precondition
 */
#define vuart_valid(VUART) ((VUART) && (VUART)->dev.dev_id == NK_DEV_ID_UART)
#define vuart_opened(VUART, LIDX) ((VUART)->opened[LIDX])
#define vuart_bufdesc_valid(DESC) ((DESC) != 0)
#define vuart_event_valid(E)	\
    ( (E) >= NK_VUART_TX_READY && (E) < NK_VUART_EVENT_MAX )


/*!
 * This function returns the index of producer ring in lidx
 * and receptor ring ridx for this site.
 */
#define getidx(vuart, lidx, ridx)		\
    if ((vuart)->ring[0].pid == los) {		\
	(lidx) = 0; (ridx) = 1;			\
    } else {					\
	(lidx) = 1; (ridx) = 0;			\
    }

/*!
 * This function returns the index of receptor ring in ridx
 */
#define getridx(vuart, ridx) (ridx) = (((vuart)->ring[0].pid == los) ? 1 : 0);

/*!
 * This function returns the index of producer ring in lidx
 */
#define getlidx(vuart, lidx) (lidx) = (((vuart)->ring[0].pid == los) ? 0 : 1);


#define buf2ringdesc(desc)			\
	((RingDesc*) ((desc)->descaddr))

#define ringfull(ring, idx)						\
	((((idx) - (ring)->ireq) & (ring)->imask) == 0)

#define  ringdesc(rg, idx)			\
    (RingDesc*)((idx) * sizeof(RingDesc) + (rg)->base + (nku32_f) ptov)

#define buffervaddr(desc) \
    ((char*) ((desc)->paddr +  (unsigned int) ptov))

	static RingDesc* 
gettopdesc(NkDevRing* rg) 
{
    return ringdesc(rg, rg->iresp);
}


    char*
vuart_buffer(NkVuartBufDesc* desc, int* psize)
{
    RingDesc*	d = buf2ringdesc(desc);
    *psize = d->rsize;
    return buffervaddr(d);
}

    /*!
     * Send an event to the peer remote site.
     * \param e  : the remote event to be sent 
     */
    static void
vuart_send_revent(Event* e)
{
	/*
	 * Race condition :
	 *   let suppose we emit from primary and secondary has not
	 *   subscribed yet but is willing to subscribe.
	 */
    prolog("(%p, %p, %d, %d, %p)\n", e, e->vuart, e->irq, e->pid, e->hdl);

    e->pending = 1;
    if (e->subscribe != 0) {
	nkops.nk_xirq_trigger(e->irq, e->pid);
    }

    epilog("() pending event == %d %d \n", e->pending, e->subscribe);
}

    static void
vuart_internal_event(NkVuart vuart, void *cookie, long param)
{
    NkPhAddr	pdev;
    RingDesc*	ringdesc;
    int		i;

    pdev =  nkops.nk_dev_alloc( (int) cookie );

    xintrprolog("(%x, %p)\n", pdev, cookie);

    vuart->allocator[0].start = pdev;	
    if (!pdev) {
	warn("%s:proxy memory allocation failure\n", vuart->name);
	return;
    }

    ringdesc = vuart->descs[0];
    if (ringdesc[0].rsize != 0) {
	for (i = 0; ringdesc[i].rsize ; i++) {
	    ringdesc[i].paddr = pdev;
	    pdev += ringdesc[i].rsize ;
	}
    }

    xintrepilog("(%x, %p)\n", pdev, cookie);
}


    /* 
     * see .h for full documentation
     * the creator receive in ring 0 and emit in ring 1. So creator initializes
     * ring 1 the first time and then ring 0.
     */
    NkVuart		
vuart_create(char* name, NkVuartConfig* config)
{
    NkDevVuart*		vuart;
    NkDevDesc*		vdev;
    NkDevRing*		vring;
    nku32_f		buffersize;
    int			i;
    NkPhAddr		pdev = 0;

    prolog("(%s, [%x, %x])\n", name, config->emitRingCnt, config->bufSize);

    precond(config->emitRingCnt < MAX_RING_DESC);

	/*
	 * init local os id
	 */
    los  = nkops.nk_id_get();

	/*
	 * print tracecontrol address 
	 */
    trace(0, "%d:&tracecontrol %p\n", los, &vuart_tracecontrol);

	/*
	 * lookup device and see if it already exists
	 */
    do {
	NkVuart		vuart;

	pdev = nkops.nk_dev_lookup_by_type(NK_DEV_ID_UART, pdev) ;
	if (pdev) {
	    int		i;
	    vuart = (NkDevVuart*)nkops.nk_ptov(pdev);
	    if (ptov == 0) 
		ptov  = (void *)(((nku32_f)vuart) - ((nku32_f)pdev));

	    for (i = 0; vuart->name[i] == name[i] ; i++) {
		if (name[i] == 0) {
			/* 
			 * we got a match. So may be this device has been
			 * allocated by a peer site. In that case we should
			 * create - it.
			 */
		    int		ridx;
	
		    vring = &vuart->ring[0];

		    if (vring[1].pid != 0 && vring[1].pid != los) {
			warn("%s:Failed because the uart is owned by another site\n", vuart->name);
			return 0;
		    }

		    getridx(vuart, ridx);

		    vring[1].pid = los;
		    if (vring[0].imask != 0 && 
			vring[0].imask != config->emitRingCnt - 1) {
			    /*
			     * note this check could be removed if we can 
			     * free resources.
			     */
			warn("%s:Reinit with a different buffer geometry\n", vuart->name);
			return 0;
		    }
		    vring[0].imask = config->emitRingCnt - 1;

			/*
			 * initialize emition buffer in secondary guest os.
			 */
		    {
			RingDesc*	ringdesc;
			Event*		e;
			ringdesc = vuart->descs[0];
			if (ringdesc[0].rsize != 0 && 
			    ringdesc[0].rsize != config->bufSize) {
			    warn("%s:Reinit with a different buffer geometry\n", vuart->name);
			    return 0;
			}

			if (ringdesc[0].rsize == 0) {
			    for (i = 0; i < config->emitRingCnt ; i++) {
				ringdesc[i].rsize = config->bufSize;
			    }

			    for (; i < MAX_RING_DESC; i++) {
				ringdesc[i].rsize = 0;
			    }
			    
			    e = &vuart->events[ridx][NK_VUART_INTERNAL_EVENT];
			    e->cookie = (void*)(config->emitRingCnt * config->bufSize);
			    vuart_send_revent(e);
			}
			
			if (ringdesc[0].rsize == 0) {
			    warn("%s:Failed to alloc buffer \n", vuart->name);
			    return 0;
			}
		    }
		    return vuart;
		}
	    }
	}
    } while (pdev);

	/*
	 * no match found .
	 * perform initialization of descriptor + communicaton buffer
	 */
    buffersize = config->bufSize * config->emitRingCnt;
    pdev =  nkops.nk_dev_alloc(buffersize + sizeof(NkDevVuart));
    if (!pdev) {
	goto alloc_failure;
    }

	/*
	 * zero the vuart.
	 */
    vuart = (NkDevVuart*)nkops.nk_ptov(pdev);
    if (ptov == 0) {
	ptov  = (void *)(((nku32_f)vuart) - ((nku32_f)pdev));
    }

    memset(vuart, 0, sizeof(NkDevVuart));


	/*
	 * initialize the virtual device and add it to the data base
	 */
    vdev = &vuart->dev;
    vdev->class_id   = NK_DEV_CLASS_GEN;
    vdev->dev_id     = NK_DEV_ID_UART;
    vdev->dev_header = pdev + sizeof(NkDevDesc);

    nkops.nk_dev_add(pdev);


    vring = &vuart->ring[0];

    
	/*
	 * init name + keep configuration under the hood.
	 */
    strncpy(vuart->name, name, sizeof(vuart->name));
    vuart->config = *config;

	/*
	 * perform ring initialization
	 */
    vring[0].pid = nkops.nk_id_get();

    vring[0].dsize = vring[1].dsize = sizeof(RingDesc);
    
    vring[1].imask = config->emitRingCnt - 1;

	/* ireq - iresp are initialized to zero */
    vring[0].base = nkops.nk_vtop(vuart->descs[0]);
    vring[1].base = nkops.nk_vtop(vuart->descs[1]);

    vring[0].type = NK_DEV_ID_UART;
    vring[1].type = NK_DEV_ID_UART;


	/*
	 * initialize allocator with physical address
	 */
    vuart->allocator[1].start = sizeof(vuart[0]) + pdev ;

    {
	RingDesc* ringdesc;
	ringdesc = vuart->descs[1];
	for (i = 0; i < config->emitRingCnt ; i++) {
	    ringdesc[i].paddr = vuart->allocator[1].start + i *config->bufSize;
	    ringdesc[i].rsize = config->bufSize;
	}
    }

	/*
	 * subscribe to internal handler.
	 */
    vuart_event_subscribe(vuart, 
			  NK_VUART_INTERNAL_EVENT, 
			  vuart_internal_event, 0);

	/*
	 * we should register to sysconfig xirq to reinitialize peer driver
	 * if it goes down.
	 */

    epilog("() %p \n", vuart);
    return (NkVuart) vuart;

#ifdef LATER
rem_dev:
	// nkops.nk_dev_rem(pdev);
free_dev:
	// nkops.nk_dev_free(pdev);
#endif
alloc_failure:
#ifdef LATER
    warn("%s:Virtual uart creation failure\n", name);
#endif
    return 0;
} 

    int
vuart_delete(NkVuart vuart)
{

    prolog("(%p)\n", vuart);

    warn("Failed to delete to vuart %s (%d, %d) (local os id %d)\n",
	 vuart->name, vuart->ring[0].pid, vuart->ring[1].pid, los); 

    epilog("(%p)\n", vuart);
    return -1;
} 


    char*
vuart_name(NkVuart vuart)
{
    return vuart->name;
} 


/*!
 * This routine handles VUART event. It is connected to the cross interrupt 
 */
    void
vuart_hdl_1(void* _e, NkXIrq irq)
{
    Event*	e = (Event*) _e;
    xintrprolog("(%p, %x, %x %x)\n", _e, irq, e->pending, e->subscribe);

    if (e->pending && e->subscribe) {
	e->pending = 0;
	e->hdl(e->vuart, e->cookie, 0);
    }
    xintrepilog("(%d)\n", e->pending);
}


/*!
 * This routine handles VUART `control' event. 
 *	It is connected to the cross interrupt so it runs under interrupt.
 */
    void
vuart_hdl_2(void* cookie, NkXIrq irq)
{
    NkDevVuart*	vuart;
    Event*	_e =  (Event*) cookie; 
    Event*	e;
    int		i;

    xintrprolog("(%p, %x)\n", _e, irq);

    vuart = _e[2].vuart;

    for ( i = NK_VUART_PEER_OPEN; i < NK_VUART_EVENT_MAX; i++) {
	e = _e + i;
	if (e->pending & e->subscribe) {
	    e->pending = 0;
	    e->hdl(vuart, e->cookie, e->param);
	}
    }

    xintrepilog("() %d \n", e - _e);
}


    int		
vuart_open(NkVuart vuart)
{
    int		lidx;
    int		ridx;

    prolog("(%p)\n", vuart);

    getidx(vuart, lidx, ridx);
    vuart->opened[lidx] = 1;
    vuart_send_revent(&vuart->events[ridx][NK_VUART_PEER_OPEN]);

    epilog("(%d)\n", 0);
    return 0;

#ifdef LATER
_xirq_free:
	// nkops.xirq_free(lirq);

error:
    epilog("(%d)\n", -1);
    return -1;
#endif
}


    int
vuart_alloc(NkVuart vuart, NkVuartBufDesc * desc)
{
    NkDevRing*	ring;
    int		lidx;
    int		ridx;
    int		res;
    unsigned long flags;

    getidx(vuart, lidx, ridx);

    prolog("(%s, %p)\n", vuart->name, desc);

    precond(desc != 0 && vuart_valid(vuart) && vuart_opened(vuart, lidx));

    ring = &vuart->ring[ridx];
    
    res = -1;

	/***
	 * Critical section 2
	 * alloc for a transmission descriptor
	 * atomicity must be insure by the caller. This is 
	 * beginning a critical section between the allocator and the 
	 * vuart_free routine
	 ***/

	/* lock */
    VUART_INTR_MASK(flags);

    {
	int		tmp;
	RingDesc*	d;

	tmp = ring->ireq;
	d = ringdesc(ring, tmp);
	tmp = (tmp + 1) & ring->imask;

	if (d->size == 0) {
		/*
		 * But the descriptor is full.
		 * ireq points to the first free descriptor
		 */
	    d->size = 1;

	    desc->descaddr = d;
	    ring->ireq  = tmp;	

	    res  = 0;
	}

    }
	/* unlock */
    VUART_INTR_UNMASK(flags);

	/*
	 * end of the critical section
	 */

    epilog("(%x) descaddr == %p\n", res, desc->descaddr);
    return res;
}

    int 
vuart_transmit(NkVuart vuart, NkVuartBufDesc* bufdesc, int size)
{
    RingDesc*	d = buf2ringdesc(bufdesc);
    int		lidx;
    int		ridx;
    NkDevRing*	ring;
    
    Event*	e;
    int		ringCurSize;

    getidx(vuart, lidx, ridx);

    prolog("(%s, %p, %d)\n", vuart->name, bufdesc->descaddr, size);
    logemitbuffer(buffervaddr(d), size);

    precond(vuart_valid(vuart) && 
	    vuart_opened(vuart, lidx) && 
	    vuart_bufdesc_valid(bufdesc));

	/***
	 * Critical section 1 :
	 * Synchronisation with vuart_receive :
	 *    if empty now I should always send a cross interrupt.
	 *    this insure this is not possible to miss a packet
	 *    So that does not require a locking.
	 ***/


    ring = &vuart->ring[ridx];

    ringCurSize = ((d - ringdesc(ring, 0)) - ring->iresp) & ring->imask;


    d->size = size + 2;


    e = &vuart->events[ridx][NK_VUART_RX_READY];

	/*
	 *    if (index of the current ring - iresp ) == 1 then
	 *       I must send an interrupt
	 *       iresp is incremented by vuart_receive
	 */
    if (ringCurSize == 0) {
	vuart_send_revent(e);
    }

	/*** 
	 * End of critical section 1 
	 ***/

    epilog("(%x)\n", 0);
    return 0;
}


    int 
vuart_receive(NkVuart vuart, NkVuartBufDesc * desc)
{
    int			lidx;
    unsigned int	res;
    RingDesc*		ringDesc;
    NkDevRing*		ring;
    unsigned long	flags;
    int			tmp;
    

    getlidx(vuart, lidx);

    prolog("(%s, %p)\n", vuart->name, desc);

    precond(desc != 0 && vuart_valid(vuart) && vuart_opened(vuart, lidx));
    
    ring = vuart->ring + lidx;

    VUART_INTR_MASK(flags);

	/*
	 * advance the ires stuff
	 */
    tmp = ring->iresp;

	/*** 
	 *    Critical section 1 
	 *    This is the beginning of critical section between 
	 *    vuart_receive and vuart_transmit
	 ***/ 

    ringDesc = ringdesc(ring, tmp);

    if (ringDesc->size > 1) {
	res = ringDesc->size - 2;
	ring->iresp  = ( tmp + 1 ) & ring->imask;
    } else {
	res = -1;
    }

	/*** 
	 *    End of Critical section 1 
	 ***/

	/* unlock */
    VUART_INTR_UNMASK(flags);

    if (res != -1) {
	logrecvbuffer(buffervaddr(ringDesc), res);
    }

    desc->descaddr = ringDesc;

    epilog("(%d)\n", res);
    return res;
}

    int 
vuart_poll(NkVuart vuart, NkVuartBufDesc * desc)
{
    int		lidx;
    int		res ;
    RingDesc*	ringDesc;

    getlidx(vuart, lidx);

    prolog("(%s, %p)\n", vuart->name, desc);

    precond(desc != 0 && vuart_valid(vuart) && vuart_opened(vuart, lidx));

    ringDesc = gettopdesc(&vuart->ring[lidx]);
    desc->descaddr = ringDesc;

    res = -1;
    if (ringDesc->size > 1) {
	res = ringDesc->size - 2;
    }

    epilog("(%d)\n", res);
    return res;
}



    /*!
     * Free a descriptor previously received by the receive routine.
     */
    int 
vuart_free(NkVuart vuart, NkVuartBufDesc * _d)
{
    int		lidx;
    int		ridx;
    RingDesc*	desc;

    getidx(vuart, lidx, ridx);
    desc = buf2ringdesc(_d);

    prolog("(%s, %p)\n", vuart->name, desc);

    precond(vuart_valid(vuart) && 
	    vuart_opened(vuart, lidx) &&
	    vuart_bufdesc_valid(_d) && desc->size > 0);

	/***
	 * Critical section 2 
	 * Synchronization with the vuart_alloc routine
	 ***/

    {

	NkDevRing*	vring;
	int		full;
	RingDesc*	start;
	int		idx;

	    /*
	     * compute the index of the descriptor
	     */
	start = ringdesc(&vuart->ring[lidx], 0);
	idx = desc - start;

	vring = vuart->ring + lidx;

	full = ringfull(vring, idx);
	desc->size = 0;

	    /*
	     * if the ring was full we must notify a txready.
	     */
	if (full) {
	    Event* e = &vuart->events[ridx][NK_VUART_TX_READY];
	    vuart_send_revent(e);
	}
    }
	
    return 0;
}

    int 
vuart_event_subscribe(NkVuart			vuart, 
		      NkVuartEvent		event, 
		      NkVuartEventHandler	hdl, 
		      void*			cookie)
{
    int			lidx;
    Event*		e;		

    assert(los != 0);
    getlidx(vuart, lidx);

    precond(vuart_valid(vuart) && vuart_event_valid(event));
    prolog("(%s, %x, %p , %p)\n", vuart->name, event, hdl, cookie);

    
	/*
	 * initialize event stuff 
	 */
    e = &vuart->events[lidx][event]; 
    e->vuart = vuart;
    e->cookie = cookie;

    if (hdl) {
	e->hdl = hdl;
	e->subscribe = 1;
	e->pid = los;
    } else {
	e->subscribe = 0;
    }

	/*
	 * xirq is not registered then I register.
	 */
    if (e->irq == 0) {
	NkXIrq		lirq;
	    /*
	     * get idx
	     */
	lirq = nkops.nk_xirq_alloc(1);
	if (lirq == 0) {
	    return -1;
	}
	e->irq = lirq;

	if (event == NK_VUART_TX_READY || event == NK_VUART_RX_READY) {
	    nkops.nk_xirq_attach(lirq, vuart_hdl_1, e); 
	} else {
	    int		i;
	    nkops.nk_xirq_attach(lirq, vuart_hdl_2, &vuart->events[0]); 
	    for (i = NK_VUART_PEER_OPEN; i < NK_VUART_EVENT_MAX; i++) {
		vuart->events[lidx][i].irq = lirq;
		vuart->events[lidx][i].vuart = vuart;
	    }
	}
    }

    if (e->pending && e->subscribe) {
	vuart->events[lidx][event].pending = 0;
	vuart->events[lidx][event].hdl(vuart, cookie, 0);
    }

    epilog("(%d)\n", 0);
    return 0;
}



    int
vuart_set_param(NkVuart vuart, NkVuartParam p) 
{
    precond(vuart_valid(vuart));
    prolog("(%s, %lx)\n", vuart->name, p);

    if (vuart->param != p) {
	int		ridx;
	getridx(vuart, ridx);

	vuart->param = p;
	vuart_send_revent(&vuart->events[ridx][NK_VUART_CONFIG_EVENT]);
    }

    epilog("(%lx)\n", vuart->param);
    return 0;
}


    int
vuart_get_param(NkVuart vuart, NkVuartParam* p) 
{
    return vuart->param;
}


    int 
vuart_close(NkVuart vuart)
{
    int		lidx;
    int		ridx;

    prolog("(%s)\n", vuart->name);

    getidx(vuart, lidx, ridx);

    vuart->opened[lidx] = 0;

    if (vuart->opened[0] == vuart->opened[1]) {
	int		i;
	    /*!
	     * This is the last close
	     */
	vuart->ring[0].iresp = 0;
	vuart->ring[0].ireq  = 0;
	vuart->ring[1].iresp = 0;
	vuart->ring[1].ireq  = 0;

	    /*
	     * now reset ring buffer
	     */
	for (i = 0; i < MAX_RING_DESC; i++) {
	    vuart->descs[0][i].size = 0;
	    vuart->descs[1][i].size = 0;
	}
    }

    vuart_send_revent(&vuart->events[ridx][NK_VUART_PEER_CLOSE]);


    epilog("(%x)\n", 0);
    return 0;
}



    int
vuart_set_break(NkVuart vuart)
{
    int		ridx;
    Event*	e;

    precond(vuart_valid(vuart));
    prolog("(%s)\n", vuart->name);

    getridx(vuart, ridx);

    e = &vuart->events[ridx][NK_VUART_BREAK_EVENT];
    e->param = 1;
    vuart_send_revent(e);

    epilog("(%d)\n", 0);
    return 0;
}    


    int
vuart_clear_break(NkVuart vuart)
{
    int		ridx;
    Event*	e;

    prolog("(%s)\n", vuart->name);
    precond(vuart_valid(vuart));

    getridx(vuart, ridx);
    e = &vuart->events[ridx][NK_VUART_BREAK_EVENT];
    e->param = 0;
    vuart_send_revent(e);

    epilog("(%d)\n", 0);
    return 0;
}    

#	define INPUT_SIG \
    (NK_VUART_SIGNAL_RTS | NK_VUART_SIGNAL_DTR | NK_VUART_SIGNAL_DCD)

    int
vuart_set_ctrl(NkVuart vuart, NkVuartModemSignal f)
{
    int		lidx;
    int		ridx;
    int		newsig; /* changed signal */
    int		oldsig; /* signals before applying f */
    long*	lsigs;
    long*	rsigs;

    getidx(vuart, lidx, ridx);

    prolog("(%s)\n", vuart->name);
    precond(vuart_valid(vuart) && vuart_opened(vuart, lidx));

    lsigs = &vuart -> events[lidx][NK_VUART_MODEM_EVENT].param;
    rsigs = &vuart -> events[ridx][NK_VUART_MODEM_EVENT].param;

    f &= INPUT_SIG;			/* keep input signals */

    newsig = f ^ lsigs[0];	/* see if they change */

	/* beginning of the critical section */ 

    oldsig = rsigs[0];
    if ( newsig & NK_VUART_SIGNAL_RTS ) { 
	oldsig ^= NK_VUART_SIGNAL_CTS;  /* report as inputsig on remote site */
    }

    if ( newsig & NK_VUART_SIGNAL_DTR ) {
	oldsig ^= NK_VUART_SIGNAL_DSR;
    }

    if ( newsig & NK_VUART_SIGNAL_DCD) {
	oldsig ^= NK_VUART_SIGNAL_DCD;
    }

    rsigs[0] = oldsig;
    lsigs[0] = (lsigs[0] & (~INPUT_SIG)) | f;

	/* end of the critical section */ 

    if (newsig) {
	vuart_send_revent(&vuart->events[ridx][NK_VUART_MODEM_EVENT]);
    }

    epilog("(%d)\n", 0);
    return 0;
}    


    int
vuart_get_ctrl(NkVuart vuart, NkVuartModemSignal* sig)
{
    int		lidx;

    getlidx(vuart, lidx);

    precond(vuart_valid(vuart) && vuart_opened(vuart, lidx));
    prolog("(%s)\n", vuart->name);

    *sig = vuart -> events[lidx][NK_VUART_MODEM_EVENT].param;

    epilog("(%d)\n", 0);
    return 0;
}


#undef assert
#undef precond
#undef trace
#undef prolog
#undef epilog
#undef warn


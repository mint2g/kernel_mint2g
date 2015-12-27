/*
 ****************************************************************
 *
 * Component = Linux console driver on top of vuart
 *
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * #ident  "@(#)console.c 1.1     07/10/18 VirtualLogix"
 *
 * Contributor(s):
 *   Vladimir Grouzdev (grouzdev@jaluna.com) Jaluna SA
 *   Francois Armand (francois.armand@jaluna.com) Jaluna SA
 *   Guennadi Maslov (guennadi.maslov@jaluna.com) Jaluna SA
 *   Gilles Maigne (gilles.maigne@jaluna.com) Jaluna SA
 *   Chi Dat Truong <chidat.truong@jaluna.com>
 *   Thierry Bianco (thierry.bianco@redbend.com)
 *
 ****************************************************************
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/spinlock.h>
#include <linux/sprdmux.h>

#include <asm/uaccess.h>
#include <asm/nkern.h>
#include <nk/nkern.h>

//#define __TRACE__

#ifdef  __TRACE__
#define CON_TRACE printk
#else
#define CON_TRACE(...)
#endif

//#define CON_DEBUG

#ifdef  CON_DEBUG
#define CON_PRINT printk
#else
#define CON_PRINT(...)
#endif




/* ========================================================================= *
 * code extracted and modified from vbpipe.c - start                         *
 * ========================================================================= */
#define RING_P_ROOM(rng,size)	((size) - ((rng)->c_idx - (rng)->s_idx))
#define RING_P_CROOM(ex_dev)	((ex_dev)->c_size - (ex_dev)->c_pos)
#define RING_C_ROOM(rng)	((rng)->c_idx - (rng)->s_idx)
#define RING_C_CROOM(ex_dev)	((ex_dev)->s_size - (ex_dev)->s_pos)

//#define CONFIG_SMP
#if defined(CONFIG_SMP)
#define DMB() __asm__ __volatile__ ("dmb" : : : "memory")
#else
#define DMB()
#endif


#define MIN_RING_SIZE 16

#define VUART_MSG "VUART: "

#define WAIT_QUEUE		wait_queue_head_t

#define MIN(a, b) ((a)<(b) ? (a) : (b))

typedef struct ExRing {
    volatile nku32_f	s_idx;	/* "free running" "server" index */
    volatile nku32_f	s_wait;	/* flag: "server" might go to sleep */
    volatile nku32_f	c_idx;	/* "free running" "client" index */
    volatile nku32_f	c_wait;	/* flag: "client" might go to sleep */
    nku8_f	ring[MIN_RING_SIZE];
} ExRing;
typedef struct ExDev {
    _Bool	 enabled;	/* flag: device has all resources allocated */
    _Bool	 defined;	/* this array entry is defined */
    NkDevVlink*  s_link;	/* server link */
    ExRing*	 s_ring;	/* server circular ring */
    int		 s_size;	/* size of server circular ring */
    int		 s_pos;		/* reading position inside ring */
    NkXIrq	 s_s_xirq;	/* server xirq for server ring */
    NkXIrq	 s_c_xirq;	/* client xirq for server ring */
    NkXIrqId	 s_s_xid;	/* server cross interrupt handler id */
    NkDevVlink*  c_link;	/* client link */
    ExRing*	 c_ring;	/* client circular ring */
    int		 c_size;	/* size of client circular ring */
    int		 c_pos;		/* writing position inside ring */
    NkXIrq	 c_s_xirq;	/* server xirq for client ring */
    NkXIrq	 c_c_xirq;	/* client xirq for client ring */
    NkXIrqId	 c_c_xid;	/* client cross interrupt handler id */
    void         *cookie;       /* for tx and rx xirq */
    WAIT_QUEUE	 wait;		/* waiting queue for all ops */
    int		 count;		/* usage counter */
} ExDev;

#define EX_DEV_MAX 1
static ExDev ex_devs[EX_DEV_MAX];
static NkXIrqId ex_sysconf_id;
static NkXIrqHandler ex_rx_xirq_hdl;
static NkXIrqHandler ex_tx_xirq_hdl;



    static void
ex_ring_reset (ExDev *ex_dev, int server)
{
    if (server) {
	ex_dev->s_ring->s_idx  = 0;
	ex_dev->s_ring->s_wait = 1;
	ex_dev->s_pos          = 0;
    } else {
	ex_dev->c_ring->c_idx  = 0;
	ex_dev->c_ring->c_wait = 0;
	ex_dev->c_pos          = 0;
    }
}

    static void
ex_ring_init (ExDev *ex_dev, int server)
{
}

    static void
ex_sysconf_trigger(ExDev *ex_dev)
{
    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, ex_dev->s_link->c_id);
}

    static int
ex_handshake (ExDev *ex_dev, int server)
{
    volatile int* my_state;
    int           peer_state;

    if (server) {
	my_state   = &ex_dev->s_link->s_state;
	peer_state =  ex_dev->s_link->c_state;
    } else {
	my_state   = &ex_dev->c_link->c_state;
	peer_state =  ex_dev->c_link->s_state;
    }
    switch (*my_state) {
	case NK_DEV_VLINK_OFF:
	    if (peer_state != NK_DEV_VLINK_ON) {
		ex_ring_reset(ex_dev, server);
		*my_state = NK_DEV_VLINK_RESET;
		ex_sysconf_trigger(ex_dev);
	    }
	    break;
	case NK_DEV_VLINK_RESET:
	    if (peer_state != NK_DEV_VLINK_OFF) {
		ex_ring_init(ex_dev, server);
		*my_state = NK_DEV_VLINK_ON;
		ex_sysconf_trigger(ex_dev);
	    }
	    break;
	case NK_DEV_VLINK_ON:
	    if (peer_state == NK_DEV_VLINK_OFF) {
		*my_state = NK_DEV_VLINK_OFF;
		ex_sysconf_trigger(ex_dev);
	    }
	    break;
    }

    return (*my_state  == NK_DEV_VLINK_ON) &&
	   (peer_state == NK_DEV_VLINK_ON);
}

    static int
ex_link_ready(ExDev *ex_dev)
{
    int s_ok = ex_handshake(ex_dev, 1);
    int c_ok = ex_handshake(ex_dev, 0);

    return s_ok && c_ok;
}

    static void
ex_dev_cleanup(ExDev *ex_dev)
{
    if (ex_dev->s_s_xid != 0) {
	nkops.nk_xirq_detach(ex_dev->s_s_xid);
    }
    if (ex_dev->c_c_xid != 0) {
	nkops.nk_xirq_detach(ex_dev->c_c_xid);
    }
    ex_dev->count = 0;
    ex_dev->s_link->s_state = NK_DEV_VLINK_OFF;
    ex_dev->c_link->c_state = NK_DEV_VLINK_OFF;
    ex_sysconf_trigger(ex_dev);
}

    static int
ex_xirq_attach (ExDev *ex_dev, NkXIrqHandler hdl, int server, void *cookie)
{
    NkDevVlink* link;
    NkXIrq      xirq;
    NkXIrqId	xid;

    if (server) {
	link = ex_dev->s_link;
	xirq = ex_dev->s_s_xirq;
    } else {
	link = ex_dev->c_link;
	xirq = ex_dev->c_c_xirq;
    }

    xid = nkops.nk_xirq_attach(xirq, hdl, cookie);

    if (xid == 0) {
	return -ENOMEM;
    }

    if (server) {
	ex_dev->s_s_xid = xid;
    } else {
	ex_dev->c_c_xid = xid;
    }

    return 0;
}

    static int
ex_open (ExDev *ex_dev)
{
	/*
	 * check if we was able to allocate all resources
	 * for this device
	 */
    if (ex_dev->enabled == 0) {
	return -ENXIO;
    }
	/*
	 * Increase usage counter
	 */
    ex_dev->count += 1;

    if (ex_dev->count != 1) {
	return 0;
    }
	/*
	 * Attach cross interrupt handlers
	 */
    if ((ex_xirq_attach(ex_dev, ex_rx_xirq_hdl, 1, ex_dev->cookie) != 0) ||
	(ex_xirq_attach(ex_dev, ex_tx_xirq_hdl, 0, ex_dev->cookie) != 0)) {
	ex_dev_cleanup(ex_dev);
	return -ENOMEM;
    }
	/*
	 * perform handshake until both client and server links are ready
	 */
    if (!ex_link_ready(ex_dev)) {
	return -EAGAIN;
    }

    return 0;
}

    static int
ex_release (ExDev *ex_dev)
{
	/*
	 * Decrease usage counter
	 */
    ex_dev->count -= 1;

	/*
	 * Do clean up for the last usage
	 */
    if (ex_dev->count == 0) {
	ex_dev_cleanup(ex_dev);
    }

    return 0;
}

    static int
ex_read (ExDev *ex_dev, char *buf, int count)
{
    ExRing *sring = ex_dev->s_ring;
    int c, done;

    CON_PRINT(VUART_MSG
	      "%s : entered (ex_dev=0x%08x, buf=0x%08x, count=%d)\n", 
	      __func__, (unsigned int)ex_dev, (unsigned int)buf, count);

    sring->s_wait = 1;
    DMB();
    done = RING_C_ROOM(sring);
    if (done > count) {
	done = count;
	sring->s_wait = 0;
    }
    CON_PRINT(VUART_MSG "%s : sleep=%s\n",
	      __func__, sring->s_wait ? "yes" : "no");
    c = done;
    while (c) {
	int n = MIN(c, RING_C_CROOM(ex_dev));
	memcpy(buf, &sring->ring[ex_dev->s_pos], n);
	DMB();
	buf += n;
	sring->s_idx += n;
	c -= n;
	ex_dev->s_pos += n;
	if (ex_dev->s_pos == ex_dev->s_size)
	    ex_dev->s_pos = 0;
    }
    /* send a tx-xirq if sring was full */
    if (RING_P_ROOM(sring, ex_dev->s_size) == done && sring->c_wait == 1) {
	CON_PRINT(VUART_MSG
		  "%s : sring no longer full, sending tx-xirq\n",
		  __func__);
	nkops.nk_xirq_trigger(ex_dev->s_c_xirq, ex_dev->s_link->c_id);
    }
    CON_PRINT(VUART_MSG "%s : exited (value=%d)\n", __func__, done);
    return done;
}

    static int
ex_write (ExDev *ex_dev, const char *buf, int count)
{
    ExRing *cring = ex_dev->c_ring;
    int c, done;

    CON_PRINT(VUART_MSG
	      "%s : entered (ex_dev=0x%08x, buf=0x%08x, count=%d)\n", 
	      __func__, (unsigned int)ex_dev, (unsigned int)buf, count);

    cring->c_wait = 1;
    DMB();
    done = RING_P_ROOM(cring, ex_dev->c_size);
    if (done > count) {
	done = count;
	cring->c_wait = 0;
    }
    CON_PRINT(VUART_MSG "%s : sleep=%s\n",
	      __func__, cring->c_wait ? "yes" : "no");
    c = done;
    while (c) {
	int n = MIN(c, RING_P_CROOM(ex_dev));
	memcpy(&cring->ring[ex_dev->c_pos], buf, n);
	DMB();
	buf += n;
	cring->c_idx += n;
	c -= n;
	ex_dev->c_pos += n;
	if (ex_dev->c_pos == ex_dev->c_size)
	    ex_dev->c_pos = 0;
    }
    /* send a rx-xirq if cring was empty */
   // if (RING_C_ROOM(cring) == done && cring->s_wait == 1) {
    if (cring->s_wait == 1) {
	CON_PRINT(VUART_MSG
		  "%s : cring no longer empty, sending rx-xirq\n",
		  __func__);
	nkops.nk_xirq_trigger(ex_dev->c_s_xirq, ex_dev->c_link->s_id);
    }
    CON_PRINT(VUART_MSG "%s : exited (value=%d)\n", __func__, done);
    return done;
}

    static int
ex_dev_ring_alloc(NkDevVlink* vlink, ExRing** p_ring, int size)
{
    NkPhAddr plink;
    NkPhAddr pring;
    int      sz		= size + sizeof(ExRing) - MIN_RING_SIZE;

    plink = nkops.nk_vtop(vlink);
    pring = nkops.nk_pmem_alloc(plink, 0, sz);
    if (pring == 0) {
	return -ENOMEM;
    }

    *p_ring = nkops.nk_mem_map(pring, sz);

    return 0;
}

    static int
ex_dev_xirq_alloc(NkDevVlink* vlink, NkXIrq* p_xirq, int server)
{
    NkPhAddr plink;
    NkXIrq   xirq;

    plink = nkops.nk_vtop(vlink);

    if (server) {
	xirq = nkops.nk_pxirq_alloc(plink, 1, vlink->s_id, 1);
    } else {
	xirq = nkops.nk_pxirq_alloc(plink, 0, vlink->c_id, 1);
    }

    if (xirq == 0) {
	return -ENOMEM;
    }

    *p_xirq = xirq;
    return 0;
}

    static void
ex_dev_init (ExDev* ex_dev)
{
    NkDevVlink* slink = ex_dev->s_link;
    NkDevVlink* clink = ex_dev->c_link;

	/*
	 * Allocate communication rings for client and server links
	 */
    if ((ex_dev_ring_alloc(clink, &ex_dev->c_ring, ex_dev->c_size) != 0) ||
	(ex_dev_ring_alloc(slink, &ex_dev->s_ring, ex_dev->s_size) != 0)
       ) {
	return;
    }

	/*
	 * Allocate client/server cross interrupts
	 * for client/server links
	 */
    if ((ex_dev_xirq_alloc(clink, &ex_dev->c_c_xirq, 0) != 0) ||
	(ex_dev_xirq_alloc(clink, &ex_dev->c_s_xirq, 1) != 0) ||
	(ex_dev_xirq_alloc(slink, &ex_dev->s_c_xirq, 0) != 0) ||
	(ex_dev_xirq_alloc(slink, &ex_dev->s_s_xirq, 1) != 0)
       ) {
	return;
    }
	/*
	 * Initialize mutual exclusion lock
	 * and waiting queue
	 */
    init_waitqueue_head(&ex_dev->wait);

	/*
	 * Say this device has all resources allocated
	 */
    ex_dev->enabled = 1;
}

    static void
ex_vlink_module_cleanup (void)
{
    if (ex_sysconf_id != 0) {
        nkops.nk_xirq_detach(ex_sysconf_id);
    }
}

    static void
exdev_set_pars (ExDev* ex_dev, unsigned int size, _Bool server)
{
    if (!ex_dev) return;	/* Sizing phase */
    if (size < MIN_RING_SIZE) {
        size = MIN_RING_SIZE;
    }

    if (server) {
	ex_dev->s_size = size;
    } else {
	ex_dev->c_size = size;
    }
    ex_dev->defined = 1;
}

    static char*
ex_a2ui (char* s, unsigned int* i)
{
    unsigned int xi = 0;
    char         c  = *s;

    while (('0' <= c) && (c <= '9')) {
	xi = xi * 10 + (c - '0');
	c = *(++s);
    }

    if        (*s == 'K') {
	xi *= 1024;
	s  += 1;
    } else if (*s == 'M') {
	xi *= (1024*1024);
	s  += 1;
    }

    *i = xi;

    return s;
}

     static void
ex_syntax_error (NkDevVlink* vlink, const char* location)
{
    printk(VUART_MSG "id %d OS%d<-OS%d syntax error: %s\n",
	   vlink->link, vlink->s_id, vlink->c_id, location);
}

    static void
ex_param(NkDevVlink* vlink, ExDev* ex_dev, _Bool server)
{
    unsigned int size = MIN_RING_SIZE;
    char*        s;

#define EX_NOT_END(ch)	((ch) != ';' && (ch) != '\0')

    if (!vlink->s_info) {
	exdev_set_pars(ex_dev, size, server);
	return;
    }
    s = nkops.nk_ptov(vlink->s_info);
    if (EX_NOT_END (*s)) {
	char* e = ex_a2ui(s, &size);

	if (EX_NOT_END (*e)) {
	    ex_syntax_error (vlink, e);
	    exdev_set_pars(ex_dev, size, server);
	    return;
	}
    }
    exdev_set_pars(ex_dev, size, server);
}

    static int 
ex_init(ExDev* ex_dev, 
	NkXIrqHandler sysconf_xirq_hdl,
	NkXIrqHandler rx_xirq_hdl,
	NkXIrqHandler tx_xirq_hdl,
	void *cookie)
{
    NkPhAddr plink;
    NkDevVlink *vlink, *slink;
    int ret;
    NkOsId my_id = nkops.nk_id_get();

    plink = 0;
    while ((plink = nkops.nk_vlink_lookup("vuart", plink))) {
	vlink = nkops.nk_ptov(plink);
	if (vlink->s_id == my_id) {
	    ex_param(vlink, NULL, 1);
	    ex_dev->s_link = vlink;
	    ex_param(vlink, ex_dev, 1);
	    break;
	}
    }

    ret = 0;
    slink = ex_dev->s_link;
    plink  = 0;
    while ((plink = nkops.nk_vlink_lookup("vuart", plink))) {
	vlink = nkops.nk_ptov(plink);
	if ((vlink != slink) &&
	    (vlink->c_id == slink->s_id) &&
	    (vlink->s_id == slink->c_id) &&
	    (vlink->link == slink->link)) {
	    ex_dev->c_link = vlink;
	    ex_param(vlink, ex_dev, 0);
	    ex_dev_init(ex_dev);
	    ret = 1;
	    break;
	}
    }
        /*
	 * Report a problem if any
	 */
    if (ret == 0) {
	printnk(VUART_MSG "cannot find link for OS#%d->OS#%d link=%d\n",
		slink->s_id, slink->c_id, slink->link);
	return -ENOMEM;
    } else {
	if (ex_dev->enabled) {
	    printk(VUART_MSG
		   "device vuart%d is created for OS#%d link=%d "
		   "s_size=%d c_size=%d\n",
		   0, slink->c_id, slink->link,
		   ex_dev->s_size, ex_dev->c_size);
	}
    }
	/*
	 * Attach sysconfig handler
	 */
    ex_sysconf_id = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF, sysconf_xirq_hdl, cookie);
    if (ex_sysconf_id == 0) {
	ex_vlink_module_cleanup();
        printnk(VUART_MSG "can't attach sysconf xirq handler\n");
	return -ENOMEM;
    }

    ex_rx_xirq_hdl = rx_xirq_hdl;
    ex_tx_xirq_hdl = tx_xirq_hdl;
    ex_dev->cookie = cookie;

    return 0;
}

    static void
ex_exit(void)
{
    ex_vlink_module_cleanup();
}

/* ========================================================================= *
 * code extracted and modified from vbpipe.c - end                           *
 * ========================================================================= */



#define SERIAL_NK_NAME	  "ttyVUART"
#define SERIAL_NK_MAJOR	  221 /* GMv 254 */
#define SERIAL_NK_MINOR	  0

#define VCONS_MSG "VCONS: "

#define	NKLINE(tty)	((tty)->index)
#define	NKPORT(tty) ( (NkPort*)((tty)->driver_data))

typedef struct NkPort NkPort;
#define MAX_BUF 128

#ifdef CONFIG_NKERNEL_MUX_IO
NkPort* 	sprd_port;
wait_queue_head_t txwait;
wait_queue_head_t rxwait;
static int stopped = 0;
#endif

struct NkPort {
    struct timer_list	timer;
    unsigned int	poss;    
    char		buf[MAX_BUF];
    volatile char	stoprx;    
    volatile char	stoptx;    
    volatile char	wakeup;
    unsigned short	count;
    unsigned short	sz;
    NkOsId		id;
    int			(*write_room)(NkPort*);
    void		(*flush_input)(NkPort*);
    int			(*write)(NkPort*, const u_char* buf, int count);
    spinlock_t		lock;
    struct tty_struct*	tty;
    ExDev               *ex_dev;
};

#define MAX_PORT 1
struct NkPort vuart_tty_port[MAX_PORT];

static struct tty_driver     serial_driver;
static struct tty_operations serial_ops;
static struct tty_struct*    serial_table[MAX_PORT];
static struct ktermios*      serial_termios[MAX_PORT];
static struct ktermios*      serial_termios_locked[MAX_PORT];



/*
 * vlink related console ops
 */

    static inline int
vcons_rxfifo_count(NkPort* port)
{
    return RING_C_ROOM(port->ex_dev->s_ring);
}

    static inline int
vcons_write_room(NkPort* port)
{
    ExDev *ex_dev = port->ex_dev;
    
    return RING_P_ROOM(ex_dev->c_ring, ex_dev->c_size);
}

    static int
vcons_write(NkPort* port, const u_char *buf, int count)
{
    int				res;
    unsigned long		flags;

    spin_lock_irqsave(&port->lock, flags);
    res = ex_write(port->ex_dev, buf, count);
    if (vcons_write_room(port) > 0) {
	if (port->tty) 
	    tty_wakeup(port->tty);
    }
    spin_unlock_irqrestore(&port->lock, flags);

    return res;
}

    static void
vcons_rx_intr (NkPort* port)
{
    unsigned int size, count = 0;

    if (!port->count)
	return;

#ifdef CONFIG_NKERNEL_MUX_IO
    if (!port->stoprx) {
		wake_up_interruptible(&rxwait);
    }
#endif
}

    static void
vcons_tx_intr (NkPort* port)
{
    unsigned long flags;

    spin_lock_irqsave(&port->lock, flags);
    if (vcons_write_room(port)) {
	    /* restart tty when some space has been freed */
	if (!port->stoptx) 
	    tty_wakeup(port->tty);
    }
    spin_unlock_irqrestore(&port->lock, flags);

#ifdef CONFIG_NKERNEL_MUX_IO
	wake_up_interruptible(&txwait);
#endif
}

    static void 
vcons_intr(void* cookie, NkXIrq xirq)
{
    NkPort *port = (NkPort*)cookie;
    (void)xirq;

    vcons_rx_intr(port);
    vcons_tx_intr(port);
}

    static void
vcons_sysconf_intr(void *cookie, NkXIrq xirq)
{
    ExDev *ex_dev = ((NkPort*)cookie)->ex_dev;
    (void)xirq;
    
    if (ex_link_ready(ex_dev) && ex_dev->enabled) {
	printk(VUART_MSG "link ready\n");
	wake_up_interruptible(&ex_dev->wait);
    }
}

    static int 
vcons_lookup(struct tty_struct* tty)
{
    int res;
    NkPort *port = (NkPort*)tty->driver_data;

    port->ex_dev = &ex_devs[0];
    res = ex_init(port->ex_dev,
		  vcons_sysconf_intr,
		  vcons_intr,
		  vcons_intr,
		  port);
    CON_PRINT(VCONS_MSG "%s : exited (value=%d)\n", __func__, res);
    return res;
}

    static int
vcons_open(NkPort *port)
{
    int res;
    
    /* initialize port ops for vlink based driver */
    port->flush_input = vcons_rx_intr;
    port->write = vcons_write;
    port->write_room = vcons_write_room;
    res = ex_open(port->ex_dev);
    if (res == -EAGAIN)
	res = 0;
    CON_PRINT(VCONS_MSG "%s : exited (value=%d)\n", __func__, res);
    return res;
}

    static int
vcons_close(NkPort* port)
{
    int res;

    res = ex_release(port->ex_dev);
    port->tty = 0;
    CON_PRINT(VCONS_MSG "%s : exited (value=%d)\n", __func__, res);
    return res;
}

/*
 * Here is the implementation of the generic tty driver interface
 */

    static int
serial_write_room (struct tty_struct* tty)
{
    NkPort *port = NKPORT(tty);
    return port->write_room(port);
}

    static void
serial_send_xchar (struct tty_struct* tty, char c)
{
}

    static void
serial_throttle (struct tty_struct* tty)
{
    NKPORT(tty)->stoprx = 1;
}

    static void
serial_unthrottle (struct tty_struct* tty)
{
    NkPort *port = NKPORT(tty);
    port->stoprx = 0;
    port->flush_input(port);
}

    static int
serial_write (struct tty_struct* tty,
	      const u_char*      buf,
	      int                count)
{
    NkPort *port = NKPORT(tty);
    return port->write(port, buf, count);
}


    static int
serial_chars_in_buffer (struct tty_struct* tty)
{
    return RING_C_ROOM(NKPORT(tty)->ex_dev->c_ring);
}

    static void
serial_flush_buffer (struct tty_struct* tty)
{
    tty_wakeup(tty);
}

    static void
serial_set_termios (struct tty_struct* tty, struct ktermios* old)
{
}

    static void
serial_stop (struct tty_struct* tty)
{
    NKPORT(tty)->stoptx = 1;
}

    static void
serial_start(struct tty_struct* tty)
{
    NKPORT(tty)->stoptx = 0;
}

    static int
serial_open (struct tty_struct* tty, struct file* filp)
{
    int			line;
    NkPort* 		port;

    line = NKLINE(tty);
    port = line + vuart_tty_port;
	
    printk("serial_open tty addr = 0x%x, filp = 0x%x, port = 0x%x\n",
	   (unsigned int)tty, (unsigned int)filp, (unsigned int)port);

    if (line >= MAX_PORT) {
	return -ENODEV;
    }

    port->count++;

    if (port->count > 1) {
	return 0;
    }

    port->id		= os_ctx->id;
    tty->driver_data	= port;
    port->stoptx	= 0;
    port->stoprx	= 0;
    port->wakeup	= 0;
    port->sz		= 0;
    port->poss		= 0;
    port->tty		= tty;
    port->timer.data 	= 0;

    spin_lock_init(&port->lock);

#ifdef CONFIG_NKERNEL_MUX_IO
	sprd_port = port;
	stopped = 0;
#endif
    /*
     * console specific initialization.
     */
    return vcons_lookup(tty) || vcons_open(port);
}

    static void
serial_close (struct tty_struct* tty, struct file* filp)
{ 
    NkPort* port = (NkPort*)tty->driver_data;

    printk("serial_close tty addr = 0x%x, filp = 0x%x, port->count=0x%x\n",
	   (unsigned int)tty, (unsigned int)filp, (unsigned int)port->count);

    port->count--;
    if (port->count == 0) {
	vcons_close(port);
    }

#ifdef DEBUG
	printk("Closing ttyNK<%d>\n", tty->index);
#endif
}

    static void
serial_wait_until_sent (struct tty_struct* tty, int timeout)
{
}

#ifdef CONFIG_NKERNEL_MUX_IO
static int nk_io_write(const char *buf, size_t len)
{
	ssize_t res;
	unsigned long flags;

	if(0 == len || !sprd_port)
		return 0;

	wait_event_interruptible(txwait, vcons_write_room(sprd_port) > 0 || 1
			== stopped);
	if(1 == stopped)
		return -1;

	spin_lock_irqsave(&sprd_port->lock, flags);
	res = ex_write(sprd_port->ex_dev, buf, len);
	spin_unlock_irqrestore(&sprd_port->lock, flags);

	return res;
}

static int nk_io_read(char *buf, size_t len)
{
	ssize_t res;

	if(0 == len || !sprd_port)
		return 0;

	wait_event_interruptible(rxwait, vcons_rxfifo_count(sprd_port) > 0 ||
			1 == stopped);
	if(1 == stopped)
		return -1;

	res = ex_read(sprd_port->ex_dev, buf, len);

	return res;
}

static int nk_io_stop(int mode)
{
	stopped = 1;
	if (mode & SPRDMUX_READ) {
		wake_up_interruptible(&rxwait);
	}

	if (mode & SPRDMUX_WRITE) {
		wake_up_interruptible(&txwait);
	}
	return 0;
}

static struct sprdmux sprd_iomux = {
	.id		= 0,
	.io_read	= nk_io_read,
	.io_write	= nk_io_write,
	.io_stop	= nk_io_stop,
};
#endif

    static int __init
serial_init (void)
{
    serial_driver.owner           = THIS_MODULE;
    serial_driver.magic           = TTY_DRIVER_MAGIC;
    serial_driver.driver_name     = "tty_vuart";
    serial_driver.name            = SERIAL_NK_NAME;
    /* GMv    serial_driver.devfs_name      = SERIAL_NK_NAME; */
    serial_driver.major           = SERIAL_NK_MAJOR;
    serial_driver.minor_start     = SERIAL_NK_MINOR;
    serial_driver.num             = MAX_PORT;
    serial_driver.type            = TTY_DRIVER_TYPE_SERIAL;
    serial_driver.subtype         = SERIAL_TYPE_NORMAL;
    serial_driver.init_termios    = tty_std_termios;
    serial_driver.init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    serial_driver.flags           = TTY_DRIVER_REAL_RAW;
    kref_init(&serial_driver.kref);
    serial_driver.ttys            = serial_table;
    serial_driver.termios         = serial_termios;
    serial_driver.termios_locked  = serial_termios_locked;
    serial_driver.ops             = &serial_ops;

    serial_ops.open            	  = serial_open;
    serial_ops.close           	  = serial_close;
    serial_ops.write           	  = serial_write;
    serial_ops.write_room      	  = serial_write_room;
    serial_ops.chars_in_buffer 	  = serial_chars_in_buffer;
    serial_ops.flush_buffer    	  = serial_flush_buffer;
    serial_ops.throttle        	  = serial_throttle;
    serial_ops.unthrottle      	  = serial_unthrottle;
    serial_ops.send_xchar      	  = serial_send_xchar;
    serial_ops.set_termios     	  = serial_set_termios;
    serial_ops.stop            	  = serial_stop;
    serial_ops.start           	  = serial_start;
    serial_ops.wait_until_sent 	  = serial_wait_until_sent;

    if (tty_register_driver(&serial_driver)) {
	printk(KERN_ERR "Couldn't register tty_vuart driver\n");
    }

#ifdef CONFIG_NKERNEL_MUX_IO
	sprd_port = NULL;
	init_waitqueue_head(&txwait);
	init_waitqueue_head(&rxwait);
	sprdmux_register(&sprd_iomux);
#endif

    return 0;

}

    static void __exit
serial_fini(void)
{
    unsigned long flags;

    local_irq_save(flags);

    if (tty_unregister_driver(&serial_driver)) {
	printk(KERN_ERR "Unable to unregister tty_vuart driver\n");
    }

    local_irq_restore(flags);
}

module_init(serial_init);
module_exit(serial_fini);


/*
 ****************************************************************
 *
 *  Component: VLX virtual UART driver
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
 *    Gilles Maigne (gilles.maigne@redbend.com)
 *
 ****************************************************************
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/major.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/tty_flip.h>

#include <linux/interrupt.h>

#include <nk/nkern.h>

#include <asm/atomic.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#define FALSE 0
#define TRUE 1


#include "vuart.h"
#include "vuart.c"

#ifndef CONFIG_JALUNA_VUART_FIFO_SIZE
#define	CONFIG_JALUNA_VUART_FIFO_SIZE	1024	/* Bytes */
#endif
#ifndef	CONFIG_JALUNA_VUART_NUM
#define	CONFIG_JALUNA_VUART_NUM	4	/* Port(s) */
#endif


#define vuart_chk(A) \
	if ( (A) < 0) { printk("%s:%d:vuart failure\n",__FILE__, __LINE__); }

#define	DKI_ASSERT(x)		if (! (x) ) { printk("%s:%d:assert fails \n",__FILE__, __LINE__); }
#define assert DKI_ASSERT
#define assert2(x, ...) if (! (x) ) { printk("%s:%d:assert fails \n",__FILE__, __LINE__); printk(__VA_ARGS__); }

#define VUART_DEV_NAME		"bustty"
#define VUART_DEVFS_NAME	"bustty"
#define VUART_FIFO_SIZE  	(CONFIG_JALUNA_VUART_FIFO_SIZE)
#define VUART_CTRL_FIFO_SIZE  	32
#define VUART_DRV_NAME 	"jaluna:nkdki-generic-uart"

#define VUART_NAME		"bustty"
#define VUART_MAJOR		252
#define VUART_MINOR		0

#define VUART_NUM	  	(CONFIG_JALUNA_VUART_NUM)
				/* temporary Tx buffer size / threshold */
#define VUART_TMP_SIZE		(VUART_FIFO_SIZE / 2)
#define VUART_TMP_THRE		(VUART_TMP_SIZE  / 2)

#ifdef DBG_DRIVER
    int				tracecontrol;
#define DBG(A) A
#define logedata(A, B)		if (tracecontrol & 1) printk("2>");_dumparray(A, B)
#define logrdata(A, B)		if (tracecontrol & 2) printk("2<");_dumparray(A, B)
#define trace(A, ...)		if (tracecontrol & 4) printk(A, ##__VA_ARGS__)
#define prolog(A, ...)		if (tracecontrol & 8) printk(">%s:%d: %s" A , __FILE__, __LINE__, __func__, ## __VA_ARGS__)
#define warn(A, ...)		if (tracecontrol & 16) printk("%s:warning" A ,__FILE__, ## __VA_ARGS__)
#define epilog(A, ...)		if (tracecontrol & 8) printk("<%s:%d: %s" A , __FILE__, __LINE__, __func__, ## __VA_ARGS__)


#else
#define DBG(A)
#define trace(A, ...)
#define warn(A,...)
#define prolog(A,...)
#define epilog(A,...)
#define logedata(A, B)
#define logrdata(A, B)
#endif


    /*
     * Port status
     */
#define STATUS_TX_STOPPED	0x02	/* TTY stopped/started    */

    /*
     * Types...
     */

    /*
     * Tty instance descriptor.
     */
typedef struct TtyDev {
    NkVuart			vuart;
    char			path[8];	/* node path */
    nku8_f			site;		/* remote site number */
    nku8_f			status;		/* port status */

    nku32_f			use_count; /* open # */
    char			tmp_buf[VUART_TMP_SIZE];
	/* temporary buffer */
    nku32_f			tmp_sz;     /* current size of tmp_buf */
    struct tty_struct*		tty_struct;    /*value of tty_struct pointer */
    struct tasklet_struct	tx_tasklet;    /* software interrupt handler */
    struct tasklet_struct	rx_tasklet;    /* software interrupt handler */
    int				rx_bytes;	/* number of Tx/Rx bytes */
    int				rx_overrun;	/* number of Tx/Rx overrun */
    int				rx_dropped;	/* number of Tx/Rx dropped */

    int				tx_bytes;	/* number of Tx/Tx bytes */
    int				tx_overrun;	/* number of Tx/Tx bytes */
    int				tx_dropped;	/* number of Tx/Rx bytes */
    int				brk;
    int				dsr;
    int				cts;
    int				dcd;
    nku8_f			signals;
} TtyDev;


#define TTYDEV(x)  	((TtyDev*)(x))

/*
 * Forward declarations...
 */

static void __flush_chars (TtyDev* tty_dev);
static void  _stop (struct tty_struct *tty);
static void  _start (struct tty_struct *tty);

#ifdef	CONFIG_JALUNA_VUART_CONSOLE	
static void _console_init (void);
#endif

/*
 * Global data...
 */

static TtyDev  ttys[VUART_NUM];
    /*
     * used to dump message
     */
#ifdef LATER
    static void
dumparray(unsigned char* b, int size)
{
    static char ch[17] = "0123456789abcdef";
    static char line[80];
    int		i, j;
    i = 0; j = 0;
    while ( i < size ) {
	line[j] = ch[b[i] >> 4];
	line[j + 1] = ch[b[i] & 0xf];
	line[j + 2] = ' ';
	j += 3;
	i++;
	if (j >= sizeof(line) - 5) {
	    line[ j ] = '\n';
	    line[ j + 1] = 0;
	    printk("%s", line);
	    j = 0;
	}
    }

    line[ j ] = '\n';
    line[ j + 1 ] = 0;
    printk("%s", line);
}
#endif


    /*
     * handler of cross interrupt - this handler is called when
     * the remote site has freed buffer so emission can be restarted
     * this routine is invoked from interrupt
     */
    static void
_tx_ready (NkVuart vuart, void* cookie, long param)
{
    TtyDev*  tty_dev = TTYDEV(cookie);

    prolog("(vuart == %p, cookie == %p, param == %ld)\n",vuart, cookie, param);

    assert(tty_dev && tty_dev->path[0] == 'v');

        /*
	 * This is called from interrupt
	 * Flush pending characters from temporary buffer
	 */
    __flush_chars(tty_dev);

        /*
	 * Wake up any asleep writers ... from a tasklet
	 */
    if (tty_dev->tty_struct) {
	tasklet_schedule(&tty_dev->tx_tasklet);
    }
}


    /*
     * event handler for handling the break event.
     */
    static void 
_break_event(NkVuart vuart, void* cookie, long param)
{
    TtyDev*  tty_dev = TTYDEV(cookie);
    prolog("(vuart==%p, cookie==%p, param==%ld)\n", vuart, cookie, param);
    tty_dev->brk++;
}

    /*
     * Copy current frame from the channel FIFO to the TTY input buffer.
     * We know at that point that there is enough space in the flip
     * buffer !
     */
    static inline void
rx_receive_chars (struct tty_struct * tty, char* addr, int size)
{
    logrdata(addr, size);

    while (size--) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
	unsigned char ch = *addr++;
	tty_insert_flip_char(tty, ch, TTY_NORMAL);
#else
	tty->flip.count++;
	*tty->flip.flag_buf_ptr++ = TTY_NORMAL;
	*tty->flip.char_buf_ptr++ = *addr++;
#endif
    }
}

    /*
     * Copy current frame from the channel FIFO to the TTY input buffer.
     */

#define TTY_FLIPBUF_FREE(tty) (TTY_FLIPBUF_SIZE - tty->flip.count)

    static inline int
rx_receive (TtyDev* t,  NkVuartBufDesc* frame, int esize)
{
    size_t	size;
    char*	buffer;

	/*
	 * fetch buffer from frame descriptor
	 */
    buffer = vuart_buffer(frame, &size);
    assert2(esize <= size, "size %d esize %d \n", esize, size);

        /*
	 * Check for enough space
	 */
	
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
    if ( tty_buffer_request_room(t->tty_struct, size) == 0) {
	return FALSE;
    }
#else
    if (size > TTY_FLIPBUF_FREE(t->tty_struct)) {
	return FALSE;
    }
#endif

    t->rx_bytes  += esize;
    
    rx_receive_chars(t->tty_struct, buffer, esize);

    return TRUE;
}


    /*
     * Rx handler is called by the uart driver when FALSE has been
     * previously returned from receive() and then a new frame has been
     * received.
     */
    static void
_rx_ready (NkVuart vuart, void* cookie, long param)
{
    TtyDev* tty = TTYDEV(cookie);

    prolog("(vuart==%p, cookie==%p, param==%lx)\n", vuart, cookie, param);

        /*
	 * Schedule Rx tasklet
	 */
    if (tty->use_count) 
	tasklet_schedule(&tty->rx_tasklet);
}

    /*
     * This handler is invoked when a modem event has been changed.
     */
    static inline void
_modem_event (NkVuart vuart, void* cookie, long p)
{
    int		signals = (int) p;
    TtyDev*	tty_dev = TTYDEV(cookie);
    nku8_f	new = tty_dev->signals ^ signals;

    prolog("(vuart==%p, cookie==%p, param==%ld)\n", vuart, cookie, p);

    if (!new) {
	return;
    }

    if (new & NK_VUART_SIGNAL_CTS) {
	tty_dev->cts++;
    }
    if (new & NK_VUART_SIGNAL_DSR) {
	tty_dev->dsr++;
    }

    if (new & NK_VUART_SIGNAL_DCD) {
	tty_dev->dcd++;

	    /* I should send a HANGUP */
#ifdef L
	if (signals & NK_VUART_SIGNAL_DCD) {
		/* wakeup process waiting for open */
	} else {
		/* send HANGUP to process */
	}
#endif

    }

        /*
	 * TX_RTS is connected to RX_CTS
	 */
    if (new & NK_VUART_SIGNAL_CTS) {
	struct tty_struct* tty     = tty_dev->tty_struct;
	char               xchar;

            /*
	     * Flow control 
	     *   CTS high must be stopped
	     *       low  ok to send.
	     */
	if (tty) {
	    if ( (signals & NK_VUART_SIGNAL_CTS) != 0) {
		_start(tty);
		if (I_IXOFF(tty)) {
		    xchar = START_CHAR(tty);
		    rx_receive_chars(tty_dev->tty_struct, &xchar, 1);
		}
	    } else {
		_stop(tty);
		if (tty && I_IXOFF(tty)) {
		    xchar = STOP_CHAR(tty);
		    rx_receive_chars(tty_dev->tty_struct, &xchar, 1);
		}
	    }
	}
    }

        /*
	 * Update channel signals
	 */
    tty_dev->signals = signals;
}



/****************************************************************
 *								*
 * 			TTY TTY driver				*
 *								*
 ****************************************************************/

static struct tty_driver  _tty_driver;
static struct tty_struct* _table[VUART_NUM];

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static struct termios*    _termios[VUART_NUM];
static struct termios*    _termios_locked[VUART_NUM];
#else
static struct ktermios*   _termios[VUART_NUM];
static struct ktermios*   _termios_locked[VUART_NUM];
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define	LINE(tty)   (MINOR((tty)->device) - (tty)->driver.minor_start)
#else
#define	LINE(tty)   ((tty)->index)
#endif

    /*
     * Rx tasklet read incoming frames and push them upstream.
     * scheduled from rx_handler() or _unthrottle()
     */
    static void
_rx_tasklet_action (unsigned long data)
{
    TtyDev*            tty_dev = TTYDEV(data);
    NkVuart	       vuart;
    struct tty_struct* tty     = tty_dev->tty_struct;
    NkVuartBufDesc		frame;
    int				esize;

    vuart = tty_dev->vuart;
    prolog("(vuart == %p, data == %lx)\n", vuart, data);


    if (tty) {
	    /*
	     * RTS signal == 0 => cannot send
	     */
	if ( (tty_dev->signals & NK_VUART_SIGNAL_RTS) == 0) {
	    return;
	}

    
	while ( (esize = vuart_poll(vuart, &frame) ) >= 0) {
	    if (rx_receive(tty_dev, &frame, esize)) {
		vuart_receive(vuart, &frame);
		vuart_free(vuart, &frame);
	    } else {
		tty_flip_buffer_push(tty);
		if ((tty_dev->signals & NK_VUART_SIGNAL_RTS) == 0) {
		    return;
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
		if (tty->flip.count) {
		    if (tty_dev->use_count) 
			tasklet_schedule(&tty_dev->rx_tasklet);
		    return;
		}
#endif
	    }
	}
	
	tty_flip_buffer_push(tty);
    }
}

    /*
     * Tx tasklet.
     * Wake up writers to the TTY that were asleep by _write()
     * returning LESS than required count.
     */
    static void
_tx_tasklet_action (unsigned long data)
{
    TtyDev*            tty_dev = TTYDEV(data);
    struct tty_struct* tty     = tty_dev->tty_struct;
    
    if (tty) {
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup) {
	    tty->ldisc.write_wakeup(tty);
	}
	wake_up_interruptible(&tty->write_wait);
    }
}

    static int
_open (struct tty_struct* tty, struct file* filp)
{
    int		line = LINE(tty);
    TtyDev*	tty_dev;
    NkVuart	vuart;

    if (line >= VUART_NUM) {
	warn("Could not open tty num %d \n", line);
	return -ENODEV;
    }


    prolog("(tty = %p(%d), filp == 0x%p)\n", tty, line, filp);

    tty_dev = &ttys[line];
    vuart = tty_dev->vuart;

    if (vuart == 0) {
	return -ENODEV;
    }

    if (tty_dev->use_count++) {
	return 0;
    }

	// MOD_INC_USE_COUNT;

    tty_dev->status = 0;

        /*
	 * Initialize tasklet to wakeup TTY writers.
	 */
    tasklet_init(&tty_dev->tx_tasklet, _tx_tasklet_action,
		 (unsigned long)tty_dev);
        /*
	 * Initialize tasklet to push frames up-stream
	 */
    tasklet_init(&tty_dev->rx_tasklet, _rx_tasklet_action,
		 (unsigned long)tty_dev);

        /*
	 * open vuart to the TTY device instance.
	 */
    if (vuart_open(vuart) < 0) {
	return -ENODEV;
    }

        /*
	 * First open on the device
	 */
    tty_dev->tty_struct = tty;
    tty->driver_data    = tty_dev;
    tty->low_latency    = 1;

        /*
	 * Emulate control signal: set DTR and RTS
	 */
    tty_dev->signals |= NK_VUART_SIGNAL_DTR | NK_VUART_SIGNAL_RTS;
    vuart_set_ctrl(tty_dev->vuart, tty_dev->signals);

    return 0;
}

    static void
_close (struct tty_struct * tty, struct file * filp)
{
    TtyDev* tty_dev;

    if (tty->driver_data == 0) {
	return;
    }

    tty_dev = TTYDEV(tty->driver_data);
    prolog("(tty = %p, filp == %p)\n", tty, filp);

    if (tty_dev == 0 || tty_dev->vuart == 0) {
	return;
    }

    if (--tty_dev->use_count) {
	return;
    }
	/*
	 * Last close ...
	 */
    tty_dev->tty_struct = NULL;

        /*
	 * Clear DTR and RTS if hangup-on-close is set for the TTY.
	 */
    if (tty->termios->c_cflag & HUPCL) {
	tty_dev->signals &= ~(NK_VUART_SIGNAL_DTR | 
			      NK_VUART_SIGNAL_RTS);
	vuart_set_ctrl(tty_dev->vuart, tty_dev->signals );
    }

        /*
	 * Kill Tx/Rx tasklets
	 */
    tasklet_kill(&tty_dev->tx_tasklet);
    tasklet_kill(&tty_dev->rx_tasklet);

    vuart_close(tty_dev->vuart);
	// MOD_DEC_USE_COUNT;

    epilog("(tty = %p)\n", tty);
}


#define TX_READY(tty_dev) \
    (!(tty_dev->status & STATUS_TX_STOPPED))

    static inline int
_write2 (TtyDev* tty_dev, int from_user,
	 const unsigned char *buf, int count)
{
    NkVuartBufDesc	frame;
    NkVuart		vuart = tty_dev->vuart;
    int			cnt;

    if (!count) {
	return count;
    }

    cnt = 0;

    if (!TX_READY(tty_dev)) {
	prolog("(tty_dev = %p)\n", tty_dev);
	/* caller should be asleep */
	return 0;
    }
    prolog("(tty_dev = %p)\n", tty_dev);


    if (from_user && access_ok(VERIFY_READ, buf, count)) {
	printk(KERN_ERR "%s: unreadable user buffer\n", tty_dev->path);
	tty_dev->tx_dropped += count;
	return -EINVAL;
    }

    do {
	int	sz;
	int	esize;
	char*	b;

	if (vuart_alloc(vuart, &frame) < 0) {
		/* caller should be asleep */
	    return cnt;
	}
	b = vuart_buffer(&frame, &sz);

	if (sz >= count ) {
	    esize = count;
	} else {
	    esize = sz;
	}

	if (from_user) {
	    int res;
	    res = __copy_from_user(b, buf + cnt, esize);
	    DKI_ASSERT (res == 0);
	} else {
	    memcpy(b, buf + cnt, esize);
	}

	logedata(b, esize);
	tty_dev->tx_bytes += esize;
	vuart_chk(vuart_transmit(vuart, &frame, esize));
	cnt += esize;
	count -= esize;
    } while(count > 0);

    epilog("(cnt == %x)\n", cnt);
    return cnt;
}

    static int
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
_write (struct tty_struct * tty,
	      const unsigned char *buf, int count)
{
    int from_user = 0;
#else
_write (struct tty_struct * tty, int from_user,
	      const unsigned char *buf, int count)
{
#endif
    
    TtyDev* tty_dev = TTYDEV(tty->driver_data);

    return _write2(tty_dev, from_user, buf, count);
}

    static void
_put_char (struct tty_struct *tty, unsigned char ch)
{
    TtyDev*			tty_dev = TTYDEV(tty->driver_data);
    nku16_f			size    = tty_dev->tmp_sz;
    NkVuartBufDesc		frame;
    NkVuart			vuart = tty_dev->vuart;

    prolog("(tty_dev = %p, ch == %d )\n", tty_dev, ch);

    if (size < VUART_TMP_SIZE) {
	tty_dev->tmp_buf[size++] = ch;
    } else {
	tty_dev->tx_dropped++;
    }

    if (TX_READY(tty_dev) && (size >= VUART_TMP_THRE)) {
	if (vuart_alloc(vuart, &frame) == 0) {
	    int		sz;
	    char*	b;
	    b = vuart_buffer(&frame, &sz);
	    assert(sz >= size);
	    memcpy(b, tty_dev->tmp_buf, size);
	    tty_dev->tx_bytes += size;
	    logedata(tty_dev->tmp_buf, size);
	    vuart_chk(vuart_transmit(vuart, &frame, size)); /* check it */
	    size = 0;
	}
    }

    tty_dev->tmp_sz = size;
    epilog("(%d)\n", size);
}

    static void
__flush_chars (TtyDev* tty_dev)
{
    nku8_f     size    = tty_dev->tmp_sz;
    NkVuartBufDesc frame;

    prolog("(tty_dev == %p)\n", tty_dev);
    if (!TX_READY(tty_dev) || !size) {
	return;
    }

    if ( vuart_alloc(tty_dev->vuart, &frame) == 0 ) {
	int		sz;
	char*	b;

	b = vuart_buffer(&frame, &sz);
	assert(sz >= size);
	memcpy(b, tty_dev->tmp_buf, size);

	tty_dev->tx_bytes += size;
	logedata(tty_dev->tmp_buf, size);
	vuart_chk(vuart_transmit(tty_dev->vuart, &frame, size));
	tty_dev->tmp_sz = 0;
    }
}

    static void
_flush_chars (struct tty_struct *tty)
{
    TtyDev* tty_dev = TTYDEV(tty->driver_data);

    __flush_chars(tty_dev);
}

    static int
_write_room (struct tty_struct *tty)
{
    TtyDev* tty_dev = TTYDEV(tty->driver_data);
    prolog("(tty_dev = %p)\n", tty_dev);

    if (TX_READY(tty_dev)) {
	return (VUART_TMP_SIZE - tty_dev->tmp_sz);
    } else {
	return 0;
    }
}

    static int
_chars_in_buffer (struct tty_struct *tty)
{
    TtyDev* tty_dev = TTYDEV(tty->driver_data);

    prolog("(tty_dev = %p, %d)\n", tty_dev, tty_dev->tmp_sz);

    return tty_dev->tmp_sz;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static int _tiocmset(struct tty_struct *tty, struct file *file,
		     unsigned int set, unsigned int clear)
{
	nku8_f		signals;
	TtyDev*		tty_dev = TTYDEV(tty->driver_data);
	prolog("(tty_dev = %p, file == %p set == %x clea == %x)\n", 
	       tty_dev, file, set, clear);
	
	signals = tty_dev->signals;
	
	if (set & TIOCM_RTS) {
	    signals |= NK_VUART_SIGNAL_RTS;
	}
	if (set & TIOCM_DTR) {
	    signals |= NK_VUART_SIGNAL_DTR;
	}
	if (clear & TIOCM_RTS) {
	    signals &= ~NK_VUART_SIGNAL_RTS;
	}
	if (clear & TIOCM_DTR) {
	    signals &= ~NK_VUART_SIGNAL_DTR;
	}
	tty_dev->signals = signals;

	if (tty_dev->vuart)
	    vuart_chk(vuart_set_ctrl(tty_dev->vuart, tty_dev->signals));

	return 0;
}

    static int
_tiocmget (struct tty_struct* tty,
	   struct file*       file)
{
	int		val = 0;
	nku8_f		signals;
	TtyDev*		tty_dev = TTYDEV(tty->driver_data);
	prolog("(tty_dev = %p, file == %p)\n", tty_dev, file);


	signals = tty_dev->signals;
	if (signals & NK_VUART_SIGNAL_CTS) {
	    val |= TIOCM_CTS;
	}
	if (signals & NK_VUART_SIGNAL_DSR) {
	    val |= TIOCM_DSR;
	}
	if (signals & NK_VUART_SIGNAL_DCD) {
	    val |= TIOCM_CAR;
	}

	epilog("(val = %d)\n", val);
	return val;
}
#endif

    static int
_ioctl (struct tty_struct* tty,
	      struct file*       file,
	      unsigned int       cmd,
	      unsigned long      arg)
{
    TtyDev*  tty_dev = TTYDEV(tty->driver_data);
    int      ret     = -ENOIOCTLCMD;

    prolog("(tty_dev = %p, file == %p, cmd == 0x%x, arg == 0x%lx)\n", 
	   tty_dev, file, cmd, arg);

    switch (cmd) {

    case TIOCGICOUNT: {
	struct serial_icounter_struct sicount;

	memset(&sicount, 0, sizeof(sicount));

	    sicount.cts         = tty_dev->cts;
	    sicount.dsr         = tty_dev->dsr;
		/* sicount.rng         = tty_dev->ri;*/
	    sicount.dcd         = tty_dev->dcd;
	    sicount.brk         = tty_dev->brk;

	    sicount.rx          = tty_dev->rx_bytes;
	    sicount.buf_overrun = tty_dev->rx_overrun;

	    sicount.tx          = tty_dev->tx_bytes;

	ret = copy_to_user((struct serial_icounter_struct *)arg,
			   &sicount, sizeof(sicount));
	if (ret) {
	    ret = -EFAULT;
	}
	break;
    }
	

    default:
	trace("%s can not handle 0x%x command arg 0x%lx\n", __func__,cmd, arg);
    }

    return ret;
}

    static void
_throttle (struct tty_struct * tty)
{
    TtyDev*  tty_dev = TTYDEV(tty->driver_data);

    prolog("(tty_dev = %p)\n", tty_dev);

	/*
	 * clear RTS slow down 
	 */
    tty_dev->signals &= ~NK_VUART_SIGNAL_RTS;

    if (I_IXOFF(tty) || (tty->termios->c_cflag & CRTSCTS)) {
	vuart_chk(vuart_set_ctrl(tty_dev->vuart, tty_dev->signals));
    }
}

    static void
_unthrottle (struct tty_struct * tty)
{
    TtyDev*  tty_dev = TTYDEV(tty->driver_data);
    prolog("(tty_dev = %p)\n", tty_dev);

    tty_dev->signals |= NK_VUART_SIGNAL_RTS;

        /*
	 * Schedule Rx tasklet
	 */
    if (tty_dev->use_count) 
	tasklet_schedule(&tty_dev->rx_tasklet);

    if (I_IXOFF(tty) || (tty->termios->c_cflag & CRTSCTS)) {
	vuart_chk(vuart_set_ctrl(tty_dev->vuart, tty_dev->signals));
    }
}

    static void
_stop (struct tty_struct *tty)
{
    TtyDev* tty_dev = TTYDEV(tty->driver_data);
    
    prolog("(tty_dev = %p)\n", tty_dev);

    if (tty_dev->status & STATUS_TX_STOPPED) {
	return;
    }

    tty_dev->status |= STATUS_TX_STOPPED;
}

    static void
_start (struct tty_struct *tty)
{
    TtyDev* tty_dev = TTYDEV(tty->driver_data);

    prolog("(tty_dev = %p)\n", tty_dev);
    

    if (! (tty_dev->status & STATUS_TX_STOPPED)) {
	return;
    }

    tty_dev->status &= ~STATUS_TX_STOPPED;

        /*
	 * Wake up writers, if any
	 */
    _tx_ready(tty_dev->vuart, tty_dev, 0);
}


    static void
_break_ctl (struct tty_struct *tty, int state)
{
    TtyDev*  tty_dev = TTYDEV(tty->driver_data);
    prolog("(tty_dev == %p, state == %x)\n", tty_dev, state);
    if (state) {
	vuart_chk(vuart_set_break(tty_dev->vuart));
    } else {
	vuart_chk(vuart_clear_break(tty_dev->vuart));
    }
}

    static void
_flush_buffer (struct tty_struct *tty)
{
    TtyDev*  tty_dev = TTYDEV(tty->driver_data);
    tty_dev->tmp_sz = 0;
}

#ifdef	CONFIG_PROC_FS

#define VUART_PROC_LINE_FMT \
    "bustty%d: tx:%d(%d,%d) rx:%d(%d,%d) brk:%d signals:%s.%s.%s.%s.%s.%s\n"

    static int
_info(char *buf, TtyDev* tty)
{
    int ret;

    if (!tty->tty_struct)
	return 0;

    ret = sprintf(buf, VUART_PROC_LINE_FMT,
		  tty - ttys,
		  (tty->tx_bytes),
		  (tty->tx_dropped),
		  (tty->tx_overrun),
		  tty->rx_bytes,
		  tty->rx_dropped,
		  tty->rx_overrun,
		  tty->brk,
		  (tty->signals & NK_VUART_SIGNAL_RTS) ? "RTS" : "rts",
		  (tty->signals & NK_VUART_SIGNAL_CTS) ? "CTS" : "cts",
		  (tty->signals & NK_VUART_SIGNAL_DTR) ? "DTR" : "dtr",
		  (tty->signals & NK_VUART_SIGNAL_DSR) ? "DSR" : "dsr",
		  (tty->signals & NK_VUART_SIGNAL_DCD) ? "DCD" : "dcd",
		    "ri" /*(tty->signals & NK_VUART_SIGNAL_RI) ? "RI" : "ri"*/
	);
    return ret;
}

    static int
_read_proc (char *page, char **start, off_t off,
		  int count, int *eof, void *data)
{
    struct tty_driver *ttydrv = data;
    int                i, len = 0;
    off_t              begin = 0;

    len += sprintf(page, "bustty ports info [%d-%d]\n",
		   ttydrv->minor_start, ttydrv->num - 1);
    for (i = 0 ; (i < VUART_NUM) && (len + 80 <= count) ; i++) {
	len += _info(page + len, ttys + i);
	if (len + begin > off + count)
	    goto done;
	if (len + begin < off) {
	    begin += len;
	    len = 0;
	}
    }
    *eof = 1;
 done:
    if (off >= len + begin)
	return 0;
    *start = page + (off - begin);
    return (count < begin + len - off) ? count : (begin + len - off);
}

#endif	/* CONFIG_PROC_FS */

    static void 
_init (void)
{
    static char done;
    prolog("()\n");

    if (done == 0) { done = 1; }
    else return;

    _tty_driver.magic           = TTY_DRIVER_MAGIC;
    _tty_driver.driver_name     = VUART_NAME;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) \
 && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13))
    _tty_driver.devfs_name      = VUART_DEVFS_NAME;
#endif
    _tty_driver.name            = VUART_DEV_NAME;
    _tty_driver.major           = VUART_MAJOR;
    _tty_driver.minor_start     = VUART_MINOR;
    _tty_driver.num             = VUART_NUM;
    _tty_driver.type            = TTY_DRIVER_TYPE_SERIAL;
    _tty_driver.subtype         = SERIAL_TYPE_NORMAL;
    _tty_driver.init_termios    = tty_std_termios;
    _tty_driver.init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    _tty_driver.flags           = TTY_DRIVER_REAL_RAW;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    _tty_driver.ttys            = _table;
#else
    _tty_driver.table           = _table;
#endif
    _tty_driver.termios         = _termios;
    _tty_driver.termios_locked  = _termios_locked;
    _tty_driver.open            = _open;
    _tty_driver.close           = _close;
    _tty_driver.write           = _write;
    _tty_driver.put_char        = _put_char;
    _tty_driver.flush_chars     = _flush_chars;
    _tty_driver.write_room      = _write_room;
    _tty_driver.chars_in_buffer = _chars_in_buffer;
    _tty_driver.ioctl           = _ioctl;
    _tty_driver.throttle        = _throttle;
    _tty_driver.unthrottle      = _unthrottle;
    _tty_driver.stop            = _stop;
    _tty_driver.start           = _start;
    _tty_driver.break_ctl       = _break_ctl;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    _tty_driver.tiocmget	= _tiocmget;
    _tty_driver.tiocmset	= _tiocmset;
#endif
    _tty_driver.flush_buffer    = _flush_buffer;
#ifdef	CONFIG_PROC_FS
    _tty_driver.read_proc       = _read_proc;
#endif
    if (tty_register_driver(&_tty_driver)) {
    	printk(KERN_ERR "Couldn't register bustty tty driver\n");
    }
}

    static void
_fini(void)
{
    unsigned long flags;

    prolog("()\n");

    local_irq_save(flags);

    if (tty_unregister_driver(&_tty_driver)) {
	printk(KERN_ERR "Unable to unregister bustty tty driver\n");
    }

    local_irq_restore(flags);
}

/****************************************************************
 *								*
 * 			BUSTTY console driver			*
 *								*
 ****************************************************************/

#ifdef	CONFIG_JALUNA_VUART_CONSOLE

    static int
_console_setup (struct console* c, char* options)
{
    prolog("()\n");

    if (c->index >= VUART_NUM) {
	c->index = 0;
    }
    return 1;
}

    static void
_console_write (struct console* c, const char* buf, unsigned int size)
{
    TtyDev* tty_dev = ttys + c->index;

    DKI_ASSERT (c->index < VUART_NUM);

    (void)_write2(tty_dev, 0, buf, size);
}

    static kdev_t
_console_device (struct console *c)
{
    DKI_ASSERT (c->index < VUART_NUM);

    return MKDEV(VUART_MAJOR, c->index);
}

static struct console _cons =
{
	name:	VUART_NAME,
	write:	_console_write,
	device:	_console_device,
	setup:	_console_setup,
	flags:	CON_ENABLED,
	index:	-1,
};

    static void
_console_init (void)
{
    static int console_registered = 0;

    if (! console_registered) {
	register_console(&_cons);
	console_registered = 1;
    }
}

#endif	/* CONFIG_JALUNA_VUART_CONSOLE */

    static void
_drv_init (void)
{
    DBG(printk("&tracecontrol : %p\n", &tracecontrol));

	/* should search new device in registry */

    do {
	char		name[8];
	NkVuart		vuart;
	int		id;
	NkVuartConfig	config = { 512, 4 };
	name[0] = 'v'; name[1] = 'u'; name[2] = 'a'; 
	name[3] = 'r'; name[4] = 't'; name[6] =  0;

	for (id = 0; id < VUART_NUM; id++) {
	    name[5] = '0' + id;
	    vuart = vuart_create(name, &config);
	    if (vuart) {
		TtyDev*	tty_dev = ttys + id;

		DBG(printk("Creating uart[%p] %s%d \n", vuart, name, id));
		memset(tty_dev, 0, sizeof(tty_dev[0]));
		memcpy(tty_dev->path, name, 7);
		tty_dev->vuart = vuart;
		tty_dev->site = -1;

		/*
		 * other field are initialized tty_dev open time.
		 */

#ifdef O    
		vuart_event_subscribe(vuart, NK_VUART_PEER_OPEN, 
				      _peeropen_hdl, tty_dev);

		vuart_event_subscribe(vuart, NK_VUART_PEER_CLOSE, 
				      _peerclose_hdl, tty_dev);
#endif
		
		vuart_chk(vuart_event_subscribe(vuart, 
						NK_VUART_RX_READY, 
						_rx_ready, 
						tty_dev));

		vuart_chk(vuart_event_subscribe(vuart, 
						NK_VUART_TX_READY, 
						_tx_ready,
						tty_dev));
		
		vuart_chk(vuart_event_subscribe(vuart, 
						NK_VUART_MODEM_EVENT, 
						_modem_event, 
						tty_dev));
		
		vuart_chk(vuart_event_subscribe(vuart, 
						NK_VUART_BREAK_EVENT, 
						_break_event, 
						tty_dev));
		
		
	    }
	    _init();
	}
	
    } while(0);
}

    void
_drv_fini(void)
{
    _fini();
}

    static int
vuart_module_init (void)
{
    _drv_init();
    return 0;
}

    static void
vuart_module_exit (void)
{
    _drv_fini();
}

module_init(vuart_module_init);
module_exit(vuart_module_exit);
MODULE_LICENSE("GPL");

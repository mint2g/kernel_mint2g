/*
 ****************************************************************
 *
 *  Component: VLX cross-interrupt performance benchmark driver
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
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/kernel.h>

#include <nk/nkern.h>

#define XIRQB_TRACE(h...)	printk(h)
#if 0
#define XIRQB_DEBUG(h...)	printk(h)
#else
#define XIRQB_DEBUG(h...)
#endif
#define XIRQB_ERR(h...)		printk("Error: " h)

typedef struct xirq_sample {
	NkTime	start;		/* start timer */
	NkTime	t1;		/* intermediate time in the handler of backend*/
	NkTime	t2;		/* intermediate time in the handler of frontend*/
	NkTime	stop;		/* time a the arrival*/
} xirq_sample_t;

typedef struct xirq_bench_desc xirq_bench_desc_t;

struct xirq_bench_desc {
	NkDevVlink*      	vlink;		/* point to vlink          */
	xirq_sample_t* 	 	sample;		/* point to data 	   */
	int			count;		/* number of cross-irq sent*/
	NkXIrqId         	xirq_desc_fe;	/* descriptor 		   */
	NkXIrqId         	xirq_desc_be;	/* descriptor 		   */
	NkXIrqId         	sysconf_desc;	/* descriptor 		   */
	NkXIrq           	xirq_fe;	/* target xirq 		   */
	NkXIrq           	xirq_be;	/* target xirq 		   */
	NkOsId		 	osid;		/* target os 		   */
	struct semaphore 	sem;		/* semaphore used to sleep */
	xirq_sample_t    	min;	        /* min time sample         */
	xirq_sample_t    	max;            /* max time sample         */
	NkTime			avg1;           /* temporary value         */
	NkTime			avg2;
	NkTime			avg3;
	NkTime			avg4;
	xirq_bench_desc_t*	next;           /* all descriptors are linked*/
	struct proc_dir_entry*	proc_entry;
};

static xirq_bench_desc_t* head; /* head of the list of bench descriptor */
static _Bool proc_nk_bench_created;

DEFINE_MUTEX(xirq_bench_lock); /* lock the driver */

static void xirq_fe_handler(void* cookie, NkXIrq xirq)
{
	xirq_bench_desc_t* d = (xirq_bench_desc_t*) cookie;
	(void) xirq;
	d->sample->t2 = os_ctx->smp_time();
	up(&d->sem);
}

static void xirq_be_handler(void* cookie, NkXIrq xirq)
{
	xirq_bench_desc_t* d = (xirq_bench_desc_t*) cookie;
	(void) xirq;
	d->sample->t1 = os_ctx->smp_time();

	XIRQB_DEBUG ("Backend:\n");
	XIRQB_DEBUG ("start %llx\n", d->sample->start);
	XIRQB_DEBUG ("   t1 %llx\n", d->sample->t1);
	XIRQB_DEBUG ("   t2 %llx\n", d->sample->t2);
	XIRQB_DEBUG (" stop %llx\n", d->sample->stop);

	nkops.nk_xirq_trigger(d->xirq_fe, d->osid);
}

static void _sysconf_trigger(xirq_bench_desc_t* desc)
{
	XIRQB_DEBUG("%s: osid %d\n", __func__, desc->osid);
	nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, desc->osid);
}

static inline void _reset(xirq_bench_desc_t* desc)
{
	(void) desc;
	/*
	 * I should probably remove entry in /proc -- but this requires to
	 * do this within a thread
	 */
}

static inline void _init(xirq_bench_desc_t* desc)
{
	(void) desc;
	/*
	 * I should probably create entry in /proc -- but this requires to
	 * do this within a thread
	 */
}

static int xirq_bench_handshake(xirq_bench_desc_t* desc, const _Bool is_server)
{
	volatile int* my_state;
	int           peer_state;

	if (is_server) {
	    my_state   = &(desc->vlink->s_state);
	    peer_state =  desc->vlink->c_state;
	} else {
	    my_state   = &(desc->vlink->c_state);
	    peer_state =  desc->vlink->s_state;
	}
	XIRQB_DEBUG("%s: (link:%d, fe:%d) myself:%d peer:%d\n", __func__,
		    desc->vlink->link, desc->vlink->c_id,
		    *my_state, peer_state);

	switch (*my_state) {
		case NK_DEV_VLINK_OFF:
			if (peer_state != NK_DEV_VLINK_ON) {
				_reset(desc);
				*my_state = NK_DEV_VLINK_RESET;
				_sysconf_trigger(desc);
			}
			break;
		case NK_DEV_VLINK_RESET:
			if (peer_state != NK_DEV_VLINK_OFF) {
				*my_state = NK_DEV_VLINK_ON;
				_init(desc);
				_sysconf_trigger(desc);
			}
			break;
		case NK_DEV_VLINK_ON:
			if (peer_state == NK_DEV_VLINK_OFF) {
				*my_state = NK_DEV_VLINK_OFF;
				_sysconf_trigger(desc);
			}
			break;
	}
	return (*my_state  == NK_DEV_VLINK_ON) &&
		(peer_state == NK_DEV_VLINK_ON);
}

static void _sysconf_handler(void* cookie, NkXIrq xirq)
{
	xirq_bench_desc_t* desc = (xirq_bench_desc_t*) cookie;
	const NkOsId myid = nkops.nk_id_get();

	(void) xirq;
	if (desc->vlink->s_id == myid) {
		xirq_bench_handshake(desc, 1);
	}
	if (desc->vlink->c_id == myid) {
		xirq_bench_handshake(desc, 0);
	}
}

static inline unsigned long long xirqb_div64_32 (unsigned long long num,
						 unsigned long _div)
{
	do_div (num, _div);
	return num;
}

typedef struct {
	char* pages;
	int len;
} xirqb_proc_t;

static void xirqb_printf (void* cookie, const char* format, ...)
{
	xirqb_proc_t* c = (xirqb_proc_t*) cookie;
	va_list ap;

	va_start(ap, format);
	c->len += vsprintf(c->pages + c->len, format, ap);
	va_end(ap);
}

static void xirqb_proc_read (xirq_bench_desc_t* d, xirqb_proc_t* c)
{
	const int		S = 1000000;
	int			hz;
	xirq_sample_t*		t;

	/* if no benchmark has been done */
	if (d->count == 0) {
	    return;
	}
	hz = os_ctx->smp_time_hz();

	mutex_lock(&xirq_bench_lock);

	xirqb_printf (c, "CROSS-IRQ BENCHMARK RESULTS:\n");
	xirqb_printf (c,
		"  Send %x cross-irq from %4d to %4d clock frequency %10d\n",
		       d->count , d->vlink->c_id,
		       d->vlink->s_id, hz);

	xirqb_printf (c, "TIME In tick: %-8s %-8s %-8s %-8s\n",
		       "FE->BE", "BE->FE-I", "I->Thrd", "ALL");

	/* display time for the best case */
	t = &d->min;
	xirqb_printf (c,
		       "  min       : %-8llu %-8llu %-8llu %-8llu\n",
		       t->t1   - t->start,
		       t->t2   - t->t1,
		       t->stop - t->t2,
		       t->stop - t->start);

	/* display time for the worst case */
	t = &d->max;
	xirqb_printf (c,
		       "  max       : %-8llu %-8llu %-8llu %-8llu\n",
		       t->t1   - t->start,
		       t->t2   - t->t1,
		       t->stop - t->t2,
		       t->stop - t->start);

	/* display average times  */
	xirqb_printf (c,
		       "  avg       : %-8llu %-8llu %-8llu %-8llu\n",
		       xirqb_div64_32 (d->avg1, d->count),
		       xirqb_div64_32 (d->avg2, d->count),
		       xirqb_div64_32 (d->avg3, d->count),
		       xirqb_div64_32 (d->avg4, d->count));

	xirqb_printf (c, "TIME In  us : %-8s %-8s %-8s %-8s\n",
		       "FE->BE", "BE->FE-I", "I->Thrd", "ALL");

#define TICKS_TO_US(x)	xirqb_div64_32 ((x) * S, hz)

	/* display time for the best case */
	t = &d->min;
	xirqb_printf (c,
		       "  min       : %-8llu %-8llu %-8llu %-8llu\n",
		       TICKS_TO_US (t->t1   - t->start),
		       TICKS_TO_US (t->t2   - t->t1),
		       TICKS_TO_US (t->stop - t->t2),
		       TICKS_TO_US (t->stop - t->start));

	/* display time for the worst case */
	t = &d->max;
	xirqb_printf (c,
		       "  max       : %-8llu %-8llu %-8llu %-8llu\n",
		       TICKS_TO_US (t->t1 -   t->start),
		       TICKS_TO_US (t->t2 -   t->t1),
		       TICKS_TO_US (t->stop - t->t2),
		       TICKS_TO_US (t->stop - t->start));

	/* display average times  */
	xirqb_printf (c,
		       "  avg       : %-8llu %-8llu %-8llu %-8llu\n",
		       TICKS_TO_US (xirqb_div64_32 (d->avg1, d->count)),
		       TICKS_TO_US (xirqb_div64_32 (d->avg2, d->count)),
		       TICKS_TO_US (xirqb_div64_32 (d->avg3, d->count)),
		       TICKS_TO_US (xirqb_div64_32 (d->avg4, d->count)));

	mutex_unlock(&xirq_bench_lock);
}

static int _proc_read(char *pages, char **start, off_t off,
		      int count, int *eof, void *data)
{
	xirqb_proc_t		c = {pages, 0};
	off_t			begin = 0;
	int			res = 0;
	xirq_bench_desc_t*	d = (xirq_bench_desc_t*) data;

	xirqb_proc_read (d, &c);

	if (c.len+begin < off) {
		begin += c.len;
		c.len = 0;
	}
	*eof = 1;

	if (off >= c.len+begin) {
		return 0;
	}

	*start = pages + (off-begin);
	res = ((count < begin+c.len-off) ? count : begin+c.len-off);

	return res;
}

#define MIN(A,B) ((A) < (B) ? (A) : (B))

static int xirqb_start (xirq_bench_desc_t* d, unsigned long count)
{
	int			ret = 0;
	unsigned		i;

	mutex_lock(&xirq_bench_lock);

	/* check remote hand is ready */
	if (d->vlink->s_state != NK_DEV_VLINK_ON  ||
	    d->vlink->c_state != NK_DEV_VLINK_ON )  {
		XIRQB_ERR("vlink %d (%d <-> %d) is not up\n",
			  d->vlink->link, d->vlink->c_id, d->vlink->s_id);
		ret = -EINVAL;
		goto out;
	}
	d->min.start = 0;
	d->min.t1 = 0;
	d->min.t2 = 0;
	d->min.stop = -1;
	d->max.start = 0;
	d->max.t1 = 0;
	d->max.t2 = 0;
	d->max.stop = 0;
	d->avg1 = 0;
	d->avg2 = 0;
	d->avg3 = 0;
	d->avg4 = 0;
	d->count = 0;

	/* initialize the number of iteration */
	d->count = count;

	/* send count xirq and take various measure */
	for (i = 0; i < count; i++) {
		NkTime	t;
		NkTime	t2;
		xirq_sample_t*	s = d->sample;

		s->t1 = 0;

		/* record start time */
		s->start = os_ctx->smp_time();

		XIRQB_DEBUG ("Before:\n");
		XIRQB_DEBUG ("start %llx\n", s->start);
		XIRQB_DEBUG ("   t1 %llx\n", s->t1);
		XIRQB_DEBUG ("   t2 %llx\n", s->t2);
		XIRQB_DEBUG (" stop %llx\n", s->stop);

		/* send the cross-irq */
		XIRQB_DEBUG ("xirq %d -> %d\n", d->xirq_be, d->osid);
		nkops.nk_xirq_trigger(d->xirq_be, d->osid);

		/* wait to be awaken */
		down(&d->sem);

		/* get end of time */
		s->stop = os_ctx->smp_time();

		XIRQB_DEBUG ("After:\n");
		XIRQB_DEBUG ("start %llx\n", s->start);
		XIRQB_DEBUG ("   t1 %llx\n", s->t1);
		XIRQB_DEBUG ("   t2 %llx\n", s->t2);
		XIRQB_DEBUG (" stop %llx\n", s->stop);

		/* compute overall time */
		t = s->stop - s->start;

		/* if we get the minimal time -> record it*/
		t2 = d->min.stop - d->min.start;
		if (t2 > t) {
			d->min = *s;
		}

		t2 = d->max.stop - d->max.start;
		if (t > t2) {
		    	d->max = *s;
		}

		/* overall time to make transition from this guest to the
		 * remote guest */
		d->avg1 += s->t1 - s->start;

		/* overall time to make transition from remote to here */
		d->avg2 += s->t2 - s->t1;

		/* time to wakeup the thread */
		d->avg3 += s->stop - s->t2;

		/* overall time to make the whole transition */
		d->avg4 += s->stop - s->start;
	}
out:
	mutex_unlock(&xirq_bench_lock);
	return ret;
}

static int _proc_write(struct file* file, const char __user *buf,
		       unsigned long count, void* data)
{
	xirq_bench_desc_t*	d = (xirq_bench_desc_t*) data;
	char			buffer[64];
	unsigned long		iterations;

	if (copy_from_user(buffer, buf, MIN(count, sizeof(buffer) - 1)))
		return -EFAULT;
	buffer [sizeof buffer - 1] = '\0';

	/* compute the number of iteration */
	iterations = simple_strtoul(buffer, NULL, 0);
	printk("Got %lu iterations\n", iterations);

	xirqb_start (d, iterations);
	return count;
}

static int __init xirq_bench_prepare(void)
{
	NkOsId      osid_self = nkops.nk_id_get();
	NkPhAddr    plink = 0;
	NkPhAddr    pa;
	NkDevVlink* vlink;
	xirq_bench_desc_t* desc;
	unsigned    desc_count = 0;

	    /*
	     * Sometimes globals are not cleared (!) and we get a crash
	     * in the module exit function as the end of list is junk.
	     * Try to prevent this by clearing explicitly.
	     */
	head = NULL;
	proc_nk_bench_created = 0;

	printk(" ++++++++======= BENCH INIT ======== +++++++++++++++++++\n");

	while ((plink = nkops.nk_vlink_lookup("xirqb", plink))) {

		vlink = (NkDevVlink*) nkops.nk_ptov(plink);
		if (vlink->s_id != osid_self &&
		    vlink->c_id != osid_self) {
			continue;
		}
		desc = (xirq_bench_desc_t*)
			kzalloc(sizeof(xirq_bench_desc_t),  GFP_KERNEL);
		if (desc == 0) {
			XIRQB_ERR("Descriptor allocation failure\n");
			return -ENOMEM;
		}
		desc->vlink = vlink;

		pa = nkops.nk_pdev_alloc(plink, 0, sizeof(xirq_sample_t));
		if (!pa) {
			XIRQB_ERR("cannot allocate sample memory\n");
			kfree (desc);
			return -ENOMEM;
		}
		desc->sample = (xirq_sample_t*) nkops.nk_ptov(pa);
		if (desc->sample == 0) {
			XIRQB_ERR("ptov failure\n");
			kfree (desc);
			return -ENOMEM;
		}
		sema_init(&desc->sem, 0);

		desc->xirq_be = nkops.nk_pxirq_alloc(plink, 0, vlink->s_id, 1);
		desc->xirq_fe = nkops.nk_pxirq_alloc(plink, 1, vlink->c_id, 1);

		if (vlink->s_id == osid_self) {
			desc->osid = vlink->c_id;
			desc->xirq_desc_be = nkops.nk_xirq_attach(desc->xirq_be,
							 xirq_be_handler,
							 desc);
			if (!desc->xirq_desc_be) {
				XIRQB_ERR("cannot attach be xirq handler\n");
				kfree (desc);
				return -ENOMEM;
			}
		}
		if (vlink->c_id == osid_self) {
			desc->osid = vlink->s_id;
			desc->xirq_desc_fe = nkops.nk_xirq_attach(desc->xirq_fe,
							 xirq_fe_handler,
							 desc);
			if (!desc->xirq_desc_fe) {
				XIRQB_ERR("cannot attach fe xirq handler\n");
				if (desc->xirq_desc_be) {
					nkops.nk_xirq_detach(desc->xirq_desc_be);
				}
				kfree (desc);
				return -ENOMEM;
			}
		}
		desc->sysconf_desc = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF,
						    _sysconf_handler,
						    desc);
		if (!desc->sysconf_desc) {
			XIRQB_ERR("cannot attach SYSCONF xirq\n");
			if (desc->xirq_desc_be) {
				nkops.nk_xirq_detach(desc->xirq_desc_be);
			}
			if (desc->xirq_desc_fe) {
				nkops.nk_xirq_detach(desc->xirq_desc_fe);
			}
			kfree (desc);
			return -ENOMEM;
		}
		if (vlink->c_id == osid_self) {
			char	name[32];
			struct proc_dir_entry *res;

			    /*
			     * Linux (2.6.32) does allow duplicate directory
			     * creation even though it will issue a warning
			     * and backtrace the location, so we will actually
			     * never get the "already registered" code.
			     */
			if (!proc_nk_bench_created) {
			    struct proc_dir_entry *dir;

			    dir = proc_mkdir("nk/bench", 0);
			    if (dir == 0) {
				XIRQB_TRACE("nk/bench already registered\n");
			    } else {
				printk("bench dir created\n");
			    }
			    proc_nk_bench_created = 1;
			}
			snprintf(name, sizeof name, "nk/bench/%d", vlink->s_id);
			desc->proc_entry = res =
				create_proc_entry(name, S_IRWXU, 0);
			if (res) {
			    printk("%s created\n", name);
				res->read_proc  = _proc_read;
				res->write_proc = _proc_write;
				res->data	= desc;
			} else {
				XIRQB_ERR("Could not allocate proc "
					  "entry for front-end\n");
			}
		}
		desc->next = head;
		head = desc;
		++desc_count;
		nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, osid_self);
	}
	XIRQB_TRACE("%d xirqb device(s) created\n", desc_count);
	return 0;
}

static void xirq_bench_exit(void)
{
	xirq_bench_desc_t*	desc;

	mutex_lock(&xirq_bench_lock);
	while ( (desc = head) != NULL ) {
		head = desc->next;
		if (desc->xirq_desc_fe) {
			nkops.nk_xirq_detach(desc->xirq_desc_fe);
		}
		if (desc->xirq_desc_be) {
			nkops.nk_xirq_detach(desc->xirq_desc_be);
		}
		if (desc->sysconf_desc) {
			nkops.nk_xirq_detach(desc->sysconf_desc);
		}
		if (desc->vlink->c_id == nkops.nk_id_get()) {
			char	buffer[32];

			snprintf(buffer, sizeof buffer, "nk/bench/%d",
				 desc->vlink->s_id);
			remove_proc_entry(buffer, 0);
			desc->vlink->c_state = NK_DEV_VLINK_OFF;
		}
		if (desc->vlink->s_id == nkops.nk_id_get()) {
			desc->vlink->s_state = NK_DEV_VLINK_OFF;
		}
		_sysconf_trigger(desc);
		kfree(desc);
	}
	if (proc_nk_bench_created) {
		remove_proc_entry("nk/bench", NULL);
	}
	mutex_unlock(&xirq_bench_lock);
}

static int __init xirq_bench_init(void)
{
	int diag = xirq_bench_prepare();

	if (diag) {
		xirq_bench_exit();
	}
	return diag;
}

module_init(xirq_bench_init);
module_exit(xirq_bench_exit);

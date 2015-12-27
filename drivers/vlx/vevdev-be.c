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

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/major.h>
#include <linux/device.h>
#include <linux/version.h>

#include <nk/nkern.h>
#include <vlx/vevdev_common.h>

#ifdef CONFIG_INPUT_VEVDEV_BE_FOCUS
#include <linux/reboot.h>
#include <linux/syscalls.h>
#include <linux/semaphore.h>
#endif

MODULE_AUTHOR("guennadi.maslov@redbend.com");
MODULE_DESCRIPTION("Virtual Input driver event char devices");
MODULE_LICENSE("GPL");

#define VLINK 		"BLINK : "

#define MAX_VINPUT		16


/*
 *  Here's we are producer
 */

typedef struct Vdev {
    int         enabled;        /* flag: device has all resources allocated */
    NkDevVlink* vlink;          /* consumer/producer link */
    VRing*      vring;          /* consumer/producer circular ring */
    NkXIrq      xirq;           /* cross interrupt to send to consumer OS */
    VIdev*      videv;          /* input device configuration */
} Vdev;

typedef Vdev*	Tdev[NK_OS_LIMIT];

static  Tdev	vconsumers[MAX_VINPUT];

static NkXIrqId vsysconf_id;    /* xirq */

static int 	os_event[MAX_VINPUT];

static int combined_mode = 0;

#define VRING_P_ROOM(rng)     (VRING_SIZE - ((rng)->p_idx - (rng)->c_idx))

#ifdef CONFIG_INPUT_VEVDEV_BE_FOCUS

#define	VEVENT_FOCUS_NAME_MAX	32
#define	VEVENT_FOCUS_KEY_MAX	(NK_OS_LIMIT * 2)

typedef enum {
    VEVENT_FOCUS_NOTHING,
    VEVENT_FOCUS_SWITCH,
    VEVENT_FOCUS_RESTART,
    VEVENT_FOCUS_CPU_STANDBY
} focus_key_action_t;

typedef struct focus_key_t {
    int                 code;
    int                 value;
    focus_key_action_t  action;
    NkOsId              id;  
    struct focus_key_t* next;
    char		name[VEVENT_FOCUS_NAME_MAX];
} focus_key_t;

    extern void
nk_change_focus (NkOsId id);

static int               focus_abort;
static pid_t             focus_task;
static struct completion exit_focus_task;
static struct semaphore  wait_focus_task;
static focus_key_t*      focus_keys;
static focus_key_t*      focus;
static focus_key_t       focus_key_pool[VEVENT_FOCUS_KEY_MAX];
static focus_key_t*      focus_key_free  = focus_key_pool;
static focus_key_t*      focus_key_limit = focus_key_pool+VEVENT_FOCUS_KEY_MAX;

    static int
_add_focus_key (char*              name,
		int                len,
		int                code,
		focus_key_action_t action,
		NkOsId             id)
{
    focus_key_t* key;
    int          i;

    if (focus_key_free == focus_key_limit) {
	return -ENOMEM;
    }
    key = focus_key_free++;

    for (i = 0; i < len; i++) {
	key->name[i] = ((name[i] == '~') ? ' ' : name[i]);
    }
    key->name[len] = 0;

    key->code   = code;
    key->action = action;
    key->id     = id;

    key->next   = focus_keys;
    focus_keys  = key;

    return 0;
}

    static focus_key_t*
_find_focus_key (const char* name, int code)
{
    focus_key_t* key = focus_keys;
    while (key && ((key->code != code) || strcmp(key->name, name))) {
	key = key->next;
    }
    return key;
}

    static int
focus_thread (void *param)
{
    while (!focus_abort) {
	focus_key_t* myfocus;

	down(&wait_focus_task);
		
	myfocus = focus;
	focus   = 0;

	if (!myfocus) {
	    continue;
	}

        switch (myfocus->action) {

	    case VEVENT_FOCUS_SWITCH: {
		nk_change_focus(myfocus->id);
		break;
	    }

	    case VEVENT_FOCUS_RESTART: {
		if (myfocus->id == nkops.nk_id_get()) {
		    int res;
                    res = sys_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                                     LINUX_REBOOT_CMD_RESTART, 0);
		    printk("sys_reboot failed %d\n", res);
		    for(;;);
		}
                os_ctx->restart(os_ctx, myfocus->id);
		break;
	    }

	    case VEVENT_FOCUS_CPU_STANDBY: {
		NkBootInfo vinfo;
		NkPhAddr   paddr = nkops.nk_vtop(&vinfo);
	        vinfo.ctrl = -1;
	        vinfo.osid = myfocus->id;
		if (os_ctx->binfo(os_ctx, paddr) >= 0) {
		    int* standby = (int*)(nkops.nk_ptov(vinfo.data));
		    *standby = (*standby ? 0 : 1);
		    printk("CPU%d is %s-line\n",
			   vinfo.osid, (*standby ? "off" : "on"));
		}
		break;
	    }

	    default: {
		break;
	    }

	}

    }

    complete_and_exit(&exit_focus_task, 0);

    return 0;
}

    static int
init_focus_task (void)
{
	/* init completion */
    init_completion(&exit_focus_task);

	/* init semaphore */
    sema_init(&wait_focus_task, 0);

	/* initialize and run working thread */
    focus_task = kernel_thread(focus_thread, NULL, 0);

    if (focus_task <= 0) {
	printk(KERN_ERR "input_focus : focus-thread creation failed.\n");
	complete(&exit_focus_task);
	return -EIO;
    }

    return 0;
}

    static void
kill_focus_task (void)
{
	/* wait for thread to stop */ 
    focus_abort = 1;

	/* wake up thread if it is sleeping */
    up(&wait_focus_task);

#ifdef later /* _PP_PP_PP_ */
    if (kill_proc(focus_task, SIGTERM, 1) < 0) {
	printk("input focus: focus_thread kill failed\n");
    }
#endif
	
    wait_for_completion(&exit_focus_task);
}

    static int
check_focus_keys (const char* name, int code, int value)
{
    focus_key_t* key = _find_focus_key(name, code);

    if (!key) {
	return 0;
    }

    if (value) {
        if (key->value) {
	    return 0;
        }
	key->value = 1;
	return 1;
    }

    if (!key->value) {
	return 0;
    }
    key->value = 0;

    focus = key;

    up(&wait_focus_task);

    return 1;
}

#endif

    static void
evdev_event (struct input_handle *handle, unsigned int type,
	     unsigned int code, int value)
{
    Vdev*  vdev;
    VRing* vring;
    NkDevVlink*   vlink;
    input_vevent* pevent;
    int i = (int)handle->private;

#ifdef CONFIG_INPUT_VEVDEV_BE_FOCUS
    if (focus_keys && check_focus_keys(handle->dev->name, code, value)) {
	return;
    }
#endif

    vdev = (os_event[i] ? vconsumers[i][os_event[i]] : 0);
    if (vdev && vdev->enabled) {

    	vring = vdev->vring;
    	vlink = vdev->vlink;

	if (vlink->c_state == NK_DEV_VLINK_ON) {

	    pevent = &VRING_PTR(vring, p_idx);

	    pevent->type  = type;
	    pevent->code  = code;
	    pevent->value = value;

	    if (VRING_P_ROOM(vring))
		vring->p_idx++;

	    nkops.nk_xirq_trigger(vdev->xirq, vlink->c_id);
#ifdef TRACE
	    printk(VLINK "event from %s (%d) for OS#%d\n",
			 kobject_name(&handle->dev->dev.kobj),
			 vdev->xirq, vlink->c_id);
#endif
	}
    }
}

    static void
vdev_sysconf_trigger(Vdev* dev)
{
#ifdef TRACE
    printk(VLINK "Sending sysconf OS#%d(%d)->OS#%d(%d) [dev=%08x]\n",
           dev->vlink->s_id,
           dev->vlink->s_state,
           dev->vlink->c_id,
           dev->vlink->c_state,
           dev);
#endif
    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, dev->vlink->c_id);
}

    static int
vdev_handshake (Vdev* dev)
{
    volatile int* my_state;
    int           peer_state;

    my_state   = &dev->vlink->s_state;
    peer_state =  dev->vlink->c_state;

#ifdef TRACE
    printk(VLINK "handshake link %d OS#%d(%d)->OS#%d(%d) [dev=%x]\n",
	   dev->vlink->link,
	   dev->vlink->c_id, peer_state, 
	   dev->vlink->s_id, *my_state, 
	   dev);
#endif
    switch (*my_state) {
        case NK_DEV_VLINK_OFF:
            if (peer_state != NK_DEV_VLINK_ON) {
                dev->vring->p_idx = 0;
                *my_state = NK_DEV_VLINK_RESET;
                vdev_sysconf_trigger(dev);
            }
            break;
        case NK_DEV_VLINK_RESET:
            if (peer_state != NK_DEV_VLINK_OFF) {
                *my_state = NK_DEV_VLINK_ON;
                vdev_sysconf_trigger(dev);
            }
            break;
        case NK_DEV_VLINK_ON:
            if (peer_state == NK_DEV_VLINK_OFF) {
                dev->vring->p_idx = 0;
                *my_state = NK_DEV_VLINK_RESET;
                vdev_sysconf_trigger(dev);
            }
            break;
    }
    return (*my_state  == NK_DEV_VLINK_ON) &&
           (peer_state == NK_DEV_VLINK_ON);
}

    static void
vsysconf_hdl(void* cookie, NkXIrq xirq)
{
    Vdev** tdev;
    Vdev*  dev;
    int    i,j;

    for ( i = 0; i < MAX_VINPUT; i++) {
    	tdev = vconsumers[i];
	for ( j = 0; j < NK_OS_LIMIT; j++) {
	    dev = tdev[j];
            if (dev && dev->enabled) {
	        vdev_handshake (dev);
	    }
	}
    }
}

    static NkDevVlink*
evdev_lookup_vlink(char *name)
{
    NkPhAddr    plink;
    NkDevVlink* vlink;
    char*	vname;
    NkOsId      my_id = nkops.nk_id_get();

    plink    = 0;
    while ((plink = nkops.nk_vlink_lookup("vevent", plink))) {

	vlink = nkops.nk_ptov(plink);

	if ((vlink->s_id != my_id) || !(vlink->s_info))
	    continue;

    	vname = nkops.nk_ptov(vlink->s_info);

    	if (strcmp(vname, name) == 0)
	    break;
    }
    return (plink?vlink:0);
}

    static int
_is_combined_device_mode(void)
{
    NkPhAddr plink;
	NkDevVlink* vlink;
	char* vname;
	NkOsId my_id = nkops.nk_id_get();
	int result = 0; // default false

	plink = 0;
	while ((plink = nkops.nk_vlink_lookup("vevent", plink))) {
		vlink = nkops.nk_ptov(plink);
		if ((vlink->s_id != my_id) || !(vlink->s_info))
			continue;
		vname = nkops.nk_ptov(vlink->s_info);
		if (strcmp(vname, VKP_NAME) != 0 && 
		    strcmp(vname, VTS_NAME) != 0 && 
		    strcmp(vname, VMS_NAME) != 0) {
			result = 1;
			break;
		}
	}
	return result;
}

    static void
evdev_get_config (VIdev* videv, char *name, struct input_dev *dev)
{
	unsigned int size = 0;
#ifdef TRACE
	printk(VLINK "%s:%d\n", __FUNCTION__, __LINE__);
#endif

	// Check compatibilities
	if (EV_CNT > VEVENT_EV_CNT)
		printk(VLINK "Warning: EV_CNT[0x%04x] > VEVENT_EV_CNT[0x%04x]\n",
				EV_CNT, VEVENT_EV_CNT);
	if (KEY_CNT > VEVENT_KEY_CNT)
		printk(VLINK "Warning: KEY_CNT[0x%04x] > VEVENT_KEY_CNT[0x%04x]\n",
				ABS_CNT, VEVENT_KEY_CNT);
	if (REL_CNT > VEVENT_REL_CNT)
		printk(VLINK "Warning: REL_CNT[0x%04x] > VEVENT_REL_CNT[0x%04x]\n",
				REL_CNT, VEVENT_REL_CNT);
	if (ABS_CNT > VEVENT_ABS_CNT)
		printk(VLINK "Warning: ABS_CNT[0x%04x] > VEVENT_ABS_CNT[0x%04x]\n",
				ABS_CNT, VEVENT_ABS_CNT);
	if (MSC_CNT > VEVENT_MSC_CNT)
		printk(VLINK "Warning: MSC_CNT[0x%04x] > VEVENT_MSC_CNT[0x%04x]\n",
				MSC_CNT, VEVENT_MSC_CNT);
	if (LED_CNT > VEVENT_LED_CNT)
		printk(VLINK "Warning: LED_CNT[0x%04x] > VEVENT_LED_CNT[0x%04x]\n",
				LED_CNT, VEVENT_LED_CNT);
	if (SND_CNT > VEVENT_SND_CNT)
		printk(VLINK "Warning: SND_CNT[0x%04x] > VEVENT_SND_CNT[0x%04x]\n",
				SND_CNT, VEVENT_SND_CNT);
	if (FF_CNT > VEVENT_FF_CNT)
		printk(VLINK "Warning: FF_CNT[0x%04x] > VEVENT_FF_CNT[0x%04x]\n",
				FF_CNT, VEVENT_FF_CNT);
	if (SW_CNT > VEVENT_SW_CNT)
		printk(VLINK "Warning: SW_CNT[0x%04x] > VEVENT_SW_CNT[0x%04x]\n",
				SW_CNT, VEVENT_SW_CNT);

	if (strcmp(name, VTS_NAME) == 0) {
		/* Virtual TouchScreen */
		size = (ABS_CNT > VEVENT_ABS_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_ABS_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(ABS_CNT));
		memcpy(videv->absbit, dev->absbit, size);

		size = (ABS_CNT > VEVENT_ABS_CNT) ? VEVENT_ABS_CNT : ABS_CNT;
		if (dev->absinfo) {
			int i;
			for (i = 0 ; i < size ; i++) {
				videv->absmax[i]  = dev->absinfo[i].maximum;
				videv->absmin[i]  = dev->absinfo[i].minimum;
				videv->absfuzz[i] = dev->absinfo[i].fuzz;
				videv->absflat[i] = dev->absinfo[i].flat;
			}
		}
	}
	else if (strcmp(name, VKP_NAME) == 0) {
		/* Virtual Keypad */
	}
	else {
		/* combined device */
		size = (EV_CNT > VEVENT_EV_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_EV_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(EV_CNT));
		memcpy(videv->evbit, dev->evbit, size);

		size = (KEY_CNT > VEVENT_KEY_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_KEY_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(KEY_CNT));
		memcpy(videv->keybit, dev->keybit, size);

		size = (REL_CNT > VEVENT_REL_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_REL_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(REL_CNT));
		memcpy(videv->relbit, dev->relbit, size);

		size = (ABS_CNT > VEVENT_ABS_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_ABS_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(ABS_CNT));
		memcpy(videv->absbit, dev->absbit, size);

		size = (MSC_CNT > VEVENT_MSC_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_MSC_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(MSC_CNT));
		memcpy(videv->mscbit, dev->mscbit, size);

		size = (LED_CNT > VEVENT_LED_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_LED_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(LED_CNT));
		memcpy(videv->ledbit, dev->ledbit, size);

		size = (SND_CNT > VEVENT_SND_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_SND_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(SND_CNT));
		memcpy(videv->sndbit, dev->sndbit, size);

		size = (FF_CNT > VEVENT_FF_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_FF_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(FF_CNT));
		memcpy(videv->ffbit, dev->ffbit, size);

		size = (SW_CNT > VEVENT_SW_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_SW_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(SW_CNT));
		memcpy(videv->swbit, dev->swbit, size);

		size = (KEY_CNT > VEVENT_KEY_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_KEY_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(KEY_CNT));
		memcpy(videv->key, dev->key, size);

		size = (LED_CNT > VEVENT_LED_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_LED_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(LED_CNT));
		memcpy(videv->led, dev->led, size);

		size = (SND_CNT > VEVENT_SND_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_SND_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(SND_CNT));
		memcpy(videv->snd, dev->snd, size);

		size = (SW_CNT > VEVENT_SW_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_SW_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(SW_CNT));
		memcpy(videv->sw, dev->sw, size);

		size = (ABS_CNT > VEVENT_ABS_CNT) ? VEVENT_ABS_CNT : ABS_CNT;
		if (dev->absinfo) {
			int i;
			for (i = 0 ; i < size ; i++) {
				videv->abs[i]     = dev->absinfo[i].value;
				videv->absmax[i]  = dev->absinfo[i].maximum;
				videv->absmin[i]  = dev->absinfo[i].minimum;
				videv->absfuzz[i] = dev->absinfo[i].fuzz;
				videv->absflat[i] = dev->absinfo[i].flat;
			}
		}
	}
}

    static int
evdev_alloc (int num_event, char *name, struct input_dev *dev)
{
    Vdev**      tdev  = vconsumers[num_event];
    NkOsId      my_id = nkops.nk_id_get();
    NkPhAddr    plink;
    NkDevVlink* vlink;
    char*	vname;
    Vdev*       vdev;
    NkPhAddr    pdata;
    NkXIrq	xirq;

    plink    = 0;
    while ((plink = nkops.nk_vlink_lookup("vevent", plink))) {

	vlink = nkops.nk_ptov(plink);

	if ((vlink->s_id != my_id) || !(vlink->s_info))
	    continue;

    	vname = nkops.nk_ptov(vlink->s_info);

	if ((vlink->link != num_event) || strcmp(vname, name))
	    continue;

	tdev[vlink->c_id] = vdev = (Vdev*)kzalloc(sizeof(Vdev), GFP_KERNEL);
	if (!vdev) {
	    return 1;
	}
	vdev->vlink = vlink;

        /*
         * Allocate communication ring and input dev config
         */
#ifdef TRACE
	printk(VLINK "sizeof(VRing)=%d  sizeof(VIdev)=%d\n", sizeof(VRing), sizeof(VIdev));
#endif
	pdata = nkops.nk_pdev_alloc(plink, 0, sizeof(VRing) + sizeof(VIdev));
	if (pdata == 0) {
	    printk(VLINK "OS#%d->OS#%d link=%d ring alloc failed\n",
		   vlink->s_id, vlink->c_id, vlink->link);
	    return 1;
    	} else {
	    vdev->vring = nkops.nk_ptov(pdata);
	    vdev->videv = nkops.nk_ptov(pdata + sizeof(VRing));
            evdev_get_config(vdev->videv, name, dev);
	}

	/*
	 * Allocate consumer cross irq
	 */
	xirq = nkops.nk_pxirq_alloc(plink, 0, vlink->c_id, 1);
	if (xirq == 0) {
	    printk(VLINK "OS#%d->OS#%d link=%d xirq alloc failed\n",
		   vlink->s_id, vlink->c_id, vlink->link);
	    return 1;
    	} else {
	    vdev->xirq = xirq;
	}

	vdev->enabled = 1;
    }
    return 0;
}

    static void
evdev_start_vlink (int num_event, char *name)
{
    Vdev**      tdev  = vconsumers[num_event];
    Vdev*       dev;
    NkOsId      my_id = nkops.nk_id_get();
    NkPhAddr    plink;
    NkDevVlink* vlink;
    char*	vname;

    plink    = 0;
    while ((plink = nkops.nk_vlink_lookup("vevent", plink))) {

	vlink = nkops.nk_ptov(plink);

	if ((vlink->s_id != my_id) || !(vlink->s_info))
	    continue;

    	vname = nkops.nk_ptov(vlink->s_info);

	if ((vlink->link != num_event) || strcmp(vname, name))
	    continue;

	/*
	 * perform handshake until both links are ready
	 */
	dev = tdev[vlink->c_id];
	if (dev && dev->enabled) {
	    vdev_handshake(dev);
	}
    }
}

/*
 *   called by input_attach_handler at device/handle matching
 */

    static int
evdev_connect (struct input_handler *handler, struct input_dev *dev,
	       const struct input_device_id *id, char *service)
{
    NkDevVlink* vlink;
    struct input_handle *handle;
    int         error;

    if (dev->id.bustype == BUS_VIRTUAL)
        return -ENODEV;

    vlink = evdev_lookup_vlink(service);
    if (!vlink)
	return -ENODEV;

    handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!handle)
	return -ENOMEM;

    handle->dev     = dev;
    handle->handler = handler;
    handle->name    = handler->name;
    handle->private = (void *)vlink->link;

    error = input_register_handle(handle);
    if (error)
	goto err_free_handle;

    error = evdev_alloc(vlink->link, service, dev);
    if (error)
	goto err_unregister_handle;

    error = input_grab_device(handle);
    if (error)
	goto err_unregister_handle;

    evdev_start_vlink(vlink->link, service);

    return 0;

err_unregister_handle:
    input_unregister_handle(handle);

err_free_handle:
    kfree(handle);
    printk (KERN_INFO "evdev-backend returns %d\n", error);
    return error;
}

    static int
combined_connect (struct input_handler *handler, struct input_dev *dev,
		  const struct input_device_id *id)
{
    // This function is called each time a new input device is registered
    // and is matched to the condition in combined_ids
	int error = 0;
	int i;
	char *service;
	service = (char*) dev->name ?: "unknown";
#ifdef TRACE
	printk(VLINK "New input device detected: %s (name=%s)\n",	dev_name(&dev->dev), dev->name ?: "unknown");
#endif
	if (!combined_mode) {
		// match vts device if device has ABS bits
		int absbit_mask0 = (BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) |
				    BIT_MASK(ABS_PRESSURE));
		int absbit_mask1 = (BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) |
				    BIT_MASK(ABS_TOOL_WIDTH));
		if (test_bit(EV_KEY, dev->evbit) && 
		    test_bit(EV_ABS, dev->evbit) && 
		    (dev->keybit[BIT_WORD(BTN_TOUCH)] & BIT_MASK(BTN_TOUCH)) &&
		    (((dev->absbit[0] & absbit_mask0) == absbit_mask0) ||
		     ((dev->absbit[0] & absbit_mask1) == absbit_mask1))) {
			error = evdev_connect(handler, dev, id , VTS_NAME);
			if (error != -ENODEV) {
				printk(VLINK "vts_connect %s\n",kobject_name(&dev->dev.kobj));
				return error;
			}
		}

		// match vms.
		if (test_bit(REL_X, dev->relbit) || test_bit(REL_Y, dev->relbit)) {
			printk(VLINK "vms_connect %s\n", kobject_name(&dev->dev.kobj));
			return evdev_connect(handler, dev, id , VMS_NAME);
		}

		// match vkp device if device has at least one button
		for (i = KEY_RESERVED; i < BTN_MISC; i++)
			if (test_bit(i, dev->keybit))
				break;
		if (i != BTN_MISC) {
			error = evdev_connect(handler, dev, id , VKP_NAME);
			if (error != -ENODEV) {
				printk(VLINK "vkp_connect %s\n",kobject_name(&dev->dev.kobj));
				return error;
			}
		}
	}
	else {
		// match combined device
		error = evdev_connect(handler, dev, id , service);
		if (error >= 0) {
			printk(VLINK "vevent_connect %s\n",kobject_name(&dev->dev.kobj));
		}
	}
	return error;
}

static void evdev_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id combined_ids[] = {
	{
		.flags   = INPUT_DEVICE_ID_MATCH_EVBIT
		| INPUT_DEVICE_ID_MATCH_KEYBIT
		| INPUT_DEVICE_ID_MATCH_RELBIT
		| INPUT_DEVICE_ID_MATCH_ABSBIT
		| INPUT_DEVICE_ID_MATCH_MSCIT
		| INPUT_DEVICE_ID_MATCH_LEDBIT
		| INPUT_DEVICE_ID_MATCH_SNDBIT
		| INPUT_DEVICE_ID_MATCH_FFBIT
		| INPUT_DEVICE_ID_MATCH_SWBIT,
		.bustype = BUS_VIRTUAL,
	}, /* Match all input devices */
	{ }, /* Terminating zero entry */
};

MODULE_DEVICE_TABLE(input, combined_ids);

static struct input_handler combined_handler = {
	.event =	evdev_event,
	.connect =	combined_connect,
	.disconnect =	evdev_disconnect,
	.name =		"evdev-backend",
	.id_table =	combined_ids,
};

extern int focus_register_client(struct notifier_block *nb);
extern int focus_unregister_client(struct notifier_block *nb);

static struct notifier_block focus_nb;

    int
evdev_notify_focus(struct notifier_block *self,
                   unsigned long event, void *data)
{
     int i;
     for (i = 0; i < MAX_VINPUT; i++) {
	 if (vconsumers[i][(int)event]) {
	     os_event[i] = (int)event;
	 }
     }
     return NOTIFY_DONE;
}

    static int __init
evdev_init (void)
{
	int i;

    	memset(vconsumers, 0, sizeof(Vdev*) * NK_OS_LIMIT * MAX_VINPUT );

        /*
         * Attach sysconfig handler
         */
	vsysconf_id = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF, vsysconf_hdl, 0);

	combined_mode = _is_combined_device_mode();
#ifdef TRACE
	printk(VLINK "Combined mode : %s\n", combined_mode ? "enabled":"disabled");
#endif
	if (input_register_handler(&combined_handler)) {
	    printk(VLINK "cannot register evdev handler\n");
	    return -1;
	}


        focus_nb.notifier_call = evdev_notify_focus;

        for (i = 0; i < MAX_VINPUT; i++) {
           if (!os_event[i]) {
               os_event[i] = nkops.nk_id_get();
           }
        }

        focus_register_client(&focus_nb);

#ifdef CONFIG_INPUT_VEVDEV_BE_FOCUS
	if (focus_keys) {
	    int err = init_focus_task();
	    if(err != 0)
	        return err;
	}
#endif

	return 0;
}

    static void
__exit evdev_exit(void)
{
	int i,j;

    	for (i = 0; i < MAX_VINPUT; i++) {
    	    for (j = 0; j <  NK_OS_LIMIT; j++) {
		Vdev* dev;
                dev = vconsumers[i][j];
		if (dev) {
		    dev->vlink->s_state = NK_DEV_VLINK_OFF;
    		    vdev_sysconf_trigger(dev);
		    kfree (dev);
		}
	    }
	}

        focus_unregister_client(&focus_nb);
	input_unregister_handler(&combined_handler);

	if (vsysconf_id)
            nkops.nk_xirq_detach(vsysconf_id);

#ifdef CONFIG_INPUT_VEVDEV_BE_FOCUS
	if (focus_keys) {
	    kill_focus_task();
	}
#endif
}

#if defined(CONFIG_INPUT_VEVDEV_BE_FOCUS) && !defined(MODULE)

    static int
_vevent_focus_params (char* start, focus_key_action_t action, int base)
{
    NkOsId id = base;
    int    len;
    char*  name;

    if (*start++ != '(') {
	return 0;
    }

    name = start;
    while (*start && (*start != ':')) {
	start++;	
    }
    if (*start != ':') {
	return 0;
    }
    len = start - name;
    if (!len || (len >= VEVENT_FOCUS_NAME_MAX)) {
	return 0;
    }
    start++;

    while (*start) {
	char* end;
	int   code;

   	code = simple_strtoul(start, &end, 0);

	if ((end[0] != ',') && (end[0] != ')')) {
	    return 0;
	}

	if (start != end) {
	    if (_add_focus_key(name, len, code, action, id)) {
		return 0;
	    }
	}

	if (end[0] == ')') {
	    return (end[1] == 0);
	}

	id++;

	if (id == NK_OS_LIMIT) {
	    return 0;
	}

	start = end+1;
    }

    return 0;
}

    static int __init
vevent_focus_params (char* start)
{
    return _vevent_focus_params(start, VEVENT_FOCUS_SWITCH, 2);
}

    static int __init
vevent_restart_params (char* start)
{
    return _vevent_focus_params(start, VEVENT_FOCUS_RESTART, 2);
}

    static int __init
vevent_cpu_standby_params (char* start)
{
    return _vevent_focus_params(start, VEVENT_FOCUS_CPU_STANDBY, 0);
}

__setup("vevent-focus=",       vevent_focus_params);
__setup("vevent-restart=",     vevent_restart_params);
__setup("vevent-cpu-standby=", vevent_cpu_standby_params);

#endif

    static int
_vevent_owners_params (char* start)
{
    int i = 0;

    if (*start++ != '(') {
       return 0;
    }

    while (*start) {
       char*  end;
       NkOsId owner;

       owner = simple_strtoul(start, &end, 0);

       if ((end[0] != ',') && (end[0] != ')')) {
           return 0;
       }

       if (start != end) {
           os_event[i] = owner;
       }

       if (end[0] == ')') {
           return (end[1] == 0);
       }

       if (++i == MAX_VINPUT) {
           return 0;
       }

       start = end+1;
    }

    return 0;
}

__setup("vevent-owners=", _vevent_owners_params);

module_init(evdev_init);
module_exit(evdev_exit);

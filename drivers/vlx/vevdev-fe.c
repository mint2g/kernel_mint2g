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

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <nk/nkern.h>
#include <vlx/vevdev_common.h>

MODULE_DESCRIPTION("vevent communication driver");
MODULE_AUTHOR("Guennadi Maslov <guennadi.maslov@redbend.com>");
MODULE_LICENSE("GPL");

#define VLINK		"FLINK: "

    /*
     * VRING_C_ROOM   - how much "room" in a circular ring (i.e. how many
     *			available bytes) we have for the consumer
     * VRING_C_CROOM  - how much "continuous room" (from the current position
     *			up to end of ring w/o ring overlapping)
     *			in a circular ring we have for the consumer
     */
#define VRING_C_ROOM(rng)	((rng)->p_idx - (rng)->c_idx)
#define VRING_C_CROOM(rng)	(VRING_SIZE - VRING_POS((rng)->c_idx))

/*
 *  Here's we are consumer
 */

typedef struct Vdev {
    int		enabled;	/* flag: device has all resources allocated */
    NkDevVlink* vlink;		/* consumer/producer link */
    VRing*	vring;		/* consumer/producer circular ring */
    NkXIrq	xirq;		/* cross interrupt number */
    NkXIrqId    xid;            /* cross interrupt id */
    struct input_dev *inputdev;
    VIdev*      videv;          /* input device configuration */
} Vdev;

static int      vdev_num;	/* total number of communication links */
static Vdev*    vdevs;		/* pointer to array of device descriptors */
static NkXIrqId vsysconf_id;	/* xirq id for sysconf handler */

    static void
vdev_sysconf_trigger(Vdev* dev)
{
#ifdef TRACE
    printk(VLINK "Sending sysconf OS#%d(%d)->OS#%d(%d)\n",
	   dev->vlink->c_id,
	   dev->vlink->c_state,
	   dev->vlink->s_id,
	   dev->vlink->s_state);
#endif
    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, dev->vlink->s_id);
}

static void vdev_get_config(VIdev* videv, const char *name, struct input_dev *dev)
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
		dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		dev->keybit[BIT_WORD(BTN_TOUCH)] |= BIT_MASK(BTN_TOUCH);
		dev->absbit[0] |= BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) | BIT_MASK(ABS_PRESSURE);

		size = (ABS_CNT > VEVENT_ABS_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_ABS_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(ABS_CNT));
		memcpy(dev->absbit, videv->absbit, size);

		size = (ABS_CNT > VEVENT_ABS_CNT) ? VEVENT_ABS_CNT : ABS_CNT;
		if (dev->absinfo) {
			int i;
			for (i = 0 ; i < size ; i++) {
				dev->absinfo[i].maximum = videv->absmax[i];
				dev->absinfo[i].minimum = videv->absmin[i];
				dev->absinfo[i].fuzz    = videv->absfuzz[i];
				dev->absinfo[i].flat    = videv->absflat[i];
			}
		}
	}
	else if (strcmp(name, VKP_NAME) == 0) {
		/* Virtual Keypad */
		int i;
		dev->evbit[0] |= BIT_MASK(EV_KEY);
		for (i = 0; i < KEY_CNT; i++)
			__set_bit(i, dev->keybit);
	}  
        else  if (strcmp(name, VMS_NAME) == 0) {		/* Virtual Mouse */
		dev->evbit[0]   |=  BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
		dev->keybit[BIT_WORD(BTN_LEFT)] |= BIT_MASK(BTN_LEFT);
		dev->keybit[BIT_WORD(BTN_RIGHT)] |= BIT_MASK(BTN_RIGHT);
		dev->relbit[0]  =  BIT_MASK(REL_X) | BIT_MASK(REL_Y);
    	}
	else {
		/* combined device */
		int i = 0;
		size = (EV_CNT > VEVENT_EV_CNT) ?
				BITS_TO_LONGS(VEVENT_EV_CNT) :
				BITS_TO_LONGS(EV_CNT);
		for (i = 0; i < size; i++) {
			dev->evbit[i] |= videv->evbit[i];
		}

		size = (KEY_CNT > VEVENT_KEY_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_KEY_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(KEY_CNT));
		memcpy(dev->keybit, videv->keybit, size);

		size = (REL_CNT > VEVENT_REL_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_REL_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(REL_CNT));
		memcpy(dev->relbit, videv->relbit, size);

		size = (ABS_CNT > VEVENT_ABS_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_ABS_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(ABS_CNT));
		memcpy(dev->absbit, videv->absbit, size);

		size = (MSC_CNT > VEVENT_MSC_CNT) ? (sizeof(unsigned long)
				* BITS_TO_LONGS(VEVENT_MSC_CNT)) : (sizeof(unsigned long)
				* BITS_TO_LONGS(MSC_CNT));
		memcpy(dev->mscbit, videv->mscbit, size);

		size = (LED_CNT > VEVENT_LED_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_LED_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(LED_CNT));
		memcpy(dev->ledbit, videv->ledbit, size);

		size = (SND_CNT > VEVENT_SND_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_SND_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(SND_CNT));
		memcpy(dev->sndbit, videv->sndbit, size);

		size = (FF_CNT > VEVENT_FF_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_FF_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(FF_CNT));
		memcpy(dev->ffbit, videv->ffbit, size);

		size = (SW_CNT > VEVENT_SW_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_SW_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(SW_CNT));
		memcpy(dev->swbit, videv->swbit, size);

		size = (KEY_CNT > VEVENT_KEY_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_KEY_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(KEY_CNT));
		memcpy(dev->key, videv->key, size);

		size = (LED_CNT > VEVENT_LED_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_LED_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(LED_CNT));
		memcpy(dev->led, videv->led, size);

		size = (SND_CNT > VEVENT_SND_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_SND_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(SND_CNT));
		memcpy(dev->snd, videv->snd, size);

		size = (SW_CNT > VEVENT_SW_CNT) ?
				(sizeof(unsigned long) * BITS_TO_LONGS(VEVENT_SW_CNT)) :
				(sizeof(unsigned long) * BITS_TO_LONGS(SW_CNT));
		memcpy(dev->sw, videv->sw, size);

		size = (ABS_CNT > VEVENT_ABS_CNT) ? VEVENT_ABS_CNT : ABS_CNT;
		if (dev->absinfo) {
			int i;
			for (i = 0 ; i < size ; i++) {
				dev->absinfo[i].value   = videv->abs[i];
				dev->absinfo[i].maximum = videv->absmax[i];
				dev->absinfo[i].minimum = videv->absmin[i];
				dev->absinfo[i].fuzz    = videv->absfuzz[i];
				dev->absinfo[i].flat    = videv->absflat[i];
			}
		}
	}
}

    static int
vdev_handshake (Vdev* dev)
{
    volatile int* my_state;
    int  peer_state;

    my_state   = &dev->vlink->c_state;
    peer_state =  dev->vlink->s_state;

#ifdef TRACE
    printk(VLINK "handshake OS#%d(%d)->OS#%d(%d) [dev=%x]\n",
        dev->vlink->s_id, peer_state,
	    dev->vlink->c_id, *my_state,
	    dev);
#endif

    switch (*my_state) {
	case NK_DEV_VLINK_OFF:
	    if (peer_state != NK_DEV_VLINK_ON) {
		dev->vring->c_idx = 0;
		*my_state = NK_DEV_VLINK_RESET;
		vdev_sysconf_trigger(dev);
	    }
	    break;
	case NK_DEV_VLINK_RESET:
	    if (peer_state != NK_DEV_VLINK_OFF) {
		*my_state = NK_DEV_VLINK_ON;
                vdev_get_config(dev->videv, dev->inputdev->name, dev->inputdev);
		vdev_sysconf_trigger(dev);
	    }
	    break;
	case NK_DEV_VLINK_ON:
	    if (peer_state == NK_DEV_VLINK_OFF) {
		dev->vring->c_idx = 0;
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
    Vdev* dev;
    int   i;

    for (i = 0, dev = vdevs; i < vdev_num; i++, dev++) {
	if (dev->enabled) {
	    vdev_handshake (dev);
	}
    }
}

    static void
vdev_xirq_hdl (void* cookie, NkXIrq xirq)
{
    Vdev*         vdev  = (Vdev*)cookie;
    VRing*	  cring = vdev->vring;
    input_vevent* pevent;

    while (VRING_C_ROOM(cring)) {

	pevent = &VRING_PTR(cring, c_idx);

#ifdef TRACE
    	printk(VLINK "input_event(%d) type %x code %x value %x\n",
	    VRING_POS(cring->c_idx),pevent->type, pevent->code, pevent->value);
#endif

        input_event(vdev->inputdev, (unsigned int)(pevent->type),
		(unsigned int)(pevent->code), (int)(pevent->value));

        cring->c_idx++;
    }

    return;
}

    static void
vdev_init (Vdev* dev)
{
    NkDevVlink* 	vlink = dev->vlink;
    NkPhAddr		plink = nkops.nk_vtop(vlink);
    VRing*     		vring;
    struct input_dev*	inputdev;
    char*               vname;
    NkPhAddr		pdata;
    NkXIrq		xirq;

    vname = (vlink->s_info)?nkops.nk_ptov(vlink->s_info):0;
    if (!vname) {
	printk(VLINK "missing vlink server name\n" );
	return;
    }

        /*
         * Allocate communication ring and input dev config
         */
    pdata = nkops.nk_pdev_alloc(plink, 0, sizeof(VRing) + sizeof(VIdev));
    if (pdata == 0) {
	printk(VLINK "OS#%d->OS#%d link=%d ring alloc failed\n",
	       vlink->s_id, vlink->c_id, vlink->link);
	return;
    }

    vring = nkops.nk_ptov(pdata);
    dev->vring = vring;
    dev->videv = nkops.nk_ptov(pdata + sizeof(VRing));

	/*
	 * Allocate persistent cross interrupt.
	 */
    xirq = nkops.nk_pxirq_alloc(plink, 0, vlink->c_id, 1);
    if (xirq == 0) {
	printk(VLINK"OS#%d->OS#%d link=%d xirq alloc failed\n",
	       vlink->s_id, vlink->c_id, vlink->link);
	return;
    }

    dev->xirq = xirq;

    inputdev = input_allocate_device();
    if (!inputdev) {
    	printk(KERN_ERR "not enough memory for input device\n");
    	return;
    }

    inputdev->name = vname;
    inputdev->id.bustype = BUS_VIRTUAL;
    input_alloc_absinfo(inputdev);

	//
	// The VKBD attributes have to set at registration time
	// in order to match the VT consol keyboard (drivers/char/keyboard.c)
	//
    if (!strcmp(vname, VKP_NAME)) {
	int i;
        __set_bit(EV_KEY, inputdev->evbit);
	for (i = 0; i < KEY_CNT; i++) {
	    __set_bit(i, inputdev->keybit);
	}  
    }

    dev->inputdev = inputdev;

    if (input_register_device(dev->inputdev)) {
    	printk(KERN_ERR "cannot register input device\n");
    	return;
    }

    /*
     * Attach cross interrupt handler
     */
    dev->xid = nkops.nk_xirq_attach(dev->xirq, vdev_xirq_hdl, dev);
    if (dev->xid == 0) {
	printk(VLINK "OS#%d->OS#%d link=%d cannot attach consumer xirq\n",
	    vlink->s_id, vlink->c_id, vlink->link);
	return;
    }
    /*
     * Say this device has all "permanent" resources allocated
     */
    dev->enabled = 1;

    /*
     * perform handshake until both links are ready
     */
    vdev_handshake (dev);

    printk(VLINK "vdev_init(%d) %s loaded\n", vlink->link, vname);
}

    static int
vinput_module_init (void)
{
    NkPhAddr    plink;
    NkDevVlink* vlink;
    Vdev*	vdev;
    int 	ret;
    NkOsId      my_id = nkops.nk_id_get();

	/*
	 * Find how many communication links
	 * should be managed by this driver
	 */
    vdev_num = 0;
    plink    = 0;

    while ((plink = nkops.nk_vlink_lookup("vevent", plink))) {
	vlink = nkops.nk_ptov(plink);
	if (vlink->c_id == my_id) {
	    vdev_num += 1;
	}
    }
	/*
	 * Nothing to do if no links found
	 */
    if (vdev_num == 0) {
	printk(VLINK "no vevent vlinks found\n");
	return -EINVAL;
    }
	/*
	 * Allocate memory for all device descriptors
	 */
    vdevs = (Vdev*)kzalloc(sizeof(Vdev) * vdev_num, GFP_KERNEL);
    if (vdevs == 0) {
	return -ENOMEM;
    }
	/*
	 * Attach sysconfig handler
	 */
    vsysconf_id = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF, vsysconf_hdl, 0);
    if (vsysconf_id == 0) {
	ret = -ENOMEM;
	goto cleanup_1;
    }
	/*
	 * Find all consumer links for this OS
	 */
    vdev = vdevs;
    plink = 0;

    while ((plink = nkops.nk_vlink_lookup("vevent", plink))) {
	vlink = nkops.nk_ptov(plink);
	if (vlink->c_id == my_id) {
	    vdev->vlink = vlink;
	    vdev_init(vdev);
	    vdev += 1;
	}
    }

    printk(VLINK "found %d vlinks\n", vdev_num);
    return 0;

cleanup_1:
    kfree(vdevs);

    return ret;
}

    static void
vinput_module_exit (void)
{
    Vdev*	vdev;
    int		i;

	/*
	* Free memory allocated for device descriptors
	*/
    for (i = 0, vdev = vdevs; i < vdev_num; i++, vdev++) {
	NkDevVlink* vlink = vdev->vlink;

	vdev->enabled = 0;

	/*
	 * Detach cross interrupt handler
	 */
	nkops.nk_xirq_detach(vdev->xid);

	/*
	 * Say we are off
	 */
	vlink->c_state = NK_DEV_VLINK_OFF;
	vdev_sysconf_trigger(vdev);

	input_unregister_device(vdev->inputdev);

	if (vdev->inputdev) {
		input_free_device(vdev->inputdev);
		vdev->inputdev = NULL;
	}
    }

    kfree(vdevs);

	/*
	 * Detach sysconfig handler
	 */
    nkops.nk_xirq_detach(vsysconf_id);

}

module_init(vinput_module_init);
module_exit(vinput_module_exit);

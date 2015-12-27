/*
 ****************************************************************
 *
 *  Component: VLX VLCD frontend driver
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

/*----- System header files -----*/

#include <linux/version.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fb.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/leds.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_ARCH_OMAP
#include <asm/uaccess.h>
#include <linux/omapfb.h>
#endif

/*----- Local configuration -----*/

#if 0
#define VLCD_DEBUG
#endif

#if 0
#define VLCD_FORCED_COMPILE
#endif

#ifdef VLCD_FORCED_COMPILE
#define CONFIG_VLCD_PWR_TEST
#endif

/*----- Local header files -----*/

#include <vlx/vlcd_common.h>

/*----- Configuration dependent header files -----*/

#ifdef CONFIG_VLCD_PWR_TEST
#ifdef VLCD_FORCED_COMPILE
#include "../staging/android/timed_output.h"
#else
#include "timed_output.h"
#endif
#endif

/*----- Tracing -----*/

#define TRACE(format, args...)	 printk ("VLCD-FE: " format, ## args)
#define WFTRACE(format, args...) TRACE  ("[W] %s: " format, __func__, ## args)
#define ETRACE(format, args...)	 TRACE  ("[E] " format, ## args)
#define EFTRACE(format, args...) ETRACE ("%s: " format, __func__, ## args)

#ifdef VLCD_DEBUG
static unsigned vlcd_debug;

#define VLCD_DEBUG_HANDSHAKE	(1 << 0)
#define VLCD_DEBUG_SET_PAR	(1 << 1)
#define VLCD_DEBUG_SETCMAP	(1 << 2)
#define VLCD_DEBUG_CHECK_VAR	(1 << 3)
#define VLCD_DEBUG_PAN_DISPLAY	(1 << 4)
#define VLCD_DEBUG_SETCOLREG	(1 << 5)

#define DTRACE(format, args...)	TRACE("%s: " format, __func__, ## args)
#define CCALL(type, action) \
	if (vlcd_debug & VLCD_DEBUG_##type) action
#define CTRACE(type, format, args...) \
	CCALL (type, TRACE("%s: " format, __func__, ## args))
#else
#define DTRACE(format, args...)	        do {} while(0)
#define CCALL(type, action)	        do {} while(0)
#define CTRACE(type, format, args...)	do {} while(0)
#endif

/*----- Single device definition -----*/

    /*
     * Not supported:
     *  - rotate
     *  - non standard pixel format
     *  - PSEUDO and DIRECT color mode
     *  - more than one configuration for each bpp
     *  - transparency
     *  - backend restart
     */

#define VLCD_COLORS_IN_PALETTE 16

#define VLCD_FB_FLIP_BUFFER
#ifdef  VLCD_FB_FLIP_BUFFER
#define VLCD_FB_FLIP_BUFFER_MAX 2
#else
#define VLCD_FB_FLIP_BUFFER_MAX 1
#endif

typedef struct vlcd_device {
    NkDevVlcd* common;		/* Part shared with frontend */
    struct fb_info* fbinfo;	/* Framebuffer information */
    NkDevVlink* vlink;		/* Vlink used by this device */
    bool is_init;		/* Set if init has been completed once */
    bool handshake_finished;	/* Set if handshake is finished */
    bool partial_update;        /* Set if partial updates are supported */
    struct vlcd_device* next;	/* Next device in list */
#ifdef CONFIG_VLCD_PWR_TEST
    struct timed_output_dev timed_output_dev;
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend;
#endif
    unsigned int flip_buffer_max; /* maximum number of flip buffers */
    unsigned int refresh_hz;	  /* VLCD refresh rate */
    int          registerID;      /* frame buffer registration ID */

     /* FE<->BE synchronization */
    wait_queue_head_t sxirq_wait;
    NkXIrq            sxirq;
    NkXIrqId          sxid;
    unsigned int      sync;     /* synchronization mode */
} vlcd_device_t;

static long _refresh_jiffies;

static const char *vlcd_sync_name[] = VLCD_SYNC_NAME;

/*----- Single device code -----*/

    static void
vlcd_dev_sysconf_trigger (const vlcd_device_t* vlcd_dev)
{
    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, vlcd_dev->vlink->s_id);
}

    static int
vlcd_wait_for_idle_wakeup (vlcd_device_t* vlcd_dev)
{
    NkDevVlcd* shared = vlcd_dev->common;
    int wakeup        = (shared->modified == 0);

    DTRACE("client %u, modified 0x%02x -> wakeup %d\n",
      vlcd_dev->vlink->c_id, shared->modified, wakeup);

    return wakeup;
}

    static int
vlcd_wait_for_idle (vlcd_device_t* vlcd_dev)
{
    NkDevVlcd* shared = vlcd_dev->common;

    DTRACE("client %u, modified 0x%02x -> waiting for idle\n",
            vlcd_dev->vlink->c_id, shared->modified);

    /*
     * If BE/FE synchronization is enabled, wait for the
     * backend to be idle (i.e. all "modified" events cleared)
     */
    if (vlcd_dev->sync == VLCD_SYNC_POLL) {
	while (vlcd_wait_for_idle_wakeup(vlcd_dev) == 0) {
	    msleep_interruptible(1);
	}
    }
    else
    if (vlcd_dev->sync == VLCD_SYNC_INTR) {
	for (;;) {
	    if (wait_event_interruptible(vlcd_dev->sxirq_wait,
		    vlcd_wait_for_idle_wakeup(vlcd_dev))) {
		DTRACE("client %u, modified 0x%02x -> interrupted\n",
		       vlcd_dev->vlink->c_id, shared->modified);
		return -EINTR;
	    }
	    if (shared->modified == 0) {
		break;
	    }
	}
    }

    DTRACE("client %u, modified 0x%02x -> now idle\n",
           vlcd_dev->vlink->c_id, shared->modified);

    return 0;
}

    static void
vlcd_dev_send_event (vlcd_device_t* vlcd_dev, const nku32_f cmd)
{
    NkDevVlcd* shared = vlcd_dev->common;

#ifdef VLCD_DEBUG
    static unsigned long event_count = 0;
    event_count++;
#endif

    DTRACE("client %u about sending event #%lu, cmd 0x%02x, modified 0x%02x\n",
           vlcd_dev->vlink->c_id, event_count, cmd, shared->modified);

    /* The backend should be already idle */
    if (vlcd_wait_for_idle(vlcd_dev)) {
	/* We got interrupted */
	return;
    }

    DTRACE("client %u sending event #%lu, cmd 0x%02x, modified 0x%02x "
           "-> firing xirq to backend\n",
           vlcd_dev->vlink->c_id, event_count, cmd, shared->modified);

    nkops.nk_atomic_set (&shared->modified, cmd);
    nkops.nk_xirq_trigger(shared->cxirq, vlcd_dev->vlink->s_id);

    DTRACE("client %u sent event #%lu, cmd 0x%02x, modified 0x%02x "
           "-> waiting for completion\n",
           vlcd_dev->vlink->c_id, event_count, cmd, shared->modified);

    /* Wait for the backend to be idle again */
    if ((cmd == VLCD_EVT_INIT) || vlcd_wait_for_idle(vlcd_dev)) {
	/* We got interrupted */
	return;
    }

    DTRACE("client %u sent event #%lu, cmd 0x%02x, modified 0x%02x "
           "-> completed\n",
           vlcd_dev->vlink->c_id, event_count, cmd, shared->modified);
}

#if defined CONFIG_VLCD_PWR_TEST || defined CONFIG_HAS_EARLYSUSPEND
    static void
vlcd_dev_switch_off (vlcd_device_t* vlcd_dev, const bool yes)
{
    NkDevVlcd* shared = vlcd_dev->common;

    TRACE("switch-off %d link %d\n", yes, vlcd_dev->vlink->link);
    if (yes) {
	shared->flags |=  VLCD_FLAGS_SWITCH_OFF;
    } else {
	shared->flags &= ~VLCD_FLAGS_SWITCH_OFF;
    }
    vlcd_dev_send_event (vlcd_dev, VLCD_EVT_FLAGS_MODIFIED);
}
#endif

#ifdef CONFIG_VLCD_PWR_TEST
    static void
vlcd_to_enable (struct timed_output_dev* dev, int value)
{
    vlcd_device_t* vlcd_dev = container_of (dev, vlcd_device_t,
					    timed_output_dev);
    vlcd_dev_switch_off (vlcd_dev, !value);
}

    static int
vlcd_to_get_time (struct timed_output_dev* dev)
{
    return 0;
}
#endif	/* CONFIG_VLCD_PWR_TEST */

#ifdef CONFIG_HAS_EARLYSUSPEND
    static void
vlcd_dev_es_suspend (struct early_suspend* es)
{
    vlcd_device_t* vlcd_dev = container_of (es, vlcd_device_t, early_suspend);

    vlcd_dev_switch_off (vlcd_dev, 1);
}

    static void
vlcd_dev_es_resume (struct early_suspend* es)
{
    vlcd_device_t* vlcd_dev = container_of (es, vlcd_device_t, early_suspend);

    vlcd_dev_switch_off (vlcd_dev, 0);
}
#endif	/* CONFIG_HAS_EARLYSUSPEND */

    /*
     * Make handshake with backend. Returns:
     *  - 0 if handshake is not finished
     *  - 1 if handshake is finished
     */

    static int
vlcd_dev_handshake (vlcd_device_t* vlcd_dev)
{
    volatile int* my_state   = &(vlcd_dev->vlink->c_state);
    int           peer_state =   vlcd_dev->vlink->s_state;

    CTRACE (HANDSHAKE, "me %d peer %d\n", *my_state, peer_state);
    switch (*my_state) {
    case NK_DEV_VLINK_OFF:
        if (peer_state != NK_DEV_VLINK_ON) {
            *my_state = NK_DEV_VLINK_RESET;
            vlcd_dev_sysconf_trigger (vlcd_dev);
        }
        break;

    case NK_DEV_VLINK_RESET:
        if (peer_state != NK_DEV_VLINK_OFF) {
            *my_state = NK_DEV_VLINK_ON;
            vlcd_dev_sysconf_trigger (vlcd_dev);
        }
        break;

    case NK_DEV_VLINK_ON:
        if (peer_state == NK_DEV_VLINK_OFF) {
		/* Backend has restarted */
		/* TODO: not tested */
            WFTRACE("Backend restarted. Not tested\n");
            *my_state = NK_DEV_VLINK_OFF;
            vlcd_dev_sysconf_trigger (vlcd_dev);
        }
        break;
    }
    return ((*my_state  == NK_DEV_VLINK_ON) &&
            (peer_state == NK_DEV_VLINK_ON));
}

#define VLCD_MAX(a, b) ((a) < (b) ? (b) :( a))

    /* Called only from vlcd_thread() */

    static int
vlcd_dev_end_init (vlcd_device_t* vlcd_dev)
{
    int retval;
    NkDevVlcd* vlcd_common = vlcd_dev->common;
    vlcd_pconf_t* pconf = vlcd_common->pconf;
    vlcd_conf_t* conf = &vlcd_common->current_conf;
    struct fb_info* info = vlcd_dev->fbinfo;
    struct fb_fix_screeninfo* fix = &info->fix;
    struct fb_var_screeninfo* var = &info->var;
    vlcd_color_conf_t* color_config = &pconf->color_conf;
    unsigned long max_zone_size;
    void* vaddr;
    int i;

    DTRACE("link %i\n", vlcd_dev->vlink->link);

	/* Verify information coming from backend */
    if ((color_config->bpp == 0) || (pconf->xres == 0) || (pconf->yres == 0)) {
        EFTRACE("Error in backend. First config not set or badly set!\n"
                "xres %u, yres %u, bitsPerPixel %u\n",
		pconf->xres, pconf->yres, color_config->bpp);
        return -EFAULT;
    }
    if (vlcd_dev->is_init == 0) {

	    /* Set fbinfo parameters */
        var->bits_per_pixel  = color_config->bpp;
        var->red.offset      = color_config->red_offset;
        var->red.length      = color_config->red_length;
        var->red.msb_right   = color_config->align;
        var->green.offset    = color_config->green_offset;
        var->green.length    = color_config->green_length;
        var->green.msb_right = color_config->align;
        var->blue.offset     = color_config->blue_offset;
        var->blue.length     = color_config->blue_length;
        var->blue.msb_right  = color_config->align;
        var->xres            = pconf->xres;
        var->yres            = pconf->yres;
        var->xres_virtual    = pconf->xres;
        var->yres_virtual    = pconf->yres;

	    /* Allocate DMA zone */
        max_zone_size = 0;
        for (i = 0; i< VLCD_MAX_CONF_NUMBER; i++) {
            max_zone_size = VLCD_MAX(max_zone_size, pconf[i].yres *
				  (vlcd_get_line_length(var->xres_virtual,
						        var->bits_per_pixel)));
	}
	max_zone_size = PAGE_ALIGN(max_zone_size);

	for (i = vlcd_dev->flip_buffer_max; i > 0; i--) {
	    conf->dma_zone_size = i * max_zone_size;

            DTRACE("dma_size %i\n", conf->dma_zone_size);

	    if (conf->dma_zone_size < 4 * 1024 * 1024) {
                vaddr = dma_alloc_writecombine(NULL, conf->dma_zone_size,
					       &(conf->dma_zone_paddr), GFP_KERNEL);
	    } else {
	        conf->dma_zone_paddr = nkops.nk_pmem_alloc(
		    nkops.nk_vtop(vlcd_dev->vlink), 0, conf->dma_zone_size);
		vaddr = conf->dma_zone_paddr ?
		    nkops.nk_mem_map(conf->dma_zone_paddr, conf->dma_zone_size) : NULL;
	    }

            if (vaddr == NULL) {
		if (i > 1) {
		    WFTRACE("DMA alloc failed of %d buffers\n", i);
		    continue;
		}
		EFTRACE("Cannot allocate DMA zone. "
			"Asked for %lu bytes, link %i\n",
			(unsigned long)(conf->dma_zone_size),
			vlcd_dev->vlink->link);
                return -ENOMEM;
	    }

	    DTRACE("dma_alloc OK. paddr 0x%x, vaddr 0x%lx size=0x%lX\n",
	      conf->dma_zone_paddr, (unsigned long)vaddr, max_zone_size);

            fix->smem_len     = conf->dma_zone_size;
            fix->smem_start   = conf->dma_zone_paddr;
            info->screen_base = vaddr;
            fix->line_length  = vlcd_get_line_length(var->xres_virtual,
						     var->bits_per_pixel);

            var->yres_virtual    = i * pconf->yres;
            if (i > 1) {
		    /* multi-screen => activate panning */
		fix->ypanstep = 1;
	    }
	    break;
	}

	/* Does the backend support partial updates? */
	if (vlcd_common->caps & VLCD_CAPS_PARTIAL_UPDATE) {

	    TRACE("enabling partial update\n");

	    vlcd_dev->partial_update = 1;

	    /* Advert 'partial update' capability */
	    fix->reserved[0] = 0x5444; /* UPDT */
	    fix->reserved[1] = 0x5055;
	    fix->reserved[2] = 0;

	    /* Current 'partial update' request */
	    var->reserved[0] = 0;
	    var->reserved[1] = 0;
	    var->reserved[2] = 0;
	}
    }
	/* Copy information in current configuration */
    conf->xres                    = var->xres;
    conf->yres                    = var->yres;
    conf->xres_virtual            = var->xres_virtual;
    conf->yres_virtual            = var->yres_virtual;
    conf->xoffset                 = var->xoffset;
    conf->yoffset                 = var->yoffset;
    conf->color_conf.bpp          = var->bits_per_pixel;
    conf->color_conf.red_offset   = var->red.offset;
    conf->color_conf.red_length   = var->red.length;
    conf->color_conf.green_offset = var->green.offset;
    conf->color_conf.green_length = var->green.length;
    conf->color_conf.blue_offset  = var->blue.offset;
    conf->color_conf.blue_length  = var->blue.length;
    conf->color_conf.align        = var->red.msb_right;
    vlcd_dev_send_event (vlcd_dev, VLCD_EVT_INIT);

    vlcd_dev->is_init = 1;
#ifdef CONFIG_FB_VLCD_BACKEND
    retval = vlcdRegisterFB(info, vlcd_dev->vlink->link);
#else
    retval = vlcdRegisterFB(info, vlcd_dev->registerID);
#endif
    if (retval < 0) {
	EFTRACE("vlcdRegisterFB() failed %d\n", retval);
    }
    return retval;
}

    /*
     * Remove device from list. Return new beginning of list (can be NULL).
     */

    static vlcd_device_t*
vlcd_dev_remove (vlcd_device_t* list, vlcd_device_t* to_remove)
{
    vlcd_device_t* dev = list;
    vlcd_device_t* prev = NULL;
    vlcd_device_t* result = NULL;

    while (dev != NULL) {
        if (dev == to_remove) {
            if (prev == NULL) {
                result = dev->next;
                break;
            } else {
                result = list;
                prev->next = dev->next;
            }
        }
        prev = dev;
        dev = dev->next;
    }
    return result;
}

    /*
     * Get device with the smallest link id in list.
     */

    static vlcd_device_t* __init
vlcd_dev_get_first (vlcd_device_t* list)
{
    int min_link = 0x7FFFFFFF;
    vlcd_device_t* dev = list;
    vlcd_device_t* first_dev = NULL;

    while (dev != NULL) {
        if (min_link > dev->vlink->link) {
            min_link = dev->vlink->link;
            first_dev = dev;
        }
        dev = dev->next;
    }
    return first_dev;
}

    /*
     * Get device with the smallest link id superior to link id of
     * given device in list. Return NULL if not found.
     */

    static vlcd_device_t* __init
vlcd_dev_get_next (vlcd_device_t* list, vlcd_device_t* cur)
{
    int link = cur->vlink->link;
    int min_link = 0x7FFFFFFF;
    vlcd_device_t* dev = list;
    vlcd_device_t* next_dev = NULL;

    while (dev != NULL) {
        if ((min_link > dev->vlink->link) && (dev->vlink->link > link)) {
            min_link = dev->vlink->link;
            next_dev = dev;
        }
        dev = dev->next;
    }
    return next_dev;
}

/*----- Framebuffer callbacks -----*/

    static int
vlcd_fb_open (struct fb_info* info, int user)
{
    vlcd_device_t* vlcd_dev = info->par;

    DTRACE("link %i is_init %i\n", vlcd_dev->vlink->link, vlcd_dev->is_init);
    if (vlcd_dev->is_init) {
	return 0;
    }
        /* Initialization not finished yet. */
    return -EACCES;
}

    static int
vlcd_fb_release (struct fb_info* info, int user)
{
    vlcd_device_t* vlcd_dev = info->par;

    (void) vlcd_dev;
    DTRACE("link %i\n", vlcd_dev->vlink->link);
    return 0;
}

    static int
vlcd_fb_blank (int blank, struct fb_info* info)
{
    vlcd_device_t* vlcd_dev = info->par;

    (void) vlcd_dev;
    DTRACE("link %i\n", vlcd_dev->vlink->link);
    return 0;
}

    static int
vlcd_fb_check_var (struct fb_var_screeninfo* var, struct fb_info* info)
{
    vlcd_device_t* vlcd_dev = info->par;
    vlcd_pconf_t* hw_possible_conf = vlcd_dev->common->pconf;
    nku8_f bpp = 0;
    int i, conf_num;
    vlcd_color_conf_t* color_config;

    CTRACE (CHECK_VAR, "link %i\n", vlcd_dev->vlink->link);
    if ((var->nonstd != 0) || (var->rotate != 0)) {
	EFTRACE("wrong rotate or non-std param\n");
        return -EINVAL;
    }
    if (var->bits_per_pixel == 0) {
        EFTRACE("bits per pixel can't be 0\n");
        return -EINVAL;
    }
	/* Find color config */
    conf_num = -1;
    for (i = 0; i < VLCD_MAX_CONF_NUMBER; i++) {
        if (var->bits_per_pixel == hw_possible_conf[i].color_conf.bpp) {
            conf_num = i;
            bpp = var->bits_per_pixel;
            if ((var->xres == hw_possible_conf[i].xres) &&
                (var->yres == hw_possible_conf[i].yres)) {
		    /*
		     * This is a "perfect" configuration:
		     * bpp, xres and yres are OK. Stop looping.
		     */
		break;
            }
        }
    }
    if (conf_num == -1) {
        ETRACE("bits_per_pixel not supported\n");
        return -EINVAL;
    }
    var->xres = hw_possible_conf[conf_num].xres;
    var->yres = hw_possible_conf[conf_num].yres;

    if (var->xres_virtual < var->xres) {
        var->xres_virtual = var->xres;
    }
    if (var->yres_virtual < var->yres) {
        var->yres_virtual = var->yres;
    }
	/* Check size of screen */
    if (var->xres_virtual * var->yres_virtual * bpp / 8 > info->fix.smem_len) {
        EFTRACE("wrong size params\n");
        return -EINVAL;
    }
	/* Adjust offset if necessary */
    if (var->xres + var->xoffset > var->xres_virtual) {
        var->xoffset = var->xres_virtual - var->xres;
    }
    if (var->yres + var->yoffset > var->yres_virtual) {
        var->yoffset = var->yres_virtual - var->yres;
    }

    color_config = &(hw_possible_conf[conf_num].color_conf);
    var->blue.offset     = color_config->blue_offset;
    var->blue.length     = color_config->blue_length;
    var->blue.msb_right  = color_config->align;
    var->green.offset    = color_config->green_offset;
    var->green.length    = color_config->green_length;
    var->green.msb_right = color_config->align;
    var->red.offset      = color_config->red_offset;
    var->red.length      = color_config->red_length;
    var->red.msb_right   = color_config->align;

	/* Other fields */
    var->height       = -1;
    var->width        = -1;
    var->grayscale    = 0;
    var->pixclock     = 0;
    var->left_margin  = 0;
    var->right_margin = 0;
    var->upper_margin = 0;
    var->lower_margin = 0;
    var->hsync_len    = 0;
    var->vsync_len    = 0;
    var->vmode        = FB_VMODE_NONINTERLACED;
    var->sync         = 0;
	/* Not necessary to send an event to backend, nothing has changed yet */
    return 0;
}

#ifdef VLCD_DEBUG
    static void
vlcd_fb_dump_var (struct fb_var_screeninfo* var)
{
    DTRACE("size (%i*%i). virt_size (%i*%i). "
            "offset (%i*%i)\n",
            var->xres, var->yres, var->xres_virtual, var->yres_virtual,
            var->xoffset, var->yoffset);
    DTRACE("bpp:%i, grayscale:%i, red (%i, %i, %i), "
            "green (%i, %i, %i) blue (%i, %i, %i)\n",
            var->bits_per_pixel, var->grayscale, var->red.offset,
            var->red.length, var->red.msb_right, var->green.offset,
            var->green.length, var->green.msb_right, var->blue.offset,
            var->blue.length, var->blue.msb_right);
    DTRACE("nonstd:%i, activate:%i, height:%i, width:%i, "
            "accel_flags:%i\n",
            var->nonstd, var->activate, var->height, var->width,
            var->accel_flags);
    DTRACE("pixclock:%i, left_margin:%i, right_margin:%i, "
            "upper_margin:%i, lower_margin:%i, hsync_len:%i, vsync_len:%i\n",
            var->pixclock, var->left_margin, var->right_margin,
            var->upper_margin, var->lower_margin, var->hsync_len,
            var->vsync_len);
    DTRACE("sync:%i, vmode:%i, rotate:%i\n",
            var->sync, var->vmode, var->rotate);
}
#endif

    static int
vlcd_fb_set_par (struct fb_info* info)
{
    vlcd_device_t* vlcd_dev = info->par;
    vlcd_conf_t* hw_conf = &(vlcd_dev->common->current_conf);
    struct fb_var_screeninfo *var = &(info->var);

    CTRACE(SET_PAR, "link %i\n", vlcd_dev->vlink->link);
    CCALL (SET_PAR, vlcd_fb_dump_var (var));
    info->fix.line_length = vlcd_get_line_length(var->xres_virtual,
						 var->bits_per_pixel);
    hw_conf->xres         = var->xres;
    hw_conf->yres         = var->yres;
    hw_conf->xres_virtual = var->xres_virtual;
    hw_conf->yres_virtual = var->yres_virtual;
    hw_conf->xoffset      = var->xoffset;
    hw_conf->yoffset      = var->yoffset;
    hw_conf->color_conf.bpp          = var->bits_per_pixel;
    hw_conf->color_conf.red_offset   = var->red.offset;
    hw_conf->color_conf.red_length   = var->red.length;
    hw_conf->color_conf.green_offset = var->green.offset;
    hw_conf->color_conf.green_length = var->green.length;
    hw_conf->color_conf.blue_offset  = var->blue.offset;
    hw_conf->color_conf.blue_length  = var->blue.length;
    hw_conf->color_conf.align        = var->red.msb_right;
    vlcd_dev_send_event (vlcd_dev,
    			 VLCD_EVT_SIZE_MODIFIED | VLCD_EVT_COLOR_MODIFIED);
    return 0;
}

    static int
vlcd_set_col_reg (u_int regno, u_int red, u_int green, u_int blue,
		  u_int transp, struct fb_info* info)
{
    u32 v;

	/* Directcolor:
        *   var->{color}.offset contains start of bitfield
        *   var->{color}.length contains length of bitfield
        *   {hardwarespecific} contains width of RAMDAC
        *   cmap[X] is programmed to (X << red.offset) | (X << green.offset) |
        *                             (X << blue.offset)
        *   RAMDAC[X] is programmed to (red, green, blue)
        *
        * Pseudocolor:
        *    uses offset = 0 && length = RAMDAC register width.
        *    var->{color}.offset is 0
        *    var->{color}.length contains width of DAC
        *    cmap is not used
        *    RAMDAC[X] is programmed to (red, green, blue)
        * Truecolor:
        *    does not use DAC. Usually 3 are present.
        *    var->{color}.offset contains start of bitfield
        *    var->{color}.length contains length of bitfield
        *    cmap is programmed to (red << red.offset) |
        *                          (green << green.offset) |
        *                          (blue << blue.offset) |
        *                          (transp << transp.offset)
        *    RAMDAC does not exist
	*/
    /* We don't support transparency */

#define CNVT_TOHW(val,width) ((((val)<<(width))+0x7FFF-(val))>>16)

    red = CNVT_TOHW(red, info->var.red.length);
    green = CNVT_TOHW(green, info->var.green.length);
    blue = CNVT_TOHW(blue, info->var.blue.length);

#undef CNVT_TOHW

    v = (red << info->var.red.offset) |
            (green << info->var.green.offset) |
            (blue << info->var.blue.offset);

	/* We don't support DIRECTCOLOR and PSEUDOCOLOR don't use palette */
    if (regno >= 16) {
        return 1;
    }
    if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
        ((u32 *) (info->pseudo_palette))[regno] = v;
    }
    return 0;
}

    static int
vlcd_fb_setcolreg (u_int regno, u_int red, u_int green, u_int blue,
		   u_int transp, struct fb_info* info)
{
    vlcd_device_t* vlcd_dev = info->par;

    (void) vlcd_dev;
    CTRACE (SETCOLREG, "link %i\n", vlcd_dev->vlink->link);
    return vlcd_set_col_reg(regno, red, green, blue, transp, info);
}

    static int
vlcd_fb_setcmap (struct fb_cmap* cmap, struct fb_info* info)
{
    int count, r;
    int index   = cmap->start;
    u16* red    = cmap->red;
    u16* green  = cmap->green;
    u16* blue   = cmap->blue;
    u16* transp = cmap->transp;
    u16 trans = 0xffff;
    vlcd_device_t* vlcd_dev = info->par;

    (void) vlcd_dev;
    CTRACE(SETCMAP, "link %i\n", vlcd_dev->vlink->link);
    for (count = 0; count < cmap->len; count++) {
        if (transp) {
            trans = *transp++;
        }
        r = vlcd_set_col_reg(index++, *red++, *green++, *blue++, 0xffff, info);
        if (r != 0) {
	    break;
	}
    }
	/* No events sent to backend */
    return 0;
}

    static int
vlcd_fb_partial_update(struct fb_info *info,
                       uint32_t left, uint32_t top,
                       uint32_t right, uint32_t bottom)
{
    vlcd_device_t* vlcd_dev = info->par;
    NkDevVlcd* shared = vlcd_dev->common;

    shared->rect.left    = left;
    shared->rect.top     = top;
    shared->rect.right   = right - 1;
    shared->rect.bottom  = bottom - 1;

    CTRACE (PAN_DISPLAY, "PARTIAL_UPDATE: left %u, top %u, right %u, bottom %u\n",
      shared->rect.left, shared->rect.top, shared->rect.right, shared->rect.bottom);

    return 0;
}

    static int
vlcd_fb_pan_display (struct fb_var_screeninfo* var, struct fb_info* info)
{
    struct fb_var_screeninfo *currentVar = &(info->var);
    vlcd_device_t* vlcd_dev = info->par;
    vlcd_conf_t* hw_conf = &(vlcd_dev->common->current_conf);
    nku32_f rect_modified = 0;

    CTRACE (PAN_DISPLAY, "link %i\n", vlcd_dev->vlink->link);
	/* Adjust given offset if necessary */
    if (currentVar->xres + var->xoffset > currentVar->xres_virtual) {
	currentVar->xoffset = var->xres_virtual - currentVar->xres;
    } else {
	currentVar->xoffset = var->xoffset;
    }
    if (currentVar->yres + var->yoffset > currentVar->yres_virtual) {
	currentVar->yoffset = var->yres_virtual - currentVar->yres;
    } else {
	currentVar->yoffset = var->yoffset;
    }

    if (vlcd_dev->partial_update) {
	/* Partial update requested? */
	if (var->reserved[0] == 0x54445055) {
	    vlcd_fb_partial_update(info,
	      var->reserved[1] & 0xffff,
	      var->reserved[1] >> 16,
	      var->reserved[2] & 0xffff,
	      var->reserved[2] >> 16);
	} else {
	    /* Otherwise, request the whole screen to be updated */
	    vlcd_fb_partial_update(info,
	      0, 0, var->xres, var->yres);
	}
	rect_modified = VLCD_EVT_RECT_MODIFIED;
    }
    if (hw_conf->xoffset == currentVar->xoffset &&
	hw_conf->yoffset == currentVar->yoffset) {
	    /* Nothing to do */
	return 0;
    }
	/* Set HW param */
    hw_conf->xoffset = currentVar->xoffset;
    hw_conf->yoffset = currentVar->yoffset;

    vlcd_dev_send_event(vlcd_dev, rect_modified | VLCD_EVT_SIZE_MODIFIED);
    return 0;
}

#ifdef CONFIG_ARCH_OMAP
#ifdef VLCD_DEBUG
    static const char*
vlcd_get_ioctl_name (const unsigned int cmd)
{
#define VLCD_CASE(n)  case n: return #n

    switch (cmd) {
    VLCD_CASE (OMAPFB_MIRROR);
    VLCD_CASE (OMAPFB_SYNC_GFX);
    VLCD_CASE (OMAPFB_VSYNC);
    VLCD_CASE (OMAPFB_SET_UPDATE_MODE);
    VLCD_CASE (OMAPFB_GET_CAPS);
    VLCD_CASE (OMAPFB_GET_UPDATE_MODE);
    VLCD_CASE (OMAPFB_LCD_TEST);
    VLCD_CASE (OMAPFB_CTRL_TEST);
    VLCD_CASE (OMAPFB_UPDATE_WINDOW_OLD);
    VLCD_CASE (OMAPFB_SET_COLOR_KEY);
    VLCD_CASE (OMAPFB_GET_COLOR_KEY);
    VLCD_CASE (OMAPFB_SETUP_PLANE);
    VLCD_CASE (OMAPFB_QUERY_PLANE);
    VLCD_CASE (OMAPFB_UPDATE_WINDOW);
    VLCD_CASE (OMAPFB_SETUP_MEM);
    VLCD_CASE (OMAPFB_QUERY_MEM);
    default: break;
    }
    return "unknown";

#undef VLCD_CASE
}
#endif
#endif

    static int
vlcd_fb_ioctl (struct fb_info* info, unsigned int cmd, unsigned long arg)
{
#ifdef CONFIG_ARCH_OMAP
    vlcd_device_t* vlcd_dev = info->par;
    int ret = 0;
    struct omapfb_caps caps;

    (void) vlcd_dev;
    DTRACE("link %i: %s\n", vlcd_dev->vlink->link, vlcd_get_ioctl_name(cmd));
    switch (cmd) {
    case OMAPFB_GET_CAPS:
        caps.ctrl = 0x1077000;
        caps.plane_color = 0x1FB;
        caps.wnd_color = 5;
        if (copy_to_user((void __user *)arg, &caps, sizeof(caps))) {
            ret = -EFAULT;
        } else {
            DTRACE("link %i: responding to OMAPFB_GET_CAPS\n",
		   vlcd_dev->vlink->link);
        }
        break;

    default:
        ret = -EINVAL;
        break;
    }
    return ret;
#else
    vlcd_device_t* vlcd_dev = info->par;

    (void) vlcd_dev;
    DTRACE("link %i: 0x%08x\n", vlcd_dev->vlink->link, cmd);
    return -EINVAL;
#endif
}

static struct fb_ops vlcd_fb_ops = {
    .fb_open        = vlcd_fb_open,
    .fb_release     = vlcd_fb_release,
    .fb_blank       = vlcd_fb_blank,
    .fb_check_var   = vlcd_fb_check_var,
    .fb_set_par     = vlcd_fb_set_par,
    .fb_setcolreg   = vlcd_fb_setcolreg,
    .fb_setcmap     = vlcd_fb_setcmap,
    .fb_pan_display = vlcd_fb_pan_display,
    .fb_fillrect    = cfb_fillrect,
    .fb_copyarea    = cfb_copyarea,
    .fb_imageblit   = cfb_imageblit,
    .fb_ioctl       = vlcd_fb_ioctl
};

    static int __init
vlcd_dev_init (struct platform_device* plat_dev, vlcd_device_t* vlcd_dev)
{
    struct fb_info *info;
    int retval;
    struct fb_fix_screeninfo* fix;
    struct fb_var_screeninfo* var;

	/* Allocate framebuffer info */
    info = framebuffer_alloc(sizeof(u32) * 256, NULL /* &plat_dev->dev */);
    if (!info) {
	return -ENOMEM;
    }
    info->fbops = &vlcd_fb_ops;

    fix = &(info->fix);
    var = &(info->var);
    memset(fix, 0, sizeof(struct fb_fix_screeninfo));
    memset(var, 0, sizeof(struct fb_var_screeninfo));
    memcpy(fix->id, "vlcd-fe", 8);
	/* TRUE COLOR is the only supported mode */
    fix->visual = FB_VISUAL_TRUECOLOR;
    info->flags = FBINFO_FLAG_DEFAULT;
    info->par = vlcd_dev;
    vlcd_dev->fbinfo = info;

    info->pseudo_palette = kzalloc(sizeof(u32) * VLCD_COLORS_IN_PALETTE,
				   GFP_KERNEL);
    if (info->pseudo_palette == NULL) {
	EFTRACE("can't allocate palette memory\n");
        retval = -ENOMEM;
        goto error1;
    }
    retval = fb_alloc_cmap(&info->cmap, VLCD_COLORS_IN_PALETTE, 0);
    if (retval < 0) {
	EFTRACE("fb_alloc_cmap() returned %d\n", retval);
        goto error2;
    }
    DTRACE("OK link %i\n", vlcd_dev->vlink->link);
    return 0;

error2:
    kfree(info->pseudo_palette);
    info->pseudo_palette = NULL;
error1:
    framebuffer_release(vlcd_dev->fbinfo);
    vlcd_dev->fbinfo = NULL;
    return retval;
}

/*----- Driver type -----*/

typedef struct {
    vlcd_device_t* dev_list;	/* List of devices */
    NkXIrqId sysconf_xid;	/* To be able to detach handler */
    struct completion thread_completion;
    struct semaphore thread_semaphore;
    bool thread_active;
} vlcd_driver_t;

/*----- Driver code -----*/

    static int
vlcd_thread (void* cookie)
{
    int             finished;
    vlcd_driver_t*  vlcd_drv = (vlcd_driver_t*) cookie;
    vlcd_device_t*  vlcd_dev;
    vlcd_device_t*  next;
    int             ret;

    daemonize("vlcd-fe");
    for (;;) {
	if (_refresh_jiffies) {
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
  	    ret = down_timeout(&vlcd_drv->thread_semaphore, _refresh_jiffies);
#else
	    down(&vlcd_drv->thread_semaphore);
#endif
	} else {
	    down(&vlcd_drv->thread_semaphore);
	}
        if (!vlcd_drv->thread_active) {
            DTRACE("thread is terminating\n");
            break;
        }
        vlcd_dev = vlcd_drv->dev_list;
        while (vlcd_dev) {
            next = vlcd_dev->next;
            finished = vlcd_dev_handshake(vlcd_dev);
            if ((finished == 1) && (vlcd_dev->handshake_finished == 0)) {
                DTRACE("Calling vlcd_dev_end_init()\n");
                vlcd_dev->handshake_finished = 1;
                ret = vlcd_dev_end_init(vlcd_dev);
                if (ret != 0) {
                    EFTRACE("Error %i in vlcd_dev_end_init()\n", ret);
                    if (vlcd_dev->fbinfo != NULL) {
                        kfree(vlcd_dev->fbinfo->pseudo_palette);
                        fb_dealloc_cmap(&vlcd_dev->fbinfo->cmap);
                        framebuffer_release(vlcd_dev->fbinfo);
                        vlcd_dev->fbinfo = NULL;
                    }
			/* TODO: This should be atomic */
                    vlcd_drv->dev_list = vlcd_dev_remove(vlcd_drv->dev_list,
							 vlcd_dev);
                    vlcd_dev->vlink->c_state = NK_DEV_VLINK_OFF;
                    vlcd_dev_sysconf_trigger (vlcd_dev);
                    kfree(vlcd_dev);
		    vlcd_dev = 0;
                } else {
		    DTRACE("vlcd_dev_end_init() OK\n");
                }
            } else if (finished == 0) {
		vlcd_dev->handshake_finished = 0;
            }
            if (finished && vlcd_dev && vlcd_dev->handshake_finished &&
	        vlcd_dev->refresh_hz) {
		vlcd_conf_t* hw_conf = &(vlcd_dev->common->current_conf);
		if (hw_conf->color_conf.bpp) {
		    vlcd_dev_send_event(vlcd_dev, VLCD_EVT_SIZE_MODIFIED);
		}
	    }
            vlcd_dev = next;
        }
    }
    complete_and_exit(&vlcd_drv->thread_completion, 0);
}

    static void
vlcd_sysconf_intr_handler (void* cookie, NkXIrq xirq)
{
    vlcd_driver_t* vlcd_drv = (vlcd_driver_t*)cookie;

    up(&vlcd_drv->thread_semaphore);
}

    static int
vlcd_drv_disconnect (vlcd_driver_t* vlcd_drv, const bool inform_backend)
{
    vlcd_device_t* vlcd_dev;

	/* Detach from SYSCONF irq */
    if (vlcd_drv->sysconf_xid) {
	nkops.nk_xirq_detach(vlcd_drv->sysconf_xid);
	vlcd_drv->sysconf_xid = 0;
    }
	/* Disconnect and free devices */
    vlcd_dev = vlcd_drv->dev_list;
    while (vlcd_dev) {
	vlcd_device_t* next;

	if (inform_backend) {
		/* Tell backend we died */
	    vlcd_dev->vlink->c_state = NK_DEV_VLINK_OFF;
	    vlcd_dev_sysconf_trigger(vlcd_dev);
	}
	next = vlcd_dev->next;
	kfree(vlcd_dev);
	vlcd_dev = next;
    }
    vlcd_drv->dev_list = NULL;
    return 0;
}

    /*
     * Convertion from character string to integer number
     */
    static const char*
_a2ui (const char* s, unsigned int* i)
{
    unsigned int xi = 0;
    char         c  = *s;

    while (('0' <= c) && (c <= '9')) {
	xi = xi * 10 + (c - '0');
	c = *(++s);
    }

    if        ((*s == 'K') || (*s == 'k')) {
	xi *= 1024;
	s  += 1;
    } else if ((*s == 'M') || (*s == 'm')) {
	xi *= (1024*1024);
	s  += 1;
    }

    *i = xi;

    return s;
}

    static unsigned int
_flip_buffer_max (const char* info)
{
    unsigned int max;
    if (!info) {
	return VLCD_FB_FLIP_BUFFER_MAX;
    }
    info = _a2ui(info, &max);
    if ((*info && (*info != ',')) || !max) {
	return VLCD_FB_FLIP_BUFFER_MAX;
    }
    return max;
}

    static unsigned int
_refresh_hz (const char* info)
{
    unsigned int msecs;
    if (!info) {
	return 0;
    }
    while (*info && (*info != ',')) info++;
    if (!*info) {
	return 0;
    }
    info = _a2ui(info+1, &msecs);
    if (*info) {
	return 0;
    }
    return msecs;
}

    static unsigned int
_sync_mode (const char* info)
{
    DTRACE("vlcd info = %s\n", info);

    if (!info) {
	return VLCD_SYNC_NONE;
    }

    if (strstr(info, "intr")) {
	return VLCD_SYNC_INTR;
    }
    if (strstr(info, "poll")) {
	return VLCD_SYNC_POLL;
    }
    return VLCD_SYNC_NONE;
}

    static void
vlcd_sxirq_handler (void* cookie, NkXIrq xirq)
{
    vlcd_device_t* vlcd_dev = (vlcd_device_t*)cookie;

    DTRACE("client %u received xirq from backend\n", vlcd_dev->vlink->c_id);

    wake_up_interruptible(&vlcd_dev->sxirq_wait);
}

    static void
vlcd_intr_sync_mode (vlcd_device_t* vlcd_dev, NkPhAddr plink)
{
    NkDevVlink* vlink = nkops.nk_ptov(plink);

    /* Polling sync, or no sync requested? */
    if (vlcd_dev->sync != VLCD_SYNC_INTR) {
	return;
    }

    if (!(vlcd_dev->common->caps & VLCD_CAPS_SYNC_INTR)) {
	ETRACE("backend does not support interrupt mode; "
	       "falling back to polling mode\n");
	vlcd_dev->sync = VLCD_SYNC_POLL;
	return;
    }

    vlcd_dev->sxirq = nkops.nk_pxirq_alloc(plink, 2, vlink->c_id, 1);
    if (vlcd_dev->sxirq == 0) {
	ETRACE("client %u could not allocate cross interrupt; "
	       "falling back to polling mode\n", vlink->c_id);
	vlcd_dev->sync = VLCD_SYNC_POLL;
	return;
    }
    DTRACE("client %u allocated sxirq %u\n", vlink->c_id, vlcd_dev->sxirq);

    vlcd_dev->sxid = nkops.nk_xirq_attach(vlcd_dev->sxirq,
        vlcd_sxirq_handler, vlcd_dev);
    if (!vlcd_dev->sxid) {
	ETRACE("client %u could not attach cross interrupt %u; "
	       "falling back to polling mode\n", vlink->c_id, vlcd_dev->sxirq);
	vlcd_dev->sync = VLCD_SYNC_POLL;
	return;
    }
}

    static int __init
vlcd_drv_create (vlcd_driver_t* vlcd_drv)
{
    vlcd_device_t* last = NULL;
    const NkOsId my_os_id = nkops.nk_id_get();
    NkPhAddr plink = 0;
    int ret = 0;

    vlcd_drv->dev_list =  NULL;
    vlcd_drv->sysconf_xid = 0;

	/* Create devices for secondaries */
    while ((plink = nkops.nk_vlink_lookup("vlcd", plink))) {
	NkDevVlink* vlink = nkops.nk_ptov(plink);

	DTRACE("osId %i, c_id %i, s_state %i\n", my_os_id,
	       vlink->c_id, vlink->s_state);
        if ((vlink->c_id == my_os_id)) {
	    vlcd_device_t* vlcd_dev;
	    NkPhAddr       data;

		/* Allocate shared data if not already allocated */
            data = nkops.nk_pdev_alloc(plink, 0, sizeof(NkDevVlcd));
	    if (data == 0) {
                EFTRACE("can't allocate shared data\n");
                ret = -ENOMEM;
                goto error;
            }
		/* Create a device */
            vlcd_dev = (vlcd_device_t*)kzalloc(sizeof(vlcd_device_t),
                        GFP_KERNEL);
            if (vlcd_dev == NULL) {
                EFTRACE("can't allocate device\n");
                ret = -ENOMEM;
                goto error;
            }
	    init_waitqueue_head(&vlcd_dev->sxirq_wait);
            vlcd_dev->common = (NkDevVlcd*) nkops.nk_ptov(data);
            vlcd_dev->vlink  = vlink;
		/* Add device in list */
            if (last == NULL) {
		vlcd_drv->dev_list = vlcd_dev;
            } else {
		last->next = vlcd_dev;
            }
	    vlcd_dev->sync = VLCD_SYNC_NONE;
	    vlcd_dev->flip_buffer_max = VLCD_FB_FLIP_BUFFER_MAX;
	    if (vlink->c_info) {
	        const char* info = nkops.nk_ptov(vlink->c_info);
		vlcd_dev->flip_buffer_max = _flip_buffer_max(info);
		vlcd_dev->refresh_hz      = _refresh_hz(info);
		if (vlcd_dev->refresh_hz) {
		    long jiffies = msecs_to_jiffies(1000/vlcd_dev->refresh_hz);
		    if (jiffies &&
			(!_refresh_jiffies || (jiffies < _refresh_jiffies))) {
			_refresh_jiffies = jiffies;
		    }
		}
		vlcd_dev->sync = _sync_mode(info);
	    }

	    vlcd_intr_sync_mode(vlcd_dev, plink);

	    TRACE("frontend %u using '%s' synchronization with backend\n",
	          vlink->c_id, vlcd_sync_name[ vlcd_dev->sync ]);

            last = vlcd_dev;
            ret ++;
        }
    }
    init_completion(&vlcd_drv->thread_completion);
    sema_init(&vlcd_drv->thread_semaphore, 1);
	/* Attach handler to SYSCONF irq */
    vlcd_drv->sysconf_xid = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF,
            vlcd_sysconf_intr_handler, vlcd_drv);
    if (vlcd_drv->sysconf_xid == 0) {
        EFTRACE("attach to sysconf irq failed\n");
        ret = -ENXIO;
        goto error;
    }
	/* Do not create thread, done later */
    return ret;

error:
    vlcd_drv_disconnect(vlcd_drv, 0);
    return ret;
}

    static int __init
vlcd_drv_register (vlcd_driver_t* vlcd_drv)
{
    int retval;
    vlcd_device_t* vlcd_dev;
    vlcd_device_t* next;
    int            registerID = 0;

	/*
	 * We go through the list many times to do the framebuffer register
	 * in the right order (link 0 first).
	 */
    vlcd_dev = vlcd_dev_get_first (vlcd_drv->dev_list);
    while (vlcd_dev) {
	vlcd_dev->registerID = registerID++;
        next = vlcd_dev_get_next (vlcd_drv->dev_list, vlcd_dev);
	    /* Do device initialization */
        retval = vlcd_dev_init (NULL, vlcd_dev);
        if (retval == 0) {
            DTRACE("initialized dev %i\n", vlcd_dev->vlink->link);
#ifdef CONFIG_VLCD_PWR_TEST
	    vlcd_dev->timed_output_dev.name     = "vlcd_state";
	    vlcd_dev->timed_output_dev.get_time = vlcd_to_get_time;
	    vlcd_dev->timed_output_dev.enable   = vlcd_to_enable;
	    timed_output_dev_register(&vlcd_dev->timed_output_dev);
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	    vlcd_dev->early_suspend.suspend = vlcd_dev_es_suspend;
	    vlcd_dev->early_suspend.resume  = vlcd_dev_es_resume;
	    vlcd_dev->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	    register_early_suspend(&vlcd_dev->early_suspend);
#endif
        } else {
            EFTRACE("Error %i while initializing dev %i\n",
		    retval, vlcd_dev->vlink->link);
            vlcd_drv->dev_list = vlcd_dev_remove(vlcd_drv->dev_list, vlcd_dev);
            vlcd_dev->vlink->c_state = NK_DEV_VLINK_OFF;
            vlcd_dev_sysconf_trigger(vlcd_dev);
            kfree(vlcd_dev);
        }
	    /* Go on with next device */
        vlcd_dev = next;
    }
    return 0;
}

    static int
vlcd_drv_unregister (vlcd_driver_t* vlcd_drv)
{
    vlcd_device_t* vlcd_dev;

    vlcd_dev = vlcd_drv->dev_list;
    while (vlcd_dev != NULL) {
        if (vlcd_dev->fbinfo != NULL) {
#ifdef CONFIG_HAS_EARLYSUSPEND
	    unregister_early_suspend(&vlcd_dev->early_suspend);
#endif
#ifdef CONFIG_VLCD_PWR_TEST
	    timed_output_dev_unregister(&vlcd_dev->timed_output_dev);
#endif
            unregister_framebuffer(vlcd_dev->fbinfo);
            kfree(vlcd_dev->fbinfo->pseudo_palette);
            fb_dealloc_cmap(&vlcd_dev->fbinfo->cmap);
            if (vlcd_dev->fbinfo->fix.smem_len != 0) {
		if (vlcd_dev->fbinfo->fix.smem_len < 4 * 1024 * 1024) {
                    dma_free_writecombine(NULL,
				          vlcd_dev->fbinfo->fix.smem_len,
				          vlcd_dev->fbinfo->screen_base,
				          vlcd_dev->fbinfo->fix.smem_start);
		} else {
		    nkops.nk_mem_unmap(vlcd_dev->fbinfo->screen_base,
				       vlcd_dev->fbinfo->fix.smem_start,
				       vlcd_dev->fbinfo->fix.smem_len);
		}
            }
            framebuffer_release(vlcd_dev->fbinfo);
            vlcd_dev->fbinfo = NULL;
        }
        vlcd_dev = vlcd_dev->next;
    }
	/* Terminate thread */
    if (vlcd_drv->thread_active) {
        vlcd_drv->thread_active = 0;
        up(&vlcd_drv->thread_semaphore);
        wait_for_completion(&vlcd_drv->thread_completion);
    }
	/* Tell backend we are disconnected and free mem */
    vlcd_drv_disconnect(vlcd_drv, 1);
    return 0;
}

/*----- Initialization and termination -----*/

static vlcd_driver_t vlcd_driver;

    static int __init
vlcd_init (void)
{
    int ret, frontend_count;

	/* Create necessary devices */
    ret = vlcd_drv_create(&vlcd_driver);
    if (ret < 0) {
	EFTRACE("Error %d in vlcd_create()\n", ret);
        return ret;
    }
    frontend_count = ret;
	/* Register devices in Linux framebuffer framework */
    ret = vlcd_drv_register(&vlcd_driver);
    if (ret < 0) {
	EFTRACE("Error %d in vlcd_register()\n", ret);
	vlcd_drv_disconnect(&vlcd_driver, 1);
        return ret;
    }
	/* Start thread and handshake */
    vlcd_driver.thread_active = 1;
    if (kernel_thread(vlcd_thread, &vlcd_driver, CLONE_KERNEL) < 0) {
	EFTRACE("Init thread creating failed\n");
	vlcd_drv_unregister(&vlcd_driver);
        return -ENOMEM;
    }
    TRACE("Created %i frontend devices\n", frontend_count);
    return 0;
}

module_init(vlcd_init);

#ifdef MODULE
    static void __exit
vlcd_exit (void)
{
    vlcd_drv_unregister(&vlcd_driver);
    TRACE("unloaded\n");
}

module_exit(vlcd_exit);
MODULE_LICENSE("GPL");
#endif /* MODULE */

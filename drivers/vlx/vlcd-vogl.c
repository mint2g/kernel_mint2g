/*
 ****************************************************************
 *
 *  Component: VLX VLCD backend driver, virtual OpenGL extension
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
 *    Thomas Charleux (thomas.charleux@redbend.com)
 *
 ****************************************************************
 */

/*
  Here is an excerpt of the vogl specification that depicts the VLCD
  extension implemented in this file:

-------------------------------------------------------------------------------
- the VLCD back-end driver provides in the "back-end" VM frame buffer
  devices that refer to the front-end frame buffers. Those devices
  are named "/dev/ffb0", "/dev/ffb1"... for instance. "ffb" stands
  for front-end frame buffer. This enables any "back-end" entity to
  get properties of a front-end frame buffer and also to map its
  color buffers.
- "/dev/fb0" should refer to "/dev/ffb0" or "/dev/ffb1".. depending
  on the front-end frame buffer that is expected to be used.
  Here, the caller of the gralloc module (which opens "/dev/fb0")
  is an instance of the VOpenGL ES back-end daemon.
  For each OpenGL ES application running in any "front-end" VMs, there
  is an instance of the VOpenGL ES daemon running in the "back-end"
  VM and serving all OpenGL ES requests coming from that particular
  application (see VOpenGL ES detailed design for more information).
  Thus each instance of the VOpenGL ES daemon is devoted to only one
  OpenGL ES application and consequently uses a particular front-end
  frame buffer.
  The idea is to create a fake environment for each VOpenGL ES daemon
  instance, where "/dev/fb0" refers to the appropriate "/dev/ffb?"
  device.
  To this end, the following method is proposed: for each VOpenGL ES
  daemon instance, the "/dev" directory will be faked. A new
  directory containing all original "/dev" entries plus a special
  entry for "dev/fb0" that links to the appropriate "dev/ffb?" will
  be mounted onto the "/dev" directory of each daemon instance.
  Consequently, when a daemon instance will call the gralloc module
  and through it will open "/dev/fb0", it will actually open the
  "/dev/ffb?" device as it expects.
-------------------------------------------------------------------------------

  Frame buffer devices that refer to the front-end frame buffers are
  registered in the "back-end" VM by calling "register_framebuffer".
  /dev/fb0.../dev/fb15 are reserved for real frame buffer devices
  whereas /dev/fb16.../dev/fb31 are used to register devices
  referring to front-end frame buffers. As The current VLCD
  implementation supports up to two physical screens the following
  layout is used:

  /dev/fb16 -> VM:1 Screen:1 
  /dev/fb17 -> VM:2 Screen:1 
  /dev/fb18 -> VM:3 Screen:1 
  /dev/fb19 -> VM:4 Screen:1 
  /dev/fb20 -> VM:5 Screen:1 
  /dev/fb21 -> VM:6 Screen:1 
  /dev/fb22 -> VM:7 Screen:1 
  /dev/fb23 -> VM:8 Screen:1 
  /dev/fb24 -> VM:1 Screen:2 
  /dev/fb25 -> VM:2 Screen:2 
  /dev/fb26 -> VM:3 Screen:2 
  /dev/fb27 -> VM:4 Screen:2 
  /dev/fb28 -> VM:5 Screen:2 
  /dev/fb29 -> VM:6 Screen:2 
  /dev/fb30 -> VM:7 Screen:2 
  /dev/fb31 -> VM:8 Screen:2 
*/

#include <linux/fb.h>

#include "vlcd-vogl.h"

/*
 * Traces
 */
#define TRACE(format, args...)	 printk ("VLCD-BE(VOGL): " format, ## args)
#define ETRACE(format, args...)	 TRACE ("[E] " format, ## args)
#define EFTRACE(format, args...) ETRACE ("%s: " format, __func__, ## args)

#if 0
#define VOGL_DEBUG
#endif

#ifdef VOGL_DEBUG
#define DTRACE(format, args...)	TRACE ("%s: " format, __func__, ## args)
#define VOGL_ASSERT(c)		BUG_ON (!(c))
#else
#define DTRACE(format, args...)
#define VOGL_ASSERT(c)
#endif

/*
 * Storage for the native framebuffer configurations (retrieved by calling
 * the "get_possible_config" method of the native driver).
 */
vlcd_pconf_t voglPConf[VLCD_BMAX_HW_DEV_SUP][VLCD_MAX_CONF_NUMBER];

    static int
voglBlankFB (int blank, struct fb_info* info)
{
    return 0;
}

/*
 * The fb ops that are used to build the frame buffer info structure. As 
 * fb_check_var and fb_set_par are not provided, the frame buffer
 * configuration is read-only. In other words any attempt to change the
 * frame buffer configuration will fail.
 */
static struct fb_ops voglFBOps = {
    .fb_blank     = voglBlankFB,
    .fb_fillrect  = cfb_fillrect,
    .fb_copyarea  = cfb_copyarea,
    .fb_imageblit = cfb_imageblit
};

#ifdef VOGL_DEBUG
    /*
     * Dumps the frame buffer configuration.
     */
    static void
voglDumpFB (vlcd_frontend_device_t* fDev)
{
    struct fb_info*           fbInfo = fDev->fbInfo;
    struct fb_fix_screeninfo* fix;
    struct fb_var_screeninfo* var;

    VOGL_ASSERT(voglEnabled());
    VOGL_ASSERT(fbInfo);

    fix = &fbInfo->fix;    
    var = &fbInfo->var;
  
    DTRACE("DUMP VAR (link:%d, OS:%u):\n", fDev->vlink->link,
	   fDev->vlink->c_id); 
    DTRACE("size (%u*%u). virt_size (%u*%u). "
	   "offset (%u*%u)\n",
	   var->xres, var->yres, var->xres_virtual, var->yres_virtual,
	   var->xoffset, var->yoffset);
    DTRACE("bpp:%u, grayscale:%u, red (%u, %u, %u), "
	   "green (%u, %u, %u) blue (%u, %u, %u)\n",
	   var->bits_per_pixel, var->grayscale, var->red.offset,
	   var->red.length, var->red.msb_right, var->green.offset,
	   var->green.length, var->green.msb_right, var->blue.offset,
	   var->blue.length, var->blue.msb_right);
    DTRACE("nonstd:%u, activate:%u, height:%u, width:%u, "
	   "accel_flags:%u\n",
	   var->nonstd, var->activate, var->height, var->width,
	   var->accel_flags);
    DTRACE("pixclock:%u, left_margin:%u, right_margin:%u, "
	   "upper_margin:%u, lower_margin:%u, hsync_len:%u, vsync_len:%u\n",
	   var->pixclock, var->left_margin, var->right_margin,
	   var->upper_margin, var->lower_margin, var->hsync_len,
	   var->vsync_len);
    DTRACE("sync:%u, vmode:%u, rotate:%u\n",
	   var->sync, var->vmode, var->rotate);

    DTRACE("DUMP FIX (link:%d, OS:%u):\n", fDev->vlink->link,
	   fDev->vlink->c_id);
    DTRACE("line_length %u smem_start 0x%lx smem_len %u\n",
	   fix->line_length, fix->smem_start, fix->smem_len);
}
#endif

    /*
     * Registers a frame buffer device in the /dev/fb16.../dev/fb31 range
     * according to the layout described above.
     * 
     * Returns negative errno on error, or zero for success.
     */
    static int
voglRegisterFB (vlcd_frontend_device_t* fDev)
{
    int             ret;
    NkOsId	    cID    = fDev->vlink->c_id;
    int             link   = fDev->vlink->link;
    struct fb_info* fbInfo = fDev->fbInfo;
    int             fbIdx  = 15 + cID + link * 8;

    VOGL_ASSERT(voglEnabled());
    VOGL_ASSERT(!fDev->fbRegistered);
    VOGL_ASSERT(fbInfo);

    ret = vlcdRegisterFB(fbInfo, fbIdx);
    if (ret) {
	EFTRACE("vlcdRegisterFB failed %d\n", ret);
	return ret;
    }

    VOGL_ASSERT(fbInfo->node == fbIdx);
    TRACE("Frontend device (OS=%u link=%d) is "
	  "registered on /dev/fb%d\n", cID, link, fbIdx);
    fDev->fbRegistered = 1;
    
    return ret;
}

    /*
     * Updates the initial frame buffer configuration.
     */
    void
voglUpdateFB (vlcd_frontend_device_t* fDev)
{
    struct fb_info*           fbInfo = fDev->fbInfo;
    NkDevVlcd*                shared = fDev->common;
    struct fb_fix_screeninfo* fix;
    struct fb_var_screeninfo* var;

    VOGL_ASSERT(voglEnabled());

    if (!fbInfo) {
	ETRACE("(hw:%d, fosId:%u): frame buffer info structure "
	       "not available\n", fDev->vlink->link, fDev->vlink->c_id);
	return;
    }

    VOGL_ASSERT(fDev->fbRegistered);

    fix = &fbInfo->fix;
    
    fix->smem_len   = shared->current_conf.dma_zone_size;
    fix->smem_start = shared->current_conf.dma_zone_paddr;
    
    var = &fbInfo->var;
    
    var->yres_virtual = shared->current_conf.yres_virtual;

#ifdef VOGL_DEBUG
    voglDumpFB(fDev);
#endif
}

    /*
     * Allocates and initializes a frame buffer info structure by using the
     * first native frame buffer configuration.
     *
     * Returns negative errno on error, or zero for success.
     */
    static int
voglPrepareFB (vlcd_frontend_device_t* fDev)
{
    struct fb_info*           fbInfo;
    struct fb_var_screeninfo* var;
    struct fb_fix_screeninfo* fix;
    int                       link     = fDev->vlink->link;
    vlcd_pconf_t*             pConf    = voglPConf[link];
    vlcd_color_conf_t*        colorCfg = &pConf->color_conf;

    VOGL_ASSERT(voglEnabled());
    VOGL_ASSERT(!fDev->fbRegistered);
    VOGL_ASSERT(!fDev->fbInfo);

    fbInfo = framebuffer_alloc(0, NULL);
    if (!fbInfo) {
	EFTRACE("framebuffer_alloc failed\n");
	return -ENOMEM;
    }

    var = &fbInfo->var;

    fbInfo->fbops = &voglFBOps;
    fbInfo->flags = FBINFO_FLAG_DEFAULT;

    var->bits_per_pixel  = colorCfg->bpp;
    var->red.offset      = colorCfg->red_offset;
    var->red.length      = colorCfg->red_length;
    var->red.msb_right   = colorCfg->align;
    var->green.offset    = colorCfg->green_offset;
    var->green.length    = colorCfg->green_length;
    var->green.msb_right = colorCfg->align;
    var->blue.offset     = colorCfg->blue_offset;
    var->blue.length     = colorCfg->blue_length;
    var->blue.msb_right  = colorCfg->align;
    var->xres            = pConf->xres;
    var->yres            = pConf->yres;
    var->xres_virtual    = pConf->xres;
    var->yres_virtual    = pConf->yres;

    fix = &fbInfo->fix;

    fix->line_length = vlcd_get_line_length(var->xres_virtual,
					    var->bits_per_pixel);

    fDev->fbInfo = fbInfo;
  
    return 0;
}

    /*
     * Unregisters a frame buffer device and frees the corresponding
     * frame buffer info structure
     */
    void
voglCleanupFB (vlcd_frontend_device_t* fDev)
{
    int ret;

    VOGL_ASSERT(voglEnabled());

    if (fDev->fbRegistered) {
	ret = unregister_framebuffer(fDev->fbInfo);
	if (ret) {
	    EFTRACE("unregister_framebuffer failed %d\n", ret);
	    return;
	}
	fDev->fbRegistered = 0;
    }
    if (fDev->fbInfo) {
	framebuffer_release(fDev->fbInfo);
	fDev->fbInfo = NULL;
    }
}

    /*
     * Prepares a frame buffer info structure and registers the
     * corresponding frame buffer device.
     *
     * Returns negative errno on error, or zero for success.
     */
    int
voglInitFB (vlcd_frontend_device_t* fDev)
{
    int ret;

    VOGL_ASSERT(voglEnabled());

    ret = voglPrepareFB(fDev);
    if (ret) {
	return ret;
    }
        
    return voglRegisterFB(fDev);
}

    /*
     * Returns 1 if a vogl server is configured in the command line.
     * Returns 0 otherwise.
     */
    int
voglEnabled (void)
{
    static int         testVogl = 0;
           NkOsId      myID     = nkops.nk_id_get();
           NkPhAddr    pLink;
           NkDevVlink* vLink;

    if (testVogl) {
	return testVogl - 1;
    }

    pLink = 0;
    while ((pLink = nkops.nk_vlink_lookup("vogl", pLink))) {
        vLink = nkops.nk_ptov(pLink);
        if (vLink->s_id == myID) {
	    TRACE("VOGL server detected\n");
	    testVogl = 2;
	    return 1;
	}
    }
 
    TRACE("no VOGL server\n");
    testVogl = 1;
    return 0;
}

EXPORT_SYMBOL (voglInitFB);
EXPORT_SYMBOL (voglPConf);
EXPORT_SYMBOL (voglEnabled);
EXPORT_SYMBOL (voglUpdateFB);
EXPORT_SYMBOL (voglCleanupFB);

MODULE_LICENSE ("GPL");


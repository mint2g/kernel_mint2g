/*
 ****************************************************************
 *
 *  Component: VLX virtual blitter backend driver
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
 *    Christian Jacquemot (Christian.Jacquemot@redbend.com)
 *    Christophe Lizzi (Christophe.Lizzi@redbend.com)
 *
 ****************************************************************
 */

#include <linux/version.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/mutex.h>
#include <linux/fb.h>

#include <nk/nkern.h>
#include <vlx/vblitter_common.h>
#include "vblitter-be.h"
#include "vrpc.h"
#include "vpmem.h"

#ifdef VBLITTER_SOFT

//#define VBLITTER_DEBUG

#define TRACE(format, args...)   printk("VBLITTER-SOFT: " format, ## args)
#define ETRACE(format, args...)  printk("VBLITTER-SOFT: [E] " format, ## args)

#ifdef VBLITTER_DEBUG
#define DTRACE(format, args...)  printk("VBLITTER-SOFT: [D] " format, ## args)
#else
#define DTRACE(format, args...)  do {} while (0)
#endif
#define DTRACE0(format, args...) do {} while (0)


// Exported by the vLCD backend
typedef struct vlcd_info {
    unsigned long fb_phys;
    unsigned int  fb_len;
} vlcd_info_t;

extern vlcd_info_t* vlcd_get_info (unsigned long id);


typedef struct vblitter_soft_dev {
    unsigned int   id;
    vpmem_handle_t vpmem;
    unsigned int   vpmem_size;
    unsigned long  vpmem_phys;
    unsigned char* vpmem_base;
    unsigned int   fb_size;
    unsigned long  fb_phys;
    unsigned char* fb_base;
} vblitter_soft_dev_t;


static vblitter_soft_dev_t vblitter_soft_dev;


    static int
vblitter_soft_fb_map (vblitter_soft_dev_t* dev)
{
    if (dev->fb_phys == 0) {
	return -ENOMEM;
    }

    DTRACE("mapping fb_phys %lx, fb_size %u\n", dev->fb_phys, dev->fb_size);

    dev->fb_base = ioremap_nocache(dev->fb_phys, dev->fb_size);
    if (dev->fb_base == NULL) {
	ETRACE("ioremap_nocache(fb_phys %lx, fb_size %u) failed\n", dev->fb_phys, dev->fb_size);
    }

    DTRACE("mapped fb_phys %lx, fb_size %u -> fb_base %p\n", dev->fb_phys, dev->fb_size, dev->fb_base);

    return 0;
}


    static void
vblitter_soft_fb_unmap (vblitter_soft_dev_t* dev)
{
    iounmap(dev->fb_base);

    dev->fb_base = 0;
}


    static int
vblitter_soft_hw_open (unsigned int hw_id, vpmem_handle_t vpmem, vblitter_hw_handle_t* hw_handle)
{
    vblitter_soft_dev_t* dev;
    vlcd_info_t*         info;
    int                  err;

    *hw_handle = 0;

    info = vlcd_get_info(hw_id);
    if (info == NULL) {
	return -EIO;
    }

    if (!info->fb_phys || !info->fb_len) {
	return -ENOMEM;
    }

    dev = &vblitter_soft_dev;
    memset(dev, 0x00, sizeof(*dev));

    dev->id = hw_id;

    dev->fb_size = info->fb_len;
    dev->fb_phys = info->fb_phys;

    err = vblitter_soft_fb_map(dev);
    if (err) {
	return err;
    }

    dev->vpmem      = vpmem;
    dev->vpmem_size = vpmem_size(vpmem);
    dev->vpmem_phys = vpmem_phys(vpmem);
    dev->vpmem_base = vpmem_map(vpmem);
    if (dev->vpmem_base == NULL) {
	vblitter_soft_fb_unmap(dev);
	return -ENOMEM;
    }

    *hw_handle = (vblitter_hw_handle_t) &vblitter_soft_dev;

    return 0;
}


    static int
vblitter_soft_hw_release (vblitter_hw_handle_t hw_handle)
{
    vblitter_soft_dev_t* dev = (vblitter_soft_dev_t*) hw_handle;

    if (dev->fb_base) {
	vblitter_soft_fb_unmap(dev);
	dev->fb_base = NULL;
    }

    if (dev->vpmem && dev->vpmem_base) {
	vpmem_unmap(dev->vpmem);
	dev->vpmem_base = NULL;
    }

    return 0;
}


    static int
vblitter_soft_hw_blit (vblitter_hw_handle_t hw_handle, vblit_blit_req_t* req)
{
    vblitter_soft_dev_t* dev = (vblitter_soft_dev_t*) hw_handle;

    unsigned int   bpp = 2;
    unsigned int   size;
    unsigned int   h;
    unsigned int   w;
    unsigned char* s;
    unsigned char* d;
    unsigned int   src_bpp;
    unsigned int   dst_bpp;
    unsigned char* src_base;
    unsigned char* dst_base;
    int            err = 0;

    if (req->src.memory_id) {
	src_base = dev->fb_base + req->src.offset;
    } else {
	src_base = dev->vpmem_base + req->src.offset;
    }

    if (req->dst.memory_id) {
	dst_base = dev->fb_base + req->dst.offset;
    } else {
	dst_base = dev->vpmem_base + req->dst.offset;
    }

    src_bpp = vblitter_get_format_bpp(req->src.format);
    dst_bpp = vblitter_get_format_bpp(req->dst.format);

    (void) src_bpp;
    (void) dst_bpp;

    DTRACE("SOFT COPYBIT: %ux%u:%u at %s:%p [%s] (%u bytes) -> %ux%u:%u at %s:%p [%s] (%u bytes) [%salpha %u]\n",
      req->src_rect.w, req->src_rect.h, src_bpp, req->src.memory_id ? "FB" : "PM", src_base,
      vblitter_get_format_str(req->src.format),
      req->src_rect.w * req->src_rect.h * src_bpp,

      req->dst_rect.w, req->dst_rect.h, dst_bpp, req->dst.memory_id ? "FB" : "PM", dst_base,
      vblitter_get_format_str(req->dst.format),
      req->dst_rect.w * req->dst_rect.h * dst_bpp,

      vblitter_get_transform_str(req->flags), req->alpha);

    if ((req->src_rect.w == 0) || (req->src_rect.h == 0)) return 0;
    if ((req->dst_rect.w == 0) || (req->dst_rect.h == 0)) return 0;

    if ((req->src_rect.w != req->dst_rect.w) || (req->src_rect.h != req->dst_rect.h)) {
	DTRACE("SOFT COPYBIT ERROR: stretching is not supported\n");
	err = -EINVAL;
	goto OUT;
    }

    if (req->src.format != req->dst.format) {
      DTRACE("SOFT COPYBIT ERROR: format conversion %d -> %d is not supported\n", req->src.format, req->dst.format);
      err = -EINVAL;
      goto OUT;
    }

    if (req->src.format >= VBLITTER_FMT_MAX) {
	DTRACE("SOFT COPYBIT ERROR: src format %d is not supported\n", req->src.format);
	err = -EINVAL;
	goto OUT;
    }
    bpp = src_bpp;

    //DTRACE("SOFT COPYBIT: format %d -> bpp %u\n", req->src.format, bpp);

    size = req->src_rect.w * bpp;

    h = req->src_rect.h;
    w = req->src.width * bpp;

    s = src_base + (req->src_rect.x + req->src.width * req->src_rect.y) * bpp;
    d = dst_base + (req->dst_rect.x + req->dst.width * req->dst_rect.y) * bpp;

    if ((req->src.width == req->dst.width) && (req->src_rect.w == req->src.width)) {
	size *= req->src_rect.h;
	h = 1;
    }

    do {
	memcpy(d, s, size);
	d += w;
	s += w;
    } while (--h > 0);

OUT:

    return err;
}


static vblitter_hw_ops_t vblitter_soft_hw_ops = {
    VBLITTER_HW_OPS_VERSION,
    vblitter_soft_hw_open,
    vblitter_soft_hw_release,
    vblitter_soft_hw_blit
};


// Software blitter entry point
    int
vblitter_hw_ops_init (vblitter_hw_ops_t** hw_ops)
{
    *hw_ops = &vblitter_soft_hw_ops;

    return 0;
}

#endif // VBLITTER_SOFT

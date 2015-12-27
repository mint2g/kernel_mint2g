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
 *    Vladimir Grouzdev (Vladimir.Grouzdev@redbend.com)
 *    Christian Jacquemot (Christian.Jacquemot@redbend.com)
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

//#define VBLITTER_DEBUG

#define TRACE(format, args...)   printk("VBLITTER-BE: " format, ## args)
#define ETRACE(format, args...)  printk("VBLITTER-BE: [E] " format, ## args)

#ifdef VBLITTER_DEBUG
#define DTRACE(format, args...)  printk("VBLITTER-BE: [D] " format, ## args)
#else
#define DTRACE(format, args...)  do {} while (0)
#endif
#define DTRACE0(format, args...) do {} while (0)


typedef struct vblitter {
    unsigned int            id;
    struct vrpc_t*          vrpc;
    void*                   data;
    vrpc_size_t             msize;
    vpmem_handle_t          vpmem_handle;
    vblitter_hw_ops_t*      hw_ops;
    vblitter_hw_handle_t    hw_handle;
} vblitter_t;

static vblitter_t _vblitter;


    unsigned int
vblitter_get_format_bpp (unsigned int fmt)
{
    static unsigned int vblitter_format_bpp[VBLITTER_FMT_MAX] = {
	2, // VBLITTER_FMT_RGB_565
	4, // VBLITTER_FMT_XRGB_8888
	2, // VBLITTER_FMT_Y_CBCR_H2V2
	4, // VBLITTER_FMT_ARGB_8888
	4, // VBLITTER_FMT_RGB_888
	2, // VBLITTER_FMT_Y_CRCB_H2V2
	2, // VBLITTER_FMT_YCRYCB_H2V1
	2, // VBLITTER_FMT_Y_CRCB_H2V1
	2, // VBLITTER_FMT_Y_CBCR_H2V1
	4, // VBLITTER_FMT_RGBA_8888
	4, // VBLITTER_FMT_BGRA_8888
	4  // VBLITTER_FMT_RGBX_8888
    };

    if (fmt >= VBLITTER_FMT_MAX) {
	return 0;
    }

    return vblitter_format_bpp[ fmt ];
}
EXPORT_SYMBOL(vblitter_get_format_bpp);

    const char*
vblitter_get_format_str (unsigned int fmt)
{
    static const char* vblitter_format_str[VBLITTER_FMT_MAX] = {
	"RGB565",
	"XRGB8888",
	"YCbCr420SP",
	"ARGB888",
	"RGB_888",
	"YCrCb",
	"YCrYCbI",
	"YCrCb",
	"YCbCr422SP",
	"RGBA8888",
	"BGRA8888",
	"RGBX8888"
    };

    if (fmt >= VBLITTER_FMT_MAX) {
	return "unknown";
    }

    return vblitter_format_str[ fmt ];
}


typedef struct vblitter_transform {
    unsigned int flags;
    unsigned int mask;
    const char*  str;
} vblitter_transform_t;

    char*
vblitter_get_transform_str (unsigned int flags)
{
    static vblitter_transform_t vblitter_transform[] = {
	{ VBLITTER_TRANS_FLIP_LR, VBLITTER_TRANS_FLIP_LR|VBLITTER_TRANS_FLIP_UD|VBLITTER_TRANS_ROT_90, "FLIP_LR" },
	{ VBLITTER_TRANS_FLIP_UD, VBLITTER_TRANS_FLIP_LR|VBLITTER_TRANS_FLIP_UD|VBLITTER_TRANS_ROT_90, "FLIP_UD" },
	{ VBLITTER_TRANS_ROT_90,  VBLITTER_TRANS_FLIP_LR|VBLITTER_TRANS_FLIP_UD|VBLITTER_TRANS_ROT_90, "ROT_90"  },
	{ VBLITTER_TRANS_ROT_180, VBLITTER_TRANS_FLIP_LR|VBLITTER_TRANS_FLIP_UD|VBLITTER_TRANS_ROT_90, "ROT_180"  },
	{ VBLITTER_TRANS_ROT_270, VBLITTER_TRANS_FLIP_LR|VBLITTER_TRANS_FLIP_UD|VBLITTER_TRANS_ROT_90, "ROT_270"  },
	{ VBLITTER_TRANS_DITHER,  VBLITTER_TRANS_DITHER,                                               "DITHER" },
	{ VBLITTER_TRANS_BLUR,    VBLITTER_TRANS_BLUR,                                                 "BLUR" },
	{ VBLITTER_TRANS_NOP,     VBLITTER_TRANS_NOP,                                                  "NOP" }
    };
    vblitter_transform_t* tr = vblitter_transform;
    static char str[ 40 ];
    char* ptr;

    ptr = str;
    *ptr = 0;
    while (tr->flags != VBLITTER_TRANS_NOP) {
	if ((flags & tr->mask) == tr->flags) ptr += sprintf(str, "%s, ", tr->str);
	tr++;
    }

    return str;
}


    static int
vblitter_copybit (vblitter_t* vblitter, vblit_blit_req_t* req)
{
    int err = 0;

    DTRACE0("src={w=%d, h=%d, f=%d, o=%d, m=%d, rect={%d,%d,%d,%d}}\n",
      req->src.width,
      req->src.height,
      req->src.format,
      req->src.offset,
      req->src.memory_id,
      req->src_rect.x,
      req->src_rect.y,
      req->src_rect.w,
      req->src_rect.h);
    DTRACE0("    dst={w=%d, h=%d, f=%d, o=%d, m=%d, rect={%d,%d,%d,%d}}\n",
      req->dst.width,
      req->dst.height,
      req->dst.format,
      req->dst.offset,
      req->dst.memory_id,
      req->dst_rect.x,
      req->dst_rect.y,
      req->dst_rect.w,
      req->dst_rect.h);
    DTRACE0("    flags=%08x\n",
      req->flags);

    err = vblitter->hw_ops->blit(vblitter->hw_handle, req);

    return err;
}


    static int
vblitter_cmd_blit (vblitter_t* vblitter, vblit_blit_req_list_t* req_list,  vblitter_res_t* res)
{
    int ret = 0;
    int err = 0;
    unsigned int i;

    if (vblitter->hw_handle == 0) {
	vblitter->hw_ops->open(vblitter->id, vblitter->vpmem_handle, &vblitter->hw_handle);
	if (vblitter->hw_handle == 0) {
	    return -EIO;
	}
    }

    for (i = 0; i < req_list->count; i++) {
	ret = vblitter_copybit(vblitter, &req_list->req[i]);
	if (ret != 0) {
	    err = ret;
	}
    }

    if (err != 0) {
	ret = 0;
    } else {
	res->res   = 0;
	res->value = 0;
	ret = sizeof(vblitter_res_t);
    }

    return ret;
}


    static vrpc_size_t
_vblitter_call (void* cookie, vrpc_size_t size)
{
    vblitter_t*     vblitter = (vblitter_t*) cookie;
    vblitter_req_t* req      = (vblitter_req_t*) vblitter->data;
    vblitter_res_t* res      = (vblitter_res_t*) vblitter->data;
    int             ret      = 0;
    
    // The field transp_mask (currently unused here) is used to pass a sequence number
    int seq = (int)((vblit_blit_req_list_t*)&req->arg)->req[0].transp_mask;

    (void) seq;

    DTRACE("_vblitter_call() upcalled, seq %d\n", counter);

    if ((vblitter->msize < sizeof(vblitter_res_t)) || (size != sizeof(vblitter_req_t))) {
	return 0;
    }

    switch (req->cmd) {
        case VBLITTER_CMD_BLIT: {
	    ret = vblitter_cmd_blit(vblitter, &req->arg, res);
	    break;
	}
    }

    DTRACE("_vblitter_call() returning, seq %d, ret %d\n", counter, ret);

    return ret;
}

    static int
vblitter_vpmem_init (vblitter_t* vblitter)
{
    vpmem_handle_t vpmem_handle;

    vpmem_handle = vpmem_lookup((char*)VBLITTER_VPMEM_DEV);
    if (vpmem_handle == 0) {
	return -ENOMEM;
    }

    vblitter->vpmem_handle = vpmem_handle;

    return 0;
}

    static int
vblitter_vrpc_init (vblitter_t* vblitter)
{
    int res;

    struct vrpc_t* vrpc = vrpc_server_lookup(VBLITTER_VRPC_NAME, 0);
    if (vrpc == NULL) {
        return -EINVAL;
    }

    vblitter->vrpc  = vrpc;
    vblitter->msize = vrpc_maxsize(vrpc);
    vblitter->data  = vrpc_data(vrpc);

    if ((vblitter->msize < sizeof(vblitter_req_t)) ||
        (vblitter->msize < sizeof(vblitter_res_t))) {
	ETRACE("not enough vRPC shared memory -> %d\n", vblitter->msize);
	return -ENOMEM;
    }

    if ((res = vrpc_server_open(vrpc, _vblitter_call, &_vblitter, 0))) {
	ETRACE("vRPC open failed -> %d\n", res);
	return -ENOMEM;
    }

    DTRACE("VLX virtual blitter %d created\n", vrpc_peer_id(vrpc));

    return 0;
}


    static void
vblitter_exit (void)
{
    vblitter_t* vblitter = &_vblitter;

    vrpc_close(vblitter->vrpc);

    vrpc_release(vblitter->vrpc);
}


    static int
_vblitter_init (void)
{
    vblitter_t* vblitter = &_vblitter;
    int         err      = 0;

    memset(vblitter, 0x00, sizeof(*vblitter));

    err = vblitter_vpmem_init(vblitter);
    if (err != 0) {
	ETRACE("vblitter vpmem init failed, err %d\n", err);
	return -EIO;
    }

    err = vblitter_vrpc_init(vblitter);
    if (err != 0) {
	ETRACE("vblitter vrpc init failed, err %d\n", err);
	return -EIO;
    }

    // Hardware blitter entry point
    err = vblitter_hw_ops_init(&vblitter->hw_ops);
    if ((err != 0) || (vblitter->hw_ops == NULL)) {
	ETRACE("vblitter hw init failed, err %d\n", err);
	return -EIO;
    }

    TRACE("VLX virtual blitter back-end initialized\n");

    return 0;
}

    static void
_vblitter_exit (void)
{
    vblitter_exit();
}


module_init(_vblitter_init);
module_exit(_vblitter_exit);

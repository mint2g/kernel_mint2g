/*
 ****************************************************************
 *
 *  Component: VLX virtual blitter
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

#ifndef _VBLITTER_COMMON_H_
#define _VBLITTER_COMMON_H_

//#define VBLITTER_SOFT

#define VBLITTER_MAJOR     242
#define VBLITTER_MINOR     0
#define VBLITTER_NAME      "vblitter"
#define VBLITTER_VRPC_NAME "vblitter"

#define VBLITTER_VPMEM_DEV "pmem_gfx"

// Pixel format
typedef enum vblit_fmt {
    VBLITTER_FMT_RGB_565,
    VBLITTER_FMT_XRGB_8888,
    VBLITTER_FMT_Y_CBCR_H2V2,
    VBLITTER_FMT_ARGB_8888,
    VBLITTER_FMT_RGB_888,
    VBLITTER_FMT_Y_CRCB_H2V2,
    VBLITTER_FMT_YCRYCB_H2V1,
    VBLITTER_FMT_Y_CRCB_H2V1,
    VBLITTER_FMT_Y_CBCR_H2V1,
    VBLITTER_FMT_RGBA_8888,
    VBLITTER_FMT_BGRA_8888,
    VBLITTER_FMT_RGBX_8888,
    VBLITTER_FMT_MAX
} vblit_fmt_t;


// Graphics buffer's origin
typedef enum vblit_mem {
    VBLITTER_MEM_PMEM,
    VBLITTER_MEM_FB
} vblit_mem_t;


// Transformations
#define VBLITTER_TRANS_NOP       0
#define VBLITTER_TRANS_FLIP_LR   (1<<0)
#define VBLITTER_TRANS_FLIP_UD   (1<<1)
#define VBLITTER_TRANS_ROT_90    (1<<2)
#define VBLITTER_TRANS_ROT_180   (VBLITTER_TRANS_FLIP_UD | VBLITTER_TRANS_FLIP_LR)
#define VBLITTER_TRANS_ROT_270   (VBLITTER_TRANS_ROT_90 | VBLITTER_TRANS_FLIP_UD | VBLITTER_TRANS_FLIP_LR)
#define VBLITTER_TRANS_DITHER    (1<<3)
#define VBLITTER_TRANS_BLUR      (1<<4)

#define VBLITTER_TRANSP_NOP      0xffffffff
#define VBLITTER_ALPHA_NOP       0xff


typedef struct vblit_rect {
    nku32_f x;
    nku32_f y;
    nku32_f w;
    nku32_f h;
} vblit_rect_t;

typedef struct vblit_img {
    nku32_f width;
    nku32_f height;
    nku32_f format;
    nku32_f offset;
    nku32_f memory_id;
} vblit_img_t;

typedef struct vblit_blit_req {
    vblit_img_t  src;
    vblit_img_t  dst;
    vblit_rect_t src_rect;
    vblit_rect_t dst_rect;
    nku32_f      alpha;
    nku32_f      transp_mask;
    nku32_f      flags;
} vblit_blit_req_t;


#define VBLITTER_REQ_MAX 4

typedef struct vblit_blit_req_list {
    nku32_f          count;
    vblit_blit_req_t req[VBLITTER_REQ_MAX];
} vblit_blit_req_list_t;

// vRPC request
typedef struct vblitter_req {
    nku32_f               cmd;	// command
    vblit_blit_req_list_t arg;  // argument
} vblitter_req_t;


// vRPC result
typedef struct vblitter_res {
    nku32_f res;	// result
    nku32_f value;	// value
} vblitter_res_t;


#define VBLITTER_IOCTL_BLIT     0

#define	VBLITTER_CMD_BLIT	0

#endif /* _VBLITTER_COMMON_H_ */

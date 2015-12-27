/*
 ****************************************************************
 *
 *  Component: VLX virtual video backend driver
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
 *    Christophe Lizzi (Christophe.Lizzi@redbend.com)
 *
 ****************************************************************
 */

#include <linux/device.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#include <linux/fb.h>
#include <linux/videodev2.h>
#include <media/v4l2-dev.h> // VIDEO_MAJOR definition

// Outdated videodev2.h header?
#ifndef V4L2_CID_ROTATE
#define V4L2_CID_ROTATE (V4L2_CID_BASE+32)
#endif
#ifndef V4L2_FBUF_CAP_SRC_CHROMAKEY
#define V4L2_FBUF_CAP_SRC_CHROMAKEY 0x0080
#endif

#include <nk/nkern.h>
#include <nk/nkdev.h>
#include <nk/nk.h>

#include <vlx/vvideo_common.h>
#include "vvideo-be.h"


#if 0
#define VVIDEO_HW_DEBUG
#endif

#define VVIDEO_HW_NAME "vvideo-hw"

#define ETRACE(fmt, args...)    printk(VVIDEO_HW_NAME ": ERROR: %s: " fmt, __func__, ## args)
#ifdef VVIDEO_HW_DEBUG
#define DTRACE(fmt, args...)    printk(VVIDEO_HW_NAME ": %s: " fmt, __func__, ## args)
#else
#define DTRACE(fmt, args...)    do {} while (0)
#endif

// Generic FIFO queue
typedef struct fifo_t {
    void**            slot;
    int               len;
    int               prod;
    int               cons;
    int               count;
    struct mutex      mutex;
    wait_queue_head_t cond;
} fifo_t;

static int     fifo_init (fifo_t* fifo, int len);
static int     fifo_put  (fifo_t* fifo, void* data);
static int     fifo_get  (fifo_t* fifo, void** data, int wait);
static int     fifo_poll (fifo_t* fifo);


// Extra, non-V4L2 commands issued by Android's liboverlay
typedef struct vvideo_hw_colorkey_t {
    unsigned int output_dev;
    unsigned int key_type;
    unsigned int key_val;
} vvideo_hw_colorkey_t;

#define VIDIOC_VVIDEO_HW_S_ROTATION		_IOW ('V',  3, int)
#define VIDIOC_VVIDEO_HW_G_ROTATION		_IOR ('V',  4, int)
#define VIDIOC_VVIDEO_HW_S_COLORKEY		_IOW ('V',  7, vvideo_hw_colorkey_t)
#define VIDIOC_VVIDEO_HW_G_COLORKEY		_IOW ('V',  8, vvideo_hw_colorkey_t)
#define VIDIOC_VVIDEO_HW_COLORKEY_ENABLE	_IOW ('V', 11, int)
#define VIDIOC_VVIDEO_HW_COLORKEY_DISABLE	_IOW ('V', 12, int)


typedef struct vvideo_hw_pix_fmt_t {
    unsigned int         pix_fmt;    // V4L2 pixel format
    enum v4l2_colorspace colorspace; // V4L2 color space
    unsigned int         bypp;       // Bytes per pixel
} vvideo_hw_pix_fmt_t;


#define VVIDEO_HW_BUF_MAX 16

typedef struct vvideo_hw_desc_t  {
    char*         name;
    unsigned int  type;
    unsigned int  width;
    unsigned int  height;
    unsigned int  bypp;
    unsigned int  maxbufs;
    unsigned long pmem_size;
    int           dev_id;
} vvideo_hw_desc_t;

typedef struct vvideo_hw_device_t {
    int                        minor;
    vvideo_hw_desc_t*          desc;
    NkPhAddr                   pmem;
    unsigned char*             mem_base;
    unsigned char*             mem_top;
    unsigned char*             mem_free;
    struct v4l2_buffer         buf[ VVIDEO_HW_BUF_MAX ];
    unsigned int               nbufs;
    fifo_t                     bufq;
    struct v4l2_capability     cap;
    struct v4l2_pix_format     pix;
    struct v4l2_window         win;
    struct v4l2_rect           crop;
    vvideo_hw_pix_fmt_t*       fmt;
    unsigned int               streaming;
    unsigned int               rotation;
    unsigned int               fbuf_flags;
    vvideo_hw_colorkey_t       colorkey;
} vvideo_hw_device_t;


static vvideo_hw_pix_fmt_t vvideo_hw_pix_fmt[] = {
    { V4L2_PIX_FMT_RGB565,  V4L2_COLORSPACE_SRGB, 2 },  // Android OVERLAY_FORMAT_RGB_565
    { V4L2_PIX_FMT_UYVY,    V4L2_COLORSPACE_JPEG, 2 },  // Android OVERLAY_FORMAT_YCbCr_422_I
    { V4L2_PIX_FMT_YUV422P, V4L2_COLORSPACE_JPEG, 2 }
};

#define VVIDEO_HW_PIX_FMT_MAX (sizeof(vvideo_hw_pix_fmt) / sizeof(vvideo_hw_pix_fmt[0]))


static vvideo_hw_desc_t vvideo_hw_desc[] = {
    { "",        VVIDEO_TYPE_NONE,    0, 0, 0, 0, 0,  0 },
    { "overlay", VVIDEO_TYPE_OVERLAY, 0, 0, 2, 4, 0, -1 },
};

#define VVIDEO_HW_DESC_MAX ((int)(sizeof(vvideo_hw_desc) / sizeof(vvideo_hw_desc[0])))


#define VVIDEO_HW_DEV_MIN 1
#define VVIDEO_HW_DEV_MAX ((int)VVIDEO_HW_DESC_MAX)

#define VVIDEO_HW_BUF_LEN(desc)  ((desc)->width * (desc)->height * (desc)->bypp)
#define VVIDEO_HW_LEN(desc)      (PAGE_ALIGN(VVIDEO_HW_BUF_LEN(desc)) * (desc)->maxbufs)


static vvideo_hw_device_t* vvideo_hw_dev[ VVIDEO_HW_DEV_MAX + 1 ] = { NULL };
static struct mutex        vvideo_hw_lock;


static vvideo_hw_pix_fmt_t*
vvideo_hw_lookup_pix_fmt (unsigned int pix_fmt)
{
    vvideo_hw_pix_fmt_t* entry = vvideo_hw_pix_fmt;
    unsigned int         i;

    DTRACE("looking for pixel format %08x\n", pix_fmt);

    for (i = 0; i < VVIDEO_HW_PIX_FMT_MAX; i++, entry++) {
	if (entry->pix_fmt == pix_fmt) {
	    DTRACE("found pixel format %08x, %u bytes/pixel\n", entry->pix_fmt, entry->bypp);
	    return entry;
	}
    }

    DTRACE("pixel format %08x not found\n", pix_fmt);
    return NULL;
}


static int
vvideo_hw_set_pix (vvideo_hw_device_t* dev, struct v4l2_pix_format* new_pix)
{
    struct v4l2_pix_format* pix = &dev->pix;
    vvideo_hw_pix_fmt_t*    fmt;

    DTRACE("new pix %ux%u, fmt %08x\n", new_pix->width, new_pix->height, pix->pixelformat);

    fmt = vvideo_hw_lookup_pix_fmt(new_pix->pixelformat);
    if (fmt == NULL) {
	ETRACE("pixel format %08x not supported\n", new_pix->pixelformat);
	return -EINVAL;
    }

    dev->fmt = fmt;

#if 0 // depends on screen orientation
    if (new_pix->width > dev->desc->width) {
	new_pix->width = dev->desc->width;
    }

    if (new_pix->height > dev->desc->height) {
	new_pix->height = dev->desc->height;
    }
#endif

    pix->width        = new_pix->width;
    pix->height       = new_pix->height;
    pix->pixelformat  = new_pix->pixelformat;
    pix->colorspace   = new_pix->colorspace;
    pix->bytesperline = pix->width * dev->fmt->bypp;
    pix->sizeimage    = pix->bytesperline * pix->height;

    return 0;
}


static int
vvideo_hw_set_win (vvideo_hw_device_t* dev, struct v4l2_window* new_win)
{
    struct v4l2_window*     win = &dev->win;

    DTRACE("new win %ux%u at %u:%u\n", new_win->w.width, new_win->w.height, new_win->w.left, new_win->w.top);

    win->w.left       = new_win->w.left;
    win->w.top        = new_win->w.top;
    win->w.width      = new_win->w.width;
    win->w.height     = new_win->w.height;
    win->chromakey    = new_win->chromakey;
    win->global_alpha = new_win->global_alpha;

    return 0;
}


static int
vvideo_hw_set_crop (vvideo_hw_device_t* dev, struct v4l2_rect* new_crop)
{
    struct v4l2_rect*       crop = &dev->crop;

    DTRACE("new crop %ux%u at %u:%u\n", new_crop->width, new_crop->height, new_crop->left, new_crop->top);

    crop->left   = new_crop->left;
    crop->top    = new_crop->top;
    crop->width  = new_crop->width;
    crop->height = new_crop->height;

    return 0;
}


static int
vvideo_hw_allocate_buffer (vvideo_hw_device_t* dev, unsigned int cacheable, unsigned int consistent)
{
    unsigned long       pix_size;
    unsigned long       buf_size;
    unsigned long       buf_avail;
    unsigned long       buf_offset;
    struct v4l2_buffer* buf;

    (void) cacheable;
    (void) consistent;

    if (dev->nbufs == VVIDEO_HW_BUF_MAX) {
	ETRACE("buffer table full\n");
	return -EAGAIN;
    }

    // Determine if the buffer can be allocated
#if 0
    pix_size   = (unsigned long)(dev->pix.width * dev->pix.height * dev->fmt->bypp);
#else
    pix_size   = (unsigned long)VVIDEO_HW_BUF_LEN(dev->desc);
#endif
    buf_size   = (unsigned long)PAGE_ALIGN(pix_size);
    buf_avail  = (unsigned long)(dev->mem_top - dev->mem_free);
    buf_offset = (unsigned long)(dev->mem_free - dev->mem_base);

    DTRACE("allocating buffer %d, %ux%u:%u, pix_size %lu, buf_size %lu, buf_avail %lu, buf_offset %lu\n",
      dev->nbufs, dev->pix.width, dev->pix.height, dev->fmt->bypp,
      pix_size, buf_size, buf_avail, buf_offset);

    if (buf_avail < buf_size) {
	ETRACE("can't allocate buffer, buf_avail %lu < buf_size %lu\n", buf_avail, buf_size);
	return -ENOMEM;
    }

    buf            = &dev->buf[ dev->nbufs ];
    buf->index     = dev->nbufs;
    buf->type      = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf->bytesused = 0;
    buf->field     = 0;
    buf->sequence  = 0;
    buf->memory    = V4L2_MEMORY_MMAP;
    buf->m.offset  = buf_offset;
    buf->length    = pix_size;
    buf->input     = 0;
    buf->reserved  = 0;

    dev->nbufs++;
    dev->mem_free += buf_size;

    return 0;
}


static int
vvideo_hw_free_buffers (vvideo_hw_device_t* dev)
{
    DTRACE("freeing all allocated buffers\n");

    // Reset 'free memory' pointer
    dev->mem_free = dev->mem_base;

    // Wipe out all allocated buffers
    memset(dev->buf, 0, sizeof(struct v4l2_buffer) * VVIDEO_HW_BUF_MAX);
    dev->nbufs = 0;

    return 0;
}


static int
vvideo_hw_query_buffer (vvideo_hw_device_t* dev, struct v4l2_buffer* qbuf)
{
    struct v4l2_buffer* buf;

    DTRACE("querying buffer index %u, nbufs %u\n", qbuf->index, dev->nbufs);

    if (qbuf->index >= dev->nbufs) {
	ETRACE("querying invalid buffer index %u\n", qbuf->index);
	return -EINVAL;
    }

    buf = &dev->buf[ qbuf->index ];

    memcpy(qbuf, buf, sizeof(*qbuf));

    return 0;
}


static struct v4l2_buffer*
vvideo_hw_lookup_buffer (vvideo_hw_device_t* dev, void* addr)
{
    struct v4l2_buffer* buf = dev->buf;
    unsigned long       buf_offset;
    unsigned int        i;

    buf_offset = (unsigned long)addr - (unsigned long)dev->mem_base;

    for (i = 0; i < dev->nbufs; i++, buf++) {

	if (buf->m.offset == buf_offset) {

	    DTRACE("looked up addr %p, offset %lu -> buffer index %u\n", addr, buf_offset, buf->index);
	    return buf;
	}
    }

    ETRACE("failed to look up addr %p, offset %lu\n", addr, buf_offset);
    return NULL;
}


static void
vvideo_hw_renderer_callback (void* cookie, void* buf_addr)
{
    vvideo_hw_device_t* dev = (vvideo_hw_device_t*) cookie;
    struct v4l2_buffer* buf = NULL;

    if (buf_addr != NULL) {
	buf = vvideo_hw_lookup_buffer(dev, buf_addr);
    }

    DTRACE("rendered buffer %p -> index %x\n", buf_addr, buf ? buf->index : 0xBAD);

    if (buf == NULL) {
	ETRACE("rendered buffer %p not found\n", buf_addr);
    } else {
	buf->flags &= ~V4L2_BUF_FLAG_QUEUED;
	buf->flags |= V4L2_BUF_FLAG_DONE;

	// Enqueue the rendered buffer.
	fifo_put(&dev->bufq, (void*)buf);
    }
}


static void
vvideo_hw_renderer (vvideo_hw_device_t* dev, unsigned char* buf_addr, unsigned int len)
{
    // We are expected to pass the buffer to the hardware renderer here.
    // When the rendering operation is completed and the buffer
    // can be recycled, then we have to call the renderer callback.

    DTRACE("rendering buffer 0x%08x physical 0x%08x, len %u\n",
      (unsigned) buf_addr, nkops.nk_vtop(buf_addr), len);

    vvideo_hw_renderer_callback((void*)dev, (void*)buf_addr);
}


static int
vvideo_hw_queue_buffer (vvideo_hw_device_t* dev, struct v4l2_buffer* qbuf)
{
    struct v4l2_buffer* buf;
    unsigned char*      buf_addr;

    DTRACE("queueing buffer index %u, nbufs %u\n", qbuf->index, dev->nbufs);

    if (qbuf->index >= dev->nbufs) {
	ETRACE("queueing invalid buffer index %u\n", qbuf->index);
	return -EINVAL;
    }

    buf = &dev->buf[ qbuf->index ];

    if (buf->flags & V4L2_BUF_FLAG_QUEUED) {
	// The buffer is already queued
	ETRACE("buffer index %u already queued\n", qbuf->index);
	return -EINVAL;
    }

    if (!dev->streaming) {
	// Streaming is not yet enabled; just skip rendering
	buf->flags |= V4L2_BUF_FLAG_DONE;

	// Enqueue the rendered buffer.
	fifo_put(&dev->bufq, (void*)buf);
	return 0;
    }

    buf->flags |= V4L2_BUF_FLAG_QUEUED;
    buf->bytesused = buf->length; // "filled len" not set by Android

    buf_addr = dev->mem_base + buf->m.offset;

    // Submit the buffer to the hw renderer. The result of the rendering
    // will be asynchronously notified via vvideo_hw_renderer_callback().
    vvideo_hw_renderer(dev, buf_addr, buf->bytesused);

    return 0;
}


static int
vvideo_hw_dequeue_buffer (vvideo_hw_device_t* dev, struct v4l2_buffer* qbuf, int wait)
{
    struct v4l2_buffer* buf;
    int                 err;

    DTRACE("waiting for a buffer to dequeue\n");

    // Dequeue a rendered buffer
    err = fifo_get(&dev->bufq, (void**) &buf, wait);

    if (err || (buf == NULL)) {
	if (wait) {
	    ETRACE("could not dequeue any buffer\n");
	}
	return -EAGAIN;
    }

    buf->flags &= ~V4L2_BUF_FLAG_DONE;
    buf->bytesused = 0;

#if 0
    memcpy(qbuf, buf, sizeof(struct v4l2_buffer));
#else
    // Minimal buffer processing to make Android's libopencorehw happy.
    qbuf->index    = buf->index;
    qbuf->flags    = buf->flags;
    qbuf->m.offset = buf->m.offset;
#endif

    DTRACE("dequeued buffer index %u, offset %u, nbufs %u\n", qbuf->index, qbuf->m.offset, dev->nbufs);

    return 0;
}


static int
vvideo_hw_stream_on (vvideo_hw_device_t* dev)
{
    if (dev->streaming) {
	ETRACE("streaming already enabled\n");
	return -EBUSY;
    }

    DTRACE("enabling streaming, nbufs %u, pix %ux%u:%u, fmt %08x\n",
      dev->nbufs, dev->pix.width, dev->pix.height, dev->fmt->bypp, dev->fmt->pix_fmt);

    // Configure and start the hw renderer here.

    dev->streaming = 1;

    DTRACE("streaming enabled\n");

    return 0;
}


static int
vvideo_hw_stream_off (vvideo_hw_device_t* dev)
{
    if (!dev->streaming) {
	DTRACE("streaming not enabled\n");
	return 0;
    }

    DTRACE("disabling streaming, nbufs %u, pix %ux%u:%u, fmt %08x\n",
      dev->nbufs, dev->pix.width, dev->pix.height, dev->fmt->bypp, dev->fmt->pix_fmt);

    // Stop the hw renderer here.

    dev->streaming = 0;

    DTRACE("streaming disabled\n");

    return 0;
}


static int
vvideo_hw_set_rotation (vvideo_hw_device_t* dev, int rotation)
{
    DTRACE("setting rotation to %u degrees\n", rotation);

    dev->rotation = rotation;

    return 0;
}


static int
vvideo_hw_ops_ioctl (void* private_data, unsigned int cmd, void* arg)
{
    vvideo_hw_device_t* dev = (vvideo_hw_device_t*) private_data;
    int                 err;

    DTRACE("processing command %08x, arg %p\n", cmd, arg);

    switch (cmd) {

    // Query capabilities
    case VIDIOC_QUERYCAP: {
	struct v4l2_capability* cap = (struct v4l2_capability*) arg;

	DTRACE("querying capabilities\n");

#if 1
	// Hack ahead!
	// The new liboverlay no longer invokes VIDIOC_REQBUFS(count = 0)
	// to explicitly free the buffers already allocated by a previous
	// call to VIDIOC_REQBUFS(count > 0).
	// However, the lib now calls VIDIOC_QUERYCAP when it resizes
	// the overlay, giving us the opportunity to release any existing
	// buffer here.

	// Free all the buffers
	vvideo_hw_free_buffers(dev);
#endif

	memcpy(cap, &dev->cap, sizeof(struct v4l2_capability));
	return 0;
    }

    // Get framebuffer
    case VIDIOC_G_FBUF: {
	struct v4l2_framebuffer* fbuf = (struct v4l2_framebuffer*) arg;

	fbuf->capability = V4L2_FBUF_CAP_EXTERNOVERLAY |
	                   V4L2_FBUF_CAP_GLOBAL_ALPHA  |
	                   V4L2_FBUF_CAP_CHROMAKEY     |
	                   V4L2_FBUF_CAP_SRC_CHROMAKEY;
	fbuf->flags      = dev->fbuf_flags;
	fbuf->base       = 0;
	memcpy(&fbuf->fmt, &dev->pix, sizeof(struct v4l2_pix_format));
	return 0;
    }

    // Set framebuffer
    case VIDIOC_S_FBUF: {
	struct v4l2_framebuffer* fbuf = (struct v4l2_framebuffer*) arg;

	dev->fbuf_flags = fbuf->flags;
	return 0;
    }

    // Get control
    case VIDIOC_G_CTRL: {
	struct v4l2_control* ctrl = (struct v4l2_control*) arg;

	DTRACE("getting control %08x\n", ctrl->id);

	switch (ctrl->id) {
	case V4L2_CID_ROTATE: {

	    DTRACE("getting rotation %d\n", dev->rotation);

	    ctrl->value = dev->rotation;
	    return 0;
	}
        default: {
	    ETRACE("failed to get unknown control %08x\n", ctrl->id);
	    return -EINVAL;
	}
        }
    }

    // Set control
    case VIDIOC_S_CTRL: {
	struct v4l2_control* ctrl = (struct v4l2_control*) arg;

	DTRACE("setting control %08x, value %08x\n", ctrl->id, ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_ROTATE: {

	    DTRACE("setting rotation to %d\n", ctrl->value);

	    err = vvideo_hw_set_rotation(dev, ctrl->value);
	    return err;
	}
        default: {
	    ETRACE("failed to set unknown control %08x\n", ctrl->id);
	    return -EINVAL;
	}
        }
    }

    // Get format
    case VIDIOC_G_FMT: {
	struct v4l2_format* fmt = (struct v4l2_format*) arg;

	DTRACE("getting format type %u\n", fmt->type);

	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT: {
	    memcpy(&fmt->fmt.pix, &dev->pix, sizeof(struct v4l2_pix_format));
	    return 0;
	}

	case V4L2_BUF_TYPE_VIDEO_OVERLAY: {
	    memcpy(&fmt->fmt.win, &dev->win, sizeof(struct v4l2_window));
	    return 0;
	}

	default: {
	    ETRACE("failed to get unknown format type %u\n", fmt->type);
	    return -EINVAL;
	}
	}
    }

    // Set format
    case VIDIOC_S_FMT: {
	struct v4l2_format* fmt = (struct v4l2_format*) arg;

	DTRACE("setting format type %u\n", fmt->type);

	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT: {
	    return vvideo_hw_set_pix(dev, &fmt->fmt.pix);
	}

	case V4L2_BUF_TYPE_VIDEO_OVERLAY: {
	    return vvideo_hw_set_win(dev, &fmt->fmt.win);
	}

	default: {
	    ETRACE("failed to set unknown format type %u\n", fmt->type);
	    return -EINVAL;
	}
        }
    }

    // Get crop
    case VIDIOC_G_CROP: {
	struct v4l2_crop* crop = (struct v4l2_crop*) arg;

	DTRACE("getting crop type %u\n", crop->type);

	if (crop->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
	    ETRACE("failed to get crop type %u\n", crop->type);
	    return -EINVAL;
	}

	memcpy(&crop->c, &dev->crop, sizeof(struct v4l2_rect));
	return 0;
    }

    // Set crop
    case VIDIOC_S_CROP: {
	struct v4l2_crop* crop = (struct v4l2_crop*) arg;

	DTRACE("setting crop type %u\n", crop->type);

	if (crop->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
	    ETRACE("failed to set crop type %u\n", crop->type);
	    return -EINVAL;
	}

	err = vvideo_hw_set_crop(dev, &crop->c);
	return err;
    }

    // Request video buffers
    case VIDIOC_REQBUFS: {
	struct v4l2_requestbuffers* req = (struct v4l2_requestbuffers*) arg;
	unsigned int i;
	unsigned int cacheable;
	unsigned int consistent;

	DTRACE("requesting buffers type %u, memory %u, count %u\n", req->type, req->memory, req->count);

	if (req->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
	    ETRACE("failed to request buffers type %u\n", req->type);
	    return -EINVAL;
	}

	if (req->memory != V4L2_MEMORY_MMAP) {
	    ETRACE("failed to request buffers memory %u\n", req->memory);
	    return -EINVAL;
	}

	if (req->count > (dev->desc->maxbufs - dev->nbufs)) {
	    req->count = (dev->desc->maxbufs - dev->nbufs);
	}

	cacheable  = req->reserved[0] ? 1 : 0;
	consistent = req->reserved[1] ? 1 : 0;

	if (req->count == 0) {
	    // Free all the buffers
	    vvideo_hw_free_buffers(dev);
	    return 0;
	}

	for (i = 0; i < req->count; i++) {
	    err = vvideo_hw_allocate_buffer(dev, cacheable, consistent);
	    if (err) {
		break;
	    }
	}

	// Return the number of buffers allocated.
	req->count = (unsigned int)i;

	DTRACE("allocated %u buffers\n", req->count);

	if (req->count == 0) {
	    ETRACE("unable to allocated any buffer\n");
	    return -ENOMEM;
	}

	return 0;
    }

    // Query a buffer
    case VIDIOC_QUERYBUF: {
	struct  v4l2_buffer* buf  = (struct v4l2_buffer*) arg;

	DTRACE("querying buffer type %u, memory %u, index %u\n", buf->type, buf->memory, buf->index);

	if (buf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
	    ETRACE("failed to query buffer type %u\n", buf->type);
	    return -EINVAL;
	}

	if (buf->memory != V4L2_MEMORY_MMAP) {
	    ETRACE("failed to query buffer memory %u\n", buf->type);
	    return -EINVAL;
	}

	err = vvideo_hw_query_buffer(dev, buf);
	return err;
    }

    // Queue a buffer for rendering
    case VIDIOC_QBUF: {
	struct v4l2_buffer* buf = (struct v4l2_buffer*) arg;

	DTRACE("queueing buffer type %u, memory %u, index %u\n", buf->type, buf->memory, buf->index);

	if (buf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
	    ETRACE("failed to queue buffer type %u\n", buf->type);
	    return -EINVAL;
	}

	if (buf->memory != V4L2_MEMORY_MMAP) {
	    ETRACE("failed to queue buffer memory %u\n", buf->type);
	    return -EINVAL;
	}

	err = vvideo_hw_queue_buffer(dev, buf);
	return err;
    }

    // Dequeue a queued buffer after rendering
    case VIDIOC_DQBUF: {
	struct v4l2_buffer* buf = (struct v4l2_buffer*) arg;
	int                 wait = 1;

	DTRACE("dequeueing buffer\n");

	// This is a blocking call when wait != 0
	err = vvideo_hw_dequeue_buffer(dev, buf, wait);

	DTRACE("dequeued buffer index %u, err %d\n", err == 0 ? buf->index : 999, err);

	return err;
    }

    // Start streaming
    case VIDIOC_STREAMON: {

	DTRACE("starting streaming\n");

	err = vvideo_hw_stream_on(dev);
	return err;
    }

    // Stop streaming
    case VIDIOC_STREAMOFF: {

	DTRACE("stopping streaming\n");

	err = vvideo_hw_stream_off(dev);
	return err;
    }

    // Set rotation
    case VIDIOC_VVIDEO_HW_S_ROTATION: {
	int* rotation = (int*) arg;

	DTRACE("setting rotation to %d\n", *rotation);

	err = vvideo_hw_set_rotation(dev, *rotation);
	return err;
    }

    // Get rotation
    case VIDIOC_VVIDEO_HW_G_ROTATION: {
	int* rotation = (int*) arg;

	*rotation = dev->rotation;

	DTRACE("getting rotation %d\n", *rotation);

	return 0;
    }

    // Set colorkey
    case VIDIOC_VVIDEO_HW_S_COLORKEY: {
	vvideo_hw_colorkey_t* colorkey = (vvideo_hw_colorkey_t*) arg;

	DTRACE("setting colorkey\n");

	memcpy(&dev->colorkey, colorkey, sizeof(vvideo_hw_colorkey_t));
	return 0;
    }

    // Get colorkey
    case VIDIOC_VVIDEO_HW_G_COLORKEY: {
	vvideo_hw_colorkey_t* colorkey = (vvideo_hw_colorkey_t*) arg;

	DTRACE("getting colorkey\n");

	memcpy(colorkey, &dev->colorkey, sizeof(vvideo_hw_colorkey_t));
	return 0;
    }

    // Enable colorkey
    case VIDIOC_VVIDEO_HW_COLORKEY_ENABLE: {

	DTRACE("enabling colorkey\n");

	return 0;
    }

    // Disable colorkey
    case VIDIOC_VVIDEO_HW_COLORKEY_DISABLE: {

	DTRACE("disabling colorkey\n");

	return 0;
    }
    }

    ETRACE("unknown command %08x\n", cmd);
    return -EINVAL;
}


static int
vvideo_hw_ops_mmap (void* private_data, unsigned long pgoff, unsigned long* bus_addr)
{
    vvideo_hw_device_t* dev = (vvideo_hw_device_t*) private_data;
    unsigned long       buf_offset;
    void*               buf_addr;
    struct v4l2_buffer* buf;

    buf_offset = pgoff << PAGE_SHIFT;
    buf_addr   = dev->mem_base + buf_offset;

    buf = vvideo_hw_lookup_buffer(dev, buf_addr);

    DTRACE("mapping buffer pgoff %lu, offset %lu, addr %p -> index %u\n",
      pgoff, buf_offset, buf_addr, buf ? buf->index : 999);

    if (buf == NULL) {
	DTRACE("buffer pgoff %lu, offset %lu, addr %p not found\n", pgoff, buf_offset, buf_addr);
	return -ENOMEM;
    }

    buf->flags |= V4L2_BUF_FLAG_MAPPED;

    *bus_addr = dev->pmem + buf_offset;

    DTRACE("found buffer pgoff %lu, offset %lu, addr %p, index %u -> bus_addr 0x%lx, len %u\n",
      pgoff, buf_offset, buf_addr, buf->index, *bus_addr, buf->length);

    return 0;
}


static int
vvideo_hw_ops_munmap (void* private_data, unsigned long pgoff, unsigned long size)
{
    vvideo_hw_device_t* dev = (vvideo_hw_device_t*) private_data;
    unsigned long       buf_offset;
    void*               buf_addr;
    struct v4l2_buffer* buf;

    (void) size;

    buf_offset = pgoff << PAGE_SHIFT;
    buf_addr   = dev->mem_base + buf_offset;

    buf = vvideo_hw_lookup_buffer(dev, buf_addr);

    DTRACE("unmapping buffer pgoff %lu, offset %lu, addr %p -> index %u\n",
      pgoff, buf_offset, buf_addr, buf ? buf->index : 999);

    if (buf == NULL) {
	DTRACE("buffer pgoff %lu, offset %lu, addr %p not found\n", pgoff, buf_offset, buf_addr);
	return -ENOMEM;
    }

    buf->flags &= ~V4L2_BUF_FLAG_MAPPED;

    DTRACE("unmapped buffer pgoff %lu, offset %lu, addr %p, index %u\n",
      pgoff, buf_offset, buf_addr, buf->index);

    return 0;
}


static int
vvideo_hw_dev_init (vvideo_hw_device_t* dev, NkPhAddr plink)
{
    unsigned long           pmem_size;
    unsigned char*          mem_base;
    struct v4l2_capability* cap;
    struct v4l2_pix_format* pix;
    struct v4l2_window*     win;
    struct v4l2_rect*       crop;

    DTRACE("initializing device %d\n", dev->minor);

    // Device already initialized?
    if (dev->pmem) {
	DTRACE("device %d already initialized\n", dev->minor);
	return 0;
    }

    dev->desc = &vvideo_hw_desc[ dev->minor ];

    pmem_size = dev->desc->pmem_size;

    DTRACE("allocating %lu bytes from pmem\n", pmem_size);

    // Allocate the video buffers from the persistent shared memory area.
    dev->pmem = nkops.nk_pmem_alloc(plink, 0, pmem_size);
    if (!dev->pmem) {
	ETRACE("nk_pmem_alloc(%lu bytes) failed\n", pmem_size);
	return -ENOMEM;
    }

    DTRACE("allocated pmem [0x%lx + len %lu -> 0x%lx]\n",
      (unsigned long)dev->pmem, pmem_size, (unsigned long)dev->pmem + pmem_size);

    // Map the pmem video buffers.
    mem_base = (unsigned char*) nkops.nk_mem_map(dev->pmem, pmem_size);
    if (!mem_base) {
	ETRACE("nk_mem_map failed\n");
	return -ENOMEM;
    }

    // Set the buffer management pointers.
    dev->mem_base = mem_base;
    dev->mem_top  = dev->mem_base + pmem_size;
    dev->mem_free = dev->mem_base;

    DTRACE("mem_base %p + len %lu -> mem_top %p\n", dev->mem_base, pmem_size, dev->mem_top);

    vvideo_hw_free_buffers(dev);

    if (fifo_init(&dev->bufq, VVIDEO_HW_BUF_MAX) != 0) {
	ETRACE("fifo_init failed\n");
	return -ENOMEM;
    }

    // Set the device's capabilities
    cap = &dev->cap;
    strncpy((char*)cap->driver,   "hw",  sizeof(cap->driver));
    strncpy((char*)cap->card,     "VLX", sizeof(cap->card));
    strncpy((char*)cap->bus_info, "",    sizeof(cap->bus_info));
    cap->version                  = 0;
    cap->capabilities             = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT;

    // Set the default framebuffer flags
    dev->fbuf_flags = 0;

    // Set the default pixel format
    dev->fmt = &vvideo_hw_pix_fmt[ 0 ]; // RGB565

    // Set the default pix format
    pix               = &dev->pix;
    pix->width        = dev->desc->width;
    pix->height       = dev->desc->height;
    pix->pixelformat  = dev->fmt->pix_fmt;
    pix->field        = V4L2_FIELD_NONE;
    pix->bytesperline = pix->width * dev->fmt->bypp;
    pix->sizeimage    = pix->bytesperline * pix->height;
    pix->priv         = 0;
    pix->colorspace   = dev->fmt->colorspace;

    // Set the default window
    win               = &dev->win;
    win->w.left       = 0;
    win->w.top        = 0;
    win->w.width      = pix->width;
    win->w.height     = pix->height;
    win->field        = V4L2_FIELD_NONE;
    win->chromakey    = 0;
    win->clips        = NULL;
    win->clipcount    = 0;
    win->bitmap       = NULL;
    win->global_alpha = 0;

    // Set the default crop
    crop              = &dev->crop;
    crop->left        = 0;
    crop->top         = 0;
    crop->width       = pix->width;
    crop->height      = pix->height;

    return 0;
}


static int
vvideo_hw_dev_deinit (vvideo_hw_device_t* dev)
{
    DTRACE("deinitializing device %d\n", dev->minor);

    vvideo_hw_stream_off(dev);

    if (dev->mem_base != NULL) {
	nkops.nk_mem_unmap(dev->mem_base, dev->pmem, dev->desc->pmem_size);
	dev->mem_base = NULL;
	dev->pmem     = 0;
    }

    return 0;
}


static struct fb_info*
vvideo_get_fbinfo (int dev_id)
{
    int minor = dev_id;

    DTRACE("looking for fb %d (%d fb registered)\n", dev_id, num_registered_fb);

    if (minor > FB_MAX) {
	return NULL;
    }

    if (minor >= 0) {
	// Return the fb identified by its minor number.
	return registered_fb[ minor ];
    }

    for (minor = 0; minor < FB_MAX; minor++) {

	// Return the first registered fb.
	if (registered_fb[ minor ] != NULL) {
	    return registered_fb[ minor ];
	}
    }

    DTRACE("fb %d not found\n", dev_id);

    return NULL;
}


static int
vvideo_hw_dev_desc (vvideo_hw_device_t* dev, VVideoDesc* vdesc)
{
    vvideo_hw_desc_t* desc = NULL;

    DTRACE("describing device %d\n", dev->minor);

    if (dev->minor >= VVIDEO_HW_DESC_MAX) {
	return -EINVAL;
    }

    desc = &vvideo_hw_desc[ dev->minor ];

    switch (desc->type) {
    case VVIDEO_TYPE_OVERLAY: {
	struct fb_info* info;

	info = vvideo_get_fbinfo(desc->dev_id);
	if (info == NULL) {
	    return -EINVAL;
	}

	DTRACE("fix smem_len %u, line_length %u\n",
	  info->fix.smem_len, info->fix.line_length);
	DTRACE("var xres %u, xyres %u, xres_virtual %u xres_virtual %u, bpp %u\n",
	  info->var.xres, info->var.yres,
	  info->var.xres_virtual, info->var.yres_virtual,
	  info->var.bits_per_pixel);

	// We assume that the framebuffer's resolution never changes.
	if (desc->width == 0) {
	    desc->width = info->var.xres;
	}

	if (desc->height == 0) {
	    desc->height = info->var.yres;
	}

	if (desc->bypp == 0) {
	    desc->bypp = info->var.bits_per_pixel / 8;
	}

	if (desc->pmem_size == 0) {
	    desc->pmem_size = VVIDEO_HW_LEN(desc);
	}
	break;
    }
    case VVIDEO_TYPE_CAMERA: {
	// Cameras are not there yet
	return -EINVAL;
    }
    default:
	// Invalid device type
	return -EINVAL;
    }

    printk(VVIDEO_HW_NAME ": %s %d: allocating %dx %ux%u:%u buffers, total %lu bytes\n",
      desc->name, dev->minor,
      desc->maxbufs, desc->width, desc->height, desc->bypp, desc->pmem_size);

    // Fill the device descriptor shared with the front-end.
    vdesc->type      = desc->type;
    vdesc->pmem_size = desc->pmem_size;

    return 0;
}


static int
vvideo_hw_ops_open (int minor, NkPhAddr plink, VVideoDesc* desc, void** private_data)
{
    vvideo_hw_device_t* dev;
    int                 err;

    DTRACE("opening device %d\n", minor);

    if ((minor < VVIDEO_HW_DEV_MIN) || (minor > VVIDEO_HW_DEV_MAX)) {
	return -EINVAL;
    }

    if (mutex_lock_interruptible(&vvideo_hw_lock)) {
	return -EINTR;
    }

    // Only a single open is supported
    if (vvideo_hw_dev[ minor ]) {
	ETRACE("device %d already open\n", minor);
	mutex_unlock(&vvideo_hw_lock);
	return -EBUSY;
    }

    dev = kmalloc(sizeof(vvideo_hw_device_t), GFP_KERNEL);
    if (dev == NULL) {
	ETRACE("device %d allocation failed\n", minor);
	mutex_unlock(&vvideo_hw_lock);
	return -ENOMEM;
    }
    memset(dev, 0, sizeof(vvideo_hw_device_t));
    dev->minor = minor;

    // Retrieve device description and compute the size the video buffers.
    err = vvideo_hw_dev_desc(dev, desc);
    if (err) {
	kfree(dev);
	mutex_unlock(&vvideo_hw_lock);
	return err;
    }

    // Init the device, retrieving video buffers from the persistent memory area.
    err = vvideo_hw_dev_init(dev, plink);
    if (err) {
	kfree(dev);
	mutex_unlock(&vvideo_hw_lock);
	return err;
    }

    vvideo_hw_dev[ minor ] = dev;

    DTRACE("opened device %p, minor %u\n", dev, dev->minor);

    *private_data = dev;

    mutex_unlock(&vvideo_hw_lock);
    return err;
}


static int
vvideo_hw_ops_release (void* private_data)
{
    vvideo_hw_device_t* dev = (vvideo_hw_device_t*) private_data;

    DTRACE("closing device %d\n", dev->minor);

    if (mutex_lock_interruptible(&vvideo_hw_lock)) {
	return -EINTR;
    }

    vvideo_hw_stream_off(dev);

    vvideo_hw_dev_deinit(dev);

    vvideo_hw_dev[ dev->minor ] = NULL;

    kfree(dev);

    mutex_unlock(&vvideo_hw_lock);
    return 0;
}



// Generic FIFO queue.

#define FIFO_ASSERT(cond) BUG_ON(!(cond))

static int
fifo_init (fifo_t* fifo, int len)
{
    fifo->slot = (void**) kmalloc(sizeof(void*) * len, GFP_KERNEL);
    if (fifo->slot == NULL) {
	return -ENOMEM;
    }

    fifo->len   = len;
    fifo->prod  = 0;
    fifo->cons  = 0;
    fifo->count = 0;

    mutex_init(&fifo->mutex);
    init_waitqueue_head(&fifo->cond);

    return 0;
}


static int
fifo_put (fifo_t* fifo, void* data)
{
    if (mutex_lock_interruptible(&fifo->mutex)) {
	return -EINTR;
    }

    FIFO_ASSERT(fifo->count <= fifo->len);
    FIFO_ASSERT(fifo->count >= 0);

    if (fifo->count == fifo->len) {
	mutex_unlock(&fifo->mutex);
	return -EAGAIN;
    }

    fifo->slot[ fifo->prod ] = data;

    if (++fifo->prod == fifo->len) {
	fifo->prod = 0;
    }

    fifo->count++;

    mutex_unlock(&fifo->mutex);

    wake_up_interruptible(&fifo->cond);

    return 0;
}


static int
fifo_poll (fifo_t* fifo)
{
    int pending;

    if (mutex_lock_interruptible(&fifo->mutex)) {
	return -EINTR;
    }

    pending = (fifo->count != 0);

    mutex_unlock(&fifo->mutex);

    return pending;
}


static int
fifo_get (fifo_t* fifo, void** data, int wait)
{
    int err = -1;

    if (mutex_lock_interruptible(&fifo->mutex)) {
	return -EINTR;
    }

    FIFO_ASSERT(fifo->count <= fifo->len);
    FIFO_ASSERT(fifo->count >= 0);

    if (wait) {
	while (fifo->count == 0) {

	    mutex_unlock(&fifo->mutex);
	    wait_event_interruptible(fifo->cond, fifo_poll(fifo));
	    mutex_lock(&fifo->mutex);
	}
    }

    if (fifo->count) {
	*data = fifo->slot[ fifo->cons ];
	err = 0;

	if (++fifo->cons == fifo->len) {
	    fifo->cons = 0;
	}

	fifo->count--;
    }

    mutex_unlock(&fifo->mutex);

    return err;
}


// Local, back-end-side-only vvideo interface that
// merely enables to mmap() the video buffers already
// allocated by the front-end via the above hw_ops interface.

static int	      vvideo_hw_chrdev = 0;
static struct class*  vvideo_hw_class  = NULL;
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
static struct device*
#else
static struct class_device*
#endif
		      vvideo_hw_device[ VVIDEO_HW_DEV_MAX + 1 ] = { NULL };

#define VVIDEO_HW_MAJOR  VIDEO_MAJOR  // V4L2 major
#define VVIDEO_HW_CHRDEV "video"      // V4L2 device name


static int
vvideo_hw_fops_open (struct inode* inode, struct file* file)
{
    unsigned int        minor = iminor(inode);
    vvideo_hw_device_t* dev;

    if ((minor < VVIDEO_HW_DEV_MIN) || (minor > VVIDEO_HW_DEV_MAX)) {
	return -EINVAL;
    }

    if (mutex_lock_interruptible(&vvideo_hw_lock)) {
	return -EINTR;
    }

    dev = vvideo_hw_dev[ minor ];

    if (dev == NULL) {
	ETRACE("device %d not yet instanciated\n", minor);
	mutex_unlock(&vvideo_hw_lock);
	return -EAGAIN;
    }

    DTRACE("opened device %d\n", minor);

    mutex_unlock(&vvideo_hw_lock);
    return 0;
}


static int
vvideo_hw_fops_release (struct inode* inode, struct file* file)
{
    unsigned int minor = iminor(inode);

    (void) minor;

    DTRACE("closed device %d\n", minor);

    return 0;
}


static int
vvideo_hw_do_mmap (
    struct vm_area_struct* vma, unsigned long bus_addr,
    unsigned long pgoff, unsigned long size, unsigned int cacheable)
{
    char* vaddr = (char*)vma->vm_start;

    cacheable = 0; // force non-cacheable

    if (!cacheable) {
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
    }

    vma->vm_flags |= VM_IO;

    if (io_remap_pfn_range(vma,
	                   (unsigned long)vaddr,
	                   (unsigned long)bus_addr >> PAGE_SHIFT,
	                   size,
	                   vma->vm_page_prot)) {
	return -EAGAIN;
    }

    DTRACE("vma start %08lx, end %08lx, "
      "size %ld, flags %08lx, prot %08lx, pgoff %08lx, bus_addr %08lx\n",
      vma->vm_start, vma->vm_end, size, vma->vm_flags, vma->vm_page_prot, pgoff, bus_addr);

    return 0;
}


static int
vvideo_hw_fops_mmap (struct file *file, struct vm_area_struct *vma)
{
    int                 minor = MINOR (file->f_dentry->d_inode->i_rdev);
    unsigned long       size;
    unsigned long       offset;
    unsigned long       bus_addr;
    void*               buf_addr;
    int                 err;
    vvideo_hw_device_t* dev;
    struct v4l2_buffer* buf;

    if (mutex_lock_interruptible(&vvideo_hw_lock)) {
	return -EINTR;
    }

    dev = vvideo_hw_dev[ minor ];

    if (dev == NULL) {
	ETRACE("device %d not instanciated\n", minor);
	mutex_unlock(&vvideo_hw_lock);
	return -EIO;
    }

    size     = vma->vm_end - vma->vm_start;
    offset   = vma->vm_pgoff << PAGE_SHIFT;
    buf_addr = dev->mem_base + offset;

    DTRACE("looking up buffer addr %p, offset %lu, size %lu\n", buf_addr, offset, size);

    buf = vvideo_hw_lookup_buffer(dev, buf_addr);
    if (buf == NULL) {
	ETRACE("failed to look up buffer addr %p, offset %lu, size %lu\n", buf_addr, offset, size);
	mutex_unlock(&vvideo_hw_lock);
	return -EINVAL;
    }

    DTRACE("looked up buffer index %u, size %u, buf_addr %p\n", buf->index, buf->length, buf_addr);

    if (size > buf->length) {
	ETRACE("can't map buffer index %u, addr %p, offset %lu, map size %lu > buf size %u\n",
	  buf->index, buf_addr, offset, size, buf->length);
	mutex_unlock(&vvideo_hw_lock);
	return -EINVAL;
    }

    // Retrieve the buffer's physical address
    bus_addr = dev->pmem + offset;

    // Map the buffer
    err = vvideo_hw_do_mmap(vma, bus_addr, vma->vm_pgoff, size, 0);
    if (err) {
	ETRACE("error %d mapping buffer index %u, addr %p, offset %lu, buf size %u, map size %lu\n",
	  err, buf->index, buf_addr, offset, buf->length, size);
	mutex_unlock(&vvideo_hw_lock);
	return -EINVAL;
    }

    DTRACE("mapped buffer index %u, addr %p, buf size %u, map size %lu -> bus_addr 0x%08lx\n",
      buf->index, buf_addr, buf->length, size, bus_addr);

    mutex_unlock(&vvideo_hw_lock);
    return 0;
}


static const struct file_operations vvideo_hw_file_ops = {
    .owner	= THIS_MODULE,
    .open	= vvideo_hw_fops_open,
    .mmap	= vvideo_hw_fops_mmap,
    .release	= vvideo_hw_fops_release,
    .llseek	= no_llseek,
};


static int
vvideo_hw_init (void)
{
    static int     initialized = 0;
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    struct device* dev;
#else
    struct class_device* dev;
#endif
    int            minor;

    if (initialized++) return 0;

    mutex_init(&vvideo_hw_lock);

    vvideo_hw_class = class_create(THIS_MODULE, VVIDEO_HW_CHRDEV);
    if (IS_ERR(vvideo_hw_class)) {
	ETRACE("class_create() failed");
	return -EIO;
    }

    for (minor = VVIDEO_HW_DEV_MIN; minor < VVIDEO_HW_DEV_MAX; minor++) {

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
	dev = device_create
#else
	dev = class_device_create
#endif
	    (vvideo_hw_class, NULL, MKDEV(VVIDEO_HW_MAJOR, minor),
	     NULL, VVIDEO_HW_CHRDEV "%d", minor);
	if (IS_ERR(dev)) {
	    ETRACE("device_create(minor %d) failed\n", minor);
	    return -EIO;
	}

	vvideo_hw_device[ minor ] = dev;
    }

    if (register_chrdev(VVIDEO_HW_MAJOR, VVIDEO_HW_CHRDEV, &vvideo_hw_file_ops)) {
	ETRACE("register_chrdev() failed\n");
	return -EIO;
    }
    vvideo_hw_chrdev = 1;

    printk(VVIDEO_HW_NAME " module initialized\n");

    return 0;
}

static int
vvideo_hw_deinit (void)
{
    int minor;

    for (minor = VVIDEO_HW_DEV_MIN; minor < VVIDEO_HW_DEV_MAX; minor++) {

	if (vvideo_hw_device[ minor ]) {
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
	    device_destroy
#else
	    class_device_destroy
#endif
		(vvideo_hw_class, MKDEV(VVIDEO_HW_MAJOR, minor));
	}
    }

    if (vvideo_hw_class != NULL) {
	class_destroy(vvideo_hw_class);
    }

    if (vvideo_hw_chrdev != 0) {
	unregister_chrdev(VVIDEO_HW_MAJOR, VVIDEO_HW_CHRDEV);
    }

    return 0;
}


static vvideo_hw_ops_t vvideo_hw_ops = {
    .version = VVIDEO_HW_OPS_VERSION,
    .open    = vvideo_hw_ops_open,
    .release = vvideo_hw_ops_release,
    .ioctl   = vvideo_hw_ops_ioctl,
    .mmap    = vvideo_hw_ops_mmap,
    .munmap  = vvideo_hw_ops_munmap,
};


// weak hw ops entry point, overridden on
// platforms featuring a real hardware overlay.
int __attribute__ ((weak))
vvideo_hw_ops_init (vvideo_hw_ops_t** hw_ops)
{
    if (hw_ops) {
	if (vvideo_hw_init() != 0) {
	    vvideo_hw_deinit();
	}

	*hw_ops = &vvideo_hw_ops;
    }

    return 0;
}


static int
vvideo_hw_module_init (void)
{
    printk(VVIDEO_HW_NAME " module loaded\n");

    return 0;
}


static void
vvideo_hw_module_exit (void)
{
    vvideo_hw_deinit();

    printk(VVIDEO_HW_NAME " module unloaded\n");
}

EXPORT_SYMBOL (vvideo_hw_ops_init);

MODULE_LICENSE ("GPL");

module_init(vvideo_hw_module_init);
module_exit(vvideo_hw_module_exit);

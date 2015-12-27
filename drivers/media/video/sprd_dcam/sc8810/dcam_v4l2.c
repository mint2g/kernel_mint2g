/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <media/videobuf-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include "../common/dcam_service.h"
#include "../common/sensor_drv.h"

JINF_EXIF_INFO_T *g_dc_exif_info_ptr = NULL;

#define INVALID_VALUE		0xff
//add for redusing start time
#define DEFAULT_WB_VALUE		0
#define DEFAULT_METERING_VALUE	0
#define DEFAULT_EFFECT_VALUE	0
#define DEFAULT_EV_VALUE		4
#define DEFAULT_SCENE_VALUE		0
//end

#define DCAM_MINOR MISC_DYNAMIC_MINOR
#define V4L2_OPEN_FOCUS 1
#define DCAM_SCALE_OUT_WIDTH_MAX    960
#define DCAM_TIME_OUT                             2000
#define DCAM_TIME_OUT_FOR_ATV            2000
#define DCAM_RESTART_COUNT   2	//3
#define DCAM_RESTART_TIMEOUT 	1000

						   /*#define FLASH_DV_OPEN_ON_RECORD		1*//* mode 1: samsung */
#define FLASH_DV_OPEN_ALWAYS		1	/* mode 2: HTC, default */
#define DCAM_HANDLE_TIMEOUT     200
#define DCAM_THREAD_END_FLAG   0xFF

/*mode of DCAM*/
#define DCAM_PREVIEW_MODE  0x0
#define DCAM_VIDEO_MODE      0x1

#define init_MUTEX(sem)		sema_init(sem, 1)
#define init_MUTEX_LOCKED(sem)	sema_init(sem, 0)
static struct task_struct *s_dcam_thread;

typedef struct dcam_info {
	SENSOR_MODE_E preview_m;
	SENSOR_MODE_E snapshot_m;
	DCAM_SIZE_T0 input_size;
	DCAM_MODE_TYPE_E mode;
	DCAM_SIZE_T0 out_size;
	DCAM_ROTATION_E rot_angle;
	DCAM_DATA_FORMAT_E out_format;
	uint32_t zoom_multiple;
	uint32_t jpg_len;
	uint8_t wb_param;
	uint8_t metering_param;
	uint8_t fps_param;
	uint8_t brightness_param;
	uint8_t contrast_param;
	uint8_t saturation_param;
	uint8_t imageeffect_param;
	uint8_t dtp_param;
	uint8_t hflip_param;
	uint8_t vflip_param;
	uint8_t previewmode_param;
	uint8_t focus_param;
	uint8_t ev_param;
	uint8_t sensor_work_mode;
	uint8_t power_freq;
	uint8_t flash_mode;
	uint8_t recording_start;
	volatile uint8_t v4l2_buf_ctrl_set_next_flag;
	volatile uint8_t v4l2_buf_ctrl_path_done_flag;
	volatile uint8_t is_streamoff;
	uint8_t iso_param;
	uint16_t rsd0;
} DCAM_INFO_T;

typedef enum {
	DCAM_START_OK = 0,
	DCAM_OK,
	DCAM_RUN,
	DCAM_LINE_ERR,
	DCAM_FRAME_ERR,
	DCAM_CAP_FIFO_OVERFLOW,
	DCAM_NO_RUN,
	DCAM_JPG_BUF_ERR,
	DCAM_RESTART,
	DCAM_RESTART_PROCESS,
	DCAM_RESTAER_FAIL,
	DCAM_WORK_STATUS_MAX
} DCAM_WORK_STATUS;

typedef enum {
	FLASH_CLOSE = 0x0,
	FLASH_OPEN = 0x1,
	FLASH_TORCH = 0x2,	/*user only set flash to close/open/torch state */
	FLASH_AUTO = 0x3,
	FLASH_CLOSE_AFTER_OPEN = 0x10,	/* following is set to sensor */
	FLASH_HIGH_LIGHT = 0x11,
	FLASH_OPEN_ON_RECORDING = 0x22,
	FLASH_STATUS_MAX
} DCAM_FLASH_STATUS;

typedef struct _dcam_error_info_tag {
	struct task_struct *th;
	DCAM_WORK_STATUS work_status;
	uint8_t is_report_err;
	uint8_t restart_cnt;
	uint8_t is_restart;
	uint8_t is_running;
	uint8_t ret;
	volatile uint8_t is_stop;
	uint8_t is_wakeup_thread;
	uint8_t rsd;
	uint32_t timeout_val;
	void *priv;
	DCAM_MODE_TYPE_E mode;
	struct semaphore dcam_start_sem;
	struct semaphore dcam_thread_sem;
	struct semaphore dcam_thread_wakeup_sem;
	struct timer_list dcam_timer;
} DCAM_ERROR_INFO_T;

typedef enum
{
	DCAM_AF_IDLE,
	DCAM_AF_GOING,
	DCAM_AF_ERR,
	DCAM_AF_OK,
	DCAM_AF_MAX,
}dcam_af_status;

static dcam_af_status s_auto_focus;

static DCAM_ERROR_INFO_T s_dcam_err_info;
uint32_t g_first_buf_addr = 0;	/*store the first buffer address */
uint32_t g_first_buf_uv_addr = 0;	/*store the address of uv buffer */
uint32_t g_last_buf = 0xFFFFFFFF;	/*record the last buffer for dcam driver */
uint32_t g_last_uv_buf = 0xFFFFFFFF;
struct dcam_fh *g_fh = NULL;	/*store the fh pointer for ISR callback function */
static uint32_t g_is_first_frame = 1;	/*store the flag for the first frame */
DCAM_INFO_T g_dcam_info;	/*store the dcam and sensor config info */
uint32_t g_zoom_level = 0;	/*zoom level: 0: 1x, 1: 2x, 2: 3x, 3: 4x */
uint32_t g_is_first_irq = 1;
static uint32_t s_int_ctrl_flag = 0;

#define DCAM_MODULE_NAME "dcam"
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001

#define DCAM_MAJOR_VERSION 0
#define DCAM_MINOR_VERSION 6
#define DCAM_RELEASE 0
#define DCAM_VERSION \
	KERNEL_VERSION(DCAM_MAJOR_VERSION, DCAM_MINOR_VERSION, DCAM_RELEASE)

static unsigned video_nr = -1;
module_param(video_nr, uint, 0644);
MODULE_PARM_DESC(video_nr, "videoX start number, -1 is autodetect");

static unsigned debug;
module_param(debug, uint, 0644);
MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
module_param(vid_limit, uint, 0644);
MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

/* R   G   B */
#define COLOR_WHITE	{204, 204, 204}
#define COLOR_AMBAR	{208, 208,   0}
#define COLOR_CIAN	{  0, 206, 206}
#define COLOR_GREEN	{  0, 239,   0}
#define COLOR_MAGENTA	{239,   0, 239}
#define COLOR_RED	{205,   0,   0}
#define COLOR_BLUE	{  0,   0, 255}
#define COLOR_BLACK	{  0,   0,   0}

struct bar_std {
	u8 bar[8][3];
};

/* Maximum number of bars are 10 - otherwise, the input print code
   should be modified */
static struct bar_std bars[] = {
	{			/* Standard ITU-R color bar sequence */
	 {
	  COLOR_WHITE,
	  COLOR_AMBAR,
	  COLOR_CIAN,
	  COLOR_GREEN,
	  COLOR_MAGENTA,
	  COLOR_RED,
	  COLOR_BLUE,
	  COLOR_BLACK,
	  }
	 }, {
	     {
	      COLOR_WHITE,
	      COLOR_AMBAR,
	      COLOR_BLACK,
	      COLOR_WHITE,
	      COLOR_AMBAR,
	      COLOR_BLACK,
	      COLOR_WHITE,
	      COLOR_AMBAR,
	      }
	     }, {
		 {
		  COLOR_WHITE,
		  COLOR_CIAN,
		  COLOR_BLACK,
		  COLOR_WHITE,
		  COLOR_CIAN,
		  COLOR_BLACK,
		  COLOR_WHITE,
		  COLOR_CIAN,
		  }
		 }, {
		     {
		      COLOR_WHITE,
		      COLOR_GREEN,
		      COLOR_BLACK,
		      COLOR_WHITE,
		      COLOR_GREEN,
		      COLOR_BLACK,
		      COLOR_WHITE,
		      COLOR_GREEN,
		      }
		     },
};

#define NUM_INPUTS ARRAY_SIZE(bars)

#define TO_Y(r, g, b) \
	(((16829 * r + 33039 * g + 6416 * b  + 32768) >> 16) + 16)
/* RGB to  V(Cr) Color transform */
#define TO_V(r, g, b) \
	(((28784 * r - 24103 * g - 4681 * b  + 32768) >> 16) + 128)
/* RGB to  U(Cb) Color transform */
#define TO_U(r, g, b) \
	(((-9714 * r - 19070 * g + 28784 * b + 32768) >> 16) + 128)

/* supported controls */
static struct v4l2_queryctrl dcam_qctrl[] = {
	{
	 .id = V4L2_CID_AUDIO_VOLUME,
	 .name = "Volume",
	 .minimum = 0,
	 .maximum = 65535,
	 .step = 65535 / 100,
	 .default_value = 65535,
	 .flags = V4L2_CTRL_FLAG_SLIDER,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 }, {
	     .id = V4L2_CID_BRIGHTNESS,
	     .type = V4L2_CTRL_TYPE_INTEGER,
	     .name = "Brightness",
	     .minimum = 0,
	     .maximum = 255,
	     .step = 1,
	     .default_value = 127,
	     .flags = V4L2_CTRL_FLAG_SLIDER,
	     }, {
		 .id = V4L2_CID_CONTRAST,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "Contrast",
		 .minimum = 0,
		 .maximum = 255,
		 .step = 0x1,
		 .default_value = 0x10,
		 .flags = V4L2_CTRL_FLAG_SLIDER,
		 }, {
		     .id = V4L2_CID_SATURATION,
		     .type = V4L2_CTRL_TYPE_INTEGER,
		     .name = "Saturation",
		     .minimum = 0,
		     .maximum = 255,
		     .step = 0x1,
		     .default_value = 127,
		     .flags = V4L2_CTRL_FLAG_SLIDER,
		     }, {
			 .id = V4L2_CID_HUE,
			 .type = V4L2_CTRL_TYPE_INTEGER,
			 .name = "Hue",
			 .minimum = -128,
			 .maximum = 127,
			 .step = 0x1,
			 .default_value = 0,
			 .flags = V4L2_CTRL_FLAG_SLIDER,
			 }, {
			     .id = V4L2_CID_DO_WHITE_BALANCE,
			     .type = V4L2_CTRL_TYPE_INTEGER,
			     .name = "whitebalance",
			     .minimum = 0,
			     .maximum = 255,
			     .step = 0x1,
			     .default_value = 0,
			     .flags = V4L2_CTRL_FLAG_SLIDER,
			     }, {
				 .id = V4L2_CID_COLORFX,
				 .type = V4L2_CTRL_TYPE_INTEGER,
				 .name = "coloreffect",
				 .minimum = 0,
				 .maximum = 255,
				 .step = 0x1,
				 .default_value = 0,
				 .flags = V4L2_CTRL_FLAG_SLIDER,
				 }, {
				     .id = V4L2_CID_COLOR_KILLER,
				     .type = V4L2_CTRL_TYPE_INTEGER,
				     .name = "scenemode",
				     .minimum = 0,
				     .maximum = 255,
				     .step = 0x1,
				     .default_value = 0,
				     .flags = V4L2_CTRL_FLAG_SLIDER,
				     }, {
					 .id = V4L2_CID_ZOOM_ABSOLUTE,
					 .type = V4L2_CTRL_TYPE_INTEGER,
					 .name = "zoom",
					 .minimum = 0,
					 .maximum = 255,
					 .step = 0x1,
					 .default_value = 0,
					 .flags = V4L2_CTRL_FLAG_SLIDER,
					 },
	{
	 .id = V4L2_CID_FOCUS_AUTO,
	 .name = "Focus, Auto",
	 .type = V4L2_CTRL_TYPE_BOOLEAN,
	 .minimum = 0,
	 .maximum = 255,
	 .step = 0x1,
	 .default_value = 0,
	 .flags = V4L2_CTRL_FLAG_SLIDER,
	 },
	{
	 .id = V4L2_CID_HFLIP,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "hmirror",
	 .minimum = 0,
	 .maximum = 255,
	 .step = 0x1,
	 .default_value = 0,
	 .flags = V4L2_CTRL_FLAG_SLIDER,
	 }, {
	     .id = V4L2_CID_VFLIP,
	     .type = V4L2_CTRL_TYPE_INTEGER,
	     .name = "vmirror",
	     .minimum = 0,
	     .maximum = 255,
	     .step = 0x1,
	     .default_value = 0,
	     .flags = V4L2_CTRL_FLAG_SLIDER,
	     },
	{
	 .id = V4L2_CID_EXPOSURE,
	 .name = "exposure",
	 .type = V4L2_CTRL_TYPE_BOOLEAN,
	 .minimum = 0,
	 .maximum = 255,
	 .step = 0x1,
	 .default_value = 0,
	 .flags = V4L2_CTRL_FLAG_SLIDER,
	 },
	{
	 .id = V4L2_CID_POWER_LINE_FREQUENCY,
	 .name = "power freq",
	 .type = V4L2_CTRL_TYPE_BOOLEAN,
	 .minimum = 0,
	 .maximum = 255,
	 .step = 0x1,
	 .default_value = 0,
	 .flags = V4L2_CTRL_FLAG_SLIDER,
	 },
	{
	 // use V4L2_CID_GAMMA for camera flash
	 .id = V4L2_CID_GAMMA,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "gamma,flah",
	 .minimum = 0,
	 .maximum = 255,
	 .step = 0x1,
	 .default_value = 0,
	 .flags = V4L2_CTRL_FLAG_SLIDER,
	 },
	{
	 .id = V4L2_CID_BLACK_LEVEL,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "preview mode",
	 .minimum = 0,
	 .maximum = 255,
	 .step = 0x1,
	 .default_value = 0,
	 .flags = V4L2_CTRL_FLAG_SLIDER,
	 },
	 {
		.id = V4L2_CID_DTP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "dtp",
		.minimum = 0,
		.maximum = 255,
		.step = 0x1,
		.default_value = 0,
		.flags = V4L2_CTRL_FLAG_SLIDER,
	},
	 {
		.id = V4L2_CID_METERING,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "metering",
		.minimum = 0,
		.maximum = 255,
		.step = 0x1,
		.default_value = 0,
		.flags = V4L2_CTRL_FLAG_SLIDER,
	},
	{
	.id            = V4L2_CID_GAIN,
	.type          = V4L2_CTRL_TYPE_INTEGER,
	.name          = "iso",
	.minimum       = 0,
	.maximum       = 255,
	.step          = 1,
	.default_value = 127,
	.flags         = V4L2_CTRL_FLAG_SLIDER,
	}
};

#define dprintk(dev, level, fmt, arg...)  v4l2_printk(KERN_DEBUG, &dev->v4l2_dev, fmt , ## arg)

/* ------------------------------------------------------------------
	Basic structures
   ------------------------------------------------------------------*/
struct dcam_fmt {
	char *name;
	u32 fourcc;		/* v4l2 format id */
	int depth;
	int flag;		/* 0:qbuf; 1: set driver */
};

static struct dcam_fmt formats[] = {
	{
	 .name = "4:2:2, packed, YUYV",
	 .fourcc = V4L2_PIX_FMT_YUYV,
	 .depth = 16,
	 },
	{
	 .name = "4:2:2, packed, YUV422",
	 .fourcc = V4L2_PIX_FMT_YUV422P,
	 .depth = 16,
	 },
	{
	 .name = "4:2:2, packed, UYVY",
	 .fourcc = V4L2_PIX_FMT_UYVY,
	 .depth = 16,
	 },
	{
	 .name = "4:2:0, packed, YUV",
	 .fourcc = V4L2_PIX_FMT_YUV420,
	 .depth = 16,
	 },
	{
	 .name = "RGB565 (LE)",
	 .fourcc = V4L2_PIX_FMT_RGB565,	/* gggbbbbb rrrrrggg */
	 .depth = 16,
	 },
	{
	 .name = "RGB565 (BE)",
	 .fourcc = V4L2_PIX_FMT_RGB565X,	/* rrrrrggg gggbbbbb */
	 .depth = 16,
	 },
	{
	 .name = "RGB555 (LE)",
	 .fourcc = V4L2_PIX_FMT_RGB555,	/* gggbbbbb arrrrrgg */
	 .depth = 16,
	 },
	{
	 .name = "RGB555 (BE)",
	 .fourcc = V4L2_PIX_FMT_RGB555X,	/* arrrrrgg gggbbbbb */
	 .depth = 16,
	 },
	{
	 .name = "RGB8888",
	 .fourcc = V4L2_PIX_FMT_RGB32,	/* aaaarrrrrgg gggbbbbb */
	 .depth = 32,
	 },
	{
	 .name = "JPEG",
	 .fourcc = V4L2_PIX_FMT_JPEG,
	 .depth = 8,
	 },
};

/* buffer for one video frame */
struct dcam_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;
	struct dcam_fmt *fmt;
};

struct dcam_dmaqueue {
	struct list_head active;

	/* thread for generating video stream */
	struct task_struct *kthread;
	wait_queue_head_t wq;
	/* Counters to control fps rate */
	int frame;
	int ini_jiffies;
};

static LIST_HEAD(dcam_devlist);

struct dcam_dev {
	struct list_head dcam_devlist;
	struct v4l2_device v4l2_dev;
	spinlock_t slock;
	struct mutex lock;
	atomic_t users;
	/* various device info */
	struct video_device *vfd;

	struct dcam_dmaqueue vidq;

	/* Several counters */
	int h, m, s, ms;
	unsigned long jiffies;
	char timestr[13];
	int mv_count;		/* Controls bars movement */
	/* Input Number */
	int input;
	/* Control 'registers' */
	int qctl_regs[ARRAY_SIZE(dcam_qctrl)];
	struct v4l2_streamparm streamparm;
};

struct dcam_fh {
	struct dcam_dev *dev;
	/* video capture */
	struct dcam_fmt *fmt;
	unsigned int width, height;
	struct videobuf_queue vb_vidq;
	enum v4l2_buf_type type;
	unsigned char bars[8][3];
	int input;		/* Input Number on bars */
};
static int dcam_start_timer(struct timer_list *dcam_timer, uint32_t time_val);
static void dcam_stop_timer(struct timer_list *dcam_timer);

void reset_sensor_param(void)
{
	/* Setting sensor parameters */
	if (INVALID_VALUE != g_dcam_info.wb_param)
		Sensor_Ioctl(SENSOR_IOCTL_SET_WB_MODE, g_dcam_info.wb_param);
	if (INVALID_VALUE != g_dcam_info.imageeffect_param)
		Sensor_Ioctl(SENSOR_IOCTL_IMAGE_EFFECT,
			     g_dcam_info.imageeffect_param);
	if (INVALID_VALUE != g_dcam_info.previewmode_param)
		Sensor_Ioctl(SENSOR_IOCTL_PREVIEWMODE,
			     g_dcam_info.previewmode_param);
	if (INVALID_VALUE != g_dcam_info.brightness_param)
		Sensor_Ioctl(SENSOR_IOCTL_BRIGHTNESS,
			     g_dcam_info.brightness_param);
	if(INVALID_VALUE != g_dcam_info.iso_param)
		Sensor_Ioctl(SENSOR_IOCTL_ISO, g_dcam_info.iso_param);
	if (INVALID_VALUE != g_dcam_info.contrast_param)
		Sensor_Ioctl(SENSOR_IOCTL_CONTRAST, g_dcam_info.contrast_param);
	if (INVALID_VALUE != g_dcam_info.ev_param)
		Sensor_Ioctl(SENSOR_IOCTL_EXPOSURE_COMPENSATION,
			     g_dcam_info.ev_param);
	if (INVALID_VALUE != g_dcam_info.power_freq)
		Sensor_Ioctl(SENSOR_IOCTL_ANTI_BANDING_FLICKER,
			     g_dcam_info.power_freq);
	if(INVALID_VALUE != g_dcam_info.sensor_work_mode)
		Sensor_Ioctl(SENSOR_IOCTL_VIDEO_MODE,
			     g_dcam_info.sensor_work_mode);
}
static int init_sensor_parameters(void *priv)
{
	uint32_t i, width, height;
	SENSOR_EXP_INFO_T *sensor_info_ptr = NULL;
	struct dcam_fh *fh = priv;
	struct dcam_dev *dev = fh->dev;
	DCAM_INIT_PARAM_T init_param;

	DCAM_V4L2_PRINT("V4L2: init sensor parameters E.\n");
	sensor_info_ptr = Sensor_GetInfo();

	if (PNULL == sensor_info_ptr) {
		DCAM_V4L2_ERR("v4l2:init_sensor_parameters,get sensor info fail .\n");
		return -1;
	}

	if ((DCAM_ROTATION_0 == g_dcam_info.rot_angle)
	    || (DCAM_ROTATION_180 == g_dcam_info.rot_angle)) {
		init_param.input_size.w = fh->width;
		init_param.input_size.h = fh->height;
	} else {
		init_param.input_size.w = fh->height;
		init_param.input_size.h = fh->width;
	}

	g_dcam_info.preview_m = SENSOR_MODE_PREVIEW_ONE;
	g_dcam_info.snapshot_m = SENSOR_MODE_PREVIEW_ONE;

	//capture
	if(1 == dev->streamparm.parm.capture.capturemode){

		for (i = SENSOR_MODE_SNAPSHOT_ONE_FIRST; i < SENSOR_MODE_PREVIEW_TWO; i++) {
			width = sensor_info_ptr->sensor_mode_info[i].width;
			height = sensor_info_ptr->sensor_mode_info[i].height; 		
			if ((init_param.input_size.w == width) && (init_param.input_size.h == height))  {
				g_dcam_info.snapshot_m =  sensor_info_ptr->sensor_mode_info[i].mode;

				g_dcam_info.input_size.w = width;
				g_dcam_info.input_size.h = height;
				break;
			}
		}
		DCAM_V4L2_PRINT("Sensor: init_sensor_parameters w,h = (%d, %d) ,snapshot_m=%d \n", init_param.input_size.w, init_param.input_size.h, g_dcam_info.snapshot_m);
		Sensor_SetMode(g_dcam_info.snapshot_m);
		
		//get EXP
		Sensor_Ioctl(SENSOR_IOCTL_AFTER_SNAPSHOT,(uint32_t) g_dcam_info.snapshot_m);

		//back to preview
		//Sensor_SetMode(SENSOR_MODE_PREVIEW_ONE);
		
	}

	//preview 
	if(1 != dev->streamparm.parm.capture.capturemode){
		//camera
		if(dev->streamparm.parm.capture.extendedmode != 1){
			DCAM_V4L2_PRINT("Sensor: camera mode \n");
			g_dcam_info.preview_m = SENSOR_MODE_PREVIEW_ONE;
			Sensor_SetMode(SENSOR_MODE_PREVIEW_ONE);		
		}
		//camcorder
		else{
			DCAM_V4L2_PRINT("Sensor: camcorder mode \n");
			g_dcam_info.preview_m = SENSOR_MODE_PREVIEW_ONE;
			Sensor_SetMode(SENSOR_MODE_PREVIEW_ONE);		
			Sensor_SetMode(SENSOR_MODE_PREVIEW_TWO);
		}

		
	}

	
	/*for preview */
	//if(1 != dev->streamparm.parm.capture.capturemode)
	//	reset_sensor_param();
	return 0;
}

static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	struct dcam_fh *fh = priv;
	struct dcam_dev *dev = fh->dev;

	strcpy(cap->driver, "dcam");
	strcpy(cap->card, "dcam");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = DCAM_VERSION;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE |
	    V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_cropcap(struct file *file, void *priv,
			  struct v4l2_cropcap *cc)
{
	if (cc->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	cc->bounds.left = 0;
	cc->bounds.top = 0;
	cc->bounds.width = 2560;
	cc->bounds.height = 2048;
	cc->defrect = cc->bounds;
	cc->pixelaspect.numerator = 54;
	cc->pixelaspect.denominator = 59;
	return 0;
}

static int vidioc_s_crop(struct file *file, void *priv, struct v4l2_crop *crop)
{
	if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	DCAM_V4L2_PRINT
	    ("V4L2: vidioc_s_crop left: %d, top: %d, width: %d,height: %d.\n",
	     crop->c.left, crop->c.top, crop->c.width, crop->c.height);
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	struct dcam_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];
	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct dcam_fh *fh = priv;

	f->fmt.pix.width = fh->width;
	f->fmt.pix.height = fh->height;
	f->fmt.pix.field = fh->vb_vidq.field;
	f->fmt.pix.pixelformat = fh->fmt->fourcc;
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fh->fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	return (0);
}

static struct dcam_fmt *get_format(struct v4l2_format *f)
{
	struct dcam_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}
	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}

static inline unsigned int norm_maxw(void)
{
	uint32_t max_width = 0;
	SENSOR_EXP_INFO_T *sensor_info_ptr = NULL;

	sensor_info_ptr = Sensor_GetInfo();
	if (PNULL == sensor_info_ptr) {
		DCAM_V4L2_ERR("v4l2:norm_maxw,get sensor info fail.\n");
		return 0;
	}

	max_width = (uint32_t) sensor_info_ptr->source_width_max;
	if (max_width < DCAM_SCALE_OUT_WIDTH_MAX)
		max_width = DCAM_SCALE_OUT_WIDTH_MAX;
	DCAM_V4L2_PRINT("V4L2: norm_maxw,max width =%d.\n", max_width);
	return max_width;
}

static inline unsigned int norm_maxh(uint32_t hw_ratio)
{
	uint32_t max_height = 0;
	uint32_t max_width = 0;
	SENSOR_EXP_INFO_T *sensor_info_ptr = NULL;

	sensor_info_ptr = Sensor_GetInfo();
	if (PNULL == sensor_info_ptr) {
		DCAM_V4L2_ERR("v4l2:norm_maxh,get sensor info fail.\n");
		return 0;
	}
	max_height = (uint32_t) sensor_info_ptr->source_height_max;
	max_width = (uint32_t) sensor_info_ptr->source_width_max;
	if(max_width<DCAM_SCALE_OUT_WIDTH_MAX) {
		max_width = DCAM_SCALE_OUT_WIDTH_MAX;
		if(!hw_ratio)
			max_height = (DCAM_SCALE_OUT_WIDTH_MAX *sensor_info_ptr->source_height_max ) /sensor_info_ptr->source_width_max;
		else
			max_height = (DCAM_SCALE_OUT_WIDTH_MAX *sensor_info_ptr->source_width_max ) /sensor_info_ptr->source_height_max;
	}
	DCAM_V4L2_PRINT("V4L2: norm_maxw,max height =%d.\n", max_height);
	return max_height;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	struct dcam_fh *fh = priv;
	struct dcam_dev *dev = fh->dev;
	struct dcam_fmt *fmt;
	enum v4l2_field field;
	unsigned int maxw, maxh;
	unsigned int temp = 0;
	ISP_RECT_T trim_rect;
	uint32_t hw_ratio = 0;

	fmt = get_format(f);
	if (!fmt) {
		dprintk(dev, 1, "Fourcc format (0x%08x) invalid.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}
	field = f->fmt.pix.field;
	if (field == V4L2_FIELD_ANY) {
		field = V4L2_FIELD_INTERLACED;
	} else if (V4L2_FIELD_INTERLACED != field) {
		dprintk(dev, 1, "Field type invalid.\n");
		return -EINVAL;
	}
	hw_ratio = (f->fmt.pix.height > f->fmt.pix.width) ? 1 : 0;
	maxw = norm_maxw();
	maxh = norm_maxh(hw_ratio);
	if (1 == f->fmt.raw_data[199]) {
		if (3 != f->fmt.raw_data[198]) {	//180 degree
			temp = maxw;
			maxw = maxh;
			maxh = temp;
		}
	}
	if (maxw < f->fmt.pix.width) {
		if ((maxw * 4 >= f->fmt.pix.width)
		    && (f->fmt.pix.width <= DCAM_SCALE_OUT_WIDTH_MAX)) {
			maxw = f->fmt.pix.width;
			maxh = f->fmt.pix.height;
		}
	}
	f->fmt.pix.field = field;
	v4l_bound_align_image(&f->fmt.pix.width, 48, maxw, 2,
			      &f->fmt.pix.height, 32, maxh, 0, 0);
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	if ((f->fmt.raw_data[197] != 0)
	    && (f->fmt.pix.width > DCAM_SCALE_OUT_WIDTH_MAX)) {
		trim_rect.x = 0;
		trim_rect.y = 0;
		trim_rect.w = f->fmt.pix.width;
		trim_rect.h = f->fmt.pix.height;
		DCAM_V4L2_PRINT("V4L2: vidioc_try_fmt_vid_cap 0: w=%d,h=%d .\n",
				f->fmt.pix.width, f->fmt.pix.height);
		dcam_get_zoom_trim(&trim_rect, f->fmt.raw_data[2]);
		DCAM_V4L2_PRINT("V4L2: vidioc_try_fmt_vid_cap 1: w=%d,h=%d .\n",
				f->fmt.pix.width, f->fmt.pix.height);
		f->fmt.pix.width = trim_rect.w;
		f->fmt.pix.height = trim_rect.h;
	}
	return 0;
}

/*FIXME: This seems to be generic enough to be at videodev2 */
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct dcam_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;
	int ret = vidioc_try_fmt_vid_cap(file, fh, f);
	if (ret < 0)
		return ret;
	g_dcam_info.is_streamoff = 0;
	mutex_lock(&q->vb_lock);
	if (videobuf_queue_is_busy(&fh->vb_vidq)) {
		dprintk(fh->dev, 1, "%s queue busy\n", __func__);
		ret = -EBUSY;
		goto out;
	}
	fh->fmt = get_format(f);
	fh->width = f->fmt.pix.width;
	fh->height = f->fmt.pix.height;
	fh->vb_vidq.field = f->fmt.pix.field;
	fh->type = f->type;
	g_dcam_info.out_size.w = fh->width;
	g_dcam_info.out_size.h = fh->height;
	if (V4L2_PIX_FMT_YUV420 == f->fmt.pix.pixelformat) {
		g_dcam_info.out_format = DCAM_DATA_YUV420;
	} else if (V4L2_PIX_FMT_YUV422P == f->fmt.pix.pixelformat) {
		g_dcam_info.out_format = DCAM_DATA_YUV422;
	} else if (V4L2_PIX_FMT_JPEG == f->fmt.pix.pixelformat) {
		g_dcam_info.out_format = DCAM_DATA_JPEG;
	} else {
		g_dcam_info.out_format = DCAM_DATA_RGB;
	}
	ret = 0;
out:
	mutex_unlock(&q->vb_lock);
	return ret;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id * i)
{
	return 0;
}

/* only one input in this sample driver */
static int vidioc_enum_input(struct file *file, void *priv,
			     struct v4l2_input *inp)
{
	if (inp->index >= NUM_INPUTS)
		return -EINVAL;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	inp->std = V4L2_STD_525_60;
	sprintf(inp->name, "Camera %u", inp->index);
	return (0);
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct dcam_fh *fh = priv;
	struct dcam_dev *dev = fh->dev;
	*i = dev->input;
	return (0);
}

/* precalculate color bar values to speed up rendering */
static void precalculate_bars(struct dcam_fh *fh)
{
	struct dcam_dev *dev = fh->dev;
	unsigned char r, g, b;
	int k, is_yuv;

	fh->input = dev->input;
	for (k = 0; k < 8; k++) {
		r = bars[fh->input].bar[k][0];
		g = bars[fh->input].bar[k][1];
		b = bars[fh->input].bar[k][2];
		is_yuv = 0;

		switch (fh->fmt->fourcc) {
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
			is_yuv = 1;
			break;
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_RGB565X:
			r >>= 3;
			g >>= 2;
			b >>= 3;
			break;
		case V4L2_PIX_FMT_RGB555:
		case V4L2_PIX_FMT_RGB555X:
			r >>= 3;
			g >>= 3;
			b >>= 3;
			break;
		}

		if (is_yuv) {
			fh->bars[k][0] = TO_Y(r, g, b);	/* Luma */
			fh->bars[k][1] = TO_U(r, g, b);	/* Cb */
			fh->bars[k][2] = TO_V(r, g, b);	/* Cr */
		} else {
			fh->bars[k][0] = r;
			fh->bars[k][1] = g;
			fh->bars[k][2] = b;
		}
	}

}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct dcam_fh *fh = priv;
	struct dcam_dev *dev = fh->dev;

	if (i >= NUM_INPUTS)
		return -EINVAL;
	dev->input = i;
	precalculate_bars(fh);
	return (0);
}

static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(dcam_qctrl); i++)
		if (qc->id && qc->id == dcam_qctrl[i].id) {
			memcpy(qc, &(dcam_qctrl[i]), sizeof(*qc));
			return (0);
		}
	return -EINVAL;
}

/*add wenfeng.yan for esd test*/
static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	if(ctrl->id==V4L2_CID_ESD)
	{
		int ret=0;
		ret=Sensor_Ioctl(SENSOR_IOCTL_GET_ESD, (uint32_t) ctrl->value);
		ctrl->value = ret;
		DCAM_V4L2_PRINT("wenfeng run vidioc_g_ctrl ctrl->value=%d\n",ctrl->value);
		return 0;
	}
	
	struct dcam_fh *fh = priv;
	struct dcam_dev *dev = fh->dev;
	
	int i;

	for (i = 0; i < ARRAY_SIZE(dcam_qctrl); i++)
		if (ctrl->id == dcam_qctrl[i].id) {
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}
	return -EINVAL;
}
/*esd end*/

static void dcam_stop_handle(int param)
{
	uint32_t handle_timeout_cnt = 0;

	if (param) {
		handle_timeout_cnt = 0;
		g_dcam_info.v4l2_buf_ctrl_set_next_flag = 1;
		while (g_dcam_info.v4l2_buf_ctrl_set_next_flag != 0) {
			if (handle_timeout_cnt > DCAM_HANDLE_TIMEOUT)
				break;
			handle_timeout_cnt++;
			msleep(1);
			DCAM_V4L2_PRINT
			    ("V4L2: dcam_stop_handle,handle_timeout_cnt=%d.\n",
			     handle_timeout_cnt);
		}
		dcam_stop_timer(&s_dcam_err_info.dcam_timer);
		dcam_stop();
		s_dcam_err_info.work_status = DCAM_WORK_STATUS_MAX;
		dcam_set_first_buf_addr(g_last_buf, g_last_uv_buf);
	}
}

static void dcam_start_handle(int param)
{
	if (param) {
		s_int_ctrl_flag = 0;
		dcam_start();
		dcam_start_timer(&s_dcam_err_info.dcam_timer,
				 s_dcam_err_info.timeout_val);
	}
}

#define FOCUS_PARAM_COUNT (2+FOCUS_ZONE_CNT_MAX*4)
#define FOCUS_PARAM_LEN   (FOCUS_PARAM_COUNT*2)

static int vidioc_handle_ctrl(struct v4l2_control *ctrl)
{
	int is_previewing = 0;
	SENSOR_EXT_FUN_PARAM_T af_param;
	uint16_t focus_param[FOCUS_PARAM_COUNT] = { 0 };
	uint32_t i = 0;
	int ret = 0;
	uint32_t handle_timeout_cnt = 0;

	DCAM_V4L2_PRINT
	    ("V4L2:vidioc_handle_ctrl, id: %d, value: %d,dcam mode=%d.\n",
	     ctrl->id, ctrl->value, g_dcam_info.mode);
	is_previewing = dcam_is_previewing(g_zoom_level);
	if (g_dcam_info.mode == 3) {
		DCAM_V4L2_ERR
		    ("v4l2:vidioc_handle_ctrl,don't adjust sensor when cap mode.\n");
	}
	if(DCAM_RESTAER_FAIL== s_dcam_err_info.work_status) {
		DCAM_V4L2_ERR("V4L2:vidioc_handle_ctrl restart end,don't need to set sensor.\n");
		return;
	}
	if(is_previewing) {
		while(DCAM_RESTART_PROCESS == s_dcam_err_info.work_status) {
			DCAM_V4L2_ERR("V4L2:vidioc_handle_ctrl,wait restart end.\n");
		}
	}
	switch (ctrl->id) {
	case V4L2_CID_BLACK_LEVEL:
		g_dcam_info.sensor_work_mode = (uint8_t)ctrl->value;
		/*if (g_dcam_info.sensor_work_mode == (uint8_t) ctrl->value) {
			DCAM_V4L2_PRINT
			    ("V4L2:vidioc_handle_ctrl,don't need to modify work mode.\n");
			break;
		}
		g_dcam_info.sensor_work_mode = (uint8_t) ctrl->value;
		if (is_previewing) {
			handle_timeout_cnt = 0;
			g_dcam_info.v4l2_buf_ctrl_set_next_flag = 1;
			while (g_dcam_info.v4l2_buf_ctrl_set_next_flag != 0) {
				if (handle_timeout_cnt > DCAM_HANDLE_TIMEOUT)
					break;
				handle_timeout_cnt++;
				msleep(1);
				DCAM_V4L2_PRINT
				    ("V4L2: video mode handle,handle_timeout_cnt=%d.\n",
				     handle_timeout_cnt);
			}
			dcam_stop_timer(&s_dcam_err_info.dcam_timer);
			dcam_stop();
			s_dcam_err_info.work_status = DCAM_WORK_STATUS_MAX;
			dcam_set_first_buf_addr(g_last_buf, g_last_uv_buf);
			Sensor_Ioctl(SENSOR_IOCTL_VIDEO_MODE,
				     g_dcam_info.sensor_work_mode);
			dcam_start_handle(1);
		}*/
		DCAM_V4L2_PRINT("V4L2:g_dcam_info.sensor_work_mode = %d.\n",
		       g_dcam_info.sensor_work_mode);
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		Sensor_SetSensorParamByKey(SENSOR_PARAM_WB, ctrl->value);
		if (g_dcam_info.wb_param == (uint8_t) ctrl->value) {
			DCAM_V4L2_PRINT("V4L2:don't need handle wb!.\n");
			break;
		}
		g_dcam_info.wb_param = (uint8_t) ctrl->value;
		dcam_stop_handle(is_previewing);
		Sensor_Ioctl(SENSOR_IOCTL_SET_WB_MODE, (uint32_t) ctrl->value);
		dcam_start_handle(is_previewing);
		break;
	case V4L2_CID_METERING:
		Sensor_SetSensorParamByKey(SENSOR_PARAM_METERING, ctrl->value);
		if (g_dcam_info.metering_param == (uint8_t) ctrl->value) {
			DCAM_V4L2_PRINT("V4L2:don't need handle metering!.\n");
			break;
		}
		g_dcam_info.metering_param = (uint8_t) ctrl->value;
		dcam_stop_handle(is_previewing);
		Sensor_Ioctl(SENSOR_IOCTL_SET_MERTERING_MODE, (uint32_t) ctrl->value);
		dcam_start_handle(is_previewing);
		break;
	case V4L2_CID_COLORFX:
		if (g_dcam_info.imageeffect_param == (uint8_t) ctrl->value) {
			DCAM_V4L2_PRINT
			    ("V4L2:don't need handle image effect!.\n");
			break;
		}
		g_dcam_info.imageeffect_param = (uint8_t) ctrl->value;
		dcam_stop_handle(is_previewing);
		Sensor_Ioctl(SENSOR_IOCTL_IMAGE_EFFECT, (uint32_t) ctrl->value);
		dcam_start_handle(is_previewing);
		break;
	case V4L2_CID_COLOR_KILLER:
		Sensor_SetSensorParamByKey(SENSOR_PARAM_SCENEMODE, ctrl->value);
		if (g_dcam_info.previewmode_param == (uint8_t) ctrl->value) {
			DCAM_V4L2_PRINT
			    ("V4L2:don't need handle preview mode!.\n");
			break;
		}
		g_dcam_info.previewmode_param = (uint8_t) ctrl->value;
		dcam_stop_handle(is_previewing);
		Sensor_Ioctl(SENSOR_IOCTL_PREVIEWMODE, (uint32_t) ctrl->value);
		dcam_start_handle(is_previewing);
		break;
	case V4L2_CID_BRIGHTNESS:
		if (g_dcam_info.brightness_param == (uint8_t) ctrl->value) {
			DCAM_V4L2_PRINT
			    ("V4L2:don't need handle brightness!.\n");
			break;
		}
		g_dcam_info.brightness_param = (uint8_t) ctrl->value;
		dcam_stop_handle(is_previewing);
		Sensor_Ioctl(SENSOR_IOCTL_BRIGHTNESS, (uint32_t) ctrl->value);
		dcam_start_handle(is_previewing);
		break;
	case V4L2_CID_DTP:
		Sensor_SetSensorParamByKey(SENSOR_PARAM_DTP, ctrl->value);
		dcam_stop_handle(is_previewing);
		Sensor_Ioctl(SENSOR_IOCTL_SET_DTP, (uint32_t) ctrl->value);
		dcam_start_handle(is_previewing);
		break;
	case V4L2_CID_GAIN:
		Sensor_SetSensorParamByKey(SENSOR_PARAM_ISO, ctrl->value);
		if(g_dcam_info.iso_param == (uint8_t)ctrl->value)
		{
			DCAM_V4L2_PRINT("V4L2:don't need handle iso!.\n");
			break;
		}
		g_dcam_info.iso_param = (uint8_t)ctrl->value;
		dcam_stop_handle(is_previewing);
		Sensor_Ioctl(SENSOR_IOCTL_ISO, (uint32_t)ctrl->value);
		dcam_start_handle(is_previewing);
		break;
	case V4L2_CID_CONTRAST:
		if (g_dcam_info.contrast_param == (uint8_t) ctrl->value) {
			DCAM_V4L2_PRINT("V4L2:don't need handle contrast!.\n");
			break;
		}
		g_dcam_info.contrast_param = (uint8_t) ctrl->value;
		dcam_stop_handle(is_previewing);
		Sensor_Ioctl(SENSOR_IOCTL_CONTRAST, (uint32_t) ctrl->value);
		dcam_start_handle(is_previewing);
		break;
	case V4L2_CID_ZOOM_ABSOLUTE:
		if (g_zoom_level == (uint32_t) ctrl->value) {
			DCAM_V4L2_PRINT("V4L2:don't need handle zoom!.\n");
			break;
		}
		g_zoom_level = (uint32_t) ctrl->value;
		if(dcam_is_previewing(g_zoom_level)) {
			dcam_stop_handle(1);
			dcam_start_handle(1);
		}
		DCAM_V4L2_PRINT("V4L2:g_zoom_level=%d.\n", g_zoom_level);
		break;
	case V4L2_CID_HFLIP:
		DCAM_V4L2_PRINT("V4L2:hflip setting.\n.");
		Sensor_Ioctl(SENSOR_IOCTL_HMIRROR_ENABLE,
			     (uint32_t) ctrl->value);
		break;
	case V4L2_CID_VFLIP:
		DCAM_V4L2_PRINT("V4L2:vflip setting.\n.");
		Sensor_Ioctl(SENSOR_IOCTL_VMIRROR_ENABLE,
			     (uint32_t) ctrl->value);
		break;
	case V4L2_CID_FOCUS_AUTO:
#ifdef V4L2_OPEN_FOCUS
		if (SENSOR_MAIN != Sensor_GetCurId()) {
			break;
		}
		if (FLASH_AUTO == g_dcam_info.flash_mode){
						DCAM_V4L2_PRINT("V4L2:vidioc_handle_ctrl FLASH_AUTO.\n.");
			Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_AUTO);
		}
		else{
		if (g_dcam_info.flash_mode) {
			DCAM_V4L2_PRINT("V4L2:vidioc_handle_ctrl FLASH_OPEN.\n.");
			Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_OPEN);	/*open flash*/
		}
		}
		copy_from_user(&focus_param[0], (uint16_t *) ctrl->value,
			       FOCUS_PARAM_LEN);
		DCAM_V4L2_PRINT("V4L2:focus kernel,type=%d,zone_cnt=%d.\n",
		       focus_param[0], focus_param[1]);
		if ((0 == g_dcam_info.focus_param) && (0 != focus_param[0])) {
			DCAM_V4L2_PRINT("V4L2: need initial auto firmware!.\n");
			af_param.cmd = SENSOR_EXT_FUNC_INIT;
			af_param.param = SENSOR_EXT_FOCUS_TRIG;
			if (SENSOR_SUCCESS !=
			    Sensor_Ioctl(SENSOR_IOCTL_FOCUS,
					 (uint32_t) & af_param)) {
				ret = -1;
				if (g_dcam_info.flash_mode) {
					Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE_AFTER_OPEN);	// close flash from open
				}
				if (FLASH_AUTO == g_dcam_info.flash_mode){
					Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE_AFTER_OPEN);	// close flash from open
				}
				DCAM_V4L2_ERR("v4l2:auto foucs init fail.\n");
				break;
			}
			g_dcam_info.focus_param = 1;
		}
		switch (focus_param[0]) {
		case 1:
			af_param.cmd = SENSOR_EXT_FOCUS_START;
			af_param.param = SENSOR_EXT_FOCUS_TRIG;
			break;
		case 2:
			af_param.cmd = SENSOR_EXT_FOCUS_START;
			af_param.param = SENSOR_EXT_FOCUS_ZONE;
			af_param.zone_cnt = 1;
			af_param.zone[0].x = focus_param[2];
			af_param.zone[0].y = focus_param[3];
			af_param.zone[0].w = focus_param[4];
			af_param.zone[0].h = focus_param[5];
			break;
		case 3:
			{
				uint16_t *param_ptr = &focus_param[2];
				af_param.cmd = SENSOR_EXT_FOCUS_START;
				af_param.param = SENSOR_EXT_FOCUS_MULTI_ZONE;
				af_param.zone_cnt = focus_param[1];
				for (i = 0; i < focus_param[1]; i++) {
					af_param.zone[i].x = *param_ptr++;
					af_param.zone[i].y = *param_ptr++;
					af_param.zone[i].w = *param_ptr++;
					af_param.zone[i].h = *param_ptr++;
				}
			}
			break;
		case 4:
			af_param.cmd = SENSOR_EXT_FOCUS_START;
			af_param.param = SENSOR_EXT_FOCUS_MACRO;
			break;
		default:
			DCAM_V4L2_ERR
			    ("V4L2:don't support this focus,focus type = %d .\n",
			     focus_param[0]);
			break;
		}

		if (SENSOR_SUCCESS !=
		    Sensor_Ioctl(SENSOR_IOCTL_FOCUS, (uint32_t) & af_param)) {
			DCAM_V4L2_ERR("V4L2:auto focus fail. \n");
			ret = -1;
		}
		if (g_dcam_info.flash_mode) {
			Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE_AFTER_OPEN);	// close flash from open
		}
		if (FLASH_AUTO == g_dcam_info.flash_mode){
			Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE_AFTER_OPEN);	// close flash from open
		}
#endif
		break;
	case V4L2_CID_EXPOSURE:
		DCAM_V4L2_PRINT("test vidioc_handle_ctrl:ev=%d .\n",
		       (uint32_t) ctrl->value);
		Sensor_SetSensorParamByKey(SENSOR_PARAM_EV, ctrl->value);
		if (g_dcam_info.ev_param == (uint8_t) ctrl->value) {
			DCAM_V4L2_PRINT("V4L2:don't need handle ev!.\n");
			break;
		}
		g_dcam_info.ev_param = (uint8_t) ctrl->value;
		dcam_stop_handle(is_previewing);
		Sensor_Ioctl(SENSOR_IOCTL_EXPOSURE_COMPENSATION,
			     (uint32_t) ctrl->value);
		dcam_start_handle(is_previewing);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		DCAM_V4L2_PRINT("test vidioc_handle_ctrl:antibanding=%d .\n",
		       (uint32_t) ctrl->value);
		if (g_dcam_info.power_freq == (uint8_t) ctrl->value) {
			DCAM_V4L2_PRINT
			    ("V4L2:don't need handle power freq!.\n");
			break;
		}
		g_dcam_info.power_freq = (uint8_t) ctrl->value;
		dcam_stop_handle(is_previewing);
		Sensor_Ioctl(SENSOR_IOCTL_ANTI_BANDING_FLICKER,
			     (uint32_t) ctrl->value);
		dcam_start_handle(is_previewing);
		break;
	case V4L2_CID_GAMMA:
		//g_dcam_info.flash_mode: 0 - close, 1 - on, 2 - torch, 0x10 - close after start, 0x11 - high light, 0x22 - recording start
		DCAM_V4L2_PRINT("test camera flash mode = 0x%x .\n",
		       (uint8_t) ctrl->value);
		if (g_dcam_info.flash_mode == (uint8_t) ctrl->value) {
			DCAM_V4L2_PRINT
			    ("V4L2:don't need handle flash: V4L2_CID_GAMMA !.\n");
			break;
		}
		if (FLASH_OPEN_ON_RECORDING == ctrl->value) {
			g_dcam_info.recording_start = 1;
#ifdef FLASH_DV_OPEN_ON_RECORD
			if (FLASH_TORCH == g_dcam_info.flash_mode)
				Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_TORCH);
#endif
		} else {
			g_dcam_info.flash_mode = (uint8_t) ctrl->value;
			if (FLASH_CLOSE == g_dcam_info.flash_mode) {
				Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE);	// disable flash
			}
#ifdef FLASH_DV_OPEN_ALWAYS
			else if (FLASH_TORCH == g_dcam_info.flash_mode) {
				Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_TORCH);
			}
#endif
		}
		break;
	default:
		break;
	}
	return ret;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct dcam_fh *fh = priv;
	struct dcam_dev *dev = fh->dev;
	int i;
	int ret = 0;
	for (i = 0; i < ARRAY_SIZE(dcam_qctrl); i++)
		if (ctrl->id == dcam_qctrl[i].id) {
			if (ctrl->id != V4L2_CID_FOCUS_AUTO) {
				if (ctrl->value < dcam_qctrl[i].minimum
				    || ctrl->value > dcam_qctrl[i].maximum) {
					return -ERANGE;
				}
			}
			dev->qctl_regs[i] = ctrl->value;
			ret = vidioc_handle_ctrl(ctrl);
			return ret;
		}
	return -EINVAL;
}

static int vidioc_g_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *streamparm)
{
	struct dcam_fh *fh = priv;
	struct dcam_dev *dev = fh->dev;
	int i;
	uint8_t *data_ptr = NULL;
	SENSOR_EXP_INFO_T *sensor_info_ptr = NULL;
	SENSOR_MODE_INFO_T *sensor_mode_info_ptr = NULL;
	DCAM_V4L2_PRINT("V4L2: vidioc_g_parm E.\n");
	streamparm->type = dev->streamparm.type;
	streamparm->parm.capture.capability =
	    dev->streamparm.parm.capture.capability;
	streamparm->parm.capture.capturemode =
	    dev->streamparm.parm.capture.capturemode;
	streamparm->parm.capture.timeperframe.numerator =
	    dev->streamparm.parm.capture.timeperframe.numerator;
	streamparm->parm.capture.timeperframe.denominator =
	    dev->streamparm.parm.capture.timeperframe.denominator;
	streamparm->parm.capture.extendedmode =
	    dev->streamparm.parm.capture.extendedmode;
	streamparm->parm.capture.readbuffers =
	    dev->streamparm.parm.capture.readbuffers;
	for (i = 0; i < 4; i++)
		streamparm->parm.capture.reserved[i] =
		    dev->streamparm.parm.capture.reserved[i];
	streamparm->parm.raw_data[0] = dev->streamparm.parm.raw_data[0];
	streamparm->parm.raw_data[1] = dev->streamparm.parm.raw_data[1];

	sensor_info_ptr = Sensor_GetInfo();
	if (PNULL == sensor_info_ptr) {
		DCAM_V4L2_ERR("v4l2:vidioc_g_parm,get sensor info fail.\n");
		return -1;
	}
	data_ptr = &streamparm->parm.raw_data[3];
	for (i = SENSOR_MODE_PREVIEW_ONE; i < SENSOR_MODE_MAX; i++) {
		sensor_mode_info_ptr =
		    &sensor_info_ptr->sensor_mode_info[i - 1];
		if ((0 != sensor_mode_info_ptr->width)
		    && (0 != sensor_mode_info_ptr->height)) {
			*data_ptr++ = i;
			*data_ptr++ = sensor_mode_info_ptr->width & 0xff;
			*data_ptr++ = (sensor_mode_info_ptr->width >> 8) & 0xff;
			*data_ptr++ = sensor_mode_info_ptr->height & 0xff;
			*data_ptr++ =
			    (sensor_mode_info_ptr->height >> 8) & 0xff;
			*data_ptr++ = sensor_mode_info_ptr->trim_start_x & 0xff;
			*data_ptr++ =
			    (sensor_mode_info_ptr->trim_start_x >> 8) & 0xff;
			*data_ptr++ = sensor_mode_info_ptr->trim_start_y & 0xff;
			*data_ptr++ =
			    (sensor_mode_info_ptr->trim_start_y >> 8) & 0xff;
			*data_ptr++ = sensor_mode_info_ptr->trim_width & 0xff;
			*data_ptr++ =
			    (sensor_mode_info_ptr->trim_width >> 8) & 0xff;
			*data_ptr++ = sensor_mode_info_ptr->trim_height & 0xff;
			*data_ptr++ =
			    (sensor_mode_info_ptr->trim_height >> 8) & 0xff;
			*data_ptr++ = sensor_mode_info_ptr->image_format;
		} else {
			if (0 != (i - 1)) {
				i = i - 1;
				break;
			}
		}
	}
	streamparm->parm.raw_data[2] = i;
	DCAM_V4L2_PRINT("V4L2: vidioc_g_parm X,sensor mode sum = %d.\n", i);
	return 0;
}
static int v4l2_sensor_set_param(uint8_t *buf)
{
	Sensor_SetSensorParam(buf);
	return 0;
}

static int v4l2_sensor_get_param(uint8_t *buf,uint8_t *is_saved)
{
	Sensor_GetSensorParam(buf,is_saved);
	return 0;
}
static int v4l2_sensor_init(uint32_t sensor_id)
{
	if (SENSOR_TRUE != Sensor_IsInit()) {
		if (SENSOR_SUCCESS != Sensor_Init(sensor_id)) {
			DCAM_V4L2_PRINT("DCAM: Fail to init sensor.\n");
			return -1;
		}
		if (0 == s_dcam_err_info.is_wakeup_thread) {
			wake_up_process(s_dcam_thread);
			s_dcam_err_info.is_stop = 0;
			s_dcam_err_info.is_wakeup_thread = 1;
			down_interruptible(&s_dcam_err_info.
					   dcam_thread_wakeup_sem);
			DCAM_V4L2_PRINT("V4L2:wake up dcam thread!.\n");
		}
	}
	DCAM_V4L2_PRINT("V4L2:sensor init OK.\n");
	return 0;
}

static int vidioc_s_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *streamparm)
{
	struct dcam_fh *fh = priv;
	struct dcam_dev *dev = fh->dev;
	int i;
	uint32_t sensor_id = 0;
	uint8_t *buf_ptr;
	uint8_t *is_saved_ptr;

	DCAM_V4L2_PRINT("V4L2: vidioc_s_parm E.\n");
	dev->streamparm.type = streamparm->type;
	dev->streamparm.parm.capture.capability =
	    streamparm->parm.capture.capability;
	dev->streamparm.parm.capture.capturemode =
	    streamparm->parm.capture.capturemode;
	dev->streamparm.parm.capture.timeperframe.numerator =
	    streamparm->parm.capture.timeperframe.numerator;
	dev->streamparm.parm.capture.timeperframe.denominator =
	    streamparm->parm.capture.timeperframe.denominator;
	dev->streamparm.parm.capture.extendedmode =
	    streamparm->parm.capture.extendedmode;
	dev->streamparm.parm.capture.readbuffers =
	    streamparm->parm.capture.readbuffers;
	for (i = 0; i < 4; i++)
		dev->streamparm.parm.capture.reserved[i] =
		    streamparm->parm.capture.reserved[i];

	if (1 == streamparm->parm.raw_data[199]) {
		if (0 == streamparm->parm.raw_data[198]) {
			sensor_id = 0;
		} else if (1 == streamparm->parm.raw_data[198]) {
			sensor_id = 1;
		} else if (5 == streamparm->parm.raw_data[198]) {
			sensor_id = 5;
		}
	} else {
		sensor_id = 0;
	}
	DCAM_V4L2_PRINT("v4l2:vidioc_s_parm,sensor_id=%d.\n", sensor_id);
	if (1 == streamparm->parm.raw_data[197]) {
		if (1 == streamparm->parm.raw_data[196]) {
			g_dcam_info.rot_angle = DCAM_ROTATION_90;
		} else if (2 == streamparm->parm.raw_data[196]) {
			g_dcam_info.rot_angle = DCAM_ROTATION_270;
		} else if (3 == streamparm->parm.raw_data[196]) {
			g_dcam_info.rot_angle = DCAM_ROTATION_180;
		} else {
			g_dcam_info.rot_angle = DCAM_ROTATION_0;
		}
	} else {
		g_dcam_info.rot_angle = DCAM_ROTATION_0;
	}
	buf_ptr = &streamparm->parm.raw_data[188];
	is_saved_ptr = &streamparm->parm.raw_data[196];
	v4l2_sensor_set_param(buf_ptr);
	if (0 != v4l2_sensor_init(sensor_id)) {
		DCAM_V4L2_PRINT("V4L2: fail to sensor_init.\n");
		*is_saved_ptr = 0;
		return -1;
	}
	v4l2_sensor_get_param(buf_ptr,is_saved_ptr);
	DCAM_V4L2_PRINT("V4L2: vidioc_s_parm X.\n");
	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct dcam_fh *fh = priv;
	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct dcam_fh *fh = priv;
	g_dcam_info.is_streamoff = 0;
	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct dcam_fh *fh = priv;
	if(1 == g_dcam_info.is_streamoff)
		return 0;
	if (1 == g_is_first_frame) {
		g_first_buf_addr = p->m.userptr;
		g_first_buf_uv_addr = p->reserved;
		g_is_first_frame = 0;
		DCAM_V4L2_PRINT("V4L2: g_first_buf_addr: %x.\n",
				g_first_buf_addr);
	}
	DCAM_V4L2_PRINT
	    ("V4L2: vidioc_qbuf: v4l2_buff : addr: 0x%08x,uaddr:0x%x.\n",
	     p->m.userptr, p->reserved);
	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct dcam_fh *fh = priv;
	int retun_val = 0;
	retun_val =
	    (videobuf_dqbuf(&fh->vb_vidq, p, file->f_flags & O_NONBLOCK));
	DCAM_V4L2_PRINT
	    ("V4L2: vidioc_dqbuf: v4l2_buff: addr: 0x%08x, file->f_flags: %x,  O_NONBLOCK: %x, g_dcam_info.mode: %d.\n",
	     p->m.userptr, file->f_flags, O_NONBLOCK, g_dcam_info.mode);
	return retun_val;
}

static int vidioc_g_output(struct file *file, void *priv, unsigned int *i)
{
	*i = g_dcam_info.jpg_len;
	return 0;
}

void vidioc_get_exif(JINF_EXIF_INFO_T * exif_ptr, uint32_t size)
{
	DC_InitExifParameter(exif_ptr, size);
	DC_GetExifParameter();
	DC_GetExifParameter_Post();
}

/* Use querymenu to get jpeg exif info */
static int vidioc_querymenu(struct file *file, void *priv,
			    struct v4l2_querymenu *qm)
{
	uint32_t p_memptr;
	uint32_t size;

	DCAM_V4L2_PRINT("V4l2:vidioc_querymenu start \n");
	size = qm->index;

	if(size >10*1024)
		size = 10*1024;

	DCAM_V4L2_PRINT("DCAM V4L2:vidioc_querymenu,addr=0x%x,size=0x%x, real_size=0x%x \n", qm->id,
		 qm->index, size);
	p_memptr = (unsigned int)ioremap(qm->id, size);
	if (0 == p_memptr) {
		DCAM_V4L2_ERR("V4L2: vidioc_querymenu error ####: Can't ioremap for PMEM_BASE_PHY_ADDR!\n");
		return -ENOMEM;
	}
	g_dc_exif_info_ptr = (JINF_EXIF_INFO_T *) p_memptr;
	DCAM_V4L2_PRINT("V4l2:vidioc_querymenu set: id=%x, index = %x, g_dc_exif_info_ptr=%x \n",
	     qm->id, qm->index, (uint32_t) g_dc_exif_info_ptr);

	vidioc_get_exif(g_dc_exif_info_ptr, size);
	iounmap((void __iomem *)p_memptr);
	return 0;
}

#define DCAM_PIXEL_ALIGNED 16
#define DCAM_W_H_ALIGNED(x) (((x) + DCAM_PIXEL_ALIGNED - 1) & ~(DCAM_PIXEL_ALIGNED - 1))

typedef struct dcam_trim_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
} DCAM_TRIM_RECT_T;

void zoom_picture_size(uint32_t in_w, uint32_t in_h,
		       DCAM_TRIM_RECT_T * trim_rect, uint32_t zoom_level)
{
	uint32_t trim_w, trim_h;

	switch (zoom_level) {
	case 0:
		trim_w = 0;
		trim_h = 0;
		break;
	case 1:
		trim_w = in_w >> 2;	/*1/4 */
		trim_h = in_h >> 2;	/*1/4 */
		break;
	case 2:
		trim_w = in_w / 3;	/* 1/3 */
		trim_h = in_h / 3;	/*1/3 */
		break;
	case 3:
		trim_w = in_w * 3 >> 3;	/*3/8 */
		trim_h = in_h * 3 >> 3;	/* 3/8 */
		break;
	default:
		trim_w = 0;
		trim_h = 0;
		break;
	}
	trim_rect->x = (trim_w + 3) & ~3;
	trim_rect->y = (trim_h + 3) & ~3;
	trim_rect->w = DCAM_W_H_ALIGNED(in_w - (trim_rect->x << 1));
	trim_rect->h = DCAM_W_H_ALIGNED(in_h - (trim_rect->y << 1));
	DCAM_V4L2_PRINT
	    ("V4l2:v4l2 trim_rect{x,y,w,h} --{%d, %d, %d, %d}, in_w: %d, in_h: %d, zoom_level: %d.\n",
	     trim_rect->x, trim_rect->y, trim_rect->w, trim_rect->h, in_w, in_h,
	     zoom_level);
}

static int init_dcam_parameters(void *priv)
{
	struct dcam_fh *fh = priv;
	struct dcam_dev *dev = fh->dev;
	DCAM_INIT_PARAM_T init_param;
	SENSOR_EXP_INFO_T *sensor_info_ptr = NULL;

	sensor_info_ptr = Sensor_GetInfo();
	if (PNULL == sensor_info_ptr) {
		DCAM_V4L2_ERR("v4l2:init_dcam_parameters,get sensor info fail.\n");
		return -1;
	}
	g_dcam_info.zoom_multiple = 2;
	if (1 == dev->streamparm.parm.capture.capturemode) {
		init_param.mode = 3;	//1
	} else {
		init_param.mode = 1;	//1
	}
	DCAM_V4L2_PRINT("v4l2: fh->fmt->fourcc: %d, init_param.mode: %d.\n",
			fh->fmt->fourcc, init_param.mode);
	DCAM_V4L2_PRINT
	    ("v4l2: fh->width=%d,fh->height=%d,g_dcam_info.out_format=%d .\n",
	     fh->width, fh->height, g_dcam_info.out_format);
	init_param.format = g_dcam_info.out_format;
	init_param.yuv_pattern = YUV_YUYV;
	init_param.display_rgb_type = RGB_565;
	init_param.input_size.w = fh->width;
	init_param.input_size.h = fh->height;
	init_param.polarity.hsync = 1;
	init_param.polarity.vsync = 0;
	init_param.polarity.pclk = 0;
	init_param.input_rect.x = 0;
	init_param.input_rect.y = 0;
	init_param.input_rect.w = init_param.input_size.w;
	init_param.input_rect.h = init_param.input_size.h;
	init_param.display_rect.x = 0;
	init_param.display_rect.y = 0;
	init_param.display_rect.w = init_param.input_size.w;
	init_param.display_rect.h = init_param.input_size.h;
	init_param.encoder_rect.x = 0;
	init_param.encoder_rect.y = 0;
	init_param.encoder_rect.w = init_param.input_size.w;
	init_param.encoder_rect.h = init_param.input_size.h;
	init_param.zoom_level = g_zoom_level;
	DCAM_V4L2_PRINT("v4l2: init param rect 0,%d,%d,%d,%d\n",
			init_param.input_rect.x, init_param.input_rect.y,
			init_param.input_rect.w, init_param.input_rect.h);

	DCAM_V4L2_PRINT("v4l2: init param rect 1,%d,%d,%d,%d\n",
			init_param.input_rect.x, init_param.input_rect.y,
			init_param.input_rect.w, init_param.input_rect.h);
	init_param.skip_frame = 0;
	init_param.rotation = 0;
	init_param.first_buf_addr = g_first_buf_addr;
	init_param.first_u_buf_addr = g_first_buf_uv_addr;
	DCAM_V4L2_PRINT("v4l2: init param rotation = %d,preview mode=%d .\n",
	       init_param.rotation, g_dcam_info.preview_m);
	if (1 == init_param.mode) {
		g_dcam_info.input_size.w =
		    sensor_info_ptr->sensor_mode_info[g_dcam_info.preview_m].
		    width;
		g_dcam_info.input_size.h =
		    sensor_info_ptr->sensor_mode_info[g_dcam_info.preview_m].
		    height;
	} else {
		g_dcam_info.input_size.w =
		    sensor_info_ptr->sensor_mode_info[g_dcam_info.snapshot_m].
		    width;
		g_dcam_info.input_size.h =
		    sensor_info_ptr->sensor_mode_info[g_dcam_info.snapshot_m].
		    height;
		g_zoom_level = 0;
	}
	init_param.input_rect.w = g_dcam_info.input_size.w;
	init_param.input_rect.h = g_dcam_info.input_size.h;
	init_param.input_size.w = init_param.input_rect.w;
	init_param.input_size.h = init_param.input_rect.h;
	DCAM_V4L2_PRINT("v4l2:init_dcam_parameters,input size %d,%d,rect:%d,%d .\n",
	       g_dcam_info.input_size.w, g_dcam_info.input_size.h,
	       init_param.input_rect.w, init_param.input_rect.h);
	g_dcam_info.mode = init_param.mode;
	init_param.zoom_multiple = g_dcam_info.zoom_multiple;
	init_param.zoom_level = g_zoom_level;
	dcam_parameter_init(&init_param);
	return 0;
}

extern ERR_SENSOR_E Sensor_SetTiming(SENSOR_MODE_E mode);
extern int Sensor_CheckTiming(SENSOR_MODE_E mode);

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct dcam_fh *fh = priv;
	int ret = 0;

	DCAM_V4L2_PRINT("#### V4L2: vidioc_streamon start.\n");
#ifdef FLASH_DV_OPEN_ON_RECORD
	if (g_dcam_info.flash_mode && g_dcam_info.recording_start) {
		Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_TORCH);	/*torch */
	}
#endif
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;
	s_dcam_err_info.work_status = DCAM_WORK_STATUS_MAX;
	s_dcam_err_info.priv = priv;
	s_dcam_err_info.is_running = 0;
	s_dcam_err_info.restart_cnt = 0;
	s_dcam_err_info.is_report_err = 0;
	s_dcam_err_info.ret = 0;
	s_dcam_err_info.timeout_val = DCAM_TIME_OUT;
	ret = init_sensor_parameters(priv);
	if (0 != ret)
		return -1;

	ret = init_dcam_parameters(priv);
	if (0 != ret)
		return -1;
	s_dcam_err_info.mode = g_dcam_info.mode;
	if (0 != (ret = videobuf_streamon(&fh->vb_vidq))) {
		DCAM_V4L2_ERR("V4L2: Fail to videobuf_streamon.\n");
		return ret;
	}
	g_is_first_irq = 1;
	g_last_buf = 0xFFFFFFFF;
	g_last_uv_buf = 0xFFFFFFFF;
	s_int_ctrl_flag = 0;

	if (5 == Sensor_GetCurId()) {
		DCAM_V4L2_ERR("v4l2:streamon,sensor is ATV .\n");
		s_dcam_err_info.timeout_val = DCAM_TIME_OUT_FOR_ATV;
	}
	ret = dcam_start();
	if (!ret) {
		dcam_start_timer(&s_dcam_err_info.dcam_timer,
				 s_dcam_err_info.timeout_val);
		down_interruptible(&s_dcam_err_info.dcam_start_sem);
		ret = s_dcam_err_info.ret;
	}
	DCAM_V4L2_PRINT("DCAM_V4L2: OK to vidioc_streamon,ret=%d.\n", ret);
	DCAM_V4L2_PRINT("#### V4L2: vidioc_streamon end .\n");
	return -ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct dcam_fh *fh = priv;
	int ret = 0;
	int k;
	uint32_t cnt = 0;
	DCAM_V4L2_PRINT("#### V4L2: vidioc_streamoff start.\n");
	g_dcam_info.recording_start = 0;

	if (g_dcam_info.flash_mode) {
#ifdef FLASH_DV_OPEN_ALWAYS
		if (FLASH_TORCH != g_dcam_info.flash_mode)
#endif
		{
			Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE_AFTER_OPEN);	// close flash from open
		}
	}
		if (FLASH_AUTO == g_dcam_info.flash_mode){
#ifdef FLASH_DV_OPEN_ALWAYS
		if (FLASH_TORCH != g_dcam_info.flash_mode)
#endif
		{
			Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE_AFTER_OPEN);	// close flash from open
		}
	}
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;
	if (0 != (ret = videobuf_streamoff(&fh->vb_vidq))) {
		DCAM_V4L2_ERR("V4L2: Fail to videobuf_streamoff,ret=%d.\n",
			      ret);
		return ret;
	}

	if(DCAM_RESTART_PROCESS == s_dcam_err_info.work_status){
		while (cnt < DCAM_RESTART_TIMEOUT) {
			if(0 == cnt%100)
				DCAM_V4L2_PRINT("V4L2: vidioc_streamoff, wait restart end, cnt=%d \n", cnt);

			if (DCAM_RESTART_PROCESS != s_dcam_err_info.work_status)
				break;

			cnt++;
			msleep(1);
		}
	}

	g_is_first_frame = 1;	/*store the nex first frame. */
	g_dcam_info.preview_m = 0;
	g_dcam_info.snapshot_m = 0;
	g_dcam_info.mode = DCAM_MODE_TYPE_IDLE;

	//g_dcam_info.wb_param = INVALID_VALUE;
	//g_dcam_info.metering_param = INVALID_VALUE;
	g_dcam_info.brightness_param = INVALID_VALUE;
	g_dcam_info.iso_param = INVALID_VALUE;
	g_dcam_info.contrast_param = INVALID_VALUE;
	g_dcam_info.saturation_param = INVALID_VALUE;
	//g_dcam_info.imageeffect_param = INVALID_VALUE;
	g_dcam_info.hflip_param = INVALID_VALUE;
	g_dcam_info.vflip_param = INVALID_VALUE;
	//g_dcam_info.previewmode_param = INVALID_VALUE;
	//g_dcam_info.ev_param = INVALID_VALUE;
	g_dcam_info.power_freq = INVALID_VALUE;
	/*g_dcam_info.sensor_work_mode = DCAM_PREVIEW_MODE;*/
	g_dcam_info.is_streamoff = 1;
	dcam_stop_timer(&s_dcam_err_info.dcam_timer);
	dcam_stop();
	s_dcam_err_info.work_status = DCAM_WORK_STATUS_MAX;
	s_int_ctrl_flag = 0;

	for (k = 0; k < VIDEO_MAX_FRAME; k++)
		if ((NULL != fh->vb_vidq.bufs[k])
		    && (VIDEOBUF_IDLE != fh->vb_vidq.bufs[k]->state)) {
			fh->vb_vidq.bufs[k]->state = VIDEOBUF_IDLE;
		}
	DCAM_V4L2_PRINT("#### V4L2: vidioc_streamoff end.\n");
	return ret;
}

static int vidioc_g_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	SENSOR_EXP_INFO_T *sensor_info_ptr = NULL;

	if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	sensor_info_ptr = Sensor_GetInfo();
	if (PNULL == sensor_info_ptr) {
		DCAM_V4L2_ERR("v4l2:vidioc_g_crop,get sensor info fail.\n");
		return -1;
	}
	crop->c.left = 0;
	crop->c.top = 0;
	crop->c.width = sensor_info_ptr->source_width_max;
	crop->c.height = sensor_info_ptr->source_height_max;
	DCAM_V4L2_PRINT("V4L2:G_CROP,maxwidth=%d,maxheight=%d.\n",
			crop->c.width, crop->c.height);
	return 0;
}

static void set_next_buffer(struct dcam_fh *fh)
{
	struct dcam_buffer *buf;
	struct dcam_dev *dev = fh->dev;
	struct dcam_dmaqueue *dma_q = &dev->vidq;
	unsigned long flags = 0;

	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
		DCAM_V4L2_ERR("V4L2: set_next_buffer:No active queue to serve\n");
		goto unlock;
	}
	if (NULL == dma_q->active.next) {
		DCAM_V4L2_ERR
		    ("V4L2: set_next_buffer: the dma_q->active.next is NULL.\n");
		goto unlock;
	}
	buf = list_entry(dma_q->active.next, struct dcam_buffer, vb.queue);
	if (0xFFFFFFFF == g_last_buf) {
		buf->fmt->flag = 1;
		g_last_buf = 0;
		g_last_uv_buf = 0;
		DCAM_V4L2_ERR("V4L2: set_next_buffer:clear g_last_buf.\n");
	}
	if ((1 == buf->fmt->flag) || (g_last_buf == buf->vb.baddr)) {
		if (NULL == dma_q->active.next->next) {
			DCAM_V4L2_ERR
			    ("V4L2: set_next_buffer: the dma_q->active.next->next is NULL.\n");
			goto unlock;
		}
		buf = list_entry(dma_q->active.next->next,
				 struct dcam_buffer, vb.queue);
	}

	/* Fill buffer */
	if (0 != buf->vb.baddr) {
		buf->fmt->flag = 1;
		g_last_buf = buf->vb.baddr;
		g_last_uv_buf = buf->vb.privsize;
		dcam_set_buffer_address(buf->vb.baddr, buf->vb.privsize);
		DCAM_V4L2_ERR
		    ("#### V4L2: v4l2_buff: set_next_buffer addr = 0x%08x \n",
		     buf->vb.baddr);
	} else {
		DCAM_V4L2_ERR
		    ("V4L2: fail: set_next_buffer filled buffer is 0.\n");
		goto unlock;
	}
/*DCAM_V4L2_PRINT("V4L2: set_next_buffer filled buffer yaddr:0x%x,uaddr:0x%x.\n", (uint32_t)buf->vb.baddr,buf->vb.privsize);*/
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
	return;
}

static void path1_done_buffer(struct dcam_fh *fh)
{
	struct dcam_buffer *buf;
	struct dcam_dev *dev = fh->dev;
	struct dcam_dmaqueue *dma_q = &dev->vidq;
	unsigned long flags = 0;

	if(1!=s_int_ctrl_flag)
	{
	     DCAM_V4L2_ERR("Warning: SOF is not received, path done return.\n");
	     return;
	}

	if (0 == s_dcam_err_info.is_running) {
		s_dcam_err_info.work_status = DCAM_START_OK;
	} else {
		s_dcam_err_info.work_status = DCAM_OK;
	}
	up(&s_dcam_err_info.dcam_thread_sem);
	spin_lock_irqsave(&dev->slock, flags);

	if (list_empty(&dma_q->active)) {
		DCAM_V4L2_ERR("V4L2: path1_done_buffer: No active queue to serve\n");
		goto unlock;
	}
	if (NULL == dma_q->active.next) {
		DCAM_V4L2_ERR("V4L2: path1_done_buffer: the active.next is NULL.\n");
		goto unlock;
	}
	buf = list_entry(dma_q->active.next, struct dcam_buffer, vb.queue);
	if (1 != g_is_first_irq) {
		if ((g_first_buf_addr != (uint32_t) buf->vb.baddr)
		    || (g_first_buf_addr == g_last_buf)) {
			DCAM_V4L2_ERR
			    ("V4L2: path1_done_buffer: Fail to this entry. last addr: %x, buf addr: %x\n",
			     g_first_buf_addr, (uint32_t) buf->vb.baddr);
			goto unlock;
		}
	} else {
		g_is_first_irq = 0;
	}
	list_del(&buf->vb.queue);
	/* Advice that buffer was filled */
	buf->vb.field_count++;
	do_gettimeofday(&buf->vb.ts);
	buf->vb.state = VIDEOBUF_DONE;
	pr_debug("time = %d.\n",
	       (int)(buf->vb.ts.tv_sec * 1000 + buf->vb.ts.tv_usec / 1000));
	wake_up(&buf->vb.done);
	g_first_buf_addr = g_last_buf;
	g_first_buf_uv_addr = g_last_uv_buf;
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
	return;
}

static void dcam_error_handle(struct dcam_fh *fh)
{
	struct dcam_buffer *buf;
	struct dcam_dev *dev = fh->dev;
	struct dcam_dmaqueue *dma_q = &dev->vidq;
	unsigned long flags = 0;

	DCAM_V4L2_PRINT("###V4L2: dcam_error_handle.\n");
	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
		DCAM_V4L2_ERR
		    ("###V4L2: dcam_error_handle: No active queue to serve\n");
		goto unlock;
	}
	if (NULL == dma_q->active.next) {
		DCAM_V4L2_ERR
		    ("###V4L2: dcam_error_handle: the active.next is NULL.\n");
		goto unlock;
	}
	buf = list_entry(dma_q->active.next, struct dcam_buffer, vb.queue);
	list_del(&buf->vb.queue);
	/* Advice that buffer was filled */
	buf->vb.field_count++;
	do_gettimeofday(&buf->vb.ts);
	buf->vb.state = VIDEOBUF_IDLE;
	wake_up(&buf->vb.done);
	g_first_buf_addr = g_last_buf;
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
	return;
}

void dcam_cb_ISRCapSOF(void)
{
	dcam_disableint();
	DCAM_V4L2_PRINT("dcam_cb_ISRCapSOF.\n");
	if (g_dcam_info.v4l2_buf_ctrl_set_next_flag == 1) {
		g_dcam_info.v4l2_buf_ctrl_set_next_flag = 0;
		dcam_enableint();
		DCAM_V4L2_ERR("dcam_cb_ISRCapSOF return.\n");
		return;
	}
        s_int_ctrl_flag = 1;
	set_next_buffer(g_fh);
	dcam_enableint();
}

void dcam_cb_ISRPath1Done(void)
{
	dcam_disableint();
	dcam_get_jpg_len(&g_dcam_info.jpg_len);
	path1_done_buffer(g_fh);
	dcam_enableint();
}

void dcam_cb_ISRPath2Done(void)
{
	if (DCAM_MODE_TYPE_PREVIEW == g_dcam_info.mode) {
		path1_done_buffer(g_fh);
	}
}

void dcam_cb_ISRCapFifoOF(void)
{
	dcam_error_close();
	s_dcam_err_info.work_status = DCAM_CAP_FIFO_OVERFLOW;
	up(&s_dcam_err_info.dcam_thread_sem);
}

void dcam_cb_ISRSensorLineErr(void)
{
	dcam_error_close();
	s_dcam_err_info.work_status = DCAM_LINE_ERR;
	up(&s_dcam_err_info.dcam_thread_sem);
}

void dcam_cb_ISRSensorFrameErr(void)
{
	dcam_error_close();
	s_dcam_err_info.work_status = DCAM_FRAME_ERR;
	up(&s_dcam_err_info.dcam_thread_sem);
}

void dcam_cb_ISRJpegBufOF(void)
{
	DCAM_V4L2_PRINT("V4L2:dcam_cb_ISRJpegBufOF.\n");
	dcam_error_close();
	s_dcam_err_info.work_status = DCAM_JPG_BUF_ERR;
	up(&s_dcam_err_info.dcam_thread_sem);
}

static int buffer_setup(struct videobuf_queue *vq, unsigned int *count,
			unsigned int *size)
{
	struct dcam_fh *fh = vq->priv_data;
	struct dcam_dev *dev = fh->dev;

	if (V4L2_PIX_FMT_RGB32 == fh->fmt->fourcc)
		*size = fh->width * fh->height * 4;
	else if (V4L2_PIX_FMT_RGB565X == fh->fmt->fourcc)
		*size = fh->width * fh->height * 2;
	else if (V4L2_PIX_FMT_JPEG == fh->fmt->fourcc)
		*size = fh->width * fh->height / 4;
	else
		*size = fh->width * fh->height * 3 / 2;

	if (0 == *count)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;

	dprintk(dev, 1, "%s, count=%d, size=%d\n", __func__, *count, *size);
	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct dcam_buffer *buf)
{
	struct dcam_fh *fh = vq->priv_data;
	struct dcam_dev *dev = fh->dev;
	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);

	if (in_interrupt())
		BUG();
	videobuf_vmalloc_free(&buf->vb);
	dprintk(dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

static int buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
			  enum v4l2_field field)
{
	struct dcam_fh *fh = vq->priv_data;
	struct dcam_buffer *buf = container_of(vb, struct dcam_buffer, vb);
	uint32_t hw_ratio = (fh->height>fh->width)? 1 : 0;

/*	DCAM_V4L2_PRINT("V4L2:buffer_prepare  w: %d, h: %d, baddr: %lx, bsize: %d.\n ",
		                              fh->width,fh->height, buf->vb.baddr, buf->vb.bsize);
	dprintk(dev, 1, "%s, field=%d\n", __func__, field);*/

	BUG_ON(NULL == fh->fmt);
	if (fh->width < 48 || fh->width > norm_maxw() ||
	    fh->height < 32 || fh->height > norm_maxh(hw_ratio))
		return -EINVAL;

	if (V4L2_PIX_FMT_RGB32 == fh->fmt->fourcc)
		buf->vb.size = fh->width * fh->height * 4;
	else if (V4L2_PIX_FMT_RGB565X == fh->fmt->fourcc)
		buf->vb.size = fh->width * fh->height * 2;
	else if (V4L2_PIX_FMT_JPEG == fh->fmt->fourcc)
		buf->vb.size = fh->width * fh->height / 4;
	else
		buf->vb.size = fh->width * fh->height * 3 / 2;

	if (0 != buf->vb.baddr && buf->vb.bsize < buf->vb.size)
		return -EINVAL;

	/* These properties only change when queue is idle, see s_fmt */
	buf->fmt = fh->fmt;
	buf->vb.width = fh->width;
	buf->vb.height = fh->height;
	buf->vb.field = field;
	precalculate_bars(fh);
	buf->vb.state = VIDEOBUF_PREPARED;
	return 0;
}

static void buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct dcam_buffer *buf = container_of(vb, struct dcam_buffer, vb);
	struct dcam_fh *fh = vq->priv_data;
	struct dcam_dev *dev = fh->dev;
	struct dcam_dmaqueue *vidq = &dev->vidq;

	buf->vb.state = VIDEOBUF_QUEUED;
	buf->fmt->flag = 0;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct dcam_buffer *buf = container_of(vb, struct dcam_buffer, vb);
	struct dcam_fh *fh = vq->priv_data;
	struct dcam_dev *dev = (struct dcam_dev *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);
	free_buffer(vq, buf);
}

static struct videobuf_queue_ops dcam_video_qops = {
	.buf_setup = buffer_setup,
	.buf_prepare = buffer_prepare,
	.buf_queue = buffer_queue,
	.buf_release = buffer_release,
};

static int dcam_scan_status_thread(void *data_ptr)
{
	DCAM_ERROR_INFO_T *info_ptr = (DCAM_ERROR_INFO_T *) data_ptr;

	if (!data_ptr) {
		DCAM_V4L2_ERR("v4l2:dcam_scan_status_thread,run error!.\n");
	}
	up(&info_ptr->dcam_thread_wakeup_sem);
	DCAM_V4L2_PRINT("v4l2:dcam_scan_status_thread,test 0!.\n");
	while (1) {
		pr_debug("v4l2:dcam_scan_status_thread,test!.\n");
		down_interruptible(&info_ptr->dcam_thread_sem);
		if (info_ptr->is_stop)
			goto dcam_thread_end;
		switch (info_ptr->work_status) {
		case DCAM_START_OK:
			info_ptr->restart_cnt = 0;
			info_ptr->is_report_err = 0;
			up(&info_ptr->dcam_start_sem);
			dcam_stop_timer(&info_ptr->dcam_timer);
			if (DCAM_MODE_TYPE_PREVIEW == info_ptr->mode) {
				dcam_start_timer(&info_ptr->dcam_timer,
						 info_ptr->timeout_val);
			}
			info_ptr->is_running = 1;
			pr_debug("v4l2:dcam_scan_status_thread,DCAM_START_OK.\n ");
			break;
		case DCAM_OK:
			info_ptr->work_status = DCAM_RUN;
			info_ptr->restart_cnt = 0;
			pr_debug("v4l2:dcam_scan_status_thread,DCAM_OK.\n ");
			break;
		case DCAM_JPG_BUF_ERR:
			dcam_stop_timer(&info_ptr->dcam_timer);
			info_ptr->ret = 1;
			up(&info_ptr->dcam_start_sem);
			DCAM_V4L2_PRINT("v4l2:dcam_scan_status_thread,DCAM_JPG_BUF_ERR,start fail!.\n");
			break;
		case DCAM_LINE_ERR:
		case DCAM_FRAME_ERR:
		case DCAM_CAP_FIFO_OVERFLOW:
		case DCAM_NO_RUN:
			info_ptr->work_status = DCAM_RESTART_PROCESS;
			dcam_stop_timer(&info_ptr->dcam_timer);
			dcam_stop();
			if (info_ptr->restart_cnt > DCAM_RESTART_COUNT) {
				if (1 == info_ptr->is_running) {
					info_ptr->is_report_err = 1;
					dcam_error_handle(g_fh);
					info_ptr->work_status = DCAM_RESTAER_FAIL;
					DCAM_V4L2_PRINT
					    ("v4l2:dcam_scan_status_thread,report error!.\n");
				} else {
					info_ptr->ret = 1;
					up(&info_ptr->dcam_start_sem);
					info_ptr->work_status = DCAM_WORK_STATUS_MAX;
					DCAM_V4L2_ERR
					    ("v4l2:dcam_scan_status_thread,start fail!.\n");
				}
				break;
			}
			if (DCAM_MODE_TYPE_PREVIEW == info_ptr->mode) {
				//Sensor_SetTiming(SENSOR_MODE_COMMON_INIT);
				Sensor_SetTiming(g_dcam_info.preview_m);
				reset_sensor_param();
			} else {
				Sensor_SetTiming(g_dcam_info.snapshot_m);
			}
			info_ptr->work_status = DCAM_RESTART;
			dcam_start_handle(1);
			info_ptr->restart_cnt++;
			break;
		case DCAM_WORK_STATUS_MAX:
			DCAM_V4L2_ERR
			    ("v4l2:dcam_scan_status_thread,work status error!.\n");
			break;
		default:
			break;
		}
	}
dcam_thread_end:
	info_ptr->is_stop = DCAM_THREAD_END_FLAG;
	DCAM_V4L2_PRINT("dcam thread end.\n");
	return 0;
}

/* ------------------------------------------------------------------
	File operations for the device
   ------------------------------------------------------------------*/
static int dcam_create_thread(void)
{
	int ret = 0;
	DCAM_V4L2_PRINT("v4l2:dcam_create_thread s!.\n");
	init_MUTEX(&s_dcam_err_info.dcam_thread_sem);
	down(&s_dcam_err_info.dcam_thread_sem);
	init_MUTEX_LOCKED(&s_dcam_err_info.dcam_thread_wakeup_sem);
	/* Start up the thread for scan dcam's status */
	s_dcam_thread =
	    kthread_create(dcam_scan_status_thread, (void *)&s_dcam_err_info,
			   "dcam-scan-status");
	if (IS_ERR(s_dcam_thread)) {
		s_dcam_err_info.is_wakeup_thread = 0xff;
		DCAM_V4L2_ERR("v4l2:dcam_create_thread error!.\n");
		ret = -1;
	}
	s_dcam_err_info.is_wakeup_thread = 0;
	DCAM_V4L2_PRINT("v4l2:dcam_create_thread e!.\n");
	return ret;
}

static void dcam_timer_callback(unsigned long data)
{
	DCAM_V4L2_PRINT("v4l2:dcam_timer_callback.\n");

	if ((s_dcam_err_info.work_status == DCAM_NO_RUN) ||
	    (s_dcam_err_info.work_status == DCAM_WORK_STATUS_MAX) ||
	    (s_dcam_err_info.work_status == DCAM_RESTART)) {
		DCAM_V4L2_PRINT("v4l2:dcam_timer_callback,dcam is DCAM_NO_RUN.\n");
		s_dcam_err_info.work_status = DCAM_NO_RUN;
		up(&s_dcam_err_info.dcam_thread_sem);
	} else {
		dcam_start_timer(&s_dcam_err_info.dcam_timer,
				 s_dcam_err_info.timeout_val);
	}
}

static int dcam_init_timer(struct timer_list *dcam_timer)
{
	pr_debug("v4l2:Timer module installing\n");
	setup_timer(dcam_timer, dcam_timer_callback, 0);
	pr_debug("v4l2:Timer module installing e\n");
	return 0;
}

static int dcam_start_timer(struct timer_list *dcam_timer, uint32_t time_val)
{
	int ret;
	DCAM_V4L2_PRINT("v4l2:dcam_start_timer,starting timer to fire in %ld \n",
	       jiffies);
	ret = mod_timer(dcam_timer, jiffies + msecs_to_jiffies(time_val));
	if (ret)
		DCAM_V4L2_ERR("v4l2:Error in mod_timer\n");
	return 0;
}

static void dcam_stop_timer(struct timer_list *dcam_timer)
{
	del_timer_sync(dcam_timer);
}

static int open(struct file *file)
{
	struct dcam_dev *dev = video_drvdata(file);
	struct dcam_fh *fh = NULL;
	int retval = 0;

	if (atomic_inc_return(&dev->users) > 1) {
		atomic_dec_return(&dev->users);
		return -EBUSY;
	}
	dprintk(dev, 1, "open /dev/video%d type=%s users=%d\n", dev->vfd->num,
		v4l2_type_names[V4L2_BUF_TYPE_VIDEO_CAPTURE], dev->users.counter);

	/* allocate + initialize per filehandle data */
	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (NULL == fh) {
		atomic_dec_return(&dev->users);
		retval = -ENOMEM;
	}
	if (retval)
		return retval;

	file->private_data = fh;
	fh->dev = dev;
	fh->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fh->fmt = &formats[0];
	fh->width = 640;
	fh->height = 480;
	/* Resets frame counters */
	dev->h = 0;
	dev->m = 0;
	dev->s = 0;
	dev->ms = 0;
	dev->mv_count = 0;
	dev->jiffies = jiffies;
	sprintf(dev->timestr, "%02d:%02d:%02d:%03d",
		dev->h, dev->m, dev->s, dev->ms);
	videobuf_queue_vmalloc_init(&fh->vb_vidq, &dcam_video_qops,
				    NULL, &dev->slock, fh->type,
				    V4L2_FIELD_INTERLACED,
				    sizeof(struct dcam_buffer), fh, &dev->lock);
	g_fh = fh;
	g_dcam_info.wb_param = DEFAULT_WB_VALUE;
	Sensor_SetSensorParamByKey(SENSOR_PARAM_WB, g_dcam_info.wb_param);
	g_dcam_info.brightness_param = INVALID_VALUE;
	g_dcam_info.iso_param = INVALID_VALUE;
	g_dcam_info.contrast_param = INVALID_VALUE;
	g_dcam_info.metering_param = DEFAULT_METERING_VALUE;
	Sensor_SetSensorParamByKey(SENSOR_PARAM_METERING, g_dcam_info.metering_param);
	g_dcam_info.saturation_param = INVALID_VALUE;
	g_dcam_info.imageeffect_param = DEFAULT_EFFECT_VALUE;
	g_dcam_info.hflip_param = INVALID_VALUE;
	g_dcam_info.vflip_param = INVALID_VALUE;
	g_dcam_info.previewmode_param = DEFAULT_SCENE_VALUE;
	Sensor_SetSensorParamByKey(SENSOR_PARAM_SCENEMODE, g_dcam_info.previewmode_param);
	g_dcam_info.ev_param = DEFAULT_EV_VALUE;
	Sensor_SetSensorParamByKey(SENSOR_PARAM_EV, g_dcam_info.ev_param);
	g_dcam_info.focus_param = 0;
	g_dcam_info.power_freq = INVALID_VALUE;
	g_dcam_info.flash_mode = FLASH_CLOSE;
	g_dcam_info.recording_start = 0;
	g_dcam_info.sensor_work_mode = DCAM_PREVIEW_MODE;
	s_auto_focus = DCAM_AF_IDLE;
	if (0 != dcam_open()) {
		return 1;
	}
	s_dcam_err_info.is_stop = DCAM_THREAD_END_FLAG;
	dcam_create_thread();
	dcam_init_timer(&s_dcam_err_info.dcam_timer);
	DCAM_V4L2_PRINT("###DCAM: OK to open dcam.\n");
	init_MUTEX(&s_dcam_err_info.dcam_start_sem);
	down(&s_dcam_err_info.dcam_start_sem);
	/*dcam_callback_fun_register(DCAM_CB_SENSOR_SOF ,dcam_cb_ISRSensorSOF); */
	dcam_callback_fun_register(DCAM_CB_CAP_SOF, dcam_cb_ISRCapSOF);
	/*dcam_callback_fun_register(DCAM_CB_CAP_EOF ,dcam_cb_ISRCapEOF); */
	dcam_callback_fun_register(DCAM_CB_PATH1_DONE, dcam_cb_ISRPath1Done);
	/*dcam_callback_fun_register(DCAM_CB_PATH2_DONE,dcam_cb_ISRPath2Done); */
	dcam_callback_fun_register(DCAM_CB_CAP_FIFO_OF, dcam_cb_ISRCapFifoOF);
	dcam_callback_fun_register(DCAM_CB_SENSOR_LINE_ERR,
				   dcam_cb_ISRSensorLineErr);
	dcam_callback_fun_register(DCAM_CB_SENSOR_FRAME_ERR,
				   dcam_cb_ISRSensorFrameErr);
	dcam_callback_fun_register(DCAM_CB_JPEG_BUF_OF, dcam_cb_ISRJpegBufOF);
	return 0;
}

#define DCAM_CLOSE_TIMEOUT     500
static int close(struct file *file)
{
	struct dcam_fh *fh = file->private_data;
	struct dcam_dev *dev = fh->dev;
	uint32_t cnt = 0;

	int minor = video_devdata(file)->minor;
	DCAM_V4L2_PRINT("#### V4L2: close start.\n");
	if (g_dcam_info.flash_mode) {
		Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE);	/*close flash */
		DCAM_V4L2_PRINT("V2L4:close the flash \n");
	}
		if (FLASH_AUTO == g_dcam_info.flash_mode){
		Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE);	/*close flash */
		DCAM_V4L2_PRINT("V2L4:close the flash \n");
	}
	dcam_stop_timer(&s_dcam_err_info.dcam_timer);
	DCAM_V4L2_PRINT("v4l2:close,stop timer.\n");
	s_dcam_err_info.is_stop = 1;
	up(&s_dcam_err_info.dcam_thread_sem);
	if (DCAM_THREAD_END_FLAG != s_dcam_err_info.is_stop) {
		while (cnt < DCAM_CLOSE_TIMEOUT) {
			cnt++;
			if (DCAM_THREAD_END_FLAG == s_dcam_err_info.is_stop)
				break;
			msleep(1);
		}
	}
	s_dcam_err_info.work_status = DCAM_WORK_STATUS_MAX;
	DCAM_V4L2_PRINT("v4l2:stop thread end.\n");
	dcam_close();
	DCAM_V4L2_PRINT("v4l2: OK to close dcam.\n");
	Sensor_Close();
	DCAM_V4L2_PRINT("V4L2: OK to close sensor.\n");
	dcam_clear_user_count();
	videobuf_stop(&fh->vb_vidq);
	videobuf_mmap_free(&fh->vb_vidq);
	s_dcam_err_info.is_wakeup_thread = 0;
	kfree(fh);
	atomic_dec_return(&dev->users);
	dprintk(dev, 1, "close called (minor=%d, users=%d)\n", minor,
		dev->users.counter);
	DCAM_V4L2_PRINT("V4L2: close end.\n");
	return 0;
}

uint32_t video_write (struct file *fd, uint8_t *buf, size_t len, loff_t * offset)
{
	SENSOR_EXT_FUN_PARAM_T af_param;
	uint16_t focus_param[FOCUS_PARAM_COUNT] = { 0 };
	int ret = 0;
	uint32_t i=0;

	if (SENSOR_MAIN != Sensor_GetCurId()) {
		return 0;
	}
		if (FLASH_AUTO == g_dcam_info.flash_mode){
									DCAM_V4L2_PRINT("V4L2: video_write FLASH_AUTO.\n.");
			Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_AUTO);
		}
		else{
				if (g_dcam_info.flash_mode) {
											DCAM_V4L2_PRINT("V4L2:video_write FLASH_OPEN.\n.");
					Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_OPEN);	/*open flash*/
				}
		}
	copy_from_user(&focus_param[0], (uint16_t *) buf,FOCUS_PARAM_LEN);
	DCAM_V4L2_PRINT("V4L2:focus kernel,type=%d,zone_cnt=%d.\n",
	       focus_param[0], focus_param[1]);
	s_auto_focus = DCAM_AF_GOING;
	if ((0 == g_dcam_info.focus_param) && (0 != focus_param[0])) {
		DCAM_V4L2_PRINT("V4L2: need initial auto firmware!.\n");
		af_param.cmd = SENSOR_EXT_FUNC_INIT;
		af_param.param = SENSOR_EXT_FOCUS_TRIG;
		if (SENSOR_SUCCESS != Sensor_Ioctl(SENSOR_IOCTL_FOCUS,(uint32_t) & af_param)) {
			if (g_dcam_info.flash_mode) {
				Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE_AFTER_OPEN);	// close flash from open
			}
			if (FLASH_AUTO == g_dcam_info.flash_mode){
				Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE_AFTER_OPEN);	// close flash from open
			}
			s_auto_focus = DCAM_AF_ERR;
			DCAM_V4L2_ERR("v4l2:auto foucs init fail.\n");
			goto VIDEO_WRITE_END;
		}
		g_dcam_info.focus_param = 1;
	}
	switch (focus_param[0]) {
	case 1:
		af_param.cmd = SENSOR_EXT_FOCUS_START;
		af_param.param = SENSOR_EXT_FOCUS_TRIG;
		break;
	case 2:
		af_param.cmd = SENSOR_EXT_FOCUS_START;
		af_param.param = SENSOR_EXT_FOCUS_ZONE;
		af_param.zone_cnt = 1;
		af_param.zone[0].x = focus_param[2];
		af_param.zone[0].y = focus_param[3];
		af_param.zone[0].w = focus_param[4];
		af_param.zone[0].h = focus_param[5];
		break;
	case 3:
		{
			uint16_t *param_ptr = &focus_param[2];
			af_param.cmd = SENSOR_EXT_FOCUS_START;
			af_param.param = SENSOR_EXT_FOCUS_MULTI_ZONE;
			af_param.zone_cnt = focus_param[1];
			for (i = 0; i < focus_param[1]; i++) {
				af_param.zone[i].x = *param_ptr++;
				af_param.zone[i].y = *param_ptr++;
				af_param.zone[i].w = *param_ptr++;
				af_param.zone[i].h = *param_ptr++;
			}
		}
		break;
	case 4:
		af_param.cmd = SENSOR_EXT_FOCUS_START;
		af_param.param = SENSOR_EXT_FOCUS_MACRO;
		break;
	default:
		DCAM_V4L2_ERR
		    ("V4L2:don't support this focus,focus type = %d .\n",
		     focus_param[0]);
		s_auto_focus = DCAM_AF_ERR;
		goto VIDEO_WRITE_END;
	}

	if (SENSOR_SUCCESS != Sensor_Ioctl(SENSOR_IOCTL_FOCUS, (uint32_t) & af_param)) {
		DCAM_V4L2_ERR("V4L2:auto focus fail. \n");
		s_auto_focus = DCAM_AF_ERR;
	}
	if (g_dcam_info.flash_mode) {
		Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE_AFTER_OPEN);	// close flash from open
	}
	if (FLASH_AUTO == g_dcam_info.flash_mode){
		Sensor_Ioctl(SENSOR_IOCTL_FLASH, FLASH_CLOSE_AFTER_OPEN);	// close flash from open
	}
VIDEO_WRITE_END:
	if(DCAM_AF_ERR != s_auto_focus)
		s_auto_focus = DCAM_AF_OK;
	if(DCAM_AF_OK == s_auto_focus) {
		ret = 1;
	} else {
		ret = 0;
	}
	s_auto_focus = DCAM_AF_IDLE;
	DCAM_V4L2_PRINT("wjp device_write ret=%d.\n",ret);
	return ret;
}
#if 0
int video_read (struct file * fd, uint8_t *buf, size_t len, loff_t *offset)
{
	int ret = -1;
	if(DCAM_AF_GOING == s_auto_focus) {
		ret = 1;
	} else if (DCAM_AF_ERR == s_auto_focus) {
		ret = 2;
	} else if(DCAM_AF_OK == s_auto_focus) {
		ret = 3;
		s_auto_focus = DCAM_AF_IDLE;
	}

	DCAM_V4L2_PRINT("wjp :device_read:ret = %d.\n",ret);
}
#endif
/**************************************************************************/

static const struct v4l2_file_operations dcam_fops = {
	.owner = THIS_MODULE,
	.open = open,
	.write = video_write,
	.release = close,
	.ioctl = video_ioctl2,	/* V4L2 ioctl handler */
};

static const struct v4l2_ioctl_ops dcam_ioctl_ops = {
	.vidioc_g_parm = vidioc_g_parm,
	.vidioc_s_parm = vidioc_s_parm,
	.vidioc_querycap = vidioc_querycap,
	.vidioc_cropcap = vidioc_cropcap,
	.vidioc_s_crop = vidioc_s_crop,
	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_s_std = vidioc_s_std,
	.vidioc_enum_input = vidioc_enum_input,
	.vidioc_g_input = vidioc_g_input,
	.vidioc_s_input = vidioc_s_input,
	.vidioc_queryctrl = vidioc_queryctrl,
	.vidioc_g_ctrl = vidioc_g_ctrl,
	.vidioc_s_ctrl = vidioc_s_ctrl,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_g_crop = vidioc_g_crop,
	.vidioc_g_output = vidioc_g_output,
	.vidioc_querymenu = vidioc_querymenu,
};

static struct video_device dcam_template = {
	.name = "dcam",
	.fops = &dcam_fops,
	.ioctl_ops = &dcam_ioctl_ops,
	.minor = -1,
	.release = video_device_release,
	.tvnorms = V4L2_STD_525_60,
	.current_norm = V4L2_STD_NTSC_M,
};

static int release(void)
{
	struct dcam_dev *dev;
	struct list_head *list;

	while (!list_empty(&dcam_devlist)) {
		list = dcam_devlist.next;
		list_del(list);
		dev = list_entry(list, struct dcam_dev, dcam_devlist);
		v4l2_info(&dev->v4l2_dev, "unregistering /dev/video%d\n",
			  dev->vfd->num);
		video_unregister_device(dev->vfd);
		v4l2_device_unregister(&dev->v4l2_dev);
		kfree(dev);
	}
	return 0;
}

static int __init create_instance(int inst)
{
	struct dcam_dev *dev;
	struct video_device *vfd;
	int ret, i;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name),
		 "%s-%03d", DCAM_MODULE_NAME, inst);
	ret = v4l2_device_register(NULL, &dev->v4l2_dev);
	if (ret)
		goto free_dev;

	/* init video dma queues */
	INIT_LIST_HEAD(&dev->vidq.active);
	init_waitqueue_head(&dev->vidq.wq);

	/* initialize locks */
	spin_lock_init(&dev->slock);
	mutex_init(&dev->lock);

	ret = -ENOMEM;
	vfd = video_device_alloc();
	if (!vfd)
		goto unreg_dev;
	*vfd = dcam_template;
	vfd->debug = debug;
//	vfd->lock = &dev->lock;
	ret = video_register_device(vfd, VFL_TYPE_GRABBER, video_nr);
	if (ret < 0)
		goto rel_vdev;
	video_set_drvdata(vfd, dev);
	/* Set all controls to their default value. */
	for (i = 0; i < ARRAY_SIZE(dcam_qctrl); i++)
		dev->qctl_regs[i] = dcam_qctrl[i].default_value;

	/* Now that everything is fine, let's add it to device list */
	list_add_tail(&dev->dcam_devlist, &dcam_devlist);
	snprintf(vfd->name, sizeof(vfd->name), "%s (%i)",
		 dcam_template.name, vfd->num);

	if (video_nr >= 0)
		video_nr++;

	dev->vfd = vfd;
	v4l2_info(&dev->v4l2_dev, "V4L2 device registered as /dev/video%d\n",
		  vfd->num);
	return 0;
rel_vdev:
	video_device_release(vfd);
unreg_dev:
	v4l2_device_unregister(&dev->v4l2_dev);
free_dev:
	kfree(dev);
	return ret;
}

int dcam_probe(struct platform_device *pdev)
{
	int ret;
	DCAM_V4L2_PRINT(KERN_ALERT "dcam_probe called\n");
	ret = create_instance(pdev->id);
	if (ret < 0) {
		DCAM_V4L2_PRINT(KERN_INFO "Error %d while loading dcam driver\n", ret);
		return ret;
	}
	DCAM_V4L2_PRINT(KERN_ALERT "dcam_probe Success.\n");
	return 0;
}

static int dcam_remove(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver dcam_driver = {
	.probe = dcam_probe,
	.remove = dcam_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "sprd_dcam",
		   },
};

int __init dcam_v4l2_init(void)
{
	int ret = 0, i;

	if (platform_driver_register(&dcam_driver) != 0) {
		DCAM_V4L2_ERR("platform device register Failed \n");
		return -1;
	}
	DCAM_V4L2_PRINT(KERN_INFO "Video Technology Magazine Virtual Video "
	       "Capture Board ver %u.%u.%u successfully loaded.\n",
	       (DCAM_VERSION >> 16) & 0xFF, (DCAM_VERSION >> 8) & 0xFF,
	       DCAM_VERSION & 0xFF);
	return ret;
}

void dcam_v4l2_exit(void)
{
	platform_driver_unregister(&dcam_driver);
	release();
}

module_init(dcam_v4l2_init);
module_exit(dcam_v4l2_exit);

MODULE_DESCRIPTION("Dcam Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jianping.wang<jianping.wang@spreadtrum.com>");

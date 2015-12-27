/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Audio (vAudio).                                   *
 *             vAudio kernel/user interface driver implementation.           *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  This program is free software: you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License Version 2           *
 *  as published by the Free Software Foundation.                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  Version 2 along with this program.                                       *
 *  If not, see <http://www.gnu.org/licenses/>.                              *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Pascal Piovesan <pascal.piovesan@redbend.com>                          *
 *                                                                           *
 *****************************************************************************/

#include <nk/nkern.h>

#define VAUDIO_USER_VERSION 1
#ifndef CONFIG_ARCH_GOLDFISH
#define VAUDIO_USER_TIMER
#endif
//#define VAUDIO_USER_DEBUG

#define VAUDIO_MAJOR   117
#define VAUDIO_NAME    "vaudio"

#define VAUDIO_MSG     "VAUDIO: "
#define VAUDIO_ERR     KERN_ERR    VAUDIO_MSG
#define VAUDIO_INFO    KERN_NOTICE VAUDIO_MSG

#define WAIT_QUEUE     wait_queue_head_t

#define RAUDIO_OPEN       'o'
#define RAUDIO_CLOSE      'c'
#define RAUDIO_SET_SAMPLE 's'
#define RAUDIO_START      'u'
#define RAUDIO_STOP       'd'

#define RAUDIO_OK         '0'
#define RAUDIO_ERROR      '1'

#define VAUDIO_IOC_GET_COMMAND  _IOR('a', 1, vaudio_command_t)
#define VAUDIO_IOC_SET_RESULT   _IOW('a', 2, vaudio_result_t)
#define VAUDIO_IOC_SET_CALLBACK  _IO('a', 3)
#define VAUDIO_IOC_GET_VERSION  _IOR('a', 4, int)

typedef struct vaudio_stream_params_t {
    unsigned int  channels;
    unsigned int  rate;
    unsigned int  period;
    unsigned int  periods;
    unsigned int  buff_addr;
    unsigned char format_size;
    unsigned char format_signed;
    unsigned char format_le;
} vaudio_stream_params_t;

typedef struct vaudio_start_params_t {
    char* buf;
    unsigned int size;
} vaudio_start_params_t;

typedef struct vaudio_command_t {
    char            name;
    union {
        vaudio_stream_params_t config;
        vaudio_start_params_t  start;
    } params;
} vaudio_command_t;

typedef struct vaudio_result_t {
    char            name;
    char            error;
} vaudio_result_t;

typedef struct VaudioDev {
    int              version;
    int              active;
    int              ack_pending;
    int              stream;

    void            *be_cb_cookie;
    unsigned int     buff_addr;    

    WAIT_QUEUE	     wait_com;
    int              cok;
    vaudio_command_t command;
    WAIT_QUEUE	     wait_res;
    int              rok;
    vaudio_result_t  result;

#ifdef VAUDIO_USER_TIMER
    struct hrtimer   hr_timer;
    unsigned long    hr_time_sec;
    unsigned long    hr_time_nsec;
    unsigned long    timer_started;
    unsigned long    timer_cancel;
#endif
} VaudioDev;

static int           backend_connected;
static AudioCallback be_callback;

static  VaudioDev vaudio_dev[DEV_MAX * STREAM_MAX];

static struct class* vaudio_class;
static int           vaudio_dev_num;

    static int
sync_command (VaudioDev *vdev, char command)
{
    vdev->command.name = command;
    vdev->cok = 1;
    wake_up_interruptible(&vdev->wait_com);
    if (wait_event_interruptible(vdev->wait_res, vdev->rok)) {
        return 1;
    }
    vdev->rok = 0;
    if (vdev->result.error != RAUDIO_OK) {
        return 1;
    }
    return 0;
}

    static void
async_command (VaudioDev *vdev, char command)
{
    vdev->command.name = command;
    vdev->cok = 1;
    wake_up_interruptible(&vdev->wait_com);
}

    static int
vaudio_open (struct inode* inode, struct file* file)
{
    unsigned int minor = iminor(inode);
    VaudioDev   *vdev;

    vdev = &vaudio_dev[minor];
    vdev->active = 1;
#ifdef VAUDIO_USER_TIMER
    if (vdev->timer_started) {
        vdev->timer_cancel  = 1;
    }
#endif
    return 0;
}

    static int
vaudio_release (struct inode* inode, struct file* file)
{
    unsigned int minor = iminor(inode);
    VaudioDev   *vdev;

    vdev = &vaudio_dev[minor];
    vdev->active = 0;
    if (vdev->ack_pending) {
        vdev->ack_pending = 0;
        be_callback(AUDIO_STATUS_OK, vdev->be_cb_cookie);
    }

    return 0;
}

    static long
vaudio_ioctl (struct file* file, unsigned int cmd, unsigned long arg)
{
    struct inode *inode = file->f_path.dentry->d_inode;
    unsigned int minor = iminor(inode);
    VaudioDev   *vdev;
    int          err = 0;

    vdev = &vaudio_dev[minor];

    switch (cmd) {

    case VAUDIO_IOC_GET_VERSION:
	if (copy_to_user((void*)arg, &vdev->version, sizeof(int))) {
	    return -EFAULT;
	}
	break;

    case VAUDIO_IOC_GET_COMMAND:
        if (wait_event_interruptible(vdev->wait_com, vdev->cok)) {
            return -EINTR;
        }
        vdev->cok = 0;
	if (copy_to_user((void*)arg, &vdev->command, sizeof(vaudio_command_t))) {
	    return -EFAULT;
	}
	break;

    case VAUDIO_IOC_SET_RESULT:
        if (copy_from_user(&vdev->result, (void*)arg, sizeof(vaudio_result_t))) {
	    return -EFAULT;
        }
        if (vdev->result.name != RAUDIO_START &&
            vdev->result.name != RAUDIO_STOP) {
            vdev->rok = 1;
            wake_up_interruptible(&vdev->wait_res);
	}
	break;

    case VAUDIO_IOC_SET_CALLBACK:
        vdev->ack_pending = 0;
        if (vdev->stream == SNDRV_PCM_STREAM_PLAYBACK) {
#ifdef VAUDIO_USER_TIMER
            if (vdev->timer_cancel) {
#endif
                be_callback(AUDIO_STATUS_OK, vdev->be_cb_cookie);
#ifdef VAUDIO_USER_TIMER
            }
#endif
        } else {
            be_callback(AUDIO_STATUS_OK, vdev->be_cb_cookie);
        }
	break;

    default:
	err = -EINVAL;
    }

    return err;
}

    static int
vaudio_mmap (struct file* file, struct vm_area_struct* vma)
{
    unsigned int minor = iminor(file->f_path.dentry->d_inode);
    VaudioDev   *vdev;

    vdev = &vaudio_dev[minor];

    return remap_pfn_range(vma, vma->vm_start,
		   vdev->buff_addr >> PAGE_SHIFT,
		   vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

    static ssize_t
vaudio_read (struct file* file, char __user* buf,
             size_t count, loff_t* ppos)
{
    return -ENOSYS;
}

    static ssize_t
vaudio_write (struct file* file, const char __user* buf,
              size_t count, loff_t* ppos)
{
    return -ENOSYS;
}

    static unsigned int
vaudio_poll(struct file* file, poll_table* wait)
{
    return -ENOSYS;
}

static const struct file_operations vaudio_fops = {
    .owner   = THIS_MODULE,
    .open    = vaudio_open,
    .read    = vaudio_read,
    .write   = vaudio_write,
    .release = vaudio_release,
    .poll    = vaudio_poll,
    .unlocked_ioctl   = vaudio_ioctl,
    .mmap    = vaudio_mmap,
    .llseek  = no_llseek,
};


    static void*
raudio_open (int dev_id, int stream_id, void* be_cb_cookie)
{
    VaudioDev *vdev;
    unsigned int minor;

    minor = (dev_id * STREAM_MAX) + stream_id;
    if (minor >= DEV_MAX * STREAM_MAX) {
        printk(VAUDIO_ERR "raudio_open failed : bad minor %d\n", minor);
        return 0;
    }
    vdev = &vaudio_dev[minor];
    if (vdev->active) {
        sync_command(vdev, RAUDIO_OPEN);
    }
    vdev->stream = stream_id;
    vdev->be_cb_cookie = be_cb_cookie;
#ifdef VAUDIO_USER_DEBUG
    printk(VAUDIO_MSG "raudio_open %x %d %d\n",
	   (unsigned int)vdev, dev_id, stream_id);
#endif

    return vdev;
}

    static void
raudio_close (void* cookie)
{
    VaudioDev *vdev;

    vdev = (VaudioDev*)cookie;
#ifdef VAUDIO_USER_DEBUG
    printk(VAUDIO_MSG "raudio_close %x\n", (unsigned int)vdev);
#endif
    if (vdev->active) {
        sync_command(vdev, RAUDIO_CLOSE);
    }
    vdev->be_cb_cookie = 0;
}

    static int
raudio_set_sample (void* cookie, unsigned int channels,
                   AudioFormat  format, unsigned int rate,
                   unsigned int period, unsigned int periods,
                   unsigned int buff_addr)
{
    VaudioDev   *vdev;
#ifdef VAUDIO_USER_TIMER
    unsigned int samples;
#endif

    vdev = (VaudioDev*)cookie;
#ifdef VAUDIO_USER_DEBUG
    printk(VAUDIO_MSG "raudio_set_sample1 %x channels %d format %d rate %d\n",
	   (unsigned int)vdev, channels, format_size[format], rate);
    printk(VAUDIO_MSG "raudio_set_sample2 %x period size %d periods %d buffer addr %08x\n",
	   (unsigned int)vdev, period, periods, buff_addr);
#endif
#ifdef VAUDIO_USER_TIMER
    samples = period / (channels * format_size[format]);
    vdev->hr_time_sec  = 0;
    vdev->hr_time_nsec = ((samples * 1000000) / rate) * 1000 ;
    vdev->timer_started = 0;
    vdev->timer_cancel  = 0;
#endif
    vdev->command.params.config.channels      = channels;
    vdev->command.params.config.rate          = rate;
    vdev->command.params.config.period        = period;
    vdev->command.params.config.periods       = periods;
    vdev->command.params.config.buff_addr     = buff_addr;
    vdev->command.params.config.format_size   = format_size[format];
    vdev->command.params.config.format_signed = format_signed[format];
    vdev->command.params.config.format_le     = format_le[format];
    vdev->buff_addr     = buff_addr;
    
    if (vdev->active) {
        sync_command(vdev, RAUDIO_SET_SAMPLE);
    }

    return 0;
}

   static int
raudio_start (void* cookie, char* buf, unsigned int size)
{
    VaudioDev *vdev;

    vdev = (VaudioDev*)cookie;
    if (vdev == 0) {
        printk(VAUDIO_ERR "raudio_start failed %x\n", (unsigned int)vdev);
        return 1;
    }
#ifdef VAUDIO_USER_DEBUG_VERBOSE
    printk(VAUDIO_MSG "raudio_start %x %x %x %d\n",
	   (unsigned int)vdev, vdev->active, (unsigned int)buf, size);
#endif
    if (vdev->active) {
#ifdef VAUDIO_USER_TIMER
        if (vdev->ack_pending && vdev->timer_started) {
	  //printk(VAUDIO_ERR "start without previous ack\n");
            return 0;
        }
#endif
        vdev->ack_pending = 1;
        vdev->command.params.start.buf  = buf;
        vdev->command.params.start.size = size;
        async_command(vdev, RAUDIO_START);
    } else {
        unsigned char* vbuf = nkops.nk_ptov((NkPhAddr)buf);
        memset(vbuf, 0, size); // zero the buffer in case of record
    }
#ifdef VAUDIO_USER_TIMER
    if (vdev->stream == SNDRV_PCM_STREAM_PLAYBACK || !vdev->active) {
        if (!vdev->timer_cancel) {
            if (!vdev->timer_started) {
	      //printk(VAUDIO_MSG "raudio_start timer %x\n", (unsigned int)vdev);
                vdev->timer_started = 1;
                hrtimer_start(&vdev->hr_timer,
                      ktime_set(vdev->hr_time_sec, vdev->hr_time_nsec),
                      HRTIMER_MODE_REL);
            }
        }
    }
#endif

    return 0;
}

   static void
raudio_stop (void* cookie)
{
    VaudioDev *vdev;

    vdev = (VaudioDev*)cookie;
    if (vdev == 0) {
        printk(VAUDIO_ERR "raudio_stop failed %x\n", (unsigned int)vdev);
        return;
    }
#ifdef VAUDIO_USER_DEBUG
    printk(VAUDIO_MSG "raudio_stop %x\n", (unsigned int)vdev);
#endif
#ifdef VAUDIO_USER_TIMER
    if (vdev->stream == SNDRV_PCM_STREAM_PLAYBACK) {
        if (vdev->timer_started) {
            vdev->ack_pending = 0;
            vdev->timer_started = 0;
            hrtimer_cancel(&vdev->hr_timer);
        }
    }
#endif
    if (vdev->active) {
        async_command(vdev, RAUDIO_STOP);
    }
}

#ifdef VAUDIO_USER_TIMER
    static enum hrtimer_restart
timer_handler (struct hrtimer *hr_timer)
{
    VaudioDev *vdev = container_of(hr_timer, struct VaudioDev, hr_timer);

    hrtimer_forward_now(&vdev->hr_timer,
                    ktime_set(vdev->hr_time_sec, vdev->hr_time_nsec));
    be_callback(AUDIO_STATUS_OK, vdev->be_cb_cookie);

    if (vdev->timer_cancel || !vdev->timer_started) {
        return HRTIMER_NORESTART;
    }
    return HRTIMER_RESTART;
}
#endif

#define NK_VAUDIO_MIXER_MAX 128

static struct snd_kcontrol* ctrl_controls[NK_VAUDIO_MIXER_MAX];
static snd_kcontrol_get_t*  ctrl_gets    [NK_VAUDIO_MIXER_MAX];
static snd_kcontrol_put_t*  ctrl_puts    [NK_VAUDIO_MIXER_MAX];
static snd_kcontrol_put_t*  ctrl_puts    [NK_VAUDIO_MIXER_MAX];
static unsigned char*       ctrl_names   [NK_VAUDIO_MIXER_MAX];
static int                  ctrl_idx;

    static int
raudio_mixer_info(int idx, struct snd_ctl_elem_info* info)
{
    struct snd_kcontrol* ctrl;

#ifdef VAUDIO_USER_DEBUG
    printk(VAUDIO_MSG "raudio_mixer_info %d\n", idx);
#endif
    if (idx >= NK_VAUDIO_MIXER_MAX) {
        return 1;
    }
    ctrl = ctrl_controls[idx];
    if (!ctrl) {
        return 1;
    }
    if (ctrl->tlv.p) {
	memcpy(info->reserved, ctrl->tlv.p, sizeof(info->reserved));
    } else {
	memset(info->reserved, 0, sizeof(info->reserved));
    }
    memcpy(info->id.name, ctrl_names[idx], sizeof(info->id.name));

    return ctrl->info(ctrl, info);
}

    static int
raudio_mixer_get(int idx, struct snd_ctl_elem_value* val)
{
    struct snd_kcontrol* ctrl;
    snd_kcontrol_get_t*  get;

#ifdef VAUDIO_USER_DEBUG
    printk(VAUDIO_MSG "raudio_mixer_get %d\n", idx);
#endif
    if (idx >= NK_VAUDIO_MIXER_MAX) {
	return 1;
    }

    ctrl = ctrl_controls[idx];
    get  = ctrl_gets[idx];
    if (!ctrl || !get) {
	return 1;
    }

    return get(ctrl, val);
}

    static int
raudio_mixer_put(int idx, struct snd_ctl_elem_value* val)
{
    struct snd_kcontrol* ctrl;
    snd_kcontrol_put_t*  put;

#ifdef VAUDIO_USER_DEBUG
    printk(VAUDIO_MSG "raudio_mixer_put %d\n", idx);
#endif
    if (idx >= NK_VAUDIO_MIXER_MAX) {
	return 1;
    }

    ctrl = ctrl_controls[idx];
    put  = ctrl_puts[idx];
    if (!ctrl || !put) {
	return 1;
    }

    return put(ctrl, val);
}

static RaudioOps raudio_ops = {
    raudio_open,
    raudio_close,
    raudio_set_sample,
    raudio_start,
    raudio_stop,
    raudio_mixer_info,
    raudio_mixer_get,
    raudio_mixer_put
};

static const struct snd_pcm_hardware pcm_hardware_playback = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.channels_min = 1,
	.channels_max = 2,
        .rate_min = 8000,
        .rate_max = 48000,
	.rates = SNDRV_PCM_RATE_8000_48000,
#ifdef VAUDIO_USER_TIMER
	.period_bytes_min	= 1  * 1024,
	.period_bytes_max	= 1  * 1024,
	.periods_min		= 8,
	.periods_max		= 8,
#else
	.period_bytes_min	= 1  * 1024,
	.period_bytes_max	= 16 * 1024,
	.periods_min		= 8,
	.periods_max		= 64,
#endif
	.buffer_bytes_max	= 128 * 1024,
};

static const struct snd_pcm_hardware pcm_hardware_capture = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.channels_min = 1,
	.channels_max = 2,
        .rate_min = 8000,
        .rate_max = 48000,
	.rates = SNDRV_PCM_RATE_8000_48000,
	.period_bytes_min	= 2  * 1024,
	.period_bytes_max	= 16 * 1024,
	.periods_min		= 8,
	.periods_max		= 64,
	.buffer_bytes_max	= 128 * 1024,
};

    static void
raudio_cleanup(void)
{
    while (vaudio_dev_num) {
        device_destroy(vaudio_class, MKDEV(VAUDIO_MAJOR, vaudio_dev_num - 1));
        vaudio_dev_num--;
    }
    if (vaudio_class) {
        class_destroy(vaudio_class);
    }
    unregister_chrdev(VAUDIO_MAJOR, VAUDIO_NAME);
}

int  raudio_be_callback(int command, void* ops, void* be_cb_func, unsigned int* prefetch,
                                           struct snd_pcm_hardware* pcm_hw)
{
    int err;
    int i, j;
    struct snd_pcm_hardware *cur_stream;
    struct device* cls_dev;
    char devname[16];

    if (command == RAUDIO_CONNECT) {
	memcpy(ops, &raudio_ops, sizeof(RaudioOps));
	be_callback = be_cb_func;
	*prefetch = 0;
	ctrl_idx = 0;

	/*
	 * Register devices as character ones
	 */
        if ((err = register_chrdev(VAUDIO_MAJOR, VAUDIO_NAME, &vaudio_fops))) {
            printk(VAUDIO_ERR "can't register chardev\n");
            raudio_cleanup();
	    return err;
	}
	/*
         * Create "vaudio" device class
	 */
        vaudio_class = class_create(THIS_MODULE, VAUDIO_NAME);
        if (IS_ERR(vaudio_class)) {
            printk(VAUDIO_ERR "can't create class\n");
            raudio_cleanup();
	    return -EIO;
	}

	cur_stream = pcm_hw;
        for (i = 0; i < NK_VAUDIO_DEV_MAX; i++) {
	    for (j = 0; j < NK_VAUDIO_STREAM_MAX; j++) {
	        /*
	         * copy hardware config
	         */
                if (j == SNDRV_PCM_STREAM_PLAYBACK) {
		    memcpy(cur_stream, &pcm_hardware_playback, sizeof(struct snd_pcm_hardware));
	        } else {
		    memcpy(cur_stream, &pcm_hardware_capture, sizeof(struct snd_pcm_hardware));
                }
		cur_stream++;

	        /*
	         * create a class device
	         */
                if (j == SNDRV_PCM_STREAM_PLAYBACK) {
	            sprintf(devname, "%s%ip", VAUDIO_NAME, i);
	        } else {
	            sprintf(devname, "%s%ic", VAUDIO_NAME, i);
	        }
                cls_dev = device_create(vaudio_class, NULL, MKDEV(VAUDIO_MAJOR, vaudio_dev_num),
			                NULL, "%s", devname);
                if (IS_ERR(cls_dev)) {
	            printk(VAUDIO_ERR "can't create device %d %d\n", i, j);
                    raudio_cleanup();
	            return -EIO;
                }
                init_waitqueue_head(&vaudio_dev[vaudio_dev_num].wait_com);
                vaudio_dev[vaudio_dev_num].cok = 0;
                init_waitqueue_head(&vaudio_dev[vaudio_dev_num].wait_res);
                vaudio_dev[vaudio_dev_num].rok     = 0;
                vaudio_dev[vaudio_dev_num].version = VAUDIO_USER_VERSION;
#ifdef VAUDIO_USER_TIMER
		hrtimer_init(&vaudio_dev[vaudio_dev_num].hr_timer,
                             CLOCK_MONOTONIC, HRTIMER_MODE_REL);
                vaudio_dev[vaudio_dev_num].hr_timer.function = timer_handler;
#endif

                vaudio_dev_num++;
	    }
	    cur_stream++;
	}

        backend_connected = 1;
	printk(VAUDIO_INFO "vaudio backend connected to user interface\n");
    } else {

        raudio_cleanup();

        backend_connected = 0;
	printk(VAUDIO_INFO "vaudio backend disconnected from user interface\n");
    }
    return 0;
}

static BLOCKING_NOTIFIER_HEAD(vaudio_be_notifier_list);
int vaudio_be_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&vaudio_be_notifier_list, nb);
}
EXPORT_SYMBOL(vaudio_be_register_client);
int vaudio_be_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&vaudio_be_notifier_list, nb);
}
EXPORT_SYMBOL(vaudio_be_unregister_client);

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <asm/uaccess.h>

#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/delay.h>

#include <mach/dma.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/board.h>

#include <linux/regulator/consumer.h>
#include <mach/hardware.h>
#include <mach/pinmap.h>
#include <mach/regulator.h>
#include <mach/adi.h>
#include <linux/platform_device.h>
//add for intr mode.
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/spi/mxd_cmmb_026x.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
//#include "smsdbg_prn.h"
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <mach/hardware.h>
#include <mach/pinmap.h>
#include <mach/regulator.h>


/*
 * This supports acccess to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/mxdspidevB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */
//#define SPIDEV_MAJOR          153 /* assigned */
int  SPIDEV_MAJOR   =       153;    /* assigned */

#define N_SPI_MINORS            32  /* ... up to 256 */

struct regulator        *reg_vdd1v8 = NULL;
struct regulator        *reg_vdd1v8_1 = NULL;
struct regulator        *reg_vdd1v8_2 = NULL;

#define __MXD_SPI_INTR_SUPPORT__  //for intr enable.

#define MXD_CMMB_RESET    GPIO_CMMB_RESET
//#define MXD_POWER_GPIO    -1
#ifdef __MXD_SPI_INTR_SUPPORT__
#define MXD_INT_GPIO          GPIO_CMMB_INT
#endif

//#define MXD_CMMB_EN      66
//#define GPIO_CMMB_26M_EN    93

#define MXD_PWR_UP_DELAY         15 //10ms
#define MXD_PWR_DN_DELAY         1 // 1ms
#define MXD_RST_DELAY            20 //10ms

#define ANA_REG_BASE       (SPRD_MISC_BASE + 0x600)
#define ANA_MIXED_CTRL  (ANA_REG_BASE + 0x80)

static unsigned long    minors[N_SPI_MINORS / BITS_PER_LONG];
extern struct spi_device *sprd_spi_cmmb_device_register(int master_bus_num, struct spi_board_info *chip);
extern void sprd_spi_tmod(struct spi_device *spi, u32 transfer_mod);


/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for CS_HIGH and 3WIRE can cause *lots* of trouble for other
 * devices on a shared bus:  CS_HIGH, because this device will be
 * active when it shouldn't be;  3WIRE, because when active it won't
 * behave as it should.
 *
 * REVISIT should changing those two modes be privileged?
 */
#define SPI_MODE_MASK       (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
                | SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP)

struct mxdspidev_data
{
    dev_t           devt;
    spinlock_t      spi_lock;
    struct spi_device   *spi;
    struct list_head    device_entry;

	//add by zhangbing.
#ifdef __MXD_SPI_INTR_SUPPORT__
	/*add for irq mode*/
	unsigned long irq_flag;	/* irq flag */
	unsigned char irq_enable;
	wait_queue_head_t iowait;	/* use for user space polling */
#endif
    /* buffer is NULL unless this device is open (users > 0) */
    struct mutex        buf_lock;
    unsigned        users;
    u8          *buffer;
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned bufsiz = (64*1024);
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

#ifdef __MXD_SPI_INTR_SUPPORT__
static int store_irq;
#endif

static   struct mxd_cmmb_026x_platform_data  *pcmmbplat =NULL; 


static  void mxd_chip_poweron()
{
	if(NULL==pcmmbplat)
	{
		pr_debug("NULL==pcmmbplat mxd_chip_poweron!\n");
		return;
	}
	pcmmbplat->poweron();
}
static  void mxd_chip_poweroff()
{
	if(NULL==pcmmbplat)
	{
		pr_debug("NULL==pcmmbplat mxd_chip_poweroff!\n");
		return;
	}
	pcmmbplat->poweroff();
}
static  int mxd_cmmb_init()
{

      if(NULL==pcmmbplat)
	{
		pr_debug("NULL==pcmmbplat mxd_cmmb_gpio_init!\n");
		return -1;
	}
	return pcmmbplat->init();
}

int comip_mxd0251_power(int onoff)
{
    pr_debug("mxd0251 pw %d\n", onoff);
    if(onoff)
    {
	if( (pcmmbplat!=NULL) && (pcmmbplat->restore_spi_pin_cfg!=NULL) ){
		pcmmbplat->restore_spi_pin_cfg();
	}

        mxd_chip_poweron();
        msleep(20);
		//reset chip.
        gpio_direction_output(MXD_CMMB_RESET,1);
        msleep(10);
		//gpio_direction_output(MXD_CMMB_RESET,0);
        gpio_set_value(MXD_CMMB_RESET, 0);
        msleep(20);
		//gpio_direction_output(MXD_CMMB_RESET,1);
        gpio_set_value(MXD_CMMB_RESET, 1);
        msleep(20);

		//intr GPIO input state.
#ifdef __MXD_SPI_INTR_SUPPORT__  //for intr enable.
		gpio_direction_input(MXD_INT_GPIO);
#endif
    }
    else
    {
        /* TODO : close clock */
		gpio_direction_output(MXD_CMMB_RESET,0);
		//gpio_direction_output(MXD_CMMB_EN,0);
		mxd_chip_poweroff();
#ifdef __MXD_SPI_INTR_SUPPORT__  //for intr enable.
		gpio_direction_output(MXD_INT_GPIO,0);
#endif
		if( (pcmmbplat!=NULL) && (pcmmbplat->set_spi_pin_input!=NULL) ){
			pcmmbplat->set_spi_pin_input();
		}

		msleep(10);
    }

    return 0;
}


/*-------------------------------------------------------------------------*/

/*
 * We can't use the standard synchronous wrappers for file I/O; we
 * need to protect against async removal of the underlying spi_device.
 */
static void mxdspidev_complete(void *arg)
{
    complete(arg);
}

static ssize_t mxdspidev_sync(struct mxdspidev_data *mxdspidev, struct spi_message *message)
{
    DECLARE_COMPLETION_ONSTACK(done);
    int status;

    message->complete = mxdspidev_complete;
    message->context = &done;

    spin_lock_irq(&mxdspidev->spi_lock);
    if(mxdspidev->spi == NULL)
        status = -ESHUTDOWN;
    else
        status = spi_async(mxdspidev->spi, message);

    spin_unlock_irq(&mxdspidev->spi_lock);

    if(status == 0)
    {
        wait_for_completion(&done);
        status = message->status;
        if(status == 0)
            status = message->actual_length;
    }
    return status;
}

static inline ssize_t mxdspidev_sync_write(struct mxdspidev_data *mxdspidev, size_t len)
{
    struct spi_transfer t =
    {
        .tx_buf     = mxdspidev->buffer,
        .len        = len,
    };
    struct spi_message  m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return mxdspidev_sync(mxdspidev, &m);
}

static inline ssize_t mxdspidev_sync_read(struct mxdspidev_data *mxdspidev, size_t len)
{
    struct spi_transfer t =
    {
        .rx_buf     = mxdspidev->buffer,
        .len        = len,
    };
    struct spi_message  m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return mxdspidev_sync(mxdspidev, &m);
}

/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
static ssize_t mxdspidev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct mxdspidev_data   *mxdspidev;
    ssize_t         status = 0;

    /* chipselect only toggles at start or end of operation */
    if(count > bufsiz)
        return -EMSGSIZE;

    mxdspidev = filp->private_data;

    mutex_lock(&mxdspidev->buf_lock);
    status = mxdspidev_sync_read(mxdspidev, count);
    if(status > 0)
    {
        unsigned long   missing;

        missing = copy_to_user(buf, mxdspidev->buffer, status);
        if(missing == status)
            status = -EFAULT;
        else
            status = status - missing;
    }
    mutex_unlock(&mxdspidev->buf_lock);

    return status;
}

/* Write-only message with current device setup */
static ssize_t mxdspidev_write(struct file *filp, const char __user *buf,size_t count, loff_t *f_pos)
{
    struct mxdspidev_data   *mxdspidev;
    ssize_t         status = 0;
    unsigned long       missing;

    /* chipselect only toggles at start or end of operation */
    if(count > bufsiz)
        return -EMSGSIZE;

    mxdspidev = filp->private_data;

    mutex_lock(&mxdspidev->buf_lock);
    missing = copy_from_user(mxdspidev->buffer, buf, count);
    if(missing == 0)
    {
        status = mxdspidev_sync_write(mxdspidev, count);
    }
    else
        status = -EFAULT;
    mutex_unlock(&mxdspidev->buf_lock);

    return status;
}

static int mxdspidev_message(struct mxdspidev_data *mxdspidev,struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
    struct spi_message  msg;
    struct spi_transfer *k_xfers;

    //struct spi_transfer   *k_xfers1;
    struct spi_transfer *k_tmp;
    struct spi_ioc_transfer *u_tmp;
    unsigned        n, total;
    u8          *buf;
    int         status = -EFAULT;

    spi_message_init(&msg);
    k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
    if(k_xfers == NULL)
    {
        pr_debug("kcalloc fail !!\n");
        return -ENOMEM;
    }
    /* Construct spi_message, copying any tx data to bounce buffer.
     * We walk the array of user-provided transfers, using each one
     * to initialize a kernel version of the same transfer.
     */
    buf = mxdspidev->buffer;
    total = 0;
    for(n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
        n;
        n--, k_tmp++, u_tmp++)
    {
        k_tmp->len = u_tmp->len;

        total += k_tmp->len;
        if(total > bufsiz)
        {
            status = -EMSGSIZE;
            pr_debug("goto done ,starus = %d  total=%d \n",status,total);
            goto done;
        }

        if(u_tmp->rx_buf)
        {
            k_tmp->rx_buf = buf;
            if(!access_ok(VERIFY_WRITE, (u8 __user *)
                          (uintptr_t) u_tmp->rx_buf,
                          u_tmp->len))
            {
                pr_debug("access_ok() not ,goto done!\n");
                goto done;
            }
        }
        if(u_tmp->tx_buf)
        {
            k_tmp->tx_buf = buf;
            if(copy_from_user(buf, (const u8 __user *)
                              (uintptr_t) u_tmp->tx_buf,
                              u_tmp->len))
            {
                pr_debug("copy_from_user(),goto done!\n");
                goto done;
            }
        }
        buf += k_tmp->len;

        k_tmp->cs_change = !!u_tmp->cs_change;
        k_tmp->bits_per_word = u_tmp->bits_per_word;
        k_tmp->delay_usecs = u_tmp->delay_usecs;
        k_tmp->speed_hz = u_tmp->speed_hz;
#ifdef VERBOSE
        dev_dbg(&spi->dev,
                "  xfer len %zd %s%s%s%dbits %u usec %uHz\n",
                u_tmp->len,
                u_tmp->rx_buf ? "rx " : "",
                u_tmp->tx_buf ? "tx " : "",
                u_tmp->cs_change ? "cs " : "",
                u_tmp->bits_per_word ? : spi->bits_per_word,
                u_tmp->delay_usecs,
                u_tmp->speed_hz ? : spi->max_speed_hz);
#endif
        spi_message_add_tail(k_tmp, &msg);
    }

#if 0
    k_xfers1 = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
    if(k_xfers1 == NULL)
    {
        pr_debug("kcalloc fail !!\n");
        return -ENOMEM;
    }
    /* Construct spi_message, copying any tx data to bounce buffer.
     * We walk the array of user-provided transfers, using each one
     * to initialize a kernel version of the same transfer.
     */
    buf = mxdspidev->buffer;
    total = 0;
    for(n = n_xfers, k_tmp = k_xfers1, u_tmp = u_xfers;
        n;
        n--, k_tmp++, u_tmp++)
    {
        k_tmp->len = u_tmp->len;

        total += k_tmp->len;
        if(total > bufsiz)
        {
            status = -EMSGSIZE;
            pr_debug("goto done ,starus = %d\n",status);
            goto done;
        }

        if(u_tmp->rx_buf)
        {
            k_tmp->rx_buf = buf;
            if(!access_ok(VERIFY_WRITE, (u8 __user *)
                          (uintptr_t) u_tmp->rx_buf,
                          u_tmp->len))
            {
                pr_debug("access_ok() not ,goto done!\n");
                goto done;
            }
        }
        /*
        if (u_tmp->tx_buf) {
            k_tmp->tx_buf = buf;
            if (copy_from_user(buf, (const u8 __user *)
                        (uintptr_t) u_tmp->tx_buf,
                    u_tmp->len)){
                pr_debug("copy_from_user(),goto done!\n");
                goto done;}
        }*/
        buf += k_tmp->len;

        k_tmp->cs_change = !!u_tmp->cs_change;
        k_tmp->bits_per_word = u_tmp->bits_per_word;
        k_tmp->delay_usecs = u_tmp->delay_usecs;
        k_tmp->speed_hz = u_tmp->speed_hz;
#ifdef VERBOSE
        dev_dbg(&spi->dev,
                "  xfer len %zd %s%s%s%dbits %u usec %uHz\n",
                u_tmp->len,
                u_tmp->rx_buf ? "rx " : "",
                u_tmp->tx_buf ? "tx " : "",
                u_tmp->cs_change ? "cs " : "",
                u_tmp->bits_per_word ? : spi->bits_per_word,
                u_tmp->delay_usecs,
                u_tmp->speed_hz ? : spi->max_speed_hz);
#endif
        spi_message_add_tail(k_tmp, &msg);
    }

#endif
    status = mxdspidev_sync(mxdspidev, &msg);
    if(status < 0)
        goto done;

    /* copy any rx data out of bounce buffer */
    buf = mxdspidev->buffer;
    for(n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++)
    {
        if(u_tmp->rx_buf)
        {
            if(__copy_to_user((u8 __user *)
                              (uintptr_t) u_tmp->rx_buf, buf,
                              u_tmp->len))
            {
                status = -EFAULT;
                goto done;
            }
        }
        buf += u_tmp->len;
    }
    status = total;

done:
    kfree(k_xfers);

    //kfree(k_xfers1);
    return status;
}

static long
mxdspidev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int         err = 0;
    int         retval = 0;
    struct mxdspidev_data   *mxdspidev;
    struct spi_device   *spi;
    u32         tmp;
    unsigned        n_ioc;
    struct spi_ioc_transfer *ioc;

    /* Check type and command number */
    if(_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
    {
        pr_debug("_ioc_type(cmd) is %c,SPI_IOC_MAGIC is %c\n",_IOC_TYPE(cmd),SPI_IOC_MAGIC);
        pr_debug("the ioctl cmd is error !\n");
        return -ENOTTY;
    }

    /* Check access direction once here; don't repeat below.
     * IOC_DIR is from the user perspective, while access_ok is
     * from the kernel perspective; so they look reversed.
     */
    if(_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE,
                         (void __user *)arg, _IOC_SIZE(cmd));
    if(err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ,
                         (void __user *)arg, _IOC_SIZE(cmd));
    if(err)
    {
        pr_debug("err !!!return -EFAULT\n");
        return -EFAULT;
    }

    /* guard against device removal before, or while,
     * we issue this ioctl.
     */
    mxdspidev = filp->private_data;
    spin_lock_irq(&mxdspidev->spi_lock);
    spi = spi_dev_get(mxdspidev->spi);
    spin_unlock_irq(&mxdspidev->spi_lock);

    if(spi == NULL)
    {
        pr_debug("spi is NULL !\n");
        return -ESHUTDOWN;
    }

    /* use the buffer lock here for triple duty:
     *  - prevent I/O (from us) so calling spi_setup() is safe;
     *  - prevent concurrent SPI_IOC_WR_* from morphing
     *    data fields while SPI_IOC_RD_* reads them;
     *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
     */
    mutex_lock(&mxdspidev->buf_lock);
#if 1
    switch(cmd)
    {
            /* read requests */
        case SPI_IOC_RD_MODE:
            retval = __put_user(spi->mode & SPI_MODE_MASK,
                                (__u8 __user *)arg);
            break;
        case SPI_IOC_RD_LSB_FIRST:
            retval = __put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
                                (__u8 __user *)arg);
            break;
        case SPI_IOC_RD_BITS_PER_WORD:
            retval = __put_user(spi->bits_per_word, (__u8 __user *)arg);
            break;
        case SPI_IOC_RD_MAX_SPEED_HZ:
            retval = __put_user(spi->max_speed_hz, (__u32 __user *)arg);
            break;

            /* write requests */
        case SPI_IOC_WR_MODE:
            retval = __get_user(tmp, (u8 __user *)arg);
            /*if (retval == 0) {
                u8  save = spi->mode;

                if (tmp & ~SPI_MODE_MASK) {
                    retval = -EINVAL;
                    break;
                }

                tmp |= spi->mode & ~SPI_MODE_MASK;
                spi->mode = (u8)tmp;
                retval = spi_setup(spi);
                if (retval < 0)
                    spi->mode = save;
                else
                    dev_dbg(&spi->dev, "spi mode %02x\n", tmp);
            }*/
            mxdspidev_sync_write(mxdspidev,32);
            break;
        case SPI_IOC_WR_LSB_FIRST:
            retval = __get_user(tmp, (__u8 __user *)arg);
            if(retval == 0)
            {
                u8  save = spi->mode;

                if(tmp)
                    spi->mode |= SPI_LSB_FIRST;
                else
                    spi->mode &= ~SPI_LSB_FIRST;
                retval = spi_setup(spi);
                if(retval < 0)
                    spi->mode = save;
                else
                    dev_dbg(&spi->dev, "%csb first\n",
                            tmp ? 'l' : 'm');
            }
            break;
        case SPI_IOC_WR_BITS_PER_WORD:
            retval = __get_user(tmp, (__u8 __user *)arg);
            if(retval == 0)
            {
                u8  save = spi->bits_per_word;

                spi->bits_per_word = tmp;
                retval = spi_setup(spi);
                if(retval < 0)
                    spi->bits_per_word = save;
                else
                    dev_dbg(&spi->dev, "%d bits per word\n", tmp);
            }
            break;
        case SPI_IOC_WR_MAX_SPEED_HZ:
            retval = __get_user(tmp, (__u32 __user *)arg);
            if(retval == 0)
            {
                u32 save = spi->max_speed_hz;

                spi->max_speed_hz = tmp;
                retval = spi_setup(spi);
                if(retval < 0)
                    spi->max_speed_hz = save;
                else
                    dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
            }
            break;

        default:
            /* segmented and/or full-duplex I/O request */
#if 1
            if(_IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
               || _IOC_DIR(cmd) != _IOC_WRITE)
            {
                retval = -ENOTTY;
                pr_debug("_ioc_nr(cmd) is %d,_ioc_nr(spi_ioc_massage(0)) is %d ,_IOC_DIR(cmd) is %d\n",_IOC_NR(cmd),_IOC_NR(SPI_IOC_MESSAGE(0)),_IOC_DIR(cmd));
                pr_debug("first if error retval = %d\n",retval);
                //break;
            }

            tmp = _IOC_SIZE(cmd);
            //pr_debug("_IOC_SIZE(cmd) is %d sizeof(spi_ioc_transfer) is %d\n",tmp,sizeof(struct spi_ioc_transfer));
            if((tmp % sizeof(struct spi_ioc_transfer)) != 0)
            {
                retval = -EINVAL;
                pr_debug("second if error retval = %d\n",retval);
                //break;
            }
            n_ioc = tmp / sizeof(struct spi_ioc_transfer);
            if(n_ioc == 0)
            {
                pr_debug("third if error retval = %d\n",retval);
                //break;
            }
            /* copy into scratch area */
            ioc = kmalloc(tmp, GFP_KERNEL);
            if(!ioc)
            {
                retval = -ENOMEM;
                pr_debug("fourth if error retval = %d\n",retval);
                //break;
            }
            if(__copy_from_user(ioc, (void __user *)arg, tmp))
            {
                kfree(ioc);
                retval = -EFAULT;
                pr_debug("fifth if error retval = %d\n",retval);
                //break;
            }
#endif
            /* translate to spi_message, execute */

            //ret = ioctl(fd_dev, SPI_IOC_MESSAGE(1), &spi_msg);


            //pr_debug("mxdspidev_message::  = %d\n",retval);
            retval = mxdspidev_message(mxdspidev, ioc, n_ioc);
            kfree(ioc);
            //mxdspidev_sync_write(mxdspidev,32);
            break;
    }
#endif
    mutex_unlock(&mxdspidev->buf_lock);
    //mxdspidev_sync_write(mxdspidev,32);
    spi_dev_put(spi);
    return retval;
}


//static struct platform_device maxd_demod_device =
//{
//   .name   = "cmmb-dev",
//    .id     = -1,
//};

//static int __init spi_module_init(void)
//{
//struct spi_device *spi_dev;
//    pr_debug("spi_module_init begin !\n");
//    platform_device_register(&maxd_demod_device);
//    return 0;
//}
static int mxdspidev_open(struct inode *inode, struct file *filp)
{
    struct mxdspidev_data   *mxdspidev;
    int         status = -ENXIO;

    mutex_lock(&device_list_lock);
    pr_debug("liwk mxdspidev_open\n");
    comip_mxd0251_power(1);
    list_for_each_entry(mxdspidev, &device_list, device_entry)
    {
        if(mxdspidev->devt == inode->i_rdev)
        {
            status = 0;
            break;
        }
    }
    if(status == 0)
    {
        if(!mxdspidev->buffer)
        {
           //mxdspidev->buffer = kmalloc(bufsiz, (GFP_KERNEL|GFP_DMA));
           mxdspidev->buffer = kmalloc(bufsiz, GFP_KERNEL);

            if(!mxdspidev->buffer)
            {	
                dev_dbg(&mxdspidev->spi->dev, "open/ENOMEM\n");
                status = -ENOMEM;
            }
        }
        if(status == 0)
        {
            mxdspidev->users++;
            filp->private_data = mxdspidev;
            nonseekable_open(inode, filp);
        }
    }
    else
        pr_debug("mxdspidev: nothing for minor %d\n", iminor(inode));

    mutex_unlock(&device_list_lock);
	//add by zhangbing to enable intr.
	#ifdef __MXD_SPI_INTR_SUPPORT__
	pr_debug("enable irq!\n");
	enable_irq(store_irq);
	#endif
    return status;
}

static int mxdspidev_release(struct inode *inode, struct file *filp)
{
    struct mxdspidev_data   *mxdspidev;
    int         status = 0;

    mutex_lock(&device_list_lock);
    mxdspidev = filp->private_data;
    filp->private_data = NULL;
    pr_debug("liwk mxdspidev_release\n");
	//add by zhangbing to disable intr.
	#ifdef __MXD_SPI_INTR_SUPPORT__
	pr_debug("enable irq!\n");
	disable_irq(store_irq);
	#endif
    comip_mxd0251_power(0);

    /* last close? */
    mxdspidev->users--;
    if(!mxdspidev->users)
    {
        int     dofree;

        kfree(mxdspidev->buffer);
        mxdspidev->buffer = NULL;

        /* ... after we unbound from the underlying device? */
        spin_lock_irq(&mxdspidev->spi_lock);
        dofree = (mxdspidev->spi == NULL);
        spin_unlock_irq(&mxdspidev->spi_lock);

        if(dofree)
            kfree(mxdspidev);
    }
    mutex_unlock(&device_list_lock);

    return status;
}
#ifdef __MXD_SPI_INTR_SUPPORT__
/* for intr process*/
static irqreturn_t mxdspidev_irq(int irq, void *data)
{
	struct mxdspidev_data *cmmb_dev = data;
	int frame = 0;

	//pr_debug("enter cmmb intr!\n");
	spin_lock(&cmmb_dev->spi_lock);
	/*
	 * Basic frame housekeeping.

	 */
	if (test_bit(frame, &cmmb_dev->irq_flag) && printk_ratelimit()) {
		/* LOG_INFO("Frame overrun on %d, frames lost\n", frame); */
		;
	}
	set_bit(frame, &cmmb_dev->irq_flag);
	spin_unlock(&cmmb_dev->spi_lock);
	wake_up_interruptible(&cmmb_dev->iowait);

	return IRQ_HANDLED;
}
/*for intr poll*/
static unsigned int mxdspidev_poll(struct file *filp,
				   struct poll_table_struct *pt)
{
	int frame = 0;
	int data_ready = 0;
	struct mxdspidev_data *cmmb_dev;
	cmmb_dev = filp->private_data;
	//pr_debug("enter cmmb poll!\n");
	poll_wait(filp, &cmmb_dev->iowait, pt);

	spin_lock(&cmmb_dev->spi_lock);	/* TODO: not need? */
	data_ready = test_and_clear_bit(frame, &cmmb_dev->irq_flag);
	spin_unlock(&cmmb_dev->spi_lock);

	if (data_ready)
		return POLLIN | POLLRDNORM;
	else
		return 0;
}
#endif
static struct file_operations mxdspidev_fops =
{
    .owner =    THIS_MODULE,
    /* REVISIT switch to aio primitives, so that userspace
     * gets more complete API coverage.  It'll simplify things
     * too, except for the locking.
     */
    .write =    mxdspidev_write,
    .read =     mxdspidev_read,
    .unlocked_ioctl = mxdspidev_ioctl,
#ifdef __MXD_SPI_INTR_SUPPORT__
	.poll = mxdspidev_poll, //for irq mode.
#endif
    .open =     mxdspidev_open,
    .release =  mxdspidev_release,
};

#define MXD_SPI_BITS_PER_WORD                   (8)
#define MXD_SPI_WORK_SPEED_HZ                   (13000000)
#define MXD_SPI_INIT_SPEED_HZ                   (13000000)


/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/mxdspidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */

static struct class *mxdspidev_class;
struct mxd0251_platform_data
{
    int gpio_irq;
    int (*power)(int onoff);
};

/*-------------------------------------------------------------------------*/
static int mxdspi_gpio_init()
{
	int ret=0;

    ret = gpio_request(MXD_CMMB_RESET,   "MXD_CMMB_RESET");
	if (ret)
	{
		pr_debug("mxd spi req gpio reset err!\n");
		goto err_gpio_reset;
	}
       gpio_direction_output(MXD_CMMB_RESET, 0);
	gpio_set_value(MXD_CMMB_RESET, 0);
	
	return  mxd_cmmb_init();


err_gpio_reset:
	gpio_free(MXD_CMMB_RESET);

	return ret;
}


static int mxdspidev_probe(struct spi_device *spi)
{
    struct mxdspidev_data   *mxdspidev;
    int         status;
    unsigned long       minor;
    //struct mxd_spi_phy *phy = (struct mxd_spi_phy *)gphy;
    struct mxd0251_platform_data* pdata = spi->controller_data;
    int ret;
#ifdef __MXD_SPI_INTR_SUPPORT__
    int gpio_irq;
    int irq;
#endif
   pcmmbplat = spi->dev.platform_data;
    if(!pdata)
        return -EINVAL;

	if( (pcmmbplat!=NULL) && (pcmmbplat->set_spi_pin_input!=NULL) ){
		pcmmbplat->set_spi_pin_input();
	}

    ret = mxdspi_gpio_init();
	if (ret)
	{
		pr_debug("mxd plat spi req gpio err!\n");
		goto err_gpio;
	}

    /* Allocate driver data */
    mxdspidev = kzalloc(sizeof(*mxdspidev), GFP_KERNEL);
    if(!mxdspidev)
    {
        pr_debug("kzalloc fail !!\n");
        return -ENOMEM;
    }

    /* Initialize the driver data */
    mxdspidev->spi = spi;
    spin_lock_init(&mxdspidev->spi_lock);
    mutex_init(&mxdspidev->buf_lock);
    INIT_LIST_HEAD(&mxdspidev->device_entry);

	//add by zhangbing.
#ifdef __MXD_SPI_INTR_SUPPORT__
	/*for cmmb wait queue.*/
	init_waitqueue_head(&mxdspidev->iowait);
    	/* Initialize the interrupt pin for chip */
	gpio_irq = MXD_INT_GPIO; 
	gpio_free(gpio_irq);
	ret = gpio_request(gpio_irq, "demod int");
	if (ret)
	{
		pr_debug("mxd spi req gpio err: %d\n",gpio_irq);
		goto err_irq;
	}
	gpio_direction_input(gpio_irq);

	irq = gpio_to_irq(gpio_irq);
	store_irq = irq;
	pr_debug("mxd spi irq req 1:%d,%d\n",gpio_irq, irq);
	ret = request_irq(irq, mxdspidev_irq, IRQF_TRIGGER_FALLING, "MXDSPIIRQ", mxdspidev);
	if (ret) 
	{
		pr_debug("CMMB request irq failed.\n");
		goto err_irq;
	}

	disable_irq(irq);
	pr_debug("request_irq OK\n");
#endif

    /* If we can allocate a minor number, hook up this device.
     * Reusing minors is fine so long as udev or mdev is working.
     */

    mutex_lock(&device_list_lock);
    minor = find_first_zero_bit(minors, N_SPI_MINORS);
    if(minor < N_SPI_MINORS)
    {
        struct device *dev;
        mxdspidev->devt = MKDEV(SPIDEV_MAJOR, minor);
        dev = device_create(mxdspidev_class, &spi->dev, mxdspidev->devt,//mxdspidev, "mxdspidev");
                            mxdspidev, "mxdspidev");//%d.%d",
        // spi->master->bus_num, spi->chip_select);
        status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
    }
    else
    {
        dev_dbg(&spi->dev, "no minor number available!\n");
        status = -ENODEV;
    }
    if(status == 0)
    {
        set_bit(minor, minors);
        list_add(&mxdspidev->device_entry, &device_list);
    }
    mutex_unlock(&device_list_lock);
    if(status == 0)
        spi_set_drvdata(spi, mxdspidev);
    else
        kfree(mxdspidev);

    return status;
#ifdef __MXD_SPI_INTR_SUPPORT__
err_irq:
    gpio_free(gpio_irq);
#endif
err_gpio:
    return ret;
}

static int mxdspidev_remove(struct spi_device *spi)
{
    struct mxdspidev_data   *mxdspidev = spi_get_drvdata(spi);

    /* make sure ops on existing fds can abort cleanly */
    spin_lock_irq(&mxdspidev->spi_lock);
    mxdspidev->spi = NULL;
    spi_set_drvdata(spi, NULL);
    spin_unlock_irq(&mxdspidev->spi_lock);

    /* prevent new opens */
    mutex_lock(&device_list_lock);
    list_del(&mxdspidev->device_entry);
    device_destroy(mxdspidev_class, mxdspidev->devt);
    clear_bit(MINOR(mxdspidev->devt), minors);
    if(mxdspidev->users == 0)
        kfree(mxdspidev);
    mutex_unlock(&device_list_lock);

    comip_mxd0251_power(0);

    return 0;
}

static struct spi_driver mxdspidev_spi =
{
    .driver = {
        .name =     "cmmb-dev",
        .owner =    THIS_MODULE,
    },
    .probe =    mxdspidev_probe,
    .remove =   __devexit_p(mxdspidev_remove),

    /* NOTE:  suspend/resume methods are not necessary here.
     * We don't do anything except pass the requests to/from
     * the underlying controller.  The refrigerator handles
     * most issues; the controller driver handles the rest.
     */
};

/*-------------------------------------------------------------------------*/

static int __init mxdspidev_init(void)
{
    int major;

    /* Claim our 256 reserved device numbers.  Then register a class
     * that will key udev/mdev to add/remove /dev nodes.  Last, register
     * the driver which manages those device numbers.
     */
    BUILD_BUG_ON(N_SPI_MINORS > 256);
    //spi_module_init();
    major = register_chrdev(0, "spi", &mxdspidev_fops);
    if(major < 0)
    {
        pr_debug("register_chrdev fail !!major = %d\n",major);
        return major;
    }
    SPIDEV_MAJOR = major;

    mxdspidev_class = class_create(THIS_MODULE, "mxdspidev");
    if(IS_ERR(mxdspidev_class))
    {
        unregister_chrdev(major, mxdspidev_spi.driver.name);
        pr_debug("class_create fail !!major = %d\n",major);
        return PTR_ERR(mxdspidev_class);
    }
    major= spi_register_driver(&mxdspidev_spi);
    if(major< 0)
    {
        class_destroy(mxdspidev_class);
        unregister_chrdev(major, mxdspidev_spi.driver.name);
        pr_debug("spi_register_driver fail !!major = %d\n",major);
    }
    pr_debug("mxdspidev_init end major=%d!\n",major);
    return major;
}
module_init(mxdspidev_init);

static void __exit mxdspidev_exit(void)
{
    spi_unregister_driver(&mxdspidev_spi);
    class_destroy(mxdspidev_class);
    unregister_chrdev(SPIDEV_MAJOR, mxdspidev_spi.driver.name);
    //platform_device_unregister(&maxd_demod_device);
}
module_exit(mxdspidev_exit);

MODULE_AUTHOR("David Jiang");
MODULE_DESCRIPTION("Maxscend User mode SPI device interface");
MODULE_LICENSE("GPL");

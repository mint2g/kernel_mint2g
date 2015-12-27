/*******************************************************************************
 * Source file : kt0812g_fm_ctl.c
 * Description : KT0812G FM Receiver driver for linux.
 * Date        : 05/11/2011
 *
 * Copyright (C) 2011 Spreadtum Inc.
 *
 ********************************************************************************
 * Revison
 2011-05-11  aijun.sun   initial version
 *******************************************************************************/
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include "kt0812g_fm_ctrl.h"

#define KT0812G_DEBUG 1

#define KT0812G_WAKEUP_CHECK_TIME         100
#define KT0812G_WAKEUP_TIMEOUT            800

#define KT0812G_SEEK_CHECK_TIME           50

#define KT0812G_TUNE_DELAY                50

#define I2C_RETRY_DELAY                   5
#define I2C_RETRIES                       3

#define	KT0812G_DEV_NAME	"KT0812G_FM"
#define KT0812G_I2C_NAME    KT0812G_DEV_NAME

const u8 KT0812G_register_map[] = {
    KT0812G_REG_DEVICE,
    KT0812G_REG_CHIPID,
    KT0812G_REG_SEEK,
    KT0812G_REG_TUNE,
    KT0812G_REG_VOLUME,
    KT0812G_REG_DSPCFGA,
    KT0812G_REG_RFCFG,
    KT0812G_REG_LOCFGA,
    KT0812G_REG_SYSCFG,
    KT0812G_REG_STATUSA,
    KT0812G_REG_STATUSB,
    KT0812G_REG_STATUSC,
    KT0812G_REG_STATUSD,
    KT0812G_REG_ANTENNA,
    KT0812G_REG_SNR,
    KT0812G_REG_SEEKTH,
    KT0812G_REG_SOFTMUTE,
    KT0812G_REG_CLOCK
};


struct kt0812g_drv_data {
    struct i2c_client *client;
    struct class      fm_class;
    int               opened_before_suspend;
    int               bg_play_enable; /* enable/disable background play. */
    struct mutex      mutex;
    atomic_t          fm_opened;
    atomic_t          fm_searching;
    int               current_freq;
    int               current_volume;
    u8                muteOn;
    struct regulator  *regu;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend;
#endif
};


struct kt0812g_drv_data *kt0812g_dev_data = NULL;

/***
 * Common i2c read and write function based i2c_transfer()
 *  The read operation sequence:
 *  7 bit chip address and Write command ("0") -> 8 bit
 *  register address n -> 7 bit chip address and Read command("1")
 *  The write operation sequence:
 * 7 bit chip address and Write command ("0") -> 8 bit
 * register address n -> write data n [15:8] -> write data n [7:0]
 ***/
static int kt0812g_i2c_read(struct kt0812g_drv_data *cxt, u8 * buf, int len)
{
    int err = 0;
    int tries = 0;

    struct i2c_msg	msgs[] = {
        {
            .addr = cxt->client->addr,
            .flags = cxt->client->flags & I2C_M_TEN,
            .len = 1,
            .buf = buf,
        },
        {
            .addr = cxt->client->addr,
            .flags = (cxt->client->flags & I2C_M_TEN) | I2C_M_RD,
            .len = len,
            .buf = buf,
        },
    };

    do {
        err = i2c_transfer(cxt->client->adapter, msgs, 2);
        if (err != 2)
            msleep_interruptible(I2C_RETRY_DELAY);
    } while ((err != 2) && (++tries < I2C_RETRIES));

    if (err != 2) {
        dev_err(&cxt->client->dev, "Read transfer error\n");
        err = -EIO;
    } else {
        err = 0;
    }

    return err;
}


static int kt0812g_i2c_write(struct kt0812g_drv_data *cxt, u8 * buf, int len)
{
    int err = 0;
    int tries = 0;

    struct i2c_msg msgs[] = {
        {
            .addr = cxt->client->addr,
            .flags = cxt->client->flags & I2C_M_TEN,
            .len = len,
            .buf = buf,
        },
    };

    do {
        err = i2c_transfer(cxt->client->adapter, msgs, 1);
        if (err != 1)
            msleep_interruptible(I2C_RETRY_DELAY);
    } while ((err != 1) && (++tries < I2C_RETRIES));

    if (err != 1) {
        dev_err(&cxt->client->dev, "write transfer error\n");
        err = -EIO;
    } else {
        err = 0;
    }

    return err;
}


/**
 * Notes: all register are 16bit wide, so the register R&W function uses
 *   2-byte arguments. */
static int kt0812g_register_read(struct kt0812g_drv_data *cxt,
        u8 reg_address,
        u16 *value)
{
    int  ret = -EINVAL;
    u8   buf[8] = {0};

    buf[0] = reg_address;
    ret = kt0812g_i2c_read(cxt, buf, 2);
    if (ret >= 0) {
        *value = (buf[0]<<8)|(buf[1]); //MSB first
    }

    return ret;
}


static int kt0812g_register_write(struct kt0812g_drv_data *cxt,
        u8 reg_address,
        u16 value)
{
    int  ret = -EINVAL;
    u8   buf[8] = {0};

    buf[0] = reg_address;
    buf[1] = value >> 8; // MSB first
    buf[2] = value & 0xff;
    ret = kt0812g_i2c_write(cxt, buf, 3);

    return ret;
}


#if defined(KT0812G_DEBUG)
static int kt0812g_read_all_registers(struct kt0812g_drv_data *cxt)
{
    int i = 0;
    int reg_num = sizeof(KT0812G_register_map)/sizeof(KT0812G_register_map[0]);
    u16 reg_value = 0x0;
    int ret = -EINVAL;

    for (i = 0; i < reg_num; i++) {
        u16 reg = KT0812G_register_map[i];
        ret = kt0812g_register_read(cxt, reg, &reg_value);
        if (ret < 0) {
            dev_err(&cxt->client->dev,
                    "Read register 0x%02x error.\n", reg);
        }
        else {
            dev_info(&cxt->client->dev,
                    "Read register [0x%02x]=0x%04x\n", reg, reg_value);
        }
    }

    return ret;
}
#endif


static int kt0812g_check_chip_id(struct kt0812g_drv_data *cxt, u16 *chip_id) {
    int ret = 0;

    /* read chip id of KT0812 */
    ret = kt0812g_register_read(cxt, KT0812G_REG_CHIPID, chip_id);
    if (ret < 0) {
        dev_err(&cxt->client->dev, "Read chip id failed.\n");
    }
    else {
        dev_info(&cxt->client->dev, "KT0812G chip id:0x%04x\n", *chip_id);
    }

    return ret;
}


static void kt0812g_chip_vdd_input(struct kt0812g_drv_data *cxt,bool turn_on)
{
    static unsigned int reg_value = 0;

    if (turn_on) {
        if(cxt->regu != NULL) {
            regulator_set_mode(cxt->regu,REGULATOR_MODE_NORMAL);
            regulator_enable(cxt->regu);
        }
        /* keep LDO_SIM2 on in deep sleep */
	 mdelay(5);
    }
    else {
        mdelay(5);
        if(cxt->regu != NULL) {
            regulator_set_mode(cxt->regu,REGULATOR_MODE_STANDBY);
	    regulator_disable(cxt->regu);
        }
    }
}


/* NOTES:
 * This function is private call by 'probe function', so not add re-entry
 * protect. */
static int kt0812g_fm_power_on(struct kt0812g_drv_data *cxt)
{
    u16     reg_value        = 0x0;
    int     check_times      = 0;
    int     ret              = -EINVAL;
    ulong   jiffies_comp     = 0;
    u8      is_timeout;

    dev_info(&cxt->client->dev, "%s\n", __func__);

    /* turn on vdd */
    kt0812g_chip_vdd_input(cxt,true);

    /* power on the chip to check chip status. */
    /* step1: wake up the module.  */
    reg_value = 0x8A00;
    ret = kt0812g_register_write(cxt, KT0812G_REG_SYSCFG, reg_value);
    if (ret < 0) {
        dev_err(&cxt->client->dev, "KT0812G wake up failed.\n");
        goto chip_poweron_failed;
    }
    /* step2: set KT0812G_ANT_TUNE_EN & KT0812G_RCLK_EN,32KHz */
    reg_value = 0x0001 | KT0812G_RCLK_EN;
    ret = kt0812g_register_write(cxt, KT0812G_REG_CLOCK, reg_value);
    if (ret < 0) {
        dev_err(&cxt->client->dev, "KT0812G set clock and tune failed.\n");
        goto chip_poweron_failed;
    }

    /* step3: wait chip ready. */
    jiffies_comp = jiffies;
    while(1) {
        msleep_interruptible(KT0812G_WAKEUP_CHECK_TIME);
        if (signal_pending(current))
            break;
        /* check chip status */
        ret = kt0812g_register_read(cxt, KT0812G_REG_STATUSA, &reg_value);

        is_timeout = time_after(jiffies,
                jiffies_comp + msecs_to_jiffies(KT0812G_WAKEUP_TIMEOUT));

        if (is_timeout || ((ret >= 0) &&
                    (KT0812G_XTAL_OK & reg_value) &&
                    (KT0812G_PLL_LOCK & reg_value))) {
            break;
        }
    };

    if (is_timeout) {
        dev_err(&cxt->client->dev, "KT0812G check chip status overload");
        goto chip_poweron_failed;
    }
    else {
        dev_info(&cxt->client->dev, "KT0812G tune status 0x%04x, times check %d\n",
                reg_value, check_times);
    }

chip_poweron_failed:
    return ret;
}


static int kt0812g_fm_close(struct kt0812g_drv_data *cxt)
{
    u16 reg_value = 0x0;
    int ret = -EINVAL;

//    if (!atomic_read(&cxt->fm_opened)) {
//        dev_err(&cxt->client->dev, "FM close: FM not open\n");
//        return ret;
//    }

    ret = kt0812g_register_read(cxt, KT0812G_REG_SYSCFG, &reg_value);
    if (ret < 0) {
        return ret;
    }

    if (reg_value & KT0812G_STANDBY_EN)
    {
	/*
        dev_info(&cxt->client->dev, "FM close: FM not power on\n");
	*/
        return ret;
    }


    reg_value |= KT0812G_STANDBY_EN;
    ret = kt0812g_register_write(cxt, KT0812G_REG_SYSCFG, reg_value);
    if (atomic_read(&cxt->fm_opened))
    {
        atomic_cmpxchg(&cxt->fm_opened, 1, 0);
    }

    /* turn off vdd */
    kt0812g_chip_vdd_input(cxt,false);

    dev_info(&cxt->client->dev, "FM close: kt0812G will run standby.\n");

    return ret;
}


/**
 * Notes: Before this call, device must be power on.
 **/
static int kt0812g_fm_open(struct kt0812g_drv_data *cxt)
{
    u16 reg_value = 0x0;
    int ret = -EINVAL;

    if (atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev,
            "FM open: already opened, ignore this operation\n");
        return ret;
    }

    ret = kt0812g_fm_power_on(cxt);
    if (ret < 0)
        return ret;

    // [0x2]= 0x2207: default SEEK_TH, 87--108MHz, 100Khz space
    reg_value = 0x2203 | KT0812G_CHAN_JAPAN_SPACE;
    kt0812g_register_write(cxt, KT0812G_REG_SEEK, reg_value);

    // [0x4]= 0xC027: MUTE_B Disable
    reg_value = 0x8227 | KT0812G_MUTE_B;
    kt0812g_register_write(cxt, KT0812G_REG_VOLUME, reg_value);

    // [0xa]= 0x0000: Turn On AFC
    reg_value = KT0812G_LO_AFCD_ENABLE;
    kt0812g_register_write(cxt, KT0812G_REG_LOCFGA, reg_value);

    // [0xf]= 0x8a40: wakeup, adv_seek(HW_SEEK)
    reg_value = 0x8a00 | KT0812G_SEEK_SEL;
    kt0812g_register_write(cxt, KT0812G_REG_SYSCFG, reg_value);

    atomic_cmpxchg(&cxt->fm_opened, 0, 1);

    dev_err(&cxt->client->dev, "FM open: FM is opened\n");

    return 0;
}



/**
 * Notes:Set FM tune, the frequency 100KHz unit.
 **/
static int kt0812g_fm_set_tune(struct kt0812g_drv_data *cxt, u16 frequency)
{
    u16  channel = 0;
    u16  reg_value = 0x0;
    int  ret = -EPERM;

    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Set tune: FM not open\n");
        return ret;
    }

    /** frequency transfer to channel number,
     * freq (MHz) = 50KHz X CHAN + 64MHz ==> freq (100KHz) = 0.5 * ch + 640
     * ch = (freq - 640)/0.5 ==>
     **/
    channel =  (frequency -640) * 2;
#if (KT0812G_DEBUG)
    dev_info(&cxt->client->dev, "Set tune: channel=%d, frequency=%d\n",
            channel, frequency);
#endif
    /* Read tune, if tune is enable, disable first  */
    kt0812g_register_read(cxt, KT0812G_REG_TUNE, &reg_value);
    if (reg_value & KT0812G_TUNE_ENABLE) {
        reg_value &= ~KT0812G_TUNE_ENABLE;
        kt0812g_register_write(cxt, KT0812G_REG_TUNE, reg_value);
    }

    /* Set tune and channel */
    reg_value &= ~KT0812G_TUNE_CHAN;
    reg_value |= channel;
    reg_value |= KT0812G_TUNE_ENABLE;
    kt0812g_register_write(cxt, KT0812G_REG_TUNE, reg_value);

    dev_info(&cxt->client->dev, "Set tune 0x%04x\n", reg_value);

    msleep(KT0812G_TUNE_DELAY);

    cxt->current_freq = frequency;
#if (KT0812G_DEBUG)
    {
        u16 tune, status, channel;

        kt0812g_register_read(cxt, KT0812G_REG_TUNE, &tune);
        kt0812g_register_read(cxt, KT0812G_REG_STATUSA, &status);
        kt0812g_register_read(cxt, KT0812G_REG_STATUSB, &channel);
        dev_info(&cxt->client->dev,
                "FMTune TUNE=0x%04x\n STATUSA=0x%04x CHANNAL=0x%04x\n",
                tune, status, channel);
    }
#endif

    return 0;
}


/*
 ** NOTES: Get tune frequency, with 100KHz unit.
 */
static int kt0812g_fm_get_frequency(struct kt0812g_drv_data *cxt)
{
    u16 reg_value = 0;
    u16 frequency = 0;
    int       ret = -EPERM;

    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Get frequency: FM not open\n");
        return ret;
    }

    kt0812g_register_read(cxt, KT0812G_REG_STATUSB, &reg_value);

    /* freq (MHz) = 50KHz X CHAN + 64MHz ==> freq (100KHz) = 0.5 * ch + 640 */
    frequency  =  ((reg_value & KT0812G_READ_CHAN) >> 1) + 640;

    dev_info(&cxt->client->dev, "Get frequency %d\n", frequency);

    return frequency;
}


/*
 * NOTES: Start searching process. Different "from kt0812g_fm_full_search",
 * this function just do seek, NOT read channel found. */
static int kt0812g_fm_do_seek(struct kt0812g_drv_data *cxt,
        u16 frequency,
        u8  seek_dir)
{
    u16 reg_value = 0x0;
    int  ret = -EPERM;

#if (KT0812G_DEBUG)
    dev_info(&cxt->client->dev,
            "%s, frequency %d, seekdir %d\n", __func__, frequency, seek_dir);
#endif
    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Do seek: FM not open\n");
        return ret;
    }

/*
    if (atomic_read(&cxt->fm_searching)) {
        dev_err(&cxt->client->dev, "Seeking is not stoped.%s\n", __func__);
        return -EBUSY;
    }
*/
    mutex_lock(&cxt->mutex);

    /* Set start frequency */
    kt0812g_fm_set_tune(cxt, frequency);

    /* If seek is on, stop it first */
    kt0812g_register_read(cxt, KT0812G_REG_SEEK, &reg_value);
    if (reg_value & KT0812G_SEEK_ENABLE) {
        reg_value &= ~ KT0812G_SEEK_ENABLE;
        kt0812g_register_write(cxt, KT0812G_REG_SEEK, reg_value);
    }

    /* Set seek direction */
    if (seek_dir == KT0812G_SEEK_DIR_UP) {
        reg_value |= KT0812G_SEEK_DIR;
    }
    else {
        reg_value &= ~KT0812G_SEEK_DIR;
    }

    /* Seeking start */
    reg_value |= KT0812G_SEEK_ENABLE;
    kt0812g_register_write(cxt, KT0812G_REG_SEEK, reg_value);

    mutex_unlock(&cxt->mutex);

    return 0;
}


/* NOTES:
 * Stop fm search and clear search status */
static int kt0812g_fm_stop_search(struct kt0812g_drv_data *cxt)
{
    int ret = -EPERM;
    u16 reg_value = 0x0;

    if (atomic_read(&cxt->fm_searching)) {

        atomic_cmpxchg(&cxt->fm_searching, 1, 0);

        /* clear seek enable bit of seek register. */
        kt0812g_register_read(cxt, KT0812G_REG_SEEK, &reg_value);
        if (reg_value & KT0812G_SEEK_ENABLE) {
            reg_value &= ~ KT0812G_SEEK_ENABLE;
            kt0812g_register_write(cxt, KT0812G_REG_SEEK, reg_value);
        }

#if (KT0812G_DEBUG)
        dev_info(&cxt->client->dev, "%s, search stopped", __func__);
#endif
        ret = 0;
    }

    return ret;
}


/* NOTES:
 * Search frequency from current frequency, if a channel is found, the
 * frequency will be read out.
 * This function is timeout call. If no channel is found when time out, a error
 * code will be given. The caller can retry search(skip "do seek")   to get
 * channel. */
static int kt0812g_fm_full_search(struct kt0812g_drv_data *cxt,
        u16  frequency,
        u8   seek_dir,
        u32  time_out,
        u16 *freq_found)
{
    int      ret               = -EPERM;
    u16      reg_value         = 0x0;
    ulong    jiffies_comp      = 0;
    u8       is_timeout;
    u8       is_search_end;

    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Full search: FM not open\n");
        return ret;
    }
/*
    if (frequency < 870 || frequency > 1080 || seek_dir > 1 || time_out < 500) {
#if (KT0812G_DEBUG)
        dev_info(&cxt->client->dev,
            "%s, invalid arguments: freq %d, dir %d, timeout %d",
            __func__, frequency, seek_dir, time_out);
#endif
        return ret;
    }
*/
    /* revese seek dir. Temporarily to fit the application. */
    seek_dir ^= 0x1;

    if (!atomic_read(&cxt->fm_searching)) {
        atomic_cmpxchg(&cxt->fm_searching, 0, 1);

        if (frequency == 0)
        {
            kt0812g_fm_do_seek(cxt, cxt->current_freq, seek_dir);
        }
        else
            kt0812g_fm_do_seek(cxt, frequency, seek_dir);
    }
    else
    {
#if (KT0812G_DEBUG)
        dev_info(&cxt->client->dev, "%s, busy searching!", __func__);
#endif
        return -EBUSY;
    }

    jiffies_comp = jiffies;
    do {
        /* search is stopped manually */
        if (atomic_read(&cxt->fm_searching) == 0)
            break;

        if (msleep_interruptible(KT0812G_SEEK_CHECK_TIME) ||
                signal_pending(current))
            break;

        kt0812g_register_read(cxt, KT0812G_REG_STATUSA, &reg_value);
        is_search_end = (KT0812G_SEEKTUNE_COMP ==
                (KT0812G_SEEKTUNE_COMP & reg_value));

        is_timeout = time_after(jiffies,
                jiffies_comp + msecs_to_jiffies(time_out));
    }while(is_search_end ==0 && is_timeout == 0);

    /* If search is not completed, need to re-search
       or stop seek (kt0812g_fm_stop_seek) */
    if (is_search_end) {
        if (KT0812G_SF_BL_FAIL == (KT0812G_SF_BL_FAIL & reg_value))
            ret = -EAGAIN;
        else
            ret = 0;
    }
    else {
        ret = -EAGAIN;
    }

    atomic_cmpxchg(&cxt->fm_searching, 1, 0);

    *freq_found = kt0812g_fm_get_frequency(cxt);
    cxt->current_freq = *freq_found;

#if (KT0812G_DEBUG)
    dev_info(&cxt->client->dev,
            "%s: ret %d, seek_dir %d, timeout %d, seek_end %d, freq %d\n",
            __func__, ret, seek_dir, is_timeout, is_search_end, *freq_found);
#endif
    return ret;
}


/* NOTES: Set fm output side volume
* control register: 0x4, volume control register. (3:0)
* 0000 --  MUTE
* 0001 -- -42dBFS
* 0010 -- -39dbFS
* 1111 --  FS */
static int kt0812g_fm_set_volume(struct kt0812g_drv_data *cxt, u8 volume)
{
    int ret       =  -EPERM;
    u16 reg_value =  0x0;

    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Set volume: FM not open\n");
        return ret;
    }

    if (volume > 15) {
        dev_err(&cxt->client->dev, "Invalid volume %d, set max\n", volume);
        volume = 15;
    }

    kt0812g_register_read(cxt, KT0812G_REG_VOLUME, &reg_value);
    reg_value &= ~KT0812G_VOLUME;
    reg_value |= volume;
    ret = kt0812g_register_write(cxt, KT0812G_REG_VOLUME, reg_value);
    if (ret == 0)
    {
        cxt->current_volume = volume;
    }

    return ret;
}


static int kt0812g_fm_get_volume(struct kt0812g_drv_data *cxt)
{
    u16 reg_value  = 0x0;

    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Get volume: FM not open\n");
        return -EPERM;
    }

    kt0812g_register_read(cxt, KT0812G_REG_VOLUME, &reg_value);
    reg_value &= KT0812G_VOLUME;

    return reg_value;
}


static int kt0812g_fm_misc_open(struct inode *inode, struct file *filep)
{
    int ret = -EINVAL;

    ret = nonseekable_open(inode, filep);
    if (ret < 0) {
        pr_err("kt0812g open misc device failed.\n");
        return ret;
    }

    filep->private_data = kt0812g_dev_data;

    return 0;
}


static int kt0812g_fm_misc_ioctl(struct file *filep,
        unsigned int cmd, unsigned long arg)
{
    void __user              *argp       = (void __user *)arg;
    int                       ret        = 0;
    int                       iarg       = 0;
    int                       buf[4]     = {0};
    struct kt0812g_drv_data  *dev_data   = filep->private_data;

	printk(KERN_ERR "kt0812g_fm_misc_ioctl:cmd=%x,enable=%x",cmd,KT0812G_FM_IOCTL_ENABLE);

    switch (cmd) {
        case KT0812G_FM_IOCTL_ENABLE:
            if (copy_from_user(&iarg, argp, sizeof(iarg)) || iarg > 1) {
                ret =  -EFAULT;
            }
            if (iarg ==1) {
                ret = kt0812g_fm_open(dev_data);
            }
            else {
                ret = kt0812g_fm_close(dev_data);
            }

            break;

        case KT0812G_FM_IOCTL_GET_ENABLE:
            iarg = atomic_read(&dev_data->fm_opened);
            if (copy_to_user(argp, &iarg, sizeof(iarg))) {
                ret = -EFAULT;
            }
            break;

        case KT0812g_FM_IOCTL_SET_TUNE:
            if (copy_from_user(&iarg, argp, sizeof(iarg))) {
                ret = -EFAULT;
            }
            ret = kt0812g_fm_set_tune(dev_data, iarg);
            break;

        case KT0812g_FM_IOCTL_GET_FREQ:
            iarg = kt0812g_fm_get_frequency(dev_data);
            if (copy_to_user(argp, &iarg, sizeof(iarg))) {
                ret = -EFAULT;
            }
            break;

        case KT0812G_FM_IOCTL_SEARCH:
            if (copy_from_user(buf, argp, sizeof(buf))) {
                ret = -EFAULT;
            }
            ret = kt0812g_fm_full_search(dev_data,
                    buf[0], /* start frequency */
                    buf[1], /* seek direction*/
                    buf[2], /* time out */
                    (u16*)&buf[3]);/* frequency found will be stored to */

            break;

        case KT0812G_FM_IOCTL_STOP_SEARCH:
            ret = kt0812g_fm_stop_search(dev_data);
            break;

        case KT0812G_FM_IOCTL_MUTE:
            break;

        case KT0812G_FM_IOCTL_SET_VOLUME:
            if (copy_from_user(&iarg, argp, sizeof(iarg))) {
                ret = -EFAULT;
            }
            ret = kt0812g_fm_set_volume(dev_data, (u8)iarg);
            break;

        case KT0812G_FM_IOCTL_GET_VOLUME:
            iarg = kt0812g_fm_get_volume(dev_data);
            if (copy_to_user(argp, &iarg, sizeof(iarg))) {
                ret = -EFAULT;
            }
            break;
	case KT0812G_FM_IOCTL_GET_STATUS:
            /* rssi */
            kt0812g_register_read(dev_data, KT0812G_REG_STATUSA, (u16*)&buf[0]);
            buf[0] &= KT0812G_RSSI;
            /* snr */
            kt0812g_register_read(dev_data, KT0812G_REG_SNR, (u16*)&buf[1]);
            buf[1] &= KT0812G_SNR;

            if (copy_to_user(argp, buf, sizeof(int[2]))) {
                ret = -EFAULT;
            }
            break;
        default:
	    printk(KERN_ERR "kt0812g_fm_misc_ioctl:cmd=%d",cmd);
            return -EINVAL;
    }

    return ret;
}


static const struct file_operations kt0812g_fm_misc_fops = {
    .owner = THIS_MODULE,
    .open  = kt0812g_fm_misc_open,
    .unlocked_ioctl = kt0812g_fm_misc_ioctl,
};

static struct miscdevice kt0812g_fm_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = KT0812G_DEV_NAME,
    .fops  = &kt0812g_fm_misc_fops,
};


/*------------------------------------------------------------------------------
 * KT0812 class attribute method
 ------------------------------------------------------------------------------*/
static ssize_t kt0812g_fm_attr_open(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
    u8 open;

    if (size) {
        open = simple_strtol(buf, NULL, 10);
        if (open)
            kt0812g_fm_open(kt0812g_dev_data);
        else
            kt0812g_fm_close(kt0812g_dev_data);
    }

    return size;
}


static ssize_t kt0812g_fm_attr_get_open(struct class *class, struct class_attribute *attr, char *buf)
{
    u8 opened;

    opened = atomic_read(&kt0812g_dev_data->fm_opened);
    if (opened)
        return sprintf(buf, "Opened\n");
    else
        return sprintf(buf, "Closed\n");
}


static ssize_t kt0812g_fm_attr_set_tune(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
    u16 frequency;

    if (size) {
        frequency = simple_strtol(buf, NULL, 10);/* decimal string to int */


        kt0812g_fm_set_tune(kt0812g_dev_data, frequency);
    }

    return size;
}

static ssize_t kt0812g_fm_attr_get_frequency(struct class *class, struct class_attribute *attr, char *buf)
{
    u16 frequency;

    frequency = kt0812g_fm_get_frequency(kt0812g_dev_data);

    return sprintf(buf, "Frequency %d\n", frequency);
}


static ssize_t kt0812g_fm_attr_search(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
    u32 timeout;
    u16 frequency;
    u8  seek_dir;
    u16 freq_found;
    char *p = (char*)buf;
    char *pi = NULL;

    if (size) {
        while (*p == ' ') p++;
        frequency = simple_strtol(p, &pi, 10); /* decimal string to int */
        if (pi == p) goto out;

        p = pi;
        while (*p == ' ') p++;
        seek_dir = simple_strtol(p, &pi, 10);
        if (pi == p) goto out;

        p = pi;
        while (*p == ' ') p++;
        timeout = simple_strtol(p, &pi, 10);
        if (pi == p) goto out;

        kt0812g_fm_full_search(kt0812g_dev_data, frequency, seek_dir, timeout, &freq_found);
    }

out:
    return size;
}


static ssize_t kt0812g_fm_attr_set_volume(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
    u8 volume;

    if (size) {
        volume = simple_strtol(buf, NULL, 10);/* decimal string to int */

        kt0812g_fm_set_volume(kt0812g_dev_data, volume);
    }

    return size;
}

static ssize_t kt0812g_fm_attr_get_volume(struct class *class, struct class_attribute *attr, char *buf)
{
    u8 volume;

    volume = kt0812g_fm_get_volume(kt0812g_dev_data);

    return sprintf(buf, "Volume %d\n", volume);
}

static struct class_attribute kt0812g_fm_attrs[] = {
    __ATTR(fm_open,   S_IRUSR|S_IWUSR, kt0812g_fm_attr_get_open,      kt0812g_fm_attr_open),
    __ATTR(fm_tune,   S_IRUSR|S_IWUSR, kt0812g_fm_attr_get_frequency, kt0812g_fm_attr_set_tune),
    __ATTR(fm_seek,   S_IWUSR,         NULL,                          kt0812g_fm_attr_search),
    __ATTR(fm_volume, S_IRUSR|S_IWUSR, kt0812g_fm_attr_get_volume,    kt0812g_fm_attr_set_volume),
    {},
};

void kt0812g_fm_sysfs_init(struct class *class)
{
    class->class_attrs = kt0812g_fm_attrs;
}


/*------------------------------------------------------------------------------
 * KT0812G i2c device driver.
 ------------------------------------------------------------------------------*/

static int __devexit kt0812g_remove(struct i2c_client *client)
{
    struct kt0812g_drv_data  *cxt = i2c_get_clientdata(client);

    kt0812g_fm_close(cxt);
    if(cxt->regu != NULL) {
        regulator_put(cxt->regu);
    }
    misc_deregister(&kt0812g_fm_misc_device);
    class_unregister(&cxt->fm_class);
    kfree(cxt);

    dev_info(&client->dev, "%s\n", __func__);

    return 0;
}


static int kt0812g_resume(struct i2c_client *client)
{
    struct kt0812g_drv_data *cxt = i2c_get_clientdata(client);

    dev_info(&cxt->client->dev, "%s, FM opened before suspend: %d\n",
        __func__, cxt->opened_before_suspend);

    if (cxt->opened_before_suspend) {
        cxt->opened_before_suspend = 0;

        kt0812g_fm_open(cxt);

        kt0812g_fm_set_volume(cxt, cxt->current_volume);
    }

    if (atomic_read(&cxt->fm_opened) == 0) {
    /* The chip must go into stand-by mode. */
        mutex_lock(&cxt->mutex);

        kt0812g_fm_open(cxt);

        kt0812g_fm_close(cxt);

        mutex_unlock(&cxt->mutex);
    }

    return 0;
}


static int kt0812g_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct kt0812g_drv_data *cxt = i2c_get_clientdata(client);

    dev_info(&cxt->client->dev, "%s, FM opend: %d\n",
        __func__, atomic_read(&cxt->fm_opened));

    if (atomic_read(&cxt->fm_opened) && cxt->bg_play_enable == 0) {
        cxt->opened_before_suspend = 1;
        kt0812g_fm_close(cxt);
    }

    if (atomic_read(&cxt->fm_opened) == 0) {
    /******************************************************************************/
    /* FIX FM power leakage problem on sp6820 and 8810.
       LDO SIM2 shoule be cut off here. Turn on the LDO when fm open
    */
    /******************************************************************************/
    }

    return 0;
}


#ifdef CONFIG_HAS_EARLYSUSPEND

static void kt0812g_early_resume (struct early_suspend* es)
{
#ifdef KT0812G_DEBUG
    printk("%s.\n", __func__);
#endif
    if (kt0812g_dev_data) {
        kt0812g_resume(kt0812g_dev_data->client);
    }
}

static void kt0812g_early_suspend (struct early_suspend* es)
{
#ifdef KT0812G_DEBUG
    printk("%s.\n", __func__);
#endif
    if (kt0812g_dev_data) {
        kt0812g_suspend(kt0812g_dev_data->client, (pm_message_t){.event=0});
    }
}

#endif /* CONFIG_HAS_EARLYSUSPEND */


static int kt0812g_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    u16    reg_value = 0x0;
    int    ret = -EINVAL;

    struct kt0812g_drv_data *cxt = NULL;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev, "kt0812g driver: client is not i2c capable.\n");
        ret = -ENODEV;
        goto i2c_functioality_failed;
    }

    cxt = kzalloc(sizeof(struct kt0812g_drv_data), GFP_KERNEL);
    if (cxt == NULL) {
        dev_err(&client->dev, "Can't alloc memory for module data.\n");
        ret = -ENOMEM;
        goto alloc_data_failed;
    }

    cxt->regu =regulator_get(&client->dev,REGU_NAME_FM);

    mutex_init(&cxt->mutex);
    mutex_lock(&cxt->mutex);

    cxt->client = client;
    i2c_set_clientdata(client, cxt);

    atomic_set(&cxt->fm_searching, 0);
    atomic_set(&cxt->fm_opened, 0);
    ret = kt0812g_fm_power_on(cxt);

    if (ret < 0) {
        goto poweron_failed;
    }

    kt0812g_check_chip_id(cxt, &reg_value);

#if (KT0812G_DEBUG)
//    kt0812g_read_all_registers(cxt);
#endif

    kt0812g_fm_close(cxt);

    cxt->fm_class.owner = THIS_MODULE;
    cxt->fm_class.name = "fm_class";
    kt0812g_fm_sysfs_init(&cxt->fm_class);
    ret = class_register(&cxt->fm_class);
    if (ret < 0) {
        dev_err(&client->dev, "kt0812g class init failed.\n");
        goto class_init_failed;
    }

    ret = misc_register(&kt0812g_fm_misc_device);
    if (ret < 0) {
        dev_err(&client->dev, "kt0812g misc device register failed.\n");
        goto misc_register_failed;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    cxt->early_suspend.suspend = kt0812g_early_suspend;
    cxt->early_suspend.resume  = kt0812g_early_resume;
    cxt->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    register_early_suspend(&cxt->early_suspend);
#endif
    cxt->opened_before_suspend = 0;
    cxt->bg_play_enable = 1;
    cxt->current_freq = 870; /* init current frequency, search may use it. */

    kt0812g_dev_data = cxt;

    mutex_unlock(&cxt->mutex);

    return ret;
misc_register_failed:
    misc_deregister(&kt0812g_fm_misc_device);
class_init_failed:
    class_unregister(&cxt->fm_class);
poweron_failed:
    mutex_unlock(&cxt->mutex);
    if(cxt->regu != NULL) {
        regulator_put(cxt->regu);
    }
    kfree(cxt);
alloc_data_failed:
i2c_functioality_failed:
    dev_err(&client->dev,"kt0812g driver init failed.\n");
    return ret;
}


static const struct i2c_device_id kt0812g_i2c_id[] = {
    { KT0812G_I2C_NAME, 0 },
    { },
};

MODULE_DEVICE_TABLE(i2c, kt0812g_i2c_id);

static struct i2c_driver kt0812g_i2c_driver = {
    .driver = {
        .name = KT0812G_I2C_NAME,
    },
    .probe    =  kt0812g_probe,
    .remove   =  __devexit_p(kt0812g_remove),
    //replaced by early suspend and resume.
    //.resume   =  kt0812g_resume,
    //.suspend  =  kt0812g_suspend,
    .id_table =  kt0812g_i2c_id,
};


static int __init kt0812g_driver_init(void)
{
    int  ret = 0;

    pr_debug("KT0812G driver: init\n");

    return i2c_add_driver(&kt0812g_i2c_driver);

init_err:
    return ret;
}

static void __exit kt0812g_driver_exit(void)
{
#ifdef DEBUG
    pr_debug("KT0812G driver exit\n");
#endif

    i2c_del_driver(&kt0812g_i2c_driver);
    return;
}

module_init(kt0812g_driver_init);
module_exit(kt0812g_driver_exit);

MODULE_DESCRIPTION("KT0812G FM radio driver");
MODULE_AUTHOR("Spreadtrum Inc.");

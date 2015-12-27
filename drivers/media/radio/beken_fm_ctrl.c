/*******************************************************************************
 * Source file : beken_fm_ctl.c
 * Description : beken FM Receiver driver for linux.
 * Date        : 11/27/2012
 *
 * Copyright (C) 2012 Bekencorp
 *
 ********************************************************************************
 * Revison
 2012-11-27  LFBAO   initial version
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
#include "beken_fm_ctrl.h"

#ifdef CONFIG_ARCH_SC8825 
#include <mach/pinmap.h>
#include <mach/sci.h>
#include <mach/hardware.h>
#include <mach/regs_glb.h>
#include <linux/clk.h>
#endif

#define BEKEN_DEBUG 1

#define I2C_RETRY_DELAY                   5
#define I2C_RETRIES                       3

#define BEKEN_DEV_NAME	"BEKEN_FM"
#define BEKEN_I2C_NAME    BEKEN_DEV_NAME

#ifdef CONFIG_ARCH_SC8825
struct clk *fm_clk;
#endif

struct beken_drv_data {
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

static u16 BK1080_Digital_Reg[] = {
0x0008,	//REG0
0x1080,	//REG1
0x4201,	//REG2
0x0000,	//REG3
0x40C0,	//REG4
0x081F,	//REG5
0x002E,	//REG6
0x02FF,	//REG7
0x5B11,	//REG8
0x0000,	//REG9
0x411E,	//REG10
0x0000,	//REG11
0xCE00,	//REG12
0x0000,	//REG13
0x0000,	//REG14
0x1000,	//REG15
0x0010,	//REG16
0x0000,	//REG17
0x13FF,	//REG18
0x9852,	//REG19
0x0000,	//REG20
0x0000,	//REG21
0x0008,	//REG22
0x0000,	//REG23
0x51E1,	//REG24
0x38BC,	//REG25
0x2645,	//REG26
0x00E4,	//REG27
0x1CD8,	//REG28
0x3A50,	//REG29
0xEAF0,	//REG30
0x3000,	//REG31
0x00B8,	//REG32
0x0000,	//REG33
};

struct beken_drv_data *beken_dev_data = NULL;

/***
 * Common i2c read and write function based i2c_transfer()
 *  The read operation sequence:
 *  7 bit chip address and Write command ("0") -> 8 bit
 *  register address n -> 7 bit chip address and Read command("1")
 *  The write operation sequence:
 * 7 bit chip address and Write command ("0") -> 8 bit
 * register address n -> write data n [15:8] -> write data n [7:0]
 ***/
static int bk1080_i2c_read(struct beken_drv_data *cxt, u8 * buf, int len)
{
    int err = 0;
    int tries = 0;

    struct i2c_msg msgs[] = {
        {
            .addr = cxt->client->addr,
            .flags =  I2C_M_RD,
            .len = len,
            .buf = buf,
        },
    };
	//dev_err(&cxt->client->dev, "read %02x,%02x,%02x\n",.addr);
    do {
        err = i2c_transfer(cxt->client->adapter, msgs, 1);
        if (err != 1)
            msleep_interruptible(I2C_RETRY_DELAY);
    } while ((err != 1) && (++tries < I2C_RETRIES));

    if (err != 1) {
        dev_err(&cxt->client->dev, "Read transfer error\n");
        err = -EIO;
    } else {
        err = 0;
    }

    return err;
}


static int bk1080_i2c_write(struct beken_drv_data *cxt, u8 * buf, int len)
{
    int err = 0;
    int tries = 0;

    struct i2c_msg msgs[] = { 
        { 
            .addr = cxt->client->addr,
            .flags = 0,
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

static int bk1080_register_read(struct beken_drv_data *cxt, 
        u8 reg_address, 
        u8 *data_buf,
	u8 len) 
{
    int  ret = -EINVAL;
    u8   buf[70] = {0};
    u8 i;
	//dev_err(&cxt->client->dev, "bk1080 i2c read\n");
    //buf[0] = (reg_address<<1)|0x1;
    ret = bk1080_i2c_read(cxt, buf, len);
//    if (ret >= 0) {
//        *value = (buf[0]<<8)|(buf[1]); //MSB first
//    }
    if (ret>=0)
    {
	    for (i=0;i<len;i++)
	    {
		data_buf[i]=buf[i];
	    }
    }
    return ret;
}

static int bk1080_register_write(struct beken_drv_data *cxt, 
        u8 reg_address,
        u8 *data_buf,
	u8 len)
{
    int  ret = -EINVAL;
    u8   buf[70] = {0};
    u8 i;
	//dev_err(&cxt->client->dev, "bk1080 i2c write\n");
    buf[0] = reg_address;
    for (i=0;i<len;i++)
    {
	buf[i+1]=data_buf[i];
    }
    //buf[1] = value >> 8; // MSB first
    //buf[2] = value & 0xff;
    ret = bk1080_i2c_write(cxt, buf, len+1);

    return ret;    
}

static int beken_check_chip_id(struct beken_drv_data *cxt, u16 *chip_id) 
{
    int ret = 0;
    u8 temp_data[4]={0};

	dev_err(&cxt->client->dev, "Read chip id\n");
	temp_data[0]=0x00;
	temp_data[1]=0x01;
	ret = bk1080_register_write(cxt, 0x7e, temp_data,2);
	ret = bk1080_register_read(cxt, 0x01, temp_data,2);

	*chip_id = (temp_data[0]<<8)+temp_data[1];
	dev_info(&cxt->client->dev, "BEKEN chip id:0x%04x\n", *chip_id);

	if((*chip_id)==0x1080)
	{
		temp_data[0]=0x00;
		temp_data[1]=0x01;
		ret = bk1080_register_write(cxt, 0x7d, temp_data,2);	//disable 0x80 i2c address
	}

    if (ret < 0) {
        dev_err(&cxt->client->dev, "Read chip id failed.\n");
    }
    else {
        dev_info(&cxt->client->dev, "BEKEN chip id:0x%04x\n", *chip_id);
    }

    return ret;
}


static void beken_chip_vdd_input(struct beken_drv_data *cxt,bool turn_on)
{
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
static int beken_fm_power_on(struct beken_drv_data *cxt)
{
    int     ret              = -EINVAL;

    dev_info(&cxt->client->dev, "%s\n", __func__);

    /* turn on vdd */
    beken_chip_vdd_input(cxt,true);

	ret = 0;

    return ret;
}


static int beken_fm_close(struct beken_drv_data *cxt)
{
    int ret = -EINVAL;
    u8 TmpData8[2];

    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "FM close: FM not open\n");
        return ret;
    }

/////////////////for bk1080/////////////////////
    TmpData8[0]=0x00;
    TmpData8[1]=0x02;
    ret = bk1080_register_write(cxt, 0x7e, TmpData8,2);
    ret = bk1080_register_read(cxt, 0x02, TmpData8,2);
    TmpData8[1]=TmpData8[1]|0x41;
    ret = bk1080_register_write(cxt, 0x2, TmpData8,2);

    /* turn off vdd */
    //beken_chip_vdd_input(cxt,false);

    if (atomic_read(&cxt->fm_opened))
    {
        atomic_cmpxchg(&cxt->fm_opened, 1, 0);
    }

#ifdef CONFIG_ARCH_SC8825   
    clk_disable(fm_clk);//alvindebug
#endif 
    //beken_chip_32k_clk_input(false); 
    dev_info(&cxt->client->dev, "FM close: beken will run standby.\n");

    return ret;
}


/**
 * Notes: Before this call, device must be power on.
 **/
static int beken_fm_open(struct beken_drv_data *cxt)
{
    //u16 reg_value = 0x0;
    int ret = -EINVAL;
    u8 writeData8[68],xbTemp;	
	dev_err(&cxt->client->dev, "FM chip open!!!!\n");
#if 1
    if (atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, 
            "FM open: already opened, ignore this operation\n");
        return ret;
    }
#endif

	dev_err(&cxt->client->dev, "FM open: bk1080_init!!!!\n");

    	writeData8[0]=0x00;
	writeData8[1]=0x01;
	ret = bk1080_register_write(cxt, 0x7e, writeData8,2);
	ret = bk1080_register_read(cxt, 0x01, writeData8,2);
	//*chip_id = (temp_data[0]<<8)+temp_data[1];
	dev_info(&cxt->client->dev, "BK1080 id:0x%02x%02x\n", writeData8[0],writeData8[1]);

	for(xbTemp = 0; xbTemp < 34; xbTemp++)//write reg2-reg31 ;reg0->device_id,reg1->chip_id only read ...
	{
		writeData8[xbTemp*2] = (BK1080_Digital_Reg[xbTemp] >> 8)&0xff;
		writeData8[xbTemp*2+1] = (BK1080_Digital_Reg[xbTemp])&0xff;
	}
	ret = bk1080_register_write(cxt, 0, writeData8,68);
	msleep(300);

#if 1
	writeData8[25*2+1] = ((BK1080_Digital_Reg[25])&0x7f);
	ret = bk1080_register_write(cxt, 25, &(writeData8[25*2]),2);

	writeData8[25*2+1] = (BK1080_Digital_Reg[25])&0xff;
	ret = bk1080_register_write(cxt, 25, &(writeData8[25*2]),2);
#endif
	msleep(80);

    atomic_cmpxchg(&cxt->fm_opened, 0, 1);
    
#ifdef CONFIG_ARCH_SC8825    
    clk_enable(fm_clk); //alvindebug
#endif
    dev_err(&cxt->client->dev, "FM open: FM is opened\n");

    return 0;
	
}



/**
 * Notes:Set FM tune, the frequency 100KHz unit.
 **/
static int beken_fm_set_tune(struct beken_drv_data *cxt, u16 frequency)
{
    u16  channel = 0;
    //u16  reg_value = 0x0;
    int  ret = -EPERM;
    u8 writeData8[2];
    u16 curChan = 0;

	dev_info(&cxt->client->dev, "set_tune %d\n", frequency);
    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Set tune:FM not open\n");
        return ret;
    }

#if (BEKEN_DEBUG)    
    dev_info(&cxt->client->dev, "Set tune: channel=%d, frequency=%d\n",
            channel, frequency);
#endif
//bk1080
	printk("beken_fm_set_tune freg=%d\n",frequency);
	curChan = (frequency - 875);
	writeData8[0]=(curChan>>8)&0xff;
	writeData8[1]=curChan&0xff;
	ret = bk1080_register_write(cxt, 3, writeData8,2);
//	kal_sleep_task(6);
	writeData8[0] |= 0x80;//start tune
	ret = bk1080_register_write(cxt, 3, writeData8,2);

	writeData8[0] &= 0x7f;//stop tune
	ret = bk1080_register_write(cxt, 3, writeData8,2);
    return 0;
}


/*
 ** NOTES: Get tune frequency, with 100KHz unit.
 */
static int beken_fm_get_frequency(struct beken_drv_data *cxt)
{
    u16 frequency = 0;
    int       ret = -EPERM;
    u8 readData8[2];

    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Get frequency: FM not open\n");
        return ret;
    }

////////////bk1080//////////////
	readData8[0]=0x00;
	readData8[1]=0x0B;
	ret = bk1080_register_write(cxt, 0x7e, readData8,2);
	ret = bk1080_register_read(cxt, 11, readData8,2);
	frequency=(readData8[0]<<8)+readData8[1]+875;
	printk("beken_fm_get_tune freg=%d\n",frequency);
	return frequency;
}

static int beken_fm_set_mute(struct beken_drv_data *cxt, u16 mute)
{
    int  ret = -EPERM;
    u8 tempData8[2];

	tempData8[0]=0x00;
	tempData8[1]=0x02;
	ret = bk1080_register_write(cxt, 0x7e, tempData8,2);
	bk1080_register_read(cxt, 0x2, tempData8, 2);
	if(mute==1)
	{
		tempData8[0] |=0x40;
	}
        else
	{
		tempData8[0] &=0xbf;
	}
	bk1080_register_write(cxt, 0x2, tempData8, 2);
	
	return 0;
}
#if 0
/*
 * NOTES: Start searching process. Different "from beken_fm_full_search",
 * this function just do seek, NOT read channel found. */
static int beken_fm_do_seek(struct beken_drv_data *cxt,
        u16 frequency,
        u8  seek_dir)
{
    int  ret = -EPERM;
    u8 tempData8[2];
	printk("beken_fm_do_seek\n");
//seek_dir=1;
#if (BEKEN_DEBUG)
    dev_info(&cxt->client->dev, 
            "%s, frequency %d, seekdir %d\n", __func__, frequency, seek_dir);
#endif    
    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Do seek: FM not open\n");
        return ret;
    }

#if 0
    if (atomic_read(&cxt->fm_searching)) {
        dev_err(&cxt->client->dev, "Seeking is not stoped.%s\n", __func__);
        return -EBUSY;
    }
#endif
    mutex_lock(&cxt->mutex);    

    /* Set start frequency */
    beken_fm_set_tune(cxt, frequency);

///bk1080
	/* If seek is on, stop it first */
	tempData8[0]=0x00;
	tempData8[1]=0x02;
	ret = bk1080_register_write(cxt, 0x7e, tempData8,2);
	bk1080_register_read(cxt, 0x2, tempData8, 2);
	if (tempData8[0] & 0x1) 
	{
		tempData8[0] &=0xfe;
		bk1080_register_write(cxt, 0x2, tempData8, 2);
	}

	/* Set seek direction */
	if (seek_dir == BEKEN_SEEK_DIR_UP) 
	{
		tempData8[0] |=0x02;
	}
	else 
	{
		tempData8[0] &=0xfd;
	}

	/* Seeking start */
	tempData8[0] |=0x01;
	bk1080_register_write(cxt, 0x2, tempData8, 2);
    mutex_unlock(&cxt->mutex);

    return 0;
}

#endif

/* NOTES:
 * Stop fm search and clear search status */
static int beken_fm_stop_search(struct beken_drv_data *cxt)
{
    int ret = -EPERM;
	u8 tempData8[2];
	printk("beken_fm_stop_search\n");
    if (atomic_read(&cxt->fm_searching)) {
        
        atomic_cmpxchg(&cxt->fm_searching, 1, 0);

///bk1080
        /* clear seek enable bit of seek register. */
	tempData8[0]=0x00;
	tempData8[1]=0x02;
	ret = bk1080_register_write(cxt, 0x7e, tempData8,2);
	bk1080_register_read(cxt, 0x2, tempData8, 2);
        if (tempData8[0] & 0x1) 
	{
		tempData8[0] &=0xfe;
		bk1080_register_write(cxt, 0x2, tempData8, 2);
        }


#if (BEKEN_DEBUG)        
        dev_info(&cxt->client->dev, "%s, search stopped", __func__);
#endif        
        ret = 0;
    }

    return ret;
}

u16 bk1080_ChanToFreq(struct beken_drv_data *cxt,u16 channel)
{
	int ret = -EPERM;
	u16 channelSpacing = 1;
	u16 bottomOfBand =0;
	u16 frequency;
	u8 readData8[2];
	
	readData8[0]=0x00;
	readData8[1]=0x05;
	ret = bk1080_register_write(cxt, 0x7e, readData8,2);
	ret = bk1080_register_read(cxt, 5, readData8,2);
	if (( readData8[1] & 0xc0) == 0x00)
		bottomOfBand = 875;
	else if (( readData8[1] & 0xc0) == 0x40)
		bottomOfBand = 760;
	else if (( readData8[1] & 0xc0) == 0x80)
		bottomOfBand = 760;

	if (( readData8[1] & 0x30) == 0x00)
		channelSpacing = 2;
	else if (( readData8[1] & 0x30) == 0x10)
		channelSpacing = 1;

	frequency = (bottomOfBand + channelSpacing * channel);
	return (frequency);
}

#if 0

u16 bk1080_FreqToChan(struct beken_drv_data *cxt,u16 frequency) 
{
	int ret = -EPERM;
	u16 channelSpacing = 1;
	u16 bottomOfBand;
	u16 channel;
	u8 readData8[2];

	readData8[0]=0x00;
	readData8[1]=0x05;
	ret = bk1080_register_write(cxt, 0x7e, readData8,2);
	ret = bk1080_register_read(cxt, 5, readData8,2);

	if (( readData8[1] & 0xc0) == 0x00)
		bottomOfBand = 875;
	else if (( readData8[1] & 0xc0) == 0x40)
		bottomOfBand = 760;
	else if (( readData8[1] & 0xc0) == 0x80)
		bottomOfBand = 760;

	if (( readData8[1] & 0x30) == 0x00)
		channelSpacing = 2;
	else if (( readData8[1] & 0x30) == 0x10)
		channelSpacing = 1;

	channel = (frequency - bottomOfBand) / channelSpacing;
	return (channel);
}

#endif

static u8 bk1080_Seek(struct beken_drv_data *cxt,u8 seekDirection)
{
	int ret = -EPERM;
	u8 tempdata[4];
	u8 fTemp=0;
	u8 i=0;
	u8 icount=0;
	dev_info(&cxt->client->dev, "bk1080_Seek %d\n", seekDirection);
	beken_fm_set_mute(cxt, 1); //mute
	tempdata[0]=0x00;
	tempdata[1]=0x0B;
	ret = bk1080_register_write(cxt, 0x7e, tempdata,2);
	if(bk1080_register_read(cxt,0x0B,tempdata,2)!=0) return(0);	
	dev_info(&cxt->client->dev, "chan=%d\n", (tempdata[1]|((tempdata[0]&0x03) <<8 ) ));
	tempdata[0]=0x00;
	tempdata[1]=0x03;
	ret = bk1080_register_write(cxt, 0x7e, tempdata,2);
	if(bk1080_register_read(cxt,0x03,tempdata,2)!=0) return(0);	
	dev_info(&cxt->client->dev, "reg3=%02x%02x\n", tempdata[0],tempdata[1]);
	while(1)
	{
		tempdata[0]=0x00;
		tempdata[1]=0x02;
		ret = bk1080_register_write(cxt, 0x7e, tempdata,2);
		ret = bk1080_register_read(cxt, 2, tempdata,2);
		//set seek bit		
		if(seekDirection == 0) 
		{
			tempdata[0] &=0xfd;
			tempdata[0] |=0x01;
		}
		else 
			tempdata[0] |=0x03;
			
	dev_info(&cxt->client->dev, "REG2=%02x%02x\n", tempdata[0],tempdata[1]);
  	if(bk1080_register_write(cxt,0x2,tempdata,2)!=0) return(0);  	
  	//msleep(10);
	icount=0;
	//wait for STC
	do
	{
		if (!atomic_read(&cxt->fm_opened))
		{
			dev_err(&cxt->client->dev, "bk1080_Seek: FM not open\n");
			return 0;
		}
		/* search is stopped manually */
		if (atomic_read(&cxt->fm_searching) == 0)
			break;

		msleep(10);
	    //read REG0A&0B
		tempdata[0]=0x00;
		tempdata[1]=0x0A;
		ret = bk1080_register_write(cxt, 0x7e, tempdata,2);
	    if(bk1080_register_read(cxt,0x0A,tempdata,4)!=0) return(0);	
		dev_info(&cxt->client->dev, "REGA=%02x%02x\n", tempdata[0],tempdata[1]);
#if 0
		if((tempdata[0]==0x00)&&(tempdata[1]==0x00))
			icount++;
		else
			icount=0;
#endif
	}while(((tempdata[0]&0x40)==0)/*&&(icount<100)*/);

	 if (((tempdata[0])&0x20)==0)
	  {	
			printk("This channel is true fTemp %d", fTemp);
			fTemp=1;
			dev_info(&cxt->client->dev, "REGB=%02x%02x\n", tempdata[2],tempdata[3]);                                                          
			//prevent < s_fm_freq_range.min_freq (RDA5802 RF_Band 870~1080)
			if( bk1080_ChanToFreq( cxt,(tempdata[3]|((tempdata[2]&0x03) <<8 ) )) < 875)
				fTemp=0;
			//Skip 960
			if( bk1080_ChanToFreq( cxt,(tempdata[3]|((tempdata[2]&0x03) <<8 )) ) == 960)
				fTemp=0;			
		}

		tempdata[0]=0x00;
		tempdata[1]=0x02;
		ret = bk1080_register_write(cxt, 0x7e, tempdata,2);
		ret = bk1080_register_read(cxt, 2, tempdata,2);
		tempdata[0] &= 0xfe;
		dev_info(&cxt->client->dev, "REG2=%02x%02x\n", tempdata[0],tempdata[1]);
		ret = bk1080_register_write(cxt, 2, tempdata,2);
		dev_info(&cxt->client->dev, "bk1080_Seek end fTemp=%d\n",fTemp);
	
		if	(fTemp) 
			break;
		else
		{
			dev_info(&cxt->client->dev, "bk1080_Seek end icount=%d\n",i);
			i++;
			if (i>2)
				break;
		}
	}		
	beken_fm_set_mute(cxt, 0); //dismute
	if(!fTemp) return(0);	 	
  	else 	  return(1);    	
}

/* NOTES:
 * Search frequency from current frequency, if a channel is found, the
 * frequency will be read out.
 * This function is timeout call. If no channel is found when time out, a error
 * code will be given. The caller can retry search(skip "do seek")   to get
 * channel. */
static int beken_fm_full_search(struct beken_drv_data *cxt,
        u16  frequency,
        u8   seek_dir,
        u32  time_out,
        u16 *freq_found)
{
    int      ret               = -EPERM;
    u8       fTemp = 0;
	printk("beken_fm_full_search freq=%d\n",frequency);
    if (!atomic_read(&cxt->fm_opened)) {                 
        dev_err(&cxt->client->dev, "Full search: FM not open\n");
        return ret;
    }
    if (frequency < 870 || frequency > 1080 || seek_dir > 1 ) {
#if (BEKEN_DEBUG)
        dev_info(&cxt->client->dev, 
            "%s, invalid arguments: freq %d, dir %d, timeout %d",
            __func__, frequency, seek_dir, time_out);
#endif        
        return ret;
    }
    if (!atomic_read(&cxt->fm_searching)) 
    {
        atomic_cmpxchg(&cxt->fm_searching, 0, 1);
	    if(frequency!=0)
	    {
			beken_fm_set_tune(cxt,  frequency );
	    }
	    else
	    {
			beken_fm_set_tune(cxt,  cxt->current_freq );
	    }

		if(seek_dir)
		{
			fTemp=bk1080_Seek(cxt,1);
			dev_err(&cxt->client->dev, "11 Full search: ftemp %d\n",fTemp);
		}
		else
		{
			fTemp=bk1080_Seek(cxt,0);
			dev_err(&cxt->client->dev, "22 Full search: ftemp %d\n",fTemp);
		}
    }
    else
    {
#if (BEKEN_DEBUG)
        dev_info(&cxt->client->dev, "%s, busy searching!", __func__);
#endif
        return -EBUSY;
    }

  if (!fTemp)
            ret = -EAGAIN;
        else
            ret = 0;

	atomic_cmpxchg(&cxt->fm_searching, 1, 0);


    *freq_found = beken_fm_get_frequency(cxt);
    cxt->current_freq = *freq_found;
   
#if 0//(BEKEN_DEBUG)
    dev_info(&cxt->client->dev, 
            "%s: ret %d, seek_dir %d, timeout %d, seek_end %d, freq %d\n",
            __func__, ret, seek_dir, is_timeout, is_search_end, *freq_found);
#endif
	dev_info(&cxt->client->dev, "full search ret=%d\n",ret);
    return ret;
}

static int beken_fm_set_volume(struct beken_drv_data *cxt, u8 volume)
{
    int ret       =  -EPERM;

    u8 tempdata8[2];

    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Set volume: FM not open\n");
        return ret;
    }
   dev_err(&cxt->client->dev, "Set volume: %d\n",volume);
    if (volume > 15) {
        dev_err(&cxt->client->dev, "Invalid volume %d, set max\n", volume);
        volume = 15;
    }
	if (atomic_read(&cxt->fm_searching) == 0)
	{
	////////////////bk1080///////////////
		printk("beken_fm_set_volume=%d\n",volume);
		dev_err(&cxt->client->dev, "Set volume: volume %d\n",volume);

		if(volume==0)
		{
			beken_fm_set_mute(cxt, 1); //mute
		}
		else
		{
			beken_fm_set_mute(cxt, 0); //dismute
		}

		tempdata8[0]=0x00;
		tempdata8[1]=0x05;
		ret = bk1080_register_write(cxt, 0x7e, tempdata8,2);
		//I2C_ReadCmdArr_BK1080(BK1080_I2C_ADDR, 5, writeData8, 2, BK1080_I2C_ACK);
		ret = bk1080_register_read(cxt,0x05,tempdata8,2);

		tempdata8[1] &= 0xf0;
		tempdata8[1] |= volume&0x0f;

		//I2C_WriteCmdArr_BK1080(BK1080_I2C_ADDR, 5, &writeData8[0], 2, BK1080_I2C_ACK);
		ret = bk1080_register_write(cxt,0x05,tempdata8,2);
	}
	else
	{
		ret=0;
	}

    if (ret == 0)
    {
        cxt->current_volume = volume;
    }

    return ret;
}


static int beken_fm_get_volume(struct beken_drv_data *cxt)
{
    u16 reg_value  = 0x0;
    u8 tempdata8[2];

    if (!atomic_read(&cxt->fm_opened)) {
        dev_err(&cxt->client->dev, "Get volume: FM not open\n");
        return -EPERM;
    }
//////////bk1080//////////////
	tempdata8[0]=0x00;
	tempdata8[1]=0x05;
	bk1080_register_write(cxt, 0x7e, tempdata8,2);
	bk1080_register_read(cxt,0x05,tempdata8,2);
	reg_value = tempdata8[1]&0xf;
	dev_err(&cxt->client->dev, "Get volume: number is %d\n",reg_value);
	printk("beken_fm_get_volume=%d\n",reg_value);
    return reg_value;
}


static int beken_fm_misc_open(struct inode *inode, struct file *filep)
{
    int ret = -EINVAL;

    ret = nonseekable_open(inode, filep);
    if (ret < 0) {
        pr_err("beken open misc device failed.\n");
        return ret;
    }

    filep->private_data = beken_dev_data;

    return 0;
}


static long beken_fm_misc_ioctl(struct file *filep,
        unsigned int cmd, unsigned long arg)
{
    void __user              *argp       = (void __user *)arg;
    int                       ret        = 0;
    int                       iarg       = 0;
    int                       buf[4]     = {0};
    u8 tempdata8[8];
	
    struct beken_drv_data  *dev_data   = filep->private_data;

	printk(KERN_ERR "beken_fm_misc_ioctl:cmd=%x,enable=%x\n",cmd,BEKEN_FM_IOCTL_ENABLE);

    switch (cmd) {
        case BEKEN_FM_IOCTL_ENABLE:
            if (copy_from_user(&iarg, argp, sizeof(iarg)) || iarg > 1) {
                ret =  -EFAULT;
            }
            if (iarg ==1) {
                ret = beken_fm_open(dev_data);
            }
            else {
                ret = beken_fm_close(dev_data);
            }

            break;

        case BEKEN_FM_IOCTL_GET_ENABLE:
            iarg = atomic_read(&dev_data->fm_opened);
            if (copy_to_user(argp, &iarg, sizeof(iarg))) {
                ret = -EFAULT;
            }
            break;

        case BEKEN_FM_IOCTL_SET_TUNE:
            if (copy_from_user(&iarg, argp, sizeof(iarg))) {
                ret = -EFAULT;
            }
            ret = beken_fm_set_tune(dev_data, iarg);
            break;

        case BEKEN_FM_IOCTL_GET_FREQ:
            iarg = beken_fm_get_frequency(dev_data);
            if (copy_to_user(argp, &iarg, sizeof(iarg))) {
                ret = -EFAULT;
            }
            break;

        case BEKEN_FM_IOCTL_SEARCH:
            if (copy_from_user(buf, argp, sizeof(buf))) {
                ret = -EFAULT;
            }
            ret = beken_fm_full_search(dev_data,
                    buf[0], /* start frequency */
                    buf[1], /* seek direction*/
                    buf[2], /* time out */
                    (u16*)&buf[3]);/* frequency found will be stored to */

            break;

        case BEKEN_FM_IOCTL_STOP_SEARCH:
            ret = beken_fm_stop_search(dev_data);
            break;

        case BEKEN_FM_IOCTL_MUTE:
            break;

        case BEKEN_FM_IOCTL_SET_VOLUME:
            if (copy_from_user(&iarg, argp, sizeof(iarg))) {
                ret = -EFAULT;
            }
            ret = beken_fm_set_volume(dev_data, (u8)iarg);
            break;

        case BEKEN_FM_IOCTL_GET_VOLUME:
            iarg = beken_fm_get_volume(dev_data);
            if (copy_to_user(argp, &iarg, sizeof(iarg))) {
                ret = -EFAULT;
            }
            break;
	case BEKEN_FM_IOCTL_GET_STATUS:
		tempdata8[0]=0x00;
		tempdata8[1]=0x07;
		bk1080_register_write(dev_data, 0x7e, tempdata8,2);
		bk1080_register_read(dev_data,0x05,tempdata8,8);
            /* rssi */		
            buf[0] = (int)(tempdata8[7]);
            /* snr */
            buf[1]  = (int)(tempdata8[1]&0xf);

            if (copy_to_user(argp, buf, sizeof(int[2]))) {
                ret = -EFAULT;
            }
            break;
        default:
	    printk(KERN_ERR "beken_fm_misc_ioctl:cmd=%d",cmd);
            return -EINVAL;
    }

    return ret;
}


static const struct file_operations beken_fm_misc_fops = {
    .owner = THIS_MODULE,
    .open  = beken_fm_misc_open,
    .unlocked_ioctl = beken_fm_misc_ioctl,
};

static struct miscdevice beken_fm_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = BEKEN_DEV_NAME,
    .fops  = &beken_fm_misc_fops,
};


/*------------------------------------------------------------------------------
 * BEKEN class attribute method
 ------------------------------------------------------------------------------*/
static ssize_t beken_fm_attr_open(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
    u8 open;

    if (size) {
        open = simple_strtol(buf, NULL, 10);
        if (open)
            beken_fm_open(beken_dev_data);
        else
            beken_fm_close(beken_dev_data);
    }

    return size;
}


static ssize_t beken_fm_attr_get_open(struct class *class, struct class_attribute *attr, char *buf)
{
    u8 opened;

    opened = atomic_read(&beken_dev_data->fm_opened);
    if (opened)
        return sprintf(buf, "Opened\n");
    else
        return sprintf(buf, "Closed\n");
}


static ssize_t beken_fm_attr_set_tune(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
    u16 frequency;

    if (size) {
        frequency = simple_strtol(buf, NULL, 10);/* decimal string to int */


        beken_fm_set_tune(beken_dev_data, frequency);
    }

    return size;
}

static ssize_t beken_fm_attr_get_frequency(struct class *class, struct class_attribute *attr, char *buf)
{
    u16 frequency;

    frequency = beken_fm_get_frequency(beken_dev_data);

    return sprintf(buf, "Frequency %d\n", frequency);
}


static ssize_t beken_fm_attr_search(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
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

        beken_fm_full_search(beken_dev_data, frequency, seek_dir, timeout, &freq_found);
    }

out:
    return size;
}


static ssize_t beken_fm_attr_set_volume(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
    u8 volume;

    if (size) {
        volume = simple_strtol(buf, NULL, 10);/* decimal string to int */

        beken_fm_set_volume(beken_dev_data, volume);
    }

    return size;
}

static ssize_t beken_fm_attr_get_volume(struct class *class, struct class_attribute *attr, char *buf)
{
    u8 volume;

    volume = beken_fm_get_volume(beken_dev_data);

    return sprintf(buf, "Volume %d\n", volume);
}

static struct class_attribute beken_fm_attrs[] = {
    __ATTR(fm_open,  S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, beken_fm_attr_get_open,      beken_fm_attr_open),
    __ATTR(fm_tune,  S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, beken_fm_attr_get_frequency, beken_fm_attr_set_tune),
    __ATTR(fm_seek,   S_IWUSR|S_IWGRP,         NULL,                          beken_fm_attr_search),
    __ATTR(fm_volume, S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, beken_fm_attr_get_volume,    beken_fm_attr_set_volume),
    {},
};

void beken_fm_sysfs_init(struct class *class)
{
    class->class_attrs = beken_fm_attrs;
}


/*------------------------------------------------------------------------------
 * BEKEN i2c device driver.
 ------------------------------------------------------------------------------*/

static int __devexit beken_remove(struct i2c_client *client)
{
    struct beken_drv_data  *cxt = i2c_get_clientdata(client);

    beken_fm_close(cxt);
    beken_chip_vdd_input(cxt,false);
    if(cxt->regu != NULL) {
        regulator_put(cxt->regu);
    }
    misc_deregister(&beken_fm_misc_device);
    class_unregister(&cxt->fm_class);
    kfree(cxt);

    dev_info(&client->dev, "%s\n", __func__);

    return 0;
}


static int beken_resume(struct i2c_client *client)
{
    struct beken_drv_data *cxt = i2c_get_clientdata(client);

    dev_info(&cxt->client->dev, "%s, FM opened before suspend: %d\n",
        __func__, cxt->opened_before_suspend);

    if (cxt->opened_before_suspend) {
        cxt->opened_before_suspend = 0;

        beken_fm_open(cxt);

        beken_fm_set_volume(cxt, cxt->current_volume);
    }

    if (atomic_read(&cxt->fm_opened) == 0) {
    /* The chip must go into stand-by mode. */
        mutex_lock(&cxt->mutex);

        beken_fm_open(cxt);

        beken_fm_close(cxt);

        mutex_unlock(&cxt->mutex);
    }

    return 0;
}


static int beken_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct beken_drv_data *cxt = i2c_get_clientdata(client);

    dev_info(&cxt->client->dev, "%s, FM opend: %d\n",
        __func__, atomic_read(&cxt->fm_opened));

    if (atomic_read(&cxt->fm_opened) && cxt->bg_play_enable == 0) {
        cxt->opened_before_suspend = 1;
        beken_fm_close(cxt);
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

static void beken_early_resume (struct early_suspend* es)
{
#ifdef BEKEN_DEBUG
    printk("%s.\n", __func__);
#endif
    if (beken_dev_data) {
        beken_resume(beken_dev_data->client);
    }
}

static void beken_early_suspend (struct early_suspend* es)
{
#ifdef BEKEN_DEBUG
    printk("%s.\n", __func__);
#endif
    if (beken_dev_data) {
        beken_suspend(beken_dev_data->client, (pm_message_t){.event=0});
    }
}

#endif /* CONFIG_HAS_EARLYSUSPEND */


static int beken_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    u16    reg_value = 0x0;
    int    ret = -EINVAL;

    struct beken_drv_data *cxt = NULL;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev, "beken driver: client is not i2c capable.\n");
        ret = -ENODEV;
        goto i2c_functioality_failed;
    }

    cxt = kzalloc(sizeof(struct beken_drv_data), GFP_KERNEL);
    if (cxt == NULL) {
        dev_err(&client->dev, "Can't alloc memory for module data.\n");
        ret = -ENOMEM;
        goto alloc_data_failed;
    }
   
    {
#ifdef CONFIG_ARCH_SC8825

     
        struct clk *clk_parent;

        fm_clk = clk_get(NULL,"clk_aux0");

        if (IS_ERR(fm_clk)) {
            printk("clock: failed to get clk_aux0\n");
        }

        clk_parent = clk_get(NULL, "ext_32k");
        if (IS_ERR(clk_parent)) {
                printk("failed to get parent ext_32k\n");
        }

        clk_set_parent(fm_clk, clk_parent);
 	clk_set_rate(fm_clk, 32000);
	 // sci_glb_write(REG_GLB_GEN1,BIT_CLK_AUX0_EN,-1UL);
    
        cxt->regu =regulator_get(&client->dev,"vdd28");

#else
         
      cxt->regu =regulator_get(&client->dev,REGU_NAME_FM);

#endif

    }
    

    mutex_init(&cxt->mutex);
    mutex_lock(&cxt->mutex);

    cxt->client = client;
    i2c_set_clientdata(client, cxt);

    atomic_set(&cxt->fm_searching, 0);
    atomic_set(&cxt->fm_opened, 0);
    ret = beken_fm_power_on(cxt);

    if (ret < 0) {
        goto poweron_failed;
    }

    beken_check_chip_id(cxt, &reg_value);

#if (BEKEN_DEBUG)
//    beken_read_all_registers(cxt);
#endif

    beken_fm_close(cxt);

    cxt->fm_class.owner = THIS_MODULE;
    cxt->fm_class.name = "fm_class";
    beken_fm_sysfs_init(&cxt->fm_class);
    ret = class_register(&cxt->fm_class);
    if (ret < 0) {
        dev_err(&client->dev, "beken class init failed.\n");
        goto class_init_failed;
    }

    ret = misc_register(&beken_fm_misc_device);
    if (ret < 0) {
        dev_err(&client->dev, "beken misc device register failed.\n");
        goto misc_register_failed;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    cxt->early_suspend.suspend = beken_early_suspend;
    cxt->early_suspend.resume  = beken_early_resume;
    cxt->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    register_early_suspend(&cxt->early_suspend);
#endif
    cxt->opened_before_suspend = 0;
    cxt->bg_play_enable = 1;
    cxt->current_freq = 870; /* init current frequency, search may use it. */

    beken_dev_data = cxt;

    mutex_unlock(&cxt->mutex);

    return ret;
misc_register_failed:
    misc_deregister(&beken_fm_misc_device);
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
    dev_err(&client->dev,"beken driver init failed.\n");
    return ret;
}


static const struct i2c_device_id beken_i2c_id[] = {
    { BEKEN_I2C_NAME, 0 },
    { },
};

MODULE_DEVICE_TABLE(i2c, beken_i2c_id);

static struct i2c_driver beken_i2c_driver = {
    .driver = {
        .name = BEKEN_I2C_NAME,
    },
    .probe    =  beken_probe,
    .remove   =  __devexit_p(beken_remove),
    //replaced by early suspend and resume.
    //.resume   =  beken_resume,
    //.suspend  =  beken_suspend,
    .id_table =  beken_i2c_id,
};


static int __init beken_driver_init(void)
{
    pr_debug("BEKEN driver: init\n");

    return i2c_add_driver(&beken_i2c_driver);

}

static void __exit beken_driver_exit(void)
{
#ifdef DEBUG
    pr_debug("BEKEN driver exit\n");
#endif

    i2c_del_driver(&beken_i2c_driver);
    return;
}

module_init(beken_driver_init);
module_exit(beken_driver_exit);

MODULE_DESCRIPTION("BEKEN FM radio driver");
MODULE_AUTHOR("Spreadtrum Inc.");
MODULE_LICENSE("GPL");

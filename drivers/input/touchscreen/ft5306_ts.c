/*
 * drivers/input/touchscreen/ft5x0x_ts.c
 *
 * FocalTech ft5x0x TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * VERSION      	DATE			AUTHOR
 *    1.0		  2010-01-05			WenFS
 *
 * note: only support mulititouch	Wenfs 2010-10-01
 */

#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c/ft5306_ts.h>
#include <mach/regulator.h>

#define I2C_BOARD_INFO_METHOD   1
#define TS_DATA_THRESHOLD_CHECK	0
#define TS_WIDTH_MAX			539
#define	TS_HEIGHT_MAX			1060
#define TOUCH_VIRTUAL_KEYS
#define CONFIG_FT5X0X_MULTITOUCH 1

static int debug_level=0;

#define TS_DBG(format, ...)	\
	if(debug_level == 1)	\
		printk(KERN_INFO "FT5X0X " format "\n", ## __VA_ARGS__)

struct sprd_i2c_setup_data {
	unsigned i2c_bus;  //the same number as i2c->adap.nr in adapter probe function
	unsigned short i2c_address;
	int irq;
	char type[I2C_NAME_SIZE];
};

struct ft5x0x_ts_data *g_ft5x0x_ts;
static struct i2c_client *this_client;
/* Attribute */
static unsigned char suspend_flag=0; //0: sleep out; 1: sleep in

static ssize_t ft5x0x_show_suspend(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t ft5x0x_store_suspend(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t ft5x0x_show_version(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t ft5x0x_update(struct device* cd, struct device_attribute *attr, const char* buf, size_t len);
static ssize_t ft5x0x_show_debug(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t ft5x0x_store_debug(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static unsigned char ft5x0x_read_fw_ver(void);
static void ft5x0x_ts_suspend(struct early_suspend *handler);
static void ft5x0x_ts_resume(struct early_suspend *handler);
static int fts_ctpm_fw_update(void);
static int fts_ctpm_fw_upgrade_with_i_file(void);

struct ts_event {
	u16	x1;
	u16	y1;
	u16	x2;
	u16	y2;
	u16	x3;
	u16	y3;
	u16	x4;
	u16	y4;
	u16	x5;
	u16	y5;
	u16	pressure;
    u8  touch_point;
};

struct ft5x0x_ts_data {
	struct input_dev	*input_dev;
	struct i2c_client	*client;
	struct ts_event	event;
	struct work_struct	pen_event_work;
	struct workqueue_struct	*ts_workqueue;
	struct early_suspend	early_suspend;
	struct ft5x0x_ts_platform_data	*platform_data;
//	struct timer_list touch_timer;
};


static DEVICE_ATTR(suspend, S_IRUGO | S_IWUSR, ft5x0x_show_suspend, ft5x0x_store_suspend);
static DEVICE_ATTR(update, S_IRUGO | S_IWUSR, ft5x0x_show_version, ft5x0x_update);
static DEVICE_ATTR(debug, S_IRUGO | S_IWUSR, ft5x0x_show_debug, ft5x0x_store_debug);

static ssize_t ft5x0x_show_debug(struct device* cd,struct device_attribute *attr, char* buf)
{
	ssize_t ret = 0;

	sprintf(buf, "FT5206 Debug %d\n",debug_level);

	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t ft5x0x_store_debug(struct device* cd, struct device_attribute *attr,
		       const char* buf, size_t len)
{
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
	debug_level = on_off;

	printk("%s: debug_level=%d\n",__func__, debug_level);

	return len;
}

static ssize_t ft5x0x_show_suspend(struct device* cd,
				     struct device_attribute *attr, char* buf)
{
	ssize_t ret = 0;

	if(suspend_flag==1)
		sprintf(buf, "FT5206 Suspend\n");
	else
		sprintf(buf, "FT5206 Resume\n");

	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t ft5x0x_store_suspend(struct device* cd, struct device_attribute *attr,
		       const char* buf, size_t len)
{
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
	suspend_flag = on_off;

	if(on_off==1)
	{
		printk("FT5206 Entry Suspend\n");
		ft5x0x_ts_suspend(NULL);
	}
	else
	{
		printk("FT5206 Entry Resume\n");
		ft5x0x_ts_resume(NULL);
	}

	return len;
}

static ssize_t ft5x0x_show_version(struct device* cd,
				     struct device_attribute *attr, char* buf)
{
	ssize_t ret = 0;
	unsigned char uc_reg_value; 
	
	//get some register information
	uc_reg_value = ft5x0x_read_fw_ver();

	sprintf(buf, "ft5x0x firmware id is V%x\n", uc_reg_value);

	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t ft5x0x_update(struct device* cd, struct device_attribute *attr,
		       const char* buf, size_t len)
{
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
	unsigned char uc_reg_value;

	//get some register information
	uc_reg_value = ft5x0x_read_fw_ver();

	if(on_off==1)
	{
		printk("ft5x0x update, current firmware id is V%x\n", uc_reg_value);
		//fts_ctpm_fw_update();
		fts_ctpm_fw_upgrade_with_i_file();
	}

	return len;
}

static int ft5x0x_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	TS_DBG("%s", __func__);

	err = device_create_file(dev, &dev_attr_suspend);
	err = device_create_file(dev, &dev_attr_update);
	err = device_create_file(dev, &dev_attr_debug);

	return err;
}

static int ft5x0x_i2c_rxdata(char *rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);

	return ret;
}

static int ft5x0x_i2c_txdata(char *txdata, int length)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};

	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s i2c write error: %d\n", __func__, ret);

	return ret;
}
/***********************************************************************************************
Name	:	 ft5x0x_write_reg

Input	:	addr -- address
                     para -- parameter

Output	:

function	:	write register of ft5x0x

***********************************************************************************************/
static int ft5x0x_write_reg(u8 addr, u8 para)
{
	u8 buf[3];
	int ret = -1;

	buf[0] = addr;
	buf[1] = para;
	ret = ft5x0x_i2c_txdata(buf, 2);
	if (ret < 0) {
		pr_err("write reg failed! %#x ret: %d", buf[0], ret);
		return -1;
	}

	return 0;
}


/***********************************************************************************************
Name	:	ft5x0x_read_reg

Input	:	addr
                     pdata

Output	:

function	:	read register of ft5x0x

***********************************************************************************************/
static int ft5x0x_read_reg(u8 addr, u8 *pdata)
{
	int ret;
	u8 buf[2] = {0};

	buf[0] = addr;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= buf,
		},
	};

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);

	*pdata = buf[0];
	return ret;
}
#ifdef TOUCH_VIRTUAL_KEYS

static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	#if defined(CONFIG_MACH_SP6825GA) || defined(CONFIG_MACH_SP6825GB)
	return sprintf(buf,
         __stringify(EV_KEY) ":" __stringify(KEY_MENU)   ":90:907:70:58"
	 ":" __stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE)   ":250:907:70:58"
	 ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":420:907:70:58"
	 "\n");
	#else
	return sprintf(buf,
         __stringify(EV_KEY) ":" __stringify(KEY_MENU)   ":100:1020:80:65"
	 ":" __stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE)   ":280:1020:80:65"
	 ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":470:1020:80:65"
	 "\n");
	#endif
}

static struct kobj_attribute virtual_keys_attr = {
    .attr = {
        .name = "virtualkeys.ft5x0x_ts",
        .mode = S_IRUGO,
    },
    .show = &virtual_keys_show,
};

static struct attribute *properties_attrs[] = {
    &virtual_keys_attr.attr,
    NULL
};

static struct attribute_group properties_attr_group = {
    .attrs = properties_attrs,
};

static void ft5x0x_ts_virtual_keys_init(void)
{
    int ret;
    struct kobject *properties_kobj;

    printk("%s\n",__func__);

    properties_kobj = kobject_create_and_add("board_properties", NULL);
    if (properties_kobj)
        ret = sysfs_create_group(properties_kobj,
                     &properties_attr_group);
    if (!properties_kobj || ret)
        pr_err("failed to create board_properties\n");
}

#endif

/***********************************************************************************************
Name	:	 ft5x0x_read_fw_ver

Input	:	 void

Output	:	 firmware version

function	:	 read TP firmware version

***********************************************************************************************/
static unsigned char ft5x0x_read_fw_ver(void)
{
	unsigned char ver;
	ft5x0x_read_reg(FT5X0X_REG_FIRMID, &ver);
	return(ver);
}


#define CONFIG_SUPPORT_FTS_CTP_UPG


#ifdef CONFIG_SUPPORT_FTS_CTP_UPG

typedef enum
{
    ERR_OK,
    ERR_MODE,
    ERR_READID,
    ERR_ERASE,
    ERR_STATUS,
    ERR_ECC,
    ERR_DL_ERASE_FAIL,
    ERR_DL_PROGRAM_FAIL,
    ERR_DL_VERIFY_FAIL
}E_UPGRADE_ERR_TYPE;

typedef unsigned char         FTS_BYTE;     //8 bit
typedef unsigned short        FTS_WORD;    //16 bit
typedef unsigned int          FTS_DWRD;    //16 bit
typedef unsigned char         FTS_BOOL;    //8 bit

#define FTS_NULL                0x0
#define FTS_TRUE                0x01
#define FTS_FALSE              0x0

#define I2C_CTPM_ADDRESS       0x70

/*
[function]:
    callback: read data from ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[out]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
FTS_BOOL i2c_read_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;

    ret=i2c_master_recv(this_client, pbt_buf, dw_lenth);

    if(ret<=0)
    {
        printk("[TSP]i2c_read_interface error\n");
        return FTS_FALSE;
    }

    return FTS_TRUE;
}

/*
[function]:
    callback: write data to ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[in]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
FTS_BOOL i2c_write_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;
    ret=i2c_master_send(this_client, pbt_buf, dw_lenth);
    if(ret<=0)
    {
        printk("[TSP]i2c_write_interface error line = %d, ret = %d\n", __LINE__, ret);
        return FTS_FALSE;
    }

    return FTS_TRUE;
}

/*
[function]:
    send a command to ctpm.
[parameters]:
    btcmd[in]        :command code;
    btPara1[in]    :parameter 1;
    btPara2[in]    :parameter 2;
    btPara3[in]    :parameter 3;
    num[in]        :the valid input parameter numbers, if only command code needed and no parameters followed,then the num is 1;
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL cmd_write(FTS_BYTE btcmd,FTS_BYTE btPara1,FTS_BYTE btPara2,FTS_BYTE btPara3,FTS_BYTE num)
{
    FTS_BYTE write_cmd[4] = {0};

    write_cmd[0] = btcmd;
    write_cmd[1] = btPara1;
    write_cmd[2] = btPara2;
    write_cmd[3] = btPara3;
    return i2c_write_interface(I2C_CTPM_ADDRESS, write_cmd, num);
}

/*
[function]:
    write data to ctpm , the destination address is 0.
[parameters]:
    pbt_buf[in]    :point to data buffer;
    bt_len[in]        :the data numbers;
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL byte_write(FTS_BYTE* pbt_buf, FTS_DWRD dw_len)
{
    return i2c_write_interface(I2C_CTPM_ADDRESS, pbt_buf, dw_len);
}

/*
[function]:
    read out data from ctpm,the destination address is 0.
[parameters]:
    pbt_buf[out]    :point to data buffer;
    bt_len[in]        :the data numbers;
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL byte_read(FTS_BYTE* pbt_buf, FTS_BYTE bt_len)
{
    return i2c_read_interface(I2C_CTPM_ADDRESS, pbt_buf, bt_len);
}


/*
[function]:
    burn the FW to ctpm.
[parameters]:(ref. SPEC)
    pbt_buf[in]    :point to Head+FW ;
    dw_lenth[in]:the length of the FW + 6(the Head length);
    bt_ecc[in]    :the ECC of the FW
[return]:
    ERR_OK        :no error;
    ERR_MODE    :fail to switch to UPDATE mode;
    ERR_READID    :read id fail;
    ERR_ERASE    :erase chip fail;
    ERR_STATUS    :status error;
    ERR_ECC        :ecc error.
*/


#define    FTS_PACKET_LENGTH        128

#if 1
static unsigned char CTPM_FW[]=
{
#include "ft5306_qHD.i"
};
#endif

E_UPGRADE_ERR_TYPE  fts_ctpm_fw_upgrade(FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    FTS_BYTE reg_val[2] = {0};
    FTS_DWRD i = 0;

    FTS_DWRD  packet_number;
    FTS_DWRD  j;
    FTS_DWRD  temp;
    FTS_DWRD  lenght;
    FTS_BYTE  packet_buf[FTS_PACKET_LENGTH + 6];
    FTS_BYTE  auc_i2c_write_buf[10];
    FTS_BYTE bt_ecc;
    int      i_ret;

    /*********Step 1:Reset  CTPM *****/
    /*write 0xaa to register 0xfc*/
    ft5x0x_write_reg(0xfc,0xaa);
    msleep(50);
     /*write 0x55 to register 0xfc*/
    ft5x0x_write_reg(0xfc,0x55);
    printk("[TSP] Step 1: Reset CTPM test, bin-length=%d\n",dw_lenth);

    msleep(100);

    /*********Step 2:Enter upgrade mode *****/
    auc_i2c_write_buf[0] = 0x55;
    auc_i2c_write_buf[1] = 0xaa;
    do
    {
        i ++;
        i_ret = ft5x0x_i2c_txdata(auc_i2c_write_buf, 2);
        msleep(5);
    }while(i_ret <= 0 && i < 5 );
    /*********Step 3:check READ-ID***********************/
    cmd_write(0x90,0x00,0x00,0x00,4);
    byte_read(reg_val,2);
    if (reg_val[0] == 0x79 && reg_val[1] == 0x3)
    {
        printk("[TSP] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
    }
    else
    {
        printk("%s: ERR_READID, ID1 = 0x%x,ID2 = 0x%x\n", __func__,reg_val[0],reg_val[1]);
        return ERR_READID;
        //i_is_new_protocol = 1;
    }

     /*********Step 4:erase app*******************************/
    cmd_write(0x61,0x00,0x00,0x00,1);

    msleep(1500);
    printk("[TSP] Step 4: erase.\n");

    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;
    printk("[TSP] Step 5: start upgrade.\n");
    dw_lenth = dw_lenth - 8;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    for (j=0;j<packet_number;j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(lenght>>8);
        packet_buf[5] = (FTS_BYTE)lenght;

        for (i=0;i<FTS_PACKET_LENGTH;i++)
        {
            packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i];
            bt_ecc ^= packet_buf[6+i];
        }

        byte_write(&packet_buf[0],FTS_PACKET_LENGTH + 6);
        msleep(FTS_PACKET_LENGTH/6 + 1);
        if ((j * FTS_PACKET_LENGTH % 1024) == 0)
        {
              printk("[TSP] upgrade the 0x%x th byte.\n", ((unsigned int)j) * FTS_PACKET_LENGTH);
        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;

        for (i=0;i<temp;i++)
        {
            packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }

        byte_write(&packet_buf[0],temp+6);
        msleep(20);
    }

    //send the last six byte
    for (i = 0; i<6; i++)
    {
        temp = 0x6ffa + i;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        temp =1;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;
        packet_buf[6] = pbt_buf[ dw_lenth + i];
        bt_ecc ^= packet_buf[6];

        byte_write(&packet_buf[0],7);
        msleep(20);
    }

    /*********Step 6: read out checksum***********************/
    /*send the opration head*/
    cmd_write(0xcc,0x00,0x00,0x00,1);
    byte_read(reg_val,1);
    printk("[TSP] Step 6:  ecc read 0x%x, new firmware 0x%x. \n", reg_val[0], bt_ecc);
    if(reg_val[0] != bt_ecc)
    {
        printk("%s: ERR_ECC\n", __func__);
        return ERR_ECC;
    }

    /*********Step 7: reset the new FW***********************/
    cmd_write(0x07,0x00,0x00,0x00,1);

    return ERR_OK;
}

int fts_ctpm_auto_clb(void)
{
    unsigned char uc_temp;
    unsigned char i ;

    printk("[FTS] start auto CLB.\n");
    msleep(200);
    ft5x0x_write_reg(0, 0x40);
    msleep(100);   //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x4);  //write command to start calibration
    msleep(300);
    for(i=0;i<100;i++)
    {
        ft5x0x_read_reg(0,&uc_temp);
        if ( ((uc_temp&0x70)>>4) == 0x0)  //return to normal mode, calibration finish
        {
            break;
        }
        msleep(200);
        printk("[FTS] waiting calibration %d\n",i);
    }
    printk("[FTS] calibration OK.\n");

    msleep(300);
    ft5x0x_write_reg(0, 0x40);  //goto factory mode
    msleep(100);   //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x5);  //store CLB result
    msleep(300);
    ft5x0x_write_reg(0, 0x0); //return to normal mode
    msleep(300);
    printk("[FTS] store CLB result OK.\n");
    return 0;
}

#if 1
int fts_ctpm_fw_upgrade_with_i_file(void)
{
   FTS_BYTE*     pbt_buf = FTS_NULL;
   int i_ret;

    //=========FW upgrade========================*/
   pbt_buf = CTPM_FW;
   /*call the upgrade function*/
   i_ret =  fts_ctpm_fw_upgrade(pbt_buf,sizeof(CTPM_FW));
   if (i_ret != 0)
   {
	printk("[FTS] upgrade failed i_ret = %d.\n", i_ret);
       //error handling ...
       //TBD
   }
   else
   {
       printk("[FTS] upgrade successfully.\n");
       fts_ctpm_auto_clb();  //start auto CLB
   }

   return i_ret;
}
#endif

static int fts_ctpm_fw_update(void)
{
    int ret = 0;
    const struct firmware *fw;
    unsigned char *fw_buf;
    struct platform_device *pdev;

    pdev = platform_device_register_simple("ft5206_ts", 0, NULL, 0);
    if (IS_ERR(pdev)) {
        printk("%s: Failed to register firmware\n", __func__);
        return -1;
    }

    ret = request_firmware(&fw, "ft5306_fw.bin", &pdev->dev);
    if (ret) {
		printk("%s: request_firmware error\n",__func__);
		platform_device_unregister(pdev);
        return -1;
    }

    platform_device_unregister(pdev);
    printk("%s:fw size=%d\n", __func__,fw->size);

    fw_buf = kzalloc(fw->size, GFP_KERNEL | GFP_DMA);
    memcpy(fw_buf, fw->data, fw->size);

    fts_ctpm_fw_upgrade(fw_buf, fw->size);

    printk("%s: update finish\n", __func__);
    release_firmware(fw);
    kfree(fw_buf);

    return 0;
}

#endif

static void ft5x0x_ts_release(void)
{
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
#ifdef CONFIG_FT5X0X_MULTITOUCH
	//input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
	input_report_key(data->input_dev, BTN_TOUCH, 0);
#else
	input_report_abs(data->input_dev, ABS_PRESSURE, 0);
	input_report_key(data->input_dev, BTN_TOUCH, 0);
#endif
	input_sync(data->input_dev);
	TS_DBG("%s",__func__);
}

static int ft5x0x_read_data(void)
{
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
//	u8 buf[14] = {0};
	u8 buf[32] = {0};
	int ret = -1;

#ifdef CONFIG_FT5X0X_MULTITOUCH
//	ret = ft5x0x_i2c_rxdata(buf, 13);
	ret = ft5x0x_i2c_rxdata(buf, 31);
#else
    ret = ft5x0x_i2c_rxdata(buf, 7);
#endif
    if (ret < 0) {
		printk("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return ret;
	}

	memset(event, 0, sizeof(struct ts_event));
//	event->touch_point = buf[2] & 0x03;// 0000 0011
	event->touch_point = buf[2] & 0x07;// 000 0111

    if (event->touch_point == 0) {
        ft5x0x_ts_release();
        return 1;
    }

#ifdef CONFIG_FT5X0X_MULTITOUCH
    switch (event->touch_point) {
		case 5:
			event->x5 = (s16)(buf[0x1b] & 0x0F)<<8 | (s16)buf[0x1c];
			event->y5 = (s16)(buf[0x1d] & 0x0F)<<8 | (s16)buf[0x1e];
		#if defined(CONFIG_MACH_SP6825GA) || defined(CONFIG_MACH_SP6825GB)
			event->x5 = event->x5*8/9;
			event->y5 = event->y5*854/960;
		#endif
			TS_DBG("===x5 = %d,y5 = %d ====",event->x5,event->y5);
		case 4:
			event->x4 = (s16)(buf[0x15] & 0x0F)<<8 | (s16)buf[0x16];
			event->y4 = (s16)(buf[0x17] & 0x0F)<<8 | (s16)buf[0x18];
		#if defined(CONFIG_MACH_SP6825GA) || defined(CONFIG_MACH_SP6825GB)
			event->x4 = event->x4*8/9;
			event->y4 = event->y4*854/960;
		#endif
			TS_DBG("===x4 = %d,y4 = %d ====",event->x4,event->y4);
		case 3:
			event->x3 = (s16)(buf[0x0f] & 0x0F)<<8 | (s16)buf[0x10];
			event->y3 = (s16)(buf[0x11] & 0x0F)<<8 | (s16)buf[0x12];
		#if defined(CONFIG_MACH_SP6825GA) || defined(CONFIG_MACH_SP6825GB)
			event->x3 = event->x3*8/9;
			event->y3 = event->y3*854/960;
		#endif
			TS_DBG("===x3 = %d,y3 = %d ====",event->x3,event->y3);
		case 2:
			event->x2 = (s16)(buf[9] & 0x0F)<<8 | (s16)buf[10];
			event->y2 = (s16)(buf[11] & 0x0F)<<8 | (s16)buf[12];
		#if defined(CONFIG_MACH_SP6825GA) || defined(CONFIG_MACH_SP6825GB)
			event->x2 = event->x2*8/9;
			event->y2 = event->y2*854/960;
		#endif
			TS_DBG("===x2 = %d,y2 = %d ====",event->x2,event->y2);
		case 1:
			event->x1 = (s16)(buf[3] & 0x0F)<<8 | (s16)buf[4];
			event->y1 = (s16)(buf[5] & 0x0F)<<8 | (s16)buf[6];
		#if defined(CONFIG_MACH_SP6825GA) || defined(CONFIG_MACH_SP6825GB)
			event->x1 = event->x1*8/9;
			event->y1 = event->y1*854/960;
		#endif
			TS_DBG("===x1 = %d,y1 = %d ====",event->x1,event->y1);
            break;
		default:
		    return -1;
	}
#else
    if (event->touch_point == 1) {
        event->x1 = (s16)(buf[3] & 0x0F)<<8 | (s16)buf[4];
        event->y1 = (s16)(buf[5] & 0x0F)<<8 | (s16)buf[6];
    }
#endif
    event->pressure = 200;

	//printk("%d (%d, %d), (%d, %d)\n", event->touch_point, event->x1, event->y1, event->x2, event->y2);
#if TS_DATA_THRESHOLD_CHECK
	#ifdef CONFIG_FT5X0X_MULTITOUCH
		if((event->x1>TS_WIDTH_MAX || event->y1>TS_HEIGHT_MAX)||
		   (event->x2>TS_WIDTH_MAX || event->y2>TS_HEIGHT_MAX)||
		   (event->x3>TS_WIDTH_MAX || event->y3>TS_HEIGHT_MAX)||
		   (event->x4>TS_WIDTH_MAX || event->y4>TS_HEIGHT_MAX)||
		   (event->x5>TS_WIDTH_MAX || event->y5>TS_HEIGHT_MAX)) {
				//printk("%s: get dirty data x1=%d,y1=%d\n",__func__, event->x1, event->y1);
				return -1;
		}
	#else
		if(event->x1>TS_WIDTH_MAX || event->y1>TS_HEIGHT_MAX){
				//printk("%s: get dirty data x1=%d,y1=%d\n",__func__, event->x1, event->y1);
				return -1;
		}
	#endif
#endif

	return 0;
}

static void ft5x0x_report_value(void)
{
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;

//		printk("==ft5x0x_report_value =\n");
	if(event->touch_point)
		input_report_key(data->input_dev, BTN_TOUCH, 1);
#ifdef CONFIG_FT5X0X_MULTITOUCH
	switch(event->touch_point) {
		case 5:
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x5);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y5);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
			input_mt_sync(data->input_dev);

		case 4:
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x4);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y4);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
			input_mt_sync(data->input_dev);

		case 3:
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x3);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y3);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
			input_mt_sync(data->input_dev);

		case 2:
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x2);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y2);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
			input_mt_sync(data->input_dev);

		case 1:
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x1);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y1);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
			input_mt_sync(data->input_dev);

		default:
//			printk("==touch_point default =\n");
			break;
	}
#else	/* CONFIG_FT5X0X_MULTITOUCH*/
	if (event->touch_point == 1) {
		input_report_abs(data->input_dev, ABS_X, event->x1);
		input_report_abs(data->input_dev, ABS_Y, event->y1);
		input_report_abs(data->input_dev, ABS_PRESSURE, event->pressure);
	}
	input_report_key(data->input_dev, BTN_TOUCH, 1);
#endif	/* CONFIG_FT5X0X_MULTITOUCH*/
	input_sync(data->input_dev);
}	/*end ft5x0x_report_value*/

static void ft5x0x_ts_pen_irq_work(struct work_struct *work)
{

	int ret = -1;

	ret = ft5x0x_read_data();
	if (ret == 0) {
		ft5x0x_report_value();
	}

	enable_irq(this_client->irq);
}

static irqreturn_t ft5x0x_ts_interrupt(int irq, void *dev_id)
{

	struct ft5x0x_ts_data *ft5x0x_ts = (struct ft5x0x_ts_data *)dev_id;

	disable_irq_nosync(this_client->irq);
	if (!work_pending(&ft5x0x_ts->pen_event_work)) {
		queue_work(ft5x0x_ts->ts_workqueue, &ft5x0x_ts->pen_event_work);
	}
	//printk("==int=, 11irq=%d\n", this_client->irq);
	return IRQ_HANDLED;
}

#if 0
void ft5x0x_tpd_polling(void)
{
    struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);

   // if (!work_pending(&data->pen_event_work)) {
    	queue_work(data->ts_workqueue, &data->pen_event_work);
    //}
	data->touch_timer.expires = jiffies + 2;
	add_timer(&data->touch_timer);
}
#endif

static void ft5x0x_ts_reset(void)
{
	struct ft5x0x_ts_platform_data *pdata = g_ft5x0x_ts->platform_data;

	gpio_direction_output(pdata->reset_gpio_number, 1);
	msleep(1);
	gpio_set_value(pdata->reset_gpio_number, 0);
	msleep(10);
	gpio_set_value(pdata->reset_gpio_number, 1);
	msleep(200);
}

static void ft5x0x_ts_suspend(struct early_suspend *handler)
{
	printk("==ft5x0x_ts_suspend=\n");
	ft5x0x_write_reg(FT5X0X_REG_PMODE, PMODE_HIBERNATE);
}

static void ft5x0x_ts_resume(struct early_suspend *handler)
{
	printk("==%s==\n", __FUNCTION__);
	ft5x0x_ts_reset();
	ft5x0x_write_reg(FT5X0X_REG_PERIODACTIVE, 8);//about 80HZ
}

static void ft5x0x_ts_hw_init(struct ft5x0x_ts_data *ft5x0x_ts)
{
	struct regulator *reg_vdd;
	struct i2c_client *client = ft5x0x_ts->client;
	struct ft5x0x_ts_platform_data *pdata = ft5x0x_ts->platform_data;

	printk(KERN_INFO "%s [irq=%d];[rst=%d]\n",__func__,pdata->irq_gpio_number,pdata->reset_gpio_number);
	gpio_request(pdata->irq_gpio_number, "ts_irq_pin");
	gpio_request(pdata->reset_gpio_number, "ts_rst_pin");
	gpio_direction_output(pdata->reset_gpio_number, 1);
	gpio_direction_input(pdata->irq_gpio_number);
	//vdd power on
	reg_vdd = regulator_get(&client->dev, pdata->vdd_name);
	regulator_set_voltage(reg_vdd, 2800000, 2800000);
	regulator_enable(reg_vdd);
	msleep(100);
	//reset
	ft5x0x_ts_reset();
}

static int
ft5x0x_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ft5x0x_ts_data *ft5x0x_ts;
	struct input_dev *input_dev;
	struct ft5x0x_ts_platform_data *pdata = client->dev.platform_data;
	int err = 0;
	unsigned char uc_reg_value;

	printk(KERN_INFO "%s: probe\n",__func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ft5x0x_ts = kzalloc(sizeof(*ft5x0x_ts), GFP_KERNEL);
	if (!ft5x0x_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	g_ft5x0x_ts = ft5x0x_ts;
	ft5x0x_ts->platform_data = pdata;
	this_client = client;
	ft5x0x_ts->client = client;
	ft5x0x_ts_hw_init(ft5x0x_ts);
	i2c_set_clientdata(client, ft5x0x_ts);
	client->irq = gpio_to_irq(pdata->irq_gpio_number);

	ft5x0x_read_reg(FT5X0X_REG_CIPHER, &uc_reg_value);
	if(uc_reg_value != 0x55)
	{
		if(uc_reg_value == 0xa3) {
			msleep(100);
			fts_ctpm_fw_upgrade_with_i_file();
		}
		else {
			printk("chip id error %x\n",uc_reg_value);
			err = -ENODEV;
			goto exit_alloc_data_failed;
		}
	}

	ft5x0x_write_reg(FT5X0X_REG_PERIODACTIVE, 8);//about 80HZ

	INIT_WORK(&ft5x0x_ts->pen_event_work, ft5x0x_ts_pen_irq_work);

	ft5x0x_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ft5x0x_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
#ifdef TOUCH_VIRTUAL_KEYS
	ft5x0x_ts_virtual_keys_init();
#endif
	ft5x0x_ts->input_dev = input_dev;

#ifdef CONFIG_FT5X0X_MULTITOUCH
	__set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
	__set_bit(KEY_MENU,  input_dev->keybit);
	__set_bit(KEY_BACK,  input_dev->keybit);
	__set_bit(KEY_HOMEPAGE,  input_dev->keybit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
#else
	__set_bit(ABS_X, input_dev->absbit);
	__set_bit(ABS_Y, input_dev->absbit);
	__set_bit(ABS_PRESSURE, input_dev->absbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(KEY_MENU,  input_dev->keybit);
	__set_bit(KEY_BACK,  input_dev->keybit);
	__set_bit(KEY_HOMEPAGE,  input_dev->keybit);

	input_set_abs_params(input_dev, ABS_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_PRESSURE, 0, PRESS_MAX, 0 , 0);
#endif

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	input_dev->name		= FT5X0X_NAME;		//dev_name(&client->dev)
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		"ft5x0x_ts_probe: failed to register input device: %s\n",
		dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

	err = request_irq(client->irq, ft5x0x_ts_interrupt, IRQF_TRIGGER_FALLING, client->name, ft5x0x_ts);
	if (err < 0) {
		dev_err(&client->dev, "ft5x0x_probe: request irq failed %d\n",err);
		goto exit_irq_request_failed;
	}

	disable_irq_nosync(client->irq);

	TS_DBG("==register_early_suspend =");
	ft5x0x_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ft5x0x_ts->early_suspend.suspend = ft5x0x_ts_suspend;
	ft5x0x_ts->early_suspend.resume	= ft5x0x_ts_resume;
	register_early_suspend(&ft5x0x_ts->early_suspend);

	msleep(100);
	//get some register information
	uc_reg_value = ft5x0x_read_fw_ver();
	printk("[FST] Firmware version = 0x%x\n", uc_reg_value);
	printk("[FST] New Firmware version = 0x%x\n", CTPM_FW[sizeof(CTPM_FW)-2]);

	if(uc_reg_value != CTPM_FW[sizeof(CTPM_FW)-2])
	{
		fts_ctpm_fw_upgrade_with_i_file();
	}

#if 0
	ft5x0x_ts->touch_timer.function = ft5x0x_tpd_polling;
	ft5x0x_ts->touch_timer.data = 0;
	init_timer(&ft5x0x_ts->touch_timer);
	ft5x0x_ts->touch_timer.expires = jiffies + HZ*3;
	add_timer(&ft5x0x_ts->touch_timer);
#endif

	ft5x0x_create_sysfs(client);

	enable_irq(client->irq);
	return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
	free_irq(client->irq, ft5x0x_ts);
exit_irq_request_failed:
	cancel_work_sync(&ft5x0x_ts->pen_event_work);
	destroy_workqueue(ft5x0x_ts->ts_workqueue);
exit_create_singlethread:
	i2c_set_clientdata(client, NULL);
	kfree(ft5x0x_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}

static int __devexit ft5x0x_ts_remove(struct i2c_client *client)
{

	struct ft5x0x_ts_data *ft5x0x_ts = i2c_get_clientdata(client);

	printk("==ft5x0x_ts_remove=\n");

	unregister_early_suspend(&ft5x0x_ts->early_suspend);
	free_irq(client->irq, ft5x0x_ts);
	input_unregister_device(ft5x0x_ts->input_dev);
	kfree(ft5x0x_ts);
	cancel_work_sync(&ft5x0x_ts->pen_event_work);
	destroy_workqueue(ft5x0x_ts->ts_workqueue);
	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct i2c_device_id ft5x0x_ts_id[] = {
	{ FT5X0X_NAME, 0 },{ }
};


MODULE_DEVICE_TABLE(i2c, ft5x0x_ts_id);

static struct i2c_driver ft5x0x_ts_driver = {
	.probe		= ft5x0x_ts_probe,
	.remove		= __devexit_p(ft5x0x_ts_remove),
	.id_table	= ft5x0x_ts_id,
	.driver	= {
		.name	= FT5X0X_NAME,
		.owner	= THIS_MODULE,
	},
};

#if I2C_BOARD_INFO_METHOD
static int __init ft5x0x_ts_init(void)
{
	int ret;
	printk("==ft5x0x_ts_init==\n");
	ret = i2c_add_driver(&ft5x0x_ts_driver);
	return ret;
//	return i2c_add_driver(&ft5x0x_ts_driver);
}

static void __exit ft5x0x_ts_exit(void)
{
	printk("==ft5x0x_ts_exit==\n");
	i2c_del_driver(&ft5x0x_ts_driver);
}
#else //register i2c device&driver dynamicly

int sprd_add_i2c_device(struct sprd_i2c_setup_data *i2c_set_data, struct i2c_driver *driver)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int ret,err;


	TS_DBG("%s : i2c_bus=%d; slave_address=0x%x; i2c_name=%s",__func__,i2c_set_data->i2c_bus, \
		    i2c_set_data->i2c_address, i2c_set_data->type);

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = i2c_set_data->i2c_address;
	strlcpy(info.type, i2c_set_data->type, I2C_NAME_SIZE);
	if(i2c_set_data->irq > 0)
		info.irq = i2c_set_data->irq;

	adapter = i2c_get_adapter( i2c_set_data->i2c_bus);
	if (!adapter) {
		printk("%s: can't get i2c adapter %d\n",
			__func__,  i2c_set_data->i2c_bus);
		err = -ENODEV;
		goto err_driver;
	}

	client = i2c_new_device(adapter, &info);
	if (!client) {
		printk("%s:  can't add i2c device at 0x%x\n",
			__func__, (unsigned int)info.addr);
		err = -ENODEV;
		goto err_driver;
	}

	i2c_put_adapter(adapter);

	ret = i2c_add_driver(driver);
	if (ret != 0) {
		printk("%s: can't add i2c driver\n", __func__);
		err = -ENODEV;
		goto err_driver;
	}

	return 0;

err_driver:
	return err;
}

void sprd_del_i2c_device(struct i2c_client *client, struct i2c_driver *driver)
{
	TS_DBG("%s : slave_address=0x%x; i2c_name=%s",__func__, client->addr, client->name);
	i2c_unregister_device(client);
	i2c_del_driver(driver);
}

static int __init ft5x0x_ts_init(void)
{
	int ft5x0x_irq;

	printk("%s\n", __func__);

	ft5x0x_irq=ft5x0x_ts_config_pins();
	ft5x0x_ts_setup.i2c_bus = 0;
	ft5x0x_ts_setup.i2c_address = FT5206_TS_ADDR;
	strcpy (ft5x0x_ts_setup.type,FT5206_TS_NAME);
	ft5x0x_ts_setup.irq = ft5x0x_irq;
	return sprd_add_i2c_device(&ft5x0x_ts_setup, &ft5x0x_ts_driver);
}

static void __exit ft5x0x_ts_exit(void)
{
	printk("%s\n", __func__);
	sprd_del_i2c_device(this_client, &ft5x0x_ts_driver);
}
#endif

module_init(ft5x0x_ts_init);
module_exit(ft5x0x_ts_exit);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x0x TouchScreen driver");
MODULE_LICENSE("GPL");

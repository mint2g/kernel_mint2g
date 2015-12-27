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
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <mach/board.h>
#include "sensor_cfg.h"
#include "sensor_drv.h"
#include "sensor_cfg.h"

#define _pard(a) __raw_readl(a)

#define SENSOR_ONE_I2C	1
#define SENSOR_ZERO_I2C	0
#define SENSOR_16_BITS_I2C	2
#define SENSOR_I2C_FREQ      (100*1000)
#define SENSOR_I2C_PORT_0		0
#define SENSOR_I2C_ACK_TRUE		1
#define SENSOR_I2C_ACK_FALSE		0
#define SENSOR_I2C_STOP    1
#define SENSOR_I2C_NOSTOP    0
#define SENSOR_I2C_NULL_HANDLE  -1
#define SENSOR_ADDR_BITS_8   1
#define SENSOR_ADDR_BITS_16   2
#define SENSOR_CMD_BITS_8   1
#define SENSOR_CMD_BITS_16   2
#define SENSOR_LOW_SEVEN_BIT     0x7f
#define SENSOR_LOW_EIGHT_BIT     0xff
#define SENSOR_HIGN_SIXTEEN_BIT  0xffff0000
#define SENSOR_LOW_SIXTEEN_BIT  0xffff
#define SENSOR_I2C_OP_TRY_NUM   4
#define SCI_TRUE 1
#define SCI_FALSE 0
#define SENSOR_MCLK_SRC_NUM   4
#define SENSOR_MCLK_DIV_MAX     4
#define NUMBER_MAX                         0x7FFFFFF
#define ABS(a) ((a) > 0 ? (a) : -(a))
SENSOR_PARAM_INFO_T g_sensor_param_info={0};
typedef struct SN_MCLK {
	int clock;
	char *src_name;
} SN_MCLK;

const SN_MCLK sensor_mclk_tab[SENSOR_MCLK_SRC_NUM] = {
	{96, "clk_96m"},
	{77, "clk_76m800k"},
	{48, "clk_48m"},
	{26, "ext_26m"}
};

/**---------------------------------------------------------------------------*
 **                         Local Variables                                   *
 **---------------------------------------------------------------------------*/
LOCAL SENSOR_INFO_T *s_sensor_list_ptr[SENSOR_ID_MAX];
LOCAL SENSOR_INFO_T *s_sensor_info_ptr = PNULL;
LOCAL SENSOR_EXP_INFO_T s_sensor_exp_info;
LOCAL uint32_t s_sensor_mclk = 0;
LOCAL BOOLEAN s_sensor_init = SENSOR_FALSE;
LOCAL SENSOR_TYPE_E s_sensor_type = SENSOR_TYPE_NONE;
LOCAL SENSOR_MODE_E s_sensor_mode[SENSOR_ID_MAX] =
    { SENSOR_MODE_MAX, SENSOR_MODE_MAX, SENSOR_MODE_MAX };
LOCAL SENSOR_MUTEX_PTR s_imgsensor_mutex_ptr = PNULL;
LOCAL SENSOR_REGISTER_INFO_T s_sensor_register_info = { 0x00 };

LOCAL SENSOR_REGISTER_INFO_T_PTR s_sensor_register_info_ptr =
    &s_sensor_register_info;
struct clk *s_ccir_clk = NULL;
struct clk *s_ccir_enable_clk = NULL;
LOCAL uint32_t s_flash_mode = 0xff;
static struct i2c_client *this_client = NULL;
static int g_is_main_sensor = 0;
static int g_is_register_sensor = 0;
#define SIGN_0  0x73
#define SIGN_1  0x69
#define SIGN_2  0x67
#define SIGN_3  0x6e
static BOOLEAN s_sensor_identified = SCI_FALSE;
static BOOLEAN s_sensor_param_saved = SCI_FALSE;
static uint8_t  s_sensor_index[SENSOR_ID_MAX]={0xFF,0xFF,0xFF,0xFF,0xFF};
#define SENSOR_DEV_NAME	SENSOR_MAIN_I2C_NAME

LOCAL EXIF_SPEC_PIC_TAKING_COND_T s_default_exif={0x00};

static const struct i2c_device_id sensor_main_id[] = {
	{SENSOR_MAIN_I2C_NAME, 0},
	{}
};

static const struct i2c_device_id sensor_sub_id[] = {
	{SENSOR_SUB_I2C_NAME, 0},
	{}
};
static unsigned short sensor_main_force[] =
    { 2, SENSOR_MAIN_I2C_ADDR, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const sensor_main_forces[] =
    { sensor_main_force, NULL };
static unsigned short sensor_main_default_addr_list[] =
    { SENSOR_MAIN_I2C_ADDR, I2C_CLIENT_END };
static unsigned short sensor_sub_force[] =
    { 2, SENSOR_SUB_I2C_ADDR, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const sensor_sub_forces[] =
    { sensor_sub_force, NULL };
static unsigned short sensor_sub_default_addr_list[] =
    { SENSOR_SUB_I2C_ADDR, I2C_CLIENT_END };
/**---------------------------------------------------------------------------*
 **                         Sensor Tuning : The register value is loaded from SD-Card.                     *
 **                         dhee79.lee@samsung.com                                                                     * 
 **---------------------------------------------------------------------------*/
/*Kyle-TD Tuning*/
//#define CONFIG_LOAD_FILE

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

static char *sensor_regs_table = NULL;

static int sensor_regs_table_size;

int32_t Sensor_regs_table_init(void)
{
	struct file *filp;

	char *dp;
	long l;
	loff_t pos;
	int ret;
       SENSOR_PRINT(KERN_ERR "***** Sensor_regs_table_init   Check the Filie in MMC\n");

       mm_segment_t fs = get_fs(); 
	set_fs(get_ds());
    
	filp = filp_open("/mnt/extSdCard/sensor_hi253_regs.h", O_RDONLY | O_LARGEFILE, 0);
        if(filp == NULL){
            SENSOR_PRINT(KERN_ERR "***** /mnt/extSdCard/sensor_hi253_regs.h failed\n");
            return 1;
        }
	if (IS_ERR(filp)) {
		SENSOR_PRINT_ERR("***** file open error: (s32)filp = %d\n", (s32)filp);
		return 1;
	}
	else
		SENSOR_PRINT(KERN_ERR "***** File is opened \n");

	l = filp->f_path.dentry->d_inode->i_size;

	SENSOR_PRINT("l = %ld\n", l);

	dp = kmalloc(l, GFP_KERNEL);
	if (dp == NULL) {
		SENSOR_PRINT_ERR("*****Out of Memory\n");
		filp_close(filp, current->files);
		return 1;
	}

	pos = 0;

	memset(dp, 0, l);

	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	if (ret != l) {
		SENSOR_PRINT_ERR("*****Failed to read file ret = %d\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		return 1;
	}

	filp_close(filp, current->files);
	set_fs(fs);

	sensor_regs_table = dp;
	sensor_regs_table_size = l;
	*((sensor_regs_table + sensor_regs_table_size) - 1) = '\0';

	SENSOR_PRINT("*****Compeleted %s %d\n", __func__, __LINE__);
	return 0;
}

int32_t Sensor_regs_table_write(char *name)
{
	char *start, *end, *reg;//, *data;	
	unsigned short addr, value;
	char reg_buf[8]={0}, data_buf[8]={0};

	addr = value = 0;

	SENSOR_PRINT("Sensor: ***** Sensor_regs_table_write : %s .  E \n", name);


	start = strstr(sensor_regs_table, name);

	end = strstr(start, "};");

	while (1) {	

		/* Find Address */	
		reg = strstr(start,"{0x");		
		if (reg)
			start = (reg + 12);
		if ((reg == NULL) || (reg > end))
			break;

		/* Write Value to Address */
		if (reg != NULL) {
			memcpy(reg_buf, (reg + 1), 4);
			memcpy(data_buf, (reg + 7), 4);
			addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16);
			value = (unsigned short)simple_strtoul(data_buf, NULL, 16);
//			SENSOR_PRINT("addr 0x%04x, value 0x%04x\n", addr, value);

                    if((addr == 0xFF) && (value == 0xFF)){
                        break;
                    } else if (addr == 0xFF)	{
				msleep(10*value);
				SENSOR_PRINT("-------->delay 0x%04x, value 0x%04x\n", addr, value);
			}	
			else
			{
				if( Sensor_WriteReg(addr, value) < 0 )
				{
					SENSOR_PRINT("<=PCAM=> %s fail on sensor_write\n", __func__);
				}
			}
		}
		else{
			SENSOR_PRINT(KERN_ERR " EXCEPTION! reg value is NULL\n");
		}
	}
	SENSOR_PRINT(KERN_ERR "***** Writing [%s] Ended\n",name);

	return 0;
}

#endif  /* CONFIG_LOAD_FILE */

#define SENSOR_INHERIT 0
#define SENSOR_WAIT_FOREVER 0

LOCAL int _Sensor_SetId(SENSOR_ID_E sensor_id);

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int res = 0;
	SENSOR_PRINT(KERN_INFO "SENSOR:sensor_probe E.\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SENSOR_PRINT_ERR(KERN_INFO "SENSOR: %s: functionality check failed\n",
		       __FUNCTION__);
		res = -ENODEV;
		goto out;
	}
	this_client = client;
	if (SENSOR_MAIN == Sensor_GetCurId()) {
		if (SENSOR_MAIN_I2C_ADDR != (this_client->addr & (~0xFF))) {
			this_client->addr =
			    (this_client->addr & (~0xFF)) |
			    (sensor_main_force[1] & 0xFF);
		}
	} else {
		if (SENSOR_SUB_I2C_ADDR != (this_client->addr & (~0xFF))) {
			this_client->addr =
			    (this_client->addr & (~0xFF)) | (sensor_sub_force[1]
							     & 0xFF);
		}
	}
	SENSOR_PRINT(KERN_INFO "sensor_probe,this_client->addr =0x%x\n",
	       this_client->addr);
	mdelay(1);  //FROM 20 TO 1
	return 0;
out:
	return res;
}

static int sensor_remove(struct i2c_client *client)
{
	return 0;
}

static int sensor_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	SENSOR_PRINT("SENSOR_DRV: detect! \n");
	strcpy(info->type, client->name);
	return 0;
}

static struct i2c_driver sensor_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   },
	.probe = sensor_probe,
	.remove = sensor_remove,
	.detect = sensor_detect,
};

SENSOR_MUTEX_PTR SENSOR_CreateMutex(const char *name_ptr,
				    uint32_t priority_inherit)
{
	return (SENSOR_MUTEX_PTR) 1;
}

uint32_t SENSOR_DeleteMutex(SENSOR_MUTEX_PTR mutex_ptr)
{
	return SENSOR_SUCCESS;
}

uint32_t SENSOR_GetMutex(SENSOR_MUTEX_PTR mutex_ptr, uint32_t wait_option)
{
	return SENSOR_SUCCESS;
}

uint32_t SENSOR_PutMutex(SENSOR_MUTEX_PTR mutex_ptr)
{
	return SENSOR_SUCCESS;
}

void ImgSensor_CreateMutex(void)
{
	s_imgsensor_mutex_ptr =
	    SENSOR_CreateMutex("IMG SENSOR SYNC MUTEX", SENSOR_INHERIT);
	SENSOR_PASSERT((s_imgsensor_mutex_ptr != PNULL),
		       ("IMG SENSOR Great MUTEX fail!"));
}

void ImgSensor_DeleteMutex(void)
{
	uint32_t ret;

	if (SENSOR_NULL == s_imgsensor_mutex_ptr) {
		return;
	}
	ret = SENSOR_DeleteMutex(s_imgsensor_mutex_ptr);
	SENSOR_ASSERT(ret == SENSOR_SUCCESS);
	s_imgsensor_mutex_ptr = SENSOR_NULL;
}

void ImgSensor_GetMutex(void)
{
	uint32_t ret;
	if (PNULL == s_imgsensor_mutex_ptr) {
		ImgSensor_CreateMutex();
	}

	ret = SENSOR_GetMutex(s_imgsensor_mutex_ptr, SENSOR_WAIT_FOREVER);
	SENSOR_ASSERT(ret == SENSOR_SUCCESS);
}

void ImgSensor_PutMutex(void)
{
	uint32_t ret;
	if (SENSOR_NULL == s_imgsensor_mutex_ptr) {
		return;
	}
	ret = SENSOR_PutMutex(s_imgsensor_mutex_ptr);
	SENSOR_ASSERT(ret == SENSOR_SUCCESS);
}

SENSOR_TYPE_E _Sensor_GetSensorType(void)
{
	return s_sensor_type;
}

void Sensor_Reset(uint32_t level)
{
	int err = 0xff;
	SENSOR_IOCTL_FUNC_PTR reset_func;
	SENSOR_PRINT("Sensor_Reset.\n");

	reset_func = s_sensor_info_ptr->ioctl_func_tab_ptr->reset;

	if (PNULL != reset_func) {
		reset_func(level);
	} else {
		err = gpio_request(GPIO_SENSOR_RESET, "ccirrst");
		if (err) {
			SENSOR_PRINT_ERR("Sensor_Reset failed requesting err=%d\n", err);
			return;
		}
		gpio_direction_output(GPIO_SENSOR_RESET, level);
		gpio_set_value(GPIO_SENSOR_RESET, level);
		msleep(40);
		gpio_set_value(GPIO_SENSOR_RESET, !level);
		msleep(20);
		gpio_free(GPIO_SENSOR_RESET);
	}
}


void Sensor_QReset(uint32_t level)
{
	int err = 0xff;
	SENSOR_IOCTL_FUNC_PTR reset_func;
	SENSOR_PRINT("Sensor_Reset.\n");

	reset_func = s_sensor_info_ptr->ioctl_func_tab_ptr->reset;

	if (PNULL != reset_func) {
		reset_func(level);
	} else {
		err = gpio_request(GPIO_SENSOR_RESET, "ccirrst");
		if (err) {
			SENSOR_PRINT_ERR("Sensor_Reset failed requesting err=%d\n", err);
			return;
		}
		gpio_direction_output(GPIO_SENSOR_RESET, level);
		gpio_set_value(GPIO_SENSOR_RESET, level);
		msleep(20);
		gpio_set_value(GPIO_SENSOR_RESET, !level);
		gpio_free(GPIO_SENSOR_RESET);
	}
}


void Sensor_Reset_EX(uint32_t power_down, uint32_t level)
{
	SENSOR_IOCTL_FUNC_PTR reset_func = 0;
	SENSOR_PRINT("Sensor_Reset_EX.\n");

	reset_func = s_sensor_info_ptr->ioctl_func_tab_ptr->reset;
	Sensor_PowerDown(!power_down);
	if(NULL != reset_func){
		reset_func(level);
	}
	else
	{
		Sensor_Reset(level);
	}
}

LOCAL int select_sensor_mclk(uint8_t clk_set, char **clk_src_name,
			     uint8_t * clk_div)
{
	uint8_t i, j, mark_src = 0, mark_div = 0, mark_src_tmp = 0;
	int clk_tmp, src_delta, src_delta_min = NUMBER_MAX;
	int div_delta_min = NUMBER_MAX;

	SENSOR_PRINT("SENSOR:select_sensor_mclk,clk_set=%d.\n", clk_set);
	if (clk_set > 96 || !clk_src_name || !clk_div) {
		return SENSOR_FAIL;
	}
	for (i = 0; i < SENSOR_MCLK_DIV_MAX; i++) {
		clk_tmp = (int)(clk_set * (i + 1));
		src_delta_min = NUMBER_MAX;
		for (j = 0; j < SENSOR_MCLK_SRC_NUM; j++) {
			src_delta = ABS(sensor_mclk_tab[j].clock - clk_tmp);
			if (src_delta < src_delta_min) {
				src_delta_min = src_delta;
				mark_src_tmp = j;
			}
		}
		if (src_delta_min < div_delta_min) {
			div_delta_min = src_delta_min;
			mark_src = mark_src_tmp;
			mark_div = i;
		}
	}
	SENSOR_PRINT("SENSOR:select_sensor_mclk,clk_src=%d,clk_div=%d .\n", mark_src,
	       mark_div);

	*clk_src_name = sensor_mclk_tab[mark_src].src_name;
	*clk_div = mark_div + 1;
	return SENSOR_SUCCESS;
}

int Sensor_SetMCLK(uint32_t mclk)
{
	struct clk *clk_parent = NULL;
	int ret;
	char *clk_src_name = NULL;
	uint8_t clk_div;

	SENSOR_PRINT
	    ("SENSOR: Sensor_SetMCLK -> s_sensor_mclk = %d MHz, clk = %d MHz\n",
	     s_sensor_mclk, mclk);

	if ((0 != mclk) && (s_sensor_mclk != mclk)) {
		if (s_ccir_clk) {
			clk_disable(s_ccir_clk);
			SENSOR_PRINT("###sensor s_ccir_clk clk_disable ok.\n");
		} else {
			s_ccir_clk = clk_get(NULL, "ccir_mclk");
			if (IS_ERR(s_ccir_clk)) {
				SENSOR_PRINT_ERR
				    ("###: Failed: Can't get clock [ccir_mclk]!\n");
				SENSOR_PRINT_ERR("###: s_sensor_clk = %p.\n",
						 s_ccir_clk);
			} else {
				SENSOR_PRINT
				    ("###sensor s_ccir_clk clk_get ok.\n");
			}
		}
		if (mclk > SENSOR_MAX_MCLK) {
			mclk = SENSOR_MAX_MCLK;
		}
		if (SENSOR_SUCCESS !=
		    select_sensor_mclk((uint8_t) mclk, &clk_src_name,
				       &clk_div)) {
			SENSOR_PRINT_ERR
			    ("SENSOR:Sensor_SetMCLK select clock source fail.\n");
			return -EINVAL;
		}

		clk_parent = clk_get(NULL, clk_src_name);
		if (!clk_parent) {
			SENSOR_PRINT_ERR
			    ("###:clock: failed to get clock [%s] by clk_get()!\n", clk_src_name);
			return -EINVAL;
		}

		ret = clk_set_parent(s_ccir_clk, clk_parent);
		if (ret) {
			SENSOR_PRINT_ERR
			    ("###:clock: clk_set_parent() failed!parent \n");
			return -EINVAL;
		}

		ret = clk_set_rate(s_ccir_clk, (mclk * SENOR_CLK_M_VALUE));
		if (ret) {
			SENSOR_PRINT_ERR
			    ("###:clock: clk_set_rate failed!\n");
			return -EINVAL;
		}
		ret = clk_enable(s_ccir_clk);
		if (ret) {
			SENSOR_PRINT_ERR("###:clock: clk_enable() failed!\n");
		} else {
			SENSOR_PRINT("###sensor s_ccir_clk clk_enable ok.\n");
		}

		if (NULL == s_ccir_enable_clk) {
			s_ccir_enable_clk = clk_get(NULL, "clk_ccir");
			if (IS_ERR(s_ccir_enable_clk)) {
				SENSOR_PRINT_ERR
				    ("###: Failed: Can't get clock [clk_ccir]!\n");
				SENSOR_PRINT_ERR
				    ("###: s_ccir_enable_clk = %p.\n",
				     s_ccir_enable_clk);
				return -EINVAL;
			} else {
				SENSOR_PRINT
				    ("###sensor s_ccir_enable_clk clk_get ok.\n");
			}
			ret = clk_enable(s_ccir_enable_clk);
			if (ret) {
				SENSOR_PRINT_ERR
				    ("###:clock: clk_enable() failed!\n");
			} else {
				SENSOR_PRINT
				    ("###sensor s_ccir_enable_clk clk_enable ok.\n");
			}
		}

		s_sensor_mclk = mclk;
		SENSOR_PRINT
		    ("SENSOR: Sensor_SetMCLK -> s_sensor_mclk = %d Hz.\n",
		     s_sensor_mclk);
	} else if (0 == mclk) {
		if (s_ccir_clk) {
			clk_disable(s_ccir_clk);
			SENSOR_PRINT("###sensor s_ccir_clk clk_disable ok.\n");
			clk_put(s_ccir_clk);
			SENSOR_PRINT("###sensor s_ccir_clk clk_put ok.\n");
			s_ccir_clk = NULL;
		}

		if (s_ccir_enable_clk) {
			clk_disable(s_ccir_enable_clk);
			SENSOR_PRINT
			    ("###sensor s_ccir_enable_clk clk_disable ok.\n");
			clk_put(s_ccir_enable_clk);
			SENSOR_PRINT
			    ("###sensor s_ccir_enable_clk clk_put ok.\n");
			s_ccir_enable_clk = NULL;
		}
		s_sensor_mclk = 0;
		SENSOR_PRINT("SENSOR: Sensor_SetMCLK -> Disable MCLK !!! \n");
	} else {
		SENSOR_PRINT("SENSOR: Sensor_SetMCLK -> Do nothing !! \n");
	}
	SENSOR_PRINT("SENSOR: Sensor_SetMCLK X \n");
	return 0;
}

static struct regulator *s_camvio_regulator = NULL;
static struct regulator *s_camavdd_regulator = NULL;
static struct regulator *s_camdvdd_regulator = NULL;

static void _sensor_regulator_disable(uint32_t power_on_count, struct regulator * ptr_cam_regulator)
{
	if(0 < power_on_count){
		regulator_disable(ptr_cam_regulator);
	}
}

void Sensor_SetVoltage(SENSOR_AVDD_VAL_E dvdd_val, SENSOR_AVDD_VAL_E avdd_val,
		       SENSOR_AVDD_VAL_E iodd_val)
{
	int err = 0;
	uint32_t volt_value = 0;
	//to make the power on&off be  pairs
	static int32_t iopower_on_count = 0;
	static int32_t avddpower_on_count = 0;
	static int32_t dvddpower_on_count = 0;

	SENSOR_PRINT
	    ("SENSOR:Sensor_SetVoltage,dvdd_val=%d,avdd_val=%d,iodd_val=%d.\n",
	     dvdd_val, avdd_val, iodd_val);

	if(NULL == s_camvio_regulator) {
		s_camvio_regulator = regulator_get(NULL, REGU_NAME_CAMVIO);
		if (IS_ERR(s_camvio_regulator)) {
			pr_err("SENSOR:could not get camvio.\n");
			return;
		}
	}
	switch (iodd_val) {
	case SENSOR_AVDD_2800MV:
		err = regulator_set_voltage(s_camvio_regulator,
					 SENSOER_VDD_2800MV,
					SENSOER_VDD_2800MV);
		volt_value = SENSOER_VDD_2800MV;
		if (err)
			pr_err("SENSOR:could not set camvio to 2800mv.\n");
		break;
	case SENSOR_AVDD_3800MV:
		err =
		    regulator_set_voltage(s_camvio_regulator,
					  SENSOER_VDD_3800MV,
					  SENSOER_VDD_3800MV);
		volt_value = SENSOER_VDD_3800MV;
		if (err)
			pr_err("SENSOR:could not set camvio to 3800mv.\n");
		break;
	case SENSOR_AVDD_1800MV:
		err =
		    regulator_set_voltage(s_camvio_regulator,
					  SENSOER_VDD_1800MV,
					  SENSOER_VDD_1800MV);
		volt_value = SENSOER_VDD_1800MV;
		if (err)
			pr_err("SENSOR:could not set camvio to 1800mv.\n");
		break;
	case SENSOR_AVDD_1200MV:
		err =
		    regulator_set_voltage(s_camvio_regulator,
					  SENSOER_VDD_1200MV,
					  SENSOER_VDD_1200MV);
		volt_value = SENSOER_VDD_1200MV;
		if (err)
			pr_err("SENSOR:could not set camvio to 1200mv.\n");
		break;
	case SENSOR_AVDD_CLOSED:
	case SENSOR_AVDD_UNUSED:
	default:
		volt_value = 0;
		break;
	}
	if (err) {
		pr_err("SENSOR:set camvio error!.\n");
		return;
	}
	if (0 != volt_value) {
		err = regulator_enable(s_camvio_regulator);
		iopower_on_count++;
		if (err) {
			regulator_put(s_camvio_regulator);
			s_camvio_regulator = NULL;
			pr_err("SENSOR:could not enable camvio.\n");
			return;
		}
	} else {
		/*regulator_disable(s_camvio_regulator);*/
		while(0 < iopower_on_count){
			_sensor_regulator_disable(iopower_on_count, s_camvio_regulator);
			iopower_on_count--;
		}

		regulator_put(s_camvio_regulator);
		s_camvio_regulator = NULL;
		pr_debug("SENSOR:disable camvio.\n");
	}

	if (NULL == s_camavdd_regulator) {
		s_camavdd_regulator = regulator_get(NULL, REGU_NAME_CAMAVDD);
		if (IS_ERR(s_camavdd_regulator)) {
			pr_err("SENSOR:could not get camavdd.\n");
			return;
		}
	}
	switch (avdd_val) {
	case SENSOR_AVDD_2800MV:
		err =
		    regulator_set_voltage(s_camavdd_regulator,
					  SENSOER_VDD_2800MV,
					  SENSOER_VDD_2800MV);
		volt_value = SENSOER_VDD_2800MV;
		if (err)
			pr_err("SENSOR:could not set camavdd to 2800mv.\n");
		break;
	case SENSOR_AVDD_3000MV:
		err =
		    regulator_set_voltage(s_camavdd_regulator,
					  SENSOER_VDD_3000MV,
					  SENSOER_VDD_3000MV);
		volt_value = SENSOER_VDD_3000MV;
		if (err)
			pr_err("SENSOR:could not set camavdd to 3800mv.\n");
		break;
	case SENSOR_AVDD_2500MV:
		err =
		    regulator_set_voltage(s_camavdd_regulator,
					  SENSOER_VDD_2500MV,
					  SENSOER_VDD_2500MV);
		volt_value = SENSOER_VDD_2500MV;
		if (err)
			pr_err("SENSOR:could not set camavdd to 1800mv.\n");
		break;
	case SENSOR_AVDD_1800MV:
		err =
		    regulator_set_voltage(s_camavdd_regulator,
					  SENSOER_VDD_1800MV,
					  SENSOER_VDD_1800MV);
		volt_value = SENSOER_VDD_1800MV;
		if (err)
			pr_err("SENSOR:could not set camavdd to 1200mv.\n");
		break;
	case SENSOR_AVDD_CLOSED:
	case SENSOR_AVDD_UNUSED:
	default:
		volt_value = 0;
		break;
	}
	if (err) {
		pr_err("SENSOR:set camavdd error!.\n");
		return;
	}
	if (0 != volt_value) {
		err = regulator_enable(s_camavdd_regulator);
		avddpower_on_count++;
		if (err) {
			regulator_put(s_camavdd_regulator);
			s_camavdd_regulator = NULL;
			pr_err("SENSOR:could not enable camavdd.\n");
			return;
		}
	} else {
		/*regulator_disable(s_camavdd_regulator);*/
		while(0 < avddpower_on_count){
			_sensor_regulator_disable(avddpower_on_count, s_camavdd_regulator);
			avddpower_on_count--;
		}

		regulator_put(s_camavdd_regulator);
		s_camavdd_regulator = NULL;
		pr_debug("SENSOR:disable camavdd.\n");
	}

	if (NULL == s_camdvdd_regulator) {
		s_camdvdd_regulator = regulator_get(NULL, REGU_NAME_CAMDVDD);
		if (IS_ERR(s_camdvdd_regulator)) {
			pr_err("SENSOR:could not get camdvdd.\n");
			return;
		}
	}
	switch (dvdd_val) {
	case SENSOR_AVDD_2800MV:
		err =
		    regulator_set_voltage(s_camdvdd_regulator,
					  SENSOER_VDD_2800MV,
					  SENSOER_VDD_2800MV);
		volt_value = SENSOER_VDD_2800MV;
		if (err)
			pr_err("SENSOR:could not set camdvdd to 2800mv.\n");
		break;
	case SENSOR_AVDD_1800MV:
		err =
		    regulator_set_voltage(s_camdvdd_regulator,
					  SENSOER_VDD_1800MV,
					  SENSOER_VDD_1800MV);
		volt_value = SENSOER_VDD_1800MV;
		if (err)
			pr_err("SENSOR:could not set camdvdd to 1800mv.\n");
		break;
	case SENSOR_AVDD_1500MV:
		err =
		    regulator_set_voltage(s_camdvdd_regulator,
					  SENSOER_VDD_1500MV,
					  SENSOER_VDD_1500MV);
		volt_value = SENSOER_VDD_1500MV;
		if (err)
			pr_err("SENSOR:could not set camdvdd to 1500mv.\n");
		break;
	case SENSOR_AVDD_1300MV:
		err =
		    regulator_set_voltage(s_camdvdd_regulator,
					  SENSOER_VDD_1300MV,
					  SENSOER_VDD_1300MV);
		volt_value = SENSOER_VDD_1300MV;
		if (err)
			pr_err("SENSOR:could not set camdvdd to 1300mv.\n");
		break;
	case SENSOR_AVDD_1200MV:
		err =
		    regulator_set_voltage(s_camdvdd_regulator,
					  SENSOER_VDD_1200MV,
					  SENSOER_VDD_1200MV);
		volt_value = SENSOER_VDD_1200MV;
		if (err)
			pr_err("SENSOR:could not set camdvdd to 1200mv.\n");
		break;
	case SENSOR_AVDD_CLOSED:
	case SENSOR_AVDD_UNUSED:
	default:
		volt_value = 0;
		break;
	}
	if (err) {
		pr_err("SENSOR:set camdvdd error,err=%d!.\n",err);
		return;
	}
	if (0 != volt_value) {
		err = regulator_enable(s_camdvdd_regulator);
		dvddpower_on_count++;
		if (err) {
			regulator_put(s_camdvdd_regulator);
			s_camdvdd_regulator = NULL;
			pr_err("SENSOR:could not enable camdvdd.\n");
			return;
		}
	} else {
		/*regulator_disable(s_camdvdd_regulator);*/
		while(0 < dvddpower_on_count){
			_sensor_regulator_disable(dvddpower_on_count, s_camdvdd_regulator);
			dvddpower_on_count--;
		}

		regulator_put(s_camdvdd_regulator);
		s_camdvdd_regulator = NULL;
		pr_debug("SENSOR:disable camdvdd.\n");
	}
	return;
}

LOCAL void Sensor_PowerOn(BOOLEAN power_on)
{
	BOOLEAN power_down;
	SENSOR_AVDD_VAL_E dvdd_val;
	SENSOR_AVDD_VAL_E avdd_val;
	SENSOR_AVDD_VAL_E iovdd_val;
	SENSOR_IOCTL_FUNC_PTR power_func;
	uint32_t rst_lvl = s_sensor_info_ptr->reset_pulse_level;

	power_down = (BOOLEAN) s_sensor_info_ptr->power_down_level;
	dvdd_val = s_sensor_info_ptr->dvdd_val;
	avdd_val = s_sensor_info_ptr->avdd_val;
	iovdd_val = s_sensor_info_ptr->iovdd_val;
	power_func = s_sensor_info_ptr->ioctl_func_tab_ptr->power;

	SENSOR_PRINT
	    ("SENSOR: Sensor_PowerOn -> power_on = %d, power_down_level = %d, avdd_val = %d\n",
	     power_on, power_down, avdd_val);

	if (PNULL != power_func) {
		power_func(power_on);
		if(power_on){
		Sensor_Reset(rst_lvl);
		}
	} else {
		if (power_on) {
			Sensor_PowerDown(power_down);
			Sensor_SetVoltage(dvdd_val, avdd_val, iovdd_val);
			msleep(10);
			Sensor_SetMCLK(SENSOR_DEFALUT_MCLK);
			msleep(5);
			Sensor_PowerDown(!power_down);
			Sensor_Reset(rst_lvl);
		} else {
			Sensor_PowerDown(power_down);
			msleep(20);
			Sensor_SetMCLK(SENSOR_DISABLE_MCLK);
			Sensor_SetVoltage(SENSOR_AVDD_CLOSED,
					SENSOR_AVDD_CLOSED,
					SENSOR_AVDD_CLOSED);
		}
	}
}
/*
		_Sensor_SetId(sensor_id);
		s_sensor_info_ptr = s_sensor_list_ptr[sensor_id];
		Sensor_PowerOn(SENSOR_TRUE);
		
		_Sensor_SetStatus(sensor_id);
		_Sensor_SetId(sensor_id);
		s_sensor_info_ptr = s_sensor_list_ptr[sensor_id];
		Sensor_SetExportInfo(&s_sensor_exp_info);
*/

LOCAL void Sensor_PowerOn_Ex(uint32_t sensor_id)
{
	BOOLEAN power_down;
	SENSOR_AVDD_VAL_E dvdd_val;
	SENSOR_AVDD_VAL_E avdd_val;
	SENSOR_AVDD_VAL_E iovdd_val;
	SENSOR_IOCTL_FUNC_PTR power_func;
	uint32_t rst_lvl = 0;

	_Sensor_SetId(sensor_id);
	s_sensor_info_ptr = s_sensor_list_ptr[sensor_id];
	s_sensor_info_ptr->reset_pulse_level;

	power_down = (BOOLEAN) s_sensor_info_ptr->power_down_level;
	dvdd_val = s_sensor_info_ptr->dvdd_val;
	avdd_val = s_sensor_info_ptr->avdd_val;
	iovdd_val = s_sensor_info_ptr->iovdd_val;
	power_func = s_sensor_info_ptr->ioctl_func_tab_ptr->power;

	SENSOR_PRINT
	    ("SENSOR:  power_down_level = %d, avdd_val = %d\n",
	     power_down, avdd_val);

	if (PNULL != power_func) {
		power_func(1);
	} else {

			Sensor_PowerDown(power_down);
			Sensor_SetVoltage(dvdd_val, avdd_val, iovdd_val);
			msleep(10);
			Sensor_SetMCLK(SENSOR_DEFALUT_MCLK);
			msleep(5);

	}
}




BOOLEAN Sensor_PowerDown(BOOLEAN power_level)
{
	SENSOR_IOCTL_FUNC_PTR entersleep_func =
	    s_sensor_info_ptr->ioctl_func_tab_ptr->enter_sleep;

	SENSOR_PRINT("SENSOR: Sensor_PowerDown -> main: power_down %d\n",
		     power_level);
	SENSOR_PRINT
	    ("SENSOR: Sensor_PowerDown PIN_CTL_CCIRPD1-> 0x8C000344 0x%x\n",
	     _pard(PIN_CTL_CCIRPD1));
	SENSOR_PRINT
	    ("SENSOR: Sensor_PowerDown PIN_CTL_CCIRPD0-> 0x8C000348 0x%x\n",
	     _pard(PIN_CTL_CCIRPD0));

	if (entersleep_func) {
		entersleep_func(power_level);
		return SENSOR_SUCCESS;
	}

	switch (Sensor_GetCurId()) {
	case SENSOR_MAIN:
		{
			gpio_request(GPIO_MAIN_SENSOR_PWN, "main camera");
			if (0 == power_level) {
				gpio_direction_output(GPIO_MAIN_SENSOR_PWN, 0);

			} else {
				gpio_direction_output(GPIO_MAIN_SENSOR_PWN, 1);
			}
			gpio_free(GPIO_MAIN_SENSOR_PWN);
			break;
		}
	case SENSOR_SUB:
		{
			gpio_request(GPIO_SUB_SENSOR_PWN, "sub camera");
			if (0 == power_level) {
				gpio_direction_output(GPIO_SUB_SENSOR_PWN, 0);
			} else {
				gpio_direction_output(GPIO_SUB_SENSOR_PWN, 1);
			}
			gpio_free(GPIO_SUB_SENSOR_PWN);
			break;
		}
	default:
		break;
	}
	return SENSOR_SUCCESS;
}

BOOLEAN Sensor_SetResetLevel(BOOLEAN plus_level)
{
	int err = 0xff;
	err = gpio_request(GPIO_SENSOR_RESET, "ccirrst");
	if (err) {
		SENSOR_PRINT_ERR("Sensor_Reset failed requesting err=%d\n", err);
		return SENSOR_FAIL;
	}
	gpio_direction_output(GPIO_SENSOR_RESET, plus_level);
	gpio_set_value(GPIO_SENSOR_RESET, plus_level);
	msleep(100);
	gpio_free(GPIO_SENSOR_RESET);
	return SENSOR_SUCCESS;
}

LOCAL void Sensor_SetExportInfo(SENSOR_EXP_INFO_T * exp_info_ptr)
{
	SENSOR_REG_TAB_INFO_T *resolution_info_ptr = PNULL;
	SENSOR_TRIM_T_PTR resolution_trim_ptr = PNULL;
	SENSOR_INFO_T *sensor_info_ptr = s_sensor_info_ptr;
	uint32_t i = 0;

	SENSOR_PRINT("SENSOR: Sensor_SetExportInfo.\n");

	SENSOR_MEMSET(exp_info_ptr, 0x00, sizeof(SENSOR_EXP_INFO_T));
	exp_info_ptr->image_format = sensor_info_ptr->image_format;
	exp_info_ptr->image_pattern = sensor_info_ptr->image_pattern;

	exp_info_ptr->pclk_polarity = (sensor_info_ptr->hw_signal_polarity & 0x01);	//the high 3bit will be the phase(delay sel)
	exp_info_ptr->vsync_polarity =
	    ((sensor_info_ptr->hw_signal_polarity >> 2) & 0x1);
	exp_info_ptr->hsync_polarity =
	    ((sensor_info_ptr->hw_signal_polarity >> 4) & 0x1);
	exp_info_ptr->pclk_delay =
	    ((sensor_info_ptr->hw_signal_polarity >> 5) & 0x07);

	exp_info_ptr->source_width_max = sensor_info_ptr->source_width_max;
	exp_info_ptr->source_height_max = sensor_info_ptr->source_height_max;

	exp_info_ptr->environment_mode = sensor_info_ptr->environment_mode;
	exp_info_ptr->image_effect = sensor_info_ptr->image_effect;
	exp_info_ptr->wb_mode = sensor_info_ptr->wb_mode;
	exp_info_ptr->step_count = sensor_info_ptr->step_count;

	exp_info_ptr->ext_info_ptr = sensor_info_ptr->ext_info_ptr;

	exp_info_ptr->preview_skip_num = sensor_info_ptr->preview_skip_num;
	exp_info_ptr->capture_skip_num = sensor_info_ptr->capture_skip_num;
	exp_info_ptr->preview_deci_num = sensor_info_ptr->preview_deci_num;
	exp_info_ptr->video_preview_deci_num =
	    sensor_info_ptr->video_preview_deci_num;

	exp_info_ptr->threshold_eb = sensor_info_ptr->threshold_eb;
	exp_info_ptr->threshold_mode = sensor_info_ptr->threshold_mode;
	exp_info_ptr->threshold_start = sensor_info_ptr->threshold_start;
	exp_info_ptr->threshold_end = sensor_info_ptr->threshold_end;

	exp_info_ptr->ioctl_func_ptr = sensor_info_ptr->ioctl_func_tab_ptr;
	if (PNULL != sensor_info_ptr->ioctl_func_tab_ptr->get_trim) {
		resolution_trim_ptr =
		    (SENSOR_TRIM_T_PTR) sensor_info_ptr->
		    ioctl_func_tab_ptr->get_trim(0x00);
	}
	for (i = SENSOR_MODE_COMMON_INIT; i < SENSOR_MODE_MAX; i++) {
		resolution_info_ptr =
		    &(sensor_info_ptr->resolution_tab_info_ptr[i]);
		if ((PNULL != resolution_info_ptr->sensor_reg_tab_ptr)
		    || ((0x00 != resolution_info_ptr->width)
			&& (0x00 != resolution_info_ptr->width))) {
			exp_info_ptr->sensor_mode_info[i].mode = i;
			exp_info_ptr->sensor_mode_info[i].width =
			    resolution_info_ptr->width;
			exp_info_ptr->sensor_mode_info[i].height =
			    resolution_info_ptr->height;
			if ((PNULL != resolution_trim_ptr)
			    && (0x00 != resolution_trim_ptr[i].trim_width)
			    && (0x00 != resolution_trim_ptr[i].trim_height)) {
				exp_info_ptr->sensor_mode_info[i].trim_start_x =
				    resolution_trim_ptr[i].trim_start_x;
				exp_info_ptr->sensor_mode_info[i].trim_start_y =
				    resolution_trim_ptr[i].trim_start_y;
				exp_info_ptr->sensor_mode_info[i].trim_width =
				    resolution_trim_ptr[i].trim_width;
				exp_info_ptr->sensor_mode_info[i].trim_height =
				    resolution_trim_ptr[i].trim_height;
				exp_info_ptr->sensor_mode_info[i].line_time =
				    resolution_trim_ptr[i].line_time;
			} else {
				exp_info_ptr->sensor_mode_info[i].trim_start_x =
				    0x00;
				exp_info_ptr->sensor_mode_info[i].trim_start_y =
				    0x00;
				exp_info_ptr->sensor_mode_info[i].trim_width =
				    resolution_info_ptr->width;
				exp_info_ptr->sensor_mode_info[i].trim_height =
				    resolution_info_ptr->height;
			}
			/*exp_info_ptr->sensor_mode_info[i].line_time=resolution_trim_ptr[i].line_time; */
			if (SENSOR_IMAGE_FORMAT_MAX !=
			    sensor_info_ptr->image_format) {
				exp_info_ptr->sensor_mode_info[i].image_format =
				    sensor_info_ptr->image_format;
			} else {
				exp_info_ptr->sensor_mode_info[i].image_format =
				    resolution_info_ptr->image_format;
			}
			SENSOR_PRINT
			    ("SENSOR: SENSOR mode Info > mode = %d, width = %d, height = %d, format = %d.\n",
			     i, resolution_info_ptr->width,
			     resolution_info_ptr->height,
			     exp_info_ptr->sensor_mode_info[i].image_format);
		} else {
			exp_info_ptr->sensor_mode_info[i].mode =
			    SENSOR_MODE_MAX;
		}
	}
	exp_info_ptr->sensor_interface = sensor_info_ptr->sensor_interface;
}

int32_t Sensor_WriteReg(uint16_t subaddr, uint16_t data)
{
	uint8_t cmd[4] = { 0 };
	uint32_t index = 0, i = 0;
	uint32_t cmd_num = 0;
	struct i2c_msg msg_w;
	int32_t ret = -1;
	SENSOR_IOCTL_FUNC_PTR write_reg_func;

	//      SENSOR_PRINT("this_client->addr=0x%x\n",this_client->addr);
	//      SENSOR_PRINT_ERR("Sensor_WriteReg:addr=0x%x,data=0x%x .\n",subaddr,data);

	write_reg_func = s_sensor_info_ptr->ioctl_func_tab_ptr->write_reg;

	if (PNULL != write_reg_func) {
		if (SENSOR_OP_SUCCESS !=
		    write_reg_func((subaddr << BIT(4)) + data)) {
			SENSOR_PRINT
			    ("SENSOR: IIC write : reg:0x%04x, val:0x%04x error\n",
			     subaddr, data);
		}
	} else {
		if (SENSOR_I2C_REG_16BIT ==
		    (s_sensor_info_ptr->reg_addr_value_bits &
		     SENSOR_I2C_REG_16BIT)) {
			cmd[cmd_num++] =
			    (uint8_t) ((subaddr >> BIT(3)) &
				       SENSOR_LOW_EIGHT_BIT);
			index++;
			cmd[cmd_num++] =
			    (uint8_t) (subaddr & SENSOR_LOW_EIGHT_BIT);
			index++;
		} else {
			cmd[cmd_num++] = (uint8_t) subaddr;
			index++;
		}

		if (SENSOR_I2C_VAL_16BIT ==
		    (s_sensor_info_ptr->reg_addr_value_bits &
		     SENSOR_I2C_VAL_16BIT)) {
			cmd[cmd_num++] =
			    (uint8_t) ((data >> BIT(3)) & SENSOR_LOW_EIGHT_BIT);
			index++;
			cmd[cmd_num++] =
			    (uint8_t) (data & SENSOR_LOW_EIGHT_BIT);
			index++;
		} else {
			cmd[cmd_num++] = (uint8_t) data;
			index++;
		}

		if (SENSOR_WRITE_DELAY != subaddr) {
			for (i = 0; i < SENSOR_I2C_OP_TRY_NUM; i++) {
				msg_w.addr = this_client->addr;
				msg_w.flags = 0;
				msg_w.buf = cmd;
				msg_w.len = index;
				ret =
				    i2c_transfer(this_client->adapter, &msg_w,
						 1);
				if (ret != 1) {
					SENSOR_PRINT_ERR
					    ("SENSOR: write sensor reg fai, ret : %d, I2C w addr: 0x%x, \n",
					     ret, this_client->addr);
					continue;
				} else {
					/*SENSOR_PRINT("SENSOR: IIC write reg OK! 0x%04x, val:0x%04x ", subaddr, data); */
					ret = 0;
					break;
				}
			}
		} else {
		       if(SENSOR_WRITE_DELAY != data){
			    msleep(10*data);
                        SENSOR_PRINT("SENSOR: IIC write Delay %d ms \n", data);
                  }

		}
	}
	return 0;
}

uint16_t Sensor_ReadReg(uint16_t reg_addr)
{
	uint32_t i = 0;
	uint8_t cmd[2] = { 0 };
	uint16_t ret_val;
	uint16_t w_cmd_num = 0;
	uint16_t r_cmd_num = 0;
	uint8_t buf_r[2] = { 0 };
	int32_t ret = -1;
	struct i2c_msg msg_r[2];
	SENSOR_IOCTL_FUNC_PTR read_reg_func;

	SENSOR_PRINT("Read:this_client->addr=0x%x\n", this_client->addr);

	read_reg_func = s_sensor_info_ptr->ioctl_func_tab_ptr->read_reg;

	if (PNULL != read_reg_func) {
		ret_val = (uint16_t)
		    read_reg_func((uint32_t)
				  (reg_addr & SENSOR_LOW_SIXTEEN_BIT));
	} else {
		if (SENSOR_I2C_REG_16BIT ==
		    (s_sensor_info_ptr->reg_addr_value_bits &
		     SENSOR_I2C_REG_16BIT)) {
			cmd[w_cmd_num++] =
			    (uint8_t) ((reg_addr >> BIT(3)) &
				       SENSOR_LOW_EIGHT_BIT);
			cmd[w_cmd_num++] =
			    (uint8_t) (reg_addr & SENSOR_LOW_EIGHT_BIT);
		} else {
			cmd[w_cmd_num++] = (uint8_t) reg_addr;
		}

		if (SENSOR_I2C_VAL_16BIT ==
		    (s_sensor_info_ptr->reg_addr_value_bits &
		     SENSOR_I2C_VAL_16BIT)) {
			r_cmd_num = SENSOR_CMD_BITS_16;
		} else {
			r_cmd_num = SENSOR_CMD_BITS_8;
		}

		for (i = 0; i < SENSOR_I2C_OP_TRY_NUM; i++) {
			msg_r[0].addr = this_client->addr;
			msg_r[0].flags = 0;
			msg_r[0].buf = cmd;
			msg_r[0].len = w_cmd_num;
			msg_r[1].addr = this_client->addr;
			msg_r[1].flags = I2C_M_RD;
			msg_r[1].buf = buf_r;
			msg_r[1].len = r_cmd_num;
			ret = i2c_transfer(this_client->adapter, msg_r, 2);
			if (ret != 2) {
				SENSOR_PRINT_ERR
				    ("SENSOR: read sensor reg fai, ret : %d, I2C w addr: 0x%x, \n",
				     ret, this_client->addr);
				msleep(20);
				ret_val = 0xFFFF;
			} else {
				ret_val = (r_cmd_num == 1) ? (uint16_t) buf_r[0]
				    : (uint16_t) ((buf_r[0] << 8) + buf_r[1]);
				break;
			}
		}
	}
	return ret_val;
}

int32_t Sensor_WriteReg_8bits(uint16_t reg_addr, uint8_t value)
{
	uint8_t buf_w[2];
	int32_t ret = -1;
	struct i2c_msg msg_w;

	if (0xFFFF == reg_addr) {
		mdelay(value);
		SENSOR_PRINT("Sensor_WriteReg_8bits wait %d ms.\n", value);
		return 0;
	}

	buf_w[0] = (uint8_t) reg_addr;
	buf_w[1] = value;
	msg_w.addr = this_client->addr;
	msg_w.flags = 0;
	msg_w.buf = buf_w;
	msg_w.len = 2;
	ret = i2c_transfer(this_client->adapter, &msg_w, 1);
	if (ret != 1) {
		SENSOR_PRINT_ERR
		    ("#DCAM: write sensor reg fai, ret : %d, I2C w addr: 0x%x, \n",
		     ret, this_client->addr);
		return -1;
	}
	return 0;
}

int32_t Sensor_ReadReg_8bits(uint8_t reg_addr, uint8_t * reg_val)
{
	uint8_t buf_w[1];
	uint8_t buf_r;
	int32_t ret = -1;
	struct i2c_msg msg_r[2];

	buf_w[0] = reg_addr;
	msg_r[0].addr = this_client->addr;
	msg_r[0].flags = 0;
	msg_r[0].buf = buf_w;
	msg_r[0].len = 1;
	msg_r[1].addr = this_client->addr;
	msg_r[1].flags = I2C_M_RD;
	msg_r[1].buf = &buf_r;
	msg_r[1].len = 1;
	ret = i2c_transfer(this_client->adapter, msg_r, 2);
	if (ret != 2) {
		SENSOR_PRINT_ERR
		    ("#sensor: read sensor reg fail, ret: %d, I2C r addr: 0x%x \n",
		     ret, this_client->addr);
		return -1;
	}
	*reg_val = buf_r;
	return ret;
}

ERR_SENSOR_E Sensor_SendRegTabToSensor(SENSOR_REG_TAB_INFO_T *
				       sensor_reg_tab_info_ptr)
{
	uint32_t i;
	struct timeval time1, time2;
	SENSOR_PRINT("SENSOR: Sensor_SendRegTabToSensor E.\n");

	do_gettimeofday(&time1);

	for (i = 0; i < sensor_reg_tab_info_ptr->reg_count; i++) {
		Sensor_WriteReg(sensor_reg_tab_info_ptr->
				sensor_reg_tab_ptr[i].reg_addr,
				sensor_reg_tab_info_ptr->
				sensor_reg_tab_ptr[i].reg_value);
	}
	do_gettimeofday(&time2);
	SENSOR_PRINT
	    ("SENSOR: Sensor_SendRegValueToSensor -> reg_count = %d, g_is_main_sensor: %d.\n",
	     sensor_reg_tab_info_ptr->reg_count, g_is_main_sensor);
	SENSOR_PRINT("SENSOR use new time sec: %ld, usec: %ld.\n", time1.tv_sec,
		     time1.tv_usec);
	SENSOR_PRINT("SENSOR use old time sec: %ld, usec: %ld.\n", time2.tv_sec,
		     time2.tv_usec);

	SENSOR_PRINT("SENSOR: Sensor_SendRegTabToSensor X.\n");

	return SENSOR_SUCCESS;
}

LOCAL void _Sensor_CleanInformation(void)
{
	SENSOR_REGISTER_INFO_T_PTR sensor_register_info_ptr =
	    s_sensor_register_info_ptr;

	s_sensor_info_ptr = PNULL;
	s_sensor_init = SENSOR_FALSE;
	SENSOR_MEMSET(&s_sensor_exp_info, 0x00, sizeof(s_sensor_exp_info));
	sensor_register_info_ptr->cur_id = SENSOR_ID_MAX;
	return;
}

LOCAL int _Sensor_SetId(SENSOR_ID_E sensor_id)
{
	SENSOR_REGISTER_INFO_T_PTR sensor_register_info_ptr =
	    s_sensor_register_info_ptr;

	sensor_register_info_ptr->cur_id = sensor_id;
	SENSOR_PRINT_HIGH
	    ("_Sensor_SetId:sensor_id=%d,g_is_register_sensor=%d,g_is_main_sensor=%d \n",
	     sensor_id, g_is_register_sensor, g_is_main_sensor);

	if (1 == g_is_register_sensor) {
		if ((SENSOR_MAIN == sensor_id) && (1 == g_is_main_sensor))
			return SENSOR_SUCCESS;
		if ((SENSOR_SUB == sensor_id) && (0 == g_is_main_sensor))
			return SENSOR_SUCCESS;
	}
	if ((SENSOR_MAIN == sensor_id) || (SENSOR_SUB == sensor_id)) {
		if (SENSOR_SUB == sensor_id) {
			sensor_i2c_driver.driver.name = SENSOR_MAIN_I2C_NAME;
			sensor_i2c_driver.id_table = sensor_main_id;
			sensor_i2c_driver.address_list =
			    &sensor_main_default_addr_list[0];
			if ((1 == g_is_register_sensor)
			    && (1 == g_is_main_sensor)) {
				i2c_del_driver(&sensor_i2c_driver);
			}
			g_is_main_sensor = 0;
			sensor_i2c_driver.driver.name = SENSOR_SUB_I2C_NAME;
			sensor_i2c_driver.id_table = sensor_sub_id;
			sensor_i2c_driver.address_list =
			    &sensor_sub_default_addr_list[0];
		} else if (SENSOR_MAIN == sensor_id) {
			sensor_i2c_driver.driver.name = SENSOR_SUB_I2C_NAME;
			sensor_i2c_driver.id_table = sensor_sub_id;
			sensor_i2c_driver.address_list =
			    &sensor_sub_default_addr_list[0];
			if ((1 == g_is_register_sensor)
			    && (0 == g_is_main_sensor)) {
				i2c_del_driver(&sensor_i2c_driver);
			}
			g_is_main_sensor = 1;
			sensor_i2c_driver.driver.name = SENSOR_MAIN_I2C_NAME;
			sensor_i2c_driver.id_table = sensor_main_id;
			sensor_i2c_driver.address_list =
			    &sensor_main_default_addr_list[0];
		}

		if (i2c_add_driver(&sensor_i2c_driver)) {
			SENSOR_PRINT_HIGH("SENSOR: add I2C driver error\n");
			return SENSOR_FAIL;
		} else {
			SENSOR_PRINT_HIGH("SENSOR: add I2C driver OK.\n");
			g_is_register_sensor = 1;
		}
	}
	return SENSOR_SUCCESS;
}

SENSOR_ID_E Sensor_GetCurId(void)
{
	SENSOR_REGISTER_INFO_T_PTR sensor_register_info_ptr =
	    s_sensor_register_info_ptr;
	SENSOR_PRINT("Sensor_GetCurId,sensor_id =%d \n",
	       sensor_register_info_ptr->cur_id);
	return (SENSOR_ID_E) sensor_register_info_ptr->cur_id;
}

uint32_t Sensor_SetCurId(SENSOR_ID_E sensor_id)
{
	SENSOR_PRINT("Sensor_SetCurId : %d.\n", sensor_id);
	if (sensor_id >= SENSOR_ID_MAX) {
		_Sensor_CleanInformation();
		return SENSOR_FAIL;
	}
	if (SENSOR_SUCCESS != _Sensor_SetId(sensor_id)) {
		SENSOR_PRINT("SENSOR: Fail to Sensor_SetCurId.\n");
		return SENSOR_FAIL;
	}
	return SENSOR_SUCCESS;
}

SENSOR_REGISTER_INFO_T_PTR Sensor_GetRegisterInfo(void)
{
	return s_sensor_register_info_ptr;
}

LOCAL void _Sensor_I2CInit(SENSOR_ID_E sensor_id)
{
	SENSOR_REGISTER_INFO_T_PTR sensor_register_info_ptr =
	    s_sensor_register_info_ptr;
	SENSOR_INFO_T** sensor_info_tab_ptr = PNULL;
	SENSOR_INFO_T* sensor_info_ptr= PNULL;
 	uint32_t i2c_clock = 100000;
	uint32_t set_i2c_clock = 0;
	sensor_register_info_ptr->cur_id = sensor_id;

	if (0 == g_is_register_sensor) {
		if ((SENSOR_MAIN == sensor_id) || (SENSOR_SUB == sensor_id)) {
			if (SENSOR_MAIN == sensor_id) {
				SENSOR_PRINT
				    ("_Sensor_I2CInit,sensor_main_force[1] =%d \n",
				     sensor_main_force[1]);
				sensor_i2c_driver.driver.name =
				    SENSOR_MAIN_I2C_NAME;
				sensor_i2c_driver.id_table = sensor_main_id;
				sensor_i2c_driver.address_list =
				    &sensor_main_default_addr_list[0];
			} else if (SENSOR_SUB == sensor_id) {
				SENSOR_PRINT
				    ("_Sensor_I2CInit,sensor_sub_force[1] =%d \n",
				     sensor_sub_force[1]);
				sensor_i2c_driver.driver.name =
				    SENSOR_SUB_I2C_NAME;
				sensor_i2c_driver.id_table = sensor_sub_id;
				sensor_i2c_driver.address_list =
				    &sensor_sub_default_addr_list[0];
			}

			if (i2c_add_driver(&sensor_i2c_driver)) {
				SENSOR_PRINT_ERR
				    ("SENSOR: add I2C driver error\n");
				return;
			} else {
				SENSOR_PRINT_ERR
				    ("SENSOR: add I2C driver OK.\n");
#if 0
				sensor_info_tab_ptr=(SENSOR_INFO_T**)Sensor_GetInforTab(sensor_id);
				if(sensor_info_tab_ptr)
				{
					sensor_info_ptr = sensor_info_tab_ptr[sensor_id];
				}
				if(sensor_info_ptr)
				{
					set_i2c_clock = sensor_info_ptr->reg_addr_value_bits & SENSOR_I2C_CLOCK_MASK;
					if(SENSOR_I2C_FREQ_100 != set_i2c_clock)
					{
						if(SENSOR_I2C_FREQ_400 == set_i2c_clock)
						{
							sc8810_i2c_set_clk(SENSOR_I2C_ID,400000);					
						}
						else if(SENSOR_I2C_FREQ_200 == set_i2c_clock)
						{
							sc8810_i2c_set_clk(SENSOR_I2C_ID,200000);
						}
						else if(SENSOR_I2C_FREQ_50 == set_i2c_clock)
						{
							sc8810_i2c_set_clk(SENSOR_I2C_ID,50000);
						}
						else if(SENSOR_I2C_FREQ_20 == set_i2c_clock)
						{
							sc8810_i2c_set_clk(SENSOR_I2C_ID,20000);
						}
					}
				}
#endif
				g_is_register_sensor = 1;
			}
		}
	} else {
		SENSOR_PRINT_ERR("Sensor: Init I2c  %d fail!\n", sensor_id);
	}
	SENSOR_PRINT_ERR
	    ("_Sensor_I2CInit,sensor_id=%d,g_is_register_sensor=%d\n",
	     sensor_id, g_is_register_sensor);
}

LOCAL int _Sensor_I2CDeInit(SENSOR_ID_E sensor_id)
{
	if (1 == g_is_register_sensor) {
		if ((SENSOR_MAIN == sensor_id) || (SENSOR_SUB == sensor_id)) {
			if (SENSOR_MAIN == sensor_id) {
				sensor_i2c_driver.driver.name =
				    SENSOR_MAIN_I2C_NAME;
				sensor_i2c_driver.id_table = sensor_main_id;
				sensor_i2c_driver.address_list =
				    &sensor_main_default_addr_list[0];

				i2c_del_driver(&sensor_i2c_driver);
				g_is_register_sensor = 0;
				SENSOR_PRINT
				    ("SENSOR: delete  I2C  %d driver OK.\n",
				     sensor_id);
			} else if (SENSOR_SUB == sensor_id) {
				sensor_i2c_driver.driver.name =
				    SENSOR_SUB_I2C_NAME;
				sensor_i2c_driver.id_table = sensor_sub_id;
				sensor_i2c_driver.address_list =
				    &sensor_sub_default_addr_list[0];
				i2c_del_driver(&sensor_i2c_driver);
				g_is_register_sensor = 0;
				SENSOR_PRINT
				    ("SENSOR: delete  I2C  %d driver OK.\n",
				     sensor_id);
			}
		}
	} else {
		SENSOR_PRINT("SENSOR: delete  I2C  %d driver OK.\n",
			     SENSOR_ID_MAX);
	}
	return SENSOR_SUCCESS;
}

LOCAL void _Sensor_Identify(SENSOR_ID_E sensor_id)
{
	uint32_t sensor_index = 0;
	SENSOR_INFO_T *sensor_info_ptr = PNULL;
	struct list_head *sensor_list = PNULL;
	struct sensor_drv_cfg *cfg;
	uint32_t i = 0;
	uint32_t get_cfg_flag = 0;

	SENSOR_PRINT_HIGH("SENSOR: sensor identifing %d \n", sensor_id);

	if (SCI_TRUE == s_sensor_register_info_ptr->is_register[sensor_id]) {
		SENSOR_PRINT("SENSOR: sensor identified \n");
		return;
	}
	sensor_list = Sensor_GetList(sensor_id);
	if(s_sensor_identified && (5 != sensor_id)) {
		sensor_index = s_sensor_index[sensor_id];
		SENSOR_PRINT("_Sensor_Identify:sensor_index=%d.\n",sensor_index);
		if(0xFF != sensor_index) {
			list_for_each_entry(cfg, sensor_list, list) {
				if(sensor_index == i) {
					get_cfg_flag = 1;
					SENSOR_PRINT("Sensor_Identify:get index from list is %d.\n",sensor_index);
					break;
				} else {
					i++;
				}
			}
			if(1 != get_cfg_flag) {
				SENSOR_PRINT_ERR("SENSOR: index %d cfg is null", sensor_index);
				goto IDENTIFY_SEARCH;
			}
			sensor_info_ptr = cfg->driver_info;
			if(NULL==sensor_info_ptr)
			{
				SENSOR_PRINT_ERR("SENSOR: %d info of Sensor_Init table %d is null", sensor_index, (uint)sensor_id);
				goto IDENTIFY_SEARCH;
			}
			_Sensor_I2CInit(sensor_id);
			s_sensor_info_ptr = sensor_info_ptr;
			Sensor_PowerOn(SCI_TRUE);
			if(PNULL!=sensor_info_ptr->ioctl_func_tab_ptr->identify)
			{
				this_client->addr = (this_client->addr & (~0xFF)) | (s_sensor_info_ptr->salve_i2c_addr_w & 0xFF);
				SENSOR_PRINT_ERR("SENSOR:identify  Sensor 01\n");
				if(SENSOR_SUCCESS==sensor_info_ptr->ioctl_func_tab_ptr->identify(SENSOR_ZERO_I2C))
				{
					s_sensor_list_ptr[sensor_id]=sensor_info_ptr;
					s_sensor_register_info_ptr->is_register[sensor_id]=SCI_TRUE;
					s_sensor_register_info_ptr->img_sensor_num++;
					//fix fisrt time power on drop time issue
					//Sensor_PowerOn(SCI_FALSE);
					SENSOR_PRINT_HIGH("_Sensor_Identify:sensor_id :%d,img_sensor_num=%d\n",
						                                     sensor_id,s_sensor_register_info_ptr->img_sensor_num);
				}
				else
				{
					Sensor_PowerOn(SCI_FALSE);
					_Sensor_I2CDeInit(sensor_id);
					SENSOR_PRINT_ERR("_Sensor_Identify:identify fail!.\n");
					goto IDENTIFY_SEARCH;
				}
			}
			else
			{
				Sensor_PowerOn(SCI_FALSE);
			}
			_Sensor_I2CDeInit(sensor_id);
			return;
		}
	}
IDENTIFY_SEARCH:
	SENSOR_PRINT("_Sensor_Identify:search.\n");
	sensor_index = 0;
	sensor_list = Sensor_GetList(sensor_id);
	_Sensor_I2CInit(sensor_id);
	list_for_each_entry(cfg, sensor_list, list) {
		sensor_info_ptr = cfg->driver_info;

		if (NULL == sensor_info_ptr) {
			sensor_index++;
			SENSOR_PRINT_ERR
			    ("SENSOR: %d info of Sensor_Init table %d is null",
			     sensor_index, (uint) sensor_id);
			continue;
		}
		s_sensor_info_ptr = sensor_info_ptr;
		Sensor_PowerOn(SCI_TRUE);
		SENSOR_PRINT_ERR
		    ("SENSOR: Sensor_PowerOn done,this_client=0x%x\n",
		     (uint32_t) this_client);
		SENSOR_PRINT_ERR("SENSOR: identify ptr =0x%x\n",
				 (uint32_t)
				 sensor_info_ptr->ioctl_func_tab_ptr->identify);

		if (PNULL != sensor_info_ptr->ioctl_func_tab_ptr->identify) {
			SENSOR_PRINT
			    ("SENSOR:identify  Sensor 00:this_client=0x%x,this_client->addr=0x%x,0x%x\n",
			     (uint32_t) this_client,
			     (uint32_t) & this_client->addr, this_client->addr);

			if (5 != Sensor_GetCurId())
				this_client->addr =
				    (this_client->addr & (~0xFF)) |
				    (s_sensor_info_ptr->salve_i2c_addr_w &
				     0xFF);
			SENSOR_PRINT_ERR("SENSOR:identify  Sensor 01\n");
			if (SENSOR_SUCCESS ==
				sensor_info_ptr-> ioctl_func_tab_ptr->identify(SENSOR_ZERO_I2C)) {
				s_sensor_list_ptr[sensor_id] = sensor_info_ptr;
				s_sensor_register_info_ptr->is_register[sensor_id] = SCI_TRUE;
				if(5 != Sensor_GetCurId())//test by wang bonnie
					s_sensor_index[sensor_id] = sensor_index;
				s_sensor_register_info_ptr->img_sensor_num++;
				//ImgSensor_PutMutex();
				//fix fisrt time power on drop time issue
				//Sensor_PowerOn(SCI_FALSE);
				SENSOR_PRINT_HIGH ("_Sensor_Identify:sensor_id :%d,img_sensor_num=%d,sensor_index=%d.\n",
				     sensor_id,
				     s_sensor_register_info_ptr->img_sensor_num,sensor_index);
				break;
			}
		}
		Sensor_PowerOn(SCI_FALSE);
		sensor_index++;
	}
	_Sensor_I2CDeInit(sensor_id);
	if (SCI_TRUE == s_sensor_register_info_ptr->is_register[sensor_id]) {
		SENSOR_PRINT_HIGH("SENSOR TYPE of %d indentify OK.\n", (uint32_t) sensor_id);
		s_sensor_param_saved = SCI_TRUE;
	} else {
		SENSOR_PRINT_HIGH("SENSOR TYPE of %d indentify FAILURE.\n",
				  (uint32_t) sensor_id);
	}
}


LOCAL uint32_t _Sensor_Register(SENSOR_ID_E sensor_id)
{
	uint32_t ret_val = SENSOR_FAIL;
	uint32_t sensor_index = 0;
	SENSOR_INFO_T *sensor_info_ptr = PNULL;
	struct list_head *sensor_list = PNULL;
	struct sensor_drv_cfg *cfg;
	uint32_t i = 0;
	uint32_t get_cfg_flag = 0;

	SENSOR_PRINT_HIGH(" _Sensor_Register %d", sensor_id);

	if (SCI_TRUE == s_sensor_register_info_ptr->is_register[sensor_id]) {
		SENSOR_PRINT("SENSOR: sensor identified \n");
		return;
	}
	sensor_list = Sensor_GetList(sensor_id);
	if(s_sensor_identified && (5 != sensor_id)) {
		sensor_index = s_sensor_index[sensor_id];
		SENSOR_PRINT("_Sensor_Register:sensor_index=%d.\n",sensor_index);
		if(0xFF != sensor_index) {
			list_for_each_entry(cfg, sensor_list, list) {
				if(sensor_index == i) {
					get_cfg_flag = 1;
					SENSOR_PRINT("_Sensor_Register:get index from list is %d.\n",sensor_index);
					break;
				} else {
					i++;
				}
			}
			if(1 != get_cfg_flag) {
				SENSOR_PRINT_ERR("_Sensor_Register: index %d cfg is null", sensor_index);
				return SENSOR_FAIL;
			}
			sensor_info_ptr = cfg->driver_info;
			if(NULL==sensor_info_ptr)
			{
				SENSOR_PRINT_ERR("_Sensor_Register: %d info of Sensor_Init table %d is null", sensor_index, (uint)sensor_id);
				return SENSOR_FAIL;
			}
			s_sensor_list_ptr[sensor_id]=sensor_info_ptr;
			s_sensor_register_info_ptr->is_register[sensor_id]=SCI_TRUE;
			s_sensor_register_info_ptr->img_sensor_num++;

			}
		}

	
	return SENSOR_SUCCESS;
	
}



LOCAL void _Sensor_SetStatus(SENSOR_ID_E sensor_id)
{
	uint32_t i = 0;
	SENSOR_REGISTER_INFO_T_PTR sensor_register_info_ptr = 0;//
	uint32_t rst_lvl = 0 ;


	//pwdn all the sensor to avoid confilct as the sensor output
	for (i = 0; i <= SENSOR_SUB; i++) {
		if (SENSOR_TRUE == s_sensor_register_info_ptr->is_register[i]) {
			_Sensor_SetId(i);
			s_sensor_info_ptr = s_sensor_list_ptr[i];
			rst_lvl = s_sensor_info_ptr->reset_pulse_level;
			if (5 != Sensor_GetCurId())
				this_client->addr =
				    (this_client->addr & (~0xFF)) |
				    (s_sensor_info_ptr->salve_i2c_addr_w &
				     0xFF);

			Sensor_PowerDown((BOOLEAN)s_sensor_info_ptr->power_down_level);
			SENSOR_PRINT_HIGH("SENSOR: Sensor_sleep of id %d \n", i);
		}
	}

	//Give votage according the target sensor
	//For dual sensor solution, the dual sensor should share all the power.
	Sensor_PowerOn_Ex(sensor_id);

	//reset other sensor and pwn again
	//may reset is not useless but still keep there
	for (i = 0; i <= SENSOR_SUB; i++) {
		if (i == sensor_id) {
			continue;
		}
		if (SENSOR_TRUE == s_sensor_register_info_ptr->is_register[i]) {
			_Sensor_SetId(i);
			s_sensor_info_ptr = s_sensor_list_ptr[i];
			rst_lvl = s_sensor_info_ptr->reset_pulse_level;
			if (5 != Sensor_GetCurId())
				this_client->addr =
				    (this_client->addr & (~0xFF)) |
				    (s_sensor_info_ptr->salve_i2c_addr_w &
				     0xFF);

			//Sensor_PowerOn(SENSOR_TRUE);
			//Sensor_SetExportInfo(&s_sensor_exp_info);
			Sensor_PowerDown((BOOLEAN)!s_sensor_info_ptr->power_down_level);
			Sensor_QReset(rst_lvl);
			Sensor_PowerDown((BOOLEAN)s_sensor_info_ptr->power_down_level);

			SENSOR_PRINT_HIGH("SENSOR: Sensor_sleep of id %d \n", i);
		}
	}

	_Sensor_SetId(sensor_id);
	s_sensor_info_ptr = s_sensor_list_ptr[sensor_id];

	//reset target sensor. and make normal.
	Sensor_Reset_EX((BOOLEAN)s_sensor_info_ptr->power_down_level, s_sensor_info_ptr->reset_pulse_level);
	Sensor_SetExportInfo(&s_sensor_exp_info);
}

LOCAL uint32_t _sensor_com_init(uint32_t sensor_id, SENSOR_REGISTER_INFO_T_PTR sensor_register_info_ptr )
{
	uint32_t ret_val = SENSOR_FAIL;
	
	if (SENSOR_TRUE == sensor_register_info_ptr->is_register[sensor_id]) {


		_Sensor_SetStatus(sensor_id);
		s_sensor_init = SENSOR_TRUE;

		if (5 != Sensor_GetCurId())
			this_client->addr =(this_client->addr & (~0xFF)) |(s_sensor_info_ptr->salve_i2c_addr_w & 0xFF);
		SENSOR_PRINT("_sensor_com_init: sensor_id :%d,addr=0x%x \n", sensor_id, this_client->addr);

		//confirm camera identify OK
		if(SENSOR_SUCCESS != s_sensor_info_ptr->ioctl_func_tab_ptr->identify(SENSOR_ZERO_I2C)){
			sensor_register_info_ptr->is_register[sensor_id] = SENSOR_FALSE;
			SENSOR_PRINT("SENSOR: Sensor_Open: sensor identify not correct!! \n");
			return SENSOR_FAIL;
		}

		ret_val = SENSOR_SUCCESS;
		if (SENSOR_SUCCESS != Sensor_SetMode(SENSOR_MODE_COMMON_INIT)) {
			SENSOR_PRINT_ERR("Sensor: _sensor_com_init set init mode error!\n");
			ret_val = SENSOR_FAIL;
		}
		s_sensor_init = SENSOR_TRUE;
		//SENSOR_PRINT("SENSOR: Sensor_Init  Success \n");
	}
	else {
		SENSOR_PRINT_ERR("_sensor_com_init  fail,sensor_id = %d \n", sensor_id);
	}

	return ret_val;


}

uint32_t Sensor_Init(uint32_t sensor_id)
{
	uint32_t ret_val = SENSOR_FAIL;
	BOOLEAN reg_flag =  SCI_TRUE;
	SENSOR_REGISTER_INFO_T_PTR sensor_register_info_ptr =
	    s_sensor_register_info_ptr;

	SENSOR_PRINT("SENSOR: Sensor_Init, sensor_id: %d.\n", sensor_id);

	if (Sensor_IsInit()) {
		SENSOR_PRINT("SENSOR: Sensor_Init is done\n");
		return SENSOR_SUCCESS;
	}
	_Sensor_CleanInformation();

	if(s_sensor_identified)
	{
		SENSOR_PRINT("SENSOR:  Sensor_Register\n");
		_Sensor_Register(SENSOR_MAIN);
		_Sensor_Register(SENSOR_SUB);

		s_sensor_identified = SCI_TRUE;
		if (5 == sensor_id) {
			msleep(20);
			_Sensor_Identify(SENSOR_ATV);
		}
		ret_val = _sensor_com_init(sensor_id, sensor_register_info_ptr);
	}

	if(SENSOR_FAIL == ret_val)
	{
		_Sensor_Identify(SENSOR_MAIN);
		_Sensor_Identify(SENSOR_SUB);
		SENSOR_PRINT("SENSOR: Sensor_Init Identify \n");

		if (5 == sensor_id||!s_sensor_identified) {
			msleep(20);
			_Sensor_Identify(SENSOR_ATV);
		}

		s_sensor_identified = SCI_TRUE;

		ret_val = _sensor_com_init(sensor_id, sensor_register_info_ptr);
	}


	SENSOR_PRINT("Sensor_Init: end init: %d \n", ret_val);
	return ret_val;
}

BOOLEAN Sensor_IsInit(void)
{
	return s_sensor_init;
}




ERR_SENSOR_E Sensor_SetModeCamcorder()
{
    SENSOR_PRINT("SENSOR: Sensor_SetModeCamcorder .\n");

    if (SENSOR_FALSE == Sensor_IsInit()) {
		SENSOR_PRINT
		    ("SENSOR: Sensor_SetModeCamcorder -> sensor has not init \n");
		return SENSOR_OP_STATUS_ERR;
        }

#ifdef CONFIG_LOAD_FILE
	if(sensor_regs_table != NULL)
            Sensor_regs_table_write("HI253_video_mode_nand_tab");
#else
    Sensor_SendRegTabToSensor
		    (&s_sensor_info_ptr->resolution_tab_info_ptr[SENSOR_MODE_PREVIEW_TWO]);
#endif

    return SENSOR_SUCCESS;
}

ERR_SENSOR_E Sensor_SetMode(SENSOR_MODE_E mode)
{
	uint32_t mclk;
	SENSOR_IOCTL_FUNC_PTR set_reg_tab_func=s_sensor_info_ptr->ioctl_func_tab_ptr->cus_func_1;

	SENSOR_PRINT("SENSOR: Sensor_SetMode -> mode = %d.\n", mode);
	if (SENSOR_FALSE == Sensor_IsInit()) {
		SENSOR_PRINT
		    ("SENSOR: Sensor_SetResolution -> sensor has not init \n");
		return SENSOR_OP_STATUS_ERR;
	}

	if (s_sensor_mode[Sensor_GetCurId()] == mode) {
		SENSOR_PRINT("SENSOR: The sensor mode as before \n");
		return SENSOR_SUCCESS;
	}

	if (PNULL !=
	    s_sensor_info_ptr->
	    resolution_tab_info_ptr[mode].sensor_reg_tab_ptr) {
		mclk =
		    s_sensor_info_ptr->
		    resolution_tab_info_ptr[mode].xclk_to_sensor;
		Sensor_SetMCLK(mclk);
		s_sensor_exp_info.image_format =
		    s_sensor_exp_info.sensor_mode_info[mode].image_format;

#ifdef CONFIG_LOAD_FILE
	int32_t result = Sensor_regs_table_init();
	if (result > 0)
	{		
		SENSOR_PRINT(KERN_ERR "***** HI253_regs_table_init  FAILED. Check the Filie in MMC\n");
		return result;
	}
	result =0;
	
           if(SENSOR_MODE_COMMON_INIT == mode)
				Sensor_regs_table_write("HI253_sensor_ini");
	       else  if(SENSOR_MODE_PREVIEW_ONE == mode)
				Sensor_regs_table_write("HI253_camera_mode");
          else  if(SENSOR_MODE_SNAPSHOT_ONE_FIRST == mode)
				Sensor_regs_table_write("HI253_YUV_640X480");
           else if (SENSOR_MODE_SNAPSHOT_ONE_SECOND == mode)
				Sensor_regs_table_write("HI253_YUV_1280X960");
		   else if (SENSOR_MODE_SNAPSHOT_ONE_THIRD == mode)
				Sensor_regs_table_write("HI253_YUV_1600X960");
		   else if (SENSOR_MODE_SNAPSHOT_ONE_FOURTH == mode)
				Sensor_regs_table_write("HI253_YUV_1600X1200");
		   else if (SENSOR_MODE_PREVIEW_TWO == mode)
				Sensor_regs_table_write("HI253_video_mode_nand_tab");

#else
		Sensor_SendRegTabToSensor
		    (&s_sensor_info_ptr->resolution_tab_info_ptr[mode]);
#endif
		//confirm camera identify OK
		if(SENSOR_SUCCESS != s_sensor_info_ptr->ioctl_func_tab_ptr->identify(SENSOR_ZERO_I2C)){
			SENSOR_PRINT("SENSOR: sensor identify not correct!! \n");
			return SENSOR_OP_STATUS_ERR;
		}

	} else {
		if(set_reg_tab_func)
			set_reg_tab_func(0);
		SENSOR_PRINT
		    ("SENSOR: Sensor_SetResolution -> No this resolution information !!! \n");
	}

	s_sensor_mode[Sensor_GetCurId()] = mode;

	return SENSOR_SUCCESS;
}

uint32_t Sensor_Ioctl(uint32_t cmd, uint32_t arg)
{
	SENSOR_IOCTL_FUNC_PTR func_ptr;
	SENSOR_IOCTL_FUNC_TAB_T *func_tab_ptr;
	uint32_t temp;
	uint32_t ret_value = SENSOR_SUCCESS;

	SENSOR_PRINT("SENSOR: Sensor_Ioctl -> cmd = %d, arg = %d.\n", cmd, arg);

	if (!Sensor_IsInit()) {
		SENSOR_PRINT("SENSOR: Sensor_Ioctl -> sensor has not init.\n");
		return SENSOR_OP_STATUS_ERR;
	}

	if (SENSOR_IOCTL_CUS_FUNC_1 > cmd) {
		SENSOR_PRINT
		    ("SENSOR: Sensor_Ioctl - > can't access internal command !\n");
		return SENSOR_SUCCESS;
	}
	func_tab_ptr = s_sensor_info_ptr->ioctl_func_tab_ptr;
	temp = *(uint32_t *) ((uint32_t) func_tab_ptr + cmd * BIT(2));
	func_ptr = (SENSOR_IOCTL_FUNC_PTR) temp;

	if (PNULL != func_ptr) {
		ImgSensor_GetMutex();
		ret_value = func_ptr(arg);
		ImgSensor_PutMutex();
	} else {
		SENSOR_PRINT
		    ("SENSOR: Sensor_Ioctl -> the ioctl function has not register err!\n");
	}
	return ret_value;
}

SENSOR_EXP_INFO_T *Sensor_GetInfo(void)
{
	if (!Sensor_IsInit()) {
		SENSOR_PRINT("SENSOR: Sensor_GetInfo -> sensor has not init \n");
		return PNULL;
	}
	return &s_sensor_exp_info;
}

ERR_SENSOR_E Sensor_Close(void)
{
	SENSOR_PRINT("SENSOR: Sensor_close \n");

	if (1 == g_is_register_sensor) {
		if (1 == g_is_main_sensor) {
			sensor_i2c_driver.address_list =
			    &sensor_main_default_addr_list[0];
		} else {
			sensor_i2c_driver.address_list =
			    &sensor_sub_default_addr_list[0];
		}
		i2c_del_driver(&sensor_i2c_driver);
		g_is_register_sensor = 0;
		g_is_main_sensor = 0;
	}

	if (SENSOR_TRUE == Sensor_IsInit()) {
		Sensor_PowerOn(SENSOR_FALSE);
		if (SENSOR_MAIN == Sensor_GetCurId()) {
			SENSOR_PRINT_HIGH("SENSOR: Sensor_close 0.\n");
			if (SCI_TRUE ==
			    s_sensor_register_info_ptr->is_register[SENSOR_SUB])
			{
				SENSOR_PRINT_HIGH("SENSOR: Sensor_close 1.\n");
				_Sensor_SetId(SENSOR_SUB);
				s_sensor_info_ptr =
				    s_sensor_list_ptr[SENSOR_SUB];
				Sensor_SetExportInfo(&s_sensor_exp_info);
				Sensor_PowerOn(SENSOR_FALSE);
				if (1 == g_is_register_sensor) {
					SENSOR_PRINT_HIGH
					    ("SENSOR: Sensor_close 2.\n");
					sensor_i2c_driver.address_list =
					    &sensor_sub_default_addr_list[0];
					i2c_del_driver(&sensor_i2c_driver);
					g_is_register_sensor = 0;
					g_is_main_sensor = 0;
				}
			}
		} else if (SENSOR_SUB == Sensor_GetCurId()) {
			SENSOR_PRINT_HIGH("SENSOR: Sensor_close 3.\n");
			if (SCI_TRUE ==
			    s_sensor_register_info_ptr->is_register
			    [SENSOR_MAIN]) {
				SENSOR_PRINT_HIGH("SENSOR: Sensor_close 4.\n");
				_Sensor_SetId(SENSOR_MAIN);
				s_sensor_info_ptr =
				    s_sensor_list_ptr[SENSOR_MAIN];
				Sensor_SetExportInfo(&s_sensor_exp_info);
				Sensor_PowerOn(SENSOR_FALSE);
				if (1 == g_is_register_sensor) {
					SENSOR_PRINT_HIGH
					    ("SENSOR: Sensor_close 5.\n");
					sensor_i2c_driver.address_list =
					    &sensor_main_default_addr_list[0];
					i2c_del_driver(&sensor_i2c_driver);
					g_is_register_sensor = 0;
					g_is_main_sensor = 0;
				}
			}
		} else if (SENSOR_ATV == Sensor_GetCurId()) {
			if (SCI_TRUE ==
			    s_sensor_register_info_ptr->is_register
			    [SENSOR_MAIN]) {
				SENSOR_PRINT_HIGH("SENSOR: Sensor_close 4.\n");
				_Sensor_SetId(SENSOR_MAIN);
				s_sensor_info_ptr =
				    s_sensor_list_ptr[SENSOR_MAIN];
				Sensor_SetExportInfo(&s_sensor_exp_info);
				Sensor_PowerOn(SENSOR_FALSE);
				if (1 == g_is_register_sensor) {
					SENSOR_PRINT_HIGH
					    ("SENSOR: Sensor_close 5.\n");
					sensor_i2c_driver.address_list =
					    &sensor_main_default_addr_list[0];
					i2c_del_driver(&sensor_i2c_driver);
					g_is_register_sensor = 0;
					g_is_main_sensor = 0;
				}
			}
			if (SCI_TRUE ==
			    s_sensor_register_info_ptr->is_register[SENSOR_SUB])
			{
				SENSOR_PRINT_HIGH("SENSOR: Sensor_close 1.\n");
				_Sensor_SetId(SENSOR_SUB);
				s_sensor_info_ptr =
				    s_sensor_list_ptr[SENSOR_SUB];
				Sensor_SetExportInfo(&s_sensor_exp_info);
				Sensor_PowerOn(SENSOR_FALSE);
				if (1 == g_is_register_sensor) {
					SENSOR_PRINT_HIGH
					    ("SENSOR: Sensor_close 2.\n");
					sensor_i2c_driver.address_list =
					    &sensor_sub_default_addr_list[0];
					i2c_del_driver(&sensor_i2c_driver);
					g_is_register_sensor = 0;
					g_is_main_sensor = 0;
				}
			}
		}
	}
	SENSOR_PRINT_HIGH("SENSOR: Sensor_close 6.\n");
	s_sensor_init = SENSOR_FALSE;
	s_sensor_mode[SENSOR_MAIN] = SENSOR_MODE_MAX;
	s_sensor_mode[SENSOR_SUB] = SENSOR_MODE_MAX;
	return SENSOR_SUCCESS;
}

uint32_t Sensor_SetSensorType(SENSOR_TYPE_E sensor_type)
{
	s_sensor_type = sensor_type;
	return SENSOR_SUCCESS;
}

ERR_SENSOR_E Sensor_SetTiming(SENSOR_MODE_E mode)
{

#if 1
	uint32_t ret_val = SENSOR_FAIL;
	SENSOR_REGISTER_INFO_T_PTR sensor_register_info_ptr = s_sensor_register_info_ptr;
	SENSOR_ID_E cur_sensor_id = SENSOR_MAIN;
	struct timeval time1, time2;

	do_gettimeofday(&time1);


	SENSOR_PRINT("Sensor_SetTiming  start: sensor_id=%d,  mode=%d \n", Sensor_GetCurId(), mode);

	cur_sensor_id = Sensor_GetCurId();

	Sensor_Close();

	ret_val = _sensor_com_init(cur_sensor_id, sensor_register_info_ptr);
	Sensor_SetMode(mode);

	do_gettimeofday(&time2);

	SENSOR_PRINT("Sensor_SetTiming  end, ret = %d, time=%d ms \n", ret_val,
		(time2.tv_sec-time1.tv_sec)*1000 + (time2.tv_usec-time1.tv_usec)/1000);

	return ret_val;
#else
	uint32_t cur_id = s_sensor_register_info_ptr->cur_id;

	SENSOR_PRINT("SENSOR: Sensor_SetTiming -> mode = %d,sensor_id=%d.\n", mode,
	       cur_id);

	if (PNULL !=
	    s_sensor_info_ptr->
	    resolution_tab_info_ptr[mode].sensor_reg_tab_ptr) {
		/*send register value to sensor */
		Sensor_SendRegTabToSensor
		    (&s_sensor_info_ptr->resolution_tab_info_ptr[mode]);
		s_sensor_mode[Sensor_GetCurId()] = mode;
	} else {
		SENSOR_PRINT
		    ("SENSOR: Sensor_SetResolution -> No this resolution information !!!");
	}

	return SENSOR_SUCCESS;
#endif
}

int Sensor_CheckTiming(SENSOR_MODE_E mode)
{
	SENSOR_REG_TAB_INFO_T *sensor_reg_tab_info_ptr =
	    &s_sensor_info_ptr->resolution_tab_info_ptr[mode];
	uint32_t i = 0;
	uint16_t data = 0;
	uint32_t cur_id = s_sensor_register_info_ptr->cur_id;
	int ret = SENSOR_SUCCESS;

	SENSOR_PRINT("SENSOR: Sensor_CheckTiming -> mode = %d,sensor_id=%d.\n", mode,
	       cur_id);

	if (0 != cur_id)
		return 0;

	for (i = 0; i < sensor_reg_tab_info_ptr->reg_count; i++) {
		if ((0x4202 ==
		     sensor_reg_tab_info_ptr->sensor_reg_tab_ptr[i].reg_addr)
		    || (SENSOR_WRITE_DELAY ==
			sensor_reg_tab_info_ptr->
			sensor_reg_tab_ptr[i].reg_addr))
			continue;
		data =
		    Sensor_ReadReg(sensor_reg_tab_info_ptr->sensor_reg_tab_ptr
				   [i].reg_addr);
		if (data !=
		    sensor_reg_tab_info_ptr->sensor_reg_tab_ptr[i].reg_value) {
			ret = -1;
			SENSOR_PRINT_ERR("SENSOR: Sensor_CheckTiming report error!.\n");
			break;
		}
	}
	SENSOR_PRINT("SENSOR: Sensor_CheckTiming return = %d.\n", ret);
	return ret;
}

uint32_t Sensor_SetFlash(uint32_t flash_mode)
{
	if (s_flash_mode == flash_mode)
		return 0;

	s_flash_mode = flash_mode;

	SENSOR_PRINT("Sensor_SetFlash:flash_mode=0x%x .\n", flash_mode);

	switch (flash_mode) {
	case 1:		/*flash on */
	case 2:		/*for torch */
		/*low light */
		gpio_request(135, "gpio135");
		gpio_direction_output(135, 1);
		gpio_set_value(135, 1);
		gpio_request(144, "gpio144");
		gpio_direction_output(144, 0);
		gpio_set_value(144, 0);
		break;
	case 0x11:
		/*high light */
		gpio_request(135, "gpio135");
		gpio_direction_output(135, 1);
		gpio_set_value(135, 1);

		gpio_request(144, "gpio144");
		gpio_direction_output(144, 1);
		gpio_set_value(144, 1);
		break;
	case 0x10:		/*close flash */
	case 0x0:
		/*close the light */
		gpio_request(135, "gpio135");
		gpio_direction_output(135, 0);
		gpio_set_value(135, 0);
		gpio_request(144, "gpio144");
		gpio_direction_output(144, 0);
		gpio_set_value(144, 0);
		break;
	default:
		SENSOR_PRINT("Sensor_SetFlash unknow mode:flash_mode=%d .\n",
		       flash_mode);
		break;
	}

	return 0;
}

struct i2c_client *Sensor_GetI2CClien(void)
{
	return this_client;
}

LOCAL uint32_t _Sensor_InitDefaultExifInfo(void)
{
	EXIF_SPEC_PIC_TAKING_COND_T* exif_ptr=&s_default_exif;

	memset(&s_default_exif, 0, sizeof(EXIF_SPEC_PIC_TAKING_COND_T));

	SENSOR_PRINT("SENSOR: Sensor_InitDefaultExifInfo \n");

	exif_ptr->valid.FNumber=1;
	exif_ptr->FNumber.numerator=280;
	exif_ptr->FNumber.denominator=100;
	exif_ptr->valid.ExposureProgram=1;
	exif_ptr->ExposureProgram=0x04;
	/*exif_ptr->SpectralSensitivity[MAX_ASCII_STR_SIZE];
	exif_ptr->ISOSpeedRatings;
	exif_ptr->OECF;
	exif_ptr->ShutterSpeedValue;*/
	exif_ptr->valid.ApertureValue=1;
	exif_ptr->ApertureValue.numerator=280;
	exif_ptr->ApertureValue.denominator=100;
	/*exif_ptr->BrightnessValue;
	exif_ptr->ExposureBiasValue;*/
	exif_ptr->valid.MaxApertureValue=1;
	exif_ptr->MaxApertureValue.numerator=280;
	exif_ptr->MaxApertureValue.denominator=100;
	//exif_ptr->SubjectDistance;
	exif_ptr->valid.MeteringMode=1;
	exif_ptr->MeteringMode=2;
	//exif_ptr->LightSource;
	//exif_ptr->Flash;
	exif_ptr->valid.FocalLength=1;
	exif_ptr->FocalLength.numerator=270;
	exif_ptr->FocalLength.denominator=100;
	/*exif_ptr->SubjectArea;
	exif_ptr->FlashEnergy;
	exif_ptr->SpatialFrequencyResponse;
	exif_ptr->FocalPlaneXResolution;
	exif_ptr->FocalPlaneYResolution;
	exif_ptr->FocalPlaneResolutionUnit;
	exif_ptr->SubjectLocation[2];
	exif_ptr->ExposureIndex;
	exif_ptr->SensingMethod;*/
	exif_ptr->valid.FileSource=1;
	exif_ptr->FileSource=0x03;
	/*exif_ptr->SceneType;
	exif_ptr->CFAPattern;
	exif_ptr->CustomRendered;*/
	exif_ptr->valid.ExposureMode=1;
	exif_ptr->ExposureMode=0x00;
	exif_ptr->valid.WhiteBalance=1;
	exif_ptr->WhiteBalance=0x00;
	/*exif_ptr->DigitalZoomRatio;
	exif_ptr->FocalLengthIn35mmFilm;
	exif_ptr->SceneCaptureType;
	exif_ptr->GainControl;
	exif_ptr->Contrast;
	exif_ptr->Saturation;
	exif_ptr->Sharpness;
	exif_ptr->DeviceSettingDescription;
	exif_ptr->SubjectDistanceRange;*/
	return SENSOR_SUCCESS;
}
uint32 Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_E cmd, uint32 param)
{
	SENSOR_EXP_INFO_T_PTR sensor_info_ptr = Sensor_GetInfo();
	EXIF_SPEC_PIC_TAKING_COND_T *sensor_exif_info_ptr = PNULL;

	SENSOR_PRINT("Sensor_SetSensorExifInfo\n");
	if (PNULL != sensor_info_ptr->ioctl_func_ptr->get_exif) {
		sensor_exif_info_ptr =
		    (EXIF_SPEC_PIC_TAKING_COND_T *)
		    sensor_info_ptr->ioctl_func_ptr->get_exif(0x00);
	} else {
		sensor_exif_info_ptr = &s_default_exif;
		SENSOR_PRINT
		    ("SENSOR: Sensor_SetSensorExifInfo the get_exif fun is null error \n");
		return SENSOR_FAIL;
	}
	SENSOR_PRINT("Sensor_SetSensorExifInfo cmd =%d, parm = %d\n", cmd, param);

	switch (cmd) {
	case SENSOR_EXIF_CTRL_EXPOSURETIME:
		{
			SENSOR_MODE_E img_sensor_mode =
			    s_sensor_mode[Sensor_GetCurId()];
			uint32 exposureline_time =
			    sensor_info_ptr->
			    sensor_mode_info[img_sensor_mode].line_time;
			uint32 exposureline_num = param;
			uint32 exposure_time = 0x00;

			exposure_time = exposureline_time * exposureline_num;
			sensor_exif_info_ptr->valid.ExposureTime = 1;

			if (0x00 == exposure_time) {
				sensor_exif_info_ptr->valid.ExposureTime = 0;
			} else if (1000000 >= exposure_time) {
				sensor_exif_info_ptr->ExposureTime.numerator =
				    0x01;
				sensor_exif_info_ptr->ExposureTime.denominator =
				    1000000 / exposure_time;
			} else {
				uint32 second = 0x00;
				do {
					second++;
					exposure_time -= 1000000;
					if (1000000 >= exposure_time) {
						break;
					}
				} while (1);
				sensor_exif_info_ptr->ExposureTime.denominator =
				    1000000 / exposure_time;
				sensor_exif_info_ptr->ExposureTime.numerator =
				    sensor_exif_info_ptr->
				    ExposureTime.denominator * second;
			}
			break;
		}
	case SENSOR_EXIF_CTRL_FNUMBER:
		break;
	case SENSOR_EXIF_CTRL_EXPOSUREPROGRAM:
		break;
	case SENSOR_EXIF_CTRL_SPECTRALSENSITIVITY:
		break;
	case SENSOR_EXIF_CTRL_ISOSPEEDRATINGS:
		break;
	case SENSOR_EXIF_CTRL_OECF:
		break;
	case SENSOR_EXIF_CTRL_SHUTTERSPEEDVALUE:
		break;
	case SENSOR_EXIF_CTRL_APERTUREVALUE:
		break;
	case SENSOR_EXIF_CTRL_BRIGHTNESSVALUE:
		break;
	case SENSOR_EXIF_CTRL_EXPOSUREBIASVALUE:
		break;
	case SENSOR_EXIF_CTRL_MAXAPERTUREVALUE:
		break;
	case SENSOR_EXIF_CTRL_SUBJECTDISTANCE:
		break;
	case SENSOR_EXIF_CTRL_METERINGMODE:
		break;
	case SENSOR_EXIF_CTRL_LIGHTSOURCE:
		{
			sensor_exif_info_ptr->valid.LightSource = 1;
			switch (param) {
			case 0:
				sensor_exif_info_ptr->LightSource = 0x00;
				break;
			case 1:
				sensor_exif_info_ptr->LightSource = 0x03;
				break;
			case 2:
				sensor_exif_info_ptr->LightSource = 0x0f;
				break;
			case 3:
				sensor_exif_info_ptr->LightSource = 0x0e;
				break;
			case 4:
				sensor_exif_info_ptr->LightSource = 0x03;
				break;
			case 5:
				sensor_exif_info_ptr->LightSource = 0x01;
				break;
			case 6:
				sensor_exif_info_ptr->LightSource = 0x0a;
				break;
			default:
				sensor_exif_info_ptr->LightSource = 0xff;
				break;
			}
			break;
		}
	case SENSOR_EXIF_CTRL_FLASH:
		break;
	case SENSOR_EXIF_CTRL_FOCALLENGTH:
		break;
	case SENSOR_EXIF_CTRL_SUBJECTAREA:
		break;
	case SENSOR_EXIF_CTRL_FLASHENERGY:
		break;
	case SENSOR_EXIF_CTRL_SPATIALFREQUENCYRESPONSE:
		break;
	case SENSOR_EXIF_CTRL_FOCALPLANEXRESOLUTION:
		break;
	case SENSOR_EXIF_CTRL_FOCALPLANEYRESOLUTION:
		break;
	case SENSOR_EXIF_CTRL_FOCALPLANERESOLUTIONUNIT:
		break;
	case SENSOR_EXIF_CTRL_SUBJECTLOCATION:
		break;
	case SENSOR_EXIF_CTRL_EXPOSUREINDEX:
		break;
	case SENSOR_EXIF_CTRL_SENSINGMETHOD:
		break;
	case SENSOR_EXIF_CTRL_FILESOURCE:
		break;
	case SENSOR_EXIF_CTRL_SCENETYPE:
		break;
	case SENSOR_EXIF_CTRL_CFAPATTERN:
		break;
	case SENSOR_EXIF_CTRL_CUSTOMRENDERED:
		break;
	case SENSOR_EXIF_CTRL_EXPOSUREMODE:
		break;
		
	case SENSOR_EXIF_CTRL_WHITEBALANCE:
		sensor_exif_info_ptr->valid.WhiteBalance = 1;
		if(param)
			sensor_exif_info_ptr->WhiteBalance = 1;
		else
			sensor_exif_info_ptr->WhiteBalance = 0;
		break;
		
	case SENSOR_EXIF_CTRL_DIGITALZOOMRATIO:
		break;
	case SENSOR_EXIF_CTRL_FOCALLENGTHIN35MMFILM:
		break;
	case SENSOR_EXIF_CTRL_SCENECAPTURETYPE:
		{
			sensor_exif_info_ptr->valid.SceneCaptureType = 1;
			switch (param) {
			case 0:
				sensor_exif_info_ptr->SceneCaptureType = 0x00;
				break;
			case 1:
				sensor_exif_info_ptr->SceneCaptureType = 0x03;
				break;
			default:
				sensor_exif_info_ptr->LightSource = 0xff;
				break;
			}
			break;
		}
	case SENSOR_EXIF_CTRL_GAINCONTROL:
		break;
	case SENSOR_EXIF_CTRL_CONTRAST:
		{
			sensor_exif_info_ptr->valid.Contrast = 1;
			switch (param) {
			case 0:
			case 1:
			case 2:
				sensor_exif_info_ptr->Contrast = 0x01;
				break;
			case 3:
				sensor_exif_info_ptr->Contrast = 0x00;
				break;
			case 4:
			case 5:
			case 6:
				sensor_exif_info_ptr->Contrast = 0x02;
				break;
			default:
				sensor_exif_info_ptr->Contrast = 0xff;
				break;
			}
			break;
		}
	case SENSOR_EXIF_CTRL_SATURATION:
		{
			sensor_exif_info_ptr->valid.Saturation = 1;
			switch (param) {
			case 0:
			case 1:
			case 2:
				sensor_exif_info_ptr->Saturation = 0x01;
				break;
			case 3:
				sensor_exif_info_ptr->Saturation = 0x00;
				break;
			case 4:
			case 5:
			case 6:
				sensor_exif_info_ptr->Saturation = 0x02;
				break;
			default:
				sensor_exif_info_ptr->Saturation = 0xff;
				break;
			}
			break;
		}
	case SENSOR_EXIF_CTRL_SHARPNESS:
		{
			sensor_exif_info_ptr->valid.Sharpness = 1;
			switch (param) {
			case 0:
			case 1:
			case 2:
				sensor_exif_info_ptr->Sharpness = 0x01;
				break;
			case 3:
				sensor_exif_info_ptr->Sharpness = 0x00;
				break;
			case 4:
			case 5:
			case 6:
				sensor_exif_info_ptr->Sharpness = 0x02;
				break;
			default:
				sensor_exif_info_ptr->Sharpness = 0xff;
				break;
			}
			break;
		}
	case SENSOR_EXIF_CTRL_DEVICESETTINGDESCRIPTION:
		break;
	case SENSOR_EXIF_CTRL_SUBJECTDISTANCERANGE:
		break;
	default:
		break;
	}
	return SENSOR_SUCCESS;
}

EXIF_SPEC_PIC_TAKING_COND_T *Sensor_GetSensorExifInfo(void)
{
	SENSOR_EXP_INFO_T_PTR sensor_info_ptr = Sensor_GetInfo();
	EXIF_SPEC_PIC_TAKING_COND_T *sensor_exif_info_ptr = PNULL;
	SENSOR_PRINT("Sensor_GetSensorExifInfo\n");
	if (PNULL != sensor_info_ptr->ioctl_func_ptr->get_exif) {
		sensor_exif_info_ptr =
		    (EXIF_SPEC_PIC_TAKING_COND_T *)
		    sensor_info_ptr->ioctl_func_ptr->get_exif(0x00);
	} else {
		sensor_exif_info_ptr = &s_default_exif;
		SENSOR_PRINT("SENSOR: Sensor_GetSensorExifInfo the get_exif fun is null, so use the default exif info.\n");
	}
	return sensor_exif_info_ptr;
}

int Sensor_SetSensorParam(uint8_t *buf)
{
	uint32_t i;

	if((SIGN_0 != buf[0]) && (SIGN_1 != buf[1])
	   && (SIGN_2 != buf[2]) && (SIGN_3 != buf[3])) {
		s_sensor_identified = SCI_FALSE;
	} else {
		s_sensor_identified = SCI_TRUE;
		for( i=0 ; i<2 ; i++) {
			s_sensor_index[i] = buf[4+i];
		}
	}
	SENSOR_PRINT("Sensor_SetSensorParam:s_sensor_identified=%d,idex is %d,%d.\n",
		    s_sensor_identified,s_sensor_index[0],s_sensor_index[1]);
	return 0;
}

int Sensor_GetSensorParam(uint8_t *buf,uint8_t *is_saved_ptr)
{
	uint32_t i,j=0;
	uint8_t *ptr=buf;

	if(SCI_TRUE == s_sensor_param_saved) {
		*is_saved_ptr = 1;
		*ptr++ = SIGN_0;
		*ptr++ = SIGN_1;
		*ptr++ = SIGN_2;
		*ptr++ = SIGN_3;
		for( i=0 ; i<2 ; i++) {
			*ptr++ = s_sensor_index[i];
		}
		SENSOR_PRINT("Sensor_GetSensorParam:index is %d,%d.\n",s_sensor_index[0],s_sensor_index[1]);
	} else {
		*is_saved_ptr = 0;
	}
	return 0;
}

void Sensor_SetSensorParamByKey(int key, uint8_t value)
{
	SENSOR_PRINT("Sensor_SetSensorParamByKey key = %d, value = %d\n", key, value);
	switch (key) {
		case SENSOR_PARAM_WB:
			g_sensor_param_info.wb_param = value;
			break;
		case SENSOR_PARAM_METERING:
			g_sensor_param_info.metering_param = value;
			break;
		case SENSOR_PARAM_DTP:
			g_sensor_param_info.dtp_param = value;
			break;
		case SENSOR_PARAM_EV:
			g_sensor_param_info.ev_param= value;
			break;
		case SENSOR_PARAM_WORKMODE:
			g_sensor_param_info.work_mode = value;
			break;
		case SENSOR_PARAM_ISO:
			g_sensor_param_info.iso_param= value;
			break;
		case SENSOR_PARAM_SCENEMODE:
			g_sensor_param_info.scenemode_param = value;
			break;
		default:
			break;
	}
}
uint8_t Sensor_GetSensorParamByKey(int key)
{
	uint8_t value = 0;
	switch (key) {
		case SENSOR_PARAM_WB:
			value = g_sensor_param_info.wb_param;
			break;
		case SENSOR_PARAM_METERING:
			value = g_sensor_param_info.metering_param;
			break;
		case SENSOR_PARAM_DTP:
			value = g_sensor_param_info.dtp_param;
			break;
		case SENSOR_PARAM_EV:
			value = g_sensor_param_info.ev_param;
			break;
		case SENSOR_PARAM_WORKMODE:
			value = g_sensor_param_info.work_mode;
			break;
		case SENSOR_PARAM_ISO:
			value = g_sensor_param_info.iso_param;
			break;
		case SENSOR_PARAM_SCENEMODE:
			value = g_sensor_param_info.scenemode_param;
			break;
		default:
			value = 0;
			break;
	}
	SENSOR_PRINT("Sensor_GetSensorParamByKey key = %d, value = %d \n", key, value);
	return value;
}


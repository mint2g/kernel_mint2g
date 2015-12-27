/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <linux/firmware.h>
#include <linux/string.h>


#include <linux/power_supply.h>

#include "ist30xx.h"
#include "ist30xx_update.h"

#include <linux/input/mt.h>

#if IST30XX_DEBUG
#include "ist30xx_misc.h"
#endif
#include "ist30xx_fw_10_86.h"
#define MAX_ERR_CNT             (100)

static char IsfwUpdate[20]={0};

#define FW_DOWNLOADING "Downloading"
#define FW_DOWNLOAD_COMPLETE "Complete"
#define FW_DOWNLOAD_FAIL "FAIL"

#define TSP_BUF_SIZE    (1024)

#if IST30XX_USE_KEY
int ist30xx_key_code[] = { 0, KEY_MENU, KEY_BACK};
#endif

#if IST30XX_DETECT_TA
static int ist30xx_ta_status = -1;
#endif
#if IST30XX_DETECT_TEMPER
static int ist30xx_temp_status = -1;
#endif

DEFINE_MUTEX(ist30xx_mutex);

struct ist30xx_data *ts_data;
static struct delayed_work work_reset_check;
#if IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE
static struct delayed_work work_fw_update;
#endif
static void clear_input_data(struct ist30xx_data *data);

#if IST30XX_NOISE_MODE
#define IST30XX_IDLE_STATUS     (0x1D4E0000)
#define IDLE_ALGORITHM_MODE     (1U << 15)
#define IDLE_TIMER_INTERVAL     (HZ/2)   // 0.5sec

bool get_event_mode = true;

static struct timer_list idle_timer;
static struct timespec t_event, t_current, t_temper;  // ns
#endif

static struct regulator *tsp_regulator_3_3=NULL;
static struct regulator *tsp_regulator_1_8=NULL;

int ist30xx_zvalue = 0;
int ist30xx_zvalue_menu = 0;
int ist30xx_zvalue_back = 0;
#define TOUCH_ON 1
#define TOUCH_OFF 0

void ts_power_enable(int en)
{
	int ret;
	
	printk(KERN_ERR "%s %s\n", __func__, (en) ? "on" : "off");
	if (en == TOUCH_ON) {

		if(tsp_regulator_3_3 == NULL) {
			tsp_regulator_3_3 = regulator_get(NULL, REGU_NAME_TP); /*touch power*/
			if(IS_ERR(tsp_regulator_3_3)){
				printk("[TSP] can not get TSP VDD 3.3V\n");
				tsp_regulator_3_3 = NULL;
				return -1;
			}
			
			ret = regulator_set_voltage(tsp_regulator_3_3,3300000,3300000);
			printk("[TSP] %s --> regulator_set_voltage ret = %d \n",__func__, ret);
		}

		if(tsp_regulator_1_8 == NULL) {
			tsp_regulator_1_8 = regulator_get(NULL, REGU_NAME_TP1); /*touch power*/
			if(IS_ERR(tsp_regulator_1_8)){
				printk("[TSP] can not get TSP VDD 1.8V\n");
				tsp_regulator_1_8 = NULL;
				return -1;
			}
			
			ret = regulator_set_voltage(tsp_regulator_1_8,1800000,1800000);
			printk("[TSP] %s --> regulator_set_voltage ret = %d \n",__func__, ret);
		}
		
		ret = regulator_enable(tsp_regulator_3_3);
		printk("[TSP] --> 1.8v regulator_enable ret = %d \n", ret);

		msleep(10);
		
		ret = regulator_enable(tsp_regulator_1_8);
		printk("[TSP] --> 3.3v regulator_enable ret = %d \n", ret);



	}
	else {
	
		ret = regulator_disable(tsp_regulator_1_8);
		printk("[TSP] --> 3.3v regulator_disable ret = %d \n", ret);

		//udelay(100);

		ret = regulator_disable(tsp_regulator_3_3);
		printk("[TSP] --> 1.8v regulator_disable ret = %d \n", ret);
		
	}
}

struct device *sec_touchscreen;
EXPORT_SYMBOL(sec_touchscreen);
struct device *sec_touchkey;
EXPORT_SYMBOL(sec_touchkey);

extern struct class *sec_class;
struct device *sec_touchscreen_dev;
struct device *sec_touchkey_dev;

#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func

struct tsp_cmd {
	struct list_head	list;
	const char	*cmd_name;
	void	(*cmd_func)(void *device_data);
};


static ssize_t phone_firmware_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t part_firmware_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t threshold_firmware_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t firmware_update(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t firmware_update_status(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t menu_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t back_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t touchkey_threshold_show(struct device *dev,  struct device_attribute *attr, char *buf);

static DEVICE_ATTR(tsp_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP, phone_firmware_show, NULL);
static DEVICE_ATTR(tsp_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP, part_firmware_show, NULL);
static DEVICE_ATTR(tsp_threshold, S_IRUGO | S_IWUSR | S_IWGRP, threshold_firmware_show, NULL);
static DEVICE_ATTR(tsp_firm_update, S_IRUGO | S_IWUSR | S_IWGRP, firmware_update, firmware_update);
static DEVICE_ATTR(tsp_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP, firmware_update_status, NULL);

static DEVICE_ATTR(touchkey_menu, S_IRUGO | S_IWUSR | S_IWGRP, menu_sensitivity_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO | S_IWUSR | S_IWGRP, back_sensitivity_show, NULL);
static DEVICE_ATTR(touchkey_threshold, S_IRUGO | S_IWUSR | S_IWGRP, touchkey_threshold_show, NULL);

//static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
//static void get_config_ver(void *device_data);
//static void get_threshold(void *device_data);
//static void module_off_master(void *device_data);
//static void module_on_master(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_reference(void *device_data);
static void get_cm_abs(void *device_data);
//static void get_cm_delta(void *device_data);
//static void get_intensity(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
//static void run_reference_read(void *device_data);
static void run_cm_abs_read(void *device_data);
//static void run_cm_delta_read(void *device_data);
//static void run_intensity_read(void *device_data);
static void get_rawcap(void *device_data);
static void not_support_cmd(void *device_data);
//static int check_delta_value(struct melfas_ts_data *ts);

struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("fw_update", not_support_cmd),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_config_ver", not_support_cmd),},
	{TSP_CMD("get_threshold", not_support_cmd),},
	{TSP_CMD("module_off_master", not_support_cmd),},
	{TSP_CMD("module_on_master", not_support_cmd),},
	{TSP_CMD("module_off_slave", not_support_cmd),},
	{TSP_CMD("module_on_slave", not_support_cmd),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("run_rawcap_read", run_cm_abs_read),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("get_reference", get_reference),},
	{TSP_CMD("get_cm_abs", get_cm_abs),},
	{TSP_CMD("get_rawcap", get_rawcap),}, 	
	{TSP_CMD("get_cm_delta", not_support_cmd),},
	{TSP_CMD("get_intensity", not_support_cmd),},
	{TSP_CMD("run_reference_read", not_support_cmd),},
	{TSP_CMD("run_cm_abs_read", not_support_cmd),},
	{TSP_CMD("run_cm_delta_read", not_support_cmd),},
	{TSP_CMD("run_intensity_read", not_support_cmd),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};

static void set_default_result(struct ist30xx_data *ts)
{
	char delim = ':';
	memset(ts->cmd_result, 0x00, ARRAY_SIZE(ts->cmd_result));
	memcpy(ts->cmd_result, ts->cmd, strlen(ts->cmd));
	strncat(ts->cmd_result, &delim, 1);
}

static void set_cmd_result(struct ist30xx_data *ts, char *buff, int len)
{
	strncat(ts->cmd_result, buff, len);
}

static void not_support_cmd(void *device_data)
{
	struct ist30xx_data *ts = (struct ist30xx_data *)device_data;
	char buff[16] = {0};
	set_default_result(ts);
	snprintf(buff, sizeof(buff), "%s", "NA");
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 4;
	dev_info(&ts->client->dev, "%s: \"%s(%d)\"\n", __func__,
				buff, strnlen(buff, sizeof(buff)));
	return;
}

static ssize_t show_close_tsp_test(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return snprintf(buf, TSP_BUF_SIZE, "%u\n", 0);
}

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct ist30xx_data *info = dev_get_drvdata(dev);
	char buff[16] = {0};

	dev_info(&info->client->dev, "tsp cmd: status:%d\n",
			info->cmd_state);

	if (info->cmd_state == 0)
		snprintf(buff, sizeof(buff), "WAITING");

	else if (info->cmd_state == 1)
		snprintf(buff, sizeof(buff), "RUNNING");

	else if (info->cmd_state == 2)
		snprintf(buff, sizeof(buff), "OK");

	else if (info->cmd_state == 3)
		snprintf(buff, sizeof(buff), "FAIL");

	else if (info->cmd_state == 4)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute
				    *devattr, char *buf)
{
	struct ist30xx_data *info = dev_get_drvdata(dev);

	dev_info(&info->client->dev, "tsp cmd: result: %s\n", info->cmd_result);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = 0;

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", info->cmd_result);
}

static ssize_t store_cmd(struct device *dev, struct device_attribute
				  *devattr, const char *buf, size_t count)
{
	struct ist30xx_data *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0};
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;
	int ret;

	if (info->cmd_is_running == true) {
		dev_err(&info->client->dev, "tsp_cmd: other cmd is running.\n");
		goto err_out;
	}


	/* check lock  */
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = true;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = 1;

	for (i = 0; i < ARRAY_SIZE(info->cmd_param); i++)
		info->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(info->cmd, 0x00, ARRAY_SIZE(info->cmd));
	memcpy(info->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);
	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}
	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				ret = kstrtoint(buff, 10,\
						info->cmd_param + param_cnt);
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}
	dev_info(&client->dev, "cmd = %s\n", tsp_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_info(&client->dev, "cmd param %d= %d\n", i,
							info->cmd_param[i]);
	tsp_cmd_ptr->cmd_func(info);

err_out:
	return count;
}

static void get_chip_vendor(void *device_data)
{
	struct ist30xx_data *ts = (struct ist30xx_data *)device_data;
	char buff[16] = {0};
	set_default_result(ts);
	snprintf(buff, sizeof(buff), "%s", "IMAGIS");
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_chip_name(void *device_data)
{
	struct ist30xx_data *ts = (struct ist30xx_data *)device_data;
	char buff[16] = {0};
	set_default_result(ts);

	snprintf(buff, sizeof(buff), "%s", "IST30XX");

	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

extern TSP_INFO ist30xx_tsp_info;
extern TKEY_INFO ist30xx_tkey_info;

#define NODE_NUM	100 /* 19x12 */
#define NODE_X_NUM 9
#define NODE_Y_NUM 10
#define TSP_CH_SCREEN   (1)
s16 cm_abs[NODE_NUM]= {{0,},};;
extern u32 *ist30xx_frame_rawbuf;
int ist30xx_parse_tsp_node_test(void)
{
	int i, j, idx;
	u16 raw, base;
	TSP_INFO *tsp = &ist30xx_tsp_info;

	printk("[TSP] ist30xx_parse_tsp_node tsp->height : %d, tsp->width : %d\n", tsp->height, tsp->width);
	int k = 0;	
	for (i = 0; i < tsp->width; i++) {
		for (j = 0; j < tsp->height; j++) {
			idx = (i * tsp->height) + j;

			raw = ist30xx_frame_rawbuf[idx] & 0xFFF;
			//base = (ist30xx_frame_rawbuf[idx] >> 16) & 0xFFF;
			//printk("[TSP] %d,  \n", raw);
			if (ist30xx_check_valid_ch(i, j) == TSP_CH_SCREEN) {
				cm_abs[k] = raw;
				//*base_buf++ = base;
				//printk("[TSP] cm_abs [%d] : %d\n", k, raw);
                                k++;				
			}
		}
	}

	return 0;
}

int ist30xx_read_tsp_node_test(struct ist30xx_data *ts)
{
	int ret = 0;
	TSP_INFO *tsp = &ist30xx_tsp_info;

	ret = ist30xx_enter_debug_mode();
	if (ret) return ret;

	ret = ist30xx_cmd_reg(ts->client, CMD_ENTER_REG_ACCESS);
	if (ret) return ret;

	ret = ist30xx_write_cmd(ts->client,
				IST30XX_RX_CNT_ADDR, tsp->buf.int_len);
	if (ret) return ret;

	ret = ist30xx_read_buf(ts->client, IST30XX_RAW_ADDR,
			       ist30xx_frame_rawbuf, tsp->buf.int_len);
	if (ret) return ret;

	ret = ist30xx_cmd_reg(ts->client, CMD_EXIT_REG_ACCESS);
	if (ret) return ret;

	ret = ist30xx_cmd_start_scan(ts->client);
	if (ret) return ret;

	ret = ist30xx_parse_tsp_node_test();

	return ret;
}

static void run_cm_abs_read(void *device_data)
{
	//not_reset = 1;
	
	struct ist30xx_data *info = (struct ist30xx_data *)device_data;

    	disable_irq(info->client->irq);

    	printk("[TSP] %s, %d\n", __func__, __LINE__ );
    	int i, j;

	set_default_result(info);

	ist30xx_read_tsp_node_test(info);

	int k = 0;
	for (i = 0; i < NODE_X_NUM; i++) {
		printk(KERN_CONT "[TSP]");
		for (j = 0; j < NODE_Y_NUM; j++) {
			printk(KERN_CONT " %d", cm_abs[k]);
			k++;
		}
		printk(KERN_CONT "\n");
	}

    	info->cmd_state = 2;
    enable_irq(info->client->irq);
   //not_reset = 0;
    
    	return;

/*	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__); */
}

static int check_rx_tx_num(void *device_data)
{
	struct ist30xx_data *info = (struct ist30xx_data *)device_data;

	char buff[TSP_CMD_STR_LEN] = {0};
	int node;

	if (info->cmd_param[0] < 0 ||
			info->cmd_param[0] >= NODE_X_NUM  ||
			info->cmd_param[1] < 0 ||
			info->cmd_param[1] >= NODE_Y_NUM) {
		snprintf(buff, sizeof(buff) , "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = 3;

		dev_info(&info->client->dev, "%s: parameter error: %u,%u\n",
				__func__, info->cmd_param[0],
				info->cmd_param[1]);
		node = -1;
		return node;
             }
	//node = info->cmd_param[1] * NODE_Y_NUM + info->cmd_param[0];
	node = info->cmd_param[0] * NODE_Y_NUM + info->cmd_param[1];
	dev_info(&info->client->dev, "%s: node = %d\n", __func__,
			node);
	return node;

 }
static void get_cm_abs(void *device_data)
{
	struct ist30xx_data *info = (struct ist30xx_data *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	printk("[TSP] %s, %d\n", __func__, __LINE__ );

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;

	val = cm_abs[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
    
}



static void get_rawcap(void *device_data)
{
	struct ist30xx_data *info = (struct ist30xx_data *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

    printk("[TSP] %s, %d\n", __func__, __LINE__ );
    
	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;
    
	val = cm_abs[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_x_num(void *device_data)
{
	struct ist30xx_data *ts = (struct ist30xx_data *)device_data;
	char buff[16] = {0};
	int val;
	int exciting_ch;
	set_default_result(ts);

	exciting_ch = 9;
	val = exciting_ch;
	if (val < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = 3;
		dev_info(&ts->client->dev,
			"%s: fail to read num of x (%d).\n", __func__, val);
		return ;
	}
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_y_num(void *device_data)
{
	struct ist30xx_data *ts = (struct ist30xx_data *)device_data;
	char buff[16] = {0};
	int val;
	int sensing_ch;
	set_default_result(ts);
	
	sensing_ch = 9;
	val = sensing_ch;
	if (val < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = 3;
		dev_info(&ts->client->dev,
			"%s: fail to read num of y (%d).\n", __func__, val);
		return ;
	}
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_fw_ver_bin(void *device_data)
{
	struct ist30xx_data *info = (struct ist30xx_data *)device_data;

	char buff[16] = {0};
	u32 val = 0;

	printk("[TSP] %s, %d\n", __func__, __LINE__ );


	set_default_result(info);

	val = ist30xx_get_ic_fw_ver();
	snprintf(buff, sizeof(buff), "IM00%04x", val);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct ist30xx_data *info = (struct ist30xx_data *)device_data;

	char buff[16] = {0};
	int ret, part_fw_ver;

	set_default_result(info);

    mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(ts_data);

	ret = ist30xx_cmd_run_device(ts_data->client);	
	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_PARAM_VER, &ts_data->param_ver);

	part_fw_ver = ts_data->param_ver;

	ist30xx_start(ts_data);

	ist30xx_enable_irq(ts_data);	
    mutex_unlock(&ist30xx_mutex);

        snprintf(buff, sizeof(buff), "IM00%02x%02x", (part_fw_ver>>8)&0xFF, part_fw_ver&0xFF);
	
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));

}

static void check_reference_value(struct ist30xx_data *ts)
{
	struct ist30xx_data *info = (struct ist30xx_data *) ts;

	char buff[TSP_CMD_STR_LEN] = {0};
	int ret, i, j, status;
	int max_value = 0, min_value = 1000000;

	printk(KERN_ERR "[TSP] %s entered. line : %d,\n", __func__, __LINE__);
	
    	disable_irq(info->client->irq);

    	printk("[TSP] %s, %d\n", __func__, __LINE__ );

	set_default_result(info);

	ist30xx_read_tsp_node_test(info);

	int k = 0;
	for (i = 0; i < NODE_X_NUM; i++) {
		for (j = 0; j < NODE_Y_NUM; j++) {
			if((cm_abs[k] !=0)&&(min_value>cm_abs[k]))
				min_value = cm_abs[k];
			if((cm_abs[k] !=0)&&(max_value<cm_abs[k]))
				max_value = cm_abs[k];			
			k++;
		}
	}
	
	printk(KERN_ERR "min:%d,max:%d", min_value, max_value);
	printk(KERN_INFO "\n");
	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	enable_irq(info->client->irq);

	printk(KERN_ERR "%s : end\n", __func__);
}

static void get_reference(void *device_data)
{
	struct ist30xx_data *ts = (struct ist30xx_data *)device_data;
	printk("[TSP] %s\n", __func__);
	set_default_result(ts);
	check_reference_value(ts);
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s:\n", __func__);
}

static ssize_t phone_firmware_show(struct device *dev, struct device_attribute *attr, char *buf)
{   
	int phone_fw_ver=0;
	u32 val = 0;	

	printk("[TSP] %s\n",__func__);

	val = ist30xx_get_ic_fw_ver();

	return sprintf(buf, "IM00%04x", val);
}

static ssize_t part_firmware_show(struct device *dev, struct device_attribute *attr, char *buf)
{   
	int ret, part_fw_ver;

	printk("[TSP] %s\n",__func__);

    mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(ts_data);

	ret = ist30xx_cmd_run_device(ts_data->client);	
	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_PARAM_VER, &ts_data->param_ver);

	part_fw_ver = ts_data->param_ver;

	ist30xx_start(ts_data);

	ist30xx_enable_irq(ts_data);
    mutex_unlock(&ist30xx_mutex);
	
	printk("[TSP] %s : %x\n",__func__, part_fw_ver);
	
	return sprintf(buf, "IM00%02x%02x", (part_fw_ver>>8)&0xFF, part_fw_ver&0xFF);
}

static ssize_t threshold_firmware_show(struct device *dev, struct device_attribute *attr, char *buf)
{  
	int threshord =30;
	printk("[TSP] %s\n",__func__);

	return sprintf(buf, "%d", threshord );
}

static ssize_t menu_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
      printk("[TSP] %s : %d \n", __func__, ist30xx_zvalue_menu);

	return sprintf(buf, "%d\n",  ist30xx_zvalue_menu);
}

static ssize_t back_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
      printk("[TSP] %s : %d\n", __func__, ist30xx_zvalue_back);

	return sprintf(buf, "%d\n",  ist30xx_zvalue_back);
}

static ssize_t touchkey_threshold_show(struct device *dev,  struct device_attribute *attr, char *buf)
{
    int threshold = 30;
	
    printk("[TSP] touch tkey threshold: %d\n", threshold);

    return snprintf(buf, sizeof(int), "%d\n", threshold);
}
static ssize_t firmware_update(struct device *dev, struct device_attribute *attr, char *buf)
{
    bool ret;
    printk("[TSP] %s\n",__func__);
    sprintf(IsfwUpdate,"%s\n",FW_DOWNLOADING);

    ret =  ist30xx_force_bin_update(ts_data);
   // ret =  ist30xx_force_param_update(ts_data);	

    if(ret == 0)
    {
        sprintf(IsfwUpdate,"%s\n",FW_DOWNLOAD_COMPLETE);
        return sprintf(buf, "%d", 1 );
    }
    else
    {
        sprintf(IsfwUpdate,"%s\n",FW_DOWNLOAD_FAIL);
        return sprintf(buf, "%d", -1 );
    }
    
}

static ssize_t firmware_update_status(struct device *dev, struct device_attribute *attr, char *buf)
{
    bool ret;

	printk("[TSP] %s\n",__func__);

    sprintf(IsfwUpdate,"%s\n",FW_DOWNLOADING);

    ret =  ist30xx_force_bin_update(ts_data);
   // ret =  ist30xx_force_param_update(ts_data);	

    if(ret == 0)
    {
        sprintf(IsfwUpdate,"%s\n",FW_DOWNLOAD_COMPLETE);
        return sprintf(buf, "%d", 1 );
    }
    else
    {
        sprintf(IsfwUpdate,"%s\n",FW_DOWNLOAD_FAIL);
        return sprintf(buf, "%d", -1 );
    }
	return sprintf(buf, "%s\n", IsfwUpdate);
}

void ist30xx_disable_irq(struct ist30xx_data *data)
{
	if (data->irq_enabled) {
		disable_irq(data->client->irq);
		data->irq_enabled = 0;
	}
}

void ist30xx_enable_irq(struct ist30xx_data *data)
{
	if (!data->irq_enabled) {
		enable_irq(data->client->irq);
		msleep(50);
		data->irq_enabled = 1;
	}
}

int ist30xx_max_error_cnt = MAX_ERR_CNT;
int ist30xx_error_cnt = 0;
static void ist30xx_request_reset(void)
{
	//clear_input_data(ts_data);

	ist30xx_error_cnt++;

	if (ist30xx_error_cnt >=  ist30xx_max_error_cnt) {
		schedule_delayed_work(&work_reset_check, 0);
		DMSG("[ TSP ] ist30xx_request_reset!\n");
		ist30xx_error_cnt = 0;
	}
}


void ist30xx_start(struct ist30xx_data *data)
{
#if IST30XX_DETECT_TA
	if (ist30xx_ta_status > -1) {
		ist30xx_write_cmd(data->client, CMD_SET_TA_MODE, ist30xx_ta_status);

		DMSG("[ TSP ] ist30xx_start, ta_mode : %d\n",
		     ist30xx_ta_status);
	}
#endif
#if IST30XX_DETECT_TEMPER
	if (ist30xx_temp_status > -1) {
		ist30xx_write_cmd(data->client, CMD_SET_TEMPER_MODE, ist30xx_temp_status);

		DMSG("[ TSP ] ist30xx_start, temperature mode : %d\n",
		     ist30xx_temp_status);
	}
#endif

	ist30xx_cmd_start_scan(data->client);

#if IST30XX_NOISE_MODE
	if ((data->status.update != 1) && (data->status.calib != 1))
	ktime_get_ts(&t_event);
#endif
}


int ist30xx_get_ver_info(struct ist30xx_data *data)
{
	int ret;

	data->fw.pre_ver = data->fw.ver;
	data->fw.ver = 0;

	ret = ist30xx_read_cmd(data->client, CMD_GET_CHIP_ID, &data->chip_id);
	if (ret)
		return -EIO;

	ret = ist30xx_read_cmd(data->client, CMD_GET_FW_VER, &data->fw.ver);
	if (ret)
		return -EIO;

	ret = ist30xx_read_cmd(data->client, CMD_GET_PARAM_VER, &data->param_ver);
	if (ret)
		return -EIO;

	DMSG("[ TSP ] Chip ID : %x F/W: %x Param: %x\n",
	     data->chip_id, data->fw.ver, data->param_ver);

	if ((data->chip_id != IST30XX_CHIP_ID) &&
	    (data->chip_id != IST30XXA_CHIP_ID))
		return -EPERM;

	return 0;
}


int ist30xx_init_touch_driver(struct ist30xx_data *data)
{
	int ret = 0;

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);

	ret = ist30xx_cmd_run_device(data->client);
	if (ret)
		goto init_touch_end;

	ret = ist30xx_get_ver_info(data);
	if (ret)
		goto init_touch_end;

init_touch_end:
	ist30xx_start(data);

	ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);

	return ret;
}


int ist30xx_get_info(struct ist30xx_data *data)
{
	int ret;

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);

	ret = ist30xx_cmd_run_device(data->client);
	if (ret) goto get_info_end;

#if !(IST30XX_INTERNAL_BIN)
	ret = ist30xx_get_ver_info(data);
	if (ret) goto get_info_end;
#endif  // !(IST30XX_INTERNAL_BIN)

#if IST30XX_DEBUG
# if IST30XX_INTERNAL_BIN
	ist30xx_get_tsp_info();
	ist30xx_get_tkey_info();
# else
	ret = ist30xx_tsp_update_info();
	if (ret) goto get_info_end;

	ret = ist30xx_tkey_update_info();
	if (ret) goto get_info_end;
# endif
#endif  // IST30XX_DEBUG

	if (!data->status.update) {
		ret = ist30xx_cmd_check_calib(data->client);
		if (!ret) {
			data->status.calib = 1;
#if IST30XX_EVENT_MODE
			ktime_get_ts(&t_event);
#endif
		}
	}

get_info_end:
	if (ret == 0)
	ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);

	return ret;
}


#define PRESS_MSG_MASK          (0x01)
#define MULTI_MSG_MASK          (0x02)
#define PRESS_MSG_KEY           (0x6)
#define CALIB_MSG_MASK          (0xF0000FFF)
#define CALIB_MSG_VALID         (0x80000CAB)

#define TOUCH_DOWN_MESSAGE      ("Touch down")
#define TOUCH_UP_MESSAGE        ("Touch up  ")
#define TOUCH_MOVE_MESSAGE      ("Touch move")
bool tsp_touched[IST30XX_MAX_MT_FINGERS] = { 0, };

void print_tsp_event(finger_info *finger)
{
#if PRINT_TOUCH_EVENT
	int idx = finger->bit_field.id - 1;
	int press = finger->bit_field.udmg & PRESS_MSG_MASK;

	if (press == PRESS_MSG_MASK) {
		if (tsp_touched[idx] == 0) { // touch down
			DMSG("[ TSP ] %s - %d (%d, %d)\n", TOUCH_DOWN_MESSAGE,
			     finger->bit_field.id,
			     finger->bit_field.x, finger->bit_field.y);
			tsp_touched[idx] = 1;
		}
#if 0
		else {                // touch move
			DMSG("[ TSP ] %s - %d (%d,%d)\n", idx, TOUCH_MOVE_MESSAGE,
			     finger->bit_field.x, finger->bit_field.y);
	}
#endif
	} else {
		if (tsp_touched[idx] == 1) { // touch up
			DMSG("[ TSP ] %s - %d (%d, %d)\n", TOUCH_UP_MESSAGE,
			     finger->bit_field.id,
			     finger->bit_field.x, finger->bit_field.y);
			tsp_touched[idx] = 0;
		}
	}
#endif  // PRINT_TOUCH_EVENT
}


static void clear_input_data(struct ist30xx_data *data)
{
	int i, pressure, count;

	for (i = 0, count = 0; i < IST30XX_MAX_MT_FINGERS; i++) {
		if (data->prev_fingers[i].bit_field.id == 0)
			continue;

		pressure = (data->prev_fingers[i].bit_field.udmg & PRESS_MSG_MASK);
		if (pressure) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
			input_mt_slot(data->input_dev, data->prev_fingers[i].bit_field.id - 1);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
#else
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					 data->prev_fingers[i].bit_field.x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					 data->prev_fingers[i].bit_field.y);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 0);
			input_mt_sync(data->input_dev);
#endif

			data->prev_fingers[i].bit_field.udmg &= ~(PRESS_MSG_MASK);
			print_tsp_event(&data->prev_fingers[i]);

			data->prev_fingers[i].bit_field.id = 0;

			count++;
		}
	}

	if (count > 0)
		input_sync(data->input_dev);
}


static void release_finger(finger_info *finger)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
    input_mt_slot(ts_data->input_dev, finger->bit_field.id - 1);
    input_mt_report_slot_state(ts_data->input_dev, MT_TOOL_FINGER, false);
#else
    input_report_abs(ts_data->input_dev, ABS_MT_POSITION_X, finger->bit_field.x);
    input_report_abs(ts_data->input_dev, ABS_MT_POSITION_Y, finger->bit_field.y);
    input_report_abs(ts_data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
    input_report_abs(ts_data->input_dev, ABS_MT_WIDTH_MAJOR, 0);
    input_mt_sync(ts_data->input_dev);
#endif                  // (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))

	printk("[ TSP ] force release %d(%d, %d)\n", finger->bit_field.id,
	       finger->bit_field.x, finger->bit_field.y);

	finger->bit_field.udmg &= ~(PRESS_MSG_MASK);
    print_tsp_event(finger);

    finger->bit_field.id = 0;

    input_sync(ts_data->input_dev);
}

#if IST30XX_DEBUG
extern TSP_INFO ist30xx_tsp_info;
extern TKEY_INFO ist30xx_tkey_info;
#endif
static int check_report_data(struct ist30xx_data *data, int finger_counts, int key_counts)
{
    int ret = -EPERM;
    int i, j;
    bool valid_id;

    for (i = 0; i < finger_counts; i++) {
		if ((data->fingers[i].bit_field.id == 0) ||
		    (data->fingers[i].bit_field.id > ist30xx_tsp_info.finger_num+1) ||
		    (data->fingers[i].bit_field.x > IST30XX_MAX_Y) ||
		    (data->fingers[i].bit_field.y > IST30XX_MAX_X)) {
			pr_err("[ TSP ] Error, %d[%d] - (%d, %d)\n", i,
			       data->fingers[i].bit_field.id,
			       data->fingers[i].bit_field.x,
			       data->fingers[i].bit_field.y);

			data->fingers[i].bit_field.id = 0;
            return ret;
        }
		}

    if (data->num_fingers >= finger_counts) {
        for (i = 0; i < IST30XX_MAX_MT_FINGERS; i++) {     // prev_fingers
            if (data->prev_fingers[i].bit_field.id != 0 &&
			    (data->prev_fingers[i].bit_field.udmg & PRESS_MSG_MASK)) {
                valid_id = false;
                for (j = 0; j < ist30xx_tsp_info.finger_num; j++) { // fingers
                    if ((data->prev_fingers[i].bit_field.id) ==
                            (data->fingers[j].bit_field.id)) {
                        valid_id = true;
                        break;
                    }
                }
                if (valid_id == false)
                    release_finger(&data->prev_fingers[i]);
            }
        }
    }

    return 0;
}

bool get_zvalue_mode = false;

static void report_input_data(struct ist30xx_data *data, int finger_counts, int key_counts)
{
	int i, pressure, count;

       memset(data->prev_fingers, 0, sizeof(data->prev_fingers));

	for (i = 0, count = 0; i < finger_counts; i++) {
		pressure = data->fingers[i].bit_field.udmg & PRESS_MSG_MASK;

		print_tsp_event(&data->fingers[i]);
		input_mt_slot(data->input_dev, data->fingers[i].bit_field.id - 1);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,
					   (pressure ? true : false));
	
		if (pressure) {
		input_report_abs(data->input_dev, ABS_MT_POSITION_X,
				 data->fingers[i].bit_field.y);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
				 data->fingers[i].bit_field.x);
		} else {
		ist30xx_zvalue = 0;
	}
		DMSG("[ TSP ] x:%d, y:%d, p:%d\n",data->fingers[i].bit_field.y, data->fingers[i].bit_field.x, pressure);
	count++;

	ist30xx_error_cnt = 0;

	data->prev_fingers[i].full_field = data->fingers[i].full_field;
        data->num_fingers = finger_counts;
}

#if IST30XX_USE_KEY
for (i = finger_counts; i < finger_counts + key_counts; i++) {
	int press, id;
	press = (data->fingers[i].bit_field.w == PRESS_MSG_KEY) ? 1 : 0;
	id = data->fingers[i].bit_field.id;

		DMSG("[ TSP ] [%08x] id: %d, event: %d, z-value: %d\n",
	     data->fingers[i].full_field, id,
	     data->fingers[i].bit_field.w,
	     data->fingers[i].bit_field.y);

		ist30xx_zvalue = data->fingers[i].bit_field.y;

	if(id==1){	
	if(!press)
			ist30xx_zvalue_menu = 0;
		else
			ist30xx_zvalue_menu = ist30xx_zvalue;
	}
	else if(id==2){
	if(!press)
			ist30xx_zvalue_back = 0;
		else
			ist30xx_zvalue_back = ist30xx_zvalue;
	}	
	input_report_key(data->input_dev, ist30xx_key_code[id], press);
	count++;
}
#endif  // IST30XX_USE_KEY

if (count > 0)
	input_sync(data->input_dev);
}

/*
 * CMD : CMD_GET_COORD
 *               [31:30]  [29:26]  [25:16]  [15:10]  [9:0]
 *   Multi(1st)  UDMG     Rsvd.    NumOfKey Rsvd.    NumOfFinger
 *    Single &   UDMG     ID       X        Area     Y
 *   Multi(2nd)
 *
 *   UDMG [31] 0/1 : single/multi
 *   UDMG [30] 0/1 : unpress/press
 */
static irqreturn_t ist30xx_irq_thread(int irq, void *ptr)
{
	int i, ret;
	int key_cnt, finger_cnt, read_cnt;
	struct ist30xx_data *data = ptr;
	u32 msg[IST30XX_MAX_MT_FINGERS];
	bool unknown_idle = false;

	if (!data->irq_enabled)
		return IRQ_HANDLED;

	memset(msg, 0, sizeof(msg));

	ret = ist30xx_get_position(data->client, msg, 1);
	if (ret)
		goto irq_err;

	//DMSG("[ TSP ] intr thread msg: 0x%08x\n", *msg);

	if (msg[0] == 0)
		return IRQ_HANDLED;

#if IST30XX_NOISE_MODE
	if ((data->status.update != 1) && (data->status.calib != 1))
	ktime_get_ts(&t_event);

	if (get_event_mode) {
		if ((msg[0] & 0xFFFF0000) == IST30XX_IDLE_STATUS) { /* ESD test code */
			if (msg[0] & IDLE_ALGORITHM_MODE)
				return IRQ_HANDLED;

		for (i = 0; i < IST30XX_MAX_MT_FINGERS; i++) {
				if (data->prev_fingers[i].bit_field.id == 0)
					continue;

			if (data->prev_fingers[i].bit_field.udmg & PRESS_MSG_MASK) {
					release_finger(&data->prev_fingers[i]);
					unknown_idle = true;
				}
			}

			if (unknown_idle) {
				schedule_delayed_work(&work_reset_check, 0);
				pr_err("[ TSP ] Find unknown pressure\n");
		}

		return IRQ_HANDLED;
	}
	}
#endif

	if ((msg[0] & CALIB_MSG_MASK) == CALIB_MSG_VALID) {
		data->status.calib_msg = msg[0];
		DMSG("[ TSP ] calib status: 0x%08x\n", data->status.calib_msg);
		return IRQ_HANDLED;
	}

	for (i = 0; i < IST30XX_MAX_MT_FINGERS; i++)
		data->fingers[i].full_field = 0;

	key_cnt = 0;
	finger_cnt = 1;
	read_cnt = 1;
	data->fingers[0].full_field = msg[0];

	if (data->fingers[0].bit_field.udmg & MULTI_MSG_MASK) {
		key_cnt = data->fingers[0].bit_field.x;
		finger_cnt = data->fingers[0].bit_field.y;
		read_cnt = finger_cnt + key_cnt;

        if (finger_cnt > ist30xx_tsp_info.finger_num ||
                key_cnt > ist30xx_tkey_info.key_num) {
			pr_err("[ TSP ] Invalid touch count - finger: %d(%d), key: %d(%d)\n",
                    finger_cnt, ist30xx_tsp_info.finger_num,
                    key_cnt, ist30xx_tkey_info.key_num);
			goto irq_err;
        }

#if I2C_BURST_MODE
		ret = ist30xx_get_position(data->client, msg, read_cnt);
		if (ret)
			goto irq_err;

		for (i = 0; i < read_cnt; i++)
			data->fingers[i].full_field = msg[i];
#else
		for (i = 0; i < read_cnt; i++) {
			ret = ist30xx_get_position(data->client, &msg[i], 1);
			if (ret)
				goto irq_err;

			data->fingers[i].full_field = msg[i];
		}
#endif
	}

	if (get_zvalue_mode == true) {
		u32 zvalue = 0;

		ret = ist30xx_get_position(data->client, &zvalue, 1);
		if (ret)
			goto irq_err;

		if (zvalue > 0)
			ist30xx_zvalue = zvalue;
		//DMSG("[ TSP ] [%08x] key raw data: %d\n", zvalue, ist30xx_zvalue);
	}

    if (check_report_data(data, finger_cnt, key_cnt))
            return IRQ_HANDLED;

	if (read_cnt > 0)
		report_input_data(data, finger_cnt, key_cnt);

	return IRQ_HANDLED;

irq_err:
	pr_err("[ TSP ] intr msg[0]: 0x%08x, ret: %d\n", msg[0], ret);
	ist30xx_request_reset();
	return IRQ_HANDLED;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
#define ist30xx_suspend NULL
#define ist30xx_resume  NULL
static void ist30xx_early_suspend(struct early_suspend *h)
{
	struct ist30xx_data *data = container_of(h, struct ist30xx_data,
						 early_suspend);

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);
	ist30xx_internal_suspend(data);
	clear_input_data(data);	
	mutex_unlock(&ist30xx_mutex);
}
static void ist30xx_late_resume(struct early_suspend *h)
{
	struct ist30xx_data *data = container_of(h, struct ist30xx_data,
						 early_suspend);

	mutex_lock(&ist30xx_mutex);
	ist30xx_internal_resume(data);
	ist30xx_start(data);
	ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);
}
#else
static int ist30xx_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	return ist30xx_internal_suspend(data);
}
static int ist30xx_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	return ist30xx_internal_resume(data);
}
#endif


void ist30xx_set_ta_mode(bool charging)
{
#if IST30XX_DETECT_TA
	if ((ist30xx_ta_status == -1) || (charging == ist30xx_ta_status))
		return;

	ist30xx_ta_status = charging ? 1 : 0;
	schedule_delayed_work(&work_reset_check, 0);
#endif
}
EXPORT_SYMBOL(ist30xx_set_ta_mode);

void ist30xx_set_temp_mode(int mode)
{
	/*
	 * #define NORMAL_TEMPERATURE      (0)
	 * #define LOW_TEMPERATURE         (1)
	 * #define HIGH_TEMPERATURE        (2)
	 */

#if IST30XX_DETECT_TEMPER
	if ((ist30xx_temp_status == -1) || (mode == ist30xx_temp_status))
		return;

	ist30xx_temp_status = mode;

	DMSG("[ TSP ] %s(), temperature = %d\n", __func__, ist30xx_temp_status);

	schedule_delayed_work(&work_reset_check, 0);
#endif
}
EXPORT_SYMBOL(ist30xx_set_temp_mode);


static void reset_work_func(struct work_struct *work)
{
	if ((ts_data == NULL) || (ts_data->client == NULL))
		return;

	DMSG("[ TSP ] Request reset function\n");

	if ((ts_data->status.power == 1) &&
	    (ts_data->status.update != 1) && (ts_data->status.calib != 1)) {
		mutex_lock(&ist30xx_mutex);
		ist30xx_disable_irq(ts_data);

		clear_input_data(ts_data);

		ist30xx_cmd_run_device(ts_data->client);

		ist30xx_start(ts_data);

		ist30xx_enable_irq(ts_data);
		mutex_unlock(&ist30xx_mutex);
	}
}

#if IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE
static void fw_update_func(struct work_struct *work)
{
	if ((ts_data == NULL) || (ts_data->client == NULL))
		return;

	DMSG("[ TSP ] FW update function\n");

	if (ist30xx_auto_bin_update(ts_data))
		ist30xx_disable_irq(ts_data);
}
#endif


#if IST30XX_NOISE_MODE
void timer_handler(unsigned long data)
{
	struct power_supply *ps;
	union power_supply_propval value;

	int event_ms;
	int curr_ms;
	int temper = 0;

	if (get_event_mode) {
		if ((ts_data->status.power == 1) && (ts_data->status.update != 1)) {
        ktime_get_ts(&t_current);

			curr_ms = t_current.tv_sec * 1000 + t_current.tv_nsec / 1000000;
			event_ms = t_event.tv_sec * 1000 + t_event.tv_nsec / 1000000;

			//printk("[ TSP ] event_ms %d, currnet: %d\n", event_ms, curr_ms);

			if (ts_data->status.calib == 1) {
				if (curr_ms - event_ms >= 3000) {
					ts_data->status.calib = 0;
					schedule_delayed_work(&work_reset_check, 0);
					ktime_get_ts(&t_event);
				}
			} else if (curr_ms - event_ms >= 5000) {
				pr_err("[ TSP ] idle timeout over 5sec\n");
				schedule_delayed_work(&work_reset_check, 0);
			}
#if IST30XX_DETECT_TEMPER
			else if (t_current.tv_sec - t_temper.tv_sec >= 30) { // 30second
				DMSG("[ TSP ] Temperature check timer. \n");

				ps = power_supply_get_by_name("battery");
				ps->get_property(ps, POWER_SUPPLY_PROP_TEMP, &value);

				temper = (value.intval/10);  // temperature read
				
				if (ist30xx_temp_status == NORMAL_TEMPERATURE) {
					if (temper < 15)        // 15 celcius
						ist30xx_set_temp_mode(LOW_TEMPERATURE);
					else if (temper > 50)   // 50 celcius
						ist30xx_set_temp_mode(HIGH_TEMPERATURE);
				} else if (ist30xx_temp_status == LOW_TEMPERATURE) {
					if (temper >= 20)        //  20 celcius
						ist30xx_set_temp_mode(NORMAL_TEMPERATURE);
				} else if (ist30xx_temp_status == HIGH_TEMPERATURE) {
					if (temper <= 40)       // 40 celcius
						ist30xx_set_temp_mode(NORMAL_TEMPERATURE);
				}

				t_temper = t_current;
			}
#endif // IST30XX_DETECT_TEMPER
		}
	}

	mod_timer(&idle_timer, get_jiffies_64() + IDLE_TIMER_INTERVAL);
}
#endif

static DEVICE_ATTR(close_tsp_test, S_IRUGO, show_close_tsp_test, NULL);
static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);

static struct attribute *sec_touch_facotry_attributes[] = {
	&dev_attr_close_tsp_test.attr,
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	NULL,
};
static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_facotry_attributes,
};

#define ENG_CALISTR				"calibration"

extern char global_cmd_line[1024];
int califlag=0;

static void isCalibration(void){

	if(strstr(global_cmd_line,ENG_CALISTR) != NULL)
		califlag = 1;
	
}

static int __devinit ist30xx_probe(struct i2c_client *		client,
				   const struct i2c_device_id * id)
{
	int i, ret;
	struct ist30xx_data *data;
	struct input_dev *input_dev;
	struct ist30xx_data *touch_dev;
	struct device *fac_dev_ts;	

	DMSG("[ TSP ] %s() ,the i2c addr=0x%x", __func__, client->addr);

	printk("[TSP] ist30xx_probe\n");

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	input_dev = input_allocate_device();
	if (!input_dev) {
		ret = -ENOMEM;
		pr_err("%s: input_allocate_device failed (%d)\n", __func__, ret);
		goto err_alloc_dev;
	}
	
	client->irq = gpio_to_irq(60);
	data->num_fingers = IST30XX_MAX_MT_FINGERS;
	data->irq_enabled = 1;
	data->client = client;
	data->input_dev = input_dev;
	i2c_set_clientdata(client, data);

	input_mt_init_slots(input_dev, IST30XX_MAX_MT_FINGERS);

	input_dev->name = "ist30xx_ts_input";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, IST30XX_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, IST30XX_MAX_Y, 0, 0);

#if IST30XX_USE_KEY
	{
		int i;
		set_bit(EV_KEY, input_dev->evbit);
		set_bit(EV_SYN, input_dev->evbit);
		for (i = 1; i < ARRAY_SIZE(ist30xx_key_code); i++)
			set_bit(ist30xx_key_code[i], input_dev->keybit);
	}
#endif

	input_set_drvdata(input_dev, data);
	ret = input_register_device(input_dev);
	if (ret) {
		input_free_device(input_dev);
		goto err_reg_dev;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = ist30xx_early_suspend;
	data->early_suspend.resume = ist30xx_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	ts_data = data;

	ret = ist30xx_init_system();
	if (ret) {
		dev_err(&client->dev, "chip initialization failed\n");
		goto err_init_drv;
	}

	ret = ist30xx_init_update_sysfs();
	if (ret)
		goto err_init_drv;

#if IST30XX_DEBUG
	ret = ist30xx_init_misc_sysfs();
	if (ret)
		goto err_init_drv;
#endif

	ret = request_threaded_irq(client->irq, NULL, ist30xx_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ist30xx_ts", data);
	if (ret)
		goto err_irq;

	ist30xx_disable_irq(data);
	isCalibration();
	
	if(califlag==0){

#if IST30XX_INTERNAL_BIN
# if IST30XX_UPDATE_BY_WORKQUEUE
			INIT_DELAYED_WORK(&work_fw_update, fw_update_func);
			schedule_delayed_work(&work_fw_update, IST30XX_UPDATE_DELAY);
# else
			ret = ist30xx_auto_bin_update(data);
			if (ret != 0)
		        goto err_irq;
# endif
#endif  // IST30XX_INTERNAL_BIN
	}
else
	printk("[TSP] Calibration mode F/W skip!!!");
	
	ist30xx_get_info(data);
	ist30xx_enable_irq(data);



# if SEC_FACTORY_TEST
	ist30xx_sec_sysfs();
	if (ret)
		goto err_init_drv;
#endif

	INIT_DELAYED_WORK(&work_reset_check, reset_work_func);

      /* sys fs */
	sec_touchscreen = device_create(sec_class, NULL, 0, NULL, "sec_touchscreen");
	if (IS_ERR(sec_touchscreen)) 
	{
		dev_err(&client->dev,"Failed to create device for the sysfs1\n");
		ret = -ENODEV;
	}

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_version_phone) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tsp_firm_version_phone.attr.name);
	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_version_panel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tsp_firm_version_panel.attr.name);
	if (device_create_file(sec_touchscreen, &dev_attr_tsp_threshold) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tsp_threshold.attr.name);
	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_update) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tsp_firm_update.attr.name);
	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_update_status) < 0)
		pr_err("[TSP] Failed to create device file(%s)!\n", dev_attr_tsp_firm_update_status.attr.name);

	sec_touchkey = device_create(sec_class, NULL, 0, NULL, "sec_touchkey");
	if (IS_ERR(sec_touchkey)) 
	{
		dev_err(&client->dev,"Failed to create device for the sysfs1\n");
		ret = -ENODEV;
	}

 	if (device_create_file(sec_touchkey, &dev_attr_touchkey_menu) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_menu.attr.name);
	if (device_create_file(sec_touchkey, &dev_attr_touchkey_back) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_back.attr.name);
	if (device_create_file(sec_touchkey, &dev_attr_touchkey_threshold) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_threshold.attr.name);

	touch_dev = kzalloc(sizeof(struct ist30xx_data), GFP_KERNEL);
	if (!touch_dev) {
		printk(KERN_ERR "unabled to allocate touch data \r\n");
		ret = -ENOMEM;
		goto err_reg_dev;
	}
	touch_dev->client = client;
	i2c_set_clientdata(client, touch_dev);

		INIT_LIST_HEAD(&touch_dev->cmd_list_head);
		for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
			list_add_tail(&tsp_cmds[i].list, &touch_dev->cmd_list_head);

		mutex_init(&touch_dev->cmd_lock);
		touch_dev->cmd_is_running = false;

	fac_dev_ts = device_create(sec_class, NULL, 0, touch_dev, "tsp");
	if (IS_ERR(fac_dev_ts))
		printk("Failed to create device for the sysfs\n");

	ret = sysfs_create_group(&fac_dev_ts->kobj,
			       &sec_touch_factory_attr_group);
	if (ret)
		dev_err(&client->dev, "Failed to create sysfs group\n");

#if IST30XX_DETECT_TA
	ist30xx_ta_status = 0;
#endif
#if IST30XX_DETECT_TEMPER
	ist30xx_temp_status = NORMAL_TEMPERATURE;
#endif

#if IST30XX_NOISE_MODE
	init_timer(&idle_timer);
	idle_timer.function = timer_handler;
	idle_timer.expires = jiffies_64 + (IDLE_TIMER_INTERVAL);

	mod_timer(&idle_timer, get_jiffies_64() + IDLE_TIMER_INTERVAL);
	ktime_get_ts(&t_event);
#if IST30XX_DETECT_TEMPER
	ktime_get_ts(&t_temper);
#endif
#endif

	return 0;

err_irq:
err_init_drv:
	ist30xx_disable_irq(data);
	free_irq(client->irq, data);
#if IST30XX_NOISE_MODE
	get_event_mode = false;
#endif
	pr_err("[ TSP ] Error, ist30xx init driver\n");
	ist30xx_power_off();
	input_unregister_device(input_dev);
	return 0;

err_reg_dev:
err_alloc_dev:
	pr_err("[ TSP ] Error, ist30xx mem free\n");
	kfree(data);
	return 0;
}


static int __devexit ist30xx_remove(struct i2c_client *client)
{
	struct ist30xx_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif

	free_irq(client->irq, data);
	ist30xx_power_off();

	input_unregister_device(data->input_dev);
	kfree(data);

	return 0;
}


static struct i2c_device_id ist30xx_idtable[] = {
	{ IST30XX_DEV_NAME, 0 },
	{},
};


MODULE_DEVICE_TABLE(i2c, ist30xx_idtable);

#ifdef CONFIG_HAS_EARLYSUSPEND
static const struct dev_pm_ops ist30xx_pm_ops = {
	.suspend	= ist30xx_suspend,
	.resume		= ist30xx_resume,
};
#endif


static struct i2c_driver ist30xx_i2c_driver = {
	.id_table	= ist30xx_idtable,
	.probe		= ist30xx_probe,
	.remove		= __devexit_p(ist30xx_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= IST30XX_DEV_NAME,
#ifdef CONFIG_HAS_EARLYSUSPEND
		.pm	= &ist30xx_pm_ops,
#endif
	},
};


static int __init ist30xx_init(void)
{
	return i2c_add_driver(&ist30xx_i2c_driver);
}


static void __exit ist30xx_exit(void)
{
	i2c_del_driver(&ist30xx_i2c_driver);
}

module_init(ist30xx_init);
module_exit(ist30xx_exit);

MODULE_DESCRIPTION("Imagis IST30XX touch driver");
MODULE_LICENSE("GPL");

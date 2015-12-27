/*
 * Copyright (c) 2011, 2012 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define COMMS_DEBUG 0

#define IRQ_DEBUG 0

#if COMMS_DEBUG || IRQ_DEBUG
#define DEBUG
#endif

#include <linux/kernel.h>
#include <linux/lockdep.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/rmi.h>
#include <linux/err.h>
#include "rmi_driver.h"

#define RMI_PAGE_SELECT_REGISTER 0xff
#define RMI_I2C_PAGE(addr) (((addr) >> 8) & 0xff)

static char *phys_proto_name = "i2c";

struct rmi_i2c_data {
	struct mutex page_mutex;
	int page;
	int enabled;
	int irq;
	int irq_flags;
	struct rmi_phys_device *phys;
};

///////////////////////////////////test mode//////////////////////////////////////

unsigned char touch_sw_ver = 0;
#define TSP_HW_VER		0x2200

#define SEC_TSP_FACTORY_TEST

#ifdef SEC_TSP_FACTORY_TEST
#define TSP_BUF_SIZE 1024

#define NODE_NUM	154	/* 11x14 */
#define NODE_X_NUM 11
#define NODE_Y_NUM 14

//unsigned int cm_abs[NODE_NUM]= {{0,},};;
//unsigned int cm_delta[NODE_NUM]= {{0,},};;

s16 cm_abs[NODE_NUM]= {{0,},};;
s16 cm_delta[NODE_NUM]= {{0,},};;

extern  const unsigned char SynaFirmware[];

#define TSP_CMD_STR_LEN 32
#define TSP_CMD_RESULT_STR_LEN 512
#define TSP_CMD_PARAM_NUM 8
#endif /* SEC_TSP_FACTORY_TEST */

enum {
	BUILT_IN = 0,
	UMS,
	REQ_FW,
};

struct device *sec_touchscreen;
EXPORT_SYMBOL(sec_touchscreen);
extern struct class *sec_class;

struct i2c_client *gb_client;

static char IsfwUpdate[20]={0};

#define FW_DOWNLOADING "Downloading"
#define FW_DOWNLOAD_COMPLETE "Complete"
#define FW_DOWNLOAD_FAIL "FAIL"
#define FWUP_NOW -1
#define FW_ADDRESS			0x34
u8 FW_KERNEL_VERSION[5] = {0, };
u8 FW_IC_VERSION[5] = {0, };
u8 FW_DATE[5] = {0, };

static ssize_t phone_firmware_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t part_firmware_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t threshold_firmware_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t config_firmware_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t firmware_update(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t firmware_update_status(struct device *dev, struct device_attribute *attr, char *buf);

static int read_ts_data(u8 address, u8 *buf, int size);

static ssize_t synaptics_menu_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t synaptics_home_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t synaptics_back_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf);

static DEVICE_ATTR(tsp_firm_version_phone, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, phone_firmware_show, NULL);
static DEVICE_ATTR(tsp_firm_version_panel, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, part_firmware_show, NULL);
static DEVICE_ATTR(tsp_threshold, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, threshold_firmware_show, NULL);
static DEVICE_ATTR(tsp_firm_update, 0777, firmware_update, firmware_update);
static DEVICE_ATTR(tsp_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, firmware_update_status, NULL);

static DEVICE_ATTR(touchkey_menu, S_IRUGO, synaptics_menu_sensitivity_show, NULL);
static DEVICE_ATTR(touchkey_home, S_IRUGO, synaptics_home_sensitivity_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, synaptics_back_sensitivity_show, NULL);

#define MAX_TOUCH_NUM			2
struct ts_data {
	struct i2c_client		*client;
	struct input_dev		*input_dev;
	struct early_suspend		early_suspend;
	//struct synaptics_platform_data	*platform_data;
	int				finger_state[MAX_TOUCH_NUM];
#if FACTORY_TESTING
	struct factory_data		*factory_data;
	struct node_data		*node_data;
#endif
#if TOUCH_BOOST
	struct timer_list		timer;
#endif
#if CONTROL_JITTER
	bool				jitter_on;
	bool				enable;
#endif

	unsigned char fw_ic_ver;

#if defined(SEC_TSP_FACTORY_TEST)
	struct list_head			cmd_list_head;
	unsigned char cmd_state;
	char			cmd[TSP_CMD_STR_LEN];
	int			cmd_param[TSP_CMD_PARAM_NUM];
	char			cmd_result[TSP_CMD_RESULT_STR_LEN];
	struct mutex			cmd_lock;
	bool			cmd_is_running;

	unsigned int reference[NODE_NUM];
	unsigned int raw[NODE_NUM]; /* CM_ABS */
	bool ft_flag;
#endif				/* SEC_TSP_FACTORY_TEST */

};

static bool fw_updater(struct ts_data *ts, char *mode);
static int read_ts_data(u8 address, u8 *buf, int size);
static u8 get_reg_address(const struct i2c_client *client, const int reg_name);
extern void set_fw_version(char *FW_KERNEL_VERSION, char* FW_DATE);


extern bool fw_update_internal(struct i2c_client *ts_client);
extern bool fw_update_file(struct i2c_client *ts_client);

#if defined(SEC_TSP_FACTORY_TEST)
#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func

struct tsp_cmd {
	struct list_head	list;
	const char	*cmd_name;
	void	(*cmd_func)(void *device_data);
};

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void get_threshold(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void module_off_slave(void *device_data);
static void module_on_slave(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_reference(void *device_data);
static void get_cm_abs(void *device_data);
static void get_rawcap(void *device_data);
static void get_cm_delta(void *device_data);
static void get_rx_to_rx(void *device_data);
static void get_intensity(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void run_reference_read(void *device_data);
static void run_cm_abs_read(void *device_data);
static void run_rawcap_read(void *device_data);
static void run_cm_delta_read(void *device_data);
static void run_rx_to_rx_read(void *device_data);
static void run_intensity_read(void *device_data);
static void not_support_cmd(void *device_data);
static void run_raw_node_read(void *device_data);

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
	{TSP_CMD("run_raw_node_read", run_raw_node_read),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("get_reference", not_support_cmd),},
	{TSP_CMD("get_cm_abs", get_cm_abs),}, //
	{TSP_CMD("get_rawcap", get_rawcap),}, //
	{TSP_CMD("get_cm_delta", get_cm_delta),}, //
	{TSP_CMD("get_rx_to_rx", get_rx_to_rx),}, //
	{TSP_CMD("get_intensity", not_support_cmd),},
	{TSP_CMD("run_reference_read", not_support_cmd),},
	{TSP_CMD("run_cm_abs_read", run_cm_abs_read),}, //
	{TSP_CMD("run_rawcap_read", run_rawcap_read),}, //
	{TSP_CMD("run_cm_delta_read", run_cm_delta_read),}, //
	{TSP_CMD("run_rx_to_rx_read", run_rx_to_rx_read),}, //
	{TSP_CMD("run_intensity_read", not_support_cmd),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};
#endif


///////////////////////////////////test mode//////////////////////////////////////

static irqreturn_t rmi_i2c_irq_thread(int irq, void *p)
{
	struct rmi_phys_device *phys = p;
	struct rmi_device *rmi_dev = phys->rmi_dev;
	struct rmi_driver *driver = rmi_dev->driver;
	struct rmi_device_platform_data *pdata = phys->dev->platform_data;

#if IRQ_DEBUG
	dev_info(phys->dev, "ATTN gpio, value: %d.\n",
			gpio_get_value(pdata->attn_gpio));
#endif
	if (gpio_get_value(pdata->attn_gpio) == pdata->attn_polarity) {
		phys->info.attn_count++;
		if (driver && driver->irq_handler && rmi_dev)
			driver->irq_handler(rmi_dev, irq);
	}

	return IRQ_HANDLED;
}

/*
 * rmi_set_page - Set RMI page
 * @phys: The pointer to the rmi_phys_device struct
 * @page: The new page address.
 *
 * RMI devices have 16-bit addressing, but some of the physical
 * implementations (like SMBus) only have 8-bit addressing. So RMI implements
 * a page address at 0xff of every page so we can reliable page addresses
 * every 256 registers.
 *
 * The page_mutex lock must be held when this function is entered.
 *
 * Returns zero on success, non-zero on failure.
 */
static int rmi_set_page(struct rmi_phys_device *phys, unsigned int page)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_i2c_data *data = phys->data;
	char txbuf[2] = {RMI_PAGE_SELECT_REGISTER, page};
	int retval;

#if COMMS_DEBUG
	dev_info(&client->dev, "RMI4 I2C writes 3 bytes: %02x %02x\n",
		txbuf[0], txbuf[1]);
#endif
	phys->info.tx_count++;
	phys->info.tx_bytes += sizeof(txbuf);
	retval = i2c_master_send(client, txbuf, sizeof(txbuf));
	if (retval != sizeof(txbuf)) {
		phys->info.tx_errs++;
		dev_err(&client->dev,
			"%s: set page failed: %d.", __func__, retval);
		return (retval < 0) ? retval : -EIO;
	}
	data->page = page;
	return 0;
}

static int rmi_i2c_write_block(struct rmi_phys_device *phys, u16 addr, u8 *buf,
			       int len)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_i2c_data *data = phys->data;
	u8 txbuf[len + 1];
	int retval;
#if	COMMS_DEBUG
	int i;
#endif

	txbuf[0] = addr & 0xff;
	memcpy(txbuf + 1, buf, len);

	mutex_lock(&data->page_mutex);

	if (RMI_I2C_PAGE(addr) != data->page) {
		retval = rmi_set_page(phys, RMI_I2C_PAGE(addr));
		if (retval < 0)
			goto exit;
	}

#if COMMS_DEBUG
	dev_info(&client->dev, "RMI4 I2C writes %d bytes: ", sizeof(txbuf));
	for (i = 0; i < sizeof(txbuf); i++)
		dev_info(&client->dev, "%02x ", txbuf[i]);
	dev_info(&client->dev, "\n");
#endif

	phys->info.tx_count++;
	phys->info.tx_bytes += sizeof(txbuf);
	retval = i2c_master_send(client, txbuf, sizeof(txbuf));
	if (retval < 0)
		phys->info.tx_errs++;
	else
		retval--; /* don't count the address byte */

exit:
	mutex_unlock(&data->page_mutex);
	return retval;
}

static int rmi_i2c_write(struct rmi_phys_device *phys, u16 addr, u8 data)
{
	int retval = rmi_i2c_write_block(phys, addr, &data, 1);
	return (retval < 0) ? retval : 0;
}

static int rmi_i2c_read_block(struct rmi_phys_device *phys, u16 addr, u8 *buf,
			      int len)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_i2c_data *data = phys->data;
	u8 txbuf[1] = {addr & 0xff};
	int retval;
#if	COMMS_DEBUG
	int i;
#endif

	mutex_lock(&data->page_mutex);

	if (RMI_I2C_PAGE(addr) != data->page) {
		retval = rmi_set_page(phys, RMI_I2C_PAGE(addr));
		if (retval < 0)
			goto exit;
	}

#if COMMS_DEBUG
	dev_info(&client->dev, "RMI4 I2C writes 1 bytes: %02x\n", txbuf[0]);
#endif
	phys->info.tx_count++;
	phys->info.tx_bytes += sizeof(txbuf);
	retval = i2c_master_send(client, txbuf, sizeof(txbuf));
	if (retval != sizeof(txbuf)) {
		phys->info.tx_errs++;
		retval = (retval < 0) ? retval : -EIO;
		goto exit;
	}

	retval = i2c_master_recv(client, buf, len);

	phys->info.rx_count++;
	phys->info.rx_bytes += len;
	if (retval < 0)
		phys->info.rx_errs++;
#if COMMS_DEBUG
	else {
		dev_info(&client->dev, "RMI4 I2C received %d bytes: ", len);
		for (i = 0; i < len; i++)
			dev_info(&client->dev, "%02x ", buf[i]);
		dev_info(&client->dev, "\n");
	}
#endif

exit:
	mutex_unlock(&data->page_mutex);
	return retval;
}

static int rmi_i2c_read(struct rmi_phys_device *phys, u16 addr, u8 *buf)
{
	int retval = rmi_i2c_read_block(phys, addr, buf, 1);
	return (retval < 0) ? retval : 0;
}

static int acquire_attn_irq(struct rmi_i2c_data *data)
{
	return request_threaded_irq(data->irq, NULL, rmi_i2c_irq_thread,
			data->irq_flags, dev_name(data->phys->dev), data->phys);
}

static int enable_device(struct rmi_phys_device *phys)
{
	int retval = 0;

	struct rmi_i2c_data *data = phys->data;

	if (data->enabled)
		return 0;

	retval = acquire_attn_irq(data);
	if (retval)
		goto error_exit;

	data->enabled = true;
	dev_info(phys->dev, "Physical device enabled.\n");
	return 0;

error_exit:
	dev_err(phys->dev, "Failed to enable physical device. Code=%d.\n",
		retval);
	return retval;
}

static void disable_device(struct rmi_phys_device *phys)
{
	struct rmi_i2c_data *data = phys->data;

	if (!data->enabled)
		return;

	disable_irq(data->irq);
	free_irq(data->irq, data->phys);

	dev_info(phys->dev, "Physical device disabled.\n");
	data->enabled = false;
}


#ifdef SEC_TSP_FACTORY_TEST
static void set_cmd_result(struct ts_data *info, char *buff, int len)
{
	strncat(info->cmd_result, buff, len);
}


static int get_hw_version(struct ts_data *info)
{
	return TSP_HW_VER;
}


static ssize_t show_close_tsp_test(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return snprintf(buf, TSP_BUF_SIZE, "%u\n", 0);
}

static void set_default_result(struct ts_data *info)
{
	char delim = ':';

	memset(info->cmd_result, 0x00, ARRAY_SIZE(info->cmd_result));
	memcpy(info->cmd_result, info->cmd, strlen(info->cmd));
	strncat(info->cmd_result, &delim, 1);
}

static void not_support_cmd(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;
	char buff[16] = {0};

	set_default_result(info);
	sprintf(buff, "%s", "NA");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 4;
	dev_info(&info->client->dev, "%s: \"%s(%d)\"\n", __func__,
				buff, strnlen(buff, sizeof(buff)));
	return;
}
#if 0
static void fw_update(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	not_support_cmd(info);
}
#endif

extern bool not_reset;
extern bool F54_SetRawCapData(struct i2c_client *ts_client, s16 *node_data);
static void run_raw_node_read(void *device_data)
{

	not_reset = 1;

	struct ts_data *info = (struct ts_data *)device_data;

	char buff[TSP_CMD_STR_LEN] = {0};
	u32 max_value = 0, min_value = 0;
    	int i, j;

    	disable_irq(info->client->irq);

    	printk("[TSP] %s, %d\n", __func__, __LINE__ );

	set_default_result(info);

	F54_SetRawCapData(gb_client, cm_abs);

	int k = 0;
	for (i = 0; i < 11; i++) {
		printk(KERN_CONT "[TSP]");
		for (j = 0; j < 14; j++) {
			printk(KERN_CONT " %d", cm_abs[k]);
			k++;
		}
		printk(KERN_CONT "\n");
	}


	for(i = 0; i < NODE_NUM; i++)
	{
		if (cm_abs[i]>0)
		{
			if(i==0)
			{
				min_value=max_value=cm_abs[i];
			}
			else
			{
				max_value = max(max_value, cm_abs[i]);
				min_value = min(min_value, cm_abs[i]);
			}
		}
	}

	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));

	enable_irq(info->client->irq);
	not_reset = 0;
    
	return;

}

static void fw_update(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;
	int ret;

	printk("[TSP] %s, %d\n", __func__, __LINE__ );

	set_default_result(info);
    	ret =  fw_update_internal(gb_client);
	info->cmd_state = 2;

}


static void get_fw_ver_bin(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	char buff[16] = {0};
	int hw_rev;
	int phone_ver;

	u8 FW_BIN_VERSION[5] = {0, };

	printk("[TSP] %s, %d\n", __func__, __LINE__ );


	set_default_result(info);
	hw_rev = get_hw_version(info);

	FW_BIN_VERSION[0] = SynaFirmware[0xb100];
	FW_BIN_VERSION[1] = SynaFirmware[0xb101];
	FW_BIN_VERSION[2] = SynaFirmware[0xb102];
	FW_BIN_VERSION[3] = SynaFirmware[0xb103];
	FW_BIN_VERSION[4] = '\0';
    
	//phone_ver = FW_KERNEL_VERSION;
    
	if (hw_rev == 0x2200)
		snprintf(buff, sizeof(buff), "%s", FW_BIN_VERSION);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	char buff[16] = {0};
	int ret, ver;
	u8 buf_temp[4] = {0, };
	buf_temp[2] = 0;

	printk("[TSP] %s, %d\n", __func__, __LINE__ );
	
	set_default_result(info);
	ret = read_ts_data(get_reg_address(gb_client, FW_ADDRESS), buf_temp, 4);	

			strncpy(FW_IC_VERSION, buf_temp, 5);
			pr_info("tsp: fw. ver. : IC (%s), Internal (%s)\n",
				(char *)FW_IC_VERSION,
				(char *)FW_KERNEL_VERSION);
	snprintf(buff, sizeof(buff), "%s",(char *)FW_IC_VERSION);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_config_ver(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	not_support_cmd(info);
}

static void get_threshold(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	not_support_cmd(info);
}

static void module_off_master(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	not_support_cmd(info);
}

static void module_on_master(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	not_support_cmd(info);
}

static void module_off_slave(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	not_support_cmd(info);
}

static void module_on_slave(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	not_support_cmd(info);
}

static void get_chip_vendor(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	char buff[16] = {0};

	printk("[TSP] %s, %d\n", __func__, __LINE__ );

	set_default_result(info);

	snprintf(buff, sizeof(buff), "%s", "SYNAPTICS");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_chip_name(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	char buff[16] = {0};

	printk("[TSP] %s, %d\n", __func__, __LINE__ );

	set_default_result(info);

	snprintf(buff, sizeof(buff), "%s", "S2200");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static int check_rx_tx_num(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

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

static void get_reference(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	not_support_cmd(info);
    
}

static void get_cm_abs(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

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
	struct ts_data *info = (struct ts_data *)device_data;

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

static void get_cm_delta(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	char buff[16] = {0};
	//unsigned int val;
	int val;
	int node;

 	printk("[TSP] %s, %d\n", __func__, __LINE__ );

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;

	val = cm_delta[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_rx_to_rx(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	char buff[16] = {0};
	//unsigned int val;
	int val;
	int node;

    printk("[TSP] %s, %d\n", __func__, __LINE__ );
    
	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;
    
	val = cm_delta[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_intensity(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	not_support_cmd(info);
}

static void get_x_num(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	char buff[16] = {0};
	//int ver;

	printk("[TSP] %s, %d\n", __func__, __LINE__ );
	
	set_default_result(info);

	//ver = info->fw_ic_ver;
	snprintf(buff, sizeof(buff), "%d", NODE_X_NUM);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_y_num(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	char buff[16] = {0};
	//int ver;

	printk("[TSP] %s, %d\n", __func__, __LINE__ );
	
	set_default_result(info);

	//ver = info->fw_ic_ver;
	snprintf(buff, sizeof(buff), "%d", NODE_Y_NUM);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void run_reference_read(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	not_support_cmd(info);

/*	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__); */
}

static void run_cm_abs_read(void *device_data)
{
	not_reset = 1;
	
	struct ts_data *info = (struct ts_data *)device_data;

    	disable_irq(info->client->irq);

    	printk("[TSP] %s, %d\n", __func__, __LINE__ );
    	int i, j;

	set_default_result(info);

	F54_SetRawCapData(gb_client, cm_abs);

	int k = 0;
	for (i = 0; i < 11; i++) {
		printk(KERN_CONT "[TSP]");
		for (j = 0; j < 14; j++) {
			printk(KERN_CONT " %d", cm_abs[k]);
			k++;
		}
		printk(KERN_CONT "\n");
	}

    	info->cmd_state = 2;
    enable_irq(info->client->irq);
   not_reset = 0;
    
    	return;

/*	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__); */
}

static void run_rawcap_read(void *device_data)
{
	not_reset = 1;
	
	struct ts_data *info = (struct ts_data *)device_data;

    	disable_irq(info->client->irq);

    	printk("[TSP] %s, %d\n", __func__, __LINE__ );
        
    	char buff[TSP_CMD_STR_LEN] = {0};
    	u32 max_value = 0, min_value = 0;
    	int i, j;

	set_default_result(info);

	F54_SetRawCapData(gb_client, cm_abs);

	int k = 0;
	for (i = 0; i < 11; i++) {
		printk(KERN_CONT "[TSP]");
		for (j = 0; j < 14; j++) {
			printk(KERN_CONT " %d", cm_abs[k]);
			k++;
		}
		printk(KERN_CONT "\n");
	}

	for(i = 0; i < NODE_NUM; i++)
	{
		if (cm_abs[i]>0)
		{
			if(i==0)
			{
				min_value=max_value=cm_abs[i];
			}
			else
			{
				max_value = max(max_value, cm_abs[i]);
				min_value = min(min_value, cm_abs[i]);
			}
	}
	}

	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

    	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff, strnlen(buff, sizeof(buff)));

    enable_irq(info->client->irq);
   not_reset = 0;
    
  return;
}

extern bool F54_SetDeltaImageData(struct i2c_client *ts_client, s16 *node_data);
static void run_cm_delta_read(void *device_data)
{
	not_reset = 1;
	
	struct ts_data *info = (struct ts_data *)device_data;

    	disable_irq(info->client->irq);

    	printk("[TSP] %s, %d\n", __func__, __LINE__ );
     	int i, j;

	set_default_result(info);

	F54_SetDeltaImageData(gb_client, cm_delta);

	int k = 0;
	for (i = 0; i < 11; i++) {
		printk(KERN_CONT "[TSP]");
		for (j = 0; j < 14; j++) {
			printk(KERN_CONT " %d", cm_delta[k]);
			k++;
		}
		printk(KERN_CONT "\n");
	}

    	info->cmd_state = 2;
    	enable_irq(info->client->irq);
    	not_reset = 0;
       
    	return;

/*	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__); */
}

static void run_rx_to_rx_read(void *device_data)
{
	not_reset = 1;
	
	struct ts_data *info = (struct ts_data *)device_data;

    	disable_irq(info->client->irq);

    	printk("[TSP] %s, %d\n", __func__, __LINE__ );
     	int i, j;

	set_default_result(info);

	F54_SetDeltaImageData(gb_client, cm_delta);

	int k = 0;
	for (i = 0; i < 11; i++) {
		printk(KERN_CONT "[TSP]");
		for (j = 0; j < 14; j++) {
			printk(KERN_CONT " %d", cm_delta[k]);
			k++;
		}
		printk(KERN_CONT "\n");
	}

    	info->cmd_state = 2;
    	enable_irq(info->client->irq);
    	not_reset = 0;
       
    	return;

/*	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__); */
}

static void run_intensity_read(void *device_data)
{
	struct ts_data *info = (struct ts_data *)device_data;

	not_support_cmd(info);

/*	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__); */
}

static ssize_t store_cmd(struct device *dev, struct device_attribute
				  *devattr, const char *buf, size_t count)
{
	struct ts_data *info = dev_get_drvdata(dev);
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

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct ts_data *info = dev_get_drvdata(dev);
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
	struct ts_data *info = dev_get_drvdata(dev);

	dev_info(&info->client->dev, "tsp cmd: result: %s\n", info->cmd_result);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = 0;

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", info->cmd_result);
}


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
#endif /* SEC_TSP_FACTORY_TEST */


static int __devinit rmi_i2c_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct rmi_phys_device *rmi_phys;
	struct rmi_i2c_data *data;
	struct rmi_device_platform_data *pdata = client->dev.platform_data;

	struct ts_data *touch_dev;
    	int i;
    
	int error;
	int ret = 0;
    

	#ifdef SEC_TSP_FACTORY_TEST
	    struct device *fac_dev_ts;
	#endif
    
	if (!pdata) {
		dev_err(&client->dev, "no platform data\n");
		return -EINVAL;
	}
	dev_info(&client->dev, "Probing sensor %s at %#02x (IRQ %d).\n",
		pdata->sensor_name ? pdata->sensor_name : "-no name-",
		client->addr, pdata->attn_gpio);

	if (pdata->gpio_config) {
		dev_info(&client->dev, "Configuring GPIOs.\n");
		error = pdata->gpio_config(pdata->gpio_data, true);
		if (error < 0) {
			dev_err(&client->dev, "Failed to configure GPIOs, code: %d.\n",
				error);
			return error;
		}
		dev_info(&client->dev, "Done with GPIO configuration.\n");
	}

	error = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!error) {
		dev_err(&client->dev, "i2c_check_functionality error %d.\n",
			error);
		return error;
	}

	rmi_phys = kzalloc(sizeof(struct rmi_phys_device), GFP_KERNEL);
	if (!rmi_phys)
		return -ENOMEM;

	data = kzalloc(sizeof(struct rmi_i2c_data), GFP_KERNEL);
	if (!data) {
		error = -ENOMEM;
		goto err_phys;
	}

	data->enabled = true;	/* We plan to come up enabled. */
	data->irq = gpio_to_irq(pdata->attn_gpio);
	if (pdata->level_triggered) {
		data->irq_flags = IRQF_ONESHOT |
			((pdata->attn_polarity == RMI_ATTN_ACTIVE_HIGH) ?
			IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW);
	} else {
		data->irq_flags =
			(pdata->attn_polarity == RMI_ATTN_ACTIVE_HIGH) ?
			IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING;
	}
	data->phys = rmi_phys;

	rmi_phys->data = data;
	rmi_phys->dev = &client->dev;

	rmi_phys->write = rmi_i2c_write;
	rmi_phys->write_block = rmi_i2c_write_block;
	rmi_phys->read = rmi_i2c_read;
	rmi_phys->read_block = rmi_i2c_read_block;
	rmi_phys->enable_device = enable_device;
	rmi_phys->disable_device = disable_device;

	rmi_phys->info.proto = phys_proto_name;

	mutex_init(&data->page_mutex);

	/* Setting the page to zero will (a) make sure the PSR is in a
	 * known state, and (b) make sure we can talk to the device.
	 */
	error = rmi_set_page(rmi_phys, 0);
	if (error) {
		dev_err(&client->dev, "Failed to set page select to 0.\n");
		goto err_data;
	}

	error = rmi_register_phys_device(rmi_phys);
	if (error) {
		dev_err(&client->dev,
			"failed to register physical driver at 0x%.2X.\n",
			client->addr);
		goto err_gpio;
	}
	i2c_set_clientdata(client, rmi_phys);

	if (pdata->attn_gpio > 0) {
		error = acquire_attn_irq(data);
		if (error < 0) {
			dev_err(&client->dev,
				"request_threaded_irq failed %d\n",
				pdata->attn_gpio);
			goto err_unregister;
		}
	}

#if defined(CONFIG_RMI4_DEV)
	error = gpio_export(pdata->attn_gpio, false);
	if (error) {
		dev_warn(&client->dev, "%s: WARNING: Failed to "
				 "export ATTN gpio!\n", __func__);
		error = 0;
	} else {
		error = gpio_export_link(&(rmi_phys->rmi_dev->dev), "attn",
					pdata->attn_gpio);
		if (error) {
			dev_warn(&(rmi_phys->rmi_dev->dev),
				 "%s: WARNING: Failed to symlink ATTN gpio!\n",
				 __func__);
			error = 0;
		} else {
			dev_info(&(rmi_phys->rmi_dev->dev),
				"%s: Exported GPIO %d.", __func__,
				pdata->attn_gpio);
		}
	}
#endif /* CONFIG_RMI4_DEV */

	dev_info(&client->dev, "registered rmi i2c driver at 0x%.2X.\n",
			client->addr);




///////////////////////////////////////test mode///////////////////////////////////////

      gb_client = client;

      /* sys fs */
	sec_touchscreen = device_create(sec_class, NULL, 0, rmi_phys, "sec_touchscreen");
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
 	if (device_create_file(sec_touchscreen, &dev_attr_touchkey_menu) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_menu.attr.name);
	if (device_create_file(sec_touchscreen, &dev_attr_touchkey_back) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_back.attr.name);

	touch_dev = kzalloc(sizeof(struct ts_data), GFP_KERNEL);
	if (!touch_dev) {
		printk(KERN_ERR "unabled to allocate touch data \r\n");
		ret = -ENOMEM;
		goto err_phys;
	}
	touch_dev->client = client;
	i2c_set_clientdata(client, touch_dev);

    #ifdef SEC_TSP_FACTORY_TEST
		INIT_LIST_HEAD(&touch_dev->cmd_list_head);
		for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
			list_add_tail(&tsp_cmds[i].list, &touch_dev->cmd_list_head);

		mutex_init(&touch_dev->cmd_lock);
		touch_dev->cmd_is_running = false;

	fac_dev_ts = device_create(sec_class, NULL, 0, touch_dev, "tsp");
	if (IS_ERR(fac_dev_ts))
		dev_err(&client->dev, "Failed to create device for the sysfs\n");

	ret = sysfs_create_group(&fac_dev_ts->kobj,	&sec_touch_factory_attr_group);
	if (ret)
		dev_err(&client->dev, "Failed to create sysfs group\n");
#endif

      /* sys fs */

    //touch_dev->fw_ic_ver = touch_dev->cap_info.chip_reg_data_version;

///////////////////////////////////////test mode///////////////////////////////////////


	/* Check the new fw. and update */
	set_fw_version(FW_KERNEL_VERSION, FW_DATE);  
	fw_updater(client, "normal");                                

	printk("rmi_i2c_probe done");
    
	return 0;

err_unregister:
	rmi_unregister_phys_device(rmi_phys);
err_gpio:
	if (pdata->gpio_config)
		pdata->gpio_config(pdata->gpio_data, false);
err_data:
	kfree(data);
err_phys:
	kfree(rmi_phys);
	return error;
}

static int __devexit rmi_i2c_remove(struct i2c_client *client)
{
	struct rmi_phys_device *phys = i2c_get_clientdata(client);
	struct rmi_device_platform_data *pd = client->dev.platform_data;

	disable_device(phys);
	rmi_unregister_phys_device(phys);
	kfree(phys->data);
	kfree(phys);

	if (pd->gpio_config)
		pd->gpio_config(&pd->gpio_data, false);

	return 0;
}

static const struct i2c_device_id rmi_id[] = {
	{ "rmi", 0 },
	{ "rmi_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rmi_id);

static struct i2c_driver rmi_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "rmi_i2c"
	},
	.id_table	= rmi_id,
	.probe		= rmi_i2c_probe,
	.remove		= __devexit_p(rmi_i2c_remove),
};

static int __init rmi_i2c_init(void)
{
    printk("[TSP] rmi_i2c_init\n");
    
	return i2c_add_driver(&rmi_i2c_driver);
}

static void __exit rmi_i2c_exit(void)
{
	i2c_del_driver(&rmi_i2c_driver);
}


//////////////////////////////////////test mode /////////////////////////////////////


static int read_ts_data(u8 address, u8 *buf, int size)
{
	if (i2c_master_send(gb_client, &address, 1) < 0)
		return -1;

	if (i2c_master_recv(gb_client, buf, size) < 0)
		return -1;

	return 1;
}

static ssize_t phone_firmware_show(struct device *dev, struct device_attribute *attr, char *buf)
{   
	printk("[TSP] %s\n",__func__);

	FW_KERNEL_VERSION[0] = SynaFirmware[0xb100];
	FW_KERNEL_VERSION[1] = SynaFirmware[0xb101];
	FW_KERNEL_VERSION[2] = SynaFirmware[0xb102];
	FW_KERNEL_VERSION[3] = SynaFirmware[0xb103];
	FW_KERNEL_VERSION[4] = '\0';


    printk("[TSP] %s, %c !!! \n",__func__,FW_KERNEL_VERSION[0]);
    printk("[TSP] %s, %c !!! \n",__func__,FW_KERNEL_VERSION[1]);
    printk("[TSP] %s, %c !!! \n",__func__,FW_KERNEL_VERSION[2]);
    printk("[TSP] %s, %c !!! \n",__func__,FW_KERNEL_VERSION[3]);
    printk("[TSP] %s, %c !!! \n",__func__,FW_KERNEL_VERSION[4]);
    printk("[TSP] %s, %c !!! \n",__func__,FW_KERNEL_VERSION[5]);
    printk("[TSP] %s, %c !!! \n",__func__,FW_KERNEL_VERSION[6]);
    printk("[TSP] %s, %c !!! \n",__func__,FW_KERNEL_VERSION[7]);

    printk("[TSP] %s, %s !!! \n",__func__,FW_KERNEL_VERSION);

	return sprintf(buf, "%s", FW_KERNEL_VERSION);
}

static ssize_t part_firmware_show(struct device *dev, struct device_attribute *attr, char *buf)
{   
	printk("[TSP] %s\n",__func__);
	u8 buf_temp[5] = {0, };
	int ret=0;
	buf_temp[2] = 0;

	ret = read_ts_data(get_reg_address(gb_client, FW_ADDRESS), buf_temp, 4);	

			strncpy(FW_IC_VERSION, buf_temp, 5);
			pr_info("tsp: fw. ver. : IC (%s), Internal (%s)\n",
				(char *)FW_IC_VERSION,
				(char *)FW_KERNEL_VERSION);
    
	return sprintf(buf, "%s",  (char *)FW_IC_VERSION);
}

#define THRESHOLD_REG			0x47       // z Touch Threshold
static ssize_t threshold_firmware_show(struct device *dev, struct device_attribute *attr, char *buf)
{   
	printk("[TSP] %s\n",__func__);

	u8 buf_temp[3] = {0, };

	read_ts_data(THRESHOLD_REG, buf_temp, 1);

	printk("[TSP] %s, %.3d !!! \n",__func__,buf_temp[0]);

	return sprintf(buf, "%.3d", buf_temp[0] );
}

static ssize_t config_firmware_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    printk("[TSP] %s\n",__func__);

    return sprintf(buf, "%s", buf );
}

static ssize_t firmware_update(struct device *dev, struct device_attribute *attr, char *buf)
{
    bool ret;
    printk("[TSP] %s\n",__func__);
    sprintf(IsfwUpdate,"%s\n",FW_DOWNLOADING);

    ret =  fw_update_internal(gb_client);

    if(ret == true)
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
	printk("[TSP] %s\n",__func__);

	return sprintf(buf, "%s\n", IsfwUpdate);
}


static u8 get_reg_address(const struct i2c_client *client, const int reg_name)
{
	u8 ret = 0;
	u8 address;
	u8 buffer[6];

	for (address = 0xE9; address > 0xD0; address -= 6) {
		read_ts_data(address, buffer, 6);

		if (buffer[5] == 0)
			break;
		switch (buffer[5]) {
		case FW_ADDRESS:
			ret = buffer[2];
			break;
		}
	}

	return ret;
}


static bool fw_updater(struct ts_data *ts, char *mode)
{
	u8 buf[5] = {0, };
	bool ret = false;

	pr_info("Enter the fw_updater.");

	/* To check whether touch IC in bootloader mode.
	 * It means that fw. update failed at previous booting.
	 */
	if (read_ts_data(0x14, buf, 1) > 0) {
		if (buf[0] == 0x01)
			mode = "force";
	}

	if (!strcmp("force", mode)) {
		pr_info("tsp: fw_updater: FW force upload.\n");
		ret = fw_update_internal(gb_client);
	} else if (!strcmp("file", mode)) {
		pr_info("tsp: fw_updater: FW force upload from bin. file.\n");
		ret = fw_update_file(gb_client);
	} else if (!strcmp("normal", mode)) {
		if (read_ts_data(get_reg_address(gb_client, FW_ADDRESS), buf, 4) > 0) {
			strncpy(FW_IC_VERSION, buf, 5);
			pr_info("tsp: fw. ver. : IC (%s), Internal (%s)\n",
				(char *)FW_IC_VERSION,
				(char *)FW_KERNEL_VERSION);
		} else {
			pr_err("tsp: fw. ver. read failed.");
			return false;
		}

		if (strcmp(FW_KERNEL_VERSION, FW_IC_VERSION) > 0) {
			pr_info("tsp: fw_updater: FW upgrade enter.\n");
			ret = fw_update_internal(gb_client);
		} else
			pr_info("tsp: fw_updater: No need FW update.\n");
	}

	pr_info("tsp: fw. update complete.");
	return ret;
}

s16 touchkey_raw[2];
extern bool F54_ButtonDeltaImage(struct i2c_client *ts_client, s16 *node_data);


static ssize_t synaptics_menu_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 menu_sensitivity=0;
    
	F54_ButtonDeltaImage(gb_client, touchkey_raw);

      printk("[TSP] %s, touchkey_raw[0] : %d !!! \n", __func__, touchkey_raw[0]);
      printk("[TSP] %s, touchkey_raw[1] : %d !!! \n", __func__, touchkey_raw[1]);

      if(touchkey_raw[0] < 0)
      {
        touchkey_raw[0] = 0;
      }

      if(touchkey_raw[1] < 0)
      {
        touchkey_raw[1] = 0;
      }

	menu_sensitivity = touchkey_raw[0];
      
	return sprintf(buf, "%d\n",  menu_sensitivity);
}

static ssize_t synaptics_back_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 back_sensitivity=0;

	F54_ButtonDeltaImage(gb_client, touchkey_raw);
    
      printk("[TSP] %s, touchkey_raw[0] : %d !!! \n", __func__, touchkey_raw[0]);
      printk("[TSP] %s, touchkey_raw[1] : %d !!! \n", __func__, touchkey_raw[1]);

      if(touchkey_raw[0] < 0)
      {
        touchkey_raw[0] = 0;
      }

      if(touchkey_raw[1] < 0)
      {
        touchkey_raw[1] = 0;
      }

	back_sensitivity = touchkey_raw[1];

	return sprintf(buf, "%d\n",  back_sensitivity);
}

//////////////////////////////////////test mode /////////////////////////////////////

module_init(rmi_i2c_init);
module_exit(rmi_i2c_exit);

MODULE_AUTHOR("Christopher Heiny <cheiny@synaptics.com>");
MODULE_DESCRIPTION("RMI I2C driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(RMI_DRIVER_VERSION);

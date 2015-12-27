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
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c/pixcir_i2c_ts.h>
#include <mach/regulator.h>


/*********************************Bee-0928-TOP****************************************/

static unsigned char debug_level=PIXCIR_DEBUG;

#define PIXCIR_DBG(format, ...)	\
	if(debug_level == 1)	\
		printk(KERN_INFO "PIXCIR " format "\n", ## __VA_ARGS__)

static struct pixcir_ts_struct *g_pixcir_ts;
static unsigned char status_reg = 0;
static struct point_node_t point_slot[MAX_FINGER_NUM*2];
static struct point_node_t point_slot_back[MAX_FINGER_NUM*2];
static int distance[5]={0};
static int touch_flage[5]={0};
static struct i2c_driver pixcir_i2c_ts_driver;
static struct class *i2c_dev_class;
static LIST_HEAD( i2c_dev_list);
static DEFINE_SPINLOCK( i2c_dev_list_lock);


static ssize_t pixcir_set_calibrate(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t pixcir_show_suspend(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t pixcir_store_suspend(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t pixcir_show_debug(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t pixcir_store_debug(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);

static void pixcir_reset(int reset_pin);
static void pixcir_ts_suspend(struct early_suspend *handler);
static void pixcir_ts_resume(struct early_suspend *handler);
static void pixcir_ts_pwron(struct regulator *reg_vdd);
static int pixcir_tx_config(void);
static DEVICE_ATTR(calibrate, 0660, NULL, pixcir_set_calibrate);
static DEVICE_ATTR(suspend, 0660, pixcir_show_suspend, pixcir_store_suspend);
static DEVICE_ATTR(debug, 0660, pixcir_show_debug, pixcir_store_debug);


/* pixcir_i2c_rxdata --  read data from i2c
 * @rxdata: read buffer, first output variable
 * @length: read length, second input variable
 * @return: eror value, 0 means successful
 */
static int pixcir_i2c_rxdata(char *rxdata, int length)
{
        int ret;
        struct i2c_msg msgs[] = {
                {
                        .addr   = g_pixcir_ts->client->addr,
                        .flags  = 0,
                        .len    = 1,
                        .buf    = rxdata,
                },
                {
                        .addr   = g_pixcir_ts->client->addr,
                        .flags  = I2C_M_RD,
                        .len    = length,
                        .buf    = rxdata,
                },
        };

        ret = i2c_transfer(g_pixcir_ts->client->adapter, msgs,2);
        if (ret < 0)
                pr_err("%s i2c read error: %d\n", __func__, ret);

        return ret;
}

/* pixcir_i2c_txdata --  write data to i2c
 * @rxdata: write buffer, first input variable
 * @length: length length, second input variable
 * @return: eror value, 0 means successful
 */
static int pixcir_i2c_txdata(char *txdata, int length)
{
		int ret;
		struct i2c_msg msg[] = {
			{
				.addr	= g_pixcir_ts->client->addr,
				.flags	= 0,
				.len		= length,
				.buf		= txdata,
			},
		};

		ret = i2c_transfer(g_pixcir_ts->client->adapter, msg, 1);
		if (ret < 0)
			pr_err("%s i2c write error: %d\n", __func__, ret);

		return ret;
}


/* pixcir_i2c_write_data --  write one byte to i2c
 * @rxdata: the register address of the i2c device , first input variable
 * @length: the data to write , second input variable
 * @return: eror value, 0 means successful
 */
static int pixcir_i2c_write_data(unsigned char addr, unsigned char data)
{
	unsigned char buf[2];
	buf[0]=addr;
	buf[1]=data;
	return pixcir_i2c_txdata(buf, 2);
}

/* pixcir_show_debug -- show debug level
 * @return: len
 */
static ssize_t pixcir_show_debug(struct device* cd,struct device_attribute *attr, char* buf)
{
	ssize_t ret = 0;

	sprintf(buf, "PIXCIR Debug %d\n",debug_level);

	ret = strlen(buf) + 1;

	return ret;
}

/* pixcir_store_debug -- set debug level
 * @return: len
 */
static ssize_t pixcir_store_debug(struct device* cd, struct device_attribute *attr,
		       const char* buf, size_t len)
{
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
	debug_level = on_off;

	printk(KERN_INFO "%s: debug_level=%d\n",__func__, debug_level);

	return len;
}



/* pixcir_set_calibrate --  enable tp chip calibration fucntion
 * @buf: 1 = start calibrate , first input variable
 * @return: len
 */
static ssize_t pixcir_set_calibrate(struct device* cd, struct device_attribute *attr,
		       const char* buf, size_t len)
{
	unsigned long on_off = simple_strtoul(buf, NULL, 10);

	if(on_off==1)
	{
		printk(KERN_INFO "%s: PIXCIR calibrate\n",__func__);
		pixcir_i2c_write_data(PIXCIR_SPECOP_REG , PIXICR_CALIBRATE_MODE);
		msleep(5*1000);
	}

	return len;
}


/* pixcir_show_suspend --  for suspend/resume debug
 *                         show current status
 * params:	no care
 * @return: len
 */
static ssize_t pixcir_show_suspend(struct device* cd,
				     struct device_attribute *attr, char* buf)
{
	ssize_t ret = 0;

	if(g_pixcir_ts->suspend_flag==1)
		sprintf(buf, "Pixcir Suspend\n");
	else
		sprintf(buf, "Pixcir Resume\n");

	ret = strlen(buf) + 1;

	return ret;
}


/* pixcir_store_suspend -- for suspend/resume debug
 *                         set suspend/resume
 * params:	no care
 * @return: len
 */
static ssize_t pixcir_store_suspend(struct device* cd, struct device_attribute *attr,
		       const char* buf, size_t len)
{
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
	g_pixcir_ts->suspend_flag= on_off;

	if(on_off==1)
	{
		printk(KERN_INFO "Pixcir Entry Suspend\n");
		pixcir_ts_suspend(NULL);
	}
	else
	{
		printk(KERN_INFO "Pixcir Entry Resume\n");
		pixcir_ts_resume(NULL);
	}

	return len;
}


/* pixcir_create_sysfs --  create sysfs attribute
 * client:	i2c client
 * @return: len
 */
static int pixcir_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	PIXCIR_DBG("%s", __func__);

	err = device_create_file(dev, &dev_attr_calibrate);
	err = device_create_file(dev, &dev_attr_suspend);
	err = device_create_file(dev, &dev_attr_debug);


	return err;
}

/* pixcir_ts_sleep --  set pixicir into sleep mode
 * @return: none
 */
static void pixcir_ts_sleep(void)
{
	printk(KERN_INFO "==%s==\n", __func__);
    pixcir_i2c_write_data(PIXCIR_PWR_MODE_REG , PIXCIR_PWR_SLEEP_MODE);
}


/* pixcir_ts_suspend --  set pixicr into suspend
 * @return: none
 */
static void pixcir_ts_suspend(struct early_suspend *handler)
{
	printk(KERN_INFO "==%s==, irq=%d\n", __func__,g_pixcir_ts->pixcir_irq);
   	pixcir_ts_sleep();
	disable_irq_nosync(g_pixcir_ts->pixcir_irq);
}

/* pixcir_ts_resume --  set pixicr to resume
 * @return: none
 */
static void pixcir_ts_resume(struct early_suspend *handler)
{
	printk(KERN_INFO "==%s==, irq=%d\n", __func__,g_pixcir_ts->pixcir_irq);
	pixcir_reset(g_pixcir_ts->platform_data->reset_gpio_number);
	msleep(200);
	pixcir_tx_config();
	enable_irq(g_pixcir_ts->pixcir_irq);
}


#ifdef TOUCH_VIRTUAL_KEYS
#define PIXCIR_KEY_HOME	102
#define PIXCIR_KEY_MENU	139
#define PIXCIR_KEY_BACK	158
#define PIXCIR_KEY_SEARCH  217

static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
	 __stringify(EV_KEY) ":" __stringify(PIXCIR_KEY_HOME) ":30:859:100:55"
	 ":" __stringify(EV_KEY) ":" __stringify(PIXCIR_KEY_MENU) ":201:859:100:55"
	 ":" __stringify(EV_KEY) ":" __stringify(PIXCIR_KEY_BACK) ":320:859:100:55"
	 ":" __stringify(EV_KEY) ":" __stringify(PIXCIR_KEY_SEARCH) ":460:859:100:55"
	 "\n");
}

static struct kobj_attribute virtual_keys_attr = {
    .attr = {
        .name = "virtualkeys.pixcir_ts",
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


/* pixcir_ts_virtual_keys_init --  register virutal keys to system
 * @return: none
 */
static void pixcir_ts_virtual_keys_init(void)
{
    int ret;
    struct kobject *properties_kobj;

    PIXCIR_DBG("%s",__func__);

    properties_kobj = kobject_create_and_add("board_properties", NULL);
    if (properties_kobj)
        ret = sysfs_create_group(properties_kobj,
                     &properties_attr_group);
    if (!properties_kobj || ret)
        pr_err("failed to create board_properties\n");
}


#endif


/* pxicir_ts_pininit --  gpio request
 * irq_pin:	irq gpio number
 * rst_pin: reset gpio number
 * @return: none
 */
static void pxicir_ts_pininit(int irq_pin, int rst_pin)
{
	printk(KERN_INFO "%s [irq=%d];[rst=%d]\n",__func__,irq_pin,rst_pin);
	gpio_request(irq_pin, TS_IRQ_PIN);
	gpio_request(rst_pin, TS_RESET_PIN);
}

/* pixcir_ts_pwron --  power on pixcir chip
 * reg_vdd:	regulator
 * @return: none
 */
static void pixcir_ts_pwron(struct regulator *reg_vdd)
{
	printk(KERN_INFO "%s\n",__func__);
	regulator_set_voltage(reg_vdd, 2800000, 2900000);
	regulator_enable(reg_vdd);
	msleep(20);
}

/* attb_read_val --  read the interrupt pin level
 * gpio_pin: pin number
 * @return: 1 or 0
 */
static int attb_read_val(int gpio_pin)
{
	return gpio_get_value(gpio_pin);
}

/* pixcir_reset --  set pixcir reset
 * reset_pin: pin number
 * @return: none
 */
static void pixcir_reset(int reset_pin)
{
	printk(KERN_INFO "%s\n",__func__);
	gpio_direction_output(reset_pin, 0);
	msleep(3);
	gpio_set_value(reset_pin, 1);
	msleep(10);
	gpio_set_value(reset_pin,0);
	msleep(10);
}

/* pixcir_tx_config --  initialize the pixcir register
 * @return: error code
 */
static int pixcir_tx_config(void)
{
	int error;
	unsigned char buf;

	error=pixcir_i2c_write_data(PIXCIR_INT_MODE_REG, PIXCIR_INT_MODE);
	buf =  PIXCIR_INT_MODE_REG;
	error=pixcir_i2c_rxdata(&buf, 1);
	PIXCIR_DBG("%s: buf=0x%x",__func__, buf);
	return error;
}


static void return_i2c_dev(struct i2c_dev *i2c_dev)
{
	spin_lock(&i2c_dev_list_lock);
	list_del(&i2c_dev->list);
	spin_unlock(&i2c_dev_list_lock);
	kfree(i2c_dev);
}

static struct i2c_dev *i2c_dev_get_by_minor(unsigned index)
{
	struct i2c_dev *i2c_dev;
	i2c_dev = NULL;

	spin_lock(&i2c_dev_list_lock);
	list_for_each_entry(i2c_dev, &i2c_dev_list, list)
	{
		if (i2c_dev->adap->nr == index)
			goto found;
	}
	i2c_dev = NULL;
	found: spin_unlock(&i2c_dev_list_lock);
	return i2c_dev;
}

static struct i2c_dev *get_free_i2c_dev(struct i2c_adapter *adap)
{
	struct i2c_dev *i2c_dev;

	if (adap->nr >= I2C_MINORS) {
		printk(KERN_ERR "%s: i2c-dev: Out of device minors\n",__func__);
		return ERR_PTR(-ENODEV);
	}

	i2c_dev = kzalloc(sizeof(*i2c_dev), GFP_KERNEL);
	if (!i2c_dev)
		return ERR_PTR(-ENOMEM);

	i2c_dev->adap = adap;

	spin_lock(&i2c_dev_list_lock);
	list_add_tail(&i2c_dev->list, &i2c_dev_list);
	spin_unlock(&i2c_dev_list_lock);
	return i2c_dev;
}
/*********************************Bee-0928-bottom**************************************/


/* pixcir_ts_poscheck -- check it finger on TP and it's position,
 *                       report event to input system
 * pixcir_ts_struct: pixicir data struct
 * @return: none
 */
static void pixcir_ts_poscheck(struct pixcir_ts_struct *data)
{
	struct pixcir_ts_struct *tsdata = data;
	int x,y;
	unsigned char *p;
	unsigned char touch, button, pix_id,slot_id;
	unsigned char rdbuf[27]={0};
	int slotid[5];
	int i,temp;
	rdbuf[0]=0;
	pixcir_i2c_rxdata(rdbuf, 27);

	touch = rdbuf[0]&0x07;
	button = rdbuf[1];

	PIXCIR_DBG("%s: touch=%d,button=%d",__func__, touch,button);

	p=&rdbuf[2];
	for (i=0; i<touch; i++)	{
		pix_id = (*(p+4));
		slot_id = ((pix_id & 7)<<1) | ((pix_id & 8)>>3);
		slotid[i]=slot_id;
		point_slot[slot_id].active = 1;
		point_slot[slot_id].finger_id = pix_id;
		point_slot[slot_id].posx = (*(p+1)<<8)+(*(p))-X_OFFSET;
		point_slot[slot_id].posy = (*(p+3)<<8)+(*(p+2))-Y_OFFSET;
		p+=5;
		if(distance[i]==0) {
		  point_slot_back[i].posx=point_slot[slot_id].posx;
		  point_slot_back[i].posy=point_slot[slot_id].posy;
		}
	}

	if(touch) {
		for(i=0;i<touch;i++){
			x=(point_slot_back[i].posx-point_slot[slotid[i]].posx);
			x=(x>0)?x:-x;
			y=(point_slot_back[i].posy-point_slot[slotid[i]].posy);
			y=(y>0)?y:-y;
			temp=x+y;
			PIXCIR_DBG("pix distance=%2d,%2d,%2d\n",distance[i],temp,touch_flage[i]);
			if(distance[i]){
				if((temp<DIS_THRESHOLD)&&(touch_flage[i]==0)){
					point_slot[slotid[i]].posx=point_slot_back[i].posx;
					point_slot[slotid[i]].posy=point_slot_back[i].posy;
					PIXCIR_DBG("pix report back\n");
				}
				else
					touch_flage[i]=1;
			} else {
				distance[i]=1;
			}
		}
	}
	else {
		memset(distance,0,sizeof(distance));
		memset(touch_flage,0,sizeof(touch_flage));
	}

	if(touch) {
		input_report_key(tsdata->input, BTN_TOUCH, 1);
		for (i=0; i<MAX_FINGER_NUM*2; i++) {
			if (point_slot[i].active == 1) {
				if(point_slot[i].posy<0) {
				} else {
					if(point_slot[i].posx<0) {
						point_slot[i].posx = 0;
					} else if (point_slot[i].posx>X_MAX) {
						point_slot[i].posx = X_MAX;
					}
					input_report_abs(tsdata->input, ABS_MT_POSITION_X,  point_slot[i].posx);
					input_report_abs(tsdata->input, ABS_MT_POSITION_Y,  point_slot[i].posy);
					input_mt_sync(tsdata->input);
					PIXCIR_DBG("%s: slot=%d,x%d=%d,y%d=%d",__func__, i, i/2,point_slot[i].posx, i/2, point_slot[i].posy);
				}
			}
		}
	} else {
		PIXCIR_DBG("%s: release",__func__);
		input_report_key(tsdata->input, BTN_TOUCH, 0);
		input_mt_sync(tsdata->input);
	}
	input_sync(tsdata->input);

	for (i=0; i<MAX_FINGER_NUM*2; i++) {
		if (point_slot[i].active == 0) {
			point_slot[i].posx = 0;
			point_slot[i].posy = 0;
		}
		point_slot[i].active = 0;
	}

}

/* pixcir_ts_isr -- disable irq, and schedule
 * irq: irq number
 * dev_id: param when request_irq
 * @return: IRQ status
 */
static irqreturn_t pixcir_ts_isr(int irq, void *dev_id)
{
	struct pixcir_ts_struct *tsdata = (struct pixcir_ts_struct *)dev_id;

	//disable irq
	disable_irq_nosync(irq);

	if (!work_pending(&tsdata->pen_event_work)) {
		queue_work(tsdata->ts_workqueue, &tsdata->pen_event_work);
	}

	return IRQ_HANDLED;

}


/* pixcir_ts_irq_work -- read tp register and report event

 * @return: none
 */
static void pixcir_ts_irq_work(struct work_struct *work)
{
	struct pixcir_ts_struct *tsdata = g_pixcir_ts;
	pixcir_ts_poscheck(tsdata);
    enable_irq(tsdata->client->irq);
}

#ifdef CONFIG_PM_SLEEP
static int pixcir_i2c_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char wrbuf[2] = { 0 };
	int ret;

	wrbuf[0] = 0x33;
	wrbuf[1] = 0x03;	//enter into freeze mode;
	/**************************************************************
	wrbuf[1]:	0x00: Active mode
			0x01: Sleep mode
			0xA4: Sleep mode automatically switch
			0x03: Freeze mode
	More details see application note 710 power manangement section
	****************************************************************/
	ret = i2c_master_send(client, wrbuf, 2);
	if(ret!=2) {
		dev_err(&client->dev,
			"%s: i2c_master_send failed(), ret=%d\n",
			__func__, ret);
	}

	if (device_may_wakeup(&client->dev))
		enable_irq_wake(client->irq);

	return 0;
}

static int pixcir_i2c_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	printk(KERN_INFO "%s\n",__func__);
	pixcir_reset(g_pixcir_ts->platform_data->reset_gpio_number);
	if (device_may_wakeup(&client->dev))
		disable_irq_wake(client->irq);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(pixcir_dev_pm_ops,
			 pixcir_i2c_ts_suspend, pixcir_i2c_ts_resume);


/* pixcir_i2c_ts_probe -- pixcir probe function
 * 						  initalize PIXCIR hardware configuration
 *                        register input system, and request irq
 * @return: error code; 0: successful
 */
static int __devinit pixcir_i2c_ts_probe(struct i2c_client *client,
					 const struct i2c_device_id *id)
{
	struct pixcir_ts_platform_data *pdata = client->dev.platform_data;
	struct pixcir_ts_struct *tsdata;
	struct input_dev *input;
	struct device *dev;
	struct i2c_dev *i2c_dev;
	int i, error;

	printk(KERN_INFO "%s: probe\n",__func__);

	for(i=0; i<MAX_FINGER_NUM*2; i++) {
		point_slot[i].active = 0;
	}

	tsdata = kzalloc(sizeof(*tsdata), GFP_KERNEL);
	input = input_allocate_device();
	if (!tsdata || !input) {
		dev_err(&client->dev, "Failed to allocate driver data!\n");
		error = -ENOMEM;
		goto err_free_mem;
	}
	g_pixcir_ts = tsdata;

	PIXCIR_DBG("%s: irq_pin=%d; reset_pin=%d", \
		__func__, pdata->irq_gpio_number,pdata->reset_gpio_number);

	tsdata->platform_data = pdata;

	//pin init
	pxicir_ts_pininit(pdata->irq_gpio_number,pdata->reset_gpio_number);

	//get regulator
#if defined(CONFIG_ARCH_SC8825)
	tsdata->reg_vdd = regulator_get(&client->dev, pdata->vdd_name);
#else
	tsdata->reg_vdd = regulator_get(&client->dev, REGU_NAME_TP);
#endif

	//enable VDD
	pixcir_ts_pwron(tsdata->reg_vdd);

	//reset TP chip
	pixcir_reset(pdata->reset_gpio_number);
	msleep(300);

	//get irq number
	client->irq = gpio_to_irq(pdata->irq_gpio_number);
	tsdata->pixcir_irq = client->irq;
	PIXCIR_DBG("%s: irq=%d",__func__, client->irq);

	//register virtual keys
#ifdef TOUCH_VIRTUAL_KEYS
	pixcir_ts_virtual_keys_init();
#endif

	tsdata->client = client;
	tsdata->input = input;

	input->name = client->name;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &client->dev;

	__set_bit(EV_KEY, input->evbit);
	__set_bit(EV_ABS, input->evbit);
	__set_bit(EV_SYN, input->evbit);
	__set_bit(BTN_TOUCH, input->keybit);
	__set_bit(ABS_MT_TOUCH_MAJOR, input->absbit);
	__set_bit(ABS_MT_POSITION_X, input->absbit);
	__set_bit(ABS_MT_POSITION_Y, input->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, input->absbit);

	__set_bit(KEY_MENU,  input->keybit);
	__set_bit(KEY_BACK,  input->keybit);
	__set_bit(KEY_HOME,  input->keybit);
	__set_bit(KEY_SEARCH,  input->keybit);

	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, X_MAX, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, Y_MAX, 0, 0);
	input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
	input_set_drvdata(input, tsdata);

	INIT_WORK(&tsdata->pen_event_work, pixcir_ts_irq_work);
	tsdata->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));

	error = request_irq(client->irq, pixcir_ts_isr, IRQF_TRIGGER_FALLING, client->name, tsdata);
	if (error) {
		printk(KERN_ERR "%s:Unable to request touchscreen IRQ.\n",__func__);
		goto err_free_mem;
	}

	disable_irq_nosync(client->irq);

	error = input_register_device(input);
	if (error)
		goto err_free_irq;
	i2c_set_clientdata(client, tsdata);
	device_init_wakeup(&client->dev, 1);
	/*********************************Bee-0928-TOP****************************************/
	i2c_dev = get_free_i2c_dev(client->adapter);
	if (IS_ERR(i2c_dev)) {
		error = PTR_ERR(i2c_dev);
		return error;
	}
	dev = device_create(i2c_dev_class, &client->adapter->dev, MKDEV(I2C_MAJOR,
			client->adapter->nr), NULL, "pixcir_i2c_ts%d", 0);
	if (IS_ERR(dev)) {
		error = PTR_ERR(dev);
		return error;
	}
	/*********************************Bee-0928-BOTTOM****************************************/
	tsdata->pixcir_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	tsdata->pixcir_early_suspend.suspend = pixcir_ts_suspend;
	tsdata->pixcir_early_suspend.resume	= pixcir_ts_resume;
	register_early_suspend(&tsdata->pixcir_early_suspend);

	if((error=pixcir_tx_config())<0) {
		printk(KERN_ERR "%s: I2C error\n",__func__);
		enable_irq(client->irq);
		goto err_i2c;
	}
	pixcir_create_sysfs(client);

	printk(KERN_INFO "%s:insmod successfully!\n",__func__);

	enable_irq(client->irq);
	return 0;

	printk(KERN_ERR "%s:insmod Fail!\n",__func__);

err_i2c:
	unregister_early_suspend(&tsdata->pixcir_early_suspend);
err_free_irq:
	free_irq(client->irq, tsdata);
err_free_mem:
	input_free_device(input);
	kfree(tsdata);
	return error;
}


/* pixcir_i2c_ts_remove -- remove pixcir from device list
 * @return: error code; 0: successful
 */
static int __devexit pixcir_i2c_ts_remove(struct i2c_client *client)
{
	int error;
	struct i2c_dev *i2c_dev;
	struct pixcir_ts_struct *tsdata = i2c_get_clientdata(client);

	device_init_wakeup(&client->dev, 0);

	free_irq(client->irq, tsdata);

	/*********************************Bee-0928-TOP****************************************/
	i2c_dev = get_free_i2c_dev(client->adapter);
	if (IS_ERR(i2c_dev)) {
		error = PTR_ERR(i2c_dev);
		return error;
	}

	return_i2c_dev(i2c_dev);
	device_destroy(i2c_dev_class, MKDEV(I2C_MAJOR, client->adapter->nr));
	/*********************************Bee-0928-BOTTOM****************************************/
	unregister_early_suspend(&tsdata->pixcir_early_suspend);
	input_unregister_device(tsdata->input);
	kfree(tsdata);

	return 0;
}

/*************************************Bee-0928****************************************/
/*                        	     pixcir_open                                     */
/*************************************Bee-0928****************************************/
static int pixcir_open(struct inode *inode, struct file *file)
{
	int subminor;
	struct i2c_client *client;
	struct i2c_adapter *adapter;
	struct i2c_dev *i2c_dev;
	int ret = 0;
	PIXCIR_DBG("enter pixcir_open function\n");

	subminor = iminor(inode);

	i2c_dev = i2c_dev_get_by_minor(subminor);
	if (!i2c_dev) {
		printk(KERN_ERR "error i2c_dev\n");
		return -ENODEV;
	}

	adapter = i2c_get_adapter(i2c_dev->adap->nr);
	if (!adapter) {
		return -ENODEV;
	}

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client) {
		i2c_put_adapter(adapter);
		ret = -ENOMEM;
	}

	snprintf(client->name, I2C_NAME_SIZE, "pixcir_i2c_ts%d", adapter->nr);
	client->driver = &pixcir_i2c_ts_driver;
	client->adapter = adapter;

	file->private_data = client;

	return 0;
}

/*************************************Bee-0928****************************************/
/*                        	     pixcir_ioctl                                    */
/*************************************Bee-0928****************************************/
static long pixcir_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *) file->private_data;

	PIXCIR_DBG("pixcir_ioctl(),cmd = %d,arg = %ld\n", cmd, arg);


	switch (cmd)
	{
	case CALIBRATION_FLAG:	//CALIBRATION_FLAG = 1
		client->addr = SLAVE_ADDR;
		status_reg = CALIBRATION_FLAG;
		break;

	case BOOTLOADER:	//BOOTLOADER = 7
		client->addr = BOOTLOADER_ADDR;
		status_reg = BOOTLOADER;

		pixcir_reset(g_pixcir_ts->platform_data->reset_gpio_number);
		mdelay(5);
		break;

	case RESET_TP:		//RESET_TP = 9
		pixcir_reset(g_pixcir_ts->platform_data->reset_gpio_number);
		break;

	case ENABLE_IRQ:	//ENABLE_IRQ = 10
		status_reg = 0;
		enable_irq(g_pixcir_ts->client->irq);
		break;

	case DISABLE_IRQ:	//DISABLE_IRQ = 11
		disable_irq_nosync(g_pixcir_ts->client->irq);
		break;

	case BOOTLOADER_STU:	//BOOTLOADER_STU = 12
		client->addr = BOOTLOADER_ADDR;
		status_reg = BOOTLOADER_STU;

		pixcir_reset(g_pixcir_ts->platform_data->reset_gpio_number);
		mdelay(5);

	case ATTB_VALUE:	//ATTB_VALUE = 13
		client->addr = SLAVE_ADDR;
		status_reg = ATTB_VALUE;
		break;

	default:
		client->addr = SLAVE_ADDR;
		status_reg = 0;
		break;
	}
	return 0;
}

/***********************************Bee-0928****************************************/
/*                        	  pixcir_read                                      */
/***********************************Bee-0928****************************************/
static ssize_t pixcir_read (struct file *file, char __user *buf, size_t count,loff_t *offset)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	unsigned char *tmp, bootloader_stu[4], attb_value[1];
	int ret = 0;

	switch(status_reg)
	{
	case BOOTLOADER_STU:
		i2c_master_recv(client, bootloader_stu, sizeof(bootloader_stu));
		if (ret!=sizeof(bootloader_stu)) {
			dev_err(&client->dev,
				"%s: BOOTLOADER_STU: i2c_master_recv() failed, ret=%d\n",
				__func__, ret);
			return -EFAULT;
		}

		ret = copy_to_user(buf, bootloader_stu, sizeof(bootloader_stu));
		if(ret)	{
			dev_err(&client->dev,
				"%s: BOOTLOADER_STU: copy_to_user() failed.\n",	__func__);
			return -EFAULT;
		}else {
			ret = 4;
		}
		break;

	case ATTB_VALUE:
		attb_value[0] = attb_read_val(g_pixcir_ts->platform_data->irq_gpio_number);
		if(copy_to_user(buf, attb_value, sizeof(attb_value))) {
			dev_err(&client->dev,
				"%s: ATTB_VALUE: copy_to_user() failed.\n", __func__);
			return -EFAULT;
		}else {
			ret = 1;
		}
		break;

	default:
		tmp = kmalloc(count,GFP_KERNEL);
		if (tmp==NULL)
			return -ENOMEM;

		ret = i2c_master_recv(client, tmp, count);
		if (ret != count) {
			dev_err(&client->dev,
				"%s: default: i2c_master_recv() failed, ret=%d\n",
				__func__, ret);
			return -EFAULT;
		}

		if(copy_to_user(buf, tmp, count)) {
			dev_err(&client->dev,
				"%s: default: copy_to_user() failed.\n", __func__);
			kfree(tmp);
			return -EFAULT;
		}
		kfree(tmp);
		break;
	}
	return ret;
}

/***********************************Bee-0928****************************************/
/*                        	  pixcir_write                                     */
/***********************************Bee-0928****************************************/
static ssize_t pixcir_write(struct file *file,const char __user *buf,size_t count, loff_t *ppos)
{
	struct i2c_client *client;
	unsigned char *tmp, bootload_data[143];
	int ret=0, i=0;

	client = file->private_data;

	switch(status_reg)
	{
	case CALIBRATION_FLAG:	//CALIBRATION_FLAG=1
		tmp = kmalloc(count,GFP_KERNEL);
		if (tmp==NULL)
			return -ENOMEM;

		if (copy_from_user(tmp,buf,count)) {
			dev_err(&client->dev,
				"%s: CALIBRATION_FLAG: copy_from_user() failed.\n", __func__);
			kfree(tmp);
			return -EFAULT;
		}

		ret = i2c_master_send(client,tmp,count);
		if (ret!=count ) {
			dev_err(&client->dev,
				"%s: CALIBRATION: i2c_master_send() failed, ret=%d\n",
				__func__, ret);
			kfree(tmp);
			return -EFAULT;
		}

		while(!attb_read_val(g_pixcir_ts->platform_data->irq_gpio_number)) {
			msleep(100);
			i++;
			if(i>99)
				break;  //10s no high aatb break
		}	//waiting to finish the calibration.(pixcir application_note_710_v3 p43)

		kfree(tmp);
		break;

	case BOOTLOADER:
		memset(bootload_data, 0, sizeof(bootload_data));

		if (copy_from_user(bootload_data, buf, count)) {
			dev_err(&client->dev,
				"%s: BOOTLOADER: copy_from_user() failed.\n", __func__);
			return -EFAULT;
		}

		ret = i2c_master_send(client, bootload_data, count);
		if(ret!=count) {
			dev_err(&client->dev,
				"%s: BOOTLOADER: i2c_master_send() failed, ret = %d\n",
				__func__, ret);
			return -EFAULT;
		}
		break;

	default:
		tmp = kmalloc(count,GFP_KERNEL);
		if (tmp==NULL)
			return -ENOMEM;

		if (copy_from_user(tmp,buf,count)) {
			dev_err(&client->dev,
				"%s: default: copy_from_user() failed.\n", __func__);
			kfree(tmp);
			return -EFAULT;
		}

		ret = i2c_master_send(client,tmp,count);
		if (ret!=count ) {
			dev_err(&client->dev,
				"%s: default: i2c_master_send() failed, ret=%d\n",
				__func__, ret);
			kfree(tmp);
			return -EFAULT;
		}
		kfree(tmp);
		break;
	}
	return ret;
}

/***********************************Bee-0928****************************************/
/*                        	  pixcir_release                                   */
/***********************************Bee-0928****************************************/
static int pixcir_release(struct inode *inode, struct file *file)
{
	struct i2c_client *client = file->private_data;

	PIXCIR_DBG("enter pixcir_release funtion\n");

	i2c_put_adapter(client->adapter);
	kfree(client);
	file->private_data = NULL;

	return 0;
}

/*********************************Bee-0928-TOP****************************************/
static const struct file_operations pixcir_i2c_ts_fops =
{	.owner		= THIS_MODULE,
	.read		= pixcir_read,
	.write		= pixcir_write,
	.open		= pixcir_open,
	.unlocked_ioctl = pixcir_ioctl,
	.release	= pixcir_release,
};
/*********************************Bee-0928-BOTTOM****************************************/


static const struct i2c_device_id pixcir_i2c_ts_id[] = {
	{ PIXICR_DEVICE_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pixcir_i2c_ts_id);

static struct i2c_driver pixcir_i2c_ts_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "pixcir_i2c_ts_v3.2.0A",
	},
	.probe		= pixcir_i2c_ts_probe,
	.remove		= __devexit_p(pixcir_i2c_ts_remove),
	.id_table	= pixcir_i2c_ts_id,
};

static int __init pixcir_i2c_ts_init(void)
{
	int ret;
	PIXCIR_DBG("%s",__func__);
	/*********************************Bee-0928-TOP****************************************/
	ret = register_chrdev(I2C_MAJOR,"pixcir_i2c_ts",&pixcir_i2c_ts_fops);
	if (ret) {
		printk(KERN_ERR "%s:register chrdev failed\n",__func__);
		return ret;
	}

	i2c_dev_class = class_create(THIS_MODULE, "pixcir_i2c_dev");
	if (IS_ERR(i2c_dev_class)) {
		ret = PTR_ERR(i2c_dev_class);
		class_destroy(i2c_dev_class);
	}
	/********************************Bee-0928-BOTTOM******************************************/
	return i2c_add_driver(&pixcir_i2c_ts_driver);
}

static void __exit pixcir_i2c_ts_exit(void)
{
	i2c_del_driver(&pixcir_i2c_ts_driver);
	/********************************Bee-0928-TOP******************************************/
	class_destroy(i2c_dev_class);
	unregister_chrdev(I2C_MAJOR,"pixcir_i2c_ts");
	/********************************Bee-0928-BOTTOM******************************************/
}

module_init(pixcir_i2c_ts_init);
module_exit(pixcir_i2c_ts_exit);

MODULE_AUTHOR("Yunlong wang <yunlong.wang@spreadtrum.com>");
MODULE_DESCRIPTION("Pixcir I2C Touchscreen Driver");
MODULE_LICENSE("GPL");

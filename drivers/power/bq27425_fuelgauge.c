
/*
 *  bq27425_fuelgauge.c
 *  fuelgauge driver  
 *
 *
 *  Copyright (C) 2011, Samsung Electronics
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */ 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/init.h>

#include <linux/power/bq27425_fuelgauge.h>
#include "bq27425_firmware.h"

#define BQ27425_DRIVER_NAME	"bq27425_fuelgauge"
#define	BQ27425_FIRMWARE_NAME	"bq27425_firmware"

#if defined(CONFIG_BQ27425_SOC_COMPENSATION_FOR_DISCHARGING)
static int old_soc = -1;
#endif 

#if defined(CONFIG_SPA)
#include <linux/power/spa.h>
static void (*spa_external_event)(int, int) = NULL;
#endif

static struct i2c_client *bq27425_client = NULL;
static struct i2c_client *bq27425_firmware_client = NULL;

static int bq27425_i2c_write(int length, int reg, unsigned char *values)
{
	struct i2c_client *client = bq27425_client;
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, length, values);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}


static int bq27425_i2c_read(int length, int reg, unsigned char *values)
{
	struct i2c_client *client = bq27425_client;
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, length, values);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}

static int bq27425_firmware_i2c_write(int length, int reg , unsigned char *values)
{
	struct i2c_client *client = bq27425_firmware_client;
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, length, values);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}

static int bq27425_firmware_i2c_read(int length, int reg , unsigned char *values)
{
	struct i2c_client *client = bq27425_firmware_client;
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, length, values);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}

static int bq27425_enter_RomMode(void)
{
	unsigned char data[]={0x0, 0xf};
	int ret = 0;

	ret = bq27425_i2c_write(2, BQ27425_CNTL, data);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_write failed %d\n", __func__, ret);
		return ret; 
	}
	return 0;
}

static int bq27425_firmware_writing_verify(void)
{
	int ret = 0; 
	unsigned char data = 0;

	ret = bq27425_firmware_i2c_read(1, BQ27425_WRITE_VERIFY, &data);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_firmware_i2c_read failed %d\n", __func__, ret);
		return ret;
	}

	if(data != 0)
	{
		printk("%s last writing had been failed\n", __func__);
		return data; 
	}

	return 0;
}

static int bq27425_firmware_control(unsigned char data1[], int data1_len,  unsigned char data2[], int data2_len, int msec, int verify)
{
	int ret = 0;

	ret = bq27425_firmware_i2c_write(data1_len , data1[0], &data1[1]);
	if(ret < 0)
	{
		printk("%s bq27425_firmware_i2c_write failed %d\n", __func__, ret);
		return ret; 
	}

	ret = bq27425_firmware_i2c_write(data2_len, data2[0], &data2[1]);
	if(ret < 0)
	{
		printk("%s bq27425_firmware_i2c_write failed %d\n", __func__, ret);
		return ret; 
	}
	
	mdelay(msec);

	if(verify)
	{
		ret = bq27425_firmware_writing_verify();
		if(ret)
		{
			printk("%s bq27425_firmware_writing_verify failed %d\n", __func__, ret);
			return ret;
		}	
	}

	return 0;
}

static int bq27425_firmware_write_data(unsigned char *data)
{
	int ret = 0;

	ret = bq27425_firmware_i2c_write(5, data[0], &data[1]);
	if(ret < 0)
	{
		printk("%s bq27425_firmware_i2c_write failed %d\n", __func__, ret);
		return ret; 
	}

	ret = bq27425_firmware_i2c_write(2, data[6], &data[7]);
	if(ret < 0)
	{
		printk("%s bq27425_firmware_i2c_write failed %d\n", __func__, ret);
		return ret; 
	}
	
	mdelay(data[9]);

	ret = bq27425_firmware_writing_verify();
	if(ret)
	{
		printk("%s bq27425_firmware_writing_verify failed %d\n", __func__, ret);
		return ret;
	}
	
	return 0;
}

static int bq27425_firmware_update(void)
{
	int firmware_size = sizeof(bq27425_firmware);	
	unsigned char start_data1[]={0x0, 0x1F, 0x0, 0x0, 0x0, 0xD2, 0xFF}; 
	unsigned char start_data2[]={0x64, 0xF0, 0x1}; 
	unsigned char end_data1[]={0x0, 0xF};
	unsigned char end_data2[]={0x64, 0xF, 0x0};
	int ret = 0, i = 0;

	printk("%s start\n", __func__);

	ret = bq27425_enter_RomMode();
	if(ret < 0)
	{
		printk("%s bq27425_enter_RomMode failed %d\n", __func__, ret);
		goto bq27425_firmware_update_failed;	
	}	

	mdelay(1);

	ret = bq27425_firmware_control(start_data1,sizeof(start_data1)-1, start_data2, sizeof(start_data2)-1, 1, 1);
	if(ret)
	{
		printk("%s start command writing failed %d\n", __func__, ret);
		goto bq27425_firmware_update_failed;
	}

	for(i=0;i<firmware_size;i+=10)
	{
		ret = bq27425_firmware_write_data(bq27425_firmware+i);
		if(ret)
		{
			printk("%s bq27425_firmware_write_data failed %d\n", __func__, ret);
			goto bq27425_firmware_update_failed; 
		}
	}	

	ret = bq27425_firmware_control(end_data1, sizeof(end_data1)-1, end_data2, sizeof(end_data2)-1, 4000, 0);
	if(ret)
	{
		printk("%s end command writing failed %d\n", __func__, ret);
		goto bq27425_firmware_update_failed;
	}

	printk("%s end\n", __func__);
	return 0;

bq27425_firmware_update_failed:
	local_irq_enable();
	printk("%s firmware update failed %d\n", __func__, ret);

	return ret;
}

static int bq27425_read_flash_data_in_firmwaremode(int length, unsigned char reg, int offset, unsigned char* data)
{
	int ret = 0;
	unsigned char block_num = offset/32;
	unsigned char flash_data[32] = {0, };

	ret = bq27425_i2c_write(1, BQ27425_DATA_FLASH_CLASS, &reg);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_write failed %d\n", __func__, ret);
		return ret; 
	}

	ret = bq27425_i2c_write(1, BQ27425_DATA_FLASH_BLOCK, &block_num);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_write failed %d\n", __func__, ret);
		return ret; 
	}

	mdelay(1);
	
	ret = bq27425_i2c_read(32, BQ27425_DATA_FLASH_DEFAULT_OFFSET, flash_data);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_read failed %d\n", __func__, ret);
		return ret; 
	}
	memcpy(data, flash_data + offset, length);

	return ret;
}

static int bq27425_read_vcell(void)
{
	unsigned char data[2] = {0,};
	int ret = 0, vbat = 0;

	ret = bq27425_i2c_read(2, BQ27425_VOLT, data);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_read failed %d\n", __func__, ret);
		return ret; 
	}
	vbat = data[0] | (data[1] <<8); 

	return vbat;	
} 

#if defined(CONFIG_BQ27425_READ_VF)
static int bq27425_read_flags(void)
{
	unsigned char data[2] = {0,};
	int ret = 0, flags = 0;

	ret = bq27425_i2c_read(2, BQ27425_FLAGS, data);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_read failed %d\n", __func__, ret);
		return ret; 
	}
	flags = data[0] | (data[1] <<8); 

	return flags;	
}
#endif

static int bq27425_read_soc(void)
{
	unsigned char data[2]={0,};
	int ret = 0, soc = 0;
#if defined(CONFIG_BQ27425_SOC_COMPENSATION_FOR_DISCHARGING)
	struct power_supply *psy = NULL;
	union power_supply_propval psy_data;
#endif
	
	ret = bq27425_i2c_read(2, BQ27425_SOC, data);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_read failed %d\n", __func__, ret);
		return ret; 
	}

	soc = data[0] | (data[1] << 8); 

#if defined(CONFIG_BQ27425_SOC_COMPENSATION_FOR_DISCHARGING)
	if(old_soc == -1)
		old_soc = soc;

	/* prevent abnormal increasing and abnormal decreasing of soc value during discharging */
	if(soc <= BQ27425_SOC_COMPENSATION_THRESHOLD_FOR_DISCHARGING)
	{
		psy = power_supply_get_by_name("battery");	
		if(psy == NULL)
		{
			printk("%s fail to get power supply from battery\n", __func__);
			goto fail_soc_compensation_for_discharging;
		}
		
		if(psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &psy_data))
		{
			printk("%s fail to get property from battery\n", __func__);
			goto fail_soc_compensation_for_discharging;
		}

		if(psy_data.intval != POWER_SUPPLY_STATUS_DISCHARGING)
		{
			goto fail_soc_compensation_for_discharging;
		}

		if(old_soc <= soc)
		{
			/* keep soc as like old soc if new soc value is higher than old soc during discharging.
			   Do nothing */
		}
		else
		{
			/* decrease only 1% of soc level if new soc level is lower than old soc
			   to prevent more than 2% decreasing of soc level */
			old_soc--;
		}
		soc = old_soc;
	}
fail_soc_compensation_for_discharging:
	/* update old soc */
	old_soc = soc;
#endif

	return soc;  
}

static int bq27425_read_opconfig(void)
{
	unsigned char data[2]={0,};
	int ret = 0, opconfig = 0;
	
	ret = bq27425_read_flash_data_in_firmwaremode(2, BQ27425_OP_CONFIG, BQ27425_OP_CONFIG_OFFSET, data);
	if(ret < 0)
	{
		printk("%s bq27425_read_flash_data_in_firmwaremode failed %d\n", __func__, ret);
		return ret; 
	}

	opconfig = (data[0] << 8) | data[1]; 

	return opconfig;  
}

static int bq27425_read_firmware_version(void)
{
	unsigned char data[2]={0,};
	int ret = 0;
	
	ret = bq27425_read_flash_data_in_firmwaremode(1, BQ27425_FIRMWARE_NUMBER, BQ27425_FIRMWARE_NUMBER_OFFSET, data);
	if(ret < 0)
	{
		printk("%s bq27425_read_flash_data_in_firmwaremode failed %d\n", __func__, ret);
		return ret; 
	}

	return (int)data[0];  
}

static int bq27425_write_firmware_version(void)
{
	int ret = 0;
	unsigned char block_num = 0;
	unsigned char reg = BQ27425_FIRMWARE_NUMBER;
	unsigned char checksum = 0;
	unsigned char enable_config_update[2] = {0x13, 0x0};
	unsigned char firmware_version = BQ27425_FIRMWARE_VERSION;

	ret = bq27425_i2c_write(2, BQ27425_CNTL, enable_config_update);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_write BQ27425_CNTL failed %d\n", __func__, ret);
		return ret; 
	}

	ret = bq27425_i2c_write(1, BQ27425_DATA_FLASH_CLASS, &reg);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_write BQ27425_DATA_FLASH_CLASS failed %d\n", __func__, ret);
		return ret; 
	}

	ret = bq27425_i2c_write(1, BQ27425_DATA_FLASH_BLOCK, &block_num);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_write BQ27425_DATA_FLASH_BLOCK failed %d\n", __func__, ret);
		return ret; 
	}

	mdelay(1);
	printk("%s write firmware version 0x%x\n", __func__, firmware_version);
	ret = bq27425_i2c_write(1, BQ27425_DATA_FLASH_DEFAULT_OFFSET, &firmware_version);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_write BQ27425_DATA_FLASH_DEFAULT_OFFSET failed %d\n", __func__, ret);
		return ret; 
	}

	checksum = 255 - firmware_version;
	ret = bq27425_i2c_write(1, BQ27425_BLOCK_DATA_CHECKSUM, &checksum);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_write BQ27425_BLOCK_DATA_CHECKSUM failed %d\n", __func__, ret);
		return ret; 
	}

	printk("%s success write firmware version %d\n", __func__, firmware_version);	

	return ret;
}

#if defined(CONFIG_SPA)
static int read_voltage(int *vbat)
{
	int data = bq27425_read_vcell();

	if(data < 0 || data >= BQ27425_MAX_VOLTAGE)
	{
		*vbat = 0;
		return -1; 
	}
	
	*vbat = data;
	return 0;
}

static int read_soc(int *soc)
{
	int data = bq27425_read_soc();
	
	if(data < 0)
		*soc = 0;
	else if(data >= 100)
		*soc = 100;
	else
		*soc = data;

	return 0;
}

static int bq27425_reset(void)
{
	unsigned char data[]={0x41, 0x0};
	int ret = 0;

	printk("%s \n", __func__);
	ret = bq27425_i2c_write(2, BQ27425_CNTL, data);
	if(ret < 0)
	{
		printk("%s bq27425_i2c_write failed %d\n", __func__, ret);
		return ret; 
	}
#if defined(CONFIG_BQ27425_SOC_COMPENSATION_FOR_DISCHARGING)
	old_soc = -1; /* reset old_soc */ 
#endif 
	msleep(BQ27425_RESET_DELAY);
	return 0;
}

#if defined(CONFIG_BQ27425_READ_VF)
static int read_VF(unsigned int *VF)
{
	int try_count=3; 
	int data=0;
	do{
		data = bq27425_read_flags();
		if(data < 0)
		{
			*VF = BQ27425_WITH_BATTERY;
			return data;
		}
	

		if(data!=0)
		{
			if(data & BQ27425_FLAGS_BAT_DET)
			{
				*VF = BQ27425_WITH_BATTERY; 
				return 0;
			}
			else
			{
				printk("%s flags : 0x%x\n", __func__, data);
			}
		}
		else
		{
			*VF = BQ27425_WITH_BATTERY;
			printk("%s flags : 0x%x\n", __func__, data);
			return -1;
		}

		try_count--;
	}while(try_count!=0);

	*VF = BQ27425_NO_BATTERY;
	return 0;
}
#endif
#endif

static irqreturn_t bq27425_lowbat_irq(int irq, void *dev_id)
{
	printk("%s\n", __func__);
	return IRQ_HANDLED;	
}

static int __devinit bq27425_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	printk("%s\n", __func__);
	if(strcmp(client->name, BQ27425_FIRMWARE_NAME)==0)
	{
		printk("%s %s\n", __func__, client->name);
		bq27425_firmware_client = client;
		return 0;
	}

	/*First check the functionality supported by the host*/
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
		printk("%s functionality check failed 1 \n", __func__);
		return -EIO;
	}
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WRITE_I2C_BLOCK)) {
		printk("%s functionality check failed 2 \n", __func__);
		return -EIO;
	}

	if(bq27425_firmware_client==NULL)
	{
		printk("%s bq27425_firmware_client is NULL\n", __func__);
		ret = -ENXIO;
		goto err_firmware_client_null;
	}

	bq27425_client = client; 

	/* first, check the presence of the bq27425 by reading OPCONFIG */ 
	ret = bq27425_read_opconfig(); 
	if(ret == -ENXIO)
	{
		printk("%s no such device or address\n", __func__);
		goto err_no_such_device_or_address;
	}

	if(ret != BQ27425_SAMSUNG_OPCONFIG)
	{
		printk("%s please check OPCONFIG 0x%x\n", __func__, ret);
		//goto err_check_opconfig;
	}

	ret = bq27425_read_firmware_version();
	if(ret < 0)
	{
		printk("%s read_firmware_version failed %d\n", __func__, ret);
		goto err_read_firmware_version;
	}

	printk("%s firmware_version in flash : 0x%x binary : 0x%x\n", __func__, ret, BQ27425_FIRMWARE_VERSION);
#if 0 //temp
	if((ret != BQ27425_FIRMWARE_VERSION) && (is_production_mode_boot()==0))
#else
	if((ret != BQ27425_FIRMWARE_VERSION))
#endif
	{
		local_irq_disable();
		ret = bq27425_firmware_update();
		if(ret)
		{
			printk("%s bq27425_firmware_update filed %d\n", __func__, ret);
			local_irq_enable();
			goto err_firmware_update_failed;
		}
		ret = bq27425_write_firmware_version();
		if(ret < 0)
		{
			printk("%s write_firmware_version failed %d\n", __func__, ret);
			local_irq_enable();
			goto err_write_firmware_version;
		}
		local_irq_enable();
	}

#if defined(CONFIG_SPA)
	spa_external_event = spa_get_external_event_handler();
#endif

	if(client->irq){
		ret = gpio_request(client->irq, "bq27425_lowbat");
		if(ret)
		{
			printk("%s gpio_request failed\n", __func__);
			goto err_gpio_request_bq27425_lowbat;	
		}
		gpio_direction_input(client->irq);
		ret = request_irq(gpio_to_irq(client->irq), bq27425_lowbat_irq, IRQF_TRIGGER_FALLING, "bq27425_lowbat", client);	
		if(ret)
		{
			printk("%s request_irq failed\n", __func__);
			goto err_request_irq_bq27425_lowbat;
		}
	}

#if 0 //temp
#if defined(CONFIG_SPA)
	ret = spa_bat_register_fuelgauge_reset(bq27425_reset);
	if(ret)
	{
		printk("%s fail to register bq27425_reset function\n", __func__);
		goto err_register_bq27425_reset;
	}
	ret = spa_bat_register_read_voltage(read_voltage);
	if (ret)
	{
		printk("%s fail to register read_voltage function\n", __func__);
		goto err_register_read_voltage;
	}
	ret = spa_bat_register_read_soc(read_soc);
	if(ret)
	{
		printk("%s fail to register read_soc function\n", __func__);
		goto err_register_read_soc;
	}
#if defined(CONFIG_BQ27425_READ_VF)
	ret = spa_bat_register_read_VF(read_VF);
	if(ret)
	{
		printk("%s fail to register read_VF function\n", __func__);
		goto err_register_read_VF;
	}
#endif
#endif
#else
	{
		int soc = 0, vbat = 0;
		read_soc(&soc);
		read_voltage(&vbat);
		printk("%s %d %d\n", __func__, soc, vbat);
	}
#endif


	return 0;

#if defined(CONFIG_SPA)
#if defined(CONFIG_BQ27425_READ_VF)
err_register_read_VF:
#endif
	spa_bat_unregister_read_soc(read_soc);
err_register_read_soc:
	spa_bat_unregister_read_voltage(read_voltage);
err_register_read_voltage:
#if 0 //temp
	spa_bat_unregister_fuelgauge_reset(bq27425_reset);
#endif
err_register_bq27425_reset:
#endif
err_request_irq_bq27425_lowbat:
	gpio_free(client->irq);
err_gpio_request_bq27425_lowbat:
#if defined(CONFIG_SPA)
	spa_external_event = NULL; 
#endif
err_write_firmware_version:
err_firmware_update_failed:
err_read_firmware_version:
err_check_opconfig:
err_no_such_device_or_address:
	bq27425_client = NULL;
	bq27425_firmware_client = NULL;
err_firmware_client_null:
	return ret;
}

static int __devexit bq27425_remove(struct i2c_client *client)
{
	return 0;
}

static void bq27425_shutdown(struct i2c_client *client)
{
	return;
}

static int bq27425_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int bq27425_resume(struct i2c_client *client)
{
	return 0;
}

/* Every chip have a unique id */
static const struct i2c_device_id bq27425_id[] = {
	{ BQ27425_FIRMWARE_NAME, 0},
	{ BQ27425_DRIVER_NAME, 0 },
	{ }
};

/* Every chip have a unique id and we need to register this ID using MODULE_DEVICE_TABLE*/
MODULE_DEVICE_TABLE(i2c, bq27425_id);


static struct i2c_driver bq27425_i2c_driver = {
	.driver = {
		.name = "bq27425_fuelgauge",
	},
	.probe		= bq27425_probe,
	.remove		= __devexit_p(bq27425_remove),
	.shutdown	= bq27425_shutdown, 
	.suspend	= bq27425_suspend,
	.resume		= bq27425_resume,
	.id_table	= bq27425_id,
};

static int __init bq27425_init(void)
{
	int ret;
	printk("%s\n", __func__);
	if ((ret = i2c_add_driver(&bq27425_i2c_driver) < 0)) {
		printk(KERN_ERR "%s i2c_add_driver failed.\n", __func__);
	}

	return ret;
}
module_init(bq27425_init);

static void __exit bq27425_exit(void)
{
	i2c_del_driver(&bq27425_i2c_driver);
}
module_exit(bq27425_exit);

MODULE_AUTHOR("YongTaek Lee <ytk.lee@samsung.com>");
MODULE_DESCRIPTION("Linux Driver for fuel gauge");
MODULE_LICENSE("GPL");

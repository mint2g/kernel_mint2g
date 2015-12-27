
#include "MMS100S_ISC_Updater_Customize.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/irq.h>

#include <asm/gpio.h>
#include <asm/io.h>


const unsigned char mfs_i2c_slave_addr = 0x48;
uint8_t mfs_slave_addr;

int MFS_I2C_set_slave_addr(unsigned char _slave_addr)
{
	mfs_slave_addr = _slave_addr << 1;
	return MFS_TRUE;
}

mfs_bool_t MFS_I2C_read_with_addr(unsigned char *_read_buf,
		unsigned char _addr, int _length)
{
	return MFS_TRUE;
}

mfs_bool_t MFS_I2C_write(const unsigned char *_write_buf, int _length)
{
	return MFS_TRUE;
}

mfs_bool_t MFS_I2C_read(unsigned char *_read_buf, int _length)
{
	return MFS_TRUE;
}

void MFS_ms_delay(int msec)
{
	msleep(msec);
}

void MFS_reboot(void)
{
	MCSDL_VDD_SET_LOW();

	MCSDL_GPIO_SDA_SET_HIGH();
	MCSDL_GPIO_SDA_SET_OUTPUT(1);

	MCSDL_GPIO_SCL_SET_HIGH();
	MCSDL_GPIO_SCL_SET_OUTPUT(1);

	MCSDL_RESETB_SET_LOW();
	MCSDL_RESETB_SET_OUTPUT(1);

	MFS_ms_delay(25);

	MCSDL_VDD_SET_HIGH();

	MCSDL_RESETB_SET_HIGH();
	MCSDL_RESETB_SET_INPUT();
	MCSDL_GPIO_SCL_SET_INPUT();
	MCSDL_GPIO_SDA_SET_INPUT();

	MFS_ms_delay(25);
}

void MFS_TSP_reboot(void)
{
#if defined(CONFIG_MACH_KYLETD) || defined(CONFIG_MACH_VASTOI)
	MFS_ms_delay(50);
	ts_power_control(0);
	MFS_ms_delay(500);
	ts_power_control(1);
	MFS_ms_delay(300);
#else
	MFS_ms_delay(50);
	gpio_direction_output(TSP_PWR_LDO_GPIO, 0);
	MFS_ms_delay(500);
	gpio_direction_output(TSP_PWR_LDO_GPIO, 1);
	MFS_ms_delay(300);
#endif
}

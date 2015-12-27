
#ifndef __MMS100S_ISC_Updater_CUSTOMIZE_H__
#define __MMS100S_ISC_Updater_CUSTOMIZE_H__


#include "MMS100S_ISC_Updater.h"
//#define GPIO_TOUCH_EN    41
//#define TOUCH_EN       41
//#define TSP_PWR_LDO_GPIO 41
#define GPIO_TOUCH_INT        60
#define GPIO_TSP_SCL             18
#define GPIO_TSP_SDA            59
#ifndef GPIO_TOUCH_I2C_SDA
#define GPIO_TOUCH_I2C_SDA GPIO_TSP_SDA
#define GPIO_TOUCH_I2C_SCL GPIO_TSP_SCL
#endif

extern void ts_power_control(int en);

#if defined(CONFIG_MACH_KYLETD) || defined(CONFIG_MACH_VASTOI)
#define MCSDL_VDD_SET_HIGH()	ts_power_control(1);
#define MCSDL_VDD_SET_LOW()	ts_power_control(0);
#else
#define MCSDL_VDD_SET_HIGH()	gpio_set_value(GPIO_TOUCH_EN, 1)
#define MCSDL_VDD_SET_LOW()	gpio_set_value(GPIO_TOUCH_EN, 0)
#endif

#define MCSDL_GPIO_SCL_SET_HIGH()		gpio_set_value(GPIO_TSP_SCL, 1)
#define MCSDL_GPIO_SCL_SET_LOW()		gpio_set_value(GPIO_TSP_SCL, 0)

#define MCSDL_GPIO_SDA_SET_HIGH()		gpio_set_value(GPIO_TSP_SDA, 1)
#define MCSDL_GPIO_SDA_SET_LOW()		gpio_set_value(GPIO_TSP_SDA, 0)

#define MCSDL_GPIO_SCL_SET_OUTPUT(n)	gpio_direction_output(GPIO_TSP_SCL, n)
#define MCSDL_GPIO_SCL_SET_INPUT()		gpio_direction_input(GPIO_TSP_SCL)
#define MCSDL_GPIO_SCL_IS_HIGH() ((gpio_get_value(GPIO_TSP_SCL) > 0) ? 1 : 0)

#define MCSDL_GPIO_SDA_SET_OUTPUT(n)	gpio_direction_output(GPIO_TSP_SDA, n)
#define MCSDL_GPIO_SDA_SET_INPUT()	gpio_direction_input(GPIO_TSP_SDA)

#define MCSDL_GPIO_SDA_IS_HIGH()	((gpio_get_value(GPIO_TSP_SDA) > 0) ? 1 : 0)

#if defined(CONFIG_MACH_KYLETD) || defined(CONFIG_MACH_VASTOI)
#define MCSDL_CE_SET_HIGH()		ts_power_control(1);
#define MCSDL_CE_SET_LOW()		ts_power_control(0);
#else
#define MCSDL_CE_SET_HIGH()		gpio_set_value(GPIO_TOUCH_EN, 1)
#define MCSDL_CE_SET_LOW()		gpio_set_value(GPIO_TOUCH_EN, 0)
#endif
//#define MCSDL_CE_SET_OUTPUT()	gpio_tlmm_config(GPIO_CFG(TOUCH_EN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE)

#define MCSDL_RESETB_SET_HIGH()		gpio_set_value(GPIO_TOUCH_INT, 1)
#define MCSDL_RESETB_SET_LOW()		gpio_set_value(GPIO_TOUCH_INT, 0)
#define MCSDL_RESETB_SET_OUTPUT(n)	gpio_direction_output(GPIO_TOUCH_INT, n)
#define MCSDL_RESETB_SET_INPUT()	gpio_direction_input(GPIO_TOUCH_INT)



extern const unsigned char mfs_i2c_slave_addr;

extern int MFS_I2C_set_slave_addr(unsigned char _slave_addr);
extern void MFS_ms_delay(int msec);
extern void MFS_reboot(void);
extern void MFS_TSP_reboot(void);

#endif

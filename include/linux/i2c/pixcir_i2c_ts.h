
#ifndef __PIXCIR_I2C_TS_H__
#define __PIXCIR_I2C_TS_H__

#include <linux/earlysuspend.h>
#include <mach/board.h>

#define PIXCIR_DEBUG		0

#define	PIXICR_DEVICE_NAME	"pixcir_ts"

#define TS_IRQ_PIN			"ts_irq_pin"
#define TS_RESET_PIN		"ts_rst_pin"
/*debug UUI HVGA effect on the 8810ea project*/
#ifdef CONFIG_TOUCHSCREEN_PIXCIR_HVGA_TEST
#define X_MAX 				320
#define Y_MAX 				480
#else
#define X_MAX 				480
#define Y_MAX 				800
#endif
#define DIS_THRESHOLD		40

#define SLAVE_ADDR			0x5c
#define	BOOTLOADER_ADDR		0x5d

#ifndef I2C_MAJOR
#define I2C_MAJOR 			125
#endif

#define I2C_MINORS 			256

#define	CALIBRATION_FLAG	1
#define	BOOTLOADER			7
#define RESET_TP			9

#define	ENABLE_IRQ			10
#define	DISABLE_IRQ			11
#define	BOOTLOADER_STU		16
#define ATTB_VALUE			17

#define	MAX_FINGER_NUM		5
#define X_OFFSET			30
#define Y_OFFSET			40

#define TOUCH_VIRTUAL_KEYS 	1

/* REG REGISTER */
#define PIXCIR_PWR_MODE_REG		51
#define PIXCIR_INT_MODE_REG		52
#define PIXCIR_INT_WIDTH_REG	53
#define PIXCIR_SPECOP_REG		58

/* REGISTER VALUE */
#define PIXCIR_PWR_SLEEP_MODE	0x03
#define PIXCIR_INT_MODE			0X09
#define PIXICR_CALIBRATE_MODE	0x03


struct i2c_dev{
	struct list_head list;
	struct i2c_adapter *adap;
	struct device *dev;
};

struct pixcir_ts_platform_data {
	int irq_gpio_number;
	int reset_gpio_number;
	const char *vdd_name;
};

struct point_node_t{
	unsigned char 	active ;
	unsigned char	finger_id;
	int	posx;
	int	posy;
};

struct pixcir_ts_struct{
	int pixcir_irq;
	int suspend_flag;
	bool exiting;
	struct i2c_client 		*client;
	struct input_dev 		*input;
	struct regulator		*reg_vdd;
	struct i2c_client 		*this_client;
	struct early_suspend	pixcir_early_suspend;
	struct pixcir_ts_platform_data	*platform_data;
	struct workqueue_struct *ts_workqueue;
	struct work_struct 	pen_event_work;
};


#endif

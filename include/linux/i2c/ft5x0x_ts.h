
#ifndef __PIXCIR_I2C_TS_H__
#define __PIXCIR_I2C_TS_H__

#include <linux/earlysuspend.h>
#include <mach/board.h>

#define PIXCIR_DEBUG		0

#define	FT5X0X_DEVICE_NAME	"ft5x0x_ts"

#define TS_IRQ_PIN			"ts_irq_pin"
#define TS_RESET_PIN		"ts_rst_pin"


#define X_MAX 				320
#define Y_MAX 				480
#define DIS_THRESHOLD		40

//#define SLAVE_ADDR			0x5c
//#define	BOOTLOADER_ADDR		0x5d
#define SLAVE_ADDR			0x70
#define	BOOTLOADER_ADDR		0x71

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

#define FT5x0x_REG_FW_VER                   0xA6

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

struct i2c_dev{
	struct list_head list;
	struct i2c_adapter *adap;
	struct device *dev;
};

struct ft5x0x_ts_platform_data {
	int irq_gpio_number;
	int reset_gpio_number;
};

struct point_node_t{
	unsigned char 	active ;
	unsigned char	finger_id;
	int	posx;
	int	posy;
};
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
struct ft5x0x_ts_struct{
	int ft5x0x_irq;
	int suspend_flag;
	bool exiting;
	struct i2c_client 		*client;
	struct input_dev 		*input;
	struct regulator		*reg_vdd;
	struct i2c_client 		*this_client;
	struct early_suspend	ft5x0x_early_suspend;
	struct ft5x0x_ts_platform_data	*platform_data;
	struct workqueue_struct *ts_workqueue;
	struct work_struct 	pen_event_work;
	struct ts_event		event;
};

#endif

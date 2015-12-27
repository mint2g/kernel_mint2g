#ifndef __AL3006_PLS_H__
#define __AL3006_PLS_H__

#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/earlysuspend.h>

#define PLS_DEBUG 1
#define AL3006_PLS_ADC_LEVEL9

#if PLS_DEBUG
#define PLS_DBG(format, ...)	\
		printk(KERN_INFO "AL3006 " format "\n", ## __VA_ARGS__)
#else
#define PLS_DBG(format, ...)
#endif


#define AL3006_PLS_DEVICE 			"al3006_pls"
#define AL3006_PLS_INPUT_DEV		"proximity"
#define AL3006_PLS_IRQ_PIN			"al3006_irq_pin"
#define AL3006_PLS_ADDRESS			0x1C

#define LTR_IOCTL_MAGIC 			0x1C
#define LTR_IOCTL_GET_PFLAG  		_IOR(LTR_IOCTL_MAGIC, 1, int)
#define LTR_IOCTL_GET_LFLAG  		_IOR(LTR_IOCTL_MAGIC, 2, int)
#define LTR_IOCTL_SET_PFLAG  		_IOW(LTR_IOCTL_MAGIC, 3, int)
#define LTR_IOCTL_SET_LFLAG  		_IOW(LTR_IOCTL_MAGIC, 4, int)
#define LTR_IOCTL_GET_DATA  		_IOW(LTR_IOCTL_MAGIC, 5, unsigned char)


/*REG*/
#define AL3006_PLS_REG_CONFIG		0x00
#define AL3006_PLS_REG_TIME_CTRL	0x01
#define AL3006_PLS_REG_DLS_CTRL		0x02
#define AL3006_PLS_REG_INT_STATUS	0x03
#define AL3006_PLS_REG_DPS_CTRL		0x04
#define AL3006_PLS_REG_DATA			0x05
#define AL3006_PLS_REG_DLS_WIN		0x08

/*CMD*/
#define AL3006_PLS_BOTH_ACTIVE		0x02
#define AL3006_PLS_ALPS_ACTIVE		0x00
#define AL3006_PLS_PXY_ACTIVE		0x01
#define AL3006_PLS_BOTH_DEACTIVE	0x0B
#define	AL3006_PLS_INT_MASK			0x03
#define AL3006_PLS_DPS_INT			0x02
#define AL3006_PLS_DLS_INT			0x01

typedef enum _SENSOR_TYPE{
	AL3006_PLS_ALPS=0,
	AL3006_PLS_PXY,
	AL3006_PLS_BOTH,
}SENSOR_TYPE;

struct al3006_pls_platform_data {
	int irq_gpio_number;
};

typedef  struct _al3006_pls_t{
	int    irq;
	struct input_dev *input;
	struct i2c_client *client;
	struct work_struct	work;
	struct workqueue_struct *ltr_work_queue;
	struct early_suspend ltr_early_suspend;
}al3006_pls_struct;

#endif


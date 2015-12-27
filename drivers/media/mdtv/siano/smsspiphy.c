/****************************************************************

Siano Mobile Silicon, Inc.
MDTV receiver kernel modules.
Copyright (C) 2006-2008, Uri Shkolnik

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

 This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

****************************************************************/

#include <linux/kernel.h>
#include <asm/irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <asm/dma.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include "smsdbg_prn.h"
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <mach/hardware.h>
#include <mach/pinmap.h>
#include <mach/regulator.h>

/* physical layer variables */
/*! global bus data */
struct spiphy_dev_s {
	struct spi_device *spidev;	/*!< ssp port configuration */
	void (*interruptHandler) (void *);
	void *intr_context;
	int rx_buf_len;
	int tx_buf_len;
};

static int cmmb_irq = 0xff;
static struct spiphy_dev_s spiphy_dev;

struct regulator        *reg_vdd1v8 = NULL;
struct regulator        *reg_vdd1v2 = NULL;

#define SPI_PIN_FUNC_MASK  (0x3<<4)
#define SPI_PIN_FUNC_DEF   (0x0<<4)
#define SPI_PIN_FUNC_GPIO  (0x3<<4)

struct spi_pin_desc {
	const char   *name;
	unsigned int pin_func;
	unsigned int reg;
	unsigned int gpio;
};

static struct spi_pin_desc spi_pin_group[] = {
	{"SPI_DI",  SPI_PIN_FUNC_DEF,  REG_PIN_SPI_DI   + CTL_PIN_BASE,  29},
	{"SPI_CLK", SPI_PIN_FUNC_DEF,  REG_PIN_SPI_CLK  + CTL_PIN_BASE,  30},
	{"SPI_DO",  SPI_PIN_FUNC_DEF,  REG_PIN_SPI_DO   + CTL_PIN_BASE,  31},
	{"SPI_CS0", SPI_PIN_FUNC_GPIO, REG_PIN_SPI_CSN0 + CTL_PIN_BASE,  32}
};


static void sprd_restore_spi_pin_cfg(void)
{
	unsigned int reg;
	unsigned int  gpio;
	unsigned int  pin_func;
	unsigned int value;
	unsigned long flags;
	int i = 0;        
	
	int regs_count = sizeof(spi_pin_group)/sizeof(struct spi_pin_desc);

	for (; i < regs_count; i++) {
	    pin_func = spi_pin_group[i].pin_func;
	    gpio = spi_pin_group[i].gpio;
	    
	    if (pin_func == SPI_PIN_FUNC_DEF) {
		 reg = spi_pin_group[i].reg;

		 /* free the gpios that have request */
		 gpio_free(gpio);
		 
		 local_irq_save(flags);
		 
		 /* config pin default spi function */
		 value = ((__raw_readl(reg) & ~SPI_PIN_FUNC_MASK) | SPI_PIN_FUNC_DEF);
		 __raw_writel(value, reg);
		 
		 local_irq_restore(flags);
	    }
	    else {  
		 /* CS should config output */
		 gpio_direction_output(gpio, 1);
	    }
	}

}


static void sprd_set_spi_pin_input(void)
{
	unsigned int reg;
	unsigned int value;
	unsigned int  gpio;
	unsigned int  pin_func;
	const char    *name;
	unsigned long flags;
	int i = 0; 

	int regs_count = sizeof(spi_pin_group)/sizeof(struct spi_pin_desc);

	for (; i < regs_count; i++) {
	    pin_func = spi_pin_group[i].pin_func;
	    gpio = spi_pin_group[i].gpio;
	    name = spi_pin_group[i].name;

	    /* config pin GPIO function */
	    if (pin_func == SPI_PIN_FUNC_DEF) {
		 reg = spi_pin_group[i].reg;

		 local_irq_save(flags);
		 
		 value = ((__raw_readl(reg) & ~SPI_PIN_FUNC_MASK) | SPI_PIN_FUNC_GPIO);
		 __raw_writel(value, reg);
		 
		 local_irq_restore(flags);

		 if (gpio_request(gpio, name)) {
		     printk("smsspi: request gpio %d failed, pin %s\n", gpio, name);
		 }

	    }

	    gpio_direction_input(gpio);
	}

}



//#define SMS_POWER_GPIO   138 
#define SMS_RESET_GPIO    138
#define SMS_IRQ_GPIO      139
#define CMMB_26M_EN_GPIO  93

#define SPI_PACKET_SIZE 256
#define SPI_TMOD_DEMOD  2


/* physical layer variables */
/*! global bus data */

/*!
invert the endianness of a single 32it integer

\param[in]		u: word to invert

\return		the inverted word
*/
static inline u32 invert_bo(u32 u)
{
	return ((u & 0xff) << 24) | ((u & 0xff00) << 8) | ((u & 0xff0000) >> 8)
		| ((u & 0xff000000) >> 24);
}

/*!
invert the endianness of a data buffer

\param[in]		buf: buffer to invert
\param[in]		len: buffer length

\return		the inverted word
*/

static int invert_endianness(char *buf, int len)
{
	int i;
	u32 *ptr = (u32 *) buf;

	len = (len + 3) / 4;
	for (i = 0; i < len; i++, ptr++)
		*ptr = invert_bo(*ptr);

	return 4 * ((len + 3) & (~3));
}


static irqreturn_t spibus_interrupt(int irq, void *context)
{
	if (spiphy_dev.interruptHandler)
		spiphy_dev.interruptHandler(spiphy_dev.intr_context);
	printk("SMS INT\n");
	return IRQ_HANDLED;
}


void smschip_poweron(void)
{
	int ret;

	/* config spi pin groups for spi operation */
	sprd_restore_spi_pin_cfg();

	printk("SMS chip power ON\r\n");

	/* Operate platform LDO to poweron */
	reg_vdd1v8 = regulator_get(NULL, REGU_NAME_CMMBIO);
	reg_vdd1v2 = regulator_get(NULL, REGU_NAME_CMMB);

        /* enable power output 1.8v and 1.2v, from VDDRF1 and VDDSIM3*/
        regulator_set_voltage(reg_vdd1v8, 1700000, 1800000);
        regulator_enable(reg_vdd1v8);

        regulator_set_voltage(reg_vdd1v2, 1100000, 1200000);
        regulator_enable(reg_vdd1v2);

	/* enable CMMB_26M clock */
	if (gpio_request(CMMB_26M_EN_GPIO, "CMMB_26M_EN_GPIO")) {
		printk("gpio_request fail: CMMB_26M_EN_GPIO %d\n", CMMB_26M_EN_GPIO);
	}

	gpio_direction_output(CMMB_26M_EN_GPIO, 1);

	if (gpio_request(SMS_IRQ_GPIO, "SMS_IRQ_GPIO")) {
		printk("gpio_request fail: SMS_IRQ_GPIO %d\n", SMS_IRQ_GPIO);
	}

	/*  init irq */
	cmmb_irq = gpio_to_irq(SMS_IRQ_GPIO);
	ret = request_irq(cmmb_irq, spibus_interrupt, IRQF_TRIGGER_RISING, "SMSSPI", &spiphy_dev);
	if(ret < 0)
	  printk("SMS request_irq failed\r\n");

	msleep(100);

	//reset
	if(gpio_request(SMS_RESET_GPIO, "SMS_RESET_GPIO")) {
		printk("gpio_request SMS_RESET_GPIO failed\r\n");
	}
	
	gpio_direction_output(SMS_RESET_GPIO, 0);
	msleep(100);
	gpio_direction_output(SMS_RESET_GPIO, 1);
	msleep(100);
}

void smschip_poweroff(void)
{
	printk("SMS chip power OFF\r\n");

	free_irq(cmmb_irq, &spiphy_dev);

	gpio_direction_output(SMS_RESET_GPIO, 0);
	gpio_direction_output(CMMB_26M_EN_GPIO, 0);

	gpio_free(SMS_RESET_GPIO);
	gpio_free(SMS_IRQ_GPIO);
	gpio_free(CMMB_26M_EN_GPIO);

	regulator_disable(reg_vdd1v8);
	regulator_disable(reg_vdd1v2);

	regulator_put(reg_vdd1v8);
	regulator_put(reg_vdd1v2);

	/* config all spi pin input */
	sprd_set_spi_pin_input();
}


static int sms_spi_probe(struct spi_device *spidev)
{
	spiphy_dev.spidev = spidev;

	sprd_set_spi_pin_input();

	printk("sms_spi_probe\n");

	return 0;
}


static int sms_spi_remove(struct spi_device *spi_dev)
{
	printk("sms_spi_remove\n");

	return 0;
}


static struct spi_driver sms_spi_driver = {
	.driver = {
		.name = "cmmb-dev",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
	},
	.probe = sms_spi_probe,
	.remove = sms_spi_remove,
};


int smsspi_driver_init(void)
{
	int status = 0;

	/*eRegister SPI driver */
	status = spi_register_driver(&sms_spi_driver);
	if (status < 0)
		printk("Failed to register SMS SPI driver\r\n");

	return status;
}

void smsspi_driver_exit(void)
{
	printk("Unregister SMS SPI driver\r\n");
	/* Unregister SPI driver */
	spi_unregister_driver(&sms_spi_driver);
}


void smsspibus_xfer(void *context, unsigned char *txbuf,
	  unsigned long txbuf_phy_addr, unsigned char *rxbuf,
	  unsigned long rxbuf_phy_addr, int len)
{
	int status;
	
	struct spi_message message;
	struct spi_transfer transfer = {
			.rx_buf	 	= rxbuf,
			.tx_buf	 	= txbuf,
			.len		= len,
	};

	spi_message_init(&message);
	spi_message_add_tail(&transfer, &message);
	status = spi_sync(spiphy_dev.spidev, &message);

	if (status != 0){
		printk("SMS spi_sync error status: %d %d\r\n", status, message.status);
	}

}

void smschipreset(void *context)
{

}

void *smsspiphy_init(void *context, void (*smsspi_interruptHandler) (void *),
	   void *intr_context)
{
	spiphy_dev.interruptHandler = smsspi_interruptHandler;
	spiphy_dev.intr_context = intr_context;

	return &spiphy_dev;
}

int smsspiphy_deinit(void *context)
{
	spiphy_dev.interruptHandler = NULL;
	spiphy_dev.intr_context = NULL;

	return 0;
}



void prepareForFWDnl(void *context)
{
}

void fwDnlComplete(void *context, int App)
{
}

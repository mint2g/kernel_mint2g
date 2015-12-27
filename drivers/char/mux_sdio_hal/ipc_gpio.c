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
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/bitops.h>
#include <mach/board.h>
#include "ipc_gpio.h"
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>

#define SDHCI_RESEND_SUPPORT

#define DRIVER_NAME "sdhci_hal"
#define KERN_DEBUG KERN_NOTICE

#define DBG(f, x...) 	pr_debug(DRIVER_NAME " [%s()]: " f, __func__,## x)

#define MAX_MIPC_WAIT_TIMEOUT     2000

static unsigned int cp_to_ap_rdy_irq;
static unsigned int cp_to_ap_rts_irq;

static struct wake_lock   s_sdhci_wake_lock;
static wait_queue_head_t   s_gpio_cp_ready_wq;

extern void process_modem_packet(unsigned long data);

int cp2ap_rdy(void)
{
	DBG("SDIO :cp2ap_rdy=%d\r\n",gpio_get_value(GPIO_CP_TO_AP_RDY));
	return gpio_get_value(GPIO_CP_TO_AP_RDY);
}

void  set_ap_status(void)
{
	gpio_set_value(GPIO_AP_STATUS, 1);
	DBG("SDIO :Set_ap_status=1\n");

}

void clear_ap_status(void)
{
	gpio_set_value(GPIO_AP_STATUS, 0);
	DBG("SDIO :Clear_ap_status=0\n");
}

int cp2ap_rts(void)
{
	DBG("SDIO :cp2ap_rts=%d\r\n",gpio_get_value(GPIO_CP_TO_AP_RTS));
	return gpio_get_value(GPIO_CP_TO_AP_RTS);
}

void ap2cp_rts_enable(void)
{
	DBG("SDIO ap2cp_rts_enable AP_TO_CP_RTS is 1\r\n");
	gpio_set_value(GPIO_AP_TO_CP_RTS, 1);
}

void ap2cp_rts_disable(void)
{
	DBG("SDIO ap2cp_rts_disable AP_TO_CP_RTS is 0\r\n");
	gpio_set_value(GPIO_AP_TO_CP_RTS, 0);
}

#define MAX_SLEEP_DELAY_TIME   5000
static struct timer_list s_sdhci_gpio_timer;

u32 s_is_sdhci_sleep = 0;
u32 s_is_sdhci_need_restore = 0;

static void _EnableSleep(unsigned long unused)
{
	printk("[MIPC]EnableSleep!%d\r\n", s_is_sdhci_sleep);
	if(s_is_sdhci_sleep)
	{
		s_is_sdhci_need_restore = 1;
		wake_unlock(&s_sdhci_wake_lock);
	 	s_is_sdhci_sleep = 0;
	}
}

static void _DisableSleep(void)
{
        pr_debug("[MIPC]_DisableSleep!\r\n");
        if(!s_is_sdhci_sleep)
        {
		printk("[MIPC]DisableSleep!%d\r\n", s_is_sdhci_sleep);
		wake_lock(&s_sdhci_wake_lock);
		s_is_sdhci_sleep = 1;
		if(s_is_sdhci_need_restore)
		{
			s_is_sdhci_need_restore = 0;
		}
        }
	mod_timer(&s_sdhci_gpio_timer,  jiffies + msecs_to_jiffies(MAX_SLEEP_DELAY_TIME));
}

extern  u32  wake_up_mipc_rx_thread(u32  need_to_rx_data);

static irqreturn_t cp_to_ap_rts_irq_handle(int irq, void *handle)
{
	if(cp2ap_rts())
	{
		irq_set_irq_type( irq,  IRQF_TRIGGER_LOW);
	}
	else
	{
		irq_set_irq_type( irq,  IRQF_TRIGGER_HIGH);
		if(!wake_up_mipc_rx_thread(!cp2ap_rts()))
		{
			_DisableSleep();
		}
	}

	return IRQ_HANDLED; 
}


static irqreturn_t cp_to_ap_rdy_handle(int irq, void *handle)
{
	DBG("[SDIO_IPC]cp_to_ap_rdy_handle:%d\r\n ", cp2ap_rdy());
	wake_up(&s_gpio_cp_ready_wq);
	return IRQ_HANDLED;
}

u32 sdhci_connect(u32   direction)
{
	_DisableSleep();
	if(SDHCI_TRANSFER_DIRECT_WRITE == direction)
	{
	          clear_ap_status();	/* send */
	}
	else
	{
		set_ap_status();	/* receive */
	}

	ap2cp_rts_enable(); 	/* start */

	if(!wait_event_timeout(s_gpio_cp_ready_wq, cp2ap_rdy(), MAX_MIPC_WAIT_TIMEOUT/HZ))
	{
		printk("[MIPC] SDIO %s Connect Timeout!\r\n", direction ? "Read" : "Write" );
		return	SDHCI_TIMEOUT;
	}
	return  SDHCI_SUCCESS;
}

u32 sdhci_disconnect(u32  status)
{
	DBG("[SDIO_IPC]sdhci_disconnect: %s\r\n", status ? "SDHCI_TRANSFER_ERROR" : "SDHCI_TRANSFER_OK");
	_DisableSleep();
#ifdef SDHCI_RESEND_SUPPORT
	if(SDHCI_TRANSFER_OK  == status)
	{
		set_ap_status();	/* success */
	}
	else
	{
		clear_ap_status();/* fail */
	}

         ap2cp_rts_disable();/* end */

	if(!wait_event_timeout(s_gpio_cp_ready_wq, !cp2ap_rdy(), MAX_MIPC_WAIT_TIMEOUT/HZ))
	{
		printk("[MIPC] SDIO Disconnect Timeout!\r\n");
		return	SDHCI_TIMEOUT;
	}
#else
	 ap2cp_rts_disable();/* end */
#endif

	return   SDHCI_SUCCESS;
}



u32 sdhci_resetconnect(void)
{
	printk("[MIPC]sdhci_resetconnect\r\n");
	clear_ap_status();
	ap2cp_rts_disable();
	return 0;
}


/*****************************************************************************\
 *                                                                           *
 * Driver init/exit                                                          *
 *                                                                           *
\*****************************************************************************/

int sdhci_hal_gpio_init(void)
{
	int ret;

	DBG("sdhci_hal_gpio init \n");

	init_waitqueue_head(&s_gpio_cp_ready_wq);
	wake_lock_init(&s_sdhci_wake_lock, WAKE_LOCK_SUSPEND, "cp_to_ap_rts");

	init_timer(&s_sdhci_gpio_timer);
	s_sdhci_gpio_timer.expires = MAX_SLEEP_DELAY_TIME;
	s_sdhci_gpio_timer.data = 0;
	s_sdhci_gpio_timer.function = _EnableSleep;


	//config ap_to_cp_rts
	ret = gpio_request(GPIO_AP_TO_CP_RTS, "ap_to_cp_rts");
	if (ret) {
		DBG("Cannot request GPIO %d\r\n", GPIO_AP_TO_CP_RTS);
		gpio_free(GPIO_AP_TO_CP_RTS);
		return ret;
	}
	gpio_direction_output(GPIO_AP_TO_CP_RTS, 0); //2012.1.10
	gpio_export(GPIO_AP_TO_CP_RTS,  1);

	//config ap_out_rdy
	ret = gpio_request(GPIO_AP_STATUS, "ap_out_rdy");
	if (ret) {
		DBG("Cannot request GPIO %d\r\n", GPIO_AP_STATUS);
		gpio_free(GPIO_AP_STATUS);
		return ret;
	}
	gpio_direction_output(GPIO_AP_STATUS, 1);
	gpio_export(GPIO_AP_STATUS,  1);

	//config cp_out_rdy
	ret = gpio_request(GPIO_CP_TO_AP_RDY, "cp_out_rdy");
	if (ret) {
		DBG("Cannot request GPIO %d\r\n",GPIO_CP_TO_AP_RDY);
		gpio_free(GPIO_CP_TO_AP_RDY);
		return ret;
	}
	gpio_direction_input(GPIO_CP_TO_AP_RDY);
	gpio_export(GPIO_CP_TO_AP_RDY,  1);

	ret = gpio_request(GPIO_CP_TO_AP_RTS, "cp_out_rts");
	if (ret) {
		DBG("Cannot request GPIO %d\r\n", GPIO_CP_TO_AP_RTS);
		gpio_free(GPIO_CP_TO_AP_RTS);
		return ret;
	}
	gpio_direction_input(GPIO_CP_TO_AP_RTS);
	gpio_export(GPIO_CP_TO_AP_RTS,  1);

	return 0;
}

int sdhci_hal_gpio_irq_init(void)
{
	int ret;

	DBG("sdhci_hal_gpio init \n");

        cp_to_ap_rdy_irq = gpio_to_irq(GPIO_CP_TO_AP_RDY);
	if (cp_to_ap_rdy_irq < 0)
		return -1;
	ret = request_threaded_irq(cp_to_ap_rdy_irq, cp_to_ap_rdy_handle,
		NULL, IRQF_DISABLED|IRQF_TRIGGER_RISING |IRQF_TRIGGER_FALLING, "cp_to_ap_rdy_irq", NULL);
	if (ret) {
		DBG("lee :cannot alloc cp_to_ap_rdy_irq, err %d\r\r\n", ret);
		return ret;
	}

        cp_to_ap_rts_irq = gpio_to_irq(GPIO_CP_TO_AP_RTS);
	if (cp_to_ap_rts_irq < 0)
		return -1;
	ret = request_threaded_irq(cp_to_ap_rts_irq, cp_to_ap_rts_irq_handle,
		NULL, IRQF_DISABLED|IRQF_TRIGGER_LOW |IRQF_NO_SUSPEND, "cp_to_ap_rts_irq", NULL);
	irq_set_irq_type(cp_to_ap_rts_irq,IRQF_TRIGGER_LOW);
	if (ret) {
		DBG("cannot alloc cp_to_ap_rts_irq, err %d\r\r\n", ret);
		return ret;
	}

	return 0;

}

void sdhci_hal_gpio_exit(void)
{
	DBG("sdhci_hal gpio exit\r\n");

	gpio_free(GPIO_AP_TO_CP_RTS);
	gpio_free(GPIO_AP_STATUS);
	gpio_free(GPIO_CP_TO_AP_RDY);
	gpio_free(GPIO_CP_TO_AP_RTS);

	free_irq(cp_to_ap_rdy_irq, NULL);
	free_irq(cp_to_ap_rts_irq, NULL);
}

MODULE_AUTHOR("Bin.Xu<bin.xu@spreadtrum.com>");
MODULE_DESCRIPTION("GPIO driver about mux sdio");
MODULE_LICENSE("GPL");

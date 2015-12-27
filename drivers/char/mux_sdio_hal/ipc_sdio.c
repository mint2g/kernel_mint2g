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
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/list.h>

#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/io.h>
#include "ipc_gpio.h"
#include <mach/modem_interface.h>
#include <linux/sprdmux.h>
#include <linux/mmc/sdio_channel.h>
#include <linux/kfifo.h>
#include "ipc_info.h"
#define  MUX_IPC_READ_DISABLE                    0x01
#define  MUX_IPC_WRITE_DISABLE                  0x02



//#define SDIO_READ_DEBUG
//#define SDIO_LOOPBACK_TEST
//#define SDIO_DEBUG_ENABLE

#define HEADER_TAG 0x7e7f
#define HEADER_TYPE 0xaa55

#define IPC_DRIVER_NAME "IPC_SDIO"

static int tx_packet_count=0;

extern void wait_modem_normal(void);

#define IPC_DBG(f, x...) 	pr_debug(IPC_DRIVER_NAME " [%s()]: " f, __func__,## x)

struct packet_header{
	u16 tag;
	u16 type;
	u32 length;
	u32 frame_num;
	u32 reserved2;
}__packed;

typedef struct  MIPC_TRANSF_FRAME_Tag
{
	u8*  buf_ptr;
	u32  buf_size;
	u32  pos;
	u32  flag;
	struct list_head  link;
}MIPC_TRANSF_FRAME_T;

typedef struct  MIPC_FRAME_LIST_Tag
{
	struct list_head   frame_list_head;
	struct mutex       list_mutex;
	u32    counter;
} MIPC_FRAME_LIST_T;

typedef struct MIPC_TRANSFER_Tag
{
   struct mutex                             transfer_mutex;
   struct list_head                        frame_fifo;
   MIPC_TRANSF_FRAME_T*        cur_frame_ptr;
   u32                                            counter;
}MIPC_TRANSFER_T;

#define MAX_IPC_TX_WAIT_TIMEOUT    10

#define MAX_MIPC_RX_FRAME_SIZE    (64*1024)
#define MAX_MIPC_TX_FRAME_SIZE    (16*1024)

#define MAX_MIPC_RX_CACHE_SIZE   MAX_MIPC_RX_FRAME_SIZE*2

static struct kfifo  s_mipc_rx_cache_kfifo;

u8   s_mipc_rx_buf[MAX_MIPC_RX_FRAME_SIZE];

#define MAX_MIPC_TX_FRAME_NUM    3
MIPC_TRANSF_FRAME_T   s_tx_transfer_frame[MAX_MIPC_TX_FRAME_NUM];


static struct mutex ipc_mutex;
wait_queue_head_t s_mux_read_rts;
u32 s_mux_ipc_event_flags = 0;

static struct task_struct *s_mux_ipc_rx_thread;
static struct task_struct *s_mux_ipc_tx_thread;


MIPC_FRAME_LIST_T  s_mipc_tx_free_frame_list;
MIPC_TRANSFER_T     s_mipc_tx_tansfer;

static u32  s_mux_ipc_enable = 0;

static wait_queue_head_t s_mux_ipc_tx_wq;
static wait_queue_head_t s_mux_ipc_rx_wq;


static DEFINE_MUTEX(sdio_tx_lock);
static ssize_t mux_ipc_xmit_buf(const char *buf, ssize_t len);
static int mux_ipc_sdio_write( const char *buf, size_t  count);
static int mux_ipc_sdio_read(  char *buf, size_t  count);
static u32 _FreeAllTxTransferFrame(void);


#define MUX_IPC_DISABLE    0
#define MUX_IPC_ENABLE    1
void  mux_ipc_enable(u8  is_enable)
{
	if(is_enable)
	{
		kfifo_reset(&s_mipc_rx_cache_kfifo);
		_FreeAllTxTransferFrame();
		sdhci_resetconnect();
		s_mux_ipc_event_flags = 0;
		s_mux_ipc_enable = MUX_IPC_ENABLE;
	}
	else
	{
		s_mux_ipc_enable = MUX_IPC_DISABLE;
	}

	printk("[mipc]mux ipc enable:0x%x, status:0x%x\r\n", is_enable, s_mux_ipc_enable);
 }


static  u32  _is_mux_ipc_enable(void)
 {
     return s_mux_ipc_enable ;
 }

int mux_ipc_sdio_stop(int mode)
{
	if(mode & SPRDMUX_READ)
	{
		s_mux_ipc_event_flags |=  MUX_IPC_READ_DISABLE;
	}

	if(mode & SPRDMUX_WRITE)
	{
		s_mux_ipc_event_flags |=  MUX_IPC_WRITE_DISABLE;
	}

    wake_up_interruptible(&s_mux_read_rts);
	   
    return 0;
}

int mux_ipc_sdio_write(const char *buf, size_t  count)
{
        wait_modem_normal();
        IPC_DBG("[mipc]mux_ipc_sdio_write write len:%d\r\n", count);
	return mux_ipc_xmit_buf(buf, count);
}

int mux_ipc_sdio_read(char *buf, size_t  count)
{
	int ret = 0;

    wait_event_interruptible(s_mux_read_rts, !kfifo_is_empty(&s_mipc_rx_cache_kfifo) || s_mux_ipc_event_flags);

    if(s_mux_ipc_event_flags & MUX_IPC_READ_DISABLE)
    {
    	printk("[mipc] mux ipc  read disable!\r\n");
    	return -1;
    }

    IPC_DBG("[mipc]mux_ipc_sdio_read read len:%d\r\n", count);
    ret = kfifo_out(&s_mipc_rx_cache_kfifo,buf,count);

    ipc_info_mux_read(ret);

    ipc_info_sdio_read_saved_count(kfifo_len(&s_mipc_rx_cache_kfifo));	
	
    return ret;
}

 static void _FrameList_Init(MIPC_FRAME_LIST_T* frame_list_ptr)
 {
       if(!frame_list_ptr)
        {
		panic("_Init_Frame_List: frame_list_ptr=NULL!\r\n");
		return;
	}

       INIT_LIST_HEAD(&frame_list_ptr->frame_list_head);
       mutex_init(&frame_list_ptr->list_mutex);
       frame_list_ptr->counter = 0;
 }

static void _TxFreeFrameList_Init(MIPC_FRAME_LIST_T* frame_list_ptr)
{
	u32 t = 0;
	MIPC_TRANSF_FRAME_T* frame_ptr = NULL;

	 _FrameList_Init(frame_list_ptr);

	for(t = 0; t < MAX_MIPC_TX_FRAME_NUM;  t++)
	{
		frame_ptr = &s_tx_transfer_frame[t];
		frame_ptr->buf_size =  MAX_MIPC_TX_FRAME_SIZE;
		frame_ptr->buf_ptr = (u8*)kmalloc(frame_ptr->buf_size , GFP_KERNEL);
		frame_ptr->pos = 0;
		frame_ptr->flag = 0;
		list_add_tail(&frame_ptr->link, &frame_list_ptr->frame_list_head);
		frame_list_ptr->counter++;
	}

}

static void _TransferInit(MIPC_TRANSFER_T* transfer_ptr)
{
	if(!transfer_ptr)
	{
		panic("_Init_Frame_List: frame_list_ptr=NULL!\r\n");
		return;
	}

	INIT_LIST_HEAD(&transfer_ptr->frame_fifo);
	mutex_init(&transfer_ptr->transfer_mutex);
	transfer_ptr->counter = 0;
	transfer_ptr->cur_frame_ptr = NULL;
}

static void _transfer_frame_init(void)
 {
	if(kfifo_alloc(&s_mipc_rx_cache_kfifo,MAX_MIPC_RX_CACHE_SIZE, GFP_KERNEL))
	{
		printk("_transfer_frame_init: kfifo rx cache no memory!\r\n");
		panic("%s[%d]kfifo rx cache no memory", __FILE__, __LINE__);
	}
	_TxFreeFrameList_Init(&s_mipc_tx_free_frame_list);
	_TransferInit(&s_mipc_tx_tansfer);

 }

 MIPC_TRANSF_FRAME_T* _AllocFrame(MIPC_FRAME_LIST_T*  frame_list_ptr)
 {
	 MIPC_TRANSF_FRAME_T*  frame_ptr = NULL;

	 if(!frame_list_ptr)
	 {
		 panic("%s[%d] frame_list_ptr = NULL!\r\n", __FILE__, __LINE__);
	 }

	 mutex_lock(&frame_list_ptr->list_mutex);/*get  lock */
	  if(!list_empty(&frame_list_ptr->frame_list_head))
	  {
		frame_ptr = list_entry(frame_list_ptr->frame_list_head.next, MIPC_TRANSF_FRAME_T, link);
		list_del(&frame_ptr->link);
		frame_ptr->pos = sizeof(struct packet_header);
		frame_ptr->flag = 1;
		frame_list_ptr->counter--;
	  }
	 mutex_unlock(&frame_list_ptr->list_mutex);/*set  lock */
        pr_debug("_AllocFrame:0x%X\r\n", (u32)frame_ptr);
	 return  frame_ptr;
 }

static void _FreeFrame(MIPC_TRANSF_FRAME_T* frame_ptr, MIPC_FRAME_LIST_T*  frame_list_ptr)
 {
	 if(!frame_list_ptr || !frame_ptr)
	 {
		 panic("%s[%d] frame_list_ptr = NULL!\r\n", __FILE__, __LINE__);
	 }
	pr_debug("_FreeFrame:0x%x\r\n", (u32)frame_ptr);
	mutex_lock(&frame_list_ptr->list_mutex);/*get	lock */
	frame_ptr->pos = 0;
	frame_ptr->flag = 0;
	list_add_tail(&frame_ptr->link, &frame_list_ptr->frame_list_head);
	frame_list_ptr->counter++;
	mutex_unlock(&frame_list_ptr->list_mutex);/*set  lock */
 }

static  void _AddFrameToTxTransferFifo(MIPC_TRANSF_FRAME_T* frame_ptr, MIPC_TRANSFER_T*  transfer_ptr)
 {
	 struct packet_header*	header_ptr = ( struct packet_header* )frame_ptr->buf_ptr;
	 header_ptr->tag = 0x7e7f;
	 header_ptr->type = 0xaa55;
	 header_ptr->length =	 frame_ptr->pos - sizeof(struct packet_header);
	 header_ptr->frame_num = tx_packet_count; //sending_cnt;
	 header_ptr->reserved2 = 0xabcdef00;
	 list_add_tail(&frame_ptr->link,  &transfer_ptr->frame_fifo);
 }

static  u32 _GetFrameFromTxTransfer(MIPC_TRANSF_FRAME_T* * out_frame_ptr)
{
	 MIPC_TRANSF_FRAME_T* frame_ptr = NULL;
	 MIPC_TRANSFER_T*  transfer_ptr =  &s_mipc_tx_tansfer;
	 mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */
	 if(!list_empty(&transfer_ptr->frame_fifo))
	 {
		frame_ptr = list_entry(transfer_ptr->frame_fifo.next, MIPC_TRANSF_FRAME_T, link);
		list_del(&frame_ptr->link);
	 }
	 mutex_unlock(&transfer_ptr->transfer_mutex);/*set  lock */

	 if(!frame_ptr)
	 {
		 return 1;
	 }

	 *out_frame_ptr = frame_ptr;
	 return 0;
 }
 
static  u32  _IsTransferFifoEmpty(MIPC_TRANSFER_T* transfer_ptr)
 {
	u32  ret = 0;
	 mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */
	 ret = list_empty(&transfer_ptr->frame_fifo);
	 mutex_unlock(&transfer_ptr->transfer_mutex);/*set  lock */
	 return ret;
 }

 u32 s_need_wake_up_tx_thread = 0;

static u32 _AddDataToTxTransfer(u8* data_ptr, u32 len)
 {
	 u32 ret = 0;
	 MIPC_TRANSF_FRAME_T* frame_ptr = NULL;
	 MIPC_TRANSF_FRAME_T* new_frame_ptr = NULL;
         MIPC_TRANSFER_T* transfer_ptr = &s_mipc_tx_tansfer;
	 mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */

	 if(0 == transfer_ptr->counter)
	 {
	 	s_need_wake_up_tx_thread = 1;
	 }
	 do
	 {
		 frame_ptr = transfer_ptr->cur_frame_ptr;
		 if(!frame_ptr)
		 {
			 frame_ptr =  _AllocFrame(&s_mipc_tx_free_frame_list);
		 }
		 if(!frame_ptr)
		 {
			// printk("%s[%d]_AddDataToFrame No Empty Frame!\r\n", __FILE__, __LINE__);
			 ret = 0;
			 break;
		 }
		 if(len > (frame_ptr->buf_size - sizeof( struct packet_header)))
		 {
			 printk("%s[%d]_AddDataToFrame	data too len!\r\n", __FILE__, __LINE__);
			 ret = 0;
			 break;
		 }
		 if((len + frame_ptr->pos) > frame_ptr->buf_size)
		 {
			 new_frame_ptr =  _AllocFrame(&s_mipc_tx_free_frame_list);
			 if(!new_frame_ptr)
			 {
				// printk("%s[%d]_AddDataToFrame No Empty Frame : pos:%d, data len:%d\r\n", "ipc_sdio.c", __LINE__, frame_ptr->pos, len);
				 ret = 0;
				 break;
			 }
			 _AddFrameToTxTransferFifo(frame_ptr, transfer_ptr);
			 frame_ptr = new_frame_ptr;
		 }
		 memcpy(&frame_ptr->buf_ptr[frame_ptr->pos], data_ptr, len);
		 frame_ptr->pos  += len;
		 transfer_ptr->counter += len;
		 transfer_ptr->cur_frame_ptr = frame_ptr;
		 ret = len;
	 }while(0);

	 mutex_unlock(&transfer_ptr->transfer_mutex);/*set	lock */

	 return ret;
 }

static u32 _DelDataFromTxTransfer(MIPC_TRANSF_FRAME_T* frame_ptr)
 {
        MIPC_TRANSFER_T* transfer_ptr  = &s_mipc_tx_tansfer;
	mutex_lock(&transfer_ptr->transfer_mutex);
	transfer_ptr->counter -=  frame_ptr->pos - sizeof(struct packet_header);
	mutex_unlock(&transfer_ptr->transfer_mutex);
	_FreeFrame(frame_ptr, &s_mipc_tx_free_frame_list);
	return 0;
 }


static u32 _FreeAllTxTransferFrame(void)
{
	MIPC_TRANSF_FRAME_T*  frame_ptr = NULL;
	while(!_GetFrameFromTxTransfer(&frame_ptr))
	{
		_DelDataFromTxTransfer(frame_ptr);
	}
    return 0;
}


static  u32 _FlushTxTransfer(void)
 {
	u32 ret = 0;
	MIPC_TRANSFER_T* transfer_ptr  = &s_mipc_tx_tansfer;
	mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */
	do
	 {
	 	if((transfer_ptr->counter == 0) || !transfer_ptr->cur_frame_ptr ||
		    (transfer_ptr->cur_frame_ptr->pos == sizeof( struct packet_header)))
	 	{
	 		ret = 1;
	 		break;
	 	}
		 _AddFrameToTxTransferFifo(transfer_ptr->cur_frame_ptr, transfer_ptr);
		 transfer_ptr->cur_frame_ptr = NULL;
	 }while(0);
	 mutex_unlock(&transfer_ptr->transfer_mutex);/*set	lock */
        return  ret;
 }

u32 mux_ipc_GetTxTransferSavedCount(void)
{
    u32 count = 0;
    MIPC_TRANSFER_T* transfer_ptr  = &s_mipc_tx_tansfer;
    mutex_lock(&transfer_ptr->transfer_mutex);
    count = transfer_ptr->counter;
    mutex_unlock(&transfer_ptr->transfer_mutex); 
    return count;
}
static size_t sdio_write_modem_data(const u8 * buf, u32 len)
{
	size_t ret = 0;
	u32  result =  SDHCI_TRANSFER_OK;
	u32 resend_count = 0;
	mutex_lock(&ipc_mutex);/*get  lock */

	do
	{
	        ipc_info_change_status(IPC_TX_CHANNEL, IPC_STATUS_CONNECT_REQ);	
		if(SDHCI_SUCCESS != sdhci_connect(SDHCI_TRANSFER_DIRECT_WRITE))
		{
			ipc_info_error_status(IPC_TX_CHANNEL, IPC_STATUS_CONNECT_TIMEOUT);	
			result = SDHCI_TRANSFER_TIMEOUT; 
			break;
		}
		ipc_info_change_status(IPC_TX_CHANNEL, IPC_STATUS_CONNECTED);	
		ret = sprd_sdio_channel_tx(buf, len);
		if(!ret)
		{
			result =  SDHCI_TRANSFER_OK;
			ret = len;
		}
		else
		{
			ipc_info_error_status(IPC_TX_CHANNEL, IPC_STATUS_CRC_ERROR);
			result =  SDHCI_TRANSFER_ERROR;
			printk("SDIO  WRITE FAIL\n");
			ret = 0;
		}

		ipc_info_change_status(IPC_TX_CHANNEL, IPC_STATUS_DISCONNECT_REQ);
		if(SDHCI_SUCCESS !=  sdhci_disconnect(result))
		{
			ipc_info_error_status(IPC_TX_CHANNEL, IPC_STATUS_DISCONNECT_TIMEOUT);
			result = SDHCI_TRANSFER_TIMEOUT;
			break;
		}
        
		ipc_info_change_status(IPC_TX_CHANNEL, IPC_STATUS_DISCONNECTED);
		if(result)
		{
			resend_count++;
			msleep(2);
		}
	}while(result && (resend_count < 3));

	if(SDHCI_TRANSFER_TIMEOUT == result)
	{
		ret = 0;
		sdhci_resetconnect();
	}
	ipc_info_change_status(IPC_TX_CHANNEL, IPC_STATUS_IDLE);
	mutex_unlock(&ipc_mutex); /* release lock */
	return ret;

}

static int sdio_read_modem_data(u8 *buf, int len)
{
	int ret = 0;
	IPC_DBG(" spi_read_modem_data entery:buf:0x%X, len:%x\r\n", (u32)buf, len);
	ret = sprd_sdio_channel_rx(buf,len);
	return ret;
}

static bool VerifyPacketHeader(struct packet_header *header)
{
	if ( (header->tag != HEADER_TAG) || (header->type != HEADER_TYPE)
		|| (header->length > MAX_MIPC_RX_FRAME_SIZE))
	{
		return false;
	}

	return true;
}

u32  process_modem_packet(unsigned long data)
{
	u32 receve_len = 0;
	int ret = 0;
	int result = SDHCI_TRANSFER_OK;
	int resend_count = 0;
	struct packet_header *packet = NULL;

	IPC_DBG(" process_modem_packet down read_mutex....\r\n");

	mutex_lock(&ipc_mutex);

	do
	{
	    IPC_DBG(" spi_read_modem_data xxxx................\r\n");
		
		ipc_info_change_status(IPC_RX_CHANNEL, IPC_STATUS_CONNECT_REQ);	 
		
		if( SDHCI_SUCCESS !=  sdhci_connect(SDHCI_TRANSFER_DIRECT_READ))
		{
			ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_CONNECT_TIMEOUT);	 
			result = SDHCI_TRANSFER_TIMEOUT;
			break;
		}

		ipc_info_change_status(IPC_RX_CHANNEL, IPC_STATUS_CONNECTED);	 
		ret = sdio_read_modem_data(s_mipc_rx_buf,  MAX_MIPC_RX_FRAME_SIZE);
		if (!ret)
		{
			packet=(struct packet_header *)s_mipc_rx_buf;
			if(VerifyPacketHeader(packet))
			{
				result = SDHCI_TRANSFER_OK;
			}
			else
			{
				ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_PACKET_ERROR);
				result = SDHCI_TRANSFER_ERROR;
				printk("SDIO READ FAIL,Packet Header error tag:0x%X, len:%d, rsev:0x%x\n", packet->tag, packet->length, packet->reserved2);
			}
		}
		else
		{
				ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_CRC_ERROR);
				result = SDHCI_TRANSFER_ERROR;
				printk("SDIO READ FAIL \n");
		}

		ipc_info_change_status(IPC_RX_CHANNEL, IPC_STATUS_DISCONNECT_REQ);	
		if(SDHCI_SUCCESS !=  sdhci_disconnect(result))
		{
			ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_DISCONNECT_TIMEOUT);	
			result = SDHCI_TRANSFER_TIMEOUT;
			break;
		}
		
		ipc_info_change_status(IPC_RX_CHANNEL, IPC_STATUS_DISCONNECTED);	
		
		if(result)
		{
			resend_count++;
			msleep(2);
		}

	}while(result && (resend_count < 3));

	if(SDHCI_TRANSFER_TIMEOUT == result)
	{
		sdhci_resetconnect();
	}

	ipc_info_change_status(IPC_RX_CHANNEL, IPC_STATUS_IDLE);
	mutex_unlock(&ipc_mutex);

	if(!result)
	{
		ipc_info_rate(IPC_RX_CHANNEL, packet->length*1000/MAX_MIPC_RX_FRAME_SIZE);
		ipc_info_sdio_read(packet->length);

			
		while( kfifo_avail(&s_mipc_rx_cache_kfifo) < packet->length)
		{
			//printk("[MIPC] MIPC Rx Cache Full!\r\n");
			ipc_info_mux_read_overflow(1);
			msleep(10);	
		}
		
		IPC_DBG("[mipc]Success receive data len:%d\r\n",  packet->length);
		
		receve_len  = packet->length;
		kfifo_in(&s_mipc_rx_cache_kfifo,&s_mipc_rx_buf[sizeof(struct packet_header )], packet->length);
			
		wake_up_interruptible(&s_mux_read_rts);
	}
	else
	{
		receve_len  = 0;		
		printk("[mipc] receive data fail! result:%d\r\n", result);
	}
	
	return receve_len;
}


static int mux_ipc_tx_thread(void *data)
{
	u32 write_len = 0;
	int rval;
    MIPC_TRANSF_FRAME_T*  frame_ptr = NULL;
	struct sched_param	 param = {.sched_priority = 30};

	IPC_DBG(KERN_INFO "mux_ipc_tx_thread");
	sched_setscheduler(current, SCHED_FIFO, &param);

	sdhci_hal_gpio_init();
        wait_modem_normal();
	rval = sprd_sdio_channel_open();
	printk(KERN_INFO "%s() sdio channel opened %d\n", __func__, rval);
	sdhci_hal_gpio_irq_init();

	while (!kthread_should_stop())
	{
		wait_event(s_mux_ipc_tx_wq,  s_mipc_tx_tansfer.counter);

		if(!s_mipc_tx_tansfer.counter || (s_mipc_tx_tansfer.counter > MAX_MIPC_TX_FRAME_SIZE))
		{
		   printk("count:%d\r\n", s_mipc_tx_tansfer.counter);
		}

		while(s_mipc_tx_tansfer.counter)
		{
			if(_IsTransferFifoEmpty(&s_mipc_tx_tansfer))
			{
				msleep(MAX_IPC_TX_WAIT_TIMEOUT);
				if(_FlushTxTransfer())
				{
					printk("No Data To Send\n");
					break;
				}
			}
			do
			{
				if(_GetFrameFromTxTransfer(&frame_ptr))
				{
					printk("Error: Flush Empty Frame \n");
					break;
				}

   			     //  printk("[mipc] write data len:%d\r\n", frame_ptr->pos);
   			     write_len = sdio_write_modem_data(frame_ptr->buf_ptr, frame_ptr->pos);
				ipc_info_rate(IPC_TX_CHANNEL, frame_ptr->pos*1000/frame_ptr->buf_size);
				ipc_info_sdio_write(frame_ptr->pos - sizeof(struct packet_header));
				_DelDataFromTxTransfer(frame_ptr);
			}while(0);


			if(!_is_mux_ipc_enable())
			{
				printk("[mipc] ipc reset free all tx data!\r\n");
				_FreeAllTxTransferFrame();
				while(!_is_mux_ipc_enable())
				{
					printk("mux ipc tx Thread Wait enable!\r\n");
					msleep(40);  
				}
				printk("mux ipc tx Thread Wait enable Finished!\r\n");
				break;		
			}
		}
	}

	sprd_sdio_channel_close();
	sdhci_hal_gpio_exit();

	return 0;
}



static ssize_t mux_ipc_xmit_buf(const char *buf, ssize_t len)
{
	ssize_t ret = 0;

	mutex_lock(&sdio_tx_lock);
	do
	{
		ret = _AddDataToTxTransfer((u8*)buf, len);
		if(!ret)
		{
			ipc_info_sdio_write_overflow(1);
			msleep(8);
		}
	}while(!ret);

	ipc_info_mux_write(len);

	if(s_need_wake_up_tx_thread)
	{
		s_need_wake_up_tx_thread = 0;
		wake_up(&s_mux_ipc_tx_wq);
	}
	mutex_unlock(&sdio_tx_lock);
	return ret;
}

static int mux_ipc_create_tx_thread(void)
{
	IPC_DBG("mux_ipc_create_tx_thread enter.\n");
	init_waitqueue_head(&s_mux_ipc_tx_wq);
	s_mux_ipc_tx_thread = kthread_create(mux_ipc_tx_thread, NULL, "mipc_tx_thread");

	if (IS_ERR(s_mux_ipc_tx_thread)) {
		IPC_DBG("mux_ipc_tx_thread error!.\n");
		return -1;
	}

	wake_up_process(s_mux_ipc_tx_thread);

	return 0;
}

u32 s_mipc_rx_event_flags = 0;
 u32  wake_up_mipc_rx_thread(u32   even_flag)
{
    u32 status = 0;
	u32 ipc_status = _is_mux_ipc_enable();
	if(ipc_status && even_flag)
	{
	      	s_mipc_rx_event_flags = even_flag;
	  	wake_up(&s_mux_ipc_rx_wq);
        status = 0;
	}
	else
	{
		printk("mux ipc rx invaild wakeup, ipc status:%u, flag:%d\r\n", ipc_status, even_flag);
        status = -1;
	}
	 return  status;
}

static int mux_ipc_rx_thread(void *data)
{
	struct sched_param	 param = {.sched_priority = 20};
	IPC_DBG(KERN_INFO "mux_ipc_rx_thread enter\r\n");
	sched_setscheduler(current, SCHED_FIFO, &param);

	s_mipc_rx_event_flags = 0;
	wait_modem_normal();
	msleep(500);

	IPC_DBG(KERN_INFO "mux_ipc_rx_thread start----\r\n");
	while (!kthread_should_stop())
	{
		wait_event(s_mux_ipc_rx_wq,  s_mipc_rx_event_flags);
		s_mipc_rx_event_flags =  0;
		process_modem_packet(0);
	}

	return 0;
}

static int mux_ipc_create_rx_thread()
{
	IPC_DBG("mux_ipc_rx_create_thread enter.\n");
	init_waitqueue_head(&s_mux_ipc_rx_wq);
	s_mux_ipc_rx_thread = kthread_create(mux_ipc_rx_thread, NULL, "mipc_rx_thread");

	if (IS_ERR(s_mux_ipc_rx_thread)) {
		IPC_DBG("ipc_spi.c:mux_ipc_rx_thread error!.\n");
		return -1;
	}

	wake_up_process(s_mux_ipc_rx_thread);

	return 0;
}


static int modem_sdio_probe(struct platform_device *pdev)
{
	int retval = 0;

	IPC_DBG("modem_spi_probe\n");
	mutex_init(&ipc_mutex);
	mux_ipc_create_tx_thread();
    mux_ipc_create_rx_thread();

	IPC_DBG("modem_sdio_probe retval=%d\n", retval);
	return retval;
}

static int modem_sdio_remove(struct platform_device *pdev)
{
	return 0;
}


static struct platform_driver modem_sdio_driver = {
	.probe = modem_sdio_probe,
	.remove = modem_sdio_remove,
	.driver = {
		   .name = "ipc_sdio",
		   }
};

static struct sprdmux sprd_iomux = {
	.id		= 0,
	.io_read	= mux_ipc_sdio_read,
	.io_write	= mux_ipc_sdio_write,
	.io_stop  =  mux_ipc_sdio_stop,
};

static int __init mux_ipc_sdio_init(void)
{

	int retval;
	IPC_DBG("\n");
	IPC_DBG("mux_ipc_sdio_init\n");
	IPC_DBG("mux_ipc_sdio_init platform_driver_register1\n");

	_transfer_frame_init();
	init_waitqueue_head(&s_mux_read_rts);
        sprdmux_register(&sprd_iomux);
	retval = platform_driver_register(&modem_sdio_driver);
	if (retval) {
		IPC_DBG(KERN_ERR "[sdio]: register modem_sdio_driver error\n");
	}

	IPC_DBG("ret=%d\n", retval);
	return retval;

}

static void __exit mux_ipc_sdio_exit(void)
{
        platform_driver_unregister(&modem_sdio_driver);
}

module_init(mux_ipc_sdio_init);
module_exit(mux_ipc_sdio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bin.Xu<bin.xu@spreadtrum.com>");
MODULE_DESCRIPTION("MUX SDIO Driver");

/***************************************************************
    A globalmem driver as an example of char device drivers
  
    The initial developer of the original code is Baohua Song
    <author@linuxdriver.cn>. All Rights Reserved.
****************************************************************/
#include <linux/types.h>
#include <linux/slab.h>
#include "ipc_info.h"

extern u32 mux_ipc_GetTxTransferSavedCount(void);

IPC_INFO_T   s_ipc_info[IPC_CHANNEL_NUM];

void ipc_info_init(void)
{
    memset(&s_ipc_info[0], 0, sizeof(IPC_INFO_T)*IPC_CHANNEL_NUM);
}


void ipc_info_rate(u32 channel,  u32 value)
{
    if(channel >= IPC_CHANNEL_NUM)
	{
		return;
	}

	s_ipc_info[channel].rate = value;
}

void ipc_info_change_status(u32 channel,  u32 status)
{
	if(channel >= IPC_CHANNEL_NUM)
	{
		return;
	}

	s_ipc_info[channel].status = status;
}


void ipc_info_mux_write(u32 size)
{
    s_ipc_info[IPC_TX_CHANNEL].mux_count += size;
}


void ipc_info_mux_read(u32 size)
{
    s_ipc_info[IPC_RX_CHANNEL].mux_count += size;
}

void ipc_info_mux_read_overflow(u32 size)
{
    s_ipc_info[IPC_RX_CHANNEL].overflow_count += size;
}


void ipc_info_sdio_write(u32 size)
{
    s_ipc_info[IPC_TX_CHANNEL].sdio_count += size;
}


void ipc_info_sdio_write_overflow(u32 size)
{
    s_ipc_info[IPC_TX_CHANNEL].overflow_count += size;
}

void ipc_info_sdio_read(u32 size)
{
    s_ipc_info[IPC_RX_CHANNEL].sdio_count += size;
}


void ipc_info_sdio_read_saved_count(u32 size)
{
    s_ipc_info[IPC_RX_CHANNEL].saved_count = size;
}

void ipc_info_error_status(u32 channel,  u32 status)
{
	if(channel >= IPC_CHANNEL_NUM)
	{
		return;
	}

	s_ipc_info[channel].status = status;

	if (IPC_STATUS_CONNECT_TIMEOUT == status) 
	{
		s_ipc_info[channel].connect_timeout_count++;
	}
	else if (IPC_STATUS_DISCONNECT_TIMEOUT == status)
	{
		s_ipc_info[channel].disconnect_timeout_count++;
	}
	else if (IPC_STATUS_CRC_ERROR == status)
	{
		s_ipc_info[channel].crc_error_count++;
	}
	else if (IPC_STATUS_PACKET_ERROR == status)
	{
		s_ipc_info[channel].packet_error_count++;
	}
}
 

IPC_INFO_T*  ipc_info_getinfo(u32 channel)
{
    if(channel >= IPC_CHANNEL_NUM)
    {
        return NULL;
    }
    s_ipc_info[IPC_TX_CHANNEL].saved_count = mux_ipc_GetTxTransferSavedCount();
    return &s_ipc_info[channel];     
}




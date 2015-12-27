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
 #ifndef _ISP_DRV_KERNEL_H_
 #define _ISP_DRV_KERNEL_H_

 enum isp_clk_sel {
	ISP_CLK_MAX,
};

struct isp_irq_param {
	uint32_t isp_irq_val;
	uint32_t dcam_irq_val;
	uint32_t irq_val;
} ;
struct isp_reg_bits {
	uint32_t reg_addr;
	uint32_t reg_value;
} ;

 struct isp_reg_param {
	uint32_t reg_param;
	uint32_t counts;
} ;

enum {
	ISP_INT_HIST_STORE = (1<<0),
	ISP_INT_STORE = (1<<1),
	ISP_INT_LEN_S_LOAD = (1<<2),
	ISP_INT_HIST_CAL = (1<<3),
	ISP_INT_HIST_RST = (1<<4),
	ISP_INT_FETCH_BUF_FULL = (1<<5),
	ISP_INT_DCAM_FULL = (1<<6),
	ISP_INT_STORE_ERR = (1<<7),
	ISP_INT_SHADOW = (1<<8),
	ISP_INT_PREVIEW_STOP = (1<<9),
	ISP_INT_AWBAE = (1<<10),
	ISP_INT_AF = (1<<11),
	ISP_INT_SENSOR_SOF = (1<<12),
	ISP_INT_SENSOR_EOF =  (1<<13),
	ISP_INT_STOP = (1<<31),
};

#define ISP_IO_MAGIC	'R'
#define ISP_IO_IRQ	_IOR(ISP_IO_MAGIC, 0, struct isp_irq_param)
#define ISP_IO_READ	_IOR(ISP_IO_MAGIC, 1, struct isp_reg_param)
#define ISP_IO_WRITE	_IOW(ISP_IO_MAGIC, 2, struct isp_reg_param)
#define ISP_IO_RST	_IOW(ISP_IO_MAGIC, 3, uint32_t)
#define ISP_IO_SETCLK	_IOW(ISP_IO_MAGIC, 4, enum isp_clk_sel)
#define ISP_IO_STOP	_IOW(ISP_IO_MAGIC, 5, uint32_t)
#define ISP_IO_INT	_IOW(ISP_IO_MAGIC, 6, uint32_t)
#define ISP_IO_DCAM_INT	_IOW(ISP_IO_MAGIC, 7, uint32_t)
#define ISP_IO_LNC_PARAM	_IOW(ISP_IO_MAGIC, 8, uint32_t)
#define ISP_IO_LNC	_IOW(ISP_IO_MAGIC, 9, uint32_t)
#define ISP_IO_ALLOC	_IOW(ISP_IO_MAGIC, 10, uint32_t)

#endif

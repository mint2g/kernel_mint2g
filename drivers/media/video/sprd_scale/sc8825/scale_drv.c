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
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <asm/io.h>
#include "scale_drv.h"
#include "gen_scale_coef.h"
#include "../sprd_dcam/sc8825/dcam_drv_sc8825.h"

//#define SCALE_DRV_DEBUG
#define SCALE_LOWEST_ADDR                              0x800
#define SCALE_ADDR_INVALIDE(addr)                     ((addr) < SCALE_LOWEST_ADDR)
#define SCALE_YUV_ADDR_INVALIDE(y,u,v)                  \
	(SCALE_ADDR_INVALIDE(y) &&                      \
	SCALE_ADDR_INVALIDE(u) &&                       \
	SCALE_ADDR_INVALIDE(v))

#define SC_COEFF_H_TAB_OFFSET                          0x400
#define SC_COEFF_V_TAB_OFFSET                          0x4F0
#define SC_COEFF_BUF_SIZE                              (8 << 10)
#define SC_COEFF_COEF_SIZE                             (1 << 10)
#define SC_COEFF_TMP_SIZE                              (6 << 10)
#define SC_COEFF_H_NUM                                 48
#define SC_COEFF_V_NUM                                 68
#define SCALE_AXI_STOP_TIMEOUT                         100
#define SCALE_PIXEL_ALIGNED                            4
#define SCALE_SLICE_HEIGHT_ALIGNED                     4

#define REG_RD(a)                                      __raw_readl(a)
#define REG_WR(a,v)                                    __raw_writel(v,a)
#define REG_AWR(a,v)                                   	__raw_writel((__raw_readl(a) & v), a)
#define REG_OWR(a,v)                                   __raw_writel((__raw_readl(a) | v), a)
#define REG_XWR(a,v)                                   __raw_writel((__raw_readl(a) ^ v), a)
#define REG_MWR(a,m,v)                                 \
	do {                                           \
		uint32_t _tmp = __raw_readl(a);        \
		_tmp &= ~(m);                          \
		__raw_writel(_tmp | ((m) & (v)), (a)); \
	}while(0)


#define SCALE_CHECK_PARAM_ZERO_POINTER(n)               \
	do {                                           \
		if (0 == (int)(n))              \
			return -SCALE_RTN_PARA_ERR;     \
	} while(0)

#define SCALE_CLEAR(a)                                  \
	do {                                           \
		memset((void *)(a), 0, sizeof(*(a)));  \
	} while(0)

#define SCALE_RTN_IF_ERR                                if(rtn) return rtn

typedef void (*scale_isr)(void);

struct scale_desc {
	struct scale_size          input_size;
	struct scale_rect          input_rect;
	struct scale_size          sc_input_size;
	struct scale_addr          input_addr;
	uint32_t                   input_format;
	struct scale_addr          temp_buf_addr;
	struct scale_addr          temp_buf_addr_vir;
	uint32_t                   temp_buf_src;
	uint32_t                   mem_order[3];
	struct scale_size          output_size;
	struct scale_addr          output_addr;
	uint32_t                   output_format;
	uint32_t                   scale_mode;
	uint32_t                   slice_height;
	uint32_t                   slice_out_height;
	uint32_t                   slice_in_height;
	uint32_t                   is_last_slice;
	scale_isr_func             user_func;
	void                       *user_data;
	uint32_t                   use_local_tmp_buf;
        atomic_t                   start_flag;
};

static uint32_t                    g_scale_irq = 0x12345678;
static struct scale_desc           scale_path;
static struct scale_desc           *g_path = &scale_path;
static uint32_t                    s_wait_flag = 0;
static struct semaphore            scale_done_sema = __SEMAPHORE_INITIALIZER(scale_done_sema, 0);

static DEFINE_SPINLOCK(scale_lock);

static int32_t     _scale_trim(void);
static int32_t     _scale_cfg_scaler(void);
static int32_t     _scale_calc_sc_size(void);
static int32_t     _scale_set_sc_coeff(void);
static void        _scale_reg_trace(void);
static irqreturn_t _scale_isr_root(int irq, void *dev_id);
static int32_t     _scale_alloc_tmp_buf(void);
static int32_t     _scale_free_tmp_buf(void);

int32_t    scale_module_en(void)
{
	int                      ret = 0;

	ret = dcam_get_resizer(0);
	if (ret) {
		printk("scale_module_en, failed to get review path %d \n", ret);
		return ret;
	}
	ret = dcam_module_en();
	REG_OWR(SCALE_BASE,  1 << 2);
	memset(g_path, 0, sizeof(scale_path));
	return ret;
}

int32_t    scale_module_dis(void)
{
	int                      ret = 0;

	REG_MWR(SCALE_BASE,  1 << 2, 0 << 2);
	ret = dcam_module_dis();
	if (ret) {
		printk("scale_module_dis, failed to disable scale module %d \n", ret);
		return ret;
	}
	
	ret = dcam_rel_resizer();
	
	return ret;
}

int32_t    scale_reset(void)
{
	// To do, clear all the review path registers

	return 0;
}

int32_t    scale_set_clk(enum scale_clk_sel clk_sel)
{
	return 0;
}

int32_t    scale_start(void)
{
	enum scale_drv_rtn      rtn = SCALE_RTN_SUCCESS;
	int                     ret = 0;

	SCALE_TRACE("SCALE DRV: scale_start: %d \n", g_path->scale_mode);

	dcam_resize_start();
	if (g_path->output_size.w > SCALE_LINE_BUF_LENGTH &&
		SCALE_MODE_NORMAL == g_path->scale_mode) {
		rtn = SCALE_RTN_SC_ERR;
		SCALE_RTN_IF_ERR;
	}
	rtn = _scale_alloc_tmp_buf();
	if(rtn) {
		printk("SCALE DRV: No mem to alloc tmp buf \n");
		goto exit;
	}

	g_path->slice_in_height  = 0;
	g_path->slice_out_height = 0;
	g_path->is_last_slice    = 0;
	REG_OWR(SCALE_INT_CLR,  SCALE_IRQ_BIT);
	REG_OWR(SCALE_INT_MASK, SCALE_IRQ_BIT);

	rtn = _scale_trim();
	if(rtn) goto exit;
	rtn = _scale_cfg_scaler();
	if(rtn) goto exit;

	ret = request_irq(SCALE_IRQ, 
			_scale_isr_root, 
			IRQF_SHARED, 
			"SCALE", 
			&g_scale_irq);
	if (ret) {
		printk("SCALE DRV: scale_start,error %d \n", ret);
		rtn = SCALE_RTN_MAX;
	}

	if (SCALE_MODE_SLICE == g_path->scale_mode) {
		g_path->slice_in_height += g_path->slice_height;
	}
	REG_OWR(SCALE_BASE,  1 << 2);
	_scale_reg_trace();	

	REG_OWR(SCALE_CFG, 1);
	atomic_inc(&g_path->start_flag);
	return SCALE_RTN_SUCCESS;

exit:
	dcam_resize_end();
	_scale_free_tmp_buf();
	printk("SCALE DRV: ret %d \n", rtn);
	return rtn;
}

int32_t    scale_continue(void)
{
	enum scale_drv_rtn      rtn = SCALE_RTN_SUCCESS;
	uint32_t                slice_h = g_path->slice_height;

	SCALE_TRACE("SCALE DRV: continue %d, %d, %d \n",
		g_path->slice_height, g_path->slice_in_height, g_path->scale_mode);

	if (SCALE_MODE_SLICE == g_path->scale_mode) {
		if (g_path->slice_in_height +  g_path->slice_height >= g_path->input_rect.h) {
			slice_h = g_path->input_rect.h - g_path->slice_in_height;
			g_path->is_last_slice = 1;
			REG_MWR(SCALE_SLICE_VER, 0x3FF, slice_h);
			REG_OWR(SCALE_SLICE_VER, (1 << 12));
			SCALE_TRACE("SCALE DRV: continue, last slice, 0x%x \n", REG_RD(SCALE_SLICE_VER));
		} else {
			g_path->is_last_slice = 0;
			REG_MWR(SCALE_SLICE_VER, (1 << 12), (0 << 12));
		}
		g_path->slice_in_height += g_path->slice_height;
	}

	REG_WR(SCALE_FRM_SWAP_Y, g_path->temp_buf_addr.yaddr);
	REG_WR(SCALE_FRM_SWAP_U, g_path->temp_buf_addr.uaddr);
	REG_WR(SCALE_FRM_LINE,   g_path->temp_buf_addr.vaddr);
	
	_scale_reg_trace();
	REG_OWR(SCALE_CFG, 1);
	atomic_inc(&g_path->start_flag);
	SCALE_TRACE("SCALE DRV: continue %x.\n", REG_RD(SCALE_CFG));

	return rtn;
}

int32_t    scale_stop(void)
{
	enum scale_drv_rtn      rtn = SCALE_RTN_SUCCESS;
	uint32_t                flag;

	spin_lock_irqsave(&scale_lock, flag);
	if (atomic_read(&g_path->start_flag)) {
		s_wait_flag = 1;
		down_interruptible(&scale_done_sema);
	}
	spin_unlock_irqrestore(&scale_lock, flag);

	REG_MWR(SCALE_CFG, 1, 0);
	REG_MWR(SCALE_INT_MASK, SCALE_IRQ_BIT, 0 << 9);
	REG_MWR(SCALE_INT_CLR,  SCALE_IRQ_BIT, 0 << 9);
	free_irq(SCALE_IRQ, &g_scale_irq);

	_scale_free_tmp_buf();

	SCALE_TRACE("SCALE DRV: stop is OK.\n");
	return rtn;
}

int32_t    scale_reg_isr(enum scale_irq_id id, scale_isr_func user_func, void* u_data)
{
	enum scale_drv_rtn      rtn = SCALE_RTN_SUCCESS;
	uint32_t                flag;

	if(id >= SCALE_IRQ_NUMBER) {
		rtn = SCALE_RTN_ISR_ID_ERR;
	} else {
		spin_lock_irqsave(&scale_lock, flag);
		g_path->user_func = user_func;
		g_path->user_data = u_data;
		spin_unlock_irqrestore(&scale_lock, flag);
	}
	return rtn;	

}

int32_t    scale_cfg(enum scale_cfg_id id, void *param)
{
	enum scale_drv_rtn      rtn = SCALE_RTN_SUCCESS;

	switch (id) {

	case SCALE_INPUT_SIZE:
	{
		struct scale_size *size = (struct scale_size*)param;
		uint32_t          reg_val = 0;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		SCALE_TRACE("SCALE DRV: SCALE_INPUT_SIZE {%d %d} \n", size->w, size->h);  
		if (size->w > SCALE_FRAME_WIDTH_MAX ||
		    size->h > SCALE_FRAME_HEIGHT_MAX) {
			rtn = SCALE_RTN_SRC_SIZE_ERR;    
		} else {
			reg_val = size->w | (size->h << 16);
			REG_WR(SCALE_SRC_SIZE, reg_val);
			g_path->input_size.w = size->w;
			g_path->input_size.h = size->h;
		}
		break;
	}

	case SCALE_INPUT_RECT:
	{
		struct scale_rect *rect = (struct scale_rect*)param;
		uint32_t          reg_val = 0;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);            

		SCALE_TRACE("SCALE DRV: SCALE_PATH_INPUT_RECT {%d %d %d %d} \n", 
		         rect->x, 
		         rect->y, 
		         rect->w, 
		         rect->h);  

		if (rect->x > SCALE_FRAME_WIDTH_MAX ||
		    rect->y > SCALE_FRAME_HEIGHT_MAX ||
		    rect->w > SCALE_FRAME_WIDTH_MAX ||
		    rect->h > SCALE_FRAME_HEIGHT_MAX) {
			rtn = SCALE_RTN_TRIM_SIZE_ERR;    
		} else {
			reg_val = rect->x | (rect->y << 16);
			REG_WR(SCALE_TRIM_START, reg_val);
			reg_val = rect->w | (rect->h << 16);
			REG_WR(SCALE_TRIM_SIZE, reg_val);
			memcpy((void*)&g_path->input_rect,
			       (void*)rect,
			       sizeof(struct scale_rect));
		}
		break;
	}

	case SCALE_INPUT_FORMAT:
	{
		enum scale_fmt format = *(enum scale_fmt*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		g_path->input_format = format;
		if (SCALE_YUV422 == format ||
			SCALE_YUV420 == format ||
			SCALE_YUV420_3FRAME == format ||
			SCALE_YUV400 == format) {
			REG_MWR(SCALE_CFG, (3 << 11), g_path->input_format << 11);
			REG_MWR(SCALE_CFG, (1 << 5), 0 << 5);
		} else if (SCALE_RGB565 == format) {
			REG_OWR(SCALE_CFG, (1 << 13));
			REG_OWR(SCALE_CFG, (1 << 5));
		} else if (SCALE_RGB888 == format) {
			REG_MWR(SCALE_CFG, (1 << 13), (0 << 13));
			REG_OWR(SCALE_CFG, (1 << 5));
		} else {
			rtn = SCALE_RTN_IN_FMT_ERR;
			g_path->input_format = SCALE_FTM_MAX;
		}
		break;
		
	}
	
	case SCALE_INPUT_ADDR:
	{
		struct scale_addr *p_addr = (struct scale_addr*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		if (SCALE_YUV_ADDR_INVALIDE(p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
			rtn = SCALE_RTN_ADDR_ERR;   
		} else {
			g_path->input_addr.yaddr = p_addr->yaddr;
			g_path->input_addr.uaddr = p_addr->uaddr;
			g_path->input_addr.vaddr = p_addr->vaddr;
			REG_WR(SCALE_FRM_IN_Y, p_addr->yaddr);
			REG_WR(SCALE_FRM_IN_U, p_addr->uaddr);
			REG_WR(SCALE_FRM_IN_V, p_addr->vaddr);
		}
		break;
	}
	
	case SCALE_INPUT_ENDIAN:
	{
		struct scale_endian_sel *endian = (struct scale_endian_sel*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		if (endian->y_endian >= SCALE_ENDIAN_MAX ||
			endian->uv_endian >= SCALE_ENDIAN_MAX) {
			rtn = SCALE_RTN_ENDIAN_ERR;
		} else {
			REG_MWR(SCALE_ENDIAN_SEL, 3, endian->y_endian);
			REG_MWR(SCALE_ENDIAN_SEL, 3 << 2, endian->uv_endian << 2);
		}
		break;
	}

	case SCALE_OUTPUT_SIZE:
	{
		struct scale_size *size = (struct scale_size*)param;
		uint32_t          reg_val = 0;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		SCALE_TRACE("SCALE DRV: SCALE_OUTPUT_SIZE {%d %d} \n", size->w, size->h);
		if (size->w > SCALE_FRAME_WIDTH_MAX ||
		    size->h > SCALE_FRAME_HEIGHT_MAX) {
			rtn = SCALE_RTN_SRC_SIZE_ERR;
		} else {
			reg_val = size->w | (size->h << 16);
			REG_WR(SCALE_DST_SIZE, reg_val);
			g_path->output_size.w = size->w;
			g_path->output_size.h = size->h;
		}		
		break;
	}

	case SCALE_OUTPUT_FORMAT:
	{
		enum scale_fmt format = *(enum scale_fmt*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		g_path->output_format = format;
		if (SCALE_YUV422 == format) {
			REG_MWR(SCALE_CFG, (3 << 6), 0 << 6);
		} else if (SCALE_YUV420 == format) {
			REG_MWR(SCALE_CFG, (3 << 6), 1 << 6);
		} else if (SCALE_RGB565 == format) {
			REG_MWR(SCALE_CFG, (3 << 6), 2 << 6);
		} else {
			rtn = SCALE_RTN_OUT_FMT_ERR;
			g_path->output_format = SCALE_FTM_MAX;
		}
		break;
	}
	
	case SCALE_OUTPUT_ADDR:
	{
		struct scale_addr *p_addr = (struct scale_addr*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		if (SCALE_YUV_ADDR_INVALIDE(p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
			rtn = SCALE_RTN_ADDR_ERR;   
		} else {
			g_path->output_addr.yaddr = p_addr->yaddr;
			g_path->output_addr.uaddr = p_addr->uaddr;
			REG_WR(SCALE_FRM_OUT_Y, p_addr->yaddr);
			REG_WR(SCALE_FRM_OUT_U, p_addr->uaddr);
		}
		break;
	}

	case SCALE_OUTPUT_ENDIAN:
	{
		struct scale_endian_sel *endian = (struct scale_endian_sel*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		if (endian->y_endian >= SCALE_ENDIAN_MAX ||
			endian->uv_endian >= SCALE_ENDIAN_MAX) {
			rtn = SCALE_RTN_ENDIAN_ERR;
		} else {
			REG_MWR(SCALE_ENDIAN_SEL, (3 << 4), endian->y_endian << 4);
			REG_MWR(SCALE_ENDIAN_SEL, (3 << 6), endian->uv_endian << 6);
		}
		break;
	}

	case SCALE_TEMP_BUFF:
	{
		struct scale_addr *p_addr = (struct scale_addr*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		if (SCALE_YUV_ADDR_INVALIDE(p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
			rtn = SCALE_RTN_ADDR_ERR;   
		} else {
			g_path->temp_buf_src = 1;
			g_path->temp_buf_addr.yaddr = p_addr->yaddr;
			g_path->temp_buf_addr.uaddr = p_addr->uaddr;
			g_path->temp_buf_addr.vaddr = p_addr->vaddr;
			REG_WR(SCALE_FRM_SWAP_Y, p_addr->yaddr);
			REG_WR(SCALE_FRM_SWAP_U, p_addr->uaddr);
			REG_WR(SCALE_FRM_LINE,   p_addr->vaddr);
		}
		break;
	}

	case SCALE_SCALE_MODE:
	{
		enum scle_mode mode = *(enum scle_mode*)param;

		if (mode >= SCALE_MODE_MAX) {
			rtn = SCALE_RTN_MODE_ERR;
		} else {
			g_path->scale_mode = mode;
			if (SCALE_MODE_NORMAL == mode) {
				REG_MWR(SCALE_CFG, (1 << 4), (0 << 4));
			} else {
				REG_OWR(SCALE_CFG, (1 << 4));
			}
		}
		
		break;
	}
	
	case SCALE_SLICE_SCALE_HEIGHT:
	{
		uint32_t height = *(uint32_t*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		if (height > SCALE_FRAME_HEIGHT_MAX || (height % SCALE_SLICE_HEIGHT_ALIGNED)) {
			rtn = SCALE_RTN_PARA_ERR;
		} else {
			g_path->slice_height = height;
			REG_MWR(SCALE_SLICE_VER, 0x3FF, height);
		}
		break;
	}

	case SCALE_START:
	{
		rtn = scale_start();
		break;

	}

	case SCALE_CONTINUE:
	{
		rtn = scale_continue();
		break;
	}
	
	case SCALE_STOP:
	{
		rtn = scale_stop();
		break;
	}

	default:
		rtn = SCALE_RTN_IO_ID_ERR;
		break;
	}

	return -rtn;
}

int32_t    scale_read_registers(uint32_t* reg_buf, uint32_t *buf_len)
{
	uint32_t                *reg_addr = (uint32_t*)SCALE_BASE;
	
	if (NULL == reg_buf || NULL == buf_len || 0 != (*buf_len % 4)) {
		return -1;
	}

	while (buf_len != 0 && (uint32_t)reg_addr < SCALE_REG_END) {
		*reg_buf++ = REG_RD(reg_addr);
		reg_addr++;
		*buf_len -= 4;
	}

	*buf_len = (uint32_t)reg_addr - SCALE_BASE;
	return 0;
}

static int32_t _scale_trim(void)
{
	enum scale_drv_rtn      rtn = SCALE_RTN_SUCCESS;

	if (g_path->input_size.w != g_path->input_rect.w ||
		g_path->input_size.h != g_path->input_rect.h) {
		REG_OWR(SCALE_CFG, 1 << 1);
	} else {
		REG_MWR(SCALE_CFG, 1 << 1, 0 << 1);
	}

	return rtn;
}

static int32_t _scale_cfg_scaler(void)
{
	enum scale_drv_rtn      rtn = SCALE_RTN_SUCCESS;

	rtn = _scale_calc_sc_size();
	SCALE_RTN_IF_ERR;
	
	if (g_path->sc_input_size.w != g_path->output_size.w ||
	    g_path->sc_input_size.h != g_path->output_size.h) {
		REG_MWR(SCALE_CFG, 1 << 3, 0 << 3);
		rtn = _scale_set_sc_coeff();
	} else {
		REG_MWR(SCALE_CFG, 1 << 3, 1 << 3);
	}

	return rtn;
}

static int32_t _scale_calc_sc_size(void)
{
	uint32_t                reg_val = 0;
	enum scale_drv_rtn      rtn = SCALE_RTN_SUCCESS;
	uint32_t                div_factor = 1;
	uint32_t                i;
		
	if (g_path->input_rect.w > (g_path->output_size.w * SCALE_SC_COEFF_MAX * (1 << SCALE_DECI_FAC_MAX)) ||
	    g_path->input_rect.h > (g_path->output_size.h * SCALE_SC_COEFF_MAX * (1 << SCALE_DECI_FAC_MAX)) ||
	    g_path->input_rect.w * SCALE_SC_COEFF_MAX < g_path->output_size.w ||
	    g_path->input_rect.h * SCALE_SC_COEFF_MAX < g_path->output_size.h) {
		SCALE_TRACE("SCALE DRV: Target too small or large \n");
		rtn = SCALE_RTN_SC_ERR;
	} else {
		g_path->sc_input_size.w = g_path->input_rect.w;
		g_path->sc_input_size.h = g_path->input_rect.h;
		if (g_path->input_rect.w > g_path->output_size.w * SCALE_SC_COEFF_MAX ||
			g_path->input_rect.h > g_path->output_size.h * SCALE_SC_COEFF_MAX) {
			for (i = 0; i < SCALE_DECI_FAC_MAX; i++) {
				div_factor = (uint32_t)(SCALE_SC_COEFF_MAX * (1 << (1 + i)));
				if (g_path->input_rect.w < (g_path->output_size.w * div_factor) &&
					g_path->input_rect.h < (g_path->output_size.h * div_factor)) {
					break;
				}
			}
			REG_OWR(SCALE_CFG, 1 << 2);
			REG_MWR(SCALE_CFG, (3 << 9), i << 9);
			g_path->sc_input_size.w = g_path->input_rect.w >> (1 + i);
			g_path->sc_input_size.h = g_path->input_rect.h >> (1 + i);
			if ((g_path->sc_input_size.w & (SCALE_PIXEL_ALIGNED - 1)) ||
				(g_path->sc_input_size.h & (SCALE_PIXEL_ALIGNED - 1))) {
				SCALE_TRACE("SCALE DRV: Unsupported sc aligned w ,h %d %d \n",
					g_path->sc_input_size.w,
					g_path->sc_input_size.h);
				g_path->sc_input_size.w = g_path->sc_input_size.w & ~(SCALE_PIXEL_ALIGNED - 1);
				g_path->sc_input_size.h = g_path->sc_input_size.h & ~(SCALE_PIXEL_ALIGNED - 1);
				g_path->input_rect.w = g_path->sc_input_size.w << (1 + i);
				g_path->input_rect.h = g_path->sc_input_size.h << (1 + i);
				SCALE_TRACE("SCALE DRV: after rearranged w ,h %d %d, sc w h %d %d \n",
					g_path->input_rect.w,
					g_path->input_rect.h,
					g_path->sc_input_size.w,
					g_path->sc_input_size.h);
				reg_val = g_path->input_rect.w | (g_path->input_rect.h << 16);
				REG_OWR(SCALE_CFG, 1 << 1);
				REG_WR(SCALE_TRIM_SIZE, reg_val);
			}
		} 

	}

	return rtn;
}

static int32_t _scale_set_sc_coeff(void)
{
	uint32_t                i = 0;
	uint32_t                h_coeff_addr = SCALE_BASE;
	uint32_t                v_coeff_addr  = SCALE_BASE;
	uint32_t                *tmp_buf = NULL;
	uint32_t                *h_coeff = NULL;
	uint32_t                *v_coeff = NULL;

	h_coeff_addr += SC_COEFF_H_TAB_OFFSET;
	v_coeff_addr += SC_COEFF_V_TAB_OFFSET;

	tmp_buf = (uint32_t *)kmalloc(SC_COEFF_BUF_SIZE, GFP_KERNEL);
	if (NULL == tmp_buf) {
		printk("SCALE DRV: No mem to alloc coeff buffer! \n");
		return SCALE_RTN_NO_MEM;
	}

	h_coeff = tmp_buf;
	v_coeff = tmp_buf + (SC_COEFF_COEF_SIZE/4);

	if (!(GenScaleCoeff((int16_t)g_path->sc_input_size.w, 
	                    (int16_t)g_path->sc_input_size.h,
	                    (int16_t)g_path->output_size.w,  
	                    (int16_t)g_path->output_size.h, 
	                    h_coeff, 
	                    v_coeff, 
	                    tmp_buf + (SC_COEFF_COEF_SIZE/2), 
	                    SC_COEFF_TMP_SIZE))) {
		kfree(tmp_buf);
		printk("SCALE DRV: _scale_set_sc_coeff error! \n");    
		return SCALE_RTN_GEN_COEFF_ERR;
	}	

	do {
		REG_OWR(SCALE_BASE, 1 << 4);
	} while ((1 << 6) != ((1 << 6) & REG_RD(SCALE_BASE)));
        
	for (i = 0; i < SC_COEFF_H_NUM; i++) {
		REG_WR(h_coeff_addr, *h_coeff);
		h_coeff_addr += 4;
		h_coeff++;
	}    
    
	for (i = 0; i < SC_COEFF_V_NUM; i++) {
		REG_WR(v_coeff_addr, *v_coeff);
		v_coeff_addr += 4;
		v_coeff++;
	}

	REG_MWR(SCALE_CFG, (0xF << 16), ((*v_coeff) & 0x0F) << 16);
	SCALE_TRACE("SCALE DRV: _scale_set_sc_coeff V[%d] = 0x%x \n", i,  (*v_coeff) & 0x0F);

	do {
		REG_MWR(SCALE_BASE, 1 << 4, 0 << 4);
	} while (0 != ((1 << 6) & REG_RD(SCALE_BASE))); 
	
	kfree(tmp_buf);

	return SCALE_RTN_SUCCESS;	
}

static irqreturn_t _scale_isr_root(int irq, void *dev_id)
{
	uint32_t                status;
	struct scale_frame      frame;
	uint32_t                flag;
	
	(void)irq; (void)dev_id;
	status = REG_RD(SCALE_INT_STS);

	if (unlikely(0 == (status & SCALE_IRQ_BIT))) {
		return IRQ_HANDLED;
	}

	SCALE_TRACE("SCALE DRV: _scale_isr_root \n");
	spin_lock_irqsave(&scale_lock, flag);
	if (g_path->user_func) {
		frame.yaddr = g_path->output_addr.yaddr;
		frame.uaddr = g_path->output_addr.uaddr;
		frame.vaddr = g_path->output_addr.vaddr;
		frame.width = g_path->output_size.w;
		if (SCALE_MODE_SLICE == g_path->scale_mode) {
			frame.height = g_path->slice_out_height;
			g_path->slice_out_height = REG_RD(SCALE_SLICE_VER);
			g_path->slice_out_height = (g_path->slice_out_height >> 16) & 0xFFF;
			frame.height = g_path->slice_out_height - frame.height;
		} else {
			frame.height = g_path->output_size.h;
		}
		g_path->user_func(&frame, g_path->user_data);
	}

	REG_OWR(SCALE_INT_CLR, SCALE_IRQ_BIT);
	atomic_dec(&g_path->start_flag);
	if (s_wait_flag) {
		up(&scale_done_sema);
		s_wait_flag = 0;
	}
	if (SCALE_MODE_NORMAL == g_path->scale_mode ||
		(SCALE_MODE_SLICE == g_path->scale_mode && g_path->output_size.h == g_path->slice_out_height)) {
		dcam_resize_end();
	}
	spin_unlock_irqrestore(&scale_lock, flag);

	return IRQ_HANDLED;
}

static void    _scale_reg_trace(void)
{
#ifdef SCALE_DRV_DEBUG
	uint32_t                addr = 0;

	for (addr = SCALE_REG_START; addr <= SCALE_REG_END; addr += 16) {
		printk("%x: %x %x %x %x \n",
			 addr,
		         REG_RD(addr),
		         REG_RD(addr + 4),
		         REG_RD(addr + 8),
		         REG_RD(addr + 12));
	}
#endif	
}

int32_t    _scale_alloc_tmp_buf(void)
{
	uint32_t                mem_szie;
	struct scale_addr       *vir_addr, *phy_addr;

	if (SCALE_MODE_SLICE == g_path->scale_mode) {
		vir_addr = &g_path->temp_buf_addr_vir;
		phy_addr = &g_path->temp_buf_addr;
		if (0 == phy_addr->yaddr ||
			0 == phy_addr->uaddr ||
			0 == phy_addr->vaddr) {
			mem_szie = g_path->output_size.w * g_path->slice_height;
			SCALE_TRACE("SCALE DRV: alloc_tmp_buf, swap buffer size, 0x%x \n", mem_szie);
			g_path->mem_order[0] = get_order(mem_szie);
			vir_addr->yaddr = (uint32_t)__get_free_pages(GFP_KERNEL | __GFP_COMP,
							g_path->mem_order[0]);
			if (NULL == (void*)vir_addr->yaddr) {
				printk("SCALE DRV: alloc_tmp_buf, y, no mem, 0x%x 0x%x \n",
					mem_szie, g_path->mem_order[0]);
				return SCALE_RTN_NO_MEM;
			}
			phy_addr->yaddr = virt_to_phys((void*)vir_addr->yaddr);

			g_path->mem_order[1] = g_path->mem_order[0];
			vir_addr->uaddr = (uint32_t)__get_free_pages(GFP_KERNEL | __GFP_COMP,
							g_path->mem_order[1]);
			if (NULL == (void*)vir_addr->uaddr) {
				printk("SCALE DRV: alloc_tmp_buf, u, no mem, 0x%x 0x%x \n",
					mem_szie, g_path->mem_order[1]);
				_scale_free_tmp_buf();
				return SCALE_RTN_NO_MEM;
			}
			phy_addr->uaddr = virt_to_phys((void*)vir_addr->uaddr);

			mem_szie = g_path->output_size.w * 8;
			SCALE_TRACE("SCALE DRV: alloc_tmp_buf, line buffer size, 0x%x \n", mem_szie);
			g_path->mem_order[2] = get_order(mem_szie);
			vir_addr->vaddr = (uint32_t)__get_free_pages(GFP_KERNEL | __GFP_COMP,
							g_path->mem_order[2]);
			if (NULL == (void*)vir_addr->vaddr) {
				printk("SCALE DRV: alloc_tmp_buf, v, no mem, 0x%x 0x%x \n",
					mem_szie, g_path->mem_order[2]);
				_scale_free_tmp_buf();
				return SCALE_RTN_NO_MEM;
			}
			phy_addr->vaddr = (uint32_t)virt_to_phys((void*)vir_addr->vaddr);
			g_path->use_local_tmp_buf = 1;
		}

		REG_WR(SCALE_FRM_SWAP_Y, phy_addr->yaddr);
		REG_WR(SCALE_FRM_SWAP_U, phy_addr->uaddr);
		REG_WR(SCALE_FRM_LINE,   phy_addr->vaddr);

		SCALE_TRACE("SCALE DRV: alloc_tmp_buf, yuv 0x%x 0x%x 0x%x, order %d %d \n",
			phy_addr->yaddr, phy_addr->uaddr, phy_addr->vaddr,
			g_path->mem_order[0], g_path->mem_order[2]);
	}

	return SCALE_RTN_SUCCESS;
}

int32_t     _scale_free_tmp_buf(void)
{
	struct scale_addr       *vir_addr = &g_path->temp_buf_addr_vir;
	struct scale_addr       *phy_addr = &g_path->temp_buf_addr;

	SCALE_TRACE("SCALE DRV: free tmp buf \n");

	if (g_path->temp_buf_src == 0) {
		if (vir_addr->yaddr) {
			free_pages(vir_addr->yaddr, g_path->mem_order[0]);
		}

		if (vir_addr->uaddr) {
			free_pages(vir_addr->uaddr, g_path->mem_order[1]);
		}

		if (vir_addr->vaddr) {
			free_pages(vir_addr->vaddr, g_path->mem_order[2]);
		}
	} else {
		g_path->temp_buf_src = 0;
	}

	memset((void*)vir_addr, 0, sizeof(struct scale_addr));
	memset((void*)phy_addr, 0, sizeof(struct scale_addr));

	return SCALE_RTN_SUCCESS;
}

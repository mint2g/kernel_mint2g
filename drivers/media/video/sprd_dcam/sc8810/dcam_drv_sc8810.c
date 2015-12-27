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
#include "dcam_drv_sc8810.h"
#include <mach/globalregs.h>
#include <linux/time.h>

#define ISP_PATH1 1
#define ISP_PATH2 2
#define ISP_DRV_SCALE_COEFF_BUF_SIZE		 (8*1024)
#define ISP_DRV_SCALE_COEFF_TMP_SIZE		 (6*1024)
#define ISP_DRV_SCALE_COEFF_COEF_SIZE   	          (1*1024)
#define ISP_DRV_JPG_BUF_UNIT			          (32*1024)
#define SCI_NULL 0
#define SCI_MEMSET memset
#define ISP_MEMCPY memcpy
#define SCI_ASSERT(...)
#define SCI_PASSERT(condition, format...)

#define SA_SHIRQ             IRQF_SHARED
#define IRQ_LINE_DCAM       27

typedef void (*ISP_ISR_PTR) (uint32_t base_addr);

#define ISP_SCALE1_H_TAB_OFFSET 0x200
#define ISP_SCALE1_V_TAB_OFFSET 0x2F0
#define ISP_SCALE2_H_TAB_OFFSET 0x400
#define ISP_SCALE2_V_TAB_OFFSET 0x4F0

#define ISP_IRQ_LINE_MASK 0x000001FFUL
#define ISP_IRQ_SENSOR_SOF_BIT BIT(0)
#define ISP_IRQ_SENSOR_EOF_BIT BIT(1)
#define ISP_IRQ_CAP_SOF_BIT BIT(2)
#define ISP_IRQ_CAP_EOF_BIT BIT(3)
#define ISP_IRQ_CMR_DONE_BIT BIT(4)
#define ISP_IRQ_CAP_BUF_OV_BIT BIT(5)
#define ISP_IRQ_SENSOR_LE_BIT BIT(6)
#define ISP_IRQ_SENSOR_FE_BIT BIT(7)
#define ISP_IRQ_JPG_BUF_OV_BIT BIT(8)
#define ISP_IRQ_REVIEW_DONE_BIT BIT(9)

#define ISP_CAP_FRAME_SKIP_NUM_MAX 0x10
#define ISP_CAP_FRAME_DECI_FACTOR_MAX 0x4
#define ISP_CAP_X_DECI_FACTOR_MAX 0x3
#define ISP_CAP_Y_DECI_FACTOR_MAX 0x3
#define ISP_CAP_FRAME_WIDTH_MAX   8192
#define ISP_CAP_FRAME_HEIGHT_MAX  4092
#define ISP_PATH_FRAME_WIDTH_MAX  4092
#define ISP_PATH_FRAME_HEIGHT_MAX 4092
#define ISP_PATH_SUB_SAMPLE_FACTOR_BASE 1
#define ISP_PATH_SCALE_LEVEL 64
#define ISP_PATH_SCALE_LEVEL_MAX 256
#define ISP_PATH_SCALE_LEVEL_MIN  16
#define ISP_PATH_SLICE_MASK  0xFFF
#define ISP_PATH_SUB_SAMPLE_MAX 3	/*0.....1/2, 1......1/4, 2......1/8, 3.......1/16 */

#define ISP_CLK_DOMAIN_AHB 1
#define ISP_CLK_DOMAIN_DCAM 0

#define ISP_PATH_ADDR_INVALIDE(addr) (SCI_NULL != (addr))
#define ISP_PATH_YUV_ADDR_INVALIDE(y,u,v)  (ISP_PATH_ADDR_INVALIDE(y) && ISP_PATH_ADDR_INVALIDE(u) && ISP_PATH_ADDR_INVALIDE(v))

#define ISP_DRV_RTN_IF_ERR  if(rtn) return rtn
#define ISP_CHECK_PARAM_ZERO_POINTER(n) do{if(SCI_NULL == (int)(n)) return ISP_DRV_RTN_PARA_ERR;}while(0)

#define ISP_DATA_CLEAR(a)                              do{memset((void *)(a),0,sizeof(*(a)));}while(0);

static uint32_t g_share_irq = 0x111;

enum {
	ISP_IRQ_SENSOR_SOF = 0,
	ISP_IRQ_SENSOR_EOF,
	ISP_IRQ_CAP_SOF,
	ISP_IRQ_CAP_EOF,
	ISP_IRQ_PATH1_DONE,
	ISP_IRQ_CAP_FIFO_OF,
	ISP_IRQ_SENSOR_LINE_ERR,
	ISP_IRQ_SENSOR_FRAME_ERR,
	ISP_IRQ_JPEG_BUF_OF,
	ISP_IRQ_PATH2_DONE,
	ISP_IRQ_NUMBER
};

enum {
	ISP_PATH_DATA_FORMAT_YUV = 0,
	ISP_PATH_DATA_FORMAT_RGB
};

enum {
	DCAM_CAP_MODE_SINGLE = 0,
	DCAM_CAP_MODE_MULTIPLE,
	DCAM_CAP__MODE_MAX
};

enum {
	ISP_MASTER_READ,
	ISP_MASTER_WRITE,
	ISP_MASTER_MAX
};

enum {
	ISP_MASTER_ENDIANNESS_BIG,
	ISP_MASTER_ENDIANNESS_LITTLE,
	ISP_MASTER_ENDIANNESS_HALFBIG,
	ISP_MASTER_ENDIANNESS_MAX
};

typedef struct _isp_cap_desc_tag {
	uint32_t input_format;
	uint32_t frame_deci_factor;
	uint32_t img_x_deci_factor;
	uint32_t img_y_deci_factor;
} ISP_CAP_DESCRIPTION_T;

typedef struct _isp_path_desc_tag {
	ISP_SIZE_T input_size;
	ISP_RECT_T input_rect;
	ISP_SIZE_T sc_input_size;
	ISP_SIZE_T output_size;
	ISP_FRAME_T input_frame;
	uint32_t input_format;
	ISP_FRAME_T *p_output_frame_head;
	ISP_FRAME_T *p_output_frame_cur;
	uint32_t output_frame_count;
	uint32_t output_format;
	uint32_t output_frame_flag;
	ISP_FRAME_T swap_frame;
	ISP_FRAME_T line_frame;
	uint32_t scale_en;
	uint32_t sub_sample_en;
	uint32_t sub_sample_factor;
	uint32_t sub_sample_mode;
	uint32_t slice_en;
	uint32_t h_scale_coeff;
	uint32_t v_scale_coeff;
} ISP_PATH_DESCRIPTION_T;

typedef struct _isp_module_tagss {
	ISP_MODE_E isp_mode;
	uint32_t module_addr;
	ISP_CAP_DESCRIPTION_T isp_cap;
	ISP_PATH_DESCRIPTION_T isp_path1;
	ISP_PATH_DESCRIPTION_T isp_path2;
	ISP_ISR_FUNC_PTR user_func[ISP_IRQ_NUMBER];
} ISP_MODULE_T;

static ISP_FRAME_T s_path1_frame[ISP_PATH1_FRAME_COUNT_MAX];
static ISP_FRAME_T s_path2_frame[ISP_PATH2_FRAME_COUNT_MAX];
static ISP_MODULE_T s_isp_mod;

uint32_t g_is_stop = 0;

static void _ISP_DrvierModuleReset(uint32_t base_addr);
static uint32_t _ISP_DriverReadIrqLine(uint32_t base_addr);
static void _ISP_DriverIrqClear(uint32_t base_addr, uint32_t mask);
static void _ISP_DriverIrqDisable(uint32_t base_addr, uint32_t mask);
static void _ISP_DriverIrqEnable(uint32_t base_addr, uint32_t mask);
static void _ISP_ISRSensorStartOfFrame(uint32_t base_addr);
static void _ISP_ISRSensorEndOfFrame(uint32_t base_addr);
static void _ISP_ISRCapStartOfFrame(uint32_t base_addr);
static void _ISP_ISRCapEndOfFrame(uint32_t base_addr);
static void _ISP_ISRPath1Done(uint32_t base_addr);
static void _ISP_ISRCapFifoOverflow(uint32_t base_addr);
static void _ISP_ISRSensorLineErr(uint32_t base_addr);
static void _ISP_ISRSensorFrameErr(uint32_t base_addr);
static void _ISP_ISRJpegBufOverflow(uint32_t base_addr);
static ISR_EXE_T _ISP_ISRSystemRoot(uint32_t param);
static void _ISP_DriverISRRoot(uint32_t base_addr);
static void _ISP_DriverLinkFrames(void);
static void _ISP_DriverAutoCopy(uint32_t base_addr);
static int32_t _ISP_DriverCalcSC1Size(void);
static int32_t _ISP_DriverSetSC1Coeff(uint32_t base_addr);
static int32_t _ISP_DriverPath1TrimAndScaling(uint32_t base_addr);
static int32_t _ISP_DriverCalcSC2Size(void);
static int32_t _ISP_DriverGenScxCoeff(uint32_t base_addr, uint32_t idxScx);
static void _ISP_ISRPath2Done(uint32_t base_addr);

static ISP_ISR_PTR s_isp_isr_list[ISP_IRQ_NUMBER] = {
	_ISP_ISRSensorStartOfFrame,
	_ISP_ISRSensorEndOfFrame,
	_ISP_ISRCapStartOfFrame,
	_ISP_ISRCapEndOfFrame,
	_ISP_ISRPath1Done,
	_ISP_ISRCapFifoOverflow,
	_ISP_ISRSensorLineErr,
	_ISP_ISRSensorFrameErr,
	_ISP_ISRJpegBufOverflow,
	NULL			//_ISP_ISRPath2Done,
};

#ifdef DCAM_DRV_DEBUG
static void _ISP_GetReg(void)
{
	printk("[DCAM DRV:DCAM_CFG:0x%x]\n", _pard(DCAM_CFG));
	printk("[DCAM DRV:DCAM_PATH_CFG:0x%x\n]", _pard(DCAM_PATH_CFG));
	printk("[DCAM DRV:DCAM_SRC_SIZE:0x%x]\n", _pard(DCAM_SRC_SIZE));
	printk("[DCAM DRV:DCAM_DES_SIZE:0x%x]\n", _pard(DCAM_DES_SIZE));
	printk("[DCAM DRV:DCAM_TRIM_START:0x%x]\n", _pard(DCAM_TRIM_START));
	printk("[DCAM DRV:DCAM_TRIM_SIZE:0x%x]\n", _pard(DCAM_TRIM_SIZE));
	printk("[DCAM DRV:DCAM_INT_STS:0x%x]\n", _pard(DCAM_INT_STS));
	printk("[DCAM DRV:DCAM_INT_MASK:0x%x]\n", _pard(DCAM_INT_MASK));
	printk("[DCAM DRV:DCAM_INT_CLR:0x%x]\n", _pard(DCAM_INT_CLR));
	printk("[DCAM DRV:DCAM_INT_RAW:0x%x]\n", _pard(DCAM_INT_RAW));
	printk("[DCAM DRV:ENDIAN_SEL:0x%x]\n", _pard(ENDIAN_SEL));
	printk("[DCAM DRV:DCAM_ADDR_7:0x%x]\n", _pard(DCAM_ADDR_7));
	printk("[DCAM DRV:DCAM_ADDR_8:0x%x]\n", _pard(DCAM_ADDR_8));
	printk("[DCAM DRV:CAP_CTRL:0x%x]\n", _pard(CAP_CTRL));
	printk("[DCAM DRV:CAP_FRM_CNT:0x%x]\n", _pard(CAP_FRM_CNT));
	printk("[DCAM DRV:CAP_START:0x%x]\n", _pard(CAP_START));
	printk("[DCAM DRV:CAP_END:0x%x]\n", _pard(CAP_END));
	printk("[DCAM DRV:CAP_IMAGE_DECI:0x%x]\n", _pard(CAP_IMAGE_DECI));
	printk("[DCAM DRV:CAP_JPG_CTL:0x%x]\n", _pard(CAP_JPG_CTL));
	printk("[DCAM DRV:CAP_JPG_FRM_SIZE:0x%x]\n", _pard(CAP_JPG_FRM_SIZE));
	printk("[DCAM DRV:REV_PATH_CFG:0x%x]\n", _pard(REV_PATH_CFG));
	printk("[DCAM DRV:REV_SRC_SIZE:0x%x]\n", _pard(REV_SRC_SIZE));
	printk("[DCAM DRV:REV_DES_SIZE:0x%x]\n", _pard(REV_DES_SIZE));
	printk("[DCAM DRV:REV_TRIM_START:0x%x]\n", _pard(REV_TRIM_START));
	printk("[DCAM DRV:REV_TRIM_SIZE:0x%x]\n", _pard(REV_TRIM_SIZE));
	printk("[DCAM DRV:SLICE_VER_CNT:0x%x]\n", _pard(SLICE_VER_CNT));
	printk("[DCAM DRV:DCAM_ADDR_0:0x%x]\n", _pard(DCAM_ADDR_0));
	printk("[DCAM DRV:DCAM_ADDR_1:0x%x]\n", _pard(DCAM_ADDR_1));
}
#endif

static void _ISP_DrvierModuleReset(uint32_t base_addr)
{
/*jianping.wang
	*(volatile uint32_t *)(base_addr + ISP_AHB_CTRL_SOFT_RESET_OFFSET) |=
	    BIT(1) | BIT(2);
	*(volatile uint32_t *)(base_addr + ISP_AHB_CTRL_SOFT_RESET_OFFSET) |=
	    BIT(1) | BIT(2);
	*(volatile uint32_t *)(base_addr + ISP_AHB_CTRL_SOFT_RESET_OFFSET) &=
	    ~(BIT(1) | BIT(2));
*/
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,AHB_SOFT_RST_CCIR_SOFT_RST,AHB_SOFT_RST);
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,AHB_SOFT_RST_DCAM_SOFT_RST,AHB_SOFT_RST);
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,AHB_SOFT_RST_CCIR_SOFT_RST,AHB_SOFT_RST);
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,AHB_SOFT_RST_DCAM_SOFT_RST,AHB_SOFT_RST);
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,AHB_SOFT_RST_CCIR_SOFT_RST,AHB_SOFT_RST);
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,AHB_SOFT_RST_DCAM_SOFT_RST,AHB_SOFT_RST);
	printk("_ISP_DrvierModuleReset .\n");
}

int32_t ISP_DriverModuleInit(uint32_t base_addr)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;

	ISP_DATA_CLEAR(&s_isp_mod);

	ISP_DriverScalingCoeffReset();
	_ISP_DriverLinkFrames();
	s_isp_mod.module_addr = base_addr;

	DCAM_DRV_TRACE("DCAM DRV:ISP Module address is 0x%x", base_addr);
	return rtn;
}

static uint32_t _ISP_DriverReadIrqLine(uint32_t base_addr)
{
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;

	return p_isp_reg->dcam_int_stat_u.dwValue;
}

static void _ISP_DriverIrqClear(uint32_t base_addr, uint32_t mask)
{
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;
	p_isp_reg->dcam_int_clr_u.dwValue |= mask;
}

static void _ISP_DriverIrqDisable(uint32_t base_addr, uint32_t mask)
{
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;
	p_isp_reg->dcam_int_mask_u.dwValue &= ~mask;
}

static void _ISP_DriverIrqEnable(uint32_t base_addr, uint32_t mask)
{
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;
	p_isp_reg->dcam_int_mask_u.dwValue |= mask;
}

static ISR_EXE_T _ISP_ISRSystemRoot(uint32_t param)
{
	_ISP_DriverISRRoot(s_isp_mod.module_addr);
	return ISR_DONE;
}

static void _ISP_DriverISRRoot(uint32_t base_addr)
{
	uint32_t irq_line, irq_status;
	uint32_t i;

	irq_line = ISP_IRQ_LINE_MASK & _ISP_DriverReadIrqLine(base_addr);
	irq_status = irq_line;
	for (i = ISP_IRQ_NUMBER - 1; i >= 0; i--) {
		if (irq_line & (1 << (uint32_t) i)) {
			s_isp_isr_list[i] (base_addr);
		}
		irq_line &= ~(uint32_t) (1 << (uint32_t) i);	//clear the interrupt flag
		if (!irq_line)	//no interrupt source leaved
			break;
	}
	_ISP_DriverIrqClear(base_addr, irq_status);
}

static void _ISP_ISRSensorStartOfFrame(uint32_t base_addr)
{
	ISP_ISR_FUNC_PTR user_func =
	    s_isp_mod.user_func[ISP_IRQ_NOTICE_SENSOR_SOF];

	if (user_func) {
		(*user_func) (NULL);
	}
	return;
}

static void _ISP_ISRSensorEndOfFrame(uint32_t base_addr)
{
	ISP_ISR_FUNC_PTR user_func =
	    s_isp_mod.user_func[ISP_IRQ_NOTICE_SENSOR_EOF];

	if (user_func) {
		(*user_func) (NULL);
	}
	return;
}

static void _ISP_ISRCapStartOfFrame(uint32_t base_addr)
{
	ISP_ISR_FUNC_PTR user_func =
	    s_isp_mod.user_func[ISP_IRQ_NOTICE_CAP_SOF];

	if (user_func) {
		(*user_func) (NULL);
	}
	return;
}

static void _ISP_ISRCapEndOfFrame(uint32_t base_addr)
{
	ISP_ISR_FUNC_PTR user_func =
	    s_isp_mod.user_func[ISP_IRQ_NOTICE_CAP_EOF];

	if (user_func) {
		(*user_func) (NULL);
	}
	return;
}

static uint32_t gettime(void)
{
	struct timeval val;
	uint32_t ret;

	do_gettimeofday(&val);
	ret = (val.tv_sec * 1000000 + val.tv_usec) / 1000;
	return ret;
}

static void _ISP_ISRPath1Done(uint32_t base_addr)
{
	ISP_ISR_FUNC_PTR user_func =
	    s_isp_mod.user_func[ISP_IRQ_NOTICE_PATH1_DONE];
	ISP_FRAME_T *frame_curr = s_isp_mod.isp_path1.p_output_frame_cur->prev;
	ISP_PATH_DESCRIPTION_T *p_path = &s_isp_mod.isp_path1;

	frame_curr->width = p_path->output_size.w;
	frame_curr->height = p_path->output_size.h;

	if (user_func) {
		(*user_func) ((void *)frame_curr);
	}
	return;
}

static void _ISP_ISRPath2Done(uint32_t base_addr)
{
	ISP_ISR_FUNC_PTR user_func =
	    s_isp_mod.user_func[ISP_IRQ_NOTICE_PATH2_DONE];
	ISP_FRAME_T *frame_curr;
	ISP_PATH_DESCRIPTION_T *p_path = &s_isp_mod.isp_path2;

	frame_curr = p_path->p_output_frame_cur->prev;
	frame_curr->flags = p_path->output_frame_flag;
	frame_curr->width = p_path->output_size.w;
	frame_curr->height = p_path->output_size.h;

	printk
	    ("DCAM DRV: _ISP_ISRPath2Done y,u,v = {0x%x,0x%x,0x%x} ,width,height = {%d, %d}",
	     frame_curr->yaddr, frame_curr->uaddr, frame_curr->vaddr,
	     frame_curr->width, frame_curr->height);

	if (user_func) {
		(*user_func) ((void *)frame_curr);
	}
	return;
}

static void _ISP_ISRCapFifoOverflow(uint32_t base_addr)
{
	ISP_ISR_FUNC_PTR user_func =
	    s_isp_mod.user_func[ISP_IRQ_NOTICE_CAP_FIFO_OF];

	DCAM_DRV_ERR("DCAM DRV:_ISP_ISRCapFifoOverflow .\n");

	if (user_func) {
		(*user_func) (NULL);
	}
	return;
}

static void _ISP_ISRSensorLineErr(uint32_t base_addr)
{
	ISP_ISR_FUNC_PTR user_func =
	    s_isp_mod.user_func[ISP_IRQ_NOTICE_SENSOR_LINE_ERR];

	DCAM_DRV_ERR("DCAM DRV:_ISP_ISRSensorLineErr .\n");

	if (user_func) {
		(*user_func) (NULL);
	}
	return;
}

static void _ISP_ISRSensorFrameErr(uint32_t base_addr)
{
	ISP_ISR_FUNC_PTR user_func =
	    s_isp_mod.user_func[ISP_IRQ_NOTICE_SENSOR_FRAME_ERR];
	DCAM_DRV_ERR("DCAM DRV:_ISP_ISRSensorFrameErr .\n");

	if (user_func) {
		(*user_func) (NULL);
	}
	return;
}

static void _ISP_ISRJpegBufOverflow(uint32_t base_addr)
{
	ISP_ISR_FUNC_PTR user_func =
	    s_isp_mod.user_func[ISP_IRQ_NOTICE_JPEG_BUF_OF];

	DCAM_DRV_ERR("DCAM DRV:_ISP_ISRJpegBufOverflow.\n");

	if (user_func) {
		(*user_func) (NULL);
	}
	return;
}

static void _ISP_DriverLinkFrames(void)
{
	uint32_t i = 0;
	ISP_FRAME_T *p_path1_frame = &s_path1_frame[0];
	ISP_FRAME_T *p_path2_frame = &s_path2_frame[0];

	for (i = 0; i < ISP_PATH1_FRAME_COUNT_MAX; i++) {
		ISP_DATA_CLEAR(p_path1_frame + i);
		(p_path1_frame + i)->next =
		    p_path1_frame + (i + 1) % ISP_PATH1_FRAME_COUNT_MAX;
		(p_path1_frame + i)->prev =
		    p_path1_frame + (i - 1 +
				     ISP_PATH1_FRAME_COUNT_MAX) %
		    ISP_PATH1_FRAME_COUNT_MAX;
	}

	for (i = 0; i < ISP_PATH2_FRAME_COUNT_MAX; i++) {
		ISP_DATA_CLEAR(p_path2_frame + i);
		(p_path2_frame + i)->next =
		    p_path2_frame + (i + 1) % ISP_PATH2_FRAME_COUNT_MAX;
		(p_path2_frame + i)->prev =
		    p_path2_frame + (i - 1 +
				     ISP_PATH2_FRAME_COUNT_MAX) %
		    ISP_PATH2_FRAME_COUNT_MAX;
	}

	s_isp_mod.isp_path1.p_output_frame_head = p_path1_frame;
	s_isp_mod.isp_path2.p_output_frame_head = p_path2_frame;
	s_isp_mod.isp_path1.p_output_frame_cur = p_path1_frame;
	s_isp_mod.isp_path2.p_output_frame_cur = p_path2_frame;
}

static void _ISP_DriverAutoCopy(uint32_t base_addr)
{
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;
	p_isp_reg->dcam_path_cfg_u.mBits.auto_copy_cap = 1;
}

static void _ISP_DriverForeCopy(uint32_t base_addr)
{
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;
	p_isp_reg->dcam_path_cfg_u.mBits.frc_copy_cap = 1;
	p_isp_reg->dcam_path_cfg_u.mBits.frc_copy_cap = 1;
	p_isp_reg->dcam_path_cfg_u.mBits.frc_copy_cap = 0;
}

static int32_t _ISP_DriverPath1TrimAndScaling(uint32_t base_addr)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_PATH_DESCRIPTION_T *p_path = &s_isp_mod.isp_path1;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;

	DCAM_DRV_TRACE
	    ("DCAM DRV:_ISP_DriverPath1TrimAndScaling:input size:%d,%d,input rect:%d,%d\n",
	     p_path->input_size.w, p_path->input_size.h, p_path->input_rect.w,
	     p_path->input_rect.h);

	if (ISP_CAP_MODE_JPEG == p_isp_reg->cap_ctrl_u.mBits.sensor_mode) {
		p_isp_reg->dcam_path_cfg_u.mBits.cam1_trim_eb = 0;
		p_isp_reg->dcam_path_cfg_u.mBits.scale_bypass = 1;
		p_path->scale_en = 0;
		return rtn;
	}

	/*trim config */
	if (p_path->input_size.w != p_path->input_rect.w
	    || p_path->input_size.h != p_path->input_rect.h) {
		p_isp_reg->dcam_path_cfg_u.mBits.cam1_trim_eb = 1;
	} else {
		p_isp_reg->dcam_path_cfg_u.mBits.cam1_trim_eb = 0;
	}

	/*scaling config */
	rtn = _ISP_DriverCalcSC1Size();
	if (rtn)
		return rtn;

	if (p_path->sc_input_size.w != p_path->output_size.w
	    || p_path->sc_input_size.h != p_path->output_size.h) {
		p_isp_reg->dcam_path_cfg_u.mBits.scale_bypass = 0;
		rtn = _ISP_DriverSetSC1Coeff(base_addr);
		p_path->scale_en = 1;
	} else {
		p_isp_reg->dcam_path_cfg_u.mBits.scale_bypass = 1;
		p_path->scale_en = 0;
	}
	return rtn;
}

static int32_t _ISP_DriverSetSC2Coeff(uint32_t base_addr)
{
	return _ISP_DriverGenScxCoeff(base_addr, ISP_PATH2);
}

static int32_t _ISP_DriverPath2TrimAndScaling(uint32_t base_addr)
{
	uint32_t rtn = ISP_DRV_RTN_SUCCESS;
	ISP_PATH_DESCRIPTION_T *p_path = &s_isp_mod.isp_path2;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;

	if (ISP_CAP_MODE_JPEG == p_isp_reg->cap_ctrl_u.mBits.sensor_mode) {
		p_isp_reg->rev_path_cfg_u.mBits.trim_eb = 0;
		p_isp_reg->rev_path_cfg_u.mBits.scale_bypass = 1;
		return rtn;
	}
	/*trim config */
	if (p_path->input_size.w != p_path->input_rect.w
	    || p_path->input_size.h != p_path->input_rect.h) {
		p_isp_reg->rev_path_cfg_u.mBits.trim_eb = 1;
	} else {
		p_isp_reg->rev_path_cfg_u.mBits.trim_eb = 0;
	}

	/*scaling config */
	rtn = _ISP_DriverCalcSC2Size();
	ISP_DRV_RTN_IF_ERR;

	if (p_path->sub_sample_en) {
		p_isp_reg->rev_path_cfg_u.mBits.sub_sample_eb = 1;
		p_isp_reg->rev_path_cfg_u.mBits.sub_sample_mode =
		    p_path->sub_sample_mode;
	} else {
		p_isp_reg->rev_path_cfg_u.mBits.sub_sample_eb = 0;
	}

	if (p_path->sc_input_size.w != p_path->output_size.w
	    || p_path->sc_input_size.h != p_path->output_size.h) {
		p_isp_reg->rev_path_cfg_u.mBits.scale_bypass = 0;
		rtn = _ISP_DriverSetSC2Coeff(base_addr);
	} else {
		p_isp_reg->rev_path_cfg_u.mBits.scale_bypass = 1;
	}

	if (p_path->slice_en) {
		p_isp_reg->rev_path_cfg_u.mBits.scale_mode = 1;
	} else {
		p_isp_reg->rev_path_cfg_u.mBits.scale_mode = 0;
	}
	return rtn;
}

static int32_t _ISP_DriverCalcSC1Size(void)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_PATH_DESCRIPTION_T *p_path = &s_isp_mod.isp_path1;

	if (p_path->input_rect.w > p_path->output_size.w * ISP_PATH_SC_COEFF_MAX
	    || p_path->input_rect.h >
	    p_path->output_size.h * ISP_PATH_SC_COEFF_MAX
	    || p_path->input_rect.w * ISP_PATH_SC_COEFF_MAX <
	    p_path->output_size.w
	    || p_path->input_rect.h * ISP_PATH_SC_COEFF_MAX <
	    p_path->output_size.h) {
		rtn = ISP_DRV_RTN_PATH_SC_COEFF_ERR;
	} else {
		p_path->sc_input_size.w = p_path->input_rect.w;
		p_path->sc_input_size.h = p_path->input_rect.h;
	}
	return rtn;
}

static uint32_t _ISP_DriverGetSubSampleFactor(uint32_t * src_width,
					      uint32_t * src_height,
					      uint32_t dst_width,
					      uint32_t dst_height)
{
	if (*src_width > (dst_width * ISP_PATH_SC_COEFF_MAX)
	    || *src_height > (dst_height * ISP_PATH_SC_COEFF_MAX)) {
		*src_width = *src_width >> 1;
		*src_height = *src_height >> 1;
		return _ISP_DriverGetSubSampleFactor(src_width, src_height,
						     dst_width,
						     dst_height) +
		    ISP_PATH_SUB_SAMPLE_FACTOR_BASE;
	} else {
		return 0;
	}
}

static int32_t _ISP_DriverCalcSC2Size(void)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_PATH_DESCRIPTION_T *p_path = &s_isp_mod.isp_path2;

	if (p_path->input_rect.w * ISP_PATH_SC_COEFF_MAX < p_path->output_size.w
	    || p_path->input_rect.h * ISP_PATH_SC_COEFF_MAX <
	    p_path->output_size.h) {
		rtn = ISP_DRV_RTN_PATH_SC_COEFF_ERR;
	} else if (p_path->input_rect.w >
		   p_path->output_size.w * ISP_PATH_SC_COEFF_MAX
		   || p_path->input_rect.h >
		   p_path->output_size.h * ISP_PATH_SC_COEFF_MAX) {
		p_path->sc_input_size.w = p_path->input_rect.w;
		p_path->sc_input_size.h = p_path->input_rect.h;

		p_path->sub_sample_factor =
		    _ISP_DriverGetSubSampleFactor(&p_path->sc_input_size.w,
						  &p_path->sc_input_size.h,
						  p_path->output_size.w,
						  p_path->output_size.h);

		if (((s_isp_mod.isp_mode == ISP_MODE_MPEG || ISP_MODE_PREVIEW_EX == s_isp_mod.isp_mode) && p_path->sub_sample_factor > ISP_PATH_SUB_SAMPLE_FACTOR_BASE) ||	// in mpeg or preview_ex mode, path2 do deci by 1/2
		    p_path->sub_sample_factor >
		    (ISP_PATH_SUB_SAMPLE_MAX +
		     ISP_PATH_SUB_SAMPLE_FACTOR_BASE)) {
			rtn = ISP_DRV_RTN_PATH_SC_COEFF_ERR;
		} else {
			p_path->sc_input_size.w =
			    (uint32_t) (p_path->input_rect.w /
					(uint32_t) (1 << p_path->
						    sub_sample_factor));
			p_path->sc_input_size.h =
			    (uint32_t) (p_path->input_rect.h /
					(uint32_t) (1 << p_path->
						    sub_sample_factor));
			p_path->sub_sample_en = 1;
			p_path->sub_sample_mode =
			    p_path->sub_sample_factor -
			    ISP_PATH_SUB_SAMPLE_FACTOR_BASE;
		}
	} else {
		p_path->sc_input_size.w = p_path->input_rect.w;
		p_path->sc_input_size.h = p_path->input_rect.h;
	}
	return rtn;
}

static int32_t _ISP_DriverGenScxCoeff(uint32_t base_addr, uint32_t idxScx)
{
	ISP_PATH_DESCRIPTION_T *p_path = SCI_NULL;
	uint32_t i = 0;
	uint32_t HScaleAddr = 0;
	uint32_t VScaleAddr = 0;
	uint32_t *pTmpBuf = SCI_NULL;
	uint32_t *pHCoeff = SCI_NULL;
	uint32_t *pVCoeff = SCI_NULL;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;

	if (ISP_PATH1 != idxScx && ISP_PATH2 != idxScx)
		return ISP_DRV_RTN_PARA_ERR;

	DCAM_DRV_TRACE("DCAM DRV: _ISP_DriverGenScxCoeff Entry");

	if (ISP_PATH1 == idxScx) {
		p_path = &s_isp_mod.isp_path1;
		HScaleAddr = base_addr + ISP_SCALE1_H_TAB_OFFSET;
		VScaleAddr = base_addr + ISP_SCALE1_V_TAB_OFFSET;
	} else {
		p_path = &s_isp_mod.isp_path2;
		HScaleAddr = base_addr + ISP_SCALE2_H_TAB_OFFSET;
		VScaleAddr = base_addr + ISP_SCALE2_V_TAB_OFFSET;
	}

	pTmpBuf =
	    (uint32_t *) kmalloc(ISP_DRV_SCALE_COEFF_BUF_SIZE, GFP_KERNEL);

	if (SCI_NULL == pTmpBuf) {
		DCAM_DRV_ERR
		    ("DCAM DRV ERROR:_ISP_DriverGenScxCoeff,kmalloc addr is NULL!");
		return ISP_DRV_RTN_KMALLOC_BUF_ERR;
	}
	SCI_MEMSET(pTmpBuf, 0, ISP_DRV_SCALE_COEFF_BUF_SIZE);

	pHCoeff = pTmpBuf;
	pVCoeff = pTmpBuf + (ISP_DRV_SCALE_COEFF_COEF_SIZE / 4);

	DCAM_DRV_TRACE
	    ("DCAM DRV:_ISP_DriverGenScxCoeff i_w/i_h/o_w/o_h = {%d, %d, %d, %d,}",
	     (int16_t) p_path->sc_input_size.w,
	     (int16_t) p_path->sc_input_size.h, (int16_t) p_path->output_size.w,
	     (int16_t) p_path->output_size.h);

	if (!(GenScaleCoeff((int16_t) p_path->sc_input_size.w,
			    (int16_t) p_path->sc_input_size.h,
			    (int16_t) p_path->output_size.w,
			    (int16_t) p_path->output_size.h,
			    pHCoeff,
			    pVCoeff,
			    pTmpBuf + (ISP_DRV_SCALE_COEFF_COEF_SIZE / 4 * 2),
			    ISP_DRV_SCALE_COEFF_TMP_SIZE))) {
		kfree(pTmpBuf);
		pTmpBuf = SCI_NULL;
		DCAM_DRV_ERR
		    ("DCAM DRV ERROR: _ISP_DriverGenScxCoeff GenScaleCoeff error!");
		return ISP_DRV_RTN_GEN_SCALECOEFF_ERR;
	}

	if (ISP_PATH1 == idxScx) {
		do {
			p_isp_reg->dcam_cfg_u.mBits.path1_clock_switch =
			    ISP_CLK_DOMAIN_AHB;
		} while (!(p_isp_reg->dcam_cfg_u.mBits.path1_clock_status));

		DCAM_DRV_TRACE
		    ("DCAM DRV: _ISP_DriverGenScxCoeff path1 Domain = 0x%x",
		     p_isp_reg->dcam_cfg_u.mBits.path1_clock_switch);
	} else {
		do {
			p_isp_reg->dcam_cfg_u.mBits.path2_clock_switch =
			    ISP_CLK_DOMAIN_AHB;
		} while (!(p_isp_reg->dcam_cfg_u.mBits.path2_clock_status));

		DCAM_DRV_TRACE
		    ("DCAM DRV: _ISP_DriverGenScxCoeff path 2 Domain = 0x%x",
		     p_isp_reg->dcam_cfg_u.mBits.path2_clock_switch);
	}

	for (i = 0; i < ISP_SCALE_COEFF_H_NUM; i++) {
		*(volatile uint32_t *)HScaleAddr = *pHCoeff;
		/*DCAM_DRV_TRACE("DCAM DRV:Coeff H[%d] = 0x%x.\n", i, *pHCoeff); */
		HScaleAddr += 4;
		pHCoeff++;
	}

	for (i = 0; i < ISP_SCALE_COEFF_V_NUM; i++) {
		*(volatile uint32_t *)VScaleAddr = *pVCoeff;
		/*DCAM_DRV_TRACE("DCAM DRV: Coeff V[%d] = 0x%x.\n", i, *pVCoeff); */
		VScaleAddr += 4;
		pVCoeff++;
	}

	if (ISP_PATH1 == idxScx) {
		p_isp_reg->dcam_path_cfg_u.mBits.ver_down_tap =
		    (*pVCoeff) & 0x0F;
	} else {
		p_isp_reg->rev_path_cfg_u.mBits.ver_down_tap =
		    (*pVCoeff) & 0x0F;
	}

	DCAM_DRV_TRACE("DCAM DRV: _ISP_DriverGenScxCoeff V[%d] = 0x%x", i,
		       (*pVCoeff) & 0x0F);
	DCAM_DRV_TRACE("DCAM DRV:_ISP_DriverGenScxCoeff V[%d] = 0x%x", i,
		       p_isp_reg->dcam_path_cfg_u.mBits.ver_down_tap);

	if (ISP_PATH1 == idxScx) {
		p_isp_reg->dcam_cfg_u.mBits.path1_clock_switch =
		    ISP_CLK_DOMAIN_DCAM;
		DCAM_DRV_TRACE("DCAM DRV: _ISP_DriverGenScxCoeff Domain = 0x%x",
			       p_isp_reg->dcam_cfg_u.mBits.path1_clock_switch);
	} else {
		p_isp_reg->dcam_cfg_u.mBits.path2_clock_switch =
		    ISP_CLK_DOMAIN_DCAM;
		DCAM_DRV_TRACE("DCAM DRV: _ISP_DriverGenScxCoeff Domain = 0x%x",
			       p_isp_reg->dcam_cfg_u.mBits.path2_clock_switch);
	}
	kfree(pTmpBuf);
	pTmpBuf = SCI_NULL;
	return ISP_DRV_RTN_SUCCESS;
}

static int32_t _ISP_DriverSetSC1Coeff(uint32_t base_addr)
{
	return _ISP_DriverGenScxCoeff(base_addr, ISP_PATH1);
}

void _ISP_DriverEnableInt(void)
{
	_paod(INT_IRQ_EN, 1 << IRQ_LINE_DCAM);
}

void _ISP_DriverDisableInt(void)
{
	_paod(INT_IRQ_DISABLE, 1 << IRQ_LINE_DCAM);
}

static irqreturn_t _ISP_DriverISR(int irq, void *dev_id)
{
	uint32_t value = 0;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) s_isp_mod.module_addr;

	if(!s_isp_mod.module_addr)
	{
		DCAM_DRV_ERR("DCAM DRV:s_isp_mod.module_addr is NULL.\n");
		return IRQ_NONE;
	}
	value = p_isp_reg->dcam_int_stat_u.dwValue & ISP_IRQ_LINE_MASK;
	if (0 == value)
		return IRQ_NONE;
#if 0
	if (1 == p_isp_reg->dcam_cfg_u.mBits.review_path_eb) {
		return IRQ_NONE;
	}
#endif

	_ISP_ISRSystemRoot(0);
	return IRQ_HANDLED;
}

void ISP_DriverHandleErr(uint32_t ahb_ctrl_addr, uint32_t base_addr)
{
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;

	DCAM_DRV_ERR
	    ("DCAM DRV:ISP_DriverHandleErr ahb_ctrl_addr=0x%x,base_addr=0x%x.\n",
	     ahb_ctrl_addr, base_addr);

	p_isp_reg->dcam_path_cfg_u.mBits.cap_eb = 0;
	/*_ISP_DrvierModuleReset(ahb_ctrl_addr);*/
	DCAM_DRV_ERR("DCAM DRV:ISP_DriverHandleErr e.\n");
}

int32_t ISP_DriverModuleEnable(uint32_t ahb_ctrl_addr)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;

	ISP_CHECK_PARAM_ZERO_POINTER(ahb_ctrl_addr);
/*jianping.wang
	*(volatile uint32_t *)(ahb_ctrl_addr + ISP_AHB_CTRL_MOD_EN_OFFSET) |=
	    (BIT(1) | BIT(2));
	*(volatile uint32_t *)(ahb_ctrl_addr + ISP_AHB_CTRL_MEM_SW_OFFSET) &= ~BIT(0);	// switch memory to ISP
*/
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,AHB_CTL0_DCAM_EN,AHB_CTL0);
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,AHB_CTL0_CCIR_EN,AHB_CTL0);
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,AHB_CTRL1_DCAM_BUF_SW,AHB_CTL1);

	_ISP_DrvierModuleReset(ahb_ctrl_addr);
	return rtn;
}

int32_t ISP_DriverModuleDisable(uint32_t ahb_ctrl_addr)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
/*jianping.wang
	*(volatile uint32_t *)(ahb_ctrl_addr + ISP_AHB_CTRL_MEM_SW_OFFSET) |= BIT(0);	// switch memory to ARM
	*(volatile uint32_t *)(ahb_ctrl_addr + ISP_AHB_CTRL_MOD_EN_OFFSET) &=
	    ~(BIT(1) | BIT(2));
*/
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,AHB_CTRL1_DCAM_BUF_SW,AHB_CTL1);
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,AHB_CTL0_DCAM_EN,AHB_CTL0);
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,AHB_CTL0_CCIR_EN,AHB_CTL0);
	return rtn;
}

int32_t ISP_DriverSoftReset(uint32_t ahb_ctrl_addr)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;

	ISP_DriverModuleEnable(ahb_ctrl_addr);
	_ISP_DrvierModuleReset(ahb_ctrl_addr);
	ISP_DriverScalingCoeffReset();
	return rtn;
}

void ISP_DriverIramSwitch(uint32_t base_addr, uint32_t isp_or_arm)
{
/*jianping.wang
	if (isp_or_arm == IRAM_FOR_ISP) {
		_paad(base_addr + ISP_AHB_CTRL_MEM_SW_OFFSET, ~BIT(0));
	} else {
		_paod(base_addr + ISP_AHB_CTRL_MEM_SW_OFFSET, BIT(0));
	}
*/
	if (isp_or_arm == IRAM_FOR_ISP) {
		sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,AHB_CTRL1_DCAM_BUF_SW,AHB_CTL1);
	} else {
		sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL,AHB_CTRL1_DCAM_BUF_SW,AHB_CTL1);
	}
}

int32_t ISP_DriverSetClk(uint32_t pll_src_addr, ISP_CLK_SEL_E clk_sel)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	return rtn;
}

int32_t ISP_DriverStart(uint32_t base_addr)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;

	g_is_stop = 0;
	ISP_CHECK_PARAM_ZERO_POINTER(base_addr);
	DCAM_DRV_TRACE("DCAM DRV: isp mode: %x", s_isp_mod.isp_mode);

	if ((ISP_MODE_PREVIEW == s_isp_mod.isp_mode)
	    || (ISP_MODE_CAPTURE == s_isp_mod.isp_mode)) {
		rtn = _ISP_DriverPath1TrimAndScaling(base_addr);
		ISP_DRV_RTN_IF_ERR;
		DCAM_DRV_TRACE
		    ("DCAM DRV: _ISP_DriverPath1TrimAndScaling ok.\n");

		_ISP_DriverIrqClear(base_addr, ISP_IRQ_LINE_MASK);
		_ISP_DriverIrqEnable(base_addr, ISP_IRQ_LINE_MASK);
		DCAM_DRV_TRACE("DCAM DRV: int mask:%x", _pard(DCAM_INT_MASK));
		p_isp_reg->endian_sel_u.mBits.dcam_output_endian_y =
		    ISP_MASTER_ENDIANNESS_LITTLE;
		p_isp_reg->endian_sel_u.mBits.dcam_output_endian_uv =
		    ISP_MASTER_ENDIANNESS_LITTLE;
	} else {
		rtn = _ISP_DriverPath2TrimAndScaling(base_addr);
		ISP_DRV_RTN_IF_ERR;

		_ISP_DriverIrqClear(base_addr, ISP_IRQ_LINE_MASK);
		_ISP_DriverIrqEnable(base_addr, ISP_IRQ_LINE_MASK);
		p_isp_reg->endian_sel_u.mBits.review_output_endian_y =
		    ISP_MASTER_ENDIANNESS_LITTLE;
		p_isp_reg->endian_sel_u.mBits.review_output_endian_uv =
		    ISP_MASTER_ENDIANNESS_LITTLE;
	}
#ifdef DCAM_DRV_DEBUG
	_ISP_GetReg();
#endif
	_ISP_DriverForeCopy(base_addr);
	p_isp_reg->dcam_path_cfg_u.mBits.cap_eb = 1;
	DCAM_DRV_TRACE("DCAM DRV: start:DCAM_PATH_CFG: %x.\n",
		       _pard(DCAM_PATH_CFG));
	return rtn;
}

int32_t ISP_DriverStop(uint32_t base_addr)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;

	if (1 == g_is_stop)
		return rtn;

	ISP_CHECK_PARAM_ZERO_POINTER(base_addr);
	_ISP_DriverIrqDisable(base_addr, ISP_IRQ_LINE_MASK);
	switch (s_isp_mod.isp_mode) {
	case ISP_MODE_CAPTURE:
	case ISP_MODE_MPEG:
	case ISP_MODE_VT:
	case ISP_MODE_PREVIEW_EX:
		p_isp_reg->dcam_path_cfg_u.mBits.cap_eb = 0;
		if (ISP_MODE_PREVIEW_EX == s_isp_mod.isp_mode) {
			isp_put_path2();
		}
		msleep(20);	//wait the dcam stop
		break;
	case ISP_MODE_PREVIEW:
		p_isp_reg->dcam_path_cfg_u.mBits.cap_eb = 0;
		g_is_stop = 1;
#if 0
		for (count = 0; count < 100; count++) {
			value = _pard(DCAM_INT_RAW);
			printk
			    ("###ISP DRV:ISP_DriverStop wait the last interrupt.value: 0x%x, count: %d\n",
			     value, count);
			if (value & 0x10) {
				DCAM_DRV_TRACE
				    ("DCAM DRV:ISP_DriverStop wait the last interrupt.\n");
				break;
			}
			msleep(20);
		}
#endif
		break;
	default:
		rtn = ISP_DRV_RTN_MODE_ERR;
		break;
	}

	_ISP_DriverIrqDisable(base_addr, ISP_IRQ_LINE_MASK);
	_ISP_DriverIrqClear(base_addr, ISP_IRQ_LINE_MASK);
	return rtn;
}

int32_t ISP_DriverCapConfig(uint32_t base_addr, ISP_CFG_ID_E isp_cfg_id,
			    void *param)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;
	uint32_t value;

	ISP_CHECK_PARAM_ZERO_POINTER(base_addr);

	switch (isp_cfg_id) {
	case ISP_CAP_CCIR565_ENABLE:
		ISP_CHECK_PARAM_ZERO_POINTER(param);
		p_isp_reg->cap_ctrl_u.mBits.ccir_656 =
		    *(uint32_t *) param ? 1 : 0;
		break;
	case ISP_CAP_TV_FRAME_SEL:
		rtn = ISP_DRV_RTN_IO_ID_UNSUPPORTED;
		break;
	case ISP_CAP_SYNC_POL:
		{
			ISP_CAP_SYNC_POL_T *p_sync_pol =
			    (ISP_CAP_SYNC_POL_T *) param;
			uint32_t tmp = 0;

			ISP_CHECK_PARAM_ZERO_POINTER(param);
			if (p_sync_pol->vsync_pol > 1
			    || p_sync_pol->hsync_pol > 1
			    || p_sync_pol->pclk_pol > 1) {
				rtn = ISP_DRV_RTN_CAP_SYNC_POL_ERR;
			} else {
				p_isp_reg->cap_ctrl_u.mBits.hsync_pol =
				    p_sync_pol->hsync_pol;
				p_isp_reg->cap_ctrl_u.mBits.vsync_pol =
				    p_sync_pol->vsync_pol;
				tmp = _pard(CLK_DLY_CTRL);
				tmp &= 0xfff7ffff;
				tmp |= ((p_sync_pol->pclk_pol & 0x1) << 19);
				_pawd(CLK_DLY_CTRL, tmp);
			}
			break;
		}
	case ISP_CAP_YUV_TYPE:
		{
			ISP_CAP_PATTERN_E cap_yuu_pat =
			    *(ISP_CAP_PATTERN_E *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (cap_yuu_pat < ISP_PATTERN_MAX) {
				p_isp_reg->cap_ctrl_u.mBits.yuv_type =
				    cap_yuu_pat;
			} else {
				rtn = ISP_DRV_RTN_CAP_INPUT_YUV_ERR;
			}
			break;
		}
	case ISP_CAP_FIFO_DATA_RATE:
		rtn = ISP_DRV_RTN_IO_ID_UNSUPPORTED;
		break;
	case ISP_CAP_PRE_SKIP_CNT:
		{
			uint32_t skip_num = *(uint32_t *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (skip_num < ISP_CAP_FRAME_SKIP_NUM_MAX) {
				p_isp_reg->cap_frm_ctrl_u.mBits.pre_skip_cnt =
				    skip_num;
			} else {
				rtn = ISP_DRV_RTN_CAP_SKIP_FRAME_TOO_MANY;
			}
			break;
		}
	case ISP_CAP_FRM_DECI:
		{
			uint32_t deci_factor = *(uint32_t *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (deci_factor < ISP_CAP_FRAME_DECI_FACTOR_MAX) {
				p_isp_reg->cap_frm_ctrl_u.mBits.cap_frm_deci =
				    deci_factor;
			} else {
				rtn = ISP_DRV_RTN_CAP_FRAME_DECI_FACTOR_ERR;
			}
			break;
		}
	case ISP_CAP_FRM_COUNT_CLR:
		p_isp_reg->cap_frm_ctrl_u.mBits.cap_frm_cnt = 1;
		break;
	case ISP_CAP_INPUT_RECT:
		{
			ISP_RECT_T *p_rect = (ISP_RECT_T *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (p_rect->x > (ISP_CAP_FRAME_WIDTH_MAX >> 1) ||
			    p_rect->y > ISP_CAP_FRAME_HEIGHT_MAX ||
			    p_rect->w > (ISP_CAP_FRAME_WIDTH_MAX >> 1) ||
			    p_rect->h > ISP_CAP_FRAME_HEIGHT_MAX) {
				rtn = ISP_DRV_RTN_CAP_FRAME_SIZE_ERR;
			} else {
				p_isp_reg->cap_start_u.mBits.start_x =
				    p_rect->x << 1;
				p_isp_reg->cap_start_u.mBits.start_y =
				    p_rect->y;
				p_isp_reg->cap_end_u.mBits.end_x =
				    ((p_rect->w + p_rect->x) << 1) - 1;
				p_isp_reg->cap_end_u.mBits.end_y =
				    p_rect->y + p_rect->h - 1;
			}
			break;
		}
	case ISP_CAP_IMAGE_XY_DECI:
		{
			ISP_CAP_DEC_T *p_cap_dec = (ISP_CAP_DEC_T *) param;

			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (p_cap_dec->x_factor > ISP_CAP_X_DECI_FACTOR_MAX ||
			    p_cap_dec->y_factor > ISP_CAP_Y_DECI_FACTOR_MAX) {
				rtn = ISP_DRV_RTN_CAP_XY_DECI_FACTOR_ERR;
			} else {
				p_isp_reg->cap_img_deci_u.mBits.cap_deci_x =
				    p_cap_dec->x_factor;
				p_isp_reg->cap_img_deci_u.mBits.cap_deci_y =
				    p_cap_dec->y_factor;
			}
			break;
		}
	case ISP_CAP_JPEG_DROP_NUM:
		rtn = ISP_DRV_RTN_IO_ID_UNSUPPORTED;
		break;
	case ISP_CAP_INPUT_FORMAT:
		{
			ISP_CAP_INPUT_FORMAT_E cap_input_format =
			    *(ISP_CAP_INPUT_FORMAT_E *) param;

			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (cap_input_format >= ISP_CAP_INPUT_FORMAT_MAX) {
				rtn = ISP_DRV_RTN_CAP_INPUT_FORMAT_ERR;
			} else {
				p_isp_reg->cap_ctrl_u.mBits.sensor_mode =
				    cap_input_format;
			}
			s_isp_mod.isp_cap.input_format = cap_input_format;
			break;
		}
	case ISP_CAP_JPEG_MEM_IN_16K:
		{
			uint32_t jpg_buf_size =*(uint32_t*)param;
			jpg_buf_size = jpg_buf_size/ISP_DRV_JPG_BUF_UNIT;
			p_isp_reg->cap_jpg_ctl_u.mBits.jpg_buf_size = jpg_buf_size & 0x3FF;
		}
		break;
	case ISP_CAP_IF_ENDIAN:
		value = *(uint32_t *) param;
		p_isp_reg->cap_ctrl_u.mBits.cap_if_endian = value ? 1 : 0;
		break;
	case ISP_CAP_IF_MODE:
		{
			ISP_CAP_IF_MODE_E cap_if_mode =
			    *(ISP_CAP_IF_MODE_E *) param;
			if (cap_if_mode > ISP_CAP_IF_MODE_MAX) {
				rtn = ISP_DRV_RTN_CAP_IF_MODE_ERR;
			} else {
				p_isp_reg->cap_ctrl_u.mBits.cap_if_mode =
				    cap_if_mode;
			}
			break;
		}
	case ISP_CAP_SPI_ORIG_WIDTH:
		{
			uint32_t width = (*(uint32_t *) param);

			if (width > 0x1FFFF) {
				rtn = ISP_DRV_RTN_CAP_SPI_WIDTH_ERR;
			} else {
				p_isp_reg->cap_spi_cfg_u.mBits.spi_orig_width =
				    width;
			}
			break;
		}
	case ISP_CAP_SENSOR_MODE:
		{
			ISP_CAP_SENSOR_MODE_E sensor_mode =
			    *(ISP_CAP_SENSOR_MODE_E *) param;
			if (sensor_mode > ISP_CAP_MODE_MAX) {
				rtn = ISP_DRV_RTN_CAP_SENSOR_MODE_ERR;
			} else {
				p_isp_reg->cap_ctrl_u.mBits.sensor_mode =
				    sensor_mode;
			}
			break;
		}
	default:
		rtn = ISP_DRV_RTN_IO_ID_UNSUPPORTED;
		break;
	}
	return rtn;
}

int32_t ISP_DriverCapGetInfo(uint32_t base_addr, ISP_CFG_ID_E id, void *param)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;

	ISP_CHECK_PARAM_ZERO_POINTER(base_addr);
	ISP_CHECK_PARAM_ZERO_POINTER(param);

	switch (id) {
	case ISP_CAP_FRM_COUNT_GET:
		*(uint32_t *) param =
		    p_isp_reg->cap_frm_ctrl_u.mBits.cap_frm_cnt;
		break;
	case ISP_CAP_JPEG_GET_NUM:
		*(uint32_t *) param = ISP_DRV_RTN_IO_ID_UNSUPPORTED;
		break;
	case ISP_CAP_JPEG_GET_LENGTH:
		*(uint32_t *) param =
		    p_isp_reg->cap_frm_size_u.mBits.cap_frm_size;
		break;
	default:
		rtn = ISP_DRV_RTN_IO_ID_UNSUPPORTED;
		break;
	}
	return rtn;
}

int32_t ISP_DriverPath1Config(uint32_t base_addr, ISP_CFG_ID_E id, void *param)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_PATH_DESCRIPTION_T *p_path = &s_isp_mod.isp_path1;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;
	ISP_CHECK_PARAM_ZERO_POINTER(base_addr);
	switch (id) {
	case ISP_PATH_INPUT_FORMAT:
		rtn = ISP_DRV_RTN_IO_ID_UNSUPPORTED;
		break;
	case ISP_PATH_INPUT_SIZE:
		{
			ISP_SIZE_T *p_size = (ISP_SIZE_T *) param;

			ISP_CHECK_PARAM_ZERO_POINTER(param);

			DCAM_DRV_TRACE
			    ("DCAM DRV: ISP_PATH_INPUT_SIZE w: %d, h: %d.\n",
			     p_size->w, p_size->h);
			if (p_size->w > ISP_PATH_FRAME_WIDTH_MAX
			    || p_size->h > ISP_PATH_FRAME_HEIGHT_MAX) {
				rtn = ISP_DRV_RTN_PATH_SRC_SIZE_ERR;
			} else {
				p_isp_reg->dcam_src_size_u.mBits.src_size_x =
				    p_size->w;
				p_isp_reg->dcam_src_size_u.mBits.src_size_y =
				    p_size->h;
				p_path->input_size.w = p_size->w;
				p_path->input_size.h = p_size->h;
			}
			break;
		}
	case ISP_PATH_INPUT_RECT:
		{
			ISP_RECT_T *p_rect = (ISP_RECT_T *) param;

			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (p_rect->x > ISP_PATH_FRAME_WIDTH_MAX ||
			    p_rect->y > ISP_PATH_FRAME_HEIGHT_MAX ||
			    p_rect->w > ISP_PATH_FRAME_WIDTH_MAX ||
			    p_rect->h > ISP_PATH_FRAME_HEIGHT_MAX) {
				rtn = ISP_DRV_RTN_PATH_TRIM_SIZE_ERR;
			} else {
				p_isp_reg->dcam_trim_start_u.mBits.start_x =
				    p_rect->x;
				p_isp_reg->dcam_trim_start_u.mBits.start_y =
				    p_rect->y;
				p_isp_reg->dcam_trim_size_u.mBits.size_x =
				    p_rect->w;
				p_isp_reg->dcam_trim_size_u.mBits.size_y =
				    p_rect->h;

				ISP_MEMCPY((void *)&p_path->input_rect,
					   (void *)p_rect, sizeof(ISP_RECT_T));
			}
			break;
		}
	case ISP_PATH_INPUT_ADDR:
		{
			ISP_ADDRESS_T *p_addr = (ISP_ADDRESS_T *) param;

			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (ISP_PATH_YUV_ADDR_INVALIDE
			    (p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
				rtn = ISP_DRV_RTN_PATH_ADDR_INVALIDE;
			} else {
				ISP_MEMCPY((void *)&p_path->input_frame,
					   (void *)p_addr,
					   sizeof(ISP_ADDRESS_T));
			}
			break;
		}
	case ISP_PATH_OUTPUT_SIZE:
		{
			ISP_SIZE_T *p_size = (ISP_SIZE_T *) param;

			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (p_size->w > ISP_PATH_FRAME_WIDTH_MAX ||
			    p_size->h > ISP_PATH_FRAME_HEIGHT_MAX) {
				rtn = ISP_DRV_RTN_PATH_DES_SIZE_ERR;
			} else {
				p_isp_reg->dcam_des_size_u.mBits.des_size_x =
				    p_size->w;
				p_isp_reg->dcam_des_size_u.mBits.des_size_y =
				    p_size->h;
				p_path->output_size.w = p_size->w;
				p_path->output_size.h = p_size->h;
			}
			DCAM_DRV_TRACE
			    ("DCAM DRV: output size w: %d, h: %d, reg DCAM_DES_SIZE: 0x %x.\n",
			     p_size->w, p_size->h, _pard(DCAM_DES_SIZE));
			break;
		}
	case ISP_PATH_OUTPUT_FORMAT:
		{
			uint32_t format = *(volatile uint32_t *)param;

			if ((format != ISP_DATA_YUV422)
			    && (format != ISP_DATA_YUV420)
			    && (format != ISP_DATA_RGB565)) {
				rtn = ISP_DRV_RTN_PATH_OUTPUT_FORMAT_ERR;
			} else {
				if (ISP_DATA_YUV422 == format) {
					p_isp_reg->dcam_path_cfg_u.mBits.
					    cam1_odata_format = 0;
				} else if (ISP_DATA_YUV420 == format) {
					p_isp_reg->dcam_path_cfg_u.mBits.
					    cam1_odata_format = 1;
				} else {
					p_isp_reg->dcam_path_cfg_u.mBits.
					    cam1_odata_format = 2;
				}
				p_path->output_format = format;
			}
			break;
		}
	case ISP_PATH_OUTPUT_ADDR:
		{
			ISP_ADDRESS_T *p_addr = (ISP_ADDRESS_T *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);
			if (ISP_PATH_YUV_ADDR_INVALIDE
			    (p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
				rtn = ISP_DRV_RTN_PATH_ADDR_INVALIDE;
			} else {
				if (p_path->output_frame_count >
				    ISP_PATH1_FRAME_COUNT_MAX - 1) {
					rtn = ISP_DRV_RTN_PATH_FRAME_TOO_MANY;
				} else {
					p_path->p_output_frame_cur->yaddr =
					    p_addr->yaddr;
					p_path->p_output_frame_cur->uaddr =
					    p_addr->uaddr;
					p_path->p_output_frame_cur->vaddr =
					    p_addr->vaddr;
					p_path->p_output_frame_cur =
					    p_path->p_output_frame_cur->next;
					p_path->output_frame_count++;
				}
			}
			break;
		}
	case ISP_PATH_SUB_SAMPLE_EN:
		{
			ISP_CHECK_PARAM_ZERO_POINTER(param);

			p_path->sub_sample_en = *(uint32_t *) param ? 1 : 0;
			p_isp_reg->dcam_path_cfg_u.mBits.cam1_deci_eb =
			    p_path->sub_sample_en;
			DCAM_DRV_TRACE
			    ("DCAM DRV: path1 1/2 sub_sample_en..\n ");
			break;
		}
	case ISP_PATH_SUB_SAMPLE_MOD:
		rtn = ISP_DRV_RTN_IO_ID_UNSUPPORTED;
		DCAM_DRV_TRACE
		    ("DCAM DRV:don't support ISP_PATH_SUB_SAMPLE_MOD of path1.\n");
		break;
	case ISP_PATH_UV420_AVG_EN:
		{
			uint32_t avg_eb = 0;
			ISP_CHECK_PARAM_ZERO_POINTER(param);
			avg_eb = *(volatile uint32_t *)param ? 1 : 0;
			p_isp_reg->dcam_path_cfg_u.mBits.cam1_uv420_avg_eb =
			    avg_eb;
			break;
		}
	case ISP_PATH_DITHER_EN:
		{
			uint32_t dither_eb = 0;

			ISP_CHECK_PARAM_ZERO_POINTER(param);
			dither_eb = *(volatile uint32_t *)param ? 1 : 0;
			p_isp_reg->dcam_path_cfg_u.mBits.cam1_dither_eb =
			    dither_eb;
			break;
		}
	case ISP_PATH_DATA_ENDIAN:
		{
			ISP_ENDIAN_T endian_param = { 0 };

			ISP_CHECK_PARAM_ZERO_POINTER(param);
			endian_param = *(ISP_ENDIAN_T *) param;
			p_isp_reg->endian_sel_u.mBits.dcam_output_endian_y =
			    endian_param.endian_y;
			p_isp_reg->endian_sel_u.mBits.dcam_output_endian_uv =
			    endian_param.endian_uv;
			break;
		}
	default:
		break;
	}
	return rtn;
}

int32_t ISP_DriverPath1GetInfo(uint32_t base_addr, ISP_CFG_ID_E id, void *param)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_PATH_DESCRIPTION_T *p_path = &s_isp_mod.isp_path1;

	ISP_CHECK_PARAM_ZERO_POINTER(base_addr);

	switch (id) {
	case ISP_PATH_IS_IN_SCALE_RANGE:
		{
			ISP_SIZE_T *p_size_src = (ISP_SIZE_T *) param;
			ISP_SIZE_T *p_size_dst = ++p_size_src;

			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (p_size_src->w >
			    p_size_dst->w * ISP_PATH_SC_COEFF_MAX
			    || p_size_src->w * ISP_PATH_SC_COEFF_MAX <
			    p_size_dst->w
			    || p_size_src->h >
			    p_size_dst->h * ISP_PATH_SC_COEFF_MAX
			    || p_size_src->h * ISP_PATH_SC_COEFF_MAX <
			    p_size_dst->h) {
				rtn = ISP_DRV_RTN_PATH_SC_COEFF_ERR;
			}
			break;
		}
	case ISP_PATH_IS_SCALE_EN:
		ISP_CHECK_PARAM_ZERO_POINTER(param);
		*(uint32_t *) param = p_path->scale_en;
		break;
	default:
		rtn = ISP_DRV_RTN_IO_ID_UNSUPPORTED;
		break;
	}
	return rtn;
}

int32_t ISP_DriverPath2Config(uint32_t base_addr, ISP_CFG_ID_E id, void *param)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;
	ISP_PATH_DESCRIPTION_T *p_path = &s_isp_mod.isp_path2;

	ISP_CHECK_PARAM_ZERO_POINTER(base_addr);
	switch (id) {
	case ISP_PATH_INPUT_FORMAT:
		{
			uint32_t format = *(uint32_t *) param;

			if (format > ISP_DATA_RGB888) {
				rtn = ISP_DRV_RTN_PATH_INPUT_FORMAT_ERR;
			} else {
				p_path->input_format = format;
				if (format < ISP_DATA_RGB565) {
					p_isp_reg->rev_path_cfg_u.mBits.
					    input_format =
					    ISP_PATH_DATA_FORMAT_YUV;
					p_isp_reg->rev_path_cfg_u.mBits.
					    yuv_input_format = format;
				} else {
					p_isp_reg->rev_path_cfg_u.mBits.
					    input_format =
					    ISP_PATH_DATA_FORMAT_RGB;
					p_isp_reg->rev_path_cfg_u.mBits.
					    rgb_input_format =
					    format == ISP_DATA_RGB565 ? 1 : 0;
				}
			}
			break;
		}
	case ISP_PATH_INPUT_SIZE:
		{
			ISP_SIZE_T *p_size = (ISP_SIZE_T *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (p_size->w > ISP_PATH_FRAME_WIDTH_MAX
			    || p_size->h > ISP_PATH_FRAME_HEIGHT_MAX) {
				rtn = ISP_DRV_RTN_PATH_SRC_SIZE_ERR;
			} else {
				p_isp_reg->rev_src_size_u.mBits.src_size_x =
				    p_size->w;
				p_isp_reg->rev_src_size_u.mBits.src_size_y =
				    p_size->h;
				p_path->input_size.w = p_size->w;
				p_path->input_size.h = p_size->h;
			}
			break;
		}
	case ISP_PATH_INPUT_RECT:
		{
			ISP_RECT_T *p_rect = (ISP_RECT_T *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (p_rect->x > ISP_PATH_FRAME_WIDTH_MAX
			    || p_rect->y > ISP_PATH_FRAME_HEIGHT_MAX
			    || p_rect->w > ISP_PATH_FRAME_WIDTH_MAX
			    || p_rect->h > ISP_PATH_FRAME_HEIGHT_MAX) {
				rtn = ISP_DRV_RTN_PATH_TRIM_SIZE_ERR;
			} else {
				p_isp_reg->rev_trim_start_u.mBits.start_x =
				    p_rect->x;
				p_isp_reg->rev_trim_start_u.mBits.start_y =
				    p_rect->y;
				p_isp_reg->rev_trim_size_u.mBits.size_x =
				    p_rect->w;
				p_isp_reg->rev_trim_size_u.mBits.size_y =
				    p_rect->h;
				ISP_MEMCPY((void *)&p_path->input_rect,
					   (void *)p_rect, sizeof(ISP_RECT_T));
			}
			break;
		}
	case ISP_PATH_INPUT_ADDR:
		{
			ISP_ADDRESS_T *p_addr = (ISP_ADDRESS_T *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);
			if (ISP_PATH_YUV_ADDR_INVALIDE
			    (p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
				rtn = ISP_DRV_RTN_PATH_ADDR_INVALIDE;
			} else {
				p_path->input_frame.yaddr = p_addr->yaddr;
				p_path->input_frame.uaddr = p_addr->uaddr;
				p_path->input_frame.vaddr = p_addr->vaddr;
			}
			break;
		}
	case ISP_PATH_OUTPUT_SIZE:
		{
			ISP_SIZE_T *p_size = (ISP_SIZE_T *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (p_size->w > ISP_PATH_FRAME_WIDTH_MAX
			    || p_size->h > ISP_PATH_FRAME_HEIGHT_MAX) {
				rtn = ISP_DRV_RTN_PATH_DES_SIZE_ERR;
			} else {
				p_isp_reg->rev_des_size_u.mBits.des_size_x =
				    p_size->w;
				p_isp_reg->rev_des_size_u.mBits.des_size_y =
				    p_size->h;
				p_path->output_size.w = p_size->w;
				p_path->output_size.h = p_size->h;
			}
			break;
		}
	case ISP_PATH_OUTPUT_FORMAT:
		{
			uint32_t format = *(uint32_t *) param;

			if (format != ISP_DATA_YUV422
			    && format != ISP_DATA_YUV420
			    && format != ISP_DATA_RGB565) {
				rtn = ISP_DRV_RTN_PATH_OUTPUT_FORMAT_ERR;
			} else {
				if (format == ISP_DATA_RGB565) {
					p_isp_reg->rev_path_cfg_u.mBits.
					    output_format = 2;
				} else if (format == ISP_DATA_YUV420) {
					p_isp_reg->rev_path_cfg_u.mBits.
					    output_format = 1;
				} else {
					p_isp_reg->rev_path_cfg_u.mBits.
					    output_format = 0;
				}
				p_path->output_format = format;
			}
			break;
		}
	case ISP_PATH_OUTPUT_ADDR:
		{
			ISP_ADDRESS_T *p_addr = (ISP_ADDRESS_T *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (ISP_PATH_YUV_ADDR_INVALIDE
			    (p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
				rtn = ISP_DRV_RTN_PATH_ADDR_INVALIDE;
			} else {
				if (p_path->output_frame_count >
				    ISP_PATH2_FRAME_COUNT_MAX - 1) {
					rtn = ISP_DRV_RTN_PATH_FRAME_TOO_MANY;
				} else {
					p_path->p_output_frame_cur->yaddr =
					    p_addr->yaddr;
					p_path->p_output_frame_cur->uaddr =
					    p_addr->uaddr;
					p_path->p_output_frame_cur->vaddr =
					    p_addr->vaddr;
					p_path->p_output_frame_cur =
					    p_path->p_output_frame_cur->next;
					p_path->output_frame_count++;
				}
			}
			break;
		}
	case ISP_PATH_OUTPUT_FRAME_FLAG:
		ISP_CHECK_PARAM_ZERO_POINTER(param);
		p_path->output_frame_flag = *(uint32_t *) param;
		break;
	case ISP_PATH_SUB_SAMPLE_EN:
		ISP_CHECK_PARAM_ZERO_POINTER(param);
		p_path->sub_sample_en = *(uint32_t *) param ? 1 : 0;
		p_isp_reg->rev_path_cfg_u.mBits.sub_sample_eb =
		    p_path->sub_sample_en;
		break;
	case ISP_PATH_SUB_SAMPLE_MOD:
		{
			uint32_t sub_sameple_mode = *(uint32_t *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);
			if (sub_sameple_mode > ISP_PATH_SUB_SAMPLE_MAX) {
				rtn = ISP_DRV_RTN_PATH_SUB_SAMPLE_ERR;
			} else {
				p_isp_reg->rev_path_cfg_u.mBits.sub_sample_eb =
				    sub_sameple_mode;
				p_path->sub_sample_factor = sub_sameple_mode;
			}
			break;
		}
	case ISP_PATH_SWAP_BUFF:
		{
			ISP_ADDRESS_T *p_addr = (ISP_ADDRESS_T *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);
			if (ISP_PATH_YUV_ADDR_INVALIDE
			    (p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
				rtn = ISP_DRV_RTN_PATH_ADDR_INVALIDE;
			} else {
				p_path->swap_frame.yaddr = p_addr->yaddr;
				p_path->swap_frame.uaddr = p_addr->uaddr;
				p_path->swap_frame.vaddr = p_addr->vaddr;
				p_isp_reg->frm_addr_2_u.mBits.frm_addr_2 =
				    p_addr->yaddr;
				p_isp_reg->frm_addr_3_u.mBits.frm_addr_3 =
				    p_addr->uaddr;
			}
			break;
		}
	case ISP_PATH_LINE_BUFF:
		{
			ISP_ADDRESS_T *p_addr = (ISP_ADDRESS_T *) param;
			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (ISP_PATH_YUV_ADDR_INVALIDE
			    (p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
				rtn = ISP_DRV_RTN_PATH_ADDR_INVALIDE;
			} else {
				p_path->line_frame.yaddr = p_addr->yaddr;
				p_path->line_frame.uaddr = p_addr->uaddr;
				p_path->line_frame.vaddr = p_addr->vaddr;
				p_isp_reg->frm_addr_6_u.mBits.frm_addr_6 =
				    p_addr->yaddr;
			}
			break;
		}
	case ISP_PATH_SLICE_SCALE_EN:
		p_isp_reg->rev_path_cfg_u.mBits.scale_mode = ISP_SCALE_SLICE;
		p_path->slice_en = 1;
		break;
	case ISP_PATH_SLICE_SCALE_HEIGHT:
		{
			uint32_t slice_height;
			ISP_CHECK_PARAM_ZERO_POINTER(param);
			slice_height =
			    (*(uint32_t *) param) & ISP_PATH_SLICE_MASK;
			p_isp_reg->slice_ver_cnt_u.mBits.slice_line_input =
			    slice_height;
			p_path->slice_en = 1;
			break;
		}
	case ISP_PATH_DITHER_EN:
		p_isp_reg->rev_path_cfg_u.mBits.dither_eb = 1;
		break;
	case ISP_PATH_ROT_MODE:
		{
			ISP_ROTATION_E rot = *((ISP_ROTATION_E *) param);
			if (rot >= 5) {
				rtn = ISP_DRV_RTN_ROTATION_ANGLE_ERR;
				break;
			}

			if (ISP_ROTATION_0 == rot) {
				p_isp_reg->rev_path_cfg_u.mBits.rot_eb = 0;
			} else if (ISP_ROTATION_90 == rot) {
				p_isp_reg->rev_path_cfg_u.mBits.rot_eb = 1;
				p_isp_reg->rev_path_cfg_u.mBits.rot_mode = 0;
			} else if (ISP_ROTATION_180 == rot) {
				p_isp_reg->rev_path_cfg_u.mBits.rot_eb = 1;
				p_isp_reg->rev_path_cfg_u.mBits.rot_mode = 2;
			} else if (ISP_ROTATION_270 == rot) {
				p_isp_reg->rev_path_cfg_u.mBits.rot_eb = 1;
				p_isp_reg->rev_path_cfg_u.mBits.rot_mode = 1;
			} else {
				p_isp_reg->rev_path_cfg_u.mBits.rot_eb = 1;
				p_isp_reg->rev_path_cfg_u.mBits.rot_mode = 3;
			}
			break;
		}
	default:
		rtn = ISP_DRV_RTN_IO_ID_UNSUPPORTED;
		break;
	}
	return rtn;
}

int32_t ISP_DriverPath2GetInfo(uint32_t base_addr, ISP_CFG_ID_E id, void *param)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;

	ISP_CHECK_PARAM_ZERO_POINTER(base_addr);
	switch (id) {
	case ISP_PATH_IS_IN_SCALE_RANGE:
		{
			ISP_SIZE_T *p_size_src = (ISP_SIZE_T *) param;
			ISP_SIZE_T *p_size_dst = ++p_size_src;

			ISP_CHECK_PARAM_ZERO_POINTER(param);

			if (p_size_src->w >
			    p_size_dst->w * ISP_PATH_SC_COEFF_MAX
			    || p_size_src->w * ISP_PATH_SC_COEFF_MAX <
			    p_size_dst->w
			    || p_size_src->h >
			    p_size_dst->h * ISP_PATH_SC_COEFF_MAX
			    || p_size_src->h * ISP_PATH_SC_COEFF_MAX <
			    p_size_dst->h) {
				rtn = ISP_DRV_RTN_PATH_SC_COEFF_ERR;
			}
			break;
		}
	case ISP_PATH_SLICE_OUT_HEIGHT:
		ISP_CHECK_PARAM_ZERO_POINTER(param);
		*(uint32_t *) param =
		    (uint32_t) p_isp_reg->slice_ver_cnt_u.mBits.
		    slice_line_output;
		break;
	default:
		rtn = ISP_DRV_RTN_IO_ID_UNSUPPORTED;
		break;
	}
	return rtn;
}

int32_t ISP_DriverSetMode(uint32_t base_addr, ISP_MODE_E mode)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;

	ISP_CHECK_PARAM_ZERO_POINTER(base_addr);

	switch (mode) {
	case ISP_MODE_PREVIEW:
		p_isp_reg->dcam_cfg_u.mBits.cam_path1_eb = 1;
		p_isp_reg->dcam_cfg_u.mBits.cam_path2_eb = 0;
		p_isp_reg->dcam_cfg_u.mBits.review_path_eb = 0;
		p_isp_reg->dcam_path_cfg_u.mBits.cap_mode =
		    DCAM_CAP_MODE_MULTIPLE;
		break;
	case ISP_MODE_PREVIEW_EX:
		p_isp_reg->dcam_cfg_u.mBits.cam_path1_eb = 0;
		p_isp_reg->dcam_cfg_u.mBits.cam_path2_eb = 1;
		p_isp_reg->dcam_cfg_u.mBits.review_path_eb = 0;
		p_isp_reg->dcam_path_cfg_u.mBits.cap_mode =
		    DCAM_CAP_MODE_MULTIPLE;
		break;
	case ISP_MODE_CAPTURE:
		p_isp_reg->dcam_cfg_u.mBits.cam_path1_eb = 1;
		p_isp_reg->dcam_cfg_u.mBits.cam_path2_eb = 0;
		p_isp_reg->dcam_cfg_u.mBits.review_path_eb = 0;
		p_isp_reg->dcam_path_cfg_u.mBits.cap_mode =
		    DCAM_CAP_MODE_SINGLE;
		break;
	default:
		rtn = ISP_DRV_RTN_MODE_ERR;
		break;
	}
	if (!rtn) {
		s_isp_mod.isp_mode = mode;
	}
	return rtn;
}

ISP_MODE_E ISP_DriverGetMode(void)
{
	return s_isp_mod.isp_mode;
}

int32_t ISP_DriverNoticeRegister(uint32_t base_addr,
				 ISP_IRQ_NOTICE_ID_E notice_id,
				 ISP_ISR_FUNC_PTR user_func)
{
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;

	if (notice_id >= ISP_IRQ_NOTICE_NUMBER) {
		rtn = ISP_DRV_RTN_ISR_NOTICE_ID_ERR;
	} else {
		s_isp_mod.user_func[notice_id] = user_func;
	}
	return rtn;
}

void ISP_DriverScalingCoeffReset(void)
{
	s_isp_mod.isp_path1.h_scale_coeff = ISP_PATH_SCALE_LEVEL_MAX + 1;
	s_isp_mod.isp_path1.v_scale_coeff = ISP_PATH_SCALE_LEVEL_MAX + 1;
	s_isp_mod.isp_path2.h_scale_coeff = ISP_PATH_SCALE_LEVEL_MAX + 1;
	s_isp_mod.isp_path2.v_scale_coeff = ISP_PATH_SCALE_LEVEL_MAX + 1;
}

int ISP_DriverRegisterIRQ(void)
{
	uint32_t ret = 0;
	//enable dcam interrupt bit on global level.
	_ISP_DriverEnableInt();

	if (0 !=
	    (ret =
	     request_irq(IRQ_LINE_DCAM, _ISP_DriverISR, SA_SHIRQ, "DCAM",
			 &g_share_irq)))
		DCAM_ASSERT(0);

	return ret;
}

void ISP_DriverUnRegisterIRQ(void)
{
	_ISP_DriverDisableInt();
	free_irq(IRQ_LINE_DCAM, &g_share_irq);
}

uint32_t ISP_DriverSetBufferAddress(uint32_t base_addr, uint32_t buf_addr,
				    uint32_t uv_addr)
{
	ISP_PATH_DESCRIPTION_T *p_path = 0;
	ISP_DRV_RTN_E rtn = ISP_DRV_RTN_SUCCESS;
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;

/*DCAM_TRACE("DCAM DRV:ISP_DriverSetBufferAddress:base_addr=0x%x,path_id = %d.\n",base_addr,s_isp_mod.isp_mode);*/

	if ((ISP_MODE_PREVIEW == s_isp_mod.isp_mode)
	    || ISP_MODE_CAPTURE == s_isp_mod.isp_mode) {
		p_path = &s_isp_mod.isp_path1;
		p_isp_reg->frm_addr_7_u.dwValue = buf_addr;

		if (p_path->input_format == ISP_CAP_INPUT_FORMAT_YUV) {
			if (0 == uv_addr) {
				p_isp_reg->frm_addr_8_u.dwValue =
				    buf_addr +
				    p_path->output_size.w *
				    p_path->output_size.h;
			} else {
				p_isp_reg->frm_addr_8_u.dwValue = uv_addr;
			}
		}
	} else if ((ISP_MODE_PREVIEW_EX == s_isp_mod.isp_mode)
		   || (ISP_MODE_CAPTURE_EX == s_isp_mod.isp_mode)) {
		p_path = &s_isp_mod.isp_path2;
		p_isp_reg->frm_addr_0_u.dwValue = buf_addr;

		if (p_path->input_format == ISP_CAP_INPUT_FORMAT_YUV) {
			if (0 == uv_addr) {
				p_isp_reg->frm_addr_1_u.dwValue =
				    buf_addr +
				    p_path->output_size.w *
				    p_path->output_size.h;
			} else {
				p_isp_reg->frm_addr_1_u.dwValue = uv_addr;
			}
		}
	} else {
		DCAM_TRACE_ERR
		    ("DCAM DRV:ISP_DriverSetBufferAddress ERROR:isp mode = %d .\n",
		     s_isp_mod.isp_mode);
	}
	_ISP_DriverAutoCopy(base_addr);
	return rtn;
}

void ISP_DriverPowerDown(uint32_t base_addr, uint32_t sensor_id, uint32_t value)
{
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;
	uint32_t reg_val = 0;

	reg_val = p_isp_reg->cap_ctrl_u.mBits.cap_ccir_pd;
	reg_val &= ~(1 << sensor_id);
	reg_val |= (value << sensor_id);
	p_isp_reg->cap_ctrl_u.mBits.cap_ccir_pd = reg_val;
}

void ISP_DriverReset(uint32_t base_addr, uint32_t value)
{
	ISP_REG_T *p_isp_reg = (ISP_REG_T *) base_addr;
	p_isp_reg->cap_ctrl_u.mBits.cap_ccir_rst = value;
}

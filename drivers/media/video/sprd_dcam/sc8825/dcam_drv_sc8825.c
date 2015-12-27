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
#include <linux/clk.h>
#include <linux/err.h>
#include <asm/io.h>
#include <mach/irqs.h>
#include "dcam_drv_sc8825.h"
#include "gen_scale_coef.h"

//#define DCAM_DRV_DEBUG
#define DCAM_LOWEST_ADDR                               0x800
#define DCAM_ADDR_INVALIDE(addr)                       ((addr) < DCAM_LOWEST_ADDR)
#define DCAM_YUV_ADDR_INVALIDE(y,u,v)                  \
	(DCAM_ADDR_INVALIDE(y) &&                      \
	DCAM_ADDR_INVALIDE(u) &&                       \
	DCAM_ADDR_INVALIDE(v))

#define DCAM_SC1_H_TAB_OFFSET                          0x200
#define DCAM_SC1_V_TAB_OFFSET                          0x2F0
#define DCAM_SC2_H_TAB_OFFSET                          0x400
#define DCAM_SC2_V_TAB_OFFSET                          0x4F0
#define DCAM_SC_COEFF_BUF_SIZE                         (8 << 10)
#define DCAM_SC_COEFF_COEF_SIZE                        (1 << 10)
#define DCAM_SC_COEFF_TMP_SIZE                         (6 << 10)
#define DCAM_SC_COEFF_H_NUM                            48
#define DCAM_SC_COEFF_V_NUM                            68
#define DCAM_AXI_STOP_TIMEOUT                          100
#define DCAM_CLK_DOMAIN_AHB                            1
#define DCAM_CLK_DOMAIN_DCAM                           0

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


#define DCAM_CHECK_PARAM_ZERO_POINTER(n)               \
	do {                                           \
		if (0 == (int)(n))              \
			return -DCAM_RTN_PARA_ERR;     \
	} while(0)

#define DCAM_CLEAR(a)                                  \
	do {                                           \
		memset((void *)(a), 0, sizeof(*(a)));  \
	} while(0)

#define DCAM_RTN_IF_ERR                                if(rtn) return -(rtn)
#define DCAM_IRQ_LINE_MASK                             0x00001FFFUL
#define DCAM_CLOCK_PARENT                              "clk_256m"

typedef void (*dcam_isr)(void);

enum {
	DCAM_FRM_UNLOCK = 0,
	DCAM_FRM_LOCK_WRITE = 0x10011001,
	DCAM_FRM_LOCK_READ = 0x01100110
};

enum {
	DCAM_PATH1 = 0,
	DCAM_PATH2
};

enum {
	SN_SOF = 0,
	SN_EOF,
	CAP_SOF,
	CAP_EOF,
	PATH1_DONE,
	PATH1_OV,
	SN_LINE_ERR,
	SN_FRAME_ERR,
	JPEG_BUF_OV,
	PATH2_DONE,
	PATH2_OV,
	ISP_OV,
	MIPI_OV,
	IRQ_NUMBER
};

#define DCAM_IRQ_ERR_MASK                              \
	((1 << PATH1_OV) | (1 << SN_LINE_ERR) |         \
	(1 << SN_FRAME_ERR) | (1 << PATH2_OV) |        \
	(1 << ISP_OV) | (1 << MIPI_OV))

#define DCAM_IRQ_JPEG_OV_MASK                          (1 << JPEG_BUF_OV)

struct dcam_cap_desc {
	uint32_t                   interface;
	uint32_t                   input_format;
	uint32_t                   frame_deci_factor;
	uint32_t                   img_x_deci_factor;
	uint32_t                   img_y_deci_factor;
};

struct dcam_path_desc {
	struct dcam_size           input_size;
	struct dcam_rect           input_rect;
	struct dcam_size           sc_input_size;
	struct dcam_size           output_size;
	struct dcam_frame          input_frame;
	struct dcam_frame          *output_frame_head;
	struct dcam_frame          *output_frame_cur;
	uint32_t                   output_frame_count;
	uint32_t                   output_format;
	uint32_t                   valide;
};

struct dcam_module {
	uint32_t                   dcam_mode;
	uint32_t                   module_addr;
	struct dcam_cap_desc       dcam_cap;
	struct dcam_path_desc      dcam_path1;
	struct dcam_path_desc      dcam_path2;
	dcam_isr_func              user_func[USER_IRQ_NUMBER];
	void                       *user_data[USER_IRQ_NUMBER];
};

static struct dcam_frame           s_path1_frame[DCAM_FRM_CNT_MAX];
static struct dcam_frame           s_path2_frame[DCAM_FRM_CNT_MAX];
static atomic_t                    s_dcam_users = ATOMIC_INIT(0);
static atomic_t                    s_resize_flag = ATOMIC_INIT(0);
static struct semaphore            s_done_sema = __SEMAPHORE_INITIALIZER(s_done_sema, 0);
static uint32_t                    s_resize_wait = 0;
static uint32_t                    s_path1_wait = 0;
static struct dcam_module          s_dcam_mod = {0};
static uint32_t                    g_dcam_irq = 0x5A0000A5;
static struct clk                  *s_dcam_clk = NULL;
static struct clk                  *s_ccir_clk = NULL;
static struct clk                  *s_dcam_mipi_clk = NULL;


static DEFINE_MUTEX(dcam_sem);
static DEFINE_SPINLOCK(dcam_lock);

static void    _dcam_frm_clear(void);
static void    _dcam_link_frm(uint32_t base_id);
static int32_t _dcam_path_set_next_frm(uint32_t path_index, uint32_t is_1st_frm);
static int32_t _dcam_path_trim(uint32_t path_index);
static int32_t _dcam_path_scaler(uint32_t path_index);
static int32_t _dcam_calc_sc_size(uint32_t path_index);
static int32_t _dcam_set_sc_coeff(uint32_t path_index);
static void    _dcam_force_copy(void);
static void    _dcam_auto_copy(void);
static void    _dcam_reg_trace(void);
static void    _sensor_sof(void);
static void    _sensor_eof(void);
static void    _cap_sof(void);
static void    _cap_eof(void);
static void    _path1_done(void);
static void    _path1_overflow(void);
static void    _sensor_line_err(void);
static void    _sensor_frame_err(void);
static void    _jpeg_buf_ov(void);
static void    _path2_done(void);
static void    _path2_ov(void);
static void    _isp_ov(void);
static void    _mipi_ov(void);
static irqreturn_t dcam_isr_root(int irq, void *dev_id);
static void    _dcam_wait_for_stop(void);
static void    _dcam_stopped(void);
static int32_t _dcam_mipi_clk_en(void);
static int32_t _dcam_mipi_clk_dis(void);
static int32_t _dcam_ccir_clk_en(void);
static int32_t _dcam_ccir_clk_dis(void);
extern void _dcam_isp_root(void);

static const dcam_isr isr_list[IRQ_NUMBER] = {
	_dcam_isp_root,
	_sensor_eof,
	_cap_sof,
	_cap_eof,
	_path1_done,
	_path1_overflow,
	_sensor_line_err,
	_sensor_frame_err,
	_jpeg_buf_ov,
	_path2_done,
	_path2_ov,
	_isp_ov,
	_mipi_ov
};

int32_t dcam_module_init(enum dcam_cap_if_mode if_mode,
	              enum dcam_cap_sensor_mode sn_mode)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_cap_desc    *cap_desc = &s_dcam_mod.dcam_cap;
	int                     ret = 0;

	if (if_mode >= DCAM_CAP_IF_MODE_MAX) {
		rtn = -DCAM_RTN_CAP_IF_MODE_ERR;
	} else {
		if (sn_mode >= DCAM_CAP_MODE_MAX) {
			rtn = -DCAM_RTN_CAP_SENSOR_MODE_ERR;
		} else {
			DCAM_CLEAR(&s_dcam_mod);
			_dcam_link_frm(0); /* set default base frame index as 0 */
			cap_desc->interface = if_mode;
			cap_desc->input_format = sn_mode;
			/*REG_OWR(DCAM_EB, BIT_13);//MM_EB*/
			/*REG_OWR(DCAM_MATRIX_EB, BIT_10|BIT_5);*/
			if (DCAM_CAP_IF_CSI2 == if_mode) {
			/*	REG_OWR(CSI2_DPHY_EB, MIPI_EB_BIT);*/
				ret = _dcam_mipi_clk_en();
				REG_OWR(DCAM_CFG, BIT_9);
				REG_MWR(CAP_MIPI_CTRL, BIT_2 | BIT_1, sn_mode << 1);
			} else {
				/*REG_OWR(DCAM_EB, CCIR_IN_EB_BIT);
				REG_OWR(DCAM_EB, CCIR_EB_BIT);*/
				ret = _dcam_ccir_clk_en();
				REG_MWR(DCAM_CFG, BIT_9, 0 << 9);
				REG_MWR(CAP_CCIR_CTRL, BIT_2 | BIT_1, sn_mode << 1);
			}
			rtn = DCAM_RTN_SUCCESS;
		}
	}
MODULE_INIT_END:
	return -rtn;
}

int32_t dcam_module_deinit(enum dcam_cap_if_mode if_mode,
	              enum dcam_cap_sensor_mode sn_mode)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	if (DCAM_CAP_IF_CSI2 == if_mode) {
		/*REG_MWR(CSI2_DPHY_EB, MIPI_EB_BIT, 0 << 10);*/
		REG_MWR(DCAM_CFG, BIT_9, 0 << 9);
		_dcam_mipi_clk_dis();
	} else {
		/*REG_MWR(DCAM_EB, CCIR_IN_EB_BIT, 0 << 2);
		REG_MWR(DCAM_EB, CCIR_EB_BIT, 0 << 9);*/
		REG_MWR(DCAM_CFG, BIT_9, 0 << 9);
		_dcam_ccir_clk_dis();
	}
	return -rtn;
}

int32_t dcam_module_en(void)
{
	int	ret = 0;

	DCAM_TRACE("DCAM DRV: dcam_module_en: %d \n", s_dcam_users.counter);
	if (atomic_inc_return(&s_dcam_users) == 1) {
		ret = dcam_set_clk(DCA_CLK_256M);
		/*REG_OWR(DCAM_EB, DCAM_EB_BIT);*/
		REG_OWR(DCAM_RST, DCAM_MOD_RST_BIT);
		REG_OWR(DCAM_RST, CCIR_RST_BIT);
		REG_AWR(DCAM_RST, ~DCAM_MOD_RST_BIT);
		REG_AWR(DCAM_RST, ~CCIR_RST_BIT);
	}
MODULE_EN_END:
	return ret;
}

int32_t dcam_module_dis(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	DCAM_TRACE("DCAM DRV: dcam_module_dis: %d \n", s_dcam_users.counter);
	if (atomic_dec_return(&s_dcam_users) == 0) {
		REG_AWR(DCAM_EB, ~DCAM_EB_BIT);
		dcam_set_clk(DCAM_CLK_NONE);
	}
	return -rtn;
}

int32_t dcam_reset(enum dcam_rst_mode reset_mode)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	uint32_t                time_out = 0;

	if (atomic_read(&s_dcam_users)) {
		/* firstly, stop AXI writing */
		REG_OWR(DCAM_BURST_GAP, BIT_18);
	}

	/* then wait for AHB busy cleared */
	while (++time_out < DCAM_AXI_STOP_TIMEOUT) {
		if (0 == (REG_RD(DCAM_AHBM_STS) & BIT_0))
			break;
	}
	if (time_out >= DCAM_AXI_STOP_TIMEOUT)
		return DCAM_RTN_TIMEOUT;

	/* do reset action */
	switch (reset_mode) {
	case DCAM_RST_PATH1:
		REG_OWR(DCAM_RST, PATH1_RST_BIT);
		REG_OWR(DCAM_RST, CCIR_RST_BIT);
		REG_AWR(DCAM_RST, ~PATH1_RST_BIT);
		REG_AWR(DCAM_RST, ~CCIR_RST_BIT);
/*
		REG_OWR(DCAM_RST, DCAM_MOD_RST_BIT);
		REG_OWR(DCAM_RST, CCIR_RST_BIT);
		REG_AWR(DCAM_RST, ~DCAM_MOD_RST_BIT);
		REG_AWR(DCAM_RST, ~CCIR_RST_BIT);
*/
		DCAM_TRACE("DCAM DRV: reset path1 \n");
		break;
	case DCAM_RST_PATH2:
		REG_OWR(DCAM_RST, PATH2_RST_BIT);
		REG_AWR(DCAM_RST, ~PATH2_RST_BIT);
		DCAM_TRACE("DCAM DRV: reset path2 \n");
		break;
	case DCAM_RST_ALL:
		REG_OWR(DCAM_RST, DCAM_MOD_RST_BIT);
		REG_OWR(DCAM_RST, CCIR_RST_BIT);
		REG_AWR(DCAM_RST, ~DCAM_MOD_RST_BIT);
		REG_AWR(DCAM_RST, ~CCIR_RST_BIT);
		DCAM_TRACE("DCAM DRV: reset all \n");
		break;
	default:
		rtn = DCAM_RTN_PARA_ERR;
		break;
	}

	if (atomic_read(&s_dcam_users)) {
		/* the end, enable AXI writing */
		REG_AWR(DCAM_BURST_GAP, ~BIT_18);
	}

	return -rtn;
}

int32_t dcam_set_clk(enum dcam_clk_sel clk_sel)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct clk              *clk_parent;
	char                    *parent = DCAM_CLOCK_PARENT;
	int                     ret = 0;

	switch (clk_sel) {
	case DCA_CLK_256M:
		parent = DCAM_CLOCK_PARENT;
		break;
	case DCAM_CLK_128M:
		parent = "clk_128m";
		break;
	case DCAM_CLK_48M:
		parent = "clk_48m";
		break;
	case DCAM_CLK_76M8:
		parent = "clk_76p8m";
		break;

	case DCAM_CLK_NONE:
		if (s_dcam_clk) {
			clk_disable(s_dcam_clk);
			clk_put(s_dcam_clk);
		}
		printk("DCAM close CLK %d \n", (int)clk_get_rate(s_dcam_clk));
		return 0;
	default:
		parent = "clk_128m";
		break;
	}

	if (NULL == s_dcam_clk) {
		s_dcam_clk = clk_get(NULL, "clk_dcam");
		if (IS_ERR(s_dcam_clk)) {
			printk("DCAM DRV: clk_get fail, %d \n", s_dcam_clk);
			return -1;
		} else {
			DCAM_TRACE("DCAM DRV: get clk_parent ok \n");
		}
	} else {
		clk_disable(s_dcam_clk);
	}

	clk_parent = clk_get(NULL, parent);
	if (IS_ERR(clk_parent)) {
		printk("DCAM DRV: dcam_set_clk fail, %d \n", clk_parent);
		return -1;
	} else {
		DCAM_TRACE("DCAM DRV: get clk_parent ok \n");
	}

	ret = clk_set_parent(s_dcam_clk, clk_parent);
	if(ret){
		printk("DCAM DRV: clk_set_parent fail, %d \n", ret);
	}

	ret = clk_enable(s_dcam_clk);
	if (ret) {
		printk("enable dcam clk error.\n");
		return -1;
	}
	return rtn;
}

int32_t _dcam_mipi_clk_en(void)
{
	int                     ret = 0;

	if (NULL == s_dcam_mipi_clk) {
		s_dcam_mipi_clk = clk_get(NULL, "clk_dcam_mipi");
	}

	if (IS_ERR(s_dcam_mipi_clk)) {
		printk("DCAM DRV: get dcam mipi clk error \n");
		return -1;
	} else {
		ret = clk_enable(s_dcam_mipi_clk);
		if (ret) {
			printk("DCAM DRV: enable dcam mipi clk error %d \n", ret);
			return -1;
		}
	}
	return 0;
}

int32_t _dcam_mipi_clk_dis(void)
{
	if (s_dcam_mipi_clk) {
		clk_disable(s_dcam_mipi_clk);
		clk_put(s_dcam_mipi_clk);
	}
	return 0;
}

int32_t _dcam_ccir_clk_en(void)
{
	int                     ret = 0;

	if (NULL == s_ccir_clk) {
		s_ccir_clk = clk_get(NULL, "clk_ccir");
	}

	if (IS_ERR(s_ccir_clk)) {
		printk("DCAM DRV: get dcam ccir clk error \n");
		return -1;
	} else {
		ret = clk_enable(s_ccir_clk);
		if (ret) {
			printk("DCAM DRV: enable dcam ccir clk error %d \n", ret);
			return -1;
		}
	}
	return 0;
}

int32_t _dcam_ccir_clk_dis(void)
{
	if (s_ccir_clk) {
		clk_disable(s_ccir_clk);
		clk_put(s_ccir_clk);
	}
	return 0;
}

int32_t dcam_start(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	int                     ret = 0;

	DCAM_TRACE("DCAM DRV: dcam_start %x \n", s_dcam_mod.dcam_mode);

#ifdef DCAM_DEBUG
	REG_MWR(CAP_CCIR_FRM_CTRL, BIT_5 | BIT_4, 1 << 4);
	REG_MWR(CAP_MIPI_FRM_CTRL, BIT_5 | BIT_4, 1 << 4);
#endif

	REG_WR(DCAM_INT_CLR,  DCAM_IRQ_LINE_MASK);
	REG_WR(DCAM_INT_MASK, DCAM_IRQ_LINE_MASK);
	ret = request_irq(DCAM_IRQ,
			dcam_isr_root,
			IRQF_SHARED,
			"DCAM",
			&g_dcam_irq);
	if (ret) {
		DCAM_TRACE("dcam_start, error %d \n", ret);
		return -DCAM_RTN_MAX;
	}
	if (s_dcam_mod.dcam_path1.valide) {
		rtn = _dcam_path_trim(DCAM_PATH1);
		DCAM_RTN_IF_ERR;
		rtn = _dcam_path_scaler(DCAM_PATH1);
		DCAM_RTN_IF_ERR;
		rtn = _dcam_path_set_next_frm(DCAM_PATH1, true);
		DCAM_RTN_IF_ERR;
		REG_OWR(DCAM_CFG, BIT_0);
	}

	if (s_dcam_mod.dcam_path2.valide) {
		rtn = _dcam_path_trim(DCAM_PATH2);
		DCAM_RTN_IF_ERR;
		rtn = _dcam_path_scaler(DCAM_PATH2);
		DCAM_RTN_IF_ERR;
		rtn = _dcam_path_set_next_frm(DCAM_PATH2, true);
		DCAM_RTN_IF_ERR;
		REG_OWR(DCAM_CFG, BIT_1);
		REG_OWR(DCAM_BURST_GAP, BIT_20);
	}

	_dcam_force_copy();

	_dcam_reg_trace();

	printk("DCAM S \n");

	REG_MWR(DCAM_PATH_CFG, BIT_0, 1);

	if (s_dcam_mod.dcam_path1.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH1, false);
		DCAM_RTN_IF_ERR;
	}

	if (s_dcam_mod.dcam_path2.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH2, false);
		DCAM_RTN_IF_ERR;
	}

	_dcam_auto_copy();
	return -rtn;
}

int32_t dcam_stop(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	/* CAP_EB */
	if (DCAM_CAPTURE_MODE_MULTIPLE == s_dcam_mod.dcam_mode) {
		REG_AWR(DCAM_PATH_CFG, ~BIT_0);
		_dcam_wait_for_stop();
		if (atomic_read(&s_resize_flag)) {
			s_resize_wait = 1;
			/* resize started , wait for it going to the end*/
			DCAM_TRACE("DCAM DRV: dcam_stop, wait: %d \n", s_done_sema.count);
			down_interruptible(&s_done_sema);
		}
	}

	free_irq(DCAM_IRQ, &g_dcam_irq);

	DCAM_TRACE("DCAM DRV: dcam_stop, exit from wait: %d \n", s_done_sema.count);

	/* PATH1_EB */
	REG_AWR(DCAM_CFG, ~BIT_0);
	if (s_dcam_mod.dcam_path2.valide) {
		/* PATH2_EB */
		REG_AWR(DCAM_CFG, ~BIT_1);
	}
	/* reset all*/
	dcam_reset(DCAM_RST_ALL);

#if 0
	if (s_dcam_mod.dcam_path1.valide) {

		/* PATH1_EB */
		REG_AWR(DCAM_CFG, ~BIT_0);
		/* path1 reset */
		dcam_reset(DCAM_RST_PATH1);
	}

	if (s_dcam_mod.dcam_path2.valide) {
		/* PATH2_EB */
		REG_AWR(DCAM_CFG, ~BIT_1);
		/* path2 reset */
		dcam_reset(DCAM_RST_PATH2);
	}
#endif
	_dcam_frm_clear();
	printk("DCAM E \n");
	return -rtn;
}

int32_t dcam_resume(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	_dcam_frm_clear();

	if (s_dcam_mod.dcam_path1.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH1, true);
		DCAM_RTN_IF_ERR;
	}

	if (s_dcam_mod.dcam_path2.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH2, true);
		DCAM_RTN_IF_ERR;
	}

	_dcam_force_copy();

	if (s_dcam_mod.dcam_path1.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH1, false);
		DCAM_RTN_IF_ERR;
		REG_OWR(DCAM_CFG, BIT_0);
	}

	if (s_dcam_mod.dcam_path2.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH2, false);
		DCAM_RTN_IF_ERR;
		REG_OWR(DCAM_CFG, BIT_1);
	}

	_dcam_auto_copy();

	printk("DCAM R \n");

	REG_OWR(DCAM_PATH_CFG, BIT_0);
	return -rtn;
}

int32_t dcam_pause(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	REG_AWR(DCAM_PATH_CFG, ~BIT_0);
	printk("DCAM P \n");
	return -rtn;
}

int32_t dcam_reg_isr(enum dcam_irq_id id, dcam_isr_func user_func, void* user_data)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	uint32_t                flag;

	if(id >= USER_IRQ_NUMBER) {
		rtn = DCAM_RTN_ISR_ID_ERR;
	} else {
		spin_lock_irqsave(&dcam_lock, flag);
		s_dcam_mod.user_func[id] = user_func;
		s_dcam_mod.user_data[id] = user_data;
		spin_unlock_irqrestore(&dcam_lock, flag);
	}
	return -rtn;
}

int32_t dcam_cap_cfg(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_cap_desc    *cap_desc = &s_dcam_mod.dcam_cap;

	switch (id) {
	case DCAM_CAP_SYNC_POL:
	{
		struct dcam_cap_sync_pol *sync_pol = (struct dcam_cap_sync_pol*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (DCAM_CAP_IF_CCIR == cap_desc->interface) {

			if (sync_pol->vsync_pol > 1 ||
			    sync_pol->hsync_pol > 1 ||
			    sync_pol->pclk_pol > 1) {
				rtn = DCAM_RTN_CAP_SYNC_POL_ERR;
			} else {
				REG_MWR(CAP_CCIR_CTRL, BIT_3, sync_pol->hsync_pol << 3);
				REG_MWR(CAP_CCIR_CTRL, BIT_4, sync_pol->vsync_pol << 4);
				REG_MWR(CLK_DLY_CTRL,  BIT_19, sync_pol->pclk_pol << 19);
			}
		} else {
			if (sync_pol->need_href) {
				REG_MWR(CAP_MIPI_CTRL, BIT_5, 1 << 5);
			} else {
				REG_MWR(CAP_MIPI_CTRL, BIT_5, 0 << 5);
			}
		}
		break;
	}

	case DCAM_CAP_DATA_BITS:
	{
		enum dcam_cap_data_bits bits = *(enum dcam_cap_data_bits*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (DCAM_CAP_IF_CCIR == cap_desc->interface) {
			if (DCAM_CAP_8_BITS == bits || DCAM_CAP_10_BITS == bits) {
				REG_MWR(CAP_CCIR_CTRL,  BIT_10 | BIT_9, 0 << 9);
			} else if (DCAM_CAP_4_BITS == bits) {
				REG_MWR(CAP_CCIR_CTRL,  BIT_10 | BIT_9, 1 << 9);
			} else if (DCAM_CAP_2_BITS == bits) {
				REG_MWR(CAP_CCIR_CTRL,  BIT_10 | BIT_9, 2 << 9);
			} else if (DCAM_CAP_1_BITS == bits) {
				REG_MWR(CAP_CCIR_CTRL,  BIT_10 | BIT_9, 3 << 9);
			} else {
				rtn = DCAM_RTN_CAP_IN_BITS_ERR;
			}

		} else {
			if (DCAM_CAP_12_BITS == bits) {
				REG_MWR(CAP_MIPI_CTRL,  BIT_4 | BIT_3, 2 << 3);
			} else if (DCAM_CAP_10_BITS == bits) {
				REG_MWR(CAP_MIPI_CTRL,  BIT_4 | BIT_3, 1 << 3);
			} else if (DCAM_CAP_8_BITS == bits) {
				REG_MWR(CAP_MIPI_CTRL,  BIT_4 | BIT_3, 0 << 3);
			} else {
				rtn = DCAM_RTN_CAP_IN_BITS_ERR;
			}
		}
		break;
	}

	case DCAM_CAP_YUV_TYPE:
	{
		enum dcam_cap_pattern pat = *(enum dcam_cap_pattern*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (pat < DCAM_PATTERN_MAX) {
			if (DCAM_CAP_IF_CCIR == cap_desc->interface)
				REG_MWR(CAP_CCIR_CTRL, BIT_8 | BIT_7, pat << 7);
			else
				REG_MWR(CAP_MIPI_CTRL, BIT_8 | BIT_7, pat << 7);
		} else {
			rtn = DCAM_RTN_CAP_IN_YUV_ERR;
		}
		break;
	}

	case DCAM_CAP_PRE_SKIP_CNT:
	{
		uint32_t skip_num = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (skip_num > DCAM_CAP_SKIP_FRM_MAX) {
			rtn = DCAM_RTN_CAP_SKIP_FRAME_ERR;
		} else {
			if (DCAM_CAP_IF_CCIR == cap_desc->interface)
				REG_MWR(CAP_CCIR_FRM_CTRL, BIT_3 | BIT_2 | BIT_1 | BIT_0, skip_num);
			else
				REG_MWR(CAP_MIPI_FRM_CTRL, BIT_3 | BIT_2 | BIT_1 | BIT_0, skip_num);
		}
		break;
	}

	case DCAM_CAP_FRM_DECI:
	{
		uint32_t deci_factor = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (deci_factor < DCAM_FRM_DECI_FAC_MAX) {
			if (DCAM_CAP_IF_CCIR == cap_desc->interface)
				REG_MWR(CAP_CCIR_FRM_CTRL, BIT_5 | BIT_4, deci_factor << 4);
			else
				REG_MWR(CAP_MIPI_FRM_CTRL, BIT_5 | BIT_4, deci_factor << 4);
		} else {
			rtn = DCAM_RTN_CAP_FRAME_DECI_ERR;
		}
		break;
	}

	case DCAM_CAP_FRM_COUNT_CLR:
		if (DCAM_CAP_IF_CCIR == cap_desc->interface)
			REG_MWR(CAP_CCIR_FRM_CTRL, BIT_22, 1 << 22);
		else
			REG_MWR(CAP_MIPI_FRM_CTRL, BIT_22, 1 << 22);
		break;

	case DCAM_CAP_INPUT_RECT:
	{
		struct dcam_rect *rect = (struct dcam_rect*)param;
		uint32_t         tmp = 0;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);
		if (rect->x > DCAM_CAP_FRAME_WIDTH_MAX ||
		rect->y > DCAM_CAP_FRAME_HEIGHT_MAX ||
		rect->w > DCAM_CAP_FRAME_WIDTH_MAX ||
		rect->h > DCAM_CAP_FRAME_HEIGHT_MAX ) {
			rtn = DCAM_RTN_CAP_FRAME_SIZE_ERR;
			return -rtn;
		}

		if (DCAM_CAP_IF_CCIR == cap_desc->interface) {
			if (DCAM_CAP_MODE_RAWRGB == cap_desc->input_format) {
				tmp = rect->x | (rect->y << 16);
				REG_WR(CAP_CCIR_START, tmp);
				tmp = (rect->x + rect->w - 1);
				tmp |= (rect->y + rect->h - 1) << 16;
				REG_WR(CAP_CCIR_END, tmp);
			} else {
				tmp = (rect->x << 1) | (rect->y << 16);
				REG_WR(CAP_CCIR_START, tmp);
				tmp = ((rect->x + rect->w) << 1) - 1;
				tmp |= (rect->y + rect->h - 1) << 16;
				REG_WR(CAP_CCIR_END, tmp);
			}
		} else {
			tmp = rect->x | (rect->y << 16);
			REG_WR(CAP_MIPI_START, tmp);
			tmp = (rect->x + rect->w - 1);
			tmp |= (rect->y + rect->h - 1) << 16;
			REG_WR(CAP_MIPI_END, tmp);
		}
		break;
	}

	case DCAM_CAP_IMAGE_XY_DECI:
	{
		struct dcam_cap_dec *cap_dec = (struct dcam_cap_dec*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (cap_dec->x_factor > DCAM_CAP_X_DECI_FAC_MAX ||
		cap_dec->y_factor > DCAM_CAP_Y_DECI_FAC_MAX ) {
			rtn = DCAM_RTN_CAP_XY_DECI_ERR;
		} else {
			if (DCAM_CAP_MODE_RAWRGB == cap_desc->input_format) {
				if (cap_dec->x_factor > 1 ||
				    cap_dec->y_factor > 1) {
					rtn = DCAM_RTN_CAP_XY_DECI_ERR;
				}
			}
			if (DCAM_CAP_IF_CCIR == cap_desc->interface) {
				REG_MWR(CAP_CCIR_IMG_DECI, BIT_1 | BIT_0, cap_dec->x_factor);
				REG_MWR(CAP_CCIR_IMG_DECI, BIT_3 | BIT_2, cap_dec->y_factor << 2);
			} else {
				if (DCAM_CAP_MODE_RAWRGB == cap_desc->input_format) {
					// REG_MWR(CAP_MIPI_IMG_DECI, BIT_0, cap_dec->x_factor); // for camera path
					REG_MWR(CAP_MIPI_IMG_DECI, BIT_1, cap_dec->x_factor << 1);//for ISP
				} else {
					REG_MWR(CAP_MIPI_IMG_DECI, BIT_1 | BIT_0, cap_dec->x_factor);
					REG_MWR(CAP_MIPI_IMG_DECI, BIT_3 | BIT_2, cap_dec->y_factor << 2);
				}
			}
		}

		break;
	}

	case DCAM_CAP_JPEG_SET_BUF_LEN:
	{
		uint32_t jpg_buf_size = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);
		jpg_buf_size = jpg_buf_size/DCAM_JPG_BUF_UNIT;
		if (jpg_buf_size >= DCAM_JPG_UNITS) {
			rtn = DCAM_RTN_CAP_JPEG_BUF_LEN_ERR;
		} else {
			if (DCAM_CAP_IF_CCIR == cap_desc->interface)
				REG_WR(CAP_CCIR_JPG_CTRL,jpg_buf_size);
			else
				REG_WR(CAP_MIPI_JPG_CTRL,jpg_buf_size);
		}
		break;
	}

	case DCAM_CAP_TO_ISP:
	{
		uint32_t need_isp = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (need_isp) {
			REG_MWR(DCAM_CFG, BIT_7, 1 << 7);
		} else {
			REG_MWR(DCAM_CFG, BIT_7, 0 << 7);
		}
		break;
	}

	case DCAM_CAP_DATA_PACKET:
	{
		uint32_t is_loose = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (DCAM_CAP_IF_CSI2 == cap_desc->interface &&
			DCAM_CAP_MODE_RAWRGB == cap_desc->input_format) {
			if (is_loose) {
				REG_MWR(CAP_MIPI_CTRL, BIT_0, 1);
			} else {
				REG_MWR(CAP_MIPI_CTRL, BIT_0, 0);
			}
		} else {
			rtn = DCAM_RTN_MODE_ERR;
		}

		break;
	}

	case DCAM_CAP_SAMPLE_MODE:
	{
		enum dcam_capture_mode samp_mode = *(enum dcam_capture_mode*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (samp_mode >= DCAM_CAPTURE_MODE_MAX) {
			rtn = DCAM_RTN_MODE_ERR;
		} else {
			if (DCAM_CAP_IF_CSI2 == cap_desc->interface) {
				REG_MWR(CAP_MIPI_CTRL, BIT_6, samp_mode << 6);
			} else {
				REG_MWR(CAP_CCIR_CTRL, BIT_6, samp_mode << 6);
			}
		}
		break;
	}
	default:
		rtn = DCAM_RTN_IO_ID_ERR;
		break;

	}

	return -rtn;
}

int32_t dcam_cap_get_info(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_cap_desc    *cap_desc = &s_dcam_mod.dcam_cap;

	DCAM_CHECK_PARAM_ZERO_POINTER(param);

	switch(id) {
		case DCAM_CAP_FRM_COUNT_GET:
			if (DCAM_CAP_IF_CSI2 == cap_desc->interface) {
				*(uint32_t*)param = REG_RD(CAP_MIPI_FRM_CTRL) >> 16;
			} else {
				*(uint32_t*)param = REG_RD(CAP_CCIR_FRM_CTRL) >> 16;
			}
			break;

		case DCAM_CAP_JPEG_GET_LENGTH:
			if (DCAM_CAP_IF_CSI2 == cap_desc->interface) {
				*(uint32_t*)param = REG_RD(CAP_MIPI_FRM_SIZE);
			} else {
				*(uint32_t*)param = REG_RD(CAP_CCIR_FRM_SIZE);
			}
			break;
		default:
			rtn = DCAM_RTN_IO_ID_ERR;
			break;
	}
	return -rtn;
}

int32_t dcam_path1_cfg(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = &s_dcam_mod.dcam_path1;

	switch (id) {

	case DCAM_PATH_INPUT_SIZE:
	{
		struct dcam_size *size = (struct dcam_size*)param;
		uint32_t         reg_val = 0;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM DRV: DCAM_PATH1_INPUT_SIZE {%d %d} \n", size->w, size->h);
		if (size->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		size->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
		} else {
			reg_val = size->w | (size->h << 16);
			REG_WR(DCAM_SRC_SIZE, reg_val);
			path->input_size.w = size->w;
			path->input_size.h = size->h;
		}
		break;
	}

	case DCAM_PATH_INPUT_RECT:
	{
		struct dcam_rect *rect = (struct dcam_rect*)param;
		uint32_t         reg_val = 0;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM DRV: DCAM_PATH1_INPUT_RECT {%d %d %d %d} \n",
			rect->x,
			rect->y,
			rect->w,
			rect->h);

		if (rect->x > DCAM_PATH_FRAME_WIDTH_MAX ||
		rect->y > DCAM_PATH_FRAME_HEIGHT_MAX ||
		rect->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		rect->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_TRIM_SIZE_ERR;
		} else {
			reg_val = rect->x | (rect->y << 16);
			REG_WR(DCAM_TRIM_START, reg_val);
			reg_val = rect->w | (rect->h << 16);
			REG_WR(DCAM_TRIM_SIZE, reg_val);
			memcpy((void*)&path->input_rect,
				(void*)rect,
				sizeof(struct dcam_rect));
		}
		break;
	}

	case DCAM_PATH_OUTPUT_SIZE:
	{
		struct dcam_size *size = (struct dcam_size*)param;
		uint32_t         reg_val = 0;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM DRV: DCAM_PATH1_OUTPUT_SIZE {%d %d} \n", size->w, size->h);
		if (size->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		size->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
		} else {
			reg_val = size->w | (size->h << 16);
			REG_WR(DCAM_DST_SIZE, reg_val);
			path->output_size.w = size->w;
			path->output_size.h = size->h;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_FORMAT:
	{
		enum dcam_fmt format = *(enum dcam_fmt*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		path->output_format = format;
		if (DCAM_YUV422 == format) {
			REG_MWR(DCAM_PATH_CFG, BIT_7 | BIT_6 | BIT_5, 0 << 5);
		} else if (DCAM_YUV420 == format) {
			REG_MWR(DCAM_PATH_CFG, BIT_7 | BIT_6 | BIT_5, 1 << 5);
		} else if (DCAM_RGB565 == format) {
			REG_MWR(DCAM_PATH_CFG, BIT_7 | BIT_6 | BIT_5, 2 << 5);
		} else if (DCAM_YUV420_3FRAME == format) {
			REG_MWR(DCAM_PATH_CFG, BIT_7 | BIT_6 | BIT_5, 3 << 5);
		} else if (DCAM_JPEG == format) {
			REG_MWR(DCAM_PATH_CFG, BIT_7 | BIT_6 | BIT_5, 4 << 5);
		} else if (DCAM_RAWRGB == format) {
			REG_MWR(DCAM_PATH_CFG, BIT_7 | BIT_6 | BIT_5, 5 << 5);
		} else {
			rtn = DCAM_RTN_PATH_OUT_FMT_ERR;
			path->output_format = DCAM_FTM_MAX;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_ADDR:
	{
		struct dcam_addr *p_addr = (struct dcam_addr*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (DCAM_YUV_ADDR_INVALIDE(p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else {
			if (path->output_frame_count > DCAM_FRM_CNT_MAX - 1) {
				rtn = DCAM_RTN_PATH_FRAME_TOO_MANY;
			} else {
				path->output_frame_cur->yaddr = p_addr->yaddr;
				path->output_frame_cur->uaddr = p_addr->uaddr;
				path->output_frame_cur->vaddr = p_addr->vaddr;
				path->output_frame_cur = path->output_frame_cur->next;
				path->output_frame_count ++;
			}
		}
		break;
	}

	case DCAM_PATH_SRC_SEL:
	{
		uint32_t       src_sel = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (src_sel >= DCAM_PATH_FROM_NONE) {
			rtn = DCAM_RTN_PATH_SRC_ERR;
		} else {
			REG_MWR(DCAM_PATH_CFG, BIT_4, src_sel << 4);
		}
		break;
	}

	case DCAM_PATH_FRAME_BASE_ID:
	{
		struct dcam_frame *frame  = path->output_frame_head;
		uint32_t          base_id = *(uint32_t*)param, i;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM DRV: DCAM_PATH_FRAME_BASE_ID 0x%x \n", base_id);
		for (i = 0; i < DCAM_FRM_CNT_MAX; i++) {
			frame->fid = base_id + i;
			frame = frame->next;
		}
		break;
	}

	case DCAM_PATH_DATA_ENDIAN:
	{
		struct dcam_endian_sel *endian = (struct dcam_endian_sel*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (endian->y_endian >= DCAM_ENDIAN_MAX ||
			endian->uv_endian >= DCAM_ENDIAN_MAX) {
			rtn = DCAM_RTN_PATH_ENDIAN_ERR;
		} else {
			REG_MWR(DCAM_ENDIAN_SEL, BIT_9 | BIT_8, endian->y_endian << 8);
			REG_MWR(DCAM_ENDIAN_SEL, BIT_11 | BIT_10, endian->uv_endian << 10);
		}
		break;
	}

	case DCAM_PATH_ENABLE:
	{
		DCAM_CHECK_PARAM_ZERO_POINTER(param);
		path->valide = *(uint32_t*)param;
		break;
	}

	case DCAM_PATH_FRAME_TYPE:
	{
		struct dcam_frame *frame  = path->output_frame_head;
		uint32_t          frm_type = *(uint32_t*)param, i;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM DRV: DCAM_PATH_FRAME_TYPE 0x%x \n", frm_type);
		for (i = 0; i < DCAM_FRM_CNT_MAX; i++) {
			frame->type = frm_type;
			frame = frame->next;
		}
		break;
	}

	default:
		break;
	}

	return -rtn;
}

int32_t dcam_path2_cfg(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = &s_dcam_mod.dcam_path2;

	switch (id) {

	case DCAM_PATH_INPUT_SIZE:
	{
		struct dcam_size *size = (struct dcam_size*)param;
		uint32_t         reg_val = 0;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM DRV: DCAM_PATH2_INPUT_SIZE {%d %d} \n", size->w, size->h);
		if (size->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		size->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
		} else {
			reg_val = size->w | (size->h << 16);
			REG_WR(REV_SRC_SIZE, reg_val);
			path->input_size.w = size->w;
			path->input_size.h = size->h;
		}
		break;
	}

	case DCAM_PATH_INPUT_RECT:
	{
		struct dcam_rect *rect = (struct dcam_rect*)param;
		uint32_t         reg_val = 0;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM DRV: DCAM_PATH2_INPUT_RECT {%d %d %d %d} \n",
			rect->x,
			rect->y,
			rect->w,
			rect->h);

		if (rect->x > DCAM_PATH_FRAME_WIDTH_MAX ||
		rect->y > DCAM_PATH_FRAME_HEIGHT_MAX ||
		rect->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		rect->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_TRIM_SIZE_ERR;
		} else {
			reg_val = rect->x | (rect->y << 16);
			REG_WR(REV_TRIM_START, reg_val);
			reg_val = rect->w | (rect->h << 16);
			REG_WR(REV_TRIM_SIZE, reg_val);
			memcpy((void*)&path->input_rect,
				(void*)rect,
				sizeof(struct dcam_rect));
		}
		break;
	}

	case DCAM_PATH_OUTPUT_SIZE:
	{
		struct dcam_size *size = (struct dcam_size*)param;
		uint32_t         reg_val = 0;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM DRV: DCAM_PATH2_OUTPUT_SIZE {%d %d} \n", size->w, size->h);
		if (size->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		size->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
		} else {
			reg_val = size->w | (size->h << 16);
			REG_WR(REV_DST_SIZE, reg_val);
			path->output_size.w = size->w;
			path->output_size.h = size->h;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_FORMAT:
	{
		enum dcam_fmt format = *(enum dcam_fmt*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		path->output_format = format;
		REG_MWR(REV_PATH_CFG, BIT_8, 0 << 8);
		if (DCAM_YUV422 == format) {
			REG_MWR(REV_PATH_CFG, BIT_7 | BIT_6, 0 << 6);
		} else if (DCAM_YUV420 == format) {
			REG_MWR(REV_PATH_CFG, BIT_7 | BIT_6, 1 << 6);
		} else if (DCAM_RGB565 == format) {
			REG_MWR(REV_PATH_CFG, BIT_8, 1 << 8);
			REG_MWR(REV_PATH_CFG, BIT_7 | BIT_6, 2 << 6);
		} else if (DCAM_YUV420_3FRAME == format) {
			REG_MWR(REV_PATH_CFG, BIT_7 | BIT_6, 3 << 6);
		} else {
			rtn = DCAM_RTN_PATH_OUT_FMT_ERR;
			path->output_format = DCAM_FTM_MAX;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_ADDR:
	{
		struct dcam_addr *p_addr = (struct dcam_addr*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (DCAM_YUV_ADDR_INVALIDE(p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else {
			if (path->output_frame_count > DCAM_FRM_CNT_MAX - 1) {
				rtn = DCAM_RTN_PATH_FRAME_TOO_MANY;
			} else {
				path->output_frame_cur->yaddr = p_addr->yaddr;
				path->output_frame_cur->uaddr = p_addr->uaddr;
				path->output_frame_cur->vaddr = p_addr->vaddr;
				path->output_frame_cur = path->output_frame_cur->next;
				path->output_frame_count ++;
			}
		}
		break;
	}
	case DCAM_PATH_SRC_SEL:
	{
		uint32_t       src_sel = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (src_sel >= DCAM_PATH_FROM_NONE) {
			rtn = DCAM_RTN_PATH_SRC_ERR;
		} else {
			REG_MWR(REV_PATH_CFG, BIT_4, src_sel << 4);
		}
		break;
	}

	case DCAM_PATH_FRAME_BASE_ID:
	{
		struct dcam_frame *frame  = path->output_frame_head;
		uint32_t          base_id = *(uint32_t*)param, i;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM DRV: DCAM_PATH_FRAME_BASE_ID 0x%x \n", base_id);
		for (i = 0; i < DCAM_FRM_CNT_MAX; i++) {
			frame->fid = base_id + i;
			frame = frame->next;
		}
		break;
	}

	case DCAM_PATH_DATA_ENDIAN:
	{
		struct dcam_endian_sel *endian = (struct dcam_endian_sel*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (endian->y_endian >= DCAM_ENDIAN_MAX ||
			endian->uv_endian >= DCAM_ENDIAN_MAX) {
			rtn = DCAM_RTN_PATH_ENDIAN_ERR;
		} else {
			REG_MWR(DCAM_ENDIAN_SEL, BIT_5 | BIT_4, endian->y_endian << 4);
			REG_MWR(DCAM_ENDIAN_SEL, BIT_7 | BIT_6, endian->uv_endian << 6);
		}
		break;
	}

	case DCAM_PATH_ENABLE:
	{
		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		path->valide = *(uint32_t*)param;

		break;
	}

	case DCAM_PATH_FRAME_TYPE:
	{
		struct dcam_frame *frame  = path->output_frame_head;
		uint32_t          frm_type = *(uint32_t*)param, i;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM DRV: DCAM_PATH_FRAME_TYPE 0x%x \n", frm_type);
		for (i = 0; i < DCAM_FRM_CNT_MAX; i++) {
			frame->type = frm_type;
			frame = frame->next;
		}
		break;
	}

	default:
		break;
	}

	return -rtn;
}

int32_t    dcam_get_resizer(uint32_t wait_opt)
{
	int32_t                 rtn = 0;

	if( 0 == wait_opt) {
		rtn = mutex_trylock(&dcam_sem) ? 0 : 1;
		return rtn;
	} else if (DCAM_WAIT_FOREVER == wait_opt){
		mutex_lock(&dcam_sem);
		return rtn;
	} else {
		return 0;//mutex_timeout(&dcam_sem, wait_opt);
	}
}

int32_t    dcam_rel_resizer(void)
{
	 mutex_unlock(&dcam_sem);
	 return 0;
}

int32_t    dcam_resize_start(void)
{
	atomic_inc(&s_resize_flag);
	return 0;
}

int32_t    dcam_resize_end(void)
{
	atomic_dec(&s_resize_flag);
	if (s_resize_wait) {
		up(&s_done_sema);
		s_resize_wait = 0;
	}
	return 0;
}

void dcam_int_en(void)
{
	if (atomic_read(&s_dcam_users) == 1) {
		enable_irq(DCAM_IRQ);
	}
}

void dcam_int_dis(void)
{
	if (atomic_read(&s_dcam_users) == 1) {
		disable_irq(DCAM_IRQ);
	}
}

int32_t dcam_frame_is_locked(struct dcam_frame *frame)
{
	uint32_t                rtn = 0;
	uint32_t                flags;

	/*To disable irq*/
	local_irq_save(flags);
	if (frame)
		rtn = frame->lock == DCAM_FRM_LOCK_WRITE ? 1 : 0;
	local_irq_restore(flags);
	/*To enable irq*/

	return -rtn;
}

int32_t dcam_frame_lock(struct dcam_frame *frame)
{
	uint32_t                rtn = 0;
	uint32_t                flags;

	DCAM_TRACE("DCAM DRV: dcam_frame_lock 0x%x \n", (uint32_t)frame);

	/*To disable irq*/
	local_irq_save(flags);
	if (likely(frame))
		frame->lock = DCAM_FRM_LOCK_WRITE;
	else
		rtn = DCAM_RTN_PARA_ERR;
	local_irq_restore(flags);
	/*To enable irq*/

	return -rtn;
}

int32_t dcam_frame_unlock(struct dcam_frame *frame)
{
	uint32_t                rtn = 0;
	uint32_t                flags;

	DCAM_TRACE("DCAM DRV: dcam_frame_unlock 0x%x \n", (uint32_t)frame);

	/*To disable irq*/
	local_irq_save(flags);
	if (likely(frame))
		frame->lock = DCAM_FRM_UNLOCK;
	else
		rtn = DCAM_RTN_PARA_ERR;
	local_irq_restore(flags);
	/*To enable irq*/

	return -rtn;
}

int32_t    dcam_read_registers(uint32_t* reg_buf, uint32_t *buf_len)
{
	uint32_t*               *reg_addr = (uint32_t*)DCAM_BASE;

	if (NULL == reg_buf || NULL == buf_len || 0 != (*buf_len % 4)) {
		return -1;
	}

	while (buf_len != 0 && (uint32_t)reg_addr < DCAM_END) {
		*reg_buf++ = REG_RD(reg_addr);
		reg_addr++;
		*buf_len -= 4;
	}

	*buf_len = (uint32_t)reg_addr - DCAM_BASE;
	return 0;
}

static irqreturn_t dcam_isr_root(int irq, void *dev_id)
{
	uint32_t                status, i, irq_line, err_flag = 0, flag;
	void                    *data;

	status = REG_RD(DCAM_INT_STS);
	if (unlikely(0 == status)) {
		return IRQ_NONE;
	}
	irq_line = status;
	if (unlikely(DCAM_IRQ_ERR_MASK & status)) {
		err_flag = 1;
		printk("DCAM DRV: error happened, 0x%x, %d \n", status, s_dcam_mod.dcam_path2.valide);
		_dcam_stopped();
		if (s_dcam_mod.dcam_path2.valide) {
			/* both dcam paths have been working, it's safe to reset the whole dcam module*/
			dcam_reset(DCAM_RST_ALL);
		} else {
			dcam_reset(DCAM_RST_PATH1);
		}
	}

	if (0 == s_dcam_mod.dcam_path2.valide) {
		status &= ~((1 << PATH2_DONE) | (1 << PATH2_OV));
		irq_line = status;
	}

	spin_lock_irqsave(&dcam_lock,flag);

	if (err_flag && s_dcam_mod.user_func[DCAM_TX_ERR]) {
		data = s_dcam_mod.user_data[DCAM_TX_ERR];
		s_dcam_mod.user_func[DCAM_TX_ERR](NULL, data);
	} else if ((DCAM_IRQ_JPEG_OV_MASK & status) && s_dcam_mod.user_func[DCAM_NO_MEM]) {
		data = s_dcam_mod.user_data[DCAM_NO_MEM];
		s_dcam_mod.user_func[DCAM_NO_MEM](NULL, data);
	} else {
		for (i = IRQ_NUMBER - 1; i >= 0; i--) {
			if (irq_line & (1 << (uint32_t)i)) {
				isr_list[i]();
			}
			irq_line &= ~(uint32_t)(1 << (uint32_t)i); //clear the interrupt flag
			if(!irq_line) //no interrupt source left
				break;
		}
	}

	REG_WR(DCAM_INT_CLR, status);

	spin_unlock_irqrestore(&dcam_lock, flag);

	return IRQ_HANDLED;
}
static void _dcam_frm_clear(void)
{
	uint32_t                i = 0;
	struct dcam_frame       *path1_frame = &s_path1_frame[0];
	struct dcam_frame       *path2_frame = &s_path2_frame[0];

	for (i = 0; i < DCAM_FRM_CNT_MAX; i++) {
		(path1_frame+i)->lock = DCAM_FRM_UNLOCK;
	}

	for (i = 0; i < DCAM_FRM_CNT_MAX; i++) {
		(path2_frame+i)->lock = DCAM_FRM_UNLOCK;
	}

	s_dcam_mod.dcam_path1.output_frame_head = path1_frame;
	s_dcam_mod.dcam_path2.output_frame_head = path2_frame;
	s_dcam_mod.dcam_path1.output_frame_cur = path1_frame;
	s_dcam_mod.dcam_path2.output_frame_cur = path2_frame;

	return;
}

static void _dcam_link_frm(uint32_t base_id)
{
	uint32_t                i = 0;
	struct dcam_frame       *path1_frame = &s_path1_frame[0];
	struct dcam_frame       *path2_frame = &s_path2_frame[0];

	for (i = 0; i < DCAM_FRM_CNT_MAX; i++) {
		DCAM_CLEAR(path1_frame + i);
		(path1_frame+i)->next = path1_frame + (i + 1) % DCAM_FRM_CNT_MAX;
		(path1_frame+i)->prev = path1_frame + (i - 1 + DCAM_FRM_CNT_MAX) % DCAM_FRM_CNT_MAX;
		(path1_frame+i)->fid  = base_id + i;
	}

	for (i = 0; i < DCAM_FRM_CNT_MAX; i++) {
		DCAM_CLEAR(path2_frame + i);
		(path2_frame+i)->next = path2_frame+(i + 1) % DCAM_FRM_CNT_MAX;
		(path2_frame+i)->prev = path2_frame+(i - 1 + DCAM_FRM_CNT_MAX) % DCAM_FRM_CNT_MAX;
		(path2_frame+i)->fid  = base_id + i;
	}

	s_dcam_mod.dcam_path1.output_frame_head = path1_frame;
	s_dcam_mod.dcam_path2.output_frame_head = path2_frame;
	s_dcam_mod.dcam_path1.output_frame_cur = path1_frame;
	s_dcam_mod.dcam_path2.output_frame_cur = path2_frame;

	return;
}

static int32_t _dcam_path_set_next_frm(uint32_t path_index, uint32_t is_1st_frm)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_frame       *frame;
	struct dcam_path_desc   *path;
	uint32_t		yuv_reg[3];

	if (DCAM_PATH1 == path_index) {
		frame = &s_path1_frame[0];
		path = &s_dcam_mod.dcam_path1;
		yuv_reg[0] = DCAM_FRM_ADDR7;
		yuv_reg[1] = DCAM_FRM_ADDR8;
		yuv_reg[2] = DCAM_FRM_ADDR9;
	} else {
		frame = &s_path2_frame[0];
		path = &s_dcam_mod.dcam_path2;
		yuv_reg[0] = DCAM_FRM_ADDR0;
		yuv_reg[1] = DCAM_FRM_ADDR1;
		yuv_reg[2] = DCAM_FRM_ADDR2;
	}

	if (is_1st_frm) {
		if (path->output_frame_count < DCAM_FRM_CNT_MAX) {
			frame = path->output_frame_cur->prev;
			frame->next = path->output_frame_head;
			path->output_frame_head->prev = frame;
			path->output_frame_cur = path->output_frame_head;
		}
	}

	if (0 == dcam_frame_is_locked(path->output_frame_cur)) {
		REG_WR(yuv_reg[0], path->output_frame_cur->yaddr);
		if (DCAM_YUV400 > path->output_format) {
			REG_WR(yuv_reg[1], path->output_frame_cur->uaddr);
			if (DCAM_YUV420_3FRAME == path->output_format) {
				REG_WR(yuv_reg[2], path->output_frame_cur->vaddr);
			}
		}
		path->output_frame_cur = path->output_frame_cur->next;
	} else {
		rtn = DCAM_RTN_PATH_FRAME_LOCKED;
	}

	return -rtn;
}

static int32_t _dcam_path_trim(uint32_t path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path;
	uint32_t                cfg_reg, ctrl_bit;

	if (DCAM_PATH1 == path_index) {
		path = &s_dcam_mod.dcam_path1;
		cfg_reg = DCAM_PATH_CFG;
		ctrl_bit = 8;
	} else {
		path = &s_dcam_mod.dcam_path2;
		cfg_reg = REV_PATH_CFG;
		ctrl_bit = 1;
	}

	if (path->input_size.w != path->input_rect.w ||
		path->input_size.h != path->input_rect.h) {
//		REG_OWR(cfg_reg, 1 << ctrl_bit);
	} else {
		REG_MWR(cfg_reg, 1 << ctrl_bit, 0 << ctrl_bit);
	}

	return rtn;

}
static int32_t _dcam_path_scaler(uint32_t path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path;
	uint32_t                cfg_reg;

	if (DCAM_PATH1 == path_index) {
		path = &s_dcam_mod.dcam_path1;
		cfg_reg = DCAM_PATH_CFG;
	} else {
		path = &s_dcam_mod.dcam_path2;
		cfg_reg = REV_PATH_CFG;
	}

	if (DCAM_RAWRGB == path->output_format ||
	DCAM_JPEG == path->output_format) {
		REG_MWR(cfg_reg, BIT_3, 1 << 3);
		return DCAM_RTN_SUCCESS;
	}

	rtn = _dcam_calc_sc_size(path_index);
	if (DCAM_RTN_SUCCESS != rtn) {
		return rtn;
	}

	if (path->sc_input_size.w != path->output_size.w ||
	path->sc_input_size.h != path->output_size.h) {
		REG_MWR(cfg_reg, BIT_3, 0 << 3);
		rtn = _dcam_set_sc_coeff(path_index);
	} else {
		REG_MWR(cfg_reg, BIT_3, 1 << 3);
	}

	return rtn;
}

static int32_t _dcam_calc_sc_size(uint32_t path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path;
	uint32_t                cfg_reg;

	if (DCAM_PATH1 == path_index) {
		path = &s_dcam_mod.dcam_path1;
		cfg_reg = DCAM_PATH_CFG;
	} else {
		path = &s_dcam_mod.dcam_path2;
		cfg_reg = REV_PATH_CFG;
	}

	if (path->input_rect.w > (path->output_size.w * DCAM_SC_COEFF_MAX * 2) ||
	path->input_rect.h > (path->output_size.h * DCAM_SC_COEFF_MAX * 2) ||
	path->input_rect.w * DCAM_SC_COEFF_MAX < path->output_size.w ||
	path->input_rect.h * DCAM_SC_COEFF_MAX < path->output_size.h) {
		rtn = DCAM_RTN_PATH_SC_ERR;
	} else {
		path->sc_input_size.w = path->input_rect.w;
		path->sc_input_size.h = path->input_rect.h;
		if (path->input_rect.w > path->output_size.w * DCAM_SC_COEFF_MAX) {
			REG_MWR(cfg_reg, BIT_1, 1 << 1);
			path->sc_input_size.w = path->input_rect.w >> 1;
		}

		if (path->input_rect.h > path->output_size.h * DCAM_SC_COEFF_MAX) {
			REG_MWR(cfg_reg, BIT_2, 1 << 2);
			path->sc_input_size.h = path->input_rect.h >> 1;
		}

	}

	return -rtn;
}

static int32_t _dcam_set_sc_coeff(uint32_t path_index)
{
	struct dcam_path_desc   *path = NULL;
	uint32_t                i = 0;
	uint32_t                h_coeff_addr = DCAM_BASE;
	uint32_t                v_coeff_addr  = DCAM_BASE;
	uint32_t                *tmp_buf = NULL;
	uint32_t                *h_coeff = NULL;
	uint32_t                *v_coeff = NULL;
	uint32_t                clk_switch_bit = 0;
	uint32_t                clk_switch_shift_bit = 0;
	uint32_t                clk_status_bit = 0;
	uint32_t                ver_tap_reg = 0;

	if (DCAM_PATH1 != path_index && DCAM_PATH2 != path_index)
		return -DCAM_RTN_PARA_ERR;

	if (DCAM_PATH1 == path_index) {
		path = &s_dcam_mod.dcam_path1;
		h_coeff_addr += DCAM_SC1_H_TAB_OFFSET;
		v_coeff_addr += DCAM_SC1_V_TAB_OFFSET;
		clk_switch_bit = BIT_3;
		clk_switch_shift_bit = 3;
		clk_status_bit = BIT_5;
		ver_tap_reg = DCAM_PATH_CFG;
	} else {
		path = &s_dcam_mod.dcam_path2;
		h_coeff_addr += DCAM_SC2_H_TAB_OFFSET;
		v_coeff_addr += DCAM_SC2_V_TAB_OFFSET;
		clk_switch_bit = BIT_4;
		clk_switch_shift_bit = 4;
		clk_status_bit = BIT_6;
		ver_tap_reg = REV_PATH_CFG;
	}

	DCAM_TRACE("DCAM DRV: _dcam_set_sc_coeff {%d %d %d %d} \n",
		path->sc_input_size.w,
		path->sc_input_size.h,
		path->output_size.w,
		path->output_size.h);


	tmp_buf = (uint32_t *)kmalloc(DCAM_SC_COEFF_BUF_SIZE, GFP_KERNEL);

	if (NULL == tmp_buf) {
		return -DCAM_RTN_PATH_NO_MEM;
	}

	h_coeff = tmp_buf;
	v_coeff = tmp_buf + (DCAM_SC_COEFF_COEF_SIZE/4);

	if (!(Dcam_GenScaleCoeff((int16_t)path->sc_input_size.w,
		(int16_t)path->sc_input_size.h,
		(int16_t)path->output_size.w,
		(int16_t)path->output_size.h,
		h_coeff,
		v_coeff,
		tmp_buf + (DCAM_SC_COEFF_COEF_SIZE/2),
		DCAM_SC_COEFF_TMP_SIZE))) {
		kfree(tmp_buf);
		DCAM_TRACE("DCAM DRV: _dcam_set_sc_coeff Dcam_GenScaleCoeff error! \n");
		return -DCAM_RTN_PATH_GEN_COEFF_ERR;
	}
#ifndef __SIMULATOR__
	do {
		REG_MWR(DCAM_CFG, clk_switch_bit, DCAM_CLK_DOMAIN_AHB << clk_switch_shift_bit);
	} while (clk_status_bit != (clk_status_bit & REG_RD(DCAM_CFG)));
#endif

	for (i = 0; i < DCAM_SC_COEFF_H_NUM; i++) {
		REG_WR(h_coeff_addr, *h_coeff);
		h_coeff_addr += 4;
		h_coeff++;
	}

	for (i = 0; i < DCAM_SC_COEFF_V_NUM; i++) {
		REG_WR(v_coeff_addr, *v_coeff);
		v_coeff_addr += 4;
		v_coeff++;
	}

	REG_MWR(ver_tap_reg, BIT_19 | BIT_18 | BIT_17 | BIT_16, ((*v_coeff) & 0x0F) << 16);
	DCAM_TRACE("DCAM DRV: _dcam_set_sc_coeff V[%d] = 0x%x \n", i,  (*v_coeff) & 0x0F);
#ifndef __SIMULATOR__
	do {
		REG_MWR(DCAM_CFG, clk_switch_bit, 0 << clk_switch_shift_bit);
	} while (0 != (clk_status_bit & REG_RD(DCAM_CFG)));
#endif
	kfree(tmp_buf);

	return DCAM_RTN_SUCCESS;
}

static void _dcam_force_copy(void)
{
	REG_MWR(DCAM_PATH_CFG, BIT_30, 1 << 30);
	REG_MWR(DCAM_PATH_CFG, BIT_30, 1 << 30);
	REG_MWR(DCAM_PATH_CFG, BIT_30, 0 << 30);
}

static void _dcam_auto_copy(void)
{
	REG_MWR(DCAM_PATH_CFG, BIT_31, 1 << 31);
}

static void _dcam_reg_trace(void)
{
#ifdef DCAM_DRV_DEBUG
	uint32_t                addr = 0;

	printk("DCAM DRV: Register list");
	for (addr = DCAM_CFG; addr <= CAP_SENSOR_CTRL; addr += 16) {
		printk("\n 0x%x: 0x%x 0x%x 0x%x 0x%x",
			addr,
			REG_RD(addr),
			REG_RD(addr + 4),
			REG_RD(addr + 8),
			REG_RD(addr + 12));
	}

	printk("\n");
#endif	
}

static void    _sensor_sof(void)
{
	//DCAM_TRACE("DCAM DRV: _sensor_sof \n");

	return;
}

static void    _sensor_eof(void)
{
	//DCAM_TRACE("DCAM DRV: _sensor_eof \n");
	_dcam_stopped();
	return;
}

static void    _cap_sof(void)
{
	//DCAM_TRACE("DCAM DRV: _cap_sof \n");

	return;
}

static void    _cap_eof(void)
{
	//DCAM_TRACE("DCAM DRV: _cap_eof \n");
	_dcam_stopped();
	return;
}

static void    _path1_done(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	dcam_isr_func           user_func = s_dcam_mod.user_func[DCAM_TX_DONE];
	void                    *data = s_dcam_mod.user_data[DCAM_TX_DONE];
	struct dcam_path_desc   *path = &s_dcam_mod.dcam_path1;
	struct dcam_frame       *frame = path->output_frame_cur->prev->prev;

	printk("DCAM 1\n");

	DCAM_TRACE("DCAM DRV: _path1_done, frame 0x%x, y uv, 0x%x 0x%x \n",
		frame, frame->yaddr, frame->uaddr);

	rtn = _dcam_path_set_next_frm(DCAM_PATH1, false);
	if (rtn) {
		DCAM_TRACE("DCAM DRV: wait for frame unlocked \n");
		return;
	}
	_dcam_auto_copy();

	frame->width = path->output_size.w;
	frame->height = path->output_size.h;

	dcam_frame_lock(frame);

	if(user_func)
	{
		(*user_func)(frame, data);
	}
	return;
}

static void    _path1_overflow(void)
{
	printk("DCAM DRV: _path1_overflow \n");

	return;
}

static void    _sensor_line_err(void)
{
	printk("DCAM DRV: _sensor_line_err \n");

	return;
}

static void    _sensor_frame_err(void)
{
	printk("DCAM DRV: _sensor_eof \n");

	return;
}

static void    _jpeg_buf_ov(void)
{
	printk("DCAM DRV: _jpeg_buf_ov \n");

	return;
}

static void    _path2_done(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	dcam_isr_func           user_func = s_dcam_mod.user_func[DCAM_TX_DONE];
	void                    *data = s_dcam_mod.user_data[DCAM_TX_DONE];
	struct dcam_path_desc   *path = &s_dcam_mod.dcam_path2;
	struct dcam_frame       *frame = path->output_frame_cur->prev->prev;

	if (0 == s_dcam_mod.dcam_path2.valide) {
		DCAM_TRACE("DCAM DRV: path2 works not for capture \n");
		return;
	}

	printk("DCAM 2\n");

	rtn = _dcam_path_set_next_frm(DCAM_PATH2, false);
	if (rtn) {
		DCAM_TRACE("DCAM DRV: wait for frame unlocked \n");
		return;
	}
	_dcam_auto_copy();

	frame->width = path->output_size.w;
	frame->height = path->output_size.h;
	

	if(user_func)
	{
		(*user_func)(frame, data);
	}
	return;
}

static void    _path2_ov(void)
{
	printk("DCAM DRV: _path2_ov \n");

	return;
}

static void    _isp_ov(void)
{
	printk("DCAM DRV: _isp_ov \n");

	return;
}

static void    _mipi_ov(void)
{
	printk("DCAM DRV: _mipi_ov \n");

	return;
}

static void    _dcam_wait_for_stop(void)
{
	s_path1_wait = 1;
	down_interruptible(&s_done_sema);
	return;
}

static void    _dcam_stopped(void)
{
	if (s_path1_wait) {
		up(&s_done_sema);
		s_path1_wait = 0;
	}
	return;
}



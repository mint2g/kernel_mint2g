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

#include <linux/kgdb.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <mach/globalregs.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#include "sprdfb.h"
#include "sprdfb_panel.h"

#include "dsi/mipi_dsih_local.h"
#include "dsi/mipi_dsih_dphy.h"
#include "dsi/mipi_dsih_hal.h"
#include "dsi/mipi_dsih_api.h"

#define DSI_SOFT_RST (26)
#define DSI_PHY_REF_CLOCK (26*1000)

#define DSI_EDPI_CFG (0x6c)

#define MIPI_DPHY_EN (0)
#define AHB_MIPI_PHY_CTRL (0x021c)
#define REG_AHB_MIPI_PHY_CTRL (AHB_MIPI_PHY_CTRL + SPRD_AHB_BASE)


struct sprdfb_dsi_context {
	struct clk		*clk_dsi;
	bool			is_inited;

	dsih_ctrl_t	dsi_inst;
};

static struct sprdfb_dsi_context dsi_ctx;

static uint32_t dsi_core_read_function(uint32_t addr, uint32_t offset)
{
	return __raw_readl(addr + offset);
}

static void dsi_core_write_function(uint32_t addr, uint32_t offset, uint32_t data)
{
	__raw_writel(data, addr + offset);
}


static irqreturn_t dsi_isr0(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t dsi_isr1(int irq, void *data)
{
	return IRQ_HANDLED;
}

static void dsi_reset(void)
{
	#define REG_AHB_SOFT_RST (AHB_SOFT_RST + SPRD_AHB_BASE)
	__raw_writel(__raw_readl(REG_AHB_SOFT_RST) | (1<<DSI_SOFT_RST), REG_AHB_SOFT_RST);
	udelay(10);
	__raw_writel(__raw_readl(REG_AHB_SOFT_RST) & (~(1<<DSI_SOFT_RST)), REG_AHB_SOFT_RST);
}

int32_t dsi_early_int(void)
{
	int ret = 0;

	pr_debug(KERN_INFO "sprdfb:[%s]\n", __FUNCTION__);

	if(dsi_ctx.is_inited){
		printk(KERN_WARNING "sprdfb: dispc early init warning!(has been inited)");
		return 0;
	}

//	dsi_ctx.clk_dsi = clk_get(NULL, "clk_dsi");
//	clk_enable(dsi_ctx.clk_dsi);

	/*enable dphy*/
	__raw_writel(__raw_readl(REG_AHB_MIPI_PHY_CTRL) | (1<<MIPI_DPHY_EN), REG_AHB_MIPI_PHY_CTRL);

	dsi_reset();

//	memset(&(dsi_ctx.dsi_inst), 0, sizeof(dsi_ctx.dsi_inst));

	ret = request_irq(IRQ_DSI_INT0, dsi_isr0, IRQF_DISABLED, "DSI_INT0", &dsi_ctx);
	if (ret) {
		printk(KERN_ERR "sprdfb: dsi failed to request irq int0!\n");
//		clk_disable(dsi_ctx.clk_dsi);
		return -1;
	}

	ret = request_irq(IRQ_DSI_INT1, dsi_isr1, IRQF_DISABLED, "DSI_INT1", &dsi_ctx);
	if (ret) {
		printk(KERN_ERR "sprdfb: dsi failed to request irq int1!\n");
//		clk_disable(dsi_ctx.clk_dsi);
		return -1;
	}

	dsi_ctx.is_inited = true;
	return 0;
}


static int32_t dsi_edpi_setbuswidth(struct info_mipi * mipi)
{
	dsih_color_coding_t color_coding;

	switch(mipi->video_bus_width){
	case 16:
		color_coding = COLOR_CODE_16BIT_CONFIG1;
		break;
	case 18:
		color_coding = COLOR_CODE_18BIT_CONFIG1;
		break;
	case 24:
		color_coding = COLOR_CODE_24BIT;
		break;
	default:
		printk(KERN_ERR "sprdfb:[%s] fail, invalid video_bus_width\n", __FUNCTION__);
		break;
	}

	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_DPI_CFG, ((uint32_t)color_coding<<2));
	return 0;
}


static int32_t dsi_edpi_init(void)
{
	dsi_core_write_function((uint32_t)SPRD_MIPI_DSIC_BASE,  (uint32_t)DSI_EDPI_CFG, 0x10500);
	return 0;
}

static int32_t dsi_dpi_init(struct panel_spec* panel)
{
	dsih_dpi_video_t dpi_param;
	dsih_error_t result;
	struct info_mipi * mipi = panel->info.mipi;

	dpi_param.no_of_lanes = mipi->lan_number;
	dpi_param.byte_clock = mipi->phy_feq / 8;
	dpi_param.pixel_clock = 384*1000/11;//16000;//DSI_PHY_REF_CLOCK / 4;

	switch(mipi->video_bus_width){
	case 16:
		dpi_param.color_coding = COLOR_CODE_16BIT_CONFIG1;
		break;
	case 18:
		dpi_param.color_coding = COLOR_CODE_18BIT_CONFIG1;
		break;
	case 24:
		dpi_param.color_coding = COLOR_CODE_24BIT;
		break;
	default:
		printk(KERN_ERR "sprdfb:[%s] fail, invalid video_bus_width\n", __FUNCTION__);
		break;
	}

	if(SPRDFB_POLARITY_POS == mipi ->h_sync_pol){
		dpi_param.h_polarity = 1;
	}

	if(SPRDFB_POLARITY_POS == mipi ->v_sync_pol){
		dpi_param.v_polarity = 1;
	}

	if(SPRDFB_POLARITY_POS == mipi ->de_pol){
		dpi_param.data_en_polarity = 1;
	}

	dpi_param.h_active_pixels = panel->width;
	dpi_param.h_sync_pixels = mipi->timing->hsync;
	dpi_param.h_back_porch_pixels = mipi->timing->hbp;
	dpi_param.h_total_pixels = panel->width + mipi->timing->hsync + mipi->timing->hbp + mipi->timing->hfp;

	dpi_param.v_active_lines = panel->height;
	dpi_param.v_sync_lines = mipi->timing->vsync;
	dpi_param.v_back_porch_lines = mipi->timing->vbp;
	dpi_param.v_total_lines = panel->height + mipi->timing->vsync + mipi->timing->vbp + mipi->timing->vfp;

	dpi_param.receive_ack_packets = 0;
	dpi_param.video_mode = VIDEO_BURST_WITH_SYNC_PULSES;
	dpi_param.virtual_channel = 0;
	dpi_param.is_18_loosely = 0;

	result = mipi_dsih_dpi_video(&(dsi_ctx.dsi_inst), &dpi_param);
	if(result != OK){
		printk(KERN_ERR "sprdfb: [%s] mipi_dsih_dpi_video fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	return 0;
}

static void dsi_log_error(const char * string)
{
	printk(string);
}


int32_t sprdfb_dsi_init(struct sprdfb_device *dev)
{
	dsih_error_t result = OK;
	dsih_ctrl_t* dsi_instance = &(dsi_ctx.dsi_inst);
	dphy_t *phy = &(dsi_instance->phy_instance);
	struct info_mipi * mipi = dev->panel->info.mipi;
	bool resume = false;

	pr_debug(KERN_INFO "sprdfb:[%s]\n", __FUNCTION__);

	if(dev->panel_ready && dsi_ctx.is_inited){
		resume = true;
	}

	if(dev->panel_ready){
		dsi_ctx.is_inited = true;
	}else{
		dsi_early_int();
	}

	phy->address = SPRD_MIPI_DSIC_BASE;
	phy->core_read_function = dsi_core_read_function;
	phy->core_write_function = dsi_core_write_function;
	phy->log_error = dsi_log_error;
	phy->log_info = NULL;
	phy->reference_freq = DSI_PHY_REF_CLOCK;

	dsi_instance->address = SPRD_MIPI_DSIC_BASE;
	dsi_instance->color_mode_polarity =mipi->color_mode_pol;
	dsi_instance->shut_down_polarity = mipi->shut_down_pol;
	dsi_instance->core_read_function = dsi_core_read_function;
	dsi_instance->core_write_function = dsi_core_write_function;
	dsi_instance->log_error = dsi_log_error;
	dsi_instance->log_info = NULL;
	 /*in our rtl implementation, this is max rd time, not bta time and use 15bits*/
	dsi_instance->max_bta_cycles = 0x6000;//10;
	dsi_instance->max_hs_to_lp_cycles = 4;//110;
	dsi_instance->max_lp_to_hs_cycles = 15;//10;
	dsi_instance->max_lanes = mipi->lan_number;

	if(dev->panel_ready && !resume){
		printk(KERN_INFO "sprdfb:[%s]: dsi has alread initialized\n", __FUNCTION__);
		dsi_instance->status = INITIALIZED;
		return 0;
	}

	if(SPRDFB_MIPI_MODE_CMD == mipi->work_mode){
		dsi_edpi_init();
	}/*else{
		dsi_dpi_init(dev->panel);
	}*/

/*
	result = mipi_dsih_unregister_all_events(dsi_instance);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_unregister_all_events fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}
*/
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK0, 0x1fffff);
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE,  R_DSI_HOST_ERROR_MSK1, 0x3ffff);

	result = mipi_dsih_open(dsi_instance);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_open fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	result = mipi_dsih_dphy_configure(phy,  mipi->lan_number, mipi->phy_feq);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_dphy_configure fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	while(5 != (dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_STATUS) & 5));

	if(SPRDFB_MIPI_MODE_CMD == mipi->work_mode){
		dsi_edpi_setbuswidth(mipi);
	}

	result = mipi_dsih_enable_rx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_enable_rx fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	result = mipi_dsih_ecc_rx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_ecc_rx fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	result = mipi_dsih_eotp_rx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_eotp_rx fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	result = mipi_dsih_eotp_tx(dsi_instance, 1);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_eotp_tx fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	if(SPRDFB_MIPI_MODE_VIDEO == mipi->work_mode){
		dsi_dpi_init(dev->panel);
	}

	return 0;
}

int32_t sprdfb_dsi_uninit(struct sprdfb_device *dev)
{
	dsih_error_t result;
	dsih_ctrl_t* dsi_instance = &(dsi_ctx.dsi_inst);
	printk(KERN_INFO "sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);
	result = mipi_dsih_close(&(dsi_ctx.dsi_inst));
	dsi_instance->status = NOT_INITIALIZED;

	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: sprdfb_dsi_uninit fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}

	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_IF_CTRL, 0);
	mdelay(10);
	return 0;
}

int32_t sprdfb_dsi_suspend(struct sprdfb_device *dev)
{
	dsih_ctrl_t* dsi_instance = &(dsi_ctx.dsi_inst);
	printk(KERN_INFO "sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);
//	sprdfb_dsi_uninit(dev);
//	mipi_dsih_dphy_close(&(dsi_instance->phy_instance));
//	mipi_dsih_dphy_shutdown(&(dsi_instance->phy_instance), 0);
	mipi_dsih_hal_power(dsi_instance, 0);

	return 0;
}

int32_t sprdfb_dsi_resume(struct sprdfb_device *dev)
{
	dsih_error_t result = OK;
	dsih_ctrl_t* dsi_instance = &(dsi_ctx.dsi_inst);
	dphy_t *phy = &(dsi_instance->phy_instance);
	struct info_mipi * mipi = dev->panel->info.mipi;

	printk(KERN_INFO "sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

#if 0
	result = mipi_dsih_dphy_open(&(dsi_instance->phy_instance));
	if(0 != result){
		printk("Jessica: mipi_dsih_dphy_open fail!(%d)\n",result);
	}
	udelay(100);
#endif
//	mipi_dsih_dphy_shutdown(&(dsi_instance->phy_instance), 1);
	mipi_dsih_hal_power(dsi_instance, 1);

#if 0
	result = mipi_dsih_dphy_configure(phy,  mipi->lan_number, mipi->phy_feq);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s]: mipi_dsih_dphy_configure fail (%d)!\n", __FUNCTION__, result);
		return -1;
	}
#endif
	return 0;
}

int32_t sprdfb_dsi_ready(struct sprdfb_device *dev)
{
	struct info_mipi * mipi = dev->panel->info.mipi;

	if(SPRDFB_MIPI_MODE_CMD == mipi->work_mode){
		mipi_dsih_cmd_mode(&(dsi_ctx.dsi_inst), 1);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_CMD_MODE_CFG, 0x1);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PHY_IF_CTRL, 0x1);
	}else{
		mipi_dsih_video_mode(&(dsi_ctx.dsi_inst), 1);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PWR_UP, 0);
		udelay(100);
		dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_PWR_UP, 1);
		mdelay(3);
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_ST0);
		dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_ST1);
	}
	return 0;
}

static int32_t sprdfb_dsi_set_cmd_mode(void)
{
	mipi_dsih_cmd_mode(&(dsi_ctx.dsi_inst), 1);
	return 0;
}

static int32_t sprdfb_dsi_set_video_mode(void)
{
	mipi_dsih_video_mode(&(dsi_ctx.dsi_inst), 1);
	return 0;
}

static int32_t sprdfb_dsi_gen_write(uint8_t *param, uint16_t param_length)
{
	dsih_error_t result;
	result = mipi_dsih_gen_wr_cmd(&(dsi_ctx.dsi_inst), 0, param, param_length);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s] error (%d)\n", __FUNCTION__, result);
		return -1;
	}
	return 0;
}

static int32_t sprdfb_dsi_gen_read(uint8_t *param, uint16_t param_length, uint8_t bytes_to_read, uint8_t *read_buffer)
{
	uint16_t result;
	result = mipi_dsih_gen_rd_cmd(&(dsi_ctx.dsi_inst), 0, param, param_length, bytes_to_read, read_buffer);
	if(0 != result){
		printk(KERN_ERR "sprdfb: [%s] error (%d)\n", __FUNCTION__, result);
		return -1;
	}
	return 0;
}

static int32_t sprdfb_dsi_dcs_write(uint8_t *param, uint16_t param_length)
{
	dsih_error_t result;
	result = mipi_dsih_dcs_wr_cmd(&(dsi_ctx.dsi_inst), 0, param, param_length);
	if(OK != result){
		printk(KERN_ERR "sprdfb: [%s] error (%d)\n", __FUNCTION__, result);
		return -1;
	}
	return 0;
}

static int32_t sprdfb_dsi_dcs_read(uint8_t command, uint8_t bytes_to_read, uint8_t *read_buffer)
{
	uint16_t result;
	result = mipi_dsih_dcs_rd_cmd(&(dsi_ctx.dsi_inst), 0, command, bytes_to_read, read_buffer);
	if(0 != result){
		printk(KERN_ERR "sprdfb: [%s] error (%d)\n", __FUNCTION__, result);
		return -1;
	}
	return 0;
}

static int32_t sprd_dsi_force_write(uint8_t data_type, uint8_t *p_params, uint16_t param_length)
{
	int32_t iRtn = 0;
	iRtn = mipi_dsih_gen_wr_packet(&(dsi_ctx.dsi_inst), 0, data_type,  p_params, param_length);
	return iRtn;
}

static int32_t sprd_dsi_force_read(uint8_t command, uint8_t bytes_to_read, uint8_t * read_buffer)
{
	int32_t iRtn = 0;
	dsih_ctrl_t *curInstancePtr = &(dsi_ctx.dsi_inst);

	mipi_dsih_eotp_rx(curInstancePtr, 0);
	mipi_dsih_eotp_tx(curInstancePtr, 0);

	iRtn = mipi_dsih_gen_rd_packet(&(dsi_ctx.dsi_inst),  0,  6,  0, command,  bytes_to_read, read_buffer);

	mipi_dsih_eotp_rx(curInstancePtr, 1);
	mipi_dsih_eotp_tx(curInstancePtr, 1);

	return iRtn;
}

struct ops_mipi sprdfb_mipi_ops = {
	.mipi_set_cmd_mode = sprdfb_dsi_set_cmd_mode,
	.mipi_set_video_mode = sprdfb_dsi_set_video_mode,
	.mipi_gen_write = sprdfb_dsi_gen_write,
	.mipi_gen_read = sprdfb_dsi_gen_read,
	.mipi_dcs_write = sprdfb_dsi_dcs_write,
	.mipi_dcs_read = sprdfb_dsi_dcs_read,
	.mipi_force_write = sprd_dsi_force_write,
	.mipi_force_read = sprd_dsi_force_read,
};




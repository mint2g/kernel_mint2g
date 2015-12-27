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
#ifndef _DCAM_DRV_SC8810_H_
#define _DCAM_DRV_SC8810_H_

#include <linux/slab.h>
#include "dcam_common.h"
#include "sc8810_reg_isp.h"
#include "gen_scale_coef.h"
#include <video/sprd_scale.h>
#include "../common/isp_control.h"
/*#define DCAM_DRV_DEBUG 1*/
#define DCAM_DRV_TRACE pr_debug
#define DCAM_DRV_ERR printk
#define GLOBAL_BASE SPRD_GREG_BASE	/*0xE0002E00UL <--> 0x8b000000 */
#define ARM_GLOBAL_REG_GEN0 GLOBAL_BASE + 0x008UL
#define ARM_GLOBAL_REG_GEN3 GLOBAL_BASE + 0x01CUL
#define ARM_GLOBAL_PLL_SCR GLOBAL_BASE + 0x070UL
#define GR_CLK_GEN5 GLOBAL_BASE + 0x07CUL
#define CLK_DLY_CTRL GLOBAL_BASE + 0x05CUL

#define AHB_BASE SPRD_AHB_BASE	/*0xE000A000 <--> 0x20900000UL */
#define AHB_GLOBAL_REG_CTL0 AHB_BASE + 0x200UL
#define AHB_GLOBAL_REG_SOFTRST AHB_BASE + 0x210UL

#define PIN_CTL_BASE SPRD_CPC_BASE	/*0xE002F000<-->0x8C000000UL */
#define PIN_CTL_CCIRPD1 PIN_CTL_BASE + 0x344UL
#define PIN_CTL_CCIRPD0 PIN_CTL_BASE + 0x348UL

#define IRQ_BASE SPRD_ASHB_BASE	/*0xE0021000<-->0x80003000 */
#define INT_IRQ_EN IRQ_BASE + 0x008UL
#define INT_IRQ_DISABLE IRQ_BASE + 0x00CUL

#define MISC_BASE SPRD_MISC_BASE	/*0xE0033000<-->0x82000000 */
#ifdef CONFIG_ARCH_SC8810
#define ANA_REG_BASE MISC_BASE + 0x600
#define ANA_LDO_PD_CTL ANA_REG_BASE + 0x10
#define ANA_LDO_VCTL2 ANA_REG_BASE + 0x20
#else
#define ANA_REG_BASE MISC_BASE + 0x480
#define ANA_LDO_PD_CTL ANA_REG_BASE + 0x10
#define ANA_LDO_VCTL2 ANA_REG_BASE + 0x1C
#endif

/*0xE0002000UL <-->0x20200000*/
#define DCAM_REG_BASE SPRD_ISP_BASE
#define DCAM_CFG DCAM_REG_BASE + 0x00UL
#define DCAM_PATH_CFG DCAM_REG_BASE + 0x04UL
#define DCAM_SRC_SIZE DCAM_REG_BASE + 0x08UL
#define DCAM_DES_SIZE DCAM_REG_BASE + 0x0CUL
#define DCAM_TRIM_START DCAM_REG_BASE + 0x10UL
#define DCAM_TRIM_SIZE DCAM_REG_BASE + 0x14UL
#define REV_PATH_CFG DCAM_REG_BASE + 0x18UL
#define REV_SRC_SIZE DCAM_REG_BASE + 0x1CUL
#define REV_DES_SIZE DCAM_REG_BASE + 0x20UL
#define REV_TRIM_START DCAM_REG_BASE + 0x24UL
#define REV_TRIM_SIZE  DCAM_REG_BASE + 0x28UL
#define SLICE_VER_CNT DCAM_REG_BASE + 0x2CUL
#define DCAM_INT_STS DCAM_REG_BASE + 0x30UL
#define DCAM_INT_MASK DCAM_REG_BASE + 0x34UL
#define DCAM_INT_CLR DCAM_REG_BASE + 0x38UL
#define DCAM_INT_RAW DCAM_REG_BASE + 0x3CUL
#define ENDIAN_SEL DCAM_REG_BASE + 0x64UL
#define DCAM_ADDR_0 DCAM_REG_BASE + 0x40UL
#define DCAM_ADDR_1 DCAM_REG_BASE + 0x44UL
#define DCAM_ADDR_7 DCAM_REG_BASE + 0x6CUL
#define DCAM_ADDR_8 DCAM_REG_BASE + 0x70UL
#define CAP_CTRL DCAM_REG_BASE + 0x100UL
#define CAP_CNTRL CAP_CTRL
#define CAP_FRM_CTRL DCAM_REG_BASE + 0x104UL
#define CAP_FRM_CNT CAP_FRM_CTRL
#define CAP_START DCAM_REG_BASE + 0x108UL
#define CAP_END DCAM_REG_BASE + 0x10CUL
#define CAP_IMAGE_DECI DCAM_REG_BASE + 0x110UL
#define CAP_JPG_CTL DCAM_REG_BASE + 0x11CUL
#define CAP_JPG_FRM_CTL CAP_JPG_CTL
#define CAP_JPG_FRM_SIZE DCAM_REG_BASE + 0x120UL
#define CAP_SPI_CFG	DCAM_REG_BASE+0x124UL

typedef void (*ISP_ISR_FUNC_PTR) (void *);
#define ISP_PATH1_FRAME_COUNT_MAX 4
#define ISP_PATH2_FRAME_COUNT_MAX ISP_PATH1_FRAME_COUNT_MAX
#define ISP_PATH_SC_COEFF_MAX 4
#define ISP_CAP_DEC_XY_MAX 3
#define ISP_SCALE_FRAME_MODE_WIDTH_TH 960
#define ISP_SCALE_COEFF_H_NUM 48
#define ISP_SCALE_COEFF_V_NUM 68
#define ISP_AHB_SLAVE_ADDR  SPRD_ISP_BASE
#define ISP_AHB_CTRL_MOD_EN_OFFSET 0
#define ISP_AHB_CTRL_MEM_SW_OFFSET 4
#define ISP_AHB_CTRL_SOFT_RESET_OFFSET 0x10
int32_t ISP_DriverModuleInit(uint32_t base_addr);
int32_t ISP_DriverModuleEnable(uint32_t ahb_ctrl_addr);
int32_t ISP_DriverModuleDisable(uint32_t ahb_ctrl_addr);
int32_t ISP_DriverSoftReset(uint32_t ahb_ctrl_addr);
void ISP_DriverScalingCoeffReset(void);
int32_t ISP_DriverSetClk(uint32_t pll_src_addr, ISP_CLK_SEL_E clk_sel);
void ISP_DriverIramSwitch(uint32_t base_addr, uint32_t isp_or_arm);
int32_t ISP_DriverStart(uint32_t base_addr);
int32_t ISP_DriverStop(uint32_t base_addr);
int32_t ISP_DriverSetMode(uint32_t base_addr, ISP_MODE_E isp_mode);
ISP_MODE_E ISP_DriverGetMode(void);
int32_t ISP_DriverNoticeRegister(uint32_t base_addr,
				 ISP_IRQ_NOTICE_ID_E notice_id,
				 ISP_ISR_FUNC_PTR user_func);
int32_t ISP_DriverCapConfig(uint32_t base_addr, ISP_CFG_ID_E id, void *param);
int32_t ISP_DriverCapGetInfo(uint32_t base_addr, ISP_CFG_ID_E id, void *param);
int32_t ISP_DriverPath1Config(uint32_t base_addr, ISP_CFG_ID_E id, void *param);
int32_t ISP_DriverPath2Config(uint32_t base_addr, ISP_CFG_ID_E id, void *param);
int ISP_DriverRegisterIRQ(void);
void ISP_DriverUnRegisterIRQ(void);
uint32_t ISP_DriverSetBufferAddress(uint32_t base_addr, uint32_t buf_addr,
				    uint32_t uv_addr);
void ISP_DriverPowerDown(uint32_t base_addr, uint32_t sensor_id,
			 uint32_t value);
void ISP_DriverReset(uint32_t base_addr, uint32_t value);
void ISP_DriverHandleErr(uint32_t ahb_ctrl_addr, uint32_t base_addr);
void _ISP_DriverEnableInt(void);
void _ISP_DriverDisableInt(void);
#endif

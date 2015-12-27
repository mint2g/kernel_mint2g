/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *************************************************
 * Automatically generated C header: do not edit *
 *************************************************
 */

SCI_CLK_ADD(ext_pad, 1000000, 0, 0,
	0, 0, 0, 0, 0);

SCI_CLK_ADD(ext_26m, 26000000, 0, 0,
	0, 0, 0, 0, 0);

SCI_CLK_ADD(ext_32k, 32768, 0, 0,
	0, 0, 0, 0, 0);

SCI_CLK_ADD(clk_mpll, 0, REG_GLB_PCTRL, BIT(1),
	REG_GLB_M_PLL_CTL0, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10), 0, 0,
	1, &ext_26m);

SCI_CLK_ADD(clk_gpll, 0, REG_GLB_PCTRL, BIT(16),
	REG_GLB_G_PLL_CTL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10), 0, 0,
	1, &ext_26m);

SCI_CLK_ADD(clk_dpll, 0, REG_GLB_PCTRL, BIT(3),
	REG_GLB_D_PLL_CTL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10), 0, 0,
	1, &ext_26m);

SCI_CLK_ADD(clk_tdpll, 768000000, REG_GLB_PCTRL, BIT(2),
	0, 0, 0, 0,
	1, &ext_26m);

SCI_CLK_ADD(clk_450m, 0, 0, 0,
	2, 0, 0, 0,
	1, &clk_mpll);

SCI_CLK_ADD(clk_300m, 0, 0, 0,
	3, 0, 0, 0,
	1, &clk_mpll);

SCI_CLK_ADD(clk_225m, 0, 0, 0,
	4, 0, 0, 0,
	1, &clk_mpll);

SCI_CLK_ADD(clk_384m, 0, REG_GLB_TD_PLL_CTL+1, BIT(11),
	2, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_256m, 0, REG_GLB_TD_PLL_CTL+1, BIT(10),
	3, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_192m, 0, REG_GLB_TD_PLL_CTL+1, BIT(9),
	4, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_153p6m, 0, REG_GLB_TD_PLL_CTL+1, BIT(8),
	5, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_48m, 0, 0, 0,
	8, 0, 0, 0,
	1, &clk_384m);

SCI_CLK_ADD(clk_24m, 0, 0, 0,
	16, 0, 0, 0,
	1, &clk_384m);

SCI_CLK_ADD(clk_12m, 0, 0, 0,
	32, 0, 0, 0,
	1, &clk_384m);

SCI_CLK_ADD(clk_128m, 0, 0, 0,
	2, 0, 0, 0,
	1, &clk_256m);

SCI_CLK_ADD(clk_64m, 0, 0, 0,
	4, 0, 0, 0,
	1, &clk_256m);

SCI_CLK_ADD(clk_32m, 0, 0, 0,
	8, 0, 0, 0,
	1, &clk_256m);

SCI_CLK_ADD(clk_96m, 0, 0, 0,
	2, 0, 0, 0,
	1, &clk_192m);

SCI_CLK_ADD(clk_76p8m, 0, 0, 0,
	2, 0, 0, 0,
	1, &clk_153p6m);

SCI_CLK_ADD(clk_51p2m, 0, 0, 0,
	3, 0, 0, 0,
	1, &clk_153p6m);

SCI_CLK_ADD(clk_10p24m, 0, 0, 0,
	15, 0, 0, 0,
	1, &clk_153p6m);

SCI_CLK_ADD(clk_5p12m, 0, 0, 0,
	30, 0, 0, 0,
	1, &clk_153p6m);

SCI_CLK_ADD(clk_mcu, 0, 0, 0,
	REG_AHB_ARM_CLK, BIT(0)|BIT(1)|BIT(2), REG_AHB_ARM_CLK, BIT(23)|BIT(24),
	4, &clk_mpll, &clk_384m, &clk_256m, &ext_26m);

SCI_CLK_ADD(clk_arm, 0, 0, 0,
	0, 0, 0, 0,
	1, &clk_mcu);

SCI_CLK_ADD(clk_axi, 0, 0, 0,
	REG_AHB_CA5_CFG, BIT(11)|BIT(12), 0, 0,
	1, &clk_mcu);

SCI_CLK_ADD(clk_ahb, 0, 0, 0,
	REG_AHB_ARM_CLK, BIT(4)|BIT(5)|BIT(6), 0, 0,
	1, &clk_mcu);

SCI_CLK_ADD(clk_dbg, 0, REG_AHB_CA5_CFG, BIT(9),
	REG_AHB_ARM_CLK, BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19), 0, 0,
	1, &clk_mcu);

SCI_CLK_ADD(clk_arm_peri, 0, 0, 0,
	REG_AHB_ARM_CLK, BIT(20)|BIT(21)|BIT(22), 0, 0,
	1, &clk_mcu);

SCI_CLK_ADD(clk_emc, 0, REG_AHB_AHB_CTL0, BIT(28),
	REG_AHB_ARM_CLK, BIT(8)|BIT(9)|BIT(10)|BIT(11), REG_AHB_ARM_CLK, BIT(12)|BIT(13),
	4, &clk_450m, &clk_dpll, &clk_256m, &ext_26m);

SCI_CLK_ADD(clk_apb, 0, REG_AHB_AHB_CTL1+1, BIT(10),
	0, 0, REG_GLB_CLKDLY, BIT(14)|BIT(15),
	4, &ext_26m, &clk_51p2m, &clk_76p8m, &clk_76p8m);

SCI_CLK_ADD(clk_disp_mtx, 0, REG_AHB_AHB_CTL2, BIT(11),
	0, 0, 0, 0,
	1, &clk_ahb);

SCI_CLK_ADD(clk_mm_mtx, 0, REG_AHB_AHB_CTL2, BIT(10),
	0, 0, 0, 0,
	1, &clk_ahb);

SCI_CLK_ADD(clk_mm, 0, REG_AHB_AHB_CTL0, BIT(13),
	0, 0, 0, 0,
	1, &clk_mm_mtx);

SCI_CLK_ADD(clk_isp_i, 0, REG_AHB_AHB_CTL0, BIT(12),
	0, 0, 0, 0,
	1, &clk_mm);

SCI_CLK_ADD(clk_dcam_i, 0, REG_AHB_AHB_CTL0, BIT(1),
	0, 0, 0, 0,
	1, &clk_mm);

SCI_CLK_ADD(clk_dispc_i, 0, REG_AHB_AHB_CTL0, BIT(22),
	0, 0, 0, 0,
	1, &clk_disp_mtx);

SCI_CLK_ADD(clk_lcdc_i, 0, REG_AHB_AHB_CTL0, BIT(3),
	0, 0, 0, 0,
	1, &clk_ahb);

SCI_CLK_ADD(clk_vsp_core, 0, REG_AHB_AHB_CTL2, BIT(6),
	0, 0, 0, 0,
	1, &clk_mm);

SCI_CLK_ADD(clk_isp_core, 0, REG_AHB_AHB_CTL2, BIT(7),
	0, 0, 0, 0,
	1, &clk_isp_i);

SCI_CLK_ADD(clk_dcam_core, 0, REG_AHB_AHB_CTL2, BIT(5),
	0, 0, 0, 0,
	1, &clk_dcam_i);

SCI_CLK_ADD(clk_dispc_core, 0, REG_AHB_AHB_CTL2, BIT(9),
	0, 0, 0, 0,
	1, &clk_dispc_i);

SCI_CLK_ADD(clk_lcdc_core, 0, REG_AHB_AHB_CTL2, BIT(8),
	0, 0, 0, 0,
	1, &clk_lcdc_i);

SCI_CLK_ADD(clk_gpu_axi, 0, REG_AHB_AHB_CTL0, BIT(21),
	REG_GLB_GEN2, BIT(14)|BIT(15)|BIT(16), REG_GLB_GEN2, BIT(0)|BIT(1),
	4, &clk_gpll, &clk_dpll, &clk_mpll, &ext_26m);

SCI_CLK_ADD(ccir_mclk, 0, REG_GLB_GEN0, BIT(14),
	REG_GLB_GEN3, BIT(24)|BIT(25)|BIT(26), REG_GLB_PLL_SCR, BIT(18)|BIT(19),
	4, &clk_96m, &clk_76p8m, &clk_48m, &ext_26m);

SCI_CLK_ADD(clk_ccir_in, 64000000, REG_AHB_AHB_CTL0, BIT(2),
	0, 0, 0, 0, 0);

SCI_CLK_ADD(clk_ccir, 0, REG_AHB_AHB_CTL0, BIT(9),
	0, 0, REG_GLB_PLL_SCR, BIT(20)|BIT(21),
	4, &clk_ccir_in, &clk_76p8m, &clk_48m, &ext_26m);

SCI_CLK_ADD(clk_dcam, 0, &clk_dcam_core, 0,
	0, 0, REG_GLB_PLL_SCR, BIT(4)|BIT(5),
	4, &clk_256m, &clk_128m, &clk_76p8m, &clk_48m);

SCI_CLK_ADD(clk_dcam_mipi, 0, REG_AHB_AHB_CTL0, BIT(10),
	0, 0, REG_GLB_PLL_SCR, BIT(22)|BIT(23),
	4, &ext_pad, &clk_96m, &clk_48m, &clk_128m);

SCI_CLK_ADD(clk_vsp, 0, &clk_vsp_core, 0,
	0, 0, REG_GLB_PLL_SCR, BIT(2)|BIT(3),
	4, &clk_192m, &clk_153p6m, &clk_64m, &clk_48m);

SCI_CLK_ADD(clk_lcd, 0, &clk_lcdc_core, 0,
	REG_GLB_GEN4, BIT(0)|BIT(1)|BIT(2), REG_GLB_PLL_SCR, BIT(6)|BIT(7),
	4, &clk_48m, &clk_128m, &clk_64m, &clk_76p8m);

SCI_CLK_ADD(clk_dispc, 0, &clk_dispc_core, 0,
	REG_AHB_DISPC_CTRL, BIT(3)|BIT(4)|BIT(5), REG_AHB_DISPC_CTRL, BIT(1)|BIT(2),
	4, &clk_256m, &clk_192m, &clk_153p6m, &clk_96m);

SCI_CLK_ADD(clk_dispc_dpi, 0, REG_AHB_AHB_CTL0, BIT(22),
	REG_AHB_DISPC_CTRL, BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26), REG_AHB_DISPC_CTRL, BIT(17)|BIT(18),
	4, &clk_384m, &clk_192m, &clk_153p6m, &clk_128m);

SCI_CLK_ADD(clk_dispc_dbi, 0, REG_AHB_AHB_CTL0, BIT(22),
	REG_AHB_DISPC_CTRL, BIT(11)|BIT(12)|BIT(13), REG_AHB_DISPC_CTRL, BIT(9)|BIT(10),
	4, &clk_256m, &clk_192m, &clk_153p6m, &clk_128m);

SCI_CLK_ADD(clk_isp, 0, &clk_isp_core, 0,
	REG_AHB_ISP_CTRL, BIT(2)|BIT(3)|BIT(4), REG_AHB_ISP_CTRL, BIT(0)|BIT(1),
	4, &clk_192m, &clk_153p6m, &clk_128m, &clk_48m);

SCI_CLK_ADD(clk_nfc, 0, REG_AHB_AHB_CTL0, BIT(8),
	REG_GLB_GEN2, BIT(6)|BIT(7)|BIT(8), REG_GLB_GEN2, BIT(4)|BIT(5),
	4, &clk_153p6m, &clk_128m, &clk_76p8m, &clk_64m);

SCI_CLK_ADD(clk_sdio_src, 0, REG_GLB_CLK_GEN5, BIT(25),
	REG_GLB_CLK_GEN5, BIT(26)|BIT(27)|BIT(28)|BIT(29), 0, 0,
	1, &clk_mpll);

SCI_CLK_ADD(clk_sdio_src1, 0, REG_GLB_CLK_GEN5, BIT(25),
	5, 0, 0, 0,
	1, &clk_gpll);

SCI_CLK_ADD(clk_sdio_src2, 0, REG_GLB_CLK_GEN5, BIT(25),
	2, 0, 0, 0,
	1, &clk_sdio_src);

SCI_CLK_ADD(clk_sdio0, 0, REG_AHB_AHB_CTL0, BIT(4),
	0, 0, REG_GLB_CLK_GEN5, BIT(17)|BIT(18),
	4, &clk_sdio_src, &clk_sdio_src1, &clk_sdio_src2, &ext_26m);

SCI_CLK_ADD(clk_sdio1, 0, REG_AHB_AHB_CTL0, BIT(19),
	0, 0, REG_GLB_CLK_GEN5, BIT(19)|BIT(20),
	4, &clk_sdio_src, &clk_sdio_src1, &clk_sdio_src2, &ext_26m);

SCI_CLK_ADD(clk_sdio2, 0, REG_AHB_AHB_CTL0, BIT(24),
	0, 0, REG_GLB_CLK_GEN5, BIT(21)|BIT(22),
	4, &clk_192m, &clk_64m, &clk_48m, &ext_26m);

SCI_CLK_ADD(clk_emmc, 0, REG_AHB_AHB_CTL0, BIT(23),
	0, 0, REG_GLB_CLK_GEN5, BIT(23)|BIT(24),
	4, &clk_384m, &clk_256m, &clk_153p6m, &ext_26m);

SCI_CLK_ADD(clk_uart0, 0, REG_GLB_GEN0, BIT(20),
	REG_GLB_CLK_GEN5, BIT(0)|BIT(1)|BIT(2), REG_GLB_CLKDLY, BIT(20)|BIT(21),
	4, &clk_96m, &clk_51p2m, &clk_48m, &ext_26m);

SCI_CLK_ADD(clk_uart1, 0, REG_GLB_GEN0, BIT(21),
	REG_GLB_CLK_GEN5, BIT(3)|BIT(4)|BIT(5), REG_GLB_CLKDLY, BIT(22)|BIT(23),
	4, &clk_96m, &clk_51p2m, &clk_48m, &ext_26m);

SCI_CLK_ADD(clk_uart2, 0, REG_GLB_GEN0, BIT(22),
	REG_GLB_CLK_GEN5, BIT(6)|BIT(7)|BIT(8), REG_GLB_CLKDLY, BIT(24)|BIT(25),
	4, &clk_96m, &clk_51p2m, &clk_48m, &ext_26m);

SCI_CLK_ADD(clk_uart3, 0, REG_GLB_GEN0, BIT(0),
	REG_GLB_GEN3, BIT(18)|BIT(19)|BIT(20), REG_GLB_GEN3, BIT(16)|BIT(17),
	4, &clk_96m, &clk_51p2m, &clk_48m, &ext_26m);

SCI_CLK_ADD(clk_spi0, 0, REG_GLB_GEN0, BIT(17),
	REG_GLB_GEN2, BIT(21)|BIT(22)|BIT(23), REG_GLB_CLKDLY, BIT(26)|BIT(27),
	4, &clk_192m, &clk_153p6m, &clk_96m, &ext_26m);

SCI_CLK_ADD(clk_spi1, 0, REG_GLB_GEN0, BIT(18),
	REG_GLB_GEN2, BIT(11)|BIT(12)|BIT(13), REG_GLB_CLKDLY, BIT(30)|BIT(31),
	4, &clk_192m, &clk_153p6m, &clk_96m, &ext_26m);

SCI_CLK_ADD(clk_spi2, 0, REG_GLB_GEN0, BIT(1),
	REG_GLB_GEN3, BIT(5)|BIT(6)|BIT(7), REG_GLB_GEN3, BIT(3)|BIT(4),
	4, &clk_192m, &clk_153p6m, &clk_96m, &ext_26m);

SCI_CLK_ADD(clk_iis0, 0, REG_GLB_GEN0, BIT(12),
	REG_GLB_GEN2, BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31), REG_GLB_PLL_SCR, BIT(8)|BIT(9),
	4, &clk_128m, &clk_51p2m, &ext_26m, &ext_26m);

SCI_CLK_ADD(clk_iis1, 0, REG_GLB_GEN0, BIT(25),
	REG_GLB_GEN3, BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15), REG_GLB_PLL_SCR, BIT(14)|BIT(15),
	4, &clk_128m, &clk_51p2m, &ext_26m, &ext_26m);

SCI_CLK_ADD(clk_vbc, 0, REG_GLB_GEN1, BIT(14),
	0, 0, 0, 0,
	1, &clk_apb);

SCI_CLK_ADD(clk_aud, 0, REG_GLB_GEN1, BIT(13),
	0, 0, 0, 0,
	1, &ext_26m);

SCI_CLK_ADD(clk_audif, 0, REG_GLB_GEN1, BIT(12),
	0, 0, REG_GLB_GEN1, BIT(19)|BIT(20),
	4, &clk_51p2m, &clk_48m, &clk_32m, &ext_26m);

SCI_CLK_ADD(clk_aux0, 0, REG_GLB_GEN1, BIT(10),
	REG_GLB_GEN1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), REG_GLB_PLL_SCR, BIT(10)|BIT(11),
	4, &clk_96m, &clk_76p8m, &ext_32k, &ext_26m);

SCI_CLK_ADD(clk_aux1, 0, REG_GLB_GEN1, BIT(11),
	REG_GLB_PCTRL, BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29), REG_GLB_PLL_SCR, BIT(12)|BIT(13),
	4, &clk_96m, &clk_76p8m, &ext_32k, &ext_26m);

SCI_CLK_ADD(clk_pwm0, 0, REG_GLB_CLK_EN, BIT(21),
	0, 0, REG_GLB_CLK_EN, BIT(25),
	2, &ext_26m, &ext_32k);

SCI_CLK_ADD(clk_pwm1, 0, REG_GLB_CLK_EN, BIT(22),
	0, 0, REG_GLB_CLK_EN, BIT(26),
	2, &ext_26m, &ext_32k);

SCI_CLK_ADD(clk_pwm2, 0, REG_GLB_CLK_EN, BIT(23),
	0, 0, REG_GLB_CLK_EN, BIT(27),
	2, &ext_26m, &ext_32k);

SCI_CLK_ADD(clk_pwm3, 0, REG_GLB_CLK_EN, BIT(24),
	0, 0, REG_GLB_CLK_EN, BIT(28),
	2, &ext_26m, &ext_32k);

SCI_CLK_ADD(clk_usb_ref, 0, REG_AHB_AHB_CTL3, BIT(6),
	0, 0, REG_AHB_AHB_CTL3, BIT(0),
	2, &clk_24m, &clk_12m);


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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/err.h>

#include <asm/clkdev.h>

#include "clock_common.h"
#include "clock_sc8810.h"

#define GREG_BASE     		SPRD_GREG_BASE
#define AHB_REG_BASE      (SPRD_AHB_BASE+0x200)


#define GR_PLL_SCR                      (GREG_BASE + 0x0070)
#define GR_GEN0                         (GREG_BASE + 0x0008)
#define GR_GEN3                         (GREG_BASE + 0x001C)

#define AHB_CA5_CFG	(AHB_REG_BASE + 0x38)
#define AHB_CTL0                (AHB_REG_BASE + 0x00)
#define AHB_CTL1                (AHB_REG_BASE + 0x04)
#define AHB_CTL2                (AHB_REG_BASE + 0x08)
#define AHB_CTL3                (AHB_REG_BASE + 0x0C)
#define AHB_ARM_CLK             (AHB_REG_BASE + 0x24)

#define GR_GEN4                         (GREG_BASE + 0x0060)
#define GR_CLK_GEN5				(GREG_BASE + 0x007C)
#define GR_CLK_DLY                      (GREG_BASE + 0x005C)
#define GR_GEN2                         (GREG_BASE + 0x002C)
#define GR_GEN1                         (GREG_BASE + 0x0018)
#define GR_PCTL                         (GREG_BASE + 0x000C)
#define GR_CLK_EN                       (GREG_BASE + 0x0074)
#define GR_MPLL_MN                      (GREG_BASE + 0x0024)
#define GR_DPLL_CTRL                    (GREG_BASE + 0x0040)

/* control registers. */
#define 	PLL_SCR		GR_PLL_SCR
#define 	GEN0		GR_GEN0
#define 	GEN1		GR_GEN1
#define 	GEN2		GR_GEN2
#define 	GEN3		GR_GEN3
#define 	GEN4		GR_GEN4
#define 	GEN5		GR_CLK_GEN5
#define	PLLMN_CTRL	GR_MPLL_MN
#define	CLK_DLY		GR_CLK_DLY
#define	PCTRL		GR_PCTL
#define	CLK_EN		GR_CLK_EN

/* register bit field for PLL_SCR */
#define	 TEST_CLK_CCIRPLL_SEL_MASK		(0x3UL << 18)
#define	 TEST_CLK_AUX1PLL_SEL_MASK		(0x3UL << 12)
#define	 TEST_CLK_AUX0PLL_SEL_MASK		(0x3UL << 10)
#define	 TEST_CLK_IISPLL_SEL_MASK		(0x3UL << 8)
#define	 TEST_CLK_LCDPLL_SEL_MASK		(0x3UL << 6)
#define	 TEST_CLK_DCAMPLL_SEL_MASK		(0x3UL << 4)
#define	 TEST_CLK_VSPMPLL_SEL_MASK		(0x3UL << 2)

/* register bit field for GEN0 */
#define	TEST_RTC_TIMER_EN_SHIFT				28
#define	TEST_RTC_SYS_TIMER_EN_SHIFT			27
#define	TEST_RTC_KEYPAD_EN_SHIFT			26
#define	TEST_RTC_GPIO_EN_SHIFT				24
#define	TEST_CLK_VOICE_BAND_EN_SHIFT		23
#define	TEST_CLK_UART2_EN_SHIFT				22
#define	TEST_CLK_UART1_EN_SHIFT				21
#define	TEST_CLK_UART0_EN_SHIFT				20
#define	TEST_CLK_SYS_TIMER_EN_SHIFT			19
#define	TEST_CLK_SPI_EN_SHIFT				17
#define	TEST_CLK_SIM1_EN_SHIFT				16
#define	TEST_CLK_EPT_EN_SHIFT				15
#define	TEST_CLK_CCIR_MCLK_EN_SHIFT			14
#define	TEST_CLK_PIN_REG_EN_SHIFT			13
#define	TEST_CLK_IIS_EN_SHIFT				12
#define	TEST_CLK_KEYPAD_EN_SHIFT			8
#define	TEST_CLK_EFUSE_EN_SHIFT				7
//#define	TEST_CLK_ADI_EN_SHIFT				6
#define	TEST_CLK_GPIO_EN_SHIFT				5
#define	TEST_CLK_I2C_EN_SHIFT				4
#define	TEST_CLK_SIM0_EN_SHIFT				3
#define	TEST_CLK_TIMER_EN_SHIFT				2

/* register bit field for GEN1 */
#define	TEST_CLK_AUX1_EN_SHIFT			11
#define	TEST_CLK_AUX0_EN_SHIFT			10
#define	TEST_CLK_AUX0_DIV_MASK			(0xffUL << 0)


/* register bit field for GEN2 */
#define	TEST_CLK_IIS_DIV_MASK			(0xffUL << 24)
#define	TEST_CLK_SPI_DIV_MASK			(0x7UL << 21)


/* register bit field for GEN3 */
#define	TEST_CLK_CCIR_MCLK_DIV_MASK		(0x3UL << 24)

/* register bit field for GEN4 */
#define	TEST_CLK_LCDC_DIV_MASK		(0x7UL << 0)

/* register bit field for GEN5 */
#define	 TEST_CLK_SDIO0PLL_SEL_MASK		(0x3UL << 17)
#define	 TEST_CLK_SDIO1PLL_SEL_MASK		(0x3UL << 19)

#define	 TEST_CLK_UART2_DIV_MASK		(0x7UL << 6)
#define	 TEST_CLK_UART1_DIV_MASK		(0x7UL << 3)
#define	 TEST_CLK_UART0_DIV_MASK		(0x7UL << 0)


/* register bit field for AHB_CTRL0 */
#define	TEST_CLK_EMC_EN_SHIFT			28
#define	TEST_CLK_AHB_ARCH_EB_SHIFT		27
#define	TEST_CLK_DRM_EN_SHIFT		16
#define	TEST_CLK_ROT_EN_SHIFT		14
#define	TEST_CLK_VSP_EN_SHIFT		13
#define	TEST_CLK_MON1_EN_SHIFT		11
#define	TEST_CLK_NFC_EN_SHIFT		8
#define	TEST_CLK_MON0_EN_SHIFT		7
#define	TEST_CLK_DMA_EN_SHIFT		6
#define	TEST_CLK_USBD_EN_SHIFT		5
#define	TEST_CLK_SDIO0_EN_SHIFT		4
#define	TEST_CLK_SDIO1_EN_SHIFT		19
#define	TEST_CLK_LCDC_EN_SHIFT		3
#define	TEST_CLK_CCIR_EN_SHIFT		2
#define	TEST_CLK_DCAM_EN_SHIFT		1

/* register bit field for CLK_DLY */
#define	TEST_CLK_ADI_EN_SHIFT		29
#define	TEST_CLK_ADI_SEL_MASK		(0x1UL << 28)
#define	TEST_CLK_SPI_SEL_MASK		(0x3UL << 26)
#define	TEST_CLK_UART2_SEL_MASK		(0x3UL << 24)
#define	TEST_CLK_UART1_SEL_MASK		(0x3UL << 22)
#define	TEST_CLK_UART0_SEL_MASK		(0x3UL << 20)


/* register bit field for PCTRL */
#define	TEST_CLK_AUX1_DIV_MASK			(0xffUL << 22)

/* register bit field for CLK_EN */
#define	TEST_CLK_PWM3_SEL_MASK			(0x1UL << 28)
#define	TEST_CLK_PWM2_SEL_MASK			(0x1UL << 27)
#define	TEST_CLK_PWM1_SEL_MASK			(0x1UL << 26)
#define	TEST_CLK_PWM0_SEL_MASK			(0x1UL << 25)
#define	TEST_CLK_PWM3_EN_SHIFT			24
#define	TEST_CLK_PWM2_EN_SHIFT			23
#define	TEST_CLK_PWM1_EN_SHIFT			22
#define	TEST_CLK_PWM0_EN_SHIFT			21

/* register bit field for AHB_CTRL3 */
#define	TEST_CLK_USB_SEL_MASK			(0x1UL << 0)


struct clk_test_suite {
	char *name;
	int (*test_entry)(void *);
};

static int test_entry_generic(void *data)
{
	return -1;
}

static int clk_divisor_test(struct clk *clk, int max_div,
		volatile void __iomem *div_reg, u32 div_mask)
{
#if 0
	int div;
	int ret;
	u32 div_read;
	u32 div_verify;

	if ((!clk->clkdiv_reg) || (!clk->clkdiv_mask)) {
		div = clk_get_divisor(clk);
		if (1 != div) {
			printk("clock[%s]: no divisor reg, divisor should be 1, but: %d.\n",
						clk->name, div);
			return -EINVAL;
		}
	}
	else {
		for (div = 1; div <= max_div; div++) {
			ret = clk_set_divisor(clk, div);
			if (ret) {
				printk("clock[%s]: clk_set_divisor() failed!\n", clk->name);
				return -EINVAL;
			}
			div_read = clk_get_divisor(clk);
			if (div != div_read) {
				printk("clock[%s]: div_read(%d) != div(%d)!\n",
						clk->name, div_read, div);
				return -EINVAL;
			}
			div_verify = __raw_readl(div_reg);
			div_verify &= div_mask;
			div_verify >>= __ffs(div_mask);
			div_verify += 1;
			if (div_verify != div) {
				printk("clock[%s]: div_verify(%d) != div(%d)!\n",
						clk->name, div_verify, div);
				return -EINVAL;
			}
		}

		//negative test, expected to fail!
		for (div = (max_div + 1); div <= (max_div + 4); div++) {
			ret = clk_set_divisor(clk, div);
			if (!ret) {
				printk("clock[%s]: clk_set_divisor() should failed, but passed!\n", clk->name);
				return -EINVAL;
			}
		}

	}
#endif
	return 0;
}

static int clk_rate_test(struct clk *clk, int max_div,
		volatile void __iomem *div_reg, u32 div_mask)
{
	long rate, rate_step, rate_rounded, max_rate = 0;
	int ret;
	u32 div_verify;
	unsigned long rate_read, rate_verify;

	if (!clk->parent) {
		printk("clock[%s]: has no parent[%s]!\n", clk->name, clk->parent->name);
		return -1;
	}

	max_rate = clk->parent->rate;
	printk("clock[%s]: max rate = %ld.\n", clk->name, max_rate);

	rate_step = max_rate / 20;
	for (rate = rate_step; rate < (max_rate + 4 * rate_step); rate = (rate + rate_step)) {
		rate_rounded = clk_round_rate(clk, rate);
		if (rate_rounded < 0) {
			/*
			printk("clock[%s]: failed to round rate[%ld].\n", clk->name, rate);
			*/
			continue;
		}
		else {
			printk("clock[%s]: rate = %ld, rate_rounded = %ld\n", 
				clk->name, rate, rate_rounded);
		}

		/* check clk_set_rate(), positive test. */
		ret = clk_set_rate(clk, rate_rounded);
		if (ret) {
			printk("clock[%s]: failed to clk_set_rate(%ld).\n", clk->name, rate_rounded);
			return -1;
		}
		rate_read = clk_get_rate(clk);
		if (rate_read != rate_rounded) {
			printk("clock[%s]: failed to clk_get_rate(), want[%ld], but[%lu]\n", 
					clk->name, rate_rounded, rate_read);
			return -1;
		}
		/* verify rate. */
		if ((!div_reg) || (!div_mask)) goto neg_test;

		div_verify = __raw_readl(div_reg);
		div_verify &= div_mask;
		div_verify >>= __ffs(div_mask);
		div_verify += 1;

		rate_verify = clk->parent->rate / div_verify;
		if (rate_verify != rate_rounded) {
			printk("clock[%s]: failed to verify rate, want[%ld], but[%lu]\n", 
					clk->name, rate_rounded, rate_verify);
			return -EINVAL;
		}
neg_test:
		/* clk_set_rate(), negative test. */
		ret = clk_set_rate(clk, rate_rounded + 1);
		if (!ret) {
			printk("clock[%s]: clk_set_rate(%ld) should fail.\n", 
					clk->name, rate_rounded);
			return -1;
		}
	}

	return 0;
}

static int test_entry_ccir_mclk(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)PLL_SCR;
	volatile void __iomem *div_reg = (volatile void __iomem *)GEN3;
	volatile void __iomem *en_reg = (volatile void __iomem *)GEN0;
	u32 sel_mask = TEST_CLK_CCIRPLL_SEL_MASK;
	u32 div_mask = TEST_CLK_CCIR_MCLK_DIV_MASK;
	u32 en_shift = TEST_CLK_CCIR_MCLK_EN_SHIFT;
	int max_divisor = 4;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}
#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);


		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "clk_76m800k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	/*source 3: */
	name_parent = "clk_48m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);



	/*source 4: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	//negative test, expected to fail
	name_parent = "clk_128m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}


static int test_entry_clk_ccir(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *en_reg = (volatile void __iomem *)AHB_CTL0;
	u32 en_shift = TEST_CLK_CCIR_EN_SHIFT;


	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_ccir_pad";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: parent-fixed clock, can't change parent!", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent [%s]: passed!\n", clk->name, clk_parent->name);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}
#if 0
		/* set & get rate, positive test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		/* can't chage rate, so restore target_rate. */
		target_rate = clk_parent->rate;
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 1, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	//negative test, expected to fail
	name_parent = "clk_128m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}


static int test_entry_clk_dcam(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)PLL_SCR;
	volatile void __iomem *en_reg = (volatile void __iomem *)AHB_CTL0;
	u32 sel_mask = TEST_CLK_DCAMPLL_SEL_MASK;
	u32 en_shift = TEST_CLK_DCAM_EN_SHIFT;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_128m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}
#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);



	/*source 2: */
	name_parent = "clk_76m800k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 3: */
	name_parent = "clk_64m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);



	/*source 4: */
	name_parent = "clk_48m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);



	//negative test, expected to fail
	name_parent = "clk_24m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}


static int test_entry_clk_vsp(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)PLL_SCR;
	volatile void __iomem *en_reg = (volatile void __iomem *)AHB_CTL0;
	u32 sel_mask = TEST_CLK_VSPMPLL_SEL_MASK;
	u32 en_shift = TEST_CLK_VSP_EN_SHIFT;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "l3_153m600k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}
#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "clk_128m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}
#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	/*source 3: */
	name_parent = "clk_64m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 4: */
	name_parent = "clk_48m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	//negative test, expected to fail
	name_parent = "clk_24m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}


static int test_entry_clk_lcdc(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)PLL_SCR;
	volatile void __iomem *div_reg = (volatile void __iomem *)GEN4;
	volatile void __iomem *en_reg = (volatile void __iomem *)AHB_CTL0;
	u32 sel_mask = TEST_CLK_LCDPLL_SEL_MASK;
	u32 div_mask = TEST_CLK_LCDC_DIV_MASK;
	u32 en_shift = TEST_CLK_LCDC_EN_SHIFT;
	int max_divisor = 8;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);



	/*source 2: */
	name_parent = "clk_64m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 3: */
	name_parent = "clk_12m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 4: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}
#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	//negative test, expected to fail
	name_parent = "clk_76m800k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}


static int test_entry_clk_sdio0(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)GEN5;
	volatile void __iomem *en_reg = (volatile void __iomem *)AHB_CTL0;
	u32 sel_mask = TEST_CLK_SDIO0PLL_SEL_MASK;
	u32 en_shift = TEST_CLK_SDIO0_EN_SHIFT;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	/*source 2: */
	name_parent = "clk_64m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}
#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	/*source 3: */
	name_parent = "clk_48m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 4: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif
	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	//negative test, expected to fail
	name_parent = "l3_256m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}



static int test_entry_clk_sdio1(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)GEN5;
	volatile void __iomem *en_reg = (volatile void __iomem *)AHB_CTL0;
	u32 sel_mask = TEST_CLK_SDIO1PLL_SEL_MASK;
	u32 en_shift = TEST_CLK_SDIO1_EN_SHIFT;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "clk_64m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}
#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 3: */
	name_parent = "clk_48m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}
#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	/*source 4: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	//negative test, expected to fail
	name_parent = "l3_256m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}


static int test_entry_clk_uart0(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)CLK_DLY;
	volatile void __iomem *div_reg = (volatile void __iomem *)GEN5;
	volatile void __iomem *en_reg = (volatile void __iomem *)GEN0;
	u32 sel_mask = TEST_CLK_UART0_SEL_MASK;
	u32 div_mask = TEST_CLK_UART0_DIV_MASK;
	u32 en_shift = TEST_CLK_UART0_EN_SHIFT;
	int max_divisor = 8;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "clk_51m200k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!\n", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	/*source 3: */
	name_parent = "clk_48m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	/*source 4: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	//negative test, expected to fail
	name_parent = "clk_10m240k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}

static int test_entry_clk_uart1(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)CLK_DLY;
	volatile void __iomem *div_reg = (volatile void __iomem *)GEN5;
	volatile void __iomem *en_reg = (volatile void __iomem *)GEN0;
	u32 sel_mask = TEST_CLK_UART1_SEL_MASK;
	u32 div_mask = TEST_CLK_UART1_DIV_MASK;
	u32 en_shift = TEST_CLK_UART1_EN_SHIFT;
	int max_divisor = 8;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "clk_51m200k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!\n", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 3: */
	name_parent = "clk_48m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 4: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);



	//negative test, expected to fail
	name_parent = "tdpll_ck";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}

static int test_entry_clk_uart2(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)CLK_DLY;
	volatile void __iomem *div_reg = (volatile void __iomem *)GEN5;
	volatile void __iomem *en_reg = (volatile void __iomem *)GEN0;
	u32 sel_mask = TEST_CLK_UART2_SEL_MASK;
	u32 div_mask = TEST_CLK_UART2_DIV_MASK;
	u32 en_shift = TEST_CLK_UART2_EN_SHIFT;
	int max_divisor = 8;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "clk_51m200k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!\n", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 3: */
	name_parent = "clk_48m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 4: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	//negative test, expected to fail
	name_parent = "tdpll_ck";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}


static int test_entry_clk_spi(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)CLK_DLY;
	volatile void __iomem *div_reg = (volatile void __iomem *)GEN2;
	volatile void __iomem *en_reg = (volatile void __iomem *)GEN0;
	u32 sel_mask = TEST_CLK_SPI_SEL_MASK;
	u32 div_mask = TEST_CLK_SPI_DIV_MASK;
	u32 en_shift = TEST_CLK_SPI_EN_SHIFT;
	int max_divisor = 8;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "l3_192m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "l3_153m600k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!\n", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 3: */
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 4: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	//negative test, expected to fail
	name_parent = "tdpll_ck";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}



static int test_entry_clk_aux0(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)PLL_SCR;
	volatile void __iomem *div_reg = (volatile void __iomem *)GEN1;
	volatile void __iomem *en_reg = (volatile void __iomem *)GEN1;
	u32 sel_mask = TEST_CLK_AUX0PLL_SEL_MASK;
	u32 div_mask = TEST_CLK_AUX0_DIV_MASK;
	u32 en_shift = TEST_CLK_AUX0_EN_SHIFT;
	int max_divisor = 256;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "clk_76m800k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!\n", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 3: */
	name_parent = "ext_32k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			//return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 4: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);



	//negative test, expected to fail
	name_parent = "tdpll_ck";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}

static int test_entry_clk_aux1(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)PLL_SCR;
	volatile void __iomem *div_reg = (volatile void __iomem *)PCTRL;
	volatile void __iomem *en_reg = (volatile void __iomem *)GEN1;
	u32 sel_mask = TEST_CLK_AUX1PLL_SEL_MASK;
	u32 div_mask = TEST_CLK_AUX1_DIV_MASK;
	u32 en_shift = TEST_CLK_AUX1_EN_SHIFT;
	int max_divisor = 256;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "clk_76m800k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!\n", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 3: */
	name_parent = "ext_32k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			//return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 4: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);



	//negative test, expected to fail
	name_parent = "tdpll_ck";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}

static int test_entry_clk_iis(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)PLL_SCR;
	volatile void __iomem *div_reg = (volatile void __iomem *)GEN2;
	volatile void __iomem *en_reg = (volatile void __iomem *)GEN0;
	u32 sel_mask = TEST_CLK_IISPLL_SEL_MASK;
	u32 div_mask = TEST_CLK_IIS_DIV_MASK;
	u32 en_shift = TEST_CLK_IIS_EN_SHIFT;
	int max_divisor = 256;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_128m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!\n", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "clk_51m200k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!\n", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);


	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 3: */
	name_parent = "clk_iis_pad";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x02;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			//return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	/*source 4: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x03;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < max_divisor; i++, v_wanted++) {
		target_rate = clk_parent->rate / (i + 1);
		ret_val = clk_set_rate(clk, target_rate);
		if (ret_val) {
			printk("clock[%s]: clk_set_rate() failed!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
		v = __raw_readl(div_reg);
		v &= div_mask;
		v >>= __ffs(div_mask);
		if (v_wanted != v) {
			printk("clock[%s]: failed to verify rate: wanted : %08x, but: %08x\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, max_divisor, div_reg, div_mask);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);



	//negative test, expected to fail
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}

static int test_entry_clk_adi_m(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)CLK_DLY;
	volatile void __iomem *en_reg = (volatile void __iomem *)CLK_DLY;
	u32 sel_mask = TEST_CLK_ADI_SEL_MASK;
	u32 en_shift = TEST_CLK_ADI_EN_SHIFT;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_76m800k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);



	/*source 2: */
	name_parent = "clk_51m200k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);


	//negative test, expected to fail
	name_parent = "clk_24m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}



static int test_entry_clk_pwm0(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)CLK_EN;
	volatile void __iomem *en_reg = (volatile void __iomem *)CLK_EN;
	u32 sel_mask = TEST_CLK_PWM0_SEL_MASK;
	u32 en_shift = TEST_CLK_PWM0_EN_SHIFT;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "ext_32k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}
#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	//negative test, expected to fail
	name_parent = "clk_24m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}



static int test_entry_clk_pwm1(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)CLK_EN;
	volatile void __iomem *en_reg = (volatile void __iomem *)CLK_EN;
	u32 sel_mask = TEST_CLK_PWM1_SEL_MASK;
	u32 en_shift = TEST_CLK_PWM1_EN_SHIFT;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "ext_32k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	//negative test, expected to fail
	name_parent = "clk_24m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}



static int test_entry_clk_pwm2(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)CLK_EN;
	volatile void __iomem *en_reg = (volatile void __iomem *)CLK_EN;
	u32 sel_mask = TEST_CLK_PWM2_SEL_MASK;
	u32 en_shift = TEST_CLK_PWM2_EN_SHIFT;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "ext_32k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	//negative test, expected to fail
	name_parent = "clk_24m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}


static int test_entry_clk_pwm3(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)CLK_EN;
	volatile void __iomem *en_reg = (volatile void __iomem *)CLK_EN;
	u32 sel_mask = TEST_CLK_PWM3_SEL_MASK;
	u32 en_shift = TEST_CLK_PWM3_EN_SHIFT;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "ext_32k";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "ext_26m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}
#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	//negative test, expected to fail
	name_parent = "clk_24m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}


static int test_entry_clk_usb_ref(void *data)
{
	struct clk *clk = (struct clk *)data;
	struct clk *clk_parent;
	char *name_parent;
	u32 v, v_wanted;
	int ret_val;
	int i;
	int target_rate, read_rate, recalc_rate;
	volatile void __iomem *sel_reg = (volatile void __iomem *)AHB_CTL3;
	volatile void __iomem *en_reg = (volatile void __iomem *)AHB_CTL0;
	u32 sel_mask = TEST_CLK_USB_SEL_MASK;
	u32 en_shift = TEST_CLK_USBD_EN_SHIFT;

	/************************************************/
	/* case[00]: init enable--> */
	/***********************************************/
	if (!(clk->flags & ENABLE_ON_INIT) && (clk->usecount != 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else if ((clk->flags & ENABLE_ON_INIT) && (clk->usecount == 0)) {
		printk("clock[%s]: \"init enable\" failed!\n", clk->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: \"init enable\" passed!\n", clk->name);
	}
	/****************************************************/
	/* case[01]: set parent--> */
	/* case[02]: get parent--> */
	/****************************************************/
	/***************************************************************/
	/* case[04]: set rate--> */
	/* case[05]: round rate--> */
	/* case[06]: get rate--> */
	/***************************************************************/


	/*source 1: */
	name_parent = "clk_12m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x00;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);
		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);


	/*source 2: */
	name_parent = "clk_24m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (ret_val) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk->name);
		return -EINVAL;
	}
	v = __raw_readl(sel_reg);
	//printk("verify: v = %08x\n", v);
	v &= sel_mask;
	v >>= __ffs(sel_mask);
	v_wanted = 0x01;
	if (v_wanted != v) {
		printk("clock[%s]: failed to set parent as [%s], wanted : %08x, but: %08x\n",
				clk->name, name_parent, v_wanted, v);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: set parent as [%s]: passed!\n", clk->name, name_parent);
	}


	clk_parent = clk_get_parent(clk);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get_parent()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}
	if (strcmp(clk_parent->name, name_parent)) {
		printk("clock[%s]: clk_get_parent() failed: want parent: [%s], but get: [%s]\n",
				clk->name, name_parent, clk_parent->name);
		return -EINVAL;
	}
	else {
		printk("clock[%s]: get parent [%s]: passed!\n", clk->name, clk_parent->name);
	}

#if 0
		/* set & get rate, positive test; */
	for (v_wanted = 0x00,i = 0; i < 4; i++, v_wanted++) {
		target_rate = clk_parent->rate;
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: rate-fixed clock, can't change rate!", clk->name);
			return -EINVAL;
		}
		read_rate = clk_get_rate(clk);
		if (read_rate != target_rate) {
			printk("clock[%s]: rate verification failed: read_rate = %d, target_rate = %d.\n",
					clk->name, read_rate, target_rate);
			return -EINVAL;
		}
		recalc_rate = sc88xx_recalc_generic(clk);
		if (recalc_rate != target_rate) {
			printk("clock[%s]: rate verification failed: recalc_rate = %d, target_rate = %d.\n",
					clk->name, recalc_rate, target_rate);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(positive): passed!\n", clk->name);

		/* set & get rate, negative test; */
	for (i = 0; i < 4; i++) {
		target_rate = clk_parent->rate / (i + 1) + (1 * (i + 1));
		ret_val = clk_set_rate(clk, target_rate);
		if (!ret_val) {
			printk("clock[%s]: clk_set_rate() should failed, but passed!\n", clk->name);
			return -EINVAL;
		}
	}
	printk("clock[%s]: set rate(negative): passed!\n", clk->name);
#endif

	ret_val = clk_divisor_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_divisor_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_divisor_test() passed!\n", clk->name);

	ret_val = clk_rate_test(clk, 4, NULL, 0);
	if (ret_val) {
		printk("clock[%s]: clk_rate_test() failed!\n", clk->name);
		return -EINVAL;
	}
	printk("clock[%s]: clk_rate_test() passed!\n", clk->name);

	//negative test, expected to fail
	name_parent = "clk_96m";
	clk_parent = clk_get(NULL, name_parent);
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
				clk->name, name_parent);
		return -EINVAL;
	}

	ret_val = clk_set_parent(clk, clk_parent);
	if (!ret_val) {
		printk("clock[%s]: negative test, should failed, but passed!", clk->name);
		return -EINVAL;
	}


	/***************************************************************/
	/* case[03]: clock enable--> */
	/* case[04]: clock disable--> */
	/***************************************************************/
	ret_val = clk_enable(clk);
	if (ret_val) {
		printk("clock[%s]: clk_enable() failed!\n", clk->name);
	}

	v_wanted = 0x01;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_enable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);
	clk_enable(clk);

	for (i = 0; i < 4; i++) {
		clk_disable(clk);
		v_wanted = 0x01;
		v = __raw_readl(en_reg);
		v &= (0x1UL << en_shift);
		v >>= en_shift;
		if (v_wanted != v) {
			printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
					clk->name, v_wanted, v);
			return -EINVAL;
		}
	}

	clk_disable(clk);
	v_wanted = 0x00;
	v = __raw_readl(en_reg);
	v &= (0x1UL << en_shift);
	v >>= en_shift;
	if (v_wanted != v) {
		printk("clock[%s]: case[clk_disable] failed, wanted: %08x, but: %08x!\n",
				clk->name, v_wanted, v);
		return -EINVAL;
	}

	return 0;
}



static struct clk_test_suite clocks[] = {
		{"ccir_mclk", &test_entry_ccir_mclk},
		{"clk_ccir", &test_entry_clk_ccir},
		{"clk_dcam", &test_entry_clk_dcam},
		{"clk_vsp", &test_entry_clk_vsp},
		{"clk_lcdc", &test_entry_clk_lcdc},
		{"clk_sdio0", &test_entry_clk_sdio0},
		{"clk_sdio1", &test_entry_clk_sdio1},
		{"clk_uart0", &test_entry_clk_uart0},
		{"clk_uart1", &test_entry_generic},
		{"clk_uart2", &test_entry_clk_uart2},
		{"clk_spi", &test_entry_clk_spi},
		{"clk_iis", &test_entry_clk_iis},
		{"clk_adi_m", &test_entry_generic},
		{"clk_aux0", &test_entry_clk_aux0},
		{"clk_aux1", &test_entry_clk_aux1},
		{"clk_pwm0", &test_entry_clk_pwm0},
		{"clk_pwm1", &test_entry_clk_pwm1},
		{"clk_pwm2", &test_entry_clk_pwm2},
		{"clk_pwm3", &test_entry_clk_pwm3},
		{"clk_usb_ref", &test_entry_clk_usb_ref},
};


static int test_thread(void *pdata)
{
	int i;
	int clock_conters = ARRAY_SIZE(clocks);
	int ret_val;
	struct clk *clk_tested;
/*
	sclk = clk_get(NULL, "sys_ck");
	clk_get_rate(sclk);
	clk_set_rate(sclk, 400000);
	clk_round_rate(sclk, 400000);
	clk_get_sys(NULL, "ext_26m");
	clk_add_alias("ext_26m_alias", NULL, "ext_26m", NULL);
	clk_set_parent(sclk, sclk);
	clk_get_parent(sclk);
	clk_enable(sclk);
	clk_disable(sclk);
	clk_put(sclk);
*/
	for (i = 0; i < clock_conters; i++) {
		printk(" ######### Running cases for clock [%s]...... #########\n", clocks[i].name);
		clk_tested = clk_get(NULL, clocks[i].name);

		if (IS_ERR(clk_tested)) {
			printk("####: Failed: Can't get clock [%s]!\n", clocks[i].name);
			printk("####: clk_tested = %p\n", clk_tested);
		}
		else {
			ret_val = clocks[i].test_entry(clk_tested);
			if (ret_val)
				printk("==================== clock [%s]: failed! ===================\n", clocks[i].name);
			else
				printk("==================== clock [%s]: all passed! ======================\n", clocks[i].name);
		}
	}

	return 0;
}

static int __init sc8800g2_clk_arch_init(void)
{
/*
	pid_t pid_number;

	pid_number = kernel_thread(test_thread, NULL, 0);
	if (pid_number < 0) {
		printk("Can't crate test thread!\n");
		return -EINVAL;
	}
*/
	test_thread(NULL);
	return 0;
}
#define SPRD_CLK_FM_TEST 1
#ifdef SPRD_CLK_FM_TEST
device_initcall(sc8800g2_clk_arch_init);
#endif

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

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <mach/globalregs.h>
#include <asm/irqflags.h>
#include "clock_common.h"
#include "clock_sc8810.h"
#include <mach/adi.h>
#include <linux/io.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <mach/pm_debug.h>

extern int sp_pm_collapse(void);
extern void sp_pm_collapse_exit(void);
extern void sc8810_standby_iram(void);
extern void sc8810_standby_iram_end(void);
extern void sc8810_standby_exit_iram(void);
extern void l2x0_suspend(void);
extern void l2x0_resume(int collapsed);

/*init (arm gsm td mm) auto power down */
#define CHIP_ID_VER_0		(0x88100000UL)
#define CHIP_ID_VER_MF		(0x88100040UL)
#define PD_AUTO_EN (1<<22)
#define AHB_REG_BASE (SPRD_AHB_BASE+0x200)
#define CHIP_ID_ADR	(AHB_REG_BASE + 0x1FC)

static void setup_autopd_mode(void)
{
	sprd_greg_write(REG_TYPE_GLOBAL, 0x06000320|PD_AUTO_EN, GR_MM_PWR_CTRL); /*MM*/
	sprd_greg_write(REG_TYPE_GLOBAL, 0x06000320|PD_AUTO_EN, GR_G3D_PWR_CTRL);/*GPU*/
	sprd_greg_write(REG_TYPE_GLOBAL, 0x04000720/*|PD_AUTO_EN*/, GR_CEVA_RAM_TH_PWR_CTRL);
	sprd_greg_write(REG_TYPE_GLOBAL, 0x05000520/*|PD_AUTO_EN*/, GR_GSM_PWR_CTRL);/*GSM*/
	sprd_greg_write(REG_TYPE_GLOBAL, 0x05000520/*|PD_AUTO_EN*/, GR_TD_PWR_CTRL);/*TD*/
	sprd_greg_write(REG_TYPE_GLOBAL, 0x04000720/*|PD_AUTO_EN*/, GR_CEVA_RAM_BH_PWR_CTRL);
	sprd_greg_write(REG_TYPE_GLOBAL, 0x03000920/*|PD_AUTO_EN*/, GR_PERI_PWR_CTRL);
	if (__raw_readl(CHIP_ID_ADR) == CHIP_ID_VER_0) {/*original version*/
		sprd_greg_write(REG_TYPE_GLOBAL, 0x02000a20|PD_AUTO_EN, GR_ARM_SYS_PWR_CTRL);
		sprd_greg_write(REG_TYPE_GLOBAL, 0x07000f20|(1<<23), GR_POWCTL0);
	}
	else {
		sprd_greg_write(REG_TYPE_GLOBAL, 0x02000f20|PD_AUTO_EN, GR_ARM_SYS_PWR_CTRL);
		sprd_greg_write(REG_TYPE_GLOBAL, 0x07000a20|(1<<23), GR_POWCTL0);
	}
}

void check_pd(void)
{
#define CHECK_PD(_type, _val, _reg) { \
	val = sprd_greg_read(_type, _reg); \
	if(val != (_val))	\
		printk("### setting not same:"#_reg" = %08x !=%08x\n", val, (_val)); \
	}
	unsigned int val;
	CHECK_PD(REG_TYPE_GLOBAL, 0x06000320|PD_AUTO_EN, GR_MM_PWR_CTRL);
	CHECK_PD(REG_TYPE_GLOBAL, 0x06000320|PD_AUTO_EN, GR_G3D_PWR_CTRL);
	CHECK_PD(REG_TYPE_GLOBAL, 0x04000720, GR_CEVA_RAM_TH_PWR_CTRL);
	CHECK_PD(REG_TYPE_GLOBAL, 0x05000520, GR_GSM_PWR_CTRL);
	CHECK_PD(REG_TYPE_GLOBAL, 0x05000520, GR_TD_PWR_CTRL);
	CHECK_PD(REG_TYPE_GLOBAL, 0x04000720, GR_CEVA_RAM_BH_PWR_CTRL);
	CHECK_PD(REG_TYPE_GLOBAL, 0x03000920, GR_PERI_PWR_CTRL);
	if (__raw_readl(CHIP_ID_ADR) == CHIP_ID_VER_0) {
		printk("####:original version\n");
		CHECK_PD(REG_TYPE_GLOBAL, 0x02000a20|PD_AUTO_EN, GR_ARM_SYS_PWR_CTRL);
		CHECK_PD(REG_TYPE_GLOBAL, 0x07000f20|(1<<23), GR_POWCTL0);
	}else{
		printk("####:next version\n");
		CHECK_PD(REG_TYPE_GLOBAL, 0x02000f20|PD_AUTO_EN, GR_ARM_SYS_PWR_CTRL);
		CHECK_PD(REG_TYPE_GLOBAL, 0x07000a20|(1<<23), GR_POWCTL0);
	}
}

/* FIXME: init led ctrl *we have no driver of led, just init here*/
#define SPRD_ANA_BASE	(SPRD_MISC_BASE + 0x600)
#define ANA_REG_BASE	SPRD_ANA_BASE
#define ANA_LED_CTRL	(ANA_REG_BASE + 0x68)

#define   ANA_LDO_SLP0           (ANA_REG_BASE + 0x2C)
#define   ANA_LDO_SLP1           (ANA_REG_BASE + 0x30)
#define   ANA_LDO_SLP2           (ANA_REG_BASE + 0x34)
#define   ANA_DCDC_CTRL         (ANA_REG_BASE + 0x38)
#define   ANA_DCDC_CTRL_DS		(ANA_REG_BASE + 0x3C)

static void init_led(void)
{
	/*all led off*/
	sci_adi_raw_write(ANA_LED_CTRL, 0x801f);
}

void check_ldo(void)
{
#define CHECK_LDO(_reg, _val) { \
	val = sci_adi_read(_reg); \
	if(val != (_val)) printk("### setting not same:"#_reg" = %08x !=%08x\n", val, (_val)); \
	}
	unsigned int val;
	CHECK_LDO(ANA_LDO_SLP0, 0x26f3);
	CHECK_LDO(ANA_LDO_SLP1, 0x8019|(1<<12));
	CHECK_LDO(ANA_LDO_SLP2, 0x0f20);
#if 0
	CHECK_LDO(ANA_DCDC_CTRL, 0x0025);
	CHECK_LDO(ANA_DCDC_CTRL_DS, 0x0f43);
	CHECK_LDO(ANA_LED_CTRL, 0x801f);
#endif
}

/*copy code for deepsleep return */
#define SAVED_VECTOR_SIZE 64
static uint32_t *sp_pm_reset_vector = NULL;
static uint32_t saved_vector[SAVED_VECTOR_SIZE];
void __iomem *iram_start;

#define IRAM_BASE_PHY   0xFFFF0000
#define IRAM_START_PHY 	0xFFFF4000
#define IRAM_SIZE 0x4000
#define SLEEP_CODE_SIZE 4096

int init_reset_vector(void)
{
	sprd_greg_write(REG_TYPE_AHB_GLOBAL, 0x1|sprd_greg_read(REG_TYPE_AHB_GLOBAL, AHB_REMAP), AHB_REMAP);

	if (!sp_pm_reset_vector) {
		sp_pm_reset_vector = ioremap(0xffff0000, PAGE_SIZE);
		if (sp_pm_reset_vector == NULL) {
			printk(KERN_ERR "sp_pm_init: failed to map reset vector\n");
			return 0;
		}
	}

	iram_start = (void __iomem *)(SPRD_IRAM_BASE);
	/* copy sleep code to IRAM. */
	if ((sc8810_standby_iram_end - sc8810_standby_iram + 128) > SLEEP_CODE_SIZE) {
		panic("##: code size is larger than expected, need more memory!\n");
	}

	memcpy_toio(iram_start, sc8810_standby_iram, SLEEP_CODE_SIZE);

	/* just make sure*/
	flush_cache_all();
	outer_flush_all();
	return 0;
}

static void save_reset_vector(void)
{
	int i = 0;
	for (i = 0; i < SAVED_VECTOR_SIZE; i++)
		saved_vector[i] = sp_pm_reset_vector[i];
}

static void set_reset_vector(void)
{
	int i = 0;
	for (i = 0; i < SAVED_VECTOR_SIZE; i++)
		sp_pm_reset_vector[i] = 0xe320f000; /* nop*/

	sp_pm_reset_vector[SAVED_VECTOR_SIZE - 2] = 0xE51FF004; /* ldr pc, 4 */

	sp_pm_reset_vector[SAVED_VECTOR_SIZE - 1] = (sc8810_standby_exit_iram -
		sc8810_standby_iram + IRAM_START_PHY);
}

static void restore_reset_vector(void)
{
	int i;
	for (i = 0; i < SAVED_VECTOR_SIZE; i++)
		sp_pm_reset_vector[i] = saved_vector[i];
}

/* irq functions */
#define hw_raw_irqs_disabled_flags(flags)	\
({										\
	(int)((flags) & PSR_I_BIT);				\
})

#define hw_irqs_disabled()				\
({										\
	unsigned long _flags;					\
	_flags = hw_local_save_flags();		\
	hw_raw_irqs_disabled_flags(_flags);	\
})

u32 __attribute__ ((naked)) sc8810_read_cpsr(void)
{
	__asm__ __volatile__("mrs r0, cpsr\nbx lr");
}

/*make sure adb ahb and audio is complete shut down.*/
#define GEN0_MASK ( GEN0_SIM0_EN | GEN0_I2C_EN | GEN0_GPIO_EN | 			\
			   GEN0_I2C0_EN|GEN0_I2C1_EN|GEN0_I2C2_EN|GEN0_I2C3_EN | 		\
			   GEN0_SPI0_EN|GEN0_SPI1_EN| GEN0_I2S0_EN | GEN0_I2S1_EN| 	\
	                GEN0_EFUSE_EN | GEN0_I2S_EN | GEN0_PIN_EN | 				\
	                GEN0_EPT_EN | GEN0_SIM1_EN | GEN0_SPI_EN | GEN0_UART0_EN | \
	                GEN0_UART1_EN | GEN0_UART2_EN)

#define CLK_EN_MASK (CLK_PWM0_EN | CLK_PWM1_EN | CLK_PWM2_EN | CLK_PWM3_EN)

#define BUSCLK_ALM_MASK (ARM_VB_MCLKON|ARM_VB_DA0ON|ARM_VB_DA1ON|ARM_VB_ADCON|ARM_VB_ANAON|ARM_VB_ACC)

#define AHB_CTL0_MASK   (AHB_CTL0_DCAM_EN|AHB_CTL0_CCIR_EN|AHB_CTL0_LCDC_EN|    \
                         AHB_CTL0_SDIO0_EN|AHB_CTL0_SDIO1_EN|AHB_CTL0_DMA_EN|     \
                         AHB_CTL0_BM0_EN |AHB_CTL0_NFC_EN|AHB_CTL0_BM1_EN|       \
                         AHB_CTL0_G2D_EN|AHB_CTL0_G3D_EN|	\
                         AHB_CTL0_AXIBUSMON0_EN|AHB_CTL0_AXIBUSMON1_EN|	\
                         AHB_CTL0_VSP_EN|AHB_CTL0_ROT_EN | AHB_CTL0_USBD_EN)

#define GR_CLK_EN_MASK CLK_EN_MASK
#define GR_GEN0_MASK GEN0_MASK

static void disable_audio_module(void)
{
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, BUSCLK_ALM_MASK, GR_BUSCLK_ALM);
}

static void disable_apb_module(void)
{
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, GEN0_MASK, GR_GEN0);
	sprd_greg_clear_bits(REG_TYPE_GLOBAL, CLK_EN_MASK, GR_CLK_EN);
}

static void disable_ahb_module (void)
{
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_MASK, AHB_CTL0);
}

#define INT_REG(off) (SPRD_INTCV_BASE + (off))

#define INT_IRQ_STS            INT_REG(0x0000)
#define INT_IRQ_RAW           INT_REG(0x0004)
#define INT_IRQ_ENB           INT_REG(0x0008)
#define INT_IRQ_DIS            INT_REG(0x000c)
#define INT_FIQ_STS            INT_REG(0x0020)

#define INT_IRQ_MASK	(1<<3)


/*save/restore global regs*/
u32 reg_gen_clk_en, reg_gen0_val, reg_busclk_alm, reg_ahb_ctl0_val;

/*register save*/
#define SAVE_GR_REG(_reg_save, _reg_type, _reg_addr, _reg_mask)  \
	{_reg_save = (sprd_greg_read(_reg_type, _reg_addr) & ((u32)_reg_mask));}

#define SAVE_GLOBAL_REG  do{ \
        SAVE_GR_REG(reg_gen_clk_en, REG_TYPE_GLOBAL, GR_CLK_EN, GR_CLK_EN_MASK); \
        SAVE_GR_REG(reg_gen0_val, REG_TYPE_GLOBAL, GR_GEN0, GR_GEN0_MASK);   \
        SAVE_GR_REG(reg_busclk_alm, REG_TYPE_GLOBAL, GR_BUSCLK_ALM, BUSCLK_ALM_MASK);    \
        SAVE_GR_REG(reg_ahb_ctl0_val, REG_TYPE_AHB_GLOBAL, AHB_CTL0, AHB_CTL0_MASK);\
    }while(0)

/*register restore*/
#define RESTORE_GR_REG(_reg_type, _reg_addr, _reg_mask, _reg_save)    do{ \
	sprd_greg_set_bits(_reg_type, _reg_mask & _reg_save, _reg_addr); \
    }while(0)

#define RESTORE_GLOBAL_REG   do{  \
        RESTORE_GR_REG(REG_TYPE_GLOBAL, GR_CLK_EN, GR_CLK_EN_MASK, reg_gen_clk_en);   \
        RESTORE_GR_REG(REG_TYPE_GLOBAL, GR_BUSCLK_ALM, BUSCLK_ALM_MASK, reg_busclk_alm); \
        RESTORE_GR_REG(REG_TYPE_GLOBAL, GR_GEN0, GR_GEN0_MASK, reg_gen0_val); \
        RESTORE_GR_REG(REG_TYPE_AHB_GLOBAL, AHB_CTL0, AHB_CTL0_MASK, reg_ahb_ctl0_val); \
    }while(0)

/* make sure printk is end, if not maybe some messy code  in SERIAL1 output */
#define UART_TRANSFER_REALLY_OVER (0x1UL << 15)
#define UART_STS0 (SPRD_SERIAL1_BASE + 0x08)
#define UART_STS1 (SPRD_SERIAL1_BASE + 0x0c)

static void wait_until_uart1_tx_done(void)
{
	u32 tx_fifo_val;
	u32 really_done = 0;
	u32 timeout = 200;

	/*uart1 owner dsp sel*/
	if (sprd_greg_read(REG_TYPE_GLOBAL, GR_PCTL) & (1<<8)/* UART1_SEL */) return ;

	tx_fifo_val = __raw_readl(UART_STS1);
	tx_fifo_val >>= 8;
	tx_fifo_val &= 0xff;
	while(tx_fifo_val != 0) {
		if (timeout <= 0) break;
		udelay(100);
		tx_fifo_val = __raw_readl(UART_STS1);
		tx_fifo_val >>= 8;
		tx_fifo_val &= 0xff;
		timeout--;
	}

	timeout = 30;
	really_done = __raw_readl(UART_STS0);
	while(!(really_done & UART_TRANSFER_REALLY_OVER)) {
		if (timeout <= 0) break;
		udelay(100);
		really_done = __raw_readl(UART_STS0);
		timeout--;
	}
}

/* arm core sleep*/
static void arm_sleep(void)
{
	cpu_do_idle();
	hard_irq_set();
}

/* arm core & ahp sleep*/
static void mcu_sleep(void)
{
	SAVE_GLOBAL_REG;
	disable_audio_module();
	disable_ahb_module();
	cpu_do_idle();
	hard_irq_set();
	RESTORE_GLOBAL_REG;
}

/* chip sleep*/
static int deep_sleep(void)
{
	u32 val, ret = 0;

	wait_until_uart1_tx_done();

	SAVE_GLOBAL_REG;
	disable_audio_module();
	disable_apb_module();
	disable_ahb_module();
	/*prevent uart1*/
	__raw_writel(INT_IRQ_MASK, INT_IRQ_DIS);

#ifdef CONFIG_CACHE_L2X0
	__raw_writel(1, SPRD_CACHE310_BASE+0xF80/*L2X0_POWER_CTRL*/);
	l2x0_suspend();
#endif

	/*go deepsleep when all PD auto poweroff en*/
	val = sprd_greg_read(REG_TYPE_AHB_GLOBAL, AHB_PAUSE);
	val &= ~(MCU_CORE_SLEEP | MCU_DEEP_SLEEP_EN | APB_SLEEP);
	val |= (MCU_SYS_SLEEP_EN | MCU_DEEP_SLEEP_EN);
	sprd_greg_write(REG_TYPE_AHB_GLOBAL, val, AHB_PAUSE);

	/* set entry when deepsleep return*/
	save_reset_vector();
	set_reset_vector();

	ret = sp_pm_collapse();
	hard_irq_set();
	restore_reset_vector();

	RESTORE_GLOBAL_REG;
	udelay(20);
	if (ret) cpu_init();

#ifdef CONFIG_CACHE_L2X0
	l2x0_resume(ret);
#endif
return ret;
}

/*sleep entry for 8810 */
int sc8810_deep_sleep(void)
{
	int status, ret = 0;
	unsigned long flags, time;

	time = get_sys_cnt();
	if (!hw_irqs_disabled())  {
		flags = sc8810_read_cpsr();
		printk("##: Error(%s): IRQ is enabled(%08lx)!\n",
			 "wakelock_suspend", flags);
	}

	status = sc8810_get_clock_status();
	if (status & DEVICE_AHB)  {
		set_sleep_mode(SLP_MODE_ARM);
		arm_sleep();
	} else if (status & DEVICE_APB) {
		set_sleep_mode(SLP_MODE_MCU);
		mcu_sleep();
	} else {
		set_sleep_mode(SLP_MODE_DEP);
		ret = deep_sleep();
	}
	time_add(get_sys_cnt() - time, ret);
	print_hard_irq_inloop(ret);
	return ret;
}

/*FIXME:should provider a mashesm but now only make keypad & usb as wake source */
#define INT_IRQ_EN				(SPRD_INTCV_BASE + 0x08)
#define ANA_GPIO_IE            (SPRD_MISC_BASE + 0x700 + 0x18)

#define WKAEUP_SRC_KEAPAD   (1<<10)
#define WKAEUP_SRC_RX0      1
#define WAKEUP_SRC_PB		(1<<3)
#define WAKEUP_SRC_CHG		(1<<2)
#define SPRD_EICINT_BASE	(SPRD_EIC_BASE+0x80)

/*init global regs for pm */
static void init_gr(void)
{
	int val;
	/* AHB_PAUSE */
	val = sprd_greg_read(REG_TYPE_AHB_GLOBAL, AHB_PAUSE);
	val &= ~(MCU_CORE_SLEEP | MCU_DEEP_SLEEP_EN | APB_SLEEP);
	val |= (MCU_SYS_SLEEP_EN);
	sprd_greg_write(REG_TYPE_AHB_GLOBAL, val, AHB_PAUSE);

	/* GR_PCTL */
	val = sprd_greg_read(REG_TYPE_GLOBAL, GR_PCTL);
	val |= (MCU_MPLL_EN);
	sprd_greg_write(REG_TYPE_GLOBAL, val, GR_PCTL);

	/* AHB_CTL0 */
	val = sprd_greg_read(REG_TYPE_AHB_GLOBAL, AHB_CTL0);
	val &= ~AHB_CTL0_ROT_EN;
	sprd_greg_write(REG_TYPE_AHB_GLOBAL, val, AHB_CTL0);

	/* AHB_CTL1 */
	val = sprd_greg_read(REG_TYPE_AHB_GLOBAL, AHB_CTL1);
	val |= (AHB_CTRL1_EMC_CH_AUTO_GATE_EN | AHB_CTRL1_EMC_AUTO_GATE_EN |
		AHB_CTRL1_ARM_AUTO_GATE_EN|
		AHB_CTRL1_AHB_AUTO_GATE_EN|
/*		AHB_CTRL1_MCU_AUTO_GATE_EN| */
		AHB_CTRL1_ARM_DAHB_SLEEP_EN|
	        AHB_CTRL1_ARMMTX_AUTO_GATE_EN | AHB_CTRL1_MSTMTX_AUTO_GATE_EN);
	val &= ~AHB_CTRL1_MCU_AUTO_GATE_EN;
	sprd_greg_write(REG_TYPE_AHB_GLOBAL, val, AHB_CTL1);

	/* GR_CLK_EN */
	/* enable XTL auto power down. */
	val = sprd_greg_read(REG_TYPE_GLOBAL, GR_CLK_EN);
	val |= MCU_XTLEN_AUTOPD_EN;
#ifndef CONFIG_NKERNEL
/* add this for dsp small code bug*/
	val &= ~0x20000;
#endif
	sprd_greg_write(REG_TYPE_GLOBAL, val, GR_CLK_EN);
}

void sc8810_pm_init(void)
{
	init_reset_vector();
	init_gr();
	setup_autopd_mode();
	init_led();
	pm_debug_init();
}



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
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <asm/irqflags.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/cacheflush.h>
#include <mach/system.h>
#include <mach/pm_debug.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/regs_ana_glb.h>
#include <mach/globalregs.h>
#include <mach/regs_glb.h>
#include <mach/regs_ahb.h>
#include <mach/adi.h>
#include <mach/irqs.h>
#include <mach/sci.h>
#include <mach/emc_repower.h>

extern int sc8825_get_clock_status(void);
extern void secondary_startup(void);
extern int sp_pm_collapse(unsigned int cpu, unsigned int save_state);
extern void sp_pm_collapse_exit(void);
extern void sc8825_standby_iram(void);
extern void sc8825_standby_iram_end(void);
extern void sc8825_standby_exit_iram(void);
extern void l2x0_suspend(void);
extern void l2x0_resume(int collapsed);
void pm_ana_ldo_config(void);

//#define FORCE_DISABLE_DSP


#define SPRD_IPI_REG(val)               (SPRD_IPI_BASE+val)
#define SPRD_IPI_INT                    SPRD_IPI_REG(0x10)
#define SPRD_IPI_ICLR                   SPRD_IPI_REG(0x14)
#define SPRD_IPI_AP2CP_INT_CTRL         SPRD_IPI_REG(0xB8)
#define SPRD_IPI_CP2AP_INT_CLR          SPRD_IPI_REG(0xBC)


#define AP_ENTER_DEEP_SLEEP	(1 << 1)
#define CORE1_RUN		(1)
/*init (arm gsm td mm) auto power down */
#define CHIP_ID_VER_0		(0x8820D000UL)
#define PD_AUTO_EN 		(1<<22)
#define AHB_REG_BASE 		(SPRD_AHB_BASE+0x200)
#define REG_CHIP_ID		(AHB_REG_BASE + 0x3FC)
#define PPI_CONTEXT_SIZE 	11
#define GIC_CONTEXT_SIZE	67
#define GIC_MASK_ALL		0x0
#define GIC_ISR_NON_SECURE	0xffffffff
#define SPI_ENABLE_SET_OFFSET	0x04
#define PPI_PRI_OFFSET		0x10
#define SPI_PRI_OFFSET		0x20
#define SPI_TARGET_OFFSET	0x20
#define SPI_CONFIG_OFFSET	0x8
#define SCU_CONTEXT_SIZE	7

/* 
 *  Variables to store maximum spi(Shared Peripheral Interrupts)
 * and scu registers.  
 */
static u32 max_spi_irq, max_spi_reg;
static u32 offset;
static void __iomem *gic_dist_base;
static void __iomem *gic_cpu_base;
static DEFINE_PER_CPU(u32[PPI_CONTEXT_SIZE], gic_ppi_context);
u32 gic_context[GIC_CONTEXT_SIZE];
u32 scu_context[SCU_CONTEXT_SIZE];
struct emc_repower_param *repower_param; 
static inline u32 gic_readl(u32 offset, u8 idx)
{
	void __iomem *gic_dist_base = sprd_get_gic_dist_base();
	return __raw_readl(gic_dist_base + offset + 4 * idx);
}

static inline void gic_writel(u32 val, u32 offset, u8 idx)
{
	void __iomem *gic_dist_base = sprd_get_gic_dist_base();
	__raw_writel(val, gic_dist_base + offset + 4 * idx);
}

static void dump_gic_saved_context(void)
{
	u32 i;
	for(i=0; i<GIC_CONTEXT_SIZE; i++){
		printk("gic_context[%u]:0x%x \n", i, gic_context[i]);
	}
}

static void gic_save_context(void)
{
	u8 i;
	u32 val;

	offset= 0;
	/* Save CPU 0 Interrupt Set Enable register */
	val = gic_readl(GIC_DIST_ENABLE_SET, 0);
	gic_context[offset] = val;
	offset++;

	/* Save all SPI Set Enable register */
	for (i = 0; i < max_spi_reg; i++) {
		val = gic_readl(GIC_DIST_ENABLE_SET + SPI_ENABLE_SET_OFFSET, i);
		gic_context[offset] = val;
		offset++;
	}
	
	/* Save SGI priority registers (Software Generated Interrupt) */
	for (i = 0; i < 4; i++) {
		val = gic_readl(GIC_DIST_PRI, i);

		/* Save the priority bits of the Interrupts */
		gic_context[offset] = val;
		offset++;
	}
	/* Save PPI priority registers (Private Peripheral Intterupts) */
	for (i = 0; i < 4; i++) {
		val = gic_readl(GIC_DIST_PRI + PPI_PRI_OFFSET, i);

		/* Save the priority bits of the Interrupts */
		gic_context[offset] = val;
		offset++;
	}
	/* SPI priority registers - 4 interrupts/register */
	for (i = 0; i < (max_spi_irq / 4); i++) {
		val = gic_readl((GIC_DIST_PRI + SPI_PRI_OFFSET), i);
		gic_context[offset] = val;
		offset++;
	}
	
	/* SPI Interrupt Target registers - 4 interrupts/register
	 * ICDIPTR0 ~ ICDIPTR7 are read-only
	 */
	for (i = 0; i < (max_spi_irq / 4); i++) {
		val = gic_readl((GIC_DIST_TARGET + SPI_TARGET_OFFSET), i);
		gic_context[offset] = val;
		offset++;
	}

	/* SPI Interrupt Congigeration eegisters- 16 interrupts/register */
	for (i = 0; i < (max_spi_irq / 16); i++) {
		val = gic_readl((GIC_DIST_CONFIG + SPI_CONFIG_OFFSET), i);
		gic_context[offset] = val;
		offset++;
	}
	offset--;

#ifdef CONFIG_SPRD_PM_DEBUG
	printk("****** %s, save %d gic registers \n ", __func__, offset);
	dump_gic_saved_context();
#endif
	return;
}

static void gic_restore_context(void){
	int i;

	/* SPI Interrupt Congigeration eegisters- 16 interrupts/register */
	for ( i=max_spi_irq/16 - 1; i >= 0; i--) {
		gic_writel(gic_context[offset], (GIC_DIST_CONFIG + SPI_CONFIG_OFFSET), i);
		offset--;
	}

	/* SPI Interrupt Target registers - 4 interrupts/register 
	 * ICDIPTR0 ~ ICDIPTR7 are read-only
	 */
	for (i=max_spi_irq/4 - 1; i >= 0; i--) {
		gic_writel(gic_context[offset], (GIC_DIST_TARGET + SPI_TARGET_OFFSET), i);
		offset--;
	}
	/* SPI priority registers - 4 interrupts/register */
	for (i=max_spi_irq/4 - 1; i >= 0; i--) {		
		gic_writel(gic_context[offset], (GIC_DIST_PRI + SPI_PRI_OFFSET), i);
		offset--;
	}
	/* restore PPI priority registers (Private Peripheral Intterupts) */
	for (i = 3; i >= 0; i--) {		
		gic_writel(gic_context[offset], (GIC_DIST_PRI + PPI_PRI_OFFSET), i);
		offset--;
	}
	/* restore SGI priority registers (Software Generated Interrupt) */
	for (i = 3; i >= 0; i--) {
		gic_writel(gic_context[offset], (GIC_DIST_PRI), i);
		offset--;
	}
	/* restore all SPI Set Enable register */
	for (i = max_spi_reg-1; i >= 0; i--) {		
		gic_writel(gic_context[offset], (GIC_DIST_ENABLE_SET + SPI_ENABLE_SET_OFFSET), i);
		offset--;
	}
	/* restore CPU 0 Interrupt Set Enable register */
	gic_writel(gic_context[offset], (GIC_DIST_ENABLE_SET), i);

#ifdef CONFIG_SPRD_PM_DEBUG
	printk("****** exit %s, restore %d gic registers \n ", __func__, offset);
#endif

	return;
}

static void gic_save_ppi(void)
{
	void __iomem *gic_dist_base = sprd_get_gic_dist_base();
	u32 *context = __get_cpu_var(gic_ppi_context);
	int i = 0;

	/* ICDIPR0 ~ ICDIPR7 */
	context[i++] = readl_relaxed(gic_dist_base + GIC_DIST_PRI);
	context[i++] = readl_relaxed(gic_dist_base + GIC_DIST_PRI + 0x4);
	context[i++] = readl_relaxed(gic_dist_base + GIC_DIST_PRI + 0x8);
	context[i++] = readl_relaxed(gic_dist_base + GIC_DIST_PRI + 0xc);
	context[i++] = readl_relaxed(gic_dist_base + GIC_DIST_PRI + 0x10);
	context[i++] = readl_relaxed(gic_dist_base + GIC_DIST_PRI + 0x14);
	context[i++] = readl_relaxed(gic_dist_base + GIC_DIST_PRI + 0x18);
	context[i++] = readl_relaxed(gic_dist_base + GIC_DIST_PRI + 0x1c);

	/* ICDICFR0 ~ ICDICFR1 */
	context[i++] = readl_relaxed(gic_dist_base + GIC_DIST_CONFIG);
	context[i++] = readl_relaxed(gic_dist_base + GIC_DIST_CONFIG + 0x4);

	/* ICDISER0 */
	context[i++] = readl_relaxed(gic_dist_base + GIC_DIST_ENABLE_SET);

	BUG_ON(i != PPI_CONTEXT_SIZE);
}

static void gic_restore_ppi(void)
{
	void __iomem *gic_dist_base = sprd_get_gic_dist_base();
	u32 *context = __get_cpu_var(gic_ppi_context);
	int i = 0;

	/* ICDIPR0 ~ ICDIPR7 */
	writel_relaxed(context[i++], gic_dist_base + GIC_DIST_PRI);
	writel_relaxed(context[i++], gic_dist_base + GIC_DIST_PRI + 0x4);
	writel_relaxed(context[i++], gic_dist_base + GIC_DIST_PRI + 0x8);
	writel_relaxed(context[i++], gic_dist_base + GIC_DIST_PRI + 0xc);
	writel_relaxed(context[i++], gic_dist_base + GIC_DIST_PRI + 0x10);
	writel_relaxed(context[i++], gic_dist_base + GIC_DIST_PRI + 0x14);
	writel_relaxed(context[i++], gic_dist_base + GIC_DIST_PRI + 0x18);
	writel_relaxed(context[i++], gic_dist_base + GIC_DIST_PRI + 0x1c);

	/* ICDICFR0 ~ ICDICFR1 */
	writel_relaxed(context[i++], gic_dist_base + GIC_DIST_CONFIG);
	writel_relaxed(context[i++], gic_dist_base + GIC_DIST_CONFIG + 0x4);

	/* ICDISER0 */
	writel_relaxed(context[i++], gic_dist_base + GIC_DIST_ENABLE_SET);

	BUG_ON(i != PPI_CONTEXT_SIZE);
}


static void scu_save_context(void){
	u32 i = 0;

	scu_context[i++] = __raw_readl(SPRD_A5MP_BASE); 
	scu_context[i++] = __raw_readl(SPRD_A5MP_BASE + 0x8); 
	scu_context[i++] = __raw_readl(SPRD_A5MP_BASE + 0xc); 
	scu_context[i++] = __raw_readl(SPRD_A5MP_BASE + 0x40); 
	scu_context[i++] = __raw_readl(SPRD_A5MP_BASE + 0x44); 
	scu_context[i++] = __raw_readl(SPRD_A5MP_BASE + 0x50); 
	scu_context[i] = __raw_readl(SPRD_A5MP_BASE + 0x5c); 
	return;
}

static void scu_restore_context(void){

	u32 i = SCU_CONTEXT_SIZE - 1;

	__raw_writel(scu_context[i--], SPRD_A5MP_BASE+0x5c);
	__raw_writel(scu_context[i--], SPRD_A5MP_BASE+0x50);
	__raw_writel(scu_context[i--], SPRD_A5MP_BASE+0x44);
	__raw_writel(scu_context[i--], SPRD_A5MP_BASE+0x40);
	__raw_writel(scu_context[i--], SPRD_A5MP_BASE+0xc);
	__raw_writel(scu_context[i--], SPRD_A5MP_BASE+0x8);
	__raw_writel(scu_context[i], SPRD_A5MP_BASE);
	
	return;
}

static void setup_autopd_mode(void)
{
#if 0
	//debug only
	sci_glb_write(REG_GLB_POWCTL0, 0x01000a20|(1<<23), -1UL);
#endif


#if 1
	sci_glb_write(REG_GLB_MM_PWR_CTL, 0x06000320|PD_AUTO_EN, -1UL); /*MM*/
	sci_glb_write(REG_GLB_G3D_PWR_CTL, 0x06000320|PD_AUTO_EN, -1UL);/*GPU*/
	sci_glb_write(REG_GLB_CEVA_L1RAM_PWR_CTL, 0x04000720/*|PD_AUTO_EN*/, -1UL);
	sci_glb_write(REG_GLB_GSM_PWR_CTL, 0x05000520/*|PD_AUTO_EN*/, -1UL);/*GSM*/
	sci_glb_write(REG_GLB_TD_PWR_CTL, 0x05000520/*|PD_AUTO_EN*/, -1UL);/*TD*/
	sci_glb_write(REG_GLB_PERI_PWR_CTL, 0x03000920/*|PD_AUTO_EN*/, -1UL);
	if (sci_glb_read(REG_AHB_CHIP_ID, -1UL) == CHIP_ID_VER_0) {
		sci_glb_write(REG_GLB_ARM_SYS_PWR_CTL, 0x02000f20|PD_AUTO_EN, -1UL);
		sci_glb_write(REG_GLB_POWCTL0, 0x07000a20|(1<<23), -1UL );
	}else {
		sci_glb_write(REG_GLB_ARM_SYS_PWR_CTL, 0x02000f20|PD_AUTO_EN, -1UL);
		sci_glb_write(REG_GLB_POWCTL0, 0x07000a20|(1<<23), -1UL);
		/* sci_glb_set(REG_GLB_POWCTL1, DSP_ROM_SLP_PD_EN|MCU_ROM_SLP_PD_EN); */
	}
#endif
}

void check_pd(void)
{
#define CHECK_PD(_val, _reg) { \
	val = sci_glb_read(_reg, -1UL); \
	if(val != (_val))	\
		printk("### setting not same:"#_reg" = %08x !=%08x\n", val, (_val)); \
	}

	unsigned int val;
	CHECK_PD(0x06000320|PD_AUTO_EN, REG_GLB_MM_PWR_CTL);
	CHECK_PD(0x06000320|PD_AUTO_EN, REG_GLB_G3D_PWR_CTL);
	CHECK_PD(0x04000720, REG_GLB_CEVA_L1RAM_PWR_CTL);
	CHECK_PD(0x05000520, REG_GLB_GSM_PWR_CTL);
	CHECK_PD(0x05000520, REG_GLB_TD_PWR_CTL);
	CHECK_PD(0x03000920, REG_GLB_PERI_PWR_CTL);
	if (sci_glb_read(REG_AHB_CHIP_ID, -1UL) == CHIP_ID_VER_0) {
		CHECK_PD(0x02000f20|PD_AUTO_EN, REG_GLB_ARM_SYS_PWR_CTL);
		CHECK_PD(0x01000a20|(1<<23), REG_GLB_POWCTL0);
	}else{
		CHECK_PD(0x02000f20|PD_AUTO_EN, REG_GLB_ARM_SYS_PWR_CTL);
		CHECK_PD(0x01000a20|(1<<23), REG_GLB_POWCTL0);
	}
}

/*
* in case the white led is not managed in any dirvers
*/
static void init_led(void)
{
	sci_adi_set(ANA_REG_GLB_LED_CTRL, BIT_WHTLED_PD );
}

void check_ldo(void)
{
#define CHECK_LDO(_reg, _val) { \
	val = sci_adi_read(_reg); \
	if(val != (_val)) printk("### setting not same:"#_reg" = %08x !=%08x\n", val, (_val)); \
	}
	unsigned int val;

	/*
	 *  LDOVDD25(bit13), LDOVDD18(bit12), LDOVDD28(bit11) 
	 *is not auto power down as default
	 */
	CHECK_LDO(ANA_REG_GLB_LDO_SLP_CTRL0, 0xc7fd);

	/*
	 * FSM_SLPPD_EN(bit15) must be set
	 */
	CHECK_LDO(ANA_REG_GLB_LDO_SLP_CTRL1, 0x8ffe);
#if 0
	CHECK_LDO(ANA_REG_GLB_LDO_SLP_CTRL2, 0xffff);
	CHECK_LDO(ANA_REG_GLB_LDO_SLP_CTRL3, 0x7);
	CHECK_LDO(ANA_REG_GLB_DCDC_CTRL0, 0x0025);
#endif
}

/*copy code for deepsleep return */
#define SAVED_VECTOR_SIZE 64
static uint32_t *sp_pm_reset_vector = NULL;
static uint32_t saved_vector[SAVED_VECTOR_SIZE];
void __iomem *iram_start;


#define SPRD_RESET_VECTORS 0X00000000
#define IRAM_START_PHY 	SPRD_IRAM_PHYS
#define IRAM_SIZE 0x8000
#define SLEEP_CODE_SIZE 4096
#define EMC_REINIT_CODE_SIZE 4096

static int init_reset_vector(void)
{
	/* remap iram to 0x00000000 */
	sci_glb_set(REG_AHB_REMAP, BIT(0));

	if (!sp_pm_reset_vector) {
		sp_pm_reset_vector = ioremap(SPRD_RESET_VECTORS, PAGE_SIZE);
		if (sp_pm_reset_vector == NULL) {
			printk(KERN_ERR "sp_pm_init: failed to map reset vector\n");
			return 0;
		}
	}

	iram_start = (void __iomem *)(SPRD_IRAM_BASE);
	/* copy sleep code to (IRAM+16K). */
	if ((sc8825_standby_iram_end - sc8825_standby_iram + 128) > SLEEP_CODE_SIZE) {
		panic("##: code size is larger than expected, need more memory!\n");
	}
	memcpy_toio(iram_start, sc8825_standby_iram, SLEEP_CODE_SIZE);

	/* copy emc re-init code to (IRAM+16k+8K) */;
	memcpy_toio(iram_start+2*SLEEP_CODE_SIZE, emc_init_repowered, EMC_REINIT_CODE_SIZE);

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

	sp_pm_reset_vector[SAVED_VECTOR_SIZE - 1] = (sc8825_standby_exit_iram -
		sc8825_standby_iram + IRAM_START_PHY);
}

static void restore_reset_vector(void)
{
	int i;
	for (i = 0; i < SAVED_VECTOR_SIZE; i++)
		sp_pm_reset_vector[i] = saved_vector[i];
}

/* irq functions */
#define hw_raw_irqs_disabled_flags(flags)	\
({						\
	(int)((flags) & PSR_I_BIT);		\
})

#define hw_irqs_disabled()			\
({						\
	unsigned long _flags;			\
	_flags = hw_local_save_flags();		\
	hw_raw_irqs_disabled_flags(_flags);	\
})

u32 __attribute__ ((naked)) read_cpsr(void)
{
	__asm__ __volatile__("mrs r0, cpsr\nbx lr");
}

/*make sure adb ahb and audio is complete shut down.*/
#define GEN0_MASK ( GEN0_SIM0_EN | /*GEN0_ADI_EN |*/ GEN0_GPIO_EN | 			\
			   GEN0_I2C0_EN|GEN0_I2C1_EN|GEN0_I2C2_EN|GEN0_I2C3_EN | 	\
			   GEN0_SPI0_EN|GEN0_SPI1_EN| GEN0_I2S0_EN | GEN0_I2S1_EN| 	\
	                GEN0_EFUSE_EN | GEN0_I2S_EN | GEN0_PIN_EN | GEN0_SPI2_EN |	\
	                GEN0_EPT_EN | GEN0_SIM1_EN | GEN0_SPI_EN | GEN0_UART0_EN | 	\
	                GEN0_UART1_EN | GEN0_UART2_EN | GEN0_UART3_EN | /*GEN0_TIMER_EN |*/	\
	                /*GEN0_SYST_EN |*/ GEN0_UART2_EN | GEN0_CCIR_MCLK_EN )

#define CLK_EN_MASK (CLK_PWM0_EN | CLK_PWM1_EN | CLK_PWM2_EN | CLK_PWM3_EN)
#define BUSCLK_ALM_MASK (ARM_VB_MCLKON|ARM_VB_DA0ON|ARM_VB_DA1ON|ARM_VB_ADCON|ARM_VB_ANAON|ARM_VB_ACC)
#define AHB_CTL0_MASK   (AHB_CTL0_DCAM_EN | AHB_CTL0_CCIR_IN_EN | AHB_CTL0_CCIR_EN | AHB_CTL0_LCDC_EN |    \
		AHB_CTL0_SDIO0_EN | AHB_CTL0_SDIO1_EN | AHB_CTL0_SDIO2_EN | AHB_CTL0_EMMC_EN  |   \
		AHB_CTL0_BM0_EN | AHB_CTL0_BM1_EN | AHB_CTL0_BM2_EN | AHB_CTL0_BM3_EN |      \
		AHB_CTL0_BM4_EN | AHB_CTL0_G3D_EN | AHB_CTL0_DMA_EN | AHB_CTL0_NFC_EN |\
		AHB_CTL0_AXIBUSMON0_EN | AHB_CTL0_AXIBUSMON1_EN | AHB_CTL0_AXIBUSMON2_EN | \
		AHB_CTL0_ISP_EN | AHB_CTL0_VSP_EN|AHB_CTL0_ROT_EN | AHB_CTL0_USBD_EN |   \
		AHB_CTL0_DCAM_MIPI_EN | AHB_CTL0_DISPC_EN | /*AHB_CTL0_SPINLOCK_EN |*/ AHB_CTL0_DRM_EN )

#define AHB_CTL2_MASK ( AHB_CTL2_DISPMTX_CLK_EN | AHB_CTL2_MMMTX_CLK_EN | AHB_CTL2_DISPC_CORE_CLK_EN | \
			AHB_CTL2_LCDC_CORE_CLK_EN | AHB_CTL2_ISP_CORE_CLK_EN | AHB_CTL2_VSP_CORE_CLK_EN | \
			AHB_CTL2_DCAM_CORE_CLK_EN )

#define GR_GEN1_MASK (GEN1_VBC_EN | GEN1_AUX1_EN | GEN1_AUX0_EN )

#define GR_CLK_EN_MASK CLK_EN_MASK
#define GR_GEN0_MASK GEN0_MASK

static void disable_audio_module(void)
{
	sci_glb_clr(REG_GLB_BUSCLK, BUSCLK_ALM_MASK);
}

static void disable_apb_module(void)
{
	sci_glb_clr(REG_GLB_GEN0, GEN0_MASK);
	sci_glb_clr(REG_GLB_CLK_EN, CLK_EN_MASK);

}

static void disable_ahb_module(void)
{
	sci_glb_clr(REG_AHB_AHB_CTL2, AHB_CTL2_MASK);
	sci_glb_clr(REG_AHB_AHB_CTL0, AHB_CTL0_MASK);
}

#define INT0_REG(off) (SPRD_INTC0_BASE + (off))
#define INT1_REG(off) (SPRD_INTC1_BASE + (off))

#define INT0_IRQ_STS            INT0_REG(0x0000)
#define INT0_IRQ_RAW           INT0_REG(0x0004)
#define INT0_IRQ_ENB           INT0_REG(0x0008)
#define INT0_IRQ_DIS            INT0_REG(0x000c)
#define INT0_FIQ_STS            INT0_REG(0x0020)
#define INT0_FIQ_ENB           INT0_REG(0x0028)
#define INT0_FIQ_DIS		INT0_REG(0x002c)
#define INT0_IRQ_MASK	(1<<7 | 1<<3)


/*save/restore global regs*/
u32 reg_gen_clk_en, reg_gen0_val, reg_busclk_alm, reg_ahb_ctl0_val, reg_ahb_ctl2_val, reg_gen1_val;

/*register save*/
#define SAVE_GR_REG(_reg_save,_reg_addr, _reg_mask)  \
	{_reg_save = (sci_glb_read( _reg_addr, -1UL) & ((u32)_reg_mask));}

#define SAVE_GLOBAL_REG  do{ \
        SAVE_GR_REG(reg_gen_clk_en, REG_GLB_CLK_EN, GR_CLK_EN_MASK); \
        SAVE_GR_REG(reg_gen0_val, REG_GLB_GEN0, GR_GEN0_MASK);   \
	SAVE_GR_REG(reg_gen1_val, REG_GLB_GEN1, GR_GEN1_MASK);   \
        SAVE_GR_REG(reg_busclk_alm, REG_GLB_BUSCLK, BUSCLK_ALM_MASK);    \
        SAVE_GR_REG(reg_ahb_ctl0_val, REG_AHB_AHB_CTL0, AHB_CTL0_MASK);\
        SAVE_GR_REG(reg_ahb_ctl2_val, REG_AHB_AHB_CTL2, AHB_CTL2_MASK);\
    }while(0)

/*register restore*/
#define RESTORE_GR_REG(_reg_addr, _reg_mask, _reg_save)    do{ \
	sci_glb_set(_reg_addr, _reg_mask & _reg_save); \
    }while(0)

#define RESTORE_GLOBAL_REG   do{  \
        RESTORE_GR_REG(REG_GLB_CLK_EN, GR_CLK_EN_MASK, reg_gen_clk_en);   \
        RESTORE_GR_REG(REG_GLB_BUSCLK, BUSCLK_ALM_MASK, reg_busclk_alm); \
        RESTORE_GR_REG(REG_GLB_GEN0, GR_GEN0_MASK, reg_gen0_val); \
	RESTORE_GR_REG(REG_GLB_GEN1, GR_GEN1_MASK, reg_gen1_val); \
        RESTORE_GR_REG(REG_AHB_AHB_CTL0, AHB_CTL0_MASK, reg_ahb_ctl0_val); \
        RESTORE_GR_REG(REG_AHB_AHB_CTL2, AHB_CTL2_MASK, reg_ahb_ctl2_val); \
    }while(0)


/* make sure printk is end, if not maybe some messy code  in SERIAL1 output */
#define UART_TRANSFER_REALLY_OVER (0x1UL << 15)
#define UART_STS0 (SPRD_UART1_BASE + 0x08)
#define UART_STS1 (SPRD_UART1_BASE + 0x0c)

static void wait_until_uart1_tx_done(void)
{
	u32 tx_fifo_val;
	u32 really_done = 0;
	u32 timeout = 2000;

	/*uart1 owner dsp sel*/
	if (sci_glb_read(REG_GLB_PCTRL, -1UL) & (1<<8)/* UART1_SEL */) return ;

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

/* arm core + arm sys */
static void mcu_sleep(void)
{
	u32 val;

	SAVE_GLOBAL_REG;
	disable_audio_module();
	disable_ahb_module();
	/* FIXME: whether enable emc auto gate or not in mcu sleep
	val = sci_glb_read(REG_AHB_AHB_CTL1, -1UL);
	val |=	AHB_CTRL1_EMC_AUTO_GATE_EN;
	sci_glb_write(REG_AHB_AHB_CTL1, val, -1UL );
	*/
	val = sci_glb_read(REG_AHB_AHB_PAUSE, -1UL);
	val &= ~( MCU_CORE_SLEEP | MCU_DEEP_SLEEP_EN | MCU_SYS_SLEEP_EN );
	/* FIXME: enable sys sleep in final version 
	val |= MCU_SYS_SLEEP_EN;
	*/
	sci_glb_write(REG_AHB_AHB_PAUSE, val, -1UL );
	cpu_do_idle();
	hard_irq_set();
	RESTORE_GLOBAL_REG;
}
#if 0
/* save pm message for debug when enter deep sleep*/
unsigned int debug_status[10];
static void pm_debug_save_ahb_glb_regs(void)
{
	debug_status[0] = sci_glb_read(REG_AHB_AHB_STATUS, -1UL);
	debug_status[1] = sci_glb_read(REG_AHB_AHB_CTL0, -1UL);
	debug_status[2] = sci_glb_read(REG_AHB_AHB_CTL1, -1UL);
	debug_status[3] = sci_glb_read(REG_AHB_AHB_STATUS, -1UL);
	debug_status[4] = sci_glb_read(REG_GLB_GEN0, -1UL);
	debug_status[5] = sci_glb_read(REG_GLB_GEN1, -1UL);
	debug_status[6] = sci_glb_read(REG_GLB_STC_DSP_ST, -1UL);
	debug_status[7] = sci_glb_read(REG_GLB_BUSCLK, -1UL);
	debug_status[8] = sci_glb_read(REG_GLB_CLKDLY, -1UL);
}
static void pm_debug_dump_ahb_glb_regs(void)
{
	printk("***** ahb and globle registers before enter deep sleep **********\n");
	printk("*** AHB_CTL0:  0x%x ***\n", debug_status[1] );
	printk("*** AHB_CTL1:  0x%x ***\n", debug_status[2] );
	printk("*** AHB_STS:  0x%x ***\n", debug_status[3] );
	printk("*** GR_GEN0:  0x%x ***\n", debug_status[4] );
	printk("*** GR_GEN1:  0x%x ***\n", debug_status[5] );
	printk("*** GR_STC_STATE:  0x%x ***\n", debug_status[6] );
	printk("*** GR_BUSCLK:  0x%x ***\n", debug_status[7] );
	printk("*** GR_CLK_DLY:  0x%x ***\n", debug_status[8] );
}
#endif
unsigned int sprd_irq_pending(void)
{
	u32 status;

	status = __raw_readl(SPRD_INTC0_BASE + 0x0000);
	if(status)
		return status;
	status = __raw_readl(SPRD_INTC0_BASE + 0x1000);
	return status;
}

static int emc_repower_init(void)
{
	repower_param = (struct emc_repower_param *)(SPRD_IRAM_BASE + 15 * 1024);
	set_emc_repower_param(repower_param, SPRD_LPDDR2C_BASE, SPRD_LPDDR2C_BASE + 0x1000);
	repower_param->cs0_training_addr_v = (u32)ioremap(repower_param->cs0_training_addr_p, 4*1024);
	if(repower_param->cs_number)
	{
		repower_param->cs1_training_addr_v = (u32)ioremap(repower_param->cs1_training_addr_p, 4*1024);
	}
	/*close emc retention*/
	sci_glb_write(0x80 + REGS_GLB_BASE, 0, -1UL);
	return 0;

}

#ifdef FORCE_DISABLE_DSP
/*
 *TODO: just for debug, should be clear in mp release version
 */
/* FPGA ONLY */
static u32 dbg_reg_base;
static u32 dsp_pmu_base;
static u32 gsm_stc_base;
static u32 td_base;
static void dbg_module_close(void)
{
	if(dbg_reg_base) {
		__raw_writel(0xc5acce55, dbg_reg_base + 0x0fb0);
		__raw_writel(0x00000000, dbg_reg_base + 0x0310);
		__raw_writel(0xc5acce55, dbg_reg_base + 0x2fb0);
		__raw_writel(0x00000000, dbg_reg_base + 0x2310);

		__raw_writel(0xc5acce55, dbg_reg_base + 0xcfb0);
		__raw_writel(0x00000001, dbg_reg_base + 0xcf00);
		__raw_writel(0x00000020, dbg_reg_base + 0xcedc);

		__raw_writel(0xc5acce55, dbg_reg_base + 0xdfb0);
		__raw_writel(0x00000001, dbg_reg_base + 0xdf00);
		__raw_writel(0x00000020, dbg_reg_base + 0xdedc);
	}
}



u32 dsp_status[10];
static int dsp_and_modem_force_close(void)
{
	u32 val;

	val = __raw_readl(SPRD_GREG_BASE + 0x8);
	//val |= BIT(10);
	__raw_writel(val, SPRD_GREG_BASE + 0x8);

	val = __raw_readl(SPRD_AHB_BASE + 0x204);
	val &= ~BIT(16);
	__raw_writel(val, SPRD_AHB_BASE + 0x204);

	val = __raw_readl(SPRD_AHB_BASE + 0x284);
	val |= BIT(2);	
	__raw_writel(val, SPRD_AHB_BASE + 0x284);

	/* soft reset dsp */
	val = __raw_readl(SPRD_AHB_BASE + 0x28c);
	val &= ~BIT(0);	
	__raw_writel(val, SPRD_AHB_BASE + 0x28c);

	/*force close cp*/
	__raw_writel(0x00000001, SPRD_AHB_BASE + 0x258);

	/* disable dsp*/
	val = __raw_readl(dsp_pmu_base + 0x14);
	val |= BIT(8)|BIT(9)|BIT(10);
	/* enable RFT DSP clock */
	val |= BIT(27);
	__raw_writel(val, dsp_pmu_base + 0x14);

	/* td_slp_poweroff_auto_en, gsm_slp_poweroff_auto_en */
	val = __raw_readl(dsp_pmu_base + 0x30);
	val |= BIT(13)|BIT(15);
	__raw_writel(val, dsp_pmu_base + 0x30);

	/* set RFT_RFLDO_AUTO_PD = 1*/
	val = __raw_readl(td_base + 0x108);
	val |= BIT(21); 
	__raw_writel(val, td_base + 0x108);

	/* set RFT_RFXTL_AUTO_PD = 1*/
	val = __raw_readl(td_base + 0x104);
	val |= BIT(21); 
	__raw_writel(val, td_base + 0x104);

	/* set ecc_force_off = 1 */
	val = __raw_readl(td_base + 0x94);
	val |= BIT(15); 
	__raw_writel(val, td_base + 0x94);
	
	/* set STC_RFLDO_AUTO_PD = 1 */
	val = __raw_readw(gsm_stc_base + 0x256);
	val |= BIT(15);
	__raw_writew(val, gsm_stc_base + 0x256);

	/* set STC_RFXTL_AUTO_PD = 1 */
	val = __raw_readw(gsm_stc_base + 0x254);
	val |= BIT(15);
	__raw_writew(val, gsm_stc_base + 0x254);

	/* set qbc_force_off = 1 */
	val = __raw_readw(gsm_stc_base + 0x2ea);
	val |= BIT(12);
	__raw_writew(val, gsm_stc_base + 0x2ea);

	flush_cache_all();
	outer_flush_all();
	udelay(1000);
	udelay(1000);
	
	dsp_status[0] =  __raw_readl(dsp_pmu_base+0x14);
	dsp_status[1] =  __raw_readl(dsp_pmu_base+0x28);
	dsp_status[2] =  __raw_readl(dsp_pmu_base+0x30);
	dsp_status[3] =  __raw_readl(td_base+0x104);
	dsp_status[4] =  __raw_readl(td_base+0x108);
	dsp_status[5] =  __raw_readl(td_base+0x94);
	dsp_status[6] =  __raw_readw(gsm_stc_base+0x254);
	dsp_status[7] =  __raw_readw(gsm_stc_base+0x256);
	dsp_status[8] =  __raw_readw(gsm_stc_base+0x2ea);

	val = __raw_readl(dsp_pmu_base + 0x14);
	/* disable RFT DSP clock */
	val &= ~BIT(27);
	__raw_writel(val, dsp_pmu_base + 0x14);

	/* enable arm2dsp bridge sleep*/
	val = __raw_readl(SPRD_AHB_BASE + 0x204);
	val |= BIT(16);
	__raw_writel(val, SPRD_AHB_BASE + 0x204);

	val = __raw_readl(SPRD_AHB_BASE + 0x284);
	val &= ~( BIT(2) );	
	__raw_writel(val, SPRD_AHB_BASE + 0x284);


	val = __raw_readl(SPRD_GREG_BASE + 0x8);
	val &= ~BIT(10);
	__raw_writel(val, SPRD_GREG_BASE + 0x8);
	return 0;
}
/* FPGA DEBUG ONLY */
#endif

/* chip sleep*/
int deep_sleep(void)
{
	u32 val, ret = 0;
	u32 holding;

	wait_until_uart1_tx_done();
	SAVE_GLOBAL_REG;
	disable_audio_module();
	disable_apb_module();
	disable_ahb_module();

	/* for dsp wake-up */
	val = __raw_readl(INT0_IRQ_ENB);
	val |= SCI_INTC_IRQ_BIT(IRQ_DSP0_INT);
	val |= SCI_INTC_IRQ_BIT(IRQ_DSP1_INT);
	__raw_writel(val, INT0_IRQ_ENB);

	val = __raw_readl(INT0_FIQ_ENB);
	val |= SCI_INTC_IRQ_BIT(IRQ_DSP0_INT);
	val |= SCI_INTC_IRQ_BIT(IRQ_DSP1_INT);
	__raw_writel(val, INT0_FIQ_ENB);

	/* prevent uart1 */
	__raw_writel(INT0_IRQ_MASK, INT0_IRQ_DIS);

#ifdef CONFIG_CACHE_L2X0
	__raw_writel(0x3, SPRD_L2_BASE+0xF80);/*L2X0_POWER_CTRL, standby_mode_enable*/
	l2x0_suspend();
#else
	__raw_writel(0x3, SPRD_L2_BASE+0xF80);/*L2X0_POWER_CTRL, standby_mode_enable*/
#endif

#ifdef FORCE_DISABLE_DSP
	/* close debug modules, only for fpga or debug */
        /*
	dbg_module_close();
	*/
	dsp_and_modem_force_close();
#endif
	/*
	 * pm_debug_set_wakeup_timer();
	*/

	/* FIXME: enable emc auto gate in final version 
	val = sci_glb_read(REG_AHB_AHB_CTL1, -1UL);
	val |= AHB_CTRL1_EMC_AUTO_GATE_EN;
	sci_glb_write(REG_AHB_AHB_CTL1, val, -1UL);
	*/
	
	/*go deepsleep when all PD auto poweroff en*/
	val = sci_glb_read(REG_AHB_AHB_PAUSE, -1UL);
	val &= ~( MCU_CORE_SLEEP | MCU_DEEP_SLEEP_EN | MCU_SYS_SLEEP_EN );
	#ifndef CONFIG_MACH_SP6825GA
	val |= (MCU_SYS_SLEEP_EN | MCU_DEEP_SLEEP_EN);
	#else
	val |= MCU_SYS_SLEEP_EN;
	#endif

	sci_glb_write(REG_AHB_AHB_PAUSE, val, -1UL);

	/* set entry when deepsleep return*/
	save_reset_vector();
	set_reset_vector();
	
	/* check globle key registers */
	pm_debug_save_ahb_glb_regs( );
	
	/* indicate cpu stopped */
	holding = sci_glb_read(REG_AHB_HOLDING_PEN, -1UL);
	sci_glb_write(REG_AHB_HOLDING_PEN, (holding & (~CORE1_RUN)) | AP_ENTER_DEEP_SLEEP , -1UL );

	save_emc_trainig_data(repower_param);
	ret = sp_pm_collapse(0, 1);
	hard_irq_set();
	/*clear dsp fiq, for dsp wakeup*/
	__raw_writel(ICLR_DSP_FRQ0_CLR, SPRD_IPI_ICLR);
	__raw_writel(ICLR_DSP_FIQ1_CLR, SPRD_IPI_ICLR);
	/*disable dsp fiq*/
	val = SCI_INTC_IRQ_BIT(IRQ_DSP0_INT) | SCI_INTC_IRQ_BIT(IRQ_DSP1_INT);
	__raw_writel(val , INT0_FIQ_DIS);

	/*clear the deep sleep status*/
	sci_glb_write(REG_AHB_HOLDING_PEN, holding & (~CORE1_RUN) & (~AP_ENTER_DEEP_SLEEP), -1UL );



	/* FIXME: clear emc auto gate in final version
	val = sci_glb_read(REG_AHB_AHB_CTL1, -1UL);
	val &= ~AHB_CTRL1_EMC_AUTO_GATE_EN;
	sci_glb_write(REG_AHB_AHB_CTL1, val, -1UL);
	*/

	restore_reset_vector();	
	RESTORE_GLOBAL_REG;

	udelay(5);
	if (ret) cpu_init();

#ifdef CONFIG_CACHE_L2X0
	/*L2X0_POWER_CTRL, auto_clock_gate, standby_mode_enable*/
	__raw_writel(0x3, SPRD_L2_BASE+0xF80);
	l2x0_resume(ret);
#endif

	return ret;
}


#define DEVICE_AHB              (0x1UL << 20)
#define DEVICE_APB              (0x1UL << 21)
#define DEVICE_VIR              (0x1UL << 22)
#define DEVICE_AWAKE            (0x1UL << 23)
#define DEVICE_TEYP_MASK        (DEVICE_AHB | DEVICE_APB | DEVICE_VIR | DEVICE_AWAKE)


static int sc8825_get_sleep_mod( void ){
	int val, ret;
	printk("*** REG_GLB_GEN1:  0x%x ***\n", sci_glb_read(REG_GLB_GEN1, -1UL));
	printk("*** REG_GLB_STC_DSP_ST:  0x%x ***\n", sci_glb_read(REG_GLB_STC_DSP_ST, -1UL));
	printk("*** REG_GLB_BUSCLK:  0x%x ***\n", sci_glb_read(REG_GLB_BUSCLK, -1UL));
	printk("*** REG_GLB_CLKDLY:  0x%x ***\n", sci_glb_read(REG_GLB_CLKDLY, -1UL));

	val = sci_glb_read(REG_AHB_AHB_CTL0, -1UL);
	ret = val & AHB_CTL0_MASK; 
	val = sci_glb_read(REG_AHB_AHB_CTL2, -1UL);
	ret = ret | (val & AHB_CTL0_MASK);
	if(ret != 0){
		printk("*** AHB_CTL0:  0x%x ***\n", sci_glb_read(REG_AHB_AHB_CTL0, -1UL));
		printk("*** AHB_CTL2:  0x%x ***\n", sci_glb_read(REG_AHB_AHB_CTL2, -1UL));
		printk("*** %s, ret:0x%x ****\n", __func__, ret);
		return DEVICE_AHB;
	} 

	val = sci_glb_read(REG_GLB_GEN0, -1UL);
	ret = val & GR_GEN0_MASK;
	printk("*** GR_GEN0:  0x%x, ret:0x%x ***\n", sci_glb_read(REG_GLB_GEN0, -1UL), ret);
	if(ret == 0){
		val = sci_glb_read(REG_GLB_CLK_EN, -1UL);
		ret = val & GR_CLK_EN_MASK;
		printk("*** GR_CLK_EN:  0x%x, ret:0x%x ***\n", sci_glb_read(REG_GLB_CLK_EN, -1UL), ret);
		if(ret == 0){
			val = sci_glb_read(REG_GLB_GEN1, -1UL);
			ret = val & GR_GEN1_MASK;
			printk("*** GR_GEN1:  0x%x, ret:0x%x ***\n", sci_glb_read(REG_GLB_GEN1, -1UL), ret);
			if(ret == 0){
				return 0;
			}else{
				return DEVICE_APB;
			}
		}else{
			return DEVICE_APB;
		}
	}else{
		return DEVICE_APB;
	}


	return 0;

}

int sc8825_enter_lowpower(void)
{
	int status, ret = 0;
	unsigned long flags, time;
	unsigned int cpu = smp_processor_id();

#ifdef CONFIG_SPRD_PM_DEBUG
	__raw_writel(0xfdffbfff, SPRD_INTC0_BASE + 0xc);//intc0
	__raw_writel(0x02004000, SPRD_INTC0_BASE + 0x8);//intc0
	__raw_writel(0xffffffff, SPRD_INTC0_BASE + 0x100c);//intc1
#endif	

	time = get_sys_cnt();
	if (!hw_irqs_disabled())  {
		flags = read_cpsr();
		printk("##: Error(%s): IRQ is enabled(%08lx)!\n",
			 "wakelock_suspend", flags);
	}
	/*TODO:
	* we need to known clock status in modem side
	*/
#ifdef FORCE_DISABLE_DSP
	status = 0;
#else
#ifdef CONFIG_NKERNEL
	status = sc8825_get_clock_status();
#else
	/*
	* TODO: get clock status in native version, force deep sleep now
	*/
	status = 0;
#endif
#endif
	if (status & DEVICE_AHB)  {
		printk("###### %s,  DEVICE_AHB ###\n", __func__ );
		set_sleep_mode(SLP_MODE_ARM);
		arm_sleep();
	} else if (status & DEVICE_APB) {
		printk("###### %s,	DEVICE_APB ###\n", __func__ );
		set_sleep_mode(SLP_MODE_MCU);
		mcu_sleep();
	} else {
		/*printk("###### %s,	DEEP ###\n", __func__ );*/
		set_sleep_mode(SLP_MODE_DEP);
		gic_save_context( );
		scu_save_context();
		ret = deep_sleep( );
		scu_restore_context();
		gic_restore_context( );
		gic_cpu_enable(cpu);
		gic_dist_enable( );
	}
	
	time_add(get_sys_cnt() - time, ret);
	print_hard_irq_inloop(ret);

	return ret;

}

/*sleep entry for tiger */
int sprd_cpu_deep_sleep(unsigned int cpu)
{
	int ret = 0;

	ret = sc8825_enter_lowpower( );

	return ret;
}

void sprd_pm_cpu_enter_lowpower(unsigned int cpu)
{
	/*
	 * Call low level function  with targeted CPU id
	 * and its low power state.
	 */
	if (cpu)
		gic_save_ppi( );
	
	/*
	* TODO: stop_critical_timings ??? 
	*/
	/* stop_critical_timings(); */
	
	flush_cache_all();
	outer_flush_all();
	sp_pm_collapse(cpu, 1);

	/*
	* TODO: start_critical_timings ??? 
	*/
	/* start_critical_timings(); */

	if (cpu)
		gic_restore_ppi( );
	gic_cpu_enable(cpu);
}


/*init global regs for pm */
static void init_gr(void)
{
	int val;

	/* remap iram to 0x00000000*/
	sci_glb_set(REG_AHB_REMAP, BIT(0));
#ifdef CONFIG_NKERNEL
	/*force close cp*/
	__raw_writel(0x00000001, REG_AHB_CP_SLEEP_CTRL);
#endif
	/* AHB_PAUSE */
	val = sci_glb_read(REG_AHB_AHB_PAUSE, -1UL);
	val &= ~(MCU_CORE_SLEEP | MCU_DEEP_SLEEP_EN | MCU_SYS_SLEEP_EN);
	sci_glb_write(REG_AHB_AHB_PAUSE, val, -1UL );

	/* AHB_CTL1 */
	val = sci_glb_read(REG_AHB_AHB_CTL1, -1UL);
	val |= (AHB_CTRL1_EMC_CH_AUTO_GATE_EN	|
		AHB_CTRL1_ARM_AUTO_GATE_EN	|
		AHB_CTRL1_AHB_AUTO_GATE_EN	|
		AHB_CTRL1_MCU_AUTO_GATE_EN	|
		AHB_CTRL1_ARM_DAHB_SLEEP_EN	|
		AHB_CTRL1_MSTMTX_AUTO_GATE_EN);
	val &=	~AHB_CTRL1_EMC_AUTO_GATE_EN;
	sci_glb_write(REG_AHB_AHB_CTL1, val, -1UL );

	/* enable XTL auto power down, set bufon_ctrl[17:16] 0 */
	val = sci_glb_read(REG_GLB_CLK_EN, -1UL);
	val |= MCU_XTLEN_AUTOPD_EN;
	val |= BUFON_CTRL_HI;
	sci_glb_write(REG_GLB_CLK_EN, val, -1UL );
}

#ifdef CONFIG_NKERNEL
void sc8825_idle(void)
{
	int val;
	if (!need_resched()) {
		hw_local_irq_disable();
		if (!arch_local_irq_pending()) {
			val = os_ctx->idle(os_ctx);
			if (0 == val) {
#ifdef CONFIG_CACHE_L2X0
				/*l2cache power control, standby mode enable*/
				/*L2X0_POWER_CTRL
				__raw_writel(1, SPRD_L2_BASE+0xF80);
				l2x0_suspend();
				*/
#endif
				cpu_do_idle();
#ifdef CONFIG_CACHE_L2X0
				/*
				l2x0_resume(1);
				*/
#endif
			}
		}
		hw_local_irq_enable();
	}
	local_irq_enable();
	return;
}

#else

void sc8825_idle(void)
{
	if (!need_resched()) {
#ifdef CONFIG_CACHE_L2X0
			/*l2cache power control, standby mode enable*/
			/*L2X0_POWER_CTRL
			__raw_writel(1, SPRD_L2_BASE+0xF80);
			l2x0_suspend();
			*/
#endif
			/* if cpu idle enabled, linux boot will suspend at console init
			 * i donot know why, so just disable it
			 */
/*
			cpu_do_idle();
*/
#ifdef CONFIG_CACHE_L2X0
			/*
			l2x0_resume(1);
			*/
#endif
	}
	local_irq_enable();
	return;
}
#endif

#ifdef FORCE_DISABLE_DSP
/* FPGA ONLY */
static void fpga_dbg_init(void)
{
	dbg_reg_base = (u32)ioremap(0x10110000, 64*1024);
	if ((void *)dbg_reg_base == NULL) {
		printk(KERN_ERR "----fpga_dbg_init: failed to map dbg_reg_base-----\n");
	}
	printk("+++ fpga_dbg_init: sucessful to map dbg_reg_base:0x%x +++++\n", dbg_reg_base );

	dsp_pmu_base = (u32)ioremap(0x70130000, 4*1024);
	if ((void *)dsp_pmu_base == NULL) {
		printk(KERN_ERR "----fpga_dbg_init: failed to map dsp_pmu_base-----\n");
	}
	printk("+++ fpga_dbg_init: sucessful to map dsp_pmu_base:0x%x +++++\n", dsp_pmu_base );

	gsm_stc_base = (u32)ioremap(0x70640000, 1*1024);
	if ((void *)gsm_stc_base == NULL) {
		printk(KERN_ERR "----fpga_dbg_init: failed to map gsm_stc_base-----\n");
	}
	printk("+++ fpga_dbg_init: sucessful to map gsm_stc_base:0x%x +++++\n", gsm_stc_base );

	td_base = (u32)ioremap(0x70750000, 4*1024);
	if ((void *)td_base == NULL) {
		printk(KERN_ERR "----fpga_dbg_init: failed to map td_base-----\n");
	}
	printk("+++ fpga_dbg_init: sucessful to map td_base:0x%x +++++\n", td_base );
}
/* FPGA ONLY */
#endif

void gic_save_init(void)
{
	gic_dist_base = sprd_get_gic_dist_base( );
	gic_cpu_base = sprd_get_gic_cpu_base( );
	pr_debug("**** %s, gic_dist_base:%p, gic_cpu_base:%p *** \n", __func__, gic_dist_base, gic_cpu_base);
	max_spi_reg = __raw_readl(gic_dist_base + GIC_DIST_CTR) & 0x1f;
	max_spi_irq = max_spi_reg * 32;
	pr_debug("**** %s, max_spi_reg:0x%x, max_spi_irq:%u *** \n", __func__, max_spi_reg, max_spi_irq);
}

void pm_ana_ldo_config(void)
{
	unsigned int val;
	
	/*
	* FIXME, should be more gental
	*/
	val = sci_adi_read(ANA_REG_GLB_LDO_SLP_CTRL0);
	val = 0xc7f1;
	sci_adi_write(ANA_REG_GLB_LDO_SLP_CTRL0, val, 0xffff);

	val = sci_adi_read(ANA_REG_GLB_LDO_SLP_CTRL1);
	val |= BIT_FSM_SLPPD_EN;
	val |= BIT_DCDC_ARM_BP_EN;
	sci_adi_write(ANA_REG_GLB_LDO_SLP_CTRL1, val, 0xffff);
	
	/*
	* set ARM_DCDC_ISONUM
	* ISO_ON_NUM: (0xa << 8), ISO_OFF_NUM: (0x20)
	*/
	val = (0xa << 8) | (0x20);
	sci_adi_write(ANA_REG_GLB_LDO_SLP_CTRL2, val, 0xffff);
	/*
	* set ARM_DCDC_DLY_NUM, DLY_NUM:0x2
	*/
	sci_adi_write(ANA_REG_GLB_LDO_SLP_CTRL3, 0x2, 0xffff);
}

static void sc8825_power_off(void)
{

	/*ture off all modules ldo*/
	sci_adi_raw_write(ANA_REG_GLB_LDO_PD_CTRL1,   0x5555);
	sci_adi_raw_write(ANA_REG_GLB_LDO_PD_CTRL0,   0x5555);

	/*ture off all system cores ldo*/
	sci_adi_clr(ANA_REG_GLB_LDO_PD_RST, 0x3ff);
	sci_adi_set(ANA_REG_GLB_LDO_PD_SET, 0x3ff);
}

static void sc8825_machine_restart(char mode, const char *cmd)
{

	/* Flush the console to make sure all the relevant messages make it
	 * out to the console drivers */
	mdelay(500);

	/* Disable interrupts first */
	local_irq_disable();
	local_fiq_disable();

	/*
	 * FIXME: Do not turn off cache before ldrex/strex!
	 */

	/*
	 * Now call the architecture specific reboot code.
	 */
	arch_reset(mode, cmd);

	/*
	 * Whoops - the architecture was unable to reboot.
	 * Tell the user!
	 */
	mdelay(1000);
	printk("Reboot failed -- System halted\n");
	while (1);
}

void sc8825_pm_init(void)
{
	unsigned int cpu1_jump_addrss;
	unsigned int val;
	
	init_reset_vector();
	pm_power_off = sc8825_power_off;
	arm_pm_restart = sc8825_machine_restart;
	pr_info("power off %pf, restart %pf\n", pm_power_off, arm_pm_restart);
#ifdef FORCE_DISABLE_DSP
	/* FPGA ONLY */
	fpga_dbg_init();
#endif
	init_gr();
	cpu1_jump_addrss = virt_to_phys(secondary_startup);
	sci_glb_write(REG_AHB_JMP_ADDR_CPU1, cpu1_jump_addrss,  -1UL);
	setup_autopd_mode();
	gic_save_init();
	pm_ana_ldo_config();
	init_led();
	emc_repower_init();
#ifndef CONFIG_SPRD_PM_DEBUG
	pm_debug_init();
#endif
	val = __raw_readl(sprd_get_scu_base());
	val |= (INTC_DYNAMIC_CLK_GATE_EN | SCU_DYNAMIC_CLK_GATE_EN);
	__raw_writel(val, sprd_get_scu_base());

}

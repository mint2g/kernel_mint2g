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
#include <mach/adi.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <mach/pm_debug.h>
#include <linux/debugfs.h>

struct dentry * dentry_debug_root = NULL;

/*print switch*/
static int is_print_sleep_mode = 0;
int is_print_linux_clock = 0;
int is_print_modem_clock = 0;
static int is_print_irq = 1;
static int is_print_wakeup = 1;
static int is_print_irq_runtime = 0;
static int is_print_time = 1;

/*time statisic*/
static unsigned int core_time = 0;
static unsigned int mcu_time = 0;
static unsigned int deep_time_successed = 0;
static unsigned int deep_time_failed = 0;
static unsigned int sleep_time = 0;

/* interrupt statistic. */
#define 	SPRD_HARD_INTERRUPT_NUM 32
#define	SPRD_IRQ_NUM			1024
static u32 sprd_hard_irq[SPRD_HARD_INTERRUPT_NUM]= {0, };
static u32 sprd_irqs[SPRD_IRQ_NUM] = {0, };
static int is_wakeup = 0;
static int irq_status = 0;

/*sleep mode*/
static int sleep_mode = SLP_MODE_NON;
static char * sleep_mode_str[]  = {
	"[ARM]",
	"[MCU]",
	"[DEP]",
	"[NON]"
};

#define INT_REG(off) (SPRD_INTCV_BASE + (off))

#define INT_IRQ_STS            INT_REG(0x0000)
#define INT_IRQ_RAW           INT_REG(0x0004)
#define INT_IRQ_ENB           INT_REG(0x0008)
#define INT_IRQ_DIS            INT_REG(0x000c)
#define INT_FIQ_STS            INT_REG(0x0020)

#define INT_IRQ_MASK	(1<<3)

/*hard irqs*/
static void hard_irq_reset(void)
{
	int i = SPRD_HARD_INTERRUPT_NUM - 1;
	do{
		sprd_hard_irq[i] = 0;
	}while(--i >= 0);
}

static void parse_hard_irq(unsigned long val)
{
	int i;
	for (i = 0; i < SPRD_HARD_INTERRUPT_NUM; i++) {
		if (test_and_clear_bit(i, &val)) sprd_hard_irq[i]++;
	}
}

void hard_irq_set(void)
{
	irq_status = __raw_readl(INT_IRQ_STS);
	parse_hard_irq(irq_status); 
}

void print_hard_irq_inloop(int ret)
{
	if(irq_status != 0 && is_print_irq_runtime)
		printk("%c#:%08x\n", ret?'S':'F', irq_status);
}

static void print_hard_irq(void)
{
	int i = SPRD_HARD_INTERRUPT_NUM -1;
	if(!is_print_irq)
		return;
	do{
		if(0 != sprd_hard_irq[i])
			printk("##: sprd_hard_irq[%d] = %d.\n",
                                i, sprd_hard_irq[i]);

	}while(--i >= 0);
}

/*irqs for ddi.c*/
static void irq_reset(void)
{
	int i = SPRD_IRQ_NUM - 1;
	do{
		sprd_irqs[i] = 0;
	}while(--i >= 0);
}

void inc_irq(int irq)
{
	if(is_wakeup){
		if (irq >= SPRD_IRQ_NUM) {
			printk("## bad irq number %d.\n", irq);
			return;
		}
		sprd_irqs[irq]++;
		if(is_print_wakeup)
			printk("\n#####: wakeup irq = %d.\n", irq);
		is_wakeup = 0;
	}
}
EXPORT_SYMBOL(inc_irq);

void irq_wakeup_set(void)
{
	is_wakeup = 1;
}

static void print_irq(void)
{
	int i = SPRD_IRQ_NUM - 1;
	if(!is_print_irq)
		return;
	do{
		if(0 != sprd_irqs[i])
			printk("##: sprd_irqs[%d] = %d.\n",
                                i, sprd_irqs[i]);

	}while(--i >= 0);
}

/*time statisic*/
void time_add(unsigned int time, int ret)
{
	switch(sleep_mode){
		case SLP_MODE_ARM:
			core_time += time;
			break;
		case SLP_MODE_MCU:
			mcu_time += time;
			break;
		case SLP_MODE_DEP:
			if(ret)
				deep_time_successed += time;
			else 
				deep_time_failed += time;
			break;
		default:
			break;
	}
}

void time_statisic_begin(void)
{
	core_time = 0;
	mcu_time = 0;
	deep_time_successed = 0;
	deep_time_failed = 0;
	sleep_time = get_sys_cnt();
	hard_irq_reset();
}

void time_statisic_end(void)
{
	sleep_time = get_sys_cnt() - sleep_time;
}

void print_time(void)
{
	if(!is_print_time)
		return;

	printk("time statisics : sleep_time=%d, core_time=%d, mcu_time=%d, deep_sus=%d, dep_fail=%d\n",
		sleep_time, core_time, mcu_time, deep_time_successed, deep_time_failed);
}

void set_sleep_mode(int sm){
	int is_print = (sm == sleep_mode);
	sleep_mode = sm;
	if(is_print_sleep_mode == 0 || is_print )
		return;
	switch(sm){
		case SLP_MODE_ARM:
			printk("\n[ARM]\n");
			break;
		case SLP_MODE_MCU:
			printk("\n[MCU]\n");
			break;
		case SLP_MODE_DEP:
			printk("\n[DEP]\n");
			break;
		default:
			printk("\nNONE\n");
	}
}

void clr_sleep_mode(void)
{
	sleep_mode = SLP_MODE_NON;
}

void print_statisic(void)
{
	print_time();
	print_hard_irq();
	print_irq();
	if(is_print_wakeup)	
		printk("###wake up form %s : %08x\n",  sleep_mode_str[sleep_mode],  irq_status);
	
}

#ifdef PM_PRINT_ENABLE
static struct wake_lock messages_wakelock;
#endif

#ifdef PM_PRINT_ENABLE
static void print_ahb(void)
{
	u32 val = sprd_greg_read(REG_TYPE_AHB_GLOBAL, AHB_CTL0);
	printk("##: AHB_CTL0 = %08x.\n", val);
	if (val & AHB_CTL0_DCAM_EN) printk("AHB_CTL0_DCAM_EN =1.\n");
	if (val & AHB_CTL0_CCIR_EN) printk("AHB_CTL0_CCIR_EN =1.\n");
	if (val & AHB_CTL0_LCDC_EN) printk("AHB_CTL0_LCDC_EN =1.\n");
	if (val & AHB_CTL0_SDIO0_EN) printk("AHB_CTL0_SDIO0_EN =1.\n");
	if (val & AHB_CTL0_SDIO1_EN) printk("AHB_CTL0_SDIO1_EN =1.\n");
	if (val & AHB_CTL0_DMA_EN) printk("AHB_CTL0_DMA_EN =1.\n");
	if (val & AHB_CTL0_BM0_EN) printk("AHB_CTL0_BM0_EN =1.\n");
	if (val & AHB_CTL0_NFC_EN) printk("AHB_CTL0_NFC_EN =1.\n");
	if (val & AHB_CTL0_BM1_EN) printk("AHB_CTL0_BM1_EN =1.\n");
	if (val & AHB_CTL0_G2D_EN) printk("AHB_CTL0_G2D_EN =1.\n");
	if (val & AHB_CTL0_G3D_EN) printk("AHB_CTL0_G3D_EN =1.\n");
	if (val & AHB_CTL0_AXIBUSMON0_EN) printk("AHB_CTL0_AXIBUSMON0_EN =1.\n");
	if (val & AHB_CTL0_AXIBUSMON1_EN) printk("AHB_CTL0_AXIBUSMON1_EN =1.\n");
	if (val & AHB_CTL0_VSP_EN) printk("AHB_CTL0_VSP_EN =1.\n");
	if (val & AHB_CTL0_ROT_EN) printk("AHB_CTL0_ROT_EN =1.\n");
	if (val & AHB_CTL0_USBD_EN) printk("AHB_CTL0_USBD_EN =1.\n");
}


static void print_gr(void)
{
	u32 val = sprd_greg_read(REG_TYPE_GLOBAL, GR_GEN0);
	printk("##: GR_GEN0 = %08x.\n", val);
	if (val & GEN0_SIM0_EN) printk("GEN0_SIM0_EN =1.\n");
	if (val & GEN0_I2C_EN) printk("GEN0_I2C_EN =1.\n");
	if (val & GEN0_GPIO_EN) printk("GEN0_GPIO_EN =1.\n");
	if (val & GEN0_I2C0_EN) printk("GEN0_I2C0_EN =1.\n");
	if (val & GEN0_I2C1_EN) printk("GEN0_I2C1_EN =1.\n");
	if (val & GEN0_I2C2_EN) printk("GEN0_I2C2_EN =1.\n");
	if (val & GEN0_I2C3_EN) printk("GEN0_I2C3_EN =1.\n");
	if (val & GEN0_SPI0_EN) printk("GEN0_SPI0_EN =1.\n");
	if (val & GEN0_SPI1_EN) printk("GEN0_SPI1_EN =1.\n");
	if (val & GEN0_I2S0_EN) printk("GEN0_I2S0_EN =1.\n");
	if (val & GEN0_I2S1_EN) printk("GEN0_I2S1_EN =1.\n");
	if (val & GEN0_EFUSE_EN) printk("GEN0_EFUSE_EN =1.\n");
	if (val & GEN0_I2S_EN) printk("GEN0_I2S_EN =1.\n");
	if (val & GEN0_PIN_EN) printk("GEN0_PIN_EN =1.\n");
	if (val & GEN0_EPT_EN) printk("GEN0_EPT_EN =1.\n");
	if (val & GEN0_SIM1_EN) printk("GEN0_SIM1_EN =1.\n");
	if (val & GEN0_SPI_EN) printk("GEN0_SPI_EN =1.\n");
	if (val & GEN0_UART0_EN) printk("GEN0_UART0_EN =1.\n");
	if (val & GEN0_UART1_EN) printk("GEN0_UART1_EN =1.\n");
	if (val & GEN0_UART2_EN) printk("GEN0_UART2_EN =1.\n");

	val = sprd_greg_read(REG_TYPE_GLOBAL, GR_CLK_EN);
	printk("##: GR_CLK_EN = %08x.\n", val);
	if (val & CLK_PWM0_EN) printk("CLK_PWM0_EN =1.\n");
	if (val & CLK_PWM1_EN) printk("CLK_PWM1_EN = 1.\n");
	if (val & CLK_PWM2_EN) printk("CLK_PWM2_EN = 1.\n");
	if (val & CLK_PWM3_EN) printk("CLK_PWM3_EN = 1.\n");

	val = sprd_greg_read(REG_TYPE_GLOBAL, GR_BUSCLK_ALM);
	printk("##: GR_BUSCLK_ALM = %08x.\n", val);
	if (val & ARM_VB_MCLKON) printk("ARM_VB_MCLKON =1.\n");
	if (val & ARM_VB_DA0ON) printk("ARM_VB_DA0ON = 1.\n");
	if (val & ARM_VB_DA1ON) printk("ARM_VB_DA1ON = 1.\n");
	if (val & ARM_VB_ADCON) printk("ARM_VB_ADCON = 1.\n");
	if (val & ARM_VB_ANAON) printk("ARM_VB_ANAON = 1.\n");
	if (val & ARM_VB_ACC) printk("ARM_VB_ACC = 1.\n");
}

/* ANA_LDO_PD_CTL0 */
#define LDO_USB_CTL		0x2
#define LDO_SDIO0_CTL	0x8
#define LDO_SIM0_CTL	0x20
#define LDO_SIM1_CTL	0x80
#define LDO_BPCAMD0_CTL		0x200
#define LDO_BPCAMD1_CTL		0x800
#define LDO_BPCAMA_CTL		0x2000
#define LDO_BPVB_CTL		0x8000

/* ANA_LDO_PD_CTL1 */
#define LDO_SDIO1_CTL	0x2
#define LDO_BPWIF0_CTL	0x8
#define LDO_BPWIF1_CTL	0x20
#define LDO_SIM2_CTL	0x80
#define LDO_SIM3_CTL	0x200

/* ANA_AUDIO_PA_CTRL0 */
#define AUDIO_PA_ENABLE		0x1
#define AUDIO_PA_ENABLE_RST		0x2

/* ANA_AUDIO_PA_CTRL1 */
#define AUDIO_PA_LDO_ENABLE		0x100
#define AUDIO_PA_LDO_ENABLE_RST		0x200

/* sc8810 ldo register */
#define	LDO_REG_BASE		(SPRD_MISC_BASE + 0x600)
#define	ANA_LDO_PD_CTL0		(LDO_REG_BASE  + 0x10)
#define	ANA_LDO_PD_CTL1		(LDO_REG_BASE  + 0x14)
#define ANA_AUDIO_CTRL		(LDO_REG_BASE  + 0x74)
#define	ANA_AUDIO_PA_CTRL0	(LDO_REG_BASE  + 0x78)
#define	ANA_AUDIO_PA_CTRL1	(LDO_REG_BASE  + 0x7C)

static void print_ana(void)
{
	u32 val = sci_adi_read(ANA_LDO_PD_CTL0);
	printk("##: ANA_LDO_PD_CTL0 = %04x.\n", val);
	if ((val & LDO_USB_CTL)) printk("##: LDO_USB_CTL is on.\n");
	else if(!(val & (LDO_USB_CTL >> 1))) printk("##: LDO_USB_CTL is not off.\n");

	if ((val & LDO_SDIO0_CTL)) printk("##: LDO_SDIO0_CTL is on.\n");
	else if(!(val & (LDO_SDIO0_CTL >> 1))) printk("##: LDO_SDIO0_CTL is not off.\n");

	if ((val & LDO_SIM0_CTL)) printk("##: LDO_SIM0_CTL is on.\n");
	else if(!(val & (LDO_SIM0_CTL >> 1))) printk("##: LDO_SIM0_CTL is not off.\n");

	if ((val & LDO_SIM1_CTL)) printk("##: LDO_SIM1_CTL is on.\n");
	else if(!(val & (LDO_SIM1_CTL >> 1))) printk("##: LDO_SIM1_CTL is not off.\n");

	if ((val & LDO_BPCAMD0_CTL)) printk("##: LDO_BPCAMD0_CTL is on.\n");
	else if(!(val & (LDO_BPCAMD0_CTL >> 1))) printk("##: LDO_BPCAMD0_CTL is not off.\n");

	if ((val & LDO_BPCAMD1_CTL)) printk("##: LDO_BPCAMD1_CTL is on.\n");
	else if(!(val & (LDO_BPCAMD1_CTL >> 1))) printk("##: LDO_BPCAMD1_CTL is not off.\n");

	if ((val & LDO_BPCAMA_CTL)) printk("##: LDO_BPCAMA_CTL is on.\n");
	else if(!(val & (LDO_BPCAMA_CTL >> 1))) printk("##: LDO_BPCAMA_CTL is not off.\n");

	if ((val & LDO_BPVB_CTL)) printk("##: LDO_BPVB_CTL is on.\n");
	else if(!(val & (LDO_BPVB_CTL >> 1))) printk("##: LDO_BPVB_CTL is not off.\n");

	val = sci_adi_read(ANA_LDO_PD_CTL1);
	printk("##: ANA_LDO_PD_CTL1 = %04x.\n", val);
	if ((val & LDO_SDIO1_CTL)) printk("##: LDO_SDIO1_CTL is on.\n");
	else if(!(val & (LDO_SDIO1_CTL >> 1))) printk("##: LDO_SDIO1_CTL is not off.\n");

	if ((val & LDO_BPWIF0_CTL)) printk("##: LDO_BPWIF0_CTL is on.\n");
	else if(!(val & (LDO_BPWIF0_CTL >> 1))) printk("##: LDO_BPWIF0_CTL is not off.\n");

	if ((val & LDO_BPWIF1_CTL)) printk("##: LDO_BPWIF1_CTL is on.\n");
	else if(!(val & (LDO_BPWIF1_CTL >> 1))) printk("##: LDO_BPWIF1_CTL is not off.\n");

	if ((val & LDO_SIM2_CTL)) printk("##: LDO_SIM2_CTL is on.\n");
	else if(!(val & (LDO_SIM2_CTL >> 1))) printk("##: LDO_SIM2_CTL is not off.\n");

	if ((val & LDO_SIM3_CTL)) printk("##: LDO_SIM3_CTL is on.\n");
	else if(!(val & (LDO_SIM3_CTL >> 1))) printk("##: LDO_SIM3_CTL is not off.\n");


	printk("\n===========================\n");
	val = sci_adi_read(ANA_AUDIO_PA_CTRL0);
	printk("##: ANA_AUDIO_PA_CTRL0 = %04x.\n", val);
	if (val & AUDIO_PA_ENABLE)	 printk("##: Audo PA is enabled.\n");	
	else if (!(val & AUDIO_PA_ENABLE_RST)) printk("##: Audo PA is not stopped.\n");

	val = sci_adi_read(ANA_AUDIO_PA_CTRL1);
	printk("##: ANA_AUDIO_PA_CTRL1 = %04x.\n", val);
	if (val & AUDIO_PA_LDO_ENABLE)	 printk("##: Audo PA_LDO is enabled.\n");	
	else if (!(val & AUDIO_PA_LDO_ENABLE_RST)) printk("##: Audo PA_LDO is not stopped.\n");
	printk("\n===========================\n");
}

/*is dsp sleep :for debug */
static int is_dsp_sleep(void)
{
	u32 val;
	val = sprd_greg_read(REG_TYPE_GLOBAL, GR_STC_STATE);

	if (GR_DSP_STOP & val)
		printk("#####: GR_STC_STATE[DSP_STOP] is set!\n");
	else
		printk("#####: GR_STC_STATE[DSP_STOP] is NOT set!\n");
	return 0;
}

static int print_thread(void * data)
{
	while(1){
		wake_lock(&messages_wakelock);
		print_ahb();
		print_gr();
		print_ana();
		is_dsp_sleep();
		/*just print locked wake_lock*/
		has_wake_lock(WAKE_LOCK_SUSPEND);
		msleep(100);
		wake_unlock(&messages_wakelock);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(30 * HZ);
	}
	return 0;
}

static void debugfs_init(void)
{
	dentry_debug_root = debugfs_create_dir("power", NULL);
	if (IS_ERR(dentry_debug_root) || !dentry_debug_root) {
		printk("!!!powermanager Failed to create debugfs directory\n");
		dentry_debug_root = NULL;
		return;
	}
	
	debugfs_create_u32("print_sleep_mode", 0664, dentry_debug_root,
			   &is_print_sleep_mode);
	debugfs_create_u32("print_linux_clock", 0664, dentry_debug_root,
			   &is_print_linux_clock);
	debugfs_create_u32("print_modem_clock", 0664, dentry_debug_root,
			   &is_print_modem_clock);
	debugfs_create_u32("print_irq", 0664, dentry_debug_root,
			   &is_print_irq);
	debugfs_create_u32("print_wakeup", 0664, dentry_debug_root,
			   &is_print_wakeup);
	debugfs_create_u32("print_irq_runtime", 0664, dentry_debug_root,
			   &is_print_irq_runtime);
	debugfs_create_u32("print_time", 0664, dentry_debug_root,
			   &is_print_time);
}

void pm_debug_init(void)
{
	struct task_struct * task;
#ifdef PM_PRINT_ENABLE
	wake_lock_init(&messages_wakelock, WAKE_LOCK_SUSPEND,
			"pm_message_wakelock");
	task = kthread_create(print_thread, NULL, "pm_print");
	if (task == 0) {
		printk("Can't crate power manager print thread!\n");
	}else
		wake_up_process(task);
#endif
	debugfs_init();
}

void pm_debug_clr(void)
{
	if(dentry_debug_root != NULL)
		debugfs_remove_recursive(dentry_debug_root);
}

#endif

 

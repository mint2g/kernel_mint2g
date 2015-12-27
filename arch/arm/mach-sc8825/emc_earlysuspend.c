#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <mach/regs_glb.h>
#include <mach/regs_ahb.h>
#include <asm/irqflags.h>
//#include "clock_common.h"
//#include "clock_sc8810.h"
#include <mach/adi.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <mach/pm_debug.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/regs_ana_glb.h>
#include <asm/hardware/gic.h>
#include <mach/emc_repower.h>
#include <mach/sci.h>
#include <mach/regs_emc.h>
#include <linux/earlysuspend.h>
#include <mach/emc_change_freq_data.h>

#if 0
#define PUBL_REG_BASE	(0x60200000 + 0x1000)//(SPRD_LPDDR2C_BASE + 0x1000)
#define UMCTL_REG_BASE	(0x60200000)//(SPRD_LPDDR2C_BASE)

#define ADDR_AHBREG_ARMCLK    0x20900224//(REG_AHB_ARM_CLK)
#define ADDR_AHBREG_AHB_CTRL0 0x20900200//(REG_AHB_AHB_CTL0)
#define GLB_REG_WR_REG_GEN1   0x4b000018//(REG_GLB_GEN1)
#define GLB_REG_DPLL_CTRL    0x4b000040//(REG_GLB_D_PLL_CTL)
#else
#define PUBL_REG_BASE   (SPRD_LPDDR2C_BASE + 0x1000)
#define UMCTL_REG_BASE  (SPRD_LPDDR2C_BASE)
#define ADDR_AHBREG_ARMCLK    (REG_AHB_ARM_CLK)
#define ADDR_AHBREG_AHB_CTRL0 (REG_AHB_AHB_CTL0)
#define GLB_REG_WR_REG_GEN1   (REG_GLB_GEN1)
#define GLB_REG_DPLL_CTRL    (REG_GLB_D_PLL_CTL)
#endif
#define REG32(x)             (*((volatile u32 *)(x)))
#if 0
typedef enum  { Init_mem = 0, Config = 1, Config_req = 2, Access = 3, Access_req = 4, Low_power = 5,
		Low_power_entry_req = 6, Low_power_exit_req = 7
              } uPCTL_STATE_ENUM;
typedef enum  {INIT = 0, CFG = 1, GO = 2, SLEEP = 3, WAKEUP = 4} uPCTL_STATE_CMD_ENUM;
typedef enum  {LPDDR2, LPDDR1, DDR2, DDR3} MEM_TYPE_ENUM;
typedef enum  {MEM_64Mb, MEM_128Mb, MEM_256Mb, MEM_512Mb, MEM_1024Mb, MEM_2048Mb, MEM_4096Mb, MEM_8192Mb} MEM_DENSITY_ENUM;
typedef enum  {X8, X16, X32} MEM_WIDTH_ENUM;
typedef enum  {LPDDR2_S2, LPDDR2_S4} MEM_COL_TYPE_ENUM;
typedef enum  {BL2 = 2, BL4 = 4, BL8 = 8, BL16 = 16} MEM_BL_ENUM;
typedef enum  {SEQ, INTLV} MEM_BT_ENUM;

static void modify_reg_field(u32 addr, u32 start_bit, u32 bit_num, u32 value)
{
	u32 temp, i;
	temp = REG32(addr);
	for (i=0; i<bit_num; i++)
	{
		temp &= ~(1<<(start_bit+i));
	}
	temp |= value<<start_bit;
	REG32(addr) = temp;
}

static u32 polling_reg_bit_field(u32 addr, u32 start_bit, u32 bit_num, u32 value)
{
	u32 temp, i;
	u32 exp_value;
	u32 mask;
	mask = 0;
	for(i = 0; i < bit_num; i++)
	{
		mask |= (1 << (start_bit + i));
	}
	exp_value = (value << start_bit);
	do {temp = REG32(addr);}
	while((temp & mask) != exp_value);
	return temp;
}
static void wait_n_pclk_cycle(u32 num)
{
	volatile u32 i;
	u32 value_temp;
	for(i = 0; i < num; i++)
	{
		value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR);
	}
}
static void disable_clk_emc(void)
{
	//modify_reg_field(ADDR_AHBREG_AHB_CTRL0, 28, 1, 0);
	REG32(ADDR_AHBREG_AHB_CTRL0) &= ~(1 << 28); 
}

static void enable_clk_emc(void)
{
	//modify_reg_field(ADDR_AHBREG_AHB_CTRL0, 28, 1, 1);
	REG32(ADDR_AHBREG_AHB_CTRL0) |= (1 << 28); 
}

static void assert_reset_acdll(void)
{
	//modify_reg_field(PUBL_REG_BASE+PUBL_CFG_ADD_ACDLLCR, 30, 1, 0);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_ACDLLCR) &= ~(1 << 30); 
}

static void deassert_reset_acdll(void)
{
	//modify_reg_field(PUBL_REG_BASE+PUBL_CFG_ADD_ACDLLCR, 30, 1, 1);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_ACDLLCR) |= (1 << 30);
}

static void assert_reset_dxdll(void)
{
	//modify_reg_field(PUBL_REG_BASE+PUBL_CFG_ADD_DX0DLLCR, 30, 1, 0);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX0DLLCR) &= ~(1 << 30);
	//modify_reg_field(PUBL_REG_BASE+PUBL_CFG_ADD_DX1DLLCR, 30, 1, 0);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX1DLLCR) &= ~(1 << 30);
	//modify_reg_field(PUBL_REG_BASE+PUBL_CFG_ADD_DX2DLLCR, 30, 1, 0);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX2DLLCR) &= ~(1 << 30);
	//modify_reg_field(PUBL_REG_BASE+PUBL_CFG_ADD_DX3DLLCR, 30, 1, 0);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX3DLLCR) &= ~(1 << 30);
}

static void deassert_reset_dxdll(void)
{
	//modify_reg_field(PUBL_REG_BASE+PUBL_CFG_ADD_DX0DLLCR, 30, 1, 1);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX0DLLCR) |= (1 << 30);
	//modify_reg_field(PUBL_REG_BASE+PUBL_CFG_ADD_DX1DLLCR, 30, 1, 1);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX1DLLCR) |= (1 << 30);
	//modify_reg_field(PUBL_REG_BASE+PUBL_CFG_ADD_DX2DLLCR, 30, 1, 1);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX2DLLCR) |= (1 << 30);
	//modify_reg_field(PUBL_REG_BASE+PUBL_CFG_ADD_DX3DLLCR, 30, 1, 1);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX3DLLCR) |= (1 << 30);
	
}
static void assert_reset_ddrphy_dll(void)
{
	assert_reset_acdll();
	assert_reset_dxdll();
}
static void deassert_reset_ddrphy_dll(void)
{
	deassert_reset_acdll();
	deassert_reset_dxdll();
}
static void modify_dpll_freq(u32 freq)
{
	u32 temp;
	//modify_reg_field(GLB_REG_WR_REG_GEN1, 9, 1, 1);
	REG32(GLB_REG_WR_REG_GEN1) |= (1 << 9);
	//modify_reg_field(GLB_REG_DPLL_CTRL, 0, 11, temp);
	temp = freq >> 2;
	temp = REG32(GLB_REG_DPLL_CTRL);
	temp &= ~(0x7ff);
	temp |= freq >> 2;
	REG32(GLB_REG_DPLL_CTRL) = temp;
	
	//modify_reg_field(GLB_REG_WR_REG_GEN1, 9, 1, 0);
	REG32(GLB_REG_WR_REG_GEN1) &= ~(1 << 9);
}
//get the dpll frequency
static u32 get_dpll_freq_value(void)
{
	u32 temp;
	//modify_reg_field(GLB_REG_WR_REG_GEN1, 9, 1, 1);
	//step 2: get the value
	temp = 0x7ff;
	temp &= REG32(GLB_REG_DPLL_CTRL);
	temp = (temp << 2);
	//modify_reg_field(GLB_REG_WR_REG_GEN1, 9, 1, 0);
	return temp;
}
static void modify_clk_emc_div(u32 div)
{
	u32 value;
	//modify_reg_field(ADDR_AHBREG_ARMCLK, 8, 4, div);
	value = REG32(ADDR_AHBREG_ARMCLK);
	value &= ~(0xf << 8);
	value |= (div << 8);
	REG32(ADDR_AHBREG_ARMCLK) = value;
}
static void write_upctl_state_cmd(uPCTL_STATE_CMD_ENUM cmd)
{
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_SCTL) = cmd;
}

static void poll_upctl_state (uPCTL_STATE_ENUM state)
{
	uPCTL_STATE_ENUM state_poll;
	u32 value_temp;
	do
	{
		value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
		state_poll = value_temp & 0x7;
	}
	while(state_poll != state);
	return;
}
static void move_upctl_state_to_low_power(void)
{
	uPCTL_STATE_ENUM upctl_state;
	//uPCTL_STATE_CMD_ENUM  upctl_state_cmd;
	u32  tmp_val ;
	tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
	upctl_state = tmp_val & 0x7;
	while(upctl_state != Low_power)
	{
		switch(upctl_state)
		{
			case Access:
				{
					write_upctl_state_cmd(SLEEP);
					poll_upctl_state(Low_power);
					upctl_state = Low_power;
					break;
				}
			case Config:
				{
					write_upctl_state_cmd(GO);
					poll_upctl_state(Access);
					upctl_state = Access;
					break;
				}
			case Init_mem:
				{
					write_upctl_state_cmd(CFG);
					poll_upctl_state(Config);
					upctl_state = Config;
					break;
				}
			default://transitional state
				{
					tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
					upctl_state = tmp_val & 0x7;
				}
		}
	}
}
static inline void move_upctl_state_to_access(void)
{
	uPCTL_STATE_ENUM upctl_state;
	u32  tmp_val ;
	tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
	upctl_state = tmp_val & 0x7;

	while(upctl_state != Access)
	{
		switch(upctl_state)
		{
			case Access:
				{
					break;
				}
			case Config:
				{
					write_upctl_state_cmd(GO);
					poll_upctl_state(Access);
					upctl_state = Access;
					break;
				}
			case Init_mem:
				{
					write_upctl_state_cmd(CFG);
					poll_upctl_state(Config);
					upctl_state = Config;
					break;
				}
			case Low_power:
				{
					write_upctl_state_cmd(WAKEUP);
					poll_upctl_state(Access);
					upctl_state = Access;
					break;
				}
			default://transitional state
				{
					tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
					upctl_state = tmp_val & 0x7;
				}
		}
	}
}
static void reset_ddrphy_dll(void)
{
	disable_clk_emc();
	assert_reset_ddrphy_dll();
	enable_clk_emc();
	wait_n_pclk_cycle(2);
	deassert_reset_ddrphy_dll();
	polling_reg_bit_field(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR, 0, 1,1);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PIR) = 0x5;
	wait_n_pclk_cycle(5);
	polling_reg_bit_field(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR, 0, 1, 1);
}
static void disable_ddrphy_dll()
{
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_ACDLLCR, 31, 1, 0x1);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ACDLLCR) |= (1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX0DLLCR, 31, 1, 0x1);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX0DLLCR) |= (1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX1DLLCR, 31, 1, 0x1);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX1DLLCR) |= (1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX2DLLCR, 31, 1, 0x1);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX2DLLCR) |= (1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX3DLLCR, 31, 1, 0x1);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX3DLLCR) |= (1 << 31);
}
static void enable_ddrphy_dll()
{
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_ACDLLCR, 31, 1, 0x0);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ACDLLCR) &= ~(1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX0DLLCR, 31, 1, 0x0);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX0DLLCR) &= ~(1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX1DLLCR, 31, 1, 0x0);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX1DLLCR) &= ~(1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX2DLLCR, 31, 1, 0x0);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX2DLLCR) &= ~(1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX3DLLCR, 31, 1, 0x0);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX3DLLCR) &= ~(1 << 31);
}
//bps200, control the DLL disbale mode. 0, DLL freq is below 100MHz, 1:DLL freq is in 100-200MHz
static void set_ddrphy_dll_bps200_mode(u32 bps200)
{
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DLLGCR, 23, 1, bps200);
	if(bps200) {
		REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DLLGCR) |= (1 << 23);
	}
	else {
		REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DLLGCR) &= ~(1 << 23);
	}
}
static void inline   __wait_ms(u32 ms)
{
	u32 i;
	for(i = 0; i < 10; i++);
//#else
	u32 before;
	u32  time1;
	u32 time2;
	before = REG32(SPRD_SYSCNT_BASE + 0x4);
	time1 = before;
	time2 = time1;
	REG32(SPRD_UART1_BASE) = 'x';
	while(time1 < (before + ms))
	{
		if((time1 - time2) > 2) {
			time2 = time1;
	        	REG32(SPRD_UART1_BASE) = 'x';
	        }
		time1 = REG32(SPRD_SYSCNT_BASE + 0x4);
	}
}
static void dll_switch_to_disable_mode(u32 clk_emc_div)
 {
	u32 dpll_freq;
	u32 bps200;
	volatile u32 i;
	REG32(SPRD_UART1_BASE) = '6';

	dpll_freq = get_dpll_freq_value();

	switch(dpll_freq)
	{
		case 400:
			if(clk_emc_div >= 3)
			{
				bps200 = 0;
			}
			else
			{
				bps200 = 1;
			}
			break;
		case 200:
			if(clk_emc_div >= 1)
			{
				bps200 = 0;
			}
			else
			{
				bps200 = 1;
			}
			break;
		case 100:
			bps200 = 0;
			break;
		default:
			while(1);
	}
	if((clk_emc_div == 0) && (dpll_freq > 200))
	{
		while(1);
	}

	REG32(SPRD_UART1_BASE) = '0';
	__wait_ms(5);
	move_upctl_state_to_low_power();
	REG32(SPRD_UART1_BASE) = '1';
	__wait_ms(5);;
	disable_clk_emc();
	REG32(SPRD_UART1_BASE) = '2';
	__wait_ms(5);
	set_ddrphy_dll_bps200_mode(bps200);
	REG32(SPRD_UART1_BASE) = '3';
	__wait_ms(5);
	disable_ddrphy_dll();
	REG32(SPRD_UART1_BASE) = '4';
	__wait_ms(5);
	modify_clk_emc_div(clk_emc_div);//update clk_emc freq
	REG32(SPRD_UART1_BASE) = '5';
	__wait_ms(5);
	enable_clk_emc();
	REG32(SPRD_UART1_BASE) = '6';
	__wait_ms(5);
	//for(i = 0; i < 10; i++);
	move_upctl_state_to_access();
	REG32(SPRD_UART1_BASE) = '7';
	__wait_ms(5);
}
/*
static void dll_switch_to_enable_mode(u32 clk_emc_div)
{
	u32 dpll_freq;
	u32 clk_emc_freq;
	volatile u32 i;
	u32 value_temp;	
	//for(i =0 ; i < 0x80000000; i++);
	dpll_freq = get_dpll_freq_value();
	//check clk emc not less thans 100MHz
	switch(dpll_freq)
	{
		case 400:
			if(clk_emc_div > 3) {
				while(1);
			}
			break;
		case 200:
			if(clk_emc_div > 1) {
				while(1);
			}
			break;
		case 100:
			if(clk_emc_div > 0) {
				while(1);
			}
			break;
	}
	REG32(SPRD_UART1_BASE) = '0';
	for(i = 0; i < 0x1000000; i++);
	
	modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DSGCR, 3, 1, 0x0);//clear LPI0RD
	REG32(SPRD_UART1_BASE) = '1';
	for(i = 0; i < 0x1000000; i++);
	move_upctl_state_to_low_power();
	REG32(SPRD_UART1_BASE) = '2';
	for(i = 0; i < 0x1000000; i++);
	disable_clk_emc();
	REG32(SPRD_UART1_BASE) = '3';
	for(i = 0; i < 0x1000000; i++);
	if(0)
	{
		//can only do when memory in seldf refresh mode
		//publ_do_zq_calibration();
	}
	enable_ddrphy_dll();
	REG32(SPRD_UART1_BASE) = '4';
	for(i = 0; i < 0x1000000; i++);
	wait_n_pclk_cycle(4);//null read to wait 100ns per DLL reuirement
	assert_reset_ddrphy_dll();
	REG32(SPRD_UART1_BASE) = '5';
	for(i = 0; i < 0x1000000; i++);
	wait_n_pclk_cycle(2);
	modify_clk_emc_div(clk_emc_div);//update clk_emc req
	REG32(SPRD_UART1_BASE) = '6';
	for(i = 0; i < 0x1000000; i++);
	enable_clk_emc();
	REG32(SPRD_UART1_BASE) = '7';
	for(i = 0; i < 0x1000000; i++);
	wait_n_pclk_cycle(2);//let clk_emc toggle for some time before reset is released
	deassert_reset_ddrphy_dll();
	REG32(SPRD_UART1_BASE) = '8';
	for(i = 0; i < 0x1000000; i++);
	if(0){
		wait_n_pclk_cycle(100 * 1000 / 25);
	}
	else {
		polling_reg_bit_field(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR, 0, 1, 1);
		REG32(SPRD_UART1_BASE) = '9';
		for(i = 0; i < 0x1000000; i++);
		//step 4
		//DLL need 5.12us to lock
		REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PIR) = 0x5;
		wait_n_pclk_cycle(5);
		polling_reg_bit_field(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR, 0, 1, 1);
		REG32(SPRD_UART1_BASE) = 'a';
		for(i = 0; i < 0x1000000; i++);
	}
	if(0) {
	}
	else {
		//
	}
	modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DSGCR, 3, 1, 0x1);//set LPI0PD
	REG32(SPRD_UART1_BASE) = 'b';
	for(i = 0; i < 0x1000000; i++);
	move_upctl_state_to_access();
	REG32(SPRD_UART1_BASE) = 'c';
	for(i = 0; i < 0x1000000; i++);
}
*/
#if 0
static void inline uart_trace(u8 *str, u32 size)
{
	u32 i;
	for(i = 0; i < size; i++)
	{
		REG32(SPRD_UART1_BASE) = *(str + i);
	}
	i = 0; 	
	while((REG32(SPRD_UART1_BASE + 0x8) & 0x8000) == 0)
	{
		if(i >= 0x10000)
			break;
	}
}
#endif
static void dll_switch_to_enable_mode(u32 clk_emc_div)
{
	u32 dpll_freq;
	u32 clk_emc_freq;
	volatile u32 i;
	u32 value;
	u32 value_temp;	
	//for(i =0 ; i < 0x80000000; i++);
	dpll_freq = get_dpll_freq_value();
	//check clk emc not less thans 100MHz
	switch(dpll_freq)
	{
		case 400:
			if(clk_emc_div > 3) {
				while(1);
			}
			break;
		case 200:
			if(clk_emc_div > 1) {
				while(1);
			}
			break;
		case 100:
			if(clk_emc_div > 0) {
				while(1);
			}
			break;
	}
	
	REG32(SPRD_UART1_BASE) = '0';
	__wait_ms(5);
	
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DSGCR, 3, 1, 0x0);//clear LPI0RD
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DSGCR) &= ~(1 << 3);
	REG32(SPRD_UART1_BASE) = '1';
	move_upctl_state_to_low_power();
	//uart_trace("2", 1);
	REG32(SPRD_UART1_BASE) = '2';
#if 0
	value_temp = REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_STAT) ;
	//uart_trace((value_temp&0xff) + '0', 1);
    	REG32(SPRD_UART1_BASE) = '3';
#endif
	__wait_ms(5);
	//for(i = 0; i < 0x1000000; i++);

if(1) {
	disable_clk_emc();
	__wait_ms(5);
	REG32(SPRD_UART1_BASE) = '3';
	if(0)
	{
		//can only do when memory in seldf refresh mode
		//publ_do_zq_calibration();
	}
	enable_ddrphy_dll();
	//uart_trace("3", 1);
	__wait_ms(5);
	wait_n_pclk_cycle(4);//null read to wait 100ns per DLL reuirement
	assert_reset_ddrphy_dll();
	REG32(SPRD_UART1_BASE) = '4';
	//uart_trace("4", 1);
	__wait_ms(5);
	
	wait_n_pclk_cycle(2);
	modify_clk_emc_div(clk_emc_div);//update clk_emc req
	//uart_trace("5", 1);
	__wait_ms(5);
	enable_clk_emc();
	//uart_trace("6", 1);
	
	wait_n_pclk_cycle(2);//let clk_emc toggle for some time before reset is released
	deassert_reset_ddrphy_dll();
	//uart_trace("7", 1);
	
}
	if(0){
		wait_n_pclk_cycle(100 * 1000 / 25);
	}
	else {
		polling_reg_bit_field(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR, 0, 1, 1);
		REG32(SPRD_UART1_BASE) = '9';
		__wait_ms(10);
		//step 4
		//DLL need 5.12us to lock
		REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PIR) = 0x5;
		wait_n_pclk_cycle(5);
		polling_reg_bit_field(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR, 0, 1, 1);
		REG32(SPRD_UART1_BASE) = 'a';
		__wait_ms(10);
	}
	//if(0) {
	//}
	//else {
	//	//
	//}
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DSGCR, 3, 1, 0x1);//set LPI0PD
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DSGCR) |= (1 << 3);
	//REG32(SPRD_UART1_BASE) = 'b';
	//__wait_ms(20);
	//move_upctl_state_to_low_power();
	//REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_SCTL) = 0x1;
	//__wait_ms(20);
	value_temp = REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_STAT) ;
#if 0
	value = REG32(SPRD_GPIO_BASE + 0x480);
	value &= ~(1 << 15);
	REG32(SPRD_GPIO_BASE + 0x480) = value;
	__wait_ms(2);
	value |= (1 << 15);
	REG32(SPRD_GPIO_BASE + 0x480) = value;
#endif
	move_upctl_state_to_access();
	//uart_trace("9", 1);
	REG32(SPRD_UART1_BASE) = '9';
	//__wait_ms(80);

//	REG32(SPRD_UART1_BASE) = 'c';
	//for(i = 0; i < 0x1000000; i++);
}
void emc_dll_switch_to_mode(u32 enable, u32 clk_emc_div)
{
	volatile u32 i;
	//dummy read for read tlb to cache
	if(REG32(UMCTL_REG_BASE) == 0xffffffff) {
		REG32(SPRD_UART1_BASE) = 0x0;
	}
	if((REG32(PUBL_REG_BASE) == 0xffffffff)) {
		REG32(SPRD_UART1_BASE) = 0x0;
	}
	if(REG32(ADDR_AHBREG_ARMCLK) == 0xffffffff) {
		REG32(SPRD_UART1_BASE) = 0x0;
	}
	if(REG32(GLB_REG_DPLL_CTRL) == 0xffffffff) {
		REG32(SPRD_UART1_BASE) = 0x0;
	}
	if(REG32(SPRD_UART1_BASE) == 0xffffffff) {
		REG32(SPRD_UART1_BASE) = 0x0;
	}

	if(enable)
	{
		dll_switch_to_enable_mode(clk_emc_div);
	}
	else
	{
		dll_switch_to_disable_mode(clk_emc_div);
	}
}
#endif
#define EMC_SWITCH_TO_DLL_DISABLE_MODE	0x1
#define EMC_SWITCH_TO_DLL_ENABLE_MODE	0x2
#define EMC_SWITCH_MODE_COMPLETE	0x3
#define EMC_SWITCH_MODE_MASK		(0xff)
#define EMC_FREQ_DIV_OFFSET		0x8
#define EMC_FREQ_DIV_MASK		(0xf << EMC_FREQ_DIV_OFFSET)
static u32 cp_iram_addr;
void cp_code_init(void)
{
	cp_iram_addr = (u32)ioremap(0x30000, 0x1000);
	if(cp_iram_addr == 0) {
		printk("cp_code_init remap cp iram erro\n");
	}
	memcpy((void *)cp_iram_addr, cp_code_data, sizeof(cp_code_data));
}
void cp_do_change_emc_freq(u32 mode, u32 div)
{
	u32 value;
	volatile u32 i;
#ifdef CONFIG_NKERNEL
	/*delete close cp*/
	__raw_writel(0x00000000, REG_AHB_CP_SLEEP_CTRL);
#endif
	value = mode | (div << EMC_FREQ_DIV_OFFSET);
	//tell cp do disable dll mode
	__raw_writel(value, REG_AHB_JMP_ADDR_CPU0);
	
	__raw_writel(0x0, REG_AHB_CP_RST);//reset cp
	__raw_writel(0x0, REG_AHB_CP_AHB_CTL);//close cp clock, selec cp iram to ap
	for(i = 0; i < 0x100; i++);
	__raw_writel(0x7, REG_AHB_CP_AHB_CTL);//open cp clock, selec cp iram to cp
	__raw_writel(0x1, REG_AHB_CP_RST);//release cp	
}
void close_cp(void)
{
	u32 value;
	//printk("close_cp++\n");
	value = __raw_readl(REG_AHB_JMP_ADDR_CPU0);
	while((value & EMC_SWITCH_MODE_MASK) != EMC_SWITCH_MODE_COMPLETE) {
		value = __raw_readl(REG_AHB_JMP_ADDR_CPU0);
		udelay(100);
	}
#ifdef CONFIG_NKERNEL
	/*force close cp*/
	__raw_writel(0x00000001, REG_AHB_CP_SLEEP_CTRL);
#endif
	//printk("close_cp---\n");
}                           
static u32  emc_freq_early_suspend_times = 0;
static void emc_earlysuspend_early_suspend(struct early_suspend *h)
{
	//printk("emc_earlysuspend_early_suspend\n");
	//hw_local_irq_disable();
	cp_do_change_emc_freq(EMC_SWITCH_TO_DLL_DISABLE_MODE, 1);
	close_cp();
	sci_glb_set(REG_AHB_AHB_CTL1, BIT_EMC_AUTO_GATE_EN);
	//hw_local_irq_enable();
	//printk("emc_freq1-\n");
	emc_freq_early_suspend_times ++;
	//printk("emc_earlysuspend_early_suspend ---times %d\n", emc_freq_early_suspend_times);
}
static void emc_earlysuspend_late_resume(struct early_suspend *h)
{
	//printk("emc_earlysuspend_late_resume\n");
	//hw_local_irq_disable();
	sci_glb_clr(REG_AHB_AHB_CTL1, BIT_EMC_AUTO_GATE_EN);
	cp_do_change_emc_freq(EMC_SWITCH_TO_DLL_ENABLE_MODE, 0);
	close_cp();
	//hw_local_irq_enable();
	//printk("emc_earlysuspend_late_resume ---");
}
static struct early_suspend emc_early_suspend_desc = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 100,
	.suspend = emc_earlysuspend_early_suspend,
	.resume = emc_earlysuspend_late_resume,
};
//#define PM_TIMER_TEST
#ifdef PM_TIMER_TEST
static struct wake_lock test_wakelock;
static u32 emc_early_suspend_test_times = 0;
static void raw_reg_set(u32 addr, u32 bits)
{
	u32 value;
	value = __raw_readl(addr);
	value |= bits;
	__raw_writel(value, addr);
}
static void raw_reg_clr(u32 addr, u32 bits)
{
	u32 value;
	value = __raw_readl(addr);
	value &= ~bits;
	__raw_writel(value, addr);
}
u32 wake_source_start(void)
{
	u32 sys_count = get_sys_cnt();
	u32 delay = 2 * 100;
	//emc_earlysuspend_early_suspend(0);
	sci_glb_set(REG_GLB_GEN0, (1 << 19) | (1 << 27));
	__raw_writel(sys_count + delay, SPRD_SYSCNT_BASE);

	raw_reg_set(SPRD_SYSCNT_BASE + 8, 1 << 0);
	raw_reg_set(SPRD_INTC0_BASE + 8, 1 << 9);
	
	raw_reg_clr(REG_GLB_GEN0, 1 << 19);
}
u32 wake_source_stop(void)
{
	//emc_earlysuspend_late_resume(0);
	sci_glb_set(REG_GLB_GEN0, (1 << 19) | (1 << 27));
	
	raw_reg_clr(SPRD_INTC0_BASE + 8, 1 << 9);
	raw_reg_set(SPRD_SYSCNT_BASE + 8, 1 << 3);
	raw_reg_clr(SPRD_SYSCNT_BASE + 8, 1 << 0);	
}
static int pm_test_thread(void * data)
{
	while(1){
		wake_lock(&test_wakelock);
		if((emc_early_suspend_test_times % 1)) {
			emc_earlysuspend_early_suspend(0);
		}
		else {
			emc_earlysuspend_late_resume(0);
		}
		emc_early_suspend_test_times ++;
		wake_unlock(&test_wakelock);
		set_current_state(TASK_INTERRUPTIBLE);
		printk("emc_early_suspend_test_times %x", emc_early_suspend_test_times);
		schedule_timeout(2 * HZ);
	}
	return 0;
}
static void pm_test_init()
{
	struct task_struct * task;

	wake_lock_init(&test_wakelock, WAKE_LOCK_SUSPEND,
			"pm_test_wakelock");
	task = kthread_create(pm_test_thread, NULL, "pm_timer_test");
	if (task == 0) {
		printk("Can't crate power manager pm_timer_test!\n");
	}else
		wake_up_process(task);
}
#else
u32 wake_source_start(void)
{
	//do nothing
}
u32 wake_source_stop(void)
{
	//do nothing
}
#endif
static int __init emc_early_suspend_init(void)
{
#ifdef CONFIG_NKERNEL
#ifndef CONFIG_MACH_SP6825GA
	register_early_suspend(&emc_early_suspend_desc);
	cp_code_init();
	//change apb clock to 76.8MHz
	//sci_glb_set(REG_GLB_CLKDLY, 3 << 14 );
#ifdef PM_TIMER_TEST
	pm_test_init(); 
#endif
#endif
#endif
	return 0;
}
static void  __exit emc_early_suspend_exit(void)
{
#ifdef CONFIG_NKERNEL
#ifndef CONFIG_MACH_SP6825GA
	unregister_early_suspend(&emc_early_suspend_desc);
#endif
#endif
}

module_init(emc_early_suspend_init);
module_exit(emc_early_suspend_exit);

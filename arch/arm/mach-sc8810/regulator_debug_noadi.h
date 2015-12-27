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

#ifndef __ASM_ARCH_REGULATOR_DEBUG_NOADI_H
#define __ASM_ARCH_REGULATOR_DEBUG_NOADI_H

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pm.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include <linux/regulator/driver.h>

#include <mach/hardware.h>
#include <mach/regulator.h>

/*ldo registers retarget func for adi independence*/

#define	_LDO_REG_BASE		( SPRD_MISC_BASE	+ 0x600	)


#define	_LDO_PD_SET		( _LDO_REG_BASE	+ 0x8 )
static unsigned int ldo_pd_set = 0x0;

#define	_LDO_PD_RST		( _LDO_REG_BASE	+ 0xc )
static unsigned int ldo_pd_rst = 0x0;

#define	_LDO_PD_CTRL0		( _LDO_REG_BASE	+ 0x10 )
static unsigned int ldo_pd_ctrl0 = 0x0;

#define	_LDO_PD_CTRL1		( _LDO_REG_BASE	+ 0x14 )
static unsigned int ldo_pd_ctrl1  = 0x0;

#define	_LDO_VCTRL0		( _LDO_REG_BASE	+ 0x18 )
static unsigned int ldo_vctrl0 = 0x0;

#define	_LDO_VCTRL1		( _LDO_REG_BASE	+ 0x1C )
static unsigned int ldo_vctrl1 = 0x0;

#define	_LDO_VCTRL2		( _LDO_REG_BASE	+ 0x20 )
static unsigned int ldo_vctrl2 = 0x0;

#define	_LDO_VCTRL3		( _LDO_REG_BASE	+ 0x24 )
static unsigned int ldo_vctrl3 = 0x0;

#define	_LDO_VCTRL4		( _LDO_REG_BASE	+ 0x28 )
static unsigned int ldo_vctrl4 = 0x0;

#define	_LDO_SLP_CTRL0		( _LDO_REG_BASE	+ 0x2C )
static unsigned int ldo_slp_ctrl0 = 0xa7ff;

#define	_LDO_SLP_CTRL1		( _LDO_REG_BASE	+ 0x30 )
static unsigned int ldo_slp_ctrl1 = 0x701f;

#define	_LDO_SLP_CTRL2		( _LDO_REG_BASE	+ 0x34 )
static unsigned int ldo_slp_ctrl2 = 0xffff;

#define	_LDO_DCDC_CTRL		( _LDO_REG_BASE	+ 0x38 )
static unsigned int ldo_dcdc_ctrl = 0x0;

#define	_LDO_VARM_CTRL		( _LDO_REG_BASE	+ 0x44 )
static unsigned int ldo_varm_ctrl = 0x0;

#define	_LDO_PA_CTRL1		( _LDO_REG_BASE	+ 0X7C )
static unsigned int ldo_pa_ctrl1= 0x1040;

#define _LDO_REG_R(_r)		(*(unsigned int *)(_r))
#define _LDO_REG_W(_r, _v)	(*(unsigned int *)(_r)) = (_v)

#ifdef pr_debug
#undef pr_debug
#define pr_debug pr_err
#endif

static unsigned int * ldo_reg_retarget(unsigned int _r)
{
	switch(_r)
	{
		case _LDO_PD_SET:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_PD_SET,"_LDO_PD_SET");
			return &ldo_pd_set;
		case _LDO_PD_RST:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_PD_RST,"_LDO_PD_RST");
			return &ldo_pd_rst;
		case _LDO_PD_CTRL0:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_PD_CTRL0,"_LDO_PD_CTRL0");
			return &ldo_pd_ctrl0;
		case _LDO_PD_CTRL1:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_PD_CTRL1,"_LDO_PD_CTRL1");
			return &ldo_pd_ctrl1;
		case _LDO_VCTRL0:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_VCTRL0,"_LDO_VCTRL0");
			return &ldo_vctrl0;
		case _LDO_VCTRL1:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_VCTRL1,"_LDO_VCTRL1");
			return &ldo_vctrl1;
		case _LDO_VCTRL2:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_VCTRL2,"_LDO_VCTRL2");
			return &ldo_vctrl2;
		case _LDO_VCTRL3:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_VCTRL3,"_LDO_VCTRL3");
			return &ldo_vctrl3;
		case _LDO_VCTRL4:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_VCTRL4,"_LDO_VCTRL4");
			return &ldo_vctrl4;
		case _LDO_SLP_CTRL0:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_SLP_CTRL0,"_LDO_SLP_CTRL0");
			return &ldo_slp_ctrl0;
		case _LDO_SLP_CTRL1:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_SLP_CTRL1,"_LDO_SLP_CTRL1");
			return &ldo_slp_ctrl1;
		case _LDO_SLP_CTRL2:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_SLP_CTRL2,"_LDO_SLP_CTRL2");
			return &ldo_slp_ctrl2;
		case _LDO_DCDC_CTRL:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_DCDC_CTRL,"_LDO_DCDC_CTRL");
			return &ldo_dcdc_ctrl;
		case _LDO_VARM_CTRL:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_VARM_CTRL,"_LDO_VARM_CTRL");
			return &ldo_varm_ctrl;
		case _LDO_PA_CTRL1:
			pr_debug("ldo_reg_retarget: reg_addr=[0x%x], reg_name=%s\n",_LDO_PA_CTRL1,"_LDO_PA_CTRL1");
			return &ldo_pa_ctrl1;
		default:
			pr_debug("================ldo_reg_retarget error!==============\n");
			return (unsigned int *)(0xFFFF);

	}
	return 0;
}

static void sci_adi_set(unsigned int _r, unsigned int _b)
{
	unsigned int *reg;
	unsigned int val;
	pr_debug("=====================================================\n");
	pr_debug("reg_set_bit: reg_phy_addr=[0x%x], bit=0x%x\n", ((_r -_LDO_REG_BASE) + SPRD_MISC_PHYS),_b);
	reg = ldo_reg_retarget(_r);
	val = _LDO_REG_R(reg);
	pr_debug("reg_set_bit: reg_old_val=0x%x\n",val);
	val |= _b;
	_LDO_REG_W(reg, val);
	val = _LDO_REG_R(reg);
	pr_debug("reg_set_bit: done! reg_new_val=0x%x\n",val);
	pr_debug("======================================================\n");

}

static void sci_adi_clr(unsigned int _r, unsigned int _b)
{
	unsigned int *reg;
	unsigned int val;
	pr_debug("=====================================================\n");
	pr_debug("reg_clr_bit: reg_phy_addr=[0x%x], bit=0x%x\n",((_r -_LDO_REG_BASE) + SPRD_MISC_PHYS),_b);
	reg = ldo_reg_retarget(_r);
	val = _LDO_REG_R(reg);
	pr_debug("reg_clr_bit: reg_old_val=0x%x\n",val);
	val &= ~(_b);
	_LDO_REG_W(reg, val);
	val = _LDO_REG_R(reg);
	pr_debug("reg_clr_bit: done! reg_new_val=0x%x\n",val);
	pr_debug("======================================================\n");

}

static unsigned int sci_adi_read(unsigned int _r)
{
	unsigned int *reg;
	unsigned int val;
	pr_debug("=====================================================\n");
	pr_debug("reg_read: reg_phy_addr=[0x%x]\n",((_r -_LDO_REG_BASE) + SPRD_MISC_PHYS));
	reg = ldo_reg_retarget(_r);
	val = _LDO_REG_R(reg);
	pr_debug("reg_read: reg_val=0x%x\n",val);
	pr_debug("======================================================\n");
	return 	val;
}

static void sci_adi_raw_write(unsigned int _r, unsigned int _v)
{
	unsigned int *reg;
	unsigned int val;
	pr_debug("=====================================================\n");
	pr_debug("reg_write: reg_phy_addr=[0x%x], val=0x%x\n",((_r -_LDO_REG_BASE) + SPRD_MISC_PHYS),_v);
	reg = ldo_reg_retarget(_r);
	_LDO_REG_W(reg, _v);
	val = _LDO_REG_R(reg);
	pr_debug("reg_write: done! reg_new_val=0x%x\n",val);
	pr_debug("======================================================\n");

}

#endif

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

#ifndef __ASM_ARCH_REGULATOR_H
#define __ASM_ARCH_REGULATOR_H


/*REGULAOTR_NAME_XXX below is prepared ready for use in DRIVER and CUSTOM_CFG.

  In dev drivers, "V_XXX" is supplied for who want to don't include <mach/regulator.h>.
  If you prefer MACOR rather "V_XXX" in your drv, just inlcude <mach/regulator.h> and use the MACOR(REGU_NAME_XXX) below is recommended.
  example in camera_drv.c, just use like:
  [0]regulator_get(&dev, "V_CAMDVDD");  to get a ldo.

  In custom cfg, REGU_NAME_XXX or "V_XXX" are both ok, just follow existing style.
  example in regulator-sp8810ga.h, just use like:
  [1]#define REGU_NAMES_VDDCAMD0 REGULATOR_SUPPLY("V_CAMDVDD", NULL),
  note1: REGU_NAMES_VDDCAMD0 is REGU_NAMES_##PhysicalName

  note2: If suppose "VDDCAMD0"(phy) are also connected to VATVIO meanwhile,
  #define change to below:
  [2]#define REGU_NAMES_VDDCAMD0		\
	REGULATOR_SUPPLY("V_CAMDVDD",	NULL),	\
	REGULATOR_SUPPLY("V_ATVIO",		NULL),

  note3: becasue the macors below can't contain and predict all the user case,
	if a dev driver can't find a close logic name in below,
	add like "#define REGU_NAME_XXX "V_XXX"" in this file,
	then add macor like [1] or [2] in regulator-spxxxx.h,
	them use like [0] in dev_driver.c.

  notes4: more likely than note3 is case below:
	board hardware change, for example: VATVIO change to VDDRF1,
	dev driver should not do any change, the only thing to do is like below:
	in regulator-sp8810ga.h:
	#define REGU_NAMES_VDDRF1 REGULATOR_SUPPLY("V_ATVIO", NULL),
*/

#define REGU_NAME_ATV			"V_ATV"
#define REGU_NAME_ATVIO			"V_ATVIO"
#define REGU_NAME_CAMAVDD		"V_CAMAVDD"
#define REGU_NAME_CAMDVDD		"V_CAMDVDD"
#define REGU_NAME_CAMVIO		"V_CAMVIO"
#define REGU_NAME_CMMB			"V_CMMB"
#define REGU_NAME_CMMBIO		"V_CMMBIO"
#define REGU_NAME_SENSOR		"V_SENSOR"
#define REGU_NAME_MSENSOR		"V_MSENSOR"
#define REGU_NAME_MSENSORIO		"V_MSENSORIO"
#define REGU_NAME_GPS			"V_GPS"
#define REGU_NAME_GSENSOR		"V_GSENSOR"
#define REGU_NAME_GSENSORIO		"V_GSENSORIO"
#define REGU_NAME_LCD			"V_LCD"
#define REGU_NAME_LCDIO			"V_LCDIO"
#define REGU_NAME_LPSENSOR		"V_LPSENSOR"
#define REGU_NAME_LPLEDA		"V_LPSENSORLEDA"
#define REGU_NAME_SDHOST0		"V_SDHOST0"
#define REGU_NAME_SDHOST1		"V_SDHOST1"
#define REGU_NAME_TP			"V_TP"
#define REGU_NAME_TP1			"V_TP1"
#define REGU_NAME_USB			"V_USB"
#define REGU_NAME_USBD			"V_USBD"
#define REGU_NAME_WIFI			"V_WIFI"
#define REGU_NAME_WIFIIO		"V_WIFIIO"
#define REGU_NAME_BT			"V_BT"
#define REGU_NAME_FM                    "V_FM"
#define REGU_NAME_MIC			"V_MIC"


/*TABLE below is example for user understanding easily.
  A logic(l) name to physic(p) name map.
  This is only a rough reference.
  Each driver should make sure the right map is defined in regulator-spxxxx.h.

	NAME_IN_USER_DRV(l)	PHYSICAL_NAME(p)
	regulator_get(dev,"X")	board-sp6820a	board-sp8810ga
	colume below is X:
	"V_ATV"			NA(ext ldo)	NA
	"V_ATVIO"		NA(ext ldo)	NA
	"V_CAMAVDD"		"VDDCAMD1"	"VDDCAMA"
	"V_CAMDVDD"		"VDDCAMD0"	"VDDCAMD0"
	"V_CAMVIO"		"VDDCAMA"	"VDDCAMD1"
	"V_CMMB"		NA		"VDDSIM3"
	"V_CMMBIO"		NA		"VDDRF1"
	"V_MSENSOR"		"VDD28"		"VDD28"
	"V_MSENSORIO"		"VDD18"		"VDD18"
	"V_GPS"			"VDDWIF0"	"VDDWIF0"
	"V_GSENSOR"		"VDD28"		"VDD28"
	"V_GSENSORIO"		"VDD18"		"VDD18"
	"V_LCD"			"VDD28"		"VDD28"
	"V_LCDIO"		"VDD28"		"VDD28"
	"V_LPSENSOR"		"VDD18"		"VDD18"
	"V_LPSENSORLEDA"	"VDD28"		"VDD28"
	"V_SDHOST0"		"VDDSD0"	"VDDSD0"
	"V_SDHOST1"		NA		NA
	"V_TP"			"VDDSIM2"	"VDDSIM2"
	"V_USB"			"VDDUSB"	"VDDUSB"
	"V_USBD"		"VDDUSBD"	"VDDUSBD"
	"V_WIFI"		"VDDWIF1"	"VDDWIF1"
	"V_WIFIIO"		"VDDSD1"	"VDDSD1"

  notes: if physical name is "VDD18" or "VDD28", user should not
	define this cfg in regulator-spxxxx.h, because both should be always on.
*/


/* REGULATOR INIT STATUS CFG RELATED*/
enum regulator_init_onoff_state{
	LDO_INIT_ON = 0,
	LDO_INIT_OFF,
	LDO_INIT_MAX,
};

enum regulator_init_autosleep_state{
	LDO_INIT_SLP_ON = 0,
	LDO_INIT_SLP_OFF,
	LDO_INIT_SLP_MAX,
};

#define	LDO_INIT_NA	(0xFFFFFFFF)	/* NA means dont need change */

#define REGU_INIT_STATUS(_ldo, _on_off, _vol_uv, _autosleep)	\
	[_ldo]	= {						\
		.ldo_id		= _ldo,				\
		.on_off		= _on_off,			\
		.vol_uv		= _vol_uv,			\
		.autosleep	= _autosleep,			\
	}


/*The file below is internal layer, user and customer ignore it*/
struct regulator_init_status {
	const unsigned int ldo_id;
	const unsigned int on_off;
	const unsigned int vol_uv;
	const unsigned int autosleep;
};


/*TOTAL 27 REGULATORS IN SC8810: 2 DCDC AND 25 LDO*/
enum regulator_supply_source{
	LDO_VDDARM	= 0,	/*	"VDDARM"	*/
	LDO_VDD25,		/*	"VDD25"		*/
	LDO_VDD18,		/*	"VDD18"		*/
	LDO_VDD28,		/*	"VDD28"		*/
	LDO_AVDDBB,		/*	"AVDDBB"	*/
	LDO_VDDRF0,		/*	"VDDRF0"	*/
	LDO_VDDRF1,		/*	"VDDRF1"	*/
	LDO_VDDMEM,		/*	"VDDMEM"	*/
	LDO_VDDCORE,		/*	"VDDCORE"	*/
	LDO_LDO_BG,		/*	"LDO_BG"	*/
	LDO_AVDDVB,		/*	"AVDDVB"	*/
	LDO_VDDCAMDA,		/*	"VDDCAMDA"	*/
	LDO_VDDCAMD1,		/*	"VDDCAMD1"	*/
	LDO_VDDCAMD0,		/*	"VDDCAMD0"	*/
	LDO_VDDSIM1,		/*	"VDDSIM1"	*/
	LDO_VDDSIM0,		/*	"VDDSIM0"	*/
	LDO_VDDSD0,		/*	"VDDSD0"	*/
	LDO_VDDUSB,		/*	"VDDUSB"	*/
	LDO_VDDUSBD,		/*	"VDDUSBD"	*/
	LDO_VDDSIM3,		/*	"VDDSIM3"	*/
	LDO_VDDSIM2,		/*	"VDDSIM2"	*/
	LDO_VDDWIF1,		/*	"VDDWIF1"	*/
	LDO_VDDWIF0,		/*	"VDDWIF0"	*/
	LDO_VDDSD1,		/*	"VDDSD1"	*/
	LDO_VDDRTC,		/*	"VDDRTC"	*/
	LDO_DVDD18,		/*	"DVDD18"	*/
	LDO_LDO_PA,		/*	"LDO_PA"	*/
	LDO_END_MAX,
};

#endif

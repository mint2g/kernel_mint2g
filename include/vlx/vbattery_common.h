/*
 ****************************************************************
 *
 *  Component: VLX virtual battery interface
 *
 *  Copyright (C) 2011, Red Bend Ltd.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  You should have received a copy of the GNU General Public License Version 2
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *  Contributor(s):
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *    Christophe Lizzi (christophe.lizzi@redbend.com)
 *
 ****************************************************************
 */

#ifndef _VBATTERY_COMMON_H_
#define _VBATTERY_COMMON_H_

#define	VBAT_VRPC_NAME	"vbattery"

    // VBAT RPC request
typedef struct vbat_req_t {
    nku32_f cmd;	// command
    nku32_f arg;	// argument (optional)
} vbat_req_t;

    // VBAT RPC result
typedef struct vbat_res_t {
    nku32_f res;	// result
    nku32_f value;	// value
} vbat_res_t;

typedef enum {
    VBAT_CMD_GET_NAME,
    VBAT_CMD_GET_VTYPE,
    VBAT_CMD_GET_VPROP_MAX,
    VBAT_CMD_GET_VPROP_ID,
    VBAT_CMD_GET_VPROP_VAL,
    VBAT_CMD_SET_MODE,
    VBAT_CMD_MAX
} vbat_cmd_t;

#define VBAT_CMD_NAME \
	{"name", "vtype", "vprop_max", "vprop_id", "vprop_val", "set_mode"}

typedef enum {
    VBAT_MODE_INTR,
    VBAT_MODE_POLL,
    VBAT_MODE_POLL_ONLY,
    VBAT_MODE_POLL_MAX,
    VBAT_MODE_POLL_LAST = 0x7FFFFFFF
} vbat_mode_t;

#define VBAT_MODE_NAME {"intr", "poll", "poll_only"}

typedef enum {
    VBAT_POWER_SUPPLY_TYPE_BATTERY,
    VBAT_POWER_SUPPLY_TYPE_UPS,
    VBAT_POWER_SUPPLY_TYPE_MAINS,
    VBAT_POWER_SUPPLY_TYPE_USB,

	/* Vendor-specific types */
    VBAT_POWER_SUPPLY_TYPE_FACTROY,	/* Original typo */
    VBAT_POWER_SUPPLY_TYPE_UNKNOWN,

    VBAT_POWER_SUPPLY_TYPE_MAX,
    VBAT_POWER_SUPPLY_TYPE_LAST = 0x7FFFFFFF
} vbat_power_supply_type_t;

#define VBAT_POWER_SUPPLY_TYPE_NAME \
	{"battery", "UPS", "mains", "usb", "factroy", "unknown"}

typedef enum {
    VBAT_POWER_SUPPLY_STATUS_UNKNOWN,
    VBAT_POWER_SUPPLY_STATUS_CHARGING,
    VBAT_POWER_SUPPLY_STATUS_DISCHARGING,
    VBAT_POWER_SUPPLY_STATUS_NOT_CHARGING,
    VBAT_POWER_SUPPLY_STATUS_FULL,
    VBAT_POWER_SUPPLY_STATUS_MAX,
    VBAT_POWER_SUPPLY_STATUS_LAST = 0x7FFFFFFF
} vbat_power_supply_status_t;

#define VBAT_POWER_SUPPLY_STATUS_NAME \
	{"unknown", "charging", "discharging", "not charging", "full"}

typedef enum {
    VBAT_POWER_SUPPLY_HEALTH_UNKNOWN,
    VBAT_POWER_SUPPLY_HEALTH_GOOD,
    VBAT_POWER_SUPPLY_HEALTH_OVERHEAT,
    VBAT_POWER_SUPPLY_HEALTH_DEAD,
    VBAT_POWER_SUPPLY_HEALTH_OVERVOLTAGE,
    VBAT_POWER_SUPPLY_HEALTH_UNSPEC_FAILURE,
    VBAT_POWER_SUPPLY_HEALTH_COLD,	/* 2.6.29 */
    VBAT_POWER_SUPPLY_HEALTH_MAX,
    VBAT_POWER_SUPPLY_HEALTH_LAST = 0x7FFFFFFF
} vbat_power_supply_health_t;

#define VBAT_POWER_SUPPLY_HEALTH_NAME \
	{"unknown", "good", "overheat", "dead", \
	"overvoltage", "unspec failure", "cold"}

typedef enum {
    VBAT_POWER_SUPPLY_TECHNOLOGY_UNKNOWN,
    VBAT_POWER_SUPPLY_TECHNOLOGY_NiMH,
    VBAT_POWER_SUPPLY_TECHNOLOGY_LION,
    VBAT_POWER_SUPPLY_TECHNOLOGY_LIPO,
    VBAT_POWER_SUPPLY_TECHNOLOGY_LiFe,
    VBAT_POWER_SUPPLY_TECHNOLOGY_NiCd,
    VBAT_POWER_SUPPLY_TECHNOLOGY_LiMn,
    VBAT_POWER_SUPPLY_TECHNOLOGY_MAX,
    VBAT_POWER_SUPPLY_TECHNOLOGY_LAST = 0x7FFFFFFF
} vbat_power_supply_technology_t;

#define VBAT_POWER_SUPPLY_TECHNOLOGY_NAME \
	{"unknown", "NiMH", "LION", "LIPO", "LiFe", "NiCd", "LiMn"}

    /* Appeared in 2.6.32 */
typedef enum {
    VBAT_POWER_SUPPLY_CHARGE_TYPE_UNKNOWN,
    VBAT_POWER_SUPPLY_CHARGE_TYPE_NONE,
    VBAT_POWER_SUPPLY_CHARGE_TYPE_TRICKLE,
    VBAT_POWER_SUPPLY_CHARGE_TYPE_FAST,
    VBAT_POWER_SUPPLY_CHARGE_TYPE_MAX,
    VBAT_POWER_SUPPLY_CHARGE_TYPE_LAST = 0x7FFFFFFF
} vbat_power_supply_charge_type_t;

#define VBAT_POWER_SUPPLY_CHARGE_TYPE_NAME \
	{"unknown", "none", "trickle", "fast"}

    /* Appeared in 2.6.32 */
typedef enum {
    VBAT_POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN,
    VBAT_POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL,
    VBAT_POWER_SUPPLY_CAPACITY_LEVEL_LOW,
    VBAT_POWER_SUPPLY_CAPACITY_LEVEL_NORMAL,
    VBAT_POWER_SUPPLY_CAPACITY_LEVEL_HIGH,
    VBAT_POWER_SUPPLY_CAPACITY_LEVEL_FULL,
    VBAT_POWER_SUPPLY_CAPACITY_LEVEL_MAX,
    VBAT_POWER_SUPPLY_CAPACITY_LEVEL_LAST
} vbat_power_supply_capacity_level_t;

#define VBAT_POWER_SUPPLY_CAPACITY_LEVEL_NAME \
	{"unknown", "critical", "low", "normal", "high", "full"}

typedef enum {
	/* Properties of type `int' */
    VBAT_POWER_SUPPLY_PROP_STATUS,
    VBAT_POWER_SUPPLY_PROP_HEALTH,
    VBAT_POWER_SUPPLY_PROP_PRESENT,
    VBAT_POWER_SUPPLY_PROP_ONLINE,
    VBAT_POWER_SUPPLY_PROP_TECHNOLOGY,
    VBAT_POWER_SUPPLY_PROP_VOLTAGE_MAX,		/* 2.6.25 */
    VBAT_POWER_SUPPLY_PROP_VOLTAGE_MIN,		/* 2.6.25 */
    VBAT_POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
    VBAT_POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
    VBAT_POWER_SUPPLY_PROP_VOLTAGE_NOW,
    VBAT_POWER_SUPPLY_PROP_VOLTAGE_AVG,
    VBAT_POWER_SUPPLY_PROP_CURRENT_NOW,
    VBAT_POWER_SUPPLY_PROP_CURRENT_AVG,
    VBAT_POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
    VBAT_POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN,
    VBAT_POWER_SUPPLY_PROP_CHARGE_FULL,
    VBAT_POWER_SUPPLY_PROP_CHARGE_EMPTY,
    VBAT_POWER_SUPPLY_PROP_CHARGE_NOW,
    VBAT_POWER_SUPPLY_PROP_CHARGE_AVG,
    VBAT_POWER_SUPPLY_PROP_CHARGE_COUNTER,	/* 2.6.27 */
    VBAT_POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
    VBAT_POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN,
    VBAT_POWER_SUPPLY_PROP_ENERGY_FULL,
    VBAT_POWER_SUPPLY_PROP_ENERGY_EMPTY,
    VBAT_POWER_SUPPLY_PROP_ENERGY_NOW,
    VBAT_POWER_SUPPLY_PROP_ENERGY_AVG,
    VBAT_POWER_SUPPLY_PROP_CAPACITY, /* in percents */
    VBAT_POWER_SUPPLY_PROP_TEMP,
    VBAT_POWER_SUPPLY_PROP_TEMP_AMBIENT,
    VBAT_POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
    VBAT_POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
    VBAT_POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
    VBAT_POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	/* Properties of type `const char *' */
    VBAT_POWER_SUPPLY_PROP_MODEL_NAME,
    VBAT_POWER_SUPPLY_PROP_MANUFACTURER,
    VBAT_POWER_SUPPLY_PROP_SERIAL_NUMBER,
	/* Properties of type `int' again */
    VBAT_POWER_SUPPLY_PROP_CYCLE_COUNT,		/* 2.6.34 */
    VBAT_POWER_SUPPLY_PROP_TYPE,		/* 2.6.35 */
    VBAT_POWER_SUPPLY_PROP_CHARGE_TYPE,		/* 2.6.32 */
    VBAT_POWER_SUPPLY_PROP_POWER_NOW,		/* 2.6.30 */
    VBAT_POWER_SUPPLY_PROP_POWER_AVG,		/* 2.6.30 */
    VBAT_POWER_SUPPLY_PROP_CAPACITY_LEVEL,	/* 2.6.32 */

	/* Vendor-specific properties */
    VBAT_POWER_SUPPLY_PROP_PMIC_SOC,
    VBAT_POWER_SUPPLY_PROP_GAUGE_VOLTAGE,
    VBAT_POWER_SUPPLY_PROP_GAUGE_CONTROL,
    VBAT_POWER_SUPPLY_PROP_GAUGE_CONTROL_COUNT,
    VBAT_POWER_SUPPLY_PROP_CHARGER_MODE,
    VBAT_POWER_SUPPLY_PROP_CHARGER_TEMP_CONTROL,

    VBAT_POWER_SUPPLY_PROP_MAX,
    VBAT_POWER_SUPPLY_PROP_LAST = 0x7FFFFFFF
} vbat_power_supply_property_t;

#define VBAT_POWER_SUPPLY_PROPERTY_NAME \
	{"status", "health", "present", "online", \
	"technology", "voltage max", "voltage min", "voltage max design",\
	"voltage min design", "voltage now", "voltage avg", "current now",\
	"current avg", "charge full design", "charge empty design",\
	"charge full",\
	"charge empty", "charge now", "charge avg", "charge counter",\
	"energy full design", "energy empty design", "energy full",\
	"energy empty",\
	"energy now", "energy avg", "capacity", "temp",\
	"temp ambient", "time to empty now", "time to empty avg",\
	"time to full now",\
	"time to full avg", "model name", "manufacturer", "serial number",\
	"cycle count", "type", "charge type", "power now", "power avg",\
	"capacity level", "pmic soc", "gauge voltage", "gauge control",\
	"gauge control count", "charger mode", "charger temp control"}

#if defined VBATTERY_FE || defined VBATTERY_BE

#include <linux/version.h>

#define _VBAT_CASE(n)  case VBAT_##n: return n

#ifdef VBATTERY_FE
    static enum power_supply_type
vbat_vtype2type (const vbat_power_supply_type_t vtype)
{
    switch (vtype) {
    _VBAT_CASE (POWER_SUPPLY_TYPE_BATTERY);
    _VBAT_CASE (POWER_SUPPLY_TYPE_UPS);
    _VBAT_CASE (POWER_SUPPLY_TYPE_MAINS);
    _VBAT_CASE (POWER_SUPPLY_TYPE_USB);

#ifdef CONFIG_VBATTERY_VENDOR_SPECIFIC_1
	/* Vendor-specific types */
    _VBAT_CASE (POWER_SUPPLY_TYPE_FACTROY);
    _VBAT_CASE (POWER_SUPPLY_TYPE_UNKNOWN);
#endif

    default: break;
    }
    return (enum power_supply_type) -1;
}

    static int
vbat_power_supply_vstatus2status (const vbat_power_supply_status_t vstatus)
{
    switch (vstatus) {
    _VBAT_CASE (POWER_SUPPLY_STATUS_UNKNOWN);
    _VBAT_CASE (POWER_SUPPLY_STATUS_CHARGING);
    _VBAT_CASE (POWER_SUPPLY_STATUS_DISCHARGING);
    _VBAT_CASE (POWER_SUPPLY_STATUS_NOT_CHARGING);
    _VBAT_CASE (POWER_SUPPLY_STATUS_FULL);
    default: break;
    }
    return -1;
}

    static int
vbat_power_supply_vhealth2health (const vbat_power_supply_health_t vhealth)
{
    switch (vhealth) {
    _VBAT_CASE (POWER_SUPPLY_HEALTH_UNKNOWN);
    _VBAT_CASE (POWER_SUPPLY_HEALTH_GOOD);
    _VBAT_CASE (POWER_SUPPLY_HEALTH_OVERHEAT);
    _VBAT_CASE (POWER_SUPPLY_HEALTH_DEAD);
    _VBAT_CASE (POWER_SUPPLY_HEALTH_OVERVOLTAGE);
    _VBAT_CASE (POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,29)
    _VBAT_CASE (POWER_SUPPLY_HEALTH_COLD);
#endif
    default: break;
    }
    return -1;
}

    static int
vbat_power_supply_vtechnology2technology (const vbat_power_supply_technology_t
					  vtechnology)
{
    switch (vtechnology) {
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_UNKNOWN);
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_NiMH);
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_LION);
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_LIPO);
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_LiFe);
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_NiCd);
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_LiMn);
#endif
    default: break;
    }
    return -1;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,32)
    static int
vbat_power_supply_vcharge_type2charge_type
			(const vbat_power_supply_charge_type_t vcharge_type)
{
    switch (vcharge_type) {
    _VBAT_CASE (POWER_SUPPLY_CHARGE_TYPE_UNKNOWN);
    _VBAT_CASE (POWER_SUPPLY_CHARGE_TYPE_NONE);
    _VBAT_CASE (POWER_SUPPLY_CHARGE_TYPE_TRICKLE);
    _VBAT_CASE (POWER_SUPPLY_CHARGE_TYPE_FAST);
    default: break;
    }
    return -1;
}
#endif

    static int
vbat_power_supply_vcapacity_level2capacity_level
		    (const vbat_power_supply_capacity_level_t vcapacity_level)
{
    switch (vcapacity_level) {
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN);
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL);
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_LOW);
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_NORMAL);
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_HIGH);
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_FULL);
    default: break;
    }
    return -1;
}
#endif

    static enum power_supply_property
vbat_vproperty2property (const vbat_power_supply_property_t vproperty)
{
    switch (vproperty) {
    _VBAT_CASE (POWER_SUPPLY_PROP_STATUS);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,32)
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_TYPE);
#endif
    _VBAT_CASE (POWER_SUPPLY_PROP_HEALTH);
    _VBAT_CASE (POWER_SUPPLY_PROP_PRESENT);
    _VBAT_CASE (POWER_SUPPLY_PROP_ONLINE);
    _VBAT_CASE (POWER_SUPPLY_PROP_TECHNOLOGY);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,34)
    _VBAT_CASE (POWER_SUPPLY_PROP_CYCLE_COUNT);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,25)
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_MAX);
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_MIN);
#endif
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_AVG);
    _VBAT_CASE (POWER_SUPPLY_PROP_CURRENT_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_CURRENT_AVG);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,30)
    _VBAT_CASE (POWER_SUPPLY_PROP_POWER_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_POWER_AVG);
#endif
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_FULL);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_EMPTY);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_AVG);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_COUNTER);
#endif
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_FULL);
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_EMPTY);
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_AVG);
    _VBAT_CASE (POWER_SUPPLY_PROP_CAPACITY);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,32)
    _VBAT_CASE (POWER_SUPPLY_PROP_CAPACITY_LEVEL);
#endif
    _VBAT_CASE (POWER_SUPPLY_PROP_TEMP);
    _VBAT_CASE (POWER_SUPPLY_PROP_TEMP_AMBIENT);
    _VBAT_CASE (POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG);
    _VBAT_CASE (POWER_SUPPLY_PROP_TIME_TO_FULL_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_TIME_TO_FULL_AVG);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,35)
    _VBAT_CASE (POWER_SUPPLY_PROP_TYPE);
#endif

#ifdef CONFIG_VBATTERY_VENDOR_SPECIFIC_1
	/* Vendor-specific properties */
    _VBAT_CASE (POWER_SUPPLY_PROP_PMIC_SOC);
    _VBAT_CASE (POWER_SUPPLY_PROP_GAUGE_VOLTAGE);
    _VBAT_CASE (POWER_SUPPLY_PROP_GAUGE_CONTROL);
    _VBAT_CASE (POWER_SUPPLY_PROP_GAUGE_CONTROL_COUNT);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGER_MODE);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGER_TEMP_CONTROL);
#endif

	/* Properties of type `const char *' */
    _VBAT_CASE (POWER_SUPPLY_PROP_MODEL_NAME);
    _VBAT_CASE (POWER_SUPPLY_PROP_MANUFACTURER);
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    _VBAT_CASE (POWER_SUPPLY_PROP_SERIAL_NUMBER);
#endif
    default: break;
    }
    return (enum power_supply_property) -1;
}

#undef _VBAT_CASE
#define _VBAT_CASE(n)  case n: return VBAT_##n

#ifdef VBATTERY_BE
    static vbat_power_supply_type_t
vbat_type2vtype (const enum power_supply_type type)
{
    switch (type) {
    _VBAT_CASE (POWER_SUPPLY_TYPE_BATTERY);
    _VBAT_CASE (POWER_SUPPLY_TYPE_UPS);
    _VBAT_CASE (POWER_SUPPLY_TYPE_MAINS);
    _VBAT_CASE (POWER_SUPPLY_TYPE_USB);

#ifdef CONFIG_VBATTERY_VENDOR_SPECIFIC_1
	/* Vendor-specific types */
    _VBAT_CASE (POWER_SUPPLY_TYPE_FACTROY);
    _VBAT_CASE (POWER_SUPPLY_TYPE_UNKNOWN);
#endif

    default: break;
    }
    return (vbat_power_supply_type_t) -1;
}

    static vbat_power_supply_status_t
vbat_power_supply_status2vstatus (const int status)
{
    switch (status) {
    _VBAT_CASE (POWER_SUPPLY_STATUS_UNKNOWN);
    _VBAT_CASE (POWER_SUPPLY_STATUS_CHARGING);
    _VBAT_CASE (POWER_SUPPLY_STATUS_DISCHARGING);
    _VBAT_CASE (POWER_SUPPLY_STATUS_NOT_CHARGING);
    _VBAT_CASE (POWER_SUPPLY_STATUS_FULL);
    default: break;
    }
    return -1;
}

    static vbat_power_supply_health_t
vbat_power_supply_health2vhealth (const int health)
{
    switch (health) {
    _VBAT_CASE (POWER_SUPPLY_HEALTH_UNKNOWN);
    _VBAT_CASE (POWER_SUPPLY_HEALTH_GOOD);
    _VBAT_CASE (POWER_SUPPLY_HEALTH_OVERHEAT);
    _VBAT_CASE (POWER_SUPPLY_HEALTH_DEAD);
    _VBAT_CASE (POWER_SUPPLY_HEALTH_OVERVOLTAGE);
    _VBAT_CASE (POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,29)
    _VBAT_CASE (POWER_SUPPLY_HEALTH_COLD);
#endif
    default: break;
    }
    return -1;
}

    static vbat_power_supply_technology_t
vbat_power_supply_technology2vtechnology (const int technology)
{
    switch (technology) {
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_UNKNOWN);
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_NiMH);
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_LION);
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_LIPO);
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_LiFe);
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_NiCd);
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    _VBAT_CASE (POWER_SUPPLY_TECHNOLOGY_LiMn);
#endif
    default: break;
    }
    return -1;
}

    static int
vbat_power_supply_charge_type2vcharge_type (const int charge_type)
{
    switch (charge_type) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,32)
    _VBAT_CASE (POWER_SUPPLY_CHARGE_TYPE_UNKNOWN);
    _VBAT_CASE (POWER_SUPPLY_CHARGE_TYPE_NONE);
    _VBAT_CASE (POWER_SUPPLY_CHARGE_TYPE_TRICKLE);
    _VBAT_CASE (POWER_SUPPLY_CHARGE_TYPE_FAST);
#endif
    default: break;
    }
    return -1;
}

    static int
vbat_power_supply_capacity_level2vcapacity_level (const int capacity_level)
{
    switch (capacity_level) {
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN);
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL);
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_LOW);
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_NORMAL);
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_HIGH);
    _VBAT_CASE (POWER_SUPPLY_CAPACITY_LEVEL_FULL);
    default: break;
    }
    return -1;
}
#endif

    static vbat_power_supply_property_t
vbat_property2vproperty (const enum power_supply_property property)
{
    switch (property) {
    _VBAT_CASE (POWER_SUPPLY_PROP_STATUS);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,32)
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_TYPE);
#endif
    _VBAT_CASE (POWER_SUPPLY_PROP_HEALTH);
    _VBAT_CASE (POWER_SUPPLY_PROP_PRESENT);
    _VBAT_CASE (POWER_SUPPLY_PROP_ONLINE);
    _VBAT_CASE (POWER_SUPPLY_PROP_TECHNOLOGY);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,34)
    _VBAT_CASE (POWER_SUPPLY_PROP_CYCLE_COUNT);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,25)
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_MAX);
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_MIN);
#endif
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_VOLTAGE_AVG);
    _VBAT_CASE (POWER_SUPPLY_PROP_CURRENT_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_CURRENT_AVG);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,30)
    _VBAT_CASE (POWER_SUPPLY_PROP_POWER_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_POWER_AVG);
#endif
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_FULL);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_EMPTY);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_AVG);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGE_COUNTER);
#endif
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN);
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_FULL);
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_EMPTY);
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_ENERGY_AVG);
    _VBAT_CASE (POWER_SUPPLY_PROP_CAPACITY);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,32)
    _VBAT_CASE (POWER_SUPPLY_PROP_CAPACITY_LEVEL);
#endif
    _VBAT_CASE (POWER_SUPPLY_PROP_TEMP);
    _VBAT_CASE (POWER_SUPPLY_PROP_TEMP_AMBIENT);
    _VBAT_CASE (POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG);
    _VBAT_CASE (POWER_SUPPLY_PROP_TIME_TO_FULL_NOW);
    _VBAT_CASE (POWER_SUPPLY_PROP_TIME_TO_FULL_AVG);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,35)
    _VBAT_CASE (POWER_SUPPLY_PROP_TYPE);
#endif

#ifdef CONFIG_VBATTERY_VENDOR_SPECIFIC_1
	/* Vendor-specific properties */
    _VBAT_CASE (POWER_SUPPLY_PROP_PMIC_SOC);
    _VBAT_CASE (POWER_SUPPLY_PROP_GAUGE_VOLTAGE);
    _VBAT_CASE (POWER_SUPPLY_PROP_GAUGE_CONTROL);
    _VBAT_CASE (POWER_SUPPLY_PROP_GAUGE_CONTROL_COUNT);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGER_MODE);
    _VBAT_CASE (POWER_SUPPLY_PROP_CHARGER_TEMP_CONTROL);
#endif

	/* Properties of type `const char *' */
    _VBAT_CASE (POWER_SUPPLY_PROP_MODEL_NAME);
    _VBAT_CASE (POWER_SUPPLY_PROP_MANUFACTURER);
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    _VBAT_CASE (POWER_SUPPLY_PROP_SERIAL_NUMBER);
#endif
    default: break;
    }
    return (vbat_power_supply_property_t) -1;
}

#undef _VBAT_CASE

#endif

#endif /* _VBATTERY_COMMON_H_ */

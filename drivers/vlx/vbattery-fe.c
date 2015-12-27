/*
 ****************************************************************
 *
 *  Component: VLX virtual battery frontend driver
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
 *    Emre Eraltan (emre.eraltan@redbend.com)
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *    Christophe Lizzi (christophe.lizzi@redbend.com)
 *
 ****************************************************************
 */

#if 0
    /*
     *  Isolated 2.6.32 does not currently create a vbattery device,
     *  so we do not declare this driver as platform for now either.
     */
#define VBATTERY_PLATFORM_DEVICE
#endif

#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#define VBATTERY_FE

#include <nk/nkern.h>
#include <vlx/vbattery_common.h>
#include "vrpc.h"

#if 0
#define VBATTERY_DEBUG
#endif

#define TRACE(format, args...)	printk("VBAT-FE: " format, ## args)
#define ETRACE(format, args...) printk("VBAT-FE: [E] " format, ## args)

#ifdef VBATTERY_DEBUG
#define DTRACE(format, args...) printk("VBAT-FE: [D] " format, ## args)
#define DFTRACE(format, args...) printk("%s: " format, __func__, ## args)
#else
#define DTRACE(format, args...)
#define DFTRACE(format, args...)
#endif

typedef struct vbat_vprop_t {
    vbat_power_supply_property_t	vproperty;
    struct vbat_vprop_t*		next;
    char				data[1];
} vbat_vprop_t;

typedef struct vbat_t {
    struct power_supply     psy;
#ifdef VBATTERY_PLATFORM_DEVICE
    struct platform_device* pdev;
#endif
    struct vrpc_t*          vrpc;
    void*                   data;
    vrpc_size_t             msize;
    vbat_vprop_t*           vprops;
    struct mutex            prop_mutex;
    struct mutex            call_mutex;
    struct delayed_work     work;
    vbat_mode_t             poll;
    NkXIrq                  cxirq;
    NkXIrqId                cxid;
} vbat_t;

static void _vbat_ready (void* cookie);

static unsigned int cache_time = 5000;
module_param(cache_time, uint, 0644);
MODULE_PARM_DESC(cache_time, "cache time in milliseconds");

#define	VBAT_POLLING_PERIOD	msecs_to_jiffies(cache_time)

#define VBAT_STATIC_ASSERT(x)	extern char vbat_static_assert [(x) ? 1 : -1]
#define VBAT_ARRAY_ELEMS(x)	(sizeof x / sizeof x [0])

#ifdef VBATTERY_DEBUG
static const char* const vbat_cmd_name[] =
		    VBAT_CMD_NAME;
VBAT_STATIC_ASSERT (VBAT_ARRAY_ELEMS (vbat_cmd_name) ==
		    VBAT_CMD_MAX);

static const char* const vbat_power_supply_type_name[] =
		    VBAT_POWER_SUPPLY_TYPE_NAME;
VBAT_STATIC_ASSERT (VBAT_ARRAY_ELEMS (vbat_power_supply_type_name) ==
		    VBAT_POWER_SUPPLY_TYPE_MAX);

static const char* const vbat_power_supply_status_name[] =
		   VBAT_POWER_SUPPLY_STATUS_NAME;
VBAT_STATIC_ASSERT (VBAT_ARRAY_ELEMS (vbat_power_supply_status_name) ==
		    VBAT_POWER_SUPPLY_STATUS_MAX);

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,32)
static const char* const vbat_power_supply_charge_type_name[] =
		   VBAT_POWER_SUPPLY_CHARGE_TYPE_NAME;
VBAT_STATIC_ASSERT (VBAT_ARRAY_ELEMS (vbat_power_supply_charge_type_name) ==
		    VBAT_POWER_SUPPLY_CHARGE_TYPE_MAX);
#endif

static const char* const vbat_power_supply_health_name[] =
		   VBAT_POWER_SUPPLY_HEALTH_NAME;
VBAT_STATIC_ASSERT (VBAT_ARRAY_ELEMS (vbat_power_supply_health_name) ==
		    VBAT_POWER_SUPPLY_HEALTH_MAX);

static const char* const vbat_power_supply_technology_name[] =
		   VBAT_POWER_SUPPLY_TECHNOLOGY_NAME;
VBAT_STATIC_ASSERT (VBAT_ARRAY_ELEMS (vbat_power_supply_technology_name) ==
		    VBAT_POWER_SUPPLY_TECHNOLOGY_MAX);

static const char* const vbat_power_supply_capacity_level_name[] =
		   VBAT_POWER_SUPPLY_CAPACITY_LEVEL_NAME;
VBAT_STATIC_ASSERT (VBAT_ARRAY_ELEMS (vbat_power_supply_capacity_level_name) ==
		    VBAT_POWER_SUPPLY_CAPACITY_LEVEL_MAX);

static const char* const vbat_power_supply_property_name[] =
		   VBAT_POWER_SUPPLY_PROPERTY_NAME;
VBAT_STATIC_ASSERT (VBAT_ARRAY_ELEMS (vbat_power_supply_property_name) ==
		    VBAT_POWER_SUPPLY_PROP_MAX);
#endif

    /* Not exported */
    /* Only called for String vproperties, from __vbat_get_property() */

    static vbat_vprop_t*
_vbat_vprop_lookup (const vbat_t* vbat,
		    const vbat_power_supply_property_t vproperty)
{
    vbat_vprop_t* vprop = vbat->vprops;

    while (vprop && (vprop->vproperty != vproperty)) {
	vprop = vprop->next;
    }
    return vprop;
}

    /* Not exported */
    /* Only called for String properties */

    static vbat_vprop_t*
_vbat_vprop_add (vbat_t*				vbat,
		 const vbat_power_supply_property_t	vproperty,
		 const void*				data,
		 const unsigned int			size)
{
    vbat_vprop_t* vprop = kmalloc(sizeof(vbat_vprop_t) + size, GFP_KERNEL);
    if (!vprop) {
	return 0;
    }
    memcpy(vprop->data, data, size);
    vprop->data[size-1] = 0;
    DTRACE ("adding vprop %d %s value '%s' size %d\n", vproperty,
	    vbat_power_supply_property_name [vproperty],
	    (const char*) data, size);
    vprop->vproperty = vproperty;
    vprop->next      = vbat->vprops;
    vbat->vprops     = vprop;
    return vprop;
}

    /* Not exported */
    /* Called only from _vbat_release() */

    static void
_vbat_vprop_clean (vbat_t* vbat)
{
    while (vbat->vprops) {
	vbat_vprop_t* vprop = vbat->vprops;
	vbat->vprops = vprop->next;
	kfree(vprop);
    }
}

    /* Not exported */

    static vrpc_size_t
_vbat_call (vbat_t* vbat, const nku32_f cmd, const nku32_f arg)
{
    struct vrpc_t* vrpc = vbat->vrpc;
    vbat_req_t*    req  = vbat->data;
    vrpc_size_t    size;

    if (mutex_lock_interruptible(&vbat->call_mutex)) {
	return 0;
    }
    DFTRACE ("cmd %s arg %x\n", vbat_cmd_name [cmd], arg);
    for (;;) {
        req->cmd = cmd;
        req->arg = arg;
	size     = sizeof(vbat_req_t);
	if (!vrpc_call(vrpc, &size)) {
	    break;
	}
	vrpc_close(vrpc);
	if (vrpc_client_open(vrpc, 0, 0)) {
	    BUG();
	}
    }
    mutex_unlock(&vbat->call_mutex);
    return size;
}

    /* Not exported */

    static int
_vbat_set_mode (vbat_t* vbat)
{
    vbat_res_t* res = vbat->data;
    vrpc_size_t size;

    TRACE("requesting %s mode\n", vbat->poll ? "polling" : "interrupt");
    size = _vbat_call(vbat, VBAT_CMD_SET_MODE, (nku32_f) vbat->poll);
    if (size != sizeof(vbat_res_t) ) {
	return -EFAULT;
    }
    DTRACE("requested mode %u -> res %u, value %u\n",
	   vbat->poll, res->res, res->value);
    if (res->res) {
	DTRACE ("%s mode not supported by the back-end\n",
	  vbat->poll ? "polling" : "interrupt");
	return res->res;
    }
    return 0;
}

    /* Not exported */
    /* Only called from _vbat_register() */

    static int
_vbat_get_name (vbat_t* vbat, char** pname)
{
    vbat_res_t* res = vbat->data;
    vrpc_size_t size;
    char*       name;

    size = _vbat_call(vbat, VBAT_CMD_GET_NAME, 0);
    if (size <= 4) {
	return -EFAULT;
    }
    if (size > vbat->msize) {
	return -EFAULT;
    }
    if (res->res) {
	return res->res;
    }
    size -= 4;

    name = kmalloc(size, GFP_KERNEL);
    if (!name) {
	return -ENOMEM;
    }
    memcpy(name, &res->value, size);
    name[size-1] = 0;

    *pname = name;

    return 0;
}

    /* Not exported */

    static int
_vbat_get_value (vbat_t* vbat, const nku32_f cmd, const nku32_f arg,
		 nku32_f* data)
{
    vbat_res_t* res = vbat->data;
    vrpc_size_t size;

    size = _vbat_call(vbat, cmd, arg);
    if (size != sizeof(vbat_res_t) ) {
	DFTRACE ("cmd %s returned bad size %d\n", vbat_cmd_name [cmd], size);
	return -EFAULT;
    }
    if (res->res) {
	DFTRACE ("cmd %s returned error %d\n", vbat_cmd_name [cmd], res->res);
	return res->res;
    }
    *data = res->value;
    return 0;
}

    /* Not exported */

    static int
_vbat_get_vtype (vbat_t* vbat, vbat_power_supply_type_t* vtype)
{
    return _vbat_get_value(vbat, VBAT_CMD_GET_VTYPE, 0, vtype);
}

    /* Not exported */

    static int
_vbat_get_vprop_max (vbat_t* vbat, int* vpropmax)
{
    return _vbat_get_value(vbat, VBAT_CMD_GET_VPROP_MAX, 0, vpropmax);
}

    /* Not exported */
    /* Called from _vbat_register() only */

    static int
_vbat_get_vprop_vid (vbat_t* vbat, const int idx, int* vid)
{
    return _vbat_get_value(vbat, VBAT_CMD_GET_VPROP_ID, idx, vid);
}

    /* Not exported */

    static int
_vbat_get_vprop_value (vbat_t* vbat,
		       const vbat_power_supply_property_t vproperty,
		       int* data)
{
    nku32_f vdata;
    int res = _vbat_get_value(vbat, VBAT_CMD_GET_VPROP_VAL, vproperty, &vdata);

    if (res) return res;
    switch (vproperty) {
    case VBAT_POWER_SUPPLY_PROP_STATUS:
	*data = vbat_power_supply_vstatus2status (vdata);
	if (*data < 0) {
	    ETRACE("Could not recognize vstatus %d\n", vdata);
	    *data = POWER_SUPPLY_STATUS_UNKNOWN;
	} else {
	    DTRACE ("%s: %d %s\n", vbat_power_supply_property_name [vproperty],
		    vdata, vbat_power_supply_status_name [vdata]);
	}
	break;

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,32)
    case VBAT_POWER_SUPPLY_PROP_CHARGE_TYPE:
	*data = vbat_power_supply_vcharge_type2charge_type (vdata);
	if (*data < 0) {
	    ETRACE("Could not recognize vcharge_type %d\n", vdata);
	    *data = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	} else {
	    DTRACE ("%s: %d %s\n", vbat_power_supply_property_name [vproperty],
		    vdata, vbat_power_supply_charge_type_name [vdata]);
	}
	break;
#endif

    case VBAT_POWER_SUPPLY_PROP_HEALTH:
	*data = vbat_power_supply_vhealth2health (vdata);
	if (*data < 0) {
	    ETRACE("Could not recognized vhealth %d\n", vdata);
	    *data = POWER_SUPPLY_HEALTH_UNKNOWN;
	} else {
	    DTRACE ("%s: %d %s\n", vbat_power_supply_property_name [vproperty],
		    vdata, vbat_power_supply_health_name [vdata]);
	}
	break;

    case VBAT_POWER_SUPPLY_PROP_TECHNOLOGY:
	*data = vbat_power_supply_vtechnology2technology (vdata);
	if (*data < 0) {
	    ETRACE("Could not recognize vtechnology %d\n", vdata);
	    *data = VBAT_POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
	} else {
	    DTRACE ("%s: %d %s\n", vbat_power_supply_property_name [vproperty],
		    vdata, vbat_power_supply_technology_name [vdata]);
	}
	break;

    case VBAT_POWER_SUPPLY_PROP_CAPACITY_LEVEL:
	*data = vbat_power_supply_vcapacity_level2capacity_level (vdata);
	if (*data < 0) {
	    ETRACE("Could not recognize vcapacity_level %d\n", vdata);
	    *data = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
	} else {
	    DTRACE ("%s: %d %s\n", vbat_power_supply_property_name [vproperty],
		    vdata, vbat_power_supply_capacity_level_name [vdata]);
	}
	break;

    default:
	DTRACE ("%s: %d\n", vbat_power_supply_property_name [vproperty], vdata);
	*data = vdata;
	break;
    }
    return res;
}

    /* Callback from Linux */

    static void
_vbat_work (struct work_struct* work)
{
    vbat_t* vbat = container_of((struct delayed_work*)work, vbat_t, work);

    DFTRACE("name %s\n", vbat->psy.name);
    power_supply_changed(&vbat->psy);
	/*
	 * When operating in polling mode, we have to periodically
	 * ask the BE for power supply changes.
	 */
    if (vbat->poll != VBAT_MODE_INTR) {
	schedule_delayed_work(&vbat->work, VBAT_POLLING_PERIOD);
    }
}

    /* interrupt signalling a power supply change */

    static void
_vbat_cxirq_handler (void* cookie, NkXIrq xirq)
{
    vbat_t* vbat = (vbat_t*) cookie;

    DTRACE("received 'power supply change' xirq from BE\n");
    schedule_delayed_work(&vbat->work, 0);
    (void) xirq;
}

    /* Not exported */

    static int
_psy_prop_is_string (const enum power_supply_property property)
{
    return (property == POWER_SUPPLY_PROP_MODEL_NAME) ||
	   (property == POWER_SUPPLY_PROP_MANUFACTURER)
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
	   || (property == POWER_SUPPLY_PROP_SERIAL_NUMBER)
#endif
	   ;
}

    /* Not exported */

    static int
__vbat_get_property (vbat_t*				vbat,
		     const enum power_supply_property	property,
		     union power_supply_propval*	val)
{
    vbat_res_t*  res = vbat->data;
    const vbat_power_supply_property_t vproperty =
	vbat_property2vproperty (property);
    vbat_vprop_t* vprop;

    if ((int) vproperty < 0) {
	ETRACE ("Linux requests unsupported property %d\n", property);
	return -ESRCH;
    }
    if (!_psy_prop_is_string (property)) {
	return _vbat_get_vprop_value (vbat, vproperty, &val->intval);
    }
    vprop = _vbat_vprop_lookup (vbat, vproperty);
    if (!vprop) {
	const vrpc_size_t size = _vbat_call(vbat, VBAT_CMD_GET_VPROP_VAL,
					    vproperty);
        if (size <= 4) {
	    return -EFAULT;
        }
	if (size > vbat->msize) {
	    return -EFAULT;
        }
        if (res->res) {
	    return res->res;
        }
        vprop = _vbat_vprop_add(vbat, vproperty, &res->value, size - 4);
        if (!vprop) {
	    return -ENOMEM;
        }
    }
    val->strval = vprop->data;
    DFTRACE ("%s: '%s'\n", vbat_power_supply_property_name [vproperty],
	     val->strval);

    return 0;
}

    /* Callback from Linux */

    static int
_vbat_get_property (struct power_supply*		psy,
		    const enum power_supply_property	property,
		    union power_supply_propval*		val)
{
    vbat_t* vbat = (vbat_t*)psy;
    int     res;
    mutex_lock(&vbat->prop_mutex);
    res = __vbat_get_property(vbat, property, val);
    mutex_unlock(&vbat->prop_mutex);

    return res;
}

#ifndef VBATTERY_PLATFORM_DEVICE
static vbat_t* vbat_device;
#endif

    /* Not exported */

    static int
_vbat_register (vbat_t* vbat)
{
#ifdef VBATTERY_PLATFORM_DEVICE
    struct platform_device* pdev = vbat->pdev;
#endif
    struct power_supply*    psy  = &vbat->psy;
    vbat_power_supply_type_t vtype;
    int                     res;
    unsigned int            i;
    unsigned		    num_vproperties;

    psy->get_property = _vbat_get_property;

    res = _vbat_get_name(vbat, (char**)&psy->name);
    if (res) {
	ETRACE("unable to get the power supply name: %d\n", res);
	return res;
    }
    DTRACE(">>> name %s\n", psy->name);

    res = _vbat_get_vtype(vbat, &vtype);
    if (!res) {
	psy->type = vbat_vtype2type (vtype);
	if ((int) psy->type < 0) {
	    ETRACE ("cannot map power supply vtype %d\n", vtype);
	    return -ESRCH;
	}
    }
    if (res) {
	ETRACE("unable to get the power supply type: %d\n", res);
	return res;
    }
    DFTRACE("type %d (vtype %d %s)\n", psy->type, vtype,
	    vbat_power_supply_type_name [vtype]);

    res = _vbat_get_vprop_max(vbat, &num_vproperties);
    if (res) {
	ETRACE("unable to get the power supply properties limit: %d\n", res);
	return res;
    }
    DTRACE(">>> num_vproperties %d\n", num_vproperties);

    psy->properties = kmalloc(
		num_vproperties * sizeof(enum power_supply_property),
		GFP_KERNEL);
    if (!psy->properties) {
	return -ENOMEM;
    }
    psy->num_properties = 0;
    for (i = 0; i < num_vproperties; i++) {
	int vproperty;
	enum power_supply_property property;

	res = _vbat_get_vprop_vid (vbat, i, &vproperty);
	if (res) {
	    ETRACE("unable to get the vproperty ID %d: %d\n", i, res);
	    continue;
	}
	property = vbat_vproperty2property (vproperty);
	if ((int) property < 0) {
	    ETRACE ("vproperty %d cannot be used on this linux\n", vproperty);
	    DFTRACE ("vproperty %d %s cannot be used on this linux\n",
		     vproperty, vbat_power_supply_property_name [vproperty]);
	    continue;
	}
	DTRACE (">>> property[%d] -> %d %s -> %d\n", i, vproperty,
		vbat_power_supply_property_name [vproperty], property);
	psy->properties [psy->num_properties++] = property;
    }
    mutex_init(&vbat->prop_mutex);

    INIT_DELAYED_WORK(&vbat->work, _vbat_work);

#ifdef VBATTERY_PLATFORM_DEVICE
    res = power_supply_register(&pdev->dev, psy);
#else
    res = power_supply_register(NULL, psy);
#endif
    if (res) {
	ETRACE("unable to register a virtual power supply: %d\n", res);
	return res;
    }

#ifdef VBATTERY_PLATFORM_DEVICE
    platform_set_drvdata(pdev, vbat);
#else
    vbat_device = vbat;
#endif

    if (_vbat_set_mode(vbat)) {
	ETRACE("couldn't set mode %u; falling back to polling mode\n",
	       vbat->poll);
	vbat->poll = VBAT_MODE_POLL;
    }
    TRACE("operating in %s mode\n", vbat->poll ? "polling" : "interrupt");
	/*
	 * When operating in polling mode, we have to periodically
	 * ask the BE for power supply changes.
	 */
    if (vbat->poll != VBAT_MODE_INTR) {
	schedule_delayed_work(&vbat->work, VBAT_POLLING_PERIOD);
    }
    TRACE("Virtual Power Supply Device: %s  Type: %d  Properties: %d\n",
	  psy->name, psy->type, psy->num_properties);
    return 0;
}

    /* Not exported */

    static void
_vbat_release (vbat_t* vbat)
{
    struct power_supply* psy  = &vbat->psy;

    vrpc_close(vbat->vrpc);
    vrpc_release(vbat->vrpc);
    _vbat_vprop_clean(vbat);
    kfree(psy->name);
    kfree(psy->properties);
    kfree(vbat);
}

    /* Not exported */

    static void
_vbat_ready (void* cookie)
{
    vbat_t* vbat = cookie;
    if (_vbat_register(vbat)) {
	ETRACE("unable to register a virtual battery\n");
	_vbat_release(vbat);
    }
}

    /* Not exported */

    static void
_vbat_info (vbat_t* vbat)
{
    const char* info = vrpc_info(vbat->vrpc);
    if (info) {
	if (strstr(info, "poll")) {
	    vbat->poll = VBAT_MODE_POLL;
	} else if (strstr(info, "intr")) {
	    vbat->poll = VBAT_MODE_INTR;
	}
    }

    DTRACE("info %s -> poll %u\n", info, vbat->poll);
}

    /* Not exported */

    static int
#ifdef VBATTERY_PLATFORM_DEVICE
_vbat_create (struct platform_device* pdev, struct vrpc_t* vrpc)
#else
_vbat_create (struct vrpc_t* vrpc)
#endif
{
    vbat_t* vbat = kzalloc(sizeof(vbat_t), GFP_KERNEL);
    int     res;

    if (!vbat) {
	return -ENOMEM;
    }
    mutex_init(&vbat->call_mutex);

#ifdef VBATTERY_PLATFORM_DEVICE
    vbat->pdev  = pdev;
#endif
    vbat->vrpc  = vrpc;
    vbat->msize = vrpc_maxsize(vrpc);
    vbat->data  = vrpc_data(vrpc);
    vbat->poll  = VBAT_MODE_POLL;

    if (vbat->msize < sizeof(vbat_req_t)) {
	return -EINVAL;
    }
    if (vbat->msize < sizeof(vbat_res_t)) {
	return -EINVAL;
    }
    res = vrpc_client_open(vrpc, _vbat_ready, vbat);
    if (res) {
	return res;
    }
    _vbat_info(vbat);

    if (vbat->poll == VBAT_MODE_INTR) {
	const NkPhAddr    plink = vrpc_plink(vbat->vrpc);
	const NkDevVlink* vlink = vrpc_vlink(vbat->vrpc);
	const NkOsId      c_id  = vlink->c_id;

	vbat->cxirq = nkops.nk_pxirq_alloc(plink, VRPC_PXIRQ_BASE, c_id, 1);
	if (vbat->cxirq == 0) {
	    ETRACE("could not allocate cross interrupt; "
		   "falling back to polling mode\n");
	    vbat->poll = VBAT_MODE_POLL;
	}
	vbat->cxid = nkops.nk_xirq_attach(vbat->cxirq,
	  _vbat_cxirq_handler, vbat);
	if (!vbat->cxid) {
	    ETRACE("could not attach cross interrupt %d; "
		   "falling back to polling mode\n", vbat->cxirq);
	    vbat->poll = VBAT_MODE_POLL;
	}
    }
    return 0;
}

    /* Callback from Linux */

    static int
#ifdef VBATTERY_PLATFORM_DEVICE
_vbat_probe (struct platform_device* pdev)
#else
_vbat_probe (void)
#endif
{
    struct vrpc_t* vrpc = vrpc_client_lookup(VBAT_VRPC_NAME, 0);

    DFTRACE("vrpc %p\n", vrpc);
    if (vrpc) {
#ifdef VBATTERY_PLATFORM_DEVICE
	int res = _vbat_create(pdev, vrpc);
#else
	int res = _vbat_create(vrpc);
#endif
	if (res) {
	    ETRACE("unable to create a virtual battery: %d\n", res);
	    vrpc_release(vrpc);
	}
    }
    return 0;
}

    /* Callback from Linux */

    static int
#ifdef VBATTERY_PLATFORM_DEVICE
_vbat_remove (struct platform_device* pdev)
#else
_vbat_remove (void)
#endif
{
#ifdef VBATTERY_PLATFORM_DEVICE
    vbat_t* vbat = platform_get_drvdata(pdev);
#else
    vbat_t* vbat = vbat_device;
#endif

    DFTRACE("vbat %p\n", vbat);
    if (vbat) {
        struct power_supply* psy = &vbat->psy;
	cancel_delayed_work_sync(&vbat->work);
        power_supply_unregister(psy);
	_vbat_release(vbat);
#ifdef VBATTERY_PLATFORM_DEVICE
	platform_set_drvdata(pdev, 0);
#else
	vbat_device = NULL;
#endif
    }
    return 0;
}

    /* Callbacks from Linux */

#ifdef VBATTERY_PLATFORM_DEVICE
static struct platform_driver _vbat = {
    .probe	= _vbat_probe,
    .remove	= _vbat_remove,
    .driver = {
	.name = "vbattery-fe"
    }
};
#endif

    static int __init
_vbat_init (void)
{
    DFTRACE ("called\n");
#ifdef VBATTERY_PLATFORM_DEVICE
    return platform_driver_register(&_vbat);
#else
    return _vbat_probe();
#endif
}

    static void __exit
_vbat_exit (void)
{
    DFTRACE ("called\n");
#ifdef VBATTERY_PLATFORM_DEVICE
    platform_driver_unregister(&_vbat);
#else
    _vbat_remove();
#endif
}

MODULE_DESCRIPTION("VLX virtual battery frontend driver");
MODULE_AUTHOR("Vladimir Grouzdev <vladimir.grouzdev@redbend.com>");
MODULE_LICENSE("GPL");

module_init(_vbat_init);
module_exit(_vbat_exit);

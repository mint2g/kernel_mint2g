/*
 ****************************************************************
 *
 *  Component: VLX virtual battery backend driver
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

#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#define VBATTERY_BE

#include <nk/nkern.h>
#include <vlx/vbattery_common.h>
#include "vrpc.h"

#define TRACE(format, args...)  printk("VBAT-BE: " format, ## args)
#define ETRACE(format, args...) printk("VBAT-BE: [E] " format, ## args)
#if 0
#define DTRACE(format, args...) printk("VBAT-BE: [D] " format, ## args)
#else
#define DTRACE(format, args...)
#endif

    extern int
vbattery_be_register_client (struct notifier_block* nblk);

    extern int
vbattery_be_unregister_client (struct notifier_block* nblk);

typedef struct vbat_t {
    struct power_supply* psy;	// power supply
    struct vrpc_t*	 vrpc;	// RPC link
    void*                data;	// RPC data
    vrpc_size_t          msize;	// maximum data size
    vbat_mode_t          poll;  // polling/interrupt mode
    NkXIrq               cxirq; // server->client cross-irq
    struct vbat_t*       next;	// next battery
} vbat_t;

static vbat_t*               _vbats;
static struct notifier_block _vbat_nblk;

    static vbat_t*
_vbat_lookup (struct power_supply* psy, struct vrpc_t* vrpc)
{
    vbat_t* vbat = _vbats;

    while (vbat) {
	if ((vbat->psy == psy) &&
	    (vrpc_peer_id(vbat->vrpc) == vrpc_peer_id(vrpc))) {
	    break;
	}
	vbat = vbat->next;
    }
    return vbat;
}

    static int
_psy_vprop_is_string (vbat_power_supply_property_t vproperty)
{
    return (vproperty == VBAT_POWER_SUPPLY_PROP_MODEL_NAME) ||
	   (vproperty == VBAT_POWER_SUPPLY_PROP_MANUFACTURER) ||
	   (vproperty == VBAT_POWER_SUPPLY_PROP_SERIAL_NUMBER);
}

    static vrpc_size_t
_vbat_get_string (vbat_t* vbat, const char* s, vbat_res_t* res)
{
    unsigned int len = strlen(s);

    if (len > (vbat->msize - 5)) {
	len = vbat->msize - 5;
    }
    memcpy(&res->value, s, len);
    ((char*) &res->value)[len] = 0;

    return (5 + len);
}

    static vrpc_size_t
_vbat_set_mode (vbat_t* vbat, nku32_f arg, vbat_res_t* res)
{
    nku32_f err = 0;

    DTRACE("current mode %u, set mode %u\n", vbat->poll, arg);

    /*
     * current mode + set mode -> new mode
     * INTR           INTR        INTR
     * INTR           POLL        POLL
     * INTR           POLL_ONLY   POLL
     * POLL           INTR        INTR
     * POLL           POLL        POLL
     * POLL           POLL_ONLY   POLL
     * POLL_ONLY      INTR        POLL_ONLY + error
     * POLL_ONLY      POLL        POLL_ONLY
     * POLL_ONLY      POLL_ONLY   POLL_ONLY
     */
    switch (arg) {
    case VBAT_MODE_POLL:
    case VBAT_MODE_POLL_ONLY:
	if (vbat->poll != VBAT_MODE_POLL_ONLY) {
	    vbat->poll = VBAT_MODE_POLL;
	}
	break;

    case VBAT_MODE_INTR:
	if (vbat->poll != VBAT_MODE_POLL_ONLY) {
	    vbat->poll = VBAT_MODE_INTR;
	} else {
	    err = (nku32_f) -EIO;
	}
	break;

    default:
	err = (nku32_f) -EINVAL;
	break;
    }

    TRACE("operating in %s mode\n", vbat->poll ? "polling" : "interrupt");

    /* Return current operation mode. */
    res->res   = err;
    res->value = (nku32_f) vbat->poll;
    return sizeof(vbat_res_t);
}

    static vrpc_size_t
_vbat_get_name (vbat_t* vbat, vbat_res_t* res)
{
    res->res = 0;
    return _vbat_get_string(vbat, vbat->psy->name, res);
}

    static vrpc_size_t
_vbat_get_vtype (vbat_t* vbat, vbat_res_t* res)
{
    const vbat_power_supply_type_t vtype = vbat_type2vtype (vbat->psy->type);

    if ((int) vtype < 0) {
	ETRACE("cannot map type %d to vtype\n", vbat->psy->type);
	res->res = (nku32_f) -ESRCH;
	return sizeof(vbat_res_t);
    }
    res->res   = 0;
    res->value = vtype;
    return sizeof(vbat_res_t);
}

    static vrpc_size_t
_vbat_get_vprop_max (vbat_t* vbat, vbat_res_t* res)
{
    res->res   = 0;
    res->value = vbat->psy->num_properties;
    return sizeof(vbat_res_t);
}

    static vrpc_size_t
_vbat_get_vprop_vid (vbat_t* vbat, nku32_f arg, vbat_res_t* res)
{
    struct power_supply* psy = vbat->psy;
    vbat_power_supply_property_t vproperty;

    if (arg >= psy->num_properties) {
	res->res = (nku32_f) -EINVAL;
	return sizeof(vbat_res_t);
    }
    vproperty = vbat_property2vproperty (psy->properties [arg]);
    if ((int) vproperty < 0) {
	ETRACE("cannot map property %d to vproperty\n", psy->properties [arg]);
	res->res = (nku32_f) -ESRCH;
	return sizeof(vbat_res_t);
    }
    res->res   = 0;
    res->value = vproperty;
    return sizeof(vbat_res_t);
}

    static vrpc_size_t
_vbat_get_vprop_value (vbat_t* vbat,
		       const vbat_power_supply_property_t vproperty,
		       vbat_res_t* res)
{
    const enum power_supply_property property =
	vbat_vproperty2property (vproperty);
    union power_supply_propval val;
    struct power_supply*       psy = vbat->psy;

    if ((int) property < 0) {
	res->res = (nku32_f) -ESRCH;
	return sizeof(vbat_res_t);
    }
    if ((res->res = psy->get_property (psy, property, &val))) {
	return sizeof(vbat_res_t);
    }
    if (_psy_vprop_is_string (vproperty)) {
	return _vbat_get_string(vbat, val.strval, res);
    }
    switch (vproperty) {
    case VBAT_POWER_SUPPLY_PROP_STATUS:
	res->value = vbat_power_supply_status2vstatus (val.intval);
	if ((int) res->value < 0) {
	    ETRACE("Could not virtualize status %d\n", val.intval);
	    res->value = VBAT_POWER_SUPPLY_STATUS_UNKNOWN;
	}
	break;

    case VBAT_POWER_SUPPLY_PROP_CHARGE_TYPE:
	res->value = vbat_power_supply_charge_type2vcharge_type (val.intval);
	if ((int) res->value < 0) {
	    ETRACE("Could not virtualize charge type %d\n", val.intval);
	    res->value = VBAT_POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}
	break;

    case VBAT_POWER_SUPPLY_PROP_HEALTH:
	res->value = vbat_power_supply_health2vhealth (val.intval);
	if ((int) res->value < 0) {
	    ETRACE("Could not virtualize health %d\n", val.intval);
	    res->value = VBAT_POWER_SUPPLY_HEALTH_UNKNOWN;
	}
	break;

    case VBAT_POWER_SUPPLY_PROP_TECHNOLOGY:
	res->value = vbat_power_supply_technology2vtechnology (val.intval);
	if ((int) res->value < 0) {
	    ETRACE("Could not virtualize technology %d\n", val.intval);
	    res->value = VBAT_POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
	}
	break;

    case VBAT_POWER_SUPPLY_PROP_CAPACITY_LEVEL:
	res->value = vbat_power_supply_capacity_level2vcapacity_level
	    (val.intval);
	if ((int) res->value < 0) {
	    ETRACE("Could not virtualize capacity level %d\n", val.intval);
	    res->value = VBAT_POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
	}
	break;

    default:
	res->value = val.intval;
	break;
    }
    return sizeof(vbat_res_t);
}

    static vrpc_size_t
_vbat_call (void* cookie, vrpc_size_t size)
{
    vbat_t*     vbat = (vbat_t*) cookie;
    vbat_req_t* req  = (vbat_req_t*) vbat->data;
    vbat_res_t* res  = (vbat_res_t*) vbat->data;

    if ((vbat->msize < sizeof(vbat_res_t)) || (size != sizeof(vbat_req_t))) {
	return 0;
    }

    DTRACE("received cmd %u, arg %u\n", req->cmd, req->arg);

    switch (req->cmd) {
    case VBAT_CMD_SET_MODE:
	return _vbat_set_mode (vbat, req->arg, res);

    case VBAT_CMD_GET_NAME:
	return _vbat_get_name (vbat, res);

    case VBAT_CMD_GET_VTYPE:
	return _vbat_get_vtype (vbat, res);

    case VBAT_CMD_GET_VPROP_MAX:
	return _vbat_get_vprop_max (vbat, res);

    case VBAT_CMD_GET_VPROP_ID:
	return _vbat_get_vprop_vid (vbat, req->arg, res);

    case VBAT_CMD_GET_VPROP_VAL:
	return _vbat_get_vprop_value (vbat, (vbat_power_supply_property_t)
				      req->arg, res);
    default:
	break;
    }
    return 0;
}

    static void
_vbat_info (vbat_t* vbat)
{
    const char* info = vrpc_info(vbat->vrpc);

    if (info) {
	if (strstr(info, "poll_only")) {
	    vbat->poll = VBAT_MODE_POLL_ONLY;
	} else if (strstr(info, "poll")) {
	    vbat->poll = VBAT_MODE_POLL;
	} else if (strstr(info, "intr")) {
	    vbat->poll = VBAT_MODE_INTR;
	}
    }
    DTRACE("info %s -> poll %u\n", info, vbat->poll);
}

    static int
_vbat_create (struct power_supply* psy, struct vrpc_t* vrpc)
{
    int     res;
    vbat_t* vbat;

    vbat = (vbat_t*) kzalloc(sizeof(vbat_t), GFP_KERNEL);
    if (!vbat) {
	ETRACE("memory allocation failed\n");
	return 0;
    }

    vbat->psy   = psy;
    vbat->vrpc  = vrpc;
    vbat->data  = vrpc_data(vrpc);
    vbat->msize = vrpc_maxsize(vrpc);

    /*
     * If the native battery monitor can not inform us of
     * a power supply state change via _vbat_changed(),
     * then vbat->poll should be set to VBAT_MODE_POLL_ONLY
     * to ensure that interrupt mode can not be set by the FE.
     */
    vbat->poll  = VBAT_MODE_POLL;

    if ((vbat->msize < sizeof(vbat_req_t)) ||
        (vbat->msize < sizeof(vbat_res_t))) {
	ETRACE("not enough VRPC shared memory -> %d\n", vbat->msize);
	kfree(vbat);
	return 0;
    }

    if ((res = vrpc_server_open(vrpc, _vbat_call, vbat, 0)) != 0) {
	ETRACE("VRPC open failed -> %d\n", res);
	kfree(vbat);
	return 0;
    }

    _vbat_info(vbat);

    if (vbat->poll != VBAT_MODE_POLL_ONLY) {

	NkPhAddr    plink = vrpc_plink(vbat->vrpc);
	NkDevVlink* vlink = vrpc_vlink(vbat->vrpc);
	NkOsId      c_id  = vlink->c_id;

	vbat->cxirq = nkops.nk_pxirq_alloc(plink, VRPC_PXIRQ_BASE, c_id, 1);
	if (vbat->cxirq == 0) {
	    ETRACE("could not allocate cross interrupt; "
	      "falling back to polling mode\n");
	    vbat->poll = VBAT_MODE_POLL_ONLY;
	}
    }

    DTRACE("VBAT %s -> %d created\n", psy->name, vrpc_peer_id(vrpc));

    vbat->next = _vbats;
    _vbats     = vbat;

    return 1;
}

    static void
_vbat_destroy (vbat_t* vbat)
{
    vbat_t** link = &_vbats;

    DTRACE("VBAT %s -> %d destroyed\n",
	   vbat->psy->name, vrpc_peer_id(vbat->vrpc));

    vrpc_close(vbat->vrpc);
    vrpc_release(vbat->vrpc);

    while (*link != vbat) link = &(*link)->next;
    *link = vbat->next;

    kfree(vbat);
}

    static void
_vbat_setup (struct power_supply* psy)
{
    struct vrpc_t* vrpc = 0;

    while ((vrpc = vrpc_server_lookup(VBAT_VRPC_NAME, vrpc)) != 0) {
	if (_vbat_lookup(psy, vrpc)) {
	    vrpc_release(vrpc);
	} else {
	    if (!_vbat_create(psy, vrpc)) {
		vrpc_release(vrpc);
	    }
	}
    }
}

    static void
_vbat_changed (struct power_supply* psy)
{
   vbat_t* vbat = _vbats;

    while (vbat) {
	if (vbat->psy == psy) {

	    DTRACE("psy '%s' changed, %ssending interrupt to FE\n",
	      psy->name, vbat->poll ? "not " : "");
	    /*
	     * If the FE does not operate in polling mode, we have to post
	     * an interrupt to explicitly inform it that the power supply
	     * state has changed.
	     */
	    if (!vbat->poll) {
		NkDevVlink* vlink = vrpc_vlink(vbat->vrpc);
		nkops.nk_xirq_trigger(vbat->cxirq, vlink->c_id);
	    }
	}
	vbat = vbat->next;
    }
}

    static int
_vbat_notify (struct notifier_block* nblk,
	      unsigned long          event,
	      void*                  data)
{
    struct power_supply* psy = data;

    if (psy->type == POWER_SUPPLY_TYPE_BATTERY) {
	if (event == 0) {
	    _vbat_setup(psy);
	} else {
	    _vbat_changed(psy);
	}
    }

    return NOTIFY_DONE;
}

    static int __init
_vbat_init (void)
{
    int res;
    _vbat_nblk.notifier_call = _vbat_notify;
    if ((res = vbattery_be_register_client(&_vbat_nblk))) {
	ETRACE("client registration failed (%d)", res);
	return res;
    }
    TRACE("module loaded\n");
    return 0;
}

    static void __exit
_vbat_exit (void)
{
    vbattery_be_unregister_client(&_vbat_nblk);
    while (_vbats) {
	_vbat_destroy(_vbats);
    }
    TRACE("module unloaded\n");
}

MODULE_DESCRIPTION("VLX Virtual battery backend driver");
MODULE_AUTHOR("Vladimir Grouzdev <vladimir.grouzdev@redbend.com>");
MODULE_LICENSE("GPL");

module_init(_vbat_init);
module_exit(_vbat_exit);

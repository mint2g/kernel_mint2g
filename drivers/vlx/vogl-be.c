/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual OpenGL ES (VOGL).                                 *
 *             VOGL backend kernel driver implementation.                    *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  This program is free software: you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License Version 2           *
 *  as published by the Free Software Foundation.                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  Version 2 along with this program.                                       *
 *  If not, see <http://www.gnu.org/licenses/>.                              *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#define DEBUG

#include <linux/module.h>
#include "vogl.h"

MODULE_DESCRIPTION("VOGL Back-End Driver");
MODULE_AUTHOR("Sebastien Laborie <sebastien.laborie@redbend.com>");
MODULE_LICENSE("GPL");

    /*
     *
     */
static VrpqDrv vrpq_srv_drv = {
    .name         = "vrpq-srv-vogl",
    .major        = VOGL_SRV_MAJOR,
    .resc_id_base = 0,
    .prop_get     = vogl_vrpq_prop_get,
};

    /*
     *
     */
static VumemDrv vumem_srv_drv = {
    .name         = "vumem-srv-vogl",
    .major        = VOGL_SRV_MAJOR + 1,
    .resc_id_base = 16,
    .prop_get     = vogl_vumem_prop_get,
};

    /*
     *
     */
    static int
vogl_srv_vlink_init (Vlink* vlink)
{
    int diag;

    VLINK_DTRACE(vlink, "vogl_srv_vlink_init called\n");

    diag = vrpq_srv_vlink_init(&vrpq_srv_drv, vlink);
    if (diag) {
	return diag;
    }

    diag = vumem_srv_vlink_init(&vumem_srv_drv, vlink);
    if (diag) {
	vlink_op_perform(vlink, VLINK_OP_CLEANUP);
	return diag;
    }

    return 0;
}

    /*
     *
     */
    static int
vogl_srv_drv_init (VlinkDrv* vogl_srv_drv)
{
    int diag;

    diag = vrpq_srv_drv_init(vogl_srv_drv, &vrpq_srv_drv);
    if (diag) {
	return diag;
    }

    diag = vumem_srv_drv_init(vogl_srv_drv, &vumem_srv_drv);
    if (diag) {
	vrpq_srv_drv_cleanup(&vrpq_srv_drv);
	return diag;
    }

    return 0;

}

    /*
     *
     */
    static void
vogl_srv_drv_cleanup (VlinkDrv* vogl_srv_drv)
{
    vrpq_srv_drv_cleanup(&vrpq_srv_drv);

    vumem_srv_drv_cleanup(&vumem_srv_drv);
}

    /*
     *
     */
static VlinkDrv vogl_srv_drv = {
    .name       = "vogl",
    .init       = vogl_srv_drv_init,
    .cleanup    = vogl_srv_drv_cleanup,
    .vlink_init = vogl_srv_vlink_init,
    .flags      = VLINK_DRV_TYPE_SERVER,
};

    /*
     *
     */
    static int
vogl_be_module_init (void)
{
    int diag;

    if ((diag = vlink_drv_probe(&vogl_srv_drv)) != 0) {
	return diag;
    }
    vlink_drv_startup(&vogl_srv_drv);

    return 0;
}

    /*
     *
     */
    static void
vogl_be_module_exit (void)
{
    vlink_drv_shutdown(&vogl_srv_drv);
    vlink_drv_cleanup(&vogl_srv_drv);
}

module_init(vogl_be_module_init);
module_exit(vogl_be_module_exit);

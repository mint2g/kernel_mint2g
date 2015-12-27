/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual OpenGL ES (VOGL).                                 *
 *             VOGL frontend kernel driver implementation.                   *
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

MODULE_DESCRIPTION("VOGL Front-End Driver");
MODULE_AUTHOR("Sebastien Laborie <sebastien.laborie@redbend.com>");
MODULE_LICENSE("GPL");

    /*
     *
     */
static VrpqDrv vrpq_clt_drv = {
    .name         = "vrpq-clt-vogl",
    .major        = VOGL_CLT_MAJOR,
    .resc_id_base = 0,
    .prop_get     = vogl_vrpq_prop_get,
};

    /*
     *
     */
static VumemDrv vumem_clt_drv = {
    .name         = "vumem-clt-vogl",
    .major        = VOGL_CLT_MAJOR + 1,
    .resc_id_base = 16,
    .prop_get     = vogl_vumem_prop_get,
};

    /*
     *
     */
    static int
vogl_clt_vlink_init (Vlink* vlink)
{
    int diag;

    VLINK_DTRACE(vlink, "vogl_clt_vlink_init called\n");

    diag = vrpq_clt_vlink_init(&vrpq_clt_drv, vlink);
    if (diag) {
	return diag;
    }

    diag = vumem_clt_vlink_init(&vumem_clt_drv, vlink);
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
vogl_clt_drv_init (VlinkDrv* vogl_clt_drv)
{
    int diag;

    diag = vrpq_clt_drv_init(vogl_clt_drv, &vrpq_clt_drv);
    if (diag) {
	return diag;
    }

    diag = vumem_clt_drv_init(vogl_clt_drv, &vumem_clt_drv);
    if (diag) {
	vrpq_clt_drv_cleanup(&vrpq_clt_drv);
	return diag;
    }

    return 0;
}

    /*
     *
     */
    static void
vogl_clt_drv_cleanup (VlinkDrv* vogl_clt_drv)
{
    vrpq_clt_drv_cleanup(&vrpq_clt_drv);

    vumem_clt_drv_cleanup(&vumem_clt_drv);
}

    /*
     *
     */
static VlinkDrv vogl_clt_drv = {
    .name       = "vogl",
    .init       = vogl_clt_drv_init,
    .cleanup    = vogl_clt_drv_cleanup,
    .vlink_init = vogl_clt_vlink_init,
    .flags      = VLINK_DRV_TYPE_CLIENT,
};

    /*
     *
     */
    static int
vogl_fe_module_init (void)
{
    int diag;

    if ((diag = vlink_drv_probe(&vogl_clt_drv)) != 0) {
	return diag;
    }
    vlink_drv_startup(&vogl_clt_drv);

    return 0;
}

    /*
     *
     */
    static void
vogl_fe_module_exit (void)
{
    vlink_drv_shutdown(&vogl_clt_drv);
    vlink_drv_cleanup(&vogl_clt_drv);
}

module_init(vogl_fe_module_init);
module_exit(vogl_fe_module_exit);

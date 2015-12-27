/*
 ****************************************************************
 *
 *  Component: VLX VLCD backend driver, virtual OpenGL extension
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
 *    Thomas Charleux (thomas.charleux@redbend.com)
 *
 ****************************************************************
 */

#ifndef VLCD_VOGL_H
#define VLCD_VOGL_H

#include <vlx/vlcd_backend.h>

extern vlcd_pconf_t voglPConf[VLCD_BMAX_HW_DEV_SUP][VLCD_MAX_CONF_NUMBER];

extern void voglUpdateFB  (vlcd_frontend_device_t* fDev);
extern void voglCleanupFB (vlcd_frontend_device_t* fDev);
extern int  voglInitFB    (vlcd_frontend_device_t* fDev);
extern int  voglEnabled   (void);

#endif

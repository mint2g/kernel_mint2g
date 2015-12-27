/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Display (VDISP).                                  *
 *             Definitions for client VDISP entities.                        *
 *                                                                           *
 *  This file provides definitions that are shared by the server library     *
 *  and the kernel back-end driver.                                          *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *    Chi Dat Truong    <chidat.truong@redbend.com>                          *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VDISP_CLIENT_DRV_H
#define _VLX_VDISP_CLIENT_DRV_H

#include <vlx/vdisplay/common.h>

#define VDISP_CLT_IOCTL(c, s)			\
    VDISP_IOCTL((c) + 0, s)

/* User IOCTL <=> FE DRV */
#define VDISP_IOCTL_SCREEN_SET_BRIGHTNESS	VDISP_CLT_IOCTL(0, sizeof(BacklightBrightness))
#define VDISP_IOCTL_SCREEN_GET_BRIGHTNESS	VDISP_CLT_IOCTL(1, sizeof(BacklightBrightness))
#define VDISP_IOCTL_SCREEN_GET_PROPERTY		VDISP_CLT_IOCTL(2, sizeof(DisplayProperty))
#define VDISP_IOCTL_SCREEN_GET_POWER		VDISP_CLT_IOCTL(3, sizeof(PanelPowerState))

#endif /* _VLX_VDISP_CLIENT_DRV_H */

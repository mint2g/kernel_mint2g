/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Display (VDISP).                                  *
 *             Common definitions for client and server VDISP entities.      *
 *                                                                           *
 *  This file provides definitions that are shared by client and server      *
 *  VDISP libraries as well as by kernel front-end and back-end drivers.     *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *    Chi Dat Truong    <chidat.truong@redbend.com>                          *
 *                                                                           *
 *****************************************************************************/

#ifndef _VLX_VDISP_COMMON_H
#define _VLX_VDISP_COMMON_H

#include <vlx/vdisplay/types.h>

#ifndef ESESSIONBROKEN
#define ESESSIONBROKEN				EPIPE
#endif

/*
 * Size type.
 */
typedef nku32_f VdispSize;

#ifndef _VLX_DEF_CLIENTID
#define _VLX_DEF_CLIENTID

/*
 * Global identifier of a virtual machine (guest OS).
 */
typedef nku32_f VmId;

/*
 * Identifier of an application within a virtual machine (guest OS).
 */
typedef nku32_f DeviceId;

/*
 * Global identifier of a client application of a virtual service.
 */
typedef struct ClientId {
    VmId     vmId;
    DeviceId devId; /* Id for supporting multiple frontend in a VM */
} ClientId;

#endif /* _VLX_DEF_CLIENTID */

/*
 * This identifier enables to name the client of a VDISPLAY session in a global
 * and unique way across the platform.
 */
typedef ClientId VdispClientId;

/*
 * Display Panel info
 */

typedef struct DisplayProperty {
     nku16_f    xres;    /* X resolution in pixel */
     nku16_f    yres;    /* Y resolution in pixel */
     nku32_f    width;   /* in mm * 100 to keep float precision */
     nku32_f    height;  /* in mm * 100 to keep float presision */
     nku32_f    density; /* in dpi * 100 to keep float precision */
     /*   density in dpi (usually 160) could be in float */
     /*   width (in mm) = ((xres * 25.4f) / density + 0.5f) */
     /*   height (in mm) = ((info.yres * 25.4f)/ density + 0.5f) */
     /*   xdpi = (((xres * 25.4f) / width) * 1000 ) / 100 */
     /*   ydpi = (((yres * 25.4f) / height) * 1000) / 100 */
     /*   line length (in bytes) = (((xres * bpp) + 31) & ~31) / 8 */
     nku32_f    line_length;
     nku16_f    fps;  /* in Hz * 100 to keep precision. fps = refreshRate / 1000.0f */
     nku16_f    orientation;  /* rotation angle of the panel in clockwise */
     nku32_f    reserved0[5];  /* for future compatibility */

     /* Color format */
     nku8_f    bpp;          /* Bits per pixel */
     nku8_f    reserved1[3];
     nku8_f    r_offset;     /* (red) beginning of bitfield */
     nku8_f    r_length;     /* (red) length of bitfield    */
     nku8_f    r_msb_right;  /* (red) != 0 : Most significant bit is */ 
     nku8_f    g_offset;     /* (green) beginning of bitfield */
     nku8_f    g_length;     /* (green) length of bitfield */
     nku8_f    g_msb_right;  /* (green) != 0 : Most significant bit is */ 
     nku8_f    b_offset;     /* (blue) beginning of bitfield */
     nku8_f    b_length;     /* (blue) length of bitfield */
     nku8_f    b_msb_right;  /* (blue) != 0 : Most significant bit is */ 
     nku8_f    a_offset;     /* (alpha) beginning of bitfield */
     nku8_f    a_length;     /* (alpha length of bitfield */
     nku8_f    a_msb_right;  /* (alpha) != 0 : Most significant bit is */
} DisplayProperty;


typedef nku32_f  BacklightBrightness;
#define BRIGHTNESS_FULL         255
#define BRIGHTNESS_OFF          0


typedef nku32_f  PanelPowerState;
/* Screen power state */
#define               PANEL_POWER_OFF      0
#define               PANEL_POWER_ON       1


/*
 *
 */
#define VDISP_IOCTL(c, s)			_IOC(_IOC_NONE,'P',c,s)
#define VDISP_IOCTL_CLT_CMD_MAX			8
#define VDISP_IOCTL_SRV_CMD_MAX			8
#define VDISP_IOCTL_CMD_MAX			(VDISP_IOCTL_CLT_CMD_MAX + \
						 VDISP_IOCTL_SRV_CMD_MAX)
#endif /* _VLX_VDISP_COMMON_H */

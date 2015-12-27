/*
 ****************************************************************
 *
 *  Component: VLX virtual video driver
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
 *    Christophe Lizzi (Christophe.Lizzi@redbend.com)
 *
 ****************************************************************
 */

#include <asm/ioctl.h>
#include <linux/videodev2.h>

char*
vvideo_ioc_name (unsigned int ioc)
{
    static struct {
	unsigned int ioc;
	char*        name;
    } ioc_table[] = {
	{ VIDIOC_QUERYCAP, "VIDIOC_QUERYCAP" },
	{ VIDIOC_RESERVED, "VIDIOC_RESERVED" },
	{ VIDIOC_ENUM_FMT, "VIDIOC_ENUM_FMT" },
	{ VIDIOC_G_FMT, "VIDIOC_G_FMT" },
	{ VIDIOC_S_FMT, "VIDIOC_S_FMT" },
	{ VIDIOC_REQBUFS, "VIDIOC_REQBUFS" },
	{ VIDIOC_QUERYBUF, "VIDIOC_QUERYBUF" },
	{ VIDIOC_G_FBUF, "VIDIOC_G_FBUF" },
	{ VIDIOC_S_FBUF, "VIDIOC_S_FBUF" },
	{ VIDIOC_OVERLAY, "VIDIOC_OVERLAY" },
	{ VIDIOC_QBUF, "VIDIOC_QBUF" },
	{ VIDIOC_DQBUF, "VIDIOC_DQBUF" },
	{ VIDIOC_STREAMON, "VIDIOC_STREAMON" },
	{ VIDIOC_STREAMOFF, "VIDIOC_STREAMOFF" },
	{ VIDIOC_G_PARM, "VIDIOC_G_PARM" },
	{ VIDIOC_S_PARM, "VIDIOC_S_PARM" },
	{ VIDIOC_G_STD, "VIDIOC_G_STD" },
	{ VIDIOC_S_STD, "VIDIOC_S_STD" },
	{ VIDIOC_ENUMSTD, "VIDIOC_ENUMSTD" },
	{ VIDIOC_ENUMINPUT, "VIDIOC_ENUMINPUT" },
	{ VIDIOC_G_CTRL, "VIDIOC_G_CTRL" },
	{ VIDIOC_S_CTRL, "VIDIOC_S_CTRL" },
	{ VIDIOC_G_TUNER, "VIDIOC_G_TUNER" },
	{ VIDIOC_S_TUNER, "VIDIOC_S_TUNER" },
	{ VIDIOC_G_AUDIO, "VIDIOC_G_AUDIO" },
	{ VIDIOC_S_AUDIO, "VIDIOC_S_AUDIO" },
	{ VIDIOC_QUERYCTRL, "VIDIOC_QUERYCTRL" },
	{ VIDIOC_QUERYMENU, "VIDIOC_QUERYMENU" },
	{ VIDIOC_G_INPUT, "VIDIOC_G_INPUT" },
	{ VIDIOC_S_INPUT, "VIDIOC_S_INPUT" },
	{ VIDIOC_G_OUTPUT, "VIDIOC_G_OUTPUT" },
	{ VIDIOC_S_OUTPUT, "VIDIOC_S_OUTPUT" },
	{ VIDIOC_ENUMOUTPUT, "VIDIOC_ENUMOUTPUT" },
	{ VIDIOC_G_AUDOUT, "VIDIOC_G_AUDOUT" },
	{ VIDIOC_S_AUDOUT, "VIDIOC_S_AUDOUT" },
	{ VIDIOC_G_MODULATOR, "VIDIOC_G_MODULATOR" },
	{ VIDIOC_S_MODULATOR, "VIDIOC_S_MODULATOR" },
	{ VIDIOC_G_FREQUENCY, "VIDIOC_G_FREQUENCY" },
	{ VIDIOC_S_FREQUENCY, "VIDIOC_S_FREQUENCY" },
	{ VIDIOC_CROPCAP, "VIDIOC_CROPCAP" },
	{ VIDIOC_G_CROP, "VIDIOC_G_CROP" },
	{ VIDIOC_S_CROP, "VIDIOC_S_CROP" },
	{ VIDIOC_G_JPEGCOMP, "VIDIOC_G_JPEGCOMP" },
	{ VIDIOC_S_JPEGCOMP, "VIDIOC_S_JPEGCOMP" },
	{ VIDIOC_QUERYSTD, "VIDIOC_QUERYSTD" },
	{ VIDIOC_TRY_FMT, "VIDIOC_TRY_FMT" },
	{ VIDIOC_ENUMAUDIO, "VIDIOC_ENUMAUDIO" },
	{ VIDIOC_ENUMAUDOUT, "VIDIOC_ENUMAUDOUT" },
	{ VIDIOC_G_PRIORITY, "VIDIOC_G_PRIORITY" },
	{ VIDIOC_S_PRIORITY, "VIDIOC_S_PRIORITY" },
	{ VIDIOC_G_SLICED_VBI_CAP, "VIDIOC_G_SLICED_VBI_CAP" },
	{ VIDIOC_LOG_STATUS, "VIDIOC_LOG_STATUS" },
	{ VIDIOC_G_EXT_CTRLS, "VIDIOC_G_EXT_CTRLS" },
	{ VIDIOC_S_EXT_CTRLS, "VIDIOC_S_EXT_CTRLS" },
	{ VIDIOC_TRY_EXT_CTRLS, "VIDIOC_TRY_EXT_CTRLS" },
	{ VIDIOC_ENUM_FRAMESIZES, "VIDIOC_ENUM_FRAMESIZES" },
	{ VIDIOC_ENUM_FRAMEINTERVALS, "VIDIOC_ENUM_FRAMEINTERVALS" },
	{ VIDIOC_G_ENC_INDEX, "VIDIOC_G_ENC_INDEX" },
	{ VIDIOC_ENCODER_CMD, "VIDIOC_ENCODER_CMD" },
	{ VIDIOC_TRY_ENCODER_CMD, "VIDIOC_TRY_ENCODER_CMD" },
	{ 0, NULL }
    }, *entry;

    entry = ioc_table;
    while (entry->ioc) {
	if (entry->ioc == ioc) {
	    return entry->name;
	}
	entry++;
    }

    return "UNKNOWN";
}

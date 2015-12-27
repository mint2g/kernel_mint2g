/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual OpenGL ES (VOGL).                                 *
 *             VOGL front-end/back-end kernel driver common services.        *
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

#include <linux/module.h>
#include "vogl.h"

    /*
     * Convert a string to an unsigned long integer.
     */
    static unsigned long
stoui (const char* s, char** e, unsigned int base)
{
    unsigned long result;
    unsigned int  value;

    if ((s[0] == '0') && ((s[1] & 0xdf) == 'X')) {
        s   += 2;
        base = 16;
    } else if (!base) {
        base = 10;
    }

    for (result = 0;; s++) {
        if (('0' <= *s) && (*s <= '9')) {
            value = *s - '0';
        } else if ((base == 16) &&
		   ('A' <= (*s & 0xdf)) && ((*s & 0xdf) <= 'F')) {
            value = (*s & 0xdf) - 'A' + 10;
        } else {
            break;
        }
        result = result * (unsigned long) base + (unsigned long) value;
    }

    *e = (char*) s;

    return result;
}

    /*
     * Convert a string to a size and perform syntax verification.
     */
    static int
strtosize (const char* start, unsigned long* psize)
{
    unsigned long size;
    char*         end;

    size = stoui(start, &end, 0);

    if (start != end) {
        if ((*end & 0xdf) == 'K') {
            size = size * 1024;
            end++;
        } else if ((*end & 0xdf) == 'M') {
            size = size * 1024 * 1024;
            end++;
        } else if ((*end & 0xdf) == 'G') {
            size = size * 1024 * 1024 * 1024;
            end++;
        }

	if ((*end == ',') || (*end == '\0')) {
	    *psize = size;
	    return 0;
	}
    }

    *psize = 0;
    return -EINVAL;
}

    /*
     * Convert a string to a number and perform syntax verification.
     */
    static int
strtonum (const char* start, unsigned long* pnum)
{
    unsigned long num;
    char*         end;

    num = stoui(start, &end, 0);

    if ((start != end) && ((*end == ',') || (*end == '\0'))) {
	*pnum = num;
	return 0;
    }

    *pnum = 0;
    return -EINVAL;
}

    /*
     *
     */
    static unsigned long
vogl_size_get (Vlink* vlink, const char* param_name, unsigned long size_dflt)
{
    NkDevVlink*   nk_vlink = vlink->nk_vlink;
    const char*   s_info;
    char*         param;
    unsigned long size;

    if (nk_vlink->s_info) {
	s_info = (const char*) nkops.nk_ptov(nk_vlink->s_info);
	param  = strstr(s_info, param_name);
	if (param) {
	    if (strtosize(param + strlen(param_name), &size) == 0) {
		return size;
	    }
	}
    }

    return size_dflt;
}

    /*
     *
     */
    static unsigned long
vogl_num_get (Vlink* vlink, const char* param_name, unsigned long num_dflt)
{
    NkDevVlink*   nk_vlink = vlink->nk_vlink;
    const char*   s_info;
    char*         param;
    unsigned long num;

    if (nk_vlink->s_info) {
	s_info = (const char*) nkops.nk_ptov(nk_vlink->s_info);
	param  = strstr(s_info, param_name);
	if (param) {
	    if (strtonum(param + strlen(param_name), &num) == 0) {
		return num;
	    }
	}
    }

    return num_dflt;
}

    /*
     *
     */
    int
vogl_vrpq_prop_get (Vlink* vlink, unsigned int type, void* prop)
{
    switch (type) {
    case VRPQ_PROP_PMEM_SIZE:
    {
	NkPhSize* psz = (NkPhSize*) prop;
	*psz = vogl_size_get(vlink, "vrpq-pmem=", VOGL_VRPQ_PMEM_SIZE_DFLT);
	break;
    }
    case VRPQ_PROP_RING_REQS_MAX:
    {
	VrpqRingIdx* preqs = (VrpqRingIdx*) prop;
	VrpqRingIdx  reqs;
	reqs = vogl_num_get(vlink, "vrpq-reqs=", VOGL_VRPQ_REQS_DEFLT);
	*preqs = reqs;
	break;
    }
    default:
	return -EINVAL;
    }
    return 0;
}

    /*
     *
     */
    int
vogl_vumem_prop_get (Vlink* vlink, unsigned int type, void* prop)
{
    switch (type) {
    case VUMEM_PROP_PDEV_SIZE:
    {
	NkPhSize* psz = (NkPhSize*) prop;
	*psz = vogl_size_get(vlink, "vumem-pdev=", VOGL_VUMEM_PDEV_SIZE_DFLT);
	break;
    }
    default:
	return -EINVAL;
    }
    return 0;
}

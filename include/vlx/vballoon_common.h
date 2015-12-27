/*
 ****************************************************************
 *
 *  Component: VLX memory balloon driver interface
 *
 *  Copyright (C) 2012, Red Bend Ltd.
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
 *
 ****************************************************************
 */

#ifndef _VBALLOON_COMMON_H_
#define _VBALLOON_COMMON_H_

typedef struct vballoon_pmem_t {
    nku32_f target_pages;   /* target pages   (client - WO / server - RO) */
    nku32_f current_pages;  /* current pages  (client - RO / server - WO) */
    nku32_f resident_pages; /* resident pages (client - RO / server - WO) */
    nku32_f balloon_pages;  /* balloon pages  (client - RO / server - WO) */
    nku32_f balloon_init_pages;/* balloon pages  (client - RO / server - WO) */
    nku32_f balloon_min_pages; /* balloon pages  (client - RO / server - WO) */
    nku32_f balloon_max_pages; /* balloon pages  (client - RO / server - WO) */
} vballoon_pmem_t;

#endif

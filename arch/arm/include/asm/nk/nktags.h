/*
 ****************************************************************
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
 *  #ident  "@(#)nktags.h 1.3     07/06/02 Red Bend"
 *
 *  Contributor(s):
 *    Guennadi Maslov (guennadi.maslov@redbend.com)
 *
 ****************************************************************
 */

#ifndef NK_NKTAGS_H
#define NK_NKTAGS_H

#include "nk_f.h"
#include "f_nk.h"

    /*
     * Tag list management.
     *
     * Tag list is a set of tags. Each tag has a fixed-size
     * header and a variable-size body. Tag header consist of
     * two fields: tag value (int) and tag size (tag header
     * plus tag body) measured in 32-bit words.
     *
     * Tag list always starts with an ATAG_CORE tag. It ends
     * with a zero-sized tag.
     */

#define ATAG_NONE	0x00000000
#define ATAG_CORE	0x54410001
#define ATAG_MEM	0x54410002
#define ATAG_RAMDISK	0x54410004
#define ATAG_INITRD	0x54410005
#define ATAG_INITRD2	0x54420005
#define ATAG_SERIAL	0x54410006
#define ATAG_REVISION	0x54410007
#define ATAG_CMDLINE	0x54410009

#define ATAG_ARCH_ID	0x4b4e0001
#define ATAG_MAP_DESC	0x4b4e0004
#define ATAG_TTB_FLAGS	0x4b4e0005
#define ATAG_TAG_REF	0x4b4e0009

    /*
     * Tag list header
     */

typedef struct NkTagHeader {
    nku32_f	size;
    nku32_f	tag;
} NkTagHeader;

    /*
     * Calculate the tag list length in bytes
     */
extern nku32_f tags_len (NkTagHeader* t);

    /*
     * Find a tag in the tag list
     */
extern NkTagHeader* find_tag (NkTagHeader* t, nku32_f tag);

    /*
     * Remove a tag from the tag list
     */
extern void remove_tag (NkTagHeader* t);

    /*
     * Next tag in the tag list
     */
extern NkTagHeader* next_tag(NkTagHeader* t);

    /*
     * Add a command line tag to the tag list.
     * Check if the resulting tag list exceeds its size limit.
     * Return added tag.
     */
extern NkTagHeader* add_cmd_line_tag (NkTagHeader* t,
				      char* cmd_line, nku32_f limit);

    /*
     * Add an init ram disk tag to the tag list.
     * Check if the resulting tag list exceeds its size limit.
     * Return added tag.
     */
extern NkTagHeader* add_ram_disk_tag (NkTagHeader* t, NkVmAddr start,
				      NkVmSize size, nku32_f limit);

    /*
     * Dump a tag list
     */
extern void dump_taglist(NkTagHeader* t);

#endif

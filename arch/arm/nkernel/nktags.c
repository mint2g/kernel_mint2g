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
 *  #ident  "@(#)nktags.c 1.3     08/02/07 Red Bend"
 *
 *  Contributor(s):
 *    Guennadi Maslov (guennadi.maslov@redbend.com)
 *
 ****************************************************************
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <asm/nk/nktags.h>

    /*
     * Move to the next tag in the tag list.
     */
    static inline NkTagHeader*
_next_tag (NkTagHeader* t)
{
    return (NkTagHeader*)((nku32_f*)t + t->size);
}

    /*
     * Find the last tag in the tag list.
     */
    static inline NkTagHeader*
_find_last_tag (NkTagHeader* t)
{
    while (t->size != 0) {
	t = _next_tag(t);
    }
    return t;
}

    /*
     * Calculate the tag list length in bytes.
     * Parameters: pointers to the first and to the last tag.
     */
    static inline nku32_f
_tags_len(NkTagHeader* t, NkTagHeader* l)
{
    return ((nku32_f)l - (nku32_f)t + sizeof(NkTagHeader));
}

    /*
     * Calculate the tag list length in bytes
     */
    nku32_f
tags_len (NkTagHeader* t)
{
    NkTagHeader* last;

    last = _find_last_tag(t);

    return _tags_len(t, last);
}

    /*
     * Find a tag in the tag list
     */
    NkTagHeader*
find_tag (NkTagHeader* t, nku32_f tag)
{
    while (t->size != 0) {
	if (t->tag == tag) {
	    return t;
	}
	t = _next_tag(t);
    }
    return 0;
}

    /*
     * Remove a tag from the tag list
     */
    void
remove_tag (NkTagHeader* t)
{
    NkTagHeader* next;
    nku32_f	 size;

    next = _next_tag(t);
    size = tags_len(next);
    memcpy(t, next, size);
}

    /*
     * Next tag in the tag list
     */
    NkTagHeader*
next_tag(NkTagHeader* t)
{
    return _next_tag(t);
}

    /*
     * Add a command line tag to the tag list.
     * Check if the resulting tag list exceeds its size limit.
     * Return added tag.
     */
    NkTagHeader*
add_cmd_line_tag (NkTagHeader* t, char* cmd_line, nku32_f limit)
{
    NkTagHeader* last;
    NkTagHeader* cmdl;
    NkTagHeader	 ltag;
    nku32_f	 size;

    last = _find_last_tag(t);
    ltag = *last;

	/*
	 * Calculate new tag size (at first in bytes, then in words)
	 */
    size = strlen(cmd_line) + 1 + sizeof(NkTagHeader);
    size = (size + sizeof(nku32_f) - 1)/sizeof(nku32_f);

	/*
	 * Check for overflow
	 */
    if (_tags_len(t, last) + size * sizeof(nku32_f) > limit) {
	return 0;
    }

	/*
	 * Add a new command line tag
	 */
    last->tag  = ATAG_CMDLINE;
    last->size = size;
    strcpy((char*)(last+1), cmd_line);
    cmdl = last;

	/*
	 * Add a termination tag
	 */
    last = _next_tag(last);
    *last = ltag;

    return cmdl;
}

    /*
     * Add an init ram disk tag to the tag list.
     * Check if the resulting tag list exceeds its size limit.
     * Return added tag.
     */
    NkTagHeader*
add_ram_disk_tag (NkTagHeader* t, NkPhAddr start, NkPhSize size, nku32_f limit)
{
    NkTagHeader* last;
    NkTagHeader* rdsk;
    NkTagHeader	 ltag;
    nku32_f*	 tag;

    last = _find_last_tag(t);
    ltag = *last;

	/*
	 * Check for overflow
	 */
    if (_tags_len(t, last) + sizeof(NkTagHeader) + 2*sizeof(nku32_f) > limit) {
	return 0;
    }

	/*
	 * Add a new initrd tag
	 */
    last->tag  = ATAG_INITRD2;
    last->size = sizeof(NkTagHeader)/sizeof(nku32_f) + 2;

    tag    = (nku32_f*)(last+1);
    tag[0] = start;
    tag[1] = size;

    rdsk = last;

	/*
	 * Add a termination tag
	 */
    last = _next_tag(last);
    *last = ltag;

    return rdsk;
}
    /*
     * Dump a tag list
     */
    void
dump_taglist(NkTagHeader* t)
{
    if (t->tag != ATAG_CORE) {
	printk("No tag list found at 0x%08x\n", (unsigned int)t);
	return;
    }

    printk(" === dump tag list at 0x%08x ===\n", (unsigned int)t);
    for (;;) {
	printk(" tag = 0x%08x  size = 0x%08x\n", t->tag, t->size);

	if (t->size == 0) break;

	switch (t->tag) {
	  case ATAG_CORE: {
	    nku32_f* tag = (nku32_f*)(t+1);
	    printk("   core tag:      0x%08x 0x%08x 0x%08x\n",
		      tag[0], tag[1], tag[2]);
	    break;
	  }
	  case ATAG_CMDLINE: {
	    printk("   cmd line tag:  <%s>\n", (char*)(t+1));
	    break;
	  }
	  case ATAG_MEM: {
	    nku32_f* tag = (nku32_f*)(t+1);
	    printk("   mem chunk tag: start = 0x%08x size = 0x%08x\n",
		      tag[1], tag[0]);
	    break;
	  }
	  case ATAG_RAMDISK: {
	    nku32_f* tag = (nku32_f*)(t+1);
	    printk("   ram disk tag:  start = 0x%08x size = 0x%08x"
		      " flags =%d\n", tag[2], tag[1], tag[0]);
	    break;
	  }
	  case ATAG_INITRD: {
	    nku32_f* tag = (nku32_f*)(t+1);
	    printk("   initrd tag:    start = 0x%08x size = 0x%08x\n",
		      tag[0], tag[1]);
	    break;
	  }
	  case ATAG_INITRD2: {
	    nku32_f* tag = (nku32_f*)(t+1);
	    printk("   initrd2 tag:   start = 0x%08x size = 0x%08x\n",
		      tag[0], tag[1]);
	    break;
	  }
	  case ATAG_SERIAL: {
	    nku32_f* tag = (nku32_f*)(t+1);
	    printk("   serial number:  0x%08x%08x\n", tag[1], tag[0]);
	    break;
	  }
	  case ATAG_REVISION: {
	    nku32_f* tag = (nku32_f*)(t+1);
	    printk("   board revision: 0x%08x\n", tag[0]);
	    break;
	  }
	  case ATAG_ARCH_ID: {
	    nku32_f* tag = (nku32_f*)(t+1);
	    printk("   arch id tag:    %d\n", tag[0]);
	    break;
	  }
	  case ATAG_MAP_DESC: {
	    nku32_f* tag = (nku32_f*)(t+1);
	    printk("   map desc tag:   pstart = 0x%08x plimit = 0x%08x\n",
		      tag[0], tag[1]);
	    printk("                   vstart = 0x%08x pte_attr = 0x%03x\n",
		      tag[2], tag[3]);
	    printk("                   mem_type = %d mem_owner = %d\n",
		      tag[4], tag[5]);
	    break;
	  }
	  case ATAG_TAG_REF: {
	    nku32_f* tag = (nku32_f*)(t+1);
	    printk("   tag ref:        addr = 0x%08x\n", tag[0]);
	    break;
	  }
	}

	t = _next_tag(t);
    }
    printk("\n");
}

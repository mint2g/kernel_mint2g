/*
 ****************************************************************
 *
 *  Component: VLX virtual Android Physical Memory driver
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
 *    Vladimir Grouzdev (Vladimir.Grouzdev@redbend.com)
 *
 ****************************************************************
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>

#undef VPMEM_DEBUG

#include "vlx/vpmem_common.h"
#include "vpmem.h"

static vpmem_dev_t* vpmem_dev_head;

    static vpmem_dev_t*
vpmem_dev_alloc (void)
{
    vpmem_dev_t* vpmem = kzalloc(sizeof(vpmem_dev_t), GFP_KERNEL);
    if (!vpmem) {
	return 0;
    }

    memset(vpmem, 0, sizeof(*vpmem));

    vpmem->next    = vpmem_dev_head;
    vpmem_dev_head = vpmem;

    DTRACE("allocated new vpmem 0x%p\n", vpmem);

    return vpmem;
}

    static void
vpmem_dev_free (vpmem_dev_t* vpmem)
{
    vpmem_dev_t** link = &vpmem_dev_head;
    vpmem_dev_t*  curr;

    while ((curr = *link) && (curr != vpmem)) link = &curr->next;

    BUG_ON(!curr);

    *link = vpmem->next;

    DTRACE("freeing vpmem 0x%p\n", vpmem);

    kfree(vpmem);
}

    static char*
_a2ui (char* s, unsigned int* i)
{
    unsigned int xi = 0;
    char         c  = *s;

    while (('0' <= c) && (c <= '9')) {
	xi = xi * 10 + (c - '0');
	c = *(++s);
    }

    if        ((*s == 'K') || (*s == 'k')) {
	xi *= 1024;
	s  += 1;
    } else if ((*s == 'M') || (*s == 'm')) {
	xi *= (1024*1024);
	s  += 1;
    }

    *i = xi;

    return s;
}

    int
vpmem_info_name (char* info, char* name, int maxlen)
{
    int len = 0;

    if (info) {
	while (info[len] && (info[len] != ',')) {
	    name[len] = info[len];
	    if (++len == (maxlen - 1)) {
		break;
	    }
	}
    }
    name[len] = '\0';

    DTRACE("info %s -> name %s (len %d)\n", (info ? info : ""), name, len);

    return len;
}
EXPORT_SYMBOL(vpmem_info_name);

    unsigned int
vpmem_info_size (char* link_info)
{
    unsigned int size = VPMEM_DEFAULT_SIZE;
    char*        info = link_info;

    if (info) {
	while (*info && (*info != ',')) info++;
	if (*info) {
	    info = _a2ui(info+1, &size);
	    if (*info && (*info != ',') && (*info != '@')) {
		size = VPMEM_DEFAULT_SIZE;
	    }
	}
    }

    if (size < PAGE_SIZE) {
	size = PAGE_SIZE;
    }

    DTRACE("info %s -> size 0x%x\n", (link_info ? link_info : ""), size);

    return size;
}
EXPORT_SYMBOL(vpmem_info_size);

    unsigned int
vpmem_info_base (char* link_info)
{
    unsigned int base = 0;
    char*        info = link_info;

    if (info) {
	while (*info && (*info != ',')) info++;
	if (*info) {
	    unsigned int size;
	    info = _a2ui(info+1, &size);
	    if (*info == '@') {
	        info = _a2ui(info+1, &base);
		if (*info && (*info != ',')) {
		    base = 0;
		}
	    }
	}
    }

    DTRACE("info %s -> base 0x%x\n", (link_info ? link_info : ""), base);

    return base;
}
EXPORT_SYMBOL(vpmem_info_base);

    unsigned int
vpmem_info_id (char* link_info)
{
    unsigned int id   = 0;
    char*        info = link_info;

    if (info) {
	while (*info && (*info != ',')) info++;
	if (info) {
	    info++;
	    while (*info && (*info != ',')) info++;
	    if (*info) {
	        info = _a2ui(info+1, &id);
	        if (*info) {
		    id = 0;
	        }
	    }
	}
    }

    DTRACE("info %s -> ID %d\n", (link_info ? link_info : "") , id);

    return id;
}
EXPORT_SYMBOL(vpmem_info_id);

    int
vpmem_module_init (int is_client, vpmem_dev_init_t dev_init)
{
    NkPhAddr       plink;
    NkDevVlink*    vlink;
    int 	   err = 0;
    NkOsId         my_id = nkops.nk_id_get();
    NkOsId         vlink_id;
    vpmem_dev_t*   vpmem;
    int            count = 0;

    DTRACE("initializing module, my_id %ld\n", (unsigned long)my_id);

    plink = 0;
    while ((plink = nkops.nk_vlink_lookup(VPMEM_VLINK_NAME, plink)) != 0) {

	vlink = nkops.nk_ptov(plink);

	vlink_id = is_client ? vlink->c_id : vlink->s_id;

	DTRACE("comparing my_id %d to vlink_id %d (c_id %d, s_id %d)\n",
		my_id, vlink_id, vlink->c_id, vlink->s_id);

	if (vlink_id == my_id) {

	    vpmem = vpmem_dev_alloc();
	    if (!vpmem) {
		err = -ENOMEM;
		break;
	    }

	    vpmem->plink = plink;
	    vpmem->vlink = vlink;

	    err = dev_init(vpmem);
	    if (err) {
		vpmem_dev_free(vpmem);
		break;
	    }

	    count++;

	    DTRACE("device %s is created for OS#%d<-OS#%d link=%d\n",
		   vpmem->name, vlink->s_id, vlink->c_id, vlink->link);
	}
    }

    DTRACE("module initialized, %u vpmem devices created, err %d\n",
	    count, err);

    return err;
}
EXPORT_SYMBOL(vpmem_module_init);

    void
vpmem_module_exit (vpmem_dev_exit_t dev_exit)
{
    DTRACE("removing module\n");
    while (vpmem_dev_head) {
	dev_exit(vpmem_dev_head);
        vpmem_dev_free(vpmem_dev_head);
    }
}
EXPORT_SYMBOL(vpmem_module_exit);

    static vpmem_dev_t*
vpmem_dev_lookup_by_name (char* name)
{
    vpmem_dev_t* vpmem = vpmem_dev_head;
    while (vpmem && strcmp(vpmem->name, name)) {
	vpmem = vpmem->next;
    }

    if (vpmem) {
	DTRACE("vpmem %s: [0x%x..0x%x]\n",
	       name, vpmem->pmem_phys, vpmem->pmem_phys + vpmem->pmem_size);
    } else {
	DTRACE("vpmem %s not found\n", name);
    }

    return vpmem;
}

    vpmem_dev_t*
vpmem_dev_peer (vpmem_dev_t* vpmem)
{
    vpmem_dev_t* peer = vpmem_dev_head;
    while (peer) {
	if ((peer != vpmem) && (peer->vlink->link == vpmem->vlink->link)) {
	    return peer;
	}
	peer = peer->next;
    }
    return 0;
}
EXPORT_SYMBOL(vpmem_dev_peer);

// vpmem API for use by the other virtual drivers

    vpmem_handle_t
vpmem_lookup (char* name)
{
    return (vpmem_handle_t)vpmem_dev_lookup_by_name(name);
}
EXPORT_SYMBOL(vpmem_lookup);

    unsigned char*
vpmem_map (vpmem_handle_t handle)
{
    vpmem_dev_t* vpmem = (vpmem_dev_t*)handle;
    if (!vpmem->pmem_base && vpmem->pmem_phys) {
	vpmem->pmem_base = (char*)nkops.nk_mem_map(vpmem->pmem_phys,
						   vpmem->pmem_size);
    }
    return vpmem->pmem_base;
}
EXPORT_SYMBOL(vpmem_map);

    void
vpmem_unmap (vpmem_handle_t handle)
{
    vpmem_dev_t* vpmem = (vpmem_dev_t*)handle;
    if (vpmem->pmem_base && vpmem->pmem_phys) {
	nkops.nk_mem_unmap(vpmem->pmem_base, vpmem->pmem_phys,
			   vpmem->pmem_size);
	vpmem->pmem_base = 0;
    }
}
EXPORT_SYMBOL(vpmem_unmap);

    unsigned long
vpmem_phys (vpmem_handle_t handle)
{
    return ((vpmem_dev_t*)handle)->pmem_phys;
}
EXPORT_SYMBOL(vpmem_phys);

    unsigned int
vpmem_size (vpmem_handle_t handle)
{
    return ((vpmem_dev_t*)handle)->pmem_size;
}
EXPORT_SYMBOL(vpmem_size);

    unsigned int
vpmem_id (vpmem_handle_t handle)
{
    return ((vpmem_dev_t*)handle)->id;
}
EXPORT_SYMBOL(vpmem_id);

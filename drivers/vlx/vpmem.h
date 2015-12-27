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
 *
 ****************************************************************
 */

#ifndef _VPMEM_H_
#define _VPMEM_H_

// vpmem API for use by the other virtual drivers

typedef void* vpmem_handle_t;

vpmem_handle_t vpmem_lookup (char* name);
unsigned char* vpmem_map    (vpmem_handle_t handle);
void           vpmem_unmap  (vpmem_handle_t handle);
unsigned long  vpmem_phys   (vpmem_handle_t handle);
unsigned int   vpmem_size   (vpmem_handle_t handle);
unsigned int   vpmem_id     (vpmem_handle_t handle);

#endif // _VPMEM_H_

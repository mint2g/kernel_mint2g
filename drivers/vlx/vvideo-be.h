/*
 ****************************************************************
 *
 *  Component: VLX virtual video backend driver
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

#ifndef _VVIDEO_BE_H_
#define _VVIDEO_BE_H_

typedef struct vvideo_hw_ops_t {
    unsigned int  version;
    int (*open)   (int minor, NkPhAddr plink, VVideoDesc* desc, void** private_data);
    int (*release)(void* private_data);
    int (*ioctl)  (void* private_data, unsigned int cmd, void* arg);
    int (*mmap)   (void* private_data, unsigned long pgoff, unsigned long* bus_addr);
    int (*munmap) (void* private_data, unsigned long pgoff, unsigned long size);
} vvideo_hw_ops_t;

#define VVIDEO_HW_OPS_VERSION 3

#endif /* _VVIDEO_BE_H_ */

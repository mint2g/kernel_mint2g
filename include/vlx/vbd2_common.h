/*
 ****************************************************************
 *
 *  Component: VLX VBD v.2 Interface
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
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *    Eric Lescouet (eric.lescout@redbend.com)
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

#ifndef VBD2_COMMON_H
#define VBD2_COMMON_H

typedef nku16_f vbd2_devid_t;
typedef nku32_f vbd2_genid_t;
typedef nku64_f vbd2_sector_t;
typedef nku64_f vbd2_cookie_t;
typedef nku8_f  vbd2_op_t;
typedef nku8_f  vbd2_status_t;
typedef nku8_f  vbd2_count_t;

#define VBD2_DEVID(major,minor)	(((major) << 8) | (minor))
#define VBD2_DEVID_MAJOR(devid)	((devid) >> 8)
#define VBD2_DEVID_MINOR(devid)	((devid) & 0xff)

#define VBD2_OP_INVALID  	0
#define VBD2_OP_PROBE    	1
#define VBD2_OP_READ     	2
#define VBD2_OP_WRITE    	3
#define VBD2_OP_READ_EXT	4
#define VBD2_OP_WRITE_EXT	5
#define VBD2_OP_MEDIA_PROBE	6
#define VBD2_OP_MEDIA_CONTROL	7
#define VBD2_OP_MEDIA_LOCK	8
#ifdef VBD2_ATAPI
#define VBD2_OP_ATAPI		9
#endif
#define VBD2_OP_OPEN		10	/* VBD v.2 only */
#define VBD2_OP_CLOSE		11	/* VBD v.2 only */
#define VBD2_OP_GETGEO		12	/* VBD v.2 only */
#define VBD2_OP_CHANGES		13	/* VBD v.2 only */
#define VBD2_OP_MAX		14

#define VBD2_OP_NAMES \
    "Invalid",   "Probe",    "Read",       "Write", \
    "ReadExt",   "WriteExt", "MediaProbe", "MediaControl", \
    "MediaLock", "Atapi",    "Open",       "Close", \
    "GetGeo",    "Changes"

#define VBD2_STATIC_ASSERT(x)	extern char vbd2_static_assert [(x) ? 1 : -1]

    /*
     * Remote Disk request header.
     * Buffers follow just behind this header.
     */    
typedef struct vbd2_req_header_t {
    vbd2_cookie_t cookie;	/* (64b) to put in the response descriptor */
    vbd2_sector_t sector;	/* (64b) sector */
    vbd2_devid_t  devid;	/* (16b) device ID */
    vbd2_op_t     op;		/*  (8b) operation */
    vbd2_count_t  count;	/*  (8b) number of buffers which follows */
    vbd2_genid_t  genid;	/* (32b) generation of devid */
} vbd2_req_header_t;

VBD2_STATIC_ASSERT (sizeof (vbd2_req_header_t) == 24);

    /*
     * The buffer address is in fact a page physical address with the
     * first and last sector numbers incorporated as less significant
     * bits ([5-9] and [0-4]).
     * Note that the buffer address type is the physical address type
     * and it is architecture specific.
     */
typedef NkPhAddr vbd2_buffer_t;
#define	VBD2_FIRST_BUF(req) \
	((vbd2_buffer_t*)(((nku8_f*)(req)) + sizeof(vbd2_req_header_t)))

#define	VBD2_SECT_SIZE_BITS	9	/* sector is 512 bytes   */
#define	VBD2_SECT_SIZE		(1 << VBD2_SECT_SIZE_BITS)

#define	VBD2_SECT_NUM_BITS	5	/* sector number is 0..31 */
#define	VBD2_SECT_NUM_MASK	((1 << VBD2_SECT_NUM_BITS) - 1)

#define	VBD2_EXT_SHIFT         12
#define	VBD2_EXT_SIZE          (1 << VBD2_EXT_SHIFT)
#define	VBD2_EXT_MASK          (VBD2_EXT_SIZE - 1)

#define	VBD2_BUFFER(paddr, start, end) \
	((paddr) | ((start) << VBD2_SECT_NUM_BITS) | (end))
#define	VBD2_BUFFER_EXT(paddr, psize) \
    ((((vbd2_buffer_t)(paddr)) << VBD2_EXT_SHIFT) | \
     ((psize) == VBD2_EXT_SIZE ? 0 : (psize)))

#define	VBD2_BUF_SSECT(buff) \
	(((buff) >> VBD2_SECT_NUM_BITS) & VBD2_SECT_NUM_MASK)
#define	VBD2_BUF_ESECT(buff) \
	((buff) & VBD2_SECT_NUM_MASK)
#define	VBD2_BUF_SECTS(buff) \
	(VBD2_BUF_ESECT(buff) - VBD2_BUF_SSECT(buff) + 1)

#define	VBD2_BUF_SOFF(buff) (VBD2_BUF_SSECT(buff) << VBD2_SECT_SIZE_BITS)
#define	VBD2_BUF_EOFF(buff) (VBD2_BUF_ESECT(buff) << VBD2_SECT_SIZE_BITS)
#define	VBD2_BUF_SIZE(buff) (VBD2_BUF_SECTS(buff) << VBD2_SECT_SIZE_BITS)
#define	VBD2_BUF_SIZE_EXT(buff) (((buff) & VBD2_EXT_MASK) ? \
				  ((buff) & VBD2_EXT_MASK) : VBD2_EXT_SIZE)

#define	VBD2_BUF_PAGE(buff) ((buff) & ~((1 << (VBD2_SECT_NUM_BITS*2)) - 1))
#define	VBD2_BUF_BASE(buff) (VBD2_BUF_PAGE(buff) + VBD2_BUF_SOFF(buff))
#define	VBD2_BUF_BASE_EXT(buff) ((buff) >> VBD2_EXT_SHIFT)

    /*
     * Response descriptor
     * status is the "count" field.
     */
typedef struct vbd2_req_header_t vbd2_resp_t;

#define VBD2_STATUS_OK		0
#define VBD2_STATUS_ERROR	0xff

typedef struct vbd2_get_geo_t {
    vbd2_req_header_t	common;
    nku16_f		sects_per_track;
    nku16_f		heads;
    nku32_f		cylinders;
} vbd2_get_geo_t;

VBD2_STATIC_ASSERT (sizeof (vbd2_get_geo_t) == 32);

    /*
     * Probing record
     */
typedef nku16_f vbd2_info_t;

typedef struct vbd2_probe_t {
    vbd2_devid_t  devid;	/* device ID (16 bits) */
    vbd2_info_t   info;		/* device type & flags (16 bits) */
    vbd2_genid_t  genid;	/* generation of devid (32 bits) */
    vbd2_sector_t sectors;	/* size in sectors (64 bits) */
} vbd2_probe_t;

VBD2_STATIC_ASSERT (sizeof (vbd2_probe_t) == 16);

typedef struct vbd2_probe_link_t {
    vbd2_req_header_t	common;
    vbd2_probe_t	probe[1];
} vbd2_probe_link_t;

VBD2_STATIC_ASSERT (sizeof (vbd2_probe_link_t) == 40);

typedef union {
    vbd2_req_header_t	req;
    vbd2_resp_t		resp;
    vbd2_get_geo_t	getgeo;
    vbd2_probe_link_t	probe;
} vbd2_msg_t;

VBD2_STATIC_ASSERT (sizeof (vbd2_msg_t) == 40);

    /*
     * Types below match ide_xxx in Linux ide.h
     */
#define VBD2_TYPE_FLOPPY  0x00
#define VBD2_TYPE_TAPE    0x01
#define VBD2_TYPE_CDROM   0x05
#define VBD2_TYPE_OPTICAL 0x07
#define VBD2_TYPE_DISK    0x20 

#define VBD2_TYPE_MASK    0x3f
#define VBD2_TYPE(x)      ((x) & VBD2_TYPE_MASK) 

#define VBD2_FLAG_RO      0x0040
#define VBD2_FLAG_VIRT    0x0080
	/* REMOVABLE MEDIA support */
#define VBD2_FLAG_CHANGED 0x0100	/* Media has changed */
#define VBD2_FLAG_LOCKED  0x0200	/* Media locked (lock in command) */
#define VBD2_FLAG_LOEJ    0x0400	/* Load/Eject flag (command) */
#define VBD2_FLAG_START   0x0800	/* Start/Stop unit (command) */

#ifdef VBD2_ATAPI
   /*
    * ATAPI request support
    */
typedef struct vbd2_atapi_sense_t {
  nku8_f error_code          : 7;
  nku8_f valid               : 1;
  nku8_f segment_number;
  nku8_f sense_key           : 4;
  nku8_f reserved2           : 1;
  nku8_f ili                 : 1;
  nku8_f reserved1           : 2;
  nku8_f information[4];
  nku8_f add_sense_len;
  nku8_f command_info[4];
  nku8_f asc;
  nku8_f ascq;
  nku8_f fruc;
  nku8_f sks[3];
  nku8_f asb[46];
} vbd2_atapi_sense_t;

#define VBD2_ATAPI_PKT_SZ	12        /* ATAPI cmd packet size */
#define VBD2_ATAPI_REP_SZ	(64*1024) /* ATAPI reply (max) size */

typedef struct vbd2_atapi_req_t {
    nku8_f		cdb[VBD2_ATAPI_PKT_SZ];	/* ATAPI cmd. desc. bloc */
    nku32_f		buflen;			/* reply buffer len */
    nku32_f		status;			/* returned (ioctl) status */
    vbd2_atapi_sense_t	sense;			/* returned sense info */
} __attribute__ ((packed)) vbd2_atapi_req_t;
#endif	/* VBD2_ATAPI */

#endif	/* VBD2_COMMON_H */


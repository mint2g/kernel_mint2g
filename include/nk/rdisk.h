/*
 ****************************************************************
 *
 *  Component: VLX virtual block device driver
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
 *
 ****************************************************************
 */

#ifndef _D_RDISK_H
#define _D_RDISK_H

#define	RDISK_RING_TYPE		0x44495348	/* DISK */

typedef nku16_f RDiskDevId;
typedef nku32_f RDiskSector;
typedef nku64_f RDiskCookie;
typedef nku8_f  RDiskOp;
typedef nku8_f  RDiskStatus;
typedef nku8_f  RDiskCount;

#define RDISK_OP_INVALID  	0
#define RDISK_OP_PROBE    	1
#define RDISK_OP_READ     	2
#define RDISK_OP_WRITE    	3
#define RDISK_OP_READ_EXT	4
#define RDISK_OP_WRITE_EXT	5
#define RDISK_OP_MEDIA_PROBE	6
#define RDISK_OP_MEDIA_CONTROL	7
#define RDISK_OP_MEDIA_LOCK	8
#ifdef VBD_ATAPI
#define RDISK_OP_ATAPI		9
#endif

    //
    // Remote Disk request header.
    // Buffers follow just behind this header.
    //    
typedef struct RDiskReqHeader {
    RDiskSector sector[2];	/* sector [0] - low; [1] - high */
    RDiskCookie cookie;		/* cookie to put in the response descriptor */
    RDiskDevId  devid;		/* device ID */
    RDiskOp     op;		/* operation */
    RDiskCount  count;		/* number of buffers which follows */
} __attribute__ ((packed)) RDiskReqHeader;

//
// The buffer address is in fact a page physical address with the first and
// last sector numbers incorporated as less significant bits ([5-9] and [0-4]).
// Note that the buffer address type is the physcial address type and it
// is architecture specific.
//
typedef NkPhAddr RDiskBuffer;
#define	RDISK_FIRST_BUF(req) \
	((RDiskBuffer*)(((nku8_f*)(req)) + sizeof(RDiskReqHeader)))

#define	RDISK_SECT_SIZE_BITS	9	/* sector is 512 bytes   */
#define	RDISK_SECT_SIZE		(1 << RDISK_SECT_SIZE_BITS)

#define	RDISK_SECT_NUM_BITS	5	/* sector number is 0..31 */
#define	RDISK_SECT_NUM_MASK	((1 << RDISK_SECT_NUM_BITS) - 1)

#define	RDISK_EXT_SHIFT         12
#define	RDISK_EXT_SIZE          (1 << RDISK_EXT_SHIFT)
#define	RDISK_EXT_MASK          (RDISK_EXT_SIZE - 1)

#define	RDISK_BUFFER(paddr, start, end) \
	((paddr) | ((start) << RDISK_SECT_NUM_BITS) | (end))
#define	RDISK_BUFFER_EXT(paddr, psize) \
    ((((RDiskBuffer)(paddr)) << RDISK_EXT_SHIFT) | \
     ((psize) == RDISK_EXT_SIZE ? 0 : (psize)))

#define	RDISK_BUF_SSECT(buff) \
	(((buff) >> RDISK_SECT_NUM_BITS) & RDISK_SECT_NUM_MASK)
#define	RDISK_BUF_ESECT(buff) \
	((buff) & RDISK_SECT_NUM_MASK)
#define	RDISK_BUF_SECTS(buff) \
	(RDISK_BUF_ESECT(buff) - RDISK_BUF_SSECT(buff) + 1)

#define	RDISK_BUF_SOFF(buff) (RDISK_BUF_SSECT(buff) << RDISK_SECT_SIZE_BITS)
#define	RDISK_BUF_EOFF(buff) (RDISK_BUF_ESECT(buff) << RDISK_SECT_SIZE_BITS)
#define	RDISK_BUF_SIZE(buff) (RDISK_BUF_SECTS(buff) << RDISK_SECT_SIZE_BITS)
#define	RDISK_BUF_SIZE_EXT(buff) (((buff) & RDISK_EXT_MASK) ? \
				  ((buff) & RDISK_EXT_MASK) : RDISK_EXT_SIZE)

#define	RDISK_BUF_PAGE(buff) ((buff) & ~((1 << (RDISK_SECT_NUM_BITS*2)) - 1))
#define	RDISK_BUF_BASE(buff) (RDISK_BUF_PAGE(buff) + RDISK_BUF_SOFF(buff))
#define	RDISK_BUF_BASE_EXT(buff) ((buff) >> RDISK_EXT_SHIFT)

    //
    // Response descriptor
    //
typedef struct RDiskResp {
    RDiskCookie cookie;		// cookie copied from request
    RDiskOp     op;		// operation code copied from request
    RDiskStatus status;		// operation status
} __attribute__ ((packed)) RDiskResp;

#define RDISK_STATUS_OK	     0
#define RDISK_STATUS_ERROR   0xff

    //
    // Probing record
    //
typedef nku16_f RDiskInfo;

typedef struct RDiskProbe {
    RDiskDevId  devid;    // device ID
    RDiskInfo   info;     // device type & flags
    RDiskSector size[2];  // size in sectors ([0] - low; [1] - high)
} __attribute__ ((packed)) RDiskProbe;

    //
    // Types below match ide_xxx in Linux ide.h
    //
#define RDISK_TYPE_FLOPPY  0x00
#define RDISK_TYPE_TAPE    0x01
#define RDISK_TYPE_CDROM   0x05
#define RDISK_TYPE_OPTICAL 0x07
#define RDISK_TYPE_DISK    0x20 

#define RDISK_TYPE_MASK    0x3f
#define RDISK_TYPE(x)      ((x) & RDISK_TYPE_MASK) 

#define RDISK_FLAG_RO      0x0040
#define RDISK_FLAG_VIRT    0x0080
	// REMOVABLE MEDIA support
#define RDISK_FLAG_CHANGED 0x0100	// Media has changed
#define RDISK_FLAG_LOCKED  0x0200	// Media locked (lock in command)
#define RDISK_FLAG_LOEJ    0x0400	// Load/Eject flag (command)
#define RDISK_FLAG_START   0x0800	// Start/Stop unit (command)

#ifdef VBD_ATAPI
   //
   // ATAPI request support
   //
typedef struct RDiskAtapiSense {
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
} RDiskAtapiSense;

#define RDISK_ATAPI_PKT_SZ	12        // ATAPI cmd packet size
#define RDISK_ATAPI_REP_SZ	(64*1024) // ATAPI reply (max) size

typedef struct RDiskAtapiReq {
  nku8_f          cdb[RDISK_ATAPI_PKT_SZ];// ATAPI cmd. desc. bloc
  nku32_f         buflen;                 // reply buffer len
  int             status;                 // returned (ioctl) status
  RDiskAtapiSense sense;                  // returned sense info
} __attribute__ ((packed)) RDiskAtapiReq;
#endif	/* VBD_ATAPI */

#endif

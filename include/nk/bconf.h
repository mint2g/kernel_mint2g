/*
 ****************************************************************
 *
 *  Component: VLX nano-kernel boot interface
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
 *  #ident  "@(#)bconf.h 1.21     08/06/02 Red Bend"
 * 
 ****************************************************************
 */

#ifndef _NK_BOOT_BCONF_H
#define _NK_BOOT_BCONF_H

#include <asm/nk/bconf_f.h>
#include <nk/env.h>

/*
 * Memory bank descriptor
 */

typedef unsigned int BankType;

#define BANK_KSP	  0x01	/* must be installed at vaddr before entering
				   in the u-kernel */
#define BANK_DEVICE	  0x10	/* must be instantiated as a memory device */
#define BANK_DEVICE_READ  0x20	/* memory device can be read */
#define BANK_DEVICE_EXEC  0x40	/* memory device can be executed */
#define BANK_DEVICE_WRITE 0x80	/* memory device can be modified */

#define BANK_OS_MASK	  	0x1f00 /* Bank owner's OS number (0..31) */
#define BANK_OS_ID(t)	  	(((t) & BANK_OS_MASK) >> 8)
#define BANK_OS_NK	  	0x0000 /* Nano kernel owned bank */
#define BANK_OS_PRIMARY	  	0x0100 /* Primary OS owned bank  */
#define BANK_OS_SECONDARY 	0x0200 /* (first) Secondary owned bank */
#define BANK_OS_SHARED    	0x1f00 /* Bank shared by all OS */
#define BANK_ID(id)	  	((id) << 8)

#define BANK_OS_CHORUS          0x0000 /* Chorus OS */
#define BANK_OS_LINUX           0x2000 /* Linux  OS */
#define BANK_RAMDISK		0x4000 /* RAM Disk */
#define BANK_OSCTX		0x6000 /* OS contexts area */
#define BANK_PMEM		0x6000 /* persistant communication memory */
#define BANK_OS_NUCLEUS		0x8000 /* Nucleus OS */
#define BANK_OS_VXWORKS		0xa000 /* VxWorks */
#define BANK_OS_GENERIC		0xc000 /* Generic OS */
#define BANK_OS_VRTX		0xe000 /* VRTX */
#define BANK_OS_RTKE		0xe000 /* RTKE */
#define BANK_TYPE_MASK		0xe000 /* OS type from 0 to 7 */
#define BANK_TYPE(t)	        (((t) & BANK_TYPE_MASK) >> 13)
#define	BANK_TYPE_OS_CHORUS	BANK_TYPE(BANK_OS_CHORUS)
#define	BANK_TYPE_OS_LINUX	BANK_TYPE(BANK_OS_LINUX)
#define	BANK_TYPE_RAMDISK	BANK_TYPE(BANK_RAMDISK)
#define	BANK_TYPE_OSCTX		BANK_TYPE(BANK_OSCTX)
#define	BANK_TYPE_PMEM		BANK_TYPE(BANK_PMEM)
#define	BANK_TYPE_OS_NUCLEUS	BANK_TYPE(BANK_OS_NUCLEUS)
#define	BANK_TYPE_OS_VXWORKS	BANK_TYPE(BANK_OS_VXWORKS)
#define	BANK_TYPE_OS_GENERIC	BANK_TYPE(BANK_OS_GENERIC)
#define	BANK_TYPE_OS_VRTX	BANK_TYPE(BANK_OS_VRTX)
#define	BANK_TYPE_OS_RTKE	BANK_TYPE(BANK_OS_RTKE)

#define BANK_PRIO_MASK		0x1f /* OS Priority (0..31) */
#define BANK_PRIO(p)	  	(((p) & BANK_PRIO_MASK) << 16)
#define BANK_OS_PRIO(t)	  	(((t) >> 16) & BANK_PRIO_MASK)

#define BANK_CPU_MASK		0x1f /* CPU ID (0..31) */
#define BANK_CPU(c)	  	(((c) & BANK_CPU_MASK) << 21)
#define BANK_OS_CPU(t)	  	(((t) >> 21) & BANK_CPU_MASK)
#define BANK_CPU_PRIMARY  	(1 << 26) /* Primary OS bank on this CPU */

#define BANK_OS_MMU_OWNER	(1 << 29)

#define	BANK_IN_PLACE		(1 << 30)
#define	BANK_ROM		(1 << 31)

typedef struct BankDesc {

    char*    id;	/* name string */
    BankType type;	/* bank's type */
    char*    fs;        /* file system type */
    VmAddr   vaddr;	/* address required for the bank's image */
    VmSize   size;	/* bank's size (actually used size) */
    VmSize   cfgSize;	/* configured bank's size (max size) */

} BankDesc;

/*
 *  Binary Descriptor
 */

    /*
     *  Binary type attribute
     */

typedef unsigned char BinType;

#define BIN_BOOTCONF	0x80    /* system boot configuration */
#define BIN_REBOOT	0x81    /* reboot program */

	/* standalone binary types */

#define BIN_STANDALONE	0x10	/* indicates that the binary is standalone */
   
#define BIN_BOOTSTRAP	0x11    /* bootstrap program */
#define BIN_DBG_DRIVER	0x12    /* debugger communication driver */
#define BIN_DBG_AGENT	0x13    /* debugger agent */
#define BIN_ALIEN_RAW	0x14    /* Other stand-alone (GPOS kernel) */

        /* Kernel binary type */

#define BIN_KERNEL	0x20	/* indicates an OS kernel binary */

        /* actor binary types */

#define BIN_ACTOR	0x40	/* indicates that the binary is an actor */

#define BIN_DRIVER	0x41    /* driver */
#define BIN_SUPERVISOR	0x42    /* supervisor actor */
#define BIN_USER	0x43    /* user actor */
#define BIN_ALIEN_ACTOR 0x44    /* Alien OS */

    /*
     * Binary loading mode attribute
     */

typedef unsigned char BinMode;

#define BIN_DEBUGGED	    0x02    /* load as debugged */
#define BIN_STOPPED	    0x04    /* load as stopped */

typedef struct BinDesc {
    char*	 name;			/* binary symbolic name */
    int		 firstSeg;		/* first segment index */
    int		 lastSeg;		/* last segment index */
    VmAddr	 entry;		        /* binary entry point */
    BinType	 type;		        /* binary type */
    BinMode	 mode;		        /* binary loading mode */
    void*	 unused;
} BinDesc;

/*
 *  Binary's Segment Descriptor
 */

    /*
     *  Segment protection flags
     */

typedef unsigned short SegType;

#define SEG_EXEC	0x01    /* memory execution allowed */
#define SEG_READ	0x02    /* memory reads allowed */
#define SEG_WRITE	0x04    /* memory writes allowed */

#define SEG_XIP		0x0100  /* segment must be executed at its place in
				 * the system image */

typedef unsigned char SegSpace ;

#define SEG_KSP		0x01	/* belongs to the kernel initial space */
#define SEG_VIRTUAL	0x02	/* belongs to another (virtual) space */

typedef struct BinSegDesc {
    VmAddr	kaddr;		     /* segment image address */
    VmAddr	vaddr;		     /* execution address (from link editor) */
    VmSize	ksize;		     /* segment image size */
    VmSize	vsize;		     /* execution size */
    SegType	type;		     /* code or data */
    SegSpace	space;		     /* address space type */
} BinSegDesc;

/*
 *  Boot configuration
 */

struct BootConf;
struct RebootDesc;

typedef void (*BootConfEntry)(void*);
typedef void (*BootConfRebootEntry)(struct RebootDesc*, void*);
typedef void (*BootstrapEntry)(struct BootConf*, void*);
typedef void (*RebootEntry)(struct BootConf*, void*);

typedef struct BootConf {
    unsigned int   stamp;	    /* identification stamp */

    int            numBanks;        /* number of memory banks */
    BankDesc*      bankDesc;        /* bank descriptors */

    int		   numBins;	    /* number of binaries */
    BinDesc*	   binDesc;	    /* array of binary descriptors */

    int		   numSegs;         /* number of segments */
    BinSegDesc*    segDesc;	    /* array of segment descriptors */

    EnvDesc*	   env;		    /* environment descriptor */

    void*	   heapStart;	    /* heap memory: */
    void*	   heapCur;	    /*  occupied from heapStart to heapCur */
    void*	   heapLimit;	    /*  available from heapCur to heapLimit */

    BootConfRebootEntry rebootEntry;/* reboot entry */
    struct RebootDesc*  rebootDesc;	    /* reboot descriptor */

} BootConf;

extern BootConf* bootConf;

#endif

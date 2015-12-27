/*
 ****************************************************************
 *
 *  Component: VLX VMTD interface
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
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

#ifndef VMTD_H
#define VMTD_H

    /* The "ecclayout" field */
typedef struct NkDevMtdNandOobFree {
    nku32_f  offset;
    nku32_f  length;
} NkDevMtdNandOobFree;	/* 2*4 = 8 bytes */

// #define NK_DEV_MTD_NAND_OOB_FREE_SIZE	8

#define NK_DEV_MTD_MAX_OOBFREE_ENTRIES	32
#define NK_DEV_MTD_MAX_ECCPOS_ENTRIES	448

typedef struct NkDevMtdNandEccLayout {
    nku32_f              eccbytes;
    nku32_f              eccpos [NK_DEV_MTD_MAX_ECCPOS_ENTRIES];
    nku32_f              oobavail;
    NkDevMtdNandOobFree  oobfree [NK_DEV_MTD_MAX_OOBFREE_ENTRIES];
} NkDevMtdNandEccLayout;	/* (1+64+1+(8*2))*4 = 328 bytes (41*8) */

// #define NK_DEV_MTD_NAND_ECC_LAYOUT_SIZE	328

typedef struct NkDevMtdOtpInfo {
    nku32_f	start;
    nku32_f	length;
    nku32_f	locked;
} NkDevMtdOtpInfo;	/* 3*4 = 12 bytes (1.5*8) */

#define NK_DEV_MTD_OTP_INFO_SIZE	12

typedef struct NkDevMtdEccStats {
    nku32_f corrected;	/* number of corrected bits */
    nku32_f failed;	/* number of uncorrectable errors */
    nku32_f badblocks;	/* number of bad blocks in this partition */
    nku32_f bbtblocks;	/* number of blocks reserved for bad block tables */
} NkDevMtdEccStats;	/* 4*4 = 16 bytes (2*8) */

#define NK_DEV_MTD_ECC_STATS_SIZE	16

#define NK_DEV_MTD_NAME_LIMIT	31

typedef struct NkDevMtd {
    nku32_f version;	/* sizeof of the structure, serves as version */
	/* Offset 4*1 = 4 bytes */
    nku8_f  type;	/* MTD type, listed below */
    nku8_f  name [NK_DEV_MTD_NAME_LIMIT];	/* Visible in /proc/mtd */
	/* Offset 4*9 = 36 bytes */
    nku32_f flags;	/* Flags bitmap, described below */
	/* Offset 4*10 = 40 bytes */
    nku64_f size;	/* Total size of the MTD in bytes */
	/* Offset 4*12 = 48 bytes */
	/*
	 * "Major" erase size for the device. Naive users may take this
	 * to be the only erase size available, or may use the more detailed
	 * information below if they desire
	 */
    nku32_f erasesize;
	/*
	 * Minimal writable flash unit size. In case of NOR flash it is 1 (even
	 * though individual bits can be cleared), in case of NAND flash it is
	 * one NAND page (or half, or one-fourths of it), in case of ECC-ed NOR
	 * it is of ECC block size, etc. It is illegal to have writesize = 0.
	 */
    nku32_f writesize;

    nku32_f oobsize;	/* Amount of OOB data per block (e.g. 16) */
    nku32_f oobavail;	/* Available OOB bytes per block */
	/* Offset 4*16 = 64 bytes */

	/*
	 * If erasesize is a power of 2 then the shift is stored in
	 * erasesize_shift, otherwise erasesize_shift is zero.
	 */
    nku16_f erasesize_shift;
    nku16_f writesize_shift;
	/* Masks based on erasesize_shift and writesize_shift */
    nku32_f erasesize_mask;
    nku32_f writesize_mask;

    nku32_f index;	/* MTD device index on the back-end side */
	/* Offset 4*20 = 80 bytes */

    NkDevMtdNandEccLayout	ecclayout;
	/* Offset 4*102 = 408 bytes */
    nku32_f			ecclayoutexists;
	/*
	 * Data for variable erase regions. If numeraseregions is zero,
	 * it means that the whole device has erasesize as given above.
	 * Otherwise, NkDevMtdEraseRegionInfo has to be used.
	 */
    nku32_f		numeraseregions;
    nku32_f		subpage_sft;	/* Subpage shift (NAND) */
    nku32_f		unused;		/* preserves 64-bit alignment */
	/* Offset 4*106 = 424 bytes */

    NkDevMtdEccStats	ecc_stats;
	/* Offset 4*110 = 440 bytes */

    nku64_f		available_functions;	/* Bitmap of available calls */

#if 0
	/* Present in 2.6.30, not required */
    struct backing_dev_info backing_dev_info;
#endif
} NkDevMtd;	/* 4*112 = 448 bytes (56*8) */

#define NK_DEV_MTD_SIZE	448

#define NK_DEV_MTD_FIELD_MTD			1
#define NK_DEV_MTD_FIELD_ERASE_REGION_INFO	2

typedef struct NkDevMtdGetNextMtd {
    nku32_f	type;
    nku32_f	len;
} NkDevMtdGetNextMtd;

    /* Values "for "type" field */
#define NK_DEV_MTD_ABSENT		0
#define NK_DEV_MTD_RAM			1
#define NK_DEV_MTD_ROM			2
#define NK_DEV_MTD_NORFLASH		3
#define NK_DEV_MTD_NANDFLASH		4
#define NK_DEV_MTD_DATAFLASH		6
#define NK_DEV_MTD_UBIVOLUME		7

#define NK_DEV_MTD_NAMES \
    "ABSENT", "RAM", "ROM", "NORFLASH", \
    "NANDFLASH", "5?", "DATAFLASH", "UBIVOLUME"

    /* Values for "flags" field */
#define NK_DEV_MTD_WRITEABLE	    0x400   /* Device is writable */
#define NK_DEV_MTD_BIT_WRITEABLE    0x800   /* Single bits can be flipped */
#define NK_DEV_MTD_NO_ERASE	    0x1000  /* No erase necessary */

typedef struct NkDevMtdEraseRegionInfo {
    nku64_f offset;	/* At which this region starts, from MTD beginning */
    nku32_f erasesize;	/* For this region */
    nku32_f numblocks;	/* Number of blocks of erasesize in this region */
} NkDevMtdEraseRegionInfo;	/* 4*4 = 16 bytes (2*8) */

#define NK_DEV_MTD_ERASE_REGION_INFO_SIZE	16

    /* Also bits in available_functions */
#define NK_DEV_MTD_FUNC_UNUSED			0
#define NK_DEV_MTD_FUNC_ERASE			1
#define NK_DEV_MTD_FUNC_POINT			2
#define NK_DEV_MTD_FUNC_UNPOINT			3
#define NK_DEV_MTD_FUNC_READ			4
#define NK_DEV_MTD_FUNC_WRITE			5
#define NK_DEV_MTD_FUNC_PANIC_WRITE		6
#define NK_DEV_MTD_FUNC_READ_OOB		7
#define NK_DEV_MTD_FUNC_WRITE_OOB		8
#define NK_DEV_MTD_FUNC_GET_FACT_PROT_INFO	9
#define NK_DEV_MTD_FUNC_READ_FACT_PROT_REG	10
#define NK_DEV_MTD_FUNC_GET_USER_PROT_INFO	11
#define NK_DEV_MTD_FUNC_READ_USER_PROT_REG	12
#define NK_DEV_MTD_FUNC_WRITE_USER_PROT_REG	13
#define NK_DEV_MTD_FUNC_LOCK_USER_PROT_REG	14
#define NK_DEV_MTD_FUNC_WRITEV			15
#define NK_DEV_MTD_FUNC_SYNC			16
#define NK_DEV_MTD_FUNC_LOCK			17
#define NK_DEV_MTD_FUNC_UNLOCK			18
#define NK_DEV_MTD_FUNC_SUSPEND			19
#define NK_DEV_MTD_FUNC_RESUME			20
#define NK_DEV_MTD_FUNC_BLOCK_ISBAD		21
#define NK_DEV_MTD_FUNC_BLOCK_MARKBAD		22
#define NK_DEV_MTD_FUNC_GET_DEVICE		23
#define NK_DEV_MTD_FUNC_PUT_DEVICE		24
#define NK_DEV_MTD_FUNC_GET_UNMAPPED_AREA	25
    /* VLX-specific functions */
#define NK_DEV_MTD_FUNC_GET_NEXT_MTD		26
#define NK_DEV_MTD_FUNC_MTD_CHANGES		27
#define NK_DEV_MTD_FUNC_ERASE_FINISHED		28
#define NK_DEV_MTD_FUNC_BLOCKS_AREBAD		29
#define NK_DEV_MTD_FUNC_READ_MANY_OOBS		30
#define NK_DEV_MTD_FUNC_MAX			31	/* last+1 */

#define NK_DEV_MTD_FUNC_IS_STANDARD(x)	((x) < NK_DEV_MTD_FUNC_GET_NEXT_MTD)

#define NK_DEV_MTD_FUNC_NAMES \
    "UNUSED", "ERASE", "POINT", "UNPOINT", \
    "READ", "WRITE", "PANIC_WRITE", "READ_OOB", \
    "WRITE_OOB", "GET_FACT_PROT_INFO", "READ_FACT_PROT_REG", \
    "GET_USER_PROT_INFO", \
    "READ_USER_PROT_REG", "WRITE_USER_PROT_REG", "LOCK_USER_PROT_REG", \
    "WRITEV", \
    "SYNC", "LOCK", "UNLOCK", "SUSPEND", \
    "RESUME", "BLOCK_ISBAD", "BLOCK_MARKBAD", "GET_DEVICE", \
    "PUT_DEVICE", "GET_UNMAPPED_AREA", "GET_NEXT_MTD", "MTD_CHANGES", \
    "ERASE_FINISHED", "BLOCKS_AREBAD", "READ_MANY_OOBS"

    /* Bits for the "flags" field */
#define NK_DEV_MTD_FLAGS_REQUEST	0x1
#define NK_DEV_MTD_FLAGS_REPLY		0x2
#define NK_DEV_MTD_FLAGS_NOTIFICATION	0x4

typedef struct NkDevMtdRequest {
    nku16_f function;	/* NK_DEV_MTD_FUNC_... */
    nku16_f flags;	/* NK_DEV_MTD_FLAGS_... */
    nku32_f index;	/* remote MTD device identifier */
    nku64_f cookie;	/* opaque for backend */

    nku64_f offset;
    nku64_f size;
    nku32_f offset2;
    nku32_f size2;

    nku64_f addr;	/* also erase_cookie */
    nku32_f dataOffset;	/* offset into communications area */
    nku32_f mode;	/* NK_DEV_MTD_OOB_... */
} NkDevMtdRequest;	/* 14*4 = 56 bytes (7*8) */

#define NK_DEV_MTD_REQUEST_SIZE	56

typedef struct NkDevMtdReply {
    nku16_f function;	/* NK_DEV_MTD_FUNC_... */
    nku16_f flags;	/* NK_DEV_MTD_FLAGS_... */
    nku32_f index;	/* remote MTD device identifier */
    nku64_f cookie;	/* NkDevMtdRequest::cookie */

    nku32_f retcode;	/* 0 if success, Linux errno code otherwise */
    nku32_f retsize;
    nku32_f retsize2;
    nku32_f reserved;

    nku64_f addr;
    nku64_f erase_cookie;
} NkDevMtdReply;	/* 10*4 = 40 bytes (5*8) */

#define NK_DEV_MTD_REPLY_SIZE	48

typedef union NkDevMtdMsg {
    NkDevMtdRequest	req;
    NkDevMtdReply	reply;
} NkDevMtdMsg;		/* 14*4 = 56 bytes (7*8) */

#define NK_DEV_MTD_MSG_SIZE	56

    /* "mode" for Write_OOB */
#define NK_DEV_MTD_OOB_PLACE	0
#define NK_DEV_MTD_OOB_AUTO	1
#define NK_DEV_MTD_OOB_RAW	2

#define NK_DEV_MTD_OOB_NAMES	"PLACE", "AUTO", "RAW"

#endif

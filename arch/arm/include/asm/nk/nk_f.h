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
 *  #ident  "@(#)nk_f.h 1.5     08/07/04 Red Bend"
 *
 *  Contributor(s):
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *    Guennadi Maslov (guennadi.maslov@redbend.com)
 *
 ****************************************************************
 */

#ifndef _NK_NK_F_H
#define _NK_NK_F_H

    /*
     * Nano-kernel OS IDs.
     */
#define NK_OS_NKERN	   0	/* index #0 is for nano-kernel itself */
#define NK_OS_PRIM	   1	/* index #1 is for primary OS */
#define NK_OS_LIMIT	  32	/* theoretical limit of supported OSes */

#define NK_OS_ANON	 (-1)	/* if NkOsId is used as owner tag, */
				/* this special value says: */
				/* owner is anonymous, or no owner */

    /*
     * Limit of the XIRQ number used to virtualize HW IRQs
     */
#define NK_HW_XIRQ_LIMIT 512

#if !defined(__ASM__) && !defined(__ASSEMBLY__)

typedef  unsigned char	     nku8_f;
typedef  unsigned short	     nku16_f;
typedef  unsigned int	     nku32_f;
typedef  unsigned long long  nku64_f;

typedef  nku32_f  NkPhAddr;	/* physical address */
typedef  nku32_f  NkPhSize;	/* physical size */

typedef  nku32_f  NkOsId;	/* OS identifier */
typedef  nku32_f  NkOsMask;	/* OS identifiers bit-mask */

typedef  nku32_f  NkXIrq;	/* cross interrupt request */
typedef  nku32_f  NkVex;	/* virtual exception number */

typedef  nku32_f  NkCpuId;	/* CPU identifier */
typedef  nku32_f  NkCpuMask;	/* CPU identifier bit-mask */

typedef struct NkSchedParams {  /* scheduling parameters */
    unsigned int fg_prio;	/* foreground priority of secondary OS */
    unsigned int bg_prio;       /* background priority of secondary OS */
    unsigned int quantum;       /* quantum in usecs */
} NkSchedParams;

    /* This is used by Performance Monitoring */
typedef nku64_f NkTime;
typedef nku32_f NkFreq;
typedef nku32_f NkPmonState;

typedef nku32_f NkPResourceId;	/* VLINK persistent resource identifier */

typedef nku32_f NkBool;		/* boolean value: true/false (1/0) */

        /* NkBalloonControl operations: */
#define	NK_BALLOON_STATUS	0	/* - query balloon status */
#define	NK_BALLOON_ALLOC	1	/* - allocate balloon memory */
#define	NK_BALLOON_FREE		2	/* - release balloon memory */

	/* Memory balloon status */
#define	NK_BALLOON_DISABLED	0	
#define	NK_BALLOON_ENABLED	1

    /*
     * Property attributes (permissions and name length)
     */
typedef nku32_f NkPropAttr;

#define	NK_PROP_ATTR_NLEN(x)	((x) & 0xffff)
#define	NK_PROP_ATTR_PERM(x)	((x) & (0x7 << 29))
#define	NK_PROP_ATTR_SET	(1 << 31)
#define	NK_PROP_ATTR_GET	(1 << 30)
#define	NK_PROP_ATTR_NOTIFY	(1 << 29)

    /*
     * The NK_PROP_BUSY error is returned from NkPropGet/Set when
     * a property update is in progress, in other words, when a contention
     * with another NkPropSet call is detected. As a common rule, receiving
     * such an error, the guest OS has to try again after some reasonable
     * delay. An important exception from the above rule is a NkPropGet call
     * issued in the SYSCONF interrupt handler as a reaction on a property
     * update event. Upon receiving such an error in the SYSCONF handler,
     * the software should simply return from the interrupt handler.
     * VLX guarantees that a SYSCONF interrupt will be sent again once the
     * update in progress is finished.
     */
#define	NK_PROP_BUSY		(-1)

    /*
     * The NK_PROP_PERMISSION error is returned from NkPropGet/Set when
     * the operation is disabled for this VM by the property permissions.
     */
#define	NK_PROP_PERMISSION	(-2)
  
    /*
     * The NK_PROP_UNKNOWN error is returned from NkPropGet/Set/Enum when
     * the property name is not found in the VLX repository.
     */
#define	NK_PROP_UNKNOWN		(-3)

    /*
     * The NK_PROP_ERROR error is returned from NkPropGet/Set/Enum when
     * a fatal error detected.
     */
#define	NK_PROP_ERROR		(-4)

#endif

#endif

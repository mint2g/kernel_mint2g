/*
 ****************************************************************
 *
 *  Component: VLX performance monitoring driver
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
 *    Chi Dat Truong (chidat.truong@redbend.com)
 *
 ****************************************************************
 */

#ifndef OSWARE_PERFMON_H
#define OSWARE_PERFMON_H

//#include <osware/osware.h>
#include <nk/nkern.h>
#include <asm/nk/f_nk.h>
#include <asm/nkern.h>

#ifdef __c6x__
#define os_ctx nkctx
#endif

#define PMON_CPU_STATS_SIZE          32
#define PMON_MISC_STATS_SIZE         8

typedef struct NkPmonRecord {
    NkTime         stamp;    // time stamp
    NkPmonState    state;    // new state transition
    NkPhAddr       cookie;   // guest specific info
} NkPmonRecord;

typedef struct NkPmonCpuStats {
    NkTime  startstamp;                      // statistic start time stamp
    NkTime  laststamp;                       // statistic last time stamp
    NkTime  cpustats[PMON_CPU_STATS_SIZE];   // statistic array for 32 OSes
    NkTime  miscstats[PMON_MISC_STATS_SIZE]; // miscellaneous statistic array
} NkPmonCpuStats;

typedef struct NkPmonBuffer {    // circular buffer with header
    nku32_f        first;      // index of the start of the record buffer
    nku32_f        last;       // index of the end of the record buffer
    nku32_f        length;     // the length of the record array (number of elements)
    nku32_f        padding;    // for 64 bit alignment
    NkPmonRecord   data[0];     // array of NkPmonRecord
} NkPmonBuffer;

#define PMON_GET_RECORD_LENGTH(bufferSize) \
            ( ((bufferSize) - sizeof(NkPmonBuffer)) / sizeof(NkPmonRecord) )

/* User can define sub-state events for record for this OS */
//#define    PMON_EVENT_****		0x1

#endif

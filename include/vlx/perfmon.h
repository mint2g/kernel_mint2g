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

#ifndef PERFMON_H
#define PERFMON_H

#define PMON_MAX_SUPPORTED_OS        32    // maximum of supported OS
#define PMON_MISC_STATS_SIZE          8    // number of miscellaneous statistics
#define PMON_SUPPORTED_SERVICE        2    // number of service supported

/*
 * CONTROL COMMANDS
 */
#define CMD_SWITCH          "PMON_CTRL_SWITCH"
#define CMD_ONESHOT_START   "PMON_CTRL_ONESHOT_START"
#define CMD_STATS_START     "PMON_CTRL_STATS_START"
#define CMD_START           "PMON_CTRL_START"
#define CMD_STOP            "PMON_CTRL_STOP"
#define CMD_IS_WORKING      "PMON_CTRL_IS_WORKING"

/*
 * User data interface
 */

typedef struct PmonTimerInfo {
    unsigned long       freq;
    unsigned long long  period;
} PmonTimerInfo;

typedef struct PmonCpuStats {
    unsigned long long    startstamp;    // statistic start time stamp
    unsigned long long    laststamp;     // statistic last time stamp
    unsigned long long    cpustats[PMON_MAX_SUPPORTED_OS];  // statistic array for 32 OSes
    unsigned long long    miscstats[PMON_MISC_STATS_SIZE];  // miscellaneous statistic array
} PmonCpuStats;

typedef struct PmonSysInfo {
    char             version[8];    // string of version number
    unsigned long    last_os_id;    // last os id number
    PmonTimerInfo    timer;         // timer information (frequency, ...)
    unsigned long    max_records;   // maximum of record elements
} PmonSysInfo;

typedef struct PmonRecordData {
    unsigned long long    stamp;    // time stamp
    unsigned long         state;    // new state transition
    unsigned long         cookie;   // guest specific info
} PmonRecordData;

#endif

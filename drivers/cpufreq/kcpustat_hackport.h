/*
 * Copyright (C) 2016 Psych Half, <psych.half@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 */

#include <linux/types.h>
#include <linux/err.h>
#include <linux/kernel_stat.h>
#include <asm-generic/cputime.h>

/* Hackport newer CPU accounting API to 3.0 kernels for using newer governors.
 *
 * The per cpu variable cpustat holds the time as struct members.
 * Since then arrays were used in upstream, to make calculations easier.
 * In here turn them into function call. This might add some overhead.
 * 
 * Requires the following changes in the governors.
 *
 * Add:
 * #include "kcpustat_hackport.h"
 *  
 * Change:
 * kcpustat_cpu(cpu).cpustat[CPUTIME_XXX] to
 * kcpustat_calc(cpu,CPUTIME_XXX)
 *
 */ 

enum cpu_usage_stats_hp {
	CPUTIME_USER,
	CPUTIME_NICE,
	CPUTIME_SYSTEM,
	CPUTIME_SOFTIRQ,
	CPUTIME_IRQ,
	CPUTIME_IDLE,
	CPUTIME_IOWAIT,
	CPUTIME_STEAL,
	CPUTIME_GUEST,
	CPUTIME_GUEST_NICE,
	NR_STATS,
};

static inline u64 kcpustat_calc(u32 c, u32 cpu_stat )  {
   switch (cpu_stat) {
   case CPUTIME_USER:
    return kstat_cpu(c).cpustat.user;
   case CPUTIME_NICE:
	return kstat_cpu(c).cpustat.nice;
   case CPUTIME_SYSTEM:
	return kstat_cpu(c).cpustat.system;
   case CPUTIME_IDLE:
	return kstat_cpu(c).cpustat.idle;
   case CPUTIME_IOWAIT:
	return kstat_cpu(c).cpustat.iowait;
   case CPUTIME_IRQ:
	return kstat_cpu(c).cpustat.irq;
   case CPUTIME_SOFTIRQ:
  	return kstat_cpu(c).cpustat.softirq;
   case CPUTIME_STEAL:
	return kstat_cpu(c).cpustat.steal;
   case CPUTIME_GUEST:
	return kstat_cpu(c).cpustat.guest;
   case CPUTIME_GUEST_NICE:
	return kstat_cpu(c).cpustat.guest_nice;
   }
   return -EINVAL;
}   


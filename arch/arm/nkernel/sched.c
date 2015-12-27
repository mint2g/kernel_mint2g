/*=================================================*/
/* !!!      THIS IS EXPERIMENTAL SOFTWARE       !!!*/
/* !!!    DO NOT ENABLE UNLESS YOU ARE SURE     !!!*/
/* !!!          OF WHAT YOU ARE DOING           !!!*/ 
/*=================================================*/
#include <linux/sched.h>
#include "asm/nkern.h"
#include "vlx/perfmon.h"
/* Locally added because we do not have access to these
 * values from the guest.
 */
#define NK_CPU_MAX 2
#define NK_OS_MAX 4
static inline unsigned nk_vm_prio(unsigned vm)
{
	/* vm priority order must reflect VLX configuration file */
	static const unsigned vm_prio[NK_OS_MAX+1] = {255, 0, 1, 3, 4};
	return vm_prio[vm];
}
static inline unsigned nk_vm_on_pcpu(unsigned vm, unsigned pcpu)
{
	/* particular case: VM runs on all pcpus */
	static const unsigned vm_pcpu[NK_CPU_MAX] = {[0 ... NK_CPU_MAX-1] = 1};
	(void)vm;
	return vm_pcpu[pcpu];
}
static inline unsigned nk_on_which_pcpu(unsigned vm, unsigned vcpu)
{
	/* particular case: no migration so identity vCPU/pCPU mapping */
	(void)vm;
	return vcpu;
}
/**
 * nk_vcpu_power - Calculates the power weight of the vCPU provided
 * as argument. Power weight ranges in [SCHED_POWER_SCALE, (2^32)-1].
 *
 * @vcpu: the vCPU number in [0, NK_CPU_MAX].
 */
static inline unsigned long nk_vcpu_power(int vcpu)
{
	/* storage for nkernel pCPUs statistics in input */
	static PmonCpuStats cpu_stats[2][NK_CPU_MAX];
	static unsigned curr_cpu_stats = 0;
	/* storage for algo. calculation */
	unsigned long pcpu_rtime[NK_CPU_MAX], pcpu_max_rtime;
	unsigned long vm_max_rtime[NK_CPU_MAX], vm_min_max_rtime;
	/* considered vm */
	const unsigned curr_vm = VCPU()->id;
	unsigned pcpu;

	/* collect pCPUs statistics */
	for (pcpu=0; pcpu<NK_CPU_MAX; pcpu++)
		VCPU()->pmonops.control(os_ctx,
					PMON_CONTROL_SET(PMON_CPUSTATS_START,
							 pcpu),
					__pa(&cpu_stats[curr_cpu_stats][pcpu]));
	/* we are interested in instantaneous stats */
	curr_cpu_stats = (curr_cpu_stats + 1) % 2;
	/*
	 * 1) calculate "pcpu_rtime[pcpu]": the running time for each pCPU
	 * 2) calculate "vm_max_rtime[pcpu]": the VM maximal possible running
	 *    time on each pCPU.
	 * 3) Find "pcpu_max_rtime": the maximal value for "vm_max_rtime[pcpu]"
	 *    on all pCPUs. It wil be used in the next step for scaling.
	 */
	pcpu_max_rtime = 0;
	for (pcpu=0; pcpu<NK_CPU_MAX; pcpu++) {
		unsigned vm;
		pcpu_rtime[pcpu] = vm_max_rtime[pcpu] = 0;
		/* skip pCPUs not hosting the considered vm */
		if (!nk_vm_on_pcpu(curr_vm, pcpu)) continue;
		for (vm=0; vm<=NK_OS_MAX; vm++) {
			/* times shall fit a 32 bit integer */
			unsigned long t = (unsigned long)
				cpu_stats[curr_cpu_stats][pcpu].cpustats[vm];
			pcpu_rtime[pcpu] += t;
			if (nk_vm_prio(vm) >= nk_vm_prio(curr_vm))
				vm_max_rtime[pcpu] += t;
		}
		if (pcpu_rtime[pcpu] > pcpu_max_rtime)
			pcpu_max_rtime = pcpu_rtime[pcpu];
	}
	/*
	 * 1) scale "vm_max_rtime[pcpu]" according to "pcpu_max_rtime",
	 *    different pCPUs may have different sampling periods
	 *    these times must be normalized before calculating "vm_min_max_rtime"
	 * 2) Find "vm_min_max_rtime": the minimal value for "vm_max_rtime[pcpu]"
	 *    on all pCPUs. This vCPU will be assigned the power weight 1024.
	 */
	vm_min_max_rtime = ~0;
	for (pcpu=0; pcpu<NK_CPU_MAX; pcpu++) {
		/* skip cpu with null running time */
		if (!pcpu_rtime[pcpu]) continue;
		vm_max_rtime[pcpu] *= pcpu_max_rtime / pcpu_rtime[pcpu];
		/* do not accept null value: force to 1 */
		if (!vm_max_rtime[pcpu])
			vm_max_rtime[pcpu] = 1;
		if (vm_max_rtime[pcpu] < vm_min_max_rtime)
			vm_min_max_rtime = vm_max_rtime[pcpu];
	}
	/* perform vCPU->pCPU resolution */
	pcpu = nk_on_which_pcpu(curr_vm, vcpu);
	/* return the power weight for the requested vCPU */
	return  vm_max_rtime[pcpu] * SCHED_POWER_SCALE / vm_min_max_rtime;
}

unsigned long arch_scale_freq_power(struct sched_domain *sd, int cpu)
{
	return nk_vcpu_power(cpu);
}

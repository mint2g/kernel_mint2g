/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/bitops.h>
#include <mach/hardware.h>
#include <mach/regulator.h>

#define DELTA 				msecs_to_jiffies(1000)
#define FREQ_TABLE_ENTRY		(4)

/*
 *   Cpu freqency is not be scaled yet, because of reasons of stablily.
 *   But we still define CONFIG_CPU_FREQ for some APKs, they will
 *display BogoMIPS instead of the real cpu frequency if CONFIG_CPU_FREQ
 *is not be defined
 */
static int cpufreq_bypass = 1;

/*
 *  sc8810+ indicator
 */
static int sc8810_plus = 0;

struct sprd_dvfs_table {
	unsigned long  clk_mcu_mhz;
	unsigned long  vdd_mcu_mv;
};

static struct sprd_dvfs_table sc8810g_dvfs_table[] = {
	[0] = { 1000000 , 1200000 }, /* 1000,000KHz,  1200mv */
	[1] = { 600000 , 1100000 },  /* 600,000KHz,  1100mv */
	[2] = { 400000 , 1100000 },  /* 400,000KHz,  1100mv */
};

static struct sprd_dvfs_table sc8810g_plus_dvfs_table[] = {
	[0] = { 1200000 , 1300000 }, /* 1200,000KHz,  1300mv */
	[1] = { 1000000 , 1200000 }, /* 1000,000KHz,  1200mv */
	[2] = { 600000 , 1100000 },  /* 600,000KHz,  1100mv */
};
//modified by xing wei for mint
struct cpufreq_frequency_table sc8810g_freq_table[FREQ_TABLE_ENTRY];
extern int global_cpufreq_min_limit;
extern int global_cpufreq_max_limit;
extern spinlock_t  g_cpufreq_lock;
//end //modified by xing wei
enum scalable_cpus {
	CPU0 = 0,
};

struct scalable {
	int 				cpu;
	struct clk			*clk;
	struct regulator		*vdd;
	struct cpufreq_frequency_table	*freq_tbl;
	struct sprd_dvfs_table		*dvfs_tbl;
};

struct scalable scalable_sc8810[] = {
	[CPU0] = {
		.cpu		= CPU0,
		.freq_tbl	= sc8810g_freq_table,
		.dvfs_tbl	= sc8810g_dvfs_table,
	},
};

struct sprd_dvfs_table current_cfg[] = {
	[CPU0] = {
		.clk_mcu_mhz = 0,
		.vdd_mcu_mv = 0,
	},
};
static unsigned int last_time[NR_CPUS];


struct clock_state {
	struct sprd_dvfs_table  current_para;
	struct mutex			lock;
}drv_state;

#ifdef CONFIG_SMP
struct cpufreq_work_struct {
	struct work_struct work;
	struct cpufreq_policy *policy;
	struct completion complete;
	unsigned int index;
	int frequency;
	int status;
};

static DEFINE_PER_CPU(struct cpufreq_work_struct, cpufreq_work);
static struct workqueue_struct *sprd_cpufreq_wq;
#endif

struct cpufreq_suspend_t {
	struct mutex suspend_mutex;
	int device_suspended;
};

static DEFINE_PER_CPU(struct cpufreq_suspend_t, cpufreq_suspend);


#ifdef CONFIG_SMP
static void set_cpu_work(struct work_struct *work)
{
	struct cpufreq_work_struct *cpu_work =
		container_of(work, struct cpufreq_work_struct, work);

	cpu_work->status = set_cpu_freq(cpu_work->policy, cpu_work->frequency);
	complete(&cpu_work->complete);
}
#endif

/*@return: Hz*/
static unsigned long cpu_clk_get_rate(int cpu){
	struct clk *mcu_clk = NULL;
	unsigned long clk_rate = 0;
	
	mcu_clk = scalable_sc8810[cpu].clk;
	clk_rate = clk_get_rate(mcu_clk);

	if(clk_rate < 0){
		pr_err("!!!%s cpu%u frequency is %lu\n", __func__, cpu, clk_rate);
	}
	return clk_rate;
}

static int set_mcu_vdd(int cpu, unsigned long vdd_mcu_uv){

	int ret = 0;
	struct regulator *vdd = scalable_sc8810[cpu].vdd;
	if(vdd)
		ret = regulator_set_voltage(vdd, vdd_mcu_uv, vdd_mcu_uv);
	else
		pr_err("!!!! %s,  no vdd !!!!\n", __func__);
	if(ret){
		pr_err("!!!! %s, set voltage error:%d !!!!\n", __func__, ret);
		return ret;
	}	
    return ret;

}

static int set_mcu_freq(int cpu, unsigned long mcu_freq_hz){
	int ret;
	unsigned long freq_mcu_hz = mcu_freq_hz * 1000;
	struct clk *clk = scalable_sc8810[cpu].clk;
	ret = clk_set_rate(clk, freq_mcu_hz);
	if(ret){
		pr_err("!!!! %s, clk_set_rate:%d !!!!\n", __func__, ret);
	}
    return ret;
}

static int cpu_set_freq_vdd(struct cpufreq_policy *policy, unsigned long mcu_clk, unsigned long mcu_vdd)
{
	unsigned int ret;
	unsigned long cpu = policy->cpu;
	struct clk *clk = scalable_sc8810[cpu].clk;
	//unsigned long cur_clk = clk_get_rate(clk) / 1000;
	unsigned long cur_clk = current_cfg[cpu].clk_mcu_mhz;

	if(mcu_clk > cur_clk){
	//	ret = set_mcu_vdd(cpu, mcu_vdd);
		ret = set_mcu_freq(cpu, mcu_clk);
	}
	if(mcu_clk < cur_clk){
		ret = set_mcu_freq(cpu, mcu_clk);
	//	ret = set_mcu_vdd(cpu, mcu_vdd);
	}

	current_cfg[cpu].clk_mcu_mhz = mcu_clk;
	current_cfg[cpu].vdd_mcu_mv  = mcu_vdd;

	return ret;
}


static int sprd_cpufreq_set_rate(struct cpufreq_policy *policy, int index){
	int ret = 0;
	if(cpufreq_bypass)
		return ret;

	struct cpufreq_freqs freqs;
	struct sprd_dvfs_table *dvfs_tbl = scalable_sc8810[policy->cpu].dvfs_tbl;
	unsigned int new_freq = dvfs_tbl[index].clk_mcu_mhz;
	unsigned int new_vdd = dvfs_tbl[index].vdd_mcu_mv;
	freqs.old = policy->cur;
	freqs.new = new_freq;
	freqs.cpu = policy->cpu;
	if(new_freq == current_cfg[policy->cpu].clk_mcu_mhz){
		return 0;
	}

	if(time_before(jiffies, last_time[policy->cpu]+DELTA)    &&
		new_freq < current_cfg[policy->cpu].clk_mcu_mhz  ){
		printk("%s, set rate in 1s(this_time:%u, last_time:%u), skip\n",
					__func__, jiffies, last_time[policy->cpu] );
		return ret;
	}
	printk("%s, old_freq:%lu KHz, old_vdd:%lu uv  \n", __func__,
			current_cfg[policy->cpu].clk_mcu_mhz, current_cfg[policy->cpu].vdd_mcu_mv);

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	ret = cpu_set_freq_vdd(policy, new_freq, new_vdd);
	if (!ret){
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
		policy->cur = new_freq;
		current_cfg[policy->cpu].clk_mcu_mhz = new_freq;
		printk("%s, new_freq:%lu KHz, new_vdd:%lu uv \n", __func__,
				current_cfg[policy->cpu].clk_mcu_mhz, current_cfg[policy->cpu].vdd_mcu_mv);
	}
	if(policy->cur == policy->max){
		last_time[policy->cpu] = jiffies;
	}
	return ret;

}

static int sc8810_is_plus(int cpu){
	int cpu_clk = 0;
	int sc8810_is_plus = 0;

	cpu_clk = cpu_clk_get_rate(cpu);
	/*
	 * sc8810+ runs at 1.2GHz
	 */
	if(cpu_clk == 1200000000)
		sc8810_is_plus =  1;

	return sc8810_is_plus;
}

static int sc8810_cpufreq_table_init( void ){
	int cnt;
	int cpu = 0;

	scalable_sc8810[cpu].cpu = cpu;
	scalable_sc8810[cpu].clk = clk_get(NULL, "mpll_ck");
	scalable_sc8810[cpu].vdd = regulator_get(NULL, "VDDARM");
	if( scalable_sc8810[cpu].clk == NULL ||
		scalable_sc8810[cpu].vdd == NULL ){
		pr_err("%s, cpu:%d, clk:%p, vdd:%p\n", __func__, cpu,
				scalable_sc8810[cpu].clk, scalable_sc8810[cpu].vdd);
		return -1;
	}

	if (sc8810g_freq_table == NULL) {
		printk(" cpufreq: No frequency information for this CPU\n");
		return -1;
	}

	sc8810_plus = sc8810_is_plus(cpu);
	for (cnt = 0; cnt < FREQ_TABLE_ENTRY-1; cnt++) {
		sc8810g_freq_table[cnt].index = cnt;
		if(!sc8810_plus)
			sc8810g_freq_table[cnt].frequency = sc8810g_dvfs_table[cnt].clk_mcu_mhz;
		else
			sc8810g_freq_table[cnt].frequency = sc8810g_plus_dvfs_table[cnt].clk_mcu_mhz;
	}
	sc8810g_freq_table[cnt].index = cnt;
	sc8810g_freq_table[cnt].frequency = CPUFREQ_TABLE_END;

	scalable_sc8810[cpu].freq_tbl = sc8810g_freq_table;
	if(sc8810_plus)
		scalable_sc8810[cpu].dvfs_tbl = sc8810g_plus_dvfs_table;


	for (cnt = 0; cnt < FREQ_TABLE_ENTRY; cnt++) {
		pr_debug("%s, scalable_sc8810[cpu].freq_tbl[%d].index:%d\n", __func__, cnt,
				scalable_sc8810[cpu].freq_tbl[cnt].index);
		pr_debug("%s, scalable_sc8810[cpu].freq_tbl[%d].frequency:%d\n", __func__, cnt,
				scalable_sc8810[cpu].freq_tbl[cnt].frequency);
	}
	return 0;
}

static int sprd_cpufreq_verify_speed(struct cpufreq_policy *policy)
{
	if (policy->cpu != 0)
		return -EINVAL;

	return cpufreq_frequency_table_verify(policy, scalable_sc8810[policy->cpu].freq_tbl);

}

/*@return: KHz*/
static unsigned int sprd_cpufreq_get_speed(unsigned int cpu)
{
	return cpu_clk_get_rate(cpu) / 1000;
}

static int sprd_cpufreq_set_target(struct cpufreq_policy *policy,
				unsigned int target_freq,
				unsigned int relation)
{
		int ret = -EFAULT;
		int index;
		struct cpufreq_frequency_table *table;
#ifdef CONFIG_SMP
		struct cpufreq_work_struct *cpu_work = NULL;
		cpumask_var_t mask;

		if (!alloc_cpumask_var(&mask, GFP_KERNEL))
			return -ENOMEM;

		if (!cpu_active(policy->cpu)) {
			pr_info("cpufreq: cpu %d is not active.\n", policy->cpu);
			return -ENODEV;
		}
#endif

//added by xing wei for mint
		//wrong logic ...
		//if (global_cpufreq_max_limit == -1 || global_cpufreq_min_limit ==  -1) {
		//	printk(" !!!we no need to change\n");
		//	return 0;	// just return do not need to change clock and vdd
		//}
		printk("! before target_feq %u ---", target_freq);
		spin_lock(&g_cpufreq_lock);
		if (target_freq > global_cpufreq_max_limit && global_cpufreq_max_limit != -1)
			target_freq = global_cpufreq_max_limit;
		if (target_freq < global_cpufreq_min_limit && global_cpufreq_min_limit != -1)
			target_freq = global_cpufreq_min_limit;
		spin_unlock(&g_cpufreq_lock);
		printk("now target_feq %u \n", target_freq);
//end //added by xing wei
		mutex_lock(&per_cpu(cpufreq_suspend, policy->cpu).suspend_mutex);

		if (per_cpu(cpufreq_suspend, policy->cpu).device_suspended) {
			printk("cpufreq: cpu%d scheduling frequency change "
					"in suspend.\n", policy->cpu);
			ret = -EFAULT;
			goto done;
		}

		table = cpufreq_frequency_get_table(policy->cpu);

		if (cpufreq_frequency_table_target(policy, table, target_freq, relation,
				&index)) {
			pr_err("cpufreq: invalid target_freq: %d\n", target_freq);
			ret = -EINVAL;
			goto done;
		}

#ifdef CONFIG_CPU_FREQ_DEBUG
		pr_debug("CPU[%d] target %d relation %d (%d-%d) selected %d\n",
			policy->cpu, target_freq, relation,
			policy->min, policy->max, table[index].frequency);
#endif

#ifdef CONFIG_SMP
		cpu_work = &per_cpu(cpufreq_work, policy->cpu);
		cpu_work->policy = policy;
		cpu_work->frequency = table[index].frequency;
		cpu_work->status = -ENODEV;
		cpu_work->index = index;

		cpumask_clear(mask);
		cpumask_set_cpu(policy->cpu, mask);
		if (cpumask_equal(mask, &current->cpus_allowed)) {
			//ret = set_cpu_freq(cpu_work->policy, cpu_work->frequency);
			ret = sprd_cpufreq_set_rate(cpu_work->policy, cpu_work->index);
			goto done;
		} else {
			cancel_work_sync(&cpu_work->work);
			INIT_COMPLETION(cpu_work->complete);
			queue_work_on(policy->cpu, sprd_cpufreq_wq, &cpu_work->work);
			wait_for_completion(&cpu_work->complete);
		}

		free_cpumask_var(mask);
		ret = cpu_work->status;
#else
		ret = sprd_cpufreq_set_rate(policy, index);
#endif

	done:

		mutex_unlock(&per_cpu(cpufreq_suspend, policy->cpu).suspend_mutex);
		return ret;

}

static int sprd_cpufreq_driver_init(struct cpufreq_policy *policy)
{
	int ret;
#ifdef CONFIG_SMP
	struct cpufreq_work_struct *cpu_work = NULL;
#endif

	ret = sc8810_cpufreq_table_init( );
	if(ret){
		return -ENODEV;
	}
	
	policy->cur = cpu_clk_get_rate(policy->cpu) / 1000; /* current cpu frequency : KHz*/
	policy->cpuinfo.transition_latency = 1 * 1000 * 1000;//why this value??

#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	cpufreq_frequency_table_get_attr(scalable_sc8810[policy->cpu].freq_tbl, policy->cpu);
#endif

	ret = cpufreq_frequency_table_cpuinfo(policy, scalable_sc8810[policy->cpu].freq_tbl);
	if (ret != 0) {
		pr_err("cpufreq: Failed to configure frequency table: %d\n", ret);
	}

#ifdef CONFIG_SMP
	cpu_work = &per_cpu(cpufreq_work, policy->cpu);
	INIT_WORK(&cpu_work->work, set_cpu_work);
	init_completion(&cpu_work->complete);
#endif

	return ret;
}

static struct freq_attr *sprd_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver sprd_cpufreq_driver = {
	.owner		= THIS_MODULE,
	.flags      = CPUFREQ_STICKY,
	.verify		= sprd_cpufreq_verify_speed,
	.target		= sprd_cpufreq_set_target,
	.get		= sprd_cpufreq_get_speed,
	.init		= sprd_cpufreq_driver_init,
	.name		= "sprd_cpufreq",
	.attr		= sprd_cpufreq_attr,
};

static int sprd_cpufreq_suspend(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		mutex_lock(&per_cpu(cpufreq_suspend, cpu).suspend_mutex);
		per_cpu(cpufreq_suspend, cpu).device_suspended = 1;
		mutex_unlock(&per_cpu(cpufreq_suspend, cpu).suspend_mutex);
	}

	return NOTIFY_DONE;
}

static int sprd_cpufreq_resume(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		per_cpu(cpufreq_suspend, cpu).device_suspended = 0;
	}

	return NOTIFY_DONE;
}

static int sprd_cpufreq_pm_event(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	switch (event) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		return sprd_cpufreq_resume();
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		return sprd_cpufreq_suspend();
	default:
		return NOTIFY_DONE;
	}
}

static struct notifier_block sprd_cpufreq_pm_notifier = {
	.notifier_call = sprd_cpufreq_pm_event,
};

static int __init sprd_cpufreq_register(void){
	int cpu;

	for_each_possible_cpu(cpu) {
		mutex_init(&(per_cpu(cpufreq_suspend, cpu).suspend_mutex));
		per_cpu(cpufreq_suspend, cpu).device_suspended = 0;
	}

#ifdef CONFIG_SMP
	sprd_cpufreq_wq = create_workqueue("sprd-cpufreq");
#endif

	register_pm_notifier(&sprd_cpufreq_pm_notifier);
	return cpufreq_register_driver(&sprd_cpufreq_driver);
}

late_initcall(sprd_cpufreq_register);

/*
 *  sec_debug.c
 *
 */

#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/sysrq.h>
//#include <mach/regs-pmu.h>

/* FIXME: This is temporary solution come from Midas */
#include <asm/outercache.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>

#include <mach/hardware.h>
#include <mach/system.h>
#include <mach/sec_debug.h>
#include <linux/fs.h>
#include <linux/seq_file.h>

#ifdef CONFIG_SEC_USER_EXEC_DIRECT
#include "sec/user/exec.h"
#endif

#define RESET_REASON_NORMAL		0x1A2B3C00
#define RESET_REASON_SMPL			0x1A2B3C01
#define RESET_REASON_WSTR			0x1A2B3C02
#define RESET_REASON_WATCHDOG 	0x1A2B3C03
#define RESET_REASON_PANIC			0x1A2B3C04
#define RESET_REASON_LPM			0x1A2B3C10
#define RESET_REASON_RECOVERY		0x1A2B3C11
#define RESET_REASON_FOTA			0x1A2B3C12

enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT = 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC = 0x000000C8,
	UPLOAD_CAUSE_FORCED_UPLOAD = 0x00000022,
	UPLOAD_CAUSE_CP_ERROR_FATAL = 0x000000CC,
	UPLOAD_CAUSE_USER_FAULT = 0x0000002F,
	UPLOAD_CAUSE_HSIC_DISCONNECTED = 0x000000DD,
};

struct sec_debug_mmu_reg_t {
	int SCTLR;
	int TTBR0;
	int TTBR1;
	int TTBCR;
	int DACR;
	int DFSR;
	int DFAR;
	int IFSR;
	int IFAR;
	int DAFSR;
	int IAFSR;
	int PMRRR;
	int NMRRR;
	int FCSEPID;
	int CONTEXT;
	int URWTPID;
	int UROTPID;
	int POTPIDR;
};

/* ARM CORE regs mapping structure */
struct sec_debug_core_t {
	/* COMMON */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;

	/* SVC */
	unsigned int r13_svc;
	unsigned int r14_svc;
	unsigned int spsr_svc;

	/* PC & CPSR */
	unsigned int pc;
	unsigned int cpsr;

	/* USR/SYS */
	unsigned int r13_usr;
	unsigned int r14_usr;

	/* FIQ */
	unsigned int r8_fiq;
	unsigned int r9_fiq;
	unsigned int r10_fiq;
	unsigned int r11_fiq;
	unsigned int r12_fiq;
	unsigned int r13_fiq;
	unsigned int r14_fiq;
	unsigned int spsr_fiq;

	/* IRQ */
	unsigned int r13_irq;
	unsigned int r14_irq;
	unsigned int spsr_irq;

	/* MON */
	unsigned int r13_mon;
	unsigned int r14_mon;
	unsigned int spsr_mon;

	/* ABT */
	unsigned int r13_abt;
	unsigned int r14_abt;
	unsigned int spsr_abt;

	/* UNDEF */
	unsigned int r13_und;
	unsigned int r14_und;
	unsigned int spsr_und;

};

/* enable/disable sec_debug feature
 * level = 0 when enable = 0 && enable_user = 0
 * level = 1 when enable = 1 && enable_user = 0
 * level = 0x10001 when enable = 1 && enable_user = 1
 * The other cases are not considered
 */
union {
	struct {
		u16 kernel_fault;
		u16 user_fault;
	} en;
	u32 uint_val;
} debug_level = { .en.kernel_fault = 1, };

module_param_named(enable, debug_level.en.kernel_fault, ushort, 0644);
module_param_named(enable_user, debug_level.en.user_fault, ushort, 0644);
module_param_named(level, debug_level.uint_val, uint, 0644);

struct sec_debug_fault_status_t {
	/* COMMON */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;
	unsigned int r13;
	unsigned int r14;
	unsigned int r15;	
	unsigned int cpsr;
	unsigned int cur_process_magic;
};

static unsigned reset_reason = 0xFFEEFFEE;

module_param_named(reset_reason, reset_reason, uint, 0644);

static const char *gkernel_sec_build_info_date_time[] = {
	__DATE__,
	__TIME__
};

static char gkernel_sec_build_info[100];

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
static struct sched_log gExcpTaskLog[NR_CPUS][SCHED_LOG_MAX]
	__cacheline_aligned;
static atomic_t gExcpTaskLogIdx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
#ifdef CONFIG_SEC_DEBUG_IRQ_EXIT_LOG
static unsigned long long gExcpIrqExitTime[NR_CPUS];
#endif

static int checksum_sched_log(void)
{
	int sum = 0, i;
	for (i = 0; i < sizeof(gExcpTaskLog); i++)
		sum += *((char *)gExcpTaskLog + i);

	return sum;
}
#else
static int checksum_sched_log(void)
{
	return 0;
}
#endif

/* klaatu - semaphore log */
#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
struct sem_debug sem_debug_free_head;
struct sem_debug sem_debug_done_head;
int sem_debug_free_head_cnt;
int sem_debug_done_head_cnt;
int sem_debug_init = 0;
spinlock_t sem_debug_lock;

/* rwsemaphore logging */
struct rwsem_debug rwsem_debug_free_head;
struct rwsem_debug rwsem_debug_done_head;
int rwsem_debug_free_head_cnt;
int rwsem_debug_done_head_cnt;
int rwsem_debug_init = 0;
spinlock_t rwsem_debug_lock;

#endif /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

DEFINE_PER_CPU(struct sec_debug_core_t, sec_debug_core_reg);
DEFINE_PER_CPU(struct sec_debug_mmu_reg_t, sec_debug_mmu_reg);
DEFINE_PER_CPU(enum sec_debug_upload_cause_t, sec_debug_upload_cause);
DEFINE_PER_CPU(struct sec_debug_fault_status_t, sec_debug_fault_status);

/* core reg dump function*/
static inline void sec_debug_save_core_reg(struct sec_debug_core_t *core_reg)
{
	/* we will be in SVC mode when we enter this function. Collect
	   SVC registers along with cmn registers. */
	asm("str r0, [%0,#0]\n\t"	/* R0 is pushed first to core_reg */
	    "mov r0, %0\n\t"		/* R0 will be alias for core_reg */
	    "str r1, [r0,#4]\n\t"	/* R1 */
	    "str r2, [r0,#8]\n\t"	/* R2 */
	    "str r3, [r0,#12]\n\t"	/* R3 */
	    "str r4, [r0,#16]\n\t"	/* R4 */
	    "str r5, [r0,#20]\n\t"	/* R5 */
	    "str r6, [r0,#24]\n\t"	/* R6 */
	    "str r7, [r0,#28]\n\t"	/* R7 */
	    "str r8, [r0,#32]\n\t"	/* R8 */
	    "str r9, [r0,#36]\n\t"	/* R9 */
	    "str r10, [r0,#40]\n\t"	/* R10 */
	    "str r11, [r0,#44]\n\t"	/* R11 */
	    "str r12, [r0,#48]\n\t"	/* R12 */
	    /* SVC */
	    "str r13, [r0,#52]\n\t"	/* R13_SVC */
	    "str r14, [r0,#56]\n\t"	/* R14_SVC */
	    "mrs r1, spsr\n\t"		/* SPSR_SVC */
	    "str r1, [r0,#60]\n\t"
	    /* PC and CPSR */
	    "sub r1, r15, #0x4\n\t"	/* PC */
	    "str r1, [r0,#64]\n\t"
	    "mrs r1, cpsr\n\t"		/* CPSR */
	    "str r1, [r0,#68]\n\t"
	    /* SYS/USR */
	    "mrs r1, cpsr\n\t"		/* switch to SYS mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x1f\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#72]\n\t"	/* R13_USR */
	    "str r14, [r0,#76]\n\t"	/* R14_USR */
	    /* FIQ */
	    "mrs r1, cpsr\n\t"		/* switch to FIQ mode */
	    "and r1,r1,#0xFFFFFFE0\n\t"
	    "orr r1,r1,#0x11\n\t"
	    "msr cpsr,r1\n\t"
	    "str r8, [r0,#80]\n\t"	/* R8_FIQ */
	    "str r9, [r0,#84]\n\t"	/* R9_FIQ */
	    "str r10, [r0,#88]\n\t"	/* R10_FIQ */
	    "str r11, [r0,#92]\n\t"	/* R11_FIQ */
	    "str r12, [r0,#96]\n\t"	/* R12_FIQ */
	    "str r13, [r0,#100]\n\t"	/* R13_FIQ */
	    "str r14, [r0,#104]\n\t"	/* R14_FIQ */
	    "mrs r1, spsr\n\t"		/* SPSR_FIQ */
	    "str r1, [r0,#108]\n\t"
	    /* IRQ */
	    "mrs r1, cpsr\n\t"		/* switch to IRQ mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x12\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#112]\n\t"	/* R13_IRQ */
	    "str r14, [r0,#116]\n\t"	/* R14_IRQ */
	    "mrs r1, spsr\n\t"		/* SPSR_IRQ */
	    "str r1, [r0,#120]\n\t"
	    /* MON */
	    "mrs r1, cpsr\n\t"		/* switch to monitor mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x16\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#124]\n\t"	/* R13_MON */
	    "str r14, [r0,#128]\n\t"	/* R14_MON */
	    "mrs r1, spsr\n\t"		/* SPSR_MON */
	    "str r1, [r0,#132]\n\t"
	    /* ABT */
	    "mrs r1, cpsr\n\t"		/* switch to Abort mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x17\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#136]\n\t"	/* R13_ABT */
	    "str r14, [r0,#140]\n\t"	/* R14_ABT */
	    "mrs r1, spsr\n\t"		/* SPSR_ABT */
	    "str r1, [r0,#144]\n\t"
	    /* UND */
	    "mrs r1, cpsr\n\t"		/* switch to undef mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x1B\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#148]\n\t"	/* R13_UND */
	    "str r14, [r0,#152]\n\t"	/* R14_UND */
	    "mrs r1, spsr\n\t"		/* SPSR_UND */
	    "str r1, [r0,#156]\n\t"
	    /* restore to SVC mode */
	    "mrs r1, cpsr\n\t"		/* switch to SVC mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x13\n\t"
	    "msr cpsr,r1\n\t" :		/* output */
	    : "r"(core_reg)		/* input */
	    : "%r0", "%r1"		/* clobbered registers */
	);

	return;
}

static inline void sec_debug_save_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
	asm("mrc    p15, 0, r1, c1, c0, 0\n\t"	/* SCTLR */
	    "str r1, [%0]\n\t"
	    "mrc    p15, 0, r1, c2, c0, 0\n\t"	/* TTBR0 */
	    "str r1, [%0,#4]\n\t"
	    "mrc    p15, 0, r1, c2, c0,1\n\t"	/* TTBR1 */
	    "str r1, [%0,#8]\n\t"
	    "mrc    p15, 0, r1, c2, c0,2\n\t"	/* TTBCR */
	    "str r1, [%0,#12]\n\t"
	    "mrc    p15, 0, r1, c3, c0,0\n\t"	/* DACR */
	    "str r1, [%0,#16]\n\t"
	    "mrc    p15, 0, r1, c5, c0,0\n\t"	/* DFSR */
	    "str r1, [%0,#20]\n\t"
	    "mrc    p15, 0, r1, c6, c0,0\n\t"	/* DFAR */
	    "str r1, [%0,#24]\n\t"
	    "mrc    p15, 0, r1, c5, c0,1\n\t"	/* IFSR */
	    "str r1, [%0,#28]\n\t"
	    "mrc    p15, 0, r1, c6, c0,2\n\t"	/* IFAR */
	    "str r1, [%0,#32]\n\t"
	    /* Don't populate DAFSR and RAFSR */
	    "mrc    p15, 0, r1, c10, c2,0\n\t"	/* PMRRR */
	    "str r1, [%0,#44]\n\t"
	    "mrc    p15, 0, r1, c10, c2,1\n\t"	/* NMRRR */
	    "str r1, [%0,#48]\n\t"
	    "mrc    p15, 0, r1, c13, c0,0\n\t"	/* FCSEPID */
	    "str r1, [%0,#52]\n\t"
	    "mrc    p15, 0, r1, c13, c0,1\n\t"	/* CONTEXT */
	    "str r1, [%0,#56]\n\t"
	    "mrc    p15, 0, r1, c13, c0,2\n\t"	/* URWTPID */
	    "str r1, [%0,#60]\n\t"
	    "mrc    p15, 0, r1, c13, c0,3\n\t"	/* UROTPID */
	    "str r1, [%0,#64]\n\t"
	    "mrc    p15, 0, r1, c13, c0,4\n\t"	/* POTPIDR */
	    "str r1, [%0,#68]\n\t" :		/* output */
	    : "r"(mmu_reg)			/* input */
	    : "%r1", "memory"			/* clobbered register */
	);
}

static inline void sec_debug_save_context(void)
{
	unsigned long flags;
	local_irq_save(flags);
	sec_debug_save_mmu_reg(&per_cpu(sec_debug_mmu_reg, smp_processor_id()));
	sec_debug_save_core_reg(&per_cpu
				(sec_debug_core_reg, smp_processor_id()));

	pr_emerg("(%s) context saved(CPU:%d)\n", __func__, smp_processor_id());
	local_irq_restore(flags);
}

void sec_debug_save_pte(void *pte, int task_addr) 
{

	memcpy(&per_cpu(sec_debug_fault_status,smp_processor_id()), pte,sizeof(struct sec_debug_fault_status_t));
	per_cpu(sec_debug_fault_status,smp_processor_id()).cur_process_magic =task_addr;
}

static void sec_debug_set_upload_magic(unsigned magic)
{
	pr_emerg("(%s) %x\n", __func__, magic);

	*(unsigned int *)MAGIC_ADDR = magic;

	flush_cache_all();

	outer_flush_all();
}

static int sec_debug_normal_reboot_handler(struct notifier_block *nb,
					   unsigned long l, void *p)
{
	sec_debug_set_upload_magic(0x0);

	return 0;
}

static void sec_debug_set_upload_cause(enum sec_debug_upload_cause_t type)
{
	per_cpu(sec_debug_upload_cause, smp_processor_id()) = type;

	/* to check VDD_ALIVE / XnRESET issue */
	__raw_writel(type, SPRD_INFORM3);
	__raw_writel(type, SPRD_INFORM4);
	__raw_writel(type, SPRD_INFORM6);

	pr_emerg("(%s) %x\n", __func__, type);
}

/*
 * Called from dump_stack()
 * This function call does not necessarily mean that a fatal error
 * had occurred. It may be just a warning.
 */
static inline int sec_debug_dump_stack(void)
{
	if (!debug_level.en.kernel_fault)
		return -1;

	sec_debug_save_context();

	/* flush L1 from each core.
	   L2 will be flushed later before reset. */
	flush_cache_all();

	return 0;
}

static inline void sec_debug_hw_reset(void)
{
	pr_emerg("(%s) %s\n", __func__, linux_banner);
	pr_emerg("(%s) rebooting...\n", __func__);

	flush_cache_all();

	outer_flush_all();

	arch_reset(0,(const char *)"bootloader");

	while (1) ;
}

static int sec_debug_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{

	if (!debug_level.en.kernel_fault)
		return -1;

	sec_debug_set_upload_magic(0x66262564);

	if (!strcmp(buf, "User Fault"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_USER_FAULT);
	else if (!strcmp(buf, "Crash Key"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strcmp(buf, "CP Crash"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_CP_ERROR_FATAL);
	else if (!strcmp(buf, "HSIC Disconnected"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_HSIC_DISCONNECTED);
	else
		sec_debug_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);

	pr_err("(%s) checksum_sched_log: %x\n", __func__, checksum_sched_log());


	sec_debug_dump_stack();
#ifdef CONFIG_SEC_USER_EXEC_DIRECT
	sec_user_exec_handler(nb, l, buf);
#endif
	sec_debug_hw_reset();
	return 0;
}
#ifdef CONFIG_SEC_DEBUG_FUPLOAD_DUMP_MORE
static void dump_state_and_upload(void);
#endif

void sec_debug_check_crash_key(unsigned int code, int value)
{
	static bool volup_p;
	static bool voldown_p;
	static int loopcount;

	if (!debug_level.en.kernel_fault)
		return;

	/* Enter Force Upload
	 *  Hold volume down key first
	 *  and then press power key twice
	 *  and volume up key should not be pressed
	 */
	if (value) {
		if (code == KEY_VOLUMEUP)
			volup_p = true;
		if (code == KEY_VOLUMEDOWN)
			voldown_p = true;
		if (!volup_p && voldown_p) {
			if (code == KEY_POWER) {
				pr_info
				    ("%s: count for enter forced upload : %d\n",
				     __func__, ++loopcount);
				if (loopcount == 2)
#ifdef CONFIG_SEC_DEBUG_FUPLOAD_DUMP_MORE
					dump_state_and_upload();
#else
					panic("Crash Key");
#endif
			}
		}
	} else {
		if (code == KEY_VOLUMEUP)
			volup_p = false;
		if (code == KEY_VOLUMEDOWN) {
			loopcount = 0;
			voldown_p = false;
		}
	}
}

static struct notifier_block nb_reboot_block = {
	.notifier_call = sec_debug_normal_reboot_handler
};

static struct notifier_block nb_panic_block = {
	.notifier_call = sec_debug_panic_handler,
};


void sec_debug_emergency_restart_handler(void)
{

	if (!debug_level.en.kernel_fault)
		return;

	sec_debug_set_upload_magic(0x66262564);
	sec_debug_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);
	sec_debug_dump_stack();
	sec_debug_hw_reset();

}

static void sec_debug_set_build_info(void)
{
	char *p = gkernel_sec_build_info;
	sprintf(p, "Kernel Build Info : ");
	strcat(p, " Date:");
	strncat(p, gkernel_sec_build_info_date_time[0], 12);
	strcat(p, " Time:");
	strncat(p, gkernel_sec_build_info_date_time[1], 9);
}

__init int sec_debug_init(void)
{
	if (!debug_level.en.kernel_fault)
		return -1;

	sec_debug_set_build_info();

	sec_debug_set_upload_magic(0x66262564);
	sec_debug_set_upload_cause(UPLOAD_CAUSE_INIT);

	register_reboot_notifier(&nb_reboot_block);

	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);

	return 0;
}

int sec_debug_level(void)
{
	return debug_level.uint_val;
}

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
void sec_debug_task_sched_log(int cpu, struct task_struct *task)
{
	unsigned i;

	if (!debug_level.en.kernel_fault)
		return;

	i = atomic_inc_return(&gExcpTaskLogIdx[cpu]) & (SCHED_LOG_MAX - 1);
	(*gExcpTaskLogPtr)[cpu][i].time = cpu_clock(cpu);
	strcpy((*gExcpTaskLogPtr)[cpu][i].log.task.comm, task->comm);
	(*gExcpTaskLogPtr)[cpu][i].log.task.pid = task->pid;
	(*gExcpTaskLogPtr)[cpu][i].log.task.cpu = cpu;
}

void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en)
{
	int cpu = smp_processor_id();
	unsigned i;

	if (!debug_level.en.kernel_fault)
		return;

	i = atomic_inc_return(&gExcpTaskLogIdx[cpu]) & (SCHED_LOG_MAX - 1);
	(*gExcpTaskLogPtr)[cpu][i].time = cpu_clock(cpu);
	(*gExcpTaskLogPtr)[cpu][i].log.irq.cpu = cpu;
	(*gExcpTaskLogPtr)[cpu][i].log.irq.irq = irq;
	(*gExcpTaskLogPtr)[cpu][i].log.irq.fn = (void *)fn;
	(*gExcpTaskLogPtr)[cpu][i].log.irq.en = en;
}

#ifdef CONFIG_SEC_DEBUG_IRQ_EXIT_LOG
void sec_debug_irq_last_exit_log(void)
{
	int cpu = smp_processor_id();
    gExcpIrqExitTime[cpu] = cpu_clock(cpu);
}
#endif
#endif /* CONFIG_SEC_DEBUG_SCHED_LOG */

/* klaatu - semaphore log */
#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
void debug_semaphore_init(void)
{
	int i = 0;
	struct sem_debug *sem_debug = NULL;

	spin_lock_init(&sem_debug_lock);
	sem_debug_free_head_cnt = 0;
	sem_debug_done_head_cnt = 0;

	/* initialize list head of sem_debug */
	INIT_LIST_HEAD(&sem_debug_free_head.list);
	INIT_LIST_HEAD(&sem_debug_done_head.list);

	for (i = 0; i < SEMAPHORE_LOG_MAX; i++) {
		/* malloc semaphore */
		sem_debug = kmalloc(sizeof(struct sem_debug), GFP_KERNEL);
		/* add list */
		list_add(&sem_debug->list, &sem_debug_free_head.list);
		sem_debug_free_head_cnt++;
	}

	sem_debug_init = 1;
}

void debug_semaphore_down_log(struct semaphore *sem)
{
	struct list_head *tmp;
	struct sem_debug *sem_dbg;
	unsigned long flags;

	if (!sem_debug_init)
		return;

	spin_lock_irqsave(&sem_debug_lock, flags);
	list_for_each(tmp, &sem_debug_free_head.list) {
		sem_dbg = list_entry(tmp, struct sem_debug, list);
		sem_dbg->task = current;
		sem_dbg->sem = sem;
		/* strcpy(sem_dbg->comm,current->group_leader->comm); */
		sem_dbg->pid = current->pid;
		sem_dbg->cpu = smp_processor_id();
		list_del(&sem_dbg->list);
		list_add(&sem_dbg->list, &sem_debug_done_head.list);
		sem_debug_free_head_cnt--;
		sem_debug_done_head_cnt++;
		break;
	}
	spin_unlock_irqrestore(&sem_debug_lock, flags);
}

void debug_semaphore_up_log(struct semaphore *sem)
{
	struct list_head *tmp;
	struct sem_debug *sem_dbg;
	unsigned long flags;

	if (!sem_debug_init)
		return;

	spin_lock_irqsave(&sem_debug_lock, flags);
	list_for_each(tmp, &sem_debug_done_head.list) {
		sem_dbg = list_entry(tmp, struct sem_debug, list);
		if (sem_dbg->sem == sem && sem_dbg->pid == current->pid) {
			list_del(&sem_dbg->list);
			list_add(&sem_dbg->list, &sem_debug_free_head.list);
			sem_debug_free_head_cnt++;
			sem_debug_done_head_cnt--;
			break;
		}
	}
	spin_unlock_irqrestore(&sem_debug_lock, flags);
}

/* rwsemaphore logging */
void debug_rwsemaphore_init(void)
{
	int i = 0;
	struct rwsem_debug *rwsem_debug = NULL;

	spin_lock_init(&rwsem_debug_lock);
	rwsem_debug_free_head_cnt = 0;
	rwsem_debug_done_head_cnt = 0;

	/* initialize list head of sem_debug */
	INIT_LIST_HEAD(&rwsem_debug_free_head.list);
	INIT_LIST_HEAD(&rwsem_debug_done_head.list);

	for (i = 0; i < RWSEMAPHORE_LOG_MAX; i++) {
		/* malloc semaphore */
		rwsem_debug = kmalloc(sizeof(struct rwsem_debug), GFP_KERNEL);
		/* add list */
		list_add(&rwsem_debug->list, &rwsem_debug_free_head.list);
		rwsem_debug_free_head_cnt++;
	}

	rwsem_debug_init = 1;
}

void debug_rwsemaphore_down_log(struct rw_semaphore *sem, int dir)
{
	struct list_head *tmp;
	struct rwsem_debug *sem_dbg;
	unsigned long flags;

	if (!rwsem_debug_init)
		return;

	spin_lock_irqsave(&rwsem_debug_lock, flags);
	list_for_each(tmp, &rwsem_debug_free_head.list) {
		sem_dbg = list_entry(tmp, struct rwsem_debug, list);
		sem_dbg->task = current;
		sem_dbg->sem = sem;
		/* strcpy(sem_dbg->comm,current->group_leader->comm); */
		sem_dbg->pid = current->pid;
		sem_dbg->cpu = smp_processor_id();
		sem_dbg->direction = dir;
		list_del(&sem_dbg->list);
		list_add(&sem_dbg->list, &rwsem_debug_done_head.list);
		rwsem_debug_free_head_cnt--;
		rwsem_debug_done_head_cnt++;
		break;
	}
	spin_unlock_irqrestore(&rwsem_debug_lock, flags);
}

void debug_rwsemaphore_up_log(struct rw_semaphore *sem)
{
	struct list_head *tmp;
	struct rwsem_debug *sem_dbg;
	unsigned long flags;

	if (!rwsem_debug_init)
		return;

	spin_lock_irqsave(&rwsem_debug_lock, flags);
	list_for_each(tmp, &rwsem_debug_done_head.list) {
		sem_dbg = list_entry(tmp, struct rwsem_debug, list);
		if (sem_dbg->sem == sem && sem_dbg->pid == current->pid) {
			list_del(&sem_dbg->list);
			list_add(&sem_dbg->list, &rwsem_debug_free_head.list);
			rwsem_debug_free_head_cnt++;
			rwsem_debug_done_head_cnt--;
			break;
		}
	}
	spin_unlock_irqrestore(&rwsem_debug_lock, flags);
}
#endif /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

#ifdef CONFIG_SEC_DEBUG_USER
void sec_user_fault_dump(void)
{
	if (debug_level.en.kernel_fault == 1 && debug_level.en.user_fault == 1)
		panic("User Fault");
}

static int sec_user_fault_write(struct file *file, const char __user * buffer,
				size_t count, loff_t * offs)
{
	char buf[100];

	if (count > sizeof(buf) - 1)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	buf[count] = '\0';

	if (strncmp(buf, "dump_user_fault", 15) == 0)
		sec_user_fault_dump();

	if(strncmp(buf,"Intentional_reboot", 18) == 0)
			kernel_restart(NULL);
	return count;
}

static const struct file_operations sec_user_fault_proc_fops = {
	.write = sec_user_fault_write,
};

char forced_ramdump_enable=0;
static int sec_debug_set_debug_level(char *str)
{
	if (!str)
		return -EINVAL;
	if (strcmp(str, "HIGH") == 0)
	{
		forced_ramdump_enable = 1;

	}
	else if (strcmp(str, "MID") == 0)
	{
		forced_ramdump_enable = 1;
	}
	else 
	{
		forced_ramdump_enable = 0;
	}
	return 0;
}
early_param("DEBUG_LEVEL", sec_debug_set_debug_level);

static int __init sec_debug_user_fault_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("user_fault", S_IWUGO, NULL,
			    &sec_user_fault_proc_fops);
	if (!entry)
		return -ENOMEM;
	return 0;
}

device_initcall(sec_debug_user_fault_init);
#endif  /* CONFIG_SEC_DEBUG_USER */

/* Reset Reason given from Bootloader */
static int set_reset_reason_proc_show(struct seq_file *m, void *v)
{
	/*
		RESET_REASON_NORMAL
		RESET_REASON_SMPL
		RESET_REASON_WSTR
		RESET_REASON_WATCHDOG
		RESET_REASON_PANIC
		RESET_REASON_LPM
		RESET_REASON_RECOVERY
		RESET_REASON_FOTA
	*/
	switch(reset_reason){
	case RESET_REASON_NORMAL:
		seq_printf(m, "PON_NORMAL\n");
		break;
	case RESET_REASON_SMPL:
		seq_printf(m, "PON_SMPL\n");
		break;	
	case RESET_REASON_WSTR:
	case RESET_REASON_WATCHDOG:
	case RESET_REASON_PANIC:
		seq_printf(m, "PON_PANIC\n");
		break;	
	case RESET_REASON_LPM:
	case RESET_REASON_RECOVERY:
	case RESET_REASON_FOTA:
		seq_printf(m, "PON_NORMAL\n");
		break;	
	default:
		seq_printf(m, "PON_NORMAL\n");
	}

	return 0;
}

static int sec_reset_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_reset_reason_proc_show, NULL);
}

static const struct file_operations sec_reset_reason_proc_fops = {
	.open		= sec_reset_reason_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init sec_debug_reset_reason_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("reset_reason", S_IRUGO, NULL,
			    &sec_reset_reason_proc_fops);
	if (!entry)
		return -ENOMEM;

	return 0;
}

device_initcall(sec_debug_reset_reason_init);

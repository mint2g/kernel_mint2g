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
 *  #ident  "@(#)f_nk.h 1.97     09/12/24 Red Bend"
 *
 *  Contributor(s):
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *    Guennadi Maslov (guennadi.maslov@redbend.com)
 *    Sebastien Laborie (sebastien.laborie@redbend.com)
 *
 ****************************************************************
 */

#ifndef _NK_F_NK_H
#define _NK_F_NK_H

#include "nk_f.h"

#define	NK_OSCTX_DATE		 0x290212 /* file modification date: DDMMYY */

	/*
	 * Standard ARM vectors 
	 */
#define NK_RESET_VECTOR		 0x00
#define NK_UNDEF_INSTR_VECTOR	 0x04
#define NK_SYSTEM_CALL_VECTOR	 0x08
#define NK_PREFETCH_ABORT_VECTOR 0x0c
#define NK_DATA_ABORT_VECTOR	 0x10
#define NK_RESERVED_VECTOR	 0x14
#define NK_IRQ_VECTOR		 0x18
#define NK_FIQ_VECTOR		 0x1c

	/*
	 * Exended NK vectors
	 */
#define NK_XIRQ_VECTOR		 0x24	/* cross IRQ vector */
#define NK_IIRQ_VECTOR		 0x28	/* indirect IRQ vector */
#define NK_DIRQ_VECTOR		 0x2c	/* direct IRQ vector */
#define NK_ISWI_VECTOR		 0x30	/* indirect SWI vector */
#define NK_IPABORT_VECTOR	 0x34	/* indirect prefetch abort vector */

	/*
	 * Virtual exceptions
	 */
#define	NK_VEX_RUN_BIT		 0	/* running/idle event */
#define	NK_VEX_IRQ_BIT		 8	/* IRQ event */
#define	NK_VEX_FIQ_BIT		 16	/* FIQ event */
#define	NK_VEX_ABORT_BIT	 24	/* ABORT event */

#define	NK_VEX_RUN		 (1 << NK_VEX_RUN_BIT)
#define	NK_VEX_IRQ		 (1 << NK_VEX_IRQ_BIT)
#define	NK_VEX_FIQ		 (1 << NK_VEX_FIQ_BIT)
#define	NK_VEX_ABORT		 (1 << NK_VEX_ABORT_BIT)
#define	NK_VEX_MASK		 (NK_VEX_RUN | NK_VEX_IRQ | NK_VEX_FIQ)

#define NK_XIRQ_IRQ		 0xff	/* IRQ assigned to XIRQ */
#define	NK_XIRQ_LIMIT		 (32*32)/* Dependency on NkXIrqMask type ! */

#define NK_COMMAND_LINE_SIZE	 1024
#define NK_TAG_LIST_SIZE	 (NK_COMMAND_LINE_SIZE + 512)

	/*
	 * Standard ARM Hardware Capabilities 
	 *
	 */
#ifdef NOT_YET
#define NK_HWCAP_SWP		    1
#define NK_HWCAP_HALF		    2
#define NK_HWCAP_THUMB		    4
#define NK_HWCAP_26BIT		    8
#define NK_HWCAP_FAST_MULT	   16
#define NK_HWCAP_FPA		   32
#define NK_HWCAP_VFP		   64
#define NK_HWCAP_EDSP		  128
#define NK_HWCAP_JAVA		  256
#define NK_HWCAP_CRUNCH		 1024
#endif

#define NK_HWCAP_VFP		   64
#define NK_HWCAP_IWMMXT		  512

	/*
	 * Priorities for idle OS's
	 */

#define NK_PRIO_LOWEST		 255
#define NK_PRIO_HIGHEST		 0

#define NK_PRIO_SEC_SLEEP	 (NK_PRIO_LOWEST + 2)
#define NK_PRIO_SEC_IDLE	 (NK_PRIO_SEC_SLEEP - 1)
#define NK_PRIO_PRIM_SLEEP	 (NK_PRIO_SEC_IDLE - 1)
#define NK_PRIO_PRIM_IDLE	 (NK_PRIO_PRIM_SLEEP - 1)

#if !defined(__ASM__) && !defined(__ASSEMBLY__)

    /*
     * Nano-kernel Types...
     */
typedef nku32_f NkMagic;	/* OS context magic */
typedef nku32_f NkVexMask;	/* virtual exceptions bit-mask */
typedef struct  NkXIrqMask {	/* cross IRQs bit-mask */
    nku32_f	lvl1;		/* 1st level bit-mask */
    nku32_f	lvl2[32];	/* 2nd level bit-mask */
} NkXIrqMask;

typedef nku32_f NkVmAddr;	/* GMv FIXME */
typedef nku32_f NkVmSize;

typedef void (*NkVector) (void);

typedef nku8_f NkTagList[NK_TAG_LIST_SIZE];

typedef struct NkOsCtx NkOsCtx;

    /*
     * Nano-Kernel Debug interface.
     */
typedef int  (*NkRead)	    (NkOsCtx* ctx, char* buf, int size);
typedef int  (*NkWrite)	    (NkOsCtx* ctx, const char* buf, int size);
typedef int  (*NkPoll)	    (NkOsCtx* ctx, char* ch);
typedef int  (*NkTest)	    (NkOsCtx* ctx);
typedef void (*NkInitLevel) (NkOsCtx* ctx, const int level);

typedef struct NkDbgOps {
    NkRead	read;
    NkWrite	write;
    NkPoll	poll;
    NkTest	test;
    NkInitLevel	init_level;
} NkDbgOps;

    /*
     * Nano-Kernel Console interface.
     */
typedef NkOsId	NkConsId;

typedef int  (*NkConsRead)	(NkConsId cid, char* buf, int size);
typedef int  (*NkConsWrite)	(NkConsId cid, const char* buf, int size);
typedef int  (*NkConsWriteMsg)	(NkConsId cid, const char* buf, int size);
typedef int  (*NkConsPoll)	(NkConsId cid, char* buf);
typedef int  (*NkConsTest)	(NkConsId cid);
typedef void (*NkConsOpen)	(NkConsId cid);
typedef void (*NkConsClose)	(NkConsId cid);
typedef int  (*NkConsFlush)	(NkConsId cid);
typedef int  (*NkConsIntrNotify)(void);

typedef struct NkConsOps {
    NkConsRead		read;
    NkConsWrite		write;
    NkConsPoll		poll;
    NkConsTest		test;
    NkConsOpen		open;
    NkConsClose		close;
    NkConsIntrNotify	intr_notify;
    NkConsFlush		flush;
    NkConsWriteMsg	write_msg;
} NkConsOps;



	/*
	 * Nano-Kernel Performance Monitoring interface.
	 */
typedef	nku32_f NkPmonControl;

#define	PMON_START		0
#define	PMON_STOP		1
#define PMON_START_ONE_SHOT	2
#define PMON_CPUSTATS_START	3
#define	PMON_NOP		4

	/* CPU number is encoded in NkPmonControl */
#define	PMON_CPU_BITS		8	
#define	PMON_CPU_SHIFT		((sizeof(NkPmonControl) << 3) - PMON_CPU_BITS)
#define	PMON_CPU_MASK		((1 << PMON_CPU_BITS) - 1)

#define	PMON_CONTROL_SET(cmd, cpu) \
	((cmd) | ((cpu) << PMON_CPU_SHIFT))

#define	PMON_CONTROL_CMD_GET(control) \
	((control) & ~(((1 << PMON_CPU_BITS) - 1) << PMON_CPU_SHIFT))

#define	PMON_CONTROL_CPU_GET(control)	\
	(((control) >> PMON_CPU_SHIFT) & PMON_CPU_MASK)

typedef int           (*NkControl)        (NkOsCtx* ctx, NkPmonControl ctrl, NkPhAddr curr);
typedef void          (*NkRecord)         (NkOsCtx* ctx, NkPmonState state, NkPhAddr cookie);
typedef int           (*NkGetTimerFreq)   (NkOsCtx* ctx);
typedef NkTime        (*NkGetTimerPeriod) (NkOsCtx* ctx);

typedef struct NkPmonOps {
    NkControl          control;
    NkRecord           record;
    NkGetTimerFreq     get_freq;
    NkGetTimerPeriod   get_period;
} NkPmonOps;

typedef nku32_f NkPrio;

typedef enum NkBinfoCtrl {
    INFO_BOOTCONF,
    INFO_OSTAGS,
    INFO_CMDLINE,
    INFO_EPOINT
} NkBinfoCtrl;

typedef struct NkBankInfo {
    int      numBanks;
    NkPhAddr banks;
} NkBankInfo;

typedef NkPhAddr NkEpntInfo;
typedef NkPhAddr NkCmdlInfo;
typedef NkPhAddr NkTagsInfo;

typedef struct NkBootInfo {
    NkBinfoCtrl ctrl;
    NkOsId      osid;
    NkPhAddr    data;
} NkBootInfo;

typedef nku32_f NkVcpuId;

typedef void*   NkTimerId;
typedef nku32_f NkTimerFlags;

#define	NK_TIMER_FLAGS_PERIODIC	(1 << 0)
#define	NK_TIMER_FLAGS_ONESHOT	(1 << 1)
#define	NK_TIMER_FLAGS_LOCAL	(1 << 2)

typedef struct NkTimerInfo {
    NkTime       min_delta;
    NkTime       max_delta;
    nku32_f      freq_hz;
    NkTimerFlags flags;
} NkTimerInfo;

typedef NkPhAddr (*NkDevAlloc)  (NkOsCtx* ctx, NkPhSize size);
typedef NkPhAddr (*NkPmemAlloc) (NkOsCtx* ctx, NkPhSize size);
typedef void     (*NkXIrqPost)  (NkOsCtx* ctx, NkOsId id);
typedef void     (*NkWakeUp)    (NkOsCtx* ctx);
typedef int	 (*NkIdle)      (NkOsCtx* ctx);
typedef int	 (*NkPrioSet)   (NkOsCtx* ctx, NkPrio prio);
typedef NkPrio	 (*NkPrioGet)   (NkOsCtx* ctx);
typedef int      (*NkHistGetc)  (NkOsCtx* ctx, nku32_f offset);
typedef void     (*NkRestart)   (NkOsCtx* ctx, NkOsId id);
typedef void     (*NkStop)      (NkOsCtx* ctx, NkOsId id);
typedef void     (*NkResume)    (NkOsCtx* ctx, NkOsId id);
typedef void     (*NkReady)     (NkOsCtx* ctx);
typedef void     (*NkStub)      (void);
typedef int      (*NkGetBinfo)  (NkOsCtx* ctx, NkPhAddr info);
typedef void	 (*NkWSyncAll)  (void);
typedef void	 (*NkWSyncEntry)(void* addr);
typedef void	 (*NkFlushAll)  (void);
typedef NkOsCtx* (*NkOsCtxGet)	(NkOsId id);
typedef int	 (*NkVfpGet)    (NkOsCtx* ctx);

    typedef NkXIrq
(*NkSmpXIrqAlloc) (int numb);

    typedef void
(*NkSmpDevAdd) (NkPhAddr dev);

    typedef NkPhAddr
(*NkSmpPdevAlloc) (NkPhAddr link, NkPResourceId id, NkPhSize sz);

    typedef NkPhAddr
(*NkSmpPmemAlloc) (NkPhAddr link, NkPResourceId id, NkPhSize sz);

    typedef NkXIrq
(*NkSmpPxirqAlloc)(NkPhAddr link, NkPResourceId id, NkOsId osid, int numb);

    typedef int
(*NkSmpIrqConnect) (NkXIrq xirq);

    typedef void
(*NkSmpIrqDisconnect) (NkXIrq xirq);

    typedef void
(*NkSmpIrqMask) (NkXIrq xirq);

    typedef void
(*NkSmpIrqUnmask) (NkXIrq xirq);

    typedef void
(*NkSmpIrqEoi) (NkXIrq xirq);

    typedef void
(*NkSmpIrqAffinity) (NkXIrq xirq);

    typedef void
(*NkSmpIrqPost) (NkXIrq xirq, NkOsId id);

    typedef void
(*NkSmpCpuStart) (NkVcpuId id, NkPhAddr pc);

    typedef void
(*NkSmpCpuStop) (NkVcpuId id);

    typedef NkTime
(*NkSmpTime) (void);

    typedef NkFreq
(*NkSmpTimeHz) (void);

    typedef NkTimerId
(*NkSmpTimerAlloc) (NkXIrq xirq, NkTimerFlags flags);

    typedef void
(*NkSmpTimerFree) (NkTimerId id);

    typedef void
(*NkSmpTimerInfo) (NkTimerId id, NkPhAddr info);

    typedef void
(*NkSmpTimerStartPeriodic) (NkTimerId id, nku32_f hz);

    typedef void
(*NkSmpTimerStartOneShot) (NkTimerId id, nku32_f time);

    typedef void
(*NkSmpTimerStop) (NkTimerId id);

    typedef void
(*NkSmpYield) (void);

    typedef void
(*NkSmpRelax) (void);

    typedef NkXIrq
(*NkSmpDxirqAlloc) (int numb);

    typedef void
(*NkSuspendToMemory) (NkOsCtx* ctx);

    typedef int
(*NkPowerState) (void);

#define	NK_POWER_RUNNING	0	/* the system is running */
#define	NK_POWER_SUSPEND	1	/* the system is suspended to RAM */

    typedef nku32_f
(*NkBalloonControl) (int op, nku32_f* frames, nku32_f count);

    /*
     * The NkPropSet VLX call updates the value of a given property
     * and sends a notification event. The <name> argument points to
     * a null terminated ASCII string specifying the property name.
     * The <value> argument points to the new property value. The <size>
     * argument specifies the new property value size. On error, a negative
     * error code is returned as described below. On success, the number
     * of bytes which have been effectively copied to the property value is
     * returned. The returned value size can be less than the requested size
     * when the last one exceeds the maximum value size.
     */
    typedef int
(*NkPropSet) (char* name, void* value, unsigned int size);

    /*
     * The NkPropGet VLX call reads the value of a given property.
     * The <name> argument points to a null terminated ASCII string specifying
     * the property name. The <value> argument points to a buffer to which the
     * property value is copied. The <maxsize> argument specifies the
     * buffer size. On error, a negative error code is returned as described
     * below. On success, the number of bytes which have been effectively
     * copied to the buffer is returned. When the current size of the property
     * value exceeds the buffer size, only first <maxsize> bytes are copied.
     */
    typedef int
(*NkPropGet) (char* name, void* value, unsigned int maxsize);

    /*
     * The NkPropEnum VLX call returns the name and attributes
     * of the property specified by the <id> argument. 
     * The <name> argument points to a buffer to which the property name
     * is copied. The <size> argument specifies the size of the name buffer
     * in bytes. The <attr> argument specifies a 32-bit word address where
     * the property attributes are return. The property attributes are 
     * combined from the permissions and the real name length (including
     * the terminating zero). On error, a VLX_ARM_PROP_UNKNOWN is returned.
     * On success, the maximum property value size is returned.
     */
    typedef int
(*NkPropEnum) (unsigned int id, char* name, unsigned int size,
	       NkPropAttr* attr);

    /*
     * The NK_ARM_PROP_BUSY error is returned from NkPropGet/Set when
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
#define	NK_ARM_PROP_BUSY		(-1)

    /*
     * The NK_ARM_PROP_PERMISSION error is returned from NkPropGet/Set when
     * the operation is disabled for this VM by the property permissions.
     */
#define	NK_ARM_PROP_PERMISSION		(-2)
  
    /*
     * The NK_ARM_PROP_UNKNOWN error is returned from NkPropGet/Set/Enum when
     * the property name is not found in the VLX repository.
     */
#define	NK_ARM_PROP_UNKNOWN		(-3)

    /*
     * The NK_ARM_PROP_ERROR error is returned from NkPropGet/Set/Enum when
     * a fatal error detected.
     */
#define	NK_ARM_PROP_ERROR		(-4)

struct NkOsCtx {
    NkVector	 os_vectors[16];/* OS vectors */

	/*
	 * Basic OS execution context,
	 * it is used mostly during interrupt
	 * handling for resgisters save/restore
	 */	
    nku32_f	 regs[16];	/* ARM registers r0-r15 */
    nku32_f	 cpsr;		/* ARM status register */

	/*
	 * Virtual Exceptions
	 */	 
    NkVexMask	 pending;	/* pending virtual exceptions */
    NkVexMask	 enabled;	/* enabled virtual exceptions */

    NkMagic	 magic;		/* OS context magic */

	/*
	 * Extended OS execution context
	 * (saved/restored on each context switch)
	 */
    nku32_f	 sp_svc;	/* nanokernel SP */
    nku32_f	 pc_svc;	/* nanokernel PC */
    NkPhAddr	 root_dir;	/* MMU root dir (TTB0) */
    nku32_f	 domain;	/* MMU domain */
    nku32_f	 cp15;		/* cp15 control register */
    nku32_f	 cpar;		/* cp15 coprocessor access register */

    nku32_f	 acc0[2];	/* XScale accumulator 0 */

    nku32_f	 asid;		/* v6 MMU Context ID */
    nku32_f	 root1_dir;	/* v6 MMU alternative root dir (TTB1) */
    nku32_f	 thread_id[3];	/* v6 MMU User RW/RO and Privilgd Thread IDs */
    nku32_f	 ttbcr;		/* v6 MMU root dir control register */

    nku32_f	 sp_usr;	/* user mode stack pointer */
    nku32_f	 lr_usr;	/* user mode link register */
    nku32_f	 spsr_svc;	/* supervisor mode spsr */
    nku32_f	 sp_irq;	/* irq mode stack register */
    nku32_f	 lr_irq;	/* irq mode link register */
    nku32_f	 spsr_irq;	/* irq mode spsr */
    nku32_f	 sp_abt;	/* abort mode stack register */
    nku32_f	 lr_abt;	/* abort mode link register */
    nku32_f	 spsr_abt;	/* abort mode spsr */
    nku32_f	 sp_und;	/* undef mode stack register */
    nku32_f	 lr_und;	/* undef mode link register */
    nku32_f	 spsr_und;	/* undef mode spsr */

    nku32_f	 pad_regs[75];	/* space for future registers storage */

    nku32_f	 pad_vars[21];	/* space for future variables storage */

	/*
	 * "visible" OS attributes (can be accessed by OS VLX plug-in)
	 */
    NkPropSet    prop_set;	/* set property value */
    NkPropGet    prop_get;	/* get property value */
    NkPropEnum   prop_enum;	/* get property name and attributes */

    int		 xirq_limit;	/* free cross IRQs limit */
    NkTagList*	 taglist;	/* new tags list this should superseeds tags*/
    void*        current_vcpu;	/* virt address of the current vCPU pointer */
    NkPhAddr     xirqs_pending; /* XIRQs pending mask */
    NkPhAddr     xirqs_affinity;/* XIRQs affinity table */
    NkVcpuId     maxvcpus;	/* maximum number of vCPUs */
    NkVcpuId     vcpuid;	/* virtual CPU ID */
    nku32_f	 vfp_owned;	/* Boolean true if vfp is owned by current OS*/
    NkOsId	 id;		/* OS id */
    nku32_f	 arch_id;	/* ARM architecture id */
    nku32_f	 cpu_cap;	/* Hardware CPU Capabilities */
    void*	 nkvector;	/* vectors virtual address */
    NkPhAddr	 vector;	/* vectors physical address */
    NkPhAddr	 dev_info;	/* device info */
    NkPhAddr	 xirqs;		/* table of pending XIRQs */
    int		 xirq_free;	/* first free cross IRQ */
    NkPhAddr	 running;	/* pointer to the mask of running OSes */
    NkOsId	 lastid;	/* last OS ID */
    nku32_f	 ttb_flags;	/* v6 MMU TTB flags to add to each TTB */

	/*
	 * "invisible" OS attributes (used only by nanokernel itself)
	 */
    int		 mmu_owner;	/* MMU owner flag */
    NkPrio	 cur_prio;	/* current priority */
    NkPrio	 fg_prio;	/* foreground priority */
    NkPrio	 bg_prio;	/* background priority */
    NkTime	 rtime;		/* OS execution time */
    NkTime	 ptime;		/* OS execution time at sched period start */
    NkTime	 quantum;	/* OS execution quantum */
    int 	 background;	/* OS is background */

	/*
	 * VLX plug-in interface (those functions
	 * can be called by OS VLX plug-in)
	 */
    NkReady	 ready;		/* OS is ready */
    NkDbgOps	 dops;		/* debug (console) ops	*/
    NkWakeUp	 wakeup;	/* leave idle state */
    NkIdle	 idle;		/* idle */
    NkPrioSet	 prio_set;	/* set current os prio */
    NkPrioGet	 prio_get;	/* get current os prio */
    NkXIrqPost	 xpost;		/* post XIRQ */
    NkDevAlloc	 dev_alloc;	/* allocate memory for device descriptors */
    NkHistGetc	 hgetc;		/* get console history character */
    NkRestart	 restart;	/* restart OS */
    NkStop	 stop;		/* stop OS */
    NkResume	 resume;	/* resume OS */
    NkStub	 upgrade;	/* stub for depreciated function */
    NkStub	 commit;	/* stub for depreciated function */
    NkPmonOps	 pmonops;  	/* performance monitoring ops */

    NkTagList	 tags;		/* tag list (boot parameters) */
    NkPhAddr	 cmdline;	/* command line pointer       */

    NkPmemAlloc	 pmem_alloc;	/* allocate persistent communication memory */
    NkWSyncAll	 wsync_all;	/* sync data cache */
    NkWSyncEntry wsync_entry;	/* sync data cache entry */
    NkFlushAll	 flush_all;	/* flush (sync & invalidate) all caches */
    NkGetBinfo	 binfo;		/* get OS info */
    NkOsCtxGet	 osctx_get;	/* return the OS context of the given OS ID */
    NkConsOps	 cops;		/* new console operation */ 
    NkVfpGet	 vfp_get;	/* obtain the vfp */ 

    NkSmpXIrqAlloc  smp_xirq_alloc;  /* legacy XIRQ allocation */
    NkSmpDevAdd     smp_dev_add;     /* legacy device adding */

    NkSmpPdevAlloc  smp_pdev_alloc;  /* persistent device allocation */
    NkSmpPmemAlloc  smp_pmem_alloc;  /* persistent memory allocation */
    NkSmpPxirqAlloc smp_pxirq_alloc; /* persistent XIRQ allocation */

    NkSmpIrqConnect	smp_irq_connect;    /* connect to phys IRQ */
    NkSmpIrqDisconnect  smp_irq_disconnect; /* disconnect from phys IRQ */
    NkSmpIrqMask        smp_irq_mask;       /* mask phys IRQ */
    NkSmpIrqUnmask      smp_irq_unmask;     /* unmask phys IRQ */
    NkSmpIrqEoi         smp_irq_eoi;        /* end of phys interrupt */
    NkSmpIrqAffinity    smp_irq_affinity;   /* adjust xIRQ affinity */
    NkSmpIrqPost	smp_irq_post;	    /* post xIRQ */

    NkSmpCpuStart       smp_cpu_start;	    /* start application CPU */
    NkSmpCpuStop        smp_cpu_stop;	    /* stop  application CPU */

    NkSmpTime		smp_time;	    /* time source counter */
    NkSmpTimeHz		smp_time_hz;	    /* time source frequency in HZ */

    NkSmpTimerAlloc	    smp_timer_alloc;
    NkSmpTimerFree 	    smp_timer_free;
    NkSmpTimerInfo	    smp_timer_info;
    NkSmpTimerStartPeriodic smp_timer_start_periodic;
    NkSmpTimerStartOneShot  smp_timer_start_oneshot;
    NkSmpTimerStop	    smp_timer_stop;

    NkSmpYield              smp_yield;
    NkSmpRelax              smp_relax;

    NkSmpDxirqAlloc     smp_dxirq_alloc;

    NkSuspendToMemory   suspend_to_memory;	/* suspend VM to memory */
    NkPowerState        power_state;		/* system wide power state */

    NkBalloonControl    balloon_ctrl;		/* ballon memory alloc/free */ 

    nku32_f	 pad_ops[2];	/* space for future ops storage */

    nku32_f      vcpu_migdis;	/* vCPU migration disable counter */
    nku32_f      vcpu_yield;	/* vCPU yield flag */
    nku32_f      vcpu_relax;	/* vCPU relax flag */
    void*        vm;		/* points to the VM descriptor */
    void*        cpu;		/* points to physical CPU descriptor */
    void*        vfpu;		/* vFPU context */
    volatile void* vfpu_cpu;	/* remote CPU where vCPU is VFP owner */
    nku32_f      vcpu_flags;	/* vCPU flags */
    volatile int vcpu_running;	/* vCPU is running when positive (> 0) */
    nku32_f      ttime_lock;	/* transition time spin lock */
    volatile struct NkOsCtx* parent; /* parent vCPU */
    NkXIrqMask*  vcpu_pending;	/* VCPU pending IRQ mask */
    NkTime       cpu_wtime;	/* scheduling wait time on the current CPU */  
    NkTime       wtime;		/* global scheduling wait time */    
    NkTime 	 ttime;		/* vCPU state transition time */    
    NkPrio	 idle_prio;	/* idle priority */

    nku32_f	 stack[255];	/* stack data */
    nku32_f	 stack_bottom;	/* stack bottom (64-bits aligned) */
    struct NkOsCtx* vcpu_next;	/* next VCPU in the scheduling list */
/* The size of the structure _must_ be 64-bits aligned */
};

	/*
	 * memory types for mapping descriptors
	 */
#define NK_MD_RAM	0
#define NK_MD_FAST_RAM	1
#define NK_MD_ROM	2
#define NK_MD_IO_MEM	4
#define NK_MD_HIGH_RAM	8

	/*
	 * mapping descriptor
	 */
typedef struct NkMapDesc {
    NkPhAddr	pstart;		/* physical start */
    NkPhAddr	plimit;		/* physical limit */
    NkVmAddr	vstart;		/* virtual start */
    nku32_f	pte_attr;	/* pte attributes */
    nku32_f	mem_type;	/* memory type */
    NkOsId	mem_owner;	/* memory owner */
} NkMapDesc;

#define	NK_OSCTX_MAGIC()	((sizeof(NkOsCtx) << 24) | NK_OSCTX_DATE)

#endif

#endif

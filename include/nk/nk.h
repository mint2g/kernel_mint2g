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
 *  #ident  "@(#)nk.h 1.45     09/11/24 Red Bend"
 *
 *  Contributor(s):
 *    Eric Lescouet (eric.lescouet@redbend.com)
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *    Guennadi Maslov (guennadi.maslov@redbend.com)
 *
 ****************************************************************
 */

#ifndef	_NK_NK_H
#define	_NK_NK_H

    /*
     * NanoKernel DDI class name
     */
#define NK_CLASS "nanokernel"

    /*
     * Version number of the NanoKernel device driver interface.
     */
typedef enum NkVersion {
    NK_VERSION_INITIAL = 0,	/* initial version */
	/*
	 * In version 1 the following methods have been added:
	 *    nk_cpu_id_get() - returns the current NK CPU ID
	 *    nk_xvex_post() - post a virtual exception to a remote CPU
	 *    nk_local_xirq_post() - post a local cross interrrupt
	 *    nk_xirq_attach_masked() - attach "masked" cross IRQ handler
	 */
    NK_VERSION_1,		/* SMP version */
        /* In version 2 the following method have been added:
	 * nk_scheduler() - set/get secondary fair-share scheduling parameters
	 */
    NK_VERSION_2,		/* scheduler parameters */
    NK_VERSION_3,		/* stop, start, suspend, resume */
    NK_VERSION_4,		/* machine_addr */
    NK_VERSION_5,		/* monitoring interface */
    NK_VERSION_6,		/* vlink & secure_shared_memory */
    NK_VERSION_7,		/* nk_xirq_affinity() */
    NK_VERSION_8,		/* nk_balloon_ctrl() */
    NK_VERSION_9,		/* nk_prop_set/get/enum() */
} NkVersion;

    /*
     * Shared device descriptor
     * These descriptors are used to build the NanoKernel repository
     */
typedef nku32_f NkDevClass;
typedef nku32_f NkDevId;

typedef struct NkDevDesc {
    NkPhAddr	next;		/* linked list of devices      */
    NkDevClass	class_id;	/* class of device             */
    NkDevId	dev_id;		/* kind of device within class */
    NkPhAddr	class_header;	/* class  specific header      */
    NkPhAddr	dev_header;	/* device specific header      */
    NkOsId	dev_lock;	/* may be used as the global spin lock */
    NkOsId      dev_owner;	/* device owner */
} NkDevDesc;

typedef nku64_f NkMhAddr;	/* FIXME: shuld be defined in nk_f.h */

    /*
     * Vlink resource enumerator.
     */
typedef nku32_f NkResourceId;

    /*
     * Vlink resource type.
     */
typedef enum {
    NK_RESOURCE_PDEV,
    NK_RESOURCE_PMEM,
    NK_RESOURCE_PXIRQ,
} NkResourceType;

    /*
     * Vlink resource descriptor.
     */
typedef struct NkResource {
    NkPhAddr		next;	/* address of the "next" resource descriptor */
    NkResourceType	type;	/* resource type */
    NkResourceId	id;	/* resource id */
    union {
	struct {
	    NkPhAddr	addr;	/* base address of allocated device memory */
	    NkPhSize	size;   /* size of allocated device memory */
	} pdev;
	struct {
	    NkPhAddr	addr;	/* base address of allocated comm. memory */
	    NkPhSize	size;   /* size of allocated comm. memory */
	} pmem;
	struct {
	    NkOsId	osid;   /* xirq owner */
	    NkXIrq	base;   /* 1st allocated xirq */
	    int		numb;	/* number of allocated xirq's */
	} pxirq;
    } r;
} NkResource;

    /*
     * Cache synchronization for DMA transfers.
     *
     * NK_CACHE_SYNC_READ:  synchronize the cache before starting a DMA read
     *			    operation from memory (typically write-back dirty
     *			    source cache lines).
     * NK_CACHE_SYNC_WRITE: synchronize the cache before starting a DMA write
     *			    operation to memory (typically invalidate
     *			    destination cache lines).
     */
typedef nku32_f NkCacheSync;

#define NK_CACHE_SYNC_READ	(1 << 0)
#define NK_CACHE_SYNC_WRITE	(1 << 1)
#define NK_CACHE_SYNC_RW	(NK_CACHE_SYNC_READ | NK_CACHE_SYNC_WRITE)

    /*
     * Virtual Machine properties.
     *
     * Such properties can be used by the virtual drivers to dynamically
     * adapt their behavior to the capabilities of the underlying virtual
     * machine monitor.
     */

    /*
     * Type of properties.
     */
typedef enum NkInfo {
    NK_INFO_VM = 0,			   /* VM/guest properties */
    NK_INFO_CACHE,			   /* Cache properties */
} NkInfo;

    /*
     * VM properties.
     */
typedef nku32_f NkVmInfo;

    /*
     * Definitions of VM properties bits.
     */    
#define NK_VM_PSEUDO_PHYSMEM	(1 << 0)   /* Physical memory is translated */
#define NK_VM_ISOLATED		(1 << 1)   /* Guest is isolated */
#define NK_VM_ALLMEM_MAPPABLE	(1 << 2)   /* Guest can map memory of others */
#define NK_VM_SLOW_DMA		(1 << 3)   /* DMA ops slower than memcpy ops */

    /*
     * Cache properties.
     */
typedef nku32_f NkCacheArch;		   /* Cache architecture */
typedef nku32_f NkCacheLevel;		   /* Cache level */
typedef nku32_f NkCacheSize;		   /* Cache size */
typedef nku32_f NkCacheWays;               /* Number of cache ways */

    /*
     * Definitions of cache architecture bits.
     */
#define NK_CACHE_PIPT		(1 << 0)   /* Cache is PIPT */
#define NK_CACHE_VIPT		(1 << 1)   /* Cache is VIPT */
#define NK_CACHE_VIVT		(1 << 2)   /* Cache is VIVT */
#define NK_CACHE_PIVT		(1 << 3)   /* Cache is PIVT (unused) */
#define NK_CACHE_INST		(1 << 4)   /* Cache is for instructions */
#define NK_CACHE_DATA		(1 << 5)   /* Cache is for data */
#define NK_CACHE_UNIFIED	(NK_CACHE_INST | NK_CACHE_DATA)
#define NK_CACHE_WB		(1 << 6)   /* Cache is write-back - Otherwise
					    * cache is write-through.
					    */
#define NK_CACHE_PG_COLORING	(1 << 7)   /* Cache uses page coloring */

    /*
     * Cache descriptor.
     */
typedef struct NkCacheInfo {
    NkCacheLevel level;			   /* Cache level */
    NkCacheArch  arch;			   /* Cache architecture */
    NkCacheSize  size;			   /* Cache size */
    NkCacheSize  line;			   /* Cache line size */
    NkCacheWays  nways;			   /* Number of cache ways */
} NkCacheInfo;

#define NK_CACHE_LIMIT		6	   /* L1 I/D + L2 I/D + L3 I/D */

    /*
     * Descriptors for all present caches.
     */
typedef struct NkCachesInfo {
    int		 limit;			   /* Number of caches */
    NkCacheInfo	 cache[NK_CACHE_LIMIT];	   /* Cache descriptors */
} NkCachesInfo;

    /*
     * The NanoKernel hardware interrupts provide a common shared
     * representation of the hardware interrupts, between the different
     * operating systems.
     * In other words, each system kernel maps its own interrupt
     * representation to/from the NanoKernel one.
     */
typedef nku32_f NkHwIrq;

    /*
     * Maximum number of HW irq per virtualized device (PIC)
     */
#define NK_HW_IRQ_LIMIT	32


    /*
     * NanoKernel cross interrupt handlers.
     *
     * The <cookie> argument is an opaque given by the client when calling
     * irq_attach(). It is passed back when called.
     *
     * May be called from hardware interrupt context
     */
typedef void (*NkXIrqHandler) (void* cookie, NkXIrq xirq);
    /*
     * Identifiers of xirq handler's attachement
     */
typedef void* NkXIrqId;

    /*
     * System (re-)configuration cross interrupt
     */
#define	NK_XIRQ_SYSCONF	NK_HW_XIRQ_LIMIT
    /*
     * First free cross interrupt
     */
#define	NK_XIRQ_FREE	(NK_XIRQ_SYSCONF + 1)

    /*
     * Structures used to implement the VLX logging system
     */
typedef nku32_f NkLogSize;
typedef nku64_f NkLogTime;
typedef nku32_f NkLogData;
typedef nku32_f NkLogEvents;

typedef struct NkLogHeader {
    NkLogSize size;      /* buffer size in bytes (including header)      */
    NkLogSize end;       /* LOG_END record offset in bytes (with header) */
    nku32_f   roll_over; /* buffer roll-over counter                     */
    nku8_f    hdr_size;  /* header size in bytes                         */
    nku8_f    rec_size;  /* record size in bytes                         */
    nku8_f    cpu;       /* physical CPU ID                              */
    nku8_f    flags;     /* unused (for future extensions)               */
    nku64_f   timer_hz;  /* timer frequency HZ at buffer plug time       */
    NkLogTime time;      /* buffer plug time stamp                       */
} NkLogHeader;

typedef struct NkLogRecord {
    nku8_f    event; /* event type                             */
    nku8_f    cpu;   /* physical CPU ID                        */
    nku8_f    guest; /* guest ID                               */
    nku8_f    vcpu;  /* virtual CPU ID (always 0 for UP guest) */
    NkLogData data;  /* event specific data                    */
    NkLogTime time;  /* event time                             */
} NkLogRecord;

    /*
     * All events that are logged by VLX.
     * Each event group is limited to 8 units.
     */

#define LOG_GROUP_BASIC  0
#define LOG_GROUP_SCHED  1
#define LOG_GROUP_VM     2
#define LOG_GROUP_GUEST  3
#define LOG_GROUP_VCPU   4
#define LOG_GROUP_SIRQ   5
#define LOG_GROUP_HIRQ   6
#define LOG_GROUP_EXCEP  7
#define LOG_GROUP_VMMU   8
#define LOG_GROUP_VFPU   9
#define LOG_GROUP_PIO    10
#define LOG_GROUP_MEMIO  11
#define LOG_GROUP_PCI    12
#define LOG_GROUP_WDT    13

#define __LOG_EVENT(group, event)  (((group) << 3) + (event))

#define LOG_NOP                                 __LOG_EVENT(LOG_GROUP_BASIC, 0)
#define LOG_CPU_PLUG                            __LOG_EVENT(LOG_GROUP_BASIC, 1)
#define LOG_CPU_UNPLUG                          __LOG_EVENT(LOG_GROUP_BASIC, 2)
#define LOG_TIMER_CALIBRATE_START               __LOG_EVENT(LOG_GROUP_BASIC, 3)
#define LOG_TIMER_CALIBRATE_STOP                __LOG_EVENT(LOG_GROUP_BASIC, 4)
#define LOG_END                                 __LOG_EVENT(LOG_GROUP_BASIC, 5)

#define LOG_SCHED_VCPU_RUN                      __LOG_EVENT(LOG_GROUP_SCHED, 0)
#define LOG_SCHED_CPU_HALT                      __LOG_EVENT(LOG_GROUP_SCHED, 1)
#define LOG_SCHED_CPU_RUN                       __LOG_EVENT(LOG_GROUP_SCHED, 2)

#define LOG_VMENTER                             __LOG_EVENT(LOG_GROUP_VM, 0)
#define LOG_VMEXIT                              __LOG_EVENT(LOG_GROUP_VM, 1)

#define LOG_GUEST_START                         __LOG_EVENT(LOG_GROUP_GUEST, 0)
#define LOG_GUEST_STOP                          __LOG_EVENT(LOG_GROUP_GUEST, 1)
#define LOG_GUEST_SUSPEND                       __LOG_EVENT(LOG_GROUP_GUEST, 2)
#define LOG_GUEST_RESUME                        __LOG_EVENT(LOG_GROUP_GUEST, 3)

#define LOG_VCPU_START                          __LOG_EVENT(LOG_GROUP_VCPU, 0)
#define LOG_VCPU_STOP                           __LOG_EVENT(LOG_GROUP_VCPU, 1)
#define LOG_VCPU_HALT                           __LOG_EVENT(LOG_GROUP_VCPU, 2)
#define LOG_VCPU_SUSPEND                        __LOG_EVENT(LOG_GROUP_VCPU, 3)
#define LOG_VCPU_RESUME                         __LOG_EVENT(LOG_GROUP_VCPU, 4)

#define LOG_XIRQ_POST                           __LOG_EVENT(LOG_GROUP_SIRQ, 0)
#define LOG_XIRQ_INJECT                         __LOG_EVENT(LOG_GROUP_SIRQ, 1)

#define LOG_IRQ_INTERCEPT                       __LOG_EVENT(LOG_GROUP_HIRQ, 0)
#define LOG_IRQ_POST                            __LOG_EVENT(LOG_GROUP_HIRQ, 1)
#define LOG_IRQ_INJECT                          __LOG_EVENT(LOG_GROUP_HIRQ, 2)

#define LOG_EXCEP_INTERCEPT                     __LOG_EVENT(LOG_GROUP_EXCEP, 0)
#define LOG_EXCEP_INJECT                        __LOG_EVENT(LOG_GROUP_EXCEP, 1)

#define LOG_VMMU_ROOT                           __LOG_EVENT(LOG_GROUP_VMMU, 0)
#define LOG_VMMU_MISS                           __LOG_EVENT(LOG_GROUP_VMMU, 1)
#define LOG_VMMU_FLUSH                          __LOG_EVENT(LOG_GROUP_VMMU, 2)

#define LOG_VFPU_ACQUIRE                        __LOG_EVENT(LOG_GROUP_VFPU, 0)
#define LOG_VFPU_RELEASE                        __LOG_EVENT(LOG_GROUP_VFPU, 1)

#define LOG_PIO_IN                              __LOG_EVENT(LOG_GROUP_PIO, 0)
#define LOG_PIO_OUT                             __LOG_EVENT(LOG_GROUP_PIO, 1)

#define LOG_MEMIO_LOAD                          __LOG_EVENT(LOG_GROUP_MEMIO, 0)
#define LOG_MEMIO_STORE                         __LOG_EVENT(LOG_GROUP_MEMIO, 1)

#define LOG_PCI_CFG_LOAD                        __LOG_EVENT(LOG_GROUP_PCI, 0)
#define LOG_PCI_CFG_STORE                       __LOG_EVENT(LOG_GROUP_PCI, 1)

#define LOG_WDT_START                           __LOG_EVENT(LOG_GROUP_WDT, 0)
#define LOG_WDT_EXPIRED1                        __LOG_EVENT(LOG_GROUP_WDT, 1)
#define LOG_WDT_EXPIRED2                        __LOG_EVENT(LOG_GROUP_WDT, 2)
#define LOG_WDT_EXPIRED3                        __LOG_EVENT(LOG_GROUP_WDT, 3)
#define LOG_WDT_STOP                            __LOG_EVENT(LOG_GROUP_WDT, 4)

    /*
     * NanoKernel generic DDI operations
     */
typedef struct NkDevOps {
    
    NkVersion nk_version;	/* DDI version */

            /*
	     * Lookup first device of given class, into NanoKernel repository
	     *
	     * Return value:
	     *
	     * if <curr_dev> is zero the first intance of a device of class
	     * <dev_class> is returned. Otherwise <curr_dev> must be an
	     * address returned by a previous call to dev_lookup_xxx().
	     * The next device of class <dev_class>, starting from <curr_dev>
	     * is returned in that case.
	     *
	     * NULL is returned, if no device of the required class is found.
	     */
        NkPhAddr
    (*nk_dev_lookup_by_class) (NkDevClass dev_class, NkPhAddr curr_dev);
            /*
	     * Lookup first device of given type, into NanoKernel repository.
	     *
	     * Return value:
	     *
	     * if <curr_dev> is zero the first intance of a device of type
	     * <dev_type> is returned. Otherwise <curr_dev> must be an
	     * address returned by a previous call to dev_lookup_xxx().
	     * The next device of class <dev_type>, starting from <curr_dev>
	     * is returned in that case.
	     *
	     * NULL is returned, if no device of the required type is found.
	     */
        NkPhAddr
    (*nk_dev_lookup_by_type) (NkDevId dev_id, NkPhAddr curr_dev);

            /*
	     * Allocates a contiguous memory block from NanoKernel repository
	     */
        NkPhAddr
    (*nk_dev_alloc) (NkPhSize size);

            /*
	     * Add a new device to NanoKernel repository.
	     *
	     * <dev> is a physical address previously returned by
	     * dev_alloc(). It must points to a completed NkDevDesc structure.
	     */
        void
    (*nk_dev_add) (NkPhAddr dev);

            /*
	     * Get self operating system ID
	     */
        NkOsId
    (*nk_id_get) (void);

            /*
	     * Get last running operating system ID
	     * (eg highest possible value for an ID)
	     */
        NkOsId
    (*nk_last_id_get) (void);

            /*
	     * Get a mask of started operating system's ID
	     * -> bit <n> is '1' if system with NkOsId <n> is started.
	     * -> bit <n> is '0' if system with NkOsId <n> is stopped.
	     */
        NkOsMask
    (*nk_running_ids_get) (void);

            /*
	     * Convert a physical address into a virtual one.
	     */
        void*
    (*nk_ptov) (NkPhAddr paddr);

            /*
	     * Convert a virtual address into a physical one.
	     */
        NkPhAddr
    (*nk_vtop) (void* vaddr);

        /*
	 * Get the "virtualized exception" mask value to be used to
	 * post a given hardware interrupt.
	 */
        nku32_f
    (*nk_vex_mask) (NkHwIrq hirq);

        /*
	 * Get physical address to be used by another system to post a
	 * "virtualized exception" to current system
	 * (eg: where to write the mask returned from vex_mask_get()).
	 */
        NkPhAddr
    (*nk_vex_addr) (NkPhAddr dev);

        /*
	 * Allocates required number of contiguous unused cross-interrupts.
	 *
	 * Return the number of the first interrupt of allocated range,
	 * or 0 if not enough xirq are available.
	 */
        NkXIrq
    (*nk_xirq_alloc) (int nb);

        /*
	 * Attach a handler to a given NanoKernel cross-interrupt.
	 * (0 returned on failure)
	 * Must be called from base level.
	 * The handler is called with ONLY masked cross interrupt source.
	 */
        NkXIrqId
    (*nk_xirq_attach) (NkXIrq        xirq,
		       NkXIrqHandler hdl,
		       void*         cookie);

        /*
	 * Mask a given cross-interrupt
	 */
        void
    (*nk_xirq_mask) (NkXIrq xirq);

        /*
	 * Unmask a given cross-interrupt
	 */
        void
    (*nk_xirq_unmask) (NkXIrq xirq);

        /*
	 * Detach a handler (previously attached with irq_attach())
	 *
	 * Must be called from base level
	 */
        void
    (*nk_xirq_detach) (NkXIrqId id);

        /*
	 * Trigger a cross-interrupt to a given operating system.
	 *
	 * Must be called from base level
	 */
        void
    (*nk_xirq_trigger) (NkXIrq xirq, NkOsId osId);

        /*
	 * Get high priority bit set in bit mask.
	 */
        nku32_f
    (*nk_mask2bit) (nku32_f mask);

        /*
	 * Insert a bit of given priority into a mask.
	 */
        nku32_f
    (*nk_bit2mask) (nku32_f bit);

        /*
	 * Atomic operation to clear bits within a bit field.
	 *
	 * The following logical operation: *mask &= ~clear
	 * is performed atomically.
	 */
        void
    (*nk_atomic_clear) (volatile nku32_f* mask, nku32_f clear);

        /*
	 * Atomic operation to clear bits within a bit field.
	 *
	 * The following logical operation: *mask &= ~clear
	 * is performed atomically.
	 *
	 * Returns 0 if and only if the result is zero.
	 */
        nku32_f
    (*nk_clear_and_test) (volatile nku32_f* mask, nku32_f clear);

        /*
	 * Atomic operation to set bits within a bit field.
	 *
	 * The following logical operation: *mask |= set
	 * is performed atomically
	 */
        void
    (*nk_atomic_set) (volatile nku32_f* mask, nku32_f set);

        /*
	 * Atomic operation to substract value to a given memory location.
	 *
	 * The following logical operation: *ptr -= val 
	 * is performed atomically.
	 */
        void
    (*nk_atomic_sub) (volatile nku32_f* ptr, nku32_f val);

        /*
	 * Atomic operation to substract value to a given memory location.
	 *
	 * The following logical operation: *ptr -= val 
	 * is performed atomically.
	 *
	 * Returns 0 if and only if the result is zero.
	 */
        nku32_f
    (*nk_sub_and_test) (volatile nku32_f* ptr, nku32_f val);

        /*
	 * Atomic operation to add value to a given memory location.
	 *
	 * The following logical operation: *ptr += val
	 * is performed atomically
	 */
        void
    (*nk_atomic_add) (volatile nku32_f* ptr, nku32_f val);

        /*
	 * Map a physical address range of "shared" memory 
	 * into supervisor space (RAM from other systems).
	 *
	 * On error, NULL is returned.
	 */
        void*
    (*nk_mem_map) (NkPhAddr paddr, NkPhSize size);
        /*
	 * Unmap memory previously mapped using nk_mem_map.
	 */
        void
    (*nk_mem_unmap) (void* vaddr, NkPhAddr paddr, NkPhSize size);

	/*
	 * Get NK CPU ID.
	 */
	NkCpuId
    (*nk_cpu_id_get) (void);

        /*
	 * Post a virtual exception (VEX) to a given operating system running
	 * on a given CPU.
	 * Must be called from base level
	 */
	void
    (*nk_xvex_post) (NkCpuId cpuid, NkOsId osid, NkVex vex);

        /*
	 * Post a cross-interrupt to a given operating system running
	 * on the same CPU.
	 * Must be called from base level
	 */
        void
    (*nk_local_xirq_post) (NkXIrq xirq, NkOsId osId);

        /*
	 * Attach a handler to a given NanoKernel cross-interrupt.
	 * (0 returned on failure)
	 * Must be called from base level.
	 * The handler is called with ALL CPU interrupts masked.
	 */
        NkXIrqId
    (*nk_xirq_attach_masked) (NkXIrq        xirq,
		              NkXIrqHandler hdl,
		              void*         cookie);

	/*
	 * Get scheduling paramteres of a given operating system 
	 */
	void
    (*nk_scheduler) (NkOsId osId, NkSchedParams* oldp, 
                     NkSchedParams* newp);

	/*
	 * Stop guest operating system.
	 */
	void
    (*nk_stop) (NkOsId osId);

	/*
	 * Start guest operating system.
	 */
	void
    (*nk_start) (NkOsId osId);

	/*
	 * Suspend guest operating system.
	 */
	void
    (*nk_suspend) (NkOsId osId);

	/*
	 * Resume guest operating system.
	 */
	void
    (*nk_resume) (NkOsId osId);

	/*
	 * Convert guest physical address to the machine (real) one.
	 * When the guest address is invalid, the result is implementation
	 * specific.
	 */
	NkMhAddr
    (*nk_machine_addr) (NkPhAddr addr);

        /*
         * Plug a new buffer to the VLX logging system.
         */
        int
    (*nk_log_buffer) (NkCpuId cpu, NkPhAddr base, NkLogSize size);

        /*
         * Tell VLX to log only events related to specific guests.
	 * The guests argument specifies a bit-mask of guests for which
	 * events are logged.
         */
        void
    (*nk_log_guests) (NkOsMask guests);

        /*
         * Enable/Disable logging of specific events.
	 * The events argument specifies a bit-mask of event groups being
	 * monitored.
         */
        void
    (*nk_log_events) (NkLogEvents events);

	/*
	 * Get command line for a given host
	 */
	int
    (*nk_cmdline_get) (NkOsId id, char* buffer, nku32_f size);

	/*
	 * Lookup first virtual communication link of given a class/name
	 * into NanoKernel repository.
	 *
	 * Return value:
	 *
	 * if <plnk> is zero the first intance of a virtual link with
	 * a required <name> is returned. Otherwise <plnk> must be an
	 * address returned by a previous call to nk_vlink_lookup().
	 * The next virtual link with required <name>, starting from <plnk>
	 * is returned in that case.
	 *
	 * NULL is returned, if no virtual link with required <name> is found.
	 */
	NkPhAddr
    (*nk_vlink_lookup) (const char* name, NkPhAddr plnk);

	/*
	 * Allocate <size> bytes of contiguous memory from the persistent
	 * device repository.
	 *
	 * The allocated memory block is labelled using <vlink, id>.
	 * It is guaranteed that for a uniq label, a uniq memory block
	 * is allocated. Thus different calls with the same label always
	 * return the same result.
	 *
	 * Return the physical base address of the allocated memory block,
	 * or 0 on failure.
	 */
	NkPhAddr
    (*nk_pdev_alloc) (NkPhAddr vlink, NkResourceId id, NkPhSize size);

	/*
	 * Allocate <size> bytes (rounded up to the nearest multiple of
	 * page size) of contiguous memory from the persistent communication
	 * memory pool.
	 *
	 * The allocated memory block is labelled using <vlink, id>.
	 * It is guaranteed that for a uniq label, a uniq memory block
	 * is allocated. Thus different calls with the same label always
	 * return the same result.
	 *
	 * Return the physical base address of the allocated memory block
	 * (aligned to a page boundary), or 0 on failure.
	 */
	NkPhAddr
    (*nk_pmem_alloc) (NkPhAddr vlink, NkResourceId id, NkPhSize size);

	/*
	 * Allocate <nb> contiguous persistent cross-interrupts.
	 *
	 * The allocated xirqs range is labelled using <vlink, id>.
	 * It is guaranteed that for a uniq label, a uniq xirqs range
	 * is allocated. Thus different calls with the same label always
	 * return the same result.
	 *
	 * Return the number of the first allocated xirq or 0 if not enough
	 * xirq are available.
	 */
	NkXIrq
    (*nk_pxirq_alloc) (NkPhAddr vlink, NkResourceId id, NkOsId osid, int nb);

	/*
	 * Copy <size> bytes from the memory of the current guest at virtual
	 * address <src> into the memory of another guest which resides at
	 * physical address <dst>.
	 *
	 * Return the number of successfully copied bytes.
	 */
	NkPhSize
    (*nk_mem_copy_to) (NkPhAddr dst, void* src, NkPhSize size);

	/*
	 * Copies <size> bytes from the memory of another guest which resides
	 * at physical address <src> into the memory of the current guest at
	 * virtual address <dst>.
	 *
	 * Return the number of successfully copied bytes.
	 */
	NkPhSize
    (*nk_mem_copy_from) (void* dst, NkPhAddr src, NkPhSize size);

	/*
	 * Map a physical address range of "shared" memory (RAM from
	 * other systems) that is suitable for DMA transfers (according
	 * to the given cache synchronization constraints <func>),
	 * into the supervisor space.
	 *
	 * Return the virtual address of the mapped area, or NULL on failure.
	 */
	void*
    (*nk_mem_map_sync) (NkPhAddr paddr, NkPhSize size, NkCacheSync func);

	/*
	 * Unmap memory previously mapped using nk_mem_map_sync.
	 */
	void
    (*nk_mem_unmap_sync) (void* vaddr, NkPhAddr paddr, NkPhSize size);

	/*
	 * Synchronize caches before starting a DMA transfer.
	 */
	void
    (*nk_cache_sync) (void* vaddr, NkPhSize size, NkCacheSync func);

	/*
	 * Get properties of the current virtual machine.
	 *
	 * The first parameter is the type of properties.
	 * The <info> parameter is an untyped pointer to memory where
	 * the requested properties are written.
	 * The <size> parameter specifies the size of the <info> buffer
	 * allocated by the caller.
	 *
	 * Return the number of bytes needed to write the requested
	 * properties, or 0 on an invalid properties type.
	 * Note that a value which is greater than the specified size
	 * can be returned. In this case, the caller should re-invoke
	 * nk_info with a bigger buffer.
	 */
	unsigned int
    (*nk_info) (NkInfo type, void* info, unsigned int size);

        /*
	 * Set vCPUs affinity for a given cross-interrupt
	 */
        void
    (*nk_xirq_affinity) (NkXIrq xirq, NkCpuMask cpus);

        /*
	 * Memory ballooning control.
	 */
        nku32_f
    (*nk_balloon_ctrl) (int op, nku32_f* pfns, nku32_f count);

	/*
	 * The nk_prop_set DDI call updates the value of a given property
	 * and sends a notification event. The <name> argument points to
	 * a null terminated ASCII string specifying the property name.
	 * The <value> argument points to the new property value. The <size>
	 * argument specifies the new property value size. On error, a negative
	 * error code is returned as described below. On success, the number
	 * of bytes which have been effectively copied to the property value is
	 * returned. The returned value size can be less than the requested
	 * size when the last one exceeds the maximum value size.
	 */
        int
    (*nk_prop_set) (char* name, void* value, unsigned int size);

	/*
	 * The nk_prop_get DDI call reads the value of a given property.
	 * The <name> argument points to a null terminated ASCII string
	 * specifying the property name. The <value> argument points to
	 * a buffer to which the property value is copied. The <maxsize>
	 * argument specifies the buffer size. On error, a negative error
	 * code is returned as described below. On success, the number of
	 * bytes which have been effectively copied to the buffer is
	 * returned. When the current size of the property value exceeds
	 * the buffer size, only first <maxsize> bytes are copied.
	 */
        int
    (*nk_prop_get) (char* name, void* value, unsigned int maxsize);

        /*
         * The nk_prop_enum VLX call returns the name and attributes
         * of the property specified by the <id> argument. 
         * The <name> argument points to a buffer to which the property name
         * is copied. The <size> argument specifies the size of the name buffer
         * in bytes. The <attr> argument specifies a 32-bit word address where
         * the property attributes are return. The property attributes are 
         * combined from the permissions and the real name length (including
         * the terminating zero). On error, a negative error code is returned.
         * On success, the maximum property value size is returned.
         */
        int
    (*nk_prop_enum) (unsigned int id, char* name, unsigned int size,
	             NkPropAttr* attr);

} NkDevOps;

    /*
     * space allocators used for various memory and I/O spaces.
     * (RAM, bus memory or I/O spaces)
     */
typedef nku32_f NkSpcTag;

#define NK_SPC_FREE		0x00000001 /* Available space */
#define NK_SPC_ALLOCATED	0x00000002 /* Allocated space */
#define NK_SPC_RESERVED		0x00000004 /* Reserved  space */
#define NK_SPC_NONEXISTENT	0x00000010 /* Hole in space   */

#define NK_SPC_STATE(tag)	(tag & 0xffff)
#define NK_SPC_OWNER(tag)	(((tag) >> 16) & (NK_OS_LIMIT-1))
#define NK_SPC_TAG(s, id)	((((id) & (NK_OS_LIMIT-1)) << 16) | s)

#define NK_SPC_MAX_CHUNK 128	/* maximum chunk number per descriptor */

    /*
     * 32 bits space allocator
     */
typedef struct NkSpcChunk_32 {
    nku32_f  start;
    nku32_f  size;
    NkSpcTag tag;
} NkSpcChunk_32;

typedef struct NkSpcDesc_32 {
    nku32_f		maxChunks;
    nku32_f		numChunks;
    NkSpcChunk_32	chunk[NK_SPC_MAX_CHUNK];
} NkSpcDesc_32;

extern void    nk_spc_init_32 (NkSpcDesc_32* desc);
extern void    nk_spc_tag_32  (NkSpcDesc_32* desc,
			       nku32_f       addr,
			       nku32_f       size,
			       NkSpcTag      tag);
extern void    nk_spc_free_32 (NkSpcDesc_32* desc,
			       nku32_f       addr,
			       nku32_f       size);
extern int     nk_spc_alloc_any_32 (NkSpcDesc_32* desc,
				    nku32_f*      addr,
				    nku32_f       size,
				    nku32_f       align,
				    NkSpcTag       tag);
extern int     nk_spc_alloc_within_range_32 (NkSpcDesc_32* desc,
					     nku32_f*      addr,
					     nku32_f       size,
					     nku32_f       align,
					     nku32_f       lo,
					     nku32_f       hi,
					     NkSpcTag      owner);
extern int     nk_spc_alloc_32 (NkSpcDesc_32* desc,
				nku32_f       addr,
				nku32_f       size,
				NkSpcTag      owner);
extern void    nk_spc_release_32 (NkSpcDesc_32* desc, NkSpcTag tag);
extern nku32_f nk_spc_size_32 (NkSpcDesc_32* desc);
extern void    nk_spc_dump_32 (NkSpcDesc_32* desc);

    /*
     * 64 bits space allocator
     */
typedef struct NkSpcChunk_64 {
    nku64_f  start;
    nku64_f  size;
    NkSpcTag tag;
} NkSpcChunk_64;

typedef struct NkSpcDesc_64 {
    nku32_f		maxChunks;
    nku32_f		numChunks;
    NkSpcChunk_64	chunk[NK_SPC_MAX_CHUNK];
} NkSpcDesc_64;

extern void    nk_spc_init_64 (NkSpcDesc_64* desc);
extern void    nk_spc_tag_64  (NkSpcDesc_64* desc,
			       nku64_f       addr,
			       nku64_f       size,
			       NkSpcTag      tag);
extern void    nk_spc_free_64 (NkSpcDesc_64* desc,
			       nku64_f       addr,
			       nku64_f       size);
extern int     nk_spc_alloc_any_64 (NkSpcDesc_64* desc,
				    nku64_f*      addr,
				    nku64_f       size,
				    nku64_f       align,
				    NkSpcTag       tag);
extern int     nk_spc_alloc_within_range_64 (NkSpcDesc_64* desc,
					     nku64_f*      addr,
					     nku64_f       size,
					     nku64_f       align,
					     nku64_f       lo,
					     nku64_f       hi,
					     NkSpcTag      owner);
extern int     nk_spc_alloc_64 (NkSpcDesc_64* desc,
				nku64_f       addr,
				nku64_f       size,
				NkSpcTag      owner);
extern void    nk_spc_release_64 (NkSpcDesc_64* desc, NkSpcTag tag);
extern nku64_f nk_spc_size_64 (NkSpcDesc_64* desc);
extern void    nk_spc_dump_64 (NkSpcDesc_64* desc);

#endif

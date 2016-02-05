/*
 ****************************************************************
 *
 *  Component: VLX nano-kernel device driver interface (NK DDI)
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
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *    Guennadi Maslov (guennadi.maslov@redbend.com)
 *    Chi Dat Truong (chidat.truong@redbend.com)
 *    Eric Lescouet (eric.lescouet@redbend.com)
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

/*
 * This driver provides the generic NKDDI services to NK specific drivers
 * when the Linux kernel runs on top of the nano-kernel and it plays
 * ether primary or secondary role.
 */

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kallsyms.h>
#include <linux/kobject.h>

#include <asm/pgtable-hwdef.h>

#include <nk/nkern.h>
#include <asm/nkern.h>
#include <mach/pm_debug.h>
/*
 * This driver provides the generic NKDDI to NK specific drivers
 */

#define	NKCTX()		os_ctx

#define DKI_PANIC(msg)  do { \
			  printnk ("NKDDI: "); printnk msg; while (1) {} \
			} while (0)
#define DKI_ERROR(msg)  do { \
			  printnk ("NKDDI: "); printnk msg; \
			} while (0)

/*
 * All shared between OS'es memory is accessed through
 * compatible mapping described by nk_maps
 */

#define	PTOV(x)	_ptov(x)
#define	VTOP(x)	_vtop(x)

/*
 * Do we want to check some (a few) cases of API misuse ?
 */
#ifdef CONFIG_NKERNEL_DDI_APICHECK
#define NKDDI_VOID_ACHECK(param) \
    __asm__ __volatile__( \
"	cmp	%0, #0\n" \
"	bxeq	lr" \
    : \
    : "r" (param) \
    : "cc")
#define NKDDI_VOID_CCHECK(param) if (0==param) return
#define NKDDI_OREG_ACHECK(param) \
    __asm__ __volatile__( \
"	cmp	%0, #0\n" \
"	mvneq r0, #0\n" \
"	bxeq	lr" \
    : \
    : "r" (param) \
    : "cc")
#define NKDDI_OREG_CCHECK(param) if (0==param) return ~0
#define NKDDI_ZREG_CCHECK(param) if (0==param) return 0
#else/*CONFIG_NKERNEL_DDI_APICHECK*/
#define NKDDI_VOID_ACHECK(param)
#define NKDDI_VOID_CCHECK(param)
#define NKDDI_OREG_ACHECK(param)
#define NKDDI_OREG_CCHECK(param)
#define NKDDI_ZREG_CCHECK(param)
#endif/*CONFIG_NKERNEL_DDI_APICHECK*/


extern NkMapDesc nk_maps[];
extern int       nk_maps_max;

    static int
_is_nk_smp (void)
{
    NkMapDesc*	map;
    NkMapDesc*  map_limit = &nk_maps[nk_maps_max];
    for (map  = &nk_maps[0]; map < map_limit; map++) {
	if ((map->mem_type == NK_MD_RAM) && (map->pte_attr & PMD_SECT_S)) {
	    return 1;
	}
    }
    return 0;
}

    /*
     * physical to virtual and virtual to physical
     * address conversions.
     */
    static void*
_ptov (NkPhAddr paddr)
{
    NkMapDesc*	map;
    NkMapDesc*  map_limit = &nk_maps[nk_maps_max];

    for (map  = &nk_maps[0]; map < map_limit; map++) {
	if ((map->pstart <= paddr) && (paddr <= map->plimit)) {
	    return (void*)(paddr - map->pstart + map->vstart);
	}
    }

    DKI_PANIC(("no virtual address for physical addr 0x%08x\n", paddr));
    return 0;
}

    static NkPhAddr
_vtop (void* addr)
{
    NkVmAddr    vaddr = (NkVmAddr)addr;
    NkMapDesc*	map;
    NkMapDesc*  map_limit = &nk_maps[nk_maps_max];

    for (map  = &nk_maps[0]; map < map_limit; map++) {
	NkVmAddr vlimit = map->vstart + (map->plimit - map->pstart);
	if ((map->vstart <= vaddr) && (vaddr <= vlimit)) {
	    return (vaddr - map->vstart + map->pstart);
	}
    }

    DKI_PANIC(("no physical address for virtual addr 0x%08x\n", addr));
    return 0;
}

typedef struct NkDev {
    NkXIrqMask*  xirqs;				   /* XIRQs table */
    NkXIrqMask*  xpending[NR_CPUS];		   /* pending XIRQs */
    spinlock_t   xirq_b_lock;			   /* base lock */
    NkOsMask*    running;			   /* mask of running OSes */
    NkXIrqMask	 xenabled;		 	   /* XIRQ enable mask  */
    NkCpuMask*   affinity;			   /* XIRQ affinity */
} NkDev;

static NkDev nkdev;

/*
 * Notification call-back
 */

static void (*_dev_notify)(void) = 0;

    int
nk_dev_notification_register (void (*handler) (void))
{
    if (_dev_notify) {
	return 0;
    }
    _dev_notify = handler;
    return 1;
}

    static void
nk_dev_notification (void)
{
    if (_dev_notify) {
        _dev_notify();
    }
}

    /*
     * Interrupt mask/unmask functions.
     * They are mainly used to implement atomic operations.
     */
    static inline nku32_f
_intr_mask (void)
{
    nku32_f cpsr;
    nku32_f temp;

    __asm__ __volatile__(
	"mrs	%0, cpsr	\n\t"
	"orr	%1, %0, #0x80	\n\t"
	"msr	cpsr_c, %1	\n\t"
	: "=r" (cpsr), "=r" (temp)
    );
    return cpsr;
}

    static inline void
_intr_unmask (nku32_f cpsr)
{
    __asm__ __volatile__(
	"msr	cpsr_c, %0	\n\t"
	:: "r" (cpsr)
    );
}

/*
 * Interface methods...
 */

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
    static unsigned int
nk_info (NkInfo type, void* info, unsigned int size)
{
    if (type == NK_INFO_VM) {
	if (size >= sizeof(NkVmInfo)) {
	    *(NkVmInfo*)info =	0 * NK_VM_PSEUDO_PHYSMEM +
				0 * NK_VM_ISOLATED +
				1 * NK_VM_ALLMEM_MAPPABLE +
				0 * NK_VM_SLOW_DMA;
	}
	return sizeof(NkVmInfo);
    }

    if (type == NK_INFO_CACHE) {
	DKI_PANIC(("nk_info NK_INFO_CACHE queried \n"));
    }

    return 0;
}

    /*
     * Get the "virtualized exception" mask value to be used to
     * post a processor exception.
     */
    static nku32_f
nk_vex_mask (NkHwIrq irq)
{
    return NK_VEX_IRQ;
}

    /*
     * Get physical address to be used by another system to post a
     * "virtualized exception" to current system
     * (where to write the mask returned from vexcep_mask()).
     */
    static NkPhAddr
nk_vex_addr (NkPhAddr dev)
{
    return VTOP(&(NKCTX()->pending));
}

#define	VDEV(x)	((NkDevDesc*) PTOV(x))
#define	VLNK(x)	((NkDevVlink*)PTOV(x))

    /*
     * Lookup first device of given class, into NanoKernel repository
     *
     * Return value:
     *
     * if <cdev> is zero the first instance of a device of class
     * <dclass> is returned. Otherwise <cdev> must be an
     * address returned by a previous call to dev_lookup_xxx().
     * The next device of class <dclass>, starting from <cdev>
     * is returned in that case.
     *
     * 0 is returned, if no device of the required class is found.
     */
    static NkPhAddr
nk_dev_lookup_by_class (NkDevClass cid, NkPhAddr cdev)
{
    cdev = (cdev ? VDEV(cdev)->next : NKCTX()->dev_info);
    while (cdev && (VDEV(cdev)->class_id != cid)) {
	cdev = VDEV(cdev)->next;
    }
    return cdev;
}

    /*
     * Lookup first device of given type, into NanoKernel repository.
     *
     * Return value:
     *
     * if <cdev> is zero the first instance of a device of type
     * <did> is returned. Otherwise <cdev> must be an
     * address returned by a previous call to dev_lookup_xxx().
     * The next device of type <did>, starting from <cdev>
     * is returned in that case.
     *
     * NULL is returned, if no device of the required type is found.
     */
    static NkPhAddr
nk_dev_lookup_by_type (NkDevId did, NkPhAddr cdev)
{
    cdev = (cdev ? VDEV(cdev)->next : NKCTX()->dev_info);
    while (cdev && (VDEV(cdev)->dev_id != did)) {
	cdev = VDEV(cdev)->next;
    }
    return cdev;
}

    /*
     * Lookup first virtual communication link of given a class/name
     * into NanoKernel repository.
     *
     * Return value:
     *
     * if <plnk> is zero the first instance of a virtual link with
     * a required <name> is returned. Otherwise <plnk> must be an
     * address returned by a previous call to nk_vlink_lookup().
     * The next virtual link with required <name>, starting from <plnk>
     * is returned in that case.
     *
     * NULL is returned, if no virtual link with required <name> is found.
     */
    static NkPhAddr
nk_vlink_lookup (const char* name, NkPhAddr plnk)
{
    NkDevDesc*  vdev;
    NkPhAddr    pdev;
    NkDevVlink* vlnk;

    if (plnk) {
	vdev = VDEV(plnk - sizeof(NkDevDesc));

	if (vdev->dev_header != plnk) {
	    DKI_PANIC(("virtual link list is corrupted\n"));
	}

	pdev = vdev->next;
    } else {
	pdev = NKCTX()->dev_info;
    }

    while (pdev) {
	vdev = VDEV(pdev);

	if (vdev->dev_id == NK_DEV_ID_VLINK) {
	    plnk = pdev + sizeof(NkDevDesc);

	    if (vdev->dev_header != plnk) {
		DKI_PANIC(("virtual link list is corrupted\n"));
	    }

	    vlnk = VLNK(plnk);

	    if (strcmp(vlnk->name, name) == 0) {
		return plnk;
	    }
	}

	pdev = vdev->next;
    }

    return 0;
}

#ifdef CONFIG_NKERNEL_PRIMARY
    /*
     * Allocates a contiguous memory block from NanoKernel repository
     */
    static NkPhAddr
nk_p_dev_alloc (NkPhSize size)		// primary kernel
{
    return NKCTX()->dev_alloc(NKCTX(), size);
}
#endif

    static NkPhAddr
nk_s_dev_alloc (NkPhSize size)		// secondary kernel
{
    NkPhAddr      pdev;
    nku32_f       cpsr;

    cpsr = _intr_mask();
    pdev =  NKCTX()->dev_alloc(NKCTX(), size);
    _intr_unmask(cpsr);

    if (pdev != 0) {
	memset((void*)PTOV(pdev), 0, size);
    }

    return pdev;
}

    /*
     * Add a new device to NanoKernel repository.
     * <dev> is a physical address previously returned by
     * dev_alloc(). It must points to a completed NkDevDesc structure.
     */
    static void
nk_dev_add (NkPhAddr dev)
{
    NkDevDesc*    head;
    NkDevDesc*    vdev;
    NkPhAddr      pdev;
    unsigned long flags;

    pdev = NKCTX()->dev_info;

    if (!pdev) {
	DKI_PANIC(("device list is empty!\n"));
    }

    VDEV(dev)->next = 0;

    head = VDEV(pdev);

    __NK_HARD_LOCK_IRQ_SAVE(&(head->dev_lock), flags);

    do {
	vdev = VDEV(pdev);
        pdev = vdev->next;
    } while (pdev);

    vdev->next = dev;

    __NK_HARD_UNLOCK_IRQ_RESTORE(&(head->dev_lock), flags);

    nk_dev_notification();
}

    /*
     * Allocates required number of contiguous unused cross-interrupts.
     * Returns first interrupt in allocated range.
     */
    static NkXIrq
_nk_pxirq_alloc (NkOsId osid, int nb)
{
    NkXIrq   xirq;
    NkOsCtx* osctx = NKCTX()->osctx_get(osid);

    xirq = osctx->xirq_free;
    if ((xirq + nb) >= NK_XIRQ_LIMIT) {
	xirq = 0;
    } else {
        osctx->xirq_free += nb;
    }

    return xirq;
}

    /*
     * Allocate a resource associated with vlink
     */
    static void
_nk_resource_alloc(NkDevVlink* vlink, NkResource* resrc)
{
    NkResource* o_resrc;
    NkResource* n_resrc;
    nku32_f     lock;
    NkPhAddr    paddr;
    nku32_f     cpsr;

	/*
	 * scan resource list to see if resource is
	 * already allocated or not.
	 */
    o_resrc = (NkResource*)&vlink->resrc;
    for (;;) {
	    /*
	     * read resource list lock
	     */
	lock  = vlink->lock;

	    /*
	     * scan resource list
	     */
	while (o_resrc->next != 0) {
	    o_resrc = (NkResource*)PTOV(o_resrc->next);
		/*
		 * check if resource is already allocated
		 */
	    if ((o_resrc->type == resrc->type) &&
	        (o_resrc->id   == resrc->id)) {
		    /*
		     * If sanity check pass return
		     * previously allocated resource
		     */
		switch (resrc->type) {
		case NK_RESOURCE_PDEV:
		    if (resrc->r.pdev.size != o_resrc->r.pdev.size) {
			DKI_ERROR(("nk_pdev_alloc: wrong size 0x%x != 0x%x\n",
				    resrc->r.pdev.size, o_resrc->r.pdev.size));
			resrc->r.pdev.addr = 0;
			break;
		    }
		    resrc->r.pdev.addr =  o_resrc->r.pdev.addr;
		    break;
		case NK_RESOURCE_PMEM:
		    if (resrc->r.pmem.size != o_resrc->r.pmem.size) {
			DKI_ERROR(("nk_pmem_alloc: wrong size 0x%x != 0x%x\n",
				    resrc->r.pmem.size, o_resrc->r.pmem.size));
			resrc->r.pmem.addr = 0;
			break;
		    }
		    resrc->r.pmem.addr =  o_resrc->r.pmem.addr;
		    break;
		case NK_RESOURCE_PXIRQ:
		    if (resrc->r.pxirq.osid != o_resrc->r.pxirq.osid) {
			DKI_ERROR(("nk_pxirq_alloc: wrong OS id %d != %d\n",
				    resrc->r.pxirq.osid,
				  o_resrc->r.pxirq.osid));
			resrc->r.pxirq.base = 0;
			break;
		    }
		    if (resrc->r.pxirq.numb != o_resrc->r.pxirq.numb) {
			DKI_ERROR(("nk_pxirq_alloc: wrong xirq nb %d != %d\n",
				    resrc->r.pxirq.numb,
				  o_resrc->r.pxirq.numb));
			resrc->r.pxirq.base = 0;
			break;
		    }
		    resrc->r.pxirq.base =  o_resrc->r.pxirq.base;
		    break;
		}
		return;
	    }
	}
	    /*
	     * No previously allocated resource found
	     * Get lock and check if resource list is changed during
	     * scan or not.
	     */
	cpsr = _intr_mask();
	if (lock == vlink->lock) {
	    break;
	}
	_intr_unmask(cpsr);
    }
	/*
	 * We have list lock here.
	 * Try to allocated new resource descriptor.
	 */
    paddr = NKCTX()->dev_alloc(NKCTX(), sizeof(NkResource));
    if (paddr == 0) {
	_intr_unmask(cpsr);
	return;
    }
    n_resrc = (NkResource*)PTOV(paddr);

	/*
	 * Try to allocate resource itself
	 */
    switch (resrc->type) {
    case NK_RESOURCE_PDEV:
	resrc->r.pdev.addr = NKCTX()->dev_alloc(NKCTX(), resrc->r.pdev.size);
	if (resrc->r.pdev.addr == 0) {
	    _intr_unmask(cpsr);
	    return;
	}
	break;
    case NK_RESOURCE_PMEM:
	resrc->r.pmem.addr = NKCTX()->pmem_alloc(NKCTX(), resrc->r.pmem.size);
	if (resrc->r.pmem.addr == 0) {
	    _intr_unmask(cpsr);
	    return;
	}
	break;
    case NK_RESOURCE_PXIRQ:
	resrc->r.pxirq.base = _nk_pxirq_alloc(resrc->r.pxirq.osid,
					      resrc->r.pxirq.numb);
	if (resrc->r.pxirq.base == 0) {
	    _intr_unmask(cpsr);
	    return;
	}
	break;
    }
	/*
	 * Fill resource descriptor, add it
	 * to resource list, and bump list lock up.
	 */
    memcpy(n_resrc, resrc, sizeof(NkResource));

    n_resrc->next = 0;
    o_resrc->next = VTOP(n_resrc);
    vlink->lock++;

	/*
	 * Release lock and return
	 */
    _intr_unmask(cpsr);
    return;
}

    /*
     * Allocate <size> bytes of contiguous memory from the persistent
     * device repository.
     *
     * The allocated memory block is labeled using <link, id>.
     * It is guaranteed that for a unique label, a unique memory block
     * is allocated. Thus different calls with the same label always
     * return the same result.
     *
     * Return the physical base address of the allocated memory block,
     * or 0 on failure.
     */
    static NkPhAddr
nk_pdev_alloc (NkPhAddr link, NkResourceId id, NkPhSize size)
{
    NkResource  resrc;
    NkDevVlink* vlink;

    NKDDI_ZREG_CCHECK(link);
    vlink = (NkDevVlink*)PTOV(link);

    resrc.type        = NK_RESOURCE_PDEV;
    resrc.id          = id;
    resrc.r.pdev.size = size;
    resrc.r.pdev.addr = 0;

    _nk_resource_alloc(vlink, &resrc);

    return resrc.r.pdev.addr;
}

    /*
     * Allocate <size> bytes (rounded up to the nearest multiple of
     * page size) of contiguous memory from the persistent communication
     * memory pool.
     *
     * The allocated memory block is labeled using <link, id>.
     * It is guaranteed that for a unique label, a unique memory block
     * is allocated. Thus different calls with the same label always
     * return the same result.
     *
     * Return the physical base address of the allocated memory block
     * (aligned to a page boundary), or 0 on failure.
     */
    static NkPhAddr
nk_pmem_alloc (NkPhAddr link, NkResourceId id, NkPhSize size)
{
    NkResource  resrc;
    NkDevVlink* vlink;

    NKDDI_ZREG_CCHECK(link);
    vlink = (NkDevVlink*)PTOV(link);

    resrc.type        = NK_RESOURCE_PMEM;
    resrc.id          = id;
    resrc.r.pmem.size = size;
    resrc.r.pmem.addr = 0;

    _nk_resource_alloc(vlink, &resrc);

    return resrc.r.pmem.addr;
}

    /*
     * Allocate <numb> contiguous persistent cross-interrupts.
     *
     * The allocated xirqs range is labeled using <link, id>.
     * It is guaranteed that for a unique label, a unique xirqs range
     * is allocated. Thus different calls with the same label always
     * return the same result.
     *
     * Return the number of the first allocated xirq or 0 if not enough
     * xirq are available.
     */
    static NkXIrq
nk_pxirq_alloc (NkPhAddr link, NkResourceId id, NkOsId osid, int numb)
{
    NkResource  resrc;
    NkDevVlink* vlink;

    NKDDI_ZREG_CCHECK(link);
    vlink = (NkDevVlink*)PTOV(link);

    resrc.type         = NK_RESOURCE_PXIRQ;
    resrc.id           = id;
    resrc.r.pxirq.osid = osid;
    resrc.r.pxirq.numb = numb;
    resrc.r.pxirq.base = 0;

    _nk_resource_alloc(vlink, &resrc);

    return resrc.r.pxirq.base;
}

    /*
     * Copy <size> bytes from the memory of the current guest at virtual
     * address <src> into the memory of another guest which resides at
     * physical address <dst>.
     *
     * Return the number of successfully copied bytes.
     */
    static NkPhSize
nk_mem_copy_to (NkPhAddr dst, void* src, NkPhSize size)
{
    memcpy(PTOV(dst), src, size);
    return size;
}

    /*
     * Copies <size> bytes from the memory of another guest which resides
     * at physical address <src> into the memory of the current guest at
     * virtual address <dst>.
     *
     * Return the number of successfully copied bytes.
     */
    static NkPhSize
nk_mem_copy_from (void* dst, NkPhAddr src, NkPhSize size)
{
#ifdef CONFIG_NKERNEL_DDI_APICHECK
    NKDDI_ZREG_CCHECK(dst);
#endif
    memcpy(dst, PTOV(src), size);
    return size;
}

    /*
     * Get self OS ID
     */
    static NkOsId
nk_id_get (void)
{
    return NKCTX()->id;
}

    /*
     * Get last OS ID (i.e current highest OS ID value)
     */
    static NkOsId
nk_last_id_get (void)
{
    return NKCTX()->lastid;
}

    /*
     * Get a mask of started operating system's ID
     * -> bit <n> is '1' if system with NkOsId <n> is running.
     * -> bit <n> is '0' if system with NkOsId <n> is stopped.
     */
    static NkOsMask
nk_running_ids_get (void)
{
    return *(nkdev.running);
}

    /*
     * Convert a physical address to a virtual one.
     */
    static void*
nk_ptov (NkPhAddr addr)
{
    return PTOV(addr);
}

    /*
     * Convert a virtual address to a physical one.
     */
    static NkPhAddr
nk_vtop (void* addr)
{
    return VTOP(addr);
}

    /*
     * Find first bit set
     */
    static inline nku32_f
_ffs(nku32_f mask)
{
    int n = 31;

    if (mask & 0x0000ffff) { n -= 16;  mask <<= 16; }
    if (mask & 0x00ff0000) { n -=  8;  mask <<=  8; }
    if (mask & 0x0f000000) { n -=  4;  mask <<=  4; }
    if (mask & 0x30000000) { n -=  2;  mask <<=  2; }
    if (mask & 0x40000000) { n -=  1;		    }

    return n;
}

    /*
     * Get next bit of highest priority set in bit mask.
     */
    static nku32_f
nk_bit_get_next (nku32_f mask)
{
    if (mask == 0) {
	DKI_PANIC(("nk_bit_get_next called with mask equal to zero\n"));
    }

    return __ffs(mask);
}

    /*
     * Insert a bit of given priority into a mask.
     */
    static inline nku32_f
nk_bit_mask (nku32_f bit)
{
    return (1 << bit);
}

    static NkPhAddr
nk_smp_dev_alloc (NkPhSize size)
{
    return NKCTX()->dev_alloc(NKCTX(), size);
}

    /*
     * Add a new device to NanoKernel repository.
     * <dev> is a physical address previously returned by
     * dev_alloc(). It must points to a completed NkDevDesc structure.
     */
    static void
nk_smp_dev_add (NkPhAddr dev)
{
    NKCTX()->smp_dev_add(dev);
    nk_dev_notification();
}

    /*
     * Allocates required number of contiguous unused cross-interrupts.
     * Returns first interrupt in allocated range.
     */
    static NkXIrq
nk_smp_xirq_alloc (int nb)
{
    return NKCTX()->smp_xirq_alloc(nb);
}

    /*
     * Allocate <size> bytes of contiguous memory from the persistent
     * device repository.
     *
     * The allocated memory block is labeled using <link, id>.
     * It is guaranteed that for a unique label, a unique memory block
     * is allocated. Thus different calls with the same label always
     * return the same result.
     *
     * Return the physical base address of the allocated memory block,
     * or 0 on failure.
     */
    static NkPhAddr
nk_smp_pdev_alloc (NkPhAddr link, NkResourceId id, NkPhSize size)
{
    return NKCTX()->smp_pdev_alloc(link, id, size);
}

    /*
     * Allocate <size> bytes (rounded up to the nearest multiple of
     * page size) of contiguous memory from the persistent communication
     * memory pool.
     *
     * The allocated memory block is labeled using <link, id>.
     * It is guaranteed that for a unique label, a unique memory block
     * is allocated. Thus different calls with the same label always
     * return the same result.
     *
     * Return the physical base address of the allocated memory block
     * (aligned to a page boundary), or 0 on failure.
     */
    static NkPhAddr
nk_smp_pmem_alloc (NkPhAddr link, NkResourceId id, NkPhSize size)
{
    return NKCTX()->smp_pmem_alloc(link, id, size);
}

    /*
     * Allocate <numb> contiguous persistent cross-interrupts.
     *
     * The allocated xirqs range is labeled using <link, id>.
     * It is guaranteed that for a unique label, a unique xirqs range
     * is allocated. Thus different calls with the same label always
     * return the same result.
     *
     * Return the number of the first allocated xirq or 0 if not enough
     * xirq are available.
     */
    static NkXIrq
nk_smp_pxirq_alloc (NkPhAddr link, NkResourceId id, NkOsId osid, int numb)
{
    return NKCTX()->smp_pxirq_alloc(link, id, osid, numb);
}

#if defined(CONFIG_SMP) && (__LINUX_ARM_ARCH__ >= 6)

    static inline nku32_f
__nk_smp_swap (volatile nku32_f* addr, nku32_f data)
{
    nku32_f res, tmp;
    __asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	strex	%1, %3, [%2]\n"
"	teq	%1, #0\n"
"	bne	1b"
    : "=&r" (res), "=&r" (tmp)
    : "r" (addr), "r" (data)
    : "cc");
    return res;
}

    static void
__nk_smp_set (volatile nku32_f* addr, nku32_f data)
{
    nku32_f tmp, tmp2;
    __asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	orr	%0, %0, %3\n"
"	strex	%1, %0, [%2]\n"
"	teq	%1, #0\n"
"	bne	1b"
    : "=&r" (tmp), "=&r" (tmp2)
    : "r" (addr), "Ir" (data)
    : "cc");
}

    /*
     * Atomic operation to clear bits within a bit field.
     *
     * The following logical operation: *addr &= ~data
     * is performed atomically.
     */
    static void
nk_atomic_clear (volatile nku32_f* addr, nku32_f data)
{
    nku32_f tmp, tmp2;
    NKDDI_VOID_ACHECK(addr);
    __asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	bic	%0, %0, %3\n"
"	strex	%1, %0, [%2]\n"
"	teq	%1, #0\n"
"	bne	1b"
    : "=&r" (tmp), "=&r" (tmp2)
    : "r" (addr), "Ir" (data)
    : "cc");
}

    /*
     * Atomic operation to clear bits within a bit field.
     *
     * The following logical operation: *addr &= ~data
     * is performed atomically.
     *
     * Returns 0 if and only if the result is 0.
     */
    static nku32_f
nk_clear_and_test (volatile nku32_f* addr, nku32_f data)
{
    nku32_f res, tmp;
	NKDDI_OREG_ACHECK(addr);
    __asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	bic	%0, %0, %3\n"
"	strex	%1, %0, [%2]\n"
"	teq	%1, #0\n"
"	bne	1b"
    : "=&r" (res), "=&r" (tmp)
    : "r" (addr), "Ir" (data)
    : "cc");
    return res;
}

    /*
     * Atomic operation to set bits within a bit field.
     * The following logical operation: *addr |= data
     * is performed atomically
     */
    static void
nk_atomic_set (volatile nku32_f* addr, nku32_f data)
{
	NKDDI_VOID_ACHECK(addr);
    __nk_smp_set(addr, data);
}

    /*
     * Atomic operation to subtract value to a given memory location.
     * The following logical operation: *addr -= data
     * is performed atomically.
     */
    static void
nk_atomic_sub (volatile nku32_f* addr, nku32_f data)
{
    nku32_f tmp, tmp2;
	NKDDI_VOID_ACHECK(addr);
    __asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	sub	%0, %0, %3\n"
"	strex	%1, %0, [%2]\n"
"	teq	%1, #0\n"
"	bne	1b"
    : "=&r" (tmp), "=&r" (tmp2)
    : "r" (addr), "Ir" (data)
    : "cc");
}

    /*
     * Atomic operation to subtract value to a given memory location.
     * The following logical operation: *addr -= data
     * is performed atomically.
     *
     * Returns 0 if and only if the result is 0.
     */
    static nku32_f
nk_sub_and_test (volatile nku32_f* addr, nku32_f data)
{
    nku32_f res, tmp;
	NKDDI_OREG_ACHECK(addr);
    __asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	sub	%0, %0, %3\n"
"	strex	%1, %0, [%2]\n"
"	teq	%1, #0\n"
"	bne	1b"
    : "=&r" (res), "=&r" (tmp)
    : "r" (addr), "Ir" (data)
    : "cc");
    return res;
}

    /*
     * Atomic operation to add value to a given memory location.
     * The following logical operation: *addr += data
     * is performed atomically
     */
    static void
nk_atomic_add (volatile nku32_f* addr, nku32_f data)
{
    nku32_f tmp, tmp2;
    NKDDI_VOID_ACHECK(addr);
    __asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	add	%0, %0, %3\n"
"	strex	%1, %0, [%2]\n"
"	teq	%1, #0\n"
"	bne	1b"
    : "=&r" (tmp), "=&r" (tmp2)
    : "r" (addr), "Ir" (data)
    : "cc");
}

#else

    static inline nku32_f
__nk_smp_swap (volatile nku32_f* addr, nku32_f data)
{
    nku32_f res;
    res   = *addr;
    *addr = data;
    return res;
}

    static inline void
__nk_smp_set (volatile nku32_f* addr, nku32_f data)
{
    *addr |= data;
}

    /*
     * Atomic operation to clear bits within a bit field.
     *
     * The following logical operation: *addr &= ~data
     * is performed atomically.
     */
    static void
nk_atomic_clear (volatile nku32_f* addr, nku32_f data)
{
    nku32_f flags;
    NKDDI_VOID_CCHECK(addr);
    flags = _intr_mask();
    *addr &= ~data;
    _intr_unmask(flags);
}

    /*
     * Atomic operation to clear bits within a bit field.
     *
     * The following logical operation: *addr &= ~data
     * is performed atomically.
     *
     * Returns 0 if and only if the result is 0.
     */
    static nku32_f
nk_clear_and_test (volatile nku32_f* addr, nku32_f data)
{
    nku32_f flags;
    nku32_f res;
	NKDDI_OREG_CCHECK(addr);
    flags = _intr_mask();
    res = *addr & ~data;
    *addr = res;
    _intr_unmask(flags);
    return res;
}

    /*
     * Atomic operation to set bits within a bit field.
     * The following logical operation: *addr |= data
     * is performed atomically
     */
    static void
nk_atomic_set (volatile nku32_f* addr, nku32_f data)
{
    nku32_f flags;
	NKDDI_VOID_CCHECK(addr);
    flags = _intr_mask();
    *addr |= data;
    _intr_unmask(flags);
}

    /*
     * Atomic operation to subtract value to a given memory location.
     * The following logical operation: *addr -= data
     * is performed atomically.
     */
    static void
nk_atomic_sub (volatile nku32_f* addr, nku32_f data)
{
    nku32_f flags;
	NKDDI_VOID_CCHECK(addr);
    flags = _intr_mask();
    *addr -= data;
    _intr_unmask(flags);
}

    /*
     * Atomic operation to subtract value to a given memory location.
     * The following logical operation: *addr -= data
     * is performed atomically.
     *
     * Returns 0 if and only if the result is 0.
     */
    static nku32_f
nk_sub_and_test (volatile nku32_f* addr, nku32_f data)
{
    nku32_f flags;
    nku32_f res;
	NKDDI_OREG_CCHECK(addr);
    flags = _intr_mask();
    res   = *addr - data;
    *addr = res;
    _intr_unmask(flags);
    return res;
}

    /*
     * Atomic operation to add value to a given memory location.
     * The following logical operation: *addr += data
     * is performed atomically
     */
    static void
nk_atomic_add (volatile nku32_f* addr, nku32_f data)
{
    nku32_f flags;
    NKDDI_VOID_CCHECK(addr);
    flags = _intr_mask();
    *addr += data;
    _intr_unmask(flags);
}

#endif

    /*
     * Physical (shared) memory mapping/unmapping
     *
     * Use one-to-one mapping for shared memory
     *
     */
    static void*
nk_mem_map (NkPhAddr paddr, NkPhSize size)
{
    return PTOV(paddr);
}

    static void
nk_mem_unmap (void* vaddr, NkPhAddr paddr, NkPhSize size)
{
}

/*
 * Cross IRQs
 */

    /*
     * Allocates required number of contiguous unused cross-interrupts.
     * Returns first interrupt in allocated range.
     */
    static NkXIrq
nk_xirq_alloc (int nb)
{
    NkXIrq free;

    spin_lock(&(nkdev.xirq_b_lock));

    free = NKCTX()->xirq_free;
    if ((free + nb) >= NK_XIRQ_LIMIT) {
	free = 0;
    } else {
        NKCTX()->xirq_free += nb;
    }

    spin_unlock(&(nkdev.xirq_b_lock));

    return free;
}

    /*
     * Wrapper for XIRQs (above NB_IRQS).
     * Wrap NK XIRQ handlers to Linux IRQ handlers.
     */
typedef struct XIrqWrapper {
    NkXIrq        xirq;
    NkXIrqHandler handler;
    void*         cookie;
#ifdef CONFIG_KALLSYMS
    char	  caller [KSYM_SYMBOL_LEN];
#endif
} XIrqWrapper;

    static irqreturn_t
xirq_handler (int irq, void* cookie)
{
    XIrqWrapper* wrapper = (XIrqWrapper*)cookie;

    wrapper->handler(wrapper->cookie, irq);

    return IRQ_HANDLED;
}

    static NkXIrqId
_nk_xirq_attach (NkXIrq        xirq,
		 NkXIrqHandler hdl,
		 void*         cookie,
		 int           masked
#ifdef CONFIG_KALLSYMS
		 , void*	       caller
#endif
		 )
{
    XIrqWrapper* wrapper;
    int          res;
#ifdef CONFIG_KALLSYMS
    char*        wrapper_ptr;
#endif

        /*
	 * Check for a valid xirq number
	 * (on SMP, the XIRQ allocation done by NK)
	 */
    if (!_is_nk_smp() && (xirq >= NKCTX()->xirq_free)) {
	return 0;
    }

        /*
	 * Allocate and fill in a new descriptor for event handlers
	 */
    wrapper = (XIrqWrapper*)kmalloc(sizeof(XIrqWrapper), GFP_KERNEL);
    if (!wrapper) {
        return 0;
    }
    wrapper->xirq    = xirq;
    wrapper->handler = hdl;
    wrapper->cookie  = cookie;
#ifdef CONFIG_KALLSYMS
	/*
	 * The initialisation of wrapper->caller is theoretically
	 * useless before calling sprint_symbol(), but failing to do
	 * something useless like that before or after the call to
	 * sprint_symbol() in kernel 2.6.32 running in VM3 leads to
	 * a system-wide deadlock. Maybe the symbol search overflows
	 * the stack? The first time we are called is from kernel_init().
	 */
    *wrapper->caller = '\0';
    sprint_symbol (wrapper->caller, (unsigned long) caller);
    wrapper_ptr = strchr(wrapper->caller, '/');
    if (wrapper_ptr != NULL)
        *wrapper_ptr = '\\';
#endif

        /*
	 * Wrap to Linux IRQ handling ...
	 */
    res = request_irq(xirq, xirq_handler,
		      (masked ? (IRQF_DISABLED | IRQF_SHARED) : IRQF_SHARED),
#ifdef CONFIG_KALLSYMS
		      wrapper->caller,
#else
		      "NK xirq",
#endif
		      wrapper);
    if (res) {
        printnk("xirq_attach: request_irq(%d) failed (%d)\n", xirq, res);
	kfree(wrapper);
	return 0;
    }

    return (NkXIrqId)wrapper;
}


    /*
     * Attach a handler to a given NanoKernel cross-interrupt.
     * 0 is returned on failure.
     */
    static NkXIrqId
nk_xirq_attach (NkXIrq        xirq,
		NkXIrqHandler hdl,
		void*         cookie)
{
    return _nk_xirq_attach(xirq, hdl, cookie, 0
#ifdef CONFIG_KALLSYMS
			   , __builtin_return_address(0)
#endif
			   );
}

    /*
     * Attach a handler to a given NanoKernel cross-interrupt.
     * 0 is returned on failure.
     */
    static NkXIrqId
nk_xirq_attach_masked (NkXIrq        xirq,
		       NkXIrqHandler hdl,
		       void*         cookie)
{
    return _nk_xirq_attach(xirq, hdl, cookie, 1
#ifdef CONFIG_KALLSYMS
			   , __builtin_return_address(0)
#endif
			   );
}


    /*
     * Detach a handler (previously connected with irq_attach())
     */
    static void
nk_xirq_detach (NkXIrqId id)
{
    XIrqWrapper*  wrapper;

    NKDDI_VOID_CCHECK(id);
    wrapper = (XIrqWrapper*)id;

    free_irq(wrapper->xirq, wrapper);
    kfree(wrapper);
}

    /*
     * Atomic operations on the NkXIrqMask objects.
     */

#define	L1_BIT(x)	((x) >> 5)
#define	L2_BIT(x)	((x) & 0x1f)
#define	XIRQ(l1,l2)	(((l1) << 5) + (l2))

    static inline void
_xirq_emask_set (NkXIrqMask* enabled, NkXIrq xirq)
{
    nku32_f l1bit = L1_BIT(xirq);
    nku32_f l2bit = L2_BIT(xirq);
    nk_atomic_set(&enabled->lvl2[l1bit], nk_bit_mask(l2bit));
}

    static inline void
_xirq_emask_clear (NkXIrqMask* enabled, NkXIrq xirq)
{
    nku32_f l1bit = L1_BIT(xirq);
    nku32_f l2bit = L2_BIT(xirq);
    nk_atomic_clear(&enabled->lvl2[l1bit], nk_bit_mask(l2bit));
}

    static inline void
_xirq_pmask_set (NkXIrqMask* pending, NkXIrq xirq)
{
    nku32_f l1bit = L1_BIT(xirq);
    nku32_f l2bit = L2_BIT(xirq);
    nku32_f cpsr;
    cpsr = _intr_mask();
    __nk_smp_set(&pending->lvl2[l1bit], nk_bit_mask(l2bit));
    __nk_smp_set(&pending->lvl1,        nk_bit_mask(l1bit));
    _intr_unmask(cpsr);
}

    static nku32_f
_xirq_pmask_get_selector (NkXIrqMask* pending)
{
    nku32_f level1;
    nku32_f cpsr;
    cpsr = _intr_mask();
    level1 = __nk_smp_swap(&pending->lvl1, 0);
    _intr_unmask(cpsr);
    return level1;
}

    static NkXIrq
_xirq_pmask_get_next (nku32_f*    selector,
		      NkXIrqMask* pending,
		      NkXIrqMask* enabled)
{
    nku32_f l1 = *selector;
    while (l1) {
	nku32_f l1bit = nk_bit_get_next(l1);
	nku32_f l2    = pending->lvl2[l1bit];
	while (l2) {
	    nku32_f l2bit  = nk_bit_get_next(l2);
	    nku32_f l2mask = nk_bit_mask(l2bit);
	    nk_atomic_clear(&pending->lvl2[l1bit], l2mask);
	    if (enabled->lvl2[l1bit] & l2mask) {
		*selector = l1;
		return XIRQ(l1bit, l2bit);
	    }
	    l2 &= ~l2mask;
	}
	l1 &= ~nk_bit_mask(l1bit);
    }
    *selector = 0;
    return (NkXIrq)-1;
}

    /*
     * Mask / Unmask a given cross-interrupt
     */
    static void
nk_xirq_mask (NkXIrq xirq)
{
    disable_irq(xirq);
}

    static void
nk_xirq_unmask (NkXIrq xirq)
{
    enable_irq(xirq);
}

    /*
     * IRQ chip operations
     */

/* add extern to fix link failures with  gcc-5.x+ -psych.half */
    extern inline void
__nk_xirq_mask (struct irq_data *id)
{
    _xirq_emask_clear(&nkdev.xenabled, id->irq);
}

    extern inline void
__nk_xirq_unmask (struct irq_data *id)
{
    _xirq_emask_set(&nkdev.xenabled, id->irq);
}

    extern inline void
__nk_xirq_ack (struct irq_data *id)
{
}

    extern inline unsigned int
__nk_xirq_startup(struct irq_data *id)
{
    _xirq_emask_set(&nkdev.xenabled, id->irq);
    return 0;
}

    extern inline void
__nk_xirq_shutdown(struct irq_data *id)
{
    _xirq_emask_clear(&nkdev.xenabled, id->irq);
}

    static int
__nk_xirq_set_wake(struct irq_data *id, unsigned int on)
{
    (void) id;
    (void) on;
	/* Any xirq wakes us up */
    return 0;
}

    /*
     * Trigger a cross-interrupt to a given operating system.
     */
    static void
nk_xirq_trigger (NkXIrq xirq, NkOsId osid)
{
    _xirq_pmask_set(&nkdev.xirqs[osid], xirq);
    NKCTX()->xpost(NKCTX(), osid);
}

    static void
nk_xirq_affinity (NkXIrq xirq, NkCpuMask cpus)
{
    if (nkdev.affinity) {
	nkdev.affinity[xirq] = cpus;
	if (xirq < NK_HW_XIRQ_LIMIT) {
	    NKCTX()->smp_irq_affinity(xirq);
	}
    }
}

    static nku32_f
nk_balloon_ctrl (int op, nku32_f* pfns, nku32_f count)
{
    if (!os_ctx->balloon_ctrl) {
	return 0;
    }
    return os_ctx->balloon_ctrl(op, pfns, count);
}

    static int
nk_prop_set (char* name, void* value, unsigned int size)
{
    if (!os_ctx->prop_set) {
	return NK_PROP_UNKNOWN;
    }
    return os_ctx->prop_set(name, value, size);
}

    static int
nk_prop_get (char* name, void* value, unsigned int maxsize)
{
    if (!os_ctx->prop_get) {
	return NK_PROP_UNKNOWN;
    }
    return os_ctx->prop_get(name, value, maxsize);
}

    static int
nk_prop_enum (unsigned int pid, char* name, unsigned int nlen_max,
              NkPropAttr* attr)
{
    if (!os_ctx->prop_enum) {
	return NK_PROP_UNKNOWN;
    }
    return os_ctx->prop_enum(pid, name, nlen_max, attr);
}

#ifdef CONFIG_SMP
    static int
__nk_set_cpu (struct irq_data *id, const struct cpumask* mask,
	      bool force)
{
    nk_xirq_affinity(id->irq, cpumask_bits(mask)[0]);
    return 0;
}
#endif

struct irq_chip nk_xirq_chip = {
    .name     = "XIRQ",
    .irq_ack      = __nk_xirq_ack,
    .irq_mask     = __nk_xirq_mask,
    .irq_unmask   = __nk_xirq_unmask,
    .irq_startup  = __nk_xirq_startup,
    .irq_shutdown = __nk_xirq_shutdown,
#ifdef CONFIG_SMP
    .irq_set_affinity = __nk_set_cpu,
#endif
    .irq_set_wake = __nk_xirq_set_wake
};

extern void nk_do_IRQ (unsigned int irq, struct pt_regs *regs);

    unsigned int
nk_do_xirq (struct pt_regs* regs)
{
    NkXIrqMask*     xpending;
    nku32_f         sel;
    struct pt_regs* old_regs = set_irq_regs(regs);

    irq_enter();

#ifdef CONFIG_SMP
    xpending = nkdev.xpending[NKCTX()->vcpuid];
#else
    xpending = nkdev.xpending[0];
#endif

    sel = _xirq_pmask_get_selector(xpending);

    while (sel) {
	NkXIrq xirq;
        xirq = _xirq_pmask_get_next(&sel, xpending, &nkdev.xenabled);
	if (xirq == (NkXIrq)-1) {
	    break;
	}
	inc_irq(xirq);
	/* if (get_sys_cnt() > (120000)) */{
		/*
		if (xirq > 31)  {
			printk("##: xirq = %d\n", xirq);
		}
		*/
		
		//if (xirq != 6)	printk("##: xirq = %d.\n", xirq);
		/*
		add_pm_message(get_sys_cnt(), "xirq = ", xirq, 0, 0);
		*/		
	}


#ifdef FIXME_GILLES
	interrupt_counter++;
	inc_sprd_irq(xirq);
#endif
	
	nk_do_IRQ(xirq, regs);
    }

    irq_exit();

    set_irq_regs(old_regs);

    return 1;
}

/*
 * Exported global variables
 */

NkDevOps nkops = {	/* NKDDI operations */
    NK_VERSION_8,
    nk_dev_lookup_by_class,
    nk_dev_lookup_by_type,
#ifdef CONFIG_NKERNEL_PRIMARY
    nk_p_dev_alloc,
#else
    nk_s_dev_alloc,
#endif
    nk_dev_add,
    nk_id_get,
    nk_last_id_get,
    nk_running_ids_get,
    nk_ptov,
    nk_vtop,
    nk_vex_mask,
    nk_vex_addr,
    nk_xirq_alloc,
    nk_xirq_attach,
    nk_xirq_mask,
    nk_xirq_unmask,
    nk_xirq_detach,
    nk_xirq_trigger,
    nk_bit_get_next,
    nk_bit_mask,
    nk_atomic_clear,
    nk_clear_and_test,
    nk_atomic_set,
    nk_atomic_sub,
    nk_sub_and_test,
    nk_atomic_add,
    nk_mem_map,
    nk_mem_unmap,
    NULL,	/* nk_cpu_id_get */
    NULL,	/* nk_xvex_post */
    NULL,	/* nk_local_xirq_post */
    nk_xirq_attach_masked,
    NULL,	/* nk_scheduler */
    NULL,	/* nk_stop */
    NULL,	/* nk_start */
    NULL,	/* nk_suspend */
    NULL,	/* nk_resume */
    NULL,	/* nk_machine_addr */
    NULL,	/* nk_log_buffer */
    NULL,	/* nk_log_guests */
    NULL,	/* nk_log_events */
    NULL,	/* nk_cmdline_get */
    nk_vlink_lookup,
    nk_pdev_alloc,
    nk_pmem_alloc,
    nk_pxirq_alloc,
    nk_mem_copy_to,
    nk_mem_copy_from,
    NULL,	/* nk_mem_map_sync */
    NULL,	/* nk_mem_unmap_sync */
    NULL,	/* nk_cache_sync */
    nk_info,
    nk_xirq_affinity,
    nk_balloon_ctrl,
    nk_prop_set,
    nk_prop_get,
    nk_prop_enum,
};

NkDevXPic*   nkxpic;	   /* virtual XPIC device */
NkOsId       nkxpic_owner; /* owner of the virtual XPIC device */
NkOsMask     nkosmask;	   /* my OS mask (cached) */

    void __init
nk_ddi_init (void)
{
    NkPhAddr   pdev;
    NkDevDesc* vdev = 0;
    NkOsId     myid = nkops.nk_id_get();
    NkXIrq     xirq;
    int        smp = _is_nk_smp();

    spin_lock_init(&(nkdev.xirq_b_lock));

    nkdev.xirqs       = ((NkXIrqMask*)PTOV(NKCTX()->xirqs));
    nkdev.running     = (NkOsMask*)PTOV(NKCTX()->running);
    if (smp) {
        nkdev.xpending[0] = ((NkXIrqMask*)PTOV(NKCTX()->xirqs_pending));
    } else {
        nkdev.xpending[0] = &nkdev.xirqs[NKCTX()->id];
    }

    if (NKCTX()->xirqs_affinity) {
	nkdev.affinity = (NkCpuMask*)PTOV(NKCTX()->xirqs_affinity);
    }

        /*
	 * Lookup for the virtual XPIC device
	 * (... created by the Core back-end driver)
	 */
    while (!(pdev = nkops.nk_dev_lookup_by_type(NK_DEV_ID_XPIC, 0))) {
	if (!vdev) {
            printnk("NK: Waiting for a virtual PIC device...\n");
	    vdev = (NkDevDesc*)1;
	}
    }
    printnk("NK: Using virtual PIC\n");
        /*
	 * Initialize global variables and our OS stuff
	 * in the XPIC device data.
	 */
    nkosmask     = nk_bit_mask(myid);
    vdev         = (NkDevDesc*)nkops.nk_ptov(pdev);
    nkxpic       = (NkDevXPic*)nkops.nk_ptov(vdev->dev_header);
    nkxpic_owner = (vdev->dev_owner ? vdev->dev_owner : NK_OS_PRIM);

        /*
	 * Install the interrupt handlers for the XIRQs
	 */
    for (xirq = NK_XIRQ_SYSCONF ; xirq < NK_XIRQ_LIMIT ; xirq++) {
	irq_set_chip_and_handler(xirq, &nk_xirq_chip, handle_edge_irq);
	set_irq_flags(xirq, IRQF_VALID);
    }

    if (smp) {
	nkops.nk_dev_alloc    = nk_smp_dev_alloc;
	nkops.nk_xirq_alloc   = nk_smp_xirq_alloc;
	nkops.nk_dev_add      = nk_smp_dev_add;
	nkops.nk_pdev_alloc   = nk_smp_pdev_alloc;
	nkops.nk_pmem_alloc   = nk_smp_pmem_alloc;
	nkops.nk_pxirq_alloc  = nk_smp_pxirq_alloc;
	nkops.nk_xirq_trigger = NKCTX()->smp_irq_post;
    }
}

#ifdef CONFIG_SMP

    void __cpuinit
nk_ddi_cpu_init (void)
{
    unsigned int cpu = NKCTX()->vcpuid;
    nkdev.xpending[cpu] = (NkXIrqMask*)PTOV(NKCTX()->xirqs_pending);
}

#endif

EXPORT_SYMBOL(nkops);

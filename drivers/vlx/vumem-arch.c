/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual User MEMory Buffers (VUMEM).                      *
 *             Architecture-specific VUMEM front-end/back-end drivers        *
 *             services.                                                     *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Ltd.                                        *
 *                                                                           *
 *  This program is free software: you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License Version 2           *
 *  as published by the Free Software Foundation.                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  Version 2 along with this program.                                       *
 *  If not, see <http://www.gnu.org/licenses/>.                              *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#include "vumem.h"
#include <linux/smp.h>
#include <linux/mm.h>
#include <linux/sched.h>

#if defined(__arm__)

#include <asm/mmu_context.h>
#ifdef CONFIG_NKERNEL
#include <asm/nkern.h>
#endif

#define VUMEM_CACHE_FULL_OP_LIMIT	(1024 * 128)

    /*
     *
     */
    static inline unsigned int
vumem_need_cache_sync (unsigned int cache_policy, VumemCacheOp cache_op)
{
    if (cache_policy < VUMEM_CACHE_WRITETHROUGH) {
	return 0;
    }

    if ((cache_op & VUMEM_CACHE_SYNC_DEVICE) && !arch_is_coherent()) {
	return 1;
    }

    if ((cache_op & VUMEM_CACHE_SYNC_MEMORY) &&
	(cache_is_vivt() || cache_is_vipt_aliasing())) {
	return 1;
    }

    return 0;
}

    /*
     *
     */
    static inline unsigned int
vumem_cache_outer_present (void)
{
#ifdef CONFIG_OUTER_CACHE
    return 1;
#else
    return 0;
#endif
}

    /*
     *
     */
    unsigned int
vumem_cache_attr_get (pgprot_t prot)
{
    unsigned int lcp = (pgprot_val(prot) & L_PTE_MT_MASK);
    unsigned int cp;
    unsigned int attr;

    if (prot == pgprot_noncached(prot)) {
	cp = VUMEM_CACHE_NONE;
    } else if (prot == pgprot_writecombine(prot)) {
	cp = VUMEM_CACHE_WRITECOMBINE;
    } else if (lcp == L_PTE_MT_WRITETHROUGH) {
	cp = VUMEM_CACHE_WRITETHROUGH;
    } else if (lcp == L_PTE_MT_WRITEBACK) {
	cp = VUMEM_CACHE_WRITEBACK;
    } else if (lcp == L_PTE_MT_WRITEALLOC) {
	cp = VUMEM_CACHE_WRITEALLOC;
    } else {
	BUG();
    }

    attr = VUMEM_ATTR_CACHE_POLICY(cp);

    if (vumem_need_cache_sync(cp, VUMEM_CACHE_SYNC_MEMORY)) {
	attr |= VUMEM_ATTR_NEED_CACHE_SYNC_MEM;
    }

    if (vumem_need_cache_sync(cp, VUMEM_CACHE_SYNC_DEVICE)) {
	attr |= VUMEM_ATTR_NEED_CACHE_SYNC_DEV;
    }

    if (vumem_cache_outer_present() && (cp >= VUMEM_CACHE_WRITETHROUGH)) {
	attr |= VUMEM_ATTR_CACHE_OUTER_ENABLED;
    }

    return attr;
}

    /*
     *
     */
    pgprot_t
vumem_cache_prot_get (pgprot_t prot, unsigned int cache_policy)
{
    pgprot_t nprot = prot & ~L_PTE_MT_MASK;

    if (cache_policy == VUMEM_CACHE_NONE) {
	nprot = pgprot_noncached(prot);
    } else if (cache_policy == VUMEM_CACHE_WRITECOMBINE) {
	nprot = pgprot_writecombine(prot);
    } else if (cache_policy == VUMEM_CACHE_WRITETHROUGH) {
	nprot |= L_PTE_MT_WRITETHROUGH;
    } else if (cache_policy == VUMEM_CACHE_WRITEBACK) {
	nprot |= L_PTE_MT_WRITEBACK;
    } else if (cache_policy == VUMEM_CACHE_WRITEALLOC) {
	nprot |= L_PTE_MT_WRITEALLOC;
    } else {
	BUG();
    }

    return nprot;
}

    /*
     *
     */
    static void
vumem_cache_op_on_other_cpus (void (*func)(void *))
{
#ifdef CONFIG_SMP
    static unsigned int prev_cpu_last_asid = 0;
    const cpumask_t*    cpu_mask;

    cpu_mask = mm_cpumask(current->mm);
    smp_mb();
    if (cpu_last_asid != prev_cpu_last_asid) {
	cpu_mask = cpu_online_mask;
	prev_cpu_last_asid = cpu_last_asid;
    }

    preempt_disable();
    smp_call_function_many(cpu_mask, func, NULL, 1);
    preempt_enable();
#endif
}

    /*
     *
     */
    static void
vumem_inner_cache_clean_full_local (void* unused)
{
#ifdef CONFIG_NKERNEL
    os_ctx->wsync_all();
#else
#warning using non-optimal cache clean operation
    flush_cache_all();
#endif
}

    /*
     *
     */
    static int
vumem_inner_cache_clean_range_local (void* start, VumemSize size)
{
#if defined(dmac_map_area)
    dmac_map_area(start, size, DMA_TO_DEVICE);
#else
    dmac_clean_range(start, (nku8_f*)start + size);
#endif
    return 0;
}

    /*
     *
     */
    static int
vumem_inner_cache_clean_range (void* start, VumemSize size)
{
    if (start != VUMEM_BUFFER_NO_VADDR) {
	vumem_cache_op_on_other_cpus(vumem_inner_cache_clean_full_local);
	if (size > VUMEM_CACHE_FULL_OP_LIMIT) {
	    vumem_inner_cache_clean_full_local(NULL);
	} else {
	    return vumem_inner_cache_clean_range_local(start, size);
	}
    }
    return 0;
}

    /*
     *
     */
    static void
vumem_inner_cache_flush_full_local (void* unused)
{
    flush_cache_all();
}

    /*
     *
     */
    static int
vumem_inner_cache_flush_range_local (void* start, VumemSize size)
{
    dmac_flush_range(start, (nku8_f*)start + size);
    return 0;
}

    /*
     *
     */
    static int
vumem_inner_cache_flush_range (void* start, VumemSize size)
{
    if (start != VUMEM_BUFFER_NO_VADDR) {
	vumem_cache_op_on_other_cpus(vumem_inner_cache_flush_full_local);
	if (size > VUMEM_CACHE_FULL_OP_LIMIT) {
	    vumem_inner_cache_flush_full_local(NULL);
	} else {
	    return vumem_inner_cache_flush_range_local(start, size);
	}
    }
    return 0;
}

    /*
     *
     */
    static int
vumem_outer_cache_clean_layout (VumemBufferLayout* layout)
{
    unsigned int i;
    int          diag = 0;

    if (layout->attr & VUMEM_ATTR_CACHE_OUTER_ENABLED) {
	if (vumem_cache_outer_present()) {
	    for (i = 0; i < layout->chunks_nr; i++) {
		unsigned long pstart = layout->chunks[i].pfn << PAGE_SHIFT;
		outer_clean_range(pstart, pstart + layout->chunks[i].size);
	    }
	} else{
	    diag = -ENOSYS;
	}
    }

    return diag;
}

    /*
     *
     */
    static int
vumem_outer_cache_flush_layout (VumemBufferLayout* layout)
{
    unsigned int i;
    int          diag = 0;

    if (layout->attr & VUMEM_ATTR_CACHE_OUTER_ENABLED) {
	if (vumem_cache_outer_present()) {
	    for (i = 0; i < layout->chunks_nr; i++) {
		unsigned long pstart = layout->chunks[i].pfn << PAGE_SHIFT;
		outer_flush_range(pstart, pstart + layout->chunks[i].size);
	    }
	} else{
	    diag = -ENOSYS;
	}
    }

    return diag;
}

    /*
     *
     */
    static int
vumem_cache_sync_to_memory (VumemBufferLayout* layout, void* start)
{
    return vumem_inner_cache_clean_range(start, layout->size);
}

    /*
     *
     */
    static int
vumem_cache_sync_from_memory (VumemBufferLayout* layout, void* start)
{
    return vumem_inner_cache_flush_range(start, layout->size);
}

    /*
     *
     */
    static int
vumem_cache_sync_to_device (VumemBufferLayout* layout, void* start)
{
    int diag;

    diag = vumem_inner_cache_clean_range(start, layout->size);
    if (diag) {
	return diag;
    }

    diag = vumem_outer_cache_clean_layout(layout);

    return diag;
}

    /*
     *
     */
    static int
vumem_cache_sync_from_device (VumemBufferLayout* layout, void* start)
{
    int diag;

    diag = vumem_inner_cache_flush_range(start, layout->size);
    if (diag) {
	return diag;
    }

    diag = vumem_outer_cache_flush_layout(layout);

    return diag;
}

    /*
     *
     */
    static  int
vumem_cache_sync_none (VumemBufferLayout* layout, void* start)
{
    return 0;
}

    /*
     *
     */
    int
vumem_cache_op (VumemCacheOp cache_op, VumemBufferLayout* layout, void* start)
{
    static int (*ops[]) (VumemBufferLayout* layout, void* start) = {
	/* ! VUMEM_CACHE_LOCAL */
	vumem_cache_sync_none,		/* 0000                             */
	vumem_cache_sync_to_memory,	/* 0001               SYNC_TO_MEM   */
	vumem_cache_sync_from_memory,	/* 0010               SYNC_FROM_MEM */
	vumem_cache_sync_from_memory,	/* 0011               SYNC_MEM      */
	vumem_cache_sync_to_device,	/* 0100 SYNC_TO_DEV                 */
	vumem_cache_sync_to_device,	/* 0101 SYNC_TO_DEV  |SYNC_TO_MEM   */
	vumem_cache_sync_from_device,	/* 0110 SYNC_TO_DEV  |SYNC_FROM_MEM */
	vumem_cache_sync_from_device,	/* 0111 SYNC_TO_DEV  |SYNC_MEM      */
	vumem_cache_sync_from_device,	/* 1000 SYNC_FROM_DEV               */
	vumem_cache_sync_from_device,	/* 1001 SYNC_FROM_DEV|SYNC_TO_MEM   */
	vumem_cache_sync_from_device,	/* 1010 SYNC_FROM_DEV|SYNC_FROM_MEM */
	vumem_cache_sync_from_device,	/* 1011 SYNC_FROM_DEV|SYNC_MEM      */
	vumem_cache_sync_from_device,	/* 1100 SYNC_DEV                    */
	vumem_cache_sync_from_device,	/* 1101 SYNC_DEV     |SYNC_TO_MEM   */
	vumem_cache_sync_from_device,	/* 1110 SYNC_DEV     |SYNC_FROM_MEM */
	vumem_cache_sync_from_device,	/* 1111 SYNC_DEV     |SYNC_MEM      */
	/* VUMEM_CACHE_LOCAL */
 	vumem_cache_sync_none,		/* 0000                             */
	vumem_cache_sync_to_memory,	/* 0001               SYNC_TO_MEM   */
	vumem_cache_sync_from_memory,	/* 0010               SYNC_FROM_MEM */
	vumem_cache_sync_from_memory,	/* 0011               SYNC_MEM      */
	vumem_cache_sync_to_memory,	/* 0100 SYNC_TO_DEV                 */
	vumem_cache_sync_to_memory,	/* 0101 SYNC_TO_DEV  |SYNC_TO_MEM   */
	vumem_cache_sync_from_memory,	/* 0110 SYNC_TO_DEV  |SYNC_FROM_MEM */
	vumem_cache_sync_from_memory,	/* 0111 SYNC_TO_DEV  |SYNC_MEM      */
	vumem_cache_sync_from_memory,	/* 1000 SYNC_FROM_DEV               */
	vumem_cache_sync_from_memory,	/* 1001 SYNC_FROM_DEV|SYNC_TO_MEM   */
	vumem_cache_sync_from_memory,	/* 1010 SYNC_FROM_DEV|SYNC_FROM_MEM */
	vumem_cache_sync_from_memory,	/* 1011 SYNC_FROM_DEV|SYNC_MEM      */
	vumem_cache_sync_from_memory,	/* 1100 SYNC_DEV                    */
	vumem_cache_sync_from_memory,	/* 1101 SYNC_DEV     |SYNC_TO_MEM   */
	vumem_cache_sync_from_memory,	/* 1110 SYNC_DEV     |SYNC_FROM_MEM */
	vumem_cache_sync_from_memory,	/* 1111 SYNC_DEV     |SYNC_MEM      */
   };
    VumemBufferAttr attr;
    unsigned int    cp;

    cache_op &= (VUMEM_CACHE_SYNC_MEMORY |
		 VUMEM_CACHE_SYNC_DEVICE |
		 VUMEM_CACHE_LOCAL);

    attr = layout->attr;

    if (!(((cache_op & VUMEM_CACHE_SYNC_MEMORY) &&
	   (attr     & VUMEM_ATTR_NEED_CACHE_SYNC_MEM)) ||
	  ((cache_op & VUMEM_CACHE_SYNC_DEVICE) &&
	   (attr     & VUMEM_ATTR_NEED_CACHE_SYNC_DEV)))) {
	return 0;
    }

    cp = VUMEM_ATTR_CACHE_POLICY_GET(attr);

    if ((!(cache_op & (VUMEM_CACHE_SYNC_FROM_MEMORY |
		       VUMEM_CACHE_SYNC_FROM_DEVICE))) &&
	(cp < VUMEM_CACHE_WRITEBACK)) {
	return 0;
    }

    return ops[cache_op](layout, start);
}

#else
#error cache functions not implemented for this architecture
#endif

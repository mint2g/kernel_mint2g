/*****************************************************************************
 *                                                                           *
 *  Component: VLX PMEM user-land mapping support.                           *
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

#ifndef _VLX_PMEM_USER_H
#define _VLX_PMEM_USER_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <nk/nkern.h>

#if defined(__arm__)

#define PMEM_USER_ARM

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>

#define CPU_VMSAv4		1
#define CPU_XSCALE		2
#define CPU_VMSAv6		3
#define CPU_VMSAv6_XP		4

#define pmd_type_table(pmd)	\
    ((pmd_val(pmd) & PMD_TYPE_MASK) == PMD_TYPE_TABLE)

#define pmd_type_section(pmd)	\
    ((pmd_val(pmd) & PMD_TYPE_MASK) == PMD_TYPE_SECT)

#define pmd_pgtab(pmd)		\
    (((pte_t*)(__va(pmd_val(pmd) & ~(PTRS_PER_PTE * sizeof(void *) - 1))))-PTRS_PER_PTE) /* FIXME SL */

#define pte_type_lpage(pte)	\
    ((pte_val(pte) & PTE_TYPE_MASK) == PTE_TYPE_LARGE)

#define pte_type_small(pte)	\
    ((pte_val(pte) & PTE_TYPE_MASK) == PTE_TYPE_SMALL)

#define pte_type_ext(pte)	\
    ((pte_val(pte) & PTE_TYPE_MASK) == PTE_TYPE_EXT)

#define PTRS_PER_LPAGE		16
#define LPAGES_PER_PTE		(PTRS_PER_PTE / PTRS_PER_LPAGE)
#define LPAGE_SIZE		(1 << 16)
#define LPAGE_MASK		(~(LPAGE_SIZE - 1))

#define PMEM_USER_PERF_ALIGN	LPAGE_SIZE

typedef struct pmd_update {
    unsigned int           section;
    unsigned long          va;
    unsigned long          pmdval;
    pmd_t*                 pmdp;
} pmd_update_t;

typedef struct pgtab {
    pte_t*                 ptep;
    struct list_head       link;
} pgtab_t;

typedef struct pmem_region_arch {
    pmd_update_t*          pmd_updates;
    unsigned int           updates_nr;
    struct list_head       pgtab_free;
    struct list_head       pgtab_used;
} pmem_region_arch_t;

typedef struct pmem_mm_arch {
    unsigned long          vstart;
    unsigned long          vend;
    struct list_head       pgtab_used;
    struct mutex           lock_map;
} pmem_mm_arch_t;

#else

#define PMEM_USER_GENERIC

#define PMEM_USER_PERF_ALIGN	PAGE_SIZE

typedef struct pmem_region_arch {
} pmem_region_arch_t;

typedef struct pmem_mm_arch {
} pmem_mm_arch_t;

#endif

#define PMEM_MAP_AT_INIT	1
#define PMEM_MAP_AT_FORK	2

#define PMEM_UNMAP_AT_EXIT	1
#define PMEM_UNMAP_AT_ERROR	2

struct pmem_mm;
struct pmem_region;

typedef struct pmem_arch_ops {
    int  (*pmem_mm_arch_init)    (struct pmem_mm*        pmem_mm);
    void (*pmem_mm_arch_cleanup) (struct pmem_mm*        pmem_mm);
    int  (*region_arch_init)     (struct pmem_region*    region);
    void (*region_arch_cleanup)  (struct pmem_region*    region);  
    int  (*vma_map)              (struct vm_area_struct* vma,
				  unsigned int           map_type);
    void (*vma_unmap)            (struct vm_area_struct* vma,
				  unsigned int           unmap_type);
} pmem_arch_ops_t;

typedef struct pmem_region {
    struct pmem_mm*        pmem_mm;
    NkPhAddr               pstart;
    NkPhSize               psize;
    unsigned long          vstart;
    unsigned int           mapped;
    pmem_region_arch_t     arch;
    struct vm_area_struct* vma;
    struct list_head       link;
} pmem_region_t;

typedef struct pmem_mm {
    struct mm_struct*      mm;
    struct list_head       regions;
    atomic_t               refcnt;
    pmem_mm_arch_t         arch;
    pmem_arch_ops_t*       ops;
    struct list_head       link;
    struct mutex           lock;
} pmem_mm_t;

#define PMEM_VMA_ATTACHED(vma,rgn)	((rgn)->vma == (vma))
#define PMEM_REGION_MAPPED(rgn)		((rgn)->mapped)

extern void* nk_pmem_map_user (struct vm_area_struct *vma, NkPhAddr addr);

#endif /* _VLX_PMEM_USER_H */

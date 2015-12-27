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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/fs.h>
#include <linux/file.h>
#include "pmem-user.h"

#if defined(PMEM_USER_ARM)

#include <asm/mach/map.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <../mm/mm.h>

static void     pmem_pgtab_do_free (pgtab_t* pgtab);
static pgtab_t* pmem_pgtab_find (struct list_head* list, pte_t* ptep);

static unsigned long prot_sect_k;
static int           cpu_type;
static int           tlb_use_asid;

    /*
     *
     */
    static int
pmem_mm_init_arm (pmem_mm_t* pmem_mm)
{
    pmem_mm->arch.vstart = -1;
    pmem_mm->arch.vend   = 0;

    INIT_LIST_HEAD(&pmem_mm->arch.pgtab_used);
    mutex_init(&pmem_mm->arch.lock_map);

    return 0;
}

    /*
     *
     */
    static void
pmem_mm_cleanup_arm (pmem_mm_t* pmem_mm)
{
    unsigned long va;
    unsigned long vend;
    unsigned long vnext;
    pgd_t*        pgd;
    pgd_t*        pgd_k;
    pmd_t*        pmdp;
    pmd_t*        pmdp_k;
    pmd_t         pmd;
    pte_t*        ptep;
    pgtab_t*      pgtab;

    va   = pmem_mm->arch.vstart;
    vend = pmem_mm->arch.vend;
    pgd  = pgd_offset(pmem_mm->mm, va);

    while (va < vend) {
	vnext = pgd_addr_end(va, vend);

	pmdp = pmd_offset(pgd, va);
	pmd  = *pmdp;

	if (pmd_type_table(pmd) &&
	    ((pmd & PMD_DOMAIN(0xf)) == PMD_DOMAIN(DOMAIN_USER))) {
	    ptep  = pmd_pgtab(pmd);
	    pgtab = pmem_pgtab_find(&pmem_mm->arch.pgtab_used, ptep);
	    if (pgtab) {
		pgd_k  = pgd_offset_k(va);
		pmdp_k = pmd_offset(pgd_k, va);

		spin_lock(&pmem_mm->mm->page_table_lock);
		copy_pmd(pmdp, pmdp_k);
		spin_unlock(&pmem_mm->mm->page_table_lock);

		mb();

		list_del(&pgtab->link);
		pmem_pgtab_do_free(pgtab);
	    }
	}

	pgd++;
	va = vnext;	
    }

    BUG_ON(!list_empty(&pmem_mm->arch.pgtab_used));

    flush_tlb_all();
}

    /*
     *
     */
    static inline void
pmem_mm_vspace_update (pmem_mm_t* pmem_mm, unsigned long vstart,
		       unsigned long vend)
{
    if (vstart < pmem_mm->arch.vstart) {
	pmem_mm->arch.vstart = vstart;
    }
    if (vend > pmem_mm->arch.vend) {
	pmem_mm->arch.vend = vend;
    }
}

    /*
     *
     */
    static int
pmem_region_init_arm (pmem_region_t* region)
{
    unsigned long size;
    unsigned int  pmd_max;

    size    = (region->psize + 2*PMD_SIZE - 1) & PMD_MASK;
    pmd_max = (pmd_offset(pgd_offset_k(size), size) -
	       pmd_offset(pgd_offset_k(0), 0));

    region->arch.pmd_updates = kmalloc(pmd_max * sizeof(pmd_update_t),
				       GFP_KERNEL);
    if (!region->arch.pmd_updates) {
	return -ENOMEM;
    }

    region->arch.updates_nr  = 0;

    INIT_LIST_HEAD(&region->arch.pgtab_free);
    INIT_LIST_HEAD(&region->arch.pgtab_used);

    return 0;
}

    /*
     *
     */
    static void
pmem_region_cleanup_arm (pmem_region_t* region)
{
    pgtab_t* pgtab;

    BUG_ON(!list_empty(&region->arch.pgtab_free));

    while (!list_empty(&region->arch.pgtab_used)) {
	pgtab = list_first_entry(&region->arch.pgtab_used, pgtab_t, link);
	list_del(&pgtab->link);
	list_add(&pgtab->link, &region->pmem_mm->arch.pgtab_used);
    }

    kfree(region->arch.pmd_updates);
}

    /*
     *
     */
    static inline int
pmem_is_init_pgtab (unsigned long va, pte_t* ptep)
{
    pmd_t* pmdp;

    pmdp = pmd_offset(pgd_offset_k(va), va);

    return pmd_pgtab(*pmdp) == ptep;
}

    /*
     *
     */
    static noinline void
pmem_pgtab_do_free (pgtab_t* pgtab)
{
    free_page((unsigned long) pgtab->ptep);
    kfree(pgtab);
}

    /*
     *
     */
    static noinline pgtab_t*
pmem_pgtab_do_alloc (void)
{
    pgtab_t* pgtab;
    pte_t*   ptep;

    pgtab = kmalloc(sizeof(pgtab_t), GFP_KERNEL);
    if (!pgtab) {
	return NULL;
    }

    ptep = (pte_t*) __get_free_page(PGALLOC_GFP);
    if (!ptep) {
	kfree(pgtab);
	return NULL;
    }

    pgtab->ptep = ptep;
    return pgtab;
}

    /*
     *
     */
    static noinline pgtab_t*
pmem_pgtab_find (struct list_head* list, pte_t* ptep)
{
    pgtab_t* pgtab;

    list_for_each_entry(pgtab, list, link) {
	if (pgtab->ptep == ptep) {
	    return pgtab;
	}
    }

    return NULL;
}
    /*
     *
     */
    static noinline int
pmem_pgtab_free (pmem_region_t* region, pte_t* ptep)
{
    pmem_mm_t* pmem_mm = region->pmem_mm;
    pgtab_t*   pgtab;

    pgtab = pmem_pgtab_find(&region->arch.pgtab_used, ptep);
    if (!pgtab) {
	pmem_region_t* iter;
	list_for_each_entry(iter, &pmem_mm->regions, link) {
	    if (iter == region) {
		continue;
	    }
	    pgtab = pmem_pgtab_find(&iter->arch.pgtab_used, ptep);
	    if (pgtab) {
		break;
	    }
	}
	if (!pgtab) {
	    pgtab = pmem_pgtab_find(&pmem_mm->arch.pgtab_used, ptep);
	}
    }

    if (pgtab) {
	list_del(&pgtab->link);
	pmem_pgtab_do_free(pgtab);
    }

    return (pgtab == NULL);
}

    /*
     *
     */
    static noinline pte_t*
pmem_pgtab_alloc (pmem_region_t* region, unsigned int map)
{
    pgtab_t* pgtab;
    pgtab_t* pgtab_unmap;

    if (map) {
	pgtab = pmem_pgtab_do_alloc();
	if (!pgtab) {
	    return NULL;
	}
	pgtab_unmap = pmem_pgtab_do_alloc();
	if (!pgtab_unmap) {
	    pmem_pgtab_do_free(pgtab);
	    return NULL;
	}
	list_add(&pgtab_unmap->link, &region->arch.pgtab_free);
    } else {
	BUG_ON(list_empty(&region->arch.pgtab_free));
	pgtab = list_first_entry(&region->arch.pgtab_free, pgtab_t, link);
	list_del(&pgtab->link);
    }

    list_add(&pgtab->link, &region->arch.pgtab_used);

    return pgtab->ptep;
}

    /*
     *
     */
    static inline unsigned long
pmem_sect_prot_get (unsigned int map)
{
    unsigned long prot;

    prot = prot_sect_k;				/* Kernel section prot */

    if (map) {
	prot &= ~PMD_DOMAIN(0xf);		/* Remove kernel domain */
	prot |=  PMD_DOMAIN(DOMAIN_USER);	/* Set user domain */
	prot |=  PMD_SECT_AP_READ;		/* Set user access rights */
    }

    return prot;
}

    /*
     *
     */
    static inline unsigned long
pmem_pte_prot_get (unsigned int map)
{
    unsigned long prot;

    prot = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY;

    if (map) {
	prot |= L_PTE_USER;
	prot |= pgprot_val(pgprot_user);
    } else {
	prot |= pgprot_val(pgprot_kernel);
    }

    return prot;
}

    /*
     *
     */
    static noinline pte_t
pmem_spte_to_lpte (pte_t pte)
{
    unsigned long opteval = pte_val(pte);
    unsigned long pteval  = PTE_TYPE_LARGE;

    if (cpu_type == CPU_VMSAv6_XP) {
	pteval |= (opteval & (LPAGE_MASK | ~PAGE_MASK)
		   & ~PTE_TYPE_MASK
		   & ~PTE_EXT_TEX(0x7));
	pteval |= (opteval & PTE_EXT_TEX(0x7)) << 6;
	pteval |= (opteval & PTE_EXT_XN) << 15;
    } else if (pte_type_small(pte)) {
	pteval |= opteval & (LPAGE_MASK | ~PAGE_MASK) & ~PTE_TYPE_MASK;
    } else {
	unsigned long ap;
	BUG_ON(!pte_type_ext(pte));
	pteval |= opteval & LPAGE_MASK;
	pteval |= opteval & (PTE_BUFFERABLE | PTE_CACHEABLE);
	pteval |= (opteval & PTE_EXT_TEX(0xf)) << 6;
	ap      = opteval & (PTE_EXT_AP0 | PTE_EXT_AP1);
	ap      = ap | (ap << 2);
	ap      = ap | (ap << 4);
	pteval |= ap;
    }

    return __pte(pteval);
}

    /*
     *
     */
    static inline void
pmem_set_pte (pte_t* ptep, pte_t pte)
{
    set_pte_ext(ptep, pte, tlb_use_asid ? PTE_EXT_NG : 0);
}

    /*
     *
     */
    static noinline void
pmem_pgtab_lpages_create (pte_t* ptep)
{
    unsigned int  lpg;
    unsigned int  spg;
    unsigned int  i;
    unsigned long pteval;
    pte_t         lpte;

    for (lpg = 0; lpg < LPAGES_PER_PTE; lpg++) {

	spg    = lpg * PTRS_PER_LPAGE;
	pteval = pte_val(ptep[spg]);

	if (pteval & ~(LPAGE_MASK | ~PAGE_MASK)) {
	    continue;
	}

	for (i = 1; i < PTRS_PER_LPAGE; i++) {
	    pteval += PAGE_SIZE;
	    if (pteval != pte_val(ptep[spg + i])) {
		break;
	    }
	}

	if (i == PTRS_PER_LPAGE) {
	    lpte = pmem_spte_to_lpte(ptep[spg]);
	    for (i = 0; i <  PTRS_PER_LPAGE; i++) {
		ptep[spg + i] = lpte;
	    }
	}
    }
}

    /*
     *
     */
    static noinline void
pmem_pgtab_init (pmd_t* pmdp, pte_t* ptep)
{
    pmd_t         pmd;
    pte_t*        optep;
    unsigned long pfn;
    unsigned int  prot;
    unsigned int  i;

    pmd   = *pmdp;
    ptep += 0 /* FIXME SL */;

    if (pmd_type_table(pmd)) {

	optep = pmd_pgtab(pmd) /* FIXME SL */;

	for (i = 0; i < PTRS_PER_PTE; i++, optep++, ptep++) {
	    pmem_set_pte(ptep, *optep);
	}

    } else if (pmd_type_section(pmd)) {

	BUG_ON(pmd_val(pmd) & PMD_SECT_SUPER);

	prot  = pmem_pte_prot_get(pmd_val(pmd) & PMD_SECT_AP_READ);
	pfn   = __phys_to_pfn(pmd_val(pmd) & SECTION_MASK);

	for (i = 0; i < PTRS_PER_PTE; i++, pfn++, ptep++) {
	    pmem_set_pte(ptep, pfn_pte(pfn, __pgprot(prot)));
	}

    } else {
	BUG();
    }
}

    /*
     *
     */
    static inline void
pmem_pmd_update (pmem_region_t* region, unsigned int section,
		 unsigned long va, unsigned long pmdval, pmd_t* pmdp)
{
    pmd_update_t* pmd_update;

    pmd_update = &region->arch.pmd_updates[region->arch.updates_nr++];

    pmd_update->section = section;
    pmd_update->va      = va;
    pmd_update->pmdval  = pmdval;
    pmd_update->pmdp    = pmdp;
}

    /*
     *
     */
    static noinline int
pmem_remap_pte_arm (pmem_region_t* region, pmd_t* pmdp,
		    unsigned long va, unsigned long vend,
		    unsigned long pfn, unsigned int map)
{
    unsigned long sva = va;
    pte_t*        ptep;
    pte_t*        sptep;
    unsigned int  prot;

    sptep = ptep = pmem_pgtab_alloc(region, map);
    if (!ptep) {
	return -ENOMEM;
    }

    pmem_pgtab_init(pmdp, ptep);

    prot  = pmem_pte_prot_get(map);
    ptep += (0 + pte_index(va)); /* FIXME SL */

    do {
	BUG_ON(tlb_use_asid && !(pte_val(*(ptep+PTRS_PER_PTE)) & PTE_EXT_NG)); /* FIXME SL */
	pmem_set_pte(ptep, pfn_pte(pfn, __pgprot(prot)));
	pfn++;
    } while (ptep++, va += PAGE_SIZE, va != vend);

    pmem_pgtab_lpages_create(sptep + PTRS_PER_PTE); /* FIXME SL */

    clean_dcache_area(sptep + PTRS_PER_PTE, sizeof(pte_t) * PTRS_PER_PTE); /* FIXME SL */
    pmem_pmd_update(region, 0, sva, (__pa(sptep + PTRS_PER_PTE) | _PAGE_USER_TABLE), pmdp); /* FIXME SL */

    return 0;
}

    /*
     *
     */
    static noinline int
pmem_remap_section_arm (pmem_region_t* region, pgd_t* pgd,
			unsigned long va, unsigned long vend, unsigned long pa,
			unsigned int map)
{
    pmd_t*          pmdp = pmd_offset(pgd, addr);
    unsigned int    prot;
    int             diag;

    if ((((va | vend | pa) & ~SECTION_MASK) == 0) && !pmd_type_table(*pmdp)) {
	if (va & SECTION_SIZE) {
	    pmdp++;
	}

	prot = pmem_sect_prot_get(map);

	do {
	    BUG_ON(tlb_use_asid && !(pmd_val(*pmdp) & PMD_SECT_nG));

	    pmem_pmd_update(region, 1, va, (pa | prot), pmdp);

	    pa += SECTION_SIZE;
	} while (pmdp++, va += SECTION_SIZE, va != vend);
	
    } else {
	diag = pmem_remap_pte_arm(region, pmdp, va, vend, __phys_to_pfn(pa),
				  map);
	if (diag) {
	    return diag;
	}
    }

    return 0;
}

    /*
     *
     */
    static noinline int
pmem_region_remap_arm (pmem_region_t* region, unsigned long vstart,
		       unsigned int map)
{
    pmem_mm_t*             pmem_mm = region->pmem_mm;
    pmd_update_t*          pmd_update;
    unsigned long          vend;
    unsigned long          va;
    unsigned long          pa;
    pgd_t*                 pgd;
    unsigned int           i;
    int                    diag;

    region->arch.updates_nr = 0;

    pa   = region->pstart;
    pgd  = pgd_offset(pmem_mm->mm, vstart);
    vend = vstart + region->psize;
    va   = vstart;

    do {
	unsigned long vnext = pgd_addr_end(va, vend);

	diag = pmem_remap_section_arm(region, pgd, va, vnext, pa, map);
	if (diag) {
	    goto out_clean;
	}

	pa += vnext - va;
	va  = vnext;
    } while (pgd++, va != vend);

    BUG_ON(!map && !list_empty(&region->arch.pgtab_free));

    pmd_update = region->arch.pmd_updates;

    for (i = 0; i < region->arch.updates_nr; i++) {
	pmd_t* pmdp = pmd_update->pmdp;
	pmd_t  pmd;
	pte_t* ptep;

	spin_lock(&pmem_mm->mm->page_table_lock);

	pmd = *pmdp;

	if (pmd_update->section) {
	    *pmdp = __pmd(pmd_update->pmdval);
	    flush_pmd_entry(pmdp);
	} else {
	    __pmd_populate(pmdp, __pmd(pmd_update->pmdval - PTE_HWTABLE_OFF), 0); /* FIXME SL */
	}

	spin_unlock(&pmem_mm->mm->page_table_lock);

	mb();

	if (pmd_type_table(pmd)) {
	    ptep = pmd_pgtab(pmd);
	    if (pmem_pgtab_free(region, ptep) != 0) {
		BUG_ON(!pmem_is_init_pgtab(pmd_update->va, ptep));
	    }
	}

	pmd_update++;
    }

    flush_tlb_all();

    return 0;

out_clean:
    BUG_ON(!map);

    pmd_update = region->arch.pmd_updates;

    for (i = 0; i < region->arch.updates_nr; i++) {

	if (!pmd_update->section) {
	    pte_t* ptep = pmd_pgtab(__pmd(pmd_update->pmdval));
	    BUG_ON(pmem_pgtab_free(region, ptep) != 0);
	}

	pmd_update++;
    }

    BUG_ON(!list_empty(&region->arch.pgtab_used));

    while (!list_empty(&region->arch.pgtab_free)) {
	pgtab_t* pgtab;

	pgtab = list_first_entry(&region->arch.pgtab_free, pgtab_t, link);
	list_del(&pgtab->link);

	pmem_pgtab_do_free(pgtab);
    }

    return diag;
}

    /*
     *
     */
    static int
pmem_vma_map_arm (struct vm_area_struct* vma, unsigned int map_type)
{
    pmem_region_t* region = vma->vm_private_data;
    pmem_mm_t*     pmem_mm;
    unsigned long  vstart;
    int            diag;

    BUG_ON((!region) ||
	   (!PMEM_VMA_ATTACHED(vma, region)) ||
	   (PMEM_REGION_MAPPED(region)));

    BUG_ON((map_type != PMEM_MAP_AT_INIT) &&
	   (map_type != PMEM_MAP_AT_FORK));

    pmem_mm = region->pmem_mm;

    mutex_lock(&pmem_mm->arch.lock_map);

    vstart = (unsigned long) nkops.nk_mem_map(region->pstart, region->psize);
    if (vstart == 0) {
	diag = -EFAULT;
    } else {
	diag = pmem_region_remap_arm(region, vstart, 1);
	if (!diag) {
	    region->vstart = vstart;
	    region->mapped = 1;
	    pmem_mm_vspace_update(pmem_mm, vstart, vstart + region->psize);
	}
    }

    mutex_unlock(&pmem_mm->arch.lock_map);

    return diag;
}

    /*
     *
     */
    static void
pmem_vma_unmap_arm (struct vm_area_struct* vma, unsigned int unmap_type)
{
    pmem_region_t* region = vma->vm_private_data;
    pmem_mm_t*     pmem_mm;

    BUG_ON(!region);

    BUG_ON((unmap_type != PMEM_UNMAP_AT_ERROR) &&
	   (unmap_type != PMEM_UNMAP_AT_EXIT));

    if ((!PMEM_VMA_ATTACHED(vma, region)) ||
	(!PMEM_REGION_MAPPED(region))) {
	return;
    }

    pmem_mm = region->pmem_mm;

    mutex_lock(&pmem_mm->arch.lock_map);

    pmem_region_remap_arm(region, region->vstart, 0);
    nkops.nk_mem_unmap((void*)region->vstart, region->pstart, region->psize);

    mutex_unlock(&pmem_mm->arch.lock_map);

    region->vstart = 0;
    region->mapped = 0;
}


static pmem_arch_ops_t pmem_arch_ops = {
    .pmem_mm_arch_init    = pmem_mm_init_arm,
    .pmem_mm_arch_cleanup = pmem_mm_cleanup_arm,
    .region_arch_init     = pmem_region_init_arm,
    .region_arch_cleanup  = pmem_region_cleanup_arm,
    .vma_map              = pmem_vma_map_arm,
    .vma_unmap            = pmem_vma_unmap_arm,
};

#ifdef CONFIG_NKERNEL

#include <asm/nkern.h>
#include <nk/bconf.h>

extern NkMapDesc nk_maps[];
extern int       nk_maps_max;

    /*
     *
     */
    static noinline int
pmem_phys_range_get (NkPhAddr* pa, NkPhSize* sz, void** iter)
{
    NkMapDesc*   map;
    unsigned int i;

    for (i = (unsigned int) *iter; i < nk_maps_max; i++) {
	map = &nk_maps[i];
	if ((map->mem_type  == NK_MD_RAM) &&
	    (map->mem_owner == BANK_OS_ID(BANK_OS_SHARED))) {
	    *iter = (void*)(i + 1);
	    *pa   = map->pstart & SECTION_MASK;
	    *sz   = ((map->plimit + SECTION_SIZE - 1) & SECTION_MASK) - *pa;
	    return 0;
	}
    }

    *iter = NULL;
    return 1;
}

#endif

    /*
     *
     */
    static noinline void
pmem_mm_fixup (struct mm_struct* mm)
{
    void*         iter = NULL;
    NkPhAddr      pa;
    NkPhSize      sz;
    unsigned long va;
    unsigned long vend;
    unsigned long vnext;
    pgd_t*        pgd;
    pmd_t*        pmdp;
    pmd_t         pmd;

    if (!tlb_use_asid) {
	return;
    }

    while (pmem_phys_range_get(&pa, &sz, &iter) == 0) {

	va = (unsigned long) nkops.nk_mem_map(pa, sz);
	BUG_ON(!va);

	vend = va + sz;
	pgd  = pgd_offset(mm, va);

	while (va < vend) {
	    vnext = pgd_addr_end(va, vend);

	    pmdp = pmd_offset(pgd, va);
	
	    if (va & SECTION_SIZE) {
		pmdp++;
	    }

	    do {
		spin_lock(&mm->page_table_lock);

		pmd = *pmdp;
		BUG_ON(!pmd_type_section(pmd));
		*pmdp = pmd | PMD_SECT_nG;
		flush_pmd_entry(pmdp);

		spin_unlock(&mm->page_table_lock);

		pmdp++;
		va = (va & SECTION_MASK) + SECTION_SIZE;
	    } while (va < vnext);

	    pgd++;
	    va = vnext;
	} 

	nkops.nk_mem_unmap((void*)va, pa, sz);
    }
}

    /*
     *
     */
    static int __init
pmem_user_init_arm (void)
{
    struct task_struct*    p;
    struct mm_struct*      mm;
    const struct mem_type* mtype;

    BUG_ON(cpu_is_xsc3()); /* Not tested */

    if (cpu_architecture() < CPU_ARCH_ARMv6) {
	if (cpu_is_xscale()) {
	    cpu_type = CPU_XSCALE;
	} else {
	    cpu_type = CPU_VMSAv4;
	}
	tlb_use_asid = 0;
    } else {
	if (get_cr() & CR_XP) {
	    cpu_type = CPU_VMSAv6_XP;
	} else {
	    cpu_type = CPU_VMSAv6;
	}
	tlb_use_asid = 1;
    }

    mtype = get_mem_type(MT_MEMORY);
    BUG_ON(!mtype);
    prot_sect_k = mtype->prot_sect | (tlb_use_asid ? PMD_SECT_nG : 0);

    pmem_mm_fixup(&init_mm);

    read_lock(&tasklist_lock);
    
    for_each_process(p) {

	mm = get_task_mm(p);

	if (!mm) {
	    continue;
	}

	down_write(&mm->mmap_sem);
	pmem_mm_fixup(mm);
	up_write(&mm->mmap_sem);

	mmput(mm);

    }

    read_unlock(&tasklist_lock);

    flush_tlb_all();

    return 0;
}

core_initcall(pmem_user_init_arm);

#elif defined(PMEM_USER_GENERIC)

    /*
     *
     */
    static int
pmem_vma_map_gen (struct vm_area_struct* vma, unsigned int map_type)
{
    pmem_region_t* region = vma->vm_private_data;
    int            diag;

    BUG_ON((!region) ||
	   (!PMEM_VMA_ATTACHED(vma, region)) ||
	   (PMEM_REGION_MAPPED(region)));

    if (map_type == PMEM_MAP_AT_INIT) {
	diag = remap_pfn_range(vma,
			       vma->vm_start,
			       region->pstart >> PAGE_SHIFT,
			       region->psize,
			       vma->vm_page_prot);
	if (diag) {
	    return diag;
	}
    } else {
	BUG_ON(map_type != PMEM_MAP_AT_FORK);
	/* The vma mapping is setup by the generic MM layer */
    }

    region->vstart = vma->vm_start;
    region->mapped = 1;

    return 0;
}

    /*
     *
     */
    static void
pmem_vma_unmap_gen (struct vm_area_struct* vma, unsigned int unmap_type)
{
    pmem_region_t* region = vma->vm_private_data;

    BUG_ON(!region);

    if (unmap_type == PMEM_UNMAP_AT_ERROR) {
	zap_page_range(vma, vma->vm_start, vma->vm_end - vma->vm_start, NULL);
    } else {
	BUG_ON(unmap_type != PMEM_UNMAP_AT_EXIT);
	/* The vma mapping is removed by the generic MM layer */
    }

    if (!PMEM_VMA_ATTACHED(vma, region)) {
	return;
    }

    region->vstart = 0;
    region->mapped = 0;
}

static pmem_arch_ops_t pmem_arch_ops = {
    .pmem_mm_arch_init    = NULL,
    .pmem_mm_arch_cleanup = NULL,
    .region_arch_init     = NULL,
    .region_arch_cleanup  = NULL,
    .vma_map              = pmem_vma_map_gen,
    .vma_unmap            = pmem_vma_unmap_gen,
};

#endif /* PMEM_USER_GENERIC */

    /*
     *
     */
static struct list_head pmem_mms = LIST_HEAD_INIT(pmem_mms);
static DEFINE_MUTEX(pmem_lock);

    /*
     *
     */
    static inline void
pmem_mm_cleanup (pmem_mm_t* pmem_mm)
{
    if (pmem_mm->ops->pmem_mm_arch_cleanup) {
	pmem_mm->ops->pmem_mm_arch_cleanup(pmem_mm);
    }

    kfree(pmem_mm);
}

    /*
     *
     */
    static noinline pmem_mm_t*
pmem_mm_setup (struct mm_struct* mm)
{
    pmem_mm_t* pmem_mm;
    int        diag;

    pmem_mm = kmalloc(sizeof(pmem_mm_t), GFP_KERNEL);
    if (!pmem_mm) {
	return ERR_PTR(-ENOMEM);
    }

    pmem_mm->mm  = mm;
    pmem_mm->ops = &pmem_arch_ops;
    INIT_LIST_HEAD(&pmem_mm->regions);
    atomic_set(&pmem_mm->refcnt, 1);
    mutex_init(&pmem_mm->lock);

    if ((pmem_mm->ops->pmem_mm_arch_init) &&
	((diag = pmem_mm->ops->pmem_mm_arch_init(pmem_mm)) != 0)) {
	kfree(pmem_mm);
	return ERR_PTR(diag);
    }

    return pmem_mm;
}

    /*
     *
     */
    static inline void
pmem_mm_incref (pmem_mm_t* pmem_mm)
{
    atomic_inc(&pmem_mm->refcnt);
}

    /*
     *
     */
    static noinline void
pmem_mm_put (pmem_mm_t* pmem_mm)
{
    mutex_lock(&pmem_lock);

    if (atomic_dec_and_test(&pmem_mm->refcnt)) {
	list_del(&pmem_mm->link);
	pmem_mm_cleanup(pmem_mm);
    }

    mutex_unlock(&pmem_lock);
}

    /*
     *
     */
    static noinline pmem_mm_t*
pmem_mm_get (struct mm_struct* mm)
{
    pmem_mm_t* pmem_mm;
 
    mutex_lock(&pmem_lock);

    list_for_each_entry(pmem_mm, &pmem_mms, link) {
	if (pmem_mm->mm == mm) {
	    pmem_mm_incref(pmem_mm);
	    mutex_unlock(&pmem_lock);
	    return pmem_mm;
	}
    }

    pmem_mm = pmem_mm_setup(mm);
    if (!IS_ERR(pmem_mm)) {
	list_add(&pmem_mm->link, &pmem_mms);
    }

    mutex_unlock(&pmem_lock);

    return pmem_mm;
}

    /*
     *
     */
    static noinline void
pmem_region_delete (pmem_region_t* region)
{
    pmem_mm_t*             pmem_mm = region->pmem_mm;
    struct vm_area_struct* vma     = region->vma;

    mutex_lock(&pmem_mm->lock);

    list_del(&region->link);
    vma->vm_private_data = 0;

    if (pmem_mm->ops->region_arch_cleanup) {
	pmem_mm->ops->region_arch_cleanup(region);
    }

    mutex_unlock(&pmem_mm->lock);

    kfree(region);
}

    /*
     *
     */
    static noinline int
pmem_region_create (pmem_mm_t* pmem_mm, struct vm_area_struct* vma,
		    NkPhAddr addr)
{
    pmem_region_t* region;
    pmem_region_t* iter;
    int            diag;

    mutex_lock(&pmem_mm->lock);

    list_for_each_entry(iter, &pmem_mm->regions, link) {
	if (addr >= iter->pstart && addr < iter->pstart + iter->psize) {
	    mutex_unlock(&pmem_mm->lock);
	    return -EINVAL;
	}
    }

    region = kmalloc(sizeof(pmem_region_t), GFP_KERNEL);
    if (!region) {
	mutex_unlock(&pmem_mm->lock);
	return -ENOMEM;
    }

    region->pmem_mm = pmem_mm;
    region->pstart  = addr;
    region->psize   = vma->vm_end - vma->vm_start;
    region->vstart  = 0;
    region->mapped  = 0;
    region->vma     = vma;

    if ((pmem_mm->ops->region_arch_init) &&
	((diag = pmem_mm->ops->region_arch_init(region)) != 0)) {
	mutex_unlock(&pmem_mm->lock);
	kfree(region);
	return diag;
    }

    vma->vm_private_data = region;

    list_add(&region->link, &pmem_mm->regions);

    mutex_unlock(&pmem_mm->lock);

    return 0;
}

    /*
     *
     */
    static noinline int
pmem_vma_attach (struct vm_area_struct* vma, pmem_mm_t* pmem_mm,
		 NkPhAddr pstart, unsigned int map_type)
{
    pmem_region_t* oregion;
    int            diag;

    oregion = vma->vm_private_data;

    BUG_ON(oregion && PMEM_VMA_ATTACHED(vma, oregion));

    diag = pmem_region_create(pmem_mm, vma, pstart);
    if (diag) {
	return diag;
    }

    diag = pmem_mm->ops->vma_map(vma, map_type);
    if (diag) {
	pmem_region_delete(vma->vm_private_data);
	vma->vm_private_data = oregion;
	return diag;
    }

    return 0;
}

    /*
     *
     */
    static noinline void
pmem_vma_detach (struct vm_area_struct* vma, unsigned int unmap_type)
{
    pmem_region_t* region;
    pmem_mm_t*     pmem_mm;

    region = vma->vm_private_data;

    BUG_ON((!region) || (!PMEM_VMA_ATTACHED(vma, region)));

    pmem_mm = region->pmem_mm;

    pmem_mm->ops->vma_unmap(vma, unmap_type);

    pmem_region_delete(region);

    pmem_mm_put(pmem_mm);
}

    /*
     *
     */
    static noinline void
pmem_vma_discard (struct vm_area_struct* vma, pmem_mm_t* pmem_mm,
		  unsigned int unmap_type)
{
    pmem_region_t* region;

    region = vma->vm_private_data;

    BUG_ON((!region) || (PMEM_VMA_ATTACHED(vma, region)));

    region->pmem_mm->ops->vma_unmap(vma, unmap_type);

    vma->vm_private_data = NULL;

    if (pmem_mm) {
	pmem_mm_put(pmem_mm);
    }
}

    /*
     *
     */
    static void
pmem_vm_open (struct vm_area_struct* vma)
{
    struct mm_struct* mm;
    pmem_mm_t*        pmem_mm;
    pmem_region_t*    region;

    region = vma->vm_private_data;

    if (!region) {
	return;
    }

    BUG_ON(PMEM_VMA_ATTACHED(vma, region));

    mm      = vma->vm_mm;
    pmem_mm = pmem_mm_get(mm);

    if (IS_ERR(pmem_mm)) {
	BUG_ON(region->pmem_mm->mm == mm);
	pmem_vma_discard(vma, NULL, PMEM_UNMAP_AT_ERROR);
	return;
    }

    if (pmem_mm != region->pmem_mm) {
	/* Fork case */
	if (pmem_vma_attach(vma, pmem_mm, region->pstart, PMEM_MAP_AT_FORK)) {
	    pmem_vma_discard(vma, pmem_mm, PMEM_UNMAP_AT_ERROR);
	    return;
	}
    } else {
	/* Munmap or mremap case */
	pmem_vma_discard(vma, pmem_mm, PMEM_UNMAP_AT_ERROR);
	pmem_vma_detach(region->vma, PMEM_UNMAP_AT_ERROR);
    }
}

    /*
     *
     */
    static void
pmem_vm_close (struct vm_area_struct* vma)
{
    if (!vma->vm_private_data) {
	return;
    }

    pmem_vma_detach(vma, PMEM_UNMAP_AT_EXIT);
}

    /*
     *
     */
    static int
pmem_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    return VM_FAULT_SIGBUS;
}

    /*
     *
     */
static struct vm_operations_struct pmem_vm_ops = {
    .open  = pmem_vm_open,
    .close = pmem_vm_close,
    .fault = pmem_vm_fault,
};

    /*
     *
     */
    void*
nk_pmem_map_user (struct vm_area_struct *vma, NkPhAddr addr)
{
    struct mm_struct* mm = vma->vm_mm;
    pmem_mm_t*        pmem_mm;
    int               diag;

    if (vma->vm_pgoff               ||
	!(vma->vm_flags & VM_READ)  ||
	!(vma->vm_flags & VM_WRITE) ||
	!(vma->vm_flags & VM_SHARED)) {
	return ERR_PTR(-EINVAL);
    }

    pmem_mm = pmem_mm_get(mm);
    if (IS_ERR(pmem_mm)) {
	return pmem_mm;
    }

    vma->vm_flags |= (VM_IO | VM_PFNMAP | VM_DONTEXPAND);
    vma->vm_ops    = &pmem_vm_ops;

    diag = pmem_vma_attach(vma, pmem_mm, addr, PMEM_MAP_AT_INIT);
    if (diag) {
	pmem_mm_put(pmem_mm);
	return ERR_PTR(diag);
    }

    return (void*) ((pmem_region_t*) vma->vm_private_data)->vstart;
}

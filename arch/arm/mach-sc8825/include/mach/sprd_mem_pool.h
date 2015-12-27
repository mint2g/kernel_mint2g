#ifndef __SPRD_MEM_POOL_H
#define __SPRD_MEM_POOL_H

extern unsigned long sprd_alloc_pgd(void);
extern void sprd_free_pgd(unsigned long addr);
extern unsigned long sprd_alloc_thread_info(void);
extern void sprd_free_thread_info(unsigned long addr);

#define __HAVE_ARCH_THREAD_INFO_ALLOCATOR
#define alloc_thread_info_node(tsk, node) sprd_alloc_thread_info()
#define free_thread_info(addr) sprd_free_thread_info((unsigned long)(addr))

#endif

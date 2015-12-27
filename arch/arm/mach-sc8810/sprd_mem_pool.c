/* linux/arch/arm/mach-sc8810/sprd_mem_pool.c
 *
 * Copyright (C) 2010 Spreadtrum
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/genalloc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>

#define THREAD_ENTRY_ORDER	THREAD_SIZE_ORDER	/* 8KB */
#define PGD_ENTRY_ORDER		2			/* 16KB */

struct sprd_mem_pool {
	struct gen_pool *pool;
	unsigned long base_addr;
	unsigned long pool_order;
	unsigned long pool_size;
	unsigned long entry_order;
	unsigned long entry_size;
	unsigned long omit;

	/* only for statistics */
	unsigned long alloc_sum;
	unsigned long free_sum;
	unsigned long alloc_from_pool_num;
	unsigned long free_from_pool_num;
};

static struct sprd_mem_pool thread_pool = {
	.entry_order = THREAD_ENTRY_ORDER,
	.entry_size = PAGE_SIZE << THREAD_ENTRY_ORDER,
	.omit = 1000
};

static struct sprd_mem_pool pgd_pool = {
	.entry_order = PGD_ENTRY_ORDER,
	.entry_size = PAGE_SIZE << PGD_ENTRY_ORDER,
	.omit = 500
};

static unsigned long sprd_mem_pool_alloc(struct sprd_mem_pool *pool_info)
{
#ifdef CONFIG_DEBUG_STACK_USAGE
	gfp_t mask = GFP_KERNEL | __GFP_ZERO;
#else
	gfp_t mask = GFP_KERNEL;
#endif
	unsigned long buffer = 0;

	/* omit some alloc request here, then we can reduce the pool size */
	if(pool_info->alloc_sum++ < pool_info->omit)
		return __get_free_pages(mask, pool_info->entry_order);

	/* alloc from pool */
	buffer = gen_pool_alloc(pool_info->pool, pool_info->entry_size);

	/* if alloc fail from pool, then try in normal method */
	if(!buffer) {
		return __get_free_pages(mask, pool_info->entry_order);
	}

	pool_info->alloc_from_pool_num++;
	return buffer;
}

static void sprd_mem_pool_free(struct sprd_mem_pool *pool_info, unsigned long addr)
{
	pool_info->free_sum++;

	/* FIXME: free thread info in different zone according to its address */ 
	if(addr < pool_info->base_addr + pool_info->pool_size && addr >= pool_info->base_addr) {
		pool_info->free_from_pool_num++;
		gen_pool_free(pool_info->pool, addr, pool_info->entry_size);
	} else
		free_pages(addr, pool_info->entry_order);
	return;
}

unsigned long sprd_alloc_thread_info()
{
	return sprd_mem_pool_alloc(&thread_pool);
}

unsigned long sprd_alloc_pgd()
{
	return sprd_mem_pool_alloc(&pgd_pool);
}

void sprd_free_thread_info(unsigned long addr)
{
	sprd_mem_pool_free(&thread_pool, addr);
}

void sprd_free_pgd(unsigned long addr)
{
	sprd_mem_pool_free(&pgd_pool, addr);
}

EXPORT_SYMBOL_GPL(sprd_alloc_thread_info);
EXPORT_SYMBOL_GPL(sprd_free_thread_info);
EXPORT_SYMBOL_GPL(sprd_alloc_pgd);
EXPORT_SYMBOL_GPL(sprd_free_pgd);

static int sprd_thread_info_pool_show(struct seq_file *m, void *v)
{
	seq_printf(m,
		"thread_info pool:\n"
		"    entry size / total entry: %lu / %lu\n"
		"    total alloc / total free: %lu / %lu\n"
		"    pool alloc / pool free: %lu / %lu\n"
		"pgd pool:\n"
		"    entry size / total entry: %lu / %lu\n"
		"    total alloc / total free: %lu / %lu\n"
		"    pool alloc / pool free: %lu / %lu\n",
		thread_pool.entry_size, thread_pool.pool_size / thread_pool.entry_size,
		thread_pool.alloc_sum, thread_pool.free_sum,
		thread_pool.alloc_from_pool_num, thread_pool.free_from_pool_num,
		pgd_pool.entry_size, pgd_pool.pool_size / pgd_pool.entry_size,
		pgd_pool.alloc_sum, pgd_pool.free_sum,
		pgd_pool.alloc_from_pool_num, pgd_pool.free_from_pool_num);
	return 0;
}

static int sprd_thread_info_pool_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_thread_info_pool_show, NULL);
}

static const struct file_operations sprd_thread_info_pool_fops = {
	.open		= sprd_thread_info_pool_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int init_mem_pool(struct sprd_mem_pool *pool_info)
{
#ifdef CONFIG_DEBUG_STACK_USAGE
	gfp_t mask = GFP_KERNEL | __GFP_ZERO;
#else
	gfp_t mask = GFP_KERNEL;
#endif

	pool_info->base_addr = __get_free_pages(mask, pool_info->pool_order);
	if(!pool_info->base_addr) {
		printk("Alloc sprd mem pool failed, no memory!!\n");
		return -1;
	}

	pool_info->pool = gen_pool_create(pool_info->entry_order, -1);
	if(!pool_info->pool) {
		printk("sprd create gen pool failed!!\n");
		free_pages(pool_info->base_addr, pool_info->pool_order);
		return -1;
	}

	if(gen_pool_add(pool_info->pool, pool_info->base_addr, pool_info->pool_size, -1) < 0) {
		printk("sprd create gen pool failed!!\n");
		free_pages(pool_info->base_addr, pool_info->pool_order);
		gen_pool_destroy(pool_info->pool);
		return -1;
	}
	return 0;
}

static int __init sprd_mem_pool_init(void)
{
	/* calculate memory pool size by total memory size, every 256MB total mem has 2.5MB pool */
	thread_pool.pool_order = get_order(PAGE_ALIGN(totalram_pages * PAGE_SIZE / 100)) - 1;
	thread_pool.pool_size = PAGE_SIZE << thread_pool.pool_order;
	printk("threadinfo memory pool initialized, order: %lu, size: %lu(KB)\n",
		thread_pool.pool_order, thread_pool.pool_size / 1024);

	pgd_pool.pool_order = thread_pool.pool_order - 2;
	pgd_pool.pool_size = PAGE_SIZE << pgd_pool.pool_order;
	printk("pgd table memory pool initialized, order: %lu, size: %lu(KB)\n",
		pgd_pool.pool_order, pgd_pool.pool_size / 1024);

	init_mem_pool(&thread_pool);
	init_mem_pool(&pgd_pool);
	proc_create("sprd_mem_pool", 0, NULL, &sprd_thread_info_pool_fops);
	return 0;
}

module_init(sprd_mem_pool_init);


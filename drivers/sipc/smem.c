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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/genalloc.h>
#include <linux/mm.h>

#include <linux/sipc.h>

struct smem_pool {
	uint32_t		addr;
	uint32_t		size;

	struct gen_pool		*gen;
};

static struct smem_pool		mem_pool;

int smem_init(uint32_t addr, uint32_t size)
{
	struct smem_pool *spool = &mem_pool;

	spool->addr = addr;
	spool->size = PAGE_ALIGN(size);

	/* allocator block size is times of pages */
	spool->gen = gen_pool_create(PAGE_SHIFT, -1);
	if (!spool->gen) {
		printk(KERN_ERR "Failed to create smem gen pool!\n");
		return -1;
	}

	if (gen_pool_add(spool->gen, spool->addr, spool->size, -1) != 0) {
		printk(KERN_ERR "Failed to add smem gen pool!\n");
		return -1;
	}

	return 0;
}

/* ****************************************************************** */

uint32_t smem_alloc(uint32_t size)
{
	struct smem_pool *spool = &mem_pool;

	size = PAGE_ALIGN(size);
	return gen_pool_alloc(spool->gen, size);
}

void smem_free(uint32_t addr, uint32_t size)
{
	struct smem_pool *spool = &mem_pool;

	size = PAGE_ALIGN(size);
	gen_pool_free(spool->gen, addr, size);
}

EXPORT_SYMBOL(smem_alloc);
EXPORT_SYMBOL(smem_free);

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SMEM driver");
MODULE_LICENSE("GPL");

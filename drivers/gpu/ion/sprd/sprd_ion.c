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

#define pr_fmt(fmt) "sprd_ion: " fmt

#include <linux/err.h>
#include <linux/ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "ion_sprd.h"
#include "../ion_priv.h"

#include <asm/cacheflush.h>
 
#define SPRD_ION_DEBUG  (1)
#if SPRD_ION_DEBUG
#define slog_err(x ...) pr_err(x) 
#define slog_info(x ...) pr_info(x)
#else
#define slog_err(x ...) (void)0
#define slog_info(x ...) (void)0
#endif



#define SPRD_ION_DEBUG  (1)
#if SPRD_ION_DEBUG
#define slog_err(x ...) pr_err(x)
#define slog_info(x ...) pr_info(x)
#else
#define slog_err(x ...) (void)0
#define slog_info(x ...) (void)0
#endif



struct ion_device *idev;
int num_heaps;
struct ion_heap **heaps;


static long sprd_heap_ioctl(struct ion_client *client, unsigned int cmd,
				unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case ION_SPRD_CUSTOM_PHYS:
	{
		struct ion_phys_data data;
		struct ion_handle *handle;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
slog_err(" failed to copy data from userspace");
			return -EFAULT;
		}

		handle = ion_import_fd(client, data.fd_buffer);

		if (IS_ERR(handle)) {
slog_err("failed to import ion handle");
			return PTR_ERR(handle);
}

		ret = ion_phys(client, handle, &data.phys, &data.size);
		if (ret) {
slog_err("failed to get phys addr from ion");
			return ret;
}
		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {

slog_err("failed to copy data from user");
			return -EFAULT;
		}
		ion_free(client, handle);

		break;
	}
	case ION_SPRD_CUSTOM_MSYNC:
	{
		struct ion_msync_data data;
		void *flush_start, *flush_end;
		void *kaddr;
		void *paddr;
		size_t size;
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
slog_err("failed to copy data from user");
			return -EFAULT;
		}
		flush_start = data.vaddr;
		flush_end = data.vaddr + data.size;
        kaddr = data.vaddr;
paddr =  data.paddr;	
		size = data.size;
		//slog_info("ion flush_start %x, %x,size %x",data.vaddr,data.paddr,data.size);
#if 0		
		dmac_flush_range(flush_start, flush_end);
#else	
		{
			//BUG_ON(!virt_addr_valid(kaddr) || !virt_addr_valid(kaddr + size - 1));

			dmac_flush_range(kaddr, kaddr + size);
 	
			outer_clean_range(
		*(phys_addr_t*) paddr, 
		*( phys_addr_t*) paddr + size);

			/* FIXME: non-speculating: flush on bidirectional mappings? */
		}
#endif
		break;
	}
	default:
		return -ENOTTY;
	}

	return ret;
}

int sprd_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	int err;
	int i;

	num_heaps = pdata->nr;

	heaps = kzalloc(sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);

	idev = ion_device_create(&sprd_heap_ioctl);
	if (IS_ERR_OR_NULL(idev)) {
		kfree(heaps);
slog_err("failed to create ion device");
		return PTR_ERR(idev);
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];
        
		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			err = PTR_ERR(heaps[i]);
			goto err;
		}
		ion_device_add_heap(idev, heaps[i]);
      slog_info("created heap id=%d, base=%lu, name=%s, size=%u", heap_data->id, heap_data->base, heap_data->name, heap_data->size);
	}
	platform_set_drvdata(pdev, idev);
	return 0;
err:
slog_err("failed to create ion heap");
	for (i = 0; i < num_heaps; i++) {
		if (heaps[i])
			ion_heap_destroy(heaps[i]);
	}
	kfree(heaps);
	return err;
}

int sprd_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	ion_device_destroy(idev);
	for (i = 0; i < num_heaps; i++)
		ion_heap_destroy(heaps[i]);
	kfree(heaps);
	return 0;
}

static struct platform_driver ion_driver = {
	.probe = sprd_ion_probe,
	.remove = sprd_ion_remove,
	.driver = { .name = "ion-sprd" }
};

static int __init ion_init(void)
{
	return platform_driver_register(&ion_driver);
}

static void __exit ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
}

module_init(ion_init);
module_exit(ion_exit);


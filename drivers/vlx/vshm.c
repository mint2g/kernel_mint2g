/*
 ****************************************************************
 *
 *  Component: VLX virtual shared memory driver
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
 ****************************************************************
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <asm/io.h>
#include <asm/page.h>

#define VSHM_MAJOR 240
#define VSHM_MINOR 0
#define VSHM_NAME  "vshm"
#define VSHM_INFO  1

#define VSHM_LEN   (820*1024*16) // 16x 820KB shared data buffers

#include <nk/nkern.h>
#include <nk/nkdev.h>
#include <nk/nk.h>

extern NkDevOps nkops;

NkPhAddr vshm_pmem = 0;

static struct class* vshm_class = NULL;


typedef struct vshm_info_t {
    unsigned long version;
    unsigned long pmem;
    unsigned long len;
} vshm_info_t;


static char* vshm_page = NULL;
static char* vshm_base = NULL;
static int   vshm_len  = VSHM_LEN;

module_param(vshm_len, int, 0444);


static int
vshm_init (void)
{
    NkPhAddr    plink;
    NkDevVlink* vlink;
    //NkOsId      my_id = nkops.nk_id_get();
    int         len_option = 0;

    plink    = 0;
    while ((plink = nkops.nk_vlink_lookup(VSHM_NAME, plink))) {
	vlink = nkops.nk_ptov(plink);

	//if (vlink->c_id == my_id) {
	if (vlink->s_info) {
	    char* vname;
	    char* end;

	    vname = nkops.nk_ptov(vlink->s_info);
	    len_option = (int)simple_strtoul(vname, &end, 0);
	    if (len_option) {
		if (end < (vname + strlen(vname))) {
		    if (!strcasecmp(end, "k"))
			len_option *= 1024;
		    else if (!strcasecmp(end, "m"))
			len_option *= 1024*1024;
		    else {
			len_option = 0;
			printk(KERN_ERR "VSHM: vlink #%d info: "
			       "invalid extra field <%s>\n", vlink->link, vname);
		    }
		}
	    }else {
		printk(KERN_ERR "VSHM: vlink #%d info: "
		       "invalid extra field <%s>\n", vlink->link, vname);
	    }
	}
	break; // for now, both sides use the same, unique "vshm" vlink
	//}
    }

    if (!plink) {
	return -ENODEV;
    }

    vlink = nkops.nk_ptov(plink);

    if (len_option)
	    vshm_len = len_option;
    vshm_pmem = nkops.nk_pmem_alloc(plink, 0, vshm_len);
    if (!vshm_pmem) {
	printk(KERN_CRIT "VSHM: nk_pmem_alloc(%d) failed for link %d\n",
	       vshm_len, vlink->link);
	return -ENOMEM;
    }

    vshm_base = (char*)nkops.nk_mem_map(vshm_pmem, vshm_len);
    vshm_page = vshm_base; // we're already page-aligned.

    return 0;
}


static void
vshm_exit (void)
{
    nkops.nk_mem_unmap(vshm_base, vshm_pmem, vshm_len);
}


static int
vshm_open (struct inode* inode, struct file* file)
{
     return 0;
}


static int
vshm_release (struct inode* inode, struct file* file)
{
    return 0;
}


static long
vshm_ioctl (struct file* file, unsigned int cmd, unsigned long arg)
{
    int err = 0;

    vshm_info_t info = { 1, (unsigned long)vshm_pmem, vshm_len };

    switch (cmd) {
    case VSHM_INFO:
	if (copy_to_user((void*)arg, &info, sizeof(info)))
	    return -EFAULT;
	break;
    default:
	err = -EINVAL;
    }

    return err;
}


static int
vshm_mmap (struct file* filp, struct vm_area_struct* vma)
{
    int err = 0;

    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

    vma->vm_flags |= VM_IO;

    err = io_remap_pfn_range(vma,
	                     vma->vm_start,
	                     virt_to_phys((void*)((unsigned long)vshm_page)) >> PAGE_SHIFT,
	                     vma->vm_end - vma->vm_start,
                             vma->vm_page_prot);

    return err ? -EAGAIN : 0;
}


static struct file_operations vshm_fops = {
    .owner   = THIS_MODULE,
    .llseek  = NULL,
    .read    = NULL,
    .write   = NULL,
    .readdir = NULL,
    .poll    = NULL,
    .unlocked_ioctl = vshm_ioctl,
    .mmap    = vshm_mmap,
    .open    = vshm_open,
    .flush   = NULL,
    .release = vshm_release,
    .fsync   = NULL,
    .fasync  = NULL,
    .lock    = NULL
};


static int __init
vshm_module_init (void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    struct device* cls_dev;
#else
    struct class_device* cls_dev;
#endif
    int            err;

    err = vshm_init();
    if (err != 0) {
	return -EIO;
    }

    vshm_class = class_create(THIS_MODULE, VSHM_NAME);
    if (IS_ERR(vshm_class)) {
	return -EIO;
    }

#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    cls_dev = device_create
#else
    cls_dev = class_device_create
#endif
	(vshm_class, NULL, MKDEV(VSHM_MAJOR, VSHM_MINOR), NULL, VSHM_NAME);
    if (IS_ERR(cls_dev)) {
	class_destroy(vshm_class);
	return -EIO;
    }

    err = register_chrdev(VSHM_MAJOR, VSHM_NAME, &vshm_fops);
    if (err != 0) {
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
	device_destroy
#else
	class_device_destroy
#endif
	    (vshm_class, MKDEV(VSHM_MAJOR, VSHM_MINOR));
	class_destroy(vshm_class);
	return -EIO;
    }

    printk(KERN_ERR "VSHM: vshm_base %p, vshm_page %p, vshm_len %d\n",
	   vshm_base, vshm_page, vshm_len);

    memset(vshm_page, 0x00, vshm_len);
	
    return 0;
}


static void __exit
vshm_module_exit (void)
{
    unregister_chrdev(VSHM_MAJOR, VSHM_NAME);
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,23)
    device_destroy
#else
    class_device_destroy
#endif
	(vshm_class, MKDEV(VSHM_MAJOR, VSHM_MINOR));
    class_destroy(vshm_class);

    vshm_exit();
}


module_init(vshm_module_init);
module_exit(vshm_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("VLX shared memory");

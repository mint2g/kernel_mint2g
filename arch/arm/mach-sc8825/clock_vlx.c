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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>

/* which level the clock belongs to, AHB or APB. */
#define	DEVICE_AHB		(0x1UL << 20)
#define	DEVICE_APB		(0x1UL << 21)
#define	DEVICE_VIR		(0x1UL << 22)
#define DEVICE_AWAKE		(0x1UL << 23)
#define DEVICE_TEYP_MASK	(DEVICE_AHB | DEVICE_APB | DEVICE_VIR | DEVICE_AWAKE)

#define	CLOCK_NUM		128
#define	MAX_CLOCK_NAME_LEN	16

struct clock_stub {
	unsigned char *name;
	unsigned int  flags;
	int usecount;
};

struct clock_stub *pstub_start;
char (*pname_start)[MAX_CLOCK_NAME_LEN];

struct clock_stub *pstub;
char (*pname)[MAX_CLOCK_NAME_LEN];

#define	RES_CLOCK_STUB_MEM	0
#define	RES_CLOCK_NAME_MEM	1

#include <nk/nkern.h>
const char vlink_name_clk_fw[] = "vclock_framework";
NkPhAddr    plink_clk_fw;
NkDevVlink* vlink_clk_fw;

static int clk_fw_vlink_init(void)
{
	plink_clk_fw = 0;
	while ((plink_clk_fw = nkops.nk_vlink_lookup(vlink_name_clk_fw, plink_clk_fw))) {
		if (0 == plink_clk_fw) {
			pr_err("#####: Can't find the vlink [%s]!\n", vlink_name_clk_fw);
			return -ENOMEM;
		}
		vlink_clk_fw = nkops.nk_ptov(plink_clk_fw);
		pr_info("#####: clock-framework: vlink info: s_id = %d, c_id = %d.\n",
				vlink_clk_fw->s_id, vlink_clk_fw->c_id);
	}
	return 0;
}

void *alloc_share_memory(unsigned int size, unsigned int res_id)
{
	void *pmem = NULL;

	NkPhAddr     paddr;

	/* Allocate persistent shared memory */
	paddr  = nkops.nk_pmem_alloc(nkops.nk_vtop(vlink_clk_fw), res_id, size);

	if (paddr == 0) {
		pr_err("OS#%d->OS#%d link=%d server pmem alloc failed.\n",
				vlink_clk_fw->c_id, vlink_clk_fw->s_id, vlink_clk_fw->link);
		return NULL;
	}

	pmem = (void *) nkops.nk_mem_map(paddr, size);
	if (pmem == 0) {
		pr_err("error while mapping\n");
	}
	return pmem;

}

int __init clock_vlx_init(void)
{
	/* modem clock begin*/
	int ret, index;
	ret = clk_fw_vlink_init();
	if (ret) {
		pr_err("######: clock-framework: vlink initialization failed!\n");
		return -ENOMEM;
	}

	/* allocate memory for shared clock information. */
	pstub_start= (struct clock_stub *)alloc_share_memory(CLOCK_NUM *
			sizeof(struct clock_stub), RES_CLOCK_STUB_MEM);
	if (NULL == pstub_start) {
		pr_err("Clock Framework: alloc_share_memory() failed!\n");
		return -ENOMEM;
	}

	/* allocate memory for clock name. */
	pname_start = alloc_share_memory(CLOCK_NUM * MAX_CLOCK_NAME_LEN,
			RES_CLOCK_NAME_MEM);
	if (NULL == pname_start) {
		pr_err("Clock Framework: alloc_share_memory() failed!\n");
		return -ENOMEM;
	}

	/* find first available block. */
	for (index = 0, pstub = pstub_start, pname = pname_start;
			NULL != pstub->name; pstub++, pname++, index++) {
/*		printk("PM clock: %s:%s\n", pstub->name, *pname); */
		continue;
	}
	/* modem clock end */
	return 0;
}

static int sc8825_get_clock_modem_status(void)
{
    int index = 0;
    int status = 0;

    /* check all clocks, both linux side and RTOS side. */
    for (index = 0, pstub = pstub_start; pstub[index].name != NULL; index++) {
        if (pstub[index].usecount) {
	      status |= pstub[index].flags;
#if 0
	    if (pstub[index].flags & DEVICE_AHB)
	        printk("###: modem clcok[%s] is on AHB.\n", pstub[index].name);
	    if (pstub[index].flags & DEVICE_APB)
	        printk("###: modem clcok[%s] is on APB.\n", pstub[index].name);
	    if (pstub[index].flags & DEVICE_VIR)
	        printk("###: modem clcok[%s] is on VIR.\n", pstub[index].name);
	    if (pstub[index].flags & DEVICE_AWAKE)
	        printk("###: modem clcok[%s] is on AWAKE.\n", pstub[index].name);
#endif
	}
    }
    return status;
}

int sc8825_get_clock_status(void)
{
	int status = 0;
	return status | sc8825_get_clock_modem_status();
}

arch_initcall(clock_vlx_init);


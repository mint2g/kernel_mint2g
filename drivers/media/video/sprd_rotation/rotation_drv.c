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
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <linux/file.h>
#include <mach/dma.h>
#include <linux/sched.h>
#include <video/sprd_rotation.h>
#include "rotation_reg.h"
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#define RTT_PRINT pr_debug

#define BOOLEAN char
#define ROTATION_TRUE 1
#define ROTATION_FALSE 0
#define DISABLE_AHB_SLEEP 0
#define ENABLE_AHB_SLEEP 1

typedef struct _dma_rotation_tag {
	ROTATION_SIZE_T img_size;
	ROTATION_DATA_FORMAT_E data_format;
	ROTATION_DIR_E rotation_dir;
	ROTATION_DATA_ADDR_T src_addr;
	ROTATION_DATA_ADDR_T dst_addr;
	uint32_t s_addr;
	uint32_t d_addr;
	ROTATION_PIXEL_FORMAT_E pixel_format;
	ROTATION_UV_MODE_E uv_mode;
	BOOLEAN is_end;
	int ch_id;
} ROTATION_CFG_T, *ROTATION_CFG_T_PTR;

static ROTATION_CFG_T s_rotation_cfg;

#define ALGIN_FOUR      0x03
#define DECLARE_ROTATION_PARAM_ENTRY(s) ROTATION_CFG_T *s=&s_rotation_cfg
#define ROTATION_MINOR MISC_DYNAMIC_MINOR

typedef void (*ROTATION_IRQ_FUNC) (uint32_t);
static struct mutex *lock;
static wait_queue_head_t wait_queue;
static int condition;
struct semaphore g_sem_rot;

#define init_MUTEX(sem)    sema_init(sem, 1)

static int rotation_check_param(ROTATION_PARAM_T * param_ptr)
{
	if (NULL == param_ptr) {
		RTT_PRINT("Rotation: the param ptr is null.\n");
		return -1;
	}

	if ((param_ptr->src_addr.y_addr & ALGIN_FOUR)
	    || (param_ptr->src_addr.uv_addr & ALGIN_FOUR)
	    || (param_ptr->src_addr.v_addr & ALGIN_FOUR)
	    || (param_ptr->dst_addr.y_addr & ALGIN_FOUR)
	    || (param_ptr->dst_addr.uv_addr & ALGIN_FOUR)
	    || (param_ptr->dst_addr.v_addr & ALGIN_FOUR)) {
		RTT_PRINT("Rotation: the addr not algin.\n");
		return -1;
	}

	if (ROTATION_RGB565 < param_ptr->data_format) {
		RTT_PRINT("Rotation: data for err : %d.\n",
			  param_ptr->data_format);
		return -1;
	}
	return 0;
}

static ROTATION_PIXEL_FORMAT_E rotation_get_pixel_format(void)
{
	DECLARE_ROTATION_PARAM_ENTRY(s);

	switch (s->data_format) {
	case ROTATION_YUV422:
	case ROTATION_YUV420:
	case ROTATION_YUV400:
		s->pixel_format = ROTATION_ONE_BYTE;
		break;
	case ROTATION_RGB565:
		s->pixel_format = ROTATION_TWO_BYTES;
		break;
	case ROTATION_RGB888:
	case ROTATION_RGB666:
		s->pixel_format = ROTATION_FOUR_BYTES;
		break;
	default:
		break;
	}
	return s->pixel_format;
}

static BOOLEAN rotation_get_isend(void)
{
	DECLARE_ROTATION_PARAM_ENTRY(s);

	switch (s->data_format) {
	case ROTATION_YUV422:
	case ROTATION_YUV420:
		s->is_end = ROTATION_FALSE;
		break;
	case ROTATION_YUV400:
	case ROTATION_RGB888:
	case ROTATION_RGB565:
	case ROTATION_RGB666:
		s->is_end = ROTATION_TRUE;
		break;
	default:
		break;
	}
	return s->is_end;
}

static int rotation_set_y_param(ROTATION_PARAM_T * param_ptr)
{
	DECLARE_ROTATION_PARAM_ENTRY(s);

	memcpy((void *)&(s->img_size), (void *)&(param_ptr->img_size),
	       sizeof(ROTATION_SIZE_T));
	memcpy((void *)&(s->src_addr), (void *)&(param_ptr->src_addr),
	       sizeof(ROTATION_DATA_ADDR_T));
	memcpy((void *)&(s->dst_addr), (void *)&(param_ptr->dst_addr),
	       sizeof(ROTATION_DATA_ADDR_T));

	s->s_addr = param_ptr->src_addr.y_addr;
	s->d_addr = param_ptr->dst_addr.y_addr;
	s->data_format = param_ptr->data_format;
	s->rotation_dir = param_ptr->rotation_dir;

	s->pixel_format = rotation_get_pixel_format();
	s->is_end = rotation_get_isend();
	s->uv_mode = ROTATION_NORMAL;
	return 0;
}

static void rotation_cfg(void)
{
	// rot eb
	_paod(AHB_GLOBAL_REG_CTL0, BIT(14));	//ROTATION_DRV_ONE

	// rot soft reset
	_paod(AHB_GLOBAL_REG_SOFTRST, BIT(10));
	_paad(AHB_GLOBAL_REG_SOFTRST, ~BIT(10));
}

static void rotation_disable(void)
{
	// rot eb
	_paad(AHB_GLOBAL_REG_CTL0, ~BIT(14));	//ROTATION_DRV_ONE
}

static void rotation_software_reset(void)
{
	// rot soft reset
	_paod(AHB_GLOBAL_REG_SOFTRST, BIT(10));
	_paad(AHB_GLOBAL_REG_SOFTRST, ~BIT(10));
}

static void rotation_set_src_addr(uint32_t src_addr)
{
	_pawd(REG_ROTATION_SRC_ADDR, src_addr);
	return;
}

static void rotation_set_dst_addr(uint32_t dst_addr)
{
	_pawd(REG_ROTATION_DST_ADDR, dst_addr);
	return;
}

static void rotation_set_img_size(ROTATION_SIZE_T * size)
{
	_paad(REG_ROTATION_IMG_SIZE, 0xFF000000);
	_paod(REG_ROTATION_IMG_SIZE,
	      (size->h & 0xFFF) | ((size->w & 0xFFF) << 12));
	_pawd(REG_ROTATION_ORIGWIDTH, size->w & 0xFFF);
	return;
}

static void rotation_set_pixel_mode(ROTATION_PIXEL_FORMAT_E pixel_format)
{
	_paad(REG_ROTATION_IMG_SIZE, ~(0x3 << 24));
	_paod(REG_ROTATION_IMG_SIZE, pixel_format << 24);
	return;
}

static void rotation_set_dir(ROTATION_DIR_E rotation_dir)
{
	_paad(REG_ROTATION_CTRL, ~(0x3 << 1));
	_paod(REG_ROTATION_CTRL, (rotation_dir & 0x3) << 1);
	return;
}

static void rotation_set_UV_mode(ROTATION_UV_MODE_E uv_mode)
{
	_paad(REG_ROTATION_CTRL, (~BIT(0)));
	_paod(REG_ROTATION_CTRL, (uv_mode & 0x1));
	return;
}

static void rotation_enable(void)
{
	_paod(REG_ROTATION_CTRL, BIT(3));
	return;
}

#ifdef ROTATION_DEBUG		//for debug
static void get_rotation_reg(void)
{
	uint32_t i, value;
	RTT_PRINT
	    ("###############get_rotation_reg##########################\n");
	for (i = 0; i < 12; i++) {
		value = _pard(REG_ROTATION_SRC_ADDR + i * 4);
		RTT_PRINT("ROT reg:0x%x, 0x%x.\n",
			  REG_ROTATION_SRC_ADDR + i * 4, value);
	}
	RTT_PRINT("###############get_DMA_reg##########################\n");
	for (i = 0; i < 49; i++) {
		value = _pard(SPRD_DMA_BASE + i * 4);
		RTT_PRINT("DMA reg:0x%x, 0x%x.\n", SPRD_DMA_BASE + i * 4,
			  value);
	}
	for (i = 0; i < 8; i++) {
		value = _pard(SPRD_DMA_BASE + 0x6A0 + i * 4);
		RTT_PRINT("DMA chn 21 reg:0x%x, 0x%x.\n",
			  SPRD_DMA_BASE + 0x6A0 + i * 4, value);
	}
}
#endif
static void rotation_done(void)
{
	DECLARE_ROTATION_PARAM_ENTRY(s);

	rotation_software_reset();
	rotation_set_src_addr(s->s_addr);
	rotation_set_dst_addr(s->d_addr);
	rotation_set_img_size(&(s->img_size));
	rotation_set_pixel_mode(s->pixel_format);
	rotation_set_dir(s->rotation_dir);
	rotation_set_UV_mode(s->uv_mode);
	rotation_enable();
	RTT_PRINT("ok to rotation_done.\n");
#ifdef ROTATION_DEBUG
	get_rotation_reg();
#endif
}

static int rotation_set_UV_param(void)
{
	DECLARE_ROTATION_PARAM_ENTRY(s);

	s->s_addr = s->src_addr.uv_addr;
	s->d_addr = s->dst_addr.uv_addr;
	s->img_size.w >>= 0x01;
	s->pixel_format = ROTATION_TWO_BYTES;
	if ((ROTATION_YUV422 == s->data_format)
	    && ((ROTATION_90 == s->rotation_dir)
		|| (ROTATION_270 == s->rotation_dir))) {
		s->uv_mode = ROTATION_UV422;
		s->img_size.h >>= 0x01;
	} else if (ROTATION_YUV420 == s->data_format) {
		s->img_size.h >>= 0x01;
	}
	return 0;
}

static void rotation_dma_irq(int dma_ch, void *dev_id)
{
	pr_debug("%s, come\n", __func__ );
	condition = 1;
	wake_up_interruptible(&wait_queue);
	RTT_PRINT("rotation_dma_irq X .\n");
}

int rotation_dma_start(ROTATION_PARAM_T * param_ptr)
{
	int ret = 0;
	int ch_id = -1;
	struct sprd_dma_channel_desc dma_desc;
	DECLARE_ROTATION_PARAM_ENTRY(s);
	/*printk("wjp rotation 0.\n");*/
	RTT_PRINT("rotation_dma_start E .\n");
	ch_id = sprd_dma_request(DMA_ROT, rotation_dma_irq, &dma_desc);
	if (ch_id < 0) {
		RTT_PRINT("fail to sprd_request_dma.\n");
		return -EFAULT;
	}
	s->ch_id = ch_id;
	condition = 0;
	memset(&dma_desc, 0, sizeof(struct sprd_dma_channel_desc));
	dma_desc.llist_ptr = (dma_addr_t) 0x20800420;
	dma_desc.cfg_req_mode_sel = DMA_REQMODE_LIST;
	sprd_dma_channel_config(ch_id, DMA_LINKLIST, &dma_desc);
	sprd_dma_set_irq_type(ch_id, LINKLIST_DONE, 1);
	/*printk("wjp rotation 1.\n");*/
	sprd_dma_start(ch_id);
	/*printk("wjp rotation 2.\n");*/
	RTT_PRINT("rotation_dma_start X .\n");
	return 0;
}

int rotation_dma_wait_stop(void)
{
	int ret = 0;
	DECLARE_ROTATION_PARAM_ENTRY(s);
	RTT_PRINT("rotation_dma_wait_stop E .\n");
	if (wait_event_interruptible(wait_queue, condition)) {
		ret = -EFAULT;
	}
	sprd_dma_stop(s->ch_id);
	sprd_dma_free(s->ch_id);
	RTT_PRINT("ok to rotation_dma_wait_stop.\n");
	return ret;
}

int rotation_start(ROTATION_PARAM_T * param_ptr)
{
	DECLARE_ROTATION_PARAM_ENTRY(s);
	rotation_check_param(param_ptr);
	rotation_set_y_param(param_ptr);
	rotation_cfg();
	rotation_dma_start(param_ptr);
	rotation_done();

	if (ROTATION_FALSE == s->is_end) {
		RTT_PRINT("ok to UV plane.\n");
		rotation_dma_wait_stop();
		rotation_dma_start(param_ptr);
		rotation_set_UV_param();
		rotation_done();
	}
	rotation_dma_wait_stop();
	return 0;
}

int rotation_IOinit(void)
{
	down(&g_sem_rot);
	return 0;
}

int rotation_IOdeinit(void)
{
	rotation_disable();
	up(&g_sem_rot);
	return 0;
}

int rotation_open(struct inode *node, struct file *file)
{
	ROTATION_PARAM_T *params;

	rotation_IOinit();

	params =
	    (ROTATION_PARAM_T *) kmalloc(sizeof(ROTATION_PARAM_T), GFP_KERNEL);

	if (params == NULL) {
		printk(KERN_ERR "Instance memory allocation was failed\n");
		return -1;
	}
	memset(params, 0, sizeof(ROTATION_PARAM_T));
	file->private_data = (ROTATION_PARAM_T *) params;
	RTT_PRINT("[pid:%d] sc8800g_rotation_open() called.\n", current->pid);
	return 0;
}

int rotation_release(struct inode *node, struct file *file)
{
	ROTATION_PARAM_T *params;

	params = (ROTATION_PARAM_T *) file->private_data;
	if (params == NULL) {
		printk(KERN_ERR "Can't release rotation_release !!\n");
		return -1;
	}
	kfree(params);
	RTT_PRINT("[pid:%d] rotation_release()\n", current->pid);
	rotation_IOdeinit();
	return 0;
}

static int rotation_start_copy_data(ROTATION_PARAM_T * param_ptr)
{
	struct sprd_dma_channel_desc dma_desc;
	uint32_t byte_per_pixel = 1;
	uint32_t src_img_postm = 0;
	uint32_t dst_img_postm = 0;
	uint32_t src_addr = param_ptr->src_addr.y_addr;
	uint32_t dst_addr = param_ptr->dst_addr.y_addr;
	uint32_t block_len;
	uint32_t total_len;
	int32_t ret = 0;
	int ch_id = 0;
	/*struct timeval ts;*/
	/*struct timeval te;*/
	/*printk("wjp:rotation_start_copy_data,w=%d,h=%d s!\n",param_ptr->img_size.w,param_ptr->img_size.h);*/
	if (ROTATION_YUV420 == param_ptr->data_format) {
		block_len =
		    param_ptr->img_size.w * param_ptr->img_size.h * 3 / 2;
	} else if (ROTATION_RGB888 == param_ptr->data_format) {
		block_len = param_ptr->img_size.w * param_ptr->img_size.h * 4;
	} else {
		block_len = param_ptr->img_size.w * param_ptr->img_size.h * 2;
	}
	total_len = block_len;

	//do_gettimeofday(&ts);
	//RTT_PRINT("convert endian   %d,%d,%x,%x\n", width,height,input_addr,output_addr);

	while (1) {
		ch_id =
		    sprd_dma_request(DMA_UID_SOFTWARE, rotation_dma_irq,
				     &dma_desc);
		if (ch_id < 0) {
			printk("rotation: convert endian request dma fail.ret : %d.\n", ret);
			msleep(5);
		} else {
			RTT_PRINT("rotation: convert endian request dma OK. ch_id:%d,total_len=0x%x.\n",
			     ch_id, total_len);
			break;
		}
	}
	condition = 0;
	memset(&dma_desc, 0, sizeof(struct sprd_dma_channel_desc));
	dma_desc.src_burst_mode = SRC_BURST_MODE_8;
	dma_desc.dst_burst_mode = SRC_BURST_MODE_8;
	dma_desc.cfg_src_data_width = DMA_SDATA_WIDTH32;
	dma_desc.cfg_dst_data_width = DMA_DDATA_WIDTH32;
	dma_desc.cfg_req_mode_sel = DMA_REQMODE_TRANS;
	dma_desc.total_len = total_len;
	dma_desc.cfg_blk_len = block_len;
	dma_desc.src_addr = src_addr;
	dma_desc.dst_addr = dst_addr;
	dma_desc.cfg_swt_mode_sel = 7 << 16;
	dma_desc.src_elem_postm = 0x0004;
	dma_desc.dst_elem_postm = 0x0004;
	sprd_dma_channel_config(ch_id, DMA_NORMAL, &dma_desc);
	sprd_dma_set_irq_type(ch_id, TRANSACTION_DONE, 1);
	/*printk("wjp:before rotation_start_copy_data start!\n");*/
	sprd_dma_channel_start(ch_id);
	if (wait_event_interruptible(wait_queue, condition)) {
		ret = -EFAULT;
	}
	sprd_dma_channel_stop(ch_id);
	sprd_dma_free(ch_id);
	/* do_gettimeofday(&te);*/
	/*printk("wjp:dma endian time=%d.\n",((te.tv_sec-ts.tv_sec)*1000+(te.tv_usec-ts.tv_usec)/1000));*/
	return ret;
}

static uint32_t user_va2pa(struct mm_struct *mm, uint32_t addr)
{
        pgd_t *pgd = pgd_offset(mm, addr);
        uint32_t pa = 0;

        if (!pgd_none(*pgd)) {
                pud_t *pud = pud_offset(pgd, addr);
                if (!pud_none(*pud)) {
                        pmd_t *pmd = pmd_offset(pud, addr);
                        if (!pmd_none(*pmd)) {
                                pte_t *ptep, pte;

                                ptep = pte_offset_map(pmd, addr);
                                pte = *ptep;
                                if (pte_present(pte))
                                        pa = pte_val(pte) & PAGE_MASK;
                                pte_unmap(ptep);
                        }
                }
        }

		//printk("user_va2pa: vir=%x, phy=%x \n", addr, pa);
        return pa;
}

struct sprd_dma_linklist_desc {
	u32 cfg;
	u32 total_len;
	u32 src_addr;
	u32 dst_addr;
	u32 llist_ptr;
	u32 elem_postm;
	u32 src_blk_postm;
	u32 dst_blk_postm;
};

static int rotation_start_copy_data_to_virtual(ROTATION_PARAM_T * param_ptr)
{
	struct sprd_dma_channel_desc dma_desc;
	uint32_t byte_per_pixel = 1;
	uint32_t src_img_postm = 0;
	uint32_t dst_img_postm = 0;
	uint32_t dma_src_phy = param_ptr->src_addr.y_addr;
	uint32_t dst_vir_addr = param_ptr->dst_addr.y_addr;
	uint32_t dma_dst_phy;
	uint32_t block_len;
	uint32_t total_len;
	int32_t ret = 0;
	int ch_id = 0;
	int i;
	uint32_t list_size;
	uint32_t list_copy_size = 4096;
	struct sprd_dma_linklist_desc *dma_cfg;
	dma_addr_t dma_cfg_phy;
	struct timeval time1, time2;


	/*struct timeval ts;*/
	/*struct timeval te;*/
	/*printk("wjp:rotation_start_copy_data,w=%d,h=%d s!\n",param_ptr->img_size.w,param_ptr->img_size.h);*/
	if (ROTATION_YUV420 == param_ptr->data_format) {
		block_len = param_ptr->img_size.w * param_ptr->img_size.h * 3 / 2;
	} else {
		block_len = param_ptr->img_size.w * param_ptr->img_size.h * 2;
	}

	total_len = block_len;

	//do_gettimeofday(&ts);
	//RTT_PRINT("convert endian   %d,%d,%x,%x\n", width,height,input_addr,output_addr);

	if(0 != dst_vir_addr%list_copy_size){
		printk("rotation_start_copy_data_to_virtual: dst_vir_addr = %x not 4K bytes align, error \n", dst_vir_addr);
		return -ENOMEM;
	}

	list_size = (total_len + list_copy_size -1)/list_copy_size;

	RTT_PRINT("rotation_start_copy_data_to_virtual: dst_vir_addr = %x, list_copy_size=%x, list_size=%x \n", dst_vir_addr, list_copy_size, list_size);

	while (1) {
		ch_id = sprd_dma_request(DMA_UID_SOFTWARE, rotation_dma_irq, &dma_desc);
		if (ch_id < 0) {
			printk("rotation: convert endian request dma fail.ret : %d.\n", ret);
			msleep(5);
		} else {
			RTT_PRINT("rotation: convert endian request dma OK. ch_id:%d,total_len=0x%x.\n",
			     ch_id, total_len);
			break;
		}
	}
	memset(&dma_desc, 0, sizeof(struct sprd_dma_channel_desc));

	dma_cfg = (struct sprd_dma_linklist_desc *)dma_alloc_writecombine(NULL,
										sizeof(*dma_cfg) * list_size,
										&dma_cfg_phy,
										GFP_KERNEL);
	if (!dma_cfg) {
		printk("rotation_start_copy_data_to_virtual allocate failed, size=%d \n", sizeof(*dma_cfg) * list_size);
		return -ENOMEM;
	}

	memset(dma_cfg, 0x0, sizeof(*dma_cfg) * list_size);

	do_gettimeofday(&time1);
	//printk("pid = %d = 0x%x \n", current->pid, current->pid);
	for (i = 0; i < list_size; i++) {
		dma_dst_phy = user_va2pa(current->mm, dst_vir_addr+i*list_copy_size);
		if (0 == dma_dst_phy) {
			printk("rotation dst addr error, vir=0x%x, phy=0x%x\n",dst_vir_addr, dma_dst_phy);
			ret = -EFAULT;
			goto rotation_start_copy_data_to_virtual_exit;
		}
		//sprd_dma_default_linklist_setting(dma_cfg + i);
		dma_cfg[i].cfg = DMA_LIT_ENDIAN | DMA_SDATA_WIDTH32 | DMA_DDATA_WIDTH32 | DMA_REQMODE_LIST;
		dma_cfg[i].elem_postm = 0x4 << 16 | 0x4;
		dma_cfg[i].src_blk_postm = SRC_BURST_MODE_8;
		dma_cfg[i].dst_blk_postm = SRC_BURST_MODE_8;

		dma_cfg[i].llist_ptr = (u32) ((char *)dma_cfg_phy + sizeof(*dma_cfg) * (i + 1));
		dma_cfg[i].src_addr = dma_src_phy + i * list_copy_size;
		dma_cfg[i].dst_addr = dma_dst_phy;
		dma_cfg[i].total_len = (block_len > list_copy_size) ? list_copy_size : block_len;
		/* block length */
		dma_cfg[i].cfg |= list_copy_size & CFG_BLK_LEN_MASK;
		block_len -= dma_cfg[i].total_len;
	}
	do_gettimeofday(&time2);
	//printk("virtual:%x, physical:%x \n", dst_vir_addr, dma_cfg[0].dst_addr);
	RTT_PRINT("virtual/physical convert time=%d \n",((time2.tv_sec-time1.tv_sec)*1000*1000+(time2.tv_usec-time1.tv_usec)));

	dma_cfg[list_size - 1].cfg |= DMA_LLEND;

	dma_desc.llist_ptr = (uint32_t)dma_cfg_phy;
	sprd_dma_channel_config(ch_id, DMA_LINKLIST, &dma_desc);

	//sprd_dma_linklist_config(ch_id, dma_cfg_phy);

	sprd_dma_set_irq_type(ch_id, LINKLIST_DONE, 1);

	condition = 0;

	sprd_dma_channel_start(ch_id);

	if (wait_event_interruptible(wait_queue, condition)) {
		ret = -EFAULT;
	}

	sprd_dma_channel_stop(ch_id);

rotation_start_copy_data_to_virtual_exit:
	sprd_dma_free(ch_id);

	dma_free_writecombine(NULL, sizeof(*dma_cfg) * list_size, dma_cfg, dma_cfg_phy);

	/* do_gettimeofday(&te);*/
	/*printk("wjp:dma endian time=%d.\n",((te.tv_sec-ts.tv_sec)*1000+(te.tv_usec-ts.tv_usec)/1000));*/
	return ret;
}

static int rotation_start_copy_data_from_virtual(ROTATION_PARAM_T * param_ptr)
{
	struct sprd_dma_channel_desc dma_desc;
	uint32_t byte_per_pixel = 1;
	uint32_t src_img_postm = 0;
	uint32_t dst_img_postm = 0;
	uint32_t dma_dst_phy = param_ptr->dst_addr.y_addr;
	uint32_t src_vir_addr = param_ptr->src_addr.y_addr;
	uint32_t dma_src_phy;
	uint32_t block_len;
	uint32_t total_len;
	int32_t ret = 0;
	int ch_id = 0;
	int i;
	uint32_t list_size;
	uint32_t list_copy_size = 4096;
	struct sprd_dma_linklist_desc *dma_cfg;
	dma_addr_t dma_cfg_phy;
	struct timeval time1, time2;


	/*struct timeval ts;*/
	/*struct timeval te;*/
	/*printk("wjp:rotation_start_copy_data,w=%d,h=%d s!\n",param_ptr->img_size.w,param_ptr->img_size.h);*/
	if (ROTATION_YUV420 == param_ptr->data_format) {
		block_len = param_ptr->img_size.w * param_ptr->img_size.h * 3 / 2;
	} else if (ROTATION_RGB888 == param_ptr->data_format) {
		block_len = param_ptr->img_size.w * param_ptr->img_size.h * 4;
	} else {
		block_len = param_ptr->img_size.w * param_ptr->img_size.h * 2;
	}

	total_len = block_len;

	//do_gettimeofday(&ts);
	//RTT_PRINT("convert endian   %d,%d,%x,%x\n", width,height,input_addr,output_addr);

	if(0 != src_vir_addr%list_copy_size){
		printk("rotation_start_copy_data_from_virtual: src_vir_addr = %x not 4K bytes align, error \n", src_vir_addr);
		return -ENOMEM;
	}

	list_size = (total_len + list_copy_size -1)/list_copy_size;

	RTT_PRINT("rotation_start_copy_data_to_virtual: src_vir_addr = %x, list_copy_size=%x, list_size=%x \n", src_vir_addr, list_copy_size, list_size);

	while (1) {
		ch_id = sprd_dma_request(DMA_UID_SOFTWARE, rotation_dma_irq, &dma_desc);
		if (ch_id < 0) {
			printk("rotation: convert endian request dma fail.ret : %d.\n", ret);
			msleep(5);
		} else {
			RTT_PRINT("rotation: convert endian request dma OK. ch_id:%d,total_len=0x%x.\n",
			     ch_id, total_len);
			break;
		}
	}
	memset(&dma_desc, 0, sizeof(struct sprd_dma_channel_desc));

	dma_cfg = (struct sprd_dma_linklist_desc *)dma_alloc_writecombine(NULL,
										sizeof(*dma_cfg) * list_size,
										&dma_cfg_phy,
										GFP_KERNEL);
	if (!dma_cfg) {
		printk("rotation_start_copy_data_to_virtual allocate failed, size=%d \n", sizeof(*dma_cfg) * list_size);
		return -ENOMEM;
	}

	memset(dma_cfg, 0x0, sizeof(*dma_cfg) * list_size);

	do_gettimeofday(&time1);
	//printk("pid = %d = 0x%x \n", current->pid, current->pid);
	for (i = 0; i < list_size; i++) {
		dma_src_phy = user_va2pa(current->mm, src_vir_addr+i*list_copy_size);
		//sprd_dma_default_linklist_setting(dma_cfg + i);
		dma_cfg[i].cfg = DMA_LIT_ENDIAN | DMA_SDATA_WIDTH32 | DMA_DDATA_WIDTH32 | DMA_REQMODE_LIST;
		dma_cfg[i].elem_postm = 0x4 << 16 | 0x4;
		dma_cfg[i].src_blk_postm = SRC_BURST_MODE_8;
		dma_cfg[i].dst_blk_postm = SRC_BURST_MODE_8;

		dma_cfg[i].llist_ptr = (u32) ((char *)dma_cfg_phy + sizeof(*dma_cfg) * (i + 1));
		dma_cfg[i].src_addr = dma_src_phy;
		dma_cfg[i].dst_addr = dma_dst_phy + i * list_copy_size;
		dma_cfg[i].total_len = (block_len > list_copy_size) ? list_copy_size : block_len;
		/* block length */
		dma_cfg[i].cfg |= list_copy_size & CFG_BLK_LEN_MASK;
		block_len -= dma_cfg[i].total_len;
	}
	do_gettimeofday(&time2);
	//printk("virtual:%x, physical:%x \n", dst_vir_addr, dma_cfg[0].dst_addr);
	RTT_PRINT("virtual/physical convert time=%d \n",((time2.tv_sec-time1.tv_sec)*1000*1000+(time2.tv_usec-time1.tv_usec)));

	dma_cfg[list_size - 1].cfg |= DMA_LLEND;

	dma_desc.llist_ptr = (uint32_t)dma_cfg_phy;
	sprd_dma_channel_config(ch_id, DMA_LINKLIST, &dma_desc);

	//sprd_dma_linklist_config(ch_id, dma_cfg_phy);

	sprd_dma_set_irq_type(ch_id, LINKLIST_DONE, 1);

	condition = 0;

	sprd_dma_channel_start(ch_id);

	if (wait_event_interruptible(wait_queue, condition)) {
		ret = -EFAULT;
	}

	sprd_dma_channel_stop(ch_id);

	sprd_dma_free(ch_id);

	dma_free_writecombine(NULL, sizeof(*dma_cfg) * list_size, dma_cfg, dma_cfg_phy);

	/* do_gettimeofday(&te);*/
	/*printk("wjp:dma endian time=%d.\n",((te.tv_sec-ts.tv_sec)*1000+(te.tv_usec-ts.tv_usec)/1000));*/
	return ret;
}

static int rotation_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	ROTATION_PARAM_T *params;
	int ret = 0;
	params = (ROTATION_PARAM_T *) file->private_data;
	if (copy_from_user(params,
			   (ROTATION_PARAM_T *) arg, sizeof(ROTATION_PARAM_T)))
		return -EFAULT;
	mutex_lock(lock);
	switch (cmd) {
	case SPRD_ROTATION_DONE:
		if (rotation_start(params)) {
			ret = -EFAULT;
		}
		break;
	case SPRD_ROTATION_DATA_COPY:
		if (rotation_start_copy_data(params)) {
			ret = -EFAULT;
		}
		break;
	case SPRD_ROTATION_DATA_COPY_VIRTUAL:
		if (rotation_start_copy_data_to_virtual(params)) {
			ret = -EFAULT;
		}
		break;
	case SPRD_ROTATION_DATA_COPY_FROM_V_TO_P:
		if (rotation_start_copy_data_from_virtual(params)) {
			ret = -EFAULT;
		}
		break;
	default:
		break;
	}
	mutex_unlock(lock);
	return ret;
}

static struct file_operations rotation_fops = {
	.owner = THIS_MODULE,
	.open = rotation_open,
	.unlocked_ioctl = rotation_ioctl,
	.release = rotation_release,
};

static struct miscdevice rotation_dev = {
	.minor = ROTATION_MINOR,
	.name = "sprd_rotation",
	.fops = &rotation_fops,
};

int rotation_probe(struct platform_device *pdev)
{
	int ret;
	printk(KERN_ALERT "rotation_probe called\n");

	ret = misc_register(&rotation_dev);
	if (ret) {
		printk(KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
		       ROTATION_MINOR, ret);
		return ret;
	}
	lock = (struct mutex *)kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (lock == NULL)
		return -1;
	mutex_init(lock);
	init_waitqueue_head(&wait_queue);
	printk(KERN_ALERT " rotation_probe Success\n");
	return 0;
}

static int rotation_remove(struct platform_device *dev)
{
	printk(KERN_INFO "rotation_remove called !\n");
	misc_deregister(&rotation_dev);
	printk(KERN_INFO "rotation_remove Success !\n");
	return 0;
}

static struct platform_driver rotation_driver = {
	.probe = rotation_probe,
	.remove = rotation_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "sprd_rotation",
		   },
};

int __init rotation_init(void)
{
	printk(KERN_INFO "rotation_init called !\n");
	if (platform_driver_register(&rotation_driver) != 0) {
		printk("platform device register Failed \n");
		return -1;
	}
	init_MUTEX(&g_sem_rot);
	return 0;
}

void rotation_exit(void)
{
	printk(KERN_INFO "rotation_exit called !\n");
	platform_driver_unregister(&rotation_driver);
	mutex_destroy(lock);
	kfree(lock);
	lock = NULL;
}

module_init(rotation_init);
module_exit(rotation_exit);

MODULE_DESCRIPTION("rotation Driver");
MODULE_LICENSE("GPL");

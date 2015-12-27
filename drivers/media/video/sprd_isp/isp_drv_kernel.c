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
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <video/isp_drv_kernel.h>
#include "Tiger_reg_isp.h"
#include <asm/cacheflush.h>

#define DEBUG_ISP_DRV
#ifdef DEBUG_ISP_DRV
#define ISP_PRINT   printk
#else
#define ISP_PRINT(...)
#endif

#define ISP_QUEUE_LENGTH 16
#define SA_SHIRQ	IRQF_SHARED

static uint32_t                    g_isp_irq = 0x12345678;/*for share irq handler function*/
static uint32_t                    g_dcam_irq = 0x12345678;/*for share irq handler function*/
/*isp minor number*/
#define ISP_MINOR		MISC_DYNAMIC_MINOR
#define init_MUTEX(sem)		sema_init(sem, 1)
#define init_MUTEX_LOCKED(sem)	sema_init(sem, 0)

#define ISP_READL(a)	__raw_readl(a)
#define ISP_WRITEL(a,v)	__raw_writel(v,a)
#define ISP_OWR(a,v)	__raw_writel((__raw_readl(a) | v), a)
#define ISP_AWR(a,v)	__raw_writel((__raw_readl(a) & v), a)
#define ISP_NAWR(a,v)	__raw_writel((__raw_readl(a) & ~v), a)
#define ISP_REG_RD(a)	ISP_READL((a))

#define ISP_DCAM_IRQ_MASK	0x03
#define ISP_IRQ_MASK		0x0fff
#define ISP_DCAM_IRQ_NUM	0x02
#define ISP_IRQ_NUM		0x0c
#define ISP_INT_STOP_BIT		0x80000
struct isp_node {
	uint32_t	isp_irq_val;
	uint32_t	dcam_irq_val;
};

struct isp_queue {
	struct isp_node 	node[ISP_QUEUE_LENGTH];
	struct isp_node 	*write;
	struct isp_node 	*read;
};

struct isp_device_t
{
	uint32_t reg_base_addr;/*the pointer of isp register context*/
	uint32_t size;/* struct size*/
	struct isp_queue queue;
	struct semaphore sem_isr;/*for interrupts*/
	struct semaphore sem_isp;/*for the isp device, protect the isp hardware; protect  only  one caller use the oi*/ 
				/*controll/read/write functions*/
};

static atomic_t s_isp_users = ATOMIC_INIT(0);

struct mutex s_isp_lock;	/*for the isp driver, protect the isp module; protect only one user open this module*/
static struct proc_dir_entry*  isp_proc_file;

struct isp_device_t g_isp_device = { 0 };

uint32_t s_dcam_int_eb = 0x00;
uint32_t s_isp_alloc_addr = 0x00;
uint32_t s_isp_alloc_order = 0x00;
uint32_t s_isp_alloc_len = 0x00;

static DEFINE_SPINLOCK(isp_spin_lock);

static int32_t _isp_module_eb(void);
static int32_t _isp_module_dis(void);
static int  _isp_registerirq(void);
static void _isp_unregisterirq(void);
static irqreturn_t _isp_irq_root(int irq, void *dev_id);
static irqreturn_t _dcam_irq_root(int irq, void *dev_id);
static int _isp_queue_init(struct isp_queue *queue);
static int _isp_queue_write(struct isp_queue *queue, struct isp_node *node);
static int _isp_queue_read(struct isp_queue *queue, struct isp_node *node);
static inline void _isp_regread(char *dst,  char *src, size_t n);
static inline void  _isp_regwrite(char *dst,  char *src, size_t n);
static void _read_reg(struct isp_reg_bits *reg_bits_ptr, uint32_t counts);
static void _write_reg(struct isp_reg_bits *reg_bits_ptr, uint32_t counts);

/**file operation functions declare**/
static int32_t _isp_kernel_open(struct inode *node, struct file *filp);
static int32_t _isp_kernel_release(struct inode *node, struct file *filp);
static loff_t _isp_kernel_seek (struct file *pf, loff_t offset, int orig);
static ssize_t _isp_kernel_read (struct file *pf, char __user *buf, size_t count, loff_t * p);
static ssize_t _isp_kernel_write (struct file *fl, const char __user *buf, size_t count, loff_t * p);
static int32_t _isp_kernel_ioctl( struct file *fl, unsigned int cmd, unsigned long param);
/**driver'  functions declare**/
static int32_t _isp_probe(struct platform_device *pdev);
static int32_t _isp_remove(struct platform_device *dev);
/**module'  functions declare**/
static int32_t __init isp_kernel_init(void);
static void isp_kernel_exit(void);

static struct file_operations isp_fops = {
	.owner	= THIS_MODULE,
	.llseek	= _isp_kernel_seek,
	.open	= _isp_kernel_open,
	.unlocked_ioctl	= _isp_kernel_ioctl,
	.release	= _isp_kernel_release,
	.read	= _isp_kernel_read,
	.write	= _isp_kernel_write,
};

static struct miscdevice isp_dev = {
	.minor	= ISP_MINOR,
	.name	= "sprd_isp",
	.fops	= &isp_fops,
};

static struct platform_driver isp_driver = {
	.probe	= _isp_probe,
	.remove	= _isp_remove,
	.driver	= {
		.owner = THIS_MODULE,
		.name = "sprd_isp",
	},
};

static int32_t _isp_module_eb(void)
{
	int32_t ret = 0;
	uint32_t value=0x00;
	if (0x01 == atomic_inc_return(&s_isp_users)) {
		ISP_OWR(ISP_CORE_CLK_EB, ISP_CORE_CLK_EB_BIT);
		ISP_OWR(ISP_MODULE_EB, ISP_EB_BIT);
	}
	return ret;
}

static int32_t _isp_module_dis(void)
{
	int32_t	ret = 0;

	if (0x00 == atomic_dec_return(&s_isp_users)) {
		ISP_AWR(ISP_MODULE_EB, ~ISP_EB_BIT);
		ISP_AWR(ISP_CORE_CLK_EB, ~ISP_CORE_CLK_EB_BIT);
	}
	return ret;
}

static int32_t _isp_module_rst(void)
{
	int32_t	ret = 0;

	if (0x00 != atomic_read(&s_isp_users)) {
		ISP_WRITEL(ISP_INT_CLEAR, 0x0fff);
		ISP_OWR(ISP_MODULE_RESET, ISP_RST_BIT);
		ISP_OWR(ISP_MODULE_RESET, ISP_RST_BIT);
		ISP_OWR(ISP_MODULE_RESET, ISP_RST_BIT);
		ISP_AWR(ISP_MODULE_RESET, ~ISP_RST_BIT);
	}
	return ret;
}

static int32_t _isp_lnc_param_load(struct isp_reg_bits *reg_bits_ptr, uint32_t counts)
{
	int32_t ret = 0;
	struct isp_reg_bits reg_bits = {0x00};
	uint32_t reg_value=0x00;

	if((0x00!=s_isp_alloc_addr)
		&&(0x00!=s_isp_alloc_len)){

		void *ptr = (void*)s_isp_alloc_addr;
		uint32_t len=s_isp_alloc_len;
		dmac_flush_range(ptr, ptr + len);
		outer_flush_range(__pa(ptr), __pa(ptr) + len);

		reg_bits_ptr->reg_value=(uint32_t)__pa(reg_bits_ptr->reg_value);

		_write_reg(reg_bits_ptr, counts);

		reg_value=ISP_READL(ISP_INT_RAW);

		while(0x00==(reg_value&ISP_INT_LEN_S_LOAD))
		{
			msleep(1);
			reg_value=ISP_READL(ISP_INT_RAW);
		}

		ISP_OWR(ISP_INT_CLEAR, ISP_INT_LEN_S_LOAD);
	}else {
		ISP_PRINT("ISP_RAW: isp load lnc param error\n");
	}

	return ret;
}

static int32_t _isp_lnc_param_set(uint32_t* addr, uint32_t len)
{
	int32_t ret = 0;
	uint16_t* buf_addr=(uint16_t*)s_isp_alloc_addr;

	if((0x00!=s_isp_alloc_addr)
		&&(0x00!=addr))
	{
		memcpy((void*)s_isp_alloc_addr, (void*)addr, len);
	}

	return ret;
}

static int32_t _isp_alloc(uint32_t* addr, uint32_t len)
{
	int32_t ret = 0;
	uint32_t buf=0x00;
	uint32_t vir_buf=0x00;
	uint32_t mem_order=0x00;
	void *ptr;
	if(0x00==s_isp_alloc_addr) {
		s_isp_alloc_len=len;
		s_isp_alloc_order = get_order(len);
		s_isp_alloc_addr = (uint32_t)__get_free_pages(GFP_KERNEL | __GFP_COMP, s_isp_alloc_order);
		if (NULL == (void*)s_isp_alloc_addr) {
			ISP_PRINT("ISP_RAW:_isp_alloc null error\n");
			return 1;
		}
		ptr = (void*)s_isp_alloc_addr;
		*addr = s_isp_alloc_addr;
		buf = virt_to_phys(s_isp_alloc_addr);
		dmac_flush_range(ptr, ptr + len);
		outer_flush_range(__pa(ptr), __pa(ptr) + len);
	}

	return ret;
}

static int32_t _isp_free(void)
{
	int32_t ret = 0;

	if((0x00!=s_isp_alloc_addr)
		&&(0x00!=s_isp_alloc_order))
	{
		free_pages(s_isp_alloc_addr, s_isp_alloc_order);
		s_isp_alloc_order = 0x00;
		s_isp_alloc_addr = 0x00;
		s_isp_alloc_len=0x00;
	}

	return ret;
}

static int32_t _isp_set_clk(enum isp_clk_sel clk_sel)
{
	int32_t       rtn = 0;
	return rtn;
}

void isp_int_en(void)
{
	//enable_irq(ISP_IRQ);
}

void isp_int_dis(void)
{
	//disable_irq(ISP_IRQ);
}

static int _isp_queue_init(struct isp_queue *queue)
{
	if (NULL == queue)
		return -EINVAL;

	memset(queue, 0x00, sizeof(*queue));
	queue->write = &queue->node[0];
	queue->read  = &queue->node[0];

	return 0;
}

static int _isp_queue_write(struct isp_queue *queue, struct isp_node *node)
{
	struct isp_node 	*ori_node = queue->write;

	if (NULL == queue || NULL == node)
	return -EINVAL;

	//ISP_PRINT("_isp_queue_write called!\n");
	*queue->write++ = *node;
	if (queue->write > &queue->node[ISP_QUEUE_LENGTH-1]) {
		queue->write = &queue->node[0];
	}

	if (queue->write == queue->read) {
	queue->write = ori_node;
	}

	//ISP_PRINT("_isp_queue_write finished!\n");
	return 0;
}

static int _isp_queue_read(struct isp_queue *queue, struct isp_node *node)
{
	int ret = 0;

	if (NULL == queue || NULL == node)
		return -EINVAL;
	//ISP_PRINT("_isp_queue_read called!\n");
	if (queue->read != queue->write) {
		*node = *queue->read++;
		if (queue->read > &queue->node[ISP_QUEUE_LENGTH-1]) {
			queue->read = &queue->node[0];
		}
	}
	//ISP_PRINT("_isp_queue_read finished!\n");
	return ret;
}

static inline void _isp_regread(char *dst,  char *src, size_t n)
{
	char tmp  = 0;
	uint32_t tmp2 = 0;
	char *char_src = 0,*d = 0;
	uint32_t *d2 = (uint32_t*) dst;
	uint32_t *int_src =  (uint32_t*) src;
	uint32_t counts = (n>>2)<<2;
	uint32_t res_counts = n -counts;
	counts = counts>>2;

	while (counts--) {
		tmp2 = ISP_READL(int_src);
		*d2++ = tmp2;
		int_src++;
	}

	if(res_counts) {
		d = (char*) d2;
		char_src = (char*) int_src;
		while(res_counts--) {
			tmp = __raw_readb(char_src);
			*d = tmp;
			char_src++;
		}
	}
}

static inline void _isp_regwrite(char *dst,  char *src, size_t n)
{
	uint32_t tmp2 = 0;
	char *char_src = 0, *d = 0;
	uint32_t *int_src = 0, *d2 = 0;
	uint32_t counts = 0, res_counts = 0;

	int_src = (uint32_t*) src;
	d2 = (uint32_t*) dst;
	counts = (n>>2)<<2;
	res_counts = n - counts;
	counts = counts>>2;

	while (counts--) {
		tmp2 = *int_src++;
		ISP_WRITEL(d2, tmp2);
		d2++;
	}

	if(res_counts) {
		d = (char*) d2;
		char_src = (char*) int_src;

		while(res_counts--) {
			tmp2 = *char_src++;
			 __raw_writeb(tmp2, d);
			d++;
		}
	}

}
static int32_t _isp_get_ctlr(void *param)
{
	struct isp_device_t *dev_ptr = (struct isp_device_t *) param;
	down(&dev_ptr->sem_isp);

	return 0;
}
static int32_t _isp_put_ctlr(void *param)
{
	struct isp_device_t *dev_ptr = (struct isp_device_t *) param;
	up(&dev_ptr->sem_isp);

	return 0;
}

static int _isp_en_irq(unsigned long int_num)
{
	uint32_t ret = 0;

	ISP_WRITEL(ISP_INT_CLEAR, 0x0fff);
	ISP_WRITEL(ISP_INT_EN, int_num);

	return ret;
}
static void _isp_dis_irq(unsigned long int_num)
{
	ISP_NAWR(ISP_INT_EN, int_num);

}

static int _isp_registerirq(void)
{
	uint32_t ret = 0;
//	ret = request_irq(ISP_IRQ, _isp_irq_root, SA_SHIRQ, "ISP", &g_isp_irq);
//	ret = request_irq(DCAM_IRQ, _dcam_irq_root, SA_SHIRQ, "ISP", &g_dcam_irq);

	ret = request_irq(ISP_IRQ, _isp_irq_root, IRQF_SHARED, "ISP", &g_isp_irq);

	return ret;
}
static void _isp_unregisterirq(void)
{
	//free_irq (DCAM_IRQ, &g_dcam_irq);
	free_irq (ISP_IRQ, &g_isp_irq);
}

static int _isp_cfg_dcam_int(uint32_t param)
{
	uint32_t ret = 0;

	s_dcam_int_eb = param;

	return ret;
}

static void _read_reg(struct isp_reg_bits *reg_bits_ptr, uint32_t counts)
{
	uint32_t i = 0;
	uint32_t reg_val = 0, reg_addr = 0;

	for (i = 0; i<counts; ++i) {
		//reg_addr = reg_bits_ptr[i].reg_addr;
		reg_addr = ISP_BASE_ADDR + reg_bits_ptr[i].reg_addr;
		reg_val = ISP_READL(reg_addr);
		reg_bits_ptr[i].reg_value = reg_val;
	}

}

static void _write_reg(struct isp_reg_bits *reg_bits_ptr, uint32_t counts)
{
	uint32_t i = 0;
	uint32_t reg_val = 0, reg_addr = 0;

	for (i = 0; i<counts; ++i) {
		reg_addr = reg_bits_ptr[i].reg_addr+ISP_BASE_ADDR;
		reg_val = reg_bits_ptr[i].reg_value;
		ISP_WRITEL(reg_addr, reg_val);
	}
}

/**********************************************************
*open the device
*
***********************************************************/
static int32_t _isp_kernel_open (struct inode *node, struct file *pf)
{
	int32_t ret = 0;
	ISP_PRINT ("isp_k: open called \n");

	ret =  _isp_get_ctlr(&g_isp_device);
	if (unlikely(ret)) {
		ISP_PRINT ("isp_k: get control failed \n");
		return -EFAULT;
	}
	
	ret = _isp_module_eb();
	if (unlikely(0 != ret)) {
		ISP_PRINT("isp_k: Failed to enable isp module \n");
		ret = -EIO;
		return ret;
	}

	ret = _isp_module_rst();
	if (unlikely(0 != ret)) {
		ISP_PRINT("isp_k: Failed to reset isp module \n");
		ret = -EIO;
		return ret;
	}

	g_isp_device.reg_base_addr = (uint32_t)ISP_BASE_ADDR;
	g_isp_device.size = ISP_REG_MAX_SIZE;
	ret = _isp_queue_init(&(g_isp_device.queue));

	ISP_PRINT ("isp_k: open finished \n");

	return 0;
}

/**********************************************************
*move the current pointer to some position
*loff_t p:
*int orig: 0: move offset from start point;1: move offset from current point
***********************************************************/
static loff_t _isp_kernel_seek (struct file *pf, loff_t offset, int orig)
{
	loff_t retval = -EINVAL;

	ISP_PRINT ("isp_k:  seek called \n");
	ISP_PRINT ("isp_k:seek orig = 0x%x, offset = 0x%x\n", (int32_t)orig, (int32_t)offset);
	if (g_isp_device.reg_base_addr == 0||offset < 0) {
		return -EFAULT;
	}

	mutex_lock (&s_isp_lock );

	switch (orig) {
	case SEEK_SET:
	 if ( offset < g_isp_device.size ) {
		pf->f_pos = offset;
		retval = pf->f_pos;
	} else {
		ISP_PRINT ("isp_k: seek offset is invalidated, offset = %d", (int32_t)offset);
		retval = -EINVAL;
	}
	break;

	case SEEK_CUR:
	if ((offset + pf->f_pos) <0) {
		ISP_PRINT ("isp_k: seek offset is invalidated, offset = %d", (int32_t)offset);
		retval = -EINVAL;
	}else if((offset + pf->f_pos) <g_isp_device.size){
		pf->f_pos += offset;
		retval = pf->f_pos;
	} else {
		pf->f_pos = ISP_REG_MAX_SIZE-1;
		retval = pf->f_pos;
	}
	break;
	case SEEK_END:
	if (offset>ISP_REG_MAX_SIZE) {
		pf->f_pos = 0;
		retval = 0;
	}else {
		pf->f_pos = ISP_REG_MAX_SIZE-1-offset;
		retval = pf->f_pos;
	}
	break;

	default:
		retval = -EINVAL;
		ISP_PRINT ("isp_k: seek orig is invalidate");
		break;
	}

	mutex_unlock (&s_isp_lock );
	ISP_PRINT ("isp_k: seek finished \n");
	return retval;

}

/**********************************************************
*read info from  file
*size_t size:
*loff_t p:
***********************************************************/
static ssize_t _isp_kernel_read (struct file *pf, char __user *buf, size_t count, loff_t * p)
{
	ssize_t ret = 0;
	ssize_t total_read = 0;
	loff_t offsets = 0;
	char *tmp_buf = 0;
	char *reg_base_add = 0;

	//ISP_PRINT("isp_k: read called\n");

	if(pf == 0|| buf == 0||p == 0) {
		ret = -EFAULT;
		ISP_PRINT("isp_k: read invalidate param, pf = 0x%x, buf = 0x%x, p = 0x%x\n", (int32_t)pf, (int32_t)buf, (uint32_t)p );
		goto free_tmp_buf;

	}

	if (g_isp_device.reg_base_addr == 0) {
		ret = -EFAULT;
		ISP_PRINT("isp_k: read invalidate param, reg_base_addr = 0x%x\n",  (int32_t)g_isp_device.reg_base_addr);
		goto free_tmp_buf;
	}
	mutex_lock (&s_isp_lock);
	if (count < 0) {
		ret = -EINVAL;
		mutex_unlock(&s_isp_lock);
		ISP_PRINT("isp_k: read invalidate param, count = %d\n", (int32_t)count);
		return ret;
	}
	tmp_buf = (char*) kzalloc(count, GFP_KERNEL);
	if(unlikely(!tmp_buf)) {
		ret = -EFAULT;
		mutex_unlock(&s_isp_lock);
		ISP_PRINT("isp_k: read kzalloc failed\n ");
		goto free_tmp_buf;
	}

	if (unlikely (!p)) {
		offsets  = pf->f_pos;
	} else {
		offsets = *p;
	}

	if ( (offsets < 0)||(g_isp_device.size < offsets)) {
		ret = -EINVAL;
		mutex_unlock(&s_isp_lock);
		ISP_PRINT("isp_k: read invalidate param, offsets = %d\n", (int32_t)offsets);
		goto free_tmp_buf;
	}

	reg_base_add = (char*)g_isp_device.reg_base_addr;

	if (g_isp_device.size<offsets+count) {
		total_read = g_isp_device.size -offsets;
	} else {
		total_read = count;
	}

	_isp_regread (tmp_buf, (char*) reg_base_add+offsets, total_read);

	ret = copy_to_user(buf, tmp_buf, total_read);
	if (unlikely(ret<0)) {
		mutex_unlock(&s_isp_lock);
		ISP_PRINT("isp_k: read copy_to_user failed, ret = %d\n", (int32_t)ret);
		goto free_tmp_buf;
	}
	pf->f_pos += total_read;

	mutex_unlock(&s_isp_lock);

	if (p) {
		* p += total_read;
	}
	ret = total_read;

	//ISP_PRINT("isp_k: read finished\n");

free_tmp_buf:

	if (tmp_buf) {
		kfree (tmp_buf);
		tmp_buf = NULL;
	}

	return ret;

}

/**********************************************************
*write info to file
*loff_t p:
*int orig:
***********************************************************/
static ssize_t _isp_kernel_write (struct file *fl, const char __user *buf, size_t count, loff_t * p)
{
	ssize_t ret = 0;
	ssize_t total_write = 0;
	loff_t offsets = 0;
	char *tmp_buf = NULL;
	char *reg_base_add = NULL;

	//ISP_PRINT("isp_k:write called\n");

	if (fl == 0|| buf == 0) {
		ret = -EFAULT;
		ISP_PRINT("isp_k: write:invalidate param, fl = 0x%x, buf = 0x%x\n", (int32_t)fl, (int32_t)buf );
		goto func_exit;
	}

	if (g_isp_device.reg_base_addr == 0) {
		ret = -EFAULT;
		ISP_PRINT("isp_k: write invalidate param, reg_base_addr = 0x%x\n", (int32_t)g_isp_device.reg_base_addr);
		goto func_exit;
	}

	mutex_lock (&s_isp_lock);
	if (count < 0) {
		ret  = -EINVAL;
		ISP_PRINT ("isp_k: write invalidate param, count = %d\n", (int32_t) count);
		goto func_exit;
	}

	tmp_buf = (char*) kzalloc(count, GFP_KERNEL);
	if (unlikely (!tmp_buf)) {
		mutex_unlock(&s_isp_lock);
		ISP_PRINT("isp_k:write kzalloc failed \n!");
		ret = -EFAULT;
		goto func_exit;
	}
	ret = copy_from_user(tmp_buf, buf, count);
	if (unlikely(ret < 0)) {
		mutex_unlock(&s_isp_lock);
		ISP_PRINT("isp_k:write copy_from_user failed, ret = %d\n", (int32_t)ret);
		goto func_exit;
	}

	if (!p) {
		offsets  = fl->f_pos;
	} else {
		offsets = *p;
	}

	if ( (offsets< 0)||(g_isp_device.size<offsets)) {
		mutex_unlock(&s_isp_lock);
		ret = -EINVAL;
		goto func_exit;
	}

	reg_base_add = (char*)g_isp_device.reg_base_addr;

	if (g_isp_device.size< offsets+count ) {
		total_write = g_isp_device.size-offsets;
	} else {
		total_write = count;
	}
	_isp_regwrite ((char*)reg_base_add+offsets, tmp_buf,total_write);

	fl->f_pos += total_write;

	mutex_unlock(&s_isp_lock);
	if (!p) {
		* p += total_write;
	}
	ret = total_write;
	//ISP_PRINT("isp_k:write finished \n");

func_exit:
	if(tmp_buf) {
		kfree(tmp_buf);
		tmp_buf = NULL;
	}

	return ret;
}

static irqreturn_t _isp_irq_root(int irq, void *dev_id)
{
	int32_t	ret = 0;
	uint32_t	status = 0;
	uint32_t	irq_line = 0;
	uint32_t	irq_status = 0;
	uint32_t	flag = 0, i = 0;
	struct isp_node    node = { 0 };

	status = ISP_REG_RD(ISP_INT_STATUS);
	irq_line = status&ISP_IRQ_MASK;
	//ISP_PRINT("ISP_RAW:isp_k: isp irq: 0x%x\n", irq_line);
	if ( 0 == irq_line ) {
		return IRQ_HANDLED;
	}

	for (i = ISP_IRQ_NUM- 1; i >= 0; i--) {
		if (irq_line & (1 << (uint32_t)i)) {
		irq_status |= 1 << (uint32_t)i;
		}
		irq_line &= ~(uint32_t)(1 << (uint32_t)i); //clear the interrupt flag
		if(!irq_line) //no interrupt source left
		break;
	}

	spin_lock_irqsave(&isp_spin_lock,flag);
	node.isp_irq_val = irq_status;
	ISP_WRITEL(ISP_INT_CLEAR, irq_status);
	ret = _isp_queue_write((struct isp_queue *)&g_isp_device.queue, (struct isp_node*)&node);
	spin_unlock_irqrestore(&isp_spin_lock, flag);
	up(&g_isp_device.sem_isr);

	return IRQ_HANDLED;
}
//static irqreturn_t _dcam_irq_root(int irq, void *dev_id)
void _dcam_isp_root(void)
{
	int32_t	ret = 0;
	uint32_t	irq_line = 0;
	uint32_t	status = 0;
	uint32_t	irq_status = 0;
	uint32_t	 flag = 0, i = 0;
	struct isp_node node = { 0 };

	//ISP_PRINT ("ISP_RAW: isp_k: _dcam_isp_root %d \n", s_dcam_int_eb);

	if(0x00 !=s_dcam_int_eb)
	{
		spin_lock_irqsave(&isp_spin_lock,flag);
		node.dcam_irq_val = 0x01;

		//ISP_PRINT("isp_k: dcam sof irq :0x%x\n", node.dcam_irq_val);
		ret = _isp_queue_write((struct isp_queue *)&g_isp_device.queue, (struct isp_node*)&node);
		spin_unlock_irqrestore(&isp_spin_lock, flag);
		up(&g_isp_device.sem_isr);
	}

	return IRQ_HANDLED;
}

/**********************************************************
*release the device
*
***********************************************************/
static int32_t _isp_kernel_release (struct inode *node, struct file *pf)
{
	int ret = 0;
	ISP_PRINT ("isp_k: release called \n");

	_isp_unregisterirq();
	_isp_module_dis();
	_isp_free();
	ret = _isp_put_ctlr(&g_isp_device);
	if (unlikely (ret) ) {
	ISP_PRINT ("isp_k: release control  failed \n");
	return -EFAULT;
	}
	ISP_PRINT ("isp_k: release finished \n");
	return ret;
}
/**********************************************************
*read info from  file
*size_t size:
*loff_t p:
***********************************************************/
static int32_t _isp_kernel_proc_read (char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int	 len = 0;
	uint32_t	 reg_buf_len = 200;
	uint32_t	 print_len = 0, print_cnt = 0;
	uint32_t	*reg_ptr = 0;

	(void)start; (void)off; (void)count; (void)eof;

	reg_ptr = (uint32_t*)g_isp_device.reg_base_addr;
	len += sprintf(page + len, "Context for ISP device \n");
	len += sprintf(page + len, "********************************************* \n");
	while (print_len < reg_buf_len) {
	len += sprintf(page + len, "offset 0x%x : 0x%x, 0x%x, 0x%x, 0x%x \n",
		print_len,
		reg_ptr[print_cnt],
		reg_ptr[print_cnt+1],
		reg_ptr[print_cnt+2],
		reg_ptr[print_cnt+3]);
	print_cnt += 4;
	print_len += 16;
	}
	len += sprintf(page + len, "********************************************* \n");
	len += sprintf(page + len, "The end of ISP device \n");

	return len;
}

/**********************************************************
*the io controller of isp
*unsigned int cmd:
*unsigned long param:
***********************************************************/
static int32_t _isp_kernel_ioctl( struct file *fl, unsigned int cmd, unsigned long param)
{
	int32_t ret = 0;
	uint32_t isp_irq, dcam_irq;
	struct isp_irq_param irq_param = { 0 };
	struct isp_node isp_node = { 0 };
	struct isp_reg_param reg_param = { 0 };
	struct isp_reg_bits *reg_bits_ptr = 0;

	//ISP_PRINT("isp_k:_ioctl called, cmd: %x\n", cmd);

	if (!fl) {
		return -EINVAL;
	}

	if(ISP_IO_IRQ==cmd)
	{
		down_interruptible(&g_isp_device.sem_isr);
		ret=_isp_queue_read(&g_isp_device.queue, &isp_node);
		if ( 0 != ret) {
			ISP_PRINT("isp_k: ioctl irq: _isp_queue_read error, ret = 0x%x", ret);
			ret = -EFAULT;
		}

		irq_param.dcam_irq_val = isp_node.dcam_irq_val;
		irq_param.isp_irq_val = isp_node.isp_irq_val;
		isp_irq = isp_node.isp_irq_val;
		dcam_irq = isp_node.dcam_irq_val;
		irq_param.irq_val = (dcam_irq<<ISP_IRQ_NUM)|isp_irq;
		ret = copy_to_user ((void*) param, (void*)&irq_param, sizeof(struct isp_irq_param));
		if ( 0 != ret) {
			ISP_PRINT("isp_k: ioctl irq: copy_to_user failed, ret = 0x%x", ret);
			ret = -EFAULT;
		}

	} else {

		mutex_lock(&s_isp_lock);

		switch (cmd)
		{
			case ISP_IO_READ: {
			uint32_t buf_size = 0;
			//ISP_PRINT("  isp_k:_ioctl read called \n");
			ret = copy_from_user((void*)&reg_param, (void*)param, sizeof(struct isp_reg_param));
			if ( 0 != ret) {

				ISP_PRINT("isp_k: ioctl read: copy_to_user failed, ret = 0x%x", ret);
				ret = -EFAULT;
				goto IO_READ_EXIT;
			}
			buf_size = reg_param.counts*sizeof(struct isp_reg_bits);
			reg_bits_ptr = (struct isp_reg_bits*) kzalloc(buf_size, GFP_KERNEL);
			ret = copy_from_user((void*)reg_bits_ptr, (void*)reg_param.reg_param, buf_size);
			if ( 0 != ret) {

				ISP_PRINT("isp_k: ioctl read: copy_to_user failed, ret = 0x%x", ret);
				ret  =-EFAULT;
				goto IO_READ_EXIT;
			}
			_read_reg(reg_bits_ptr, reg_param.counts);

			ret = copy_to_user((void*)reg_param.reg_param, (void*)reg_bits_ptr, buf_size);
			if ( 0 != ret) {

				ISP_PRINT("isp_k: ioctl read: copy_to_user failed, ret = 0x%x", ret);
				ret = -EFAULT;
				goto IO_READ_EXIT;
			}
			IO_READ_EXIT:
			if(reg_bits_ptr) {
				kfree(reg_bits_ptr);
				reg_bits_ptr = 0;
			}
			}
			break;

			case ISP_IO_WRITE: {
			uint32_t buf_size = 0;
			//ISP_PRINT(" isp_k:_ioctl write called \n");
			ret = copy_from_user((void*)&reg_param, (void*)param, sizeof(struct isp_reg_param));
			if ( 0 != ret) {

				ISP_PRINT("isp_k: ioctl write: copy_to_user failed, ret = 0x%x", ret);
				ret = -EFAULT;
				goto IO_WRITE_EXIT;
			}
			buf_size = reg_param.counts*sizeof(struct isp_reg_bits);
			reg_bits_ptr = (struct isp_reg_bits*) kzalloc(buf_size, GFP_KERNEL);
			if(0x00==reg_bits_ptr)
			{
				ret = -EFAULT;
				goto IO_WRITE_EXIT;
			}
			ret = copy_from_user((void*)reg_bits_ptr, (void*)reg_param.reg_param, buf_size);
			if ( 0 != ret) {

				ISP_PRINT("isp_k: ioctl write: copy_to_user failed, ret = 0x%x", ret);
				ret = -EFAULT;
				goto IO_WRITE_EXIT;
			}
			_write_reg(reg_bits_ptr, reg_param.counts);

			IO_WRITE_EXIT:
			if(reg_bits_ptr) {
				kfree(reg_bits_ptr);
				reg_bits_ptr = 0;
			}

			}
			break;

			case ISP_IO_RST: {
			ISP_PRINT(" isp_k:ioctl restet called \n");
			ret = _isp_module_rst();
			if (ret) {
				ISP_PRINT("isp_k: ioctl restet failed!\n");
				ret = -EFAULT;
			}
			}
			break;

			case ISP_IO_SETCLK: {
			ISP_PRINT(" isp_k:ioctl set clock called \n");
			//ret = _isp_set_clk(ISP_CLK_xxx);
			if (ret) {
				ISP_PRINT("isp_k: ioctl set clock failed!\n");
				ret = -EFAULT;
			}
			}
			break;

			case ISP_IO_STOP: {
			uint32_t flag = 0;
			struct isp_node node = { 0 };
			ISP_PRINT("isp_k: ioctl  stop called !\n");
			spin_lock_irqsave(&isp_spin_lock,flag);
			node.dcam_irq_val = ISP_INT_STOP_BIT;
			ret = _isp_queue_write((struct isp_queue *)&g_isp_device.queue, (struct isp_node*)&node);
			spin_unlock_irqrestore(&isp_spin_lock, flag);
			up(&g_isp_device.sem_isr);
			}
			break;

			case ISP_IO_INT: {
				unsigned long int_num;
				ret = copy_from_user((void*)&int_num, (void*)param, 0x04);
				ret = _isp_en_irq(int_num);
				ret = _isp_registerirq();
				if (unlikely(ret)) {
					ISP_PRINT ("isp_k:register interrupt failed \n");
					ret = -EFAULT;
				}
			}
			break;

			case ISP_IO_DCAM_INT: {
				unsigned long int_param;
				ret = copy_from_user((void*)&int_param, (void*)param, 0x04);
				ret = _isp_cfg_dcam_int(int_param);
				if (unlikely(ret)) {
					ISP_PRINT ("isp_k:cfg dcam interrupt failed \n");
					ret = -EFAULT;
				}
			}
			break;

			case ISP_IO_LNC_PARAM: {
				uint32_t buf_size = 0;
				uint32_t* addr = 0;
				ret = copy_from_user((void*)&reg_param, (void*)param, sizeof(struct isp_reg_param));
				if ( 0 != ret) {

					ISP_PRINT("isp_k: ioctl lnc param: copy_to_user failed, ret = 0x%x", ret);
					ret = -EFAULT;
					goto IO_LNC_PARAM_EXIT;
				}
				buf_size = reg_param.counts;
				addr = (uint32_t*) kzalloc(buf_size, GFP_KERNEL);
				if(0x00==addr){
					ret = -EFAULT;
					goto IO_LNC_PARAM_EXIT;
				}
				ret = copy_from_user((void*)addr, (void*)reg_param.reg_param, buf_size);
				if ( 0 != ret) {

					ISP_PRINT("isp_k: ioctl lnc param: copy_to_user failed, ret = 0x%x", ret);
					ret = -EFAULT;
					goto IO_LNC_PARAM_EXIT;
				}
				ret = _isp_lnc_param_set(addr, buf_size);
				if ( 0 != ret) {
					ISP_PRINT("isp_k: ioctl lnc param failed, ret = 0x%x", ret);
					ret = -EFAULT;
					goto IO_LNC_PARAM_EXIT;
				}

				IO_LNC_PARAM_EXIT:
				if(addr) {
					kfree(addr);
					addr = 0;
				}
			}
			break;

			case ISP_IO_LNC: {
				uint32_t buf_size = 0;
				ret = copy_from_user((void*)&reg_param, (void*)param, sizeof(struct isp_reg_param));
				if ( 0 != ret) {
					ISP_PRINT("isp_k: ioctl lnc: copy_to_user failed, ret = 0x%x", ret);
					ret = -EFAULT;
					goto IO_LNC_EXIT;
				}
				buf_size = reg_param.counts*sizeof(struct isp_reg_bits);
				reg_bits_ptr = (struct isp_reg_bits*) kzalloc(buf_size, GFP_KERNEL);
				if(0x00==reg_bits_ptr)
				{
					ret = -EFAULT;
					goto IO_LNC_EXIT;
				}
				ret = copy_from_user((void*)reg_bits_ptr, (void*)reg_param.reg_param, buf_size);
				if ( 0 != ret) {
					ISP_PRINT("isp_k: ioctl lnc: copy_to_user failed, ret = 0x%x", ret);
					ret = -EFAULT;
					goto IO_LNC_EXIT;
				}

				ret = _isp_lnc_param_load(reg_bits_ptr, reg_param.counts);
				if (unlikely(ret)) {
					ISP_PRINT ("isp_k:load lnc failed \n");
					ret = -EFAULT;
				}
				IO_LNC_EXIT:
				if(reg_bits_ptr) {
					kfree(reg_bits_ptr);
					reg_bits_ptr = 0;
				}
			}
			break;

			case ISP_IO_ALLOC: {
				uint32_t buf_size = 0;
				ret = copy_from_user((void*)&reg_param, (void*)param, sizeof(struct isp_reg_param));
				if ( 0 != ret) {
					ISP_PRINT("isp_k: ioctl write: copy_to_user failed, ret = 0x%x", ret);
					ret = -EFAULT;
					break;
				}
				ret = _isp_alloc(&reg_param.reg_param, reg_param.counts);
				if ( 0 == ret) {
					ret = copy_to_user((void*)param, (void*)&reg_param, sizeof(struct isp_reg_param));
				} else {
					ISP_PRINT("isp_k: ioctl alloc failed, ret = 0x%x", ret);
					ret = -EFAULT;
				}
			}
			break;

			default:
			mutex_unlock(&s_isp_lock);
			ISP_PRINT("isp_k:_ioctl cmd is unsupported, cmd = %x\n", (int32_t)cmd);
			return -EFAULT;
		}

		mutex_unlock(&s_isp_lock);
	}

	//ISP_PRINT("isp_k:_ioctl finished\n");

	return ret;
}

static int _isp_probe(struct platform_device *pdev)
{
	int ret = 0;
	ISP_PRINT ("isp_k:probe called\n");

	ret = misc_register(&isp_dev);
	if (ret) {
		ISP_PRINT ( "isp_k:probe cannot register miscdev on minor=%d (%d)\n",(int32_t)ISP_MINOR, (int32_t)ret);
		return ret;
	}
	//mutex_init(&g_isp_device.isp_lock);
	mutex_init(&s_isp_lock);

	isp_proc_file = create_proc_read_entry("driver/sprd_isp" ,
					0444,
					NULL,
					_isp_kernel_proc_read,
					NULL);
	if (unlikely(NULL == isp_proc_file)) {
		ISP_PRINT("isp_k:probe Can't create an entry for isp in /proc \n");
		ret = ENOMEM;
		return ret;
	}

	ISP_PRINT (" isp_k:probe Success\n");
	return 0;
}

static int _isp_remove(struct platform_device * dev)
{
	ISP_PRINT ("isp_k: remove called !\n");

	misc_deregister(&isp_dev);

	if (isp_proc_file) {
		remove_proc_entry("driver/sprd_isp", NULL);
	}

	ISP_PRINT ("isp_k: remove Success !\n");

	return 0;
}

static int32_t __init isp_kernel_init(void)
{
	ISP_PRINT ("isp_k: init called \n");
	if (platform_driver_register(&isp_driver) != 0) {
		ISP_PRINT ("isp_kernel_init: platform device register Failed \n");
		return -1;
	}
	memset(&g_isp_device, 0, sizeof(struct isp_device_t));
	init_MUTEX(&g_isp_device.sem_isp);
	init_MUTEX_LOCKED(&g_isp_device.sem_isr); /*for interrupt */

	ISP_PRINT ("isp_k: init finished\n");
	return 0;
}

static void isp_kernel_exit(void)
{
	ISP_PRINT ("isp_k: exit called \n");
	platform_driver_unregister(&isp_driver);
	mutex_destroy(&s_isp_lock);
	memset (&g_isp_device, 0, sizeof(struct isp_device_t));
	ISP_PRINT ("isp_k: exit finished \n");
}

module_init(isp_kernel_init);
module_exit(isp_kernel_exit);

MODULE_DESCRIPTION("Isp Driver");
MODULE_LICENSE("GPL");

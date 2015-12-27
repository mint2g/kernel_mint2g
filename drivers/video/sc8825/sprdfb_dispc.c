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

#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <mach/irqs.h>

#include "sprdfb_dispc_reg.h"
#include "sprdfb.h"

#define DISPC_SOFT_RST (20)
#define DISPC_CLOCK_PARENT ("clk_256m")
#define DISPC_CLOCK (256*1000000)
#define DISPC_DBI_CLOCK_PARENT ("clk_256m")
#define DISPC_DBI_CLOCK (256*1000000)
#define DISPC_DPI_CLOCK_PARENT ("clk_384m")
#define DISPC_DPI_CLOCK (384*1000000/11)

#define DISPMTX_CLK_EN (11)
#define DISPC_CORE_CLK_EN (9)

struct sprdfb_dispc_context {
	struct clk		*clk_dispc;
	struct clk 		*clk_dispc_dpi;
	struct clk 		*clk_dispc_dbi;
	bool			is_inited;
	bool			is_first_frame;

	struct sprdfb_device	*dev;

	uint32_t	 	vsync_waiter;
	wait_queue_head_t		vsync_queue;
	uint32_t	        vsync_done;

#ifdef  CONFIG_FB_LCD_OVERLAY_SUPPORT
	/* overlay */
	uint32_t  overlay_state;  /*0-closed, 1-configed, 2-started*/
	struct semaphore   overlay_lock;
#endif
};

static struct sprdfb_dispc_context dispc_ctx = {0};

void clk_force_disable(struct clk *clk);
extern void sprdfb_panel_suspend(struct sprdfb_device *dev);
extern void sprdfb_panel_resume(struct sprdfb_device *dev, bool from_deep_sleep);
extern void sprdfb_panel_before_refresh(struct sprdfb_device *dev);
extern void sprdfb_panel_after_refresh(struct sprdfb_device *dev);
extern void sprdfb_panel_invalidate(struct panel_spec *self);
extern void sprdfb_panel_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom);

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
static int overlay_start(struct sprdfb_device *dev, uint32_t layer_index);
static int overlay_close(struct sprdfb_device *dev);
#endif


static irqreturn_t dispc_isr(int irq, void *data)
{
	struct sprdfb_dispc_context *dispc_ctx = (struct sprdfb_dispc_context *)data;
	uint32_t reg_val;
	struct sprdfb_device *dev = dispc_ctx->dev;
	bool done = false;

	reg_val = dispc_read(DISPC_INT_STATUS);

	pr_debug("dispc_isr (0x%x)\n",reg_val );

	if(reg_val & 0x04){
		printk("Warning: dispc underflow (0x%x)!\n",reg_val);
		dispc_write(0x04, DISPC_INT_CLR);
	}

	if(NULL == dev){
		return IRQ_HANDLED;
	}

	if((reg_val & 0x10) && (SPRDFB_PANEL_IF_DPI ==  dev->panel_if_type)){/*dispc update done isr*/
#if 0
		if(dispc_ctx->is_first_frame){
			/*dpi register update with SW and VSync*/
			dispc_clear_bits(BIT(4), DISPC_DPI_CTRL);

			/* start refresh */
			dispc_set_bits((1 << 4), DISPC_CTRL);

			dispc_ctx->is_first_frame = false;
		}
#endif
		dispc_write(0x10, DISPC_INT_CLR);
		done = true;
	}else if ((reg_val & 0x1) && (SPRDFB_PANEL_IF_DPI !=  dev->panel_if_type)){ /* dispc done isr */
			dispc_write(1, DISPC_INT_CLR);
			dispc_ctx->is_first_frame = false;
			done = true;
	}

	if(done){
		dispc_ctx->vsync_done = 1;

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	if(SPRD_OVERLAY_STATUS_STARTED == dispc_ctx->overlay_state){
		overlay_close(dev);
	}
#endif

		if (dispc_ctx->vsync_waiter) {
			wake_up_interruptible_all(&(dispc_ctx->vsync_queue));
			dispc_ctx->vsync_waiter = 0;
		}
		sprdfb_panel_after_refresh(dev);
		pr_debug(KERN_INFO "sprdfb: [%s]: Done INT, reg_val = %d !\n", __FUNCTION__, reg_val);
	}

	return IRQ_HANDLED;
}


/* dispc soft reset */
static void dispc_reset(void)
{
	#define REG_AHB_SOFT_RST (AHB_SOFT_RST + SPRD_AHB_BASE)
	__raw_writel(__raw_readl(REG_AHB_SOFT_RST) | (1<<DISPC_SOFT_RST), REG_AHB_SOFT_RST);
	udelay(10);
	__raw_writel(__raw_readl(REG_AHB_SOFT_RST) & (~(1<<DISPC_SOFT_RST)), REG_AHB_SOFT_RST);
}

static inline void dispc_set_bg_color(uint32_t bg_color)
{
	dispc_write(bg_color, DISPC_BG_COLOR);
}

static inline void dispc_set_osd_ck(uint32_t ck_color)
{
	dispc_write(ck_color, DISPC_OSD_CK);
}

static void dispc_dithering_enable(bool enable)
{
	if(enable){
		dispc_set_bits(BIT(6), DISPC_CTRL);
	}else{
		dispc_clear_bits(BIT(6), DISPC_CTRL);
	}
}

static void dispc_set_exp_mode(uint16_t exp_mode)
{
	uint32_t reg_val = dispc_read(DISPC_CTRL);

	reg_val &= ~(0x3 << 16);
	reg_val |= (exp_mode << 16);
	dispc_write(reg_val, DISPC_CTRL);
}

static void dispc_module_enable(void)
{
	/*dispc module enable */
	dispc_write((1<<0), DISPC_CTRL);

	/*disable dispc INT*/
	dispc_write(0x0, DISPC_INT_EN);

	/* clear dispc INT */
	dispc_write(0x1F, DISPC_INT_CLR);
}

static inline int32_t  dispc_set_disp_size(struct fb_var_screeninfo *var)
{
	uint32_t reg_val;

	reg_val = (var->xres & 0xfff) | ((var->yres & 0xfff ) << 16);
	dispc_write(reg_val, DISPC_SIZE_XY);

	return 0;
}

static void dispc_layer_init(struct fb_var_screeninfo *var)
{
	uint32_t reg_val = 0;

//	dispc_clear_bits((1<<0),DISPC_IMG_CTRL);
	dispc_write(0x0, DISPC_IMG_CTRL);
	dispc_clear_bits((1<<0),DISPC_OSD_CTRL);

	/******************* OSD layer setting **********************/

	/*enable OSD layer*/
	reg_val |= (1 << 0);

	/*disable  color key */

	/* alpha mode select  - block alpha*/
	reg_val |= (1 << 2);

	/* data format */
	if (var->bits_per_pixel == 32) {
		/* ABGR */
		reg_val |= (3 << 4);
		/* rb switch */
		reg_val |= (1 << 15);
	} else {
		/* RGB565 */
		reg_val |= (5 << 4);
		/* B2B3B0B1 */
		reg_val |= (2 << 8);
	}

	dispc_write(reg_val, DISPC_OSD_CTRL);

	/* OSD layer alpha value */
	dispc_write(0xff, DISPC_OSD_ALPHA);

	/* OSD layer size */
	reg_val = ( var->xres & 0xfff) | (( var->yres & 0xfff ) << 16);
	dispc_write(reg_val, DISPC_OSD_SIZE_XY);

	/* OSD layer start position */
	dispc_write(0, DISPC_OSD_DISP_XY);

	/* OSD layer pitch */
	reg_val = ( var->xres & 0xfff) ;
	dispc_write(reg_val, DISPC_OSD_PITCH);

	/* OSD color_key value */
	dispc_set_osd_ck(0x0);

	/* DISPC workplane size */
	dispc_set_disp_size(var);
}

static void dispc_layer_update(struct fb_var_screeninfo *var)
{
	uint32_t reg_val = 0;

	/******************* OSD layer setting **********************/

	/*enable OSD layer*/
	reg_val |= (1 << 0);

	/*disable  color key */

	/* alpha mode select  - block alpha*/
	reg_val |= (1 << 2);

	/* data format */
	if (var->bits_per_pixel == 32) {
		/* ABGR */
		reg_val |= (3 << 4);
		/* rb switch */
		reg_val |= (1 << 15);
	} else {
		/* RGB565 */
		reg_val |= (5 << 4);
		/* B2B3B0B1 */
		reg_val |= (2 << 8);
	}

	dispc_write(reg_val, DISPC_OSD_CTRL);
}

static int32_t dispc_sync(struct sprdfb_device *dev)
{
	int ret;

	if (dev->enable == 0) {
		printk("sprdfb: dispc_sync fb suspeneded already!!\n");
		return -1;
	}

	ret = wait_event_interruptible_timeout(dispc_ctx.vsync_queue,
			          dispc_ctx.vsync_done, msecs_to_jiffies(100));

	if (!ret) { /* time out */
		dispc_ctx.vsync_done = 1; /*error recovery */
		printk(KERN_ERR "sprdfb: dispc_sync time out!!!!!\n");
		{/*for debug*/
			int32_t i;
			for(i=0;i<256;i+=16){
				printk("sprdfb: %x: 0x%x, 0x%x, 0x%x, 0x%x\n", i, dispc_read(i), dispc_read(i+4), dispc_read(i+8), dispc_read(i+12));
			}
			printk("**************************************\n");
		}

		return -1;
	}
	return 0;
}


static void dispc_run(struct sprdfb_device *dev)
{
	if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
		if(!dispc_ctx.is_first_frame){
			dispc_ctx.vsync_done = 0;
			dispc_ctx.vsync_waiter ++;
		}

		/*dpi register update*/
		dispc_set_bits(BIT(5), DISPC_DPI_CTRL);

		udelay(30);

		if(dispc_ctx.is_first_frame){
			/*dpi register update with SW and VSync*/
			dispc_clear_bits(BIT(4), DISPC_DPI_CTRL);

			/* start refresh */
			dispc_set_bits((1 << 4), DISPC_CTRL);

			dispc_ctx.is_first_frame = false;
		}else{
			dispc_sync(dev);
		}
	}else{
		/* start refresh */
		dispc_set_bits((1 << 4), DISPC_CTRL);
	}
}

static void dispc_stop(struct sprdfb_device *dev)
{
	if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
		/*dpi register update with SW only*/
		dispc_set_bits(BIT(4), DISPC_DPI_CTRL);

		/* stop refresh */
		dispc_clear_bits((1 << 4), DISPC_CTRL);

		dispc_ctx.is_first_frame = true;
	}
}

static int32_t sprdfb_dispc_early_init(struct sprdfb_device *dev)
{
	int ret = 0;
	struct clk *clk_parent1, *clk_parent2, *clk_parent3;

	pr_debug(KERN_INFO "sprdfb:[%s]\n", __FUNCTION__);

	if(dispc_ctx.is_inited){
		printk(KERN_WARNING "sprdfb: dispc early init warning!(has been inited)");
		return 0;
	}

	/*usesd to open dipsc matix clock*/
	__raw_writel((__raw_readl(REG_AHB_MATRIX_CLOCK)) | (1<<DISPC_CORE_CLK_EN) | (1<<DISPMTX_CLK_EN), 
			REG_AHB_MATRIX_CLOCK);

	clk_parent1 = clk_get(NULL, DISPC_CLOCK_PARENT);
	if (IS_ERR(clk_parent1)) {
		printk(KERN_WARNING "sprdfb: get clk_parent1 fail!\n");
		return 0;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_parent1 ok!\n");
	}

	clk_parent2 = clk_get(NULL, DISPC_DBI_CLOCK_PARENT);
	if (IS_ERR(clk_parent2)) {
		printk(KERN_WARNING "sprdfb: get clk_parent2 fail!\n");
		return 0;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_parent2 ok!\n");
	}

	clk_parent3 = clk_get(NULL, DISPC_DPI_CLOCK_PARENT);
	if (IS_ERR(clk_parent3)) {
		printk(KERN_WARNING "sprdfb: get clk_parent3 fail!\n");
		return 0;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_parent3 ok!\n");
	}

	dispc_ctx.clk_dispc = clk_get(NULL, "clk_dispc");
	if (IS_ERR(dispc_ctx.clk_dispc)) {
		printk(KERN_WARNING "sprdfb: get clk_dispc fail!\n");
		return 0;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_dispc ok!\n");
	}

	dispc_ctx.clk_dispc_dbi = clk_get(NULL, "clk_dispc_dbi");
	if (IS_ERR(dispc_ctx.clk_dispc_dbi)) {
		printk(KERN_WARNING "sprdfb: get clk_dispc_dbi fail!\n");
		return 0;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_dispc_dbi ok!\n");
	}

	dispc_ctx.clk_dispc_dpi = clk_get(NULL, "clk_dispc_dpi");
	if (IS_ERR(dispc_ctx.clk_dispc_dpi)) {
		printk(KERN_WARNING "sprdfb: get clk_dispc_dpi fail!\n");
		return 0;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_dispc_dpi ok!\n");
	}

	ret = clk_set_parent(dispc_ctx.clk_dispc, clk_parent1);
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set clk parent fail\n");
	}
	ret = clk_set_rate(dispc_ctx.clk_dispc, DISPC_CLOCK);
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set clk parent fail\n");
	}

	ret = clk_set_parent(dispc_ctx.clk_dispc_dbi, clk_parent2);
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set dbi clk parent fail\n");
	}
	ret = clk_set_rate(dispc_ctx.clk_dispc_dbi, DISPC_DBI_CLOCK);
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set dbi clk parent fail\n");
	}

	ret = clk_set_parent(dispc_ctx.clk_dispc_dpi, clk_parent3);
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set dpi clk parent fail\n");
	}
	ret = clk_set_rate(dispc_ctx.clk_dispc_dpi, DISPC_DPI_CLOCK);
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set dpi clk parent fail\n");
	}

	ret = clk_enable(dispc_ctx.clk_dispc);
	if (ret) {
		printk(KERN_WARNING "sprdfb: enable clk_dispc fail!\n");
		return 0;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_dispc ok!\n");
	}

	ret = clk_enable(dispc_ctx.clk_dispc_dbi);
	if (ret) {
		printk(KERN_WARNING "sprdfb: enable clk_dispc_dbi fail!\n");
		clk_disable(dispc_ctx.clk_dispc);
		return 0;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_dispc_dbi ok!\n");
	}

	ret = clk_enable(dispc_ctx.clk_dispc_dpi);
	if (ret) {
		printk(KERN_WARNING "sprdfb: enable clk_dispc_dpi fail!\n");
		clk_disable(dispc_ctx.clk_dispc);
		clk_disable(dispc_ctx.clk_dispc_dbi);
		return 0;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_dispc_dpi ok!\n");
	}

	printk("0x20900200 = 0x%x\n", __raw_readl(SPRD_AHB_BASE + 0x200));
	printk("0x20900208 = 0x%x\n", __raw_readl(SPRD_AHB_BASE + 0x208));
	printk("0x20900220 = 0x%x\n", __raw_readl(SPRD_AHB_BASE + 0x220));

	if(!dev->panel_ready){
		dispc_reset();
		dispc_module_enable();
		dispc_ctx.is_first_frame = true;
	}else{
		dispc_ctx.is_first_frame = false;
	}

	dispc_ctx.vsync_done = 1;
	dispc_ctx.vsync_waiter = 0;
	init_waitqueue_head(&(dispc_ctx.vsync_queue));

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	sema_init(&dispc_ctx.overlay_lock, 1);
#endif

	dispc_ctx.is_inited = true;

	ret = request_irq(IRQ_DISPC_INT, dispc_isr, IRQF_DISABLED, "DISPC", &dispc_ctx);
	if (ret) {
		printk(KERN_ERR "sprdfb: dispcfailed to request irq!\n");
		clk_disable(dispc_ctx.clk_dispc);
		clk_disable(dispc_ctx.clk_dispc_dbi);
		clk_disable(dispc_ctx.clk_dispc_dpi);
		dispc_ctx.is_inited = false;
		return -1;
	}

	return 0;
}


static int32_t sprdfb_dispc_init(struct sprdfb_device *dev)
{
	pr_debug(KERN_INFO "sprdfb:[%s]\n",__FUNCTION__);
	/*set bg color*/
	dispc_set_bg_color(0xFFFFFFFF);
	/*enable dithering*/
	dispc_dithering_enable(true);
	/*use MSBs as img exp mode*/
	dispc_set_exp_mode(0x0);

	if(dispc_ctx.is_first_frame){
		dispc_layer_init(&(dev->fb->var));
	}else{
		dispc_layer_update(&(dev->fb->var));
	}

	if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
		if(dispc_ctx.is_first_frame){
			/*set dpi register update only with SW*/
			dispc_set_bits(BIT(4), DISPC_DPI_CTRL);
		}else{
			/*set dpi register update with SW & VSYNC*/
			dispc_clear_bits(BIT(4), DISPC_DPI_CTRL);
		}
		/*enable dispc update done INT*/
		dispc_write((1<<4), DISPC_INT_EN);
	}else{
		/* enable dispc DONE  INT*/
		dispc_write((1<<0), DISPC_INT_EN);
	}
	dispc_set_bits(BIT(2), DISPC_INT_EN);
	dev->enable = 1;
	return 0;
}

static int32_t sprdfb_dispc_uninit(struct sprdfb_device *dev)
{
	pr_debug(KERN_INFO "sprdfb:[%s]\n",__FUNCTION__);

	dev->enable = 0;
	if(dispc_ctx.is_inited){
		clk_disable(dispc_ctx.clk_dispc);
		clk_disable(dispc_ctx.clk_dispc_dpi);
		clk_disable(dispc_ctx.clk_dispc_dbi);
		dispc_ctx.is_inited = false;
	}
	return 0;
}

static int32_t sprdfb_dispc_refresh (struct sprdfb_device *dev)
{
	struct fb_info *fb = dev->fb;

	uint32_t base = fb->fix.smem_start + fb->fix.line_length * fb->var.yoffset;

	pr_debug(KERN_INFO "sprdfb:[%s]\n",__FUNCTION__);

	if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
		dispc_ctx.vsync_waiter ++;
		dispc_sync(dev);
		dispc_ctx.vsync_done = 0;
	}

	pr_debug(KERN_INFO "srpdfb: [%s] got sync\n", __FUNCTION__);

	dispc_ctx.dev = dev;

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	down(&dispc_ctx.overlay_lock);
#endif

#ifdef LCD_UPDATE_PARTLY
	if ((fb->var.reserved[0] == 0x6f766572) &&(SPRDFB_PANEL_IF_DPI != dev->panel_if_type)) {
		uint32_t x,y, width, height;

		x = fb->var.reserved[1] & 0xffff;
		y = fb->var.reserved[1] >> 16;
		width  = fb->var.reserved[2] &  0xffff;
		height = fb->var.reserved[2] >> 16;

		base += ((x + y * fb->var.xres) * fb->var.bits_per_pixel / 8);
		dispc_write(base, DISPC_OSD_BASE_ADDR);
		dispc_write(0, DISPC_OSD_DISP_XY);
		dispc_write(fb->var.reserved[2], DISPC_OSD_SIZE_XY);
		dispc_write(fb->var.xres, DISPC_OSD_PITCH);

		dispc_write(fb->var.reserved[2], DISPC_SIZE_XY);

		sprdfb_panel_invalidate_rect(dev->panel,
					x, y, x+width-1, y+height-1);
	} else
#endif
	{
		uint32_t size = (fb->var.xres & 0xffff) | ((fb->var.yres) << 16);

		dispc_write(base, DISPC_OSD_BASE_ADDR);
		dispc_write(0, DISPC_OSD_DISP_XY);
		dispc_write(size, DISPC_OSD_SIZE_XY);
		dispc_write(fb->var.xres, DISPC_OSD_PITCH);

		dispc_write(size, DISPC_SIZE_XY);

		if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
			sprdfb_panel_invalidate(dev->panel);
		}
	}

	sprdfb_panel_before_refresh(dev);

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	dispc_set_bits(BIT(0), DISPC_OSD_CTRL);
	if(SPRD_OVERLAY_STATUS_ON == dispc_ctx.overlay_state){
		overlay_start(dev, (SPRD_LAYER_IMG));
	}
#endif

	dispc_run(dev);

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	up(&dispc_ctx.overlay_lock);
#endif

	pr_debug("DISPC_CTRL: 0x%x\n", dispc_read(DISPC_CTRL));
	pr_debug("DISPC_SIZE_XY: 0x%x\n", dispc_read(DISPC_SIZE_XY));

	pr_debug("DISPC_BG_COLOR: 0x%x\n", dispc_read(DISPC_BG_COLOR));

	pr_debug("DISPC_INT_EN: 0x%x\n", dispc_read(DISPC_INT_EN));

	pr_debug("DISPC_OSD_CTRL: 0x%x\n", dispc_read(DISPC_OSD_CTRL));
	pr_debug("DISPC_OSD_BASE_ADDR: 0x%x\n", dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("DISPC_OSD_SIZE_XY: 0x%x\n", dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("DISPC_OSD_PITCH: 0x%x\n", dispc_read(DISPC_OSD_PITCH));
	pr_debug("DISPC_OSD_DISP_XY: 0x%x\n", dispc_read(DISPC_OSD_DISP_XY));
	pr_debug("DISPC_OSD_ALPHA	: 0x%x\n", dispc_read(DISPC_OSD_ALPHA));
	return 0;
}

static int32_t sprdfb_dispc_suspend(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb:[%s], dev->enable = %d\n",__FUNCTION__, dev->enable);

	if (0 != dev->enable){
		if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
			/* must wait ,dispc_sync() */
			dispc_ctx.vsync_waiter ++;
			dispc_sync(dev);
			printk(KERN_INFO "sprdfb:[%s] got sync\n",__FUNCTION__);
		}

		sprdfb_panel_suspend(dev);

		dispc_stop(dev);

		mdelay(50); /*fps>20*/

		dev->enable = 0;
		clk_disable(dispc_ctx.clk_dispc);
		clk_disable(dispc_ctx.clk_dispc_dpi);
		clk_disable(dispc_ctx.clk_dispc_dbi);
	}else{
		printk(KERN_ERR "sprdfb: [%s]: Invalid device status %d\n", __FUNCTION__, dev->enable);
	}
	return 0;
}

static int32_t sprdfb_dispc_resume(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb:[%s], dev->enable= %d\n",__FUNCTION__, dev->enable);

	if (dev->enable == 0) {
		clk_enable(dispc_ctx.clk_dispc);
		clk_enable(dispc_ctx.clk_dispc_dpi);
		clk_enable(dispc_ctx.clk_dispc_dbi);
		dispc_ctx.vsync_done = 1;
		if (dispc_read(DISPC_SIZE_XY) == dispc_read(DISPC_CTRL)) { /* resume from deep sleep */
			printk(KERN_INFO "sprdfb:[%s], dev->enable= %d\n",__FUNCTION__, dev->enable);
			dispc_reset();
			dispc_module_enable();
			dispc_ctx.is_first_frame = true;
			sprdfb_dispc_init(dev);

			sprdfb_panel_resume(dev, true);
		} else {
			printk(KERN_INFO "sprdfb:[%s]  not from deep sleep\n",__FUNCTION__);

			sprdfb_panel_resume(dev, true);
		}

		dev->enable = 1;
	}
	printk(KERN_INFO "sprdfb:[%s], leave dev->enable= %d\n",__FUNCTION__, dev->enable);

	return 0;
}


#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
static int overlay_open(void)
{
	printk("sprdfb: [%s] : %d\n", __FUNCTION__,dispc_ctx.overlay_state);

/*
	if(SPRD_OVERLAY_STATUS_OFF  != dispc_ctx.overlay_state){
		printk(KERN_ERR "sprdfb: Overlay open fail (has been opened)");
		return -1;
	}
*/

	dispc_ctx.overlay_state = SPRD_OVERLAY_STATUS_ON;
	return 0;
}

static int overlay_start(struct sprdfb_device *dev, uint32_t layer_index)
{
	printk("sprdfb: [%s] : %d, %d\n", __FUNCTION__,dispc_ctx.overlay_state, layer_index);


	if(SPRD_OVERLAY_STATUS_ON  != dispc_ctx.overlay_state){
		printk(KERN_ERR "sprdfb: overlay start fail. (not opened)");
		return -1;
	}

	if((0 == dispc_read(DISPC_IMG_Y_BASE_ADDR)) && (0 == dispc_read(DISPC_OSD_BASE_ADDR))){
		printk(KERN_ERR "sprdfb: overlay start fail. (not configged)");
		return -1;
	}

/*
	if(0 != dispc_sync(dev)){
		printk(KERN_ERR "sprdfb: overlay start fail. (wait done fail)");
		return -1;
	}
*/
	dispc_set_bg_color(0x0);
	dispc_clear_bits(BIT(2), DISPC_OSD_CTRL); /*use pixel alpha*/
	dispc_write(0x80, DISPC_OSD_ALPHA);

	if((layer_index & SPRD_LAYER_IMG) && (0 != dispc_read(DISPC_IMG_Y_BASE_ADDR))){
		dispc_set_bits(BIT(0), DISPC_IMG_CTRL);/* enable the image layer */
	}
	if((layer_index & SPRD_LAYER_OSD) && (0 != dispc_read(DISPC_OSD_BASE_ADDR))){
		dispc_set_bits(BIT(0), DISPC_OSD_CTRL);/* enable the osd layer */
	}
	dispc_ctx.overlay_state = SPRD_OVERLAY_STATUS_STARTED;
	return 0;
}

static int overlay_img_configure(struct sprdfb_device *dev, int type, overlay_rect *rect, unsigned char *buffer, int y_endian, int uv_endian, bool rb_switch)
{
	uint32_t reg_value;

	printk("sprdfb: [%s] : %d, (%d, %d,%d,%d), 0x%x\n", __FUNCTION__, type, rect->x, rect->y, rect->h, rect->w, (unsigned int)buffer);


	if(SPRD_OVERLAY_STATUS_ON  != dispc_ctx.overlay_state){
		printk(KERN_ERR "sprdfb: Overlay config fail (not opened)");
		return -1;
	}

	if (type >= SPRD_DATA_TYPE_LIMIT) {
		printk(KERN_ERR "sprdfb: Overlay config fail (type error)");
		return -1;
	}

	if((y_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT) || (uv_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT)){
		printk(KERN_ERR "sprdfb: Overlay config fail (y, uv endian error)");
		return -1;
	}

/*	lcdc_write(((type << 3) | (1 << 0)), LCDC_IMG_CTRL); */
	/*lcdc_write((type << 3) , LCDC_IMG_CTRL);*/
	reg_value = (y_endian << 8)|(uv_endian<< 10)|(type << 4);
	if(rb_switch){
		reg_value |= (1 << 15);
	}
	dispc_write(reg_value, DISPC_IMG_CTRL);

	dispc_write((uint32_t)buffer, DISPC_IMG_Y_BASE_ADDR);
	if (type < SPRD_DATA_TYPE_RGB888) {
		uint32_t size = rect->w * rect->h;
		dispc_write((uint32_t)(buffer + size), DISPC_IMG_UV_BASE_ADDR);
	}

	reg_value = (rect->h << 16) | (rect->w);
	dispc_write(reg_value, DISPC_IMG_SIZE_XY);

	dispc_write(rect->w, DISPC_IMG_PITCH);

	reg_value = (rect->y << 16) | (rect->x);
	dispc_write(reg_value, DISPC_IMG_DISP_XY);

	if(type < SPRD_DATA_TYPE_RGB888) {
		dispc_write(1, DISPC_Y2R_CTRL);
		dispc_write(64, DISPC_Y2R_CONTRAST);
		dispc_write(64, DISPC_Y2R_SATURATION);
		dispc_write(0, DISPC_Y2R_BRIGHTNESS);
	}

	pr_debug("DISPC_IMG_CTRL: 0x%x\n", dispc_read(DISPC_IMG_CTRL));
	pr_debug("DISPC_IMG_Y_BASE_ADDR: 0x%x\n", dispc_read(DISPC_IMG_Y_BASE_ADDR));
	pr_debug("DISPC_IMG_UV_BASE_ADDR: 0x%x\n", dispc_read(DISPC_IMG_UV_BASE_ADDR));
	pr_debug("DISPC_IMG_SIZE_XY: 0x%x\n", dispc_read(DISPC_IMG_SIZE_XY));
	pr_debug("DISPC_IMG_PITCH: 0x%x\n", dispc_read(DISPC_IMG_PITCH));
	pr_debug("DISPC_IMG_DISP_XY: 0x%x\n", dispc_read(DISPC_IMG_DISP_XY));
	pr_debug("DISPC_Y2R_CTRL: 0x%x\n", dispc_read(DISPC_Y2R_CTRL));
	pr_debug("DISPC_Y2R_CONTRAST: 0x%x\n", dispc_read(DISPC_Y2R_CONTRAST));
	pr_debug("DISPC_Y2R_SATURATION: 0x%x\n", dispc_read(DISPC_Y2R_SATURATION));
	pr_debug("DISPC_Y2R_BRIGHTNESS: 0x%x\n", dispc_read(DISPC_Y2R_BRIGHTNESS));

	return 0;
}

static int overlay_osd_configure(struct sprdfb_device *dev, int type, overlay_rect *rect, unsigned char *buffer, int y_endian, int uv_endian, bool rb_switch)
{
	uint32_t reg_value;

	printk("sprdfb: [%s] : %d, (%d, %d,%d,%d), 0x%x\n", __FUNCTION__, type, rect->x, rect->y, rect->h, rect->w, (unsigned int)buffer);


	if(SPRD_OVERLAY_STATUS_ON  != dispc_ctx.overlay_state){
		printk(KERN_ERR "sprdfb: Overlay config fail (not opened)");
		return -1;
	}

	if ((type >= SPRD_DATA_TYPE_LIMIT) || (type <= SPRD_DATA_TYPE_YUV400)) {
		printk(KERN_ERR "sprdfb: Overlay config fail (type error)");
		return -1;
	}

	if(y_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT ){
		printk(KERN_ERR "sprdfb: Overlay config fail (rgb endian error)");
		return -1;
	}

/*	lcdc_write(((type << 3) | (1 << 0)), LCDC_IMG_CTRL); */
	/*lcdc_write((type << 3) , LCDC_IMG_CTRL);*/

	/*use premultiply pixel alpha*/
	reg_value = (y_endian<<8)|(type << 4|(1<<2))|(2<<16);
	if(rb_switch){
		reg_value |= (1 << 15);
	}
	dispc_write(reg_value, DISPC_OSD_CTRL);

	dispc_write((uint32_t)buffer, DISPC_OSD_BASE_ADDR);

	reg_value = (rect->h << 16) | (rect->w);
	dispc_write(reg_value, DISPC_OSD_SIZE_XY);

	dispc_write(rect->w, DISPC_OSD_PITCH);

	reg_value = (rect->y << 16) | (rect->x);
	dispc_write(reg_value, DISPC_OSD_DISP_XY);


	pr_debug("DISPC_OSD_CTRL: 0x%x\n", dispc_read(DISPC_OSD_CTRL));
	pr_debug("DISPC_OSD_BASE_ADDR: 0x%x\n", dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("DISPC_OSD_SIZE_XY: 0x%x\n", dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("DISPC_OSD_PITCH: 0x%x\n", dispc_read(DISPC_OSD_PITCH));
	pr_debug("DISPC_OSD_DISP_XY: 0x%x\n", dispc_read(DISPC_OSD_DISP_XY));

	return 0;
}

static int overlay_close(struct sprdfb_device *dev)
{
	if(SPRD_OVERLAY_STATUS_OFF  == dispc_ctx.overlay_state){
		printk(KERN_ERR "sprdfb: overlay close fail. (has been closed)");
		return 0;
	}

/*
	if (0 != sprd_lcdc_sync(dev)) {
		printk(KERN_ERR "sprdfb: overlay close fail. (wait done fail)\n");
		return -1;
	}
*/
	dispc_set_bg_color(0xFFFFFFFF);
	dispc_set_bits(BIT(2), DISPC_OSD_CTRL);/*use block alpha*/
	dispc_write(0xff, DISPC_OSD_ALPHA);
	dispc_clear_bits(BIT(0), DISPC_IMG_CTRL);	/* disable the image layer */
	dispc_write(0, DISPC_IMG_Y_BASE_ADDR);
	dispc_write(0, DISPC_OSD_BASE_ADDR);
	dispc_layer_init(&(dev->fb->var));
	dispc_ctx.overlay_state = SPRD_OVERLAY_STATUS_OFF;

	return 0;
}

/*TO DO: need mutext with suspend, resume*/
static int32_t sprdfb_dispc_enable_overlay(struct sprdfb_device *dev, struct overlay_info* info, int enable)
{
	int result = -1;

	if(0 == dev->enable){
		printk(KERN_ERR "sprdfb: sprdfb_dispc_enable_overlay fail. (dev not enable)\n");
		return -1;
	}

	printk("sprdfb: [%s]: %d, %d\n", __FUNCTION__, enable,  dev->enable);

	if(enable){  /*enable*/
		if(NULL == info){
			printk(KERN_ERR "sprdfb: sprdfb_dispc_enable_overlay fail (Invalid parameter)\n");
			return -1;
		}

		down(&dispc_ctx.overlay_lock);

		if(0 != dispc_sync(dev)){
			printk(KERN_ERR "sprdfb: sprdfb_dispc_enable_overlay fail. (wait done fail)\n");
			up(&dispc_ctx.overlay_lock);
			return -1;
		}

		result = overlay_open();
		if(0 != result){
			up(&dispc_ctx.overlay_lock);
			return -1;
		}

		if(SPRD_LAYER_IMG == info->layer_index){
			result = overlay_img_configure(dev, info->data_type, &(info->rect), info->buffer, info->y_endian, info->uv_endian, info->rb_switch);
		}else if(SPRD_LAYER_OSD == info->layer_index){
			result = overlay_osd_configure(dev, info->data_type, &(info->rect), info->buffer, info->y_endian, info->uv_endian, info->rb_switch);
		}else{
			printk(KERN_ERR "sprdfb: sprdfb_dispc_enable_overlay fail. (invalid layer index)\n");
		}
		if(0 != result){
			up(&dispc_ctx.overlay_lock);
			return -1;
		}

		up(&dispc_ctx.overlay_lock);

		/*result = overlay_start(dev);*/
	}else{   /*disable*/
		/*result = overlay_close(dev);*/
	}

	printk("sprdfb: [%s] return %d\n", __FUNCTION__, result);
	return result;
}


static int32_t sprdfb_dispc_display_overlay(struct sprdfb_device *dev, struct overlay_display* setting)
{
	struct overlay_rect* rect = &(setting->rect);
	uint32_t size =( (rect->h << 16) | (rect->w & 0xffff));

	dispc_ctx.dev = dev;

	printk("sprdfb: sprdfb_dispc_display_overlay: layer:%d, (%d, %d,%d,%d)\n",
		setting->layer_index, setting->rect.x, setting->rect.y, setting->rect.h, setting->rect.w);

	down(&dispc_ctx.overlay_lock);

	if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
		dispc_ctx.vsync_waiter ++;
		dispc_sync(dev);
		dispc_ctx.vsync_done = 0;
	}

	printk(KERN_INFO "srpdfb: [%s] got sync\n", __FUNCTION__);

	dispc_ctx.dev = dev;

#ifdef LCD_UPDATE_PARTLY
	if ((setting->rect->h < dev->panel->height) ||
		(setting->rect->w < dev->panel->width)){
		dispc_write(size, DISPC_SIZE_XY);

		sprdfb_panel_invalidate_rect(dev->panel,
					rect->x, rect->y, rect->x + rect->w-1, rect->y + rect->h-1);
	} else
#endif
	{
		dispc_write(size, DISPC_SIZE_XY);

		if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
			sprdfb_panel_invalidate(dev->panel);
		}
	}

	sprdfb_panel_before_refresh(dev);

	dispc_clear_bits(BIT(0), DISPC_OSD_CTRL);
	if(SPRD_OVERLAY_STATUS_ON == dispc_ctx.overlay_state){
		overlay_start(dev, setting->layer_index);
	}


	dispc_run(dev);

	if((SPRD_OVERLAY_DISPLAY_SYNC == setting->display_mode) && (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)){
		dispc_ctx.vsync_waiter ++;
		if (dispc_sync(dev) != 0) {/* time out??? disable ?? */
			printk("sprdfb  do sprd_lcdc_display_overlay  time out!\n");
		}
		dispc_ctx.vsync_done = 0;
	}

	up(&dispc_ctx.overlay_lock);

	pr_debug("DISPC_CTRL: 0x%x\n", dispc_read(DISPC_CTRL));
	pr_debug("DISPC_SIZE_XY: 0x%x\n", dispc_read(DISPC_SIZE_XY));

	pr_debug("DISPC_BG_COLOR: 0x%x\n", dispc_read(DISPC_BG_COLOR));

	pr_debug("DISPC_INT_EN: 0x%x\n", dispc_read(DISPC_INT_EN));

	pr_debug("DISPC_OSD_CTRL: 0x%x\n", dispc_read(DISPC_OSD_CTRL));
	pr_debug("DISPC_OSD_BASE_ADDR: 0x%x\n", dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("DISPC_OSD_SIZE_XY: 0x%x\n", dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("DISPC_OSD_PITCH: 0x%x\n", dispc_read(DISPC_OSD_PITCH));
	pr_debug("DISPC_OSD_DISP_XY: 0x%x\n", dispc_read(DISPC_OSD_DISP_XY));
	pr_debug("DISPC_OSD_ALPHA	: 0x%x\n", dispc_read(DISPC_OSD_ALPHA));
	return 0;
}

#endif

struct display_ctrl sprdfb_dispc_ctrl = {
	.name		= "dispc",
	.early_init		= sprdfb_dispc_early_init,
	.init		 	= sprdfb_dispc_init,
	.uninit		= sprdfb_dispc_uninit,
	.refresh		= sprdfb_dispc_refresh,
	.suspend		= sprdfb_dispc_suspend,
	.resume		= sprdfb_dispc_resume,
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	.enable_overlay = sprdfb_dispc_enable_overlay,
	.display_overlay = sprdfb_dispc_display_overlay,
#endif
};



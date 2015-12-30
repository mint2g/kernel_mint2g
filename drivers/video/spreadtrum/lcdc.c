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
#include <mach/pinmap.h>

#include "sprdfb.h"

extern void lcdc_dithering_enable(void);

#define SPRDFB_CONTRAST (74)
#define SPRDFB_SATURATION (73)
#define SPRDFB_BRIGHTNESS (2)

struct sprd_lcd_controller {
	/* only one device can work one time */
	struct sprdfb_device  *dev;

	struct clk		*clk_lcdc;

	wait_queue_head_t       vsync_queue;
	uint32_t	        vsync_done;

#ifdef  CONFIG_FB_LCD_OVERLAY_SUPPORT
	/* overlay */
	uint32_t  overlay_state;  /*0-closed, 1-configed, 2-started*/
	struct semaphore   overlay_lock;
#endif
	#if 0
	spinlock_t 		my_lock;
	#endif
};

static struct sprd_lcd_controller lcdc;

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
static int overlay_start(struct sprdfb_device *dev, uint32_t layer_index);
static int overlay_close(struct sprdfb_device *dev);
#endif

typedef struct {
	uint32_t reg;
	uint32_t val;
} lcd_pinmap_t;

lcd_pinmap_t lcd_rstpin_map[] = {
	{REG_PIN_LCD_RSTN, BITS_PIN_DS(1)|BITS_PIN_AF(0)|BIT_PIN_NUL|BIT_PIN_SLP_WPU|BIT_PIN_SLP_Z},
	{REG_PIN_LCD_RSTN, BITS_PIN_DS(3)|BITS_PIN_AF(3)|BIT_PIN_NUL|BIT_PIN_NUL|BIT_PIN_SLP_OE},
};

static void sprd_lcdc_set_rstn_prop(unsigned int if_slp)
{
	int i;

	if (if_slp)
		i = 0;
	else
		i = 1;

	__raw_writel(lcd_rstpin_map[i].val, CTL_PIN_BASE + lcd_rstpin_map[i].reg);
}

/* lcdc soft reset */
static void sprd_lcdc_reset(void)
{
	#define REG_AHB_SOFT_RST (AHB_SOFT_RST + SPRD_AHB_BASE)
	__raw_writel(__raw_readl(REG_AHB_SOFT_RST) | (1<<3), REG_AHB_SOFT_RST);
	udelay(10);
	__raw_writel(__raw_readl(REG_AHB_SOFT_RST) & (~(1<<3)), REG_AHB_SOFT_RST);

}

static irqreturn_t lcdc_isr(int irq, void *data)
{
	uint32_t val;
        struct sprd_lcd_controller *lcdc = (struct sprd_lcd_controller *)data;
	struct sprdfb_device *dev = lcdc->dev;

	val = lcdc_read(LCDC_IRQ_STATUS);

	if (val & 1) { /* lcdc done isr */
		lcdc_write(1, LCDC_IRQ_CLR);

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	if(SPRD_OVERLAY_STATUS_STARTED == lcdc->overlay_state){
		overlay_close(dev);
	}
#endif


		if (dev->ctrl->set_timing) {
			dev->ctrl->set_timing(dev,LCD_REGISTER_TIMING);
		}

		lcdc->vsync_done = 1;
		if (dev->vsync_waiter) {
			wake_up_interruptible_all(&(lcdc->vsync_queue));
			dev->vsync_waiter = 0;
		}
		pr_debug(KERN_INFO "lcdc_done_isr !\n");

	}

	return IRQ_HANDLED;
}

static inline int32_t set_lcdsize(struct fb_var_screeninfo *var)
{
	uint32_t reg_val;

	reg_val = (var->xres & 0xfff) | ((var->yres & 0xfff ) << 16);
	lcdc_write(reg_val, LCDC_DISP_SIZE);

	return 0;
}

static inline int32_t set_lcmrect(struct fb_var_screeninfo *var)
{
	uint32_t reg_val;

	lcdc_write(0, LCDC_LCM_START);

	reg_val = (var->xres & 0xfff) | ((var->yres & 0xfff ) << 16);
	lcdc_write(reg_val, LCDC_LCM_SIZE);

	return 0;
}

static void lcdc_layer_init(struct fb_var_screeninfo *var)
{
	uint32_t reg_val = 0;

	/******************* OSD1 layer setting **********************/

	lcdc_clear_bits((1<<0),LCDC_IMG_CTRL);
	lcdc_clear_bits((1<<0),LCDC_OSD2_CTRL);
	lcdc_clear_bits((1<<0),LCDC_OSD3_CTRL);
	lcdc_clear_bits((1<<0),LCDC_OSD4_CTRL);
	lcdc_clear_bits((1<<0),LCDC_OSD5_CTRL);
	/*enable OSD1 layer*/
	reg_val |= (1 << 0);

	/* color key */

	/* alpha mode select */
	reg_val |= (1 << 2);

	/* data format */
	if (var->bits_per_pixel == 32) {
		/* ABGR */
		reg_val |= (3 << 3);
		/* rb switch */
	 	reg_val |= (1 << 9);

	} else {
		/* RGB565 */
		reg_val |= (5 << 3);
		/* B2B3B0B1 */
		reg_val |= (2 << 7);
	}

	lcdc_write(reg_val, LCDC_OSD1_CTRL);

	/* OSD1 layer alpha value */
	lcdc_write(0xff, LCDC_OSD1_ALPHA);

	/* alpha base addr */

	/* OSD1 layer size */
	reg_val = ( var->xres & 0xfff) | (( var->yres & 0xfff ) << 16);

	lcdc_write(reg_val, LCDC_OSD1_SIZE_XY);

	/* OSD1 layer start position */
	lcdc_write(0, LCDC_OSD1_DISP_XY);

	/* OSD1 layer pitch */
	reg_val = ( var->xres & 0xfff) ;
	lcdc_write(reg_val, LCDC_OSD1_PITCH);

	/* OSD1 color_key value */

	/* OSD1 grey RGB */

	/* LCDC workplane size */
	set_lcdsize(var);

	/*LCDC LCM rect size */
	set_lcmrect(var);
}

static void lcdc_hw_init(void)
{
	uint32_t reg_val;

	/* LCDC module enable */
	reg_val = (1<<0);

	/* FMARK mode */
	#if CONFIG_FB_LCD_NOFMARK
	reg_val |= (1<<1);
	#endif

	/* FMARK pol */
	reg_val |= (1<<2);

	lcdc_write(reg_val, LCDC_CTRL);

	/* set background */
	lcdc_write(0xFFFFFF, LCDC_BG_COLOR);

	/* clear lcdc IRQ */
	lcdc_set_bits((1<<0), LCDC_IRQ_CLR);
}

static int32_t sprd_lcdc_early_init(void)
{
	int ret = 0;
	lcdc.clk_lcdc = clk_get(NULL, "clk_lcdc");
	clk_enable(lcdc.clk_lcdc);

	sprd_lcdc_reset();
	lcdc_hw_init();

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	sema_init(&lcdc.overlay_lock, 1);
#endif

	lcdc.vsync_done = 1;
	init_waitqueue_head(&(lcdc.vsync_queue));
	ret = request_irq(IRQ_LCDC_INT, lcdc_isr, IRQF_DISABLED, "LCDC", &lcdc);
	if (ret) {
		printk(KERN_ERR "lcdc: failed to request irq!\n");
		return -1;
	}
	return 0;
}

static int32_t sprd_lcdc_init(struct sprdfb_device *dev)
{
	lcdc_layer_init(&(dev->fb->var));

	/* enable lcdc IRQ */
	lcdc_write((1<<0), LCDC_IRQ_EN);
	dev->enable = 1;
	return 0;
}

static int32_t sprd_lcdc_uninit(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb:[%s]\n",__FUNCTION__);
	dev->enable = 0;
	clk_disable(lcdc.clk_lcdc);
	return 0;
}

static int32_t sprd_lcdc_cleanup(struct sprdfb_device *dev)
{
	return 0;
}

static int32_t sprd_lcdc_enable_plane(struct sprdfb_device *dev, int enable)
{
	if (lcdc.dev == NULL ) {
		lcdc.dev = dev;
	}
	if (enable) {
		if (dev->enable !=0) {
			return 0;
		} else {
			dev->init_panel(dev);
			dev->enable = 1;
		}
	} else {
		if (dev->enable !=0) {
			dev->enable = 0;
		}
	}
	return 0;
}

static int32_t sprd_lcdc_refresh (struct sprdfb_device *dev)
{
	struct fb_info *fb = dev->fb;

	uint32_t base = fb->fix.smem_start + fb->fix.line_length * fb->var.yoffset;

	lcdc.dev = dev;

	pr_debug("fb->var.yoffset: 0x%x\n", fb->var.yoffset);

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	down(&lcdc.overlay_lock);
#endif

	lcdc.vsync_done = 0;

#ifdef LCD_UPDATE_PARTLY
	if (fb->var.reserved[0] == 0x6f766572) {
		uint32_t x,y, width, height;

		x = fb->var.reserved[1] & 0xffff;
		y = fb->var.reserved[1] >> 16;
   		width  = fb->var.reserved[2] &  0xffff;
		height = fb->var.reserved[2] >> 16;

		base += ((x + y * fb->var.xres) * fb->var.bits_per_pixel / 8);
		lcdc_write(base, LCDC_OSD1_BASE_ADDR);
		lcdc_write(fb->var.reserved[2], LCDC_OSD1_SIZE_XY);

		lcdc_write(fb->var.reserved[2], LCDC_LCM_SIZE);
		lcdc_write(fb->var.reserved[2], LCDC_DISP_SIZE);

		dev->panel->ops->panel_invalidate_rect(dev->panel,
					left, top, left+width-1, top+height-1);
	} else
#endif
	{
		uint32_t size = (fb->var.xres & 0xffff) | ((fb->var.yres) << 16);

		lcdc_write(base, LCDC_OSD1_BASE_ADDR);
		lcdc_write(0, LCDC_OSD1_DISP_XY);
		lcdc_write(size,LCDC_OSD1_SIZE_XY);
		lcdc_write(fb->var.xres, LCDC_OSD1_PITCH);

		lcdc_write(size, LCDC_DISP_SIZE);

		dev->panel->ops->panel_invalidate(dev->panel);
	}

	if (dev->ctrl->set_timing) {
		dev->ctrl->set_timing(dev,LCD_GRAM_TIMING);
	}

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	lcdc_set_bits(BIT(0), LCDC_OSD1_CTRL);
	if(SPRD_OVERLAY_STATUS_ON == lcdc.overlay_state){
		overlay_start(dev, (SPRD_LAYER_IMG | SPRD_LAYER_OSD));
	}
#endif

	/* start refresh */
	lcdc_set_bits((1 << 3), LCDC_CTRL);

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	up(&lcdc.overlay_lock);
#endif
	pr_debug("LCDC_CTRL: 0x%x\n", lcdc_read(LCDC_CTRL));
	pr_debug("LCDC_DISP_SIZE: 0x%x\n", lcdc_read(LCDC_DISP_SIZE));
	pr_debug("LCDC_LCM_START: 0x%x\n", lcdc_read(LCDC_LCM_START));
	pr_debug("LCDC_LCM_SIZE: 0x%x\n", lcdc_read(LCDC_LCM_SIZE));
	pr_debug("LCDC_BG_COLOR: 0x%x\n", lcdc_read(LCDC_BG_COLOR));
	pr_debug("LCDC_FIFO_STATUS: 0x%x\n", lcdc_read(LCDC_FIFO_STATUS));

	pr_debug("LCM_CTRL: 0x%x\n", lcdc_read(LCM_CTRL));
	pr_debug("LCM_TIMING0: 0x%x\n", lcdc_read(LCM_TIMING0));
	pr_debug("LCM_RDDATA: 0x%x\n", lcdc_read(LCM_RDDATA));

	pr_debug("LCDC_IRQ_EN: 0x%x\n", lcdc_read(LCDC_IRQ_EN));

	pr_debug("LCDC_OSD1_CTRL: 0x%x\n", lcdc_read(LCDC_OSD1_CTRL));
	pr_debug("LCDC_OSD1_BASE_ADDR: 0x%x\n", lcdc_read(LCDC_OSD1_BASE_ADDR));
	pr_debug("LCDC_OSD1_ALPHA_BASE_ADDR: 0x%x\n", lcdc_read(LCDC_OSD1_ALPHA_BASE_ADDR));
	pr_debug("LCDC_OSD1_SIZE_XY: 0x%x\n", lcdc_read(LCDC_OSD1_SIZE_XY));
	pr_debug("LCDC_OSD1_PITCH: 0x%x\n", lcdc_read(LCDC_OSD1_PITCH));
	pr_debug("LCDC_OSD1_DISP_XY: 0x%x\n", lcdc_read(LCDC_OSD1_DISP_XY));
	pr_debug("LCDC_OSD1_ALPHA	: 0x%x\n", lcdc_read(LCDC_OSD1_ALPHA));

	return 0;
}
extern int lcd_check;
static int32_t sprd_lcdc_sync(struct sprdfb_device *dev)
{
	int ret;
	if(lcd_check==1){
	if (dev->enable == 0) {
		pr_debug("lcdc: sprd_lcdc_sync fb suspeneded already!!\n");
		return -1;
	}
	ret = wait_event_interruptible_timeout(lcdc.vsync_queue,
			          lcdc.vsync_done, msecs_to_jiffies(100));
	if (!ret) { /* time out */
		lcdc.vsync_done = 1; /*error recovery */
		printk(KERN_ERR "lcdc: sprd_lcdc_sync time out!!!!!\n");
		return -1;
	}
		}
	return 0;
}

static int32_t sprd_lcdc_suspend(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb:[%s]\n",__FUNCTION__);
	down(&dev->work_proceedure_lock);
	if (dev->enable != 0) {
		/* must wait ,sprd_lcdc_sync() */
		dev->vsync_waiter ++;
		dev->ctrl->sync(dev);

		/* let lcdc sleep in */
		if (dev->panel->ops->panel_enter_sleep != NULL) {
			dev->panel->ops->panel_enter_sleep(dev->panel,1);
		}

		sprd_lcdc_set_rstn_prop(1);		/*modify reset pin status  for lcdc reset pin  sleep power issue */

		dev->enable = 0;
		clk_disable(lcdc.clk_lcdc);
	}
	up(&dev->work_proceedure_lock);	
	return 0;
}

#if defined(CONFIG_FB_LCD_ILI9341_BOE_MINT)
extern int lcd_regulator_enable(int en);

void lcd_esd_recovery(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb:[%s]++++\n",__FUNCTION__);
	down(&dev->work_proceedure_lock);
		msleep(200);		
		lcd_regulator_enable(0);
		msleep(500);
		lcd_regulator_enable(1);
		msleep(200);		
		clk_enable(lcdc.clk_lcdc);
		lcdc.vsync_done = 1;

		msleep(200);	
		sprd_lcdc_reset();
		lcdc_hw_init();
		sprd_lcdc_init(dev);
		lcdc_dithering_enable(); /* dithering for deep sleep */

		dev->panel->ops->panel_reset(dev->panel);
		dev->init_panel(dev);
		dev->panel->ops->panel_init(dev->panel);

		dev->enable = 1;
		dev->vsync_waiter ++;
		dev->ctrl->sync(dev);	
		dev->ctrl->refresh(dev);
	up(&dev->work_proceedure_lock);
	printk(KERN_INFO "sprdfb:[%s]----\n",__FUNCTION__);	
}
EXPORT_SYMBOL(lcd_esd_recovery);
#endif

static int32_t sprd_lcdc_resume(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb:[%s]\n",__FUNCTION__);
	down(&dev->work_proceedure_lock);
	if (dev->enable == 0) {
		clk_enable(lcdc.clk_lcdc);
		lcdc.vsync_done = 1;

		sprd_lcdc_set_rstn_prop(0);		/*resume for lcdc reset pin  sleep power issue */

		if (lcdc_read(LCDC_CTRL) == 0) { /* resume from deep sleep */
			sprd_lcdc_reset();
			lcdc_hw_init();
			sprd_lcdc_init(dev);
			lcdc_dithering_enable(); /* dithering for deep sleep */

			dev->panel->ops->panel_reset(dev->panel);
			dev->init_panel(dev);
			dev->panel->ops->panel_init(dev->panel);
		} else {
			/* let lcd sleep out */
			dev->panel->ops->panel_enter_sleep(dev->panel,0);
		}

		dev->enable = 1;
	}
	up(&dev->work_proceedure_lock);	
	return 0;
}

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
static int overlay_open(void)
{
	pr_debug("lcdc: [%s] : %d\n", __FUNCTION__,lcdc.overlay_state);

/*
	if(SPRD_OVERLAY_STATUS_OFF  != lcdc.overlay_state){
		printk(KERN_ERR "sprd_fb: Overlay open fail (has been opened)");
		return -1;
	}
*/

	lcdc.overlay_state = SPRD_OVERLAY_STATUS_ON;
	return 0;
}

static int overlay_start(struct sprdfb_device *dev, uint32_t layer_index)
{
	pr_debug("lcdc: [%s] : %d, %d\n", __FUNCTION__,lcdc.overlay_state, layer_index);


	if(SPRD_OVERLAY_STATUS_ON  != lcdc.overlay_state){
		printk(KERN_ERR "sprd_fb: overlay start fail. (not opened)");
		return -1;
	}

	if((0 == lcdc_read(LCDC_IMG_Y_BASE_ADDR)) && (0 == lcdc_read(LCDC_OSD2_BASE_ADDR))){
		printk(KERN_ERR "sprd_fb: overlay start fail. (not configged)");
		return -1;
	}

/*
	if(0 != sprd_lcdc_sync(dev)){
		printk(KERN_ERR "sprd_fb: overlay start fail. (wait done fail)");
		return -1;
	}
*/
	lcdc_write(0x00000000, LCDC_BG_COLOR);
	lcdc_clear_bits(BIT(2), LCDC_OSD1_CTRL); /*use pixel alpha*/
	lcdc_write(0x80, LCDC_OSD1_ALPHA);
	lcdc_write(0x80, LCDC_OSD2_ALPHA);

	if((layer_index & SPRD_LAYER_IMG) && (0 != lcdc_read(LCDC_IMG_Y_BASE_ADDR))){
		lcdc_set_bits(BIT(0), LCDC_IMG_CTRL);/* enable the image layer */
	}
	if((layer_index & SPRD_LAYER_OSD) && (0 != lcdc_read(LCDC_OSD2_BASE_ADDR))){
		lcdc_set_bits(BIT(0), LCDC_OSD2_CTRL);/* enable the osd2 layer */
	}
	lcdc.overlay_state = SPRD_OVERLAY_STATUS_STARTED;
	return 0;
}

static int overlay_img_configure(struct sprdfb_device *dev, int type, overlay_rect *rect, unsigned char *buffer, int y_endian, int uv_endian, bool rb_switch)
{
	uint32_t reg_value;

	pr_debug("lcdc: [%s] : %d, (%d, %d,%d,%d), 0x%x\n", __FUNCTION__, type, rect->x, rect->y, rect->h, rect->w, (unsigned int)buffer);


	if(SPRD_OVERLAY_STATUS_ON  != lcdc.overlay_state){
		printk(KERN_ERR "sprd_fb: Overlay config fail (not opened)");
		return -1;
	}

	if (type >= SPRD_DATA_TYPE_LIMIT) {
		printk(KERN_ERR "sprd_fb: Overlay config fail (type error)");
		return -1;
	}

	if((y_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT) || (uv_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT)){
		printk(KERN_ERR "sprd_fb: Overlay config fail (y, uv endian error)");
		return -1;
	}

/*	lcdc_write(((type << 3) | (1 << 0)), LCDC_IMG_CTRL); */
	/*lcdc_write((type << 3) , LCDC_IMG_CTRL);*/
	reg_value = (y_endian << 7)|(uv_endian<< 10)|(type << 3);
	if(rb_switch){
		reg_value |= (1 << 9);
	}
	lcdc_write(reg_value, LCDC_IMG_CTRL);

	lcdc_write((uint32_t)buffer, LCDC_IMG_Y_BASE_ADDR);
	if (type < SPRD_DATA_TYPE_RGB888) {
		uint32_t size = rect->w * rect->h;
		lcdc_write((uint32_t)(buffer + size), LCDC_IMG_UV_BASE_ADDR);
	}

	reg_value = (rect->h << 16) | (rect->w);
	lcdc_write(reg_value, LCDC_IMG_SIZE_XY);

	lcdc_write(rect->w, LCDC_IMG_PITCH);

	reg_value = (rect->y << 16) | (rect->x);
	lcdc_write(reg_value, LCDC_IMG_DISP_XY);

	if(type < SPRD_DATA_TYPE_RGB888) {
		lcdc_write(1, LCDC_Y2R_CTRL);
		lcdc_write(SPRDFB_CONTRAST, LCDC_Y2R_CONTRAST);
		lcdc_write(SPRDFB_SATURATION, LCDC_Y2R_SATURATION);
		lcdc_write(SPRDFB_BRIGHTNESS, LCDC_Y2R_BRIGHTNESS);
	}

	pr_debug("LCDC_IMG_CTRL: 0x%x\n", lcdc_read(LCDC_IMG_CTRL));
	pr_debug("LCDC_IMG_Y_BASE_ADDR: 0x%x\n", lcdc_read(LCDC_IMG_Y_BASE_ADDR));
	pr_debug("LCDC_IMG_UV_BASE_ADDR: 0x%x\n", lcdc_read(LCDC_IMG_UV_BASE_ADDR));
	pr_debug("LCDC_IMG_SIZE_XY: 0x%x\n", lcdc_read(LCDC_IMG_SIZE_XY));
	pr_debug("LCDC_IMG_PITCH: 0x%x\n", lcdc_read(LCDC_IMG_PITCH));
	pr_debug("LCDC_IMG_DISP_XY: 0x%x\n", lcdc_read(LCDC_IMG_DISP_XY));
	pr_debug("LCDC_Y2R_CTRL: 0x%x\n", lcdc_read(LCDC_Y2R_CTRL));
	pr_debug("LCDC_Y2R_CONTRAST: 0x%x\n", lcdc_read(LCDC_Y2R_CONTRAST));
	pr_debug("LCDC_Y2R_SATURATION: 0x%x\n", lcdc_read(LCDC_Y2R_SATURATION));
	pr_debug("LCDC_Y2R_BRIGHTNESS: 0x%x\n", lcdc_read(LCDC_Y2R_BRIGHTNESS));

	return 0;
}

static int overlay_osd_configure(struct sprdfb_device *dev, int type, overlay_rect *rect, unsigned char *buffer, int y_endian, int uv_endian, bool rb_switch)
{
	uint32_t reg_value;

	pr_debug("lcdc: [%s] : %d, (%d, %d,%d,%d), 0x%x\n", __FUNCTION__, type, rect->x, rect->y, rect->h, rect->w, (unsigned int)buffer);


	if(SPRD_OVERLAY_STATUS_ON  != lcdc.overlay_state){
		printk(KERN_ERR "sprd_fb: Overlay config fail (not opened)");
		return -1;
	}

	if ((type >= SPRD_DATA_TYPE_LIMIT) || (type <= SPRD_DATA_TYPE_YUV400)) {
		printk(KERN_ERR "sprd_fb: Overlay config fail (type error)");
		return -1;
	}

	if(y_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT ){
		printk(KERN_ERR "sprd_fb: Overlay config fail (rgb endian error)");
		return -1;
	}

/*	lcdc_write(((type << 3) | (1 << 0)), LCDC_IMG_CTRL); */
	/*lcdc_write((type << 3) , LCDC_IMG_CTRL);*/

	reg_value = (y_endian<<7)|(type << 3);
	if(rb_switch){
		reg_value |= (1 << 9);
	}
	lcdc_write(reg_value, LCDC_OSD2_CTRL);

	lcdc_write((uint32_t)buffer, LCDC_OSD2_BASE_ADDR);

	reg_value = (rect->h << 16) | (rect->w);
	lcdc_write(reg_value, LCDC_OSD2_SIZE_XY);

	lcdc_write(rect->w, LCDC_OSD2_PITCH);

	reg_value = (rect->y << 16) | (rect->x);
	lcdc_write(reg_value, LCDC_OSD2_DISP_XY);


	pr_debug("LCDC_OSD2_CTRL: 0x%x\n", lcdc_read(LCDC_OSD2_CTRL));
	pr_debug("LCDC_OSD2_BASE_ADDR: 0x%x\n", lcdc_read(LCDC_OSD2_BASE_ADDR));
	pr_debug("LCDC_OSD2_SIZE_XY: 0x%x\n", lcdc_read(LCDC_OSD2_SIZE_XY));
	pr_debug("LCDC_OSD2_PITCH: 0x%x\n", lcdc_read(LCDC_OSD2_PITCH));
	pr_debug("LCDC_OSD2_DISP_XY: 0x%x\n", lcdc_read(LCDC_OSD2_DISP_XY));

	return 0;
}

static int overlay_close(struct sprdfb_device *dev)
{
	if(SPRD_OVERLAY_STATUS_OFF  == lcdc.overlay_state){
		printk(KERN_ERR "sprd_fb: overlay close fail. (has been closed)");
		return 0;
	}

/*
	if (0 != sprd_lcdc_sync(dev)) {
		printk(KERN_ERR "sprd_fb: overlay close fail. (wait done fail)\n");
		return -1;
	}
*/
	lcdc_write(0xFFFFFF, LCDC_BG_COLOR);
	lcdc_set_bits(BIT(2), LCDC_OSD1_CTRL);
	lcdc_write(0xff, LCDC_OSD1_ALPHA);
	lcdc_clear_bits(BIT(0), LCDC_IMG_CTRL);	/* disable the image layer */
	lcdc_clear_bits(BIT(0), LCDC_OSD2_CTRL);
	lcdc_write(0, LCDC_IMG_Y_BASE_ADDR);
	lcdc_write(0, LCDC_OSD2_BASE_ADDR);
	lcdc.overlay_state = SPRD_OVERLAY_STATUS_OFF;

	return 0;
}

/*TO DO: need mutext with suspend, resume*/
static int32_t sprd_lcdc_enable_overlay(struct sprdfb_device *dev, struct overlay_info* info, int enable)
{
	int result = -1;

	if(0 == dev->enable){
		printk(KERN_ERR "sprdfb: sprd_lcdc_enable_overlay fail. (dev not enable)\n");
		return -1;
	}

	pr_debug("lcdc: [%s]: %d, %d\n", __FUNCTION__, enable,  dev->enable);

	if(enable){  /*enable*/
		if(NULL == info){
			printk(KERN_ERR "sprdfb: sprd_lcdc_enable_overlay fail (Invalid parameter)\n");
			return -1;
		}

		down(&lcdc.overlay_lock);

		if(0 != sprd_lcdc_sync(dev)){
			printk(KERN_ERR "sprd_fb: sprd_lcdc_enable_overlay fail. (wait done fail)\n");
			up(&lcdc.overlay_lock);
			return -1;
		}

		result = overlay_open();
		if(0 != result){
			up(&lcdc.overlay_lock);
			return -1;
		}

		if(SPRD_LAYER_IMG == info->layer_index){
			result = overlay_img_configure(dev, info->data_type, &(info->rect), info->buffer, info->y_endian, info->uv_endian, info->rb_switch);
		}else if(SPRD_LAYER_OSD == info->layer_index){
			result = overlay_osd_configure(dev, info->data_type, &(info->rect), info->buffer, info->y_endian, info->uv_endian, info->rb_switch);
		}else{
			printk(KERN_ERR "sprd_fb: sprd_lcdc_enable_overlay fail. (invalid layer index)\n");
		}
		if(0 != result){
			up(&lcdc.overlay_lock);
			return -1;
		}

		up(&lcdc.overlay_lock);

		/*result = overlay_start(dev);*/
	}else{   /*disable*/
		/*result = overlay_close(dev);*/
	}

	pr_debug("lcdc: [%s] return %d\n", __FUNCTION__, result);
	return result;
}


static int32_t sprd_lcdc_display_overlay(struct sprdfb_device *dev, struct overlay_display* setting)
{
	struct overlay_rect* rect = &(setting->rect);
	uint32_t size =( (rect->h << 16) | (rect->w & 0xffff));

	lcdc.dev = dev;

	pr_debug("sprd_lcdc_display_overlay: layer:%d, (%d, %d,%d,%d)\n",
		setting->layer_index, setting->rect.x, setting->rect.y, setting->rect.h, setting->rect.w);

	down(&lcdc.overlay_lock);

	dev->vsync_waiter ++;
	if (dev->ctrl->sync(dev) != 0) {/* time out??? disable ?? */
		dev->vsync_waiter = 0;
		/* dev->pending_addr = 0; */
		printk("sprdfb can not do sprd_lcdc_display_overlay !!!!\n");
		return 0;
	}

	lcdc.vsync_done = 0;


#ifdef LCD_UPDATE_PARTLY
	if ((setting->rect->h < dev->panel->height) ||
		(setting->rect->w < dev->panel->width)){
		lcdc_write(size, LCDC_LCM_SIZE);
		lcdc_write(size, LCDC_DISP_SIZE);

		dev->panel->ops->panel_invalidate_rect(dev->panel,
					rect->x, rect->y, rect->x + rect->w-1, rect->y + rect->h-1);
	} else
#endif
	{
		lcdc_write(size, LCDC_DISP_SIZE);

		dev->panel->ops->panel_invalidate(dev->panel);
	}

	if (dev->ctrl->set_timing) {
		dev->ctrl->set_timing(dev,LCD_GRAM_TIMING);
	}

	if(SPRD_OVERLAY_STATUS_ON == lcdc.overlay_state){
		overlay_start(dev, setting->layer_index);
	}

	lcdc_clear_bits(BIT(0), LCDC_OSD1_CTRL);

	/* start refresh */
	lcdc_set_bits((1 << 3), LCDC_CTRL);

	if(SPRD_OVERLAY_DISPLAY_SYNC == setting->display_mode){
		dev->vsync_waiter ++;
		if (dev->ctrl->sync(dev) != 0) {/* time out??? disable ?? */
			dev->vsync_waiter = 0;
			printk("sprdfb  do sprd_lcdc_display_overlay  time out!\n");
		}
	}

	up(&lcdc.overlay_lock);

	pr_debug("LCDC_CTRL: 0x%x\n", lcdc_read(LCDC_CTRL));
	pr_debug("LCDC_DISP_SIZE: 0x%x\n", lcdc_read(LCDC_DISP_SIZE));
	pr_debug("LCDC_LCM_START: 0x%x\n", lcdc_read(LCDC_LCM_START));
	pr_debug("LCDC_LCM_SIZE: 0x%x\n", lcdc_read(LCDC_LCM_SIZE));
	pr_debug("LCDC_BG_COLOR: 0x%x\n", lcdc_read(LCDC_BG_COLOR));
	pr_debug("LCDC_FIFO_STATUS: 0x%x\n", lcdc_read(LCDC_FIFO_STATUS));

	pr_debug("LCM_CTRL: 0x%x\n", lcdc_read(LCM_CTRL));
	pr_debug("LCM_TIMING0: 0x%x\n", lcdc_read(LCM_TIMING0));
	pr_debug("LCM_RDDATA: 0x%x\n", lcdc_read(LCM_RDDATA));

	pr_debug("LCDC_IRQ_EN: 0x%x\n", lcdc_read(LCDC_IRQ_EN));
	return 0;
}

#endif


struct panel_ctrl sprd_lcdc_ctrl = {
	.name			= "lcdc",
	.early_init		= sprd_lcdc_early_init,
	.init		 	= sprd_lcdc_init,
	.cleanup		= sprd_lcdc_cleanup,
	.enable_plane           = sprd_lcdc_enable_plane,
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	.enable_overlay = sprd_lcdc_enable_overlay,
	.display_overlay = sprd_lcdc_display_overlay,
#endif
	.refresh	        = sprd_lcdc_refresh,
	.sync                   = sprd_lcdc_sync,
	.suspend		= sprd_lcdc_suspend,
	.resume			= sprd_lcdc_resume,
	.uninit		= sprd_lcdc_uninit,
};




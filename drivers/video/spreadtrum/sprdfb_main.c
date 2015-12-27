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
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/console.h>
#include "sprdfb.h"
#include "lcdpanel.h"
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <mach/globalregs.h>
#include <linux/gpio.h>
#include <mach/pinmap.h>
#include <linux/semaphore.h>

#if defined (CONFIG_SEC_DEBUG)
#include <mach/sec_debug.h>
#endif


#ifdef	CONFIG_TRIPLE_FRAMEBUFFER
#define FRAMEBUFFER_NR		3
#else
#define FRAMEBUFFER_NR		2
#endif

// LCD_ESD
static struct workqueue_struct *lcd_wq;
int lcd_esd;

#define GPIO_LCD_ESD_DET 90
// LCD_ESD
static int lcd_is_dummy = 0;
extern int sprd_probe_panel(struct sprdfb_device *dev, int cs);
extern int lcd_id;
extern struct panel_ctrl sprd_lcdc_ctrl;
struct panel_ctrl *sprd_ctrl[] =
{
	&sprd_lcdc_ctrl,   /*  lcdc (MCU) */
};

static int sprdfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *fb)
{
	int32_t ret;
	struct sprdfb_device *dev = fb->par;

	/* dev->pending_addr = fb->fix.smem_start + fb->fix.line_length * var->yoffset; */
	pr_debug("sprdfb: [%s]\n", __FUNCTION__);

	if (lcd_is_dummy) return 0;

	/* wait for vsync done */
	dev->vsync_waiter ++;
	if (dev->ctrl->sync(dev) != 0) {/* time out??? disable ?? */
		dev->vsync_waiter = 0;
		/* dev->pending_addr = 0; */
		pr_debug("sprdfb can not do pan_display !!!!\n");
		return 0;
	}

	/* TODO: set pending address and do refreshing */
	/* dev->pending_addr = 0; */
	ret = dev->ctrl->refresh(dev);
	if (ret) {
		printk(KERN_ERR "sprdfb failed to refresh !!!!\n");
	}

	return 0;
}

static int sprdfb_check_var(struct fb_var_screeninfo *var, struct fb_info *fb)
{
	if ((var->xres != fb->var.xres) ||
		(var->yres != fb->var.yres) ||
		(var->xres_virtual != fb->var.xres_virtual) ||
		(var->yres_virtual != fb->var.yres_virtual) ||
		(var->xoffset != fb->var.xoffset) ||
		(var->bits_per_pixel != fb->var.bits_per_pixel) ||
		(var->grayscale != fb->var.grayscale))
			return -EINVAL;
	return 0;
}

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
#include <video/sprd_fb.h>
static int sprdfb_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	int result = 0;
	struct sprdfb_device *dev = NULL;

	pr_debug("sprd_fb: [%s]: %d\n", __FUNCTION__, cmd);

	if(NULL == info){
		printk(KERN_ERR "sprd_fb: sprdfb_ioctl error. (Invalid Parameter)");
		return -1;
	}

	dev = info->par;

	switch(cmd){
	case SPRD_FB_SET_OVERLAY:
		result = dev->ctrl->enable_overlay(dev, (overlay_info*)arg, 1);
		break;
	case SPRD_FB_DISPLAY_OVERLAY:
		result = dev->ctrl->display_overlay(dev, (overlay_display*)arg);
		break;
	default:
		break;
	}

	pr_debug("sprd_fb: [%s]: return %d\n",__FUNCTION__, result);
	return result;
}

#endif


static struct fb_ops sprdfb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = sprdfb_check_var,
	.fb_pan_display = sprdfb_pan_display,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	.fb_ioctl = sprdfb_ioctl,
#endif
};

static int setup_fb_mem(struct sprdfb_device *dev, struct platform_device *pdev)
{
	uint32_t len, addr;

	printk("\n%s %d %d %d\n", __func__, dev->panel->width, dev->panel->height, dev->bpp);

	len = dev->panel->width * dev->panel->height * (dev->bpp / 8) * FRAMEBUFFER_NR;
	addr = __get_free_pages(GFP_ATOMIC | __GFP_ZERO, get_order(len));
	if (!addr) {
		printk(KERN_ERR "Failed to allocate framebuffer memory\n");
		return -ENOMEM;
	}
	pr_debug("sprdfb got %d bytes mem at 0x%x\n", len, addr);

	dev->fb->fix.smem_start = __pa(addr);
	dev->fb->fix.smem_len = len;
	dev->fb->screen_base = (char*)addr;

#ifdef SPRDFB_TEST
	{
		int32_t w, h;
		uint32_t *base = (uint32_t *)addr;
		for (h = 0 ; h < dev->panel->height; h ++ ) {
			for (w = 0; w < dev->panel->width; w++) {
				*base ++ = 0xFF00FF00;
			}
		}
		for (h = 0 ; h < dev->panel->height; h ++ ) {
			for (w = 0; w < dev->panel->width; w++) {
				*base ++ = 0xFF0000FF;
			}
		}
	}
#endif
	return 0;
}

static unsigned PP[16];
static void setup_fb_info(struct sprdfb_device *dev)
{
	struct fb_info *fb = dev->fb;
	struct panel_spec *panel = dev->panel;
	int r;

	fb->fbops = &sprdfb_ops;
	fb->flags = FBINFO_DEFAULT;

	/* finish setting up the fb_info struct */
	strncpy(fb->fix.id, "sc8810fb", 16);
	fb->fix.ypanstep = 1;
	fb->fix.type = FB_TYPE_PACKED_PIXELS;
	fb->fix.visual = FB_VISUAL_TRUECOLOR;
	fb->fix.line_length = panel->width * dev->bpp / 8;

	fb->var.xres = panel->width;
	fb->var.yres = panel->height;
	fb->var.width = panel->width;
	fb->var.height = panel->height;
	fb->var.xres_virtual = panel->width;
	fb->var.yres_virtual = panel->height * FRAMEBUFFER_NR;
	fb->var.bits_per_pixel = dev->bpp;
	fb->var.pixclock = 45000; /* fake pixel clock to avoid divide 0 */
	fb->var.accel_flags = 0;
	fb->var.yoffset = 0;

	/* only support two pixel format */
	if (dev->bpp == 32) { /* ABGR */
		fb->var.red.offset     = 24;
		fb->var.red.length     = 8;
		fb->var.red.msb_right  = 0;
		fb->var.green.offset   = 16;
		fb->var.green.length   = 8;
		fb->var.green.msb_right = 0;
		fb->var.blue.offset    = 8;
		fb->var.blue.length    = 8;
		fb->var.blue.msb_right = 0;
	} else {
		fb->var.red.offset     = 11;
		fb->var.red.length     = 5;
		fb->var.red.msb_right  = 0;
		fb->var.green.offset   = 5;
		fb->var.green.length   = 6;
		fb->var.green.msb_right = 0;
		fb->var.blue.offset    = 0;
		fb->var.blue.length    = 5;
		fb->var.blue.msb_right = 0;
	}
	r = fb_alloc_cmap(&fb->cmap, 16, 0);
	fb->pseudo_palette = PP;

	PP[0] = 0;
	for (r = 1; r < 16; r++)
		PP[r] = 0xffffffff;

#if defined(CONFIG_SEC_DEBUG)
	// Mark for GetLog
	sec_getlog_supply_fbinfo(fb->fix.smem_start, fb->var.xres, fb->var.yres, fb->var.bits_per_pixel,2);
#endif
}

static void fb_free_resources(struct sprdfb_device *dev)
{
	if (dev == NULL)
		return;

	if (dev->panel != NULL && dev->panel->ops->panel_close != NULL) {
		dev->panel->ops->panel_close(dev->panel);
	}

	if (&dev->fb->cmap != NULL) {
		fb_dealloc_cmap(&dev->fb->cmap);
	}
	if (dev->fb->screen_base) {
		free_pages((unsigned long)dev->fb->screen_base,
				get_order(dev->fb->fix.smem_len));
	}
	unregister_framebuffer(dev->fb);
	framebuffer_release(dev->fb);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sprdfb_early_suspend (struct early_suspend* es)
{
	struct sprdfb_device *dev = container_of(es, struct sprdfb_device, early_suspend);
	struct fb_info *fb = dev->fb;
	
	#if defined(CONFIG_FB_LCD_ILI9341_BOE_MINT)
	disable_irq_nosync(lcd_esd);
	#endif
	
	if (lcd_is_dummy) return;

	fb_set_suspend(fb, FBINFO_STATE_SUSPENDED);

	if (!lock_fb_info(fb)) {
		return ;
	}
	dev->ctrl->suspend(dev);
	unlock_fb_info(fb);
}

static void sprdfb_late_resume (struct early_suspend* es)
{
	struct sprdfb_device *dev = container_of(es, struct sprdfb_device, early_suspend);
	struct fb_info *fb = dev->fb;

	if (lcd_is_dummy) return;

	if (!lock_fb_info(fb)) {
		return ;
	}
	dev->ctrl->resume(dev);
	unlock_fb_info(fb);

	fb_set_suspend(fb, FBINFO_STATE_RUNNING);

	#if defined(CONFIG_FB_LCD_ILI9341_BOE_MINT)	
	enable_irq(lcd_esd);	
	#endif
}
#endif

static int sprdfb_suspend(struct platform_device *pdev,pm_message_t state)
{
	struct sprdfb_device *dev = platform_get_drvdata(pdev);
	printk("sprdfb: [%s]\n",__FUNCTION__);

	if (lcd_is_dummy) return 0;
	/* TODO: suspend entries */
	dev->ctrl->suspend(dev);

	return 0;
}

static int sprdfb_resume(struct platform_device *pdev)
{
	struct sprdfb_device *dev = platform_get_drvdata(pdev);

	if (lcd_is_dummy) return 0;

	/* TODO: resume entries */
	dev->ctrl->resume(dev);
	return 0;
}


#ifdef SPRDFB_TEST
static int sprdfb_test_thread(void * data)
{
	uint32_t *addr;
	struct fb_info *fb = (struct fb_info *)data;
	struct fb_var_screeninfo *fb_var = &fb->var;

	while (1)
	{
		if (fb_var->yoffset == 0) {
			fb_var->yoffset = fb->var.yres;
		} else {
			fb_var->yoffset = 0;
		}

		sprdfb_pan_display(&fb_var, fb);
		msleep(1000);
	}
}

#endif

#if defined(CONFIG_FB_LCD_ILI9341_BOE_MINT)
extern void lcd_esd_recovery(struct sprdfb_device *dev);

static void lcd_esd_detect(struct work_struct *work)
{
	struct sprdfb_device *pdata = container_of(work, struct sprdfb_device, work);

	printk("[LCD] lcd_esd_detect_enter\n");

	lcd_esd_recovery(pdata);
	msleep(200);	
	enable_irq(lcd_esd);
}


static irqreturn_t lcd_esd_irq_handler(int irq, void *dev_id)
{
	struct sprdfb_device *pdata = (struct sprdfb_device *)dev_id;
	printk("[LCD] %s, %d\n", __func__, __LINE__ );
	
	disable_irq_nosync(lcd_esd);

	queue_work(lcd_wq, &pdata->work);

	return IRQ_HANDLED;
}

int lcd_check =1;
EXPORT_SYMBOL(lcd_check);
#endif

static int sprdfb_probe(struct platform_device *pdev)
{
	struct fb_info *fb;
	struct sprdfb_device *dev;
	int ret;

	printk("sprdfb initialize!\n");

	fb = framebuffer_alloc(sizeof(struct sprdfb_device), &pdev->dev);
	if (!fb) {
		ret = -ENOMEM;
		goto err0;
	}

	dev = fb->par;
	dev->fb = fb;

	/* use platform device id as CS number */
	dev->csid = pdev->id;

	/* support mcu only now */
	dev->mode = LCD_MODE_MCU;
	dev->ctrl = sprd_ctrl[0];
	dev->ctrl->early_init();

	/* TODO: do some early init here to probe panel */
	/* some actions can only done once if two panel is supported by CS0/1 */
	if (sprd_probe_panel(dev, dev->csid) == 1) {
		lcd_is_dummy = 1;
	}

	if((lcd_id==0x040404)||(lcd_id==0))
		lcd_check = 0;
	
	ret = setup_fb_mem(dev, pdev);

	if (ret) {
		goto cleanup;
	}

	setup_fb_info(dev);

	if (!lcd_is_dummy)
		dev->ctrl->init(dev);

	/* TODO: do more init after panel is attached */
	/* some actions can only done once if two panel is supported by CS0/1 */

	/* register framebuffer device */
	ret = register_framebuffer(fb);
	if (ret) {
		goto cleanup;
	}
	platform_set_drvdata(pdev, dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	dev->early_suspend.suspend = sprdfb_early_suspend;
	dev->early_suspend.resume  = sprdfb_late_resume;
	dev->early_suspend.level   = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&dev->early_suspend);
#endif

#ifdef SPRDFB_TEST
	kernel_thread(sprdfb_test_thread, fb, 0);;
#endif
	sema_init(&dev->work_proceedure_lock, 1);

	#if defined(CONFIG_FB_LCD_ILI9341_BOE_MINT)
	// LCD_ESD
	if(lcd_check==1){
	lcd_wq = create_workqueue("lcd_wq");
	
	INIT_WORK(&dev->work, lcd_esd_detect);
	
	lcd_esd = gpio_to_irq(GPIO_LCD_ESD_DET);

	ret = request_irq(lcd_esd, lcd_esd_irq_handler,/* IRQF_TRIGGER_RISING |*/ IRQF_TRIGGER_LOW , "lcdc", dev);
	}
	// LCD_ESD
	#endif
	return 0;

cleanup:
	fb_free_resources(dev);
err0:
	dev_err(&pdev->dev, "failed to probe sprdfb\n");
	return ret;
}

static void __devexit sprdfb_remove(struct platform_device *pdev)
{
	/* TODO: removed entries */
}

static struct platform_driver sprdfb_driver = {
	.probe = sprdfb_probe,

#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = sprdfb_suspend,
	.resume = sprdfb_resume,
#endif
	.remove = __devexit_p(sprdfb_remove),
	.driver = {
		.name = "sprd_fb",
		.owner = THIS_MODULE,
	},
};

static int __init sprdfb_init(void)
{
	return platform_driver_register(&sprdfb_driver);
}

static void __exit sprdfb_exit(void)
{
	return platform_driver_unregister(&sprdfb_driver);
}

module_init(sprdfb_init);
module_exit(sprdfb_exit);

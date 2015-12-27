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
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/fb.h>

#include "sprdfb.h"
#include "sprdfb_panel.h"

enum{
	SPRD_IN_DATA_TYPE_ABGR888 = 0,
	SPRD_IN_DATA_TYPE_BGR565,
/*
	SPRD_IN_DATA_TYPE_RGB666,
	SPRD_IN_DATA_TYPE_RGB555,
	SPRD_IN_DATA_TYPE_PACKET,
*/ /*not support*/
	SPRD_IN_DATA_TYPE_LIMIT
};

#define SPRDFB_IN_DATA_TYPE SPRD_IN_DATA_TYPE_ABGR888

#ifdef CONFIG_TRIPLE_FRAMEBUFFER
#define FRAMEBUFFER_NR		(3)
#else
#define FRAMEBUFFER_NR		(2)
#endif

#define SPRDFB_DEFAULT_FPS (60)

extern bool sprdfb_panel_get(struct sprdfb_device *dev);
extern int sprdfb_panel_probe(struct sprdfb_device *dev);
extern void sprdfb_panel_remove(struct sprdfb_device *dev);

extern struct display_ctrl sprdfb_dispc_ctrl ;
extern struct display_ctrl sprdfb_lcdc_ctrl;

static unsigned PP[16];

static int sprdfb_check_var(struct fb_var_screeninfo *var, struct fb_info *fb);
static int sprdfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *fb);
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
static int sprdfb_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg);
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

	len = dev->panel->width * dev->panel->height * (dev->bpp / 8) * FRAMEBUFFER_NR;
	addr = __get_free_pages(GFP_ATOMIC | __GFP_ZERO, get_order(len));
	if (!addr) {
		printk(KERN_ERR "sprdfb: Failed to allocate framebuffer memory\n");
		return -ENOMEM;
	}
	pr_debug(KERN_INFO "sprdfb:  got %d bytes mem at 0x%x\n", len, addr);

	dev->fb->fix.smem_start = __pa(addr);
	dev->fb->fix.smem_len = len;
	dev->fb->screen_base = (char*)addr;

	return 0;
}

static void setup_fb_info(struct sprdfb_device *dev)
{
	struct fb_info *fb = dev->fb;
	struct panel_spec *panel = dev->panel;
	int r;

	fb->fbops = &sprdfb_ops;
	fb->flags = FBINFO_DEFAULT;

	/* finish setting up the fb_info struct */
	strncpy(fb->fix.id, "tigerfb", 16);
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
	if(0 != dev->panel->fps){
		fb->var.pixclock = (1000 * 1000 * 1000) / (dev->panel->fps * panel->width * panel->height) * 1000;
	}else{
		fb->var.pixclock = (1000 * 1000 * 1000) / (SPRDFB_DEFAULT_FPS * panel->width * panel->height) * 1000;
	}
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
	} else { /*BGR*/
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
	for (r = 1; r < 16; r++){
		PP[r] = 0xffffffff;
	}
}

static void fb_free_resources(struct sprdfb_device *dev)
{
	if (dev == NULL)
		return;

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

static int sprdfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *fb)
{
	int32_t ret;
	struct sprdfb_device *dev = fb->par;

	pr_debug("sprdfb: [%s]\n", __FUNCTION__);

	if(0 == dev->enable){
		printk(KERN_ERR "sprdfb:[%s]: Invalid Device status %d", __FUNCTION__, dev->enable);
		return -1;
	}

	ret = dev->ctrl->refresh(dev);
	if (ret) {
		printk(KERN_ERR "sprdfb: failed to refresh !!!!\n");
		return -1;
	}

	return 0;
}

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
#include <video/sprd_fb.h>
static int sprdfb_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	int result = 0;
	struct sprdfb_device *dev = NULL;

	if(NULL == info){
		printk(KERN_ERR "sprdfb: sprdfb_ioctl error. (Invalid Parameter)");
		return -1;
	}

	dev = info->par;

	switch(cmd){
	case SPRD_FB_SET_OVERLAY:
		printk(KERN_INFO "sprdfb: [%s]: SPRD_FB_SET_OVERLAY\n", __FUNCTION__);
		if(NULL != dev->ctrl->enable_overlay){
			result = dev->ctrl->enable_overlay(dev, (overlay_info*)arg, 1);
		}
		break;
	case SPRD_FB_DISPLAY_OVERLAY:
		printk(KERN_INFO "sprdfb: [%s]: SPRD_FB_DISPLAY_OVERLAY\n", __FUNCTION__);
		if(NULL != dev->ctrl->display_overlay){
			result = dev->ctrl->display_overlay(dev, (overlay_display*)arg);
		}
		break;
	default:
		printk(KERN_INFO "sprdfb: [%s]: unknown cmd(%d)\n", __FUNCTION__, cmd);
		break;
	}

	printk(KERN_INFO "sprdfb: [%s]: return %d\n",__FUNCTION__, result);
	return result;
}

#endif

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


#ifdef CONFIG_HAS_EARLYSUSPEND
static void sprdfb_early_suspend (struct early_suspend* es)
{
	struct sprdfb_device *dev = container_of(es, struct sprdfb_device, early_suspend);
	struct fb_info *fb = dev->fb;
	printk("sprdfb: [%s]\n",__FUNCTION__);

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
	printk("sprdfb: [%s]\n",__FUNCTION__);

	if (!lock_fb_info(fb)) {
		return ;
	}
	dev->ctrl->resume(dev);
	unlock_fb_info(fb);

	fb_set_suspend(fb, FBINFO_STATE_RUNNING);
}

#else

static int sprdfb_suspend(struct platform_device *pdev,pm_message_t state)
{
	struct sprdfb_device *dev = platform_get_drvdata(pdev);
	printk("sprdfb: [%s]\n",__FUNCTION__);

	dev->ctrl->suspend(dev);

	return 0;
}

static int sprdfb_resume(struct platform_device *pdev)
{
	struct sprdfb_device *dev = platform_get_drvdata(pdev);
	printk("sprdfb: [%s]\n",__FUNCTION__);

	dev->ctrl->resume(dev);

	return 0;
}
#endif

static int sprdfb_probe(struct platform_device *pdev)
{
	struct fb_info *fb = NULL;
	struct sprdfb_device *dev = NULL;
	int ret = 0;

	pr_debug(KERN_INFO "sprdfb:[%s], id = %d\n", __FUNCTION__, pdev->id);

	fb = framebuffer_alloc(sizeof(struct sprdfb_device), &pdev->dev);
	if (!fb) {
		printk(KERN_ERR "sprdfb: sprdfb_probe allocate buffer fail.\n");
		ret = -ENOMEM;
		goto err0;
	}

	dev = fb->par;
	dev->fb = fb;

	dev->dev_id = pdev->id;
	if((SPRDFB_MAINLCD_ID != dev->dev_id) &&(SPRDFB_SUBLCD_ID != dev->dev_id)){
		printk(KERN_ERR "sprdfb: sprdfb_probe fail. (unsupported device id)\n");
		goto err0;
	}

	switch(SPRDFB_IN_DATA_TYPE){
	case SPRD_IN_DATA_TYPE_ABGR888:
		dev->bpp = 32;
		break;
	case SPRD_IN_DATA_TYPE_BGR565:
		dev->bpp = 16;
		break;
	default:
		dev->bpp = 32;
		break;
	}

	if(SPRDFB_MAINLCD_ID == dev->dev_id){
		dev->ctrl = &sprdfb_dispc_ctrl;
	}else{
		dev->ctrl = &sprdfb_lcdc_ctrl;
	}

	if(sprdfb_panel_get(dev)){
		dev->panel_ready = true;
	}else{
		dev->panel_ready = false;
	}

	dev->ctrl->early_init(dev);

	if(!dev->panel_ready){
		if (!sprdfb_panel_probe(dev)) {
			ret = -EIO;
			goto cleanup;
		}
	}

	ret = setup_fb_mem(dev, pdev);
	if (ret) {
		goto cleanup;
	}

	setup_fb_info(dev);

	dev->ctrl->init(dev);

	/* register framebuffer device */
	ret = register_framebuffer(fb);
	if (ret) {
		printk(KERN_ERR "sprdfb: sprdfb_probe register framebuffer fail.\n");
		goto cleanup;
	}
	platform_set_drvdata(pdev, dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	dev->early_suspend.suspend = sprdfb_early_suspend;
	dev->early_suspend.resume  = sprdfb_late_resume;
	dev->early_suspend.level   = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&dev->early_suspend);
#endif

	return 0;

cleanup:
	sprdfb_panel_remove(dev);
	dev->ctrl->uninit(dev);
	fb_free_resources(dev);
err0:
	dev_err(&pdev->dev, "failed to probe sprdfb\n");
	return ret;
}

static void __devexit sprdfb_remove(struct platform_device *pdev)
{
	struct sprdfb_device *dev = platform_get_drvdata(pdev);
	printk("sprdfb: [%s]\n",__FUNCTION__);

	sprdfb_panel_remove(dev);
	dev->ctrl->uninit(dev);
	fb_free_resources(dev);
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

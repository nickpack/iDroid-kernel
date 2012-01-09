/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/platform_device.h>

static u32 palette[16];

static struct fb_fix_screeninfo s5l8920_fb_fix __devinitdata = {
	.id =		"S5L8920 CLCD",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.ypanstep =	1,
	.ywrapstep =	0,
	.line_length =	640 * 4,
	.accel =	FB_ACCEL_NONE,
};

static struct fb_info info;

static struct fb_ops s5l8920_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_fillrect	= cfb_fillrect, 	/* Needed !!! */
	.fb_copyarea	= cfb_copyarea,	/* Needed !!! */
	.fb_imageblit	= cfb_imageblit,	/* Needed !!! */
};

static struct fb_var_screeninfo s5l8920_fb_var __devinitdata = {
	.xres = 640,
	.yres = 960,
	.xres_virtual = 640,
	.yres_virtual = 960,
	.xoffset = 0,
	.yoffset = 0,
	.bits_per_pixel = 32,
	.grayscale = 0,
	.red = {
		.offset = 16,
		.length = 8,
		.msb_right = 0
	},
	.blue = {
		.offset = 0,
		.length = 8,
		.msb_right = 0
	},
	.green = {
		.offset = 8,
		.length = 8,
		.msb_right = 0
	},
	.width = 50,
	.height = 75,
	.activate = FB_ACTIVATE_NOW
};

/* ------------------------------------------------------------------------- */

    /*
     *  Initialization
     */

static int __devinit s5l8920_fb_probe(struct platform_device *pdev)
{
    struct fb_info *info;
    struct xxx_par *par;
    struct device *device = &pdev->dev;
   
    /*
     * Dynamically allocate info and par
     */
    info = framebuffer_alloc(0, device);

    if (!info) {
	    /* goto error path */
		return EIO;
    }

    par = info->par;

    /* 
     * Here we set the screen_base to the virtual memory address
     * for the framebuffer. Usually we obtain the resource address
     * from the bus layer and then translate it to virtual memory
     * space via ioremap. Consult ioport.h. 
     */
    info->screen_base = device->platform_data;
    info->fbops = &s5l8920_fb_ops;
    info->fix = s5l8920_fb_fix; /* this will be the only time s5l8920_fb_fix will be
			    * used, so mark it as __devinitdata
			    */

    fb_alloc_cmap(&info->cmap, 256, 0);
    info->pseudo_palette = palette;

    /*
     * Set up flags to indicate what sort of acceleration your
     * driver can provide (pan/wrap/copyarea/etc.) and whether it
     * is a module -- see FBINFO_* in include/linux/fb.h
     *
     * If your hardware can support any of the hardware accelerated functions
     * fbcon performance will improve if info->flags is set properly.
     *
     * FBINFO_HWACCEL_COPYAREA - hardware moves
     * FBINFO_HWACCEL_FILLRECT - hardware fills
     * FBINFO_HWACCEL_IMAGEBLIT - hardware mono->color expansion
     * FBINFO_HWACCEL_YPAN - hardware can pan display in y-axis
     * FBINFO_HWACCEL_YWRAP - hardware can wrap display in y-axis
     * FBINFO_HWACCEL_DISABLED - supports hardware accels, but disabled
     * FBINFO_READS_FAST - if set, prefer moves over mono->color expansion
     * FBINFO_MISC_TILEBLITTING - hardware can do tile blits
     *
     * NOTE: These are for fbcon use only.
     */
    info->flags = FBINFO_DEFAULT;

    /* 
     * The following is done in the case of having hardware with a static 
     * mode. If we are setting the mode ourselves we don't call this. 
     */	
    info->var = s5l8920_fb_var;

    /*
     * Does a call to fb_set_par() before register_framebuffer needed?  This
     * will depend on you and the hardware.  If you are sure that your driver
     * is the only device in the system, a call to fb_set_par() is safe.
     *
     * Hardware in x86 systems has a VGA core.  Calling set_par() at this
     * point will corrupt the VGA console, so it might be safer to skip a
     * call to set_par here and just allow fbcon to do it for you.
     */
    /* s5l8920_fb_set_par(info); */

    if (register_framebuffer(info) < 0) {
	fb_dealloc_cmap(&info->cmap);
	return -EINVAL;
    }
    printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
	   info->fix.id);
    platform_set_drvdata(pdev, info);
    return 0;
}

    /*
     *  Cleanup
     */
static void __devexit s5l8920_fb_remove(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);

	if (info) {
		unregister_framebuffer(info);
		fb_dealloc_cmap(&info->cmap);
		/* ... */
		framebuffer_release(info);
	}
}

#ifdef CONFIG_PM
/**
 *	s5l8920_fb_suspend - Optional but recommended function. Suspend the device.
 *	@dev: platform device
 *	@msg: the suspend event code.
 *
 *      See Documentation/power/devices.txt for more information
 */
static int s5l8920_fb_suspend(struct platform_device *pdev, pm_message_t msg)
{
	struct fb_info *info = platform_get_drvdata(pdev);
	struct s5l8920_fb_par *par = info->par;

	/* suspend here */
	return 0;
}

/**
 *	s5l8920_fb_resume - Optional but recommended function. Resume the device.
 *	@dev: platform device
 *
 *      See Documentation/power/devices.txt for more information
 */
static int s5l8920_fb_resume(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);
	struct s5l8920_fb_par *par = info->par;

	/* resume here */
	return 0;
}
#else
#define s5l8920_fb_suspend NULL
#define s5l8920_fb_resume NULL
#endif /* CONFIG_PM */

static struct platform_driver s5l8920_fb_driver = {
	.probe = s5l8920_fb_probe,
	.remove = s5l8920_fb_remove,
	.suspend = s5l8920_fb_suspend, /* optional but recommended */
	.resume = s5l8920_fb_resume,   /* optional but recommended */
	.driver = {
		.name = "s5l8920_fb",
	},
};

static int __init s5l8920_fb_init(void)
{
	return platform_driver_register(&s5l8920_fb_driver);
}

static void __exit s5l8920_fb_exit(void)
{
	platform_driver_unregister(&s5l8920_fb_driver);
}

module_init(s5l8920_fb_init);
module_exit(s5l8920_fb_remove);

MODULE_LICENSE("GPL");

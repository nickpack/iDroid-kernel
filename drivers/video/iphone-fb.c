/*
 * iphone-fb.c - A framebuffer driver for the iPhone LCD screen.
 *
 * Copyright 2010 Ricky Taylor
 * 	- Added sleep support.
 *
 * Copyright 2008 Yidou Wang
 *
 * This file is part of iDroid. An android distribution for Apple products.
 * For more information, please visit http://www.idroidproject.org/.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <mach/map.h>
#include <mach/hardware.h>
#include <mach/iphone-clock.h>
#include <mach/gpio.h>

#define DEFAULT_WINDOW_NUM 	2
#define LCD					IO_ADDRESS(0x38900000)
#define LCD_INTERRUPT 		0xD
#define LCD_POWER			0x100
#define LCD_CLOCKGATE1		0x07
#define LCD_CLOCKGATE2		0x1D
#define VIDINTCON0			0x14
#define VIDINTCON1			0x18
#define BYTES_PER_PIXEL		2
#define NUMBER_OF_BUFFERS	2

DECLARE_COMPLETION(vsync_completion);

static void iphone_set_fb_address(int window, dma_addr_t address) {
	u32 windowBase;
	switch(window) {
		case 1:
			windowBase = LCD + 0x58;
			break;
		case 2:
			windowBase = LCD + 0x70;
			break;
		case 3:
			windowBase = LCD + 0x88;
			break;
		case 4:
			windowBase = LCD + 0xA0;
			break;
		case 5:
			windowBase = LCD + 0xB8;
			break;
		default:
			return;
	}

	__raw_writel(address, windowBase + 8);
}

/*
 *  This is just simple sample code.
 *
 *  No warranty that it actually compiles.
 *  Even less warranty that it actually works :-)
 */

/*
 * Driver data
 */
static void* framebuffer_virtual_memory __devinitdata;

static struct fb_var_screeninfo iphonefb_var __devinitdata = {
	.xres = 320,
	.yres = 480,
	.xres_virtual = 320,
	.yres_virtual = 480 * NUMBER_OF_BUFFERS,
	.xoffset = 0,
	.yoffset = 0,
	.bits_per_pixel = (BYTES_PER_PIXEL * 8),
	.grayscale = 0,
#if (BYTES_PER_PIXEL == 2)
	.red = {
		.offset = 11,
		.length = 5,
		.msb_right = 0
	},
	.blue = {
		.offset = 0,
		.length = 5,
		.msb_right = 0
	},
	.green = {
		.offset = 5,
		.length = 6,
		.msb_right = 0
	},
#endif
#if (BYTES_PER_PIXEL == 4)
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
#endif
	.width = 50,
	.height = 75,
	.activate = FB_ACTIVATE_NOW
};

// TODO: Make this thread-safe!
static void iphonefb_wait_for_vsync(void)
{
	INIT_COMPLETION(vsync_completion);

	/* Clear any already pending interrupts */
	writel(1, LCD + VIDINTCON1);
	
	/* Enable frame interrupts */
	writel(0x7F01, LCD + VIDINTCON0);

	wait_for_completion(&vsync_completion);
}

/*
 *  If your driver supports multiple boards, you should make the
 *  below data types arrays, or allocate them dynamically (using kmalloc()).
 */

/*
 * This structure defines the hardware state of the graphics card. Normally
 * you place this in a header file in linux/include/video. This file usually
 * also includes register information. That allows other driver subsystems
 * and userland applications the ability to use the same header file to
 * avoid duplicate work and easy porting of software.
 */
struct iphonefb_par {
	u32 palette[16];
};

/*
 * Here we define the default structs fb_fix_screeninfo and fb_var_screeninfo
 * if we don't use modedb. If we do use modedb see iphonefb_init how to use it
 * to get a fb_var_screeninfo. Otherwise define a default var as well.
 */
static struct fb_fix_screeninfo iphonefb_fix __devinitdata = {
	.id =		"iphonefb",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.ypanstep =	1,
	.ywrapstep =	0,
	.line_length =	320 * BYTES_PER_PIXEL,
	.accel =	FB_ACCEL_NONE,
};

/*
 * 	Modern graphical hardware not only supports pipelines but some
 *  also support multiple monitors where each display can have its
 *  its own unique data. In this case each display could be
 *  represented by a separate framebuffer device thus a separate
 *  struct fb_info. Now the struct iphonefb_par represents the graphics
 *  hardware state thus only one exist per card. In this case the
 *  struct iphonefb_par for each graphics card would be shared between
 *  every struct fb_info that represents a framebuffer on that card.
 *  This allows when one display changes it video resolution (info->var)
 *  the other displays know instantly. Each display can always be
 *  aware of the entire hardware state that affects it because they share
 *  the same iphonefb_par struct. The other side of the coin is multiple
 *  graphics cards that pass data around until it is finally displayed
 *  on one monitor. Such examples are the voodoo 1 cards and high end
 *  NUMA graphics servers. For this case we have a bunch of pars, each
 *  one that represents a graphics state, that belong to one struct
 *  fb_info. Their you would want to have *par point to a array of device
 *  states and have each struct fb_ops function deal with all those
 *  states. I hope this covers every possible hardware design. If not
 *  feel free to send your ideas at jsimmons@users.sf.net
 */

/*
 *  If your driver supports multiple boards or it supports multiple
 *  framebuffers, you should make these arrays, or allocate them
 *  dynamically using framebuffer_alloc() and free them with
 *  framebuffer_release().
 */

/*
 * Each one represents the state of the hardware. Most hardware have
 * just one hardware state. These here represent the default state(s).
 */

int iphonefb_init(void);

static inline u32 convert_bitfield(int val, struct fb_bitfield *bf)
{
	unsigned int mask = (1 << bf->length) - 1;

	return (val >> (16 - bf->length) & mask) << bf->offset;
}

static int iphonefb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue, unsigned transp, struct fb_info *info) {
	if (regno < 16) {
		u32* pal = (u32*) info->pseudo_palette;
		pal[regno] = convert_bitfield(blue, &info->var.blue) |
			convert_bitfield(green, &info->var.green) |
			convert_bitfield(red, &info->var.red);
		return 0;
	}
	else {
		return 1;
	}

}

static int iphonefb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	if((var->rotate & 1) != (info->var.rotate & 1)) {
		if((var->xres != info->var.yres) ||
				(var->yres != info->var.xres) ||
				(var->xres_virtual != info->var.yres) ||
				(var->yres_virtual >
				 info->var.xres * NUMBER_OF_BUFFERS) ||
				(var->yres_virtual < info->var.xres )) {
			return -EINVAL;
		}
	}
	else {
		if((var->xres != info->var.xres) ||
				(var->yres != info->var.yres) ||
				(var->xres_virtual != info->var.xres) ||
				(var->yres_virtual >
				 info->var.yres * NUMBER_OF_BUFFERS) ||
				(var->yres_virtual < info->var.yres )) {
			return -EINVAL;
		}
	}
	if((var->xoffset != info->var.xoffset) ||
			(var->bits_per_pixel != info->var.bits_per_pixel) ||
			(var->grayscale != info->var.grayscale)) {
		return -EINVAL;
	}
	return 0;
}

static int iphonefb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	iphone_set_fb_address(DEFAULT_WINDOW_NUM, info->fix.smem_start + (info->var.xres * BYTES_PER_PIXEL * var->yoffset));

	iphonefb_wait_for_vsync();
	return 0;
}

static int iphonefb_blank(int _blank, struct fb_info *_info)
{
	unsigned int colour;

	if(_blank > FB_BLANK_UNBLANK)
		colour = 0xffffffff;
	else
		colour = 0x00000000;

	// TODO: This is a horrible hack,
	// I mean, it would've taken LESS TIME
	// to write the code that multiplies the width
	// by the height, by the bitdepth, than writing this
	// comment about it being a hack. -- Ricky26
	memset(_info->screen_base, 0xFFFFFFFF, 320*480*4);

	if(_blank < FB_BLANK_POWERDOWN)
	{
		// TODO: sort this out -- Ricky26
		iphonefb_wait_for_vsync();
		iphonefb_wait_for_vsync();
		iphonefb_wait_for_vsync();
	}

	return 0;
}

/*
 *  Frame buffer operations
 */

static struct fb_ops iphonefb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var   = iphonefb_check_var,
	.fb_setcolreg   = iphonefb_setcolreg,
	.fb_pan_display = iphonefb_pan_display,
	.fb_blank 		= iphonefb_blank,
	.fb_fillrect	= cfb_fillrect, 	/* Needed !!! */
	.fb_copyarea	= cfb_copyarea,	/* Needed !!! */
	.fb_imageblit	= cfb_imageblit,	/* Needed !!! */
};


static irqreturn_t lcd_frame_irq(int irq, void* pToken)
{
	/* Clear pending interrupt */
	writel(1, LCD + VIDINTCON1);

	/* Disable frame interrupt */
	writel(0x7F00, LCD + VIDINTCON0);
	
	/* Notify pan operation */
	complete_all(&vsync_completion);
	
	return IRQ_HANDLED;
}

/*
 * Power Management
 */


/**
 * This function changes the LCD's power state,
 * never call it. Not ever.
 *
 * There is a good reason for this:
 * We don't have the code for re-initializing the
 * LCD display, and like hell am I going to port it
 * from OpeniBoot. -- Ricky26
 */
static int iphonefb_power(int _pwr)
{
	if(_pwr > 0)
		iphone_power_ctrl(LCD_POWER, 1);

	iphone_clock_gate_switch(LCD_CLOCKGATE1, _pwr);
	iphone_clock_gate_switch(LCD_CLOCKGATE2, _pwr);

	if(_pwr <= 0)
		iphone_power_ctrl(LCD_POWER, 0);

	return 0;
}

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND

// Guh, DAMN YOU EARLY SUSPEND!
static struct fb_info *iphone_suspend_info = NULL;

static void iphonefb_early_suspend(struct early_suspend *_susp)
{
	if(iphone_suspend_info)
		fb_blank(iphone_suspend_info, FB_BLANK_NORMAL);

	iphone_gpio_pin_output(0x3, 0);
}

static void iphonefb_late_resume(struct early_suspend *_susp)
{
	if(iphone_suspend_info)
		fb_blank(iphone_suspend_info, FB_BLANK_UNBLANK);

	iphone_gpio_custom_io(0x3, 0x2);
}

struct early_suspend iphone_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend = &iphonefb_early_suspend,
	.resume = &iphonefb_late_resume,
};

#define iphonefb_suspend NULL
#define iphonefb_resume NULL
#else

static int iphonefb_suspend(struct platform_device *_pdev, pm_message_t _state)
{
	struct fb_info *info = (struct fb_info*)platform_get_drvdata(_pdev);
	if(info)
		fb_blank(info, FB_BLANK_NORMAL);

	iphone_gpio_pin_output(0x3, 0);
	return 0;
}

static int iphonefb_resume(struct platform_device *_pdev)
{
	struct fb_info *info = (struct fb_info*)platform_get_drvdata(_pdev);
	if(info)
		fb_blank(info, FB_BLANK_UNBLANK);

	iphone_gpio_custom_io(0x3, 0x2);
	
	return 0;
}

#endif
#endif


/* ------------------------------------------------------------------------- */

/*
 *  Initialization
 */

/* static int __init xxfb_probe (struct platform_device *pdev) -- for platform devs */
static int __init iphonefb_probe(struct platform_device *pdev)
{
    struct fb_info *info;
    struct iphonefb_par *par;
    struct device *device = &pdev->dev; /* or &pdev->dev */
    dma_addr_t dma_map;
    u32 framesize;
    int ret;
	
	iphonefb_power(1);

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&iphone_early_suspend);
#endif

    /* Disable frame interrupts */
    writel(0x7F00, LCD + 0x14);

    ret = request_irq(LCD_INTERRUPT, lcd_frame_irq, IRQF_DISABLED, "iphonefb", (void*) 0);
    if(ret)
	    return ret;

    /*
     * Dynamically allocate info and par
     */
    info = framebuffer_alloc(sizeof(struct iphonefb_par), device);
    framesize = iphonefb_var.xres_virtual * iphonefb_var.yres_virtual * BYTES_PER_PIXEL;
    framebuffer_virtual_memory = dma_alloc_writecombine(device, PAGE_ALIGN(framesize), &dma_map, GFP_KERNEL);
    iphone_set_fb_address(DEFAULT_WINDOW_NUM, dma_map);

    if (!info) {
	    /* goto error path */
    }

    par = info->par;

    /*
     * Here we set the screen_base to the virtual memory address
     * for the framebuffer. Usually we obtain the resource address
     * from the bus layer and then translate it to virtual memory
     * space via ioremap. Consult ioport.h.
     */
    info->screen_base = framebuffer_virtual_memory;
    info->fbops = &iphonefb_ops;
    info->fix = iphonefb_fix; /* this will be the only time iphonefb_fix will be
			    * used, so mark it as __devinitdata
			    */

    info->fix.smem_start = dma_map;
    info->fix.smem_len = framesize;

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
    info->flags = FBINFO_FLAG_DEFAULT;

    /* This has to been done !!! */
    fb_alloc_cmap(&info->cmap, 256, 0);
    info->pseudo_palette = ((struct iphonefb_par*) info->par)->palette;
    /*
     * The following is done in the case of having hardware with a static
     * mode. If we are setting the mode ourselves we don't call this.
     */
    info->var = iphonefb_var;

    /*
     * Does a call to fb_set_par() before register_framebuffer needed?  This
     * will depend on you and the hardware.  If you are sure that your driver
     * is the only device in the system, a call to fb_set_par() is safe.
     *
     * Hardware in x86 systems has a VGA core.  Calling set_par() at this
     * point will corrupt the VGA console, so it might be safer to skip a
     * call to set_par here and just allow fbcon to do it for you.
     */
    /* iphonefb_set_par(info); */

    if (register_framebuffer(info) < 0)
	return -EINVAL;
    printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
	   info->fix.id);
    platform_set_drvdata(pdev, info);

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
	// ALERT: DIRTY HACK! -- Ricky26
	iphone_suspend_info = info;
#endif
#endif

    return 0;
}

/*
 *  Cleanup
 */
static int __init iphonefb_remove(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&iphone_early_suspend);
#endif

	if (info) {
		fb_blank(info, FB_BLANK_POWERDOWN);	
		unregister_framebuffer(info);
		fb_dealloc_cmap(&info->cmap);
		kfree(framebuffer_virtual_memory);
		framebuffer_release(info);
		platform_set_drvdata(pdev, NULL);
	}

	return 0;
}

/*
 *  Shutdown
 */
/*static void __init iphonefb_shutdown(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);
	if(info)
	   iphonefb_blank(FB_BLANK_POWERDOWN, info);
}
*/

/* for platform devices */

static struct platform_driver iphonefb_driver = {
	.probe = iphonefb_probe,
	.remove = iphonefb_remove,
	.suspend = iphonefb_suspend, /* optional but recommended */
	.resume = iphonefb_resume,   /* optional but recommended */
	//.shutdown = iphonefb_shutdown,
	.driver = {
		.owner = THIS_MODULE,
		.name = "iphonefb",
	},
};

static u64 iphonefb_dmamask = ~(u32)0;

static struct platform_device iphonefb_device = {
	.name = "iphonefb",
	.id = -1,
	.dev = {
		.dma_mask		= &iphonefb_dmamask,
		.coherent_dma_mask      = 0xffffffff,
	}
};

    /*
     *  Setup
     */

int __init iphonefb_init(void)
{
	int ret;
	/*
	 *  For kernel boot options (in 'video=iphonefb:<options>' format)
	 */
	char *option = NULL;

	if (fb_get_options("iphonefb", &option))
		return -ENODEV;

	ret = platform_driver_register(&iphonefb_driver);

	if (!ret) {
		ret = platform_device_register(&iphonefb_device);

		if (ret != 0) {
			platform_driver_unregister(&iphonefb_driver);
		}
	}

	return ret;
}

static void __exit iphonefb_exit(void)
{
	platform_device_unregister(&iphonefb_device);
	platform_driver_unregister(&iphonefb_driver);
}

module_init(iphonefb_init);
module_exit(iphonefb_exit);

MODULE_LICENSE("GPL");

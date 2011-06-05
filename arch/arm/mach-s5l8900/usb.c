/*
 *  arch/arm/mach-apple_iphone/usb.c
 *
 *  Copyright (C) 2008 Yiduo Wang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/usb/android_composite.h>

#include <mach/map.h>

#include "core.h"

static struct resource s3c_usb_hsotg_resources[] = {
	[0] = {
		.start	= S3C_PA_USB_HSOTG,
		.end	= S3C_PA_USB_HSOTG + SZ_256K - 1,
		.flags	= IORESOURCE_MEM,
	},

	[1] = {
		.start	= S3C_PA_USB_HSPHY,
		.end	= S3C_PA_USB_HSPHY + SZ_256K - 1,
		.flags	= IORESOURCE_MEM,
	},

	[2] = {
		.start	= 0x13,
		.end	= 0x13,
		.flags	= IORESOURCE_IRQ,
        },
};

struct platform_device s3c_device_usb_hsotg = {
#ifdef CONFIG_USB_GADGET_S3C_HSOTG
	.name		= "s3c-hsotg",
#else
	.name		= "dwc_otg",
#endif
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_usb_hsotg_resources),
	.resource	= s3c_usb_hsotg_resources,

	.dev = {
		.dma_mask			= DMA_BIT_MASK(32),
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

#ifdef CONFIG_USB_ANDROID
char *android_usb_functions[] = {
#ifdef CONFIG_USB_ANDROID_ADB
	"adb",
#endif
#ifdef CONFIG_USB_ANDROID_ACM
	"acm",
#endif
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	"usb_mass_storage",
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	"rndis",
#endif
};

static struct android_usb_product android_products[] = {
	{
		.product_id	= 0x1234,
		.num_functions	= ARRAY_SIZE(android_usb_functions),
		.functions	= android_usb_functions,
	},
};

struct android_usb_platform_data android_usb_config = {
#ifdef CONFIG_IPHONE_3G
	.product_name		= "iPhone3G",
#endif
#ifdef CONFIG_IPHONE_2G
	.product_name		= "iPhone2G",
#endif
#ifdef CONFIG_IPODTOUCH_1G
	.product_name		= "iPodTouch1G",
#endif

	//.vendor_id			= TODO,
	//.product_id			= 0x1234,
	.manufacturer_name	= "Apple",
	.serial_number		= "0123456789", // TODO: Do we need to bother with this?

	.version			= 0x0100,

	.products			= android_products,
	.num_products		= ARRAY_SIZE(android_products),

	.functions			= android_usb_functions,
	.num_functions		= ARRAY_SIZE(android_usb_functions),
};

struct platform_device android_usb = {
	.name			= "android_usb",
	.dev			= {
		.platform_data = &android_usb_config,
	}
};

struct usb_mass_storage_platform_data android_usb_storage_config = {
	.vendor		= "Apple",

#ifdef CONFIG_IPHONE_3G
	.product	= "iPhone3G",
#endif

#ifdef CONFIG_IPHONE_2G
	.product	= "iPhone2G",
#endif
#ifdef CONFIG_IPODTOUCH_1G
	.product	= "iPodTouch1G",
#endif

	.release	= 1,

	.nluns		= 1, // TODO: What the hell does this number mean?
};

struct platform_device android_usb_storage = {
	.name	= "usb_mass_storage",
	.dev	= {
		.platform_data = &android_usb_storage_config,
	}
};

struct usb_ether_platform_data android_usb_ether_config = {
	.vendorDescr	= "Apple",
	//.vendorID		= 0x1d8c, // TODO: What should we use as the vendor ID?
};

struct platform_device android_usb_ether = {
	.name			= "rndis",
	.dev			= {
		.platform_data = &android_usb_ether_config,
	}
};
#endif

static int __init iphone_usb_init(void)
{
	int ret;
	ret = platform_device_register(&s3c_device_usb_hsotg);
	if (ret)
		goto out;
#ifdef CONFIG_USB_ANDROID
	ret = platform_device_register(&android_usb_ether);
	if (ret)
		goto out_s3c;
	ret = platform_device_register(&android_usb_storage);
	if (ret)
		goto out_android_ether;
	ret = platform_device_register(&android_usb);
	if (ret)
		goto out_android_storage;
#endif
	return 0;

#ifdef CONFIG_USB_ANDROID
out_android_storage:
	platform_device_unregister(&android_usb_storage);
out_android_ether:
	platform_device_unregister(&android_usb_ether);
out_s3c:
	platform_device_unregister(&s3c_device_usb_hsotg);
#endif

out:
	printk(KERN_INFO "iphone-usb: Initialization failed.");
	return ret;
}

static void __exit iphone_usb_exit(void)
{
#ifdef CONFIG_USB_ANDROID
	platform_device_unregister(&android_usb);
	platform_device_unregister(&android_usb_storage);
	platform_device_unregister(&android_usb_ether);
#endif
	platform_device_unregister(&s3c_device_usb_hsotg);
}

module_init(iphone_usb_init);
module_exit(iphone_usb_exit);

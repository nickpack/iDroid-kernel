/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <plat/display-pipe.h>
#include <plat/mipi_dsim.h>
#include <plat/cpu.h>
#include <mach/cpu.h>
#include <mach/time.h>
#include <mach/map.h>
#include <mach/gpio.h>
#include <mach/devices.h>

static struct s5l_clcd_info clcd_info = {
	.reset_pin = S5L8930_GPIO(0x1404),
	.pull_pin = -1,

	.irq = IRQ_CLCD0,

	.dot_pitch = 326,

	.horizontal = { 133, 133, 135 },
	.vertical = { 10, 10, 12 },
};

static struct fb_videomode video_mode = {
	.xres = 1024,
	.yres = 768,

	.pixclock = 68400000,

	.left_margin = 15,
	.right_margin = 14,
	.upper_margin = 10,
	.lower_margin = 10,

	.hsync_len = 1,
	.vsync_len = 12,

	.flag = 0xD,
};

static struct gpio_keys_button buttons[] = {
	[0] = {
		.type = EV_KEY,
		.code = KEY_ENTER,
		.gpio = S5L8930_GPIO(0x0),
		.desc = "Home",
	},
	[1] = {
		.type = EV_KEY,
		.code = KEY_ESC,
		.gpio = S5L8930_GPIO(0x1),
		.desc = "Hold",
	},
	[2] = {
		.type = EV_KEY,
		.code = KEY_VOLUMEUP,
		.gpio = S5L8930_GPIO(0x2),
		.desc = "Volume Up",
	},
	[3] = {
		.type = EV_KEY,
		.code = KEY_VOLUMEDOWN,
		.gpio = S5L8930_GPIO(0x3),
		.desc = "Volume Down",
	},
};

static void __init ipad1g_init(void)
{
	s5l8930_init();

	s5l8930_register_gpio_keys(buttons, ARRAY_SIZE(buttons));
	s5l8930_register_mipi_dsim(&video_mode, 0, 0, 0, 4);
	s5l8930_register_clcd(&video_mode, 24, &clcd_info);
}

MACHINE_START(IPAD_1G, "Apple iPad 1G")
	/* Maintainer: iDroid Project */
	.map_io		= s5l8930_map_io,
	.init_irq	= s5l8930_init_irq,
	.timer		= &s5l8930_timer,
	.init_machine	= ipad1g_init,
MACHINE_END

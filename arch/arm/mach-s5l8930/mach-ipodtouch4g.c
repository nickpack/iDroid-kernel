/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <plat/display-pipe.h>
#include <plat/mipi_dsim.h>
#include <plat/cpu.h>
#include <mach/cpu.h>
#include <mach/time.h>
#include <mach/map.h>
#include <mach/gpio.h>

static struct s5l_clcd_info clcd_info = {
	.reset_pin = S5L8930_GPIO(0x206),
	.pull_pin = -1,

	.irq = IRQ_CLCD0,

	.dot_pitch = 326,

	.horizontal = { 71, 71, 73 },
	.vertical = { 12, 12, 16 },
};

static void __init ipt4g_init(void)
{
	s5l8930_init();

	s5l8930_register_mipi_dsim(640, 960, 2, 57, 1, 3);
	s5l8930_register_clcd(640, 960, 24, 51300000, &clcd_info);
}

MACHINE_START(IPOD_TOUCH_4G, "Apple iPod Touch 4G")
	/* Maintainer: iDroid Project */
	.map_io		= s5l8930_map_io,
	.init_irq	= s5l8930_init_irq,
	.timer		= &s5l8930_timer,
	.init_machine	= ipt4g_init,
MACHINE_END

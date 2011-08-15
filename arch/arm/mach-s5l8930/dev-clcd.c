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
#include <linux/dma-mapping.h>

#include <plat/display-pipe.h>
#include <mach/map.h>
#include <mach/irqs.h>

static struct resource cld0_res[] = {
	[0] = {
		.start = PA_CLCD0,
		.end = PA_CLCD0 + SZ_CLCD0 - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct resource cld1_res[] = {
	[0] = {
		.start = PA_CLCD1,
		.end = PA_CLCD1 + SZ_CLCD1 - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct s5l_display_pipe_info dp_info = {
	.driver = &s5l_display_pipe_clcd,
	
	.clk = "clcd",
	
	.resource = cld1_res,
	.num_resources = ARRAY_SIZE(cld1_res),

	.index = 0,
};

static struct platform_device clcd_dev = {
	.name = "s5l-display-pipe",
	.id = -1,

	.resource = cld0_res,
	.num_resources = ARRAY_SIZE(cld0_res),

	.dev = {
		.platform_data = &dp_info,

		.coherent_dma_mask = DMA_32BIT_MASK,
	},
};

int s5l8930_register_clcd(int _w, int _h, int _bpp, int _freq, struct s5l_clcd_info *_info)
{
	dp_info.width = _w;
	dp_info.height = _h;
	dp_info.bpp = _bpp;
	dp_info.clock_frequency = _freq;
	dp_info.pdata = _info;

	return platform_device_register(&clcd_dev);
}

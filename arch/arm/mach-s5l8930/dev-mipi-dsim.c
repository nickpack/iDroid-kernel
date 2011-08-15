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
#include <plat/display-pipe.h>
#include <mach/map.h>
#include <mach/irqs.h>

static struct resource mipi_res[] = {
	[0] = {
		.start = PA_MIPI_DSIM,
		.end = PA_MIPI_DSIM + SZ_MIPI_DSIM - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_MIPI_DSIM,
		.end = IRQ_MIPI_DSIM,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mipi_dsim_config dsim_config = {
	.e_interface = DSIM_VIDEO,
	.e_pixel_format = DSIM_24BPP_888,
	.e_no_data_lane = DSIM_DATA_LANE_3,

	.bta_timeout = 0xFF,
	.rx_timeout = 0xFFFF,

	.pll_stable_time = 300000,
};

static struct fb_videomode videomode = {
	.xres = 640,
	.yres = 960,
};

static struct s5p_platform_mipi_dsim dsim_data = {
	.dsim_config = &dsim_config,
	.lcd_panel_info = &videomode,
	.mipi_power = s5p_mipi_dsi_dphy_power,
	.phy_enable = s5p_dsim_phy_enable,
};

static struct platform_device mipi_dev = {
	.name = "s5p-mipi-dsim",
	.id = -1,
	
	.resource = mipi_res,
	.num_resources = ARRAY_SIZE(mipi_res),

	.dev = {
		.platform_data = &dsim_data,
	},
};

int s5l8930_register_mipi_dsim(int _w, int _h, int _p, int _m, int _s, int _lanes)
{
	dsim_config.p = _p;
	dsim_config.m = _m;
	dsim_config.s = _s;

	dsim_config.e_no_data_lane = _lanes;

	videomode.xres = _w;
	videomode.yres = _h;

	return platform_device_register(&mipi_dev);
}

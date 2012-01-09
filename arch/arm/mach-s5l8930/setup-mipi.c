/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/delay.h>

#include <plat/mipi_dsim.h>
#include <plat/regs-dsim.h>

#define MIPI_SETUP_DEBUG

#ifdef MIPI_SETUP_DEBUG
#define dbg_printk(x...) printk(x)
#else
#define dbg_printk(x...)
#endif

int s5p_mipi_dsi_dphy_power(struct mipi_dsim_device *dsim,
					unsigned int enable)
{
	dbg_printk("%s: %d\n", __func__, enable);
	return 0;
}

int s5p_dsim_phy_enable(struct mipi_dsim_device *dsim, bool on)
{
	//u32 cfg = (u32)dsim->pd->lcd_panel_info;

	dbg_printk("%s: %d\n", __func__, on);
	return 0;
}

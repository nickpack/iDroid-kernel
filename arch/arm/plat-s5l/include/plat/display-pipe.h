/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  S5L_DISPLAY_PIPE_H
#define  S5L_DISPLAY_PIPE_H

#include <plat/mipi_dsim.h>

struct s5l_display_pipe_state;

struct s5l_dd_sync
{
	u32 back_porch;
	u32 front_porch;
	u32 sync_pulse_width;
};

struct s5l_display_pipe_driver
{
	const char 			*name;

	int (*init)(struct s5l_display_pipe_state *_state);
	void (*shutdown)(struct s5l_display_pipe_state *_state);

	int (*suspend)(struct s5l_display_pipe_state *_state, pm_message_t);
	int (*resume)(struct s5l_display_pipe_state *_state);
};

struct s5l_display_pipe_info
{
	struct s5l_display_pipe_driver *driver;

	const char			*clk;

	struct resource		*resource;
	u32					num_resources;

	u16					index;
	u32					bpp;
	u32					clock_frequency;

	u32					width;
	u32					height;

	void 				*pdata;
};

#define S5L_NUM_UI 2

struct s5l_display_pipe_state
{
	struct platform_device *pdev;

	void *__iomem	regs;
	struct clk		*clk;

	struct s5l_display_pipe_ui *uis[S5L_NUM_UI];

	struct s5l_display_pipe_info *info;
	void			*drvdata;
};

struct s5l_clcd_info
{
	int					reset_pin;
	int					pull_pin;

	int					irq;

	u32					dot_pitch;
	u32					iv_clk;
	u32					iv_sync, ih_sync;
	u32					iv_den;

	struct s5l_dd_sync	horizontal;
	struct s5l_dd_sync	vertical;
};

extern struct s5l_display_pipe_driver s5l_display_pipe_clcd;
extern struct s5l_display_pipe_driver s5l_display_pipe_rgbout;

struct resource *display_pipe_get_resource(struct s5l_display_pipe_info *_info,
		unsigned int _type, int _idx);
int display_pipe_set_framebuffer(struct s5l_display_pipe_state *_state, u32 _idx, void *_buffer);
int display_pipe_configure_window(struct s5l_display_pipe_state *_state, u32 _idx,
	u32 _width, u32 _height, u8 _bpp);

int s5l8930_register_mipi_dsim(int _w, int _h, int _p, int _m, int _s, int _lanes);
int s5l8930_register_clcd(int _w, int _h, int _bpp, int _freq, struct s5l_clcd_info *_info);

#endif //S5L_DISPLAY_PIPE_H


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
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/compiler.h>
#include <linux/slab.h>

#include <plat/display-pipe.h>
#include <plat/mipi_dsim.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-clcd.h>

typedef struct s5l_display_pipe_state *dp_t;
struct clcd_state
{
	dp_t dp;
	struct s5l_clcd_info *info;
	struct mipi_dsim_lcd_device lcd_dev;

	u32 frame_time;

	void *__iomem regs;
};

static inline void s5l_clcd_frame_sleep(struct clcd_state *_state, u32 _frames)
{
	msleep((_frames * _state->frame_time) / 1000);
}

static inline void s5l_clcd_enable_framebuffer(struct clcd_state *_state, bool _on)
{
	if(_on)
	{
		// TODO: clear before enabling!

		writel(readl(_state->regs + 0x50) | 1, _state->regs + 0x50);
	}
	else
		writel(readl(_state->regs + 0x50) &~ 1, _state->regs + 0x50);
}

static int s5l_clcd_dev_probe(struct mipi_dsim_lcd_device *_dev)
{
	struct clcd_state *state = _dev->platform_data;
	struct mipi_dsim_device *dsim = _dev->master;
	uint8_t buffer[15];
	u32 panel_id = 0;
	u32 default_colour = 0;
	u32 backlight_cal = 0;
	
	printk("%s\n", __func__);

	gpio_set_value(state->info->reset_pin, 1);
	msleep(6);

	dsim->master_ops->cmd_write(dsim, 5, 0, 0);
	udelay(10);

	/*if(dsim->master_ops->cmd_read(dsim, 0x14, 0xb1, 0,
			&buffer, sizeof(buffer)) >= 0)
	{
		panel_id = buffer[0] << 24
			| buffer[1] << 16
			| (buffer[3] & 0xF0) << 8
			| (buffer[2] & 0xF8) << 4
			| (buffer[3] & 0xF) << 3
			| (buffer[2] & 0x7);

		default_colour = (buffer[3] & 0x8) ? 0x0 : 0xFFFFFF;
		backlight_cal = buffer[5];

		printk(KERN_INFO "clcd: panel id = 0x%08x\n", panel_id);
		printk(KERN_INFO "clcd: default colour = 0x%08x\n", default_colour);
		printk(KERN_INFO "clcd: backlight calibration = 0x%08x\n", backlight_cal);
	}
	else
		printk(KERN_ERR "%s: failed to read panel id.\n", __func__);*/

	//display_pipe_configure_window(state->dp, 0,
	//		state->dp->info->width, state->dp->info->height, 32); // Framebuffer is always RGB888
	s5l_clcd_enable_framebuffer(state, true);
	udelay(100);

	dsim->master_ops->cmd_write(dsim, 5, 0x11, 0);
	s5l_clcd_frame_sleep(state, 7);
	dsim->master_ops->cmd_write(dsim, 5, 0x29, 0);
	s5l_clcd_frame_sleep(state, 7);

	if(state->info->pull_pin != -1)
		s3c_gpio_setpull(state->info->pull_pin, S3C_GPIO_PULL_NONE);

	return 0;
}

static int s5l_clcd_dev_remove(struct mipi_dsim_lcd_device *_dev)
{
	return 0;
}

static void s5l_clcd_dev_shutdown(struct mipi_dsim_lcd_device *_dev)
{
}

#ifdef CONFIG_PM
static int s5l_clcd_dev_suspend(struct mipi_dsim_lcd_device *_dev, pm_message_t _msg)
{
	return 0;
}

static int s5l_clcd_dev_resume(struct mipi_dsim_lcd_device *_dev)
{
	return 0;
}
#else
#define s5l_clcd_dev_suspend	NULL
#define s5l_clcd_dev_resume		NULL
#endif

static struct mipi_dsim_lcd_driver s5l_clcd_driver = {
	.name = "s5l-clcd",

	.probe = s5l_clcd_dev_probe,
	.remove = s5l_clcd_dev_remove,
	.shutdown = s5l_clcd_dev_shutdown,
	.suspend = s5l_clcd_dev_suspend,
	.resume = s5l_clcd_dev_resume,
};

//
// Below is the code that makes up
// the CLCD driver referenced by other
// code. Despite being called s5l_clcd
// it just adds the MIPI device and
// passes messages along.
//


static int s5l_clcd_setup(dp_t _state)
{
	int ret = 0;
	struct clcd_state *state;
	struct s5l_clcd_info *info = _state->info->pdata;
	struct resource *res;

	printk("%s.\n", __func__);

	state = kzalloc(sizeof(struct clcd_state), GFP_KERNEL);
	if(!state)
	{
		printk("%s: Failed to allocate clcd_state!\n", __func__);
		return -ENOMEM;
	}

	state->dp = _state;
	state->info = info;

	ret = gpio_request(state->info->reset_pin, "clcd_reset");
	if(ret)
	{
		printk("%s: failed to get reset GPIO.\n", __func__);
		goto err_state;
	}

	res = display_pipe_get_resource(_state->info, IORESOURCE_MEM, 0);
	if(!res)
	{
		printk("%s: failed to get register block.\n", __func__);
		goto err_state;
	}
	
	state->regs = ioremap(res->start, resource_size(res));
	if(!state->regs)
	{
		printk("%s: failed to map register block.\n", __func__);
		goto err_state;
	}

	memset(&state->lcd_dev, 0, sizeof(state->lcd_dev));
	state->lcd_dev.name = "s5l-clcd";
	state->lcd_dev.id = -1;
	state->lcd_dev.bus_id = -1;
	state->lcd_dev.platform_data = state;

	// CLCD init
	writel(S5L_CLCD_CTL_RESET, state->regs + S5L_CLCD_CTL);
	while(readl(state->regs + S5L_CLCD_CTL) & S5L_CLCD_CTL_RESET)
		barrier();

	udelay(1);
	writel(4, state->regs + S5L_CLCD_CTL);

	writel(3, state->regs + 0x4);
	writel(0x80000001, state->regs + 0x14);
	writel(0x20408, state->regs + 0x18);

	if(_state->info->bpp <= 18)
		writel(0x1110000 | readl(state->regs + 0x14), state->regs + 0x14);

	_state->drvdata = state;

	writel(0, state->regs + 0x50);
	writel((info->iv_clk << S5L_CLCD_VIDTCON_IVCLK_SHIFT)
			| (info->ih_sync << S5L_CLCD_VIDTCON_IHSYNC_SHIFT)
			| (info->iv_sync << S5L_CLCD_VIDTCON_IVSYNC_SHIFT)
			| (info->iv_den << S5L_CLCD_VIDTCON_IVDEN_SHIFT),
			state->regs + S5L_CLCD_VIDTCON0);
	writel(((info->vertical.back_porch-1) << S5L_CLCD_VIDTCON_BP_SHIFT)
			| ((info->vertical.front_porch-1) << S5L_CLCD_VIDTCON_FP_SHIFT)
			| ((info->vertical.sync_pulse_width-1) << S5L_CLCD_VIDTCON_SPW_SHIFT),
			state->regs + S5L_CLCD_VIDTCON1);
	writel(((info->horizontal.back_porch-1) << S5L_CLCD_VIDTCON_BP_SHIFT)
			| ((info->horizontal.front_porch-1) << S5L_CLCD_VIDTCON_FP_SHIFT)
			| ((info->horizontal.sync_pulse_width-1) << S5L_CLCD_VIDTCON_SPW_SHIFT),
			state->regs + S5L_CLCD_VIDTCON2);
	writel(((_state->info->width-1) << S5L_CLCD_HOZVAL_SHIFT)
			| ((_state->info->height-1) << S5L_CLCD_LINEVAL_SHIFT),
			state->regs + S5L_CLCD_VIDTCON3);

	state->frame_time = 1000000
		* (info->vertical.back_porch
				+ info->vertical.front_porch
				+ _state->info->height)
		* (info->horizontal.back_porch
				+ info->horizontal.front_porch
				+ _state->info->width)
		/ _state->info->clock_frequency;

	// Pinot init
	gpio_direction_output(state->info->reset_pin, 0);
	msleep(10);

	// Enable MIPI
	s5p_mipi_dsi_register_lcd_device(&state->lcd_dev);

	goto exit;
err_state:
	kfree(state);

exit:
	return ret;
}

static void s5l_clcd_shutdown(dp_t _state)
{
	struct clcd_state *state = _state->drvdata;
	if(!state)
		return;

	iounmap(state->regs);
	gpio_free(state->info->reset_pin);
}

#ifdef CONFIG_PM
static int s5l_clcd_suspend(dp_t _state, pm_message_t _msg)
{
	return 0;
}

static int s5l_clcd_resume(dp_t _state)
{
	return 0;
}
#else
#define s5l_clcd_suspend	NULL
#define s5l_clcd_resume		NULL
#endif

struct s5l_display_pipe_driver s5l_display_pipe_clcd = {
	.name = "clcd",

	.init = s5l_clcd_setup,
	.shutdown = s5l_clcd_shutdown,

	.suspend = s5l_clcd_suspend,
	.resume = s5l_clcd_resume,
};
EXPORT_SYMBOL_GPL(s5l_display_pipe_clcd);

static __init int s5l_clcd_init(void)
{
	return s5p_mipi_dsi_register_lcd_driver(&s5l_clcd_driver);
}
module_init(s5l_clcd_init);

static __exit void s5l_clcd_exit(void)
{
	s5p_mipi_dsi_unregister_lcd_driver(&s5l_clcd_driver);
}
module_exit(s5l_clcd_exit);

MODULE_DESCRIPTION("S5L Colour LCD Driver");
MODULE_AUTHOR("Richard Ian Taylor");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:s5l-clcd");

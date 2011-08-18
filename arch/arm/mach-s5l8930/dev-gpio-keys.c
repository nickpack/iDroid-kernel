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
#include <linux/gpio_keys.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

static struct gpio_keys_platform_data pdata = {
};

static struct platform_device dev = {
	.name = "gpio-keys",
	
	.dev = {
		.platform_data = &pdata,
	}
};

__init void s5l8930_register_gpio_keys(struct gpio_keys_button *_btn, size_t _num)
{
	pdata.buttons = _btn;
	pdata.nbuttons = _num;

	platform_device_register(&dev);
}

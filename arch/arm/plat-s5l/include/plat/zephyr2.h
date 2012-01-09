/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct zephyr2_platform_data
{
	void *prox_cal;
	size_t prox_cal_size;

	void *calibration;
	size_t calibration_size;

	void (*power)(struct zephyr2_platform_data *_pdata, int _val);

	int reset_gpio;
	int touch_gpio;
};

/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  _S5L8930_GPIO_
#define  _S5L8930_GPIO_

#include <linux/interrupt.h>
#include <mach/hardware.h>
#include <asm-generic/gpio.h>

static inline int gpio_to_irq(unsigned gpio)
{
	return gpio;
}

static inline int irq_to_gpio(unsigned irq)
{
	return irq;
}

static inline int gpio_get_value(unsigned gpio)
{
	return 0;
}

static inline void gpio_set_value(unsigned gpio, int value)
{
}

static inline int gpio_cansleep(unsigned gpio)
{
	return 0;
}

#endif //_S5L8930_GPIO_


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

#define gpio_get_value	__gpio_get_value
#define gpio_set_value	__gpio_set_value
#define gpio_cansleep	__gpio_cansleep
#define gpio_to_irq	__gpio_to_irq

#define S5L8930_GPIO(x)	(((((x) >> 8) & 0xFF)*8) + ((x) & 0xF))
#define S3C_GPIO_END	176
#define ARCH_NR_GPIOS	(S3C_GPIO_END)

static inline int irq_to_gpio(int x) { return 0xFFFFFFFF; }

#include <asm-generic/gpio.h>

#endif //_S5L8930_GPIO_


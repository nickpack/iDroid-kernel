/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  S5L8930_DEVICES_H
#define  S5L8930_DEVICES_H

extern void s5l8930_register_gpio_keys(struct gpio_keys_button *_btn, size_t _num);
extern int s5l8930_register_h2fmi(void);

#endif //S5L8930_DEVICES_H

/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  _S5L8930_CPU_
#define  _S5L8930_CPU_

#include <linux/init.h>

extern void __init s5l8930_map_io(void);
extern void __init s5l8930_init_irq(void);
extern void __init s5l8930_init(void);

#endif //_S5L8930_CPU_

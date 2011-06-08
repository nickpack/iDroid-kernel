/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  _S5L_IRQ_
#define  _S5L_IRQ_

#include <linux/compiler.h>
#include <linux/types.h>

extern void s5l_init_vics(void __iomem **_bases, uint32_t _count);

#endif //_S5L_IRQ_

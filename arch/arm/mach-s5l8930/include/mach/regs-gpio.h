/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  S5L8930_REGS_GPIO_H
#define  S5L8930_REGS_GPIO_H

#define S5L_GPIO_INTBLK		0xc00
#define S5L_GPIO_INTDIS(x)	(0x800 + (4*(x)))
#define S5L_GPIO_INTEN(x)	(0x840 + (4*(x)))
#define S5L_GPIO_INTSTS(x)	(0x880 + (4*(x)))
#define S5L_GPIO_PIN(x)		(4*(x))
#define S5L_GPIOIC_BLKMSK	(S5L_GPIOIC_BLKSZ-1)
#define S5L_GPIOIC_BLKSHIFT	5

#endif //S5L8930_REGS_GPIO_H

/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  _S5L8930_TIME_
#define  _S5L8930_TIME_

#include <mach/map.h>

#define S5L_CLOCK_HZ	24000000
#define S5L_CLOCK_LO	(VA_PMGR0 + 0x2000)
#define S5L_CLOCK_HI	(VA_PMGR0 + 0x2004)

#define S5L_TIMER0_IRQ	6
#define S5L_TIMER0_VAL	(VA_PMGR0 + 0x2008)
#define S5L_TIMER0_CTRL	(VA_PMGR0 + 0x2010)

#define S5L_TIMER1_IRQ	5
#define S5L_TIMER1_VAL	(VA_PMGR0 + 0x200c)
#define S5L_TIMER1_CTRL	(VA_PMGR0 + 0x2014)

#define S5L_TIMER_ENABLE	1
#define S5L_TIMER_DISABLE	2

struct sys_timer;
extern struct sys_timer s5l8930_timer;

#endif //_S5L8930_TIME_

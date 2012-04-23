/**
 * Copyright (c) 2012 Alexey Makhalov (makhaloff@gmail.com)
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  _S5L8930_SYSTEM_
#define  _S5L8930_SYSTEM_

#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/platform.h>
#include <mach/map.h>
#include <asm/proc-fns.h>

static inline void arch_idle(void)
{
	// Dear Linux,
	// For Christmas this year,
	// please make the CPU not combust. -- Ricky
	cpu_do_idle();
}

static inline void arch_reset(char mode, const char* cmd)
{
	while (1) {
		__raw_writel(0,	VA_PMGR0 + 0x202c);
		__raw_writel(1, VA_PMGR0 + 0x2024);
		__raw_writel(0x80000000, VA_PMGR0 + 0x2020);
		__raw_writel(4, VA_PMGR0 + 0x202c);
		__raw_writel(0, VA_PMGR0 + 0x2020);
	}
}

#endif //_S5L8930_SYSTEM_

/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <mach/hardware.h>
#include <mach/map.h>

		.macro	disable_fiq
		.endm

		.macro  get_irqnr_preamble, base, tmp
		.endm

		.macro  arch_ret_to_user, tmp1, tmp2
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		ldr	\base, =PA_VIC0
		ldr	\irqstat, [\base, #0]           @ get masked status
		cmp	\irqstat, #0
		addeq	\base, \base, #0x10000
		ldreq	\irqstat, [\base, #0]           @ get masked status
		cmpeq	\irqstat, #0
		addeq	\base, \base, #0x10000
		ldreq	\irqstat, [\base, #0]           @ get masked status
		cmpeq	\irqstat, #0
		addeq	\base, \base, #0x10000
		ldreq	\irqstat, [\base, #0]           @ get masked status
		cmpeq	\irqstat, #0
		ldr	\irqnr, [\base, #0xF00]         @ read for VICADDR[01]
		.endm

		.macro irq_prio_table
		.endm

/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <mach/map.h>
#include <asm/memory.h>

		.macro	addruart, rp, rv
		ldr		\rp, =PA_UART0
		ldr		\rv, =PA_UART0
		.endm

		.macro  senduart, rd, rx
		str     \rd, [\rx, #0x20]       @ UTXH
		.endm

		.macro  waituart, rd, rx
		.endm

		.macro  busyuart, rd, rx
1001:		ldr     \rd, [\rx, #0x10]	@ UTRSTAT
		tst     \rd, #1 << 2
		beq	1001b
		.endm

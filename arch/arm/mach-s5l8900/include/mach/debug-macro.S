/* arch/arm/mach-apple_iphone/include/mach/debug-macro.S
 *
 * Debugging macro include header
 *
 *  Copyright (C) 2008 Yiduo Wang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <mach/hardware.h>

		.macro	addruart, rx, tmp
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		ldreq	\rx, =0x3CC00000
		ldrne	\rx, =IO_ADDRESS(0x3CC00000)
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

/*
 *  arch/arm/mach-apple_iphone/clock.c
 *
 *  Copyright (C) 2008 Yiduo Wang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/iphone-clock.h>

// Constants
#define NUM_PLL 4
#define FREQUENCY_BASE 12000000
#define PLL0_INFREQ_DIV FREQUENCY_BASE /* 12 MHz */
#define PLL1_INFREQ_DIV FREQUENCY_BASE /* 12 MHz */
#define PLL2_INFREQ_DIV FREQUENCY_BASE /* 12 MHz */
#define PLL3_INFREQ_DIV 13500000 /* 13.5 MHz */
#define PLL0_INFREQ_MULT 0x4000
#define PLL1_INFREQ_MULT 0x4000
#define PLL2_INFREQ_MULT 0x4000
#define PLL3_INFREQ_MULT FREQUENCY_BASE

// Devices
#define CLOCK1 IO_ADDRESS(0x3C500000)

// Registers
#define CLOCK1_CONFIG0 0x0
#define CLOCK1_CONFIG1 0x4
#define CLOCK1_CONFIG2 0x8
#define CLOCK1_PLL0CON 0x20
#define CLOCK1_PLL1CON 0x24
#define CLOCK1_PLL2CON 0x28
#define CLOCK1_PLL3CON 0x2C
#define CLOCK1_PLLMODE 0x44
#define CLOCK1_CL2_GATES 0x48
#define CLOCK1_CL3_GATES 0x4C

// Values
#define CLOCK1_Separator 0x20

#define CLOCK1_CLOCKPLL(x) GET_BITS((x), 12, 2)
#define CLOCK1_CLOCKDIVIDER(x) (GET_BITS((x), 0, 4) + 1)
#define CLOCK1_CLOCKHASDIVIDER(x) GET_BITS((x), 8, 1)

#define CLOCK1_MEMORYPLL(x) GET_BITS((x), 12, 2)
#define CLOCK1_MEMORYDIVIDER(x) (GET_BITS((x), 16, 4) + 1)
#define CLOCK1_MEMORYHASDIVIDER(x) GET_BITS((x), 24, 1)

#define CLOCK1_BUSPLL(x) GET_BITS((x), 12, 2)
#define CLOCK1_BUSDIVIDER(x) (GET_BITS((x), 16, 4) + 1)
#define CLOCK1_BUSHASDIVIDER(x) GET_BITS((x), 24, 1)

#define CLOCK1_UNKNOWNPLL(x) GET_BITS((x), 12, 2)
#define CLOCK1_UNKNOWNDIVIDER1(x) (GET_BITS((x), 0, 4) + 1)
#define CLOCK1_UNKNOWNDIVIDER2(x) (GET_BITS((x), 4, 4) + 1)
#define CLOCK1_UNKNOWNDIVIDER(x) (CLOCK1_UNKNOWNDIVIDER1(x) * CLOCK1_UNKNOWNDIVIDER2(x))
#define CLOCK1_UNKNOWNHASDIVIDER(x) GET_BITS((x), 8, 1)

#define CLOCK1_PERIPHERALDIVIDER(x) GET_BITS((x), 20, 2)

#define CLOCK1_DISPLAYPLL(x) GET_BITS((x), 28, 2)
#define CLOCK1_DISPLAYDIVIDER(x) GET_BITS((x), 16, 4)
#define CLOCK1_DISPLAYHASDIVIDER(x) GET_BITS((x), 24, 1)

#define CLOCK1_PLLMODE_ONOFF(x, y) (((x) >> (y)) & 0x1)
#define CLOCK1_PLLMODE_DIVIDERMODE(x, y) (((x) >> (y + 4)) & 0x1)
#define CLOCK1_PLLMODE_DIVIDE 1
#define CLOCK1_PLLMODE_MULTIPLY 0

#define CLOCK1_MDIV(x) (((x) >> 8) & 0x3FF)
#define CLOCK1_PDIV(x) (((x) >> 24) & 0x3F)
#define CLOCK1_SDIV(x) ((x) & 0x3)

void iphone_clock_gate_switch(u32 gate, int on_off) {
	u32 gate_register;
	u32 gate_flag;
	u32 gates;

	if(gate < CLOCK1_Separator) {
		gate_register = CLOCK1 + CLOCK1_CL2_GATES;
		gate_flag = gate;
	} else {
		gate_register = CLOCK1 + CLOCK1_CL3_GATES;
		gate_flag = gate - CLOCK1_Separator;
	}

	gates = __raw_readl(gate_register);

	if(on_off) {
		gates &= ~(1 << gate_flag);
	} else {
		gates |= 1 << gate_flag;
	}

	__raw_writel(gates, gate_register);

}


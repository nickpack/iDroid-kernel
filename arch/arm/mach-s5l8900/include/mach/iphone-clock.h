/*
 *  arch/arm/mach-apple_iphone/clock.h
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

#define FREQUENCY_CPU 412000000
#define FREQUENCY_MEMORY 137333333
#define FREQUENCY_BUS 103000000
#define FREQUENCY_PERIPHERAL 51500000
#define FREQUENCY_DISPLAY 54000000
#define FREQUENCY_FIXED 24000000
#define FREQUENCY_TIMEBASE 6000000
#define FREQUENCY_BASE 12000000

extern int iphone_power_ctrl(u32 device, int on_off);
extern void iphone_clock_gate_switch(u32 gate, int on_off);
extern u64 iphone_microtime(void);
extern int iphone_has_elapsed(u64 startTime, u64 elapsedTime);

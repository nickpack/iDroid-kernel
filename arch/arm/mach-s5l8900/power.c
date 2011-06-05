/*
 *  arch/arm/mach-apple_iphone/power.c
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

#define POWER IO_ADDRESS(0x39A00000)	/* probably a part of the system controller */

#define POWER_ONCTRL 0xC
#define POWER_OFFCTRL 0x10
#define POWER_SETSTATE 0x8
#define POWER_STATE 0x14

int iphone_power_ctrl(u32 device, int on_off) {
	if(on_off) {
		__raw_writel(device, POWER + POWER_ONCTRL);
	} else {
		__raw_writel(device, POWER + POWER_OFFCTRL);
	}

	/* wait for the new state to take effect */
	while((__raw_readl(POWER + POWER_SETSTATE) & device) != (__raw_readl(POWER + POWER_STATE) & device));

	return 0;
}

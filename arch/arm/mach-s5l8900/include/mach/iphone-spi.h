/*
 * arch/arm/mach-apple_iphone/include/mach/iphone-spi.h - SPI header for iPhone
 *
 * Copyright (C) 2008 Yiduo Wang
 *
 * Portions Copyright (C) 2010 Ricky Taylor
 *
 * This file is part of iDroid. An android distribution for Apple products.
 * For more information, please visit http://www.idroidproject.org/.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef IPHONE_SPI_H
#define IPHONE_SPI_H

// Register Addresses
#define IPHONE_SPI0_REGBASE			((uint32_t*)IO_ADDRESS(0x3C300000))
#define IPHONE_SPI1_REGBASE			((uint32_t*)IO_ADDRESS(0x3CE00000))
#define IPHONE_SPI2_REGBASE			((uint32_t*)IO_ADDRESS(0x3D200000))

// Registers
#define IPHONE_SPI_REGISTER(a, b)	((uint32_t*)(((uint8_t*)a)+b))
#define IPHONE_SPI_CONTROL(a)		IPHONE_SPI_REGISTER(a, 0x00)
#define IPHONE_SPI_SETUP(a)			IPHONE_SPI_REGISTER(a, 0x04)
#define IPHONE_SPI_STATUS(a)		IPHONE_SPI_REGISTER(a, 0x08)
#define IPHONE_SPI_PIN(a)			IPHONE_SPI_REGISTER(a, 0x0C)
#define IPHONE_SPI_TXDATA(a)		IPHONE_SPI_REGISTER(a, 0x10)
#define IPHONE_SPI_RXDATA(a)		IPHONE_SPI_REGISTER(a, 0x20)
#define IPHONE_SPI_CLKDIV(a)		IPHONE_SPI_REGISTER(a, 0x30)
#define IPHONE_SPI_RXAMT(a)			IPHONE_SPI_REGISTER(a, 0x34)
#define IPHONE_SPI_UNKREG1(a)		IPHONE_SPI_REGISTER(a, 0x38)

// Values
#define IPHONE_SPI_MAX_TX_BUFFER		8
#define IPHONE_SPI_TX_BUFFER_USED(x)	GET_BITS(x, 4, 4)
#define IPHONE_SPI_RX_BUFFER_USED(x)	GET_BITS(x, 8, 4)

#define IPHONE_SPI_CLOCK_SHIFT 12
#define IPHONE_SPI_MAX_DIVIDER 0x3FF

#define IPHONE_SPI0_CLOCKGATE 0x22
#define IPHONE_SPI1_CLOCKGATE 0x2B
#define IPHONE_SPI2_CLOCKGATE 0x2F

#define IPHONE_SPI0_IRQ 0x9
#define IPHONE_SPI1_IRQ 0xA
#define IPHONE_SPI2_IRQ 0xB

#define IPHONE_SPI_TIMEOUT_MSECS		5000

#endif


/*
 *  arch/arm/mach-apple_iphone/include/mach/uncompress.h
 *
 *  Copyright (C) 2008 Apple iPhone
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

#define IPHONE_UART_UFSTAT_TXFIFO_FULL (0x1 << 9)
#define IPHONE_UART_UTRSTAT_TRANSMITTEREMPTY 0x4
#define IPHONE_UART0_UTRSTAT (*(volatile unsigned char*)(0x3CC00000 + 0x0 + 0x10))
#define IPHONE_UART0_UFSTAT (*(volatile unsigned char*)(0x3CC00000 + 0x0 + 0x18))
#define IPHONE_UART0_UTXH (*(volatile unsigned char*)(0x3CC00000 + 0x0 + 0x20))

/*
 * This does not append a newline
 */
static inline void putc(int c)
{
	while ((IPHONE_UART0_UTRSTAT & IPHONE_UART_UTRSTAT_TRANSMITTEREMPTY) == 0)
		barrier();

	IPHONE_UART0_UTXH = c;

}

static inline void flush(void)
{
	while ((IPHONE_UART0_UTRSTAT & IPHONE_UART_UTRSTAT_TRANSMITTEREMPTY) == 0)
		barrier();
}

/*
 * nothing to do
 */
#define arch_decomp_setup()
#define arch_decomp_wdog()

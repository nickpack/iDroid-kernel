/*
 *  arch/arm/mach-apple_iphone/uart.c
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
#include <linux/sysdev.h>
#include <linux/io.h>

#include <linux/delay.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <mach/iphone-clock.h>
#include <mach/hardware.h>

// Devices
#define UART IO_ADDRESS(0x3CC00000)

// Registers
#define UART0 0x0
#define UART1 0x4000
#define UART2 0x8000
#define UART3 0xC000
#define UART4 0x10000

#define UART_ULCON 0x0
#define UART_UCON 0x4
#define UART_UFCON 0x8
#define UART_UMCON 0xC
#define UART_UTRSTAT 0x10
#define UART_UERSTAT 0x14
#define UART_UFSTAT 0x18
#define UART_UMSTAT 0x1C
#define UART_UTXH 0x20
#define UART_URXH 0x24
#define UART_UBAUD 0x28
#define UART_UDIVSLOT 0x2C

// Values
#define NUM_UARTS 5
#define UART_CLOCKGATE 0x29

#define UART_CLOCK_SELECTION_MASK (0x3 << 10) // Bit 10-11
#define UART_CLOCK_SELECTION_SHIFT 10 // Bit 10-11
#define UART_UCON_UNKMASK 0x7000
#define UART_UCON_LOOPBACKMODE (0x1 << 5)
#define UART_UCON_RXMODE_SHIFT 0
#define UART_UCON_RXMODE_MASK (0x3 << UART_UCON_RXMODE_SHIFT)
#define UART_UCON_TXMODE_SHIFT 2
#define UART_UCON_TXMODE_MASK (0x3 << UART_UCON_TXMODE_SHIFT)

#define UART_FIFO_RESET_TX 0x4
#define UART_FIFO_RESET_RX 0x2
#define UART_FIFO_ENABLE 0x1

#define UART_DIVVAL_MASK 0x0000FFFF
#define UART_SAMPLERATE_MASK 0x00030000 // Bit 16-17
#define UART_SAMPLERATE_SHIFT 16

#define UART_UCON_MODE_DISABLE 0
#define UART_UCON_MODE_IRQORPOLL 1
#define UART_UCON_MODE_DMA0 2
#define UART_UCON_MODE_DMA1 3

#define UART_CLOCK_PCLK 0
#define UART_CLOCK_EXT_UCLK0 1
#define UART_CLOCK_EXT_UCLK1 3

#define UART_SAMPLERATE_4 2
#define UART_SAMPLERATE_8 1
#define UART_SAMPLERATE_16 0

#define UART_UMCON_AFC_BIT 0x10
#define UART_UMCON_NRTS_BIT 0x1

#define UART_UTRSTAT_TRANSMITTEREMPTY 0x4
#define UART_UTRSTAT_RECEIVEDATAREADY 0x1

#define UART_UMSTAT_CTS 0x1

#define UART_UFSTAT_TXFIFO_FULL (0x1 << 9)
#define UART_UFSTAT_RXFIFO_FULL (0x1 << 8)
#define UART_UFSTAT_RXCOUNT_MASK 0xF

#define UART_UERSTAT_OVERRUN	(1<<0)
#define UART_UERSTAT_FRAME		(1<<2)
#define UART_UERSTAT_BREAK		(1<<3)
#define UART_UERSTAT_PARITY		(1<<1)

#define UART_5BITS 0
#define UART_6BITS 1
#define UART_7BITS 2
#define UART_8BITS 3

#define UART_POLL_MODE 0

struct UARTSettings {
	u32 ureg;
	u32 baud;
	u32 sample_rate;
	int flow_control;
	u32 mode;
	u32 clock;
};

struct UARTRegisters {
	u32 ULCON;
	u32 UCON;
	u32 UFCON;
	u32 UMCON;

	u32 UTRSTAT;
	u32 UERSTAT;
	u32 UFSTAT;
	u32 UMSTAT;

	u32 UTXH;
	u32 URXH;
	u32 UBAUD;
	u32 UDIVSLOT;
};

const struct UARTRegisters HWUarts[] = {
	{UART + UART0 + UART_ULCON, UART + UART0 + UART_UCON, UART + UART0 + UART_UFCON, 0,
		UART + UART0 + UART_UTRSTAT, UART + UART0 + UART_UERSTAT, UART + UART0 + UART_UFSTAT,
		0, UART + UART0 + UART_UTXH, UART + UART0 + UART_URXH, UART + UART0 + UART_UBAUD,
		UART + UART0 + UART_UDIVSLOT},
	{UART + UART1 + UART_ULCON, UART + UART1 + UART_UCON, UART + UART1 + UART_UFCON, UART + UART1 + UART_UMCON,
		UART + UART1 + UART_UTRSTAT, UART + UART1 + UART_UERSTAT, UART + UART1 + UART_UFSTAT,
		UART + UART1 + UART_UMSTAT, UART + UART1 + UART_UTXH, UART + UART1 + UART_URXH, UART + UART1 + UART_UBAUD,
		UART + UART1 + UART_UDIVSLOT},
	{UART + UART2 + UART_ULCON, UART + UART2 + UART_UCON, UART + UART2 + UART_UFCON, UART + UART2 + UART_UMCON,
		UART + UART2 + UART_UTRSTAT, UART + UART2 + UART_UERSTAT, UART + UART2 + UART_UFSTAT,
		UART + UART2 + UART_UMSTAT, UART + UART2 + UART_UTXH, UART + UART2 + UART_URXH, UART + UART2 + UART_UBAUD,
		UART + UART2 + UART_UDIVSLOT},
	{UART + UART3 + UART_ULCON, UART + UART3 + UART_UCON, UART + UART3 + UART_UFCON, UART + UART3 + UART_UMCON,
		UART + UART3 + UART_UTRSTAT, UART + UART3 + UART_UERSTAT, UART + UART3 + UART_UFSTAT,
		UART + UART3 + UART_UMSTAT, UART + UART3 + UART_UTXH, UART + UART3 + UART_URXH, UART + UART3 + UART_UBAUD,
		UART + UART3 + UART_UDIVSLOT},
	{UART + UART4 + UART_ULCON, UART + UART4 + UART_UCON, UART + UART4 + UART_UFCON, UART + UART4 + UART_UMCON,
		UART + UART4 + UART_UTRSTAT, UART + UART4 + UART_UERSTAT, UART + UART4 + UART_UFSTAT,
		UART + UART4 + UART_UMSTAT, UART + UART4 + UART_UTXH, UART + UART4 + UART_URXH, UART + UART4 + UART_UBAUD,
		UART + UART4 + UART_UDIVSLOT}};

static struct UARTSettings UARTs[5];

struct iphone_uart_info
{
	struct uart_port port;
	int ureg;
	int tx_enabled;
	int rx_enabled;
};

static void iphone_uart_stop_tx(struct uart_port *port);

static int iphone_uart_set_baud_rate(int ureg, u32 baud)
{
	u32 clockFrequency;
	u32 div_val;

	if(ureg > 4)
		return -1; // Invalid ureg

	if(UARTs[ureg].sample_rate == 0 || baud == 0)
		return -1;

	//u32 clockFrequency = (UARTs[ureg].clock == UART_CLOCK_PCLK) ? PeripheralFrequency : FixedFrequency;
	// FIXME: Hardwired to fixed frequency
	clockFrequency = FREQUENCY_FIXED;
	div_val = clockFrequency / (baud * UARTs[ureg].sample_rate) - 1;

	__raw_writel((__raw_readl(HWUarts[ureg].UBAUD) & (~UART_DIVVAL_MASK)) | div_val, HWUarts[ureg].UBAUD);

	// vanilla iBoot also does a reverse calculation from div_val and solves for baud and reports
	// the "actual" baud rate, or what is after loss during integer division

	UARTs[ureg].baud = baud;

	return 0;
}

static int iphone_uart_set_clk(int ureg, int clock) {
	if(ureg > 4)
		return -1; // Invalid ureg

	if(clock != UART_CLOCK_PCLK && clock != UART_CLOCK_EXT_UCLK0 && clock != UART_CLOCK_EXT_UCLK1) {
		return -1; // Invalid clock
	}

	__raw_writel((__raw_readl(HWUarts[ureg].UCON) & (~UART_CLOCK_SELECTION_MASK)) | (clock << UART_CLOCK_SELECTION_SHIFT), HWUarts[ureg].UCON);

	UARTs[ureg].clock = clock;
	iphone_uart_set_baud_rate(ureg, UARTs[ureg].baud);

	return 0;
}

static int iphone_uart_set_sample_rate(int ureg, int rate) {
	u32 newSampleRate;

	if(ureg > 4)
		return -1; // Invalid ureg

	switch(rate) {
		case 4:
			newSampleRate = UART_SAMPLERATE_4;
			break;
		case 8:
			newSampleRate = UART_SAMPLERATE_8;
			break;
		case 16:
			newSampleRate = UART_SAMPLERATE_16;
			break;
		default:
			return -1; // Invalid sample rate
	}

	__raw_writel((__raw_readl(HWUarts[ureg].UBAUD) & (~UART_SAMPLERATE_MASK)) | (newSampleRate << UART_SAMPLERATE_SHIFT), HWUarts[ureg].UBAUD);

	UARTs[ureg].sample_rate = rate;
	iphone_uart_set_baud_rate(ureg, UARTs[ureg].baud);

	return 0;
}

static int iphone_uart_set_flow_control(int ureg, int flow_control) {
	if(ureg > 4)
		return -1; // Invalid ureg

	if(flow_control == 1) {
		if(ureg == 0)
			return -1; // uart0 does not support flow control

		__raw_writel(UART_UMCON_AFC_BIT, HWUarts[ureg].UMCON);
	} else {
		if(ureg != 0) {
			__raw_writel(UART_UMCON_NRTS_BIT, HWUarts[ureg].UMCON);
		}
	}

	UARTs[ureg].flow_control = flow_control;

	return 0;
}

static int iphone_uart_set_mode(int ureg, u32 mode) {
	if(ureg > 4)
		return -1; // Invalid ureg

	UARTs[ureg].mode = mode;

	if(mode == UART_POLL_MODE) {
		// Setup some defaults, like no loopback mode
		__raw_writel(__raw_readl(HWUarts[ureg].UCON) & (~UART_UCON_UNKMASK) & (~UART_UCON_UNKMASK) & (~UART_UCON_LOOPBACKMODE), HWUarts[ureg].UCON);

		// Use polling mode
		__raw_writel((__raw_readl(HWUarts[ureg].UCON) & (~UART_UCON_RXMODE_MASK) & (~UART_UCON_TXMODE_MASK))
			| (UART_UCON_MODE_IRQORPOLL << UART_UCON_RXMODE_SHIFT)
			| (UART_UCON_MODE_IRQORPOLL << UART_UCON_TXMODE_SHIFT), HWUarts[ureg].UCON);
	}

	return 0;
}

static int iphone_uart_set_bits(int ureg, int bits) {
	if(ureg > 4)
		return -1; // Invalid ureg

	switch(bits) {
		case 8:
			__raw_writel(UART_8BITS, HWUarts[ureg].ULCON);
			break;
		case 7:
			__raw_writel(UART_7BITS, HWUarts[ureg].ULCON);
			break;
		case 6:
			__raw_writel(UART_6BITS, HWUarts[ureg].ULCON);
			break;
		case 5:
			__raw_writel(UART_5BITS, HWUarts[ureg].ULCON);
			break;
		default:
			return -1;
	}

	return 0;
}

static int iphone_uart_setup(void) {
	int i;

	iphone_clock_gate_switch(UART_CLOCKGATE, 1);

	for(i = 0; i < NUM_UARTS; i++) {
		// set all uarts to transmit 8 bit frames, one stop bit per frame, no parity, no infrared mode
		__raw_writel(UART_8BITS, HWUarts[i].ULCON);

		// set all uarts to use polling for rx/tx, no breaks, no loopback, no error status interrupts,
		// no timeouts, pulse interrupts for rx/tx, peripheral clock. Basically, the defaults.
		__raw_writel((UART_UCON_MODE_IRQORPOLL << UART_UCON_RXMODE_SHIFT) | (UART_UCON_MODE_IRQORPOLL << UART_UCON_TXMODE_SHIFT), HWUarts[i].UCON);

		// Initialize the settings array a bit so the helper functions can be used properly
		UARTs[i].ureg = i;
		UARTs[i].baud = 115200;

		iphone_uart_set_clk(i, UART_CLOCK_EXT_UCLK0);
		iphone_uart_set_sample_rate(i, 16);
	}

	// Set flow control
	iphone_uart_set_flow_control(0, 0);
	iphone_uart_set_flow_control(1, 1);
	iphone_uart_set_flow_control(2, 1);
	iphone_uart_set_flow_control(3, 1);
	iphone_uart_set_flow_control(4, 0);

	// Reset and enable fifo
	for(i = 0; i < NUM_UARTS; i++) {
		__raw_writel(UART_FIFO_RESET_TX | UART_FIFO_RESET_RX, HWUarts[i].UFCON);
		__raw_writel(UART_FIFO_ENABLE, HWUarts[i].UFCON);
	}

	for(i = 0; i < NUM_UARTS; i++) {
		iphone_uart_set_mode(i, UART_POLL_MODE);
	}

	return 0;
}

void iphone_uart_enable_tx_irq(struct iphone_uart_info* info)
{
	const struct UARTRegisters* uart = &HWUarts[info->ureg];
	__raw_writel(__raw_readl(uart->UCON) | (1 << 13), uart->UCON);
}

void iphone_uart_enable_rx_irq(struct iphone_uart_info* info)
{
	const struct UARTRegisters* uart = &HWUarts[info->ureg];
	__raw_writel(__raw_readl(uart->UCON) | (1 << 12) | (1 << 7) | (1 << 11), uart->UCON);
}

void iphone_uart_disable_tx_irq(struct iphone_uart_info* info)
{
	const struct UARTRegisters* uart = &HWUarts[info->ureg];
	__raw_writel(__raw_readl(uart->UCON) & ~(1 << 13), uart->UCON);
}

void iphone_uart_disable_rx_irq(struct iphone_uart_info* info)
{
	const struct UARTRegisters* uart = &HWUarts[info->ureg];
	__raw_writel(__raw_readl(uart->UCON) & ~((1 << 12) | (1 << 7) | (1 << 11)), uart->UCON);
}

static irqreturn_t iphone_uart_tx_chars(int irq, void* id)
{
	struct iphone_uart_info* info = id;
	struct uart_port* port = &info->port;
	struct circ_buf *xmit = &port->state->xmit;
	const struct UARTRegisters* uart = &HWUarts[info->ureg];
	int count = 256;

	if((__raw_readl(uart->UFSTAT) & UART_UFSTAT_TXFIFO_FULL) != 0)
		goto out;

	if(port->x_char)
	{
		__raw_writel(port->x_char, uart->UTXH);
		port->icount.tx++;
		port->x_char = 0;
		goto out;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(port))
	{
		iphone_uart_stop_tx(port);	/* no-op for us */
		goto out;
	}

	while (!uart_circ_empty(xmit) && count-- > 0)
	{
		// if the tx fifo buffer is full
		if((__raw_readl(uart->UFSTAT) & UART_UFSTAT_TXFIFO_FULL) != 0)
			break;

		__raw_writel(xmit->buf[xmit->tail], uart->UTXH);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit))
		iphone_uart_stop_tx(port);	/* no-op for us */

out:
	return IRQ_HANDLED;

}

static irqreturn_t iphone_uart_rx_chars(int irq, void* id)
{
	struct iphone_uart_info* info = id;
	struct uart_port* port = &info->port;
	struct tty_struct *tty = port->state->port.tty;
	const struct UARTRegisters* uart = &HWUarts[info->ureg];
	int count = 64;

	while (count-- > 0)
	{
		u32 ufcon = __raw_readl(uart->UFCON);
		u32 ufstat = __raw_readl(uart->UFSTAT);
		u32 uerstat;
		u32 flag;
		u8 ch;

		if(((ufstat & UART_UFSTAT_RXFIFO_FULL) | (ufstat & UART_UFSTAT_RXCOUNT_MASK)) == 0)
			break;

		uerstat = __raw_readl(uart->UERSTAT);
		ch = __raw_readl(uart->URXH);

		if (port->flags & UPF_CONS_FLOW)
		{
			int txe = __raw_readl(uart->UTRSTAT) & UART_UTRSTAT_TRANSMITTEREMPTY;

			if(info->rx_enabled)
			{
				if (!txe)
				{
					info->rx_enabled = 0;
					continue;
				}
			} else
			{
				if (txe)
				{
					ufcon |= UART_FIFO_RESET_RX;
					__raw_writel(ufcon, uart->UFCON);
					info->rx_enabled = 1;
					goto out;
				}
			}
		}

		/* insert the character into the buffer */

		flag = TTY_NORMAL;
		port->icount.rx++;

		if(unlikely(uerstat != 0))
		{
			/* check for break */
			if (uerstat & UART_UERSTAT_BREAK)
			{
				port->icount.brk++;
				if(uart_handle_break(port))
				    goto ignore_char;
			}

			if (uerstat & UART_UERSTAT_FRAME)
				port->icount.frame++;

			if (uerstat & UART_UERSTAT_OVERRUN)
				port->icount.overrun++;

			uerstat &= port->read_status_mask;

			if (uerstat & UART_UERSTAT_BREAK)
				flag = TTY_BREAK;

			else if (uerstat & UART_UERSTAT_PARITY)
				flag = TTY_PARITY;

			else if (uerstat & (UART_UERSTAT_FRAME |
					    UART_UERSTAT_OVERRUN))
				flag = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(port, ch))
			goto ignore_char;

		uart_insert_char(port, uerstat, UART_UERSTAT_OVERRUN,
				 ch, flag);

 ignore_char:
		continue;
	}

	tty_flip_buffer_push(tty);

 out:
	return IRQ_HANDLED;
}

static irqreturn_t iphone_uart_handle_irq(int irq, void* id)
{
	struct iphone_uart_info* info = id;
	const struct UARTRegisters* uart = &HWUarts[info->ureg];
	u32 pending = __raw_readl(uart->UTRSTAT);
	__raw_writel(pending, uart->UTRSTAT);

	pr_debug("iphone_uart_handle_irq: %x %x\n", __raw_readl(uart->UCON), pending >> 4);

	if((pending & (1 << 4)) || (pending & (1 << 3)))
		iphone_uart_rx_chars(irq, info);

	if(pending & (1 << 5))
		iphone_uart_tx_chars(irq, info);

	return IRQ_HANDLED;
}

/**
 * iphone_uart_type - What type of console are we?
 * @port: Port to operate with (we ignore since we only have one port)
 *
 */
static const char *iphone_uart_type(struct uart_port *port)
{
	return ("iPhone Serial");
}

/**
 * iphone_uart_tx_empty - Is the transmitter empty?  We pretend we're always empty
 * @port: Port to operate on (we ignore since we only have one port)
 *
 */
static unsigned int iphone_uart_tx_empty(struct uart_port *port)
{
	struct iphone_uart_info* info = (struct iphone_uart_info*) port;
	const struct UARTRegisters* uart = &HWUarts[info->ureg];

	if((__raw_readl(uart->UFSTAT) & UART_UFSTAT_TXFIFO_FULL) != 0)
		return 0;
	else
		return 1;
}

/**
 * iphone_uart_stop_tx - stop the transmitter - no-op for us
 * @port: Port to operat eon - we ignore - no-op function
 *
 */
static void iphone_uart_stop_tx(struct uart_port *port)
{
	struct iphone_uart_info* info = (struct iphone_uart_info*) port;

	if(info->tx_enabled)
	{
		iphone_uart_disable_tx_irq(info);
		info->tx_enabled = 0;
		if(port->flags & UPF_CONS_FLOW)
		{
			const struct UARTRegisters* uart = &HWUarts[info->ureg];
			unsigned long flags;
			int count = 10000;

			spin_lock_irqsave(&port->lock, flags);

			while (--count && !(__raw_readl(uart->UTRSTAT) & UART_UTRSTAT_TRANSMITTEREMPTY))
				udelay(100);

			__raw_writel(__raw_readl(uart->UFCON) | UART_FIFO_RESET_RX, uart->UFCON);

			__raw_writel((__raw_readl(uart->UCON) & (~UART_UCON_RXMODE_MASK))
					| (UART_UCON_MODE_IRQORPOLL << UART_UCON_RXMODE_SHIFT), uart->UCON);

			info->rx_enabled = 1;

			spin_unlock_irqrestore(&port->lock, flags);
		}
	}
}

/**
 * iphone_uart_release_port - Free i/o and resources for port - no-op for us
 * @port: Port to operate on - we ignore - no-op function
 *
 */
static void iphone_uart_release_port(struct uart_port *port)
{
}

/**
 * iphone_uart_enable_ms - Force modem status interrupts on - no-op for us
 * @port: Port to operate on - we ignore - no-op function
 *
 */
static void iphone_uart_enable_ms(struct uart_port *port)
{
}

/**
 * iphone_uart_shutdown - shut down the port - free irq and disable - no-op for us
 * @port: Port to shut down - we ignore
 *
 */
static void iphone_uart_shutdown(struct uart_port *port)
{
	struct iphone_uart_info* info = (struct iphone_uart_info*) port;
	const struct UARTRegisters* uart = &HWUarts[info->ureg];

	// mask all irqs
	__raw_writel(__raw_readl(uart->UCON) & ~(0xF << 12), uart->UCON);

	free_irq(0x18 + info->ureg, info);
}

/**
 * iphone_uart_set_mctrl - set control lines (dtr, rts, etc) - no-op for our console
 * @port: Port to operate on - we ignore
 * @mctrl: Lines to set/unset - we ignore
 *
 */
static void iphone_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

/**
 * iphone_uart_get_mctrl - get control line info, we just return a static value
 * @port: port to operate on - we only have one port so we ignore this
 *
 */
static unsigned int iphone_uart_get_mctrl(struct uart_port *port)
{
	struct iphone_uart_info* info = (struct iphone_uart_info*) port;
	const struct UARTRegisters* uart = &HWUarts[info->ureg];

	if(__raw_readl(uart->UMSTAT) & UART_UMSTAT_CTS)
		return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
	else
		return TIOCM_CAR | TIOCM_DSR;
}

/**
 * iphone_uart_stop_rx - Stop the receiver - we ignor ethis
 * @port: Port to operate on - we ignore
 *
 */
static void iphone_uart_stop_rx(struct uart_port *port)
{
	struct iphone_uart_info* info = (struct iphone_uart_info*) port;

	if(info->rx_enabled)
	{
		iphone_uart_disable_rx_irq(info);
		info->rx_enabled = 0;
	}
}

/**
 * iphone_uart_start_tx - Start transmitter
 * @port: Port to operate on
 *
 */
static void iphone_uart_start_tx(struct uart_port *port)
{
	struct iphone_uart_info* info = (struct iphone_uart_info*) port;

	if(!info->tx_enabled)
	{
		unsigned long flags;
		if(port->flags & UPF_CONS_FLOW)
		{
			const struct UARTRegisters* uart = &HWUarts[info->ureg];
			unsigned long flags;

			spin_lock_irqsave(&port->lock, flags);

			__raw_writel(__raw_readl(uart->UCON) & ~~UART_UCON_RXMODE_MASK, uart->UCON);

			info->rx_enabled = 0;
			spin_unlock_irqrestore(&port->lock, flags);
		}

		info->tx_enabled = 1;
		local_irq_save(flags);
		iphone_uart_enable_tx_irq(info);
		iphone_uart_tx_chars(0, info);
		local_irq_restore(flags);
	}
}

/**
 * iphone_uart_break_ctl - handle breaks - ignored by us
 * @port: Port to operate on
 * @break_state: Break state
 *
 */
static void iphone_uart_break_ctl(struct uart_port *port, int break_state)
{
}

/**
 * iphone_uart_startup - Start up the serial port - always return 0 (We're always on)
 * @port: Port to operate on
 *
 */
static int iphone_uart_startup(struct uart_port *port)
{
	struct iphone_uart_info* info = (struct iphone_uart_info*) port;
	const struct UARTRegisters* uart = &HWUarts[info->ureg];
	int ret;

	info->rx_enabled = 1;
	info->tx_enabled = 0;

	// mask all irqs
	__raw_writel(__raw_readl(uart->UCON) & ~(0xF << 12), uart->UCON);

	ret = request_irq(0x18 + info->ureg, iphone_uart_handle_irq, IRQF_DISABLED, "iphone_uart", info);
	if(ret)
	{
		pr_debug(KERN_ERR "error getting irq for uart %d\n", info->ureg);
		return ret;
	}

	iphone_uart_enable_rx_irq(info);

	return 0;
}

/**
 * iphone_uart_set_termios - set termios stuff - we ignore these
 * @port: port to operate on
 * @termios: New settings
 * @termios: Old
 *
 */
static void
iphone_uart_set_termios(struct uart_port *port, struct ktermios *termios,
		struct ktermios *old)
{
	struct iphone_uart_info* info = (struct iphone_uart_info*) port;
	const struct UARTRegisters* uart = &HWUarts[info->ureg];
	unsigned int baud;
	unsigned long flags;

	termios->c_cflag &= ~(HUPCL | CMSPAR);
	termios->c_cflag |= CLOCAL;

	baud = uart_get_baud_rate(port, termios, old, 0, 4000000 * 8);

	// awful, awful hack because Linux doesn't support our baud rate
	if(baud == 921600)
		baud = 750000;

	pr_debug("uart %d:\n", info->ureg);

	spin_lock_irqsave(&port->lock, flags);

	iphone_uart_set_baud_rate(info->ureg, baud);
	pr_debug("\t%d baud\n", baud);

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		pr_debug("\t5-bit\n");
		iphone_uart_set_bits(info->ureg, 5);
		break;
	case CS6:
		pr_debug("\t6-bit\n");
		iphone_uart_set_bits(info->ureg, 6);
		break;
	case CS7:
		pr_debug("\t7-bit\n");
		iphone_uart_set_bits(info->ureg, 7);
		break;
	case CS8:
	default:
		pr_debug("\t8-bit\n");
		iphone_uart_set_bits(info->ureg, 8);
		break;
	}

	if(termios->c_cflag & CRTSCTS)
	{
		iphone_uart_set_flow_control(info->ureg, 1);
		pr_debug("\tflow control\n");
	} else
	{
		iphone_uart_set_flow_control(info->ureg, 0);
		pr_debug("\tno flow control\n");
	}

	if (termios->c_cflag & PARENB) {
		if (termios->c_cflag & PARODD)
		{
			__raw_writel(__raw_readl(uart->ULCON) | (4 << 3), uart->ULCON);
			pr_debug("\todd parity\n");
		} else
		{
			__raw_writel(__raw_readl(uart->ULCON) | (5 << 3), uart->ULCON);
			pr_debug("\teven parity\n");
		}
	} else
	{
		pr_debug("\tno parity\n");
	}

	uart_update_timeout(port, termios->c_cflag, baud);

	port->read_status_mask = UART_UERSTAT_OVERRUN;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= UART_UERSTAT_FRAME | UART_UERSTAT_PARITY;

	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= UART_UERSTAT_OVERRUN;
	if (termios->c_iflag & IGNBRK && termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= UART_UERSTAT_FRAME;

	spin_unlock_irqrestore(&port->lock, flags);
}

/**
 * iphone_uart_request_port - allocate resources for port - ignored by us
 * @port: port to operate on
 *
 */
static int iphone_uart_request_port(struct uart_port *port)
{
	return 0;
}

/**
 * iphone_uart_config_port - allocate resources, set up - we ignore,  we're always on
 * @port: Port to operate on
 * @flags: flags used for port setup
 *
 */
static void iphone_uart_config_port(struct uart_port *port, int flags)
{
}

static struct uart_ops iphone_uart_ops = {
	.tx_empty     = iphone_uart_tx_empty,
	.set_mctrl    = iphone_uart_set_mctrl,
	.get_mctrl    = iphone_uart_get_mctrl,
	.stop_tx      = iphone_uart_stop_tx,
	.start_tx     = iphone_uart_start_tx,
	.stop_rx      = iphone_uart_stop_rx,
	.enable_ms    = iphone_uart_enable_ms,
	.break_ctl    = iphone_uart_break_ctl,
	.startup      = iphone_uart_startup,
	.shutdown     = iphone_uart_shutdown,
	.type         = iphone_uart_type,
	.release_port = iphone_uart_release_port,
	.request_port = iphone_uart_request_port,
	.config_port  = iphone_uart_config_port,
	.verify_port  = NULL,
	.set_termios  = iphone_uart_set_termios,
};

struct uart_driver iphone_reg = {
	.owner        = THIS_MODULE,
	.driver_name  = "iphone_serial",
	.dev_name     = "ttyS",
	.major        = TTY_MAJOR,
	.minor        = 64,
	.nr           = NUM_UARTS,
};

static struct iphone_uart_info iphone_uart_port[NUM_UARTS];

static int __init iphone_uart_moduleinit(void)
{
	int i;

	uart_register_driver(&iphone_reg);

	iphone_uart_setup();

	for(i = 0; i < NUM_UARTS; i++)
	{
		struct uart_port* port = &iphone_uart_port[i].port;
		const struct UARTRegisters* uart = &HWUarts[i];

		spin_lock_init(&port->lock);

		iphone_uart_port[i].ureg = i;
		iphone_uart_port[i].rx_enabled = 0;
		iphone_uart_port[i].tx_enabled = 0;

		/* Setup the port struct with the minimum needed */
		port->membase = (char*) uart->ULCON;	/* just needs to be non-zero */
		port->type = PORT_S3C6400;
		port->fifosize = 10;
		port->ops = &iphone_uart_ops;
		port->line = i;

		uart_add_one_port(&iphone_reg, (struct uart_port*) &iphone_uart_port[i]);
	}

	return 0;
}

module_init(iphone_uart_moduleinit);

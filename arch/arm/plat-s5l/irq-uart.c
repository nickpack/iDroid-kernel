/*
 * Copyright (c) 2012 Alexey Makhalov (makhaloff@gmail.com).
 *
 * S5L - UART Interrupt handling (Software demuxing)
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/serial_core.h>
#include <linux/irq.h>
#include <linux/io.h>

#include <mach/map.h>
#include <plat/irq-uart.h>
#include <plat/regs-uart.h>
#include <plat/cpu.h>

#define SOFTWARE_RX_TIMEOUT

#ifdef SOFTWARE_RX_TIMEOUT
/* TODO: each timer for each uart channel */
static struct timer_list rx_timeout_timer;

static void rx_timeout_timer_handler(unsigned long data)
{
	struct s5l_uart_irq *uirq = (struct s5l_uart_irq*) data;
	struct irq_desc *desc = irq_to_desc(uirq->base_irq);

	del_timer(&rx_timeout_timer);

	if (desc) {
		struct irqaction *action = desc->action;
		action->handler(uirq->base_irq, action->dev_id);
	}

	__raw_writel(__raw_readl(uirq->regs + S5L8930_UCON) |
			S5L8930_UCON_RXSIRQMASK, uirq->regs + S5L8930_UCON);
}
#endif
/* Note, we make use of the fact that the parent IRQs, IRQ_UART[0..3]
 * are consecutive when looking up the interrupt in the demux routines.
 */
static void s5l_irq_demux_uart(unsigned int irq, struct irq_desc *desc)
{
	struct s5l_uart_irq *uirq = desc->irq_data.handler_data;
	u32 utrstat = __raw_readl(uirq->regs + S5L8930_UTRSTAT);
	int base = uirq->base_irq;

	if (utrstat & S5L8930_UTRSTAT_RXIRQ && utrstat & S5L8930_UTRSTAT_RXDR) {
#ifdef SOFTWARE_RX_TIMEOUT
		del_timer(&rx_timeout_timer);
		generic_handle_irq(base);
		__raw_writel(__raw_readl(uirq->regs + S5L8930_UCON) |
				S5L8930_UCON_RXSIRQMASK, uirq->regs + S5L8930_UCON);
#else
		generic_handle_irq(base);
#endif
	}
	if (utrstat & S5L8930_UTRSTAT_TXIRQ)
		generic_handle_irq(base + 2);
#ifdef SOFTWARE_RX_TIMEOUT
	if (utrstat & S5L8930_UTRSTAT_RXSIRQ) {
		/* ack */
		__raw_writel(utrstat, uirq->regs + S5L8930_UTRSTAT);
		/* 1/16 sec timeout */
		rx_timeout_timer.expires = jiffies + (HZ >> 4);
		add_timer(&rx_timeout_timer);
	}
#endif
}

static inline void s5l_irq_uart_ack(struct irq_data *data)
{
	struct s5l_uart_irq *uirq = (struct s5l_uart_irq*) data->chip_data;
	u32 utrstat;

	utrstat = __raw_readl(uirq->regs + S5L8930_UTRSTAT);
	__raw_writel(utrstat, uirq->regs + S5L8930_UTRSTAT);
}

static inline void s5l_irq_uart_mask(struct irq_data *data)
{
	struct s5l_uart_irq *uirq = (struct s5l_uart_irq*) data->chip_data;
	u32 ucon, mask;

	if (data->irq - uirq->base_irq == 0)
#ifdef SOFTWARE_RX_TIMEOUT
		mask = ~(S5L8930_UCON_RXIRQEN | S5L8930_UCON_RXS);
#else
		mask = ~S5L8930_UCON_RXIRQEN;
#endif
	else if (data->irq - uirq->base_irq == 2)
		mask = ~S5L8930_UCON_TXIRQEN;
	else
		return;

	ucon = __raw_readl(uirq->regs + S5L8930_UCON);
	__raw_writel(ucon & mask, uirq->regs + S5L8930_UCON);
}

static void s5l_irq_uart_unmask(struct irq_data *data)
{
	struct s5l_uart_irq *uirq = (struct s5l_uart_irq*) data->chip_data;
	u32 ucon, mask;

	if (data->irq - uirq->base_irq == 0)
#ifdef SOFTWARE_RX_TIMEOUT
		mask = S5L8930_UCON_RXIRQEN | S5L8930_UCON_RXS;
#else
		mask = S5L8930_UCON_RXIRQEN;
#endif
	else if (data->irq - uirq->base_irq == 2)
		mask = S5L8930_UCON_TXIRQEN;
	else
		return;

	ucon = __raw_readl(uirq->regs + S5L8930_UCON);
	__raw_writel(ucon | mask, uirq->regs + S5L8930_UCON);
}

static struct irq_chip s5l_irq_uart = {
	.name		= "s5l-uart",
	.irq_mask	= s5l_irq_uart_mask,
	.irq_unmask	= s5l_irq_uart_unmask,
//	.irq_mask_ack	= s5l_irq_uart_maskack,
	.irq_ack	= s5l_irq_uart_ack,
//	.irq_set_type	= s5l_irq_uart_set_type,
#ifdef CONFIG_PM
//	.irq_set_wake	= s3c_irqext_wake,
#endif
};
static void __init s5l_init_uart_irq(struct s5l_uart_irq *uirq)
{
	u32 ucon;

	/* mask all interrupts at the start. */
	ucon = __raw_readl(uirq->regs + S5L8930_UCON);
	__raw_writel(ucon & ~(S5L8930_UCON_RXIRQEN | S5L8930_UCON_TXIRQEN |
				S5L8930_UCON_RXS),
			uirq->regs + S5L8930_UCON);

	irq_set_chip_and_handler(uirq->base_irq, &s5l_irq_uart, handle_level_irq);
	set_irq_flags(uirq->base_irq, IRQF_VALID);
	irq_set_chip_data(uirq->base_irq, uirq);

	irq_set_chip_and_handler(uirq->base_irq + 2, &s5l_irq_uart, handle_level_irq);
	set_irq_flags(uirq->base_irq + 2, IRQF_VALID);
	irq_set_chip_data(uirq->base_irq + 2, uirq);

	irq_set_handler_data(uirq->parent_irq, uirq);
	irq_set_chained_handler(uirq->parent_irq, s5l_irq_demux_uart);
}

/**
 * s5l_init_uart_irqs() - initialise UART IRQs and the necessary demuxing
 * @irq: The interrupt data for registering
 * @nr_irqs: The number of interrupt descriptions in @irq.
 *
 * Register the UART interrupts specified by @irq including the demuxing
 * routines. This supports the S5L8930 based devices.
 */
void __init s5l_init_uart_irqs(struct s5l_uart_irq *irq, unsigned int nr_irqs)
{
#ifdef SOFTWARE_RX_TIMEOUT
	init_timer(&rx_timeout_timer);
	rx_timeout_timer.data = (unsigned long)irq;
	rx_timeout_timer.function = rx_timeout_timer_handler;
#endif

	for (; nr_irqs > 0; nr_irqs--, irq++)
		s5l_init_uart_irq(irq);
}

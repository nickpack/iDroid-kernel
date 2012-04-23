/*
 * Copyright (c) 2012 Alexey Makhalov (makhaloff@gmail.com).
 *
 * S5L - Interrupt Initialization
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <plat/irq.h>
#include <asm/hardware/vic.h>
#include <linux/io.h>

#include <linux/serial_core.h>
#include <mach/map.h>
#include <plat/regs-timer.h>
#include <plat/regs-serial.h>
#include <plat/cpu.h>
#include <plat/irq-vic-timer.h>
#include <plat/irq-uart.h>

static struct s5l_uart_irq uart_irqs[] = {
	[0] = {
		.regs		= VA_UART0,
		.base_irq	= IRQ_S5L_UART_BASE0,
		.parent_irq	= IRQ_UART0,
	},
	[1] = {
		.regs		= VA_UART1,
		.base_irq	= IRQ_S5L_UART_BASE1,
		.parent_irq	= IRQ_UART1,
	},
	[2] = {
		.regs		= VA_UART2,
		.base_irq	= IRQ_S5L_UART_BASE2,
		.parent_irq	= IRQ_UART2,
	},
};

void s5l_init_irq(u32 *vic, u32 num_vic)
{
#ifdef CONFIG_ARM_VIC
	int irq;
	for(irq = 0; irq < num_vic; irq++)
		vic_init(VA_VIC(irq), VIC_BASE(irq), vic[irq], 0);
#endif

/*	s3c_init_vic_timer_irq(1, 11);*/
	s5l_init_uart_irqs(uart_irqs, ARRAY_SIZE(uart_irqs));
}

/*
 * Copyright (c) 2012 Alexey Makhalov (makhaloff@gmail.com).
 *
 * Based on arch/arm/plat-samsung/include/plat/irq-uart.h
 *
 * Internal header file for Samsung S5L8930 serial ports (UART0-3)
 * Header file for Samsung SoC UART IRQ demux for S5L8930
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct s5l_uart_irq {
	void __iomem	*regs;
	unsigned int	 base_irq;
	unsigned int	 parent_irq;
};

extern void s5l_init_uart_irqs(struct s5l_uart_irq *irq, unsigned int nr_irqs);


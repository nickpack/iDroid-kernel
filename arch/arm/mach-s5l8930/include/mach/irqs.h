/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  _S5L8930_IRQS_
#define  _S5L8930_IRQS_

#include <plat/irqs.h>

#define IRQ_TIMER	0x11

#define IRQ_TIMER0	700
#define IRQ_TIMER1	701
#define IRQ_TIMER2	702
#define IRQ_TIMER3	703
#define IRQ_TIMER4	704

#define IRQ_IIC		0x13
#define IRQ_IIC1	0x14
#define IRQ_IIC2	0x15

#define IRQ_OTG		0xD

#define IRQ_UART0	0x16
#define IRQ_UART1	0x17
#define IRQ_UART2	0x18
#define IRQ_UART3	0x19

#define NR_IRQS		(32*4)

#define IRQ_EINT(x)			(x)
#define EINT_OFFSET(irq)	(irq)
#define IRQ_EINT_BIT(x)		EINT_OFFSET(x)

#endif //_S5L8930_IRQS_

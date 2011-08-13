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

#define IRQ_TIMER0	300
#define IRQ_TIMER1	301
#define IRQ_TIMER2	302
#define IRQ_TIMER3	303
#define IRQ_TIMER4	304

#define IRQ_IIC		0x13
#define IRQ_IIC1	0x14
#define IRQ_IIC2	0x15

#define IRQ_OTG		0xD

#define IRQ_GPIO	0x74

#define IRQ_UART0	0x16
#define IRQ_UART1	0x17
#define IRQ_UART2	0x18
#define IRQ_UART3	0x19

#define IRQ_SPI0 0x1D
#define IRQ_SPI1 0x1E

#define S5L_SUBIRQ_START	(32*4) + 176
#define S5L_SUBIRQ(x)		(S5L_SUBIRQ_START + (x))

#define NR_IRQS				S5L_SUBIRQ_START + 100 // 100 'spare' IRQs for software demuxing

#define IRQ_EINT(x)			((32*4) + (x))
#define EINT_OFFSET(irq)	((irq)/32)
#define IRQ_EINT_BIT(x)		((x)&0x1f)

#endif //_S5L8930_IRQS_

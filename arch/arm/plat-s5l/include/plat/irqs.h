/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * Portions Copyright (c) 2009 Samsung Electronics Co., Ltd.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  _S5L_IRQS_
#define  _S5L_IRQS_

#define S5L_SUBIRQ_START	(4*32) // We have 4 VICs.
#define S5L_SUBIRQ(x)		(S5L_SUBIRQ_START + (x))

/* UART interrupts, each UART has 4 intterupts per channel so
 * use the space between the ISA and S3C main interrupts. Note, these
 * are not in the same order as the S3C24XX series! */

#define IRQ_S5L_UART_BASE0	S5L_SUBIRQ(16)
#define IRQ_S5L_UART_BASE1	S5L_SUBIRQ(20)
#define IRQ_S5L_UART_BASE2	S5L_SUBIRQ(24)
#define IRQ_S5L_UART_BASE3	S5L_SUBIRQ(28)

#define UART_IRQ_RXD		(0)
#define UART_IRQ_ERR		(1)
#define UART_IRQ_TXD		(2)

#define IRQ_S5L_UART_RX0	(IRQ_S5L_UART_BASE0 + UART_IRQ_RXD)
#define IRQ_S5L_UART_TX0	(IRQ_S5L_UART_BASE0 + UART_IRQ_TXD)
#define IRQ_S5L_UART_ERR0	(IRQ_S5L_UART_BASE0 + UART_IRQ_ERR)

#define IRQ_S5L_UART_RX1	(IRQ_S5L_UART_BASE1 + UART_IRQ_RXD)
#define IRQ_S5L_UART_TX1	(IRQ_S5L_UART_BASE1 + UART_IRQ_TXD)
#define IRQ_S5L_UART_ERR1	(IRQ_S5L_UART_BASE1 + UART_IRQ_ERR)

#define IRQ_S5L_UART_RX2	(IRQ_S5L_UART_BASE2 + UART_IRQ_RXD)
#define IRQ_S5L_UART_TX2	(IRQ_S5L_UART_BASE2 + UART_IRQ_TXD)
#define IRQ_S5L_UART_ERR2	(IRQ_S5L_UART_BASE2 + UART_IRQ_ERR)

#define IRQ_S5L_UART_RX3	(IRQ_S5L_UART_BASE3 + UART_IRQ_RXD)
#define IRQ_S5L_UART_TX3	(IRQ_S5L_UART_BASE3 + UART_IRQ_TXD)
#define IRQ_S5L_UART_ERR3	(IRQ_S5L_UART_BASE3 + UART_IRQ_ERR)

/* S3C compatibilty defines */
#define IRQ_S3CUART_RX0		IRQ_S5L_UART_RX0
#define IRQ_S3CUART_RX1		IRQ_S5L_UART_RX1
#define IRQ_S3CUART_RX2		IRQ_S5L_UART_RX2
#define IRQ_S3CUART_RX3		IRQ_S5L_UART_RX3

#endif //_S5L_IRQS_

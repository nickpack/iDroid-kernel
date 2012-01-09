/*
 *  arch/arm/mach-apple_iphone/irq.c
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
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/sysdev.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>

// Devices
#define EDGEIC IO_ADDRESS(0x38E02000)

// Registers
#define EDGEICCONFIG0 0x0
#define EDGEICCONFIG1 0x4
#define EDGEICLOWSTATUS 0x8
#define EDGEICHIGHSTATUS 0xC

// Values
#define EDGEIC_CONFIG0RESET 0
#define EDGEIC_CONFIG1RESET 0

// Constants

#define VIC_MaxInterrupt 0x40
#define VIC_InterruptSeparator 0x20

// Devices

#define VIC0 IO_ADDRESS(0x38E00000)
#define VIC1 IO_ADDRESS(0x38E01000)

// Registers

#define VICIRQSTATUS 0x000
#define VICRAWINTR 0x8
#define VICINTSELECT 0xC
#define VICINTENABLE 0x10
#define VICINTENCLEAR 0x14
#define VICSWPRIORITYMASK 0x24
#define VICVECTADDRS 0x100
#define VICADDRESS 0xF00
#define VICPERIPHID0 0xFE0
#define VICPERIPHID1 0xFE4
#define VICPERIPHID2 0xFE8
#define VICPERIPHID3 0xFEC

static inline void iphone_irq_eoi(struct irq_data *_data)
{
	if(_data->irq < VIC_InterruptSeparator) {
		__raw_writel(1, VIC0 + VICADDRESS);
	} else {
		__raw_writel(1, VIC1 + VICADDRESS);
	}
}

static inline void iphone_irq_mask(struct irq_data *_data)
{
	if(_data->irq < VIC_InterruptSeparator) {
		__raw_writel(__raw_readl(VIC0 + VICINTENABLE) & ~(1 << _data->irq), VIC0 + VICINTENABLE);
	} else {
		__raw_writel(__raw_readl(VIC1 + VICINTENABLE) & ~(1 << (_data->irq - VIC_InterruptSeparator)), VIC1 + VICINTENABLE);
	}
}

static inline void iphone_irq_unmask(struct irq_data *_data)
{
	if(_data->irq < VIC_InterruptSeparator) {
		__raw_writel(__raw_readl(VIC0 + VICINTENABLE) | (1 << _data->irq), VIC0 + VICINTENABLE);
	} else {
		__raw_writel(__raw_readl(VIC1 + VICINTENABLE) | (1 << (_data->irq - VIC_InterruptSeparator)), VIC1 + VICINTENABLE);
	}
}

static struct irq_chip iphone_irq_fasteoi_chip = {
	.name = "iphone_vic",
	.irq_eoi = iphone_irq_eoi,
	.irq_mask = iphone_irq_mask,
	.irq_unmask = iphone_irq_unmask,
};

void __init iphone_init_irq(void)
{
	int i;

	printk("iphone-irq: initializing\n");
	if((0xfff & (__raw_readl(VIC0 + VICPERIPHID0) | (__raw_readl(VIC0 + VICPERIPHID1) << 8) | (__raw_readl(VIC0 + VICPERIPHID2) << 16) | (__raw_readl(VIC0 + VICPERIPHID3) << 24))) != 0x192) {
		printk("iphone-irq: incorrect device id\n");
		return;
	}

	if((0xfff & (__raw_readl(VIC1 + VICPERIPHID0) | (__raw_readl(VIC1 + VICPERIPHID1) << 8) | (__raw_readl(VIC1 + VICPERIPHID2) << 16) | (__raw_readl(VIC1 + VICPERIPHID3) << 24))) != 0x192) {
		printk("iphone-irq: incorrect device id\n");
		return;
	}

	__raw_writel(EDGEIC_CONFIG0RESET, EDGEIC + EDGEICCONFIG0);
	__raw_writel(EDGEIC_CONFIG1RESET, EDGEIC + EDGEICCONFIG1);

	__raw_writel(0xFFFFFFFF, VIC0 + VICINTENCLEAR); // disable all interrupts
	__raw_writel(0xFFFFFFFF, VIC1 + VICINTENCLEAR);

	__raw_writel(0, VIC0 + VICINTSELECT); // 0 means to use IRQs, 1 means to use FIQs, so use all IRQs
	__raw_writel(0, VIC1 + VICINTSELECT);

	__raw_writel(0xffff, VIC0 + VICSWPRIORITYMASK); // unmask all 16 interrupt levels
	__raw_writel(0xffff, VIC1 + VICSWPRIORITYMASK);

	// Set interrupt vector addresses to the interrupt number. This will signal the interrupt handler to consult the handler tables
	for(i = 0; i < VIC_InterruptSeparator; i++)
	{
		__raw_writel(i, VIC0 + VICVECTADDRS + (i * 4));
		__raw_writel(VIC_InterruptSeparator + i, VIC1 + VICVECTADDRS + (i * 4));

		irq_set_chip(i, &iphone_irq_fasteoi_chip);
		irq_set_handler(i, handle_fasteoi_irq);
		set_irq_flags(i, IRQF_VALID);

		irq_set_chip(VIC_InterruptSeparator + i, &iphone_irq_fasteoi_chip);
		irq_set_handler(VIC_InterruptSeparator + i, handle_fasteoi_irq);
		set_irq_flags(VIC_InterruptSeparator + i, IRQF_VALID);
	}

	printk("iphone-irq: finished initialization\n");
}


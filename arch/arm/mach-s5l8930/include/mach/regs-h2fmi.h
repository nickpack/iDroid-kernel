/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  __S5L8930_REGS_H2FMI__
#define  __S5L8930_REGS_H2FMI__

// Controller regs
#define H2FMI_ECCFMT		(0x0)
#define H2FMI_CCMD			(0x4)
#define H2FMI_UNK8			(0x8)
#define H2FMI_CSTS			(0xC)
#define H2FMI_CREQ			(0x10)
#define H2FMI_DATA0			(0x14)
#define H2FMI_DATA1			(0x18)
#define H2FMI_UNKREG16		(0x1C)
#define H2FMI_PAGEFMT		(0x34)

// Flash regs
#define H2FMI_RESET			(0x0)
#define H2FMI_TIMING		(0x8)
#define H2FMI_CHIP_MASK		(0xC)
#define H2FMI_NREQ			(0x10)
#define H2FMI_NCMD			(0x14)
#define H2FMI_ADDR0			(0x18)
#define H2FMI_ADDR1			(0x1C)
#define H2FMI_ADDRMODE		(0x20)
#define H2FMI_UNKREG8		(0x24)
#define H2FMI_UNK440		(0x40)
#define H2FMI_NSTS			(0x44)
#define H2FMI_STATUS		(0x48)
#define H2FMI_UNK44C		(0x4C)

// BCH regs
#define H2FMI_ECCCFG		(0x8)
#define H2FMI_ECCBUF		(0xC)
#define H2FMI_ECCSTS		(0x10)
#define H2FMI_ECCINT		(0x14)

#endif //__S5L8930_REGS_H2FMI__

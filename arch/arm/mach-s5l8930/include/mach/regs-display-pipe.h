/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  S5L_REGS_DISPLAY_PIPE_H
#define  S5L_REGS_DISPLAY_PIPE_H

// Control Block

#define S5L_DPOSIZE				(0x1030)
#define S5L_DPCTL				(0x1038)

#define S5L_DPCTL_UIxEN(x)		(1 << (8+(x)))
#define S5L_DPCTL_UI0EN			(S5L_DPCTL_UIxEN(0))
#define S5L_DPCTL_UI1EN			(S5L_DPCTL_UIxEN(1))

#define S5L_DPCPFDMA			(0x104C)

#define S5L_DPCPFDMA_AUTO		(0x10)

// Blending Block
#define S5L_DPFIFOCFG			(0x205C)
#define S5L_DPUNDERC			(0x2064)

#define S5L_DPUI0ALPH			(0x2040)

// Video Block

// UI0/2 Block
#define S5L_DPUIx(x)			(0x4000 + (0x1000*(x)))
#define S5L_DPUI0				S5L_DPUIx(0)
#define S5L_DPUI1				S5L_DPUIx(1)

#define S5L_DPUICTL				(0x40)
#define S5L_DPUIBUF				(0x44)
#define S5L_DPUILEN				(0x48)
#define S5L_DPUIUNK0			(0x50)
#define S5L_DPUISIZE			(0x60)
#define S5L_DPUIUNK1			(0x4C)
#define S5L_DPUIUNK2			(0x74)
#define S5L_DPUIUNK3			(0x78)

#endif //S5L_REGS_DISPLAY_PIPE_H

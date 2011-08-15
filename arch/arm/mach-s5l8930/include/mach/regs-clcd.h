/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  S5L8930_REGS_CLCD_H
#define  S5L8930_REGS_CLCD_H

#define S5L_CLCD_CTL						(0x00)
	
#define S5L_CLCD_CTL_RESET					(1 << 8)

#define S5L_CLCD_VIDTCON0					(0x54)
#define S5L_CLCD_VIDTCON1					(0x58)
#define S5L_CLCD_VIDTCON2					(0x5c)
#define S5L_CLCD_VIDTCON3					(0x60)

#define S5L_CLCD_VIDTCON_IVCLK_SHIFT		(3)
#define S5L_CLCD_VIDTCON_IHSYNC_SHIFT		(2)
#define S5L_CLCD_VIDTCON_IVSYNC_SHIFT		(1)
#define S5L_CLCD_VIDTCON_IVDEN_SHIFT		(0)

#define S5L_CLCD_VIDTCON_BP_SHIFT			(16)
#define S5L_CLCD_VIDTCON_BP_MASK			(0xFF)
#define S5L_CLCD_VIDTCON_BP(x)				(((x) & S5L_CLCD_VIDTCON_BP_SHIFT) << S5L_CLCD_VIDTCON_BP_SHIFT)
#define S5L_CLCD_VIDTCON_FP_SHIFT			(8)
#define S5L_CLCD_VIDTCON_FP_MASK			(0xFF)
#define S5L_CLCD_VIDTCON_FP(x)				(((x) & S5L_CLCD_VIDTCON_FP_SHIFT) << S5L_CLCD_VIDTCON_FP_SHIFT)
#define S5L_CLCD_VIDTCON_SPW_SHIFT			(0)
#define S5L_CLCD_VIDTCON_SPW_MASK			(0xFF)
#define S5L_CLCD_VIDTCON_SPW(x)				(((x) & S5L_CLCD_VIDTCON_SPW_SHIFT) << S5L_CLCD_VIDTCON_SPW_SHIFT)

#define S5L_CLCD_LINEVAL_SHIFT				(0)
#define S5L_CLCD_LINEVAL_MASK				(0x3FF)
#define S5L_CLCD_LINEVAL(x)					(((x) & S5L_CLCD_LINEVAL_MASK) << S5L_CLCD_LINEVAL_MASK)
#define S5L_CLCD_HOZVAL_SHIFT				(16)
#define S5L_CLCD_HOZVAL_MASK				(0x3FF)
#define S5L_CLCD_HOZVAL(x)					(((x) & S5L_CLCD_HOZVAL_MASK) << S5L_CLCD_HOZVAL_MASK)

#endif //S5L8930_REGS_CLCD_H

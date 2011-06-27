/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  _S5L8930_MAP_
#define  _S5L8930_MAP_

#include <asm/memory.h>

#define DO_VA_IO0(x)		(((x) & 0x0FFFFFFF) | 0xF0000000)
#define DO_VA_IO1(x)		(((x) & 0x00FFFFFF) | 0xFB000000)

#ifndef __ASSEMBLY__
#define VA_IO0(x)			((void*)(DO_VA_IO0(x)))
#define VA_IO1(x)			((void*)(DO_VA_IO1(x)))
#else
#define VA_IO0(x)			(DO_VA_IO0(x))
#define VA_IO1(x)			(DO_VA_IO1(x))
#endif

#define SZ_VIC			0x10000
#define PA_VIC(x)		(0xBF200000 + (x*SZ_VIC))
#define VA_VIC(x)		(VA_IO1(PA_VIC(x)))
#define PA_VIC0			PA_VIC(0)
#define VA_VIC0			VA_VIC(0)
#define PA_VIC1			PA_VIC(1)
#define VA_VIC1			VA_VIC(1)
#define PA_VIC2			PA_VIC(2)
#define VA_VIC2			VA_VIC(2)
#define PA_VIC3			PA_VIC(3)
#define VA_VIC3			VA_VIC(3)

#define PA_GPIO			0xBFA00000
#define VA_GPIO			(VA_IO1(PA_GPIO))
#define SZ_GPIO			0x1000

#define PA_PMGR0		0xBF100000
#define VA_PMGR0		(VA_IO1(PA_PMGR0))
#define SZ_PMGR0		0x6000
#define PA_PMGR1		0x85E00000
#define VA_PMGR1		(VA_IO0(PA_PMGR1))
#define SZ_PMGR1		0x1000
#define PA_PMGR2		0x85F00000
#define VA_PMGR2		(VA_IO0(PA_PMGR2))
#define SZ_PMGR2		0x1000
#define PA_PMGR3		0x88E00000
#define VA_PMGR3		(VA_IO0(PA_PMGR3))
#define SZ_PMGR3		0x1000
#define PA_PMGR4		0x88F00000
#define VA_PMGR4		(VA_IO0(PA_PMGR4))
#define SZ_PMGR4		0x1000
#define PA_PMGR5		0x89E00000
#define VA_PMGR5		(VA_IO0(PA_PMGR5))
#define SZ_PMGR5		0x1000
#define PA_PMGR6		0x89F00000
#define VA_PMGR6		(VA_IO0(PA_PMGR6))
#define SZ_PMGR6		0x1000

#define PA_DEBUG		0xBF701000
#define VA_DEBUG		(VA_IO1(PA_DEBUG))
#define SZ_DEBUG		0x1000

#define PA_CDMA			0x87000000
#define VA_CDMA			(VA_IO0(PA_CDMA))
#define SZ_CDMA			0x26000
#define PA_CDMA_AES		0x87800000
#define VA_CDMA_AES		(VA_IO0(PA_CDMA_AES))
#define SZ_CDMA_AES		0x9000

#define PA_DART1		0x88D00000
#define VA_DART1		(VA_IO0(PA_DART1))
#define SZ_DART1		0x2000
#define PA_DART2		0x89D00000
#define VA_DART2		(VA_IO0(PA_DART2))
#define SZ_DART2		0x2000

#define PA_SDIO			0x80000000
#define VA_SDIO			(VA_IO0(PA_SDIO))
#define SZ_SDIO			0x1000

#define PA_SHA			0x80100000
#define VA_SHA			(VA_IO0(PA_SHA))
#define SZ_SHA			0x1000

#define PA_CEATA		0x81000000
#define VA_CEATA		(VA_IO0(PA_CEATA))
#define SZ_CEATA		0x1000

#define PA_FMI0			0x81200000
#define VA_FMI0			(VA_IO0(PA_FMI0))
#define SZ_FMI0			0x1000
#define PA_FMI1			0x81240000
#define VA_FMI1			(VA_IO0(PA_FMI1))
#define SZ_FMI1			0x1000
#define PA_FMI2			0x81280000
#define VA_FMI2			(VA_IO0(PA_FMI2))
#define SZ_FMI2			0x1000
#define PA_FMI3			0x81300000
#define VA_FMI3			(VA_IO0(PA_FMI3))
#define SZ_FMI3			0x1000
#define PA_FMI4			0x81340000
#define VA_FMI4			(VA_IO0(PA_FMI4))
#define SZ_FMI4			0x1000
#define PA_FMI5			0x81380000
#define VA_FMI5			(VA_IO0(PA_FMI5))
#define SZ_FMI5			0x1000

#define PA_SPI0			0x82000000
#define VA_SPI0			(VA_IO0(PA_SPI0))
#define SZ_SPI0			0x1000
#define PA_SPI1			0x82100000
#define VA_SPI1			(VA_IO0(PA_SPI1))
#define SZ_SPI1			0x1000

#define PA_UART(x)		(0x82500000 + 0x100000*(x))
#define VA_UART(x)		(VA_IO0(PA_UART(x)))
#define SZ_UART			0x1000
#define PA_UART0		PA_UART(0)
#define VA_UART0		0xFFE00000 //VA_UART(0)
#define PA_UART1		PA_UART(1)
#define VA_UART1		VA_UART(1)
#define PA_UART2		PA_UART(2)
#define VA_UART2		VA_UART(2)

#define PA_PKE			0x83100000
#define VA_PKE			(VA_IO0(PA_PKE))
#define SZ_PKE			0x1000

#define PA_I2C(x)		(0x83200000 + 0x100000*(x))
#define VA_I2C(x)		(VA_IO0(PA_I2C(x)))
#define SZ_I2C			0x1000
#define PA_I2C0			PA_I2C(0)
#define VA_I2C0			VA_I2C(0)
#define PA_I2C1			PA_I2C(1)
#define VA_I2C1			VA_I2C(1)

#define PA_PWM			0x83500000
#define VA_PWM			(VA_IO0(PA_PWM))
#define SZ_PWM			0x1000

#define PA_I2S0			0x84500400
#define VA_I2S0			(VA_IO0(PA_I2S0))
#define SZ_I2S0			0xC00

#define PA_USB_PHY		0x86000000
#define VA_USB_PHY		(VA_IO0(PA_USB_PHY))
#define SZ_USB_PHY		0x1000

#define PA_USB			0x86100000
#define VA_USB			(VA_IO0(PA_USB))
#define SZ_USB			0x10000

#define PA_USB_EHCI		0x86400000
#define VA_USB_EHCI		(VA_IO0(PA_USB_EHCI))
#define SZ_USB_EHCI		0x10000

#define PA_USB_OHCI0	0x86500000
#define VA_USB_OHCI0	(VA_IO0(PA_USB_OHCI0))
#define SZ_USB_OHCI0	0x10000
#define PA_USB_OHCI1	0x86600000
#define VA_USB_OHCI1	(VA_IO0(PA_USB_OHCI1))
#define SZ_USB_OHCI1	0x10000

#define PA_IOP0			0x86300000
#define VA_IOP0			(VA_IO0(PA_IOP0))
#define SZ_IOP0			0x1000
#define PA_IOP1			0xBF300000
#define VA_IOP1			(VA_IO1(PA_IOP1))
#define SZ_IOP1			0x1000

#define PA_VXD			0x85000000
#define VA_VXD			(VA_IO0(PA_VXD))
#define SZ_VXD			0x100000

#define PA_SGX			0x85100000
#define VA_SGX			(VA_IO0(PA_SGX))
#define SZ_SGX			0x1000

#define PA_VENC			0x88000000
#define VA_VENC			(VA_IO0(PA_VENC))
#define SZ_VENC			0x1000

#define PA_JPEG			0x88200000
#define VA_JPEG			(VA_IO0(PA_JPEG))
#define SZ_JPEG			0x1000

#define PA_ISP0			0x88300000
#define VA_ISP0			(VA_IO0(PA_ISP0))
#define SZ_ISP0			0xD6000
#define PA_ISP1			0x88100000
#define VA_ISP1			(VA_IO0(PA_ISP1))
#define SZ_ISP1			0x1000

#define PA_SCALER		0x89300000
#define VA_SCALER		(VA_IO0(PA_SCALER))
#define SZ_SCALER		0x1000

#define PA_CLCD0		0x89000000
#define VA_CLCD0		(VA_IO0(PA_CLCD0))
#define SZ_CLCD0		0x7000
#define PA_CLCD1		0x89200000
#define VA_CLCD1		(VA_IO0(PA_CLCD1))
#define SZ_CLCD1		0x2000

#define PA_MIPI_DSIM	0x89500000
#define VA_MIPI_DSIM	(VA_IO0(PA_MIPI_DSIM))
#define SZ_MIPI_DSIM	0x1000

#define PA_SWI			0xBF600000
#define VA_SWI			(VA_IO1(PA_SWI))
#define SZ_SWI			0x1000

#define PA_RGBOUT0		0x89100000
#define VA_RGBOUT0		(VA_IO0(PA_RGBOUT0))
#define SZ_RGBOUT0		0x7000
#define PA_RGBOUT1		0x89600000
#define VA_RGBOUT1		(VA_IO0(PA_RGBOUT1))
#define SZ_RGBOUT1		0x1000

#define PA_TVOUT		0x89400000
#define VA_TVOUT		(VA_IO0(PA_TVOUT))
#define SZ_TVOUT		0x1000

#define PA_AMC0			0x84100000
#define VA_AMC0			(VA_IO0(PA_AMC0))
#define SZ_AMC0			0x3000
#define PA_AMC1			0x84000000
#define VA_AMC1			(VA_IO0(PA_AMC1))
#define SZ_AMC1			0x40000
#define PA_AMC2			0x84300000
#define VA_AMC2			(VA_IO0(PA_AMC2))
#define SZ_AMC2			0x5000

#define PA_DISPLAYPORT	0x84900000
#define VA_DISPLAYPORT	(VA_IO0(PA_DISPLAYPORT))
#define SZ_DISPLAYPORT	0x2000

#endif //_S5L_8930_MAP_

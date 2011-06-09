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

#define SZ_VIC			0x10000
#define PA_VIC(x)		(0xBF200000 + (x*SZ_VIC))
#define VA_VIC(x)		(__va(PA_VIC(x)))
#define PA_VIC0			PA_VIC(0)
#define VA_VIC0			VA_VIC(0)
#define PA_VIC1			PA_VIC(1)
#define VA_VIC1			VA_VIC(1)
#define PA_VIC2			PA_VIC(2)
#define VA_VIC2			VA_VIC(2)
#define PA_VIC3			PA_VIC(3)
#define VA_VIC3			VA_VIC(3)

#define PA_GPIO			0xBFA00000
#define VA_GPIO			(__va(PA_GPIO))
#define SZ_GPIO			0x1000

#define PA_PMGR0		0xBF100000
#define VA_PMGR0		(__va(PA_PMGR0))
#define SZ_PMGR0		0x6000
#define PA_PMGR1		0x85E00000
#define VA_PMGR1		(__va(PA_PMGR1))
#define SZ_PMGR1		0x1000
#define PA_PMGR2		0x85F00000
#define VA_PMGR2		(__va(PA_PMGR2))
#define SZ_PMGR2		0x1000
#define PA_PMGR3		0x88E00000
#define VA_PMGR3		(__va(PA_PMGR3))
#define SZ_PMGR3		0x1000
#define PA_PMGR4		0x88F00000
#define VA_PMGR4		(__va(PA_PMGR4))
#define SZ_PMGR4		0x1000
#define PA_PMGR5		0x89E00000
#define VA_PMGR5		(__va(PA_PMGR5))
#define SZ_PMGR5		0x1000
#define PA_PMGR6		0x89F00000
#define VA_PMGR6		(__va(PA_PMGR6))
#define SZ_PMGR6		0x1000

#define PA_DEBUG		0xBF701000
#define VA_DEBUG		(__va(PA_DEBUG))
#define SZ_DEBUG		0x1000

#define PA_CDMA			0x87000000
#define VA_CDMA			(__va(PA_CDMA))
#define SZ_CDMA			0x26000
#define PA_CDMA_AES		0x87800000
#define VA_CDMA_AES		(__va(PA_CDMA_AES))
#define SZ_CDMA_AES		0x9000

#define PA_DART1		0x88D00000
#define VA_DART1		(__va(PA_DART1))
#define SZ_DART1		0x2000
#define PA_DART2		0x89D00000
#define VA_DART2		(__va(PA_DART2))
#define SZ_DART2		0x2000

#define PA_SDIO			0x80000000
#define VA_SDIO			(__va(PA_SDIO))
#define SZ_SDIO			0x1000

#define PA_SHA			0x80100000
#define VA_SHA			(__va(PA_SHA))
#define SZ_SHA			0x1000

#define PA_CEATA		0x81000000
#define VA_CEATA		(__va(PA_CEATA))
#define SZ_CEATA		0x1000

#define PA_FMI0			0x81200000
#define VA_FMI0			(__va(PA_FMI0))
#define SZ_FMI0			0x1000
#define PA_FMI1			0x81240000
#define VA_FMI1			(__va(PA_FMI1))
#define SZ_FMI1			0x1000
#define PA_FMI2			0x81280000
#define VA_FMI2			(__va(PA_FMI2))
#define SZ_FMI2			0x1000
#define PA_FMI3			0x81300000
#define VA_FMI3			(__va(PA_FMI3))
#define SZ_FMI3			0x1000
#define PA_FMI4			0x81340000
#define VA_FMI4			(__va(PA_FMI4))
#define SZ_FMI4			0x1000
#define PA_FMI5			0x81380000
#define VA_FMI5			(__va(PA_FMI5))
#define SZ_FMI5			0x1000

#define PA_SPI0			0x82000000
#define VA_SPI0			(__va(PA_SPI0))
#define SZ_SPI0			0x1000
#define PA_SPI1			0x82100000
#define VA_SPI1			(__va(PA_SPI1))
#define SZ_SPI1			0x1000

#define PA_UART(x)		(0x82500000 + 0x100000*(x))
#define VA_UART(x)		(__va(PA_UART(x)))
#define SZ_UART			0x1000
#define PA_UART0		PA_UART(0)
#define VA_UART0		VA_UART(0)
#define PA_UART1		PA_UART(1)
#define VA_UART1		VA_UART(1)
#define PA_UART2		PA_UART(2)
#define VA_UART2		VA_UART(2)

#define PA_PKE			0x83100000
#define VA_PKE			(__va(PA_PKE))
#define SZ_PKE			0x1000

#define PA_I2C(x)		(0x83200000 + 0x100000*(x))
#define VA_I2C(x)		(__va(PA_I2C(x)))
#define SZ_I2C			0x1000
#define PA_I2C0			PA_I2C(0)
#define VA_I2C0			VA_I2C(0)
#define PA_I2C1			PA_I2C(1)
#define VA_I2C1			VA_I2C(1)

#define PA_PWM			0x83500000
#define VA_PWM			(__va(PA_PWM))
#define SZ_PWM			0x1000

#define PA_I2S0			0x84500400
#define VA_I2S0			(__va(PA_I2S0))
#define SZ_I2S0			0xC00

#define PA_USB_PHY		0x86000000
#define VA_USB_PHY		(__va(PA_USB_PHY))
#define SZ_USB_PHY		0x1000

#define PA_USB			0x86100000
#define VA_USB			(__va(PA_USB))
#define SZ_USB			0x10000

#define PA_USB_EHCI		0x86400000
#define VA_USB_EHCI		(__va(PA_USB_EHCI))
#define SZ_USB_EHCI		0x10000

#define PA_USB_OHCI0	0x86500000
#define VA_USB_OHCI0	(__va(PA_USB_OHCI0))
#define SZ_USB_OHCI0	0x10000
#define PA_USB_OHCI1	0x86600000
#define VA_USB_OHCI1	(__va(PA_USB_OHCI1))
#define SZ_USB_OHCI1	0x10000

#define PA_IOP0			0x86300000
#define VA_IOP0			(__va(PA_IOP0))
#define SZ_IOP0			0x1000
#define PA_IOP1			0xBF300000
#define VA_IOP1			(__va(PA_IOP1))
#define SZ_IOP1			0x1000

#define PA_VXD			0x85000000
#define VA_VXD			(__va(PA_VXD))
#define SZ_VXD			0x100000

#define PA_SGX			0x85100000
#define VA_SGX			(__va(PA_SGX))
#define SZ_SGX			0x1000

#define PA_VENC			0x88000000
#define VA_VENC			(__va(PA_VENC))
#define SZ_VENC			0x1000

#define PA_JPEG			0x88200000
#define VA_JPEG			(__va(PA_JPEG))
#define SZ_JPEG			0x1000

#define PA_ISP0			0x88300000
#define VA_ISP0			(__va(PA_ISP0))
#define SZ_ISP0			0xD6000
#define PA_ISP1			0x88100000
#define VA_ISP1			(__va(PA_ISP1))
#define SZ_ISP1			0x1000

#define PA_SCALER		0x89300000
#define VA_SCALER		(__va(PA_SCALER))
#define SZ_SCALER		0x1000

#define PA_CLCD0		0x89000000
#define VA_CLCD0		(__va(PA_CLCD0))
#define SZ_CLCD0		0x7000
#define PA_CLCD1		0x89200000
#define VA_CLCD1		(__va(PA_CLCD1))
#define SZ_CLCD1		0x2000

#define PA_MIPI_DSIM	0x89500000
#define VA_MIPI_DSIM	(__va(PA_MIPI_DSIM))
#define SZ_MIPI_DSIM	0x1000

#define PA_SWI			0xBF600000
#define VA_SWI			(__va(PA_SWI))
#define SZ_SWI			0x1000

#define PA_RGBOUT0		0x89100000
#define VA_RGBOUT0		(__va(PA_RGBOUT0))
#define SZ_RGBOUT0		0x7000
#define PA_RGBOUT1		0x89600000
#define VA_RGBOUT1		(__va(PA_RGBOUT1))
#define SZ_RGBOUT1		0x1000

#define PA_TVOUT		0x89400000
#define VA_TVOUT		(__va(PA_TVOUT))
#define SZ_TVOUT		0x1000

#define PA_AMC0			0x84100000
#define VA_AMC0			(__va(PA_AMC0))
#define SZ_AMC0			0x3000
#define PA_AMC1			0x84000000
#define VA_AMC1			(__va(PA_AMC1))
#define SZ_AMC1			0x40000
#define PA_AMC2			0x84300000
#define VA_AMC2			(__va(PA_AMC2))
#define SZ_AMC2			0x5000

#define PA_DISPLAYPORT	0x84900000
#define VA_DISPLAYPORT	(__va(PA_DISPLAYPORT))
#define SZ_DISPLAYPORT	0x2000

#endif //_S5L_8930_MAP_

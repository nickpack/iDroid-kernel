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

#define SZ_VIC			0x10000
#define VA_VIC(x)		(0xBF200000 + (x*SZ_VIC))
#define VA_VIC0			VA_VIC(0)
#define VA_VIC1			VA_VIC(1)
#define VA_VIC2			VA_VIC(2)
#define VA_VIC3			VA_VIC(3)

#define VA_GPIO			0xBFA00000
#define SZ_GPIO			0x1000

#define VA_PMGR0		0xBF100000
#define SZ_PMGR0		0x6000
#define VA_PMGR1		0x85E00000
#define SZ_PMGR1		0x1000
#define VA_PMGR2		0x85F00000
#define SZ_PMGR2		0x1000
#define VA_PMGR3		0x88E00000
#define SZ_PMGR3		0x1000
#define VA_PMGR4		0x88F00000
#define SZ_PMGR4		0x1000
#define VA_PMGR5		0x89E00000
#define SZ_PMGR5		0x1000
#define VA_PMGR6		0x89F00000
#define SZ_PMGR6		0x1000

#define VA_DEBUG		0xBF701000
#define SZ_DEBUG		0x1000

#define VA_CDMA			0x87000000
#define SZ_CDMA			0x26000
#define VA_CDMA_AES		0x87800000
#define SZ_CDMA_AES		0x9000

#define VA_DART1		0x88D00000
#define SZ_DART1		0x2000
#define VA_DART2		0x89D00000
#define SZ_DART2		0x2000

#define VA_SDIO			0x80000000
#define SZ_SDIO			0x1000

#define VA_SHA			0x80100000
#define SZ_SHA			0x1000

#define VA_CEATA		0x81000000
#define SZ_CEATA		0x1000

#define VA_FMI0			0x81200000
#define SZ_FMI0			0x1000
#define VA_FMI1			0x81240000
#define SZ_FMI1			0x1000
#define VA_FMI2			0x81280000
#define SZ_FMI2			0x1000
#define VA_FMI3			0x81300000
#define SZ_FMI3			0x1000
#define VA_FMI4			0x81340000
#define SZ_FMI4			0x1000
#define VA_FMI5			0x81380000
#define SZ_FMI5			0x1000

#define VA_SPI0			0x82000000
#define SZ_SPI0			0x1000
#define VA_SPI1			0x82100000
#define SZ_SPI1			0x1000

#define VA_UART(x)		(0x82500000 + 0x100000*(x))
#define SZ_UART			0x1000
#define VA_UART0		VA_UART(0)
#define VA_UART1		VA_UART(1)
#define VA_UART2		VA_UART(2)

#define VA_PKE			0x83100000
#define SZ_PKE			0x1000

#define VA_I2C(x)		(0x83200000 + 0x100000*(x))
#define SZ_I2C			0x1000
#define VA_I2C0			VA_I2C(0)
#define VA_I2C1			VA_I2C(1)

#define VA_PWM			0x83500000
#define SZ_PWM			0x1000

#define VA_I2S0			0x84500400
#define SZ_I2S0			0xC00

#define VA_USB_PHY		0x86000000
#define SZ_USB_PHY		0x1000

#define VA_USB			0x86100000
#define SZ_USB			0x10000

#define VA_USB_EHCI		0x86400000
#define SZ_USB_EHCI		0x10000

#define VA_USB_OHCI0	0x86500000
#define SZ_USB_OHCI0	0x10000
#define VA_USB_OHCI1	0x86600000
#define SZ_USB_OHCI1	0x10000

#define VA_IOP0			0x86300000
#define SZ_IOP0			0x1000
#define VA_IOP1			0xBF300000
#define SZ_IOP1			0x1000

#define VA_VXD			0x85000000
#define SZ_VXD			0x100000

#define VA_SGX			0x85100000
#define SZ_SGX			0x1000

#define VA_VENC			0x88000000
#define SZ_VENC			0x1000

#define VA_JPEG			0x88200000
#define SZ_JPEG			0x1000

#define VA_ISP0			0x88300000
#define SZ_ISP0			0xD6000
#define VA_ISP1			0x88100000
#define SZ_ISP1			0x1000

#define VA_SCALER		0x89300000
#define SZ_SCALER		0x1000

#define VA_CLCD0		0x89000000
#define SZ_CLCD0		0x7000
#define VA_CLCD1		0x89200000
#define SZ_CLCD1		0x2000

#define VA_MIPI_DSIM	0x89500000
#define SZ_MIPI_DSIM	0x1000

#define VA_SWI			0xBF600000
#define SZ_SWI			0x1000

#define VA_RGBOUT0		0x89100000
#define SZ_RGBOUT0		0x7000
#define VA_RGBOUT1		0x89600000
#define SZ_RGBOUT1		0x1000

#define VA_TVOUT		0x89400000
#define SZ_TVOUT		0x1000

#define VA_AMC0			0x84100000
#define SZ_AMC0			0x3000
#define VA_AMC1			0x84000000
#define SZ_AMC1			0x40000
#define VA_AMC2			0x84300000
#define SZ_AMC2			0x5000

#define VA_DISPLAYPORT	0x84900000
#define SZ_DISPLAYPORT	0x2000

#endif //_S5L_8930_MAP_

#ifndef __ASM_ARCH_MAP_H
#define __ASM_ARCH_MAP_H __FILE__

#include <mach/hardware.h>

#define S3C_PA_USB_HSPHY    (0x3C400000)
#define S3C_VA_USB_HSPHY    IO_ADDRESS(0x3C400000)

#define S3C_PA_USB_HSOTG    (0x38400000)
#define S3C_VA_USB_HSOTG    IO_ADDRESS(0x38400000)

#define IPHONE_USB_POWER    (0x200)
#define IPHONE_USB_CLOCK    (0x2)
#define IPHONE_USBPHY_CLOCK (0x23)
#define IPHONE_EDRAM_CLOCK  (0x1B)

#endif

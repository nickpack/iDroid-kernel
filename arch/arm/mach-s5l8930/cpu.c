#include <linux/init.h>
#include <linux/kernel.h>
#include <mach/cpu.h>
#include <mach/map.h>
#include <asm/mach/map.h>
#include <plat/irq.h>
#include <asm/page.h>

static struct map_desc s5l8930_iodesc[] __initdata = {
	{
		.virtual	= (unsigned long)VA_VIC0,
		.pfn		= __phys_to_pfn(PA_VIC0),
		.length		= SZ_VIC,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_VIC1,
		.pfn		= __phys_to_pfn(PA_VIC1),
		.length		= SZ_VIC,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_VIC2,
		.pfn		= __phys_to_pfn(PA_VIC2),
		.length		= SZ_VIC,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_VIC3,
		.pfn		= __phys_to_pfn(PA_VIC3),
		.length		= SZ_VIC,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_GPIO,
		.pfn		= __phys_to_pfn(PA_GPIO),
		.length		= SZ_GPIO,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_PMGR0,
		.pfn		= __phys_to_pfn(PA_PMGR0),
		.length		= SZ_PMGR0,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_PMGR1,
		.pfn		= __phys_to_pfn(PA_PMGR1),
		.length		= SZ_PMGR1,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_PMGR2,
		.pfn		= __phys_to_pfn(PA_PMGR2),
		.length		= SZ_PMGR2,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_PMGR3,
		.pfn		= __phys_to_pfn(PA_PMGR3),
		.length		= SZ_PMGR3,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_PMGR4,
		.pfn		= __phys_to_pfn(PA_PMGR4),
		.length		= SZ_PMGR4,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_PMGR5,
		.pfn		= __phys_to_pfn(PA_PMGR5),
		.length		= SZ_PMGR5,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_PMGR6,
		.pfn		= __phys_to_pfn(PA_PMGR6),
		.length		= SZ_PMGR6,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_DEBUG,
		.pfn		= __phys_to_pfn(PA_DEBUG),
		.length		= SZ_DEBUG,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_CDMA,
		.pfn		= __phys_to_pfn(PA_CDMA),
		.length		= SZ_CDMA,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_CDMA_AES,
		.pfn		= __phys_to_pfn(PA_CDMA_AES),
		.length		= SZ_CDMA_AES,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_DART1,
		.pfn		= __phys_to_pfn(PA_DART1),
		.length		= SZ_DART1,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_DART2,
		.pfn		= __phys_to_pfn(PA_DART2),
		.length		= SZ_DART2,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_SDIO,
		.pfn		= __phys_to_pfn(PA_SDIO),
		.length		= SZ_SDIO,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_SHA,
		.pfn		= __phys_to_pfn(PA_SHA),
		.length		= SZ_SHA,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_CEATA,
		.pfn		= __phys_to_pfn(PA_CEATA),
		.length		= SZ_CEATA,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_FMI0,
		.pfn		= __phys_to_pfn(PA_FMI0),
		.length		= SZ_FMI0,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_FMI1,
		.pfn		= __phys_to_pfn(PA_FMI1),
		.length		= SZ_FMI1,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_FMI2,
		.pfn		= __phys_to_pfn(PA_FMI2),
		.length		= SZ_FMI2,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_FMI3,
		.pfn		= __phys_to_pfn(PA_FMI3),
		.length		= SZ_FMI3,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_FMI4,
		.pfn		= __phys_to_pfn(PA_FMI4),
		.length		= SZ_FMI4,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_FMI5,
		.pfn		= __phys_to_pfn(PA_FMI5),
		.length		= SZ_FMI5,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_SPI0,
		.pfn		= __phys_to_pfn(PA_SPI0),
		.length		= SZ_SPI0,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_SPI1,
		.pfn		= __phys_to_pfn(PA_SPI1),
		.length		= SZ_SPI1,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_UART0,
		.pfn		= __phys_to_pfn(PA_UART0),
		.length		= SZ_UART,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_UART1,
		.pfn		= __phys_to_pfn(PA_UART1),
		.length		= SZ_UART,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_UART2,
		.pfn		= __phys_to_pfn(PA_UART2),
		.length		= SZ_UART,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_PKE,
		.pfn		= __phys_to_pfn(PA_PKE),
		.length		= SZ_PKE,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_I2C0,
		.pfn		= __phys_to_pfn(PA_I2C0),
		.length		= SZ_I2C,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_I2C1,
		.pfn		= __phys_to_pfn(PA_I2C1),
		.length		= SZ_I2C,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_PWM,
		.pfn		= __phys_to_pfn(PA_PWM),
		.length		= SZ_PWM,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_I2S0,
		.pfn		= __phys_to_pfn(PA_I2S0),
		.length		= SZ_I2S0,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_USB_PHY,
		.pfn		= __phys_to_pfn(PA_USB_PHY),
		.length		= SZ_USB_PHY,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_USB,
		.pfn		= __phys_to_pfn(PA_USB),
		.length		= SZ_USB,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_USB_EHCI,
		.pfn		= __phys_to_pfn(PA_USB_EHCI),
		.length		= SZ_USB_EHCI,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_USB_OHCI0,
		.pfn		= __phys_to_pfn(PA_USB_OHCI0),
		.length		= SZ_USB_OHCI0,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_USB_OHCI1,
		.pfn		= __phys_to_pfn(PA_USB_OHCI1),
		.length		= SZ_USB_OHCI1,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_IOP0,
		.pfn		= __phys_to_pfn(PA_IOP0),
		.length		= SZ_IOP0,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_IOP1,
		.pfn		= __phys_to_pfn(PA_IOP1),
		.length		= SZ_IOP1,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_VXD,
		.pfn		= __phys_to_pfn(PA_VXD),
		.length		= SZ_VXD,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_SGX,
		.pfn		= __phys_to_pfn(PA_SGX),
		.length		= SZ_SGX,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_VENC,
		.pfn		= __phys_to_pfn(PA_VENC),
		.length		= SZ_VENC,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_JPEG,
		.pfn		= __phys_to_pfn(PA_JPEG),
		.length		= SZ_JPEG,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_ISP0,
		.pfn		= __phys_to_pfn(PA_ISP0),
		.length		= SZ_ISP0,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_ISP1,
		.pfn		= __phys_to_pfn(PA_ISP1),
		.length		= SZ_ISP1,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_SCALER,
		.pfn		= __phys_to_pfn(PA_SCALER),
		.length		= SZ_SCALER,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_CLCD0,
		.pfn		= __phys_to_pfn(PA_CLCD0),
		.length		= SZ_CLCD0,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_CLCD1,
		.pfn		= __phys_to_pfn(PA_CLCD1),
		.length		= SZ_CLCD1,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_MIPI_DSIM,
		.pfn		= __phys_to_pfn(PA_MIPI_DSIM),
		.length		= SZ_MIPI_DSIM,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_SWI,
		.pfn		= __phys_to_pfn(PA_SWI),
		.length		= SZ_SWI,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_RGBOUT0,
		.pfn		= __phys_to_pfn(PA_RGBOUT0),
		.length		= SZ_RGBOUT0,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_RGBOUT1,
		.pfn		= __phys_to_pfn(PA_RGBOUT1),
		.length		= SZ_RGBOUT1,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_TVOUT,
		.pfn		= __phys_to_pfn(PA_TVOUT),
		.length		= SZ_TVOUT,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_AMC0,
		.pfn		= __phys_to_pfn(PA_AMC0),
		.length		= SZ_AMC0,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_AMC1,
		.pfn		= __phys_to_pfn(PA_AMC1),
		.length		= SZ_AMC1,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_AMC2,
		.pfn		= __phys_to_pfn(PA_AMC2),
		.length		= SZ_AMC2,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= (unsigned long)VA_DISPLAYPORT,
		.pfn		= __phys_to_pfn(PA_DISPLAYPORT),
		.length		= SZ_DISPLAYPORT,
		.type		= MT_DEVICE,
	},
};

void __init s5l8930_map_io(void)
{
	iotable_init(s5l8930_iodesc, ARRAY_SIZE(s5l8930_iodesc));
}

static void __iomem *s5l8930_vics[] = {
	VA_VIC0,
	VA_VIC1,
	VA_VIC2,
	VA_VIC3,
};

void __init s5l8930_init_irq(void)
{
	s5l_init_vics(s5l8930_vics, ARRAY_SIZE(s5l8930_vics));
}

void __init s5l8930_init(void)
{
}

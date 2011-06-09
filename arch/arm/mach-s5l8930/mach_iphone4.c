#include <asm/mach/arch.h>
#include <mach/cpu.h>
#include <mach/time.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>

// tmp
#include <asm/io.h>

static struct platform_device fbdev = {
	.name = "s5l8920_fb",

	.dev = {
		.platform_data = (void*)0x5F700000,
	},
};

static void __init ip4_init(void)
{
	s5l8930_init();

	{
		int i;
		for(i = 0; i < 400; i++)
			writel(0xffffffff, 0x5f700000 + i*4);
	}

	platform_device_register(&fbdev);

	// TODO: Add ipt4g devices here!
}

MACHINE_START(IPHONE_4, "Apple iPhone 4")
	/* Maintainer: iDroid Project */
	.boot_params	= 0x46000000,
	.map_io		= s5l8930_map_io,
	.init_irq	= s5l8930_init_irq,
	.timer		= &s5l8930_timer,
	.init_machine	= ip4_init,
MACHINE_END

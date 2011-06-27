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
		.platform_data = __va(0x5F700000),
	},
};

static void __init ip4_init(void)
{
	printk("%s\n", __func__);

	s5l8930_init();

	//platform_device_register(&fbdev);

	// TODO: Add ipt4g devices here!
}

static void __init ip4_init_early(void)
{
	printk("%s\n", __func__);
}

MACHINE_START(IPHONE_4, "Apple iPhone 4")
	/* Maintainer: iDroid Project */
	.boot_params	= 0x46000000,
	.map_io		= s5l8930_map_io,
	.init_irq	= s5l8930_init_irq,
	.timer		= &s5l8930_timer,
	.init_machine	= ip4_init,
	.init_early		= ip4_init_early,
MACHINE_END

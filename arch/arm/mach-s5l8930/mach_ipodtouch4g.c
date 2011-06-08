#include <asm/mach/arch.h>
#include <mach/cpu.h>
#include <mach/time.h>
#include <asm/mach-types.h>

static void __init ipt4g_init(void)
{
	s5l8930_init();

	// TODO: Add ipt4g devices here!
}

MACHINE_START(IPOD_TOUCH_4G, "Apple iPod Touch 4G")
	/* Maintainer: iDroid Project */
	.boot_params	= 0x46000000,
	.map_io		= s5l8930_map_io,
	.init_irq	= s5l8930_init_irq,
	.timer		= &s5l8930_timer,
	.init_machine	= ipt4g_init,
MACHINE_END

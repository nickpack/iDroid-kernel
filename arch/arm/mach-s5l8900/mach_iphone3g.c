/*
 * arch/arm/mach-s5l8900/mach_iphone3g.c - The base machine layout and functions.
 *
 * Copyright (C) 2008 Yiduo Wang
 *
 * Portions Copyright (C) 2010 Ricky Taylor
 *
 * This file is part of iDroid. An android distribution for Apple products.
 * For more information, please visit http://www.idroidproject.org/.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/power_supply.h>
#include <linux/pm.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/mach/irq.h>

#include "lcd.h"
#include <mach/iphone-dma.h>
#include <mach/iphone-i2c.h>
#include <mach/usb.h>
#include <mach/pmu.h>

#include <linux/platform_device.h>

#include <ftl/nand.h>

#include <asm/system.h>
#include <mach/system.h>

extern void __init iphone_init_irq(void);
extern struct sys_timer iphone_timer;

static struct map_desc iphone_io_desc[] __initdata = {
	{
		.virtual	=  IO_ADDRESS(0x3CC00000),
		.pfn		= __phys_to_pfn(0x3CC00000),
		.length		= SZ_2M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3C500000),
		.pfn		= __phys_to_pfn(0x3C500000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x38E00000),
		.pfn		= __phys_to_pfn(0x38E00000),
		.length		= SZ_8K,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x38E02000),
		.pfn		= __phys_to_pfn(0x38E02000),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3E300000),
		.pfn		= __phys_to_pfn(0x3E300000),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3E200000),
		.pfn		= __phys_to_pfn(0x3E200000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x38400000),
		.pfn		= __phys_to_pfn(0x38400000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x38D00000),
		.pfn		= __phys_to_pfn(0x38D00000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3C400000),
		.pfn		= __phys_to_pfn(0x3C400000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x39A00000),
		.pfn		= __phys_to_pfn(0x39A00000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x38900000),
		.pfn		= __phys_to_pfn(0x38900000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x38A00000),
		.pfn		= __phys_to_pfn(0x38A00000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x38F00000),
		.pfn		= __phys_to_pfn(0x38F00000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x38200000),
		.pfn		= __phys_to_pfn(0x38200000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x39900000),
		.pfn		= __phys_to_pfn(0x39900000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3C600000),
		.pfn		= __phys_to_pfn(0x3C600000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3C900000),
		.pfn		= __phys_to_pfn(0x3C900000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x39A00000),
		.pfn		= __phys_to_pfn(0x39A00000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3E400000),
		.pfn		= __phys_to_pfn(0x3E400000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3CA00000),
		.pfn		= __phys_to_pfn(0x3CA00000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3CD00000),
		.pfn		= __phys_to_pfn(0x3CD00000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3E500000),
		.pfn		= __phys_to_pfn(0x3E500000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3C300000),
		.pfn		= __phys_to_pfn(0x3C300000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3CE00000),
		.pfn		= __phys_to_pfn(0x3CE00000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
	{
		.virtual	=  IO_ADDRESS(0x3D200000),
		.pfn		= __phys_to_pfn(0x3D200000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
};

static void __init iphone_map_io(void)
{
	printk("iphone: initializing io map\n");
	iotable_init(iphone_io_desc, ARRAY_SIZE(iphone_io_desc));
}

static struct i2c_board_info __initdata iphone_i2c0[] = {
	{
		I2C_BOARD_INFO("iphone-accel", 0x3a),
	},
	{
		I2C_BOARD_INFO("pcf50633", 0xe6),
		.irq = IPHONE_GPIO_IRQS + 0x55,
		.platform_data = &pcf50633_pdata,
	},
	{
		I2C_BOARD_INFO("wm8991", 0x36),
	},
	{
		I2C_BOARD_INFO("isl29003", 0x88),
	},
};

static struct i2c_board_info __initdata iphone_i2c1[] = {
};

static struct spi_board_info __initdata iphone_spi[] = {
	{
		.bus_num = 0,
		.controller_data = (void*)0x400,
		.max_speed_hz = 12000000,
		.mode = SPI_MODE_0,
		.modalias = "iphone-nor",
	},
	// TODO: LCD SPI devices
	{
		.bus_num = 1,
		.controller_data = (void*)0x1800,
		.max_speed_hz = 4500000,
		.mode = SPI_MODE_3,
		.modalias = "zephyr2",
	},
	{
		.bus_num = 2,
		.controller_data = (void*)0x17021804, // MRDY/SRDY
		.max_speed_hz = 12000000,
		.mode = SPI_MODE_0,
		.modalias = "iphone-baseband",
	},
};

static void iphone_power_off(void)
{
	pcf50633_power_off();
	arch_reset('h', NULL);
}

static void __init iphone_init(void)
{
	printk("iphone: platform init\r\n");

	pm_power_off = &iphone_power_off;

	iphone_dma_setup();

	iphone_init_suspend();

	i2c_register_board_info(0, iphone_i2c0, ARRAY_SIZE(iphone_i2c0));
	i2c_register_board_info(1, iphone_i2c1, ARRAY_SIZE(iphone_i2c1));

	spi_register_board_info(iphone_spi, ARRAY_SIZE(iphone_spi));

	platform_device_register(&iphone_dma);
	platform_device_register(&iphone_nand);
	platform_device_register(&iphone_i2c);
}

static void __exit iphone_exit(void)
{
	platform_device_unregister(&iphone_i2c);
	platform_device_unregister(&iphone_nand);
	platform_device_unregister(&iphone_dma);
}
module_exit(iphone_exit);

MACHINE_START(IPHONE_3G, "Apple iPhone 3G")
	/* Maintainer: iDroid Project */
	.boot_params	= 0x09000000,
	.map_io		= iphone_map_io,
	.init_irq	= iphone_init_irq,
	.timer		= &iphone_timer,
	.init_machine	= iphone_init,
MACHINE_END

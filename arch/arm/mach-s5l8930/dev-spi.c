/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <plat/spi.h>
#include <mach/gpio.h>
#include <mach/irqs.h>
#include <mach/map.h>

static struct resource s5l8930_spi0_resources[] = {
	[0] = {
		.start = PA_SPI0,
		.end   = PA_SPI0 + SZ_SPI0 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SPI0,
		.end   = IRQ_SPI0,
		.flags = IORESOURCE_IRQ,
	},
};

static struct s5l_spi_info s5l8930_spi0_pdata = {
	.pin_cs = S5L8930_GPIO(0x505),
	.num_cs = 1,
	.bus_num = 0,
};

static u64 spi_dmamask = DMA_BIT_MASK(32);

struct platform_device s5l8930_spi0 = {
	.name					= "s5l89xx-spi",
	.id						= 0,
	.num_resources			= ARRAY_SIZE(s5l8930_spi0_resources),
	.resource				= s5l8930_spi0_resources,
	.dev = {
		.dma_mask			= &spi_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &s5l8930_spi0_pdata,
	},
};

static struct resource s5l8930_spi1_resources[] = {
	[0] = {
		.start = PA_SPI1,
		.end   = PA_SPI1 + SZ_SPI1 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SPI1,
		.end   = IRQ_SPI1,
		.flags = IORESOURCE_IRQ,
	},
};

static struct s5l_spi_info s5l8930_spi1_pdata = {
	.pin_cs = S5L8930_GPIO(0x601),
	.num_cs = 1,
	.bus_num = 1,
};

struct platform_device s5l8930_spi1 = {
	.name					= "s5l89xx-spi",
	.id						= 1,
	.num_resources			= ARRAY_SIZE(s5l8930_spi1_resources),
	.resource				= s5l8930_spi1_resources,
	.dev = {
		.dma_mask			= &spi_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &s5l8930_spi1_pdata,
	},
};

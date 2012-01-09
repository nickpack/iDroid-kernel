/*
 * dwc_otg_driver.c - platform-device specific functions
 *
 * Author: Ricky Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "dwc_otg_driver.h"

#include <linux/irq.h>
#include <linux/slab.h>
#include <asm/io.h>

#include "dwc_otg_regs.h"

int dwc_otg_ioremap(dwc_otg_iomapping_t *_map, struct resource *_rsrc)
{
	struct resource *mapping = NULL;

	if(!_map)
		return -EINVAL;

	memset(_map, sizeof(dwc_otg_iomapping_t), 0);

	if(!_rsrc)
		return -EINVAL;

	mapping = request_mem_region(_rsrc->start, resource_size(_rsrc), DWC_OTG_DRIVER_NAME);
	if(!mapping)
	{
		DWC_ERROR("Failed to request memory region (address=0x%08x, length=0x%08x).\n",
				_rsrc->start,
				resource_size(_rsrc));
		return -EIO;
	}

	_map->physical_address = _rsrc->start;
	_map->length = resource_size(_rsrc);
	_map->address = ioremap(_map->physical_address, _map->length);
	if(!_map->address)
	{
		DWC_ERROR("ioremap failed for memory region (address=0x%08x, length=0x%08x).\n",
			_rsrc->start,
			resource_size(_rsrc));
		return -EIO;
	}

	return 0;
}

void dwc_otg_iounmap(dwc_otg_iomapping_t *_map)
{
	if(!_map)
		return;

	if(_map->address)
	{
		iounmap(_map->address);
		_map->address = NULL;
	}

	if(_map->physical_address)
	{
		release_mem_region(_map->physical_address, _map->length);
		_map->physical_address = 0;
		_map->length = 0;
	}
}

static int dwc_otg_driver_probe(struct platform_device *_dev)
{
	dwc_otg_driver_t *driver = NULL;
	struct resource *resource = NULL;
	int ret = 0;

	DWC_PRINT("DWC OTG Hardware Detected.\n");

	// Allocate the driver structure.
	driver = kmalloc(sizeof(dwc_otg_driver_t), GFP_KERNEL);
	if(!driver)
	{
		DWC_ERROR("Failed to allocate driver structure.\n");
		return -ENOMEM;
	}

	// Clear the driver structure!
	memset(driver, sizeof(dwc_otg_driver_t), 0);

	// Store the driver structure!
	platform_set_drvdata(_dev, driver);

	// Request IRQ.
	driver->irq = platform_get_irq(_dev, 0);
	if(!driver->irq)
	{
		DWC_ERROR("Failed to retrieve device IRQ.\n");
		// TODO: Does _remove still get called if we return an error? -- Ricky26
		return -ENXIO;
	}

	// Request register memory.
	resource = platform_get_resource(_dev, IORESOURCE_MEM, 0);
	if(!resource)
	{
		DWC_ERROR("Failed to get register memory resource.\n");
		return -ENXIO;
	}

	// Claim register memory.
	if(dwc_otg_ioremap(&driver->registers, resource))
	{
		DWC_ERROR("Failed to claim register memory.\n");
		return -EIO;
	}

	// Request PHY memory.
	resource = platform_get_resource(_dev, IORESOURCE_MEM, 1);
	if(resource)
	{
		if(dwc_otg_ioremap(&driver->phy_registers, resource))
		{
			DWC_ERROR("Failed to claim PHY register memory.\n");
			return -EIO;
		}
	}

	// Initialise the core.
	ret = dwc_otg_core_init(&driver->core, driver->irq, driver->registers.address, driver->phy_registers.address);
	if(ret)
	{
		DWC_ERROR("Failed to initialise the core!\n");
		return ret;
	}

	// Initialise device mode.
	ret = dwc_otg_device_init(&driver->device, &driver->core);
	if(ret)
	{
		DWC_ERROR("Failed to initialise device mode!\n");
		return ret;
	}

	// Start core operation.
	ret = dwc_otg_core_start(&driver->core);
	if(ret)
	{
		DWC_ERROR("Failed to start core.\n");
		return ret;
	}

	// Start device mode.
	ret = dwc_otg_device_start(&driver->device);
	if(ret)
	{
		DWC_ERROR("Failed to start device mode.\n");
		return ret;
	}

	DWC_PRINT("DWC OTG Driver Installed.\n");

	return 0;
}

static int dwc_otg_driver_remove(struct platform_device *_dev)
{
	dwc_otg_driver_t *driver = dwc_otg_driver_get(_dev);
	if(driver)
	{
		dwc_otg_device_stop(&driver->device);
		dwc_otg_core_stop(&driver->core);

		dwc_otg_device_destroy(&driver->device);
		dwc_otg_core_destroy(&driver->core);

		dwc_otg_iounmap(&driver->registers);
		dwc_otg_iounmap(&driver->phy_registers);

		kfree(driver);
		platform_set_drvdata(_dev, NULL);
	}

	DWC_PRINT("DWC OTG Driver Removed.\n");

	return 0;
}

struct platform_driver dwc_otg_driver = {
	.probe = dwc_otg_driver_probe,
	.remove = dwc_otg_driver_remove,
	.suspend = NULL, /* optional but recommended */
	.resume = NULL,   /* optional but recommended */
	.driver = {
		.owner = THIS_MODULE,
		.name = DWC_OTG_DRIVER_NAME,
	},
};

static int dwc_otg_driver_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&dwc_otg_driver);
	if(ret)
	{
		DWC_ERROR("Failed to register driver (0x%08x).\n", ret);
		return ret;
	}

	DWC_PRINT("DWC OTG Driver Loaded.\n");

	return ret;
}
module_init(dwc_otg_driver_init);

static void dwc_otg_driver_exit(void)
{
	platform_driver_unregister(&dwc_otg_driver);

	DWC_PRINT("DWC OTG Driver Unloaded.\n");
}
module_exit(dwc_otg_driver_exit);

MODULE_DESCRIPTION("Synopsys On-the-Go USB DesignWare Core");
MODULE_AUTHOR("Ricky Taylor <rickytaylor26@gmail.com>");
MODULE_LICENSE("GPL");

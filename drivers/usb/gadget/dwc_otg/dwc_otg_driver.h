/*
 * dwc_otg_driver.h - header for platform-device specific functions
 *
 * Author: Ricky Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  __DWC_OTG_DRIVER_H__
#define  __DWC_OTG_DRIVER_H__

#include <linux/platform_device.h>

#include "dwc_otg.h"
#include "dwc_otg_core.h"
#include "dwc_otg_device.h"

/**
 * @file dwc_otg_driver.h
 *
 * This file contains the driver-framework specific parts of the driver.
 *
 * For instance, only this file and its sister code file need be altered
 * to use this driver with standard device structures (instead of the 
 * platform_device structures used here).
 */

/**
 * DWC OTG IO Mapping Structure
 *
 * This structure is used to store current IO mappings,
 * so that they can be released once they are finished with.
 */
typedef struct dwc_otg_iomapping
{
	/** The remapped address. Stored so it can be unmapped. */
	void *address;

	/** Physical memory address. Stored to allow memory to be released. */
	resource_size_t physical_address; 

	/** Length of memory mapping. Stored to allow memory to be released. */
	resource_size_t length;

} dwc_otg_iomapping_t;

/**
 * dwc_otg_ioremap
 *
 * Remaps a IORESOURCE_MEM resource into a dwc_otg_iomapping.
 * Returns 0 on success.
 */
int dwc_otg_ioremap(dwc_otg_iomapping_t *_map, struct resource *_rsrc);

/**
 * dwc_otg_iounmap
 *
 * Releases a previously mapped dwc_otg_iomapping.
 */
void dwc_otg_iounmap(dwc_otg_iomapping_t *_map);

/**
 * DWC OTG Driver Structure
 *
 * This structure stores any information required for the
 * platform driver.
 */
typedef struct dwc_otg_driver_struct
{
	/** The allocated IRQ. Stored here so that it can be deallocated. */
	int irq;

	/** The IO mapping for the main registers. */
	dwc_otg_iomapping_t registers;

	/** The IO mapping for the on-chip PHY's registers. */
	dwc_otg_iomapping_t phy_registers;

	/** The core state and parameters. */
	dwc_otg_core_t core;

	/** Device mode state and parameters. */
	dwc_otg_device_t device;

} dwc_otg_driver_t;

/**
 * dwc_otg_driver_get
 *
 * Returns the dwc_otg_driver_t for the specified platform_device.
 */
static inline dwc_otg_driver_t* dwc_otg_driver_get(struct platform_device *_dev)
{
	return (dwc_otg_driver_t*)platform_get_drvdata(_dev);
}

#endif //__DWC_OTG_DRIVER_H__

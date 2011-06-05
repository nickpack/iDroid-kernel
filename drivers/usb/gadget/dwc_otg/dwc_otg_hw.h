/*
 * dwc_otg_hw.h - header for hardware-specific code
 *
 * Author: Ricky Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  __DWC_OTG_HW_H__
#define  __DWC_OTG_HW_H__

#include <asm/io.h>

#include "dwc_otg_core.h"

/**
 * This powers up/down the hardware.
 */
int dwc_otg_hw_power(dwc_otg_core_t *core, int _pwr);

/*
 *
 * Helper functions for IO.
 *
 */

/**
 * Write a 32-bit register.
 */
static inline void dwc_otg_write_reg32(volatile uint32_t* _address, uint32_t _value)
{
	writel(_value, _address);
}

/**
 * Read a 32-bit register.
 */
static inline uint32_t dwc_otg_read_reg32(volatile uint32_t* _address)
{
	return readl(_address);
}

/**
 * Modify a 32-bit register.
 */
static inline void dwc_otg_modify_reg32(volatile uint32_t* _address, uint32_t _clear, uint32_t _set)
{
	writel((readl(_address) &~ _clear) | _set, _address);
}

#endif //__DWC_OTG_HW_H__

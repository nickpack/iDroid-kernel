/*
 * dwc_otg_device.h - header for device-mode specific functions
 *
 * Author: Ricky Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  __DWC_OTG_DEVICE_H__
#define  __DWC_OTG_DEVICE_H__

#include <linux/workqueue.h>
#include "dwc_otg_core.h"

/**
 * @file dwc_otg_device.h
 *
 * This file pair contains all of the device-mode specific code.
 */

/**
 * This structure defines the state and parameters
 * of the device mode of the OTG chip.
 */
typedef struct dwc_otg_device_struct
{
	/** The representation of the core. */
	dwc_otg_core_t *core;

	/** The IRQ of the device. */
	int irq;

	/** Whether remote wakeup is enabled. */
	unsigned remote_wakeup : 1;

	/** This work is used to deal with a usb reset. */
	struct work_struct reset_work;

	/** This work is used to deal with a usb reset. */
	struct work_struct suspend_work;

	/** This work is used to deal with being disconnected. */
	struct work_struct disconnect_work;

} dwc_otg_device_t;

/**
 * dwc_otg_device_init
 *
 * Initialise the dwc_otg_device structure, and the hardware
 * so that it can run in device mode.
 */
int dwc_otg_device_init(dwc_otg_device_t *_dev, dwc_otg_core_t *_core);

/**
 * dwc_otg_device_destroy
 *
 * Release all resources used by device mode, and return
 * the hardware to its original state.
 */
void dwc_otg_device_destroy(dwc_otg_device_t *_dev);

/**
 * dwc_otg_device_start
 *
 * Start device mode on the chip.
 */
int dwc_otg_device_start(dwc_otg_device_t *_dev);

/**
 * dwc_otg_device_stop
 *
 * Stop device mode on the chip.
 */
void dwc_otg_device_stop(dwc_otg_device_t *_dev);

/**
 * dwc_otg_device_enable_interrupts
 *
 * Enables interrupts.
 */
int dwc_otg_device_enable_interrupts(dwc_otg_device_t *_dev);

/**
 * dwc_otg_device_disable_interrupts
 *
 * Disables interrupts.
 */
int dwc_otg_device_disable_interrupts(dwc_otg_device_t *_dev);

/**
 * dwc_otg_device_reset_usb
 *
 * Resets the USB connection and re-enables EP0.
 */
int dwc_otg_device_usb_reset(dwc_otg_device_t *_dev);

/**
 * The device USB IRQ handler.
 */
irqreturn_t dwc_otg_device_irq(int _irq, void *_dev);

/**
 * The in endpoint interrupt handler.
 */
irqreturn_t dwc_otg_device_handle_in_interrupt(dwc_otg_device_t *_dev, int _ep);

/**
 * The out endpoint interrupt handler.
 */
irqreturn_t dwc_otg_device_handle_out_interrupt(dwc_otg_device_t *_dev, int _ep);

/**
 * dwc_otg_device_receive_ep0
 */
void dwc_otg_device_receive_ep0(dwc_otg_device_t *_dev);

/**
 * dwc_otg_device_send_ep0
 */
void dwc_otg_device_send_ep0(dwc_otg_device_t *_dev);

/**
 * dwc_otg_device_complete_ep0
 *
 * This is called to complete an interrupt on EP0.
 */
void dwc_otg_device_complete_ep0(dwc_otg_core_request_t *_req);

#endif //__DWC_OTG_DEVICE_H__

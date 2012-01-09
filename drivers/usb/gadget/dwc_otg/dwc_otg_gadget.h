/*
 * dwc_otg_gadget.h - header for gadget-specific API
 *
 * Author: Ricky Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  __DWC_OTG_GADGET_H__
#define  __DWC_OTG_GADGET_H__

#include <linux/usb.h>
#include <linux/usb/gadget.h>

#include "dwc_otg_device.h"

/**
 * @file dwc_otg_gadget.h
 *
 * This file pair implements the usb_gadget famework
 * for the driver.
 */

/**
 * This structure represents an EP of our device.
 *
 * usb_ep is used by the usb_gadget framework, so this
 * structure was created to allow us to store additonal
 * information. (You can retrieve a handle to the
 * @ref dwc_otg_gadget_ep_t using container_of.)
 */
typedef struct dwc_otg_gadget_ep_struct
{
	/** The USB EP information used by the gadget driver. */
	struct usb_ep usb_ep;

	/** Our EP information for translating the gadget requests. */
	dwc_otg_core_ep_t *ep;

} dwc_otg_gadget_ep_t;

#define dwc_otg_gadget_get_ep(a) (dwc_otg_gadget_ep_t*)container_of((a), dwc_otg_gadget_ep_t, usb_ep)

/**
 * This structure represents a request.
 *
 * 
 */
typedef struct dwc_otg_gadget_request_struct
{
	/** The request used by the usb_gadget framework. */
	struct usb_request usb_request;

	/** The endpoint used by the usb_gadget framework. */
	struct usb_ep *usb_ep;

	/** Our request. Used to actually queue the request. */
	dwc_otg_core_request_t dwc_request;

	/**
	 * We have been told to free the request, but it's in use.
	 * Instead of passing control to the usb gadget driver,
	 * free the request when it completes.
	 */
	unsigned free : 1;

	/**
	 * Whether to free the DMA memory, this will be false if
	 * they supplied it.
	 */
	unsigned free_dma : 1;
} dwc_otg_gadget_request_t;

#define dwc_otg_gadget_get_request(a) (dwc_otg_gadget_request_t*)container_of((a), dwc_otg_gadget_request_t, usb_request)

/**
 * We use this variable to store the current gadget driver,
 * we can't really get around the global variable business
 * unfortunately.
 */
extern struct usb_gadget_driver *dwc_otg_gadget_driver;

/**
 * Because the gadget stuff is hooked into 'the code', we must
 * have this available. ;_;
 */
extern struct usb_gadget dwc_otg_gadget;

/**
 * usb_gadget_register_driver
 *
 * This function is called when the gadget driver is loaded
 * to bind it to the controller.
 */
int usb_gadget_register_driver(struct usb_gadget_driver* _dri);

/**
 * usb_gadget_unregister_driver
 *
 * This function is called when the gadget driver is being released.
 */
int usb_gadget_unregister_driver(struct usb_gadget_driver* _dri);

/**
 * usb_gadget_register_controller
 *
 * This is called to store the current controller for use in the
 * other usb_gadget* functions.
 */
int usb_gadget_register_controller(dwc_otg_device_t *_dev);

/**
 * usb_gadget_unregister_controller
 *
 * This is called to release the controller.
 */
int usb_gadget_unregister_controller(dwc_otg_device_t *_dev);

/**
 * This function enables an EP with the parameters required
 * for the supplied usb_endpoint_descriptor.
 */
int dwc_otg_gadget_enable_ep(struct usb_ep *_ep, const struct usb_endpoint_descriptor *_desc);

/**
 * This function disables an EP.
 */
int dwc_otg_gadget_disable_ep(struct usb_ep *_ep);

/**
 * This function allocates a request, to be filled in by the caller
 * which can then be queued to be sent to the host.
 */
struct usb_request *dwc_otg_gadget_alloc_request(struct usb_ep *_ep, gfp_t _gfp);

/**
 * This function frees a request previously allocated by
 * dwc_otg_gadget_alloc_request.
 */
void dwc_otg_gadget_free_request(struct usb_ep *_ep, struct usb_request *_req);

/**
 * This queues a request to be sent to the host.
 */
int dwc_otg_gadget_queue_request(struct usb_ep *_ep, struct usb_request *_req, gfp_t _gfp);

/**
 * This function removes a request from the queue, providing it has not
 * already been sent.
 */
int dwc_otg_gadget_dequeue_request(struct usb_ep *_ep, struct usb_request *_req);

/**
 * This sends a stall to the host if the value is 1,
 * or clears a previously set halt if 0.
 */
int dwc_otg_gadget_set_halt(struct usb_ep *_ep, int _val);

/**
 * This stalls the endpoint and prevents it from being
 * restarted by the host.
 */
int dwc_otg_gadget_set_wedge(struct usb_ep *_ep);

/**
 * This retrieves the fifo status of the EP.
 */
int dwc_otg_gadget_fifo_status(struct usb_ep *_ep);

/**
 * This function flushes an EP's FIFO.
 */
void dwc_otg_gadget_fifo_flush(struct usb_ep *_ep);

/**
 * This function can disconnect, or reconnect the host
 * and the device.
 */
int dwc_otg_gadget_pullup(struct usb_gadget *_gad, int _on);

/**
 * This is the callback used as a bridge to the
 * usb_gadget callback.
 */
void dwc_otg_gadget_complete(dwc_otg_core_request_t *_req);

#endif //__DWC_OTG_GADGET_H__

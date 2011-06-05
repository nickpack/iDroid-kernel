/*
 * dwc_otg_gadget.c - gadget API specific functions
 *
 * Author: Ricky Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Description: MIPS USB IP core family device controller
 *              Structures, registers and logging macros
 */

#include "dwc_otg_gadget.h"
#include "dwc_otg_hw.h"

#include <linux/errno.h>

/*
 * We are forced to use static variables, O' cursed
 * usb_gadget API!
 */
static dwc_otg_device_t *dwc_otg_gadget_device = NULL;
struct usb_gadget_driver *dwc_otg_gadget_driver = NULL;

/**
 * This structure stores function pointers
 * for all the operations the usb_gadget
 * framework can perform on any EP we have
 * exposed.
 */
static struct usb_ep_ops dwc_otg_ep_ops = {
	.enable = &dwc_otg_gadget_enable_ep,
	.disable = &dwc_otg_gadget_disable_ep,

	.alloc_request = &dwc_otg_gadget_alloc_request,
	.free_request = &dwc_otg_gadget_free_request,

	.queue = &dwc_otg_gadget_queue_request,
	.dequeue = &dwc_otg_gadget_dequeue_request,

	.set_halt = &dwc_otg_gadget_set_halt,
	.set_wedge = &dwc_otg_gadget_set_wedge,

	.fifo_status = &dwc_otg_gadget_fifo_status,
	.fifo_flush = &dwc_otg_gadget_fifo_flush,
};

/**
 * This structure stores function pointers
 * for all of the operations the usb_gadget
 * framework can perform on the gadget 
 * hardware.
 */
static struct usb_gadget_ops dwc_otg_gadget_ops = {
	.pullup = &dwc_otg_gadget_pullup,
};

/**
 * This structure stores the basic information
 * about our gadget hardware, it also contains a pointer
 * to @{link dwc_otg_gadget_ops}.
 */
struct usb_gadget dwc_otg_gadget = {
	.ops = &dwc_otg_gadget_ops,
	.name = DWC_OTG_DRIVER_NAME,

	.is_dualspeed = 1,
	.is_otg = 1,
	.is_a_peripheral = 1,

	.a_hnp_support = 1,
	.b_hnp_enable = 1,
	// TODO: Fill these in properly... >_>
};

/**
 * usb_gadget_register_driver
 *
 * This function is called when the gadget driver is loaded
 * to bind it to the controller.
 */
int usb_gadget_probe_driver(struct usb_gadget_driver *_dri,
		int (*_bind)(struct usb_gadget *))
{
	int ret = 0;
	int i;
	dwc_otg_core_t *core;
	dctl_data_t dctl = { .d32 = 0 };

	if(dwc_otg_gadget_device == NULL)
	{
		DWC_ERROR("Tried to register gadget before driver loaded.\n");
		return -EIO;
	}

	core = dwc_otg_gadget_device->core;

	// Set up device
	INIT_LIST_HEAD(&dwc_otg_gadget.ep_list);
	dev_set_name(&dwc_otg_gadget.dev, "gadget");
	dwc_otg_gadget.dev.driver = &_dri->driver;

	// Set up gadget structure
	for(i = 0; i < core->num_eps; i++)
	{
		dwc_otg_core_ep_t *ep = &core->endpoints[i];
		dwc_otg_gadget_ep_t *g_ep = (dwc_otg_gadget_ep_t*)kmalloc(sizeof(dwc_otg_gadget_ep_t), GFP_KERNEL);
		if(!g_ep)
		{
			DWC_ERROR("Failed to allocate usb_ep structure for endpoint %d.\n", i);
			break;
		}

		// Clear structure
		memset(g_ep, 0, sizeof(dwc_otg_gadget_ep_t));

		INIT_LIST_HEAD(&g_ep->usb_ep.ep_list);
		g_ep->usb_ep.driver_data = NULL;
		g_ep->usb_ep.name = ep->name;
		g_ep->usb_ep.ops = &dwc_otg_ep_ops;
		g_ep->usb_ep.maxpacket =  dwc_otg_mps_from_speed(ep->speed);
		g_ep->ep = ep;

		if(i > 0)
			list_add_tail(&g_ep->usb_ep.ep_list, &dwc_otg_gadget.ep_list);
		else
			dwc_otg_gadget.ep0 = &g_ep->usb_ep;
	}
	
	// Register device
	ret = device_register(&dwc_otg_gadget.dev);
	if(ret)
	{
		DWC_ERROR("Failed to register gadget driver!\n");
		return ret;
	}

	// Bind the gadget
	ret = _bind(&dwc_otg_gadget);
	if(ret)
	{
		DWC_ERROR("Failed to bind gadget driver!\n");
		return ret;
	}

	dctl.b.sftdiscon = 1;
	dwc_otg_modify_reg32(&core->device_registers->dctl, dctl.d32, 0);
	
	dwc_otg_gadget_driver = _dri;
	return ret;
}
EXPORT_SYMBOL(usb_gadget_probe_driver);

/**
 * usb_gadget_unregister_driver
 *
 * This function is called when the gadget driver is being released.
 */
int usb_gadget_unregister_driver(struct usb_gadget_driver* _dri)
{
	dwc_otg_gadget_ep_t *ep0 = dwc_otg_gadget_get_ep(dwc_otg_gadget.ep0);
	struct list_head *list_prev = &dwc_otg_gadget.ep_list;
	struct list_head *list_ptr = list_prev->next;
	dctl_data_t dctl = { .d32 = 0 };

	dctl.b.sftdiscon = 1;
	dwc_otg_modify_reg32(&dwc_otg_gadget_device->core->device_registers->dctl, 0, dctl.d32);

	dwc_otg_gadget_driver = NULL;

	_dri->unbind(&dwc_otg_gadget);

	while(list_ptr != &dwc_otg_gadget.ep_list)
	{
		struct usb_ep *usb_ep = list_entry(list_ptr, struct usb_ep, ep_list);
		dwc_otg_gadget_ep_t *g_ep = container_of(usb_ep, dwc_otg_gadget_ep_t, usb_ep);

		list_ptr = list_ptr->next;
		list_prev->next = list_ptr;
		list_ptr->prev = list_prev;
		list_prev = list_prev->next;

		kfree(g_ep);
	}
	kfree(ep0);

	return 0;
}
EXPORT_SYMBOL(usb_gadget_unregister_driver);

/**
 * usb_gadget_register_controller
 *
 * This is called to store the current controller for use in the
 * other usb_gadget* functions.
 */
int usb_gadget_register_controller(dwc_otg_device_t *_dev)
{
	dwc_otg_gadget_device = _dev;
	return 0;
}

/**
 * usb_gadget_unregister_controller
 *
 * This is called to release the controller.
 */
int usb_gadget_unregister_controller(dwc_otg_device_t *_dev)
{
	if(dwc_otg_gadget_device != _dev)
		return -ENXIO;

	return 0;
}

/**
 * This function enables an EP with the parameters required
 * for the supplied usb_endpoint_descriptor.
 */
int dwc_otg_gadget_enable_ep(struct usb_ep *_ep, const struct usb_endpoint_descriptor *_desc)
{
	dwc_otg_gadget_ep_t *ep = dwc_otg_gadget_get_ep(_ep);
	dwc_otg_core_enable_ep(dwc_otg_gadget_device->core, ep->ep, (struct usb_endpoint_descriptor*)_desc);

	DWC_VERBOSE("%s(%p, %p)\n", __func__, _ep, _desc);

	return 0;
}

/**
 * This function disables an EP.
 */
int dwc_otg_gadget_disable_ep(struct usb_ep *_ep)
{
	dwc_otg_gadget_ep_t *ep = dwc_otg_gadget_get_ep(_ep);
	
	DWC_VERBOSE("%s(%p)\n", __func__, _ep);

	dwc_otg_core_disable_ep(dwc_otg_gadget_device->core, ep->ep);
	return 0;
}

/**
 * This function allocates a request, to be filled in by the caller
 * which can then be queued to be sent to the host.
 */
struct usb_request *dwc_otg_gadget_alloc_request(struct usb_ep *_ep, gfp_t _gfp)
{
	dwc_otg_gadget_request_t *req = (dwc_otg_gadget_request_t*)kmalloc(sizeof(dwc_otg_gadget_request_t), _gfp);

	DWC_VERBOSE("%s(%p, %d) sz=%d\n", __func__, _ep, _gfp, sizeof(dwc_otg_gadget_request_t));

	// Clear the structure
	memset(req, 0, sizeof(dwc_otg_gadget_request_t));

	req->usb_ep = _ep;
	req->free = 1;
	//req->usb_request.dma = 0;
	//req->usb_request.zero = 0;

	return &req->usb_request;
}

/**
 * This function frees a request previously allocated by
 * dwc_otg_gadget_alloc_request.
 */
void dwc_otg_gadget_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	dwc_otg_gadget_request_t *req = dwc_otg_gadget_get_request(_req);

	DWC_VERBOSE("%s(%p, %p)\n", __func__, _ep, _req);

	//if(!req->dwc_request.active)
	//{
	//	if(req->free_dma)
	//		dma_free_coherent(NULL, req->dwc_request.buffer_length, req->dwc_request.dma_buffer, req->dwc_request.dma_address);

		kfree(req);
	//}
}

/**
 * This queues a request to be sent to the host.
 */
int dwc_otg_gadget_queue_request(struct usb_ep *_ep, struct usb_request *_req, gfp_t _gfp)
{
	dwc_otg_gadget_request_t *req = dwc_otg_gadget_get_request(_req);
	dwc_otg_gadget_ep_t *ep = dwc_otg_gadget_get_ep(_ep);

	// TODO: Use internal request rather than creating a new one!
	dwc_otg_core_request_t *cReq = kzalloc(sizeof(dwc_otg_core_request_t), _gfp);
	//dwc_otg_core_request_t *cReq = &req->dwc_request;

	memset(cReq, 0, sizeof(dwc_otg_core_request_t)); // Clear Structure

	cReq->dont_free = 1;
	cReq->request_type = DWC_EP_TYPE_CONTROL;
	cReq->direction = DWC_OTG_REQUEST_IN;
	cReq->buffer_length = _req->length;
	cReq->length = _req->length;
	cReq->buffer = _req->buf;
	cReq->cancelled = 0;
	cReq->cancelled_handler = NULL;
	cReq->data = (void*)req;
	cReq->zero = _req->zero;

	if(ep->ep->num != 0)
	{
		if(ep->ep->descriptor != NULL)
		{
			cReq->request_type = ep->ep->descriptor->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK; // Our DWC_OTG_XXX line up with the USB_XXX equivalents.
			cReq->direction = (ep->ep->direction == DWC_OTG_EP_OUT) ? DWC_OTG_REQUEST_OUT : DWC_OTG_REQUEST_IN;
		}
		else
		{
			DWC_ERROR("%s: tried to make a transfer but %s is disabled.\n", ep->ep->name, ep->ep->name);
			return -1;
		}
	}

#if defined(DEBUG)&&defined(VERBOSE)
	{
		char *dir = "Invalid";
		if(cReq->direction == DWC_OTG_REQUEST_IN)
			dir = "in";
		else if(cReq->direction == DWC_OTG_REQUEST_OUT)
			dir = "out";

		DWC_PRINT("%s(%p, %p, %d) me=%p, ep=%s, dir=%s, req=%p, len=%d\n", __func__,
				_ep, _req, _gfp, dwc_otg_gadget_device, ep->ep->name, dir, req, _req->length);
	}
#endif
	
	if(_req->dma != 0)
	{
		cReq->dma_buffer = _req->buf;
		cReq->dma_address = _req->dma;
		req->free_dma = 0;
	}
	else
	{
		cReq->dma_address = 0;
		cReq->dma_buffer = 0;
		req->free_dma = 1;
	}

	cReq->completed_handler = &dwc_otg_gadget_complete;
	dwc_otg_core_enqueue_request(dwc_otg_gadget_device->core, ep->ep, cReq);
	
	return 0;
}

/**
 * This function removes a request from the queue, providing it has not
 * already been sent.
 */
int dwc_otg_gadget_dequeue_request(struct usb_ep *_ep, struct usb_request *_req)
{
	DWC_VERBOSE("%s(%p, %p)\n", __func__, _ep, _req);
	// TODO: IMPL
	return 0;
}

/**
 * This sends a stall to the host if the value is 1,
 * or clears a previously set halt if 0.
 */
int dwc_otg_gadget_set_halt(struct usb_ep *_ep, int _val)
{
	dwc_otg_gadget_ep_t *ep = dwc_otg_gadget_get_ep(_ep);

	DWC_VERBOSE("%s(%p, %i) ep=%s\n", __func__, _ep, _val, ep->ep->name);

	if(_val > 0)
		dwc_otg_core_stall_ep(dwc_otg_gadget_device->core, ep->ep, _val);

	return 0;
}

/**
 * This stalls the endpoint and prevents it from being
 * restarted by the host.
 */
int dwc_otg_gadget_set_wedge(struct usb_ep *_ep)
{
	DWC_VERBOSE("%s(%p)\n", __func__, _ep);

	dwc_otg_gadget_set_halt(_ep, 2);
	return 0;
}

/**
 * This retrieves the fifo status of the EP.
 */
int dwc_otg_gadget_fifo_status(struct usb_ep *_ep)
{
	// TODO: IMPL
	return 0;
}

/**
 * This function flushes an EP's FIFO.
 */
void dwc_otg_gadget_fifo_flush(struct usb_ep *_ep)
{
	// TODO: IMPL
}

/**
 * This function can disconnect, or reconnect the host
 * and the device.
 */
int dwc_otg_gadget_pullup(struct usb_gadget *_gad, int _on)
{
	// TODO: Make soft disconnection something done on the device side,
	// rather than the gadget-side -- Ricky26
	/*if(dwc_otg_gadget_device)
	{
		dctl_data_t dctl = { .d32 = 0 };
		dctl.b.sftdiscon = 1;
		if(_on)
			dwc_otg_modify_reg32(&dwc_otg_gadget_device->core->device_registers->dctl, dctl.d32, 0);
		else
		{
			dwc_otg_modify_reg32(&dwc_otg_gadget_device->core->device_registers->dctl, 0, dctl.d32);
			dwc_otg_gadget.speed = USB_SPEED_UNKNOWN;
		}
	}*/

	return 0;
}

/**
 * This is the callback used as a bridge to the
 * usb_gadget callback.
 */
void dwc_otg_gadget_complete(dwc_otg_core_request_t *_req)
{
	dwc_otg_gadget_request_t *req = (dwc_otg_gadget_request_t*)_req->data; //container_of(_req, dwc_otg_gadget_request_t, dwc_request);

	DWC_VERBOSE("%s(%p) len=%d, ep=%s, cancel=%d\n", __func__, _req, _req->length, _req->ep->name, _req->cancelled);
	DWC_DEBUG("%s(%p) len=%d, ep=%s, cancel=%d\n", __func__, _req, _req->length, _req->ep->name, _req->cancelled);

	if(req->free_dma)
	{
		if(_req->dma_buffer && _req->buffer)
		{
			memcpy(_req->buffer, _req->dma_buffer, _req->length);
		}

		if(_req->dma_address)
			dma_free_coherent(NULL, _req->buffer_length, _req->dma_buffer, _req->dma_address);
	}

	req->usb_request.actual = (_req->length > 0) ? _req->length : 0;
	req->usb_request.status = (_req->cancelled == 0) ? 0 : -ESHUTDOWN;
	DWC_DEBUG("%s(%p) status=%d\n", __func__, _req, req->usb_request.status);
	if(req->usb_request.complete)
		req->usb_request.complete(req->usb_ep, &req->usb_request);

	if(req->free)
		kfree(_req);
}


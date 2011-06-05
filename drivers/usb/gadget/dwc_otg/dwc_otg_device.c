/*
 * dwc_otg_device.c - device-mode specific functions
 *
 * Author: Ricky Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "dwc_otg_device.h"
#include "dwc_otg_hw.h"
#include "dwc_otg_gadget.h"

/**
 * A helper function to deal with USB resets requested from interrupts.
 */
static void dwc_otg_device_usb_reset_work(struct work_struct *_work)
{
	dwc_otg_device_t *dev = container_of(_work, dwc_otg_device_t, reset_work);
	DWC_DEBUG("%s(%p)\n", __func__, dev);

	dwc_otg_device_usb_reset(dev);
	
	//INIT_WORK(_work, &dwc_otg_device_usb_reset_work);
}

/**
 * A helper function to deal with USB suspend being requested from interrupts.
 */
static void dwc_otg_device_usb_suspend_work(struct work_struct *_work)
{
	dwc_otg_device_t *dev = container_of(_work, dwc_otg_device_t, suspend_work);
	DWC_DEBUG("%s(%p)\n", __func__, dev);

	INIT_WORK(_work, &dwc_otg_device_usb_suspend_work);

	// Notify gadget driver
	if(dwc_otg_gadget_driver && dwc_otg_gadget_driver->suspend)
		dwc_otg_gadget_driver->suspend(&dwc_otg_gadget);
}

/**
 * A helper function to deal with disconnects.
 */
static void dwc_otg_device_disconnect_work(struct work_struct *_work)
{
	dwc_otg_device_t *dev = container_of(_work, dwc_otg_device_t, disconnect_work);
	DWC_DEBUG("%s(%p)\n", __func__, dev);

	INIT_WORK(_work, &dwc_otg_device_disconnect_work);

	// Notify gadget driver
	if(dwc_otg_gadget_driver && dwc_otg_gadget_driver->disconnect)
		dwc_otg_gadget_driver->disconnect(&dwc_otg_gadget);
}

/**
 * dwc_otg_device_init
 *
 * Initialise the dwc_otg_device structure, and the hardware
 * so that it can run in device mode.
 */
int dwc_otg_device_init(dwc_otg_device_t *_dev, dwc_otg_core_t *_core)
{
	int ret = 0;

	if(!_dev)
	{
		DWC_ERROR("%s passed a null device structure pointer!\n", __func__);
		return -EINVAL;
	}

	// Clear device structure.
	// (This should have been by the driver,
	//  but better safe than sorry.)
	memset(_dev, 0, sizeof(dwc_otg_device_t));
	INIT_WORK(&_dev->reset_work, &dwc_otg_device_usb_reset_work);
	INIT_WORK(&_dev->suspend_work, &dwc_otg_device_usb_suspend_work);
	INIT_WORK(&_dev->disconnect_work, &dwc_otg_device_disconnect_work);

	// Initialise members
	_dev->remote_wakeup = 1;

	if(!_core)
	{
		DWC_ERROR("%s passed a null core structure pointer!\n", __func__);
		return -EINVAL;
	}
	else
		_dev->core = _core;

	// Register IRQ.
	ret = request_irq(_core->irq, dwc_otg_device_irq, IRQF_SHARED, DWC_OTG_DRIVER_NAME, _dev);
	if(ret)
	{
		DWC_ERROR("Failed to register IRQ (%#x).\n", ret);
		return ret;
	}
	else
		_dev->irq = _core->irq;

	
	return 0;
}

/**
 * dwc_otg_device_destroy
 *
 * Release all resources used by device mode, and return
 * the hardware to its original state.
 */
void dwc_otg_device_destroy(dwc_otg_device_t *_dev)
{
	if(!_dev)
	{
		DWC_WARNING("%s passed a null device structure pointer!\n", __func__);
		return;
	}

	if(_dev->irq)
		free_irq(_dev->irq, _dev);
}

/**
 * dwc_otg_device_start
 *
 * Start device mode on the chip.
 */
int dwc_otg_device_start(dwc_otg_device_t *_dev)
{
	int ret = 0, i;
	diepmsk_data_t diepmsk = { .d32 = 0 };
	doepmsk_data_t doepmsk = { .d32 = 0 };

	if(!_dev)
	{
		DWC_ERROR("%s passed a null device structure pointer!\n", __func__);
		return -EINVAL;
	}

	DWC_VERBOSE("%s(%p)\n", __func__, _dev);

	// Enable Interrupts
	if(dwc_otg_device_enable_interrupts(_dev))
		DWC_WARNING("Failed to enable device interrupts.\n");

	// Clear D(I|O)PCTLs.
	for(i = 0; i < _dev->core->num_eps; i++)
	{
		dwc_otg_write_reg32(&_dev->core->in_ep_registers[i]->diepctl, 0);
		dwc_otg_write_reg32(&_dev->core->out_ep_registers[i]->doepctl, 0);
	}

	// Enable EP Interrupts
	diepmsk.b.xfercompl = 1;
	diepmsk.b.ahberr = 1;
	diepmsk.b.timeout = 1;
	diepmsk.b.epdisabled = 1;
	diepmsk.b.inepnakeff = 1;
	dwc_otg_write_reg32(&_dev->core->device_registers->diepmsk, diepmsk.d32);

	doepmsk.b.xfercompl = 1;
	doepmsk.b.setup = 1;
	doepmsk.b.back2backsetup = 1;
	doepmsk.b.epdisabled = 1;
	dwc_otg_write_reg32(&_dev->core->device_registers->doepmsk, doepmsk.d32);
	
	// Reset the device.
	dwc_otg_device_usb_reset(_dev);

	// We're all set up, tell the usb_gadget framework
	// that we exist.
	ret = usb_gadget_register_controller(_dev);
	if(ret)
	{
		DWC_ERROR("Failed to register controller.\n");
		return ret;
	}
	

	return 0;
}

/**
 * dwc_otg_device_stop
 *
 * Stop device mode on the chip.
 */
void dwc_otg_device_stop(dwc_otg_device_t *_dev)
{
	if(!_dev)
	{
		DWC_WARNING("%s passed a null device structure pointer!\n", __func__);
		return;
	}

	if(dwc_otg_device_disable_interrupts(_dev))
		DWC_WARNING("Failed to disable device interrupts.\n");
}

/**
 * dwc_otg_device_enable_interrupts
 *
 * enables interrupts.
 */
int dwc_otg_device_enable_interrupts(dwc_otg_device_t *_dev)
{
	gintmsk_data_t gintmsk = { .d32 = 0 };

	DWC_VERBOSE("%s(%p)\n", __func__, _dev);

	gintmsk.b.usbreset = 1;
	gintmsk.b.usbsuspend = 1;
	gintmsk.b.disconnect = 1;
	gintmsk.b.inepintr = 1;
	gintmsk.b.outepintr = 1;
	gintmsk.b.enumdone = 1;
	gintmsk.b.otgintr = 1;
	gintmsk.b.epmismatch = 1;
	dwc_otg_modify_reg32(&_dev->core->registers->gintmsk, 0, gintmsk.d32);

	return 0;
}

/**
 * dwc_otg_device_disable_interrupts
 *
 * disables interrupts.
 */
int dwc_otg_device_disable_interrupts(dwc_otg_device_t *_dev)
{
	gintmsk_data_t gintmsk = { .d32 = 0 };

	DWC_VERBOSE("%s(%p)\n", __func__, _dev);

	gintmsk.b.usbreset = 1;
	gintmsk.b.usbsuspend = 1;
	gintmsk.b.disconnect = 1;
	gintmsk.b.inepintr = 1;
	gintmsk.b.outepintr = 1;
	gintmsk.b.enumdone = 1;
	gintmsk.b.otgintr = 1;
	gintmsk.b.epmismatch = 1;
	dwc_otg_modify_reg32(&_dev->core->registers->gintmsk, gintmsk.d32, 0);

	return 0;
}

/**
 * dwc_otg_device_usb_reset
 *
 * Resets the USB connection and re-enables EP0.
 */
int dwc_otg_device_usb_reset(dwc_otg_device_t *_dev)
{
	diepmsk_data_t diepmsk = { .d32 = 0 };
	doepmsk_data_t doepmsk = { .d32 = 0 };
	dcfg_data_t dcfg = { .d32 = 0 };
	daint_data_t daint = { .d32 = 0 };
	unsigned long flags;

	DWC_VERBOSE("%s(%p)\n", __func__, _dev);

	spin_lock_irqsave(&_dev->core->lock, flags);

	// Clear Device Address
	dcfg.d32 = dwc_otg_read_reg32(&_dev->core->device_registers->dcfg);
	dcfg.b.devaddr = 0;
	dcfg.b.epmscnt = 1; // TODO: if shared fifo
	dwc_otg_write_reg32(&_dev->core->device_registers->dcfg, dcfg.d32);

	// Reset global NAK registers.
	_dev->core->global_in_nak_count = 0;
	_dev->core->global_out_nak_count = 0;
	dwc_otg_core_clear_global_in_nak(_dev->core);
	dwc_otg_core_clear_global_out_nak(_dev->core);

	// Reset EPs
	dwc_otg_core_ep_reset(_dev->core);

	// Enable interrupts on EP0
	daint.b.inep0 = 1;
	daint.b.outep0 = 1;
	dwc_otg_write_reg32(&_dev->core->device_registers->daintmsk, daint.d32);

	// Clear EP0 interrupts
	dwc_otg_write_reg32(&_dev->core->in_ep_registers[0]->diepint, 0xffffffff);
	dwc_otg_write_reg32(&_dev->core->out_ep_registers[0]->doepint, 0xffffffff);

	// Enable EP Interrupts
	diepmsk.b.xfercompl = 1;
	diepmsk.b.ahberr = 1;
	diepmsk.b.timeout = 1;
	diepmsk.b.epdisabled = 1;
	diepmsk.b.inepnakeff = 1;
	dwc_otg_write_reg32(&_dev->core->device_registers->diepmsk, diepmsk.d32);

	doepmsk.b.xfercompl = 1;
	doepmsk.b.setup = 1;
	doepmsk.b.back2backsetup = 1;
	doepmsk.b.epdisabled = 1;
	dwc_otg_write_reg32(&_dev->core->device_registers->doepmsk, doepmsk.d32);
	
	spin_unlock_irqrestore(&_dev->core->lock, flags);

	// Notify gadget driver
	if(dwc_otg_gadget_driver && dwc_otg_gadget_driver->resume)
		dwc_otg_gadget_driver->resume(&dwc_otg_gadget);

	return 0;
}

/** The EP0 USB descriptor! */
static struct usb_endpoint_descriptor ep0_descriptor = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = 0,
	.bmAttributes = 0,
};

/** The OUT request for EP0. */
static dwc_otg_core_request_t ep0_out_request = {
	.request_type = DWC_EP_TYPE_CONTROL,
	.direction = DWC_OTG_REQUEST_OUT,
	.completed_handler = &dwc_otg_device_complete_ep0,
	.dont_free = 1,	
	.buffer_length = 64,
	.dma_buffer = NULL,
};

/** The IN request for EP0. */
static dwc_otg_core_request_t ep0_in_request = {
	.request_type = DWC_EP_TYPE_CONTROL,
	.direction = DWC_OTG_REQUEST_IN,
	.dont_free = 1,
	.buffer_length = 64,
	.dma_buffer = NULL,
};

/**
 * The device USB IRQ handler.
 */
irqreturn_t dwc_otg_device_irq(int _irq, void *_dev)
{
	dwc_otg_device_t *dev = (dwc_otg_device_t*)_dev;
	dwc_otg_core_t *core = dev->core;
	gintsts_data_t gintsts = { .d32 = 0 };
	gintsts_data_t gintclr = { .d32 = 0 };

	gintsts.d32 = dwc_otg_read_reg32(&core->registers->gintsts) & dwc_otg_read_reg32(&core->registers->gintmsk);
	DWC_VERBOSE("%s(%d, %p) gintsts=0x%08x\n", __func__, _irq, _dev, gintsts.d32);

	if(gintsts.b.otgintr)
	{
		gotgint_data_t gotgint;
		
		gotgint.d32 = dwc_otg_read_reg32(&core->registers->gotgint);
		DWC_DEBUG("otgintr 0x%08x\n", gotgint.d32);

		if(gotgint.b.sesenddet)
		{
			dcfg_data_t dcfg = { .d32 = dwc_otg_read_reg32(&core->device_registers->dcfg) };

			DWC_DEBUG("session end detected\n");

			dcfg.b.nzstsouthshk = 1;
			dwc_otg_write_reg32(&core->device_registers->dcfg, dcfg.d32);

			DWC_DEBUG("DCFG=0x%08x\n", dcfg.d32);

			schedule_work(&dev->disconnect_work);
		}

		// Clear OTG interrupts
		dwc_otg_write_reg32(&core->registers->gotgint, 0xffffffff);

		gintclr.b.otgintr = 1;
	}

	if(gintsts.b.enumdone)
	{
		int dwcSpeed;
		int i;
		dsts_data_t dsts;

		DWC_DEBUG("enumdone\n");

		// Enable EP0.
		dwc_otg_core_enable_ep(core, &core->endpoints[0], &ep0_descriptor);

		//ep0_in_request.queued = 0;
		//ep0_out_request.queued = 0;
		
		// Listen for setup packets on EP0.
		dwc_otg_device_receive_ep0(_dev);

		// Tell gadget driver our top speed.
		dsts.d32 = dwc_otg_read_reg32(&core->device_registers->dsts);
		switch (dsts.b.enumspd)
		{
		case DWC_DSTS_ENUMSPD_HS_PHY_30MHZ_OR_60MHZ:
			dwc_otg_gadget.speed = USB_SPEED_HIGH;
			dwcSpeed = DWC_OTG_HIGH_SPEED;
			break;

		case DWC_DSTS_ENUMSPD_FS_PHY_30MHZ_OR_60MHZ:
		case DWC_DSTS_ENUMSPD_FS_PHY_48MHZ:
			dwc_otg_gadget.speed = USB_SPEED_FULL;
			dwcSpeed = DWC_OTG_FULL_SPEED;
			break;

		case DWC_DSTS_ENUMSPD_LS_PHY_6MHZ:
			dwc_otg_gadget.speed = USB_SPEED_LOW;
			dwcSpeed = DWC_OTG_LOW_SPEED;
			break;
		}
		
		// Update the endpoint speeds.
		for(i = 1; i < core->num_eps; i++)
			core->endpoints[i].speed = dwcSpeed;

		gintclr.b.enumdone = 1;
	}

	if(gintsts.b.usbreset)
	{
		DWC_DEBUG("usbreset\n");
		
		//schedule_work(&dev->reset_work);
		dwc_otg_device_usb_reset(dev);

		gintclr.b.usbreset = 1;
	}

	if(gintsts.b.usbsuspend)
	{
		DWC_DEBUG("usbsuspend\n");
		
		schedule_work(&dev->suspend_work);

		gintclr.b.usbsuspend = 1;
	}

	if(gintsts.b.disconnect)
	{
		DWC_DEBUG("disconnect\n");

		schedule_work(&dev->disconnect_work);
		gintclr.b.disconnect = 1;
	}

	if(gintsts.b.epmismatch)
	{
		dtknq1_data_t t1;
		int t2, t3, t4;
		int i;
		int num_tokens;
		int doneEPs = 0;
		dwc_otg_core_request_t *req;

		DWC_ERROR("EP Mismatch.\n");

		dwc_otg_core_set_global_in_nak(dev->core);
		for(i = 0; i < dev->core->num_eps; i++)
		{
			dwc_otg_core_ep_t *ep = &core->endpoints[i];
			req = list_first_entry(&ep->transfer_queue, dwc_otg_core_request_t, queue_pointer);
			if(req && req->direction == DWC_OTG_REQUEST_IN)
			{
				dwc_otg_core_cancel_ep(dev->core, ep);
			}
		}
		dwc_otg_core_clear_global_in_nak(dev->core);

		t1.d32 = dwc_otg_read_reg32(&dev->core->device_registers->dtknqr1);
		t2 = dwc_otg_read_reg32(&dev->core->device_registers->dtknqr2);
		t3 = dwc_otg_read_reg32(&dev->core->device_registers->dtknqr3_dthrctl);
		t4 = dwc_otg_read_reg32(&dev->core->device_registers->dtknqr4_fifoemptymsk);

		num_tokens = min((int)dev->core->hwcfg2.b.dev_token_q_depth, (int)t1.b.intknwptr);

		DWC_PRINT("Requeing %d requests.\n", num_tokens);

		for(i = 0; i < num_tokens; i++)
		{
			int ep;

#define GET_BITS(x, start, len) (((x) << ((sizeof(x)*8)-(len)-(start))) >> ((sizeof(x)*8)-(len)))

			if(i < 6)
				ep = GET_BITS((int)t1.b.epnums0_5, 8 + (i*4), 4);
			else if(i < 14)
				ep = GET_BITS(t2, (i-6)*4, 4);
			else if(i < 21)
				ep = GET_BITS(t3, (i-14)*4, 4);
			else if(i < 30)
				ep = GET_BITS(t4, (i-21)*4, 4);
			else
				break;

			if(doneEPs & (1 << ep))
				break;

			DWC_ERROR("Requeing %d.\n", ep);
	
			req = list_first_entry(&core->endpoints[i].transfer_queue, dwc_otg_core_request_t, queue_pointer);
			if(req && req->direction == DWC_OTG_REQUEST_IN)
				dwc_otg_core_start_request(core, req);
			else
				DWC_ERROR("Tried to requeue invalid request %p.\n", req);

			doneEPs |= (1 << ep);
		}

		for(i = 0; i < dev->core->num_eps; i++)
		{
			if(doneEPs & (1 << i))
				continue;
	
			req = list_first_entry(&core->endpoints[i].transfer_queue, dwc_otg_core_request_t, queue_pointer);
			if(req && req->direction == DWC_OTG_REQUEST_IN)
				dwc_otg_core_start_request(core, req);
			//else
			//	DWC_ERROR("Tried to requeue invalid request %p.\n", req);
		}

		gintclr.b.epmismatch = 1;
	}

	if(gintsts.b.inepintr || gintsts.b.outepintr)
	{
		daint_data_t daint = { .d32 = 0 };
		int i;

		DWC_DEBUG("epint\n");

		daint.d32 = dwc_otg_read_reg32(&core->device_registers->daint) & dwc_otg_read_reg32(&core->device_registers->daintmsk);

		for(i = 0; i < core->num_eps; i++)
		{
			if(daint.ep.in & 1)
				dwc_otg_device_handle_in_interrupt(dev, i);

			if(daint.ep.out & 1)
				dwc_otg_device_handle_out_interrupt(dev, i);

			daint.ep.out >>= 1;
			daint.ep.in >>= 1;
		}
	}

	// Clear the interrupts
	gintsts.d32 &= ~gintclr.d32;
	dwc_otg_write_reg32(&core->registers->gintsts, gintclr.d32);
	return gintsts.d32 != 0 ? IRQ_NONE : IRQ_HANDLED;
}

/**
 * The in endpoint interrupt handler.
 */
irqreturn_t dwc_otg_device_handle_in_interrupt(dwc_otg_device_t *_dev, int _ep)
{
	dwc_otg_core_ep_t *ep = &_dev->core->endpoints[_ep];
	diepint_data_t depint = { .d32 = 0 };
	diepint_data_t depclr = { .d32 = 0 };

	DWC_VERBOSE("%s(%p, %d)\n", __func__, _dev, _ep);

	depint.d32 = dwc_otg_read_reg32(&ep->in_registers->diepint) & dwc_otg_read_reg32(&_dev->core->device_registers->diepmsk);

	if(!ep->exists)
	{
		DWC_ERROR("%s called on non-existant in endpoint %d.\n", __func__, _ep);
		return -ENXIO;
	}

	if(depint.b.inepnakeff)
	{
		DWC_DEBUG("in nak eff %s\n", ep->name);

		complete_all(&ep->nakeff_completion);
		init_completion(&ep->nakeff_completion);

		depclr.b.inepnakeff = 1;
	}

	if(depint.b.epdisabled)
	{
		DWC_DEBUG("in epdisabled %s\n", ep->name);

		complete_all(&ep->disabled_completion);
		init_completion(&ep->disabled_completion);

		depclr.b.epdisabled = 1;
	}

	/*if(depint.b.intknepmis)
	{
		gintsts_data_t gintsts = { .d32 = 0 };

		DWC_DEBUG("in tn mis\n");

		gintsts.b.epmismatch = 1;
		dwc_otg_modify_reg32(&_dev->core->registers->gintsts, gintsts.d32, 0);
	}*/

	if(depint.b.intktxfemp)
	{
		DWC_DEBUG("in tx f emp\n");

		// Do nothing... ;_;
		
		depclr.b.intktxfemp = 1;
	}

	if(depint.b.timeout)
	{
		DWC_DEBUG("in timeout %s\n", ep->name);

		// Do something!?
		// Cancel sending?!
		
		depclr.b.timeout = 1;
	}

	if(depint.b.ahberr)
	{
		DWC_DEBUG("in ahberr\n");

		// Do nothing again... :'(
		
		depclr.b.ahberr = 1;
	}

	if(depint.b.xfercompl)
	{
		dwc_otg_core_request_t *req = list_first_entry(&ep->transfer_queue, dwc_otg_core_request_t, queue_pointer);

		DWC_DEBUG("in xfercompl %s\n", ep->name);

		if(list_empty(&ep->transfer_queue) || req->direction != DWC_OTG_REQUEST_IN)
		{
			DWC_ERROR("XferCompl received when we weren't transferring anything!\n");
		}
		else
			dwc_otg_core_complete_request(_dev->core, req);
		
		depclr.b.xfercompl = 1;
	}

	// Clear interrupts
	dwc_otg_write_reg32(&ep->in_registers->diepint, depclr.d32);
	
	return 0;
}

/**
 * The out endpoint interrupt handler.
 */
irqreturn_t dwc_otg_device_handle_out_interrupt(dwc_otg_device_t *_dev, int _ep)
{
	dwc_otg_core_ep_t *ep = &_dev->core->endpoints[_ep];
	doepint_data_t depint = { .d32 = 0 };
	doepint_data_t depclr = { .d32 = 0 };

	DWC_VERBOSE("%s(%p, %d)\n", __func__, _dev, _ep);

	depint.d32 = dwc_otg_read_reg32(&ep->out_registers->doepint) & dwc_otg_read_reg32(&_dev->core->device_registers->doepmsk);

	if(!ep->exists)
	{
		DWC_ERROR("%s called on non-existant in endpoint %d.\n", __func__, _ep);
		return -ENXIO;
	}

	if(depint.b.setup)
	{
		dwc_otg_core_request_t *req;

		// We received a setup packet, yaaay! 
		DWC_VERBOSE("setup\n");

		// If we had a setup packet, set the flag on the
		// request that says it was prepended by a setup
		// token.
		req = list_first_entry(&ep->transfer_queue, dwc_otg_core_request_t, queue_pointer);
		if(!list_empty(&ep->transfer_queue) && req->direction == DWC_OTG_REQUEST_OUT)
		{
			req->setup = 1;
			dwc_otg_core_complete_request(_dev->core, req);
		}
		else
			DWC_ERROR("Setup Packet Received without us initiating a transfer!\n");

		depclr.b.setup = 1;
	}

	if(depint.b.back2backsetup)
	{
		// Do nothing
		DWC_DEBUG("back2backsetup\n");

		depclr.b.back2backsetup = 1;
	}

	if(depint.b.epdisabled)
	{
		DWC_DEBUG("out epdisabled\n");

		complete_all(&ep->disabled_completion);
		init_completion(&ep->disabled_completion);

		depclr.b.epdisabled = 1;
	}

	if(depint.b.ahberr)
	{
		// Do nothing again... :'(
		DWC_DEBUG("out ahberr\n");

		depclr.b.ahberr = 1;
	}
		
	if(depint.b.outtknepdis)
	{
		// Nothing to do again! Wah... :''(
		DWC_DEBUG("out tkn epdis\n");

		depclr.b.outtknepdis = 1;
	}

	if(depint.b.xfercompl)
	{
		dwc_otg_core_request_t *req = list_first_entry(&ep->transfer_queue, dwc_otg_core_request_t, queue_pointer);

		DWC_DEBUG("out xfercompl\n");

		// Yay! :D

		if(list_empty(&ep->transfer_queue) || req->direction != DWC_OTG_REQUEST_OUT)
		{
			DWC_ERROR("XferCompl received when we weren't transferring anything!\n");
		}
		else
			dwc_otg_core_complete_request(_dev->core, req);

		depclr.b.xfercompl = 1;
	}

	// Clear interrupts
	dwc_otg_write_reg32(&ep->out_registers->doepint, depclr.d32);

	return 0;
}

/**
 * dwc_otg_device_receive_ep0
 */
void dwc_otg_device_receive_ep0(dwc_otg_device_t *_dev)
{
	ep0_out_request.data = _dev;
	ep0_out_request.length = 8; //ep0_out_request.buffer_length;

	if(ep0_out_request.dma_buffer == NULL)
	{
		ep0_out_request.dma_buffer = dma_alloc_coherent(NULL, ep0_out_request.buffer_length, &ep0_out_request.dma_address, GFP_KERNEL);
		if(!ep0_out_request.dma_buffer)
		{
			DWC_ERROR("Failed to allocate setup buffer for EP0!\n");
			return;
		}

		// As we've allocated a DMA buffer, we don't
		// need another. :D -- Ricky26
		ep0_out_request.buffer = ep0_out_request.dma_buffer;

		// Setup the in request, as we might need it as a response.
		ep0_in_request.length = ep0_in_request.buffer_length;
		ep0_in_request.dma_buffer = dma_alloc_coherent(NULL, ep0_in_request.buffer_length, &ep0_in_request.dma_address, GFP_KERNEL);
		if(!ep0_in_request.dma_buffer)
		{
			DWC_ERROR("Failed to allocate in buffer for EP0!\n");
			return;
		}

		ep0_in_request.buffer = ep0_in_request.dma_buffer;
	}

	DWC_VERBOSE("%s: enqueue (%p):%d\n", __func__, &ep0_out_request, ep0_out_request.length);
	dwc_otg_core_enqueue_request(_dev->core, &_dev->core->endpoints[0], &ep0_out_request);
}

/**
 * dwc_otg_device_send_ep0
 */
void dwc_otg_device_send_ep0(dwc_otg_device_t *_dev)
{
	ep0_in_request.data = _dev;
	dwc_otg_core_enqueue_request(_dev->core, &_dev->core->endpoints[0], &ep0_in_request);
}

/**
 * dwc_otg_device_complete_ep0
 *
 * This is called to complete an interrupt on EP0.
 */
void dwc_otg_device_complete_ep0(dwc_otg_core_request_t *_req)
{
	dwc_otg_core_t *core = _req->core;
	dwc_otg_device_t *dev = (dwc_otg_device_t*)_req->data;
	
	if(_req->cancelled)
	{
		DWC_VERBOSE("Shutting down EP0 control channel.\n");
		return;
	}

	if(_req->setup)
	{
		struct usb_ctrlrequest *packet = (struct usb_ctrlrequest*)_req->buffer;

#if defined(VERBOSE)&&defined(DEBUG)
		uint8_t *b = (uint8_t*)packet;
		
		DWC_PRINT("EP0 Sent us a setup packet! :3\n");
		DWC_PRINT("%02x %02x %02x %02x %02x %02x %02x %02x\n", 
				b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
		DWC_PRINT("RT %d, R %d, w %d\n", packet->bRequestType & 0x7F, packet->bRequest, packet->wIndex);
#endif

		if((packet->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD)
		{
			int set = 0;

			switch(packet->bRequest)
			{
				case USB_REQ_GET_STATUS:
					{
						uint16_t *result = (uint16_t*)ep0_in_request.buffer;

						switch (packet->bRequestType & USB_RECIP_MASK)
						{
						case USB_RECIP_DEVICE:
							*result = 0x1 | (1 << dev->remote_wakeup); // Self powered, remote wakeup enabled.
							break;

						case USB_RECIP_INTERFACE:
							*result = 0;
							break;

						case USB_RECIP_ENDPOINT:
							{
								// TODO: check this!
								int epnum = packet->wIndex & 0xf;
								if(epnum < core->num_eps &&
										list_empty(&core->endpoints[epnum].transfer_queue))
								{
									*result = 0;
									break;
								}

								*result = 1;
							}
							break;
						}
						
						ep0_in_request.length = 2;
						dwc_otg_device_send_ep0(dev);
					}
					goto exit;

				case USB_REQ_SET_FEATURE:
					set = 1;
					// fall through
				case USB_REQ_CLEAR_FEATURE:

					switch (packet->bRequestType & USB_RECIP_MASK)
					{
						case USB_RECIP_DEVICE:
							switch(packet->wValue)
							{
								case USB_DEVICE_REMOTE_WAKEUP:
									dev->remote_wakeup = set;
									break;
							}
							break;

						case USB_RECIP_ENDPOINT:
							{
								int ep = packet->wValue;
								if(ep == 0 || ep > core->num_eps)
									goto stall;

								dwc_otg_core_stall_ep(core, &core->endpoints[ep], set);
							}
							break;

					}
					goto send_zlp;

				case USB_REQ_SET_ADDRESS:
					{
						dcfg_data_t dcfg = { .d32 = 0 };

						dcfg.d32 = dwc_otg_read_reg32(&core->device_registers->dcfg);
						dcfg.b.devaddr = packet->wValue;
						dwc_otg_write_reg32(&core->device_registers->dcfg, dcfg.d32);

						DWC_VERBOSE("Received address %d.\n", dcfg.b.devaddr);
					}
					goto send_zlp;
			}
		}

		// If we got here we don't know how to handle
		// the setup packet, so let's ask the gadget driver to
		// do it, and then blame it if it doesn't work...

		DWC_VERBOSE("Passing setup packet to gadget driver.\n");
		if(dwc_otg_gadget_driver == NULL)
		{
			DWC_WARNING("No gadget driver yet, stalling.\n");
			goto stall;
		}

		if(dwc_otg_gadget_driver->setup(&dwc_otg_gadget, packet))
		{
			DWC_ERROR("Got a setup packet we don't handle properly yet.\n");
			goto stall;
		}

		goto exit;
	}
	else if(ep0_out_request.length == 0)
	{
		DWC_VERBOSE("Received a ZLP from host.\n");
	}
	else
	{
		DWC_ERROR("EP0 sent us some shit we don't know how to handle.\n");
	}

	goto exit;

stall:
	//dwc_otg_core_stall_ep(core, &core->endpoints[0], 1);
	{
		depctl_data_t depctl = { .d32 = dwc_otg_read_reg32(&core->in_ep_registers[0]->diepctl) };
		depctl.b.stall = 1;
		depctl.b.cnak = 1;
		dwc_otg_write_reg32(&core->in_ep_registers[0]->diepctl, depctl.d32);
	}
	goto exit;

send_zlp:
	ep0_in_request.length = 0;
	dwc_otg_device_send_ep0(dev);

exit:
	dwc_otg_device_receive_ep0(dev);
}

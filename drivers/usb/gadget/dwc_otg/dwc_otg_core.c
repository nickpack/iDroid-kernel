/*
 * dwc_otg_core.c - core functions for the DWC OTG chip
 *
 * Author: Ricky Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "dwc_otg_core.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "dwc_otg_hw.h"

/**
 * dwc_otg_core_init_eps
 *
 * This is a helper function for dwc_otg_core_init that
 * sets up the EP structures.
 */
int dwc_otg_core_init_eps(dwc_otg_core_t *_core)
{
	int i;

	// Clear EP data structure (this sets them all as inactive).
	memset(&_core->endpoints, 0, sizeof(_core->endpoints));

	for(i = 0; i < MAX_EPS_CHANNELS; i++)
	{
		uint32_t ret = 0;

		_core->in_ep_registers[i] = (dwc_otg_dev_in_ep_regs_t*)(((uint8_t*)_core->registers) + DWC_OTG_IN_EP_OFFSET + (DWC_OTG_IN_EP_SIZE * i));
		_core->out_ep_registers[i] = (dwc_otg_dev_out_ep_regs_t*)(((uint8_t*)_core->registers) + DWC_OTG_OUT_EP_OFFSET + (DWC_OTG_OUT_EP_SIZE * i));

		// Setup Endpoint
		_core->endpoints[i].num = i;
		_core->endpoints[i].speed = 0;
		_core->endpoints[i].active = 0;
		_core->endpoints[i].descriptor = NULL;
		_core->endpoints[i].in_registers = _core->in_ep_registers[i];
		_core->endpoints[i].out_registers = _core->out_ep_registers[i];
		INIT_LIST_HEAD(&_core->endpoints[i].transfer_queue);
		spin_lock_init(&_core->endpoints[i].lock);
		init_completion(&_core->endpoints[i].nakeff_completion);
		init_completion(&_core->endpoints[i].disabled_completion);

		ret = DWC_OTG_EP_DIRECTION(dwc_otg_read_reg32(&_core->registers->ghwcfg1), i);
		if(ret == DWC_OTG_EP_IN)
			ret = snprintf(_core->endpoints[i].name, DWC_OTG_EP_MAX_NAME_SIZE, "ep%din", i);
		else if(ret == DWC_OTG_EP_OUT)
			ret = snprintf(_core->endpoints[i].name, DWC_OTG_EP_MAX_NAME_SIZE, "ep%dout", i);
		else
			ret = snprintf(_core->endpoints[i].name, DWC_OTG_EP_MAX_NAME_SIZE, "ep%d", i);

		if(ret > DWC_OTG_EP_MAX_NAME_SIZE)
		{
			DWC_WARNING("EP name buffer too small.\n");
		}

		/*
		 * This is where we would check the direction,
		 * but it seems that upon further inspection of the
		 * documentation floating around on the internet,
		 * hwcfg1 does in fact _not_ contain the EP
		 * capabilities, but in fact the _current_ ep 
		 * layout. -- Ricky26
		 */
		if(i < _core->num_eps)
			_core->endpoints[i].exists = 1;

		if(i == 0)
			_core->endpoints[i].speed = DWC_OTG_FULL_SPEED;
	}

	return 0;
}

/**
 * dwc_otg_core_init
 *
 * This function initialises the hardware and the
 * structure specified by _core.
 */
int dwc_otg_core_init(dwc_otg_core_t *_core, int _irq, void *_regs, void *_phy)
{
	int i;
	int ret = 0;
	dctl_data_t dctl = { .d32 = 0 };
	pcgcctl_data_t pcgcctl = { .d32 = 0 };

	DWC_VERBOSE("%s(%p, %d, %p, %p)\n", __func__, _core, _irq, _regs, _phy);

	if(!_core)
	{
		DWC_ERROR("%s called with null core pointer.\n", __func__);
		return -EINVAL;
	}

	// Clear core structure.
	// (This should have already been done by the driver interface,
	//  but better safe than sorry.)
	memset(_core, 0, sizeof(dwc_otg_core_t));

	// Initialise internal variables
	spin_lock_init(&_core->lock);

	// Copy parameters into core.
	_core->registers = (dwc_otg_core_global_regs_t*)_regs;
	_core->phy_registers = (dwc_otg_phy_regs_t*)_phy;
	_core->device_registers = (dwc_otg_device_global_regs_t*)(((uint8_t*)_regs) + DWC_OTG_DEVICE_OFFSET);
	_core->pcgcctl = (uint32_t*)(((uint8_t*)_regs) + DWC_OTG_PCGCCTL_OFFSET);

	// Initialise hardware.
	dwc_otg_hw_power(_core, 0); // Make sure everything is reset, incase this
								// hardware was used in the bootloader, for example.
	dwc_otg_hw_power(_core, 1);

	// Read Hardware Configuration Registers
	_core->hwcfg1.d32 = dwc_otg_read_reg32(&_core->registers->ghwcfg1);
	_core->hwcfg2.d32 = dwc_otg_read_reg32(&_core->registers->ghwcfg2);
	_core->hwcfg3.d32 = dwc_otg_read_reg32(&_core->registers->ghwcfg3);
	_core->hwcfg4.d32 = dwc_otg_read_reg32(&_core->registers->ghwcfg4);

	_core->num_eps = _core->hwcfg2.b.num_dev_ep + 1;
	DWC_DEBUG("%d endpoints detected.\n", _core->num_eps);

	// Initialise EP structures
	dwc_otg_core_init_eps(_core);
	
	// Send soft disconnect
	dctl.b.sftdiscon = 1;
	dwc_otg_modify_reg32(&_core->device_registers->dctl, 0, dctl.d32);
	msleep(4);

	// Power on core.
	dwc_otg_write_reg32(_core->pcgcctl, pcgcctl.d32);
	udelay(100);

	if(_core->phy_registers)
	{
		phypwr_data_t phypwr = { .d32 = 0 };	
		phyclk_data_t phyclk = { .d32 = 0 };
		rstcon_data_t rstcon = { .d32 = 0 };

		DWC_DEBUG("PHY Registers Detected.\n");

		// Power on PHY, if we have one.
		dwc_otg_write_reg32(&_core->phy_registers->phypwr, phypwr.d32);
		udelay(10);

		// Select 48Mhz
		phyclk.b.clksel = DWC_OTG_PHYCLK_CLKSEL_48MHZ;
		dwc_otg_modify_reg32(&_core->phy_registers->phyclk, DWC_OTG_PHYCLK_CLKSEL_MASK, phyclk.d32);

		// Reset PHY
		rstcon.b.physwrst = 1;
		dwc_otg_modify_reg32(&_core->phy_registers->phyclk, 0, rstcon.d32);
		udelay(20);
		dwc_otg_modify_reg32(&_core->phy_registers->phyclk, rstcon.d32, 0);
		msleep(1);
	}

	dwc_otg_core_soft_reset(_core);

	// Clear soft disconnect
	dwc_otg_modify_reg32(&_core->device_registers->dctl, dctl.d32, 0);
	msleep(4);

	// Clear interrupts
	dwc_otg_write_reg32(&_core->in_ep_registers[_core->num_eps]->diepint, 0xffffffff); // No idea why this is here, reverse-engineered from iBoot.
	dwc_otg_write_reg32(&_core->out_ep_registers[_core->num_eps]->doepint, 0xffffffff);

	for(i = 0; i < _core->num_eps; i++)
	{
		dwc_otg_write_reg32(&_core->in_ep_registers[i]->diepint, 0xffffffff);
		dwc_otg_write_reg32(&_core->out_ep_registers[i]->doepint, 0xffffffff);
	}

	dwc_otg_write_reg32(&_core->registers->gintmsk, 0);
	dwc_otg_write_reg32(&_core->device_registers->diepmsk, 0);
	dwc_otg_write_reg32(&_core->device_registers->doepmsk, 0);

	// Register IRQ.
	ret = request_irq(_irq, dwc_otg_core_irq, IRQF_SHARED, DWC_OTG_DRIVER_NAME, _core);
	if(ret)
	{
		DWC_ERROR("Failed to register IRQ (%#x).\n", ret);
		return ret;
	}
	else
		_core->irq = _irq;

	return 0;
}

/**
 * dwc_otg_core_destroy
 *
 * This shuts down the hardware and releases any resources
 * obtained whilst the driver was active.
 *
 * This function will not free _core.
 */
void dwc_otg_core_destroy(dwc_otg_core_t *_core)
{
	DWC_VERBOSE("%s(%p)\n", __func__, _core);

	if(!_core)
	{
		DWC_ERROR("%s called with null core pointer.\n", __func__);
		return;
	}

	// Shutdown hardware.
	dwc_otg_hw_power(_core, 0);

	// Release IRQ
	if(_core->irq)
		free_irq(_core->irq, _core);
}

/**
 * Helper function for soft_reset, waits for a certain register
 * to have a certain value.
 */
inline int helper_wait_for_reg(volatile uint32_t *_reg, uint32_t _mask, uint32_t _val)
{
	uint32_t count = 0;
	uint32_t curr = dwc_otg_read_reg32(_reg);

	if(_mask == 0)
		return -EINVAL;

	while((curr & _mask) != _val)
	{
		count++;
		if(count > 1000)
		{
			DWC_ERROR("Waited 10 seconds in dwc_otg_core_soft_reset. Bailing.");
			return -EIO;
		}

		msleep(10);
		curr = dwc_otg_read_reg32(_reg);
	}

	return 0;
}

/**
 * dwc_otg_core_soft_reset
 *
 * Soft Resets the core.
 */
int dwc_otg_core_soft_reset(dwc_otg_core_t *_core)
{
	grstctl_data_t grstctl = { .d32 = 0 };
	int ret = 0;

	DWC_VERBOSE("%s(%p)\n", __func__, _core);

	if(!_core)
	{
		DWC_ERROR("%s called with null core pointer.\n", __func__);
		return -EINVAL;
	}

	// Wait for AHBIDLE
	grstctl.b.ahbidle = 1;
	ret = helper_wait_for_reg(&_core->registers->grstctl, grstctl.d32, grstctl.d32);

	if(ret)
		return ret;

	grstctl.b.ahbidle = 0;

	// Set core reset flag
	grstctl.b.csftrst = 1;
	dwc_otg_modify_reg32(&_core->registers->grstctl, grstctl.d32, 0);
	ret = helper_wait_for_reg(&_core->registers->grstctl, grstctl.d32, 0);
	if(ret)
		return ret;

	grstctl.b.csftrst = 0;

	// Wait for AHBIDLE
	grstctl.b.ahbidle = 1;
	ret = helper_wait_for_reg(&_core->registers->grstctl, ~grstctl.d32, 0);
	if(ret)
		return ret;

	msleep(1);

	return 0;
}

/**
 * Set the global non-periodic in nak register, if it's not
 * already set. It will be cleared after the same amount of 
 * clear calls have been made as set calls.
 */
int dwc_otg_core_set_global_in_nak(dwc_otg_core_t *_core)
{
	_core->global_in_nak_count++;

	if(_core->global_in_nak_count <= 1)
	{
		dctl_data_t dctl = { .d32 = 0 };

		DWC_DEBUG("SGNPINNAK\n");

		_core->global_in_nak_count = 1;

		dctl.d32 = dwc_otg_read_reg32(&_core->device_registers->dctl);
		if(!dctl.b.gnpinnaksts)
		{
			dctl.b.sgnpinnak = 1;
			dwc_otg_write_reg32(&_core->device_registers->dctl, dctl.d32);

			while(!dctl.b.gnpinnaksts)
			{
				dctl.d32 = dwc_otg_read_reg32(&_core->device_registers->dctl);
				udelay(50);
			}
		}
	}

	return _core->global_in_nak_count;
}

/**
 * Clear the non-periodic global out nak register.
 */
int dwc_otg_core_clear_global_in_nak(dwc_otg_core_t *_core)
{
	_core->global_in_nak_count--;

	if(_core->global_in_nak_count <= 0)
	{
		dctl_data_t dctl = { .d32 = 0 };
		_core->global_in_nak_count = 0;

		DWC_DEBUG("CGNPINNAK\n");

		dctl.d32 = dwc_otg_read_reg32(&_core->device_registers->dctl);
		dctl.b.cgnpinnak = 1;
		dwc_otg_write_reg32(&_core->device_registers->dctl, dctl.d32);
	}

	return _core->global_in_nak_count;
}

/**
 * Set the global out nak register.
 */
int dwc_otg_core_set_global_out_nak(dwc_otg_core_t *_core)
{
	_core->global_out_nak_count++;

	if(_core->global_out_nak_count <= 1)
	{
		dctl_data_t dctl = { .d32 = 0 };

		_core->global_out_nak_count = 1;

		DWC_DEBUG("SGOUTNAK\n");

		dctl.d32 = dwc_otg_read_reg32(&_core->device_registers->dctl);
		if(!dctl.b.goutnaksts)
		{
			dctl.b.sgoutnak = 1;
			dwc_otg_write_reg32(&_core->device_registers->dctl, dctl.d32);

			while(!dctl.b.goutnaksts)
			{
				udelay(50);
				dctl.d32 = dwc_otg_read_reg32(&_core->device_registers->dctl);
			}
		}
	}

	return _core->global_out_nak_count;
}

/**
 * Clear the global out nak register.
 */
int dwc_otg_core_clear_global_out_nak(dwc_otg_core_t *_core)
{
	_core->global_out_nak_count--;

	if(_core->global_out_nak_count <= 0)
	{
		dctl_data_t dctl = { .d32 = 0 };
		_core->global_out_nak_count = 0;

		DWC_DEBUG("CGOUTNAK\n");

		dctl.d32 = dwc_otg_read_reg32(&_core->device_registers->dctl);
		dctl.b.cgoutnak = 1;
		dwc_otg_write_reg32(&_core->device_registers->dctl, dctl.d32);
	}

	return _core->global_out_nak_count;
}

/**
 * Flush one of the core's TX fifos.
 */
int dwc_otg_core_flush_tx_fifo(dwc_otg_core_t *_core, int _fnum)
{
	grstctl_data_t grstctl;
	
	grstctl.d32 = dwc_otg_read_reg32(&_core->registers->grstctl);
	
	while(!grstctl.b.ahbidle)
		grstctl.d32 = dwc_otg_read_reg32(&_core->registers->grstctl);

	grstctl.b.txfflsh = 1;
	grstctl.b.txfnum = _fnum;

	dwc_otg_write_reg32(&_core->registers->grstctl, grstctl.d32);

	while(grstctl.b.txfflsh)
		grstctl.d32 = dwc_otg_read_reg32(&_core->registers->grstctl);

	return 0;
}

/**
 * dwc_otg_core_start
 *
 * Start chip operation.
 */
int dwc_otg_core_start(dwc_otg_core_t *_core)
{
	int i = 0;
	gahbcfg_data_t gahbcfg = { .d32 = 0 };
	gusbcfg_data_t gusbcfg = { .d32 = 0 };
	dcfg_data_t dcfg = { .d32 = 0 };
	dctl_data_t dctl = { .d32 = 0 };
	gotgctl_data_t gotgctl = { .d32 = 0 };
	gintmsk_data_t gintmsk = { .d32 = 0 };

	DWC_VERBOSE("%s(%p)\n", __func__, _core);

	// Set some configuration
	gahbcfg.b.glblintrmsk = 1; // Enable Interrupts
	gahbcfg.b.dmaenable = DWC_GAHBCFG_DMAENABLE;
	gahbcfg.b.hburstlen = DWC_GAHBCFG_INT_DMA_BURST_INCR8;
	dwc_otg_write_reg32(&_core->registers->gahbcfg, gahbcfg.d32);

	// Set USB Configuration
	gusbcfg.b.phyif = 1;
	gusbcfg.b.srpcap = 1;
	gusbcfg.b.hnpcap = 1;
	gusbcfg.b.usbtrdtim = 5;
	dwc_otg_write_reg32(&_core->registers->gusbcfg, gusbcfg.d32);

	// Set Device Configuration
	dcfg.b.devspd = DWC_DCFG_HIGH_SPEED;
	dcfg.b.perfrint = DWC_DCFG_FRAME_INTERVAL_80;
	dcfg.b.nzstsouthshk = 1;
	dwc_otg_write_reg32(&_core->device_registers->dcfg, dcfg.d32);

	// Write FIFO sizes
	dwc_otg_write_reg32(&_core->registers->grxfsiz, DWC_OTG_RX_FIFO_SIZE);
	dwc_otg_write_reg32(&_core->registers->gnptxfsiz, (DWC_OTG_TX_FIFO_OFFSET << 16) | DWC_OTG_TX_FIFO_SIZE);

	// Clear Interrupts
	for(i = 0; i < _core->num_eps; i++)
	{
		dwc_otg_write_reg32(&_core->in_ep_registers[i]->diepint, 0xffffffff);
		dwc_otg_write_reg32(&_core->out_ep_registers[i]->doepint, 0xffffffff);
	}

	// We're ready!
	_core->ready = 1;

	// Enable Interrupts
	gintmsk.b.inepintr = 1;
	gintmsk.b.outepintr = 1;
	dwc_otg_write_reg32(&_core->registers->gintmsk, gintmsk.d32);
	dwc_otg_write_reg32(&_core->device_registers->daintmsk, 0);

	dctl.b.pwronprgdone = 1;
	dctl.b.cgoutnak = 1;
	dctl.b.cgnpinnak = 1;
	dwc_otg_write_reg32(&_core->device_registers->dctl, dctl.d32);

	gotgctl.b.sesreq = 1;
	dwc_otg_modify_reg32(&_core->registers->gotgctl, 0, gotgctl.d32);

	return 0;
}

/**
 * dwc_otg_core_stop
 *
 * Stop chip operation.
 */
void dwc_otg_core_stop(dwc_otg_core_t *_core)
{
	DWC_VERBOSE("%s(%p)\n", __func__, _core);
}

/**
 * dwc_otg_core_ep_reset
 *
 * Resets all of the endpoints, cancelling any
 * current requests.
 */
void dwc_otg_core_ep_reset(dwc_otg_core_t *_core)
{
	int i;

	DWC_VERBOSE("%s(%p)\n", __func__, _core);

	// Cancel all current requests.
	dwc_otg_core_cancel_all_requests(_core);

	// Disable EPs.
	for(i = 1; i < _core->num_eps; i++)
	{
		dwc_otg_core_ep_t *ep = &_core->endpoints[i];
		dwc_otg_core_disable_ep(_core, ep);
	}
}

/**
 * dwc_otg_core_enable_interrupts
 *
 * Enables interrupts.
 */
int dwc_otg_core_enable_interrupts(dwc_otg_core_t *_core)
{
	gahbcfg_data_t gahbcfg = { .d32 = 0 };

	DWC_VERBOSE("%s(%p)\n", __func__, _core);

	gahbcfg.b.glblintrmsk = 1;
	dwc_otg_modify_reg32(&_core->registers->gahbcfg, 0, gahbcfg.d32);

	return 0;
}

/**
 * dwc_otg_core_disable_interrupts
 *
 * Disables interrupts.
 */
int dwc_otg_core_disable_interrupts(dwc_otg_core_t *_core)
{
	gahbcfg_data_t gahbcfg = { .d32 = 0 };

	DWC_VERBOSE("%s(%p)\n", __func__, _core);

	gahbcfg.b.glblintrmsk = 1;
	dwc_otg_modify_reg32(&_core->registers->gahbcfg, gahbcfg.d32, 0);

	return 0;
}

/**
 * The core USB IRQ handler.
 */
irqreturn_t dwc_otg_core_irq(int _irq, void *_dev)
{
	dwc_otg_core_t *core = (dwc_otg_core_t*)_dev;
	gintsts_data_t gintsts = { .d32 = 0 };
	gintsts_data_t gintclr = { .d32 = 0 };

	gintsts.d32 = dwc_otg_read_reg32(&core->registers->gintsts) & dwc_otg_read_reg32(&core->registers->gintmsk);

	DWC_VERBOSE("%s(%d, %p) gintsts=0x%08x\n", __func__, _irq, _dev, gintsts.d32);

	// Clear the interrupts
	gintsts.d32 &= ~gintclr.d32;
	dwc_otg_write_reg32(&core->registers->gintsts, gintclr.d32);
	return gintsts.d32 != 0 ? IRQ_NONE : IRQ_HANDLED;
}

/**
 * Add an EP to the shared FIFO ep loop.
 */
static int dwc_otg_core_add_ep_to_loop(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep)
{
	depctl_data_t depctl;

	if(_core->ep_queue_first == NULL)
	{
		// The queue is empty, add us.
		_core->ep_queue_first = _ep;
		_core->ep_queue_last = _ep;

		DWC_DEBUG("Creating EP loop starting with %s.\n", _ep->name);

		// Set our next ep to ourself, creating a loop.
		depctl.d32 = dwc_otg_read_reg32(&_ep->in_registers->diepctl);
		depctl.b.nextep = _ep->num;
		dwc_otg_write_reg32(&_ep->in_registers->diepctl, depctl.d32);
	}
	else
	{
		dwc_otg_core_ep_t *prevEP = _core->ep_queue_last;
		_core->ep_queue_last = _ep;
		
		DWC_DEBUG("Adding %s to chain after %s.\n", _ep->name, prevEP->name);

		// Set our nextep to the start of the queue
		depctl.d32 = dwc_otg_read_reg32(&_ep->in_registers->diepctl);
		depctl.b.nextep = _core->ep_queue_first->num;
		dwc_otg_write_reg32(&_ep->in_registers->diepctl, depctl.d32);

		// Set the previous ep's nextep to us
		depctl.d32 = dwc_otg_read_reg32(&prevEP->in_registers->diepctl);
		depctl.b.nextep = _ep->num;
		dwc_otg_write_reg32(&prevEP->in_registers->diepctl, depctl.d32);
	}

	return 0;
}

/**
 * Enable an endpoint.
 */
int dwc_otg_core_enable_ep(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep, struct usb_endpoint_descriptor *_desc)
{
	daint_data_t daint = { .d32 = 0 };
	depctl_data_t diepctl;
	depctl_data_t doepctl;
	int mps;
	unsigned long flags;

	DWC_VERBOSE("%s(%p, %p, %p)\n", __func__, _core, _ep, _desc);

	if(_desc == NULL)
	{
		DWC_WARNING("%s: cannot enable with NULL descriptor!\n", _ep->name);
		return 0;
	}

	if(_ep->descriptor && _ep->descriptor != _desc)
	{
		DWC_WARNING("%s: %s already enabled!\n", __func__, _ep->name);
		return 0;
	}
	
	spin_lock_irqsave(&_core->lock, flags);

	//INIT_LIST_HEAD(&_ep->transfer_queue);
	_ep->descriptor = _desc;
	mps = dwc_otg_mps_from_speed(_ep->speed);
	if(mps < _desc->wMaxPacketSize)
	{
		DWC_WARNING("%s: Tried to set maxpacket too high! (%d->%d)\n", _ep->name, mps, _desc->wMaxPacketSize);
	}		
	else if(_desc->wMaxPacketSize > 0)
		mps = _desc->wMaxPacketSize;


	if(_ep->num == 0)
	{
		_ep->direction = DWC_OTG_EP_BIDIR;
	}
	else
	{
		if((_desc->bEndpointAddress & USB_DIR_IN) != 0)
			_ep->direction = DWC_OTG_EP_IN;
		else
			_ep->direction = DWC_OTG_EP_OUT;
	}

	daint.d32 = dwc_otg_read_reg32(&_core->device_registers->daintmsk);
	diepctl.d32 = dwc_otg_read_reg32(&_ep->in_registers->diepctl);
	doepctl.d32 = dwc_otg_read_reg32(&_ep->out_registers->doepctl);

	// Setup EP0 max packet size
	if(_ep->num == 0)
	{
		dsts_data_t dsts;

		dsts.d32 = dwc_otg_read_reg32(&_core->device_registers->dsts);

		switch (dsts.b.enumspd)
		{
		case DWC_DSTS_ENUMSPD_HS_PHY_30MHZ_OR_60MHZ:
		case DWC_DSTS_ENUMSPD_FS_PHY_30MHZ_OR_60MHZ:
		case DWC_DSTS_ENUMSPD_FS_PHY_48MHZ:
			diepctl.b.mps = DWC_DEP0CTL_MPS_64;
			doepctl.b.mps = DWC_DEP0CTL_MPS_64;
			break;

		case DWC_DSTS_ENUMSPD_LS_PHY_6MHZ:
			diepctl.b.mps = DWC_DEP0CTL_MPS_8;
			doepctl.b.mps = DWC_DEP0CTL_MPS_8;
			break;
		}

		daint.b.inep0 = 1;
		daint.b.outep0 = 1;

		diepctl.b.usbactep = 1;
		doepctl.b.usbactep = 1;
		
		diepctl.b.snak = 1;
		doepctl.b.snak = 1;

		diepctl.b.stall = 0;
		doepctl.b.stall = 0;

		// if shared fifo mode
		dwc_otg_core_add_ep_to_loop(_core, _ep);
	}
	else
	{
		if(_ep->direction & DWC_OTG_EP_IN)
		{
			dcfg_data_t dcfg = { .d32 = dwc_otg_read_reg32(&_core->device_registers->dcfg) };
			dcfg.b.epmscnt++;

			daint.ep.in |= 1 << _ep->num;

			diepctl.b.usbactep = 1;
			diepctl.b.snak = 1;
			diepctl.b.mps = mps;
			diepctl.b.eptype = (_desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK);
			diepctl.b.nextep = 0; // We're the last EP now, so the next EP must be 0.
			diepctl.b.stall = 0;
			diepctl.b.setd0pid = 1;

			// if shared fifo mode
			dwc_otg_core_add_ep_to_loop(_core, _ep);

			dwc_otg_write_reg32(&_core->device_registers->dcfg, dcfg.d32);
		}

		if(_ep->direction & DWC_OTG_EP_OUT)
		{
			daint.ep.out |= 1 << _ep->num;

			doepctl.b.usbactep = 1;
			doepctl.b.snak = 1;
			doepctl.b.mps = mps;
			doepctl.b.eptype = (_desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK); 
			doepctl.b.stall = 0;
			doepctl.b.setd0pid = 1;
		}
	}

	dwc_otg_write_reg32(&_core->device_registers->daintmsk, daint.d32);
	dwc_otg_write_reg32(&_ep->in_registers->diepctl, diepctl.d32);
	dwc_otg_write_reg32(&_ep->out_registers->doepctl, doepctl.d32);

	spin_unlock_irqrestore(&_core->lock, flags);

	return 0;
}

/**
 * Remove an EP to the shared FIFO ep loop.
 */
static int dwc_otg_core_remove_ep_from_loop(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep)
{
	depctl_data_t diepctl;
	dwc_otg_core_ep_t *prevEP = _core->ep_queue_first;

	if(!prevEP)
	{
		DWC_ERROR("Tried to remove %s from empty queue.\n", _ep->name);
		return -EINVAL;
	}

	DWC_DEBUG("Removing %s from loop.\n", _ep->name);

	diepctl.d32 = dwc_otg_read_reg32(&_ep->in_registers->diepctl);
	
	do
	{
		depctl_data_t pdepctl;
		dwc_otg_core_ep_t *nextEP;

		pdepctl.d32 = dwc_otg_read_reg32(&prevEP->in_registers->diepctl);
		nextEP = &_core->endpoints[pdepctl.b.nextep];
		if(nextEP->num == _ep->num)
			break;

		if(nextEP == _core->ep_queue_first)
		{
			prevEP = NULL;
			break;
		}

		prevEP = nextEP;
	}
	while(prevEP != _core->ep_queue_first);

	DWC_DEBUG("LastEP was %p.\n", prevEP);

	if(_ep == _core->ep_queue_first)
	{
		if(_ep == _core->ep_queue_last)
		{
			// We're the last EP in the queue, destroy it.
			_core->ep_queue_first = NULL;
			_core->ep_queue_last = NULL;

			DWC_DEBUG("Destroyed EP queue, last ep was %s.\n", _ep->name);
		}
		else
		{
			dwc_otg_core_ep_t *nextEP = &_core->endpoints[diepctl.b.nextep];
			_core->ep_queue_first = nextEP;
		}
	}
	else if(_ep == _core->ep_queue_last)
	{
		// The prev EP is the new last
		
		if(prevEP == NULL)
		{
			DWC_ERROR("prevEP is NULL, the loop was invalid when removing %s.\n", _ep->name);
			_core->ep_queue_first = prevEP;
		}

		_core->ep_queue_last = prevEP;		
	}

	return 0;
}

/**
 * Disable an endpoint.
 */
int dwc_otg_core_disable_ep(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep)
{
	unsigned long flags;

	DWC_VERBOSE("%s(%p, %p)\n", __func__, _core, _ep);

	spin_lock_irqsave(&_core->lock, flags);

	dwc_otg_core_cancel_ep(_core, _ep);

	if(_ep->direction & DWC_OTG_EP_IN)
	{
		// if shared fifo mode
		dwc_otg_core_remove_ep_from_loop(_core, _ep);
	}

	while(!list_empty(&_ep->transfer_queue))
	{
		dwc_otg_core_request_t *req = list_first_entry(&_ep->transfer_queue, dwc_otg_core_request_t, queue_pointer);

		DWC_DEBUG("Cancelling request %p, on %s (%p).\n", req, _ep->name, _ep);

		// Cancel the request.
		req->cancelled = 1;
		dwc_otg_core_complete_request(_core, req);
	}

	_ep->direction = DWC_OTG_EP_DISABLED;
	_ep->descriptor = NULL;
	
	spin_unlock_irqrestore(&_core->lock, flags);

	return 0;
}

/**
 * Cancel the current transfer on an endpoint.
 */
int dwc_otg_core_cancel_ep(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep)
{
	daint_data_t daint = { .d32 = 0 };
	int epDir;
	unsigned long flags;
	depctl_data_t diepctl;
	depctl_data_t doepctl;

	DWC_VERBOSE("%s(%p, %p)\n", __func__, _core, _ep);

	spin_lock_irqsave(&_core->lock, flags);

	epDir = DWC_OTG_EP_DIRECTION(dwc_otg_read_reg32(&_core->registers->ghwcfg1), _ep->num);
	DWC_DEBUG("Cancelling %s (0x%x).\n", _ep->name, epDir);
		
	daint.d32 = dwc_otg_read_reg32(&_core->device_registers->daintmsk);
	diepctl.d32 = dwc_otg_read_reg32(&_ep->in_registers->diepctl);
	doepctl.d32 = dwc_otg_read_reg32(&_ep->out_registers->doepctl);

	if(epDir & DWC_OTG_EP_IN && diepctl.b.epena)
	{
		int i, fifo;

		DWC_DEBUG("Cancelling %s-in\n", _ep->name);

		dwc_otg_core_set_global_in_nak(_core);

		daint.ep.in &= ~(1 << _ep->num);

		fifo = diepctl.b.txfnum;
		
		if(!diepctl.b.naksts)
		{
			diepctl.b.stall = 0;
			diepctl.b.snak = 1;
			dwc_otg_write_reg32(&_ep->in_registers->diepctl, diepctl.d32);

			for(i = 0; i < 1000; i++)
			{
				diepint_data_t diepint = { .d32 = dwc_otg_read_reg32(&_ep->in_registers->diepint) };
				if(diepint.b.inepnakeff)
					break;

				udelay(100);
			}

			if(i == 1000)
				DWC_ERROR("%s didn't set NAK in time.\n", _ep->name);
		}

		diepctl.b.epena = 0;
		diepctl.b.epdis = 1;
		dwc_otg_write_reg32(&_ep->in_registers->diepctl, diepctl.d32);

		//wait_for_completion(&_ep->disabled_completion);

		for(i = 0; i < 1000; i++)
		{
			diepint_data_t diepint = { .d32 = dwc_otg_read_reg32(&_ep->in_registers->diepint) };
			if(diepint.b.epdisabled)
				break;
			
			udelay(100);
		}

		if(i == 1000)
			DWC_ERROR("%s didn't become disabled in time.\n", _ep->name);

		diepctl.b.usbactep = 0;
		diepctl.b.epena = 0;
		diepctl.b.epdis = 0;
		diepctl.b.snak = 1;
		dwc_otg_write_reg32(&_ep->in_registers->diepctl, diepctl.d32);

		dwc_otg_core_flush_tx_fifo(_core, fifo);

		dwc_otg_core_clear_global_in_nak(_core);
	}

	if(epDir & DWC_OTG_EP_OUT && doepctl.b.epena)
	{
		dwc_otg_core_set_global_out_nak(_core);

		daint.ep.out &= ~(1 << _ep->num);

		doepctl.b.usbactep = 0;
		doepctl.b.epena = 0;
		doepctl.b.snak = 1;
		doepctl.b.epdis = 1;
		dwc_otg_write_reg32(&_ep->out_registers->doepctl, doepctl.d32);

		dwc_otg_core_clear_global_out_nak(_core);
	}

	dwc_otg_write_reg32(&_core->device_registers->daintmsk, daint.d32);

	spin_unlock_irqrestore(&_core->lock, flags);

	return 0;
}

/**
 * Stall an endpoint.
 */
int dwc_otg_core_stall_ep(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep, int _stall)
{
	depctl_data_t depctl = { .d32 = 0 };
	depctl.b.stall = (_stall > 0) ? 1 : 0;

	DWC_VERBOSE("%s(%p, %p) ep=%s\n", __func__, _core, _ep, _ep->name);

	if(_ep->direction == DWC_OTG_EP_OUT)
	{
		dwc_otg_modify_reg32(&_ep->out_registers->doepctl, 0, depctl.d32);
	}
	else
		dwc_otg_modify_reg32(&_ep->in_registers->diepctl, 0, depctl.d32);

	return 0;
}

/**
 * dwc_otg_core_enqueue_request
 *
 * Queue an out request.
 */
int dwc_otg_core_enqueue_request(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep, dwc_otg_core_request_t *_req)
{
	// If the list is empty, we will need to start the EP up.
	// Store this now, as we're about to modify the list.
	uint32_t startEP = list_empty(&_ep->transfer_queue);

#ifdef VERBOSE
	char *dir = "invalid";
	if(_req->direction == DWC_OTG_REQUEST_IN)
		dir = "in";
	else if(_req->direction == DWC_OTG_REQUEST_OUT)
		dir = "out";

	DWC_DEBUG("%s(%p, %p, %p) ep=%s, dir=%s, len=%d\n", __func__, _core, _ep, _req, _ep->name, dir, _req->length);
#endif

	if(_req->queued == 1)
	{
		DWC_ERROR("Tried to queue a request twice!\n");
		return -EINVAL;
	}

	if(_req->length && !_req->buffer)
	{
		DWC_ERROR("Tried to send a request with no buffer, yet a length.\n");
		return -EINVAL;
	}
	else if(_req->length)
	{
		if(_req->dont_free && !_req->dma_buffer)
		{
			DWC_WARNING("Allocating a DMA buffer for a request that won't be freed.\n");
		}

		if(!_req->dont_free && _req->dma_buffer)
		{
			DWC_WARNING("Pre-allocated DMA buffer in a request that's going to be freed.\n");
		}
	}

	if(_req->length && !_req->dma_buffer)
	{
		// Allocate DMA buffer
		_req->dma_buffer = dma_alloc_coherent(NULL, _req->buffer_length, &_req->dma_address, GFP_KERNEL);
		if(!_req->dma_buffer)
		{
			DWC_ERROR("Failed to allocate a buffer for DMA.\n");
			return -EIO;
		}

		memcpy(_req->dma_buffer, _req->buffer, _req->length);
	}

	// TODO: Do a lock here?

	INIT_LIST_HEAD(&_req->queue_pointer);
	list_add_tail(&_req->queue_pointer, &_ep->transfer_queue);

	_req->core = _core;
	_req->ep = _ep;
	_req->queued = 1;
	_req->amount_done = 0;
	_req->setup = 0;
	_req->cancelled = 0;
	
	if(startEP)
		dwc_otg_core_start_request(_core, _req);

	// TODO: End lock
	
	return 0;
}

/**
 * dwc_otg_core_start_request
 */
int dwc_otg_core_start_request(dwc_otg_core_t *_core, dwc_otg_core_request_t *_req)
{
	dwc_otg_core_ep_t *ep = _req->ep;
	depctl_data_t depctl = { .d32 = 0 };
	daint_data_t daint = { .d32 = 0 };
	volatile uint32_t *depctl_ptr;
	volatile uint32_t *depdma_ptr;
	volatile uint32_t *deptsiz_ptr;
	unsigned long flags;
	int txAmt;

#ifdef VERBOSE
	char *dir = "invalid";
	if(_req->direction == DWC_OTG_REQUEST_IN)
		dir = "in";
	else if(_req->direction == DWC_OTG_REQUEST_OUT)
		dir = "out";

	DWC_DEBUG("%s(%p, %p) type=%d, dma=%p, ep=%s, dir=%s, len=%d, done=%d\n", __func__,
			_core, _req, _req->request_type, (void*)_req->dma_address, ep->name, dir, _req->length, _req->amount_done);
#endif

	// TODO: Does this need to be locked by the
	// core or the EP?
	spin_lock_irqsave(&_core->lock, flags);

	// Reset Flags
	_req->active = 1;

	if(_req->direction == DWC_OTG_REQUEST_OUT)
	{
		daint.ep.out = 1 << ep->num;
		depctl_ptr = &ep->out_registers->doepctl;
		depdma_ptr = &ep->out_registers->doepdma;
		deptsiz_ptr = &ep->out_registers->doeptsiz;
	}
	else
	{
		daint.ep.in = 1 << ep->num;
		depctl_ptr = &ep->in_registers->diepctl;
		depdma_ptr = &ep->in_registers->diepdma;
		deptsiz_ptr = &ep->in_registers->dieptsiz;
	}

	// Enable interrupts on this EP!
	dwc_otg_modify_reg32(&_core->device_registers->daintmsk, 0, daint.d32);

	// Read initial control register value.
	depctl.d32 = dwc_otg_read_reg32(depctl_ptr);

	depctl.b.epena = 1;
	depctl.b.cnak = 1;
	depctl.b.usbactep = 1;

	// Set the DMA address of the data!
	dwc_otg_write_reg32(depdma_ptr, _req->dma_address+_req->amount_done);

	// Calculate packet size, number of packets,
	// and set the max packet size in the control
	// register.
	txAmt = _req->length-_req->amount_done;

	if(ep->num == 0)
	{
		deptsiz0_data_t deptsiz0;
		deptsiz0.d32 = dwc_otg_read_reg32(deptsiz_ptr);

		deptsiz0.b.pktcnt = 1;
		//if(_req->zero)
		//	deptsiz0.b.pktcnt++;

		if(_req->direction == DWC_OTG_REQUEST_OUT)
			deptsiz0.b.supcnt = 1;

		if(txAmt &~ 0x7f)
		{
			DWC_WARNING("Packet too large for EP0 (0x%0x).\n", txAmt);
			txAmt = 0x7f;
		}
		
		deptsiz0.b.xfersize = txAmt;

		DWC_DEBUG("XferSize: pkt=%d, xfersize=%d\n", deptsiz0.b.pktcnt, deptsiz0.b.xfersize);

		dwc_otg_write_reg32(deptsiz_ptr, deptsiz0.d32);
	}
	else
	{
		deptsiz_data_t deptsiz = { .d32 = 0 };

		int mps = dwc_otg_mps_from_speed(ep->speed);
		if(ep->descriptor != NULL)
			mps = ep->descriptor->wMaxPacketSize;

		deptsiz.d32 = dwc_otg_read_reg32(deptsiz_ptr);

		deptsiz.b.xfersize = txAmt;
		deptsiz.b.pktcnt = (txAmt == 0) ? 1 : ((txAmt + mps - 1)/mps);
		if(_req->zero && (txAmt % mps) == 0)
			deptsiz.b.pktcnt++;

		DWC_DEBUG("XferSize: pkt=%d, xfersize=%d\n", deptsiz.b.pktcnt, deptsiz.b.xfersize);

		dwc_otg_write_reg32(deptsiz_ptr, deptsiz.d32);
	}

	_req->current_load = txAmt;

	// Set the control register. (This starts the transfer!)
	dwc_otg_write_reg32(depctl_ptr, depctl.d32);

	spin_unlock_irqrestore(&_core->lock, flags);

	return 0;
}

/**
 * dwc_otg_core_complete_request
 */
int dwc_otg_core_complete_request(dwc_otg_core_t *_core, dwc_otg_core_request_t *_req)
{
	dwc_otg_core_ep_t *ep = _req->ep;
	dwc_otg_core_request_t *req = list_first_entry(&ep->transfer_queue, dwc_otg_core_request_t, queue_pointer);
	deptsiz_data_t deptsiz = { .d32 = 0 };
	volatile uint32_t *depctl_ptr;
	volatile uint32_t *deptsiz_ptr;
	unsigned long flags;
		
	DWC_VERBOSE("%s(%p, %p)\n", __func__, _core, _req);

	if(req != _req)
	{
		DWC_ERROR("Tried to complete request out of phase!\n");
		return -EIO;
	}

	// Setup register pointers
	if(_req->direction == DWC_OTG_REQUEST_OUT)
	{
		depctl_ptr = &ep->out_registers->doepctl;
		deptsiz_ptr = &ep->out_registers->doeptsiz;
	}
	else
	{
		depctl_ptr = &ep->in_registers->diepctl;
		deptsiz_ptr = &ep->in_registers->dieptsiz;
	}
	
	// TODO: Does this need to be locked by the core
	// or the EP?
	spin_lock_irqsave(&_core->lock, flags);

	// Mark the request as inactive
	_req->active = 0;

	// Calculate how much was actually sent
	deptsiz.d32 = dwc_otg_read_reg32(deptsiz_ptr);
	_req->current_load -= deptsiz.b.xfersize;
	_req->amount_done += _req->current_load;

	if(_req->amount_done < 0)
		DWC_ERROR("STRANGE THINGS (%d, %d)!\n", _req->amount_done, deptsiz.b.xfersize);
	else
		_req->length = _req->amount_done;

	// Remove this request as it is now complete.
	list_del(&_req->queue_pointer);
	_req->queued = 0;

	// If there are more requests to process on this EP...
	if(!list_empty(&ep->transfer_queue) && _core->ready)
	{
		// Start the next one!
		req = list_first_entry(&ep->transfer_queue, dwc_otg_core_request_t, queue_pointer);
		dwc_otg_core_start_request(_core, req);
	}
	else
		ep->active = 0;

	spin_unlock_irqrestore(&_core->lock, flags);

	// Call the handler
	DWC_VERBOSE("%s: calling handler. cancel=%p, compl=%p\n", __func__, _req->cancelled_handler, req->completed_handler);
	if(_req->cancelled && _req->cancelled_handler)
		_req->cancelled_handler(_req);
	else if(_req->completed_handler)
		_req->completed_handler(_req);
	
	if(!_req->dont_free)
	{
		if(_req->dma_buffer)
			dma_free_coherent(NULL, _req->buffer_length, _req->dma_buffer, _req->dma_address);

		if(_req->dma_buffer != _req->buffer)
			kfree(_req->buffer);

		kfree(_req);
		_req = NULL;
	}

	return 0;
}

/**
 * Cancel all current requests.
 */
int dwc_otg_core_cancel_all_requests(dwc_otg_core_t *_core)
{
	uint32_t i;

	DWC_VERBOSE("%s(%p)\n", __func__, _core);

	_core->ready = 0;

	for(i = 0; i < _core->num_eps; i++)
	{
		dwc_otg_core_ep_t *ep = &_core->endpoints[i];
		depctl_data_t depctl = { .d32 = 0 };

		// Set NAK on all out endpoints
		depctl.b.snak = 1;
		dwc_otg_modify_reg32(&_core->out_ep_registers[i]->doepctl, 0, depctl.d32);

		DWC_DEBUG("Deleting transfers from %s (%p).\n", ep->name, ep);

 		while(!list_empty(&ep->transfer_queue))
		{
			dwc_otg_core_request_t *req = list_first_entry(&ep->transfer_queue, dwc_otg_core_request_t, queue_pointer);

			DWC_DEBUG("Cancelling request %p, on %s (%p).\n", req, ep->name, ep);

			// Cancel the request.
			req->cancelled = 1;
			dwc_otg_core_complete_request(_core, req);
		}
	}

	_core->ready = 1;

	return 0;
}

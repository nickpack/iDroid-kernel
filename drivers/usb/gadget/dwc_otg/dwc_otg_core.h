/*
 * dwc_otg_core.h - header for core functions for the DWC OTG chip
 *
 * Author: Ricky Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  __DWC_OTG_CORE_H__
#define  __DWC_OTG_CORE_H__

#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/dmaengine.h>
#include <linux/usb/ch9.h>
#include <linux/spinlock.h>
#include <linux/completion.h>

#include "dwc_otg.h"
#include "dwc_otg_regs.h"

/**
 * @file dwc_otg_core.h
 * This file contains the core components of operating the OTG hardware.
 */

/*
 * Constants and configuration.
 */
#define DWC_OTG_DEVICE_OFFSET	0x800
#define DWC_OTG_IN_EP_OFFSET	0x900
#define DWC_OTG_IN_EP_SIZE		0x020
#define DWC_OTG_OUT_EP_OFFSET	0xB00
#define DWC_OTG_OUT_EP_SIZE		0x020
#define DWC_OTG_PCGCCTL_OFFSET	0xE00

#define DWC_OTG_RX_FIFO_SIZE	0x1c0
#define DWC_OTG_TX_FIFO_SIZE	0x1c0
#define DWC_OTG_TX_FIFO_OFFSET	0x200

#define DWC_OTG_EP_MAX_NAME_SIZE 16

#define DWC_OTG_HIGH_SPEED 0
#define DWC_OTG_FULL_SPEED 1
#define DWC_OTG_LOW_SPEED 2

struct dwc_otg_core_struct;
struct dwc_otg_core_ep_struct;
struct dwc_otg_core_request_struct;
typedef void (*request_handler_t)(struct dwc_otg_core_request_struct *);

/**
 * dwc_otg_core_request
 *
 * A USB request.
 */
typedef struct dwc_otg_core_request_struct
{
	/** Our previous and next links in the EP's queue. */
	struct list_head queue_pointer;

	/** The request type: Control, Interrupt, Isoc or Bulk. */
	int request_type;

	/** The request direction. */
	int direction;
#define DWC_OTG_REQUEST_OUT 0
#define DWC_OTG_REQUEST_IN 1

	/** The request buffer. */
	void *buffer;

	/** The buffer length. */
	ssize_t buffer_length;

	/** The amount being transferred. */
	ssize_t current_load;

	/** The amount transferred. */
	ssize_t amount_done;

	/** The request size. */
	ssize_t length;

	/** The DMA buffer. */
	void *dma_buffer;

	/** The DMA address. */
	dma_addr_t dma_address;

	/**
	 * If the request is allocated by dwc_otg_core_enqueue, it
	 * needs to be released.
	 * */
	unsigned dont_free : 1;

	/**
	 * This flag says that the request is currently queued or active.
	 */
	unsigned queued : 1;
	
	/**
	 * If this flag is set, an extra empty packet is sent at the
	 * end of the transfer.
	 */
	unsigned zero : 1;

	/**
	 * This flag says that the request is currently active.
	 */
	unsigned active : 1;

	/**
	 * This is set on out requests if the data was prepended
	 * with a setup packet.
	 */
	unsigned setup : 1;

	/**
	 * This flag is set if the request failed.
	 */
	unsigned cancelled : 1;

	/**
	 * The core handling this request,
	 * filled in when the request is queued.
	 */
	struct dwc_otg_core_struct *core;

	/**
	 * The ep structure of this request,
	 * filled in when the request is queued.
	 */
	struct dwc_otg_core_ep_struct *ep;

	/**
	 * The request handler which is called when the request completed.
	 */
	request_handler_t completed_handler;

	/**
	 * The request handler which is called when the request was cancelled.
	 *
	 * If this isn't set, the completed_handler is called when the
	 * request is cancelled.
	 */
	request_handler_t cancelled_handler;

	/**
	 * This variable is for use entirely by whoever creates
	 * the request.
	 */
	void *data;

} dwc_otg_core_request_t;

/**
 * dwc_otg_core_ep_struct
 *
 * This stores all the information about an endpoint.
 * (If an endpoint is bi-directional, there will
 *  actually be two of these structures.)
 */
typedef struct dwc_otg_core_ep_struct
{
	/** This is set to 1 if this EP exists. */
	int exists;

	/** The index of the EP. */
	int num;

	/** The name of the EP. */
	char name[DWC_OTG_EP_MAX_NAME_SIZE];

	/**
	 * The in EP registers.
	 */
	dwc_otg_dev_in_ep_regs_t *in_registers;

	/**
	 * The out EP registers.
	 */
	dwc_otg_dev_out_ep_regs_t *out_registers;

	/** The current transfer queue. */
	struct list_head transfer_queue;

	/** The current speed of the EP. */
	int speed;

	/**
	 * Whether the EP is active
	 * (essentially a copy of the .epena
	 * register in the hardware, but stored
	 * seperately so that hardware errors do
	 * not cause the drivers to malfunction.)
	 */
	unsigned active : 1;

	/** Which direction the EP is currently working in. */
	unsigned direction : 2;
#define DWC_OTG_EP_DIRECTION(hwcfg1, ep) ((~(hwcfg1)) >> (2*(ep)) & 3)
#define DWC_OTG_EP_DISABLED 0
#define DWC_OTG_EP_IN 2
#define DWC_OTG_EP_OUT 1
#define DWC_OTG_EP_BIDIR 3

	/** The endpoint's spinlock. */
	spinlock_t lock;

	/** This completion is fired when the NAK bit becomes effective. */
	struct completion nakeff_completion;

	/** This completion is fired when the EP is disabled. */
	struct completion disabled_completion;

	/** This is set by the usb_gadget API when the EP is enabled. */
	struct usb_endpoint_descriptor *descriptor;

} dwc_otg_core_ep_t;

/**
 * dwc_otg_core_struct
 *
 * This structure holds all the data required to operate
 * the device.
 */ 
typedef struct dwc_otg_core_struct
{
	/**
	 * This is the IRQ of the core.
	 */
	int irq;

	/**
	 * This is the memory address of the start of
	 * the remapped registers.
	 */
	volatile dwc_otg_core_global_regs_t *registers;

	/**
	 * This is the memory address of the start of
	 * the remapped PHY registers.
	 *
	 * This variable will be NULL if there is no
	 * on-core PHY.
	 */
	volatile dwc_otg_phy_regs_t *phy_registers;

	/**
	 * This is the clock speed and gating register,
	 * it's used to restart the PHY.
	 */
	volatile uint32_t *pcgcctl;

	/**
	 * This is the memory address of the start of the
	 * device registers.
	 */
	volatile dwc_otg_device_global_regs_t *device_registers;

	/**
	 * The in EP registers.
	 */
	dwc_otg_dev_in_ep_regs_t *in_ep_registers[MAX_EPS_CHANNELS];

	/**
	 * The out EP registers.
	 */
	dwc_otg_dev_out_ep_regs_t *out_ep_registers[MAX_EPS_CHANNELS];

	/**
	 * The endpoints.
	 */
	int num_eps;
	dwc_otg_core_ep_t endpoints[MAX_EPS_CHANNELS];

	/**
	 * The start of the EP queue used in Shared FIFO mode.
	 */
	dwc_otg_core_ep_t *ep_queue_first;

	/**
	 * The end of the EP queue used in Shared FIFO mode.
	 */
	dwc_otg_core_ep_t *ep_queue_last;

	/**
	 * These registers are read once and then modified
	 * to disable unwanted features.
	 */
	hwcfg1_data_t hwcfg1;
	hwcfg2_data_t hwcfg2;
	hwcfg3_data_t hwcfg3;
	hwcfg4_data_t hwcfg4;

	/**
	 * Determines whether the core is ready for
	 * transfers. (EG, it isn't whilst we're
	 * shutting down the core after we receive a
	 * USB reset.)
	 */
	unsigned ready : 1;

	/**
	 * If > 0, the global out nak bit is set.
	 */
	uint32_t global_out_nak_count;

	/**
	 * If > 0, the global non-periodic in bit is set.
	 */
	uint32_t global_in_nak_count;

	/** The core spinlock, for protecting registers. */
	spinlock_t lock;

} dwc_otg_core_t;

/**
 * dwc_otg_core_init
 *
 * This function initialises the hardware and the
 * structure specified by _core.
 */
int dwc_otg_core_init(dwc_otg_core_t *_core, int _irq, void *_regs, void *_phy);

/**
 * dwc_otg_core_destroy
 *
 * This shuts down the hardware and releases any resources
 * obtained whilst the driver was active.
 *
 * This function will not free _core.
 */
void dwc_otg_core_destroy(dwc_otg_core_t *_core);

/**
 * dwc_otg_core_soft_reset
 *
 * Soft Resets the core.
 */
int dwc_otg_core_soft_reset(dwc_otg_core_t *_core);

/**
 * Set the global non-periodic in nak register, if it's not
 * already set. It will be cleared after the same amount of 
 * clear calls have been made as set calls.
 */
int dwc_otg_core_set_global_in_nak(dwc_otg_core_t *_core);

/**
 * Clear the non-periodic global out nak register.
 */
int dwc_otg_core_clear_global_in_nak(dwc_otg_core_t *_core);

/**
 * Set the global out nak register.
 */
int dwc_otg_core_set_global_out_nak(dwc_otg_core_t *_core);

/**
 * Clear the global out nak register.
 */
int dwc_otg_core_clear_global_out_nak(dwc_otg_core_t *_core);

/**
 * Flush one of the core's TX fifos.
 */
int dwc_otg_core_flush_tx_fifo(dwc_otg_core_t *_core, int _fnum);

/**
 * dwc_otg_core_start
 *
 * Start chip operation.
 */
int dwc_otg_core_start(dwc_otg_core_t *_core);

/**
 * dwc_otg_core_stop
 *
 * Stop chip operation.
 */
void dwc_otg_core_stop(dwc_otg_core_t *_core);

/**
 * dwc_otg_core_ep_reset
 *
 * Resets all of the endpoints, cancelling any
 * current requests.
 */
void dwc_otg_core_ep_reset(dwc_otg_core_t *_core);

/**
 * dwc_otg_core_enable_interrupts
 *
 * Enables interrupts.
 */
int dwc_otg_core_enable_interrupts(dwc_otg_core_t *_core);

/**
 * dwc_otg_core_disable_interrupts
 *
 * Disables interrupts.
 */
int dwc_otg_core_disable_interrupts(dwc_otg_core_t *_core);

/**
 * The core USB IRQ handler.
 */
irqreturn_t dwc_otg_core_irq(int _irq, void *_dev);

/**
 * Enable an endpoint.
 */
int dwc_otg_core_enable_ep(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep, struct usb_endpoint_descriptor *_desc);

/**
 * Disable an endpoint.
 */
int dwc_otg_core_disable_ep(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep);

/**
 * Cancel the current request on the endpoint, and reset it to a known state.
 */
int dwc_otg_core_cancel_ep(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep);

/**
 * Stall an endpoint.
 */
int dwc_otg_core_stall_ep(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep, int _stall);

/**
 * dwc_otg_core_enqueue_request
 *
 * Queue a request.
 */
int dwc_otg_core_enqueue_request(dwc_otg_core_t *_core, dwc_otg_core_ep_t *_ep, dwc_otg_core_request_t *_req);

/**
 * dwc_otg_core_start_request
 */
int dwc_otg_core_start_request(dwc_otg_core_t *_core, dwc_otg_core_request_t *_req);

/**
 * dwc_otg_core_complete_request
 */
int dwc_otg_core_complete_request(dwc_otg_core_t *_core, dwc_otg_core_request_t *_req);

/**
 * Cancel all current requests.
 */
int dwc_otg_core_cancel_all_requests(dwc_otg_core_t *_core);

/**
 * dwc_otg_mps_from_speed
 */
static inline int dwc_otg_mps_from_speed(int _speed)
{
	if(_speed == DWC_OTG_HIGH_SPEED)
		return 512;

	if(_speed == DWC_OTG_FULL_SPEED)
		return 64;

	if(_speed == DWC_OTG_LOW_SPEED)
		return 32;

	DWC_WARNING("%s called with invalid speed, %d.\n", __func__, _speed);
	return -1;
}

#endif //__DWC_OTG_CORE_H__

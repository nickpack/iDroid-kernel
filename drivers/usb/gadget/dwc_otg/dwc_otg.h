/*
 * dwc_otg.h - root header for the DWC OTG driver
 *
 * Author: Ricky Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef  __DWC_OTG_H__
#define  __DWC_OTG_H__

#define		DWC_OTG_DRIVER_NAME "dwc_otg"
#define		DWC_OTG_TAG DWC_OTG_DRIVER_NAME ": "

// Logging Macros -- Ricky26
#define		DWC_PRINT(args...) printk(DWC_OTG_TAG args)

// TODO: Tell the kernel this is actually an error? -- Ricky26
#define		DWC_ERROR(args...) DWC_PRINT(args) 

#ifdef DEBUG
#	define	DWC_DEBUG(args...) DWC_PRINT(args)
#else
#	define	DWC_DEBUG(args...)
#endif

#if defined(DEBUG)&&defined(VERBOSE)
#	define DWC_VERBOSE(args...) DWC_PRINT(args)
#else
#	define DWC_VERBOSE(args...)
#endif

// TODO: Use the kernel stuff to make this a real warning?
//       Then remove the (DEBUG) if.
#if defined(DEBUG)
#	define DWC_WARNING(args...) DWC_PRINT(args)
#else
#   define DWC_WARNING(args...)
#endif

#endif //__DWC_OTG_H__

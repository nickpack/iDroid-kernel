#
# Makefile for DWC OTG Highspeed USB controller driver
#

KBUILD_CPPFLAGS	+= -Dlinux
KBUILD_CPPFLAGS += -DDEBUG
#KBUILD_CPPFLAGS += -DVERBOSE

obj-$(CONFIG_USB_DWC_OTG) += dwc_otg.o

dwc_otg-objs	:= dwc_otg_driver.o
dwc_otg-objs	+= dwc_otg_hw.o
dwc_otg-objs	+= dwc_otg_core.o
dwc_otg-objs	+= dwc_otg_device.o
dwc_otg-objs	+= dwc_otg_gadget.o


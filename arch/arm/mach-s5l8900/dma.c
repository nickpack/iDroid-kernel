#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include <mach/hardware.h>
#include <mach/iphone-dma.h>
#include <mach/dma.h>
#include <mach/iphone-clock.h>

#define GET_BITS(x, start, length) ((((u32)(x)) << (32 - ((start) + (length)))) >> (32 - (length)))

static const int ControllerLookupTable[] = {
	1, 1, 1, 1, 1,
	1, 1, 1, 1, 1,
	2, 2, 2, 2, 2,
	2, 2, 2, 2, 3,
	3, 3, 3, 3, 3,
	3
};

static const u32 AddressLookupTable[] = {
	0x3CC00024, 0x3CC00020, 0x3CC08024, 0x3CC08020, 0x3CC0C024,
	0x3CC0C020, 0x3CC10024, 0x3CC10020, 0x38A00080, 0x38300040,
	0x3CE00020, 0x3CE00010, 0x3D200020, 0x3D200010, 0x3CD00038,
	0x3CD00010, 0x3D400038, 0x3D400010, 0x3CB00010, 0x3CA00038,
	0x3CA00010, 0x3C300020, 0x3C300010, 0x3CC04024, 0x3CC04020,
	0
};

static const u32 PeripheralLookupTable[][IPHONE_DMA_NUMCONTROLLERS] = {
	{0x07, 0x10}, {0x06, 0x10}, {0x0B, 0x10}, {0x0A, 0x10}, {0x0D, 0x10},
	{0x0C, 0x10}, {0x0F, 0x10}, {0x0E, 0x10}, {0x02, 0x10}, {0x03, 0x10},
	{0x10, 0x0D}, {0x10, 0x0C}, {0x10, 0x0F}, {0x10, 0x0E}, {0x10, 0x03},
	{0x10, 0x02}, {0x10, 0x05}, {0x10, 0x04}, {0x10, 0x06}, {0x01, 0x01},
	{0x00, 0x00}, {0x05, 0x0B}, {0x04, 0x0A}, {0x09, 0x09}, {0x08, 0x08},
	{0x00, 0x00}
};

static volatile DMARequest requests[IPHONE_DMA_NUMCONTROLLERS][IPHONE_DMA_NUMCHANNELS];
static DMALinkedList* DMALists[IPHONE_DMA_NUMCONTROLLERS][IPHONE_DMA_NUMCHANNELS];
static dma_addr_t DMAListsPhys[IPHONE_DMA_NUMCONTROLLERS][IPHONE_DMA_NUMCHANNELS];
static size_t DMAListsSize[IPHONE_DMA_NUMCONTROLLERS][IPHONE_DMA_NUMCHANNELS];

static void dispatchRequest(volatile DMARequest *request);

static irqreturn_t dmaIRQHandler(int irq, void* pController);

static volatile int Controller0FreeChannels[IPHONE_DMA_NUMCHANNELS] = {0};
static volatile int Controller1FreeChannels[IPHONE_DMA_NUMCHANNELS] = {0};
static spinlock_t freeChannelsLock;

static struct device* dma_dev;

int iphone_dma_setup(void) {
	int ret;

	iphone_clock_gate_switch(DMAC0_CLOCKGATE, 1);
	iphone_clock_gate_switch(DMAC1_CLOCKGATE, 1);

	memset(requests, 0, sizeof(requests));

        ret = request_irq(DMAC0_INTERRUPT, dmaIRQHandler, IRQF_DISABLED, "iphone_dma", (void*) 1);
        ret = request_irq(DMAC1_INTERRUPT, dmaIRQHandler, IRQF_DISABLED, "iphone_dma", (void*) 2);

	spin_lock_init(&freeChannelsLock);

	return ret;
}

static irqreturn_t dmaIRQHandler(int irq, void* pController) {
	u32 intTCStatusReg;
	u32 intTCClearReg;
	u32 intTCStatus;
	int channel;
	u32 controller = (u32)pController;

	if(controller == 1) {
		intTCStatusReg = DMAC0 + DMACIntTCStatus;
		intTCClearReg = DMAC0 + DMACIntTCClear;
	} else {
		intTCStatusReg = DMAC1 + DMACIntTCStatus;
		intTCClearReg = DMAC1 + DMACIntTCClear;
	}

	intTCStatus = __raw_readl(intTCStatusReg);

	for(channel = 0; channel < IPHONE_DMA_NUMCHANNELS; channel++) {
		if((intTCStatus & (1 << channel)) != 0) {
			dispatchRequest(&requests[controller - 1][channel]);
			__raw_writel(1 << channel, intTCClearReg);
		}
	}

	return IRQ_HANDLED;
}

static int getFreeChannel(int* controller, int* channel) {
	int i;
	unsigned long flags;

	spin_lock_irqsave(&freeChannelsLock, flags);
	if((*controller & (1 << 0)) != 0) {
		for(i = 0; i < IPHONE_DMA_NUMCHANNELS; i++) {
			if(Controller0FreeChannels[i] == 0) {
				*controller = 1;
				*channel = i;
				Controller0FreeChannels[i] = 1;
				spin_unlock_irqrestore(&freeChannelsLock, flags);
				return 0;
			}
		}
	}

	if((*controller & (1 << 1)) != 0) {
		for(i = 0; i < IPHONE_DMA_NUMCHANNELS; i++) {
			if(Controller1FreeChannels[i] == 0) {
				*controller = 2;
				*channel = i;
				Controller1FreeChannels[i] = 1;
				spin_unlock_irqrestore(&freeChannelsLock, flags);
				return 0;
			}
		}
	}
	spin_unlock_irqrestore(&freeChannelsLock, flags);

	return ERROR_BUSY;
}

void iphone_dma_set_done_handler(int* controller, int* channel, void (*done_handler)(int controller, int channel, void* token), void* token)
{
	requests[*controller - 1][*channel].token = token;
	requests[*controller - 1][*channel].done_handler = done_handler;
}

int iphone_dma_request(int Source, int SourceTransferWidth, int SourceBurstSize, int Destination, int DestinationTransferWidth, int DestinationBurstSize, int* controller, int* channel) {
	u32 DMACControl;
	u32 DMACConfig;
	u32 config;
	int x;

	if(*controller == 0) {
		*controller = ControllerLookupTable[Source] & ControllerLookupTable[Destination];

		if(*controller == 0) {
			return ERROR_DMA;
		}

		while(getFreeChannel(controller, channel) == ERROR_BUSY);
		requests[*controller - 1][*channel].started = true;
		requests[*controller - 1][*channel].done = false;
		requests[*controller - 1][*channel].done_handler = NULL;
		requests[*controller - 1][*channel].controller = *controller;
		requests[*controller - 1][*channel].channel = *channel;
	}

	if(*controller == 1) {
		DMACControl = DMAC0 + DMAC0Control0 + (*channel * DMAChannelRegSize);
		DMACConfig = DMAC0 + DMACConfiguration;
	} else if(*controller == 2) {
		DMACControl = DMAC1 + DMAC0Control0 + (*channel * DMAChannelRegSize);
		DMACConfig = DMAC1 + DMACConfiguration;
	} else {
		return ERROR_DMA;
	}

	config = 0;

	__raw_writel(DMACConfiguration_ENABLE, DMACConfig);

	config |= (SourceTransferWidth / 2) << DMAC0Control0_SWIDTHSHIFT; // 4 -> 2, 2 -> 1, 1 -> 0
	config |= (DestinationTransferWidth / 2) << DMAC0Control0_DWIDTHSHIFT; // 4 -> 2, 2 -> 1, 1 -> 0

	for(x = 0; x < 7; x++) {
		if((1 << (x + 1)) >= SourceBurstSize) {
			config |= x << DMAC0Control0_SBSIZESHIFT;
			break;
		}
	}
	for(x = 0; x < 7; x++) {
		if((1 << (x + 1)) >= DestinationBurstSize) {
			config |= x << DMAC0Control0_DBSIZESHIFT;
			break;
		}
	}

	config |= 1 << DMAC0Control0_TERMINALCOUNTINTERRUPTENABLE;
	config |= 1 << DMAC0Control0_SOURCEAHBMASTERSELECT;

	__raw_writel(config, DMACControl);

	return 0;
}

dma_addr_t iphone_dma_dstpos(int controller, int channel) {
	u32 regOffset = channel * DMAChannelRegSize;

	if(controller == 1) {
		regOffset += DMAC0;
	} else if(controller == 2) {
		regOffset += DMAC1;
	}

	return __raw_readl(regOffset + DMAC0DestAddress);
}

dma_addr_t iphone_dma_srcpos(int controller, int channel) {
	u32 regOffset = channel * DMAChannelRegSize;

	if(controller == 1) {
		regOffset += DMAC0;
	} else if(controller == 2) {
		regOffset += DMAC1;
	}

	return __raw_readl(regOffset + DMAC0SrcAddress);
}

size_t iphone_dma_continue_list_size(dma_addr_t Source, dma_addr_t Destination, int size, int* controller, int* channel)
{
	int transfers;
	int segments;
	u32 regControl0;
	u32 regOffset = (*channel * DMAChannelRegSize);

	if(*controller == 1)
		regOffset += DMAC0;
	else if(*controller == 2)
		regOffset += DMAC1;

	regControl0 = regOffset + DMAC0Control0;

	transfers = size / (1 << DMAC0Control0_DWIDTH(__raw_readl(regControl0)));
	segments = (transfers + (0xE00 - 1))/0xE00;

	return segments * sizeof(DMALinkedList);
}

void iphone_dma_create_continue_list(dma_addr_t Source, dma_addr_t Destination, int size, int* controller, int* channel,
		DMALinkedList** continueList, dma_addr_t* phys, size_t* list_size, DMALinkedList** last)
{
	int transfers;
	int segments;
	u32 src;
	u32 dest;
	DMALinkedList* list;
	DMALinkedList* item;
	dma_addr_t listPhys;
	dma_addr_t itemPhys;
	u32 control;
	u32 width_shift;
	u32 regControl0;
	u32 sourceIncrement = 0;
	u32 destinationIncrement = 0;
	u32 regOffset = (*channel * DMAChannelRegSize);

	if(*controller == 1)
		regOffset += DMAC0;
	else if(*controller == 2)
		regOffset += DMAC1;

	regControl0 = regOffset + DMAC0Control0;

	control = __raw_readl(regControl0) & ~(DMAC0Control0_SIZEMASK | DMAC0Control0_SOURCEINCREMENT | DMAC0Control0_DESTINATIONINCREMENT | (1 << DMAC0Control0_TERMINALCOUNTINTERRUPTENABLE));
	width_shift = DMAC0Control0_DWIDTH(__raw_readl(regControl0));
	transfers = size / (1 << width_shift);

	segments = (transfers + (0xE00 - 1))/0xE00;

	if(*list_size >= (segments * sizeof(DMALinkedList)))
	{
		list = *continueList;
		listPhys = *phys;
	} else
	{
		*list_size = segments * sizeof(DMALinkedList);
		list = dma_alloc_writecombine(dma_dev, *list_size, &listPhys, GFP_KERNEL);
		*continueList = list;
	}

	if(Source <= (sizeof(AddressLookupTable)/sizeof(u32))) {
		if(Destination <= (sizeof(AddressLookupTable)/sizeof(u32))) {
			src = AddressLookupTable[Source];
			dest = AddressLookupTable[Destination];
		} else {
			src = AddressLookupTable[Source];
			dest = Destination;
			destinationIncrement = 1 << DMAC0Control0_DESTINATIONINCREMENT;
		}
	} else {
		if(Destination <= (sizeof(AddressLookupTable)/sizeof(u32))) {
			src = Source;
			dest = AddressLookupTable[Destination];
			sourceIncrement = 1 << DMAC0Control0_SOURCEINCREMENT;
		} else {
			src = Source;
			dest = Destination;
			sourceIncrement = 1 << DMAC0Control0_SOURCEINCREMENT;
			destinationIncrement = 1 << DMAC0Control0_DESTINATIONINCREMENT;
		}
	}

	item = list;
	itemPhys = listPhys;

	while(transfers > 0)
	{
		unsigned int curTransfer = (transfers > 0xE00) ? 0xE00 : transfers;

		item->control = destinationIncrement | sourceIncrement | control | (curTransfer & DMAC0Control0_SIZEMASK);
		item->source = src;
		item->destination = dest;
		item->next = itemPhys + sizeof(DMALinkedList);

		if(sourceIncrement != 0)
			src += 0xE00 << width_shift;

		if(destinationIncrement != 0)
			dest += 0xE00 << width_shift;

		transfers -= curTransfer;
		itemPhys = item->next;
		++item;
	}

	--item;
	item->control |= 1 << DMAC0Control0_TERMINALCOUNTINTERRUPTENABLE;
	item->next = 0;

	if(last)
		*last = item;

	*phys = listPhys;
	return;
}

int iphone_dma_prepare(dma_addr_t Source, dma_addr_t Destination, int size, DMALinkedList* continueList, int* controller, int* channel)
{
	u32 regSrcAddress;
	u32 regDestAddress;
	u32 regLLI;
	u32 regControl0;
	u32 regConfiguration;
	DMALinkedList* list = NULL;
	dma_addr_t listPhys = 0;
	size_t listSize = 0;
	u32 sourcePeripheral = 0;
	u32 destPeripheral = 0;
	u32 flowControl = 0;
	u32 regOffset = (*channel * DMAChannelRegSize);

	if(*controller == 1)
		regOffset += DMAC0;
	else if(*controller == 2)
		regOffset += DMAC1;

	regSrcAddress = regOffset + DMAC0SrcAddress;
	regDestAddress = regOffset + DMAC0DestAddress;
	regLLI = regOffset + DMAC0LLI;
	regControl0 = regOffset + DMAC0Control0;
	regConfiguration = regOffset + DMAC0Configuration;

	if(DMALists[*controller - 1][*channel])
	{
		dma_free_writecombine(dma_dev, DMAListsSize[*controller -  1][*channel], DMALists[*controller - 1][*channel], DMAListsPhys[*controller - 1][*channel]);
		DMALists[*controller - 1][*channel] = NULL;
	}

	if(!continueList)
	{
		iphone_dma_create_continue_list(Source, Destination, size, controller, channel, &list, &listPhys, &listSize, NULL);

		DMALists[*controller - 1][*channel] = list;
		DMAListsPhys[*controller - 1][*channel] = listPhys;
		DMAListsSize[*controller - 1][*channel] = listSize;

		continueList = list;
	}

	if(Source <= (sizeof(AddressLookupTable)/sizeof(u32))) {
		if(Destination <= (sizeof(AddressLookupTable)/sizeof(u32))) {
			sourcePeripheral = PeripheralLookupTable[Source][*controller - 1];
			destPeripheral = PeripheralLookupTable[Destination][*controller - 1];
			flowControl =  DMAC0Configuration_FLOWCNTRL_P2P;
		} else {
			sourcePeripheral = PeripheralLookupTable[Source][*controller - 1];
			destPeripheral = PeripheralLookupTable[IPHONE_DMA_MEMORY][*controller - 1];
			flowControl =  DMAC0Configuration_FLOWCNTRL_P2M;
		}
	} else {
		if(Destination <= (sizeof(AddressLookupTable)/sizeof(u32))) {
			sourcePeripheral = PeripheralLookupTable[IPHONE_DMA_MEMORY][*controller - 1];
			destPeripheral = PeripheralLookupTable[Destination][*controller - 1];
			flowControl =  DMAC0Configuration_FLOWCNTRL_M2P;
		} else {
			sourcePeripheral = PeripheralLookupTable[IPHONE_DMA_MEMORY][*controller - 1];
			destPeripheral = PeripheralLookupTable[IPHONE_DMA_MEMORY][*controller - 1];
			flowControl =  DMAC0Configuration_FLOWCNTRL_M2M;
		}
	}

	__raw_writel(DMAC0Configuration_TERMINALCOUNTINTERRUPTMASK
			| (flowControl << DMAC0Configuration_FLOWCNTRLSHIFT)
			| (destPeripheral << DMAC0Configuration_DESTPERIPHERALSHIFT)
			| (sourcePeripheral << DMAC0Configuration_SRCPERIPHERALSHIFT), regConfiguration);

	__raw_writel(continueList->control, regControl0);
	__raw_writel(continueList->source, regSrcAddress);
	__raw_writel(continueList->destination, regDestAddress);
	__raw_writel(continueList->next, regLLI);

	return 0;
}

int iphone_dma_perform(dma_addr_t Source, dma_addr_t Destination, int size, DMALinkedList* continueList, int* controller, int* channel) {
	int ret;

	ret = iphone_dma_prepare(Source, Destination, size, continueList, controller, channel);

	if(ret != 0)
		return ret;

	iphone_dma_resume(*controller, *channel);
	return 0;
}

void iphone_dma_pause(int controller, int channel) {
	u32 regOffset = channel * DMAChannelRegSize;

	if(controller == 1) {
		regOffset += DMAC0;
	} else if(controller == 2) {
		regOffset += DMAC1;
	}

	__raw_writel(__raw_readl(regOffset + DMAC0Configuration) & ~DMAC0Configuration_CHANNELENABLED,
			regOffset + DMAC0Configuration);
}

void iphone_dma_resume(int controller, int channel) {
	u32 regOffset = channel * DMAChannelRegSize;

	if(controller == 1) {
		regOffset += DMAC0;
	} else if(controller == 2) {
		regOffset += DMAC1;
	}

	__raw_writel(__raw_readl(regOffset + DMAC0Configuration) | DMAC0Configuration_CHANNELENABLED,
			regOffset + DMAC0Configuration);
}

int iphone_dma_finish(int controller, int channel, int timeout) {
	unsigned long flags;
	int ret = 0;
	unsigned long timeout_jiffies;

	timeout_jiffies = msecs_to_jiffies(timeout) + 1;
		requests[controller - 1][channel].task = current;

	set_current_state(TASK_INTERRUPTIBLE);
	if(!requests[controller - 1][channel].done)
		timeout_jiffies = schedule_timeout(timeout_jiffies);
	set_current_state(TASK_RUNNING);

	if(!requests[controller - 1][channel].done)
		ret = -1;

	requests[controller - 1][channel].started = false;
	requests[controller - 1][channel].done = false;
	requests[controller - 1][channel].task = NULL;

	spin_lock_irqsave(&freeChannelsLock, flags);
	if(controller == 1)
		Controller0FreeChannels[channel] = 0;
	else if(controller == 2)
		Controller1FreeChannels[channel] = 0;
	spin_unlock_irqrestore(&freeChannelsLock, flags);

	return ret;
}

static void dispatchRequest(volatile DMARequest *request) {
	// TODO: Implement this
	request->done = true;

	if(request->task)
	{
		wake_up_process(request->task);
	}

	if(request->done_handler)
	{
		request->done_handler(request->controller, request->channel, request->token);
}
}

static u64 iphone_dma_dmamask = DMA_BIT_MASK(32);

static int __devinit iphone_dma_probe(struct platform_device *pdev)
{
	dma_dev = &pdev->dev;
	return 0;
}

static int __devexit iphone_dma_remove(struct platform_device *pdev)
{
	return 0;
}

static struct resource iphone_dma_resources[] = {
	[0] = {
		.start  = DMAC0_PA,
		.end    = DMAC0_PA + 0x1000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = DMAC1_PA,
		.end    = DMAC1_PA + 0x1000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	[2] = {
		.start  = DMAC0_INTERRUPT,
		.end    = DMAC1_INTERRUPT,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device iphone_dma = {
	.name           = "iphone-dma",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(iphone_dma_resources),
	.resource       = iphone_dma_resources,
	.dev		= {
		.dma_mask		= &iphone_dma_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32)
	}
};

static struct platform_driver iphone_dma_driver = {
	.driver         = {
		.name   = "iphone-dma",
		.owner  = THIS_MODULE,
	},
	.probe          = iphone_dma_probe,
	.remove         = __devexit_p(iphone_dma_remove),
	.suspend        = NULL,
	.resume         = NULL,
};

static int __init iphone_dma_modinit(void)
{
	memset(DMALists, 0, sizeof(DMALists));
	return platform_driver_register(&iphone_dma_driver);
}

static void __exit iphone_dma_modexit(void)
{
	platform_driver_unregister(&iphone_dma_driver);
}

module_init(iphone_dma_modinit);
module_exit(iphone_dma_modexit);

MODULE_DESCRIPTION("iPhone DMA Controller");
MODULE_LICENSE("GPL");

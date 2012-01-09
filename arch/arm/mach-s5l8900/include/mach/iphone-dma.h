#ifndef IPHONE_DMA_H
#define IPHONE_DMA_H
#include <linux/sched.h>

#define ERROR_DMA 0x13
#define ERROR_BUSY 0x15
#define ERROR_ALIGN 0x9

typedef void request_done(int controller, int channel, void* token);

typedef volatile struct DMARequest {
	bool started;
	bool done;
	struct task_struct* task;
	int controller;
	int channel;
	void (*done_handler)(int controller, int channel, void* token);
	void* token;
	// TODO: fill this thing out
} DMARequest;

typedef struct DMALinkedList {
    u32 source;
    u32 destination;
    u32 next;
    u32 control;
} DMALinkedList;

#define IPHONE_DMA_I2S0_RX 19
#define IPHONE_DMA_I2S0_TX 20
#define IPHONE_DMA_I2S1_RX 14
#define IPHONE_DMA_I2S1_TX 15
#define IPHONE_DMA_MEMORY 25
#define IPHONE_DMA_NAND 8

extern struct platform_device iphone_dma;

int iphone_dma_setup(void);
int iphone_dma_request(int Source, int SourceTransferWidth, int SourceBurstSize, int Destination, int DestinationTransferWidth, int DestinationBurstSize, int* controller, int* channel);
void iphone_dma_set_done_handler(int* controller, int* channel, void (*done_handler)(int controller, int channel, void* token), void* token);
void iphone_dma_create_continue_list(dma_addr_t Source, dma_addr_t Destination, int size, int* controller, int* channel,
		DMALinkedList** continueList, dma_addr_t* phys, size_t* list_size, DMALinkedList** last);
size_t iphone_dma_continue_list_size(dma_addr_t Source, dma_addr_t Destination, int size, int* controller, int* channel);
int iphone_dma_prepare(dma_addr_t Source, dma_addr_t Destination, int size, DMALinkedList* continueList, int* controller, int* channel);
int iphone_dma_perform(dma_addr_t Source, dma_addr_t Destination, int size, DMALinkedList* continueList, int* controller, int* channel);
int iphone_dma_finish(int controller, int channel, int timeout);
dma_addr_t iphone_dma_dstpos(int controller, int channel);
dma_addr_t iphone_dma_srcpos(int controller, int channel);
void iphone_dma_resume(int controller, int channel);
void iphone_dma_pause(int controller, int channel);

#endif


#ifndef DMA_H
#define DMA_H

#define ERROR_DMA 0x13
#define ERROR_BUSY 0x15
#define ERROR_ALIGN 0x9

typedef struct DMARequest {
	int started;
	int done;
	// TODO: fill this thing out
} DMARequest;

typedef struct DMALinkedList {
    uint32_t source;	
    uint32_t destination;
    struct DMALinkedList* next;
    uint32_t control;
} DMALinkedList;

#define DMA_MEMORY 25
#define DMA_NAND 8

int dma_setup();
int dma_request(int Source, int SourceTransferWidth, int SourceBurstSize, int Destination, int DestinationTransferWidth, int DestinationBurstSize, int* controller, int* channel);
int dma_perform(uint32_t Source, uint32_t Destination, int size, int continueList, int* controller, int* channel);
int dma_finish(int controller, int channel, int timeout);

#endif


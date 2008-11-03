#ifndef DMA_H
#define DMA_H

#define ERROR_DMA 0x13
#define ERROR_BUSY 0x15

typedef struct DMARequest {
	uint32_t field_0;
	uint32_t field_4;
	void* field_8;
	void* field_C;
	// TODO: fill this thing out
} DMARequest;

typedef struct DMALinkedList {
    uint32_t source;	
    uint32_t destination;
    uint32_t control;
    struct DMALinkedList* next;
} DMALinkedList;

#define DMA_MEMORY 25

int dma_setup();

#endif


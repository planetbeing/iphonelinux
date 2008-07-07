#include "openiboot.h"
#include "images.h"
#include "nor.h"
#include "util.h"
#include "aes.h"

static const uint32_t IMG2Offset = 0x8400;
static const uint32_t NOREnd = 0xF0000;

Image* imageList = NULL;

static uint32_t MaxOffset = 0;
static uint32_t ImagesStart = 0;
static uint32_t SegmentSize = 0;

int images_setup() {
	IMG2* header;
	Img2Header* curImg2;

	header = (IMG2*) malloc(sizeof(IMG2));

	nor_read(header, IMG2Offset, sizeof(IMG2));

	SegmentSize = header->segmentSize;
	ImagesStart = (header->imagesStart + header->dataStart) * SegmentSize;

	curImg2 = (Img2Header*) malloc(sizeof(Img2Header));

	Image* curImage = NULL;

	uint32_t curOffset;
	for(curOffset = ImagesStart; curOffset < NOREnd; curOffset += SegmentSize) {
		nor_read(curImg2, curOffset, sizeof(Img2Header));
		if(curImg2->signature != Img2Signature)
			continue;

		uint32_t checksum = 0;
		crc32(&checksum, curImg2, 0x64);

		if(checksum != curImg2->header_checksum)
			continue;

		if(curImage == NULL) {
			curImage = (Image*) malloc(sizeof(Image));
			imageList = curImage;
		} else {
			curImage->next = (Image*) malloc(sizeof(Image));
			curImage = curImage->next;
		}

		curImage->type = curImg2->imageType;
		curImage->offset = curOffset;
		curImage->length = curImg2->dataLen;
		curImage->padded = curImg2->dataLenPadded;
		curImage->index = curImg2->index;
		curImage->next = NULL;

		if((curOffset + curImage->padded) > MaxOffset) {
			MaxOffset = curOffset + curImage->padded;
		}
	}

	free(curImg2);
	free(header);
	
	return 0;
}

void images_list() {
	Image* curImage = imageList;

	while(curImage != NULL) {
		print_fourcc(curImage->type);
		bufferPrintf("(%d): %x %x %x\r\n", curImage->index, curImage->offset, curImage->length, curImage->padded);
		curImage = curImage->next;
	}
}

Image* images_get(uint32_t type) {
	Image* curImage = imageList;

	while(curImage != NULL) {
		if(type == curImage->type) {
			return curImage;
		}
		curImage = curImage->next;
	}

	return NULL;
}

void images_release() {
	Image* curImage = imageList;
	Image* toRelease = NULL;
	while(curImage != NULL) {
		toRelease = curImage;
		curImage = curImage->next;
		free(toRelease);
	}
	imageList = NULL;
}

void images_duplicate(Image* image, uint32_t type, int index) {
	if(image == NULL)
		return;

	uint32_t offset = MaxOffset + (SegmentSize - (MaxOffset % SegmentSize));

	uint32_t totalLen = sizeof(Img2Header) + image->padded;
	uint8_t* buffer = (uint8_t*) malloc(totalLen);

	nor_read(buffer, image->offset, totalLen);
	Img2Header* header = (Img2Header*) buffer;
	header->imageType = type;

	if(index >= 0)
		header->index = index;

	uint32_t checksum = 0;
	crc32(&checksum, buffer, 0x64);
	header->header_checksum = checksum;

	nor_write(buffer, offset, totalLen);

	images_release();
	images_setup();
}

void images_erase(Image* image) {
	if(image == NULL)
		return;

	nor_erase_sector(image->offset);

	images_release();
	images_setup();
}

void images_write(Image* image, void* data, unsigned int length) {
	if(image == NULL)
		return;

	uint32_t padded = length;
	if((length & 0xF) != 0) {
		padded = (padded & ~0xF) + 0x10;
	}

	if(image->next != NULL && (image->offset + sizeof(Img2Header) + padded) >= image->next->offset) {
		bufferPrintf("requested length greater than space available in the hole\r\n");
		return;
	}

	uint32_t totalLen = sizeof(Img2Header) + padded;
	uint8_t* writeBuffer = (uint8_t*) malloc(totalLen);
	nor_read(writeBuffer, image->offset, sizeof(Img2Header));
	memcpy(writeBuffer + sizeof(Img2Header), data, length);
	aes_838_encrypt(writeBuffer + sizeof(Img2Header), length, NULL);

	Img2Header* header = (Img2Header*) writeBuffer;
	header->dataLen = length;
	header->dataLenPadded = padded;

	uint32_t checksum = 0;
	crc32(&checksum, writeBuffer, 0x64);
	header->header_checksum = checksum;

	nor_write(writeBuffer, image->offset, totalLen);

	images_release();
	images_setup();
}

unsigned int images_read(Image* image, void** data) {
	if(image == NULL) {
		*data = NULL;
		return 0;
	}

	*data = malloc(image->padded);
	nor_read(*data, image->offset + sizeof(Img2Header), image->length);
	aes_838_decrypt(*data, image->length, NULL);
	return image->length;
}


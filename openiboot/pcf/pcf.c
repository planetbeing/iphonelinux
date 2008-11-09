#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// For simplicity's sake, system endianness is assumed to be little-endian

// Some of these structures are most likely bound by the X11 license, but I'm not sure

typedef struct PCFHeader {
	uint32_t sig;
	uint32_t table_count;
	struct toc_entry {
		uint32_t type;
		uint32_t format;
		uint32_t size;
		uint32_t offset;
	} tables[];
} __attribute__ ((packed)) PCFHeader;

#define PCF_PROPERTIES		    (1<<0)
#define PCF_ACCELERATORS	    (1<<1)
#define PCF_METRICS		    (1<<2)
#define PCF_BITMAPS		    (1<<3)
#define PCF_INK_METRICS		    (1<<4)
#define	PCF_BDF_ENCODINGS	    (1<<5)
#define PCF_SWIDTHS		    (1<<6)
#define PCF_GLYPH_NAMES		    (1<<7)
#define PCF_BDF_ACCELERATORS	    (1<<8)

#define PCF_DEFAULT_FORMAT	0x00000000
#define PCF_INKBOUNDS		0x00000200
#define PCF_ACCEL_W_INKBOUNDS	0x00000100
#define PCF_COMPRESSED_METRICS	0x00000100

#define PCF_GLYPH_PAD_MASK	(3<<0)		/* See the bitmap table for explanation */
#define PCF_BYTE_MASK		(1<<2)		/* If set then Most Sig Byte First */
#define PCF_BIT_MASK		(1<<3)		/* If set then Most Sig Bit First */
#define PCF_SCAN_UNIT_MASK	(3<<4)		/* See the bitmap table for explanation */

typedef struct PCFEncoding {
	uint32_t format;
	uint16_t min_char_or_byte2;
	uint16_t max_char_or_byte2;
	uint16_t min_byte1;
	uint16_t max_byte1;
	uint16_t default_char;
	uint16_t glyphindices[];
} __attribute__ ((packed)) PCFEncoding;

typedef struct PCFBitmaps {
	uint32_t format;
	uint32_t glyphcount;	
	uint8_t data[];
} __attribute__ ((packed)) PCFBitmaps;

typedef struct PCFUncompressedMetrics {
	int16_t left_sided_bearing;
	int16_t right_side_bearing;
	int16_t character_width;
	int16_t character_ascent;
	int16_t character_descent;
	uint16_t character_attributes;
} __attribute__ ((packed)) PCFUncompressedMetrics;

typedef struct PCFCompressedMetrics {
	uint8_t left_sided_bearing;
	uint8_t right_side_bearing;
	uint8_t character_width;
	uint8_t character_ascent;
	uint8_t character_descent;
} __attribute__ ((packed)) PCFCompressedMetrics;

typedef struct PCFUncompressedMetricsTable {
	int32_t metricscount;
	PCFUncompressedMetrics table[];
} __attribute__ ((packed)) PCFUncompressedMetricsTable;

typedef struct PCFCompressedMetricsTable {
	int16_t metricscount;
	PCFCompressedMetrics table[];
} __attribute__ ((packed)) PCFCompressedMetricsTable;

typedef struct PCFMetrics {
	uint32_t format;
	uint8_t data[];
} __attribute__ ((packed)) PCFMetrics;

inline uint16_t swap16(register uint16_t d) {
	return ((d & 0xff) << 8) | (d >> 8);
}

inline uint16_t swap32(register uint32_t d) {
	return ((d & 0xff) << 24) | (d >> 24) | ((d & 0xff00) << 8) | ((d & 0xff0000) >> 8);
}

void getDimensions(PCFMetrics* metrics, uint16_t index, uint32_t* width, uint32_t* height) {
	int16_t ascent;
	int16_t descent;
	if((metrics->format & PCF_COMPRESSED_METRICS) == PCF_COMPRESSED_METRICS) {
		PCFCompressedMetricsTable* m = (PCFCompressedMetricsTable*) metrics->data;
		ascent = m->table[index].character_ascent - 0x80;
		descent = m->table[index].character_descent - 0x80;
		*width = m->table[index].character_width - 0x80;
	} else {
		PCFUncompressedMetricsTable* m = (PCFUncompressedMetricsTable*) metrics->data;
		if(metrics->format & PCF_BYTE_MASK) {
			ascent = swap16(m->table[index].character_ascent);
			descent = swap16(m->table[index].character_descent);
			*width = swap16(m->table[index].character_width);
		} else {
			ascent = m->table[index].character_ascent;
			descent = m->table[index].character_descent;
			*width = m->table[index].character_width;
		}
	}

	*height = ascent + descent;
}

uint16_t getGlyph(PCFEncoding* encoding, uint16_t ch) {
	uint16_t min1, min2, max1, max2;
	if(encoding->format & PCF_BYTE_MASK) {
		min2 = swap16(encoding->min_char_or_byte2);	
		max2 = swap16(encoding->max_char_or_byte2);
		min1 = swap16(encoding->min_byte1);	
		max1 = swap16(encoding->max_byte1);
		ch = swap16(ch);
	} else {
		min2 = encoding->min_char_or_byte2;	
		max2 = encoding->max_char_or_byte2;
		min1 = encoding->min_byte1;	
		max1 = encoding->max_byte1;
	}

	uint8_t* bytes = (uint8_t*) &ch;
	uint16_t index = encoding->glyphindices[((bytes[0] - min1) * (max2 - min2 + 1)) + (bytes[1] - min2)];
	
	if(encoding->format & PCF_BYTE_MASK)
		return swap16(index);
	else 
		return index;
}

inline int getPixel(int x, int y, int width, uint32_t format, uint8_t* data) {
	uint8_t* ptr = NULL;
	switch(format & 0x3) {
		case 0:
			ptr = data + ((width + 7) / 8) * y;
			break;
		case 1:
			ptr = data + ((((width + 7) / 8) + 1) / 2) * 2 * y;
			break;
		case 2:
			ptr = data + ((((width + 7) / 8) + 3) / 4) * 4 * y;
			break;
	}

	int byteIndex = x / 8;
	int bitIndex;

 	if(format & 0x8)	
		bitIndex = 7 - (x % 8);
	else
		bitIndex = x % 8;

	return (*(ptr + byteIndex) >> bitIndex) & 0x1;
}

uint8_t* copyGlyph(PCFBitmaps* bitmaps, PCFMetrics* metrics, uint16_t index) {
	uint32_t* offsets = (uint32_t*) bitmaps->data;
	uint32_t glyphcount;
	uint32_t offset;

	if(index == 0xffff)
		index = 0;

	if(bitmaps->format & PCF_BYTE_MASK) {
		glyphcount = swap32(bitmaps->glyphcount);
		offset = swap32(offsets[index]);
	} else {
		glyphcount = bitmaps->glyphcount;
		offset = offsets[index];
	}

	uint32_t* bitmapSizes = &offsets[glyphcount];
	uint32_t bitmapSize;
	if(bitmaps->format & PCF_BYTE_MASK) {
		bitmapSize = swap32(bitmapSizes[bitmaps->format & 0x3]);
	} else {
		bitmapSize = bitmapSizes[bitmaps->format & 0x3];
	}

	uint32_t width;
	uint32_t height;
	
	getDimensions(metrics, index, &width, &height);

	int i, j;

	uint8_t* glyph = (uint8_t*) malloc(width * height);

	for(i = 0; i < height; i++) {
		for(j = 0; j < width; j++) {
			glyph[i * width + j] = getPixel(j, i, width, bitmaps->format, ((uint8_t*) &bitmapSizes[4]) + offset);
		}
	}

	return glyph;
}

int main(int argc, char* argv[]) {
	if(argc < 3) {
		printf("Usage: %s <input.pcf> <output.oif>\n", argv[0]);
		return 1;
	}

	FILE* file = fopen(argv[1], "rb");
	if(!file) {
		printf("Cannot open file!\n");
		return 1;
	}

	fseek(file, 0, SEEK_END);
	size_t fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	uint8_t* data = (uint8_t*) malloc(fileSize);
	fread(data, 1, fileSize, file);
	fclose(file);

	PCFHeader* header = (PCFHeader*) data;
	if(header->sig != 0x70636601) {
		printf("Not pcf!\n");
		return 1;
	}

	PCFEncoding* encoding;
	PCFBitmaps* bitmaps;
	PCFMetrics* metrics;
	int i;
	for(i = 0; i < header->table_count; i++) {
		if(header->tables[i].type == PCF_BDF_ENCODINGS) {
			printf("encoding table at: %d\n", header->tables[i].offset);
			encoding = (PCFEncoding*)(data + header->tables[i].offset);
		}
		if(header->tables[i].type == PCF_BITMAPS) {
			printf("bitmaps table at: %d\n", header->tables[i].offset);
			bitmaps = (PCFBitmaps*)(data + header->tables[i].offset);
		}
		if(header->tables[i].type == PCF_METRICS) {
			printf("metrics table at: %d\n", header->tables[i].offset);
			metrics = (PCFMetrics*)(data + header->tables[i].offset);
		}
	}

	uint8_t** glyphs = malloc(sizeof(uint8_t*) * 0x100);

	for(i = 0; i <= 0xff; i++) {
		glyphs[i] = copyGlyph(bitmaps, metrics, getGlyph(encoding, i));
	}

	uint32_t width;
	uint32_t height;
	getDimensions(metrics, 0, &width, &height);

	FILE* out = fopen(argv[2], "wb");
	fwrite(&width, 1, 4, out);
	fwrite(&height, 1, 4, out);

	uint8_t bitBuffer = 0;
	int curBit = 0;

	for(i = 0; i <= 0xff; i++) {
		int j;
		for(j = 0; j < (width * height); j++) {
			bitBuffer |= (glyphs[i][j] & 0x1) << curBit;
			curBit++;
			if(curBit == 8) {
				fwrite(&bitBuffer, 1, 1, out);
				bitBuffer = 0;
				curBit = 0;
			}
		}
	}

	if(curBit != 0)
		fwrite(&bitBuffer, 1, 1, out);

	fclose(out);
}


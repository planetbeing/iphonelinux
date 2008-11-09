#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct OpenIBootFont {
	uint32_t width;
	uint32_t height;
	uint8_t data[];
} __attribute__ ((packed)) OpenIBootFont;

inline int getCharPixel(OpenIBootFont* font, int ch, int x, int y) {
	register int bitIndex = ((font->width * font->height) * ch) + (font->width * y) + x;
	return (font->data[bitIndex / 8] >> (bitIndex % 8)) & 0x1;
}

int main(int argc, char* argv[]) {
	if(argc < 3) {
		printf("Usage: %s <fontfile.oif> <character>\n", argv[0]);
		return;
	}

	FILE* file = fopen(argv[1], "rb");
	if(file == NULL) {
		printf("File not found\n");
		return;
	}

	fseek(file, 0, SEEK_END);
	size_t fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	OpenIBootFont* font = (OpenIBootFont*) malloc(fileSize);

	fread(font, 1, fileSize, file);
	fclose(file);


	int i, j;
	for(i = 0; i < font->height; i++) {
		for(j = 0; j < font->width; j++) {
			if(getCharPixel(font, argv[2][0], j, i))
				printf("@");
			else
				printf(" ");
		}
		printf("\n");
	}
}

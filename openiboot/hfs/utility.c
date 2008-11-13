#include <hfs/hfsplus.h>

void hfs_panic(const char* hfs_panicString) {
	bufferPrintf("%s\n", hfs_panicString);
	panic();
}

void printUnicode(HFSUniStr255* str) {
	int i;

	for(i = 0; i < str->length; i++) {
		bufferPrintf("%c", (char)(str->unicode[i] & 0xff));
	}
}

char* unicodeToAscii(HFSUniStr255* str) {
	int i;
	char* toReturn;

	toReturn = (char*) malloc(sizeof(char) * (str->length + 1));

	for(i = 0; i < str->length; i++) {
		toReturn[i] = (char)(str->unicode[i] & 0xff);
	}
	toReturn[i] = '\0';

	return toReturn;
}

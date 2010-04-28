#include <assert.h>
#include <stdio.h>
#include <unzip.h>
#include <common.h>
#include <string.h>
#include <dmg/dmgfile.h>
#include <dmg/filevault.h>

#define DEFAULT_BUFFER_SIZE (1 * 1024 * 1024)
char endianness;

char *createTempFile() {
	char tmpFileBuffer[512];
#ifdef WIN32
	char tmpFilePath[512];
	GetTempPath(512, tmpFilePath);
	GetTempFileName(tmpFilePath, "pwn", 0, tmpFileBuffer);
	CloseHandle(CreateFile(tmpFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL));
#else
	strcpy(tmpFileBuffer, "/tmp/pwnXXXXXX");
	close(mkstemp(tmpFileBuffer));
	FILE* tFile = fopen(tmpFileBuffer, "wb");
	fclose(tFile);
#endif
	return strdup(tmpFileBuffer);
}

char *loadZip(const char* ipsw, const char* toExtract) {
	char *fileName;
	void* buffer;
	unzFile zip;
	unz_file_info pfile_info;
	
	char *tmpFileName = createTempFile();
	ASSERT(zip = unzOpen(ipsw), "error opening ipsw");
	ASSERT(unzGoToFirstFile(zip) == UNZ_OK, "cannot seek to first file in ipsw");
	
	do {
		ASSERT(unzGetCurrentFileInfo(zip, &pfile_info, NULL, 0, NULL, 0, NULL, 0) == UNZ_OK, "cannot get current file info from ipsw");
		fileName = (char*) malloc(pfile_info.size_filename + 1);
		ASSERT(unzGetCurrentFileInfo(zip, NULL, fileName, pfile_info.size_filename + 1, NULL, 0, NULL, 0) == UNZ_OK, "cannot get current file name from ipsw");
		if(strcmp(toExtract, fileName) == 0) {
			ASSERT(unzOpenCurrentFile(zip) == UNZ_OK, "cannot open compressed file in IPSW");
			uLong fileSize = pfile_info.uncompressed_size;
			
			FILE *tmpFile = fopen(tmpFileName, "wb");
			buffer = malloc(DEFAULT_BUFFER_SIZE);
			size_t left = pfile_info.uncompressed_size;
			while(left > 0) {
				size_t toRead;
				if(left > DEFAULT_BUFFER_SIZE)
					toRead = DEFAULT_BUFFER_SIZE;
				else
					toRead = left;
				ASSERT(unzReadCurrentFile(zip, buffer, toRead) == toRead, "cannot read file from ipsw");
				fwrite(buffer, toRead, 1, tmpFile);
				left -= toRead;
			}
			fclose(tmpFile);
			free(buffer);
			break;
		}
	} while(unzGoToNextFile(zip) == UNZ_OK);
	return tmpFileName;
}

void TestByteOrder()
{
	short int word = 0x0001;
	char *byte = (char *) &word;
	endianness = byte[0] ? IS_LITTLE_ENDIAN : IS_BIG_ENDIAN;
}

char *extractPlist(Volume* volume) {
	HFSPlusCatalogRecord* record;
	AbstractFile *outFile;
	char* outFileName = createTempFile();
	
	outFile = createAbstractFileFromFile(fopen(outFileName, "wb"));
	
	if(outFile == NULL) {
		printf("cannot create file");
	}
	
	record = getRecordFromPath("/usr/share/firmware/multitouch/iPhone.mtprops", volume, NULL, NULL);
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord)
			writeToFile((HFSPlusCatalogFile*)record, outFile, volume);
		else
			printf("Not a file\n");
	} else {
		printf("No such file or directory\n");
	}
	
	outFile->close(outFile);
	free(record);
	
	return outFileName;
}

int is_base64(const char c) {
	if ( (c >= 'A' && c <= 'Z') || 
		(c >= 'a' && c <= 'z') || 
          (c >= '0' && c <= '9') ||
	      c == '+' || 
	      c == '/' || 
	      c == '=') return 1;
	return 0;
}

void cleanup_base64(char *inp, const unsigned int size) {
	unsigned int i;
    char *tinp1,*tinp2;
	tinp1 = inp;
     tinp2 = inp;
     for (i = 0; i < size; i++) {
     	if (is_base64(*tinp2)) {
          	*tinp1++ = *tinp2++;
          }
          else {
          	*tinp1 = *tinp2++;
          }
     }
     *(tinp1) = 0;
}

unsigned char decode_base64_char(const char c) {
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
     if (c == '=') return 0;
	return 63;   
}

void decode_base64(const char *inp, unsigned int isize, 
			char *out, unsigned int *osize) {
    unsigned int i;
     char *tinp = (char*)inp; 
     char *tout;
     *osize = isize / 4 * 3;
    tout = tinp;
     for(i = 0; i < isize >> 2; i++) {
		*tout++ = (decode_base64_char(*tinp++) << 2) | (decode_base64_char(*tinp) >> 4);
          *tout++ = (decode_base64_char(*tinp++) << 4) | (decode_base64_char(*tinp) >> 2);
          *tout++ = (decode_base64_char(*tinp++) << 6) | decode_base64_char(*tinp++);
	}
     if (*(tinp-1) == '=') (*osize)--;
     if (*(tinp-2) == '=') (*osize)--;
}


char * find_string(char * string, char * sub_string)
{
    size_t sub_s_len;

    sub_s_len = strlen(sub_string);

    while (*string)
    {
        if (strncmp(string, sub_string, sub_s_len) == 0)
        {
            return string;
        }
        string++;
    }

    return NULL;
}

int main(int argc, const char *argv[]) {
	io_func* io;
	Volume* volume;
	AbstractFile* image;
	FILE * iphone_mtprops;
    size_t file_len;
    char * buffer;
    char * p;
    char * a_speed_firmware;
    size_t a_speed_fw_len;
    size_t final_a_speed_fw_len;
    char * firmware;
    size_t fw_len;
    size_t final_fw_len;
    FILE * output;
	
	if(argc < 4) {
		printf("usage: %s <ipsw> <key> <root-fs-name>\n", argv[0]);
		return 0;
	}
	char *filename = loadZip(argv[1], argv[3]);	
	
	TestByteOrder();
	image = createAbstractFileFromFile(fopen(filename, "rb"));
	image = createAbstractFileFromFileVault(image, argv[2]);
	io = openDmgFilePartition(image, -1);
	
	if(io == NULL) {
		fprintf(stderr, "error: cannot open dmg image\n");
		return 1;
	}
	
	volume = openVolume(io);
	if(volume == NULL) {
		fprintf(stderr, "error: cannot open volume\n");
		CLOSE(io);
		return 1;
	}
	char *plistName = extractPlist(volume);
	CLOSE(io);
	ASSERT(remove(filename) == 0, "Error deleting root filesystem.");

    iphone_mtprops = fopen(plistName, "r");
    assert(iphone_mtprops != NULL);
    fseek(iphone_mtprops, 0, SEEK_END);
    file_len = ftell(iphone_mtprops);

    buffer = (char *)malloc(sizeof(char) * file_len);
    fseek(iphone_mtprops, 0, SEEK_SET);
    fread(buffer, file_len, 1, iphone_mtprops);

    fclose(iphone_mtprops);

    p = find_string(buffer, "<key>A-Speed Firmware</key>");
    assert(p != NULL);

    while (*p)
    {
        if (*p == '\n')
            break;
        p++;
    }
    while (*p)
    {
        if (*p == '>')
            break;
        p++;
    }

    p++;

    a_speed_firmware = p;

    while (*p)
    {
        if (*p == '<')
            break;
        p++;
    }
    a_speed_fw_len = p - a_speed_firmware;

    p = find_string(p, "<key>Firmware</key>");
    assert(p != NULL);

    while (*p)
    {
        if (*p == '\n')
            break;
        p++;
    }
    while (*p)
    {
        if (*p == '>')
            break;
        p++;
    }
    p++;
	firmware = p;
    while (*p)
    {
        if (*p == '<')
            break;
        p++;
    }
    fw_len = p - firmware;
    cleanup_base64(firmware, fw_len);
    decode_base64(firmware, fw_len, firmware, (unsigned int *)&final_fw_len);
    output = fopen("zephyr_main.bin", "a+");
    fwrite(firmware, final_fw_len, 1, output);
    fclose(output);
    cleanup_base64(a_speed_firmware, a_speed_fw_len);
    decode_base64(a_speed_firmware, a_speed_fw_len, a_speed_firmware, (unsigned int *)&final_a_speed_fw_len);
    output = fopen("zephyr_aspeed.bin", "a+");
    fwrite(a_speed_firmware, final_a_speed_fw_len, 1, output);
    fclose(output);
    free(buffer);


	ASSERT(remove(plistName) == 0, "Error deleting iPhone.mtprops");
	printf("Zephyr files extracted succesfully.\n");
	return 0;
}
#include "openiboot.h"
#include "commands.h"
#include "nvram.h"
#include "hfs/fs.h"
#include "hfs/hfsplus.h"
#include "hfs/bdev.h"
#include "printf.h"
/*#include "stdio.h"*/
#include "util.h"


void startScripting(){
	uint32_t size = 0;
	//uint32_t k; /*variable for for loops*/
	uint32_t i; /* another variable for for loops*/
	uint32_t numberOfLines = 1; /*number of lines in the script files*/
	char bufferLine[100];
	char* bufferReady;
	bufferReady = malloc(1);
	int charAT; /* position in the buffer */
	HFSPlusCatalogFile* file;
	const char* partitionScript = nvram_getvar("partition-script"); /*get the partition where the script is in NVRAM*/
	bufferPrintf("partition-script %s\r\n",partitionScript);
	const char* fileScript = nvram_getvar("file-script"); /*get the path to the file script in NVRAM*/
	bufferPrintf("file-script %s\r\n",fileScript);
	/* ------ extracting the script file at address 0x09000000 ----- */
	Volume* volume;
	io_func* io;
	io = bdev_open(parseNumber(partitionScript));
	if(io == NULL) {
		bufferPrintf("fs: cannot read partition!\r\n");
		return;
	}

	volume = openVolume(io);
	if(volume == NULL) {
		bufferPrintf("fs: cannot openHFS volume!\r\n");
		return;
	}
	HFSPlusCatalogRecord* record;
	char* name;
	record = getRecordFromPath(fileScript, volume, &name, NULL);
	if(record != NULL) {
		if(record->recordType == kHFSPlusFolderRecord)
			bufferPrintf("this path is a folder, not a file\r\n");  
		else
		{	file = (HFSPlusCatalogFile*)record;
			uint32_t size = file->dataFork.logicalSize;
			bufferPrintf("size = %d\r\n",size);
			}
	} else {
		bufferPrintf("No such file or directory\r\n");
	}
	uint32_t address = parseNumber("0x09000000");
	if(fs_extract(parseNumber(partitionScript), fileScript, (void*)address)!=0)
	{
		bufferPrintf("script file opened\r\n");
	}
	else{
		bufferPrintf("wrong file\r\n");
	}
	closeVolume(volume);
	CLOSE(io);
	char* addrBOF = (char*)address; /* pointer on the begening of the file*/
	char* addrEOF = (char*)address+size+1; /*pointer 1 byte after the file */
	char* addr; /*pointer on the current space on memory*/
	//addrEOF = (void*)address+size+1;
	/* ----- counting how many lines are present in the script file by the '\n' ----- */
	addr = addrBOF;
	while(addr < addrEOF)
	{
		if(*addr=='\n'){
			numberOfLines++;
		}
		addr++;
	}
	bufferPrintf("number of lines : %d\r\n", numberOfLines);
	/* ----- extracting each line to the buffer -----*/
	addr = addrBOF;
	while(addr < addrEOF)
	{
		/*for(k = 1; k <= numberOfLines; k++)
		{*/
			charAT = 0;
			while(*addr!='\n')
			{
				if(*addr!='\r')
				{
					bufferLine[charAT]=*addr;
					bufferPrintf("reading char : %c\r\n",*addr);
					charAT++;
				}
				addr++;
			}

			bufferLine[charAT]='\0';
			bufferPrintf("number of char (from 0) : %d\r\n",charAT);
			bufferReady = realloc(bufferReady, sizeof(char)*(charAT));
			for(i = 0; i < charAT; i++)
			{
				bufferReady[i] = bufferLine[i];
			}
			bufferReady[i]='\n';
			bufferReady[i+1]='\0';
			bufferPrintf("%s\r\n",bufferReady);
			scriptCommand(bufferReady);
			addr++;
		/*}*/
	}
	free(bufferReady);
}

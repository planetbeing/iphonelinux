#include "openiboot.h"
#include "commands.h"
#include "nvram.h"
#include "hfs/fs.h"
#include "hfs/hfsplus.h"
#include "hfs/bdev.h"
#include "printf.h"
/*#include "stdio.h"*/
#include "util.h"


void startScripting(char* loadedFrom){
	uint32_t size = 0;
	uint32_t i; /*  variable for for loops*/
	//uint32_t numberOfLines = 1; /*number of lines in the script files - To be used with the next commented section of code for debug*/
	char bufferLine[100];
	char* bufferReady;
	bufferReady = malloc(1);
	int charAT; /* position in the buffer */
	HFSPlusCatalogFile* file;
	const char* partitionScript = nvram_getvar("partition-script"); /*get the partition where the script is in NVRAM*/
	//bufferPrintf("partition-script %s\r\n",partitionScript);  /*For debug*/
	const char* fileScript = nvram_getvar("file-script"); /*get the path to the file script in NVRAM*/
	//bufferPrintf("file-script %s\r\n",fileScript);  /*For debug*/
	const char* scriptingLinux = nvram_getvar("scripting-linux"); /*tells whether to run the script before booting linux. Accepted values : true or 1, anything else if false*/
	const char* scriptingOpeniboot = nvram_getvar("scripting-openiboot"); /*tells whether to run the script before launching openiboot console. Accepted values : true or 1, anything else if false*/
	/* ------ extracting the script file at address 0x09000000 ----- */
	if(loadedFrom=="linux")
	{
		if(!((strcmp(scriptingLinux,"true")==0)||(strcmp(scriptingLinux,"1")==0)))
		{
			return; /* terminate the function if scripting is not asked*/
		}
	}
	if(loadedFrom=="openiboot")
	{
		if(!((strcmp(scriptingOpeniboot,"true")==0)||(strcmp(scriptingOpeniboot,"1")==0)))
		{
			return; /* terminate the function if scripting is not asked*/
		}
	}
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
			size = file->dataFork.logicalSize;
			//bufferPrintf("size = %d\r\n",size);  /*size of script file, used later*/
			}
	} else {
		bufferPrintf("No such file or directory\r\n");
	}
	uint32_t address = parseNumber("0x09000000"); /*address where the script file is put in memory  beware 0x09000000 is the default address for receiving files*/
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
	char* addrEOF = (char*)(address+size); /*pointer 1 byte after the file */
	char* addr; /*pointer on the current space on memory*/
	//addrEOF = (void*)address+size+1;
	/* ----- counting how many lines are present in the script file by the '\n' ----- 
	addr = addrBOF;
	while(addr < addrEOF)
	{
		if(*addr=='\r'){
			numberOfLines++;
		}
		addr++;
	}
	bufferPrintf("number of lines : %d\r\n", numberOfLines); */

	/* ----- extracting each line to the buffer -----*/
	addr = addrBOF;
	while(addr < addrEOF)
	{
		charAT = 0;
		while((*addr!='\n')&& (addr<addrEOF))
		{
			if(*addr!='\r')
			{
				bufferLine[charAT]=*addr;
				//bufferPrintf("reading char : %c\r\n",*addr);
				charAT++;
			}
			addr++;
		}
		bufferLine[charAT]='\0';
		bufferReady = realloc(bufferReady, sizeof(char)*(charAT)); /*copying the buffer to a specially allocated area*/
		for(i = 0; i < charAT; i++)
		{
			bufferReady[i] = bufferLine[i];
		}
		bufferReady[i]='\n';
		bufferReady[i+1]='\0';
		bufferPrintf("\r\n%s\r\n",bufferReady);
		if(scriptCommand(bufferReady)==1)
		{
			//bufferPrintf("command sent\r\n");
		}
		/*else { //error in command, function scriptCommand returned false
		}*/
		addr++;
	}
	free(bufferReady);
}

#include "openiboot.h"
#include "commands.h"
#include "nvram.h"
#include "hfs/common.h"
#include "hfs/bdev.h"
#include "hfs/fs.h"
#include "hfs/hfsplus.h"
#include "printf.h"
#include "util.h"

int scriptCommand(char* command){
    int argc;
	char** argv = tokenize(command, &argc);

	OPIBCommand* curCommand = CommandList;

	int success = FALSE;
	while(curCommand->name != NULL) {
		if(strcmp(argv[0], curCommand->name) == 0) {
			curCommand->routine(argc, argv);
			success = TRUE;
			break;
		}
		curCommand++;
	}

	if(!success) {
		bufferPrintf("unknown command: %s\r\n", command);
	}

	free(argv);
	return success;
}

void startScripting(char* loadedFrom)
{
	uint32_t size = 0;


	uint8_t* address = NULL;

	const char* partitionScript = nvram_getvar("partition-script"); /*get the partition where the script is in NVRAM*/
	const char* fileScript = nvram_getvar("file-script"); /*get the path to the file script in NVRAM*/
	const char* scriptingLinux = nvram_getvar("scripting-linux"); /*tells whether to run the script before booting linux. Accepted values : true or 1, anything else if false*/
	const char* scriptingOpeniboot = nvram_getvar("scripting-openiboot"); /*tells whether to run the script before launching openiboot console. Accepted values : true or 1, anything else if false*/

	//uint32_t numberOfLines = 1; /*number of lines in the script files - To be used with the next commented section of code for debug*/
	//bufferPrintf("partition-script %s\r\n",partitionScript);  /*For debug*/
	//bufferPrintf("file-script %s\r\n",fileScript);  /*For debug*/

	if(!partitionScript)
		return;

	if(!fileScript)
		return;

	/* ------ extracting the script file at address 0x09000000 ----- */
	if(strcmp(loadedFrom, "linux") == 0)
	{
		if(!scriptingLinux || (strcmp(scriptingLinux, "true") != 0 && strcmp(scriptingLinux, "1") != 0))
		{
			return; /* terminate the function if scripting is not asked*/
		}
	}

	if(strcmp(loadedFrom, "openiboot") == 0)
	{
		if(!scriptingOpeniboot || (strcmp(scriptingOpeniboot, "true") != 0 && strcmp(scriptingOpeniboot, "1") != 0))
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
		{
			bufferPrintf("this path is a folder, not a file\r\n");
			return;
		} else
		{
			size = readHFSFile((HFSPlusCatalogFile*)record, &address, volume);
			if(!address)
				return;
			//bufferPrintf("size = %d\r\n",size);  /*size of script file, used later*/
		}
	} else {
		bufferPrintf("No such file or directory\r\n");
		return;
	}

	closeVolume(volume);
	CLOSE(io);

	char* addrBOF = (char*) address; /* pointer on the begening of the file*/
	char* addrEOF = (char*) (address + size); /*pointer 1 byte after the file */
	char* addr; /*pointer on the current space on memory*/

#if 0
	//addrEOF = (void*)address+size+1;
	/* ----- counting how many lines are present in the script file by the '\n' -----  */
	addr = addrBOF;
	while(addr < addrEOF)
	{
		if(*addr=='\r'){
			numberOfLines++;
		}
		addr++;
	}
	bufferPrintf("number of lines : %d\r\n", numberOfLines);
#endif

	char* bufferLine = malloc(100);

	/* ----- extracting each line to the buffer -----*/
	addr = addrBOF;
	while(addr < addrEOF)
	{
		int charAt = 0;
		while((*addr != '\n') && (addr < addrEOF))
		{
			if(*addr != '\r')
			{
				bufferLine[charAt] = *addr;
				//bufferPrintf("reading char : %c\r\n",*addr);
				charAt++;
			}
			addr++;
		}

		bufferLine[charAt]='\0';

		bufferPrintf("\r\n%s\r\n", bufferLine);

		bufferLine[charAt]='\n';
		bufferLine[charAt + 1] = '\0';

		if(scriptCommand(bufferLine))
		{
			//bufferPrintf("command sent\r\n");
		}
		/*else { //error in command, function scriptCommand returned false
		}*/
		addr++;
	}

	free(bufferLine);
	free(address);
}

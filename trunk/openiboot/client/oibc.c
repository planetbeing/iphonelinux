#include <stdio.h>
#include <string.h>
#include <usb.h>
#include <pthread.h>

#define OPENIBOOTCMD_DUMPBUFFER 0
#define OPENIBOOTCMD_DUMPBUFFER_LEN 1
#define OPENIBOOTCMD_DUMPBUFFER_GOAHEAD 2
#define OPENIBOOTCMD_SENDCOMMAND 3
#define OPENIBOOTCMD_SENDCOMMAND_GOAHEAD 4

typedef struct OpenIBootCmd {
	uint32_t command;
	uint32_t dataLen;
}  __attribute__ ((__packed__)) OpenIBootCmd;

usb_dev_handle* device;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
FILE* outputFile = NULL;
volatile size_t readIntoOutput = 0;

void* doOutput(void* threadid) {
	OpenIBootCmd cmd;
	char* buffer;
	int totalLen = 0;

	while(1) {
		pthread_mutex_lock(&lock);

		cmd.command = OPENIBOOTCMD_DUMPBUFFER;
		cmd.dataLen = 0;
		usb_interrupt_write(device, 4, (char*) (&cmd), sizeof(OpenIBootCmd), 1000);
		if(usb_interrupt_read(device, 3, (char*) (&cmd), sizeof(OpenIBootCmd), 1000) < 0) {
			exit(0);
		}
		totalLen = cmd.dataLen;

		while(totalLen > 0) {
			buffer = (char*) malloc(totalLen);

			cmd.command = OPENIBOOTCMD_DUMPBUFFER_GOAHEAD;
			cmd.dataLen = totalLen;
			usb_interrupt_write(device, 4, (char*) (&cmd), sizeof(OpenIBootCmd), 1000);

			int read = 0;
			while(read < totalLen) {
				int left = (totalLen - read);
				size_t toRead = (left > 0x80) ? 0x80 : left;
				int hasRead;
				hasRead = usb_bulk_read(device, 1, buffer + read, toRead, 1000);
				read += hasRead;
			}

			int discarded = 0;
			if(readIntoOutput > 0) {
				if(readIntoOutput <= read) {
					fwrite(buffer, 1, readIntoOutput, outputFile);
					discarded += readIntoOutput;
					readIntoOutput = 0;
					fclose(outputFile);
				} else {
					fwrite(buffer, 1, read, outputFile);
					discarded += read;
					readIntoOutput -= read;
				}
			}

			*(buffer + read) = '\0';
			printf("%s", buffer + discarded); fflush(stdout);

			cmd.command = OPENIBOOTCMD_DUMPBUFFER;
			cmd.dataLen = 0;
			usb_interrupt_write(device, 4, (char*) (&cmd), sizeof(OpenIBootCmd), 1000);
			usb_interrupt_read(device, 3, (char*) (&cmd), sizeof(OpenIBootCmd), 1000);
			totalLen = cmd.dataLen;
		}

		pthread_mutex_unlock(&lock);

		pthread_yield();
	}
	pthread_exit(NULL);
}

void sendBuffer(char* buffer, size_t size) {
	OpenIBootCmd cmd;

	cmd.command = OPENIBOOTCMD_SENDCOMMAND;
	cmd.dataLen = size;

	usb_interrupt_write(device, 4, (char*) (&cmd), sizeof(OpenIBootCmd), 1000);

	while(1) {
		usb_interrupt_read(device, 3, (char*) (&cmd), sizeof(OpenIBootCmd), 1000);
		if(cmd.command == OPENIBOOTCMD_SENDCOMMAND_GOAHEAD)
			break;
	}

	usb_bulk_write(device, 2, buffer, size, 1000);
}

void* doInput(void* threadid) {
	char commandBuffer[0x80];

	while(1) {
		fgets(commandBuffer, sizeof(commandBuffer), stdin);
		char* fileBuffer = NULL;
		int len = strlen(commandBuffer);
		if(commandBuffer[len - 1] == '\n') {
			commandBuffer[len - 1] = '\0';
			len--;
		}

		if(commandBuffer[0] == '!') {
			char* atLoc = strchr(&commandBuffer[1], '@');

			if(atLoc != NULL)
				*atLoc = '\0';

			FILE* file = fopen(&commandBuffer[1], "rb");
			if(!file) {
				fprintf(stderr, "file not found: %s\n", &commandBuffer[1]);
				continue;
			}
			fseek(file, 0, SEEK_END);
			len = ftell(file);
			fseek(file, 0, SEEK_SET);
			fileBuffer = malloc(len);
			fread(fileBuffer, 1, len, file);
			fclose(file);

			if(atLoc != NULL) {
				sprintf(commandBuffer, "sendfile %s", atLoc + 1);
			} else {
				sprintf(commandBuffer, "sendfile 0x09000000");
			}

			pthread_mutex_lock(&lock);
			sendBuffer(commandBuffer, strlen(commandBuffer));
			sendBuffer(fileBuffer, len);
			pthread_mutex_unlock(&lock);
		} else if(commandBuffer[0] == '~') {
			char* sizeLoc = strchr(&commandBuffer[1], ':');

			if(sizeLoc == NULL) {
				fprintf(stderr, "must specify length to read\n");
				continue;
			}
			
			*sizeLoc = '\0';
			sizeLoc++;

			int toRead;
			sscanf(sizeLoc, "%d", &toRead);

			char* atLoc = strchr(&commandBuffer[1], '@');

			if(atLoc != NULL)
				*atLoc = '\0';

			FILE* file = fopen(&commandBuffer[1], "wb");
			if(!file) {
				fprintf(stderr, "cannot open file: %s\n", &commandBuffer[1]);
				continue;
			}

			if(atLoc != NULL) {
				sprintf(commandBuffer, "getfile %s %d", atLoc + 1, toRead);
			} else {
				sprintf(commandBuffer, "getfile 0x09000000 %d", toRead);
			}

			pthread_mutex_lock(&lock);
			sendBuffer(commandBuffer, strlen(commandBuffer));
			outputFile = file;
			readIntoOutput = toRead;
			pthread_mutex_unlock(&lock);
		} else {
			pthread_mutex_lock(&lock);
			sendBuffer(commandBuffer, len);
			pthread_mutex_unlock(&lock);
		}

		pthread_yield();
	}
	pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
	usb_init();
	usb_find_busses();
	usb_find_devices();

	struct usb_bus *busses;
	struct usb_bus *bus;
	struct usb_device *dev;

	busses = usb_get_busses();

	int found = 0;
	int c, i, a;
	for(bus = busses; bus; bus = bus->next) {
    		for (dev = bus->devices; dev; dev = dev->next) {
    			/* Check if this device is a printer */
    			if (dev->descriptor.idVendor != 0x05ac || dev->descriptor.idProduct != 0x1280) {
    				continue;
    			}
    
			/* Loop through all of the interfaces */
			for (i = 0; i < dev->config[0].bNumInterfaces; i++) {
				for (a = 0; a < dev->config[0].interface[i].num_altsetting; a++) {
					if(dev->config[0].interface[i].altsetting[a].bInterfaceClass == 0xFF
						&& dev->config[0].interface[i].altsetting[a].bInterfaceSubClass == 0xFF
						&& dev->config[0].interface[i].altsetting[a].bInterfaceProtocol == 0x51) {
						goto done;
					}
    				}
    			}
    		}
	}

	return 1;

done:

	device = usb_open(dev);
	if(!device) {
		return 2;
	}

	if(usb_claim_interface(device, i) != 0) {
		return 3;
	}

	pthread_t inputThread;
	pthread_t outputThread;

	printf("Client connected: !<filename>[@<address>] to send a file, ~<filename>[@<address>]:<len> to receive a file\n");
	printf("---------------------------------------------------------------------------------------------------------\n");

	pthread_create(&outputThread, NULL, doOutput, NULL);
	pthread_create(&inputThread, NULL, doInput, NULL);

	pthread_exit(NULL);

	usb_release_interface(device, i);
	usb_close(device);
}


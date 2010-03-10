#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <usb.h>
#include <pthread.h>
#include <ncurses.h>

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

volatile int InterestWrite = 0;

#define USB_BYTES_AT_A_TIME 512

void* doOutput(void* threadid) {
	OpenIBootCmd cmd;
	char* buffer;
	int totalLen = 0;

	while(1) {
		if(InterestWrite)
		{
			sched_yield();
			continue;
		}

		pthread_mutex_lock(&lock);

		cmd.command = OPENIBOOTCMD_DUMPBUFFER;
		cmd.dataLen = 0;
		usb_interrupt_write(device, 4, (char*) (&cmd), sizeof(OpenIBootCmd), 1000);
		if(usb_interrupt_read(device, 3, (char*) (&cmd), sizeof(OpenIBootCmd), 1000) < 0) {
			endwin();
			exit(0);
		}
		totalLen = cmd.dataLen;

		while(totalLen > 0) {
			buffer = (char*) malloc(totalLen + 1);

			cmd.command = OPENIBOOTCMD_DUMPBUFFER_GOAHEAD;
			cmd.dataLen = totalLen;
			usb_interrupt_write(device, 4, (char*) (&cmd), sizeof(OpenIBootCmd), 1000);

			int read = 0;
			while(read < totalLen) {
				int left = (totalLen - read);
				size_t toRead = (left > USB_BYTES_AT_A_TIME) ? USB_BYTES_AT_A_TIME : left;
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

			free(buffer);

			cmd.command = OPENIBOOTCMD_DUMPBUFFER;
			cmd.dataLen = 0;
			usb_interrupt_write(device, 4, (char*) (&cmd), sizeof(OpenIBootCmd), 1000);
			usb_interrupt_read(device, 3, (char*) (&cmd), sizeof(OpenIBootCmd), 1000);
			totalLen = cmd.dataLen;
		}

		pthread_mutex_unlock(&lock);

		sched_yield();
	}
	pthread_exit(NULL);
}

#define MAX_TO_SEND 512

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

	int toSend = 0;
	while(size > 0) {
		if(size <= MAX_TO_SEND)
			toSend = size;
		else
			toSend = MAX_TO_SEND;

		usb_bulk_write(device, 2, buffer, toSend, 1000);
		buffer += toSend;
		size -= toSend;
	}
}

void* doInput(void* threadid) {
	while(1) {
		int ch = getch();

		char theChar = ch;
		InterestWrite = 1;
		pthread_mutex_lock(&lock);
		sendBuffer(&theChar, 1);
		pthread_mutex_unlock(&lock);
		InterestWrite = 0;

		sched_yield();
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
	initscr();
	noecho();

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


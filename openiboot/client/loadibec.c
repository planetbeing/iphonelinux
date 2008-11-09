// Reverse engineering courtesty of c1de0x, et al. of the iPhone Dev Team

#include <usb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>

#define APPLE_VENDOR_ID			0x05AC
#define IPHONE_1_2_RECOVERY_PRODUCT_ID	0x1281

#define MSG_DUMP_BUFFER			0x802
#define MSG_SEND_COMMAND		0x803
#define MSG_SEND_FILE			0x805
#define MSG_ACK				0x808
#define MSG_REJ				0x809

#define PROTOCOL_1_2_EP_CONTROL_W	0x00

char response_buffer[0x1000];

int send_command(usb_dev_handle* device, char* command) {
	return usb_bulk_write(device, 0x02, command, strlen(command), 1000);
}

int send_file(usb_dev_handle* device, const char* fileName) {
	static char file_buffer[0x1000];

	FILE* file = fopen(fileName, "rb");

	if(file == NULL) {
		fprintf(stderr, "file not found: %s\n", fileName);
		return -1;
	}

	// request file buffer pointer
	usb_control_msg(device, 0x21, 6, 1, 0, "", 0, 1000);

	int value = 0;
	while(1) {
		int len = fread(file_buffer, 1, 0x1000, file);
		usb_control_msg(device, 0x21, 1, value, 0, file_buffer, len, 1000);
		value++;

		if(len < 0x1000)
			break;
	}

	fclose(file);

	usb_control_msg(device, 0x21, 1, 1, 0, "", 0, 1000);

	uint32_t retval;
	uint8_t statusPkt[6];
	retval = usb_control_msg(device, 0xa1, 3, 1, 0, statusPkt, 6, 1000);
	if(statusPkt[4] != 0x6) {
		fprintf(stderr, "Uploaded failed: 0x%x\n", retval);
		return -1;
	}
	fprintf(stderr, "Uploaded ended\n");

	retval = usb_control_msg(device, 0xa1, 3, 1, 0, statusPkt, 6, 1000);
	if(statusPkt[4] != 0x7) {
		fprintf(stderr, "Upload confirmation failed: 0x%x\n", retval);
		return -1;
	}
	fprintf(stderr, "Upload confirmed\n");

	retval = usb_control_msg(device, 0xa1, 3, 1, 0, statusPkt, 6, 1000);
	if(statusPkt[4] != 0x8) {
		fprintf(stderr, "Filesize set failed: 0x%x\n", retval);
		return -1;
	}
	fprintf(stderr, "Filesize set\n");

	return 0;
}

int get_response(usb_dev_handle* device) {
	return usb_bulk_read(device, 0x81, response_buffer, sizeof(response_buffer), 1000);
}

char* strnstr(const char* haystack, const char* needle, int len) {
	int i;
	const char* x = needle;
	for(i = 0; i < len; i++) {
		if(haystack[i] == *x) {
			x++;
			if(*x == '\0')
				return (char*)(&haystack[i] - (x - needle));
		} else {
			x = needle;
		}
	}

	return NULL;
}

int interactive(usb_dev_handle* device) {
	int len;
	char* commandBuffer = NULL;

	do {
		do {
			usleep(100000);
			len = get_response(device);

			if(len > 0) {
				fwrite(response_buffer, 1, len, stdout);
				fflush(stdout);
			}
		} while(len > 0 && strnstr(response_buffer, "] ", len) == NULL);

		if(len < 0)
			break;

ProcessCommand:
		if(commandBuffer != NULL)
			free(commandBuffer);

		commandBuffer = readline(NULL);
		if(commandBuffer && *commandBuffer) {
			add_history(commandBuffer);
		}

		if(strncmp(commandBuffer, "sendfile ", sizeof("sendfile ") - 1) == 0) {
			char* file = commandBuffer + sizeof("sendfile ") - 1;
			send_file(device, file);
			goto ProcessCommand;
		} else {
			send_command(device, commandBuffer);
			send_command(device, "\n");
		}

	} while(1);

	return 0;
}

int main(int argc, char* argv[]) {
	usb_dev_handle* device;

#ifdef HAVE_GETEUID
	if(geteuid() != 0) {
		fprintf(stderr, "Must run as root\n");
		return 1;
	}
#endif

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
    			if (dev->descriptor.idVendor != APPLE_VENDOR_ID || dev->descriptor.idProduct != IPHONE_1_2_RECOVERY_PRODUCT_ID) {
    				continue;
    			}

			goto done;	
    		}
	}

	return 1;

done:

	device = usb_open(dev);
	usb_claim_interface(device, 1);
	usb_set_altinterface(device, 1);

	if(argc >= 2) {
		if(strcmp(argv[1], "-i") == 0) {
			interactive(device);
		} else {
			send_file(device, argv[1]);
			send_command(device, "go\n");
		}
	} else {
		printf("loadibec - Loads an iBEC img3 (such as openiboot.img3) into a phone in recovery mode. Use '-i' for interactive mode (buggy).\n");
		printf("Usage: %s <-i|ibec.img3>", argv[0]);
	}

	return 0;
}

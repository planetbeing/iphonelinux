// Reverse engineering courtesty of c1de0x, et al. of the iPhone Dev Team

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libusb-1.0/libusb.h>
#include <readline/readline.h>

#define USB_APPLE_ID		0x05AC
#define USB_RECOVERY		0x1281
#define USB_DFU_MODE		0x1227

struct libusb_device_handle* open_device(int devid) {
	int configuration = 0;
	struct libusb_device_handle* handle = NULL;

	libusb_init(NULL);
	handle = libusb_open_device_with_vid_pid(NULL, USB_APPLE_ID, devid);
	if (handle == NULL) {
		printf("open_device: unable to connect to device.\n");
		return NULL;
	}

	libusb_get_configuration(handle, &configuration);
	if(configuration != 1) {
		if (libusb_set_configuration(handle, 1) < 0) {
			printf("open_device: error setting configuration.\n");
			return NULL;
		}
	}
	if (libusb_claim_interface(handle, 0) < 0) {
		printf("open_device: error claiming interface.");
		return NULL;
	}

	return handle;
}

int close_device(struct libusb_device_handle* handle) {
	if (handle == NULL) {
		printf("close_device: device has not been initialized yet.\n");
		return -1;
	}

	libusb_release_interface(handle, 0);
	libusb_release_interface(handle, 1);
	libusb_close(handle);
	libusb_exit(NULL);
	return 0;
}

int send_command(struct libusb_device_handle* handle, char* command) {
	size_t length = strlen(command);
	if (length >= 0x200) {
		printf("send_command: command is too long.\n");
		return -1;
	}

	if (!libusb_control_transfer(handle, 0x40, 0, 0, 0, command, length + 1, 1000)) {
		printf("send_command: unable to send command.\n");
		return -1;
	}
 
	return 0;
}

int send_file(struct libusb_device_handle *handle, const char* filename) {
	if(handle == NULL) {
		printf("send_file: device has not been initialized yet.\n");
		return -1;
	}
	
	FILE* file = fopen(filename, "rb");
	if(file == NULL) {
		printf("send_file: unable to find file.\n");
		return 1;
	}

	fseek(file, 0, SEEK_END);
	long len = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* buffer = malloc(len);
	if (buffer == NULL) {
		printf("send_file: error allocating memory.\n");
		fclose(file);
		return 1;
	}

	fread(buffer, 1, len, file);
	fclose(file);

	int packets = len / 0x800;
	if(len % 0x800) {
		packets++;
	}

	int last = len % 0x800;
	if(!last) {
		last = 0x800;
	}

	int i = 0;
	char response[6];
	for(i = 0; i < packets; i++) {
		int size = i + 1 < packets ? 0x800 : last;

		if(!libusb_control_transfer(handle, 0x21, 1, i, 0, &buffer[i * 0x800], size, 1000)) {
			printf("send_file: error sending packet.\n");
			return -1;
		}

		if(libusb_control_transfer(handle, 0xA1, 3, 0, 0, response, 6, 1000) != 6) {
			printf("send_file: error receiving status.\n");
			return -1;

		} else {
			if(response[4] != 5) {
				printf("send_file: invalid status.\n");
				return -1;
			}
		}

	}

	libusb_control_transfer(handle, 0x21, 1, i, 0, buffer, 0, 1000);
	for(i = 6; i <= 8; i++) {
		if(libusb_control_transfer(handle, 0xA1, 3, 0, 0, response, 6, 1000) != 6) {
			printf("send_file: error receiving status.\n");
			return -1;

		} else {
			if(response[4] != i) {
				printf("send_file: invalid status.\n");
				return -1;
			}
		}
	}

	free(buffer);
	return 0;
}

int main(int argc, char* argv[]) {
	struct libusb_device_handle* handle = NULL;
	if (argc != 2) {
		printf("usage: loadibec <img3>\n");
		return -1;
	}

	handle = open_device(USB_RECOVERY);
	if (handle == NULL) {
		printf("your device must be in recovery mode.\n");
		return -1;
	}
	
	// TODO: add interactive mode
	if(send_file(handle, argv[1]) >= 0) {
	    send_command(handle, "go");
	}

    close_device(handle);
	return 0;
}

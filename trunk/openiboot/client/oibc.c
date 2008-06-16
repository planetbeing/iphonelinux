#include <stdio.h>
#include <usb.h>

#define OPENIBOOTCMD_DUMPBUFFER 0
#define OPENIBOOTCMD_DUMPBUFFER_LEN 1
#define OPENIBOOTCMD_DUMPBUFFER_GOAHEAD 2

typedef struct OpenIBootCmd {
	uint32_t command;
	uint32_t dataLen;
}  __attribute__ ((__packed__)) OpenIBootCmd;

usb_dev_handle *find_openiboot() {
	usb_init();
	usb_find_busses();
	usb_find_devices();

	struct usb_bus *busses;
	struct usb_bus *bus;
	struct usb_device *dev;
	usb_dev_handle* device;

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

	return NULL;

done:

	device = usb_open(dev);
	printf("open: %x\n", device);
	printf("claim: %d %d\n", i, usb_claim_interface(device, i));

	OpenIBootCmd cmd;
	char buffer[1024];
	int read = 0;
	cmd.command = OPENIBOOTCMD_DUMPBUFFER;
	printf("write: %d\n", usb_interrupt_write(device, 4, (char*) (&cmd), sizeof(OpenIBootCmd), 1000)); fflush(stdout);
	while(read == 0) {
		printf("read: %d\n", read = usb_interrupt_read(device, 3, (char*) (&cmd), sizeof(OpenIBootCmd), 1000)); fflush(stdout);
	}
	cmd.command = OPENIBOOTCMD_DUMPBUFFER_GOAHEAD;
	printf("write: %d\n", usb_interrupt_write(device, 4, (char*) (&cmd), sizeof(OpenIBootCmd), 1000)); fflush(stdout);
	read = 0;
	while(read == 0) {
		printf("read: %d %d\n", cmd.dataLen, read = usb_bulk_read(device, 1, buffer, cmd.dataLen, 1000)); fflush(stdout);
	}
	printf("exiting\n"); fflush(stdout);
	//printf(buffer); fflush(stdout);
	usb_release_interface(device, i);
	usb_close(device);
}

int main(int argc, char* argv[]) {
	find_openiboot();
}


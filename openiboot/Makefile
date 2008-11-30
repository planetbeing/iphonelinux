CROSS ?= arm-elf-
CC = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy
HFS_OBJS = hfs/btree.o hfs/catalog.o hfs/extents.o hfs/fastunicodecompare.o hfs/rawfile.o hfs/utility.o hfs/volume.o hfs/bdev.o hfs/fs.o
OPENIBOOT_OBJS=entry.o openiboot.o arm.o tasks.o openiboot-asmhelpers.o mmu.o miu.o clock.o power.o uart.o interrupt.o timer.o event.o usb.o gpio.o util.o printf.o malloc.o dma.o nor.o aes.o spi.o chipid.o lcd.o i2c.o images.o sha1.o commands.o pmu.o buttons.o framebuffer.o menu.o actions.o nvram.o nand.o ftl.o stb_image.o
OPENIBOOT_OBJS += $(HFS_OBJS)
ARCHFLAGS += -mlittle-endian -mfpu=vfp -mthumb -mthumb-interwork
LDFLAGS += -Ttext=0x00000000 --nostdlib
LIBRARIES += -lgcc -nostdlib
INC += -Iincludes
CFLAGS += -Wall -O3
CONFIGFLAGS =
VERSION = "0.1"
COMMIT = $(shell git log | head -n1 | cut -b8-14)
BUILD = "$(COMMIT)"

ifeq ($(PLATFORM),IPOD)
	CONFIGFLAGS += -DCONFIG_IPOD
	IMG3TEMPLATE = mk8900image/template-ipod.img3
else
ifeq ($(PLATFORM),3G)
	CONFIGFLAGS += -DCONFIG_3G
	IMG3TEMPLATE = mk8900image/template-3g.img3
else
	CONFIGFLAGS += -DCONFIG_IPHONE
	IMG3TEMPLATE = mk8900image/template.img3
endif
endif

ifeq ($(DEBUG),YES)
	CONFIGFLAGS += -DDEBUG
endif

CONFIGFLAGS += -DOPENIBOOT_VERSION=$(VERSION) -DOPENIBOOT_VERSION_BUILD=$(BUILD)

CFLAGS += $(CONFIGFLAGS)

%.o:	%.S
	$(CC) $(INC) $(CFLAGS) $(ARCHFLAGS) -c $< -o $@

%.o:	%.c
	$(CC) $(INC) $(CFLAGS) $(ARCHFLAGS) -c $< -o $@


all:	openiboot.img2	openiboot.img3 openiboot.bin

all-payload: openiboot-payload.img2 openiboot-payload.bin

openiboot:	$(OPENIBOOT_OBJS)
	$(CC) $(ARCHFLAGS) $(LDFLAGS) $(OPENIBOOT_OBJS) $(LIBRARIES) -o $@

payload.o:	payload.bin
	$(OBJCOPY) -I binary -O elf32-little -B arm $< $@

openiboot-payload: $(OPENIBOOT_OBJS) payload.o
	$(CC) --no-warn-mismatch $(LDFLAGS) $(OPENIBOOT_OBJS) payload.o $(LIBRARIES) -o $@

mk8900image/mk8900image:
	cd mk8900image/; make

openiboot.bin: openiboot mk8900image/mk8900image
	mk8900image/mk8900image openiboot openiboot.bin

openiboot.img2:	openiboot mk8900image/mk8900image
	mk8900image/mk8900image openiboot openiboot.img2 mk8900image/template.img2 mk8900image/iphonelinux.der

openiboot.img3:	openiboot mk8900image/mk8900image
	mk8900image/mk8900image openiboot openiboot.img3 $(IMG3TEMPLATE)

openiboot-payload.bin: openiboot-payload mk8900image/mk8900image
	mk8900image/mk8900image openiboot-payload openiboot-payload.bin

openiboot-payload.img2: openiboot-payload mk8900image/mk8900image
	mk8900image/mk8900image openiboot-payload openiboot-payload.img2 mk8900image/template.img2 mk8900image/iphonelinux.der

clean:
	-rm *.o
	-rm hfs/*.o
	-rm openiboot
	-rm openiboot.img2
	-rm openiboot.img3
	-rm openiboot.bin
	-rm openiboot-payload
	-rm openiboot-payload.img2
	-rm openiboot-payload.bin


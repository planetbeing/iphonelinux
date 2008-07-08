AS = arm-elf-as
LD = arm-elf-ld
CC = arm-elf-gcc
OPENIBOOT_OBJS=entry.o openiboot.o arm.o tasks.o openiboot-asmhelpers.o mmu.o miu.o clock.o power.o uart.o interrupt.o timer.o event.o usb.o gpio.o util.o printf.o malloc.o dma.o nor.o aes.o spi.o chipid.o lcd.o i2c.o images.o sha1.o
ASFLAGS += -mcpu=arm11
ARCHFLAGS += -mlittle-endian -mfpu=vfp -mthumb -mthumb-interwork
LDFLAGS += -Ttext 0x18000000 -Tdata 0x18021980 --nostdlib
LIBRARIES += -lgcc -nostdlib
LIBRARY_PATH += -L/usr/local/lib/gcc/arm-elf/4.1.1/interwork/vfp
INC += -Iincludes
CFLAGS += -Wall -O3

%.o:	%.S
	$(CC) $(INC) -E -x c $< | $(AS) $(ASFLAGS) $(ARCHFLAGS) -o $@

%.o:	%.c
	$(CC) $(INC) $(CFLAGS) $(ARCHFLAGS) -c $< -o $@


all:	openiboot.img2	openiboot.img3 openiboot.bin openiboot-iboot.img2

openiboot:	$(OPENIBOOT_OBJS)
	$(LD) $(LDFLAGS) $(OPENIBOOT_OBJS) $(LIBRARIES) $(LIBRARY_PATH) -o $@

mk8900image/mk8900image:
	cd mk8900image/; make

openiboot.bin: openiboot mk8900image/mk8900image
	mk8900image/mk8900image openiboot openiboot.bin

openiboot.img2:	openiboot mk8900image/mk8900image
	mk8900image/mk8900image openiboot openiboot.img2 mk8900image/template.img2 mk8900image/iphonelinux.der

openiboot.img3:	openiboot mk8900image/mk8900image
	mk8900image/mk8900image openiboot openiboot.img3 mk8900image/template.img3 mk8900image/iphonelinux.der

openiboot-iboot.img2:
	mk8900image/mk8900image openiboot openiboot-iboot.img2 mk8900image/iboot-template.img2

clean:
	-rm *.o
	-rm openiboot
	-rm openiboot-iboot.img2
	-rm openiboot.img2
	-rm openiboot.img3
	-rm openiboot.bin


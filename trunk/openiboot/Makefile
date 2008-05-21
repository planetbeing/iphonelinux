AS = arm-elf-as
LD = arm-elf-ld
CC = arm-elf-gcc
OPENIBOOT_OBJS=entry.o openiboot.o openiboot-asmhelpers.o mmu.o

ASFLAGS += -mcpu=arm11
ARCHFLAGS += -mlittle-endian -mfpu=vfp -mthumb -mthumb-interwork
LDFLAGS += -Ttext 0x18000000 -Tdata 0x18021000
LIBRARIES += -lgcc
LIBRARY_PATH += -L/usr/local/lib/gcc/arm-elf/4.1.1/interwork/vfp

%.o:	%.S
	$(CC) -E -x c $< | $(AS) $(ASFLAGS) $(ARCHFLAGS) -o $@

%.o:	%.c
	$(CC) $(CFLAGS) $(ARCHFLAGS) -c $< -o $@


all:	openiboot.img2

openiboot:	$(OPENIBOOT_OBJS)
	$(LD) $(LDFLAGS) $(OPENIBOOT_OBJS) $(LIBRARIES) $(LIBRARY_PATH) -o $@

mk8900image/mk8900image:
	cd mk8900image/; make

openiboot.img2:	openiboot mk8900image/mk8900image
	mk8900image/mk8900image openiboot openiboot.img2 mk8900image/template.img2
clean:
	-rm *.o
	-rm openiboot
	-rm openiboot.img2


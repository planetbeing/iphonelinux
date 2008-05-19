AS = arm-elf-as
LD = arm-elf-ld
CC = arm-elf-gcc
OPENIBOOT_OBJS=entry.o openiboot.o openibootasm.o

ASFLAGS += -mcpu=arm11
ARCHFLAGS += -mlittle-endian -mfpu=fpa -mthumb -mthumb-interwork
LDFLAGS += -Ttext 0x18000000 -Tdata 0x18021000
LIBRARIES += -lgcc
LIBRARY_PATH += -L/usr/local/lib/gcc/arm-elf/4.1.1/interwork

%.o:	%.S
	$(AS) $(ASFLAGS) $(ARCHFLAGS) $< -o $@

%.o:	%.c
	$(CC) $(CFLAGS) $(ARCHFLAGS) -c $< -o $@


all:	openiboot

openiboot:	$(OPENIBOOT_OBJS)
	$(LD) $(LDFLAGS) $(OPENIBOOT_OBJS) $(LIBRARIES) $(LIBRARY_PATH) -o $@

clean:
	rm *.o
	rm openiboot
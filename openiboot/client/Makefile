OIBC_OBJS = oibc.o
LOADIBEC_OBJS = loadibec.o
LINUX_OBJS = linux.o
LIBRARIES = -lusb -lpthread -lreadline
CFLAGS += -DHAVE_GETEUID

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@


all:	oibc loadibec linux

oibc:	$(OIBC_OBJS)
	$(CC) $(CFLAGS) $(OIBC_OBJS) $(LIBRARIES) -o $@

loadibec: ${LOADIBEC_OBJS}
	$(CC) $(CFLAGS) $(LOADIBEC_OBJS) $(LIBRARIES) -o $@

linux:	$(LINUX_OBJS)
	$(CC) $(CFLAGS) $(LINUX_OBJS) -lusb -lpthread -lncurses -o $@

clean:
	-rm *.o
	-rm oibc
	-rm loadibec


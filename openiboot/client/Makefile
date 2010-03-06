OIBC_OBJS = oibc.o
LOADIBEC_OBJS = loadibec.o
LINUX_OBJS = linux.o
LIBRARIES = -L/opt/local-universal-10.4/lib -lusb -lpthread -lreadline
LOADIBEC_LIBS = -L/opt/local-universal-10.4/lib -lusb-1.0
CFLAGS += -DHAVE_GETEUID -I/opt/local-universal-10.4/include

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@


all:	oibc loadibec linux

oibc:	$(OIBC_OBJS)
	$(CC) $(CFLAGS) $(OIBC_OBJS) $(LIBRARIES) -o $@

loadibec: ${LOADIBEC_OBJS}
	$(CC) $(CFLAGS) $(LOADIBEC_OBJS) $(LOADIBEC_LIBS) -o $@

linux:	$(LINUX_OBJS)
	$(CC) $(CFLAGS) $(LINUX_OBJS) -lusb -lpthread -lncurses -o $@

clean:
	-rm *.o
	-rm oibc
	-rm loadibec


OIBC_OBJS = oibc.o
LIBRARIES = -lusb -lpthread -lreadline

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@


all:	oibc

oibc:	$(OIBC_OBJS)
	$(CC) $(CFLAGS) $(OIBC_OBJS) $(LIBRARIES) -o $@

clean:
	-rm *.o
	-rm oibc


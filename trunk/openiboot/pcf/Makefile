PCF_OBJS = pcf.o
OIF_OBJS = oif.o
LIBRARIES =

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@


all:	pcf oif

pcf:	$(PCF_OBJS)
	$(CC) $(CFLAGS) $(PCF_OBJS) $(LIBRARIES) -o $@

oif:	$(OIF_OBJS)
	$(CC) $(CFLAGS) $(OIF_OBJS) $(LIBRARIES) -o $@

clean:
	-rm *.o
	-rm oif
	-rm pcf


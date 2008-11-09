PCF_OBJS = pcf.o
OIF_OBJS = oif.o
BIN2C_OBJS = bin2.o
LIBRARIES =

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@


all:	pcf oif bin2c

pcf:	$(PCF_OBJS)
	$(CC) $(CFLAGS) $(PCF_OBJS) $(LIBRARIES) -o $@

oif:	$(OIF_OBJS)
	$(CC) $(CFLAGS) $(OIF_OBJS) $(LIBRARIES) -o $@

bin2c:	$(BIN2C_OBJS)
	$(CC) $(CFLAGS) $(BIN2C_OBJS) $(LIBRARIES) -o $@

clean:
	-rm *.o
	-rm oif
	-rm pcf


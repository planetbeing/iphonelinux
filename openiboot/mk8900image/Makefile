MKIMAGEOBJS=mk8900image.o
LIBXPWN=libxpwn.a libcommon.a
LIBRARIES=-L/usr/lib32 -L/opt/local-universal-10.4/lib -lm -ldl -lpng -lcrypto -lz
CFLAGS += -m32

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

mk8900image:	$(MKIMAGEOBJS) $(LIBXPWN)
	$(CC) $(CFLAGS) $(MKIMAGEOBJS) $(LIBXPWN) $(LIBRARIES) -o $@

clean:
	-rm *.o
	-rm mk8900image


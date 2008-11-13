MKIMAGEOBJS=mk8900image.o
LIBXPWN=libxpwn.a
LIBRARIES=-lm -ldl
CFLAGS += -m32

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

mk8900image:	$(MKIMAGEOBJS) $(LIBXPWN)
	$(CC) $(CFLAGS) $(MKIMAGEOBJS) $(LIBXPWN) $(LIBRARIES) -o $@

clean:
	-rm *.o
	-rm mk8900image


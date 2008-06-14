MKIMAGEOBJS=mk8900image.o
LIBXPWN=libxpwn.a
LIBRARIES=-lm -ldl

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

mk8900image:	$(MKIMAGEOBJS) $(LIBXPWN)
	$(CC) $(MKIMAGEOBJS) $(LIBXPWN) $(LIBRARIES) -o $@

clean:
	-rm *.o
	-rm mk8900image


MKIMAGEOBJS=mk8900image.o
LIBRARIES=libxpwn.a -lm

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

mk8900image:	$(MKIMAGEOBJS)
	$(CC) $(MKIMAGEOBJS) $(LIBRARIES) -o $@

clean:
	-rm *.o
	-rm mk8900image


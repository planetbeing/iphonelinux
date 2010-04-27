MKIMAGEOBJS=mk8900image.o
LIBRARIES=-L/usr/lib -lm -ldl -lpng -lcrypto -lz
CFLAGS +=

ifeq ($(ARCH), x86_64)
	LIBXPWN=x86_64/libxpwn.a x86_64/libcommon.a
else
	LIBXPWN=x86/libxpwn.a x86/libcommon.a
endif

%.o:	%.c
	@echo "Compiling $@"
	@$(CC) $(CFLAGS) -c $< -o $@

mk8900image:	$(MKIMAGEOBJS) $(LIBXPWN)
	@echo "Building $@"
	@$(CC) $(CFLAGS) $(MKIMAGEOBJS) $(LIBXPWN) $(LIBRARIES) -o $@

clean:
	@rm -f *.o
	@rm -f mk8900image

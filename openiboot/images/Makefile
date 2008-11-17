all:	ConsolePNG.h HeaderPNG.h iPhoneOSPNG.h SelectionPNG.h

bin2c:	bin2c.c
	$(CC) bin2c.c -o $@

%PNG.h:	bin2c %.png
	./bin2c data$*PNG < $< > $@

clean:
	-rm *.o
	-rm bin2c
	-rm *.h

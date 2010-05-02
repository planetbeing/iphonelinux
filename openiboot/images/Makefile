all:	ConsolePNG.h ConsoleSelectedPNG.h HeaderPNG.h iPhoneOSPNG.h iPhoneOSSelectedPNG.h AndroidOSSelectedPNG.h AndroidOSPNG.h

bin2c:	bin2c.c
	$(CC) bin2c.c -o $@

%PNG.h:	%.png bin2c
	./bin2c data$*PNG < $< > $@

clean:
	-rm *.o
	-rm bin2c
	-rm *.h

dripwn : dripwn.o
	gcc ./dripwn.o ./libdmg.a ./libhfs.a ./libcommon.a ./libminizip.a -lz -lcrypto -o ./dripwn

dripwn.o : dripwn.c
	gcc ./dripwn.c -L./minizip/ -I./minizip/ -L./includes/ -I./includes -c
#
# Simple make for ibooter/libibooter
#
CC=g++

CFLAGS=-g
INC=-I./include/

SRCDIR=src
OBJDIR=build

LDFLAGS_SO=-lusb -shared
LDFLAGS=-L../$(OBJDIR) -libooter

OBJS=example.o
LIBOBJS=libibooter.o


all: prepare libibooter.so example

VPATH=$(SRCDIR)

.cpp.o:
	$(CC) $(CFLAGS) -c $< $(INC) -o $(OBJDIR)/$@

libibooter.so: $(LIBOBJS)
	cd $(OBJDIR); $(CC) $(LDFLAGS_SO) $^ -o $@

example: $(OBJS) libibooter.so
	cd $(OBJDIR); $(CC) $(LDFLAGS) $^ -o $@

prepare:
	mkdir -p $(OBJDIR)

clean:
	 rm -rf $(OBJDIR)

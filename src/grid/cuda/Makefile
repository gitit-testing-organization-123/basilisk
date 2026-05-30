.PHONY: all clean

CFLAGS += -O2

all: libreduce.a

libreduce.a: reduce.o
	ar cr libreduce.a reduce.o

reduce.o: reduce.h

include ../../Makefile.defs

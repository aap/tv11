CFLAGS=-Wall -Wextra -Wno-parentheses
LDFLAGS=-lpthread
tv11: tv11.o cpu.o mem.o eae.o util.o tv.o
tv11.o: tv11.h
cpu.o: tv11.h
mem.o: tv11.h
eae.o: tv11.h
tv.o: tv11.h
util.o: tv11.h

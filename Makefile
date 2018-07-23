CFLAGS=-Wall -Wextra -Wno-parentheses
tv11: tv11.o mem.o eae.o util.o
tv11.o: tv11.h
mem.o: tv11.h
eae.o: tv11.h
util.o: tv11.h

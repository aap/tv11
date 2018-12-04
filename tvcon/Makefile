#CFLAGS=`sdl2-config --cflags`
#LDLIBS=`sdl2-config --libs` -lpthread
CFLAGS=-I/c/MinGW/msys/1.0/include/SDL2
LDLIBS=-L/c/MinGW/msys/1.0/lib -lmingw32 -lws2_32 -lSDL2main -lSDL2 -mwindows
tvcon: tvcon.c
	gcc $(CFLAGS) -o $@ $< $(LDLIBS)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

typedef uint8_t uint8, byte;
typedef uint16_t uint16, word;
typedef uint32_t uint32;

#define WD(hi, lo) ((word)((hi)<<8 | (lo)))
#define W(w) ((word)(w))
#define nil NULL

//#define trace printf
#define trace(...)

int hasinput(int fd);
int dial(char *host, int port);

typedef struct Bus Bus;
typedef struct Busdev Busdev;

struct Busdev
{
	Busdev *next;
	void *dev;
	int (*dati)(Bus *bus, void *dev);
	int (*dato)(Bus *bus, void *dev);
	int (*datob)(Bus *bus, void *dev);
};

struct Bus
{
	Busdev *devs;
	uint32 addr;
	word data;
};

typedef struct Memory Memory;
struct Memory
{
	word *mem;
	uint32 start, end;
};
int dati_mem(Bus *bus, void *dev);
int dato_mem(Bus *bus, void *dev);
int datob_mem(Bus *bus, void *dev);

typedef struct KE11 KE11;
struct KE11
{
	word ac;
	word mq;
	word x;
	byte sc;
	byte sr;
};
int dati_ke11(Bus *bus, void *dev);
int dato_ke11(Bus *bus, void *dev);
int datob_ke11(Bus *bus, void *dev);

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

#define SETMASK(l, r, m) l = ((l)&~(m) | (r)&(m))


//#define trace printf
#define trace(...)

int hasinput(int fd);
int dial(char *host, int port);
void serve(int port, void (*handlecon)(int, void*), void *arg);
void nodelay(int fd);

word sgn(word w);
word sxt(byte b);

typedef struct Bus Bus;
typedef struct Busdev Busdev;

struct Busdev
{
	Busdev *next;
	void *dev;
	int (*dati)(Bus *bus, void *dev);
	int (*dato)(Bus *bus, void *dev);
	int (*datob)(Bus *bus, void *dev);
	void (*reset)(void *dev);
};
void reset_null(void *dev);

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
	byte sc;	/* 6 bits */
	byte sr;
};
int dati_ke11(Bus *bus, void *dev);
int dato_ke11(Bus *bus, void *dev);
int datob_ke11(Bus *bus, void *dev);
void reset_ke11(void *dev);


enum {
	NUMFBUFFERS = 16,
	NUMCONNECTIONS = 32,
};

/* A remove TV connection */
typedef struct TVcon TVcon;
struct TVcon
{
	int fd;
	int dpy;
	int kbd;
};

typedef struct FBuffer FBuffer;
struct FBuffer
{
	word fb[16*1024 - 1];
	word csa;
	word mask;	/* 0 or ~0 for bw flip */
};

/* The whole TV system */
typedef struct TV TV;
struct TV
{
	FBuffer buffers[NUMFBUFFERS];	/* 256 is the theoretical maximum */
	FBuffer *curbuf;
	word creg;
	
	/* Two sections.
	 * Each has 32 outputs that can have one of 16 inputs.
	 * Input 0 is null on both,
	 * that leaves 30 different actual inputs in total. */
	uint8 vswsect[2][32];

	word kms;
	word kma;

	TVcon cons[NUMCONNECTIONS];
};
int dato_tv(Bus *bus, void *dev);
int datob_tv(Bus *bus, void *dev);
int dati_tv(Bus *bus, void *dev);
void reset_tv(void *dev);
void handletvs(TV *tv);
void accepttv(int fd, void *arg);

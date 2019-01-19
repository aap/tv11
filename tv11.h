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

// 11/05 cpu
typedef struct KD11B KD11B;
struct KD11B
{
	word r[16];
	word ba;
	word ir;
	Bus *bus;
	byte psw;
	int traps;
	int be;
	int state;

	struct {
		int (*bg)(void *dev);
		void *dev;
	} br[4];

	word sw;

	/* line clock */
	int lc_int_enab;
	int lc_clock;
	int lc_int;

	/* keyboard */
	int rcd_busy;
	int rcd_rdr_enab;
	int rcd_int_enab;
	int rcd_int;
	int rcd_da;
	byte rcd_b;

	/* printer */
	int xmit_int_enab;
	int xmit_maint;
	int xmit_int;
	int xmit_tbmt;
	byte xmit_b;

	int ttyfd;
};
void run(KD11B *cpu);
void reset(KD11B *cpu);

struct Busdev
{
	Busdev *next;
	void *dev;
	int (*dati)(Bus *bus, void *dev);
	int (*dato)(Bus *bus, void *dev);
	int (*datob)(Bus *bus, void *dev);
	int (*svc)(Bus *bus, void *dev);
	int (*bg)(void *dev);
	void (*reset)(void *dev);
};
void reset_null(void *dev);

struct Bus
{
	Busdev *devs;
	uint32 addr;
	word data;
};
int dati_bus(Bus *bus);
int dato_bus(Bus *bus);
int datob_bus(Bus *bus);

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


/* The 10-11 interface */
typedef struct Ten11 Ten11;
struct Ten11
{
	int cycle;
	int fd;
};


/* Some of these numbers are also hardcoded! */
enum {
	NUMFBUFFERS = 16,
	NUMCONNECTIONS = 32,
	NUMOUTPUTS = 32,
	NUMINPUTS = 16,
	NUMSECTIONS = 2,
};

/* Mask to get buffer number from CREG. */
#define BUFMASK  037

/* A remote TV connection */
typedef struct TVcon TVcon;
struct TVcon
{
	int fd;
	int dpy;	/* output number */
	int kbd;
};

typedef struct FBuffer FBuffer;
struct FBuffer
{
	word fb[16*1024 - 1];
	word csa;
	word mask;	/* 0 or ~0 for bw flip */

	/* list of all outputs that are driven
	 * by this buffer. */
	int osw[NUMOUTPUTS];
	int nosw;
};

/* The whole TV system */
typedef struct TV TV;
struct TV
{
	/* need this to tell 11 from 10 cycles */
	Ten11 *ten11;

	FBuffer buffers[NUMFBUFFERS];	/* 256 is the theoretical maximum */
	/* two different variables for 10 and 11 */
	word creg11;
	word creg10;
	
	/* Two sections.
	 * Each has 32 outputs that can have one of 16 inputs.
	 * Input 0 is null on both,
	 * that leaves 30 different actual inputs in total. */
	uint8 vswsect[NUMSECTIONS][NUMOUTPUTS];

	word kms;
	uint32 kma_hi;
	uint32 kma_lo;
	int km_haskey;
	word km_kbd;
	word km_key;

	TVcon cons[NUMCONNECTIONS];
	int omap[NUMOUTPUTS];	/* map of outputs to connections */
};
void inittv(TV *tv);
void closetv(TV *tv);
int dato_tv(Bus *bus, void *dev);
int datob_tv(Bus *bus, void *dev);
int dati_tv(Bus *bus, void *dev);
int svc_tv(Bus *bus, void *dev);
int bg_tv(void *dev);
void reset_tv(void *dev);
void handletvs(TV *tv);
void accepttv(int fd, void *arg);
void servetv(TV *tv, int port);

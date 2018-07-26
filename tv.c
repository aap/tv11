#include "tv11.h"
#include <sys/poll.h>
#include <pthread.h>

/* maximum:
 * 256 framebuffers
 * 64 keyboards
 * 16 video switch inputs (buffers) per section
 * 32 video switch outputs (displays)
 */

enum {
	TVLO = 060000,
	CSA = 0157776,
	CREG = 0764044,
	KMS = 0764050,
	KMA = 0764052,
	VSW = 0764060,
	ASW = 0770670,
};

enum {
	ALU_SETC = 0,
	ALU_NOR,
	ALU_ANDC,
	ALU_SETZ,
	ALU_NAND,
	ALU_COMP,
	ALU_XOR,
	ALU_ANDCSD,
	ALU_ORCSD,
	ALU_EQV,
	ALU_SAME,
	ALU_AND,
	ALU_ORSCD,
	ALU_SETO,
	ALU_IOR,
	ALU_SET
};

static pthread_mutex_t lock;

static word nullfb[16*1024 - 1];

/* map from input switch to buffer number
 * This is the config at tech square that the TV program expects */
static int dpymap[32] = {
	/* first section */
	-1,	/* null input */
	0, 1, 2, 3, 4, 5, 6, 7, 014, 015, 016, 017, -1, -1, -1,
	/* first section */
	-1,	/* null input */
	010, 011, 012, 013, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

/* List of video switch output and a keyboard */
static struct {
	int dpy;
	int kbd;
} dpykbdlist[32] = {
	{ 0,	0, },	// 809 FAHLMAN, HOLLOWAY, KNIGHT
	{ 01,	014 },	// 820 MINSKY
	{ 02,	021 },	// 824 RICH, DEKLEER
	{ 03,	06 },	// 808 FREILING, ULLMAN
	{ 04,	010 },	// 817 JABARI
	{ 05,	022 },	// 825 Sjoberg
	{ 06,	04 },	// 813 HEWITT
	{ 07,	05 },	// 814 SUSSMAN
	{ 010,	013 },	// 819 GOLDSTEIN
	{ 011,	017 },	// 821A  MARR
	{ 012,	035 },	// 914 COHEN, GOSPER, ETC.
	{ 013,	034 },	// 913 BAISLEY, GREENBLATT
	{ 014,	033 },	// 334 EDWARDS, LEBEL
	{ 015,	030 },	// 925 MOON'S REFRIGERATOR
	{ 016,	031 },	// 902 TAENZER, MASON
	{ 017,	032 },	// 919 Very Small Data losers
	{ 020,	03 },	// 812 YVONNE
	{ 021,	036 },	// 912 9TH FLOOR LOUNGE
	{ 022,	037 },	// 907 CHESS, LISP MACHINES
	{ 023,	01 },	// 810 LAVIN, KUIPERS, MILLER
	{ 024,	02 },	// 919 Very Small Data Bases NORTH (FAR END)
	{ -1,	-1, },
	{ 026,	026 },	// 826 Fredkin
	{ -1,	-1, },
	{ 030,	046 },	// 3rd Floor #6
	{ 031,	031 },	// 815 Horn
	{ -1,	-1, },
	{ 033,	045 },	// 3rd Floor #5
	{ 034,	044 },	// 3rd Floor #4
	{ 035,	043 },	// 3rd Floor #3
	{ 036,	042 },	// 3rd Floor #2
	{ 037,	041 },	// 3rd Floor #1
};

void
vswinfo(TV *tv)
{
	int i;
	for(i = 0; i < 32; i++)
		printf("%o|%o -> %o\n", tv->vswsect[0][i], tv->vswsect[1][i], i);
}

int
dato_tv(Bus *bus, void *dev)
{
	TV *tv = dev;
	int waddr = bus->addr>>1;
	word w, d;

	d = bus->data;
	if(bus->addr >= TVLO && bus->addr < 0160000){
		if(tv->curbuf == nil)
			return 1;
		/* In Buffer */
		if(bus->addr == CSA){
			tv->curbuf->csa = bus->data;
			tv->curbuf->mask = tv->curbuf->csa&010000 ? ~0 : 0;
		}else{
			waddr -= TVLO>>1;
			w = tv->curbuf->fb[waddr];
			switch(tv->creg>>8){
			case ALU_SETC:	w = ~d; break;
			case ALU_NOR:	w = ~(w|d); break;
			case ALU_ANDC:	w = w&~d; break;
			case ALU_SETZ:	w = 0; break;
			case ALU_NAND:	w = ~(w&d); break;
			case ALU_COMP:	w = ~w; break;
			case ALU_XOR:	w ^= d; break;
			case ALU_ANDCSD: w = ~w&d; break;
			case ALU_ORCSD:	w = ~w|d; break;
			case ALU_EQV:	w = ~(w^d); break;
			case ALU_SAME:	break;
			case ALU_AND:	w &= d; break;
			case ALU_ORSCD:	w = w|~d; break;
			case ALU_SETO:	w = ~0; break;
			case ALU_IOR:	w |= d; break;
			case ALU_SET:	w = d; break;
			}
			tv->curbuf->fb[waddr] = w;
		}
		return 0;
	}
	switch(bus->addr){
	case CREG:
		tv->creg = d;
		if((tv->creg & 0377) < NUMFBUFFERS)
			tv->curbuf = &tv->buffers[tv->creg & 0377];
		else
			tv->curbuf = nil;
		return 0;
	case KMS:
		tv->kms = d;
		// TODO: do something
		return 0;
	case KMA:
		tv->kma = d;
		// TODO: do something
		return 0;
	case VSW:{
		int i, o, s;
		i = d&017;	
		o = d>>8 & 037;
		s = d>>13 & 07;
		if(s < 2)
			tv->vswsect[s][o] = i;
		}
		return 0;
	case ASW:
		// TODO, but we don't support audio anyway
		return 0;
	}
	return 1;
}

int
datob_tv(Bus *bus, void *dev)
{
	TV *tv = dev;
	int waddr = bus->addr>>1;
	word w, d, m;
	d = bus->data;
	m = bus->addr&1 ? ~0377 : 0377;
	if(bus->addr >= TVLO && bus->addr < 0160000){
		/* In Buffer */
		if((bus->addr&~1) == CSA){
			if(bus->addr&1)
				SETMASK(tv->curbuf->csa, bus->data, ~0377);
			else
				SETMASK(tv->curbuf->csa, bus->data, 0377);
			tv->curbuf->mask = tv->curbuf->csa&010000 ? ~0 : 0;
		}else{
			waddr -= TVLO>>1;
			w = tv->curbuf->fb[waddr];
			switch(tv->creg>>8){
			case ALU_SETC:	w = ~d; break;
			case ALU_NOR:	w = ~(w|d); break;
			case ALU_ANDC:	w = w&~d; break;
			case ALU_SETZ:	w = 0; break;
			case ALU_NAND:	w = ~(w&d); break;
			case ALU_COMP:	w = ~w; break;
			case ALU_XOR:	w ^= d; break;
			case ALU_ANDCSD: w = ~w&d; break;
			case ALU_ORCSD:	w = ~w|d; break;
			case ALU_EQV:	w = ~(w^d); break;
			case ALU_SAME:	break;
			case ALU_AND:	w &= d; break;
			case ALU_ORSCD:	w = w|~d; break;
			case ALU_SETO:	w = ~0; break;
			case ALU_IOR:	w |= d; break;
			case ALU_SET:	w = d; break;
			}
			SETMASK(tv->curbuf->fb[waddr], w, m);
		}
		return 0;
	}
	switch(bus->addr&~1){
	case CREG:
		SETMASK(tv->creg, d, m);
		if((tv->creg & 0377) < NUMFBUFFERS)
			tv->curbuf = &tv->buffers[tv->creg & 0377];
		else
			tv->curbuf = nil;
		return 0;
	case KMS:
		// TODO: do something
		SETMASK(tv->kms, d, m);
		return 0;
	case KMA:
		// TODO: do something
		SETMASK(tv->kma, d, m);
		return 0;
	case VSW:
	case ASW:
		// don't allow byte writes to this
		// doesn't make that much sense
		return 0;
	}
	return 1;
}

int
dati_tv(Bus *bus, void *dev)
{
	TV *tv = dev;
	int waddr = bus->addr>>1;
	if(bus->addr >= TVLO && bus->addr < 0160000){
		/* In Buffer */
		if((bus->addr&~1) == CSA)
			bus->data = tv->curbuf->csa;
		else{
			waddr -= TVLO>>1;
			bus->data = tv->curbuf->fb[waddr];
		}
		return 0;
	}
	switch(bus->addr){
	case CREG:
		bus->data = tv->creg;
		return 0;
	case KMS:
		bus->data = tv->kms;
		return 0;
	case KMA:
		bus->data = tv->kma;
		return 0;
	case VSW:
	case ASW:
		/* write only */
		bus->data = 0;
		return 0;
	}
	return 1;
}

void
reset_tv(void *dev)
{
	TV *tv = dev;
	memset(tv->buffers, 0, sizeof(tv->buffers));
	tv->curbuf = &tv->buffers[tv->creg & 0377];
	tv->creg = 0;
	memset(tv->vswsect, 0, sizeof(tv->vswsect));
	tv->kms = 0;
	tv->kma = 0;

	int i, j;
	for(i = 0; i < NUMFBUFFERS; i++)
		for(j = 0; j < 576*454/16; j++)
			tv->buffers[i].fb[j] = i+1; //j ^ i;
}

void
inittv(TV *tv)
{
	TVcon *con;
	for(con = tv->cons; con < &tv->cons[NUMCONNECTIONS]; con++){
		con->fd = -1;
		con->dpy = -1;
		con->kbd = -1;
	}
	pthread_mutex_init(&lock, nil);
}

/*
 * The network TV connections
 */

enum {
	MSG_KEYDN = 0,
	MSG_GETFB,
};

#define GUARD	pthread_mutex_lock(&lock)
#define UNGUARD	pthread_mutex_unlock(&lock)

word
b2w(uint8 *b)
{
	return b[0] | b[1]<<8;
}

void
w2b(uint8 *b, word w)
{
	b[0] = w;
	b[1] = w>>8;
}

uint8 largebuf[16*1024*2];

void
dumpbuf(uint8 *b, int n)
{
	while(n--)
		printf("%o ", *b++);
	printf("\n");
}

static void
packfb(TV *tv, int osw, int x, int y, int w, int h)
{
	int stride;
	uint8 *dst;
	int n1, n2;
	word *src1, *src2;
	word bw1, bw2;

	stride = 576/16;
	dst = largebuf;

	/* We mix the outputs of both sections for the final output.
	 * This feature does not seem to be used by the TV system
	 * but it's theoretically capable of doing something like this.
	 * External video input is not supported here of course. */
	n1 = dpymap[tv->vswsect[0][osw]];
	n2 = dpymap[tv->vswsect[1][osw]];
printf("inbuf: %d %d\n", n1, n2);
	src1 = n1 < 0 ? nullfb : &tv->buffers[n1].fb[stride*y + x];
	src2 = n2 < 0 ? nullfb : &tv->buffers[n2].fb[stride*y + x];
	bw1 = n1 < 0 ? 0 : tv->buffers[n1].mask;
	bw2 = n2 < 0 ? 0 : tv->buffers[n2].mask;
	for(y = 0; y < h; y++){
		for(x = 0; x < w; x++){
			/* We mix with an OR */
			w2b(dst, src1[x]^bw1 | src2[x]^bw2);
			dst += 2;
		}
		src1 += stride;
		src2 += stride;
	}
}

/* Tell the other side what we have allocated for them.
 * Not terribly important but they may want to know. */
void
setdpykbd(int fd, int dpy, int kbd)
{
	uint8 buf[2];
	buf[0] = dpy;
	buf[1] = kbd;
	write(fd, buf, 2);
}

/* Get first free connection or -1 */
static int
alloccon(TV *tv)
{
	int i;
	for(i = 0; i < NUMCONNECTIONS; i++)
		if(tv->cons[i].fd < 0 && dpykbdlist[i].dpy >= 0)
			return i;
	return -1;
}

/* NB: this could be GUARDed by the caller */
static void
closecon(TVcon *con)
{
	printf("disconnect display %o\n", con->dpy);
	close(con->fd);
	con->fd = -1;
	con->dpy = -1;
	con->kbd = -1;
}

void
accepttv(int fd, void *arg)
{
	TV *tv = arg;
	int c;
	TVcon *con;

	c = alloccon(tv);
	if(c < 0){
		/* Nothing free */
		setdpykbd(fd, -1, -1);
		close(fd);
		return;
	}
	con = &tv->cons[c];
	GUARD;
	con->dpy = dpykbdlist[c].dpy;
	con->kbd = dpykbdlist[c].kbd;
	con->fd = fd;
	UNGUARD;
	setdpykbd(con->fd, con->dpy, con->kbd);
	printf("connected display %o\n", con->dpy);
}

static void
handlemsg(TV *tv, TVcon *con)
{
	uint8 len;
	uint8 buf[100];
	word w;
	int x, y, width, height;

	if(read(con->fd, &len, 1) != 1){
		closecon(con);
		return;
	}
	assert(len <= 100);

	read(con->fd, buf, len);
	switch(buf[0]){
	case MSG_KEYDN:
		w = WD(buf[2], buf[1]);
		printf("key: %o %o\n", con->kbd, w);
		break;

	case MSG_GETFB:
		x = b2w(buf+1);
		y = b2w(buf+3);
		width = b2w(buf+5);
		height = b2w(buf+7);

		x /= 16;
		width = (width+15) / 16;
		packfb(tv, con->dpy, x, y, width, height);
		write(con->fd, largebuf, width*height*2);
		break;

	default:
		fprintf(stderr, "unknown msg type %d\n", buf[0]);
	}
	
}

static void*
handletv_thread(void *arg)
{
	TV *tv = arg;
	int i;
	int nfds;
	int n;
	struct pollfd fds[NUMCONNECTIONS];
	int conmap[NUMCONNECTIONS];
	TVcon *con;

	for(;;){
		nfds = 0;
		for(i = 0; i < NUMCONNECTIONS; i++){
			if(tv->cons[i].fd < 0)
				continue;
			fds[nfds].fd = tv->cons[i].fd;
			fds[nfds].events = POLLIN;
			conmap[nfds] = i;
			nfds++;
		}
		if(nfds == 0)
			/* TODO: sleep until there's fds */
			continue;

		/* We need a timeout here so poll can see
		 * when we open a new connection */
		n = poll(fds, nfds, 200);
		if(n < 0){
			perror("error: poll");
			/* Is this the correct thing to do? */
			return nil;
		}	
		/* timeout */
		if(n == 0)
			continue;

		for(i = 0; i < nfds; i++){
			if(fds[i].revents == 0)
				continue;
			con = &tv->cons[conmap[i]];
			if(fds[i].revents != POLLIN){
				/* When does this happen? */
				GUARD;
				closecon(con);
				UNGUARD;
				continue;
			}
			GUARD;
			/* Check whether it's still the connection
			 * we thought we're using. */
			/* TODO: make this check more sound */
			if(fds[i].fd == con->fd){
				/* All good, talk to the display */
				printf("handling con %d\n", conmap[i]);
				handlemsg(tv, con);
			}
			UNGUARD;
		}
	}
	return nil;
}

void
handletvs(TV *tv)
{
	pthread_t th;
	pthread_create(&th, nil, handletv_thread, tv);
}

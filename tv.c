#include "tv11.h"

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
		if(bus->addr == CSA)
			tv->curbuf->csa = bus->data;
		else{
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
	memset(tv, 0, sizeof(TV));
	tv->curbuf = &tv->buffers[tv->creg & 0377];

	int i, j;
	for(i = 0; i < NUMFBUFFERS; i++)
		for(j = 0; j < 576*454/16; j++)
			tv->buffers[i].fb[j] = i+1; //j ^ i;
}


enum {
	MSG_KEYDN = 0,
	MSG_GETFB,
};

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

	stride = 576/16;
	dst = largebuf;

	/* We mix the outputs of both sections for this output.
	 * This feature does not seem to be used by the TV system
	 * but it's theoretically capable of doing something like this.
	 * External video input is not supported here of course. */
	n1 = dpymap[tv->vswsect[0][osw]];
	n2 = dpymap[tv->vswsect[1][osw]];
printf("inbuf: %d %d\n", n1, n2);
	src1 = n1 < 0 ? nullfb : &tv->buffers[n1].fb[stride*y + x];
	src2 = n2 < 0 ? nullfb : &tv->buffers[n2].fb[stride*y + x];
	for(y = 0; y < h; y++){
		for(x = 0; x < w; x++){
			/* We mix with an OR */
			w2b(dst, src1[x]|src2[x]);
			dst += 2;
		}
		src1 += stride;
		src2 += stride;
	}
}

void
srvtv(int fd, void *arg)
{
	TV *tv = arg;
	uint8 len;
	uint8 buf[100];
	word w;
	int x, y, width, height;

	printf("connected\n");
	while(read(fd, &len, 1) == 1){
		assert(len <= 100);
		read(fd, buf, len);
		switch(buf[0]){
		case MSG_KEYDN:
			w = WD(buf[2], buf[1]);
			printf("key: %o\n", w);
			break;

		case MSG_GETFB:
			x = b2w(buf+1);
			y = b2w(buf+3);
			width = b2w(buf+5);
			height = b2w(buf+7);
			printf("%d %d %d %d\n", x, y, width, height);

			x /= 16;
			width = (width+15) / 16;
			packfb(tv, 0, x, y, width, height);
			printf("write %d\n", width*height*2);
			write(fd, largebuf, width*height*2);
			break;

		default:
			fprintf(stderr, "unknown msg type %d\n", buf[0]);
		}
	}
	printf("disconnected\n");
	close(fd);
}

#include "tv11.h"
#include "args.h"

// in words
#define MEMSIZE (12*1024)

uint16 memory[MEMSIZE];

void
busadddev(Bus *bus, Busdev *dev)
{
	Busdev **bp;
	for(bp = &bus->devs; *bp; bp = &(*bp)->next)
		;
	*bp = dev;
	dev->next = nil;
}

int
dati_bus(Bus *bus)
{
	Busdev *bd;
	for(bd = bus->devs; bd; bd = bd->next)
		if(bd->dati(bus, bd->dev) == 0)
			return 0;
	return 1;
}

int
dato_bus(Bus *bus)
{
	Busdev *bd;
	for(bd = bus->devs; bd; bd = bd->next)
		if(bd->dato(bus, bd->dev) == 0)
			return 0;
	return 1;
}

int
datob_bus(Bus *bus)
{
	Busdev *bd;
	for(bd = bus->devs; bd; bd = bd->next)
		if(bd->datob(bus, bd->dev) == 0)
			return 0;
	return 1;
}

int dati_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 1; }
int dato_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 1; }
int datob_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 1; }
void reset_null(void *dev) { (void)dev; }
int svc_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 0; }

word
sgn(word w)
{
	return (w>>15)&1;
}

word
sxt(byte b)
{
	return (word)(int8_t)b;
}

int
svc_ten11(Bus *bus, void *dev)
{
	Ten11 *ten11 = dev;
	uint8 buf[6], len;
	int n;
	uint32 a;
	word d;

	/* KLUDGE */
//	if((ten11->cpu->psw>>5 & 7) > 0)
//		return 0;

	if(ten11->fd < 0)
		return 0;

	memset(buf, 0, sizeof(buf));
	if(!hasinput(ten11->fd))
		return 0;
	n = read(ten11->fd, &len, 1);
	if(n != 1){
		fprintf(stderr, "fd closed, exiting\n");
		exit(0);
	}
	if(len > 6){
		fprintf(stderr, "unibus botch, exiting\n");
		exit(0);
	}
	read(ten11->fd, buf, len);

	a = buf[1] | buf[2]<<8 | buf[3]<<16;
	d = buf[4] | buf[5]<<8;

	ten11->cycle = 1;
	switch(buf[0]){
	case 1:		/* write */
		bus->addr = a;
		bus->data = d;
		if(a&1) goto be;
		if(dato_bus(bus)) goto be;
//fprintf(stderr, "TEN11 write: %06o %06o\n", bus->addr, bus->data);
		buf[0] = 1;
		buf[1] = 3;
		write(ten11->fd, buf, 2);
		break;
	case 2:		/* read */
		bus->addr = a;
		if(a&1) goto be;
		if(dati_bus(bus)) goto be;
//fprintf(stderr, "TEN11 read: %06o %06o\n", bus->addr, bus->data);
		buf[0] = 3;
		buf[1] = 3;
		buf[2] = bus->data;
		buf[3] = bus->data>>8;
		write(ten11->fd, buf, 4);
		break;
	default:
		fprintf(stderr, "unknown ten11 message type %d\n", buf[0]);
		break;
	}
	ten11->cycle = 0;
	return 0;
be:
fprintf(stderr, "TEN11 bus error %06o\n", bus->addr);
	buf[0] = 1;
	buf[1] = 4;
	write(ten11->fd, buf, 2);
	ten11->cycle = 0;
	return 0;
}


void
loadmem(char *filename)
{
	FILE *f;
	char line[100], *s;
	word w;

	w = 0;
	f = fopen(filename, "r");
	if(f == nil)
		return;
	while(s = fgets(line, 100, f)){
		while(isspace(*s)) s++;
		if(*s == ':')
			w = strtol(s+1, nil, 8)>>1;
		if(*s >= '0' && *s <= '7')
			memory[w++] = strtol(s, nil, 8);
	}
	fclose(f);
}

int
loadpt(char *filename)
{
	FILE *f;
	byte hi, lo, s;
	word n, a, w;

	f = fopen(filename, "rb");
	if(f == nil){
		printf("can't open %s\n", filename);
		return 1;
	}

	for(;;){
		s = 0;
		if(fread(&lo, 1, 1, f) < 1) break;
		if(lo != 1)
			continue;
		s += lo;
		if(fread(&hi, 1, 1, f) < 1) break;
		s += hi;
		w = WD(hi, lo);
		if(w != 1)
			continue;
		if(fread(&lo, 1, 1, f) < 1) goto botch;
		s += lo;
		if(fread(&hi, 1, 1, f) < 1) goto botch;
		s += hi;
		n = WD(hi, lo);
		if(fread(&lo, 1, 1, f) < 1) goto botch;
		s += lo;
		if(fread(&hi, 1, 1, f) < 1) goto botch;
		s += hi;
		a = WD(hi, lo);
		if(a == 1)
			break;
		if(n == 0)
			continue;
		n -= 6;
		/* odd number of bytes is probably allowed but we can't do it */
		if(n&1)
			goto botch;
		n >>= 1;
		a >>= 1;
		while(n--){
			if(fread(&lo, 1, 1, f) < 1) goto botch;
			s += lo;
			if(fread(&hi, 1, 1, f) < 1) goto botch;
			s += hi;
			w = WD(hi, lo);
			memory[a++] = w;
		}
		if(fread(&lo, 1, 1, f) < 1) goto botch;
		s += lo;
		if(s)
			goto botch;
	}
	fclose(f);
	return 0;
botch:
	printf("paper tape botch\n");
	fclose(f);
	return 1;
}

void
dumpmem(word start, word end)
{
	start >>= 1;
	end >>= 1;
	for(; start != end; start++)
		printf("%06o: %06o\n", start<<1, memory[start]);
}

KD11B cpu;
Bus bus;
Memory memdev = { memory, 0, MEMSIZE };
Busdev membusdev = { nil, &memdev, dati_mem, dato_mem, datob_mem, svc_null, nil, reset_null };
KE11 ke11;
Busdev kebusdev = { nil, &ke11, dati_ke11, dato_ke11, datob_ke11, svc_null, nil, reset_ke11 };
TV tv;
Busdev tvbusdev = { nil, &tv, dati_tv, dato_tv, datob_tv, svc_tv, bg_tv, reset_tv };
Ten11 ten11;
Busdev ten11busdev = { nil, &ten11, dati_null, dato_null, datob_null, svc_ten11, nil, reset_null };

void
setunibus(uint8 n)
{
	uint8 len;
	uint8 buf[3];
	buf[0] = 2;
	buf[1] = 0;
	buf[2] = n;
	write(ten11.fd, buf, 3);

	if(read(ten11.fd, &len, 1) != 1)
		goto err;
	if(len != 1) goto err;
	if(read(ten11.fd, buf, len) != len) goto err;
	if(buf[0] != 3){
		fprintf(stderr, "couldn't attach as unibus %d, exiting\n", n);
		exit(0);
	}
	printf("unibus %d OK\n", n);
	return;
err:
	fprintf(stderr, "unibus protocol botch, exiting\n");
	exit(0);
}

char *argv0;

void
usage(void)
{
	fprintf(stderr, "usage: %s [-p port] [-l listenport] server\n", argv0);
	fprintf(stderr, "\tserver: host running pdp-10\n");
	fprintf(stderr, "\t-p: pdp-10 port; default 1110\n");
	fprintf(stderr, "\t-l: tv11 listen port; default 11100\n");
	exit(0);
}

int
main(int argc, char *argv[])
{
	int port;
	int lport;
	char *host;

	memset(&cpu, 0, sizeof(KD11B));
	memset(&bus, 0, sizeof(Bus));
	cpu.bus = &bus;
	busadddev(&bus, &membusdev);
	busadddev(&bus, &tvbusdev);
	busadddev(&bus, &kebusdev);
	busadddev(&bus, &ten11busdev);

	port = 1110;
	lport = 11100;
	ARGBEGIN{
	case 'p':
		port = atoi(EARGF(usage()));
		break;
	case 'l':
		lport = atoi(EARGF(usage()));
		break;
	}ARGEND;

	if(argc < 1)
		usage();

	host = argv[0];

	printf("connecting to PDP-10\n");
//	ten11.fd = -1;
	ten11.cycle = 0;
	ten11.fd = dial(host, port);
	if(ten11.fd < 0){
		printf("can't connect to PDP-10\n");
//		return 1;
	}else{
		nodelay(ten11.fd);
		setunibus(0);
		printf("connected to PDP-10\n");
	}

	loadmem("mem.txt");

//	if(loadpt("maindec/MAINDEC-11-D0NA-PB.ptap"))
//	if(loadpt("maindec/MAINDEC-11-D2AA-PB.ptap"))
//		return 0;

//	cpu.r[7] = 0200;
//	cpu.sw = 0104000;

	//void eaetest(KE11 *ke);
	//eaetest(&ke11);

	inittv(&tv);
	tv.ten11 = &ten11;

	reset(&cpu);

	/* open a tty if it exists */
	cpu.ttyfd = open("/tmp/tty", O_RDWR);
	printf("tty connected to %d\n", cpu.ttyfd);

	/* start the two threads for listening for
	 * and handling TV display connections */
	handletvs(&tv);
	servetv(&tv, lport);

	//void tvtest(TV *tv, Bus *bus);
	//tvtest(&tv, &bus);

	/* wait until we get some data from the 10 */
	cpu.r[7] = 0;
	memory[0] = 0777;
	run(&cpu);

	closetv(&tv);

	return 0;
}

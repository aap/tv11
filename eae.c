#include "tv11.h"

enum {
	SR_C = 1,
	SR_AC_EQ_MQ15 = 2,
	SR_AC_MQ_0 = 4,
	SR_MQ_0 = 010,
	SR_AC_0 = 020,
	SR_AC_1 = 040,
	SR_NEG = 0100,
	SR_OV = 0200
};

/*
 777300	divide	X
 777302	none	AC
 777304 none	MQ
 777306	mult	X
 777310	none	SC
 777311	none	SR
 777312 norm	SC
 777314 lsh	SC
 777315 ash	SC
*/

int
dato_ke11(Bus *bus, void *dev)
{
	KE11 *ke = dev;
	if(bus->addr >= 0777300 && bus->addr < 0777316){
		printf("EAE DATO %o %o\n", bus->addr, bus->data);
		return 0;
	}
	return 1;
}

int
datob_ke11(Bus *bus, void *dev)
{
	KE11 *ke = dev;
	if(bus->addr >= 0777300 && bus->addr < 0777316){
		printf("EAE DATOB %o %o\n", bus->addr, bus->data);
		return 0;
	}
	return 1;
}

int
dati_ke11(Bus *bus, void *dev)
{
	KE11 *ke = dev;
	if(bus->addr >= 0777300 && bus->addr < 0777316){
		printf("EAE DATI %o\n", bus->addr);
		bus->data = 0;
		return 0;
	}
	return 1;
}


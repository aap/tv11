#include "tv11.h"

enum {
	PSW_PR = 0340,
	PSW_T = 020,
	PSW_N = 010,
	PSW_Z = 004,
	PSW_V = 002,
	PSW_C = 001,
};

enum {
	TRAP_STACK = 1,
	TRAP_PWR = 2,	// can't happen
	TRAP_BR7 = 4,
	TRAP_BR6 = 010,
	TRAP_CLK = 020,
	TRAP_BR5 = 040,
	TRAP_BR4 = 0100,
	TRAP_RX  = 0200,
	TRAP_TX  = 0400,
	TRAP_CSTOP = 01000	// can't happen?
};

#define ISSET(f) ((cpu->psw&(f)) != 0)

enum {
	STATE_HALTED = 0,
	STATE_RUNNING,
	STATE_WAITING
};


#define CLOCKFREQ (1000000000/60)

static struct timespec oldtime, newtime;

static void
initclock(void)
{
	clock_gettime(CLOCK_REALTIME, &newtime);
	oldtime = newtime;
}

static void
handleclock(KD11B *cpu)
{
	struct timespec diff;
	clock_gettime(CLOCK_REALTIME, &newtime);
	diff.tv_sec = newtime.tv_sec - oldtime.tv_sec;
	diff.tv_nsec = newtime.tv_nsec - oldtime.tv_nsec;
	if(diff.tv_nsec < 0){
		diff.tv_nsec += 1000000000;
		diff.tv_sec -= 1;
	}
	if(diff.tv_nsec >= CLOCKFREQ){
		cpu->lc_clock = 1;
		cpu->lc_int = 1;
		oldtime.tv_nsec += CLOCKFREQ;
		if(oldtime.tv_nsec >= 1000000000){
			oldtime.tv_nsec -= 1000000000;
			oldtime.tv_sec += 1;
		}
	}
}

static uint32
ubxt(word a)
{
	return (a&0160000)==0160000 ? a|0600000 : a;
}

void
tracestate(KD11B *cpu)
{
	(void)cpu;
	trace(" R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\n"
		" 10 %06o 11 %06o 12 %06o 13 %06o 14 %06o 15 %06o 16 %06o 17 %06o\n"
		" BA %06o IR %06o PSW %03o\n"
		,
		cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3],
		cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7],
		cpu->r[8], cpu->r[9], cpu->r[10], cpu->r[11],
		cpu->r[12], cpu->r[13], cpu->r[14], cpu->r[15],
		cpu->ba, cpu->ir, cpu->psw);
}

void
printstate(KD11B *cpu)
{
	(void)cpu;
	printf(" R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\n"
		" 10 %06o 11 %06o 12 %06o 13 %06o 14 %06o 15 %06o 16 %06o 17 %06o\n"
		" BA %06o IR %06o PSW %03o\n"
		,
		cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3],
		cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7],
		cpu->r[8], cpu->r[9], cpu->r[10], cpu->r[11],
		cpu->r[12], cpu->r[13], cpu->r[14], cpu->r[15],
		cpu->ba, cpu->ir, cpu->psw);
}

void
reset(KD11B *cpu)
{
	Busdev *bd;

	cpu->rcd_busy = 0;
	cpu->rcd_rdr_enab = 0;
	cpu->rcd_int_enab = 0;
	cpu->rcd_int = 0;
	cpu->rcd_da = 0;
	cpu->rcd_b = 0;

	cpu->xmit_int_enab = 0;
	cpu->xmit_maint = 0;
	cpu->xmit_int = 1;
	cpu->xmit_tbmt = 1;
	cpu->xmit_b = 0;

	cpu->lc_int_enab = 0;
	// TODO: 1?
	cpu->lc_clock = 0;
	cpu->lc_int = 0;

	cpu->traps = 0;

	for(bd = cpu->bus->devs; bd; bd = bd->next)
		bd->reset(bd->dev);
}

int
dati(KD11B *cpu, int b)
{
trace("dati %06o: ", cpu->ba);
	/* allow odd addresses for bytes and registers */
	int alodd = b || cpu->ba >= 0177700 && cpu->ba < 0177720;

	if(!alodd && cpu->ba&1)
		goto be;

	/* internal registers */
	if((cpu->ba&0177400) == 0177400){
		if((cpu->ba&0360) == 0300){
			cpu->bus->data = cpu->r[cpu->ba&017];
			goto ok;
		}
		switch(cpu->ba&0377){
		/* Line clock */
		case 0146:
			cpu->bus->data = cpu->lc_int_enab<<6 |
				cpu->lc_clock<<7;
			goto ok;
		/* Receive */
		case 0160:
			cpu->bus->data = cpu->rcd_int_enab<<6 |
				cpu->rcd_int<<7 |
				cpu->rcd_busy<<1;
			goto ok;
		case 0162:
			cpu->bus->data = cpu->rcd_b;
			cpu->rcd_b = 0;
			cpu->rcd_da = 0;
			cpu->rcd_int = 0;
			goto ok;
		/* Transmit */
		case 0164:
			cpu->bus->data = cpu->xmit_maint<<2 |
				cpu->xmit_int_enab<<6 |
				cpu->xmit_int<<7;
			goto ok;
		case 0166:
			/* write only */
			cpu->bus->data = 0;
			goto ok;
		case 0170: case 0171:
			cpu->bus->data = cpu->sw;
			goto ok;
		case 0376: case 0377:
			cpu->bus->data = cpu->psw;
			goto ok;

		/* respond but don't return real data */
		case 0147:
		case 0161:
		case 0163:
		case 0165:
		case 0167:
			cpu->bus->data = 0;
			goto ok;
		}
	}

	cpu->bus->addr = ubxt(cpu->ba)&~1;
	if(dati_bus(cpu->bus))
		goto be;
ok:
	trace("%06o\n", cpu->bus->data);
	cpu->be = 0;
	return 0;
be:
	trace("BE\n");
	cpu->be++;
	return 1;
}

int
dato(KD11B *cpu, int b)
{
trace("dato %06o %06o %d\n", cpu->ba, cpu->bus->data, b);
	/* allow odd addresses for bytes and registers */
	int alodd = b || cpu->ba >= 0177700 && cpu->ba < 0177720;

	if(!alodd && cpu->ba&1)
		goto be;

	cpu->be = 0;

	/* internal registers */
	if((cpu->ba&0177400) == 0177400){
		if((cpu->ba&0360) == 0300){
			/* no idea if byte access even makes sense here */
			cpu->r[cpu->ba&017] = cpu->bus->data;
			goto ok;
		}
		switch(cpu->ba&0377){
		/* Line clock */
		case 0146:
			cpu->lc_int_enab = cpu->bus->data>>6 & 1;
			if((cpu->bus->data & 0200) == 0){
				cpu->lc_clock = 0;
				cpu->lc_int = 0;
			}
			goto ok;
		/* Receive */
		case 0160:
			// TODO: RDR ENAB
			cpu->rcd_rdr_enab = cpu->bus->data & 1;
			if(!cpu->rcd_int_enab && cpu->bus->data&0100 && cpu->rcd_da)
				cpu->rcd_int = 1;
			cpu->rcd_int_enab = cpu->bus->data>>6 & 1;
			goto ok;
		case 0162:
			/* read only */
			goto ok;
		/* Transmit */
		case 0164:
			// TODO: MAINT
			cpu->xmit_maint = cpu->bus->data>>2 & 1;
			if(!cpu->xmit_int_enab && cpu->bus->data&0100 && cpu->xmit_tbmt)
				cpu->xmit_int = 1;
			cpu->xmit_int_enab = cpu->bus->data>>6 & 1;
			goto ok;
		case 0166:
			cpu->xmit_b = cpu->bus->data;
			cpu->xmit_tbmt = 0;
			cpu->xmit_int = 0;
			goto ok;
		case 0170: case 0171:
			/* can't write switches */
			goto ok;
		case 0376: case 0377:
			/* writes 0 for the odd byte.
			   I think this is correct. */
			cpu->psw = cpu->bus->data;
			goto ok;

		/* respond but don't do anything */
		case 0147:
		case 0161:
		case 0163:
		case 0165:
		case 0167:
			goto ok;
		}
	}

	if(b){
		cpu->bus->addr = ubxt(cpu->ba);
		if(datob_bus(cpu->bus))
			goto be;
	}else{
		cpu->bus->addr = ubxt(cpu->ba)&~1;
		if(dato_bus(cpu->bus))
			goto be;
	}
ok:
	cpu->be = 0;
	return 0;
be:
	cpu->be++;
	return 1;
}

static void
svc(KD11B *cpu, Bus *bus)
{
	int l;
	Busdev *bd;
	static int brtraps[4] = { TRAP_BR4, TRAP_BR5, TRAP_BR6, TRAP_BR7 };
	for(l = 0; l < 4; l++){
		cpu->br[l].bg = nil;
		cpu->br[l].dev = nil;
	}
	cpu->traps &= ~(TRAP_BR4|TRAP_BR5|TRAP_BR6|TRAP_BR7);
	for(bd = bus->devs; bd; bd = bd->next){
		l = bd->svc(bus, bd->dev);
		if(l >= 4 && l <= 7 && cpu->br[l-4].bg == nil){
			cpu->br[l-4].bg = bd->bg;
			cpu->br[l-4].dev = bd->dev;
			cpu->traps |= brtraps[l-4];
		}
	}
}

// Read source operand of double operand instructions into r[8]
static int
readsrc(KD11B *cpu, int b)
{
	int s;
	int ai;

	s = (cpu->ir>>6) & 7;
	ai = 1 + (!b || s==6 || s==7);
	switch((cpu->ir>>9) & 7){
	case 0:		// REG
		cpu->r[8] = cpu->r[s];
		if(b)
			cpu->r[8] = sxt(cpu->r[8]);
		return 0;
	case 1:		// REG deferred
		cpu->ba = cpu->r[s];
		if(dati(cpu, b)) return 1;
	s12:
		cpu->r[8] = cpu->bus->data;
		if(b){
			if(cpu->ba&1)
				cpu->r[8] = cpu->r[8]>>8;
			cpu->r[8] = sxt(cpu->r[8]);
		}
		return 0;
	case 2:		// INC
		cpu->ba = cpu->r[s];
		cpu->r[s] += ai;
		if(dati(cpu, b)) return 1;
		goto s12;
	case 3:		// INC deferred
		cpu->ba = cpu->r[s];
		cpu->r[s] += 2;
		if(dati(cpu, 0)) return 1;
	s34:
		cpu->ba = cpu->bus->data;
		if(dati(cpu, b)) return 1;
		goto s12;
	case 4:		// DEC
		cpu->ba = cpu->r[s]-ai;
		if(s == 6 && (cpu->ba&~0377) == 0)
			cpu->traps |= TRAP_STACK;
		cpu->r[s] = cpu->ba;
		if(dati(cpu, b)) return 1;
		goto s12;
	case 5:		// DEC deferred
		cpu->ba = cpu->r[s]-2;
		if(s == 6 && (cpu->ba&~0377) == 0)
			cpu->traps |= TRAP_STACK;
		cpu->r[s] = cpu->ba;
		if(dati(cpu, 0)) return 1;
		goto s34;
	case 6:		// INDEX
		cpu->ba = cpu->r[7];
		cpu->r[7] += 2;
		if(dati(cpu, 0)) return 1;
		cpu->ba = cpu->bus->data + cpu->r[s];
		if(dati(cpu, b)) return 1;
		goto s12;
	case 7:		// INDEX deferred
		cpu->ba = cpu->r[7];
		cpu->r[7] += 2;
		if(dati(cpu, 0)) return 1;
		cpu->ba = cpu->bus->data + cpu->r[s];
		if(dati(cpu, b)) return 1;
		goto s34;
	}
	// cannot happen
	return 0;
}

// Read dest operand into r[9]
// TODO: use same as readsrc?
static int
readdest(KD11B *cpu, int b)
{
	int d;
	int ai;

	d = cpu->ir & 7;
	ai = 1 + (!b || d==6 || d==7);
	switch((cpu->ir>>3) & 7){
	case 0:		// REG
		cpu->r[9] = cpu->r[d];
		if(b)
			cpu->r[9] = sxt(cpu->r[9]);
		return 0;
	case 1:		// REG deferred
		cpu->ba = cpu->r[d];
		if(dati(cpu, b)) return 1;
	d12:
		cpu->r[9] = cpu->bus->data;
		if(b){
			if(cpu->ba&1)
				cpu->r[9] = cpu->r[9]>>8;
			cpu->r[9] = sxt(cpu->r[9]);
		}
		return 0;
	case 2:		// INC
		cpu->ba = cpu->r[d];
		cpu->r[d] += ai;
		if(dati(cpu, b)) return 1;
		goto d12;
	case 3:		// INC deferred
		cpu->ba = cpu->r[d];
		cpu->r[d] += 2;
		if(dati(cpu, 0)) return 1;
	// deferred
	d34:
		cpu->ba = cpu->bus->data;
		if(dati(cpu, b)) return 1;
		goto d12;
	case 4:		// DEC
		cpu->ba = cpu->r[d]-ai;
		if(d == 6 && (cpu->ba&~0377) == 0)
			cpu->traps |= TRAP_STACK;
		cpu->r[d] = cpu->ba;
		if(dati(cpu, b)) return 1;
		goto d12;
	case 5:		// DEC deferred
		cpu->ba = cpu->r[d]-2;
		if(d == 6 && (cpu->ba&~0377) == 0)
			cpu->traps |= TRAP_STACK;
		cpu->r[d] = cpu->ba;
		if(dati(cpu, 0)) return 1;
		goto d34;
	case 6:		// INDEX
		cpu->ba = cpu->r[7];
		cpu->r[7] += 2;
		if(dati(cpu, 0)) return 1;
		cpu->ba = cpu->bus->data + cpu->r[d];
		if(dati(cpu, b)) return 1;
		goto d12;
	case 7:		// INDEX deferred
		cpu->ba = cpu->r[7];
		cpu->r[7] += 2;
		if(dati(cpu, 0)) return 1;
		cpu->ba = cpu->bus->data + cpu->r[d];
		if(dati(cpu, b)) return 1;
		goto d34;
	}
	// cannot happen
	return 0;
}

static int
writedest(KD11B *cpu, word v, int b)
{
	int d;

	d = cpu->ir & 7;
	switch((cpu->ir>>3) & 7){
	case 0:
		// MOVB sign extends, everything else doesn't
		if(b){
			if((cpu->ir&0070000) == 0010000)
				cpu->r[d] = sxt(v);
			else
				cpu->r[d] = cpu->r[d]&0177400 | v&0377;
		}else
			cpu->r[d] = v;
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		if(b & cpu->ba&1)
			v <<= 8;
		cpu->bus->data = v;
		if(dato(cpu, b)) return 1;
		return 0;
	}
	return 0;
}

static void
setnz(KD11B *cpu, word w)
{
	cpu->psw &= ~(PSW_N|PSW_Z);
	if(w & 0100000) cpu->psw |= PSW_N;
	if(w == 0) cpu->psw |= PSW_Z;
}

void
step(KD11B *cpu)
{
	int by;
	int br;
	int b;
	int c;
	word mask, sign;
	int inhov;
	byte oldpsw;

//	trace("fetch from %06o\n", cpu->r[7]);
//	printstate(cpu);

	oldpsw = cpu->psw;

	cpu->ba = cpu->r[7];
	if(dati(cpu, 0)) goto be;
	cpu->r[7] += 2;	/* don't increment on bus error! */
	cpu->ir = cpu->bus->data;

	by = !!(cpu->ir&0100000);
	br = sxt(cpu->ir)<<1;
	if(by){
		mask = 0377;
		sign = 0200;
	}else{
		mask = 0177777;
		sign = 0100000;
	}

	/* Double operand */
	switch(cpu->ir & 0070000){
	case 0010000:	/* MOV */
		trace("%06o MOV%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readsrc(cpu, by)) goto be;
		if(readdest(cpu, by)) goto be;
		cpu->psw &= ~PSW_V;
		setnz(cpu, cpu->r[8]);
		if(writedest(cpu, cpu->r[8], by)) goto be;
		goto service;
	case 0020000:	/* CMP */
		trace("%06o CMP%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readsrc(cpu, by)) goto be;
		if(readdest(cpu, by)) goto be;
		cpu->psw &= ~(PSW_V|PSW_C);
		b = cpu->r[8] + (word)(~cpu->r[9]) + 1;
		if((b & 0200000) == 0)
			cpu->psw |= PSW_C;
		if(by) b = sxt(b);
		if(sgn(cpu->r[8]) != sgn(cpu->r[9]) &&
		   sgn(cpu->r[9]) == sgn(b))
			cpu->psw |= PSW_V;
		setnz(cpu, b);
		goto service;
	case 0030000:	/* BIT */
		trace("%06o BIT%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readsrc(cpu, by)) goto be;
		if(readdest(cpu, by)) goto be;
		b = cpu->r[9] & cpu->r[8];
		cpu->psw &= ~PSW_V;
		setnz(cpu, b);
		goto service;
	case 0040000:	/* BIC */
		trace("%06o BIC%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readsrc(cpu, by)) goto be;
		if(readdest(cpu, by)) goto be;
		b = cpu->r[9] & ~cpu->r[8];
		cpu->psw &= ~PSW_V;
		setnz(cpu, b);
		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0050000:	/* BIS */
		trace("%06o BIS%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readsrc(cpu, by)) goto be;
		if(readdest(cpu, by)) goto be;
		b = cpu->r[9] | cpu->r[8];
		cpu->psw &= ~PSW_V;
		setnz(cpu, b);
		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0060000:	/* ADD/SUB */
		trace("%06o %s\n", cpu->r[7]-2, by ? "SUB" : "ADD");
		if(readsrc(cpu, 0)) goto be;
		if(readdest(cpu, 0)) goto be;
		cpu->psw &= ~(PSW_V|PSW_C);
		if(cpu->ir&0100000){
			// SUB
			b = cpu->r[9] + (word)(~cpu->r[8]) + 1;
			if((b & 0200000) == 0)
				cpu->psw |= PSW_C;
			if(sgn(cpu->r[8]) != sgn(cpu->r[9]) &&
			   sgn(cpu->r[9]) != sgn(b))
				cpu->psw |= PSW_V;
		}else{
			// ADD
			b = cpu->r[8] + cpu->r[9];
			if(b & 0200000)
				cpu->psw |= PSW_C;
			if(sgn(cpu->r[8]) == sgn(cpu->r[9]) &&
			   sgn(cpu->r[9]) != sgn(b))
				cpu->psw |= PSW_V;
		}
		setnz(cpu, b);
		if(writedest(cpu, b, 0)) goto be;
		goto service;

	/* Reserved instructions */
	case 0070000:
		goto ri;
	}

	/* Single Operand */
	switch(cpu->ir & 0007700){
	case 0005000:	/* CLR */
		trace("%06o CLR%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		cpu->psw &= ~(PSW_V|PSW_C);
		b = 0;
		setnz(cpu, b);

		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0005100:	/* COM */
		trace("%06o COM%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		cpu->psw &= ~PSW_V;
		cpu->psw |= PSW_C;
		b = W(~cpu->r[8]);
		setnz(cpu, b);

		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0005200:	/* INC */
		trace("%06o INC%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		cpu->psw &= ~PSW_V;
		b = W(cpu->r[8] + 1);
		if(by) b = sxt(b);
		if(!sgn(cpu->r[8]) && sgn(b))
			cpu->psw |= PSW_V;
		setnz(cpu, b);

		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0005300:	/* DEC */
		trace("%06o DEC%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		cpu->psw &= ~PSW_V;
		b = W(cpu->r[8] + 0177777);
		if(by) b = sxt(b);
		if(sgn(cpu->r[8]) && !sgn(b))
			cpu->psw |= PSW_V;
		setnz(cpu, b);

		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0005400:	/* NEG */
		trace("%06o NEG%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		cpu->psw &= ~(PSW_V|PSW_C);
		b = W(~cpu->r[8] + 1);
		if(by) b = sxt(b);
		if(b) cpu->psw |= PSW_C;
		if(sgn(b) && sgn(cpu->r[8])) cpu->psw |= PSW_V;
		setnz(cpu, b);

		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0005500:	/* ADC */
		trace("%06o ADC%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		c = !!(cpu->psw&PSW_C);
		cpu->psw &= ~(PSW_V|PSW_C);
		b = cpu->r[8] + c;
		if(b & 0200000)
			cpu->psw |= PSW_C;
		if(by) b = sxt(b);
		if(!sgn(cpu->r[8]) && sgn(b))
			cpu->psw |= PSW_V;
		setnz(cpu, b);

		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0005600:	/* SBC */
		trace("%06o SBC%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		c = cpu->psw&PSW_C ? 0177777 : 0;
		cpu->psw &= ~(PSW_V|PSW_C);
		b = W(cpu->r[8] + c);
		// DOCU
		// This is NOT what the documentation says
		// but it passes diagnostics
		if(c && cpu->r[8] == 0)
			cpu->psw |= PSW_C;
		if(by) b = sxt(b);
		if(sgn(cpu->r[8]) && !sgn(b))
			cpu->psw |= PSW_V;
		setnz(cpu, b);

		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0005700:	/* TST */
		trace("%06o TST%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		cpu->psw &= ~(PSW_V|PSW_C);
		setnz(cpu, cpu->r[8]);

		goto service;

	case 0006000:	/* ROR */
		trace("%06o ROR%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		c = ISSET(PSW_C);
		cpu->psw &= ~(PSW_V|PSW_C);
		b = (cpu->r[8]&mask) >> 1;
		if(c) b |= sign;
		if(cpu->r[8] & 1)
			cpu->psw |= PSW_C;
		if(by) b = sxt(b);
		setnz(cpu, b);
		if(ISSET(PSW_N) != ISSET(PSW_C))
			cpu->psw |= PSW_V;

		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0006100:	/* ROL */
		trace("%06o ROL%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		c = ISSET(PSW_C);
		cpu->psw &= ~(PSW_V|PSW_C);
		b = (cpu->r[8]<<1) & mask;
		if(c) b |= 1;
		if(cpu->r[8] & 0100000)
			cpu->psw |= PSW_C;
		if(by) b = sxt(b);
		setnz(cpu, b);
		if(ISSET(PSW_N) != ISSET(PSW_C))
			cpu->psw |= PSW_V;

		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0006200:	/* ASR */
		trace("%06o ASR%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		c = ISSET(PSW_C);
		cpu->psw &= ~(PSW_V|PSW_C);
		b = W(cpu->r[8]>>1) | cpu->r[8]&0100000;
		if(cpu->r[8] & 1)
			cpu->psw |= PSW_C;
		if(by) b = sxt(b);
		setnz(cpu, b);
		if(ISSET(PSW_N) != ISSET(PSW_C))
			cpu->psw |= PSW_V;

		if(writedest(cpu, b, by)) goto be;
		goto service;
	case 0006300:	/* ASL */
		trace("%06o ASL%s\n", cpu->r[7]-2, by ? "B" : "");
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		cpu->psw &= ~(PSW_V|PSW_C);
		b = W(cpu->r[8]<<1);
		if(cpu->r[8] & 0100000)
			cpu->psw |= PSW_C;
		if(by) b = sxt(b);
		setnz(cpu, b);
		if(ISSET(PSW_N) != ISSET(PSW_C))
			cpu->psw |= PSW_V;

		if(writedest(cpu, b, by)) goto be;
		goto service;

	case 0006400:
	case 0006500:
	case 0006600:
	case 0006700:
		goto ri;

	}

	switch(cpu->ir & 0107400){
	case 0004000:	/* JSR */
	case 0004400:
		trace("%06o JSR\n", cpu->r[7]-2);
		if((cpu->ir & 070) == 0)
			goto ill;
		if(readdest(cpu, 0)) goto be;
		cpu->r[9] = cpu->ba;
		cpu->r[6] -= 2;
		if((cpu->r[6]&~0377) == 0)
			cpu->traps |= TRAP_STACK;
		cpu->ba = cpu->r[6];
		cpu->bus->data = cpu->r[(cpu->ir>>6)&7];
		if(dato(cpu, 0)) goto be;
		cpu->r[(cpu->ir>>6)&7] = cpu->r[7];
		cpu->r[7] = cpu->r[9];
		goto service;

	case 0104000:	/* EMT */
		trace("%06o EMT\n", cpu->r[7]-2);
		cpu->r[10] = 030;
		goto trap;
	case 0104400:	/* TRAP */
		trace("%06o TRAP\n", cpu->r[7]-2);
		cpu->r[10] = 034;
		goto trap;
	}

	/* Branches */
	switch(cpu->ir & 0103400){
	case 0000400:	/* BR */
		trace("%06o BR\n", cpu->r[7]-2);
		cpu->r[7] += br;
		goto service;
	case 0001000:	/* BNE */
		trace("%06o BNE\n", cpu->r[7]-2);
		if(!ISSET(PSW_Z)) cpu->r[7] += br;
		goto service;
	case 0001400:	/* BEQ */
		trace("%06o BEQ\n", cpu->r[7]-2);
		if(ISSET(PSW_Z)) cpu->r[7] += br;
		goto service;
	case 0002000:	/* BGE */
		trace("%06o BGE\n", cpu->r[7]-2);
		if(ISSET(PSW_N) == ISSET(PSW_V)) cpu->r[7] += br;
		goto service;
	case 0002400:	/* BLT */
		trace("%06o BLT\n", cpu->r[7]-2);
		if(ISSET(PSW_N) != ISSET(PSW_V)) cpu->r[7] += br;
		goto service;
	case 0003000:	/* BGT */
		trace("%06o BGT\n", cpu->r[7]-2);
		if(ISSET(PSW_N) == ISSET(PSW_V) && !ISSET(PSW_Z)) cpu->r[7] += br;
		goto service;
	case 0003400:	/* BLE */
		trace("%06o BLE\n", cpu->r[7]-2);
		if(ISSET(PSW_N) != ISSET(PSW_V) || ISSET(PSW_Z)) cpu->r[7] += br;
		goto service;
	case 0100000:	/* BPL */
		trace("%06o BPL\n", cpu->r[7]-2);
		if(!ISSET(PSW_N)) cpu->r[7] += br;
		goto service;
	case 0100400:	/* BMI */
		trace("%06o BMI\n", cpu->r[7]-2);
		if(ISSET(PSW_N)) cpu->r[7] += br;
		goto service;
	case 0101000:	/* BHI */
		trace("%06o BHI\n", cpu->r[7]-2);
		if(!ISSET(PSW_C) && !ISSET(PSW_Z)) cpu->r[7] += br;
		goto service;
	case 0101400:	/* BLOS */
		trace("%06o BLOS\n", cpu->r[7]-2);
		if(ISSET(PSW_C) || ISSET(PSW_Z)) cpu->r[7] += br;
		goto service;
	case 0102000:	/* BVC */
		trace("%06o BVC\n", cpu->r[7]-2);
		if(!ISSET(PSW_V)) cpu->r[7] += br;
		goto service;
	case 0102400:	/* BVS */
		trace("%06o BVS\n", cpu->r[7]-2);
		if(ISSET(PSW_V)) cpu->r[7] += br;
		goto service;
	case 0103000:	/* BCC/BHIS */
		trace("%06o BCC\n", cpu->r[7]-2);
		if(!ISSET(PSW_C)) cpu->r[7] += br;
		goto service;
	case 0103400:	/* BCS/BLO */
		trace("%06o BCS\n", cpu->r[7]-2);
		if(ISSET(PSW_C)) cpu->r[7] += br;
		goto service;
	}

	// Hope we caught all instructions we meant to
	assert((cpu->ir & 0177400) == 0);

	/* Misc */
	switch(cpu->ir & 0300){
	case 0100:	/* JMP */
		trace("%06o JMP\n", cpu->r[7]-2);
		if((cpu->ir & 070) == 0)
			goto ill;
		if(readdest(cpu, 0)) goto be;
		cpu->r[7] = cpu->ba;
		goto service;
	case 0200:	/* RTS, Clear, Set */
		switch(cpu->ir&070){
		case 000:	/* RTS */
			trace("%06o RTS\n", cpu->r[7]-2);
			cpu->ba = cpu->r[6];
			cpu->r[6] += 2;
			cpu->r[7] = cpu->r[cpu->ir&7];
			if(dati(cpu, 0)) goto be;
			cpu->r[cpu->ir&7] = cpu->bus->data;
			break;
		case 010: case 020: case 030:
			goto ri;
		case 040: case 050:
			/* clear flags */
			trace("%06o CCC\n", cpu->r[7]-2);
			cpu->psw &= ~(cpu->ir&017);
			break;
		case 060: case 070:
			/* set flags */
			trace("%06o SEC\n", cpu->r[7]-2);
			cpu->psw |= cpu->ir&017;
			break;
		}
		goto service;
	case 0300:	/* SWAB */
		trace("%06o SWAB\n", cpu->r[7]-2);
		if(readdest(cpu, by)) goto be;
		cpu->r[8] = cpu->r[9];

		cpu->psw &= ~(PSW_V|PSW_C);
		b = WD(cpu->r[8] & 0377, (cpu->r[8]>>8) & 0377);
		setnz(cpu, b);

		if(writedest(cpu, b, by)) goto be;
		goto service;
	}

	/* Operate */
	switch(cpu->ir & 7){
	case 0:		/* HALT */
		trace("%06o HALT\n", cpu->r[7]-2);
		cpu->state = STATE_HALTED;
		return;
	case 1:		/* WAIT */
		trace("%06o WAIT\n", cpu->r[7]-2);
		cpu->state = STATE_WAITING;
		return;
	case 2:		/* RTI */
		trace("%06o RTI\n", cpu->r[7]-2);
		cpu->ba = cpu->r[6];
		cpu->r[6] += 2;
		if(dati(cpu, 0)) goto be;
		cpu->r[7] = cpu->bus->data;
		cpu->ba = cpu->r[6];
		cpu->r[6] += 2;
		if(dati(cpu, 0)) goto be;
		cpu->psw = cpu->bus->data;
		goto service;
	case 3:		/* BPT */
		trace("%06o BPT\n", cpu->r[7]-2);
		cpu->r[10] = 014;
		goto trap;
	case 4:		/* IOT */
		trace("%06o IOT\n", cpu->r[7]-2);
		cpu->r[10] = 020;
		goto trap;
	case 5:		/* RESET */
		trace("%06o RESET\n", cpu->r[7]-2);
		reset(cpu);
		goto service;
	}

	// All other instructions should be reserved now

ri:
	cpu->r[10] = 010;
	goto trap;

ill:
	cpu->r[10] = 4;
	goto trap;

be:
	cpu->be++;
	if(cpu->be > 1){
		printf("double bus error, HALT\n");
		cpu->state = STATE_HALTED;
		return;
	}
printf("bus error\n");
	cpu->r[10] = 4;
	goto trap;

service:
//	printstate(cpu);

	inhov = 0;
	c = cpu->psw >> 5;
	if(oldpsw & PSW_T){
		oldpsw &= ~PSW_T;
		cpu->r[10] = 014;
	}else if(cpu->traps & TRAP_STACK){
		cpu->traps &= ~TRAP_STACK;
		cpu->r[10] = 4;
		inhov = 1;
	}else if(cpu->traps & TRAP_PWR){
		cpu->traps &= ~TRAP_PWR;
		cpu->r[10] = 024;
	}else if(c < 7 && cpu->traps & TRAP_BR7){
		cpu->traps &= ~TRAP_BR7;
		cpu->r[10] = cpu->br[3].bg(cpu->br[3].dev);
	}else if(c < 6 && cpu->traps & TRAP_BR6){
		cpu->traps &= ~TRAP_BR6;
		cpu->r[10] = cpu->br[2].bg(cpu->br[2].dev);
	}else if(c < 6 && cpu->traps & TRAP_CLK){
		cpu->traps &= ~TRAP_CLK;
		cpu->lc_int = 0;
		cpu->r[10] = 0100;
	}else if(c < 5 && cpu->traps & TRAP_BR5){
		cpu->traps &= ~TRAP_BR5;
		cpu->r[10] = cpu->br[1].bg(cpu->br[1].dev);
	}else if(c < 4 && cpu->traps & TRAP_BR4){
		cpu->traps &= ~TRAP_BR4;
		cpu->r[10] = cpu->br[0].bg(cpu->br[0].dev);
	}else if(c < 4 && cpu->traps & TRAP_RX){
		cpu->traps &= ~TRAP_RX;
		cpu->rcd_int = 0;
		cpu->r[10] = 060;
	}else if(c < 4 && cpu->traps & TRAP_TX){
		cpu->traps &= ~TRAP_TX;
		cpu->xmit_int = 0;
		cpu->r[10] = 064;
	}else
	// TODO? console stop
		/* fetch next instruction */
		return;

trap:
	trace("TRAP %o\n", cpu->r[10]);
	/* save psw */
	cpu->r[6] -= 2;
	cpu->ba = cpu->r[6];
	if(!inhov && (cpu->ba&~0377) == 0)
		cpu->traps |= TRAP_STACK;
	cpu->bus->data = cpu->psw;
	if(dato(cpu, 0)) goto be;
	/* save pc */
	cpu->r[6] -= 2;
	cpu->ba = cpu->r[6];
	if(!inhov && (cpu->ba&~0377) == 0)
		cpu->traps |= TRAP_STACK;
	cpu->bus->data = cpu->r[7];
	if(dato(cpu, 0)) goto be;
	/* read new pc */
	cpu->ba = cpu->r[10];
	if(dati(cpu, 0)) goto be;
	cpu->r[7] = cpu->bus->data;
	/* read new psw */
	cpu->ba = cpu->r[10] + 2;
	if(dati(cpu, 0)) goto be;
	cpu->psw = cpu->bus->data;
	/* no trace trap after a trap */
	oldpsw = cpu->psw;

	tracestate(cpu);
	goto service;
}

void
run(KD11B *cpu)
{
	int n;
	cpu->state = STATE_RUNNING;
	initclock();
	n = 0;
	while(cpu->state != STATE_HALTED){
		handleclock(cpu);

		cpu->traps &= ~TRAP_CLK;
		if(cpu->lc_int && cpu->lc_int_enab)
			cpu->traps |= TRAP_CLK;

		cpu->traps &= ~TRAP_TX;
		if(cpu->xmit_int && cpu->xmit_int_enab)
			cpu->traps |= TRAP_TX;

		cpu->traps &= ~TRAP_RX;
		if(cpu->rcd_int && cpu->rcd_int_enab)
			cpu->traps |= TRAP_RX;

		svc(cpu, cpu->bus);

		if(cpu->state == STATE_RUNNING ||
		   cpu->state == STATE_WAITING && cpu->traps){
			cpu->state = STATE_RUNNING;
			step(cpu);
		}

		// Don't handle IO all the time
		n++;
		if(n != 20)
			continue;
		n = 0;

		/* transmit */
		if(!cpu->xmit_tbmt){
			uint8 c = cpu->xmit_b & 0177;
			write(cpu->ttyfd, &c, 1);
			cpu->xmit_tbmt = 1;
			cpu->xmit_int = 1;
		}

		/* receive */
		if(hasinput(cpu->ttyfd)){
			cpu->rcd_busy = 1;
			cpu->rcd_rdr_enab = 0;
			read(cpu->ttyfd, &cpu->rcd_b, 1);
			cpu->rcd_da = 1;
			cpu->rcd_busy = 0;
			cpu->rcd_int = 1;
		}
	}

	printstate(cpu);
}

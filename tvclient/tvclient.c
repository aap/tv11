#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <SDL.h>

typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

#define nil NULL


#define WIDTH 576
#define HEIGHT 454

SDL_Renderer *renderer;
SDL_Texture *screentex;
uint32 fb[WIDTH*HEIGHT];
uint32 fg = 0x00FF0000;
uint32 bg = 0x00000000;
int fd;

enum {
	MSG_KEYDN = 0,
	MSG_GETFB,
};

void
panic(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
	SDL_Quit();
}

uint16
b2w(uint8 *b)
{
	return b[0] | b[1]<<8;
}

void
w2b(uint8 *b, uint16 w)
{
	b[0] = w;
	b[1] = w>>8;
}

int
dial(char *host, int port)
{
	int sockfd;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		perror("error: socket");
		return -1;
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	inet_pton(AF_INET, host, &serv_addr.sin_addr);
	serv_addr.sin_port = htons(port);

	if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("error: connect");
		return -1;
	}
	return sockfd;
}

void
draw(void)
{
	SDL_UpdateTexture(screentex, nil, fb, WIDTH*sizeof(uint32));
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, screentex, nil, nil);
	SDL_RenderPresent(renderer);
}

/* Map SDL scancodes to Knight keyboard codes as best we can */
int scancodemap[SDL_NUM_SCANCODES];

void
initkeymap(void)
{
	int i;
	for(i = 0; i < SDL_NUM_SCANCODES; i++)
		scancodemap[i] = -1;

	scancodemap[SDL_SCANCODE_F12] = 000;	/* BREAK */
	scancodemap[SDL_SCANCODE_F1] = 001;	/* ESC */
	scancodemap[SDL_SCANCODE_1] = 002;
	scancodemap[SDL_SCANCODE_2] = 003;
	scancodemap[SDL_SCANCODE_3] = 004;
	scancodemap[SDL_SCANCODE_4] = 005;
	scancodemap[SDL_SCANCODE_5] = 006;
	scancodemap[SDL_SCANCODE_6] = 007;
	scancodemap[SDL_SCANCODE_7] = 010;
	scancodemap[SDL_SCANCODE_8] = 011;
	scancodemap[SDL_SCANCODE_9] = 012;
	scancodemap[SDL_SCANCODE_0] = 013;
	scancodemap[SDL_SCANCODE_MINUS] = 014;	/* - = */
	scancodemap[SDL_SCANCODE_EQUALS] = 015;	/* @ ` */
	scancodemap[SDL_SCANCODE_GRAVE] = 016;	/* ^ ~ */
	scancodemap[SDL_SCANCODE_BACKSPACE] = 017;
	scancodemap[SDL_SCANCODE_F2] = 0020;	/* CALL */

	scancodemap[SDL_SCANCODE_F3] = 0021;	/* CLEAR */
	scancodemap[SDL_SCANCODE_TAB] = 022;
	scancodemap[SDL_SCANCODE_ESCAPE] = 023;	/* ALT MODE */
	scancodemap[SDL_SCANCODE_Q] = 024;
	scancodemap[SDL_SCANCODE_W] = 025;
	scancodemap[SDL_SCANCODE_E] = 026;
	scancodemap[SDL_SCANCODE_R] = 027;
	scancodemap[SDL_SCANCODE_T] = 030;
	scancodemap[SDL_SCANCODE_Y] = 031;
	scancodemap[SDL_SCANCODE_U] = 032;
	scancodemap[SDL_SCANCODE_I] = 033;
	scancodemap[SDL_SCANCODE_O] = 034;
	scancodemap[SDL_SCANCODE_P] = 035;
	scancodemap[SDL_SCANCODE_LEFTBRACKET] = 036;
	scancodemap[SDL_SCANCODE_RIGHTBRACKET] = 037;
	scancodemap[SDL_SCANCODE_BACKSLASH] = 040;
	// / inf
	// +- delta
	// O+ gamma

	// FORM
	// VTAB
	scancodemap[SDL_SCANCODE_DELETE] = 046;	/* RUBOUT */
	scancodemap[SDL_SCANCODE_A] = 047;
	scancodemap[SDL_SCANCODE_S] = 050;
	scancodemap[SDL_SCANCODE_D] = 051;
	scancodemap[SDL_SCANCODE_F] = 052;
	scancodemap[SDL_SCANCODE_G] = 053;
	scancodemap[SDL_SCANCODE_H] = 054;
	scancodemap[SDL_SCANCODE_J] = 055;
	scancodemap[SDL_SCANCODE_K] = 056;
	scancodemap[SDL_SCANCODE_L] = 057;
	scancodemap[SDL_SCANCODE_SEMICOLON] = 060;	/* ; + */
	scancodemap[SDL_SCANCODE_APOSTROPHE] = 061;	/* : * */
	scancodemap[SDL_SCANCODE_RETURN] = 062;
	// LINE FEED
	// NEXT BACK

	scancodemap[SDL_SCANCODE_Z] = 065;
	scancodemap[SDL_SCANCODE_X] = 066;
	scancodemap[SDL_SCANCODE_C] = 067;
	scancodemap[SDL_SCANCODE_V] = 070;
	scancodemap[SDL_SCANCODE_B] = 071;
	scancodemap[SDL_SCANCODE_N] = 072;
	scancodemap[SDL_SCANCODE_M] = 073;
	scancodemap[SDL_SCANCODE_COMMA] = 074;
	scancodemap[SDL_SCANCODE_PERIOD] = 075;
	scancodemap[SDL_SCANCODE_SLASH] = 076;
	scancodemap[SDL_SCANCODE_SPACE] = 077;
}

/* These bits are directly sent to the 11 */
enum {
	MOD_RSHIFT = 0100,
	MOD_LSHIFT = 0200,
	MOD_RTOP = 00400,
	MOD_LTOP = 01000,
	MOD_RCTRL = 02000,
	MOD_LCTRL = 04000,
	MOD_RMETA = 010000,
	MOD_LMETA = 020000,
	MOD_SLOCK = 040000,
};

int curmod;

void
keydown(SDL_Keysym keysym)
{
	int key;

	switch(keysym.scancode){
	case SDL_SCANCODE_LSHIFT: curmod |= MOD_LSHIFT; break;
	case SDL_SCANCODE_RSHIFT: curmod |= MOD_RSHIFT; break;
	case SDL_SCANCODE_LGUI: curmod |= MOD_LTOP; break;
	case SDL_SCANCODE_RGUI: curmod |= MOD_RTOP; break;
	case SDL_SCANCODE_LCTRL: curmod |= MOD_LCTRL; break;
	case SDL_SCANCODE_RCTRL: curmod |= MOD_RCTRL; break;
	case SDL_SCANCODE_LALT: curmod |= MOD_LMETA; break;
	case SDL_SCANCODE_RALT: curmod |= MOD_RMETA; break;
	case SDL_SCANCODE_CAPSLOCK: curmod |= MOD_SLOCK; break;
	}

	key = scancodemap[keysym.scancode];
	if(key < 0)
		return;

	key |= curmod;
printf("%o\n", key);
	uint8 buf[4];
	buf[0] = 3;
	buf[1] = MSG_KEYDN;
	w2b(buf+2, key);
	write(fd, buf, 4);
//	printf("down: %d %o %o\n", keysym.scancode, key, curmod);
}

void
keyup(SDL_Keysym keysym)
{
	switch(keysym.scancode){
	case SDL_SCANCODE_LSHIFT: curmod &= ~MOD_LSHIFT; break;
	case SDL_SCANCODE_RSHIFT: curmod &= ~MOD_RSHIFT; break;
	case SDL_SCANCODE_LGUI: curmod &= ~MOD_LTOP; break;
	case SDL_SCANCODE_RGUI: curmod &= ~MOD_RTOP; break;
	case SDL_SCANCODE_LCTRL: curmod &= ~MOD_LCTRL; break;
	case SDL_SCANCODE_RCTRL: curmod &= ~MOD_RCTRL; break;
	case SDL_SCANCODE_LALT: curmod &= ~MOD_LMETA; break;
	case SDL_SCANCODE_RALT: curmod &= ~MOD_RMETA; break;
	case SDL_SCANCODE_CAPSLOCK: curmod &= ~MOD_SLOCK; break;
	}
//	printf("up: %d %o %o\n", keysym.scancode, scancodemap[keysym.scancode], curmod);
}

void
winevent(SDL_WindowEvent ev)
{
}

uint8 largebuf[16*1024*2];

void
dumpbuf(uint8 *b, int n)
{
	while(n--)
		printf("%o ", *b++);
	printf("\n");
}

void
unpackfb(uint8 *src, int x, int y, int w, int h)
{
	int i, j;
	uint32 *dst;
	uint16 wd;

	dst = &fb[y*WIDTH + x];
	for(i = 0; i < h; i++){
		for(j = 0; j < w; j++){
			if(j%16 == 0){
				wd = b2w(src);
				src += 2;
			}
			dst[j] = wd&0100000 ? fg : bg;
			wd <<= 1;
		}
		dst += WIDTH;
	}
	printf("update: %d %d %d %d\n", x, y, w, h);
	draw();
}

void
getfb(void)
{
	uint8 buf[10];
	int x, y, w, h;

	x = 0;
	y = 0;
	w = WIDTH;
	h = HEIGHT;

/*	x = WIDTH/2;
	y = HEIGHT/2;
	w = WIDTH/2;
	h = HEIGHT/2;
*/
	buf[0] = 9;
	buf[1] = MSG_GETFB;
	w2b(buf+2, x);
	w2b(buf+4, y);
	w2b(buf+6, w);
	w2b(buf+8, h);
	write(fd, buf, 10);

	x /= 16;
	w = (w+15) / 16;
printf("buf size: %d\n", w*h*2);
	read(fd, largebuf, w*h*2);
	unpackfb(largebuf, x*16, y, w*16, h);
}

void
getdpykbd(void)
{
	uint8 buf[2];
	if(read(fd, buf, 2) != 2){
		fprintf(stderr, "protocol botch\n");
		return;
	}
	printf("%o %o\n", buf[0], buf[1]);
}

int
main(int argc, char *argv[])
{
	SDL_Window *window;
	SDL_Event event;
	int running;

	initkeymap();

	fd = dial("localhost", 10000);

	if(SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer) < 0)
		panic("SDL_CreateWindowAndRenderer() failed: %s\n", SDL_GetError());

	screentex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

	int i;
	for(i = 0; i < WIDTH*HEIGHT; i++)
		fb[i] = bg;

	getdpykbd();
	getfb();

	running = 1;
	while(running){
		if(SDL_WaitEvent(&event) < 0)
			panic("SDL_PullEvent() error: %s\n", SDL_GetError());
		switch(event.type){
		case SDL_MOUSEBUTTONDOWN:
			break;

		case SDL_WINDOWEVENT:
			winevent(event.window);
			break;

		case SDL_KEYDOWN:
			keydown(event.key.keysym);
			break;
		case SDL_KEYUP:
			keyup(event.key.keysym);
			break;
		case SDL_QUIT:
			running = 0;
			break;
		}
		draw();
	}
	return 0;
}

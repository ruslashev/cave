#include "gfx.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//04/29/2005: Fixed warnings in OpenWatcom 1.3. Fixed move&look up/down speed.
//10/08/2002: Added screen capture to X.PNG using F12.
//   Last touch before today was:
//      CAVE.C 11/17/1994 03:22 AM 15,722
//   Looks like I ported GROUCAVE.BAS->CAVE.C around 04/20/1994 to 04/21/1994

#define MAXPALOOKUPS 64u

long posx, posy, posz, horiz, xdim, ydim;
long newposz, vel, svel, angvel;
short ang, vidmode;
short mousx, mousy;

char h1[65536], c1[65536];
char h2[65536], c2[65536];
short sintable[2048];
char scrbuf[128000];
unsigned char scr[320 * 200];
unsigned short numpalookups;
unsigned char palookup[MAXPALOOKUPS << 8u];
uint32_t palette[256];

volatile char keystatus[256];

long scale(long a, long b, long c)
{
	return (a * b) / c;
}

long mulscale(long a, long b, long c)
{
	return (a * b) >> (c & 0xff);
}

long divscale(long a, long b, long c)
{
	return (a << (c & 0xff)) / b;
}

long groudiv(long a, long b)
{
	a <<= 12;
	a -= posz;
	b >>= 8;
	a /= (b & 0xffff);
	return a;
}

long drawtopslab(long edi, long ecx, long eax)
{
	int al = eax & 0xFF;
	int carry;

	carry = ecx & 1;
	ecx >>= 1;

	if (carry == 0)
		goto skipdraw1a;

	scr[edi] = al;

	edi += 80;

skipdraw1a:
	carry = ecx & 1;
	ecx >>= 1;

	if (carry == 0)
		goto skipdraw2a;

	scr[edi] = al;
	scr[edi + 80] = al;
	edi += 160;

skipdraw2a:
	if (ecx == 0)
		goto skipdraw4a;

startdrawa:
	scr[edi] = al;
	scr[edi + 80] = al;
	scr[edi + 160] = al;
	scr[edi + 240] = al;
	edi += 320;

	ecx--;
	if (ecx != 0)
		goto startdrawa;

skipdraw4a:
	eax = edi;

	return eax;
}

int drawbotslab(int edi, int ecx, int eax)
{
	int al = eax & 0xFF;
	int carry;

	carry = ecx & 1;
	ecx >>= 1;

	if (carry == 0)
		goto skipdraw1b;

	scr[edi] = al;

	edi -= 80;

skipdraw1b:
	carry = ecx & 1;
	ecx >>= 1;

	if (carry == 0)
		goto skipdraw2b;

	scr[edi] = al;
	scr[edi - 80] = al;
	edi -= 160;

skipdraw2b:
	if (ecx == 0)
		goto skipdraw4b;

startdrawb:
	scr[edi] = al;
	scr[edi - 80] = al;
	scr[edi - 160] = al;
	scr[edi - 240] = al;
	edi -= 320;

	ecx--;
	if (ecx != 0)
		goto startdrawb;

skipdraw4b:
	eax = edi;

	return eax;
}

void readmouse()
{
    mousx = 0;
    mousy = 0;
}

void setscreenmode()
{
	int fd;
	unsigned char palette_interleaved[768];

	if ((fd = open("palette.dat", O_RDONLY)) == -1)
		die("Can't load palette.dat.  Now why could that be?");

	read(fd, &palette_interleaved[0], 768);
	read(fd, &numpalookups, 2);
	read(fd, &palookup[0], numpalookups << 8u);

	close(fd);

	for (int i = 0; i < 256; ++i) {
		unsigned char r = palette_interleaved[i * 3 + 0] * 4,
		              g = palette_interleaved[i * 3 + 1] * 4,
		              b = palette_interleaved[i * 3 + 2] * 4;
		palette[i] = (r << 16) + (g << 8) + b;
	}
}

void loadtables()
{
	int fd;

	if ((fd = open("tables.dat", O_RDONLY)) == -1)
		die("can't open tables.dat");

	read(fd, &sintable[0], 4096);
	close(fd);
}

void loadboard()
{
	posx = 512;
	posy = 512;
	posz = (128 - 32) << 12;
	ang = 0;
	horiz = ydim >> 1;

	for (int i = 0; i < 256; ++i)
		for (int j = 0; j < 256; ++j) {
			h1[(i << 8) + j] = 255;
			c1[(i << 8) + j] = 128;
			h2[(i << 8) + j] = 0;
			c2[(i << 8) + j] = 128;
		}
}

long ksqrt(long num)
{
	long root = 128, temp;

	do {
		temp = root;
		root = (root + (num / root)) >> 1;
	} while (labs(temp - root) > 1);

	return root;
}

void blast(long gridx, long gridy, long rad, char blastingcol)
{
	short tempshort;
	long i, j, dax, day, daz, dasqr, templong;

	templong = rad + 2;
	for (i = -templong; i <= templong; ++i)
		for (j = -templong; j <= templong; ++j) {
			dax = (gridx + i + 256) & 255;
			day = (gridy + j + 256) & 255;
			dasqr = rad * rad - (i * i + j * j);

			if (dasqr >= 0)
				daz = ksqrt(dasqr) << 1;
			else
				daz = -(ksqrt(-dasqr) << 1);

			if ((posz >> 12) - daz < h1[(dax << 8) + day]) {
				h1[(dax << 8) + day] = (posz >> 12) - daz;
				if ((posz >> 12) - daz < 0)
					h1[(dax << 8) + day] = 0;
			}

			if ((posz >> 12) + daz > h2[(dax << 8) + day]) {
				h2[(dax << 8) + day] = (posz >> 12) + daz;
				if ((posz >> 12) + daz > 255)
					h2[(dax << 8) + day] = 255;
			}

			tempshort = h1[(dax << 8) + day];
			if (tempshort >= h2[(dax << 8)+day]) tempshort = posz >> 12;
			tempshort = labs(64 - (tempshort & 127)) + (rand() & 3) - 2;
			if (tempshort < 0) tempshort = 0;
			if (tempshort > 63) tempshort = 63;
			c1[(dax << 8) + day] = (char)(tempshort + blastingcol);

			tempshort = h2[(dax << 8) + day];
			if (tempshort <= h1[(dax << 8) + day]) tempshort = (posz >> 12);
			tempshort = labs(64 - (tempshort & 127)) + (rand() & 3) - 2;
			if (tempshort < 0) tempshort = 0;
			if (tempshort > 63) tempshort = 63;
			c2[(dax << 8) + day] = (char)(tempshort + blastingcol);
		}
}

void grouvline (short x, long scandist)
{
	long dist[2], dinc[2], incr[2];
	short grid[2], dir[2];

	char oh1, oh2;
	short um, dm, h, i;
	long plc1, plc2, cosval, sinval;
	long snx, sny, dax, c, shade, cnt, bufplc;

	plc1 = 0 * 80 + (x >> 2);
	plc2 = (ydim - 1) * 80 + (x >> 2);
	if ((x & 2) > 0) {
		plc1 += 32000 * (vidmode + 1);
		plc2 += 32000 * (vidmode + 1);
	}
	if ((x & 1) > 0) {
		plc1 += 16000 * (vidmode + 1);
		plc2 += 16000 * (vidmode + 1);
	}

	cosval = sintable[(ang+2560)&2047];
	sinval = sintable[(ang+2048)&2047];

	dax = (x<<1)-xdim;

	incr[0] = cosval - scale(sinval,dax,xdim);
	incr[1] = sinval + scale(cosval,dax,xdim);

	if (incr[0] < 0)
		dir[0] = -1, incr[0] = -incr[0];
	else
		dir[0] = 1;

	if (incr[1] < 0)
		dir[1] = -1, incr[1] = -incr[1];
	else
		dir[1] = 1;

	snx = posx & 1023;
	if (dir[0] == 1)
		snx ^= 1023;

	sny = posy & 1023;
	if (dir[1] == 1)
		sny ^= 1023;

	cnt = (snx * incr[1] - sny * incr[0]) >> 10;
	grid[0] = (posx >> 10) & 255;
	grid[1] = (posy >> 10) & 255;

	if (incr[0] != 0) {
		dinc[0] = divscale(65536 >> vidmode, incr[0], 12);
		dist[0] = mulscale(dinc[0], snx, 10);
	}
	if (incr[1] != 0) {
		dinc[1] = divscale(65536 >> vidmode, incr[1], 12);
		dist[1] = mulscale(dinc[1], sny, 10);
	}

	um = -horiz;
	dm = ydim - 1 - horiz;

	i = incr[0];
	incr[0] = incr[1];
	incr[1] = -i;

	shade = 8;
	while (dist[cnt >= 0] <= 8192) {
		i = cnt >= 0;

		grid[i] = (grid[i] + dir[i]) & 255;
		dist[i] += dinc[i];
		cnt += incr[i];
		++shade;
	}

	bufplc = (grid[0] << 8) + grid[1];

	while (shade < scandist - 9) {
		i = cnt >= 0;

		oh1 = h1[bufplc];
		oh2 = h2[bufplc];

		h = groudiv((long)oh1, dist[i]);
		if (um <= h) {
			c = palookup[((shade >> 1) << 8) + c1[bufplc]];
			if (h > dm)
				break;
			plc1 = drawtopslab(plc1, h - um + 1, c);
			um = h + 1;
		}

		h = groudiv((long)oh2, dist[i]);
		if (dm >= h) {
			c = palookup[((shade >> 1) << 8) + c2[bufplc]];
			if (h < um)
				break;
			plc2 = drawbotslab(plc2, dm - h + 1, c);
			dm = h - 1;
		}

		grid[i] = (grid[i] + dir[i]) & 255;
		bufplc = (grid[0] << 8) + grid[1];

		if (h1[bufplc] > oh1) {
			h = groudiv((long)h1[bufplc], dist[i]);
			if (um <= h) {
				c = palookup[(((shade >> 1) - (i << 2)) << 8) + c1[bufplc]];
				if (h > dm)
					break;
				plc1 = drawtopslab(plc1, h - um + 1, c);
				um = h + 1;
			}
		}

		if (h2[bufplc] < oh2) {
			h = groudiv((long)h2[bufplc], dist[i]);
			if (dm >= h) {
				c = palookup[(((shade >> 1) + (i << 2)) << 8) + c2[bufplc]];
				if (h < um)
					break;
				plc2 = drawbotslab(plc2, dm - h + 1, c);
				dm = h - 1;
			}
		}

		dist[i] += dinc[i];
		cnt += incr[i];
		++shade;
	}

	if (dm >= um) {
		if (shade >= scandist - 9)
			c = palookup[(numpalookups - 1) << 8];
		drawtopslab(plc1, dm - um + 1, c);
	}
}

void keydown(char key, int down)
{
	printf("key '%c' down %d\n", key, down);
}

int main()
{
	char blastcol;

	vidmode = 0;
	xdim = 320;
	ydim = 200;
	blastcol = 0;

	gfx_init(xdim, ydim);

	setscreenmode();
	loadtables();
	loadboard();

	blast((posx >> 10) & 255, (posy >> 10) & 255, 8L, blastcol);

	while (gfx_update(keydown)) {
		gfx_clear();

		for (long i = 0; i < xdim; ++i)
			grouvline((short)i, 128L);

		/*
		if (keystatus[0x33] > 0)   // ,< Change blasting color
		{
			keystatus[0x33] = 0;
			blastcol = ((blastcol+64)&255);
		}
		if (keystatus[0x34] > 0)   // .> Change blasting color
		{
			keystatus[0x34] = 0;
			blastcol = ((blastcol+192)&255);
		}
		if (keystatus[0x39] > 0)
		{
			if ((keystatus[0x1d]|keystatus[0x9d]) > 0)
				blast(((posx>>10)&255),((posy>>10)&255),16L,blastcol);
			else
				blast(((posx>>10)&255),((posy>>10)&255),8L,blastcol);
		}
		*/

		vel = 0L;
		svel = 0L;
		angvel = 0;

		/*
		readmouse();
		ang += mousx;
		vel = (((long)-mousy)<<3);

		if (keystatus[0x4e] > 0) horiz += clockspeed;
		if (keystatus[0x4a] > 0) horiz -= clockspeed;
		if (keystatus[0x1e] > 0)
		{
			posz -= (clockspeed<<(keystatus[0x2a]+8));
			if (posz < 2048) posz = 2048;
		}
		if (keystatus[0x2c] > 0)
		{
			posz += (clockspeed<<(keystatus[0x2a]+8));
			if (posz >= 1048576-4096-2048) posz = 1048575-4096-2048;
		}
		if (keystatus[0x9d] == 0)
		{
			if (keystatus[0xcb] > 0) angvel = -16;
			if (keystatus[0xcd] > 0) angvel = 16;
		}
		else
		{
			if (keystatus[0xcb] > 0) svel = 12L;
			if (keystatus[0xcd] > 0) svel = -12L;
		}
		if (keystatus[0xc8] > 0) vel = 12L;
		if (keystatus[0xd0] > 0) vel = -12L;
		if (keystatus[0x2a] > 0)
		{
			vel <<= 1;
			svel <<= 1;
		}
		if (angvel != 0)
		{
			ang += ((angvel*((short int)clockspeed))>>3);
			ang = (ang+2048)&2047;
		}
		if ((vel != 0L) || (svel != 0L))
		{
			posx += ((vel*clockspeed*sintable[(2560+ang)&2047])>>12);
			posy += ((vel*clockspeed*sintable[(2048+ang)&2047])>>12);
			posx += ((svel*clockspeed*sintable[(2048+ang)&2047])>>12);
			posy -= ((svel*clockspeed*sintable[(2560+ang)&2047])>>12);
			posx &= 0x3ffffff;
			posy &= 0x3ffffff;
		}
		*/
	}
}


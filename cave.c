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
short ang, pixs, vidmode;
short mousx, mousy;

char h1[65536], c1[65536];
char h2[65536], c2[65536];
short sintable[2048];
char scrbuf[128000];
unsigned short numpalookups;
unsigned char palookup[MAXPALOOKUPS << 8u];
uint32_t palette[256];

volatile char keystatus[256];
volatile long clockspeed, totalclock, numframes;

long scale (long, long, long);
#pragma aux scale =\
	"imul ebx"\
	"idiv ecx"\
	parm [eax][ebx][ecx] modify [eax edx]

long mulscale (long, long, long);
#pragma aux mulscale =\
	"imul ebx"\
	"shrd eax, edx, cl"\
	parm [eax][ebx][ecx] modify [edx]

long divscale (long, long, long);
#pragma aux divscale =\
	"cdq"\
	"shld edx, eax, cl"\
	"sal eax, cl"\
	"idiv ebx"\
	parm [eax][ebx][ecx] modify [edx]

long groudiv (long, long);
#pragma aux groudiv =\
	"shl eax, 12"\
	"sub eax, posz"\
	"shld edx, eax, 16"\
	"sar ebx, 8"\
	"idiv bx"\
	parm [eax][ebx] modify [edx]

long drawtopslab (long, long, long);
#pragma aux drawtopslab =\
	"shr ecx, 1"\
	"jnc skipdraw1a"\
	"mov [edi], al"\
	"add edi, 80"\
	"skipdraw1a: shr ecx, 1"\
	"jnc skipdraw2a"\
	"mov [edi], al"\
	"mov [edi+80], al"\
	"add edi, 160"\
	"skipdraw2a: jecxz skipdraw4a"\
	"startdrawa: mov [edi], al"\
	"mov [edi+80], al"\
	"mov [edi+160], al"\
	"mov [edi+240], al"\
	"add edi, 320"\
	"loop startdrawa"\
	"skipdraw4a: mov eax, edi"\
	parm [edi][ecx][eax] modify [edi ecx eax]

long drawbotslab (long, long, long);
#pragma aux drawbotslab =\
	"shr ecx, 1"\
	"jnc skipdraw1b"\
	"mov [edi], al"\
	"sub edi, 80"\
	"skipdraw1b: shr ecx, 1"\
	"jnc skipdraw2b"\
	"mov [edi], al"\
	"mov [edi-80], al"\
	"sub edi, 160"\
	"skipdraw2b: jecxz skipdraw4b"\
	"startdrawb: mov [edi], al"\
	"mov [edi-80], al"\
	"mov [edi-160], al"\
	"mov [edi-240], al"\
	"sub edi, 320"\
	"loop startdrawb"\
	"skipdraw4b: mov eax, edi"\
	parm [edi][ecx][eax] modify [edi ecx eax]

void readmouse()
{
    mousx = 0;
    mousy = 0;
}

void setscreenmode()
{
	int fil;
	unsigned char palette_interleaved[768];

	if ((fil = open("palette.dat", O_RDONLY)) == -1)
		die("Can't load palette.dat.  Now why could that be?");

	read(fil, &palette_interleaved[0], 768);
	read(fil, &numpalookups, 2);
	read(fil, &palookup[0], numpalookups << 8u);

	close(fil);

	for (int i = 0; i < 256; ++i) {
		unsigned char r = palette_interleaved[i * 3 + 0] * 4,
		              g = palette_interleaved[i * 3 + 1] * 4,
		              b = palette_interleaved[i * 3 + 2] * 4;
		palette[i] = (r << 16) + (g << 8) + b;
	}
}

void loadtables ()
{
	short fil;

	if ((fil = open("tables.dat",O_BINARY|O_RDWR,S_IREAD)) != -1)
	{
		read(fil,&sintable[0],4096);
		close(fil);
	}
}

void loadboard ()
{
	long i, j;

	posx = 512; posy = 512; posz = ((128-32)<<12); ang = 0;
	horiz = (ydim>>1);
	for(i=0;i<256;i++)
		for(j=0;j<256;j++)
		{
			h1[(i<<8)+j] = 255;
			c1[(i<<8)+j] = 128;
			h2[(i<<8)+j] = 0;
			c2[(i<<8)+j] = 128;
		}
}

long ksqrt (long num)
{
	long root, temp;

	root = 128;
	do
	{
		temp = root;
		root = ((root+(num/root))>>1);
	}
	while (labs(temp-root) > 1);
	return(root);
}

void blast (long gridx, long gridy, long rad, char blastingcol)
{
	short tempshort;
	long i, j, dax, day, daz, dasqr, templong;

	templong = rad+2;
	for(i=-templong;i<=templong;i++)
		for(j=-templong;j<=templong;j++)
		{
			dax = ((gridx+i+256)&255);
			day = ((gridy+j+256)&255);
			dasqr = rad*rad-(i*i+j*j);

			if (dasqr >= 0)
				daz = (ksqrt(dasqr)<<1);
			else
				daz = -(ksqrt(-dasqr)<<1);

			if ((posz>>12)-daz < h1[(dax<<8)+day])
			{
				h1[(dax<<8)+day] = (posz>>12)-daz;
				if (((posz>>12)-daz) < 0)
					h1[(dax<<8)+day] = 0;
			}

			if ((posz>>12)+daz > h2[(dax<<8)+day])
			{
				h2[(dax<<8)+day] = (posz>>12)+daz;
				if (((posz>>12)+daz) > 255)
					h2[(dax<<8)+day] = 255;
			}

			tempshort = h1[(dax<<8)+day];
			if (tempshort >= h2[(dax<<8)+day]) tempshort = (posz>>12);
			tempshort = labs(64-(tempshort&127))+(rand()&3)-2;
			if (tempshort < 0) tempshort = 0;
			if (tempshort > 63) tempshort = 63;
			c1[(dax<<8)+day] = (char)(tempshort+blastingcol);

			tempshort = h2[(dax<<8)+day];
			if (tempshort <= h1[(dax<<8)+day]) tempshort = (posz>>12);
			tempshort = labs(64-(tempshort&127))+(rand()&3)-2;
			if (tempshort < 0) tempshort = 0;
			if (tempshort > 63) tempshort = 63;
			c2[(dax<<8)+day] = (char)(tempshort+blastingcol);
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

	switch(pixs)
	{
		case 1:
			plc1 = 0 * 80+(x>>2)+FP_OFF(scrbuf);
			plc2 = (ydim - 1) * 80+(x>>2)+FP_OFF(scrbuf);
			if ((x&2) > 0)
			{
				plc1 += 32000*(vidmode+1);
				plc2 += 32000*(vidmode+1);
			}
			if ((x&1) > 0)
			{
				plc1 += 16000*(vidmode+1);
				plc2 += 16000*(vidmode+1);
			}
			break;
		case 2:
			plc1 = 0 * 80+(x>>2)+FP_OFF(scrbuf);
			plc2 = (ydim - 1) * 80+(x>>2)+FP_OFF(scrbuf);
			if ((x&2) > 0)
			{
				plc1 += 16000*(vidmode+1);
				plc2 += 16000*(vidmode+1);
			}
			break;
		case 4:
			plc1 = 0 * 80+(x>>2)+FP_OFF(scrbuf);
			plc2 = (ydim - 1) * 80+(x>>2)+FP_OFF(scrbuf);
			break;
	}

	cosval = sintable[(ang+2560)&2047];
	sinval = sintable[(ang+2048)&2047];

	if (pixs == 1) dax = (x<<1)-xdim;
	if (pixs == 2) dax = (x<<1)+1-xdim;
	if (pixs == 4) dax = (x<<1)+3-xdim;

	incr[0] = cosval - scale(sinval,dax,xdim);
	incr[1] = sinval + scale(cosval,dax,xdim);

	if (incr[0] < 0) dir[0] = -1, incr[0] = -incr[0]; else dir[0] = 1;
	if (incr[1] < 0) dir[1] = -1, incr[1] = -incr[1]; else dir[1] = 1;
	snx = (posx&1023); if (dir[0] == 1) snx ^= 1023;
	sny = (posy&1023); if (dir[1] == 1) sny ^= 1023;
	cnt = ((snx*incr[1] - sny*incr[0])>>10);
	grid[0] = ((posx>>10)&255); grid[1] = ((posy>>10)&255);

	if (incr[0] != 0)
	{
		dinc[0] = divscale(65536>>vidmode,incr[0],12);
		dist[0] = mulscale(dinc[0],snx,10);
	}
	if (incr[1] != 0)
	{
		dinc[1] = divscale(65536>>vidmode,incr[1],12);
		dist[1] = mulscale(dinc[1],sny,10);
	}

	um = 0 - horiz;
	dm = (ydim - 1) - horiz;

	i = incr[0]; incr[0] = incr[1]; incr[1] = -i;

	shade = 8;
	while (dist[cnt>=0] <= 8192)
	{
		i = (cnt>=0);

		grid[i] = ((grid[i]+dir[i])&255);
		dist[i] += dinc[i];
		cnt += incr[i];
		shade++;
	}

	bufplc = (grid[0]<<8)+grid[1];

	while (shade < scandist-9)
	{
		i = (cnt>=0);

		oh1 = h1[bufplc], oh2 = h2[bufplc];

		h = groudiv((long)oh1,dist[i]);
		if (um <= h)
		{
			c = palookup[((shade>>1)<<8)+c1[bufplc]];
			if (h > dm) break;
			plc1 = drawtopslab(plc1,h-um+1,c);
			um = h+1;
		}

		h = groudiv((long)oh2,dist[i]);
		if (dm >= h)
		{
			c = palookup[((shade>>1)<<8)+c2[bufplc]];
			if (h < um) break;
			plc2 = drawbotslab(plc2,dm-h+1,c);
			dm = h-1;
		}

		grid[i] = ((grid[i]+dir[i])&255);
		bufplc = (grid[0]<<8)+grid[1];

		if (h1[bufplc] > oh1)
		{
			h = groudiv((long)h1[bufplc],dist[i]);
			if (um <= h)
			{
				c = palookup[(((shade>>1)-(i<<2))<<8)+c1[bufplc]];
				if (h > dm) break;
				plc1 = drawtopslab(plc1,h-um+1,c);
				um = h+1;
			}
		}

		if (h2[bufplc] < oh2)
		{
			h = groudiv((long)h2[bufplc],dist[i]);
			if (dm >= h)
			{
				c = palookup[(((shade>>1)+(i<<2))<<8)+c2[bufplc]];
				if (h < um) break;
				plc2 = drawbotslab(plc2,dm-h+1,c);
				dm = h-1;
			}
		}

		dist[i] += dinc[i];
		cnt += incr[i];
		shade++;
	}

	if (dm >= um)
	{
		if (shade >= scandist-9) c = palookup[(numpalookups-1)<<8];
		drawtopslab(plc1,dm-um+1,c);
	}
}

void main ()
{
	char blastcol;
	long i, j, templong;

	pixs = 2;
	vidmode = 0;
	xdim = 320;
	ydim = 200;
	blastcol = 0;

	setscreenmode();
	loadtables();
	loadboard();

	clockspeed = 0L;
	totalclock = 0L;
	numframes = 0L;
	outp(0x43,54); outp(0x40,4972&255); outp(0x40,4972>>8);

	blast(((posx>>10)&255),((posy>>10)&255),8L,blastcol);

	while (keystatus[1] == 0)
	{
		for(i=0;i<xdim;i+=pixs)
			grouvline((short)i,128L);                 //Draw to non-video memory

		if (vidmode == 0)                            //Copy to screen
		{
			if (pixs == 4) showscreen4pix320200();
			if (pixs == 2) showscreen2pix320200();
			if (pixs == 1) showscreen1pix320200();
		}
		else
		{
			if (pixs == 4) showscreen4pix320400();
			if (pixs == 2) showscreen2pix320400();
			if (pixs == 1) showscreen1pix320400();
		}

		if (vidmode == 0)
		{
			pageoffset += 16384;
			if (pageoffset >= 0xb0000) pageoffset = 0xa0000;
		}
		else
		{
			pageoffset += 32768;
			if (pageoffset > 0xa8000) pageoffset = 0xa0000;
		}

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

		vel = 0L;
		svel = 0L;
		angvel = 0;

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

		if (keystatus[0x10] > 0) keystatus[0x10] = 0, pixs = 1;
		if (keystatus[0x11] > 0) keystatus[0x11] = 0, pixs = 2;
		if (keystatus[0x12] > 0) keystatus[0x12] = 0, pixs = 4;
		if ((keystatus[0x1f]|keystatus[0x20]) > 0)
		{
			if (keystatus[0x1f] > 0)
			{
				if (vidmode == 0)
				{
					pageoffset = 0xa0000;
					vidmode = 1;
					outp(0x3d4,0x9); outp(0x3d5,inp(0x3d5)&254);
					keystatus[0x1f] = 0;
					ydim = 400L;
					horiz <<= 1;
				}
			}
			if (keystatus[0x20] > 0)
			{
				if (vidmode == 1)
				{
					vidmode = 0;
					outp(0x3d4,0x9); outp(0x3d5,inp(0x3d5)|1);
					keystatus[0x20] = 0;
					ydim = 200L;
					horiz >>= 1;
				}
			}
		}

		numframes++;
		totalclock += clockspeed;
		clockspeed = 0L;
	}
	outp(0x43,54); outp(0x40,255); outp(0x40,255);
	if (totalclock != 0)
	{
		templong = (numframes*24000L)/totalclock;
		printf("%d.%1d%1d frames per second\n",(short int)(templong/100),(short int)((templong/10)%10),(short int)(templong%10));
	}
}


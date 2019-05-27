#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <sys\types.h>
#include <sys\stat.h>

//04/29/2005: Fixed warnings in OpenWatcom 1.3. Fixed move&look up/down speed.
//10/08/2002: Added screen capture to X.PNG using F12.
//   Last touch before today was:
//      CAVE.C 11/17/1994 03:22 AM 15,722
//   Looks like I ported GROUCAVE.BAS->CAVE.C around 04/20/1994 to 04/21/1994

#define MAXPALOOKUPS 64

long posx, posy, posz, horiz, xdim, ydim;
long newposz, vel, svel, angvel, pageoffset;
short ang, pixs, vidmode, detmode;
short moustat, mousx, mousy;

char h1[65536], c1[65536];
char h2[65536], c2[65536];
short sintable[2048], startumost[320], startdmost[320];
char scrbuf[128000];
short numpalookups;
unsigned char palookup[MAXPALOOKUPS<<8], palette[768];

volatile char keystatus[256], readch, oldreadch, extended;
volatile long clockspeed, totalclock, numframes;
void (__interrupt __far *oldtimerhandler)();
void __interrupt __far timerhandler(void);
void (__interrupt __far *oldkeyhandler)();
void __interrupt __far keyhandler(void);

void setvmode (long);
#pragma aux setvmode =\
	"int 0x10"\
	parm [eax]

void drawpixel (long, long);
#pragma aux drawpixel =\
	"mov byte ptr [edi], al"\
	parm [edi][eax]

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

void showscreen4pix320200 ();
#pragma aux showscreen4pix320200 =\
	"mov dx, 0x3c4"\
	"mov ax, 0x0f02"\
	"out dx, ax"\
	"mov ecx, 4000"\
	"mov esi, offset scrbuf"\
	"mov edi, pageoffset"\
	"rep movsd"\
	modify [eax ecx edx esi edi]

void showscreen4pix320400 ();
#pragma aux showscreen4pix320400 =\
	"mov dx, 0x3c4"\
	"mov ax, 0x0f02"\
	"out dx, ax"\
	"mov ecx, 8000"\
	"mov esi, offset scrbuf"\
	"mov edi, pageoffset"\
	"rep movsd"\
	modify [eax ecx edx esi edi]

void showscreen2pix320200 ();
#pragma aux showscreen2pix320200 =\
	"mov dx, 0x3c4"\
	"mov ax, 0x0302"\
	"out dx, ax"\
	"mov ecx, 4000"\
	"mov esi, offset scrbuf"\
	"mov edi, pageoffset"\
	"rep movsd"\
	"mov ax, 0x0c02"\
	"out dx, ax"\
	"mov ecx, 4000"\
	"mov edi, pageoffset"\
	"rep movsd"\
	modify [eax ecx edx esi edi]

void showscreen2pix320400 ();
#pragma aux showscreen2pix320400 =\
	"mov dx, 0x3c4"\
	"mov ax, 0x0302"\
	"out dx, ax"\
	"mov ecx, 8000"\
	"mov esi, offset scrbuf"\
	"mov edi, pageoffset"\
	"rep movsd"\
	"mov ax, 0x0c02"\
	"out dx, ax"\
	"mov ecx, 8000"\
	"mov edi, pageoffset"\
	"rep movsd"\
	modify [eax ecx edx esi edi]

void showscreen1pix320200 ();
#pragma aux showscreen1pix320200 =\
	"mov dx, 0x3c4"\
	"mov ax, 0x0102"\
	"out dx, ax"\
	"mov ecx, 4000"\
	"mov esi, offset scrbuf"\
	"mov edi, pageoffset"\
	"rep movsd"\
	"mov ax, 0x0202"\
	"out dx, ax"\
	"mov ecx, 4000"\
	"mov edi, pageoffset"\
	"rep movsd"\
	"mov ax, 0x0402"\
	"out dx, ax"\
	"mov ecx, 4000"\
	"mov edi, pageoffset"\
	"rep movsd"\
	"mov ax, 0x0802"\
	"out dx, ax"\
	"mov ecx, 4000"\
	"mov edi, pageoffset"\
	"rep movsd"\
	modify [eax ecx edx esi edi]

void showscreen1pix320400 ();
#pragma aux showscreen1pix320400 =\
	"mov dx, 0x3c4"\
	"mov ax, 0x0102"\
	"out dx, ax"\
	"mov ecx, 8000"\
	"mov esi, offset scrbuf"\
	"mov edi, pageoffset"\
	"rep movsd"\
	"mov ax, 0x0202"\
	"out dx, ax"\
	"mov ecx, 8000"\
	"mov edi, pageoffset"\
	"rep movsd"\
	"mov ax, 0x0402"\
	"out dx, ax"\
	"mov ecx, 8000"\
	"mov edi, pageoffset"\
	"rep movsd"\
	"mov ax, 0x0802"\
	"out dx, ax"\
	"mov ecx, 8000"\
	"mov edi, pageoffset"\
	"rep movsd"\
	modify [eax ecx edx esi edi]

void setupmouse ();
#pragma aux setupmouse =\
	"mov ax, 0"\
	"int 33h"\
	"mov moustat,1"

void readmouse ();
#pragma aux readmouse =\
	"mov ax, 11d"\
	"int 33h"\
	"sar cx, 1"\
	"sar dx, 1"\
	"mov mousx, cx"\
	"mov mousy, dx"\
	modify [ax cx dx]

void interruptend ();
#pragma aux interruptend =\
	"mov al, 20h"\
	"out 20h, al"\
	modify [eax]

void readkey ();
#pragma aux readkey =\
	"in al, 0x60"\
	"mov readch, al"\
	"in al, 0x61"\
	"or al, 0x80"\
	"out 0x61, al"\
	"and al, 0x7f"\
	"out 0x61, al"\
	modify [eax]

void __interrupt __far timerhandler()
{
	clockspeed++;
	interruptend();
}

void __interrupt __far keyhandler()
{
	oldreadch = readch;
	readkey();
	if ((readch|1) == 0xe1)
		extended = 128;
	else
	{
		if (oldreadch != readch)
			keystatus[(readch&127)+extended] = ((readch>>7)^1);
		extended = 0;
	}
	interruptend();
}

//------------------------ Simple PNG OUT code begins ------------------------
FILE *pngofil;
long pngoxplc, pngoyplc, pngoxsiz, pngoysiz;
unsigned long pngocrc, pngoadcrc;

long bswap (long);
#pragma aux bswap =\
	".586" "bswap eax"\
	parm [eax] modify nomemory exact [eax] value [eax]

long crctab32[256];  //SEE CRC32.C
#define updatecrc32(c,crc) crc=(crctab32[(crc^c)&255]^(((unsigned)crc)>>8))
#define updateadl32(c,crc) \
{  c += (crc&0xffff); if (c   >= 65521) c   -= 65521; \
	crc = (crc>>16)+c; if (crc >= 65521) crc -= 65521; \
	crc = (crc<<16)+c; \
} \

void fputbytes (unsigned long v, long n)
	{ for(;n;v>>=8,n--) { fputc(v,pngofil); updatecrc32(v,pngocrc); } }

void pngoutopenfile (char *fnam, long xsiz, long ysiz)
{
	long i, j, k;
	char a[40];

	pngoxsiz = xsiz; pngoysiz = ysiz; pngoxplc = pngoyplc = 0;
	for(i=255;i>=0;i--)
	{
		k = i; for(j=8;j;j--) k = ((unsigned long)k>>1)^((-(k&1))&0xedb88320);
		crctab32[i] = k;
	}
	pngofil = fopen(fnam,"wb");
	*(long *)&a[0] = 0x474e5089; *(long *)&a[4] = 0x0a1a0a0d;
	*(long *)&a[8] = 0x0d000000; *(long *)&a[12] = 0x52444849;
	*(long *)&a[16] = bswap(xsiz); *(long *)&a[20] = bswap(ysiz);
	*(long *)&a[24] = 0x00000208; *(long *)&a[28] = 0;
	for(i=12,j=-1;i<29;i++) updatecrc32(a[i],j);
	*(long *)&a[29] = bswap(j^-1);
	fwrite(a,37,1,pngofil);
	pngocrc = -1; pngoadcrc = 1;
	fputbytes(0x54414449,4); fputbytes(0x0178,2);
}

void pngoutputpixel (long rgbcol)
{
	long a[4];

	if (!pngoxplc)
	{
		fputbytes(pngoyplc==pngoysiz-1,1);
		fputbytes(((pngoxsiz*3+1)*0x10001)^0xffff0000,4);
		fputbytes(0,1); a[0] = 0; updateadl32(a[0],pngoadcrc);
	}
	fputbytes(bswap(rgbcol<<8),3);
	a[0] = (rgbcol>>16)&255; updateadl32(a[0],pngoadcrc);
	a[0] = (rgbcol>> 8)&255; updateadl32(a[0],pngoadcrc);
	a[0] = (rgbcol    )&255; updateadl32(a[0],pngoadcrc);
	pngoxplc++; if (pngoxplc < pngoxsiz) return;
	pngoxplc = 0; pngoyplc++; if (pngoyplc < pngoysiz) return;
	fputbytes(bswap(pngoadcrc),4);
	a[0] = bswap(pngocrc^-1); a[1] = 0; a[2] = 0x444e4549; a[3] = 0x826042ae;
	fwrite(a,1,16,pngofil);
	a[0] = bswap(ftell(pngofil)-(33+8)-16);
	fseek(pngofil,33,SEEK_SET); fwrite(a,1,4,pngofil);
	fclose(pngofil);
}
//------------------------- Simple PNG OUT code ends -------------------------

void main ()
{
	char blastcol;
	long i, j, templong;

	pixs = 2;
	vidmode = 0;
	xdim = 320;
	ydim = 200;
	pageoffset = 0xa0000;
	blastcol = 0;
	detmode = 0;

	setscreenmode();
	loadtables();
	loadboard();

	setupmouse();
	oldkeyhandler = _dos_getvect(0x9);
	_disable(); _dos_setvect(0x9, keyhandler); _enable();
	clockspeed = 0L;
	totalclock = 0L;
	numframes = 0L;
	outp(0x43,54); outp(0x40,4972&255); outp(0x40,4972>>8);
	oldtimerhandler = _dos_getvect(0x8);
	_disable(); _dos_setvect(0x8, timerhandler); _enable();

	for(i=0;i<xdim;i++)
	{
		startumost[i] = 0;
		startdmost[i] = ydim-1;
	}

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

		if (keystatus[0x58]) //F12
		{
			long x, y, xx, yy;
			xx = 320/pixs; yy = (vidmode+1)*200;
			if (pixs == 1) j = (vidmode+1)*16000;
			if (pixs == 2) j = (vidmode+1)*8000;
			if (pixs == 4) j = (vidmode+1)*4000;
			pngoutopenfile("x.png",xx,yy);
			for(y=0;y<yy;y++)
				for(x=0;x<320;x+=pixs)
				{
					i = ((long)scrbuf[y*80+(x>>2)+(x&3)*j])*3;
					pngoutputpixel((((long)palette[i  ])<<18)+
										(((long)palette[i+1])<<10)+
										(((long)palette[i+2])<< 2));
				}
		}

		outp(0x3d4,0xc); outp(0x3d5,(pageoffset&65535)>>8);  //Nextpage
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
		if (moustat == 1)
		{
			readmouse();
			ang += mousx;
			vel = (((long)-mousy)<<3);
		}
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

		if (detmode == 0)
		{
			if ((vel|svel|angvel) == 0)
				pixs = 1;
			else
				pixs = 2;
		}
		if (keystatus[0x10] > 0) keystatus[0x10] = 0, pixs = 1;
		if (keystatus[0x11] > 0) keystatus[0x11] = 0, pixs = 2;
		if (keystatus[0x12] > 0) keystatus[0x12] = 0, pixs = 4;
		if (keystatus[0x13] > 0) keystatus[0x13] = 0, detmode = 1-detmode;
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
			for(i=0;i<xdim;i++)
			{
				startumost[i] = 0;
				startdmost[i] = ydim-1;
			}
		}

		numframes++;
		totalclock += clockspeed;
		clockspeed = 0L;
	}
	outp(0x43,54); outp(0x40,255); outp(0x40,255);
	_dos_setvect(0x8, oldtimerhandler);
	_dos_setvect(0x9, oldkeyhandler);
	setvmode(0x3);
	if (totalclock != 0)
	{
		templong = (numframes*24000L)/totalclock;
		printf("%d.%1d%1d frames per second\n",(short int)(templong/100),(short int)((templong/10)%10),(short int)(templong%10));
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

void setscreenmode ()
{
	long i, fil;

	setvmode(0x13);
	outp(0x3c4,0x4); outp(0x3c5,0x6);
	outp(0x3d4,0x14); outp(0x3d5,0x0);
	outp(0x3d4,0x17); outp(0x3d5,0xe3);
	if (ydim == 400)
	{
		outp(0x3d4,0x9); outp(0x3d5,inp(0x3d5)&254);
	}

	if ((fil = open("palette.dat",O_BINARY|O_RDWR,S_IREAD)) == -1)
	{
		printf("Can't load palette.dat.  Now why could that be?\n");
		exit(0);
	}
	read(fil,&palette[0],768);
	read(fil,&numpalookups,2);
	read(fil,&palookup[0],numpalookups<<8);
	close(fil);

	outp(0x3c8,0);
	for(i=0;i<768;i++)
		outp(0x3c9,palette[i]);
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

	if (startumost[x] > startdmost[x])
		return;

	switch(pixs)
	{
		case 1:
			plc1 = startumost[x]*80+(x>>2)+FP_OFF(scrbuf);
			plc2 = startdmost[x]*80+(x>>2)+FP_OFF(scrbuf);
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
			plc1 = startumost[x]*80+(x>>2)+FP_OFF(scrbuf);
			plc2 = startdmost[x]*80+(x>>2)+FP_OFF(scrbuf);
			if ((x&2) > 0)
			{
				plc1 += 16000*(vidmode+1);
				plc2 += 16000*(vidmode+1);
			}
			break;
		case 4:
			plc1 = startumost[x]*80+(x>>2)+FP_OFF(scrbuf);
			plc2 = startdmost[x]*80+(x>>2)+FP_OFF(scrbuf);
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

	um = startumost[x]-horiz;
	dm = startdmost[x]-horiz;

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

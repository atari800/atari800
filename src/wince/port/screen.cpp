/* (C) 2001  Vasyl Tsvirkunov */
/* Based on Win32 port by  Krzysztof Nikiel */

#include <windows.h>
#include "gx.h"

extern "C"
{
	/* These need C name mangling */
#include "screen.h"
#include "main.h"
#include "colours.h"
#include "ui.h"
#include "keyboard.h"
};

#define SMOOTH

#define MAX_CLR         0x100
#ifdef SMOOTH
static UBYTE palRed[MAX_CLR];
static UBYTE palGreen[MAX_CLR];
static UBYTE palBlue[MAX_CLR];
#endif
static unsigned short pal[MAX_CLR];


GXDisplayProperties gxdp;
int active;
int kbd_image_ok = 0; /* flag to prevent extra redraws of non-overlay keyboard */

struct tScreenGeometry
{
	long width;
	long height;
	long startoffset;
	long sourceoffset;
	long linestep;
	long pixelstep;
	long xSkipMask;
	long xLimit;
	long lineLimit;
};

tScreenGeometry geom[3];

int currentScreenMode = 0;
int useMode = 0;
int maxMode = 2;

void set_screen_mode(int mode)
{
	currentScreenMode = mode;
	if(currentScreenMode > maxMode)
		currentScreenMode = 0;
	
	if(currentScreenMode == 0)
		currentKeyboardMode = 4;
	
	kbd_image_ok = 0;
}

int get_screen_mode()
{
	return currentScreenMode;
}

void gr_suspend()
{
	if(active)
	{
		active = 0;
		GXSuspend();
	}
}

void gr_resume()
{
	if(!active)
	{
		active = 1;
		GXResume();
	}
	kbd_image_ok = 0;
}

void groff(void)
{
	GXCloseDisplay();
	active = 0;
}

int gron(int *argc, char *argv[])
{
	GXOpenDisplay(hWndMain, GX_FULLSCREEN);
	
	gxdp = GXGetDisplayProperties();
	if((gxdp.ffFormat & (kfDirect555 | kfDirect565)) == 0 || gxdp.cxWidth < 240 || gxdp.cyHeight < 240)
	{
		groff();
		return 1;
	}
	
	// portrait
	geom[0].width = gxdp.cxWidth; // 240
	geom[0].height = gxdp.cyHeight; // 320
	geom[0].startoffset = 0;
	geom[0].sourceoffset = 8;
	geom[0].linestep = gxdp.cbyPitch;
	geom[0].pixelstep = gxdp.cbxPitch;
	geom[0].xSkipMask = gxdp.cxWidth < 320 ? 0x00000003 : 0xffffffff;
	geom[0].xLimit = 320; // skip 1/4
	geom[0].lineLimit = ATARI_WIDTH*240;
	
	// left handed landscape
	geom[1].width = gxdp.cyHeight; // 320
	geom[1].height = gxdp.cxWidth; // 240
	geom[1].startoffset = gxdp.cbyPitch*(gxdp.cyHeight-1);
	geom[1].sourceoffset = 8;
	geom[1].linestep = gxdp.cbxPitch;
	geom[1].pixelstep = -gxdp.cbyPitch;
	geom[1].xSkipMask = 0xffffffff;
	geom[1].xLimit = 320; // no skip
	geom[1].lineLimit = ATARI_WIDTH*240;
	
	// right handed landscape
	geom[2].width = gxdp.cyHeight; // 320
	geom[2].height = gxdp.cxWidth; // 240
	geom[2].startoffset = gxdp.cbxPitch*(gxdp.cxWidth-1);
	geom[2].sourceoffset = 8;
	geom[2].linestep = -gxdp.cbxPitch;
	geom[2].pixelstep = gxdp.cbyPitch;
	geom[2].xSkipMask = 0xffffffff;
	geom[2].xLimit = 320; // no skip
	geom[2].lineLimit = ATARI_WIDTH*240;
	
	if(gxdp.cyHeight < 320)
		maxMode = 0; // portrait only!

	for(int i = 0; i < MAX_CLR; i++)
	{
		palette(i,  (colortable[i] >> 16) & 0xff,
			(colortable[i] >> 8) & 0xff,
			(colortable[i]) & 0xff);
	}
	
	active = 1;
	kbd_image_ok = 0;
	return 0;
}

void palette(int ent, UBYTE r, UBYTE g, UBYTE b)
{
	if (ent >= MAX_CLR)
		return;
#ifdef SMOOTH
	palRed[ent] = r;
	palGreen[ent] = g;
	palBlue[ent] = b;
#endif
	if(gxdp.ffFormat & kfDirect565)
		pal[ent] = ((r&0xf8)<<(11-3))|((g&0xfc)<<(5-2))|((b&0xf8)>>3);
	else if(gxdp.ffFormat & kfDirect555)
		pal[ent] = ((r&0xf8)<<(10-3))|((g&0xf8)<<(5-2))|((b&0xf8)>>3);
}

void cls()
{
	int x, y;
	UBYTE* dst;
	UBYTE *scraddr;
	scraddr = (UBYTE*)GXBeginDraw();
	if(scraddr)
	{
		for(y=0; y<geom[useMode].height; y++)
		{
			dst = scraddr+geom[useMode].startoffset;
			for(x=0; x<geom[useMode].width; x++)
			{
				*(unsigned short*)dst = 0;
				dst += geom[useMode].pixelstep;
			}
			scraddr += geom[useMode].linestep;
		}
		GXEndDraw();
	}
	kbd_image_ok = 0;
}

int counter = 0;
void overlay_kbd(UBYTE* scr_ptr);
void draw_kbd(UBYTE* scraddr);

void refreshv(UBYTE * scr_ptr)
{
	static UBYTE *src;
	static UBYTE *dst;
	static UBYTE *scraddr;
	static UBYTE *scr_ptr_limit;
	static UBYTE *src_limit;
	static long pixelstep;
	static long linestep;
	static long skipmask;

#ifdef SMOOTH
	static bool b565 = (gxdp.ffFormat & kfDirect565);
#endif
	
	if(!active)
	{
		Sleep(100);
#ifndef MULTITHREADED
		MsgPump();
#endif
		return;
	}
	
	/* Update screen mode, also thread protection by doing this */
	if(useMode != currentScreenMode)
	{
		useMode = currentScreenMode;
		cls();
	}
	
	/* Ensure keyboard flag consistency */
	if(currentScreenMode == 0)
		currentKeyboardMode = 4;
	
	/* Overlay keyboard in landscape modes */
	if(currentKeyboardMode != 4)
		overlay_kbd(scr_ptr);
	
	scraddr = (UBYTE*)GXBeginDraw();
	if(scraddr)
	{
		if(useMode == 0)
			draw_kbd(scraddr);
		
		scraddr += geom[useMode].startoffset;
		scr_ptr += geom[useMode].sourceoffset;
		scr_ptr_limit = scr_ptr + geom[useMode].lineLimit;
		pixelstep = geom[useMode].pixelstep;
		linestep = geom[useMode].linestep;
		src_limit = scr_ptr + geom[useMode].xLimit;
		skipmask = geom[useMode].xSkipMask;
		
		/* Internal pixel loops */
#ifdef SMOOTH
		if(skipmask == 3)
		{
			while(scr_ptr < scr_ptr_limit)
			{
				src = scr_ptr;
				dst = scraddr;
				while(src < src_limit)
				{
					UBYTE r, g, b;
					r = (3*palRed[*(src+0)] + palRed[*(src+1)])>>2;
					g = (3*palGreen[*(src+0)] + palGreen[*(src+1)])>>2;
					b = (3*palBlue[*(src+0)] + palBlue[*(src+1)])>>2;

					if(b565)
						*(unsigned short*)dst = ((r&0xf8)<<(11-3))|((g&0xfc)<<(5-2))|((b&0xf8)>>3);
					else
						*(unsigned short*)dst = ((r&0xf8)<<(10-3))|((g&0xf8)<<(5-2))|((b&0xf8)>>3);
					dst += pixelstep;

					r = (palRed[*(src+1)] + palRed[*(src+2)])>>1;
					g = (palGreen[*(src+1)] + palGreen[*(src+2)])>>1;
					b = (palBlue[*(src+1)] + palBlue[*(src+2)])>>1;

					if(b565)
						*(unsigned short*)dst = ((r&0xf8)<<(11-3))|((g&0xfc)<<(5-2))|((b&0xf8)>>3);
					else
						*(unsigned short*)dst = ((r&0xf8)<<(10-3))|((g&0xf8)<<(5-2))|((b&0xf8)>>3);
					dst += pixelstep;

					r = (palRed[*(src+2)] + 3*palRed[*(src+3)])>>2;
					g = (palGreen[*(src+2)] + 3*palGreen[*(src+3)])>>2;
					b = (palBlue[*(src+2)] + 3*palBlue[*(src+3)])>>2;

					if(b565)
						*(unsigned short*)dst = ((r&0xf8)<<(11-3))|((g&0xfc)<<(5-2))|((b&0xf8)>>3);
					else
						*(unsigned short*)dst = ((r&0xf8)<<(10-3))|((g&0xf8)<<(5-2))|((b&0xf8)>>3);
					dst += pixelstep;

					src += 4;
				}
				scraddr += linestep;
				scr_ptr += ATARI_WIDTH;
				src_limit += ATARI_WIDTH;
			}
		}
		else
#endif
		if(skipmask != 0xffffffff)
		{
			while(scr_ptr < scr_ptr_limit)
			{
				src = scr_ptr;
				dst = scraddr;
				while(src < src_limit)
				{
					if((long)src & skipmask)
					{
						*(unsigned short*)dst = pal[*src];
						dst += pixelstep;
					}
					src ++;
				}
				scraddr += linestep;
				scr_ptr += ATARI_WIDTH;
				src_limit += ATARI_WIDTH;
			}
		}
		else
		{
			while(scr_ptr < scr_ptr_limit)
			{
				src = scr_ptr;
				dst = scraddr;
				while(src < src_limit)
				{
					*(unsigned short*)dst = pal[*src];
					dst += pixelstep;
					src ++;
				}
				scraddr += linestep;
				scr_ptr += ATARI_WIDTH;
				src_limit += ATARI_WIDTH;
			}
		}
		
		GXEndDraw();
	}
}

void refresh_kbd()
{
	static UBYTE *scraddr;
	scraddr = (UBYTE*)GXBeginDraw();
	if(scraddr)
	{
		draw_kbd(scraddr);
		GXEndDraw();
	}
}

/*
Overlay keyboard image on top of generated Atari screen. The following
assumptions are made: at least 320x240 portion of the screen will be
rendered; typical crop on the left is 8 pixels; keyboard image is 240x80
packed bitmap. pos denotes screen corner, 0 to 3. Color index 15 is used,
it is the brightest color in the palette.
*/
void overlay_kbd(UBYTE* scr_ptr)
{
	static ULONG* kbd_image_ptr;
	static unsigned long curBit;
	static BYTE color;
	static UBYTE* scr_ptr_limit;
	static UBYTE* dest;
	static UBYTE* dest_limit;
	
	if(currentKeyboardColor)
		color = 0;
	else
		color = 15;
	
	scr_ptr += 8;
	if(currentKeyboardMode & 1)
		scr_ptr += (320-240); /* offset horizontally */
	if(currentKeyboardMode & 2)
		scr_ptr += (240-80)*ATARI_WIDTH; /* offset vertically */
	
	scr_ptr_limit = scr_ptr + 80*ATARI_WIDTH;
	dest_limit = scr_ptr + 240;
	
	kbd_image_ptr = kbd_image; /* dword pointer in keyboard bitmap */
	curBit = 1;  /* rotating bit pointer */
	while(scr_ptr < scr_ptr_limit)
	{
		dest = scr_ptr;
		while(dest < dest_limit)
		{
			if(*kbd_image_ptr & curBit)
				*dest = color;
			dest ++;
			curBit <<= 1;
			if(curBit == 0) /* next dword? */
			{
				kbd_image_ptr ++;
				curBit = 1;
			}
		}
		/* Lines are dword aligned */
		if(curBit != 1)
		{
			kbd_image_ptr ++;
			curBit = 1;
		}
		scr_ptr += ATARI_WIDTH;
		dest_limit += ATARI_WIDTH;
	}
}

/*
Draw keyboard directly on the screen in portrait mode. Assumptions are
that the screen is 240x320, 16 bit surface; keyboard is the same as in
overlay.
*/

void draw_kbd(UBYTE* scraddr)
{
	static ULONG* kbd_image_ptr;
	static unsigned long curBit;
	static int linestep, pixelstep;
	static int colorOn, colorOff;
	static UBYTE* scraddr_limit;
	static UBYTE* dest;
	static UBYTE* dest_limit;
	static int cachedcolor=-1; /* must be static */
	static ULONG* cachedkbd=NULL;
	
	if(cachedcolor != currentKeyboardColor || cachedkbd != kbd_image)
	{
		kbd_image_ok = 0;
		cachedcolor = currentKeyboardColor;
		cachedkbd = kbd_image;
	}
	
	if(kbd_image_ok)
		return;
	
	if(currentKeyboardColor)
	{
		colorOn = 0x0000;
		colorOff = 0xffff;
	}
	else
	{
		colorOn = 0xffff;
		colorOff = 0x0000;
	}
	
	linestep = geom[0].linestep;
	pixelstep = geom[0].pixelstep;
	
	scraddr += geom[0].startoffset + (geom[0].height-80)*linestep;
	scraddr_limit = scraddr + linestep*80; /* note that linestep may be negative */
	dest_limit = scraddr + pixelstep*240; /* again, pixelstep may be negative */
	
	kbd_image_ptr = kbd_image; /* word pointer in keyboard bitmap */
	curBit = 1;  /* rotating bit pointer */
	while(scraddr != scraddr_limit) /* note '!=' */
	{
		dest = scraddr;
		while(dest != dest_limit)
		{
			if(*kbd_image_ptr & curBit)
				*(unsigned short*)dest = colorOn;
			else
				*(unsigned short*)dest = colorOff;
			
			dest += pixelstep;
			curBit <<= 1;
			if(curBit == 0) /* next dword? */
			{
				kbd_image_ptr ++;
				curBit = 1;
			}
		}
		
		/* Lines are dword aligned */
		if(curBit != 1)
		{
			kbd_image_ptr ++;
			curBit = 1;
		}
		
		scraddr += linestep;
		dest_limit += linestep;
	}
	kbd_image_ok = 1;
}

void translate_kbd(short* px, short* py)
{
	int x, y;
	
	switch(currentScreenMode)
	{
	case 0: /* portrait */
		*py -= 240;
		break;
	case 1: /* landscape left */
		x = 320 - *py;
		y = *px;
		if(currentKeyboardMode & 1)
			x -= 80;
		if(currentKeyboardMode & 2)
			y -= 160;
		*px = x;
		*py = y;
		break;
	case 2: /* landscape right */
		x = *py;
		y = 240 - *px;
		if(currentKeyboardMode & 1)
			x -= 80;
		if(currentKeyboardMode & 2)
			y -= 160;
		*px = x;
		*py = y;
		break;
	}
}

/* (C) 2001,2002  Vasyl Tsvirkunov */
/* Originally based on Win32 port by  Krzysztof Nikiel */

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

int smooth_filter = 1; /* default is YES */
int filter_available = 0;
};


#define MAX_CLR         0x100
static UBYTE palRed[MAX_CLR];
static UBYTE palGreen[MAX_CLR];
static UBYTE palBlue[MAX_CLR];
static unsigned short pal[MAX_CLR];
/* First 10 and last 10 colors on palettized devices require special treatment,
   this table will map all colors into 236-color space starting at 10 */
static UBYTE staticTranslate[256];

/* Hicolor mode conversion parameters */
static int redshift = 8; /* 8 for 565 mode, 7 for 555 */
static int greenmask = 0xfc; /* 0xfc for 565, 0xf8 for 555 */
/* Monochromatic mode conversion parameters */
static UBYTE invert = 0;
static int colorscale = 0;

#define COLORCONVHICOLOR(r,g,b) ((((r)&0xf8)<<redshift)|(((g)&greenmask)<<(5-2))|(((b)&0xf8)>>3))
#define COLORCONVMONO(r,g,b) ((((3*(r)>>3)+((g)>>1)+((b)>>3))>>colorscale)^invert)

/* Using vectorized function to save on branches */
typedef void (*tCls)();
typedef void (*tRefresh)(UBYTE*);
typedef void (*tDrawKbd)(UBYTE*);

void mono_Cls();
void mono_Refresh(UBYTE*);
void mono_DrawKbd(UBYTE*);

void palette_Cls();
void palette_Refresh(UBYTE*);
void palette_DrawKbd(UBYTE*);

void hicolor_Cls();
void hicolor_Refresh(UBYTE*);
void hicolor_DrawKbd(UBYTE*);

static tCls        pCls        = NULL;
static tRefresh    pRefresh    = NULL;
static tDrawKbd    pDrawKbd    = NULL;

// Acquire screen pointer every frame?
#define FRAMEBASE

#ifdef FRAMEBASE
	#define GET_SCREEN_PTR() ((UBYTE*)GXBeginDraw())
	#define RELEASE_SCREEN() (GXEndDraw())
#else
	UBYTE* spScreen = NULL;
	#define GET_SCREEN_PTR() (spScreen)
	#define RELEASE_SCREEN()
#endif


/* */

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

	entire_screen_dirty();
}

int get_screen_mode()
{
	return currentScreenMode;
}

void gr_suspend()
{
	if(active)
	{
#ifndef FRAMEBASE
		if(spScreen)
		{
			GXEndDraw();
			spScreen = NULL;
		}
#endif
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
#ifndef FRAMEBASE
		spScreen = (UBYTE*)GXBeginDraw();
#endif
	}
	palette_update();
	kbd_image_ok = 0;
}

void groff(void)
{
#ifndef FRAMEBASE
	if(spScreen)
	{
		GXEndDraw();
		spScreen = NULL;
	}
#endif
	GXCloseDisplay();
	active = 0;
}

int gron(int *argc, char *argv[])
{
	GXOpenDisplay(hWndMain, GX_FULLSCREEN);
	
	gxdp = GXGetDisplayProperties();

	if(gxdp.ffFormat & kfDirect565)
	{
		pCls =        hicolor_Cls;
		pRefresh =    hicolor_Refresh;
		pDrawKbd =    hicolor_DrawKbd;

		redshift = 8;
		greenmask = 0xfc;

		filter_available = 1;
	}
	else if(gxdp.ffFormat & kfDirect555)
	{
		pCls =        hicolor_Cls;
		pRefresh =    hicolor_Refresh;
		pDrawKbd =    hicolor_DrawKbd;

		redshift = 7;
		greenmask = 0xf8;

		filter_available = 1;
	}
	else if((gxdp.ffFormat & kfDirect) && (gxdp.cBPP <= 8))
	{
		pCls =        mono_Cls;
		pRefresh =    mono_Refresh;
		pDrawKbd =    mono_DrawKbd;

		if(gxdp.ffFormat & kfDirectInverted)
			invert = (1<<gxdp.cBPP)-1;
		colorscale = gxdp.cBPP < 8 ? 8-gxdp.cBPP : 0;

		if(gxdp.cBPP >= 4)
			filter_available = 1;
	}
	else if(gxdp.ffFormat & kfPalette)
	{
		pCls =        palette_Cls;
		pRefresh =    palette_Refresh;
		pDrawKbd =    palette_DrawKbd;
	}


	if(!pCls || !pRefresh || !pDrawKbd || gxdp.cxWidth < 240 || gxdp.cyHeight < 240)
	{
	// I don't believe there are devices that end up here
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
	
	palette_update();

	active = 1;
	kbd_image_ok = 0;

#ifndef FRAMEBASE
	spScreen = (UBYTE*)GXBeginDraw();
#endif

	return 0;
}

void palette(int ent, UBYTE r, UBYTE g, UBYTE b)
{
	if (ent >= MAX_CLR)
		return;

	palRed[ent] = r;
	palGreen[ent] = g;
	palBlue[ent] = b;

	if(gxdp.ffFormat & (kfDirect565|kfDirect555))
		pal[ent] = COLORCONVHICOLOR(r,g,b);
	else if(gxdp.ffFormat & kfDirect)
		pal[ent] = COLORCONVMONO(r,g,b);
}

/* Find the best color match in the palette (limited to 'limit' entries) */
UBYTE best_match(UBYTE r, UBYTE g, UBYTE b, int limit)
{
	UBYTE best = 0;
	int distance = 768;
	int i, d;
	for(i=0; i<limit; i++)
	{
	/* Manhattan distance for now. Not the best but rather fast */
		d = abs(r-palRed[i])+abs(g-palGreen[i])+abs(b-palBlue[i]);
		if(d < distance)
		{
			distance = d;
			best = i;
		}
	}

	return (UBYTE)best;
}

void palette_update()
{
	int i;
	if(gxdp.ffFormat & kfPalette)
	{
		LOGPALETTE* ple = (LOGPALETTE*)malloc(sizeof(LOGPALETTE)+sizeof(PALETTEENTRY)*255);
		ple->palVersion = 0x300;
		ple->palNumEntries = 256;
		for(i=0; i<236; i++) // first 10 and last 10 belong to the system!
		{
			ple->palPalEntry[i+10].peBlue =  palBlue[i];
			ple->palPalEntry[i+10].peGreen = palGreen[i];
			ple->palPalEntry[i+10].peRed =   palRed[i];
			ple->palPalEntry[i+10].peFlags = PC_RESERVED;
		}
		HDC hDC = GetDC(hWndMain);
		GetSystemPaletteEntries(hDC, 0, 10, &(ple->palPalEntry[0]));
		GetSystemPaletteEntries(hDC, 246, 10, &(ple->palPalEntry[246]));
		HPALETTE hpal =	CreatePalette(ple);
		SelectPalette(hDC, hpal, FALSE);
		RealizePalette(hDC);
		DeleteObject((HGDIOBJ)hpal);
		ReleaseDC(hWndMain, hDC);
		free((void*)ple);

		for(i=0; i<236; i++)
			staticTranslate[i] = i+10;
		for(i=236; i<256; i++)
			staticTranslate[i] = best_match(palRed[i], palGreen[i], palBlue[i], 236)+10;
	}
}

void cls()
{
	pCls();
	entire_screen_dirty();
}

void mono_Cls()
{
	int x, y;
	UBYTE* dst;
	UBYTE *scraddr;
	int linestep, pixelstep;
	UBYTE fillcolor;

	fillcolor = (gxdp.ffFormat & kfDirectInverted) ? 0xff : 0x00;

	pixelstep = geom[0].pixelstep;
	if(pixelstep == 0)
		return;
	linestep = (pixelstep > 0) ? -1 : 1;

	scraddr = GET_SCREEN_PTR();
	if(scraddr)
	{
		for(y=0; y<geom[0].height*gxdp.cBPP/8; y++)
		{
			dst = scraddr+geom[0].startoffset;
			for(x=0; x<geom[0].width; x++)
			{
				*dst = fillcolor;
				dst += pixelstep;
			}
			scraddr += linestep;
		}
		RELEASE_SCREEN();
	}
	kbd_image_ok = 0;
}

void palette_Cls()
{
	int x, y;
	UBYTE* dst;
	UBYTE *scraddr;
	scraddr = GET_SCREEN_PTR();
	if(scraddr)
	{
		for(y=0; y<geom[useMode].height; y++)
		{
			dst = scraddr+geom[useMode].startoffset;
			for(x=0; x<geom[useMode].width; x++)
			{
				*dst = 0;
				dst += geom[useMode].pixelstep;
			}
			scraddr += geom[useMode].linestep;
		}
		RELEASE_SCREEN();
	}
	kbd_image_ok = 0;
}

void hicolor_Cls()
{
	int x, y;
	UBYTE* dst;
	UBYTE *scraddr;
	scraddr = GET_SCREEN_PTR();
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
		RELEASE_SCREEN();
	}
	kbd_image_ok = 0;
}

void refreshv(UBYTE* scr_ptr)
{
	pRefresh(scr_ptr);
}

void overlay_kbd(UBYTE* scr_ptr);

#define ADVANCE_PARTIAL(address, step) \
	bitshift += gxdp.cBPP;             \
	if(bitshift >= 8)                  \
	{                                  \
		bitshift = refbitshift;        \
		bitmask = refbitmask;          \
		address += step;               \
	}                                  \
	else                               \
		bitmask <<= gxdp.cBPP;


#define ADVANCE_REV_PARTIAL(address, step)        \
	bitshift -= gxdp.cBPP;                        \
	if(bitshift < 0)                              \
	{                                             \
		bitshift = refbitshift;                   \
		bitmask = refbitmask;                     \
		address += step;                          \
	}                                             \
	else                                          \
		bitmask >>= gxdp.cBPP;


void mono_Refresh(UBYTE * scr_ptr)
{
// Mono blit routines contain good deal of voodoo
	static UBYTE *src;
	static UBYTE *dst;
	static UBYTE *scraddr;
	static UBYTE *scr_ptr_limit;
	static UBYTE *src_limit;
	static UBYTE *dirty_ptr_line;
	static UBYTE *dirty_ptr;
	static long pixelstep;
	static long linestep;
	static long skipmask;

// Special code is used to deal with packed pixels in monochrome mode
	static UBYTE bitmask;
	static int   bitshift;
	static UBYTE refbitmask;
	static int   refbitshift;

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
		pCls();
	}
	
	/* Ensure keyboard flag consistency */
	if(currentScreenMode == 0)
		currentKeyboardMode = 4;
	
	/* Overlay keyboard in landscape modes */
	if(currentKeyboardMode != 4)
		overlay_kbd(scr_ptr);

	pixelstep = geom[useMode].pixelstep;
	linestep = geom[useMode].linestep;
	skipmask = geom[useMode].xSkipMask;

	scraddr = GET_SCREEN_PTR();
	
	if(pixelstep)
	{
	// this will work on mono iPAQ and @migo, don't know about any others
		linestep = (pixelstep > 0) ? -1 : 1;

		bitshift = refbitshift = 0;
		bitmask = refbitmask = (1<<gxdp.cBPP)-1;

		if(scraddr)
		{
			if(useMode == 0)
				pDrawKbd(scraddr);
			
			scraddr += geom[useMode].startoffset;
			scr_ptr += geom[useMode].sourceoffset;
			scr_ptr_limit = scr_ptr + geom[useMode].lineLimit;
			src_limit = scr_ptr + geom[useMode].xLimit;
			dirty_ptr_line = screen_dirty + ((ULONG)scr_ptr-(ULONG)atari_screen)/8;
			dirty_ptr;

			/* Internal pixel loops */
			if(skipmask == 3 && (smooth_filter || ui_is_active) && gxdp.cBPP >= 4)
			{
				while(scr_ptr < scr_ptr_limit)
				{
					src = scr_ptr;
					dst = scraddr;
					dirty_ptr = dirty_ptr_line;
					while(src < src_limit)
					{
						if(*dirty_ptr)
						{
							UBYTE r, g, b;
							r = (3*palRed[*(src+0)] + palRed[*(src+1)])>>2;
							g = (3*palGreen[*(src+0)] + palGreen[*(src+1)])>>2;
							b = (3*palBlue[*(src+0)] + palBlue[*(src+1)])>>2;

							*dst = (*dst & ~bitmask) | (COLORCONVMONO(r,g,b)<<bitshift);

							dst += pixelstep;

							r = (palRed[*(src+1)] + palRed[*(src+2)])>>1;
							g = (palGreen[*(src+1)] + palGreen[*(src+2)])>>1;
							b = (palBlue[*(src+1)] + palBlue[*(src+2)])>>1;

							*dst = (*dst & ~bitmask) | (COLORCONVMONO(r,g,b)<<bitshift);

							dst += pixelstep;

							r = (palRed[*(src+2)] + 3*palRed[*(src+3)])>>2;
							g = (palGreen[*(src+2)] + 3*palGreen[*(src+3)])>>2;
							b = (palBlue[*(src+2)] + 3*palBlue[*(src+3)])>>2;

							*dst = (*dst & ~bitmask) | (COLORCONVMONO(r,g,b)<<bitshift);

							dst += pixelstep;

							r = (3*palRed[*(src+4)] + palRed[*(src+5)])>>2;
							g = (3*palGreen[*(src+4)] + palGreen[*(src+5)])>>2;
							b = (3*palBlue[*(src+4)] + palBlue[*(src+5)])>>2;

							*dst = (*dst & ~bitmask) | (COLORCONVMONO(r,g,b)<<bitshift);

							dst += pixelstep;

							r = (palRed[*(src+5)] + palRed[*(src+6)])>>1;
							g = (palGreen[*(src+5)] + palGreen[*(src+6)])>>1;
							b = (palBlue[*(src+5)] + palBlue[*(src+6)])>>1;

							*dst = (*dst & ~bitmask) | (COLORCONVMONO(r,g,b)<<bitshift);

							dst += pixelstep;

							r = (palRed[*(src+6)] + 3*palRed[*(src+7)])>>2;
							g = (palGreen[*(src+6)] + 3*palGreen[*(src+7)])>>2;
							b = (palBlue[*(src+6)] + 3*palBlue[*(src+7)])>>2;

							*dst = (*dst & ~bitmask) | (COLORCONVMONO(r,g,b)<<bitshift);

							dst += pixelstep;

							*dirty_ptr = 0;
						}
						else
							dst += pixelstep*6;

						src += 8;
						dirty_ptr ++;
					}

					ADVANCE_PARTIAL(scraddr, linestep);

					scr_ptr += ATARI_WIDTH;
					src_limit += ATARI_WIDTH;
					dirty_ptr_line += ATARI_WIDTH/8;
				}
			}
			else if(skipmask == 0x00000003)
			{
				while(scr_ptr < scr_ptr_limit)
				{
					src = scr_ptr;
					dst = scraddr;
					dirty_ptr = dirty_ptr_line;
					while(src < src_limit)
					{
						if(*dirty_ptr)
						{
							*dst = ((*dst)&~bitmask)|(pal[*(src+0)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+1)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+2)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+4)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+5)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+6)]<<bitshift);
							dst += pixelstep;
							*dirty_ptr = 0;
						}
						else
							dst += pixelstep*6;

						src += 8;
						dirty_ptr ++;
					}

					ADVANCE_PARTIAL(scraddr, linestep);

					scr_ptr += ATARI_WIDTH;
					src_limit += ATARI_WIDTH;
					dirty_ptr_line += ATARI_WIDTH/8;
				}
			}
			else if(skipmask == 0xffffffff) /* sanity check */
			{
				while(scr_ptr < scr_ptr_limit)
				{
					src = scr_ptr;
					dst = scraddr;
					dirty_ptr = dirty_ptr_line;
					while(src < src_limit)
					{
						if(*dirty_ptr)
						{
							*dst = ((*dst)&~bitmask)|(pal[*(src+0)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+1)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+2)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+3)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+4)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+5)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+6)]<<bitshift);
							dst += pixelstep;
							*dst = ((*dst)&~bitmask)|(pal[*(src+7)]<<bitshift);
							dst += pixelstep;
							*dirty_ptr = 0;
						}
						else
							dst += pixelstep*8;

						src += 8;
						dirty_ptr ++;
					}

					ADVANCE_PARTIAL(scraddr, linestep);

					scr_ptr += ATARI_WIDTH;
					src_limit += ATARI_WIDTH;
					dirty_ptr_line += ATARI_WIDTH/8;
				}
			}
		}
	}
	else
	{
	// Filtering is not implemented in this mode. Not needed by current devices anyway
		pixelstep = (linestep > 0) ? 1 : -1;

		if(scraddr)
		{
			if(useMode == 0)
				pDrawKbd(scraddr);
			
			scraddr += geom[useMode].startoffset;
			scr_ptr += geom[useMode].sourceoffset;
			scr_ptr_limit = scr_ptr + geom[useMode].lineLimit;
			src_limit = scr_ptr + geom[useMode].xLimit;
			dirty_ptr_line = screen_dirty + ((ULONG)scr_ptr-(ULONG)atari_screen)/8;

			if(skipmask == 0x00000003)
			{
				if(pixelstep > 0)
				{
					bitshift = refbitshift = 8-gxdp.cBPP;
					bitmask = refbitmask = ((1<<gxdp.cBPP)-1)<<bitshift;

					while(scr_ptr < scr_ptr_limit)
					{
						src = scr_ptr;
						dst = scraddr;
						dst -= (linestep-pixelstep);
						dirty_ptr = dirty_ptr_line;
						while(src < src_limit)
						{
							if(*dirty_ptr)
							{
								*dst = ((*dst)&~bitmask)|(pal[*(src+0)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+1)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+2)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+4)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+5)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+6)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dirty_ptr = 0;
							}
							else
							{
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
							}

							src += 8;
							dirty_ptr ++;
						}

						scraddr += linestep;

						scr_ptr += ATARI_WIDTH;
						src_limit += ATARI_WIDTH;
						dirty_ptr_line += ATARI_WIDTH/8;
					}
				}
				else
				{
					bitshift = refbitshift = 0;
					bitmask = refbitmask = (1<<gxdp.cBPP)-1;

					while(scr_ptr < scr_ptr_limit)
					{
						src = scr_ptr;
						dst = scraddr;
						dirty_ptr = dirty_ptr_line;
						while(src < src_limit)
						{
							if(*dirty_ptr)
							{
								*dst = ((*dst)&~bitmask)|(pal[*(src+0)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+1)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+2)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+4)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+5)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+6)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dirty_ptr = 0;
							}
							else
							{
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
							}
							src += 8;
							dirty_ptr ++;
						}

						scraddr += linestep;

						scr_ptr += ATARI_WIDTH;
						src_limit += ATARI_WIDTH;
						dirty_ptr_line += ATARI_WIDTH/8;
					}
				}
			}
			else if(skipmask == 0xffffffff)
			{
				if(pixelstep > 0)
				{
					bitshift = refbitshift = 8-gxdp.cBPP;
					bitmask = refbitmask = ((1<<gxdp.cBPP)-1)<<bitshift;

					while(scr_ptr < scr_ptr_limit)
					{
						src = scr_ptr;
						dst = scraddr;
						dst -= (linestep-pixelstep);
						dirty_ptr = dirty_ptr_line;
						while(src < src_limit)
						{
							if(*dirty_ptr)
							{
								*dst = ((*dst)&~bitmask)|(pal[*(src+0)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+1)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+2)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+3)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+4)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+5)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+6)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+7)]<<bitshift);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								*dirty_ptr = 0;
							}
							else
							{
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
								ADVANCE_REV_PARTIAL(dst, pixelstep);
							}

							src += 8;
							dirty_ptr ++;
						}

						scraddr += linestep;

						scr_ptr += ATARI_WIDTH;
						src_limit += ATARI_WIDTH;
						dirty_ptr_line += ATARI_WIDTH/8;
					}
				}
				else
				{
					bitshift = refbitshift = 0;
					bitmask = refbitmask = (1<<gxdp.cBPP)-1;

					while(scr_ptr < scr_ptr_limit)
					{
						src = scr_ptr;
						dst = scraddr;
						dirty_ptr = dirty_ptr_line;
						while(src < src_limit)
						{
							if(*dirty_ptr)
							{
								*dst = ((*dst)&~bitmask)|(pal[*(src+0)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+1)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+2)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+3)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+4)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+5)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+6)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dst = ((*dst)&~bitmask)|(pal[*(src+7)]<<bitshift);
								ADVANCE_PARTIAL(dst, pixelstep);
								*dirty_ptr = 0;
							}
							else
							{
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
								ADVANCE_PARTIAL(dst, pixelstep);
							}
							src += 8;
							dirty_ptr ++;
						}

						scraddr += linestep;

						scr_ptr += ATARI_WIDTH;
						src_limit += ATARI_WIDTH;
						dirty_ptr_line += ATARI_WIDTH/8;
					}
				}
			}
		}
	}
	RELEASE_SCREEN();
}


void palette_Refresh(UBYTE* scr_ptr)
{
/* Mode-specific metrics */
	static long pixelstep = geom[useMode].pixelstep;
	static long linestep  = geom[useMode].linestep;
	static long low_limit = geom[useMode].sourceoffset+24;
	static long high_limit = low_limit + geom[useMode].xLimit;

/* Addressing withing screen_dirty array. This implementation assumes that the
   array does not get reallocated in runtime */
	static UBYTE* screen_dirty_ptr;
	static UBYTE* screen_dirty_limit = screen_dirty + ATARI_HEIGHT*ATARI_WIDTH/8;

/* Source base pointer */
	static UBYTE* screen_line_ptr;
/* Source/destination pixel offset */
	static long xoffset;

/* Destination base pointer */
	static UBYTE* dest_line_ptr;

/* Running source and destination pointers */
	static UBYTE* src_ptr;
	static UBYTE* src_ptr_next;
	static UBYTE* dest_ptr;

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

		entire_screen_dirty();

		pixelstep = geom[useMode].pixelstep;
		linestep = geom[useMode].linestep;
		low_limit = geom[useMode].sourceoffset+24;
		high_limit = low_limit + geom[useMode].xLimit;
	}
	
/* Ensure keyboard flag consistency */
	if(currentScreenMode == 0)
		currentKeyboardMode = 4;
	
/* Overlay keyboard in landscape modes */
	if(currentKeyboardMode != 4)
		overlay_kbd(scr_ptr);

	dest_line_ptr = GET_SCREEN_PTR();
	if(dest_line_ptr)
	{
		if(useMode == 0)
			pDrawKbd(dest_line_ptr);
		
		screen_dirty_ptr = screen_dirty;
		screen_line_ptr = (UBYTE*)atari_screen;
		xoffset = 0;

	/* Offset destination pointer to allow for rotation and particular screen geometry */
		dest_line_ptr += geom[useMode].startoffset;

	/* There are multiple versions of the internal loop based on screen geometry and settings */
		if(geom[currentScreenMode].xSkipMask == 0xffffffff)
		{
		/* Regular 1:1 copy, with color conversion */
			while(screen_dirty_ptr < screen_dirty_limit)
			{
				if(*screen_dirty_ptr)
				{
					if(xoffset >= low_limit && xoffset < high_limit)
					{
						dest_ptr = dest_line_ptr + (xoffset-low_limit)*pixelstep;
						src_ptr = screen_line_ptr + xoffset;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						*dest_ptr = staticTranslate[*src_ptr];
					}
					*screen_dirty_ptr = 0;
				}

				xoffset += 8; /* One screen_dirty item corresponds to 8 pixels */
				if(xoffset >= ATARI_WIDTH) /* move to the next line */
				{
					xoffset = 0;
					screen_line_ptr += ATARI_WIDTH;
					dest_line_ptr += linestep;
				}

				screen_dirty_ptr ++;
			}
		}
		else
		{
		/* 3/4 pixel skip version. Filtering is not supported for paletted mode */
			while(screen_dirty_ptr < screen_dirty_limit)
			{
				if(*screen_dirty_ptr)
				{
					if(xoffset >= low_limit && xoffset < high_limit)
					{
						dest_ptr = dest_line_ptr + (xoffset-low_limit)*3/4*pixelstep;
						src_ptr = screen_line_ptr + xoffset;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						src_ptr ++;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						*dest_ptr = staticTranslate[*src_ptr++]; dest_ptr += pixelstep;
						*dest_ptr = staticTranslate[*src_ptr];
					}
					*screen_dirty_ptr = 0;
				}

				xoffset += 8; /* One screen_dirty item corresponds to 8 pixels */
				if(xoffset >= ATARI_WIDTH) /* move to the next line */
				{
					xoffset = 0;
					screen_line_ptr += ATARI_WIDTH;
					dest_line_ptr += linestep;
				}

				screen_dirty_ptr ++;
			}
		}

		RELEASE_SCREEN();
	}
}

void hicolor_Refresh(UBYTE* scr_ptr)
{
/* Mode-specific metrics */
	static long pixelstep = geom[useMode].pixelstep;
	static long linestep  = geom[useMode].linestep;
	static long low_limit = geom[useMode].sourceoffset+24;
	static long high_limit = low_limit + geom[useMode].xLimit;

/* Addressing withing screen_dirty array. This implementation assumes that the
   array does not get reallocated in runtime */
	static UBYTE* screen_dirty_ptr;
	static UBYTE* screen_dirty_limit = screen_dirty + ATARI_HEIGHT*ATARI_WIDTH/8;

/* Source base pointer */
	static UBYTE* screen_line_ptr;
/* Source/destination pixel offset */
	static long xoffset;

/* Destination base pointer */
	static UBYTE* dest_line_ptr;

/* Running source and destination pointers */
	static UBYTE* src_ptr;
	static UBYTE* src_ptr_next;
	static UBYTE* dest_ptr;

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

		entire_screen_dirty();

		pixelstep = geom[useMode].pixelstep;
		linestep = geom[useMode].linestep;
		low_limit = geom[useMode].sourceoffset+24;
		high_limit = low_limit + geom[useMode].xLimit;
	}
	
/* Ensure keyboard flag consistency */
	if(currentScreenMode == 0)
		currentKeyboardMode = 4;
	
/* Overlay keyboard in landscape modes */
	if(currentKeyboardMode != 4)
		overlay_kbd(scr_ptr);

	dest_line_ptr = GET_SCREEN_PTR();
	if(dest_line_ptr)
	{
		if(useMode == 0)
			pDrawKbd(dest_line_ptr);
		
		screen_dirty_ptr = screen_dirty;
		screen_line_ptr = (UBYTE*)atari_screen;
		xoffset = 0;

	/* Offset destination pointer to allow for rotation and particular screen geometry */
		dest_line_ptr += geom[useMode].startoffset;

	/* There are multiple versions of the internal loop based on screen geometry and settings */
		if(geom[currentScreenMode].xSkipMask == 0xffffffff)
		{
		/* Regular 1:1 copy, with color conversion */
			while(screen_dirty_ptr < screen_dirty_limit)
			{
				if(*screen_dirty_ptr)
				{
					if(xoffset >= low_limit && xoffset < high_limit)
					{
						dest_ptr = dest_line_ptr + (xoffset-low_limit)*pixelstep;
						src_ptr = screen_line_ptr + xoffset;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						*(unsigned short*)dest_ptr = pal[*src_ptr];
					}
					*screen_dirty_ptr = 0;
				}

				xoffset += 8; /* One screen_dirty item corresponds to 8 pixels */
				if(xoffset >= ATARI_WIDTH) /* move to the next line */
				{
					xoffset = 0;
					screen_line_ptr += ATARI_WIDTH;
					dest_line_ptr += linestep;
				}

				screen_dirty_ptr ++;
			}
		}
		else if(smooth_filter || ui_is_active)
		{
		/* The most complex version of the internal loop does 3/4 linear filtering */
			while(screen_dirty_ptr < screen_dirty_limit)
			{
				if(*screen_dirty_ptr)
				{
					if(xoffset >= low_limit && xoffset < high_limit)
					{
						dest_ptr = dest_line_ptr + (xoffset-low_limit)*3/4*pixelstep;
						src_ptr = screen_line_ptr + xoffset;
						src_ptr_next = src_ptr+1;

						UBYTE r, g, b;
						r = (3*palRed[*src_ptr] + palRed[*src_ptr_next])>>2;
						g = (3*palGreen[*src_ptr] + palGreen[*src_ptr_next])>>2;
						b = (3*palBlue[*src_ptr] + palBlue[*src_ptr_next])>>2;

						*(unsigned short*)dest_ptr = COLORCONVHICOLOR(r,g,b);
						dest_ptr += pixelstep;
						src_ptr++;
						src_ptr_next++;

						r = (palRed[*src_ptr] + palRed[*src_ptr_next])>>1;
						g = (palGreen[*src_ptr] + palGreen[*src_ptr_next])>>1;
						b = (palBlue[*src_ptr] + palBlue[*src_ptr_next])>>1;

						*(unsigned short*)dest_ptr = COLORCONVHICOLOR(r,g,b);
						dest_ptr += pixelstep;
						src_ptr++;
						src_ptr_next++;

						r = (palRed[*src_ptr] + 3*palRed[*src_ptr_next])>>2;
						g = (palGreen[*src_ptr] + 3*palGreen[*src_ptr_next])>>2;
						b = (palBlue[*src_ptr] + 3*palBlue[*src_ptr_next])>>2;

						*(unsigned short*)dest_ptr = COLORCONVHICOLOR(r,g,b);
						dest_ptr += pixelstep;
						src_ptr += 2;
						src_ptr_next += 2;

						r = (3*palRed[*src_ptr] + palRed[*src_ptr_next])>>2;
						g = (3*palGreen[*src_ptr] + palGreen[*src_ptr_next])>>2;
						b = (3*palBlue[*src_ptr] + palBlue[*src_ptr_next])>>2;

						*(unsigned short*)dest_ptr = COLORCONVHICOLOR(r,g,b);
						dest_ptr += pixelstep;
						src_ptr++;
						src_ptr_next++;

						r = (palRed[*src_ptr] + palRed[*src_ptr_next])>>1;
						g = (palGreen[*src_ptr] + palGreen[*src_ptr_next])>>1;
						b = (palBlue[*src_ptr] + palBlue[*src_ptr_next])>>1;

						*(unsigned short*)dest_ptr = COLORCONVHICOLOR(r,g,b);
						dest_ptr += pixelstep;
						src_ptr++;
						src_ptr_next++;

						r = (palRed[*src_ptr] + 3*palRed[*src_ptr_next])>>2;
						g = (palGreen[*src_ptr] + 3*palGreen[*src_ptr_next])>>2;
						b = (palBlue[*src_ptr] + 3*palBlue[*src_ptr_next])>>2;

						*(unsigned short*)dest_ptr = COLORCONVHICOLOR(r,g,b);
					}
					*screen_dirty_ptr = 0;
				}

				xoffset += 8; /* One screen_dirty item corresponds to 8 pixels */
				if(xoffset >= ATARI_WIDTH) /* move to the next line */
				{
					xoffset = 0;
					screen_line_ptr += ATARI_WIDTH;
					dest_line_ptr += linestep;
				}

				screen_dirty_ptr ++;
			}
		}
		else
		{
		/* 3/4 pixel skip version. Not recommended anymore */
			while(screen_dirty_ptr < screen_dirty_limit)
			{
				if(*screen_dirty_ptr)
				{
					if(xoffset >= low_limit && xoffset < high_limit)
					{
						dest_ptr = dest_line_ptr + (xoffset-low_limit)*3/4*pixelstep;
						src_ptr = screen_line_ptr + xoffset;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						src_ptr ++;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						*(unsigned short*)dest_ptr = pal[*src_ptr++]; dest_ptr += pixelstep;
						*(unsigned short*)dest_ptr = pal[*src_ptr];
					}
					*screen_dirty_ptr = 0;
				}

				xoffset += 8; /* One screen_dirty item corresponds to 8 pixels */
				if(xoffset >= ATARI_WIDTH) /* move to the next line */
				{
					xoffset = 0;
					screen_line_ptr += ATARI_WIDTH;
					dest_line_ptr += linestep;
				}

				screen_dirty_ptr ++;
			}
		}

		RELEASE_SCREEN();
	}
}


void refresh_kbd()
{
	static UBYTE *scraddr;
	scraddr = GET_SCREEN_PTR();
	if(scraddr)
	{
		pDrawKbd(scraddr);
		RELEASE_SCREEN();
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

void mono_DrawKbd(UBYTE* scraddr)
{
	static ULONG* kbd_image_ptr;
	static unsigned long curBit;
	static int linestep, pixelstep;
	static UBYTE colorOn, colorOff;
	static UBYTE* scraddr_limit;
	static UBYTE* dest;
	static UBYTE* dest_limit;
	static int cachedcolor=-1; /* must be static */
	static ULONG* cachedkbd=NULL;
	
	static UBYTE bitmask;
	static int   bitshift;

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
		colorOn = 0xff;
		colorOff = 0x00;
	}
	else
	{
		colorOn = 0x00;
		colorOff = 0xff;
	}
	
	linestep = geom[0].linestep;
	pixelstep = geom[0].pixelstep;
	
	if(pixelstep == 0)
		return; // unsupported screen geometry
// this will work on mono iPAQ and @migo, don't know about any others
	bitshift = 0;
	bitmask = (1<<gxdp.cBPP)-1;
	linestep = (pixelstep > 0) ? -1 : 1;

	scraddr += (geom[0].startoffset + (geom[0].height-80)*linestep)*gxdp.cBPP>>3;
	scraddr_limit = scraddr + (linestep*80*gxdp.cBPP>>3); /* note that linestep may be negative */
	dest_limit = scraddr + pixelstep*240; /* again, pixelstep may be negative */
	
	kbd_image_ptr = kbd_image; /* word pointer in keyboard bitmap */
	curBit = 1;  /* rotating bit pointer */
	while(scraddr != scraddr_limit) /* note '!=' */
	{
		dest = scraddr;
		while(dest != dest_limit)
		{
			if(*kbd_image_ptr & curBit)
				*dest = ((*dest)&~bitmask)|(colorOn<<bitshift);
			else
				*dest = ((*dest)&~bitmask)|(colorOff<<bitshift);

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

		bitshift += gxdp.cBPP;
		if(bitshift >= 8)
		{
			bitshift = 0;
			bitmask = (1<<gxdp.cBPP)-1;
			scraddr += linestep;
			dest_limit += linestep;
		}
		else
			bitmask <<= gxdp.cBPP;
	}
	kbd_image_ok = 1;
}

void palette_DrawKbd(UBYTE* scraddr)
{
	static ULONG* kbd_image_ptr;
	static unsigned long curBit;
	static int linestep, pixelstep;
	static UBYTE colorOn, colorOff;
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
		colorOn = 0x0f;
		colorOff = 0x00;
	}
	else
	{
		colorOn = 0x00;
		colorOff = 0x0f;
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
				*dest = colorOn;
			else
				*dest = colorOff;

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

void hicolor_DrawKbd(UBYTE* scraddr)
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

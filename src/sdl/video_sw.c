/*
 * sdl/video_sw.c - SDL library specific port code - software-based video display
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2010 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <SDL.h>

#include "af80.h"
#include "atari.h"
#include "colours.h"
#include "config.h"
#include "filter_ntsc.h"
#include "log.h"
#include "pbi_proto80.h"
#include "platform.h"
#include "screen.h"
#include "xep80.h"
#include "xep80_fonts.h"
#include "util.h"

#include "sdl/palette.h"
#include "sdl/video.h"
#include "sdl/video_sw.h"

static int fullscreen = 1;

static SDL_Surface *MainScreen = NULL;
static SDL_Color colors[256];			/* palette */
static union {
	Uint16 bpp16[256];	/* 16-bit palette */
	Uint32 bpp32[256];	/* 32-bit palette */
} palette;

static void DisplayWithoutScaling(void);
static void DisplayWithScaling(void);
static void DisplayRotated(void);
static void DisplayNTSCEmu(void);
static void DisplayXEP80(void);
static void DisplayProto80(void);
static void DisplayAF80(void);

static void (*blit_funcs[VIDEOMODE_MODE_SIZE])(void) = {
	&DisplayWithoutScaling,
	&DisplayNTSCEmu,
	&DisplayXEP80,
	&DisplayProto80,
	&DisplayAF80
};

/* Indicates current display mode */
static VIDEOMODE_MODE_t current_display_mode = VIDEOMODE_MODE_NORMAL;

static void SetPalette(void)
{
	SDL_SetPalette(MainScreen, SDL_LOGPAL | SDL_PHYSPAL, colors, 0, 256);
}

static void CalcPalette(VIDEOMODE_MODE_t mode)
{
	int *pal = SDL_PALETTE_tab[mode].palette;
	int size = SDL_PALETTE_tab[mode].size;
	int i, rgb;
	Uint32 c;
	for (i = 0; i < size; i++) {
		rgb = pal[i];
		colors[i].r = (rgb & 0x00ff0000) >> 16;
		colors[i].g = (rgb & 0x0000ff00) >> 8;
		colors[i].b = (rgb & 0x000000ff) >> 0;
	}
	for (i = 0; i < size; i++) {
		c =
			SDL_MapRGB(MainScreen->format, colors[i].r, colors[i].g,
					   colors[i].b);
		switch (MainScreen->format->BitsPerPixel) {
		case 16:
			palette.bpp16[i] = (Uint16) c;
			break;
		case 32:
			palette.bpp32[i] = (Uint32) c;
			break;
		}
	}

}

void SDL_VIDEO_SW_PaletteUpdate(void)
{
	CalcPalette(current_display_mode);
	if (current_display_mode == VIDEOMODE_MODE_NTSC_FILTER)
		FILTER_NTSC_Update(FILTER_NTSC_emu);
	SetPalette();
}

static void ModeInfo(void)
{
	char *fullstring;
	if (fullscreen)
		fullstring = "fullscreen";
	else
		fullstring = "windowed";
	Log_print("Video Mode: %dx%dx%d %s", MainScreen->w, MainScreen->h,
		   MainScreen->format->BitsPerPixel, fullstring);
}

static void SetVideoMode(int w, int h, int bpp)
{
	Uint32 flags = fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE;
	MainScreen = SDL_SetVideoMode(w, h, bpp, flags);
	if (MainScreen == NULL) {
		Log_print("Setting Video Mode: %dx%dx%d failed: %s", w, h, bpp, SDL_GetError());
		Log_flushlog();
		exit(-1);
	}
	SDL_VIDEO_width = MainScreen->w;
	SDL_VIDEO_height = MainScreen->h;
}

static void UpdateNtscFilter(VIDEOMODE_MODE_t mode)
{
	if (mode != VIDEOMODE_MODE_NTSC_FILTER && FILTER_NTSC_emu != NULL) {
		/* Turning filter off */
		FILTER_NTSC_Delete(FILTER_NTSC_emu);
		FILTER_NTSC_emu = NULL;
	}
	else if (mode == VIDEOMODE_MODE_NTSC_FILTER && FILTER_NTSC_emu == NULL) {
		/* Turning filter on */
		FILTER_NTSC_emu = FILTER_NTSC_New();
		FILTER_NTSC_Update(FILTER_NTSC_emu);
	}
}

void SDL_VIDEO_SW_SetVideoMode(VIDEOMODE_resolution_t const *res, int windowed, VIDEOMODE_MODE_t mode, int rotate90)
{
	int old_bpp = MainScreen == NULL ? 0 : MainScreen->format->BitsPerPixel;
	int need_setvideo = TRUE;
	fullscreen = !windowed;
	UpdateNtscFilter(mode);

	if (SDL_VIDEO_bpp == 0) {
		/* Autodetect bpp */
		SetVideoMode(res->width, res->height, SDL_VIDEO_bpp);
		SDL_VIDEO_bpp = MainScreen->format->BitsPerPixel;
		Log_print("detected %dbpp", SDL_VIDEO_bpp);
		if ((SDL_VIDEO_bpp != 8) && (SDL_VIDEO_bpp != 16) && (SDL_VIDEO_bpp != 32)) {
			Log_print("it's unsupported, so setting 8bit mode (slow conversion)");
			SDL_VIDEO_bpp = 8;
		} else
			need_setvideo = FALSE;
	}

	if ((rotate90 && SDL_VIDEO_bpp != 16) || (mode == VIDEOMODE_MODE_NTSC_FILTER && SDL_VIDEO_bpp != 16 && SDL_VIDEO_bpp != 32)) {
		/* Rotate90 supports only 16bpp; NTSC filter doesn't support 8bpp. */
		SDL_VIDEO_bpp = 16;
		need_setvideo = TRUE;
	}

	if (need_setvideo) {
		SetVideoMode(res->width, res->height, SDL_VIDEO_bpp);
	}

	/* BPP changed, or the new display mode's palette is different than the previous one. */
	if (SDL_PALETTE_tab[mode].palette != SDL_PALETTE_tab[current_display_mode].palette
	    || SDL_VIDEO_bpp != old_bpp)
		CalcPalette(mode);
	if (SDL_VIDEO_bpp == 8)
		SetPalette();

	/* Clear the screen. */
	SDL_FillRect(MainScreen, NULL, 0);
	SDL_Flip(MainScreen);

	SDL_ShowCursor(SDL_DISABLE);	/* hide mouse cursor */
	ModeInfo();

	current_display_mode = mode;
	if (mode == VIDEOMODE_MODE_NORMAL) {
		if (rotate90)
			blit_funcs[0] = &DisplayRotated;
		else if (VIDEOMODE_src_width == VIDEOMODE_dest_width && VIDEOMODE_src_height == VIDEOMODE_dest_height)
			blit_funcs[0] = &DisplayWithoutScaling;
		else
			blit_funcs[0] = &DisplayWithScaling;
	}
	SDL_VIDEO_SW_DisplayScreen();
}

int SDL_VIDEO_SW_SupportsVideomode(VIDEOMODE_MODE_t mode, int stretch, int rotate90)
{
	if (mode == VIDEOMODE_MODE_NORMAL) {
		/* Normal mode doesn't support stretching together with rotation. */
		return !(stretch && rotate90);
	} else
		/* Other modes don't support stretching or rotation at all. */
		return !stretch && !rotate90;
}

/* License of scanLines_16():*/
/* This function has been altered from its original version */
/* This license is a verbatim copy of the license of ZLib 
 * http://www.gnu.org/licenses/license-list.html#GPLCompatibleLicenses
 * This is a free software license, and compatible with the GPL. */
/*****************************************************************************
 ** Original Source: /cvsroot/bluemsx/blueMSX/Src/VideoRender/VideoRender.c,v 
 **
 ** Original Revision: 1.25 
 **
 ** Original Date: 2006/01/17 08:49:34 
 **
 ** More info: http://www.bluemsx.com
 **
 ** Copyright (C) 2003-2004 Daniel Vik
 **
 **  This software is provided 'as-is', without any express or implied
 **  warranty.  In no event will the authors be held liable for any damages
 **  arising from the use of this software.
 **
 **  Permission is granted to anyone to use this software for any purpose,
 **  including commercial applications, and to alter it and redistribute it
 **  freely, subject to the following restrictions:
 **
 **  1. The origin of this software must not be misrepresented; you must not
 **     claim that you wrote the original software. If you use this software
 **     in a product, an acknowledgment in the product documentation would be
 **     appreciated but is not required.
 **  2. Altered source versions must be plainly marked as such, and must not be
 **     misrepresented as being the original software.
 **  3. This notice may not be removed or altered from any source distribution.
 **
 ******************************************************************************
 */

/* Modified version, which optionally uses interpolation (slower but better).
   Caution! This function assumes that the 16-bit screen format is 565
   (rrrrrggg gggbbbbb). */
static void scanLines_16(void* pBuffer, int width, int height, int pitch, int scanLinesPct)
{
	Uint32* pBuf = (Uint32*)(pBuffer)+pitch/sizeof(Uint32);
	Uint32* sBuf = (Uint32*)(pBuffer);
	Uint32* tBuf = (Uint32*)(pBuffer)+pitch*2/sizeof(Uint32);
	int w, h;
	static int prev_scanLinesPct;

	pitch = pitch * 2 / (int)sizeof(Uint32);
	height /= 2;
	width /= 2;

	if (scanLinesPct < 0) scanLinesPct = 0;
	if (scanLinesPct > 100) scanLinesPct = 100;

	if (scanLinesPct == 100) {
		if (prev_scanLinesPct != 100) {
			/*clean dirty blank scanlines*/
			prev_scanLinesPct = 100;
			for (h = 0; h < height; h++) {
				memset(pBuf, 0, width * sizeof(Uint32));
				pBuf += pitch;
			}
		}
		return;
	}
	prev_scanLinesPct = scanLinesPct;


	if (scanLinesPct == 0) {
	/* fill in blank scanlines */
		for (h = 0; h < height; h++) {
			memcpy(pBuf, sBuf, width * sizeof(Uint32));
			sBuf += pitch;
			pBuf += pitch;
		}
		return;
	}

	if (SDL_VIDEO_interpolate_scanlines) {
		scanLinesPct = (100-scanLinesPct) * 32 / 200;
		for (h = 0; h < height-1; h++) {
			for (w = 0; w < width; w++) {
				Uint32 pixel = sBuf[w];
				Uint32 pixel2 = tBuf[w];
				Uint32 a = ((((pixel & 0x07e0f81f)+(pixel2 & 0x07e0f81f)) * scanLinesPct) & 0xfc1f03e0) >> 5;
				Uint32 b = ((((pixel >> 5) & 0x07c0f83f)+((pixel2 >> 5) & 0x07c0f83f)) * scanLinesPct) & 0xf81f07e0;
				pBuf[w] = a | b;
			}
			sBuf += pitch;
			tBuf += pitch;
			pBuf += pitch;
		}
	} else {
		scanLinesPct = (100-scanLinesPct) * 32 / 100;
		for (h = 0; h < height; h++) {
			for (w = 0; w < width; w++) {
				Uint32 pixel = sBuf[w];
				Uint32 a = (((pixel & 0x07e0f81f) * scanLinesPct) & 0xfc1f03e0) >> 5;
				Uint32 b = (((pixel >> 5) & 0x07c0f83f) * scanLinesPct) & 0xf81f07e0;
				pBuf[w] = a | b;
			}
			sBuf += pitch;
			pBuf += pitch;
		}
	}
}

/* Modified version of scanLines_16, for 32-bit screen.
   Caution! This function assumes that the 32-bit screen format is ARGB
   (aaaaaaaa rrrrrrrr gggggggg bbbbbbbb). */
static void scanLines_32(void* pBuffer, int width, int height, int pitch, int scanLinesPct)
{
	Uint32* pBuf = (Uint32*)(pBuffer)+pitch/sizeof(Uint32);
	Uint32* sBuf = (Uint32*)(pBuffer);
	Uint32* tBuf = (Uint32*)(pBuffer)+pitch*2/sizeof(Uint32);
	int w, h;
	static int prev_scanLinesPct;

	pitch = pitch * 2 / (int)sizeof(Uint32);
	height /= 2;

	if (scanLinesPct < 0) scanLinesPct = 0;
	if (scanLinesPct > 100) scanLinesPct = 100;

	if (scanLinesPct == 100) {
		if (prev_scanLinesPct != 100) {
			/*clean dirty blank scanlines*/
			prev_scanLinesPct = 100;
			for (h = 0; h < height; h++) {
				memset(pBuf, 0, width * sizeof(Uint32));
				pBuf += pitch;
			}
		}
		return;
	}
	prev_scanLinesPct = scanLinesPct;


	if (scanLinesPct == 0) {
	/* fill in blank scanlines */
		for (h = 0; h < height; h++) {
			memcpy(pBuf, sBuf, width * sizeof(Uint32));
			sBuf += pitch;
			pBuf += pitch;
		}
		return;
	}

	if (SDL_VIDEO_interpolate_scanlines) {
		scanLinesPct = (100-scanLinesPct) * 256 / 200;
		for (h = 0; h < height-1; h++) {
			for (w = 0; w < width; w++) {
				Uint32 pixel = sBuf[w];
				Uint32 pixel2 = tBuf[w];
				Uint32 a = ((((pixel & 0x00ff00ff)+(pixel2 & 0x00ff00ff)) * scanLinesPct) & 0xff00ff00) >> 8;
				Uint32 b = ((((pixel & 0x0000ff00)+(pixel2 & 0x0000ff00)) >> 8) * scanLinesPct) & 0x0000ff00;
				pBuf[w] = a | b;
			}
			sBuf += pitch;
			tBuf += pitch;
			pBuf += pitch;
		}
	} else {
		scanLinesPct = (100-scanLinesPct) * 256 / 100;
		for (h = 0; h < height; h++) {
			for (w = 0; w < width; w++) {
				Uint32 pixel = sBuf[w];
				Uint32 a = (((pixel & 0x00ff00ff) * scanLinesPct) & 0xff00ff00) >> 8;
				Uint32 b = (((pixel & 0x0000ff00) >> 8) * scanLinesPct) & 0x0000ff00;
				pBuf[w] = a | b;
			}
			sBuf += pitch;
			pBuf += pitch;
		}
	}
}

static void DisplayXEP80(void)
{
	static int xep80Frame = 0;
	int pitch4 = MainScreen->pitch / 2;
	UBYTE *screen;
	Uint8 *pixels = (Uint8 *) MainScreen->pixels + MainScreen->pitch * VIDEOMODE_dest_offset_top;
	xep80Frame++;
	if (xep80Frame == 60) xep80Frame = 0;
	if (xep80Frame > 29) {
		screen = XEP80_screen_1;
	}
	else {
		screen = XEP80_screen_2;
	}

	screen += XEP80_SCRN_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	switch (MainScreen->format->BitsPerPixel) {
	case 8:
		pixels += VIDEOMODE_dest_offset_left;
		SDL_VIDEO_BlitXEP80_8((Uint32 *)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height);
		break;
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		SDL_VIDEO_BlitXEP80_16((Uint32 *)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, palette.bpp16);
		scanLines_16((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, MainScreen->pitch, SDL_VIDEO_scanlines_percentage);
		break;
	default:
		pixels += VIDEOMODE_dest_offset_left * 4;
		SDL_VIDEO_BlitXEP80_32((Uint32 *)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, palette.bpp32);
		scanLines_32((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, MainScreen->pitch, SDL_VIDEO_scanlines_percentage);
	}
}

static void DisplayNTSCEmu(void)
{
	Uint8 *pixels = (Uint8*)MainScreen->pixels + MainScreen->pitch * VIDEOMODE_dest_offset_top;
	switch (MainScreen->format->BitsPerPixel) {
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		/* blit atari image, doubled vertically */
		atari_ntsc_blit16(FILTER_NTSC_emu,
		                  (ATARI_NTSC_IN_T *) ((UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left),
		                  Screen_WIDTH,
		                  VIDEOMODE_src_width,
		                  VIDEOMODE_src_height,
		                  pixels,
		                  MainScreen->pitch * 2);
		scanLines_16((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, MainScreen->pitch, SDL_VIDEO_scanlines_percentage);
		break;
	case 32:
		pixels += VIDEOMODE_dest_offset_left * 4;
		atari_ntsc_blit24(FILTER_NTSC_emu,
		                  (ATARI_NTSC_IN_T *) ((UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left),
		                  Screen_WIDTH,
		                  VIDEOMODE_src_width,
		                  VIDEOMODE_src_height,
		                  pixels,
		                  MainScreen->pitch * 2);
		scanLines_32((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, MainScreen->pitch, SDL_VIDEO_scanlines_percentage);
		break;
	}
}

static void DisplayProto80(void)
{
	int first_column = (VIDEOMODE_src_offset_left+7) / 8;
	int last_column = (VIDEOMODE_src_offset_left + VIDEOMODE_src_width) / 8;
	int first_line = VIDEOMODE_src_offset_top;
	int last_line = first_line + VIDEOMODE_src_height;
	int pitch4 = MainScreen->pitch / 2;
	Uint8 *pixels = (Uint8*)MainScreen->pixels + MainScreen->pitch * VIDEOMODE_dest_offset_top;

	
	switch (MainScreen->format->BitsPerPixel) {
	case 8:
		pixels += VIDEOMODE_dest_offset_left;
		SDL_VIDEO_BlitProto80_8((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line);
		break;
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		SDL_VIDEO_BlitProto80_16((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, palette.bpp16);
		scanLines_16((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, MainScreen->pitch, SDL_VIDEO_scanlines_percentage);
		break;
	default:
		pixels += VIDEOMODE_dest_offset_left * 4;
		SDL_VIDEO_BlitProto80_32((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, palette.bpp32);
		scanLines_32((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, MainScreen->pitch, SDL_VIDEO_scanlines_percentage);
	}
}

static void DisplayAF80(void)
{
	int first_column = (VIDEOMODE_src_offset_left+7) / 8;
	int last_column = (VIDEOMODE_src_offset_left + VIDEOMODE_src_width) / 8;
	int first_line = VIDEOMODE_src_offset_top;
	int last_line = first_line + VIDEOMODE_src_height;
	int pitch4 = MainScreen->pitch / 2;
	Uint8 *pixels = (Uint8*)MainScreen->pixels + MainScreen->pitch * VIDEOMODE_dest_offset_top;

	static int AF80Frame = 0;
	int blink;
	AF80Frame++;
	if (AF80Frame == 60) AF80Frame = 0;
	blink = AF80Frame >= 30;
	
	switch (MainScreen->format->BitsPerPixel) {
	case 8:
		pixels += VIDEOMODE_dest_offset_left;
		SDL_VIDEO_BlitAF80_8((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, blink);
		break;
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		SDL_VIDEO_BlitAF80_16((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, blink, palette.bpp16);
		scanLines_16((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, MainScreen->pitch, SDL_VIDEO_scanlines_percentage);
		break;
	default:
		pixels += VIDEOMODE_dest_offset_left * 4;
		SDL_VIDEO_BlitAF80_32((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, blink, palette.bpp32);
		scanLines_32((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, MainScreen->pitch, SDL_VIDEO_scanlines_percentage);
	}
}

static void DisplayRotated(void)
{
	unsigned int x, y;
	register Uint32 *start32 = (Uint32 *) MainScreen->pixels + MainScreen->pitch / 4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 2;
	int pitch4 = MainScreen->pitch / 4 - VIDEOMODE_dest_width / 2;
	UBYTE *screen = (UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	for (y = 0; y < VIDEOMODE_dest_height; y++) {
		for (x = 0; x < VIDEOMODE_dest_width / 2; x++) {
			Uint8 left = screen[Screen_WIDTH * (x * 2) + VIDEOMODE_src_width - y];
			Uint8 right = screen[Screen_WIDTH * (x * 2 + 1) + VIDEOMODE_src_width - y];
#ifdef WORDS_BIGENDIAN
			*start32++ = (palette.bpp16[left] << 16) + palette.bpp16[right];
#else
			*start32++ = (palette.bpp16[right] << 16) + palette.bpp16[left];
#endif
		}
		start32 += pitch4;
	}
}

static void DisplayWithoutScaling(void)
{
	int pitch4 = MainScreen->pitch / 4;
	UBYTE *screen = (UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	Uint8 *pixels = (Uint8 *) MainScreen->pixels + MainScreen->pitch * VIDEOMODE_dest_offset_top;
	switch (MainScreen->format->BitsPerPixel) {
	/* Possible values are 8, 16 and 32, as checked earlier in the
	 * PLATFORM_SetVideoMode() function. */
	case 8:
		pixels += VIDEOMODE_dest_offset_left;
		SDL_VIDEO_BlitNormal8((Uint32 *)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height);
		break;
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		SDL_VIDEO_BlitNormal16((Uint32*)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, palette.bpp16);
		break;
	default: /* MainScreen->format->BitsPerPixel == 32 */
		pixels += VIDEOMODE_dest_offset_left * 4;
		SDL_VIDEO_BlitNormal32((Uint32 *)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, palette.bpp32);
	}
}

static void DisplayWithScaling(void)
{
	register Uint32 quad;
	register int x;
	register int dx;
	register int yy;
	register Uint8 *ss = (UBYTE *)Screen_atari;
	register Uint32 *start32;
	int i;
	int y;
	int w1;
	int w, h;
	int pos;
	int pitch4;
	int dy;
	int init_x = ((VIDEOMODE_src_width + VIDEOMODE_src_offset_left) << 16) - 0x4000;

	Uint8 c;
	pitch4 = MainScreen->pitch / 4;
	start32 = (Uint32 *) MainScreen->pixels;

	w = (VIDEOMODE_src_width) << 16;
	h = (VIDEOMODE_src_height) << 16;
	dx = w / VIDEOMODE_dest_width;
	dy = h / VIDEOMODE_dest_height;
	y = (VIDEOMODE_src_offset_top) << 16;
	i = VIDEOMODE_dest_height;

	switch (MainScreen->format->BitsPerPixel) {
	/* Possible values are 8, 16 and 32, as checked earlier in the
	 * PLATFORM_SetVideoMode() function. */
	case 8:
		start32 += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 4;
		w1 = VIDEOMODE_dest_width / 4 - 1;
		while (i > 0) {
			x = init_x;
			pos = w1;
			yy = Screen_WIDTH * (y >> 16);
			while (pos >= 0) {
				quad = (ss[yy + (x >> 16)] << 24);
				x = x - dx;
				quad += (ss[yy + (x >> 16)] << 16);
				x = x - dx;
				quad += (ss[yy + (x >> 16)] << 8);
				x = x - dx;
				quad += (ss[yy + (x >> 16)] << 0);
				x = x - dx;

				start32[pos] = quad;
				pos--;

			}
			start32 += pitch4;
			y = y + dy;
			i--;
		}
		break;
	case 16:
		start32 += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 2;
		w1 = VIDEOMODE_dest_width / 2 - 1;
		while (i > 0) {
			x = init_x;
			pos = w1;
			yy = Screen_WIDTH * (y >> 16);
			while (pos >= 0) {
				c = ss[yy + (x >> 16)];
				quad = palette.bpp16[c] << 16;
				x = x - dx;
				c = ss[yy + (x >> 16)];
				quad += palette.bpp16[c];
				x = x - dx;
				start32[pos] = quad;
				pos--;
			}
			start32 += pitch4;
			y = y + dy;
			i--;
		}
		break;
	default:
		start32 += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left;
		w1 = VIDEOMODE_dest_width - 1;
		/* MainScreen->format->BitsPerPixel = 32 */
		while (i > 0) {
			x = init_x;
			pos = w1;
			yy = Screen_WIDTH * (y >> 16);
			while (pos >= 0) {
				c = ss[yy + (x >> 16)];
				quad = palette.bpp32[c];
				x = x - dx;
				start32[pos] = quad;
				pos--;
			}
			start32 += pitch4;
			y = y + dy;
			i--;
		}
	}
}

void SDL_VIDEO_SW_DisplayScreen(void)
{
	/* Use function corresponding to the current_display_mode. */
	(*blit_funcs[current_display_mode])();
	/* SDL_UpdateRect is faster than SDL_Flip, because it updates only
	   the relevant part of the screen. */
/*	SDL_Flip(MainScreen);*/
	SDL_UpdateRect(MainScreen, VIDEOMODE_dest_offset_left, VIDEOMODE_dest_offset_top, VIDEOMODE_dest_width, VIDEOMODE_dest_height);
}

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

#include <stdio.h>
#include <string.h>
#include <SDL.h>

#include "af80.h"
#include "bit3.h"
#include "artifact.h"
#include "atari.h"
#include "colours.h"
#include "config.h"
#include "filter_ntsc.h"
#include "log.h"
#include "pbi_proto80.h"
#ifdef PAL_BLENDING
#include "pal_blending.h"
#endif /* PAL_BLENDING */
#include "platform.h"
#include "screen.h"
#include "videomode.h"
#include "xep80.h"
#include "xep80_fonts.h"
#include "util.h"

#include "sdl/palette.h"
#include "sdl/video.h"
#include "sdl/video_sw.h"

static int fullscreen = 1;

int SDL_VIDEO_SW_bpp = 0;

static void DisplayWithoutScaling(void);
static void DisplayWithScaling(void);
static void DisplayRotated(void);
#ifdef NTSC_FILTER
static void DisplayNTSCEmu(void);
#endif
#ifdef XEP80_EMULATION
static void DisplayXEP80(void);
#endif
#ifdef PBI_PROTO80
static void DisplayProto80(void);
#endif
#ifdef AF80
static void DisplayAF80(void);
#endif
#ifdef BIT3
static void DisplayBIT3(void);
#endif
#ifdef PAL_BLENDING
static void DisplayPalBlending(void);
static void DisplayPalBlendingScaled(void);
#endif /* PAL_BLENDING */

static void (*blit_funcs[VIDEOMODE_MODE_SIZE])(void) = {
	&DisplayWithoutScaling
#ifdef NTSC_FILTER
	,&DisplayNTSCEmu
#endif
#ifdef XEP80_EMULATION
	,&DisplayXEP80
#endif
#ifdef PBI_PROTO80
	,&DisplayProto80
#endif
#ifdef AF80
	,&DisplayAF80
#endif
#ifdef BIT3
	,&DisplayBIT3
#endif
};

static void Set8BitPalette(VIDEOMODE_MODE_t mode)
{
	int *pal = SDL_PALETTE_tab[mode].palette;
	int size = SDL_PALETTE_tab[mode].size;
	SDL_Color colors[256];
	int i, rgb;
	for (i = 0; i < size; i++) {
		rgb = pal[i];
		colors[i].r = (rgb & 0x00ff0000) >> 16;
		colors[i].g = (rgb & 0x0000ff00) >> 8;
		colors[i].b = (rgb & 0x000000ff) >> 0;
	}

	/* As the data that will be written to SDL_VIDEO_screen is already palettised,
	   SDL_SetPalette shouldn't modify the surface's logical palette (so no
	   SDL_LOGPAL here). Adding SDL_LOGPAL would break the palette when running
	   with DirectX video driver in 8bpp fullscreen videomode on Intel GMA 3100. */
	SDL_SetPalette(SDL_VIDEO_screen, SDL_PHYSPAL, colors, 0, 256);
}

void SDL_VIDEO_SW_GetPixelFormat(PLATFORM_pixel_format_t *format)
{
	format->bpp = SDL_VIDEO_SW_bpp;
	format->rmask = SDL_VIDEO_screen->format->Rmask;
	format->gmask = SDL_VIDEO_screen->format->Gmask;
	format->bmask = SDL_VIDEO_screen->format->Bmask;
}

void SDL_VIDEO_SW_MapRGB(void *dest, int const *palette, int size)
{
	int i;
	for (i = 0; i < size; ++i) {
		Uint32 c = SDL_MapRGB(SDL_VIDEO_screen->format,
		                      (palette[i] & 0x00ff0000) >> 16,
		                      (palette[i] & 0x0000ff00) >> 8,
		                      (palette[i] & 0x000000ff));
		switch (SDL_VIDEO_screen->format->BitsPerPixel) {
		case 16:
			((UWORD *)dest)[i] = (UWORD) c;
			break;
		case 32:
			((ULONG *)dest)[i] = (ULONG) c;
			break;
		}
	}
}

static void UpdatePaletteLookup(VIDEOMODE_MODE_t mode)
{
	if (SDL_VIDEO_screen->format->BitsPerPixel == 8)
		Set8BitPalette(mode);
	else
		SDL_VIDEO_UpdatePaletteLookup(mode, SDL_VIDEO_screen->format->BitsPerPixel == 32);
}

void SDL_VIDEO_SW_PaletteUpdate(void)
{
	UpdatePaletteLookup(SDL_VIDEO_current_display_mode);
}

static void ModeInfo(void)
{
	const char *fullstring = fullscreen ? "fullscreen" : "windowed";
	const char *vsyncstring = (SDL_VIDEO_screen->flags & SDL_DOUBLEBUF) ? "with vsync" : "without vsync";
	Log_print("Video Mode: %dx%dx%d %s %s", SDL_VIDEO_screen->w, SDL_VIDEO_screen->h,
	          SDL_VIDEO_screen->format->BitsPerPixel, fullstring, vsyncstring);
}

static void SetVideoMode(int w, int h, int bpp)
{
	Uint32 flags = (fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE)
	               | SDL_HWPALETTE;
	if (SDL_VIDEO_vsync)
		flags |= SDL_HWSURFACE | SDL_DOUBLEBUF;

	SDL_VIDEO_screen = SDL_SetVideoMode(w, h, bpp, flags);
	if (SDL_VIDEO_screen == NULL) {
		/* Some SDL_SetVideoMode errors can be averted by reinitialising the SDL video subsystem. */
		Log_print("Setting video mode: %dx%dx%d failed: %s. Reinitialising video.", w, h, bpp, SDL_GetError());
		SDL_VIDEO_ReinitSDL();
		SDL_VIDEO_screen = SDL_SetVideoMode(w, h, bpp, flags);
		if (SDL_VIDEO_screen == NULL) {
			Log_print("Setting Video Mode: %dx%dx%d failed: %s", w, h, bpp, SDL_GetError());
			Log_flushlog();
			exit(-1);
		}
	}
	SDL_VIDEO_width = SDL_VIDEO_screen->w;
	SDL_VIDEO_height = SDL_VIDEO_screen->h;
	/* When vsync is off, set its availability to TRUE. Otherwise check if
	   SDL_DOUBLEBUF is supported by the screen. */
	SDL_VIDEO_vsync_available = !SDL_VIDEO_vsync || (SDL_VIDEO_screen->flags & SDL_DOUBLEBUF);
	ModeInfo();
}

void SDL_VIDEO_SW_SetVideoMode(VIDEOMODE_resolution_t const *res, int windowed, VIDEOMODE_MODE_t mode, int rotate90)
{
	int old_bpp = SDL_VIDEO_screen == NULL ? 0 : SDL_VIDEO_screen->format->BitsPerPixel;

	if (SDL_VIDEO_SW_bpp == 0) {
		/* Autodetect bpp */
		if ((SDL_VIDEO_native_bpp != 8) && (SDL_VIDEO_native_bpp != 16) && (SDL_VIDEO_native_bpp != 32)) {
			Log_print("Native BPP of %i not supported, setting 8bit mode (slow conversion)", SDL_VIDEO_native_bpp);
			SDL_VIDEO_SW_bpp = 8;
		} else
			SDL_VIDEO_SW_bpp = SDL_VIDEO_native_bpp;
	}

	if ((rotate90 && SDL_VIDEO_SW_bpp != 16) ||
		((0
#ifdef NTSC_FILTER
		  || mode == VIDEOMODE_MODE_NTSC_FILTER
#endif
#ifdef PAL_BLENDING
		  || (mode == VIDEOMODE_MODE_NORMAL && ARTIFACT_mode == ARTIFACT_PAL_BLEND)
#endif /* PAL_BLENDING */
		 ) && SDL_VIDEO_SW_bpp != 16 && SDL_VIDEO_SW_bpp != 32)) {
		/* Rotate90 supports only 16bpp; NTSC filter and PAL blending don't support 8bpp. */
		SDL_VIDEO_SW_bpp = 16;
	}

	/* Call SetVideoMode only when there was change in width, height, bpp, windowed/fullscreen, or vsync. */
	if (SDL_VIDEO_screen == NULL || SDL_VIDEO_screen->w != res->width || SDL_VIDEO_screen->h != res->height || old_bpp != SDL_VIDEO_SW_bpp ||
	    fullscreen == windowed || (SDL_VIDEO_vsync && SDL_VIDEO_vsync_available) != ((SDL_VIDEO_screen->flags & SDL_DOUBLEBUF) == SDL_DOUBLEBUF)) { 
		fullscreen = !windowed;
		SetVideoMode(res->width, res->height, SDL_VIDEO_SW_bpp);
	}

	UpdatePaletteLookup(mode);

	/* Clear the screen. */
	SDL_FillRect(SDL_VIDEO_screen, NULL, 0);
	SDL_Flip(SDL_VIDEO_screen);
	if (SDL_VIDEO_vsync_available)
		/* Also clear the backbuffer. */
		SDL_FillRect(SDL_VIDEO_screen, NULL, 0);

	SDL_ShowCursor(SDL_DISABLE);	/* hide mouse cursor */

	if (mode == VIDEOMODE_MODE_NORMAL) {
		if (rotate90)
			blit_funcs[0] = &DisplayRotated;
#ifdef PAL_BLENDING
		else if (ARTIFACT_mode == ARTIFACT_PAL_BLEND) {
			if (VIDEOMODE_src_width == VIDEOMODE_dest_width && VIDEOMODE_src_height == VIDEOMODE_dest_height)
				blit_funcs[0] = &DisplayPalBlending;
			else
				blit_funcs[0] = &DisplayPalBlendingScaled;
		}
#endif /* PAL_BLENDING */
		else if (VIDEOMODE_src_width == VIDEOMODE_dest_width && VIDEOMODE_src_height == VIDEOMODE_dest_height)
			blit_funcs[0] = &DisplayWithoutScaling;
		else
			blit_funcs[0] = &DisplayWithScaling;
	}
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

int SDL_VIDEO_SW_SetBpp(int value)
{
	int old_value = SDL_VIDEO_SW_bpp;
	if (old_value != value) {
		SDL_VIDEO_SW_bpp = value;
		if (!VIDEOMODE_Update()) {
			SDL_VIDEO_SW_bpp = old_value;
			return FALSE;
		}
	}
	return TRUE;
}

int SDL_VIDEO_SW_ToggleBpp(void)
{
	int new_bpp;
	switch (SDL_VIDEO_SW_bpp) {
	case 16:
		new_bpp = 32;
		break;
	case 32:
		new_bpp = 8;
		break;
	default: /* 0 or 8 */
		new_bpp = 16;
	}
	return SDL_VIDEO_SW_SetBpp(new_bpp);
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

#ifdef XEP80_EMULATION
static void DisplayXEP80(void)
{
	static int xep80Frame = 0;
	int pitch4 = SDL_VIDEO_screen->pitch / 2;
	UBYTE *screen;
	Uint8 *pixels = (Uint8 *) SDL_VIDEO_screen->pixels + SDL_VIDEO_screen->pitch * VIDEOMODE_dest_offset_top;
	xep80Frame++;
	if (xep80Frame == 60) xep80Frame = 0;
	if (xep80Frame > 29) {
		screen = XEP80_screen_1;
	}
	else {
		screen = XEP80_screen_2;
	}

	screen += XEP80_SCRN_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	switch (SDL_VIDEO_screen->format->BitsPerPixel) {
	case 8:
		pixels += VIDEOMODE_dest_offset_left;
		SDL_VIDEO_BlitXEP80_8((Uint32 *)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height);
		break;
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		SDL_VIDEO_BlitXEP80_16((Uint32 *)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, SDL_PALETTE_buffer.bpp16);
		scanLines_16((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, SDL_VIDEO_screen->pitch, SDL_VIDEO_scanlines_percentage);
		break;
	default:
		pixels += VIDEOMODE_dest_offset_left * 4;
		SDL_VIDEO_BlitXEP80_32((Uint32 *)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, SDL_PALETTE_buffer.bpp32);
		scanLines_32((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, SDL_VIDEO_screen->pitch, SDL_VIDEO_scanlines_percentage);
	}
}
#endif

#ifdef NTSC_FILTER
static void DisplayNTSCEmu(void)
{
	Uint8 *pixels = (Uint8*)SDL_VIDEO_screen->pixels + SDL_VIDEO_screen->pitch * VIDEOMODE_dest_offset_top;
	switch (SDL_VIDEO_screen->format->BitsPerPixel) {
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		/* blit atari image, doubled vertically */
		atari_ntsc_blit_rgb16(FILTER_NTSC_emu,
		                      (ATARI_NTSC_IN_T *) ((UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left),
		                      Screen_WIDTH,
		                      VIDEOMODE_src_width,
		                      VIDEOMODE_src_height,
		                      pixels,
		                      SDL_VIDEO_screen->pitch * 2);
		scanLines_16((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, SDL_VIDEO_screen->pitch, SDL_VIDEO_scanlines_percentage);
		break;
	case 32:
		pixels += VIDEOMODE_dest_offset_left * 4;
		atari_ntsc_blit_argb32(FILTER_NTSC_emu,
		                      (ATARI_NTSC_IN_T *) ((UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left),
		                       Screen_WIDTH,
		                       VIDEOMODE_src_width,
		                       VIDEOMODE_src_height,
		                       pixels,
		                       SDL_VIDEO_screen->pitch * 2);
		scanLines_32((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, SDL_VIDEO_screen->pitch, SDL_VIDEO_scanlines_percentage);
		break;
	}
}
#endif

#ifdef PBI_PROTO80
static void DisplayProto80(void)
{
	int first_column = (VIDEOMODE_src_offset_left+7) / 8;
	int last_column = (VIDEOMODE_src_offset_left + VIDEOMODE_src_width) / 8;
	int first_line = VIDEOMODE_src_offset_top;
	int last_line = first_line + VIDEOMODE_src_height;
	int pitch4 = SDL_VIDEO_screen->pitch / 2;
	Uint8 *pixels = (Uint8*)SDL_VIDEO_screen->pixels + SDL_VIDEO_screen->pitch * VIDEOMODE_dest_offset_top;

	
	switch (SDL_VIDEO_screen->format->BitsPerPixel) {
	case 8:
		pixels += VIDEOMODE_dest_offset_left;
		SDL_VIDEO_BlitProto80_8((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line);
		break;
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		SDL_VIDEO_BlitProto80_16((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, SDL_PALETTE_buffer.bpp16);
		scanLines_16((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, SDL_VIDEO_screen->pitch, SDL_VIDEO_scanlines_percentage);
		break;
	default:
		pixels += VIDEOMODE_dest_offset_left * 4;
		SDL_VIDEO_BlitProto80_32((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, SDL_PALETTE_buffer.bpp32);
		scanLines_32((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, SDL_VIDEO_screen->pitch, SDL_VIDEO_scanlines_percentage);
	}
}
#endif

#ifdef AF80
static void DisplayAF80(void)
{
	int first_column = (VIDEOMODE_src_offset_left+7) / 8;
	int last_column = (VIDEOMODE_src_offset_left + VIDEOMODE_src_width) / 8;
	int first_line = VIDEOMODE_src_offset_top;
	int last_line = first_line + VIDEOMODE_src_height;
	int pitch4 = SDL_VIDEO_screen->pitch / 2;
	Uint8 *pixels = (Uint8*)SDL_VIDEO_screen->pixels + SDL_VIDEO_screen->pitch * VIDEOMODE_dest_offset_top;

	static int AF80Frame = 0;
	int blink;
	AF80Frame++;
	if (AF80Frame == 60) AF80Frame = 0;
	blink = AF80Frame >= 30;
	
	switch (SDL_VIDEO_screen->format->BitsPerPixel) {
	case 8:
		pixels += VIDEOMODE_dest_offset_left;
		SDL_VIDEO_BlitAF80_8((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, blink);
		break;
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		SDL_VIDEO_BlitAF80_16((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, blink, SDL_PALETTE_buffer.bpp16);
		scanLines_16((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, SDL_VIDEO_screen->pitch, SDL_VIDEO_scanlines_percentage);
		break;
	default:
		pixels += VIDEOMODE_dest_offset_left * 4;
		SDL_VIDEO_BlitAF80_32((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, blink, SDL_PALETTE_buffer.bpp32);
		scanLines_32((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, SDL_VIDEO_screen->pitch, SDL_VIDEO_scanlines_percentage);
	}
}
#endif

#ifdef BIT3
static void DisplayBIT3(void)
{
	int first_column = (VIDEOMODE_src_offset_left+7) / 8;
	int last_column = (VIDEOMODE_src_offset_left + VIDEOMODE_src_width) / 8;
	int first_line = VIDEOMODE_src_offset_top;
	int last_line = first_line + VIDEOMODE_src_height;
	int pitch4 = SDL_VIDEO_screen->pitch / 2;
	Uint8 *pixels = (Uint8*)SDL_VIDEO_screen->pixels + SDL_VIDEO_screen->pitch * VIDEOMODE_dest_offset_top;

	static int BIT3Frame = 0;
	int blink;
	BIT3Frame++;
	if (BIT3Frame == 60) BIT3Frame = 0;
	blink = BIT3Frame >= 30;
	
	switch (SDL_VIDEO_screen->format->BitsPerPixel) {
	case 8:
		pixels += VIDEOMODE_dest_offset_left;
		SDL_VIDEO_BlitBIT3_8((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, blink);
		break;
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		SDL_VIDEO_BlitBIT3_16((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, blink, SDL_PALETTE_buffer.bpp16);
		scanLines_16((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, SDL_VIDEO_screen->pitch, SDL_VIDEO_scanlines_percentage);
		break;
	default:
		pixels += VIDEOMODE_dest_offset_left * 4;
		SDL_VIDEO_BlitBIT3_32((Uint32 *)pixels, first_column, last_column, pitch4, first_line, last_line, blink, SDL_PALETTE_buffer.bpp32);
		scanLines_32((void *)pixels, VIDEOMODE_dest_width, VIDEOMODE_dest_height, SDL_VIDEO_screen->pitch, SDL_VIDEO_scanlines_percentage);
	}
}
#endif

static void DisplayRotated(void)
{
	unsigned int x, y;
	register Uint32 *start32 = (Uint32 *) SDL_VIDEO_screen->pixels + SDL_VIDEO_screen->pitch / 4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 2;
	int pitch4 = SDL_VIDEO_screen->pitch / 4 - VIDEOMODE_dest_width / 2;
	UBYTE *screen = (UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	for (y = 0; y < VIDEOMODE_dest_height; y++) {
		for (x = 0; x < VIDEOMODE_dest_width / 2; x++) {
			Uint8 left = screen[Screen_WIDTH * (x * 2) + VIDEOMODE_src_width - y];
			Uint8 right = screen[Screen_WIDTH * (x * 2 + 1) + VIDEOMODE_src_width - y];
#ifdef WORDS_BIGENDIAN
			*start32++ = (SDL_PALETTE_buffer.bpp16[left] << 16) + SDL_PALETTE_buffer.bpp16[right];
#else
			*start32++ = (SDL_PALETTE_buffer.bpp16[right] << 16) + SDL_PALETTE_buffer.bpp16[left];
#endif
		}
		start32 += pitch4;
	}
}

static void DisplayWithoutScaling(void)
{
	int pitch4 = SDL_VIDEO_screen->pitch / 4;
	UBYTE *screen = (UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	Uint8 *pixels = (Uint8 *) SDL_VIDEO_screen->pixels + SDL_VIDEO_screen->pitch * VIDEOMODE_dest_offset_top;
	switch (SDL_VIDEO_screen->format->BitsPerPixel) {
	/* Possible values are 8, 16 and 32, as checked earlier in the
	 * PLATFORM_SetVideoMode() function. */
	case 8:
		pixels += VIDEOMODE_dest_offset_left;
		SDL_VIDEO_BlitNormal8((Uint32 *)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height);
		break;
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		SDL_VIDEO_BlitNormal16((Uint32*)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, SDL_PALETTE_buffer.bpp16);
		break;
	default: /* SDL_VIDEO_screen->format->BitsPerPixel == 32 */
		pixels += VIDEOMODE_dest_offset_left * 4;
		SDL_VIDEO_BlitNormal32((Uint32 *)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, SDL_PALETTE_buffer.bpp32);
	}
}

static void DisplayWithScaling(void)
{
	register Uint32 quad;
	register int x;
	register Uint8 *screen = (UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	register Uint32 *pixels = (Uint32 *) SDL_VIDEO_screen->pixels;
	int i;
	int y = 0;
	int w1;
	int w = (VIDEOMODE_src_width) << 16;
	int h = (VIDEOMODE_src_height) << 16;
	register int dx = w / VIDEOMODE_dest_width;
	register int yy;
	int pos;
	int pitch4 = SDL_VIDEO_screen->pitch / 4;
	int dy = h / VIDEOMODE_dest_height;
	int init_x = (VIDEOMODE_src_width << 16) - 0x4000;

	Uint8 c;

	i = VIDEOMODE_dest_height;

	switch (SDL_VIDEO_screen->format->BitsPerPixel) {
	/* Possible values are 8, 16 and 32, as checked earlier in the
	 * PLATFORM_SetVideoMode() function. */
	case 8:
		pixels += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 4;
		w1 = VIDEOMODE_dest_width / 4 - 1;
		while (i > 0) {
			x = init_x;
			pos = w1;
			yy = Screen_WIDTH * (y >> 16);
			while (pos >= 0) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				quad = (screen[yy + (x >> 16)] << 0);
				x -= dx;
				quad += (screen[yy + (x >> 16)] << 8);
				x -= dx;
				quad += (screen[yy + (x >> 16)] << 16);
				x -= dx;
				quad += (screen[yy + (x >> 16)] << 24);
				x -= dx;
#else
				quad = (screen[yy + (x >> 16)] << 24);
				x -= dx;
				quad += (screen[yy + (x >> 16)] << 16);
				x -= dx;
				quad += (screen[yy + (x >> 16)] << 8);
				x -= dx;
				quad += (screen[yy + (x >> 16)] << 0);
				x -= dx;
#endif

				pixels[pos] = quad;
				pos--;

			}
			pixels += pitch4;
			y += dy;
			i--;
		}
		break;
	case 16:
		pixels += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 2;
		w1 = VIDEOMODE_dest_width / 2 - 1;
		while (i > 0) {
			x = init_x;
			pos = w1;
			yy = Screen_WIDTH * (y >> 16);
			while (pos >= 0) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				c = screen[yy + (x >> 16)];
				quad = SDL_PALETTE_buffer.bpp16[c];
				x -= dx;
				c = screen[yy + (x >> 16)];
				quad += SDL_PALETTE_buffer.bpp16[c] << 16;
				x -= dx;
#else
				c = screen[yy + (x >> 16)];
				quad = SDL_PALETTE_buffer.bpp16[c] << 16;
				x -= dx;
				c = screen[yy + (x >> 16)];
				quad += SDL_PALETTE_buffer.bpp16[c];
				x -= dx;
#endif

				pixels[pos] = quad;
				pos--;
			}
			pixels += pitch4;
			y += dy;
			i--;
		}
		break;
	default:
		pixels += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left;
		w1 = VIDEOMODE_dest_width - 1;
		/* SDL_VIDEO_screen->format->BitsPerPixel = 32 */
		while (i > 0) {
			x = init_x;
			pos = w1;
			yy = Screen_WIDTH * (y >> 16);
			while (pos >= 0) {
				c = screen[yy + (x >> 16)];
				quad = SDL_PALETTE_buffer.bpp32[c];
				x -= dx;
				pixels[pos] = quad;
				pos--;
			}
			pixels += pitch4;
			y += dy;
			i--;
		}
	}
}

#ifdef PAL_BLENDING
static void DisplayPalBlending(void)
{
	int pitch4 = SDL_VIDEO_screen->pitch / 4;
	UBYTE *screen = (UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	Uint8 *pixels = (Uint8 *) SDL_VIDEO_screen->pixels + SDL_VIDEO_screen->pitch * VIDEOMODE_dest_offset_top;
	switch (SDL_VIDEO_screen->format->BitsPerPixel) {
	/* Possible values are 8, 16 and 32, as checked earlier in the
	 * PLATFORM_SetVideoMode() function. */
	case 16:
		pixels += VIDEOMODE_dest_offset_left * 2;
		PAL_BLENDING_Blit16((ULONG*)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, VIDEOMODE_src_offset_top % 2);
		break;
	default: /* SDL_VIDEO_screen->format->BitsPerPixel == 32 */
		pixels += VIDEOMODE_dest_offset_left * 4;
		PAL_BLENDING_Blit32((ULONG *)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, VIDEOMODE_src_offset_top % 2);
	}
}

static void DisplayPalBlendingScaled(void)
{
	int pitch4 = SDL_VIDEO_screen->pitch / 4;
	Uint8 *screen = (UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	Uint32 *pixels = (Uint32 *) SDL_VIDEO_screen->pixels;
	switch (SDL_VIDEO_screen->format->BitsPerPixel) {
	/* Possible values are 8, 16 and 32, as checked earlier in the
	 * PLATFORM_SetVideoMode() function. */
	case 16:
		pixels += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 2;
		PAL_BLENDING_BlitScaled16((ULONG*)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, VIDEOMODE_dest_width, VIDEOMODE_dest_height, VIDEOMODE_src_offset_top % 2);
		break;
	case 32:
		pixels += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left;
		PAL_BLENDING_BlitScaled32((ULONG*)pixels, screen, pitch4, VIDEOMODE_src_width, VIDEOMODE_src_height, VIDEOMODE_dest_width, VIDEOMODE_dest_height, VIDEOMODE_src_offset_top % 2);
	}
}
#endif /* PAL_BLENDING */

void SDL_VIDEO_SW_DisplayScreen(void)
{
	if (SDL_LockSurface(SDL_VIDEO_screen) != 0)
		/* When the window manager decides to switch the SDL display from
		   fullscreen to windowed mode (eg. by minimising the window after the
		   user pressed Alt+Tab in Windows), hardware surface gets disabled
		   immediately. In such case surface locking will fail. When it happens,
		   don't blit to screen as it would cause a segfault. When fullscreen
		   mode gets re-enabled, surface locking will work again and screen
		   displaying will be restored */
		   return;
	/* Use function corresponding to the current_display_mode. */
	(*blit_funcs[SDL_VIDEO_current_display_mode])();
	SDL_UnlockSurface(SDL_VIDEO_screen);
	/* SDL_UpdateRect is faster than SDL_Flip for a software surface, because
	   it copies only the used part of the screen. */
	if (SDL_VIDEO_screen->flags & SDL_DOUBLEBUF)
		SDL_Flip(SDL_VIDEO_screen);
	else
		SDL_UpdateRect(SDL_VIDEO_screen, VIDEOMODE_dest_offset_left, VIDEOMODE_dest_offset_top, VIDEOMODE_dest_width, VIDEOMODE_dest_height);
}

int SDL_VIDEO_SW_ReadConfig(char *option, char *parameters)
{
	if (strcmp(option, "VIDEO_BPP") == 0) {
		int value = Util_sscandec(parameters);
		if (value != 0 && value != 8 && value != 16 && value != 32)
			return FALSE;
		else
			SDL_VIDEO_SW_bpp = value;
	}
	else
		return FALSE;
	return TRUE;
}

void SDL_VIDEO_SW_WriteConfig(FILE *fp)
{
	fprintf(fp, "VIDEO_BPP=%d\n", SDL_VIDEO_SW_bpp);
}

int SDL_VIDEO_SW_Initialise(int *argc, char *argv[])
{
	int i, j;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc);		/* is argument available? */
		int a_m = FALSE;			/* error, argument missing! */
		if (strcmp(argv[i], "-bpp") == 0) {
			if (i_a) {
				SDL_VIDEO_SW_bpp = Util_sscandec(argv[++i]);
				if (SDL_VIDEO_SW_bpp != 0 && SDL_VIDEO_SW_bpp != 8 && SDL_VIDEO_SW_bpp != 16 && SDL_VIDEO_SW_bpp != 32) {
					Log_print("Invalid BPP value %s", argv[i]);
					return FALSE;
				}
			}
			else a_m = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0)
				Log_print("\t-bpp <num>        Host color depth (0 = autodetect)");
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		}
	}
	*argc = j;

	return TRUE;
}

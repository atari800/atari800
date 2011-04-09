/*
 * sdl/video.c - SDL library specific port code - video display
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
#include "util.h"
#include "videomode.h"
#include "xep80.h"

#include "sdl/video.h"
#include "sdl/video_sw.h"
#if HAVE_OPENGL
#include "sdl/video_gl.h"
#endif

/* This value must be set during initialisation, because on Windows BPP
   autodetection works only before the first call to SDL_SetVideoMode(). */
int SDL_VIDEO_native_bpp;

int SDL_VIDEO_scanlines_percentage = 5;
int SDL_VIDEO_interpolate_scanlines = TRUE;
int SDL_VIDEO_width;
int SDL_VIDEO_height;

VIDEOMODE_MODE_t SDL_VIDEO_current_display_mode = VIDEOMODE_MODE_NORMAL;

/* Desktop screen resolution is stored here on initialisation. */
static VIDEOMODE_resolution_t desktop_resolution;

#if HAVE_OPENGL
int SDL_VIDEO_opengl_available;
int SDL_VIDEO_opengl = FALSE;
/* Was OpenGL active previously? */
static int currently_opengl = FALSE;
#endif
int SDL_VIDEO_vsync = FALSE;
int SDL_VIDEO_vsync_available;

void PLATFORM_PaletteUpdate(void)
{
	if (SDL_VIDEO_current_display_mode == VIDEOMODE_MODE_NTSC_FILTER)
		FILTER_NTSC_Update(FILTER_NTSC_emu);
	else {
#if HAVE_OPENGL
		if (SDL_VIDEO_opengl)
			SDL_VIDEO_GL_PaletteUpdate();
		else
#endif
			SDL_VIDEO_SW_PaletteUpdate();
	}
}

static void UpdateNtscFilter(VIDEOMODE_MODE_t mode)
{
	if (mode != VIDEOMODE_MODE_NTSC_FILTER && FILTER_NTSC_emu != NULL) {
		/* Turning filter off */
		FILTER_NTSC_Delete(FILTER_NTSC_emu);
		FILTER_NTSC_emu = NULL;
#if HAVE_OPENGL
		if (SDL_VIDEO_opengl)
			SDL_VIDEO_GL_PaletteUpdate();
		else
#endif
			SDL_VIDEO_SW_PaletteUpdate();
	}
	else if (mode == VIDEOMODE_MODE_NTSC_FILTER && FILTER_NTSC_emu == NULL) {
		/* Turning filter on */
		FILTER_NTSC_emu = FILTER_NTSC_New();
		FILTER_NTSC_Update(FILTER_NTSC_emu);
	}
}

void PLATFORM_SetVideoMode(VIDEOMODE_resolution_t const *res, int windowed, VIDEOMODE_MODE_t mode, int rotate90, int window_resized)
{
#if HAVE_OPENGL
	if (SDL_VIDEO_opengl) {
		if (!currently_opengl)
			SDL_VIDEO_SW_Cleanup();
		/* Switching to OpenGL can fail when the host machine doesn't
		   support it. If so, revert to software mode. */
		if (!SDL_VIDEO_GL_SetVideoMode(res, windowed, mode, rotate90, window_resized)) {
			SDL_VIDEO_opengl = SDL_VIDEO_opengl_available = FALSE;
			VIDEOMODE_Update();
		}
	} else {
		if (currently_opengl)
			SDL_VIDEO_GL_Cleanup();
		SDL_VIDEO_SW_SetVideoMode(res, windowed, mode, rotate90);
	}
	currently_opengl = SDL_VIDEO_opengl;
#else
	SDL_VIDEO_SW_SetVideoMode(res, windowed, mode, rotate90);
#endif
	SDL_VIDEO_current_display_mode = mode;
	UpdateNtscFilter(mode);
	PLATFORM_DisplayScreen();
}

VIDEOMODE_resolution_t *PLATFORM_AvailableResolutions(unsigned int *size)
{
	SDL_Rect **modes = SDL_ListModes(NULL, SDL_FULLSCREEN);
	VIDEOMODE_resolution_t *resolutions;
	unsigned int num_modes;
	unsigned int i;
	if (modes == (SDL_Rect**)0 || modes == (SDL_Rect**)-1)
		return NULL;
	/* Determine number of available modes. */
	for (num_modes = 0; modes[num_modes] != NULL; ++num_modes);

	resolutions = Util_malloc(num_modes * sizeof(VIDEOMODE_resolution_t));
	for (i = 0; i < num_modes; i++) {
		resolutions[i].width = modes[i]->w;
		resolutions[i].height = modes[i]->h;
	}
	*size = num_modes;

	return resolutions;
}

VIDEOMODE_resolution_t *PLATFORM_DesktopResolution(void)
{
	/* FIXME: This function should always return the current desktop
	   resolution, not a resolution stored on initialisation. */
	return &desktop_resolution;
}

int PLATFORM_SupportsVideomode(VIDEOMODE_MODE_t mode, int stretch, int rotate90)
{
#if HAVE_OPENGL
	if (SDL_VIDEO_opengl)
		return SDL_VIDEO_GL_SupportsVideomode(mode, stretch, rotate90);
	else
#endif
		return SDL_VIDEO_SW_SupportsVideomode(mode, stretch, rotate90);
}

void PLATFORM_DisplayScreen(void)
{
#if HAVE_OPENGL
	if (SDL_VIDEO_opengl)
		SDL_VIDEO_GL_DisplayScreen();
	else
#endif
		SDL_VIDEO_SW_DisplayScreen();
}

int SDL_VIDEO_ReadConfig(char *option, char *parameters)
{
	if (strcmp(option, "SCANLINES_PERCENTAGE") == 0) {
		int value = Util_sscandec(parameters);
		if (value < 0 || value > 100)
			return FALSE;
		else {
			SDL_VIDEO_scanlines_percentage = value;
		}
	}
	else if (strcmp(option, "INTERPOLATE_SCANLINES") == 0)
		return (SDL_VIDEO_interpolate_scanlines = Util_sscanbool(parameters)) != -1;
	else if (strcmp(option, "VIDEO_VSYNC") == 0)
		return (SDL_VIDEO_vsync = Util_sscanbool(parameters)) != -1;
#if HAVE_OPENGL
	else if (strcmp(option, "ENABLE_OPENGL") == 0)
		return (currently_opengl = SDL_VIDEO_opengl = Util_sscanbool(parameters)) != -1;
	else if (SDL_VIDEO_GL_ReadConfig(option, parameters)) {
	}
#endif /* HAVE_OPENGL */
	else if (SDL_VIDEO_SW_ReadConfig(option, parameters)) {
	}
	else
		return FALSE;
	return TRUE;
}

void SDL_VIDEO_WriteConfig(FILE *fp)
{
	fprintf(fp, "SCANLINES_PERCENTAGE=%d\n", SDL_VIDEO_scanlines_percentage);
	fprintf(fp, "INTERPOLATE_SCANLINES=%d\n", SDL_VIDEO_interpolate_scanlines);
	fprintf(fp, "VIDEO_VSYNC=%d\n", SDL_VIDEO_vsync);
#if HAVE_OPENGL
	fprintf(fp, "ENABLE_OPENGL=%d\n", SDL_VIDEO_opengl);
	SDL_VIDEO_GL_WriteConfig(fp);
#endif
	SDL_VIDEO_SW_WriteConfig(fp);
}

int SDL_VIDEO_Initialise(int *argc, char *argv[])
{
	int i, j;
	int help_only = FALSE;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc);		/* is argument available? */
		int a_m = FALSE;			/* error, argument missing! */
		if (strcmp(argv[i], "-scanlines") == 0) {
			if (i_a) {
				SDL_VIDEO_scanlines_percentage  = Util_sscandec(argv[++i]);
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-scanlinesint") == 0)
			SDL_VIDEO_interpolate_scanlines = TRUE;
		else if (strcmp(argv[i], "-no-scanlinesint") == 0)
			SDL_VIDEO_interpolate_scanlines = FALSE;
#if HAVE_OPENGL
		else if (strcmp(argv[i], "-opengl") == 0)
			SDL_VIDEO_opengl = TRUE;
		else if (strcmp(argv[i], "-no-opengl") == 0)
			SDL_VIDEO_opengl = FALSE;
#endif /* HAVE_OPENGL */
		else if (strcmp(argv[i], "-vsync") == 0)
			SDL_VIDEO_vsync = TRUE;
		else if (strcmp(argv[i], "-no-vsync") == 0)
			SDL_VIDEO_vsync = FALSE;
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-scanlines        Set visibility of scanlines (0..100)");
				Log_print("\t-scanlinesint     Enable scanlines interpolation");
				Log_print("\t-no-scanlinesint  Disable scanlines interpolation");
#if HAVE_OPENGL
				Log_print("\t-opengl           Enable hardware video acceleration");
				Log_print("\t-no-opengl        Disable hardware video acceleration");
#endif /* HAVE_OPENGL */
				Log_print("\t-vsync            Synchronize display to vertical retrace");
				Log_print("\t-no-vsync         Don't synchronize display to vertical retrace");
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		}
	}
	*argc = j;

	if (!SDL_VIDEO_SW_Initialise(argc, argv)
#if HAVE_OPENGL
	    || !SDL_VIDEO_GL_Initialise(argc, argv)
#endif
	)
		return FALSE;

	if (help_only)
		return TRUE; /* return before initialising SDL */

	/* Get the desktop resolution and bit depth. */
	{
		SDL_VideoInfo const * const info = SDL_GetVideoInfo();
		desktop_resolution.width = info->current_w;
		desktop_resolution.height = info->current_h;
		SDL_VIDEO_native_bpp = info->vfmt->BitsPerPixel;
	}

#if HAVE_OPENGL
	if (!SDL_VIDEO_opengl_available)
		currently_opengl = SDL_VIDEO_opengl = FALSE;
#endif
	return TRUE;
}

void SDL_VIDEO_Exit(void)
{
#if HAVE_OPENGL
	if (currently_opengl)
		SDL_VIDEO_GL_Cleanup();
	else
#endif
		SDL_VIDEO_SW_Cleanup();
}

void SDL_VIDEO_BlitNormal8(Uint32 *dest, Uint8 *src, int pitch, int width, int height)
{
	register Uint32 *start32 = dest;
	while (height > 0) {
		memcpy(start32, src, width);
		src += Screen_WIDTH;
		start32 += pitch;
		height--;
	}
}

void SDL_VIDEO_BlitNormal16(Uint32 *dest, Uint8 *src, int pitch, int width, int height, Uint16 *palette16)
{
	register Uint32 quad;
	register Uint32 *start32 = dest;
	register Uint8 c;
	register int pos;
	int width_32;
	if (width & 0x01)
		width_32 = width + 1;
	else
		width_32 = width;
	while (height > 0) {
		pos = width_32;
		do {
			pos--;
			c = src[pos];
			quad = palette16[c] << 16;
			pos--;
			c = src[pos];
			quad += palette16[c];
			start32[pos >> 1] = quad;
		} while (pos > 0);
		src += Screen_WIDTH;
		start32 += pitch;
		height--;
	}
}

void SDL_VIDEO_BlitNormal32(Uint32 *dest, Uint8 *src, int pitch, int width, int height, Uint32 *palette32)
{
	register Uint32 quad;
	register Uint32 *start32 = dest;
	register Uint8 c;
	register int pos;
	while (height > 0) {
		pos = width;
		do {
			pos--;
			c = src[pos];
			quad = palette32[c];
			start32[pos] = quad;
		} while (pos > 0);
		src += Screen_WIDTH;
		start32 += pitch;
		height--;
	}
}

void SDL_VIDEO_BlitXEP80_8(Uint32 *dest, Uint8 *src, int pitch, int width, int height)
{
	register Uint32 *start32 = dest;
	while (height > 0) {
		memcpy(start32, src, width);
		start32 += pitch;
		src += XEP80_SCRN_WIDTH;
		height--;
	}
}

void SDL_VIDEO_BlitXEP80_16(Uint32 *dest, Uint8 *src, int pitch, int width, int height, Uint16 *palette16)
{
	register Uint32 quad;
	register Uint32 *start32 = dest;
	register Uint8 c;
	register int pos;
	int width_32;
	if (width & 0x01)
		width_32 = width + 1;
	else
		width_32 = width;
	while (height > 0) {
		pos = width_32;
		do {
			pos--;
			c = src[pos];
			quad = palette16[c] << 16;
			pos--;
			c = src[pos];
			quad += palette16[c];
			start32[pos >> 1] = quad;
		} while (pos > 0);
		src += XEP80_SCRN_WIDTH;
		start32 += pitch;
		height--;
	}
}

void SDL_VIDEO_BlitXEP80_32(Uint32 *dest, Uint8 *src, int pitch, int width, int height, Uint32 *palette32)
{
	register Uint32 quad;
	register Uint32 *start32 = dest;
	register Uint8 c;
	register int pos;
	while (height > 0) {
		pos = width;
		do {
			pos--;
			c = src[pos];
			quad = palette32[c];
			start32[pos] = quad;
		} while (pos > 0);
		src += XEP80_SCRN_WIDTH;
		start32 += pitch;
		height--;
	}
}

void SDL_VIDEO_BlitProto80_8(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line)
{
	int skip = pitch - (last_column - first_column)*2;
	register Uint32 *start32 = dest;
	unsigned int column;
	UBYTE pixels;
	register Uint32 quad;
	for (; first_line < last_line; first_line++) {
		for (column = first_column; column < last_column; column++) {
			int i;
			pixels = PBI_PROTO80_GetPixels(first_line, column);
			for (i = 0; i < 2; i++) {
				if (pixels & 0x80)
					quad = 0x0000000f;
				else
					quad = 0x00000000;
				if (pixels & 0x40)
					quad |= 0x00000f00;
				if (pixels & 0x20)
					quad |= 0x000f0000;
				if (pixels & 0x10)
					quad |= 0x0f000000;
				*start32++ = quad;
				pixels <<= 4;
			}
		}
		start32 += skip;
	}
}

void SDL_VIDEO_BlitProto80_16(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line, Uint16 *palette16)
{
	Uint32 const black = (Uint32)palette16[0];
	Uint32 const white = (Uint32)palette16[15];
	Uint32 const black2 = black << 16;
	Uint32 const white2 = white << 16;
	int skip = pitch - (last_column - first_column)*4;
	register Uint32 *start32 = dest;
	unsigned int column;
	UBYTE pixels;
	register Uint32 quad;
	for (; first_line < last_line; first_line++) {
		for (column = first_column; column < last_column; column++) {
			int i;
			pixels = PBI_PROTO80_GetPixels(first_line, column);
			for (i = 0; i < 4; i++) {
				if (pixels & 0x80)
					quad = white;
				else
					quad = black;
				if (pixels & 0x40)
					quad |= white2;
				else
					quad |= black2;
				*start32++ = quad;
				pixels <<= 2;
			}
		}
		start32 += skip;
	}
}

void SDL_VIDEO_BlitProto80_32(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line, Uint32 *palette32)
{
	Uint32 const black = palette32[0];
	Uint32 const white = palette32[15];
	int skip = pitch - (last_column - first_column)*8;
	register Uint32 *start32 = dest;
	unsigned int column;
	UBYTE pixels;
	for (; first_line < last_line; first_line++) {
		for (column = first_column; column < last_column; column++) {
			int i;
			pixels = PBI_PROTO80_GetPixels(first_line, column);
			for (i = 0; i < 8; i++) {
				if (pixels & 0x80)
					*start32++ = white;
				else
					*start32++ = black;
				pixels <<= 1;
			}
		}
		start32 += skip;
	}
}

void SDL_VIDEO_BlitAF80_8(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line, int blink)
{
	int skip = pitch - (last_column - first_column)*2;
	register Uint32 *start32 = dest;
	unsigned int column;
	UBYTE pixels;
	register Uint32 quad;
	for (; first_line < last_line; first_line++) {
		for (column = first_column; column < last_column; column++) {
			int i;
			int colour;
			pixels = AF80_GetPixels(first_line, column, &colour, blink);
			for (i = 0; i < 2; i++) {
				if (pixels & 0x01)
					quad = colour;
				else
					quad = 0;
				if (pixels & 0x02)
					quad |= colour << 8;
				if (pixels & 0x04)
					quad |= colour << 16;
				if (pixels & 0x08)
					quad |= colour << 24;
				*start32++ = quad;
				pixels >>= 4;
			}
		}
		start32 += skip;
	}
}

void SDL_VIDEO_BlitAF80_16(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line, int blink, Uint16 *palette16)
{
	Uint32 const black = (Uint32)palette16[0];
	Uint32 const black2 = black << 16;
	int skip = pitch - (last_column - first_column)*4;
	register Uint32 *start32 = dest;
	unsigned int column;
	UBYTE pixels;
	register Uint32 quad;
	for (; first_line < last_line; first_line++) {
		for (column = first_column; column < last_column; column++) {
			int i;
			int colour;
			pixels = AF80_GetPixels(first_line, column, &colour, blink);
			for (i = 0; i < 4; i++) {
				if (pixels & 0x01)
					quad = palette16[colour];
				else
					quad = black;
				if (pixels & 0x02)
					quad |= palette16[colour] << 16;
				else
					quad |= black2;
				*start32++ = quad;
				pixels >>= 2;
			}
		}
		start32 += skip;
	}
}

void SDL_VIDEO_BlitAF80_32(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line, int blink, Uint32 *palette32)
{
	Uint32 const black = palette32[0];
	int skip = pitch - (last_column - first_column)*8;
	register Uint32 *start32 = dest;
	unsigned int column;
	UBYTE pixels;
	for (; first_line < last_line; first_line++) {
		for (column = first_column; column < last_column; column++) {
			int i;
			int colour;
			pixels = AF80_GetPixels(first_line, column, &colour, blink);
			for (i = 0; i < 8; i++) {
				if (pixels & 0x01)
					*start32++ = palette32[colour];
				else
					*start32++ = black;
				pixels >>= 1;
			}
		}
		start32 += skip;
	}
}

void SDL_VIDEO_SetScanlinesPercentage(int value)
{
	if (value < 0)
		value = 0;
	else if (value > 100)
		value = 100;
	SDL_VIDEO_scanlines_percentage = value;
#if HAVE_OPENGL
	SDL_VIDEO_GL_ScanlinesPercentageChanged();
#endif /* HAVE_OPENGL */
}

void SDL_VIDEO_SetInterpolateScanlines(int value)
{
	SDL_VIDEO_interpolate_scanlines = value;
#if HAVE_OPENGL
	SDL_VIDEO_GL_InterpolateScanlinesChanged();
#endif /* HAVE_OPENGL */
}

void SDL_VIDEO_ToggleInterpolateScanlines(void)
{
	SDL_VIDEO_SetInterpolateScanlines(!SDL_VIDEO_interpolate_scanlines);
}

#if HAVE_OPENGL
/* Sets an integer parameter and updates the video mode if needed. */
static int SetIntAndUpdateVideo(int *ptr, int value)
{
	int old_value = *ptr;
	if (old_value != value) {
		*ptr = value;
		if (!VIDEOMODE_Update()) {
			*ptr = old_value;
			return FALSE;
		}
	}
	return TRUE;
}

int SDL_VIDEO_SetOpengl(int value)
{
	if (!SDL_VIDEO_opengl_available)
		return FALSE;
	return SetIntAndUpdateVideo(&SDL_VIDEO_opengl, value);
}

int SDL_VIDEO_ToggleOpengl(void)
{
	return SDL_VIDEO_SetOpengl(!SDL_VIDEO_opengl);
}
#endif /* HAVE_OPENGL */

int SDL_VIDEO_SetVsync(int value)
{
	SDL_VIDEO_vsync = value;
	VIDEOMODE_Update();
	/* Return false if vsync is requested but not available. */
	return !SDL_VIDEO_vsync || SDL_VIDEO_vsync_available;
}

int SDL_VIDEO_ToggleVsync(void)
{
	return SDL_VIDEO_SetVsync(!SDL_VIDEO_vsync);
}

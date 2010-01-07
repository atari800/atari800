/*
 * colours.c - Atari colour palette adjustment - functions common for NTSC and
 *             PAL palettes
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2010 Atari800 development team (see DOC/CREDITS)
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

#include "config.h"
#include <stdio.h>
#include <string.h>	/* for strcmp() */
#include <math.h>
#include <stdlib.h>
#include "atari.h"
#include "colours.h"
#include "colours_external.h"
#include "colours_ntsc.h"
#include "colours_pal.h"
#include "log.h"
#include "util.h"
#include "platform.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

Colours_setup_t *Colours_setup;
COLOURS_EXTERNAL_t *Colours_external;

/* Default setup - used by Colours_RestoreDefaults(). For both NTSC and PAL. */
static const Colours_setup_t default_setup = {
	0.0, /* saturation */
	0.0, /* contrast */
	0.0, /* brightness */
	0.3, /* gamma adjustment */
	16, /* black level */
	235 /* white level */
	/* The above 16..235 range is taken from http://en.wikipedia.org/wiki/Rec._601 */
};

int Colours_table[256];

void Colours_SetRGB(int i, int r, int g, int b, int *colortable_ptr)
{
	if (r < 0)
		r = 0;
	else if (r > 255)
		r = 255;
	if (g < 0)
		g = 0;
	else if (g > 255)
		g = 255;
	if (b < 0)
		b = 0;
	else if (b > 255)
		b = 255;
	colortable_ptr[i] = (r << 16) + (g << 8) + b;
}

void Colours_InitialiseMachine(void)
{
	static int saved_tv_mode = Atari800_TV_UNSET;
	if (saved_tv_mode != Atari800_tv_mode) {
		/* TV mode has changed */
		saved_tv_mode = Atari800_tv_mode;
		Colours_SetVideoSystem(Atari800_tv_mode);
	}
}

void Colours_SetVideoSystem(int mode) {
	/* Set pointers to the currnt setup and external palette */
	if (mode == Atari800_TV_NTSC) {
		Colours_setup = &COLOURS_NTSC_setup;
		Colours_external = &COLOURS_NTSC_external;
	}
       	else if (mode == Atari800_TV_PAL) {
		Colours_setup = &COLOURS_PAL_setup;
		Colours_external = &COLOURS_PAL_external;
	}
	else {
		Atari800_Exit(FALSE);
		Log_print("Interal error: Invalid Atari800_tv_mode\n");
		exit(1);
	}
	/* Apply changes */
	Colours_Update();
}

/* Copies the loaded external palette into current palette - without applying
   adjustments. */
static void CopyExternalWithoutAdjustments(void)
{
	int i;
	unsigned char *ext_ptr;
	for (i = 0, ext_ptr = Colours_external->palette; i < 256; i ++, ext_ptr += 3)
		Colours_SetRGB(i, *ext_ptr, *(ext_ptr + 1), *(ext_ptr + 2), Colours_table);
}

void Colours_Update(void)
{
	if (Colours_external->loaded && !Colours_external->adjust)
		CopyExternalWithoutAdjustments();
	else if (Atari800_tv_mode == Atari800_TV_NTSC)
		COLOURS_NTSC_Update(Colours_table);
	else /* PAL */
		COLOURS_PAL_Update(Colours_table);
#if SUPPORTS_PLATFORM_PALETTEUPDATE
	PLATFORM_PaletteUpdate();
#endif
}

void Colours_RestoreDefaults(void)
{
	*Colours_setup = default_setup;
	if (Atari800_tv_mode == Atari800_TV_NTSC)
		COLOURS_NTSC_RestoreDefaults();
}

int Colours_Save(const char *filename)
{
	FILE *fp;
	int i;

	fp = fopen(filename, "w");
	if (fp == NULL) {
		return FALSE;
	}

	/* Create a raw 768-byte file with RGB values. */
	for (i = 0; i < 256; i ++) {
		char rgb[3];
		rgb[0] = Colours_GetR(i);
		rgb[1] = Colours_GetG(i);
		rgb[2] = Colours_GetB(i);
		if (fwrite(rgb, sizeof(rgb), 1, fp) != 1) {
			fclose(fp);
			return FALSE;
		}
	}

	fclose(fp);
	return TRUE;
}

int Colours_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	/* Copy default setup for both NTSC and PAL. */
	COLOURS_NTSC_setup = COLOURS_PAL_setup = default_setup;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc);		/* is argument available? */
		int a_m = FALSE;			/* error, argument missing! */
		
		if (strcmp(argv[i], "-saturation") == 0) {
			if (i_a)
				COLOURS_NTSC_setup.saturation = COLOURS_PAL_setup.saturation = atof(argv[++i]);
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-contrast") == 0) {
			if (i_a)
				COLOURS_NTSC_setup.contrast = COLOURS_PAL_setup.contrast = atof(argv[++i]);
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-brightness") == 0) {
			if (i_a)
				COLOURS_NTSC_setup.brightness = COLOURS_PAL_setup.brightness = atof(argv[++i]);
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-gamma") == 0) {
			if (i_a)
				COLOURS_NTSC_setup.gamma = COLOURS_PAL_setup.gamma = atof(argv[++i]);
			else a_m = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-saturation <num> Set saturation of colours");
				Log_print("\t-contrast <num>   Set contrast");
				Log_print("\t-brightness <num> Set brightness");
				Log_print("\t-gamma <num>      Set colour gamma factor");
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		}
	}
	*argc = j;

	if (!COLOURS_NTSC_Initialise(argc, argv) ||
	    !COLOURS_PAL_Initialise(argc, argv))
		return FALSE;

	return TRUE;
}

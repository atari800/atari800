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

/* Settings for "Standard", Rec. 601 analog video raster standard */
static const Colours_setup_t default_setup = {
	0.0, /* saturation */
	0.0, /* contrast */
	0.0, /* brightness */
	0.3, /* gamma adjustment */
	16,  /* black level */
	235  /* white level */
};

/* Settings for "Classic" black blacks style calibration */
static const Colours_setup_t classic_setup = {
	/* The above 16..235 range is taken from http://en.wikipedia.org/wiki/Rec._601 */
	0.0,   /* saturation */
	0.2,   /* contrast */
	-0.15, /* brightness */
	0.5,   /* gamma adjustment */
	16,    /* black level */
	235    /* white level */
};

/* Settings for vibrant "Arcade" style calibration */
static const Colours_setup_t arcade_setup = {
	0.25,  /* saturation */
	0.7,   /* contrast */
	-0.15, /* brightness */
	0.15,  /* gamma adjustment */
	16,    /* black level */
	235    /* white level */
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

static void UpdateModeDependentPointers(int tv_mode)
{
	/* Set pointers to the currnt setup and external palette */
	if (tv_mode == Atari800_TV_NTSC) {
		Colours_setup = &COLOURS_NTSC_setup;
		Colours_external = &COLOURS_NTSC_external;
	}
       	else if (tv_mode == Atari800_TV_PAL) {
		Colours_setup = &COLOURS_PAL_setup;
		Colours_external = &COLOURS_PAL_external;
	}
	else {
		Atari800_Exit(FALSE);
		Log_print("Interal error: Invalid Atari800_tv_mode\n");
		exit(1);
	}
}

void Colours_SetVideoSystem(int mode)
{
	UpdateModeDependentPointers(mode);
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

/* Updates contents of Colours_table. */
static void UpdatePalette(void)
{
	if (Colours_external->loaded && !Colours_external->adjust)
		CopyExternalWithoutAdjustments();
	else if (Atari800_tv_mode == Atari800_TV_NTSC)
		COLOURS_NTSC_Update(Colours_table);
	else /* PAL */
		COLOURS_PAL_Update(Colours_table);
}

void Colours_Update(void)
{
	UpdatePalette();
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

/* Sets the video calibration profile to the user preference */
void Colours_Set_Calibration_Profile(COLOURS_VIDEO_PROFILE cp)
{
	switch (cp)
	{
		case COLOURS_STANDARD:
			*Colours_setup = default_setup;
			break;
		case COLOURS_CLASSIC:
			*Colours_setup = classic_setup;
			break;
		case COLOURS_ARCADE:
			*Colours_setup = arcade_setup;
			break;
		default:
			*Colours_setup = default_setup;
	}

	if (Atari800_tv_mode == Atari800_TV_NTSC) 
		COLOURS_NTSC_Set_Calibration_Profile(COLOURS_STANDARD);
		
#ifdef DIRECTX
    /* Support PLATFORM_Configure */
	Colours_video_profile = cp;
#endif
}

/* Compares the current settings to the available calibration profiles
   and returns the matching profile -- or CUSTOM if no match is found */
COLOURS_VIDEO_PROFILE Colours_Get_Calibration_Profile()
{
	if (Atari800_tv_mode == Atari800_TV_NTSC) {
		if (COLOURS_NTSC_setup.saturation == default_setup.saturation &&
			COLOURS_NTSC_setup.contrast == default_setup.contrast &&
			COLOURS_NTSC_setup.brightness == default_setup.brightness &&
			COLOURS_NTSC_setup.gamma == default_setup.gamma &&
			COLOURS_NTSC_setup.black_level == default_setup.black_level &&
			COLOURS_NTSC_setup.white_level == default_setup.white_level &&
			COLOURS_NTSC_Get_Calibration_Profile() == COLOURS_STANDARD)
			return COLOURS_STANDARD; 
		else if (COLOURS_NTSC_setup.saturation == classic_setup.saturation &&
			COLOURS_NTSC_setup.contrast == classic_setup.contrast &&
			COLOURS_NTSC_setup.brightness == classic_setup.brightness &&
			COLOURS_NTSC_setup.gamma == classic_setup.gamma &&
			COLOURS_NTSC_setup.black_level == classic_setup.black_level &&
			COLOURS_NTSC_setup.white_level == classic_setup.white_level &&
			COLOURS_NTSC_Get_Calibration_Profile() == COLOURS_STANDARD)
			return COLOURS_CLASSIC;
		else if (COLOURS_NTSC_setup.saturation == arcade_setup.saturation &&
			COLOURS_NTSC_setup.contrast == arcade_setup.contrast &&
			COLOURS_NTSC_setup.brightness == arcade_setup.brightness &&
			COLOURS_NTSC_setup.gamma == arcade_setup.gamma &&
			COLOURS_NTSC_setup.black_level == arcade_setup.black_level &&
			COLOURS_NTSC_setup.white_level == arcade_setup.white_level &&
			COLOURS_NTSC_Get_Calibration_Profile() == COLOURS_STANDARD)
			return COLOURS_ARCADE;
	}
		
	if (Atari800_tv_mode == Atari800_TV_PAL) {
		if (COLOURS_PAL_setup.saturation == default_setup.saturation &&
			COLOURS_PAL_setup.contrast == default_setup.contrast &&
			COLOURS_PAL_setup.brightness == default_setup.brightness &&
			COLOURS_PAL_setup.gamma == default_setup.gamma &&
			COLOURS_PAL_setup.black_level == default_setup.black_level &&
			COLOURS_PAL_setup.white_level == default_setup.white_level)
			return COLOURS_STANDARD; 
		else if (COLOURS_NTSC_setup.saturation == classic_setup.saturation &&
			COLOURS_PAL_setup.contrast == classic_setup.contrast &&
			COLOURS_PAL_setup.brightness == classic_setup.brightness &&
			COLOURS_PAL_setup.gamma == classic_setup.gamma &&
			COLOURS_PAL_setup.black_level == classic_setup.black_level &&
			COLOURS_PAL_setup.white_level == classic_setup.white_level)
			return COLOURS_CLASSIC;
		else if (COLOURS_NTSC_setup.saturation == arcade_setup.saturation &&
			COLOURS_PAL_setup.contrast == arcade_setup.contrast &&
			COLOURS_PAL_setup.brightness == arcade_setup.brightness &&
			COLOURS_PAL_setup.gamma == arcade_setup.gamma &&
			COLOURS_PAL_setup.black_level == arcade_setup.black_level &&
			COLOURS_PAL_setup.white_level == arcade_setup.white_level)
			return COLOURS_ARCADE;
	}
	
	return COLOURS_CUSTOM;
}

int Colours_Save(const char *filename)
{
	FILE *fp;
	int i;

	fp = fopen(filename, "wb");
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

#ifdef DIRECTX
	/* Support PLATFORM_Configure */
	/* Copy appropriate setup for both NTSC and PAL. */
	switch (Colours_video_profile)
	{
		case COLOURS_STANDARD:
			COLOURS_NTSC_setup = COLOURS_PAL_setup = default_setup;
			break;
		case COLOURS_CLASSIC:
			COLOURS_NTSC_setup = COLOURS_PAL_setup = classic_setup;
			break;
		case COLOURS_ARCADE:
			COLOURS_NTSC_setup = COLOURS_PAL_setup = arcade_setup;
			break;
		default:
			COLOURS_NTSC_setup = COLOURS_PAL_setup = default_setup;
	}
#else
	/* Copy default setup for both NTSC and PAL. */
	COLOURS_NTSC_setup = COLOURS_PAL_setup = default_setup;
#endif

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
		else if (strcmp(argv[i], "-video-profile") == 0) {
			if (i_a) {
				if (strcmp(argv[++i], "standard") == 0) {
				    COLOURS_NTSC_setup = COLOURS_PAL_setup = default_setup;
				}
				else if (strcmp(argv[i], "classic") == 0) {
				    COLOURS_NTSC_setup = COLOURS_PAL_setup = classic_setup;
				}
				else if (strcmp(argv[i], "arcade") == 0) {
					COLOURS_NTSC_setup = COLOURS_PAL_setup = arcade_setup;
				} else {
					Log_print("Invalid value for -video-profile");
					return FALSE;
				}
			} else a_m = TRUE;
		}

		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-video-profile <name>  Set video profile <standard> <classic> <arcade>");
				Log_print("\t-saturation <num>      Set saturation of colours");
				Log_print("\t-contrast <num>        Set contrast");
				Log_print("\t-brightness <num>      Set brightness");
				Log_print("\t-gamma <num>           Set colour gamma factor");
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

	/* Assume that Atari800_tv_mode has been already initialised. */
	UpdateModeDependentPointers(Atari800_tv_mode);
	UpdatePalette();
	return TRUE;
}

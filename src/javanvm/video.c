/*
 * javanvm/video.c - NestedVM-specific port code - video display
 *
 * Copyright (c) 2001-2002 Jacek Poplawski (original atari_sdl.c)
 * Copyright (c) 2007-2008 Perry McFarlane (javanvm port)
 * Copyright (C) 2001-2008 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.

 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with Atari800; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "javanvm/video.h"

#include "colours.h"
#include "screen.h"
#include "log.h"
#include "platform.h"
#include "util.h"
#include "javanvm/javanvm.h"

/* These functions call the NestedVM runtime */
static void JAVANVM_DisplayScreen(void *as)
{
	_call_java(JAVANVM_FUN_DisplayScreen, (int)as, 0, 0);
}

static void JAVANVM_InitPalette(void *ct)
{
	_call_java(JAVANVM_FUN_InitPalette, (int)ct, 0, 0);
}

static int JAVANVM_InitGraphics(void *config)
{
	return _call_java(JAVANVM_FUN_InitGraphics, (int)config, 0, 0);
}

void PLATFORM_PaletteUpdate(void)
{
	JAVANVM_InitPalette((void *)&Colours_table[0]);
}

int JAVANVM_VIDEO_Initialise(int *argc, char *argv[])
{
	int i, j;
	int help_only = FALSE;
	int scale = 2;
	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc);		/* is argument available? */
		int a_m = FALSE;			/* error, argument missing! */

		if (strcmp(argv[i], "-scale") == 0) {
			if (i_a)
				scale = Util_sscandec(argv[++i]);
			else a_m = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-scale <n>       Scale width and height by <n>");
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		}
	}
	*argc = j;

	if (!help_only) {
        int config[JAVANVM_InitGraphicsSIZE];
        config[JAVANVM_InitGraphicsScalew] = scale;
        config[JAVANVM_InitGraphicsScaleh] = scale;
        config[JAVANVM_InitGraphicsScreen_WIDTH] = Screen_WIDTH;
        config[JAVANVM_InitGraphicsScreen_HEIGHT] = Screen_HEIGHT;
        config[JAVANVM_InitGraphicsATARI_VISIBLE_WIDTH] = 336;
        config[JAVANVM_InitGraphicsATARI_LEFT_MARGIN] = 24;
		JAVANVM_InitGraphics((void *)&config[0]);
		JAVANVM_InitPalette((void *)&Colours_table[0]);
	}
	return TRUE;
}

void PLATFORM_DisplayScreen(void){
	JAVANVM_DisplayScreen((void *)Screen_atari);
	return;
}

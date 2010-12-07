/*
 * sdl/palette.c - SDL library specific port code - table of display palettes
 *
 * Copyright (c) 2010 Tomasz Krasuski
 * Copyright (C) 2010 Atari800 development team (see DOC/CREDITS)
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

#include "sdl/palette.h"
#include "af80.h"
#include "colours.h"
#include "videomode.h"

struct SDL_PALETTE_tab_t const SDL_PALETTE_tab[VIDEOMODE_MODE_SIZE] = {
	{ Colours_table, 256 }, /* Standard display */
	{ Colours_table, 256 }, /* NTSC filter */
	{ Colours_table, 256 }, /* XEP80 also uses the standard palette */
	{ Colours_table, 256 }, /* So does PBI Proto80 */
	{ AF80_palette, 16 } /* AF80 */
};
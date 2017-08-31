/*
 * bit3.c - Emulation of the Bit3 Full View 80 column card.
 *
 * Copyright (C) 2009 Perry McFarlane
 * Copyright (C) 2009 Atari800 development team (see DOC/CREDITS)
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

#include "bit3.h"
#include "atari.h"
#include "util.h"
#include "log.h"
#include "memory.h"
#include "cpu.h"
#include "videomode.h"
#include <stdlib.h>

static UBYTE *bit3_rom = NULL;
static char bit3_rom_filename[FILENAME_MAX];
static UBYTE *bit3_charset = NULL;
static char bit3_charset_filename[FILENAME_MAX];

static UBYTE *bit3_screen = NULL;

int BIT3_enabled = FALSE;
static int video_latch = 0;
static int rom_bank_select; /* bits 5 and 0-2 of d508, $0-$f 16 banks */
static UBYTE crtreg[0x40];

int BIT3_palette[2] = {
	0x000000, /* black */
	0xFFFFFF  /* white (high intensity) */
};

#ifdef BIT3_DEBUG
#define D(a) a
#else
#define D(a) do{}while(0)
#endif

static void update_d6(void)
{
	memcpy(MEMORY_mem + 0xd600, bit3_rom + (rom_bank_select<<8), 0x100);
}

int BIT3_Initialise(int *argc, char *argv[])
{
	int i, j;
	int help_only = FALSE;
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-bit3") == 0) {
			BIT3_enabled = TRUE;
		}
		else {
		 	if (strcmp(argv[i], "-help") == 0) {
		 		help_only = TRUE;
				Log_print("\t-bit3            Emulate the Bit3 Full View 80 column board");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	if (help_only)
		return TRUE;

	if (BIT3_enabled) {
		Log_print("Bit 3 Full View enabled");
		bit3_rom = (UBYTE *)Util_malloc(0x1000);
		if (!Atari800_LoadImage(bit3_rom_filename, bit3_rom, 0x1000)) {
			free(bit3_rom);
			bit3_rom = NULL;
			BIT3_enabled = FALSE;
			Log_print("Couldn't load Bit3 Full View ROM image");
			return FALSE;
		}
		else {
			Log_print("loaded Bit3 Full View ROM image");
		}
		bit3_charset = (UBYTE *)Util_malloc(0x1000);
		if (!Atari800_LoadImage(bit3_charset_filename, bit3_charset, 0x1000)) {
			free(bit3_charset);
			free(bit3_rom);
			bit3_charset = bit3_rom = NULL;
			BIT3_enabled = FALSE;
			Log_print("Couldn't load Bit3 Full View charset image");
			return FALSE;
		}
		else {
			Log_print("loaded Bit3 Full View charset image");
		}
		bit3_screen = (UBYTE *)Util_malloc(0x800);
		VIDEOMODE_80_column = 0; /* Disable 80 column mode if set in .cfg, Bit3 uses software control for this */
		BIT3_Reset(); /* With VIDEOMODE_80_column = 0, VIDEOMODE_Set80Column(0) will not change modes */

	}

	return TRUE;
}

void BIT3_Exit(void)
{
	free(bit3_screen);
	free(bit3_charset);
	free(bit3_rom);
	bit3_screen = bit3_charset = bit3_rom = NULL;
}

int BIT3_ReadConfig(char *string, char *ptr)
{
	if (strcmp(string, "BIT3_ROM") == 0)
		Util_strlcpy(bit3_rom_filename, ptr, sizeof(bit3_rom_filename));
	else if (strcmp(string, "BIT3_CHARSET") == 0)
		Util_strlcpy(bit3_charset_filename, ptr, sizeof(bit3_charset_filename));
	else return FALSE; /* no match */
	return TRUE; /* matched something */
}

void BIT3_WriteConfig(FILE *fp)
{
	fprintf(fp, "BIT3_ROM=%s\n", bit3_rom_filename);
	fprintf(fp, "BIT3_CHARSET=%s\n", bit3_charset_filename);
}

int BIT3_D6GetByte(UWORD addr, int no_side_effects)
{
	int result = MEMORY_dGetByte(addr);
	return result;
}

void BIT3_D6PutByte(UWORD addr, UBYTE byte)
{
	return;
}

int BIT3_D5GetByte(UWORD addr, int no_side_effects)
{
	int result=0xff;
	if (addr == 0xd508) {
	}
	else if (addr == 0xd580) {
		/* crtc status TODO */
	}
	else if (addr == 0xd581) {
		result = crtreg[crtreg[0x00]&0x3f];
	}
	else if (addr == 0xd583 || addr == 0xd585) {
		/* d583 is used for reading screen ram, d585 for writing, in the ROM.
		 * This code supports both since the manual only mentions using 
		 * d583 for read/write */
		result = bit3_screen[(((crtreg[0x12]&0x07)<<8)|crtreg[0x13])];
		if(crtreg[0x13] == 0) {
			crtreg[0x12] = ((crtreg[0x12]+1)&0x3f);
		}
	}
	else {
   		result = MEMORY_dGetByte(addr);
	}
	return result;
}

void BIT3_D5PutByte(UWORD addr, UBYTE byte)
{
	if (addr == 0xd508) {
		/* ROM bank bits 0-2 and bit 5 */
		/* The manual says bit 3 unblanks the 80x24 display and bit 4 turns the video switch to 80 x 24 (from 40 col)*/
			if (rom_bank_select != (((byte & 0x20)>>2)|(byte & 0x07))) {
				rom_bank_select = (((byte & 0x20)>>2)|(byte & 0x07));
				update_d6();
			};
			if (video_latch != !!(byte & 0x10)){
				video_latch = !!(byte & 0x10);
				VIDEOMODE_Set80Column(video_latch);
			}
	}
	else if (addr == 0xd580) {
		/* select crtc register */
		crtreg[0] = byte;
	}
	else if (addr == 0xd581) {
		/* write selected crtc register */
		crtreg[crtreg[0]&0x3f] = byte;
	}
	else if (addr == 0xd583 || addr == 0xd585) {
		/* d583 is used for reading screen ram, d585 for writing, in the ROM.
		 * This code supports both since the manual only mentions using 
		 * d583 for read/write */
		bit3_screen[(((crtreg[0x12]&0x07)<<8)|crtreg[0x13])] = byte;
		crtreg[0x13]++;
		if(crtreg[0x13] == 0) {
			crtreg[0x12] = ((crtreg[0x12]+1)&0x3f);
		}
	}
}

UBYTE BIT3_GetPixels(int scanline, int column, int *colour, int blink)
{
#define BIT3_ROWS 24
#define BIT3_CELL_HEIGHT 10
	UBYTE character;
	UBYTE font_data;
	int table_start = crtreg[0x0d] + ((crtreg[0x0c]&0x3f)<<8);
	int row = scanline / BIT3_CELL_HEIGHT;
	int line = scanline % BIT3_CELL_HEIGHT;
	int screen_pos;

	if (row  >= BIT3_ROWS) {
		return 0;
	}
	screen_pos = ((row*80+column + table_start)&0x3fff);
	character = bit3_screen[screen_pos&0x7ff];
	font_data = bit3_charset[(character&0x7f)*16 + line];
	if (character & 0x80) {
		font_data ^= 0xff; /* invert */
	}
	if (screen_pos == (((crtreg[0x0e]&0x3f)<<8)|crtreg[0x0f]) && !blink) {
		if (line >= (crtreg[0x0a]&0x1f) && line <= (crtreg[0x0b]&0x1f)){
			if ((crtreg[0x0a]&0x60) == 0x00 ||
			((crtreg[0x0a]&0x60) == 0x40 && !blink) ||
			((crtreg[0x0a]&0x60) == 0x60 && !blink)) {
					/* 0x00: no blinking */
					/* 0x20: no cursor */
					/* 0x40: blink at 1/16 field rate */
					/* 0x60: blink at 1/32 field rate TODO */
				font_data ^= 0xff; /* cursor */
			}
		}
	}
	*colour = 1; /* set number of palette entry for foreground pixels */
	return font_data;
}

void BIT3_Reset(void)
{
	memset(bit3_screen, 0, 0x800);
	rom_bank_select = 0;
	memset(crtreg, 0, sizeof(crtreg));
	update_d6();
	video_latch = 0;
	VIDEOMODE_Set80Column(video_latch);
}

/*
vim:ts=4:sw=4:
*/

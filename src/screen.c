/*
 * screen.c - Atari screen handling
 *
 * Copyright (c) 2001 Robert Golias and Piotr Fusik
 * Copyright (C) 2001-2008 Atari800 development team (see DOC/CREDITS)
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

#define _POSIX_C_SOURCE 200112L /* for snprintf */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antic.h"
#include "atari.h"
#include "cassette.h"
#include "colours.h"
#include "log.h"
#include "pia.h"
#include "screen.h"
#include "sio.h"
#include "util.h"
#include "file_export.h"

ULONG *Screen_atari = NULL;
#ifdef DIRTYRECT
UBYTE *Screen_dirty = NULL;
#endif
#ifdef BITPL_SCR
ULONG *Screen_atari_b = NULL;
ULONG *Screen_atari1 = NULL;
ULONG *Screen_atari2 = NULL;
#endif

/* The area that can been seen is Screen_visible_x1 <= x < Screen_visible_x2,
   Screen_visible_y1 <= y < Screen_visible_y2.
   Full Atari screen is 336x240. Screen_WIDTH is 384 only because
   the code in antic.c sometimes draws more than 336 bytes in a line.
   Currently Screen_visible variables are used only to place
   disk led and snailmeter in the corners of the screen.
*/
int Screen_visible_x1 = 24;				/* 0 .. Screen_WIDTH */
int Screen_visible_y1 = 0;				/* 0 .. Screen_HEIGHT */
int Screen_visible_x2 = 360;			/* 0 .. Screen_WIDTH */
int Screen_visible_y2 = Screen_HEIGHT;	/* 0 .. Screen_HEIGHT */

int Screen_show_atari_speed = FALSE;
int Screen_show_disk_led = TRUE;
int Screen_show_sector_counter = FALSE;
int Screen_show_1200_leds = TRUE;

#ifndef DREAMCAST
#ifdef HAVE_LIBPNG
#define DEFAULT_SCREENSHOT_FILENAME_FORMAT "atari%03d.png"
#else
#define DEFAULT_SCREENSHOT_FILENAME_FORMAT "atari%03d.pcx"
#endif

static char screenshot_filename_format[FILENAME_MAX] = DEFAULT_SCREENSHOT_FILENAME_FORMAT;
static int screenshot_no_last = -1;
static int screenshot_no_max = 1000;

#endif /* !DREAMCAST */

int Screen_Initialise(int *argc, char *argv[])
{
	int i;
	int j;
	int help_only = FALSE;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc);		/* is argument available? */
		int a_m = FALSE;			/* error, argument missing! */

#ifndef DREAMCAST
		if (strcmp(argv[i], "-screenshots") == 0) {
			if (i_a)
				screenshot_no_max = Util_filenamepattern(argv[++i], screenshot_filename_format, FILENAME_MAX, DEFAULT_SCREENSHOT_FILENAME_FORMAT);
			else a_m = TRUE;
		}
		else
#endif /* !DREAMCAST */
		if (strcmp(argv[i], "-showspeed") == 0) {
			Screen_show_atari_speed = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
#ifndef DREAMCAST
				Log_print("\t-screenshots <p> Set filename pattern for screenshots");
#endif /* !DREAMCAST */
				Log_print("\t-showspeed       Show percentage of actual speed");
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		}
	}
	*argc = j;

	/* don't bother mallocing Screen_atari with just "-help" */
	if (help_only)
		return TRUE;

	if (Screen_atari == NULL) { /* platform-specific code can initialize it */
		Screen_atari = (ULONG *) Util_malloc(Screen_HEIGHT * Screen_WIDTH);
		/* Clear the screen. */
		memset(Screen_atari, 0, Screen_HEIGHT * Screen_WIDTH);
#ifdef DIRTYRECT
		Screen_dirty = (UBYTE *) Util_malloc(Screen_HEIGHT * Screen_WIDTH / 8);
		Screen_EntireDirty();
#endif
#ifdef BITPL_SCR
		Screen_atari_b = (ULONG *) Util_malloc(Screen_HEIGHT * Screen_WIDTH);
		memset(Screen_atari_b, 0, Screen_HEIGHT * Screen_WIDTH);
		Screen_atari1 = Screen_atari;
		Screen_atari2 = Screen_atari_b;
#endif
	}

	return TRUE;
}

int Screen_ReadConfig(char *string, char *ptr)
{
	if (strcmp(string, "SCREEN_SHOW_SPEED") == 0)
		return (Screen_show_atari_speed = Util_sscanbool(ptr)) != -1;
	else if (strcmp(string, "SCREEN_SHOW_IO_ACTIVITY") == 0)
		return (Screen_show_disk_led = Util_sscanbool(ptr)) != -1;
	else if (strcmp(string, "SCREEN_SHOW_IO_COUNTER") == 0)
		return (Screen_show_sector_counter = Util_sscanbool(ptr)) != -1;
	else if (strcmp(string, "SCREEN_SHOW_1200XL_LEDS") == 0)
		return (Screen_show_1200_leds = Util_sscanbool(ptr)) != -1;
	else return FALSE;
	return TRUE;
}

void Screen_WriteConfig(FILE *fp)
{
	fprintf(fp, "SCREEN_SHOW_SPEED=%d\n", Screen_show_atari_speed);
	fprintf(fp, "SCREEN_SHOW_IO_ACTIVITY=%d\n", Screen_show_disk_led);
	fprintf(fp, "SCREEN_SHOW_IO_COUNTER=%d\n", Screen_show_sector_counter);
	fprintf(fp, "SCREEN_SHOW_1200XL_LEDS=%d\n", Screen_show_1200_leds);
}

#define SMALLFONT_WIDTH    5
#define SMALLFONT_HEIGHT   7
#define SMALLFONT_PERCENT  10
#define SMALLFONT_C        11
#define SMALLFONT_D        12
#define SMALLFONT_L        13
#define SMALLFONT_SLASH    14
#define SMALLFONT_____ 0x00
#define SMALLFONT___X_ 0x02
#define SMALLFONT__X__ 0x04
#define SMALLFONT__XX_ 0x06
#define SMALLFONT_X___ 0x08
#define SMALLFONT_X_X_ 0x0A
#define SMALLFONT_XX__ 0x0C
#define SMALLFONT_XXX_ 0x0E

static void SmallFont_DrawChar(UBYTE *screen, int ch, UBYTE color1, UBYTE color2)
{
	static const UBYTE font[15][SMALLFONT_HEIGHT] = {
		{
			SMALLFONT_____,
			SMALLFONT__X__,
			SMALLFONT_X_X_,
			SMALLFONT_X_X_,
			SMALLFONT_X_X_,
			SMALLFONT__X__,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT__X__,
			SMALLFONT_XX__,
			SMALLFONT__X__,
			SMALLFONT__X__,
			SMALLFONT__X__,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT_XX__,
			SMALLFONT___X_,
			SMALLFONT__X__,
			SMALLFONT_X___,
			SMALLFONT_XXX_,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT_XX__,
			SMALLFONT___X_,
			SMALLFONT__X__,
			SMALLFONT___X_,
			SMALLFONT_XX__,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT___X_,
			SMALLFONT__XX_,
			SMALLFONT_X_X_,
			SMALLFONT_XXX_,
			SMALLFONT___X_,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT_XXX_,
			SMALLFONT_X___,
			SMALLFONT_XXX_,
			SMALLFONT___X_,
			SMALLFONT_XX__,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT__X__,
			SMALLFONT_X___,
			SMALLFONT_XX__,
			SMALLFONT_X_X_,
			SMALLFONT__X__,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT_XXX_,
			SMALLFONT___X_,
			SMALLFONT__X__,
			SMALLFONT__X__,
			SMALLFONT__X__,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT__X__,
			SMALLFONT_X_X_,
			SMALLFONT__X__,
			SMALLFONT_X_X_,
			SMALLFONT__X__,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT__X__,
			SMALLFONT_X_X_,
			SMALLFONT__XX_,
			SMALLFONT___X_,
			SMALLFONT__X__,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT_X_X_,
			SMALLFONT___X_,
			SMALLFONT__X__,
			SMALLFONT_X___,
			SMALLFONT_X_X_,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT__X__,
			SMALLFONT_X_X_,
			SMALLFONT_X___,
			SMALLFONT_X_X_,
			SMALLFONT__X__,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT_XX__,
			SMALLFONT_X_X_,
			SMALLFONT_X_X_,
			SMALLFONT_X_X_,
			SMALLFONT_XX__,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT_X___,
			SMALLFONT_X___,
			SMALLFONT_X___,
			SMALLFONT_X___,
			SMALLFONT_XXX_,
			SMALLFONT_____
		},
		{
			SMALLFONT_____,
			SMALLFONT___X_,
			SMALLFONT___X_,
			SMALLFONT__X__,
			SMALLFONT__X__,
			SMALLFONT_X___,
			SMALLFONT_____
		}
	};
	int y;
	for (y = 0; y < SMALLFONT_HEIGHT; y++) {
		int src;
		int mask;
		src = font[ch][y];
		for (mask = 1 << (SMALLFONT_WIDTH - 1); mask != 0; mask >>= 1) {
			ANTIC_VideoPutByte(screen, (UBYTE) ((src & mask) != 0 ? color1 : color2));
			screen++;
		}
		screen += Screen_WIDTH - SMALLFONT_WIDTH;
	}
}

/* Returns screen address for placing the next character on the left of the
   drawn number. */
static UBYTE *SmallFont_DrawInt(UBYTE *screen, int n, UBYTE color1, UBYTE color2)
{
	do {
		SmallFont_DrawChar(screen, n % 10, color1, color2);
		screen -= SMALLFONT_WIDTH;
		n /= 10;
	} while (n > 0);
	return screen;
}

void Screen_DrawAtariSpeed(double cur_time)
{
	if (Screen_show_atari_speed) {
		static int percent_display = 100;
		static int last_updated = 0;
		static double last_time = 0;
		if ((cur_time - last_time) >= 0.5) {
			percent_display = (int) (100 * (Atari800_nframes - last_updated) / (cur_time - last_time) / (Atari800_tv_mode == Atari800_TV_PAL ? 50 : 60));
			last_updated = Atari800_nframes;
			last_time = cur_time;
		}
		/* if (percent_display < 99 || percent_display > 101) */
		{
			/* space for 5 digits - up to 99999% Atari speed */
			UBYTE *screen = (UBYTE *) Screen_atari + Screen_visible_x1 + 5 * SMALLFONT_WIDTH
			          	+ (Screen_visible_y2 - SMALLFONT_HEIGHT) * Screen_WIDTH;
			SmallFont_DrawChar(screen, SMALLFONT_PERCENT, 0x0c, 0x00);
			SmallFont_DrawInt(screen - SMALLFONT_WIDTH, percent_display, 0x0c, 0x00);
		}
	}
}

void Screen_DrawDiskLED(void)
{
	if (Screen_show_disk_led || Screen_show_sector_counter) {
		UBYTE *screen = (UBYTE *) Screen_atari + Screen_visible_x2 - SMALLFONT_WIDTH
				        + (Screen_visible_y2 - SMALLFONT_HEIGHT) * Screen_WIDTH;
		if (SIO_last_op_time > 0) {
			SIO_last_op_time--;
			if (Screen_show_disk_led) {
				SmallFont_DrawChar(screen, SIO_last_drive, 0x00, (UBYTE) (SIO_last_op == SIO_LAST_READ ? 0xac : 0x2b));
				SmallFont_DrawChar(screen -= SMALLFONT_WIDTH, SMALLFONT_D, 0x00, (UBYTE) (SIO_last_op == SIO_LAST_READ ? 0xac : 0x2b));
				screen -= SMALLFONT_WIDTH;
			}

			if (Screen_show_sector_counter)
				screen = SmallFont_DrawInt(screen, SIO_last_sector, 0x00, 0x88);
		}
		if ((CASSETTE_readable && !CASSETTE_record) ||
		    (CASSETTE_writable && CASSETTE_record)) {
			if (Screen_show_disk_led)
				SmallFont_DrawChar(screen, SMALLFONT_C, 0x00, (UBYTE) (CASSETTE_record ? 0x2b : 0xac));

			if (Screen_show_sector_counter) {
				/* Displaying tape length during saving is pointless since it would equal the number
				of the currently-written block, which is already displayed. */
				if (!CASSETTE_record) {
					screen = SmallFont_DrawInt(screen - SMALLFONT_WIDTH, CASSETTE_GetSize(), 0x00, 0x88);
					SmallFont_DrawChar(screen, SMALLFONT_SLASH, 0x00, 0x88);
				}
				SmallFont_DrawInt(screen - SMALLFONT_WIDTH, CASSETTE_GetPosition(), 0x00, 0x88);
			}
		}
	}
}

void Screen_Draw1200LED(void)
{
	if (Screen_show_1200_leds && Atari800_keyboard_leds) {
		UBYTE *screen = (UBYTE *) Screen_atari + Screen_visible_x1 + SMALLFONT_WIDTH * 10
			+ (Screen_visible_y2 - SMALLFONT_HEIGHT) * Screen_WIDTH;
		UBYTE portb = PIA_PORTB | PIA_PORTB_mask;
		if ((portb & 0x04) == 0) {
			SmallFont_DrawChar(screen, SMALLFONT_L, 0x00, 0x36);
			SmallFont_DrawChar(screen + SMALLFONT_WIDTH, 1, 0x00, 0x36);
		}
		screen += SMALLFONT_WIDTH * 3;
		if ((portb & 0x08) == 0) {
			SmallFont_DrawChar(screen, SMALLFONT_L, 0x00, 0x36);
			SmallFont_DrawChar(screen + SMALLFONT_WIDTH, 2, 0x00, 0x36);
		}
	}
}

#ifndef DREAMCAST
static int striendswith(const char *s1, const char *s2)
{
	int pos;
	pos = strlen(s1) - strlen(s2);
	if (pos < 0)
		return 0;
	return Util_stricmp(s1 + pos, s2) == 0;
}

int Screen_SaveScreenshot(const char *filename, int interlaced)
{
	int is_png;
	FILE *fp;
	ULONG *main_screen_atari;
	UBYTE *ptr1;
	UBYTE *ptr2;
	if (striendswith(filename, ".pcx"))
		is_png = 0;
#ifdef HAVE_LIBPNG
	else if (striendswith(filename, ".png"))
		is_png = 1;
#endif
	else
		return FALSE;
	fp = fopen(filename, "wb");
	if (fp == NULL)
		return FALSE;
	main_screen_atari = Screen_atari;
	ptr1 = (UBYTE *) Screen_atari;
	if (interlaced) {
		Screen_atari = (ULONG *) Util_malloc(Screen_WIDTH * Screen_HEIGHT);
		ptr2 = (UBYTE *) Screen_atari;
		ANTIC_Frame(TRUE); /* draw on Screen_atari */
	}
	else {
		ptr2 = NULL;
	}
	if (is_png) {
#ifdef HAVE_LIBPNG
		PNG_SaveScreen(fp, ptr1, ptr2);
#endif
	}
	else
		PCX_SaveScreen(fp, ptr1, ptr2);
	fclose(fp);
	if (interlaced) {
		free(Screen_atari);
		Screen_atari = main_screen_atari;
	}
	return TRUE;
}

void Screen_SaveNextScreenshot(int interlaced)
{
	char filename[FILENAME_MAX];
	Util_findnextfilename(screenshot_filename_format, &screenshot_no_last, screenshot_no_max, filename, sizeof(filename), TRUE);
	Screen_SaveScreenshot(filename, interlaced);
}
#endif /* !DREAMCAST */

void Screen_EntireDirty(void)
{
#ifdef DIRTYRECT
	if (Screen_dirty)
		memset(Screen_dirty, 1, Screen_WIDTH * Screen_HEIGHT / 8);
#endif /* DIRTYRECT */
}

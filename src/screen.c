/*
 * screen.c - Atari screen handling
 *
 * Copyright (c) 2001 Robert Golias and Piotr Fusik
 * Copyright (C) 2001-2005 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LIBPNG
#include <png.h>
#endif

#include "antic.h"
#include "atari.h"
#include "colours.h"
#include "log.h"
#include "screen.h"
#include "sio.h"
#include "util.h"

#define ATARI_VISIBLE_WIDTH 336
#define ATARI_LEFT_MARGIN 24

ULONG *atari_screen = NULL;
#ifdef DIRTYRECT
UBYTE *screen_dirty = NULL;
#endif
#ifdef BITPL_SCR
ULONG *atari_screen_b = NULL;
ULONG *atari_screen1 = NULL;
ULONG *atari_screen2 = NULL;
#endif

/* The area that can been seen is screen_visible_x1 <= x < screen_visible_x2,
   screen_visible_y1 <= y < screen_visible_y2.
   Full Atari screen is 336x240. ATARI_WIDTH is 384 only because
   the code in antic.c sometimes draws more than 336 bytes in a line.
   Currently screen_visible variables are used only to place
   disk led and snailmeter in the corners of the screen.
*/
int screen_visible_x1 = 24;				/* 0 .. ATARI_WIDTH */
int screen_visible_y1 = 0;				/* 0 .. ATARI_HEIGHT */
int screen_visible_x2 = 360;			/* 0 .. ATARI_WIDTH */
int screen_visible_y2 = ATARI_HEIGHT;	/* 0 .. ATARI_HEIGHT */

int show_atari_speed = FALSE;
int show_disk_led = TRUE;
int show_sector_counter = FALSE;

#ifdef HAVE_LIBPNG
#define DEFAULT_SCREENSHOT_FILENAME_FORMAT "atari%03d.png"
#else
#define DEFAULT_SCREENSHOT_FILENAME_FORMAT "atari%03d.pcx"
#endif

static char screenshot_filename_format[FILENAME_MAX] = DEFAULT_SCREENSHOT_FILENAME_FORMAT;
static int screenshot_no_max = 1000;

/* converts "foo%bar##.pcx" to "foo%%bar%02d.pcx" */
static void Screen_SetScreenshotFilenamePattern(const char *p)
{
	char *f = screenshot_filename_format;
	char no_width = '0';
	screenshot_no_max = 1;
	/* 9 because sprintf'ed "no" can be 9 digits */
	while (f < screenshot_filename_format + FILENAME_MAX - 9) {
		/* replace a sequence of hashes with e.g. "%05d" */
		if (*p == '#') {
			if (no_width > '0') /* already seen a sequence of hashes */
				break;          /* invalid */
			/* count hashes */
			do {
				screenshot_no_max *= 10;
				p++;
				no_width++;
				/* now no_width is the number of hashes seen so far
				   and p points after the counted hashes */
			} while (no_width < '9' && *p == '#'); /* no more than 9 hashes */
			*f++ = '%';
			*f++ = '0';
			*f++ = no_width;
			*f++ = 'd';
			continue;
		}
		if (*p == '%')
			*f++ = '%'; /* double the percents */
		*f++ = *p;
		if (*p == '\0')
			return; /* ok */
		p++;
	}
	Aprint("Invalid filename pattern for screenshots, using default.");
	strcpy(screenshot_filename_format, DEFAULT_SCREENSHOT_FILENAME_FORMAT);
	screenshot_no_max = 1000;
}

void Screen_Initialise(int *argc, char *argv[])
{
	int i;
	int j;
	int help_only = FALSE;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-screenshots") == 0) {
			Screen_SetScreenshotFilenamePattern(argv[++i]);
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Aprint("\t-screenshots <p> Set filename pattern for screenshots");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	/* don't bother mallocing atari_screen with just "-help" */
	if (help_only)
		return;

	if (atari_screen == NULL) { /* platform-specific code can initalize it in theory */
		atari_screen = (ULONG *) Util_malloc(ATARI_HEIGHT * ATARI_WIDTH);
#ifdef DIRTYRECT
		screen_dirty = (UBYTE *) Util_malloc(ATARI_HEIGHT * ATARI_WIDTH / 8);
		entire_screen_dirty();
#endif
#ifdef BITPL_SCR
		atari_screen_b = (ULONG *) Util_malloc(ATARI_HEIGHT * ATARI_WIDTH);
		atari_screen1 = atari_screen;
		atari_screen2 = atari_screen_b;
#endif
	}
}

#define SMALLFONT_WIDTH    5
#define SMALLFONT_HEIGHT   7
#define SMALLFONT_PERCENT  10
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
	static const UBYTE font[11][SMALLFONT_HEIGHT] = {
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
		}
	};
	int y;
	for (y = 0; y < SMALLFONT_HEIGHT; y++) {
		int src;
		int mask;
		src = font[ch][y];
		for (mask = 1 << (SMALLFONT_WIDTH - 1); mask != 0; mask >>= 1) {
			video_putbyte(screen, (UBYTE) ((src & mask) != 0 ? color1 : color2));
			screen++;
		}
		screen += ATARI_WIDTH - SMALLFONT_WIDTH;
	}
}

static void SmallFont_DrawInt(UBYTE *screen, int n, UBYTE color1, UBYTE color2)
{
	do {
		SmallFont_DrawChar(screen, n % 10, color1, color2);
		screen -= SMALLFONT_WIDTH;
		n /= 10;
	} while (n > 0);
}

void Screen_DrawAtariSpeed(void)
{
	if (show_atari_speed) {
		static int percent_display = 100;
		static int last_updated = 0;
		if ((nframes - last_updated) >= 25) {
			last_updated = nframes;
			percent_display = percent_atari_speed;
		}
		/* don't show if 99-101% */
		if (percent_display < 99 || percent_display > 101) {
			/* space for 5 digits - up to 99999% Atari speed */
			UBYTE *screen = (UBYTE *) atari_screen + screen_visible_x1 + 5 * SMALLFONT_WIDTH
				          + (screen_visible_y2 - SMALLFONT_HEIGHT) * ATARI_WIDTH;
			SmallFont_DrawChar(screen, SMALLFONT_PERCENT, 0x0c, 0x00);
			SmallFont_DrawInt(screen - SMALLFONT_WIDTH, percent_display, 0x0c, 0x00);
		}
	}
}

void Screen_DrawDiskLED(void)
{
	if (sio_last_op_time > 0) {
		UBYTE *screen;
		sio_last_op_time--;
		screen = (UBYTE *) atari_screen + screen_visible_x2 - SMALLFONT_WIDTH
			+ (screen_visible_y2 - SMALLFONT_HEIGHT) * ATARI_WIDTH;
		if (show_disk_led)
			SmallFont_DrawChar(screen, sio_last_drive, 0x00, (UBYTE) (sio_last_op == SIO_LAST_READ ? 0xac : 0x2b));
		if (show_sector_counter)
			SmallFont_DrawInt(screen - SMALLFONT_WIDTH, sio_last_sector, 0x00, 0x88);
	}
}

void Screen_FindScreenshotFilename(char *buffer)
{
	static int no = -1;
	static int overwrite = FALSE;

	for (;;) {
		if (++no >= screenshot_no_max) {
			no = 0;
			overwrite = TRUE;
		}
		sprintf(buffer, screenshot_filename_format, no);
		if (overwrite)
			break;
		if (!Util_fileexists(buffer))
			break; /* file does not exist - we can create it */
	}
}

static void fputw(int x, FILE *fp)
{
	fputc(x & 0xff, fp);
	fputc(x >> 8, fp);
}

static void Screen_SavePCX(FILE *fp, UBYTE *ptr1, UBYTE *ptr2)
{
	int i;
	int x;
	int y;
	UBYTE plane = 16;	/* 16 = Red, 8 = Green, 0 = Blue */
	UBYTE last;
	UBYTE count;

	fputc(0xa, fp);   /* pcx signature */
	fputc(0x5, fp);   /* version 5 */
	fputc(0x1, fp);   /* RLE encoding */
	fputc(0x8, fp);   /* bits per pixel */
	fputw(0, fp);     /* XMin */
	fputw(0, fp);     /* YMin */
	fputw(ATARI_VISIBLE_WIDTH - 1, fp); /* XMax */
	fputw(ATARI_HEIGHT - 1, fp);        /* YMax */
	fputw(0, fp);     /* HRes */
	fputw(0, fp);     /* VRes */
	for (i = 0; i < 48; i++)
		fputc(0, fp); /* EGA color palette */
	fputc(0, fp);     /* reserved */
	fputc(ptr2 != NULL ? 3 : 1, fp); /* number of bit planes */
	fputw(ATARI_VISIBLE_WIDTH, fp);  /* number of bytes per scan line per color plane */
	fputw(1, fp);     /* palette info */
	fputw(ATARI_VISIBLE_WIDTH, fp); /* screen resolution */
	fputw(ATARI_HEIGHT, fp);
	for (i = 0; i < 54; i++)
		fputc(0, fp);  /* unused */

	for (y = 0; y < ATARI_HEIGHT; ) {
		x = 0;
		do {
			last = ptr2 != NULL ? (((colortable[*ptr1] >> plane) & 0xff) + ((colortable[*ptr2] >> plane) & 0xff)) >> 1 : *ptr1;
			count = 0xc0;
			do {
				ptr1++;
				if (ptr2 != NULL)
					ptr2++;
				count++;
				x++;
			} while (last == (ptr2 != NULL ? (((colortable[*ptr1] >> plane) & 0xff) + ((colortable[*ptr2] >> plane) & 0xff)) >> 1 : *ptr1)
						&& count < 0xff && x < ATARI_VISIBLE_WIDTH);
			if (count > 0xc1 || last >= 0xc0)
				fputc(count, fp);
			fputc(last, fp);
		} while (x < ATARI_VISIBLE_WIDTH);

		if (ptr2 != NULL && plane) {
			ptr1 -= ATARI_VISIBLE_WIDTH;
			ptr2 -= ATARI_VISIBLE_WIDTH;
			plane -= 8;
		}
		else {
			ptr1 += ATARI_WIDTH - ATARI_VISIBLE_WIDTH;
			if (ptr2 != NULL) {
				ptr2 += ATARI_WIDTH - ATARI_VISIBLE_WIDTH;
				plane = 16;
			}
			y++;
		}
	}

	if (ptr2 == NULL) {
		/* write palette */
		fputc(0xc, fp);
		for (i = 0; i < 256; i++) {
			fputc(Palette_GetR(i), fp);
			fputc(Palette_GetG(i), fp);
			fputc(Palette_GetB(i), fp);
		}
	}
}

static int striendswith(const char *s1, const char *s2)
{
	int pos;
	pos = strlen(s1) - strlen(s2);
	if (pos < 0)
		return 0;
	return Util_stricmp(s1 + pos, s2) == 0;
}

#ifdef HAVE_LIBPNG
static void Screen_SavePNG(FILE *fp, UBYTE *ptr1, UBYTE *ptr2)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep rows[ATARI_HEIGHT];

	png_ptr = png_create_write_struct(
		PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL
	);
	if (png_ptr == NULL)
		return;
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
		return;
	png_init_io(png_ptr, fp);
	png_set_IHDR(
		png_ptr, info_ptr, ATARI_VISIBLE_WIDTH, ATARI_HEIGHT,
		8, ptr2 == NULL ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);
	if (ptr2 == NULL) {
		int i;
		png_color palette[256];
		for (i = 0; i < 256; i++) {
			palette[i].red = Palette_GetR(i);
			palette[i].green = Palette_GetG(i);
			palette[i].blue = Palette_GetB(i);
		}
		png_set_PLTE(png_ptr, info_ptr, palette, 256);
		for (i = 0; i < ATARI_HEIGHT; i++) {
			rows[i] = ptr1;
			ptr1 += ATARI_WIDTH;
		}
	}
	else {
		png_bytep ptr3;
		int x;
		int y;
		ptr3 = (png_bytep) Util_malloc(3 * ATARI_VISIBLE_WIDTH * ATARI_HEIGHT);
		for (y = 0; y < ATARI_HEIGHT; y++) {
			rows[y] = ptr3;
			for (x = 0; x < ATARI_VISIBLE_WIDTH; x++) {
				*ptr3++ = (png_byte) ((Palette_GetR(*ptr1) + Palette_GetR(*ptr2)) >> 1);
				*ptr3++ = (png_byte) ((Palette_GetG(*ptr1) + Palette_GetG(*ptr2)) >> 1);
				*ptr3++ = (png_byte) ((Palette_GetB(*ptr1) + Palette_GetB(*ptr2)) >> 1);
				ptr1++;
				ptr2++;
			}
			ptr1 += ATARI_WIDTH - ATARI_VISIBLE_WIDTH;
			ptr2 += ATARI_WIDTH - ATARI_VISIBLE_WIDTH;
		}
	}
	png_set_rows(png_ptr, info_ptr, rows);
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	if (ptr2 != NULL)
		free(rows[0]);
}
#endif /* HAVE_LIBPNG */

int Screen_SaveScreenshot(const char *filename, int interlaced)
{
	int is_png;
	FILE *fp;
	ULONG *main_atari_screen;
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
	main_atari_screen = atari_screen;
	ptr1 = (UBYTE *) atari_screen + ATARI_LEFT_MARGIN;
	if (interlaced) {
		atari_screen = (ULONG *) Util_malloc(ATARI_WIDTH * ATARI_HEIGHT);
		ptr2 = (UBYTE *) atari_screen + ATARI_LEFT_MARGIN;
		ANTIC_Frame(TRUE); /* draw on atari_screen */
	}
	else {
		ptr2 = NULL;
	}
#ifdef HAVE_LIBPNG
	if (is_png)
		Screen_SavePNG(fp, ptr1, ptr2);
	else
#endif
		Screen_SavePCX(fp, ptr1, ptr2);
	fclose(fp);
	if (interlaced) {
		free(atari_screen);
		atari_screen = main_atari_screen;
	}
	return TRUE;
}

void Screen_SaveNextScreenshot(int interlaced)
{
	char filename[FILENAME_MAX];
	Screen_FindScreenshotFilename(filename);
	Screen_SaveScreenshot(filename, interlaced);
}

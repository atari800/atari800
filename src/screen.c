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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antic.h"
#include "atari.h"
#include "config.h"
#include "colours.h"
#include "screen.h"

#ifdef HAVE_LIBPNG
#include <png.h>
#endif

#define ATARI_VISIBLE_WIDTH 336
#define ATARI_LEFT_MARGIN 24

void Screen_FindScreenshotFilename(char *buffer)
{
	int no = -1;
	FILE *fp;

	while (++no < 1000) {
#ifdef HAVE_LIBPNG
		sprintf(buffer, "atari%03d.png", no);
#else
		sprintf(buffer, "atari%03d.pcx", no);
#endif
		fp = fopen(buffer, "rb");
		if (fp == NULL)
			return; /*file does not exist - we can create it */
		fclose(fp);
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
	int xpos;
	int ypos;
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
	for (i=0; i < 48; i++)
		fputc(0, fp); /*EGA color palette */
	fputc(0, fp);     /* reserved */
	fputc(ptr2 != NULL ? 3 : 1, fp); /* number of bit planes */
	fputw(ATARI_VISIBLE_WIDTH, fp);  /* number of bytes per scan line per color plane */
	fputw(1, fp);     /* palette info */
	fputw(ATARI_VISIBLE_WIDTH, fp); /* screen resolution */
	fputw(ATARI_HEIGHT, fp);
	for (i=0; i<54; i++)
		fputc(0,fp);  /* unused */

	for (ypos = 0; ypos < ATARI_HEIGHT; ) {
		xpos = 0;
		do {
			last = ptr2 != NULL ? (((colortable[*ptr1] >> plane) & 0xff) + ((colortable[*ptr2] >> plane) & 0xff)) >> 1 : *ptr1;
			count = 0xc0;
			do {
				ptr1++;
				if (ptr2 != NULL)
					ptr2++;
				count++;
				xpos++;
			} while (last == (ptr2 != NULL ? (((colortable[*ptr1] >> plane) & 0xff) + ((colortable[*ptr2] >> plane) & 0xff)) >> 1 : *ptr1)
						&& count < 0xff && xpos < ATARI_VISIBLE_WIDTH);
			if (count > 0xc1 || last >= 0xc0)
				fputc(count, fp);
			fputc(last, fp);
		} while (xpos < ATARI_VISIBLE_WIDTH);

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
			ypos++;
		}
	}

	if (ptr2 == NULL) {
		/*write palette*/
		fputc(0xc, fp);
		for (i=0; i<256; i++) {
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
#ifdef HAVE_STRCASECMP
	return strcasecmp(s1 + pos, s2) == 0;
#else
	return stricmp(s1 + pos, s2) == 0;
#endif
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
		ptr3 = (png_bytep) malloc(3 * ATARI_VISIBLE_WIDTH * ATARI_HEIGHT);
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
		atari_screen = (ULONG *) malloc(ATARI_WIDTH * ATARI_HEIGHT);
		ptr2 = (UBYTE *) atari_screen + ATARI_LEFT_MARGIN;
		ANTIC_Frame(TRUE); /* draw on atari_screen */
	}
	else {
		ptr2 = NULL;
	}
	if (is_png)
		Screen_SavePNG(fp, ptr1, ptr2);
	else
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

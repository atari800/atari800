/*
 * image_pcx.c - support for PCX images
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
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
#include "screen.h"
#include "colours.h"
#include "file_export.h"
#include "codecs/image.h"


/* PCX_SaveScreen saves the screen data to the file in PCX format, optionally
   using interlace if ptr2 is not NULL.

   PCX format is a lossless image file format derived from PC Paintbrush, a
   DOS-era paint program, and is widely supported by image viewers. The
   compression method is run-length encoding, which is simple to implement but
   only compresses well when groups of neighboring pixels on a scan line have
   the same color.

   The PNG format (see PNG_SaveScreen below) compresses much better, but depends
   on the external libpng library. No external dependencies are needed for PCX
   format.

   fp:          file pointer of file open for writing
   ptr1:        pointer to Screen_atari
   ptr2:        (optional) pointer to another array of size Screen_atari containing
                the interlaced scan lines to blend with ptr1. Set to NULL if no
				interlacing.
*/
static int PCX_SaveScreen(FILE *fp, UBYTE *ptr1, UBYTE *ptr2)
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
	fputw(image_codec_width - 1, fp); /* XMax */
	fputw(image_codec_height - 1, fp);        /* YMax */
	fputw(0, fp);     /* HRes */
	fputw(0, fp);     /* VRes */
	for (i = 0; i < 48; i++)
		fputc(0, fp); /* EGA color palette */
	fputc(0, fp);     /* reserved */
	fputc(ptr2 != NULL ? 3 : 1, fp); /* number of bit planes */
	fputw(image_codec_width, fp);  /* number of bytes per scan line per color plane */
	fputw(1, fp);     /* palette info */
	fputw(image_codec_width, fp); /* screen resolution */
	fputw(image_codec_height, fp);
	for (i = 0; i < 54; i++)
		fputc(0, fp);  /* unused */

	ptr1 += (Screen_WIDTH * image_codec_top_margin) + image_codec_left_margin;
	if (ptr2 != NULL) {
		ptr2 += (Screen_WIDTH * image_codec_top_margin) + image_codec_left_margin;
	}
	for (y = 0; y < image_codec_height; ) {
		x = 0;
		do {
			last = ptr2 != NULL ? (((Colours_table[*ptr1] >> plane) & 0xff) + ((Colours_table[*ptr2] >> plane) & 0xff)) >> 1 : *ptr1;
			count = 0xc0;
			do {
				ptr1++;
				if (ptr2 != NULL)
					ptr2++;
				count++;
				x++;
			} while (last == (ptr2 != NULL ? (((Colours_table[*ptr1] >> plane) & 0xff) + ((Colours_table[*ptr2] >> plane) & 0xff)) >> 1 : *ptr1)
						&& count < 0xff && x < image_codec_width);
			if (count > 0xc1 || last >= 0xc0)
				fputc(count, fp);
			fputc(last, fp);
		} while (x < image_codec_width);

		if (ptr2 != NULL && plane) {
			ptr1 -= image_codec_width;
			ptr2 -= image_codec_width;
			plane -= 8;
		}
		else {
			ptr1 += Screen_WIDTH - image_codec_width;
			if (ptr2 != NULL) {
				ptr2 += Screen_WIDTH - image_codec_width;
				plane = 16;
			}
			y++;
		}
	}

	if (ptr2 == NULL) {
		/* write palette */
		fputc(0xc, fp);
		for (i = 0; i < 256; i++) {
			fputc(Colours_GetR(i), fp);
			fputc(Colours_GetG(i), fp);
			fputc(Colours_GetB(i), fp);
		}
	}

	return 1;
}

IMAGE_CODEC_t Image_Codec_PCX = {
	"pcx",
	"PC Paintbrush",
	&PCX_SaveScreen,
	NULL,
};

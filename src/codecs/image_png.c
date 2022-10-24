/*
 * image_png.c - support for PNG images
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


/* This file is only compiled when HAVE_LIBPNG is defined. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "screen.h"
#include "colours.h"
#include "util.h"
#include "log.h"
#include "file_export.h"
#include "codecs/image.h"
#include "codecs/image_png.h"

#include <png.h>

#ifdef VIDEO_CODEC_PNG
static int current_png_size = -1;
static int max_buffer_size = 0;
static UBYTE *image_buffer = NULL;

static void png_write_fn_callback(png_structp png_ptr, png_bytep data, png_size_t length)
{
	if (current_png_size >= 0) {
		if (current_png_size + length < max_buffer_size) {
			memcpy(image_buffer + current_png_size, data, length);
			current_png_size += length;
		}
		else {
			Log_print("PNG write error: buffer size too small.");
			current_png_size = -1;
		}
	}
}
#endif /* VIDEO_CODEC_PNG */

/* PNG_SaveScreen saves the screen data to the file in PNG format, optionally
   using interlace if ptr2 is not NULL.

   PNG format is a lossless image file format that compresses much better than
   PCX. Because it depends on the external libpng library, it is only compiled
   in atari800 if requested and libpng is found on the system.

   fp:          file pointer of file open for writing
   ptr1:        pointer to Screen_atari
   ptr2:        (optional) pointer to another array of size Screen_atari containing
                the interlaced scan lines to blend with ptr1. Set to NULL if no
				interlacing.
*/
static int PNG_SaveScreen(FILE *fp, UBYTE *ptr1, UBYTE *ptr2)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep rows[Screen_HEIGHT];

	png_ptr = png_create_write_struct(
		PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL
	);
	if (png_ptr == NULL)
		return 0;
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, NULL);
		return 0;
	}
#ifdef VIDEO_CODEC_PNG
	if (fp == NULL) {
		png_set_write_fn(png_ptr, NULL, png_write_fn_callback, NULL);
	}
	else
#endif
	{
		png_init_io(png_ptr, fp);
	}

	png_set_compression_level(png_ptr, FILE_EXPORT_compression_level);
	png_set_IHDR(
		png_ptr, info_ptr, image_codec_width, image_codec_height,
		8, ptr2 == NULL ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);
	if (ptr2 == NULL) {
		int i;
		png_color palette[256];
		for (i = 0; i < 256; i++) {
			palette[i].red = Colours_GetR(i);
			palette[i].green = Colours_GetG(i);
			palette[i].blue = Colours_GetB(i);
		}
		png_set_PLTE(png_ptr, info_ptr, palette, 256);
		ptr1 += (Screen_WIDTH * image_codec_top_margin) + image_codec_left_margin;
		for (i = 0; i < image_codec_height; i++) {
			rows[i] = ptr1;
			ptr1 += Screen_WIDTH;
		}
	}
	else {
		png_bytep ptr3;
		int x;
		int y;
		ptr1 += (Screen_WIDTH * image_codec_top_margin) + image_codec_left_margin;
		ptr2 += (Screen_WIDTH * image_codec_top_margin) + image_codec_left_margin;
		ptr3 = (png_bytep) Util_malloc(3 * image_codec_width * image_codec_height);
		for (y = 0; y < image_codec_height; y++) {
			rows[y] = ptr3;
			for (x = 0; x < image_codec_width; x++) {
				*ptr3++ = (png_byte) ((Colours_GetR(*ptr1) + Colours_GetR(*ptr2)) >> 1);
				*ptr3++ = (png_byte) ((Colours_GetG(*ptr1) + Colours_GetG(*ptr2)) >> 1);
				*ptr3++ = (png_byte) ((Colours_GetB(*ptr1) + Colours_GetB(*ptr2)) >> 1);
				ptr1++;
				ptr2++;
			}
			ptr1 += Screen_WIDTH - image_codec_width;
			ptr2 += Screen_WIDTH - image_codec_width;
		}
	}
	png_set_rows(png_ptr, info_ptr, rows);
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	if (ptr2 != NULL)
		free(rows[0]);

#ifdef VIDEO_CODEC_PNG
	return current_png_size;
#else
	return -1;
#endif
}

#ifdef VIDEO_CODEC_PNG
/* Instead of saving PNG to a file, this function allows saving the screen to a buffer */
static int PNG_SaveToBuffer(UBYTE *buf, int bufsize, UBYTE *ptr1, UBYTE *ptr2)
{
	int result;

	image_buffer = buf;
	max_buffer_size = bufsize;
	current_png_size = 0;

	result = PNG_SaveScreen(NULL, ptr1, ptr2);

	image_buffer = NULL;
	max_buffer_size = 0;
	current_png_size = -1;

	return result;
}
#endif

IMAGE_CODEC_t Image_Codec_PNG = {
	"png",
	"Portable Network Graphics",
	&PNG_SaveScreen,
#ifdef VIDEO_CODEC_PNG
	&PNG_SaveToBuffer,
#else
	NULL,
#endif
};

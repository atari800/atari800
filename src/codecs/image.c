/*
 * image.c - interface for image codecs
 *
 * Copyright (C) 2021 Rob McMullen
 * Copyright (C) 1998-2021 Atari800 development team (see DOC/CREDITS)
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
#include <stdlib.h>
#include "screen.h"
#include "cfg.h"
#include "util.h"
#include "log.h"
#ifdef SUPPORTS_CHANGE_VIDEOMODE
#include "videomode.h"
#endif
#include "codecs/image.h"
#include "codecs/image_pcx.h"
#ifdef HAVE_LIBPNG
#include "codecs/image_png.h"
#endif

IMAGE_CODEC_t *image_codec = NULL;

/* image size will be determined in a call to CODECS_IMAGE_SetMargins() below */
int image_codec_left_margin;
int image_codec_top_margin;
int image_codec_width;
int image_codec_height;

static IMAGE_CODEC_t *known_image_codecs[] = {
	&Image_Codec_PCX,
#ifdef HAVE_LIBPNG
	&Image_Codec_PNG,
#endif
	NULL,
};


static IMAGE_CODEC_t *match_image_codec(const char *id)
{
	IMAGE_CODEC_t **v = known_image_codecs;
	IMAGE_CODEC_t *found = NULL;

	while (*v) {
		if (Util_striendswith(id, (*v)->codec_id)) {
			found = *v;
			break;
		}
		v++;
	}
	return found;
}


void CODECS_IMAGE_SetMargins(void)
{
#ifdef SUPPORTS_CHANGE_VIDEOMODE
	image_codec_left_margin = VIDEOMODE_src_offset_left;
	image_codec_width = VIDEOMODE_src_offset_left + VIDEOMODE_src_width - image_codec_left_margin;
#else
	image_codec_left_margin = Screen_visible_x1;
	image_codec_width = Screen_visible_x2 - image_codec_left_margin;
#endif
	image_codec_top_margin = Screen_visible_y1;
	image_codec_height = Screen_visible_y2 - image_codec_top_margin;
}

/* Sets the global image_codec if the filename has an extension that matches a known
   image codec. Also sets the margins so the codec is ready to save images */
int CODECS_IMAGE_Init(const char *filename)
{
	image_codec = match_image_codec(filename);
	CODECS_IMAGE_SetMargins();
	return (image_codec != NULL);
}

/*
 * video_mpng.c - Video codec for Motion-PNG
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


/* This file is only compiled when HAVE_LIBPNG and VIDEO_CODEC_PNG are defined. */

#include <string.h>
#include "codecs/video.h"
#include "codecs/image_png.h"
#include "codecs/video_mpng.h"

static int MPNG_Init(int width, int height, int left_margin, int top_margin)
{
	int comp_size = width * height;

	/* In the worst case, PNG can store uncompressed image. Due to the overhead
	   in the format the resulting data will be larger than the source data.
	   Because PNG uses the deflate algorithm, the same calculation is used here
	   as in ZMBV. */

	/* Conservative upper bound taken from zlib v1.2.1 source via lcl.c */
	comp_size = comp_size + ((comp_size + 7) >> 3) + ((comp_size + 63) >> 6) + 11;
	return comp_size;
}

static int MPNG_CreateFrame(UBYTE *source, int keyframe, UBYTE *buf, int bufsize)
{
	return Image_Codec_PNG.to_buffer(buf, bufsize, source, NULL);
}

static int MPNG_End(void)
{
	return 1;
}

VIDEO_CODEC_t Video_Codec_MPNG = {
	"png",
	"Motion-PNG",
	{'M', 'P', 'N', 'G'},
	{'M', 'P', 'N', 'G'},
	FALSE,
	&MPNG_Init,
	&MPNG_CreateFrame,
	&MPNG_End,
};

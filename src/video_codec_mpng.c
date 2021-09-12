/*
 * video_codec_mpng.c - Video codec for Motion-PNG
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

#include <string.h>
#include "file_export.h"
#include "video_codec_mpng.h"

#ifdef HAVE_LIBPNG

static int MPNG_Init(int width, int height, int left_margin, int top_margin)
{
	return width * height;
}

static int MPNG_CreateFrame(UBYTE *source, int keyframe, UBYTE *buf, int bufsize)
{
	return PNG_SaveScreen(NULL, source, NULL);
}

static int MPNG_End(void)
{
	return 1;
}

VIDEO_CODEC_t Video_Codec_MPNG = {
	VIDEO_CODEC_PNG,
	"PNG",
	"Motion-PNG",
	{'M', 'P', 'N', 'G'},
	{'M', 'P', 'N', 'G'},
	FALSE,
	&MPNG_Init,
	&MPNG_CreateFrame,
	&MPNG_End,
};

#endif /* HAVE_LIBPNG */

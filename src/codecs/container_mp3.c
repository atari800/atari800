/*
 * container_mp3.c - support for MP3 audio files
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


/* This file is only compiled when AUDIO_RECORDING and AUDIO_CODEC_MP3 are defined. */

#include <stdio.h>
#include <stdlib.h>
#include "file_export.h"
#include "util.h"
#include "log.h"
#include "codecs/container.h"
#include "codecs/container_mp3.h"
#include "codecs/audio.h"


/* MP3_Prepare just returns because the only thing in a constant bitrate MP3
   file is a concatenation of MP3 frames.
   */
static int MP3_Prepare(FILE *fp)
{
	if (strcmp(audio_codec->codec_id, "mp3") != 0) {
		File_Export_SetErrorMessageArg("Can't store %s in mp3 file", audio_codec->codec_id);
		return 0;
	}
	return 1;
}

/* MP3_AudioFrame just adds another MP3 frame to the file. */
static int MP3_AudioFrame(FILE *fp, const UBYTE *buf, int bufsize)
{
	int size;

	size = fwrite(buf, 1, bufsize, fp);
	if (size < bufsize) {
		File_Export_SetErrorMessage("Failed writing to MP3 file");
		size = 0;
	}

	return (size > 0);
}


/* MP3 doesn't have a size limit. */
static int MP3_SizeCheck(int size)
{
	return 1;
}

/* MP3_Finalize only prints out statistics because nothing is required
   to make a valid CBR MP3 file. */
static int MP3_Finalize(FILE *fp)
{
	return 1;
}

CONTAINER_t Container_MP3 = {
	"mp3",
	"MP3 format audio",
	&MP3_Prepare,
	&MP3_AudioFrame,
	NULL,
	&MP3_SizeCheck,
	&MP3_Finalize,
};

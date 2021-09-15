/*
 * audio_codec_pcm.c - Audio codec for raw PCM samples
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
#include "audio_codec_pcm.h"

static AUDIO_OUT_t audio_out;

static int PCM_Init(int sample_rate, int fps, int sample_size, int num_channels)
{
	int comp_size;

	audio_out.sample_rate = sample_rate;
	audio_out.sample_size = sample_size;
	audio_out.num_channels = num_channels;

	comp_size = sample_rate * sample_size * num_channels / fps;
	return comp_size;
}

static AUDIO_OUT_t *PCM_AudioOut(void) {
	return &audio_out;
}

static int PCM_CreateFrame(const UBYTE *source, int num_samples, UBYTE *buf, int bufsize)
{
	int size;

	size = num_samples * audio_out.sample_size;
	if (size > bufsize) {
		return -1;
	}
#ifdef WORDS_BIGENDIAN
	if (audio_out.sample_size == 2) {
		UBYTE c;

		while (num_samples > 0) {
			c = *source++;
			*buf++ = *source++;
			*buf++ = c;
			num_samples--;
		}
	}
	else
#endif
	{
		memcpy(buf, source, size);
	}
	return size;
}

static int PCM_End(void)
{
	return 1;
}

AUDIO_CODEC_t Audio_Codec_PCM = {
	"pcm",
	"PCM Samples",
	{1, 0, 0, 0}, /* fourcc */
	1, /* format type */
	&PCM_Init,
	&PCM_AudioOut,
	&PCM_CreateFrame,
	&PCM_End,
};

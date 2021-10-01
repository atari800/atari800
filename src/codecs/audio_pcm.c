/*
 * audio_pcm.c - Audio codec for raw PCM samples
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
#include "codecs/audio.h"
#include "codecs/audio_pcm.h"

static AUDIO_OUT_t out;

static int PCM_Init(int sample_rate, float fps, int sample_size, int num_channels)
{
	int comp_size;

	out.sample_rate = sample_rate;
	out.sample_size = sample_size;
	out.bits_per_sample = sample_size * 8;
	out.bitrate = sample_rate * num_channels * out.bits_per_sample;
	out.num_channels = num_channels;
	out.block_align = num_channels * sample_size;
	out.scale = 1;
	out.rate = sample_rate;
	out.length = 0;
	out.extra_data_size = 0;

	comp_size = sample_rate * sample_size * num_channels / (int)fps;
	return comp_size;
}

static AUDIO_OUT_t *PCM_AudioOut(void) {
	return &out;
}

static int PCM_CreateFrame(const UBYTE *source, int num_samples, UBYTE *buf, int bufsize)
{
	int size;

	size = num_samples * out.sample_size;
	if (size > bufsize) {
		return -1;
	}
#ifdef WORDS_BIGENDIAN
	if (out.sample_size == 2) {
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
	out.length += num_samples;
	return size;
}

static int PCM_AnotherFrame(void)
{
	/* This codec doesn't buffer frames */
	return 0;
}

static int PCM_Flush(float duration)
{
	/* This codec will never have any extra frames */
	return 0;
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
	AUDIO_CODEC_FLAG_SUPPORTS_8_BIT_SAMPLES,
	&PCM_Init,
	&PCM_AudioOut,
	&PCM_CreateFrame,
	&PCM_AnotherFrame,
	&PCM_Flush,
	&PCM_End,
};

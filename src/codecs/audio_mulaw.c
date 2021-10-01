/*
 * audio_mulaw.c - Audio codec for mu-law telephony encoding
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

/* NOTE: mu-law is simple codec that taxes 16-bit PCM samples as input and
   outputs an encoding of 8 bits per sample. Its original design goal was to
   minimize losses over low-bandwidth connections, like telephone lines. It's
   not necessarily a great codec for recording Atari audio, although it sounds
   much better than I expected. It is included here as more of an example for
   further codec implementers, as it populates the out array with different
   parameters than the input: it requires 16-bit samples, and outputs 8 bits of
   data for each sample. The number of channels and sample rate remain the same.
   */

static AUDIO_OUT_t out;

/* From https://en.wikipedia.org/wiki/%CE%9C-law_algorithm the mulaw encoding is
   based on converting samples to 14 bits and then categorizing them in the
   segments shown below. The encoding is then computed by logical OR of the sign,
   the computed prefix, and the interval (the xxxx bits) within the segment.

     positive        negative
   segment range   segment range    bit pattern    prefix   encoding
   8191 to 4096   -8192 to -4097   1xxxxyyyyyyyy   111      s111xxxx
   4095 to 2048   -4096 to -2049   01xxxxxyyyyyy   110      s110xxxx
   2047 to 1024   -2048 to -1025   001xxxxxyyyyy   101      s101xxxx
   1023 to 512    -1024 to -513    0001xxxxxyyyy   100      s100xxxx
   511 to 256     -512 to -257     00001xxxxxyyy   011      s011xxxx
   255 to 128     -256 to -129     000001xxxxxyy   010      s010xxxx
   127 to 64      -128 to -65      0000001xxxxxy   001      s001xxxx
   63 to 33       -64 to -35       00000001xxxxx   000      s000xxxx
   33 to 0        -34 to -1        000000001xxxx   111      s1111111

   A table is used to generate the prefix. The table was created using the
   following python code:

       import math
	   intervals = [0] + [math.floor(math.log2(i)) for i in range(1,256)]
       print(",\n".join([str(intervals[i:i+16])[1:-1] for i in range(0,256,16)]))

*/

static UBYTE interval_lookup[256] = {
	0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

static int MULAW_Init(int sample_rate, float fps, int sample_size, int num_channels)
{
	int comp_size;

	out.sample_rate = sample_rate;
	out.sample_size = 1;
	out.bits_per_sample = 8;
	out.bitrate = sample_rate * num_channels * out.bits_per_sample;
	out.num_channels = num_channels;
	out.block_align = num_channels;
	out.scale = 1;
	out.rate = sample_rate;
	out.length = 0;
	out.extra_data_size = 0;

	comp_size = sample_rate * sample_size * num_channels / (int)fps;
	return comp_size;
}

static AUDIO_OUT_t *MULAW_AudioOut(void) {
	return &out;
}

static int MULAW_CreateFrame(const UBYTE *source, int num_samples, UBYTE *buf, int bufsize)
{
	int size;
	const SWORD *words;
	SWORD sample;
	int segment_number;
    int interval;
    int prefix;
    int encoding;
	int sign;

	size = num_samples;
	if (size > bufsize) {
		return -1;
	}
	out.length += num_samples;
	words = (const SWORD *)source;
	while (num_samples > 0) {
		sample = *words++;

		if (sample == -32768) {
			/* handle the oddball case where signed 16 bit can hold one more
			   negative number than positive number */
			encoding = (UBYTE) 0x7f;
		}
		else {
			/* need sign bit */
			sign = (sample < 0);

			/* make sample positive, and reduce to 14 bits */
			if (sign) sample = -sample;
			sample = sample >> 2;

			/* clip and add bias to get into range 33 - 8191 */
			if (sample > 8158) sample = 8158;
			sample += 33;

			/* do the lookup and construct the code */
			segment_number = sample >> 5;
			prefix = interval_lookup[segment_number];
			interval = (sample >> (prefix + 1)) & 0x0f;
			encoding = ~ (sign * 0x80 | (prefix << 4) | interval);
		}

		*buf++ = (UBYTE)encoding;
		num_samples--;
	}

	return size;
}

static int MULAW_AnotherFrame(void)
{
	/* This codec doesn't buffer frames */
	return 0;
}

static int MULAW_Flush(float duration)
{
	/* This codec will never have any extra frames */
	return 0;
}

static int MULAW_End(void)
{
	return 1;
}

AUDIO_CODEC_t Audio_Codec_MULAW = {
	"mulaw",
	"mu-law 8-bit Telephony Codec",
	{1, 0, 0, 0}, /* fourcc */
	7, /* mu-law */
	0, /* flags */
	&MULAW_Init,
	&MULAW_AudioOut,
	&MULAW_CreateFrame,
	&MULAW_AnotherFrame,
	&MULAW_Flush,
	&MULAW_End,
};

AUDIO_CODEC_t Audio_Codec_PCM_MULAW = {
	"pcm_mulaw",
	"mu-law 8-bit Telephony Codec",
	{1, 0, 0, 0}, /* fourcc */
	7, /* mu-law */
	0, /* flags */
	&MULAW_Init,
	&MULAW_AudioOut,
	&MULAW_CreateFrame,
	&MULAW_AnotherFrame,
	&MULAW_Flush,
	&MULAW_End,
};

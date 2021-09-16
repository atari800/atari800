/*
 * audio_codec_pcm.c - Audio codec for Microsoft ADPCM adaptive PCM coding
 *
 * This is a derivative work of code from the FFmpeg project. The FFmpeg code
 * used here is licensed under the LGPLv2.1; Provisions within that license
 * permit derivative works to be licensed under the GPLv2.
 *
 * Modifications Copyright (C) 2021 Rob McMullen
 * Original code Copyright (c) 2001-2003 The FFmpeg project
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
#include <stdlib.h>
#include "file_export.h"
#include "util.h"
#include "log.h"
#include "audio_codec_adpcm.h"

static AUDIO_OUT_t audio_out;

static ADPCMChannelStatus channel_status[2];

static int debug_samples_in;
static int debug_samples_consumed;
static int samples_per_block;
static SWORD *leftover_samples;
static SWORD *leftover_samples_boundary;
static SWORD *leftover_samples_end;

/* adpcm_AdaptationTable[], adpcm_AdaptCoeff1[], and
   adpcm_AdaptCoeff2[] are from libsndfile */
const SWORD adpcm_AdaptationTable[] = {
	230, 230, 230, 230, 307, 409, 512, 614,
	768, 614, 512, 409, 307, 230, 230, 230
};

/* Divided by 4 to fit in 8-bit integers */
const UBYTE adpcm_AdaptCoeff1[] = {
	64, 128, 0, 48, 60, 115, 98
};

/* Divided by 4 to fit in 8-bit integers */
const SBYTE adpcm_AdaptCoeff2[] = {
	0, -64, 0, 16, 0, -52, -58
};

/* Clip a signed integer value into the -32768,32767 range.
 * @param a value to clip
 * @return clipped value
 */
static inline const SWORD clip_int16(int a)
{
	if ((a+0x8000U) & ~0xFFFF) return (a>>31) ^ 0x7FFF;
	else                      return a;
}

/* Clip a signed integer into the -(2^p),(2^p-1) range.
 * @param  a value to clip
 * @param  p bit position to clip at
 * @return clipped value
 */
static inline const int clip_intp2(int a, int p)
{
    if (((unsigned)a + (1 << p)) & ~((2 << p) - 1))
        return (a >> 31) ^ ((1 << p) - 1);
    else
        return a;
}

static inline UBYTE adpcm_ms_compress_sample(ADPCMChannelStatus *c, SWORD sample)
{
    int predictor, nibble, bias;

    predictor = (((c->sample1) * (c->coeff1)) +
                (( c->sample2) * (c->coeff2))) / 64;

    nibble = sample - predictor;
    if (nibble >= 0)
        bias =  c->idelta / 2;
    else
        bias = -c->idelta / 2;

    nibble = (nibble + bias) / c->idelta;
    nibble = clip_intp2(nibble, 3) & 0x0F;

    predictor += ((nibble & 0x08) ? (nibble - 0x10) : nibble) * c->idelta;

    c->sample2 = c->sample1;
    c->sample1 = clip_int16(predictor);

    c->idelta = (adpcm_AdaptationTable[nibble] * c->idelta) >> 8;
    if (c->idelta < 16)
        c->idelta = 16;

    return nibble;
}

#define PUT_LE_WORD(p, val) do {         \
        UWORD d = (val);             \
        ((UBYTE*)(p))[0] = (d);      \
        ((UBYTE*)(p))[1] = (d)>>8;   \
		p += 2;                      \
    } while(0)

static int ADPCM_Init(int sample_rate, float fps, int sample_size, int num_channels)
{
	int comp_size;
	int i;
	UBYTE *extra;

	if (sample_size < 2)
		return -1;
	audio_out.sample_rate = sample_rate;
	audio_out.sample_size = 1;
	audio_out.bits_per_sample = 4;
	audio_out.num_channels = num_channels;
	audio_out.block_align = 1024;
	audio_out.scale = 1000000;
	audio_out.rate = (int)(fps * 1000000);
	audio_out.length = 0;
	audio_out.extra_data_size = 32;
	extra = audio_out.extra_data;
	samples_per_block = (audio_out.block_align - 7 * num_channels) * 2 / num_channels + 2;
	PUT_LE_WORD(extra, samples_per_block);
	PUT_LE_WORD(extra, 7);
	for (i = 0; i < 7; i++) {
		PUT_LE_WORD(extra, adpcm_AdaptCoeff1[i] * 4);
		PUT_LE_WORD(extra, adpcm_AdaptCoeff2[i] * 4);
	}

	/* only channel status values that must be initialized; others are set each frame */
	channel_status[0].idelta = 0;
	channel_status[1].idelta = 0;

	comp_size = sample_rate * sample_size * num_channels / (int)(fps);

	leftover_samples = (SWORD *)Util_malloc(comp_size * 4);
	leftover_samples_boundary = leftover_samples + comp_size * 2;
	leftover_samples_end = leftover_samples;

	debug_samples_in = 0;
	debug_samples_consumed = 0;

	return comp_size;
}

static AUDIO_OUT_t *ADPCM_AudioOut(void) {
	return &audio_out;
}

static int ADPCM_CreateFrame(const UBYTE *source, int num_samples, UBYTE *buf, int bufsize)
{
	UBYTE *buf_start;
	int i;
	int st;
	int samples_consumed;
	const SWORD *samples;
	const SWORD *samples_start;

	if (audio_out.block_align > bufsize) {
		return -1;
	}

	buf_start = buf;
	samples = (const SWORD *)leftover_samples;

	if (!source) {
		/* we have reached the end of the file, so we need to flush the last frame.
		   by filling the buffer with enough zeros to guarantee a full frame. */
		num_samples = samples_per_block - (int)(leftover_samples_end - samples);
		memset(leftover_samples_end, 0, num_samples * 2);
	}
	else if (num_samples > 0) {
		/* add new samples to end of any remaining samples */
		memcpy(leftover_samples_end, source, num_samples * 2);
	}
	leftover_samples_end += num_samples;
	if (leftover_samples_end > leftover_samples_boundary) {
		Log_print("audio_codec_adpcm: leftover sample buffer too small!\n");
		return -1;
	}

	num_samples = (int)(leftover_samples_end - samples);
	if (num_samples < samples_per_block) {
		/* not enough samples to fill a block, so we have to wait until a
		   subsequent call to this function fills the buffer full enough */
		return 0;
	}

    samples_start = samples;
	st = audio_out.num_channels == 2;

	for (i = 0; i < audio_out.num_channels; i++) {
		int predictor = 0;
		*buf++ = predictor;
		channel_status[i].coeff1 = adpcm_AdaptCoeff1[predictor];
		channel_status[i].coeff2 = adpcm_AdaptCoeff2[predictor];
	}
	for (i = 0; i < audio_out.num_channels; i++) {
		if (channel_status[i].idelta < 16)
			channel_status[i].idelta = 16;
		PUT_LE_WORD(buf, channel_status[i].idelta);
	}
	for (i = 0; i < audio_out.num_channels; i++)
		channel_status[i].sample2 = *samples++;
	for (i = 0; i < audio_out.num_channels; i++) {
		channel_status[i].sample1 = *samples++;
		PUT_LE_WORD(buf, channel_status[i].sample1);
	}
	for (i = 0; i < audio_out.num_channels; i++)
		PUT_LE_WORD(buf, channel_status[i].sample2);

	for (i = 7 * audio_out.num_channels; i < audio_out.block_align; i++) {
		int nibble;
		nibble  = adpcm_ms_compress_sample(&channel_status[ 0], *samples++) << 4;
		nibble |= adpcm_ms_compress_sample(&channel_status[st], *samples++);
		*buf++  = nibble;
	}

	samples_consumed = (int)(samples - leftover_samples);

	/* move leftover samples down and leave them for the next time through the loop */
	memmove(leftover_samples, samples, (num_samples - samples_consumed) * 2);
	leftover_samples_end = leftover_samples + (num_samples - samples_consumed);

	audio_out.length++;

	return buf - buf_start;
}

static int ADPCM_AnotherFrame(int final_frame)
{
	if (final_frame) {
		return (int)(leftover_samples_end - leftover_samples) > 0;
	}
	return (int)(leftover_samples_end - leftover_samples) >= samples_per_block;
}

static int ADPCM_End(float duration)
{
	float rate;

	/* Because this codec adds audio frames to the AVI at a different rate than
	   video frames get added, we need to compute rate of audio frames using
	   number of video frames to calculate the total duration of the AVI. */
	if (duration > 0) {
		rate = (float)audio_out.length / duration;
		audio_out.rate = (int)(rate * 1000000);
	}
	free(leftover_samples);
	return 1;
}

AUDIO_CODEC_t Audio_Codec_ADPCM = {
	"adpcm",
	"Microsoft ADPCM",
	{1, 0, 0, 0}, /* fourcc */
	2, /* format type */
	&ADPCM_Init,
	&ADPCM_AudioOut,
	&ADPCM_CreateFrame,
	&ADPCM_AnotherFrame,
	&ADPCM_End,
};

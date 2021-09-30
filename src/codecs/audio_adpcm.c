/*
 * audio_adpcm.c - Audio codec for Microsoft ADPCM adaptive PCM coding
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
#include "codecs/audio.h"
#include "util.h"
#include "log.h"
#include "codecs/audio_adpcm.h"

#define FFMIN(a,b) ((a) > (b) ? (b) : (a))

#define FORMAT_MS 0x02
#define FORMAT_IMA 0x11
#define FORMAT_YAMAHA 0x20

static AUDIO_OUT_t out;

static ADPCMChannelStatus channel_status[2];

static int samples_per_block;
static int format_type;
static float final_duration;
static SWORD *leftover_samples;
static SWORD *leftover_samples_boundary;
static SWORD *leftover_samples_end;

/* adpcm_step_table[] and adpcm_index_table[] are from the ADPCM reference source */
static const SBYTE adpcm_index_table[16] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8,
};

static const SWORD adpcm_step_table[89] = {
		7,     8,     9,    10,    11,    12,    13,    14,    16,    17,
	   19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
	   50,    55,    60,    66,    73,    80,    88,    97,   107,   118,
	  130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
	  337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
	  876,   963,  1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
	 2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
	 5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

/* adpcm_AdaptationTable[], adpcm_AdaptCoeff1[], and
   adpcm_AdaptCoeff2[] are from libsndfile */
static const SWORD adpcm_AdaptationTable[] = {
	230, 230, 230, 230, 307, 409, 512, 614,
	768, 614, 512, 409, 307, 230, 230, 230
};

/* Divided by 4 to fit in 8-bit integers */
static const UBYTE adpcm_AdaptCoeff1[] = {
	64, 128, 0, 48, 60, 115, 98
};

/* Divided by 4 to fit in 8-bit integers */
static const SBYTE adpcm_AdaptCoeff2[] = {
	0, -64, 0, 16, 0, -52, -58
};

static const SWORD adpcm_yamaha_indexscale[] = {
	230, 230, 230, 230, 307, 409, 512, 614,
	230, 230, 230, 230, 307, 409, 512, 614
};

static const SBYTE adpcm_yamaha_difflookup[] = {
	 1,  3,  5,  7,  9,  11,  13,  15,
	-1, -3, -5, -7, -9, -11, -13, -15
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

/**
 * Clip a signed integer value into the amin-amax range.
 * @param a value to clip
 * @param amin minimum value of the clip range
 * @param amax maximum value of the clip range
 * @return clipped value
 */
static inline const int clip(int a, int amin, int amax)
{
	if      (a < amin) return amin;
	else if (a > amax) return amax;
	else               return a;
}

static inline UBYTE adpcm_ima_compress_sample(ADPCMChannelStatus *c, SWORD sample)
{
	int delta  = sample - c->sample1;
	int nibble = FFMIN(7, abs(delta) * 4 /
					   adpcm_step_table[c->step]) + (delta < 0) * 8;
	c->sample1 += ((adpcm_step_table[c->step] *
						adpcm_yamaha_difflookup[nibble]) / 8);
	c->sample1 = clip_int16(c->sample1);
	c->step  = clip(c->step + adpcm_index_table[nibble], 0, 88);
	return nibble;
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

static inline UBYTE adpcm_yamaha_compress_sample(ADPCMChannelStatus *c, SWORD sample)
{
	int nibble, delta;

	if (!c->step) {
		c->predictor = 0;
		c->step      = 127;
	}

	delta = sample - c->predictor;

	nibble = FFMIN(7, abs(delta) * 4 / c->step) + (delta < 0) * 8;

	c->predictor += ((c->step * adpcm_yamaha_difflookup[nibble]) / 8);
	c->predictor = clip_int16(c->predictor);
	c->step = (c->step * adpcm_yamaha_indexscale[nibble]) >> 8;
	c->step = clip(c->step, 127, 24576);

	return nibble;
}

static int reserve_buffers(void)
{
	int num_samples;

	num_samples = samples_per_block * out.num_channels * 2;

	leftover_samples = (SWORD *)Util_malloc(num_samples * 2);
	leftover_samples_boundary = leftover_samples + num_samples;
	leftover_samples_end = leftover_samples;

	return out.block_align;
}

static void init_common(int sample_rate, float fps, int sample_size, int num_channels)
{
	out.sample_rate = sample_rate;
	out.sample_size = 1;
	out.bits_per_sample = 4;
	out.bitrate = sample_rate * num_channels * out.bits_per_sample;
	out.num_channels = num_channels;
	out.block_align = 1024;
	out.scale = 1000000;
	out.rate = (int)(fps * 1000000);
	out.length = 0;
	out.extra_data_size = 0;

	final_duration = 0.0;
}

static int ADPCM_Init_IMA(int sample_rate, float fps, int sample_size, int num_channels)
{
	init_common(sample_rate, fps, sample_size, num_channels);

	format_type = FORMAT_IMA;
	samples_per_block = (out.block_align - 4 * num_channels) * 8 / (4 * num_channels) + 1;

	/* only step index must be initialized; others are set each frame */
	channel_status[0].step = 0;
	channel_status[1].step = 0;

	return reserve_buffers();
}

static int ADPCM_Init_MS(int sample_rate, float fps, int sample_size, int num_channels)
{
	int i;
	UBYTE *extra;

	init_common(sample_rate, fps, sample_size, num_channels);

	out.extra_data_size = 32;
	extra = out.extra_data;
	format_type = FORMAT_MS;
	samples_per_block = (out.block_align - 7 * num_channels) * 2 / num_channels + 2;
	PUT_LE_WORD(extra, samples_per_block);
	PUT_LE_WORD(extra, 7);
	for (i = 0; i < 7; i++) {
		PUT_LE_WORD(extra, adpcm_AdaptCoeff1[i] * 4);
		PUT_LE_WORD(extra, adpcm_AdaptCoeff2[i] * 4);
	}

	/* only idelta values must be initialized; others are set each frame */
	channel_status[0].idelta = 0;
	channel_status[1].idelta = 0;

	return reserve_buffers();
}

static int ADPCM_Init_Yamaha(int sample_rate, float fps, int sample_size, int num_channels)
{
	init_common(sample_rate, fps, sample_size, num_channels);

	format_type = FORMAT_YAMAHA;
	samples_per_block = (out.block_align * 2) / num_channels;

	/* only predictor and step values are used in Yamaha */
	channel_status[0].predictor = 0;
	channel_status[0].step = 0;
	channel_status[1].predictor = 0;
	channel_status[1].step = 0;

	return reserve_buffers();
}

static AUDIO_OUT_t *ADPCM_AudioOut(void) {
	return &out;
}

static int ADPCM_CreateFrame(const UBYTE *source, int num_samples, UBYTE *buf, int bufsize)
{
	UBYTE *buf_start;
	int i;
	int j;
	int ch;
	int st;
	int samples_consumed;
	const SWORD *samples;
	ADPCMChannelStatus *status;

	buf_start = buf;
	samples = (const SWORD *)leftover_samples;

	if (leftover_samples_end + num_samples > leftover_samples_boundary) {
		Log_print("audio_adpcm: leftover sample buffer too small!\n");
		return -1;
	}
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

	/* incoming samples have been added to the list of leftover samples; adjust
	   total number of samples to reflect total size of leftover samples buffer */
	num_samples = (int)(leftover_samples_end - samples);
	if (num_samples < samples_per_block) {
		/* not enough samples to fill a block, so we have to wait until a
		   subsequent call to this function fills the buffer full enough */
		return 0;
	}

	st = out.num_channels == 2;

	if (format_type == FORMAT_MS) { /* Microsoft ADPCM */
		for (i = 0; i < out.num_channels; i++) {
			int predictor = 0;
			*buf++ = predictor;
			channel_status[i].coeff1 = adpcm_AdaptCoeff1[predictor];
			channel_status[i].coeff2 = adpcm_AdaptCoeff2[predictor];
		}
		for (i = 0; i < out.num_channels; i++) {
			if (channel_status[i].idelta < 16)
				channel_status[i].idelta = 16;
			PUT_LE_WORD(buf, channel_status[i].idelta);
		}
		for (i = 0; i < out.num_channels; i++)
			channel_status[i].sample2 = *samples++;
		for (i = 0; i < out.num_channels; i++) {
			channel_status[i].sample1 = *samples++;
			PUT_LE_WORD(buf, channel_status[i].sample1);
		}
		for (i = 0; i < out.num_channels; i++)
			PUT_LE_WORD(buf, channel_status[i].sample2);

		for (i = 7 * out.num_channels; i < out.block_align; i++) {
			int nibble;
			nibble  = adpcm_ms_compress_sample(&channel_status[ 0], *samples++) << 4;
			nibble |= adpcm_ms_compress_sample(&channel_status[st], *samples++);
			*buf++  = nibble;
		}
	}
	else if (format_type == FORMAT_YAMAHA) { /* Yamaha ADPCM */
		int n = samples_per_block / 2;
		for (n *= out.num_channels; n > 0; n--) {
			int nibble;
			nibble  = adpcm_yamaha_compress_sample(&channel_status[ 0], *samples++);
			nibble |= adpcm_yamaha_compress_sample(&channel_status[st], *samples++) << 4;
			*buf++  = nibble;
		}
	}
	else { /* IMA ADPCM */
		for (ch = 0; ch < out.num_channels; ch++) {
			status = &channel_status[ch];
			status->sample1 = *samples++;
			/* status->step = 0;
			   XXX: not sure how to init the state machine */
			PUT_LE_WORD(buf, status->sample1);
			*buf++ = status->step;
			*buf++ = 0; /* unknown */
		}
        /* stereo: 4 bytes (8 samples) for left, 4 bytes for right */
		/* 0 2 4 6 8 10 12 14 1 3 5 7 9 11 13 15 */
		if (st) {
			int blocks = (samples_per_block - 1) / 8;

			for (i = 0; i < blocks; i++) {
				for (ch = 0; ch < 2; ch++) {
					status = &channel_status[ch];
					for (j = 0; j < 16; j += 4) {
						UBYTE nibble;

						nibble = adpcm_ima_compress_sample(status, samples[ch + j]);
						nibble |= adpcm_ima_compress_sample(status, samples[ch + j + 2]) << 4;
						*buf++ = nibble;
					}
				}
				samples += 16;
			}
		}
		else {
			status = &channel_status[0];
			for (i = 0; i < samples_per_block - 1; i+=2) {
				UBYTE nibble;

				nibble = adpcm_ima_compress_sample(status, *samples++);
				nibble |= adpcm_ima_compress_sample(status, *samples++) << 4;
				*buf++ = nibble;
			}
		}
	}

	samples_consumed = (int)(samples - leftover_samples);

	/* move leftover samples down and leave them for the next time through the loop */
	memmove(leftover_samples, samples, (num_samples - samples_consumed) * 2);
	leftover_samples_end = leftover_samples + (num_samples - samples_consumed);

	out.length++;

	return buf - buf_start;
}

/* Because this codec adds audio frames to the AVI at a different rate than
   video frames get added, we need to compute rate of audio frames using number
   of video frames to calculate the total duration of the AVI. */
static void update_rate(void)
{
	float rate;

	if (final_duration > 0) {
		rate = (float)out.length / final_duration;
		out.rate = (int)(rate * 1000000);
	}
}

static int ADPCM_AnotherFrame(void)
{
	if (final_duration) {
		/* every time we add another frame, have to update the output rate to
		   make sure the correct value will be written to the AVI header. */
		update_rate();
		return (int)(leftover_samples_end - leftover_samples) > 0;
	}
	return (int)(leftover_samples_end - leftover_samples) >= samples_per_block;
}

static int ADPCM_Flush(float duration)
{
	final_duration = duration;
	update_rate();

	return (int)(leftover_samples_end - leftover_samples) > 0;
}

static int ADPCM_End(void)
{
	free(leftover_samples);
	return 1;
}

/* Yamaha ADPCM seems to be better than MS for square waves, so it's the default */
AUDIO_CODEC_t Audio_Codec_ADPCM = {
	"adpcm",
	"DVI IMA ADPCM",
	{1, 0, 0, 0}, /* fourcc */
	FORMAT_IMA, /* format type */
	0, /* flags */
	&ADPCM_Init_IMA,
	&ADPCM_AudioOut,
	&ADPCM_CreateFrame,
	&ADPCM_AnotherFrame,
	&ADPCM_Flush,
	&ADPCM_End,
};

AUDIO_CODEC_t Audio_Codec_ADPCM_IMA = {
	"adpcm_ima_wav",
	"DVI IMA ADPCM",
	{1, 0, 0, 0}, /* fourcc */
	FORMAT_IMA, /* format type */
	0, /* flags */
	&ADPCM_Init_IMA,
	&ADPCM_AudioOut,
	&ADPCM_CreateFrame,
	&ADPCM_AnotherFrame,
	&ADPCM_Flush,
	&ADPCM_End,
};

AUDIO_CODEC_t Audio_Codec_ADPCM_MS = {
	"adpcm_ms",
	"Microsoft ADPCM",
	{1, 0, 0, 0}, /* fourcc */
	FORMAT_MS, /* format type */
	0, /* flags */
	&ADPCM_Init_MS,
	&ADPCM_AudioOut,
	&ADPCM_CreateFrame,
	&ADPCM_AnotherFrame,
	&ADPCM_Flush,
	&ADPCM_End,
};

AUDIO_CODEC_t Audio_Codec_ADPCM_YAMAHA = {
	"adpcm_yamaha",
	"Yamaha ADPCM",
	{1, 0, 0, 0}, /* fourcc */
	FORMAT_YAMAHA, /* format type */
	0, /* flags */
	&ADPCM_Init_Yamaha,
	&ADPCM_AudioOut,
	&ADPCM_CreateFrame,
	&ADPCM_AnotherFrame,
	&ADPCM_Flush,
	&ADPCM_End,
};

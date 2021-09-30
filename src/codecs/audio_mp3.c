/*
 * audio_mp3.c - MP3 audio codec
 *
 * This is a derivative work of code from the FFmpeg project. The FFmpeg code
 * used here is licensed under the LGPLv2.1; Provisions within that license
 * permit derivative works to be licensed under the GPLv2.
 *
 * Modifications Copyright (C) 2021 Rob McMullen
 * Original code Copyright (c) 2001, 2002 Fabrice Bellard
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
#include "util.h"
#include "log.h"
#include "file_export.h"

#include <lame.h>
#include "codecs/audio.h"
#include "codecs/audio_mp3.h"

#define DEFAULT_BITRATE 128

static AUDIO_OUT_t out;

static float final_duration;
static int flushing;
static int flushed;
static int last_frame_size;
static int leftover_bytes;
static lame_global_flags *lame;

#define AV_RB32(x)                                 \
	(((ULONG)((const UBYTE*)(x))[0] << 24) |    \
			   (((const UBYTE*)(x))[1] << 16) |    \
			   (((const UBYTE*)(x))[2] <<  8) |    \
				((const UBYTE*)(x))[3])

const UWORD mpa_freq_tab[3] = { 44100, 48000, 32000 };
const UWORD mpa_bitrate_tab[2][3][15] = {
	{ {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
	  {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384 },
	  {0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320 } },
	{ {0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256},
	  {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160},
	  {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160}
	}
};

static int mpegaudio_frame_size(ULONG header)
{
	int sample_rate, frame_size, mpeg25, padding;
	int sample_rate_index, bitrate_index;
	int layer, lsf;

	/* header */
	if ((header & 0xffe00000) != 0xffe00000)
		return -1;
	/* version check */
	if ((header & (3<<19)) == 1<<19)
		return -2;
	/* layer check */
	if ((header & (3<<17)) == 0)
		return -3;
	/* bit rate */
	if ((header & (0xf<<12)) == 0xf<<12)
		return -4;
	/* frequency */
	if ((header & (3<<10)) == 3<<10)
		return -5;

	if (header & (1<<20)) {
		lsf = (header & (1<<19)) ? 0 : 1;
		mpeg25 = 0;
	} else {
		lsf = 1;
		mpeg25 = 1;
	}

	layer = 4 - ((header >> 17) & 3);
	/* extract frequency */
	sample_rate_index = (header >> 10) & 3;
	if (sample_rate_index >= sizeof(mpa_freq_tab))
		sample_rate_index = 0;
	sample_rate = mpa_freq_tab[sample_rate_index] >> (lsf + mpeg25);

	bitrate_index = (header >> 12) & 0xf;
	padding = (header >> 9) & 1;

	if (bitrate_index != 0) {
		frame_size = mpa_bitrate_tab[lsf][layer - 1][bitrate_index];
		switch(layer) {
		case 1:
			frame_size = (frame_size * 12000) / sample_rate;
			frame_size = (frame_size + padding) * 4;
			break;
		case 2:
			frame_size = (frame_size * 144000) / sample_rate;
			frame_size += padding;
			break;
		default:
		case 3:
			frame_size = (frame_size * 144000) / (sample_rate << lsf);
			frame_size += padding;
			break;
		}
	} else {
		/* if no frame size computed, signal it */
		return -6;
	}

	return frame_size;
}

static int MP3_Init(int sample_rate, float fps, int sample_size, int num_channels)
{
	UBYTE *extra;
	int bitrate;
	int requested_out_samplerate;
	int encoder_delay;
	int result;
	int abr = FALSE; /* Currently CBR only */

	requested_out_samplerate = audio_param_samplerate;
	if (requested_out_samplerate < 0)
		requested_out_samplerate = sample_rate;

	lame = lame_init();
	if (!lame) {
		Log_print("Failed initializing libmp3lame");
		return -1;
	}
	lame_set_in_samplerate(lame, sample_rate);
	lame_set_out_samplerate(lame, requested_out_samplerate);
	lame_set_num_channels(lame, num_channels);
	lame_set_mode(lame, num_channels > 1 ? 0 : 3); /* 0 = stereo, 1 = joint stereo, 3 = mono */
	lame_set_quality(lame, 9 - audio_param_quality); /* libmp3lame uses 0 = best, 9 = worst */
	if (abr) {
		lame_set_VBR_mean_bitrate_kbps(lame, audio_param_bitrate);
		lame_set_VBR(lame, 3); /* ABR coding */
	}
	else {
		lame_set_brate(lame, audio_param_bitrate);
		lame_set_VBR(lame, 0);
	}
	lame_set_bWriteVbrTag(lame, 0); /* don't write Xing header */
	lame_set_disable_reservoir(lame, 1); /* need constant size frames to decode them in MP3_CreateFrame */
 
	result = lame_init_params(lame);
	if (result < 0) {
		Log_print("Invalid libmp3lame initialization parameters");
		return -1;
	}

	if (requested_out_samplerate != lame_get_out_samplerate(lame))
		Log_print("audio_mp3: requested sample rate %d not available; using %d", requested_out_samplerate, lame_get_out_samplerate(lame));
	if (audio_param_bitrate != lame_get_brate(lame))
		Log_print("audio_mp3: requested bitrate %d not available; using %d", audio_param_bitrate, lame_get_brate(lame));

	out.sample_rate = lame_get_out_samplerate(lame);
	out.block_align = lame_get_framesize(lame);
	encoder_delay = lame_get_encoder_delay(lame);
	bitrate = lame_get_brate(lame);

	last_frame_size = 0;
	leftover_bytes = 0;
	out.sample_size = 1;
	out.bits_per_sample = 0;
	out.bitrate = bitrate * 1000;
	out.num_channels = num_channels;
	out.scale = 1000000;
	out.rate = (int)(fps * 1000000);
	out.length = 0;
	out.extra_data_size = 14;
	extra = out.extra_data;
	PUT_LE_WORD(extra, 12); /* extra size */
	PUT_LE_WORD(extra, 1); /* wID, MPEGLAYER3_ID_MPEG */
	PUT_LE_WORD(extra, 2); /* low word wFlags, PADDING_OFF */
	PUT_LE_WORD(extra, 0); /* high word wFlags, 0 */
	PUT_LE_WORD(extra, out.block_align); /* nBlockSize */
	PUT_LE_WORD(extra, 1); /* nFramesPerBlock */
	PUT_LE_WORD(extra, encoder_delay + 528 + 1); /* nCodecDelay, straight from ffmpeg, no idea why */
	out.samples_processed = 0;

	final_duration = 0.0;
	flushing = 0;
	flushed = 0;

	return (int)(1.25 * sample_rate / 50 + 7200);
}

static AUDIO_OUT_t *MP3_AudioOut(void) {
	return &out;
}

static int MP3_CreateFrame(const UBYTE *source, int num_samples, UBYTE *buf, int bufsize)
{
	int encoded_size;
	ULONG h;

	/* Clean up last frame. If last_frame_size is positive, then the caller
	   pulled a frame out of the audio data the last time this function was
	   called. The data is still there on the buffer, though, so we remove that
	   last frame and reset the memory pointers so the next frame gets stored
	   immediately after any frames still in the buffer.. */
	if (last_frame_size > 0) {
		if (leftover_bytes > last_frame_size) {
			leftover_bytes -= last_frame_size;
			/* move the bytes starting at last_frame_size to zero which makes
			   those frames next in line to be pulled off the buffer. */
			memmove(buf, buf + last_frame_size, leftover_bytes);
		}
		else {
			/* no leftover bytes, so don't need to move or clear anything */
			leftover_bytes = 0;
		}
		last_frame_size = 0;
	}

	/* Add new frames to the end of the buffer. It is possible that there are
	   multiple frames, ususally this happens as a result of lame_encode_flush,
	   but FFmpeg is coded as if it may happen other times. */
	if (flushing) {
		/* Only need to call lame_encode_flusg once. Multiple frames are likely encoded here */
		encoded_size = lame_encode_flush(lame, buf + leftover_bytes, bufsize - leftover_bytes);
		flushing = FALSE;
		flushed = TRUE;
	}
	else if (flushed) {
		/* just deal with remaining samples */
		encoded_size = 0;
	}
	else {
		switch (out.num_channels) {
		case 1:
			encoded_size = lame_encode_buffer(lame, (SWORD *)source, NULL, num_samples, buf + leftover_bytes, bufsize - leftover_bytes);
			out.samples_processed += num_samples;
			break;
		default:
			encoded_size = lame_encode_buffer_interleaved(lame, (SWORD *)source, num_samples / 2, buf + leftover_bytes, bufsize - leftover_bytes);
			out.samples_processed += num_samples / 2;
			break;
		}
	}
	leftover_bytes += encoded_size;
	/* printf("encoded size=%d, total=%d num frames=%d\n", encoded_size, leftover_bytes, lame_get_frameNum(lame)); */
	if (leftover_bytes > 0) {

		/* See if there is a frame in the data, and if so, output it. */

		h = AV_RB32(buf);
		last_frame_size = mpegaudio_frame_size(h);
		if (last_frame_size < 0) {
			Log_print("Failed decoding mp3 header, err=%d", last_frame_size);
			return -1;
		}
		else if (last_frame_size > 0) {
			out.length++;
		}
	}

	/* The caller uses the number of bytes in last_frame_size as the frame to
	   add to the output. The remaining data in the buffer will be sent in
	   subsequent calls to MP3_CreateFrame. */
	return last_frame_size;
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

static int MP3_AnotherFrame(void)
{
	return (flushing || leftover_bytes > 0);
}

static int MP3_Flush(float duration)
{
	final_duration = duration;
	flushing = TRUE;
	update_rate();

	/* always force the flush */
	return 1;
}

static int MP3_End(void)
{
	lame_close(lame);
	return 1;
}

/* Yamaha ADPCM seems to be better than MS for square waves, so it's the default */
AUDIO_CODEC_t Audio_Codec_MP3 = {
	"mp3",
	"MP3 Audio",
	{1, 0, 0, 0}, /* fourcc */
	0x55, /* format type */
	AUDIO_CODEC_FLAG_VBR_POSSIBLE,
	&MP3_Init,
	&MP3_AudioOut,
	&MP3_CreateFrame,
	&MP3_AnotherFrame,
	&MP3_Flush,
	&MP3_End,
};

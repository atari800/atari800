/*
 * container_wav.c - support for WAV audio files
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
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


/* This file is only compiled when SOUND is defined. */

#include <stdio.h>
#include <stdlib.h>
#include "file_export.h"
#include "codecs/video_mrle.h"
#include "screen.h"
#include "colours.h"
#include "cfg.h"
#include "util.h"
#include "log.h"
#include "sound.h"
#include "pokeysnd.h"
#include "codecs/container.h"
#include "codecs/container_wav.h"
#include "codecs/audio.h"


static FILE *fp = NULL;

/* WAV_OpenFile will start a new sound file and write out the header. Note that
   the file will not be valid until the it is closed with WAV_CloseFile because
   the length information contained in the header must be updated with the
   number of samples in the file.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */
static int WAV_OpenFile(const char *filename)
{
	if (!(fp = fopen(filename, "wb")))
		return 0;
	/*
	The RIFF header:

	  Offset  Length   Contents
	  0       4 bytes  'RIFF'
	  4       4 bytes  <file length - 8>
	  8       4 bytes  'WAVE'

	The fmt chunk:

	  12      4 bytes  'fmt '
	  16      4 bytes  0x00000010     // Length of the fmt data (16 bytes)
	  20      2 bytes  0x0001         // Format tag: 1 = PCM
	  22      2 bytes  <channels>     // Channels: 1 = mono, 2 = stereo
	  24      4 bytes  <sample rate>  // Samples per second: e.g., 44100
	  28      4 bytes  <bytes/second> // sample rate * block align
	  32      2 bytes  <block align>  // channels * bits/sample / 8
	  34      2 bytes  <bits/sample>  // 8 or 16

	The data chunk:

	  36      4 bytes  'data'
	  40      4 bytes  <length of the data block>
	  44        bytes  <sample data>

	All chunks must be word-aligned.

	Good description of WAVE format: http://www.sonicspot.com/guide/wavefiles.html
	*/

	fputs("RIFF", fp);
	fputl(0, fp); /* length to be filled in upon file close */
	fputs("WAVE", fp);

	fputs("fmt ", fp);
	fputl(16 + audio_out->extra_data_size, fp);
	fputw(audio_codec->format_type, fp);
	fputw(audio_out->num_channels, fp);
	fputl(audio_out->sample_rate, fp);
	fputl(audio_out->sample_rate * audio_out->num_channels * audio_out->sample_size, fp);
	fputw(audio_out->block_align, fp);
	fputw(audio_out->bits_per_sample, fp);
	if (audio_out->extra_data_size > 0) {
		fwrite(audio_out->extra_data, audio_out->extra_data_size, 1, fp);
	}

	fputs("data", fp);
	fputl(0, fp); /* length to be filled in upon file close */

	if (ftell(fp) != 44 + audio_out->extra_data_size) {
		fclose(fp);
		return 0;
	}
	return 1;
}

/* WAV_WriteSamples will dump PCM data to the WAV file. The best way
   to do this for Atari800 is probably to call it directly after
   POKEYSND_Process(buffer, size) with the same values (buffer, size)

   RETURNS: the number of bytes written to the file (should be equivalent to the
   input num_samples * sample size) */
static int WAV_WriteSamples(const UBYTE *buf, int num_samples)
{
	int size;

	if (!fp) return 0;

	if (!buf) {
		/* This happens at file close time, checking if audio codec has samples
		   remaining */
		if (!audio_codec->another_frame || !audio_codec->another_frame(TRUE)) {
			/* If the codec doesn't support buffered frames or there's nothing
			   remaining, there's no need to try to write another frame */
			return 1;
		}
	}

	do {
		video_frame_count++;

		size = audio_codec->frame(buf, num_samples, audio_buffer, audio_buffer_size);
		if (size < 0) {
			/* failed creating video frame; force close of file */
			Log_print("audio codec %s failed encoding frame", audio_codec->codec_id);
			return 0;
		}

		/* If audio frame size is zero, that means the codec needs more samples
		   before it can create a frame. See audio_codec_adpcm.c for an example.
		   Only if there is some data do we write the frame to the file. */
		if (size > 0) {
			size = fwrite(audio_buffer, 1, size, fp);
			if (!size) {
				/* failed during write; force close of file */
				return 0;
			}
			byteswritten += size;

			/* for next loop, only output samples remaining from previous frame */
			num_samples = 0;
		}
		else {
			/* report success if there weren't enough samples to fill a frame. */
			return 1;
		}
	} while (audio_codec->another_frame && audio_codec->another_frame(FALSE));

	return 1;
}


/* WAV_CloseFile must be called to create a valid WAV file, because the header
   at the beginning of the file must be modified to indicate the number of
   samples in the file.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */
static int WAV_CloseFile(void)
{
	int result = TRUE;
	char aligned = 0;
	int seconds;

	if (fp != NULL) {
		/* Force audio codec to write out the last frame. This only occurs in codecs
		with fixed block alignments */
		result = WAV_WriteSamples(NULL, 0);

		if (result) {
			/* A RIFF file's chunks must be word-aligned. So let's align. */
			if (byteswritten & 1) {
				if (putc(0, fp) == EOF)
					result = FALSE;
				else
					aligned = 1;
			}

			if (result) {
				CODECS_AUDIO_End((float)(video_frame_count / fps));

				/* Sound file is finished, so modify header and close it. */
				if (fseek(fp, 4, SEEK_SET) != 0)	/* Seek past RIFF */
					result = FALSE;
				else {
					/* RIFF header's size field must equal the size of all chunks
					* with alignment, so the alignment byte is added.
					*/
					fputl(byteswritten + 36 + aligned, fp);
					if (fseek(fp, 40 + audio_out->extra_data_size, SEEK_SET) != 0)
						result = FALSE;
					else {
						/* But in the "data" chunk size field, the alignment byte
						* should be ignored. */
						fputl(byteswritten, fp);
						seconds = (int)(video_frame_count / fps);
						Log_print("WAV stats: %d:%02d:%02d, %dMB, %d frames", seconds / 60 / 60, (seconds / 60) % 60, seconds % 60, byteswritten / 1024 / 1024, video_frame_count);
					}
				}
			}
		}
		fclose(fp);
	}

	return result;
}


CONTAINER_t Container_WAV = {
    "wav",
    "WAV format audio",
    &WAV_OpenFile,
    &WAV_WriteSamples,
    NULL,
    &WAV_CloseFile,
};

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


/* This file is only compiled when AUDIO_RECORDING is defined. */

#include <stdio.h>
#include <stdlib.h>
#include "file_export.h"
#include "log.h"
#include "file_export.h"
#include "codecs/container.h"
#include "codecs/container_wav.h"
#include "codecs/audio.h"


static int fact_chunk_size;

/* WAV_Prepare will start a new sound file and write out the header. Note that
   the file will not be valid until the it is closed with WAV_Finalize because
   the length information contained in the header must be updated with the
   number of samples in the file.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */
static int WAV_Prepare(FILE *fp)
{
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
	fputl(audio_out->bitrate / 8, fp);
	fputw(audio_out->block_align, fp);
	fputw(audio_out->bits_per_sample, fp);
	if (audio_out->extra_data_size > 0) {
		fwrite(audio_out->extra_data, audio_out->extra_data_size, 1, fp);
	}

	if (audio_codec->codec_flags & AUDIO_CODEC_FLAG_VBR_POSSIBLE) {
		/* compressed codecs need sample count, used with bytes/second field to
		   determine duration */
		fact_chunk_size = 12;
		fputs("fact", fp);
		fputl(4, fp);
		fputl(0, fp); /* number of samples, to be filled in upon file close */
	}
	else {
		fact_chunk_size = 0;
	}

	fputs("data", fp);
	fputl(0, fp); /* length to be filled in upon file close */

	if (ftell(fp) != 44 + audio_out->extra_data_size + fact_chunk_size) {
		File_Export_SetErrorMessage("Error writing WAV header");
		return 0;
	}
	return 1;
}

/* WAV_AudioFrame will dump PCM data to the WAV file. The best way
   to do this for Atari800 is probably to call it directly after
   POKEYSND_Process(buffer, size) with the same values (buffer, size)

   RETURNS: 1 if frame successfully written, 0 if not */
static int WAV_AudioFrame(FILE *fp, const UBYTE *buf, int bufsize)
{
	int size;

	size = fwrite(buf, 1, bufsize, fp);
	if (size < bufsize) {
		File_Export_SetErrorMessage("Failed writing to WAV file");
		Log_print("Failed writing audio: expected %d, wrote %d", bufsize, size);
		size = 0;
	}

	return (size > 0);
}

static int WAV_SizeCheck(int size) {
	return size < MAX_RIFF_FILE_SIZE;
}

/* WAV_Finalize must be called to create a valid WAV file, because the header
   at the beginning of the file must be modified to indicate the number of
   samples in the file.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */
static int WAV_Finalize(FILE *fp)
{
	int result = TRUE;
	char aligned = 0;

	/* A RIFF file's chunks must be word-aligned. So let's align. */
	if (byteswritten & 1) {
		fputc(0, fp);
		aligned = 1;
	}

	/* Sound file is finished, so modify header and close it. */

	/* RIFF header's size field must equal the size of all chunks with
		alignment, so the alignment byte is added. */
	fseek(fp, 4, SEEK_SET);	/* Seek past RIFF */
	fputl(byteswritten + 36 + aligned, fp);

	/* Alignment byte is ignored in the "data" chunk size field. */
	fseek(fp, 40 + audio_out->extra_data_size + fact_chunk_size, SEEK_SET);
	fputl(byteswritten, fp);

	if (fact_chunk_size) {
		/* number of samples is needed in non-PCM formats */
		fseek(fp, 44 + audio_out->extra_data_size, SEEK_SET);
		fputl(audio_out->samples_processed, fp);
	}

	return result;
}


CONTAINER_t Container_WAV = {
	"wav",
	"WAV format audio",
	&WAV_Prepare,
	&WAV_AudioFrame,
	NULL,
	&WAV_SizeCheck,
	&WAV_Finalize,
};

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


/* This file is only compiled when SOUND and HAVE_LIBMP3LAME are defined. */

#include <stdio.h>
#include <stdlib.h>
#include "file_export.h"
#include "util.h"
#include "log.h"
#include "codecs/container.h"
#include "codecs/container_mp3.h"
#include "codecs/audio.h"


static FILE *fp = NULL;

/* MP3_OpenFile will start a new sound file and write out the header. Note that
   an MP3 file isn't really required to be anything but a concatenation of MP3
   frames.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */
static int MP3_OpenFile(const char *filename)
{
	if (!(fp = fopen(filename, "wb")))
		return 0;

	return 1;
}

/* MP3_WriteSamples will dump PCM data to the WAV file. The best way
   to do this for Atari800 is probably to call it directly after
   POKEYSND_Process(buffer, size) with the same values (buffer, size)

   RETURNS: the number of bytes written to the file (should be equivalent to the
   input num_samples * sample size) */
static int MP3_WriteSamples(const UBYTE *buf, int num_samples)
{
	int size;

	if (!fp) return 0;

	if (!buf) {
		/* This happens at file close time, checking if audio codec has samples
		   remaining */
		if (!audio_codec->another_frame()) {
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
	} while (audio_codec->another_frame());

	return 1;
}


/* MP3_CloseFile must be called to create a valid WAV file, because the header
   at the beginning of the file must be modified to indicate the number of
   samples in the file.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */
static int MP3_CloseFile(void)
{
	int result = TRUE;
	int seconds;

	if (fp != NULL) {
		if (audio_codec->flush((float)(video_frame_count / fps))) {
			/* Force audio codec to write out any remaining frames. This only
			   occurs in codecs that buffer frames or force fixed block sizes */
			result = MP3_WriteSamples(NULL, 0);
		}

		if (result) {
			CODECS_AUDIO_End();

			seconds = (int)(video_frame_count / fps);
			Log_print("MP3 stats: %d:%02d:%02d, %d%sB, %d frames", seconds / 60 / 60, (seconds / 60) % 60, seconds % 60, byteswritten < 1024*1024 ? byteswritten / 1024 : byteswritten / 1024 / 1024, byteswritten < 1024*1024 ? "k" : "M", video_frame_count);

			if (ferror(fp)) {
				Log_print("Error writing MPe header\n");
				result = 0;
			}
		}
		fclose(fp);
	}

	return result;
}


CONTAINER_t Container_MP3 = {
    "mp3",
    "MP3 format audio",
    &MP3_OpenFile,
    &MP3_WriteSamples,
    NULL,
    &MP3_CloseFile,
};

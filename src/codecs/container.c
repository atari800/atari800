/*
 * container.c - interface for multimedia containers
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


/* This file is only compiled when either SOUND or VIDEO_RECORDING is defined. */

#include <stdio.h>
#include <stdlib.h>
#include "screen.h"
#include "util.h"
#include "log.h"
#include "file_export.h"
#include "codecs/container.h"
#ifdef SOUND
#include "sound.h"
#include "codecs/audio.h"
#include "codecs/container_wav.h"
#ifdef AUDIO_CODEC_MP3
#include "codecs/container_mp3.h"
#endif
#endif
#ifdef VIDEO_RECORDING
#include "codecs/video.h"
#include "codecs/container_avi.h"
#endif

/* Global pointer to current multimedia container, or NULL if one has not been
   initialized. This pointer should not be used after a call to
   container->close(). */
CONTAINER_t *container = NULL;

/* Global variable containing the amount of bytes written to the currently open
   container. This value is updated as the container adds video and audio
   frames, so may be used during the creation of the file */
ULONG byteswritten;

/* Global variable containing the number of video frames processed during the
   creation of the multimedia file. This is updated even when audio-only files
   are being created, because the duration of the file must be tracked to write
   audio header information. */
ULONG video_frame_count;

/* Global variable containing the frames per second at time of file creation,
   either Atari800_FPS_NTSC or Atari800_FPS_PAL. */
float fps;

/* Global variable containing the text description of the current container type
   and the video and audio codecs being used to write data into the file. */
char description[32];

static CONTAINER_t *known_containers[] = {
#ifdef SOUND
	&Container_WAV,
#ifdef AUDIO_CODEC_MP3
	&Container_MP3,
#endif
#endif
#ifdef VIDEO_RECORDING
	&Container_AVI,
#endif
	NULL,
};

static FILE *fp = NULL;

/* Some codecs allow for keyframes (full frame compression) and inter-frames
   (only the differences from the previous frame) */
static int keyframe_count;

/* audio statistics */
static ULONG audio_frame_count;
static ULONG total_audio_size;
static ULONG smallest_audio_frame;
static ULONG largest_audio_frame;

/* video statistics */
static ULONG total_video_size;
static ULONG smallest_video_frame;
static ULONG largest_video_frame;


static CONTAINER_t *match_container(const char *id)
{
	CONTAINER_t **v = known_containers;
	CONTAINER_t *found = NULL;

	while (*v) {
		if (Util_striendswith(id, (*v)->container_id)) {
			found = *v;
			break;
		}
		v++;
	}
	return found;
}


/* Convenience function to check if container type is supported. */
int CONTAINER_IsSupported(const char *filename)
{
	CONTAINER_t *c;

	c = match_container(filename);

	return (c != NULL);
}


static int close_codecs(void)
{
#ifdef VIDEO_RECORDING
	if (video_codec) {
		CODECS_VIDEO_End();
		video_codec = NULL;
	}
#endif
#ifdef SOUND
	if (audio_codec) {
		CODECS_AUDIO_End();
		audio_codec = NULL;
	}
#endif
	container = NULL;

	return 0;
}

/* Sets the global container variable if the filename has an extension that
   matches a known container. It will also set the audio_codec and video_codec
   if the container supports either. For each audio or video codec that is not
   used, they will be set to NULL so calling functions can check those variables
   to determine if audio or video is being used. */
int CONTAINER_Open(const char *filename)
{
	container = match_container(filename);

	if (container) {

		/* initialize variables common to all containers */
		fps = Atari800_tv_mode == Atari800_TV_PAL ? Atari800_FPS_PAL : Atari800_FPS_NTSC;
		byteswritten = 0;

		keyframe_count = 0; /* force first frame to be keyframe */

		/* video statistics */
		video_frame_count = 0;
		total_video_size = 0;
		smallest_video_frame = 0xffffffff;
		largest_video_frame = 0;

		/* audio statistics */
		audio_frame_count = 0;
		total_audio_size = 0;
		smallest_audio_frame = 0xffffffff;
		largest_audio_frame = 0;

#ifdef SOUND
		if (Sound_enabled) {
			if (!CODECS_AUDIO_Init()) {
				/* error message set in codec */
				Log_print(FILE_EXPORT_error_message);
				return close_codecs();
			}
		}
		else {
			audio_codec = NULL;
		}
#endif
#ifdef VIDEO_RECORDING
		if (container->video_frame) {
			if (!CODECS_VIDEO_Init()) {
				/* error message set in codec */
				Log_print(FILE_EXPORT_error_message);
				return close_codecs();
			}
		}
		else {
			video_codec = NULL;
		}
#endif
		strcpy(description, container->container_id);
#ifdef VIDEO_RECORDING
		if (video_codec) {
			strcat(description, " ");
			strcat(description, video_codec->codec_id);
		}
#endif
#ifdef SOUND
		if (audio_codec) {
			/* If the audio codec has the same name as the container (e.g. mp3
			   is both a codec and container type), don't duplicate the name */
			if (strcmp(container->container_id, audio_codec->codec_id) != 0) {
				strcat(description, " ");
				strcat(description, audio_codec->codec_id);
			}
		}
#endif
		fp = fopen(filename, "wb");
		if (fp) {
			if (!container->prepare(fp)) {
				/* error message set in container */
				Log_print(FILE_EXPORT_error_message);
				fclose(fp);
				fp = NULL;
			}
		}
		else {
			File_Export_SetErrorMessageArg("Can't write to file \"%s\"", filename);
		}
	}
	else {
		File_Export_SetErrorMessageArg("Unsupported file type \"%s\"", filename);
		Log_print(FILE_EXPORT_error_message);
	}

	if (!fp) {
		close_codecs();
	}

	return (fp != NULL);
}

int CONTAINER_AddAudioSamples(const UBYTE *buf, int num_samples)
{
	int result;
	int size;

	if (!fp || !audio_codec) return 0;

	if (!buf) {
		/* This happens at file close time, checking if audio codec has samples
		   remaining */
		if (!audio_codec->another_frame()) {
			/* If the codec doesn't support buffered frames or there's nothing
			   remaining, there's no need to try to write another frame */
			return 1;
		}
	}
	else if (!video_codec) {
		/* Before file close time, there is one call to this function every
		   video frame. If the video codec is not being used, we need to count
		   frames here because frame count is used to determine the duration of
		   the audio file. */
		video_frame_count++;
	}

	do {
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
			if (!container->audio_frame(fp, audio_buffer, size)) {
				/* failed during write; force close of file */
				return 0;
			}

			/* for next loop, only output samples remaining from previous frame */
			num_samples = 0;

			/* update statistics */
			byteswritten += size;
			audio_frame_count++;
			total_audio_size += size;
		}
		if (size < smallest_audio_frame) {
			smallest_audio_frame = size;
		}
		if (size > largest_audio_frame) {
			largest_audio_frame = size;
		}
	} while (audio_codec->another_frame());

	result = container->size_check(ftell(fp));
	if (!result) {
		Log_print("%s maximum file size reached, closing file", container->container_id);
	}
	return result;
}

int CONTAINER_AddVideoFrame(void)
{
	int size;
	int result;
	int is_keyframe;

	if (!fp || !video_codec) return 0;

	/* When a codec uses interframes (deltas from the previous frame), a
	   keyframe is needed every keyframe interval. */
	if (video_codec->uses_interframes) {
		keyframe_count--;
		if (keyframe_count <= 0) {
			is_keyframe = TRUE;
			keyframe_count = video_codec_keyframe_interval;
		}
		else {
			is_keyframe = FALSE;
		}
	}
	else {
		is_keyframe = TRUE;
	}

	size = video_codec->frame((UBYTE *)Screen_atari, is_keyframe, video_buffer, video_buffer_size);
	if (size < 0) {
		/* failed creating video frame; force close of file */
		Log_print("video codec %s failed encoding frame", video_codec->codec_id);
		return 0;
	}
	result = container->video_frame(fp, video_buffer, size, is_keyframe);
	if (result) {
		/* update statistics */
		byteswritten += size;
		video_frame_count++;
		total_video_size += size;
		if (size < smallest_video_frame) {
			smallest_video_frame = size;
		}
		if (size > largest_video_frame) {
			largest_video_frame = size;
		}

		result = container->size_check(ftell(fp));
		if (!result) {
			Log_print("%s maximum file size reached, closing file", container->container_id);
		}
	}

	return result;
}

/* Closes the current container, flushing any buffered audio data and updating
   the container metadata with the final sizes of all video and audio frames
   written. */
int CONTAINER_Close(int file_ok)
{
	int result = 1;
	int mega = FALSE;
	int size;
	int seconds;
	int video_average;
	int audio_average;

	if (!fp || !container) return 0;

	/* Note that all video frames will be written, but the audio codec may
		still have frames buffered. */

	if (file_ok && audio_codec && audio_codec->flush((float)(video_frame_count / fps))) {
		/* Force audio codec to write out any remaining frames. This only
			occurs in codecs that buffer frames or force fixed block sizes */
		result = CONTAINER_AddAudioSamples(NULL, 0);
	}

	if (result) {
		clearerr(fp);
		result = container->finalize(fp);
		if (ferror(fp)) {
			Log_print("Error finalizing %s file\n", container->container_id);
			result = 0;
		}
		else {
			/* success, print out stats */
			seconds = (int)(video_frame_count / fps);
			size = byteswritten / 1024;
			if (size > 1024 * 1024) {
				size /= 1024;
				mega = TRUE;
			}
			if (smallest_audio_frame == 0xffffffff) smallest_audio_frame = 0;
			if (smallest_video_frame == 0xffffffff) smallest_video_frame = 0;
			if (video_frame_count > 0) {
				video_average = total_video_size / video_frame_count;
				audio_average = total_audio_size / video_frame_count;
			}
			else {
				video_average = 0;
				audio_average = 0;
			}
			Log_print("%s stats: %d:%02d:%02d, %d%sB, %d frames, video %d/%d/%d, audio %d/%d/%d", container->container_id, seconds / 60 / 60, (seconds / 60) % 60, seconds % 60, size, mega ? "M": "k", video_frame_count, smallest_video_frame, video_average, largest_video_frame, smallest_audio_frame, audio_average, largest_audio_frame);
		}
	}
	fclose(fp);

	close_codecs();

	return result;
}

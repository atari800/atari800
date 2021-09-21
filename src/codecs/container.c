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
#include "util.h"
#include "codecs/container.h"
#ifdef SOUND
#include "sound.h"
#include "codecs/audio.h"
#include "codecs/container_wav.h"
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
#endif
#ifdef VIDEO_RECORDING
	&Container_AVI,
#endif
	NULL,
};


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


/* Sets the global container variable if the filename has an extension that
   matches a known container. It will also set the audio_codec and video_codec
   if the container supports either. For each audio or video codec that is not
   used, they will be set to NULL so calling functions can check those variables
   to determine if audio or video is being used. */
int CODECS_CONTAINER_Open(const char *filename)
{
	container = match_container(filename);

	if (container) {

		/* initialize variables common to all containers */
		fps = Atari800_tv_mode == Atari800_TV_PAL ? Atari800_FPS_PAL : Atari800_FPS_NTSC;
		video_frame_count = 0;
		byteswritten = 0;

#ifdef SOUND
		if (Sound_enabled) {
			if (!CODECS_AUDIO_Init()) {
				container = NULL;
				return 0;
			}
		}
		else {
			audio_codec = NULL;
		}
#endif
#ifdef VIDEO_RECORDING
		if (container->save_video) {
			if (!CODECS_VIDEO_Init()) {
#ifdef SOUND
				if (audio_codec) {
					CODECS_AUDIO_End();
				}
#endif
				container = NULL;
				return 0;
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
			strcat(description, " ");
			strcat(description, audio_codec->codec_id);
		}
#endif

		if (!container->open(filename)) {
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
		}
	}
	return (container != NULL);
}

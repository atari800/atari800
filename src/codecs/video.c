/*
 * video.c - interface for video codecs
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

#include <stdio.h>
#include <stdlib.h>
#include "screen.h"
#include "colours.h"
#include "cfg.h"
#include "util.h"
#include "log.h"

#include "codecs/image.h"
#include "codecs/video.h"
#include "codecs/video_mrle.h"
#ifdef VIDEO_CODEC_PNG
#include "codecs/video_mpng.h"
#endif
#ifdef VIDEO_CODEC_ZMBV
#include "codecs/video_zmbv.h"
#endif


static VIDEO_CODEC_t *requested_video_codec = NULL;
VIDEO_CODEC_t *video_codec = NULL;
int video_buffer_size = 0;
UBYTE *video_buffer = NULL;

/* Some codecs allow for keyframes (full frame compression) and inter-frames
   (only the differences from the previous frame) */
#define MAX_KEYFRAME_INTERVAL 500
int video_codec_keyframe_interval = 0;


static VIDEO_CODEC_t *known_video_codecs[] = {
	&Video_Codec_MRLE,
	&Video_Codec_MSRLE,
#ifdef VIDEO_CODEC_PNG
	&Video_Codec_MPNG,
#endif
#ifdef VIDEO_CODEC_ZMBV
	&Video_Codec_ZMBV,
#endif
	NULL,
};


static VIDEO_CODEC_t *match_video_codec(char *id)
{
	VIDEO_CODEC_t **v = known_video_codecs;
	VIDEO_CODEC_t *found = NULL;

	while (*v) {
		if (Util_stricmp(id, (*v)->codec_id) == 0) {
			found = *v;
			break;
		}
		v++;
	}
	return found;
}

static VIDEO_CODEC_t *get_best_video_codec(void)
{
	/* ZMBV is the default if we also have zlib because compressed ZMBV is far
	   superior to the others. If zlib is not available, RLE becomes the default
	   because it's better than uncompressed ZMBV in most cases. PNG is never
	   the default. */
#if defined(VIDEO_CODEC_ZMBV) && defined(HAVE_LIBZ)
	return &Video_Codec_ZMBV;
#else
	return &Video_Codec_MRLE;
#endif
}

static char *video_codec_args(char *buf)
{
	VIDEO_CODEC_t **v = known_video_codecs;

	strcpy(buf, "\t-vcodec auto");
	while (*v) {
		strcat(buf, "|");
		strcat(buf, (*v)->codec_id);
		v++;
	}
	return buf;
}


int CODECS_VIDEO_Initialise(int *argc, char *argv[])
{

	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc); /* is argument available? */
		int a_m = FALSE; /* error, argument missing! */
		int a_i = FALSE; /* error, argument invalid! */

		if (strcmp(argv[i], "-vcodec") == 0) {
			if (i_a) {
				char *mode = argv[++i];
				if (strcmp(mode, "auto") == 0) {
					requested_video_codec = NULL; /* want best available */
				}
				else {
					requested_video_codec = match_video_codec(mode);
					if (!requested_video_codec) {
						a_i = TRUE;
					}
				}
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-keyint") == 0) {
			if (i_a) {
				video_codec_keyframe_interval = Util_sscandec(argv[++i]);
				if (video_codec_keyframe_interval > MAX_KEYFRAME_INTERVAL) {
					Log_print("Invalid keyframe interval, must be less than 500 frames.");
					return FALSE;
				}
			}
			else a_m = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				char buf[256];
				Log_print(video_codec_args(buf));
				Log_print("\t                 Select video codec (default: auto)");
				Log_print("\t-keyint <num>    Set video keyframe interval to one keyframe every num frames");
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		} else if (a_i) {
			Log_print("Invalid argument for '%s'", argv[--i]);
			return FALSE;
		}
	}
	*argc = j;

	return TRUE;
}

int CODECS_VIDEO_ReadConfig(char *string, char *ptr)
{
	if (strcmp(string, "VIDEO_CODEC") == 0) {
		if (Util_stricmp(ptr, "auto") == 0) {
			requested_video_codec = NULL; /* want best available */
		}
		else {
			requested_video_codec = match_video_codec(ptr);
			if (!requested_video_codec) {
				return FALSE;
			}
		}
	}
	else if (strcmp(string, "VIDEO_CODEC_KEYFRAME_INTERVAL") == 0) {
		int num = Util_sscandec(ptr);
		if (num < 500)
			video_codec_keyframe_interval = num;
		else return FALSE;
	}
	else return FALSE;
	return TRUE;
}

void CODECS_VIDEO_WriteConfig(FILE *fp)
{
	if (!requested_video_codec) {
		fprintf(fp, "VIDEO_CODEC=AUTO\n");
	}
	else {
		fprintf(fp, "VIDEO_CODEC=%s\n", requested_video_codec->codec_id);
	}
	fprintf(fp, "VIDEO_CODEC_KEYFRAME_INTERVAL=%d\n", video_codec_keyframe_interval);
}


int CODECS_VIDEO_Init(void)
{
	if (video_codec_keyframe_interval == 0)
		video_codec_keyframe_interval = (Atari800_tv_mode == Atari800_TV_PAL ? 50 : 60);

	CODECS_IMAGE_SetMargins();

	if (!video_codec) {
		if (!requested_video_codec) {
			video_codec = get_best_video_codec();
		}
		else {
			video_codec = requested_video_codec;
		}
	}

	video_buffer_size = video_codec->init(image_codec_width, image_codec_height, image_codec_left_margin, image_codec_top_margin);
	if (video_buffer_size < 0) {
		Log_print("Failed to initialize %s video codec", video_codec->codec_id);
		return 0;
	}
	video_buffer = (UBYTE *)Util_malloc(video_buffer_size);

	return 1;
}

void CODECS_VIDEO_End(void)
{
	video_codec->end();
	if (video_buffer) {
		free(video_buffer);
		video_buffer_size = 0;
		video_buffer = NULL;
	}
}

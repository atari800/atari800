/*
 * audio.c - interface for audio codecs
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

/* This file is only included in compilation when sound enabled, so no need to
   do any #ifdef SOUND directives. */

#include <stdio.h>
#include <stdlib.h>
#include "cfg.h"
#include "util.h"
#include "log.h"
#include "sound.h"
#include "pokeysnd.h"

#include "codecs/audio.h"
#include "codecs/audio_pcm.h"
#include "codecs/audio_adpcm.h"
#include "codecs/audio_mulaw.h"

static AUDIO_CODEC_t *requested_audio_codec = NULL;
AUDIO_CODEC_t *audio_codec = NULL;
AUDIO_OUT_t *audio_out = NULL;
int audio_buffer_size = 0;
UBYTE *audio_buffer = NULL;

static AUDIO_CODEC_t *known_audio_codecs[] = {
	&Audio_Codec_PCM,
	&Audio_Codec_ADPCM,
	&Audio_Codec_ADPCM_MS,
	&Audio_Codec_MULAW,
	&Audio_Codec_PCM_MULAW,
	NULL,
};


static AUDIO_CODEC_t *match_audio_codec(char *id)
{
	AUDIO_CODEC_t **a = known_audio_codecs;
	AUDIO_CODEC_t *found = NULL;

	while (*a) {
		if (Util_stricmp(id, (*a)->codec_id) == 0) {
			found = *a;
			break;
		}
		a++;
	}
	return found;
}

static AUDIO_CODEC_t *get_best_audio_codec(void)
{
	return &Audio_Codec_PCM;
}

static char *audio_codec_args(char *buf)
{
	AUDIO_CODEC_t **a = known_audio_codecs;

	strcpy(buf, "\t-acodec auto");
	while (*a) {
		strcat(buf, "|");
		strcat(buf, (*a)->codec_id);
		a++;
	}
	return buf;
}


int CODECS_AUDIO_Initialise(int *argc, char *argv[])
{

	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc); /* is argument available? */
		int a_m = FALSE; /* error, argument missing! */
		int a_i = FALSE; /* error, argument invalid! */

		if (0) {}
		else if (strcmp(argv[i], "-acodec") == 0) {
			if (i_a) {
				char *mode = argv[++i];
				if (strcmp(mode, "auto") == 0) {
					requested_audio_codec = NULL; /* want best available */
				}
				else {
					requested_audio_codec = match_audio_codec(mode);
					if (!requested_audio_codec) {
						a_i = TRUE;
					}
				}
			}
			else a_m = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				char buf[256];
				Log_print(audio_codec_args(buf));
				Log_print("\t                 Select audio codec (default: auto)");
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

int CODECS_AUDIO_ReadConfig(char *string, char *ptr)
{
	if (strcmp(string, "AUDIO_CODEC") == 0) {
		if (Util_stricmp(ptr, "auto") == 0) {
			requested_audio_codec = NULL; /* want best available */
		}
		else {
			requested_audio_codec = match_audio_codec(ptr);
			if (!requested_audio_codec) {
				return FALSE;
			}
		}
	}
	else return FALSE;
	return TRUE;
}

void CODECS_AUDIO_WriteConfig(FILE *fp)
{
	if (!requested_audio_codec) {
		fprintf(fp, "AUDIO_CODEC=AUTO\n");
	}
	else {
		fprintf(fp, "AUDIO_CODEC=%s\n", requested_audio_codec->codec_id);
	}
}

int CODECS_AUDIO_Init(void)
{
	float fps;

	if (!audio_codec) {
		if (!requested_audio_codec) {
			audio_codec = get_best_audio_codec();
		}
		else {
			audio_codec = requested_audio_codec;
		}
	}

	fps = Atari800_tv_mode == Atari800_TV_PAL ? Atari800_FPS_PAL : Atari800_FPS_NTSC;
	audio_buffer_size = audio_codec->init(POKEYSND_playback_freq, fps, POKEYSND_snd_flags & POKEYSND_BIT16? 2 : 1, POKEYSND_num_pokeys);
	if (audio_buffer_size < 0) {
		Log_print("Failed to initialize %s audio codec", audio_codec->codec_id);
		return 0;
	}
	audio_buffer = (UBYTE *)Util_malloc(audio_buffer_size);
	audio_out = audio_codec->audio_out();

	return 1;
}

void CODECS_AUDIO_End(float duration)
{
	audio_codec->end(duration);
	if (audio_buffer) {
		free(audio_buffer);
		audio_buffer_size = 0;
		audio_buffer = NULL;
	}
}

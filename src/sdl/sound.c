/*
 * sdl/sound.c - SDL library specific port code - sound output
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2013 Atari800 development team (see DOC/CREDITS)
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

#include <SDL.h>

#include "atari.h"
#include "log.h"
#include "platform.h"
#include "sound.h"

static void SoundCallback(void *userdata, Uint8 *stream, int len)
{
	Sound_Callback(stream, len);
}

int PLATFORM_SoundSetup(Sound_setup_t *setup)
{
	SDL_AudioSpec desired;

	if (Sound_enabled)
		SDL_CloseAudio();
	else if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		Log_print("SDL_INIT_AUDIO FAILED: %s", SDL_GetError());
		return FALSE;
	}

	desired.freq = setup->freq;
	desired.format = setup->sample_size == 2 ? AUDIO_S16SYS : AUDIO_U8;
	desired.channels = setup->channels;

	if (setup->buffer_frames == 0)
		/* Set buffer_frames automatically. */
		setup->buffer_frames = setup->freq / 50;

	setup->buffer_frames = Sound_NextPow2(setup->buffer_frames);

	desired.samples = setup->buffer_frames;
	desired.callback = SoundCallback;
	desired.userdata = NULL;

	if (SDL_OpenAudio(&desired, NULL) < 0) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return FALSE;
	}
	setup->buffer_frames = desired.samples;

	return TRUE;
}

void PLATFORM_SoundExit(void)
{
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void PLATFORM_SoundPause(void)
{
	SDL_PauseAudio(1);
}

void PLATFORM_SoundContinue(void)
{
	SDL_PauseAudio(0);
}

void PLATFORM_SoundLock(void)
{
	SDL_LockAudio();
}

void PLATFORM_SoundUnlock(void)
{
	SDL_UnlockAudio();
}

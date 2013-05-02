/*
 * javanvm/sound.c - NestedVM-specific port code - sound output
 *
 * Copyright (c) 2001-2002 Jacek Poplawski (original atari_sdl.c)
 * Copyright (c) 2007-2008 Perry McFarlane (javanvm port)
 * Copyright (C) 2001-2008 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.

 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with Atari800; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "atari.h"
#include "pokeysnd.h"
#include "sound.h"
#include "javanvm/javanvm.h"

/* Sound */
static UBYTE *dsp_buffer = NULL;
static int line_buffer_size;
static int dsp_buffer_size;

/* These functions call the NestedVM runtime */
static int JAVANVM_InitSound(void *config)
{
	return _call_java(JAVANVM_FUN_InitSound, (int)config, 0, 0);
}

static int JAVANVM_SoundAvailable(void)
{
	return _call_java(JAVANVM_FUN_SoundAvailable, 0, 0, 0);
}

static int JAVANVM_SoundWrite(void *buffer,int len)
{
	return _call_java(JAVANVM_FUN_SoundWrite, (int)buffer, len, 0);
}

static int JAVANVM_SoundPause(void)
{
	return _call_java(JAVANVM_FUN_SoundPause, 0, 0, 0);
}

static int JAVANVM_SoundContinue(void)
{
	return _call_java(JAVANVM_FUN_SoundContinue, 0, 0, 0);
}

void Sound_Pause(void)
{
	/* stop audio output */
	JAVANVM_SoundPause();
}

void Sound_Continue(void)
{
	/* start audio output */
	JAVANVM_SoundContinue();
}

void Sound_Update(void)
{
	int avail = JAVANVM_SoundAvailable();
	while (avail > 0) {
		int len = dsp_buffer_size > avail ? avail : dsp_buffer_size;
		POKEYSND_Process(dsp_buffer, len / 2 / (POKEYSND_stereo_enabled ? 2 : 1));
		JAVANVM_SoundWrite((void *)dsp_buffer, len);
		avail -= len;
	}
}

static void SoundSetup(void)
{
	int dsprate = 48000;
	int sound_flags = 0;
	int sconfig[JAVANVM_InitSoundSIZE];
	sound_flags |= POKEYSND_BIT16;
	sconfig[JAVANVM_InitSoundSampleRate] = dsprate;
	sconfig[JAVANVM_InitSoundBitsPerSample] = 16;
	sconfig[JAVANVM_InitSoundChannels] = POKEYSND_stereo_enabled ? 2 : 1;
	sconfig[JAVANVM_InitSoundSigned] = TRUE;
	sconfig[JAVANVM_InitSoundBigEndian] = TRUE;
	line_buffer_size = JAVANVM_InitSound((void *)&sconfig[0]);
	dsp_buffer_size = 4096; /*adjust this to fix skipping/latency*/
	if (POKEYSND_stereo_enabled) dsp_buffer_size *= 2;
	if (line_buffer_size < dsp_buffer_size) dsp_buffer_size = line_buffer_size;
	free(dsp_buffer);
	dsp_buffer = (UBYTE*)malloc(dsp_buffer_size);
	POKEYSND_Init(POKEYSND_FREQ_17_EXACT, dsprate, (POKEYSND_stereo_enabled ? 2 : 1) , sound_flags);
}

void Sound_Reinit(void)
{
	SoundSetup();
}

int Sound_Initialise(int *argc, char *argv[])
{
	int i;
	int help_only = FALSE;
	for (i = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-help") == 0)
			help_only = TRUE;
	}

	if (!help_only)
		SoundSetup();

	return TRUE;
}

void Sound_Exit(void)
{
}

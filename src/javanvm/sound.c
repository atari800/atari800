/*
 * javanvm/sound.c - NestedVM-specific port code - sound output
 *
 * Copyright (c) 2001-2002 Jacek Poplawski (original atari_sdl.c)
 * Copyright (c) 2007-2008 Perry McFarlane (javanvm port)
 * Copyright (C) 2001-2013 Atari800 development team (see DOC/CREDITS)
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
#include "platform.h"
#include "sound.h"
#include "javanvm/javanvm.h"

/* These functions call the NestedVM runtime */
static int JAVANVM_InitSound(void *config)
{
	return _call_java(JAVANVM_FUN_InitSound, (int)config, 0, 0);
}

static int JAVANVM_SoundExit(void)
{
	return _call_java(JAVANVM_FUN_SoundExit, 0, 0, 0);
}

static int JAVANVM_SoundAvailable(void)
{
	return _call_java(JAVANVM_FUN_SoundAvailable, 0, 0, 0);
}

static int JAVANVM_SoundWrite(void const *buffer,int len)
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

int PLATFORM_SoundSetup(Sound_setup_t *setup)
{
	int sconfig[JAVANVM_InitSoundSIZE];
	int hw_buffer_size;

	if (setup->buffer_frames == 0)
		/* Set buffer_frames automatically. */
		setup->buffer_frames = Sound_NextPow2(setup->freq * 4 / 50);

	sconfig[JAVANVM_InitSoundSampleRate] = setup->freq;
	sconfig[JAVANVM_InitSoundBitsPerSample] = setup->sample_size * 8;
	sconfig[JAVANVM_InitSoundChannels] = setup->channels;
	sconfig[JAVANVM_InitSoundSigned] = setup->sample_size == 2;
	sconfig[JAVANVM_InitSoundBigEndian] = TRUE;
	sconfig[JAVANVM_InitSoundBufferSize] = setup->buffer_frames * setup->sample_size * setup->channels;
	hw_buffer_size = JAVANVM_InitSound((void *)sconfig);
	if (hw_buffer_size == 0)
		return FALSE;
	setup->buffer_frames = hw_buffer_size / setup->sample_size / setup->channels;

	return TRUE;
}

void PLATFORM_SoundExit(void)
{
	JAVANVM_SoundExit();
}

void PLATFORM_SoundPause(void)
{
	/* stop audio output */
	JAVANVM_SoundPause();
}

void PLATFORM_SoundContinue(void)
{
	/* start audio output */
	JAVANVM_SoundContinue();
}

unsigned int PLATFORM_SoundAvailable(void)
{
	return JAVANVM_SoundAvailable();
}

void PLATFORM_SoundWrite(UBYTE const *buffer, unsigned int size)
{
	JAVANVM_SoundWrite(buffer, size);
}

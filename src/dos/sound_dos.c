/*
 * sound_dos.c - high level sound routines for DOS port
 *
 * Copyright (c) 1998-2000 Matthew Conte
 * Copyright (c) 2000-2014 Atari800 development team (see DOC/CREDITS)
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

#include "dos_sb.h"

#include "atari.h"
#include "log.h"
#include "platform.h"
#include "sound.h"
#include "util.h" /* TODO */

int PLATFORM_SoundSetup(Sound_setup_t *setup)
{
	int playback_freq = setup->freq;
	int bps = setup->sample_size * 8;
	int buffer_samples;
	int stereo = setup->channels == 2;

	if (setup->buffer_frames == 0)
		/* Set buffer_frames automatically. */
		setup->buffer_frames = Sound_NextPow2(setup->freq / 50);

	buffer_samples = setup->buffer_frames * setup->channels;

	if (sb_init(&playback_freq, &bps, &buffer_samples, &stereo) < 0) {
		Log_print("Cannot init sound card");
		return FALSE;
	}

	setup->channels = stereo ? 2 : 1;
	setup->sample_size = bps / 8;
	setup->buffer_frames = buffer_samples / setup->channels;
	setup->freq = playback_freq;

	return TRUE;
}

void PLATFORM_SoundExit(void)
{
	sb_shutdown();
}

void PLATFORM_SoundPause(void)
{
	sb_stopoutput();
}

void PLATFORM_SoundContinue(void)
{
	sb_startoutput((sbmix_t) Sound_Callback);
}

void PLATFORM_SoundLock(void)
{
}

void PLATFORM_SoundUnlock(void)
{
}

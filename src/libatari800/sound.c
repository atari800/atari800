/*
 * libatari800/sound.c - Atari800 as a library - sound output
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2013 Atari800 development team (see DOC/CREDITS)
 * Copyright (c) 2016-2019 Rob McMullen
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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "atari.h"
#include "log.h"
#include "platform.h"
#include "init.h"
#include "sound.h"
#include "util.h"

UBYTE *LIBATARI800_Sound_array;

unsigned int sound_array_fill = 0;

unsigned int sound_hw_buffer_size = 0;

/* difference between an integer sample rate and the floating point sample rate, used
   keep track of which frames need to drop a sample to stay at the constant audio
   sampling rate */
static double sample_diff;

double sample_residual;

int PLATFORM_SoundSetup(Sound_setup_t *setup)
{
	double refresh_rate;
	double samples_per_video_frame;

	refresh_rate = Atari800_tv_mode == Atari800_TV_PAL ? Atari800_FPS_PAL : Atari800_FPS_NTSC;
	samples_per_video_frame = setup->freq / refresh_rate;
	setup->buffer_frames = (int)(ceil(samples_per_video_frame));

	sound_hw_buffer_size = setup->buffer_frames * setup->sample_size * setup->channels;
	if (sound_hw_buffer_size == 0)
	        return FALSE;

	LIBATARI800_Sound_array = Util_malloc(sound_hw_buffer_size);

	sample_diff = (double)setup->buffer_frames - samples_per_video_frame;
	sample_residual = 0;

	return TRUE;
}

void PLATFORM_SoundExit(void)
{
	free(LIBATARI800_Sound_array);
}

void PLATFORM_SoundPause(void)
{
}

void PLATFORM_SoundContinue(void)
{
}

/* Called just before audio buffer is filled; used to initialize sound parameters */
unsigned int PLATFORM_SoundAvailable(void)
{
	int buf_size = sound_hw_buffer_size;

	/* Because the frame rate is not an integer (59.92 NTSC, 49.86 PAL), the sample
	   rate will not be constant. For example, on NTSC with a sample rate of 44100Hz,
	   there should be 735.9476... samples per second. But, obviously there can't be
	   a fraction of a sample, so sample sizes will mostly be 736 samples with the
	   occasional 735 thrown in to make the rate average to 44100Hz. The sample_residual
	   keeps track of the fractional samples to see when we must shorten the sample
	   buffer. */
	sample_residual += sample_diff;
	if (sample_residual > 1.0) {
		sample_residual -= 1.0;
		buf_size -= Sound_out.sample_size * Sound_out.channels;
	}

	sound_array_fill = 0;
	return buf_size;
}

void PLATFORM_SoundWrite(UBYTE const *buffer, unsigned int size)
{
	memcpy(LIBATARI800_Sound_array, buffer, size);
	sound_array_fill = size;
}

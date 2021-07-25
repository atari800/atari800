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

#include "atari.h"
#include "log.h"
#include "platform.h"
#include "init.h"
#include "sound.h"
#include "util.h"

UBYTE *LIBATARI800_Sound_array;

unsigned int sound_array_fill = 0;

static unsigned int hw_buffer_size = 0;

int PLATFORM_SoundSetup(Sound_setup_t *setup)
{
	int refresh_rate;

	if (setup->buffer_frames == 0) {
	    refresh_rate = Atari800_tv_mode == Atari800_TV_PAL ? 50 : 60;
		setup->buffer_frames = setup->freq / refresh_rate;
	}

	hw_buffer_size = setup->buffer_frames * setup->sample_size * setup->channels;
	if (hw_buffer_size == 0)
	        return FALSE;

	LIBATARI800_Sound_array = Util_malloc(hw_buffer_size);

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
	sound_array_fill = 0;
	return hw_buffer_size;
}

void PLATFORM_SoundWrite(UBYTE const *buffer, unsigned int size)
{
	memcpy(LIBATARI800_Sound_array + sound_array_fill, buffer, size);
	sound_array_fill += size;
}

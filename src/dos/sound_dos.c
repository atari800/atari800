/*
 * sound_dos.c - high level sound routines for DOS port
 *
 * Copyright (c) 1998-2000 Matthew Conte
 * Copyright (c) 2000-2005 Atari800 development team (see DOC/CREDITS)
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
#include <string.h>		/* for strcmp */

#ifdef SOUND

#include "atari.h"
#include "log.h"
#include "pokeysnd.h"
#include "util.h"
#include "sound.h"

#include "dos_sb.h"

static int sound_enabled = TRUE;

static int playback_freq = FREQ_17_APPROX / 28 / 3;
#ifdef STEREO_SOUND
static int buffersize = 880;
static int stereo = TRUE;
#else
static int buffersize = 440;
static int stereo = FALSE;
#endif
static int bps = 8;

void Sound_Initialise(int *argc, char *argv[])
{
	int i, j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		else if (strcmp(argv[i], "-dsprate") == 0)
			playback_freq = Util_sscandec(argv[++i]);
		else if (strcmp(argv[i], "-bufsize") == 0)
			buffersize = Util_sscandec(argv[++i]);
		else {
			if (strcmp(argv[i], "-help") == 0) {
				sound_enabled = FALSE;
				Aprint("\t-sound           Enable sound");
				Aprint("\t-nosound         Disable sound");
				Aprint("\t-dsprate <freq>  Set mixing frequency (Hz)");
				Aprint("\t-bufsize         Set sound buffer size");
			}
			argv[j++] = argv[i];
		}
	}

	*argc = j;

	if (sound_enabled) {
		if (sb_init(&playback_freq, &bps, &buffersize, &stereo) < 0) {
			Aprint("Cannot init sound card");
			sound_enabled = FALSE;
		}
		else {
			Pokey_sound_init(FREQ_17_APPROX, playback_freq, stereo ? 2 : 1, 0);
			sb_startoutput((sbmix_t) Pokey_process);
		}
	}
}

void Sound_Pause(void)
{
	if (sound_enabled)
		sb_stopoutput();
}

void Sound_Continue(void)
{
	if (sound_enabled)
		sb_startoutput((sbmix_t) Pokey_process);
}

void Sound_Update(void)
{
}

void Sound_Exit(void)
{
	if (sound_enabled)
		sb_shutdown();
}

#endif /* SOUND */

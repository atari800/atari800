/*
 * sound_dos.c - high level sound routines for DOS port
 *
 * Copyright (c) 1998-2000 Matthew Conte
 * Copyright (c) 2000-2003 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>		/* for sscanf */
#include "config.h"

#ifdef SOUND

#include "pokeysnd.h"
#include "dos_sb.h"
#include "log.h"

#define FALSE 0
#define TRUE 1

static int sound_enabled = TRUE;

int playback_freq = FREQ_17_APPROX / 28 / 3;
int buffersize = 440;
int stereo = FALSE;
int bps = 8;

void Sound_Initialise(int *argc, char *argv[])
{
	int i, j;

#ifdef STEREO
	stereo = TRUE;
#endif

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		else if (strcmp(argv[i], "-dsprate") == 0)
			sscanf(argv[++i], "%d", &playback_freq);
		else if (strcmp(argv[i], "-bufsize") == 0)
			sscanf(argv[++i], "%d", &buffersize);
		else
			argv[j++] = argv[i];
	}

	*argc = j;

	if (sound_enabled) {
		if (sb_init(&playback_freq, &bps, &buffersize, &stereo) < 0) {
			Aprint("Cannot init sound card");
			sound_enabled = FALSE;
		}
		else {

#ifdef STEREO
			Pokey_sound_init(FREQ_17_APPROX, playback_freq, 2, 0);
#else
			Pokey_sound_init(FREQ_17_APPROX, playback_freq, 1, 0);
#endif
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

/*
 * sound.c - Unix (and Linux) specific code
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
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

#include "config.h"

#ifdef SOUND

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "atari.h"
#include "log.h"
#include "pokeysnd.h"
#include "sndsave.h"
#include "util.h"

#define FRAGSIZE	7

#define DEFDSPRATE 22050

static const char *dspname = "/dev/dsp";
static int dsprate = DEFDSPRATE;
static int fragstofill = 0;
static int snddelay = 60;		/* delay in milliseconds */

static int sound_enabled = TRUE;
static int dsp_fd;

void Sound_Initialise(int *argc, char *argv[])
{
	int i, j;
	struct audio_buf_info abi;
	int help_only = FALSE;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		else if (strcmp(argv[i], "-dsprate") == 0)
			dsprate = Util_sscandec(argv[++i]);
		else if (strcmp(argv[i], "-snddelay") == 0)
			snddelay = Util_sscandec(argv[++i]);
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Aprint("\t-sound           Enable sound\n"
				       "\t-nosound         Disable sound\n"
				       "\t-dsprate <rate>  Set DSP rate in Hz\n"
				       "\t-snddelay <ms>   Set sound delay in milliseconds"
				      );
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	if (help_only)
		return;

	if (sound_enabled) {
		if ((dsp_fd = open(dspname, O_WRONLY
#ifdef __linux__
			| O_NONBLOCK
#endif
		)) == -1) {
			perror(dspname);
			sound_enabled = 0;
			return;
		}
#ifdef __linux__
		if (fcntl(dsp_fd, F_SETFL, 0) < 0) {
			Aprint("%s: Can't make filedescriptor blocking", dspname);
			close(dsp_fd);
			sound_enabled = 0;
			return;
		}
#endif
		i = AFMT_U8;
		if (ioctl(dsp_fd, SNDCTL_DSP_SETFMT, &i)) {
			Aprint("%s: cannot set 8-bit sample", dspname);
			close(dsp_fd);
			sound_enabled = 0;
			return;
		}
#ifdef STEREO_SOUND
		i = 1;
		if (ioctl(dsp_fd, SNDCTL_DSP_STEREO, &i)) {
			Aprint("%s: cannot set stereo", dspname);
			close(dsp_fd);
			sound_enabled = 0;
			return;
		}
#endif
		if (ioctl(dsp_fd, SNDCTL_DSP_SPEED, &dsprate)) {
			Aprint("%s: cannot set %d speed", dspname, dsprate);
			close(dsp_fd);
			sound_enabled = 0;
			return;
		}

		fragstofill = ((dsprate * snddelay / 1000) >> FRAGSIZE) + 1;
		if (fragstofill > 100)
			fragstofill = 100;

		/* fragments of size 2^FRAGSIZE bytes */
		i = ((fragstofill + 1) << 16) | FRAGSIZE;
		if (ioctl(dsp_fd, SNDCTL_DSP_SETFRAGMENT, &i)) {
			Aprint("%s: cannot set fragments", dspname);
			close(dsp_fd);
			sound_enabled = 0;
			return;
		}

		if (ioctl(dsp_fd, SNDCTL_DSP_GETOSPACE, &abi)) {
			Aprint("%s: unable to get output space", dspname);
			close(dsp_fd);
			sound_enabled = 0;
			return;
		}

#ifdef STEREO_SOUND
		Pokey_sound_init(FREQ_17_EXACT, dsprate, 2, 0);
#else
		Pokey_sound_init(FREQ_17_EXACT, dsprate, 1, 0);
#endif
	}
}

void Sound_Pause(void)
{
	if (sound_enabled) {
		/* stop audio output */
	}
}

void Sound_Continue(void)
{
	if (sound_enabled) {
		/* start audio output */
	}
}

void Sound_Exit(void)
{
	if (sound_enabled)
		close(dsp_fd);
}

void Sound_Update(void)
{
	int i;
	struct audio_buf_info abi;
	char dsp_buffer[1 << FRAGSIZE];

	if (!sound_enabled)
		return;

	if (ioctl(dsp_fd, SNDCTL_DSP_GETOSPACE, &abi))
		return;

	i = (abi.fragstotal * abi.fragsize - abi.bytes) >> FRAGSIZE;

	/* we need fragstofill fragments to be filled */
#ifdef STEREO_SOUND
	for (; i < fragstofill * 2; i++) {
#else
	for (; i < fragstofill; i++) {
#endif
		Pokey_process(dsp_buffer, sizeof(dsp_buffer));
#if 1
#warning Please report whether sound is good to the Atari800 mailing list.
		/* For some unknown reason, this is needed
		   on my Red Hat 9, VT82C686 AC97 Audio Controller.
		   Not that the sound is just a bit silent without this,
		   it is totally broken. */
		{
			int j;
			for (j = 0; j < sizeof(dsp_buffer); j++)
				dsp_buffer[j] <<= 1;
		}
#endif
		write(dsp_fd, dsp_buffer, sizeof(dsp_buffer));
	}
}
#endif	/* SOUND */

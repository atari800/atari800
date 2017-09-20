/*
 * sound.c - high-level sound routines for the Atari Falcon port
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"

#ifdef SOUND
#include <stdlib.h>
#include <stdio.h>

#include <mint/cookie.h>
#include <mint/falcon.h>
#include <mint/osbind.h>

#include "atari.h"
#include "log.h"
#include "pokeysnd.h"
#include "sound.h"
#include "util.h"

static char *dsp_buffer1, *dsp_endbuf1;
static char *dsp_buffer2, *dsp_endbuf2;

static int sound_enabled = TRUE;
static int stereo_enabled = FALSE;

#define RATE12KHZ		12517
#define RATE25KHZ		25033
#define RATE50KHZ		50066

#define SNDBUFSIZE		600

static int dsprate = RATE12KHZ;
static int sndbufsize = SNDBUFSIZE;

#define IVECTOR		*(volatile UBYTE *)0xFFFA17

volatile short *DMActrlptr = (volatile short *) 0xff8900;

void SetBuffer(long bufbeg, long bufsize)
{
	long bufend = bufbeg + bufsize;
	DMActrlptr[1] = (bufbeg >> 16) & 0xff;
	DMActrlptr[2] = (bufbeg >> 8) & 0xff;
	DMActrlptr[3] = bufbeg & 0xff;
	DMActrlptr[7] = (bufend >> 16) & 0xff;
	DMActrlptr[8] = (bufend >> 8) & 0xff;
	DMActrlptr[9] = bufend & 0xff;
}

void Sound_Update(void)
{
	/* dunno what to do here as the sound precomputing is interrupt driven */
}

static void __attribute__((interrupt)) timer_A( void )
{
	static int first = FALSE;	/* start computing second buffer */

	if (first) {
		SetBuffer((long)dsp_buffer1, sndbufsize);		/* set next DMA buffer */
		POKEYSND_Process(dsp_buffer1, sndbufsize);		/* quickly compute it */
		first = FALSE;
	}
	else {
		SetBuffer((long)dsp_buffer2, sndbufsize);
		POKEYSND_Process(dsp_buffer2, sndbufsize);
		first = TRUE;
	}
}

void MFP_IRQ_on(void)
{
	SetBuffer((long)dsp_buffer1, sndbufsize);		/* start playing first buffer */
	short mono = stereo_enabled ? 0x00 : 0x80;

	if (dsprate == RATE25KHZ)
		DMActrlptr[0x10] = mono | 2;	/* mono/stereo 25 kHz */
	else if (dsprate == RATE50KHZ)
		DMActrlptr[0x10] = mono | 3;	/* mono/stereo 50 kHz */
	else
		DMActrlptr[0x10] = mono | 1;	/* mono/stereo 12 kHz */

	DMActrlptr[0] = 0x400 | 3;	/* play until stopped, interrupt at end of frame */

	Mfpint(MFP_TIMERA, timer_A);
	Xbtimer(XB_TIMERA, 1<<3, 1, timer_A);	/* event count mode, interrupt after 1st frame */
	IVECTOR &= ~(1 << 3);		/* turn on AEO */
	Jenabint(MFP_TIMERA);
}

void MFP_IRQ_off(void)
{
	Jdisint(MFP_TIMERA);
	DMActrlptr[0] = 0;			/* stop playing */
}

int Sound_Initialise(int *argc, char *argv[])
{
	int i, j;
	int help_only = FALSE;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc);		/* is argument available? */
		int a_m = FALSE;			/* error, argument missing! */

		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		else if (strcmp(argv[i], "-dsprate") == 0) {
			if (i_a) {
				dsprate = Util_sscandec(argv[++i]);
				if (dsprate == RATE50KHZ)
					sndbufsize = 4*SNDBUFSIZE;
				else if (dsprate == RATE25KHZ)
					sndbufsize = 2*SNDBUFSIZE;
				else {
					dsprate = RATE12KHZ;
					sndbufsize = SNDBUFSIZE;
				}
			}
			else a_m = TRUE;
		}
#ifdef STEREO_SOUND
		else if (strcmp(argv[i], "-stereo") == 0) {
			stereo_enabled = TRUE;
		}
		else if (strcmp(argv[i], "-nostereo") == 0) {
			stereo_enabled = FALSE;
		}
#endif
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-sound           Enable sound\n"
				       "\t-nosound         Disable sound\n"
#ifdef STEREO_SOUND
					   "\t-stereo          Enable stereo sound\n"
					   "\t-nostereo        Disable stereo sound\n"
#endif
				       "\t-dsprate <rate>  Set sample rate in Hz"
				      );
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			sound_enabled = FALSE;
			return FALSE;
		}
	}

	*argc = j;

	if (help_only) {
		sound_enabled = FALSE;
		return TRUE;
	}

	/* test of Sound hardware availability */
	if (sound_enabled) {
		long val;

		if (Getcookie(C__SND, &val) == C_FOUND) {
			if (!(val & SND_8BIT))
				sound_enabled = FALSE;	/* Sound DMA hardware is missing */
		}
		else
			sound_enabled = FALSE;	/* CookieJar is missing */
	}

	if (sound_enabled) {
		if (stereo_enabled)
			sndbufsize *= 2;

		dsp_buffer1 = (char *) Mxalloc(2 * sndbufsize, MX_STRAM);
		if (!dsp_buffer1) {
			printf("can't allocate sound buffer\n");
			exit(1);
		}
		dsp_buffer2 = dsp_endbuf1 = dsp_buffer1 + sndbufsize;
		dsp_endbuf2 = dsp_buffer2 + sndbufsize;
		memset(dsp_buffer1, 0, sndbufsize);
		memset(dsp_buffer2, 127, sndbufsize);

		POKEYSND_Init(POKEYSND_FREQ_17_EXACT, dsprate, stereo_enabled ? 2 : 1, 0);

		Supexec(MFP_IRQ_on);
	}
	return TRUE;
}

void Sound_Exit(void)
{
	if (sound_enabled) {
		Supexec(MFP_IRQ_off);
		Mfree(dsp_buffer1);
	}
}

#endif
void Sound_Pause(void)
{
#ifdef SOUND
	if (sound_enabled)
		Supexec(MFP_IRQ_off);
#endif
}

void Sound_Continue(void)
{
#ifdef SOUND
	if (sound_enabled)
		Supexec(MFP_IRQ_on);
#endif
}


/*
 * sound.c - high-level sound routines for the Atari Falcon port
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2017 Atari800 development team (see DOC/CREDITS)
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

#include <mint/falcon.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>

#include <stdlib.h>
#include <string.h>

/* https://github.com/mikrosk/atari_sound_setup */
#include <usound.h>

#include "platform.h"
#include "sound.h"

static char* pPhysical;
static char* pLogical;
static char* pBuffer;
static size_t bufferSize;
static int isLogicalBufferActive;
static SndBufPtr soundBufferPtr;
static size_t soundBufferWritten;

unsigned int PLATFORM_SoundAvailable(void)
{
	if (Buffptr(&soundBufferPtr) == 0) {
		if (!isLogicalBufferActive) {
			/* we play from pPhysical (1st buffer) */
			if (soundBufferPtr.play < pLogical) {
				return bufferSize - soundBufferWritten;
			}
		} else {
			/* we play from pLogical (2nd buffer) */
			if (soundBufferPtr.play >= pLogical) {
				return bufferSize - soundBufferWritten;
			}
		}
	}

	return 0;
}

void PLATFORM_SoundWrite(UBYTE const *buffer, unsigned int size)
{
	int written = FALSE;

	if (Buffptr(&soundBufferPtr) == 0) {
		if (!isLogicalBufferActive) {
			/* we play from pPhysical (1st buffer) */
			if (soundBufferPtr.play < pLogical) {
				memcpy(pLogical + soundBufferWritten, buffer, size);
				written = TRUE;
			}
		} else {
			/* we play from pLogical (2nd buffer) */
			if (soundBufferPtr.play >= pLogical) {
				memcpy(pPhysical + soundBufferWritten, buffer, size);
				written = TRUE;
			}
		}
	}

	if (written) {
		if (soundBufferWritten + size < bufferSize) {
			soundBufferWritten += size;
		} else {
			soundBufferWritten = 0;
			isLogicalBufferActive = !isLogicalBufferActive;
		}
	}
}

int PLATFORM_SoundSetup(Sound_setup_t *setup)
{
	AudioSpec desired, obtained;

	if (Sound_enabled) {
		PLATFORM_SoundExit();
	}

	/*
	 * Sound_Setup():
	 *	- calculates setup->buffer_frames which may be NULL if setup->buffer_ms == 0
	 *	- calls PLATFORM_SoundSetup(setup)
	 *	- calculates setup->buffer_ms from setup->buffer_frames and setup->freq on success
	 */
	desired.frequency = setup->freq;
	desired.channels  = setup->channels;
	desired.format    = setup->sample_size == 1 ? AudioFormatSigned8 : AudioFormatSigned16MSB;
	desired.samples   = Sound_NextPow2(setup->buffer_frames == 0
		? setup->freq / 50	/* buffer for at least 1/50th of a second */
		: setup->buffer_frames);

	if (!AtariSoundSetupInitXbios(&desired, &obtained)) {
		return FALSE;
	}

	setup->freq          = obtained.frequency;
	setup->channels      = obtained.channels;
	setup->sample_size   = (obtained.format == AudioFormatSigned8 || obtained.format == AudioFormatUnsigned8) ? 1 : 2;
	setup->buffer_frames = desired.samples;	/* stick with requested */

	/* channels * 8/16 bit * freq in Hz * seconds */
	bufferSize = setup->channels * setup->sample_size * setup->buffer_frames;

	pBuffer = (char*)Mxalloc(2*bufferSize, MX_STRAM);
	if (pBuffer == NULL) {
		goto malloc_failed;
	}
	memset(pBuffer, 0, 2*bufferSize);

	pPhysical = pBuffer;
	pLogical = pBuffer + bufferSize;

	if (Setbuffer(SR_PLAY, pBuffer, pBuffer + 2*bufferSize) != 0) {
		goto set_buffer_failed;
	}

	return TRUE;

set_buffer_failed:
	Mfree(pBuffer);
	pBuffer = NULL;
malloc_failed:
	AtariSoundSetupDeinitXbios();

	return FALSE;
}

void PLATFORM_SoundExit(void)
{
	Buffoper(0x00);

	if (pBuffer != NULL) {
		Mfree(pBuffer);
		pBuffer = NULL;
	}

	AtariSoundSetupDeinitXbios();
}

void PLATFORM_SoundPause(void)
{
	Buffoper(0x00);
}

void PLATFORM_SoundContinue(void)
{
	Buffoper(SB_PLA_ENA | SB_PLA_RPT);
}

#endif /* SOUND */

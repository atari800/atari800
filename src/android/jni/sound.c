/*
 * sound.c - android sound
 *
 * Copyright (C) 2010 Kostas Nakos
 * Copyright (C) 2010 Atari800 development team (see DOC/CREDITS)
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

#include "pokeysnd.h"
#include "log.h"

static int sixteenbit = 0;

void Sound_Initialise(int *argc, char *argv[])
{
}

void Android_SoundInit(int rate, int bit16, int hq)
{
	POKEYSND_bienias_fix = 0;
	POKEYSND_enable_new_pokey = hq;
	sixteenbit = bit16;
	POKEYSND_Init(POKEYSND_FREQ_17_EXACT, rate, 1, bit16 ? POKEYSND_BIT16 : 0);
}

void Sound_Exit(void)
{
}

void Sound_Update(void)
{
}

void Sound_Pause(void)
{
}

void Sound_Continue(void)
{
}

void SoundThread_Update(void *buf, int offs, int len)
{
	POKEYSND_Process(buf + offs, len >> sixteenbit);
}

/*
 * atari_basic.c - Text-only specific port code
 *
 * Copyright (c) 1995-1998 David Firth
 * Copyright (c) 1998-2005 Atari800 development team (see DOC/CREDITS)
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

#include "atari.h"
#include "config.h"
#include "input.h"
#include "monitor.h"
#include "log.h"

void Atari_Initialise(int *argc, char *argv[])
{
}

int Atari_Exit(int run_monitor)
{
	if (run_monitor)
		return monitor();

	Aflushlog();
	return FALSE;
}

int Atari_Keyboard(void)
{
	return AKEY_NONE;
}

int Atari_PORT(int num)
{
	return 0xff;
}

int Atari_TRIG(int num)
{
	return 1;
}

int main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	while (TRUE) {
		Atari800_Frame();
	}
}

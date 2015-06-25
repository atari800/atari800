/*
 * javanvm/main.c - NestedVM-specific port code - main interface
 *
 * Copyright (c) 2001-2002 Jacek Poplawski (original atari_sdl.c)
 * Copyright (c) 2007-2008 Perry McFarlane (javanvm port)
 * Copyright (C) 2001-2008 Atari800 development team (see DOC/CREDITS)
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Atari800 includes */
#include "../input.h"
#include "monitor.h"
#include "platform.h"
#include "sound.h"
#include "javanvm/javanvm.h"

/* These functions call the NestedVM runtime */
static int JAVANVM_Sleep(int millis)
{
	return _call_java(JAVANVM_FUN_Sleep, millis, 0, 0);
}

static int JAVANVM_CheckThreadStatus(void)
{
	return _call_java(JAVANVM_FUN_CheckThreadStatus, 0, 0, 0);
}

int PLATFORM_Initialise(int *argc, char *argv[])
{
	if (!JAVANVM_VIDEO_Initialise(argc, argv)
#ifdef SOUND
	    || !Sound_Initialise(argc, argv)
#endif
	    || !JAVANVM_INPUT_Initialise(argc, argv))
		return FALSE;

	return TRUE;
}

int PLATFORM_Exit(int run_monitor){
	int restart;
	if (run_monitor) {
#ifdef SOUND
		Sound_Pause();
#endif
		restart = MONITOR_Run();
#ifdef SOUND
		Sound_Continue();
#endif
	}
   	else {
		restart = FALSE;
	}
	if (restart) {
		return 1;
	}
	return restart;
}

void PLATFORM_Sleep(double s){
	JAVANVM_Sleep((int)(s*1e3));
}

int main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	for (;;) {
		INPUT_key_code = PLATFORM_Keyboard();
		Atari800_Frame();
		if (Atari800_display_screen)
			PLATFORM_DisplayScreen();
		if (JAVANVM_CheckThreadStatus()) {
		   	Atari800_Exit(FALSE);
			exit(0);
		}
	}
}

/*
vim:ts=4:sw=4:
*/

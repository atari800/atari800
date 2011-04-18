/*
 * sdl/main.c - SDL library specific port code - main interface
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2009 Atari800 development team (see DOC/CREDITS)
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

/*
   Thanks to David Olofson for scaling tips!

   TODO:
   - implement all Atari800 parameters
   - turn off fullscreen when error happen
*/

#include "config.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

/* Atari800 includes */
#include "atari.h"
#include "../input.h"
#include "log.h"
#include "monitor.h"
#include "platform.h"
#ifdef SOUND
#include "../sound.h"
#include "sdl/sound.h"
#endif
#include "videomode.h"
#include "sdl/video.h"
#include "sdl/input.h"

int PLATFORM_Configure(char *option, char *parameters)
{
	return SDL_VIDEO_ReadConfig(option, parameters) ||
	       SDL_INPUT_ReadConfig(option, parameters);
}

void PLATFORM_ConfigSave(FILE *fp)
{
	SDL_VIDEO_WriteConfig(fp);
	SDL_INPUT_WriteConfig(fp);
}

int PLATFORM_Initialise(int *argc, char *argv[])
{
	int i, j;
	int help_only = FALSE;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-help") == 0) {
			help_only = TRUE;
		}
		argv[j++] = argv[i];
	}
	*argc = j;

	if (!help_only) {
		i = SDL_INIT_JOYSTICK;
#ifdef SOUND
		i |= SDL_INIT_AUDIO;
#endif
#ifdef HAVE_WINDOWS_H
		/* Windows SDL version 1.2.10+ uses windib as the default, but it is slower.
		   Additionally on some graphic cards it doesn't work properly in low
		   resolutions like Atari800's default of 336x240. */
		if (SDL_getenv("SDL_VIDEODRIVER")==NULL)
			SDL_putenv("SDL_VIDEODRIVER=directx");
#endif

		if (SDL_Init(i) != 0) {
			Log_print("SDL_Init FAILED: %s", SDL_GetError());
			Log_flushlog();
			exit(-1);
		}
		atexit(SDL_Quit);
	}

	if (!SDL_VIDEO_Initialise(argc, argv)
#ifdef SOUND
	    || !SDL_SOUND_Initialise(argc, argv)
#endif
	    || !SDL_INPUT_Initialise(argc, argv))
		return FALSE;

	return TRUE;
}

int PLATFORM_Exit(int run_monitor)
{
	int restart;

	SDL_INPUT_Exit();
	/* With SDL_VIDEODRIVER=directs keyboard presses in console are still
	   fetched by the SDL window. This results in locked Return key when
	   the user leaves the monitor. The only solution is to completely
	   close the video subsystem for the time of running the monitor. */
	SDL_VIDEO_Exit();
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
		/* Reinitialise the SDL subsystem. */
		SDL_VIDEO_InitSDL();
		SDL_INPUT_Restart();
		/* This call reopens the SDL window. */
		VIDEOMODE_Update();
		return 1;
	}

	SDL_Quit();

	Log_flushlog();

	return restart;
}

#if HAVE_WINDOWS_H
static BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		/* Perform a normal exit. */
		Atari800_Exit(FALSE);
	case CTRL_C_EVENT:
		/* Ctrl+C is handled in atari.c */
		return TRUE;
	default:
		return FALSE;
	}
}
#endif /* HAVE_WINDOWS_H */

int main(int argc, char **argv)
{
#if HAVE_WINDOWS_H
	/* Handle Windows console signals myself. If not, then closing
	   the console window would cause emulator crash due to the sound
	   subsystem being active. */
	if(!SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlHandler, TRUE)) {
		Log_print("ERROR: Could not set console control handler");
		return 1;
	}
#endif /* HAVE_WINDOWS_H */

	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	for (;;) {
		INPUT_key_code = PLATFORM_Keyboard();
		SDL_INPUT_Mouse();
		Atari800_Frame();
		if (Atari800_display_screen)
			PLATFORM_DisplayScreen();
	}
}

/*
vim:ts=4:sw=4:
*/

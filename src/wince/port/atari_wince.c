/*
 * atari_wince.c - WinCE port specific code
 *
 * Copyright (C) 2001 Vasyl Tsvirkunov
 * Copyright (C) 2001-2005 Atari800 development team (see DOC/CREDITS)
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

/* Based on Win32 port by  Krzysztof Nikiel */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "platform.h"
#include "atari.h"
#include "input.h"
#include "screen.h"
#include "keyboard.h"
#include "main.h"
#include "sound.h"
#include "diskled.h"
#include "rt-config.h"
#include "ui.h"
#include "screen.h"

static int usesnd = 1;

static int kbjoy = 1;

/* WinCE port does not have console, no log either */
void Aprint(char *format, ... ) {}
void Aflushlog(void) {}


int Atari_Keyboard(void)
{
	int keycode;
	
	prockb();

	keycode = get_last_key();

/*
	if(keycode == AKEY_TAB)
	{
		gr_suspend();
		MessageBox(hWndMain, TEXT("Test"), TEXT("Test"), MB_APPLMODAL|MB_OK);
		gr_resume();
		clearkb();
	}
*/
	return keycode;
}

void Atari_Initialise(int *argc, char *argv[])
{
#ifdef SOUND
	if (usesnd)
		Sound_Initialise(argc, argv);
#endif
	if(initinput())
	{
		perror("Input initialization failed");
		exit(1);
	}
	if (gron(argc, argv))
	{
		perror("Graphics initialization failed");
		exit(1);
	}
	
	clearkb();
}

int Atari_Exit(int run_monitor)
{
	/* monitor is not avaliable in this port */
	if(run_monitor)
		return 1;
	
#ifdef BUFFERED_LOG
	Aflushlog();
#endif
	
	uninitinput();
	groff();
	
#ifdef SOUND
	Sound_Exit();
#endif
	
	return 0;
}

void Atari_DisplayScreen(UBYTE * ascreen)
{
	refreshv(ascreen + 24);
}

int Atari_PORT(int num)
{
	if (num == 0)
		return stick0;
	else
		return 0xff;
}


int Atari_TRIG(int num)
{
	if (num == 0)
		return trig0;
	else
		return 1;
}

int wince_main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	while(1)
	{
		static int test_val = 0;
		int keycode;

		keycode = Atari_Keyboard();

		switch (keycode) {
		case AKEY_COLDSTART:
			Coldstart();
			break;
		case AKEY_WARMSTART:
			Warmstart();
			break;
		case AKEY_EXIT:
			Atari800_Exit(FALSE);
			exit(1);
		case AKEY_UI:
#ifdef SOUND
			Sound_Pause();
#endif
			ui((UBYTE *)atari_screen);
#ifdef SOUND
			Sound_Continue();
#endif
			break;
		case AKEY_SCREENSHOT:
			Screen_SaveNextScreenshot(FALSE);
			break;
		case AKEY_SCREENSHOT_INTERLACE:
			Screen_SaveNextScreenshot(TRUE);
			break;
		case AKEY_BREAK:
			key_break = 1;
			break;
		default:
			key_break = 0;
			key_code = keycode;
			break;
		}

		if (++test_val == refresh_rate)
		{
			Atari800_Frame(EMULATE_FULL);
#ifndef DONT_SYNC_WITH_HOST
			atari_sync(); /* here seems to be the best place to sync */
#endif
			Atari_DisplayScreen((UBYTE *) atari_screen);
			test_val = 0;
		}
		else
		{
#ifdef VERY_SLOW
			Atari800_Frame(EMULATE_BASIC);
#else	/* VERY_SLOW */
			Atari800_Frame(EMULATE_NO_SCREEN);
#ifndef DONT_SYNC_WITH_HOST
			atari_sync();
#endif
#endif	/* VERY_SLOW */
		}
	}
}

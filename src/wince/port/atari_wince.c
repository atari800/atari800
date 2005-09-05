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
#include "screen_wince.h"
#include "keyboard.h"
#include "main.h"
#include "sound.h"
#include "rt-config.h"
#include "ui.h"
#include "screen.h"

//extern int show_atari_speed;

static int usesnd = 1;

static int kbjoy = 1;

DWORD REG_bat, REG_ac, REG_disp;

/* WinCE port does not have console, no log either */
void Aprint(char *format, ... ) {}
void Aflushlog(void) {}

static DWORD reg_access(TCHAR *key, TCHAR *val, DWORD data)
{
	HKEY regkey;
	DWORD tmpval, cbdata;

	if (RegOpenKeyEx(HKEY_CURRENT_USER, key, 0, 0, &regkey) != ERROR_SUCCESS)
		return data;

	cbdata = sizeof(DWORD);
	if (RegQueryValueEx(regkey, val, NULL, NULL, (LPBYTE) &tmpval, &cbdata) != ERROR_SUCCESS)
	{
		RegCloseKey(regkey);
		return data;
	}

	cbdata = sizeof(DWORD);
	if (RegSetValueEx(regkey, val, 0, REG_DWORD, (LPBYTE) &data, cbdata) != ERROR_SUCCESS)
	{
		RegCloseKey(regkey);
		return data;
	}

	RegCloseKey(regkey);
	return tmpval;
}

static void backlight_xchg(void)
{
	HANDLE h;

	REG_bat = reg_access(_T("ControlPanel\\BackLight"), _T("BatteryTimeout"), REG_bat);
	REG_ac = reg_access(_T("ControlPanel\\BackLight"), _T("ACTimeout"), REG_ac);
	REG_disp = reg_access(_T("ControlPanel\\Power"), _T("Display"), REG_disp);

	h = CreateEvent(NULL, FALSE, FALSE, _T("BackLightChangeEvent"));
	if (h)
	{
		SetEvent(h);
		CloseHandle(h);
	}
}

int Atari_Keyboard(void)
{
	int keycode;
	
	prockb();

	keycode = get_last_key();

	return keycode;
}

void Atari_Initialise(int *argc, char *argv[])
{
#ifdef SOUND
	if (usesnd)
		Sound_Initialise(argc, argv);
#endif

	if (gron(argc, argv))
	{
		perror("Graphics initialization failed");
		exit(1);
	}
	if(initinput())
	{
		perror("Input initialization failed");
		exit(1);
	}

	/* backlight */
	REG_bat = REG_ac = REG_disp = 2 * 60 * 60 * 1000; /* 2hrs should do it */
	backlight_xchg();
	
	clearkb();
}

int Atari_Exit(int run_monitor)
{
	backlight_xchg();	/* restore backlight settings */

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

void Atari_ConfigInit(void)
{
	strcpy(atari_osa_filename, "atariosa.rom");
	strcpy(atari_osb_filename, "atariosb.rom");
	strcpy(atari_xlxe_filename, "atarixl.rom");
	strcpy(atari_basic_filename, "ataribas.rom");
	strcpy(atari_5200_filename, "5200.rom");
	strcpy(atari_disk_dirs[0], ".");
	strcpy(atari_rom_dir, ".");
	strcpy(atari_exe_dir, ".");
	strcpy(atari_state_dir, ".");
	enable_new_pokey = 0;
	screen_visible_x1 = 24;
	screen_visible_y1 = 10;
	screen_visible_x2 = 344;
	screen_visible_y2 = 230;
}

int Atari_Configure(char* option, char *parameters)
{
	if (strcmp(option, "WCE_LINEAR_FILTER") == 0)
	{
		sscanf(parameters, "%d", &smooth_filter);
		return 1;
	}

	return 0;
}

void Atari_ConfigSave(FILE *fp)
{
	fprintf(fp, "WCE_LINEAR_FILTER=%d\n", smooth_filter);
}

int wince_main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	while (TRUE) {
		key_code = Atari_Keyboard();
		Atari800_Frame();
		if (display_screen)
			Atari_DisplayScreen((UBYTE *) atari_screen);
	}
}

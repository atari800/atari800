/*
 * atari_wince.c - WinCE port specific code
 *
 * Copyright (C) 2001 Vasyl Tsvirkunov
 * Copyright (C) 2001-2006 Atari800 development team (see DOC/CREDITS)
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

/* Based on Win32 port by  Krzysztof Nikiel */

#include "config.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "atari.h"
#include "input.h"
#include "platform.h"
#include "screen.h"
#include "sound.h"
#include "ui.h"
#include "pokeysnd.h"

#include "keyboard.h"
#include "main.h"
#include "screen_wince.h"

static int kbjoy = 1;

DWORD REG_bat, REG_ac, REG_disp, bat_timeout;

/* XXX: don't define Log_print, use a platform-specific output in log.c */
void Log_print(char *format, ... )
{
#ifdef DEBUG
	char tmp1[512];
	TCHAR tmp2[512];
	va_list va;

	va_start(va, format);
	_vsnprintf(tmp1, 512, format, va);
	va_end(va);
	MultiByteToWideChar(CP_ACP, 0, tmp1, strlen(tmp1) + 1, tmp2, sizeof(tmp2));
	OutputDebugString(tmp2);
#endif
}
void Log_flushlog(void) {}

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

int PLATFORM_Keyboard(void)
{
	int keycode;

	prockb();
	keycode = get_last_key();

	return keycode;
}

int PLATFORM_Initialise(int *argc, char *argv[])
{
#ifdef SOUND
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
	SystemParametersInfo(SPI_GETBATTERYIDLETIMEOUT, 0, (void *) &bat_timeout, 0);
	SystemParametersInfo(SPI_SETBATTERYIDLETIMEOUT, 60 * 60 * 2, NULL, SPIF_SENDCHANGE);

	clearkb();

	return TRUE;
}

int PLATFORM_Exit(int run_monitor)
{
	backlight_xchg();	/* restore backlight settings */
	SystemParametersInfo(SPI_SETBATTERYIDLETIMEOUT, bat_timeout, NULL, SPIF_SENDCHANGE);

	/* monitor is not avaliable in this port */
	if(run_monitor)
		return 1;

#ifdef BUFFERED_LOG
	Log_flushlog();
#endif

	uninitinput();
	groff();

#ifdef SOUND
	Sound_Exit();
#endif

	return 0;
}

void PLATFORM_DisplayScreen(void)
{
	refreshv((UBYTE *) Screen_atari + 24);
}

int PLATFORM_PORT(int num)
{
	if (num == 0)
		return stick0;
	else
		return 0xff;
}


int PLATFORM_TRIG(int num)
{
	if (num == 0)
		return trig0;
	else
		return 1;
}

void PLATFORM_ConfigInit(void)
{
	enable_new_pokey = 0;
	Screen_visible_x1 = 24;
	Screen_visible_y1 = 10;
	Screen_visible_x2 = 344;
	Screen_visible_y2 = 230;
	Screen_show_disk_led = 1;
	Screen_show_sector_counter = 1;
	Screen_show_atari_speed = 1;
}

int PLATFORM_Configure(char* option, char *parameters)
{
	if (strcmp(option, "WCE_LINEAR_FILTER") == 0)
	{
		sscanf(parameters, "%d", &smooth_filter);
		return 1;
	}
	else if (strcmp(option, "WCE_VIRTUAL_JOYSTICK") == 0)
	{
		sscanf(parameters, "%d", &virtual_joystick);
		return 1;
	}
	else if (strcmp(option, "WCE_SMARTPHONE_KBHACK") == 0)
	{
		sscanf(parameters, "%d", &smkeyhack);
		return 1;
	}

	return 0;
}

void PLATFORM_ConfigSave(FILE *fp)
{
	fprintf(fp, "WCE_LINEAR_FILTER=%d\n", smooth_filter);
	fprintf(fp, "WCE_VIRTUAL_JOYSTICK=%d\n", virtual_joystick);
}

void AboutPocketAtari(void)
{
	ui_driver->fInfoScreen("About Pocket Atari", issmartphone ?
		"Pocket Atari for Smartphones\0"
		"Built on: " __DATE__ "\0"
		"\0"
		"Ported by Kostas Nakos\0"
		"(knakos@gmail.com)\0"
		"http://pocketatari.atari.org"
		"\0"
		"\0"
		"\0"
		"Based on the PocketPC/WinCE port\0"
		"by Vasyl Tsvirkunov\0"
		"http://pocketatari.retrogames.com\0"
		"\0"
		"\0"
		"Atari core for this version\0"
		Atari800_TITLE "\0"
		"http://atari800.sf.net\0"
		"\n"
	:
		"Pocket Atari v.1.2 (" __DATE__ ")\0"
		"by Vasyl Tsvirkunov (C) 2002\0"
		"http://pocketatari.retrogames.com\0"
		"\0"
		"\0"
		"This port is based on\0"
		Atari800_TITLE "\0"
		"http://atari800.sf.net\0"
		"\0"
		"PocketPC port update and\0"
		"Smartphone port by Kostas Nakos\0"
		"(knakos@gmail.com)\0"
		"http://pocketatari.atari.org"
		"\0"
		"\n"
	);
}

int wince_main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	for (;;) 
	{
		if (emulator_active)
		{
			INPUT_key_code = PLATFORM_Keyboard();
			Atari800_Frame();
			if (Atari800_display_screen)
				PLATFORM_DisplayScreen();
		}
		else
		{
			Sleep(100);
		}
	}
}

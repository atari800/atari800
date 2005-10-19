/*
 * ui_wince.c - WinCE PocketPC UI driver
 *
 * Copyright (C) 2001 Vasyl Tsvirkunov
 * Copyright (C) 2000-2005 Atari800 development team (see DOC/CREDITS)
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
#include <stdio.h>
#include <string.h>
#include "atari.h"
#include "input.h"    /* For joystick autofire */
#include "ui.h"
#include "util.h"

#include "keyboard.h" /* For virtual joystick */
#include "screen_wince.h"   /* For linear filter */

int WinCeUISelect(const char *pTitle, int bFloat, int nDefault, tMenuItem *menu, int *ascii);
int BasicUISelectInt(int nDefault, int nMin, int nMax);
int BasicUIEditString(const char *pTitle, char *pString, int nSize);
int BasicUIGetSaveFilename(char *pFilename, char pDirectories[][FILENAME_MAX], int nDirectories);
int BasicUIGetLoadFilename(char *pFilename, char pDirectories[][FILENAME_MAX], int nDirectories);
int BasicUIGetDirectoryPath(char *pDirectory);
void BasicUIMessage(const char *pMessage);
void BasicUIAboutBox(void);
void BasicUIInit(void);

extern int smooth_filter;
extern int filter_available;
extern char issmartphone;

tUIDriver wince_ui_driver =
{
	&WinCeUISelect,
	&BasicUISelectInt,
	&BasicUIEditString,
	&BasicUIGetSaveFilename,
	&BasicUIGetLoadFilename,
	&BasicUIGetDirectoryPath,
	&BasicUIMessage,
	&BasicUIAboutBox,
	&BasicUIInit
};

void AboutPocketAtari()
{
	ClearScreen();
	Box(0x9a, 0x94, 0, 0, 39, 23);
	if (issmartphone) {
		CenterPrint(0x9a, 0x94, "Pocket Atari for Smartphones", 1);
		CenterPrint(0x9a, 0x94, "Built on: " __DATE__, 2);
		CenterPrint(0x9a, 0x94, "Ported by Kostas Nakos", 4);
		CenterPrint(0x9a, 0x94, "(knakos@gmail.com)", 5);
		CenterPrint(0x9a, 0x94, "http://users.uoa.gr/...               ", 6);
		CenterPrint(0x9a, 0x94, "             ...(tilde)knakos/atari800", 7);
		CenterPrint(0x9a, 0x94, "Based on the PocketPC/WinCE port", 10);
		CenterPrint(0x9a, 0x94, "by Vasyl Tsvirkunov", 11);
		CenterPrint(0x9a, 0x94, "http://pocketatari.retrogames.com", 12);
		CenterPrint(0x9a, 0x94, "Atari core for this version", 15);
		CenterPrint(0x9a, 0x94, ATARI_TITLE, 16);
		CenterPrint(0x9a, 0x94, "http://atari800.sf.net", 17);
	}
	else {
		CenterPrint(0x9a, 0x94, "Pocket Atari v.1.2 (" __DATE__ ")", 1);
		CenterPrint(0x9a, 0x94, "by Vasyl Tsvirkunov (C) 2002", 2);
		CenterPrint(0x9a, 0x94, "http://pocketatari.retrogames.com", 3);
		CenterPrint(0x9a, 0x94, "This port is based on", 6);
		CenterPrint(0x9a, 0x94, ATARI_TITLE, 7);
		CenterPrint(0x9a, 0x94, "http://atari800.sf.net", 8);
		CenterPrint(0x9a, 0x94, "PocketPC port update and", 10);
		CenterPrint(0x9a, 0x94, "Smartphone port by Kostas Nakos", 11);
		CenterPrint(0x9a, 0x94, "(knakos@gmail.com)", 12);
		CenterPrint(0x9a, 0x94, "http://users.uoa.gr/...               ", 13);
		CenterPrint(0x9a, 0x94, "             ...(tilde)knakos/atari800", 14);
	}
	CenterPrint(0x94, 0x9a, "Press any Key to Continue", 22);
	GetKeyPress();
}

void EmulatorSettings()
{
	static tMenuItem menu_array[] =
	{
		{ "SMTH", ITEM_ENABLED | ITEM_CHECK,  NULL, "Enable linear filtering:", NULL, 0 },
		{ "VJOY", ITEM_ENABLED | ITEM_CHECK,  NULL, "Virtual joystick:", NULL, 1 },
		{ "AFRE", ITEM_ENABLED | ITEM_CHECK,  NULL, "Joystick autofire:", NULL, 2 },
		MENU_END
	};

	int option = 0;

	do {
		if (filter_available)
			menu_array[0].flags |= ITEM_ENABLED;
		else
			menu_array[0].flags &= ~ITEM_ENABLED;

		if (smooth_filter)
			menu_array[0].flags |= ITEM_CHECKED;
		else
			menu_array[0].flags &= ~ITEM_CHECKED;
		if (virtual_joystick)
			menu_array[1].flags |= ITEM_CHECKED;
		else
			menu_array[1].flags &= ~ITEM_CHECKED;
		if (joy_autofire[0])
			menu_array[2].flags |= ITEM_CHECKED;
		else
			menu_array[2].flags &= ~ITEM_CHECKED;

		option = ui_driver->fSelect(NULL, TRUE, option, menu_array, NULL);

		switch (option) {
		case 0:
			smooth_filter = !smooth_filter;
			break;
		case 1:
			virtual_joystick = !virtual_joystick;
			break;
		case 2:
			joy_autofire[0] = joy_autofire[0] ? 0 : 1;
			break;
		default:
			break;
		}
	} while(option >= 0);
}


int WinCeUISelect(const char *pTitle, int bFloat, int nDefault, tMenuItem *menu, int *ascii)
{
	tMenuItem *pNewMenu;
	int i;
	int result;
	int itemcount;

/* Count items in old menu */
	for (itemcount = 1; menu[itemcount].sig[0] != '\0'; itemcount++) ;
/* Allocate memory for new menu, one extra item */
	pNewMenu = (tMenuItem *) Util_malloc(sizeof(tMenuItem) * itemcount);
/* Copy old menu, add/modify items when necessary */
	memcpy(pNewMenu, menu, sizeof(tMenuItem) * itemcount);
/* Modify old menu items */
	for (i = 0; pNewMenu[i].sig[0] != '\0'; i++) {
		if (strcmp(pNewMenu[i].sig, "STER") == 0) {
#ifndef STEREO_SOUND
			/* Stereo is disabled based on compile settings */
			pNewMenu[i].flags &= ~ITEM_ENABLED;
#endif
		}
#if 0
		else if (strcmp(pNewMenu[i].sig, "PCXI") == 0 || strcmp(pNewMenu[i].sig, "PCXN") == 0) {
			/* Disable screenshots */
			pNewMenu[i].flags &= ~ITEM_ENABLED;
		}
#endif
		else if (strcmp(pNewMenu[i].sig, "SREC") == 0) {
			/* Sound Recording Start/Stop replaced with Emulator Settings */
			strcpy(pNewMenu[i].sig, "EMUX");
			pNewMenu[i].flags = ITEM_ENABLED | ITEM_SUBMENU;
			pNewMenu[i].prefix = NULL;
			pNewMenu[i].item = "Pocket Atari Settings";
			pNewMenu[i].suffix = NULL;
			pNewMenu[i].retval = 1000;
		}
		else if (strcmp(pNewMenu[i].sig, "MONI") == 0) {
			if (strcmp(pNewMenu[0].sig, "XBIN") == 0) { /* Main menu? */
			/* Monitor menu item replaced with About box */
				strcpy(pNewMenu[i].sig, "ABPA");
				pNewMenu[i].flags = ITEM_ENABLED | ITEM_SUBMENU;
				pNewMenu[i].prefix = NULL;
				pNewMenu[i].item = "About Pocket Atari";
				pNewMenu[i].suffix = NULL;
				pNewMenu[i].retval = 1001;
			}
			else {
				pNewMenu[i].flags &= ~ITEM_ENABLED;
			}
		}
		else if ((strcmp(pNewMenu[i].sig, "HFPO") == 0) && issmartphone) {
			/* NEVER allow hifi pokey in smartphones */
			pNewMenu[i].flags &= ~ITEM_ENABLED;
		}
		else if ((strcmp(pNewMenu[i].sig, "VJOY") == 0) && issmartphone) {
			pNewMenu[i].flags &= ~ITEM_ENABLED;
		}
	}
/* Passthrough to the basic driver */
	result = basic_ui_driver.fSelect(pTitle, bFloat, nDefault, pNewMenu, ascii);
/* Free memory */
	free((void *) pNewMenu);
/* Done */
	switch (result) {
	case 1000:
		EmulatorSettings();
		break;
	case 1001:
		AboutPocketAtari();
		break;
	}
	return result;
}

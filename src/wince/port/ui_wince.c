/*
 * ui_wince.c - WinCE PocketPC UI driver
 *
 * Copyright (C) 2001 Vasyl Tsvirkunov
 * Copyright (C) 2000-2003 Atari800 development team (see DOC/CREDITS)
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

#include <string.h>
#include "atari.h"
#include "ui.h"
#include "screen.h"   /* For linear filter */
#include "keyboard.h" /* For virtual joystick */
#include "input.h"    /* For joystick autofire */
#include "rt-config.h"/* For refresh rate */

int WinCeUISelect(char* pTitle, int bFloat, int nDefault, tMenuItem* menu, int* ascii);
int WinCeUIGetSaveFilename(char* pFilename);
int WinCeUIGetLoadFilename(char* pDirectory, char* pFilename);
void WinCeUIMessage(char* pMessage);
void WinCeUIAboutBox();
void WinCeUIInit();


tUIDriver wince_ui_driver =
{
	&WinCeUISelect,
	&WinCeUIGetSaveFilename,
	&WinCeUIGetLoadFilename,
	&WinCeUIMessage, 
	&WinCeUIAboutBox,
	&WinCeUIInit
};

void AboutPocketAtari()
{
	static tMenuItem menu_array[] =
	{
		{ "WKEY", ITEM_ENABLED|ITEM_ACTION, NULL, "Press Enter To Continue", NULL, 0 },
		MENU_END
	};

	char* About =
		"Pocket Atari v.1.1 ("__DATE__")\n"
		"by Vasyl Tsvirkunov (C) 2002\n"
		"http://pocketatari.retrogames.com\n"
		"\n"
		"Please report all problems\n"
		"to vasyl@pacbell.net\n"
		"\n"
		"This port is based on\n"
		ATARI_TITLE "\n"
		"http://atari800.sf.net\n"
		"\n"
		"\n"
		"\n"
		"\n"
		"\n"
		"\n"
		"\n"
		;
	ui_driver->fSelect(About, FALSE, 0, menu_array, NULL);
}



void EmulatorSettings()
{
	static tMenuItem menu_array[] =
	{
		{ "SMTH", ITEM_ENABLED|ITEM_CHECK,  NULL, "Enable linear filtering:",          NULL, 0 },
		{ "VJOY", ITEM_ENABLED|ITEM_CHECK,  NULL, "Virtual joystick:",                 NULL, 1 },
		{ "AFRE", ITEM_ENABLED|ITEM_CHECK,  NULL, "Joystick autofire:",                NULL, 2 },
		{ "FRAM", ITEM_ENABLED|ITEM_ACTION, NULL, "Current frameskip 00",              NULL, 3 },
		{ "FRUP", ITEM_ENABLED|ITEM_ACTION, NULL, "Increase frameskip",                NULL, 4 },
		{ "FRDN", ITEM_ENABLED|ITEM_ACTION, NULL, "Decrease frameskip",                NULL, 5 },
		MENU_END
	};

	int option = 0;

	do
	{
		if(filter_available)
			menu_array[0].flags |= ITEM_ENABLED;
		else
			menu_array[0].flags &= ~ITEM_ENABLED;

		if(smooth_filter)
			menu_array[0].flags |= ITEM_CHECKED;
		else
			menu_array[0].flags &= ~ITEM_CHECKED;
		if(virtual_joystick)
			menu_array[1].flags |= ITEM_CHECKED;
		else
			menu_array[1].flags &= ~ITEM_CHECKED;
		if(joy_autofire[0])
			menu_array[2].flags |= ITEM_CHECKED;
		else
			menu_array[2].flags &= ~ITEM_CHECKED;

		menu_array[3].item[18] = (refresh_rate > 9) ? (char)('0' + refresh_rate/10) : ' ';
		menu_array[3].item[19] = (char)('0' + refresh_rate%10);

		option = ui_driver->fSelect(NULL, TRUE, option, menu_array, NULL);

		switch(option)
		{
		case 0:
			smooth_filter = !smooth_filter;
			break;
		case 1:
			virtual_joystick = !virtual_joystick;
			break;
		case 2:
			joy_autofire[0] = joy_autofire[0]?0:1;
			break;
		case 4:
			if(refresh_rate < 15)
				refresh_rate ++;
			break;
		case 5:
			if(refresh_rate > 1)
				refresh_rate --;
			break;
		}
	}
	while(option >= 0);
}


int WinCeUISelect(char* pTitle, int bFloat, int nDefault, tMenuItem* menu, int* ascii)
{
	tMenuItem* pNewMenu;
	int i;
	int result;
	int itemcount;


/* Count items in old menu */	
	for(itemcount=0; menu[itemcount].sig; itemcount++) ;
	itemcount ++;
/* Allocate memory for new menu, one extra item */
	pNewMenu = (tMenuItem*)malloc(sizeof(tMenuItem)*itemcount);
/* Copy old menu, add/modify items when necessary */
	memcpy(pNewMenu, menu, sizeof(tMenuItem)*itemcount);
/* Modify old menu items */
	for(i=0; pNewMenu[i].sig; i++)
	{
		pNewMenu[i].suffix = NULL; /* no keyboard shortcuts in Pocket Atari */

		if(strcmp(pNewMenu[i].sig, "STER") == 0)
		{
#ifndef STEREO_SOUND
		/* Stereo is disabled based on compile settings */
			pNewMenu[i].flags &= ~ITEM_ENABLED;
#endif
		}
		else if(strcmp(pNewMenu[i].sig, "PCXI") == 0)
		{
		/* Interlaced PCX screenshot is disabled */
			pNewMenu[i].flags &= ~ITEM_ENABLED;
		}
		else if(strcmp(pNewMenu[i].sig, "SREC") == 0)
		{
		/* Interlaced PCX replaced by Emulator Settings (old feature was too esoteric) */
			pNewMenu[i].sig = "EMUX";
			pNewMenu[i].flags = ITEM_ENABLED|ITEM_SUBMENU;
			pNewMenu[i].prefix = NULL;
			pNewMenu[i].item = "Pocket Atari Settings";
			pNewMenu[i].suffix = NULL;
			pNewMenu[i].retval = 1000;
		}
		else if(strcmp(pNewMenu[i].sig, "MONI") == 0)
		{
			if(strcmp(pNewMenu[0].sig, "DISK") == 0) /* Main menu? */
			{
			/* Monitor menu item replaced with About box */
				pNewMenu[i].sig = "ABPA";
				pNewMenu[i].flags = ITEM_ENABLED|ITEM_SUBMENU;
				pNewMenu[i].prefix = NULL;
				pNewMenu[i].item = "About Pocket Atari";
				pNewMenu[i].suffix = NULL;
				pNewMenu[i].retval = 1001;
			}
			else
			{
				pNewMenu[i].flags &= ~ITEM_ENABLED;
			}
		}
	}
/* Passthrough to the basic driver */
	result = basic_ui_driver.fSelect(pTitle, bFloat, nDefault, pNewMenu, ascii);
/* Free memory */
	free((void*)pNewMenu);
/* Done */
	switch(result)
	{
	case 1000:
		EmulatorSettings();
		break;
	case 1001:
		AboutPocketAtari();
		break;
	}
	return result;
}

int WinCeUIGetSaveFilename(char* pFilename)
{
	return basic_ui_driver.fGetSaveFilename(pFilename);
}

int WinCeUIGetLoadFilename(char* pDirectory, char* pFilename)
{
	return basic_ui_driver.fGetLoadFilename(pDirectory, pFilename);
}

void WinCeUIMessage(char* pMessage)
{
	basic_ui_driver.fMessage(pMessage);
}

void WinCeUIAboutBox()
{
	basic_ui_driver.fAboutBox();
}

void WinCeUIInit()
{
	basic_ui_driver.fInit();
}


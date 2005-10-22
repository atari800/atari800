/*
 * ui_wince.c - WinCE PocketPC User Interface
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

#include "atari.h"
#include "ui.h"
#include "main.h"

void AboutPocketAtari(void)
{
	ui_driver->fInfoScreen("About Pocket Atari", issmartphone ?
		"Pocket Atari for Smartphones\0"
		"Built on: " __DATE__ "\0"
		"\0"
		"Ported by Kostas Nakos\0"
		"(knakos@gmail.com)\0"
		"http://users.uoa.gr/...               \0"
		"             ...(tilde)knakos/atari800\0"
		"\0"
		"\0"
		"Based on the PocketPC/WinCE port\0"
		"by Vasyl Tsvirkunov\0"
		"http://pocketatari.retrogames.com\0"
		"\0"
		"\0"
		"Atari core for this version\0"
		ATARI_TITLE "\0"
		"http://atari800.sf.net\0"
		"\n"
	:
		"Pocket Atari v.1.2 (" __DATE__ ")\0"
		"by Vasyl Tsvirkunov (C) 2002\0"
		"http://pocketatari.retrogames.com\0"
		"\0"
		"\0"
		"This port is based on\0"
		ATARI_TITLE "\0"
		"http://atari800.sf.net\0"
		"\0"
		"PocketPC port update and\0"
		"Smartphone port by Kostas Nakos\0"
		"(knakos@gmail.com)\0"
		"http://users.uoa.gr/...               \0"
		"             ...(tilde)knakos/atari800\0"
		"\n"
	);
}

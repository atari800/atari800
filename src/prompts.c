/*
 * prompts.c - Routines which prompt for input
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
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

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "prompts.h"

void GetString(char *message, char *string)
{
	char gash[128];

	printf(message, string);
	fgets(gash, sizeof(gash), stdin);
	RemoveLF(gash);
	if (strlen(gash) > 0)
		strcpy(string, gash);
}

void GetNumber(char *message, int *num)
{
	char gash[128];

	printf(message, *num);
	fgets(gash, sizeof(gash), stdin);
	RemoveLF(gash);
	if (strlen(gash) > 0)
		sscanf(gash, "\n%d", num);
}

void GetYesNo(char *message, char *yn)
{
	char gash[128];
	char t_yn;

	do {
		printf(message, *yn);
		fgets(gash, sizeof(gash), stdin);
		RemoveLF(gash);

		if (strlen(gash) > 0)
			t_yn = toupper(gash[0]);
		else
			t_yn = ' ';

	} while ((t_yn != ' ') && (t_yn != 'Y') && (t_yn != 'N'));

	if (t_yn != ' ')
		*yn = t_yn;
}

void GetYesNoAsInt(char *message, int *num)
{
	char yn = (*num > 0) ? 'Y' : 'N';
	GetYesNo(message, &yn);
	*num = (yn == 'Y') ? 1 : 0;
}

void RemoveSpaces(char *string)
{
	static char SPACES_TO_REMOVE[] = " \t\r\n";
	int len = strlen(string);

	/* head */
/*
	int skiphead = 0;
	while(skiphead < len && strchr(SPACES_TO_REMOVE, string[skiphead])) {
		skiphead++;
	}
*/
	int skiphead = strspn(string, SPACES_TO_REMOVE);
	if (skiphead > 0) {
		memmove(string, string+skiphead, len-skiphead+1);
		len -= skiphead;
	}
	/* tail */
	while(--len > 0) {
		if (strchr(SPACES_TO_REMOVE, string[len]) != NULL) {
			string[len] = '\0';
		}
		else {
			break;
		}
	}
}

void RemoveLF(char *string)
{
	int len;

	len = strlen(string);
	if (len >= 2 && string[len - 1] == '\n' && string[len - 2] == '\r')
		string[len - 2] = '\0';
	else if (len >= 1 && (string[len - 1] == '\n' || string[len - 1] == '\r'))
		string[len - 1] = '\0';
}

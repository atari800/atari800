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
	for (;;) {
		char gash[128];
		printf(message, *yn);
		fgets(gash, sizeof(gash), stdin);
		switch (gash[0]) {
		case '\0':
		case '\r':
		case '\n':
		case ' ':
			return; /* keep *yn unchanged */
		case 'Y':
		case 'y':
			*yn = 'Y';
			return;
		case 'N':
		case 'n':
			*yn = 'N';
			return;
		default:
			break;
		}
	}
}

void GetYesNoAsInt(char *message, int *num)
{
	char yn = (*num > 0) ? 'Y' : 'N';
	GetYesNo(message, &yn);
	*num = (yn == 'Y') ? 1 : 0;
}

void RemoveSpaces(char *string)
{
	char *p = string;
	char *q;
	/* skip leading whitespace */
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		p++;
	/* now p points at the first non-whitespace character */

	if (*p == '\0') {
		/* only whitespace */
		*string = '\0';
		return;
	}

	q = string + strlen(string);
	/* skip trailing whitespace */
	/* we have found p < q such that *p is non-whitespace,
	   so this loop terminates with q >= p */
	do
		q--;
	while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n');

	/* now q points at the last non-whitespace character */
	/* cut off trailing whitespace */
	*++q = '\0';

	/* move to string */
	memmove(string, p, q + 1 - p);
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

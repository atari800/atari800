/*
 * binload.c - load a binary executable file
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2003 Atari800 development team (see DOC/CREDITS)
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

#include "atari.h"
#include "cpu.h"
#include "log.h"
#include "memory.h"

int start_binloading = 0;
static FILE *binf = NULL;

/* Read a word from file */
static SLONG BIN_read_word(void)
{
	UBYTE buf[2];
	if (fread(buf, 1, 2, binf) != 2) {
		fclose(binf);
		binf = 0;
		if (start_binloading) {
			start_binloading = 0;
			Aprint("binload: not valid BIN file");
			return -1;
		}
		regPC = dGetWord(0x2e0);
		return -1;
	}
	return buf[0] | (buf[1] << 8);
}

/* Start or continue loading */
void BIN_loader_cont(void)
{
	SLONG temp;
	UWORD from;
	UWORD to;
	UBYTE byte;

	if (!binf)
		return;
	if (start_binloading) {
		dPutByte(0x244, 0);
		dPutByte(0x09, 1);
	}
	else
		regS += 2;	/* pop ESC code */

	dPutByte(0x2e3, 0xd7);
	do {
		do
			temp = BIN_read_word();
		while (temp == 0xffff);
		if (temp < 0)
			return;
		from = (UWORD) temp;

		temp = BIN_read_word();
		if (temp < 0)
			return;
		to = (UWORD) temp;

		if (start_binloading) {
			dPutWord(0x2e0, from);
			start_binloading = 0;
		}

		to++;
		do {
			if (fread(&byte, 1, 1, binf) == 0) {
				fclose(binf);
				binf = 0;
				regPC = dGetWord(0x2e0);
				if (dGetByte(0x2e3) != 0xd7) {
					regPC--;
					dPutByte(0x0100 + regS--, regPC >> 8);		/* high */
					dPutByte(0x0100 + regS--, regPC & 0xff);	/* low */
					regPC = dGetWord(0x2e2);
				}
				return;
			}
			PutByte(from, byte);
			from++;
		} while (from != to);
	} while (dGetByte(0x2e3) == 0xd7);

	regS--;
	Atari800_AddEsc(0x100 + regS, ESC_BINLOADER_CONT, BIN_loader_cont);
	regS--;
	dPutByte(0x0100 + regS--, 0x01);	/* high */
	dPutByte(0x0100 + regS, regS + 1);	/* low */
	regS--;
	regPC = dGetWord(0x2e2);
	SetC;

	dPutByte(0x0300, 0x31);	/* for "Studio Dream" */
}

/* Fake boot sector to call BIN_loader_cont at boot time */
int BIN_loade_start(UBYTE *buffer)
{
	buffer[0] = 0x00;	/* ignored */
	buffer[1] = 0x01;	/* one boot sector */
	buffer[2] = 0x00;	/* start at memory location 0x0700 */
	buffer[3] = 0x07;
	buffer[4] = 0x77;	/* reset reboots (0xe477 = Atari OS Coldstart) */
	buffer[5] = 0xe4;
	buffer[6] = 0xf2;	/* ESC */
	buffer[7] = ESC_BINLOADER_CONT;
	Atari800_AddEsc(0x706, ESC_BINLOADER_CONT, BIN_loader_cont);
	return 'C';
}

/* Load BIN file, returns TRUE if ok */
int BIN_loader(char *filename)
{
	UBYTE buf[2];
	if (binf)		/* close previously open file */
		fclose(binf);
	if (!(binf = fopen(filename, "rb"))) {	/* open */
		Aprint("binload: can't open %s", filename);
		return FALSE;
	}
	if (fread(buf, 1, 2, binf) != 2 || buf[0] != 0xff || buf[1] != 0xff) {	/* check header */
		fclose(binf);
		binf = 0;
		Aprint("binload: not valid BIN file");
		return FALSE;
	}

	Coldstart();			/* reboot */
	start_binloading = 1;	/* force SIO to call BIN_loade_start at boot */
	return TRUE;
}

/*
 * binload.c - load a binary executable file
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

#include "config.h"
#include <stdio.h>

#include "atari.h"
#include "binload.h"
#include "cpu.h"
#include "devices.h"
#include "log.h"
#include "memory.h"
#include "sio.h"

int start_binloading = FALSE;
int loading_basic = 0;
FILE *bin_file = NULL;

/* Read a word from file */
static int BIN_read_word(void)
{
	UBYTE buf[2];
	if (fread(buf, 1, 2, bin_file) != 2) {
		fclose(bin_file);
		bin_file = NULL;
		if (start_binloading) {
			start_binloading = FALSE;
			Aprint("binload: not valid BIN file");
			return -1;
		}
		regPC = dGetWordAligned(0x2e0);
		return -1;
	}
	return buf[0] + (buf[1] << 8);
}

/* Start or continue loading */
void BIN_loader_cont(void)
{
	if (bin_file == NULL)
		return;
	if (start_binloading) {
		dPutByte(0x244, 0);
		dPutByte(0x09, 1);
	}
	else
		regS += 2;	/* pop ESC code */

	dPutByte(0x2e3, 0xd7);
	do {
		int temp;
		UWORD from;
		UWORD to;
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
			dPutWordAligned(0x2e0, from);
			start_binloading = FALSE;
		}

		to++;
		do {
			int byte = fgetc(bin_file);
			if (byte == EOF) {
				fclose(bin_file);
				bin_file = NULL;
				regPC = dGetWordAligned(0x2e0);
				if (dGetByte(0x2e3) != 0xd7) {
					/* run INIT routine which RTSes directly to RUN routine */
					regPC--;
					dPutByte(0x0100 + regS--, regPC >> 8);		/* high */
					dPutByte(0x0100 + regS--, regPC & 0xff);	/* low */
					regPC = dGetWordAligned(0x2e2);
				}
				return;
			}
			PutByte(from, (UBYTE) byte);
			from++;
		} while (from != to);
	} while (dGetByte(0x2e3) == 0xd7);

	regS--;
	Atari800_AddEsc((UWORD) (0x100 + regS), ESC_BINLOADER_CONT, BIN_loader_cont);
	regS--;
	dPutByte(0x0100 + regS--, 0x01);	/* high */
	dPutByte(0x0100 + regS, regS + 1);	/* low */
	regS--;
	regPC = dGetWordAligned(0x2e2);
	SetC;

	dPutByte(0x0300, 0x31);	/* for "Studio Dream" */
}

/* Fake boot sector to call BIN_loader_cont at boot time */
int BIN_loader_start(UBYTE *buffer)
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
int BIN_loader(const char *filename)
{
	UBYTE buf[2];
	if (bin_file != NULL) {		/* close previously open file */
		fclose(bin_file);
		bin_file = NULL;
		loading_basic = 0;
	}
	if (machine_type == MACHINE_5200) {
		Aprint("binload: can't run Atari programs directly on the 5200");
		return FALSE;
	}
	bin_file = fopen(filename, "rb");
	if (bin_file == NULL) {	/* open */
		Aprint("binload: can't open \"%s\"", filename);
		return FALSE;
	}
	/* Avoid "BOOT ERROR" when loading a BASIC program */
	if (drive_status[0] == NoDisk)
		SIO_DisableDrive(1);
	if (fread(buf, 1, 2, bin_file) == 2) {
		if (buf[0] == 0xff && buf[1] == 0xff) {
			start_binloading = TRUE; /* force SIO to call BIN_loader_start at boot */
			Coldstart();             /* reboot */
			return TRUE;
		}
		else if (buf[0] == 0 && buf[1] == 0) {
			loading_basic = LOADING_BASIC_SAVED;
			Device_PatchOS();
			Coldstart();
			return TRUE;
		}
		else if (buf[0] >= '0' && buf[0] <= '9') {
			loading_basic = LOADING_BASIC_LISTED;
			Device_PatchOS();
			Coldstart();
			return TRUE;
		}
	}
	fclose(bin_file);
	bin_file = NULL;
	Aprint("binload: \"%s\" not recognized as a DOS or BASIC program", filename);
	return FALSE;
}

/*
 * cassette.c - cassette emulation
 *
 * Copyright (C) 2001 Piotr Fusik
 * Copyright (C) 2001-2003 Atari800 development team (see DOC/CREDITS)
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

#include "atari.h"
#include "cassette.h"
#include "log.h"
#include "memory.h"
#include "sio.h"

#define MAX_BLOCKS 2048

static FILE *cassette_file = NULL;
static int cassette_isCAS;
UBYTE cassette_buffer[4096];
static ULONG cassette_block_offset[MAX_BLOCKS];

char cassette_filename[FILENAME_MAX];
char cassette_description[CASSETTE_DESCRIPTION_MAX];
int cassette_current_block;
int cassette_max_block;

int hold_start = 0;
int press_space = 0;

typedef struct {
	char identifier[4];
	UBYTE length_lo;
	UBYTE length_hi;
	UBYTE aux_lo;
	UBYTE aux_hi;
} CAS_Header;

void CASSETTE_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-tape") == 0)
			CASSETTE_Insert(argv[++i]);
		else if (strcmp(argv[i], "-boottape") == 0) {
			CASSETTE_Insert(argv[++i]);
			hold_start = 1;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-tape <file>     Insert cassette image");
				Aprint("\t-boottape <file>\tInsert cassette image and boot it");
			}
			argv[j++] = argv[i];
		}
	}

	*argc = j;
}

/* Gets information about the cassette image. Returns TRUE if ok.
   To get information without attaching file, use:
   char description[CASSETTE_DESCRIPTION_MAX];
   int last_block;
   int isCAS;
   CASSETTE_CheckFile(filename, NULL, description, &last_block, &isCAS);
*/
int CASSETTE_CheckFile(char *filename, FILE **fp, char *description, int *last_block, int *isCAS)
{
	FILE *f;
	CAS_Header header;
	int blocks;

	f = fopen(filename, "rb");
	if (f == NULL)
		return FALSE;
	if (description)
		memset(description, 0, CASSETTE_DESCRIPTION_MAX);
	if (fread(&header, 1, 6, f) == 6
		&& header.identifier[0] == 'F'
		&& header.identifier[1] == 'U'
		&& header.identifier[2] == 'J'
		&& header.identifier[3] == 'I') {
		/* CAS file */
		UWORD length;
		UWORD skip;
		if (isCAS)
			*isCAS = TRUE;
		fseek(f, 2L, SEEK_CUR);	/* ignore the aux bytes */

		/* read or skip file description */
		skip = length = header.length_lo + (header.length_hi << 8);
		if (description) {
			if (length < CASSETTE_DESCRIPTION_MAX)
				skip = 0;
			else
				skip -= CASSETTE_DESCRIPTION_MAX - 1;
			fread(description, 1, length - skip, f);
		}
		fseek(f, skip, SEEK_CUR);

		/* count number of blocks */
		blocks = 0;
		do {
			if (fread(&header, 1, 4, f) != 4)
				break;
			if (header.identifier[0] == 'd'
				&& header.identifier[1] == 'a'
				&& header.identifier[2] == 't'
				&& header.identifier[3] == 'a') {
				blocks++;
				if (fp)
					cassette_block_offset[blocks] = ftell(f);
			}
			if (fread(&header.length_lo, 1, 2, f) != 2)
				break;
			length = header.length_lo + (header.length_hi << 8);
			/* skip two aux bytes and the data block */
			fseek(f, 2 + length, SEEK_CUR);
		} while (blocks < MAX_BLOCKS);
	}
	else {
		/* raw file */
		ULONG file_length;
		if (isCAS)
			*isCAS = FALSE;
		fseek(f, 0L, SEEK_END);
		file_length = ftell(f);
		blocks = (file_length + 127) / 128 + 1;
	}
	if (last_block)
		*last_block = blocks;
	if (fp)
		*fp = f;
	else
		fclose(f);
	return TRUE;
}

int CASSETTE_Insert(char *filename)
{
	strcpy(cassette_filename, filename);
	cassette_current_block = 1;
	return CASSETTE_CheckFile(filename, &cassette_file, cassette_description,
		 &cassette_max_block, &cassette_isCAS);
}

void CASSETTE_Remove(void)
{
	if (cassette_file != NULL) {
		fclose(cassette_file);
		cassette_file = NULL;
	}
	strcpy(cassette_filename, "None");
	memset(cassette_description, 0, sizeof(cassette_description));
}

/* returns block length (with checksum) */
static UWORD ReadRecord(void)
{
	UWORD length;
	if (cassette_isCAS) {
		CAS_Header header;
		fseek(cassette_file, cassette_block_offset[cassette_current_block], SEEK_SET);
		fread(&header.length_lo, 1, 4, cassette_file);
		length = header.length_lo + (header.length_hi << 8);
		fread(cassette_buffer, 1, length, cassette_file);
	}
	else {
		length = 132;
		cassette_buffer[0] = 0x55;
		cassette_buffer[1] = 0x55;
		if (cassette_current_block == cassette_max_block) {
			/* EOF record */
			cassette_buffer[2] = 0xfe;
			memset(cassette_buffer + 3, 0, 128);
		}
		else {
			int bytes;
			fseek(cassette_file, (cassette_current_block - 1) * 128, SEEK_SET);
			bytes = fread(cassette_buffer + 3, 1, 128, cassette_file);
			if (bytes < 128) {
				cassette_buffer[2] = 0xfa;	/* non-full record */
				memset(cassette_buffer + 3 + bytes, 0, 127 - bytes);
				cassette_buffer[0x82] = bytes;
			}
			else
				cassette_buffer[2] = 0xfc;	/* full record */
		}
		cassette_buffer[0x83] = SIO_ChkSum(cassette_buffer, 0x84);
	}
	cassette_current_block++;
	return length;
}

UBYTE CASSETTE_Sio(void)
{
	int length = dGetByte(0x0308) + (dGetByte(0x0309) << 8);
	/* Only 0x52 (Read) command is supported */
	if (dGetByte(0x302) != 0x52)
		return 'N';
	if (cassette_file == NULL)
		return 'E';
	if (cassette_current_block > cassette_max_block)
		return 'E';	/* EOF */
	if (ReadRecord() - 1 != length)
		return 'E';
	return 'C';
}

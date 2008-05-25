/*
 * pbi_scsi.c - SCSI emulation for the MIO and Black Box
 *
 * Copyright (C) 2007-2008 Perry McFarlane
 * Copyright (C) 1998-2008 Atari800 development team (see DOC/CREDITS)
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

#include "atari.h"
#include "util.h"
#include "log.h"

#ifdef PBI_DEBUG
#define D(a) a
#else
#define D(a) do{}while(0)
#endif

int SCSI_CD = FALSE;
int SCSI_MSG = FALSE;
int SCSI_IO = FALSE;
int SCSI_BSY = FALSE;
int SCSI_REQ = FALSE;
int SCSI_ACK = FALSE;

int SCSI_SEL = FALSE;

static UBYTE SCSI_byte;

#define SCSI_PHASE_SELECTION 0
#define SCSI_PHASE_DATAIN 1
#define SCSI_PHASE_DATAOUT 2
#define SCSI_PHASE_COMMAND 3
#define SCSI_PHASE_STATUS 4 
#define SCSI_PHASE_MSGIN 5

static int SCSI_phase = SCSI_PHASE_SELECTION;
static int SCSI_bufpos = 0;
static UBYTE SCSI_buffer[256];
static int SCSI_count = 0;

FILE *SCSI_disk = NULL;

static void SCSI_changephase(int phase)
{
	D(printf("SCSI_changephase:%d\n",phase));
	switch(phase) {
		case SCSI_PHASE_SELECTION:
				SCSI_REQ = FALSE;
				SCSI_BSY = FALSE;
				SCSI_CD = FALSE;
				SCSI_IO = FALSE;
				SCSI_MSG = FALSE;
				break;
		case SCSI_PHASE_DATAOUT:
				SCSI_REQ = TRUE;
				SCSI_BSY = TRUE;
				SCSI_CD = FALSE;
				SCSI_IO = FALSE;
				SCSI_MSG = FALSE;
				break;
		case SCSI_PHASE_DATAIN:
				SCSI_REQ = TRUE;
				SCSI_BSY = TRUE;
				SCSI_CD = FALSE;
				SCSI_IO = TRUE;
				SCSI_MSG = FALSE;
				break;
		case SCSI_PHASE_COMMAND:
				SCSI_REQ = TRUE;
				SCSI_BSY = TRUE;
				SCSI_CD = TRUE;
				SCSI_IO = FALSE;
				SCSI_MSG = FALSE;
				break;
		case SCSI_PHASE_STATUS:
				SCSI_REQ = TRUE;
				SCSI_BSY = TRUE;
				SCSI_CD = TRUE;
				SCSI_IO = TRUE;
				SCSI_MSG = FALSE;
				break;
		case SCSI_PHASE_MSGIN:
				SCSI_REQ = TRUE;
				SCSI_BSY = TRUE;
				SCSI_CD = TRUE;
				SCSI_IO = FALSE;
				SCSI_MSG = TRUE;
				break;
	}
	SCSI_bufpos = 0;
	SCSI_phase = phase;
}

static void SCSI_process_command(void)
{
	int i;
	int lba;
	int lun;
	D(printf("SCSI command:"));
	for (i = 0; i < 6; i++) {
		D(printf(" %02x",SCSI_buffer[i]));
	}
	D(printf("\n"));
	switch (SCSI_buffer[0]) {
		case 0x00:
			/* test unit ready */
			D(printf("SCSI: test unit ready\n"));
			SCSI_changephase(SCSI_PHASE_STATUS);
			SCSI_buffer[0] = 0;
			break;
		case 0x03:
			/* request sense */
			D(printf("SCSI: request sense\n"));
			SCSI_changephase(SCSI_PHASE_DATAIN);
			memset(SCSI_buffer,0,1);
			SCSI_count = 4;
			break;
		case 0x08:
			/* read */
			lun = ((SCSI_buffer[1]&0xe0)>>5);
			lba = (((SCSI_buffer[1]&0x1f)<<16)|(SCSI_buffer[2]<<8)|(SCSI_buffer[3]));
			D(printf("SCSI: read lun:%d lba:%d\n",lun,lba));
			fseek(SCSI_disk, lba*256, SEEK_SET);
			fread(SCSI_buffer, 1, 256, SCSI_disk);
			SCSI_changephase(SCSI_PHASE_DATAIN);
			SCSI_count = 256;
			break;
		case 0x0a:
			/* write */
			lun = ((SCSI_buffer[1]&0xe0)>>5);
			lba = (((SCSI_buffer[1]&0x1f)<<16)|(SCSI_buffer[2]<<8)|(SCSI_buffer[3]));
			D(printf("SCSI: write lun:%d lba:%d\n",lun,lba));
			fseek(SCSI_disk, lba*256, SEEK_SET);
			SCSI_changephase(SCSI_PHASE_DATAOUT);
			SCSI_count = 256;
			break;
		default:
			D(printf("SCSI: unknown command:%2x\n", SCSI_buffer[0]));
			SCSI_changephase(SCSI_PHASE_SELECTION);
			break;
	}
}

static void SCSI_nextbyte(void)
{
	if (SCSI_phase == SCSI_PHASE_DATAIN) {
		SCSI_bufpos++;
		if (SCSI_bufpos >= SCSI_count) {
			SCSI_changephase(SCSI_PHASE_STATUS);
			SCSI_buffer[0] = 0;
		}
	}
	else if (SCSI_phase == SCSI_PHASE_STATUS) {
		D(printf("SCSI status\n"));
		SCSI_changephase(SCSI_PHASE_MSGIN);
		SCSI_buffer[0] = 0;
	}
	else if (SCSI_phase == SCSI_PHASE_MSGIN) {
		D(printf("SCSI msg\n"));
		SCSI_changephase(SCSI_PHASE_SELECTION);
	}
	else if (SCSI_phase == SCSI_PHASE_COMMAND) {
		SCSI_buffer[SCSI_bufpos++] = SCSI_byte;
		if (SCSI_bufpos >= 0x06) {
			SCSI_process_command();
			SCSI_bufpos = 0;
		}
	}
	else if (SCSI_phase == SCSI_PHASE_DATAOUT) {
		D(printf("SCSI data out:%2x\n", SCSI_byte));
		SCSI_buffer[SCSI_bufpos++] = SCSI_byte;
		if (SCSI_bufpos >= SCSI_count) {
			fwrite(SCSI_buffer, 1, 256, SCSI_disk);
			SCSI_changephase(SCSI_PHASE_STATUS);
			SCSI_buffer[0] = 0;
		}
	}
}

void SCSI_PutSEL(int newsel)
{
	if (newsel != SCSI_SEL) {
		/* SEL changed state */
		SCSI_SEL = newsel;
		if (SCSI_SEL && SCSI_phase == SCSI_PHASE_SELECTION && SCSI_byte == 0x01) {
			SCSI_changephase(SCSI_PHASE_COMMAND);
		}
		D(printf("changed SEL:%d  SCSI_byte:%2x\n",SCSI_SEL, SCSI_byte));
	}
}

void SCSI_PutACK(int newack)
{
	if (newack != SCSI_ACK) {
		/* ACK changed state */
		SCSI_ACK = newack;
		if (SCSI_ACK) {
			/* REQ goes false when ACK goes true */
			SCSI_REQ = FALSE;
		}
		else {
			/* falling ACK triggers next byte */
			if (SCSI_phase != SCSI_PHASE_SELECTION) {
				SCSI_REQ = TRUE;
				SCSI_nextbyte();
			}
		}
	}
}

UBYTE SCSI_GetByte(void)
{
	return (SCSI_buffer[SCSI_bufpos]);
}

void SCSI_PutByte(UBYTE byte)
{
	SCSI_byte = byte;
}

/*
vim:ts=4:sw=4:
*/

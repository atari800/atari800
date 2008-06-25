/*
 * pbi_mio.c - ICD MIO board emulation
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
#include "pbi_mio.h"
#include "util.h"
#include "log.h"
#include "memory.h"
#include "pia.h"
#include "pbi.h"
#include "cpu.h"
#include "stdlib.h"
#include "pbi_scsi.h"
#include "statesav.h"

#ifdef PBI_DEBUG
#define D(a) a
#else
#define D(a) do{}while(0)
#endif

int PBI_MIO_enabled = FALSE;

static UBYTE *mio_rom;
static int mio_rom_size = 0x4000;
static int mio_ram_bank_offset = 0;
static UBYTE *mio_ram;
static int mio_ram_size = 0x100000;
static UBYTE mio_rom_bank = 0;
static int mio_ram_enabled = FALSE;
static char mio_rom_filename[FILENAME_MAX] = FILENAME_NOT_SET;
static char mio_scsi_disk_filename[FILENAME_MAX] = FILENAME_NOT_SET;
static int mio_scsi_enabled = FALSE;

static void init_mio(void)
{
	free(mio_rom);
	mio_rom = (UBYTE *)Util_malloc(mio_rom_size);
	if (!Atari800_LoadImage(mio_rom_filename, mio_rom, mio_rom_size)) {
		free(mio_rom);
		return;
	}
	D(printf("Loaded mio rom image\n"));
	PBI_MIO_enabled = TRUE;
	if (SCSI_disk != NULL) fclose(SCSI_disk);
	if (strcmp(mio_scsi_disk_filename, FILENAME_NOT_SET)) {
		SCSI_disk = fopen(mio_scsi_disk_filename, "rb+");
		if (SCSI_disk == NULL) {
			Aprint("Error opening SCSI disk image:%s", mio_scsi_disk_filename);
		}
		else {
			D(printf("Opened SCSI disk image\n"));
			mio_scsi_enabled = TRUE;
		}
	}
	if (!mio_scsi_enabled) {
		SCSI_BSY = TRUE; /* makes MIO give up easier */
	}
	free(mio_ram);
	mio_ram = (UBYTE *)Util_malloc(mio_ram_size);
	memset(mio_ram, 0, mio_ram_size);
}


void PBI_MIO_Initialise(int *argc, char *argv[])
{
	int i, j;
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-mio") == 0) {
			init_mio();
		}
		else {
		 	if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-mio             Emulate the ICD MIO board");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;
}

int PBI_MIO_ReadConfig(char *string, char *ptr) 
{
	if (strcmp(string, "MIO_ROM") == 0)
		Util_strlcpy(mio_rom_filename, ptr, sizeof(mio_rom_filename));
	else if (strcmp(string, "MIO_SCSI_DISK") == 0)
		Util_strlcpy(mio_scsi_disk_filename, ptr, sizeof(mio_scsi_disk_filename));
	else return FALSE; /* no match */
	return TRUE; /* matched something */
}

void PBI_MIO_WriteConfig(FILE *fp)
{
	fprintf(fp, "MIO_ROM=%s\n", mio_rom_filename);
	if (strcmp(mio_scsi_disk_filename, FILENAME_NOT_SET)) {
		fprintf(fp, "MIO_SCSI_DISK=%s\n", mio_scsi_disk_filename);
	}
}

/* $D1xx */
UBYTE PBI_MIO_D1_GetByte(UWORD addr)
{
	UBYTE result = 0x00;/*ff*/;
	addr &= 0xffe3; /* 7 mirrors */
	D(printf("MIO Read:%4x  PC:%4x\n", addr, remember_PC[(remember_PC_curpos-1)%REMEMBER_PC_STEPS]));
	if (addr == 0xd1e2) {
		result = ((!SCSI_CD) | (!SCSI_MSG<<1) | (!SCSI_IO<<2) | (!SCSI_BSY<<5) | (!SCSI_REQ<<7));
	}
	else if (addr == 0xd1e1) {
		if (mio_scsi_enabled) {
			result = SCSI_GetByte()^0xff;
			SCSI_PutACK(1);
			SCSI_PutACK(0);
		}
	}
	return result;
}

/* $D1xx */
void PBI_MIO_D1_PutByte(UWORD addr, UBYTE byte)
{
	int old_mio_ram_bank_offset = mio_ram_bank_offset;
	int old_mio_ram_enabled = mio_ram_enabled;
	int offset_changed;
	int ram_enabled_changed;
	addr &= 0xffe3; /* 7 mirrors */
	if (addr == 0xd1e0) {
		/* ram bank A15-A8 */
		mio_ram_bank_offset &= 0xf0000;
		mio_ram_bank_offset |= (byte << 8);
	}
	else if (addr == 0xd1e1) {
		if (mio_scsi_enabled) {
			SCSI_PutByte(byte^0xff);
			SCSI_PutACK(1);
			SCSI_PutACK(0);
		}
	}
	else if (addr == 0xd1e2) {
		/* ram bank A19-A16, ram enable, other stuff */
		mio_ram_bank_offset &= 0x0ffff;
		mio_ram_bank_offset |= ( (byte & 0x0f) <<  16);
		mio_ram_enabled = (byte & 0x20);
		if (mio_scsi_enabled) SCSI_PutSEL(!!(byte & 0x10));
	}
	else if (addr == 0xd1e3) {
		/* or 0xd1ff. rom bank. */
		if (mio_rom_bank != byte){
			int offset = -1;
			if (byte == 4) offset = 0x2000;
			else if (byte == 8) offset = 0x2800;
			else if (byte == 0x10) offset = 0x3000;
			else if (byte == 0x20) offset = 0x3800;
			if (offset != -1) {
				memcpy(memory + 0xd800, mio_rom+offset, 0x800);
				D(printf("mio bank:%2x activated\n", byte));
			}else{
				memcpy(memory + 0xd800, atari_os + 0x1800, 0x800);
				D(printf("Floating point rom activated\n"));

			}
			mio_rom_bank = byte;	
		}
		
	}
	offset_changed = (old_mio_ram_bank_offset != mio_ram_bank_offset);
	ram_enabled_changed = (old_mio_ram_enabled != mio_ram_enabled);
	if (mio_ram_enabled && ram_enabled_changed) {
		/* Copy new page from buffer, overwrite ff page */
		memcpy(memory + 0xd600, mio_ram + mio_ram_bank_offset, 0x100);
	} else if (mio_ram_enabled && offset_changed) {
		/* Copy old page to buffer, copy new page from buffer */
		memcpy(mio_ram + old_mio_ram_bank_offset,memory + 0xd600, 0x100);
		memcpy(memory + 0xd600, mio_ram + mio_ram_bank_offset, 0x100);
	} else if (!mio_ram_enabled && ram_enabled_changed) {
		/* Copy old page to buffer, set new page to ff */
		memcpy(mio_ram + old_mio_ram_bank_offset, memory + 0xd600, 0x100);
		memset(memory + 0xd600, 0xff, 0x100);
	}
	D(printf("MIO Write addr:%4x byte:%2x, cpu:%4x\n", addr, byte,remember_PC[(remember_PC_curpos-1)%REMEMBER_PC_STEPS]));
}

/* MIO RAM page at D600-D6ff */
/* Possible to put code in this ram, so we can't avoid using memory[] */
/* because opcode fetch doesn't call this function */
UBYTE PBI_MIO_D6_GetByte(UWORD addr)
{
	if (!mio_ram_enabled) return 0xff;
	return memory[addr];
}

/* $D6xx */
void PBI_MIO_D6_PutByte(UWORD addr, UBYTE byte)
{
	if (!mio_ram_enabled) return;
	memory[addr]=byte;
}

void PBI_MIOStateSave(void)
{
	SaveINT(&PBI_MIO_enabled, 1);
	if (PBI_MIO_enabled) {
		SaveFNAME(mio_scsi_disk_filename);
		SaveFNAME(mio_rom_filename);
		SaveINT(&mio_ram_size, 1);

		SaveINT(&mio_ram_bank_offset, 1);
		SaveUBYTE(mio_ram, mio_ram_size);
		SaveUBYTE(&mio_rom_bank, 1);
		SaveINT(&mio_ram_enabled, 1);
	}
}

void PBI_MIOStateRead(void)
{
	ReadINT(&PBI_MIO_enabled, 1);
	if (PBI_MIO_enabled) {
		ReadFNAME(mio_scsi_disk_filename);
		ReadFNAME(mio_rom_filename);
		ReadINT(&mio_ram_size, 1);
		init_mio();
		ReadINT(&mio_ram_bank_offset, 1);
		ReadUBYTE(mio_ram, mio_ram_size);
		ReadUBYTE(&mio_rom_bank, 1);
		ReadINT(&mio_ram_enabled, 1);
	}
}

/*
vim:ts=4:sw=4:
*/

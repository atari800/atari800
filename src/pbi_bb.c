/*
 * pbi_bb.c - CSS Black Box emulation
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
#include "pbi_bb.h"
#include "util.h"
#include "log.h"
#include "pbi.h"
#include "memory.h"
#include "pia.h"
#include "pokey.h"
#include "cpu.h"
#include "pbi_scsi.h"
#include "statesav.h"
#include <stdlib.h>

#ifdef PBI_DEBUG
#define D(a) a
#else
#define D(a) do{}while(0)
#endif

/* CSS Black Box emulation */
/* information source: http://www.mathyvannisselroy.nl/bbdoku.txt*/
#define BB_BUTTON_IRQ_MASK 1

int PBI_BB_enabled = FALSE;

static UBYTE *bb_rom;
static int bb_ram_bank_offset = 0;
static UBYTE *bb_ram;
#define BB_RAM_SIZE 0x10000
static UBYTE bb_rom_bank = 0;
static int bb_rom_size;
static int bb_rom_high_bit = 0x00;/*0x10*/
static char bb_rom_filename[FILENAME_MAX] = FILENAME_NOT_SET;
static UBYTE bb_PCR = 0; /* VIA Peripheral control register*/
static int bb_scsi_enabled = FALSE;
static char bb_scsi_disk_filename[FILENAME_MAX] = FILENAME_NOT_SET;

static void init_bb(void)
{
	FILE *bbfp;
	bbfp = fopen(bb_rom_filename,"rb");
	bb_rom_size = Util_flen(bbfp);
	fclose(bbfp);
	if (bb_rom_size != 0x10000 && bb_rom_size != 0x4000) {
		Aprint("Invalid black box rom size\n");
		return;
	}
	bb_rom = (UBYTE *)Util_malloc(bb_rom_size);
	if (!Atari800_LoadImage(bb_rom_filename, bb_rom, bb_rom_size)) {
		free(bb_rom);
		return;
	}
	D(printf("loaded black box rom image\n"));
	PBI_BB_enabled = TRUE;
	if (SCSI_disk != NULL) fclose(SCSI_disk);
	if (strcmp(bb_scsi_disk_filename, FILENAME_NOT_SET)) {
		SCSI_disk = fopen(bb_scsi_disk_filename, "rb+");
		if (SCSI_disk == NULL) {
			Aprint("Error opening BB SCSI disk image:%s", bb_scsi_disk_filename);
		}
		else {
			D(printf("Opened BB SCSI disk image\n"));
			bb_scsi_enabled = TRUE;
		}
	}
	if (!bb_scsi_enabled) {
		SCSI_BSY = TRUE; /* makes BB give up easier? */
	}
	bb_ram = (UBYTE *)Util_malloc(BB_RAM_SIZE);
	memset(bb_ram,0,BB_RAM_SIZE);
}

void PBI_BB_Initialise(int *argc, char *argv[])
{
	int i, j;
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-bb") == 0) {
			init_bb();
		}
		else {
		 	if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-bb              Emulate the CSS Black Box");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;
}

int PBI_BB_ReadConfig(char *string, char *ptr) 
{
	if (strcmp(string, "BLACK_BOX_ROM") == 0)
		Util_strlcpy(bb_rom_filename, ptr, sizeof(bb_rom_filename));
	else if (strcmp(string, "BB_SCSI_DISK") == 0)
		Util_strlcpy(bb_scsi_disk_filename, ptr, sizeof(bb_scsi_disk_filename));
	else return FALSE; /* no match */
	return TRUE; /* matched something */
}

void PBI_BB_WriteConfig(FILE *fp)
{
	fprintf(fp, "BLACK_BOX_ROM=%s\n", bb_rom_filename);
	if (strcmp(bb_scsi_disk_filename, FILENAME_NOT_SET)) {
		fprintf(fp, "BB_SCSI_DISK=%s\n", bb_scsi_disk_filename);
	}
}

UBYTE PBI_BB_D1_GetByte(UWORD addr)
{
	UBYTE result = 0x00;/*ff;*/
	if (addr == 0xd1be) result = 0xff;
	else if (addr == 0xd170) {
		/* status */
		result = ((!(SCSI_REQ))<<7)|((!SCSI_BSY)<<6)|((!SCSI_SEL)<<2)|((!SCSI_CD)<<1)|(!SCSI_IO);
	}
	else if (addr == 0xd171) {
		if (bb_scsi_enabled) {
			result = SCSI_GetByte();
			if (((bb_PCR & 0x0e)>>1) == 0x04) {
				/* handshake output */
				SCSI_PutACK(1);
				SCSI_PutACK(0);
			}
		}
	}
	else if (addr == 0xd1bc) result = (bb_ram_bank_offset >> 8);/*RAMPAGE*/
	else if (addr == 0xd1be) {
		/* SWITCH */
		result = 0x02;
	}
	else if (addr == 0xd1ff) result = PBI_IRQ ? (0x08 | 0x02) : 0 ;
	D(if (addr!=0xd1ff) printf("BB Read:%4x  PC:%4x byte=%2x\n", addr, remember_PC[(remember_PC_curpos-1)%REMEMBER_PC_STEPS], result));
	return result;
}

void PBI_BB_D1_PutByte(UWORD addr, UBYTE byte)
{
	D(printf("BB Write addr:%4x byte:%2x, cpu:%4x\n", addr, byte, remember_PC[(remember_PC_curpos-1)%REMEMBER_PC_STEPS]));
	if (addr == 0xd170) {
		if (bb_scsi_enabled) SCSI_PutSEL(!(byte&0x04));
	}
	else if (addr == 0xd171) {
		if (bb_scsi_enabled) {
			SCSI_PutByte(byte);
			if (((bb_PCR & 0x0e)>>1) == 0x04) {
				/* handshake output */
				SCSI_PutACK(1);
				SCSI_PutACK(0);
			}
		}
	}
	else if (addr == 0xd17c) { /* PCR */
		bb_PCR = byte;
		if (((bb_PCR & 0x0e)>>1) == 0x06) {
			/* low output */
			if (bb_scsi_enabled) SCSI_PutACK(1);
		}
		else if (((bb_PCR & 0x0e)>>1) == 0x07) {
			/* high output */
			if (bb_scsi_enabled) SCSI_PutACK(0);
		}
	}
	else if (addr == 0xd1bc) {
		/* RAMPAGE */
		/* Copy old page to buffer, Copy new page from buffer */
		memcpy(bb_ram+bb_ram_bank_offset,memory + 0xd600,0x100);
		bb_ram_bank_offset = (byte << 8);
		memcpy(memory + 0xd600, bb_ram+bb_ram_bank_offset, 0x100);
	} 
	else if (addr  == 0xd1be) {
		/* high rom bit */
		if (bb_rom_high_bit != ((byte & 0x04) << 2) && bb_rom_size == 0x10000) {
			/* high bit has changed */
			bb_rom_high_bit = ((byte & 0x04) << 2);
			if (bb_rom_bank > 0 && bb_rom_bank < 8) {
					memcpy(memory + 0xd800, bb_rom + (bb_rom_bank + bb_rom_high_bit)*0x800, 0x800);
					D(printf("black box bank:%2x activated\n", bb_rom_bank+bb_rom_high_bit));
			}
		}
	}
	else if ((addr & 0xffc0) == 0xd1c0) {
		/* byte &= 0x0f; */
		if (bb_rom_bank != byte) {
			/* PDVS (d1ff) rom bank */
			int offset = -1;
			if (bb_rom_size == 0x4000) {
				if (byte >= 8 && byte <= 0x0f) offset = (byte - 8)*0x800;
				else if (byte > 0 && byte < 0x08) offset = byte*0x800;
			}
			else { /* bb_rom_size == 0x10000 */
				if (byte > 0 && byte < 0x10) offset = (byte + bb_rom_high_bit)*0x800;
			}

			if (offset != -1) {
					memcpy(memory + 0xd800, bb_rom + offset, 0x800);
					D(printf("black box bank:%2x activated\n", byte + bb_rom_high_bit));
			}
			else {
					memcpy(memory + 0xd800, atari_os + 0x1800, 0x800);
					if (byte != 0) D(printf("d1ff ERROR: byte=%2x\n", byte));
					D(printf("Floating point rom activated\n"));
			}
			bb_rom_bank = byte;	
		}
	}
}

/* Black Box RAM page at D600-D6ff*/
/* Possible to put code in this ram, so we can't avoid using memory[]
 * because opcode fetch doesn't call this function*/
UBYTE PBI_BB_D6_GetByte(UWORD addr)
{
	return memory[addr];
}

/* $D6xx */
void PBI_BB_D6_PutByte(UWORD addr, UBYTE byte)
{
	memory[addr]=byte;
}

static int buttondown;

void PBI_BB_Menu(void)
{
	if (!PBI_BB_enabled) return;
	if (buttondown == FALSE) {
		D(printf("blackbox button down interrupt generated\n"));
		GenerateIRQ();
		PBI_IRQ |= BB_BUTTON_IRQ_MASK;
		buttondown = TRUE;
	}
}

void PBI_BB_Frame(void)
{
	static int count = 0;
	if (buttondown) {
	 	if (count < 1) count++;
		else {
			D(printf("blackbox button up\n"));
			PBI_IRQ &= ~BB_BUTTON_IRQ_MASK;
			/* update pokey IRQ status */
			POKEY_PutByte(_IRQEN, IRQEN);
			buttondown = FALSE;
			count = 0;
		}
	}
}

void PBI_BBStateSave(void)
{
	SaveINT(&PBI_BB_enabled, 1);
	if (PBI_BB_enabled) {
		SaveFNAME(bb_scsi_disk_filename);
		SaveFNAME(bb_rom_filename);

		SaveINT(&bb_ram_bank_offset, 1);
		SaveUBYTE(bb_ram, BB_RAM_SIZE);
		SaveUBYTE(&bb_rom_bank, 1);
		SaveINT(&bb_rom_high_bit, 1);
		SaveUBYTE(&bb_PCR, 1);
	}
}

void PBI_BBStateRead(void)
{
	ReadINT(&PBI_BB_enabled, 1);
	if (PBI_BB_enabled) {
		ReadFNAME(bb_scsi_disk_filename);
		ReadFNAME(bb_rom_filename);
		init_bb();
		ReadINT(&bb_ram_bank_offset, 1);
		ReadUBYTE(bb_ram, BB_RAM_SIZE);
		ReadUBYTE(&bb_rom_bank, 1);
		ReadINT(&bb_rom_high_bit, 1);
		ReadUBYTE(&bb_PCR, 1);
	}
}

/*
vim:ts=4:sw=4:
*/

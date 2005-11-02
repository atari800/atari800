/*
 * memory.c - memory emulation
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
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "antic.h"
#include "cpu.h"
#include "cartridge.h"
#include "gtia.h"
#include "log.h"
#include "memory.h"
#include "pbi.h"
#include "pia.h"
#include "pokey.h"
#include "util.h"
#ifndef BASIC
#include "statesav.h"
#endif

UBYTE memory[65536 + 2];

#ifndef PAGED_ATTRIB

UBYTE attrib[65536];

#else /* PAGED_ATTRIB */

rdfunc readmap[256];
wrfunc writemap[256];

typedef struct map_save {
	int     code;
	rdfunc  rdptr;
	wrfunc  wrptr;
} map_save;

void ROM_PutByte(UWORD addr, UBYTE value)
{
}

map_save save_map[2] = {
	{0, NULL, NULL},          /* RAM */
	{1, NULL, ROM_PutByte}    /* ROM */
};

#endif /* PAGED_ATTRIB */

static UBYTE under_atarixl_os[16384];
static UBYTE under_atari_basic[8192];
static UBYTE *atarixe_memory = NULL;
static ULONG atarixe_memory_size = 0;

int have_basic = FALSE; /* Atari BASIC image has been successfully read (Atari 800 only) */

extern const UBYTE *antic_xe_ptr;	/* Separate ANTIC access to extended memory */

static void AllocXEMemory(void)
{
	if (ram_size > 64) {
		/* don't count 64 KB of base memory */
		/* count number of 16 KB banks, add 1 for saving base memory 0x4000-0x7fff */
		ULONG size = (1 + (ram_size - 64) / 16) * 16384;
		if (size != atarixe_memory_size) {
			if (atarixe_memory != NULL)
				free(atarixe_memory);
			atarixe_memory = (UBYTE *) Util_malloc(size);
			atarixe_memory_size = size;
			memset(atarixe_memory, 0, size);
		}
	}
	/* atarixe_memory not needed, free it */
	else if (atarixe_memory != NULL) {
		free(atarixe_memory);
		atarixe_memory = NULL;
		atarixe_memory_size = 0;
	}
}

void MEMORY_InitialiseMachine(void)
{
	antic_xe_ptr = NULL;
	switch (machine_type) {
	case MACHINE_OSA:
	case MACHINE_OSB:
		memcpy(memory + 0xd800, atari_os, 0x2800);
		Atari800_PatchOS();
		dFillMem(0x0000, 0x00, ram_size * 1024 - 1);
		SetRAM(0x0000, ram_size * 1024 - 1);
		if (ram_size < 52) {
			dFillMem(ram_size * 1024, 0xff, 0xd000 - ram_size * 1024);
			SetROM(ram_size * 1024, 0xcfff);
		}
#ifndef PAGED_ATTRIB
		SetHARDWARE(0xd000, 0xd7ff);
#else
		readmap[0xd0] = GTIA_GetByte;
		readmap[0xd1] = PBI_GetByte;
		readmap[0xd2] = POKEY_GetByte;
		readmap[0xd3] = PIA_GetByte;
		readmap[0xd4] = ANTIC_GetByte;
		readmap[0xd5] = CART_GetByte;
		readmap[0xd6] = PBIM1_GetByte;
		readmap[0xd7] = PBIM2_GetByte;
		writemap[0xd0] = GTIA_PutByte;
		writemap[0xd1] = PBI_PutByte;
		writemap[0xd2] = POKEY_PutByte;
		writemap[0xd3] = PIA_PutByte;
		writemap[0xd4] = ANTIC_PutByte;
		writemap[0xd5] = CART_PutByte;
		writemap[0xd6] = PBIM1_PutByte;
		writemap[0xd7] = PBIM2_PutByte;
#endif
		SetROM(0xd800, 0xffff);
		break;
	case MACHINE_XLXE:
		memcpy(memory + 0xc000, atari_os, 0x4000);
		Atari800_PatchOS();
		if (ram_size == 16) {
			dFillMem(0x0000, 0x00, 0x4000);
			SetRAM(0x0000, 0x3fff);
			dFillMem(0x4000, 0xff, 0x8000);
			SetROM(0x4000, 0xcfff);
		} else {
			dFillMem(0x0000, 0x00, 0xc000);
			SetRAM(0x0000, 0xbfff);
			SetROM(0xc000, 0xcfff);
		}
#ifndef PAGED_ATTRIB
		SetHARDWARE(0xd000, 0xd7ff);
#else
		readmap[0xd0] = GTIA_GetByte;
		readmap[0xd1] = PBI_GetByte;
		readmap[0xd2] = POKEY_GetByte;
		readmap[0xd3] = PIA_GetByte;
		readmap[0xd4] = ANTIC_GetByte;
		readmap[0xd5] = CART_GetByte;
		readmap[0xd6] = PBIM1_GetByte;
		readmap[0xd7] = PBIM2_GetByte;
		writemap[0xd0] = GTIA_PutByte;
		writemap[0xd1] = PBI_PutByte;
		writemap[0xd2] = POKEY_PutByte;
		writemap[0xd3] = PIA_PutByte;
		writemap[0xd4] = ANTIC_PutByte;
		writemap[0xd5] = CART_PutByte;
		writemap[0xd6] = PBIM1_PutByte;
		writemap[0xd7] = PBIM2_PutByte;
#endif
		SetROM(0xd800, 0xffff);
		break;
	case MACHINE_5200:
		memcpy(memory + 0xf800, atari_os, 0x800);
		dFillMem(0x0000, 0x00, 0xf800);
		SetRAM(0x0000, 0x3fff);
		SetROM(0x4000, 0xffff);
#ifndef PAGED_ATTRIB
		SetHARDWARE(0xc000, 0xc0ff);	/* 5200 GTIA Chip */
		SetHARDWARE(0xd400, 0xd4ff);	/* 5200 ANTIC Chip */
		SetHARDWARE(0xe800, 0xe8ff);	/* 5200 POKEY Chip */
		SetHARDWARE(0xeb00, 0xebff);	/* 5200 POKEY Chip */
#else
		readmap[0xc0] = GTIA_GetByte;
		readmap[0xd4] = ANTIC_GetByte;
		readmap[0xe8] = POKEY_GetByte;
		readmap[0xeb] = POKEY_GetByte;
		writemap[0xc0] = GTIA_PutByte;
		writemap[0xd4] = ANTIC_PutByte;
		writemap[0xe8] = POKEY_PutByte;
		writemap[0xeb] = POKEY_PutByte;
#endif
		break;
	}
	AllocXEMemory();
	Coldstart();
}

#ifndef BASIC

void MemStateSave(UBYTE SaveVerbose)
{
	SaveUBYTE(&memory[0], 65536);
#ifndef PAGED_ATTRIB
	SaveUBYTE(&attrib[0], 65536);
#else
	{
		/* I assume here that consecutive calls to SaveUBYTE()
		   are equivalent to a single call with all the values
		   (i.e. SaveUBYTE() doesn't write any headers). */
		UBYTE attrib_page[256];
		int i;
		for (i = 0; i < 256; i++) {
			if (writemap[i] == NULL)
				memset(attrib_page, RAM, 256);
			else if (writemap[i] == ROM_PutByte)
				memset(attrib_page, ROM, 256);
			else if (i == 0x4f || i == 0x5f || i == 0x8f || i == 0x9f) {
				/* special case: Bounty Bob bank switching registers */
				memset(attrib_page, ROM, 256);
				attrib_page[0xf6] = HARDWARE;
				attrib_page[0xf7] = HARDWARE;
				attrib_page[0xf8] = HARDWARE;
				attrib_page[0xf9] = HARDWARE;
			}
			else {
				memset(attrib_page, HARDWARE, 256);
			}
			SaveUBYTE(&attrib_page[0], 256);
		}
	}
#endif

	if (machine_type == MACHINE_XLXE) {
		if (SaveVerbose != 0)
			SaveUBYTE(&atari_basic[0], 8192);
		SaveUBYTE(&under_atari_basic[0], 8192);

		if (SaveVerbose != 0)
			SaveUBYTE(&atari_os[0], 16384);
		SaveUBYTE(&under_atarixl_os[0], 16384);
	}

	if (ram_size > 64) {
		SaveUBYTE(&atarixe_memory[0], atarixe_memory_size);
		/* a hack that makes state files compatible with previous versions:
           for 130 XE there's written 192 KB of unused data */
		if (ram_size == 128) {
			UBYTE buffer[256];
			int i;
			memset(buffer, 0, 256);
			for (i = 0; i < 192 * 4; i++)
				SaveUBYTE(&buffer[0], 256);
		}
	}

}

void MemStateRead(UBYTE SaveVerbose)
{
	ReadUBYTE(&memory[0], 65536);
#ifndef PAGED_ATTRIB
	ReadUBYTE(&attrib[0], 65536);
#else
	{
		UBYTE attrib_page[256];
		int i;
		for (i = 0; i < 256; i++) {
			ReadUBYTE(&attrib_page[0], 256);
			/* note: 0x40 is intentional here:
			   we want ROM on page 0xd1 if H: patches are enabled */
			switch (attrib_page[0x40]) {
			case RAM:
				readmap[i] = NULL;
				writemap[i] = NULL;
				break;
			case ROM:
				if (i != 0xd1 && attrib_page[0xf6] == HARDWARE) {
					if (i == 0x4f || i == 0x8f) {
						readmap[i] = BountyBob1_GetByte;
						writemap[i] = BountyBob1_PutByte;
					}
					else if (i == 0x5f || i == 0x9f) {
						readmap[i] = BountyBob2_GetByte;
						writemap[i] = BountyBob2_PutByte;
					}
					/* else something's wrong, so we keep current values */
				}
				else {
					readmap[i] = NULL;
					writemap[i] = ROM_PutByte;
				}
				break;
			case HARDWARE:
				switch (i) {
				case 0xc0:
				case 0xd0:
					readmap[i] = GTIA_GetByte;
					writemap[i] = GTIA_PutByte;
					break;
				case 0xd1:
					readmap[i] = PBI_GetByte;
					writemap[i] = PBI_PutByte;
					break;
				case 0xd2:
				case 0xe8:
				case 0xeb:
					readmap[i] = POKEY_GetByte;
					writemap[i] = POKEY_PutByte;
				case 0xd3:
					readmap[i] = PIA_GetByte;
					writemap[i] = PIA_PutByte;
					break;
				case 0xd4:
					readmap[i] = ANTIC_GetByte;
					writemap[i] = ANTIC_PutByte;
					break;
				case 0xd5:
					readmap[i] = CART_GetByte;
					writemap[i] = CART_PutByte;
					break;
				case 0xd6:
					readmap[i] = PBIM1_GetByte;
					readmap[i] = PBIM2_GetByte;
					break;
				case 0xd7:
					writemap[i] = PBIM1_PutByte;
					writemap[i] = PBIM2_PutByte;
					break;
				default:
					/* something's wrong, so we keep current values */
					break;
				}
				break;
			default:
				/* something's wrong, so we keep current values */
				break;
			}
		}
	}
#endif

	if (machine_type == MACHINE_XLXE) {
		if (SaveVerbose != 0)
			ReadUBYTE(&atari_basic[0], 8192);
		ReadUBYTE(&under_atari_basic[0], 8192);

		if (SaveVerbose != 0)
			ReadUBYTE(&atari_os[0], 16384);
		ReadUBYTE(&under_atarixl_os[0], 16384);
	}

	antic_xe_ptr = NULL;
	AllocXEMemory();
	if (ram_size > 64) {
		ReadUBYTE(&atarixe_memory[0], atarixe_memory_size);
		/* a hack that makes state files compatible with previous versions:
           for 130 XE there's written 192 KB of unused data */
		if (ram_size == 128) {
			UBYTE buffer[256];
			int i;
			for (i = 0; i < 192 * 4; i++)
				ReadUBYTE(&buffer[0], 256);
		}
	}

}

#endif /* BASIC */

void CopyFromMem(UWORD from, UBYTE *to, int size)
{
	while (--size >= 0) {
		*to++ = GetByte(from);
		from++;
	}
}

void CopyToMem(const UBYTE *from, UWORD to, int size)
{
	while (--size >= 0) {
		PutByte(to, *from);
		from++;
		to++;
	}
}

/*
 * Returns non-zero, if Atari BASIC is disabled by given PORTB output.
 * Normally BASIC is disabled by setting bit 1, but it's also disabled
 * when using 576K and 1088K memory expansions, where bit 1 is used
 * for selecting extended memory bank number.
 */
static int basic_disabled(UBYTE portb)
{
	return (portb & 0x02) != 0
	 || ((portb & 0x10) == 0 && (ram_size == 576 || ram_size == 1088));
}

/* Note: this function is only for XL/XE! */
void MEMORY_HandlePORTB(UBYTE byte, UBYTE oldval)
{
	/* Switch XE memory bank in 0x4000-0x7fff */
	if (ram_size > 64) {
		int bank = 0;
		/* bank = 0 : base RAM */
		/* bank = 1..64 : extended RAM */
		if ((byte & 0x10) == 0)
			switch (ram_size) {
			case 128:
				bank = ((byte & 0x0c) >> 2) + 1;
				break;
			case RAM_320_RAMBO:
				bank = (((byte & 0x0c) + ((byte & 0x60) >> 1)) >> 2) + 1;
				break;
			case RAM_320_COMPY_SHOP:
				bank = (((byte & 0x0c) + ((byte & 0xc0) >> 2)) >> 2) + 1;
				break;
			case 576:
				bank = (((byte & 0x0e) + ((byte & 0x60) >> 1)) >> 1) + 1;
				break;
			case 1088:
				bank = (((byte & 0x0e) + ((byte & 0xe0) >> 1)) >> 1) + 1;
				break;
			}
		/* Note: in Compy Shop bit 5 (ANTIC access) disables Self Test */
		if (selftest_enabled && (bank != xe_bank || (ram_size == RAM_320_COMPY_SHOP && (byte & 0x20) == 0))) {
			/* Disable Self Test ROM */
			memcpy(memory + 0x5000, under_atarixl_os + 0x1000, 0x800);
			SetRAM(0x5000, 0x57ff);
			selftest_enabled = FALSE;
		}
		if (bank != xe_bank) {
			memcpy(atarixe_memory + (xe_bank << 14), memory + 0x4000, 16384);
			memcpy(memory + 0x4000, atarixe_memory + (bank << 14), 16384);
			xe_bank = bank;
		}
		if (ram_size == 128 || ram_size == RAM_320_COMPY_SHOP)
			switch (byte & 0x30) {
			case 0x20:	/* ANTIC: base, CPU: extended */
				antic_xe_ptr = atarixe_memory;
				break;
			case 0x10:	/* ANTIC: extended, CPU: base */
				if (ram_size == 128)
					antic_xe_ptr = atarixe_memory + ((((byte & 0x0c) >> 2) + 1) << 14);
				else	/* 320 Compy Shop */
					antic_xe_ptr = atarixe_memory + (((((byte & 0x0c) + ((byte & 0xc0) >> 2)) >> 2) + 1) << 14);
				break;
			default:	/* ANTIC same as CPU */
				antic_xe_ptr = NULL;
				break;
			}
	}

	/* Enable/disable OS ROM in 0xc000-0xcfff and 0xd800-0xffff */
	if ((oldval ^ byte) & 0x01) {
		if (byte & 0x01) {
			/* Enable OS ROM */
			if (ram_size > 48) {
				memcpy(under_atarixl_os, memory + 0xc000, 0x1000);
				memcpy(under_atarixl_os + 0x1800, memory + 0xd800, 0x2800);
				SetROM(0xc000, 0xcfff);
				SetROM(0xd800, 0xffff);
			}
			memcpy(memory + 0xc000, atari_os, 0x1000);
			memcpy(memory + 0xd800, atari_os + 0x1800, 0x2800);
			Atari800_PatchOS();
		}
		else {
			/* Disable OS ROM */
			if (ram_size > 48) {
				memcpy(memory + 0xc000, under_atarixl_os, 0x1000);
				memcpy(memory + 0xd800, under_atarixl_os + 0x1800, 0x2800);
				SetRAM(0xc000, 0xcfff);
				SetRAM(0xd800, 0xffff);
			} else {
				dFillMem(0xc000, 0xff, 0x1000);
				dFillMem(0xd800, 0xff, 0x2800);
			}
			/* When OS ROM is disabled we also have to disable Self Test - Jindroush */
			if (selftest_enabled) {
				if (ram_size > 20) {
					memcpy(memory + 0x5000, under_atarixl_os + 0x1000, 0x800);
					SetRAM(0x5000, 0x57ff);
				}
				else
					dFillMem(0x5000, 0xff, 0x800);
				selftest_enabled = FALSE;
			}
		}
	}

	/* Enable/disable BASIC ROM in 0xa000-0xbfff */
	if (!cartA0BF_enabled) {
		/* BASIC is disabled if bit 1 set or accessing extended 576K or 1088K memory */
		int now_disabled = basic_disabled(byte);
		if (basic_disabled(oldval) != now_disabled) {
			if (now_disabled) {
				/* Disable BASIC ROM */
				if (ram_size > 40) {
					memcpy(memory + 0xa000, under_atari_basic, 0x2000);
					SetRAM(0xa000, 0xbfff);
				}
				else
					dFillMem(0xa000, 0xff, 0x2000);
			}
			else {
				/* Enable BASIC ROM */
				if (ram_size > 40) {
					memcpy(under_atari_basic, memory + 0xa000, 0x2000);
					SetROM(0xa000, 0xbfff);
				}
				memcpy(memory + 0xa000, atari_basic, 0x2000);
			}
		}
	}

	/* Enable/disable Self Test ROM in 0x5000-0x57ff */
	if (byte & 0x80) {
		if (selftest_enabled) {
			/* Disable Self Test ROM */
			if (ram_size > 20) {
				memcpy(memory + 0x5000, under_atarixl_os + 0x1000, 0x800);
				SetRAM(0x5000, 0x57ff);
			}
			else
				dFillMem(0x5000, 0xff, 0x800);
			selftest_enabled = FALSE;
		}
	}
	else {
		/* We can enable Self Test only if the OS ROM is enabled */
		/* and we're not accessing extended 320K Compy Shop or 1088K memory */
		/* Note: in Compy Shop bit 5 (ANTIC access) disables Self Test */
		if (!selftest_enabled && (byte & 0x01)
		&& !((byte & 0x30) != 0x30 && ram_size == RAM_320_COMPY_SHOP)
		&& !((byte & 0x10) == 0 && ram_size == 1088)) {
			/* Enable Self Test ROM */
			if (ram_size > 20) {
				memcpy(under_atarixl_os + 0x1000, memory + 0x5000, 0x800);
				SetROM(0x5000, 0x57ff);
			}
			memcpy(memory + 0x5000, atari_os + 0x1000, 0x800);
			selftest_enabled = TRUE;
		}
	}
}

static int cart809F_enabled = FALSE;
int cartA0BF_enabled = FALSE;
static UBYTE under_cart809F[8192];
static UBYTE under_cartA0BF[8192];

void Cart809F_Disable(void)
{
	if (cart809F_enabled) {
		if (ram_size > 32) {
			memcpy(memory + 0x8000, under_cart809F, 0x2000);
			SetRAM(0x8000, 0x9fff);
		}
		else
			dFillMem(0x8000, 0xff, 0x2000);
		cart809F_enabled = FALSE;
	}
}

void Cart809F_Enable(void)
{
	if (!cart809F_enabled) {
		if (ram_size > 32) {
			memcpy(under_cart809F, memory + 0x8000, 0x2000);
			SetROM(0x8000, 0x9fff);
		}
		cart809F_enabled = TRUE;
	}
}

void CartA0BF_Disable(void)
{
	if (cartA0BF_enabled) {
		/* No BASIC if not XL/XE or bit 1 of PORTB set */
		/* or accessing extended 576K or 1088K memory */
		if ((machine_type != MACHINE_XLXE) || basic_disabled((UBYTE) (PORTB | PORTB_mask))) {
			if (ram_size > 40) {
				memcpy(memory + 0xa000, under_cartA0BF, 0x2000);
				SetRAM(0xa000, 0xbfff);
			}
			else
				dFillMem(0xa000, 0xff, 0x2000);
		}
		else
			memcpy(memory + 0xa000, atari_basic, 0x2000);
		cartA0BF_enabled = FALSE;
		if (machine_type == MACHINE_XLXE) {
			TRIG[3] = 0;
			if (GRACTL & 4)
				TRIG_latch[3] = 0;
		}
	}
}

void CartA0BF_Enable(void)
{
	if (!cartA0BF_enabled) {
		/* No BASIC if not XL/XE or bit 1 of PORTB set */
		/* or accessing extended 576K or 1088K memory */
		if (ram_size > 40 && ((machine_type != MACHINE_XLXE) || (PORTB & 0x02)
		|| ((PORTB & 0x10) == 0 && (ram_size == 576 || ram_size == 1088)))) {
			/* Back-up 0xa000-0xbfff RAM */
			memcpy(under_cartA0BF, memory + 0xa000, 0x2000);
			SetROM(0xa000, 0xbfff);
		}
		cartA0BF_enabled = TRUE;
		if (machine_type == MACHINE_XLXE)
			TRIG[3] = 1;
	}
}

void get_charset(UBYTE *cs)
{
	const UBYTE *p;
	switch (machine_type) {
	case MACHINE_OSA:
	case MACHINE_OSB:
		p = memory + 0xe000;
		break;
	case MACHINE_XLXE:
		p = atari_os + 0x2000;
		break;
	case MACHINE_5200:
		p = memory + 0xf800;
		break;
	default:
		/* shouldn't happen */
		return;
	}
	/* copy font, but change screencode order to ATASCII order */
	memcpy(cs, p + 0x200, 0x100); /* control chars */
	memcpy(cs + 0x100, p, 0x200); /* !"#$..., uppercase letters */
	memcpy(cs + 0x300, p + 0x300, 0x100); /* lowercase letters */
}

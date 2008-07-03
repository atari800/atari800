/*
 * cartridge.c - cartridge emulation
 *
 * Copyright (C) 2001-2006 Piotr Fusik
 * Copyright (C) 2001-2006 Atari800 development team (see DOC/CREDITS)
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "binload.h" /* BINLOAD_loading_basic */
#include "cartridge.h"
#include "memory.h"
#include "pia.h"
#include "rtime.h"
#include "util.h"
#ifndef BASIC
#include "statesav.h"
#endif

int cart_kb[CART_LAST_SUPPORTED + 1] = {
	0,
	8,    /* CART_STD_8 */
	16,   /* CART_STD_16 */
	16,   /* CART_OSS_16 */
	32,   /* CART_5200_32 */
	32,   /* CART_DB_32 */
	16,   /* CART_5200_EE_16 */
	40,   /* CART_5200_40 */
	64,   /* CART_WILL_64 */
	64,   /* CART_EXP_64 */
	64,   /* CART_DIAMOND_64 */
	64,   /* CART_SDX */
	32,   /* CART_XEGS_32 */
	64,   /* CART_XEGS_64 */
	128,  /* CART_XEGS_128 */
	16,   /* CART_OSS2_16 */
	16,   /* CART_5200_NS_16 */
	128,  /* CART_ATRAX_128 */
	40,   /* CART_BBSB_40 */
	8,    /* CART_5200_8 */
	4,    /* CART_5200_4 */
	8,    /* CART_RIGHT_8 */
	32,   /* CART_WILL_32 */
	256,  /* CART_XEGS_256 */
	512,  /* CART_XEGS_512 */
	1024, /* CART_XEGS_1024 */
	16,   /* CART_MEGA_16 */
	32,   /* CART_MEGA_32 */
	64,   /* CART_MEGA_64 */
	128,  /* CART_MEGA_128 */
	256,  /* CART_MEGA_256 */
	512,  /* CART_MEGA_512 */
	1024, /* CART_MEGA_1024 */
	32,   /* CART_SWXEGS_32 */
	64,   /* CART_SWXEGS_64 */
	128,  /* CART_SWXEGS_128 */
	256,  /* CART_SWXEGS_256 */
	512,  /* CART_SWXEGS_512 */
	1024, /* CART_SWXEGS_1024 */
	8,    /* CART_PHOENIX_8 */
	16,   /* CART_BLIZZARD_16 */
	128,  /* CART_ATMAX_128 */
	1024, /* CART_ATMAX_1024 */
	128   /* CART_SDX_128 */
};

int CART_IsFor5200(int type)
{
	switch (type) {
	case CART_5200_32:
	case CART_5200_EE_16:
	case CART_5200_40:
	case CART_5200_NS_16:
	case CART_5200_8:
	case CART_5200_4:
		return TRUE;
	default:
		break;
	}
	return FALSE;
}

UBYTE *cart_image = NULL;		/* cartridge memory */
char cart_filename[FILENAME_MAX];
int cart_type = CART_NONE;

static int bank;

/* DB_32, XEGS_32, XEGS_64, XEGS_128, XEGS_256, XEGS_512, XEGS_1024,
   SWXEGS_32, SWXEGS_64, SWXEGS_128, SWXEGS_256, SWXEGS_512, SWXEGS_1024 */
static void set_bank_809F(int b, int main)
{
	if (b != bank) {
		if (b & 0x80) {
			Cart809F_Disable();
			CartA0BF_Disable();
		}
		else {
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image + b * 0x2000);
			if (bank & 0x80)
				CopyROM(0xa000, 0xbfff, cart_image + main);
		}
		bank = b;
	}
}

/* OSS_16, OSS2_16 */
static void set_bank_A0AF(int b, int main)
{
	if (b != bank) {
		if (b < 0)
			CartA0BF_Disable();
		else {
			CartA0BF_Enable();
			CopyROM(0xa000, 0xafff, cart_image + b * 0x1000);
			if (bank < 0)
				CopyROM(0xb000, 0xbfff, cart_image + main);
		}
		bank = b;
	}
}

/* WILL_64, EXP_64, DIAMOND_64, SDX_64, WILL_32, ATMAX_128, ATMAX_1024,
   ATRAX_128 */
static void set_bank_A0BF(int b, int n)
{
	if (b != bank) {
		if (b & n)
			CartA0BF_Disable();
		else {
			CartA0BF_Enable();
			CopyROM(0xa000, 0xbfff, cart_image + (b & (n - 1)) * 0x2000);
		}
		bank = b;
	}
}

static void set_bank_80BF(int b)
{
	if (b != bank) {
		if (b & 0x80) {
			Cart809F_Disable();
			CartA0BF_Disable();
		}
		else {
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0xbfff, cart_image + b * 0x4000);
		}
		bank = b;
	}
}

/* an access (read or write) to D500-D5FF area */
static void CART_Access(UWORD addr)
{
	int b = bank;
	switch (cart_type) {
	case CART_OSS_16:
		if (addr & 0x08)
			b = -1;
		else
			switch (addr & 0x07) {
			case 0x00:
			case 0x01:
				b = 0;
				break;
			case 0x03:
			case 0x07:
				b = 1;
				break;
			case 0x04:
			case 0x05:
				b = 2;
				break;
			/* case 0x02:
			case 0x06: */
			default:
				break;
			}
		set_bank_A0AF(b, 0x3000);
		break;
	case CART_DB_32:
		set_bank_809F(addr & 0x03, 0x6000);
		break;
	case CART_WILL_64:
		set_bank_A0BF(addr, 8);
		break;
	case CART_WILL_32:
		set_bank_A0BF(addr & 0xb, 8);
		break;
	case CART_EXP_64:
		if ((addr & 0xf0) == 0x70)
			set_bank_A0BF(addr ^ 7, 8);
		break;
	case CART_DIAMOND_64:
		if ((addr & 0xf0) == 0xd0)
			set_bank_A0BF(addr ^ 7, 8);
		break;
	case CART_SDX_64:
		if ((addr & 0xf0) == 0xe0)
			set_bank_A0BF(addr ^ 7, 8);
		break;
	case CART_SDX_128:
		if ((addr & 0xe0) == 0xe0 && addr != bank) {
			if (addr & 8)
				CartA0BF_Disable();
			else {
				CartA0BF_Enable();
				CopyROM(0xa000, 0xbfff,
					cart_image + (((addr & 7) + ((addr & 0x10) >> 1)) ^ 0xf) * 0x2000);
			}
			bank = addr;
		}
		break;
	case CART_OSS2_16:
		switch (addr & 0x09) {
		case 0x00:
			b = 1;
			break;
		case 0x01:
			b = 3;
			break;
		case 0x08:
			b = -1;
			break;
		case 0x09:
			b = 2;
			break;
		}
		set_bank_A0AF(b, 0x0000);
		break;
	case CART_PHOENIX_8:
		CartA0BF_Disable();
		break;
	case CART_BLIZZARD_16:
		Cart809F_Disable();
		CartA0BF_Disable();
		break;
	case CART_ATMAX_128:
		if ((addr & 0xe0) == 0)
			set_bank_A0BF(addr, 16);
		break;
	case CART_ATMAX_1024:
		set_bank_A0BF(addr, 128);
		break;
	default:
		break;
	}
}

/* a read from D500-D5FF area */
UBYTE CART_GetByte(UWORD addr)
{
	if (RTIME_enabled && (addr == 0xd5b8 || addr == 0xd5b9))
		return RTIME_GetByte();
	CART_Access(addr);
	return 0xff;
}

/* a write to D500-D5FF area */
void CART_PutByte(UWORD addr, UBYTE byte)
{
	if (RTIME_enabled && (addr == 0xd5b8 || addr == 0xd5b9)) {
		RTIME_PutByte(byte);
		return;
	}
	switch (cart_type) {
	case CART_XEGS_32:
		set_bank_809F(byte & 0x03, 0x6000);
		break;
	case CART_XEGS_64:
		set_bank_809F(byte & 0x07, 0xe000);
		break;
	case CART_XEGS_128:
		set_bank_809F(byte & 0x0f, 0x1e000);
		break;
	case CART_XEGS_256:
		set_bank_809F(byte & 0x1f, 0x3e000);
		break;
	case CART_XEGS_512:
		set_bank_809F(byte & 0x3f, 0x7e000);
		break;
	case CART_XEGS_1024:
		set_bank_809F(byte & 0x7f, 0xfe000);
		break;
	case CART_ATRAX_128:
		set_bank_A0BF(byte & 0x8f, 128);
		break;
	case CART_MEGA_16:
		set_bank_80BF(byte & 0x80);
		break;
	case CART_MEGA_32:
		set_bank_80BF(byte & 0x81);
		break;
	case CART_MEGA_64:
		set_bank_80BF(byte & 0x83);
		break;
	case CART_MEGA_128:
		set_bank_80BF(byte & 0x87);
		break;
	case CART_MEGA_256:
		set_bank_80BF(byte & 0x8f);
		break;
	case CART_MEGA_512:
		set_bank_80BF(byte & 0x9f);
		break;
	case CART_MEGA_1024:
		set_bank_80BF(byte & 0xbf);
		break;
	case CART_SWXEGS_32:
		set_bank_809F(byte & 0x83, 0x6000);
		break;
	case CART_SWXEGS_64:
		set_bank_809F(byte & 0x87, 0xe000);
		break;
	case CART_SWXEGS_128:
		set_bank_809F(byte & 0x8f, 0x1e000);
		break;
	case CART_SWXEGS_256:
		set_bank_809F(byte & 0x9f, 0x3e000);
		break;
	case CART_SWXEGS_512:
		set_bank_809F(byte & 0xbf, 0x7e000);
		break;
	case CART_SWXEGS_1024:
		set_bank_809F(byte, 0xfe000);
		break;
	default:
		CART_Access(addr);
		break;
	}
}

/* special support of Bounty Bob on Atari5200 */
void CART_BountyBob1(UWORD addr)
{
	if (machine_type == MACHINE_5200) {
		if (addr >= 0x4ff6 && addr <= 0x4ff9) {
			addr -= 0x4ff6;
			CopyROM(0x4000, 0x4fff, cart_image + addr * 0x1000);
		}
	} else {
		if (addr >= 0x8ff6 && addr <= 0x8ff9) {
			addr -= 0x8ff6;
			CopyROM(0x8000, 0x8fff, cart_image + addr * 0x1000);
		}
	}
}

void CART_BountyBob2(UWORD addr)
{
	if (machine_type == MACHINE_5200) {
		if (addr >= 0x5ff6 && addr <= 0x5ff9) {
			addr -= 0x5ff6;
			CopyROM(0x5000, 0x5fff, cart_image + 0x4000 + addr * 0x1000);
		}
	}
	else {
		if (addr >= 0x9ff6 && addr <= 0x9ff9) {
			addr -= 0x9ff6;
			CopyROM(0x9000, 0x9fff, cart_image + 0x4000 + addr * 0x1000);
		}
	}
}

#ifdef PAGED_ATTRIB
UBYTE BountyBob1_GetByte(UWORD addr)
{
	if (machine_type == MACHINE_5200) {
		if (addr >= 0x4ff6 && addr <= 0x4ff9) {
			CART_BountyBob1(addr);
			return 0;
		}
	} else {
		if (addr >= 0x8ff6 && addr <= 0x8ff9) {
			CART_BountyBob1(addr);
			return 0;
		}
	}
	return dGetByte(addr);
}

UBYTE BountyBob2_GetByte(UWORD addr)
{
	if (machine_type == MACHINE_5200) {
		if (addr >= 0x5ff6 && addr <= 0x5ff9) {
			CART_BountyBob2(addr);
			return 0;
		}
	} else {
		if (addr >= 0x9ff6 && addr <= 0x9ff9) {
			CART_BountyBob2(addr);
			return 0;
		}
	}
	return dGetByte(addr);
}

void BountyBob1_PutByte(UWORD addr, UBYTE value)
{
	if (machine_type == MACHINE_5200) {
		if (addr >= 0x4ff6 && addr <= 0x4ff9) {
			CART_BountyBob1(addr);
		}
	} else {
		if (addr >= 0x8ff6 && addr <= 0x8ff9) {
			CART_BountyBob1(addr);
		}
	}
}

void BountyBob2_PutByte(UWORD addr, UBYTE value)
{
	if (machine_type == MACHINE_5200) {
		if (addr >= 0x5ff6 && addr <= 0x5ff9) {
			CART_BountyBob2(addr);
		}
	} else {
		if (addr >= 0x9ff6 && addr <= 0x9ff9) {
			CART_BountyBob2(addr);
		}
	}
}
#endif

int CART_Checksum(const UBYTE *image, int nbytes)
{
	int checksum = 0;
	while (nbytes > 0) {
		checksum += *image++;
		nbytes--;
	}
	return checksum;
}

int CART_Insert(const char *filename)
{
	FILE *fp;
	int len;
	int type;
	UBYTE header[16];

	/* remove currently inserted cart */
	CART_Remove();

	/* open file */
	fp = fopen(filename, "rb");
	if (fp == NULL)
		return CART_CANT_OPEN;
	/* check file length */
	len = Util_flen(fp);
	Util_rewind(fp);

	/* Save Filename for state save */
	strcpy(cart_filename, filename);

	/* if full kilobytes, assume it is raw image */
	if ((len & 0x3ff) == 0) {
		/* alloc memory and read data */
		cart_image = (UBYTE *) Util_malloc(len);
		fread(cart_image, 1, len, fp);
		fclose(fp);
		/* find cart type */
		cart_type = CART_NONE;
		len >>= 10;	/* number of kilobytes */
		for (type = 1; type <= CART_LAST_SUPPORTED; type++)
			if (cart_kb[type] == len) {
				if (cart_type == CART_NONE)
					cart_type = type;
				else
					return len;	/* more than one cartridge type of such length - user must select */
			}
		if (cart_type != CART_NONE) {
			CART_Start();
			return 0;	/* ok */
		}
		free(cart_image);
		cart_image = NULL;
		return CART_BAD_FORMAT;
	}
	/* if not full kilobytes, assume it is CART file */
	fread(header, 1, 16, fp);
	if ((header[0] == 'C') &&
		(header[1] == 'A') &&
		(header[2] == 'R') &&
		(header[3] == 'T')) {
		type = (header[4] << 24) |
			(header[5] << 16) |
			(header[6] << 8) |
			header[7];
		if (type >= 1 && type <= CART_LAST_SUPPORTED) {
			int checksum;
			len = cart_kb[type] << 10;
			/* alloc memory and read data */
			cart_image = (UBYTE *) Util_malloc(len);
			fread(cart_image, 1, len, fp);
			fclose(fp);
			checksum = (header[8] << 24) |
				(header[9] << 16) |
				(header[10] << 8) |
				header[11];
			cart_type = type;
			CART_Start();
			return checksum == CART_Checksum(cart_image, len) ? 0 : CART_BAD_CHECKSUM;
		}
	}
	fclose(fp);
	return CART_BAD_FORMAT;
}

void CART_Remove(void)
{
	cart_type = CART_NONE;
	if (cart_image != NULL) {
		free(cart_image);
		cart_image = NULL;
	}
	CART_Start();
}

void CART_Start(void) {
	if (machine_type == MACHINE_5200) {
		SetROM(0x4ff6, 0x4ff9);		/* disable Bounty Bob bank switching */
		SetROM(0x5ff6, 0x5ff9);
		switch (cart_type) {
		case CART_5200_32:
			CopyROM(0x4000, 0xbfff, cart_image);
			break;
		case CART_5200_EE_16:
			CopyROM(0x4000, 0x5fff, cart_image);
			CopyROM(0x6000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0x2000);
			break;
		case CART_5200_40:
			CopyROM(0x4000, 0x4fff, cart_image);
			CopyROM(0x5000, 0x5fff, cart_image + 0x4000);
			CopyROM(0x8000, 0x9fff, cart_image + 0x8000);
			CopyROM(0xa000, 0xbfff, cart_image + 0x8000);
#ifndef PAGED_ATTRIB
			SetHARDWARE(0x4ff6, 0x4ff9);
			SetHARDWARE(0x5ff6, 0x5ff9);
#else
			readmap[0x4f] = BountyBob1_GetByte;
			readmap[0x5f] = BountyBob2_GetByte;
			writemap[0x4f] = BountyBob1_PutByte;
			writemap[0x5f] = BountyBob2_PutByte;
#endif
			break;
		case CART_5200_NS_16:
			CopyROM(0x8000, 0xbfff, cart_image);
			break;
		case CART_5200_8:
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image);
			break;
		case CART_5200_4:
			CopyROM(0x8000, 0x8fff, cart_image);
			CopyROM(0x9000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xafff, cart_image);
			CopyROM(0xb000, 0xbfff, cart_image);
			break;
		default:
			/* clear cartridge area so the 5200 will crash */
			dFillMem(0x4000, 0, 0x8000);
			break;
		}
	}
	else {
		switch (cart_type) {
		case CART_STD_8:
		case CART_PHOENIX_8:
			Cart809F_Disable();
			CartA0BF_Enable();
			CopyROM(0xa000, 0xbfff, cart_image);
			break;
		case CART_STD_16:
		case CART_BLIZZARD_16:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0xbfff, cart_image);
			break;
		case CART_OSS_16:
			Cart809F_Disable();
			CartA0BF_Enable();
			CopyROM(0xa000, 0xafff, cart_image);
			CopyROM(0xb000, 0xbfff, cart_image + 0x3000);
			bank = 0;
			break;
		case CART_DB_32:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0x6000);
			bank = 0;
			break;
		case CART_WILL_64:
		case CART_WILL_32:
		case CART_EXP_64:
		case CART_DIAMOND_64:
		case CART_SDX_64:
		case CART_SDX_128:
			Cart809F_Disable();
			CartA0BF_Enable();
			CopyROM(0xa000, 0xbfff, cart_image);
			bank = 0;
			break;
		case CART_XEGS_32:
		case CART_SWXEGS_32:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0x6000);
			bank = 0;
			break;
		case CART_XEGS_64:
		case CART_SWXEGS_64:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0xe000);
			bank = 0;
			break;
		case CART_XEGS_128:
		case CART_SWXEGS_128:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0x1e000);
			bank = 0;
			break;
		case CART_XEGS_256:
		case CART_SWXEGS_256:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0x3e000);
			bank = 0;
			break;
		case CART_XEGS_512:
		case CART_SWXEGS_512:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0x7e000);
			bank = 0;
			break;
		case CART_XEGS_1024:
		case CART_SWXEGS_1024:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0xfe000);
			bank = 0;
			break;
		case CART_OSS2_16:
			Cart809F_Disable();
			CartA0BF_Enable();
			CopyROM(0xa000, 0xafff, cart_image + 0x1000);
			CopyROM(0xb000, 0xbfff, cart_image);
			bank = 0;
			break;
		case CART_ATRAX_128:
			Cart809F_Disable();
			CartA0BF_Enable();
			CopyROM(0xa000, 0xbfff, cart_image);
			bank = 0;
			break;
		case CART_BBSB_40:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x8fff, cart_image);
			CopyROM(0x9000, 0x9fff, cart_image + 0x4000);
			CopyROM(0xa000, 0xbfff, cart_image + 0x8000);
#ifndef PAGED_ATTRIB
			SetHARDWARE(0x8ff6, 0x8ff9);
			SetHARDWARE(0x9ff6, 0x9ff9);
#else
			readmap[0x8f] = BountyBob1_GetByte;
			readmap[0x9f] = BountyBob2_GetByte;
			writemap[0x8f] = BountyBob1_PutByte;
			writemap[0x9f] = BountyBob2_PutByte;
#endif
			break;
		case CART_RIGHT_8:
			if (machine_type == MACHINE_OSA || machine_type == MACHINE_OSB) {
				Cart809F_Enable();
				CopyROM(0x8000, 0x9fff, cart_image);
				if ((!disable_basic || BINLOAD_loading_basic) && have_basic) {
					CartA0BF_Enable();
					CopyROM(0xa000, 0xbfff, atari_basic);
					break;
				}
				CartA0BF_Disable();
				break;
			}
			/* there's no right slot in XL/XE */
			Cart809F_Disable();
			CartA0BF_Disable();
			break;
		case CART_MEGA_16:
		case CART_MEGA_32:
		case CART_MEGA_64:
		case CART_MEGA_128:
		case CART_MEGA_256:
		case CART_MEGA_512:
		case CART_MEGA_1024:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0xbfff, cart_image);
			bank = 0;
			break;
		case CART_ATMAX_128:
			Cart809F_Disable();
			CartA0BF_Enable();
			CopyROM(0xa000, 0xbfff, cart_image);
			bank = 0;
			break;
		case CART_ATMAX_1024:
			Cart809F_Disable();
			CartA0BF_Enable();
			CopyROM(0xa000, 0xbfff, cart_image + 0xfe000);
			bank = 0x7f;
			break;
		default:
			Cart809F_Disable();
			if ((machine_type == MACHINE_OSA || machine_type == MACHINE_OSB)
			 && (!disable_basic || BINLOAD_loading_basic) && have_basic) {
				CartA0BF_Enable();
				CopyROM(0xa000, 0xbfff, atari_basic);
				break;
			}
			CartA0BF_Disable();
			break;
		}
	}
}

#ifndef BASIC

void CARTStateRead(void)
{
	int savedCartType = CART_NONE;

	/* Read the cart type from the file.  If there is no cart type, becaused we have
	   reached the end of the file, this will just default to CART_NONE */
	StateSav_ReadINT(&savedCartType, 1);
	if (savedCartType != CART_NONE) {
		char filename[FILENAME_MAX];
		StateSav_ReadFNAME(filename);
		if (filename[0]) {
			/* Insert the cartridge... */
			if (CART_Insert(filename) >= 0) {
				/* And set the type to the saved type, in case it was a raw cartridge image */
				cart_type = savedCartType;
				CART_Start();
			}
		}
	}
}

void CARTStateSave(void)
{
	/* Save the cartridge type, or CART_NONE if there isn't one...*/
	StateSav_SaveINT(&cart_type, 1);
	if (cart_type != CART_NONE) {
		StateSav_SaveFNAME(cart_filename);
	}
}

#endif

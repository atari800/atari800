#include <stdio.h>
#include <stdlib.h>

#include "atari.h"
#include "cartridge.h"
#include "memory.h"
#include "pia.h"
#include "rt-config.h"
#include "rtime.h"

int cart_kb[CART_LAST_SUPPORTED + 1] = {
	0,
	8,  /* CART_STD_8 */
	16, /* CART_STD_16 */
	16, /* CART_OSS_16 */
	32, /* CART_5200_32 */
	32, /* CART_DB_32 */
	16, /* CART_5200_EE_16 */
	40, /* CART_5200_40 */
	64, /* CART_WILL_64 */
	64, /* CART_EXP_64 */
	64, /* CART_DIAMOND_64 */
	64, /* CART_SDX */
	32, /* CART_XEGS_32 */
	64, /* CART_XEGS_64 */
	128,/* CART_XEGS_128 */
	16, /* CART_OSS2_16 */
	16, /* CART_5200_NS_16 */
	128,/* CART_ATRAX_128 */
	40, /* CART_BBSB_40 */
	8,  /* CART_5200_8 */
	4,  /* CART_5200_4 */
	8,  /* CART_RIGHT_8 */
	32, /* CART_WILL_32 */
	256,/* CART_XEGS_256 */
	512,/* CART_XEGS_512 */
	1024/* CART_XEGS_1024 */
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

UBYTE *cart_image = NULL;		/* For cartridge memory */
int cart_type = CART_NONE;

static int bank;

/* DB_32, XEGS_32, XEGS_64, XEGS_128, XEGS_256, XEGS_512, XEGS_1024 */
static void set_bank_809F(int b)
{
	if (b != bank) {
		CopyROM(0x8000, 0x9fff, cart_image + b * 0x2000);
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

/* EXP_64, DIAMOND_64, SDX_64 */
static void set_bank_A0BF(int b)
{
	if (b != bank) {
		if (b & 0x0008)
			CartA0BF_Disable();
		else {
			CartA0BF_Enable();
			CopyROM(0xa000, 0xbfff, cart_image + (~b & 0x0007) * 0x2000);
		}
		bank = b;
	}
}

static void set_bank_A0BF_WILL64(int b)
{
	if (b != bank) {
		if (b & 0x0008)
			CartA0BF_Disable();
		else {
			CartA0BF_Enable();
			CopyROM(0xa000, 0xbfff, cart_image + ( b & 7 ) * 0x2000);
		}
		bank = b;
	}
}

static void set_bank_A0BF_WILL32(int b)
{
	if (b != bank) {
		if (b & 0x0008)
			CartA0BF_Disable();
		else {
			CartA0BF_Enable();
			CopyROM(0xa000, 0xbfff, cart_image + ( b & 3 ) * 0x2000);
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
			}
		set_bank_A0AF(b, 0x3000);
		break;
	case CART_DB_32:
		set_bank_809F(addr & 0x03);
		break;
	case CART_WILL_64:
		set_bank_A0BF_WILL64(addr);
		break;
	case CART_WILL_32:
		set_bank_A0BF_WILL32(addr);
		break;
	case CART_EXP_64:
		if ((addr & 0xf0) == 0x70)
			set_bank_A0BF(addr);
		break;
	case CART_DIAMOND_64:
		if ((addr & 0xf0) == 0xd0)
			set_bank_A0BF(addr);
		break;
	case CART_SDX_64:
		if ((addr & 0xf0) == 0xe0)
			set_bank_A0BF(addr);
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
	default:
		break;
	}
}

/* a read from D500-D5FF area */
UBYTE CART_GetByte(UWORD addr)
{
	if (rtime_enabled && (addr == 0xd5b8 || addr == 0xd5b9))
		return RTIME_GetByte();
	CART_Access(addr);
	return 0xff;
}

/* a write to D500-D5FF area */
void CART_PutByte(UWORD addr, UBYTE byte)
{
	if (rtime_enabled && (addr == 0xd5b8 || addr == 0xd5b9)) {
		RTIME_PutByte(byte);
		return;
	}
	switch (cart_type) {
	case CART_XEGS_32:
		set_bank_809F(byte & 0x03);
		break;
	case CART_XEGS_64:
		set_bank_809F(byte & 0x07);
		break;
	case CART_XEGS_128:
		set_bank_809F(byte & 0x0f);
		break;
	case CART_XEGS_256:
		set_bank_809F(byte & 0x1f);
		break;
	case CART_XEGS_512:
		set_bank_809F(byte & 0x3f);
		break;
	case CART_XEGS_1024:
		set_bank_809F(byte & 0x7f);
		break;
	case CART_ATRAX_128:
		if (byte & 0x80) {
			if (bank >= 0) {
				CartA0BF_Disable();
				bank = -1;
			}
		}
		else {
			int b = byte & 0xf;
			if (b != bank) {
				CartA0BF_Enable();
				CopyROM(0xa000, 0xbfff, cart_image + b * 0x2000);
				bank = b;
			}
		}
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
	}
	else {
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

int CART_Checksum(UBYTE *image, int nbytes)
{
	int checksum = 0;
	while (nbytes > 0) {
		checksum += *image++;
		nbytes--;
	}
	return checksum;
}

int CART_Insert(char *filename)
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
	fseek(fp, 0L, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	/* if full kilobytes, assume it is raw image */
	if ((len & 0x3ff) == 0) {
		/* alloc memory and read data */
		cart_image = malloc(len);
		if (cart_image == NULL) {
			fclose(fp);
			return CART_BAD_FORMAT;
		}
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
			cart_image = malloc(len);
			if (cart_image == NULL) {
				fclose(fp);
				return CART_BAD_FORMAT;
			}
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
			SetHARDWARE(0x4ff6, 0x4ff9);
			SetHARDWARE(0x5ff6, 0x5ff9);
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
			Cart809F_Disable();
			CartA0BF_Enable();
			CopyROM(0xa000, 0xbfff, cart_image);
			break;
		case CART_STD_16:
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
			Cart809F_Disable();
			CartA0BF_Enable();
			CopyROM(0xa000, 0xbfff, cart_image);
			bank = 0;
			break;
		case CART_XEGS_32:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0x6000);
			bank = 0;
			break;
		case CART_XEGS_64:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0xe000);
			bank = 0;
			break;
		case CART_XEGS_128:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0x1e000);
			bank = 0;
			break;
		case CART_XEGS_256:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0x3e000);
			bank = 0;
			break;
		case CART_XEGS_512:
			Cart809F_Enable();
			CartA0BF_Enable();
			CopyROM(0x8000, 0x9fff, cart_image);
			CopyROM(0xa000, 0xbfff, cart_image + 0x7e000);
			bank = 0;
			break;
		case CART_XEGS_1024:
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
			SetHARDWARE(0x8ff6, 0x8ff9);
			SetHARDWARE(0x9ff6, 0x9ff9);
			break;
		case CART_RIGHT_8:
			if (machine_type == MACHINE_OSA || machine_type == MACHINE_OSB) {
				Cart809F_Enable();
				CopyROM(0x8000, 0x9fff, cart_image);
				if (!disable_basic && have_basic) {
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
		default:
			Cart809F_Disable();
			if ((machine_type == MACHINE_OSA || machine_type == MACHINE_OSB)
			 && !disable_basic && have_basic) {
				CartA0BF_Enable();
				CopyROM(0xa000, 0xbfff, atari_basic);
				break;
			}
			CartA0BF_Disable();
			break;
		}
	}
}

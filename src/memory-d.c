/* $Id$ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "antic.h"
#include	"cpu.h"
#include "cartridge.h"
#include "gtia.h"
#include "log.h"
#include "memory.h"
#include "pia.h"
#include "rt-config.h"
#include "statesav.h"

UBYTE memory[65536];
UBYTE attrib[65536];
static UBYTE under_atarixl_os[16384];
static UBYTE under_atari_basic[8192];
static UBYTE *atarixe_memory = NULL;
static ULONG atarixe_memory_size = 0;

int have_basic;	/* Atari BASIC image has been successfully read (Atari 800 only) */

extern int pil_on;

static void AllocXEMemory(void)
{
	if (ram_size > 64) {
		/* don't count 64 KB of base memory */
		/* count number of 16 KB banks, add 1 for saving base memory 0x4000-0x7fff */
		ULONG size = (1 + (ram_size - 64) / 16) * 16384;
		if (size != atarixe_memory_size) {
			if (atarixe_memory != NULL)
				free(atarixe_memory);
			atarixe_memory = malloc(size);
			if (atarixe_memory == NULL) {
				Aprint("MEMORY_InitialiseMachine: Out of memory! Switching to 64 KB mode");
				atarixe_memory_size = 0;
				ram_size = 64;
			}
			else {
				atarixe_memory_size = size;
				memset(atarixe_memory, 0, size);
			}
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
		SetHARDWARE(0xd000, 0xd7ff);
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
		}
		else {
			dFillMem(0x0000, 0x00, 0xc000);
			SetRAM(0x0000, 0xbfff);
			SetROM(0xc000, 0xcfff);
		}
		SetHARDWARE(0xd000, 0xd7ff);
		SetROM(0xd800, 0xffff);
		break;
	case MACHINE_5200:
		memcpy(memory + 0xf800, atari_os, 0x800);
		dFillMem(0x0000, 0x00, 0xf800);
		SetRAM(0x0000, 0x3fff);
		SetROM(0x4000, 0xffff);
		SetHARDWARE(0xc000, 0xc0ff);	/* 5200 GTIA Chip */
		SetHARDWARE(0xd400, 0xd4ff);	/* 5200 ANTIC Chip */
		SetHARDWARE(0xe800, 0xe8ff);	/* 5200 POKEY Chip */
		SetHARDWARE(0xeb00, 0xebff);	/* 5200 POKEY Chip */
		break;
	}
	AllocXEMemory();
	Coldstart();
}

void EnablePILL(void)
{
	SetROM(0x8000, 0xbfff);
	pil_on = TRUE;
}

void DisablePILL(void)
{
	SetRAM(0x8000, 0xbfff);
	pil_on = FALSE;
}

void MemStateSave( UBYTE SaveVerbose )
{
	SaveUBYTE( &memory[0], 65536 );
	SaveUBYTE( &attrib[0], 65536 );

	if (machine_type == MACHINE_XLXE) {
		if( SaveVerbose != 0 )
			SaveUBYTE( &atari_basic[0], 8192 );
		SaveUBYTE( &under_atari_basic[0], 8192 );

		if( SaveVerbose != 0 )
			SaveUBYTE( &atari_os[0], 16384 );
		SaveUBYTE( &under_atarixl_os[0], 16384 );
	}

	if (ram_size > 64) {
		SaveUBYTE( &atarixe_memory[0], atarixe_memory_size );
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

void MemStateRead( UBYTE SaveVerbose )
{
	ReadUBYTE( &memory[0], 65536 );
	ReadUBYTE( &attrib[0], 65536 );

	if (machine_type == MACHINE_XLXE) {
		if( SaveVerbose != 0 )
			ReadUBYTE( &atari_basic[0], 8192 );
		ReadUBYTE( &under_atari_basic[0], 8192 );

		if( SaveVerbose != 0 )
			ReadUBYTE( &atari_os[0], 16384 );
		ReadUBYTE( &under_atarixl_os[0], 16384 );
	}

	AllocXEMemory();
	if (ram_size > 64) {
		ReadUBYTE( &atarixe_memory[0], atarixe_memory_size );
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

void CopyFromMem(ATPtr from, UBYTE * to, int size)
{
	memcpy(to, from + memory, size);
}

void CopyToMem(UBYTE * from, ATPtr to, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (!attrib[to])
			dPutByte(to, *from);
		from++, to++;
	}
}

void PORTB_handler(UBYTE byte)
{
	if (ram_size > 64) {
		int bank = 0;
		/* bank = 0 ...normal RAM */
		/* bank = 1..4 (..16) ...extended RAM */

		if ((byte & 0x10) == 0)
			switch (ram_size) {
			case 128:
				bank = ((byte & 0x0c) >> 2) + 1;
				break;
			case RAM_320_RAMBO:
				bank = (((byte & 0x0c) | ((byte & 0x60) >> 1)) >> 2) + 1;
				break;
			case RAM_320_COMPY_SHOP:
				bank = (((byte & 0x0c) | ((byte & 0xc0) >> 2)) >> 2) + 1;
				break;
			}

		if (bank != xe_bank) {
			if (selftest_enabled) {
				/* SelfTestROM Disable */
				memcpy(memory + 0x5000, under_atarixl_os + 0x1000, 0x800);
				SetRAM(0x5000, 0x57ff);
				selftest_enabled = FALSE;
			}
			memcpy(atarixe_memory + (((long) xe_bank) << 14), memory + 0x4000, 16384);
			memcpy(memory + 0x4000, atarixe_memory + (((long) bank) << 14), 16384);
			xe_bank = bank;
		}
	}
#ifdef DEBUG
	printf("Storing %x to PORTB, PC = %x\n", byte, regPC);
#endif
/*
 * Enable/Disable OS ROM 0xc000-0xcfff and 0xd800-0xffff
 */
	if ((PORTB ^ byte) & 0x01) {	/* Only when is changed this bit !RS! */
		if (byte & 0x01) {
			/* OS ROM Enable */
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
			/* OS ROM Disable */
			if (ram_size > 48) {
				memcpy(memory + 0xc000, under_atarixl_os, 0x1000);
				memcpy(memory + 0xd800, under_atarixl_os + 0x1800, 0x2800);
				SetRAM(0xc000, 0xcfff);
				SetRAM(0xd800, 0xffff);
			}
			else {
				dFillMem(0xc000, 0xff, 0x1000);
				dFillMem(0xd800, 0xff, 0x2800);
			}

/* when OS ROM is disabled we also have to disable SelfTest - Jindroush */
			/* SelfTestROM Disable */
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

/*
   =====================================
   Enable/Disable BASIC ROM
   An Atari XL/XE can only disable Basic
   Other cartridge cannot be disable
   =====================================
 */
	if (!cartA0BF_enabled) {
		if ((PORTB ^ byte) & 0x02) {	/* Only when change this bit !RS! */
			if (byte & 0x02) {
				/* BASIC Disable */
				if (ram_size > 40) {
					memcpy(memory + 0xa000, under_atari_basic, 0x2000);
					SetRAM(0xa000, 0xbfff);
				}
				else
					dFillMem(0xa000, 0xff, 0x2000);
			}
			else {
				/* BASIC Enable */
				if (ram_size > 40) {
					memcpy(under_atari_basic, memory + 0xa000, 0x2000);
					SetROM(0xa000, 0xbfff);
				}
				memcpy(memory + 0xa000, atari_basic, 0x2000);
			}
		}
	}
/*
 * Enable/Disable Self Test ROM
 */
	if (byte & 0x80) {
		/* SelfTestROM Disable */
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
	else {
/* we can enable Selftest only if the OS ROM is enabled */
		/* SELFTEST ROM enable */
		if (!selftest_enabled && (byte & 0x01) && ((byte & 0x10) || (ram_size != RAM_320_COMPY_SHOP))) {
			/* Only when CPU access to normal RAM or isn't 256Kb RAM or RAMBO mode is set */
			if (ram_size > 20) {
				memcpy(under_atarixl_os + 0x1000, memory + 0x5000, 0x800);
				SetROM(0x5000, 0x57ff);
			}
			memcpy(memory + 0x5000, atari_os + 0x1000, 0x800);
			selftest_enabled = TRUE;
		}
	}

	PORTB = byte;
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
		if ((machine_type != MACHINE_XLXE) || (PORTB & 0x02)) {
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
		if (((machine_type != MACHINE_XLXE) || (PORTB & 0x02)) && ram_size > 40) {
			memcpy(under_cartA0BF, memory + 0xa000, 0x2000);
			SetROM(0xa000, 0xbfff);
		}
		cartA0BF_enabled = TRUE;
		if (machine_type == MACHINE_XLXE)
			TRIG[3] = 1;
	}
}

void get_charset(char * cs)
{
	switch (machine_type) {
	case MACHINE_OSA:
	case MACHINE_OSB:
		memcpy(cs, memory + 0xe000, 1024);
		break;
	case MACHINE_XLXE:
		memcpy(cs, atari_os + 0x2000, 1024);
		break;
	case MACHINE_5200:
		memcpy(cs, memory + 0xf800, 1024);
		break;
	}
}

/*
$Log$
Revision 1.16  2002/07/04 12:40:57  pfusik
emulation of 16K RAM machines: 400 and 600XL

Revision 1.15  2001/10/26 05:43:00  fox
made 130 XE state files compatible with previous versions

Revision 1.14  2001/10/08 11:40:48  joy
neccessary include for compiling with DEBUG defined (see line 200)

Revision 1.13  2001/10/03 16:42:50  fox
rewritten escape codes handling

Revision 1.12  2001/10/01 17:13:26  fox
Poke -> dPutByte

Revision 1.11  2001/09/17 18:19:50  fox
malloc/free atarixe_memory, enable_c000_ram -> ram_size = 52

Revision 1.10  2001/09/17 18:12:08  fox
machine, mach_xlxe, Ram256, os, default_system -> machine_type, ram_size

Revision 1.9  2001/09/17 07:33:07  fox
Initialise_Atari... functions moved to atari.c

Revision 1.8  2001/09/09 08:39:01  fox
read Atari BASIC for Atari 800

Revision 1.7  2001/08/16 23:28:57  fox
deleted CART_Remove() in Initialise_Atari*, so auto-switching to 5200 mode
when inserting a 5200 cartridge works

Revision 1.6  2001/07/20 20:15:35  fox
rewritten to support the new cartridge module

Revision 1.3  2001/03/25 06:57:35  knik
open() replaced by fopen()

Revision 1.2  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/

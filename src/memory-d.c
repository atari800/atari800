/* $Id$ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "antic.h"
#include "cartridge.h"
#include "cpu.h"
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
static UBYTE atarixe_memory[278528];	/* 16384 (for RAM under BankRAM buffer) + 65536 (for 130XE) * 4 (for Atari320) */

int have_basic;	/* Atari BASIC image has been successfully read (Atari 800 only) */

extern int os;
extern int pil_on;

int load_image(char *filename, int addr, int nbytes)
{
	int status = FALSE;
	FILE *f;

	f = fopen(filename, "rb");
	if (f) {
		status = fread(&memory[addr], 1, nbytes, f);
		fclose(f);
		if (status != nbytes) {
			Aprint("Error reading %s", filename);
			return FALSE;
		}

		status = TRUE;
	}
	else
		Aprint("Error loading rom: %s", filename);

	return status;
}

void add_esc(UWORD address, UBYTE esc_code)
{
	memory[address++] = 0xf2;	/* ESC */
	memory[address++] = esc_code;	/* ESC CODE */
	memory[address] = 0x60;		/* RTS */
}

void PatchOS(void)
{
	const unsigned short o_open = 0;
	const unsigned short o_close = 2;
	const unsigned short o_read = 4;
	const unsigned short o_write = 6;
	const unsigned short o_status = 8;
	/* const unsigned short   o_special = 10; */
	const unsigned short o_init = 12;

	unsigned short addr = 0;
	unsigned short entry;
	unsigned short devtab;
	int i;

#ifndef BASIC
	/* Check if ROM patches are enabled - if not return immediately */
	if (!enable_sio_patch && !enable_h_patch && !enable_p_patch)
		return;
#endif

	/* Disable Checksum Test */
	switch (machine) {
	case Atari:
		break;
	case AtariXL:
	case AtariXE:
	case Atari320XE:
		memory[0xc314] = 0x8e;
		memory[0xc315] = 0xff;
		memory[0xc319] = 0x8e;
		memory[0xc31a] = 0xff;
		break;
	default:
		Aprint("Fatal Error in atari.c: PatchOS(): Unknown machine");
		Atari800_Exit(FALSE);
		break;
	}

	/* Set SIO (fast disk access) patch */
	if (enable_sio_patch)
		add_esc(0xe459, ESC_SIOV);

/*
   ==========================================
   Patch O.S. - Modify Handler Table (HATABS)
   ==========================================
 */
	switch (machine) {
	case Atari:
		addr = 0xf0e3;
		break;
	case AtariXL:
	case AtariXE:
	case Atari320XE:
		addr = 0xc42e;
		break;
	default:
		Aprint("Fatal Error in atari.c: PatchOS(): Unknown machine");
		Atari800_Exit(FALSE);
		break;
	}

	for (i = 0; i < 5; i++) {
		devtab = (memory[addr + 2] << 8) | memory[addr + 1];

		switch (memory[addr]) {
		case 'P':
			if (!enable_p_patch)
				break;
			entry = (memory[devtab + o_open + 1] << 8) | memory[devtab + o_open];
			add_esc((UWORD)(entry + 1), ESC_PHOPEN);
			entry = (memory[devtab + o_close + 1] << 8) | memory[devtab + o_close];
			add_esc((UWORD)(entry + 1), ESC_PHCLOS);
/*
   entry = (memory[devtab+o_read+1] << 8) | memory[devtab+o_read];
   add_esc (entry+1, ESC_PHREAD);
 */
			entry = (memory[devtab + o_write + 1] << 8) | memory[devtab + o_write];
			add_esc((UWORD)(entry + 1), ESC_PHWRIT);
			entry = (memory[devtab + o_status + 1] << 8) | memory[devtab + o_status];
			add_esc((UWORD)(entry + 1), ESC_PHSTAT);
/*
   entry = (memory[devtab+o_special+1] << 8) | memory[devtab+o_special];
   add_esc (entry+1, ESC_PHSPEC);
 */
			memory[devtab + o_init] = 0xd2;
			memory[devtab + o_init + 1] = ESC_PHINIT;
			break;
		case 'C':
			if (!enable_h_patch)
				break;
			memory[addr] = 'H';
			entry = (memory[devtab + o_open + 1] << 8) | memory[devtab + o_open];
			add_esc((UWORD)(entry + 1), ESC_HHOPEN);
			entry = (memory[devtab + o_close + 1] << 8) | memory[devtab + o_close];
			add_esc((UWORD)(entry + 1), ESC_HHCLOS);
			entry = (memory[devtab + o_read + 1] << 8) | memory[devtab + o_read];
			add_esc((UWORD)(entry + 1), ESC_HHREAD);
			entry = (memory[devtab + o_write + 1] << 8) | memory[devtab + o_write];
			add_esc((UWORD)(entry + 1), ESC_HHWRIT);
			entry = (memory[devtab + o_status + 1] << 8) | memory[devtab + o_status];
			add_esc((UWORD)(entry + 1), ESC_HHSTAT);
			break;
		case 'E':
#ifdef BASIC
			Aprint("Editor Device");
			entry = (memory[devtab + o_open + 1] << 8) | memory[devtab + o_open];
			add_esc((UWORD)(entry + 1), ESC_E_OPEN);
			entry = (memory[devtab + o_read + 1] << 8) | memory[devtab + o_read];
			add_esc((UWORD)(entry + 1), ESC_E_READ);
			entry = (memory[devtab + o_write + 1] << 8) | memory[devtab + o_write];
			add_esc((UWORD)(entry + 1), ESC_E_WRITE);
#endif
			break;
		case 'S':
			break;
		case 'K':
#ifdef BASIC
			Aprint("Keyboard Device");
			entry = (memory[devtab + o_read + 1] << 8) | memory[devtab + o_read];
			add_esc(entry + 1, ESC_K_READ);
#endif
			break;
		default:
			break;
		}

		addr += 3;				/* Next Device in HATABS */
	}
}

int Initialise_AtariXL(void)
{
	int status;
	mach_xlxe = TRUE;
	status = load_image(atari_xlxe_filename, 0xc000, 0x4000);
	if (status) {
		memcpy(atari_os, memory + 0xc000, 0x4000);
		machine = AtariXL;
		PatchOS();

		status = load_image(atari_basic_filename, 0xa000, 0x2000);
		memcpy(atari_basic, memory + 0xa000, 0x2000);
		SetRAM(0x0000, 0xbfff);
		SetROM(0xc000, 0xffff);
		SetHARDWARE(0xd000, 0xd7ff);
		Coldstart();
	}
	return status;
}

int Initialise_Atari5200(void)
{
	int status;
	mach_xlxe = FALSE;
	memset(memory, 0, 0xf800);
	status = load_image(atari_5200_filename, 0xf800, 0x800);
	if (status) {
		machine = Atari5200;
		SetRAM(0x0000, 0x3fff);
		/*SetROM(0xf800, 0xffff);*/
		SetROM(0x4000, 0xffff);
		SetHARDWARE(0xc000, 0xc0ff);	/* 5200 GTIA Chip */
		SetHARDWARE(0xd400, 0xd4ff);	/* 5200 ANTIC Chip */
		SetHARDWARE(0xe800, 0xe8ff);	/* 5200 POKEY Chip */
		SetHARDWARE(0xeb00, 0xebff);	/* 5200 POKEY Chip */
		Coldstart();
	}
	return status;
}

/*
 * Initialise System with an replacement OS. It has just
 * enough functionality to run Defender and Star Raider.
 */

#include "emuos.h"

int Initialise_EmuOS(void)
{
	int status;

	status = load_image("emuos.img", 0xc000, 0x4000);
	if (!status)
		memcpy(&memory[0xc000], emuos_h, 0x4000);
	else
		Aprint("EmuOS: Using external emulated OS");

	memcpy(atari_os, memory + 0xd800, 0x2800);
	machine = Atari;
	PatchOS();
	SetRAM(0x0000, 0xbfff);
	if (enable_c000_ram)
		SetRAM(0xc000, 0xcfff);
	else
		SetROM(0xc000, 0xcfff);
	SetROM(0xd800, 0xffff);
	SetHARDWARE(0xd000, 0xd7ff);
	Coldstart();

	return TRUE;
}

void ClearRAM(void)
{
	memset(memory, 0, 65536);	/* Optimalize by Raster */
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

int Initialise_AtariOSA(void)
{
	int status;
	mach_xlxe = FALSE;
	status = load_image(atari_osa_filename, 0xd800, 0x2800);
	if (status) {
		memcpy(atari_os, memory + 0xd800, 0x2800);
		machine = Atari;
		PatchOS();

		have_basic = load_image(atari_basic_filename, 0xa000, 0x2000);
		memcpy(atari_basic, memory + 0xa000, 0x2000);
		SetRAM(0x0000, 0xbfff);
		if (enable_c000_ram)
			SetRAM(0xc000, 0xcfff);
		else
			SetROM(0xc000, 0xcfff);
		SetROM(0xd800, 0xffff);
		SetHARDWARE(0xd000, 0xd7ff);
		Coldstart();
	}
	return status;
}

int Initialise_AtariOSB(void)
{
	int status;
	mach_xlxe = FALSE;
	status = load_image(atari_osb_filename, 0xd800, 0x2800);
	if (status) {
		memcpy(atari_os, memory + 0xd800, 0x2800);
		machine = Atari;
		PatchOS();

		have_basic = load_image(atari_basic_filename, 0xa000, 0x2000);
		memcpy(atari_basic, memory + 0xa000, 0x2000);
		SetRAM(0x0000, 0xbfff);
		if (enable_c000_ram)
			SetRAM(0xc000, 0xcfff);
		else
			SetROM(0xc000, 0xcfff);
		SetROM(0xd800, 0xffff);
		SetHARDWARE(0xd000, 0xd7ff);
		Coldstart();
	}
	return status;
}

void MemStateSave( UBYTE SaveVerbose )
{
	SaveUBYTE( &memory[0], 65536 );
	SaveUBYTE( &attrib[0], 65536 );

	if( mach_xlxe )
	{
		if( SaveVerbose != 0 )
			SaveUBYTE( &atari_basic[0], 8192 );
		SaveUBYTE( &under_atari_basic[0], 8192 );

		if( SaveVerbose != 0 )
			SaveUBYTE( &atari_os[0], 16384 );
		SaveUBYTE( &under_atarixl_os[0], 16384 );
	}

	if( machine == Atari320XE || machine == AtariXE  )
		SaveUBYTE( &atarixe_memory[0], 278528 );

}

void MemStateRead( UBYTE SaveVerbose )
{
	ReadUBYTE( &memory[0], 65536 );
	ReadUBYTE( &attrib[0], 65536 );

	if( mach_xlxe )
	{
		if( SaveVerbose != 0 )
			ReadUBYTE( &atari_basic[0], 8192 );
		ReadUBYTE( &under_atari_basic[0], 8192 );

		if( SaveVerbose != 0 )
			ReadUBYTE( &atari_os[0], 16384 );
		ReadUBYTE( &under_atarixl_os[0], 16384 );
	}

	if( machine == Atari320XE || machine == AtariXE  )
		ReadUBYTE( &atarixe_memory[0], 278528 );

}

void CopyFromMem(ATPtr from, UBYTE * to, int size)
{
	memcpy(to, from + memory, size);
}

extern UBYTE attrib[65536];

void CopyToMem(UBYTE * from, ATPtr to, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (!attrib[to])
			Poke(to, *from);
		from++, to++;
	}
}

void PORTB_handler(UBYTE byte)
{
	switch (machine) {
	case AtariXE:
	case Atari320XE:
		{
			int bank;
			/* bank = 0 ...normal RAM */
			/* bank = 1..4 (..16) ...extended RAM */

			if (byte & 0x10) {
				/* CPU to normal RAM */
				bank = 0;
			}
			else {
				/* CPU to extended RAM */
				if (machine == Atari320XE) {
					if (Ram256 == 1)
						bank = (((byte & 0x0c) | ((byte & 0x60) >> 1)) >> 2) + 1; /* RAMBO */
					else 
						bank = (((byte & 0x0c) | ((byte & 0xc0) >> 2)) >> 2) + 1; /* COMPY SHOP */
				}
				else
					bank = ((byte & 0x0c) >> 2) + 1;
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
	case AtariXL:
#ifdef DEBUG
		printf("Storing %x to PORTB, PC = %x\n", byte, regPC);
#endif
/*
 * Enable/Disable OS ROM 0xc000-0xcfff and 0xd800-0xffff
 */
		if ((PORTB ^ byte) & 0x01) {	/* Only when is changed this bit !RS! */
			if (byte & 0x01) {
				/* OS ROM Enable */
				memcpy(under_atarixl_os, memory + 0xc000, 0x1000);
				memcpy(under_atarixl_os + 0x1800, memory + 0xd800, 0x2800);
				memcpy(memory + 0xc000, atari_os, 0x1000);
				memcpy(memory + 0xd800, atari_os + 0x1800, 0x2800);
				SetROM(0xc000, 0xcfff);
				SetROM(0xd800, 0xffff);
				PatchOS();
			}
			else {
				/* OS ROM Disable */
				memcpy(memory + 0xc000, under_atarixl_os, 0x1000);
				memcpy(memory + 0xd800, under_atarixl_os + 0x1800, 0x2800);
				SetRAM(0xc000, 0xcfff);
				SetRAM(0xd800, 0xffff);

/* when OS ROM is disabled we also have to disable SelfTest - Jindroush */
				/* SelfTestROM Disable */
				if (selftest_enabled) {
					memcpy(memory + 0x5000, under_atarixl_os + 0x1000, 0x800);
					SetRAM(0x5000, 0x57ff);
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
					memcpy(memory + 0xa000, under_atari_basic, 0x2000);
					SetRAM(0xa000, 0xbfff);
				}
				else {
					/* BASIC Enable */
					memcpy(under_atari_basic, memory + 0xa000, 0x2000);
					memcpy(memory + 0xa000, atari_basic, 0x2000);
					SetROM(0xa000, 0xbfff);
				}
			}
		}
/*
 * Enable/Disable Self Test ROM
 */
		if (byte & 0x80) {
			/* SelfTestROM Disable */
			if (selftest_enabled) {
				memcpy(memory + 0x5000, under_atarixl_os + 0x1000, 0x800);
				SetRAM(0x5000, 0x57ff);
				selftest_enabled = FALSE;
			}
		}
		else {
/* we can enable Selftest only if the OS ROM is enabled */
			/* SELFTEST ROM enable */
			if (!selftest_enabled && (byte & 0x01) && ((byte & 0x10) || (Ram256 < 2))) {
				/* Only when CPU access to normal RAM or isn't 256Kb RAM or RAMBO mode is set */
				memcpy(under_atarixl_os + 0x1000, memory + 0x5000, 0x800);
				memcpy(memory + 0x5000, atari_os + 0x1000, 0x800);
				SetROM(0x5000, 0x57ff);
				selftest_enabled = TRUE;
			}
		}

		PORTB = byte;
		break;
	default:
		Aprint("Fatal Error in pia.c: PIA_PutByte(): Unknown machine\n");
		Atari800_Exit(FALSE);
		exit(1);
		break;
	}
}

static int cart809F_enabled = FALSE;
int cartA0BF_enabled = FALSE;
static UBYTE under_cart809F[8192];
static UBYTE under_cartA0BF[8192];

void Cart809F_Disable(void)
{
	if (cart809F_enabled) {
		memcpy(memory + 0x8000, under_cart809F, 0x2000);
		SetRAM(0x8000, 0x9fff);
		cart809F_enabled = FALSE;
	}
}

void Cart809F_Enable(void)
{
	if (!cart809F_enabled) {
		memcpy(under_cart809F, memory + 0x8000, 0x2000);
		SetROM(0x8000, 0x9fff);
		cart809F_enabled = TRUE;
	}
}

void CartA0BF_Disable(void)
{
	if (cartA0BF_enabled) {
		if (!mach_xlxe || (PORTB & 0x02)) {
			memcpy(memory + 0xa000, under_cartA0BF, 0x2000);
			SetRAM(0xa000, 0xbfff);
		}
		else
			memcpy(memory + 0xa000, atari_basic, 0x2000);
		cartA0BF_enabled = FALSE;
		if (mach_xlxe) {
			TRIG[3] = 0;
			if (GRACTL & 4)
				TRIG_latch[3] = 0;
		}
	}
}

void CartA0BF_Enable(void)
{
	if (!cartA0BF_enabled) {
		if (!mach_xlxe || (PORTB & 0x02)) {
			memcpy(under_cartA0BF, memory + 0xa000, 0x2000);
			SetROM(0xa000, 0xbfff);
		}
		cartA0BF_enabled = TRUE;
		if (mach_xlxe)
			TRIG[3] = 1;
	}
}

void get_charset(char * cs)
{
	if (mach_xlxe)
		memcpy(cs, atari_os + 0x2000, 1024);
	else if (machine == Atari5200)
		memcpy(cs, memory + 0xf800, 1024);
	else
		memcpy(cs, memory + 0xe000, 1024);
}

/*
$Log$
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

/* #define MEM_DEBUG */

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include "atari.h"
#include "log.h"
#include "rt-config.h"
#include "antic.h"
#include "cpu.h"
#include "gtia.h"
#include "pbi.h"
#include "pia.h"
#include "pokey.h"
#include "supercart.h"
#include "memory.h"

static UBYTE main_bank[65536];
static UBYTE xe_ram[16][16384];
UBYTE *atarixl_os;
UBYTE *atari_basic;

#ifndef INLINE_GETBYTE
typedef UBYTE (*rdfunc)(UWORD addr);
#endif
typedef int (*wrfunc)(UWORD addr, UBYTE value);

rdfunc readmap[256];
wrfunc writemap[256];

extern int mach_xlxe;
extern int cart_type;
extern int rom_inserted;
extern int selftest_enabled;
extern int os;
extern int Ram256;

UBYTE XE_Bank_GetByte(UWORD addr)
{
	return xe_ram[xe_bank-1][addr];
}

int XE_Bank_PutByte(UWORD addr, UBYTE value)
{
	xe_ram[xe_bank-1][addr] = value;
	return FALSE;
}

UBYTE XLRAM_GetByte(UWORD addr)
{
	return main_bank[addr];
}

int XLRAM_PutByte(UWORD addr, UBYTE value)
{
	main_bank[addr] = value;
	return FALSE;
}

UBYTE XLOS_GetByte(UWORD addr)
{
	return atarixl_os[addr-0xc000];
}

int XLOS_PutByte(UWORD addr, UBYTE value)
{
	return FALSE;
}

UBYTE XL_SelfTest_GetByte(UWORD addr)
{
	return atarixl_os[addr-0x4000];
}

int XL_SelfTest_PutByte(UWORD addr, UBYTE value)
{
	return FALSE;
}

UBYTE XLBasic_GetByte(UWORD addr)
{
	return atari_basic[addr-0xa000];
}

int XLBasic_PutByte(UWORD addr, UBYTE value)
{
	return FALSE;
}

UBYTE *load_file(char *filename)
{
	struct stat stat_buf;
	int status;
	UBYTE *buffer;
	int fd;

	status = stat(filename, &stat_buf);
	if (status == -1) return NULL;
	buffer = (UBYTE *) malloc(stat_buf.st_size);
	if (!buffer) return NULL;
	fd = open(filename, O_RDONLY | O_BINARY);
	if (fd == -1) {
		free(buffer);
		return NULL;
	}
	status = read(fd, buffer, stat_buf.st_size);
	if (status != stat_buf.st_size) {
		free(buffer);
		return NULL;
	}
	return buffer;
}

void ClearRAM(void)
{
	memset(main_bank, 0, 65536);
}

void PatchOS(void)
{
	if (enable_rom_patches == 0) return;

	Aprint("PatchOS called");
	exit(1);
}

void SetXLHardware(void)
{
#ifdef MEM_DEBUG
	fprintf(stderr, "SetXLHardware called\n");
#endif
	readmap[0xd0] = GTIA_GetByte;
	readmap[0xd1] = PBI_GetByte;
	readmap[0xd2] = POKEY_GetByte;
	readmap[0xd3] = PIA_GetByte;
	readmap[0xd4] = ANTIC_GetByte;
	readmap[0xd5] = SuperCart_GetByte;
	readmap[0xd6] = PBIM1_GetByte;
	readmap[0xd7] = PBIM2_GetByte;
	writemap[0xd0] = GTIA_PutByte;
	writemap[0xd1] = PBI_PutByte;
	writemap[0xd2] = POKEY_PutByte;
	writemap[0xd3] = PIA_PutByte;
	writemap[0xd4] = ANTIC_PutByte;
	writemap[0xd5] = SuperCart_PutByte;
	writemap[0xd6] = PBIM1_PutByte;
	writemap[0xd7] = PBIM2_PutByte;
}

void XL_SelfTest_Off(void)
{
	int i;

#ifdef MEM_DEBUG
	fprintf(stderr, "XL_SelfTest_Off called\n");
#endif
	if (xe_bank) {
		for (i=0x50; i<=0x5f; i++) {
			readmap[i] = XE_Bank_GetByte;
			writemap[i] = XE_Bank_PutByte;
		}
	} else {
		for (i=0x50; i<=0x5f; i++) {
			readmap[i] = XLRAM_GetByte;
			writemap[i] = XLRAM_PutByte;
		}
	}
}

void XL_SelfTest_On(void)
{
	int i;

#ifdef MEM_DEBUG
	fprintf(stderr, "XL_SelfTest_On called\n");
#endif
	for (i=0x50; i<=0x5f; i++) {
		readmap[i] = XL_SelfTest_GetByte;
		writemap[i] = XL_SelfTest_PutByte;
	}
}

void XE_RAM_On(void)
{
	int i;

#ifdef MEM_DEBUG
	fprintf(stderr, "XE_RAM_On called\n");
#endif
	for (i=0x40; i<=0x7f; i++) {
		readmap[i] = XE_Bank_GetByte;
		writemap[i] = XE_Bank_PutByte;
	}
}

void XE_RAM_Off(void)
{
	int i;

#ifdef MEM_DEBUG
	fprintf(stderr, "XE_RAM_Off called\n");
#endif
	for (i=0x40; i<=0x7f; i++) {
		readmap[i] = XLRAM_GetByte;
		writemap[i] = XLRAM_PutByte;
	}
}

void SetXLRAM(void)
{
	int i;

#ifdef MEM_DEBUG
	fprintf(stderr, "SetXLRAM called\n");
#endif
	for (i=0x0; i<=0xbf; i++) {
		readmap[i] = XLRAM_GetByte;
		writemap[i] = XLRAM_PutByte;
	}
}

void SetXLOSRAM(void)
{
	int i;

#ifdef MEM_DEBUG
	fprintf(stderr, "SetXLOSRAM called\n");
#endif
	for (i=0xc0; i<=0xcf; i++) {
		readmap[i] = XLRAM_GetByte;
		writemap[i] = XLRAM_PutByte;
	}
	for (i=0xd8; i<=0xff; i++) {
		readmap[i] = XLRAM_GetByte;
		writemap[i] = XLRAM_PutByte;
	}
}

void SetXLOS(void)
{
	int i;

#ifdef MEM_DEBUG
	fprintf(stderr, "SetXLOS called\n");
#endif
	for (i=0xc0; i<=0xcf; i++) {
		readmap[i] = XLOS_GetByte;
		writemap[i] = XLOS_PutByte;
	}
	for (i=0xd8; i<=0xff; i++) {
		readmap[i] = XLOS_GetByte;
		writemap[i] = XLOS_PutByte;
	}
}

void SetXLBasic(void)
{
	int i;

#ifdef MEM_DEBUG
	fprintf(stderr, "SetXLBasic called\n");
#endif
	for (i=0xa0; i<=0xbf; i++) {
		readmap[i] = XLBasic_GetByte;
		writemap[i] = XLBasic_PutByte;
	}
}

void XL_Basic_Off(void)
{
	int i;

#ifdef MEM_DEBUG
	fprintf(stderr, "XL_Basic_Off called\n");
#endif
	for (i=0xa0; i<=0xbf; i++) {
		readmap[i] = XLRAM_GetByte;
		writemap[i] = XLRAM_PutByte;
	}
}

int Initialise_AtariXL(void)
{
	mach_xlxe = TRUE;
	atarixl_os = load_file(atari_xlxe_filename);
	if (!atarixl_os) return FALSE;
	atari_basic = load_file(atari_basic_filename);
	if (!atari_basic) {
		free(atarixl_os);
		return FALSE;
	}
	machine = AtariXL;
	PatchOS();
	SetXLOS();
	SetXLRAM();
	SetXLHardware();
	if (cart_type == NO_CART) {
		SetXLBasic();
		rom_inserted = FALSE;
		Coldstart();
	} else {
		rom_inserted = FALSE;
		Coldstart();
	}
	return TRUE;
}

UBYTE GetByte(UWORD addr)
{
	/* UBYTE i = ((*readmap[addr>>8])(addr));
	if ((addr > 0xcfff) && (addr < 0xd800))
		fprintf(stderr, "GetByte(%04x) = %02x\n", addr, i);
	return i; */
	return ((*readmap[addr>>8])(addr));
}

int PutByte(UWORD addr, UBYTE value)
{
	/* if ((addr > 0xcfff) && (addr < 0xd800))
		fprintf(stderr, "PutByte(%04x) = %02x\n", addr, value); */
	
	/* if ((addr >> 8) == 0xd3) {
		fprintf(stderr, "%p %p\n", &PIA_PutByte, writemap[0xd3]);
		fprintf(stderr, "%p %p\n", &PIA_GetByte, readmap[0xd3]);
	} */
	return ((*writemap[addr>>8])(addr, value));
}

int load_image(char *filename, int addr, int nbytes)
{
	Aprint("load_image called");
	exit(1);
}

int Insert_8K_ROM(char *filename)
{
	Aprint("Insert_8K_ROM called");
	exit(1);
}

int Insert_16K_ROM(char *filename)
{
	Aprint("Insert_16K_ROM called");
	exit(1);
}

int Insert_OSS_ROM(char *filename)
{
	Aprint("Insert_OSS_ROM called");
	exit(1);
}

int Insert_DB_ROM(char *filename)
{
	Aprint("Insert_DB_ROM called");
	exit(1);
}

int Insert_32K_5200ROM(char *filename)
{
	Aprint("Insert_32K_5200ROM called");
	exit(1);
}

int Remove_ROM(void)
{
	Aprint("Remove_ROM called");
	exit(1);
}

int Insert_Cartridge(char *filename)
{
	Aprint("Insert_Cartridge called");
	exit(1);
}

void RestoreSIO( void )
{
	Aprint("RestoreSIO called");
	exit(1);
}

int Initialise_Monty(void)
{
	Aprint("Initialise_Monty called");
	exit(1);
}

int Initialise_Atari5200(void)
{
	Aprint("Initialise_Atari5200 called");
	exit(1);
}

int Initialise_EmuOS(void)
{
	Aprint("Initialise_EmuOS called");
	exit(1);
}

int bounty_bob1(UWORD addr)
{
	Aprint("bounty_bob1 called");
	exit(1);
}

int bounty_bob2(UWORD addr)
{
	Aprint("bounty_bob2 called");
	exit(1);
}

void DisablePILL(void)
{
	Aprint("DisablePILL called");
	exit(1);
}

void EnablePILL(void)
{
	Aprint("EnablePILL called");
	exit(1);
}

int Initialise_AtariOSA(void)
{
	Aprint("Initialise_OSA called");
	exit(1);
}

int Initialise_AtariOSB(void)
{
	Aprint("Initialise_OSB called");
	exit(1);
}

void MemStateSave(UBYTE SaveVerbose)
{
	Aprint("MemStateSave called");
	exit(1);
}

void MemStateRead(UBYTE SaveVerbose)
{
	Aprint("MemStateRead called");
	exit(1);
}

void CopyFromMem(ATPtr from, UBYTE * to, int size)
{
	Aprint("CopyFromMem called");
	exit(1);
}

void CopyToMem(UBYTE * from, ATPtr to, int size)
{
	Aprint("CopyToMem called");
	exit(1);
}

void PORTB_handler(UBYTE byte)
{
#ifdef MEM_DEBUG
	fprintf(stderr, "PORTB_handler: %02x\n", byte);
#endif
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
				XE_RAM_Off();
			} else {
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
					XL_SelfTest_Off();
					selftest_enabled = FALSE;
				}
				if (bank == 0) {
					XE_RAM_Off();
				} else {
					XE_RAM_On();
				}
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
				SetXLOS();
			}
			else {
				/* OS ROM Disable */
				SetXLOSRAM();

/* when OS ROM is disabled we also have to disable SelfTest - Jindroush */
				/* SelfTestROM Disable */
				if (selftest_enabled) {
					XL_SelfTest_Off();
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
		if (!rom_inserted) {
			if ((PORTB ^ byte) & 0x02) {	/* Only when change this bit !RS! */
				if (byte & 0x02) {
					/* BASIC Disable */
					XL_Basic_Off();
				}
				else {
					/* BASIC Enable */
					SetXLBasic();
				}
			}
		}
/*
 * Enable/Disable Self Test ROM
 */
		if (byte & 0x80) {
			/* SelfTestROM Disable */
			if (selftest_enabled) {
				XL_SelfTest_Off();
				selftest_enabled = FALSE;
			}
		}
		else {
/* we can enable Selftest only if the OS ROM is enabled */
			/* SELFTEST ROM enable */
			if (!selftest_enabled && (byte & 0x01) && ((byte & 0x10) || (Ram256 < 2))) {
				/* Only when CPU access to normal RAM or isn't 256Kb RAM or RAMBO mode is set */
				XL_SelfTest_On();
				selftest_enabled = TRUE;
			}
		}

		PORTB = byte;
		break;
	default:
		Aprint("Fatal Error in pia.c: PIA_PutByte(): Unknown machine\n");
		Atari800_Exit(FALSE);
#ifndef WIN32
		exit(1);
#endif
		break;
	}
}

void supercart_handler(UWORD addr, UBYTE byte)
{
	Aprint("supercart_handler called %04x = %02x", addr, byte);
	/* exit(1); */
}

void get_charset(char * cs)
{
	Aprint("get_charset called");
	exit(1);
}

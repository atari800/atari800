#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "antic.h"
#include "atari.h"
#include "cpu.h"
#include "gtia.h"
#include "log.h"
#include "memory.h"
#include "pia.h"
#include "rt-config.h"
#include "statesav.h"
#include "memory.h"

UBYTE memory[65536];
UBYTE attrib[65536];
static UBYTE under_atarixl_os[16384];
static UBYTE under_atari_basic[8192];
static UBYTE atarixe_memory[278528];	/* 16384 (for RAM under BankRAM buffer) + 65536 (for 130XE) * 4 (for Atari320) */

extern int cart_type;
extern int rom_inserted;
extern UBYTE *cart_image;
extern int os;
extern int mach_xlxe;
extern int selftest_enabled;
extern UBYTE atarixl_os[16384];
extern UBYTE atari_basic[8192];
extern int pil_on;
extern int Ram256;

#ifdef WIN32
extern char	current_rom[ ];
#endif

int load_image(char *filename, int addr, int nbytes)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY | O_BINARY, 0777);
	if (fd != -1) {
		status = read(fd, &memory[addr], nbytes);
		if (status != nbytes) {
			Aprint("Error reading %s", filename);
			Atari800_Exit(FALSE);
			return FALSE;
		}
		close(fd);

		status = TRUE;
	}
	else
		Aprint("Error loading rom: %s", filename);

	return status;
}

/*
 * Load a standard 8K ROM from the specified file
 */

int Insert_8K_ROM(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY | O_BINARY, 0777);
	if (fd != -1) {
		read(fd, &memory[0xa000], 0x2000);
		close(fd);
		SetRAM(0x8000, 0x9fff);
		SetROM(0xa000, 0xbfff);
		cart_type = NORMAL8_CART;
		rom_inserted = TRUE;
		status = TRUE;
#ifdef WIN32
		strcpy( current_rom, filename );
#endif
	}
	return status;
}

/*
 * Load a standard 16K ROM from the specified file
 */

int Insert_16K_ROM(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY | O_BINARY, 0777);
	if (fd != -1) {
		read(fd, &memory[0x8000], 0x4000);
		close(fd);
		SetROM(0x8000, 0xbfff);
		cart_type = NORMAL16_CART;
		rom_inserted = TRUE;
		status = TRUE;
#ifdef WIN32
		strcpy( current_rom, filename );
#endif
	}
	return status;
}

/*
 * Load an OSS Supercartridge from the specified file
 * The OSS cartridge is a 16K bank switched cartridge
 * that occupies 8K of address space between $a000
 * and $bfff
 */

int Insert_OSS_ROM(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY | O_BINARY, 0777);
	if (fd != -1) {
		cart_image = (UBYTE *) malloc(0x4000);
		if (cart_image) {
			read(fd, cart_image, 0x4000);
			memcpy(&memory[0xa000], cart_image, 0x1000);
			memcpy(&memory[0xb000], cart_image + 0x3000, 0x1000);
			SetRAM(0x8000, 0x9fff);
			SetROM(0xa000, 0xbfff);
			cart_type = OSS_SUPERCART;
			rom_inserted = TRUE;
			status = TRUE;
#ifdef WIN32
			strcpy( current_rom, filename );
#endif
		}
		close(fd);
	}
	return status;
}

/*
 * Load a DB Supercartridge from the specified file
 * The DB cartridge is a 32K bank switched cartridge
 * that occupies 16K of address space between $8000
 * and $bfff
 */

int Insert_DB_ROM(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY | O_BINARY, 0777);
	if (fd != -1) {
		cart_image = (UBYTE *) malloc(0x8000);
		if (cart_image) {
			read(fd, cart_image, 0x8000);
			memcpy(&memory[0x8000], cart_image, 0x2000);
			memcpy(&memory[0xa000], cart_image + 0x6000, 0x2000);
			SetROM(0x8000, 0xbfff);
			cart_type = DB_SUPERCART;
			rom_inserted = TRUE;
			status = TRUE;
#ifdef WIN32
			strcpy( current_rom, filename );
#endif
		}
		close(fd);
	}
	return status;
}

/*
 * Load a 32K 5200 ROM from the specified file
 */

int Insert_32K_5200ROM(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY | O_BINARY, 0777);
	if (fd != -1) {
		/* read the first 16k */
		if (read(fd, &memory[0x4000], 0x4000) != 0x4000) {
			close(fd);
			return FALSE;
		}
		/* try and read next 16k */
		cart_type = AGS32_CART;
		if ((status = read(fd, &memory[0x8000], 0x4000)) == 0) {
			/* note: AB__ ABB_ ABBB AABB */
			memcpy(&memory[0x8000], &memory[0x6000], 0x2000);
			memcpy(&memory[0xA000], &memory[0x6000], 0x2000);
			memcpy(&memory[0x6000], &memory[0x4000], 0x2000);
		}
		else if (status != 0x4000) {
			close(fd);
			Aprint("Error reading 32K 5200 rom, %X", status);
			return FALSE;
		}
		else {
			UBYTE temp_byte;
			if (read(fd, &temp_byte, 1) == 1) {
				/* ABCD EFGH IJ */
				if (!(cart_image = (UBYTE *) malloc(0x8000)))
					return FALSE;
				*cart_image = temp_byte;
				if (read(fd, &cart_image[1], 0x1fff) != 0x1fff)
					return FALSE;
				memcpy(&cart_image[0x2000], &memory[0x6000], 0x6000);	/* IJ CD EF GH :CI */

				memcpy(&memory[0xa000], &cart_image[0x0000], 0x2000);	/* AB CD EF IJ :MEM */

				memcpy(&memory[0x8000], &cart_image[0x0000], 0x2000);	/* AB CD IJ IJ :MEM?ij copy */

				memcpy(&cart_image[0x0000], &memory[0x4000], 0x2000);	/* AB CD EF GH :CI */

				memcpy(&memory[0x5000], &cart_image[0x4000], 0x1000);	/* AE CD IJ IJ :MEM CD dont care? */

				SetHARDWARE(0x4ff6, 0x4ff9);
				SetHARDWARE(0x5ff6, 0x5ff9);
			}
		}
		close(fd);
		/* SetROM (0x4000, 0xbfff); */
		/* cart_type = AGS32_CART; */
		rom_inserted = TRUE;
		status = TRUE;
#ifdef WIN32
		strcpy( current_rom, filename );
#endif
	}
	return status;
}

/*
 * This removes any loaded cartridge ROM files from the emulator
 * It doesn't remove either the OS, FFP or character set ROMS.
 */

int Remove_ROM(void)
{
	if (cart_image) {			/* Release memory allocated for Super Cartridges */
		free(cart_image);
		cart_image = NULL;
	}
	SetRAM(0x8000, 0xbfff);		/* Ensure cartridge area is RAM */
	cart_type = NO_CART;
	rom_inserted = FALSE;

	return TRUE;
}

int Insert_Cartridge(char *filename)
{
	int status = FALSE;
	int fd;

	fd = open(filename, O_RDONLY | O_BINARY, 0777);
	if (fd != -1) {
		UBYTE header[16];

		read(fd, header, sizeof(header));
		if ((header[0] == 'C') &&
			(header[1] == 'A') &&
			(header[2] == 'R') &&
			(header[3] == 'T')) {
			int type;
			int checksum;

			type = (header[4] << 24) |
				(header[5] << 16) |
				(header[6] << 8) |
				header[7];

			checksum = (header[4] << 24) |
				(header[5] << 16) |
				(header[6] << 8) |
				header[7];

			switch (type) {
#define STD_8K 1
#define STD_16K 2
#define OSS 3
#define AGS 4
			case STD_8K:
				read(fd, &memory[0xa000], 0x2000);
				SetRAM(0x8000, 0x9fff);
				SetROM(0xa000, 0xbfff);
				cart_type = NORMAL8_CART;
				rom_inserted = TRUE;
				status = TRUE;
				break;
			case STD_16K:
				read(fd, &memory[0x8000], 0x4000);
				SetROM(0x8000, 0xbfff);
				cart_type = NORMAL16_CART;
				rom_inserted = TRUE;
				status = TRUE;
				break;
			case OSS:
				cart_image = (UBYTE *) malloc(0x4000);
				if (cart_image) {
					read(fd, cart_image, 0x4000);
					memcpy(&memory[0xa000], cart_image, 0x1000);
					memcpy(&memory[0xb000], cart_image + 0x3000, 0x1000);
					SetRAM(0x8000, 0x9fff);
					SetROM(0xa000, 0xbfff);
					cart_type = OSS_SUPERCART;
					rom_inserted = TRUE;
					status = TRUE;
				}
				break;
			case AGS:
				read(fd, &memory[0x4000], 0x8000);
				close(fd);
				SetROM(0x4000, 0xbfff);
				cart_type = AGS32_CART;
				rom_inserted = TRUE;
				status = TRUE;
				break;
			default:
				Aprint("%s is in unsupported cartridge format %d", filename, type);
				break;
			}
		}
		else {
			Aprint("%s is not a cartridge", filename);
		}
		close(fd);
	}
	return status;
}

void add_esc(UWORD address, UBYTE esc_code)
{
	memory[address++] = 0xf2;	/* ESC */
	memory[address++] = esc_code;	/* ESC CODE */
	memory[address] = 0x60;		/* RTS */
}

void SetSIOEsc( void )
{
	if (enable_sio_patch)
		add_esc(0xe459, ESC_SIOV);
}

void RestoreSIO( void )
{
	memory[0xe459] = 0x4c;
	memory[0xe45a] = 0x59;
	memory[0xe45b] = 0xe9;
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

/*
	Check if ROM patches are enabled - if not return immediately
*/
	if (enable_rom_patches == 0)
		return;
/*
   =====================
   Disable Checksum Test
   =====================
 */

	SetSIOEsc( );

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
		machine = AtariXL;
		PatchOS();
		memcpy(atarixl_os, memory + 0xc000, 0x4000);

		if (cart_type == NO_CART) {
			status = Insert_8K_ROM(atari_basic_filename);
			if (status) {
				memcpy(atari_basic, memory + 0xa000, 0x2000);
				SetRAM(0x0000, 0x9fff);
				SetROM(0xc000, 0xffff);
				SetHARDWARE(0xd000, 0xd7ff);
				rom_inserted = FALSE;
				Coldstart();
			}
			else {
				Aprint("Unable to load %s", atari_basic_filename);
				Atari800_Exit(FALSE);
		  		return FALSE;
			}
		}
		else {
			SetRAM(0x0000, 0xbfff);
			SetROM(0xc000, 0xffff);
			SetHARDWARE(0xd000, 0xd7ff);
			rom_inserted = FALSE;
			Coldstart();
		}
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
		SetROM(0xf800, 0xffff);
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

/* special support of Bounty Bob on Atari5200 */
int bounty_bob1(UWORD addr)
{
	if (addr >= 0x4ff6 && addr <= 0x4ff9) {
		addr -= 0x4ff6;
		memcpy(&memory[0x4000], &cart_image[addr << 12], 0x1000);
		return FALSE;
	}
	return TRUE;
}

int bounty_bob2(UWORD addr)
{
	if (addr >= 0x5ff6 && addr <= 0x5ff9) {
		addr -= 0x5ff6;
		memcpy(&memory[0x5000], &cart_image[(addr << 12) + 0x4000], 0x1000);
		return FALSE;
	}
	return TRUE;
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
	}
	return status;
}

int Initialise_AtariOSB(void)
{
	int status;
	mach_xlxe = FALSE;
	status = load_image(atari_osb_filename, 0xd800, 0x2800);
	if (status) {
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
			SaveUBYTE( &atarixl_os[0], 16384 );
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
			ReadUBYTE( &atarixl_os[0], 16384 );
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
				memcpy(memory + 0xc000, atarixl_os, 0x1000);
				memcpy(memory + 0xd800, atarixl_os + 0x1800, 0x2800);
				SetROM(0xc000, 0xcfff);
				SetROM(0xd800, 0xffff);
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
		if (!rom_inserted) {
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
				memcpy(memory + 0x5000, atarixl_os + 0x1000, 0x800);
				SetROM(0x5000, 0x57ff);
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
	if (!cart_image) return;
	switch (cart_type) {
	case OSS_SUPERCART:
		switch (addr & 0xff0f) {
		case 0xd500:
			memcpy(memory + 0xa000, cart_image, 0x1000);
			break;
		case 0xd504:
			memcpy(memory + 0xa000, cart_image + 0x1000, 0x1000);
			break;
		case 0xd503:
		case 0xd507:
			memcpy(memory + 0xa000, cart_image + 0x2000, 0x1000);
			break;
		}
		break;
	case DB_SUPERCART:
		switch (addr & 0xff07) {
		case 0xd500:
			memcpy(memory + 0x8000, cart_image, 0x2000);
			break;
		case 0xd501:
			memcpy(memory + 0x8000, cart_image + 0x2000, 0x2000);
			break;
		case 0xd506:
			memcpy(memory + 0x8000, cart_image + 0x4000, 0x2000);
			break;
		}
		break;
	default:
		break;
	}
}

void get_charset(char * cs)
{
	if (mach_xlxe)
		memcpy(cs, atarixl_os + 0x2000, 1024);
	else if (machine == Atari5200)
		memcpy(cs, memory + 0xf800, 1024);
	else
		memcpy(cs, memory + 0xe000, 1024);
}

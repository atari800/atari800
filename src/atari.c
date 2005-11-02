/*
 * atari.c - main high-level routines
 *
 * Copyright (c) 1995-1998 David Firth
 * Copyright (c) 1998-2005 Atari800 development team (see DOC/CREDITS)
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
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# elif defined(HAVE_TIME_H)
#  include <time.h>
# endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef WIN32
#include <windows.h>
#endif
#ifdef __EMX__
#define INCL_DOS
#include <os2.h>
#endif
#ifdef __BEOS__
#include <OS.h>
#endif
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#include "antic.h"
#include "atari.h"
#include "binload.h"
#include "cartridge.h"
#include "cassette.h"
#include "cpu.h"
#include "devices.h"
#include "gtia.h"
#include "input.h"
#include "log.h"
#include "memory.h"
#include "monitor.h"
#include "pia.h"
#include "platform.h"
#include "pokey.h"
#include "rtime.h"
#include "sio.h"
#include "util.h"
#if !defined(BASIC) && !defined(CURSES_BASIC)
#include "colours.h"
#include "screen.h"
#endif
#ifndef BASIC
#include "statesav.h"
#ifndef __PLUS
#include "ui.h"
#endif
#endif /* BASIC */
#if defined(SOUND) && !defined(__PLUS)
#include "pokeysnd.h"
#include "sndsave.h"
#include "sound.h"
#endif

#ifdef __PLUS
#ifdef _WX_
#include "export.h"
#else /* _WX_ */
#include "globals.h"
#include "macros.h"
#include "display_win.h"
#include "misc_win.h"
#include "registry.h"
#include "timing.h"
#include "FileService.h"
#include "Helpers.h"
#endif /* _WX_ */
#endif /* __PLUS */

int machine_type = MACHINE_XLXE;
int ram_size = 64;
int tv_mode = TV_PAL;
int disable_basic = TRUE;
int enable_sio_patch = TRUE;

int verbose = FALSE;

int display_screen = FALSE;
int nframes = 0;
int refresh_rate = 1;
int sprite_collisions_in_skipped_frames = FALSE;

static double frametime = 0.1;
double fps;
int percent_atari_speed = 100;
#ifdef BENCHMARK
static double benchmark_start_time;
#endif

char atari_osa_filename[FILENAME_MAX] = FILENAME_NOT_SET;
char atari_osb_filename[FILENAME_MAX] = FILENAME_NOT_SET;
char atari_xlxe_filename[FILENAME_MAX] = FILENAME_NOT_SET;
char atari_5200_filename[FILENAME_MAX] = FILENAME_NOT_SET;
char atari_basic_filename[FILENAME_MAX] = FILENAME_NOT_SET;

int emuos_mode = 1;	/* 0 = never use EmuOS, 1 = use EmuOS if real OS not available, 2 = always use EmuOS */

static double Atari_time(void);

#ifdef HAVE_SIGNAL
RETSIGTYPE sigint_handler(int num)
{
	int restart;

	restart = Atari800_Exit(TRUE);
	if (restart) {
		signal(SIGINT, sigint_handler);
		return;
	}

	exit(0);
}
#endif

/* Now we check address of every escape code, to make sure that the patch
   has been set by the emulator and is not a CIM in Atari program.
   Also switch() for escape codes has been changed to array of pointers
   to functions. This allows adding port-specific patches (e.g. modem device)
   using Atari800_AddEsc, Device_UpdateHATABSEntry etc. without modifying
   atari.c/devices.c. Unfortunately it can't be done for patches in Atari OS,
   because the OS in XL/XE can be disabled.
*/
static UWORD esc_address[256];
static EscFunctionType esc_function[256];

void Atari800_ClearAllEsc(void)
{
	int i;
	for (i = 0; i < 256; i++)
		esc_function[i] = NULL;
}

void Atari800_AddEsc(UWORD address, UBYTE esc_code, EscFunctionType function)
{
	esc_address[esc_code] = address;
	esc_function[esc_code] = function;
	dPutByte(address, 0xf2);			/* ESC */
	dPutByte(address + 1, esc_code);	/* ESC CODE */
}

void Atari800_AddEscRts(UWORD address, UBYTE esc_code, EscFunctionType function)
{
	esc_address[esc_code] = address;
	esc_function[esc_code] = function;
	dPutByte(address, 0xf2);			/* ESC */
	dPutByte(address + 1, esc_code);	/* ESC CODE */
	dPutByte(address + 2, 0x60);		/* RTS */
}

/* 0xd2 is ESCRTS, which works same as pair of ESC and RTS (I think so...).
   So this function does effectively the same as Atari800_AddEscRts,
   except that it modifies 2, not 3 bytes in Atari memory.
   I don't know why it is done that way, so I simply leave it
   unchanged (0xf2/0xd2 are used as in previous versions).
*/
void Atari800_AddEscRts2(UWORD address, UBYTE esc_code, EscFunctionType function)
{
	esc_address[esc_code] = address;
	esc_function[esc_code] = function;
	dPutByte(address, 0xd2);			/* ESCRTS */
	dPutByte(address + 1, esc_code);	/* ESC CODE */
}

void Atari800_RemoveEsc(UBYTE esc_code)
{
	esc_function[esc_code] = NULL;
}

void Atari800_RunEsc(UBYTE esc_code)
{
	if (esc_address[esc_code] == regPC - 2 && esc_function[esc_code] != NULL) {
		esc_function[esc_code]();
		return;
	}
#ifdef CRASH_MENU
	regPC -= 2;
	crash_address = regPC;
	crash_afterCIM = regPC + 2;
	crash_code = dGetByte(crash_address);
	ui();
#else /* CRASH_MENU */
	cim_encountered = 1;
	Aprint("Invalid ESC code %02x at address %04x", esc_code, regPC - 2);
#ifndef __PLUS
	if (!Atari800_Exit(TRUE))
		exit(0);
#else /* __PLUS */
	Atari800_Exit(TRUE);
#endif /* __PLUS */
#endif /* CRASH_MENU */
}

void Atari800_PatchOS(void)
{
	int patched = Device_PatchOS();
	if (enable_sio_patch) {
		UWORD addr_l;
		UWORD addr_s;
		UBYTE save_check_bytes[2];
		/* patch Open() of C: so we know when a leader is processed */
		switch (machine_type) {
		case MACHINE_OSA:
		case MACHINE_OSB:
			addr_l = 0xef74;
			addr_s = 0xefbc;
			save_check_bytes[0] = 0xa0;
			save_check_bytes[1] = 0x80;
			break;
		case MACHINE_XLXE:
			addr_l = 0xfd13;
			addr_s = 0xfd60;
			save_check_bytes[0] = 0xa9;
			save_check_bytes[1] = 0x03;
			break;
		default:
			return;
		}
		/* don't hurt non-standard OSes that may not support cassette at all  */
		if (dGetByte(addr_l)     == 0xa9 && dGetByte(addr_l + 1) == 0x03
		 && dGetByte(addr_l + 2) == 0x8d && dGetByte(addr_l + 3) == 0x2a
		 && dGetByte(addr_l + 4) == 0x02
		 && dGetByte(addr_s)     == save_check_bytes[0]
		 && dGetByte(addr_s + 1) == save_check_bytes[1]
		 && dGetByte(addr_s + 2) == 0x20 && dGetByte(addr_s + 3) == 0x5c
		 && dGetByte(addr_s + 4) == 0xe4) {
			Atari800_AddEsc(addr_l, ESC_COPENLOAD, CASSETTE_LeaderLoad);
			Atari800_AddEsc(addr_s, ESC_COPENSAVE, CASSETTE_LeaderSave);
		}
		Atari800_AddEscRts(0xe459, ESC_SIOV, SIO);
		patched = TRUE;
	}
	else {
		Atari800_RemoveEsc(ESC_COPENLOAD);
		Atari800_RemoveEsc(ESC_COPENSAVE);
		Atari800_RemoveEsc(ESC_SIOV);
	};
	if (patched && machine_type == MACHINE_XLXE) {
		/* Disable Checksum Test */
		dPutByte(0xc314, 0x8e);
		dPutByte(0xc315, 0xff);
		dPutByte(0xc319, 0x8e);
		dPutByte(0xc31a, 0xff);
	}
}

void Warmstart(void)
{
	if (machine_type == MACHINE_OSA || machine_type == MACHINE_OSB) {
		/* RESET key in 400/800 does not reset chips,
		   but only generates RNMI interrupt */
		NMIST = 0x3f;
		NMI();
	}
	else {
		PIA_Reset();
		ANTIC_Reset();
		/* CPU_Reset() must be after PIA_Reset(),
		   because Reset routine vector must be read from OS ROM */
		CPU_Reset();
		/* note: POKEY and GTIA have no Reset pin */
	}
#ifdef __PLUS
	HandleResetEvent();
#endif
}

void Coldstart(void)
{
	PIA_Reset();
	ANTIC_Reset();
	/* CPU_Reset() must be after PIA_Reset(),
	   because Reset routine vector must be read from OS ROM */
	CPU_Reset();
	/* note: POKEY and GTIA have no Reset pin */
#ifdef __PLUS
	HandleResetEvent();
#endif
	/* reset cartridge to power-up state */
	CART_Start();
	/* set Atari OS Coldstart flag */
	dPutByte(0x244, 1);
	/* handle Option key (disable BASIC in XL/XE)
	   and Start key (boot from cassette) */
	consol_index = 2;
	consol_table[2] = 0x0f;
	if (disable_basic && !loading_basic) {
		/* hold Option during reboot */
		consol_table[2] &= ~CONSOL_OPTION;
	}
	if (hold_start) {
		/* hold Start during reboot */
		consol_table[2] &= ~CONSOL_START;
	}
	consol_table[1] = consol_table[2];
}

static int load_image(const char *filename, UBYTE *buffer, int nbytes)
{
	FILE *f;
	int len;

	f = fopen(filename, "rb");
	if (f == NULL) {
		Aprint("Error loading rom: %s", filename);
		return FALSE;
	}
	len = fread(buffer, 1, nbytes, f);
	fclose(f);
	if (len != nbytes) {
		Aprint("Error reading %s", filename);
		return FALSE;
	}
	return TRUE;
}

#include "emuos.h"

#define COPY_EMUOS(padding) do { \
		memset(atari_os, 0, padding); \
		memcpy(atari_os + (padding), emuos_h, 0x2000); \
	} while (0)

static int load_roms(void)
{
	switch (machine_type) {
	case MACHINE_OSA:
		if (emuos_mode == 2)
			COPY_EMUOS(0x0800);
		else if (!load_image(atari_osa_filename, atari_os, 0x2800)) {
			if (emuos_mode == 1)
				COPY_EMUOS(0x0800);
			else
				return FALSE;
		}
		else
			have_basic = load_image(atari_basic_filename, atari_basic, 0x2000);
		break;
	case MACHINE_OSB:
		if (emuos_mode == 2)
			COPY_EMUOS(0x0800);
		else if (!load_image(atari_osb_filename, atari_os, 0x2800)) {
			if (emuos_mode == 1)
				COPY_EMUOS(0x0800);
			else
				return FALSE;
		}
		else
			have_basic = load_image(atari_basic_filename, atari_basic, 0x2000);
		break;
	case MACHINE_XLXE:
		if (emuos_mode == 2)
			COPY_EMUOS(0x2000);
		else if (!load_image(atari_xlxe_filename, atari_os, 0x4000)) {
			if (emuos_mode == 1)
				COPY_EMUOS(0x2000);
			else
				return FALSE;
		}
		else if (!load_image(atari_basic_filename, atari_basic, 0x2000))
			return FALSE;
		xe_bank = 0;
		break;
	case MACHINE_5200:
		if (!load_image(atari_5200_filename, atari_os, 0x800))
			return FALSE;
		break;
	}
	return TRUE;
}

int Atari800_InitialiseMachine(void)
{
	Atari800_ClearAllEsc();
	if (!load_roms())
		return FALSE;
	MEMORY_InitialiseMachine();
	Device_UpdatePatches();
	return TRUE;
}

int Atari800_DetectFileType(const char *filename)
{
	UBYTE header[4];
	int file_length;
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL)
		return AFILE_ERROR;
	if (fread(header, 1, 4, fp) != 4) {
		fclose(fp);
		return AFILE_ERROR;
	}
	switch (header[0]) {
	case 0:
		if (header[1] == 0 && (header[2] != 0 || header[3] != 0) /* && file_length < 37 * 1024 */) {
			fclose(fp);
			return AFILE_BAS;
		}
		break;
	case 0x1f:
		if (header[1] == 0x8b) {
#ifndef HAVE_LIBZ
			fclose(fp);
			Aprint("\"%s\" is a compressed file.");
			Aprint("This executable does not support compressed files. You can uncompress this file");
			Aprint("with an external program that supports gzip (*.gz) files (e.g. gunzip)");
			Aprint("and then load into this emulator.");
			return AFILE_ERROR;
#else /* HAVE_LIBZ */
			gzFile gzf;
			fclose(fp);
			gzf = gzopen(filename, "rb");
			if (gzf == NULL)
				return AFILE_ERROR;
			if (gzread(gzf, header, 4) != 4) {
				gzclose(gzf);
				return AFILE_ERROR;
			}
			gzclose(gzf);
			if (header[0] == 0x96 && header[1] == 0x02)
				return AFILE_ATR_GZ;
			if (header[0] == 'A' && header[1] == 'T' && header[2] == 'A' && header[3] == 'R')
				return AFILE_STATE_GZ;
			return AFILE_XFD_GZ;
#endif /* HAVE_LIBZ */
		}
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		if ((header[1] >= '0' && header[1] <= '9') || header[1] == ' ') {
			fclose(fp);
			return AFILE_LST;
		}
		break;
	case 'A':
		if (header[1] == 'T' && header[2] == 'A' && header[3] == 'R') {
			fclose(fp);
			return AFILE_STATE;
		}
		break;
	case 'C':
		if (header[1] == 'A' && header[2] == 'R' && header[3] == 'T') {
			fclose(fp);
			return AFILE_CART;
		}
		break;
	case 'F':
		if (header[1] == 'U' && header[2] == 'J' && header[3] == 'I') {
			fclose(fp);
			return AFILE_CAS;
		}
		break;
	case 0x96:
		if (header[1] == 0x02) {
			fclose(fp);
			return AFILE_ATR;
		}
		break;
	case 0xf9:
	case 0xfa:
		fclose(fp);
		return AFILE_DCM;
	case 0xff:
		if (header[1] == 0xff && (header[2] != 0xff || header[3] != 0xff)) {
			fclose(fp);
			return AFILE_XEX;
		}
		break;
	default:
		break;
	}
	file_length = Util_flen(fp);
	fclose(fp);
	/* 40K or a-power-of-two between 4K and CART_MAX_SIZE */
	if (file_length >= 4 * 1024 && file_length <= CART_MAX_SIZE
	 && ((file_length & (file_length - 1)) == 0 || file_length == 40 * 1024))
		return AFILE_ROM;
	/* BOOT_TAPE is a raw file containing a program booted from a tape */
	if ((header[1] << 7) == file_length)
		return AFILE_BOOT_TAPE;
	if ((file_length & 0x7f) == 0)
		return AFILE_XFD;
	return AFILE_ERROR;
}

int Atari800_OpenFile(const char *filename, int reboot, int diskno, int readonly)
{
	int type = Atari800_DetectFileType(filename);
	switch (type) {
	case AFILE_ATR:
	case AFILE_XFD:
	case AFILE_ATR_GZ:
	case AFILE_XFD_GZ:
	case AFILE_DCM:
		if (!SIO_Mount(diskno, filename, readonly))
			return AFILE_ERROR;
		if (reboot)
			Coldstart();
		break;
	case AFILE_XEX:
	case AFILE_BAS:
	case AFILE_LST:
		if (!BIN_loader(filename))
			return AFILE_ERROR;
		break;
	case AFILE_CART:
	case AFILE_ROM:
		/* TODO: select format for ROMs; switch 5200 ? */
		if (CART_Insert(filename) != 0)
			return AFILE_ERROR;
		if (reboot)
			Coldstart();
		break;
	case AFILE_CAS:
	case AFILE_BOOT_TAPE:
		if (!CASSETTE_Insert(filename))
			return AFILE_ERROR;
		if (reboot) {
			hold_start = TRUE;
			Coldstart();
		}
		break;
	case AFILE_STATE:
	case AFILE_STATE_GZ:
#ifdef BASIC
		Aprint("State files are not supported in BASIC version");
		return AFILE_ERROR;
#else
		if (!ReadAtariState(filename, "rb"))
			return AFILE_ERROR;
		/* Don't press Option */
		consol_table[1] = consol_table[2] = 0xf;
		break;
#endif
	default:
		break;
	}
	return type;
}

#ifndef __PLUS

void Atari800_FindROMImages(const char *directory, int only_if_not_set)
{
	static char * const rom_filenames[5] = {
		atari_osa_filename,
		atari_osb_filename,
		atari_xlxe_filename,
		atari_5200_filename,
		atari_basic_filename
	};
	static const char * const common_filenames[] = {
		"atariosa.rom", "atari_osa.rom", "atari_os_a.rom",
		"ATARIOSA.ROM", "ATARI_OSA.ROM", "ATARI_OS_A.ROM",
		NULL,
		"atariosb.rom", "atari_osb.rom", "atari_os_b.rom",
		"ATARIOSB.ROM", "ATARI_OSB.ROM", "ATARI_OS_B.ROM",
		NULL,
		"atarixlxe.rom", "atarixl.rom", "atari_xlxe.rom", "atari_xl_xe.rom",
		"ATARIXLXE.ROM", "ATARIXL.ROM", "ATARI_XLXE.ROM", "ATARI_XL_XE.ROM",
		NULL,
		"atari5200.rom", "atar5200.rom", "5200.rom", "5200.bin", "atari_5200.rom",
		"ATARI5200.ROM", "ATAR5200.ROM", "5200.ROM", "5200.BIN", "ATARI_5200.ROM",
		NULL,
		"ataribasic.rom", "ataribas.rom", "basic.rom", "atari_basic.rom",
		"ATARIBASIC.ROM", "ATARIBAS.ROM", "BASIC.ROM", "ATARI_BASIC.ROM",
		NULL
	};
	const char * const *common_filename = common_filenames;
	int i;
	for (i = 0; i < 5; i++) {
		if (!only_if_not_set || Util_filenamenotset(rom_filenames[i])) {
			do {
				char full_filename[FILENAME_MAX];
				Util_catpath(full_filename, directory, *common_filename);
				if (Util_fileexists(full_filename)) {
					strcpy(rom_filenames[i], full_filename);
					break;
				}
			} while (*++common_filename != NULL);
		}
		while (*common_filename++ != NULL);
	}
}

/* If another default path config path is defined use it
   otherwise use the default one */
#ifndef DEFAULT_CFG_NAME
#define DEFAULT_CFG_NAME ".atari800.cfg"
#endif

#ifndef SYSTEM_WIDE_CFG_FILE
#define SYSTEM_WIDE_CFG_FILE "/etc/atari800.cfg"
#endif

static char rtconfig_filename[FILENAME_MAX];

int Atari800_ReadConfig(const char *alternate_config_filename)
{
	FILE *fp;
	const char *fname = rtconfig_filename;
	char string[256];
#ifndef BASIC
	int was_obsolete_dir = FALSE;
#endif

#ifdef SUPPORTS_ATARI_CONFIGINIT
	Atari_ConfigInit();
#endif

	/* if alternate config filename is passed then use it */
	if (alternate_config_filename != NULL && *alternate_config_filename > 0) {
		Util_strlcpy(rtconfig_filename, alternate_config_filename, FILENAME_MAX);
	}
	/* else use the default config name under the HOME folder */
	else {
		char *home = getenv("HOME");
		if (home != NULL)
			Util_catpath(rtconfig_filename, home, DEFAULT_CFG_NAME);
		else
			strcpy(rtconfig_filename, DEFAULT_CFG_NAME);
	}

	fp = fopen(fname, "r");
	if (fp == NULL) {
		Aprint("User config file '%s' not found.", rtconfig_filename);

#ifdef SYSTEM_WIDE_CFG_FILE
		/* try system wide config file */
		fname = SYSTEM_WIDE_CFG_FILE;
		Aprint("Trying system wide config file: %s", fname);
		fp = fopen(fname, "r");
#endif
		if (fp == NULL) {
			Aprint("No configuration file found, will create fresh one from scratch:");
			return FALSE;
		}
	}

	fgets(string, sizeof(string), fp);

	Aprint("Using Atari800 config file: %s\nCreated by %s", fname, string);

	while (fgets(string, sizeof(string), fp)) {
		char *ptr;
		Util_chomp(string);
		ptr = strchr(string, '=');
		if (ptr != NULL) {
			*ptr++ = '\0';
			Util_trim(string);
			Util_trim(ptr);

			if (strcmp(string, "OS/A_ROM") == 0)
				Util_strlcpy(atari_osa_filename, ptr, sizeof(atari_osa_filename));
			else if (strcmp(string, "OS/B_ROM") == 0)
				Util_strlcpy(atari_osb_filename, ptr, sizeof(atari_osb_filename));
			else if (strcmp(string, "XL/XE_ROM") == 0)
				Util_strlcpy(atari_xlxe_filename, ptr, sizeof(atari_xlxe_filename));
			else if (strcmp(string, "BASIC_ROM") == 0)
				Util_strlcpy(atari_basic_filename, ptr, sizeof(atari_basic_filename));
			else if (strcmp(string, "5200_ROM") == 0)
				Util_strlcpy(atari_5200_filename, ptr, sizeof(atari_5200_filename));
#ifdef BASIC
			else if (strcmp(string, "ATARI_FILES_DIR") == 0
				  || strcmp(string, "SAVED_FILES_DIR") == 0
				  || strcmp(string, "DISK_DIR") == 0 || strcmp(string, "ROM_DIR") == 0
				  || strcmp(string, "EXE_DIR") == 0 || strcmp(string, "STATE_DIR") == 0)
				/* do nothing */;
#else
			else if (strcmp(string, "ATARI_FILES_DIR") == 0) {
				if (n_atari_files_dir >= MAX_DIRECTORIES)
					Aprint("All ATARI_FILES_DIR slots used!");
				else
					Util_strlcpy(atari_files_dir[n_atari_files_dir++], ptr, FILENAME_MAX);
			}
			else if (strcmp(string, "SAVED_FILES_DIR") == 0) {
				if (n_saved_files_dir >= MAX_DIRECTORIES)
					Aprint("All SAVED_FILES_DIR slots used!");
				else
					Util_strlcpy(saved_files_dir[n_saved_files_dir++], ptr, FILENAME_MAX);
			}
			else if (strcmp(string, "DISK_DIR") == 0 || strcmp(string, "ROM_DIR") == 0
				  || strcmp(string, "EXE_DIR") == 0 || strcmp(string, "STATE_DIR") == 0) {
				/* ignore blank and "." values */
				if (ptr[0] != '\0' && (ptr[0] != '.' || ptr[1] != '\0'))
					was_obsolete_dir = TRUE;
			}
#endif
			else if (strcmp(string, "H1_DIR") == 0)
				Util_strlcpy(atari_h_dir[0], ptr, FILENAME_MAX);
			else if (strcmp(string, "H2_DIR") == 0)
				Util_strlcpy(atari_h_dir[1], ptr, FILENAME_MAX);
			else if (strcmp(string, "H3_DIR") == 0)
				Util_strlcpy(atari_h_dir[2], ptr, FILENAME_MAX);
			else if (strcmp(string, "H4_DIR") == 0)
				Util_strlcpy(atari_h_dir[3], ptr, FILENAME_MAX);
			else if (strcmp(string, "HD_READ_ONLY") == 0)
				h_read_only = Util_sscandec(ptr);

			else if (strcmp(string, "PRINT_COMMAND") == 0) {
				if (!Device_SetPrintCommand(ptr))
					Aprint("Unsafe PRINT_COMMAND ignored");
			}

			else if (strcmp(string, "SCREEN_REFRESH_RATIO") == 0)
				refresh_rate = Util_sscandec(ptr);
			else if (strcmp(string, "DISABLE_BASIC") == 0)
				disable_basic = Util_sscanbool(ptr);

			else if (strcmp(string, "ENABLE_SIO_PATCH") == 0) {
				enable_sio_patch = Util_sscanbool(ptr);
			}
			else if (strcmp(string, "ENABLE_H_PATCH") == 0) {
				enable_h_patch = Util_sscanbool(ptr);
			}
			else if (strcmp(string, "ENABLE_P_PATCH") == 0) {
				enable_p_patch = Util_sscanbool(ptr);
			}
			else if (strcmp(string, "ENABLE_R_PATCH") == 0) {
				enable_r_patch = Util_sscanbool(ptr);
			}

			else if (strcmp(string, "ENABLE_NEW_POKEY") == 0) {
#ifdef SOUND
				enable_new_pokey = Util_sscanbool(ptr);
#endif
			}
			else if (strcmp(string, "STEREO_POKEY") == 0) {
#ifdef STEREO_SOUND
				stereo_enabled = Util_sscanbool(ptr);
#endif
			}
			else if (strcmp(string, "SPEAKER_SOUND") == 0) {
#ifdef CONSOLE_SOUND
				console_sound_enabled = Util_sscanbool(ptr);
#endif
			}
			else if (strcmp(string, "SERIO_SOUND") == 0) {
#ifdef SERIO_SOUND
				serio_sound_enabled = Util_sscanbool(ptr);
#endif
			}
			else if (strcmp(string, "MACHINE_TYPE") == 0) {
				if (strcmp(ptr, "Atari OS/A") == 0)
					machine_type = MACHINE_OSA;
				else if (strcmp(ptr, "Atari OS/B") == 0)
					machine_type = MACHINE_OSB;
				else if (strcmp(ptr, "Atari XL/XE") == 0)
					machine_type = MACHINE_XLXE;
				else if (strcmp(ptr, "Atari 5200") == 0)
					machine_type = MACHINE_5200;
				else
					Aprint("Invalid machine type: %s", ptr);
			}
			else if (strcmp(string, "RAM_SIZE") == 0) {
				if (strcmp(ptr, "16") == 0)
					ram_size = 16;
				else if (strcmp(ptr, "48") == 0)
					ram_size = 48;
				else if (strcmp(ptr, "52") == 0)
					ram_size = 52;
				else if (strcmp(ptr, "64") == 0)
					ram_size = 64;
				else if (strcmp(ptr, "128") == 0)
					ram_size = 128;
				else if (strcmp(ptr, "320 (RAMBO)") == 0)
					ram_size = RAM_320_RAMBO;
				else if (strcmp(ptr, "320 (COMPY SHOP)") == 0)
					ram_size = RAM_320_COMPY_SHOP;
				else if (strcmp(ptr, "576") == 0)
					ram_size = 576;
				else if (strcmp(ptr, "1088") == 0)
					ram_size = 1088;
				else
					Aprint("Invalid RAM size: %s", ptr);
			}
			else if (strcmp(string, "DEFAULT_TV_MODE") == 0) {
				if (strcmp(ptr, "PAL") == 0)
					tv_mode = TV_PAL;
				else if (strcmp(ptr, "NTSC") == 0)
					tv_mode = TV_NTSC;
				else
					Aprint("Invalid TV Mode: %s", ptr);
			}
			else {
#ifdef SUPPORTS_ATARI_CONFIGURE
				if (!Atari_Configure(string, ptr)) {
					Aprint("Unrecognized variable or bad parameters: '%s=%s'", string, ptr);
				}
#else
				Aprint("Unrecognized variable: %s", string);
#endif
			}
		}
		else {
			Aprint("Ignored config line: %s", string);
		}
	}

	fclose(fp);
#ifndef BASIC
	if (was_obsolete_dir) {
		Aprint(
			"DISK_DIR, ROM_DIR, EXE_DIR and STATE_DIR configuration options\n"
			"are no longer supported. Please use ATARI_FILES_DIR\n"
			"and SAVED_FILES_DIR in your Atari800 configuration file.");
	}
#endif
	return TRUE;
}

int Atari800_WriteConfig(void)
{
	FILE *fp;
	int i;
	static const char * const machine_type_string[4] = {
		"OS/A", "OS/B", "XL/XE", "5200"
	};

	fp = fopen(rtconfig_filename, "w");
	if (fp == NULL) {
		perror(rtconfig_filename);
		Aprint("Cannot write to config file: %s", rtconfig_filename);
		return FALSE;
	}
	Aprint("Writing config file: %s", rtconfig_filename);

	fprintf(fp, "%s\n", ATARI_TITLE);
	fprintf(fp, "OS/A_ROM=%s\n", atari_osa_filename);
	fprintf(fp, "OS/B_ROM=%s\n", atari_osb_filename);
	fprintf(fp, "XL/XE_ROM=%s\n", atari_xlxe_filename);
	fprintf(fp, "BASIC_ROM=%s\n", atari_basic_filename);
	fprintf(fp, "5200_ROM=%s\n", atari_5200_filename);
#ifndef BASIC
	for (i = 0; i < n_atari_files_dir; i++)
		fprintf(fp, "ATARI_FILES_DIR=%s\n", atari_files_dir[i]);
	for (i = 0; i < n_saved_files_dir; i++)
		fprintf(fp, "SAVED_FILES_DIR=%s\n", saved_files_dir[i]);
#endif
	for (i = 0; i < 4; i++)
		fprintf(fp, "H%c_DIR=%s\n", '1' + i, atari_h_dir[i]);
	fprintf(fp, "HD_READ_ONLY=%d\n", h_read_only);

#ifdef HAVE_SYSTEM
	fprintf(fp, "PRINT_COMMAND=%s\n", print_command);
#endif

#ifndef BASIC
	fprintf(fp, "SCREEN_REFRESH_RATIO=%d\n", refresh_rate);
#endif

	fprintf(fp, "MACHINE_TYPE=Atari %s\n", machine_type_string[machine_type]);

	fprintf(fp, "RAM_SIZE=");
	switch (ram_size) {
	case RAM_320_RAMBO:
		fprintf(fp, "320 (RAMBO)\n");
		break;
	case RAM_320_COMPY_SHOP:
		fprintf(fp, "320 (COMPY SHOP)\n");
		break;
	default:
		fprintf(fp, "%d\n", ram_size);
		break;
	}

	fprintf(fp, (tv_mode == TV_PAL) ? "DEFAULT_TV_MODE=PAL\n" : "DEFAULT_TV_MODE=NTSC\n");

	fprintf(fp, "DISABLE_BASIC=%d\n", disable_basic);
	fprintf(fp, "ENABLE_SIO_PATCH=%d\n", enable_sio_patch);
	fprintf(fp, "ENABLE_H_PATCH=%d\n", enable_h_patch);
	fprintf(fp, "ENABLE_P_PATCH=%d\n", enable_p_patch);
#ifdef R_IO_DEVICE
	fprintf(fp, "ENABLE_R_PATCH=%d\n", enable_r_patch);
#endif

#ifdef SOUND
	fprintf(fp, "ENABLE_NEW_POKEY=%d\n", enable_new_pokey);
#ifdef STEREO_SOUND
	fprintf(fp, "STEREO_POKEY=%d\n", stereo_enabled);
#endif
#ifdef CONSOLE_SOUND
	fprintf(fp, "SPEAKER_SOUND=%d\n", console_sound_enabled);
#endif
#ifdef SERIO_SOUND
	fprintf(fp, "SERIO_SOUND=%d\n", serio_sound_enabled);
#endif
#endif /* SOUND */

#ifdef SUPPORTS_ATARI_CONFIGSAVE
	Atari_ConfigSave(fp);
#endif

	fclose(fp);
	return TRUE;
}

#endif /* __PLUS */

int Atari800_Initialise(int *argc, char *argv[])
{
	int i, j;
	const char *rom_filename = NULL;
	const char *run_direct = NULL;
#ifndef BASIC
	const char *state_file = NULL;
#endif
#ifdef __PLUS
	/* Atari800Win PLus doesn't use configuration files,
	   it reads configuration from the Registry */
#ifndef _WX_
	int bUpdateRegistry = (*argc > 1);
#endif
	int bTapeFile = FALSE;
	int nCartType = cart_type;

	/* It is necessary because of the CART_Start (there must not be the
	   registry-read value available at startup) */
	cart_type = CART_NONE;

#ifndef _WX_
	/* Print the time info in the "Log file" window */
	Misc_PrintTime();

	/* Force screen refreshing */
	g_nTestVal = _GetRefreshRate() - 1;

	g_ulAtariState = ATARI_UNINITIALIZED;
#endif /* _WX_ */

#else /* __PLUS */
	const char *rtconfig_filename = NULL;
	int got_config;
	int help_only = FALSE;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-config") == 0) {
			rtconfig_filename = argv[++i];
		}
		else if (strcmp(argv[i], "-v") == 0 ||
				 strcmp(argv[i], "-version") == 0 ||
				 strcmp(argv[i], "--version") == 0) {
			printf("%s\n", ATARI_TITLE);
			return FALSE;
		}
		else if (strcmp(argv[i], "--usage") == 0 ||
				 strcmp(argv[i], "--help") == 0) {
			argv[j++] = "-help";
		}
		else if (strcmp(argv[i], "-verbose") == 0) {
			verbose = TRUE;
		}
		else {
			argv[j++] = argv[i];
		}
	}

	*argc = j;

	got_config = Atari800_ReadConfig(rtconfig_filename);

	/* try to find ROM images if the configuration file is not found
	   or it does not specify some ROM paths (blank paths count as specified) */
	Atari800_FindROMImages("", TRUE); /* current directory */
#if defined(unix) || defined(__unix__) || defined(__linux__)
	Atari800_FindROMImages("/usr/share/atari800", TRUE);
#endif
	if (*argc > 0 && argv[0] != NULL) {
		char atari800_exe_dir[FILENAME_MAX];
		char atari800_exe_rom_dir[FILENAME_MAX];
		/* the directory of the Atari800 program */
		Util_splitpath(argv[0], atari800_exe_dir, NULL);
		Atari800_FindROMImages(atari800_exe_dir, TRUE);
		/* "rom" and "ROM" subdirectories of this directory */
		Util_catpath(atari800_exe_rom_dir, atari800_exe_dir, "rom");
		Atari800_FindROMImages(atari800_exe_rom_dir, TRUE);
/* skip "ROM" on systems that are known to be case-insensitive */
#if !defined(DJGPP) && !defined(WIN32)
		Util_catpath(atari800_exe_rom_dir, atari800_exe_dir, "ROM");
		Atari800_FindROMImages(atari800_exe_rom_dir, TRUE);
#endif
	}
	/* finally if nothing is found, set some defaults to make
	   the configuration file easier to edit */
	if (Util_filenamenotset(atari_osa_filename))
		strcpy(atari_osa_filename, "atariosa.rom");
	if (Util_filenamenotset(atari_osb_filename))
		strcpy(atari_osb_filename, "atariosb.rom");
	if (Util_filenamenotset(atari_xlxe_filename))
		strcpy(atari_xlxe_filename, "atarixl.rom");
	if (Util_filenamenotset(atari_5200_filename))
		strcpy(atari_5200_filename, "5200.rom");
	if (Util_filenamenotset(atari_basic_filename))
		strcpy(atari_basic_filename, "ataribas.rom");

	/* if no configuration file read, try to save one with the defaults */
	if (!got_config)
		Atari800_WriteConfig();

#endif /* __PLUS */

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-atari") == 0) {
			if (machine_type != MACHINE_OSA) {
				machine_type = MACHINE_OSB;
				ram_size = 48;
			}
		}
		else if (strcmp(argv[i], "-xl") == 0) {
			machine_type = MACHINE_XLXE;
			ram_size = 64;
		}
		else if (strcmp(argv[i], "-xe") == 0) {
			machine_type = MACHINE_XLXE;
			ram_size = 128;
		}
		else if (strcmp(argv[i], "-320xe") == 0) {
			machine_type = MACHINE_XLXE;
			ram_size = RAM_320_COMPY_SHOP;
		}
		else if (strcmp(argv[i], "-rambo") == 0) {
			machine_type = MACHINE_XLXE;
			ram_size = RAM_320_RAMBO;
		}
		else if (strcmp(argv[i], "-5200") == 0) {
			machine_type = MACHINE_5200;
			ram_size = 16;
		}
		else if (strcmp(argv[i], "-nobasic") == 0)
			disable_basic = TRUE;
		else if (strcmp(argv[i], "-basic") == 0)
			disable_basic = FALSE;
		else if (strcmp(argv[i], "-nopatch") == 0)
			enable_sio_patch = FALSE;
		else if (strcmp(argv[i], "-nopatchall") == 0)
			enable_sio_patch = enable_h_patch = enable_p_patch = enable_r_patch = FALSE;
		else if (strcmp(argv[i], "-pal") == 0)
			tv_mode = TV_PAL;
		else if (strcmp(argv[i], "-ntsc") == 0)
			tv_mode = TV_NTSC;
		else if (strcmp(argv[i], "-a") == 0) {
			machine_type = MACHINE_OSA;
			ram_size = 48;
		}
		else if (strcmp(argv[i], "-b") == 0) {
			machine_type = MACHINE_OSB;
			ram_size = 48;
		}
		else if (strcmp(argv[i], "-emuos") == 0)
			emuos_mode = 2;
		else if (strcmp(argv[i], "-c") == 0) {
			if (ram_size == 48)
				ram_size = 52;
		}
		else {
			/* parameters that take additional argument follow here */
			int i_a = (i + 1 < *argc);		/* is argument available? */
			int a_m = FALSE;				/* error, argument missing! */

			if (strcmp(argv[i], "-osa_rom") == 0) {
				if (i_a) Util_strlcpy(atari_osa_filename, argv[++i], sizeof(atari_osa_filename)); else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-osb_rom") == 0) {
				if (i_a) Util_strlcpy(atari_osb_filename, argv[++i], sizeof(atari_osb_filename)); else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-xlxe_rom") == 0) {
				if (i_a) Util_strlcpy(atari_xlxe_filename, argv[++i], sizeof(atari_xlxe_filename)); else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-5200_rom") == 0) {
				if (i_a) Util_strlcpy(atari_5200_filename, argv[++i], sizeof(atari_5200_filename)); else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-basic_rom") == 0) {
				if (i_a) Util_strlcpy(atari_basic_filename, argv[++i], sizeof(atari_basic_filename)); else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-cart") == 0) {
				if (i_a) rom_filename = argv[++i]; else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-run") == 0) {
				if (i_a) run_direct = argv[++i]; else a_m = TRUE;
			}
#ifndef BASIC
			/* The BASIC version does not support state files, because:
			   1. It has no ability to save state files, because of lack of UI.
			   2. It uses a simplified emulation, so the state files would be
			      incompatible with other versions.
			   3. statesav is not compiled in to make the executable smaller. */
			else if (strcmp(argv[i], "-state") == 0) {
				if (i_a) state_file = argv[++i]; else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-refresh") == 0) {
				if (i_a) {
					refresh_rate = Util_sscandec(argv[++i]);
					if (refresh_rate < 1) {
						Aprint("Invalid refresh rate, using 1");
						refresh_rate = 1;
					}
				}
				else
					a_m = TRUE;
			}
#endif
			else {
				/* all options known to main module tried but none matched */

				if (strcmp(argv[i], "-help") == 0) {
#ifndef __PLUS
					help_only = TRUE;
					Aprint("\t-config <file>   Specify Alternate Configuration File");
#endif
					Aprint("\t-atari           Emulate Atari 800");
					Aprint("\t-xl              Emulate Atari 800XL");
					Aprint("\t-xe              Emulate Atari 130XE");
					Aprint("\t-320xe           Emulate Atari 320XE (COMPY SHOP)");
					Aprint("\t-rambo           Emulate Atari 320XE (RAMBO)");
					Aprint("\t-5200            Emulate Atari 5200 Games System");
					Aprint("\t-nobasic         Turn off Atari BASIC ROM");
					Aprint("\t-basic           Turn on Atari BASIC ROM");
					Aprint("\t-pal             Enable PAL TV mode");
					Aprint("\t-ntsc            Enable NTSC TV mode");
					Aprint("\t-osa_rom <file>  Load OS A ROM from file");
					Aprint("\t-osb_rom <file>  Load OS B ROM from file");
					Aprint("\t-xlxe_rom <file> Load XL/XE ROM from file");
					Aprint("\t-5200_rom <file> Load 5200 ROM from file");
					Aprint("\t-basic_rom <fil> Load BASIC ROM from file");
					Aprint("\t-cart <file>     Install cartridge (raw or CART format)");
					Aprint("\t-run <file>      Run Atari program (COM, EXE, XEX, BAS, LST)");
#ifndef BASIC
					Aprint("\t-state <file>    Load saved-state file");
					Aprint("\t-refresh <rate>  Specify screen refresh rate");
#endif
					Aprint("\t-nopatch         Don't patch SIO routine in OS");
					Aprint("\t-nopatchall      Don't patch OS at all, H: device won't work");
					Aprint("\t-a               Use OS A");
					Aprint("\t-b               Use OS B");
					Aprint("\t-c               Enable RAM between 0xc000 and 0xcfff in Atari 800");
					Aprint("\t-v               Show version/release number");
				}

				/* copy this option for platform/module specific evaluation */
				argv[j++] = argv[i];
			}

			/* this is the end of the additional argument check */
			if (a_m) {
				printf("Missing argument for '%s'\n", argv[i]);
				return FALSE;
			}
		}
	}

	*argc = j;

#if !defined(BASIC) && !defined(CURSES_BASIC)
	Palette_Initialise(argc, argv);
#endif
	Device_Initialise(argc, argv);
	RTIME_Initialise(argc, argv);
	SIO_Initialise (argc, argv);
	CASSETTE_Initialise(argc, argv);
#ifndef BASIC
	INPUT_Initialise(argc, argv);
#endif
#ifndef DONT_DISPLAY
	/* Platform Specific Initialisation */
	Atari_Initialise(argc, argv);
#endif
#if !defined(BASIC) && !defined(CURSES_BASIC)
	Screen_Initialise(argc, argv);
#endif
	/* Initialise Custom Chips */
	ANTIC_Initialise(argc, argv);
	GTIA_Initialise(argc, argv);
	PIA_Initialise(argc, argv);
	POKEY_Initialise(argc, argv);

#ifndef __PLUS

	if (help_only) {
		Atari800_Exit(FALSE);
		return FALSE;
	}

	/* Configure Atari System */
	Atari800_InitialiseMachine();

#else /* __PLUS */

	if (!InitialiseMachine()) {
#ifndef _WX_
		if (bUpdateRegistry)
			WriteAtari800Registry();
#endif
		return FALSE;
	}

#endif /* __PLUS */

	/* Auto-start files left on the command line */
	j = 1; /* diskno */
	for (i = 1; i < *argc; i++) {
		if (j > 8) {
			/* The remaining arguments are not necessary disk images, but ignore them... */
			Aprint("Too many disk image filenames on the command line (max. 8).");
			break;
		}
		switch (Atari800_OpenFile(argv[i], i == 1, j, FALSE)) {
		case AFILE_ERROR:
			Aprint("Error opening \"%s\"", argv[i]);
			break;
		case AFILE_ATR:
		case AFILE_XFD:
		case AFILE_ATR_GZ:
		case AFILE_XFD_GZ:
		case AFILE_DCM:
			j++;
			break;
		default:
			break;
		}
	}

	/* Install requested ROM cartridge */
	if (rom_filename) {
		int r = CART_Insert(rom_filename);
		if (r < 0) {
			Aprint("Error inserting cartridge \"%s\": %s", rom_filename,
			r == CART_CANT_OPEN ? "Can't open file" :
			r == CART_BAD_FORMAT ? "Bad format" :
			r == CART_BAD_CHECKSUM ? "Bad checksum" :
			"Unknown error");
		}
		if (r > 0) {
#ifdef BASIC
			Aprint("Raw cartridge images not supported in BASIC version!");
#else /* BASIC */

#ifndef __PLUS
			ui_is_active = TRUE;
			cart_type = SelectCartType(r);
			ui_is_active = FALSE;
#else /* __PLUS */
			cart_type = (CART_NONE == nCartType ? SelectCartType(r) : nCartType);
#endif /* __PLUS */
			CART_Start();

#endif /* BASIC */
		}
#ifndef __PLUS
		if (cart_type != CART_NONE) {
			int for5200 = CART_IsFor5200(cart_type);
			if (for5200 && machine_type != MACHINE_5200) {
				machine_type = MACHINE_5200;
				ram_size = 16;
				Atari800_InitialiseMachine();
			}
			else if (!for5200 && machine_type == MACHINE_5200) {
				machine_type = MACHINE_XLXE;
				ram_size = 64;
				Atari800_InitialiseMachine();
			}
		}
#endif /* __PLUS */
	}

	/* Load Atari executable, if any */
	if (run_direct != NULL)
		BIN_loader(run_direct);

#ifndef BASIC
	/* Load state file */
	if (state_file != NULL) {
		if (ReadAtariState(state_file, "rb"))
			/* Don't press Option */
			consol_table[1] = consol_table[2] = 0x0f;
	}
#endif

#ifdef HAVE_SIGNAL
	/* Install CTRL-C Handler */
	signal(SIGINT, sigint_handler);
#endif

#ifdef __PLUS
#ifndef _WX_
	/* Update the Registry if any parameters were specified */
	if (bUpdateRegistry)
		WriteAtari800Registry();
	Timer_Start(FALSE);
#endif /* _WX_ */
	g_ulAtariState &= ~ATARI_UNINITIALIZED;
#endif /* __PLUS */

#ifdef BENCHMARK
	benchmark_start_time = Atari_time();
#endif

	return TRUE;
}

int Atari800_Exit(int run_monitor)
{
	int restart;

#ifdef __PLUS
	if (cim_encountered)
		g_ulAtariState |= ATARI_CRASHED;
#endif

	if (verbose) {
		Aprint("Current frames per second: %f", fps);
	}
	restart = Atari_Exit(run_monitor);
#ifndef __PLUS
	if (!restart) {
		SIO_Exit();	/* umount disks, so temporary files are deleted */
#ifdef SOUND
		CloseSoundFile();
#endif
	}
#endif /* __PLUS */
	return restart;
}

UBYTE Atari800_GetByte(UWORD addr)
{
	UBYTE byte = 0xff;
	switch (addr & 0xff00) {
	case 0x4f00:
	case 0x8f00:
		CART_BountyBob1(addr);
		byte = 0;
		break;
	case 0x5f00:
	case 0x9f00:
		CART_BountyBob2(addr);
		byte = 0;
		break;
	case 0xd000:				/* GTIA */
	case 0xc000:				/* GTIA - 5200 */
		byte = GTIA_GetByte(addr);
		break;
	case 0xd200:				/* POKEY */
	case 0xe800:				/* POKEY - 5200 */
	case 0xeb00:				/* POKEY - 5200 */
		byte = POKEY_GetByte(addr);
		break;
	case 0xd300:				/* PIA */
		byte = PIA_GetByte(addr);
		break;
	case 0xd400:				/* ANTIC */
		byte = ANTIC_GetByte(addr);
		break;
	case 0xd500:				/* bank-switching cartridges, RTIME-8 */
		byte = CART_GetByte(addr);
		break;
	default:
		break;
	}

	return byte;
}

void Atari800_PutByte(UWORD addr, UBYTE byte)
{
	switch (addr & 0xff00) {
	case 0x4f00:
	case 0x8f00:
		CART_BountyBob1(addr);
		break;
	case 0x5f00:
	case 0x9f00:
		CART_BountyBob2(addr);
		break;
	case 0xd000:				/* GTIA */
	case 0xc000:				/* GTIA - 5200 */
		GTIA_PutByte(addr, byte);
		break;
	case 0xd200:				/* POKEY */
	case 0xe800:				/* POKEY - 5200 AAA added other pokey space */
	case 0xeb00:				/* POKEY - 5200 */
		POKEY_PutByte(addr, byte);
		break;
	case 0xd300:				/* PIA */
		PIA_PutByte(addr, byte);
		break;
	case 0xd400:				/* ANTIC */
		ANTIC_PutByte(addr, byte);
		break;
	case 0xd500:				/* bank-switching cartridges, RTIME-8 */
		CART_PutByte(addr, byte);
		break;
	default:
		break;
	}
}

void Atari800_UpdatePatches(void)
{
	switch (machine_type) {
	case MACHINE_OSA:
	case MACHINE_OSB:
		/* Restore unpatched OS */
		dCopyToMem(atari_os, 0xd800, 0x2800);
		/* Set patches */
		Atari800_PatchOS();
		Device_UpdatePatches();
		break;
	case MACHINE_XLXE:
		/* Don't patch if OS disabled */
		if ((PORTB & 1) == 0)
			break;
		/* Restore unpatched OS */
		dCopyToMem(atari_os, 0xc000, 0x1000);
		dCopyToMem(atari_os + 0x1800, 0xd800, 0x2800);
		/* Set patches */
		Atari800_PatchOS();
		Device_UpdatePatches();
		break;
	default:
		break;
	}
}

#ifndef __PLUS

#ifndef USE_CLOCK

static double Atari_time(void)
{
#ifdef WIN32
	return GetTickCount() * 1e-3;
#elif defined(DJGPP)
	/* DJGPP has gettimeofday, but it's not more accurate than uclock */
	return uclock() * (1.0 / UCLOCKS_PER_SEC);
#elif defined(HAVE_GETTIMEOFDAY)
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec + 1e-6 * tp.tv_usec;
#elif defined(HAVE_UCLOCK)
	return uclock() * (1.0 / UCLOCKS_PER_SEC);
#elif defined(HAVE_CLOCK)
#define USE_CLOCK
	return clock() * (1.0 / CLK_TCK);
#else
#error No function found for Atari_time()
#endif
}

#endif /* USE_CLOCK */

#ifndef USE_CLOCK

static void Atari_sleep(double s)
{
	if (s > 0) {
#ifdef WIN32
		Sleep((DWORD) (s * 1e3));
#elif defined(DJGPP)
		/* DJGPP has usleep and select, but they don't work that good */
		/* XXX: find out why */
		double curtime = Atari_time();
		while ((curtime + s) > Atari_time());
#elif defined(HAVE_NANOSLEEP)
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = s * 1e9;
		nanosleep(&ts, NULL);
#elif defined(HAVE_USLEEP)
		usleep(s * 1e6);
#elif defined(__BEOS__)
		/* added by Walter Las for BeOS */
		snooze(s * 1e6);
#elif defined(__EMX__)
		/* added by Brian Smith for os/2 */
		DosSleep(s);
#elif defined(HAVE_SELECT)
		/* linux */
		struct timeval tp;
		tp.tv_sec = 0;
		tp.tv_usec = s * 1e6;
		select(1, NULL, NULL, NULL, &tp);
#else
		double curtime = Atari_time();
		while ((curtime + s) > Atari_time());
#endif
	}
}

#endif /* USE_CLOCK */

void atari_sync(void)
{
#ifdef USE_CLOCK
	static ULONG nextclock = 1;	/* put here a non-zero value to enable speed regulator */
	/* on Atari Falcon CLK_TCK = 200 (i.e. 5 ms granularity) */
	/* on DOS (DJGPP) CLK_TCK = 91 (not too precise, but should work anyway) */
	if (nextclock) {
		ULONG curclock;
		do {
			curclock = clock();
		} while (curclock < nextclock);

		nextclock = curclock + (CLK_TCK / (tv_mode == TV_PAL ? 50 : 60));
	}
#else /* USE_CLOCK */
	double deltatime;
#ifdef ALTERNATE_SYNC_WITH_HOST
	if (ui_is_active)
		deltatime = (tv_mode == TV_PAL) ? (1.0 / 50.0) : (1.0 / 60.0);
	else
		deltatime = (((float)refresh_rate) / (tv_mode == TV_PAL ? 50.0f : 60.0f));
#else
	deltatime = (tv_mode == TV_PAL) ? (1.0 / 50.0) : (1.0 / 60.0);
#endif
	if (deltatime > 0.0) { /* TODO: implement fullspeed mode */
		static double lasttime = 0, lastcurtime = 0;
		double curtime = Atari_time();
		Atari_sleep(lasttime + deltatime - curtime);
		curtime = Atari_time();

		/* make average time */
		frametime = (frametime * 4.0 + curtime - lastcurtime) * 0.2;
		fps = 1.0 / frametime;
		lastcurtime = curtime;

		lasttime += deltatime;
		if ((lasttime + deltatime) < curtime)
			lasttime = curtime;
	}
	percent_atari_speed = (int) (100.0 * deltatime / frametime + 0.5);
#endif /* USE_CLOCK */
}

#ifdef USE_CURSES
void curses_clear_screen(void);
#endif

#if defined(BASIC) || defined(VERY_SLOW) || defined(CURSES_BASIC)

#ifdef CURSES_BASIC
void curses_display_line(int anticmode, const UBYTE *screendata);
#endif

static int scanlines_to_dl;

/* steal cycles and generate DLI */
static void basic_antic_scanline(void)
{
	static UBYTE IR = 0;
	static const UBYTE mode_scanlines[16] =
		{ 0, 0, 8, 10, 8, 16, 8, 16, 8, 4, 4, 2, 1, 2, 1, 1 };
	static const UBYTE mode_bytes[16] =
		{ 0, 0, 40, 40, 40, 40, 20, 20, 10, 10, 20, 20, 20, 40, 40, 40 };
	static const UBYTE font40_cycles[4] = { 0, 32, 40, 47 };
	static const UBYTE font20_cycles[4] = { 0, 16, 20, 24 };
#ifdef CURSES_BASIC
	static int scanlines_to_curses_display = 0;
	static UWORD screenaddr = 0;
	static UWORD newscreenaddr = 0;
#endif

	int bytes = 0;
	if (--scanlines_to_dl <= 0) {
		if (DMACTL & 0x20) {
			IR = ANTIC_GetDLByte(&dlist);
			xpos++;
		}
		else
			IR &= 0x7f;	/* repeat last instruction, but don't generate DLI */
		switch (IR & 0xf) {
		case 0:
			scanlines_to_dl = ((IR >> 4) & 7) + 1;
			break;
		case 1:
			if (DMACTL & 0x20) {
				dlist = ANTIC_GetDLWord(&dlist);
				xpos += 2;
			}
			scanlines_to_dl = (IR & 0x40) ? 1024 /* no more DL in this frame */ : 1;
			break;
		default:
			if (IR & 0x40 && DMACTL & 0x20) {
#ifdef CURSES_BASIC
				screenaddr =
#endif
					ANTIC_GetDLWord(&dlist);
				xpos += 2;
			}
			/* can't steal cycles now, because DLI must come first */
			/* just an approximation: doesn't check HSCROL */
			switch (DMACTL & 3) {
			case 1:
				bytes = mode_bytes[IR & 0xf] * 8 / 10;
				break;
			case 2:
				bytes = mode_bytes[IR & 0xf];
				break;
			case 3:
				bytes = mode_bytes[IR & 0xf] * 12 / 10;
				break;
			default:
				break;
			}
#ifdef CURSES_BASIC
			/* Normally, we would call curses_display_line here,
			   and not use scanlines_to_curses_display at all.
			   That would however cause incorrect color of the "MEMORY"
			   menu item in Self Test - it isn't set properly
			   in the first scanline. We therefore postpone
			   curses_display_line call to the next scanline. */
			scanlines_to_curses_display = 2;
			newscreenaddr = screenaddr + bytes;
#endif
			/* just an approximation: doesn't check VSCROL */
			scanlines_to_dl = mode_scanlines[IR & 0xf];
			break;
		}
	}
	if (scanlines_to_dl == 1 && (IR & 0x80)) {
		GO(NMIST_C);
		NMIST = 0x9f;
		if (NMIEN & 0x80) {
			GO(NMI_C);
			NMI();
		}
	}
#ifdef CURSES_BASIC
	if (--scanlines_to_curses_display == 0) {
		curses_display_line(IR & 0xf, memory + screenaddr);
		/* 4k wrap */
		if (((screenaddr ^ newscreenaddr) & 0x1000) != 0)
			screenaddr = newscreenaddr - 0x1000;
		else
			screenaddr = newscreenaddr;
	}
#endif
	xpos += bytes;
	/* steal cycles in font modes */
	switch (IR & 0xf) {
	case 2:
	case 3:
	case 4:
	case 5:
		xpos += font40_cycles[DMACTL & 3];
		break;
	case 6:
	case 7:
		xpos += font20_cycles[DMACTL & 3];
		break;
	default:
		break;
	}
}

#define BASIC_LINE GO(LINE_C); xpos -= LINE_C - DMAR; screenline_cpu_clock += LINE_C; ypos++

static void basic_frame(void)
{
	/* scanlines 0 - 7 */
	ypos = 0;
	do {
		POKEY_Scanline();		/* check and generate IRQ */
		BASIC_LINE;
	} while (ypos < 8);

	scanlines_to_dl = 1;
	/* scanlines 8 - 247 */
	do {
		POKEY_Scanline();		/* check and generate IRQ */
		basic_antic_scanline();
		BASIC_LINE;
	} while (ypos < 248);

	/* scanline 248 */
	POKEY_Scanline();			/* check and generate IRQ */
	GO(NMIST_C);
	NMIST = 0x5f;				/* Set VBLANK */
	if (NMIEN & 0x40) {
		GO(NMI_C);
		NMI();
	}
	BASIC_LINE;

	/* scanlines 249 - 261(311) */
	do {
		POKEY_Scanline();		/* check and generate IRQ */
		BASIC_LINE;
	} while (ypos < max_ypos);
}

#endif /* defined(BASIC) || defined(VERY_SLOW) || defined(CURSES_BASIC) */

void Atari800_Frame(void)
{
#ifndef BASIC
	static int refresh_counter = 0;
	switch (key_code) {
	case AKEY_COLDSTART:
		Coldstart();
		break;
	case AKEY_WARMSTART:
		Warmstart();
		break;
	case AKEY_EXIT:
		Atari800_Exit(FALSE);
		exit(0);
	case AKEY_UI:
#ifdef SOUND
		Sound_Pause();
#endif
		ui();
#ifdef SOUND
		Sound_Continue();
#endif
		/* frametime = deltatime; XXX? */
		break;
#ifndef CURSES_BASIC
	case AKEY_SCREENSHOT:
		Screen_SaveNextScreenshot(FALSE);
		break;
	case AKEY_SCREENSHOT_INTERLACE:
		Screen_SaveNextScreenshot(TRUE);
		break;
#endif /* CURSES_BASIC */
	default:
		break;
	}
#endif /* BASIC */

	Device_Frame();
#ifndef BASIC
	INPUT_Frame();
#endif
	GTIA_Frame();
#ifdef SOUND
	Sound_Update();
#endif

#ifdef BASIC
	basic_frame();
#else /* BASIC */
	if (++refresh_counter >= refresh_rate) {
		refresh_counter = 0;
#ifdef USE_CURSES
		curses_clear_screen();
#endif
#ifdef CURSES_BASIC
		basic_frame();
#else
		ANTIC_Frame(TRUE);
		INPUT_DrawMousePointer();
		Screen_DrawAtariSpeed();
		Screen_DrawDiskLED();
#endif /* CURSES_BASIC */
#ifdef DONT_DISPLAY
		display_screen = FALSE;
#else
		display_screen = TRUE;
#endif /* DONT_DISPLAY */
	}
	else {
#if defined(VERY_SLOW) || defined(CURSES_BASIC)
		basic_frame();
#else
		ANTIC_Frame(sprite_collisions_in_skipped_frames);
#endif
		display_screen = FALSE;
	}
#endif /* BASIC */

	POKEY_Frame();
	nframes++;

#ifdef BENCHMARK
	if (nframes >= BENCHMARK) {
		double benchmark_time = Atari_time() - benchmark_start_time;
		Atari800_Exit(FALSE);
		printf("%d frames emulated in %.2f seconds\n", BENCHMARK, benchmark_time);
		exit(0);
	}
#else

#ifndef DONT_SYNC_WITH_HOST
#ifdef ALTERNATE_SYNC_WITH_HOST
	if (refresh_counter == 0)
#endif
		atari_sync();
#endif

#endif /* BENCHMARK */
}

#endif /* __PLUS */

#ifndef BASIC

void MainStateSave(void)
{
	UBYTE temp;
	int default_tv_mode;
	int os = 0;
	int default_system = 3;
	int pil_on = FALSE;

	/* Possibly some compilers would handle an enumerated type differently,
	   so convert these into unsigned bytes and save them out that way */
	if (tv_mode == TV_PAL) {
		temp = 0;
		default_tv_mode = 1;
	}
	else {
		temp = 1;
		default_tv_mode = 2;
	}
	SaveUBYTE(&temp, 1);

	switch (machine_type) {
	case MACHINE_OSA:
		temp = ram_size == 16 ? 5 : 0;
		os = 1;
		default_system = 1;
		break;
	case MACHINE_OSB:
		temp = ram_size == 16 ? 5 : 0;
		os = 2;
		default_system = 2;
		break;
	case MACHINE_XLXE:
		switch (ram_size) {
		case 16:
			temp = 6;
			default_system = 3;
			break;
		case 64:
			temp = 1;
			default_system = 3;
			break;
		case 128:
			temp = 2;
			default_system = 4;
			break;
		case RAM_320_RAMBO:
		case RAM_320_COMPY_SHOP:
			temp = 3;
			default_system = 5;
			break;
		case 576:
			temp = 7;
			default_system = 6;
			break;
		case 1088:
			temp = 8;
			default_system = 7;
			break;
		}
		break;
	case MACHINE_5200:
		temp = 4;
		default_system = 6;
		break;
	}
	SaveUBYTE(&temp, 1);

	SaveINT(&os, 1);
	SaveINT(&pil_on, 1);
	SaveINT(&default_tv_mode, 1);
	SaveINT(&default_system, 1);
}

void MainStateRead(void)
{
	/* these are all for compatibility with previous versions */
	UBYTE temp;
	int default_tv_mode;
	int os;
	int default_system;
	int pil_on;

	ReadUBYTE(&temp, 1);
	tv_mode = (temp == 0) ? TV_PAL : TV_NTSC;

	ReadUBYTE(&temp, 1);
	ReadINT(&os, 1);
	switch (temp) {
	case 0:
		machine_type = os == 1 ? MACHINE_OSA : MACHINE_OSB;
		ram_size = 48;
		break;
	case 1:
		machine_type = MACHINE_XLXE;
		ram_size = 64;
		break;
	case 2:
		machine_type = MACHINE_XLXE;
		ram_size = 128;
		break;
	case 3:
		machine_type = MACHINE_XLXE;
		ram_size = RAM_320_COMPY_SHOP;
		break;
	case 4:
		machine_type = MACHINE_5200;
		ram_size = 16;
		break;
	case 5:
		machine_type = os == 1 ? MACHINE_OSA : MACHINE_OSB;
		ram_size = 16;
		break;
	case 6:
		machine_type = MACHINE_XLXE;
		ram_size = 16;
		break;
	case 7:
		machine_type = MACHINE_XLXE;
		ram_size = 576;
		break;
	case 8:
		machine_type = MACHINE_XLXE;
		ram_size = 1088;
		break;
	default:
		machine_type = MACHINE_XLXE;
		ram_size = 64;
		Aprint("Warning: Bad machine type read in from state save, defaulting to 800 XL");
		break;
	}

	ReadINT(&pil_on, 1);
	ReadINT(&default_tv_mode, 1);
	ReadINT(&default_system, 1);
	load_roms();
	/* XXX: what about patches? */
}

#endif

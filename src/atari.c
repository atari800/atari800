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
#include "rt-config.h"
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
#include "sound.h"
#include "sndsave.h"
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

int verbose = FALSE;

int display_screen = FALSE;
int nframes = 0;
int sprite_collisions_in_skipped_frames = FALSE;

static double frametime = 0.1;
double deltatime;
double fps;
int percent_atari_speed = 100;

#ifdef BENCHMARK
static double benchmark_start_time;
#endif

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
#ifdef MONITOR_BREAK
	break_cim = 1;
#endif
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
			Aprint("Fatal Error in Atari800_PatchOS(): Unknown machine");
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
	PIA_Reset();
	ANTIC_Reset();
	/* CPU_Reset() must be after PIA_Reset(),
	   because Reset routine vector must be read from OS ROM */
	CPU_Reset();
	/* note: POKEY and GTIA have no Reset pin */
#ifdef __PLUS
	HandleResetEvent();
#endif
}

void Coldstart(void)
{
	Warmstart();
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
			fclose(fp):
			Aprint("\"%s\" is a compressed file.");
			Aprint("This executable does not support compressed files. You can uncompress this file");
			Aprint("with an external program that supports gzip (*.gz) files (e.g. gunzip)");
			Aprint("and then load into this emulator");
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
	switch (file_length) {
	case 4 * 1024:
	case 8 * 1024:
	case 16 * 1024:
	case 32 * 1024:
	case 40 * 1024:
	case 64 * 1024:
	case 128 * 1024:
	case 256 * 1024:
	case 512 * 1024:
	case 1024 * 1024:
		return AFILE_ROM;
	default:
		break;
	}
	/* BOOT_TAPE is a raw file containing a program booted from a tape */
	if ((header[1] << 7) == file_length)
		return AFILE_BOOT_TAPE;
	/* Normally: return (file_length % 128 == 0) ? AFILE_XFD : AFILE_ERROR; */
	/* but we support corrupted XFDs. */
	return AFILE_XFD;
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
	int config = FALSE;
	int help_only = FALSE;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-configure") == 0) {
			config = TRUE;
		}
		else if (strcmp(argv[i], "-config") == 0) {
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

	if (!RtConfigLoad(rtconfig_filename))
		config = TRUE;

	if (config) {
#ifndef DONT_USE_RTCONFIGUPDATE
		RtConfigUpdate();
#endif
		RtConfigSave();
	}

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
					Aprint("\t-configure       Update Configuration File");
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

#ifndef __PLUS
	if (tv_mode == TV_PAL)
		deltatime = (1.0 / 50.0);
	else
		deltatime = (1.0 / 60.0);
#endif /* __PLUS */

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

#if defined(__PLUS) && defined(MONITOR_BREAK)
	if (break_cim == 1)
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
#ifdef DJGPP
	/* DJGPP has gettimeofday, but it's not more accurate than uclock */
	return uclock() * (1.0 / UCLOCKS_PER_SEC);
#elif defined(WIN32)
	return GetTickCount() * 1e-3;
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
#ifdef DJGPP
		/* DJGPP has usleep and select, but they don't work that good */
		/* XXX: find out why */
		double curtime = Atari_time();
		while ((curtime + s) > Atari_time());
#elif defined(HAVE_USLEEP)
		usleep(s * 1e6);
#elif defined(__BEOS__)
		/* added by Walter Las for BeOS */
		snooze(s * 1e6);
#elif defined(__EMX__)
		/* added by Brian Smith for os/2 */
		DosSleep(s);
#elif defined(WIN32)
		Sleep((DWORD) (s * 1e3));
#elif defined(HAVE_SELECT)
		/* linux */
		struct timeval tp;
		tp.tv_sec = 0;
		tp.tv_usec = 1e6 * s;
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
	static double lasttime = 0, lastcurtime = 0;
	double curtime;

	if (deltatime > 0.0) {
		curtime = Atari_time();
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
		frametime = deltatime;
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

/*
$Log$
Revision 1.74  2005/09/06 22:48:36  pfusik
introduced util.[ch]

Revision 1.73  2005/09/03 11:29:31  pfusik
fixed the recently broken BASIC version

Revision 1.72  2005/08/31 19:55:33  pfusik
auto-starting any files supported by the emulator;
support for Atari800Win PLus

Revision 1.71  2005/08/27 10:32:15  pfusik
BENCHMARK

Revision 1.70  2005/08/24 21:04:41  pfusik
load state files from the command line using "-state"

Revision 1.69  2005/08/22 20:52:21  pfusik
stripped 2k more from emuos;
don't try to load BASIC ROM in 400/800 emuos mode

Revision 1.68  2005/08/21 15:35:25  pfusik
fixed loading of non-verbose state files;
correct highlighting of Self Test menu on CURSES_BASIC;
"--usage" and "--help" now work; Atari_tmpfile()

Revision 1.67  2005/08/18 23:27:37  pfusik
Screen_Initialise(); smaller emuos_h

Revision 1.66  2005/08/17 22:21:41  pfusik
auto-define USE_CLOCK as a last resort for Atari_time

Revision 1.65  2005/08/15 20:36:25  pfusik
allow no more than 8 disk images from the command line;
back to #ifdef WIN32, __BEOS__, __EMX__

Revision 1.64  2005/08/15 17:13:27  pfusik
Basic loader; VOL_ONLY_SOUND in BASIC and CURSES_BASIC

Revision 1.63  2005/08/13 08:42:33  pfusik
CURSES_BASIC; generate curses screen basing on the DL

Revision 1.62  2005/08/10 19:49:59  pfusik
greatly improved BASIC and VERY_SLOW

Revision 1.61  2005/08/07 13:45:18  pfusik
removed zlib_capable()

Revision 1.60  2005/08/06 18:14:07  pfusik
if no sleep-like function available, fall back to time polling

Revision 1.59  2005/08/04 22:51:33  pfusik
use best time functions available - now checked by Configure,
not hard-coded for platforms (almost...)

Revision 1.58  2005/03/10 04:43:32  pfusik
removed the unused "screen" parameter from ui() and SelectCartType()

Revision 1.57  2005/03/08 04:48:19  pfusik
tidied up a little

Revision 1.56  2005/03/05 12:28:24  pfusik
support for special AKEY_*, refresh rate control and atari_sync()
moved to Atari800_Frame()

Revision 1.55  2005/03/03 09:36:26  pfusik
moved screen-related variables to the new "screen" module

Revision 1.54  2005/02/23 16:45:30  pfusik
PNG screenshots

Revision 1.53  2003/12/16 18:30:34  pfusik
check OS before applying C: patches

Revision 1.52  2003/11/22 23:26:19  joy
cassette support improved

Revision 1.51  2003/10/25 18:40:54  joy
various little updates for better MacOSX support

Revision 1.50  2003/08/05 13:32:08  joy
more options documented

Revision 1.49  2003/08/05 13:22:56  joy
security fix

Revision 1.48  2003/05/28 19:54:58  joy
R: device support (networking?)

Revision 1.47  2003/03/07 11:38:40  pfusik
oops, didn't updated to 1.45 before commiting 1.46

Revision 1.46  2003/03/07 11:24:09  pfusik
Warmstart() and Coldstart() cleanup

Revision 1.45  2003/03/03 09:57:32  joy
Ed improved autoconf again plus fixed some little things

Revision 1.44  2003/02/24 09:32:32  joy
header cleanup

Revision 1.43  2003/02/10 11:22:32  joy
preparing for 1.3.0 release

Revision 1.42  2003/01/27 21:05:33  joy
little BeOS support

Revision 1.41  2002/11/05 23:01:21  joy
OS/2 compatibility

Revision 1.40  2002/08/07 07:59:24  joy
displaying version (-v) fixed

Revision 1.39  2002/08/07 07:23:02  joy
-help formatting fixed, crlf converted to lf

Revision 1.38  2002/07/14 13:25:24  pfusik
emulation of 576K and 1088K RAM machines

Revision 1.37  2002/07/04 12:41:21  pfusik
emulation of 16K RAM machines: 400 and 600XL

Revision 1.36  2002/06/23 21:48:58  joy
Atari800 does quit immediately when started with "-help" option.
"-palette" moved to colours.c

Revision 1.35  2002/06/20 20:59:37  joy
missing header file included

Revision 1.34  2002/03/30 06:19:28  vasyl
Dirty rectangle scheme implementation part 2.
All video memory accesses everywhere are going through the same macros
in ANTIC.C. UI_BASIC does not require special handling anymore. Two new
functions are exposed in ANTIC.H for writing to video memory.

Revision 1.32  2001/12/04 22:28:16  joy
the speed regulation when -DUSE_CLOCK is enabled so that key autorepeat in UI works.

Revision 1.31  2001/11/11 22:11:53  joy
wrong value for vertical position of disk led caused x11 port to crash badly.

Revision 1.30  2001/10/26 05:42:44  fox
made 130 XE state files compatible with previous versions

Revision 1.29  2001/10/05 16:46:45  fox
H: didn't worked until a patch was toggled

Revision 1.28  2001/10/05 10:20:24  fox
added Bounty Bob Strikes Back cartridge for 800/XL/XE

Revision 1.27  2001/10/03 16:49:24  fox
added screen_visible_* variables, Update_LED -> LED_Frame

Revision 1.26  2001/10/03 16:40:17  fox
rewritten escape codes handling

Revision 1.25  2001/10/01 17:22:16  fox
unused CRASH_MENU externs removed; Poke -> dPutByte;
memcpy(memory + ...) -> dCopyToMem

Revision 1.24  2001/09/27 22:36:39  fox
called INPUT_DrawMousePointer

Revision 1.23  2001/09/27 09:34:32  fox
called INPUT_Initialise

Revision 1.22  2001/09/21 17:09:05  fox
main() is now in platform-dependent code, should call Atari800_Initialise
and Atari800_Frame

Revision 1.21  2001/09/21 17:00:57  fox
part of keyboard handling moved to INPUT_Frame()

Revision 1.20  2001/09/21 16:54:56  fox
Atari800_Frame()

Revision 1.19  2001/09/17 19:30:27  fox
shortened state file of 130 XE, enable_c000_ram -> ram_size = 52

Revision 1.18  2001/09/17 18:09:40  fox
machine, mach_xlxe, Ram256, os, default_system -> machine_type, ram_size

Revision 1.17  2001/09/17 07:39:50  fox
Initialise_Atari... functions moved to atari.c

Revision 1.16  2001/09/16 11:22:56  fox
removed default_tv_mode

Revision 1.15  2001/09/09 08:34:13  fox
hold_option -> disable_basic

Revision 1.14  2001/08/16 23:24:25  fox
selecting cartridge type didn't worked in 5200 mode

Revision 1.13  2001/08/06 13:11:19  fox
hold_start support

Revision 1.12  2001/08/03 12:48:55  fox
cassette support

Revision 1.11  2001/07/25 12:58:25  fox
added SIO_Exit(), slight clean up

Revision 1.10  2001/07/20 20:14:14  fox
support for new rtime and cartridge modules

Revision 1.8  2001/04/15 09:14:33  knik
zlib_capable -> have_libz (autoconf compatibility)

Revision 1.7  2001/04/08 05:57:12  knik
sound calls update

Revision 1.6  2001/04/03 05:43:36  knik
reorganized sync code; new snailmeter

Revision 1.5  2001/03/18 06:34:58  knik
WIN32 conditionals removed

Revision 1.4  2001/03/18 06:24:04  knik
unused variable removed

*/

/*
 * atari.c - main high-level routines
 *
 * Copyright (c) 1995-1998 David Firth
 * Copyright (c) 1998-2003 Atari800 development team (see DOC/CREDITS)
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
#include <ctype.h>
#include <signal.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef __EMX__
#define INCL_DOS
#include <os2.h>
#endif

#ifdef __BEOS__
#include <OS.h>
#endif

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "antic.h"
#include "gtia.h"
#include "pia.h"
#include "pokey.h"
#include "cartridge.h"
#include "devices.h"
#include "sio.h"
#include "monitor.h"
#include "platform.h"
#include "prompts.h"
#include "rt-config.h"
#include "ui.h"
#include "ataripcx.h"		/* for Save_PCX() */
#include "log.h"
#include "statesav.h"
#include "diskled.h"
#include "colours.h"
#include "binload.h"
#include "rtime.h"
#include "cassette.h"
#include "input.h"
#ifdef SOUND
#include "sound.h"
#endif

ULONG *atari_screen = NULL;
#ifdef DIRTYRECT
UBYTE *screen_dirty = NULL;
#endif
#ifdef BITPL_SCR
ULONG *atari_screen_b = NULL;
ULONG *atari_screen1 = NULL;
ULONG *atari_screen2 = NULL;
#endif

/* The area that can been seen on screen is x1 <= x < x2, y1 <= y < y2.
   Full Atari screen is 336x240. ATARI_WIDTH is 384 only because
   the code in antic.c sometimes draws more than 336 bytes in a line.
   Currently screen_visible variables are used only to place
   disk led and snailmeter in corners of screen, but they might be
   also used for optimisations in antic.c.
*/
int screen_visible_x1 = 24;				/* 0 .. ATARI_WIDTH */
int screen_visible_y1 = 0;				/* 0 .. ATARI_HEIGHT */
int screen_visible_x2 = 360;			/* 0 .. ATARI_WIDTH */
int screen_visible_y2 = ATARI_HEIGHT;	/* 0 .. ATARI_HEIGHT */

int machine_type = MACHINE_OSB;
int ram_size = 48;
int tv_mode = TV_PAL;

int verbose = FALSE;
double fps;
int nframes = 0;
static double frametime = 0.1;	/* measure time between two Antic runs */
static int emu_too_fast = 0;

int emuos_mode = 1;	/* 0 = never use EmuOS, 1 = use EmuOS if real OS not available, 2 = always use EmuOS */
int pil_on = FALSE;

double deltatime;

static char *rom_filename = NULL;

void sigint_handler(int num)
{
	int restart;

	restart = Atari800_Exit(TRUE);
	if (restart) {
		signal(SIGINT, sigint_handler);
		return;
	}

	exit(0);
}

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
	ui((UBYTE *) atari_screen);
#else
	Aprint("Invalid ESC Code %x at Address %x", esc_code, regPC - 2);
	if (!Atari800_Exit(TRUE))
		exit(0);
#endif
}

void Atari800_PatchOS(void)
{
	int patched = Device_PatchOS();
	if (enable_sio_patch) {
                UWORD addr_l,addr_s;
                //patch Open() of C: so we know when a leader is processed
                switch (machine_type) {
                case MACHINE_OSA:
                case MACHINE_OSB:
                        addr_l = 0xef74;
                        addr_s = 0xefbc;
                        break;
                case MACHINE_XLXE:
                        addr_l = 0xfd13;
                        addr_s = 0xfd60;
                        break;
                default:
                        Aprint("Fatal Error in Atari800_PatchOS(): Unknown machine");
                        return;
                }
                Atari800_AddEsc(addr_l, ESC_COPENLOAD, CASSETTE_LeaderLoad);
                Atari800_AddEsc(addr_s, ESC_COPENSAVE, CASSETTE_LeaderSave);

		Atari800_AddEscRts(0xe459, ESC_SIOV, SIO);
		patched = TRUE;
	}
	else
	{
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
	if (disable_basic) {
		/* hold Option during reboot */
		consol_table[2] &= ~4;
	}
	if (hold_start) {
		/* hold Start during reboot */
		consol_table[2] &= ~1;
	}
	consol_table[1] = consol_table[2];
}

static int load_image(char *filename, UBYTE *buffer, int nbytes)
{
	int status = FALSE;
	FILE *f;

	f = fopen(filename, "rb");
	if (f) {
		status = fread(buffer, 1, nbytes, f);
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

#include "emuos.h"

int Atari800_InitialiseMachine(void)
{
	Atari800_ClearAllEsc();
	switch (machine_type) {
	case MACHINE_OSA:
		if (emuos_mode == 2)
			memcpy(atari_os, emuos_h + 0x1800, 0x2800);
		else {
			if (!load_image(atari_osa_filename, atari_os, 0x2800)) {
				if (emuos_mode == 1)
					memcpy(atari_os, emuos_h + 0x1800, 0x2800);
				else
					return FALSE;
			}
		}
		have_basic = load_image(atari_basic_filename, atari_basic, 0x2000);
		break;
	case MACHINE_OSB:
		if (emuos_mode == 2)
			memcpy(atari_os, emuos_h + 0x1800, 0x2800);
		else {
			if (!load_image(atari_osb_filename, atari_os, 0x2800)) {
				if (emuos_mode == 1)
					memcpy(atari_os, emuos_h + 0x1800, 0x2800);
				else
					return FALSE;
			}
		}
		have_basic = load_image(atari_basic_filename, atari_basic, 0x2000);
		break;
	case MACHINE_XLXE:
		if (emuos_mode == 2)
			memcpy(atari_os, emuos_h, 0x4000);
		else {
			if (!load_image(atari_xlxe_filename, atari_os, 0x4000)) {
				if (emuos_mode == 1)
					memcpy(atari_os, emuos_h, 0x4000);
				else
					return FALSE;
			}
			else if (!load_image(atari_basic_filename, atari_basic, 0x2000))
				return FALSE;
		}
		xe_bank = 0;
		break;
	case MACHINE_5200:
		if (!load_image(atari_5200_filename, atari_os, 0x800))
			return FALSE;
		break;
	}
	MEMORY_InitialiseMachine();
	Device_UpdatePatches();
	return TRUE;
}

char *safe_strncpy(char *dest, const char *src, size_t size)
{
	strncpy(dest, src, size);
	dest[size-1] = '\0';
	return dest;
}

int Atari800_Initialise(int *argc, char *argv[])
{
	int diskno = 1;
	int i, j;
	char *run_direct=NULL;
	char *rtconfig_filename = NULL;
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
			printf("%s: unrecognized option `%s'\n", argv[0], argv[1]);
			printf("Try `%s -help' for more information.\n", argv[0]);
			return FALSE;
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
#endif /* !DONT_USE_RTCONFIGUPDATE */

		RtConfigSave();
	}

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
			int i_a = ( (i+1) < *argc );	/* is argument available? */
			int a_m = FALSE;				/* error, argument missing! */

			if (strcmp(argv[i], "-osa_rom") == 0) {
				if (i_a) safe_strncpy(atari_osa_filename, argv[++i], sizeof(atari_osa_filename)); else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-osb_rom") == 0) {
				if (i_a) safe_strncpy(atari_osb_filename, argv[++i], sizeof(atari_osb_filename)); else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-xlxe_rom") == 0) {
				if (i_a) safe_strncpy(atari_xlxe_filename, argv[++i], sizeof(atari_xlxe_filename)); else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-5200_rom") == 0) {
				if (i_a) safe_strncpy(atari_5200_filename, argv[++i], sizeof(atari_5200_filename)); else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-basic_rom") == 0) {
				if (i_a) safe_strncpy(atari_basic_filename, argv[++i], sizeof(atari_basic_filename)); else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-cart") == 0) {
				if (i_a) rom_filename = argv[++i]; else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-run") == 0) {
				if (i_a) run_direct = argv[++i]; else a_m = TRUE;
			}
			else if (strcmp(argv[i], "-refresh") == 0) {
				if (i_a) {
					sscanf(argv[++i], "%d", &refresh_rate);
					if (refresh_rate < 1)
						refresh_rate = 1;
				}
				else
					a_m = TRUE;
			}

			else {
				/* all options known to main module tried but none matched */

				if (strcmp(argv[i], "-help") == 0) {
					help_only = TRUE;
					Aprint("\t-configure       Update Configuration File");
					Aprint("\t-config <file>   Specify Alternate Configuration File");
					Aprint("\t-atari           Standard Atari 800 mode");
					Aprint("\t-xl              Atari 800XL mode");
					Aprint("\t-xe              Atari 130XE mode");
					Aprint("\t-320xe           Atari 320XE mode (COMPY SHOP)");
					Aprint("\t-rambo           Atari 320XE mode (RAMBO)");
					Aprint("\t-nobasic         Turn off Atari BASIC ROM");
					Aprint("\t-basic           Turn on Atari BASIC ROM");
					Aprint("\t-5200            Atari 5200 Games System");
					Aprint("\t-pal             Enable PAL TV mode");
					Aprint("\t-ntsc            Enable NTSC TV mode");
					Aprint("\t-osa_rom <file>  Load OS A ROM from file");
					Aprint("\t-osb_rom <file>  Load OS B ROM from file");
					Aprint("\t-xlxe_rom <file> Load XL/XE ROM from file");
					Aprint("\t-5200_rom <file> Load 5200 ROM from file");
					Aprint("\t-basic_rom <fil> Load BASIC ROM from file");
					Aprint("\t-cart <file>     Install cartridge (raw or CART format)");
					Aprint("\t-run <file>      Run file directly");
					Aprint("\t-refresh <rate>  Specify screen refresh rate");
					Aprint("\t-nopatch         Don't patch SIO routine in OS");
					Aprint("\t-nopatchall      Don't patch OS at all, H: device won't work");
					Aprint("\t-a               Use OS A");
					Aprint("\t-b               Use OS B");
					Aprint("\t-c               Enable RAM between 0xc000 and 0xd000");
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

	if (tv_mode == TV_PAL)
	{
		deltatime = (1.0 / 50.0);
	}
	else
	{
		deltatime = (1.0 / 60.0);
	}

	Palette_Initialise(argc, argv);
	Device_Initialise(argc, argv);
	RTIME_Initialise(argc, argv);
	SIO_Initialise (argc, argv);
	CASSETTE_Initialise(argc, argv);
	INPUT_Initialise(argc, argv);
	Atari_Initialise(argc, argv);	/* Platform Specific Initialisation */

	if (!atari_screen) {
		atari_screen = (ULONG *) malloc(ATARI_HEIGHT * ATARI_WIDTH);
#ifdef DIRTYRECT
		screen_dirty = (UBYTE *) malloc(ATARI_HEIGHT * ATARI_WIDTH/8);
		entire_screen_dirty();
#endif
#ifdef BITPL_SCR
		atari_screen_b = (ULONG *) malloc(ATARI_HEIGHT * ATARI_WIDTH);
		atari_screen1 = atari_screen;
		atari_screen2 = atari_screen_b;
#endif
	}
	/*
	 * Initialise Custom Chips
	 */

	ANTIC_Initialise(argc, argv);
	GTIA_Initialise(argc, argv);
	PIA_Initialise(argc, argv);
	POKEY_Initialise(argc, argv);

	if (help_only) {
		Atari800_Exit(FALSE);
		return FALSE;
	}

	/*
	 * Any parameters left on the command line must be disk images.
	 */

	for (i = 1; i < *argc; i++) {
		if (!SIO_Mount(diskno++, argv[i], FALSE)) {
			Aprint("Disk File %s not found", argv[i]);
		}
	}

	/*
	 * Install CTRL-C Handler
	 */
	signal(SIGINT, sigint_handler);
	/*
	 * Configure Atari System
	 */

	Atari800_InitialiseMachine();

/*
 * ================================
 * Install requested ROM cartridges
 * ================================
 */
	if (rom_filename) {
		int r = CART_Insert(rom_filename);
		if (r < 0) {
			Aprint("Error inserting cartridge %s: %s", rom_filename,
			r == CART_CANT_OPEN ? "Can't open file" :
			r == CART_BAD_FORMAT ? "Bad format" :
			r == CART_BAD_CHECKSUM ? "Bad checksum" :
			"Unknown error");
		}
		if (r > 0) {
			ui_is_active = TRUE;
			cart_type = SelectCartType((UBYTE *) atari_screen, r);
			ui_is_active = FALSE;
			CART_Start();
		}
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
	}
/*
 * ======================================
 * Reset CPU and start hardware emulation
 * ======================================
 */

	if (run_direct != NULL)
		BIN_loader(run_direct);

	return TRUE;
}

int Atari800_Exit(int run_monitor)
{
	int restart;
	if (verbose) {
		Aprint("Current Frames per Secound = %f", fps);
	}
	restart = Atari_Exit(run_monitor);
	if (!restart)
		SIO_Exit();	/* umount disks, so temporary files are deleted */
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
	case 0xd500:				/* RTIME-8 */
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
	case 0xd500:				/* Super Cartridges */
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
	}
}

#ifdef SNAILMETER
static void ShowRealSpeed(ULONG * atari_screen)
{
#define MAXSPEED	200
  UBYTE *ptr;
  int speed = (int) (100.0 * deltatime / frametime + 0.5);

  if (speed > MAXSPEED)
    speed = MAXSPEED;

  ptr = (UBYTE *) atari_screen + screen_visible_x1 + ATARI_WIDTH * (screen_visible_y2 - 1);
  video_memset(ptr, 0xc8, speed);
  video_memset(ptr+speed, 0x02, MAXSPEED-speed);
  video_putbyte(ptr+100, 0x38);
}
#endif

static double Atari_time(void)
{
#ifdef WIN32
  return GetTickCount() * 1e-3;
#elif defined(DJGPP)
  return uclock() * (1.0 / UCLOCKS_PER_SEC);
#else
  struct timeval tp;

  gettimeofday(&tp, NULL);
  return tp.tv_sec + 1e-6 * tp.tv_usec;
#endif
}

static void Atari_sleep(double s)
{
  emu_too_fast = 0;
  if (s > 0)
  {
#ifdef linux
    struct timeval tp;

    tp.tv_sec = 0;
    tp.tv_usec = 1e6 * s;
    select(1,NULL,NULL,NULL,&tp);

#elif defined(WIN32)
    Sleep(s * 1e3);
    
#elif defined(DJGPP)
    double curtime = Atari_time();
    while ((curtime + s) > Atari_time());

#elif defined __EMX__
    DosSleep(s); /* added by Brian Smith for os/2 */

#elif defined __BEOS__
    snooze(s * 1e6); /* added by Walter Las for BeOS */

#else
    usleep(s * 1e6);
#endif
    emu_too_fast = 1;
  }
}

void atari_sync(void)
{
#ifdef USE_CLOCK
	static ULONG nextclock = 1;	/* put here a non-zero value to enable speed regulator */
	/* on Atari Falcon CLK_TCK = 200 (i.e. 5 ms granularity) */
	/* on DOS (DJGPP) CLK_TCK = 91 (not too precise, but should work anyway)*/
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

	if (deltatime > 0.0)
	{
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
#endif /* USE_CLOCK */
}

void Atari800_Frame(int mode)
{
	Device_Frame();
	INPUT_Frame();
	GTIA_Frame();

#ifdef SOUND
	Sound_Update();
#endif

	switch (mode) {
	case EMULATE_BASIC:
		for (ypos = 0; ypos < max_ypos; ypos++) {
			GO(LINE_C);
			xpos -= LINE_C - DMAR;
		}
		break;
	case EMULATE_NO_SCREEN:
		ANTIC_Frame(FALSE);
		break;
	case EMULATE_FULL:
		ANTIC_Frame(TRUE);
#ifdef SHOW_DISK_LED
		LED_Frame();
#endif
#ifdef SNAILMETER
		if (!emu_too_fast)
			ShowRealSpeed(atari_screen);
#endif
		INPUT_DrawMousePointer();
		break;
	}
	POKEY_Frame();
	nframes++;
}

int zlib_capable(void)
{
#ifdef HAVE_LIBZ
	return TRUE;
#else
	return FALSE;
#endif
}

int prepend_tmpfile_path(char *buffer)
{
	if (buffer)
		*buffer = 0;
	return 0;
}

int ReadDisabledROMs(void)
{
	return FALSE;
}

void MainStateSave( void )
{
	UBYTE	temp;
	int default_tv_mode;	/* for compatibility with previous versions */
	int os = 0;
	int default_system = 3;

	/* Possibly some compilers would handle an enumerated type differently,
	   so convert these into unsigned bytes and save them out that way */
	if( tv_mode == TV_PAL ) {
		temp = 0;
		default_tv_mode = 1;
	}
	else {
		temp = 1;
		default_tv_mode = 2;
	}
	SaveUBYTE( &temp, 1 );

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
	SaveUBYTE( &temp, 1 );

	SaveINT( &os, 1 );
	SaveINT( &pil_on, 1 );
	SaveINT( &default_tv_mode, 1 );
	SaveINT( &default_system, 1 );
}

void MainStateRead( void )
{
	UBYTE	temp;
	int default_tv_mode;	/* for compatibility with previous versions */
	int os;
	int default_system;

	ReadUBYTE( &temp, 1 );
	if( temp == 0 )
		tv_mode = TV_PAL;
	else
		tv_mode = TV_NTSC;

	ReadUBYTE( &temp, 1 );
	ReadINT( &os, 1 );
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
		Aprint( "Warning: Bad machine type read in from state save, defaulting to 800 XL" );
		break;
	}

	ReadINT( &pil_on, 1 );
	ReadINT( &default_tv_mode, 1 );
	ReadINT( &default_system, 1 );
}

/*
$Log$
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

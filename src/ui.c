/*
 * ui.c - main user interface
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2010 Atari800 development team (see DOC/CREDITS)
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
#include <string.h>
#include <stdlib.h> /* free() */

#include "afile.h"
#include "antic.h"
#include "atari.h"
#include "binload.h"
#include "cartridge.h"
#include "cassette.h"
#if SUPPORTS_PLATFORM_PALETTEUPDATE
#include "colours.h"
#include "colours_external.h"
#include "colours_ntsc.h"
#endif
#include "compfile.h"
#include "cfg.h"
#include "cpu.h"
#include "devices.h" /* Devices_SetPrintCommand */
#include "esc.h"
#if NTSC_FILTER
#include "filter_ntsc.h"
#endif
#include "gtia.h"
#include "input.h"
#include "akey.h"
#include "log.h"
#include "memory.h"
#include "platform.h"
#include "rtime.h"
#include "screen.h"
#include "sio.h"
#include "statesav.h"
#include "ui.h"
#include "ui_basic.h"
#include "util.h"
#ifdef SOUND
#include "pokeysnd.h"
#include "sndsave.h"
#include "sound.h"
#endif


#ifdef DIRECTX
#include "win32\main.h"
#include "win32\joystick.h"
#include "win32\screen_win32.h"

/* Display Settings */
extern RENDERMODE rendermode;
extern FRAMEPARAMS frameparams;
extern DISPLAYMODE displaymode;
extern FSRESOLUTION fsresolution;
extern SCREENMODE screenmode;
extern ASPECTMODE scalingmethod;
extern ASPECTRATIO aspectmode;
extern CROP crop;
extern OFFSET offset;
extern BOOL usecustomfsresolution;
extern BOOL hidecursor;
extern BOOL lockaspect;
extern BOOL showmenu;
extern int windowscale;
extern int fullscreenWidth;
extern int fullscreenHeight;
extern int origScreenWidth;
extern int origScreenHeight;
extern int origScreenDepth;

/* Controller Settings */
extern BOOL mapController1Buttons;
extern BOOL mapController2Buttons;

/* local variables */
static char desktopreslabel[30];
static char hcrop_label[4];
static char vcrop_label[4];
static char hshift_label[4];
static char vshift_label[4];
static char monitor_label[40];
static char native_width_label[10];
static char native_height_label[20];
#endif

#ifdef _WIN32_WCE
extern int smooth_filter;
extern int filter_available;
extern int virtual_joystick;
extern void AboutPocketAtari(void);
#endif

#ifdef DREAMCAST
extern int db_mode;
extern int screen_tv_mode;
extern int emulate_paddles;
extern int glob_snd_ena;
extern void ButtonConfiguration(void);
extern void AboutAtariDC(void);
extern void update_vidmode(void);
extern void update_screen_updater(void);
#ifdef HZ_TEST
extern void do_hz_test(void);
#endif
#endif

#if SUPPORTS_PLATFORM_PALETTEUPDATE
COLOURS_VIDEO_PROFILE calibration_profile;
#endif

UI_tDriver *UI_driver = &UI_BASIC_driver;

int UI_is_active = FALSE;
int UI_alt_function = -1;
int UI_current_function = -1;

#ifdef CRASH_MENU
int UI_crash_code = -1;
UWORD UI_crash_address;
UWORD UI_crash_afterCIM;
int CrashMenu(void);
#endif

char UI_atari_files_dir[UI_MAX_DIRECTORIES][FILENAME_MAX];
char UI_saved_files_dir[UI_MAX_DIRECTORIES][FILENAME_MAX];
int UI_n_atari_files_dir = 0;
int UI_n_saved_files_dir = 0;
#if defined(XEP80_EMULATION) || defined(AF80) || defined(PBI_PROTO80)
static int saved_show_80;
#endif

static UI_tMenuItem *FindMenuItem(UI_tMenuItem *mip, int option)
{
	while (mip->retval != option)
		mip++;
	return mip;
}

static void SetItemChecked(UI_tMenuItem *mip, int option, int checked)
{
	FindMenuItem(mip, option)->flags = checked ? (UI_ITEM_CHECK | UI_ITEM_CHECKED) : UI_ITEM_CHECK;
}

static void FilenameMessage(const char *format, const char *filename)
{
	char msg[FILENAME_MAX + 30];
	sprintf(msg, format, filename);
	UI_driver->fMessage(msg, 1);
}

static const char * const cant_load = "Can't load \"%s\"";
static const char * const cant_save = "Can't save \"%s\"";
static const char * const created = "Created \"%s\"";

#define CantLoad(filename) FilenameMessage(cant_load, filename)
#define CantSave(filename) FilenameMessage(cant_save, filename)
#define Created(filename) FilenameMessage(created, filename)

static void SelectSystem(void)
{
	typedef struct {
		int type;
		int ram;
	} tSysConfig;

	static UI_tMenuItem menu_array[] = {
		UI_MENU_ACTION(0, "Atari OS/A (16 KB)"),
		UI_MENU_ACTION(1, "Atari OS/A (48 KB)"),
		UI_MENU_ACTION(2, "Atari OS/A (52 KB)"),
		UI_MENU_ACTION(3, "Atari OS/B (16 KB)"),
		UI_MENU_ACTION(4, "Atari OS/B (48 KB)"),
		UI_MENU_ACTION(5, "Atari OS/B (52 KB)"),
		UI_MENU_ACTION(6, "Atari 600XL (16 KB)"),
		UI_MENU_ACTION(7, "Atari 800XL (64 KB)"),
		UI_MENU_ACTION(8, "Atari 130XE (128 KB)"),
		UI_MENU_ACTION(9, "Atari XL/XE (192 KB)"),
		UI_MENU_ACTION(10, "Atari XL/XE (320 KB RAMBO)"),
		UI_MENU_ACTION(11, "Atari XL/XE (320 KB COMPY SHOP)"),
		UI_MENU_ACTION(12, "Atari XL/XE (576 KB)"),
		UI_MENU_ACTION(13, "Atari XL/XE (1088 KB)"),
		UI_MENU_ACTION(14, "Atari 5200 (16 KB)"),
		UI_MENU_ACTION(15, "Video system:"),
		UI_MENU_END
	};

	static const tSysConfig machine[] = {
		{ Atari800_MACHINE_OSA, 16 },
		{ Atari800_MACHINE_OSA, 48 },
		{ Atari800_MACHINE_OSA, 52 },
		{ Atari800_MACHINE_OSB, 16 },
		{ Atari800_MACHINE_OSB, 48 },
		{ Atari800_MACHINE_OSB, 52 },
		{ Atari800_MACHINE_XLXE, 16 },
		{ Atari800_MACHINE_XLXE, 64 },
		{ Atari800_MACHINE_XLXE, 128 },
		{ Atari800_MACHINE_XLXE, 192 },
		{ Atari800_MACHINE_XLXE, MEMORY_RAM_320_RAMBO },
		{ Atari800_MACHINE_XLXE, MEMORY_RAM_320_COMPY_SHOP },
		{ Atari800_MACHINE_XLXE, 576 },
		{ Atari800_MACHINE_XLXE, 1088 },
		{ Atari800_MACHINE_5200, 16 }
	};

#define N_MACHINES  ((int) (sizeof(machine) / sizeof(machine[0])))

	int option = 0;
	int new_tv_mode = Atari800_tv_mode;

	int i;
	for (i = 0; i < N_MACHINES; i++)
		if (Atari800_machine_type == machine[i].type && MEMORY_ram_size == machine[i].ram) {
			option = i;
			break;
		}

	for (;;) {
		menu_array[N_MACHINES].suffix = (new_tv_mode == Atari800_TV_PAL) ? "PAL" : "NTSC";
		option = UI_driver->fSelect("Select System", 0, option, menu_array, NULL);
		if (option < N_MACHINES)
			break;
		new_tv_mode = (new_tv_mode == Atari800_TV_PAL) ? Atari800_TV_NTSC : Atari800_TV_PAL;
	}
	if (option >= 0) {
		Atari800_machine_type = machine[option].type;
		MEMORY_ram_size = machine[option].ram;
		Atari800_InitialiseMachine();
	}
	else if (new_tv_mode != Atari800_tv_mode) {
		Atari800_SetTVMode(new_tv_mode);
		Atari800_InitialiseMachine(); /* XXX: Atari800_Coldstart() is probably enough */
	}
}

/* Inspired by LNG (lng.sourceforge.net) */
/* Writes a blank ATR. The ATR must by formatted by an Atari DOS
   before files are written to it. */
static void MakeBlankDisk(FILE *setFile)
{
/* 720, so it's a standard Single Density disk,
   which can be formatted by 2.x DOSes.
   It will be resized when formatted in another density. */
#define BLANK_DISK_SECTORS  720
#define BLANK_DISK_PARAS    (BLANK_DISK_SECTORS * 128 / 16)
	int i;
	struct AFILE_ATR_Header hdr;
	UBYTE sector[128];

	memset(&hdr, 0, sizeof(hdr));
	hdr.magic1 = 0x96;
	hdr.magic2 = 0x02;
	hdr.seccountlo = (UBYTE) BLANK_DISK_PARAS;
	hdr.seccounthi = (UBYTE) (BLANK_DISK_PARAS >> 8);
	hdr.hiseccountlo = (UBYTE) (BLANK_DISK_PARAS >> 16);
	hdr.secsizelo = 128;
	fwrite(&hdr, 1, sizeof(hdr), setFile);

	memset(sector, 0, sizeof(sector));
	for (i = 1; i <= BLANK_DISK_SECTORS; i++)
		fwrite(sector, 1, sizeof(sector), setFile);
}

static void DiskManagement(void)
{
	static char drive_array[8][5] = { " D1:", " D2:", " D3:", " D4:", " D5:", " D6:", " D7:", " D8:" };

	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL_PREFIX_TIP(0, drive_array[0], SIO_filename[0], NULL),
		UI_MENU_FILESEL_PREFIX_TIP(1, drive_array[1], SIO_filename[1], NULL),
		UI_MENU_FILESEL_PREFIX_TIP(2, drive_array[2], SIO_filename[2], NULL),
		UI_MENU_FILESEL_PREFIX_TIP(3, drive_array[3], SIO_filename[3], NULL),
		UI_MENU_FILESEL_PREFIX_TIP(4, drive_array[4], SIO_filename[4], NULL),
		UI_MENU_FILESEL_PREFIX_TIP(5, drive_array[5], SIO_filename[5], NULL),
		UI_MENU_FILESEL_PREFIX_TIP(6, drive_array[6], SIO_filename[6], NULL),
		UI_MENU_FILESEL_PREFIX_TIP(7, drive_array[7], SIO_filename[7], NULL),
		UI_MENU_FILESEL(8, "Save Disk Set"),
		UI_MENU_FILESEL(9, "Load Disk Set"),
		UI_MENU_ACTION(10, "Rotate Disks"),
		UI_MENU_FILESEL(11, "Make Blank ATR Disk"),
		UI_MENU_FILESEL_TIP(12, "Uncompress Disk Image", "Convert GZ or DCM to ATR"),
		UI_MENU_END
	};

	int dsknum = 0;

	for (;;) {
		static char disk_filename[FILENAME_MAX] = "";
		static char set_filename[FILENAME_MAX] = "";
		int i;
		int seltype;

		for (i = 0; i < 8; i++) {
			drive_array[i][0] = ' ';
			switch (SIO_drive_status[i]) {
			case SIO_OFF:
				menu_array[i].suffix = "Return:insert";
				break;
			case SIO_NO_DISK:
				menu_array[i].suffix = "Return:insert Backspace:off";
				break;
			case SIO_READ_ONLY:
				drive_array[i][0] = '*';
				/* FALLTHROUGH */
			default:
				menu_array[i].suffix = "Ret:insert Bksp:eject Space:read-only";
				break;
			}
		}

		dsknum = UI_driver->fSelect("Disk Management", 0, dsknum, menu_array, &seltype);

		switch (dsknum) {
		case 8:
			if (UI_driver->fGetSaveFilename(set_filename, UI_saved_files_dir, UI_n_saved_files_dir)) {
				FILE *fp = fopen(set_filename, "w");
				if (fp == NULL) {
					CantSave(set_filename);
					break;
				}
				for (i = 0; i < 8; i++)
					fprintf(fp, "%s\n", SIO_filename[i]);
				fclose(fp);
				Created(set_filename);
			}
			break;
		case 9:
			if (UI_driver->fGetLoadFilename(set_filename, UI_saved_files_dir, UI_n_saved_files_dir)) {
				FILE *fp = fopen(set_filename, "r");
				if (fp == NULL) {
					CantLoad(set_filename);
					break;
				}
				for (i = 0; i < 8; i++) {
					char filename[FILENAME_MAX];
					fgets(filename, FILENAME_MAX, fp);
					Util_chomp(filename);
					if (strcmp(filename, "Empty") != 0 && strcmp(filename, "Off") != 0)
						SIO_Mount(i + 1, filename, FALSE);
				}
				fclose(fp);
			}
			break;
		case 10:
			SIO_RotateDisks();
			break;
		case 11:
			if (UI_driver->fGetSaveFilename(disk_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
				FILE *fp = fopen(disk_filename, "wb");
				if (fp == NULL) {
					CantSave(disk_filename);
					break;
				}
				MakeBlankDisk(fp);
				fclose(fp);
				Created(disk_filename);
			}
			break;
		case 12:
			if (UI_driver->fGetLoadFilename(disk_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
				char uncompr_filename[FILENAME_MAX];
				FILE *fp = fopen(disk_filename, "rb");
				const char *p;
				if (fp == NULL) {
					CantLoad(disk_filename);
					break;
				}
				/* propose an output filename to make user's life easier */
				p = strrchr(disk_filename, '.');
				if (p != NULL) {
					char *q;
					p++;
					q = uncompr_filename + (p - disk_filename);
					if (Util_stricmp(p, "atz") == 0) {
						/* change last 'z' to 'r', preserving case */
						p += 2;
						q[2] = p[0] == 'z' ? 'r' : 'R';
						q[3] = '\0';
					}
					else if (Util_stricmp(p, "xfz") == 0) {
						/* change last 'z' to 'd', preserving case */
						p += 2;
						q[2] = p[0] == 'z' ? 'd' : 'D';
						q[3] = '\0';
					}
					else if (Util_stricmp(p, "gz") == 0) {
						/* strip ".gz" */
						p--;
						q[-1] = '\0';
					}
					else if (Util_stricmp(p, "atr") == 0) {
						/* ".atr" ? Probably won't work, anyway cut the extension but leave the dot */
						q[0] = '\0';
					}
					else {
						/* replace extension with "atr", preserving case */
						strcpy(q, p[0] <= 'Z' ? "ATR" : "atr");
					}
					memcpy(uncompr_filename, disk_filename, p - disk_filename);
				}
				else
					/* was no extension -> propose no filename */
					uncompr_filename[0] = '\0';
				/* recognize file type and uncompress */
				switch (fgetc(fp)) {
				case 0x1f:
					fclose(fp);
					if (UI_driver->fGetSaveFilename(uncompr_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
						FILE *fp2 = fopen(uncompr_filename, "wb");
						int success;
						if (fp2 == NULL) {
							CantSave(uncompr_filename);
							continue;
						}
						success = CompFile_ExtractGZ(disk_filename, fp2);
						fclose(fp2);
						UI_driver->fMessage(success ? "Conversion successful" : "Cannot convert this file", 1);
					}
					break;
				case 0xf9:
				case 0xfa:
					if (UI_driver->fGetSaveFilename(uncompr_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
						FILE *fp2 = fopen(uncompr_filename, "wb");
						int success;
						if (fp2 == NULL) {
							fclose(fp);
							CantSave(uncompr_filename);
							continue;
						}
						Util_rewind(fp);
						success = CompFile_DCMtoATR(fp, fp2);
						fclose(fp2);
						fclose(fp);
						UI_driver->fMessage(success ? "Conversion successful" : "Cannot convert this file", 1);
					}
					break;
				default:
					fclose(fp);
					UI_driver->fMessage("This is not a compressed disk image", 1);
					break;
				}
			}
			break;
		default:
			if (dsknum < 0)
				return;
			/* dsknum = 0..7 */
			switch (seltype) {
			case UI_USER_SELECT: /* "Enter" */
				if (SIO_drive_status[dsknum] != SIO_OFF && SIO_drive_status[dsknum] != SIO_NO_DISK)
					strcpy(disk_filename, SIO_filename[dsknum]);
				if (UI_driver->fGetLoadFilename(disk_filename, UI_atari_files_dir, UI_n_atari_files_dir))
					if (!SIO_Mount(dsknum + 1, disk_filename, FALSE))
						CantLoad(disk_filename);
				break;
			case UI_USER_TOGGLE: /* "Space" */
				i = FALSE;
				switch (SIO_drive_status[dsknum]) {
				case SIO_READ_WRITE:
					i = TRUE;
					/* FALLTHROUGH */
				case SIO_READ_ONLY:
					strcpy(disk_filename, SIO_filename[dsknum]);
					SIO_Mount(dsknum + 1, disk_filename, i);
					break;
				default:
					break;
				}
				break;
			default: /* Backspace */
				switch (SIO_drive_status[dsknum]) {
				case SIO_OFF:
					break;
				case SIO_NO_DISK:
					SIO_DisableDrive(dsknum + 1);
					break;
				default:
					SIO_Dismount(dsknum + 1);
					break;
				}
			}
			break;
		}
	}
}

int UI_SelectCartType(int k)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_ACTION(1, "Standard 8 KB cartridge"),
		UI_MENU_ACTION(2, "Standard 16 KB cartridge"),
		UI_MENU_ACTION(3, "OSS '034M' 16 KB cartridge"),
		UI_MENU_ACTION(4, "Standard 32 KB 5200 cartridge"),
		UI_MENU_ACTION(5, "DB 32 KB cartridge"),
		UI_MENU_ACTION(6, "Two chip 16 KB 5200 cartridge"),
		UI_MENU_ACTION(7, "Bounty Bob 40 KB 5200 cartridge"),
		UI_MENU_ACTION(8, "64 KB Williams cartridge"),
		UI_MENU_ACTION(9, "Express 64 KB cartridge"),
		UI_MENU_ACTION(10, "Diamond 64 KB cartridge"),
		UI_MENU_ACTION(11, "SpartaDOS X 64 KB cartridge"),
		UI_MENU_ACTION(12, "XEGS 32 KB cartridge"),
		UI_MENU_ACTION(13, "XEGS 64 KB cartridge"),
		UI_MENU_ACTION(14, "XEGS 128 KB cartridge"),
		UI_MENU_ACTION(15, "OSS 'M091' 16 KB cartridge"),
		UI_MENU_ACTION(16, "One chip 16 KB 5200 cartridge"),
		UI_MENU_ACTION(17, "Atrax 128 KB cartridge"),
		UI_MENU_ACTION(18, "Bounty Bob 40 KB cartridge"),
		UI_MENU_ACTION(19, "Standard 8 KB 5200 cartridge"),
		UI_MENU_ACTION(20, "Standard 4 KB 5200 cartridge"),
		UI_MENU_ACTION(21, "Right slot 8 KB cartridge"),
		UI_MENU_ACTION(22, "32 KB Williams cartridge"),
		UI_MENU_ACTION(23, "XEGS 256 KB cartridge"),
		UI_MENU_ACTION(24, "XEGS 512 KB cartridge"),
		UI_MENU_ACTION(25, "XEGS 1 MB cartridge"),
		UI_MENU_ACTION(26, "MegaCart 16 KB cartridge"),
		UI_MENU_ACTION(27, "MegaCart 32 KB cartridge"),
		UI_MENU_ACTION(28, "MegaCart 64 KB cartridge"),
		UI_MENU_ACTION(29, "MegaCart 128 KB cartridge"),
		UI_MENU_ACTION(30, "MegaCart 256 KB cartridge"),
		UI_MENU_ACTION(31, "MegaCart 512 KB cartridge"),
		UI_MENU_ACTION(32, "MegaCart 1 MB cartridge"),
		UI_MENU_ACTION(33, "Switchable XEGS 32 KB cartridge"),
		UI_MENU_ACTION(34, "Switchable XEGS 64 KB cartridge"),
		UI_MENU_ACTION(35, "Switchable XEGS 128 KB cartridge"),
		UI_MENU_ACTION(36, "Switchable XEGS 256 KB cartridge"),
		UI_MENU_ACTION(37, "Switchable XEGS 512 KB cartridge"),
		UI_MENU_ACTION(38, "Switchable XEGS 1 MB cartridge"),
		UI_MENU_ACTION(39, "Phoenix 8 KB cartridge"),
		UI_MENU_ACTION(40, "Blizzard 16 KB cartridge"),
		UI_MENU_ACTION(41, "Atarimax 128 KB Flash cartridge"),
		UI_MENU_ACTION(42, "Atarimax 1 MB Flash cartridge"),
		UI_MENU_ACTION(43, "SpartaDOS X 128 KB cartridge"),
		UI_MENU_END
	};

	int i;
	int option = 0;

	UI_driver->fInit();

	for (i = 1; i <= CARTRIDGE_LAST_SUPPORTED; i++)
		if (CARTRIDGE_kb[i] == k) {
			if (option == 0)
				option = i;
			menu_array[i - 1].flags = UI_ITEM_ACTION;
		}
		else
			menu_array[i - 1].flags = UI_ITEM_HIDDEN;

	if (option == 0)
		return CARTRIDGE_NONE;

	option = UI_driver->fSelect("Select Cartridge Type", 0, option, menu_array, NULL);
	if (option > 0)
		return option;

	return CARTRIDGE_NONE;
}

static void CartManagement(void)
{
	static UI_tMenuItem menu_array_sdx[] = {
		UI_MENU_FILESEL(0, "Create Cartridge from ROM image"),
		UI_MENU_FILESEL(1, "Extract ROM image from Cartridge"),
		UI_MENU_FILESEL(2, "Insert Cartridge"),
		UI_MENU_ACTION(3, "Remove Cartridge"),
		UI_MENU_FILESEL(4, "Insert SDX Piggyback Cartridge"),
		UI_MENU_ACTION(5, "Remove SDX Piggyback Cartridge"),
		UI_MENU_END
	};
	
	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL(0, "Create Cartridge from ROM image"),
		UI_MENU_FILESEL(1, "Extract ROM image from Cartridge"),
		UI_MENU_FILESEL(2, "Insert Cartridge"),
		UI_MENU_ACTION(3, "Remove Cartridge"),
		UI_MENU_END
	};
	
	typedef struct {
		UBYTE id[4];
		UBYTE type[4];
		UBYTE checksum[4];
		UBYTE gash[4];
	} Header;
	
	int option = 2;

	for (;;) {
		static char cart_filename[FILENAME_MAX] = "";
		
		if (CARTRIDGE_type != CARTRIDGE_SDX_64 && CARTRIDGE_type != CARTRIDGE_SDX_128) {
			option = UI_driver->fSelect("Cartridge Management", 0, option, menu_array, NULL);
		}
		else
		{
			option = UI_driver->fSelect("Cartridge Management", 0, option, menu_array_sdx, NULL);
		}

		switch (option) {
		case 0:
			if (UI_driver->fGetLoadFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
				FILE *f;
				int nbytes;
				int type;
				UBYTE *image;
				int checksum;
				Header header;

				f = fopen(cart_filename, "rb");
				if (f == NULL) {
					CantLoad(cart_filename);
					break;
				}
				nbytes = Util_flen(f);
				if ((nbytes & 0x3ff) != 0) {
					fclose(f);
					UI_driver->fMessage("ROM image must be full kilobytes long", 1);
					break;
				}
				type = UI_SelectCartType(nbytes >> 10);
				if (type == CARTRIDGE_NONE) {
					fclose(f);
					break;
				}

				image = (UBYTE *) Util_malloc(nbytes);
				Util_rewind(f);
				if ((int) fread(image, 1, nbytes, f) != nbytes) {
					fclose(f);
					CantLoad(cart_filename);
					break;
				}
				fclose(f);

				if (!UI_driver->fGetSaveFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir))
					break;

				checksum = CARTRIDGE_Checksum(image, nbytes);
				header.id[0] = 'C';
				header.id[1] = 'A';
				header.id[2] = 'R';
				header.id[3] = 'T';
				header.type[0] = '\0';
				header.type[1] = '\0';
				header.type[2] = '\0';
				header.type[3] = (UBYTE) type;
				header.checksum[0] = (UBYTE) (checksum >> 24);
				header.checksum[1] = (UBYTE) (checksum >> 16);
				header.checksum[2] = (UBYTE) (checksum >> 8);
				header.checksum[3] = (UBYTE) checksum;
				header.gash[0] = '\0';
				header.gash[1] = '\0';
				header.gash[2] = '\0';
				header.gash[3] = '\0';

				f = fopen(cart_filename, "wb");
				if (f == NULL) {
					CantSave(cart_filename);
					break;
				}
				fwrite(&header, 1, sizeof(header), f);
				fwrite(image, 1, nbytes, f);
				fclose(f);
				free(image);
				Created(cart_filename);
			}
			break;
		case 1:
			if (UI_driver->fGetLoadFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
				FILE *f;
				int nbytes;
				Header header;
				UBYTE *image;

				f = fopen(cart_filename, "rb");
				if (f == NULL) {
					CantLoad(cart_filename);
					break;
				}
				nbytes = Util_flen(f) - sizeof(header);
				Util_rewind(f);
				if (nbytes <= 0 || fread(&header, 1, sizeof(header), f) != sizeof(header)
				 || header.id[0] != 'C' || header.id[1] != 'A' || header.id[2] != 'R' || header.id[3] != 'T') {
					fclose(f);
					UI_driver->fMessage("Not a CART file", 1);
					break;
				}
				image = (UBYTE *) Util_malloc(nbytes);
				fread(image, 1, nbytes, f);
				fclose(f);

				if (!UI_driver->fGetSaveFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir))
					break;

				f = fopen(cart_filename, "wb");
				if (f == NULL) {
					CantSave(cart_filename);
					break;
				}
				fwrite(image, 1, nbytes, f);
				fclose(f);
				free(image);
				Created(cart_filename);
			}
			break;
		case 2:
			if (UI_driver->fGetLoadFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
				int r = CARTRIDGE_Insert(cart_filename);
				switch (r) {
				case CARTRIDGE_CANT_OPEN:
					CantLoad(cart_filename);
					break;
				case CARTRIDGE_BAD_FORMAT:
					UI_driver->fMessage("Unknown cartridge format", 1);
					break;
				case CARTRIDGE_BAD_CHECKSUM:
					UI_driver->fMessage("Warning: bad CART checksum", 1);
					break;
				case 0:
					/* ok */
					break;
				default:
					/* r > 0 */
					CARTRIDGE_type = UI_SelectCartType(r);
					break;
				}
				if (CARTRIDGE_type != CARTRIDGE_NONE) {
					int for5200 = CARTRIDGE_IsFor5200(CARTRIDGE_type);
					if (for5200 && Atari800_machine_type != Atari800_MACHINE_5200) {
						Atari800_machine_type = Atari800_MACHINE_5200;
						MEMORY_ram_size = 16;
						Atari800_InitialiseMachine();
					}
					else if (!for5200 && Atari800_machine_type == Atari800_MACHINE_5200) {
						Atari800_machine_type = Atari800_MACHINE_XLXE;
						MEMORY_ram_size = 64;
						Atari800_InitialiseMachine();
					}
				}
				Atari800_Coldstart();
				return;
			}
			break;
		case 3:
			CARTRIDGE_Remove();
			Atari800_Coldstart();
			return;
		case 4:
			if (UI_driver->fGetLoadFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
				int r = CARTRIDGE_Insert_Second(cart_filename);
				switch (r) {
				case CARTRIDGE_CANT_OPEN:
					CantLoad(cart_filename);
					break;
				case CARTRIDGE_BAD_FORMAT:
					UI_driver->fMessage("Unknown cartridge format", 1);
					break;
				case CARTRIDGE_BAD_CHECKSUM:
					UI_driver->fMessage("Warning: bad CART checksum", 1);
					break;
				case 0:
					/* ok */
					break;
				default:
					/* r > 0 */
					CARTRIDGE_second_type = UI_SelectCartType(r);
					break;
				}
				return;
		case 5:
			CARTRIDGE_Remove_Second();
			return;
		default:
			return;
			}
		}
	}
}

#if defined(SOUND) && !defined(DREAMCAST)
static void SoundRecording(void)
{
	if (!SndSave_IsSoundFileOpen()) {
		int no = 0;
		do {
			char buffer[32];
			sprintf(buffer, "atari%03d.wav", no);
			if (!Util_fileexists(buffer)) {
				/* file does not exist - we can create it */
				FilenameMessage(SndSave_OpenSoundFile(buffer)
					? "Recording sound to file \"%s\""
					: "Can't write to file \"%s\"", buffer);
				return;
			}
		} while (++no < 1000);
		UI_driver->fMessage("All atariXXX.wav files exist!", 1);
	}
	else {
		SndSave_CloseSoundFile();
		UI_driver->fMessage("Recording stopped", 1);
	}
}
#endif /* defined(SOUND) && !defined(DREAMCAST) */

static int AutostartFile(void)
{
	static char filename[FILENAME_MAX] = "";
	if (UI_driver->fGetLoadFilename(filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
		if (AFILE_OpenFile(filename, TRUE, 1, FALSE))
			return TRUE;
		CantLoad(filename);
	}
	return FALSE;
}

static void LoadTape(void)
{
	static char filename[FILENAME_MAX] = "";
	if (UI_driver->fGetLoadFilename(filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
		if (!CASSETTE_Insert(filename))
			CantLoad(filename);
	}
}

static void AdvancedHOptions(void)
{
	static char open_info[] = "0 currently open files";
	static UI_tMenuItem menu_array[] = {
		UI_MENU_ACTION(0, "Atari executables path"),
		UI_MENU_ACTION_TIP(1, open_info, NULL),
		UI_MENU_LABEL("Current directories:"),
		UI_MENU_ACTION_PREFIX_TIP(2, "H1:", Devices_h_current_dir[0], NULL),
		UI_MENU_ACTION_PREFIX_TIP(3, "H2:", Devices_h_current_dir[1], NULL),
		UI_MENU_ACTION_PREFIX_TIP(4, "H3:", Devices_h_current_dir[2], NULL),
		UI_MENU_ACTION_PREFIX_TIP(5, "H4:", Devices_h_current_dir[3], NULL),
		UI_MENU_END
	};
	int option = 0;
	for (;;) {
		int i;
		int seltype;
		i = Devices_H_CountOpen();
		open_info[0] = (char) ('0' + i);
		open_info[21] = (i != 1) ? 's' : '\0';
		menu_array[1].suffix = (i > 0) ? ((i == 1) ? "Backspace: close" : "Backspace: close all") : NULL;
		for (i = 0; i < 4; i++)
			menu_array[3 + i].suffix = Devices_h_current_dir[i][0] != '\0' ? "Backspace: reset to root" : NULL;
		option = UI_driver->fSelect("Advanced H: options", 0, option, menu_array, &seltype);
		switch (option) {
		case 0:
			{
				char tmp_path[FILENAME_MAX];
				strcpy(tmp_path, Devices_h_exe_path);
				if (UI_driver->fEditString("Atari executables path", tmp_path, FILENAME_MAX))
					strcpy(Devices_h_exe_path, tmp_path);
			}
			break;
		case 1:
			if (seltype == UI_USER_DELETE)
				Devices_H_CloseAll();
			break;
		case 2:
		case 3:
		case 4:
		case 5:
			if (seltype == UI_USER_DELETE)
				Devices_h_current_dir[option - 2][0] = '\0';
			break;
		default:
			return;
		}
	}
}

static void ConfigureDirectories(void)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_LABEL("Directories with Atari software:"),
		UI_MENU_FILESEL(0, UI_atari_files_dir[0]),
		UI_MENU_FILESEL(1, UI_atari_files_dir[1]),
		UI_MENU_FILESEL(2, UI_atari_files_dir[2]),
		UI_MENU_FILESEL(3, UI_atari_files_dir[3]),
		UI_MENU_FILESEL(4, UI_atari_files_dir[4]),
		UI_MENU_FILESEL(5, UI_atari_files_dir[5]),
		UI_MENU_FILESEL(6, UI_atari_files_dir[6]),
		UI_MENU_FILESEL(7, UI_atari_files_dir[7]),
		UI_MENU_FILESEL(8, "[add directory]"),
		UI_MENU_LABEL("Directories for emulator-saved files:"),
		UI_MENU_FILESEL(10, UI_saved_files_dir[0]),
		UI_MENU_FILESEL(11, UI_saved_files_dir[1]),
		UI_MENU_FILESEL(12, UI_saved_files_dir[2]),
		UI_MENU_FILESEL(13, UI_saved_files_dir[3]),
		UI_MENU_FILESEL(14, UI_saved_files_dir[4]),
		UI_MENU_FILESEL(15, UI_saved_files_dir[5]),
		UI_MENU_FILESEL(16, UI_saved_files_dir[6]),
		UI_MENU_FILESEL(17, UI_saved_files_dir[7]),
		UI_MENU_FILESEL(18, "[add directory]"),
		UI_MENU_ACTION(9, "What's this?"),
		UI_MENU_ACTION(19, "Back to Emulator Settings"),
		UI_MENU_END
	};
	int option = 9;
	int flags = 0;
	for (;;) {
		int i;
		int seltype;
		char tmp_dir[FILENAME_MAX];
		for (i = 0; i < 8; i++) {
			menu_array[1 + i].flags = (i < UI_n_atari_files_dir) ? (UI_ITEM_FILESEL | UI_ITEM_TIP) : UI_ITEM_HIDDEN;
			menu_array[11 + i].flags = (i < UI_n_saved_files_dir) ? (UI_ITEM_FILESEL | UI_ITEM_TIP) : UI_ITEM_HIDDEN;
			menu_array[1 + i].suffix = menu_array[11 + i].suffix = (flags != 0)
				? "Up/Down:move Space:release"
				: "Ret:change Bksp:delete Space:reorder";
		}
		if (UI_n_atari_files_dir < 2)
			menu_array[1].suffix = "Return:change Backspace:delete";
		if (UI_n_saved_files_dir < 2)
			menu_array[11].suffix = "Return:change Backspace:delete";
		menu_array[9].flags = (UI_n_atari_files_dir < 8) ? UI_ITEM_FILESEL : UI_ITEM_HIDDEN;
		menu_array[19].flags = (UI_n_saved_files_dir < 8) ? UI_ITEM_FILESEL : UI_ITEM_HIDDEN;
		option = UI_driver->fSelect("Configure Directories", flags, option, menu_array, &seltype);
		if (option < 0)
			return;
		if (flags != 0) {
			switch (seltype) {
			case UI_USER_DRAG_UP:
				if (option != 0 && option != 10) {
					strcpy(tmp_dir, menu_array[1 + option].item);
					strcpy(menu_array[1 + option].item, menu_array[option].item);
					strcpy(menu_array[option].item, tmp_dir);
					option--;
				}
				break;
			case UI_USER_DRAG_DOWN:
				if (option != UI_n_atari_files_dir - 1 && option != 10 + UI_n_saved_files_dir - 1) {
					strcpy(tmp_dir, menu_array[1 + option].item);
					strcpy(menu_array[1 + option].item, menu_array[2 + option].item);
					strcpy(menu_array[2 + option].item, tmp_dir);
					option++;
				}
				break;
			default:
				flags = 0;
				break;
			}
			continue;
		}
		switch (option) {
		case 8:
			tmp_dir[0] = '\0';
			if (UI_driver->fGetDirectoryPath(tmp_dir)) {
				strcpy(UI_atari_files_dir[UI_n_atari_files_dir], tmp_dir);
				option = UI_n_atari_files_dir++;
			}
			break;
		case 18:
			tmp_dir[0] = '\0';
			if (UI_driver->fGetDirectoryPath(tmp_dir)) {
				strcpy(UI_saved_files_dir[UI_n_saved_files_dir], tmp_dir);
				option = 10 + UI_n_saved_files_dir++;
			}
			break;
		case 9:
#if 0
			{
				static const UI_tMenuItem help_menu_array[] = {
					UI_MENU_LABEL("You can configure directories,"),
					UI_MENU_LABEL("where you usually store files used by"),
					UI_MENU_LABEL("Atari800, to make them available"),
					UI_MENU_LABEL("at Tab key press in file selectors."),
					UI_MENU_LABEL(""),
					UI_MENU_LABEL("\"Directories with Atari software\""),
					UI_MENU_LABEL("are for disk images, cartridge images,"),
					UI_MENU_LABEL("tape images, executables and BASIC"),
					UI_MENU_LABEL("programs."),
					UI_MENU_LABEL(""),
					UI_MENU_LABEL("\"Directories for emulator-saved files\""),
					UI_MENU_LABEL("are for state files, disk sets and"),
					UI_MENU_LABEL("screenshots taken via User Interface."),
					UI_MENU_LABEL(""),
					UI_MENU_LABEL(""),
					UI_MENU_ACTION(0, "Back"),
					UI_MENU_END
				};
				UI_driver->fSelect("Configure Directories - Help", 0, 0, help_menu_array, NULL);
			}
#else
			UI_driver->fInfoScreen("Configure Directories - Help",
				"You can configure directories,\0"
				"where you usually store files used by\0"
				"Atari800, to make them available\0"
				"at Tab key press in file selectors.\0"
				"\0"
				"\"Directories with Atari software\"\0"
				"are for disk images, cartridge images,\0"
				"tape images, executables\0"
				"and BASIC programs.\0"
				"\0"
				"\"Directories for emulator-saved files\"\0"
				"are for state files, disk sets and\0"
				"screenshots taken via User Interface.\0"
				"\n");
#endif
			break;
		case 19:
			return;
		default:
			if (seltype == UI_USER_TOGGLE) {
				if ((option < 10 ? UI_n_atari_files_dir : UI_n_saved_files_dir) > 1)
					flags = UI_SELECT_DRAG;
			}
			else if (seltype == UI_USER_DELETE) {
				if (option < 10) {
					if (option >= --UI_n_atari_files_dir) {
						option = 8;
						break;
					}
					for (i = option; i < UI_n_atari_files_dir; i++)
						strcpy(UI_atari_files_dir[i], UI_atari_files_dir[i + 1]);
				}
				else {
					if (option >= --UI_n_saved_files_dir) {
						option = 18;
						break;
					}
					for (i = option - 10; i < UI_n_saved_files_dir; i++)
						strcpy(UI_saved_files_dir[i], UI_saved_files_dir[i + 1]);
				}
			}
			else
				UI_driver->fGetDirectoryPath(menu_array[1 + option].item);
			break;
		}
	}
}

static void AtariSettings(void)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_CHECK(0, "Disable BASIC when booting Atari:"),
		UI_MENU_CHECK(1, "Boot from tape (hold Start):"),
		UI_MENU_CHECK(2, "Enable R-Time 8:"),
		UI_MENU_CHECK(3, "SIO patch (fast disk access):"),
		UI_MENU_CHECK(21, "Turbo (F12):"),
		UI_MENU_ACTION(4, "H: device (hard disk):"),
		UI_MENU_CHECK(5, "P: device (printer):"),
#ifdef R_IO_DEVICE
		UI_MENU_CHECK(6, "R: device (Atari850 via net):"),
#endif
		UI_MENU_FILESEL_PREFIX(7, "H1: ", Devices_atari_h_dir[0]),
		UI_MENU_FILESEL_PREFIX(8, "H2: ", Devices_atari_h_dir[1]),
		UI_MENU_FILESEL_PREFIX(9, "H3: ", Devices_atari_h_dir[2]),
		UI_MENU_FILESEL_PREFIX(10, "H4: ", Devices_atari_h_dir[3]),
		UI_MENU_SUBMENU(20, "Advanced H: options"),
		UI_MENU_ACTION_PREFIX(11, "Print command: ", Devices_print_command),
		UI_MENU_FILESEL_PREFIX(12, " OS/A ROM: ", CFG_osa_filename),
		UI_MENU_FILESEL_PREFIX(13, " OS/B ROM: ", CFG_osb_filename),
		UI_MENU_FILESEL_PREFIX(14, "XL/XE ROM: ", CFG_xlxe_filename),
		UI_MENU_FILESEL_PREFIX(15, " 5200 ROM: ", CFG_5200_filename),
		UI_MENU_FILESEL_PREFIX(16, "BASIC ROM: ", CFG_basic_filename),
		UI_MENU_FILESEL(17, "Find ROM images in a directory"),
		UI_MENU_SUBMENU(18, "Configure directories"),
		UI_MENU_ACTION(19, "Save configuration file"),
		UI_MENU_END
	};
	char tmp_command[256];
	char rom_dir[FILENAME_MAX];

	int option = 0;

	for (;;) {
		int seltype;
		SetItemChecked(menu_array, 0, Atari800_disable_basic);
		SetItemChecked(menu_array, 1, CASSETTE_hold_start_on_reboot);
		SetItemChecked(menu_array, 2, RTIME_enabled);
		SetItemChecked(menu_array, 3, ESC_enable_sio_patch);
		SetItemChecked(menu_array, 21, Atari800_turbo);
		menu_array[5].suffix = Devices_enable_h_patch ? (Devices_h_read_only ? "Read-only" : "Read/write") : "No ";
		SetItemChecked(menu_array, 5, Devices_enable_p_patch);
#ifdef R_IO_DEVICE
		SetItemChecked(menu_array, 6, Devices_enable_r_patch);
#endif

		option = UI_driver->fSelect("Emulator Settings", 0, option, menu_array, &seltype);

		switch (option) {
		case 0:
			Atari800_disable_basic = !Atari800_disable_basic;
			break;
		case 1:
			CASSETTE_hold_start_on_reboot = !CASSETTE_hold_start_on_reboot;
			CASSETTE_hold_start = CASSETTE_hold_start_on_reboot;
			break;
		case 2:
			RTIME_enabled = !RTIME_enabled;
			break;
		case 3:
			ESC_enable_sio_patch = !ESC_enable_sio_patch;
			break;
		case 4:
			if (!Devices_enable_h_patch) {
				Devices_enable_h_patch = TRUE;
				Devices_h_read_only = TRUE;
			}
			else if (Devices_h_read_only)
				Devices_h_read_only = FALSE;
			else {
				Devices_enable_h_patch = FALSE;
				Devices_h_read_only = TRUE;
			}
			break;
		case 5:
			Devices_enable_p_patch = !Devices_enable_p_patch;
			break;
#ifdef R_IO_DEVICE
		case 6:
			Devices_enable_r_patch = !Devices_enable_r_patch;
			break;
#endif
		case 7:
		case 8:
		case 9:
		case 10:
			if (seltype == UI_USER_DELETE)
				FindMenuItem(menu_array, option)->item[0] = '\0';
			else
				UI_driver->fGetDirectoryPath(FindMenuItem(menu_array, option)->item);
			break;
		case 20:
			AdvancedHOptions();
			break;
		case 11:
			strcpy(tmp_command, Devices_print_command);
			if (UI_driver->fEditString("Print command", tmp_command, sizeof(tmp_command)))
				if (!Devices_SetPrintCommand(tmp_command))
					UI_driver->fMessage("Specified command is not allowed", 1);
			break;
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
			if (seltype == UI_USER_DELETE)
				FindMenuItem(menu_array, option)->item[0] = '\0';
			else
				UI_driver->fGetLoadFilename(FindMenuItem(menu_array, option)->item, NULL, 0);
			break;
		case 17:
			Util_splitpath(CFG_xlxe_filename, rom_dir, NULL);
			if (UI_driver->fGetDirectoryPath(rom_dir))
				CFG_FindROMImages(rom_dir, FALSE);
			break;
		case 18:
			ConfigureDirectories();
			break;
		case 19:
			UI_driver->fMessage(CFG_WriteConfig() ? "Configuration file updated" : "Error writing configuration file", 1);
			break;
		case 21:
			Atari800_turbo = !Atari800_turbo;
			break;
		default:
			ESC_UpdatePatches();
			return;
		}
	}
}

static char state_filename[FILENAME_MAX] = "";

static void SaveState(void)
{
	if (UI_driver->fGetSaveFilename(state_filename, UI_saved_files_dir, UI_n_saved_files_dir)) {
		int result;
		UI_driver->fMessage("Please wait while saving...", 0);
#if defined(XEP80_EMULATION) || defined(AF80) || defined(PBI_PROTO80)
		/* Save true 80 column state */
		PLATFORM_show_80 = saved_show_80;
#endif
		result = StateSav_SaveAtariState(state_filename, "wb", TRUE);
#if defined(XEP80_EMULATION) || defined(AF80) || defined(PBI_PROTO80)
		PLATFORM_show_80 = FALSE;
#endif
		if (!result)
			CantSave(state_filename);
	}
}

static void LoadState(void)
{
	if (UI_driver->fGetLoadFilename(state_filename, UI_saved_files_dir, UI_n_saved_files_dir)) {
		UI_driver->fMessage("Please wait while loading...", 0);
		if (!StateSav_ReadAtariState(state_filename, "rb"))
			CantLoad(state_filename);
	}
#if defined(XEP80_EMULATION) || defined(AF80) || defined(PBI_PROTO80)
	saved_show_80 = PLATFORM_show_80;
	PLATFORM_show_80 = FALSE;
#endif
}

/* CURSES_BASIC doesn't use artifacting or Atari800_collisions_in_skipped_frames,
   but does use Atari800_refresh_rate. However, changing Atari800_refresh_rate on CURSES is mostly
   useless, as the text-mode screen is normally updated infrequently by Atari.
   In case you wonder how artifacting affects CURSES version without
   CURSES_BASIC: it is visible on the screenshots (yes, they are bitmaps). */
#ifndef CURSES_BASIC

#if SUPPORTS_PLATFORM_PALETTEUPDATE
/* An array of pointers to colour controls (brightness, contrast, NTSC filter
   controls, etc.). Used to display values of controls in GUI. */
static struct {
	double *setting; /* Pointer to a setup value */
	char string[10]; /* string representation, for displaying */
} colour_controls[12];

/* Updates the string representation of a single colour control. */
static void UpdateColourControl(const int idx)
{
	snprintf(colour_controls[idx].string,
		 sizeof(colour_controls[0].string) - 1,
		 "%f",
		 *(colour_controls[idx].setting));
}

/* Sets pointers to colour controls properly, and hides/shows the Hue and
   Color delay controls, which are applicable only for NTSC. */
static void UpdateColourControls(UI_tMenuItem menu_array[])
{
	int i;
	colour_controls[0].setting = &Colours_setup->brightness;
	colour_controls[1].setting = &Colours_setup->contrast;
	colour_controls[2].setting = &Colours_setup->saturation;
	colour_controls[3].setting = &COLOURS_NTSC_specific_setup.hue;
	colour_controls[4].setting = &Colours_setup->gamma;
	colour_controls[5].setting = &COLOURS_NTSC_specific_setup.color_delay;
	for (i = 0; i < 6; i ++)
		UpdateColourControl(i);
	/* Hide/show Hue and Color delay. */
	if (Atari800_tv_mode == Atari800_TV_NTSC) {
		FindMenuItem(menu_array, 16)->flags = UI_ITEM_ACTION;
		FindMenuItem(menu_array, 18)->flags = UI_ITEM_ACTION;
	} else {
		FindMenuItem(menu_array, 16)->flags = UI_ITEM_HIDDEN;
		FindMenuItem(menu_array, 18)->flags = UI_ITEM_HIDDEN;
	}
}

#if NTSC_FILTER
/* Submenu with controls for NTSC filter. */
static void NTSCFilterSettings(void)
{
	int i;
	static UI_tMenuItem menu_array[] = {
		UI_MENU_ACTION_PREFIX(0, "Sharpness: ", colour_controls[6].string),
		UI_MENU_ACTION_PREFIX(1, "Resolution: ", colour_controls[7].string),
		UI_MENU_ACTION_PREFIX(2, "Artifacts: ", colour_controls[8].string),
		UI_MENU_ACTION_PREFIX(3, "Fringing: ", colour_controls[9].string),
		UI_MENU_ACTION_PREFIX(4, "Bleed: ", colour_controls[10].string),
		UI_MENU_ACTION_PREFIX(5, "Burst phase: ", colour_controls[11].string),
		UI_MENU_END
	};

	int option = 0;

	/* Set pointers to colour controls. */
	colour_controls[6].setting = &FILTER_NTSC_setup.sharpness;
	colour_controls[7].setting = &FILTER_NTSC_setup.resolution;
	colour_controls[8].setting = &FILTER_NTSC_setup.artifacts;
	colour_controls[9].setting = &FILTER_NTSC_setup.fringing;
	colour_controls[10].setting = &FILTER_NTSC_setup.bleed;
	colour_controls[11].setting = &FILTER_NTSC_setup.burst_phase;
	for (i = 6; i < 12; i ++)
		UpdateColourControl(i);

	for (;;) {
		option = UI_driver->fSelect("NTSC Filter Settings", 0, option, menu_array, NULL);
		switch (option) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			{
				char buffer[10]; /* same size as in colour_controls[0].string */
				memcpy(buffer, colour_controls[option + 6].string, sizeof(buffer));
				if (UI_driver->fEditString("Enter value", buffer, sizeof(buffer))) {
					*(colour_controls[option + 6].setting) = atof(buffer);
					UpdateColourControl(option + 6);
					Colours_Update();
				}
			}
			break;
		default:
			return;
		}
	}
}
#endif /* NTSC_FILTER */

/* Saves the current colours, including adjustments, in a palette file chosen
   by user. */
static void SavePalette(void)
{
	static char filename[FILENAME_MAX] = "";
	if (UI_driver->fGetSaveFilename(filename, UI_saved_files_dir, UI_n_saved_files_dir)) {
		UI_driver->fMessage("Please wait while saving...", 0);
		if (!Colours_Save(filename))
			CantSave(filename);
	}
}
#endif /* SUPPORTS_PLATFORM_PALETTEUPDATE */

static void DisplaySettings(void)
{
	static const UI_tMenuItem artif_quality_menu_array[] = {
		UI_MENU_ACTION(0, "off"),
		UI_MENU_ACTION(1, "original"),
		UI_MENU_ACTION(2, "new"),
#if NTSC_FILTER
		UI_MENU_ACTION(3, "NTSC filter"),
#endif
		UI_MENU_END
	};
	static const UI_tMenuItem artif_mode_menu_array[] = {
		UI_MENU_ACTION(0, "blue/brown 1"),
		UI_MENU_ACTION(1, "blue/brown 2"),
		UI_MENU_ACTION(2, "GTIA"),
		UI_MENU_ACTION(3, "CTIA"),
		UI_MENU_END
	};

#if SUPPORTS_PLATFORM_PALETTEUPDATE	
	static const UI_tMenuItem calibration_profile_menu_array[] = {
		UI_MENU_ACTION(0, "Standard [Atari Raster Standard]"),
		UI_MENU_ACTION(1, "Classic  [Black-Level Optimized]"),
		UI_MENU_ACTION(2, "Arcade   [Levels & Color Enhanced]"),
		UI_MENU_END
	};
#endif
	
	static char refresh_status[16];
	static UI_tMenuItem menu_array[] = {
		UI_MENU_SUBMENU_SUFFIX(0, "NTSC artifacting quality:", NULL),
		UI_MENU_SUBMENU_SUFFIX(11, "NTSC artifacting mode:", NULL),
		UI_MENU_SUBMENU_SUFFIX(1, "Current refresh rate:", refresh_status),
		UI_MENU_CHECK(2, "Accurate skipped frames:"),
		UI_MENU_CHECK(3, "Show percents of Atari speed:"),
		UI_MENU_CHECK(4, "Show disk drive activity:"),
		UI_MENU_CHECK(5, "Show sector counter:"),
#ifdef _WIN32_WCE
		UI_MENU_CHECK(6, "Enable linear filtering:"),
#endif
#ifdef DREAMCAST
		UI_MENU_CHECK(7, "Double buffer video data:"),
		UI_MENU_ACTION(8, "Emulator video mode:"),
		UI_MENU_ACTION(9, "Display video mode:"),
#ifdef HZ_TEST
		UI_MENU_ACTION(10, "DO HZ TEST:"),
#endif
#endif
#if SUPPORTS_PLATFORM_PALETTEUPDATE
	    UI_MENU_SUBMENU_SUFFIX(12, "Video Calibration Profile: ", NULL),
		UI_MENU_ACTION_PREFIX(13, " Brightness: ", colour_controls[0].string),
		UI_MENU_ACTION_PREFIX(14, " Contrast: ", colour_controls[1].string),
		UI_MENU_ACTION_PREFIX(15, " Saturation: ", colour_controls[2].string),
		UI_MENU_ACTION_PREFIX(16, " Hue: ", colour_controls[3].string),
		UI_MENU_ACTION_PREFIX(17, " Gamma: ", colour_controls[4].string),
		UI_MENU_ACTION_PREFIX(18, " GTIA delay: ", colour_controls[5].string),
#if NTSC_FILTER
		UI_MENU_SUBMENU(19, "NTSC filter settings"),
#endif
		UI_MENU_ACTION(20, "Restore default colors"),
		UI_MENU_FILESEL_PREFIX_TIP(21, "External palette: ", NULL, NULL),
		UI_MENU_CHECK(22, "Also adjust external palette: "),
		UI_MENU_FILESEL(23, "Save current palette"),
#endif /* SUPPORTS_PLATFORM_PALETTEUPDATE */
		UI_MENU_END
	};

	int option = 0;
	int option2;
	int seltype;

	/* Current artifacting quality, computed from
	   PLATFORM_artifacting and ANTIC_artif_new */
	int artif_quality;

#if SUPPORTS_PLATFORM_PALETTEUPDATE
	int i;
	UpdateColourControls(menu_array);
#endif

	for (;;) {
#if NTSC_FILTER
#if SUPPORTS_PLATFORM_PALETTEUPDATE
		/* Show NTSC filter option only if necessary. */
		if (PLATFORM_filter == PLATFORM_FILTER_NTSC)
			FindMenuItem(menu_array, 19)->flags = UI_ITEM_SUBMENU;
		else
			FindMenuItem(menu_array, 19)->flags = UI_ITEM_HIDDEN;
#endif

		/* Computing current artifacting quality... */
		if (PLATFORM_filter != PLATFORM_FILTER_NONE) {
			/* NTSC filter is on */
			FindMenuItem(menu_array, 0)->suffix = artif_quality_menu_array[2 + PLATFORM_filter].item;
			FindMenuItem(menu_array, 11)->suffix = "N/A";
			artif_quality = 2 + PLATFORM_filter;
		} else
#endif /* NTSC_FILTER */
		if (ANTIC_artif_mode == 0) { /* artifacting is off */
			FindMenuItem(menu_array, 0)->suffix = artif_quality_menu_array[0].item;
			FindMenuItem(menu_array, 11)->suffix = "N/A";
			artif_quality = 0;
		} else { /* ANTIC artifacting is on */
			FindMenuItem(menu_array, 0)->suffix = artif_quality_menu_array[1 + ANTIC_artif_new].item;
			FindMenuItem(menu_array, 11)->suffix = artif_mode_menu_array[ANTIC_artif_mode - 1].item;
			artif_quality = 1 + ANTIC_artif_new;
		}

#if SUPPORTS_PLATFORM_PALETTEUPDATE
		calibration_profile = Colours_Get_Calibration_Profile();
		if (calibration_profile == COLOURS_CUSTOM)
			FindMenuItem(menu_array, 12)->suffix = "Custom"; 
		else if (calibration_profile == COLOURS_STANDARD)
			FindMenuItem(menu_array, 12)->suffix = "Standard";
		else if (calibration_profile == COLOURS_CLASSIC)
			FindMenuItem(menu_array, 12)->suffix = "Classic";
		else if (calibration_profile == COLOURS_ARCADE)
			FindMenuItem(menu_array, 12)->suffix = "Arcade";
		
		/* Set the palette file description */
		if (Colours_external->loaded) {
			FindMenuItem(menu_array, 21)->item = Colours_external->filename;
			FindMenuItem(menu_array, 21)->suffix = "Return:load Backspace:remove";
			FindMenuItem(menu_array, 22)->flags = UI_ITEM_CHECK;
			SetItemChecked(menu_array, 22, Colours_external->adjust);
		} else {
			FindMenuItem(menu_array, 21)->item = "None";
			FindMenuItem(menu_array, 21)->suffix = "Return:load";
			FindMenuItem(menu_array, 22)->flags = UI_ITEM_ACTION;
			FindMenuItem(menu_array, 22)->suffix = "N/A";
		}
#endif

		sprintf(refresh_status, "1:%-2d", Atari800_refresh_rate);
		SetItemChecked(menu_array, 2, Atari800_collisions_in_skipped_frames);
		SetItemChecked(menu_array, 3, Screen_show_atari_speed);
		SetItemChecked(menu_array, 4, Screen_show_disk_led);
		SetItemChecked(menu_array, 5, Screen_show_sector_counter);
#ifdef _WIN32_WCE
		FindMenuItem(menu_array, 6)->flags = filter_available ? (smooth_filter ? (UI_ITEM_CHECK | UI_ITEM_CHECKED) : UI_ITEM_CHECK) : UI_ITEM_HIDDEN;
#endif
#ifdef DREAMCAST
		SetItemChecked(menu_array, 7, db_mode);
		FindMenuItem(menu_array, 8)->suffix = Atari800_tv_mode == Atari800_TV_NTSC ? "NTSC" : "PAL";
		FindMenuItem(menu_array, 9)->suffix = screen_tv_mode == Atari800_TV_NTSC ? "NTSC" : "PAL";
#endif
		option = UI_driver->fSelect("Display Settings", 0, option, menu_array, &seltype);
		switch (option) {
		case 0:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, artif_quality, artif_quality_menu_array, NULL);
			if (option2 >= 0)
			{
#if NTSC_FILTER
				/* If switched between non-filter and NTSC filter,
				   PLATFORM_filter must be updated. */
				if (option2 >= 3 && artif_quality < 3)
					PLATFORM_SetFilter(option2 - 2);
				else if (option2 < 3 && artif_quality >= 3)
					PLATFORM_SetFilter(PLATFORM_FILTER_NONE);
#endif
				/* ANTIC artifacting settings cannot be turned on
				   when artifacting is off or NTSC filter. */
				if (option2 == 0 || option2 >= 3) {
					ANTIC_artif_new = ANTIC_artif_mode = 0;
				} else {
					/* Do not reset artifacting mode when switched between original and new. */
					if (artif_quality >= 3 || artif_quality == 0)
						/* switched from off/ntsc filter to ANTIC artifacting */
						ANTIC_artif_mode = 1;

					ANTIC_artif_new = option2 - 1;
				}
				ANTIC_UpdateArtifacting();
			}
			break;
		case 11:
			/* The artifacting mode option is only active for ANTIC artifacting. */
			if (artif_quality > 0 && artif_quality < 3)
			{
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, ANTIC_artif_mode - 1, artif_mode_menu_array, NULL);
				if (option2 >= 0) {
					ANTIC_artif_mode = option2 + 1;
					ANTIC_UpdateArtifacting();
				}
			}
			break;
		case 1:
			Atari800_refresh_rate = UI_driver->fSelectInt(Atari800_refresh_rate, 1, 99);
			break;
		case 2:
			if (Atari800_refresh_rate == 1)
				UI_driver->fMessage("No effect with refresh rate 1", 1);
			Atari800_collisions_in_skipped_frames = !Atari800_collisions_in_skipped_frames;
			break;
		case 3:
			Screen_show_atari_speed = !Screen_show_atari_speed;
			break;
		case 4:
			Screen_show_disk_led = !Screen_show_disk_led;
			break;
		case 5:
			Screen_show_sector_counter = !Screen_show_sector_counter;
			break;
#ifdef _WIN32_WCE
		case 6:
			smooth_filter = !smooth_filter;
			break;
#endif
#ifdef DREAMCAST
		case 7:
			if (db_mode)
				db_mode = FALSE;
			else if (Atari800_tv_mode == screen_tv_mode)
				db_mode = TRUE;
			update_screen_updater();
			Screen_EntireDirty();
			break;
		case 8:
			Atari800_tv_mode = (Atari800_tv_mode == Atari800_TV_PAL) ? Atari800_TV_NTSC : Atari800_TV_PAL;
			if (Atari800_tv_mode != screen_tv_mode) {
				db_mode = FALSE;
				update_screen_updater();
			}
			update_vidmode();
			Screen_EntireDirty();
			break;
		case 9:
			Atari800_tv_mode = screen_tv_mode = (screen_tv_mode == Atari800_TV_PAL) ? Atari800_TV_NTSC : Atari800_TV_PAL;
			update_vidmode();
			Screen_EntireDirty();
			break;
#ifdef HZ_TEST
		case 10:
			do_hz_test();
			Screen_EntireDirty();
			break;
#endif
#endif /* DREAMCAST */
#if SUPPORTS_PLATFORM_PALETTEUPDATE
		case 12:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, calibration_profile, calibration_profile_menu_array, NULL);
			if (option2 >= 0) {
				Colours_Set_Calibration_Profile(option2);
				Colours_Update();
				for (i=0; i<6; i++) {
					UpdateColourControl(i);
				}					
			}	
			break;
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:
			{
				char buffer[10]; /* same size as in colour_controls[0].string */
				memcpy(buffer, colour_controls[option - 13].string, sizeof(buffer));
				if (UI_driver->fEditString("Enter value", buffer, sizeof(buffer))) {
					*(colour_controls[option - 13].setting) = atof(buffer);
					UpdateColourControl(option - 13);
					Colours_Update();
				}
			}
			break;
#if NTSC_FILTER
		case 19:
			NTSCFilterSettings();
			break;
#endif
		case 20:
			Colours_RestoreDefaults();
#if NTSC_FILTER
			if (PLATFORM_filter == PLATFORM_FILTER_NTSC)
				FILTER_NTSC_RestoreDefaults();
#endif
			UpdateColourControls(menu_array);
			Colours_Update();
			break;
		case 21:
			switch (seltype) {
			case UI_USER_SELECT:
				if (UI_driver->fGetLoadFilename(Colours_external->filename, UI_saved_files_dir, UI_n_saved_files_dir)) {
					if (COLOURS_EXTERNAL_Read(Colours_external))
						Colours_Update();
					else
						CantLoad(Colours_external->filename);
				}
				break;
			case UI_USER_DELETE: /* Backspace */
				COLOURS_EXTERNAL_Remove(Colours_external);
				Colours_Update();
				break;
			}
			break;
		case 22:
			if (Colours_external->loaded) {
				Colours_external->adjust = !Colours_external->adjust;
				Colours_Update();
			}
			break;
		case 23:
			SavePalette();
			break;
#endif /* SUPPORTS_PLATFORM_PALETTEUPDATE */

		default:
			return;
		}
	}
}

#ifdef DIRECTX
static void WindowsOptions(void)
{
	static const UI_tMenuItem screen_mode_menu_array[] = {
		UI_MENU_ACTION(0, "Fullscreen"),
		UI_MENU_ACTION(1, "Window"),
		UI_MENU_END
	};
	
	static const UI_tMenuItem display_mode_menu_array[] = {
		UI_MENU_ACTION(0, "GDI"),
		UI_MENU_ACTION(1, "GDI+"),
		UI_MENU_ACTION(2, "GDI+/Bilinear"),
		UI_MENU_ACTION(3, "GDI+/Bilinear(HQ)"),
		UI_MENU_ACTION(4, "GDI+/Bicubic(HQ)"),
		UI_MENU_ACTION(5, "Direct3D"),
		UI_MENU_ACTION(6, "Direct3D/Bilinear"),
		UI_MENU_END
	};
	
	static const UI_tMenuItem window_scale_menu_array[] = {
		UI_MENU_ACTION(0, "100% [320x240]"),
		UI_MENU_ACTION(1, "150% [480x360]"),
		UI_MENU_ACTION(2, "200% [640x480]"),
		UI_MENU_ACTION(3, "250% [800x600]"),
		UI_MENU_ACTION(4, "300% [960x720]"),
		UI_MENU_ACTION(5, "350% [1120x840]"),
		UI_MENU_ACTION(6, "400% [1280x960]"),
		UI_MENU_ACTION(7, "450% [1440x1080]"),
		UI_MENU_ACTION(8, "500% [1600x1200]"),
		UI_MENU_END
	};
	
	static const UI_tMenuItem fsresolution_menu_array[] = {
		UI_MENU_ACTION(0, desktopreslabel),
		UI_MENU_ACTION(1, "VGA     [640x480]   (2x)"),
		UI_MENU_ACTION(2, "SXGA    [1280x960]  (4x)"),
		UI_MENU_ACTION(3, "UXGA    [1600x1200] (5x)"),
		UI_MENU_END
	};
	
	static const UI_tMenuItem scaling_method_menu_array[] = {
		UI_MENU_ACTION(0, "Off"),
		UI_MENU_ACTION(1, "Normal"),
		UI_MENU_ACTION(2, "Simple"),
		UI_MENU_ACTION(3, "Adaptive"),
		UI_MENU_END
	};
	
	static const UI_tMenuItem aspect_mode_menu_array[] = {
		UI_MENU_ACTION(0, "Auto       [7:5/4:3]"),
		UI_MENU_ACTION(1, "Wide       [7:5]"),
		UI_MENU_ACTION(2, "Cropped    [4:3]"),
		UI_MENU_ACTION(3, "Compressed [4:3]"),
		UI_MENU_END
	};
	
	static const UI_tMenuItem scanline_mode_menu_array[] = {
		UI_MENU_ACTION(0, "Off"),
		UI_MENU_ACTION(1, "Low     [1x]"),
		UI_MENU_ACTION(2, "Medium  [2x]"),
		UI_MENU_ACTION(3, "High    [3x]"),
		UI_MENU_END
	};

	static char refresh_status[16];
	static UI_tMenuItem menu_array[] = {
	    UI_MENU_SUBMENU_SUFFIX(0, "Display rendering: ", NULL),
		UI_MENU_SUBMENU_SUFFIX(1, "Screen mode: ", NULL),
		UI_MENU_SUBMENU_SUFFIX(2, "Window scale: ", NULL),
		UI_MENU_SUBMENU_SUFFIX(3, "Fullscreen resolution:", NULL),
		UI_MENU_SUBMENU_SUFFIX(4, "Scaling method:", NULL),
		UI_MENU_SUBMENU_SUFFIX(5, "Aspect mode:", NULL),		
		UI_MENU_ACTION_PREFIX(6, "Horizontal crop: ", native_width_label),		
		UI_MENU_ACTION_PREFIX(7, "Vertical crop:   ", native_height_label),
		UI_MENU_CHECK(8, "Lock aspect mode when cropping:"),
		UI_MENU_SUBMENU_SUFFIX(9, "Horizontal offset: ", NULL),
		UI_MENU_SUBMENU_SUFFIX(10, "Vertical offset: ", NULL),
		UI_MENU_SUBMENU_SUFFIX(11, "Scanline mode:", NULL),
		UI_MENU_CHECK(12, "Hide cursor in fullscreen UI:"),
		UI_MENU_CHECK(13, "Show menu in window mode:"),
		UI_MENU_END
	};

	int option = 0;
	int option2;
	int seltype;
	int prev_value;
	char current_scale[5], trim_value[4], shift_value[4];
	char displaymodename[20];
	int i;

	for (;;) {
		if (rendermode == DIRECTDRAW) {
			for (i = 0; i <= 7; i++) {
				FindMenuItem(menu_array, i)->suffix = "N/A";
			}
		}
		else {
			/*SetDisplayMode(GetActiveDisplayMode());*/
			GetDisplayModeName(displaymodename);
			FindMenuItem(menu_array, 0)->suffix = displaymodename; 
			FindMenuItem(menu_array, 1)->suffix = screen_mode_menu_array[screenmode].item;
			memcpy(current_scale, window_scale_menu_array[(int)((windowscale/100.0f-1)*2)].item, 5);
			current_scale[4] = '\0'; 
			
			FindMenuItem(menu_array, 2)->suffix = current_scale;
			
			if (fsresolution == VGA)
				FindMenuItem(menu_array, 3)->suffix = "VGA";
			else if (fsresolution == SXGA)
				FindMenuItem(menu_array, 3)->suffix = "SXGA";
			else if (fsresolution == UXGA)
				FindMenuItem(menu_array, 3)->suffix = "UXGA";
			else	
				FindMenuItem(menu_array, 3)->suffix = "Desktop";
			
			FindMenuItem(menu_array, 4)->suffix = scaling_method_menu_array[scalingmethod].item;
			
			if (aspectmode == AUTO)
				FindMenuItem(menu_array, 5)->suffix = "Auto";
			else if (aspectmode == WIDE)
				FindMenuItem(menu_array, 5)->suffix = "Wide";
			else if (aspectmode == CROPPED)
				FindMenuItem(menu_array, 5)->suffix = "Cropped";
			else if (aspectmode == COMPRESSED)
				FindMenuItem(menu_array, 5)->suffix = "Compressed";

			sprintf(hcrop_label, "%d", crop.horizontal);
			sprintf(vcrop_label, "%d", crop.vertical);
		    FindMenuItem(menu_array, 6)->suffix = hcrop_label;
			FindMenuItem(menu_array, 7)->suffix = vcrop_label; 
			
			SetItemChecked(menu_array, 8, lockaspect);
			sprintf(hshift_label, "%d", offset.horizontal);
			sprintf(vshift_label, "%d", offset.vertical);
			FindMenuItem(menu_array, 9)->suffix = hshift_label;
			FindMenuItem(menu_array, 10)->suffix = vshift_label;
			
			if (frameparams.scanlinemode == NONE)
				FindMenuItem(menu_array, 11)->suffix = "Off";
			else if (frameparams.scanlinemode == LOW)
				FindMenuItem(menu_array, 11)->suffix = "Low";
			else if (frameparams.scanlinemode == MEDIUM)
				FindMenuItem(menu_array, 11)->suffix = "Medium";
			else if (frameparams.scanlinemode == HIGH)
				FindMenuItem(menu_array, 11)->suffix = "High";
				
			SetItemChecked(menu_array, 12, hidecursor);
			SetItemChecked(menu_array, 13, showmenu);
		}

		option = UI_driver->fSelect("Windows Display Options", 0, option, menu_array, &seltype);
		switch (option) {

		case 0:
			if (rendermode != DIRECTDRAW) {
				prev_value = displaymode;
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, displaymode, display_mode_menu_array, NULL);
				if (option2 >= 0) {
					displaymode = option2;					
					if (prev_value != option2)
						UI_driver->fMessage("Save the config and restart emulator", 1);
				}
			}
			break;
		case 1:
			if (rendermode != DIRECTDRAW) {
				prev_value = screenmode;
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, screenmode, screen_mode_menu_array, NULL);
				if (option2 >= 0)
					if (prev_value != option2)
						togglewindowstate();
			}
			break;
		case 2:
			if (rendermode != DIRECTDRAW) {
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, (int)((windowscale/100.0f-1)*2), window_scale_menu_array, NULL);
				if (option2 >= 0) {					
					changewindowsize(SET, (int)((option2/2.0f+1)*100));
					prev_value = windowscale;
					windowscale = (int)((option2/2.0f+1)*100);
					if (windowscale != prev_value) {
						if (screenmode == WINDOW)
							UI_driver->fMessage("Cannot display at this size", 1);
						else
							UI_driver->fMessage("Cannot preview in fullscreen mode", 1);
					}
				}
			}
			break;
		case 3:
			if (rendermode != DIRECTDRAW) {
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, fsresolution, fsresolution_menu_array, NULL);
				if (option2 >= 0)
					fsresolution = option2;
				if (fsresolution == DESKTOP)
					usecustomfsresolution = FALSE;
				else if (fsresolution == UXGA) {
					usecustomfsresolution = TRUE;
					fullscreenWidth = 1600;
					fullscreenHeight = 1200;
				}
				else if (fsresolution == SXGA) {
					usecustomfsresolution = TRUE;
					fullscreenWidth = 1280;
					fullscreenHeight = 960;
				}
				else if (fsresolution == VGA) {
					usecustomfsresolution = TRUE;
					fullscreenWidth = 640;
					fullscreenHeight = 480;
				}
			}
			break;
		case 4:
			if (rendermode != DIRECTDRAW) {
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, scalingmethod, scaling_method_menu_array, NULL);
				if (option2 >= 0) {
					scalingmethod = option2;
					changewindowsize(RESET, 0);
					refreshframe();
				}
			}
			break;
		case 5:
			if (rendermode != DIRECTDRAW) {
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, aspectmode, aspect_mode_menu_array, NULL);
				if (option2 >= 0) {
					aspectmode = option2;					
					changewindowsize(RESET, 0);
					refreshframe();
					PLATFORM_DisplayScreen(); /* force rebuild of the clipping frame */
					sprintf(native_height_label, "[Height: %d]", frameparams.view.bottom - frameparams.view.top);
					sprintf(native_width_label, "[Width:  %d]", frameparams.view.right - frameparams.view.left);
				}
			}
			break;
		case 6:
			if (rendermode != DIRECTDRAW)  {
				sprintf(trim_value, "%d", crop.horizontal);
				if (UI_driver->fEditString("Enter value", trim_value, sizeof(trim_value))) {
					if (atoi(trim_value) > 150) 
						UI_driver->fMessage("Maximum X-Trim value is 150", 1);
					else if (atoi(trim_value) < -24) 
						UI_driver->fMessage("Minimum X-Trim value is -24", 1);
					else {
						crop.horizontal = atoi(trim_value);	
						changewindowsize(RESET, 0);
						refreshframe();
						PLATFORM_DisplayScreen(); /* force rebuild of the clipping frame */
						sprintf(native_width_label, "[Width:  %d]", frameparams.view.right - frameparams.view.left);
					}
				}
			}
			break;
		case 7:
			if (rendermode != DIRECTDRAW)  {
				sprintf(trim_value, "%d", crop.vertical);
				if (UI_driver->fEditString("Enter value", trim_value, sizeof(trim_value))) {
					if (atoi(trim_value) < 0) 
						UI_driver->fMessage("Minimum Y-Trim value is 0", 1);
					else if (atoi(trim_value) > 108)
						UI_driver->fMessage("Maximum Y-Trim value is 108", 1);
					else {
						crop.vertical = atoi(trim_value);
						changewindowsize(RESET, 0);
						refreshframe();
						PLATFORM_DisplayScreen(); /* force rebuild of the clipping frame */
						sprintf(native_height_label, "[Height: %d]", frameparams.view.bottom - frameparams.view.top);
					}
				}
			}
			break;
		case 8:
			if (rendermode != DIRECTDRAW) {
				lockaspect = !lockaspect;
				changewindowsize(RESET, 0);
				refreshframe();
			}
			break;
		case 9:
			if (rendermode != DIRECTDRAW)  {
				sprintf(shift_value, "%d", offset.horizontal);
				if (UI_driver->fEditString("Enter value", shift_value, sizeof(shift_value))) {
					if (atoi(shift_value) > 24) 
						UI_driver->fMessage("Maximum horizontal offset is 24", 1);
					else if (atoi(shift_value) < -24) 
						UI_driver->fMessage("Minimum horizontal offset is -24", 1);
					else {
						offset.horizontal = atoi(shift_value);						
						changewindowsize(RESET, 0);
						refreshframe();
					}
				}
			}
			break;
		case 10:
			if (rendermode != DIRECTDRAW)  {
				sprintf(shift_value, "%d", offset.vertical);
				if (UI_driver->fEditString("Enter value", shift_value, sizeof(shift_value))) {
					if (atoi(shift_value) > 50) 
						UI_driver->fMessage("Maximum vertical offset is 50", 1);
					else if (atoi(shift_value) < -50) 
						UI_driver->fMessage("Minimum vertical offset is 50", 1);
					else {
						offset.vertical = atoi(shift_value);					
						changewindowsize(RESET, 0);
						refreshframe();
					}
				}
			}
			break;
		case 11:
			if (rendermode != DIRECTDRAW)  {
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, frameparams.scanlinemode, scanline_mode_menu_array, NULL);
				if (option2 >= 0) {
					frameparams.scanlinemode = option2;
					refreshframe();
				}
			}
			break;
		case 12:
			if (rendermode != DIRECTDRAW) {
				hidecursor = !hidecursor;
				setcursor();
			}
			break;
		case 13:
			if (rendermode != DIRECTDRAW) {
				togglemenustate();
			}
			break;
			
		default:
			return;
		}
	}
}
#endif /* DIRECTX */

#endif /* CURSES_BASIC */

#ifndef USE_CURSES

#ifdef SDL
static char joys[2][5][16];
static const UI_tMenuItem joy0_menu_array[] = {
	UI_MENU_LABEL("Select joy direction"),
	UI_MENU_LABEL("\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022"),
	UI_MENU_SUBMENU_SUFFIX(0, "Left   : ", joys[0][0]),
	UI_MENU_SUBMENU_SUFFIX(1, "Up     : ", joys[0][1]),
	UI_MENU_SUBMENU_SUFFIX(2, "Right  : ", joys[0][2]),
	UI_MENU_SUBMENU_SUFFIX(3, "Down   : ", joys[0][3]),
	UI_MENU_SUBMENU_SUFFIX(4, "Trigger: ", joys[0][4]),
	UI_MENU_END
};
static const UI_tMenuItem joy1_menu_array[] = {
	UI_MENU_LABEL("Select joy direction"),
	UI_MENU_LABEL("\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022"),
	UI_MENU_SUBMENU_SUFFIX(0, "Left   : ", joys[1][0]),
	UI_MENU_SUBMENU_SUFFIX(1, "Up     : ", joys[1][1]),
	UI_MENU_SUBMENU_SUFFIX(2, "Right  : ", joys[1][2]),
	UI_MENU_SUBMENU_SUFFIX(3, "Down   : ", joys[1][3]),
	UI_MENU_SUBMENU_SUFFIX(4, "Trigger: ", joys[1][4]),
	UI_MENU_END
};

static void KeyboardJoystickConfiguration(int joystick)
{
	char title[40];
	int option2 = 0;
#ifdef HAVE_SNPRINTF
	snprintf(title, sizeof(title), "Define keys for joystick %d", joystick);
#else
	sprintf(title, "Define keys for joystick %d", joystick);
#endif
	for(;;) {
		int j0d;
		for(j0d = 0; j0d <= 4; j0d++)
			PLATFORM_GetJoystickKeyName(joystick, j0d, joys[joystick][j0d], sizeof(joys[joystick][j0d]));
		option2 = UI_driver->fSelect(title, UI_SELECT_POPUP, option2, joystick == 0 ? joy0_menu_array : joy1_menu_array, NULL);
		if (option2 >= 0 && option2 <= 4) {
			PLATFORM_SetJoystickKey(joystick, option2, GetRawKey());
		}
		if (option2 < 0) break;
		if (++option2 > 4) option2 = 0;
	}
}
#endif

#ifdef DIRECTX
static char buttons[9][2][16];
static const UI_tMenuItem joy0_menu_array[] = {
	UI_MENU_LABEL("Select button to map"),
	UI_MENU_LABEL("\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022"),
	UI_MENU_SUBMENU_SUFFIX(0, "Button 2  : ", buttons[0][0]),
	UI_MENU_SUBMENU_SUFFIX(1, "Button 3  : ", buttons[0][1]),
	UI_MENU_SUBMENU_SUFFIX(2, "Button 4  : ", buttons[0][2]),
	UI_MENU_SUBMENU_SUFFIX(3, "Button 5  : ", buttons[0][3]),
	UI_MENU_SUBMENU_SUFFIX(4, "Button 6  : ", buttons[0][4]),
	UI_MENU_SUBMENU_SUFFIX(5, "Button 7  : ", buttons[0][5]),
	UI_MENU_SUBMENU_SUFFIX(6, "Button 8  : ", buttons[0][6]),
	UI_MENU_SUBMENU_SUFFIX(7, "Button 9  : ", buttons[0][7]),
	UI_MENU_SUBMENU_SUFFIX(8, "Button 10 : ", buttons[0][8]),
	UI_MENU_END
};
static const UI_tMenuItem joy1_menu_array[] = {
	UI_MENU_LABEL("Select button to map"),
	UI_MENU_LABEL("\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022"),
	UI_MENU_SUBMENU_SUFFIX(0, "Button 2  : ", buttons[1][0]),
	UI_MENU_SUBMENU_SUFFIX(1, "Button 3  : ", buttons[1][1]),
	UI_MENU_SUBMENU_SUFFIX(2, "Button 4  : ", buttons[1][2]),
	UI_MENU_SUBMENU_SUFFIX(3, "Button 5  : ", buttons[1][3]),
	UI_MENU_SUBMENU_SUFFIX(4, "Button 6  : ", buttons[1][4]),
	UI_MENU_SUBMENU_SUFFIX(5, "Button 7  : ", buttons[1][5]),
	UI_MENU_SUBMENU_SUFFIX(6, "Button 8  : ", buttons[1][6]),
	UI_MENU_SUBMENU_SUFFIX(7, "Button 9  : ", buttons[1][7]),
	UI_MENU_SUBMENU_SUFFIX(8, "Button 10 : ", buttons[1][8]),
	UI_MENU_END
};

static void ConfigureControllerButtons(int stick)
{
	int i;
	char title[40];
	int option2 = 0;
	
	sprintf(title, "Define keys for controller %d", stick + 1);
	for(;;) {
		for(i = 0; i <= 8; i++) 
			PLATFORM_GetButtonAssignments(stick, i, buttons[stick][i], sizeof(buttons[stick][i]));
		option2 = UI_driver->fSelect(title, UI_SELECT_POPUP, option2, stick == 0 ? joy0_menu_array : joy1_menu_array, NULL);
		if (option2 >= 0 && option2 <= 8)
			PLATFORM_SetButtonAssignment(stick, option2, GetKeyName());
		if (option2 < 0) break;
		if (++option2 > 8) option2 = 0;
	}
}
#endif

static void ControllerConfiguration(void)
{
#ifdef DIRECTX
	static const UI_tMenuItem keyboard_joystick_mode_array[] = {
		UI_MENU_ACTION(0, "Keypad"),
		UI_MENU_ACTION(1, "Keypad+"),
		UI_MENU_ACTION(2, "Arrows"),
		UI_MENU_END
	};
	
	static const UI_tMenuItem alternate_joystick_mode_array[] = {
		UI_MENU_ACTION(0, "Normal"),
		UI_MENU_ACTION(1, "Dual"),
		UI_MENU_ACTION(2, "Shared"),
		UI_MENU_END
	};
#endif

#if !defined(_WIN32_WCE) && !defined(DREAMCAST)
	static const UI_tMenuItem mouse_mode_menu_array[] = {
		UI_MENU_ACTION(0, "None"),
		UI_MENU_ACTION(1, "Paddles"),
		UI_MENU_ACTION(2, "Touch tablet"),
		UI_MENU_ACTION(3, "Koala pad"),
		UI_MENU_ACTION(4, "Light pen"),
		UI_MENU_ACTION(5, "Light gun"),
		UI_MENU_ACTION(6, "Amiga mouse"),
		UI_MENU_ACTION(7, "ST mouse"),
		UI_MENU_ACTION(8, "Trak-ball"),
		UI_MENU_ACTION(9, "Joystick"),
		UI_MENU_END
	};
	static char mouse_port_status[2] = { '1', '\0' };
	static char mouse_speed_status[2] = { '1', '\0' };
#endif
	static UI_tMenuItem menu_array[] = {
		UI_MENU_ACTION(0, "Joystick autofire:"),
		UI_MENU_CHECK(1, "Enable MultiJoy4:"),
#if defined(_WIN32_WCE)
		UI_MENU_CHECK(5, "Virtual joystick:"),
#elif defined(DREAMCAST)
		UI_MENU_CHECK(9, "Emulate Paddles:"),
		UI_MENU_ACTION(10, "Button configuration"),
#else
		UI_MENU_SUBMENU_SUFFIX(2, "Mouse device: ", NULL),
		UI_MENU_SUBMENU_SUFFIX(3, "Mouse port:", mouse_port_status),
		UI_MENU_SUBMENU_SUFFIX(4, "Mouse speed:", mouse_speed_status),
#endif
#ifdef SDL
		UI_MENU_CHECK(5, "Enable keyboard joystick 1:"),
		UI_MENU_SUBMENU(6, "Define layout of keyboard joystick 1"),
		UI_MENU_CHECK(7, "Enable keyboard joystick 2:"),
		UI_MENU_SUBMENU(8, "Define layout of keyboard joystick 2"),
#endif
#ifdef DIRECTX
		UI_MENU_SUBMENU_SUFFIX(5, "Keyboard joystick mode: ", NULL),
		UI_MENU_SUBMENU_SUFFIX(6, "Alternate joystick mode: ", NULL),
		UI_MENU_CHECK(7, "Enable custom buttons (joy 1):"),
		UI_MENU_SUBMENU(8, "Assign custom buttons (joy 1):"),
		UI_MENU_CHECK(9, "Enable custom buttons (joy 2):"),
		UI_MENU_SUBMENU(10, "Assign custom buttons (joy 2):"),
#endif
		UI_MENU_END
	};

	int option = 0;
#if !defined(_WIN32_WCE) && !defined(DREAMCAST)
	int option2;
#endif
#ifdef DIRECTX
    int prev_option;
#endif
	for (;;) {
		menu_array[0].suffix = INPUT_joy_autofire[0] == INPUT_AUTOFIRE_FIRE ? "Fire"
		                     : INPUT_joy_autofire[0] == INPUT_AUTOFIRE_CONT ? "Always"
		                     : "No ";
		SetItemChecked(menu_array, 1, INPUT_joy_multijoy);
#if defined(_WIN32_WCE)
		/* XXX: not on smartphones? */
		SetItemChecked(menu_array, 5, virtual_joystick);
#elif defined(DREAMCAST)
		SetItemChecked(menu_array, 9, emulate_paddles);
#else
		menu_array[2].suffix = mouse_mode_menu_array[INPUT_mouse_mode].item;
		mouse_port_status[0] = (char) ('1' + INPUT_mouse_port);
		mouse_speed_status[0] = (char) ('0' + INPUT_mouse_speed);
#endif
#ifdef SDL
		SetItemChecked(menu_array, 5, PLATFORM_kbd_joy_0_enabled);
		SetItemChecked(menu_array, 7, PLATFORM_kbd_joy_1_enabled);
#endif
#ifdef DIRECTX
		menu_array[5].suffix = keyboard_joystick_mode_array[keyboardJoystickMode].item;
		menu_array[6].suffix = alternate_joystick_mode_array[alternateJoystickMode].item;
		SetItemChecked(menu_array, 7, mapController1Buttons);
		SetItemChecked(menu_array, 9, mapController2Buttons);
#endif
		option = UI_driver->fSelect("Controller Configuration", 0, option, menu_array, NULL);
		switch (option) {
		case 0:
			switch (INPUT_joy_autofire[0]) {
			case INPUT_AUTOFIRE_FIRE:
				INPUT_joy_autofire[0] = INPUT_joy_autofire[1] = INPUT_joy_autofire[2] = INPUT_joy_autofire[3] = INPUT_AUTOFIRE_CONT;
				break;
			case INPUT_AUTOFIRE_CONT:
				INPUT_joy_autofire[0] = INPUT_joy_autofire[1] = INPUT_joy_autofire[2] = INPUT_joy_autofire[3] = INPUT_AUTOFIRE_OFF;
				break;
			default:
				INPUT_joy_autofire[0] = INPUT_joy_autofire[1] = INPUT_joy_autofire[2] = INPUT_joy_autofire[3] = INPUT_AUTOFIRE_FIRE;
				break;
			}
			break;
		case 1:
			INPUT_joy_multijoy = !INPUT_joy_multijoy;
			break;
#if defined(_WIN32_WCE)
		case 5:
			virtual_joystick = !virtual_joystick;
			break;
#elif defined(DREAMCAST)
		case 9:
			emulate_paddles = !emulate_paddles;
			break;
		case 10:
			ButtonConfiguration();
			break;
#else
		case 2:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, INPUT_mouse_mode, mouse_mode_menu_array, NULL);
			if (option2 >= 0)
				INPUT_mouse_mode = option2;
			break;
		case 3:
			INPUT_mouse_port = UI_driver->fSelectInt(INPUT_mouse_port + 1, 1, 4) - 1;
			break;
		case 4:
			INPUT_mouse_speed = UI_driver->fSelectInt(INPUT_mouse_speed, 1, 9);
			break;
#endif
#ifdef SDL
		case 5:
			PLATFORM_kbd_joy_0_enabled = !PLATFORM_kbd_joy_0_enabled;
			break;
		case 6:
			KeyboardJoystickConfiguration(0);
			break;
		case 7:
			PLATFORM_kbd_joy_1_enabled = !PLATFORM_kbd_joy_1_enabled;
			break;
		case 8:
			KeyboardJoystickConfiguration(1);
			break;
#endif
#ifdef DIRECTX
		case 5:
		    prev_option = keyboardJoystickMode;
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, keyboardJoystickMode, keyboard_joystick_mode_array, NULL);
			if (option2 >= 0)
				keyboardJoystickMode = option2;
			break;
		case 6:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, alternateJoystickMode, alternate_joystick_mode_array, NULL);
			if (option2 >= 0)
				alternateJoystickMode = option2;
			break;
		case 7:
			mapController1Buttons = !mapController1Buttons;
			break;
		case 8:
			ConfigureControllerButtons(0);
			break;
		case 9:
			mapController2Buttons = !mapController2Buttons;
			break;
		case 10:
			ConfigureControllerButtons(1);
			break;
#endif
		default:
			return;
		}
	}
}

#endif /* USE_CURSES */

#ifdef SOUND

static int SoundSettings(void)
{
	static UI_tMenuItem menu_array[] = {
		/* XXX: don't allow on smartphones? */
#ifndef SYNCHRONIZED_SOUND
		UI_MENU_CHECK(0, "High Fidelity POKEY:"),
#endif
#ifdef STEREO_SOUND
		UI_MENU_CHECK(1, "Dual POKEY (Stereo):"),
#endif
#ifdef CONSOLE_SOUND
		UI_MENU_CHECK(2, "Speaker (Key Click):"),
#endif
#ifdef SERIO_SOUND
		UI_MENU_CHECK(3, "Serial IO Sound:"),
#endif
#ifndef SYNCHRONIZED_SOUND
		UI_MENU_ACTION(4, "Enable higher frequencies:"),
#endif
#ifdef DREAMCAST
		UI_MENU_CHECK(5, "Enable sound:"),
#endif
		UI_MENU_END
	};

	int option = 0;

	for (;;) {
#ifndef SYNCHRONIZED_SOUND
		SetItemChecked(menu_array, 0, POKEYSND_enable_new_pokey);
#endif
#ifdef STEREO_SOUND
		SetItemChecked(menu_array, 1, POKEYSND_stereo_enabled);
#endif
#ifdef CONSOLE_SOUND
		SetItemChecked(menu_array, 2, POKEYSND_console_sound_enabled);
#endif
#ifdef SERIO_SOUND
		SetItemChecked(menu_array, 3, POKEYSND_serio_sound_enabled);
#endif
#ifndef SYNCHRONIZED_SOUND
		FindMenuItem(menu_array, 4)->suffix = POKEYSND_enable_new_pokey ? "N/A" : POKEYSND_bienias_fix ? "Yes" : "No ";
#endif
#ifdef DREAMCAST
		SetItemChecked(menu_array, 5, glob_snd_ena);
#endif

#if 0
		option = UI_driver->fSelect(NULL, UI_SELECT_POPUP, option, menu_array, NULL);
#else
		option = UI_driver->fSelect("Sound Settings", 0, option, menu_array, NULL);
#endif

		switch (option) {
#ifndef SYNCHRONIZED_SOUND
		case 0:
			POKEYSND_enable_new_pokey = !POKEYSND_enable_new_pokey;
			POKEYSND_DoInit();
			/* According to the PokeySnd doc the POKEY switch can occur on
			   a cold-restart only */
			UI_driver->fMessage("Will reboot to apply the change", 1);
			return TRUE; /* reboot required */
#endif
#ifdef STEREO_SOUND
		case 1:
			POKEYSND_stereo_enabled = !POKEYSND_stereo_enabled;
#ifdef SUPPORTS_SOUND_REINIT
			Sound_Reinit();
#endif
			break;
#endif
#ifdef CONSOLE_SOUND
		case 2:
			POKEYSND_console_sound_enabled = !POKEYSND_console_sound_enabled;
			break;
#endif
#ifdef SERIO_SOUND
		case 3:
			POKEYSND_serio_sound_enabled = !POKEYSND_serio_sound_enabled;
			break;
#endif
#ifndef SYNCHRONIZED_SOUND
		case 4:
			if (! POKEYSND_enable_new_pokey) POKEYSND_bienias_fix = !POKEYSND_bienias_fix;
			break;
#endif
#ifdef DREAMCAST
		case 5:
			glob_snd_ena = !glob_snd_ena;
			break;
#endif
		default:
			return FALSE;
		}
	}
}

#endif /* SOUND */

#if !defined(CURSES_BASIC) && !defined(DREAMCAST)

#ifdef USE_CURSES
extern void curses_clear_screen(void);
#endif

static void Screenshot(int interlaced)
{
	static char filename[FILENAME_MAX] = "";
	if (UI_driver->fGetSaveFilename(filename, UI_saved_files_dir, UI_n_saved_files_dir)) {
#ifdef USE_CURSES
		/* must clear, otherwise in case of a failure we'll see parts
		   of Atari screen on top of UI screen */
		curses_clear_screen();
#endif
		ANTIC_Frame(TRUE);
		if (!Screen_SaveScreenshot(filename, interlaced))
			CantSave(filename);
	}
}

#endif /* !defined(CURSES_BASIC) && !defined(DREAMCAST) */

static void AboutEmulator(void)
{
	UI_driver->fInfoScreen("About the Emulator",
		Atari800_TITLE "\0"
		"Copyright (c) 1995-1998 David Firth\0"
		"and\0"
		"(c)1998-2010 Atari800 Development Team\0"
		"See CREDITS file for details.\0"
		"http://atari800.atari.org/\0"
		"\0"
		"\0"
		"\0"
		"This program is free software; you can\0"
		"redistribute it and/or modify it under\0"
		"the terms of the GNU General Public\0"
		"License as published by the Free\0"
		"Software Foundation; either version 2,\0"
		"or (at your option) any later version.\0"
		"\n");
}

#ifdef DIRECTX
static void FunctionKeyHelp(void)
{
	UI_driver->fInfoScreen("Function Key List",
		Atari800_TITLE "\0"
		"\0"
		"Function Key Assignments   \0"
		"------------------------   \0"
		"\0"
		"F1  - User Interface       \0"
		"F2  - Option key           \0"
		"F3  - Select key           \0"
		"F4  - Start key            \0"
		"F5  - Reset key            \0"
		"F6  - Help key (XL/XE only)\0"
		"F7  - Break key            \0"
		"F8  - Enter monitor        \0"
		"      (-console required)  \0"
		"F9  - Exit emulator        \0"
		"F10 - Save screenshot      \0"
		"\n");
}

static void HotKeyHelp(void)
{
	UI_driver->fInfoScreen("Hot Key List",
		Atari800_TITLE "\0"
		"\0"
		"Hot Key Assignments \0"
		"------------------- \0"
		"\0"
		"Alt+Enter - Toggle Fullscreen/Window\0"
		"Alt+PgUp  - Increase window size    \0"
		"Alt+PgDn  - Decrease window size    \0"
		"Alt+I     - Next scanline mode      \0"
		"Alt+M     - Hide/Show main menu     \0"
		"Alt+T     - 3D Tilt                 \0"
		"            (Direct3D modes only)   \0"
		"Alt+Z     - 3D Screensaver          \0"
		"            (Direct3D modes only)   \0"
		"\n");
}
#endif

void UI_Run(void)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL_ACCEL(UI_MENU_RUN, "Run Atari Program", "Alt+R"),
		UI_MENU_SUBMENU_ACCEL(UI_MENU_DISK, "Disk Management", "Alt+D"),
		UI_MENU_SUBMENU_ACCEL(UI_MENU_CARTRIDGE, "Cartridge Management", "Alt+C"),
		UI_MENU_FILESEL(UI_MENU_CASSETTE, "Select Tape Image"),
		UI_MENU_SUBMENU_ACCEL(UI_MENU_SYSTEM, "Select System", "Alt+Y"),
#ifdef SOUND
		UI_MENU_SUBMENU_ACCEL(UI_MENU_SOUND, "Sound Settings", "Alt+O"),
#ifndef DREAMCAST
		UI_MENU_ACTION_ACCEL(UI_MENU_SOUND_RECORDING, "Sound Recording Start/Stop", "Alt+W"),
#endif
#endif
#ifndef CURSES_BASIC
		UI_MENU_SUBMENU(UI_MENU_DISPLAY, "Display Settings"),
#endif
#ifdef DIRECTX
		UI_MENU_SUBMENU(UI_MENU_WINDOWS, "Windows Display Options"),
#endif
#ifndef USE_CURSES
		UI_MENU_SUBMENU(UI_MENU_CONTROLLER, "Controller Configuration"),
#endif
		UI_MENU_SUBMENU(UI_MENU_SETTINGS, "Emulator Configuration"),
		UI_MENU_FILESEL_ACCEL(UI_MENU_SAVESTATE, "Save State", "Alt+S"),
		UI_MENU_FILESEL_ACCEL(UI_MENU_LOADSTATE, "Load State", "Alt+L"),
#if !defined(CURSES_BASIC) && !defined(DREAMCAST)
#ifdef HAVE_LIBPNG
		UI_MENU_FILESEL_ACCEL(UI_MENU_PCX, "Save Screenshot", "F10"),
		/* there isn't enough space for "PNG/PCX Interlaced Screenshot Shift+F10" */
		UI_MENU_FILESEL_ACCEL(UI_MENU_PCXI, "Save Interlaced Screenshot", "Shift+F10"),
#else
		UI_MENU_FILESEL_ACCEL(UI_MENU_PCX, "PCX Screenshot", "F10"),
		UI_MENU_FILESEL_ACCEL(UI_MENU_PCXI, "PCX Interlaced Screenshot", "Shift+F10"),
#endif
#endif
		UI_MENU_ACTION_ACCEL(UI_MENU_BACK, "Back to Emulated Atari", "Esc"),
		UI_MENU_ACTION_ACCEL(UI_MENU_RESETW, "Reset (Warm Start)", "F5"),
		UI_MENU_ACTION_ACCEL(UI_MENU_RESETC, "Reboot (Cold Start)", "Shift+F5"),
#if defined(_WIN32_WCE)
		UI_MENU_ACTION(UI_MENU_MONITOR, "About Pocket Atari"),
#elif defined(DREAMCAST)
		UI_MENU_ACTION(UI_MENU_MONITOR, "About AtariDC"),
#elif defined(DIRECTX)
		UI_MENU_ACTION_ACCEL(UI_MENU_MONITOR, monitor_label, "F8"),
#else
		UI_MENU_ACTION_ACCEL(UI_MENU_MONITOR, "Enter Monitor", "F8"),
#endif
		UI_MENU_ACTION_ACCEL(UI_MENU_ABOUT, "About the Emulator", "Alt+A"),
		UI_MENU_ACTION_ACCEL(UI_MENU_EXIT, "Exit Emulator", "F9"),
		UI_MENU_END
	};

	int option = UI_MENU_RUN;
	int done = FALSE;
#if defined(XEP80_EMULATION) || defined(AF80) || defined(PBI_PROTO80)
	saved_show_80 = PLATFORM_show_80;
	if (PLATFORM_show_80) {
		PLATFORM_Switch80();
	}
#endif

	UI_is_active = TRUE;

#ifdef DIRECTX
	setcursor();
	sprintf(desktopreslabel, "Desktop [%dx%d]", origScreenWidth, origScreenHeight);
	sprintf(hcrop_label, "%d", crop.horizontal);
	sprintf(vcrop_label, "%d", crop.vertical);
	sprintf(hshift_label, "%d", offset.horizontal);
	sprintf(vshift_label, "%d", offset.vertical);
	sprintf(native_width_label, "[Width:  %d]", frameparams.view.right - frameparams.view.left);
	sprintf(native_height_label, "[Height: %d]", frameparams.view.bottom - frameparams.view.top);
	if (useconsole)
		strcpy(monitor_label, "Enter Monitor");
	else
		strcpy(monitor_label, "Enter Monitor (need -console)"); 
#endif
	

	/* Sound_Active(FALSE); */
	UI_driver->fInit();

#ifdef CRASH_MENU
	if (UI_crash_code >= 0) {
		done = CrashMenu();
		UI_crash_code = -1;
	}
#endif

	while (!done) {
		
		if (UI_alt_function != -1)
			UI_current_function = UI_alt_function;		
		if (UI_alt_function < 0)
			option = UI_driver->fSelect(Atari800_TITLE, 0, option, menu_array, NULL);
		if (UI_alt_function >= 0) {
			option = UI_alt_function;
			UI_alt_function = -1;
			done = TRUE;
		}

		switch (option) {
		case -2:
		case -1:				/* ESC key */
			done = TRUE;
			break;
		case UI_MENU_DISK:
			DiskManagement();
			break;
		case UI_MENU_CARTRIDGE:
			CartManagement();
			break;
		case UI_MENU_RUN:
			/* if (RunExe()) */
			if (AutostartFile())
				done = TRUE;	/* reboot immediately */
			break;
		case UI_MENU_CASSETTE:
			LoadTape();
			break;
		case UI_MENU_SYSTEM:
			SelectSystem();
			break;
		case UI_MENU_SETTINGS:
			AtariSettings();
			break;
#ifdef SOUND
		case UI_MENU_SOUND:
			if (SoundSettings()) {
				Atari800_Coldstart();
				done = TRUE;	/* reboot immediately */
			}
			break;
#ifndef DREAMCAST
		case UI_MENU_SOUND_RECORDING:
			SoundRecording();
			break;
#endif
#endif
		case UI_MENU_SAVESTATE:
			SaveState();
			break;
		case UI_MENU_LOADSTATE:
			/* Note: AutostartFile() handles state files, too,
			   so we can remove LoadState() now. */
			LoadState();
			break;
#ifndef CURSES_BASIC
		case UI_MENU_DISPLAY:
			DisplaySettings();
			break;
#ifndef DREAMCAST
		case UI_MENU_PCX:
			Screenshot(FALSE);
			break;
		case UI_MENU_PCXI:
			Screenshot(TRUE);
			break;
#endif
#endif
#ifdef DIRECTX
		case UI_MENU_WINDOWS:
			WindowsOptions();
			break;
		case UI_MENU_SAVE_CONFIG:
			CFG_WriteConfig();
			return;
#endif
#ifndef USE_CURSES
		case UI_MENU_CONTROLLER:
			ControllerConfiguration();
			break;
#endif
		case UI_MENU_BACK:
			done = TRUE;		/* back to emulator */
			break;
		case UI_MENU_RESETW:
			Atari800_Warmstart();
			done = TRUE;		/* reboot immediately */
			break;
		case UI_MENU_RESETC:
			Atari800_Coldstart();
			done = TRUE;		/* reboot immediately */
			break;
		case UI_MENU_ABOUT:
			AboutEmulator();
			break;
#ifdef DIRECTX
		case UI_MENU_FUNCT_KEY_HELP:
			FunctionKeyHelp();
			break;
		case UI_MENU_HOT_KEY_HELP:
			HotKeyHelp();
			break;
#endif
		case UI_MENU_MONITOR:
#if defined(_WIN32_WCE)
			AboutPocketAtari();
			break;
#elif defined(DREAMCAST)
			AboutAtariDC();
			break;
#elif defined(DIRECTX)
			if (useconsole) {
				if (PLATFORM_Exit(TRUE)) 
					done = TRUE;
					break;
			}
			else {
				UI_driver->fMessage("Console required for monitor", 1);
				break;
			}				
#else
			if (PLATFORM_Exit(TRUE)) {
				done = TRUE;
				break;
			}
			/* if 'quit' typed in monitor, exit emulator */
#endif
		case UI_MENU_EXIT:
			Atari800_Exit(FALSE);
			exit(0);
		}
	}

	/* Sound_Active(TRUE); */
	UI_is_active = FALSE;
#ifdef DIRECTX
	setcursor();
#endif
	
	/* flush keypresses */
	while (PLATFORM_Keyboard() != AKEY_NONE)
		Atari800_Sync();

	UI_alt_function = UI_current_function = -1;
	/* restore 80 column screen */
#if defined(XEP80_EMULATION) || defined(AF80) || defined(PBI_PROTO80)
	if (saved_show_80 != PLATFORM_show_80) {
		PLATFORM_Switch80();
	}
#endif
}


#ifdef CRASH_MENU

int CrashMenu(void)
{
	static char cim_info[42];
	static UI_tMenuItem menu_array[] = {
		UI_MENU_LABEL(cim_info),
		UI_MENU_ACTION_ACCEL(0, "Reset (Warm Start)", "F5"),
		UI_MENU_ACTION_ACCEL(1, "Reboot (Cold Start)", "Shift+F5"),
		UI_MENU_ACTION_ACCEL(2, "Menu", "F1"),
#if !defined(_WIN32_WCE) && !defined(DREAMCAST) && !defined(DIRECTX)
		UI_MENU_ACTION_ACCEL(3, "Enter Monitor", "F8"),
#endif
#ifdef DIRECTX
		UI_MENU_ACTION_ACCEL(3, monitor_label, "F8"),
#endif
		UI_MENU_ACTION_ACCEL(4, "Continue After CIM", "Esc"),
		UI_MENU_ACTION_ACCEL(5, "Exit Emulator", "F9"),
		UI_MENU_END
	};

	int option = 0;
	sprintf(cim_info, "Code $%02X (CIM) at address $%04X", UI_crash_code, UI_crash_address);

	for (;;) {
		option = UI_driver->fSelect("!!! The Atari computer has crashed !!!", 0, option, menu_array, NULL);

		if (UI_alt_function >= 0) /* pressed F5, Shift+F5 or F9 */
			return FALSE;

		switch (option) {
		case 0:				/* Power On Reset */
			UI_alt_function = UI_MENU_RESETW;
			return FALSE;
		case 1:				/* Power Off Reset */
			UI_alt_function = UI_MENU_RESETC;
			return FALSE;
		case 2:				/* Menu */
			return FALSE;
#if !defined(_WIN32_WCE) && !defined(DREAMCAST)
		case 3:				/* Monitor */
			UI_alt_function = UI_MENU_MONITOR;
			return FALSE;
#endif
		case -2:
		case -1:			/* ESC key */
		case 4:				/* Continue after CIM */
			CPU_regPC = UI_crash_afterCIM;
			return TRUE;
		case 5:				/* Exit */
			UI_alt_function = UI_MENU_EXIT;
			return FALSE;
		}
	}
}
#endif

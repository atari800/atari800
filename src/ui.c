/*
 * ui.c - main user interface
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
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
#include <string.h>
#include <stdlib.h> /* free() */

#include "antic.h"
#include "atari.h"
#include "binload.h"
#include "cartridge.h"
#include "cassette.h"
#include "cpu.h"
#include "devices.h" /* Device_SetPrintCommand */
#include "gtia.h"
#include "input.h"
#include "log.h"
#include "memory.h"
#include "platform.h"
#include "rtime.h"
#include "screen.h"
#include "sio.h"
#include "statesav.h"
#include "ui.h"
#include "util.h"
#ifdef SOUND
#include "pokeysnd.h"
#include "sndsave.h"
#endif

#ifdef _WIN32_WCE
extern int smooth_filter;
extern int filter_available;
extern int virtual_joystick;
void AboutPocketAtari(void);
#endif

tUIDriver *ui_driver = &basic_ui_driver;

int ui_is_active = FALSE;
int alt_function = -1;

#ifdef CRASH_MENU
int crash_code = -1;
UWORD crash_address;
UWORD crash_afterCIM;
int CrashMenu();
#endif

char atari_files_dir[MAX_DIRECTORIES][FILENAME_MAX];
char saved_files_dir[MAX_DIRECTORIES][FILENAME_MAX];
int n_atari_files_dir = 0;
int n_saved_files_dir = 0;

static void SetItemChecked(tMenuItem *mip, int checked)
{
	mip->flags = checked ? (ITEM_CHECK | ITEM_CHECKED) : ITEM_CHECK;
}

static void FilenameMessage(const char *format, const char *filename)
{
	char msg[FILENAME_MAX + 30];
	sprintf(msg, format, filename);
	ui_driver->fMessage(msg);
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

	static tMenuItem menu_array[] = {
		MENU_ACTION(0, "Atari OS/A (16 KB)"),
		MENU_ACTION(1, "Atari OS/A (48 KB)"),
		MENU_ACTION(2, "Atari OS/A (52 KB)"),
		MENU_ACTION(3, "Atari OS/B (16 KB)"),
		MENU_ACTION(4, "Atari OS/B (48 KB)"),
		MENU_ACTION(5, "Atari OS/B (52 KB)"),
		MENU_ACTION(6, "Atari 600XL (16 KB)"),
		MENU_ACTION(7, "Atari 800XL (64 KB)"),
		MENU_ACTION(8, "Atari 130XE (128 KB)"),
		MENU_ACTION(9, "Atari 320XE (320 KB RAMBO)"),
		MENU_ACTION(10, "Atari 320XE (320 KB COMPY SHOP)"),
		MENU_ACTION(11, "Atari 576XE (576 KB)"),
		MENU_ACTION(12, "Atari 1088XE (1088 KB)"),
		MENU_ACTION(13, "Atari 5200 (16 KB)"),
		MENU_ACTION(14, "Video system:"),
		MENU_END
	};

	static const tSysConfig machine[] = {
		{ MACHINE_OSA, 16 },
		{ MACHINE_OSA, 48 },
		{ MACHINE_OSA, 52 },
		{ MACHINE_OSB, 16 },
		{ MACHINE_OSB, 48 },
		{ MACHINE_OSB, 52 },
		{ MACHINE_XLXE, 16 },
		{ MACHINE_XLXE, 64 },
		{ MACHINE_XLXE, 128 },
		{ MACHINE_XLXE, RAM_320_RAMBO },
		{ MACHINE_XLXE, RAM_320_COMPY_SHOP },
		{ MACHINE_XLXE, 576 },
		{ MACHINE_XLXE, 1088 },
		{ MACHINE_5200, 16 }
	};

	int option = 0;
	int old_tv_mode = tv_mode;

	int i;
	for (i = 0; i < sizeof(machine) / sizeof(machine[0]); i++)
		if (machine_type == machine[i].type && ram_size == machine[i].ram) {
			option = i;
			break;
		}

	for (;;) {
		menu_array[14].suffix = (tv_mode == TV_PAL) ? "PAL" : "NTSC";
		option = ui_driver->fSelect("Select System", 0, option, menu_array, NULL);
		if (option == 14) {
			tv_mode = (tv_mode == TV_PAL) ? TV_NTSC : TV_PAL;
			continue;
		}
		break;
	}
	if (option >= 0) {
		machine_type = machine[option].type;
		ram_size = machine[option].ram;
		Atari800_InitialiseMachine();
	}
	else if (tv_mode != old_tv_mode)
		Atari800_InitialiseMachine(); /* XXX: Coldstart() is probably enough */
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
	struct ATR_Header hdr;
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

	static tMenuItem menu_array[] = {
		MENU_FILESEL_PREFIX_TIP(0, drive_array[0], sio_filename[0], NULL),
		MENU_FILESEL_PREFIX_TIP(1, drive_array[1], sio_filename[1], NULL),
		MENU_FILESEL_PREFIX_TIP(2, drive_array[2], sio_filename[2], NULL),
		MENU_FILESEL_PREFIX_TIP(3, drive_array[3], sio_filename[3], NULL),
		MENU_FILESEL_PREFIX_TIP(4, drive_array[4], sio_filename[4], NULL),
		MENU_FILESEL_PREFIX_TIP(5, drive_array[5], sio_filename[5], NULL),
		MENU_FILESEL_PREFIX_TIP(6, drive_array[6], sio_filename[6], NULL),
		MENU_FILESEL_PREFIX_TIP(7, drive_array[7], sio_filename[7], NULL),
		MENU_FILESEL(8, "Save Disk Set"),
		MENU_FILESEL(9, "Load Disk Set"),
		MENU_ACTION(10, "Rotate Disks"),
		MENU_FILESEL(11, "Make Blank ATR Disk"),
		MENU_END
	};

	int dsknum = 0;

	for (;;) {
		static char disk_filename[FILENAME_MAX] = "";
		static char set_filename[FILENAME_MAX] = "";
		int i;
		int seltype;

		for (i = 0; i < 8; i++) {
			drive_array[i][0] = ' ';
			switch (drive_status[i]) {
			case Off:
				menu_array[i].suffix = "Return:insert";
				break;
			case NoDisk:
				menu_array[i].suffix = "Return:insert Backspace:off";
				break;
			case ReadOnly:
				drive_array[i][0] = '*';
				/* FALLTHROUGH */
			default:
				menu_array[i].suffix = "Ret:insert Bksp:eject Space:read-only";
				break;
			}
		}

		dsknum = ui_driver->fSelect("Disk Management", 0, dsknum, menu_array, &seltype);

		switch (dsknum) {
		case 8:
			if (ui_driver->fGetSaveFilename(set_filename, saved_files_dir, n_saved_files_dir)) {
				FILE *fp = fopen(set_filename, "w");
				if (fp == NULL) {
					CantSave(set_filename);
					break;
				}
				for (i = 0; i < 8; i++)
					fprintf(fp, "%s\n", sio_filename[i]);
				fclose(fp);
				Created(set_filename);
			}
			break;
		case 9:
			if (ui_driver->fGetLoadFilename(set_filename, saved_files_dir, n_saved_files_dir)) {
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
			Rotate_Disks();
			break;
		case 11:
			if (ui_driver->fGetSaveFilename(disk_filename, atari_files_dir, n_atari_files_dir)) {
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
		default:
			if (dsknum < 0)
				return;
			/* dsknum = 0..7 */
			switch (seltype) {
			case USER_SELECT: /* "Enter" */
				if (drive_status[dsknum] != Off && drive_status[dsknum] != NoDisk)
					strcpy(disk_filename, sio_filename[dsknum]);
				if (ui_driver->fGetLoadFilename(disk_filename, atari_files_dir, n_atari_files_dir))
					if (!SIO_Mount(dsknum + 1, disk_filename, FALSE))
						CantLoad(disk_filename);
				break;
			case USER_TOGGLE: /* "Space" */
				i = FALSE;
				switch (drive_status[dsknum]) {
				case ReadWrite:
					i = TRUE;
					/* FALLTHROUGH */
				case ReadOnly:
					strcpy(disk_filename, sio_filename[dsknum]);
					SIO_Mount(dsknum + 1, disk_filename, i);
					break;
				default:
					break;
				}
				break;
			default: /* Backspace */
				switch (drive_status[dsknum]) {
				case Off:
					break;
				case NoDisk:
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

int SelectCartType(int k)
{
	static tMenuItem menu_array[] = {
		MENU_ACTION(1, "Standard 8 KB cartridge"),
		MENU_ACTION(2, "Standard 16 KB cartridge"),
		MENU_ACTION(3, "OSS '034M' 16 KB cartridge"),
		MENU_ACTION(4, "Standard 32 KB 5200 cartridge"),
		MENU_ACTION(5, "DB 32 KB cartridge"),
		MENU_ACTION(6, "Two chip 16 KB 5200 cartridge"),
		MENU_ACTION(7, "Bounty Bob 40 KB 5200 cartridge"),
		MENU_ACTION(8, "64 KB Williams cartridge"),
		MENU_ACTION(9, "Express 64 KB cartridge"),
		MENU_ACTION(10, "Diamond 64 KB cartridge"),
		MENU_ACTION(11, "SpartaDOS X 64 KB cartridge"),
		MENU_ACTION(12, "XEGS 32 KB cartridge"),
		MENU_ACTION(13, "XEGS 64 KB cartridge"),
		MENU_ACTION(14, "XEGS 128 KB cartridge"),
		MENU_ACTION(15, "OSS 'M091' 16 KB cartridge"),
		MENU_ACTION(16, "One chip 16 KB 5200 cartridge"),
		MENU_ACTION(17, "Atrax 128 KB cartridge"),
		MENU_ACTION(18, "Bounty Bob 40 KB cartridge"),
		MENU_ACTION(19, "Standard 8 KB 5200 cartridge"),
		MENU_ACTION(20, "Standard 4 KB 5200 cartridge"),
		MENU_ACTION(21, "Right slot 8 KB cartridge"),
		MENU_ACTION(22, "32 KB Williams cartridge"),
		MENU_ACTION(23, "XEGS 256 KB cartridge"),
		MENU_ACTION(24, "XEGS 512 KB cartridge"),
		MENU_ACTION(25, "XEGS 1 MB cartridge"),
		MENU_ACTION(26, "MegaCart 16 KB cartridge"),
		MENU_ACTION(27, "MegaCart 32 KB cartridge"),
		MENU_ACTION(28, "MegaCart 64 KB cartridge"),
		MENU_ACTION(29, "MegaCart 128 KB cartridge"),
		MENU_ACTION(30, "MegaCart 256 KB cartridge"),
		MENU_ACTION(31, "MegaCart 512 KB cartridge"),
		MENU_ACTION(32, "MegaCart 1 MB cartridge"),
		MENU_ACTION(33, "Switchable XEGS 32 KB cartridge"),
		MENU_ACTION(34, "Switchable XEGS 64 KB cartridge"),
		MENU_ACTION(35, "Switchable XEGS 128 KB cartridge"),
		MENU_ACTION(36, "Switchable XEGS 256 KB cartridge"),
		MENU_ACTION(37, "Switchable XEGS 512 KB cartridge"),
		MENU_ACTION(38, "Switchable XEGS 1 MB cartridge"),
		MENU_ACTION(39, "Phoenix 8 KB cartridge"),
		MENU_ACTION(40, "Blizzard 16 KB cartridge"),
		MENU_ACTION(41, "Atarimax 128 KB Flash cartridge"),
		MENU_ACTION(42, "Atarimax 1 MB Flash cartridge"),
		MENU_END
	};

	int i;
	int option = 0;

	ui_driver->fInit();

	for (i = 1; i <= CART_LAST_SUPPORTED; i++)
		if (cart_kb[i] == k) {
			if (option == 0)
				option = i;
			menu_array[i - 1].flags = ITEM_ACTION;
		}
		else
			menu_array[i - 1].flags = ITEM_HIDDEN;

	if (option == 0)
		return CART_NONE;

	option = ui_driver->fSelect("Select Cartridge Type", 0, option, menu_array, NULL);
	if (option > 0)
		return option;

	return CART_NONE;
}

static void CartManagement(void)
{
	static const tMenuItem menu_array[] = {
		MENU_FILESEL(0, "Create Cartridge from ROM image"),
		MENU_FILESEL(1, "Extract ROM image from Cartridge"),
		MENU_FILESEL(2, "Insert Cartridge"),
		MENU_ACTION(3, "Remove Cartridge"),
		MENU_END
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

		option = ui_driver->fSelect("Cartridge Management", 0, option, menu_array, NULL);

		switch (option) {
		case 0:
			if (ui_driver->fGetLoadFilename(cart_filename, atari_files_dir, n_atari_files_dir)) {
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
					ui_driver->fMessage("ROM image must be full kilobytes long");
					break;
				}
				type = SelectCartType(nbytes >> 10);
				if (type == CART_NONE) {
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

				if (!ui_driver->fGetSaveFilename(cart_filename, atari_files_dir, n_atari_files_dir))
					break;

				checksum = CART_Checksum(image, nbytes);
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
			if (ui_driver->fGetLoadFilename(cart_filename, atari_files_dir, n_atari_files_dir)) {
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
					ui_driver->fMessage("Not a CART file");
					break;
				}
				image = (UBYTE *) Util_malloc(nbytes);
				fread(image, 1, nbytes, f);
				fclose(f);

				if (!ui_driver->fGetSaveFilename(cart_filename, atari_files_dir, n_atari_files_dir))
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
			if (ui_driver->fGetLoadFilename(cart_filename, atari_files_dir, n_atari_files_dir)) {
				int r = CART_Insert(cart_filename);
				switch (r) {
				case CART_CANT_OPEN:
					CantLoad(cart_filename);
					break;
				case CART_BAD_FORMAT:
					ui_driver->fMessage("Unknown cartridge format");
					break;
				case CART_BAD_CHECKSUM:
					ui_driver->fMessage("Warning: bad CART checksum");
					break;
				case 0:
					/* ok */
					break;
				default:
					/* r > 0 */
					cart_type = SelectCartType(r);
					break;
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
				Coldstart();
				return;
			}
			break;
		case 3:
			CART_Remove();
			Coldstart();
			return;
		default:
			return;
		}
	}
}

#ifdef SOUND
static void SoundRecording(void)
{
	if (!IsSoundFileOpen()) {
		int no = 0;
		do {
			char buffer[32];
			sprintf(buffer, "atari%03d.wav", no);
			if (!Util_fileexists(buffer)) {
				/* file does not exist - we can create it */
				FilenameMessage(OpenSoundFile(buffer)
					? "Recording sound to file \"%s\""
					: "Can't write to file \"%s\"", buffer);
				return;
			}
		} while (++no < 1000);
		ui_driver->fMessage("All atariXXX.wav files exist!");
	}
	else {
		CloseSoundFile();
		ui_driver->fMessage("Recording stopped");
	}
}
#endif /* SOUND */

static int AutostartFile(void)
{
	static char filename[FILENAME_MAX] = "";
	if (ui_driver->fGetLoadFilename(filename, atari_files_dir, n_atari_files_dir)) {
		if (Atari800_OpenFile(filename, TRUE, 1, FALSE))
			return TRUE;
		CantLoad(filename);
	}
	return FALSE;
}

static void LoadTape(void)
{
	static char filename[FILENAME_MAX] = "";
	if (ui_driver->fGetLoadFilename(filename, atari_files_dir, n_atari_files_dir)) {
		if (!CASSETTE_Insert(filename))
			CantLoad(filename);
	}
}

static void ConfigureDirectories(void)
{
	static tMenuItem menu_array[] = {
		MENU_LABEL("Directories with Atari software:"),
		MENU_FILESEL(0, atari_files_dir[0]),
		MENU_FILESEL(1, atari_files_dir[1]),
		MENU_FILESEL(2, atari_files_dir[2]),
		MENU_FILESEL(3, atari_files_dir[3]),
		MENU_FILESEL(4, atari_files_dir[4]),
		MENU_FILESEL(5, atari_files_dir[5]),
		MENU_FILESEL(6, atari_files_dir[6]),
		MENU_FILESEL(7, atari_files_dir[7]),
		MENU_FILESEL(8, "[add directory]"),
		MENU_LABEL("Directories for emulator-saved files:"),
		MENU_FILESEL(10, saved_files_dir[0]),
		MENU_FILESEL(11, saved_files_dir[1]),
		MENU_FILESEL(12, saved_files_dir[2]),
		MENU_FILESEL(13, saved_files_dir[3]),
		MENU_FILESEL(14, saved_files_dir[4]),
		MENU_FILESEL(15, saved_files_dir[5]),
		MENU_FILESEL(16, saved_files_dir[6]),
		MENU_FILESEL(17, saved_files_dir[7]),
		MENU_FILESEL(18, "[add directory]"),
		MENU_ACTION(9, "What's this?"),
		MENU_ACTION(19, "Back to Emulator Settings"),
		MENU_END
	};
	int option = 9;
	int flags = 0;
	for (;;) {
		int i;
		int seltype;
		char tmp_dir[FILENAME_MAX];
		for (i = 0; i < 8; i++) {
			menu_array[1 + i].flags = (i < n_atari_files_dir) ? (ITEM_FILESEL | ITEM_TIP) : ITEM_HIDDEN;
			menu_array[11 + i].flags = (i < n_saved_files_dir) ? (ITEM_FILESEL | ITEM_TIP) : ITEM_HIDDEN;
			menu_array[1 + i].suffix = menu_array[11 + i].suffix = (flags != 0)
				? "Up/Down:move Space:release"
				: "Ret:change Bksp:delete Space:reorder";
		}
		if (n_atari_files_dir < 2)
			menu_array[1].suffix = "Return:change Backspace:delete";
		if (n_saved_files_dir < 2)
			menu_array[11].suffix = "Return:change Backspace:delete";
		menu_array[9].flags = (n_atari_files_dir < 8) ? ITEM_FILESEL : ITEM_HIDDEN;
		menu_array[19].flags = (n_saved_files_dir < 8) ? ITEM_FILESEL : ITEM_HIDDEN;
		option = ui_driver->fSelect("Configure Directories", flags, option, menu_array, &seltype);
		if (option < 0)
			return;
		if (flags != 0) {
			switch (seltype) {
			case USER_DRAG_UP:
				if (option != 0 && option != 10) {
					strcpy(tmp_dir, menu_array[1 + option].item);
					strcpy(menu_array[1 + option].item, menu_array[option].item);
					strcpy(menu_array[option].item, tmp_dir);
					option--;
				}
				break;
			case USER_DRAG_DOWN:
				if (option != n_atari_files_dir - 1 && option != 10 + n_saved_files_dir - 1) {
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
			if (ui_driver->fGetDirectoryPath(tmp_dir)) {
				strcpy(atari_files_dir[n_atari_files_dir], tmp_dir);
				option = n_atari_files_dir++;
			}
			break;
		case 18:
			tmp_dir[0] = '\0';
			if (ui_driver->fGetDirectoryPath(tmp_dir)) {
				strcpy(saved_files_dir[n_saved_files_dir], tmp_dir);
				option = 10 + n_saved_files_dir++;
			}
			break;
		case 9:
#if 0
			{
				static const tMenuItem help_menu_array[] = {
					MENU_LABEL("You can configure directories,"),
					MENU_LABEL("where you usually store files used by"),
					MENU_LABEL("Atari800, to make them available"),
					MENU_LABEL("at Tab key press in file selectors."),
					MENU_LABEL(""),
					MENU_LABEL("\"Directories with Atari software\""),
					MENU_LABEL("are for disk images, cartridge images,"),
					MENU_LABEL("tape images, executables and BASIC"),
					MENU_LABEL("programs."),
					MENU_LABEL(""),
					MENU_LABEL("\"Directories for emulator-saved files\""),
					MENU_LABEL("are for state files, disk sets and"),
					MENU_LABEL("screenshots taken via User Interface."),
					MENU_LABEL(""),
					MENU_LABEL(""),
					MENU_ACTION(0, "Back"),
					MENU_END
				};
				ui_driver->fSelect("Configure Directories - Help", 0, 0, help_menu_array, NULL);
			}
#else
			ui_driver->fInfoScreen("Configure Directories - Help",
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
			if (seltype == USER_TOGGLE) {
				if ((option < 10 ? n_atari_files_dir : n_saved_files_dir) > 1)
					flags = SELECT_DRAG;
			}
			else if (seltype == USER_DELETE) {
				if (option < 10) {
					if (option >= --n_atari_files_dir) {
						option = 8;
						break;
					}
					for (i = option; i < n_atari_files_dir; i++)
						strcpy(atari_files_dir[i], atari_files_dir[i + 1]);
				}
				else {
					if (option >= --n_saved_files_dir) {
						option = 18;
						break;
					}
					for (i = option - 10; i < n_saved_files_dir; i++)
						strcpy(saved_files_dir[i], saved_files_dir[i + 1]);
				}
			}
			else
				ui_driver->fGetDirectoryPath(menu_array[1 + option].item);
			break;
		}
	}
}

static void AtariSettings(void)
{
	static tMenuItem menu_array[] = {
		MENU_CHECK(0, "Disable BASIC when booting Atari:"),
		MENU_CHECK(1, "Boot from tape (hold Start):"),
		MENU_CHECK(2, "Enable R-Time 8:"),
		MENU_CHECK(3, "SIO patch (fast disk access):"),
		MENU_ACTION(4, "H: device (hard disk):"),
		MENU_CHECK(5, "P: device (printer):"),
#ifdef R_IO_DEVICE
		MENU_CHECK(6, "R: device (Atari850 via net):"),
#else
		MENU_PLACEHOLDER,
#endif
		MENU_FILESEL_PREFIX(7, "H1: ", atari_h_dir[0]),
		MENU_FILESEL_PREFIX(8, "H2: ", atari_h_dir[1]),
		MENU_FILESEL_PREFIX(9, "H3: ", atari_h_dir[2]),
		MENU_FILESEL_PREFIX(10, "H4: ", atari_h_dir[3]),
		MENU_ACTION_PREFIX(11, "Print command: ", print_command),
		MENU_FILESEL_PREFIX(12, " OS/A ROM: ", atari_osa_filename),
		MENU_FILESEL_PREFIX(13, " OS/B ROM: ", atari_osb_filename),
		MENU_FILESEL_PREFIX(14, "XL/XE ROM: ", atari_xlxe_filename),
		MENU_FILESEL_PREFIX(15, " 5200 ROM: ", atari_5200_filename),
		MENU_FILESEL_PREFIX(16, "BASIC ROM: ", atari_basic_filename),
		MENU_FILESEL(17, "Find ROM images in a directory"),
		MENU_SUBMENU(18, "Configure directories"),
		MENU_ACTION(19, "Save configuration file"),
		MENU_END
	};
	char tmp_command[256];
	char rom_dir[FILENAME_MAX];

	int option = 0;

	for (;;) {
		int seltype;
		SetItemChecked(&menu_array[0], disable_basic);
		SetItemChecked(&menu_array[1], hold_start_on_reboot);
		SetItemChecked(&menu_array[2], rtime_enabled);
		SetItemChecked(&menu_array[3], enable_sio_patch);
		menu_array[4].suffix = enable_h_patch ? (h_read_only ? "Read-only" : "Read/write") : "No ";
		SetItemChecked(&menu_array[5], enable_p_patch);
#ifdef R_IO_DEVICE
		SetItemChecked(&menu_array[6], enable_r_patch);
#endif

		option = ui_driver->fSelect("Emulator Settings", 0, option, menu_array, &seltype);

		switch (option) {
		case 0:
			disable_basic = !disable_basic;
			break;
		case 1:
			hold_start_on_reboot = !hold_start_on_reboot;
			hold_start = hold_start_on_reboot;
			break;
		case 2:
			rtime_enabled = !rtime_enabled;
			break;
		case 3:
			enable_sio_patch = !enable_sio_patch;
			break;
		case 4:
			if (!enable_h_patch) {
				enable_h_patch = TRUE;
				h_read_only = TRUE;
			}
			else if (h_read_only)
				h_read_only = FALSE;
			else {
				enable_h_patch = FALSE;
				h_read_only = TRUE;
			}
			break;
		case 5:
			enable_p_patch = !enable_p_patch;
			break;
#ifdef R_IO_DEVICE
		case 6:
			enable_r_patch = !enable_r_patch;
			break;
#endif
		case 7:
		case 8:
		case 9:
		case 10:
			if (seltype == USER_DELETE)
				menu_array[option].item[0] = '\0';
			else
				ui_driver->fGetDirectoryPath(menu_array[option].item);
			break;
		case 11:
			strcpy(tmp_command, print_command);
			if (ui_driver->fEditString("Print command", tmp_command, sizeof(tmp_command)))
				if (!Device_SetPrintCommand(tmp_command))
					ui_driver->fMessage("Specified command is not allowed");
			break;
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
			if (seltype == USER_DELETE)
				menu_array[option].item[0] = '\0';
			else
				ui_driver->fGetLoadFilename(menu_array[option].item, NULL, 0);
			break;
		case 17:
			Util_splitpath(atari_xlxe_filename, rom_dir, NULL);
			if (ui_driver->fGetDirectoryPath(rom_dir)) {
				static const char * const rom_filenames[] = {
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
				const char * const *rom_filename = rom_filenames;
				int i;
				for (i = 0; i < 5; i++) {
					do {
						char full_filename[FILENAME_MAX];
						Util_catpath(full_filename, rom_dir, *rom_filename);
						if (Util_fileexists(full_filename)) {
							strcpy(menu_array[13 + i].item, full_filename);
							while (*++rom_filename != NULL);
							break;
						}
					} while (*++rom_filename != NULL);
					rom_filename++;
				}
			}
			break;
		case 18:
			ConfigureDirectories();
			break;
		case 19:
			ui_driver->fMessage(Atari800_WriteConfig() ? "Configuration file updated" : "Error writing configuration file");
			break;
		default:
			Atari800_UpdatePatches();
			return;
		}
	}
}

static char state_filename[FILENAME_MAX] = "";

static void SaveState(void)
{
	if (ui_driver->fGetSaveFilename(state_filename, saved_files_dir, n_saved_files_dir)) {
		if (!SaveAtariState(state_filename, "wb", TRUE))
			CantSave(state_filename);
	}
}

static void LoadState(void)
{
	if (ui_driver->fGetLoadFilename(state_filename, saved_files_dir, n_saved_files_dir)) {
		if (!ReadAtariState(state_filename, "rb"))
			CantLoad(state_filename);
	}
}

/* CURSES_BASIC doesn't use artifacting or sprite_collisions_in_skipped_frames,
   but does use refresh_rate. However, changing refresh_rate on CURSES is mostly
   useless, as the text-mode screen is normally updated infrequently by Atari.
   In case you wonder how artifacting affects CURSES version without
   CURSES_BASIC: it is visible on the screenshots (yes, they are bitmaps). */
#ifndef CURSES_BASIC

static void DisplaySettings(void)
{
	static const tMenuItem artif_menu_array[] = {
		MENU_ACTION(0, "none"),
		MENU_ACTION(1, "blue/brown 1"),
		MENU_ACTION(2, "blue/brown 2"),
		MENU_ACTION(3, "GTIA"),
		MENU_ACTION(4, "CTIA"),
		MENU_END
	};
	static char refresh_status[16];
	static tMenuItem menu_array[] = {
		MENU_SUBMENU_SUFFIX(0, "Artifacting mode:", NULL),
		MENU_SUBMENU_SUFFIX(1, "Current refresh rate:", refresh_status),
		MENU_CHECK(2, "Accurate skipped frames:"),
		MENU_CHECK(3, "Show percents of Atari speed:"),
		MENU_CHECK(4, "Show disk drive activity:"),
		MENU_CHECK(5, "Show sector counter:"),
#ifdef _WIN32_WCE
		MENU_CHECK(6, "Enable linear filtering:"),
#endif
		MENU_END
	};

	int option = 0;
	int option2;
	for (;;) {
		menu_array[0].suffix = artif_menu_array[global_artif_mode].item;
		sprintf(refresh_status, "1:%-2d", refresh_rate);
		SetItemChecked(&menu_array[2], sprite_collisions_in_skipped_frames);
		SetItemChecked(&menu_array[3], show_atari_speed);
		SetItemChecked(&menu_array[4], show_disk_led);
		SetItemChecked(&menu_array[5], show_sector_counter);
#ifdef _WIN32_WCE
		menu_array[6].flags = filter_available ? (smooth_filter ? (ITEM_CHECK | ITEM_CHECKED) : ITEM_CHECK) : ITEM_HIDDEN;
#endif
		option = ui_driver->fSelect("Display Settings", 0, option, menu_array, NULL);
		switch (option) {
		case 0:
			option2 = ui_driver->fSelect(NULL, SELECT_POPUP, global_artif_mode, artif_menu_array, NULL);
			if (option2 >= 0) {
				global_artif_mode = option2;
				ANTIC_UpdateArtifacting();
			}
			break;
		case 1:
			refresh_rate = ui_driver->fSelectInt(refresh_rate, 1, 99);
			break;
		case 2:
			if (refresh_rate == 1)
				ui_driver->fMessage("No effect with refresh rate 1");
			sprite_collisions_in_skipped_frames = !sprite_collisions_in_skipped_frames;
			break;
		case 3:
			show_atari_speed = !show_atari_speed;
			break;
		case 4:
			show_disk_led = !show_disk_led;
			break;
		case 5:
			show_sector_counter = !show_sector_counter;
			break;
#ifdef _WIN32_WCE
		case 6:
			smooth_filter = !smooth_filter;
			break;
#endif
		default:
			return;
		}
	}
}

#endif /* CURSES_BASIC */

#ifndef USE_CURSES

static void ControllerConfiguration(void)
{
#ifndef _WIN32_WCE
	static const tMenuItem mouse_mode_menu_array[] = {
		MENU_ACTION(0, "None"),
		MENU_ACTION(1, "Paddles"),
		MENU_ACTION(2, "Touch tablet"),
		MENU_ACTION(3, "Koala pad"),
		MENU_ACTION(4, "Light pen"),
		MENU_ACTION(5, "Light gun"),
		MENU_ACTION(6, "Amiga mouse"),
		MENU_ACTION(7, "ST mouse"),
		MENU_ACTION(8, "Trak-ball"),
		MENU_ACTION(9, "Joystick"),
		MENU_END
	};
	static char mouse_port_status[2] = { '1', '\0' };
	static char mouse_speed_status[2] = { '1', '\0' };
#endif
	static tMenuItem menu_array[] = {
		MENU_ACTION(0, "Joystick autofire:"),
		MENU_CHECK(1, "Enable MultiJoy4:"),
#ifdef _WIN32_WCE
		MENU_CHECK(5, "Virtual joystick:"),
#else
		MENU_SUBMENU_SUFFIX(2, "Mouse device: ", NULL),
		MENU_SUBMENU_SUFFIX(3, "Mouse port:", mouse_port_status),
		MENU_SUBMENU_SUFFIX(4, "Mouse speed:", mouse_speed_status),
#endif
		MENU_END
	};

	int option = 0;
#ifndef _WIN32_WCE
	int option2;
#endif
	for (;;) {
		menu_array[0].suffix = joy_autofire[0] == AUTOFIRE_FIRE ? "Fire"
		                     : joy_autofire[0] == AUTOFIRE_CONT ? "Always"
		                     : "No ";
		SetItemChecked(&menu_array[1], joy_multijoy);
#ifdef _WIN32_WCE
		/* XXX: not on smartphones? */
		SetItemChecked(&menu_array[2], virtual_joystick);
#else
		menu_array[2].suffix = mouse_mode_menu_array[mouse_mode].item;
		mouse_port_status[0] = (char) ('1' + mouse_port);
		mouse_speed_status[0] = (char) ('0' + mouse_speed);
#endif
		option = ui_driver->fSelect("Controller Configuration", 0, option, menu_array, NULL);
		switch (option) {
		case 0:
			switch (joy_autofire[0]) {
			case AUTOFIRE_FIRE:
				joy_autofire[0] = joy_autofire[1] = joy_autofire[2] = joy_autofire[3] = AUTOFIRE_CONT;
				break;
			case AUTOFIRE_CONT:
				joy_autofire[0] = joy_autofire[1] = joy_autofire[2] = joy_autofire[3] = AUTOFIRE_OFF;
				break;
			default:
				joy_autofire[0] = joy_autofire[1] = joy_autofire[2] = joy_autofire[3] = AUTOFIRE_FIRE;
				break;
			}
			break;
		case 1:
			joy_multijoy = !joy_multijoy;
			break;
#ifdef _WIN32_WCE
		case 5:
			virtual_joystick = !virtual_joystick;
			break;
#else
		case 2:
			option2 = ui_driver->fSelect(NULL, SELECT_POPUP, mouse_mode, mouse_mode_menu_array, NULL);
			if (option2 >= 0)
				mouse_mode = option2;
			break;
		case 3:
			mouse_port = ui_driver->fSelectInt(mouse_port + 1, 1, 4) - 1;
			break;
		case 4:
			mouse_speed = ui_driver->fSelectInt(mouse_speed, 1, 9);
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
	static tMenuItem menu_array[] = {
		/* XXX: don't allow on smartphones? */
		MENU_CHECK(0, "High Fidelity POKEY:"),
#ifdef STEREO_SOUND
		MENU_CHECK(1, "Dual POKEY (Stereo):"),
#else
		MENU_PLACEHOLDER,
#endif
#ifdef CONSOLE_SOUND
		MENU_CHECK(2, "Speaker (Key Click):"),
#else
		MENU_PLACEHOLDER,
#endif
#ifdef SERIO_SOUND
		MENU_CHECK(3, "Serial IO Sound:"),
#endif
		MENU_END
	};

	int option = 0;

	for (;;) {
		SetItemChecked(&menu_array[0], enable_new_pokey);
#ifdef STEREO_SOUND
		SetItemChecked(&menu_array[1], stereo_enabled);
#endif
#ifdef CONSOLE_SOUND
		SetItemChecked(&menu_array[2], console_sound_enabled);
#endif
#ifdef SERIO_SOUND
		SetItemChecked(&menu_array[3], serio_sound_enabled);
#endif

		option = ui_driver->fSelect(NULL, SELECT_POPUP, option, menu_array, NULL);

		switch (option) {
		case 0:
			enable_new_pokey = !enable_new_pokey;
			Pokey_DoInit();
			/* According to the PokeySnd doc the POKEY switch can occur on
			   a cold-restart only */
			ui_driver->fMessage("Will reboot to apply the change");
			return TRUE; /* reboot required */
#ifdef STEREO_SOUND
		case 1:
			stereo_enabled = !stereo_enabled;
			break;
#endif
#ifdef CONSOLE_SOUND
		case 2:
			console_sound_enabled = !console_sound_enabled;
			break;
#endif
#ifdef SERIO_SOUND
		case 3:
			serio_sound_enabled = !serio_sound_enabled;
			break;
#endif
		default:
			return FALSE;
		}
	}
}

#endif /* SOUND */

#ifndef CURSES_BASIC

#ifdef USE_CURSES
void curses_clear_screen(void);
#endif

static void Screenshot(int interlaced)
{
	static char filename[FILENAME_MAX] = "";
	if (ui_driver->fGetSaveFilename(filename, saved_files_dir, n_saved_files_dir)) {
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

#endif /* CURSES_BASIC */

static void AboutEmulator(void)
{
	ui_driver->fInfoScreen("About the Emulator",
		ATARI_TITLE "\0"
		"Copyright (c) 1995-1998 David Firth\0"
		"and\0"
		"(c)1998-2005 Atari800 Development Team\0"
		"See CREDITS file for details.\0"
		"http://atari800.atari.org/\0"
		"\0"
		"\0"
		"\0"
		"This program is free software; you can\0"
		"redistribute it and/or modify it under\0"
		"the terms of the GNU General Public\0"
		"License as published by the Free\0"
		"Software Foundation; either version 1,\0"
		"or (at your option) any later version.\0"
		"\n");
}

void ui(void)
{
#define MENU_CONTROLLER 19
	static tMenuItem menu_array[] = {
		MENU_FILESEL_ACCEL(MENU_RUN, "Run Atari Program", "Alt+R"),
		MENU_SUBMENU_ACCEL(MENU_DISK, "Disk Management", "Alt+D"),
		MENU_SUBMENU_ACCEL(MENU_CARTRIDGE, "Cartridge Management", "Alt+C"),
		MENU_FILESEL(MENU_CASSETTE, "Select Tape Image"),
		MENU_SUBMENU_ACCEL(MENU_SYSTEM, "Select System", "Alt+Y"),
#ifdef SOUND
		MENU_SUBMENU_ACCEL(MENU_SOUND, "Sound Settings", "Alt+O"),
		MENU_ACTION_ACCEL(MENU_SOUND_RECORDING, "Sound Recording Start/Stop", "Alt+W"),
#endif
#ifndef CURSES_BASIC
		MENU_SUBMENU(MENU_DISPLAY, "Display Settings"),
#endif
#ifndef USE_CURSES
		MENU_SUBMENU(MENU_CONTROLLER, "Controller Configuration"),
#endif
		MENU_SUBMENU(MENU_SETTINGS, "Emulator Configuration"),
		MENU_FILESEL_ACCEL(MENU_SAVESTATE, "Save State", "Alt+S"),
		MENU_FILESEL_ACCEL(MENU_LOADSTATE, "Load State", "Alt+L"),
#ifndef CURSES_BASIC
#ifdef HAVE_LIBPNG
		MENU_FILESEL_ACCEL(MENU_PCX, "Save Screenshot", "F10"),
		/* there isn't enough space for "PNG/PCX Interlaced Screenshot Shift+F10" */
		MENU_FILESEL_ACCEL(MENU_PCXI, "Save Interlaced Screenshot", "Shift+F10"),
#else
		MENU_FILESEL_ACCEL(MENU_PCX, "PCX Screenshot", "F10"),
		MENU_FILESEL_ACCEL(MENU_PCXI, "PCX Interlaced Screenshot", "Shift+F10"),
#endif
#endif
		MENU_ACTION_ACCEL(MENU_BACK, "Back to Emulated Atari", "Esc"),
		MENU_ACTION_ACCEL(MENU_RESETW, "Reset (Warm Start)", "F5"),
		MENU_ACTION_ACCEL(MENU_RESETC, "Reboot (Cold Start)", "Shift+F5"),
#ifdef _WIN32_WCE
		MENU_ACTION(MENU_MONITOR, "About Pocket Atari"),
#else
		MENU_ACTION_ACCEL(MENU_MONITOR, "Enter Monitor", "F8"),
#endif
		MENU_ACTION_ACCEL(MENU_ABOUT, "About the Emulator", "Alt+A"),
		MENU_ACTION_ACCEL(MENU_EXIT, "Exit Emulator", "F9"),
		MENU_END
	};

	int option = MENU_RUN;
	int done = FALSE;

	ui_is_active = TRUE;

	/* Sound_Active(FALSE); */
	ui_driver->fInit();

#ifdef CRASH_MENU
	if (crash_code >= 0) {
		done = CrashMenu();
		crash_code = -1;
	}
#endif

	while (!done) {

		if (alt_function < 0)
			option = ui_driver->fSelect(ATARI_TITLE, 0, option, menu_array, NULL);
		if (alt_function >= 0) {
			option = alt_function;
			alt_function = -1;
			done = TRUE;
		}

		switch (option) {
		case -2:
		case -1:				/* ESC key */
			done = TRUE;
			break;
		case MENU_DISK:
			DiskManagement();
			break;
		case MENU_CARTRIDGE:
			CartManagement();
			break;
		case MENU_RUN:
			/* if (RunExe()) */
			if (AutostartFile())
				done = TRUE;	/* reboot immediately */
			break;
		case MENU_CASSETTE:
			LoadTape();
			break;
		case MENU_SYSTEM:
			SelectSystem();
			break;
		case MENU_SETTINGS:
			AtariSettings();
			break;
#ifdef SOUND
		case MENU_SOUND:
			if (SoundSettings()) {
				Coldstart();
				done = TRUE;	/* reboot immediately */
			}
			break;
		case MENU_SOUND_RECORDING:
			SoundRecording();
			break;
#endif
		case MENU_SAVESTATE:
			SaveState();
			break;
		case MENU_LOADSTATE:
			/* Note: AutostartFile() handles state files, too,
			   so we can remove LoadState() now. */
			LoadState();
			break;
#ifndef CURSES_BASIC
		case MENU_DISPLAY:
			DisplaySettings();
			break;
		case MENU_PCX:
			Screenshot(FALSE);
			break;
		case MENU_PCXI:
			Screenshot(TRUE);
			break;
#endif
#ifndef USE_CURSES
		case MENU_CONTROLLER:
			ControllerConfiguration();
			break;
#endif
		case MENU_BACK:
			done = TRUE;		/* back to emulator */
			break;
		case MENU_RESETW:
			Warmstart();
			done = TRUE;		/* reboot immediately */
			break;
		case MENU_RESETC:
			Coldstart();
			done = TRUE;		/* reboot immediately */
			break;
		case MENU_ABOUT:
			AboutEmulator();
			break;
		case MENU_MONITOR:
#ifdef _WIN32_WCE
			AboutPocketAtari();
			break;
#else
			if (Atari_Exit(TRUE)) {
				done = TRUE;
				break;
			}
			/* if 'quit' typed in monitor, exit emulator */
#endif
		case MENU_EXIT:
			Atari800_Exit(FALSE);
			exit(0);
		}
	}
	/* Sound_Active(TRUE); */
	ui_is_active = FALSE;
	/* flush keypresses */
	while (Atari_Keyboard() != AKEY_NONE)
		atari_sync();
	alt_function = -1;
}


#ifdef CRASH_MENU

int CrashMenu(void)
{
	static char cim_info[42];
	static tMenuItem menu_array[] = {
		MENU_LABEL(cim_info),
		MENU_ACTION_ACCEL(0, "Reset (Warm Start)", "F5"),
		MENU_ACTION_ACCEL(1, "Reboot (Cold Start)", "Shift+F5"),
		MENU_ACTION_ACCEL(2, "Menu", "F1"),
#ifndef _WIN32_WCE
		MENU_ACTION_ACCEL(3, "Enter Monitor", "F8"),
#endif
		MENU_ACTION_ACCEL(4, "Continue After CIM", "Esc"),
		MENU_ACTION_ACCEL(5, "Exit Emulator", "F9"),
		MENU_END
	};

	int option = 0;
	sprintf(cim_info, "Code $%02X (CIM) at address $%04X", crash_code, crash_address);

	for (;;) {
		option = ui_driver->fSelect("!!! The Atari computer has crashed !!!", 0, option, menu_array, NULL);

		if (alt_function >= 0) /* pressed F5, Shift+F5 or F9 */
			return FALSE;

		switch (option) {
		case 0:				/* Power On Reset */
			alt_function = MENU_RESETW;
			return FALSE;
		case 1:				/* Power Off Reset */
			alt_function = MENU_RESETC;
			return FALSE;
		case 2:				/* Menu */
			return FALSE;
#ifndef _WIN32_WCE
		case 3:				/* Monitor */
			alt_function = MENU_MONITOR;
			return FALSE;
#endif
		case -2:
		case -1:			/* ESC key */
		case 4:				/* Continue after CIM */
			regPC = crash_afterCIM;
			return TRUE;
		case 5:				/* Exit */
			alt_function = MENU_EXIT;
			return FALSE;
		}
	}
}
#endif


/*
$Log$
Revision 1.75  2005/10/22 18:13:02  pfusik
another bunch of changes

Revision 1.74  2005/10/19 21:40:30  pfusik
tons of changes, see ChangeLog

Revision 1.73  2005/09/18 14:58:30  pfusik
improved "Extract ROM image from Cartridge";
don't exit emulator if "Update configuration file" failed

Revision 1.72  2005/09/11 20:39:24  pfusik
removed an opendir() call

Revision 1.71  2005/09/07 21:54:02  pfusik
improved "Save Disk Set" and "Make blank ATR disk"

Revision 1.70  2005/09/06 22:54:20  pfusik
improved "Create Cartridge from ROM image"; introduced util.[ch]

Revision 1.69  2005/08/31 20:07:06  pfusik
auto-starting any file supported by the emulator;
MakeBlankDisk() now writes a blank Single Density disk
rather than a 3-sector disk with useless executable loader

Revision 1.68  2005/08/24 21:02:20  pfusik
show_atari_speed, show_disk_led, show_sector_counter
available in "Display Settings"

Revision 1.67  2005/08/23 03:49:34  markgrebe
Fixed typo in #if SERIO_SOUND

Revision 1.66  2005/08/22 20:50:24  pfusik
"Display Settings"

Revision 1.65  2005/08/21 17:40:06  pfusik
Atarimax cartridges; error messages for state load/save;
DO_DIR -> HAVE_OPENDIR; minor clean up

Revision 1.64  2005/08/18 23:34:00  pfusik
shortcut keys in UI

Revision 1.63  2005/08/17 22:47:54  pfusik
compile without <dirent.h>; removed PILL

Revision 1.62  2005/08/16 23:07:28  pfusik
#include "config.h" before system headers

Revision 1.61  2005/08/15 17:26:18  pfusik
"Run BIN file" -> "Run Atari program"

Revision 1.60  2005/08/13 08:53:09  pfusik
CURSES_BASIC; no sound objects if SOUND disabled; no R: if not compiled in

Revision 1.59  2005/08/10 19:39:24  pfusik
hold_start_on_reboot moved to cassette.c;
improved autogeneration of filenames for sound recording

Revision 1.58  2005/08/07 13:44:08  pfusik
display error messages for "Run BIN file", "Select tape",
"Insert cartridge" and "Save screenshot"

Revision 1.57  2005/08/06 18:25:40  pfusik
changed () function signatures to (void)

Revision 1.56  2005/05/15 07:03:52  emuslor
Added configuration update to settings menu

Revision 1.55  2005/04/30 13:46:42  joy
indented properly

Revision 1.54  2005/04/30 13:42:00  joy
make blank boot disk

Revision 1.53  2005/03/10 04:42:35  pfusik
"Extract ROM image from Cartridge" should skip the CART header,
not just copy the whole file;
removed the unused "screen" parameter from ui() and SelectCartType()

Revision 1.52  2005/02/23 16:45:35  pfusik
PNG screenshots

Revision 1.51  2003/12/16 18:26:30  pfusik
new cartridge types: Phoenix and Blizzard

Revision 1.50  2003/12/12 00:24:52  markgrebe
Added enable for console and sio sound

Revision 1.49  2003/11/22 23:26:19  joy
cassette support improved

Revision 1.48  2003/09/23 15:39:07  pfusik
Rotate_Disks()

Revision 1.47  2003/08/31 22:00:06  joy
R: patch named as Atari850 emulation

Revision 1.46  2003/05/28 19:54:58  joy
R: device support (networking?)

Revision 1.45  2003/03/03 10:16:01  joy
multiple disk sets supported

Revision 1.44  2003/02/27 17:42:08  pfusik
new cartridge type

Revision 1.43  2003/02/24 09:33:12  joy
header cleanup

Revision 1.42  2003/02/19 14:07:48  joy
configure stuff cleanup

Revision 1.41  2003/02/09 21:20:43  joy
updated for global enable_new_pokey

Revision 1.40  2003/02/09 13:17:29  joy
switch Pokey cores on-the-fly

Revision 1.39  2002/11/05 22:40:56  joy
UI disk mounting fixes

Revision 1.38  2002/09/16 11:22:06  pfusik
five new cartridge types (Nir Dary)

Revision 1.37  2002/09/05 08:35:11  pfusik
seven new cartridge types (by Nir Dary)

Revision 1.36  2002/08/15 16:57:20  pfusik
1 MB XEGS cart

Revision 1.34  2002/07/14 13:25:36  pfusik
emulation of 576K and 1088K RAM machines

Revision 1.33  2002/07/04 22:35:07  vasyl
Added cassette support in main menu

Revision 1.32  2002/07/04 12:41:38  pfusik
emulation of 16K RAM machines: 400 and 600XL

Revision 1.31  2002/06/23 21:42:09  joy
SoundRecording() accessible from outside (atari_x11.c needs it)

Revision 1.30  2002/03/30 06:19:28  vasyl
Dirty rectangle scheme implementation part 2.
All video memory accesses everywhere are going through the same macros
in ANTIC.C. UI_BASIC does not require special handling anymore. Two new
functions are exposed in ANTIC.H for writing to video memory.

Revision 1.28  2002/01/10 16:46:42  joy
new cartridge type added

Revision 1.27  2001/11/18 19:35:59  fox
fixed a bug: modification of string literals

Revision 1.26  2001/11/04 23:31:39  fox
right slot cartridge

Revision 1.25  2001/10/26 05:43:17  fox
current system is selected by default in SelectSystem()

Revision 1.24  2001/10/12 07:56:15  fox
added 8 KB and 4 KB cartridges for 5200

Revision 1.23  2001/10/11 08:40:29  fox
removed CURSES-specific code

Revision 1.22  2001/10/10 21:35:00  fox
corrected a typo

Revision 1.21  2001/10/10 07:00:45  joy
complete refactoring of UI by Vasyl

Revision 1.20  2001/10/09 00:43:31  fox
OSS 'M019' -> 'M091'

Revision 1.19  2001/10/08 21:03:10  fox
corrected stack bug (thanks Vasyl) and renamed some cartridge types

Revision 1.18  2001/10/05 10:21:52  fox
added Bounty Bob Strikes Back cartridge for 800/XL/XE

Revision 1.17  2001/10/01 17:30:27  fox
Atrax 128 KB cartridge, artif_init -> ANTIC_UpdateArtifacting;
CURSES code cleanup (spaces, memory[], goto)

Revision 1.16  2001/09/21 17:04:57  fox
ANTIC_RunDisplayList -> ANTIC_Frame

Revision 1.15  2001/09/21 16:58:03  fox
included input.h

Revision 1.14  2001/09/17 18:17:53  fox
enable_c000_ram -> ram_size = 52

Revision 1.13  2001/09/17 18:14:01  fox
machine, mach_xlxe, Ram256, os, default_system -> machine_type, ram_size

Revision 1.12  2001/09/09 08:38:02  fox
hold_option -> disable_basic

Revision 1.11  2001/09/08 07:52:30  knik
used FILENAME_MAX instead of MAX_FILENAME_LEN

Revision 1.10  2001/09/04 20:37:01  fox
hold_option, enable_c000_ram and rtime_enabled available in menu

Revision 1.9  2001/07/20 20:14:47  fox
inserting, removing and converting of new cartridge types

Revision 1.7  2001/03/25 06:57:36  knik
open() replaced by fopen()

Revision 1.6  2001/03/18 07:56:48  knik
win32 port

Revision 1.5  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/

/*
 * ui.c - main user interface
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2008 Atari800 development team (see DOC/CREDITS)
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

#include "antic.h"
#include "atari.h"
#include "binload.h"
#include "cartridge.h"
#include "cassette.h"
#include "compfile.h"
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

tUIDriver *ui_driver = &basic_ui_driver;

int ui_is_active = FALSE;
int alt_function = -1;

#ifdef CRASH_MENU
int crash_code = -1;
UWORD crash_address;
UWORD crash_afterCIM;
int CrashMenu(void);
#endif

char atari_files_dir[MAX_DIRECTORIES][FILENAME_MAX];
char saved_files_dir[MAX_DIRECTORIES][FILENAME_MAX];
int n_atari_files_dir = 0;
int n_saved_files_dir = 0;

static tMenuItem *FindMenuItem(tMenuItem *mip, int option)
{
	while (mip->retval != option)
		mip++;
	return mip;
}

static void SetItemChecked(tMenuItem *mip, int option, int checked)
{
	FindMenuItem(mip, option)->flags = checked ? (ITEM_CHECK | ITEM_CHECKED) : ITEM_CHECK;
}

static void FilenameMessage(const char *format, const char *filename)
{
	char msg[FILENAME_MAX + 30];
	sprintf(msg, format, filename);
	ui_driver->fMessage(msg, 1);
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
		MENU_ACTION(9, "Atari XL/XE (192 KB)"),
		MENU_ACTION(10, "Atari XL/XE (320 KB RAMBO)"),
		MENU_ACTION(11, "Atari XL/XE (320 KB COMPY SHOP)"),
		MENU_ACTION(12, "Atari XL/XE (576 KB)"),
		MENU_ACTION(13, "Atari XL/XE (1088 KB)"),
		MENU_ACTION(14, "Atari 5200 (16 KB)"),
		MENU_ACTION(15, "Video system:"),
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
		{ MACHINE_XLXE, 192 },
		{ MACHINE_XLXE, RAM_320_RAMBO },
		{ MACHINE_XLXE, RAM_320_COMPY_SHOP },
		{ MACHINE_XLXE, 576 },
		{ MACHINE_XLXE, 1088 },
		{ MACHINE_5200, 16 }
	};

#define N_MACHINES  ((int) (sizeof(machine) / sizeof(machine[0])))

	int option = 0;
	int old_tv_mode = tv_mode;

	int i;
	for (i = 0; i < N_MACHINES; i++)
		if (machine_type == machine[i].type && ram_size == machine[i].ram) {
			option = i;
			break;
		}

	for (;;) {
		menu_array[N_MACHINES].suffix = (tv_mode == TV_PAL) ? "PAL" : "NTSC";
		option = ui_driver->fSelect("Select System", 0, option, menu_array, NULL);
		if (option < N_MACHINES)
			break;
		tv_mode = (tv_mode == TV_PAL) ? TV_NTSC : TV_PAL;
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
		MENU_FILESEL_TIP(12, "Uncompress Disk Image", "Convert GZ or DCM to ATR"),
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
		case 12:
			if (ui_driver->fGetLoadFilename(disk_filename, atari_files_dir, n_atari_files_dir)) {
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
					if (ui_driver->fGetSaveFilename(uncompr_filename, atari_files_dir, n_atari_files_dir)) {
						FILE *fp2 = fopen(uncompr_filename, "wb");
						int success;
						if (fp2 == NULL) {
							CantSave(uncompr_filename);
							continue;
						}
						success = CompressedFile_ExtractGZ(disk_filename, fp2);
						fclose(fp2);
						ui_driver->fMessage(success ? "Conversion successful" : "Cannot convert this file", 1);
					}
					break;
				case 0xf9:
				case 0xfa:
					if (ui_driver->fGetSaveFilename(uncompr_filename, atari_files_dir, n_atari_files_dir)) {
						FILE *fp2 = fopen(uncompr_filename, "wb");
						int success;
						if (fp2 == NULL) {
							fclose(fp);
							CantSave(uncompr_filename);
							continue;
						}
						Util_rewind(fp);
						success = CompressedFile_DCMtoATR(fp, fp2);
						fclose(fp2);
						fclose(fp);
						ui_driver->fMessage(success ? "Conversion successful" : "Cannot convert this file", 1);
					}
					break;
				default:
					fclose(fp);
					ui_driver->fMessage("This is not a compressed disk image", 1);
					break;
				}
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
		MENU_ACTION(43, "SpartaDOS X 128 KB cartridge"),
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
					ui_driver->fMessage("ROM image must be full kilobytes long", 1);
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
					ui_driver->fMessage("Not a CART file", 1);
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
					ui_driver->fMessage("Unknown cartridge format", 1);
					break;
				case CART_BAD_CHECKSUM:
					ui_driver->fMessage("Warning: bad CART checksum", 1);
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

#if defined(SOUND) && !defined(DREAMCAST)
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
		ui_driver->fMessage("All atariXXX.wav files exist!", 1);
	}
	else {
		CloseSoundFile();
		ui_driver->fMessage("Recording stopped", 1);
	}
}
#endif /* defined(SOUND) && !defined(DREAMCAST) */

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

static void AdvancedHOptions(void)
{
	static char open_info[] = "0 currently open files";
	static tMenuItem menu_array[] = {
		MENU_ACTION(0, "Atari executables path"),
		MENU_ACTION_TIP(1, open_info, NULL),
		MENU_LABEL("Current directories:"),
		MENU_ACTION_PREFIX_TIP(2, "H1:", h_current_dir[0], NULL),
		MENU_ACTION_PREFIX_TIP(3, "H2:", h_current_dir[1], NULL),
		MENU_ACTION_PREFIX_TIP(4, "H3:", h_current_dir[2], NULL),
		MENU_ACTION_PREFIX_TIP(5, "H4:", h_current_dir[3], NULL),
		MENU_END
	};
	int option = 0;
	for (;;) {
		int i;
		int seltype;
		i = Device_H_CountOpen();
		open_info[0] = (char) ('0' + i);
		open_info[21] = (i != 1) ? 's' : '\0';
		menu_array[1].suffix = (i > 0) ? ((i == 1) ? "Backspace: close" : "Backspace: close all") : NULL;
		for (i = 0; i < 4; i++)
			menu_array[3 + i].suffix = h_current_dir[i][0] != '\0' ? "Backspace: reset to root" : NULL;
		option = ui_driver->fSelect("Advanced H: options", 0, option, menu_array, &seltype);
		switch (option) {
		case 0:
			{
				char tmp_path[FILENAME_MAX];
				strcpy(tmp_path, h_exe_path);
				if (ui_driver->fEditString("Atari executables path", tmp_path, FILENAME_MAX))
					strcpy(h_exe_path, tmp_path);
			}
			break;
		case 1:
			if (seltype == USER_DELETE)
				Device_H_CloseAll();
			break;
		case 2:
		case 3:
		case 4:
		case 5:
			if (seltype == USER_DELETE)
				h_current_dir[option - 2][0] = '\0';
			break;
		default:
			return;
		}
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
#endif
		MENU_FILESEL_PREFIX(7, "H1: ", atari_h_dir[0]),
		MENU_FILESEL_PREFIX(8, "H2: ", atari_h_dir[1]),
		MENU_FILESEL_PREFIX(9, "H3: ", atari_h_dir[2]),
		MENU_FILESEL_PREFIX(10, "H4: ", atari_h_dir[3]),
		MENU_SUBMENU(20, "Advanced H: options"),
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
		SetItemChecked(menu_array, 0, disable_basic);
		SetItemChecked(menu_array, 1, hold_start_on_reboot);
		SetItemChecked(menu_array, 2, rtime_enabled);
		SetItemChecked(menu_array, 3, enable_sio_patch);
		menu_array[4].suffix = enable_h_patch ? (h_read_only ? "Read-only" : "Read/write") : "No ";
		SetItemChecked(menu_array, 5, enable_p_patch);
#ifdef R_IO_DEVICE
		SetItemChecked(menu_array, 6, enable_r_patch);
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
				FindMenuItem(menu_array, option)->item[0] = '\0';
			else
				ui_driver->fGetDirectoryPath(FindMenuItem(menu_array, option)->item);
			break;
		case 20:
			AdvancedHOptions();
			break;
		case 11:
			strcpy(tmp_command, print_command);
			if (ui_driver->fEditString("Print command", tmp_command, sizeof(tmp_command)))
				if (!Device_SetPrintCommand(tmp_command))
					ui_driver->fMessage("Specified command is not allowed", 1);
			break;
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
			if (seltype == USER_DELETE)
				FindMenuItem(menu_array, option)->item[0] = '\0';
			else
				ui_driver->fGetLoadFilename(FindMenuItem(menu_array, option)->item, NULL, 0);
			break;
		case 17:
			Util_splitpath(atari_xlxe_filename, rom_dir, NULL);
			if (ui_driver->fGetDirectoryPath(rom_dir))
				Atari800_FindROMImages(rom_dir, FALSE);
			break;
		case 18:
			ConfigureDirectories();
			break;
		case 19:
			ui_driver->fMessage(Atari800_WriteConfig() ? "Configuration file updated" : "Error writing configuration file", 1);
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
		ui_driver->fMessage("Please wait while saving...", 0);
		if (!SaveAtariState(state_filename, "wb", TRUE))
			CantSave(state_filename);
	}
}

static void LoadState(void)
{
	if (ui_driver->fGetLoadFilename(state_filename, saved_files_dir, n_saved_files_dir)) {
		ui_driver->fMessage("Please wait while loading...", 0);
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
		MENU_CHECK(11, "Enable new artifacting:"),
		MENU_SUBMENU_SUFFIX(1, "Current refresh rate:", refresh_status),
		MENU_CHECK(2, "Accurate skipped frames:"),
		MENU_CHECK(3, "Show percents of Atari speed:"),
		MENU_CHECK(4, "Show disk drive activity:"),
		MENU_CHECK(5, "Show sector counter:"),
#ifdef _WIN32_WCE
		MENU_CHECK(6, "Enable linear filtering:"),
#endif
#ifdef DREAMCAST
		MENU_CHECK(7, "Double buffer video data:"),
		MENU_ACTION(8, "Emulator video mode:"),
		MENU_ACTION(9, "Display video mode:"),
#ifdef HZ_TEST
		MENU_ACTION(10, "DO HZ TEST:"),
#endif
#endif
		MENU_END
	};

	int option = 0;
	int option2;
	for (;;) {
		FindMenuItem(menu_array, 0)->suffix = artif_menu_array[global_artif_mode].item;
		SetItemChecked(menu_array, 11, artif_new);
		sprintf(refresh_status, "1:%-2d", refresh_rate);
		SetItemChecked(menu_array, 2, sprite_collisions_in_skipped_frames);
		SetItemChecked(menu_array, 3, show_atari_speed);
		SetItemChecked(menu_array, 4, show_disk_led);
		SetItemChecked(menu_array, 5, show_sector_counter);
#ifdef _WIN32_WCE
		FindMenuItem(menu_array, 6)->flags = filter_available ? (smooth_filter ? (ITEM_CHECK | ITEM_CHECKED) : ITEM_CHECK) : ITEM_HIDDEN;
#endif
#ifdef DREAMCAST
		SetItemChecked(menu_array, 7, db_mode);
		FindMenuItem(menu_array, 8)->suffix = tv_mode == TV_NTSC ? "NTSC" : "PAL";
		FindMenuItem(menu_array, 9)->suffix = screen_tv_mode == TV_NTSC ? "NTSC" : "PAL";
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
		case 11:
			artif_new = !artif_new;
			ANTIC_UpdateArtifacting();
			break;
		case 1:
			refresh_rate = ui_driver->fSelectInt(refresh_rate, 1, 99);
			break;
		case 2:
			if (refresh_rate == 1)
				ui_driver->fMessage("No effect with refresh rate 1", 1);
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
#ifdef DREAMCAST
		case 7:
			if (db_mode)
				db_mode = FALSE;
			else if (tv_mode == screen_tv_mode)
				db_mode = TRUE;
			update_screen_updater();
			entire_screen_dirty();
			break;
		case 8:
			tv_mode = (tv_mode == TV_PAL) ? TV_NTSC : TV_PAL;
			if (tv_mode != screen_tv_mode) {
				db_mode = FALSE;
				update_screen_updater();
			}
			update_vidmode();
			entire_screen_dirty();
			break;
		case 9:
			tv_mode = screen_tv_mode = (screen_tv_mode == TV_PAL) ? TV_NTSC : TV_PAL;
			update_vidmode();
			entire_screen_dirty();
			break;
#ifdef HZ_TEST
		case 10:
			do_hz_test();
			entire_screen_dirty();
			break;
#endif
#endif /* DREAMCAST */
		default:
			return;
		}
	}
}

#endif /* CURSES_BASIC */

#ifndef USE_CURSES

#ifdef SDL
static char joy_0_desc[40];
static char joy_1_desc[40];
#endif

static void ControllerConfiguration(void)
{
#if !defined(_WIN32_WCE) && !defined(DREAMCAST)
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
#if defined(_WIN32_WCE)
		MENU_CHECK(5, "Virtual joystick:"),
#elif defined(DREAMCAST)
		MENU_CHECK(9, "Emulate Paddles:"),
		MENU_ACTION(10, "Button configuration"),
#else
		MENU_SUBMENU_SUFFIX(2, "Mouse device: ", NULL),
		MENU_SUBMENU_SUFFIX(3, "Mouse port:", mouse_port_status),
		MENU_SUBMENU_SUFFIX(4, "Mouse speed:", mouse_speed_status),
#endif
#ifdef SDL
		MENU_CHECK(5, "Enable keyboard joystick 1:"),
		MENU_LABEL(joy_0_desc),
		MENU_CHECK(7, "Enable keyboard joystick 2:"),
		MENU_LABEL(joy_1_desc),
#endif
		MENU_END
	};

	int option = 0;
#if !defined(_WIN32_WCE) && !defined(DREAMCAST)
	int option2;
#endif
	for (;;) {
		menu_array[0].suffix = joy_autofire[0] == AUTOFIRE_FIRE ? "Fire"
		                     : joy_autofire[0] == AUTOFIRE_CONT ? "Always"
		                     : "No ";
		SetItemChecked(menu_array, 1, joy_multijoy);
#if defined(_WIN32_WCE)
		/* XXX: not on smartphones? */
		SetItemChecked(menu_array, 5, virtual_joystick);
#elif defined(DREAMCAST)
		SetItemChecked(menu_array, 9, emulate_paddles);
#else
		menu_array[2].suffix = mouse_mode_menu_array[mouse_mode].item;
		mouse_port_status[0] = (char) ('1' + mouse_port);
		mouse_speed_status[0] = (char) ('0' + mouse_speed);
#endif
#ifdef SDL
		SetItemChecked(menu_array, 5, kbd_joy_0_enabled);
		joy_0_description(joy_0_desc, sizeof(joy_0_desc));
		SetItemChecked(menu_array, 7, kbd_joy_1_enabled);
		joy_1_description(joy_1_desc, sizeof(joy_1_desc));
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
#ifdef SDL
		case 5:
			kbd_joy_0_enabled = !kbd_joy_0_enabled;
			break;
		case 7:
			kbd_joy_1_enabled = !kbd_joy_1_enabled;
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
#endif
#ifdef CONSOLE_SOUND
		MENU_CHECK(2, "Speaker (Key Click):"),
#endif
#ifdef SERIO_SOUND
		MENU_CHECK(3, "Serial IO Sound:"),
#endif
		MENU_ACTION(4, "Enable higher frequencies:"),
#ifdef DREAMCAST
		MENU_CHECK(5, "Enable sound:"),
#endif
		MENU_END
	};

	int option = 0;

	for (;;) {
		SetItemChecked(menu_array, 0, enable_new_pokey);
#ifdef STEREO_SOUND
		SetItemChecked(menu_array, 1, stereo_enabled);
#endif
#ifdef CONSOLE_SOUND
		SetItemChecked(menu_array, 2, console_sound_enabled);
#endif
#ifdef SERIO_SOUND
		SetItemChecked(menu_array, 3, serio_sound_enabled);
#endif
		FindMenuItem(menu_array, 4)->suffix = enable_new_pokey ? "N/A" : snd_bienias_fix ? "Yes" : "No ";
#ifdef DREAMCAST
		SetItemChecked(menu_array, 5, glob_snd_ena);
#endif

#if 0
		option = ui_driver->fSelect(NULL, SELECT_POPUP, option, menu_array, NULL);
#else
		option = ui_driver->fSelect("Sound Settings", 0, option, menu_array, NULL);
#endif

		switch (option) {
		case 0:
			enable_new_pokey = !enable_new_pokey;
			Pokey_DoInit();
			/* According to the PokeySnd doc the POKEY switch can occur on
			   a cold-restart only */
			ui_driver->fMessage("Will reboot to apply the change", 1);
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
		case 4:
			if (! enable_new_pokey) snd_bienias_fix = !snd_bienias_fix;
			break;
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

#endif /* !defined(CURSES_BASIC) && !defined(DREAMCAST) */

static void AboutEmulator(void)
{
	ui_driver->fInfoScreen("About the Emulator",
		ATARI_TITLE "\0"
		"Copyright (c) 1995-1998 David Firth\0"
		"and\0"
		"(c)1998-2008 Atari800 Development Team\0"
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
#ifndef DREAMCAST
		MENU_ACTION_ACCEL(MENU_SOUND_RECORDING, "Sound Recording Start/Stop", "Alt+W"),
#endif
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
#if !defined(CURSES_BASIC) && !defined(DREAMCAST)
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
#if defined(_WIN32_WCE)
		MENU_ACTION(MENU_MONITOR, "About Pocket Atari"),
#elif defined(DREAMCAST)
		MENU_ACTION(MENU_MONITOR, "About AtariDC"),
#else
		MENU_ACTION_ACCEL(MENU_MONITOR, "Enter Monitor", "F8"),
#endif
		MENU_ACTION_ACCEL(MENU_ABOUT, "About the Emulator", "Alt+A"),
		MENU_ACTION_ACCEL(MENU_EXIT, "Exit Emulator", "F9"),
		MENU_END
	};

	int option = MENU_RUN;
	int done = FALSE;
#ifdef XEP80_EMULATION
	int saved_xep80 = Atari_xep80;
	if (Atari_xep80) {
		Atari_SwitchXep80();
	}
#endif

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
#ifndef DREAMCAST
		case MENU_SOUND_RECORDING:
			SoundRecording();
			break;
#endif
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
#ifndef DREAMCAST
		case MENU_PCX:
			Screenshot(FALSE);
			break;
		case MENU_PCXI:
			Screenshot(TRUE);
			break;
#endif
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
#if defined(_WIN32_WCE)
			AboutPocketAtari();
			break;
#elif defined(DREAMCAST)
			AboutAtariDC();
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
	/* restore XEP80 screen */
#ifdef XEP80_EMULATION
	if (saved_xep80 != Atari_xep80) {
		Atari_SwitchXep80();
	}
#endif
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
#if !defined(_WIN32_WCE) && !defined(DREAMCAST)
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
#if !defined(_WIN32_WCE) && !defined(DREAMCAST)
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

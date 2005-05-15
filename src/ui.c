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

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>				/* for free() */
#include <unistd.h>				/* for open() */
#include <dirent.h>
#include <sys/stat.h>
#include "rt-config.h"
#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "platform.h"
#include "prompts.h"
#include "gtia.h"
#include "sio.h"
#include "ui.h"
#include "log.h"
#include "statesav.h"
#include "config.h"
#include "antic.h"
#include "screen.h"
#include "binload.h"
#include "sndsave.h"
#include "cartridge.h"
#include "cassette.h"
#include "rtime.h"
#include "input.h"
#include "pokeysnd.h"
#include "rt-config.h"			/* extern for enable_new_pokey and stereo_enabled */
#include "boot.h"

tUIDriver *ui_driver = &basic_ui_driver;

int ui_is_active = FALSE;
int alt_function = -1;			/* alt function init */
int current_disk_directory = 0;
int hold_start_on_reboot = 0;

static char curr_disk_dir[FILENAME_MAX] = "";
static char curr_cart_dir[FILENAME_MAX] = "";
static char curr_exe_dir[FILENAME_MAX] = "";
static char curr_state_dir[FILENAME_MAX] = "";
static char curr_tape_dir[FILENAME_MAX] = "";

#ifdef CRASH_MENU
int crash_code = -1;
UWORD crash_address;
UWORD crash_afterCIM;
int CrashMenu();
#endif

/* Forward declarations */
void DiskManagement();
void CartManagement();
int RunExe();
int LoadTape();
void SelectSystem();
int SoundSettings();
void SelectArtifacting();
void AtariSettings();
int SaveState();
int LoadState();
void Screenshot(int interlaced);
void MakeBlankDisk(FILE * setFile);

void SelectSystem()
{
	typedef struct {
		int type;
		int ram;
	} tSysConfig;

	static tMenuItem menu_array[] = {
		{"SYAF", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari OS/A (16 KB)", NULL, 0},
		{"SYAS", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari OS/A (48 KB)", NULL, 1},
		{"SYAL", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari OS/A (52 KB)", NULL, 2},
		{"SYBF", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari OS/B (16 KB)", NULL, 3},
		{"SYBS", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari OS/B (48 KB)", NULL, 4},
		{"SYBL", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari OS/B (52 KB)", NULL, 5},
		{"SYXS", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari 600XL (16 KB)", NULL, 6},
		{"SYXL", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari 800XL (64 KB)", NULL, 7},
		{"SYXE", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari 130XE (128 KB)", NULL, 8},
		{"SYRM", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari 320XE (320 KB RAMBO)", NULL, 9},
		{"SYCS", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari 320XE (320 KB COMPY SHOP)", NULL, 10},
		{"SY05", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari 576XE (576 KB)", NULL, 11},
		{"SY1M", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari 1088XE (1088 KB)", NULL, 12},
		{"SY52", ITEM_ENABLED | ITEM_ACTION, NULL, "Atari 5200 (16 KB)", NULL, 13},
		MENU_END
	};

	static tSysConfig machine[] = {
		{MACHINE_OSA, 16},
		{MACHINE_OSA, 48},
		{MACHINE_OSA, 52},
		{MACHINE_OSB, 16},
		{MACHINE_OSB, 48},
		{MACHINE_OSB, 52},
		{MACHINE_XLXE, 16},
		{MACHINE_XLXE, 64},
		{MACHINE_XLXE, 128},
		{MACHINE_XLXE, RAM_320_RAMBO},
		{MACHINE_XLXE, RAM_320_COMPY_SHOP},
		{MACHINE_XLXE, 576},
		{MACHINE_XLXE, 1088},
		{MACHINE_5200, 16}
	};

	int system = 0;
	int nsystems = sizeof(machine) / sizeof(machine[0]);

	int i;
	for (i = 0; i < nsystems; i++)
		if (machine_type == machine[i].type && ram_size == machine[i].ram) {
			system = i;
			break;
		}

	system = ui_driver->fSelect("Select System", FALSE, system, menu_array, NULL);

	if (system >= 0 && system < nsystems) {
		machine_type = machine[system].type;
		ram_size = machine[system].ram;
		Atari800_InitialiseMachine();
	}
}

void DiskManagement()
{
	static char drive_array[8][7];

	static tMenuItem menu_array[] = {
		{"DKS1", ITEM_ENABLED | ITEM_FILESEL | ITEM_MULTI, drive_array[0], sio_filename[0], NULL, 0},
		{"DKS2", ITEM_ENABLED | ITEM_FILESEL | ITEM_MULTI, drive_array[1], sio_filename[1], NULL, 1},
		{"DKS3", ITEM_ENABLED | ITEM_FILESEL | ITEM_MULTI, drive_array[2], sio_filename[2], NULL, 2},
		{"DKS4", ITEM_ENABLED | ITEM_FILESEL | ITEM_MULTI, drive_array[3], sio_filename[3], NULL, 3},
		{"DKS5", ITEM_ENABLED | ITEM_FILESEL | ITEM_MULTI, drive_array[4], sio_filename[4], NULL, 4},
		{"DKS6", ITEM_ENABLED | ITEM_FILESEL | ITEM_MULTI, drive_array[5], sio_filename[5], NULL, 5},
		{"DKS7", ITEM_ENABLED | ITEM_FILESEL | ITEM_MULTI, drive_array[6], sio_filename[6], NULL, 6},
		{"DKS8", ITEM_ENABLED | ITEM_FILESEL | ITEM_MULTI, drive_array[7], sio_filename[7], NULL, 7},
		{"SSET", ITEM_ENABLED | ITEM_FILESEL | ITEM_MULTI, NULL, "Save Disk Set", NULL, 8},
		{"LSET", ITEM_ENABLED | ITEM_FILESEL | ITEM_MULTI, NULL, "Load Disk Set", NULL, 9},
		{"RDSK", ITEM_ENABLED | ITEM_ACTION, NULL, "Rotate Disks", NULL, 10},
		{"MDSK", ITEM_ENABLED | ITEM_ACTION | ITEM_MULTI, NULL, "Make Blank Boot Disk", NULL, 11},
		MENU_END
	};

	int rwflags[8];
	int done = FALSE;
	int dsknum = 0;
	int i;

	for (i = 0; i < 8; ++i) {
		menu_array[i].item = sio_filename[i];
		rwflags[i] = (drive_status[i] == ReadOnly ? TRUE : FALSE);
	}

	while (!done) {
		char filename[FILENAME_MAX + 1];
		char diskfilename[FILENAME_MAX + 1];
		char setname[FILENAME_MAX + 1];
		int seltype;
		FILE *setFile;

		for (i = 0; i < 8; i++) {
			sprintf(menu_array[i].prefix, "<%c>D%d:", rwflags[i] ? 'R' : 'W', i + 1);
		}

		dsknum = ui_driver->fSelect("Disk Management", FALSE, dsknum, menu_array, &seltype);

		if (dsknum > -1) {
			if (dsknum == 10) {
				Rotate_Disks();
			}
			else if (seltype == USER_SELECT) {	/* User pressed "Enter" to select a disk image */
				char *pathname;

/*              pathname=atari_disk_dirs[current_disk_directory]; */

				if (dsknum == 11) {
					ui_driver->fGetSaveFilename(filename);
					strcpy(setname, curr_disk_dir);
					if (*setname) {
						char last = setname[strlen(setname) - 1];
						if (last != '/' && last != '\\')
#ifdef BACK_SLASH
							strcat(setname, "\\");
#else
							strcat(setname, "/");
#endif
					}
					strcat(setname, filename);
					setFile = fopen(setname, "wb");
					MakeBlankDisk(setFile);
					fclose(setFile);
					done = TRUE;
					continue;
				}
				if (curr_disk_dir[0] == '\0')
					strcpy(curr_disk_dir, atari_disk_dirs[current_disk_directory]);

				if (dsknum == 8) {	/* Save a disk set */
					/* Get the filename */
					ui_driver->fGetSaveFilename(filename);

					/* Put it in the current disks directory */
					strcpy(setname, curr_disk_dir);
					if (*setname) {
						char last = setname[strlen(setname) - 1];
						if (last != '/' && last != '\\')
#ifdef BACK_SLASH
							strcat(setname, "\\");
#else
							strcat(setname, "/");
#endif
					}
					strcat(setname, filename);

					/* Write the current disk file names out to it */
					setFile = fopen(setname, "w");
					if (setFile) {
						for (i = 0; i < 8; i++)
							fprintf(setFile, "%s\n", sio_filename[i]);
					}
					fclose(setFile);
				}
				else {
					while (ui_driver->fGetLoadFilename(curr_disk_dir, filename)) {
						DIR *subdir;

						subdir = opendir(filename);
						if (!subdir) {	/* A file was selected */
							if (dsknum < 8) {	/* Normal disk mount */
								SIO_Dismount(dsknum + 1);
								/* try to mount read/write */
								SIO_Mount(dsknum + 1, filename, FALSE);
								/* update rwflags with the real mount status */
								rwflags[dsknum] = (drive_status[dsknum] == ReadOnly ? TRUE : FALSE);
							}
							else if (dsknum == 9) {	/* Load a disk set */
								setFile = fopen(filename, "r");
								if (setFile) {
									for (i = 0; i < 8; i++) {
										/* Get the disk filename from the set file */
										fgets(diskfilename, FILENAME_MAX, setFile);
										/* Remove the trailing newline */
										if (strlen(diskfilename) != 0)
											diskfilename[strlen(diskfilename) - 1] = 0;
										/* If the disk drive wasn't empty or off when saved,
										   mount the disk */
										if ((strcmp(diskfilename, "Empty") != 0) && (strcmp(diskfilename, "Off") != 0)) {
											SIO_Dismount(i + 1);
											/* Mount the disk read/write */
											SIO_Mount(i + 1, diskfilename, FALSE);
											/* update rwflags with the real mount status */
											rwflags[i] = (drive_status[i] == ReadOnly ? TRUE : FALSE);
										}
									}
									fclose(setFile);
								}
							}
							break;
						}
						else {	/* A directory was selected */
							closedir(subdir);
							pathname = filename;
						}
					}
				}
			}
			else if (seltype == USER_TOGGLE) {
				if (dsknum < 8) {
					/* User pressed "SpaceBar" to change R/W status of this drive */
					/* unmount */
					SIO_Dismount(dsknum + 1);
					/* try to remount with changed R/W status (!rwflags) */
					SIO_Mount(dsknum + 1, filename, !rwflags[dsknum]);
					/* update rwflags with the real mount status */
					rwflags[dsknum] = (drive_status[dsknum] == ReadOnly ? TRUE : FALSE);
				}
			}
			else {
				if (dsknum < 8) {
					if (strcmp(sio_filename[dsknum], "Empty") == 0)
						SIO_DisableDrive(dsknum + 1);
					else
						SIO_Dismount(dsknum + 1);
				}
			}
		}
		else
			done = TRUE;
	}
}

int SelectCartType(int k)
{
	static tMenuItem menu_array[] = {
		{"NONE", 0, NULL, NULL, NULL, 0},
		{"CRT1", ITEM_ACTION, NULL, "Standard 8 KB cartridge", NULL, 1},
		{"CRT2", ITEM_ACTION, NULL, "Standard 16 KB cartridge", NULL, 2},
		{"CRT3", ITEM_ACTION, NULL, "OSS '034M' 16 KB cartridge", NULL, 3},
		{"CRT4", ITEM_ACTION, NULL, "Standard 32 KB 5200 cartridge", NULL, 4},
		{"CRT5", ITEM_ACTION, NULL, "DB 32 KB cartridge", NULL, 5},
		{"CRT6", ITEM_ACTION, NULL, "Two chip 16 KB 5200 cartridge", NULL, 6},
		{"CRT7", ITEM_ACTION, NULL, "Bounty Bob 40 KB 5200 cartridge", NULL, 7},
		{"CRT8", ITEM_ACTION, NULL, "64 KB Williams cartridge", NULL, 8},
		{"CRT9", ITEM_ACTION, NULL, "Express 64 KB cartridge", NULL, 9},
		{"CRTA", ITEM_ACTION, NULL, "Diamond 64 KB cartridge", NULL, 10},
		{"CRTB", ITEM_ACTION, NULL, "SpartaDOS X 64 KB cartridge", NULL, 11},
		{"CRTC", ITEM_ACTION, NULL, "XEGS 32 KB cartridge", NULL, 12},
		{"CRTD", ITEM_ACTION, NULL, "XEGS 64 KB cartridge", NULL, 13},
		{"CRTE", ITEM_ACTION, NULL, "XEGS 128 KB cartridge", NULL, 14},
		{"CRTF", ITEM_ACTION, NULL, "OSS 'M091' 16 KB cartridge", NULL, 15},
		{"CRTG", ITEM_ACTION, NULL, "One chip 16 KB 5200 cartridge", NULL, 16},
		{"CRTH", ITEM_ACTION, NULL, "Atrax 128 KB cartridge", NULL, 17},
		{"CRTI", ITEM_ACTION, NULL, "Bounty Bob 40 KB cartridge", NULL, 18},
		{"CRTJ", ITEM_ACTION, NULL, "Standard 8 KB 5200 cartridge", NULL, 19},
		{"CRTK", ITEM_ACTION, NULL, "Standard 4 KB 5200 cartridge", NULL, 20},
		{"CRTL", ITEM_ACTION, NULL, "Right slot 8 KB cartridge", NULL, 21},
		{"CRTM", ITEM_ACTION, NULL, "32 KB Williams cartridge", NULL, 22},
		{"CRTN", ITEM_ACTION, NULL, "XEGS 256 KB cartridge", NULL, 23},
		{"CRTO", ITEM_ACTION, NULL, "XEGS 512 KB cartridge", NULL, 24},
		{"CRTP", ITEM_ACTION, NULL, "XEGS 1 MB cartridge", NULL, 25},
		{"CRTQ", ITEM_ACTION, NULL, "MegaCart 16 KB cartridge", NULL, 26},
		{"CRTR", ITEM_ACTION, NULL, "MegaCart 32 KB cartridge", NULL, 27},
		{"CRTS", ITEM_ACTION, NULL, "MegaCart 64 KB cartridge", NULL, 28},
		{"CRTT", ITEM_ACTION, NULL, "MegaCart 128 KB cartridge", NULL, 29},
		{"CRTU", ITEM_ACTION, NULL, "MegaCart 256 KB cartridge", NULL, 30},
		{"CRTV", ITEM_ACTION, NULL, "MegaCart 512 KB cartridge", NULL, 31},
		{"CRTW", ITEM_ACTION, NULL, "MegaCart 1 MB cartridge", NULL, 32},
		{"CRTX", ITEM_ACTION, NULL, "Switchable XEGS 32 KB cartridge", NULL, 33},
		{"CRTY", ITEM_ACTION, NULL, "Switchable XEGS 64 KB cartridge", NULL, 34},
		{"CRTZ", ITEM_ACTION, NULL, "Switchable XEGS 128 KB cartridge", NULL, 35},
		{"CRU0", ITEM_ACTION, NULL, "Switchable XEGS 256 KB cartridge", NULL, 36},
		{"CRU1", ITEM_ACTION, NULL, "Switchable XEGS 512 KB cartridge", NULL, 37},
		{"CRU2", ITEM_ACTION, NULL, "Switchable XEGS 1 MB cartridge", NULL, 38},
		{"CRU3", ITEM_ACTION, NULL, "Phoenix 8 KB cartridge", NULL, 39},
		{"CRU4", ITEM_ACTION, NULL, "Blizzard 16 KB cartridge", NULL, 40},
		MENU_END
	};

	int i;
	int option = 0;

	ui_driver->fInit();

	for (i = 1; i <= CART_LAST_SUPPORTED; i++)
		if (cart_kb[i] == k)
			menu_array[i].flags |= ITEM_ENABLED;
		else
			menu_array[i].flags &= ~ITEM_ENABLED;

	option = ui_driver->fSelect("Select Cartridge Type", FALSE, option, menu_array, NULL);

	if (option >= 0 && option <= CART_LAST_SUPPORTED)
		return option;

	return CART_NONE;
}

void CartManagement()
{
	static tMenuItem menu_array[] = {
		{"CRCR", ITEM_ENABLED | ITEM_FILESEL, NULL, "Create Cartridge from ROM image", NULL, 0},
		{"EXCR", ITEM_ENABLED | ITEM_FILESEL, NULL, "Extract ROM image from Cartridge", NULL, 1},
		{"INCR", ITEM_ENABLED | ITEM_FILESEL, NULL, "Insert Cartridge", NULL, 2},
		{"RECR", ITEM_ENABLED | ITEM_ACTION, NULL, "Remove Cartridge", NULL, 3},
		{"PILL", ITEM_ENABLED | ITEM_ACTION, NULL, "Enable PILL Mode", NULL, 4},
		MENU_END
	};

	typedef struct {
		UBYTE id[4];
		UBYTE type[4];
		UBYTE checksum[4];
		UBYTE gash[4];
	} Header;

	int done = FALSE;
	int option = 2;

	if (!curr_cart_dir[0])
		strcpy(curr_cart_dir, atari_rom_dir);

	while (!done) {
		char filename[FILENAME_MAX + 1];
		int ascii;

		option = ui_driver->fSelect("Cartridge Management", FALSE, option, menu_array, &ascii);

		switch (option) {
		case 0:
			if (ui_driver->fGetLoadFilename(curr_cart_dir, filename)) {
				UBYTE *image;
				int nbytes;
				FILE *f;

				f = fopen(filename, "rb");
				if (!f) {
					perror(filename);
					exit(1);
				}
				image = malloc(CART_MAX_SIZE + 1);
				if (image == NULL) {
					fclose(f);
					Aprint("CartManagement: out of memory");
					break;
				}
				nbytes = fread(image, 1, CART_MAX_SIZE + 1, f);
				fclose(f);
				if ((nbytes & 0x3ff) == 0) {
					int type = SelectCartType(nbytes / 1024);
					if (type != CART_NONE) {
						Header header;

						int checksum = CART_Checksum(image, nbytes);

						char fname[FILENAME_SIZE + 1];

						if (!ui_driver->fGetSaveFilename(fname))
							break;

						header.id[0] = 'C';
						header.id[1] = 'A';
						header.id[2] = 'R';
						header.id[3] = 'T';
						header.type[0] = (type >> 24) & 0xff;
						header.type[1] = (type >> 16) & 0xff;
						header.type[2] = (type >> 8) & 0xff;
						header.type[3] = type & 0xff;
						header.checksum[0] = (checksum >> 24) & 0xff;
						header.checksum[1] = (checksum >> 16) & 0xff;
						header.checksum[2] = (checksum >> 8) & 0xff;
						header.checksum[3] = checksum & 0xff;
						header.gash[0] = '\0';
						header.gash[1] = '\0';
						header.gash[2] = '\0';
						header.gash[3] = '\0';

						sprintf(filename, "%s/%s", atari_rom_dir, fname);
						f = fopen(filename, "wb");
						if (f) {
							fwrite(&header, 1, sizeof(header), f);
							fwrite(image, 1, nbytes, f);
							fclose(f);
						}
					}
				}
				free(image);
			}
			break;
		case 1:
			if (ui_driver->fGetLoadFilename(curr_cart_dir, filename)) {
				FILE *f;

				f = fopen(filename, "rb");
				if (f) {
					Header header;
					UBYTE *image;
					char fname[FILENAME_SIZE + 1];
					int nbytes;

					fread(&header, 1, sizeof(header), f);
					if (header.id[0] != 'C' || header.id[1] != 'A' || header.id[2] != 'R' || header.id[3] != 'T') {
						fclose(f);
						ui_driver->fMessage("Not a CART file");
						break;
					}
					image = malloc(CART_MAX_SIZE + 1);
					if (image == NULL) {
						fclose(f);
						Aprint("CartManagement: out of memory");
						break;
					}
					nbytes = fread(image, 1, CART_MAX_SIZE + 1, f);

					fclose(f);

					if (!ui_driver->fGetSaveFilename(fname))
						break;

					sprintf(filename, "%s/%s", atari_rom_dir, fname);

					f = fopen(filename, "wb");
					if (f) {
						fwrite(image, 1, nbytes, f);
						fclose(f);
					}
					free(image);
				}
			}
			break;
		case 2:
			if (ui_driver->fGetLoadFilename(curr_cart_dir, filename)) {
				int r = CART_Insert(filename);
				if (r > 0)
					cart_type = SelectCartType(r);
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
				done = TRUE;
			}
			break;
		case 3:
			CART_Remove();
			Coldstart();
			done = TRUE;
			break;
		case 4:
			EnablePILL();
			Coldstart();
			break;
		default:
			done = TRUE;
			break;
		}
	}
}

void SoundRecording()
{
	static int record_num = 0;
	char buf[128];
	char msg[256];

	if (!IsSoundFileOpen()) {
		sprintf(buf, "%d.raw", record_num);
		if (OpenSoundFile(buf))
			sprintf(msg, "Recording sound to file \"%s\"", buf);
		else
			sprintf(msg, "Can't write to file \"%s\"", buf);
	}
	else {
		CloseSoundFile();
		sprintf(msg, "Recording stopped");
		record_num++;
	}

	ui_driver->fMessage(msg);
}

int RunExe()
{
	char exename[FILENAME_MAX + 1];
	int ret = FALSE;

	if (!curr_exe_dir[0])
		strcpy(curr_exe_dir, atari_exe_dir);
	if (ui_driver->fGetLoadFilename(curr_exe_dir, exename)) {
		ret = BIN_loader(exename);
		if (!ret) {
			/* display log to a window */
		}
	}

	return ret;
}

int LoadTape()
{
	char tapename[FILENAME_MAX + 1];
	int ret = FALSE;

	if (!curr_tape_dir[0])
		strcpy(curr_tape_dir, atari_exe_dir);
	if (ui_driver->fGetLoadFilename(curr_tape_dir, tapename)) {
		ret = CASSETTE_Insert(tapename);
		if (!ret) {
			/* display log to a window */
		}
	}

	return ret;
}

void AtariSettings()
{
	static tMenuItem menu_array[] = {
		{"NBAS", ITEM_ENABLED | ITEM_CHECK, NULL, "Disable BASIC when booting Atari:", NULL, 0},
		{"STRT", ITEM_ENABLED | ITEM_CHECK, NULL, "Boot from tape (hold Start):", NULL, 1},
		{"RTM8", ITEM_ENABLED | ITEM_CHECK, NULL, "Enable R-Time 8:", NULL, 2},
		{"SIOP", ITEM_ENABLED | ITEM_CHECK, NULL, "SIO patch (fast disk access):", NULL, 3},
		{"HDEV", ITEM_ENABLED | ITEM_CHECK, NULL, "H: device (hard disk):", NULL, 4},
		{"PDEV", ITEM_ENABLED | ITEM_CHECK, NULL, "P: device (printer):", NULL, 5},
		{"RDEV", ITEM_ENABLED | ITEM_CHECK, NULL, "R: device (Atari850 via net):", NULL, 6},
		{"UCFG", ITEM_ENABLED | ITEM_ACTION, NULL, "Update configuration file", NULL, 7},
		MENU_END
	};

	int option = 0;

	do {
		if (disable_basic)
			menu_array[0].flags |= ITEM_CHECKED;
		else
			menu_array[0].flags &= ~ITEM_CHECKED;
		if (hold_start_on_reboot)
			menu_array[1].flags |= ITEM_CHECKED;
		else
			menu_array[1].flags &= ~ITEM_CHECKED;
		if (rtime_enabled)
			menu_array[2].flags |= ITEM_CHECKED;
		else
			menu_array[2].flags &= ~ITEM_CHECKED;
		if (enable_sio_patch)
			menu_array[3].flags |= ITEM_CHECKED;
		else
			menu_array[3].flags &= ~ITEM_CHECKED;
		if (enable_h_patch)
			menu_array[4].flags |= ITEM_CHECKED;
		else
			menu_array[4].flags &= ~ITEM_CHECKED;
		if (enable_p_patch)
			menu_array[5].flags |= ITEM_CHECKED;
		else
			menu_array[5].flags &= ~ITEM_CHECKED;
		if (enable_r_patch)
			menu_array[6].flags |= ITEM_CHECKED;
		else
			menu_array[6].flags &= ~ITEM_CHECKED;

		option = ui_driver->fSelect(NULL, TRUE, option, menu_array, NULL);

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
			enable_h_patch = !enable_h_patch;
			break;
		case 5:
			enable_p_patch = !enable_p_patch;
			break;
		case 6:
			enable_r_patch = !enable_r_patch;
			break;
		case 7:
			RtConfigSave();
			ui_driver->fMessage("Configuration file updated");
			break;
		}
	} while (option >= 0);
	Atari800_UpdatePatches();
}

int SaveState()
{
	char statename[FILENAME_MAX];
	char fname[FILENAME_SIZE + 1];

	if (!ui_driver->fGetSaveFilename(fname))
		return 0;

	strcpy(statename, atari_state_dir);
	if (*statename) {
		char last = statename[strlen(statename) - 1];
		if (last != '/' && last != '\\')
#ifdef BACK_SLASH
			strcat(statename, "\\");
#else
			strcat(statename, "/");
#endif
	}
	strcat(statename, fname);

	return SaveAtariState(statename, "wb", TRUE);
}

int LoadState()
{
	char statename[FILENAME_MAX + 1];
	int ret = FALSE;

	if (!curr_state_dir[0])
		strcpy(curr_state_dir, atari_state_dir);
	if (ui_driver->fGetLoadFilename(curr_state_dir, statename))
		ret = ReadAtariState(statename, "rb");

	return ret;
}

void SelectArtifacting()
{
	static tMenuItem menu_array[] = {
		{"ARNO", ITEM_ENABLED | ITEM_ACTION, NULL, "none", NULL, 0},
		{"ARB1", ITEM_ENABLED | ITEM_ACTION, NULL, "blue/brown 1", NULL, 1},
		{"ARB2", ITEM_ENABLED | ITEM_ACTION, NULL, "blue/brown 2", NULL, 2},
		{"ARGT", ITEM_ENABLED | ITEM_ACTION, NULL, "GTIA", NULL, 3},
		{"ARCT", ITEM_ENABLED | ITEM_ACTION, NULL, "CTIA", NULL, 4},
		MENU_END
	};

	int option = global_artif_mode;

	option = ui_driver->fSelect(NULL, TRUE, option, menu_array, NULL);

	if (option >= 0) {
		global_artif_mode = option;
		ANTIC_UpdateArtifacting();
	}
}

int SoundSettings()
{
	int reboot_required = FALSE;

	static tMenuItem menu_array[] = {
		{"HFPO", ITEM_ENABLED | ITEM_CHECK, NULL, "High Fidelity POKEY:", NULL, 0},
		{"STER", ITEM_ENABLED | ITEM_CHECK, NULL, "Dual POKEY (Stereo):", NULL, 1},
		{"CONS", ITEM_ENABLED | ITEM_CHECK, NULL, "Speaker (Key Click):", NULL, 2},
		{"SERI", ITEM_ENABLED | ITEM_CHECK, NULL, "Serial IO Sound    :", NULL, 3},
		MENU_END
	};

	int option = 0;

	do {
		if (enable_new_pokey)
			menu_array[0].flags |= ITEM_CHECKED;
		else
			menu_array[0].flags &= ~ITEM_CHECKED;

#ifdef STEREO_SOUND
		if (stereo_enabled)
			menu_array[1].flags |= ITEM_CHECKED;
		else
#endif
			menu_array[1].flags &= ~ITEM_CHECKED;

#ifdef CONSOLE_SOUND
		if (console_sound_enabled)
			menu_array[2].flags |= ITEM_CHECKED;
		else
#endif
			menu_array[2].flags &= ~ITEM_CHECKED;

#ifdef SERIO_SOUND
		if (serio_sound_enabled)
			menu_array[3].flags |= ITEM_CHECKED;
		else
#endif
			menu_array[3].flags &= ~ITEM_CHECKED;

		option = ui_driver->fSelect(NULL, TRUE, option, menu_array, NULL);

		switch (option) {
		case 0:
			enable_new_pokey = !enable_new_pokey;
			Pokey_DoInit();
			/* According the PokeySnd doc the POKEY switch can occur on
			   a cold-restart only */
			reboot_required = TRUE;
			ui_driver->fMessage("Will reboot to apply the change");
			option = -1;		/* immediate exit from the while loop */
			break;
		case 1:
#ifdef STEREO_SOUND
			stereo_enabled = !stereo_enabled;
#else
			ui_driver->fMessage("Stereo sound support not compiled in");
#endif
			break;
		case 2:
#ifdef CONSOLE_SOUND
			console_sound_enabled = !console_sound_enabled;
#else
			ui_driver->fMessage("Speaker sound support not compiled in");
#endif
			break;
		case 3:
#ifdef SERIO_SOUND
			serio_sound_enabled = !serio_sound_enabled;
#else
			ui_driver->fMessage("SERIO sound support not compiled in");
#endif
			break;
		}
	} while (option >= 0);

	return reboot_required;
}

void Screenshot(int interlaced)
{
	char fname[FILENAME_SIZE + 1];
	if (ui_driver->fGetSaveFilename(fname)) {
		ANTIC_Frame(TRUE);
		Screen_SaveScreenshot(fname, interlaced);
	}
}

void ui(void)
{
	static tMenuItem menu_array[] = {
		{"DISK", ITEM_ENABLED | ITEM_SUBMENU, NULL, "Disk Management", "Alt+D", MENU_DISK},
		{"CART", ITEM_ENABLED | ITEM_SUBMENU, NULL, "Cartridge Management", "Alt+C", MENU_CARTRIDGE},
		{"XBIN", ITEM_ENABLED | ITEM_FILESEL, NULL, "Run BIN file directly", "Alt+R", MENU_RUN},
		{"CASS", ITEM_ENABLED | ITEM_FILESEL, NULL, "Select tape image", NULL, MENU_CASSETTE},
		{"SYST", ITEM_ENABLED | ITEM_SUBMENU, NULL, "Select System", "Alt+Y", MENU_SYSTEM},
		{"SNDS", ITEM_ENABLED | ITEM_ACTION, NULL, "Sound Settings", "Alt+O", MENU_SOUND},
		{"SREC", ITEM_ENABLED | ITEM_ACTION, NULL, "Sound Recording start/stop", "Alt+W", MENU_SOUND_RECORDING},
		{"ARTF", ITEM_ENABLED | ITEM_SUBMENU, NULL, "Artifacting mode", NULL, MENU_ARTIF},
		{"SETT", ITEM_ENABLED | ITEM_SUBMENU, NULL, "Atari Settings", NULL, MENU_SETTINGS},
		{"SAVE", ITEM_ENABLED | ITEM_FILESEL, NULL, "Save State", "Alt+S", MENU_SAVESTATE},
		{"LOAD", ITEM_ENABLED | ITEM_FILESEL, NULL, "Load State", "Alt+L", MENU_LOADSTATE},
#ifdef HAVE_LIBPNG
		{"PCXN", ITEM_ENABLED | ITEM_FILESEL, NULL, "PNG/PCX screenshot", "F10", MENU_PCX},
#else
		{"PCXN", ITEM_ENABLED | ITEM_FILESEL, NULL, "PCX screenshot", "F10", MENU_PCX},
#endif
/*		{ "PCXI", ITEM_ENABLED|ITEM_FILESEL, NULL, "PCX interlaced screenshot",  "Shift+F10",MENU_PCXI }, */
		{"CONT", ITEM_ENABLED | ITEM_ACTION, NULL, "Back to emulated Atari", "Esc", MENU_BACK},
		{"REST", ITEM_ENABLED | ITEM_ACTION, NULL, "Reset (Warm Start)", "F5", MENU_RESETW},
		{"REBT", ITEM_ENABLED | ITEM_ACTION, NULL, "Reboot (Cold Start)", "Shift+F5", MENU_RESETC},
		{"MONI", ITEM_ENABLED | ITEM_ACTION, NULL, "Enter monitor", "F8", MENU_MONITOR},
		{"ABOU", ITEM_ENABLED | ITEM_ACTION, NULL, "About the Emulator", "Alt+A", MENU_ABOUT},
		{"EXIT", ITEM_ENABLED | ITEM_ACTION, NULL, "Exit Emulator", "F9", MENU_EXIT},
		MENU_END
	};

	int option = 0;
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

		if (alt_function < 0) {
			option = ui_driver->fSelect(ATARI_TITLE, FALSE, option, menu_array, NULL);
		}
		else {
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
			if (RunExe())
				done = TRUE;	/* reboot immediately */
			break;
		case MENU_CASSETTE:
			LoadTape();
			break;
		case MENU_SYSTEM:
			SelectSystem();
			break;
		case MENU_ARTIF:
			SelectArtifacting();
			break;
		case MENU_SETTINGS:
			AtariSettings();
			break;
		case MENU_SOUND:
			if (SoundSettings()) {
				Coldstart();
				done = TRUE;	/* reboot immediately */
			}
			break;
		case MENU_SOUND_RECORDING:
			SoundRecording();
			break;
		case MENU_SAVESTATE:
			SaveState();
			break;
		case MENU_LOADSTATE:
			LoadState();
			break;
		case MENU_PCX:
			Screenshot(0);
			break;
		case MENU_PCXI:
			Screenshot(1);
			break;
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
			ui_driver->fAboutBox();
			break;
		case MENU_MONITOR:
			if (Atari_Exit(1)) {
				done = TRUE;
				break;
			}
			/* if 'quit' typed in monitor, exit emulator */
		case MENU_EXIT:
			Atari800_Exit(0);
			exit(0);
		}
	}
	/* Sound_Active(TRUE); */
	ui_is_active = FALSE;
	while (Atari_Keyboard() != AKEY_NONE);	/* flush keypresses */
}


#ifdef CRASH_MENU

int CrashMenu()
{
	static tMenuItem menu_array[] = {
		{"REST", ITEM_ENABLED | ITEM_ACTION, NULL, "Reset (Warm Start)", "F5", 0},
		{"REBT", ITEM_ENABLED | ITEM_ACTION, NULL, "Reboot (Cold Start)", "Shift+F5", 1},
		{"MENU", ITEM_ENABLED | ITEM_SUBMENU, NULL, "Menu", "F1", 2},
		{"MONI", ITEM_ENABLED | ITEM_ACTION, NULL, "Enter monitor", "F8", 3},
		{"CONT", ITEM_ENABLED | ITEM_ACTION, NULL, "Continue after CIM", "Esc", 4},
		{"EXIT", ITEM_ENABLED | ITEM_ACTION, NULL, "Exit Emulator", "F9", 5},
		MENU_END
	};

	int option = 0;
	char bf[80];				/* CIM info */

	while (1) {
		sprintf(bf, "!!! The Atari computer has crashed !!!\nCode $%02X (CIM) at address $%04X", crash_code, crash_address);

		option = ui_driver->fSelect(bf, FALSE, option, menu_array, NULL);

		switch (option) {
		case 0:				/* Power On Reset */
			alt_function = MENU_RESETW;
			return FALSE;
		case 1:				/* Power Off Reset */
			alt_function = MENU_RESETC;
			return FALSE;
		case 2:				/* Menu */
			return FALSE;
		case 3:				/* Monitor */
			alt_function = MENU_MONITOR;
			return FALSE;
		case -2:
		case -1:				/* ESC key */
		case 4:				/* Continue after CIM */
			regPC = crash_afterCIM;
			return TRUE;
		case 5:				/* Exit */
			alt_function = MENU_EXIT;
			return FALSE;
		}
	}
	return FALSE;
}
#endif


/* Inspired by LNG (lng.sourceforge.net) */
void MakeBlankDisk(FILE * setFile)
{

	unsigned long sectorCnt = 0;
	unsigned long paras = 0;
	struct ATR_Header hdr;
	int fileSize = 0;
	typedef unsigned char byte;
	size_t padding;

	sectorCnt = (unsigned short) (127L / 128L + 3L);
	paras = sectorCnt * 8;
	memset(&hdr, 0, sizeof(hdr));
	hdr.magic1 = (byte) 0x96;
	hdr.magic2 = (byte) 0x02;
	hdr.seccountlo = (byte) (paras & 0xFF);
	hdr.seccounthi = (byte) ((paras >> 8) & 0xFF);
	hdr.hiseccountlo = (byte) ((paras >> 16) & 0xFF);
	hdr.secsizelo = (byte) 128;

	fwrite(&hdr, 1, sizeof(hdr), setFile);

	bootData[9] = (byte) (fileSize & 0xFF);
	bootData[10] = (byte) ((fileSize >> 8) & 0xFF);
	bootData[11] = (byte) ((fileSize >> 16) & 0xFF);
	bootData[12] = 0;

	fwrite(bootData, 1, 384, setFile);

	padding = (size_t) ((sectorCnt - 3) * 128 - fileSize);
	if (padding)
		fwrite(bootData, 1, padding, setFile);
}

/*
$Log$
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

/*
vim:ts=4:sw=4:
*/

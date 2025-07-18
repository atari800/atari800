/*
 * ui.c - main user interface
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2015 Atari800 development team (see DOC/CREDITS)
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

#define _POSIX_C_SOURCE 200112L /* for snprintf */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* free() */

#include "afile.h"
#include "antic.h"
#include "artifact.h"
#include "atari.h"
#include "binload.h"
#include "cartridge.h"
#include "cassette.h"
#include "util.h"
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
#endif /* NTSC_FILTER */
#include "gtia.h"
#include "input.h"
#include "akey.h"
#include "log.h"
#include "memory.h"
#ifdef PAL_BLENDING
#include "pal_blending.h"
#endif /* PAL_BLENDING */
#include "platform.h"
#include "rtime.h"
#include "screen.h"
#include "sio.h"
#include "statesav.h"
#include "sysrom.h"
#include "ui.h"
#include "ui_basic.h"
#ifdef XEP80_EMULATION
#include "xep80.h"
#endif /* XEP80_EMULATION */
#ifdef PBI_PROTO80
#include "pbi_proto80.h"
#endif /* PBI_PROTO80 */
#ifdef AF80
#include "af80.h"
#endif /* AF80 */
#ifdef BIT3
#include "bit3.h"
#endif /* BIT3 */
#ifdef SOUND
#include "pokeysnd.h"
#include "sound.h"
#endif /* SOUND */
#if defined(AUDIO_RECORDING) || defined(VIDEO_RECORDING)
#include "file_export.h"
#endif /* defined(SOUND) || defined(VIDEO_RECORDING) */
#if SUPPORTS_CHANGE_VIDEOMODE
#include "videomode.h"
#endif /* SUPPORTS_CHANGE_VIDEOMODE */
#if GUI_SDL
#include "sdl/video.h"
#include "sdl/video_sw.h"
#include "sdl/input.h"
#if HAVE_OPENGL
#include "sdl/video_gl.h"
#endif /* HAVE_OPENGL */
#endif /* GUI_SDL */

#ifdef _WIN32_WCE
extern int smooth_filter;
extern int filter_available;
extern int virtual_joystick;
extern void AboutPocketAtari(void);
#endif /* _WIN32_WCE */

#ifdef DREAMCAST
extern int db_mode;
extern int screen_tv_mode;
extern int emulate_paddles;
extern void JoystickConfiguration(void);
extern void ButtonConfiguration(void);
extern void AboutAtariDC(void);
extern void ScreenPositionConfiguration(void);
extern void update_vidmode(void);
extern void update_screen_updater(void);
#ifdef HZ_TEST
extern void do_hz_test(void);
#endif /* HZ_TEST */
#endif /* DREAMCAST */

#ifdef RPI
extern int op_filtering;
extern float op_zoom;
#endif /* RPI */

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

static UI_tMenuItem *FindMenuItem(UI_tMenuItem *mip, int option)
{
	while (mip->retval != option) {
		if (mip->flags == UI_ITEM_END) return NULL;
		mip++;
	}
	return mip;
}

static void SetItemChecked(UI_tMenuItem *mip, int option, int checked)
{
	UI_tMenuItem* item = FindMenuItem(mip, option);
	if (item) item->flags = checked ? (UI_ITEM_CHECK | UI_ITEM_CHECKED) : UI_ITEM_CHECK;
}

static void FilenameMessage(const char *format, const char *filename)
{
	char msg[FILENAME_MAX + 30];
	snprintf(msg, sizeof(msg), format, filename);
	UI_driver->fMessage(msg, 1);
}

static const char * const cant_load = "Can't load \"%s\"";
static const char * const cant_save = "Can't save \"%s\"";
static const char * const created = "Created \"%s\"";

#define CantLoad(filename) FilenameMessage(cant_load, filename)
#define CantSave(filename) FilenameMessage(cant_save, filename)
#define Created(filename) FilenameMessage(created, filename)

/* Callback function that writes a text label to *LABEL, for use by
   the Select Mosaic RAM slider. */
static void MosaicSliderLabel(char *label, int value, void *user_data)
{
	sprintf(label, "%i KB", value * 4); /* WARNING: No more that 10 chars! */
}

static void SystemSettings(void)
{
	static UI_tMenuItem ram800_menu_array[] = {
		UI_MENU_ACTION(8, "8 KB"),
		UI_MENU_ACTION(16, "16 KB"),
		UI_MENU_ACTION(24, "24 KB"),
		UI_MENU_ACTION(32, "32 KB"),
		UI_MENU_ACTION(40, "40 KB"),
		UI_MENU_ACTION(48, "48 KB"),
		UI_MENU_ACTION(52, "52 KB"),
		UI_MENU_END
	};

	enum { MOSAIC_OTHER = 65 }; /* must be a value that's illegal for MEMORY_mosaic_num_banks */
	static UI_tMenuItem mosaic_ram_menu_array[] = {
		UI_MENU_ACTION(0, "Disabled"),
		UI_MENU_ACTION(4, "1 64K RAM Select board (16 KB)"),
		UI_MENU_ACTION(20, "2 64K RAM Select boards (80 KB)"),
		UI_MENU_ACTION(36, "3 64K RAM Select boards (144 KB)"),
		UI_MENU_ACTION(MOSAIC_OTHER, "Other"),
		UI_MENU_END
	};
	static UI_tMenuItem axlon_ram_menu_array[] = {
		UI_MENU_ACTION(0, "Disabled"),
		UI_MENU_ACTION(8, "128 KB"),
		UI_MENU_ACTION(16, "256 KB"),
		UI_MENU_ACTION(32, "512 KB"),
		UI_MENU_ACTION(64, "1 MB"),
		UI_MENU_ACTION(128, "2 MB"),
		UI_MENU_ACTION(256, "4 MB"),
		UI_MENU_END
	};
	static UI_tMenuItem ramxl_menu_array[] = {
		UI_MENU_ACTION(16, "16 KB"),
		UI_MENU_ACTION(32, "32 KB"),
		UI_MENU_ACTION(48, "48 KB"),
		UI_MENU_ACTION(64, "64 KB"),
		UI_MENU_ACTION(128, "128 KB"),
		UI_MENU_ACTION(192, "192 KB"),
		UI_MENU_ACTION(MEMORY_RAM_320_RAMBO, "320 KB (Rambo)"),
		UI_MENU_ACTION(MEMORY_RAM_320_COMPY_SHOP, "320 KB (Compy-Shop)"),
		UI_MENU_ACTION(576, "576 KB"),
		UI_MENU_ACTION(1088, "1088 KB"),
		UI_MENU_END
	};
	static UI_tMenuItem os800_menu_array[] = {
		UI_MENU_ACTION(SYSROM_AUTO, "Choose automatically"),
		UI_MENU_ACTION(SYSROM_A_NTSC, "Rev. A NTSC"),
		UI_MENU_ACTION(SYSROM_A_PAL, "Rev. A PAL"),
		UI_MENU_ACTION(SYSROM_B_NTSC, "Rev. B NTSC"),
		UI_MENU_ACTION(SYSROM_800_CUSTOM, "Custom"),
#if EMUOS_ALTIRRA
		UI_MENU_ACTION(SYSROM_ALTIRRA_800, "AltirraOS"),
#endif /* EMUOS_ALTIRRA */
		UI_MENU_END
	};
	static UI_tMenuItem osxl_menu_array[] = {
		UI_MENU_ACTION(SYSROM_AUTO, "Choose automatically"),
		UI_MENU_ACTION(SYSROM_AA00R10, "AA00 Rev. 10"),
		UI_MENU_ACTION(SYSROM_AA01R11, "AA01 Rev. 11"),
		UI_MENU_ACTION(SYSROM_BB00R1, "BB00 Rev. 1"),
		UI_MENU_ACTION(SYSROM_BB01R2, "BB01 Rev. 2"),
		UI_MENU_ACTION(SYSROM_BB02R3, "BB02 Rev. 3"),
		UI_MENU_ACTION(SYSROM_BB02R3V4, "BB02 Rev. 3 Ver. 4"),
		UI_MENU_ACTION(SYSROM_CC01R4, "CC01 Rev. 4"),
		UI_MENU_ACTION(SYSROM_BB01R3, "BB01 Rev. 3"),
		UI_MENU_ACTION(SYSROM_BB01R4_OS, "BB01 Rev. 4"),
		UI_MENU_ACTION(SYSROM_BB01R59, "BB01 Rev. 59"),
		UI_MENU_ACTION(SYSROM_BB01R59A, "BB01 Rev. 59 alt."),
		UI_MENU_ACTION(SYSROM_XL_CUSTOM, "Custom"),
#if EMUOS_ALTIRRA
		UI_MENU_ACTION(SYSROM_ALTIRRA_XL, "AltirraOS"),
#endif /* EMUOS_ALTIRRA */
		UI_MENU_END
	};
	static UI_tMenuItem os5200_menu_array[] = {
		UI_MENU_ACTION(SYSROM_AUTO, "Choose automatically"),
		UI_MENU_ACTION(SYSROM_5200, "Original"),
		UI_MENU_ACTION(SYSROM_5200A, "Rev. A"),
		UI_MENU_ACTION(SYSROM_5200_CUSTOM, "Custom"),
#if EMUOS_ALTIRRA
		UI_MENU_ACTION(SYSROM_ALTIRRA_5200, "AltirraOS"),
#endif /* EMUOS_ALTIRRA */
		UI_MENU_END
	};
	static UI_tMenuItem * const os_menu_arrays[Atari800_MACHINE_SIZE] = {
		os800_menu_array,
		osxl_menu_array,
		os5200_menu_array
	};
	static UI_tMenuItem basic_menu_array[] = {
		UI_MENU_ACTION(SYSROM_AUTO, "Choose automatically"),
		UI_MENU_ACTION(SYSROM_BASIC_A, "Rev. A"),
		UI_MENU_ACTION(SYSROM_BASIC_B, "Rev. B"),
		UI_MENU_ACTION(SYSROM_BASIC_C, "Rev. C"),
		UI_MENU_ACTION(SYSROM_BASIC_CUSTOM, "Custom"),
#if EMUOS_ALTIRRA
		UI_MENU_ACTION(SYSROM_ALTIRRA_BASIC, "Altirra BASIC"),
#endif /* EMUOS_ALTIRRA */
		UI_MENU_END
	};
	static UI_tMenuItem xegame_menu_array[] = {
		UI_MENU_ACTION(0, "None"),
		UI_MENU_ACTION(SYSROM_AUTO, "Choose automatically"),
		UI_MENU_ACTION(SYSROM_XEGAME, "Missile Command"),
		UI_MENU_ACTION(SYSROM_XEGAME_CUSTOM, "Custom"),
		UI_MENU_END
	};
	static struct {
		int type;
		int ram;
		int basic;
		int leds;
		int f_keys;
		int jumper;
		int game;
		int keyboard;
	} const machine[] = {
		{ Atari800_MACHINE_800, 16, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE },
		{ Atari800_MACHINE_800, 48, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE },
		{ Atari800_MACHINE_XLXE, 64, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE },
		{ Atari800_MACHINE_XLXE, 16, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
		{ Atari800_MACHINE_XLXE, 64, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
		{ Atari800_MACHINE_XLXE, 128, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE },
		{ Atari800_MACHINE_XLXE, 64, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE },
		{ Atari800_MACHINE_5200, 16, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE }
	};
	static UI_tMenuItem machine_menu_array[] = {
		UI_MENU_ACTION(0, "Atari 400 (16 KB)"),
		UI_MENU_ACTION(1, "Atari 800 (48 KB)"),
		UI_MENU_ACTION(2, "Atari 1200XL (64 KB)"),
		UI_MENU_ACTION(3, "Atari 600XL (16 KB)"),
		UI_MENU_ACTION(4, "Atari 800XL (64 KB)"),
		UI_MENU_ACTION(5, "Atari 130XE (128 KB)"),
		UI_MENU_ACTION(6, "Atari XEGS (64 KB)"),
		UI_MENU_ACTION(7, "Atari 5200 (16 KB)"),
		UI_MENU_END
	};
	enum { N_MACHINES = (int) (sizeof(machine) / sizeof(machine[0])) };

	static UI_tMenuItem menu_array[] = {
		UI_MENU_SUBMENU_SUFFIX(0, "Machine:", NULL),
		UI_MENU_SUBMENU_SUFFIX(1, "OS version:", NULL),
		UI_MENU_ACTION(2, "BASIC:"),
		UI_MENU_SUBMENU_SUFFIX(3, "BASIC version:", NULL),
		UI_MENU_SUBMENU_SUFFIX(4, "XEGS game:", NULL),
		UI_MENU_SUBMENU_SUFFIX(5, "RAM size:", NULL),
		UI_MENU_ACTION(6, "Video system:"),
		UI_MENU_SUBMENU_SUFFIX(7, "Mosaic RAM:", NULL),
		UI_MENU_SUBMENU_SUFFIX(8, "Axlon RAMDisk:", NULL),
		UI_MENU_ACTION(9, "Axlon RAMDisk page $0F shadow:"),
		UI_MENU_SUBMENU(10, "1200XL keyboard LEDs:"),
		UI_MENU_ACTION(11, "1200XL F1-F4 keys:"),
		UI_MENU_ACTION(12, "1200XL option jumper J1:"),
		UI_MENU_ACTION(13, "Keyboard:"),
		UI_MENU_ACTION(14, "MapRAM:"),
		UI_MENU_END
	};

	/* Size must be long enough to store "<longest OS label> (auto)". */
	char default_os_label[26];
	/* Size must be long enough to store "<longest BASIC label> (auto)". */
	char default_basic_label[21];
	/* Size must be long enough to store "<longest XEGAME label> (auto)". */
	char default_xegame_label[23];
	char mosaic_label[7]; /* Fits "256 KB" */

	int option = 0;
	int option2 = 0;
	int new_tv_mode = Atari800_tv_mode;
	int need_initialise = FALSE;

	for (;;) {
		int sys_id;
		/* Set label for the "Machine" action. */
		for (sys_id = 0; sys_id < N_MACHINES; ++sys_id) {
			if (Atari800_machine_type == machine[sys_id].type
			    && MEMORY_ram_size == machine[sys_id].ram
			    && Atari800_builtin_basic == machine[sys_id].basic
			    && Atari800_keyboard_leds == machine[sys_id].leds
			    && Atari800_f_keys == machine[sys_id].f_keys
			    && (machine[sys_id].jumper || !Atari800_jumper)
			    && Atari800_builtin_game == machine[sys_id].game
			    && (machine[sys_id].keyboard || !Atari800_keyboard_detached)) {
				menu_array[0].suffix = machine_menu_array[sys_id].item;
				break;
			}
		}
		if (sys_id >= N_MACHINES) { /* Loop ended without break */
			if (Atari800_machine_type == Atari800_MACHINE_XLXE)
				menu_array[0].suffix = "Custom XL/XE";
			else
				menu_array[0].suffix = "Custom 400/800";
		}

		/* Set label for the "OS version" action. */
		if (SYSROM_os_versions[Atari800_machine_type] == SYSROM_AUTO) {
			int auto_os = SYSROM_AutoChooseOS(Atari800_machine_type, MEMORY_ram_size, new_tv_mode);
			if (auto_os == -1)
				menu_array[1].suffix = "ROM missing";
			else {
				sprintf(default_os_label, "%s (auto)", FindMenuItem(os_menu_arrays[Atari800_machine_type], auto_os)->item);
				menu_array[1].suffix = default_os_label;
			}
		}
		else if (SYSROM_roms[SYSROM_os_versions[Atari800_machine_type]].data == NULL
		         && SYSROM_roms[SYSROM_os_versions[Atari800_machine_type]].filename[0] == '\0')
			menu_array[1].suffix = "ROM missing";
		else
			menu_array[1].suffix = FindMenuItem(os_menu_arrays[Atari800_machine_type], SYSROM_os_versions[Atari800_machine_type])->item;

		/* Set label for the "BASIC" action. */
		menu_array[2].suffix = Atari800_machine_type == Atari800_MACHINE_5200
		                       ? "N/A"
		                       : Atari800_builtin_basic ? "built in" : "external";

		/* Set label for the "BASIC version" action. */
		if (Atari800_machine_type == Atari800_MACHINE_5200)
			menu_array[3].suffix = "N/A";
		else {
			if (SYSROM_basic_version == SYSROM_AUTO) {
				int auto_basic = SYSROM_AutoChooseBASIC();
				if (auto_basic == -1)
					menu_array[3].suffix = "ROM missing";
				else {
					sprintf(default_basic_label, "%s (auto)", FindMenuItem(basic_menu_array, auto_basic)->item);
					menu_array[3].suffix = default_basic_label;
				}
			}
			else if (SYSROM_roms[SYSROM_basic_version].data == NULL
		             && SYSROM_roms[SYSROM_basic_version].filename[0] == '\0')
				menu_array[3].suffix = "ROM missing";
			else {
				menu_array[3].suffix = FindMenuItem(basic_menu_array, SYSROM_basic_version)->item;
			}
		}

		/* Set label for the "Builtin XEGS game" action. */
		if (Atari800_machine_type != Atari800_MACHINE_XLXE)
			menu_array[4].suffix = "N/A";
		else if (Atari800_builtin_game) {
			if (SYSROM_xegame_version == SYSROM_AUTO) {
				int auto_xegame = SYSROM_AutoChooseXEGame();
				if (auto_xegame == -1)
					menu_array[4].suffix = "ROM missing";
				else {
					sprintf(default_xegame_label, "%s (auto)", FindMenuItem(xegame_menu_array, auto_xegame)->item);
					menu_array[4].suffix = default_xegame_label;
				}
			}
			else if (SYSROM_roms[SYSROM_xegame_version].data == NULL
		             && SYSROM_roms[SYSROM_xegame_version].filename[0] == '\0')
				menu_array[4].suffix = "ROM missing";
			else
				menu_array[4].suffix = FindMenuItem(xegame_menu_array, SYSROM_xegame_version)->item;
		}
		else
			menu_array[4].suffix = xegame_menu_array[0].item;

		/* Set label for the "RAM size" action. */
		switch (Atari800_machine_type) {
		case Atari800_MACHINE_800:
			menu_array[5].suffix = FindMenuItem(ram800_menu_array, MEMORY_ram_size)->item;
			break;
		case Atari800_MACHINE_XLXE:
			menu_array[5].suffix = FindMenuItem(ramxl_menu_array, MEMORY_ram_size)->item;
			break;
		case Atari800_MACHINE_5200:
			menu_array[5].suffix = "16 KB";
			break;
		}

		menu_array[6].suffix = (new_tv_mode == Atari800_TV_PAL) ? "PAL" : "NTSC";

		/* Set label for the "Mosaic" action. */
		if (Atari800_machine_type == Atari800_MACHINE_800) {
			if (MEMORY_mosaic_num_banks == 0)
				menu_array[7].suffix = mosaic_ram_menu_array[0].item;
			else {
				sprintf(mosaic_label, "%i KB", MEMORY_mosaic_num_banks * 4);
				menu_array[7].suffix = mosaic_label;
			}
		}
		else
			menu_array[7].suffix = "N/A";

		/* Set label for the "Axlon RAM" action. */
		menu_array[8].suffix = Atari800_machine_type != Atari800_MACHINE_800
		                       ? "N/A"
		                       : FindMenuItem(axlon_ram_menu_array, MEMORY_axlon_num_banks)->item;

		/* Set label for the "Axlon $0F shadow" action. */
		menu_array[9].suffix = Atari800_machine_type != Atari800_MACHINE_800
		                       ? "N/A"
		                       : MEMORY_axlon_0f_mirror ? "on" : "off";

		/* Set label for the "keyboard LEDs" action. */
		menu_array[10].suffix = Atari800_machine_type != Atari800_MACHINE_XLXE
		                        ? "N/A"
		                        : Atari800_keyboard_leds ? "Yes" : "No";

		/* Set label for the "F keys" action. */
		menu_array[11].suffix = Atari800_machine_type != Atari800_MACHINE_XLXE
		                        ? "N/A"
		                        : Atari800_f_keys ? "Yes" : "No";

		/* Set label for the "1200XL option jumper" action. */
		menu_array[12].suffix = Atari800_machine_type != Atari800_MACHINE_XLXE ? "N/A" :
		                        Atari800_jumper ? "installed" : "none";

		/* Set label for the "XEGS keyboard" action. */
		menu_array[13].suffix = Atari800_machine_type != Atari800_MACHINE_XLXE ? "N/A" :
		                        Atari800_keyboard_detached ? "detached (XEGS)" : "integrated/attached";

		/* Set label for the "XL/XE MapRAM" action. */
		menu_array[14].suffix = (Atari800_machine_type != Atari800_MACHINE_XLXE || MEMORY_ram_size < 20)
		                        ? "N/A"
		                        : MEMORY_enable_mapram ? "Yes" : "No";

		option = UI_driver->fSelect("System Settings", 0, option, menu_array, NULL);
		switch (option) {
		case 0:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, sys_id, machine_menu_array, NULL);
			if (option2 >= 0) {
				Atari800_machine_type = machine[option2].type;
				MEMORY_ram_size = machine[option2].ram;
				Atari800_builtin_basic = machine[option2].basic;
				Atari800_keyboard_leds = machine[option2].leds;
				Atari800_f_keys = machine[option2].f_keys;
				if (!machine[option2].jumper)
					Atari800_jumper = FALSE;
				Atari800_builtin_game = machine[option2].game;
				if (!machine[option2].keyboard)
					Atari800_keyboard_detached = FALSE;
				need_initialise = TRUE;
			}
			break;
		case 1:
			{
				int rom_available = FALSE;
				/* Start from index 1, to skip the "Choose automatically" option,
				   as it can never be hidden. */
				UI_tMenuItem *menu_ptr = os_menu_arrays[Atari800_machine_type] + 1;
				do {
					if (SYSROM_roms[menu_ptr->retval].data != NULL
					    || SYSROM_roms[menu_ptr->retval].filename[0] != '\0') {
						menu_ptr->flags = UI_ITEM_ACTION;
						rom_available = TRUE;
					}
					else
						menu_ptr->flags = UI_ITEM_HIDDEN;
				} while ((++menu_ptr)->flags != UI_ITEM_END);
				if (!rom_available)
					UI_driver->fMessage("No OS version available, ROMs missing", 1);
				else {
					option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, SYSROM_os_versions[Atari800_machine_type], os_menu_arrays[Atari800_machine_type], NULL);
					if (option2 >= 0) {
						SYSROM_os_versions[Atari800_machine_type] = option2;
						need_initialise = TRUE;
					}
				}
			}
			break;
		case 2:
			if (Atari800_machine_type == Atari800_MACHINE_XLXE) {
				Atari800_builtin_basic = !Atari800_builtin_basic;
				need_initialise = TRUE;
			}
			break;
		case 3:
			if (Atari800_machine_type != Atari800_MACHINE_5200) {
				int rom_available = FALSE;
				/* Start from index 1, to skip the "Choose automatically" option,
				   as it can never be hidden. */
				UI_tMenuItem *menu_ptr = basic_menu_array + 1;
				do {
					if (SYSROM_roms[menu_ptr->retval].data != NULL
					    || SYSROM_roms[menu_ptr->retval].filename[0] != '\0') {
						menu_ptr->flags = UI_ITEM_ACTION;
						rom_available = TRUE;
					}
					else
						menu_ptr->flags = UI_ITEM_HIDDEN;
				} while ((++menu_ptr)->flags != UI_ITEM_END);

				if (!rom_available)
					UI_driver->fMessage("No BASIC available, ROMs missing", 1);
				else {
					option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, SYSROM_basic_version, basic_menu_array, NULL);
					if (option2 >= 0) {
						SYSROM_basic_version = option2;
						need_initialise = TRUE;
					}
				}
			}
			break;
		case 4:
			if (Atari800_machine_type == Atari800_MACHINE_XLXE) {
				/* Start from index 2, to skip the "None" and "Choose automatically" options,
				   as they can never be hidden. */
				UI_tMenuItem *menu_ptr = xegame_menu_array + 2;
				do {
					if (SYSROM_roms[menu_ptr->retval].data != NULL
					    || SYSROM_roms[menu_ptr->retval].filename[0] != '\0') {
						menu_ptr->flags = UI_ITEM_ACTION;
					}
					else
						menu_ptr->flags = UI_ITEM_HIDDEN;
				} while ((++menu_ptr)->flags != UI_ITEM_END);

				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, Atari800_builtin_game ? SYSROM_xegame_version : 0, xegame_menu_array, NULL);
				if (option2 >= 0) {
					if (option2 > 0) {
						Atari800_builtin_game = TRUE;
						SYSROM_xegame_version = option2;
					}
					else
						Atari800_builtin_game = FALSE;
					need_initialise = TRUE;
				}
			}
			break;
		case 5:
			{
				UI_tMenuItem *menu_ptr;
				switch (Atari800_machine_type) {
				case Atari800_MACHINE_5200:
					goto leave;
				case Atari800_MACHINE_800:
					menu_ptr = ram800_menu_array;
					break;
				default: /* Atari800_MACHINE_XLXE */
					menu_ptr = ramxl_menu_array;
					break;
				}
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, MEMORY_ram_size, menu_ptr, NULL);
				if (option2 >= 0) {
					MEMORY_ram_size = option2;
					need_initialise = TRUE;
				}
			}
			leave:
			break;
		case 6:
			new_tv_mode = (new_tv_mode == Atari800_TV_PAL) ? Atari800_TV_NTSC : Atari800_TV_PAL;
			break;
		case 7:
			if (Atari800_machine_type == Atari800_MACHINE_800) {
				if (MEMORY_mosaic_num_banks == 0 || MEMORY_mosaic_num_banks == 4 || MEMORY_mosaic_num_banks == 20 || MEMORY_mosaic_num_banks == 36)
					option2 = MEMORY_mosaic_num_banks;
				else
					option2 = MOSAIC_OTHER;
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, option2, mosaic_ram_menu_array, NULL);
				if (option2 >= 0) {
					if (option2 == MOSAIC_OTHER) {
						int offset = 0;
						int value = UI_driver->fSelectSlider("Select amount of Mosaic RAM",
						                      MEMORY_mosaic_num_banks,
						                      64, &MosaicSliderLabel, &offset);
						if (value != -1) {
							MEMORY_mosaic_num_banks = value;
						}
					}
					else
						MEMORY_mosaic_num_banks = option2;
					if (option2 > 0)
						/* Can't have both Mosaic and Axlon active together. */
						MEMORY_axlon_num_banks = 0;
					need_initialise = TRUE;
				}
			}
			break;
		case 8:
			if (Atari800_machine_type == Atari800_MACHINE_800) {
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, MEMORY_axlon_num_banks, axlon_ram_menu_array, NULL);
				if (option2 >= 0) {
					MEMORY_axlon_num_banks = option2;
					if (option2 > 0)
						/* Can't have both Mosaic and Axlon active together. */
						MEMORY_mosaic_num_banks = 0;
					need_initialise = TRUE;
				}
			}
			break;
		case 9:
			if (Atari800_machine_type == Atari800_MACHINE_800) {
				MEMORY_axlon_0f_mirror = !MEMORY_axlon_0f_mirror;
				need_initialise = TRUE;
			}
			break;
		case 10:
			if (Atari800_machine_type == Atari800_MACHINE_XLXE)
				Atari800_keyboard_leds = !Atari800_keyboard_leds;
			break;
		case 11:
			if (Atari800_machine_type == Atari800_MACHINE_XLXE)
				Atari800_f_keys = !Atari800_f_keys;
			break;
		case 12:
			if (Atari800_machine_type == Atari800_MACHINE_XLXE) {
				Atari800_jumper = !Atari800_jumper;
				Atari800_UpdateJumper();
			}
			break;
		case 13:
			if (Atari800_machine_type == Atari800_MACHINE_XLXE) {
				Atari800_keyboard_detached = !Atari800_keyboard_detached;
				Atari800_UpdateKeyboardDetached();
			}
			break;
		case 14:
			if (Atari800_machine_type == Atari800_MACHINE_XLXE && MEMORY_ram_size > 20) {
				MEMORY_enable_mapram = !MEMORY_enable_mapram;
				need_initialise = TRUE;
			}
			break;
		default:
			if (new_tv_mode != Atari800_tv_mode) {
				Atari800_SetTVMode(new_tv_mode);
				need_initialise = TRUE;
			}
			if (need_initialise)
				Atari800_InitialiseMachine();
			return;
		}
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

int UI_show_hidden_files = FALSE;

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
		UI_MENU_CHECK(13, "Show hidden files/directories:"),
		UI_MENU_END
	};

	int dsknum = 0;

	for (;;) {
		static char disk_filename[FILENAME_MAX];
		static char set_filename[FILENAME_MAX];
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

		SetItemChecked(menu_array, 13, UI_show_hidden_files);

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
					if (fgets(filename, FILENAME_MAX, fp) != NULL) {
						Util_chomp(filename);
						if (strcmp(filename, "Empty") != 0 && strcmp(filename, "Off") != 0)
							SIO_Mount(i + 1, filename, FALSE);
					}
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
		case 13:
			UI_show_hidden_files = !UI_show_hidden_files;
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
	UI_tMenuItem menu_array[CARTRIDGE_TYPE_COUNT] = { 0 };
	int cart_entry;
	int menu_entry = 0;
	int option = 0;

	UI_driver->fInit();

	for (cart_entry = 1; cart_entry < CARTRIDGE_TYPE_COUNT; cart_entry++) {
		if (CARTRIDGES[cart_entry].kb == k) {
			menu_array[menu_entry].flags = UI_ITEM_ACTION;
			menu_array[menu_entry].retval = cart_entry;
			menu_array[menu_entry].item = CARTRIDGES[cart_entry].description;
			menu_entry++;
	    	}
	}
		
	if (menu_entry == 0)
		return CARTRIDGE_NONE;

	/* Terminate menu_array, but do it by hand */
	menu_array[menu_entry].flags = UI_ITEM_END;

	option = UI_driver->fSelect("Select Cartridge Type", 0, option, menu_array, NULL);
	if (option > 0)
		return option;

	return CARTRIDGE_NONE;
}

int UI_SelectCartTypeBetween(int *types)
{
	UI_tMenuItem menu_array[CARTRIDGE_TYPE_COUNT] = { 0 };
	int cart_entry;
	int menu_entry = 0;
	int option = 0;

	UI_driver->fInit();

	for (cart_entry = 1; cart_entry < CARTRIDGE_TYPE_COUNT; cart_entry++) {
		if (cart_entry == types[menu_entry]) {
			menu_array[menu_entry].flags = UI_ITEM_ACTION;
			menu_array[menu_entry].retval = cart_entry;
			menu_array[menu_entry].item = CARTRIDGES[cart_entry].description;
			menu_entry++;
	    	}
	}
	
	if (menu_entry == 0)
		return CARTRIDGE_NONE;

	/* Terminate menu_array, but do it by hand */
	menu_array[menu_entry].flags = UI_ITEM_END;

	option = UI_driver->fSelect("Select Cartridge Type", 0, option, menu_array, NULL);
	if (option > 0)
		return option;

	return CARTRIDGE_NONE;
}

static void CartManagement(void)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL(0, "Create Cartridge from ROM image"),
		UI_MENU_FILESEL(1, "Extract ROM image from Cartridge"),
		UI_MENU_FILESEL_PREFIX_TIP(2, "Cartridge:", NULL, NULL),
		UI_MENU_FILESEL_PREFIX_TIP(3, "Piggyback:", NULL, NULL),
		UI_MENU_CHECK(4, "Ram-Cart R/W switch:"),
		UI_MENU_ACTION(5, "Ram-Cart P1 switch:"),
		UI_MENU_ACTION(6, "Ram-Cart P2 switch:"),
		UI_MENU_ACTION(7, "Ram-Cart ABC jumpers:"),
		UI_MENU_CHECK(8, "Ram-Cart A switch:"),
		UI_MENU_CHECK(9, "Ram-Cart B switch:"),
		UI_MENU_CHECK(10, "Ram-Cart C switch:"),
		UI_MENU_CHECK(11, "Ram-Cart 1/2M switch:"), /* or "Ram-Cart D switch" */
		UI_MENU_CHECK(12, "Ram-Cart 2/4M switch:"),
		UI_MENU_ACTION(13, "Ram-Cart address decoder:"),
		UI_MENU_ACTION(14, "Ram-Cart control register:"),
		UI_MENU_ACTION(15, "Ram-Cart Reset"),
		UI_MENU_CHECK(16, "Reboot after cartridge change:"),
		UI_MENU_FILESEL(17, "Make blank Cartridge"),
		UI_MENU_END
	};

	/* Cartridge types should be placed here in ascending order and ended by CARTRIDGE_NONE */
	static int writable_carts_array[] = {
		CARTRIDGE_RAMCART_64,
		CARTRIDGE_RAMCART_128,
		CARTRIDGE_DOUBLE_RAMCART_256,
		CARTRIDGE_RAMCART_1M,
		CARTRIDGE_RAMCART_2M,
		CARTRIDGE_RAMCART_4M,
		CARTRIDGE_RAMCART_8M,
		CARTRIDGE_RAMCART_16M,
		CARTRIDGE_RAMCART_32M,
		CARTRIDGE_SIDICAR_32,

		CARTRIDGE_NONE /* obligatory */
	};

	int option = 2;
	int seltype;
	CARTRIDGE_image_t *ramcart;
	int old_state;

	for (;;) {
		static char cart_filename[FILENAME_MAX];
		
		if (CARTRIDGE_main.type == CARTRIDGE_NONE) {
			menu_array[2].item = "None";
			menu_array[2].suffix = "Return:insert";
		}
		else {
			menu_array[2].item = CARTRIDGE_main.filename;
			menu_array[2].suffix = "Return:insert Backspace:remove";
		}

		if (CARTRIDGE_main.type == CARTRIDGE_SDX_64 || CARTRIDGE_main.type == CARTRIDGE_SDX_128) {
			menu_array[3].flags = UI_ITEM_FILESEL | UI_ITEM_TIP;
			if (CARTRIDGE_piggyback.type == CARTRIDGE_NONE) {
				menu_array[3].item = "None";
				menu_array[3].suffix = "Return:insert";
			}
			else {
				menu_array[3].item = CARTRIDGE_piggyback.filename;
				menu_array[3].suffix = "Return:insert Backspace:remove";
			}
		} else
			menu_array[3].flags = UI_ITEM_HIDDEN;

		ramcart = NULL;
		switch (CARTRIDGE_main.type) {
		case CARTRIDGE_RAMCART_64:
		case CARTRIDGE_RAMCART_128:
		case CARTRIDGE_DOUBLE_RAMCART_256:
		case CARTRIDGE_RAMCART_1M:
		case CARTRIDGE_RAMCART_2M:
		case CARTRIDGE_RAMCART_4M:
		case CARTRIDGE_RAMCART_8M:
		case CARTRIDGE_RAMCART_16M:
		case CARTRIDGE_RAMCART_32M:
			ramcart = &CARTRIDGE_main;
			break;
		}
		switch (CARTRIDGE_piggyback.type) {
		case CARTRIDGE_RAMCART_64:
		case CARTRIDGE_RAMCART_128:
		case CARTRIDGE_DOUBLE_RAMCART_256:
		case CARTRIDGE_RAMCART_1M:
		case CARTRIDGE_RAMCART_2M:
		case CARTRIDGE_RAMCART_4M:
		case CARTRIDGE_RAMCART_8M:
		case CARTRIDGE_RAMCART_16M:
		case CARTRIDGE_RAMCART_32M:
			ramcart = &CARTRIDGE_piggyback;
		}
		menu_array[4].flags = 
		menu_array[5].flags = 
		menu_array[6].flags = 
		menu_array[7].flags = 
		menu_array[8].flags = 
		menu_array[9].flags = 
		menu_array[10].flags = 
		menu_array[11].flags = 
		menu_array[12].flags = 
		menu_array[13].flags = 
		menu_array[14].flags = 
		menu_array[15].flags = UI_ITEM_HIDDEN;
		old_state = -1;
		if (ramcart != NULL) {
			old_state = ramcart->state;

			if (ramcart->type == CARTRIDGE_RAMCART_2M)
				menu_array[11].item = "Ram-Cart 1/2M switch";
			else if (ramcart->type == CARTRIDGE_RAMCART_4M)
				menu_array[11].item = "Ram-Cart D switch";

			switch (ramcart->type) {
			case CARTRIDGE_DOUBLE_RAMCART_256:
				menu_array[14].flags = UI_ITEM_ACTION;
				menu_array[14].suffix = ramcart->state & 0x20000 ? "Read/Write" : "Write Only";

				menu_array[5].flags = 
				menu_array[6].flags = UI_ITEM_ACTION;
				if (ramcart->state & 0x2000) {
					menu_array[5].suffix = "256K";
					menu_array[6].suffix = ramcart->state & 0x4000 ? "Swapped Order" : "Normal Order";
				}
				else {
					menu_array[5].suffix = "2x128K";
					menu_array[6].suffix = ramcart->state & 0x4000 ? "Second Module" : "First Module";
				}
				SetItemChecked(menu_array, 4, ramcart->state & 0x1000); /* R/W */
				break;
			case CARTRIDGE_RAMCART_4M:
				SetItemChecked(menu_array, 12, ramcart->state & 0x0200); /* 2/4M */
			case CARTRIDGE_RAMCART_2M:
				SetItemChecked(menu_array, 11, ramcart->state & 0x0100); /* 1/2M or D */
			case CARTRIDGE_RAMCART_1M:
				if (ramcart->type == CARTRIDGE_RAMCART_1M) {
					menu_array[6].flags = 
					menu_array[13].flags = 
					menu_array[14].flags = UI_ITEM_ACTION;
					menu_array[6].suffix = ramcart->state & 0x4000 ? "Swapped Order" : "Normal Order";
					menu_array[13].suffix = ramcart->state & 0x10000 ? "Full" : "Simplified";
					menu_array[14].suffix = ramcart->state & 0x20000 ? "Read/Write" : "Write Only";
				}
				else
					menu_array[13].suffix = "N/A";

				menu_array[7].flags = UI_ITEM_ACTION;
				if (ramcart->state & 0x8000) {
					menu_array[10].flags = 
					menu_array[9].flags = 
					menu_array[8].flags = UI_ITEM_ACTION;
					menu_array[10].suffix = 
					menu_array[9].suffix = 
					menu_array[8].suffix = "N/A";
					menu_array[7].suffix = "Installed";
				}
				else {
					SetItemChecked(menu_array, 10, ramcart->state & 0x0080); /* C */
					SetItemChecked(menu_array, 9, ramcart->state & 0x0040); /* B */
					SetItemChecked(menu_array, 8, ramcart->state & 0x0004); /* A */
					menu_array[7].suffix = "None";
				}
			case CARTRIDGE_RAMCART_32M:
			case CARTRIDGE_RAMCART_16M:
			case CARTRIDGE_RAMCART_8M:
			case CARTRIDGE_RAMCART_128:
			case CARTRIDGE_RAMCART_64:
				SetItemChecked(menu_array, 4, ramcart->state & 0x1000); /* R/W */
			}
			menu_array[15].flags = UI_ITEM_ACTION;
		}

		SetItemChecked(menu_array, 16, CARTRIDGE_autoreboot);

		option = UI_driver->fSelect("Cartridge Management", 0, option, menu_array, &seltype);

		switch (option) {
		case 0:
			if (UI_driver->fGetLoadFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
				int error;
				CARTRIDGE_image_t cart;

				int kb = CARTRIDGE_ReadImage(cart_filename, &cart);
				if (kb == CARTRIDGE_CANT_OPEN) {
					CantLoad(cart_filename);
					break;
				}
				else if (kb == CARTRIDGE_TOO_FEW_DATA) {
					UI_driver->fMessage("Error reading CART file", 1);
					break;
				}
				else if (kb == CARTRIDGE_BAD_FORMAT) {
					UI_driver->fMessage("ROM image must be full kilobytes long", 1);
					break;
				}

				if (!cart.raw) {
					free(cart.image);
					UI_driver->fMessage("Not an image file", 1);
					break;
				}

				cart.type = UI_SelectCartType(kb);
				if (cart.type == CARTRIDGE_NONE) {
					free(cart.image);
					break;
				}

				if (!UI_driver->fGetSaveFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
					free(cart.image);
					break;
				}

				error = CARTRIDGE_WriteImage(cart_filename, cart.type, cart.image, kb << 10, FALSE, -1);
				free(cart.image);
				if (error)
					CantSave(cart_filename);
				else
					Created(cart_filename);
			}
			break;
		case 1:
			if (UI_driver->fGetLoadFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
				int error;
				CARTRIDGE_image_t cart;

				int kb = CARTRIDGE_ReadImage(cart_filename, &cart);
				if (kb == CARTRIDGE_CANT_OPEN) {
					CantLoad(cart_filename);
					break;
				}
				else if (kb == CARTRIDGE_TOO_FEW_DATA) {
					UI_driver->fMessage("Error reading CART file", 1);
					break;
				}
				else if (kb == CARTRIDGE_BAD_FORMAT) {
					UI_driver->fMessage("Not a CART file", 1);
					break;
				}

				if (cart.raw) {
					free(cart.image);
					UI_driver->fMessage("Not a CART file", 1);
					break;
				}

				if (!UI_driver->fGetSaveFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
					free(cart.image);
					break;
				}

				error = CARTRIDGE_WriteImage(cart_filename, CARTRIDGE_UNKNOWN, cart.image, cart.size << 10, TRUE, -1);
				free(cart.image);
				if (error)
					CantSave(cart_filename);
				else
					Created(cart_filename);
			}
			break;
		case 2:
			switch (seltype) {
			case UI_USER_SELECT: /* Enter */
				if (UI_driver->fGetLoadFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
					int r = CARTRIDGE_InsertAutoReboot(cart_filename);
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
						CARTRIDGE_SetTypeAutoReboot(&CARTRIDGE_main, UI_SelectCartType(r));
						break;
					}
				}
				break;
			case UI_USER_DELETE: /* Backspace */
				CARTRIDGE_RemoveAutoReboot();
				break;
			}
			break;
		case 3:
			switch (seltype) {
			case UI_USER_SELECT: /* Enter */
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
						CARTRIDGE_SetType(&CARTRIDGE_piggyback, UI_SelectCartType(r));
						break;
					}
				}
				break;
			case UI_USER_DELETE: /* Backspace */
				CARTRIDGE_Remove_Second();
				break;
			}
			break;
		case 4: /* Ram-Cart R/W */
			ramcart->state ^= 0x1000;
			CARTRIDGE_UpdateState(ramcart, old_state);
			break;
		case 5: /* Ram-Cart P1 (2x128/256K) */
			ramcart->state ^= 0x2000;
			CARTRIDGE_UpdateState(ramcart, old_state);
			break;
		case 6: /* Ram-Cart P2 (Exchange 128K modules) */
			ramcart->state ^= 0x4000;
			CARTRIDGE_UpdateState(ramcart, old_state);
			break;
		case 7: /* Ram-Cart jumpers ABC installation flag */
			ramcart->state ^= 0x8000;
			CARTRIDGE_UpdateState(ramcart, old_state);
			break;
		case 8: /* Ram-Cart A */
			if (!(ramcart->state & 0x8000)) {
				ramcart->state ^= 0x0004;
				CARTRIDGE_UpdateState(ramcart, old_state);
			}
			break;
		case 9: /* Ram-Cart B */
			if (!(ramcart->state & 0x8000)) {
				ramcart->state ^= 0x0040;
				CARTRIDGE_UpdateState(ramcart, old_state);
			}
			break;
		case 10: /* Ram-Cart C */
			if (!(ramcart->state & 0x8000)) {
				ramcart->state ^= 0x0080;
				CARTRIDGE_UpdateState(ramcart, old_state);
			}
			break;
		case 11: /* Ram-Cart 1/2M or D */
			ramcart->state ^= 0x0100;
			CARTRIDGE_UpdateState(ramcart, old_state);
			break;
		case 12: /* Ram-Cart 2/4M */
			ramcart->state ^= 0x0200;
			CARTRIDGE_UpdateState(ramcart, old_state);
			break;
		case 13: /* Ram-Cart address decoder */
			if (ramcart->type == CARTRIDGE_RAMCART_1M)
				ramcart->state ^= 0x10000;
			break;
		case 14: /* Ram-Cart control register */
			if (ramcart->type == CARTRIDGE_DOUBLE_RAMCART_256 || ramcart->type == CARTRIDGE_RAMCART_1M)
				ramcart->state ^= 0x20000;
			break;
		case 15: /* Ram-Cart Reset */
			ramcart->state &= 0xfff00;
			CARTRIDGE_UpdateState(ramcart, old_state);
			UI_driver->fMessage("Ram-Cart reinitialized", 1);
			break;
		case 16:
			CARTRIDGE_autoreboot = !CARTRIDGE_autoreboot;
			break;
		case 17:
			if (UI_driver->fGetSaveFilename(cart_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
				int cart_type = UI_SelectCartTypeBetween(writable_carts_array);
				if (cart_type != CARTRIDGE_NONE) {
					if ( CARTRIDGE_WriteImage(
							cart_filename, 
							cart_type, NULL, CARTRIDGES[cart_type].kb << 10, FALSE, 0x00 ) )
						CantSave(cart_filename);
					else
						Created(cart_filename);
				}
			}
			break;
		default:
			return;
		}
	}
}

#ifdef AUDIO_RECORDING
static void SoundRecording(void)
{
	if (!Sound_enabled) {
		UI_driver->fMessage("Can't record. Sound not enabled.", 1);
		return;
	}
	if (!File_Export_IsRecording()) {
		char buffer[FILENAME_MAX];
		if (File_Export_GetNextSoundFile(buffer, sizeof(buffer))) {
			/* file does not exist - we can create it */
			if (File_Export_StartRecording(buffer)) {
				FilenameMessage("Recording sound to file \"%s\"", buffer);
			}
			else {
				UI_driver->fMessage(FILE_EXPORT_error_message, 1);
			}
			return;
		}
		UI_driver->fMessage("All sound files exist!", 1);
	}
	else {
		File_Export_StopRecording();
		UI_driver->fMessage("Recording stopped", 1);
	}
}
#endif /* AUDIO_RECORDING */

#ifdef VIDEO_RECORDING
static void VideoRecording(void)
{
	if (!File_Export_IsRecording()) {
		char buffer[FILENAME_MAX];
		if (File_Export_GetNextVideoFile(buffer, sizeof(buffer))) {
			/* file does not exist - we can create it */
			if (File_Export_StartRecording(buffer)) {
				FilenameMessage("Recording video to file \"%s\"", buffer);
			}
			else {
				UI_driver->fMessage(FILE_EXPORT_error_message, 1);
			}
			return;
		}
		UI_driver->fMessage("All video files exist!", 1);
	}
	else {
		File_Export_StopRecording();
		UI_driver->fMessage("Recording stopped", 1);
	}
}
#endif /* VIDEO_RECORDING */

static int AutostartFile(void)
{
	static char filename[FILENAME_MAX];
	if (UI_driver->fGetLoadFilename(filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
		int file_type = AFILE_OpenFile(filename, TRUE, 1, FALSE);
		if (file_type != 0) {
			int rom_kb;
			if ((file_type & 0xff) == AFILE_ROM
			    && (rom_kb = (file_type & ~0xff) >> 8) != 0) {
				int cart_type = UI_SelectCartType(rom_kb);
				if (cart_type == CARTRIDGE_NONE)
					/* User chose nothing - go back to parent menu. */
					return FALSE;
				CARTRIDGE_SetTypeAutoReboot(&CARTRIDGE_main, cart_type);
			}
			return TRUE;
		}
		CantLoad(filename);
	}
	return FALSE;
}

static void MakeBlankTapeMenu(void)
{
	char filenm[FILENAME_MAX];
	char description[CASSETTE_DESCRIPTION_MAX];
	description[0] = '\0';
	strncpy(filenm, CASSETTE_filename, FILENAME_MAX);
	if (!UI_driver->fGetSaveFilename(filenm, UI_atari_files_dir, UI_n_atari_files_dir))
		return;
	if (!UI_driver->fEditString("Enter tape's description", description, sizeof(description)))
		return;
	if (!CASSETTE_CreateCAS(filenm, description))
		CantSave(filenm);
}

/* Callback function that writes a text label to *LABEL, for use by
   any slider that adjusts tape position. */
static void TapeSliderLabel(char *label, int value, void *user_data)
{
	if (value >= CASSETTE_GetSize())
		sprintf(label, "End");
	else
		snprintf(label, 10, "%u", (unsigned int)value + 1);
}

static void TapeManagement(void)
{
	static char position_string[17];
	static char cas_symbol[] = " C:";

	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL_PREFIX_TIP(0, cas_symbol, NULL, NULL),
		UI_MENU_LABEL("Description:"),
		UI_MENU_LABEL(CASSETTE_description),
		UI_MENU_ACTION_PREFIX_TIP(1, "Position: ", position_string, NULL),
		UI_MENU_CHECK(2, "Record:"),
		UI_MENU_SUBMENU(3, "Make blank tape"),
		UI_MENU_END
	};

	int option = 0;
	int seltype;

	for (;;) {

		int position = CASSETTE_GetPosition();
		int size = CASSETTE_GetSize();

		/* Set the cassette file description and set the Select Tape tip */
		switch (CASSETTE_status) {
		case CASSETTE_STATUS_NONE:
			menu_array[0].item = "None";
			menu_array[0].suffix = "Return:insert";
			menu_array[3].suffix = "Tape not loaded";
			cas_symbol[0] = ' ';
			break;
		case CASSETTE_STATUS_READ_ONLY:
			menu_array[0].item = CASSETTE_filename;
			menu_array[0].suffix = "Return:insert Backspace:eject";
			menu_array[3].suffix = "Return:change Backspace:rewind";
			cas_symbol[0] = '*';
			break;
		default: /* CASSETTE_STATUS_READ_WRITE */
			menu_array[0].item = CASSETTE_filename;
			menu_array[0].suffix = "Ret:insert Bksp:eject Space:read-only";
			menu_array[3].suffix = "Return:change Backspace:rewind";
			cas_symbol[0] = CASSETTE_write_protect ? '*' : ' ';
			break;
		}

		SetItemChecked(menu_array, 2, CASSETTE_record);

		if (CASSETTE_status == CASSETTE_STATUS_NONE)
			memcpy(position_string, "N/A", 4);
		else {
			if (position > size)
				snprintf(position_string, sizeof(position_string) - 1, "End/%u blocks", size);
			else
				snprintf(position_string, sizeof(position_string) - 1, "%u/%u blocks", position, size);
		}

		option = UI_driver->fSelect("Tape Management", 0, option, menu_array, &seltype);

		switch (option) {
		case 0:
			switch (seltype) {
			case UI_USER_SELECT: /* Enter */
				if (UI_driver->fGetLoadFilename(CASSETTE_filename, UI_atari_files_dir, UI_n_atari_files_dir)) {
					UI_driver->fMessage("Please wait while inserting...", 0);
					if (!CASSETTE_Insert(CASSETTE_filename)) {
						CantLoad(CASSETTE_filename);
						break;
					}
				}
				break;
			case UI_USER_DELETE: /* Backspace */
				if (CASSETTE_status != CASSETTE_STATUS_NONE)
					CASSETTE_Remove();
				break;
			case UI_USER_TOGGLE: /* Space */
				/* Toggle only if the cassette is mounted. */
				if (CASSETTE_status != CASSETTE_STATUS_NONE && !CASSETTE_ToggleWriteProtect())
					/* The file is read-only. */
					UI_driver->fMessage("Cannot switch to read/write", 1);
				break;
			}
			break;
		case 1:
			/* The Current Block control is inactive if no cassette file present */
			if (CASSETTE_status == CASSETTE_STATUS_NONE)
				break;

			switch (seltype) {
			case UI_USER_SELECT: { /* Enter */
					int value = UI_driver->fSelectSlider("Position tape",
					                                     position - 1,
					                                     size, &TapeSliderLabel, NULL);
					if (value != -1)
						CASSETTE_Seek(value + 1);
				}
				break;
			case UI_USER_DELETE: /* Backspace */
				CASSETTE_Seek(1);
				break;
			}
			break;
		case 2:
			/* Toggle only if the cassette is mounted. */
			if (CASSETTE_status != CASSETTE_STATUS_NONE && !CASSETTE_ToggleRecord())
				UI_driver->fMessage("Tape is read-only, recording will fail", 1);
			break;
		case 3:
			MakeBlankTapeMenu();
			break;
		default:
			return;
		}
	}

}

static void HDeviceStatus(void)
{
	static char open_info[] = " 0 currently open files";
	static UI_tMenuItem menu_array[] = {
		UI_MENU_ACTION(0, "Devices enabled:"),
		UI_MENU_ACTION(1, "SIO letter:"),
		UI_MENU_FILESEL_PREFIX_TIP(2, "Device 1 path: ", Devices_atari_h_dir[0], "Also device 6 with ASCII conversion"),
		UI_MENU_FILESEL_PREFIX_TIP(3, "Device 2 path: ", Devices_atari_h_dir[1], "Also device 7 with ASCII conversion"),
		UI_MENU_FILESEL_PREFIX_TIP(4, "Device 3 path: ", Devices_atari_h_dir[2], "Also device 8 with ASCII conversion"),
		UI_MENU_FILESEL_PREFIX_TIP(5, "Device 4 path: ", Devices_atari_h_dir[3], "Also device 9 with ASCII conversion"),
		UI_MENU_LABEL("Atari executable path:"),
		UI_MENU_ACTION_PREFIX(6, " ", Devices_h_exe_path),
		UI_MENU_LABEL("File status:"),
		UI_MENU_ACTION_TIP(7, open_info, NULL),
		UI_MENU_LABEL("Current directories:"),
		UI_MENU_ACTION_PREFIX_TIP(8, " Dev 1:", Devices_h_current_dir[0], NULL),
		UI_MENU_ACTION_PREFIX_TIP(9, " Dev 2:", Devices_h_current_dir[1], NULL),
		UI_MENU_ACTION_PREFIX_TIP(10, " Dev 3:", Devices_h_current_dir[2], NULL),
		UI_MENU_ACTION_PREFIX_TIP(11, " Dev 4:", Devices_h_current_dir[3], NULL),
		UI_MENU_END
	};
	int option = 0;
	char hdev_option[4];
	for (;;) {
		int i;
		int seltype;
		i = Devices_H_CountOpen();
		open_info[1] = (char) ('0' + i);
		open_info[22] = (i != 1) ? 's' : '\0';
		FindMenuItem(menu_array, 7)->suffix = (i > 0) ? ((i == 1) ? "Backspace: close" : "Backspace: close all") : NULL;
		for (i = 0; i < 4; i++)
			menu_array[11 + i].suffix = Devices_h_current_dir[i][0] != '\0' ? "Backspace: reset to root" : NULL;
		FindMenuItem(menu_array, 0)->suffix = Devices_enable_h_patch ? (Devices_h_read_only ? "Read-only" : "Read/write") : "No ";
		strcpy(hdev_option + 1, ":");
		hdev_option[0] = Devices_h_device_name;
		FindMenuItem(menu_array, 1)->suffix = hdev_option;
		option = UI_driver->fSelect("Host device settings", 0, option, menu_array, &seltype);
		switch (option) {
		case 0:
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
		case 1:
			if (seltype == UI_USER_DELETE)
				Devices_h_device_name = 'H';
			else {
				hdev_option[1] = 0;
				UI_driver->fEditString("Host Device SIO Letter:", hdev_option, 2);
				if( hdev_option[0] >= 'a' && hdev_option[0] <= 'z' )
					hdev_option[0] -= 'a' - 'A';
				if( hdev_option[0] && strchr("ABDFGHIJLMNOQTUVWXYZ", hdev_option[0]) )
					Devices_h_device_name = hdev_option[0];
				else
					UI_driver->fMessage("Invalid device letter", 1);
			}
			break;
		case 2:
		case 3:
		case 4:
		case 5:
			if (seltype == UI_USER_DELETE)
				FindMenuItem(menu_array, option)->item[0] = '\0';
			else
				UI_driver->fGetDirectoryPath(FindMenuItem(menu_array, option)->item);
			break;
		case 6:
			{
				char tmp_path[FILENAME_MAX];
				strcpy(tmp_path, Devices_h_exe_path);
				if (UI_driver->fEditString("Atari executables path", tmp_path, FILENAME_MAX))
					strcpy(Devices_h_exe_path, tmp_path);
			}
			break;
		case 7:
			if (seltype == UI_USER_DELETE)
				Devices_H_CloseAll();
			break;
		case 8:
		case 9:
		case 10:
		case 11:
			if (seltype == UI_USER_DELETE)
				Devices_h_current_dir[option - 8][0] = '\0';
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

static void ROMLocations(char const *title, UI_tMenuItem *menu_array)
{
	int option = 0;

	for (;;) {
		int seltype;

		UI_tMenuItem *item;
		for (item = menu_array; item->flags != UI_ITEM_END; ++item) {
			int i = item->retval;
			if (SYSROM_roms[i].filename[0] == '\0')
				item->item = "None";
			else
				item->item = SYSROM_roms[i].filename;
		}

		option = UI_driver->fSelect(title, 0, option, menu_array, &seltype);
		if (option < 0)
			return;
		if (seltype == UI_USER_DELETE)
			SYSROM_roms[option].filename[0] = '\0';
		else {
			char filename[FILENAME_MAX] = "";
			if (SYSROM_roms[option].filename[0] != '\0')
				strcpy(filename, SYSROM_roms[option].filename);
			else {
				/* Use first non-empty ROM path as a starting filename for the dialog. */
				int i;
				for (i = 0; i < SYSROM_LOADABLE_SIZE; ++i) {
					if (SYSROM_roms[i].filename[0] != '\0') {
						strcpy(filename, SYSROM_roms[i].filename);
						break;
					}
				}
			}
			for (;;) {
				if (!UI_driver->fGetLoadFilename(filename, NULL, 0))
					break;
				switch(SYSROM_SetPath(filename, 1, option)) {
				case SYSROM_ERROR:
					CantLoad(filename);
					continue;
				case SYSROM_BADSIZE:
					UI_driver->fMessage("Can't load, incorrect file size", 1);
					continue;
				case SYSROM_BADCRC:
					UI_driver->fMessage("Can't load, incorrect checksum", 1);
					continue;
				}
				break;
			}
		}
	}

}

static void ROMLocations800(void)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL_PREFIX(SYSROM_A_NTSC, " Rev. A NTSC:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_A_PAL, " Rev. A PAL:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_B_NTSC, " Rev. B NTSC:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_800_CUSTOM, " Custom:", NULL),
		UI_MENU_END
	};
	ROMLocations("400/800 OS ROM Locations", menu_array);
}

static void ROMLocationsXL(void)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL_PREFIX(SYSROM_AA00R10, " AA00 Rev. 10:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_AA01R11, " AA01 Rev. 11:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_BB00R1, " BB00 Rev. 1:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_BB01R2, " BB01 Rev. 2:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_BB02R3, " BB02 Rev. 3:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_BB02R3V4, " BB02 Rev. 3 Ver. 4:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_CC01R4, " CC01 Rev. 4:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_BB01R3, " BB01 Rev. 3:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_BB01R4_OS, " BB01 Rev. 4:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_BB01R59, " BB01 Rev. 59:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_BB01R59A, " BB01 Rev. 59 alt.:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_XL_CUSTOM, " Custom:", NULL),
		UI_MENU_END
	};
	ROMLocations("XL/XE OS ROM Locations", menu_array);
}

static void ROMLocations5200(void)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL_PREFIX(SYSROM_5200, " Original:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_5200A, " Rev. A:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_5200_CUSTOM, " Custom:", NULL),
		UI_MENU_END
	};
	ROMLocations("5200 BIOS ROM Locations", menu_array);
}

static void ROMLocationsBASIC(void)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL_PREFIX(SYSROM_BASIC_A, " Rev. A:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_BASIC_B, " Rev. B:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_BASIC_C, " Rev. C:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_BASIC_CUSTOM, " Custom:", NULL),
		UI_MENU_END
	};
	ROMLocations("BASIC ROM Locations", menu_array);
}

static void ROMLocationsXEGame(void)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL_PREFIX(SYSROM_XEGAME, " Original:", NULL),
		UI_MENU_FILESEL_PREFIX(SYSROM_XEGAME_CUSTOM, " Custom:", NULL),
		UI_MENU_END
	};
	ROMLocations("XEGS Builtin Game ROM Locations", menu_array);
}

static SYSROM_t GetCurrentOS(void)
{
	SYSROM_t sysrom = { 0 };

	int rom = SYSROM_os_versions[Atari800_machine_type];
	if (rom == SYSROM_AUTO)
		rom = SYSROM_AutoChooseOS(Atari800_machine_type, MEMORY_ram_size, Atari800_tv_mode);

	if (rom != -1)
		sysrom = SYSROM_roms[rom];

	return sysrom;
}

static SYSROM_t GetCurrentBASIC(void)
{
	SYSROM_t sysrom = { 0 };

	int rom = SYSROM_basic_version;
	if (rom == SYSROM_AUTO)
		rom = SYSROM_AutoChooseBASIC();

	if (rom != -1)
		sysrom = SYSROM_roms[rom];

	return sysrom;
}

static SYSROM_t GetCurrentXEGame(void)
{
	SYSROM_t sysrom = { 0 };

	int rom = SYSROM_xegame_version;
	if (rom == SYSROM_AUTO)
		rom = SYSROM_AutoChooseXEGame();

	if (rom != -1)
		sysrom = SYSROM_roms[rom];

	return sysrom;
}

static void SystemROMSettings(void)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL(0, "Find ROM images in a directory"),
		UI_MENU_SUBMENU(1, "400/800 OS ROM locations"),
		UI_MENU_SUBMENU(2, "XL/XE OS ROM locations"),
		UI_MENU_SUBMENU(3, "5200 BIOS ROM locations"),
		UI_MENU_SUBMENU(4, "BASIC ROM locations"),
		UI_MENU_SUBMENU(5, "XEGS builtin game ROM locations"),
		UI_MENU_END
	};

	int option = 0;
	int need_initialise = FALSE;
	SYSROM_t old_sysrom, new_sysrom;

	for (;;) {
		int seltype;

		option = UI_driver->fSelect("System ROM Settings", 0, option, menu_array, &seltype);

		switch (option) {
		case 0:
			{
				char rom_dir[FILENAME_MAX] = "";
				int i;
				/* Use first non-empty ROM path as a starting filename for the dialog. */
				for (i = 0; i < SYSROM_LOADABLE_SIZE; ++i) {
					if (SYSROM_roms[i].filename[0] != '\0') {
						Util_splitpath(SYSROM_roms[i].filename, rom_dir, NULL);
						break;
					}
				}
				if (UI_driver->fGetDirectoryPath(rom_dir)) {
					SYSROM_t old_basic, old_xegame;

					old_sysrom = GetCurrentOS();
					old_basic = GetCurrentBASIC();
					old_xegame = GetCurrentXEGame();

					if (SYSROM_FindInDir(rom_dir, FALSE)) {
						new_sysrom = GetCurrentOS();

						if (old_sysrom.data != new_sysrom.data) {
							need_initialise = TRUE;
							break;
						}

						if (Atari800_machine_type != Atari800_MACHINE_5200) {
							new_sysrom = GetCurrentBASIC();

							if (old_basic.data != new_sysrom.data) {
								need_initialise = TRUE;
								break;
							}
						}

						if (Atari800_machine_type == Atari800_MACHINE_XLXE && Atari800_builtin_game) {
							new_sysrom = GetCurrentXEGame();

							if (old_xegame.data != new_sysrom.data) {
								need_initialise = TRUE;
								break;
							}
						}
					}
				}
			}
			break;

		case 1:
		case 2:
		case 3:
			old_sysrom = GetCurrentOS();

			switch (option) {
			case 1:
				ROMLocations800();
				break;
			case 2:
				ROMLocationsXL();
				break;
			case 3:
				ROMLocations5200();
				break;
			}

			new_sysrom = GetCurrentOS();

			if (old_sysrom.data != new_sysrom.data)
				need_initialise = TRUE;
			break;

		case 4:
			if (Atari800_machine_type != Atari800_MACHINE_5200) {
				old_sysrom = GetCurrentBASIC();

				ROMLocationsBASIC();

				new_sysrom = GetCurrentBASIC();

				if (old_sysrom.data != new_sysrom.data)
					need_initialise = TRUE;
			} else {
				/* ignore BASIC changes on 5200 */
				ROMLocationsBASIC();
			}
			break;

		case 5:
			if (Atari800_machine_type == Atari800_MACHINE_XLXE && Atari800_builtin_game) {
				old_sysrom = GetCurrentXEGame();

				ROMLocationsXEGame();

				new_sysrom = GetCurrentXEGame();

				if (old_sysrom.data != new_sysrom.data)
					need_initialise = TRUE;
			} else {
				/* ignore XEGame changes on non-XE */
				ROMLocationsXEGame();
			}
			break;

		default:
			if (need_initialise)
				Atari800_InitialiseMachine();
			return;
		}
	}
}

/* percentages of normal speed, with 0 -> max speed */
static const int turbo_speeds[] = {
	50, 60, 70, 80, 90,
	110, 120, 130, 140, 150,
	170, 200, 250, 300, 400, 500,
	600, 700, 800, 1000, 0
};
static const int TURBO_SPEEDS = sizeof(turbo_speeds) / sizeof(*turbo_speeds);

static void format_turbo_speed(char* label, int value, void* user_data) {
	int speed;
	if (value < 0 || value >= TURBO_SPEEDS) {
		strcpy(label, "?");
		return;
	}

	speed = turbo_speeds[value];
	if (speed == 0) {
		strcpy(label, "Max");
	}
	else {
		int prec = speed % 100 ? 1 : 0;
		sprintf(label, "x%.*f", prec, speed / 100.0);
	}
}

static int find_turbo_speed_index(int turbo_speed) {
	int i;
	for (i = 0; i < TURBO_SPEEDS; ++i) {
		if (turbo_speeds[i] == turbo_speed) {
			return i;
		}
	}
	return -1;
}

static void AtariSettings(void)
{
#ifdef XEP80_EMULATION
	static const UI_tMenuItem xep80_menu_array[] = {
		UI_MENU_ACTION(0, "off"),
		UI_MENU_ACTION(1, "port 1"),
		UI_MENU_ACTION(2, "port 2"),
		UI_MENU_END
	};
#endif /* XEP80_EMULATION */

	static UI_tMenuItem menu_array[] = {
		UI_MENU_CHECK(0, "Disable BASIC when booting Atari:"),
		UI_MENU_CHECK(1, "Boot from tape (hold Start):"),
		UI_MENU_CHECK(2, "Enable R-Time 8:"),
#ifdef XEP80_EMULATION
		UI_MENU_SUBMENU_SUFFIX(18, "Enable XEP80:", NULL),
#endif /* XEP80_EMULATION */
		UI_MENU_CHECK(3, "SIO patch (fast disk access):"),
		UI_MENU_CHECK(17, "Turbo (F12):"),
		UI_MENU_ACTION(20, " Turbo speed:"),
		UI_MENU_CHECK(19, "Slow booting of DOS binary files:"),
		UI_MENU_CHECK(5, "P: device (printer):"),
		UI_MENU_ACTION_PREFIX(12, " Print command: ", Devices_print_command),
#ifdef R_IO_DEVICE
#ifdef DREAMCAST
		UI_MENU_CHECK(6, "R: device (using Coder's Cable):"),
#else
		UI_MENU_CHECK(6, "R: device (Atari850 via net):"),
#endif
#endif
		UI_MENU_SUBMENU(11, "Host device settings"),
		UI_MENU_SUBMENU(13, "System ROM settings"),
		UI_MENU_SUBMENU(14, "Configure directories"),
#ifndef DREAMCAST
		UI_MENU_ACTION(15, "Save configuration file"),
		UI_MENU_CHECK(16, "Save configuration on exit:"),
#endif
		UI_MENU_END
	};

	char tmp_command[256];
	char turbo[40];

	int option = 0;
	int speed = 0;

	for (;;) {
		int seltype;
		SetItemChecked(menu_array, 0, Atari800_disable_basic);
		SetItemChecked(menu_array, 1, CASSETTE_hold_start_on_reboot);
		SetItemChecked(menu_array, 2, RTIME_enabled);
		SetItemChecked(menu_array, 3, ESC_enable_sio_patch);
#ifdef XEP80_EMULATION
		FindMenuItem(menu_array, 18)->suffix = xep80_menu_array[XEP80_enabled ? XEP80_port + 1 : 0].item;
#endif /* XEP80_EMULATION */
		SetItemChecked(menu_array, 17, Atari800_turbo);
		format_turbo_speed(turbo, find_turbo_speed_index(Atari800_turbo_speed), NULL);
		FindMenuItem(menu_array, 20)->suffix = turbo;
		SetItemChecked(menu_array, 19, BINLOAD_slow_xex_loading);
		SetItemChecked(menu_array, 5, Devices_enable_p_patch);
#ifdef R_IO_DEVICE
		SetItemChecked(menu_array, 6, Devices_enable_r_patch);
#endif
		SetItemChecked(menu_array, 16, CFG_save_on_exit);

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
		case 5:
			Devices_enable_p_patch = !Devices_enable_p_patch;
			break;
#ifdef R_IO_DEVICE
		case 6:
			Devices_enable_r_patch = !Devices_enable_r_patch;
			break;
#endif
		case 11:
			HDeviceStatus();
			break;
		case 12:
			strcpy(tmp_command, Devices_print_command);
			if (UI_driver->fEditString("Print command", tmp_command, sizeof(tmp_command)))
				if (!Devices_SetPrintCommand(tmp_command))
					UI_driver->fMessage("Specified command is not allowed", 1);
			break;
		case 13:
			SystemROMSettings();
			break;
		case 14:
			ConfigureDirectories();
			break;
#ifndef DREAMCAST
		case 15:
			UI_driver->fMessage(CFG_WriteConfig() ? "Configuration file updated" : "Error writing configuration file", 1);
			break;
		case 16:
			CFG_save_on_exit = !CFG_save_on_exit;
			break;
#endif
		case 17:
			Atari800_turbo = !Atari800_turbo;
			break;
#ifdef XEP80_EMULATION
		case 18:
			{
				int option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, XEP80_enabled ? XEP80_port + 1 : 0, xep80_menu_array, NULL);
				if (option2 == 0)
					XEP80_SetEnabled(FALSE);
				else if (option2 > 0) {
					if (XEP80_SetEnabled(TRUE))
						XEP80_port = option2 - 1;
					else
						UI_driver->fMessage("Error: Missing XEP80 charset ROM.", 1);
				}
			}
			break;
#endif /* XEP80_EMULATION */
		case 19:
			BINLOAD_slow_xex_loading = !BINLOAD_slow_xex_loading;
			break;
		case 20:
			speed = UI_driver->fSelectSlider("Turbo speed", find_turbo_speed_index(Atari800_turbo_speed), TURBO_SPEEDS - 1, &format_turbo_speed, NULL);
			if (speed >= 0) {
				Atari800_turbo_speed = turbo_speeds[speed];
			}
			break;
		default:
			ESC_UpdatePatches();
			return;
		}
	}
}

static char state_filename[FILENAME_MAX];

static void SaveState(void)
{
	if (UI_driver->fGetSaveFilename(state_filename, UI_saved_files_dir, UI_n_saved_files_dir)) {
		int result;
		UI_driver->fMessage("Please wait while saving...", 0);
		result = StateSav_SaveAtariState(state_filename, "wb", TRUE);
		if (!result)
			CantSave(state_filename);
	}
}

static const char* get_state_filename(void) {
	const char* fname = ".atari800-quicksave.state";
	char* home = getenv("HOME");
	if (home) {
		Util_catpath(state_filename, home, fname);
	}
	else {
		strcpy(state_filename, fname);
	}
	return state_filename;
}

static void QuickSaveState(void) {
	int result = StateSav_SaveAtariState(get_state_filename(), "wb", TRUE);
	if (!result) {
		CantSave(state_filename);
	}
	else {
		Screen_SetStatusText("Saved", 120);
	}
}

static void QuickLoadState(void) {
	int result = StateSav_ReadAtariState(get_state_filename(), "rb");
	if (!result) {
		CantLoad(state_filename);
	}
	else {
		Screen_SetStatusText("Loaded", 120);
	}
}

static void LoadState(void)
{
	if (UI_driver->fGetLoadFilename(state_filename, UI_saved_files_dir, UI_n_saved_files_dir)) {
		UI_driver->fMessage("Please wait while loading...", 0);
		if (!StateSav_ReadAtariState(state_filename, "rb"))
			CantLoad(state_filename);
	}
}

/* CURSES_BASIC doesn't use artifacting or Atari800_collisions_in_skipped_frames,
   but does use Atari800_refresh_rate. However, changing Atari800_refresh_rate on CURSES is mostly
   useless, as the text-mode screen is normally updated infrequently by Atari.
   In case you wonder how artifacting affects CURSES version without
   CURSES_BASIC: it is visible on the screenshots (yes, they are bitmaps). */
#ifndef CURSES_BASIC

#if SUPPORTS_CHANGE_VIDEOMODE
/* Scrollable submenu for choosing fullscreen reslution. */
static int ChooseVideoResolution(int current_res)
{
	UI_tMenuItem *menu_array;
	char (*res_strings)[10];
	int num_res = VIDEOMODE_NumAvailableResolutions();

	unsigned int i;

	menu_array = (UI_tMenuItem *)Util_malloc((num_res+1) * sizeof(UI_tMenuItem));
	res_strings = (char (*)[10])Util_malloc(num_res * sizeof(char[10]));

	for (i = 0; i < num_res; i ++) {
		VIDEOMODE_CopyResolutionName(i, res_strings[i], 10);
		menu_array[i].flags = UI_ITEM_ACTION;
		menu_array[i].retval = i;
		menu_array[i].prefix = NULL;
		menu_array[i].item = res_strings[i];
		menu_array[i].suffix = NULL;
	}
	menu_array[num_res].flags = UI_ITEM_END;
	menu_array[num_res].retval = 0;
	menu_array[i].prefix = NULL;
	menu_array[i].item = NULL;
	menu_array[i].suffix = NULL;

	current_res = UI_driver->fSelect(NULL, UI_SELECT_POPUP, current_res, menu_array, NULL);
	free(res_strings);
	free(menu_array);
	return current_res;
}

/* Callback function that writes a text label to *LABEL, for use by
   any slider that adjusts an integer value. */
static void IntSliderLabel(char *label, int value, void *user_data)
{
	value += *((int *)user_data);
	sprintf(label, "%i", value); /* WARNING: No more that 10 chars! */
}

/* Callback function that writes a text label to *LABEL, for use by
   the Set Horizontal Offset slider. */
static void HorizOffsetSliderLabel(char *label, int value, void *user_data)
{
	value += *((int *)user_data);
	sprintf(label, "%i", value); /* WARNING: No more that 10 chars! */
	VIDEOMODE_SetHorizontalOffset(value);
}

/* Callback function that writes a text label to *LABEL, for use by
   the Set Vertical Offset slider. */
static void VertOffsetSliderLabel(char *label, int value, void *user_data)
{
	value += *((int *)user_data);
	sprintf(label, "%i", value); /* WARNING: No more that 10 chars! */
	VIDEOMODE_SetVerticalOffset(value);
}

#if GUI_SDL
/* Callback function that writes a text label to *LABEL, for use by
   the Scanlines Visibility slider. */
static void ScanlinesSliderLabel(char *label, int value, void *user_data)
{
	sprintf(label, "%i", value);
	SDL_VIDEO_SetScanlinesPercentage(value);
}

#if HAVE_OPENGL && SDL2
static void CrtBarrelSliderLabel(char *label, int value, void *user_data) {
	sprintf(label, "%i", value);
	SDL_VIDEO_CrtBarrelPercentage(value);
}

static void CrtBeamSliderLabel(char *label, int value, void *user_data) {
	sprintf(label, "%i", value);
	SDL_VIDEO_CrtBeamShape(value);
}

static void CrtGlowSliderLabel(char *label, int value, void *user_data) {
	sprintf(label, "%i", value);
	SDL_VIDEO_CrtPhosphorGlow(value);
}

#endif /* HAVE_OPENGL && SDL2 */
#endif /* GUI_SDL */

#if SDL2
static void show_hide_crt_options(UI_tMenuItem menu_array[]) {
	// hide/show options that require hardware renderer (and shaders)
	for (int index = 19; index <= 21; ++index) {
		FindMenuItem(menu_array, index)->flags = !SDL_VIDEO_opengl ? UI_ITEM_HIDDEN : UI_ITEM_SUBMENU;
	}
}
#endif

static void VideoModeSettings(void)
{
	static const UI_tMenuItem host_aspect_menu_array[] = {
		UI_MENU_ACTION(0, "autodetect"),
		UI_MENU_ACTION(1, "4:3"),
		UI_MENU_ACTION(2, "5:4"),
		UI_MENU_ACTION(3, "16:9"),
		UI_MENU_ACTION(4, "custom"),
		UI_MENU_END
	};
	static const UI_tMenuItem stretch_menu_array[] = {
		UI_MENU_ACTION(0, "none (1x)"),
		UI_MENU_ACTION(1, "2x"),
		UI_MENU_ACTION(2, "3x"),
		UI_MENU_ACTION(3, "fit screen - integral"),
		UI_MENU_ACTION(4, "fit screen - full"),
		UI_MENU_ACTION(5, "custom"),
		UI_MENU_END
	};
	char stretch_string[10];
	static const UI_tMenuItem fit_menu_array[] = {
		UI_MENU_ACTION(0, "fit width"),
		UI_MENU_ACTION(1, "fit height"),
		UI_MENU_ACTION(2, "fit both"),
		UI_MENU_END
	};
	static const UI_tMenuItem aspect_menu_array[] = {
		UI_MENU_ACTION(0, "don't keep"),
		UI_MENU_ACTION(1, "square pixels"),
		UI_MENU_ACTION(2, "authentic"),
		UI_MENU_END
	};
	static const UI_tMenuItem width_menu_array[] = {
		UI_MENU_ACTION(0, "narrow"),
		UI_MENU_ACTION(1, "like on TV"),
		UI_MENU_ACTION(2, "full overscan"),
		UI_MENU_ACTION(3, "custom"),
		UI_MENU_END
	};
	char horiz_area_string[4];
	static const UI_tMenuItem height_menu_array[] = {
		UI_MENU_ACTION(0, "short"),
		UI_MENU_ACTION(1, "like on TV"),
		UI_MENU_ACTION(2, "full overscan"),
		UI_MENU_ACTION(3, "custom"),
		UI_MENU_END
	};
	char vert_area_string[4];
	static char res_string[10];
	static char ratio_string[10];
	static char horiz_offset_string[4];
	static char vert_offset_string[4];
#if GUI_SDL
	static const UI_tMenuItem bpp_menu_array[] = {
		UI_MENU_ACTION(0, "autodetect"),
		UI_MENU_ACTION(1, "8"),
		UI_MENU_ACTION(2, "16"),
		UI_MENU_ACTION(3, "32"),
		UI_MENU_END
	};
	static char bpp_string[3];
#if HAVE_OPENGL
	static const UI_tMenuItem pixel_format_menu_array[] = {
		UI_MENU_ACTION(0, "16-bit BGR"),
		UI_MENU_ACTION(1, "16-bit RGB"),
		UI_MENU_ACTION(2, "32-bit BGRA"),
		UI_MENU_ACTION(3, "32-bit ARGB"),
		UI_MENU_END
	};
#endif /* HAVE_OPENGL */
	static char scanlines_string[4];
	static char barrel_string[4];
	static char beam_string[4];
	static char glow_string[4];
#endif /* GUI_SDL */

	static UI_tMenuItem menu_array[] = {
		UI_MENU_SUBMENU_SUFFIX(0, "Host display aspect ratio:", ratio_string),
#if GUI_SDL && HAVE_OPENGL
		UI_MENU_CHECK(1, "Hardware acceleration:"),
#if !SDL2
		UI_MENU_CHECK(2, " Bilinear filtering:"),
		UI_MENU_CHECK(3, " Use pixel buffer objects:"),
#endif
#endif /* GUI_SDL && HAVE_OPENGL */
		UI_MENU_CHECK(4, "Fullscreen:"),
		UI_MENU_SUBMENU_SUFFIX(5, " Fullscreen resolution:", res_string),
#if SUPPORTS_ROTATE_VIDEOMODE
		UI_MENU_CHECK(6, "Rotate sideways:"),
#endif /* SUPPORTS_ROTATE_VIDEOMODE */
#if GUI_SDL
		UI_MENU_SUBMENU_SUFFIX(7, "Bits per pixel:", bpp_string),
#if HAVE_OPENGL
		UI_MENU_SUBMENU_SUFFIX(8, "Pixel format:", NULL),
#endif /* HAVE_OPENGL */
		UI_MENU_CHECK(9, "Vertical synchronization:"),
#endif /* GUI_SDL */
		UI_MENU_SUBMENU_SUFFIX(10, "Image aspect ratio:", NULL),
		UI_MENU_SUBMENU_SUFFIX(11, "Stretch image:", NULL),
		UI_MENU_SUBMENU_SUFFIX(12, "Fit screen method:", NULL),
		UI_MENU_SUBMENU_SUFFIX(13, "Horizontal view area:", NULL),
		UI_MENU_SUBMENU_SUFFIX(14, "Vertical view area:", NULL),
		UI_MENU_SUBMENU_SUFFIX(15, "Horizontal shift:", horiz_offset_string),
		UI_MENU_SUBMENU_SUFFIX(16, "Vertical shift:", vert_offset_string),
#if GUI_SDL
		UI_MENU_SUBMENU_SUFFIX(17, "Scanlines visibility:", scanlines_string),
		UI_MENU_CHECK(18, " Interpolate scanlines:"),
#if HAVE_OPENGL && SDL2
		UI_MENU_SUBMENU_SUFFIX(19, "CRT barrel distortion:", barrel_string),
		UI_MENU_SUBMENU_SUFFIX(20, "CRT beam shape:", beam_string),
		UI_MENU_SUBMENU_SUFFIX(21, "CRT phosphor glow:", glow_string),
#endif
#endif /* GUI_SDL */
		UI_MENU_END
	};
	int option = 0;
	int option2 = 0;
	int seltype;

	for (;;) {
		VIDEOMODE_CopyHostAspect(ratio_string, 10);
#if GUI_SDL
		snprintf(bpp_string, sizeof(bpp_string), "%d", SDL_VIDEO_SW_bpp);
#if HAVE_OPENGL
		SetItemChecked(menu_array, 1, SDL_VIDEO_opengl);
		SetItemChecked(menu_array, 2, SDL_VIDEO_GL_filtering);
		SetItemChecked(menu_array, 3, SDL_VIDEO_GL_pbo);
		if (SDL_VIDEO_opengl) {
			FindMenuItem(menu_array, 7)->flags = UI_ITEM_HIDDEN;
			FindMenuItem(menu_array, 8)->flags = UI_ITEM_SUBMENU;
			FindMenuItem(menu_array, 8)->suffix = pixel_format_menu_array[SDL_VIDEO_GL_pixel_format].item;
		} else {
			FindMenuItem(menu_array, 7)->flags = UI_ITEM_SUBMENU;
			FindMenuItem(menu_array, 8)->flags = UI_ITEM_HIDDEN;
		}
#endif /* HAVE_OPENGL */
		if (SDL_VIDEO_vsync && !SDL_VIDEO_vsync_available) {
			FindMenuItem(menu_array, 9)->flags = UI_ITEM_ACTION;
			FindMenuItem(menu_array, 9)->suffix = "N/A";
		} else {
			FindMenuItem(menu_array, 9)->flags = UI_ITEM_CHECK;
			SetItemChecked(menu_array, 9, SDL_VIDEO_vsync);
		}
		snprintf(scanlines_string, sizeof(scanlines_string), "%d", SDL_VIDEO_scanlines_percentage);
		SetItemChecked(menu_array, 18, SDL_VIDEO_interpolate_scanlines);
		snprintf(barrel_string, sizeof(barrel_string), "%d", SDL_VIDEO_crt_barrel_distortion);
		snprintf(beam_string, sizeof(beam_string), "%d", SDL_VIDEO_crt_beam_shape);
		snprintf(glow_string, sizeof(glow_string), "%d", SDL_VIDEO_crt_phosphor_glow);
#endif /* GUI_SDL */
		SetItemChecked(menu_array, 4, !VIDEOMODE_windowed);
		VIDEOMODE_CopyResolutionName(VIDEOMODE_GetFullscreenResolution(), res_string, 10);
#if SUPPORTS_ROTATE_VIDEOMODE
		SetItemChecked(menu_array, 6, VIDEOMODE_rotate90);
#endif
		FindMenuItem(menu_array, 10)->suffix = aspect_menu_array[VIDEOMODE_keep_aspect].item;
		if (VIDEOMODE_stretch < VIDEOMODE_STRETCH_CUSTOM)
			FindMenuItem(menu_array, 11)->suffix = stretch_menu_array[VIDEOMODE_stretch].item;
		else {
			FindMenuItem(menu_array, 11)->suffix = stretch_string;
			snprintf(stretch_string, sizeof(stretch_string), "%f", VIDEOMODE_custom_stretch);
		}
		FindMenuItem(menu_array, 12)->suffix = fit_menu_array[VIDEOMODE_fit].item;
		if (VIDEOMODE_horizontal_area < VIDEOMODE_HORIZONTAL_CUSTOM)
			FindMenuItem(menu_array, 13)->suffix = width_menu_array[VIDEOMODE_horizontal_area].item;
		else {
			FindMenuItem(menu_array, 13)->suffix = horiz_area_string;
			snprintf(horiz_area_string, sizeof(horiz_area_string), "%d", VIDEOMODE_custom_horizontal_area);
		}
		if (VIDEOMODE_vertical_area < VIDEOMODE_VERTICAL_CUSTOM)
			FindMenuItem(menu_array, 14)->suffix = height_menu_array[VIDEOMODE_vertical_area].item;
		else {
			FindMenuItem(menu_array, 14)->suffix = vert_area_string;
			snprintf(vert_area_string, sizeof(vert_area_string), "%d", VIDEOMODE_custom_vertical_area);
		}
		snprintf(horiz_offset_string, sizeof(horiz_offset_string), "%d", VIDEOMODE_horizontal_offset);
		snprintf(vert_offset_string, sizeof(vert_offset_string), "%d", VIDEOMODE_vertical_offset);
#if SDL2
		show_hide_crt_options(menu_array);
#endif

		option = UI_driver->fSelect("Video Mode Settings", 0, option, menu_array, &seltype);
		switch (option) {
		case 0:
			{
				int current;
				for (current = 1; current < 4; ++current) {
					/* Find the currently-chosen host aspect ratio. */
					if (strcmp(ratio_string, host_aspect_menu_array[current].item) == 0)
						break;
				}
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, current, host_aspect_menu_array, NULL);
				if (option2 == 4) {
					char buffer[sizeof(ratio_string)];
					memcpy(buffer, ratio_string, sizeof(buffer));
					if (UI_driver->fEditString("Enter value in x:y format", buffer, sizeof(buffer)))
						VIDEOMODE_SetHostAspectString(buffer);
				}
				else if (option2 >= 1)
					VIDEOMODE_SetHostAspectString(host_aspect_menu_array[option2].item);
				else if (option2 == 0)
					VIDEOMODE_AutodetectHostAspect();
			}
			break;
#if GUI_SDL && HAVE_OPENGL
		case 1:
			SDL_VIDEO_ToggleOpengl();
			if (!SDL_VIDEO_opengl_available)
				UI_driver->fMessage("Error: OpenGL is not available.", 1);
#if SDL2
			show_hide_crt_options(menu_array);
#endif
			break;
		case 2:
			if (!SDL_VIDEO_opengl)
				UI_driver->fMessage("Works only with hardware acceleration.", 1);
			SDL_VIDEO_GL_ToggleFiltering();
			break;
		case 3:
			if (!SDL_VIDEO_opengl)
				UI_driver->fMessage("Works only with hardware acceleration.", 1);
			if (!SDL_VIDEO_GL_TogglePbo())
				UI_driver->fMessage("Pixel buffer objects not available.", 1);
			break;
#endif /* GUI_SDL && HAVE_OPENGL */
		case 4:
			VIDEOMODE_ToggleWindowed();
			break;
		case 5:
			option2 = ChooseVideoResolution(VIDEOMODE_GetFullscreenResolution());
			if (option2 >= 0)
				VIDEOMODE_SetFullscreenResolution(option2);
			break;
#if SUPPORTS_ROTATE_VIDEOMODE
		case 6:
			VIDEOMODE_ToggleRotate90();
			break;
#endif
#if GUI_SDL
		case 7:
			{
				int current;
				switch (SDL_VIDEO_SW_bpp) {
				case 0:
					current = 0;
					break;
				case 8:
					current = 1;
					break;
				case 16:
					current = 2;
					break;
				default: /* 32 */
					current = 3;
				}
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, current, bpp_menu_array, NULL);
				switch(option2) {
				case 0:
					SDL_VIDEO_SW_SetBpp(0);
					break;
				case 1:
					SDL_VIDEO_SW_SetBpp(8);
					break;
				case 2:
					SDL_VIDEO_SW_SetBpp(16);
					break;
				case 3:
					SDL_VIDEO_SW_SetBpp(32);
				}
			}
			break;
#if HAVE_OPENGL
		case 8:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, SDL_VIDEO_GL_pixel_format, pixel_format_menu_array, NULL);
			if (option2 >= 0)
				SDL_VIDEO_GL_SetPixelFormat(option2);
			break;
#endif /* HAVE_OPENGL */
		case 9:
			if (!SDL_VIDEO_ToggleVsync())
				UI_driver->fMessage("Not available in this video mode.", 1);
			break;
#endif /* GUI_SDL */
		case 10:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, VIDEOMODE_keep_aspect, aspect_menu_array, NULL);
			if (option2 >= 0)
				VIDEOMODE_SetKeepAspect(option2);
			break;
		case 11:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, VIDEOMODE_stretch, stretch_menu_array, NULL);
			if (option2 >= 0) {
				if (option2 < VIDEOMODE_STRETCH_CUSTOM)
					VIDEOMODE_SetStretch(option2);
				else {
					char buffer[sizeof(stretch_string)];
					snprintf(buffer, sizeof(buffer), "%f", VIDEOMODE_custom_stretch);
					if (UI_driver->fEditString("Enter multiplier", buffer, sizeof(buffer)))
						VIDEOMODE_SetCustomStretch(atof(buffer));
				}
			}
			break;
		case 12:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, VIDEOMODE_fit, fit_menu_array, NULL);
			if (option2 >= 0)
				VIDEOMODE_SetFit(option2);
			break;
		case 13:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, VIDEOMODE_horizontal_area, width_menu_array, NULL);
			if (option2 >= 0) {
				if (option2 < VIDEOMODE_HORIZONTAL_CUSTOM)
					VIDEOMODE_SetHorizontalArea(option2);
				else {
					int offset = VIDEOMODE_MIN_HORIZONTAL_AREA;
					int value = UI_driver->fSelectSlider("Adjust horizontal area",
					                                     VIDEOMODE_custom_horizontal_area - offset,
					                                     VIDEOMODE_MAX_HORIZONTAL_AREA - VIDEOMODE_MIN_HORIZONTAL_AREA,
					                                     &IntSliderLabel, &offset);
					if (value != -1)
						VIDEOMODE_SetCustomHorizontalArea(value + offset);
				}
			}
			break;
		case 14:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, VIDEOMODE_vertical_area, height_menu_array, NULL);
			if (option2 >= 0) {
				if (option2 < VIDEOMODE_VERTICAL_CUSTOM)
					VIDEOMODE_SetVerticalArea(option2);
				else {
					int offset = VIDEOMODE_MIN_VERTICAL_AREA;
					int value = UI_driver->fSelectSlider("Adjust vertical area",
					                                     VIDEOMODE_custom_vertical_area - offset,
					                                     VIDEOMODE_MAX_VERTICAL_AREA - VIDEOMODE_MIN_VERTICAL_AREA,
					                                     &IntSliderLabel, &offset);
					if (value != -1)
						VIDEOMODE_SetCustomVerticalArea(value + offset);
				}
			}
			break;
		case 15:
			switch (seltype) {
			case UI_USER_SELECT:
				{
					int range = VIDEOMODE_MAX_HORIZONTAL_AREA - VIDEOMODE_custom_horizontal_area;
					int offset = - range / 2;
					int value = UI_driver->fSelectSlider("Adjust horizontal shift",
					                                     VIDEOMODE_horizontal_offset - offset,
					                                     range, &HorizOffsetSliderLabel, &offset);
					if (value != -1)
						VIDEOMODE_SetHorizontalOffset(value + offset);
				}
				break;
			case UI_USER_DELETE:
				VIDEOMODE_SetHorizontalOffset(0);
				break;
			}
			break;
		case 16:
			switch (seltype) {
			case UI_USER_SELECT:
				{
					int range = VIDEOMODE_MAX_VERTICAL_AREA - VIDEOMODE_custom_vertical_area;
					int offset = - range / 2;
					int value = UI_driver->fSelectSlider("Adjust vertical shift",
					                                     VIDEOMODE_vertical_offset - offset,
					                                     range, &VertOffsetSliderLabel, &offset);
					if (value != -1)
						VIDEOMODE_SetVerticalOffset(value + offset);
				}
				break;
			case UI_USER_DELETE:
				VIDEOMODE_SetVerticalOffset(0);
				break;
			}
			break;
#if GUI_SDL
		case 17:
			{
				int value = UI_driver->fSelectSlider("Adjust scanlines visibility",
				                                     SDL_VIDEO_scanlines_percentage,
				                                     100, &ScanlinesSliderLabel, NULL);
				if (value != -1)
					SDL_VIDEO_SetScanlinesPercentage(value);
			}
			break;
		case 18:
			SDL_VIDEO_ToggleInterpolateScanlines();
			break;
#if HAVE_OPENGL && SDL2
		case 19:
			{
				int value = UI_driver->fSelectSlider("Adjust CRT barrel",
				                                     SDL_VIDEO_crt_barrel_distortion,
				                                     100, &CrtBarrelSliderLabel, NULL);
				if (value != -1)
					SDL_VIDEO_CrtBarrelPercentage(value);
			}
			break;
		case 20:
			{
				int value = UI_driver->fSelectSlider("Adjust CRT beam shape",
				                                     SDL_VIDEO_crt_beam_shape,
				                                     20, &CrtBeamSliderLabel, NULL);
				if (value != -1)
					SDL_VIDEO_CrtBeamShape(value);
			}
			break;
		case 21:
			{
				int value = UI_driver->fSelectSlider("Adjust CRT glow",
				                                     SDL_VIDEO_crt_phosphor_glow,
				                                     20, &CrtGlowSliderLabel, NULL);
				if (value != -1)
					SDL_VIDEO_CrtPhosphorGlow(value);
			}
			break;
#endif
#endif /* GUI_SDL */
		default:
			return;
		}
	}
}
#endif /* SUPPORTS_CHANGE_VIDEOMODE */

#if SUPPORTS_PLATFORM_PALETTEUPDATE
/* An array of pointers to colour controls (brightness, contrast, NTSC filter
   controls, etc.). Used to display values of controls in GUI. */
static struct {
	double min; /* Minimum allowed value */
	double max; /* Maximum allowed value */
	double *setting; /* Pointer to a setup value */
	char string[10]; /* string representation, for displaying */
} colour_controls[] = {
	{ COLOURS_BRIGHTNESS_MIN, COLOURS_BRIGHTNESS_MAX },
	{ COLOURS_CONTRAST_MIN, COLOURS_CONTRAST_MAX },
	{ COLOURS_SATURATION_MIN, COLOURS_SATURATION_MAX },
	{ COLOURS_HUE_MIN, COLOURS_HUE_MAX },
	{ COLOURS_GAMMA_MIN, COLOURS_GAMMA_MAX },
	{ COLOURS_DELAY_MIN, COLOURS_DELAY_MAX }
#if NTSC_FILTER
	,
	{ FILTER_NTSC_SHARPNESS_MIN, FILTER_NTSC_SHARPNESS_MAX },
	{ FILTER_NTSC_RESOLUTION_MIN, FILTER_NTSC_RESOLUTION_MAX },
	{ FILTER_NTSC_ARTIFACTS_MIN, FILTER_NTSC_ARTIFACTS_MAX },
	{ FILTER_NTSC_FRINGING_MIN, FILTER_NTSC_FRINGING_MAX },
	{ FILTER_NTSC_BLEED_MIN, FILTER_NTSC_BLEED_MAX },
	{ FILTER_NTSC_BURST_PHASE_MIN, FILTER_NTSC_BURST_PHASE_MAX }
#endif /* NTSC_FILTER */
};

/* Updates the string representation of a single colour control. */
static void UpdateColourControl(const int idx)
{
	snprintf(colour_controls[idx].string,
		 sizeof(colour_controls[0].string),
		 "%.2f",
		 *(colour_controls[idx].setting));
}

/* Sets pointers to colour controls properly. */
static void UpdateColourControls(UI_tMenuItem menu_array[])
{
	int i;
	colour_controls[0].setting = &Colours_setup->brightness;
	colour_controls[1].setting = &Colours_setup->contrast;
	colour_controls[2].setting = &Colours_setup->saturation;
	colour_controls[3].setting = &Colours_setup->hue;
	colour_controls[4].setting = &Colours_setup->gamma;
	colour_controls[5].setting = &Colours_setup->color_delay;
	for (i = 0; i < 6; i ++)
		UpdateColourControl(i);
}

/* Converts value of a colour setting to range usable by slider (0..100). */
static int ColourSettingToSlider(int index)
{
	/* min <= value <= max */
	return (int)Util_round((*(colour_controls[index].setting) - colour_controls[index].min) * 100.0 /
	                       (colour_controls[index].max - colour_controls[index].min));
}

/* Converts a slider value (0..100) to a range of a given colour setting. */
static double SliderToColourSetting(int value, int index)
{
	/* 0 <= value <= 100 */
	return (double)value * (colour_controls[index].max - colour_controls[index].min) / 100.0 + colour_controls[index].min;
}

/* Callback function that writes a text label to *LABEL, for use by a slider. */
static void ColourSliderLabel(char *label, int value, void *user_data)
{
	int index = *((int *)user_data);
	double setting = SliderToColourSetting(value, index);
	sprintf(label, "% .2f", setting);
	*colour_controls[index].setting = setting;
	UpdateColourControl(index);
	Colours_Update();
}

#ifdef RPI
static int ZoomSettingToSlider()
{
	/* 0.8 <= op_zoom <= 1.3 */
	return (int) Util_round((op_zoom - 0.8f) * 50.0 / (1.3f - 0.8f));
}
static double SliderToZoomSetting(int value)
{
	/* 0 <= value <= 50 */
	return (double) value * (1.3f - 0.8f) / 50.0 + 0.8f;
}
static void ZoomSliderLabel(char *label, int value, void *user_data)
{
	double setting = SliderToZoomSetting(value);
	sprintf(label, "% .2f", setting);
	op_zoom = setting;
}
#endif /* RPI */

#if NTSC_FILTER
/* Submenu with controls for NTSC filter. */
static void NTSCFilterSettings(void)
{
	int i;
	int preset;

	static const UI_tMenuItem preset_menu_array[] = {
		UI_MENU_ACTION(0, "Composite"),
		UI_MENU_ACTION(1, "S-Video"),
		UI_MENU_ACTION(2, "RGB"),
		UI_MENU_ACTION(3, "Monochrome"),
		UI_MENU_END
	};
	static UI_tMenuItem menu_array[] = {
		UI_MENU_SUBMENU_SUFFIX(0, "Filter preset: ", NULL),
		UI_MENU_ACTION_PREFIX(1, "Sharpness: ", colour_controls[6].string),
		UI_MENU_ACTION_PREFIX(2, "Resolution: ", colour_controls[7].string),
		UI_MENU_ACTION_PREFIX(3, "Artifacts: ", colour_controls[8].string),
		UI_MENU_ACTION_PREFIX(4, "Fringing: ", colour_controls[9].string),
		UI_MENU_ACTION_PREFIX(5, "Bleed: ", colour_controls[10].string),
		UI_MENU_ACTION_PREFIX(6, "Burst phase: ", colour_controls[11].string),
		UI_MENU_ACTION(7, "Restore default settings"),
		UI_MENU_END
	};

	int option = 0;
	int option2;

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
		preset = FILTER_NTSC_GetPreset();
		if (preset == FILTER_NTSC_PRESET_CUSTOM)
			FindMenuItem(menu_array, 0)->suffix = "Custom";
		else
			FindMenuItem(menu_array, 0)->suffix = preset_menu_array[preset].item;

		option = UI_driver->fSelect("NTSC Filter Settings", 0, option, menu_array, NULL);
		switch (option) {
		case 0:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, preset, preset_menu_array, NULL);
			if (option2 >= 0) {
				FILTER_NTSC_SetPreset(option2);
				Colours_Update();
				for (i=6; i<12; i++)
					UpdateColourControl(i);
			}
			break;
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			{
				int index = option + 5;
				int value = UI_driver->fSelectSlider("Adjust value",
								     ColourSettingToSlider(index),
								     100, &ColourSliderLabel, &index);
				if (value != -1) {
					*(colour_controls[index].setting) = SliderToColourSetting(value, index);
					UpdateColourControl(index);
					Colours_Update();
				}
			}
			break;
		case 7:
			FILTER_NTSC_RestoreDefaults();
			Colours_Update();
			for (i = 6; i < 12; i ++)
				UpdateColourControl(i);
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
	static char filename[FILENAME_MAX];
	if (UI_driver->fGetSaveFilename(filename, UI_saved_files_dir, UI_n_saved_files_dir)) {
		UI_driver->fMessage("Please wait while saving...", 0);
		if (!Colours_Save(filename))
			CantSave(filename);
	}
}
#endif /* SUPPORTS_PLATFORM_PALETTEUPDATE */

static void DisplaySettings(void)
{
	static UI_tMenuItem artif_menu_array[] = {
		UI_MENU_ACTION(ARTIFACT_NONE, "off"),
		UI_MENU_ACTION(ARTIFACT_NTSC_OLD, "old NTSC artifacts"),
		UI_MENU_ACTION(ARTIFACT_NTSC_NEW, "new NTSC artifacts"),
#if NTSC_FILTER
		UI_MENU_ACTION(ARTIFACT_NTSC_FULL, "full NTSC filter"),
#endif
#ifndef NO_SIMPLE_PAL_BLENDING
		UI_MENU_ACTION(ARTIFACT_PAL_SIMPLE, "simple PAL blending"),
#endif
#if PAL_BLENDING
		UI_MENU_ACTION(ARTIFACT_PAL_BLEND, "accurate PAL blending"),
#endif /* PAL_BLENDING */
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
	static const UI_tMenuItem colours_preset_menu_array[] = {
		UI_MENU_ACTION(0, "Standard (most realistic)"),
		UI_MENU_ACTION(1, "Deep black (allows pure black color)"),
		UI_MENU_ACTION(2, "Vibrant (enhanced colors and levels)"),
		UI_MENU_END
	};
	static char const * const colours_preset_names[] = { "Standard", "Deep black", "Vibrant", "Custom" };
#endif

	static char refresh_status[16];
#ifdef RPI
	static char op_zoom_string[16];
#endif /* RPI */
	static UI_tMenuItem menu_array[] = {
#if SUPPORTS_CHANGE_VIDEOMODE
		UI_MENU_SUBMENU(24, "Video mode settings"),
#endif /* SUPPORTS_CHANGE_VIDEOMODE */
#ifdef RPI
		UI_MENU_CHECK(30, "Filtering:"),
		{ UI_ITEM_ACTION, 31, NULL, "Zoom: ", op_zoom_string },
#endif /* RPI */
		UI_MENU_SUBMENU_SUFFIX(0, "Video artifacts:", NULL),
		UI_MENU_SUBMENU_SUFFIX(11, "NTSC artifacting mode:", NULL),
#if SUPPORTS_CHANGE_VIDEOMODE && (defined(XEP80_EMULATION) || defined(PBI_PROTO80) || defined(AF80) || defined(BIT3))
		UI_MENU_CHECK(25, "Show output of 80 column device:"),
#endif
		UI_MENU_SUBMENU_SUFFIX(1, "Current refresh rate:", refresh_status),
		UI_MENU_CHECK(2, "Accurate skipped frames:"),
		UI_MENU_CHECK(3, "Show percents of Atari speed:"),
		UI_MENU_CHECK(4, "Show disk/tape activity:"),
		UI_MENU_CHECK(5, "Show sector/block counter:"),
		UI_MENU_CHECK(26, "Show 1200XL LEDs:"),
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
		UI_MENU_ACTION(32, "Screen position configuration:"),
#endif
#if SUPPORTS_PLATFORM_PALETTEUPDATE
		UI_MENU_SUBMENU_SUFFIX(12, "Color preset: ", NULL),
		UI_MENU_ACTION_PREFIX(13, " Brightness: ", colour_controls[0].string),
		UI_MENU_ACTION_PREFIX(14, " Contrast: ", colour_controls[1].string),
		UI_MENU_ACTION_PREFIX(15, " Saturation: ", colour_controls[2].string),
		UI_MENU_ACTION_PREFIX(16, " Tint: ", colour_controls[3].string),
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

#if SUPPORTS_CHANGE_VIDEOMODE
	int option = 24;
#elif RPI
	int option = 30;
#else /* RPI */
	int option = 0;
#endif /* RPI */
	int option2;
	int seltype;

#if SUPPORTS_PLATFORM_PALETTEUPDATE
	Colours_preset_t colours_preset;
	int i;
	UpdateColourControls(menu_array);
#endif

	for (;;) {
		if (ANTIC_artif_mode == 0) { /* artifacting is off */
			FindMenuItem(menu_array, 11)->suffix = "N/A";
		} else { /* ANTIC artifacting is on */
			FindMenuItem(menu_array, 11)->suffix = artif_mode_menu_array[ANTIC_artif_mode - 1].item;
		}
		FindMenuItem(menu_array, 0)->suffix = artif_menu_array[ARTIFACT_mode].item;

#if SUPPORTS_PLATFORM_PALETTEUPDATE
		colours_preset = Colours_GetPreset();
		FindMenuItem(menu_array, 12)->suffix = colours_preset_names[colours_preset];

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

#if SUPPORTS_CHANGE_VIDEOMODE && (defined(XEP80_EMULATION) || defined(PBI_PROTO80) || defined(AF80) || defined(BIT3))
		SetItemChecked(menu_array, 25, VIDEOMODE_80_column);
#endif
		snprintf(refresh_status, sizeof(refresh_status), "1:%-2d", Atari800_refresh_rate);
#ifdef RPI
		snprintf(op_zoom_string, sizeof(op_zoom_string), "%.2f", op_zoom);
		SetItemChecked(menu_array, 30, op_filtering);
#endif /* RPI */
		SetItemChecked(menu_array, 2, Atari800_collisions_in_skipped_frames);
		SetItemChecked(menu_array, 3, Screen_show_atari_speed);
		SetItemChecked(menu_array, 4, Screen_show_disk_led);
		SetItemChecked(menu_array, 5, Screen_show_sector_counter);
		SetItemChecked(menu_array, 26, Screen_show_1200_leds);
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
#if SUPPORTS_CHANGE_VIDEOMODE
		case 24:
			VideoModeSettings();
			break;
#endif
		case 0:
			artif_menu_array[ARTIFACT_NTSC_OLD].flags =
				Atari800_tv_mode == Atari800_TV_NTSC ? UI_ITEM_ACTION : UI_ITEM_HIDDEN;
			artif_menu_array[ARTIFACT_NTSC_NEW].flags =
				Atari800_tv_mode == Atari800_TV_NTSC ? UI_ITEM_ACTION : UI_ITEM_HIDDEN;
#if NTSC_FILTER
			artif_menu_array[ARTIFACT_NTSC_FULL].flags =
				Atari800_tv_mode == Atari800_TV_NTSC ? UI_ITEM_ACTION : UI_ITEM_HIDDEN;
#endif /* NTSC_FILTER */
#ifndef NO_SIMPLE_PAL_BLENDING
			artif_menu_array[ARTIFACT_PAL_SIMPLE].flags =
				Atari800_tv_mode == Atari800_TV_PAL ? UI_ITEM_ACTION : UI_ITEM_HIDDEN;
#endif /* NO_SIMPLE_PAL_BLENDING */
#ifdef PAL_BLENDING
			artif_menu_array[ARTIFACT_PAL_BLEND].flags =
				Atari800_tv_mode == Atari800_TV_PAL ? UI_ITEM_ACTION : UI_ITEM_HIDDEN;
#endif /* PAL_BLENDING */
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, ARTIFACT_mode, artif_menu_array, NULL);
			if (option2 >= 0)
				ARTIFACT_Set((ARTIFACT_t)option2);
			break;
		case 11:
			/* The artifacting mode option is only active for ANTIC artifacting. */
			if (ANTIC_artif_mode != 0)
			{
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, ANTIC_artif_mode - 1, artif_mode_menu_array, NULL);
				if (option2 >= 0) {
					ANTIC_artif_mode = option2 + 1;
					ANTIC_UpdateArtifacting();
				}
			}
			break;
#if SUPPORTS_CHANGE_VIDEOMODE && (defined(XEP80_EMULATION) || defined(PBI_PROTO80) || defined(AF80) || defined(BIT3) )
		case 25:
			VIDEOMODE_Toggle80Column();
			if (TRUE
#ifdef XEP80_EMULATION
			    && !XEP80_enabled
#endif /* XEP80_EMULATION */
#ifdef PBI_PROTO80
			    && !PBI_PROTO80_enabled
#endif /* PBI_PROTO80 */
#ifdef AF80
			    && !AF80_enabled
#endif /* AF80 */
#ifdef BIT3
			    && !BIT3_enabled
#endif /* BIT3 */
			   )
				UI_driver->fMessage("No 80 column hardware available now.", 1);
			break;
#endif /* SUPPORTS_CHANGE_VIDEOMODE && (defined(XEP80_EMULATION) || defined(PBI_PROTO80) || defined(AF80)) */
#ifdef RPI
		case 30:
			op_filtering = !op_filtering;
			break;
		case 31:
			{
				int value = UI_driver->fSelectSlider("Adjust zoom",
								     ZoomSettingToSlider(),
								     50, &ZoomSliderLabel, NULL);
				if (value != -1) {
					op_zoom = SliderToZoomSetting(value);
				}
			}
			break;
#endif /* RPI */
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
		case 26:
			Screen_show_1200_leds = !Screen_show_1200_leds;
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
		case 32:
			ScreenPositionConfiguration();
			break;
#endif /* DREAMCAST */
#if SUPPORTS_PLATFORM_PALETTEUPDATE
		case 12:
			option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, colours_preset, colours_preset_menu_array, NULL);
			if (option2 >= 0) {
				Colours_SetPreset((Colours_preset_t)option2);
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
				int index = option - 13;
				int value = UI_driver->fSelectSlider("Adjust value",
								     ColourSettingToSlider(index),
								     100, &ColourSliderLabel, &index);
				if (value != -1) {
					*(colour_controls[index].setting) = SliderToColourSetting(value, index);
					UpdateColourControl(index);
					Colours_Update();
				}
			}
			break;
#if NTSC_FILTER
		case 19:
			if (ARTIFACT_mode == ARTIFACT_NTSC_FULL) {
				NTSCFilterSettings();
				/* While in NTSC Filter menu, the "Filter preset" option also changes the "standard" colour
				   controls (saturation etc.) - so we need to call UpdateColourControls to update the menu. */
				UpdateColourControls(menu_array);
			}
			else
				UI_driver->fMessage("Available only when NTSC filter is on", 1);
			break;
#endif
		case 20:
			Colours_RestoreDefaults();
#if NTSC_FILTER
			if (ARTIFACT_mode == ARTIFACT_NTSC_FULL)
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

#endif /* CURSES_BASIC */

#ifndef USE_CURSES

#ifdef GUI_SDL
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
	snprintf(title, sizeof(title), "Define keys for joystick %d", joystick);
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

#if SDL2
static UI_tMenuItem joy_buttons_menu_array[] = {
	UI_MENU_LABEL("  Configure controller buttons"),
	UI_MENU_LABEL("\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022\022"),
	UI_MENU_ACTION(0, ""),
	UI_MENU_ACTION(1, ""),
	UI_MENU_ACTION(2, ""),
	UI_MENU_ACTION(3, ""),
	UI_MENU_ACTION(4, ""),
	UI_MENU_ACTION(5, ""),
	UI_MENU_ACTION(6, ""),
	UI_MENU_ACTION(7, ""),
	UI_MENU_ACTION(8, ""),
	UI_MENU_ACTION(9, ""),
	UI_MENU_ACTION(10, ""),
	UI_MENU_ACTION(11, ""),
	UI_MENU_ACTION(12, ""),
	UI_MENU_ACTION(13, ""),
	UI_MENU_ACTION(14, ""),
	UI_MENU_ACTION(15, ""),
	UI_MENU_END
};
static char* joy_button_names[] = {
	"A", "B", "X", "Y",
	"Back", "Guide", "Start",
	"Left stick", "Right stick",
	"Left shoulder", "Right shoulder",
	"D-Pad up", "D-Pad down", "D-Pad left", "D-Pad right",
	"Miscellaneous"
};
static UI_tMenuItem joy_menu_action_key[] = {
	UI_MENU_ACTION(1, "Action"),
	UI_MENU_ACTION(2, "Atari key"),
	UI_MENU_ACTION(3, "Keyboard key"),
	UI_MENU_ACTION(0, "None"),
	UI_MENU_END
};

#define KEYBASE 1000
static UI_tMenuItem joy_menu_keys[] = {
	UI_MENU_ACTION(KEYBASE + AKEY_START, "Start"),
	UI_MENU_ACTION(KEYBASE + AKEY_SELECT, "Select"),
	UI_MENU_ACTION(KEYBASE + AKEY_OPTION, "Option"),
	UI_MENU_ACTION(KEYBASE + AKEY_HELP, "Help"),
	UI_MENU_ACTION(KEYBASE + AKEY_BREAK, "Break"),
	UI_MENU_END
};
static UI_tMenuItem joy_menu_actions[] = {
	UI_MENU_ACTION(KEYBASE + UI_MENU_RUN, "Run program"),
	UI_MENU_ACTION(KEYBASE + UI_MENU_DISK, "Disk"),
	UI_MENU_ACTION(KEYBASE + UI_MENU_CARTRIDGE, "Cartridge"),
	UI_MENU_ACTION(KEYBASE + AKEY_UI, "Enter setup"),
	UI_MENU_ACTION(KEYBASE + AKEY_WARMSTART, "Reset (warm)"),
	UI_MENU_ACTION(KEYBASE + AKEY_COLDSTART, "Reset (cold)"),
	UI_MENU_ACTION(KEYBASE + AKEY_TURBO, "Toggle turbo"),
	UI_MENU_ACTION(KEYBASE + AKEY_CONTROLLER_BUTTON_TRIGGER, "Joy trigger"),
	UI_MENU_ACTION(KEYBASE + UI_MENU_SAVESTATE, "Save state"),
	UI_MENU_ACTION(KEYBASE + UI_MENU_LOADSTATE, "Load state"),
	UI_MENU_ACTION(KEYBASE + UI_MENU_QUICKSAVESTATE, "Quick save state"),
	UI_MENU_ACTION(KEYBASE + UI_MENU_QUICKLOADSTATE, "Quick load state"),
	UI_MENU_ACTION(KEYBASE + AKEY_EXIT, "Quit!"),
	UI_MENU_END
};
#define BUTTONS (sizeof(joy_button_names) / sizeof(*joy_button_names))
static char joy_key_name[BUTTONS][20];

static void JoystickMenuUpdate(SDL_INPUT_RealJSConfig_t* js_config) {
	for (int i = 0; i < BUTTONS; ++i) {
		UI_tMenuItem* menu = FindMenuItem(joy_buttons_menu_array, i);
		menu->item = joy_button_names[i];
		struct INPUT_joystick_button* btn = &js_config->buttons[i];

		if (btn->action == JoystickNoAction) {
			menu->suffix = "<empty>";
		}
		else if (btn->action == JoystickKeyboard) {
			int len = sizeof(joy_key_name[i]);
			snprintf(joy_key_name[i], len - 1, "\"%.16s\"", SDL_GetKeyName(btn->key));
			menu->suffix = joy_key_name[i];
		}
		else {
			UI_tMenuItem* entry = FindMenuItem(btn->action == JoystickUiAction ? joy_menu_actions : joy_menu_keys, KEYBASE + btn->key);
			menu->suffix = entry ? entry->item : "?";
		}
	}
}

static void JoystickButtonsConfiguration(SDL_INPUT_RealJSConfig_t* js_config) {
	JoystickMenuUpdate(js_config);

	for (int option = 0; option >= 0; ) {
		option = UI_driver->fSelect("", UI_SELECT_POPUP | UI_SELECT_JOY_BTN, option, joy_buttons_menu_array, NULL);

		if (option >= AKEY_CONTROLLER_BUTTON_FIRST && option <= AKEY_CONTROLLER_BUTTON_LAST) {
			int btn = option - AKEY_CONTROLLER_BUTTON_FIRST;
			option = btn >= 0 && btn < BUTTONS ? btn : 0;
			continue;
		}

		if (option >= 0) {
			int opt = UI_driver->fSelect("", UI_SELECT_POPUP, 1, joy_menu_action_key, NULL);
			if (opt == 1) {
				// select an action
				int action = UI_driver->fSelect("", UI_SELECT_POPUP, 0, joy_menu_actions, NULL);
				if (action >= 0) {
					js_config->buttons[option].key = action - KEYBASE;
					js_config->buttons[option].action = JoystickUiAction;
				}
			}
			else if (opt == 2) {
				// assign a key
				int key = UI_driver->fSelect("", UI_SELECT_POPUP, 0, joy_menu_keys, NULL);
				if (key >= 0) {
					js_config->buttons[option].key = key - KEYBASE;
					js_config->buttons[option].action = JoystickAtariKey;
				}
			}
			else if (opt == 3) {
				// keyboard key
				js_config->buttons[option].key = GetRawKey();
				js_config->buttons[option].action = JoystickKeyboard;
			}
			else if (opt == 0) {
				// reset
				js_config->buttons[option].key = 0;
				js_config->buttons[option].action = JoystickNoAction;
			}
	
			if (opt >= 0) {
				JoystickMenuUpdate(js_config);
			}
		}
	}
}

#endif /* SDL2 */

static void RealJoystickConfiguration(void)
{
	char title[40];
	int option = 0;
	int i;
	SDL_INPUT_RealJSConfig_t *js_config;
#if SDL2
	static UI_tMenuItem real_js_menu_array[] = {
		UI_MENU_LABEL("Joystick 1"),
		UI_MENU_CHECK(0, " Use hat/D-Pad:"),
		UI_MENU_ACTION(1, " Analog axes:"),
		UI_MENU_ACTION(2, " Diagonals zone:"),
		UI_MENU_ACTION(3, " Configure buttons"),
		UI_MENU_LABEL("Joystick 2"),
		UI_MENU_CHECK(4, " Use hat/D-Pad:"),
		UI_MENU_ACTION(5, " Analog axes:"),
		UI_MENU_ACTION(6, " Diagonals zone:"),
		UI_MENU_ACTION(7, " Configure buttons"),
		UI_MENU_LABEL("Joystick 3"),
		UI_MENU_CHECK(8, " Use hat/D-Pad:"),
		UI_MENU_ACTION(9, " Analog axes:"),
		UI_MENU_ACTION(10, " Diagonals zone:"),
		UI_MENU_ACTION(11, " Configure buttons"),
		UI_MENU_LABEL("Joystick 4"),
		UI_MENU_CHECK(12, " Use hat/D-Pad:"),
		UI_MENU_ACTION(13, " Analog axes:"),
		UI_MENU_ACTION(14, " Diagonals zone:"),
		UI_MENU_ACTION(15, " Configure buttons"),
		UI_MENU_END
	};

	snprintf(title, sizeof (title), "Configuration of Real Joysticks");

	for (;;) {
		/*Set the CHECK items*/
		for (i = 0; i < 4; i++) {
			int opt = i * 4;
			SDL_INPUT_RealJSConfig_t* cfg = SDL_INPUT_GetRealJSConfig(i);
			SetItemChecked(real_js_menu_array, opt++, cfg->use_hat);
		
			FindMenuItem(real_js_menu_array, opt++)->suffix = cfg->axes == 0 ? "1&2" : "3&4";

			FindMenuItem(real_js_menu_array, opt)->suffix =
				cfg->diagonal_zones == JoystickNarrowDiagonalsZone ?
					"Narrow" : (cfg->diagonal_zones == JoystickWideDiagonalsZone ?  "Wide" : "None");
		}

		option = UI_driver->fSelect(title, 0, option, real_js_menu_array, NULL);

		if (option < 0) break;

		js_config = SDL_INPUT_GetRealJSConfig(option / 4);
		switch (option) {
			case 0:
			case 4:
			case 8:
			case 12:
				js_config->use_hat = !js_config->use_hat;
				break;
			case 1:
			case 5:
			case 9:
			case 13:
				js_config->axes = js_config->axes ? 0 : 2;
				break;
			case 2:
			case 6:
			case 10:
			case 14:
				js_config->diagonal_zones = (js_config->diagonal_zones + 1) % 3;
				break;
			case 3:
			case 7:
			case 11:
			case 15:
				// configure buttons
				JoystickButtonsConfiguration(js_config);
				break;
			default:
				break;
		}
	}
#else
	static UI_tMenuItem real_js_menu_array[] = {
		UI_MENU_LABEL("Joystick 1"),
		UI_MENU_CHECK(0, "Use hat/D-PAD:"),
		UI_MENU_LABEL("Joystick 2"),
		UI_MENU_CHECK(1, "Use hat/D-PAD:"),
		UI_MENU_LABEL("Joystick 3"),
		UI_MENU_CHECK(2, "Use hat/D-PAD:"),
		UI_MENU_LABEL("Joystick 4"),
		UI_MENU_CHECK(3, "Use hat/D-PAD:"),
		UI_MENU_END
	};

	snprintf(title, sizeof (title), "Configuration of Real Joysticks");

	for (;;) {
		/*Set the CHECK items*/
		for (i = 0; i < 4; i++) {
			SetItemChecked(real_js_menu_array, i, SDL_INPUT_GetRealJSConfig(i)->use_hat);
		}

		option = UI_driver->fSelect(title, 0, option, real_js_menu_array, NULL);

		if (option < 0) break;

		switch (option) {
			case 0:
				js_config = SDL_INPUT_GetRealJSConfig(0);
				js_config->use_hat = !js_config->use_hat;
				break;
			case 1:
				js_config = SDL_INPUT_GetRealJSConfig(1);
				js_config->use_hat = !js_config->use_hat;
				break;
			case 2:
				js_config = SDL_INPUT_GetRealJSConfig(2);
				js_config->use_hat = !js_config->use_hat;
				break;
			case 3:
				js_config = SDL_INPUT_GetRealJSConfig(3);
				js_config->use_hat = !js_config->use_hat;
				break;
		}
	}
#endif /* SDL2 */
}
#endif

static void ControllerConfiguration(void)
{
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
		UI_MENU_ACTION(10, "Joystick/D-Pad configuration"),
		UI_MENU_ACTION(11, "Button configuration"),
#else
		UI_MENU_SUBMENU_SUFFIX(2, "Mouse device: ", NULL),
		UI_MENU_SUBMENU_SUFFIX(3, "Mouse port:", mouse_port_status),
		UI_MENU_SUBMENU_SUFFIX(4, "Mouse speed:", mouse_speed_status),
#endif
#ifdef GUI_SDL
		UI_MENU_CHECK(5, "Enable keyboard joystick 1:"),
		UI_MENU_SUBMENU(6, "Define layout of keyboard joystick 1"),
		UI_MENU_CHECK(7, "Enable keyboard joystick 2:"),
		UI_MENU_SUBMENU(8, "Define layout of keyboard joystick 2"),
		UI_MENU_SUBMENU(9, "Configure real joysticks"),
#endif
		UI_MENU_END
	};

	int option = 0;
#if !defined(_WIN32_WCE) && !defined(DREAMCAST)
	int option2;
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
#ifdef GUI_SDL
		SetItemChecked(menu_array, 5, PLATFORM_IsKbdJoystickEnabled(0));
		SetItemChecked(menu_array, 7, PLATFORM_IsKbdJoystickEnabled(1));
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
			JoystickConfiguration();
			break;
		case 11:
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
#ifdef GUI_SDL
		case 5:
			PLATFORM_ToggleKbdJoystickEnabled(0);
			break;
		case 6:
			KeyboardJoystickConfiguration(0);
			break;
		case 7:
			PLATFORM_ToggleKbdJoystickEnabled(1);
			break;
		case 8:
			KeyboardJoystickConfiguration(1);
			break;
		case 9: RealJoystickConfiguration();
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
	Sound_setup_t setup = Sound_desired;
	int sound_out_valid = setup.buffer_ms == 0;	/* Sound_out has been initialized */
	int update_sound_params = TRUE;
	static char freq_string[9]; /* "nnnnn Hz\0" */
	static char hw_buflen_string[15]; /* "auto (nnnn ms)\0" */
	static char latency_string[8]; /* nnnn ms\0" */

	static const unsigned int freq_values[] = {
		8192,
		11025,
		22050,
		44100,
		48000
	};
	static const UI_tMenuItem freq_menu_array[] = {
		UI_MENU_ACTION(0, "8192 Hz"),
		UI_MENU_ACTION(1, "11025 Hz"),
		UI_MENU_ACTION(2, "22050 Hz"),
		UI_MENU_ACTION(3, "44100 Hz"),
		UI_MENU_ACTION(4, "48000 Hz"),
		UI_MENU_ACTION(5, "custom"),
		UI_MENU_END
	};

	static const UI_tMenuItem hw_buflen_menu_array[] = {
		UI_MENU_ACTION(0, "choose automatically"),
		UI_MENU_ACTION(20, "20 ms"),
		UI_MENU_ACTION(40, "40 ms"),
		UI_MENU_ACTION(80, "80 ms"),
		UI_MENU_ACTION(160, "160 ms"),
		UI_MENU_ACTION(320, "320 ms"),
		UI_MENU_ACTION(1, "custom"),
		UI_MENU_END
	};

	static UI_tMenuItem menu_array[] = {
		UI_MENU_CHECK(0, "Enable sound:"),
		UI_MENU_SUBMENU_SUFFIX(1, "Frequency:", freq_string),
		UI_MENU_ACTION(2, "Bit depth:"),
		UI_MENU_SUBMENU_SUFFIX(3, "Hardware buffer length:", hw_buflen_string),
		UI_MENU_SUBMENU_SUFFIX(4, "Latency:", latency_string),
#ifdef DREAMCAST
		UI_MENU_CHECK(0, "Enable sound:"),
#endif
#ifdef STEREO_SOUND
		UI_MENU_CHECK(5, "Dual POKEY (Stereo):"),
#endif
		UI_MENU_CHECK(6, "High Fidelity POKEY:"),
#ifdef CONSOLE_SOUND
		UI_MENU_CHECK(7, "Speaker (Key Click):"),
#endif
		UI_MENU_ACTION(9, "Enable higher frequencies:"),
		UI_MENU_END
	};

	int option = 0;

	for (;;) {
		if (update_sound_params) {
			SetItemChecked(menu_array, 0, Sound_enabled);
			snprintf(freq_string, sizeof(freq_string), "%i Hz", setup.freq);
			menu_array[2].suffix = setup.sample_size == 2 ? "16 bit" : "8 bit";
			if (setup.buffer_ms == 0) {
				if (Sound_enabled && sound_out_valid) {
					snprintf(hw_buflen_string, sizeof(hw_buflen_string), "auto (%u ms)", Sound_out.buffer_ms);
					sound_out_valid = FALSE;
				}
				else
					strncpy(hw_buflen_string, "auto", sizeof(hw_buflen_string));
			}
			else
				snprintf(hw_buflen_string, sizeof(hw_buflen_string), "%u ms", setup.buffer_ms);
#ifdef STEREO_SOUND
			SetItemChecked(menu_array, 5, setup.channels == 2);
#endif /* STEREO_SOUND */
		}
		snprintf(latency_string, sizeof(latency_string), "%u ms", Sound_latency);
		SetItemChecked(menu_array, 6, POKEYSND_enable_new_pokey);
#ifdef CONSOLE_SOUND
		SetItemChecked(menu_array, 7, POKEYSND_console_sound_enabled);
#endif
		FindMenuItem(menu_array, 9)->suffix = POKEYSND_enable_new_pokey ? "N/A" : POKEYSND_bienias_fix ? "Yes" : "No ";

		option = UI_driver->fSelect("Sound Settings", 0, option, menu_array, NULL);
		switch (option) {
		case 0:
			if (Sound_enabled)
				Sound_Exit();
			else {
				Sound_desired = setup;
				if (!Sound_Setup())
					UI_driver->fMessage("Error: can't open sound device", 1);
				else {
					setup = Sound_desired;
					sound_out_valid = TRUE;
				}
			}
			update_sound_params = TRUE;
			break;
		case 1:
			{
				int option2;
				int current;
				for (current = 0; freq_menu_array[current].retval < 5; ++current) {
					/* Find the currently-chosen frequency. */
					if (freq_values[freq_menu_array[current].retval] == setup.freq)
						break;
				}
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, current, freq_menu_array, NULL);
				if (option2 == 5) {
					snprintf(freq_string, sizeof(freq_string), "%u", setup.freq); /* Remove " Hz" suffix */
					if (UI_driver->fEditString("Enter sound frequency", freq_string, sizeof(freq_string)-3))
						setup.freq = atoi(freq_string);
				}
				else if (option2 >= 0)
					setup.freq = freq_values[option2];

				update_sound_params = (option2 >= 0);
			}
			break;
		case 2:
			setup.sample_size = 3 - setup.sample_size; /* Toggle 1<->2 */
			update_sound_params = TRUE;
			break;
		case 3:
			{
				int current = 1; /* 1 means "custom" as in hw_buflen_menu_array */
				int i, option2;
				for (i = 0; hw_buflen_menu_array[i].retval != 1; ++i) {
					/* Find the currently-chosen buffer length. */
					if (setup.buffer_ms == hw_buflen_menu_array[i].retval) {
						current = setup.buffer_ms;
						break;
					}
				}
				option2 = UI_driver->fSelect(NULL, UI_SELECT_POPUP, current, hw_buflen_menu_array, NULL);
				if (option2 == 1) {
					snprintf(hw_buflen_string, sizeof(hw_buflen_string), "%u", setup.buffer_ms); /* Remove " ms" suffix */
					if (UI_driver->fEditString("Enter hardware buffer length", hw_buflen_string, sizeof(hw_buflen_string)-3))
						setup.buffer_ms = atoi(hw_buflen_string);
				}
				else if (option2 >= 0)
					setup.buffer_ms = option2;

				update_sound_params = (option2 >= 0);
			}
			break;
		case 4:
			snprintf(latency_string, sizeof(latency_string), "%u", Sound_latency); /* Remove " ms" suffix */
			if (UI_driver->fEditString("Enter sound latency", latency_string, sizeof(latency_string)-3))
				Sound_SetLatency(atoi(latency_string));
			break;
#ifdef STEREO_SOUND
		case 5:
			setup.channels = 3 - setup.channels; /* Toggle 1<->2 */
			update_sound_params = TRUE;
			break;
#endif
		case 6:
			POKEYSND_enable_new_pokey = !POKEYSND_enable_new_pokey;
			POKEYSND_DoInit();
			/* According to the PokeySnd doc the POKEY switch can occur on
			   a cold-restart only */
			UI_driver->fMessage("Will reboot to apply the change", 1);
			return TRUE; /* reboot required */
#ifdef CONSOLE_SOUND
		case 7:
			POKEYSND_console_sound_enabled = !POKEYSND_console_sound_enabled;
			break;
#endif
		case 9:
			if (!POKEYSND_enable_new_pokey)
				POKEYSND_bienias_fix = !POKEYSND_bienias_fix;
			break;
		default:
			if (!Sound_enabled)
				/* Only store setup from menu in Sound_desired. */
				Sound_desired = setup;
			else if (setup.freq        != Sound_desired.freq ||
			         setup.sample_size != Sound_desired.sample_size ||
#ifdef STEREO_SOUND
			         setup.channels    != Sound_desired.channels ||
#endif
			         setup.buffer_ms != Sound_desired.buffer_ms) {
				/* Sound output reinitialisation needed. */
				Sound_desired = setup;
				if (!Sound_Setup()) {
					UI_driver->fMessage("Error: can't open sound device", 1);
					/* Don't leave menu on failure. */
					break;
				}
				setup = Sound_desired;
			}
			return FALSE;
		}
	}
}

#endif /* SOUND */

#ifdef SCREENSHOTS

static void Screenshot(int interlaced)
{
	static char filename[FILENAME_MAX];
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
		"(c)1998-2023 Atari800 Development Team\0"
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

int UI_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc); /* is argument available? */
		int a_m = FALSE; /* error, argument missing! */
		int a_i = FALSE; /* error, argument invalid! */

		if (strcmp(argv[i], "-atari_files") == 0) {
			if (i_a) {
				if (UI_n_atari_files_dir >= UI_MAX_DIRECTORIES)
					Log_print("All ATARI_FILES_DIR slots used!");
				else
					Util_strlcpy(UI_atari_files_dir[UI_n_atari_files_dir++], argv[++i], FILENAME_MAX);
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-saved_files") == 0) {
			if (i_a) {
				if (UI_n_saved_files_dir >= UI_MAX_DIRECTORIES)
					Log_print("All SAVED_FILES_DIR slots used!");
				else
					Util_strlcpy(UI_saved_files_dir[UI_n_saved_files_dir++], argv[++i], FILENAME_MAX);
			}
			else a_m = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-atari_files <path>  Set default path for Atari executables");
				Log_print("\t-saved_files <path>  Set default path for saved files");
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		} else if (a_i) {
			Log_print("Invalid argument for '%s'", argv[--i]);
			return FALSE;
		}
	}
	*argc = j;

	return TRUE;
}

void UI_Run(void)
{
	static UI_tMenuItem menu_array[] = {
		UI_MENU_FILESEL_ACCEL(UI_MENU_RUN, "Run Atari Program", "Alt+R"),
		UI_MENU_SUBMENU_ACCEL(UI_MENU_DISK, "Disk Management", "Alt+D"),
		UI_MENU_SUBMENU_ACCEL(UI_MENU_CARTRIDGE, "Cartridge Management", "Alt+C"),
		UI_MENU_SUBMENU_ACCEL(UI_MENU_CASSETTE, "Tape Management", "Alt+T"),
		UI_MENU_SUBMENU_ACCEL(UI_MENU_SYSTEM, "System Settings", "Alt+Y"),
#ifdef SOUND
		UI_MENU_SUBMENU_ACCEL(UI_MENU_SOUND, "Sound Settings", "Alt+O"),
#ifndef DREAMCAST
		UI_MENU_ACTION_ACCEL(UI_MENU_SOUND_RECORDING, "Sound Recording Start/Stop", "Alt+W"),
#endif
#endif
#ifdef VIDEO_RECORDING
		UI_MENU_ACTION_ACCEL(UI_MENU_VIDEO_RECORDING, "Video Recording Start/Stop", "Alt+V"),
#endif
#ifndef CURSES_BASIC
		UI_MENU_SUBMENU(UI_MENU_DISPLAY, "Display Settings"),
#endif
#ifndef USE_CURSES
		UI_MENU_SUBMENU(UI_MENU_CONTROLLER, "Controller Configuration"),
#endif
		UI_MENU_SUBMENU(UI_MENU_SETTINGS, "Emulator Configuration"),
		UI_MENU_FILESEL_ACCEL(UI_MENU_SAVESTATE, "Save State", "Alt+S"),
		UI_MENU_FILESEL_ACCEL(UI_MENU_LOADSTATE, "Load State", "Alt+L"),
#if SCREENSHOTS
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
#else
		UI_MENU_ACTION_ACCEL(UI_MENU_MONITOR, "Enter Monitor", "F8"),
#endif
		UI_MENU_ACTION_ACCEL(UI_MENU_ABOUT, "About the Emulator", "Alt+A"),
		UI_MENU_ACTION_ACCEL(UI_MENU_EXIT, "Exit Emulator", "F9"),
		UI_MENU_END
	};

	int option = UI_MENU_RUN;
	int done = FALSE;
#if SUPPORTS_CHANGE_VIDEOMODE
	VIDEOMODE_ForceStandardScreen(TRUE);
#endif

	UI_is_active = TRUE;

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
			TapeManagement();
			break;
		case UI_MENU_SYSTEM:
			SystemSettings();
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
#ifdef AUDIO_RECORDING
		case UI_MENU_SOUND_RECORDING:
			SoundRecording();
			break;
#endif
#endif
#ifdef VIDEO_RECORDING
		case UI_MENU_VIDEO_RECORDING:
			VideoRecording();
			break;
#endif
		case UI_MENU_SAVESTATE:
			SaveState();
			break;
		case UI_MENU_LOADSTATE:
			/* Note: AutostartFile() handles state files, too,
			   so we can remove LoadState() now. */
			LoadState();
			break;
		case UI_MENU_QUICKSAVESTATE:
			QuickSaveState();
			break;
		case UI_MENU_QUICKLOADSTATE:
			QuickLoadState();
			break;
#ifndef CURSES_BASIC
		case UI_MENU_DISPLAY:
			DisplaySettings();
			break;
#ifdef SCREENSHOTS
		case UI_MENU_PCX:
			Screenshot(FALSE);
			break;
		case UI_MENU_PCXI:
			Screenshot(TRUE);
			break;
#endif
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
		case UI_MENU_MONITOR:
#if defined(_WIN32_WCE)
			AboutPocketAtari();
			break;
#elif defined(DREAMCAST)
			AboutAtariDC();
			break;
#else
			if (Atari800_Exit(TRUE))
				break;
			/* if 'quit' typed in monitor, exit emulator */
			exit(0);
#endif
		case UI_MENU_EXIT:
			Atari800_Exit(FALSE);
			exit(0);
		}
	}

	/* Sound_Active(TRUE); */
	UI_is_active = FALSE;
	
	/* flush keypresses */
	while (PLATFORM_Keyboard() != AKEY_NONE)
		Atari800_Sync();

	UI_alt_function = UI_current_function = -1;
	/* restore 80 column screen */
#if SUPPORTS_CHANGE_VIDEOMODE
	VIDEOMODE_ForceStandardScreen(FALSE);
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
#if !defined(_WIN32_WCE) && !defined(DREAMCAST)
		UI_MENU_ACTION_ACCEL(3, "Enter Monitor", "F8"),
#endif
		UI_MENU_ACTION_ACCEL(4, "Continue After CIM", "Esc"),
		UI_MENU_ACTION_ACCEL(5, "Exit Emulator", "F9"),
		UI_MENU_END
	};

	int option = 0;
	snprintf(cim_info, sizeof(cim_info), "Code $%02X (CIM) at address $%04X", UI_crash_code, UI_crash_address);

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

/*
vim:ts=4:sw=4:
*/

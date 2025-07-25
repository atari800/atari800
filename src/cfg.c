/*
 * cfg.c - Emulator Configuration
 *
 * Copyright (c) 1995-1998 David Firth
 * Copyright (c) 1998-2014 Atari800 development team (see DOC/CREDITS)
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
#include "artifact.h"
#include "atari.h"
#include <stdlib.h>
#include "cartridge.h"
#include "cassette.h"
#include "binload.h"
#include "cfg.h"
#include "devices.h"
#include "esc.h"
#include "log.h"
#include "memory.h"
#include "pbi.h"
#include "rtime.h"
#include "sysrom.h"
#ifdef XEP80_EMULATION
#include "xep80.h"
#endif
#ifdef AF80
#include "af80.h"
#endif
#ifdef BIT3
#include "bit3.h"
#endif
#include "platform.h"
#include "pokeysnd.h"
#include "ui.h"
#include "util.h"
#if !defined(BASIC) && !defined(CURSES_BASIC)
#include "colours.h"
#include "screen.h"
#endif
#ifdef NTSC_FILTER
#include "filter_ntsc.h"
#endif
#if SUPPORTS_CHANGE_VIDEOMODE
#include "videomode.h"
#endif
#ifdef SOUND
#include "sound.h"
#endif
#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ) || defined(AUDIO_RECORDING) || defined(VIDEO_RECORDING)
#include "file_export.h"
#endif

int CFG_save_on_exit = FALSE;

/* If another default path config path is defined use it
   otherwise use the default one */
#ifndef DEFAULT_CFG_NAME
#define DEFAULT_CFG_NAME ".atari800.cfg"
#endif

#ifndef SYSTEM_WIDE_CFG_FILE
#define SYSTEM_WIDE_CFG_FILE "/etc/atari800.cfg"
#endif

static char rtconfig_filename[FILENAME_MAX];

int CFG_LoadConfig(const char *alternate_config_filename)
{
	FILE *fp;
	const char *fname = rtconfig_filename;
	char string[256];
#ifndef BASIC
	int was_obsolete_dir = FALSE;
#endif

#ifdef SUPPORTS_PLATFORM_CONFIGINIT
	PLATFORM_ConfigInit();
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
		Log_print("User config file '%s' not found.", rtconfig_filename);

#ifdef SYSTEM_WIDE_CFG_FILE
		/* try system wide config file */
		fname = SYSTEM_WIDE_CFG_FILE;
		Log_print("Trying system wide config file: %s", fname);
		fp = fopen(fname, "r");
#endif
		if (fp == NULL) {
			Log_print("No configuration file found, will create fresh one from scratch:");
			return FALSE;
		}
	}

	if (fgets(string, sizeof(string), fp) != NULL) {
		Log_print("Using Atari800 config file: %s\nCreated by %s", fname, string);
	}

	while (fgets(string, sizeof(string), fp)) {
		char *ptr;
		Util_chomp(string);
		Util_trim(string);
		/* Check for comments */
		if (string[0] == '#') {
			continue;
		}
		ptr = strchr(string, '=');
		if (ptr != NULL) {
			*ptr++ = '\0';
			Util_trim(string);
			Util_trim(ptr);

			if (SYSROM_ReadConfig(string, ptr)) {
			}
#ifdef BASIC
			else if (strcmp(string, "ATARI_FILES_DIR") == 0
				  || strcmp(string, "SAVED_FILES_DIR") == 0
				  || strcmp(string, "DISK_DIR") == 0 || strcmp(string, "ROM_DIR") == 0
				  || strcmp(string, "EXE_DIR") == 0 || strcmp(string, "STATE_DIR") == 0)
				/* do nothing */;
#else
			else if (strcmp(string, "ATARI_FILES_DIR") == 0) {
				if (UI_n_atari_files_dir >= UI_MAX_DIRECTORIES)
					Log_print("All ATARI_FILES_DIR slots used!");
				else
					Util_strlcpy(UI_atari_files_dir[UI_n_atari_files_dir++], ptr, FILENAME_MAX);
			}
			else if (strcmp(string, "SAVED_FILES_DIR") == 0) {
				if (UI_n_saved_files_dir >= UI_MAX_DIRECTORIES)
					Log_print("All SAVED_FILES_DIR slots used!");
				else
					Util_strlcpy(UI_saved_files_dir[UI_n_saved_files_dir++], ptr, FILENAME_MAX);
			}
			else if (strcmp(string, "SHOW_HIDDEN_FILES") == 0)
				UI_show_hidden_files = Util_sscanbool(ptr);
			else if (strcmp(string, "DISK_DIR") == 0 || strcmp(string, "ROM_DIR") == 0
				  || strcmp(string, "EXE_DIR") == 0 || strcmp(string, "STATE_DIR") == 0) {
				/* ignore blank and "." values */
				if (ptr[0] != '\0' && (ptr[0] != '.' || ptr[1] != '\0'))
					was_obsolete_dir = TRUE;
			}
#endif
			else if (strcmp(string, "H1_DIR") == 0)
				Util_strlcpy(Devices_atari_h_dir[0], ptr, FILENAME_MAX);
			else if (strcmp(string, "H2_DIR") == 0)
				Util_strlcpy(Devices_atari_h_dir[1], ptr, FILENAME_MAX);
			else if (strcmp(string, "H3_DIR") == 0)
				Util_strlcpy(Devices_atari_h_dir[2], ptr, FILENAME_MAX);
			else if (strcmp(string, "H4_DIR") == 0)
				Util_strlcpy(Devices_atari_h_dir[3], ptr, FILENAME_MAX);
			else if (strcmp(string, "HD_READ_ONLY") == 0)
				Devices_h_read_only = Util_sscandec(ptr);
			else if (strcmp(string, "HD_DEVICE_NAME") == 0)
				Devices_h_device_name = *ptr;

			else if (strcmp(string, "PRINT_COMMAND") == 0) {
				if (!Devices_SetPrintCommand(ptr))
					Log_print("Unsafe PRINT_COMMAND ignored");
			}

			else if (strcmp(string, "ACCURATE_SKIPPED_FRAMES") == 0)
				Atari800_collisions_in_skipped_frames = Util_sscanbool(ptr);
			else if (strcmp(string, "SCREEN_REFRESH_RATIO") == 0)
				Atari800_refresh_rate = Util_sscandec(ptr);
			else if (strcmp(string, "DISABLE_BASIC") == 0)
				Atari800_disable_basic = Util_sscanbool(ptr);
			else if (strcmp(string, "TURBO_SPEED") == 0) {
				Atari800_turbo_speed = Util_sscandec(ptr);
			}
			else if (strcmp(string, "ENABLE_SIO_PATCH") == 0) {
				ESC_enable_sio_patch = Util_sscanbool(ptr);
			}
			else if (strcmp(string, "ENABLE_SLOW_XEX_LOADING") == 0) {
				BINLOAD_slow_xex_loading = Util_sscanbool(ptr);
			}
			else if (strcmp(string, "ENABLE_H_PATCH") == 0) {
				Devices_enable_h_patch = Util_sscanbool(ptr);
			}
			else if (strcmp(string, "ENABLE_P_PATCH") == 0) {
				Devices_enable_p_patch = Util_sscanbool(ptr);
			}
			else if (strcmp(string, "ENABLE_R_PATCH") == 0) {
				Devices_enable_r_patch = Util_sscanbool(ptr);
			}

			else if (strcmp(string, "ENABLE_NEW_POKEY") == 0) {
#ifdef SOUND
				POKEYSND_enable_new_pokey = Util_sscanbool(ptr);
#endif /* SOUND */
			}
			else if (strcmp(string, "STEREO_POKEY") == 0) {
#ifdef STEREO_SOUND
				POKEYSND_stereo_enabled = Util_sscanbool(ptr);
				Sound_desired.channels = POKEYSND_stereo_enabled ? 2 : 1;
#endif /* STEREO_SOUND */
			}
			else if (strcmp(string, "SPEAKER_SOUND") == 0) {
#ifdef CONSOLE_SOUND
				POKEYSND_console_sound_enabled = Util_sscanbool(ptr);
#endif
			}
			else if (strcmp(string, "MACHINE_TYPE") == 0) {
				if (strcmp(ptr, "Atari 400/800") == 0 ||
				    /* Also recognise legacy values of this parameter */
				    strcmp(ptr, "Atari OS/A") == 0 ||
				    strcmp(ptr, "Atari OS/B") == 0)
					Atari800_machine_type = Atari800_MACHINE_800;
				else if (strcmp(ptr, "Atari XL/XE") == 0)
					Atari800_machine_type = Atari800_MACHINE_XLXE;
				else if (strcmp(ptr, "Atari 5200") == 0)
					Atari800_machine_type = Atari800_MACHINE_5200;
				else
					Log_print("Invalid machine type: %s", ptr);
			}
			else if (strcmp(string, "RAM_SIZE") == 0) {
				if (strcmp(ptr, "320 (RAMBO)") == 0)
					MEMORY_ram_size = MEMORY_RAM_320_RAMBO;
				else if (strcmp(ptr, "320 (COMPY SHOP)") == 0)
					MEMORY_ram_size = MEMORY_RAM_320_COMPY_SHOP;
				else {
					int size = Util_sscandec(ptr);
					if (MEMORY_SizeValid(size))
						MEMORY_ram_size = size;
					else
						Log_print("Invalid RAM size: %s", ptr);
				}
			}
			else if (strcmp(string, "DEFAULT_TV_MODE") == 0) {
				if (strcmp(ptr, "PAL") == 0)
					Atari800_tv_mode = Atari800_TV_PAL;
				else if (strcmp(ptr, "NTSC") == 0)
					Atari800_tv_mode = Atari800_TV_NTSC;
				else
					Log_print("Invalid TV Mode: %s", ptr);
			}
			else if (strcmp(string, "MOSAIC_RAM_NUM_BANKS") == 0) {
				int num = Util_sscandec(ptr);
				if (num >= 0 && num <= 64)
					MEMORY_mosaic_num_banks = num;
				else
					Log_print("Invalid Mosaic RAM number of banks: %s", ptr);
			}
			else if (strcmp(string, "AXLON_RAM_NUM_BANKS") == 0) {
				int num = Util_sscandec(ptr);
				if (num == 0 || num == 8 || num == 16 || num == 32 || num == 64 || num == 128 || num == 256)
					MEMORY_axlon_num_banks = num;
				else
					Log_print("Invalid Mosaic RAM number of banks: %s", ptr);
			}
			else if (strcmp(string, "ENABLE_MAPRAM") == 0)
				MEMORY_enable_mapram = Util_sscanbool(ptr);
			else if (strcmp(string, "BUILTIN_BASIC") == 0)
				Atari800_builtin_basic = Util_sscanbool(ptr);
			else if (strcmp(string, "KEYBOARD_LEDS") == 0)
				Atari800_keyboard_leds = Util_sscanbool(ptr);
			else if (strcmp(string, "F_KEYS") == 0)
				Atari800_f_keys = Util_sscanbool(ptr);
			else if (strcmp(string, "BUILTIN_GAME") == 0)
				Atari800_builtin_game = Util_sscanbool(ptr);
			else if (strcmp(string, "KEYBOARD_DETACHED") == 0)
				Atari800_keyboard_detached = Util_sscanbool(ptr);
			else if (strcmp(string, "1200XL_JUMPER") == 0)
				Atari800_jumper = Util_sscanbool(ptr);
			else if (strcmp(string, "CFG_SAVE_ON_EXIT") == 0) {
				CFG_save_on_exit = Util_sscanbool(ptr);
			}
			/* Add module-specific configurations here */
			else if (PBI_ReadConfig(string,ptr)) {
			}
			else if (CARTRIDGE_ReadConfig(string, ptr)) {
			}
			else if (CASSETTE_ReadConfig(string, ptr)) {
			}
			else if (RTIME_ReadConfig(string, ptr)) {
			}
#ifdef XEP80_EMULATION
			else if (XEP80_ReadConfig(string, ptr)) {
			}
#endif
#ifdef AF80
			else if (AF80_ReadConfig(string,ptr)) {
			}
#endif
#ifdef BIT3
			else if (BIT3_ReadConfig(string,ptr)) {
			}
#endif
#if !defined(BASIC) && !defined(CURSES_BASIC)
			else if (Colours_ReadConfig(string, ptr)) {
			}
			else if (ARTIFACT_ReadConfig(string, ptr)) {
			}
			else if (Screen_ReadConfig(string, ptr)) {
			}
#endif
#ifdef NTSC_FILTER
			else if (FILTER_NTSC_ReadConfig(string, ptr)) {
			}
#endif
#if SUPPORTS_CHANGE_VIDEOMODE
			else if (VIDEOMODE_ReadConfig(string, ptr)) {
			}
#endif
#ifdef SOUND
			else if (Sound_ReadConfig(string, ptr)) {
			}
#endif /* SOUND */
#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ) || defined(AUDIO_RECORDING) || defined(VIDEO_RECORDING)
			else if (File_Export_ReadConfig(string, ptr)) {
			}
#endif
			else {
#ifdef SUPPORTS_PLATFORM_CONFIGURE
				if (!PLATFORM_Configure(string, ptr)) {
					Log_print("Unrecognized variable or bad parameters: '%s=%s'", string, ptr);
				}
#else
				Log_print("Unrecognized variable: %s", string);
#endif
			}
		}
		else {
			Log_print("Ignored config line: %s", string);
		}
	}

	fclose(fp);
#ifndef BASIC
	if (was_obsolete_dir) {
		Log_print(
			"DISK_DIR, ROM_DIR, EXE_DIR and STATE_DIR configuration options\n"
			"are no longer supported. Please use ATARI_FILES_DIR\n"
			"and SAVED_FILES_DIR in your Atari800 configuration file.");
	}
#endif
	return TRUE;
}

int CFG_WriteConfig(void)
{
	FILE *fp;
	int i;
	static const char * const machine_type_string[Atari800_MACHINE_SIZE] = {
		"400/800", "XL/XE", "5200"
	};

	fp = fopen(rtconfig_filename, "w");
	if (fp == NULL) {
		perror(rtconfig_filename);
		Log_print("Cannot write to config file: %s", rtconfig_filename);
		return FALSE;
	}
	Log_print("Writing config file: %s", rtconfig_filename);

	fprintf(fp, "%s\n", Atari800_TITLE);
	SYSROM_WriteConfig(fp);
#ifndef BASIC
	for (i = 0; i < UI_n_atari_files_dir; i++)
		fprintf(fp, "ATARI_FILES_DIR=%s\n", UI_atari_files_dir[i]);
	for (i = 0; i < UI_n_saved_files_dir; i++)
		fprintf(fp, "SAVED_FILES_DIR=%s\n", UI_saved_files_dir[i]);
	fprintf(fp, "SHOW_HIDDEN_FILES=%d\n", UI_show_hidden_files);
#endif
	for (i = 0; i < 4; i++)
		fprintf(fp, "H%c_DIR=%s\n", '1' + i, Devices_atari_h_dir[i]);
	fprintf(fp, "HD_READ_ONLY=%d\n", Devices_h_read_only);
	fprintf(fp, "HD_DEVICE_NAME=%c\n", Devices_h_device_name);

#ifdef HAVE_SYSTEM
	fprintf(fp, "PRINT_COMMAND=%s\n", Devices_print_command);
#endif

#ifndef BASIC
	fprintf(fp, "SCREEN_REFRESH_RATIO=%d\n", Atari800_refresh_rate);
	fprintf(fp, "ACCURATE_SKIPPED_FRAMES=%d\n", Atari800_collisions_in_skipped_frames);
#endif

	fprintf(fp, "MACHINE_TYPE=Atari %s\n", machine_type_string[Atari800_machine_type]);

	fprintf(fp, "RAM_SIZE=");
	switch (MEMORY_ram_size) {
	case MEMORY_RAM_320_RAMBO:
		fprintf(fp, "320 (RAMBO)\n");
		break;
	case MEMORY_RAM_320_COMPY_SHOP:
		fprintf(fp, "320 (COMPY SHOP)\n");
		break;
	default:
		fprintf(fp, "%d\n", MEMORY_ram_size);
		break;
	}

	fprintf(fp, (Atari800_tv_mode == Atari800_TV_PAL) ? "DEFAULT_TV_MODE=PAL\n" : "DEFAULT_TV_MODE=NTSC\n");
	fprintf(fp, "MOSAIC_RAM_NUM_BANKS=%d\n", MEMORY_mosaic_num_banks);
	fprintf(fp, "AXLON_RAM_NUM_BANKS=%d\n", MEMORY_axlon_num_banks);
	fprintf(fp, "ENABLE_MAPRAM=%d\n", MEMORY_enable_mapram);

	fprintf(fp, "DISABLE_BASIC=%d\n", Atari800_disable_basic);
	fprintf(fp, "TURBO_SPEED=%d\n", Atari800_turbo_speed);
	fprintf(fp, "ENABLE_SIO_PATCH=%d\n", ESC_enable_sio_patch);
	fprintf(fp, "ENABLE_SLOW_XEX_LOADING=%d\n", BINLOAD_slow_xex_loading);
	fprintf(fp, "ENABLE_H_PATCH=%d\n", Devices_enable_h_patch);
	fprintf(fp, "ENABLE_P_PATCH=%d\n", Devices_enable_p_patch);
#ifdef R_IO_DEVICE
	fprintf(fp, "ENABLE_R_PATCH=%d\n", Devices_enable_r_patch);
#endif

#ifdef SOUND
	fprintf(fp, "ENABLE_NEW_POKEY=%d\n", POKEYSND_enable_new_pokey);
#ifdef STEREO_SOUND
	fprintf(fp, "STEREO_POKEY=%d\n", POKEYSND_stereo_enabled);
#endif
#ifdef CONSOLE_SOUND
	fprintf(fp, "SPEAKER_SOUND=%d\n", POKEYSND_console_sound_enabled);
#endif
#endif /* SOUND */
	fprintf(fp, "BUILTIN_BASIC=%d\n", Atari800_builtin_basic);
	fprintf(fp, "KEYBOARD_LEDS=%d\n", Atari800_keyboard_leds);
	fprintf(fp, "F_KEYS=%d\n", Atari800_f_keys);
	fprintf(fp, "BUILTIN_GAME=%d\n", Atari800_builtin_game);
	fprintf(fp, "KEYBOARD_DETACHED=%d\n", Atari800_keyboard_detached);
	fprintf(fp, "1200XL_JUMPER=%d\n", Atari800_jumper);
	fprintf(fp, "CFG_SAVE_ON_EXIT=%d\n", CFG_save_on_exit);
	/* Add module-specific configurations here */
	PBI_WriteConfig(fp);
	CARTRIDGE_WriteConfig(fp);
	CASSETTE_WriteConfig(fp);
	RTIME_WriteConfig(fp);
#ifdef XEP80_EMULATION
	XEP80_WriteConfig(fp);
#endif
#ifdef AF80
	AF80_WriteConfig(fp);
#endif
#ifdef BIT3
	BIT3_WriteConfig(fp);
#endif
#if !defined(BASIC) && !defined(CURSES_BASIC)
	Colours_WriteConfig(fp);
	ARTIFACT_WriteConfig(fp);
	Screen_WriteConfig(fp);
#endif
#ifdef NTSC_FILTER
	FILTER_NTSC_WriteConfig(fp);
#endif
#if SUPPORTS_CHANGE_VIDEOMODE
	VIDEOMODE_WriteConfig(fp);
#endif
#ifdef SOUND
	Sound_WriteConfig(fp);
#endif /* SOUND */
#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ) || defined(AUDIO_RECORDING) || defined(VIDEO_RECORDING)
	File_Export_WriteConfig(fp);
#endif
#ifdef SUPPORTS_PLATFORM_CONFIGSAVE
	PLATFORM_ConfigSave(fp);
#endif
	fclose(fp);
	return TRUE;
}

int CFG_MatchTextParameter(char const *param, char const * const cfg_strings[], int cfg_strings_size)
{
	int i;
	for (i = 0; i < cfg_strings_size; i ++) {
		if (Util_stricmp(param, cfg_strings[i]) == 0)
			return i;
	}
	/* Unrecognised value */
	return -1;
}

/*
vim:ts=4:sw=4:
*/

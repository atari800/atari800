/*
 * rt-config.c - Routines to support the atari800.cfg file
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
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "prompts.h"
#include "rt-config.h"
#include "log.h"

#ifdef SUPPORTS_ATARI_CONFIGURE
/* This procedure processes lines not recognized by RtConfigLoad. */
int Atari_Configure(char* option, char *parameters);
#endif

#ifdef SUPPORTS_ATARI_CONFIGSAVE
/* This function saves additional config lines */
void Atari_ConfigSave(FILE *fp);
#endif

#ifdef SUPPORTS_ATARI_CONFIGINIT
/* This function set the configuration parameters to default values */
void Atari_ConfigInit(void);
#endif

char *safe_strncpy(char *dest, const char *src, size_t size);

char atari_osa_filename[FILENAME_MAX];
char atari_osb_filename[FILENAME_MAX];
char atari_xlxe_filename[FILENAME_MAX];
char atari_basic_filename[FILENAME_MAX];
char atari_5200_filename[FILENAME_MAX];
char atari_disk_dirs[MAX_DIRECTORIES][FILENAME_MAX];
char atari_rom_dir[FILENAME_MAX];
char atari_h1_dir[FILENAME_MAX];
char atari_h2_dir[FILENAME_MAX];
char atari_h3_dir[FILENAME_MAX];
char atari_h4_dir[FILENAME_MAX];
char atari_exe_dir[FILENAME_MAX];
char atari_state_dir[FILENAME_MAX];
char print_command[256];
int hd_read_only;
int refresh_rate;
int disable_basic;
int enable_sio_patch;
int enable_h_patch;
int enable_p_patch;
int enable_r_patch;
int disk_directories;
int enable_new_pokey;
int stereo_enabled;

/* If another default path config path is defined use it
   otherwise use the default one */
#ifndef DEFAULT_CFG_NAME
#define DEFAULT_CFG_NAME	".atari800.cfg"
#endif

#ifndef SYSTEM_WIDE_CFG_FILE
#define SYSTEM_WIDE_CFG_FILE	"/etc/atari800.cfg"
#endif

static char rtconfig_filename[FILENAME_MAX];
#define RTCFGNAME_MAX	(sizeof(rtconfig_filename)-1)

#ifdef BACK_SLASH
#define PATH_SEPARATOR		'\\'
#define PATH_SEPARATOR_STR	"\\"
#else
#define PATH_SEPARATOR	'/'
#define PATH_SEPARATOR_STR	"/"
#endif

/*
 * Set Configuration Parameters to sensible values
 * in case the configuration file is missing.
 */
static void RtPresetDefaults()
{
	atari_osa_filename[0] = '\0';
	atari_osb_filename[0] = '\0';
	atari_xlxe_filename[0] = '\0';
	atari_basic_filename[0] = '\0';
	atari_5200_filename[0] = '\0';
	atari_disk_dirs[0][0] = '\0';
	disk_directories = 0;
	atari_rom_dir[0] = '\0';
	atari_h1_dir[0] = '\0';
	atari_h2_dir[0] = '\0';
	atari_h3_dir[0] = '\0';
	atari_h4_dir[0] = '\0';
	atari_exe_dir[0] = '\0';
	atari_state_dir[0] = '\0';

#ifdef SUPPORTS_ATARI_CONFIGINIT
	Atari_ConfigInit();
#endif

	strcpy(print_command, "lpr %s");
	hd_read_only = 1;
	refresh_rate = 1;
	machine_type = MACHINE_XLXE;
	ram_size = 64;
	tv_mode = TV_PAL;
	disable_basic = 1;
	enable_sio_patch = 1;
	enable_h_patch = 1;
	enable_p_patch = 1;
	enable_r_patch = 0;
	enable_new_pokey = 1;
	stereo_enabled = 0;
}

static int is_print_command_safe(const char *command)
{
	const char *p = command;
	int was_percent_s = FALSE;
	while (*p != '\0') {
		if (*p++ == '%') {
			char c = *p++;
			if (c == '%')
				continue; /* %% is safe */
			if (c == 's' && !was_percent_s) {
				was_percent_s = TRUE; /* only one %s is safe */
				continue;
			}
			return FALSE;
		}
	}
	return TRUE;
}

int RtConfigLoad(const char *alternate_config_filename)
{
	FILE *fp;
	const char *fname = rtconfig_filename;
	int status = TRUE;
	char string[256];
	char *ptr;

	RtPresetDefaults();

	*rtconfig_filename = '\0';

	/* if alternate config filename is passed then use it */
	if (alternate_config_filename != NULL && *alternate_config_filename > 0) {
		strncpy(rtconfig_filename, alternate_config_filename, RTCFGNAME_MAX);
		rtconfig_filename[RTCFGNAME_MAX] = '\0';
	}
	/* else use the default config name under the HOME folder */
	else {
		char *home = getenv("HOME");
		if (home != NULL) {
			int len;
			strncpy(rtconfig_filename, home, RTCFGNAME_MAX);
			rtconfig_filename[RTCFGNAME_MAX] = '\0';
			len = strlen(rtconfig_filename);
			if (len > 0) {
				if (rtconfig_filename[len-1] != PATH_SEPARATOR) {
					strcat(rtconfig_filename, PATH_SEPARATOR_STR);
				}
			}
		}
		strncat(rtconfig_filename, DEFAULT_CFG_NAME, RTCFGNAME_MAX-strlen(rtconfig_filename));
		rtconfig_filename[RTCFGNAME_MAX] = '\0';
	}

	fp = fopen(fname, "r");
	if (!fp) {
		Aprint("User config file '%s' not found.", rtconfig_filename);

#ifdef SYSTEM_WIDE_CFG_FILE
		/* try system wide config file */
		fname = SYSTEM_WIDE_CFG_FILE;
		Aprint("Trying system wide config file: %s", fname);
		fp = fopen(fname, "r");
#endif
	}
	if (!fp) {
		Aprint("No configuration file found, will create fresh one from scratch:");
		return FALSE;
	}

	fgets(string, sizeof(string), fp);

	Aprint("Using Atari800 config file: %s\nCreated by %s", fname, string);

	while (fgets(string, sizeof(string), fp)) {
		RemoveLF(string);
		ptr = strchr(string, '=');
		if (ptr) {
			*ptr++ = '\0';
			RemoveSpaces(string);
			RemoveSpaces(ptr);

			if (strcmp(string, "OS/A_ROM") == 0)
				safe_strncpy(atari_osa_filename, ptr, sizeof(atari_osa_filename));
			else if (strcmp(string, "OS/B_ROM") == 0)
				safe_strncpy(atari_osb_filename, ptr, sizeof(atari_osb_filename));
			else if (strcmp(string, "XL/XE_ROM") == 0)
				safe_strncpy(atari_xlxe_filename, ptr, sizeof(atari_xlxe_filename));
			else if (strcmp(string, "BASIC_ROM") == 0)
				safe_strncpy(atari_basic_filename, ptr, sizeof(atari_basic_filename));
			else if (strcmp(string, "5200_ROM") == 0)
				safe_strncpy(atari_5200_filename, ptr, sizeof(atari_5200_filename));
			else if (strcmp(string, "DISK_DIR") == 0) {
				if (disk_directories == MAX_DIRECTORIES)
					Aprint("All disk directory slots used!");
				else
					safe_strncpy(atari_disk_dirs[disk_directories++], ptr, sizeof(atari_disk_dirs[0]));
			}
			else if (strcmp(string, "ROM_DIR") == 0)
				safe_strncpy(atari_rom_dir, ptr, sizeof(atari_rom_dir));
			else if (strcmp(string, "H1_DIR") == 0)
				safe_strncpy(atari_h1_dir, ptr, sizeof(atari_h1_dir));
			else if (strcmp(string, "H2_DIR") == 0)
				safe_strncpy(atari_h2_dir, ptr, sizeof(atari_h2_dir));
			else if (strcmp(string, "H3_DIR") == 0)
				safe_strncpy(atari_h3_dir, ptr, sizeof(atari_h3_dir));
			else if (strcmp(string, "H4_DIR") == 0)
				safe_strncpy(atari_h4_dir, ptr, sizeof(atari_h4_dir));
			else if (strcmp(string, "HD_READ_ONLY") == 0)
				sscanf(ptr, "%d", &hd_read_only);
			else if (strcmp(string, "EXE_DIR") == 0)
				safe_strncpy(atari_exe_dir, ptr, sizeof(atari_exe_dir));
			else if (strcmp(string, "STATE_DIR") == 0)
				safe_strncpy(atari_state_dir, ptr, sizeof(atari_state_dir));
			else if (strcmp(string, "PRINT_COMMAND") == 0) {
				if (is_print_command_safe(ptr))
					safe_strncpy(print_command, ptr, sizeof(print_command));
				else
					Aprint("Unsafe PRINT_COMMAND ignored");
			}
			else if (strcmp(string, "SCREEN_REFRESH_RATIO") == 0)
				sscanf(ptr, "%d", &refresh_rate);
			else if (strcmp(string, "DISABLE_BASIC") == 0)
				sscanf(ptr, "%d", &disable_basic);
			else if (strcmp(string, "ENABLE_SIO_PATCH") == 0) {
				sscanf(ptr, "%d", &enable_sio_patch);
			}
			else if (strcmp(string, "ENABLE_H_PATCH") == 0) {
				sscanf(ptr, "%d", &enable_h_patch);
			}
			else if (strcmp(string, "ENABLE_P_PATCH") == 0) {
				sscanf(ptr, "%d", &enable_p_patch);
			}
			else if (strcmp(string, "ENABLE_R_PATCH") == 0) {
				sscanf(ptr, "%d", &enable_r_patch);
			}
			else if (strcmp(string, "ENABLE_NEW_POKEY") == 0) {
				sscanf(ptr, "%d", &enable_new_pokey);
			}
			else if (strcmp(string, "STEREO_POKEY") == 0) {
				sscanf(ptr, "%d", &stereo_enabled);
			}
			else if (strcmp(string, "SPEAKER_SOUND") == 0) {
#ifdef CONSOLE_SOUND
				sscanf(ptr, "%d", &console_sound_enabled);
#endif
			}
			else if (strcmp(string, "SERIO_SOUND") == 0) {
#ifdef SERIO_SOUND
				sscanf(ptr, "%d", &serio_sound_enabled);
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
				Aprint("Unrecognized Variable: %s", string);
#endif
			}
		}
		else {
			Aprint("Ignored Config Line: %s", string);
		}
	}

	fclose(fp);
	return status;
}

void RtConfigSave(void)
{
	FILE *fp;
	int i;

	fp = fopen(rtconfig_filename, "w");
	if (!fp) {
		perror(rtconfig_filename);
		Aprint("Cannot write to config file: %s", rtconfig_filename);
		Aflushlog();
		exit(1);
	}
	Aprint("Writing config file: %s", rtconfig_filename);

	fprintf(fp, "%s\n", ATARI_TITLE);
	fprintf(fp, "OS/A_ROM=%s\n", atari_osa_filename);
	fprintf(fp, "OS/B_ROM=%s\n", atari_osb_filename);
	fprintf(fp, "XL/XE_ROM=%s\n", atari_xlxe_filename);
	fprintf(fp, "BASIC_ROM=%s\n", atari_basic_filename);
	fprintf(fp, "5200_ROM=%s\n", atari_5200_filename);
	for (i = 0; i < disk_directories; i++)
		fprintf(fp, "DISK_DIR=%s\n", atari_disk_dirs[i]);
	fprintf(fp, "ROM_DIR=%s\n", atari_rom_dir);
	fprintf(fp, "H1_DIR=%s\n", atari_h1_dir);
	fprintf(fp, "H2_DIR=%s\n", atari_h2_dir);
	fprintf(fp, "H3_DIR=%s\n", atari_h3_dir);
	fprintf(fp, "H4_DIR=%s\n", atari_h4_dir);
	fprintf(fp, "HD_READ_ONLY=%d\n", hd_read_only);
	fprintf(fp, "EXE_DIR=%s\n", atari_exe_dir);
#ifndef BASIC
	fprintf(fp, "STATE_DIR=%s\n", atari_state_dir);
#endif
#ifdef HAVE_SYSTEM
	fprintf(fp, "PRINT_COMMAND=%s\n", print_command);
#endif
#ifndef BASIC
	fprintf(fp, "SCREEN_REFRESH_RATIO=%d\n", refresh_rate);
#endif

	fprintf(fp, "MACHINE_TYPE=Atari ");
	switch (machine_type) {
	case MACHINE_OSA:
		fprintf(fp, "OS/A\n");
		break;
	case MACHINE_OSB:
		fprintf(fp, "OS/B\n");
		break;
	case MACHINE_XLXE:
		fprintf(fp, "XL/XE\n");
		break;
	case MACHINE_5200:
		fprintf(fp, "5200\n");
		break;
	}

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

	if (tv_mode == TV_PAL)
		fprintf(fp, "DEFAULT_TV_MODE=PAL\n");
	else
		fprintf(fp, "DEFAULT_TV_MODE=NTSC\n");

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
#endif

#ifdef SUPPORTS_ATARI_CONFIGSAVE
	Atari_ConfigSave(fp);
#endif

	fclose(fp);
}

#ifndef DONT_USE_RTCONFIGUPDATE

void RtConfigUpdate(void)
{
	strcpy(atari_osa_filename, "atariosa.rom");
	strcpy(atari_osb_filename, "atariosb.rom");
	strcpy(atari_xlxe_filename, "atarixl.rom");
	strcpy(atari_basic_filename, "ataribas.rom");
	strcpy(atari_disk_dirs[0], ".");
	disk_directories = 1;
	strcpy(atari_rom_dir, ".");

	GetString("Enter path with filename of Atari OS/A ROM [%s] ", atari_osa_filename);
	GetString("Enter path with filename of Atari OS/B ROM [%s] ", atari_osb_filename);
	GetString("Enter path with filename of Atari XL/XE ROM [%s] ", atari_xlxe_filename);
	GetString("Enter path with filename of Atari BASIC ROM [%s] ", atari_basic_filename);
	GetString("Enter path with filename of Atari 5200 ROM [%s] ", atari_5200_filename);
	GetString("Enter path for disk images [%s] ", atari_disk_dirs[0]);
	GetString("Enter path for ROM images [%s] ", atari_rom_dir);
	GetString("Enter path for H1: device [%s] ", atari_h1_dir);
	GetString("Enter path for H2: device [%s] ", atari_h2_dir);
	GetString("Enter path for H3: device [%s] ", atari_h3_dir);
	GetString("Enter path for H4: device [%s] ", atari_h4_dir);
	GetYesNoAsInt("H: devices are read only [%c] ", &hd_read_only);
	GetString("Enter path for single exe files [%s] ", atari_exe_dir);
#ifndef BASIC
	GetString("Enter path for state files [%s] ", atari_state_dir);
#endif
#ifdef HAVE_SYSTEM
	for (;;) {
		char command[256];
		strcpy(command, print_command);
		GetString("Enter print command [%s] ", command);
		if (is_print_command_safe(command)) {
			strcpy(print_command, command);
			break;
		}
		printf("The command you entered is not safe. You may use at most one %%s.\n");
 	}
#endif
#ifndef BASIC
	GetNumber("Enter default screen refresh ratio 1:[%d] ", &refresh_rate);
	if (refresh_rate < 1)
		refresh_rate = 1;
	else if (refresh_rate > 60)
		refresh_rate = 60;
#endif

	do {
		GetNumber("Machine type: 0=OS/A 1=OS/B 2=XL/XE 3=5200 [%d] ",
				  &machine_type);
	} while ((machine_type < 0) || (machine_type > 3));

	switch (machine_type) {
	case MACHINE_OSA:
	case MACHINE_OSB:
		{
			int enable_c000_ram = (ram_size == 52);
			GetYesNoAsInt("Enable C000-CFFF RAM in Atari800 mode [%c] ",
						  &enable_c000_ram);
			ram_size = enable_c000_ram ? 52 : 48;
		}
		break;
	case MACHINE_XLXE:
		{
			int k = ram_size;
			if (k == RAM_320_RAMBO || k == RAM_320_COMPY_SHOP)
				k = 320;
			do {
				GetNumber("Ram size (kilobytes): 16,64,128,320,576,1088 [%d] ",
						  &k);
			} while (k != 16 && k != 64 && k != 128 && k != 320 && k != 576 && k != 1088);
			if (k == 320) {
				if (ram_size == RAM_320_RAMBO)
					k = 1;
				else
					k = 2;
				do {
					GetNumber("320XE memory bank control 1=RAMBO, 2=COMPY SHOP [%d] ",
						&k);
				} while ((k < 1) || (k > 2));
				ram_size = k == 1 ? RAM_320_RAMBO : RAM_320_COMPY_SHOP;
			}
		}
		break;
	case MACHINE_5200:
		ram_size = 16;
		break;
	}

	{
		int default_tv_mode = tv_mode == TV_NTSC ? 2 : 1;
		do {
			GetNumber("Default TV mode 1=PAL 2=NTSC [%d] ",
					  &default_tv_mode);
		} while ((default_tv_mode < 1) || (default_tv_mode > 2));
		tv_mode = default_tv_mode == 1 ? TV_PAL : TV_NTSC;
	}

	GetYesNoAsInt("Disable BASIC when booting Atari [%c] ", &disable_basic);
	GetYesNoAsInt("Enable SIO patch (Recommended for speed) [%c] ",
			  	&enable_sio_patch);
	GetYesNoAsInt("Enable H: (Hard disk) patch [%c] ", &enable_h_patch);
	GetYesNoAsInt("Enable P: (Printer) patch [%c] ", &enable_p_patch);
#ifdef R_IO_DEVICE
	GetYesNoAsInt("Enable R: (Atari850 via net) patch [%c] ", &enable_r_patch);
#endif
#ifdef SOUND
	GetYesNoAsInt("Enable new HiFi POKEY [%c] ", &enable_new_pokey);
#ifdef STEREO_SOUND
	GetYesNoAsInt("Enable STEREO POKEY Sound [%c] ", &stereo_enabled);
#endif
#ifdef CONSOLE_SOUND
	GetYesNoAsInt("Enable SPEAKER Sound [%c] ", &console_sound_enabled);
#endif
#ifdef SERIO_SOUND
	GetYesNoAsInt("Enable SERIO Sound [%c] ", &serio_sound_enabled);
#endif
#endif
}

#endif /* DONT_USE_RTCONFIGUPDATE */

/*
$Log$
Revision 1.25  2005/08/17 22:41:48  pfusik
is_print_command_safe()

Revision 1.24  2005/08/16 23:07:28  pfusik
#include "config.h" before system headers

Revision 1.23  2005/08/14 08:43:19  pfusik
support 16, 576, 1088 RAM; skip non-working options

Revision 1.22  2005/08/13 08:49:22  pfusik
no pokeysnd if SOUND disabled; no R: if not compiled in

Revision 1.21  2004/11/26 18:10:29  joy
buffer overflow fixes

Revision 1.20  2004/06/06 12:19:19  joy
RemoveSpaces() used on the variable and its value
Aprint() fixes

Revision 1.19  2004/06/04 22:15:30  joy
SUPPORTS_ATARI_CONFIG* is enough, other platforms define it if needed

Revision 1.18  2003/12/12 00:24:16  markgrebe
Added enable for console and sio sound

Revision 1.17  2003/08/31 21:59:13  joy
R: not enabled by default

Revision 1.16  2003/05/28 19:54:58  joy
R: device support (networking?)

Revision 1.15  2003/02/24 09:33:10  joy
header cleanup

Revision 1.14  2003/02/10 12:41:11  joy
simple fix

Revision 1.13  2003/02/09 21:19:59  joy
reworked searching for config file

Revision 1.12  2002/04/07 19:35:40  joy
remove non ANSI t parameter in fopen

Revision 1.11  2001/10/03 16:32:07  fox
fixed default TV mode in RtConfigUpdate

Revision 1.10  2001/09/17 18:16:03  fox
enable_c000_ram -> ram_size = 52

Revision 1.9  2001/09/17 18:13:05  fox
machine, mach_xlxe, Ram256, os, default_system -> machine_type, ram_size

Revision 1.8  2001/09/16 11:24:26  fox
removed default_tv_mode

Revision 1.7  2001/09/09 08:33:17  fox
hold_option -> disable_basic

Revision 1.6  2001/09/08 07:52:30  knik
used FILENAME_MAX instead of MAX_FILENAME_LEN

Revision 1.5  2001/07/20 20:03:15  fox
removed #define TRUE/FALSE and extern int Ram256 (it's in atari.h)

Revision 1.3  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/

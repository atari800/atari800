/* $Id$ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "prompts.h"
#include "rt-config.h"

#if defined(VGA) || defined(SUPPORTS_ATARI_CONFIGURE)
	/* This procedure processes lines not recognized by RtConfigLoad. */
	extern int Atari_Configure(char* option,char *parameters);
#endif

#ifdef SUPPORTS_ATARI_CONFIGSAVE
	/* This function saves additional config lines */
	extern void Atari_ConfigSave( FILE *fp);
#endif

#ifdef SUPPORTS_ATARI_CONFIGINIT
	/* This function set the configuration parameters to
     default values */
	extern void Atari_ConfigInit(void);
#endif

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
int disk_directories;

/* If another default path config path is defined use it
   otherwise use the default one */
#ifdef DEFAULT_CFG_PATH
static char *rtconfig_filename1 = DEFAULT_CFG_PATH;
#else
static char *rtconfig_filename1 = "atari800.cfg";
#endif

static char *rtconfig_filename2 = "/etc/atari800.cfg";

int RtConfigLoad(char *rtconfig_filename)
{
	FILE *fp;
	int status = TRUE;

	/*
	 * Set Configuration Parameters to sensible values
	 * in case the configuration file is missing.
	 */

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

	if (rtconfig_filename) {
		fp = fopen(rtconfig_filename, "rt");
		if (!fp) {
			perror(rtconfig_filename);
			exit(1);
		}
	}
	else {
		fp = fopen(rtconfig_filename1, "rt");
		if (!fp)
			fp = fopen(rtconfig_filename2, "rt");
	}

	if (fp) {
		char string[256];
		char *ptr;

		fgets(string, sizeof(string), fp);

		fprintf(stderr, "Configuration Created by %s", string);

		while (fgets(string, sizeof(string), fp)) {
			RemoveLF(string);
			ptr = strchr(string, '=');
			if (ptr) {
				*ptr++ = '\0';

				if (strcmp(string, "OS/A_ROM") == 0)
					strcpy(atari_osa_filename, ptr);
				else if (strcmp(string, "OS/B_ROM") == 0)
					strcpy(atari_osb_filename, ptr);
				else if (strcmp(string, "XL/XE_ROM") == 0)
					strcpy(atari_xlxe_filename, ptr);
				else if (strcmp(string, "BASIC_ROM") == 0)
					strcpy(atari_basic_filename, ptr);
				else if (strcmp(string, "5200_ROM") == 0)
					strcpy(atari_5200_filename, ptr);
				else if (strcmp(string, "DISK_DIR") == 0) {
					if (disk_directories == MAX_DIRECTORIES)
						printf("All disk directory slots used!\n");
					else
						strcpy(atari_disk_dirs[disk_directories++], ptr);
				}
				else if (strcmp(string, "ROM_DIR") == 0)
					strcpy(atari_rom_dir, ptr);
				else if (strcmp(string, "H1_DIR") == 0)
					strcpy(atari_h1_dir, ptr);
				else if (strcmp(string, "H2_DIR") == 0)
					strcpy(atari_h2_dir, ptr);
				else if (strcmp(string, "H3_DIR") == 0)
					strcpy(atari_h3_dir, ptr);
				else if (strcmp(string, "H4_DIR") == 0)
					strcpy(atari_h4_dir, ptr);
				else if (strcmp(string, "HD_READ_ONLY") == 0)
					sscanf(ptr, "%d", &hd_read_only);
				else if (strcmp(string, "EXE_DIR") == 0)
					strcpy(atari_exe_dir, ptr);
				else if (strcmp(string, "STATE_DIR") == 0)
					strcpy(atari_state_dir, ptr);
				else if (strcmp(string, "PRINT_COMMAND") == 0)
					strcpy(print_command, ptr);
				else if (strcmp(string, "SCREEN_REFRESH_RATIO") == 0)
					sscanf(ptr, "%d", &refresh_rate);
				/* HOLD_OPTION supported for compatibility with previous Atari800 versions */
				else if (strcmp(string, "DISABLE_BASIC") == 0 || strcmp(string, "HOLD_OPTION") == 0)
					sscanf(ptr, "%d", &disable_basic);
				/* Supported for compatibility with previous Atari800 versions */
				else if (strcmp(string, "ENABLE_C000_RAM") == 0) {
					int enable_c000_ram = 0;
					sscanf(ptr, "%d", &enable_c000_ram);
					if (enable_c000_ram && ram_size == 48)
						ram_size = 52;
				}
				else if (strcmp(string, "ENABLE_ROM_PATCH") == 0) {
					/* Supported for compatibility with previous Atari800 versions */
					int enable_rom_patches;
					sscanf(ptr, "%d", &enable_rom_patches);
					enable_h_patch = enable_p_patch = enable_rom_patches;
				}
				else if (strcmp(string, "ENABLE_SIO_PATCH") == 0) {
					sscanf(ptr, "%d", &enable_sio_patch);
				}
				else if (strcmp(string, "ENABLE_H_PATCH") == 0) {
					sscanf(ptr, "%d", &enable_h_patch);
				}
				else if (strcmp(string, "ENABLE_P_PATCH") == 0) {
					sscanf(ptr, "%d", &enable_p_patch);
				}
				/* Supported for compatibility with previous Atari800 versions */
				else if (strcmp(string, "DEFAULT_SYSTEM") == 0) {
					if (strcmp(ptr, "Atari OS/A") == 0) {
						machine_type = MACHINE_OSA;
						ram_size = 48;
					}
					else if (strcmp(ptr, "Atari OS/B") == 0) {
						machine_type = MACHINE_OSB;
						ram_size = 48;
					}
					else if (strcmp(ptr, "Atari XL") == 0) {
						machine_type = MACHINE_XLXE;
						ram_size = 64;
					}
					else if (strcmp(ptr, "Atari XE") == 0) {
						machine_type = MACHINE_XLXE;
						ram_size = 128;
					}
					else if (strcmp(ptr, "Atari 320XE (RAMBO)") == 0) {
						machine_type = MACHINE_XLXE;
						ram_size = RAM_320_RAMBO;
					}
					else if (strcmp(ptr, "Atari 320XE (COMPY SHOP)") == 0) {
						machine_type = MACHINE_XLXE;
						ram_size = RAM_320_COMPY_SHOP;
					}
					else if (strcmp(ptr, "Atari 5200") == 0) {
						machine_type = MACHINE_5200;
						ram_size = 16;
					}
					else
						printf("Invalid System: %s\n", ptr);
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
						printf("Invalid machine type: %s\n", ptr);
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
					else
						printf("Invalid ram size: %s\n", ptr);
				}
				else if (strcmp(string, "DEFAULT_TV_MODE") == 0) {
					if (strcmp(ptr, "PAL") == 0)
						tv_mode = TV_PAL;
					else if (strcmp(ptr, "NTSC") == 0)
						tv_mode = TV_NTSC;
					else
						printf("Invalid TV Mode: %s\n", ptr);
				}
				else
#if defined(VGA) || defined(SUPPORTS_ATARI_CONFIGURE)
                                if (!Atari_Configure(string,ptr))
					printf("Unrecognized variable or bad parameters: %s=%s\n", string,ptr);
#else
					printf("Unrecognized Variable: %s\n", string);
#endif
			}
			else {
				printf("Ignored Config Line: %s\n", string);
			}
		}


		fclose(fp);
	}
	else {
		status = FALSE;
	}
	return status;
}

void RtConfigSave(void)
{
	FILE *fp;
	int i;

	fp = fopen(rtconfig_filename1, "wt");
	if (!fp) {
		perror(rtconfig_filename1);
		exit(1);
	}
	printf("\nWriting: %s\n\n", rtconfig_filename1);

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
	fprintf(fp, "STATE_DIR=%s\n", atari_state_dir);
	fprintf(fp, "PRINT_COMMAND=%s\n", print_command);
	fprintf(fp, "SCREEN_REFRESH_RATIO=%d\n", refresh_rate);

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
	do {
		GetNumber("H: devices are read only [%d] ",
				  &hd_read_only);
	} while ((hd_read_only < 0) || (hd_read_only > 1));
	GetString("Enter path for single exe files [%s] ", atari_exe_dir);
	GetString("Enter path for state files [%s] ", atari_state_dir);
	GetString("Enter print command [%s] ", print_command);
	GetNumber("Enter default screen refresh ratio 1:[%d] ", &refresh_rate);
	if (refresh_rate < 1)
		refresh_rate = 1;
	else if (refresh_rate > 60)
		refresh_rate = 60;

	do {
		GetNumber("Machine type: 0=OS/A 1=OS/B 2=XL/XE 3=5200 [%d] ",
				  &machine_type);
	} while ((machine_type < 0) || (machine_type > 3));

	switch (machine_type) {
	case MACHINE_OSA:
	case MACHINE_OSB:
		{
			int enable_c000_ram = ram_size == 52;
			do {
				GetNumber("Enable C000-CFFF RAM in Atari800 mode [%d] ",
						  &enable_c000_ram);
			} while ((enable_c000_ram < 0) || (enable_c000_ram > 1));
			ram_size = enable_c000_ram ? 52 : 48;
		}
		break;
	case MACHINE_XLXE:
		{
			int k = ram_size;
			if (k == RAM_320_RAMBO || k == RAM_320_COMPY_SHOP)
				k = 320;
			do {
				GetNumber("Ram size (kilobytes): 64,128,320 [%d] ",
						  &k);
			} while ((k != 64) && (k != 128) && (k != 320));
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

	do {
		GetNumber("Disable BASIC when booting Atari [%d] ",
				  &disable_basic);
	} while ((disable_basic < 0) || (disable_basic > 1));

	do {
		GetNumber("Enable SIO patch (Recommended for speed) [%d] ",
			  	&enable_sio_patch);
	} while ((enable_sio_patch < 0) || (enable_sio_patch > 1));

	do {
		GetNumber("Enable H: (Hard disk) patch [%d] ",
				  &enable_h_patch);
	} while ((enable_h_patch < 0) || (enable_h_patch > 1));

	do {
		GetNumber("Enable P: (Printer) patch [%d] ",
				  &enable_p_patch);
	} while ((enable_p_patch < 0) || (enable_p_patch > 1));

#ifdef VGA
	printf("Standard joysticks configuration selected.\n"
			"Use joycfg.exe to change it. Press RETURN to continue ");
	fflush(stdout);
	getchar();
#endif
}

#endif /* DONT_USE_RTCONFIGUPDATE */

/*
$Log$
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

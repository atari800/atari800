#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FALSE   0
#define TRUE    1

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

char atari_osa_filename[MAX_FILENAME_LEN];
char atari_osb_filename[MAX_FILENAME_LEN];
char atari_xlxe_filename[MAX_FILENAME_LEN];
char atari_basic_filename[MAX_FILENAME_LEN];
char atari_5200_filename[MAX_FILENAME_LEN];
char atari_disk_dirs[MAX_DIRECTORIES][MAX_FILENAME_LEN];
char atari_rom_dir[MAX_FILENAME_LEN];
char atari_h1_dir[MAX_FILENAME_LEN];
char atari_h2_dir[MAX_FILENAME_LEN];
char atari_h3_dir[MAX_FILENAME_LEN];
char atari_h4_dir[MAX_FILENAME_LEN];
char atari_exe_dir[MAX_FILENAME_LEN];
char atari_state_dir[MAX_FILENAME_LEN];
char print_command[256];
int hd_read_only;
int refresh_rate;
int default_system;
int default_tv_mode;
int hold_option;
int enable_c000_ram;
int enable_rom_patches;
int enable_sio_patch;
int disk_directories;

extern int Ram256;

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

	/* Win32 version doesn't use any of the RTxxx functions, but needs the variable 
	   declarations above, so all of these are stubbed out by ifndefs in Win32	*/
#ifndef WIN32
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
	default_system = 3;
	default_tv_mode = 1;
	hold_option = 1;
	enable_c000_ram = 0;
	enable_rom_patches = 1;
	enable_sio_patch = 1;

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

		printf("Configuration Created by %s", string);

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
				else if (strcmp(string, "HOLD_OPTION") == 0)
					sscanf(ptr, "%d", &hold_option);
				else if (strcmp(string, "ENABLE_C000_RAM") == 0)
					sscanf(ptr, "%d", &enable_c000_ram);
				else if (strcmp(string, "ENABLE_ROM_PATCH") == 0)
					sscanf(ptr, "%d", &enable_rom_patches);
				else if (strcmp(string, "ENABLE_SIO_PATCH") == 0) {
					if (enable_rom_patches)
						sscanf(ptr, "%d", &enable_sio_patch);
					else
						enable_sio_patch = 0;
				}
				else if (strcmp(string, "DEFAULT_SYSTEM") == 0) {
					if (strcmp(ptr, "Atari OS/A") == 0)
						default_system = 1;
					else if (strcmp(ptr, "Atari OS/B") == 0)
						default_system = 2;
					else if (strcmp(ptr, "Atari XL") == 0)
						default_system = 3;
					else if (strcmp(ptr, "Atari XE") == 0)
						default_system = 4;
					else if (strcmp(ptr, "Atari 320XE (RAMBO)") == 0) {
						default_system = 5;
						Ram256 = 1;
					}
					else if (strcmp(ptr, "Atari 320XE (COMPY SHOP)") == 0) {
						default_system = 5;
						Ram256 = 2;
					}
					else if (strcmp(ptr, "Atari 5200") == 0)
						default_system = 6;
					else
						printf("Invalid System: %s\n", ptr);
				}
				else if (strcmp(string, "DEFAULT_TV_MODE") == 0) {
					if (strcmp(ptr, "PAL") == 0)
						default_tv_mode = 1;
					else if (strcmp(ptr, "NTSC") == 0)
						default_tv_mode = 2;
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
#endif /* Win32 */
	return status;
}

void RtConfigSave(void)
{
#ifndef WIN32
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

	fprintf(fp, "DEFAULT_SYSTEM=Atari ");
	switch (default_system) {
	case 1:
		fprintf(fp, "OS/A\n");
		break;
	case 2:
		fprintf(fp, "OS/B\n");
		break;
	case 3:
		fprintf(fp, "XL\n");
		break;
	case 4:
		fprintf(fp, "XE\n");
		break;
	case 5:
		if (Ram256 == 1)
			fprintf(fp, "320XE (RAMBO)\n");
		else if (Ram256 == 2)
			fprintf(fp, "320XE (COMPY SHOP)\n");
		break;
	case 6:
		fprintf(fp, "5200\n");
		break;
	}

	if (default_tv_mode == 1)
		fprintf(fp, "DEFAULT_TV_MODE=PAL\n");
	else
		fprintf(fp, "DEFAULT_TV_MODE=NTSC\n");

	fprintf(fp, "HOLD_OPTION=%d\n", hold_option);
	fprintf(fp, "ENABLE_C000_RAM=%d\n", enable_c000_ram);
	fprintf(fp, "ENABLE_ROM_PATCH=%d\n", enable_rom_patches);
	fprintf(fp, "ENABLE_SIO_PATCH=%d\n", enable_rom_patches ? enable_sio_patch : 0);

#ifdef SUPPORTS_ATARI_CONFIGSAVE
	Atari_ConfigSave(fp);
#endif

	fclose(fp);
#endif /* Win32 */
}

#ifndef DONT_USE_RTCONFIGUPDATE

void RtConfigUpdate(void)
{
#ifndef WIN32
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
		GetNumber("Default System 1=OS/A 2=OS/B 3=800XL 4=130XE 5=320XE 6=5200 [%d] ",
				  &default_system);
	} while ((default_system < 1) || (default_system > 6));

	if (default_system == 5) {
		do {
			if (!Ram256)
				Ram256 = 2;
			GetNumber("320XE memory bank control 1=RAMBO, 2=COMPY SHOP [%d] ",
					  &Ram256);
		} while ((Ram256 < 1) || (Ram256 > 2));
	}

	do {
		GetNumber("Default TV mode 1=PAL 2=NTSC [%d] ",
				  &default_tv_mode);
	} while ((default_tv_mode < 1) || (default_tv_mode > 2));

	do {
		GetNumber("Hold OPTION during Coldstart [%d] ",
				  &hold_option);
	} while ((hold_option < 0) || (hold_option > 1));

	do {
		GetNumber("Enable C000-CFFF RAM in Atari800 mode [%d] ",
				  &enable_c000_ram);
	} while ((enable_c000_ram < 0) || (enable_c000_ram > 1));

	do {
		GetNumber("Enable ROM PATCH (for H: device and SIO PATCH) [%d] ",
				  &enable_rom_patches);
	} while ((enable_rom_patches < 0) || (enable_rom_patches > 1));

	if (enable_rom_patches) {
		do {
			GetNumber("Enable SIO PATCH (Recommended for speed) [%d] ",
				  	&enable_sio_patch);
		} while ((enable_sio_patch < 0) || (enable_sio_patch > 1));
	}
	else
		enable_sio_patch = 0;	/* sio_patch only if rom_patches */

#ifdef VGA
	printf("Standard joysticks configuration selected.\n"
			"Use joycfg.exe to change it. Press RETURN to continue ");
	fflush(stdout);
	getchar();
#endif
#endif /* Win32 */
}

#endif /* DONT_USE_RTCONFIGUPDATE */

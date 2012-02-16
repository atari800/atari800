/*
 * sysrom.c - functions for searching and loading system ROM images
 *
 * Copyright (C) 2012 Tomasz Krasuski
 * Copyright (C) 2012 Atari800 development team (see DOC/CREDITS)
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

#include <ctype.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "sysrom.h"

#include "cfg.h"
#include "crc32.h"
#include "log.h"
#include "memory.h"
#include "util.h"

int SYSROM_os_versions[3] = { SYSROM_AUTO, SYSROM_AUTO, SYSROM_AUTO };
int SYSROM_basic_version = SYSROM_AUTO;

static char osa_ntsc_filename[FILENAME_MAX] = "";
static char osa_pal_filename[FILENAME_MAX] = "";
static char osb_ntsc_filename[FILENAME_MAX] = "";
static char osbb00r1_filename[FILENAME_MAX] = "";
static char osbb01r2_filename[FILENAME_MAX] = "";
static char osbb01r3_filename[FILENAME_MAX] = "";
static char osbb01r4_filename[FILENAME_MAX] = "";
static char os5200a_filename[FILENAME_MAX] = "";
static char os5200b_filename[FILENAME_MAX] = "";
static char basica_filename[FILENAME_MAX] = "";
static char basicb_filename[FILENAME_MAX] = "";
static char basicc_filename[FILENAME_MAX] = "";
static char os800_custom_filename[FILENAME_MAX] = "";
static char osxl_custom_filename[FILENAME_MAX] = "";
static char os5200_custom_filename[FILENAME_MAX] = "";
static char basic_custom_filename[FILENAME_MAX] = "";

/* Indicates an unknown checksum - an entry with this value will not be tested for checksum.
   Choosea value different than any know checksum in SYSROM_roms. */
enum { CRC_NULL = 0 };

SYSROM_t SYSROM_roms[SYSROM_SIZE] = {
	{ osa_ntsc_filename, 0x2800, 0xc1b3bb02, TRUE }, /* SYSROM_A_NTSC */
	{ osa_pal_filename, 0x2800, 0x72b3fed4, TRUE }, /* SYSROM_A_PAL */
	{ osb_ntsc_filename, 0x2800, 0x0e86d61d, TRUE }, /* SYSROM_B_NTSC */
	{ osbb00r1_filename, 0x4000, 0x643bcc98, TRUE }, /* SYSROM_BB00R1 */
	{ osbb01r2_filename, 0x4000, 0x1f9cd270, TRUE }, /* SYSROM_BB01R2 */
	{ osbb01r3_filename, 0x4000, 0x29f133f7, TRUE }, /* SYSROM_BB01R3 */
	{ osbb01r4_filename, 0x4000, 0x1eaf4002, TRUE }, /* SYSROM_BB01R4_OS */
	{ os5200a_filename, 0x0800, 0x4248d3e3, TRUE }, /* SYSROM_5200 */
	{ os5200b_filename, 0x0800, 0xc2ba2613, TRUE }, /* SYSROM_5200A */
	{ basica_filename, 0x2000, 0x4bec4de2, TRUE }, /* SYSROM_BASIC_A */
	{ basicb_filename, 0x2000, 0xf0202fb3, TRUE }, /* SYSROM_BASIC_B */
	{ basicc_filename, 0x2000, 0x7d684184, TRUE }, /* SYSROM_BASIC_C */
	{ os800_custom_filename, 0x2800, CRC_NULL, TRUE }, /* SYSROM_400800_CUSTOM */
	{ osxl_custom_filename, 0x4000, CRC_NULL, TRUE }, /* SYSROM_XL_CUSTOM */
	{ os5200_custom_filename, 0x0800, CRC_NULL, TRUE }, /* SYSROM_5200_CUSTOM */
	{ basic_custom_filename, 0x2000, CRC_NULL, TRUE } /* SYSROM_BASIC_CUSTOM */
};

/* Used in reading the config file to match option names. */
static char const * const cfg_strings[SYSROM_SIZE] = {
	"ROM_OS_A_NTSC",
	"ROM_OS_A_PAL",
	"ROM_OS_B_NTSC",
	"ROM_OS_BB00R1",
	"ROM_OS_BB01R2",
	"ROM_OS_BB01R3",
	"ROM_OS_BB01R4",
	"ROM_5200",
	"ROM_5200_A",
	"ROM_BASIC_A",
	"ROM_BASIC_B",
	"ROM_BASIC_C",
	"ROM_400/800_CUSTOM",
	"ROM_XL/XE_CUSTOM",
	"ROM_5200_CUSTOM",
	"ROM_BASIC_CUSTOM"
};

/* Used to recognise values of the "OS/BASIC revision" options. */
static char const * const cfg_strings_rev[SYSROM_SIZE+1] = {
	"A-NTSC",
	"A-PAL",
	"B-NTSC",
	"1",
	"2",
	"3",
	"4",
	"ORIG",
	"A",
	"A",
	"B",
	"C",
	"CUSTOM",
	"CUSTOM",
	"CUSTOM",
	"CUSTOM",
	"AUTO"
};

/* Number of ROM paths not set during initialisation. */
static int num_unset_roms = SYSROM_SIZE;

/* Checks if LEN is a correct ROM length. */
static int IsLengthAllowed(int len)
{
	return len == 0x2800 || len == 0x4000 || len == 0x0800 || len == 0x2000;
}

/* Clears the unset flag for a given ROM ID. */
static void ClearUnsetFlag(int id)
{
	if (SYSROM_roms[id].unset) {
		SYSROM_roms[id].unset = FALSE;
		--num_unset_roms;
	}
}

int SYSROM_SetPath(char const *filename, int num, ...)
{
	va_list ap;
	int len;
	ULONG crc;
	int retval = SYSROM_OK;
	FILE *f = fopen(filename, "rb");

	if (f == NULL)
		return SYSROM_ERROR;

	len = Util_flen(f);
	/* Don't proceed to CRC computation if the file has invalid size. */
	if (!IsLengthAllowed(len)) {
		fclose(f);
		return SYSROM_BADSIZE;
	}
	Util_rewind(f);
	if (!CRC32_FromFile(f, &crc)) {
		fclose(f);
		return SYSROM_ERROR;
	}
	fclose(f);

	va_start(ap, num);
	while (num > 0) {
		int id = va_arg(ap, int);
		--num;
		if (len != SYSROM_roms[id].size) {
			retval = SYSROM_BADSIZE;
			continue;
		}
		if (SYSROM_roms[id].crc32 != CRC_NULL && crc != SYSROM_roms[id].crc32) {
			retval = SYSROM_BADCRC;
			continue;
		}
		strcpy(SYSROM_roms[id].filename, filename);
		ClearUnsetFlag(id);
		retval = SYSROM_OK;
		break;
	}
	va_end(ap);
	return retval;
}

/* Checks if FILENAME matches to any common ROM image name. Only checks ROM
   images of size LEN. If matched, returns ROM ID.
   Otherwise returns -1. */
static int MatchByName(char const *filename, int len, int only_if_not_set)
{
	static char const * const common_filenames[] = {
		"atariosa.rom", "atari_osa.rom", "atari_os_a.rom",
		"atariosb.rom", "atari_osb.rom", "atari_os_b.rom",
		NULL,
		"atarixlxe.rom", "atarixl.rom", "atari_xlxe.rom", "atari_xl_xe.rom",
		NULL,
		"atari5200.rom", "atar5200.rom", "5200.rom", "5200.bin", "atari_5200.rom",
		NULL,
		"ataribasic.rom", "ataribas.rom", "basic.rom", "atari_basic.rom",
		NULL
	};
	static struct { int const len; int const id; int const offset; } const offsets[] = {
			{ 0x2800, SYSROM_800_CUSTOM, 0 },
			{ 0x4000, SYSROM_XL_CUSTOM, 7 },
			{ 0x800, SYSROM_5200_CUSTOM, 12 },
			{ 0x2000, SYSROM_BASIC_CUSTOM, 18 }
	};
	int i;

	for (i = 0; i < 4; ++i) {
		if (len == offsets[i].len
		    && (!only_if_not_set || SYSROM_roms[i].unset)) {
			/* Start at common filename indexed by OFFSET, end if NULL reached. */
			char const * const *common_filename = common_filenames + offsets[i].offset;
			do {
				if (strcmp(filename, *common_filename) == 0)
					return offsets[i].id;
			} while (*++common_filename != NULL);
		}
	}
	return -1;
}

int SYSROM_FindInDir(char const *directory, int only_if_not_set)
{
	DIR *dir;
	struct dirent *entry;

	if (only_if_not_set && num_unset_roms == 0)
		/* No unset ROM paths left. */
		return TRUE;

	if ((dir = opendir(directory)) == NULL)
		return FALSE;

	while ((entry = readdir(dir)) != NULL) {
		char full_filename[FILENAME_MAX];
		FILE *file;
		int len;
		int id;
		ULONG crc;
		int matched_crc = FALSE;
		Util_catpath(full_filename, directory, entry->d_name);
		if ((file = fopen(full_filename, "rb")) == NULL)
			/* Ignore non-readable files (e.g. directories). */
			continue;

		len = Util_flen(file);
		/* Don't proceed to CRC computation if the file has invalid size. */
		if (!IsLengthAllowed(len)){
			fclose(file);
			continue;
		}
		Util_rewind(file);

		if (!CRC32_FromFile(file, &crc)) {
			fclose(file);
			continue;
		}
		fclose(file);

		/* Match ROM image by CRC. */
		for (id = 0; id < SYSROM_SIZE; ++id) {
			if ((!only_if_not_set || SYSROM_roms[id].unset)
			    && SYSROM_roms[id].size == len
			    && SYSROM_roms[id].crc32 != CRC_NULL && SYSROM_roms[id].crc32 == crc) {
				strcpy(SYSROM_roms[id].filename, full_filename);
				ClearUnsetFlag(id);
				matched_crc = TRUE;
				break;
			}
		}

		if (!matched_crc) {
			/* Match custom ROM image by name. */
			char *c = entry->d_name;
			while (*c != 0) {
				*c = (char)tolower(*c);
				++c;
			}

			id = MatchByName(entry->d_name, len, only_if_not_set);
			if (id >= 0){
				strcpy(SYSROM_roms[id].filename, full_filename);
				ClearUnsetFlag(id);
			}
		}
	}

	closedir(dir);
	return TRUE;
}

void SYSROM_SetDefaults(void)
{
	int i;
	for (i = 0; i < SYSROM_SIZE; ++i)
		SYSROM_roms[i].unset = FALSE;
	num_unset_roms = 0;
}

/* Ordered lists of ROM IDs to choose automatically when SYSROM_os_versions or
   SYSROM_basic_version are set to SYSROM_AUTO. */
static int const autochoose_order_800_ntsc[] = { SYSROM_B_NTSC, SYSROM_A_NTSC, SYSROM_A_PAL, SYSROM_800_CUSTOM, -1 };
static int const autochoose_order_800_pal[] = { SYSROM_A_PAL, SYSROM_B_NTSC, SYSROM_A_NTSC, SYSROM_800_CUSTOM, -1 };
static int const autochoose_order_600xl[] = { SYSROM_BB00R1, SYSROM_BB01R2, SYSROM_BB01R3, SYSROM_BB01R4_OS, SYSROM_XL_CUSTOM, -1 };
static int const autochoose_order_800xl[] = { SYSROM_BB01R2, SYSROM_BB01R3, SYSROM_BB01R4_OS, SYSROM_BB00R1, SYSROM_XL_CUSTOM, -1 };
static int const autochoose_order_xe[] = { SYSROM_BB01R3, SYSROM_BB01R4_OS, SYSROM_BB01R2, SYSROM_BB00R1, SYSROM_XL_CUSTOM, -1 };
static int const autochoose_order_5200[] = { SYSROM_5200, SYSROM_5200A, SYSROM_5200_CUSTOM, -1 };
static int const autochoose_order_basic[] = { SYSROM_BASIC_C, SYSROM_BASIC_B, SYSROM_BASIC_A, SYSROM_BASIC_CUSTOM, -1 };

int SYSROM_AutoChooseOS(int machine_type, int ram_size, int tv_system)
{
	int const *order;

	switch (machine_type) {
	case Atari800_MACHINE_800:
		if (tv_system == Atari800_TV_NTSC)
			order = autochoose_order_800_ntsc;
		else
			order = autochoose_order_800_pal;
		break;
	case Atari800_MACHINE_XLXE:
		switch (ram_size) {
		case 16:
			order = autochoose_order_600xl;
			break;
		case 64:
			order = autochoose_order_800xl;
			break;
		default:
			order = autochoose_order_xe;
		}
		break;
	default: /* Atari800_MACHINE_5200 */
		order = autochoose_order_5200;
		break;
	}

	do {
		if (SYSROM_roms[*order].filename[0] != '\0')
			return *order;
	} while (*++order != -1);
	return -1;
}

int SYSROM_AutoChooseBASIC(void)
{
	int const *order = autochoose_order_basic;
	do {
		if (SYSROM_roms[*order].filename[0] != '\0')
			return *order;
	} while (*++order != -1);
	return -1;
}

int SYSROM_LoadROMs(void)
{
	int os_version;
	if (SYSROM_os_versions[Atari800_machine_type] == SYSROM_AUTO)
		os_version = SYSROM_AutoChooseOS(Atari800_machine_type, MEMORY_ram_size, Atari800_tv_mode);
	else
		os_version = SYSROM_os_versions[Atari800_machine_type];

	/* Load OS ROM. */
	if (os_version == -1
	    || SYSROM_roms[os_version].filename[0] == '\0'
	    || !Atari800_LoadImage(SYSROM_roms[os_version].filename, MEMORY_os, SYSROM_roms[os_version].size))
		/* Loading OS ROM failed. */
		return FALSE;

	if (Atari800_machine_type != Atari800_MACHINE_5200) {
		/* Load BASIC ROM. */
		int basic_version;
		if (SYSROM_basic_version == SYSROM_AUTO)
			basic_version = SYSROM_AutoChooseBASIC();
		else
			basic_version = SYSROM_basic_version;
		if (basic_version == -1
		    || SYSROM_roms[basic_version].filename[0] == '\0'
		    || !(MEMORY_have_basic = Atari800_LoadImage(SYSROM_roms[basic_version].filename, MEMORY_basic, SYSROM_roms[basic_version].size))) {
			/* Loading BASIC ROM failed. */
			if (Atari800_machine_type != Atari800_MACHINE_XLXE)
				return FALSE;
			/* Don't fail on XL/XE - instead fill BASIC ROM with zeroes. */
			MEMORY_have_basic = TRUE;
			memset(MEMORY_basic, 0, 0x2000);
		}
	}

	return TRUE;
}

/* Matches values of OS_*_VERSION and BASIC_VERSION parameters in the config file.
   If matched, writes an appropriate value to *VERSION_PTR (which is a pointer
   to SYSROM_os_versions/SYSROM_basic_version) and returns TRUE; otherwise
   returns FALSE. */
static int MatchROMVersionParameter(char const *string, int const *allowed_vals, int *version_ptr)
{
	if (strcmp(string, "AUTO") == 0) {
		*version_ptr = SYSROM_AUTO;
		return TRUE;
	}
	else do {
		if (Util_stricmp(string, cfg_strings_rev[*allowed_vals]) == 0) {
			*version_ptr = *allowed_vals;
			return TRUE;
		}
	} while (*++allowed_vals != -1);
	/* *string not matched to any allowed value. */
	return FALSE;
}

int SYSROM_ReadConfig(char *string, char *ptr)
{
	int id = CFG_MatchTextParameter(string, cfg_strings, SYSROM_SIZE);
	if (id >= 0) {
		/* For faster start, don't check if CRC matches. */
		Util_strlcpy(SYSROM_roms[id].filename, ptr, FILENAME_MAX);
		ClearUnsetFlag(id);
	}
	else if (strcmp(string, "OS_400/800_VERSION") == 0) {
		if (!MatchROMVersionParameter(ptr, autochoose_order_800_ntsc, &SYSROM_os_versions[Atari800_MACHINE_800]))
			return FALSE;
	}
	else if (strcmp(string, "OS_XL/XE_VERSION") == 0) {
		if (!MatchROMVersionParameter(ptr, autochoose_order_600xl, &SYSROM_os_versions[Atari800_MACHINE_XLXE]))
			return FALSE;
	}
	else if (strcmp(string, "OS_5200_VERSION") == 0) {
		if (!MatchROMVersionParameter(ptr, autochoose_order_5200, &SYSROM_os_versions[Atari800_MACHINE_5200]))
			return FALSE;
	}
	else if (strcmp(string, "BASIC_VERSION") == 0) {
		if (!MatchROMVersionParameter(ptr, autochoose_order_basic, &SYSROM_basic_version))
			return FALSE;
	}
	/* Parse legacy config parameters. */
	else if (strcmp(string, "OS/A_ROM") == 0) {
		if (SYSROM_SetPath(ptr, 2, SYSROM_A_NTSC, SYSROM_A_PAL) != SYSROM_OK)
			return FALSE;
	}
	else if (strcmp(string, "OS/B_ROM") == 0) {
		if (SYSROM_SetPath(ptr, 2, SYSROM_B_NTSC, SYSROM_800_CUSTOM) != SYSROM_OK)
			return FALSE;
	}
	else if (strcmp(string, "XL/XE_ROM") == 0) {
		if (SYSROM_SetPath(ptr, 5, SYSROM_BB00R1, SYSROM_BB01R2, SYSROM_BB01R3, SYSROM_BB01R4_OS, SYSROM_XL_CUSTOM) != SYSROM_OK)
			return FALSE;
	}
	else if (strcmp(string, "5200_ROM") == 0) {
		if (SYSROM_SetPath(ptr, 3, SYSROM_5200, SYSROM_5200A, SYSROM_5200_CUSTOM) != SYSROM_OK)
			return FALSE;
	}
	else if (strcmp(string, "BASIC_ROM") == 0) {
		if (SYSROM_SetPath(ptr, 4, SYSROM_BASIC_A, SYSROM_BASIC_B, SYSROM_BASIC_C, SYSROM_BASIC_CUSTOM) != SYSROM_OK)
			return FALSE;
	}
	else return FALSE;
	return TRUE;
}

void SYSROM_WriteConfig(FILE *fp)
{
	int id;
	for (id = 0; id < SYSROM_SIZE; ++id) {
		if (!SYSROM_roms[id].unset)
			fprintf(fp, "%s=%s\n", cfg_strings[id], SYSROM_roms[id].filename);
	}
	fprintf(fp, "OS_400/800_VERSION=%s\n", cfg_strings_rev[SYSROM_os_versions[Atari800_MACHINE_800]]);
	fprintf(fp, "OS_XL/XE_VERSION=%s\n", cfg_strings_rev[SYSROM_os_versions[Atari800_MACHINE_XLXE]]);
	fprintf(fp, "OS_5200_VERSION=%s\n", cfg_strings_rev[SYSROM_os_versions[Atari800_MACHINE_5200]]);
	fprintf(fp, "BASIC_VERSION=%s\n", cfg_strings_rev[SYSROM_basic_version]);
}

int SYSROM_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc); /* is argument available? */
		int a_m = FALSE; /* error, argument missing! */
		int a_i = FALSE; /* error, argument invalid! */

		if (strcmp(argv[i], "-osa_rom") == 0) {
			if (i_a) {
				if (SYSROM_SetPath(argv[++i], 2, SYSROM_A_NTSC, SYSROM_A_PAL) != SYSROM_OK)
					a_i = TRUE;
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-osb_rom") == 0) {
			if (i_a) {
				if (SYSROM_SetPath(argv[++i], 2, SYSROM_B_NTSC, SYSROM_800_CUSTOM) != SYSROM_OK)
					a_i = TRUE;
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-xlxe_rom") == 0) {
			if (i_a) {
				if (SYSROM_SetPath(argv[++i], 5, SYSROM_BB00R1, SYSROM_BB01R2, SYSROM_BB01R3, SYSROM_BB01R4_OS, SYSROM_XL_CUSTOM) != SYSROM_OK)
					a_i = TRUE;
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-5200_rom") == 0) {
			if (i_a) {
				if (SYSROM_SetPath(argv[++i], 3, SYSROM_5200, SYSROM_5200A, SYSROM_5200_CUSTOM) != SYSROM_OK)
					a_i = TRUE;
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-basic_rom") == 0) {
			if (i_a) {
				if (SYSROM_SetPath(argv[++i], 4, SYSROM_BASIC_A, SYSROM_BASIC_B, SYSROM_BASIC_C, SYSROM_BASIC_CUSTOM) != SYSROM_OK)
					a_i = TRUE;
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-800-rev") == 0) {
			if (i_a) {
				if (!MatchROMVersionParameter(argv[++i], autochoose_order_800_ntsc, &SYSROM_os_versions[Atari800_MACHINE_800]))
					a_i = TRUE;
			}
			else a_m= TRUE;
		}
		else if (strcmp(argv[i], "-xl-rev") == 0) {
			if (i_a) {
				if (!MatchROMVersionParameter(argv[++i], autochoose_order_600xl, &SYSROM_os_versions[Atari800_MACHINE_XLXE]))
					a_i = TRUE;
			}
			else a_m= TRUE;
		}
		else if (strcmp(argv[i], "-5200-rev") == 0) {
			if (i_a) {
				if (!MatchROMVersionParameter(argv[++i], autochoose_order_5200, &SYSROM_os_versions[Atari800_MACHINE_5200]))
					a_i = TRUE;
			}
			else a_m= TRUE;
		}
		else if (strcmp(argv[i], "-basic-rev") == 0) {
			if (i_a) {
				if (!MatchROMVersionParameter(argv[++i], autochoose_order_basic, &SYSROM_basic_version))
					a_i = TRUE;
			}
			else a_m= TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-osa_rom <file>   Load OS A ROM from file");
				Log_print("\t-osb_rom <file>   Load OS B ROM from file");
				Log_print("\t-xlxe_rom <file>  Load XL/XE ROM from file");
				Log_print("\t-5200_rom <file>  Load 5200 ROM from file");
				Log_print("\t-basic_rom <file> Load BASIC ROM from file");
				Log_print("\t-800-rev auto|a-ntsc|a-pal|b-ntsc|custom");
				Log_print("\t                  Select 400/800 OS revision");
				Log_print("\t-xl-rev auto|1|2|3|4|custom");
				Log_print("\t                  Select XL/XE OS revision");
				Log_print("\t-5200-rev auto|orig|a|custom");
				Log_print("\t                  Select 5200 OS revision");
				Log_print("\t-basic-rev auto|a|b|c|custom");
				Log_print("\t                  Select BASIC revision");
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

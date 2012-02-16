#ifndef SYSROM_H_
#define SYSROM_H_

#include "atari.h"

/* ROM IDs for all supported ROM images. */
enum {
	/* --- OS ROMs from released Atari computers --- */
	/* OS rev. A (1979) from early NTSC 400/800. Part no. CO12499A + CO14599A + CO12399B */
	SYSROM_A_NTSC,
	/* OS rev. A (1979) from PAL 400/800. Part no. CO15199 + CO15299 + CO12399B */
	SYSROM_A_PAL,
	/* OS rev. B (1981) from late NTSC 400/800. Part no. CO12499B + CO14599B + 12399B */
	SYSROM_B_NTSC,
	/* OS rev. 1 (1983-03-11) from 600XL. Part no. CO62024 */
	SYSROM_BB00R1,
	/* OS rev. 2 (1983-05-10) from 800XL and early 65XE/130XE. Part no. CO61598B */
	SYSROM_BB01R2,
	/* OS rev. 3 (1985-03-01) from late 65XE/130XE. Part no. C300717 */
	SYSROM_BB01R3,
	/* OS rev. 4 from XEGS - OS only. Part no. C101687 (2nd half) */
	SYSROM_BB01R4_OS,
	/* --- BIOS ROMs from Atari 5200 --- */
	/* BIOS from 4-port and early 2-port 5200 (1982). Part no. C019156 */
	SYSROM_5200,
	/* BIOS from late 2-port 5200 (1983). Part no. C019156A */
	SYSROM_5200A,
	/* --- Atari BASIC ROMs --- */
	/* Rev. A (1979), sold on cartridge. Part no. CO12402 + CO14502 */
	SYSROM_BASIC_A,
	/* Rev. B (1983), from 600XL/early 800XL, also on cartridge. Part no. CO60302A */
	SYSROM_BASIC_B,
	/* Rev. C (1984), from late 800XL and all XE/XEGS, also on cartridge, Part no. CO24947A */
	SYSROM_BASIC_C,
	/* --- Custom ROMs --- */
	SYSROM_800_CUSTOM, /* Custom 400/800 OS */
	SYSROM_XL_CUSTOM, /* Custom XL/XE OS */
	SYSROM_5200_CUSTOM, /* Custom 5200 BIOS */
	SYSROM_BASIC_CUSTOM,/* Custom BASIC */
	SYSROM_SIZE, /* Number of available OS ROMs */
	SYSROM_AUTO = SYSROM_SIZE /* Use to indicate that OS revision should be chosen automatically */
};
typedef struct SYSROM_t {
	char *filename; /* Path to the ROM image file */
	size_t size; /* Expected size of the ROM image */
	ULONG crc32; /* Expected CRC32 of the ROM image */
	int unset; /* During initialisation indicates that no filename was given for this ROM image in config/command line */
} SYSROM_t;

/* Table of all supported ROM images with their sizes and CRCs, indexed by ROM IDs. */
extern SYSROM_t SYSROM_roms[SYSROM_SIZE];

/* OS version preference chosen by user. Indexed by value of Atari800_machine_type.
   Set these to SYSROM_AUTO to let the emulator choose the OS revision automatically. */
extern int SYSROM_os_versions[3];
/* BASIC version preference chosen by user. Set this to SYSROM_AUTO to let the emulator
   choose the BASIC revision automatically. */
extern int SYSROM_basic_version;

/* Values returned by SYSROM_SetPath(). */
enum{
	SYSROM_OK, /* No error. */
	SYSROM_ERROR, /* File read error */
	SYSROM_BADSIZE, /* File has inappropriate size */
	SYSROM_BADCRC /* File has invalid checksum */
};

/* Set path to a ROM image. The ... parameters are ROM IDs (NUM indicates
   number of parameters). The file pointed by FILENAME is checked against the
   expected size and CRC for each of the roms in ..., and the path is set to
   the first match. See the above enum for possible return values. */
int SYSROM_SetPath(char const *filename, int num, ...);

/* Find ROM images in DIRECTORY, checking each file against known file sizes
   and CRCs. For each ROM for which a matching file is found, its filename is
   updated.
   If ONLY_IF_NOT_SET is TRUE, only paths that weren't given in the config
   file/command line are updated.
   Returns FALSE if it couldn't open DIRECTORY; otherwise returns TRUE. */
int SYSROM_FindInDir(char const *directory, int only_if_not_set);

/* Return ROM ID of the "best" available OS ROM for a given machine_type/
   ram_size/tv_system. If no OS for the given system is available, returns -1;
   otherwise returns a ROM ID. */
int SYSROM_AutoChooseOS(int machine_type, int ram_size, int tv_system);
/* Return ROM ID of the "best" available BASIC ROM. If no BASIC ROM is
   available, returns -1; otherwise returns a ROM ID. */
int SYSROM_AutoChooseBASIC(void);

/* Called after initialisation ended. Fills the filenames with default values,
   so that when writing the config file all config options will be written,
   making edits to the config file easier. */
void SYSROM_SetDefaults(void);

/* Called from Atari800_Initialise(). Loads the OS/BASIC ROMs chosen by
   SYSROM_os_versions and SYSROM_basic_version. If ROM loading succeeded,
   returns TRUE; otherwise FALSE. */
int SYSROM_LoadROMs(void);

/* Read/write from/to configuration file. */
int SYSROM_ReadConfig(char *string, char *ptr);
void SYSROM_WriteConfig(FILE *fp);

/* Processing of command line arguments and initialisation of the module. */
int SYSROM_Initialise(int *argc, char *argv[]);

#endif /* SYSROM_H_ */

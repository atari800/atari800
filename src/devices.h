#ifndef _DEVICES_H_
#define _DEVICES_H_

#include <stdio.h> /* FILENAME_MAX */
#include "atari.h" /* UWORD */

void Device_Initialise(int *argc, char *argv[]);
int Device_PatchOS(void);
void Device_Frame(void);
void Device_UpdatePatches(void);

UWORD Device_SkipDeviceName(void);

extern int enable_h_patch;
extern int enable_p_patch;
extern int enable_r_patch;

extern char atari_h_dir[4][FILENAME_MAX];
extern int h_read_only;

#define DEFAULT_H_PATH  "H1:>DOS;>DOS"
extern char h_exe_path[FILENAME_MAX];

char h_current_dir[4][FILENAME_MAX];

int Device_H_CountOpen(void);
void Device_H_CloseAll(void);

extern char print_command[256];

int Device_SetPrintCommand(const char *command);

#define	ICHIDZ	0x0020
#define	ICDNOZ	0x0021
#define	ICCOMZ	0x0022
#define	ICSTAZ	0x0023
#define	ICBALZ	0x0024
#define	ICBAHZ	0x0025
#define	ICPTLZ	0x0026
#define	ICPTHZ	0x0027
#define	ICBLLZ	0x0028
#define	ICBLHZ	0x0029
#define	ICAX1Z	0x002a
#define	ICAX2Z	0x002b

#define IOCB0   0x0340
#define	ICHID	0x0000
#define	ICDNO	0x0001
#define	ICCOM	0x0002
#define	ICSTA	0x0003
#define	ICBAL	0x0004
#define	ICBAH	0x0005
#define	ICPTL	0x0006
#define	ICPTH	0x0007
#define	ICBLL	0x0008
#define	ICBLH	0x0009
#define	ICAX1	0x000a
#define	ICAX2	0x000b
#define	ICAX3	0x000c
#define	ICAX4	0x000d
#define	ICAX5	0x000e
#define	ICAX6	0x000f

#define DEVICE_TABLE_OPEN	0
#define DEVICE_TABLE_CLOS	2
#define DEVICE_TABLE_READ	4
#define DEVICE_TABLE_WRIT	6
#define DEVICE_TABLE_STAT	8
#define DEVICE_TABLE_SPEC	10
#define DEVICE_TABLE_INIT	12

UWORD Device_UpdateHATABSEntry(char device, UWORD entry_address, UWORD table_address);
void Device_RemoveHATABSEntry(char device, UWORD entry_address, UWORD table_address);

#endif /* _DEVICES_H_ */

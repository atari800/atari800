#ifndef __DEVICES__
#define __DEVICES__

#include "atari.h"	/* for UWORD */

/* following is for atari.c */

void Device_Initialise(int *argc, char *argv[]);
int Device_PatchOS(void);
void Device_Frame(void);
void Device_UpdatePatches(void);

/* following is for ports which want to add their specific devices */

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

#define DEVICE_TABLE_OPEN	0
#define DEVICE_TABLE_CLOS	2
#define DEVICE_TABLE_READ	4
#define DEVICE_TABLE_WRIT	6
#define DEVICE_TABLE_STAT	8
#define DEVICE_TABLE_SPEC	10
#define DEVICE_TABLE_INIT	12

UWORD Device_UpdateHATABSEntry(char device, UWORD entry_address, UWORD table_address);
void Device_RemoveHATABSEntry(char device, UWORD entry_address, UWORD table_address);

#endif

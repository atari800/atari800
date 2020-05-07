#ifndef CARTRIDGE_H_
#define CARTRIDGE_H_

#include "config.h"
#include "atari.h"
#include "cartridge_info.h"

/* Indicates whether the emulator should automatically reboot (coldstart)
   after inserting/removing a cartridge. (Doesn't affect the piggyback
   cartridge - in this case system will never autoreboot.) */
extern int CARTRIDGE_autoreboot;

typedef struct CARTRIDGE_image_t {
	int type;
	int state; /* Cartridge's state, such as selected bank or switch on/off. */
	int size; /* Size of the image, in kilobytes */
	UBYTE *image;
	char filename[FILENAME_MAX];
} CARTRIDGE_image_t;

extern CARTRIDGE_image_t CARTRIDGE_main;
extern CARTRIDGE_image_t CARTRIDGE_piggyback;

int CARTRIDGE_ReadConfig(char *string, char *ptr);
void CARTRIDGE_WriteConfig(FILE *fp);
int CARTRIDGE_Initialise(int *argc, char *argv[]);
void CARTRIDGE_Exit(void);

#define CARTRIDGE_CANT_OPEN		-1	/* Can't open cartridge image file */
#define CARTRIDGE_BAD_FORMAT		-2	/* Unknown cartridge format */
#define CARTRIDGE_BAD_CHECKSUM	-3	/* Warning: bad CART checksum */
/* Inserts the left cartrifge. */
int CARTRIDGE_Insert(const char *filename);
/* Inserts the left cartridge and reboots the system if needed. */
int CARTRIDGE_InsertAutoReboot(const char *filename);
/* Inserts the piggyback cartridge. */
int CARTRIDGE_Insert_Second(const char *filename);
/* When the cartridge type is CARTRIDGE_UNKNOWN after a call to
   CARTRIDGE_Insert(), this function should be called to set the
   cartridge's type manually to a value chosen by user. */
void CARTRIDGE_SetType(CARTRIDGE_image_t *cart, int type);
/* Sets type of the cartridge and reboots the system if needed. */
void CARTRIDGE_SetTypeAutoReboot(CARTRIDGE_image_t *cart, int type);

/* Removes the left cartridge. */
void CARTRIDGE_Remove(void);
/* Removes the left cartridge and reboots the system if needed. */
void CARTRIDGE_RemoveAutoReboot(void);
/* Removed the piggyback cartridge. */
void CARTRIDGE_Remove_Second(void);

/* Called on system coldstart. Resets the states of mounted cartridges. */
void CARTRIDGE_ColdStart(void);

UBYTE CARTRIDGE_GetByte(UWORD addr, int no_side_effects);
void CARTRIDGE_PutByte(UWORD addr, UBYTE byte);
void CARTRIDGE_BountyBob1(UWORD addr);
void CARTRIDGE_BountyBob2(UWORD addr);
void CARTRIDGE_StateSave(void);
void CARTRIDGE_StateRead(UBYTE version);
#ifdef PAGED_ATTRIB
UBYTE CARTRIDGE_BountyBob1GetByte(UWORD addr, int no_side_effects);
UBYTE CARTRIDGE_BountyBob2GetByte(UWORD addr, int no_side_effects);
void CARTRIDGE_BountyBob1PutByte(UWORD addr, UBYTE value);
void CARTRIDGE_BountyBob2PutByte(UWORD addr, UBYTE value);
#endif

#endif /* CARTRIDGE_H_ */

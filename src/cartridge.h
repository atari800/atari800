#ifndef CARTRIDGE_H_
#define CARTRIDGE_H_

#include "config.h"
#include "atari.h"
#include "cartridge_info.h"

/* Indicates whether the emulator should automatically reboot (coldstart)
   after inserting/removing a cartridge. (Doesn't affect the piggyback
   cartridge - in this case system will never autoreboot.) */
extern int CARTRIDGE_autoreboot;

/*
 * Ram-Cart state flag bits meaning:
 * ---------------------------------
 * Lower byte contains value of control register at $D500
 *  0: $A000-$BFFF enable (0-on in ReadOnly mode and off in Read/Write mode, 1-off in RO and on in R/W mode)
 *  1: $8000-$9FFF enable (0-off, 1-on)
 *  2: lock in 64K/128K/2x128K (0-off, 1-on)
 *  2: bank select in 256K and modified 1M/2M/4M (when ABC jumpers are installed)
 *  2: read A switch in stock 1M/2M/4M (not affected by write)
 *  3: bank select in 64K
 *  4: bank select in 64K
 *  5: bank select in 128K
 *  6: bank select in modified 1M/2M/4M
 *  6: read B switch in stock 1M/2M/4M (not affected by write)
 *  7: bank select in modified 1M/2M/4M
 *  7: read C switch in stock 1M/2M/4M (not affected by write)
 * Upper byte contains state of switches on the top of case
 *  8: 1/2M switch state in 2M
 *  8: D switch state in 4M
 *  8: bank select in 8/16/32M
 *  9: 2/4 switch state in 4M
 *  9: bank select in 8/16/32M
 * 10: bank select in 8/16/32M
 * 11: bank select in 16/32M
 * 12: R/W switch state (0-ReadOnly, 1-Read/Write mode)
 * 13: P1 switch state for Double Ram-Cart 2x128K/256K (0-2x128K, 1-256K mode)
 * 14: P2 switch state for Double Ram-Cart 2x128K/256K and 1M from Zenon/Dial
 *     (0-first module, 1-second module when cart is in 2x128K mode;
 *      0-normal module order, 1-swapped module order when cart is in 256K mode)
 * 15: jumpers ABC installed flag (0-stock mode, 1-jumpers installed)
 * Next byte contains additional flags for many variants of Zenon/Dial carts
 * 16: full address decoder for 1M from Zenon/Dial (0-not installed, 1-installed)
 * 17: control register type for 2x128K/256K and 1M from Zenon/Dial (0-write only, 1-read/write)
 * 18: bank select in 32M
 */
typedef struct CARTRIDGE_image_t {
	int type;
	int state; /* Cartridge's state, such as selected bank or switch on/off. */
	int size; /* Size of the image, in kilobytes. */
	UBYTE *image;
	char filename[FILENAME_MAX];
	int raw; /* File contains RAW data (important for writeable cartridges). */
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
#define CARTRIDGE_TOO_FEW_DATA	-4	/* Too few data in file */

/* Inserts the left cartridge from FILENAME. Copies FILENAME to
   CARTRIDGE_main.filename.
   If loading failed, sets CARTRIDGE_main.type to CARTRIDGE_NONE and returns
   one of:
   * CARTRIDGE_CANT_OPEN if there was an error when opening file,
   * CARTRIDGE_BAD_FORMAT if the file is not a proper cartridge image.

   If loading succeeded, allocates a buffer with cartridge image data and puts
   it in CARTRIDGE_main.image. Then sets CARTRIDGE_main.type if possible, and
   returns one of:
   * 0 if cartridge type was recognized; CARTRIDGE_main.type is then set
     correctly;
   * CARTRIDGE_BAD_CHECKSUM if cartridge is a CART file but with invalid
     checksum; CARTRIDGE_main.type is then set correctly;
   * a positive integer: size in KB if cartridge type was not guessed;
     CARTRIDGE_main.type is then set to CARTRIDGE_UNKNOWN. The caller is
     expected to select a cartridge type according to the returned size, and
     call either CARTRIDGE_SetType() or CARTRIDGE_SetTypeAutoReboot(). */
int CARTRIDGE_Insert(const char *filename);
/* Inserts the left cartridge - identically to CARTRIDGE_Insert(), then
   reboots the system if needed. */
int CARTRIDGE_InsertAutoReboot(const char *filename);
/* Inserts the piggyback cartridge. Works identically to CARTRIDGE_Insert(),
   but modifies CARTRIDGE_piggyback instead of CARTRIDGE_main. */
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
void CARTRIDGE_StateSave(void);
void CARTRIDGE_StateRead(UBYTE version);

/* addr must be $4fxx in 5200 mode or $8fxx in 800 mode. */
UBYTE CARTRIDGE_BountyBob1GetByte(UWORD addr, int no_side_effects);

/* addr must be $5fxx in 5200 mode or $9fxx in 800 mode. */
UBYTE CARTRIDGE_BountyBob2GetByte(UWORD addr, int no_side_effects);

/* addr must be $bfxx in 5200 mode only. */
UBYTE CARTRIDGE_5200SuperCartGetByte(UWORD addr, int no_side_effects);

/* addr must be $4fxx in 5200 mode or $8fxx in 800 mode. */
void CARTRIDGE_BountyBob1PutByte(UWORD addr, UBYTE value);

/* addr must be $5fxx in 5200 mode or $9fxx in 800 mode. */
void CARTRIDGE_BountyBob2PutByte(UWORD addr, UBYTE value);

/* addr must be $bfxx in 5200 mode only. */
void CARTRIDGE_5200SuperCartPutByte(UWORD addr, UBYTE value);

int CARTRIDGE_ReadImage(const char *filename, CARTRIDGE_image_t *cart);
int CARTRIDGE_WriteImage(char *filename, int type, UBYTE *image, int size, int raw, UBYTE value);

void CARTRIDGE_UpdateState(CARTRIDGE_image_t *cart, int old_state);
#endif /* CARTRIDGE_H_ */

#ifndef CASSETTE_H_
#define CASSETTE_H_

#include <stdio.h>		/* for FILE and FILENAME_MAX */

#include "atari.h"		/* for UBYTE */

#define CASSETTE_DESCRIPTION_MAX 256

extern char CASSETTE_filename[FILENAME_MAX];
extern char CASSETTE_description[CASSETTE_DESCRIPTION_MAX];
typedef enum {
	CASSETTE_STATUS_NONE,
	CASSETTE_STATUS_READ_ONLY,
	CASSETTE_STATUS_READ_WRITE
} CASSETTE_status_t;
extern CASSETTE_status_t CASSETTE_status;

extern int CASSETTE_current_block;
extern int CASSETTE_max_block;

/* Used in Atari800_Initialise during emulator initialisation */
int CASSETTE_Initialise(int *argc, char *argv[]);
void CASSETTE_Exit(void);
/* Config file read/write */
int CASSETTE_ReadConfig(char *string, char *ptr);
void CASSETTE_WriteConfig(FILE *fp);

int CASSETTE_CheckFile(const char *filename, FILE **fp, char *description, int *last_block, int *isCAS, int *writable);
int CASSETTE_Insert(const char *filename);
void CASSETTE_Remove(void);
/* Creates a new file in CAS format. DESCRIPTION can be NULL.
   Returns TRUE on success, FALSE otherwise. */
int CASSETTE_CreateCAS(char const *filename, char const *description);

extern int CASSETTE_hold_start;
extern int CASSETTE_hold_start_on_reboot; /* preserve hold_start after reboot */
extern int CASSETTE_press_space;

 /* Is cassette record button pressed? Don't change the variable directly, use CASSETTE_ToggleRecord(). */
extern int CASSETTE_record;
void CASSETTE_ToggleRecord(void);

int CASSETTE_AddGap(int gaptime);
void CASSETTE_LeaderLoad(void);
void CASSETTE_LeaderSave(void);
/* Reads a record from tape and copies its contents (max. LENGTH bytes,
   excluding the trailing checksum) to memory starting at address DEST_ADDR.
   Returns FALSE if number of bytes in record doesn't equal LENGTH, or
   checksum is incorrect, or there was a read error/end of file; otherwise
   returns TRUE. */
int CASSETTE_ReadToMemory(UWORD dest_addr, int length);
/* Reads LENGTH bytes from memory starting at SRC_ADDR and writes them as
   a record (with added checksum) to tape. Returns FALSE if there was a write
   error, TRUE otherwise. */
int CASSETTE_WriteFromMemory(UWORD src_addr, int length);
void CASSETTE_Seek(unsigned int position);
int CASSETTE_IOLineStatus(void);
int CASSETTE_GetByte(void);
void CASSETTE_PutByte(int byte);
void CASSETTE_TapeMotor(int onoff);
/* Advance the tape by a scanline. Return TRUE if a new byte has been loaded
   and POKEY_SERIN must be updated. */
int CASSETTE_AddScanLine(void);
/* Reset cassette serial transmission; call when resseting POKEY by SKCTL. */
void CASSETTE_ResetPOKEY(void);
#endif /* CASSETTE_H_ */

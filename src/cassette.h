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
/* Attaches a tape image. Returns FALSE on failure.
   Resets the CASSETTE_write_protect to FALSE. */
int CASSETTE_Insert(const char *filename);
void CASSETTE_Remove(void);
/* Creates a new file in CAS format. DESCRIPTION can be NULL.
   Returns TRUE on success, FALSE otherwise. */
int CASSETTE_CreateCAS(char const *filename, char const *description);

extern int CASSETTE_hold_start;
extern int CASSETTE_hold_start_on_reboot; /* preserve hold_start after reboot */
extern int CASSETTE_press_space;

/* Is cassette file write-protected? Don't change directly, use CASSETTE_ToggleWriteProtect(). */
extern int CASSETTE_write_protect;
/* Switches RO/RW. Fails with FALSE if the tape cannot be switched to RW. */
int CASSETTE_ToggleWriteProtect(void);

 /* Is cassette record button pressed? Don't change directly, use CASSETTE_ToggleRecord(). */
extern int CASSETTE_record;
/* If tape is mounted, switches recording on/off (otherwise return FALSE).
   Recording operations would fail if the tape is read-only. In such
   situation, when switching recording on the function returns FALSE. */
int CASSETTE_ToggleRecord(void);

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

/* --- Functions used by patched SIO --- */
/* -- SIO_Handler() -- */
int CASSETTE_AddGap(int gaptime);
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
/* -- Other -- */
void CASSETTE_LeaderLoad(void);
void CASSETTE_LeaderSave(void);

#endif /* CASSETTE_H_ */

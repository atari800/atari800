#ifndef _CASSETTE_H_
#define _CASSETTE_H_

#include <stdio.h>		/* for FILE and FILENAME_MAX */

#include "atari.h"		/* for UBYTE */

#define CASSETTE_DESCRIPTION_MAX 256

void CASSETTE_Initialise(int *argc, char *argv[]);

int CASSETTE_CheckFile(const char *filename, FILE **fp, char *description, int *last_block, int *isCAS);
int CASSETTE_CreateFile(const char *filename, FILE **fp, int *isCAS);
int CASSETTE_Insert(const char *filename);
void CASSETTE_Remove(void);
extern char cassette_filename[FILENAME_MAX];
extern char cassette_description[CASSETTE_DESCRIPTION_MAX];

extern int cassette_current_block;
extern int cassette_max_block;

extern int hold_start;
extern int hold_start_on_reboot; /* preserve hold_start after reboot */
extern int press_space;

int CASSETTE_AddGap(int gaptime);
void CASSETTE_LeaderLoad(void);
void CASSETTE_LeaderSave(void);
int CASSETTE_Read(void);
int CASSETTE_Write(int length);
int CASSETTE_IOLineStatus(void);
int CASSETTE_GetByte(void);
int CASSETTE_GetInputIRQDelay(void);
void CASSETTE_TapeMotor(int onoff);
void CASSETTE_AddScanLine(void);
extern UBYTE cassette_buffer[4096];

#endif /* _CASSETTE_H_ */

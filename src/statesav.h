#ifndef _STATESAV_H_
#define _STATESAV_H_

#include "atari.h"

int SaveAtariState(const char *filename, const char *mode, UBYTE SaveVerbose);
int ReadAtariState(const char *filename, const char *mode);

void SaveUBYTE(const UBYTE *data, int num);
void SaveUWORD(const UWORD *data, int num);
void SaveINT(const int *data, int num);
void SaveFNAME(const char *filename);

void ReadUBYTE(UBYTE *data, int num);
void ReadUWORD(UWORD *data, int num);
void ReadINT(int *data, int num);
void ReadFNAME(char *filename);

#endif

#ifndef __SNDSAVE__
#define __SNDSAVE__

#include "atari.h"

int SndSave_IsSoundFileOpen(void);
int SndSave_CloseSoundFile(void);
int SndSave_OpenSoundFile(const char *szFileName);
int SndSave_WriteToSoundFile(const UBYTE *ucBuffer, unsigned int uiSize);

#endif


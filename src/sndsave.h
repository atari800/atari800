#ifndef __SNDSAVE__
#define __SNDSAVE__

#include "atari.h"

int IsSoundFileOpen(void);
int CloseSoundFile(void);
int OpenSoundFile(const char *szFileName);
int WriteToSoundFile(const UBYTE *ucBuffer, unsigned int uiSize);

#endif


#ifndef __SNDSAVE__
#define __SNDSAVE__

#include "atari.h"
#include <stdio.h>

extern FILE		*sndoutput;
extern int		iSoundRate;

typedef int BOOL;

BOOL IsSoundFileOpen( void );
BOOL CloseSoundFile( void );
BOOL OpenSoundFile( const char *szFileName );
int WriteToSoundFile( const unsigned char *ucBuffer, const unsigned int uiSize );

#endif


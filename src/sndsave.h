#ifndef __SNDSAVE__
#define __SNDSAVE__

#include "atari.h"
#include <stdio.h>

extern FILE		*sndoutput;
extern int		iSoundRate;

BOOL IsSoundFileOpen( void );
BOOL CloseSoundFile( void );
BOOL OpenSoundFile( char *szFileName );
int WriteToSoundFile( unsigned char *ucBuffer, unsigned int uiSize );

#endif
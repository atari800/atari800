#ifndef MULTIMEDIA_H_
#define MULTIMEDIA_H_

#include "atari.h"

int Multimedia_IsFileOpen(void);
int Multimedia_OpenSoundFile(const char *szFileName);
int Multimedia_WriteAudio(const UBYTE *ucBuffer, unsigned int uiSize);
int Multimedia_CloseFile(void);

#ifdef AVI_VIDEO_RECORDING
int Multimedia_OpenVideoFile(const char *szFileName);
int Multimedia_WriteVideo(void);
#endif

#endif /* MULTIMEDIA_H_ */


#ifndef FILE_EXPORT_H_
#define FILE_EXPORT_H_

#include "atari.h"

#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ)
extern int FILE_EXPORT_compression_level;
#endif

int File_Export_Initialise(int *argc, char *argv[]);
int File_Export_ReadConfig(char *string, char *ptr);
void File_Export_WriteConfig(FILE *fp);

#if defined(SOUND) || defined(AVI_VIDEO_RECORDING)
int File_Export_ElapsedTime(void);
int File_Export_CurrentSize(void);
char *File_Export_Description(void);
#endif

void fputw(UWORD, FILE *fp);
void fputl(ULONG, FILE *fp);
size_t fwritele(const void *ptr, size_t size, size_t nmemb, FILE *fp);

#ifdef SOUND
FILE *WAV_OpenFile(const char *szFileName);
int WAV_WriteSamples(const unsigned char *buf, unsigned int num_samples, FILE *fp);
int WAV_CloseFile(FILE *fp);
#endif

#ifdef AVI_VIDEO_RECORDING
FILE *AVI_OpenFile(const char *szFileName);
int AVI_CloseFile(FILE *fp);
int AVI_AddVideoFrame(FILE *fp);
#ifdef SOUND
int AVI_AddAudioSamples(const UBYTE *buf, int num_samples, FILE *fp);
#endif /* SOUND */
#endif /* AVI_VIDEO_RECORDING */

#endif /* FILE_EXPORT_H_ */


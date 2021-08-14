#ifndef FILE_SAVE_H_
#define FILE_SAVE_H_

#include "atari.h"

#define VIDEO_CODEC_MRLE 1
#define VIDEO_CODEC_PNG 2
#ifdef HAVE_LIBPNG
#define VIDEO_CODEC_BEST_AVAILABLE 2
#else
#define VIDEO_CODEC_BEST_AVAILABLE 1
#endif

int File_Save_Initialise(int *argc, char *argv[]);

void fputw(UWORD, FILE *fp);
void fputl(ULONG, FILE *fp);
size_t fwritele(const void *ptr, size_t size, size_t nmemb, FILE *fp);

void PCX_SaveScreen(FILE *fp, UBYTE *ptr1, UBYTE *ptr2);
#ifdef HAVE_LIBPNG
void PNG_SaveScreen(FILE *fp, UBYTE *ptr1, UBYTE *ptr2);
#endif

#ifdef SOUND
FILE *WAV_OpenFile(const char *szFileName);
int WAV_WriteSamples(const unsigned char *buf, unsigned int num_samples, FILE *fp);
int WAV_CloseFile(FILE *fp);
#ifdef AVI_VIDEO_RECORDING
int MRLE_CreateFrame(UBYTE *buf, int bufsize, const UBYTE *source);
FILE *AVI_OpenFile(const char *szFileName);
int AVI_CloseFile(FILE *fp);
int AVI_AddVideoFrame(FILE *fp);
int AVI_AddAudioSamples(const UBYTE *buf, int num_samples, FILE *fp);
#endif /* AVI_VIDEO_RECORDING */
#endif /* SOUND */

#endif /* FILE_SAVE_H_ */


#ifndef FILE_EXPORT_H_
#define FILE_EXPORT_H_

#include "atari.h"

#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ)
extern int FILE_EXPORT_compression_level;
#endif

int File_Export_Initialise(int *argc, char *argv[]);
int File_Export_ReadConfig(char *string, char *ptr);
void File_Export_WriteConfig(FILE *fp);

void fputw(UWORD, FILE *fp);
void fputl(ULONG, FILE *fp);

extern char *FILE_EXPORT_error_message;
void File_Export_SetErrorMessage(const char *string);
void File_Export_SetErrorMessageArg(const char *format, const char *arg);

#ifdef MULTIMEDIA
int File_Export_IsRecording(void);
int File_Export_StopRecording(void);
int File_Export_StartRecording(const char *fileName);

#ifdef SOUND
int File_Export_GetNextSoundFile(char *buffer, int bufsize);
int File_Export_WriteAudio(const UBYTE *samples, int num_samples);
#endif

#ifdef VIDEO_RECORDING
int File_Export_GetNextVideoFile(char *buffer, int bufsize);
int File_Export_WriteVideo(void);
#endif

int File_Export_GetRecordingStats(int *seconds, int *size, char **media_type);
#endif /* MULTIMEDIA */

#if !defined(BASIC) && !defined(CURSES_BASIC)
int File_Export_ImageTypeSupported(const char *id);
int File_Export_SaveScreen(const char *filename, UBYTE *ptr1, UBYTE *ptr2);
#endif

#endif /* FILE_EXPORT_H_ */

